// The Real3D's pictures on the GPU. See r3d_gpu.h; the shaders are src/gpu/r3d.hlsl.
//
// A picture: the texture RAM's changes uploaded (rows of the 2048 x 2048 16-bit texels, into a storage buffer the
// shaders read as words), the polygons' vertices and each mesh's and viewport's state uploaded, then for each
// viewport priority (and its high priority polygons again): the opaque layer into base (the translucent layers
// cleared where it draws, its depth kept as a colour too), the first translucent layer into t1 and the second into
// t2 (each with its own depth: only the nearest translucent polygon of a layer is kept, and only in front of the
// opaque layer), and last the three put together into the output, premultiplied, as the CPU does.
#include "r3d_gpu.h"
#include "r3d_shaders.h"
#include "../machine/model3.h"
#include "../video/tex3d_hd.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace {

struct GDraw
{
	float colour[4];
	uint32_t flags;
	int32_t format, tx, ty;
	int32_t tw, th, page, wrap;
	float levels, shininess, spec_value, fog_intensity;
	uint32_t vp;
	float flip;               // a flipbook's step before mixed in (0: none), its picture at flip_x, flip_y
	int32_t flip_x, flip_y;
};
static_assert(sizeof(GDraw) == 80, "as the shader's Draw");

struct GVp
{
	float sun[4], light[4], spot_ellipse[4], spot_range[4], spot_colour[4], spot_fog_colour[4], fog[4], fog2[4];
};

enum : uint32_t { F_TEXTURED = 1, F_ALPHA_TEST = 2, F_TEX_ALPHA = 4, F_LIGHTING = 8, F_FIXED = 16, F_SMOOTH = 32, F_SPECULAR = 64, F_INVERTED = 128 };

const float NEAR = 1e-3f;

} // namespace

R3dGpu::~R3dGpu() { shutdown(); }

void R3dGpu::shutdown()
{
	if (!dev) return;
	SDL_WaitForGPUIdle(dev);
	free_targets();
	for (SDL_GPUGraphicsPipeline *p : { p_opq, p_opq_l, p_tr, p_tr_l, p_fog, p_out, p_shadow })
		if (p) SDL_ReleaseGPUGraphicsPipeline(dev, p);
	for (SDL_GPUBuffer *b : { vbuf, dbuf, pbuf, texram })
		if (b) SDL_ReleaseGPUBuffer(dev, b);
	if (tb) SDL_ReleaseGPUTransferBuffer(dev, tb);
	// the shadow maps outlive a change of resolution, so free_targets does not hold them
	for (SDL_GPUTexture *tx : { smap[0], smap[1], smap_ds })
		if (tx) SDL_ReleaseGPUTexture(dev, tx);
	smap[0] = smap[1] = smap_ds = nullptr;
	if (sampler) SDL_ReleaseGPUSampler(dev, sampler);
	p_opq = p_opq_l = p_tr = p_tr_l = p_fog = p_out = p_shadow = nullptr;
	vbuf = dbuf = pbuf = texram = nullptr;
	tb = nullptr;
	sampler = nullptr;
	dev = nullptr;
}

const char *R3dGpu::driver() const { return dev ? SDL_GetGPUDeviceDriver(dev) : "none"; }

bool R3dGpu::init(SDL_GPUDevice *device)
{
	dev = device;
	const SDL_GPUShaderFormat have = SDL_GetGPUShaderFormats(dev);
	SDL_GPUShaderFormat fmt;
	if (have & SDL_GPU_SHADERFORMAT_SPIRV) fmt = SDL_GPU_SHADERFORMAT_SPIRV;
	else if (have & SDL_GPU_SHADERFORMAT_DXIL) fmt = SDL_GPU_SHADERFORMAT_DXIL;
	else if (have & SDL_GPU_SHADERFORMAT_MSL) fmt = SDL_GPU_SHADERFORMAT_MSL;
	else { err = "no shader format this build has"; return false; }
	auto shader = [&](const uint8_t *spv, size_t nspv, const uint8_t *dxil, size_t ndxil, const uint8_t *msl, size_t nmsl,
					  SDL_GPUShaderStage stage, int samplers, int storage, int uniforms) -> SDL_GPUShader * {
		SDL_GPUShaderCreateInfo ci = {};
		ci.format = fmt;
		if (fmt == SDL_GPU_SHADERFORMAT_SPIRV) { ci.code = spv; ci.code_size = nspv; }
		else if (fmt == SDL_GPU_SHADERFORMAT_DXIL) { ci.code = dxil; ci.code_size = ndxil; }
		else { ci.code = msl; ci.code_size = nmsl - 1; }
		ci.entrypoint = fmt == SDL_GPU_SHADERFORMAT_MSL ? "main0" : "main";
		ci.stage = stage;
		ci.num_samplers = uint32_t(samplers);
		ci.num_storage_buffers = uint32_t(storage);
		ci.num_uniform_buffers = uint32_t(uniforms);
		SDL_GPUShader *s = SDL_CreateGPUShader(dev, &ci);
		if (!s) err = std::string("a shader: ") + SDL_GetError();
		return s;
	};
#define SH(n) r3d_##n##_spv, sizeof(r3d_##n##_spv), r3d_##n##_dxil, sizeof(r3d_##n##_dxil), r3d_##n##_msl, sizeof(r3d_##n##_msl)
	SDL_GPUShader *vs_poly = shader(SH(vs_poly), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0);
	// three samplers: the opaque layer's depth, and Fighting Vipers 2's two shadow maps (r3d.hlsl)
	SDL_GPUShader *fs_opq = shader(SH(fs_opq), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 3, 1);
	SDL_GPUShader *fs_tr = shader(SH(fs_tr), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 3, 1);
	SDL_GPUShader *vs_quad = shader(SH(vs_quad), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1);
	SDL_GPUShader *fs_fog = shader(SH(fs_fog), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 1);
	SDL_GPUShader *fs_out = shader(SH(fs_out), SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 0, 1);
	SDL_GPUShader *vs_shadow = shader(SH(vs_shadow), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1);
	SDL_GPUShader *fs_shadow = shader(SH(fs_shadow), SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0);
#undef SH
	auto release = [&] {
		for (SDL_GPUShader *s : { vs_poly, fs_opq, fs_tr, vs_quad, fs_fog, fs_out, vs_shadow, fs_shadow })
			if (s) SDL_ReleaseGPUShader(dev, s);
	};
	if (!vs_poly || !fs_opq || !fs_tr || !vs_quad || !fs_fog || !fs_out || !vs_shadow || !fs_shadow) { release(); return false; }

	// the depth and stencil format: 32-bit float depth if it's here, else 24-bit
	for (SDL_GPUTextureFormat f : { SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT })
		if (SDL_GPUTextureSupportsFormat(dev, f, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) { ds_format = f; break; }
	if (ds_format == SDL_GPU_TEXTUREFORMAT_INVALID) { release(); err = "no depth and stencil format"; return false; }

	SDL_GPUVertexBufferDescription vb = { 0, sizeof(GV), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
	SDL_GPUVertexAttribute va[6] = {
		{ 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(GV, clip) },
		{ 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(GV, view) },
		{ 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(GV, normal) },
		{ 3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(GV, misc) },
		{ 4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, offsetof(GV, lodbase) },
		{ 5, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT, offsetof(GV, draw) },
	};
	const SDL_GPUTextureFormat F16 = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, F32 = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
	auto pipeline = [&](SDL_GPUShader *vs, SDL_GPUShader *fs, std::initializer_list<SDL_GPUTextureFormat> targets, bool poly,
						bool depth, SDL_GPUCompareOp cmp, bool layered, bool strip) -> SDL_GPUGraphicsPipeline * {
		SDL_GPUGraphicsPipelineCreateInfo ci = {};
		ci.vertex_shader = vs;
		ci.fragment_shader = fs;
		if (poly) {
			ci.vertex_input_state.vertex_buffer_descriptions = &vb;
			ci.vertex_input_state.num_vertex_buffers = 1;
			ci.vertex_input_state.vertex_attributes = va;
			ci.vertex_input_state.num_vertex_attributes = 6;
		}
		ci.primitive_type = strip ? SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
		ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		ci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
		ci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
		ci.rasterizer_state.enable_depth_clip = true;   // (no depth clamping: not every GPU has it)
		ci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
		if (depth) {
			ci.depth_stencil_state.enable_depth_test = true;
			ci.depth_stencil_state.enable_depth_write = true;
			ci.depth_stencil_state.compare_op = cmp;
			if (layered) {
				// the layered polygons: only where none has drawn yet (the stencil's low 7 bits 0), counting each
				SDL_GPUStencilOpState s = { SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP,
					SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP, SDL_GPU_COMPAREOP_EQUAL };
				ci.depth_stencil_state.front_stencil_state = s;
				ci.depth_stencil_state.back_stencil_state = s;
				ci.depth_stencil_state.compare_mask = 0x7F;
				ci.depth_stencil_state.write_mask = 0x7F;
				ci.depth_stencil_state.enable_stencil_test = true;
			}
			ci.target_info.depth_stencil_format = ds_format;
			ci.target_info.has_depth_stencil_target = true;
		}
		std::vector<SDL_GPUColorTargetDescription> cts;
		for (SDL_GPUTextureFormat f : targets) {
			SDL_GPUColorTargetDescription d = {};
			d.format = f;
			cts.push_back(d);
		}
		ci.target_info.color_target_descriptions = cts.data();
		ci.target_info.num_color_targets = uint32_t(cts.size());
		SDL_GPUGraphicsPipeline *p = SDL_CreateGPUGraphicsPipeline(dev, &ci);
		if (!p) err = std::string("a pipeline: ") + SDL_GetError();
		return p;
	};
	p_opq = pipeline(vs_poly, fs_opq, { F16, F16, F16, F32 }, true, true, SDL_GPU_COMPAREOP_GREATER_OR_EQUAL, false, false);
	p_opq_l = pipeline(vs_poly, fs_opq, { F16, F16, F16, F32 }, true, true, SDL_GPU_COMPAREOP_GREATER_OR_EQUAL, true, false);
	p_tr = pipeline(vs_poly, fs_tr, { F16 }, true, true, SDL_GPU_COMPAREOP_GREATER, false, false);
	p_tr_l = pipeline(vs_poly, fs_tr, { F16 }, true, true, SDL_GPU_COMPAREOP_GREATER, true, false);
	p_fog = pipeline(vs_quad, fs_fog, { F16, F16, F16 }, false, false, SDL_GPU_COMPAREOP_ALWAYS, false, true);
	p_out = pipeline(vs_quad, fs_out, { SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM }, false, false, SDL_GPU_COMPAREOP_ALWAYS, false, true);
	// the shadow map: the same vertex buffer, the nearest to the light kept (its depth runs 0 at the light to 1)
	p_shadow = pipeline(vs_shadow, fs_shadow, { F32 }, true, true, SDL_GPU_COMPAREOP_LESS, false, false);
	release();
	if (!p_opq || !p_opq_l || !p_tr || !p_tr_l || !p_fog || !p_out || !p_shadow) return false;
	SDL_GPUSamplerCreateInfo si = {};
	si.min_filter = si.mag_filter = SDL_GPU_FILTER_NEAREST;
	si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
	si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
	sampler = SDL_CreateGPUSampler(dev, &si);
	SDL_GPUBufferCreateInfo bi = {};
	bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
	// the board's two pages of 2048 x 1024 words, and the texture pack's pages after them (tex3d_hd.h): the
	// shader reaches a page by its number alone, so a replacement is read by the code that reads a native texture
	hd_pages = video::hd3d().page_count();
	bi.size = uint32_t(2048 * 2048 * 2) + uint32_t(hd_pages) * 1024 * 2048 * 2;
	texram = SDL_CreateGPUBuffer(dev, &bi);
	if (!sampler || !texram) { err = std::string("its buffers: ") + SDL_GetError(); return false; }
	texram_full = true;
	return true;
}

void R3dGpu::free_targets()
{
	for (SDL_GPUTexture **t : { &base, &t1, &t2, &odepth, &ds, &out })
		if (*t) { SDL_ReleaseGPUTexture(dev, *t); *t = nullptr; }
	w = h = 0;
}

bool R3dGpu::make_targets(int nw, int nh, int nss)
{
	if (nw == w && nh == h && nss == ss && out) return true;
	SDL_WaitForGPUIdle(dev);
	free_targets();
	auto tex = [&](SDL_GPUTextureFormat f, SDL_GPUTextureUsageFlags usage, int k = 1) {
		SDL_GPUTextureCreateInfo ci = {};
		ci.type = SDL_GPU_TEXTURETYPE_2D;
		ci.format = f;
		ci.usage = usage;
		ci.width = uint32_t(nw / k);
		ci.height = uint32_t(nh / k);
		ci.layer_count_or_depth = 1;
		ci.num_levels = 1;
		ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
		return SDL_CreateGPUTexture(dev, &ci);
	};
	const auto CT = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	base = tex(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, CT);
	t1 = tex(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, CT);
	t2 = tex(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, CT);
	odepth = tex(SDL_GPU_TEXTUREFORMAT_R32_FLOAT, CT);
	ds = tex(ds_format, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
	out = tex(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, CT, nss);
	if (!base || !t1 || !t2 || !odepth || !ds || !out) {
		err = std::string("its targets: ") + SDL_GetError();
		free_targets();
		return false;
	}
	w = nw;
	h = nh;
	ss = nss;
	return true;
}

SDL_GPUTexture *R3dGpu::output(int nw, int nh, int nss)
{
	return make_targets(nw, nh, std::max(1, nss)) ? out : nullptr;
}

bool R3dGpu::ensure_buffer(SDL_GPUBuffer *&b, uint32_t &size, uint32_t need, SDL_GPUBufferUsageFlags usage)
{
	if (b && size >= need) return true;
	if (b) SDL_ReleaseGPUBuffer(dev, b);
	uint32_t n = std::max<uint32_t>(need + need / 2, 65536);
	SDL_GPUBufferCreateInfo bi = {};
	bi.usage = usage;
	bi.size = n;
	b = SDL_CreateGPUBuffer(dev, &bi);
	size = b ? n : 0;
	if (!b) err = std::string("a buffer: ") + SDL_GetError();
	return b != nullptr;
}

bool R3dGpu::render(const video::Scene &scene, const video::Raster &prep, m3::Model3 &board)
{
	return render(scene, prep, board.tex_ram, board.tex_uploads);
}

bool R3dGpu::render(const video::Scene &scene, const video::Raster &prep, const std::vector<uint16_t> &tex_ram, std::vector<m3::Model3::TexUpload> &tex_uploads)
{
	const auto t0 = std::chrono::steady_clock::now();
	if (!make_targets(prep.width(), prep.height(), ss)) return false;
	using video::Raster;
	// ---- the vertices, meshes' and viewports' state ----
	std::vector<GDraw> gd(prep.draws.size());
	std::vector<GVp> gv(std::max<size_t>(1, scene.viewports.size()));
	// the light and the units each layer is drawn in, before the viewports' state, which carries them
	light_ok = false;
	if (shadows) {
		fit_light(scene, prep);
		// VF3_SHADOW_FAIL=<n>: give up on the light every nth picture on purpose. The net below is meant for
		// a picture that fails, and once the two causes that made them fail were fixed none did any more --
		// so this is how it is still exercised.
		if (const char *fl = getenv("VF3_SHADOW_FAIL"); fl && light_ok) {
			static unsigned tick = 0;
			const int every = std::max(2, atoi(fl));
			if (++tick % unsigned(every) == 0) light_ok = false;
		}
		if (light_ok) {
			memcpy(kept.light_m, light_m, sizeof(light_m));
			memcpy(kept.floor_p, floor_p, sizeof(floor_p));
			memcpy(kept.vp_scale, vp_scale, sizeof(vp_scale));
			memcpy(kept.caster_vp, caster_vp, sizeof(caster_vp));
			memcpy(kept.vp_group, vp_group, sizeof(vp_group));
			kept.nmaps = nmaps;
			kept_ok = true;
		} else if (!light_refused && shadow_tris > 0 && kept_ok) {
			// the scene still carries the game's shadows, so leaving them out is still right; the light from
			// the picture before does for this one
			memcpy(light_m, kept.light_m, sizeof(light_m));
			memcpy(floor_p, kept.floor_p, sizeof(floor_p));
			memcpy(vp_scale, kept.vp_scale, sizeof(vp_scale));
			memcpy(caster_vp, kept.caster_vp, sizeof(caster_vp));
			memcpy(vp_group, kept.vp_group, sizeof(vp_group));
			nmaps = kept.nmaps;
			light_ok = true;
			why[6 + 1]++;   // counted apart: a picture kept, not a picture lost
		} else if (light_refused) {
			kept_ok = false;
		} else {
			// No shadows in the scene at all. That is a menu or a cut as often as it is a stage with its
			// shadow switch off, so the kept light is only let go after a while -- long enough to outlast a
			// picture or two, short enough that the next stage never gets the last one's light.
			static int empty = 0;
			if (shadow_tris > 0) empty = 0;
			else if (++empty > 30) kept_ok = false;
		}
	}
	// VF3_SHADOW_WHY: how often the light could not be worked out, and why. Every time it cannot, the
	// game's own shadows come back for that one picture -- which is what a player sees flicker.
	if (shadows && getenv("VF3_SHADOW_WHY")) {
		static unsigned said[8] = {}, frames = 0;
		frames++;
		bool moved = false;
		for (int k = 0; k < 8; k++) if (why[k] != said[k]) moved = true;
		if (moved && (frames % 60) == 0) {
			static const char *name[8] = { "pas d ombres dans la scene", "pas de direction de sol",
				"pas de faces de combattant", "aucune couche au niveau des pieds", "direction du sol perdue",
				"aucun groupe ou repere degenere", "trop peu de sommets", "images rattrapees" };
			fprintf(stderr, "lumiere non trouvee, sur %u images :", frames);
			for (int k = 0; k < 8; k++) if (why[k]) fprintf(stderr, " [%s] %u", name[k], why[k]);
			fprintf(stderr, "\n");
			for (int k = 0; k < 8; k++) said[k] = why[k];
		}
	}
	for (size_t i = 0; i < scene.viewports.size(); i++) {
		const video::Viewport &v = scene.viewports[i];
		GVp &o = gv[i];
		o = {};
		o.sun[0] = v.sun[0]; o.sun[1] = v.sun[1]; o.sun[2] = v.sun[2]; o.sun[3] = v.sun_intensity;
		o.light[0] = v.ambient; o.light[1] = 1.0f;   // (the sun clamped: Step 1.0's)
		// whether the light that reaches a surface stops at full: Step 1.0 only, as the CPU rasterizer has it
		o.light[2] = v.intensity_clamp ? 1.0f : 0.0f;
		// How many of this layer's units make one of the casters' (measure_scales); 0 means the shadow leaves
		// that layer alone.
		//
		// Only the layers drawn in the casters' own units take a shadow. A layer drawn in others is matched to
		// them by its floor, and on the dam stage that is simply wrong: its widest flat surface is the water
		// under the walkway, seen THROUGH the grating, and matching it to the walkway drags it up to the
		// walkway's height -- the shadow then paints the gaps between the gratings' bars, which carry no
		// matter at all. Measured at a fight frame: of the ground the shadow touched, the near layer's bars
		// (the floor) and the far layer's openings each took about half. Nothing in the scene says how far
		// below the walkway that water is, so the ratio cannot be had from the floor, and a ratio that cannot
		// be measured is not used. VF3_SHADOW_FAR=1 applies it anyway, which is how the two can be compared.
		static const bool far_on = getenv("VF3_SHADOW_FAR") != nullptr;
		const bool own_units = i < 64 && vp_scale[i] == 1.0f;
		o.light[3] = (shadows && light_ok && i < 64 && (own_units || far_on)) ? vp_scale[i] : 0.0f;
		memcpy(o.spot_ellipse, v.spot_ellipse, sizeof(o.spot_ellipse));
		o.spot_range[0] = v.spot_range[0]; o.spot_range[1] = v.spot_range[1];
		// Which shadow map this viewport may be judged by: its own, when it carries a fighter, and both
		// when it carries none. A fighter lit by one light has no business being darkened by a map drawn
		// along the other's.
		//
		// Measured, and it changes nothing at a fight frame of stage 3: a fighter's pixels that fall inside
		// the other map's box read an empty texel there, since that map holds only the other fighter. It is
		// kept because it costs a branch and it is right the day the two boxes hold each other's fighter --
		// a throw, a clinch. What makes stage 3's fighters darker than the dam's (51.2 %% and 57.9 %% of
		// their pixels in shadow against 31.8 %% and 39.3 %%) is not this: it is the light, 48.5 degrees off
		// the floor's own direction there against 32 at the dam, so more of a body turns away from it. -1: both.
		o.spot_range[2] = (shadows && light_ok && i < 64 && caster_vp[i]) ? float(vp_group[i]) : -1.0f;
		memcpy(o.spot_colour, v.spot_colour, 12);
		memcpy(o.spot_fog_colour, v.spot_fog_colour, 12);
		memcpy(o.fog, v.fog_colour, 12);
		o.fog[3] = v.fog_density;
		o.fog2[0] = v.fog_start; o.fog2[1] = v.fog_attenuation; o.fog2[2] = v.fog_ambient;
	}
	for (size_t i = 0; i < prep.draws.size(); i++) {
		const Raster::Draw &d = prep.draws[i];
		const video::Mesh &m = *d.mesh;
		GDraw &o = gd[i];
		o = {};
		const video::Vertex &pv = m.verts[0];
		float k = m.translator_map ? 16.0f : 1.0f;
		o.colour[0] = pv.colour[0] / 255.0f * k; o.colour[1] = pv.colour[1] / 255.0f * k; o.colour[2] = pv.colour[2] / 255.0f * k;
		o.colour[3] = pv.colour[3] / 255.0f * d.model->alpha;
		o.flags = (m.textured ? F_TEXTURED : 0) | (m.alpha_test ? F_ALPHA_TEST : 0) | (m.texture_alpha ? F_TEX_ALPHA : 0) |
			(m.lighting ? F_LIGHTING : 0) | (m.fixed_shading ? F_FIXED : 0) | (m.smooth_shading ? F_SMOOTH : 0) |
			(m.specular ? F_SPECULAR : 0) | (m.inverted ? F_INVERTED : 0);
		// the nodes' texture offset moves where on the sheet the mesh reads from, as the CPU rasterizer has it
		// (Step 1.5 and later; 0 on Step 1.0, which has no such word, so these are then m.x and m.y as before)
		const int tox = d.model->tex_x, toy = d.model->tex_y;
		o.format = m.format; o.tx = (m.x + tox) & 2047; o.ty = (m.y + toy) & 2047; o.tw = m.width; o.th = m.height;
		o.page = m.page ^ d.model->page;
		o.wrap = m.wrap_u | (m.wrap_v << 2);
		o.levels = m.textured ? std::floor(std::log2(float(std::min(m.width, m.height)))) - 1.0f : 0.0f;
		o.shininess = m.shininess; o.spec_value = m.specular_value; o.fog_intensity = m.fog_intensity;
		o.vp = uint32_t(d.vp - scene.viewports.data());
		o.flip = m.flip; o.flip_x = (m.flip_x + tox) & 2047; o.flip_y = (m.flip_y + toy) & 2047;
		// The texture pack's replacement for this mesh's texture, where there is one (Raster::resolve_hd, which
		// the CPU rasteriser reads the same way): the pack's pages come after the board's two in this buffer, so
		// only these six numbers change. Level 0 only, a pack's images having no mipmaps, and no flipbook
		// cross-fade, a mesh's two pictures sharing one page and one size here.
		if (!prep.hd.empty() && prep.hd[i].hd) {
			const video::TexPlace &p = prep.hd[i];
			o.page = 2 + p.page;
			o.tx = p.x; o.ty = p.y; o.tw = p.w; o.th = p.h;
			o.format = p.format;
			o.levels = 0;
			o.flip = 0;
		}
	}
	// each polygon (a triangle, or four sides after the near plane) as triangles; their raw attributes (the
	// prepared ones are over w) and clip positions in their viewport's projection
	verts.clear();
	struct Range { size_t first, count; int draw; };
	std::vector<Range> ranges;
	size_t batch_first[24], batch_end[24];
	for (int b = 0; b < 24; b++) {
		batch_first[b] = ranges.size();
		for (size_t ti = prep.batches[b].begin; ti < prep.batches[b].end; ti++) {
			const Raster::Tri &t = prep.tris[ti];
			const Raster::Draw &d = prep.draws[t.draw];
			GV g[8];
			for (int k = 0; k < t.n; k++) {
				const float wv = 1.0f / t.iw[k];
				const float nx = (t.sx[k] - d.vx) / d.vw * 2.0f - 1.0f, ny = 1.0f - (t.sy[k] - d.vy) / d.vh * 2.0f;
				GV &o = g[k];
				o.clip[0] = nx * wv; o.clip[1] = ny * wv; o.clip[2] = NEAR; o.clip[3] = wv;
				for (int q = 0; q < 3; q++) { o.view[q] = t.a[k][q] * wv; o.normal[q] = t.a[k][3 + q] * wv; }
				o.misc[0] = t.a[k][6] * wv; o.misc[1] = t.a[k][7] * wv; o.misc[2] = t.a[k][8] * wv; o.misc[3] = t.a[k][9] * wv;
				o.lodbase = t.a[k][10] * wv;
				o.draw = uint32_t(t.draw);
			}
			if (ranges.size() == batch_first[b] || ranges.back().draw != t.draw)
				ranges.push_back({ verts.size(), 0, t.draw });
			for (int f = 1; f + 1 < t.n; f++) {
				verts.push_back(g[0]); verts.push_back(g[f]); verts.push_back(g[f + 1]);
				ranges.back().count += 3;
			}
		}
		batch_end[b] = ranges.size();
	}
	// ---- the uploads ----
	std::vector<video::Raster::Draw> dummy;
	(void)dummy;
	const uint32_t vbytes = uint32_t(std::max<size_t>(1, verts.size()) * sizeof(GV));
	const uint32_t dbytes = uint32_t(std::max<size_t>(1, gd.size()) * sizeof(GDraw));
	const uint32_t pbytes = uint32_t(gv.size() * sizeof(GVp));
	// texture RAM: whole rows of the areas written (all of it the first time, or after many changes)
	std::vector<std::pair<uint32_t, uint32_t>> rows;   // first row, count
	if (texram_full || tex_uploads.size() > 256) {
		rows.push_back({ 0, 2048 });
	} else {
		for (auto &u : tex_uploads) rows.push_back({ u.y & 2047, std::min<uint32_t>(u.h, 2048 - (u.y & 2047)) });
		std::sort(rows.begin(), rows.end());
		std::vector<std::pair<uint32_t, uint32_t>> merged;
		for (auto &r : rows) {
			if (!merged.empty() && r.first <= merged.back().first + merged.back().second)
				merged.back().second = std::max(merged.back().second, r.first + r.second - merged.back().first);
			else merged.push_back(r);
		}
		rows = merged;
	}
	// (the rows asked for are taken now; if a step below fails, the whole of texture memory goes up next time)
	tex_uploads.clear();
	texram_full = true;
	if (tex_ram.size() < size_t(2048) * 2048) { err = "no texture memory yet"; return false; }
	uint32_t tbytes = 0;
	for (auto &r : rows) tbytes += r.second * 4096;
	// The texture pack's pages: made bigger than the board's own buffer at start-up and uploaded once, since
	// nothing writes to them while the game runs. A pack loaded after this renderer was made (not something the
	// options allow today) would need the buffer again, so the size is checked rather than assumed.
	const int want_hd = video::hd3d().page_count();
	if (want_hd != hd_pages) {
		SDL_WaitForGPUIdle(dev);
		if (texram) SDL_ReleaseGPUBuffer(dev, texram);
		SDL_GPUBufferCreateInfo bi = {};
		bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
		bi.size = uint32_t(2048 * 2048 * 2) + uint32_t(want_hd) * 1024 * 2048 * 2;
		texram = SDL_CreateGPUBuffer(dev, &bi);
		hd_pages = want_hd;
		hd_up = false;
		rows.assign(1, { 0, 2048 });
		tbytes = 2048 * 4096;
		if (!texram) { err = std::string("the texture buffer: ") + SDL_GetError(); return false; }
	}
	const uint32_t hdbytes = hd_up ? 0 : uint32_t(hd_pages) * 1024 * 4096;
	if (!ensure_buffer(vbuf, vbuf_size, vbytes, SDL_GPU_BUFFERUSAGE_VERTEX) ||
		!ensure_buffer(dbuf, dbuf_size, dbytes, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ) ||
		!ensure_buffer(pbuf, pbuf_size, pbytes, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ))
		return false;
	const uint32_t total = vbytes + dbytes + pbytes + tbytes + hdbytes;
	if (!tb || tb_size < total) {
		if (tb) SDL_ReleaseGPUTransferBuffer(dev, tb);
		SDL_GPUTransferBufferCreateInfo ti = {};
		ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		ti.size = std::max<uint32_t>(total + total / 2, 1 << 20);
		tb = SDL_CreateGPUTransferBuffer(dev, &ti);
		tb_size = tb ? ti.size : 0;
		if (!tb) { err = std::string("a transfer buffer: ") + SDL_GetError(); return false; }
	}
	uint8_t *map = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(dev, tb, true));
	if (!map) { err = std::string("mapping: ") + SDL_GetError(); return false; }
	if (!verts.empty()) memcpy(map, verts.data(), verts.size() * sizeof(GV));
	if (!gd.empty()) memcpy(map + vbytes, gd.data(), gd.size() * sizeof(GDraw));
	memcpy(map + vbytes + dbytes, gv.data(), pbytes);
	{
		uint32_t at = vbytes + dbytes + pbytes;
		for (auto &r : rows) {
			memcpy(map + at, tex_ram.data() + size_t(r.first) * 2048, size_t(r.second) * 4096);
			at += r.second * 4096;
		}
		if (hdbytes) memcpy(map + at, video::hd3d().pages(), hdbytes);
	}
	SDL_UnmapGPUTransferBuffer(dev, tb);
	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
	if (!cmd) { err = std::string("a command buffer: ") + SDL_GetError(); return false; }
	{
		SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
		auto up = [&](uint32_t from, SDL_GPUBuffer *b, uint32_t to, uint32_t n) {
			if (!n) return;
			SDL_GPUTransferBufferLocation src = { tb, from };
			SDL_GPUBufferRegion dst = { b, to, n };
			SDL_UploadToGPUBuffer(cp, &src, &dst, false);
		};
		up(0, vbuf, 0, vbytes);
		up(vbytes, dbuf, 0, dbytes);
		up(vbytes + dbytes, pbuf, 0, pbytes);
		uint32_t at = vbytes + dbytes + pbytes;
		for (auto &r : rows) { up(at, texram, r.first * 4096, r.second * 4096); at += r.second * 4096; }
		if (hdbytes) { up(at, texram, 2048 * 4096, hdbytes); hd_up = true; }
		SDL_EndGPUCopyPass(cp);
	}
	// ---- the passes ----
	auto colour = [](SDL_GPUTexture *t, bool clear) {
		SDL_GPUColorTargetInfo c = {};
		c.texture = t;
		c.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
		c.store_op = SDL_GPU_STOREOP_STORE;
		c.clear_color = { 0, 0, 0, 0 };
		return c;
	};
	auto depth_target = [&] {
		SDL_GPUDepthStencilTargetInfo d = {};
		d.texture = ds;
		d.clear_depth = 0.0f;
		d.load_op = SDL_GPU_LOADOP_CLEAR;
		d.store_op = SDL_GPU_STOREOP_DONT_CARE;
		d.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
		d.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		d.clear_stencil = 0;
		return d;
	};
	// Fighting Vipers 2's shadow map, before anything is drawn: the fighters seen from the light. Its only
	// reader for now is read_shadow_map, which is how it is checked before anything leans on it.
	if (shadows && light_ok && make_shadow_map()) {
		for (int g = 0; g < nmaps; g++) {
			SDL_GPUColorTargetInfo sc = {};
			sc.texture = smap[g];
			sc.load_op = SDL_GPU_LOADOP_CLEAR;
			sc.store_op = SDL_GPU_STOREOP_STORE;
			sc.clear_color = { 1, 1, 1, 1 };
			SDL_GPUDepthStencilTargetInfo sd = {};
			sd.texture = smap_ds;
			sd.clear_depth = 1.0f;
			sd.load_op = SDL_GPU_LOADOP_CLEAR;
			sd.store_op = SDL_GPU_STOREOP_DONT_CARE;
			sd.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
			sd.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
			SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &sc, 1, &sd);
			SDL_GPUBufferBinding vb = { vbuf, 0 };
			SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);
			SDL_BindGPUGraphicsPipeline(rp, p_shadow);
			SDL_PushGPUVertexUniformData(cmd, 0, light_m[g], sizeof(light_m[g]));
			const SDL_GPUViewport gv = { 0, 0, float(SMAP), float(SMAP), 0, 1 };
			SDL_SetGPUViewport(rp, &gv);
			const SDL_Rect rc = { 0, 0, SMAP, SMAP };
			SDL_SetGPUScissor(rp, &rc);
			int cast = 0;
			for (const Range &rg : ranges) {
				const Raster::Draw &dr = prep.draws[rg.draw];
				const size_t iv = size_t(dr.vp - scene.viewports.data());
				// VF3_SHADOW_ALL: every viewport into the map, to see which ones share the casters' space
				static const bool all = getenv("VF3_SHADOW_ALL") != nullptr;
				if (iv >= 64 || (!all && vp_group[iv] != g)) continue;
				if (dr.model->flattened) continue;
				if (!dr.mesh->in_layer(0, dr.model->alpha)) continue;
				SDL_DrawGPUPrimitives(rp, uint32_t(rg.count), 1, uint32_t(rg.first), 0);
				cast++;
			}
			if (debug_now) fprintf(stderr, "shadow pass %d: %d draws of %zu\n", g, cast, ranges.size());
			SDL_EndGPURenderPass(rp);
		}
	}
	// the picture's own numbers, then whether this port's shadows are on and the matrix that reads their map
	float frame_u[44] = { float(prep.game_x()), prep.game_xscale(), prep.game_yscale(),
	                      (shadows && light_ok && smap[0]) ? (getenv("VF3_SHADOW_DEBUG") ? 1.0f + float(atoi(getenv("VF3_SHADOW_DEBUG"))) : 1.0f) : 0.0f };
	memcpy(frame_u + 4, light_m, sizeof(light_m));
	// the second map is only read when there is one; with one light, both slots hold the same matrix
	if (nmaps < 2) memcpy(frame_u + 20, light_m[0], sizeof(light_m[0]));
	// VF3_SHADOW_DARK: how much of the ambient the shadow takes as well, on top of the sun it already
	// takes whole. 0 (the default) is the game's own light left alone.
	frame_u[36] = getenv("VF3_SHADOW_DARK") ? float(atof(getenv("VF3_SHADOW_DARK"))) : 0.0f;
	// VF3_SHADOW_NONE: the game's own shadows left out and ours not applied either -- the lit ground, to
	// measure what a shadow actually takes away
	if (getenv("VF3_SHADOW_NONE")) frame_u[3] = 0;
	// the floor measure_scales found, which is also where the shadow stops: nothing under it takes one
	memcpy(frame_u + 40, floor_p, sizeof(floor_p));
	// the fog layers first, into the cleared layers
	{
		SDL_GPUColorTargetInfo c[3] = { colour(base, true), colour(t1, true), colour(t2, true) };
		SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, c, 3, nullptr);
		if (scene.blank) {
			// block culling: the 3D white
			SDL_BindGPUGraphicsPipeline(rp, p_fog);
			const float rect[4] = { -1, -1, 1, 1 };
			const float fog[16] = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, frame_u[0], frame_u[1], frame_u[2], 0 };
			SDL_PushGPUVertexUniformData(cmd, 0, rect, sizeof(rect));
			SDL_PushGPUFragmentUniformData(cmd, 0, fog, sizeof(fog));
			SDL_DrawGPUPrimitives(rp, 4, 1, 0, 0);
		} else {
			auto vp_px = [&](const video::Viewport &v, int &x0, int &y0, int &x1, int &y1) {
				const float sc = prep.game_yscale(), xs = prep.game_xscale();
				if (v.full_screen) { x0 = 0; x1 = prep.width(); }
				else { x0 = prep.game_x() + int(v.vp_x * xs); x1 = x0 + int(v.vp_w * xs); }
				y0 = int(v.vp_y * sc); y1 = y0 + int(v.vp_h * sc);
				x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(prep.width(), x1); y1 = std::min(prep.height(), y1);
			};
			auto fill = [&](const video::Viewport &v, const float *fog) {
				int x0, y0, x1, y1;
				vp_px(v, x0, y0, x1, y1);
				if (x0 >= x1 || y0 >= y1) return;
				SDL_GPUViewport gvp = { float(x0), float(y0), float(x1 - x0), float(y1 - y0), 0, 1 };
				SDL_SetGPUViewport(rp, &gvp);
				SDL_Rect sc = { x0, y0, x1 - x0, y1 - y0 };
				SDL_SetGPUScissor(rp, &sc);
				SDL_BindGPUGraphicsPipeline(rp, p_fog);
				const float rect[4] = { -1, -1, 1, 1 };
				SDL_PushGPUVertexUniformData(cmd, 0, rect, sizeof(rect));
				SDL_PushGPUFragmentUniformData(cmd, 0, fog, 16 * sizeof(float));
				SDL_DrawGPUPrimitives(rp, 4, 1, 0, 0);
			};
			// the ambient fog (as the CPU's fog_layers)
			{
				float amb = 1.0f;
				const video::Viewport *pick = nullptr;
				for (int i = 0; i < 4; i++) {
					bool has = false;
					for (auto &v : scene.viewports) {
						if (v.priority != i) continue;
						has = true;
						if (v.scroll_fog > 0) continue;
						if (v.fog_density <= 0 && v.fog_start <= 0) continue;
						if (v.fog_ambient < amb) { pick = &v; amb = v.fog_ambient; }
					}
					if (pick || has) break;
				}
				if (pick) {
					const float fog[16] = { 0, 0, 0, 1.0f - amb, 0, 0, 0, 0, 0, 0, 1, 1, frame_u[0], frame_u[1], frame_u[2], 0 };
					fill(*pick, fog);
				}
			}
			const video::Viewport *pick = nullptr;
			for (int i = 0; i < 4 && !pick; i++)
				for (auto &v : scene.viewports)
					if (v.priority == i && v.scroll_fog > 0 && (!pick || pick->scroll_fog < v.scroll_fog)) pick = &v;
			if (pick)
				for (auto &n : scene.viewports) {
					if (memcmp(n.fog_colour, pick->fog_colour, sizeof(n.fog_colour))) continue;
					bool spot_on_fog = n.spot_fog_colour[0] > 0 || n.spot_fog_colour[1] > 0 || n.spot_fog_colour[2] > 0;
					bool fogon = n.fog_density > 0 || n.fog_start > 0 || n.scroll_att > 0;
					bool spot_fog = n.fog_attenuation > 0 && spot_on_fog;
					if (!fogon && !spot_fog) continue;
					float att = (fogon && spot_fog) ? 0.0f : n.fog_attenuation;
					float fog[16] = { pick->fog_colour[0] * n.fog_ambient, pick->fog_colour[1] * n.fog_ambient, pick->fog_colour[2] * n.fog_ambient,
						pick->scroll_fog, att * n.spot_fog_colour[0] * pick->fog_colour[0], att * n.spot_fog_colour[1] * pick->fog_colour[1],
						att * n.spot_fog_colour[2] * pick->fog_colour[2], 1, n.spot_ellipse[0], n.spot_ellipse[1], n.spot_ellipse[2],
						n.spot_ellipse[3], frame_u[0], frame_u[1], frame_u[2], 0 };
					fill(*pick, fog);
					break;
				}
		}
		SDL_EndGPURenderPass(rp);
	}
	if (!scene.blank) {
		SDL_GPUBufferBinding vb = { vbuf, 0 };
		SDL_GPUBuffer *storage[3] = { texram, dbuf, pbuf };
		auto draw_batch = [&](SDL_GPURenderPass *rp, int b, SDL_GPUGraphicsPipeline *plain, SDL_GPUGraphicsPipeline *layered, SDL_GPUTexture *od) {
			SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);
			// the two shadow maps after it; with the shadows off they are the depth again, since a sampler
			// must always have a texture
			SDL_GPUTexture *m0 = (shadows && smap[0]) ? smap[0] : od;
			SDL_GPUTexture *m1 = (shadows && nmaps > 1 && smap[1]) ? smap[1] : m0;
			SDL_GPUTextureSamplerBinding sb[3] = { { od, sampler }, { m0, sampler }, { m1, sampler } };
			int last_vp = -1;
			SDL_GPUGraphicsPipeline *bound = nullptr;
			for (size_t r = batch_first[b]; r < batch_end[b]; r++) {
				const Range &rg = ranges[r];
				const Raster::Draw &d = prep.draws[rg.draw];
				// With this port's shadows on, the game's own are left out here rather than in the raster:
				// they still mark the floor for measure_scales, they are simply not drawn.
				if (shadows && light_ok && d.model->flattened) continue;
				SDL_GPUGraphicsPipeline *p = d.mesh->layered ? layered : plain;
				if (p != bound) {
					SDL_BindGPUGraphicsPipeline(rp, p);
					SDL_BindGPUFragmentSamplers(rp, 0, sb, 3);
					SDL_BindGPUFragmentStorageBuffers(rp, 0, storage, 3);
					SDL_PushGPUFragmentUniformData(cmd, 0, frame_u, sizeof(frame_u));
					SDL_SetGPUStencilReference(rp, 0);
					bound = p;
				}
				const int vi = int(d.vp - scene.viewports.data());
				if (vi != last_vp) {
					SDL_GPUViewport gvp = { d.vx, d.vy, d.vw, d.vh, 0, 1 };
					SDL_SetGPUViewport(rp, &gvp);
					SDL_Rect sc = { d.x0, d.y0, d.x1 - d.x0, d.y1 - d.y0 };
					SDL_SetGPUScissor(rp, &sc);
					last_vp = vi;
				}
				SDL_DrawGPUPrimitives(rp, uint32_t(rg.count), 1, uint32_t(rg.first), 0);
			}
		};
		for (int pri = 0; pri < 4; pri++) {
			if (!prep.prio[pri].any) continue;
			for (int pass = 0; pass < 2; pass++) {
				const int b = (pri * 2 + pass) * 3;
				{
					SDL_GPUColorTargetInfo c[4] = { colour(base, false), colour(t1, false), colour(t2, false), colour(odepth, true) };
					SDL_GPUDepthStencilTargetInfo dt = depth_target();
					SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, c, 4, &dt);
					// (the sampled texture must not be a target of this pass: the translucent layer's, unused here)
					draw_batch(rp, b, p_opq, p_opq_l, out);
					SDL_EndGPURenderPass(rp);
				}
				for (int layer = 1; layer <= 2; layer++) {
					if (prep.batches[b + layer].begin == prep.batches[b + layer].end) continue;
					SDL_GPUColorTargetInfo c = colour(layer == 1 ? t1 : t2, false);
					SDL_GPUDepthStencilTargetInfo dt = depth_target();
					SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &c, 1, &dt);
					draw_batch(rp, b + layer, p_tr, p_tr_l, odepth);
					SDL_EndGPURenderPass(rp);
				}
				if (!prep.prio[pri].has_overlay) break;
			}
		}
	}
	// the three layers put together
	{
		SDL_GPUColorTargetInfo c = colour(out, true);
		SDL_GPURenderPass *rp = SDL_BeginGPURenderPass(cmd, &c, 1, nullptr);
		SDL_BindGPUGraphicsPipeline(rp, p_out);
		SDL_GPUTextureSamplerBinding sb[3] = { { base, sampler }, { t1, sampler }, { t2, sampler } };
		SDL_BindGPUFragmentSamplers(rp, 0, sb, 3);
		SDL_GPUViewport gvp = { 0, 0, float(w / ss), float(h / ss), 0, 1 };
		SDL_SetGPUViewport(rp, &gvp);
		const float rect[4] = { -1, -1, 1, 1 };
		SDL_PushGPUVertexUniformData(cmd, 0, rect, sizeof(rect));
		const int32_t k[4] = { ss, 0, 0, 0 };
		SDL_PushGPUFragmentUniformData(cmd, 0, k, sizeof(k));
		SDL_DrawGPUPrimitives(rp, 4, 1, 0, 0);
		SDL_EndGPURenderPass(rp);
	}
	last_cpu_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	if (!SDL_SubmitGPUCommandBuffer(cmd)) { err = std::string("submitting: ") + SDL_GetError(); return false; }
	texram_full = false;
	return true;
}


bool R3dGpu::make_shadow_map()
{
	if (smap[0] && smap[MAPS - 1] && smap_ds) return true;
	SDL_GPUTextureCreateInfo ci = {};
	ci.type = SDL_GPU_TEXTURETYPE_2D;
	ci.width = ci.height = uint32_t(SMAP);
	ci.layer_count_or_depth = 1;
	ci.num_levels = 1;
	ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
	ci.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
	ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	for (SDL_GPUTexture *&tx : smap) if (!tx) tx = SDL_CreateGPUTexture(dev, &ci);
	ci.format = ds_format;
	ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	smap_ds = SDL_CreateGPUTexture(dev, &ci);
	if (!smap[0] || !smap[MAPS - 1] || !smap_ds) { err = std::string("the shadow map: ") + SDL_GetError(); return false; }
	return true;
}

// How many of each viewport's units make one of the casters'. Fighting Vipers 2 does not draw its stage in one
// space: r3d_raster.cpp clears the depth buffer once per priority, and the game uses that to draw the same
// stage in several layers, each in its own units. Measured on the dam stage: the fighters stand at z -6 while
// the layer that carries the dam and the grating is at z -129, twenty times bigger and twenty times farther,
// through the SAME frustum, the same cota and the same root matrix -- which is how the game's fog (density
// 0.0005) reaches the background and leaves the fighters alone. Nothing in the scene, in the culling nodes'
// own scale or in the models' matrices, says "twenty": it is in the vertices.
//
// So it is measured, off the one surface the layers might share: the floor. The game marks it for us -- its
// own shadows lie on it -- so their quads give the direction, and then, per viewport, the plane parallel to
// it carrying the most area gives that layer's floor. The ratio of two layers' floors is the ratio of their
// units, IF both of those planes really are the floor. That is the catch, and the reason only the layers
// measured at one -- the casters' own units -- are given a shadow: see where light[3] is filled in render().
// What this still buys is the knowing: it names which layers stand apart, and the floor it finds is also
// where the shadow stops, since nothing below the floor takes one.
void R3dGpu::measure_scales(const video::Scene &scene, const video::Raster &prep)
{
	for (float &s : vp_scale) s = 0;
	floor_p[0] = floor_p[1] = floor_p[2] = floor_p[3] = 0;
	shadow_tris = 0;
	auto vertex = [](const video::Raster::Tri &t, int k, float v[3]) {
		const float wv = 1.0f / t.iw[k];
		for (int q = 0; q < 3; q++) v[q] = t.a[k][q] * wv;
	};
	// Every face once: which viewport drew it, which way it faces, how big it is and where its middle lies.
	// One sweep, because the scene carries some twenty thousand triangles and a frame has 17 ms in it.
	flats.clear();
	float fn[3] = { 0, 0, 0 }, first_q[3] = { 0, 0, 0 };
	int quads = 0, quad_tris = 0;
	for (const video::Raster::Tri &t : prep.tris) {
		const video::Raster::Draw &dr = prep.draws[t.draw];
		const size_t iv = size_t(dr.vp - scene.viewports.data());
		if (iv >= 64) continue;
		float p[3][3];
		for (int k = 0; k < 3; k++) vertex(t, k, p[k]);
		const float u[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] };
		const float v[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
		Flat f;
		f.n[0] = u[1] * v[2] - u[2] * v[1];
		f.n[1] = u[2] * v[0] - u[0] * v[2];
		f.n[2] = u[0] * v[1] - u[1] * v[0];
		const float l = std::sqrt(f.n[0] * f.n[0] + f.n[1] * f.n[1] + f.n[2] * f.n[2]);
		if (dr.model->flattened) quad_tris++;
		if (!(l > 1e-12f)) continue;
		for (int q = 0; q < 3; q++) {
			f.n[q] /= l;
			f.mid[q] = (p[0][q] + p[1][q] + p[2][q]) / 3.0f;
		}
		f.area = l * 0.5f;
		f.vp = int(iv);
		// The floor's direction to start from: the game's own shadows lie on the floor, so their quads carry
		// it. It is only a start -- they are flattened by a matrix the game builds from the stage's light
		// angles -- so once a layer is known to hold the floor, the direction is taken from that layer's own
		// floor and everything is measured a second time.
		if (dr.model->flattened) {
			// The quads face the same way, but which way that is in numbers is not known yet, and adding
			// normals that disagree in sign cancels them out: measured, 14 pictures of 660 in one fight came
			// out with no floor direction at all, and on those the game's own shadows were drawn again. So
			// they are aligned on the first one, and the side the fighters stand on settles the sign after.
			if (!quads) for (int q = 0; q < 3; q++) first_q[q] = f.n[q];
			const float s = f.n[0] * first_q[0] + f.n[1] * first_q[1] + f.n[2] * first_q[2] < 0 ? -f.area : f.area;
			for (int q = 0; q < 3; q++) fn[q] += f.n[q] * s;
			quads++;
			continue;
		}
		flats.push_back(f);
	}
	auto unit = [](float v[3]) {
		const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		if (!(l > 1e-9f)) return false;
		for (int q = 0; q < 3; q++) v[q] /= l;
		return true;
	};
	shadow_tris = quad_tris;
	// No shadow triangle at all in the picture: the models are in the scene but none of them on screen, so
	// the game draws no shadow here either and there is nothing to replace. Measured in the attract loop,
	// that is every one of the pictures this used to count as lost.
	if (!quad_tris) { why[0]++; return; }
	if (!unit(fn)) { why[1]++; return; }
	// which way is up: the fighters stand ON the floor, so their faces lie on the side the shadow quads do
	// not. Both are measured along fn as it came out, and the sign is flipped if they disagree.
	{
		double qs = 0, cs = 0;
		int nq = 0, nc = 0;
		for (const video::Raster::Tri &t2 : prep.tris) {
			const video::Raster::Draw &dr2 = prep.draws[t2.draw];
			const size_t iv2 = size_t(dr2.vp - scene.viewports.data());
			if (iv2 >= 64 || !caster_vp[iv2]) continue;
			float p2[3][3];
			for (int k = 0; k < 3; k++) vertex(t2, k, p2[k]);
			const double off = (double(p2[0][0]) + p2[1][0] + p2[2][0]) / 3 * fn[0]
			                 + (double(p2[0][1]) + p2[1][1] + p2[2][1]) / 3 * fn[1]
			                 + (double(p2[0][2]) + p2[1][2] + p2[2][2]) / 3 * fn[2];
			if (dr2.model->flattened) { qs += off; nq++; }
			else { cs += off; nc++; }
		}
		if (nq && nc && cs / nc < qs / nq) for (float &q : fn) q = -q;
	}
	float plane[64] = {}, share[64] = {}, ref = 0;
	// how deep each layer's geometry goes, in its own units
	float zlo[64], zhi[64];
	for (int iv = 0; iv < 64; iv++) { zlo[iv] = 1e30f; zhi[iv] = -1e30f; }
	for (const Flat &f : flats) {
		zlo[f.vp] = std::min(zlo[f.vp], f.mid[2]);
		zhi[f.vp] = std::max(zhi[f.vp], f.mid[2]);
	}
	size_t ref_vp = 64;
	for (int pass = 0; pass < 2; pass++) {
		// each viewport's floor: among the faces turned the floor's way, the offset carrying the most area.
		// The window is a relative one, a hundredth of the offset, because the layers' units differ.
		for (float &p : plane) p = 0;
		for (float &p : share) p = 0;
		for (int iv = 0; iv < 64; iv++) {
			band.clear();
			for (const Flat &f : flats) {
				if (f.vp != iv) continue;
				if (std::fabs(f.n[0] * fn[0] + f.n[1] * fn[1] + f.n[2] * fn[2]) < 0.99f) continue;
				band.push_back({ f.mid[0] * fn[0] + f.mid[1] * fn[1] + f.mid[2] * fn[2], f.area });
			}
			if (band.size() < 3) continue;
			std::sort(band.begin(), band.end());
			float best = 0, whole = 0;
			for (const auto &b : band) whole += b.second;
			for (size_t i = 0, j = 0; i < band.size(); i++) {
				const float top = band[i].first + std::fabs(band[i].first) * 0.01f;
				float sum = 0;
				for (j = i; j < band.size() && band[j].first <= top; j++) sum += band[j].second;
				if (sum > best) { best = sum; plane[iv] = (band[i].first + band[j - 1].first) * 0.5f; }
			}
			share[iv] = whole > 0 ? best / whole : 0;
		}
		// the layer the casters stand in: the one whose floor is nearest their feet, in proportion. Their
		// feet are taken as the lowest of their faces along the floor's direction, near enough to tell one
		// layer from another when the layers differ by ten or twenty times.
		float feet = 1e30f;
		for (const Flat &f : flats)
			if (caster_vp[f.vp]) feet = std::min(feet, f.mid[0] * fn[0] + f.mid[1] * fn[1] + f.mid[2] * fn[2]);
		if (feet > 1e29f) { why[2]++; return; }
		// The floor is the flat surface NEAREST the fighters' feet. It was "the highest one at or below
		// them", which is the same thing when they stand on it and sounder when they leave the ground -- but
		// it turns on a tolerance, and that tolerance decided a stage.
		//
		// Measured over 857 pictures of Honey's stage, played: the floor at -1.15 sits a hair ABOVE the
		// lowest face of a fighter (-1.30, a foot dipping through it), so the old test threw it out as "not
		// the floor", and the only candidate left was the stage's far layer at -220. The port therefore
		// changed its mind about where the floor was on one picture in three -- vp13 532 times, vp12 255 --
		// by a factor of 180. Everything downstream leans on that plane: which layers share the casters'
		// units, and what counts as under the floor, where no shadow falls. So the ground's shadow came and
		// went with it.
		//
		// Nearest needs no tolerance, and it is right in both cases: a fighter in the air is still nearer
		// his floor at -1.3 than the far layer's at -220.
		float nearest = 1e30f;
		ref_vp = 64;
		for (int iv = 0; iv < 64; iv++) {
			if (plane[iv] == 0 || caster_vp[iv]) continue;
			const float gap = std::fabs(plane[iv] - feet);
			if (gap < nearest) { nearest = gap; ref_vp = size_t(iv); }
		}
		if (ref_vp >= 64) { why[3]++; return; }
		ref = plane[ref_vp];
		// VF3_SHADOW_WHY: how often the floor changes layer. It should not: everything that leans on it
		// -- which layers share the casters' units, what counts as under the floor -- changes with it.
		if (pass == 1 && getenv("VF3_SHADOW_WHY")) {
			static size_t said_ref = 64;
			static unsigned moves = 0, seen = 0;
			if (said_ref != 64 && ref_vp != said_ref) moves++;
			said_ref = ref_vp;
			if (++seen % 240 == 0)
				fprintf(stderr, "le sol change de couche %u fois sur %u images\n", moves, seen);
		}
		if (pass == 1) break;
		// the direction again, from that layer's own floor rather than from the flattened quads, and the
		// casters' feet along it
		float nf[3] = { 0, 0, 0 };
		for (const Flat &f : flats) {
			if (size_t(f.vp) != ref_vp) continue;
			const float dot = f.n[0] * fn[0] + f.n[1] * fn[1] + f.n[2] * fn[2];
			if (std::fabs(dot) < 0.99f) continue;
			const float off = f.mid[0] * fn[0] + f.mid[1] * fn[1] + f.mid[2] * fn[2];
			if (std::fabs(off - ref) > std::fabs(ref) * 0.01f) continue;
			for (int q = 0; q < 3; q++) nf[q] += f.n[q] * (dot < 0 ? -f.area : f.area);
		}
		if (!unit(nf)) { why[4]++; return; }
		for (int q = 0; q < 3; q++) fn[q] = nf[q];
	}
	// Which layers share the casters' units, and which do not. A layer may hold several surfaces turned the
	// floor's way -- a step, a ledge, the ground outside the ring -- so the widest of them is not proof of
	// anything on its own: measured on a stage of the attract loop, three layers that plainly share one space
	// (their depths run into one another, -9..-2, -28..-5, -25..-13) each show a widest flat surface lower
	// than the floor, which taken alone would read as 1.8 or 1.6 times the units.
	//
	// What settles it is depth. Two layers drawn in the same units, through the same frustum, lie at the same
	// depths; a layer drawn in other units lies far beyond. Measured: on the dam stage the far layer begins
	// at 49 and runs to 1455 while everything else ends at 11 -- four times beyond; on that attract stage the
	// three layers begin at 2, 5 and 13 and end at 9, 28 and 25 -- each within twice what the others reach.
	// So a layer joins the casters' own as long as it begins within TWICE the depth they already reach, which
	// grows as layers join; a layer that begins beyond that stands apart and is given the ratio its own floor
	// measures. Two is the one number here that measurement does not hand over: it is the gap between what
	// the two stages show, four times against one and a half.
	bool same[64] = {};
	float reach = -zlo[ref_vp];
	for (int iv = 0; iv < 64; iv++)
		if (caster_vp[iv] && zlo[iv] < 1e29f) reach = std::max(reach, -zlo[iv]);
	for (bool again = true; again; ) {
		again = false;
		for (int iv = 0; iv < 64; iv++) {
			if (same[iv] || zlo[iv] > 1e29f) continue;
			if (-zhi[iv] > 2.0f * reach) continue;
			same[iv] = true;
			reach = std::max(reach, -zlo[iv]);
			again = true;
		}
	}
	for (int iv = 0; iv < 64; iv++) {
		if (caster_vp[iv] || same[iv]) { vp_scale[iv] = 1; continue; }
		if (plane[iv] == 0 || share[iv] < 0.25f) continue;
		const float s = plane[iv] / ref;
		if (s > 0.01f && s < 1000.0f) vp_scale[iv] = s;
	}
	for (int q = 0; q < 3; q++) floor_p[q] = fn[q];
	floor_p[3] = ref;
	// VF3_SCALE_LOG: the measured scales, whenever one moves by more than a hundredth -- how steady they are
	// from frame to frame, and what they are on other stages
	if (getenv("VF3_SCALE_LOG")) {
		static float said[64] = {};
		bool moved = false;
		for (int iv = 0; iv < 64; iv++)
			if (std::fabs(vp_scale[iv] - said[iv]) > 0.01f * std::max(1.0f, said[iv])) moved = true;
		if (moved) {
			fprintf(stderr, "echelles :");
			for (int iv = 0; iv < 64; iv++) {
				said[iv] = vp_scale[iv];
				if (vp_scale[iv] != 0 || plane[iv] != 0)
					fprintf(stderr, " vp%d p%d %.3f/%.3f z %.1f..%.1f", iv,
					        iv < int(scene.viewports.size()) ? scene.viewports[iv].priority : -1,
					        double(vp_scale[iv]), double(plane[iv] / (ref != 0 ? ref : 1)),
					        double(zlo[iv]), double(zhi[iv]));
			}
			fprintf(stderr, " (sol vp%zu a %.3f, direction [%.4f %.4f %.4f])\n",
			        ref_vp, double(ref), double(fn[0]), double(fn[1]), double(fn[2]));
		}
	}
	if (debug_now) {
		fprintf(stderr, "  le sol : direction [%.4f %.4f %.4f], vp%zu a %.3f\n",
		        double(fn[0]), double(fn[1]), double(fn[2]), ref_vp, double(ref));
		for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++)
			if (plane[iv] != 0 || vp_scale[iv] != 0)
				fprintf(stderr, "  vp%zu: sol a %.3f, echelle %.4f, ambiante %.3f, soleil %.3f\n", iv,
				        double(plane[iv]), double(vp_scale[iv]), double(scene.viewports[iv].ambient),
				        double(scene.viewports[iv].sun_intensity));
	}
}
// Where the light is, and what each map has to cover -- read from the scene, and from the game's own shadows
// rather than from any sun it happens to carry.
//
// A shadow piece's matrix is S = C.P.R: the camera, the projection that flattens it, the limb's own rotation.
// C and R being orthonormal, S.S^T = C.P.P^T.C^T no longer depends on the limb, so every piece of one fighter
// gives the same six numbers -- measured, they agree to five decimals. And with N the floor's direction, a
// projection onto the floor along d is P = I - k.N^T with k = d/(d.N), so that
//
//     S.S^T = I - N.N^T + w.w^T   with   w = k - N,   w perpendicular to N
//
// which is the identity plus a rank-one term. So w.w^T = S.S^T - I + N.N^T is read straight off, w with it,
// and d = N + w. Checked at the dam against the direction the previous measurement fitted offline, and
// against the stage's own sun: the same to four decimals.
//
// This is done per caster viewport, because the game does not always flatten the two fighters the same way.
// Measured at a fight frame: at the dam the two fighters' S.S^T are the same to five decimals, while at
// stages 3, 10 and 11 -- where the game works the light out from each fighter's POSITION -- they are not
// (2.2595 against 1.0389 on the first of the six). So the viewports are grouped by the direction they give,
// and each group gets a map of its own, up to two. One group, one map, which is every other stage.
void R3dGpu::fit_light(const video::Scene &scene, const video::Raster &prep)
{
	nmaps = 0;
	light_ok = false;
	light_refused = false;
	shadow_models = 0;
	shadow_tris = 0;   // measure_scales sets it, and it is not reached when the scene carries no shadows
	for (bool &b : caster_vp) b = false;
	for (int &g : vp_group) g = -1;
	// the shadows' own places, which is where the ground is
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	int n = 0;
	double sst[64][6] = {};
	int pieces[64] = {};
	static constexpr int MAXP = 64;   // pieces kept per viewport for the grouping below
	static double psst[64][MAXP][6];
	// where each viewport's shadow sits, and where its fighter sits: the shadow falls AWAY from the light,
	// so the step from one to the other settles which way the light leans (below)
	double smid[64][3] = {}, cmid[64][3] = {};
	int sn[64] = {}, cn[64] = {};
	for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
		for (const video::Model &m : scene.viewports[iv].models) {
			if (!m.flattened) continue;
			const float *M = m.matrix.m;
			const float p[3] = { M[12], M[13], M[14] };
			for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
			const double r0[3] = { M[0], M[4], M[8] }, r1[3] = { M[1], M[5], M[9] }, r2[3] = { M[2], M[6], M[10] };
			const double s[6] = {
				r0[0]*r0[0]+r0[1]*r0[1]+r0[2]*r0[2], r1[0]*r1[0]+r1[1]*r1[1]+r1[2]*r1[2],
				r2[0]*r2[0]+r2[1]*r2[1]+r2[2]*r2[2], r0[0]*r1[0]+r0[1]*r1[1]+r0[2]*r1[2],
				r0[0]*r2[0]+r0[1]*r2[1]+r0[2]*r2[2], r1[0]*r2[0]+r1[1]*r2[1]+r1[2]*r2[2] };
			// each piece's own S.S^T, not just their sum: whether they agree is what says the light can be
			// read at all (below)
			if (pieces[iv] < MAXP) {
				for (int q = 0; q < 6; q++) psst[iv][pieces[iv]][q] = s[q];
			}
			for (int q = 0; q < 6; q++) sst[iv][q] += s[q];
			pieces[iv]++;
			caster_vp[iv] = true;
			n++;
			for (int q = 0; q < 3; q++) smid[iv][q] += p[q];
			sn[iv]++;
		}
	}
	shadow_models = n;
	if (!n) { why[0]++; return; }
	// and where the fighters themselves are, in the same viewports
	for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
		if (!caster_vp[iv]) continue;
		for (const video::Model &m : scene.viewports[iv].models) {
			if (m.flattened) continue;
			const float *M = m.matrix.m;
			for (int q = 0; q < 3; q++) cmid[iv][q] += M[12 + q];
			cn[iv]++;
		}
	}
	// the floor, and the units each layer is drawn in
	measure_scales(scene, prep);
	const float *N = floor_p;
	if (!(N[0] || N[1] || N[2])) return;   // measure_scales has already said why
	// VF3_SHADOW_WHY: a ring out, found without asking the game. In Fighting Vipers 2 the casters' own
	// viewport holds the fighters and nothing else -- the stage is drawn in its own layers, at twenty times
	// the unit -- so the models there that are not flattened ARE the fighters. A fighter thrown out of the
	// ring goes under the floor plane and stays there, which nothing else in that viewport does. The line
	// says how far under, with the pieces the game is still flattening, so that a picture where the shadow
	// goes missing can be told apart from one where the game stopped casting it.
	if (getenv("VF3_SHADOW_WHY")) {
		static unsigned tick = 0, said = 0;
		tick++;
		float lowest = 1e30f;
		int bodies = 0;
		for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
			if (!caster_vp[iv]) continue;
			for (const video::Model &m : scene.viewports[iv].models) {
				if (m.flattened) continue;
				const float *M = m.matrix.m;
				const float h = N[0] * M[12] + N[1] * M[13] + N[2] * M[14] - floor_p[3];
				lowest = std::min(lowest, h);
				bodies++;
			}
		}
		if (lowest < -0.3f && lowest < 1e29f && tick - said > 10) {
			said = tick;
			fprintf(stderr, "sous le sol : %.2f, %d corps, %d pieces aplaties, image %u\n",
			        double(lowest), bodies, n, tick);
		}
	}
	// each caster's own direction, out of its pieces' S.S^T
	float dirs[64][3] = {};
	const float fn0[3] = { N[0], N[1], N[2] };
	bool per_piece[64] = {};
	for (size_t iv = 0; iv < 64; iv++) {
		if (!caster_vp[iv] || !pieces[iv]) continue;
		double a[6];
		for (int q = 0; q < 6; q++) a[q] = sst[iv][q] / pieces[iv];
		// The direction, piece by piece, and not from their mean.
		//
		// Fighting Vipers 2 flattens a whole fighter onto ONE plane, so every piece gives the same S.S^T --
		// measured at a fight frame, they agree to five decimals -- and their mean was the light. Virtua
		// Fighter 3's ground has relief: a piece is flattened onto the plane UNDER IT, so the plane's
		// normal differs from piece to piece and so does S.S^T. Measured: three groups in one viewport,
		// 0.023 apart, where Fighting Vipers 2 shows 0.00000. Their mean is then a plane that exists
		// nowhere, and the light read off it swung 5.55 degrees a picture, 123.7 at worst, over 2040
		// pictures -- which is what showed as the lighting turning about and blinking.
		//
		// But the LIGHT is the same for every piece; only the plane changes. So each piece gives up its own
		// plane and its own direction, and it is the directions that are averaged. For one piece, S.S^T is
		// I - N.N^T + w.w^T with N the plane it was flattened onto: N is what S.S^T sends to zero, which is
		// the cross product of two of its rows, and w.w^T = S.S^T - I + N.N^T is rank one as before.
		{
			double acc[3] = { 0, 0, 0 }, resid = 0;
			int seen_p = 0, good_p = 0;
			constexpr double PLANE = 0.04;
			int got = 0;
			for (int i = 0; i < std::min(pieces[iv], MAXP); i++) {
				const double *s = psst[iv][i];
				const double r[3][3] = { { s[0], s[3], s[4] }, { s[3], s[1], s[5] }, { s[4], s[5], s[2] } };
				double nn[3] = { 0, 0, 0 }, bl = 0;
				for (int u = 0; u < 3; u++) {
					const int v = (u + 1) % 3;
					const double c[3] = { r[u][1] * r[v][2] - r[u][2] * r[v][1],
					                      r[u][2] * r[v][0] - r[u][0] * r[v][2],
					                      r[u][0] * r[v][1] - r[u][1] * r[v][0] };
					const double l2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
					if (l2 > bl) { bl = l2; for (int q = 0; q < 3; q++) nn[q] = c[q]; }
				}
				if (!(bl > 1e-18)) continue;
				const double nl = std::sqrt(bl);
				for (int q = 0; q < 3; q++) nn[q] /= nl;
				// turned the floor's way, so that the pieces' directions can be added up
				if (nn[0] * fn0[0] + nn[1] * fn0[1] + nn[2] * fn0[2] < 0) for (double &q : nn) q = -q;
				double b2[3][3];
				for (int u = 0; u < 3; u++)
					for (int v = 0; v < 3; v++)
						b2[u][v] = r[u][v] - (u == v ? 1.0 : 0.0) + nn[u] * nn[v];
				const double ww = std::max(0.0, b2[0][0] + b2[1][1] + b2[2][2]);
				int bc = 0;
				double bn = -1;
				for (int c2 = 0; c2 < 3; c2++) {
					const double l2 = b2[0][c2] * b2[0][c2] + b2[1][c2] * b2[1][c2] + b2[2][c2] * b2[2][c2];
					if (l2 > bn) { bn = l2; bc = c2; }
				}
				double w2[3] = { 0, 0, 0 };
				if (bn > 1e-12) {
					const double sc = std::sqrt(ww / bn);
					for (int q = 0; q < 3; q++) w2[q] = b2[q][bc] * sc;
				}
				// Is this piece's matrix a planar projection at all? Everything above rests on it: only
				// then is S.S^T equal to I - N.N^T + w.w^T, and only then does d = N + w mean anything. So
				// the fit is put back together and held against what was measured.
				double pr = 0;
				for (int u = 0; u < 3; u++)
					for (int v = 0; v < 3; v++) {
						const double back = (u == v ? 1.0 : 0.0) - nn[u] * nn[v] + w2[u] * w2[v];
						pr = std::max(pr, std::fabs(back - r[u][v]));
					}
				resid = std::max(resid, pr);
				seen_p++;
				// PLANE: how far a piece may sit from the planar projection rebuilt out of it and still
				// count. Measured per piece, not per viewport:
				//
				//     Fighting Vipers 2   0.0002 at a quiet picture, 0.031 at the worst of thirteen arenas
				//     Virtua Fighter 3    0.058 at its lowest, 0.081 at its worst, and not ONE piece below
				//
				// 0.04 sits between the two with margin on both sides. The old 0.005 was read off the dam
				// alone, where every piece is within a millionth; the other twelve arenas are not so clean.
				if (pr <= PLANE) good_p++;
				// A piece that is not a planar projection says nothing about the light: it is counted and
				// left out, instead of poisoning the mean and, through the gate below, the whole picture.
				if (pr > PLANE) continue;
				// w's sign is free, so d came out one of two mirror images: take the one leaning the way
				// the pieces counted so far lean
				double dd[3] = { nn[0] + w2[0], nn[1] + w2[1], nn[2] + w2[2] };
				if (got && (dd[0] * acc[0] + dd[1] * acc[1] + dd[2] * acc[2]) < 0)
					for (int q = 0; q < 3; q++) dd[q] = nn[q] - w2[q];
				const double dl2 = std::sqrt(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]);
				if (!(dl2 > 1e-9)) continue;
				for (int q = 0; q < 3; q++) acc[q] += dd[q] / dl2;
				got++;
			}
			const double al = std::sqrt(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
			// A light that cannot be read is not invented. Measured, the worst gap between a piece's
			// S.S^T and the planar projection put back together from it:
			//
			//     Fighting Vipers 2   0.000000, worst 0.000001, over 16000 pieces
			//     Virtua Fighter 3    0.069551, worst 0.081600, over 64000 pieces
			//
			// So Virtua Fighter 3 does not flatten its shadows onto a plane at all -- its ground has
			// relief, and whatever it does instead is not a projection. Reading a light out of it gave
			// the pieces of ONE picture disagreeing by 6.7 degrees on average and 61 at worst, which is
			// what showed as the lighting turning about, blinking and going out. That viewport is left
			// out, and with no caster left the game draws its own shadows as it always did. 0.005 is the
			// one number measurement does not hand over; it sits between a millionth and seven hundredths.
			// VF3_SHADOW_FORCE: go ahead anyway, to look at what the refusal is turning down
			// VF3_SHADOW_WHY: how many of this viewport's pieces ARE planar projections. The gate below
			// is the WORST piece, so one doubtful piece condemns the whole picture -- and Fighting
			// Vipers 2 has occasional bad pieces where Virtua Fighter 3 has nothing but. The count is
			// what tells the two apart; the worst no longer does.
			if (getenv("VF3_SHADOW_WHY")) {
				static unsigned said = 0;
				if (++said <= 6 || (said % 300) == 0)
					fprintf(stderr, "planeite : vp%zu, %d pieces sur %d sont planes, pire %.6f\n",
						iv, good_p, seen_p, resid);
			}
			static const bool forced = getenv("VF3_SHADOW_FORCE") != nullptr;
			if (good_p * 2 < seen_p && !forced) {
				// VF3_SHADOW_WHY: by how much, and where. The gate was set from Fighting Vipers 2 showing
				// 0.000001 and Virtua Fighter 3 0.08; an arena of Fighting Vipers 2 tripping it is new.
				if (getenv("VF3_SHADOW_WHY")) {
					static double worst_r = 0;
					static unsigned n_r = 0;
					double sum = 0;
					(void)sum;
					if (resid > worst_r) worst_r = resid;
					if (++n_r <= 10 || (n_r % 200) == 0)
						fprintf(stderr, "vue refusee : vp%zu, %d pieces planes sur %d, pire %.6f (pire vu %.6f, %u fois)\n",
							iv, good_p, seen_p, resid, worst_r, n_r);
				}
				caster_vp[iv] = false;
				light_refused = true;
				why[2]++;
				continue;
			}
			if (got && al > 1e-9) {
				for (int q = 0; q < 3; q++) dirs[iv][q] = float(acc[q] / al);
				per_piece[iv] = true;
			}
		}
		double worst = 0;
		for (int i = 0; i < std::min(pieces[iv], MAXP); i++)
			for (int q = 0; q < 6; q++) worst = std::max(worst, std::fabs(psst[iv][i][q] - a[q]));
		if (debug_now)
			fprintf(stderr, "  vp%zu: %d pieces, ecart maximum a la moyenne %.5f\n", iv, pieces[iv], worst);
		// Pieces that disagree are expected now -- a ground with relief gives each its own plane -- so this
		// is only told, not acted on. (void) keeps it alive when nothing is being told.
		(void)worst;
		// w.w^T = S.S^T - I + N.N^T, a rank-one matrix: its biggest column is w, and its trace is w.w
		double b[3][3] = {
			{ a[0] - 1 + double(N[0]) * N[0], a[3] + double(N[0]) * N[1], a[4] + double(N[0]) * N[2] },
			{ a[3] + double(N[1]) * N[0], a[1] - 1 + double(N[1]) * N[1], a[5] + double(N[1]) * N[2] },
			{ a[4] + double(N[2]) * N[0], a[5] + double(N[2]) * N[1], a[2] - 1 + double(N[2]) * N[2] } };
		const double ww = std::max(0.0, b[0][0] + b[1][1] + b[2][2]);
		int best = 0;
		double bl = -1;
		for (int c = 0; c < 3; c++) {
			const double l = b[0][c] * b[0][c] + b[1][c] * b[1][c] + b[2][c] * b[2][c];
			if (l > bl) { bl = l; best = c; }
		}
		if (!(bl > 1e-12)) {      // w is zero: the light is straight down the floor's own direction
			if (!per_piece[iv]) for (int k = 0; k < 3; k++) dirs[iv][k] = N[k];
			continue;
		}
		const double s = std::sqrt(ww / bl);
		double w[3] = { b[0][best] * s, b[1][best] * s, b[2][best] * s };
		// The sign the column came out with is arbitrary: N + w and N - w are mirror images about the floor's
		// own direction, and only one of them is the light. What settles it is where the shadow landed. A
		// point h above the floor casts at h.w away from itself, so the step from the fighter to its shadow
		// runs along -w. It was this viewport's sun before, which works in Fighting Vipers 2 and not in
		// Virtua Fighter 3, where every viewport carries the same [0 1 0] and it says nothing about the
		// geometry -- measured, the direction came out 45 degrees off, mirrored.
		if (sn[iv] && cn[iv]) {
			double step[3];
			for (int q = 0; q < 3; q++) step[q] = smid[iv][q] / sn[iv] - cmid[iv][q] / cn[iv];
			if (step[0] * w[0] + step[1] * w[1] + step[2] * w[2] > 0) for (double &q : w) q = -q;
		}
		float d[3] = { float(N[0] + w[0]), float(N[1] + w[1]), float(N[2] + w[2]) };
		const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		// the pieces' own answer wins when they gave one; this is the whole-viewport fallback
		if (!(dl > 1e-6f)) { if (!per_piece[iv]) for (int k = 0; k < 3; k++) dirs[iv][k] = N[k]; continue; }
		if (!per_piece[iv]) for (int k = 0; k < 3; k++) dirs[iv][k] = d[k] / dl;
	}
	// One refused viewport refuses the picture. Letting the others through would light some pictures and
	// not the next, which is the blinking itself; and a game whose shadows are not planar projections
	// has no light to read, not a light read from part of it.
	if (light_refused) return;
	// VF3_SHADOW_TABLE=1: take the light from the ARENA instead of reading it out of the game's
	// shadow matrices every picture.
	//
	// The direction read above comes out of S = C.P.R, so it is in the CAMERA's space and it moves
	// when the camera does: measured over 720 pictures of a fight at the dam, 0.27 degrees a picture
	// and 16.3 at worst, eleven of them past 5 degrees -- the camera's own cuts. Carried back through
	// the viewport's `root`, which is the camera's matrix, the same direction holds still: 0.038
	// degrees a picture, 4.4 at worst, not one past 5. So `root` really is the camera here, and the
	// light belongs to the arena. That is the vector this table holds, and it is turned back into the
	// camera's space with each viewport's own root.
	//
	// The thirteen were taken one arena per game, each loaded by the game's own loader (VF3_STAGE),
	// with the journal naming the arena that came up. Both fighter viewports agreed to four decimals
	// in every one of them, and four of the values land on exact axes -- [0 1 0] at ROOM_00 and
	// FURNACE, z = 0 at HANGAR, x = 0 at WESTERN -- which a noisy reading does not do.
	static const bool table_on = getenv("VF3_SHADOW_TABLE") != nullptr;
	if (table_on && arena >= 0 && arena < 13) {
		static const float ARENE[13][3] = {
			{ -0.2002f, 0.8476f,  0.4915f },   //  0 TEST
			{ -0.0004f, 0.8318f,  0.5551f },   //  1 WESTERN
			{  0.2560f, 0.8946f, -0.3664f },   //  2 AC_CARRIER
			{  0.0000f, 1.0000f,  0.0000f },   //  3 ROOM_00, straight down
			{  0.7071f, 0.7071f, -0.0013f },   //  4 GREAT_BM -- 45 degrees exactly, but read off an
			                                   //    arena that does not load this way: six flattened
			                                   //    pieces instead of 36. Kept, not trusted.
			{  0.0000f, 1.0000f,  0.0000f },   //  5 FURNACE, straight down
			{  0.4387f, 0.8407f, -0.3177f },   //  6 STORE
			{  0.3819f, 0.9242f, -0.0001f },   //  7 HANGAR
			{ -0.2008f, 0.8470f,  0.4923f },   //  8 CAPSULE
			{  0.2356f, 0.8601f,  0.4525f },   //  9 DAM
			{ -0.2009f, 0.8472f,  0.4920f },   // 10 TOWER
			{ -0.1526f, 0.6076f,  0.7795f },   // 11 BUILDING
			{ -0.2012f, 0.8478f,  0.4907f },   // 12 MUSEUM
		};
		const float *a = ARENE[arena];
		for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
			if (!caster_vp[iv] || !pieces[iv]) continue;
			// back into the camera's space: root carries a direction forward, its transpose carried it
			// back when the table was taken
			const float *R = scene.viewports[iv].root.m;
			float d[3];
			for (int r = 0; r < 3; r++)
				d[r] = R[0 + r] * a[0] + R[4 + r] * a[1] + R[8 + r] * a[2];
			const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
			if (dl > 1e-6f) for (int q = 0; q < 3; q++) dirs[iv][q] = d[q] / dl;
			if (debug_now)
				fprintf(stderr, "table : arene %d, vp%zu prend [%.4f %.4f %.4f]\n", arena, iv,
					double(dirs[iv][0]), double(dirs[iv][1]), double(dirs[iv][2]));
		}
	}
	// the groups: viewports whose directions agree to within a degree share a map, up to two maps
	float gdir[2][3] = {};
	for (size_t iv = 0; iv < 64; iv++) {
		if (!caster_vp[iv] || !pieces[iv]) continue;
		int g = -1;
		for (int q = 0; q < nmaps; q++) {
			const float dot = dirs[iv][0] * gdir[q][0] + dirs[iv][1] * gdir[q][1] + dirs[iv][2] * gdir[q][2];
			if (dot > 0.99985f) { g = q; break; }   // a degree
		}
		if (g < 0 && nmaps < 2) { g = nmaps++; for (int k = 0; k < 3; k++) gdir[g][k] = dirs[iv][k]; }
		if (g < 0) g = 0;   // a third direction would need a third map; it joins the first
		vp_group[iv] = g;
	}
	// a viewport whose pieces disagree casts nothing: with none left there is no light to read
	{
		int left = 0;
		for (int iv = 0; iv < 64; iv++) if (caster_vp[iv]) left++;
		if (!left) {
			if (getenv("VF3_SHADOW_WHY")) {
				static unsigned said = 0;
				if (++said <= 4 || (said % 100) == 0)
					fprintf(stderr, "plus aucun projeteur : %u fois\n", said);
			}
			why[2]++;
			return;
		}
	}
	if (!nmaps) { why[5]++; return; }
	// VF3_SHADOW_WHY: how steady the direction is. It is read per caster from the game's own shadow
	// matrices, but its SIGN is settled by where the shadow sits against its caster -- and a viewport
	// that holds the stage as well as the fighters makes that comparison meaningless.
	if (getenv("VF3_SHADOW_WHY")) {
		static float said[64][3] = {};
		static unsigned flips = 0, moves = 0, seen = 0, over5 = 0, n_ang = 0;
		static double sum_ang = 0, max_ang = 0;
		seen++;
		for (int iv = 0; iv < 64; iv++) {
			if (!caster_vp[iv] || !pieces[iv]) continue;
			const float dot = dirs[iv][0] * said[iv][0] + dirs[iv][1] * said[iv][1] + dirs[iv][2] * said[iv][2];
			if (said[iv][0] || said[iv][1] || said[iv][2]) {
				const double ang = std::acos(std::min(1.0, std::max(-1.0, double(dot)))) * 57.2957795;
				if (dot < 0) flips++;
				else if (dot < 0.999f) moves++;
				sum_ang += ang;
				n_ang++;
				if (ang > max_ang) max_ang = ang;
				if (ang > 5) over5++;
			}
			for (int q = 0; q < 3; q++) said[iv][q] = dirs[iv][q];
		}
		static float saidN[3] = {};
		static unsigned nmoves = 0;
		{
			const float dot = floor_p[0] * saidN[0] + floor_p[1] * saidN[1] + floor_p[2] * saidN[2];
			if ((saidN[0] || saidN[1] || saidN[2]) && dot < 0.999f) nmoves++;
			for (int q = 0; q < 3; q++) saidN[q] = floor_p[q];
		}
		// Is the light we read fixed to the ARENA or to the CAMERA? It comes out of S = C.P.R, so it is
		// in the camera's space, and a table of fixed directions would only work if that space held
		// still. The viewport's `root` is the camera's own matrix where the game puts one there, so the
		// same direction is also carried back through it -- R^T.d, the direction before the camera --
		// and the two are followed picture to picture. The steadier of the two is the one a table can
		// hold.
		static float said_w[64][3] = {};
		static double wsum = 0, wmax = 0;
		static unsigned wn = 0, wover5 = 0;
		for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
			if (!caster_vp[iv] || !pieces[iv]) continue;
			const float *R = scene.viewports[iv].root.m;
			float w[3];
			for (int i = 0; i < 3; i++)
				w[i] = R[4 * i + 0] * dirs[iv][0] + R[4 * i + 1] * dirs[iv][1] + R[4 * i + 2] * dirs[iv][2];
			const float wl = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
			if (!(wl > 1e-6f)) continue;
			for (float &q : w) q /= wl;
			if (said_w[iv][0] || said_w[iv][1] || said_w[iv][2]) {
				const double dw = double(w[0]) * said_w[iv][0] + double(w[1]) * said_w[iv][1] + double(w[2]) * said_w[iv][2];
				const double ang = std::acos(std::min(1.0, std::max(-1.0, std::fabs(dw)))) * 57.2957795;
				wsum += ang;
				wn++;
				if (ang > wmax) wmax = ang;
				if (ang > 5) wover5++;
			}
			for (int q = 0; q < 3; q++) said_w[iv][q] = w[q];
		}
		if ((seen % 120) == 0)
			fprintf(stderr, "avant la camera : %u mesures, moyenne %.3f deg, pire %.1f deg, %u au dela de 5 deg" "\n",
				wn, wn ? wsum / wn : 0.0, wmax, wover5);
		// Could the light come from the game's OWN sun instead of its shadow matrices? Each viewport
		// carries a sun direction, and if it agreed with the direction read out of the flattened pieces
		// the port would not need those pieces at all. So the two are held against each other, picture
		// after picture, and the gap is reported in degrees.
		static double sun_sum = 0, sun_max = 0;
		static unsigned sun_n = 0, sun_flat = 0;
		for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
			if (!caster_vp[iv] || !pieces[iv]) continue;
			const float *s = scene.viewports[iv].sun;
			const float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
			if (!(sl > 1e-6f)) { sun_flat++; continue; }
			double d2 = 0;
			for (int q = 0; q < 3; q++) d2 += double(dirs[iv][q]) * (s[q] / sl);
			const double ang = std::acos(std::min(1.0, std::max(-1.0, std::fabs(d2)))) * 57.2957795;
			sun_sum += ang;
			sun_n++;
			if (ang > sun_max) sun_max = ang;
		}
		if ((seen % 120) == 0)
			fprintf(stderr, "le soleil du jeu contre la lumiere lue : %u mesures, moyenne %.2f deg, pire %.1f deg, %u viewports sans soleil" "\n",
				sun_n, sun_n ? sun_sum / sun_n : 0.0, sun_max, sun_flat);
		if ((seen % 120) == 0)
			fprintf(stderr, "direction : %u images, %u retournements, %u ecarts (moyenne %.2f deg, pire %.1f deg, %u au dela de 5 deg) ; le sol bouge %u fois\n",
			        seen, flips, moves, n_ang ? sum_ang / n_ang : 0.0, max_ang, over5, nmoves);
	}
	const float ground = std::max(lo[1], hi[1]);
	for (int g = 0; g < nmaps; g++) {
		// The box, read off that group's casters' own VERTICES. A model's place is one point and says nothing
		// of its size, and a box fitted to the shadows' places alone left the fighters outside it -- measured,
		// the map came out empty for 143 draws. prep.tris already carries every vertex in the camera's space,
		// the space this matrix works in, so the box is simply read off them, then taken down to the floor and
		// widened the way the ray runs so that what RECEIVES a shadow is covered too.
		const float *d = gdir[g];
		float blo[3] = { 1e30f, 1e30f, 1e30f }, bhi[3] = { -1e30f, -1e30f, -1e30f };
		int nv = 0;
		for (const video::Raster::Tri &t : prep.tris) {
			const video::Raster::Draw &dr = prep.draws[t.draw];
			const size_t ivv = size_t(dr.vp - scene.viewports.data());
			if (ivv >= 64 || vp_group[ivv] != g) continue;
			if (dr.model->flattened) continue;
			for (int k = 0; k < t.n; k++) {
				// a vertex keeps its attributes divided by w (that is how they interpolate): w back on, and it
				// is the position in the camera's space again -- the space this matrix works in
				const float wv = 1.0f / t.iw[k];
				for (int q = 0; q < 3; q++) {
					const float v = t.a[k][q] * wv;
					blo[q] = std::min(blo[q], v);
					bhi[q] = std::max(bhi[q], v);
				}
				nv++;
			}
		}
		if (nv < 3) {
			// VF3_SHADOW_WHY: say WHICH viewports were looked at and what they held. The light is lost
			// here on a good half of a fight's pictures at WESTERN, and a counter alone does not say why.
			if (getenv("VF3_SHADOW_WHY")) {
				static unsigned said = 0;
				if (++said <= 20) {
					fprintf(stderr, "boite vide : groupe %d sur %d, %zu triangles prepares ;", g, nmaps, prep.tris.size());
					for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++)
						if (caster_vp[iv])
							fprintf(stderr, " vp%zu(%d pieces, groupe %d, %zu modeles)", iv, pieces[iv], vp_group[iv],
								scene.viewports[iv].models.size());
					fprintf(stderr, "\n");
				}
			}
			nmaps = 0;
			why[6]++;
			return;
		}
		const float PAD = 0.3f;
		blo[0] -= PAD; bhi[0] += PAD;
		blo[2] -= PAD; bhi[2] += PAD;
		blo[1] = std::min(blo[1], ground) - 0.2f;
		bhi[1] += 0.2f;
		const float tall = bhi[1] - blo[1];
		const float ox = d[1] > 0.1f ? tall * d[0] / d[1] : 0.0f;
		const float oz = d[1] > 0.1f ? tall * d[2] / d[1] : 0.0f;
		blo[0] = std::min(blo[0], blo[0] - ox); bhi[0] = std::max(bhi[0], bhi[0] - ox);
		blo[2] = std::min(blo[2], blo[2] - oz); bhi[2] = std::max(bhi[2], bhi[2] - oz);
		// the light looks along -d, with a frame around it
		const float z[3] = { -d[0], -d[1], -d[2] };
		float up[3] = { 0, 1, 0 };
		if (std::fabs(z[1]) > 0.99f) { up[1] = 0; up[2] = 1; }
		float x[3] = { up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0] };
		const float xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
		if (!(xl > 1e-6f)) { nmaps = 0; why[5]++; return; }
		for (int k = 0; k < 3; k++) x[k] /= xl;
		const float y[3] = { z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0] };
		const float c[3] = { (blo[0] + bhi[0]) * 0.5f, (blo[1] + bhi[1]) * 0.5f, (blo[2] + bhi[2]) * 0.5f };
		float ex[2] = { 1e30f, -1e30f }, ey[2] = { 1e30f, -1e30f }, ez[2] = { 1e30f, -1e30f };
		for (int i = 0; i < 8; i++) {
			const float p[3] = { (i & 1) ? bhi[0] : blo[0], (i & 2) ? bhi[1] : blo[1], (i & 4) ? bhi[2] : blo[2] };
			const float q[3] = { p[0] - c[0], p[1] - c[1], p[2] - c[2] };
			const float qx = x[0] * q[0] + x[1] * q[1] + x[2] * q[2];
			const float qy = y[0] * q[0] + y[1] * q[1] + y[2] * q[2];
			const float qz = z[0] * q[0] + z[1] * q[1] + z[2] * q[2];
			ex[0] = std::min(ex[0], qx); ex[1] = std::max(ex[1], qx);
			ey[0] = std::min(ey[0], qy); ey[1] = std::max(ey[1], qy);
			ez[0] = std::min(ez[0], qz); ez[1] = std::max(ez[1], qz);
		}
		const float hx = std::max(1e-3f, (ex[1] - ex[0]) * 0.5f), hy = std::max(1e-3f, (ey[1] - ey[0]) * 0.5f);
		const float cx = (ex[0] + ex[1]) * 0.5f, cy = (ey[0] + ey[1]) * 0.5f;
		const float z0 = ez[0] - 0.5f, dz = std::max(1e-3f, (ez[1] + 0.5f) - z0);
		const float dxc = x[0] * c[0] + x[1] * c[1] + x[2] * c[2];
		const float dyc = y[0] * c[0] + y[1] * c[1] + y[2] * c[2];
		const float dzc = z[0] * c[0] + z[1] * c[1] + z[2] * c[2];
		float *m = light_m[g];
		auto row = [&](int r, const float a[3], float sc, float off) {
			m[r * 4 + 0] = a[0] * sc; m[r * 4 + 1] = a[1] * sc;
			m[r * 4 + 2] = a[2] * sc; m[r * 4 + 3] = off;
		};
		row(0, x, 1.0f / hx, (-dxc - cx) / hx);
		row(1, y, 1.0f / hy, (-dyc - cy) / hy);
		row(2, z, 1.0f / dz, (-dzc - z0) / dz);
		m[12] = m[13] = m[14] = 0;
		m[15] = 1;
		if (debug_now)
			fprintf(stderr, "light %d: [%.4f %.4f %.4f], %d caster vertices, box x %.2f..%.2f y %.2f..%.2f "
			        "z %.2f..%.2f, half %.2f %.2f, depth %.2f\n", g, double(d[0]), double(d[1]), double(d[2]),
			        nv, blo[0], bhi[0], blo[1], bhi[1], blo[2], bhi[2], hx, hy, dz);
	}
	light_ok = true;
	if (debug_now) {
		fprintf(stderr, "%d shadow models, %d map%s\n", n, nmaps, nmaps > 1 ? "s" : "");
		for (size_t iv = 0; iv < 64; iv++)
			if (caster_vp[iv]) {
				// and the same direction carried back before the camera, which is where it holds still:
				// measured on the dam, 0.038 deg a picture and never past 4.4, against 0.27 and 16.3 in
				// the camera's own space. That is the vector a per-arena table can hold.
				const float *R = scene.viewports[iv].root.m;
				float w[3] = {};
				for (int i = 0; i < 3; i++)
					w[i] = R[4 * i + 0] * dirs[iv][0] + R[4 * i + 1] * dirs[iv][1] + R[4 * i + 2] * dirs[iv][2];
				const float wl = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
				if (wl > 1e-6f) for (float &q : w) q /= wl;
				fprintf(stderr, "  vp%zu: %d pieces, direction [%.4f %.4f %.4f], avant camera [%.4f %.4f %.4f], map %d\n",
					iv, pieces[iv], double(dirs[iv][0]), double(dirs[iv][1]), double(dirs[iv][2]),
					double(w[0]), double(w[1]), double(w[2]), vp_group[iv]);
			}
	}
}

bool R3dGpu::read_shadow_map(std::vector<float> &px, int &side, int which)
{
	side = SMAP;
	// only the maps this picture made: the others hold whatever was last drawn in them
	if (which < 0 || which >= nmaps || !smap[which]) return false;
	SDL_GPUTransferBufferCreateInfo ti = {};
	ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	ti.size = uint32_t(SMAP) * uint32_t(SMAP) * 4;
	SDL_GPUTransferBuffer *dl = SDL_CreateGPUTransferBuffer(dev, &ti);
	if (!dl) return false;
	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
	if (!cmd) { SDL_ReleaseGPUTransferBuffer(dev, dl); return false; }
	SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
	SDL_GPUTextureRegion src = {};
	src.texture = smap[which];
	src.w = uint32_t(SMAP);
	src.h = uint32_t(SMAP);
	src.d = 1;
	SDL_GPUTextureTransferInfo dst = { dl, 0, uint32_t(SMAP), uint32_t(SMAP) };
	SDL_DownloadFromGPUTexture(cp, &src, &dst);
	SDL_EndGPUCopyPass(cp);
	SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
	bool ok = f && SDL_WaitForGPUFences(dev, true, &f, 1);
	if (f) SDL_ReleaseGPUFence(dev, f);
	if (ok) {
		const void *m = SDL_MapGPUTransferBuffer(dev, dl, false);
		if (m) {
			px.resize(size_t(SMAP) * SMAP);
			memcpy(px.data(), m, px.size() * 4);
			SDL_UnmapGPUTransferBuffer(dev, dl);
		} else ok = false;
	}
	SDL_ReleaseGPUTransferBuffer(dev, dl);
	return ok;
}

bool R3dGpu::read_output(std::vector<uint32_t> &px)
{
	if (!out) return false;
	SDL_GPUTransferBufferCreateInfo ti = {};
	ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	const int ow = w / ss, oh = h / ss;
	ti.size = uint32_t(ow) * uint32_t(oh) * 4;
	SDL_GPUTransferBuffer *dl = SDL_CreateGPUTransferBuffer(dev, &ti);
	if (!dl) return false;
	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
	if (!cmd) { SDL_ReleaseGPUTransferBuffer(dev, dl); return false; }
	SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
	SDL_GPUTextureRegion src = {};
	src.texture = out;
	src.w = uint32_t(ow);
	src.h = uint32_t(oh);
	src.d = 1;
	SDL_GPUTextureTransferInfo dst = { dl, 0, uint32_t(ow), uint32_t(oh) };
	SDL_DownloadFromGPUTexture(cp, &src, &dst);
	SDL_EndGPUCopyPass(cp);
	SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
	bool ok = f && SDL_WaitForGPUFences(dev, true, &f, 1);
	if (f) SDL_ReleaseGPUFence(dev, f);
	if (ok) {
		const uint8_t *m = static_cast<const uint8_t *>(SDL_MapGPUTransferBuffer(dev, dl, false));
		if (m) {
			px.resize(size_t(ow) * oh);
			memcpy(px.data(), m, px.size() * 4);
			SDL_UnmapGPUTransferBuffer(dev, dl);
		} else ok = false;
	}
	SDL_ReleaseGPUTransferBuffer(dev, dl);
	return ok;
}

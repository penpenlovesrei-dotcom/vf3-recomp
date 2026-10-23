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
	// two samplers: the opaque layer's depth, and Fighting Vipers 2's shadow map (r3d.hlsl)
	SDL_GPUShader *fs_opq = shader(SH(fs_opq), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 3, 1);
	SDL_GPUShader *fs_tr = shader(SH(fs_tr), SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 3, 1);
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
	if (shadows) fit_light(scene, prep);
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
	if (shadows) {
		if (light_ok && make_shadow_map()) {
			SDL_GPUColorTargetInfo sc = {};
			sc.texture = smap;
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
			SDL_PushGPUVertexUniformData(cmd, 0, light_m, sizeof(light_m));
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
				if (iv >= 64 || (!all && !caster_vp[iv])) continue;
				if (dr.mesh->colour_index == video::SHADOW_COLOUR_INDEX) continue;
				if (!dr.mesh->in_layer(0, dr.model->alpha)) continue;
				SDL_DrawGPUPrimitives(rp, uint32_t(rg.count), 1, uint32_t(rg.first), 0);
				cast++;
			}
			if (debug_now) fprintf(stderr, "shadow pass: %d draws of %zu\n", cast, ranges.size());
			SDL_EndGPURenderPass(rp);
		}
	}
	// the picture's own numbers, then whether this port's shadows are on and the matrix that reads their map
	float frame_u[28] = { float(prep.game_x()), prep.game_xscale(), prep.game_yscale(),
	                      (shadows && light_ok && smap) ? (getenv("VF3_SHADOW_DEBUG") ? 1.0f + float(atoi(getenv("VF3_SHADOW_DEBUG"))) : 1.0f) : 0.0f };
	memcpy(frame_u + 4, light_m, sizeof(light_m));
	// VF3_SHADOW_DARK: how much of the ambient the shadow takes as well, on top of the sun it already
	// takes whole. 0 (the default) is the game's own light left alone.
	frame_u[20] = getenv("VF3_SHADOW_DARK") ? float(atof(getenv("VF3_SHADOW_DARK"))) : 0.0f;
	// VF3_SHADOW_NONE: the game's own shadows left out and ours not applied either -- the lit ground, to
	// measure what a shadow actually takes away
	if (getenv("VF3_SHADOW_NONE")) frame_u[3] = 0;
	// the floor measure_scales found, which is also where the shadow stops: nothing under it takes one
	memcpy(frame_u + 24, floor_p, sizeof(floor_p));
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
			// the shadow map second; with the shadows off it is the depth again, since a sampler must have a texture
			SDL_GPUTextureSamplerBinding sb[2] = { { od, sampler }, { (shadows && smap) ? smap : od, sampler } };
			int last_vp = -1;
			SDL_GPUGraphicsPipeline *bound = nullptr;
			for (size_t r = batch_first[b]; r < batch_end[b]; r++) {
				const Range &rg = ranges[r];
				const Raster::Draw &d = prep.draws[rg.draw];
				// With this port's shadows on, the game's own are left out here rather than in the raster:
				// they still mark the floor for measure_scales, they are simply not drawn.
				if (shadows && light_ok && d.mesh->colour_index == video::SHADOW_COLOUR_INDEX) continue;
				SDL_GPUGraphicsPipeline *p = d.mesh->layered ? layered : plain;
				if (p != bound) {
					SDL_BindGPUGraphicsPipeline(rp, p);
					SDL_BindGPUFragmentSamplers(rp, 0, sb, 2);
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
	if (smap && smap_ds) return true;
	SDL_GPUTextureCreateInfo ci = {};
	ci.type = SDL_GPU_TEXTURETYPE_2D;
	ci.width = ci.height = uint32_t(SMAP);
	ci.layer_count_or_depth = 1;
	ci.num_levels = 1;
	ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
	ci.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
	ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	smap = SDL_CreateGPUTexture(dev, &ci);
	ci.format = ds_format;
	ci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	smap_ds = SDL_CreateGPUTexture(dev, &ci);
	if (!smap || !smap_ds) { err = std::string("the shadow map: ") + SDL_GetError(); return false; }
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
void R3dGpu::measure_scales(const video::Scene &scene, const video::Raster &prep, const float d[3])
{
	for (float &s : vp_scale) s = 0;
	floor_p[0] = floor_p[1] = floor_p[2] = floor_p[3] = 0;
	auto vertex = [](const video::Raster::Tri &t, int k, float v[3]) {
		const float wv = 1.0f / t.iw[k];
		for (int q = 0; q < 3; q++) v[q] = t.a[k][q] * wv;
	};
	// Every face once: which viewport drew it, which way it faces, how big it is and where its middle lies.
	// One sweep, because the scene carries some twenty thousand triangles and a frame has 17 ms in it.
	flats.clear();
	float fn[3] = { 0, 0, 0 };
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
		if (dr.mesh->colour_index == video::SHADOW_COLOUR_INDEX) {
			const float s = f.n[0] * d[0] + f.n[1] * d[1] + f.n[2] * d[2] < 0 ? -f.area : f.area;
			for (int q = 0; q < 3; q++) fn[q] += f.n[q] * s;
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
	if (!unit(fn)) return;
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
		if (feet > 1e29f) return;
		float nearest = 1e30f;
		ref_vp = 64;
		for (int iv = 0; iv < 64; iv++) {
			if (plane[iv] == 0 || caster_vp[iv]) continue;
			const float rel = std::fabs(std::fabs(plane[iv]) - std::fabs(feet)) / std::max(1e-6f, std::fabs(feet));
			if (rel < nearest) { nearest = rel; ref_vp = size_t(iv); }
		}
		if (!(nearest < 0.3f) || ref_vp >= 64) return;
		ref = plane[ref_vp];
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
		if (!unit(nf)) return;
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

// Where the light is, and what the map has to cover. Both are read from the scene rather than invented: the
// direction is the stage's sun -- the one the game projects its own shadows along, measured to five decimals
// (docs/fv2-ombres.md) -- and the ground to cover is where those shadows fall, which the game has already
// worked out for us. The fighters' own viewports carry a different sun (the light seen from each fighter),
// so they are not the ones to ask.
void R3dGpu::fit_light(const video::Scene &scene, const video::Raster &prep)
{
	light_ok = false;
	for (bool &b : caster_vp) b = false;
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	int n = 0;
	char say[1024];
	int say_len = 0;
	for (size_t iv = 0; iv < scene.viewports.size() && iv < 64; iv++) {
		const video::Viewport &vp = scene.viewports[iv];
		int here = 0;
		for (const video::Model &m : vp.models) {
			bool sh = false;
			for (const video::Mesh &me : *m.meshes)
				if (me.colour_index == video::SHADOW_COLOUR_INDEX) { sh = true; break; }
			if (!sh) continue;
			here++;
			const float *M = m.matrix.m;
			const float p[3] = { M[12], M[13], M[14] };
			for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
			n++;
		}
		if (here) caster_vp[iv] = true;
		if (debug_now && !vp.models.empty() && say_len < 900)
			say_len += snprintf(say + say_len, sizeof(say) - size_t(say_len), " vp%zu[%zu models, %d shadow]",
			                    iv, vp.models.size(), here);
	}
	if (debug_now) fprintf(stderr, "shadows per viewport:%s\n", say_len ? say : " none");
	if (!n) return;
	// The stage's light, taken by agreement rather than by picking a viewport: each fighter's own viewport
	// carries the light as THAT fighter sees it (ROB_LIGHT is a point at distance 30, not a direction), while
	// the rest of the scene shares the one the game projects its own shadows along -- measured, fourteen
	// viewports of sixteen agree on it at a fight frame (docs/fv2-ombres.md).
	int agreed = 0;
	float d[3] = { 0, 0, 0 };
	for (const video::Viewport &a : scene.viewports) {
		int same = 0;
		for (const video::Viewport &b : scene.viewports) {
			float dd = 0;
			for (int k = 0; k < 3; k++) dd += (a.sun[k] - b.sun[k]) * (a.sun[k] - b.sun[k]);
			if (dd < 1e-8f) same++;
		}
		if (same > agreed) { agreed = same; d[0] = a.sun[0]; d[1] = a.sun[1]; d[2] = a.sun[2]; }
	}
	if (agreed < 2) return;
	const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
	if (!(dl > 1e-6f)) return;
	for (int k = 0; k < 3; k++) d[k] /= dl;
	measure_scales(scene, prep, d);
	// The box, read off the casters' own VERTICES. A model's place is one point and says nothing of its size,
	// and a box fitted to the shadows' places alone left the fighters outside it -- measured, the map came out
	// empty for 143 draws. prep.tris already carries every vertex in the camera's space, the space this matrix
	// works in, so the box is simply read off them, then taken down to the floor and widened the way the ray
	// runs so that what RECEIVES a shadow is covered too.
	const float ground = std::max(lo[1], hi[1]);
	for (int k = 0; k < 3; k++) { lo[k] = 1e30f; hi[k] = -1e30f; }
	int nv = 0;
	for (const video::Raster::Tri &t : prep.tris) {
		const video::Raster::Draw &dr = prep.draws[t.draw];
		const size_t ivv = size_t(dr.vp - scene.viewports.data());
		if (ivv >= 64 || !caster_vp[ivv]) continue;
		if (dr.mesh->colour_index == video::SHADOW_COLOUR_INDEX) continue;
		for (int k = 0; k < t.n; k++) {
			// a vertex keeps its attributes divided by w (that is how they interpolate): w back on, and it is
			// the position in the camera's space again -- the space this matrix works in
			const float wv = 1.0f / t.iw[k];
			for (int q = 0; q < 3; q++) {
				const float v = t.a[k][q] * wv;
				lo[q] = std::min(lo[q], v);
				hi[q] = std::max(hi[q], v);
			}
			nv++;
		}
	}
	if (nv < 3) return;
	const float PAD = 0.3f;
	lo[0] -= PAD; hi[0] += PAD;
	lo[2] -= PAD; hi[2] += PAD;
	lo[1] = std::min(lo[1], ground) - 0.2f;
	hi[1] += 0.2f;
	const float tall = hi[1] - lo[1];
	const float ox = d[1] > 0.1f ? tall * d[0] / d[1] : 0.0f;
	const float oz = d[1] > 0.1f ? tall * d[2] / d[1] : 0.0f;
	lo[0] = std::min(lo[0], lo[0] - ox); hi[0] = std::max(hi[0], hi[0] - ox);
	lo[2] = std::min(lo[2], lo[2] - oz); hi[2] = std::max(hi[2], hi[2] - oz);
	// the light looks along -d, with a frame around it
	const float z[3] = { -d[0], -d[1], -d[2] };
	float up[3] = { 0, 1, 0 };
	if (std::fabs(z[1]) > 0.99f) { up[1] = 0; up[2] = 1; }
	float x[3] = { up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0] };
	const float xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
	if (!(xl > 1e-6f)) return;
	for (int k = 0; k < 3; k++) x[k] /= xl;
	const float y[3] = { z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0] };
	const float c[3] = { (lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f };
	float ex[2] = { 1e30f, -1e30f }, ey[2] = { 1e30f, -1e30f }, ez[2] = { 1e30f, -1e30f };
	for (int i = 0; i < 8; i++) {
		const float p[3] = { (i & 1) ? hi[0] : lo[0], (i & 2) ? hi[1] : lo[1], (i & 4) ? hi[2] : lo[2] };
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
	auto row = [&](int r, const float a[3], float sc, float off) {
		light_m[r * 4 + 0] = a[0] * sc; light_m[r * 4 + 1] = a[1] * sc;
		light_m[r * 4 + 2] = a[2] * sc; light_m[r * 4 + 3] = off;
	};
	row(0, x, 1.0f / hx, (-dxc - cx) / hx);
	row(1, y, 1.0f / hy, (-dyc - cy) / hy);
	row(2, z, 1.0f / dz, (-dzc - z0) / dz);
	light_m[12] = light_m[13] = light_m[14] = 0;
	light_m[15] = 1;
	light_ok = true;
	if (debug_now)
		fprintf(stderr, "light: sun [%.4f %.4f %.4f] (%d viewports of %zu agree), %d shadow models, %d caster "
		        "vertices, box x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f, half %.2f %.2f, depth %.2f\n",
		        d[0], d[1], d[2], agreed, scene.viewports.size(), n, nv,
		        lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], hx, hy, dz);
}

bool R3dGpu::read_shadow_map(std::vector<float> &px, int &side)
{
	side = SMAP;
	if (!smap) return false;
	SDL_GPUTransferBufferCreateInfo ti = {};
	ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
	ti.size = uint32_t(SMAP) * uint32_t(SMAP) * 4;
	SDL_GPUTransferBuffer *dl = SDL_CreateGPUTransferBuffer(dev, &ti);
	if (!dl) return false;
	SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
	if (!cmd) { SDL_ReleaseGPUTransferBuffer(dev, dl); return false; }
	SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
	SDL_GPUTextureRegion src = {};
	src.texture = smap;
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

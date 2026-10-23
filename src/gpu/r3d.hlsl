// The Real3D's pictures on the GPU (SDL_GPU): the same layers, tests and shading as the CPU's rasteriser
// (src/video/r3d_raster.cpp), whose comments say what each part does. Compiled by tools/gpu/shaders.sh with
// DXC into SPIR-V and DXIL, and from the SPIR-V into MSL with SPIRV-Cross, into src/gpu/r3d_shaders.h.
//
// Entry points (one per -D):
//   VS_POLY   the polygons: their clip positions and attributes worked out by the CPU (r3d_gpu.cpp)
//   FS_POLY   their pixels: FS_LAYER 0 the opaque layer (base, the two translucent layers cleared, its depth as
//             a colour too), 1 or 2 a translucent layer (only in front of the opaque layer's depth)
//   VS_QUAD   a rectangle in clip space (the uniforms')
//   FS_FOG    the fog layers: a colour, with the spotlight on fog's ellipse
//   FS_OUT    the three layers put together into one premultiplied picture

#ifdef __spirv__
#define BIND(n, s) [[vk::binding(n, s)]]
#define COMBINED [[vk::combinedImageSampler]]
#else
#define BIND(n, s)
#define COMBINED
#endif

struct Draw
{
	float4 colour;      // rgb (the translator map's x16 in), a: the polygon's translucency times the nodes' alpha
	uint flags;         // see F_ below
	int format, tx, ty;
	int tw, th, page, wrap;   // wrap: u in bits 0-1, v in 2-3
	float levels, shininess, spec_value, fog_intensity;
	uint vp;
	float flip;         // a flipbook's step before mixed in (0: none), its picture at flip_x, flip_y
	int flip_x, flip_y;
};

struct Vp
{
	float4 sun;         // xyz, w intensity
	float4 light;       // x ambient, y sun clamp (1/0), z the light intensity clamped at full (1/0: Step 1.0 only),
	                    // w how many of this viewport's units make one of the shadow casters' (0: not measured)
	float4 spot_ellipse;
	float4 spot_range;  // xy
	float4 spot_colour, spot_fog_colour;
	float4 fog;         // rgb colour, a density
	float4 fog2;        // x start, y attenuation, z ambient
};

static const uint F_TEXTURED = 1, F_ALPHA_TEST = 2, F_TEX_ALPHA = 4, F_LIGHTING = 8, F_FIXED = 16, F_SMOOTH = 32,
	F_SPECULAR = 64, F_INVERTED = 128;

static const int MIP_X[12] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
static const int MIP_Y[11] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };

#if defined(VS_POLY) || defined(FS_POLY)
struct PolyV
{
	float4 pos : SV_Position;
	float3 view : TEXCOORD0;
	float3 normal : TEXCOORD1;
	float4 misc : TEXCOORD2;    // u, v, fixed shade, back-face test
	float lodbase : TEXCOORD3;
	nointerpolation uint draw : TEXCOORD4;
};
#endif

#ifdef VS_POLY
struct VsIn
{
	float4 clip : TEXCOORD0;
	float3 view : TEXCOORD1;
	float3 normal : TEXCOORD2;
	float4 misc : TEXCOORD3;
	float lodbase : TEXCOORD4;
	uint draw : TEXCOORD5;
};
PolyV main(VsIn i)
{
	PolyV o;
	o.pos = i.clip;
	o.view = i.view;
	o.normal = i.normal;
	o.misc = i.misc;
	o.lodbase = i.lodbase;
	o.draw = i.draw;
	return o;
}
#endif

#ifdef FS_POLY
COMBINED BIND(0, 2) Texture2D<float4> odepth : register(t0, space2);
COMBINED BIND(0, 2) SamplerState odepth_s : register(s0, space2);
// Fighting Vipers 2's shadow map (VS_SHADOW above): the fighters' distance to the light. When shadow_on is 0
// it is odepth bound a second time and nothing reads it -- a sampler must always have a texture.
COMBINED BIND(1, 2) Texture2D<float4> smap : register(t1, space2);
COMBINED BIND(1, 2) SamplerState smap_s : register(s1, space2);
BIND(2, 2) ByteAddressBuffer texram : register(t2, space2);
// (raw buffers, read by byte offset: SDL_GPU's Direct3D 12 binds every storage buffer as a raw view, and a raw view
// in a structured buffer's slot is undefined there: some cards, an AMD Radeon 780M and a GTX 970, drew nothing)
BIND(3, 2) ByteAddressBuffer draws : register(t3, space2);
BIND(4, 2) ByteAddressBuffer vps : register(t4, space2);
BIND(0, 3) cbuffer Frame : register(b0, space3)
{
	float gx, xscale, yscale, shadow_on;
	float4 srow0, srow1, srow2, srow3;   // view space -> the shadow map's clip space, by rows
	float4 shadow_k;                     // x: how much of the ambient the shadow takes as well (0: none)
	float4 floorp;                       // the floor: xyz its direction, w its offset, in the casters' units
};

float4 decode(int type, uint v)
{
	float4 c = 0;
	if (type == 0) {
		c = float4(((v >> 10) & 31) / 31.0, ((v >> 5) & 31) / 31.0, (v & 31) / 31.0, 1.0 - float((v >> 15) & 1));
	} else if (type == 1) { float g = (v & 15) / 15.0; c = float4(g, g, g, ((v >> 4) & 15) / 15.0); }
	else if (type == 2) { float g = ((v >> 4) & 15) / 15.0; c = float4(g, g, g, (v & 15) / 15.0); }
	else if (type == 3) { float g = ((v >> 8) & 15) / 15.0; c = float4(g, g, g, ((v >> 12) & 15) / 15.0); }
	else if (type == 4) { float g = ((v >> 12) & 15) / 15.0; c = float4(g, g, g, ((v >> 8) & 15) / 15.0); }
	else if (type == 5) { uint b = v & 0xFF; float g = b / 255.0; c = float4(g, g, g, b == 255 ? 0.0 : 1.0); }
	else if (type == 6) { uint b = (v >> 8) & 0xFF; float g = b / 255.0; c = float4(g, g, g, b == 255 ? 0.0 : 1.0); }
	else if (type == 7) { c = float4(((v >> 12) & 15) / 15.0, ((v >> 8) & 15) / 15.0, ((v >> 4) & 15) / 15.0, (v & 15) / 15.0); }
	else if (type >= 8 && type <= 11) { uint n = (v >> ((type - 8) * 4)) & 15; float g = n / 15.0; c = float4(g, g, g, n == 15 ? 0.0 : 1.0); }
	return c;
}

float fract_(float x) { return x - floor(x); }

float tex_locations(int wrap, float size, float u, out float u0, out float u1)
{
	const float texel = 1.0 / size, half_ = 0.5 / size;
	if (wrap == 0) {
		u = u * size - 0.5;
		u0 = fract_((floor(u) + 0.5) / size);
		u1 = fract_(u0 + texel);
		return fract_(u);
	}
	if (wrap == 1) {
		u = fract_(u) * size - 0.5;
	} else {
		float m = u - 2.0 * floor(u / 2.0);
		float odd = floor(m);
		u = odd > 0 ? 1.0 - fract_(u) : fract_(u);
		u = u * size - 0.5;
	}
	u0 = (floor(u) + 0.5) / size;
	u1 = u0 + texel;
	if (u0 < 0) u0 = 0;
	if (u1 >= 1) u1 = 1 - half_;
	return fract_(u);
}

float4 fetch(Draw d, int page, int level, int x, int y)
{
	x &= int(2047u >> uint(level));
	y &= int(1023u >> uint(level));
	uint at = uint(page * 1024 + MIP_Y[level] + y) * 2048u + uint(MIP_X[level] + x);
	uint w = texram.Load((at >> 1) * 4);
	return decode(d.format, (w >> ((at & 1) * 16)) & 0xFFFF);
}

float4 bilinear(Draw d, int page, int level, float u, float v)
{
	const float sw = float(d.tw >> level), sh = float(d.th >> level);
	const int px = d.tx >> level, py = d.ty >> level;
	float u0, u1, v0, v1;
	float a = tex_locations(d.wrap & 3, sw, u, u0, u1);
	float b = tex_locations((d.wrap >> 2) & 3, sh, v, v0, v1);
	int x0 = int(u0 * sw + px), x1 = int(u1 * sw + px), y0 = int(v0 * sh + py), y1 = int(v1 * sh + py);
	float4 p00 = fetch(d, page, level, x0, y0), p10 = fetch(d, page, level, x1, y0);
	float4 p01 = fetch(d, page, level, x0, y1), p11 = fetch(d, page, level, x1, y1);
	if (d.flags & F_ALPHA_TEST) {
		if (p00.a > p10.a) p10.rgb = p00.rgb;
		if (p00.a > p01.a) p01.rgb = p00.rgb;
		if (p10.a > p00.a) p00.rgb = p10.rgb;
		if (p10.a > p11.a) p11.rgb = p10.rgb;
		if (p01.a > p00.a) p00.rgb = p01.rgb;
		if (p01.a > p11.a) p11.rgb = p01.rgb;
		if (p11.a > p01.a) p01.rgb = p11.rgb;
		if (p11.a > p10.a) p10.rgb = p11.rgb;
	}
	return lerp(lerp(p00, p10, a), lerp(p01, p11, a), b);
}

struct FsOut
{
#if FS_LAYER == 0
	float4 base : SV_Target0;
	float4 t1 : SV_Target1;
	float4 t2 : SV_Target2;
	float4 depth : SV_Target3;
#else
	float4 t : SV_Target0;
#endif
};

FsOut main(PolyV i)
{
	FsOut o;
	if (i.misc.w > 0)
		discard;   // facing away
	const float wi = 1.0 / i.pos.w;   // (SV_Position.w is w: the reciprocal of GLSL's gl_FragCoord.w)
#if FS_LAYER != 0
	// translucent: only in front of the opaque layer
	if (wi <= odepth.Load(int3(int2(i.pos.xy), 0)).r)
		discard;
#endif
	const Draw d = draws.Load<Draw>(i.draw * sizeof(Draw));
	const Vp vp = vps.Load<Vp>(d.vp * sizeof(Vp));
	float4 t = 1;
	if (d.flags & F_TEXTURED) {
		const int page = d.page;
		float lod = -log2(wi * wi * i.lodbase);
		float fl = clamp(lod, 0.0, d.levels);
		if (!(fl == fl)) fl = 0;
		int il = int(fl);
		t = bilinear(d, page, il, i.misc.x, i.misc.y);
		float fr = fl - il;
		if (fr > 0) t = lerp(t, bilinear(d, page, il + 1, i.misc.x, i.misc.y), fr);
		if (d.flip > 0) {   // a flipbook's step faded in: the step before's picture mixed in
			Draw o = d;
			o.tx = d.flip_x;
			o.ty = d.flip_y;
			float4 f = bilinear(o, page, il, i.misc.x, i.misc.y);
			if (fr > 0) f = lerp(f, bilinear(o, page, il + 1, i.misc.x, i.misc.y), fr);
			t = lerp(t, f, d.flip);
		}
		if (d.flags & F_INVERTED) t.rgb = 1 - t.rgb;
		if ((d.flags & F_ALPHA_TEST) && t.a < 32.0 / 255.0)
			discard;
		if (d.flags & F_TEX_ALPHA) {
#if FS_LAYER == 0
			if (t.a < 1.0) discard;
#else
			if (t.a * d.colour.a >= 1.0) discard;
#endif
		} else if (!(d.flags & F_ALPHA_TEST)) {
			t.a = 1;
		}
	}
	float4 c = t * d.colour;
	if (c.a < 1.0 / 32.0)
		discard;
	// spotlight
	float lobe = 0, lobe_fog = 0;
	{
		float gxp = (i.pos.x - gx) / xscale, gyp = i.pos.y / yscale;
		float ex = (gxp - vp.spot_ellipse.x) / vp.spot_ellipse.z, ey = (gyp - vp.spot_ellipse.y) / vp.spot_ellipse.w;
		float ell = max(0.0, 1.0 - (ex * ex + ey * ey));
		const float sz = i.view.z * (1.0 / 2048.0);   // (as the CPU's: Step 1.0's spotlight distances / 2048)
		float enable = -sz >= vp.spot_range.x ? 1.0 : 0.0;
		float range = 0;
		if (vp.spot_range.y != 0) {
			float ext = abs(vp.spot_range.y);
			float dd = min(vp.spot_range.x + ext + sz, 0.0);
			float q = dd / (1.0 + ext) - 1.0;
			range = enable / (q * q);
		}
		lobe = range * ell;
		lobe_fog = enable * ell;
	}
	// Fighting Vipers 2's shadow: 0 where the light does not reach this pixel. It multiplies the SUN, not the
	// colour, so a pixel in shadow keeps the viewport's ambient and its texture stays visible -- which the game's
	// own shadow does not do: being opaque, it paints the ground over (docs/fv2-ombres.md).
	float shade = 1;
	float3 shade_dbg = 0, look_dbg = 0;
	if (shadow_on > 0 && vp.light.w > 0) {
		// the map is in the casters' units; this viewport's may be others (measure_scales in r3d_gpu.cpp)
		const float4 wv = float4(i.view / vp.light.w, 1);
		// Nothing below the floor takes a shadow. The game's own shadow is a quad IN the floor, so it never
		// falls through it; ours is cast from a map and would, and on a stage whose floor is a grating it
		// landed on the structure a metre under the openings -- measured, 1102 pixels of 13415.
		const bool under = dot(floorp.xyz, wv.xyz) < floorp.w - 0.05;
		const float4 lp = float4(dot(srow0, wv), dot(srow1, wv), dot(srow2, wv), dot(srow3, wv));
		const float2 suv = float2(lp.x * 0.5 + 0.5, 0.5 - lp.y * 0.5);
		const bool inside = lp.z >= 0.0 && lp.z <= 1.0 && all(suv >= 0.0) && all(suv <= 1.0);
		const float near_light = smap.SampleLevel(smap_s, saturate(suv), 0).r;
		if (inside && !under && lp.z - 0.002 > near_light) shade = 0.0;
		// shadow_on 2 (VF3_SHADOW_DEBUG): what the lookup sees, painted instead of the picture --
		// red and green the place read in the map, blue what was found there, black outside it
		// rouge : le pixel est juge a l ombre ; vert : ce que la carte rend ; bleu : la lecture est dans la boite
		shade_dbg = float3(1.0 - shade, near_light, inside ? 1.0 : 0.0);
		// shadow_on 3 (VF3_SHADOW_DEBUG=2): where this pixel reads the map, and how far it is along the
		// light -- rouge et vert : l endroit lu dans la carte ; bleu : la distance a la lumiere
		look_dbg = float3(saturate(suv), saturate(lp.z));
	}
	if (d.flags & F_LIGHTING) {
		float sun;
		if (d.flags & F_FIXED) sun = i.misc.z;
		else sun = dot(vp.sun.xyz, i.normal);
		sun = clamp(sun, -1.0, 1.0);
		if (vp.light.y > 0 || c.a < 0.999) sun = max(sun, 0.0);
		// the sun and ambient light, up to full on Step 1.0 and past it on the later steppings, as the CPU
		// rasterizer has it (r3d_raster.cpp)
		// The shadow multiplies the SUN. A pixel in shadow therefore keeps the viewport's ambient whole --
		// measured 0.6 on this stage -- which is why it stays light while the game's own opaque shadow
		// paints the ground over. shadow_k.x takes some of that ambient as well: 0 is the game's own
		// light untouched, 1 leaves a pixel in shadow with nothing at all.
		float li = sun * vp.sun.w * shade + vp.light.x * (1.0 - shadow_k.x * (1.0 - shade));
		if (vp.light.z > 0) li = min(li, 1.0);
		float3 l3 = li + vp.spot_colour.rgb * lobe;
		c.rgb *= l3;
		if (d.flags & F_SPECULAR) {
			float s;
			const float exp_index[4] = { 8, 16, 32, 64 }, mult_index[4] = { 1.6, 1.6, 2.4, 3.2 };
			int shin = clamp(int(d.shininess), 0, 3);
			if (d.flags & F_SMOOTH) {
				s = pow(max(0.0, sun), exp_index[shin]) * mult_index[shin];
			} else {
				float nl = dot(vp.sun.xyz, i.normal);
				float rz = -vp.sun.z + 2.0 * nl * i.normal.z;
				s = max(0.0, rz);
			}
			s *= d.spec_value * vp.sun.w * shade;
			if (d.colour.a < 1.0) c.a = max(c.a, s);
			c.rgb += s;
		}
	}
	c.rgb = min(c.rgb, 1.0);
	float fog = d.fog_intensity * clamp(vp.fog2.x + -i.view.z * vp.fog.a, 0.0, 1.0);
	if (fog > 0) {
		float3 target = vp.fog.rgb * vp.fog2.z + vp.spot_fog_colour.rgb * vp.fog2.y * vp.fog.rgb * lobe_fog;
		c.rgb += (target - c.rgb) * fog;
	}
	// shadow_on 7 (VF3_SHADOW_DEBUG=6): the place AFTER the layer's units are taken out, so every layer
	// reads on the same ruler: view / scale * 0.05 + 0.5
	if (shadow_on > 6.5) c.rgb = i.view / max(vp.light.w, 1e-6) * 0.05 + 0.5;
	// shadow_on 6 (VF3_SHADOW_DEBUG=5): the same, far coarser, for the viewports drawn at a big scale
	else if (shadow_on > 5.5) c.rgb = i.view * 0.0005 + 0.5;
	// shadow_on 5 (VF3_SHADOW_DEBUG=4): the pixel's place in its viewport's space, as view * 0.05 + 0.5
	else if (shadow_on > 4.5) c.rgb = i.view * 0.05 + 0.5;
	// shadow_on 4 (VF3_SHADOW_DEBUG=3): which viewport drew this pixel, as vp/16 in red
	else if (shadow_on > 3.5) c.rgb = float3(float(d.vp) / 16.0, 0, 0);
	else if (shadow_on > 2.5) c.rgb = look_dbg;
	else if (shadow_on > 1.5) c.rgb = shade_dbg;
#if FS_LAYER == 0
	o.base = float4(c.rgb, 1);
	o.t1 = 0;
	o.t2 = 0;
	o.depth = float4(wi, 0, 0, 0);
#else
	o.t = float4(c.rgb, min(c.a, 1.0));
#endif
	return o;
}
#endif

// The shadow map (Fighting Vipers 2, docs/fv2-ombres.md): the fighters drawn once more, seen from the light the
// game lights them with, their distance to it kept. Nothing else is written. It works on the SAME vertex buffer
// as the picture, because a vertex already carries its position in the camera's space (VsIn's view) and every
// viewport of a scene shares that camera -- so one matrix, view space to the light's clip space, is enough.
#if defined(VS_SHADOW) || defined(FS_SHADOW)
struct ShadowV
{
	float4 pos : SV_Position;
	float dist : TEXCOORD0;
};
#endif

#ifdef VS_SHADOW
BIND(0, 1) cbuffer Light : register(b0, space1)
{
	float4 lrow0, lrow1, lrow2, lrow3;   // view space -> the light's clip space, by rows
};
struct ShadowIn
{
	float4 clip : TEXCOORD0;
	float3 view : TEXCOORD1;
	float3 normal : TEXCOORD2;
	float4 misc : TEXCOORD3;
	float lodbase : TEXCOORD4;
	uint draw : TEXCOORD5;
};
ShadowV main(ShadowIn i)
{
	ShadowV o;
	// view is already the position in the camera's space: the CPU keeps its attributes divided by w and puts
	// w back on when it fills the vertex buffer (r3d_gpu.cpp), so nothing has to be undone here.
	const float4 v = float4(i.view, 1);
	o.pos = float4(dot(lrow0, v), dot(lrow1, v), dot(lrow2, v), dot(lrow3, v));
	o.dist = o.pos.z;
	return o;
}
#endif

#ifdef FS_SHADOW
// The distance kept as a colour, as the opaque layer keeps its own depth: a target that can be read back and
// looked at, which a depth buffer cannot.
float4 main(ShadowV i) : SV_Target0
{
	return float4(i.dist, 0, 0, 1);
}
#endif

#ifdef VS_QUAD
BIND(0, 1) cbuffer Quad : register(b0, space1)
{
	float4 rect;   // x0, y0, x1, y1 in clip space
};
float4 main(uint id : SV_VertexID) : SV_Position
{
	float2 p = float2((id & 1) ? rect.z : rect.x, (id & 2) ? rect.w : rect.y);
	return float4(p, 0, 1);
}
#endif

#ifdef FS_FOG
BIND(0, 3) cbuffer Fog : register(b0, space3)
{
	float4 colour;      // rgb (times its ambient), a the amount
	float4 spot;        // rgb: the spotlight on fog's colour (times its attenuation and the fog colour); a: on
	float4 ellipse;     // as Vp's
	float4 map;         // gx, xscale, yscale
};
struct FogOut
{
	float4 base : SV_Target0;
	float4 t1 : SV_Target1;
	float4 t2 : SV_Target2;
};
FogOut main(float4 pos : SV_Position)
{
	FogOut o;
	float3 c = colour.rgb;
	if (spot.a > 0) {
		float px = (pos.x - map.x) / map.y, py = pos.y / map.z;
		float ex = (px - ellipse.x) / ellipse.z, ey = (py - ellipse.y) / ellipse.w;
		c += spot.rgb * max(0.0, 1.0 - (ex * ex + ey * ey));
	}
	o.base = float4(c, colour.a);
	o.t1 = 0;
	o.t2 = 0;
	return o;
}
#endif

#ifdef FS_OUT
COMBINED BIND(0, 2) Texture2D<float4> base : register(t0, space2);
COMBINED BIND(0, 2) SamplerState base_s : register(s0, space2);
COMBINED BIND(1, 2) Texture2D<float4> t1 : register(t1, space2);
COMBINED BIND(1, 2) SamplerState t1_s : register(s1, space2);
COMBINED BIND(2, 2) Texture2D<float4> t2 : register(t2, space2);
COMBINED BIND(2, 2) SamplerState t2_s : register(s2, space2);
BIND(0, 3) cbuffer Out : register(b0, space3)
{
	int4 ss;   // x: antialiasing, the layers' pixels each of these is the average of, ss.x by ss.x
};
float4 one(int2 xy)
{
	int3 at = int3(xy, 0);
	float4 p = base.Load(at), q = t1.Load(at), s = t2.Load(at);
	float3 c = p.rgb * p.a;
	float al = p.a;
	float4 tr;
	if (q.a * s.a > 0) {
		float k = (s.a + (1.0 - q.a)) / 2.0;
		tr = float4(lerp(q.rgb, s.rgb, k), 1.0);
	} else {
		tr = q.a > 0 ? q : s;
	}
	if (tr.a > 0) {
		c = tr.rgb * tr.a + c * (1 - tr.a);
		al = tr.a + al * (1 - tr.a);
	}
	return float4(c, al);
}
float4 main(float4 pos : SV_Position) : SV_Target0
{
	const int k = max(ss.x, 1);
	const int2 at = int2(pos.xy) * k;
	float4 sum = 0;
	for (int j = 0; j < k; j++)
		for (int i = 0; i < k; i++) sum += one(at + int2(i, j));
	return sum / float(k * k);
}
#endif

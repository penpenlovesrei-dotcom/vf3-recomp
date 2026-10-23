// The Real3D's pictures on the GPU, with SDL_GPU (Vulkan, Direct3D 12, Metal): the CPU's polygons as prepared for
// its own rasteriser (video::Raster::prepare: transformed, clipped by the near plane, in the passes' order), drawn
// by the GPU's rasteriser with the CPU's shading (src/gpu/r3d.hlsl) in the same layers and passes, into a texture
// of the host's renderer: the picture is put together with the 2D there, on the GPU, and never read back (but for
// the start-up check against the CPU's picture).
#pragma once
#include "../video/r3d_raster.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

#include "../machine/model3.h"

class R3dGpu
{
public:
	~R3dGpu();
	// Lets go of everything on the device (before the renderer that owns it goes)
	void shutdown();
	// On the host renderer's device (SDL_GetGPURendererDevice); false if its shaders can't be made here
	bool init(SDL_GPUDevice *device);
	const std::string &error() const { return err; }
	const char *driver() const;

	// The output texture (made here, RGBA8, premultiplied), for the renderer to show: the picture drawn w x h, shown
	// w / ss x h / ss (antialiasing: each pixel the average of ss x ss drawn)
	SDL_GPUTexture *output(int w, int h, int ss = 1);
	// Draws a picture: the scene as `prep` prepared it (prep.prepare(scene) done, at the output's size), the
	// texture RAM from the board (its changes since the last picture: board.tex_uploads, taken here).
	// Submitted to the GPU; the renderer's own commands after it see the picture.
	bool render(const video::Scene &scene, const video::Raster &prep, m3::Model3 &board);
	// (the texture RAM and its areas written given apart from the board: a copy of them, the game running alongside)
	bool render(const video::Scene &scene, const video::Raster &prep, const std::vector<uint16_t> &tex_ram, std::vector<m3::Model3::TexUpload> &tex_uploads);
	// The output read back (tests and the start-up check only): RGBA8
	bool read_output(std::vector<uint32_t> &px);

	double last_cpu_ms = 0;   // the CPU's share of the last picture (setting it up)

	// Fighting Vipers 2's shadows, this port's own (docs/fv2-ombres.md). False: nothing of it runs and the
	// picture is the one the checks compare. True: the fighters are drawn once more from the light the game
	// lights them with, their distance to it kept in a map -- which is all this step does; what reads the map
	// comes after.
	bool shadows = false;
	// Say what the shadow is doing for THIS picture -- the frame VF3_SHADOW_MAP names -- and not for
	// whichever one came first. Every number read off the wrong frame costs an hour.
	bool debug_now = false;
	// The shadow map read back, to look at it (tests): side x side distances, 1 where nothing was drawn
	bool read_shadow_map(std::vector<float> &px, int &side, int which = 0);

private:
	SDL_GPUDevice *dev = nullptr;
	std::string err;
	SDL_GPUGraphicsPipeline *p_opq = nullptr, *p_opq_l = nullptr, *p_tr = nullptr, *p_tr_l = nullptr, *p_fog = nullptr, *p_out = nullptr;
	SDL_GPUSampler *sampler = nullptr;
	SDL_GPUTextureFormat ds_format = SDL_GPU_TEXTUREFORMAT_INVALID;
	int w = 0, h = 0, ss = 1;
	SDL_GPUTexture *base = nullptr, *t1 = nullptr, *t2 = nullptr, *odepth = nullptr, *ds = nullptr, *out = nullptr;
	// the shadow map: a fixed size, having nothing to do with the output's
	static constexpr int SMAP = 1024;
	// Two of them: at stages 3, 10 and 11 the game flattens each fighter along a light of its own, measured
	// from that fighter's position, so one map cannot hold both. Everywhere else the two agree and only the
	// first is made (fit_light).
	static constexpr int MAPS = 2;
	SDL_GPUTexture *smap[MAPS] = {}, *smap_ds = nullptr;
	SDL_GPUGraphicsPipeline *p_shadow = nullptr;
	float light_m[MAPS][16] = {};   // view space -> each light's clip space, by rows (fit_light)
	int nmaps = 0;
	// why the light could not be worked out, counted per reason: when it cannot, the game's own shadows
	// are drawn again, so any count here that is not the first is a flicker the player sees
	unsigned why[8] = {};
	int shadow_models = 0;      // how many the scene carries, whether or not the light was worked out
	// How many of their triangles actually reach the raster. A model can be in the scene and none of it
	// on screen; then the game draws no shadow for that picture either, and there is nothing to replace.
	int shadow_tris = 0;
	// The last light that WAS worked out. When a picture fails to give one while the scene still carries
	// the game's shadows, this one is used instead: a light one picture old cannot be seen, whereas
	// falling back on the game's own shadows for that picture is a flicker the player does see.
	struct Kept { float light_m[MAPS][16], floor_p[4], vp_scale[64]; int nmaps; bool caster_vp[64]; int vp_group[64]; };
	Kept kept = {};
	bool kept_ok = false;
	bool light_ok = false;
	bool caster_vp[64] = {};     // the viewports that carry the game's own shadows: their models cast
	int vp_group[64] = {};       // which map each caster goes into (-1: none)
	// How many of a viewport's units make one of the casters' (fit_light): the game draws its stage in
	// several layers, each with its own depth buffer AND its own units, and nothing in the scene says by how
	// much they differ -- it is measured off the one surface they share, the floor. 0: not measured, no shadow.
	float vp_scale[64] = {};
	// the floor measure_scales found, in the casters' units: xyz its direction, w its offset
	float floor_p[4] = {};
	// what measure_scales sweeps the scene into, kept from frame to frame so it allocates nothing
	struct Flat { int vp; float n[3], mid[3], area; };
	std::vector<Flat> flats;
	std::vector<std::pair<float, float>> band;
	SDL_GPUBuffer *vbuf = nullptr, *dbuf = nullptr, *pbuf = nullptr, *texram = nullptr;
	uint32_t vbuf_size = 0, dbuf_size = 0, pbuf_size = 0;
	SDL_GPUTransferBuffer *tb = nullptr;
	uint32_t tb_size = 0;
	bool texram_full = true;
	// The texture pack's pages, held in the same buffer after the board's two (src/video/tex3d_hd.h): how many
	// the buffer was made for, and whether they have been uploaded (they never change while the game runs)
	int hd_pages = 0;
	bool hd_up = false;
	struct GV { float clip[4], view[3], normal[3], misc[4], lodbase; uint32_t draw; };
	std::vector<GV> verts;
	void free_targets();
	bool make_targets(int w, int h, int ss);
	bool ensure_buffer(SDL_GPUBuffer *&b, uint32_t &size, uint32_t need, SDL_GPUBufferUsageFlags usage);
	bool make_shadow_map();
	void measure_scales(const video::Scene &scene, const video::Raster &prep);
	// The light to draw the shadow map from, and the box it covers, worked out from the scene: the stage's
	// sun for the direction, and the game's own shadows for where on the ground they fall
	void fit_light(const video::Scene &scene, const video::Raster &prep);
};

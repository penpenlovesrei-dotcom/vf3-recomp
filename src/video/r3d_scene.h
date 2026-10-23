// The Real3D's scene, read from its memories each frame: the viewports (a linked list from culling RAM word
// 0x800000), and under each the models to draw with their matrices, made from the tree of culling nodes. Models
// are turned into meshes (vertices in model space plus the state their polygons share) once for the ROM's and
// every frame for those in polygon RAM. The CPU rasteriser (r3d_raster) and the GPU renderers draw from this.
//
// What is known of the formats is written where it is read (r3d_scene.cpp). Step 1.0 only (VF3).
#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace m3 { class Model3; }

namespace video {

// 4x4 matrices are column-major (m[col * 4 + row]), as GL's
struct Mat4
{
	float m[16];
	static Mat4 identity();
	Mat4 operator*(const Mat4 &b) const;
};

struct Vertex
{
	float pos[3];
	float normal[3];
	float uv[2];          // in texture sizes (1.0 = the texture's width or height)
	float fixed_shade;
	float face_normal[3]; // for the back-face test
	float texture_np;     // the texture LOD's base
	uint8_t colour[4];    // RGBA, alpha the polygon's translucency
};

struct Mesh
{
	// what the polygons share
	bool textured = false, alpha_test = false, texture_alpha = false, poly_alpha = false, lighting = false;
	bool fixed_shading = false, smooth_shading = false, high_priority = false, trans_select = false, layered = false;
	bool specular = false, translator_map = false, inverted = false, micro = false;
	float shininess = 0, specular_value = 0, fog_intensity = 0;
	int format = 0, x = 0, y = 0, width = 0, height = 0, page = 0;
	int wrap_u = 0, wrap_v = 0;   // 0 repeat, 1 repeat clamped, 2 mirror, 3 mirror clamped
	int micro_id = 0;
	float micro_min_lod = 0;
	// smooth motion's flipbooks (interpolate): the step before's picture at flip_x, flip_y, mixed in by flip (0: none)
	float flip = 0;
	int flip_x = 0, flip_y = 0;
	std::vector<Vertex> verts;    // triangles
	uint64_t key = 0;             // the polygon header bits that made it
	// Where its colour came from: an entry of the colour table the game keeps in polygon RAM, or -1 when the
	// polygon carried its own RGB. The game rewrites that table every frame, which is how Fighting Vipers 2
	// gives its shadows the stage's colour: theirs is entry 16 (docs/fv2-ombres.md). A mesh holds one colour,
	// so one entry; two entries that resolve to the same RGB share a mesh and the first one's index is kept.
	int colour_index = -1;
	// Which of the passes draws it: 0 the opaque layer, 1 and 2 the two translucent layers
	bool in_layer(int layer, float node_alpha) const
	{
		bool na = node_alpha < 1.0f;
		if (layer == 0) return !(poly_alpha || na);
		if (!texture_alpha && !poly_alpha && !na) return false;
		return layer == 1 ? !trans_select : trans_select;
	}
};

// Fighting Vipers 2's shadows: their polygons take their colour from this entry of the table, which the game
// rewrites every frame with the stage's shadow colour (docs/fv2-ombres.md). It is that game's own marker for
// them -- every other mesh of a fight frame carries its own RGB. Model::flattened says the same thing without
// asking the game, and is what the renderer goes by; this is kept because it is also where the colour of a
// shadow comes from, and because it is how the marker was first found.
constexpr int SHADOW_COLOUR_INDEX = 16;

using Meshes = std::shared_ptr<std::vector<Mesh>>;

struct Model
{
	Meshes meshes;
	uint32_t addr = 0;     // where it came from (for matching between frames)
	Mat4 matrix;           // model to view
	float alpha = 1, scale = 1;
	// A shadow is a model the game has flattened: its matrix carries a projection onto the floor, so the
	// linear part is of rank two and the determinant is exactly zero. That is what a shadow IS, in both
	// games, and it needs nothing a game puts there on purpose. Measured on a fight frame of Fighting
	// Vipers 2: the 37 meshes the colour table marks (SHADOW_COLOUR_INDEX) are exactly the 37 under a flat
	// matrix, no more and no fewer, while the other 479 are rigid. And on one of Virtua Fighter 3: ten
	// models flat out of 345, the rest at a determinant of exactly 1 -- where the colour index says nothing
	// at all, every mesh of that frame taking its colour from its vertices.
	bool flattened = false;
	int page = 0;          // the texture sheet flip from the nodes
	int tex_x = 0, tex_y = 0;   // the texture offset from the nodes (Step 1.5 and later; 0 on Step 1.0)
	uint32_t node_path = 0;   // a hash of the culling nodes above it (for matching between frames)
	// debugging (VF3_MESH_AUDIT): the last matrix index applied on the way down to this model, and the
	// viewport's matrix table, so that the matrix's own address in culling memory can be worked out
	uint32_t matrix_index = 0, matrix_table = 0;
};

struct Viewport
{
	int priority = 0;
	float vp_x = 0, vp_y = 0, vp_w = 496, vp_h = 384;   // in the 496x384 screen, from the top left
	float left = -1, right = 1, bottom = -1, top = 1;    // the frustum's tangents
	float cota = 0;
	float sun[3] = {}, sun_intensity = 0, ambient = 0;
	float spot_ellipse[4] = {};  // centre x, y (from the top), radius scale x, y; in 496x384 pixels
	float spot_range[2] = {};
	float spot_colour[3] = {}, spot_fog_colour[3] = {};
	float fog_colour[3] = {}, fog_density = 0, fog_start = 0, fog_attenuation = 0, fog_ambient = 0;
	float scroll_fog = 0, scroll_att = 0;
	bool full_screen = false;
	// Step 1.0 clamps the light that reaches a surface at full; Step 1.5 and later let a game drive it past
	// that, and some do. Set from the board's stepping (r3d_scene.cpp, build).
	bool intensity_clamp = true;
	Mat4 root = Mat4::identity();   // the matrix of the first node with one (the camera's, where the game puts it there)
	int root_matrix = -1;            // its index in the matrix table (-1: none)
	std::vector<Model> models;
};

struct Scene
{
	std::vector<Viewport> viewports;
	bool blank = false;      // block culling: the 3D shows white
	uint64_t id = 0;         // (each scene built its own: smooth motion pairs two frames' models once, not each picture)
};

// Smooth motion: the scene between two frames' (t 0: a, 1: b). Each of b's models is moved between where it was in
// a and where it is in b (its matrix blended); models are matched by viewport and address (several sharing one: the
// nearest), then by shape (an animation's next model: the nearest), then by place. A model that turned or moved
// too far in a frame is shown where it is in b; if most did (a cut), out is b and the result false.
struct InterpStats { size_t total = 0, no_key = 0, turned = 0, moved = 0, matched = 0, blended = 0, not_blended = 0; };
bool interpolate(const Scene &a, const Scene &b, float t, Scene &out, InterpStats *stats = nullptr);
// The camera's move between two frames' viewport (b = camera a for what stands still), from the models alone at their
// address in both; false if too few agree on one
bool camera_move(const Viewport &a, const Viewport &b, Mat4 &camera);

class SceneBuilder
{
public:
	// Reads the board's memories into scene (replacing what it held)
	void build(const m3::Model3 &board, Scene &scene);
	// The cache of ROM models' meshes (cleared when it grows too big, or on reset)
	void clear_cache() { rom_cache.clear(); dyn_cache.clear(); }
	size_t cached() const { return rom_cache.size(); }
	// Widescreen: how much wider than 4:3 the full-screen viewports are drawn (the picture's width over its 4:3
	// width); their culling's side planes are widened as much, or what's past the 4:3 sides would pop in and out
	float widen = 1;

private:
	const uint32_t *cull_lo = nullptr, *cull_hi = nullptr, *poly = nullptr, *vrom = nullptr;
	std::vector<uint32_t> chain;   // node(): sibling chains being walked
	uint32_t visits = 0;           // nodes visited this scene
	static constexpr uint32_t NODE_BUDGET = 200000;
	void node_one(uint32_t addr);
	const float *matrix_base = nullptr;
	const float *lod_table = nullptr;
	uint32_t colour_table = 0;
	// tex_x, tex_y: the texture offset a Step 1.5+ culling node can set for everything below it (Step 1.0 has no
	// such word and leaves them 0)
	struct Attribs { float alpha = 1, scale = 1; int page = 0, tex_x = 0, tex_y = 0; bool no_cull = false; };
	Attribs attr;
	// The culling nodes' layout and the fixed-point formats go with the board's stepping, and build() sets these
	// from it. Step 1.0: 8-word nodes, vertices 17.7, texture NP 12.12. Step 1.5 and later: 10-word nodes, with
	// two more words at the start (a model scale and a texture offset, which Step 1.0 has nowhere), vertices
	// 13.11 and texture NP 10.14. Everything after the first word is at an index two higher on those boards,
	// which is what node_shift is.
	int node_shift = 0;
	uint32_t mat_table = 0, last_matrix = 0;   // debugging (VF3_MESH_AUDIT)
	float vertex_scale = 1.0f / 128.0f;
	float np_scale = 1.0f / 4096.0f;
	int depth = 0;
	std::vector<Mat4> stack;
	Mat4 cur;
	uint32_t path = 0;
	Viewport *vp = nullptr;
	struct Planes { float lu, lv, tu, tw, ru, rv, bu, bw; } planes = {};
	std::unordered_map<uint32_t, Meshes> rom_cache;
	// Models parsed each frame (built in polygon RAM, or ROM's coloured from the colour table): the same while their
	// words (hash) and the colour table's entries they read are (as parsed then: the table's place and each entry
	// read, with what they left as the previous polygon's vertices)
	struct DynEntry { Meshes meshes; uint64_t hash = 0; uint32_t table = 0; std::vector<std::pair<uint32_t, uint32_t>> refs; Vertex prev[4]; uint16_t prev_uv[4][2]; };
	std::unordered_map<uint32_t, DynEntry> dyn_cache;
	std::vector<std::pair<uint32_t, uint32_t>> *colour_refs = nullptr;   // (parse: the colour table entries read)
	// the previous polygon's vertices, which the next can share (kept across models, as the hardware does)
	Vertex prev[4] = {};
	uint16_t prev_uv[4][2] = {};

	const uint32_t *culling(uint32_t addr) const;
	const uint32_t *model_at(uint32_t addr) const;
	void viewport(uint32_t addr, Scene &scene, int guard);
	void node(uint32_t addr);
	void node_ptr(uint32_t addr);
	void pointer_list(uint32_t addr);
	void draw_model(uint32_t addr);
	void mult_matrix(uint32_t index);
	Meshes parse(const uint32_t *data, bool dynamic);
};

} // namespace video

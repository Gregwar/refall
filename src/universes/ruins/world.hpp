#pragma once
// RUINS' world as meshes (world.cpp): built once per scene on the CPU, procedurally, from the same parameters the
// ruin's restoration uses (no asset files): the ground, the avenue of columns and its fallen pieces, the temple of
// the variant (MAYA's terraces, stairs, shrine and roof comb; KHMER's terrace, gallery and redented towers with
// their tiers and lotus buds, the strangler fig; GREEK's stylobate, cella, fluted columns, entablature and
// pediment), the great trunks, the braziers. Each frame animate() turns the restoration state into instance
// transforms (parts flying back into place, tiers and stones re-forming out of golden dust, the jungle's trees
// shrinking back), drawn instanced.
//
// Every part is authored at its rest pose (restored, in place) in world coordinates; an instance carries the
// transform from that rest pose to where the part is now, so the materials' patterns (masonry, carvings, moss) are
// computed in rest coordinates and stay on the stone as it moves.
#include <cstdint>
#include <vector>

#include "mathutil.hpp"

namespace ruins {

// GLSL's hash(vec2) and vnoise (shaders.hpp) on the CPU
float ghash(float x, float y);
float gnoise(float x, float y);

// A vertex: position, normal, a = material (0 ground, 1 stone, 2 bark, 3 column stone, 4 bronze), ambient
// occlusion, pattern (0 coursed masonry, 1 one block, 2 a drum), seed.
struct Vtx { float p[3], n[3], a[4]; };
struct Range { int first = 0, count = 0; };

// An affine transform: p' = x * c0 + y * c1 + z * c2 + t.
struct Aff {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1}, t{0, 0, 0};
    vec3 lin(const vec3& v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    vec3 operator()(const vec3& p) const { return lin(p) + t; }
    Aff operator*(const Aff& b) const { Aff r; r.c0 = lin(b.c0); r.c1 = lin(b.c1); r.c2 = lin(b.c2); r.t = (*this)(b.t); return r; }
    static Aff tr(const vec3& v) { Aff a; a.t = v; return a; }
    static Aff sc(const vec3& s) { Aff a; a.c0 = vec3(s.x, 0, 0); a.c1 = vec3(0, s.y, 0); a.c2 = vec3(0, 0, s.z); return a; }
    static Aff rotX(float a);
    static Aff rotY(float a);
    static Aff rotZ(float a);
};

// The kits: unit meshes drawn instanced (scaled and placed by their instances).
enum Kit { K_BLOCK, K_DRUM0, K_DRUM1, K_DRUM2, K_ECHINUS, K_BALUSTER, K_BRAZIER, K_ROOF, K_COUNT };

// What a piece does as time rewinds.
enum Role {
    R_STATIC,     // stays
    R_COLTOP,     // an avenue column's fallen top (idx: the column): lifts, turns upright, sets back on its stump
    R_FLY,        // flies from where it lay (from, tumbled by rot) back to its place, as e goes 0..1 (src, e0, e1)
    R_SHOW,       // re-forms out of golden dust when its group's condition holds (src: the group, a, b: its params)
    R_GONE,       // rubble: there while its group's condition holds, then gone (dust)
    R_BRAZIER,    // rises from the ground in the prime
    R_SINK,       // a jungle tree: shrinks back with the jungle (about its foot, from)
};
// The groups of R_SHOW / R_GONE pieces (each with its own condition on the restoration)
enum Group {
    G_NONE,
    G_SHRINE,     // MAYA's shrine and roof comb: under the ruin's ragged top (a, b: the piece's x, z)
    G_CORNER,     // MAYA's collapsed corner: closed
    G_GALLERY,    // KHMER's fallen stretch of gallery: under its ragged top (a: the piece's x)
    G_TIER,       // KHMER's tower tiers and buds (a: the tower, b: the tier)
    G_CELLA,      // GREEK's cella walls: under the ruin's ragged top
    G_GCOL,       // GREEK's columns' drums and capitals (a: the column, b: the drum's top)
    G_ENT,        // GREEK's entablature (a: the piece's left x)
    G_PED,        // GREEK's pediment (a: the slice's left x)
    G_FIG,        // KHMER's fig: there while its roots are
};

struct Piece {
    int kit = -1;          // a kit, or -1: a part of its own (range r)
    Range r;
    vec3 c{0, 0, 0}, s{1, 1, 1}; // the kit's rest placement (center, scale); a part: none
    float seed = 0.f, mat = -1.f; // mat >= 0: overrides the kit's material
    int role = R_STATIC, src = 0, idx = 0, group = G_NONE;
    float a = 0.f, b = 0.f, e0 = 0.f, e1 = 1.f;
    vec3 from{0, 0, 0}, rot{0, 0, 0};
};

// The restoration, as the universe drives it (see ruins.cpp)
struct State {
    int kind = 0;
    float jungle = 0.f, temple = 0.f, broken = 0.f, wear = 1.f, prime = 0.f;
    float col[8] = {};
};

// One instance, as the shaders read it: the model (rows of a 3x4, rest -> now, the kit's placement included), the
// rest placement (center, seed; scale, dissolve), params (glow, material override, -, -).
struct Inst { float m[12], c[4], s[4], p[4]; };
struct Draw { Range r; int first, count; bool dyn; };

class Scene {
public:
    void build(int kind);
    int kind() const { return kind_; }
    // the instances and their draws for this state; snap: the dissolves jump to their targets (no dust)
    void animate(const State& st, float dt, bool snap, std::vector<Inst>& inst, std::vector<Draw>& draws);
    const std::vector<Vtx>& verts() const { return verts_; }
    // the fig's roots, rebuilt as they withdraw (KHMER); true when they changed
    bool figChanged() const { return figChanged_; }
    const std::vector<Vtx>& figVerts() const { return fig_; }
    int triangles() const { return (int)verts_.size() / 3; }
    int instancedTriangles() const { return instTris_; }

private:
    int kind_ = -1;
    std::vector<Vtx> verts_, fig_;
    Range kit_[K_COUNT];
    std::vector<Piece> pieces_;
    std::vector<float> vis_;   // each piece's dissolve (1 gone .. 0 there)
    float figK_ = -1.f;
    bool figChanged_ = false;
    int instTris_ = 0;
    void buildFig(float rk);
};

} // namespace ruins

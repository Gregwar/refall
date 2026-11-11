#pragma once
// ARCHITECT's building site: the villa's construction program, authored element by element in the order it is
// built (site.cpp), and the trees and props around it. World meters: the ground is y = 0, x to the right, z toward
// the camera; the villa stands around the origin, the pool in front of its left wing.
#include <cmath>
#include <cstdint>
#include <vector>

#include "mathutil.hpp"

namespace arch_site {

struct Q { float x = 0, y = 0, z = 0, w = 1; };
inline Q qAxis(const vec3& axis, float ang) {
    const vec3 a = normalize(axis);
    const float s = std::sin(ang * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(ang * 0.5f)};
}
inline Q qYaw(float ang) { return {0.f, std::sin(ang * 0.5f), 0.f, std::cos(ang * 0.5f)}; }
inline Q qMul(const Q& a, const Q& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline vec3 qRot(const Q& q, const vec3& v) {
    const vec3 u(q.x, q.y, q.z);
    return v + cross(u, cross(u, v) + v * q.w) * 2.f;
}
// The rotation taking +Y to the direction d.
inline Q qFromY(const vec3& d) {
    const vec3 n = normalize(d);
    const float c = n.y;
    if (c > 0.99999f) return Q{};
    if (c < -0.99999f) return qAxis(vec3(1, 0, 0), 3.14159265f);
    const vec3 ax = cross(vec3(0, 1, 0), n);
    return qAxis(ax, std::acos(std::max(-1.f, std::min(1.f, c))));
}

inline vec3 mixc(const vec3& a, const vec3& b, float t) { return a + (b - a) * t; }

enum Mesh { BOX = 0, SPHERE = 1, CYL = 2, CONE = 3, MESH_COUNT = 4 };
enum Mat {
    RENDER = 0, WOOD = 1, GLASS = 2, STONE = 3, WATER = 4, FOLIAGE = 5, BARK = 6, LATTICE = 7, STEEL = 8, TILES = 9,
    SOIL = 10, FRAME = 11, LAMP = 12, FABRIC = 13, CABLE = 14, FROND = 15, BOARDFORM = 16, CONCRETE = 17, DECK = 18,
    CACTUS = 19
};

// What lights a part at dusk: a room (by the order the rooms light up), the garden's lamps, the pool.
enum Light { NO_LIGHT = -1, GARDEN_LAMP = 100 };

struct Part {
    vec3 c{0, 0, 0}, s{1, 1, 1};
    Q q;
    int mesh = BOX, mat = RENDER;
    vec3 col{1, 1, 1};       // linear
    float param = 0.f;       // wood: 1 horizontal boards; stone: course scale
    float depth = 3.f;       // glass: the room's depth behind it
    int light = NO_LIGHT;    // glass: the room's light order; lamps: GARDEN_LAMP
    bool tiles = false;      // the pool's tiles (raw concrete until they are laid)
};

// How an element comes (and goes): lowered by a crane, grown from its foot (slabs, walls, hedges, the water),
// a tree planted and growing, popped in (furniture, lamps), driven in.
enum Kind { CRANE, GROW, TREE, POP, DRIVE };

struct Element {
    float due = 0.f, dur = 1.f;   // paced lines: when it starts, over how many lines it comes (not for CRANE)
    float gone = 1e9f, goneDur = 1.5f; // when it leaves (the stakes, the mixer, the pallets)
    Kind kind = GROW;
    int crane = -1;               // CRANE: which crane lowers it (0 left, 1 right)
    std::vector<Part> parts;
    // derived (finish()): the bounds, the crane's attach point (the top's center), its base
    vec3 lo{0, 0, 0}, hi{0, 0, 0}, anchor{0, 0, 0};
    void finish();
};

// The variant's palette for the site (linear colors).
struct Palette {
    vec3 render, render2, stone, wood, deck, frame, concrete, glass, tile, water, soil;
    vec3 leaf, leaf2, bark, fabric, crane;
    int wallMat = RENDER;       // the main walls (render, or board-formed concrete)
    int upperMat = WOOD;        // the upper box (wood cladding, render, stone)
    int flora = 0;              // 0 olive and cypress, 1 pines and birches, 2 palms, 3 cacti and agaves
    vec3 lampCol{1.f, 0.75f, 0.45f};
};

// The program, in the order it is built (due ascending for each kind); and the scenery that is there from the
// start (the field's trees, far and near).
std::vector<Element> program(const Palette& pal, uint64_t seed);
std::vector<Part> scenery(const Palette& pal, uint64_t seed);

// The cranes' bases (x, z), their mast height and jib length.
constexpr float CRANE_X[2] = {-26.f, 23.f}, CRANE_Z[2] = {-19.f, -20.f};
constexpr float MAST_H = 17.f, JIB_L = 31.f, COUNTER_L = 9.f;

// Stage marks (paced lines, out of 50).
constexpr float L_LEVEL = 0.6f, L_SLAB = 2.f, L_CRANES = 4.5f, L_WALLS = 8.f, L_GLASS = 20.f, L_POOL = 30.f,
                L_GARDEN = 36.f, L_LEAVE = 44.f, L_LIGHTS = 45.5f, L_DONE = 50.f;
// The pool's pit (x0, z0, x1, z1) and depth; the site; the footprints the lawn keeps off.
constexpr float POOL[4] = {-23.f, 8.2f, -6.f, 12.4f};
constexpr float POOL_DEPTH = 1.6f;
constexpr float SITE[4] = {-30.f, -15.f, 26.f, 16.f};
extern const float FOOTPRINTS[][4];
extern const int FOOTPRINT_COUNT;

} // namespace arch_site

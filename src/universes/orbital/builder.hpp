#pragma once
// ORBITAL STATION: the helper that turns a module's parts (in its own frame) into instances (station frame).
#include <vector>

#include "universes/orbital/orbital.hpp"

namespace orbital {
enum Mat { M_HULL = 0, M_GOLD = 1, M_SOLAR = 2, M_RADIATOR = 3, M_TRUSS = 4, M_DARK = 5, M_EMIT = 6, M_HAB = 7, M_LAND = 8,
           M_SHIELD = 9, M_GLASS = 10, M_SILVER = 11, M_MOON = 12 };
enum Shape { S_BOX = 0, S_CYL = 1, S_SPHERE = 2, S_TORUS = 3, S_BAND = 5 };
inline void basisOf(const vec3& v, vec3& a, vec3& b) {
    const vec3 h = std::fabs(v.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
    a = normalize(cross(h, v));
    b = cross(v, a);
}
} // namespace orbital

struct OrbitalUniverse::Builder {
    std::vector<Part>& box;
    std::vector<Part>& fine;
    std::vector<Part>& coarse;
    std::vector<Glow>& glow;
    orbital::Xf xf;          // local -> station
    vec3 pivot{0.f};         // local point the module shrinks toward (a new game)
    float s = 1.f;           // its presence
    float emitK = 1.f;       // windows, its own light
    float glowK = 1.f;       // its lamps
    float towns = 0.f;
    float minR = 0.f;        // a cylinder thinner than this (meters) is drawn coarse
    float chaseOff = 0.f;    // its ring's turn, taken off the drop's chase (the strips of light)

    Builder(std::vector<Part>& b, std::vector<Part>& f, std::vector<Part>& c, std::vector<Glow>& g)
        : box(b), fine(f), coarse(c), glow(g) {}

    vec3 P(const vec3& c) const { return xf.at(pivot + (c - pivot) * s); }
    vec3 D(const vec3& v) const { return xf.dir(v) * s; }

    void put(std::vector<Part>& L, int shape, const vec3& c, const vec3& X, const vec3& Y, const vec3& Z, int mat,
             const vec3& col, float p0 = 0.f, float p1 = 0.f, float seed = 0.f, float p2 = 0.f, float p3 = 0.f, float g = -1.f) {
        if (s <= 0.002f) return;
        const vec3 C = P(c), x = D(X), y = D(Y), z = D(Z);
        Part p = {{C.x, C.y, C.z, (float)shape}, {x.x, x.y, x.z, (float)mat}, {y.x, y.y, y.z, p0}, {z.x, z.y, z.z, p1},
                  {col.x, col.y, col.z, seed}, {p2 * s, p3, g < 0.f ? emitK : g, chaseOff}};
        L.push_back(p);
    }
    // an axis-aligned box (local), half sizes
    void boxA(const vec3& c, const vec3& h, int mat, const vec3& col, float seed = 0.f, float g = -1.f) {
        put(box, orbital::S_BOX, c, vec3(h.x, 0, 0), vec3(0, h.y, 0), vec3(0, 0, h.z), mat, col, 0, 0, seed, 0, 0, g);
    }
    void boxO(const vec3& c, const vec3& X, const vec3& Y, const vec3& Z, int mat, const vec3& col, float seed = 0.f, float g = -1.f) {
        put(box, orbital::S_BOX, c, X, Y, Z, mat, col, 0, 0, seed, 0, 0, g);
    }
    // a beam from a to b, square section of half width w (its Y axis along it)
    void beam(const vec3& a, const vec3& b, float w, int mat, const vec3& col, float seed = 0.f) {
        const vec3 d = b - a;
        const float L = length(d);
        if (L < 1e-5f) return;
        const vec3 ax = d / L;
        vec3 u, v;
        orbital::basisOf(ax, u, v);
        boxO((a + b) * 0.5f, u * w, ax * (L * 0.5f), v * w, mat, col, seed);
    }
    void cyl(const vec3& c, const vec3& axis, float r, float hl, int mat, const vec3& col, float top = 1.f, bool open = false,
             float seed = 0.f, float g = -1.f) {
        vec3 u, v;
        orbital::basisOf(axis, u, v);
        std::vector<Part>& L = r * s < minR ? coarse : fine;
        put(L, orbital::S_CYL, c, u * r, axis * hl, v * r, mat, col, top, open ? -1.f : 1.f, seed, 0, 0, g);
    }
    // a cylinder between two points
    void tube(const vec3& a, const vec3& b, float r, int mat, const vec3& col, float seed = 0.f) {
        const vec3 d = b - a;
        const float L = length(d);
        if (L < 1e-5f) return;
        cyl((a + b) * 0.5f, d / L, r, L * 0.5f, mat, col, 1.f, false, seed);
    }
    void sphere(const vec3& c, float r, int mat, const vec3& col, float seed = 0.f, float g = -1.f) {
        std::vector<Part>& L = r * s < minR ? coarse : fine;
        put(L, orbital::S_SPHERE, c, vec3(r, 0, 0), vec3(0, r, 0), vec3(0, 0, r), mat, col, 0, 0, seed, 0, 0, g);
    }
    // a torus arc about axis n through c, angle 0 along ref, from a0 over al radians
    void torus(const vec3& c, const vec3& n, const vec3& ref, float R, float r, float a0, float al, int mat, const vec3& col,
               float seed = 0.f, float g = -1.f) {
        put(fine, orbital::S_TORUS, c, ref * R, n * r, cross(n, ref) * R, mat, col, a0, al, seed, 0, 0, g);
    }
    void band(const vec3& c, const vec3& n, const vec3& ref, float R, float hw, float a0, float al, float wall, int mat,
              const vec3& col, float seed, float g, float towns_) {
        put(fine, orbital::S_BAND, c, ref * R, n * hw, cross(n, ref) * R, mat, col, a0, al, seed, wall, towns_, g);
    }
    // a light (local position, radius in meters), mode 0 lamp, 2 plume (along dir), 3 glint
    void light(const vec3& p, float r, const vec3& col, float k, int mode = 0, const vec3& dir = vec3(0, 1, 0)) {
        const float kk = k * glowK;
        if (kk <= 0.003f || s <= 0.02f) return;
        const vec3 W = P(p), d = xf.dir(dir);
        Glow g = {{W.x, W.y, W.z, r * s}, {col.x, col.y, col.z, kk}, {d.x, d.y, d.z, (float)mode}};
        glow.push_back(g);
    }
};

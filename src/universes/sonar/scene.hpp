#pragma once
// SONAR's sunken world in 3D (see scene.cpp): the seabed, its rocks, a wreck and the sunken city's buildings as
// one static mesh, and the closed loop the camera glides along.
#include <cstdint>
#include <vector>

#include "mathutil.hpp"

namespace sonar_scene {

// One vertex: position, normal, the distances to its polygon's edges (world units: the shader draws a fine line
// of light along them, 100 = no line), and layer, flags and the face's own coordinates (for the windows).
struct Vtx { float p[3], n[3], e[4], i[4]; };
enum Layer { BED = 0, ROCK = 1, WRECK = 2, CITY = 3 };
enum Flags { WINDOWS = 1, CONTOURS = 2 };
// A point an echo can return from (just outside a surface, on the side the camera passes).
struct Anchor { vec3 p; int layer; };

// A local frame: origin and three unit axes (x right, y up, z along).
struct Fr {
    vec3 o, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    vec3 at(const vec3& l) const { return o + x * l.x + y * l.y + z * l.z; }
    vec3 dir(const vec3& l) const { return x * l.x + y * l.y + z * l.z; }
    Fr moved(const vec3& l) const { Fr f = *this; f.o = at(l); return f; }
    Fr rotX(float a) const; // turned about its own axes
    Fr rotY(float a) const;
    Fr rotZ(float a) const;
};

class Scene {
public:
    void build();
    std::vector<Vtx> verts;
    std::vector<uint32_t> idx;
    std::vector<Anchor> anchors;
    float height(float x, float z) const; // the seabed (bilinear on the terrain grid)
    float loopLength() const { return len_; }
    vec3 path(float s) const;             // the camera's loop by arc length (wraps)

private:
    // the loop
    std::vector<vec3> ctrl_;
    std::vector<float> tabS_, tabU_;
    float len_ = 1.f;
    vec3 spline(float u) const;
    void buildPath();
    float pathDist(float x, float z, float& y) const;
    // the terrain
    static constexpr int GN = 240;
    static constexpr float GEXT = 96.f;
    std::vector<float> hgt_;
    float rawHeight(float x, float z) const;
    void buildTerrain();
    // geometry
    void poly(const vec3* p, int n, const vec3* nrm, int layer, int flags, unsigned edgeMask);
    void quad(const vec3& a, const vec3& b, const vec3& c, const vec3& d, int layer, int flags = 0, unsigned edges = 15);
    void box(const Fr& f, float hx, float hy, float hz, int layer, int flags = 0);
    void cylinder(const Fr& f, float r, float h, int seg, int layer, bool flutes, bool caps = true);
    void dome(const Fr& f, float r, int mer, int rings, int layer);
    void rock(const vec3& c, float r, float squash, uint64_t seed);
    void wreck(const Fr& f);
    void anchor(const vec3& p, int layer) { anchors.push_back({p, layer}); }
    // the city, placed along the loop
    Fr station(float frac) const;           // a frame on the seabed at a fraction of the loop (x to the right)
    float ground(const Fr& f, float x, float z, float r) const; // lowest seabed under a footprint
    void buildCity();
};

} // namespace sonar_scene

#pragma once
// PLANKS' village: what the cleared rows build beside the well, authored as lists of real planks (every plank the
// same, 1 : 3 : 15), in the order they are laid (see build.cpp).
#include <cmath>
#include <vector>

#include "mathutil.hpp"

namespace planks_build {

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
inline Q qSlerp(Q a, const Q& b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0) { a = {-a.x, -a.y, -a.z, -a.w}; d = -d; }
    float k0, k1;
    if (d > 0.9995f) { k0 = 1 - t; k1 = t; }
    else {
        const float th = std::acos(d), s = std::sin(th);
        k0 = std::sin((1 - t) * th) / s;
        k1 = std::sin(t * th) / s;
    }
    Q r{a.x * k0 + b.x * k1, a.y * k0 + b.y * k1, a.z * k0 + b.z * k1, a.w * k0 + b.w * k1};
    const float n = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
    return {r.x / n, r.y / n, r.z / n, r.w / n};
}
// The rotation taking the local axes to X, Y, Z (orthonormal, right handed).
Q qBasis(const vec3& X, const vec3& Y, const vec3& Z);

// The village's plank: the well's planks are 1 long (a cell), the village's a little longer (it stands farther).
constexpr float LC = 2.6f, WC = LC / 5.f, TC = LC / 15.f;
constexpr float TABLE = -10.4f;

// A plank of the village: its center and turn (world; the train's planks in their car's frame), the car it rides
// (0 none, 1 the locomotive, 2 the carriage) and its height over the table (the ripples run up by it).
struct PDef {
    vec3 c{0, 0, 0};
    Q q;
    int group = 0;
    float h = 0.f;
};

// Each side's program (0 left, 1 right), in the order its planks are laid; beyond it, more builds farther back.
std::vector<PDef> program(int side);
std::vector<PDef> extra(int side, int n);

// The track: a curve on the table behind the well, both ends in view; the train shuttles along it.
float trackLength();
vec3 trackPoint(float s, vec3* tangent);
float trainStation();         // where the train is built (arc length of the locomotive's center)
constexpr float CAR_GAP = 2.75f; // the carriage's center, behind the locomotive's

} // namespace planks_build

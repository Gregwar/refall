#pragma once
// REWILD's world, shared by its simulation (rewild.cpp) and its drawing (render.cpp): the land, the life map, the
// palettes. groundH is the same function as in WORLD_GLSL (shaders.hpp).
#include <cmath>

#include "universes/rewild/rewild.hpp"

namespace rewild_world {

using Palette = RewildUniverse::Palette;

// the diorama's cut face, just behind the board plane: the land lies behind it
constexpr float FRONT_Z = -1.0f;
// how many cleared lines turn the dry land into a full jungle (a Tetris counts 1.5 per line)
constexpr float FULL_LINES = 60.f;
// gravity for the flying seeds and the shards
constexpr float GRAV = 18.f;
// the life map: a square of the land behind the well, one texel per LIFE_CELL board units
constexpr int LIFE_RES = 128;
constexpr float LIFE_X0 = -64.f, LIFE_Z0 = -1.f, LIFE_CELL = 1.f; // x from -64 to 64, z from -1 back to -129
// the camera's eye height (board units)
constexpr float EYE_Y = 5.2f;

inline vec2 v2(float x, float y) { return vec2{x, y}; }
inline vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// The land: a wide shallow plain behind the well, gently rolling, rising far out into a line of low hills on the
// horizon (z <= FRONT_Z).
inline float groundH(float x, float z) {
    const float dz = -z;
    const float d = 0.35f * std::sin(x * 0.23f + 1.3f + z * 0.13f) + 0.22f * std::sin(x * 0.61f + 0.4f - z * 0.29f) +
                    0.12f * std::sin(x * 1.3f + 2.f + z * 0.7f);
    const float hill = smoothstepf(30.f, 170.f, dz) *
                       (5.f + 4.f * std::sin(x * 0.035f + 0.8f) + 3.f * std::sin(x * 0.071f - z * 0.02f + 2.1f) +
                        2.f * std::sin(x * 0.13f + z * 0.05f));
    return -9.4f + 1.2f * smoothstepf(5.f, 60.f, dz) + d * (1.f + 0.8f * smoothstepf(10.f, 80.f, dz)) + hill;
}
inline vec3 groundN(float x, float z) {
    const float e = 0.05f;
    return normalize(vec3(groundH(x - e, z) - groundH(x + e, z), 2.f * e, groundH(x, z - e) - groundH(x, z + e)));
}

// A small value noise (the same lattice hash as nothing else: only the simulation uses it, to cluster the forest).
inline float hash2(float x, float y) {
    const uint64_t h = splitmix64((uint64_t)(int64_t)std::floor(x) * 73856093ull ^ (uint64_t)(int64_t)std::floor(y) * 19349663ull);
    return (h >> 40) * (1.0f / 16777216.0f);
}
inline float vnoise2(float x, float y) {
    const float ix = std::floor(x), iy = std::floor(y), fx = x - ix, fy = y - iy;
    const float ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy);
    const float a = hash2(ix, iy), b = hash2(ix + 1.f, iy), c = hash2(ix, iy + 1.f), d = hash2(ix + 1.f, iy + 1.f);
    return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

Palette paletteOf(int variant);
void lerpPalette(Palette& a, const Palette& b, float k);
int variantCount();
const char* variantName(int i);

} // namespace rewild_world

#pragma once
// TERRARIUM's world as meshes (world.cpp), built on the CPU from the same shapes the jars always had (no asset
// files): the room (the window wall, the sill, the shelves), each jar's glass (its outer surface and its cavity, a
// revolved profile found from the same signed distance as before), its cork, its soil (the heightfield and its
// section against the glass), its pond, the slots' plants at their growth (moss cushions, ferns with their pinnae
// and fiddleheads, toadstools, saplings, flowers, rosettes, cacti, pebbles, mossy rocks and their trickles), the
// sprouts, the beetle and the snail at their pose, the glass tubes.
//
// The jars' contents are in jar units (the jar's radius 1, its foot at the origin), drawn with the jar's place and
// size; a slot's mesh is rebuilt only when its growth (or its soil) changed, so a still jar costs no upload.
#include <cstdint>
#include <vector>

#include "mathutil.hpp"

namespace terra {

// A vertex: position, normal; m = material (the old gMat ids, see shaders.hpp), ambient occlusion, seed,
// two-sided (thin leaves: the normal turned toward the eye); f = the material's parameters (the old gInf); s = the
// sway: a turn about the vertical through (s.x, s.y), amplitude s.w, phase s.z (ferns, broad leaves).
struct Vtx { float p[3], n[3], m[4], f[4], s[4]; };

// The jar's shape (as the shaders': JR, JHB, JRN, JHN, JT)
constexpr float JR = 1.f, JHB = 0.8f, JRN = 0.62f, JHN = 0.93f, JT = 0.022f;
constexpr float BS = 1.9f; // the beetle's scale

// GLSL's hashes and value noise (COMMON_GLSL) on the CPU
float ghash(float x, float y);
float ghash1(float n);
float ghash3(const vec3& p);
float gnoise3(const vec3& p);

// A jar's soil and pond, as the shader's soilH: pond center, radius, current depth
struct Pond { float x = 0.5f, z = 0.42f, r = 0.19f, depth = 0.f; };
float soilH(float x, float z, const Pond& p);

// One slot as it is drawn: type (TerrariumUniverse::SlotType), base (on the soil), size, growth, seed, rotation, param
struct SlotDraw { int type; vec3 base; float size, g, seed, rot, param; };
// The pond as the rock's trickle needs it
struct PondState { Pond pd; float level = 0.f, fill = 0.f; };

// The fixed meshes
void buildGlass(std::vector<Vtx>& out, bool inner); // the jar's outer surface, or its cavity (glass's inside)
void buildCork(std::vector<Vtx>& out);
void buildRoom(std::vector<Vtx>& out);              // the wall, the sill and its front, the shelves (world units)
// a glass tube: a quadratic curve a -> c -> b, radius r (f.x: where along it, 0..1; world units)
void buildTube(std::vector<Vtx>& out, const vec3& a, const vec3& c, const vec3& b, float r);

// A jar's contents (jar units)
void buildSoil(std::vector<Vtx>& out, const Pond& pd, int kind);  // the soil's top and its section against the glass
void buildPond(std::vector<Vtx>& out, const Pond& pd, float level); // the water's surface
void buildSlot(std::vector<Vtx>& out, const SlotDraw& s, const PondState& ps);
// a sprout: where, growth, its index (its shape from a hash, as before), the jar's world kind
void buildSprout(std::vector<Vtx>& out, const vec3& at, float g, int k, int kind);
// the beetle (world units): position, heading, walk phase, moving, look, time (antennae); its blink is the shader's
void buildBeetle(std::vector<Vtx>& out, const vec3& at, float head, float walk, float mv, float look, float time);
// the snail (jar units, on a soil): time
void buildSnail(std::vector<Vtx>& out, float time, const Pond& pd);

} // namespace terra

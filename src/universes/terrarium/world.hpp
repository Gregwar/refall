#pragma once
// TERRARIUM's world as meshes, built on the CPU (no asset files). The jar (world.cpp), from the same shapes it always
// had: its glass (its outer surface and its cavity, a revolved profile found from its signed distance), its cork, its
// soil (the heightfield and its section against the glass), its pond, the slots' plants at their growth (moss
// cushions, ferns with their pinnae and fiddleheads, toadstools, saplings, flowers, rosettes, cacti, pebbles, mossy
// rocks and their trickles), the sprouts, the beetle at its pose. The lab, the city and what grows over them
// (lab.cpp).
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
// g = the growth (what creeps over the lab and the city): the vertex is drawn at pivot + (p - pivot) x k, k rising
// from 0 to 1 over a few lines from the appear line g.w (g.w <= -999: always there).
struct Vtx { float p[3], n[3], m[4], f[4], s[4], g[4]; };

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

// A jar's contents (jar units)
void buildSoil(std::vector<Vtx>& out, const Pond& pd, int kind);  // the soil's top and its section against the glass
void buildPond(std::vector<Vtx>& out, const Pond& pd, float level); // the water's surface
void buildSlot(std::vector<Vtx>& out, const SlotDraw& s, const PondState& ps);
// a sprout: where, growth, its index (its shape from a hash, as before), the jar's world kind
void buildSprout(std::vector<Vtx>& out, const vec3& at, float g, int k, int kind);
// the beetle (world units): position, heading, walk phase, moving, look, time (antennae); its blink is the shader's
void buildBeetle(std::vector<Vtx>& out, const vec3& at, float head, float walk, float mv, float look, float time);

// ---------------------------------------------------------------- the lab and the city (lab.cpp)
// World units are the jar's (its radius 1, about 15 cm; its foot at the origin on the bench, y up). The lab is a room
// on the third floor of a concrete building whose street front faces +z; the city stands all around it.
namespace lab {
constexpr float FLOOR = -6.f, CEIL = 14.f;          // the lab's floor and ceiling
constexpr float RX = 30.f, RZ0 = -20.f, RZ1 = 33.f;  // its walls' inner faces (x = +-RX; the back, the front)
constexpr float BACK = -21.5f, FRONT = 35.f;         // the building's back and street faces
constexpr float BX = 60.f;                           // its half width
constexpr float GROUND = -53.f, STOREY = 23.5f, ROOF = 64.5f;
constexpr float WIN_Y0 = -2.f, WIN_Y1 = 12.f;        // the windows' sills and heads (the lab's floor)
constexpr float FWIN_X = 24.f;                       // the street window's half width (open: the camera leaves by it)
constexpr float BWIN_X0 = 6.f, BWIN_X1 = 19.f;       // the back windows (x = +-[X0, X1])
// the crack in the jar's glass: its center (the glass's u = the angle atan(x, z), v = the height)
constexpr float CRACK_U = -1.0f, CRACK_V = 0.36f;
// the growth's two clocks: what escapes the jar onto the bench (appear lines below GROUP_B) runs on its own
// clock (the burst at a drop can start it early); the room and the city on the other
constexpr float GROUP_B = 26.5f;
} // namespace lab

// A point where the growth flowers (spores and pollen at the drop come from those already grown), or a light along
// the vines at night: where, the line it appears at, its size
struct Bloom { vec3 p; float appear, scale; };
// A point of a neon tube (its glow), its color (0..2) and its sign (flicker)
struct NeonPt { vec3 p; int color; float sign; };
// A light in the city at night (a neon sign, a street lamp): where, reach, color index (0..2 neon, 3 lamp), sign id
struct CityLight { vec3 p; float reach; int color; float sign; };

void buildLab(std::vector<Vtx>& out);          // the lab: the room, its benches, shelves, equipment, the tag (opaque)
void buildLabGlass(std::vector<Vtx>& out);     // its glassware and the back windows' panes (laid over as glass)
// (with the overgrowth of the buildings around ours: their vines, clumps, trees and roofs' crowns)
void buildCity(std::vector<Vtx>& out, std::vector<NeonPt>& neon, std::vector<CityLight>& lights, std::vector<Bloom>& blooms);
// what grows out of the jar over the bench, the room and the city: vines, roots, leaf clumps, flowers, trees
// (benchEnd: where what grows over the bench ends in out: the shadow map draws only the lab and that)
void buildGrowth(std::vector<Vtx>& out, std::vector<Bloom>& blooms, size_t* benchEnd = nullptr);

} // namespace terra

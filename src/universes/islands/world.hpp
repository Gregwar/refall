#pragma once
// ISLANDS' world: the endless archipelago the flight crosses (world.cpp), its islands, their shapes (one height
// function per island type, shared by the terrain meshes and the camera's terrain following) and their content
// (palms, villas, decks, jetties, bungalows on stilts, villages, lighthouses, umbrellas), and the route the flight
// follows through them (a smooth steered curve, with loops around some islands for the orbits).
//
// Coordinates: meters, y up, the sea at y = 0. The world is rebased now and then (rebase()) so that every float
// stays near the camera; the cells that place the filler islands are kept in absolute coordinates (doubles).
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include "mathutil.hpp"

namespace isl {

// ---------------------------------------------------------------- small helpers
float hashf(uint32_t x);
uint32_t hashu(uint32_t x);
float noise2(float x, float y, uint32_t seed);
float fbm2(float x, float y, uint32_t seed, int oct);
float smooth01(float t);

struct Q { float x = 0, y = 0, z = 0, w = 1; };
Q qAxis(const vec3& axis, float a);
Q qMul(const Q& a, const Q& b);
vec3 qRot(const Q& q, const vec3& v);
inline Q qYaw(float a) { return qAxis(vec3(0, 1, 0), a); }

// ---------------------------------------------------------------- meshes and materials
enum Mesh {
    M_BOX, M_SPHERE, M_LUMP, M_CYL, M_CONE, M_FRUSTUM, M_DOME, M_GABLE, M_HIP, M_HULL, M_SAIL, M_JIB,
    M_PALM0, M_PALM1, M_PALM2, M_BIRD, M_DOLPHIN, M_COUNT
};
enum Mat {
    MAT_RENDER, MAT_GLASS, MAT_DECK, MAT_WOOD, MAT_THATCH, MAT_POOL, MAT_PAINT, MAT_FOLIAGE, MAT_PALM, MAT_SAIL,
    MAT_HULL, MAT_CANVAS, MAT_ROCK, MAT_LAMP, MAT_METAL, MAT_TOWER, MAT_BIRD, MAT_TILE, MAT_COUNT
};
// One instance of a mesh: a = position, seed; b = scale, material; c = color, param; d = rotation.
struct Inst { float a[4], b[4], c[4], d[4]; };
Inst inst(int mat, const vec3& p, const vec3& s, const Q& q, const vec3& col, float param = 0.f, float seed = 0.f);
struct Part { int mesh; Inst in; };

// The object meshes, built once: pos, normal, aux (part, along) per vertex (8 floats), unit sized.
std::vector<float> buildMesh(int mesh);

// ---------------------------------------------------------------- the variants' world style
struct Style {
    float w[6];          // weights of the island types for the fillers (VOLCANO CAY ATOLL ISLET VILLAGE VILLA)
    int heroBag[8];      // the hero sequence (types; -1 ends it; 100 + type: an orbit around it, 200: a gap)
    float fillP;         // a filler island in a cell
    bool dry;            // limestone (GOLDEN): olives and cypresses, white cube villages, narrow shelves, more cliffs
    bool thatch;         // atolls (CORAL): overwater bungalows on most villages, peaks in the atolls
    vec3 villageWalls[4];
    vec3 roof, hull[3], sailCol, canvas[3], accentWood;
};

// ---------------------------------------------------------------- islands
enum Type { T_VOLCANO, T_CAY, T_ATOLL, T_ISLET, T_VILLAGE, T_VILLA, T_COUNT };

struct Obst { float x, z, r, top; };   // what stands above the terrain (the camera passes over it)
struct Light { vec3 p; float size; vec3 col; int kind; }; // the lights at dusk (0 window, 1 lamp, 2 pool, 3 lighthouse)

struct Island {
    int type = T_CAY;
    float x = 0, z = 0;                // world position (rebased)
    float R = 50, H = 2;               // radius (the coast), height
    uint32_t seed = 1;
    float face = 0;                    // the angle (around y, atan2(z, x)) the island turns to the route
    float ext = 200;                   // half extent of its terrain grid
    bool hero = false, peak = false, dry = false;
    float cliffK = 0.5f, shelfW = 1.f;
    // the villa's pad (local, along the face): its center and half sizes, its height
    float padX = 0, padZ = 0, padA = 0, padHW = 0, padHD = 0, padH = 0;
    float fallA = 0, fallW = 0;        // a waterfall: its angle and half width (radians); 0: none
    vec3 focus{0, 0, 0};               // the spot the camera turns to (world)
    float top = 0;                     // the highest point (with the objects)
    std::vector<Part> parts;           // world coordinates
    std::vector<Obst> obst;
    std::vector<Light> lights;
    std::vector<vec3> shore;           // points along the beach (the clears' lanterns)
    std::vector<vec3> birdSpots;       // where flocks rise from
    // GL (main thread only; freed through the graveyard, see takeGraveyard)
    ~Island();
    unsigned vao = 0, vbo = 0;
    int lod = -1;                      // the grid size uploaded (index into LOD_N), -1 none
    int want = -1, jobLod = -1;
    bool busy = false;
    std::vector<float> ready;          // a finished mesh waiting for its upload
    int readyLod = -1;
};
constexpr int LOD_COUNT = 5;
constexpr int LOD_N[LOD_COUNT] = {33, 65, 129, 257, 385};
constexpr int TERRAIN_FLOATS = 11;     // pos 3, normal 3, attr 4 (sand, vegetation, rock, reef), occlusion 1

// The island's ground at a local point: its height and materials.
struct Ground { float h, sand, veg, rock, reef; };
Ground groundAt(const Island& I, float lx, float lz);
inline float heightAt(const Island& I, float lx, float lz) { return groundAt(I, lx, lz).h; }
// The terrain grid of an island (N x N vertices over [-ext, ext]^2, local coordinates).
std::vector<float> buildTerrain(const Island& I, int n);
// The island's terrain as drawn at a local point: its grid at the level of detail uploaded (flat facets between
// jittered vertices, which stand above the height function where it dips between them); -45 off the grid.
float drawnHeight(const Island& I, float lx, float lz);

// ---------------------------------------------------------------- the route and the archipelago
struct RoutePt { float x, z, head, curv; double s; };
struct Hero {
    std::shared_ptr<Island> isl;       // (a gap: the left one; isl2 the right one)
    std::shared_ptr<Island> isl2;
    double s0, s1, sc;                 // where it starts and ends along the route, the closest approach
    float side;                        // +1 the island on the left
    bool orbit = false, gap = false;
};

class World {
public:
    World();
    ~World();
    void reset(uint64_t seed, const Style& st);
    // Streams the world around the camera (route ahead, filler islands, meshes to build).
    void update(const vec3& cam, double s, float viewDist);
    RoutePt route(double s) const;
    // The highest thing at a point (terrain and what stands on it), and the terrain alone.
    float obstacle(float x, float z) const;
    float terrain(float x, float z) const;
    // The terrain or what stands on it (below the sea too: the depth where nothing stands).
    float solid(float x, float z) const;
    // The ground as drawn (the islands' facets) at (x, z) and at six points around it at r, the highest, and what
    // stands within r of it (the obstacles).
    float groundMax(float x, float z, float r) const;
    // An island's grid within r of (x, z) (else only the open sea's floor there: a quick test before the others);
    // the islands whose grids are, and solid() among those only (many tests in one place).
    bool landNear(float x, float z, float r) const;
    void islandsNear(float x, float z, float r, std::vector<const Island*>& out) const;
    static float solidAmong(const std::vector<const Island*>& list, float x, float z);
    // Moves everything by (-dx, -dz) (the camera stays near the origin).
    void rebase(float dx, float dz);
    const Hero* heroNear(double s, double ahead, double behind) const;
    std::shared_ptr<Island> nearestIsland(const vec3& p, float maxDist) const;

    std::vector<std::shared_ptr<Island>> islands;
    std::deque<RoutePt> pts;
    std::vector<Hero> heroes;
    double ox = 0, oz = 0;             // the absolute position of the local origin
    // meshes built on the worker, to upload (main thread): the island and its lod
    void collectReady(std::vector<std::shared_ptr<Island>>& out);
    void requestMesh(const std::shared_ptr<Island>& I, int lod);

private:
    Style st_;
    uint64_t seed_ = 1;
    // the route generator
    struct Gen {
        float x = 0, z = 0, head = 0, curv = 0;
        double s = 0;
        float base = 0, p1 = 0, p2 = 0;
        int orbitLeft = 0;             // samples left in an orbit
        float orbitK = 0;
        double nextHero = 300;
        int bag = 0;
    } g_;
    void extendRoute(double toS);
    void stepRoute();
    void placeHero(int kind);
    std::shared_ptr<Island> makeIsland(int type, float x, float z, uint32_t seed, bool hero, float face, float sizeK, bool content = true);
    bool clearOf(float x, float z, float r, bool routeToo) const;
    std::set<std::pair<long long, long long>> cells_;
    void fillCells(const vec3& cam, float radius);
    uint32_t nextSeed();
    uint32_t seedCounter_ = 1;

    // the mesh worker
    struct Job { std::shared_ptr<Island> isl; int lod; };
    std::thread worker_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::vector<std::pair<std::shared_ptr<Island>, std::pair<int, std::vector<float>>>> done_;
    bool quit_ = false;
    void work();
};

// The GL buffers of the islands gone (an island may be released on any thread): the main thread frees them.
void takeGraveyard(std::vector<unsigned>& vaos, std::vector<unsigned>& vbos);

// Builds an island's content (palms, trees, its villa, village, bungalows, lighthouse...) and its obstacles.
void furnish(Island& I, const Style& st);

} // namespace isl

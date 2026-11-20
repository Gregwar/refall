#pragma once
// CORAL REEF: a sunny shallow reef that grows behind the well with the cleared lines (see coralreef.cpp).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The reef's world (meters, y up, the sand around y = 0, the surface at reef::SURF_Y), shared by the C++ side and
// the shaders (REEF_GLSL repeats groundH).
namespace reef {
constexpr float SURF_Y = 7.5f;    // the water's surface
constexpr int MAX_MOUNDS = 16;    // rock outcrops in the seabed
struct Mound { float x, z, r, h, seed; };
// The seabed's height at (x, z): a gentle slope down into the distance, low dunes, the rock outcrops.
float groundH(float x, float z, const Mound* m, int n);
// How much of the ground at (x, z) is rock (0 sand .. 1 rock).
float rockAt(float x, float z, const Mound* m, int n);

// A mesh: 10 floats a vertex (position, normal, attributes: along 0 base .. 1 tip, a free parameter, the part,
// a phase), indexed.
struct Mesh {
    std::vector<float> v;
    std::vector<uint32_t> idx;
    int vertices() const { return (int)(v.size() / 10); }
};
// The coral forms (each a few templates), the fish species, the manta.
enum Kind { K_BRAIN, K_STAG, K_TABLE, K_FAN, K_SPONGE, K_ANEMONE, K_SOFT, K_COUNT };
constexpr int TEMPLATES = 3;              // templates per kind
Mesh buildCoral(int kind, int variant);   // unit size (about 1 m tall or wide), its base at the origin
Mesh buildRock(int variant);              // a boulder, unit wide
// The tower (the growth's later stages, as towerProfile in shaders.hpp).
constexpr float TOWER_CX = 3.f, TOWER_CZ = -6.f, TOP_R = 46.f, TOWER_SLOPE = 2.6f, TOWER_MAX = 430.f;
inline float towerE(float a) {
    return 1.f + 0.2f * std::sin(3.f * a + 0.7f) + 0.11f * std::sin(5.f * a + 2.f) + 0.06f * std::sin(11.f * a + 0.3f) + 0.03f * std::sin(23.f * a);
}
// The tower's surface height at (x, z) for a floor at floorY (without the summit's own ground).
inline float towerH(float x, float z, float floorY) {
    const float dx = x - TOWER_CX, dz = z - TOWER_CZ;
    const float r = std::sqrt(dx * dx + dz * dz), a = std::atan2(dz, dx);
    const float t = r / towerE(a) - TOP_R;
    const float top = -1.6f + 0.4f * std::sin(0.21f * x + 0.4f) * std::sin(0.17f * z + 1.1f);
    if (t <= 0.f) return top;
    float zz = t * TOWER_SLOPE;
    zz = zz + 5.6f * std::sin(zz * 6.2831f / 38.f + a * 0.6f);
    zz += 4.f * std::sin(a * 7.f + zz * 0.05f) * smoothstepf(0.f, 20.f, zz);
    const float h = top - std::max(zz, 0.f);
    const float k = 12.f;
    return floorY + std::log(std::exp((h - floorY) / k) + 1.f) * k;
}
// The tower's radius at a depth (meters below the summit), along an angle.
inline float towerR(float depth, float a) { return (TOP_R + depth / TOWER_SLOPE) * towerE(a); }
enum Species { F_CHROMIS, F_ANTHIAS, F_YELLOW, F_BLUE, F_IDOL, F_PARROT, F_CLOWN, F_COUNT };
Mesh buildFish(int species);              // unit length along +x (the nose), centered
Mesh buildManta();                        // unit half span along x, the head toward +z
} // namespace reef

class CoralReefUniverse : public Universe {
public:
    ~CoralReefUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // A variant's colors (linear RGB) and light: floats only (palettes are blended float by float).
    struct Palette {
        vec3 water, deep, haze, surface, sun, shade, rim, caustic, ray;
        vec3 sand, sand2, rock, rock2, crust, crust2;
        vec3 reef[8];
        vec3 ink, accent, veil, rail;
        vec3 piece[7];
        float sunAz, sunEl, fog, rays, caustics, glow, night, sat;
    };

private:
    // ---- The reef: slots planted one after the other with the lines (paceLines: 50 at scene completion).
    struct Slot {
        vec3 p;          // its base (meters)
        float size, yaw, seed;
        int kind, tmpl;
        vec3 colA, colB; // reef palette indices resolved per frame (see colIdx)
        int colIdx[2];
        float birth;     // the line (on the 50-line scale) that plants it
        float startT;    // when it started growing (seconds, world clock), < 0: not yet
        float grow;      // 0..1 shown
        bool awaiting;   // a seed is on its way to it
    };
    std::vector<Slot> slots_;
    struct Rock { vec3 p; float size, yaw, seed; int tmpl; float birth, grow; };
    std::vector<Rock> rocks_;           // boulders on the outcrops and in the sand, the bommie's as it rises
    float surfaceAt(float x, float z) const; // the seabed or a boulder's top
    uint64_t reefSeed_ = 0;
    bool reefDirty_ = true;
    void buildReef();                   // the slots and the seabed's outcrops for reefSeed_
    reef::Mound mounds_[reef::MAX_MOUNDS];
    int moundCount_ = 0;
    float lines_ = 0.f, shownLines_ = -1.f; // the paced lines, and the growth shown (springs after them)
    bool shownLinesFirst_ = false;
    std::vector<vec2> clearPts_;            // the last clear's cells (board units): the seeds start there
    bool clearPending_ = false;

    // ---- Fish: schools steered like boids, clownfish by their anemones.
    struct Fish {
        vec3 p, v, dir;
        float phase, size, bank, speed, seed;
        int school;     // -1: a clownfish, -2: a little fish of the coral head and the bommie
        int home;       // clownfish: its anemone slot; -2: its hovering radius index
        float in;       // 0..1 fading in (new fish swim in from afar)
    };
    struct School {
        int species;
        float appear;   // the line it appears at
        int maxCount;   // fish at scene completion
        vec3 target, center;
        float path, speed, size;
        vec2 zoneX;     // its favorite stretch (x, meters)
    };
    std::vector<Fish> fish_;
    std::vector<School> schools_;
    void buildSchools();
    void updateFish(float dt, float energy, float melody, float swirl);
    vec3 swirlC_{0, 3, -8};
    float swirl_ = 0.f, swirlT_ = -1.f, swirlSide_ = 1.f; // a quad: the schools swirl together for a while

    // ---- The drop: mass coral spawning, a pair of mantas looping through a side band, the schools in one
    // great bait ball on the other side, the light turning dusky blue-violet.
    struct Egg { vec3 p; float r, age, life, seed, fade; vec3 col; };
    std::vector<Egg> eggs_;
    float spawnAcc_ = 0.f, dropT_ = 0.f, dropLight_ = 0.f, mantaK_ = 0.f, pulse_ = 0.f;
    float mantaT_ = 0.f;
    float mantaSide_ = -1.f;  // the band the mantas loop in (the bait ball takes the other)
    bool dropOn_ = false;
    vec3 mantaAt(float t, float enter) const;
    void spawnEgg(const Slot& sl, float delay); // the leader's place (world) at time t of its loop

    // ---- Particles in the world: bubbles, coral seeds, specks.
    struct Bubble { vec3 p, v; float r, age, life, seed; };
    std::vector<Bubble> bubbles_;
    struct Seed { vec3 from, to, ctl; float age, life, seed; int slot; vec3 col; };
    std::vector<Seed> seeds_;
    std::vector<float> pinst_;

    // ---- The growth's camera: one continuous pull-back from a macro lens on a single polyp to orbit, its scale
    // (the distance to what it frames, D) and its place set by the lines it follows (camL_, a calm spring).
    float camL_ = -1.f, camLv_ = 0.f;
    vec3 anchor_{0, 0, 0};
    vec3 P0_{9.5f, 0.f, 4.f};          // where the first polyp lives (on its pebble)
    GLsizei groundRings_ = 0;
    float camD_ = 15.f;
    float surfY_ = reef::SURF_Y;        // the surface's plane
    float towerH_ = 0.f;                // the tower's height (its floor at -towerH_), with the lines
    mat4 refVP_;                        // the reef stage's framing (where the reef's slots are laid out)
    bool toScreenRef(const vec3& w, vec2* ndc) const;
    // The camera's place at a point of the growth (t: its breathing): the eye, what it frames, its forward axis.
    void poseAt(float L, float t, vec3* eye, vec3* anchor, vec3* fwd, vec3* right, vec3* up) const;
    std::vector<vec3> path_;            // the camera's path (eye, what it frames) while it pulls back over the reef

    // ---- The camera (world meters).
    vec3 eye_{0, 3, 15}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 view3_, proj3_, vp3_;
    void updateCamera();
    vec3 boardToWorld(vec2 b, float dist) const; // a board point (board units) seen at that distance in the world
    bool toScreen(const vec3& w, vec2* ndc) const;

    // ---- Board. Each block a chunk of living coral (its species the piece type's, see CELL_FS): its own seed (its
    // variation of the species' texture) and its colony's (the piece it came with: the blocks of one colony grow
    // into each other, a seam between two colonies), both kept through the falls and the clears.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    // nb: the sides it shares with blocks of its own colony (1 above, 2 below, 4 left, 8 right); place: its place in
    // its colony (column + 8 x row up from the colony's lowest, both 0..3: the colony's textures run on across it)
    void cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, int type = 0,
              float seed = 0.5f, float colony = 0.5f, int nb = 0, int place = 0);
    void buildBoard(const Game& g);
    float blockSeeds_[22][10] = {};          // the board's blocks' seeds (0: none yet)
    float colonies_[22][10] = {};       // their colonies' seeds
    int8_t seedType_[22][10] = {};      // the type the seeds were given for (+1; another type there: not ours)
    int8_t places_[22][10] = {};        // their places in their colonies
    float pieceSeeds_[4] = {0.13f, 0.37f, 0.61f, 0.89f}, pieceColony_ = 0.5f; // the falling piece's
    Rng blockRng_{9157};                // the blocks' seeds (apart from the world's draws)
    struct Fading { float x, y; int type; float t, seed, colony; int nb, place; };
    // a cleared block bursts into chunks of its coral, flung out through the water, slowing, sinking, fading
    struct Chunk { vec2 p, v; float half, age, life, seed, colony; int type; };
    std::vector<Chunk> chunks_;
    std::vector<Fading> fading_;
    struct Puff { vec2 p, v; float age, life, size; };
    std::vector<Puff> puffs_;
    struct SpriteInst { float a[4], b[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // the halo at .75 (most lines), at .85 (the celebrations, the dialogs)

    // ---- GL.
    struct MeshGL { GLuint vao = 0, vbo = 0, ibo = 0, inst = 0; GLsizei count = 0; };
    MeshGL coral_[reef::K_COUNT][reef::TEMPLATES];
    MeshGL rockM_[reef::TEMPLATES];
    MeshGL fishM_[reef::F_COUNT];
    MeshGL manta_;
    void upload(MeshGL& m, const reef::Mesh& mesh, int instFloats);
    GLuint progWater_ = 0, progGround_ = 0, progCoral_ = 0, progFish_ = 0, progGlow_ = 0, progComp_ = 0,
           progVeil_ = 0, progCell_ = 0, progSprite_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, groundVao_ = 0, groundVbo_ = 0, groundIbo_ = 0, glowVao_ = 0, glowVbo_ = 0,
           cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLsizei groundCount_ = 0;
    // the world multisampled (4x) at the full resolution (up to 1440 lines), resolved into worldT_ (alpha: the
    // distance); lowH_: the lines of the reduced resolution it had, the sizes in pixels kept from it
    ugl::Target worldT_, sceneT_;
    ugl::MsTarget msT_;
    int lowH_ = 720;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickK_ = 0.f, causticPhase_ = 0.f;
    float melody_ = 0.5f, energyS_ = 0.5f;
    float surge_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dive_ = 0.f, drift_ = 0.f; // the setup's dive down to the view, the teardown's drift back from it (0..1)
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

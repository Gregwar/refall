#pragma once
// CORAL REEF: a sunny shallow reef that grows behind the well with the cleared lines (see coralreef.cpp).
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
    struct Rock { vec3 p; float size, yaw, seed; int tmpl; };
    std::vector<Rock> rocks_;           // boulders on the outcrops and in the sand (there from the start)
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
        int school;     // -1: a clownfish
        int home;       // clownfish: its anemone slot
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
    float swirl_ = 0.f, swirlT_ = -1.f, swirlSide_ = 1.f; // a Tetris: the schools swirl together for a while

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

    // ---- The camera (world meters).
    vec3 eye_{0, 3, 15}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 view3_, proj3_, vp3_;
    void updateCamera();
    vec3 boardToWorld(vec2 b, float dist) const; // a board point (board units) seen at that distance in the world
    bool toScreen(const vec3& w, vec2* ndc) const;

    // ---- Board.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type; float t; };
    std::vector<Fading> fading_;
    struct Puff { vec2 p, v; float age, life, size; };
    std::vector<Puff> puffs_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font sans_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    struct MeshGL { GLuint vao = 0, vbo = 0, ibo = 0, inst = 0; GLsizei count = 0; };
    MeshGL coral_[reef::K_COUNT][reef::TEMPLATES];
    MeshGL rockM_[reef::TEMPLATES];
    MeshGL fishM_[reef::F_COUNT];
    MeshGL manta_;
    void upload(MeshGL& m, const reef::Mesh& mesh, int instFloats);
    GLuint progWater_ = 0, progGround_ = 0, progCoral_ = 0, progFish_ = 0, progGlow_ = 0, progComp_ = 0,
           progVeil_ = 0, progCell_ = 0, progSprite_ = 0, progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, groundVao_ = 0, groundVbo_ = 0, groundIbo_ = 0, glowVao_ = 0, glowVbo_ = 0,
           cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLsizei groundCount_ = 0;
    ugl::Target worldT_, sceneT_;
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
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

#pragma once
// SKY WHALE: a koi that grows with every cleared line into a whale swimming up out of the sea, through the clouds
// and around the Earth (see skywhale.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SkyWhaleUniverse : public Universe {
public:
    ~SkyWhaleUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override;
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

    // A variant's colors (linear RGB) and light (floats only: palettes are blended float by float).
    struct Palette {
        vec3 zenith, horizon, space, nebula, nebula2, sun, sunGlow, lit, shadow, haze, rim;
        vec3 groundA, groundB, ocean, land, desert, flower, sand, pad, lotus, water, deep, tree, treeShade;
        vec3 koi, koiPatch, koiPatch2, back, belly, fin, glow, eye, aurora, aurora2, lantern;
        vec3 cloudLit, cloudShade, ink, accent, veil;
        vec3 piece[7];
        float sunAz, sunEl, sunSize, night, stars, patch2, auroraSky, bloom, glowK;
    };

    // Where the creature is: its center, its axes (x forward, y up, z its right side), its length (meters).
    struct Pose { vec3 c, fx, fy, fz; float L, bend; };

private:
    // ---- The growth: G = ln(the creature's length in meters), from a 30 cm koi to a million kilometers.
    float growthOf(int lines) const;
    int lines_ = 0, linesShown_ = -1, here_ = 0;
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f;
    float gBonus_ = 0.f;
    float stroke_ = 0.f;          // a clear's tail stroke (the swim surging)
    float burst_ = 0.f;           // a Tetris' burst
    float hatchT_ = 99.f;         // a new game: a new koi
    float swimPh_ = 0.f, finPh_ = 0.f, singPh_ = 0.f;
    float breachPh_ = 0.f;        // the breaching cycle at sea (0..1)
    float breachKick_ = 0.f;
    uint64_t koiSeed_ = 1;
    float koiSeedF_ = 0.3f;
    Pose pose_{};
    Pose poseAt(float G, float t) const; // the path (from the growth) and the swim's own motion (t: the world clock)
    vec3 pathPos(float G) const;        // the path's center at G
    vec3 lastHead_{0.f}, lastTail_{0.f};
    vec3 splashC_{0.f};
    float splashAge_ = 99.f;
    float sink_ = 0.f;

    // ---- The camera (world: meters, the pond at the origin, y up; drawn relative to the eye).
    vec3 eye_{0, 1, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0}, camUp_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 proj3_, vp3_;
    float gc_ = -99.f, gcVel_ = 0.f;
    float alt_ = 1.f;
    vec3 camAnchor_{0.f};
    bool camInit_ = false;
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const;

    // ---- Particles in the world: spray, sparkles, petals, streaks, rings of song.
    struct Spark { vec3 p, v; float age, life, size, seed, spin; int kind; vec3 col; };
    std::vector<Spark> sparks_;
    float sparkAcc_ = 0.f, streakAcc_ = 0.f;
    struct Ring { vec3 c, n; float age, life, r0, r1, hue; };
    std::vector<Ring> rings_;
    double lastRingBeat_ = -1e9;
    float ringClock_ = 0.f;
    std::vector<float> ginst_;
    std::vector<float> ribbon_;
    // ---- Clouds.
    struct Puff { vec3 p; float r, seed; };
    std::vector<Puff> clouds_;
    std::vector<float> cinst_;
    // ---- The banks: trees and bushes (toon puffs), reeds, grass.
    std::vector<float> trees_;
    struct Tuft { vec3 p; float h, seed; int kind; };
    std::vector<Tuft> tufts_;
    void buildWorld();
    // ---- Props: the lighthouse, stone lanterns (meshes turned on the CPU).
    struct PropMesh { GLuint vao = 0, vbo = 0; GLsizei count = 0; };
    PropMesh lighthouse_, toro_;
    void buildProps();

    // ---- The creature's mesh (a grid per part, placed by the shaders).
    GLuint whaleVao_ = 0, whaleVbo_ = 0, whaleIbo_ = 0;
    GLsizei whaleCount_ = 0;
    void buildWhale();

    // ---- 2D motes over the board (the cleared rows flowing to the creature).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 3.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 0 drifting, 1 flying along a curve to the creature, 3 a hard drop's bubbles
        vec2 from{0, 0}, ctl{0, 0}, to{0, 0};
    };
    std::vector<Mote> motes_;
    void addMote(const Mote& m);

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase, float shine);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t; };
    std::vector<Fading> fading_, overCells_;
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
    Font caps_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progSky_ = 0, progWhale_ = 0, progProp_ = 0, progPuff_ = 0, progGlow_ = 0, progRibbon_ = 0, progCloud_ = 0,
           progSea_ = 0, progComp_ = 0, progBright_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0,
           progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, puffVao_ = 0, puffVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0,
           ribbonVao_ = 0, ribbonVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target worldT_, bloomA_, bloomB_, sceneT_;
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
    float dropAmt_ = 0.f, swell_ = 0.f, sing_ = 0.f;
    float kickIn_ = 0.f;
    float levelGlow_ = 0.f;
    float beatClock_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{9917};
};

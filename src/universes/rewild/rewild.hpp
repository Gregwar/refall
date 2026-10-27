#pragma once
// REWILD: glass cubes of seeds over a parched land that grows into a jungle, a 3D diorama (see rewild.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RewildUniverse : public Universe {
public:
    ~RewildUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override { refresh(1.f); }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

    // A variant's colors, linear RGB once loaded (floats only: palettes blend).
    struct Palette {
        vec3 skyTop, skyLow, sun, cloud, dust, mist, stormSky, stormCloud;
        vec3 soilDry, soilWet, soilCut, crack, grassDry, grassLush, leafA, leafB, leafFar, bark, dead;
        vec3 flowerA, flowerB, bird, panel, ink, accent, dusk;
        vec3 piece[7];
        float sunAz, sunEl, sunR, moon, stars, night, lightK, ambK, shadowK, panelA, clouds;
    };

    // The plants' kinds (their meshes).
    enum { K_GRASS = 0, K_FLOWER, K_FERN, K_BUSH, K_TREE, K_EMERGENT, K_PALM, K_DEAD, K_COUNT };

private:
    // ---- The stack, ghost, falling piece and previews: glass cubes of seeds on the board plane, over the world.
    struct CellInst { float a0[4], a1[4], a2[4], a3[4]; };
    std::vector<CellInst> cells_;
    void cube(float x, float y, float size, int type, float alpha, bool ghost, float seed, float crack = 0.f);
    void flushCells();

    // ---- Seeds flying out of a broken cube on their arc into the land, glass shards, dust, rain splashes, pollen.
    enum { P_SEED = 0, P_SHARD, P_DUST, P_SPLASH, P_MOTE, P_PETAL };
    struct Particle {
        vec3 p, v;
        float age, life, size, grav, drag, seed, spin;
        int kind, type;
        vec3 col;
        bool real;   // ages at the real speed, even in slow motion
    };
    std::vector<Particle> parts_;
    void breakCube(const vec3& p, int type, float power, float seeds);
    void land(const Particle& s);
    void dustPuff(const vec3& p, int n, float power);
    void simulateParticles(float dt, float wdt);
    // A game over: the stack cracks row by row from the top, each cube then breaks open like a cleared one.
    struct Dying { float x, y, delay, t; int type; bool gone; };
    std::vector<Dying> dying_;
    // A lock: a small puff of dust pushed out from under the piece (board plane, drawn after the stack).
    struct Puff { vec2 p, v; float age, life, size; };
    std::vector<Puff> puffs_;
    int boardCount_ = 0;
    void watchBoard(const Game& game);

    // ---- The plants: each planted seed grows (slowly: grass in seconds, trees over minutes), sways, blooms.
    struct Plant {
        vec3 p;
        int kind;
        float g, rate, scale, rot, seed, tint, vines, wither, cov;
        bool wild;   // grown on its own far out (the forest spreading), not from a seed
    };
    std::vector<Plant> plants_;
    int plantCount_[K_COUNT] = {};
    void plant(const vec3& p, int kind, float g0, bool wild, int type);
    void growPlants(float wdt);
    void spreadWild(float wdt, bool instant);
    int pickKind(float x, float z, int type);
    void seedInstant(float lines);   // a land already grown by `lines` cleared lines (a stack joined in progress)
    void plantStumps();              // the dead trees of the parched land

    // ---- The life map: grass cover, damp soil and flowers around what grows (a texture over the land).
    std::vector<float> lifeG_, lifeM_, lifeF_;
    std::vector<unsigned char> lifeTex_;
    bool lifeDirty_ = true;
    float lifeUpload_ = 0.f;
    void stamp(float x, float z, float radius, float g, float m, float f);

    // ---- Progress: how alive the land is (the lines cleared this song), the rain and the wet soil.
    float lines_ = 0.f;       // cleared lines (Tetris lines count 1.5)
    float life_ = 0.f;        // 0 parched .. 1 jungle, shown (approaches the target)
    float lifeTarget() const { return std::min(1.f, lines_ / 60.f); }
    float rainLin_ = 0.f, rain_ = 0.f, wet_ = 0.f, flash_ = 0.f, flashAz_ = 20.f;
    float surge_ = 0.f;       // the drop's growth surge (decays slowly: what it grew stays)
    float bloom_ = 0.f;       // flowers open (peaks, drops, Tetris)
    float wind_ = 0.1f, gust_ = 0.f, swayT_ = 0.f, cloudT_ = 0.f;
    float dropPending_ = 0.f;
    bool rainWasOn_ = false;
    bool sectionDrop_ = false, sectionKnown_ = false;
    float wildAcc_ = 0.f;

    // ---- Birds (flying around and between the trees), butterflies, fireflies.
    struct Bird {
        vec3 c;              // the center it circles
        float r, ang, w, h, phase, size, dy, alpha, life, flap;
        int kind;            // 0 bird, 1 colorful bird, 2 butterfly, 3 firefly
        vec3 col, p, v;
    };
    std::vector<Bird> birds_;
    float birdT_ = 0.f;
    void spawnBird(int kind, bool onScreen, bool flock);
    void simulateBirds(float dt, float wdt, float world);

    void simulate(const Game& game, const MusicState& music, float dt, float wdt);

    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    int pickVariant(const SongLook& look) const;
    float bgLuma(float bx, float by) const;
    float hwAt(float z) const;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr; // stbtt_fontinfo
    };
    Font fontItalic_, fontRoman_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    float textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float bias);

    // ---- The camera: the layout's reference lens raised above the land, its image shifted back down (a shift
    // lens), so the board plane falls on the very same pixels while the land behind it is seen from above.
    void setCamera();
    vec3 sunDir() const;
    mat4 view_, vp_, invVP_, lightVP_;
    vec3 eye_{0.f, 5.2f, 34.6f}, camR_{1.f, 0.f, 0.f}, camU_{0.f, 1.f, 0.f};
    float horizon_ = 0.3f;
    float camDist_ = 34.6f;

    // ---- GL.
    struct Mesh { GLuint vao = 0, vbo = 0, ibo = 0; int count = 0; };
    void buildMeshes();
    void worldUniforms(GLuint prog);
    void drawPlants(GLuint prog, bool shadow);
    GLuint progSky_ = 0, progGround_ = 0, progPlant_ = 0, progDepth_ = 0, progBird_ = 0, progPart_ = 0, progRain_ = 0,
           progVeil_ = 0, progCell_ = 0, progSprite_ = 0, progText_ = 0, progShape_ = 0, progPost_ = 0;
    Mesh ground_;
    // the plants' meshes: one per kind, near (detailed) and far (simple)
    GLuint plantVao_ = 0, plantVbo_ = 0, plantInstVbo_ = 0;
    int meshFirst_[K_COUNT][2] = {}, meshCount_[K_COUNT][2] = {};
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, birdVao_ = 0, birdVbo_ = 0,
           partVao_ = 0, partVbo_ = 0, textVao_ = 0, textVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint lifeTexId_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0, shadowFbo_ = 0, shadowTex_ = 0;
    static constexpr int SHADOW_RES = 2048;
    int samples_ = 4;
    ugl::Target sceneT_, bgT_;
    vec2 viewC_{0, 0.4f}, viewH_{22, 12.4f};
    float px_ = 0.05f;

    // ---- Song, variant, palette.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{};
    uint64_t seed_ = 0;
    bool started_ = false;

    // ---- Scene level: continuous 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;

    // ---- Music.
    float time_ = 0.f, worldTime_ = 0.f;
    double lastBar_ = -1.0;
    float prevKick_ = 0.f, kickIn_ = 0.f, sinceKick_ = 9.f;
    float refreshT_ = 0.f;

    // ---- Game state seen by the universe.
    bool slowOn_ = false;
    float slow_ = 0.f;
    bool over_ = false;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{7117};
};

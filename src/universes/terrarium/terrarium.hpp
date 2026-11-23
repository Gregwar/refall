#pragma once
// TERRARIUM: a sealed specimen jar on a lab bench; over the song its life cracks the glass, escapes, swallows the lab
// and then the city around it (see terrarium.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/terrarium/world.hpp"

class TerrariumUniverse : public Universe {
public:
    ~TerrariumUniverse() override;
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

    // A variant's colors (linear RGB): floats only, palettes are blended float by float.
    struct Palette {
        vec3 soil, soil2, gravel, strata1, strata2, moss, moss2, leaf, leaf2, stem, cap, cap2, flower, flower2, bark,
            water, stone, bug, bugSpot, fly, sill, frame, wall, neonA, neonB;
        vec3 ink, accent, veil;
        vec3 piece[7];
    };

    // The things growing in the jar: a slot each, born at a line of the scene (on the 50-line scale).
    enum SlotType { S_MOSS = 0, S_FERN, S_MUSH, S_TWIG, S_FLOWER, S_ROSETTE, S_CACTUS, S_PEBBLE, S_ROCK, S_COUNT };
    struct Slot {
        int type;
        float x, z, size, rot, birth, param;
    };

private:
    // ---- Growth: paceLines() (50 at scene completion) -> each slot's growth, eased.
    float lines_ = 0.f, linesShown_ = -1.f, linesEased_ = 0.f;
    std::vector<float> growth_;          // per slot, 0..1 (eased)
    float pond_ = 0.f, tod_ = 0.f, moss_ = 0.f;
    // ---- The story: the crack, the hole, the escape (the bench's clock, ahead of the lines after a burst), the
    // room's and the city's clock, the drop's surge
    float shiftA_ = 0.f, shiftTarget_ = 0.f, surgeAcc_ = 0.f, surge_ = 0.f;
    float linesA_ = 0.f, linesB_ = 0.f, crack_ = 0.f, hole_ = 0.f, burstT_ = -1.f;
    float labOn_ = 1.f;
    int stage_ = 0;

    // ---- The camera (world: the jar's radius is 1, its foot at the origin, y up).
    vec3 eye_{0, 1, 4}, fwd_{0, 0, -1}, right_{1, 0, 0}, up_{0, 1, 0};
    float tanH_ = 0.25f, aspect_ = 1.78f, near_ = 0.05f, far_ = 80.f;
    void updateCamera(float lines = 0.f);
    bool toScreen(const vec3& w, vec2& board) const; // a world point -> board units (false: behind the eye)
    vec3 boardRay(vec2 b) const;                     // the view ray through a board point

    // ---- Life: fireflies, the beetle, seeds drifting down and the sprouts they leave, spores and pollen.
    struct Fly { vec3 home, p; float phase, freq, seed, glow, on; };
    std::vector<Fly> flies_;
    struct Seed { vec3 p, v, to; float age, life, seed; bool landed; int target; }; // target 0: the jar's soil, 1: the bench
    std::vector<Seed> seeds_;
    struct Sprout { float x, z, age, seed; bool bench; };
    std::vector<Sprout> sprouts_;
    void addSeed(const vec3& from);
    float inVine_ = 0.f;                  // vines up the inside of the glass
    struct Spore { vec3 p, v; float age, life, size, seed; };
    std::vector<Spore> spores_;
    void sporeBurst(int n, float speed);
    bool pickBenchSpot(vec3& at);
    // ---- the glass's shards: falling out of the hole, flying at a burst, lying on the bench
    struct Shard { vec3 p, v, ax; float ang, spin, size, seed; bool rest; };
    std::vector<Shard> shards_;
    int shardsOut_ = 0;
    void spawnShards(int n, float speed, bool landed);
    // ---- the jar as it is drawn this frame (see computeJar), its meshes (rebuilt only when they changed)
    struct JarState {
        float level = 0.f, fill = 0.f, moss = 0.f, vine = 0.f;
        int kind = 0;
        float pal[17 * 3] = {};
        terra::Pond pd;
        std::vector<terra::SlotDraw> slots;
    };
    struct Mesh { GLuint vao = 0, vbo = 0; int count = 0; };
    struct JarMesh {
        Mesh mesh, soilMesh;                   // the pond; the soil
        std::vector<Mesh> slot;                // each slot's plant
        std::vector<terra::SlotDraw> built;    // what each slot's mesh was built for
        std::vector<float> builtPond;          // (the trickles: the pond's level, filled or not)
        float soilDepth = -1.f, pondLevel = -9.f;
        int soilKind = -1;
        bool hasPond = false, dirty = true;
    };
    JarState jar_;
    JarMesh jarMesh_;
    void computeJar(const std::vector<Slot>& slots, float after);
    void updateJarMesh();
    void upload(Mesh& m, const std::vector<terra::Vtx>& v, bool dynamic);
    // the beetle's excursion out through the hole: at home it sometimes walks to the crack, out over its lip, hops
    // down onto the bench, explores, looks at us and comes back
    int bugMode_ = 0, exPhase_ = 0;
    float exT_ = 0.f, bugCool_ = 12.f;
    bool bugGoing_ = false;
    vec3 bugP_{0, 0, 0}, exFrom_{0, 0, 0};
    float bugHead_ = 0.f, bugSpeed_ = 0.f, bugLook_ = 0.f;
    void updateExcursion(float dt);
    bool pickSpot(float& x, float& z, float clearance);
    float soilHeight(float x, float z) const; // the same function as the shader's
    bool blocked(float x, float z, float r) const;
    struct Beetle {
        float x = -0.55f, z = 0.55f, head = 0.4f, speed = 0.f, walk = 0.f, look = 0.f, lookT = 0.f, pause = 2.f;
        float tx = -0.4f, tz = 0.6f, blink = 0.f, blinkT = 3.f, bob = 0.f;
    } bug_;
    void updateBeetle(float dt, float wm, float wp);

    // ---- Particles over the board: a hard drop's puff.
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, alpha = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;
    };
    std::vector<Mote> motes_;

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float seed);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type; float t; };
    std::vector<Fading> fading_, overCells_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    std::string stageName_;
    float fade_ = 1.f; // the frame's fade in, the HUD's own (it was lettered under the post's fade and dim)
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progLab_ = 0, progSky_ = 0, progContent_ = 0, progShadow_ = 0, progGlass_ = 0, progTube_ = 0, progGlow_ = 0, progComp_ = 0;
    GLuint progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint glowVao_ = 0, glowVbo_ = 0;
    size_t worldLab_ = 0, worldGrowth_ = 0, worldBench_ = 0; // (the world mesh's parts: the lab, the bench's growth)
    Mesh world_, labGlass_, glassOut_, glassIn_, cork_, sproutMesh_, beetle_, shardMesh_;
    std::vector<terra::NeonPt> neon_;
    std::vector<terra::CityLight> lights_;
    std::vector<terra::Bloom> blooms_;
    float shadowSoft_ = 0.001f;
    std::vector<float> sproutKey_;
    // the world: multisampled at the full resolution (up to 1440 lines), resolved twice (the glass reads the world
    // behind it), with its depth; the key light's shadow map
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0, worldDepth_ = 0, backDepth_ = 0, shadowTex_ = 0, shadowFbo_ = 0;
    int msW_ = 0, msH_ = 0, samples_ = 4;
    ugl::Target worldT_, backT_, sceneT_;
    mat4 vp_, vp3_, lightVp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;
    int tagNo_ = 7;
    const std::vector<Slot>& slots() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f, flicker_ = 0.f, allLit_ = 0.f;
    float fog_ = 0.5f, cond_ = 0.5f;
    float kickIn_ = 0.f, kickS_ = 0.f;
    double ownBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{8128};
};

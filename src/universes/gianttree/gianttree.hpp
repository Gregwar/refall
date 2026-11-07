#pragma once
// GIANT TREE: a 3D world tree growing behind the well with the cleared lines (see gianttree.cpp).
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The tree, generated once per game from a seed (tree.cpp): every part of it carries the growth at which it is
// born, and the shaders reveal it against the growth uniform (nothing is rebuilt while it grows).
struct GiantTreeMesh {
    // Bark: 20 floats a vertex (see BARK_VS): ring center + radius, radial normal + place along the branch, branch
    // base + level, birth + duration + distance from the root + sway phase, bark coordinates + occlusion.
    std::vector<float> bark;
    std::vector<uint32_t> index;
    // Leaf clusters: 16 floats each (position + size, clump normal + birth, death + root distance + seed + kind,
    // sway weight + 3 spare).
    std::vector<float> leaves;
    // Lanterns: 8 floats each (position + size, birth + seed + root distance + spare).
    std::vector<float> lanterns;
    // Growth tables (TABLE samples over growth 0..1): the height of what is born, the foliage's center and radius.
    static constexpr int TABLE = 65;
    float height[TABLE] = {}, crownY[TABLE] = {}, crownR[TABLE] = {}, crownX[TABLE] = {}, crownZ[TABLE] = {};
    float maxPath = 1.f;   // the longest distance from the root (the drop's light flows up to it)
    float trunkR = 6.f;    // the trunk's radius at its foot (tree units)
    float sample(const float* t, float g) const;
};
GiantTreeMesh buildGiantTree(uint64_t seed, int style);

class GiantTreeUniverse : public Universe {
public:
    ~GiantTreeUniverse() override;
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

    // A variant's colors (linear RGB) and light (floats only: palettes are blended float by float).
    struct Palette {
        vec3 zenith, sky, horizon, haze, sun, sunGlow, ambient, ground, groundFar, mist;
        vec3 bark, barkDark, leaf, leaf2, blossom, glow, lantern, cloudLit, cloudShade, petal;
        vec3 ink, accent, veil;
        vec3 piece[7];
        float sunAz, sunEl, sunSize, night, stars, nebula, leafGlow, blossomAmt, bloom, fog;
    };

private:
    // ---- The tree.
    GiantTreeMesh tree_;
    uint64_t treeSeed_ = 0;
    int treeStyle_ = -1;
    bool treeDirty_ = true;
    void uploadTree();
    float growthOf(int lines) const;  // lines -> growth target
    float scaleOf(float g) const;     // growth -> meters per tree unit
    int lines_ = 0, linesShown_ = -1;
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f;   // the growth shown chases its target (a spurt)
    float spurt_ = 0.f, bloomBurst_ = 0.f;          // a clear's spurt; a Tetris' blossom burst
    float shed_ = 0.f;                               // game over: the leaves fall (0..1+)
    float seedT_ = 99.f;                             // a new game: a new seed sprouts

    // ---- The camera (world: meters, the tree's foot at the origin, y up).
    vec3 eye_{0, 1, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.4f, aspect_ = 1.78f;
    mat4 view3_, proj3_, vp3_;
    float camH_ = -1.f;    // the framed height (smoothed)
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const; // a world point on the board plane (for the motes)
    float groundAt(float x, float z) const;

    // ---- Particles in the world: petals, spores, the drop's shower.
    struct Petal { vec3 p, v; float age, life, size, seed, spin; int kind; };
    std::vector<Petal> petals_;
    float petalAcc_ = 0.f, sporeAcc_ = 0.f;
    std::vector<float> pinst_;
    // ---- Clouds: soft puffs in a layer the tree grows through.
    struct Puff { vec3 p; float r, seed; };
    std::vector<Puff> puffs_;
    std::vector<float> cinst_;
    // ---- Grass tufts and wildflowers around the sapling (meters).
    struct Tuft { vec3 p; float h, seed, flower; };
    std::vector<Tuft> tufts_;

    // ---- 2D motes over the board (the cleared rows flowing to the tree).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 3.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 0 drifting, 1 flying along a curve to the tree, 3 a hard drop's puff
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
    GLuint progSky_ = 0, progGround_ = 0, progBark_ = 0, progLeaf_ = 0, progGlow_ = 0, progCloud_ = 0, progComp_ = 0,
           progBright_ = 0, progSea_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, barkVao_ = 0, barkVbo_ = 0, barkIbo_ = 0, leafVao_ = 0, leafVbo_ = 0, lanternVao_ = 0, lanternVbo_ = 0,
           glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0, groundVao_ = 0, groundVbo_ = 0, groundIbo_ = 0,
           spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLsizei barkCount_ = 0, leafCount_ = 0, lanternCount_ = 0, groundCount_ = 0;
    ugl::Target worldT_, bloomA_, bloomB_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;
    void buildGround();

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
    float flowT_ = 99.f, flowK_ = 0.f, flowSpeed_ = 1.f; // light flowing up from the root (drop, Tetris)
    float gust_ = 0.f;
    float kickIn_ = 0.f, kickGlow_ = 0.f;
    float windPhase_ = 0.f;
    float tetrisGlow_ = 0.f;
    double lastBeat_ = -1.0;
    float beatClock_ = 0.f;
    int beatCount_ = 0;
    float shimmer_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{9917};
};

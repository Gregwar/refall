#pragma once
// GIANT TREE: a toon world tree shooting up absurdly with the cleared lines, from a meadow into space (see gianttree.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The tree's growth is G = ln(the trunk's height in meters): from a sapling of about a meter (G ~ 0.3) to a tree
// reaching past the moon (G ~ 21). Its shape is parametric (the same functions on the CPU and in the shaders, see
// TREE_GLSL): the trunk's radius and the crown's size follow G, so nothing is rebuilt while it grows.
namespace gtree {
constexpr float G_MAX = 21.6f;          // the tallest it gets (some 2.4 million km)
constexpr float R_EARTH = 6.371e6f;     // the planet it grows on (meters)
// The crown's scale (meters): wide on the sapling (a round head), ever narrower relative to the soaring trunk.
inline float crownW(float G) { return std::exp(G) * (0.05f + 1.1f * std::exp(-G / 3.f)); }
// The trunk's axis at height y (meters): a gentle lean winding in log space (wob: amplitude, frequency, phases).
inline vec3 trunkCenter(float y, const float* wob) {
    const float l = std::log(std::max(y, 0.f) + 0.05f);
    return vec3(y * wob[0] * std::sin(wob[1] * l + wob[2]), y, y * wob[0] * std::cos(wob[1] * l * 0.8f + wob[3]));
}
inline vec3 tip(float G, const float* wob) { return trunkCenter(std::exp(G), wob); }
} // namespace gtree

// The tree, generated once per game from a seed (tree.cpp): every part of it carries the growth at which it is
// born, and the shaders reveal it against the growth uniform.
struct GiantTreeMesh {
    // Bark: 20 floats a vertex (see BARK_VS): center + radius, radial normal + place along the branch, branch base +
    // space (0 the trunk, 1 the crown, 2 the world), birth + duration + path + sway phase, bark u + v + seed + death.
    std::vector<float> bark;
    std::vector<uint32_t> index;
    // Puffs of foliage (and fruits, vine leaves): 16 floats each (see LEAF_VS).
    std::vector<float> puffs;
    float wob[4] = {0.04f, 0.8f, 0.f, 0.f};
    vec3 crownC{0.f, 0.1f, 0.f};   // the crown's center (crown space: unit = crownW, origin at the trunk's tip)
    float crownR = 0.6f;           // and its radius
};
GiantTreeMesh buildGiantTree(uint64_t seed);

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
        vec3 zenith, horizon, space, nebula, nebula2, sun, sunGlow, lit, shadow, haze, rim;
        vec3 groundA, groundB, ocean, land, desert, flower;
        vec3 bark, barkDark, leaf, leaf2, leafShade, blossom, fruit, glow, cloudLit, cloudShade, petal;
        vec3 ink, accent, veil;
        vec3 piece[7];
        float sunAz, sunEl, sunSize, night, stars, leafGlow, blossomAmt, bloom;
    };

private:
    // ---- The tree: its growth G = ln(height in meters) (see gtree::).
    GiantTreeMesh tree_;
    uint64_t treeSeed_ = 0;
    bool treeDirty_ = true;
    void uploadTree();
    float growthOf(int lines) const;  // lines -> G
    int lines_ = 0, linesShown_ = -1;
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f;   // the growth shown chases its target (a spurt)
    float gBonus_ = 0.f;                             // the drops' extra growth
    float spurt_ = 0.f, bloomBurst_ = 0.f;          // a clear's spurt; a Tetris' burst
    float jelly_ = 0.f, jellyV_ = 0.f;              // the crown's squash and stretch
    float shed_ = 0.f;                               // game over: the leaves fall (0..1+)
    float seedT_ = 99.f;                             // a new game: a new seed sprouts

    // ---- The camera (world: meters, the tree's foot at the origin, y up; drawn relative to the eye).
    vec3 eye_{0, 1, 10}, look_{0, 0, -1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    float tanH_ = 0.5f, aspect_ = 1.78f;
    mat4 proj3_, vp3_;
    float gc_ = -99.f, gcVel_ = 0.f;   // the growth the camera frames (lagging behind: the tree shoots up first)
    float alt_ = 1.f;
    void updateCamera(float dt);
    vec2 toBoard(const vec3& world) const; // a world point on the board plane (for the motes)
    vec3 crownCenter(float G) const;       // meters
    float crownRadius(float G) const;      // meters

    // ---- Particles in the world: petals, sparkles, streaks.
    struct Petal { vec3 p, v; float age, life, size, seed, spin; int kind; vec3 col; };
    std::vector<Petal> petals_;
    float petalAcc_ = 0.f, sporeAcc_ = 0.f, streakAcc_ = 0.f;
    std::vector<float> pinst_;
    // ---- Clouds: toon cumulus in a layer the tree grows through.
    struct Puff { vec3 p; float r, seed; };
    std::vector<Puff> puffs_;
    std::vector<float> cinst_;
    // ---- Grass tufts and wildflowers around the sapling (meters).
    struct Tuft { vec3 p; float h, seed, flower; };
    std::vector<Tuft> tufts_;
    void buildMeadow();

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
    GLuint progSky_ = 0, progBark_ = 0, progLeaf_ = 0, progGlow_ = 0, progCloud_ = 0, progComp_ = 0,
           progBright_ = 0, progSea_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, barkVao_ = 0, barkVbo_ = 0, barkIbo_ = 0, leafVao_ = 0, leafVbo_ = 0,
           glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0,
           spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLsizei barkCount_ = 0, leafCount_ = 0;
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
    float dropAmt_ = 0.f, swell_ = 0.f;
    float flowT_ = 99.f, flowK_ = 0.f, flowDur_ = 2.f; // light flowing up from the root (drop, Tetris)
    float beam_ = 0.f;                                    // the drop's light rushing up into space
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

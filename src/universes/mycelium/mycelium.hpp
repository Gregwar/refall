#pragma once
// MYCELIUM: a glowing fungal network spreading through a cross-section of the forest floor at night, from a single
// spore to mushroom towers taller than the trees (see mycelium.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

// The world is in meters: x to the right, y up (the forest floor's surface at y = myc::surf(x), the soil under it),
// z into the screen (the soil is cut along the plane z = 0, the forest floor and its trees stand behind it).
// The growth is G = ln(the network's reach in meters): a spore of a millimeter at the start, a kilometer of threads
// at the end; everything (the network, the mushrooms, the camera) is placed against it.
namespace myc {
constexpr float SPORE_Y = -0.0007f;     // the spore, just under the surface (meters)
constexpr float DEPTH_K = 0.42f;        // the network's reach downward, relative to its reach sideways
constexpr float G_MAX = 7.6f;           // the most it spreads (some 2 km)
// The surface of the forest floor along the cut (the same sum of waves as SURF_GLSL), 0 at the spore.
inline float surf(float x) {
    float s = 0.f, s0 = 0.f, l = 0.01f;
    for (int i = 0; i < 9; i++) {
        s += 0.012f * l * std::sin(x / l + i * 1.7f);
        s0 += 0.012f * l * std::sin(i * 1.7f);
        l *= 3.f;
    }
    return s - s0;
}
// Lines -> G (a table in log space, then slower and slower).
float growthOf(int lines);
// The framing for a growth: the window on the cut (center, half height, meters) and the eye (an off-axis
// perspective: the cut plane maps to the window, what stands behind it converges toward the eye).
struct View { float cx, cy, vh, ex, ey, D; };
View frame(float G, float aspect, float boardHalfH);
} // namespace myc

// The network and the soil's roots, generated once per game from a seed (network.cpp).
struct MycNet {
    // Threads: 12 floats each: a.xy, b.xy | width (m), birth g0, g1, kind (0 cord, 1 hypha, 2 hair, 3 fusion) |
    // pulse coordinate q0, q1, lineage hash, thickness class.
    std::vector<float> seg;
    // Nodes: 8 floats: x, y, radius (m), birth g | q, hash, kind (0 branch, 1 fusion, 2 the spore), 0.
    std::vector<float> nodes;
    // Places where the threads reach the surface (where mushrooms can come up).
    struct Site { float x, g, q; };
    std::vector<Site> sites;
    // Roots in the cut: 8 floats: a.xy, b.xy | radius a, radius b, shade, class.
    std::vector<float> roots;
    // A plant on the cut's edge above each cluster of roots.
    struct Plant { float x, s, seed; int cls; };
    std::vector<Plant> plants;
};
MycNet buildMycNet(uint64_t seed);

class MyceliumUniverse : public Universe {
public:
    ~MyceliumUniverse() override;
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

    // A variant's colors (linear RGB), floats only (palettes are blended float by float).
    struct Palette {
        vec3 skyTop, skyLow, mist, moon, floorC, floorLit, soilA, soilB, soilC, soilD, rock, rootC, ambient;
        vec3 thread, threadTip, pulse, cap[3], gill[3], stem, foliage, bark, speck, ink, accent, veil;
        vec3 piece[7];
        float glow, soilLight, mistK, stars;
    };

private:
    // ---- The network.
    MycNet net_;
    uint64_t netSeed_ = 0;
    bool netDirty_ = true;
    void uploadNet();
    int lines_ = 0, linesShown_ = -1;
    float gTarget_ = 0.f, g_ = 0.f, gVel_ = 0.f, gBonus_ = 0.f;
    float gc_ = -99.f, gcVel_ = 0.f;   // the growth the camera frames (lagging behind the spurt)
    float spurt_ = 0.f;

    // ---- Mushrooms: chosen from the network's sites once per game, each bigger than the last.
    struct Mushroom {
        float x, z, h0, g, q, seed, lean;
        int type, color;
        bool born = false;
        float age = 0.f, flareT = 99.f, flare = 0.f, flareK = 0.f, beat = 0.f, beatK = 0.f;
        bool companion = false;
    };
    std::vector<Mushroom> shrooms_;
    void planMushrooms();
    float shroomHeight(const Mushroom& m) const;

    // ---- The camera.
    myc::View view_{0, 0, 0.004f, 0, 0, 0.01f};
    float aspect_ = 1.78f;
    float pxW_ = 1e-5f;   // window meters per pixel of the world target
    void updateCamera(float dt);
    vec2 worldToBoard(float x, float y, float z = 0.f) const;

    // ---- Pulses along the threads (q: the pulse coordinate, ~ ln of the distance along the threads).
    struct Pulse { float q, speed, amp, frac, seed; };
    std::vector<Pulse> pulses_;
    float waveQ_ = -99.f, waveSpeed_ = 1.f, waveAmp_ = 0.f, waveT_ = 0.f;   // the drop's wave
    double lastBeat_ = -1.0;
    int beatCount_ = 0;
    float beatClock_ = 0.f;

    // ---- Billboards (plants, trees, rocks, mushrooms), built every frame for the view, far to near.
    struct Board { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<Board> boards_;
    std::vector<std::pair<float, int>> order_;
    std::vector<Board> sorted_;
    void buildFlora();
    void buildShroomBoards();

    // ---- Particles in the world: spores floating up, the caps' halos in the mist.
    struct Spore { vec3 p, v; float age, life, size, seed; vec3 col; int kind; };
    std::vector<Spore> spores_;
    std::vector<float> ginst_;
    float sporeAcc_ = 0.f;

    // ---- 2D motes over the board (the cleared rows flying down to the network).
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;   // 1 flying along a curve, 3 a hard drop's puff
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
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase);
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
    Font caps_, capsB_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progBg_ = 0, progThread_ = 0, progNode_ = 0, progRoot_ = 0, progBoard_ = 0, progGlow_ = 0, progComp_ = 0,
           progBright_ = 0, progBlur_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, threadVao_ = 0, threadVbo_ = 0, nodeVao_ = 0, nodeVbo_ = 0, rootVao_ = 0, rootVbo_ = 0,
           boardVao_ = 0, boardVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0,
           cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLsizei threadCount_ = 0, nodeCount_ = 0, rootCount_ = 0;
    ugl::Target worldT_, lightA_, lightB_, bloomA_, bloomB_, sceneT_;
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
    float kickIn_ = 0.f, kickGlow_ = 0.f;
    float tetrisGlow_ = 0.f;
    float windPhase_ = 0.f, gust_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4471};
};

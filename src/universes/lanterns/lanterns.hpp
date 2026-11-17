#pragma once
// LANTERNS: paper lanterns floating on a dark lake, the pieces lit from inside, the well a frame of light (see
// lanterns.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class LanternsUniverse : public Universe {
public:
    ~LanternsUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

private:
    // ---- Boxes (world units, the board's cells on z = 0, the water at y = WATER): pieces, lanterns, the frames of
    // light (the well's, mirrored in the water; the hold box's), the well's glass and its grid.
    // center, mask | size, kind | paper, alpha | light, cap, seed, flame | candle (round for a lantern), r
    struct BoxInst { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<BoxInst> stack_, lamps_, frame_, holdFrame_, glass_, ghost_;
    int ids_[22][10] = {};
    float flare_[22][10] = {};    // a piece's candle catching (lock, kick)
    float flame_[22][10] = {};    // the small flames standing in some cells (0: none, else its flicker's phase)
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct OverCell { float x, y; int row, type, mask; float cx, cy, r, seed, flame; }; // the stack at game over
    std::vector<OverCell> overCells_;

    // ---- The lanterns: the runs of a cleared row leaving the well and landing on the water, the lake's lanterns,
    // the sky lanterns of the drop; the far lights; embers.
    enum LanternState { L_LEAVE, L_DESCEND, L_TRAVEL, L_FLOAT, L_RISE };
    struct Lantern {
        float x = 0, y = 0, z = 0;          // the box's center
        float sx = 0.75f, sy = 0.9f, sz = 0.75f;
        float x0 = 0, y0 = 0, z0 = 0, sx0 = 1, tx = 0, tz = 0; // a leg's start, the place on the lake
        float lx = 0, lz = 0;               // where it lands on the water (after leaving the well)
        float t = 0, dur = 1.f, delay = 0;
        int type = 0, state = L_FLOAT;
        bool sky = false;                   // a sky lantern of the drop (never floated on the lake)
        float cap = 1.f;                    // 0: still the piece's paper .. 1: a lantern (cap and base)
        float vy = 0, phase = 0, flick = 0, seed = 0, alpha = 1.f, fade = 0.f;
        bool dying = false, pendingLift = false;
        int slot = 0;                       // its order on its side when leaving the row (0: nearest the frame)
    };
    std::vector<Lantern> lanterns_;
    struct Ember { float x, y, z, vx, vy, t, life; int type; };
    std::vector<Ember> embers_;
    struct SpriteInst { float a[4], b[4]; }; // x, y, z, size | color, alpha
    std::vector<SpriteInst> sprites_;
    bool pickTarget(int side, float& x, float& z);
    void spawnSky(int side, bool far);
    void liftAll(float spread);
    void liftOne(Lantern& L, float spread);
    void updateLanterns(float dt, float wdt);
    void buildLamps();
    vec2 screenBoard(const vec3& p) const; // a world point on screen, in the board plane's units
    float hudDim(const vec3& p) const;     // 1 .. dimmed behind the HUD's lettering

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    void hudInks(vec3& ink, vec3& halo, vec3& accent) const;
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progEnv_ = 0, progBox_ = 0, progSprite_ = 0, progBright_ = 0, progBlur_ = 0, progFinal_ = 0, progShape_ = 0;
    GLuint cubeVbo_ = 0, quadVbo_ = 0, boxVao_ = 0, boxVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0;
    ugl::Target sceneT_, reflT_, bloomA_, bloomB_;
    mat4 vp_, mirrorVp_;
    vec3 eye_{0, 3, 34};
    float focal_ = 1.f, shift_ = 0.f, dist_ = 34.f;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void drawBoxes(const std::vector<BoxInst>& v, const mat4& vp, bool mirror, float stackA);
    void drawSprites(const mat4& vp, bool mirror);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 tint(int i) const;    // a piece's paper (linear)
    vec3 skyHor() const;       // the sky at the horizon now (linear)
    vec3 skyTop() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dev_ = 0.f;          // the drop's sky 0..1
    float swell_ = 0.f;        // the drop's single swell (eased toward swellIn_)
    float swellIn_ = 0.f;
    float pulse_ = 0.f;        // the ripple of the beat grid (every two beats)
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    // The scene's setup and teardown: the clock of the lanterns released across the lake as the night comes out of
    // the dark, the lake's lanterns lifted away once the song is over.
    float setupClock_ = 0.f;
    bool liftedAway_ = false;
    double lastHalf_ = -1.0;
    float beatClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    float light_ = 1.f, ripple_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    bool bare_ = false; // this frame is a break's: the lake alone
    Bridge bridge_;
    Rng rng_{31};
};

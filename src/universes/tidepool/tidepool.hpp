#pragma once
// TIDE POOL: attention as a journey. A rock pool at low tide behind the well; every cleared line zooms deeper into
// it, one continuous dive through nine scales, then back to a rock pool again (see tidepool.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class TidePoolUniverse : public Universe {
public:
    ~TidePoolUniverse() override;
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

    // A variant's colors (linear RGB) and a few amounts (floats only: palettes are blended float by float).
    struct Palette {
        vec3 deep, water, shallow, light, rock, sand, warm, warm2, weed, cool, glow;
        vec3 ink, accent, veil;
        vec3 piece[7];
        float lum, fever;
    };

private:
    // ---- The dive: depth z in octaves of zoom (one stage = S octaves, fifteen stages a loop).
    double z_ = 0.0, zVel_ = 0.0;
    double zBase_ = 0.0;          // the universe this game started from (a multiple of the loop)
    double creep_ = 0.0;          // the slow drift inward, always on
    double bonus_ = 0.0;          // the quads' plunges
    double breath_ = 0.0;         // a slow in and out of the zoom
    float lines_ = 0, linesShown_ = -1; // paceLines(): 50 at scene completion
    bool zSet_ = false;
    double zTarget(float lines) const;
    double debugZ_ = -1.0;        // ZEN_TIDEPOOL_DEPTH: a fixed depth (testing)
    vec2 focus_{15.f, -0.8f};     // board units: where the zoom dives
    std::string scaleText_, stageName_;

    // ---- Particles over the board: bubbles of cleared rows, silt of hard drops.
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, alpha = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0; // 0 silt, 2 bubble
    };
    std::vector<Mote> motes_;

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

    // ---- The HUD (its lettering: hudLine, the host letters it).
    float fade_ = 1.f; // the frame's fade in, the HUD's own (it was lettered under the post's fade and dim)
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progWorld_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
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
    float waveT_ = 99.f, waveK_ = 0.f;   // the drop's wave washing over (the fabric rippling, deep down)
    float kickIn_ = 0.f;
    float stageClock_[15] = {};        // each stage's own clock (slower as the zoom goes into it)
    float earthClock_ = 0.f;           // the planet's clock, from orbit to the shore (slower as the zoom goes in)
    float kickS_ = 0.f;                // the kicks, eased (no flashes)
    float bandS_[4] = {0.f, 0.f, 0.f, 0.f}; // the song's bands, eased (for the strings)
    float beatClock_ = 0.f;
    double ownBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

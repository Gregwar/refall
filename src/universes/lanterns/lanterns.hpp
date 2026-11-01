#pragma once
// LANTERNS: paper lanterns floating on a dark lake, the pieces lit from inside (see lanterns.cpp).
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

private:
    // ---- Pieces: instanced cells (board units).
    struct CellInst { float a[4], b[4], c[4], d[4]; }; // x, y, half, mask | tint, alpha | kind, progress, light, seed | candle x, y, r, ripple
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    float flare_[22][10] = {};    // a piece's candle catching (lock, kick)
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Fade { float x, y; int type, mask; float t, cx, cy, r, seed; }; // a cell going out (a clear)
    std::vector<Fade> fades_;
    struct OverCell { float x, y; int row, type, mask; float cx, cy, r, seed; }; // the stack at game over
    std::vector<OverCell> overCells_;

    // ---- The lake's lanterns (released by the clears), the sky lanterns (the drop), the far lights, embers.
    enum LanternState { L_TRAVEL, L_FLOAT, L_RISE };
    struct Lantern {
        float x = 0, y = 0;        // the base on the water (floating), its position (rising)
        float x0 = 0, y0 = 0, tx = 0, ty = 0;
        float t = 0, travel = 5.f, delay = 0;
        int type = 0, state = L_TRAVEL;
        bool sky = false;          // a sky lantern (no base on the lake)
        float w = 0.5f;            // width while rising (sky lanterns)
        float wy = 0;              // the water under it when it lifted
        float vy = 0, rise = 0, phase = 0, flick = 0, seed = 0, alpha = 1.f, fade = 0.f;
        bool dying = false;
        bool pendingLift = false;  // lifts when it reaches its place
    };
    std::vector<Lantern> lanterns_;
    struct Ember { float x, y, vx, vy, t, life; int type; };
    std::vector<Ember> embers_;
    struct SpriteInst { float a[4], b[4], c[4]; }; // x, y, w, kind | tint, alpha | light, ripple, seed, length
    std::vector<SpriteInst> sprites_, glows_;
    float lakeScale(float y) const;
    bool pickTarget(int side, float& x, float& y);
    void release(int type, int side, float delay, float jitter);
    void spawnSky(int side, bool far);
    void liftAll(float spread);
    void liftOne(Lantern& L, float spread);
    void updateLanterns(float dt, float wdt);
    void buildSprites(float ripple);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontReg_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progLake_ = 0, progCell_ = 0, progSprite_ = 0, progBright_ = 0, progBlur_ = 0, progFinal_ = 0, progShape_ = 0,
           progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

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
    float swell_ = 0.f;        // the drop's single swell
    float pulse_ = 0.f;        // the ripple of the beat grid (every two beats)
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    double lastHalf_ = -1.0;
    float beatClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{31};
};

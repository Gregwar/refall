#pragma once
// NEON: luminous watercolor shapes over a painted night skyline (see neon.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class NeonUniverse : public Universe {
public:
    ~NeonUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };
    struct WispInst { float a[4], b[4], c[4]; };

    // ---- The stack: each piece keeps an id, its wash (noise origin, seed) and the song's phase it locked in.
    struct CellLook { float nx = 0, ny = 0, seed = 0, phase = 0, bright = 1, hue = 0; };
    int ids_[22][10] = {};
    CellLook look_[22][10];
    int nextId_ = 1;
    CellLook lookOf(int id, float relX, float relY, float phase) const;
    void syncIds(const Game& g);
    struct Lift { float x, y; int mask; CellLook l; float t; };   // a cleared cell, its pigment lifting
    std::vector<Lift> lifts_;
    struct Run { float x, y; int mask; CellLook l; float delay; }; // game over: the stack running down
    std::vector<Run> runs_;

    // ---- The skyline: slots painted in as the lines are cleared.
    struct Building {
        float x = 0, base = 0, hw = 1, h = 4;
        int kind = 0, layer = 1;
        float seed = 0;
        float paint = 0, paintDelay = 0, paintDur = 1.6f;
        bool painted = false;   // in color (or waiting to be)
        float phaseA = 0, phaseB = 0, mixAB = 1, tint = 0, tintTarget = 0;
        float alpha = 0.7f, fadeOut = -1.f;
        bool windows = true;
        int order = 0;          // when it was last painted (for re-tinting the oldest)
    };
    std::vector<Building> city_;
    std::vector<int> paintOrder_;      // unpainted slots, in the order they fill
    int paintStamp_ = 0;
    void buildCity(bool instant);
    int paintBuilding(float phase, float delay, int prefer);
    int paintBlock(float phase, float delay, int side);

    // ---- Pigment in the air.
    struct Wisp {
        vec2 p0, p, target;
        float t = 0, delay = 0, life = 1.4f, rise = 1.5f;
        float r0 = 0.45f, r1 = 1.4f, u = 0.5f, phase = 0, seed = 0, rot = 0, stretch = 1.f, peak = 0.75f;
        bool soft = false;
    };
    std::vector<Wisp> wisps_;

    // ---- Drawing.
    std::vector<Inst> builds_, cells_;
    std::vector<WispInst> wispInst_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int mask, const CellLook& l, float xoff, float alpha, int kind, float prog);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progSky_ = 0, progBuild_ = 0, progVeil_ = 0, progPiece_ = 0, progWisp_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0;
    GLuint quadVbo_ = 0, buildVao_ = 0, buildVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, wispVao_ = 0, wispVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};
    void setSpectrum(GLuint prog);

    // ---- Song, variant, the spectrum.
    int variant_ = 0;
    vec3 lab_[5], labTarget_[5];
    vec3 sky_[2], skyTarget_[2];
    bool labSet_ = false;
    float drift_ = 1.f;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;
    int section_ = 0, prevSection_ = 0;
    float phase_ = 0.f, phaseTarget_ = 0.f;
    double refreshAt_ = -1.0;
    float phaseOf(int section) const;
    vec3 spectrumAt(float u, float phase) const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beat_ = 0.f;
    float quadGlow_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the night and the city, no well, no pieces
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x4E30};
};

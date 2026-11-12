#pragma once
// SHADOW: a little model of the game on a table, lit by a vintage projector; the playfield is its shadow on the
// wall behind (see shadow.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class ShadowUniverse : public Universe {
public:
    ~ShadowUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override { glowIn_ = std::max(glowIn_, 0.4f * strength); }
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }

private:
    // ---- The board: piece ids (a piece's cells merge into one block), uploaded as a small texture.
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void uploadBoard(const Game& g);
    GLuint cellsTex_ = 0;
    float rowY_[22] = {}, slot_[24] = {};
    float piece_[16] = {}, ghost_[16] = {}, pieceType_ = 0.f;
    std::vector<float> prev_;

    // ---- Clears: the row's shadow shrinks away; on the model its blocks are lifted out and set on the stacks.
    struct Flight {
        vec3 from, to, axis;
        float t, dur, spin;
        int type, dest; // dest: its color's stack (0..6) or the box (7)
    };
    std::vector<Flight> flights_;
    struct Shrink { float x, y, t; int type; };
    std::vector<Shrink> shrinks_;
    int pile_[8] = {}, reserved_[8] = {}; // the seven stacks in the tray, then the box
    vec3 pileSlot(int dest, int n) const;
    int pileDest(int type) const;
    vec2 pileCenter(int type) const;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font font_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const std::string& s, float size, float track) const;
    void textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const std::vector<float>& q, const vec3& srgb, float alpha, float bias, float soft);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels, float mul);
    float wallLightAt(float px, float py) const; // how lit the wall is at a pixel (0..1)

    // ---- GL.
    GLuint progWall_ = 0, progModel_ = 0, progHaze_ = 0, progFinal_ = 0, progCompose_ = 0, progText_ = 0,
           progPanel_ = 0;
    GLuint quadVbo_ = 0, rectVao_ = 0, textVao_ = 0, textVbo_ = 0, panelVao_ = 0, panelVbo_ = 0;
    ugl::Target sceneT_, modelT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{1, 1};
    float modelRect_[4] = {-21.8f, -12.2f, -11.3f, 2.6f}; // board space (x0, y0, x1, y1)
    vec3 eye_, fwd_, right_, up_;
    vec2 tan_{1, 1};
    vec3 lensPos_, lensDir_;
    vec2 lensScreen_{-18, -7};
    void setupModelCamera();
    void setCommon(GLuint prog);

    // ---- Song, variant, staging.
    int variant_ = 0;
    uint64_t seed_ = 1;
    int level_ = 1;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f, energyS_ = 0.5f;
    bool dropMode_ = false;
    float drop_ = 0.f;          // the second projector, held through the drop
    float dropSwell_ = 0.f;
    float kickIn_ = 0.f, glowIn_ = 0.f, lensGlow_ = 0.f;
    float swirl_ = 0.f, swirlAmp_ = 0.f;
    float flicker_ = 1.f, flickT_ = 0.f, flickTo_ = 1.f;
    float time_ = 0.f, worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    float quadFlash_ = 0.f;
    double lastHalf_ = -1.0;
    Rng rng_{139};
};

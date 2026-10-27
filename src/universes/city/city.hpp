#pragma once
// CITY: a tower's facade at night, its windows the board (see city.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class CityUniverse : public Universe {
public:
    ~CityUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    void forceFx(int slow, int tetris) override;

private:
    struct Inst { float a0[4], a1[4], a2[4], a3[4]; };

    // ---- The facade's windows (visual rows 0 = top of the board .. 19), each one a light that switches like a
    // real one: a quick warm-up (with a flicker for a fluorescent tube), a fade when it goes out.
    struct Win {
        bool on = false, falling = false;
        int style = -1;
        vec3 col{0.f};
        float level = 0.f, from = 0.f, t = 10.f;
        int onKind = 0, offKind = 0;
        float surge = 0.f, dip = 0.f, ghost = 0.f;
        float seed = 0.f;
    };
    static constexpr int ROWS = 20, COLS = 10;
    Win win_[ROWS][COLS];
    int pendingOff_[ROWS][COLS]; // how the next switch-off goes (0 plain, 1 a cleared floor, 2 the blackout, 3 the falling piece moving on)
    float pendingDelay_[ROWS][COLS];
    // the stack's floors step down after a clear (each one a light switch), after the cleared floor went out
    int off_[22];
    float offHold_ = 0.f, offStep_ = 0.f;
    void updateWindows(const Game& g, float dt);
    float onLevel(const Win& w) const;
    float offLevel(const Win& w) const;

    struct Contact { float x0, x1, y, t = 0.f, life = 0.35f, power = 1.f; vec3 col; };
    std::vector<Contact> contacts_;
    bool hardPending_ = false;

    // ---- The city: buildings in three layers (generated per variant), the traffic, the crane.
    struct Bldg { float x0, y0, x1, y1; int layer, style; float seed, lit, crown; bool beacon; };
    std::vector<Bldg> bldgs_;
    int builtFor_ = -1;
    void buildCity(int variant);
    struct Particle { vec2 p, v; vec3 c; float t, life; float delay; };
    std::vector<Particle> sparks_;
    void spawnFireworks();
    struct Flake { vec2 p; float s, ph; };
    std::vector<Flake> flakes_;

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
    void drawText(const std::vector<float>& quads, const vec3& srgb, float alpha, float bias, float soft);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels, float mul);

    // ---- GL.
    GLuint progSky_ = 0, progBldg_ = 0, progTower_ = 0, progWin_ = 0, progDot_ = 0, progFinal_ = 0, progText_ = 0, progPanel_ = 0;
    GLuint quadVbo_ = 0, instVao_ = 0, instVbo_ = 0, textVao_ = 0, textVbo_ = 0, panelVao_ = 0, panelVbo_ = 0;
    GLuint cellA_ = 0, cellB_ = 0;
    ugl::Target sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0.4f}, viewH_{22, 12.4f};
    void drawInst(const std::vector<Inst>& v);
    std::vector<Inst> inst_, dots_, wins_;

    // ---- Variant (colors eased toward it).
    int variant_ = 0;
    bool palSet_ = false;
    struct Pal {
        vec3 top, mid, hor, glow, fog, far, midB, nearB, frame, glass, reflTop, reflBot, city, accent, text, shade, lobby;
        vec3 piece[7];
        float mull, spand, recess, reflK, stars, fogK, rain, snow;
    };
    Pal pal_{};
    vec3 dropA_{1.f}, dropB_{1.f};
    int dropIndex_ = 0, lastPair_ = -1;
    uint64_t songSeed_ = 0;
    void pickDropColors();

    // ---- Level, drop, music.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool inDrop_ = false, sectionKnown_ = false, sectionDrop_ = false;
    float dropWait_ = 0.f, dropT_ = 99.f, dropAmp_ = 0.f;
    float drop_ = 0.f, chorus_ = 0.f, refreshT_ = 99.f;
    float tetrisT_ = 99.f, overT_ = 99.f, cityShift_ = 0.f;
    float slow_ = 0.f;
    bool slowOn_ = false;
    float time_ = 0.f, world_ = 0.f;
    float dimT_ = 0.f, gaugeShown_ = 0.f;
    float eq_[8] = {};
    float patEpoch_ = 0.f, patPhase_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

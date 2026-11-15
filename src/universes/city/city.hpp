#pragma once
// CITY: a tower's facade in a 3D night city, its windows the board (see city.cpp).
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
    void forceFx(int slow, int quadClear) override;
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

private:
    struct Inst { float a0[4], a1[4], a2[4], a3[4]; };

    // ---- The facade's windows (visual rows 0 = top of the board .. 19). A window's light only ever changes
    // smoothly: each change of what it should show starts a short eased blend from what it shows now.
    struct Win {
        vec3 cur{0.f}, from{0.f}, to{0.f};
        float t = 1.f, dur = 0.1f, delay = 0.f;
        float fall = 0.f; // 1 while the falling piece is in it (eased)
        float ghost = 0.f;
        bool falling = false;
    };
    static constexpr int ROWS = 22, COLS = 10; // the board's rows, the two above the well too (pieces spawn there)
    Win win_[ROWS][COLS];
    int pendingOff_[ROWS][COLS]; // how the next switch-off goes (0 plain, 1 a cleared floor, 2 the blackout)
    float pendingDelay_[ROWS][COLS];
    // the stack's floors step down after a clear, one by one, after the cleared floor went out
    int off_[22];
    float offHold_ = 0.f, offStep_ = 0.f;
    void updateWindows(const Game& g, float dt);

    struct Contact { float x0, x1, y, t = 0.f, life = 0.5f, power = 1.f; vec3 col; };
    std::vector<Contact> contacts_;
    bool hardPending_ = false;

    // ---- The city: boxes on a street grid far below, generated per variant.
    struct Bldg { float cx, cz, hw, hd, y0, y1; int style; float seed, lit, crown; bool beacon, patterned; vec3 alb; float refl; };
    std::vector<Bldg> bldgs_;
    int builtFor_ = -1;
    void buildCity(int variant);
    struct Particle { vec3 p, v; vec3 c; float t, life; float delay; };
    std::vector<Particle> sparks_;
    void spawnFireworks();
    struct Flake { vec3 p; float s, ph; };
    std::vector<Flake> flakes_;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels, float mul);
    float hudMul_ = 1.f; // the frame's fade, on the inks

    // ---- GL.
    GLuint progSky_ = 0, progGround_ = 0, progBox_ = 0, progTower_ = 0, progWin_ = 0, progDot_ = 0, progBright_ = 0,
           progBlur_ = 0, progFinal_ = 0, progPanel_ = 0;
    GLuint quadVbo_ = 0, instVao_ = 0, instVbo_ = 0, cubeVbo_ = 0, cubeVao_ = 0, cubeInstVbo_ = 0, quadVao_ = 0;
    GLuint panelVao_ = 0, panelVbo_ = 0, emptyVao_ = 0;
    GLuint cellA_ = 0, cellB_ = 0;
    GLuint sceneFbo_ = 0, sceneTex_ = 0, depthTex_ = 0;
    ugl::Target bloomA_, bloomB_, bloomC_, bloomD_;
    mat4 vp_, invVP_;
    vec3 eye_{0.f};
    void drawInst(const std::vector<Inst>& v);
    void fullscreen();
    std::vector<Inst> inst_, dots_, wins_;

    // ---- Variant (colors eased toward it).
    int variant_ = 0;
    bool palSet_ = false;
    struct Pal {
        vec3 top, hor, glow, fog, cloud, amb, moon, upl, street, city, frame, tint, near, far, accent, text, shade, reflCity;
        vec3 piece[7];
        float fogD, mull, spand, recess, reflK, stars, clouds, rain, snow, glowK, exposure;
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
    float drop_ = 0.f, chorus_ = 0.f;
    float quadT_ = 99.f, overT_ = 99.f, cityShift_ = 0.f;
    float slow_ = 0.f;
    bool slowOn_ = false;
    float time_ = 0.f, world_ = 0.f;
    float dimT_ = 0.f;
    float eq_[8] = {};
    float patEpoch_ = 0.f, patPhase_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

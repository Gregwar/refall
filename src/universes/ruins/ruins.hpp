#pragma once
// RUINS: time running backwards from overgrown ruins to a temple in its prime, ray marched in 3D behind the well;
// the pieces are stone blocks carved with a glyph each (see ruins.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RuinsUniverse : public Universe {
public:
    ~RuinsUniverse() override;
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

    struct Palette {
        vec3 stone, stone2, moss, moss2, leaf, leafLit, bark, ground, skyZ, skyH, fog, sun, amb, ink, accent, veil;
        vec3 piece[7];
    };

private:
    // ---- The camera drifting among the ruins (world units, y up).
    vec3 eye_{0, 3, 14}, fwd_{0, 0, -1}, right_{1, 0, 0}, up_{0, 1, 0};
    float tanH_ = 0.45f, aspect_ = 16.f / 9.f, camPhase_ = 0.f, side_ = 1.f;
    void updateCamera();

    // ---- The rewind: time runs backwards with the lines (on the SCENE_REF_LINES scale, eased): the jungle
    // recedes, the ruins repair themselves, the temple comes back to its prime.
    float linesEased_ = -1.f, lines_ = 0.f, swirlT_ = 0.f;
    float jungle_ = 1.f, temple_ = 0.f, broken_ = 0.16f, wear_ = 1.f, prime_ = 0.f, col_[8] = {};
    void updateRewind(float L);
    bool toBoard(const vec3& w, vec2& b) const;
    struct Swirl { vec2 c; float age, life, ang, rad, size, seed; };
    std::vector<Swirl> swirls_;
    void dustAt(const vec3& w, int n);
    // The repairs, one event at a time (a column's top flying back, the block beside it, a step of the temple), in an
    // order the camera can follow: it goes to each one, frames it beside the well, closer, and only then it starts.
    struct Ev { int kind, idx; float from, to; vec3 focus; float dist, side; };
    std::vector<Ev> plan_;
    int planFor_ = -1, next_ = 0, active_ = -1;
    float evT_ = -1.f, travelT_ = 0.f, idleT_ = 99.f, colRep_[8] = {}, templeRep_ = 0.f, dustT_ = 0.f;
    vec3 focusP_{0, 0, 0};
    float focusA_ = 0.f, focusR_ = 3.f;
    vec3 ovEye_{0, 3, 14}, ovTgt_{0, 10, -50}, camEye_{0, 3, 14}, camTgt_{0, 10, -50}, eyeV_{0, 0, 0}, tgtV_{0, 0, 0};
    vec3 goalEye_{0, 3, 14}, goalTgt_{0, 10, -50};
    bool camInit_ = false;
    void buildPlan();
    int owedEvents(float L) const;
    void applyEv(const Ev& e, float t);
    void focusPose(const Ev& e, vec3& eye, vec3& tgt) const;
    void stepCamera(float dt);

    // ---- The board (ortho, the layout's plane).
    mat4 vp_;
    vec2 viewH_{10, 10}, viewC_{0, 0};
    float pxPerUnit_ = 50.f;

    // ---- Pieces: one carved glyph block per cell.
    struct CellInst { float a[4], b[4], c[4]; }; // x y half seed | tint alpha | mode erase dull type
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, int type, float alpha, int mode, float erase, float seed);
    void buildBoard(const Game& g);
    struct Crumble { float x, y; int type; float t, seed; };
    std::vector<Crumble> crumbling_;
    struct Over { float x, y; int type; float row; };
    std::vector<Over> overCells_;
    float overT_ = -1.f, over_ = 0.f;

    // ---- Sprites: dust, stone chips, birds, butterflies (behind the well), the hard drop's dust and the gauge.
    struct SpriteInst { float a[4], b[4], c[4]; }; // x y size mode | rgb alpha | param param2 - -
    std::vector<SpriteInst> back_, front_;
    void sprite(std::vector<SpriteInst>& to, vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f,
                float param2 = 0.f);
    struct Bit { vec2 p, v; float age, life, size, spin, seed; vec3 col; int mode; float alpha; };
    std::vector<Bit> chips_, dust_, puffs_;
    struct Bird { vec2 p, v; float age, phase, size, glide; };
    std::vector<Bird> birds_;
    void releaseBirds(int n, int side);
    struct Fly { vec2 home, p; float phase, seed, hue; };
    std::vector<Fly> flies_;
    float birdCool_ = 0.f, calmBirds_ = 12.f;
    double lastHalf_ = -1.0, ownBeat_ = 0.0;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // its halo: the usual one, the celebrations' and the dialogs' lighter one
    void drawSprites(const std::vector<SpriteInst>& list);

    // ---- GL.
    GLuint progWorld_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVbo_ = 0;
    ugl::Target worldT_, sceneT_;

    // ---- Song, variant, staging.
    int variant_ = 0;
    uint64_t seed_ = 1;
    Rng rng_{0x2B1E5};
    Palette pal_;
    bool palSet_ = false;
    int level_ = 1;
    bool hasLevel_ = false, dropMode_ = false, slowOn_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f, energy_ = 0.5f;
    float gold_ = 0.f, swell_ = 0.f, kickIn_ = 0.f, kickS_ = 0.f, slow_ = 0.f, dimT_ = 0.f, gaugeShown_ = 0.f;
    float time_ = 0.f, sway_ = 0.f, swayAmp_ = 0.3f;
    Bridge bridge_;
};

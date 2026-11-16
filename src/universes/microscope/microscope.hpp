#pragma once
// MICROSCOPE: the whole screen a microscope's field of view in a soft micrograph look; the pieces colonies of living
// cells, a few microorganisms wandering on the sides (see microscope.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class MicroscopeUniverse : public Universe {
public:
    ~MicroscopeUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    HudInk hudInk(const HudText& h) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };

    // ---- The illumination: a variant's colors, its own and the drop's (linear RGB).
    struct Light {
        vec3 field[2];   // the field of view: center, edge
        vec3 piece[7];   // the seven piece types' stains
        vec3 org[3];     // the organisms: body, rim, organelles
        vec3 nucGlow;    // the nuclei's glow (dark-field)
        float dark = 0;  // 1: dark-field (light on black), 0: bright-field (stains on a lit field)
        float phase = 1; // the phase halos' strength
    };
    Light own_, ownT_, drop_, dropT_;
    bool colorsSet_ = false;
    void setLight(GLuint prog);
    float darkNow() const;

    // ---- The stack: each cell keeps its look (nucleus, seed), its colony (the piece it came with: the cells of one
    // colony are joined) and the colony's anchor on the board (its membrane's noise is laid from it, so a piece
    // breathes the same falling and locked).
    struct CellLook { float nx = 0, ny = 0, seed = 0; int colony = -1, ax = 0, ay = 0; };
    CellLook look_[22][10];
    int8_t has_[22][10] = {};   // the type of the cell the look was made for (-1: none)
    CellLook lookOf();
    CellLook fallLook_[4];   // the falling piece's cells, kept as they are when it locks
    int colonies_ = 0;
    void syncLooks(const Game& g);
    struct Dying { float x, y; int type; CellLook l; float delay; }; // game over: plasmolysis
    std::vector<Dying> dying_;
    struct Speck { vec2 p, v; float t, life, r; int type; };       // loose granules (a hard drop, a burst cell)
    std::vector<Speck> specks_;
    // a cleared cell bursting: its look and its joins (to the other cleared cells of its colony) as it was
    struct Burst { float x, y; int type; CellLook l; int mask; float gx, gy, t; };
    std::vector<Burst> bursts_;

    // ---- The life: a few organisms wandering on the sides, each on its own side, at its own depth.
    struct Org {
        int kind = 0, side = 1;
        vec2 p{0, 0}, goal{0, 0};
        float ang = 0, speed = 0.3f, size = 2.f, seed = 0, depth = 0, turnPhase = 0, cilia = 0, dir = 1.f, spin = 0;
        float goalT = 0, back = 0;   // time left on its goal; backing up (a paramecium's avoiding reaction)
    };
    std::vector<Org> orgs_;
    void populate();
    vec2 pickGoal(int side, float size);
    float zoneDist(const vec2& p, vec2* grad) const;  // distance to the well, the HUD and the screen's edge

    // ---- Drawing.
    std::vector<Inst> cells_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha, float flash,
                 int mask, float gx, float gy);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);
    void drawPanels(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progField_ = 0, progOrg_ = 0, progPiece_ = 0, progSpeck_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, orgVao_ = 0, orgVbo_ = 0, speckVao_ = 0, speckVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0;
    ugl::Target sceneT_, blurA_, blurB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant, staging.
    int variant_ = 0;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float dropK_ = 0.f;            // the drop's look, eased in and out (music.inDrop)
    bool inDrop_ = false;
    float swell_ = 0.f;            // a single swell of light (into a drop, a quad)
    float focus_ = 0.f, focusT_ = 0.f; // the focal plane (the organisms' depth in focus)
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    bool bare_ = false; // this frame is the world alone (a break): the field and its life, no cells, no chamber
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x3C1};
};

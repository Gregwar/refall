#pragma once
// GLOWING FOREST: a night forest as a relief print (a black key block carved with a gouge, fluorescent inks for
// everything that glows, blazing under a blacklight on the drop; see forest.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class ForestUniverse : public Universe {
public:
    ~ForestUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
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

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };
    struct Inst2 { float a[4], b[4]; };

    // ---- The stack: each piece keeps an id; each cell its noise origin (its place within the piece) and seed.
    struct CellLook { float nx = 0, ny = 0, seed = 0; };
    int ids_[22][10] = {};
    CellLook look_[22][10];
    int nextId_ = 1;
    CellLook lookOf(int id, float relX, float relY) const;
    void syncIds(const Game& g);
    struct Fade { float x, y; int type; CellLook l; float t, delay; };
    std::vector<Fade> clears_;   // cleared cells gouged away
    std::vector<Fade> outs_;     // game over: the stack inked over

    // ---- The forest: near trunks, and the glowing life placed among them (it stays for the universe's life).
    struct Trunk { float x, hw, lean, flare; };
    std::vector<Trunk> trunks_;
    struct Plant {
        float x = 0, y = 0, scale = 1, seed = 0;
        int kind = 0, group = 0;    // its kind; its quarter of the forest on the beat grid
        bool flip = false;
        float colMix = 0;           // 0 the variant's first glow ink .. 1 its second
        float kickGlow = 0;
        float height = 2.f;         // its top, in plant units
    };
    std::vector<Plant> plants_;
    void buildTrunks();
    void buildForest();
    vec2 plantTop(const Plant& p) const;

    // ---- Light in the air: fireflies, spores.
    struct Mote {
        vec2 p, v, p0, target;
        float t = 0, life = 8.f, delay = 0, r = 0.3f, a = 1.f, seed = 0;
        int kind = 0;               // 0 firefly, 1 spore rising, 2 a chip at a hard drop
        float inkB = 0.f;           // its glow ink: 0 the first .. 1 the second
        vec2 trail[8];              // a firefly's recent places (its fading trail)
        int trailN = 0;
        float trailAcc = 0.f;
    };
    std::vector<Mote> motes_;

    // ---- Drawing.
    std::vector<Inst> plantInst_, cells_;
    std::vector<Inst2> halos_, moteInst_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, const CellLook& l, float alpha, int kind, float prog);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);
    vec3 pieceColor(int type) const;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    // the HUD's lines in board units (x0, y0, x1, y1), kept uncut
    static constexpr int MAX_BOXES = 24;
    float boxes_[MAX_BOXES * 4] = {};
    int boxN_ = 0;
    void readBoxes(const Frame& f);
    void setBoxes(GLuint prog) const;
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progBg_ = 0, progHalo_ = 0, progPlant_ = 0, progMote_ = 0, progPiece_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0;
    GLuint quadVbo_ = 0, plantVao_ = 0, plantVbo_ = 0, haloVao_ = 0, haloVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0;
    ugl::Target plateT_, lightT_, bloomA_, bloomB_;
    GLuint tagTex_ = 0;             // the plates' second texture: each element's tag (its neon on the drop)  // the plates (carved, ink A, ink B), the plants' light, the fluorescence
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant.
    int variant_ = 0;
    // linear: the paper, the key block's ink, the world's two glow inks, the drop's third ink, slow motion's ink, the
    // seven blocks
    vec3 paper_, paperT_, key_, keyT_, glow_[2], glowT_[2], dropInk_, dropInkT_, slowInk_, slowInkT_, piece_[7], pieceT_[7];
    vec3 neon_[4], neonT_[4];       // the drop's neon inks
    bool colSet_ = false;
    uint64_t seed_ = 1, trunkSeed_ = 0x7F0E57ull, forestSeed_ = 0;
    float seedF_ = 0.f, groundSeed_ = 0.f;
    float rock_[3] = {-17.4f, 7.4f, 2.7f};  // the mossy rock (the bottom-left corner): center, half width, height
    float cornerRight_ = -12.8f;    // the right end of the corner's text (board units)
    float pool_[3] = {0.f, 0.f, -10.66f};  // the still pool on the right floor: x0, x1 (none when equal), water level
    float moon_[3] = {16.f, 7.f, 2.5f};     // the moon cut out of the sky: center, radius
    float glade_[4] = {-16.f, -1.f, 4.f, 3.f};  // a far glade glowing between the trunks on the left: center, radii
    float wind_ = 0.4f;             // the breeze in the plants and vines

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, waveT_ = 99.f, waveK_ = 0.f;
    float dropInkK_ = 0.f;          // the drop's third ink on the print (0..1)
    float blaze_ = 0.f;             // the drop's blacklight: the forest's cuts lit in neon (0..1)
    float jolt_ = 0.f;              // the glow plates knocked out of register on the drop, settling back
    float kickIn_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beat_ = 0.f;
    int lastHalf_ = -1;
    float quadGlow_ = 0.f;
    float fireflyAcc_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    bool bare_ = false; // this frame is the world alone (a break): the forest and its life, no well, no blocks
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0xF0257};
};

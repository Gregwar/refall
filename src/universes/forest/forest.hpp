#pragma once
// GLOWING FOREST: a night forest lit only by bioluminescence, growing with the cleared lines (see forest.cpp).
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
    std::vector<Fade> clears_;   // cleared cells breaking into spores
    std::vector<Fade> outs_;     // game over: the stack's glow going out

    // ---- The forest: near trunks, and slots where life can grow, filled as lines clear.
    struct Trunk { float x, hw, lean, flare; };
    std::vector<Trunk> trunks_;
    struct Plant {
        float x = 0, y = 0, scale = 1, seed = 0;
        int kind = 0, group = 0;
        bool flip = false;
        float colMix = 0;           // 0 the variant's first glow .. 1 its second
        bool live = false;
        float grow = 0, delay = 0, growDur = 3.f;
        float fade = -1.f;          // withering (a new song), 0..1
        float kickGlow = 0;
        float height = 2.f;         // its top, in plant units
    };
    std::vector<Plant> plants_;
    std::vector<int> order_;        // the slots still empty, in the order they fill
    void buildTrunks();
    void buildForest(bool instant);
    int sprout(float delay, float nearX, bool fast);
    vec2 plantTop(const Plant& p) const;

    // ---- Light in the air: fireflies, spores.
    struct Mote {
        vec2 p, v, p0, target;
        float t = 0, life = 8.f, delay = 0, r = 0.3f, a = 1.f, seed = 0;
        int kind = 0;               // 0 firefly, 1 spore flying to a plant, 2 spore rising, 3 a puff at a hard drop
        vec3 col{1.f};
        vec2 trail[8];              // a firefly's recent places (its fading trail)
        int trailN = 0;
        float trailAcc = 0.f;
    };
    std::vector<Mote> motes_;

    // ---- Drawing.
    std::vector<Inst> plantInst_, cells_;
    std::vector<Inst2> halos_, moteInst_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, const CellLook& l, float alpha, int kind, float prog, float glow = 1.f);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);
    vec3 pieceColor(int type) const;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontCap_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.45f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progBg_ = 0, progHalo_ = 0, progPlant_ = 0, progVeil_ = 0, progMote_ = 0, progPiece_ = 0, progDown_ = 0,
           progBlur_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, plantVao_ = 0, plantVbo_ = 0, haloVao_ = 0, haloVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant.
    int variant_ = 0;
    vec3 sky_[2], skyT_[2], mist_, mistT_, moss_, mossT_, glow_[2], glowT_[2], piece_[7], pieceT_[7], moon_, moonT_;
    bool colSet_ = false;
    uint64_t seed_ = 1, trunkSeed_ = 0x7F0E57ull;
    float seedF_ = 0.f, groundSeed_ = 0.f;
    float rock_[3] = {-17.4f, 7.4f, 2.7f};  // the mossy rock (the bottom-left corner): center, half width, height
    float cornerRight_ = -12.8f;    // the right end of the corner's text (board units)
    float pool_[3] = {0.f, 0.f, -10.66f};  // the still pool on the right floor: x0, x1 (none when equal), water level
    float grow_ = 0.f;              // how much of the forest has grown (0..1, eased)
    float wind_ = 0.4f;             // the breeze in the plants and vines

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, waveT_ = 99.f, waveK_ = 0.f;
    float kickIn_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beat_ = 0.f;
    int lastHalf_ = -1;
    float tetrisGlow_ = 0.f;
    float fireflyAcc_ = 0.f;
    int sideFlip_ = 0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the forest and its life, no well, no blocks
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0xF0257};
};

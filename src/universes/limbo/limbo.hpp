#pragma once
// LIMBO: a monochrome world of silhouettes in fog (see limbo.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class LimboUniverse : public Universe {
public:
    ~LimboUniverse() override;
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
    // ---- The world's silhouettes: instanced primitives, one list per depth (far to near).
    struct Prim { float a[4], b[4], c[4]; }; // x0 y0 x1 y1 | r0 r1 kind fog | alpha angle extra tone
    enum { L_FAR, L_EDGE, L_MID, L_NEAR, L_COUNT };
    std::vector<Prim> layer_[L_COUNT];
    std::vector<Prim> glows_, flockPrims_;
    float layerFog_[L_COUNT] = {};

    // What stands in the world: trees, structures, ropes, grasses... each from its own seed, rebuilt every frame
    // (they sway on the beat grid). `grow` >= 0: a thing the play added (index in the growth order).
    enum Kind { K_DEADTREE, K_PINE, K_TOWER, K_WATERTOWER, K_MILL, K_ROPE, K_GRASS, K_POST, K_BIRD, K_LANTERN,
                K_HILL, K_GEARS, K_PYLON };
    struct Thing {
        int kind, layer;
        float x, y, size;
        uint64_t seed;
        float phase;
        int grow = -1;   // -1: always there; else appears with the grow-th cleared row
        int host = -1;   // a bird or a lantern: the thing it sits on / hangs from
        float shown = 1; // 0..1 how far it has grown
    };
    std::vector<Thing> things_;
    std::vector<vec2> perches_[64]; // per thing (by index, up to 64 hosts): where a bird may sit, where a lantern hangs
    int grownTarget_ = 0;           // how many things the play has added so far
    bool grownInit_ = false;
    void placeWorld();
    void buildWorld(float sway, float beatStep);
    void tree(std::vector<Prim>& out, const Thing& t, float fog, float sway, std::vector<vec2>* perches);
    void branch(std::vector<Prim>& out, Rng& r, vec2 p, float ang, float len, float rad, int depth, float fog,
                float sway, float grow, std::vector<vec2>* perches);

    // ---- Pieces (instanced cells, board units).
    struct CellInst { float a[4], b[4], c[4]; }; // x, y, half, mask | tint, alpha | kind, dissolve, glint, seed
    std::vector<CellInst> cells_, ghost_;
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t, delay, seed; }; // a cell dissolving into the fog
    std::vector<Fading> fading_;

    // ---- Motes (a cleared row's drift, a hard drop's dust) and the Tetris flock.
    struct Mote { vec2 p, v; float r, a, life, t; int tone; };
    std::vector<Mote> motes_;
    struct MoteInst { float a[4], b[4]; };
    std::vector<MoteInst> moteInst_;
    struct Bird { vec2 p, v; float flap, t, size; };
    std::vector<Bird> flock_;
    // What passes through the scene during a drop, in black against the bright fog: herons gliding across the side
    // skies, crows, a far train, a balloon rising, leaves in the gust.
    enum PassKind { P_HERON, P_CROW, P_TRAIN, P_BALLOON, P_LEAF };
    struct Passer { int kind, layer; vec2 p, v; float t, life, size, phase, dir; };
    std::vector<Passer> passers_;
    long passBar_ = -1;
    int passCount_ = 0;
    double beat_ = 0.0;
    void spawnPassers();
    void spawnFlyer(int kind, float side);
    void buildPassers();

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontSans_, fontSerif_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    float haloA_ = 0.5f;
    void flushShapes();

    // ---- GL.
    GLuint progRays_ = 0, progSky_ = 0, progSil_ = 0, progBank_ = 0, progPiece_ = 0, progMote_ = 0, progFinal_ = 0, progShape_ = 0,
           progText_ = 0;
    GLuint quadVbo_ = 0, primVao_ = 0, primVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, moteVao_ = 0, moteVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, rayT_; // rayT_: the light shafts (quarter resolution)
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void drawPrims(const std::vector<Prim>& v);
    void drawBank(float density, float y, float thick, float drift, float scale, float layer, float bright);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float partGoal_ = 0.f, partMid_ = 0.f, part_ = 0.f; // the drop's parting fog (eased twice: no jump)
    float swellGoal_ = 0.f, swell_ = 0.f;              // the drop's light swell (eased in, settling over seconds)
    float celebrate_ = 0.f;
    float dropHold_ = 0.f;                             // a drop outside a peak holds its look this long (seconds)
    float gustGoal_ = 0.f, gust_ = 0.f;                // the drop's gust bending the trees outward (eased)
    float invert_ = 0.f;                               // the drop's negative (0..1)
    float gearSpin_ = 0.f;                             // the gears spinning through a drop                            // a Tetris: the light warms for a moment
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float lampKick_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    float drift_ = 0.f;      // the fog banks' drift (board units)
    float gearAngle_ = 0.f;  // the gears turn a notch every two beats
    float millAngle_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): no pieces, no posts, no gauge, the fog even
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

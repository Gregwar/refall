#pragma once
// CRYSTAL CAVE: gemstone pieces in a geode cave, each phase of the song its own mineral (see crystal.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class CrystalUniverse : public Universe {
public:
    ~CrystalUniverse() override;
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
    // ---- The stack: every cell keeps the mineral it locked in (its color frozen at lock time).
    struct Stone {
        vec3 col{0.f};    // linear, the mineral's tone at lock (before the piece type's own shade)
        int mineral = 0;  // 0 calm, 1 mid, 2 peak, 3 drop
        int tone = 0;     // which of the mineral's tones (a section)
        float px = 0, py = 0; // where its facet pattern was cut (the board cell at lock), kept as rows fall
    };
    Stone st_[22][10];
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);

    // Instanced gem cells: x, y, half, mask | rgb, alpha | type, glint, kind, dull | pattern x, y, pad, pad
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_;
    void buildBoard(const Game& g);

    // ---- A cleared row's shards, chips knocked off by a hard drop.
    struct Shard { vec2 p, v; float rot, spin, size, t, life, landY; vec3 col; float seed; bool landed; };
    std::vector<Shard> shards_;
    struct ShardInst { float a[4], b[4], c[4]; }; // x, y, size, rot | rgb, alpha | phase, under, seed, pad
    std::vector<ShardInst> shardInst_;
    void shatter(float x, float y, const vec3& col, int n, float dull);
    // ---- Game over: the stack's stones (the game has emptied its board), their light going out, then crumbling.
    struct Dead { int x, y, type, mask; vec3 col; float px, py; bool gone; };
    std::vector<Dead> dead_;

    // ---- The cave's crystals (instanced pointed prisms) and their light.
    struct Crys {
        vec2 base;
        float angle, len, width, tip, depth, alpha;
        float u;          // which share of the stack's minerals it echoes (0..1, fixed)
        vec3 col{0.f}, target{0.f};
        float glint = 0, phase = 0;
        int cluster = 0;
    };
    std::vector<Crys> crys_;
    struct CrysInst { float a[4], b[4], c[4]; }; // base x, y, angle, len | width, tip, depth, alpha | rgb, glint
    std::vector<CrysInst> crysInst_;
    struct Cluster { vec2 c; float r; vec3 col{0.f}; };
    std::vector<Cluster> clusters_;
    std::vector<vec2> dripSrc_; // tips that drip
    void placeCrystals();

    // ---- Light sprites: glows, sparkles, water drops (additive).
    struct GlowInst { float a[4], b[4], c[4]; }; // x, y, rx, ry | rgb, intensity | kind, rot, pad, pad
    std::vector<GlowInst> glows_;
    struct Sparkle { vec2 p; float t, life, size; vec3 col; };
    std::vector<Sparkle> sparkles_;
    struct Drip { float x, landY, hitBeat; int src; };
    std::vector<Drip> drips_;
    struct Ripple { float x, depth, t, amp; };
    std::vector<Ripple> ripples_;
    double nextDripBeat_ = -1.0;
    int dripTurn_ = 0;

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
    GLuint progBg_ = 0, progPool_ = 0, progCrys_ = 0, progGlow_ = 0, progCell_ = 0, progShard_ = 0, progDown_ = 0,
           progBlur_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, crysVao_ = 0, crysVbo_ = 0, glowVao_ = 0, glowVbo_ = 0,
           shardVao_ = 0, shardVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target bgT_, sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    float waterY_ = -8.6f;
    void drawGlows();

    // ---- Song, variant, the minerals.
    int variant_ = 0;
    uint64_t seed_ = 1;
    int tone_[4] = {0, 0, 0, 0};   // each mineral's current tone (a new section of that level moves it on)
    int prevTone_[4] = {0, 0, 0, 0};
    bool used_[4] = {false, false, false, false}; // a mineral already shown in this song (coming back: next tone)
    void enterMineral(int m);
    float refreshAt_ = -1.f;
    int mineral_ = 1;              // the mineral of the moment
    vec3 cur_{0.f};                // its color as shown (eases to the target, sRGB)
    bool curSet_ = false;
    vec3 stackAvg_{0.f};           // the stack's minerals, averaged (linear)
    vec3 mineralColor(int m, int tone) const; // linear
    vec3 currentColor() const;     // linear

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropOn_ = false;          // the drop's mineral is up
    double dropBeat_ = 0.0;        // when it came (beats)
    float dropStart_ = 0.f;        // (seconds, without a song)
    float dropT_ = 0.f, dropA_ = 0.f, dropEnv_ = 0.f; // the drop's illumination (two eased stages: no jump)
    float ring_ = -1.f;            // a Tetris: a ring of light running through the cave's crystals (seconds)
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    double beat_ = 0.0;
    float bps_ = 2.f;
    bool hasSong_ = false;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

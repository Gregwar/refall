#pragma once
// RAINY WINDOW: a night window pane, a city out of focus behind it, rain on the glass (see rainy.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RainyUniverse : public Universe {
public:
    ~RainyUniverse() override;
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
    // ---- The glass tiles (instanced quads, board units).
    struct CellInst { float a[4], b[4], c[4]; }; // x, y, half, mask | color, alpha | kind, progress, glint, seed
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    float pool_[22][10] = {};  // water resting on a cell's top face (0..1)
    float poolShow_[22][10] = {}; // the pools as drawn, easing toward pool_
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Melt { float x, y; int type, mask; float t, delay, seed; }; // a tile turning to water (a clear, game over)
    std::vector<Melt> melt_;

    // ---- The water on the glass (CPU): running and stuck drops, small beads, the mist.
    enum DropState { STUCK, RUN, SLIDE, GONE };
    struct Drop {
        float x, y, r;
        float vy = 0.f, vx = 0.f;
        DropState st = STUCK;
        float grow = 0.f;      // appearing 0..1
        float travel = 0.f;    // distance since the last trail bead
        float gap = 0.3f;      // distance to the next one
        float tx = 0.f;        // SLIDE: the x it slides to
        int row = 0;           // SLIDE: the row whose top face it slides on
        float wig = 0.f;       // meander phase
        float fade = 1.f;      // GONE: shrinking away
        bool merging = false;  // GONE: flowing into the drop that took it, at (mx, my)
        float mx = 0.f, my = 0.f;
        // what is drawn eases toward the simulation, so a drop never jumps: its radius, its stretch, an offset
        // left when it is set onto a piece's top
        float rv = -1.f, el = 0.f, oy = 0.f;
    };
    std::vector<Drop> drops_;
    struct Bead { float x, y, r, grow, dry, dryMax; }; // dry > 0: a trail bead drying away
    std::vector<Bead> beads_;
    struct Splash { float x, y, r, vy, t, life; };     // water running over the tiles (a melted row)
    std::vector<Splash> splash_;
    std::vector<unsigned char> mistTex_;
    std::vector<float> mist_;
    int mw_ = 0, mh_ = 0;
    float rainAcc_ = 0.f, stuckAcc_ = 0.f;
    void spawnBead(float x, float y, float r, float grow);
    bool spawnStuck(bool anywhere);
    void releaseRuns(int n, bool upper);
    void stepWater(const Game& g, float dt);
    bool occupied(const Game& g, int cx, int cy) const;
    float hudBox(int b, int k) const;
    float hudMask(float x, float y) const;
    void clearMist(float x, float y, float r);
    struct WInst { float a[4], b[4]; };   // x, y, r, elong | height scale
    std::vector<WInst> water_;
    void buildWater(const Game& g);

    // ---- The city out of focus: bokeh discs.
    struct Light {
        float x, y, r, base;  // board units, brightness
        int pal;              // palette slot 0..3
        float on = 1.f, onTo = 1.f;
        float vx = 0.f;       // cars drift
        bool bass = false;    // glows with the kicks of a peak
        float soft = 0.1f;
    };
    std::vector<Light> lights_;
    struct Lantern { float x, y, r, t, life; vec3 col; }; // a quad: the rows' colors rising into the city
    std::vector<Lantern> lanterns_;
    struct BInst { float a[4], b[4]; };   // x, y, r, intensity | rgb, softness
    std::vector<BInst> bokeh_;
    void planCity();

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontLight_, fontReg_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.45f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();
    void disc(float cx, float cy, float r, const vec3& c, float a);

    // ---- GL.
    GLuint progSky_ = 0, progBokeh_ = 0, progBlur_ = 0, progWater_ = 0, progGlass_ = 0, progCell_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, waterVao_ = 0, waterVbo_ = 0, bokehVao_ = 0, bokehVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0, mistTexId_ = 0;
    ugl::Target cityT_, blurA_, blurB_, waterT_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 sky(int i) const;       // 0 top, 1 bottom (linear)
    vec3 lamp(int i) const;      // the city's four light colors now (linear), between the variant's and the drop's
    vec3 tint(int i) const;      // a piece's glass (linear)
    vec3 accent() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dev_ = 0.f;            // the drop's city colors 0..1
    float swell_ = 0.f;          // the drop's light swell
    float kickIn_ = 0.f;
    double lastHalf_ = -1.0, lastBar_ = -1.0;
    float beatClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    bool bare_ = false; // this frame is a break's: the window and the rain alone
    Bridge bridge_;
    Rng rng_{777};
};

#pragma once
// STAINED GLASS: a chapel's window, the well and its pieces leaded glass; a cleared row melts and runs as molten
// glass through the lead channels of the stone into a pane of the side windows (see stainedglass.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class StainedGlassUniverse : public Universe {
public:
    ~StainedGlassUniverse() override;
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
    // ---- The stack: each piece one sheet of glass (its texture runs on across its cells).
    struct Sheet { float px = 0, py = 0, seed = 0; }; // where the sheet was cut (kept as rows fall), its own seed
    Sheet sh_[22][10];
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);

    // Instanced cells: x, y, half, mask | rgb, alpha | type, glint, kind, dark | pattern x, y, seed, crack
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_;
    int previewStart_ = 0; // the previews' cells come last
    void buildBoard(const Game& g);

    // ---- A cleared row: its glass melts, then runs out of the well as molten glass (a Flow).
    struct Crack { int x, y, type, mask; float px, py, seed, t; };
    std::vector<Crack> cracks_;
    struct Shard { vec2 p, v; float rot, spin, size, t, life, floorY; vec3 col; float seed; bool landed; };
    std::vector<Shard> shards_;
    struct ShardInst { float a[4], b[4], c[4]; }; // x, y, size, rot | rgb, alpha | phase, seed, pad, pad
    std::vector<ShardInst> shardInst_;
    void shatter(float x, float y, const vec3& col, int n, float lift);
    // ---- Game over: the stack's glass (the game has emptied its board), its light going out, then falling.
    struct Dead { int x, y, type, mask; float px, py, seed; bool gone; };
    std::vector<Dead> dead_;

    // ---- The side windows: their panes, clear until molten glass from a cleared row pours into one.
    struct Pane {
        vec3 a{0.f}, b{0.f};  // ground and medallion colors (linear)
        vec3 na{0.f}, nb{0.f}; // the glass on its way (taken on when it starts pouring in)
        float fill = 0.f;     // its level (0 clear .. 1 full)
        float target = 0.f;
        float heat = 0.f;     // 1 white-hot .. 0 set
        float glint = 0.f;
        int order = -1;       // when it was filled (the oldest is filled again once the window is complete)
        bool pending = false; // molten glass is on its way (its old glass drains away meanwhile)
        bool pouring = false;
        bool rose = false;
        vec2 pos{0.f};        // its middle (board units), for the glints
        float yLo = 0.f, yHi = 0.f; // its bottom and top (the level rises between them)
        vec2 entry{0.f};      // where the molten glass comes in (its bottom, on the lead)
        float half = 0.5f;    // about its half width (sparks)
    };
    std::vector<Pane> panes_;
    int slotsPerWin_ = 0, panelsK_ = 6, lancets_ = 2;
    bool winOn_ = false, roseOn_ = false;
    float winCx_ = 16.f, winBase_ = -8.f, winSpring_ = 3.f, winA0_ = 3.f, winR0_ = 6.f, winA1_ = 1.5f, winR1_ = 3.f,
          winM_ = 0.13f, roseY_ = 3.f, roseR_ = 1.8f, roseCore_ = 0.5f, panelH_ = 1.6f;
    int fillCount_ = 0;
    int roseLit_ = -1;            // the rose a Tetris glazed last (it blazes)
    std::vector<int> lancetOrder_; // the order lancet panes fill in (bottom up, alternating sides)
    void placeWindows();
    int fillPane(const vec3& a, const vec3& b);
    int roseSide();

    // ---- Molten glass: a cleared row runs out of the well along the lead channels (down the jamb, along the
    // string course under the HUD, up the window's mullion and saddle bars) into its pane.
    struct Flow {
        std::vector<vec2> pts;   // its path
        std::vector<float> cum;  // arc length at each point
        float len = 0.f;
        float sWell = 9.6f;      // where it leaves the well
        float t = 0.f;           // seconds since it started to run (< 0: the row is still melting)
        float speed = 11.f;      // its head's speed in the channels
        float vol = 4.5f;        // its length once out of the well
        int pane = -1;
        int sgn = 1;             // the side it runs to
        bool arrived = false;
        float lenArrive = 1.f;
        vec3 cols[10];           // the row's glass, cell by cell (x)
        vec3 tint{1.f};
    };
    std::vector<Flow> flows_;
    void addFlow(int sgn, float rowY, const vec3* cols, int pane, float delay, float speed, float vol);
    std::vector<vec2> trunk(int sgn, float rowY) const;
    void paneTail(int pane, std::vector<vec2>& pts) const;
    float riserX_ = 11.6f, gutterY_ = -9.75f;
    // the lead channels in the stone (always there, dark) and the window's leads (only seen glowing, at the drop)
    struct Channel { vec2 a, b; float d0, d1; bool cut; };
    std::vector<Channel> channels_;
    void placeChannels();
    float leadT_ = 0.f, leadAmp_ = 0.f; // a real drop: molten light runs through the whole lead network
    struct FlowInst { float a[4], b[4], c[4], d[4]; };
    std::vector<FlowInst> flowInst_, chanInst_;
    void buildFlows();
    struct Spark { vec2 p, v; float t, life, size; vec3 col; };
    std::vector<Spark> sparks_;
    void spit(vec2 p, vec2 spread, int n, const vec3& col, float up);

    // ---- Light sprites: dust in the beams, glints, candle flames and their wax, puffs of dust.
    struct SpriteInst { float a[4], b[4], c[4]; }; // x, y, rx, ry | rgb, intensity | kind, rot, pad, pad
    std::vector<SpriteInst> sprites_;
    struct Mote { vec2 p, v; float size, phase; };
    std::vector<Mote> motes_;
    struct Puff { vec2 p; float t, life, size; };
    std::vector<Puff> puffs_;
    struct Sparkle { vec2 p; float t, life, size; vec3 col; };
    std::vector<Sparkle> sparkles_;
    struct Candle { vec2 p; float h, phase; };
    std::vector<Candle> candles_;
    void placeCandles();

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
    GLuint progFlow_ = 0, flowVao_ = 0, flowVbo_ = 0;
    void drawFlows(const std::vector<FlowInst>& v, bool molten, int emit);
    GLuint progWall_ = 0, progCell_ = 0, progShard_ = 0, progSprite_ = 0, progShaft_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shardVao_ = 0, shardVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, emitT_, glowA_, glowB_, shaftT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    float floorY_ = -10.6f;
    void drawSprites();
    void drawCells(bool emit);
    void wallUniforms(GLuint p);

    // ---- Song, variant, the light.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec2 sun_{-14.f, 6.f}, sunFrom_{-14.f, 6.f}, sunTo_{-14.f, 6.f};
    float sunT_ = 1.f, sunDur_ = 4.f;
    int sunTurn_ = 0;
    void aimSun(float seconds);
    float sunI_ = 0.6f;  // the daylight's strength, eased

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropOn_ = false;          // the drop (or a chorus's rise) is on
    double dropBeat_ = 0.0;        // when it came (beats)
    float dropStart_ = 0.f;        // (seconds, without a song)
    float dropT_ = 0.f, dropA_ = 0.f, dropEnv_ = 0.f; // the sun striking: two eased stages (no jump)
    bool realDrop_ = false;        // a DROP (the full strike), not a chorus
    float flare_ = 0.f, flareA_ = 0.f; // a Tetris: the roses blaze
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    double beat_ = 0.0, lastGlintBeat_ = -1.0;
    float bps_ = 2.f;
    bool hasSong_ = false;
    float refreshAt_ = -1.f;
    bool sunMovedByRefresh_ = false;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

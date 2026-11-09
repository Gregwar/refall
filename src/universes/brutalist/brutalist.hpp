#pragma once
// BRUTALIST: raw concrete under a hard sun, in 3D; the blocks are cast concrete (see brutalist.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class BrutalistUniverse : public Universe {
public:
    ~BrutalistUniverse() override;
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

    // Instances (public for the geometry helpers in build.cpp).
    struct Box { float c[4], x[4], y[4], z[4], col[4]; }; // center + material, three half axes (+ params), color + seed
    struct Sprite { float p[4], c[4]; };                  // a mote of dust: position + radius, color + intensity
    struct Line { float a[4], b[4], c[4]; };              // a cable: from (+ width px) to (+ alpha), color

    // ---- Materials (mirrored in the shaders).
    enum Mat { M_CONC = 0, M_BLOCK = 1, M_LINING = 2, M_GLASS = 3, M_PAVE = 4, M_PLANT = 5, M_STEEL = 6, M_FAR = 7, M_EMIT = 8 };

private:
    // ---- Geometry helpers.
    static void box(std::vector<Box>& out, const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0,
                    float p2 = 0, float seed = 0);
    static void obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
                     float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0);
    static void beam(std::vector<Box>& out, const vec3& a, const vec3& b, float w, const vec3& col, int mat = 6);
    void line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha);
    void tuft(std::vector<Box>& out, const vec3& base, float size, int blades, uint64_t seed, const vec3& col);

    // ---- The stack: each cell remembers which cast piece it belongs to (the joints show between pieces only).
    int pid_[22][10] = {};
    float seeds_[22][10] = {};
    int nextPid_ = 1;

    // ---- Cleared blocks: popped out of the slot, tumbling down onto the plaza; game over: the stack falls out too.
    struct Flight {
        vec3 p0, cur, vel, spin;
        int type = 0;
        float t = 0, delay = 0, seed = 0;
        int kind = 0;   // 0 a cleared row's block, 1 game over
        float ang = 0;
        float spinK = 1.f, rest = -1.f, scale = 1.f; // (cleared blocks) tumble rate, time at rest, shrinking away
    };
    std::vector<Flight> flights_;

    // ---- The growing building: a pile of cast modules, one more for every cleared row, lowered by the tower crane.
    struct Module {
        vec3 pos;            // its center once in place
        int col = 0, level = 0, kind = 0; // 0 a dwelling, 1 a cantilevered one, 2 a stair tower's lift
        vec3 tone{1.f};
        float t = -1.f;      // < 0 waiting, 0..1 being lowered, 1 in place
        float landed = 99.f; // seconds since it landed
        float seed = 0;
    };
    std::vector<Module> plan_;   // every slot of the pile, in the order it fills
    int placed_ = 0;             // modules taken from the plan (lowered or in place)
    int queued_ = 0;             // rows cleared but not yet lowered
    void planGrowth();
    void buildModule(std::vector<Box>& out, const Module& m, const vec3& at, float alpha) const;
    struct Crane { float jib = 0.f, jibFrom = 0.f, hookFrom = 6.f, trolley = 20.f, tFrom = 20.f, tTo = 20.f, tT = 1.f, hook = 6.f; int carrying = -1; float wait = 0.f; };
    Crane crane_;
    void buildCrane(std::vector<Box>& out);

    // ---- Dust: motes drifting in the air, glittering where the sun crosses them; puffs from the impacts.
    struct Mote { vec3 p, v; float life = 0, max = 1, size = 1, k = 1; int kind = 0; float seed = 0; };
    std::vector<Mote> motes_;
    void puff(const vec3& at, int count, float spread, float up, float k);

    // ---- Building the place.
    void buildWorld();
    void buildBoard(const Game& g);
    vec3 pieceColor(int type) const;

    std::vector<Box> static_, boxes_, cells_;
    std::vector<Sprite> sprites_;
    std::vector<Line> lines_, cellLines_;
    size_t staticCount_ = 0;
    bool staticDirty_ = true;
    // the monument (the well's frame: sill, piers, lintel, lining, mass) is static_[monument0_, monument1_); in the
    // VBO it comes last, after the worldCount_ other boxes: a break (bare) draws the place without it
    size_t monument0_ = 0, monument1_ = 0, worldCount_ = 0;
    bool bare_ = false;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font font_, fontBold_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.5f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progSky_ = 0, progBox_ = 0, progDepth_ = 0, progSprite_ = 0, progLine_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint cubeVbo_ = 0, quadVbo_ = 0, quadVao_ = 0, staticVao_ = 0, staticVbo_ = 0, boxVao_ = 0, boxVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0,
           lineVao_ = 0, lineVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    static constexpr int SHADOW_N = 2048;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_, invVp_, sunVp_;
    vec3 eye_;
    void setupBoxVao(GLuint& vao, GLuint& vbo);
    void drawBoxes(GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count, bool upload);

    // ---- Song, variant.
    int variant_ = 0;
    struct Palette {
        vec3 zen, hor, glow, sun, sky, ground, conc, lining, glass, win, plant, steel, far, piece[7];
        float sunAz = 0, sunEl = 0, snow = 0, haze = 0, veil = 0, stars = 0, warmLights = 0, hudDark = 0;
    };
    Palette pal_, palT_;
    bool colSet_ = false;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;

    // ---- The sun: it moves on with every phase of the song, its shadows sweeping the place.
    float sunAz_ = 0.f, sunEl_ = 0.6f, sunAzTo_ = 0.f, sunElTo_ = 0.6f;
    int hour_ = 0;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, dropAmt_ = 0.f, swell_ = 0.f, swellPeak_ = 0.f;
    float kickIn_ = 0.f, refreshK_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beat_ = 0.f;
    int lastHalf_ = -1;
    float tetrisT_ = 99.f, clearT_ = 99.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0xB2u};
};

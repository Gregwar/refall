#pragma once
// DOCKS: a container port at dusk or night, in 3D; the blocks are shipping containers (see docks.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class DocksUniverse : public Universe {
public:
    ~DocksUniverse() override;
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

    // Instances (public for the geometry helpers in port.cpp).
    struct Box { float c[4], x[4], y[4], z[4], col[4]; }; // center + material, three half axes (+ params), color + seed
    struct Sprite { float p[4], c[4]; };                  // a light: position + radius, color + intensity
    struct Line { float a[4], b[4], c[4]; };              // a cable: from (+ width px) to (+ alpha), color

private:
    // ---- Materials (mirrored in the shaders).
    enum Mat { M_STEEL = 0, M_BOX = 1, M_CONCRETE = 2, M_BUILDING = 3, M_EMIT = 4, M_HULL = 5, M_HOUSE = 6, M_DARK = 7,
               M_CAB = 8, M_DECK = 9 };

    // ---- Geometry helpers.
    static void box(std::vector<Box>& out, const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0,
                    float p2 = 0, float seed = 0);
    static void obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
                     float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0);
    static void beam(std::vector<Box>& out, const vec3& a, const vec3& b, float w, const vec3& col, int mat = 0);
    void line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha);
    void sprite(const vec3& p, float r, const vec3& col, float k);

    // ---- The stack: each cell remembers a small seed (dents, stencils).
    float seeds_[22][10] = {};

    // ---- The crane holding the falling piece: a lifting beam over the piece, its extents eased.
    struct Spreader {
        float x0 = -1, x1 = 1, y = 12, cx = 0;
        bool on = false;
        float hang[4] = {0, 0, 0, 0}; // per column under the beam (relative), how far down its hanger reaches
        int hangCols = 0;
        float rel = -1.f;   // released: rising away (seconds), -1 while holding
        float relY = 0.f;
    };
    Spreader spr_;

    // ---- Cleared containers on their way: lifted back out of the well, carried to a ship and set on its deck.
    struct Flight {
        vec3 p0, p1, p2, cur;  // start, the swift lift's end, the deck slot
        vec3 col;
        int ship = -1, slot = -1;
        float t = 0, dur = 4.f, delay = 0, seed = 0;
        int kind = 0;          // 0 a cleared row's container, 1 game over (lifted out of frame)
        float sizeK = 1.f;     // 1 a cell's cube .. its slot's box once landed
    };
    std::vector<Flight> flights_;

    // ---- The ships at their berths (0 left, 1 right): bow toward the middle, deck house outward.
    struct Ship {
        float x = 0;            // center
        int state = 0;          // 0 docked, 1 leaving, 2 arriving, 3 away
        float t = 0, wait = 0;
        int seed = 1;
        vec3 hull{0.1f}, band{1.f};
        std::vector<signed char> slot; // per cargo slot: -1 free, else a color index (0..6 pieces, 7.. neutral)
        std::vector<float> landT;      // seconds since the slot's container landed
        std::vector<int> order;        // free slots in the order they fill
        int reserved = 0;
        float wake = 0.f;
    };
    Ship ships_[2];
    static constexpr int BAYS = 12, ROWS = 7, TIERS = 4;
    void newShip(int i, bool docked);
    vec3 slotPos(int ship, int slot) const;
    float shipBerthX(int i) const { return i == 0 ? -27.5f : 27.5f; }
    int reserveSlot(int ship);

    // ---- The port's moving parts.
    struct Crane {
        float x = 0, x0 = 0;      // along the far quay
        float trolley = 0.5f, tFrom = 0.5f, tTo = 0.5f, tT = 1.f, tDur = 2.f;   // along the boom (0 back .. 1 tip)
        float hoist = 0.5f, hFrom = 0.5f, hTo = 0.5f;                             // the spreader's height (0 up .. 1 down)
        bool loaded = false;
        int colorIdx = 0, ship = 0;
        float blink = 0.f, beacon = 0.f;
        float raise = 0.f;        // 1: an idle crane, its boom raised
    };
    std::vector<Crane> cranes_;
    struct Carrier { float z = 0, x = 0, from = 0, to = 0, t = 1, dur = 6, wait = 2; bool loaded = true; int col = 0; float yaw = 0; };
    std::vector<Carrier> carriers_;
    struct Rtg { float x = 0, z = 0, from = 0, to = 0, t = 1, dur = 8, wait = 3; };
    std::vector<Rtg> rtgs_;

    // ---- Lights: lamps on masts, cranes and ships; on in groups by the song's level, switched on in sequence on the drop.
    struct Lamp {
        vec3 p, col;
        float r = 1.f, k = 1.f;   // sprite radius, intensity
        float range = 0.f;        // > 0: it lights the scene (a point light) of this reach
        int group = 0;            // 0 always, 1 mid, 2 peak
        float on = 0.f;           // eased 0..1
        float order = 0.f;        // the drop's sequence (distance from the well)
    };
    std::vector<Lamp> lamps_;
    std::vector<vec4> pointP_, pointC_;

    // ---- Building the port.
    void buildPort();
    void buildCrane(std::vector<Box>& out, const Crane& c, bool moving) const;
    float craneHook(const Crane& c) const;
    void buildShip(std::vector<Box>& out, int i);
    void buildBoard(const Game& g);
    void buildSpreader(const Game& g, float dt);
    void buildMoving(float t);
    vec3 pieceColor(int type) const;
    vec3 cargoColor(int idx) const;

    std::vector<Box> static_, boxes_, cells_;
    std::vector<Sprite> sprites_;
    std::vector<Line> lines_, cellLines_, cones_;
    size_t staticCount_ = 0;
    bool staticDirty_ = true;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font font_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.5f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progSky_ = 0, progWater_ = 0, progBox_ = 0, progSprite_ = 0, progLine_ = 0, progCone_ = 0, progVeil_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint cubeVbo_ = 0, quadVbo_ = 0, quadVao_ = 0, staticVao_ = 0, staticVbo_ = 0, boxVao_ = 0, boxVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0,
           lineVao_ = 0, lineVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    ugl::Target sceneT_, reflT_, bloomA_, bloomB_;
    mat4 vp_, invVp_, mirror_;
    vec3 eye_;
    void setupBoxVao(GLuint& vao, GLuint& vbo);
    void drawBoxes(GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count, const mat4& vp, float clipY, bool upload);
    void drawSprites(const mat4& vp, float clipY);
    void drawLines(const std::vector<Line>& l, const mat4& vp);
    void setScene(GLuint p, const mat4& vp, float clipY);
    void drawWorld(const mat4& vp, bool reflection);

    // ---- Song, variant.
    int variant_ = 0;
    struct Palette {
        vec3 zen, mid, hor, glow, sun, fog, amb, ground, key, water, lampA, lampB, quay, crane, crane2, piece[7], hull[3];
        float sunAz = 0, sunEl = 0, stars = 0, fogD = 0, moon = 0, haze = 0, lampK = 1, warm = 0;
    };
    Palette pal_, palT_;
    bool colSet_ = false;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, swell_ = 0.f, swellPeak_ = 0.f;
    float kickIn_ = 0.f, refreshK_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beat_ = 0.f;
    int lastHalf_ = -1;
    float tetrisT_ = 99.f;
    int sideFlip_ = 0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the port, no stack, no beam, no shade
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0xD0C5};
};

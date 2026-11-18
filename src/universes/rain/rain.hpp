#pragma once
// RAIN: a colorful street at dusk in the rain; the rain pours over the blocks, the falling piece shelters the cells
// under it, cleared rows are thrown out to two piles on the sides where the rain falls on them too (see rain.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RainUniverse : public Universe {
public:
    ~RainUniverse() override;
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
    // ---- Shapes and tiles (pixels).
    struct Shape { float g[4], c[4], k[4]; };
    struct Tile { float g[4], c[4], p[4], e[4], f[4]; };
    std::vector<Shape> shapes_;
    std::vector<Tile> tiles_;
    void box(float cx, float cy, float hx, float hy, const vec3& c, float a, float round = 0.f, float outline = 0.f);
    void disc(float cx, float cy, float rx, float ry, const vec3& c, float a, float soft = 0.f);
    void seg(float x0, float y0, float x1, float y1, float w, const vec3& c, float a, float tail = 0.f);
    void tile(float bx, float by, float hx, float hy, const vec3& c, float a, float seed, float wet, bool plain, float angle = 0.f);
    void flushShapes();
    void flushTiles();
    vec2 px(float x, float y) const;
    float unitPx_ = 45.f;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    void drawPreviews(const Game& g);

    // ---- The rain: near streaks with physics, splashes, drips running down the faces.
    struct Drop { float x, y, vx, vy, len, a, endY; bool drip; };
    struct Spark { float x, y, vx, vy, t, life, r; };      // a splash's droplet
    struct Crown { float x, y, t, w; };                      // a splash's flat crown at the impact
    struct Run { float x, y, side, v, t; int body; float sl; }; // a drip running down a face (or sliding down a heaped tile's slope)
    std::vector<Drop> drops_;
    std::vector<Spark> sparks_;
    std::vector<Crown> crowns_;
    std::vector<Run> runs_;
    float spawnAcc_ = 0.f;
    // the solids the rain meets, rebuilt every frame: per well column the cells' [bottom, top] spans
    struct Span { float b, t; int kind; int cx, cy; };      // kind 0 stack (cx, cy its cell), 1 piece, 2 floor
    std::vector<Span> cols_[10];
    void buildSolids(const Game& g);
    bool solidAt(float x, float y) const;
    // the highest surface at x at or below y (board units); -1e9 when none (off screen)
    float surfaceBelow(float x, float y, int* kind, int* cx, int* cy, bool* inside) const;
    void impact(float x, float y, float speed, int kind, int cx, int cy);
    void stepRain(float dt, float rdt);
    void drawRain(bool well);

    // ---- The heaps: cleared tiles thrown out of the well, then tumbling rigid boxes (a small 2D solver of our own)
    // settling into heaps on the street in the side bands, where the rain falls on their tilted faces.
    struct Body {
        int id, type, side;
        float seed, half;              // half size (board units)
        float x, y, vx, vy, a, w;      // center, velocity, angle (radians, counterclockwise), angular velocity
        float wet, fade, fadeRate;
        bool fading, solid, asleep;
        float still, awake;            // seconds nearly still, seconds awake (both put a body to sleep)
        // the throw: a scripted arc (Hermite) out of the well to its release over the band, then physics
        bool thrown;                   // waiting in its row for its turn (then thrown: a ballistic impulse)
        float t, delay, x0, y0, tx1, ty1;
        bool inBand;                   // reached its band (its walls hold it from then on)
        float soak, slump;             // how soggy it is (0 firm .. 1 pulp), how far it has sunk since it last moved
        int hits;                      // drops that hit it this frame (they soak it)
    };
    std::vector<Body> bodies_;
    int nextBody_ = 1;
    float bandIn_ = 11.9f, bandOut_ = 21.f, ground_ = -11.6f;
    float physAcc_ = 0.f;
    std::vector<int> bins_[96];                 // the heaped tiles by column of one unit (the rain's queries)
    float binX0_ = -48.f;
    void buildBins();
    float leftOut_ = 21.f;                      // the left heap's outer wall (clear of the HUD's corner lines)
    vec2 signPos_[2];                           // the blade signs over the heaps (board units)
    float flood_ = 0.f;                         // the storm's flooding street
    std::vector<vec4> textRects_;      // the HUD's bottom corner lines (board units): the heaps dim under them
    void eject(int x, int y, int type, float delay);
    void stepHeaps(float dt);
    void physicsStep(float h);
    void drawHeaps();
    Body* bodyById(int id);
    float bodyTop(const Body& b, float x, float* slope) const; // where a vertical line at x enters the box from above (-1e9: misses)
    bool bodyInside(const Body& b, float x, float y) const;
    float textCover(float x, float y) const;

    // ---- The foreground: some of a clear's tiles (the ones nearest the walls) come out through the glass toward the
    // camera: they fall along the well's outer wall (clear of the HUD's lettering), grow as they come closer, land
    // with a big splash at the bottom of the picture beside the well, and melt there in the rain (soaking, sagging,
    // crumbling, slumping to mush in a puddle of their color) until nothing is left: they never pile up.
    struct Fore {
        int type, side;
        float seed, half;              // half size at scale 1 (board units)
        float x, y, vx, vy, a, w, s;   // center (board units), velocity, angle, angular velocity, scale (closer: larger)
        float sMax, t, delay, x0, y0, a0, xOut, tx, aT, T;
        int stage;                     // 0 waiting in its cell, 1 out through the glass, 2 falling, 3 bouncing, 4 melting
        float melt, rate, wet, gone;   // melt 0 firm .. 1 dissolved; gone: the puddle's fading after
        float crumbAcc, rainAcc, wob;
    };
    struct Crumb { float x, y, vx, vy, a, w, r, t, life; int type; float seed; bool rest; };
    std::vector<Fore> fores_;
    std::vector<Crumb> crumbs_;
    std::vector<Spark> fsparks_;      // the foreground's splashes (in front of everything)
    std::vector<Crown> fcrowns_;
    std::vector<vec4> hudRects_;      // every HUD line beside the well (board units): the foreground keeps clear of them
    float foreGround_ = -11.2f;
    struct Streak { float x, y, v, len, w, a; };
    std::vector<Streak> nearRain_;    // a few big out-of-focus streaks right in front of the camera
    bool foreTake(int side);
    void ejectFore(int x, int y, int type, float delay);
    float foreExt(const Fore& f, float s) const;
    float foreClampX(int side, float x, float y, float ext) const;
    void foreSplash(float x, float y, float s, float strength, const vec3& col);
    void stepFore(float dt, float rdt);
    void drawFore();

    // ---- The board.
    float wet_[Game::H][Game::W] = {};
    float seeds_[Game::H][Game::W] = {};
    vec3 pieceColor(int type) const;
    void drawBoard(const Game& g);

    // ---- GL.
    GLuint progBg_ = 0, progShape_ = 0, progTile_ = 0;
    GLuint quadVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, tileVao_ = 0, tileVbo_ = 0;
    mat4 vp_;

    // ---- Song, staging.
    int variant_ = 0;
    uint64_t seed_ = 1;
    int level_ = 1;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float rain_ = 0.4f;      // the rain's intensity shown (0 drizzle .. 1 downpour)
    float storm_ = 0.f;      // the drop's thunderstorm (0..1, held for the whole drop section)
    float lights_ = 1.f;     // the setup / teardown: how many of the street's lights are on (0..1)
    float shower_ = 1.f;     // the setup / teardown: the rain starting, stopping (scales the near rain)
    float glow_ = 0.8f;
    float flash_ = 0.f;      // the soft lightning glow
    vec2 flashPos_{0, 8};
    long flashBar_ = -1000;
    float strikeT_ = 9.f;    // seconds since the last lightning strike (a double pulse)
    float strikeAmp_ = 0.f;
    // the drop's colour mood: a grade of the whole street (not the board, the pieces, the HUD), see render()
    struct Grade { float c = 1.f, s = 0.f, sat = 1.f, duo = 0.f, expo = 1.f; vec3 lo{0, 0, 0}, hi{1, 1, 1}; };
    Grade grade_;
    float gradeK_ = 0.f;     // how far into the drop's mood (0 the street's own colours .. 1 the mood)
    float gradeAB_ = 0.f;    // which of the variant's two moods (they alternate every 8 bars, gliding)
    double dropBeat0_ = 0.0; // the beat the drop started on
    bool wasDrop_ = false;
    vec3 graded(const vec3& c) const; // the grade on the CPU (the rain's tint, the lights caught on the tiles)
    float neonGain_ = 1.f;
    float sweepX_ = 0.f;     // the colored light sweeping across the wet street
    float kickIn_ = 0.f;
    float time_ = 0.f, rainT_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float dimT_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{777};
};

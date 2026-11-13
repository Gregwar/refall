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
    void drawHud(const Game& g, const std::vector<HudRect>& panels);
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
        float paint, pr, pg, pb;       // the paint left on it and its color (mixed with what ran down from above)
        float dr, dg, db, drip;        // the paint running down its face from the tile above
        int hits;                      // drops that hit it this frame (they wash the paint off)
    };
    std::vector<Body> bodies_;
    int nextBody_ = 1;
    float bandIn_ = 11.9f, bandOut_ = 21.f, ground_ = -11.6f;
    float physAcc_ = 0.f;
    struct PDrip { float x, y, vy, r, g, b; };   // a drop of paint falling off a heaped tile
    std::vector<PDrip> pdrips_;
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
    float glow_ = 0.8f;
    float flash_ = 0.f;      // the soft lightning glow
    vec2 flashPos_{0, 8};
    long flashBar_ = -1000;
    float strikeT_ = 9.f;    // seconds since the last lightning strike (a double pulse)
    float strikeAmp_ = 0.f;
    long surgeBeat_ = -1000;
    float surge_ = 0.f;      // the drop's neon surging on the beats (and flickering)
    float neonGain_ = 1.f;
    float sweepX_ = 0.f;     // the colored light sweeping across the wet street
    float kickIn_ = 0.f;
    float time_ = 0.f, rainT_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float dimT_ = 0.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{777};
};

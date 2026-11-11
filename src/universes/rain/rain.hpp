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

private:
    // ---- Shapes and tiles (pixels).
    struct Shape { float g[4], c[4], k[4]; };
    struct Tile { float g[4], c[4], p[4]; };
    std::vector<Shape> shapes_;
    std::vector<Tile> tiles_;
    void box(float cx, float cy, float hx, float hy, const vec3& c, float a, float round = 0.f, float outline = 0.f);
    void disc(float cx, float cy, float rx, float ry, const vec3& c, float a, float soft = 0.f);
    void seg(float x0, float y0, float x1, float y1, float w, const vec3& c, float a, float tail = 0.f);
    void tile(float bx, float by, float hx, float hy, const vec3& c, float a, float seed, float wet, bool plain);
    void flushShapes();
    void flushTiles();
    vec2 px(float x, float y) const;
    float unitPx_ = 45.f;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Type.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        float kern[96][96];
        void* info = nullptr;
    };
    Font med_, light_;
    void buildFont(Font& f, const unsigned char* ttf, float px, int atlas);
    float capWidth(const Font& f, const std::string& s, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float soft = 0.f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    void drawPreviews(const Game& g);

    // ---- The rain: near streaks with physics, splashes, drips running down the faces.
    struct Drop { float x, y, vx, vy, len, a, endY; bool drip; };
    struct Spark { float x, y, vx, vy, t, life, r; };      // a splash's droplet
    struct Crown { float x, y, t, w; };                      // a splash's flat crown at the impact
    struct Run { float x, y, side, v, t; };                  // a drip running down an exposed face
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

    // ---- The side piles.
    struct PileBlock { int id, type; float seed, h, y, wet, fadeRate; bool arrived, fading; };
    int nextBlock_ = 1;
    static constexpr int PILE_COLS = 9;
    std::vector<PileBlock> pile_[2][PILE_COLS];
    float bandX0_[2] = {12.f, 12.f};       // each band's inner edge (board units), cells growing outward
    float pileCell_ = 0.92f;
    float ground_ = -11.6f;
    int pileMax_ = 7;
    int colCap_[2][PILE_COLS] = {};        // each column's height, kept under the HUD's corner lines
    float pileColX(int side, int c) const;
    float pileTop(int side, int c) const;  // the top of its arrived blocks
    int pileColAt(float x, int* side) const;
    struct Fly { float x0, y0; int side, col, id, type; float t, delay, dur, arc; };
    std::vector<Fly> flies_;
    void eject(int x, int y, int type, float delay);
    void stepPiles(float dt);
    void drawPiles();
    void drawFlies();

    // ---- The board.
    float wet_[Game::H][Game::W] = {};
    float seeds_[Game::H][Game::W] = {};
    vec3 pieceColor(int type) const;
    void drawBoard(const Game& g);

    // ---- GL.
    GLuint progBg_ = 0, progShape_ = 0, progTile_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, tileVao_ = 0, tileVbo_ = 0, textVao_ = 0, textVbo_ = 0;
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

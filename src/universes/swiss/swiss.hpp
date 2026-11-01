#pragma once
// SWISS POSTER: the International Typographic Style, a poster recomposed by the music (see swiss.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SwissUniverse : public Universe {
public:
    ~SwissUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override {}
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    // ---- Flat shapes (pixels): rectangles, discs, ring arcs, each clipped to a rectangle.
    struct Shape { float r[4], c[4], k[4], clip[4]; };
    std::vector<Shape> shapes_;
    void rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a);
    void rectPxClip(float x0, float y0, float x1, float y1, const vec3& c, float a, const float clip[4]);
    void rectB(float x0, float y0, float x1, float y1, const vec3& c, float a); // board units (y up)
    void discB(float cx, float cy, float r, const vec3& c, float a);
    void ringPx(float cx, float cy, float rIn, float rOut, float fill, const vec3& c, float a);
    void flushShapes();
    vec2 px(float x, float y) const; // board -> pixels
    float unitPx_ = 45.f;            // pixels per board unit

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
    Font bold_, reg_;
    void buildFont(Font& f, const unsigned char* ttf, float px, int atlas);
    // Width of a string in cap heights (track in cap heights too).
    float capWidth(const Font& f, const std::string& s, float track) const;
    // Quads for a string: (x, base) its baseline's start in pixels, rot 1 = turned 90 degrees, reading upward.
    void textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, int rot,
                   std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, bool adaptive = false);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- The poster's composition: elements eased between layouts set on the bar grid.
    enum Slot { E_TITLE, E_ARTIST, E_SECTION, E_FIELD0, E_FIELD1, E_FIELD2, E_DISC, E_RULE0, E_RULE1, E_RULE2,
                E_INFO, E_BULLET, E_COUNT };
    struct Target {
        bool on = false;
        float v[4] = {0, 0, 0, 0}; // field: x0 y0 x1 y1 | text: x, top, cap, - | disc: cx cy r - | rule: x0 y x1 -
        int color = 0;             // 0 ink, 1..3 the palette's colors
        int rot = 0;
        std::string text;
    };
    struct Element {
        Target cur, from, to;
        float alpha = 0.f, alphaFrom = 0.f;
        float t = 1.f, dur = 1.f;
        bool twoPhase = false, swapped = false;
        float ruleBase = 0.f;      // rules: their place in the layout (the beat shifts them along the grid)
    };
    Element el_[E_COUNT];
    void compose(long bar, float dur, bool instant);
    void retarget(int slot, const Target& t, float dur, bool instant);
    void updateElements(float dt);
    void drawPoster();
    std::string title_, artist_, info1_, info2_, section_;
    std::vector<std::string> titleWords() const;
    long bar_ = -1000, step_ = -1000;
    int composeCount_ = 0;
    long tetrisUntil_ = -1;

    // ---- Pieces: one id per piece (its cells merge into one flat shape), the palette it locked in.
    int ids_[22][10] = {};
    int pal_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct Wipe { float y; int type[10], pal[10], mask[10]; float t, delay, dir; };
    std::vector<Wipe> wipes_;
    vec3 pieceColor(int type, int pal) const;
    void drawCell(float x, float y, float half, int mask, const vec3& c, float a, bool snap = true);
    void drawBoard(const Game& g);

    // ---- GL.
    GLuint progShape_ = 0, progText_ = 0, progComp_ = 0;
    GLuint quadVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target posterT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant, palettes.
    int variant_ = 0;
    uint64_t seed_ = 1;
    int palIdx_ = 1;           // the section's palette (pieces lock in it)
    vec3 palShown_[4];         // the poster's colors (ink, c0, c1, c2), eased
    vec3 ground_{1.f}, ink_{0.f};
    bool palInit_ = false;

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false, dropPending_ = false;
    float dropDur_ = 1.f;
    float kickIn_ = 0.f, kickShown_ = 0.f;
    float lockLine_ = 0.f, lockX0_ = 0.f, lockX1_ = 0.f, lockY_ = 0.f;
    float time_ = 0.f;
    float bpm_ = 120.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

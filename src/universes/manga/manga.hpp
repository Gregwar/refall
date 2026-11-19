#pragma once
// MANGA: a manga's color page drawn in ink, screentones and washes of color, inked and colored panel by panel
// over the song (see manga.cpp).
#include <algorithm>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/manga/art.hpp"

class MangaUniverse : public Universe {
public:
    ~MangaUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override { (void)strength; }
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
    using V2 = manga::V2;

    // ---- Ink: triangles in screentones (pixels), flushed in one draw.
    std::vector<float> verts_;
    struct ToneP { int kind; float cell, angle; vec2 origin; int clip; };
    void vtx(float x, float y, float dens, const ToneP& t, const vec3& c, float a);
    void tri(const V2& a, const V2& b, const V2& c, const ToneP& t, float da, float db, float dc, const vec3& col, float al);
    // A triangle with a color and an opacity per vertex (washes, gradients).
    void triC(const V2& a, const V2& b, const V2& c, const ToneP& t, float d, const vec3* cols, const float* als);
    // Board units in, pixels out.
    void fillPoly(const std::vector<V2>& pts, const manga::Tone& tone, float densK, const vec3& col, float a, int clip,
                  bool strip = false, const V2* pivot = nullptr, float rotA = 0.f);
    void strokeLine(const std::vector<V2>& pts, float widthPx, float taper, bool closed, float upTo, const vec3& col, float a,
                    int clip, const V2* pivot = nullptr, float rotA = 0.f);
    void rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a);
    // A hand-inked line from a to b (pixels): a brush stroke, its shape the seed's (see manga.cpp).
    void brushPx(vec2 a, vec2 b, float widthPx, uint64_t seed, float from, float to, float loose, const vec3& col, float alpha, int clip);
    // A panel's frame (board units, its corners clockwise): each edge a brush stroke, ruled in along them (upTo 0..1).
    void inkQuad(const V2* q, float widthPx, uint64_t seed, float upTo, const vec3& col, float a);
    void discPx(float cx, float cy, float r, const vec3& c, float a, int clip = -1);
    void flushInk();
    vec2 px(const V2& b) const; // board -> pixels
    float unitPx_ = 45.f, s_ = 1.f; // pixels per board unit, the 1080p scale

    // ---- Lettering: the sound effects' (the HUD's is the host's, hudLine).
    struct Glyph { int cp; float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f, pad = 8.f;
        std::vector<Glyph> g; // sorted by codepoint
        void* info = nullptr;
        const Glyph* find(int cp) const;
    };
    Font dela_;
    void buildFont(Font& f, const unsigned char* ttf, float px, const std::vector<int>& cps);
    float textWidth(const Font& f, const std::string& s, float capPx, float track) const;
    // Quads for a string set from (x, base) along a baseline turned by `angle` (radians, clockwise on screen).
    void textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, float angle,
                   std::vector<float>& out) const;
    struct TextStyle { vec3 fill; float fillA = 1.f; vec3 line; float lineA = 0.f, linePx = 0.f; vec3 halo; float haloA = 0.f, haloPx = 0.f; };
    void drawText(const Font& f, const std::vector<float>& q, float capPx, const TextStyle& st, float alpha);
    // the HUD's panels, gauge, caption box and balloon (around its lines as lettered: boxes, f.hudBoxes)
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<vec4>* boxes, const std::vector<HudRect>& panels);

    // ---- The page.
    manga::Page page_;
    float scoreBottom_ = -6.5f;
    std::vector<float> inkT_;  // per element: < 0 pencil, else seconds since its inking began
    std::vector<float> inkQ_;  // inkings waiting to start (seconds of delay)
    size_t nextInk_ = 0;
    float resetT_ = -1.f, resetDur_ = 1.f; // a new song: the ink fading off the page
    void resetPage();
    void inkNext(float delay);
    void drawPanels(float t, float wm, float wp);
    void drawElement(const manga::Element& e, float inkT, float fade, float rotA);
    void drawBurst(float amt);
    void drawFrames(float shatter, bool art); // the drawing panels' frames (art) or the HUD's and the well's
    // the setup: each drawing panel ruled in then drawn out of the blank paper in turn (0..1); the teardown: each
    // goes dark in turn (0..1), the ink coming down over it like a curtain
    float setup_ = 1.f, teardown_ = 0.f;
    float panelIn(int panel) const;
    float panelOut(int panel) const;
    void drawSplash(float amt, float t, const vec3& color);
    void setClips();
    // the panels' clips, then spare slots for the drop's own panels (set per frame)
    static constexpr int CLIP_SLOTS = manga::P_COUNT + 8;
    float clipEq_[CLIP_SLOTS][4][3];
    void setClip(int slot, const V2* quad);

    // ---- Pieces: one id per piece (its cells share one outline).
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void drawBoard(const Game& g, float t);
    // its flat color, its ink (seams and outline), or both (the stack's colors all go down before its ink, so a
    // piece's outline is never covered by its neighbor's color)
    enum { PIECE_FILL = 1, PIECE_INK = 2, PIECE_ALL = 3 };
    void drawPiece(const int cells[][2], int n, int type, float dx, float dy, float alpha, float inkK, int parts = PIECE_ALL);
    // brush strokes along a piece's outline, one per straight run of it (cells in rows below minRow left out)
    void inkOutline(const int cells[][2], int n, int minRow, float dx, float dy, float widthPx, uint64_t seed, float loose,
                    const vec3& col, float a);
    struct Crumb { V2 p, v; float r, t, life; int type; };
    std::vector<Crumb> crumbs_;
    struct Fade { int x, y, type; float t; int mask; };
    std::vector<Fade> fades_;  // cleared cells breaking up, and the stack at the end
    struct Impact { V2 a, b; float t; };
    std::vector<Impact> impacts_;

    // ---- Sound effects lettered on the page.
    // Styles: a white letter outlined in ink on a paper margin, a black letter on a paper margin, a thin outlined
    // letter (silence), a bold color outlined in ink on a paper margin, the crossing's red, a white letter outlined
    // in ink with no margin (on an ink ground).
    enum { SFX_OUTLINED = 0, SFX_BLACK = 1, SFX_THIN = 2, SFX_COLOR = 3, SFX_LAMP = 4, SFX_WHITE = 5 };
    // fx: the drop effect it belongs to (-1: none; it lasts as long as that effect), vel: sliding (units/s)
    struct Sfx { std::string text; V2 at; float size, angle, t, life, in; bool vertical; int style; int fx = -1; V2 vel{0, 0}; };
    std::vector<Sfx> sfx_;
    Sfx& addSfx(const std::string& text, V2 at, float size, float angle, float life, bool vertical, int style, float in = 0.35f);
    void drawSfx(const Sfx& s, float alpha);

    // ---- GL.
    GLuint progInk_ = 0, progText_ = 0, progPaper_ = 0;
    GLuint inkVao_ = 0, inkVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0;
    int samples_ = 4;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    // The colors: the paper, the ink, the spot (the HUD's accent), the pencil, then the palette's hues, the seven
    // piece colors, the drop's burst (center, middle, edge) and the sound effects' fill.
    enum { K_PAPER = 0, K_INK, K_SPOT, K_PENCIL, K_HUE, K_PIECE = K_HUE + manga::H_COUNT, K_BURST = K_PIECE + 7, K_SFX = K_BURST + 3, K_COUNT };
    vec3 cur_[K_COUNT], tgt_[K_COUNT]; // eased into on a new song
    vec3 &paper_ = cur_[K_PAPER], &ink_ = cur_[K_INK], &spot_ = cur_[K_SPOT], &pencil_ = cur_[K_PENCIL];
    float colorRate_ = -1.f;
    float toneK_ = 1.f;        // the variant's screentone scale (coarser newsprint)
    bool print_ = false;       // washes printed in CMYK halftone
    static vec3 shadeOf(const vec3& c);
    vec3 col(int c) const {
        if (c >= manga::C_PAL) {
            const int h = (c - manga::C_PAL) % manga::C_SHADE;
            const vec3& v = cur_[K_HUE + std::min(h, (int)manga::H_COUNT - 1)];
            return c >= manga::C_PAL + manga::C_SHADE ? shadeOf(v) : v;
        }
        return c == manga::C_PAPER ? paper_ : c == manga::C_SPOT ? spot_ : ink_;
    }
    vec3 hue(int h) const { return cur_[K_HUE + h]; }
    vec3 pieceColor(int type) const { return cur_[K_PIECE + std::clamp(type, 0, 6)]; }

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropT_ = -1.f;       // seconds since the drop's downbeat (-1: none)
    bool donShown_ = false;
    float splash_ = 0.f;       // the drop's weight 0..1 (the strongest of its effects)
    // ---- The drop's effects: one per drop (a shuffled rotation, never the same twice in a row), a new one every
    // 32 beats of a long drop.
    enum { DF_SPLASH = 0, DF_DASH, DF_PANELS, DF_IMPACT, DF_SHOJO, DF_BARRAGE, DF_COUNT };
    int fx_ = -1, lastFx_ = -1;
    int bag_[DF_COUNT] = {}, bagPos_ = DF_COUNT;
    float fxAmt_[DF_COUNT] = {};   // each effect's weight 0..1 (eased in and out)
    float fxT_[DF_COUNT] = {};     // seconds since it started
    double fxBeat0_ = 0.0;         // the beat it started on
    long fxStep_ = -1000;          // its own beat counter
    float jolt_ = 0.f;             // the balloons' jolt as a new one bursts in (barrage)
    V2 joltDir_{0, 0};
    int pickFx();
    void startFx(int fx, double beat);
    void fxBeat(int fx, long n, double beat, float bpm);
    // art modes: 0 as drawn, 1 negative (the ink's lines in paper, on an ink ground), 2 the lines only (ink and paper)
    int artMode_ = 0;
    std::vector<float> elemBox_; // per element: its bounds (board units), x0 y0 x1 y1
    void drawArtAt(int srcPanel, int clipSlot, V2 focus, float zoom, V2 centerB, int mode, float alpha, bool inked, const float* cullPx);
    void gradRect(float x0, float y0, float x1, float y1, const vec3& top, const vec3& bottom, float a); // board units
    void starburst(V2 c, float rx, float ry, int spikes, float inner, uint64_t seed, const vec3& fill, float a, float outlinePx);
    void sparkle(V2 c, float r, float rot, const vec3& fill, const vec3& line, float a);
    void flower(V2 c, float r, float rot, float open, const vec3& petal, const vec3& line, float a);
    void drawDash(float a, float t, double beat);
    void drawPanelBreak(float a, double beat);
    void drawImpact(float a, double beat);
    void drawShojo(float a, double beat);
    void drawBarrage(float a, double beat);
    void drawBalloons(float a);
    struct Ring { V2 c; float t, life, r0, r1; };
    std::vector<Ring> rings_;
    struct Shard { V2 p, v; float rot, vr, size, t, life; int kind; };
    std::vector<Shard> shards_;
    struct Pop { V2 at; float t, life, size, angle; int kind; std::string text; };
    std::vector<Pop> pops_;     // barrage: sound effects in starburst balloons
    bool negative_ = false;     // drawing elements inverted (white on ink)
    int clipOverride_ = -1;     // drawing elements into another clip slot
    float burst_ = 0.f;        // its burst of color (or a quad's)
    float quadT_ = -1.f;     // seconds since a quad
    float kickIn_ = 0.f;
    long step_ = -1000;        // the two-beat grid
    int rumble_ = 0;
    float time_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    bool bare_ = false; // a break: the art panels alone, no well, HUD panels, gauge or pieces
    float chime_ = 0.f, chimeV_ = 0.f;
    float lampT_[2] = {0.f, 0.f};
    std::string section_;
    Bridge bridge_;
    Rng rng_{777};
};

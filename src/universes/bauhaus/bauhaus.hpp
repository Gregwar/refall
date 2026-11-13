#pragma once
// BAUHAUS: a Bauhaus poster of circles, triangles and squares cut out of colored paper in relief, recomposed by the
// music (see bauhaus.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class BauhausUniverse : public Universe {
public:
    ~BauhausUniverse() override;
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
    // ---- Flat shapes (pixels): rotated rectangles, disc sectors and rings, triangles, each clipped to a rectangle.
    enum Kind { K_BOX = 0, K_DISC = 1, K_TRI = 2 };
    struct Shape { float a[4], c[4], k[4], e[4], clip[4]; };
    std::vector<Shape> shapes_;
    void shape(Kind kind, float cx, float cy, float hx, float hy, float rot, const vec3& c, float a,
               float p1 = 0.f, float p2 = 0.f, float p3 = 0.f, const float* clip = nullptr);
    void rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a, const float* clip = nullptr);
    void flushShapes();
    vec2 px(float x, float y) const; // board -> pixels
    float unitPx_ = 45.f;            // pixels per board unit

    // ---- Type: the words printed on the poster (the HUD is lettered by the host: hudLine).
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
    float capWidth(const Font& f, const std::string& s, float track) const; // in cap heights
    // Quads for a string: (x, base) its baseline's start in pixels, turned by `angle` (radians, counterclockwise).
    void textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, float angle,
                   std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha);
    void drawHud(const Frame& f); // the HUD's panels, gauge and small forms

    // ---- The poster: forms eased between compositions set on the bar grid.
    enum Slot { F_CIRCLE, F_RING, F_SQUARE, F_TRIANGLE, F_HALF, F_QUARTER, F_BAR0, F_BAR1, F_LINE0, F_LINE1,
                F_DOT0, F_DOT1, F_DOT2, F_TITLE, F_NUMBER, F_INFO, F_COUNT };
    // A form's state: center, size (radius / half side / half length), second size (thickness / height), rotation
    // (radians), alpha, and its color as a blend of two palette colors.
    struct Form {
        float x = 0, y = 0, s = 0, s2 = 0, rot = 0, a = 0;
        vec3 col{0.f};
    };
    struct Element {
        Form cur, from, to;
        float t = 1.f, dur = 1.f;
        bool twoPhase = false, swapped = false; // a far move or a new text: out where it was, in where it goes
        std::string text, nextText;
    };
    Element el_[F_COUNT];
    void compose(long bar, float dur, bool instant);
    void setTarget(int slot, Form to, float dur, bool instant, const std::string& text = std::string());
    void updateElements(float dt);
    void drawPoster(const MusicState& music);
    void drawForm(int slot, const Form& f, float spin);

    // ---- Paper cut-outs: every form of the poster is a card of thick paper laid on the sheet at its own height
    // (casting a soft shadow on the sheet and on the cards under it); in a drop the cards lift off and turn in space.
    enum Motion { M_FREE, M_ROD, M_TILE }; // in a drop: turns freely, rolls about its length, flips like a tile
    struct Card {
        int kind;
        float hx, hy, p1, p2, p3; // as the shapes', in board units
        vec3 col;
        float a;
        float x, y, rot, h;       // its place on the sheet (rot clockwise, as the shapes'), its height
        int key, motion;          // its identity (its motion in a drop)
        float rise;               // how high it rises in a drop (board units)
        vec3 c;                   // its pose: center, rotation (column major), its lift (0..1)
        float R[9];
        float lift;
    };
    std::vector<Card> cards_;
    float cardH_ = 0.f, cardRise_ = 0.f; // the height, the rise and the key of the cards being added
    int cardKey_ = 0, cardMotion_ = M_FREE;
    void card(Kind kind, float x, float y, float hx, float hy, float rot, const vec3& c, float a, float p1 = 0.f,
              float p2 = 0.f, float p3 = 0.f);
    void poseCards(const MusicState& music);
    void drawCards();
    void setLight(GLuint prog);
    std::string title_, info1_, info2_, section_;
    long bar_ = -1000, step_ = -1000;
    int heavy_ = -1;             // the side holding the big circle (-1 left, +1 right)
    float spin_ = 0.f;           // the big forms' turn at a peak (radians)
    float quadTurn_ = 0.f, quadTurnTo_ = 0.f; // a quad turns the forms by a quarter

    // ---- Traces: each cleared row joins the poster as a small form in a growing sampler, a quad as a medallion.
    struct Trace {
        int kind;          // 0 square, 1 disc, 2 triangle, 3 half disc, 4 quarter disc
        int col;           // its piece color
        int quarters[4];   // a medallion: its four quarters' colors
        bool medallion;
        float rot;         // multiples of a quarter turn
        Form cur, from, to;
        float t, dur;
        bool far;          // a move across the sheet (shrinks out, grows in)
    };
    std::vector<Trace> traces_;
    float samplerX_ = 0.f, samplerY_ = 0.f, samplerDir_ = 1.f, samplerCell_ = 0.9f;
    int samplerCols_ = 6;
    float medalX_[2] = {0, 0}, medalY_[2] = {0, 0}, medR_ = 1.6f;
    void placeTraces(float dur, bool instant);

    // ---- Pieces: one id per piece (its cells merge into one flat shape).
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct Wipe { float y; int type[10], mask[10]; float t, delay, dir; };
    std::vector<Wipe> wipes_;
    vec3 pieceColor(int type) const;
    void drawCell(float x, float y, float half, int mask, const vec3& c, float a, float round = 0.f,
                  const float* clip = nullptr);
    void drawBoard(const Game& g);

    // ---- GL.
    GLuint progShape_ = 0, progText_ = 0, progComp_ = 0, progCard_ = 0, progShadow_ = 0, progSheet_ = 0;
    GLuint quadVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint cubeVao_ = 0, cubeVbo_ = 0, quadVao_ = 0;
    ugl::Target posterT_;
    mat4 vp_;
    mat4 vp3_;                   // the poster in space: the reference camera (the sheet at z = 0 maps as vp_ does)
    vec3 eye_{0.f};
    float camDist_ = 36.f;
    // the light: a far sun over the sheet, swinging in to the drop's spotlight circling the sculpture
    vec3 lamp_{0.f}, spotDir_{0.f, 0.f, -1.f};
    float lightR_ = 0.f;
    float lit_[4] = {0.5f, 0.6f, 1.f, 0.f};
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 ground_{1.f}, ink_{0.f}; // the paper and the ink as shown (slow motion turns them)
    float bpm_ = 120.f;
    float hudTop_ = 9.6f, hudBottom_ = 8.6f; // the left zone's free height, clear of the HUD's corner lines

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false, dropPending_ = false;
    bool realDrop_ = false;
    bool wasReal_ = false;
    float dropLift_ = 0.f;    // the drop's sculpture (0 flat .. 1 every card lifted; up over 1.1 s, down over 1.8 s)
    double dropClock_ = 0.0;  // beats since the cards lifted (held while they fold back)
    double lastBeat_ = 0.0;
    uint64_t salt_ = 0;       // a fold after a drop lays down a new composition
    bool folding_ = false;
    float hop_ = 1.f;         // a quad: every card hops off the sheet as it turns (0..1)
    float kickIn_ = 0.f, kickShown_ = 0.f;
    float lockLine_ = 0.f, lockX0_ = 0.f, lockX1_ = 0.f, lockY_ = 0.f;
    float time_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the poster, no game, no veil, no gauge
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

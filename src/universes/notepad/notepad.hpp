#pragma once
// NOTEPAD: a page of a squared notebook, drawn by hand in stop motion (see notepad.cpp).
#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "text.hpp"
#include "ugl.hpp"
#include "universe.hpp"

class NotepadUniverse : public Universe {
public:
    ~NotepadUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; dirty_ = true; }
    void levelUp() override { levelUpPending_ = true; }
    void kick(float amount) override { kick_ = std::max(kick_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // One mark of a drawing, an instanced quad (see MARK_FS): a stroke (a wobbly line of some tool) or a
    // coloured-in area (a cell, a pie).
    struct Mark {
        float head[4]; // kind (+10: the world's, not the board's), tool, seed, alpha
        float p[4];    // stroke: x0 y0 x1 y1; fill: cx cy hw hh
        float q[4];    // stroke: width, wobble, reveal, bow; fill: pattern, angle, spacing, overflow
        float c[4];    // ink (multiplicative), stroke: dash length; fill: neighbor mask
        float e[4];    // stroke: boil seed, boil, joints (1 start, 2 end); fill: reveal, line width, inset / sweep
        float f[4];    // being rubbed out: amount, seed
    };
    // One thing on the page as the hand drew it on a drawing (a stop-motion frame). Drawings are compared by
    // key: what moved or disappeared is erased (a ghost stays in the residue), what is new is drawn in a few
    // passes (outline, then colour), what stayed the same only boils.
    struct Item {
        uint64_t key = 0;
        int kind = 0;
        float x = 0, y = 0, s = 1, r = 0; // position (board units, pixels for text), scale, rotation
        float a = 0, b = 0, c = 0, d = 0; // by kind
        int i0 = 0, i1 = 0, i2 = 0;       // by kind
        vec3 col{0.f};
        float alpha = 1;
        float wt = 1, wob = 0.03f; // the hand when it was drawn (stroke weight, wobble): kept while it stays
        int tool = 0;
        std::string text;
        int n = 0;          // outline: its cells (offsets in cells from x, y)
        float pts[8] = {};
        bool world = false; // the world's (doodles, crumbs), not the board's
        // the hand
        float seed = 0;        // this drawing of it (redrawn: a new seed)
        int born = 0;          // the drawing it first appeared on
        float residue = 0;     // how much of it an eraser leaves when it disappears
        float moveResidue = 0; // the same when it moves
    };

private:
    // A HUD line as the hand writes it (see hudLine).
    struct Word {
        std::string text;
        float size = 16, dy = 0, boost = 1.2f;
        vec3 col{0.f};
        bool underline = false;
    };
    struct Doodle {
        int shape = 0;
        vec2 pos;
        float size = 1, rot = 0;
        vec3 col{0.f};
        int tool = 1;
        bool hatch = false;
        float speed = 0.3f; // fraction of its strokes drawn per drawing
        float until = -1;   // erased at this time (-1: when the music calms down)
        uint64_t id = 0;
    };
    struct Transient { vec2 a, b, vel; float width, alpha, from, until; vec3 col; int tool; uint64_t id; float residue; bool world; };
    struct Dying { int x, y, type, mask; uint64_t key; float t, dur; };
    // A cell of the stack: which piece it belongs to (its index in it), and its type.
    struct InkCell { uint64_t id = 0; int idx = 0; int type = -1; };
    enum { IT_CELL = 0, IT_OUTLINE = 1, IT_STROKE = 2, IT_DOODLE = 3, IT_PIE = 4, IT_TEXT = 5, IT_SCRIBBLE = 6, IT_MOVER = 7 };
    // A drop's travelling drawings: redrawn at a new pose on every drawing, the old pose rubbed out.
    enum { MV_CUBE = 0, MV_BALL = 1, MV_RUNNER = 2, MV_PLANE = 3, MV_ROCKET = 4, MV_COUNT = 5 };
    struct Mover {
        int kind = 0, side = 0;
        vec2 start, vel;      // vel: board units per beat
        double beat0 = 0;
        float size = 1, ylo = 0, yhi = 0;
        vec3 col{0.f};
        int tool = 1;
        uint64_t id = 0;
    };

    // drawings
    void tick(const Frame& f, float since);
    void style(const MusicState& music);
    void buildDrawing(const Frame& f, std::vector<Item>& out);
    void addPiece(std::vector<Item>& out, uint64_t base, const int (*cells)[2], int count, vec2 origin, float scale,
                  int type, float alpha, bool ghost, float residue, float moveResidue, int maskCells = 4);
    void manageDoodles();
    void manageMovers();
    void spawnMover(int kind, int side);
    void marginBox(int side, float& x0, float& y0, float& x1, float& y1) const;
    void emitMover(const Item& it, std::vector<Mark>& out);
    void emitKind(const Item& it, int age, bool residue, std::vector<Mark>& out, std::vector<Item>* words);
    void spawnDoodle(bool burst);
    bool freeSpot(float size, vec2& out);
    void syncInk(const Game& game);
    void hudItems(const Frame& f, std::vector<Item>& out);
    Word word(const HudText& h) const;

    // marks
    void emit(const Item& it, int age, bool residue, std::vector<Mark>& out, std::vector<Item>* words);
    void emitCell(const Item& it, int age, bool residue, std::vector<Mark>& out);
    void emitOutline(const Item& it, int age, bool residue, std::vector<Mark>& out);
    void emitPolyline(const std::vector<vec2>& pts, float width, vec3 ink, float alpha, int tool, float seed,
                      float wobble, float reveal, bool world, std::vector<Mark>& out);
    void stroke(std::vector<Mark>& out, vec2 a, vec2 b, float width, vec3 ink, float alpha, int tool, float seed,
                float wobble, float reveal = 1.f, float bow = 0.f, float dash = 0.f, bool world = false, int joints = 0);
    vec3 inkOf(const vec3& c) const;

    // GL
    void createTargets();
    void drawMarks(const std::vector<Mark>& m, bool residue, vec2 smear);
    void drawGhostWords(const std::vector<Item>& words);
    int pickVariant(const SongLook& look) const;

    GLuint progPaper_ = 0, progMark_ = 0, progPost_ = 0, progFade_ = 0;
    GLuint quadVbo_ = 0, markVao_ = 0, markVbo_ = 0, emptyVao_ = 0;
    ugl::Target residueT_, sceneT_;
    // the words: lettered by the host over the page (hudLine), their eraser ghosts written into the residue here
    text::Lettering hand_, ghostHand_;
    text::Engine ghostText_;
    mat4 vp_;
    vec2 viewC_{0, 0.4f}, viewH_{22, 12.4f};
    float px_ = 0.05f;
    float marginX_ = -16.f;

    // the song and its staging
    int variant_ = 0;
    int level_ = 1;
    float drive_ = 0.f;      // a drop's peak (against a chorus' one)
    float dropHeld_ = 0.f;   // after drop(): the drop look before the level settles
    bool dropOn_ = false;    // the drop's look, until the peak ends
    int markerBorn_ = -100;  // the drawing the well's marker retrace started on
    int stage_ = 1;          // 0 break, 1 mid, 2 chorus, 3 drop
    float stageF_ = 1.f;     // the stage the hand's style is at: up at once, down slowly when the music calms
    float markerA_ = 0.f;    // the drop's marker over the well, fading out after the drop
    double moverOut_ = 0.0;  // when the next travelling drawing is rubbed out after the drop
    float kick_ = 0;
    bool slowOn_ = false;
    float slow_ = 0;
    bool levelUpPending_ = false;
    float refreshPending_ = 0;
    Bridge bridge_;
    float pauseFade_ = 0;
    bool bare_ = false; // a break's page: no drawings, the words written as given

    // the hand's style on this drawing
    vec3 outlineCol_{0.f}, graphite_{0.f};
    int outlineTool_ = 1;
    float weight_ = 1, wobble_ = 0.02f, boil_ = 0.02f, colorAmt_ = 1, fillAlpha_ = 1;
    float handSpeed_ = 0.5f; // how much of a new thing is drawn per drawing

    // the stop-motion clock
    double clock_ = 0;
    double lastTick_ = -1;
    long beatSlot_ = -1;
    long lastBeatI_ = -1;
    int beatsSeen_ = 0, lastDoodleBeat_ = 0;
    int drawing_ = 0;
    bool dirty_ = true;
    bool clearPage_ = true;
    uint64_t sig_ = 0;

    // the game, as drawn
    InkCell ink_[22][10];
    uint64_t serial_ = 1;
    uint64_t activeId_ = 0;
    bool hardPending_ = false;
    std::vector<Dying> dying_;
    std::vector<Transient> transients_;
    std::vector<Doodle> doodles_;
    std::vector<Mover> movers_;
    double beatNow_ = 0; // the song's beat at the current drawing
    float overT_ = -1;       // game over: time since the top out
    std::vector<Item> dead_; // the stack at the top out: scribbled over, then erased
    float deadTop_ = 0;

    std::vector<Item> items_;                    // the current drawing
    std::unordered_map<uint64_t, size_t> index_; // key -> item
    std::vector<Mark> marks_;                    // its marks
    size_t markCount_ = 0;
    Rng rng_{777};
};

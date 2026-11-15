#pragma once
// RECYCLE: a table seen from above, the pieces assembled from heaps of cubes beside the well (see recycle.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RecycleUniverse : public Universe {
public:
    ~RecycleUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // A variant's colors (linear RGB) and materials.
    struct Palette {
        vec3 table, tray, shadowTint, ink, accent, pile;
        vec3 piece[7], drop[7];
        float shadowAmt, mottle, grain, lamp, trayDepth, dark, gloss, wood, woodPiece, lift, edge;
    };

private:
    // ---- The heaps. A formation lays the cubes out on both sides of the well: piles, each an ordered list of
    // slots (bottom to top, the order they fill in); a pile holds its first `count` slots.
    enum Form { F_HEAPS, F_TOWERS, F_WAVE, F_RINGS, F_COUNT };
    struct Slot { vec3 home; float rot; float key; int tag = 0; }; // tag: its ring (a color)
    enum { EMPTY = 0, FULL = 1, RESERVED = 2 };
    struct Occ {
        int state = EMPTY;
        vec3 from{};          // where its move started
        float t = 1.f, dur = 0.5f, delay = 0.f, arc = 0.f, rotFrom = 0.f;
        float hopT = 9.f, hopA = 0.f, hopDelay = 0.f; // a hop in place
        int tint = -1;        // the piece type it last belonged to
        float tintAge = 99.f; // seconds since it came back (its piece's color fades)
        vec3 shown{};         // last drawn position
        float rotShown = 0.f;
    };
    struct Pile {
        int side = 1;         // -1 left, +1 right
        int colorIdx = 0;     // its color in a drop (-1: along its slots)
        float weight = 1.f;   // its share of its side's cubes
        std::vector<Slot> slots;
        std::vector<Occ> occ;
        int full() const { int n = 0; for (const Occ& o : occ) n += o.state != EMPTY; return n; }
    };
    std::vector<Pile> piles_;
    Form form_ = F_HEAPS;
    int dropForm_ = 0;
    void buildForm(Form f, std::vector<Pile>& out);
    void switchForm(Form f, float spread);
    // A pile cube for a new piece (the top of the fullest pile on that side, -1 if none), and a slot for a cube
    // coming back (on the emptiest pile of that side).
    bool takeCube(int side, vec3& pos, float& rot, int& tint);
    bool reserveSlot(int side, const vec3& near, int& pile, int& slot);
    vec3 slotPos(const Pile& p, int i) const;

    // ---- Cubes in the air: from the heaps to the piece or the stack, from a cleared row back to the heaps.
    enum Target { T_PIECE, T_CELL, T_PILE };
    struct Flight {
        vec3 from{};
        float rotFrom = 0.f, spin = 0.f;
        Target target = T_PILE;
        int a = 0, b = 0;      // T_PIECE: the piece's cell index; T_CELL: x, y; T_PILE: pile, slot
        float t = 0.f, dur = 0.4f, delay = 0.f, arc = 1.f;
        int type = -1;         // the color it flies with (piece type), -1 neutral
        int fromType = -1;     // its color at the start
        float fromTint = 0.f;
        bool realTime = true;
        vec3 last{};
    };
    std::vector<Flight> flights_;
    bool incoming_[22][10] = {};   // a stack cell whose cube is still flying in
    bool mirror_[22][10] = {};     // the stack as we know it (from the events)
    bool pieceIn_ = false;         // we have assembled the current piece
    int pieceType_ = -1;
    bool pieceArrived_[4] = {};
    vec3 lastPiece_[4];            // where the piece's cubes were drawn last frame
    bool lastPieceOk_ = false;
    int lastPieceType_ = -1;
    bool fresh_ = true;
    int total_ = 176;              // the cubes in the world
    float fallRate_ = 1.5f;        // how fast pieces fall (rows a second, smoothed): a fast game assembles faster
    float fallY_ = 0.f, fallAge_ = 9.f; // fallAge_: seconds since the piece spawned
    bool fallOk_ = false;
    void assemblePiece(const Game& g, bool instant);
    void sendHome(const vec3& from, int type, float delay, float dur, float arc, bool realTime);
    void reconcile(const Game& g);
    vec3 targetPos(const Flight& fl, const Game& g) const;
    int boardCubes(const Game& g) const;
    int pileCubes() const;

    // ---- Drawing.
    struct Inst { float a[4], b[4], c[4], d[4]; };
    std::vector<Inst> shadows_, flat_, cubes_;
    struct Drawn { vec3 p; float rot, half; vec3 col; float alpha, squash, gloss, wood, edge, seed; };
    std::vector<Drawn> drawn_;
    void addCube(const vec3& p, float rot, const vec3& col, float alpha, float squash, float seed, bool piece);
    void flushCubes();
    void buildScene(const Game& g, bool bare);
    vec3 pileColor(const Pile& p, const Occ& o, int slot) const;
    vec3 pieceCol(int t) const { return pal_.piece[std::clamp(t, 0, 6)]; }

    // ---- The HUD (its lettering: hudLine, the host letters it).
    float fade_ = 1.f; // the frame's fade in, the HUD's own (it was lettered under the post's fade and dim)
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progTable_ = 0, progShadow_ = 0, progCube_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, instVao_ = 0, instVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    ugl::Target sceneT_, shadowT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void drawInst(GLuint prog, const std::vector<Inst>& v);

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, colAmt_ = 0.f;
    double lastTick_ = -1.0;
    int ticks_ = 0;
    float kickIn_ = 0.f;
    float rippleT_ = 9.f;
    void ripple(float amp, float spread, float delay = 0.f);
    void hopBetweenPiles();

    // ---- Effects.
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    float worldTime_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

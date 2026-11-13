#pragma once
// BLUEPRINT: a technical drawing whose parts are the pieces (see blueprint.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class BlueprintUniverse : public Universe {
public:
    ~BlueprintUniverse() override;
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

    // A sheet's colors (linear RGB) and a few numbers.
    struct Palette {
        vec3 paper, paper2, line, cons, dim, accent, ink, mach;
        vec3 piece[7];
        vec3 outline;   // the parts' outline (white on a blueprint)
        float dark, fillA, hatchA, outlineTint, mottle;
    };

private:
    // ---- Pen strokes: polylines in board units, drawn progressively.
    enum Weight { W_THIN, W_MED, W_THICK };
    enum Pattern { P_SOLID, P_HIDDEN, P_CENTER, P_DOT, P_BUTT };  // P_BUTT: solid, square-cut ends
    enum Role { R_LINE, R_CONS, R_DIM, R_ACCENT, R_MACH };
    struct Stroke {
        std::vector<vec2> pts;
        Weight w = W_MED;
        Pattern pat = P_SOLID;
        Role role = R_LINE;
        float alpha = 1.f;
    };
    struct LineInst { float a[4], b[4], c[4]; };
    std::vector<LineInst> lines_;
    void emit(const Stroke& s, float reveal, float alpha, float scale = 1.f);
    float strokeLen(const Stroke& s) const;
    void emitAll(const std::vector<Stroke>& ss, float reveal, float alpha);
    vec3 roleColor(Role r) const;
    float weightPx(Weight w) const;

    // ---- The machine: parts drawn in the margins, one after another as the song goes on; they move in a drop.
    struct Part {
        int kind = 0;
        float reveal = 0.f;  // how much of it the pen has drawn (0..1)
        float ink = 0.f;     // 1 inked, toward 0 a faint ghost (a break)
        float delay = 0.f;   // seconds before the pen starts
        float speed = 0.3f;  // reveal per second
        bool started = false, ever = false;
    };
    std::vector<Part> parts_;
    int partsWanted_ = 0;        // how many parts the drawing has (in order)
    float machScale_ = 1.f;
    bool machOn_ = true;
    vec2 gA_, gB_, gC_, cam_, link_, yoke_;  // part anchors
    float rA_ = 2.3f, rB_ = 1.15f, rC_ = 1.5f;
    void layoutMachine();
    void buildPart(int kind, std::vector<Stroke>& out) const;
    void addPart(float speed, float delay);
    float angle_ = 0.f;          // the machine's driving angle (radians, the big gear's)
    float motion_ = 0.f;         // 0 still .. 1 running (eased)
    double driveBeat_ = 0.0;
    float fire_ = 0.f;           // the piston's firing on a kick (local)
    void gear(std::vector<Stroke>& out, vec2 c, float r, int teeth, float ang, int holes, bool detail) const;

    // ---- Dimensions: the last placed piece, the stack's height, the well's width.
    // The last part's dimension: drawn in over 0.4 s, held, then faded out quickly; anything landing above the
    // part or a clear lifts it at once (life jumps it to the start of its fade).
    static constexpr float DIM_HOLD = 1.5f, DIM_FADE = 0.3f;  // seconds (from the lock)
    struct Dim { float x0, x1, y; float t; float life; bool alive; int value; };
    Dim pieceDim_{};
    float stackShown_ = 0.f;     // the stack's dimensioned height (rows), gliding
    int stackRows_ = 0;
    float stackDimT_ = 0.f;      // time since the height changed
    void dimH(float x0, float x1, float y, float yRef0, float yRef1, float reveal, float alpha, Role role, std::vector<Stroke>& out) const;
    void dimV(float y0, float y1, float x, float xRef, float reveal, float alpha, Role role, std::vector<Stroke>& out) const;
    void arrow(vec2 tip, vec2 dir, Role role, float alpha, std::vector<Stroke>& out) const;
    struct Label { std::string text; vec2 pos; float size; float alpha; Role role; bool vertical; bool center; };
    std::vector<Label> labels_;

    // ---- The sheet: border, zones, title block, revisions.
    int rev_ = 0;                // revisions (clears)
    float revT_ = 9.f;           // time since the last revision
    float cloudT_ = 9.f;         // a quad's revision cloud
    void buildSheet(std::vector<Stroke>& out, float lvA, bool bare);
    std::string dwgNo_ = "BP-0000";

    // ---- Pieces.
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int kind, int style, float hatchA,
              float erase, const vec3& line, float front);
    void buildBoard(const Game& g);
    // The ghost: a hidden line around the part's outline only, dashed run by run (pen-line instances, butt-ended
    // segments in the line color, drawn in their own MAX layer under the pieces).
    std::vector<LineInst> ghost_;
    void buildGhost(const int (&cells)[4][2], const vec3& color, float alpha);
    float cellLineW() const;  // the parts' outline width (pixels)
    struct Fading { float x, y; int type, mask; float t; };  // a cleared cell being rubbed out
    std::vector<Fading> fading_;
    // a part just locked: a hard drop draws its hatching up from the impact; a soft landing only darkens the falling
    // part's lighter hatching to the locked one's (no redrawing)
    struct LockFx { int id; float minY, maxY, t; bool dropped; };
    std::vector<LockFx> lockFx_;

    // ---- Text: the world's lettering (dimensions, the title block); the HUD's is the host's (hudLine).
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
    float textWidth(const std::string& s, float size, float track) const;
    // Quads for a line of text from its top-left (pixels), with a slant and an optional quarter turn (reading up).
    void textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out,
                   float slant = 0.f, bool vertical = false) const;
    void drawText(const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.85f);
    float fade_ = 1.f; // the frame's fade in, the HUD's own (it was lettered under the post's fade and dim)
    void drawHud(const std::vector<HudRect>& panels);
    void drawLabels();

    // ---- GL.
    GLuint progPaper_ = 0, progLine_ = 0, progLayer_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, lineT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    float pxPerUnit_ = 20.f;

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
    float dropAmt_ = 0.f, colAmt_ = 0.f, swell_ = 0.f;
    double lastBeat_ = -1.0;
    int barCount_ = 0, lastPartBar_ = -99;
    float barClock_ = 0.f;       // without a song: a bar every two seconds
    float kickIn_ = 0.f;
    float consA_ = 0.f;          // construction lines shown
    float detail_ = 0.f;         // chorus details (center marks, section hatching, the coupler curve)

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

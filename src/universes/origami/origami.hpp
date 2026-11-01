#pragma once
// ORIGAMI: folded paper pieces on a sheet, cleared rows folded into paper cranes that gather around the well
// (see origami.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class OrigamiUniverse : public Universe {
public:
    ~OrigamiUniverse() override;
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

private:
    // ---- Pieces: folded cells (instanced quads, board units).
    // x, y, half, mask | color, alpha | kind, progress, fold, lift | ridge mask (folding away), 0, 0, 0
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_, shadows_;
    int ids_[22][10] = {};
    float fold_[22][10] = {};   // how tightly the cell is folded (1 falling .. rest when settled)
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Folding { float x, y; int type, mask, ridge; float t, delay; }; // a cell folding away (a clear, game over)
    std::vector<Folding> folding_;

    // ---- The cranes: one per cleared row, gathered around the well for the whole song.
    struct Crane {
        vec2 home{0, 0}, pos{0, 0}, from{0, 0};
        float scale = 1.f;
        int color = 0;           // 0..6 a piece's paper, 7 the accent (a Tetris)
        int slot = -1;
        float age = 0.f;         // since its row was cleared (the row slides out, is folded into a crane, flies off)
        float arrive = 4.f;      // when it is in its place (the folding, then the flight)
        int strip[10] = {};      // the row's papers (0..6), from the left
        float stripY = 0.f;      // the row's height
        float side = 1.f;        // the side the row slides out to
        vec2 spot{0, 0};         // where it is folded, outside the well
        float wingT = 1.f;       // a wing beat in progress (0..1)
        float wingDelay = 0.f;
        float hop = 0.f, hopV = 0.f;
        float fly = 0.f;         // 0 perched .. 1 in the flock
        float flyDelay = 0.f;
        float orbit = 0.f, rx = 3.f, ry = 8.f, speed = 0.1f, phase = 0.f;
        float face = 1.f;        // +1 facing right
        bool leaving = false;
        float leaveT = 0.f;
        vec2 vel{0, 0};
    };
    std::vector<Crane> cranes_;
    std::vector<vec2> slots_;
    std::vector<int> slotUsed_;   // crane count per slot (0 free)
    void planSlots();
    int takeSlot();
    void addCrane(float rowY, const int* strip, int color, float scale);
    // The paper model: a face of folded paper (local units, z toward the viewer), its two sides' colours.
    struct PFace { vec3 v[3]; vec3 front, back; float key; };
    void craneModel(float F, float flap, const vec3& col, std::vector<PFace>& out) const;
    void stripFaces(const Crane& c, float t, float flipX, std::vector<PFace>& out) const;
    void emitFaces(std::vector<PFace>& faces, vec2 pos, float scale, float face, float alpha, float shadowOff,
                   float shadowA, bool outline);
    void updateCranes(const MusicState& m, float dt, float wdt, bool paused);
    void buildCranes();
    std::vector<float> craneTris_, craneShadow_; // x, y, r, g, b, a (board space)

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontReg_, fontLight_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progPaper_ = 0, progCell_ = 0, progTri_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, triVao_ = 0, triVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, shadowT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void drawCells(const std::vector<CellInst>& v);
    void drawTris(const std::vector<float>& v);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 sheet() const;        // the sheet now (linear), between its two sides
    vec3 paper(int i) const;   // a piece's paper now (linear): 0..6, 7 the accent
    vec3 lightDir() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dev_ = 0.f;          // the sheet turned to its coloured side (the drop) 0..1
    float swell_ = 0.f;        // the drop's light swell
    float flock_ = 0.f;        // the cranes asked to fly
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    double lastHalf_ = -1.0;
    float beatClock_ = 0.f, flapPhase_ = 0.f;
    int beatCount_ = 0;
    float time_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{321};
};

#pragma once
// SPECIMEN: plump lumps of living tissue on a dark bench, and on each side a loupe looking into one of them; a cleared
// row's organisms crawl off the screen, a loupe following one (see specimen.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SpecimenUniverse : public Universe {
public:
    ~SpecimenUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };

    // ---- The dyes: a variant's colors, its own and the drop's (linear RGB).
    struct Stain {
        vec3 field[2];   // the loupes' slide: center, edge
        vec3 piece[7];   // the seven piece types
        vec3 org[3];     // the organisms: body, rim, organelles
    };
    Stain base_, baseT_, dropS_, dropT_;
    vec3 back_[2], backT_[2];   // the bench: under the lamp, at the edges
    bool colorsSet_ = false;
    void setStain(GLuint prog);

    // ---- The stack: each cell keeps its look (a seed) and, in some of them, an organism living inside (its kind, -1
    // for none), seen under a loupe, which crawls out when the row clears.
    struct CellLook { float nx = 0, ny = 0, seed = 0; int life = -1; };
    CellLook look_[22][10];
    bool has_[22][10] = {};
    CellLook lookOf(int x, int y);
    CellLook fallLook_[4];   // the falling piece's cells, kept as they are when it locks
    void syncLooks(const Game& g);
    struct Lyse { float x, y; int type; CellLook l; float t; };   // a cleared cell bursting
    std::vector<Lyse> lyses_;
    struct Dying { float x, y; int type; CellLook l; float delay; }; // game over: plasmolysis
    std::vector<Dying> dying_;
    struct Speck { vec2 p, v; float t, life, r; };                // fine debris pushed out by a hard drop
    std::vector<Speck> specks_;

    // ---- The life.
    struct Org {
        int kind = 0;
        vec2 p{0, 0};
        float ang = 0, size = 2.f, seed = 0, depth = 0, turnPhase = 0, tint = 0, spin = 0;
        int type = 0;
    };
    Org dweller(const CellLook& l, float x, float y, int type) const;   // a cell's organism, as it is now inside it
    int pickKind();
    int lastKind_ = -1;

    // ---- The loupes: one on each side, looking into a block of its half of the stack (its tissue magnified,
    // the organism living in it if it has one), a fine line to the block, a while on each.
    struct Lens {
        int x = -1, y = -1;     // the board cell it looks into
        int type = 0;
        CellLook l;             // that cell, as it was when the loupe found it
        vec2 at{0, 0};          // the cell's place
        float a = 0.f, age = 0.f, ly = 0.f, pan = 0.f;
        bool leaving = false, lost = false, placed = false;
        int follow = -1;        // or the crawler it follows out of the screen (its id)
    };
    // The organisms a cleared row frees: tiny, they crawl out of the well and off the screen by the nearer side.
    struct Crawler { Org o; vec2 dest{0, 0}; float age = 0.f, speed = 1.8f; int id = 0; };
    std::vector<Crawler> crawlers_;
    int crawlerIds_ = 0;
    const Crawler* crawler(int id) const;
    Lens lenses_[2];
    float lensR_ = 4.f, lensShown_ = 0.f;   // (the loupes appear with the first block they find)
    vec2 lensCenter(int side) const;
    void updateLenses(const Game& g, float dt, float wdt);

    // ---- Drawing.
    std::vector<Inst> cells_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha, float flash);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progField_ = 0, progOrg_ = 0, progPiece_ = 0, progSpeck_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progLens_ = 0, progLine_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, orgVao_ = 0, orgVbo_ = 0, speckVao_ = 0, speckVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0, lensVao_ = 0, lensVbo_ = 0, lineVao_ = 0, lineVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant, staging.
    int variant_ = 0;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float stain_ = 0.f;            // the drop's stain soaked in, 0..1
    float swell_ = 0.f;            // the drop's (and a quad's) single swell of light
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    bool bare_ = false; // this frame is the world alone (a break): the bench, no tissue, no loupes
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x3C1};
};

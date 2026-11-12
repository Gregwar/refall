#pragma once
// MICROSCOPE: a Petri dish seen through a microscope's eyepiece, filled with life by the cleared rows (see
// microscope.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class MicroscopeUniverse : public Universe {
public:
    ~MicroscopeUniverse() override;
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
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };

    // ---- The stain: a variant's colors, its own and the drop's (linear RGB).
    struct Stain {
        vec3 field[2];   // the field of view: center, edge
        vec3 piece[7];   // the seven piece types
        vec3 org[3];     // the organisms: body, rim, organelles
    };
    Stain base_, baseT_, dropS_, dropT_;
    float dark_ = 1.f, darkT_ = 1.f;   // 1: dark-field (light on black), 0: bright-field (stains on a pale field)
    bool colorsSet_ = false;
    void setStain(GLuint prog);

    // ---- The stack: each cell keeps its nucleus (offset, seed) and, in some of them, a small organism living inside
    // (its kind, -1 for none), which swims out into the dish when the row clears.
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

    // ---- The dish's life.
    struct Org {
        int kind = 0;
        vec2 p{0, 0}, home{0, 0};
        float ang = 0, speed = 0.3f, size = 2.f, seed = 0, age = 0, fade = -1.f, depth = 0, burst = 0;
        float turnPhase = 0, cilia = 0, dir = 1.f, tint = 0, spin = 0, dart = 0;
        int type = 0;
        float grow0 = 0.45f;     // its size at birth, a fraction of its full size
        bool fromCell = false;   // born out of a cleared cell: crisp and in full view from the start
    };
    std::vector<Org> orgs_;
    Org dweller(const CellLook& l, float x, float y, int type) const;   // a cell's organism, as it is now inside it
    void releaseFrom(const CellLook& l, float x, float y, int type, float delay, bool big);
    vec2 pickHome(int side);
    std::vector<std::pair<float, Org>> pending_;   // organisms released a moment after their row bursts
    int pickKind();
    float zoneDist(const vec2& p, vec2* grad) const;  // distance to the well and the HUD (negative inside)
    float diskR_ = 20.f;

    // ---- Drawing.
    std::vector<Inst> cells_, orgInst_;
    std::vector<float> specksV_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, const CellLook& l, int kind, float prog, float alpha, float flash);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontSans_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, bool accent, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes(int mode);
    vec3 inkIn_, inkOut_, accentIn_, accentOut_, haloIn_, haloOut_;

    // ---- GL.
    GLuint progField_ = 0, progOrg_ = 0, progPiece_ = 0, progSpeck_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, orgVao_ = 0, orgVbo_ = 0, speckVao_ = 0, speckVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
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
    float focus_ = 0.f, focusT_ = 0.f; // the focal plane (organisms' depth in focus)
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    double lastBar_ = -1.0;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the dish and its life, no tissue, no reticle
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x3C1};
};

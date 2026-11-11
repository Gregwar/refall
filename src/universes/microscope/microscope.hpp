#pragma once
// MICROSCOPE: a picture-book Petri dish seen through a toy microscope, filled with cute creatures by the cleared rows
// (see microscope.cpp).
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
    struct LInst { float a[4], b[4], c[4], d[4], e[4]; };

    // ---- The palette: a variant's colors, its own and the drop's (linear RGB).
    struct Palette {
        vec3 water[3];   // the water: center, edge, the well's pane
        vec3 dropWater[2];
        vec3 piece[7];
        vec3 life[5], dropLife[5];
        vec3 table, rim, rimInk, line, eyeInk, blush, night;
    };
    Palette pal_, palT_;
    bool colorsSet_ = false;
    void setPalette(GLuint prog);

    // ---- The stack's cells, popping and deflating.
    struct Pop { float x, y; int type; float t; };          // a cleared cell popping
    std::vector<Pop> pops_;
    struct Dying { float x, y; int type; float delay; };    // game over: the cells deflate from the top
    std::vector<Dying> dying_;

    // ---- The dish's life.
    struct Org {
        int kind = 0, col = 0;
        vec2 p{0, 0}, home{0, 0};
        float ang = 0, speed = 0.3f, size = 3.f, seed = 0, age = 0, fade = -1.f, depth = 0, burst = 0;
        float phase = 0, spin = 0, look = 0, hop = 0, happy = 0, blink = 0, nextBlink = 3.f;
        float grow0 = 0.3f;     // its size at birth, a fraction of its full size
        bool fromCell = false;  // born out of a cleared row: it swims out of the well, behind the stack
    };
    std::vector<Org> orgs_;
    struct Bubble { vec2 p, v; float t, life, r; bool deco; };
    std::vector<Bubble> bubbles_;
    struct ClearAt { std::vector<vec2> cells; int rows; };   // cleared rows waiting for the frame that frees their life
    std::vector<ClearAt> clears_;
    void release(const ClearAt& c, float pace);
    void spawn(vec2 at, int side, bool fromCell, bool big);
    vec2 pickHome(int side, float size);
    int pickKind();
    int pickColor();
    float zoneDist(const vec2& p, vec2* grad) const;  // distance to the well and the HUD (negative inside)
    int alive() const;
    int sideCount(int side) const;   // the creatures living on one side (-1 left, 1 right)
    float diskR_ = 20.f;
    float full_ = 12.f;   // the creatures at scene completion (the room there is around the well and the HUD)
    float pace_ = 0.f, paceShown_ = 0.f, lastPace_ = 0.f;
    int beatCount_ = 0;
    float fillT_ = 0.f;

    // ---- Drawing.
    std::vector<Inst> cells_;
    std::vector<LInst> lifeBack_, lifeFront_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int type, int kind, float prog, float alpha, float flash);
    void addLife(std::vector<LInst>& out, const Org& o, float alpha, float sizeK);
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
    Font fontB_, fontM_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, bool accent, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes(int mode);
    vec3 inkIn_, inkOut_, accentIn_, accentOut_, haloIn_, haloOut_;

    // ---- GL.
    GLuint progField_ = 0, progLife_ = 0, progPiece_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, lifeVao_ = 0, lifeVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0,
           textVbo_ = 0;
    ugl::Target sceneT_;
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
    float stain_ = 0.f;            // the drop's palette soaked in, 0..1
    float swell_ = 0.f;            // the drop's (and a Tetris') single swell of light
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    double lastBar_ = -1.0, lastHalf_ = -1.0;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false; // this frame is the world alone (a break): the dish and its life, no stack, no pane
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x3C1};
};

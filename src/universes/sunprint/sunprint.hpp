#pragma once
// SUNPRINT: a cyanotype photogram exposed under the stack (see sunprint.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SunprintUniverse : public Universe {
public:
    ~SunprintUniverse() override;
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
    // ---- Pieces: glass plates lying on the sheet (instanced quads, board units).
    struct CellInst { float a[4], b[4], c[4]; }; // x, y, half, mask | color, alpha | kind, lift, glint, shade
    std::vector<CellInst> cells_, raised_, flat_;
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Lifted { float x, y; int type, mask; float t, delay; }; // a plate lifted off the sheet (a clear, game over)
    std::vector<Lifted> lifted_;

    // ---- Ferns pressed on the sheet in the margins (instanced leaflets: ellipses).
    struct Frond { vec2 base; float angle, length, curl, phase; };
    std::vector<Frond> fronds_;
    struct LeafInst { float a[4], b[4]; }; // x, y, half length, half width | angle, alpha, pad, pad
    std::vector<LeafInst> leaves_;
    void placeFronds();
    void buildFronds(float sway);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontReg_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progOcc_ = 0, progLeafOcc_ = 0, progBlur_ = 0, progExpose_ = 0, progPaper_ = 0, progPlate_ = 0, progLeaf_ = 0,
           progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, leafVao_ = 0, leafVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target occT_, fallT_, blurT_, expoT_, expoB_, sceneT_; // expoT_: the tint map (expoB_: the next one)
    bool expoClear_ = true;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void drawCells(const std::vector<CellInst>& v, int pass);
    void blur(ugl::Target& t, float radius);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dev_ = 0.f;         // the drop's tints 0..1
    float swell_ = 0.f;       // the drop's light swell
    float flare_ = 0.f;       // a Tetris: a burst of sun over the well
    float flareY_ = 0.f;      // where (board y)
    float rinse_ = 0.f;       // a new song: the sheet is rinsed to its tint
    int tint_ = 0;            // the tint the open paper is going to (0, 1)
    float tintShown_ = 0.f;   // the open paper's tint, as it eases there
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float sunAngle_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    double beatClock_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

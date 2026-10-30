#pragma once
// HEAT: the stack seen through a thermal camera (see heat.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class HeatUniverse : public Universe {
public:
    ~HeatUniverse() override;
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
    // ---- The stack's heat: the excess temperature of every locked cell (0 room .. 1 just landed), conducted
    // between neighbouring cells and into the well's walls and floor, lost to the air through the exposed faces.
    float heat_[22][10] = {};
    int ids_[22][10] = {};     // one id per locked piece (its outline is where it radiates)
    int nextId_ = 1;
    bool hardPending_ = false; // a HardDrop event came before this Lock
    float wallL_[22] = {}, wallR_[22] = {}, floor_[10] = {};
    void syncCells(const Game& g);
    void conduct(const Game& g, float dt);
    std::vector<float> heatUp_;
    GLuint heatTex_ = 0;

    // ---- The air: a coarse grid over the whole view, the heat the stack gives off rising, swaying, spreading
    // and fading (the glow above the stack, the clears' plumes, the beats' bubbles).
    float ax0_ = 0, ay0_ = 0, as_ = 0.3f;
    int aw_ = 0, ah_ = 0;
    std::vector<float> air_, airTmp_;
    GLuint airTex_ = 0;
    void buildAir();
    void splatAir(float x, float y, float r, float amt);
    void updateAir(const Game& g, float dt);
    struct Vent { float x0, x1, y, amt, t, life; };   // a source of hot air for a while (a clear's plume)
    std::vector<Vent> vents_;
    float airT_ = 0.f;

    // ---- Far away: a few warm shapes in the cold room.
    struct Blob { float x, y, rx, ry, temp, target; };
    std::vector<Blob> blobs_;
    void placeBlobs();

    // ---- Board geometry (instanced cells written into the temperature image).
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void buildBoard(const Game& g);
    struct Fading { float x, y; int gx, gy, type, mask; float h, t; };
    std::vector<Fading> fading_;

    // ---- Text and the camera's overlay.
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
    void textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const std::vector<float>& quads, const vec3& ink, float alpha);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    void drawOverlay(const Game& g);
    std::vector<float> shapes_;
    void rect(float x0, float y0, float x1, float y1, const vec3& c, float a);
    void flushShapes();

    // ---- GL.
    GLuint progBg_ = 0, progCell_ = 0, progMap_ = 0, progBlur_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint lutTex_ = 0;
    ugl::Target tempT_, glowA_, glowB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    float lutBase_[256][3] = {}, lutDrop_[256][3] = {};
    void buildLuts();
    vec3 lutAt(float t) const; // the colormap on screen (sRGB)

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, colAmt_ = 0.f, flare_ = 0.f, tetris_ = 0.f;
    float lo_ = 0.f, hi_ = 1.f; // the camera's range (temperature image units)
    double lastBeat_ = -1.0;
    int beatCount_ = 0, bubbleSide_ = 1;
    float kickIn_ = 0.f, kickPrev_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float worldTime_ = 0.f;
    Bridge bridge_;
    Rng rng_{4343};
};

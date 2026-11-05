#pragma once
// RIPPLES: cut stone blocks resting in still water seen from above; every motion sends fine ripples (see ripples.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class RipplesUniverse : public Universe {
public:
    ~RipplesUniverse() override;
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
    struct Inst { float a[4], b[4], c[4]; };
    struct Rect { float r[4]; };
    struct Splat { float a[4], b[4]; };

    // ---- The water.
    ugl::Target state_[2], src_, mask_, softA_, softB_;
    int cur_ = 0;
    int simW_ = 1, simH_ = 1;
    float stepAcc_ = 0.f;
    std::vector<Splat> splats_;
    std::vector<Rect> rects_;
    void splat(float x0, float y0, float x1, float y1, float r, float amp);
    void ripple(float x, float y, float r, float amp) { splat(x, y, x, y, r, amp); }
    void outlineRing(const int cells[][2], int n, float dx, float dy, float amp, float out = 0.12f);
    void simulate(int steps, float damp);

    // ---- The stones: a texture origin per piece, kept with the cells (so a stone keeps its grain once it lands).
    struct CellLook { float nx = 0, ny = 0, seed = 0; };
    int ids_[22][10] = {};
    CellLook look_[22][10];
    int nextId_ = 1;
    CellLook lookOf(int id, float relX, float relY) const;
    void syncIds(const Game& g);
    struct Sink { float x, y; int mask, type; CellLook l; float t, delay, dur; bool rung; };
    std::vector<Sink> sinks_;

    // ---- The falling piece's motion (its wake).
    int serial_ = 0, trackSerial_ = -1, trackRot_ = -1, trackType_ = -1;
    float trackX_ = 0, trackY_ = 0;
    float maskX_ = 0;            // the falling stone's place in the water, eased (a sideways jump sweeps it smoothly)
    int maskSerial_ = -1;
    bool rotated_ = false;
    int rotFrom_[4][2] = {};

    // ---- The pond: lily pads and lotus flowers, one per line cleared, kept for the song.
    struct Pad { float x, y, r, rot, grow, seed, alpha, delay; int kind; float fade; };
    std::vector<Pad> pads_;
    std::vector<Inst> padInst_;
    bool placePad(int kind, float nearX, float nearY, float delay);
    bool freeSpot(float x, float y, float r) const;

    // ---- Drawing.
    std::vector<Inst> stones_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int mask, int type, const CellLook& l, float alpha, int kind, float prog);
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
    Font fontCap_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.4f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progMask_ = 0, progBlur1_ = 0, progSplat_ = 0, progStep_ = 0, progWater_ = 0, progStone_ = 0, progPad_ = 0,
           progDown_ = 0, progBlur_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, maskVao_ = 0, maskVbo_ = 0, splatVao_ = 0, splatVbo_ = 0, stoneVao_ = 0, stoneVbo_ = 0, padVao_ = 0,
           padVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- The variant (eased to on a new song) and its light.
    struct Look {
        vec3 bed, bedFar, zenith, horizon, light, kerb, pad, pad2, lotus, lotusCore, ink, halo, accent, ghost;
        vec3 base[7], vein[7];
        float refl, spec, specW, polish, shadowK, caustK, aoK, glow, bloom, vignette;
        vec2 lightAt;
    };
    Look look_cur_, look_to_;
    bool lookSet_ = false;
    bool light_ = false;
    int variant_ = 0;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;
    Look makeLook(int v) const;
    Look nightOf(const Look& l) const;
    static Look mix(const Look& a, const Look& b, float t);

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropT_ = 99.f, gust_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    double lastHalfBeat_ = -1.0;
    float ringT_ = -1.f, ringA_ = 0.f;
    vec2 wind_{1.f, 0.3f};

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x51991E};
};

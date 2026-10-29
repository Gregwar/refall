#pragma once
// EMBROIDERY: cross-stitch on linen, a sampler stitched in the margins over the song (see embroidery.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class EmbroideryUniverse : public Universe {
public:
    ~EmbroideryUniverse() override;
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
    // ---- Pieces: cells of 4 x 4 cross stitches (instanced quads, board units).
    struct CellInst { float a[4], b[4], c[4]; }; // x, y, half, mask | color, alpha | kind, progress, glint, seed
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    float glint_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    struct Unpick { float x, y; int type, mask; float t, delay, seed; }; // a cell being unpicked (a clear, game over)
    std::vector<Unpick> unpick_;

    // ---- The sampler: a grid of stitches over the whole view (a quarter of a cell each), stitched by a needle in a
    // planned order; its state is a small texture (palette index, stitched 0 / half 0.5 / whole 1).
    struct Stitch { int i, j, pal; };
    std::vector<Stitch> plan_;
    size_t done_ = 0;          // crosses finished
    bool half_ = false;        // the next cross has its first strand
    float credit_ = 0.f;       // stitch steps owed (cleared rows)
    float stepT_ = 0.f;        // time since the last step
    std::vector<unsigned char> design_;
    int gw_ = 0, gh_ = 0;
    vec2 gO_{0, 0};            // the grid's origin (board units, a cell corner)
    bool designDirty_ = true;
    float designA_ = 1.f;      // the sampler's visibility (a new song unpicks the old one)
    bool designOut_ = false;
    void planDesign();
    void stitchStep();
    vec2 stitchPos(int i, int j) const { return vec2{gO_.x + (i + 0.5f) * 0.25f, gO_.y + (j + 0.5f) * 0.25f}; }
    // the needle
    vec2 needle_{0, 0}, needleFrom_{0, 0}, needleTo_{0, 0};
    float needleT_ = 1.f, needleA_ = 0.f;
    bool needleSet_ = false;
    struct SegInst { float a[4], b[4], c[4]; }; // x0, y0, x1, y1 | color, alpha | half width, kind
    std::vector<SegInst> segs_;
    void buildNeedle();

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
    GLuint progCloth_ = 0, progCell_ = 0, progSeg_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, segVao_ = 0, segVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0, designTex_ = 0;
    ugl::Target sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 cloth() const;        // the cloth now (linear), between the variant's and the drop's
    vec3 thread(int i) const;  // a thread now (linear): 0..6 the pieces, 7 leaves, 8 brown, 9 the outline ink

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dev_ = 0.f;          // the drop's cloth and threads 0..1
    float swell_ = 0.f;        // the drop's light swell
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    double lastBeat_ = -1.0;
    float barClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

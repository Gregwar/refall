#pragma once
// TILES: a great wall mosaic of thousands of tesserae; the pieces are glazed ceramic tiles, and a cleared row breaks
// into tesserae that fly out and are set, one by one along the andamento, into the figures of the mosaic (see
// tiles.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/tiles/mosaic.hpp"

class TilesUniverse : public Universe {
public:
    ~TilesUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    void forceFx(int slow, int quadClear) override { (void)slow; forceQuad_ = quadClear; }

private:
    // ---- The mosaic.
    tiles::Design design_;
    // A break's wall (bare): the ground laid over the well and its frame too (tesserae of a wall built without them,
    // over the frame's box; slot ids after the design's), the frame left out. Its own instance buffer.
    std::vector<tiles::Tessera> fill_;
    bool gbBare_ = false; // the G-buffer holds the break's wall
    bool bare_ = false;   // this frame is a break's
    int designVariant_ = -1;
    vec2 designC_{0, 0}, designH_{0, 0};
    float resizeWait_ = 0.f;
    void rebuild();
    // per tessera (slot id = index + 1; 0: none): set time, glint time, unset time, 0 (uploaded when it changes)
    std::vector<float> state_;
    bool stateDirty_ = true;
    int stateRows_ = 1;
    bool gbDirty_ = true;
    int progress_[2] = {0, 0};   // figure tesserae given out per side (order lists)
    int nextSide_ = 0;
    void resetFigures(float seconds);
    void setTess(int i, float t) {
        if (i < 0 || i >= (int)design_.tess.size()) return;
        state_[(i + 1) * 4 + 0] = t;
        state_[(i + 1) * 4 + 2] = 1e9f;
        stateDirty_ = true;
    }
    void glintTess(int i, float t) {
        if (i < 0 || i >= (int)design_.tess.size()) return;
        state_[(i + 1) * 4 + 1] = t;
        stateDirty_ = true;
    }
    bool isSet(int i) const { return state_[(i + 1) * 4] <= time_ && time_ < state_[(i + 1) * 4 + 2]; }

    // ---- The stack.
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    vec2 org_[22][10];     // each tile's place in its piece's painted pattern
    float flash_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    vec3 pieceColor(int type) const;

    // ---- A cleared row: its tiles crack, then break into fragments flying to their tesserae.
    struct Crack { int x, y, type; float px, py, seed, t; };
    std::vector<Crack> cracks_;
    struct Frag {
        int tess = -1;      // the tessera it becomes
        vec2 from, ctrl1, ctrl2;
        float appear = 0.f, launch = 0.f, land = 0.f; // seconds (time_): out of its broken tile, off, set
        float spin = 0.f, rot0 = 0.f;
        vec3 col0;          // the tile's glaze
        bool renew = false; // re-sets a tessera already there (a glint), the figures being complete
    };
    std::vector<Frag> frags_;
    struct FragInst { float a[4], o01[4], o23[4], b[4], c[4]; };
    std::vector<FragInst> fragInst_;
    void sendRow(const std::vector<vec3>& cellCols, const std::vector<float>& cellY, const std::vector<int>& targets, float t0,
                 float tap, bool renew);
    std::vector<int> takeTargets(int side, int n);

    // ---- Game over: the tiles fall off the wall, row by row from the top.
    struct Dead { int x, y, type; float px, py, seed; };
    std::vector<Dead> dead_;
    float overT_ = -1.f, over_ = 0.f;

    // ---- Small lights.
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    struct Sparkle { vec2 p; float t, life, size; vec3 col; };
    std::vector<Sparkle> sparkles_;
    struct Puff { vec2 p; float t, life, size; };
    std::vector<Puff> puffs_;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progGb_ = 0, progComp_ = 0, progCell_ = 0, progFrag_ = 0, progSprite_ = 0, progDown_ = 0, progBlur_ = 0,
           progFinal_ = 0, progShape_ = 0;
    GLuint quadVbo_ = 0, tessVao_ = 0, tessVbo_ = 0, bareVao_ = 0, bareVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, fragVao_ = 0, fragVbo_ = 0, spriteVao_ = 0,
           spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint gbFbo_ = 0, gbTex_[3] = {0, 0, 0};
    GLuint stateTex_ = 0, sketchTex_ = 0, geoTex_ = 0;
    int tessCount_ = 0, bareCount_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    void createGbuffer();
    void renderGbuffer(bool bare);
    void uploadDesign();

    // ---- Song, variant, the light.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 lightPos_{-12.f, 8.f, 11.f};
    float lightI_ = 0.8f;
    float lightPhase_ = 0.f;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropOn_ = false, realDrop_ = false;
    double dropBeat_ = 0.0;
    float dropStart_ = 0.f, dropT_ = 0.f, dropA_ = 0.f, dropEnv_ = 0.f, ignite_ = 0.f;
    float waveT_ = 99.f, waveA_ = 0.f;
    double lastWaveBeat_ = -1.0, lastGlintBeat_ = -1.0;
    float ringGlintT_ = -1.f;
    // The drop's show: waves turning the tesserae over every two beats (on the bar a new design, between them a full
    // turn showing their gold backs), the medallions chasing their light; the last wave lays the mosaic back.
    struct Sweep { double beat = -1e4; int kind = 0, from = 0, to = 0; bool spin = false; };
    bool show_ = false, showEnding_ = false;
    double showStart_ = 0.0, showNext_ = 0.0;
    int showStep_ = 0, showPat_ = 0, showCount_ = 0;
    float showK_ = 0.f;
    Sweep swA_, swB_;
    void updateShow(bool want);
    int ringGlintSide_ = 0;
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    double beat_ = 0.0;
    float bps_ = 2.f;
    bool hasSong_ = false;
    int forceQuad_ = -1;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{777};
};

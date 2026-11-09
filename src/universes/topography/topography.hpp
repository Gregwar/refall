#pragma once
// TOPOGRAPHY: a contour map whose terrain is the stack (see topography.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class TopographyUniverse : public Universe {
public:
    ~TopographyUniverse() override;
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

    // A map's colors (linear RGB) and a few numbers.
    struct Palette {
        vec3 paper, line, index, water, lake, tint0, tint1, tint2, wood, ink, accent;
        vec3 piece[7];
        float dark, lineAlpha, woodAmt, shade;
    };

private:
    // ---- The terrain: a grid over the whole view (board units), the displayed elevation easing toward the
    // target one (the ground, the stack's plateau and its foothills, the clears' debris, minus the erosion).
    float gx0_ = 0, gy0_ = 0, gs_ = 0.25f;
    int gw_ = 0, gh_ = 0;
    std::vector<float> base_, wood_, pondL_, pondT_, eros_, sed_, disp_, target_, mask_, tmp_, tmp2_, big_;
    std::vector<float> upload_;
    bool fresh_ = true;
    void buildGrid();
    void buildGround();
    // The ground keeps changing, slowly: its noise drifts and warps with time (the octaves each their own way).
    void evolveGround();
    bool spotFree(float x, float y) const;
    float evo_ = 0.f, evoSpeed_ = 0.7f;
    std::vector<float> nC_, wC_;
    int woodRow_ = 0; // the ground's noise and woodland on the coarse grid
    void updateTerrain(const Game& game, float dt, float wdt, bool bare);
    // ---- Rivers: a drainage network on a coarser grid (every other sample): each node drains to its lowest
    // neighbour, the rain gathers downstream, a river shows where enough of it runs, and it carves its valley.
    int cw_ = 0, ch_ = 0;
    std::vector<float> nh_, fill_, acc_, rainF_, rvis_;
    std::vector<int> down_, up_, order_;
    float netT_ = 9.f;
    struct Shower { float x, y, r, t, life, amt; };
    std::vector<Shower> showers_;
    void shower(float x, float y, float r, float amt, float life = 7.f);
    void drainage(float interval);
    struct RiverInst { float a[4], b[4]; };
    std::vector<RiverInst> rivers_;
    void buildRivers();
    float drizzle_ = 0.3f;

    // ---- The drop: peaks in the margins rising and falling with the bands (one moves every four bars), the land
    // heaving around the stack on the downbeat, the half-interval contours, the drop's colors.
    struct Peak { float x, y; int band; float w, wT; };
    std::vector<Peak> peaks_;
    float bandLv_[3] = {}, bandMax_[3] = {0.2f, 0.2f, 0.2f};
    float heave_ = 0.f, colAmt_ = 0.f, half_ = 0.f, relief_ = 0.f;
    int lastBar4_ = -1;
    bool placePeak(Peak& p);
    Palette basePal_{};
    std::vector<float> hudRects_; // pixels x0 y0 x1 y1: where the map keeps quiet behind the HUD
    void buildHudRects(const std::vector<HudText>& hud, bool bare);
    void splat(std::vector<float>& f, float x, float y, float v);
    float waterPercentile(float p);
    std::vector<float> sorted_;
    float sortT_ = 9.f;

    // ---- Pieces: one id per locked piece, so that two pieces of the same type never merge.
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct CellInst { float a[4], b[4], c[4], d[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, int mask, const vec3& fill, float alpha, int kind, float inset,
              float formA, float shrink, const vec3& line, float extra = 0.f);
    void buildBoard(const Game& g);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontItalic_, fontRoman_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progMap_ = 0, progRiver_ = 0, riverVao_ = 0, riverVbo_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint fieldTex_ = 0;
    ugl::Target sceneT_, riverT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{};
    uint64_t seed_ = 1;
    struct Spot { float x, y; int value; };
    std::vector<Spot> spots_;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;     // this peak is a drop (the flood), not only a chorus
    float dropAmt_ = 0.f;       // eased dropMode_
    float surge_ = 0.f;         // the drop's surge of water, settling over seconds
    float tetrisFlood_ = 0.f;
    float tint_ = 0.f;          // layer tints shown
    float water_level_ = -99.f; // the lakes' level (contour intervals)
    float waterOn_ = 0.f;
    float phase_ = 0.f, flow_ = 0.f, riverVis_ = 0.f, pond_ = 0.f;
    double lastBeat_ = -1.0;
    int beatCount_ = 0;
    float kickIn_ = 0.f;

    // ---- Effects.
    struct Fading { float x, y; int type, mask; float t; };  // a cleared cell sinking into the map
    std::vector<Fading> fading_;
    struct Impact { float x, y, t, amp; };                    // a hard landing's small swell of the ground
    std::vector<Impact> impacts_;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float worldTime_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

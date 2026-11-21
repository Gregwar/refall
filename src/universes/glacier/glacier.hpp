#pragma once
// GLACIER: an ice cave seen from inside; the blocks are ice cubes, cleared rows melt into the water that rises in the
// cave over the run, and the drop is a sun bursting from the tunnel behind the well (see glacier.cpp).
#include <cmath>
#include <cstdint>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class GlacierUniverse : public Universe {
public:
    ~GlacierUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    HudInk hudInk(const HudText& h) override;

    // A variant's colors (linear RGB) and amounts (floats only: palettes are blended float by float).
    struct Palette {
        vec3 deep, edge, far, open, floor, water, waterHi, shaft, glowA, glowB, well, grid, frame;
        vec3 ink, accent, veil;
        vec3 piece[7];
        vec3 sun, warm[7]; // the drop's sun, and the cubes' tints in its light
        float night, aurora;
    };

private:
    // ---- The water: its level (board units) rises with the lines melted (paceLines(): 50 at the scene's end).
    float lines_ = 0.f;
    float level_ = -10.6f, levelVel_ = 0.f;
    bool levelSet_ = false;
    float levelFor(float lines) const;
    float waterBottom_ = -11.6f, waterTop_ = -3.9f; // the level at 0 and at 50 paced lines
    // Ripples on the surface: x, age, amplitude, width (a melt's surge is wide, a drip narrow).
    struct Ripple { float x, age, amp, width; };
    std::vector<Ripple> ripples_;
    void ripple(float x, float amp, float width);
    float surface(float x) const; // the surface's height at x (the same function as the shaders')
    double lastRippleBeat_ = -1.0;

    // ---- Particles: drips of melting ice, frost motes, snow and flakes shaken from the icicles, mist.
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, alpha = 1.f, seed = 0.f;
        float stop = -99.f; // a drip: where it lands (the stack under it, the well's floor), unless the water is higher
        vec3 col{1.f};
        int kind = 0; // 0 frost mote, 1 drip, 3 flake, 4 mist, 5 spray, 6 splash crown
    };
    std::vector<Mote> motes_;
    float ambientAcc_ = 0.f;

    // ---- Cubes: each locked cell keeps its own id (its cracks and bubbles go with it as rows fall).
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    float flash_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void cube(float x, float y, float half, float seed, const vec3& tint, float alpha, int mode, float erase, float flash, float frost);
    void buildBoard(const Game& g);
    // a melting cube: where, its type and seed, its time, its stagger, its row and the lowest row of its clear (the
    // rows above fall into the room it leaves), its column, where its drip hangs (local x)
    struct Melting { float x, y; int type; float seed, t, delay; int row, low, col; float dripX; };
    std::vector<Melting> melting_;
    // The melt's water flowing (real time): a sheet spilling from the row's end down the outside of the glass into
    // the pool (never over the stack). Its top (y0) leaves when the cubes have run dry, its head (y1) runs down; both
    // accelerate to a cap.
    struct Stream {
        float x, y0, y1, v0 = 0.f, v1 = 0.f, t = 0.f, start, stop, hw, seed, surge = 0.f, sprayAcc = 0.f;
        vec3 col;
        bool sheet = false, arrived = false;
    };
    std::vector<Stream> streams_;
    void splash(vec2 p, const vec3& col, int n, float strength);
    float riseHold_ = 0.f; // the water rises once the melt reaches it
    struct OverCell { float x, y; int type; float seed, row; };
    std::vector<OverCell> overCells_; // the stack at the top out (the board is emptied)
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float stretch = 1.f, float seed = 0.f);

    // ---- HUD (lettered by the host; its plates here).
    float fade_ = 1.f;
    void drawPanels(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progCave_ = 0, progComp_ = 0, progCase_ = 0, progCell_ = 0, progSprite_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    ugl::Target caveT_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{};
    uint64_t seed_ = 1;

    // ---- Staging: a continuous level 0 calm .. 2 peak.
    int lvl_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f, energyS_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float dropFlash_ = 0.f; // the drop's first light, a burst fading over a second and a half
    float warm_ = 0.f;      // the drop's sun (0 none): the cave's warm light, the cubes' pastels
    bool wasDrop_ = false;
    float kickIn_ = 0.f, kickPrev_ = 0.f, glint_ = 0.f;
    double ownBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x1CE};
};

#pragma once
// TERRARIUM: a sealed glass jar on a windowsill, a tiny ecosystem growing in it over the song (see terrarium.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class TerrariumUniverse : public Universe {
public:
    ~TerrariumUniverse() override;
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

    // A variant's colors (linear RGB): floats only, palettes are blended float by float.
    struct Palette {
        vec3 soil, soil2, gravel, strata1, strata2, moss, moss2, leaf, leaf2, stem, cap, cap2, flower, flower2, bark,
            water, stone, bug, bugSpot, fly, sill, frame, wall, hill, hill2;
        vec3 ink, accent, veil;
        vec3 piece[7];
    };

    // The things growing in the jar: a slot each, born at a line of the scene (on the 50-line scale).
    enum SlotType { S_MOSS = 0, S_FERN, S_MUSH, S_TWIG, S_FLOWER, S_ROSETTE, S_CACTUS, S_PEBBLE, S_COUNT };
    struct Slot {
        int type;
        float x, z, size, rot, birth, param;
    };

private:
    // ---- Growth: paceLines() (50 at scene completion) -> each slot's growth, eased.
    float lines_ = 0.f, linesShown_ = -1.f, linesEased_ = 0.f;
    std::vector<float> growth_;          // per slot, 0..1 (eased)
    float pond_ = 0.f, tod_ = 0.f, moss_ = 0.f;

    // ---- The camera (world: the jar's radius is 1, its foot at the origin, y up).
    vec3 eye_{0, 1, 4}, fwd_{0, 0, -1}, right_{1, 0, 0}, up_{0, 1, 0};
    float tanH_ = 0.25f, aspect_ = 1.78f;
    void updateCamera();
    bool toScreen(const vec3& w, vec2& board) const; // a world point -> board units (false: behind the eye)
    vec3 boardRay(vec2 b) const;                     // the view ray through a board point

    // ---- Life: fireflies, the beetle, seeds drifting down and the sprouts they leave.
    struct Fly { vec3 home, p; float phase, freq, seed, glow, on; };
    std::vector<Fly> flies_;
    struct Seed { vec3 p, v, to; float age, life, seed; bool landed; };
    std::vector<Seed> seeds_;
    struct Sprout { float x, z, age, seed; };
    std::vector<Sprout> sprouts_;
    void addSeed(const vec3& from);
    bool pickSpot(float& x, float& z, float clearance);
    float soilHeight(float x, float z) const; // the same function as the shader's
    bool blocked(float x, float z, float r) const;
    struct Beetle {
        float x = -0.55f, z = 0.55f, head = 0.4f, speed = 0.f, walk = 0.f, look = 0.f, lookT = 0.f, pause = 2.f;
        float tx = -0.4f, tz = 0.6f, blink = 0.f, blinkT = 3.f, bob = 0.f;
    } bug_;
    void updateBeetle(float dt, float wm, float wp);

    // ---- Particles over the board: a hard drop's puff.
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 1.f, size = 0.1f, alpha = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;
    };
    std::vector<Mote> motes_;

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float seed);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type; float t; };
    std::vector<Fading> fading_, overCells_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font caps_, ital_;
    std::string todName_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progWorld_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target worldT_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;
    const std::vector<Slot>& slots() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f, rain_ = 0.f, allLit_ = 0.f;
    float fog_ = 0.5f, cond_ = 0.5f;
    float kickIn_ = 0.f, kickS_ = 0.f;
    double ownBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{8128};
};

#pragma once
// BIOME: glass cubes over a 3D diorama world that the cleared lines bring to life (see biome.cpp). Two worlds share
// the universe, each a set of variants: the sea (cubes of water thrown out onto dry sand, the sea rising, sea.cpp) and
// the land (cubes of seeds breaking open over parched clay, a jungle growing, land.cpp).
//
// This header holds what both worlds share: the palette, the state the frame and the worlds read (the camera, the
// song's level, the weather, the GL targets), and the interface a world implements. The shell (biome.cpp, render.cpp)
// owns the camera, the glass cubes of the stack, the well's pane, the rain, the HUD, the variant pick and
// the transitions; a world owns its ground, its life and what a clear does to it.
#include <memory>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

namespace biome {

// A variant's colors, linear RGB once loaded (floats only: palettes blend). Each world fills the shared block and its
// own; the other world's block stays zero.
struct Palette {
    // the sky and its light
    vec3 skyTop, skyLow, sun, cloud, stormSky, stormCloud, haze, dusk;
    // the well's pane (and its edge), the HUD (its panels, the ink and its dark and light choices, the accent)
    vec3 panel, edge, hudPanel, ink, inkDark, inkLight, accent;
    // the stack's cubes: what fills each piece type's cube, the world's own color per piece (the sea's life, the
    // seeds' flowers), what the glass reflects under the horizon
    vec3 cell[7], piece[7], envLow;
    // the sea (and its sand, the dust blowing over it: how thick)
    vec3 sea0, sea1, sea2, deep, foam, stormSea, hull, super_, sail, baccent, window, cont0, cont1, cont2, sand;
    float airDust;
    // the land
    vec3 dust, mist, soilDry, soilWet, soilCut, crack, grassDry, grassLush, leafA, leafB, leafFar, bark, dead;
    vec3 flowerA, flowerB, bird;
    // the sun or moon: azimuth (degrees, 0 into the scene, 90 to the right, 180 behind the eye), elevation (degrees;
    // negative: a fraction of the sky shown above the horizon), its disc's size; moon 0/1, stars, night; the sun's
    // strength, the sky's light, how dark the shadows are
    float sunAz, sunEl, sunR, moon, stars, night, lightK, ambK, shadowK;
    // the pane's opacity, cloudiness, a light HUD panel (the day seas)
    float panelA, clouds, lightPanel;
    // where the stack's cubes see the sun from (board units; 0, 0: from the 3D sun)
    float sunX, sunY;
};
void lerpPalette(Palette& a, const Palette& b, float k);
inline vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
inline vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
inline vec2 v2(float x, float y) { return vec2{x, y}; }

// The diorama's cut face, just behind the board plane: the world lies behind it.
constexpr float FRONT_Z = -1.0f;

// What the frame and the worlds share.
struct Ctx {
    int width = 1, height = 1;
    // ---- The camera: the layout's reference lens raised above the world, its image shifted back down (a shift lens),
    // so the board plane falls on the very same pixels while the world behind it is seen from above.
    mat4 view, vp, invVP, lightVP;
    vec3 eye{0.f, 4.5f, 34.6f}, camR{1.f, 0.f, 0.f}, camU{0.f, 1.f, 0.f};
    float horizon = 0.3f, camDist = 34.6f, px = 0.05f;
    vec2 viewC{0.f, 0.4f}, viewH{22.f, 12.4f};
    vec3 sunDir() const;                                       // toward the sun or moon
    float hwAt(float z) const { return viewH.x * (camDist - z) / camDist; } // the view's half width at depth z

    // ---- The song, the palette.
    Palette pal{};
    uint64_t seed = 0;
    Rng rng{5150};

    // ---- Time, the scene's level (continuous 0 calm .. 2 peak), the game.
    float time = 0.f, worldTime = 0.f;
    int level = 1;
    float lv = 1.f, energy = 0.5f;
    float wm() const { return smoothstepf(0.2f, 1.f, lv); }   // toward mid
    float wp() const { return smoothstepf(1.1f, 2.f, lv); }   // toward a peak
    float slow = 0.f;
    bool over = false;
    bool bare = false; // a break's frame: the world alone (no well, no stack, nothing thrown from it)
    Bridge bridge;
    float world() const { return 1.f - smoothstepf(0.f, 0.6f, saturate(bridge.amount)); } // the world shown (bridge)

    // ---- The weather, driven by the shell: a drop's storm (the sea's waves, the land's rain), its lightning, gusts,
    // the clouds' drift.
    float storm = 0.f, flash = 0.f, flashAz = 20.f, gust = 0.f, cloudT = 0.f;

    // ---- GL shared by the passes.
    GLuint quadVbo = 0, partVao = 0, partVbo = 0, shadowTex = 0, msFbo = 0;
    ugl::Target sceneT, bgT;   // the scene, and a copy of it behind the glass
    static constexpr int SHADOW_RES = 2048;
    // an instanced quad: `vec4s` vec4 attributes per instance (locations 1..)
    void instanced(GLuint& vao, GLuint& vbo, int vec4s) const;
    // the sky's uniforms (SKY_GLSL): the sun's light scaled by `sunK`, the clouds' cover, their band's lower edge (from,
    // to: how high above the horizon they start), the sky's gradient height, the dust's veil over it
    void skyUniforms(GLuint p, float sunK, float cloudCov, vec2 cloudBand, float skyH, float dustK) const;
    // the particles (PART_VS / PART_FS, 16 floats each), the program in use with its world uniforms
    void drawParts(GLuint p, const std::vector<float>& v) const;

    // ---- The lens (LENS_FS), a world's last pass over what it drew, before the shell's rain and the stack: the bloom,
    // the sun's glare, the dark smoky glass of the well (a fine grid, a bright rim) and of the previews' panels.
    struct Lens {
        float deepY = -1e4f;               // under it the glass darkens toward `panel` (the sea's depths)
        float glare = 0.f, bloom = 0.25f;  // the sun's glare, the bloom
        float tint = 0.82f, deepVeil = 1.f; // how much of the glass is its own tint, the depths' share of it
        float grid = 0.24f;                // how bright the well's grid
        vec3 glareCol, frostCol, panel;    // the glare's light, the glass's tint, its tint over the depths
    };
    void drawLens(const Lens& l);
    GLuint progLensDown = 0, progLensBlur = 0, progLens = 0;
    ugl::Target quarterT[2], eighthT[2];   // the world at a quarter (the frosted glass), its brightest at an eighth
};

// A world: its ground and its life, what the clears do to it, its variants.
class World {
public:
    explicit World(Ctx& ctx) : ctx(ctx) {}
    virtual ~World() = default;
    virtual bool init() = 0;   // GL resources (the context is current); freed by the destructor

    virtual Palette palette(int i) const = 0;
    virtual int pickVariant(const SongLook& look) const = 0;   // the variant (in this world) for a song

    // A song in this world: `fresh` when the world was not on screen before (it starts over at once).
    virtual void setSong(const SongLook& look, float seconds, bool fresh) = 0;
    virtual void refresh(float strength) { (void)strength; }
    virtual void onEvent(const GameEvent& ev, const Game& game) = 0;
    virtual void breakOut(const vec3& p, int type) = 0;   // game over: a cracked cube leaves the stack
    virtual void dropHit() = 0;                           // the drop's hit (the shell flashes the lightning)
    virtual void kick() = 0;                              // a kick at a peak
    virtual void grown() = 0;                             // a whole stack filled at once while shown (a review)
    virtual void simulate(const Game& game, const MusicState& music, float dt, float wdt) = 0;

    virtual float eyeY() const = 0;                       // the camera's height
    virtual float stormIn() const { return 0.8f; }        // seconds for a drop's storm to come in
    virtual float cloudDrift() const { return 0.04f; }    // the clouds' drift in a storm
    virtual void shadowPass() = 0;     // sets ctx.lightVP, draws the casters into the bound shadow map
    virtual void drawWorld() = 0;      // into the multisampled target with depth (the shadow map on unit 5)
    virtual void drawAfterPane() {}    // over the well's pane, before the stack (still multisampled)
    virtual void drawAfterStack() {}   // over the stack (the scene target)
    // the world's color behind a point of the board plane (the HUD's ink), above the horizon from `ndc` up
    virtual vec3 behind(float bx, float by, float ndc) const = 0;
    virtual float paneDeepY() const { return -1e4f; }   // under it the pane darkens (the sea's depths)
    virtual bool ownPane() const { return false; }      // it draws the well's pane itself (the shell: only the bridge)
    virtual float rainFloorY() const { return -1e4f; }  // no rain under it (the sea's cut face)
    virtual vec3 rainColor() const = 0;
    virtual float rainSlant() const { return 1.f; }
    virtual GLuint cellProgram() const = 0;   // the stack's glass cubes (CELL_FS with this world's fill)
    virtual void cellUniforms(GLuint p) const { (void)p; }
    virtual vec3 background(const Palette& p) const = 0; // the transitions' backdrop

protected:
    Ctx& ctx;
};

} // namespace biome

class BiomeUniverse : public Universe {
public:
    BiomeUniverse();
    ~BiomeUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override { refresh(1.f); }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { c_.bridge = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // The variants of both worlds, in registry order: the sea's, then the land's.
    static int variantCount();
    static const char* variantName(int i);

private:
    biome::Ctx c_;
    std::unique_ptr<biome::World> worlds_[2];
    biome::World* w_ = nullptr;
    int variant_ = 0;           // over both worlds
    int worldOf(int v) const;   // 0 sea, 1 land
    int localOf(int v) const;   // its index in its world
    int pickVariant(const SongLook& look) const;
    bool palSet_ = false;

    // ---- The stack, ghost, falling piece and previews: glass cubes on the board plane, drawn over the world.
    struct CellInst { float a0[4], a1[4], a2[4], a3[4]; };
    std::vector<CellInst> cells_;
    void cube(float x, float y, float size, int type, float alpha, bool ghost, float seed, float crack = 0.f);
    void flushCells();
    // A game over: the stack cracks row by row from the top, each cube then leaves it as a cleared one does.
    struct Dying { float x, y, delay, t; int type; bool gone; };
    std::vector<Dying> dying_;
    int boardCount_ = -1; // the stack's cells last frame (-1: not seen yet)
    void watchBoard(const Game& game);

    void simulate(const Game& game, const MusicState& music, float dt, float wdt);
    float stormLin_ = 0.f, dropPending_ = 0.f, sinceHit_ = 9.f;
    bool stormWasOn_ = false, sectionDrop_ = false, sectionKnown_ = false;
    double lastBar_ = -1.0;
    float prevKick_ = 0.f, kickIn_ = 0.f, sinceKick_ = 9.f;
    bool hasLevel_ = false;
    float lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;

    // ---- The HUD (its lettering: hudLine, the host letters it).
    float fade_ = 1.f; // the frame's fade in, the HUD's own (it was lettered under the post's fade and dim)
    void drawHud(const std::vector<HudRect>& panels);
    float bgLuma(float bx, float by) const;

    // away_: the scene's setup and teardown (0 the lens in place .. 1 risen high above the diorama and drawn back)
    float away_ = 0.f;
    void setCamera();
    void drawRain();
    void drawPane();

    // ---- GL.
    GLuint progVeil_ = 0, progRain_ = 0, progShape_ = 0, progPost_ = 0;
    GLuint cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint msColor_ = 0, msDepth_ = 0, shadowFbo_ = 0;
    int samples_ = 4;

    // ---- Game state seen by the universe.
    bool slowOn_ = false;
    float dimT_ = 0.f;
};

#pragma once
// SMOKE: a dark stage, slow smoke and one hard spotlight (see smoke.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SmokeUniverse : public Universe {
public:
    ~SmokeUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override;
    void levelUp() override;
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void drop(float strength) override;
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // A lighting setup (linear RGB once loaded; floats only, so two setups blend).
    struct Palette {
        vec3 light, smoke, fill, panel, rim, ink, accent;
        vec3 piece[7];
        vec3 lpos, aim;   // the lamp (board space, z toward the viewer) and where it points
        float gain, cone, haze, smokeAmt;
    };

private:
    struct Tile { float a0[4], a1[4], a2[4], a3[4]; };
    struct Sprite { float a0[4], a1[4], a2[4]; };
    struct Vert { float x, y, r, g, b, a; };
    std::vector<Tile> tiles_;
    std::vector<Sprite> sprites_, motes_, blades_, wisps_;
    void tile(float x, float y, float hw, float hh, const vec3& c, float alpha, int kind, float e0 = 1.f, float e1 = 0.f,
              float e2 = 0.f, float e3 = 0.f, float angle = 0.f, float age = -1.f);

    // ---- The block looks (REFALL_SMOKE_BLOCK=0..6; 2, the default, the acrylic): 0 the satin blocks, 1 props lit by the
    // spotlight, 2 acrylic, 3 neon tubes, 4 boxes of smoke, 5 stage lenses, 6 glass cubes holding smoke. Each cell
    // remembers its piece (its serial), its links to the cells of the same piece (L R U D UL UR DL DR, set when it
    // locks, cut by a clear), its place in the piece and the piece's seed: a block looks the same falling, locked and
    // after rows fall.
    int blockStyle_ = 2;
    int ids_[22][10] = {};
    unsigned char links_[22][10] = {}, rel_[22][10] = {};
    float pseed_[22][10] = {};
    int serial_ = 1, nextId_ = 1 << 20;
    GLuint occTex_ = 0; // the stack's heights (style 1: the hard shadows the props cast on each other)
    float camDist_ = 36.f;
    vec3 fall_[4];  // the falling piece's cells (board xy, prop height): style 1's shadows
    int fallN_ = 0;
    void syncIds(const Game& g);

    void simulate(const Game& game, const MusicState& music, float dt, float wdt);
    void buildBoard(const Game& game);
    void buildFx();
    void setWell(GLuint prog) const;
    void drawSmoke();
    void drawPanels(const std::vector<HudRect>& panels);
    int pickVariant(const SongLook& look) const;
    void puff(vec2 p, vec2 v, float r, float grow, float life, float alpha, float delay = 0.f, float stretch = 1.f, int kind = 0);
    float spotAt(const vec3& p) const; // the spotlight's strength at a point (0..~1.3)
    vec3 lampPos() const;
    vec3 lampAxis() const;

    // ---- GL.
    GLuint progSmoke_ = 0, progComp_ = 0, progShape_ = 0, progTile_ = 0, progSprite_ = 0, progDown_ = 0,
           progUp_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, tileVao_ = 0, tileVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           volTex_ = 0, noiseTex_ = 0;
    ugl::Target smokeT_, sceneT_;
    static constexpr int BLOOM_MIPS = 5;
    ugl::Target bloomT_[BLOOM_MIPS];
    void createTargets();
    void buildNoise();
    mat4 vp_;
    vec2 viewC_{0, 0.4f}, viewH_{22, 12.4f};
    float px_ = 0.05f;

    // ---- Song, variant, palette.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{};   // eased toward the variant
    Palette cur_{};   // this frame's (the variant, its pin spot in slow motion, the lamp cooling at game over)
    uint64_t seed_ = 0;
    float seedF_ = 0.f;

    // ---- Scene level: continuous 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;

    // ---- The smoke and the light.
    float worldTime_ = 0.f, lastEased_ = -1.f, lastBeatPhase_ = 0.f, beats_ = 0.f;
    vec3 flow_{0.f, 0.f, 0.f};   // the smoke's offset in the noise (it rises and drifts)
    float swirl_ = 0.f, bass_ = 0.f, surge_ = 0.f, surgeIn_ = 0.f, beatGlow_ = 0.f; // surge_ eases to surgeIn_
    float prevKick_ = 0.f, kickIn_ = 0.f, sinceKick_ = 9.f;
    float swell_ = 0.f;            // the light pushed up (phase changes, level up, quad)
    float blastT_ = 9.f;           // a quad blows the smoke outward from the well
    float iris_ = 0.f;             // extra cone opening (peak, quad)

    // ---- Effects.
    struct Puff { vec2 p, v; float r, grow, t, life, alpha, seed, delay, stretch; int kind; float minGlow = 0.f; int tint = -1; float drag = -1.f, fall = 0.f; }; // tint: a piece type whose color the smoke takes, -1 none; drag (< 0: the kind's own) and fall (gravity) for dust that sags back
    std::vector<Puff> puffs_;
    bool inDrop_ = false; // a drop: smoke spills out of the beam for its whole length
    float spill_ = 0.f;
    int lastDrop_ = -1; // rows of the last hard drop, until its lock (-1: none)
    // A hard-dropped piece's lock, waiting for the frame (the rows its lock cleared are known only then): its dust
    // only comes out where it rests on something.
    std::vector<GameEvent::CellInfo> landed_;
    std::vector<int> landedFull_;
    int landedDrop_ = 0;
    void landDust(const Game& game);
    struct Dying { float x, y, t, delay; int type; bool over; float seed; float big = 1.f, cx = 0.f; int pack = 0; float pseed = 0.f; }; // pack, pseed: its links, place and piece seed (the block looks) // big: the plume's size (a quad), cx: the row's middle
    std::vector<Dying> dying_;
    struct Mote { vec3 p; float phase, size; };
    std::vector<Mote> moteList_;
    struct Sweep { float y, t, dir; };
    std::vector<Sweep> sweeps_;

    // ---- Game state seen by the universe.
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f, overFog_ = 0.f;
    float lamp_ = 1.f;      // the lamp on (game over, the setup and the teardown turn it off)
    float stageIris_ = 1.f, stageSmoke_ = 1.f, stageRise_ = 0.f; // the setup / teardown: iris, smoke amount, smoke lift
    float dimT_ = 0.f;
    bool bare_ = false; // this frame is a break's: the stage alone, no well's glass, no blocks, no smoke of theirs
    Bridge bridge_;
    Rng rng_{1010};
};

#pragma once
// CELESTIAL OCEAN: a sea of clouds at night, where cleared rows rise as creatures of light (see skyocean.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SkyOceanUniverse : public Universe {
public:
    ~SkyOceanUniverse() override;
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

    // A night's colors (linear RGB) and its moon (floats only: palettes are blended float by float).
    struct Palette {
        vec3 skyTop, skyMid, horizon, haze, moon, moonGlow, cloudLit, cloudMid, cloudShadow;
        vec3 life[3];      // the creatures' light: main, second, the bright core
        vec3 aur1, aur2;   // the aurora's curtains (low, high)
        vec3 ink, accent;  // the HUD's lettering, its highlight
        vec3 piece[7];
        float aurora;      // how much aurora the sky holds
        float moonX, moonY, moonR; // the moon: x as a fraction of the half width, y as a fraction of the sky's height
    };

    enum Kind { FISH = 0, JELLY = 1, MANTA = 2, WHALE = 3 };

private:
    // ---- Creatures of light: one per clear (a school of fish, jellyfish, a manta ray, a whale for a quad).
    struct Creature {
        int kind = FISH;
        float size = 1.f;       // body length (board units)
        vec2 start{0, 0};       // where it rises from (under the cloud tops)
        float cruiseY = 4.f;    // the height it swims at
        float dir = 1.f;        // swimming right (1) or left (-1)
        float speed = 1.f;      // board units a second once under way
        float riseT = 3.f;      // how long it takes to rise out of the clouds
        float age = 0.f;
        float life = 30.f;      // then it fades away
        float phase = 0.f, phaseRate = 1.f; // its body's motion (tail beats, wing flaps, the bell's pulse)
        float seed = 0.f;
        float bobA = 0.4f, bobW = 0.3f;
        float surface = -4.f;   // the cloud tops it breaks through (board y)
        float fade = 1.f;       // 0: gone
        bool leaving = false;
        float diveAt = -1.f;    // >= 0: diving back into the clouds since that age
        float dim = 1.f;        // the far ones are dimmer
        int count = 1;          // fish in a school
        float flash = 1.f;      // the light it rises with
        float trail = 0.f;      // trail motes owed
    };
    std::vector<Creature> creatures_;
    vec2 pathAt(const Creature& c, float t) const;
    void spawnCreature(int kind, float side, float scale, bool far);
    struct Spawn { float delay; Creature c; };  // a clear's creature, rising once its motes reach the clouds
    std::vector<Spawn> spawns_;
    float ambientT_ = 6.f;
    float nextSide_ = 1.f;

    // ---- The drop's whale: a giant passing slowly across the whole sky, diving back into the clouds after.
    struct Giant {
        bool on = false;
        float x = 0.f, y = 0.f, dir = 1.f, age = 0.f;
        float diveAt = -1.f;
        float alpha = 0.f;
        float phase = 0.f;
        float size = 30.f;
        float trail = 0.f;
    } giant_;
    vec2 giantPos(float age) const;
    void startGiant();

    // ---- Motes of light: trails, the plankton rising from the clouds, the cleared rows flowing out of the well.
    struct Mote {
        vec2 p{0, 0}, v{0, 0};
        float age = 0.f, life = 3.f, size = 0.1f, bright = 1.f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;         // 0 drifting, 1 flying along a curve (a cleared cell), 2 sparkle
        vec2 from{0, 0}, ctl{0, 0}, to{0, 0};
    };
    std::vector<Mote> motes_;
    void addMote(const Mote& m);

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase, float shine);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t; };
    std::vector<Fading> fading_, overCells_;

    // ---- Instanced creatures and sprites.
    struct BodyInst { float a[4], b[4], c[4], d[4]; };
    std::vector<BodyInst> bodies_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_;
    void body(int kind, vec2 p, float size, float angle, float flip, const vec3& col, float alpha, float phase,
              float seed, float surface, float glow);
    void sprite(vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f, float param2 = 0.f);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // its halo: the usual one, the celebrations' and the dialogs' fuller one

    // ---- GL.
    GLuint progSky_ = 0, progComp_ = 0, progBody_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, bodyVao_ = 0, bodyVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0;
    ugl::Target skyT_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;
    float horizon_ = -1.f;
    vec2 moonP_{-15.f, 7.f};
    float moonR_ = 1.6f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    bool wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickGlow_ = 0.f;
    float quadGlow_ = 0.f;
    float drift_ = 0.f;
    float twinkleBeat_ = 0.f;
    double lastBeat_ = -1.0;
    float beatClock_ = 0.f;
    int beatCount_ = 0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{7731};
};

#pragma once
// LOST PANDA: a little kawaii panda lost in the city at night finds its way home, one cleared line at a time, in one
// continuous world: the rainy street, the shopping street, down the stairs into the subway, a train through the
// tunnel and out at dawn, the industrial district, the fields, the bamboo forest, and its family waiting in a glade
// at sunset (see lostpanda.cpp).
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class LostPandaUniverse : public Universe {
public:
    ~LostPandaUniverse() override;
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

    // A panda's pose: every value eased toward its target, never set abruptly.
    struct Pose {
        float walk = 0, phase = 0, mood = 0, turn = 0.5f, tilt = 0, lookX = 0.5f, lookY = 0, blink = 0, sit = 0,
              wipe = 0, tear = 0, hop = 0, lean = 0, arms = 0, joyEyes = 0, hug = 0;
        float earN = 0, earF = 0, earVN = 0, earVF = 0; // the ears' springs
        float breath = 0, wind = 0.3f;
        float blinkT = 2.f, blinkAge = 9.f;
        float munch = 0, chew = 0;
    };

private:
    // ---- The way home, one continuous world: P the cub's place along it (its world x), C the camera's (the world x
    // at the cub's spot on the screen), camY_ the camera's height (the world y at the screen's ground line - GY).
    double P_ = 0.0, C_ = 0.0, vel_ = 0.0, stroll_ = 0.0, creep_ = 0.0;
    float camY_ = 0.f;
    float viewY_ = 0.f; // the height drawn: camY_, raised into the sky in the setup and the teardown
    // ---- The subway train: away, arriving, waiting (doors), riding with the cub, at the terminus, leaving, gone.
    enum { T_AWAY, T_ARRIVE, T_WAIT, T_OUT, T_RIDE, T_TERM, T_LEAVE, T_GONE };
    int train_ = T_AWAY;
    double trainX_ = -1000.0;     // the train's rear (world x)
    float rideS() const;          // the ride's length: T_RIDE_S, shorter in a shorter song's scene
    float trainT_ = 0.f, door_ = 0.f, boardT_ = 0.f, leaveT_ = 0.f, trainClock_ = 0.f, rideT_ = 0.f;
    float missedT_ = 0.f;          // since the cub missed a train (it watches it go)
    // ---- The end: the camera stops, the cub sees its family on the right and runs to them.
    float noticeT_ = 0.f;
    // ---- Fed on the way: munching a snack, strength gained.
    float munchT_ = 0.f, strength_ = 0.f;
    float kidRoll_ = 0.f;          // the floor's tilt under the cub (its car on the grade)
    struct Heart { vec2 p{0, 0}; float age = 0.f, life = 4.f, size = 0.3f, seed = 0.f; vec3 col{1.f}; };
    std::vector<Heart> hearts_;
    float heartT_ = 0.f;
    double lineTarget_ = 0.0;
    float lineDelay_ = 0.f;       // a clear's walk starts once the panda has seen its lights coming
    float lines_ = 0, linesShown_ = -1, linesWalk_ = 0; // paceLines(): 50 at scene completion
    bool placed_ = false;
    float xP_ = -16.f;            // the panda's spot on the screen (board x)
    float kidH_ = 6.6f, famS_ = 1.f; // the cub's height, the family's scale (both from the free band's width)
    float arriveT_ = 0.f;          // since the cub reached home
    // the reunion's places on the screen (board x), where the parents come in from, the cub's place along the way
    float dadEndX_ = -19.f, momEndX_ = -16.f, kidEndX_ = -13.5f, enterX_ = -24.f, momX_ = -24.f, dadX_ = -24.f;
    double pEnd_ = 374.5;
    std::string stageName_;
    int stageAt_ = 0;

    // ---- The little panda's life: an idle action at a time, a pose eased toward it.
    Pose kid_, mom_, dad_;
    int act_ = 0;
    float actT_ = 0.f, actDur_ = 3.f;
    float joy_ = 0.f;             // a drop's, a quad's lift of the spirits (fading)
    float reunion_ = 0.f;         // 0 .. 1 as the family comes into view and the cub runs to them
    void chooseAct(float mood, int stage, bool home);
    void updateKid(float dt, float stageMood, int stage, bool travelling, float speed);
    void updatePose(Pose& p, float dt, const Pose& target, float rate);

    // ---- The butterfly the cub follows (fields, forest, home).
    vec2 fly_{0, 0}, flyV_{0, 0};
    float flyA_ = 0.f, flyFlap_ = 0.f;

    // ---- Lights: a cleared row's lights of hope drifting to the panda (behind the well), fireflies.
    struct Light {
        vec2 a{0, 0}, b{0, 0}, ctl{0, 0};
        float age = 0.f, life = 2.f, size = 0.3f, seed = 0.f;
        vec3 col{1.f};
        int kind = 0;             // 0 a light, 1 a bamboo snack, 2 a big one (a quad)
        bool fed = false;
    };
    std::vector<Light> lights_;
    struct Mote { vec2 p{0, 0}, v{0, 0}; float age = 0.f, life = 1.f, size = 0.1f, alpha = 1.f; vec3 col{1.f}; };
    std::vector<Mote> motes_;

    // ---- Pieces.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, const vec3& tint, float alpha, int mode, float erase, float glow = 0.f);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type; float t; };
    std::vector<Fading> fading_;
    struct SpriteInst { float a[4], b[4], c[4]; };
    std::vector<SpriteInst> sprites_, bgSprites_, fgSprites_;
    void sprite(std::vector<SpriteInst>& out, vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f,
                float param2 = 0.f);
    void drawSprites(const std::vector<SpriteInst>& v, bool additive);
    void drawPanda(const Pose& p, vec2 feet, float h, float face, float adult, float alpha, float haze, bool reflect, int part);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    float fade_ = 1.f; // the frame's fade in (the post's, the lettering's too)
    text::Lettering lettering_[2]; // the halo at .75 (most lines), at .85 (the celebrations, the dialogs)

    // ---- GL.
    GLuint progWorld_ = 0, progFront_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0,
           progPost_ = 0, progPanda_ = 0;
    GLuint quadVbo_ = 0, quadVao_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0;
    ugl::Target worldT_, sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickS_ = 0.f;
    double ownBeat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

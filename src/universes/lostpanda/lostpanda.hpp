#pragma once
// LOST PANDA: a little panda lost in the city at night finds its way home, one cleared line at a time: a rainy
// street, the shopping street's crossing, the subway, a night train, the canal at dawn, the fields at the city's
// edge, the bamboo forest, and its family waiting in a glade at sunset (see lostpanda.cpp).
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

    // A panda's pose: every value eased toward its target, never set abruptly.
    struct Pose {
        float walk = 0, phase = 0, mood = 0, turn = 0.5f, tilt = 0, lookX = 0.5f, lookY = 0, blink = 0, sit = 0,
              wipe = 0, tear = 0, hop = 0, lean = 0, arms = 0, joyEyes = 0, hug = 0;
        float earN = 0, earF = 0, earVN = 0, earVF = 0; // the ears' springs
        float breath = 0, scarfT = 0, wind = 0.3f;
        float blinkT = 2.f, blinkAge = 9.f;
    };

private:
    // ---- The way home: P the panda's place along it, C the camera's (both board units at the ground's depth).
    double P_ = 0.0, C_ = 0.0, vel_ = 0.0, stroll_ = 0.0;
    double lineTarget_ = 0.0;
    float lineDelay_ = 0.f;       // a clear's walk starts once the panda has seen its lights coming
    int lines_ = 0, linesShown_ = -1, linesWalk_ = 0;
    bool placed_ = false;
    float xP_ = -16.f;            // the panda's spot on the screen (board x)
    std::string stageName_;
    int stageAt_ = 0;

    // ---- The little panda's life: an idle action at a time, a pose eased toward it.
    Pose kid_, mom_, dad_;
    int act_ = 0;
    float actT_ = 0.f, actDur_ = 3.f;
    float joy_ = 0.f;             // a drop's, a Tetris's lift of the spirits (fading)
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
    std::vector<SpriteInst> sprites_, bgSprites_;
    void sprite(std::vector<SpriteInst>& out, vec2 p, float size, const vec3& col, float alpha, int mode, float param = 0.f,
                float param2 = 0.f);
    void drawSprites(const std::vector<SpriteInst>& v, bool additive);
    void drawPanda(const Pose& p, vec2 feet, float h, float face, float adult, float alpha, float haze, bool reflect);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font sans_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progWorld_ = 0, progComp_ = 0, progSprite_ = 0, progVeil_ = 0, progCell_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0, progPanda_ = 0;
    GLuint quadVbo_ = 0, quadVao_ = 0, spriteVao_ = 0, spriteVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0,
           shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
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
    float trainT_ = 0.f;
    double trainStop_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

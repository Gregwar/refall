#pragma once
// SONAR: a dark sea where shapes surface from the black when an echo returns from them (see sonar.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/sonar/scene.hpp"

class SonarUniverse : public Universe {
public:
    ~SonarUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    struct Inst { float a[4], b[4], c[4]; };
    struct MoteInst { float a[4], b[4]; };

    // ---- The sunken world (3D) and the camera gliding along its loop.
    sonar_scene::Scene scene_;
    float pathS_ = 0.f, speed_ = 0.9f, speedV_ = 0.f; // arc length along the loop, speed (units/s) and its rate
    float rise_ = 0.f;         // the setup / teardown: the camera above its glide (sinking into place, rising away)
    vec3 camPos_, camF_{0, 0, 1}, camR_{1, 0, 0}, camU_{0, 1, 0};
    mat4 wvp_;
    float decay_ = 1.5f;
    float wTan_ = 0.5f;               // tan of half the world camera's vertical field of view
    void updateCamera(float wdt, float targetSpeed, float dt);
    bool toBoard(const vec3& p, vec2& board) const; // world -> board units (the plane the well is drawn on)
    bool toNdc(const vec3& p, vec2& ndc, float& depth) const;
    bool seabedAt(float nx, float ny, vec3& hit) const; // the seabed seen at a screen point (ndc)

    // ---- The echoes: pings on the beat grid, each lighting what lies around one point for a moment.
    struct Ping { vec3 p; float age = -1.f, strength = 0, reach = 4.f; };
    Ping pings_[8];
    int pingNext_ = 0;
    long long pingSlot_ = -1;
    int pingSide_ = 1;
    int lastAnchor_[2] = {-1, -1};
    void ping(const vec3& p, float strength, float reach);
    void pingSomewhere(int side, float strength, float reach, int maxLayer, float cityChance);

    // ---- The chart: each cleared row maps a spot of the seabed (later, of the city) for good.
    struct Chart { vec3 p; float r, amount = 0, delay = 0; bool fading = false; };
    std::vector<Chart> charts_;
    int chartCount_ = 0;
    int chartSpot(int side, float delay);

    // ---- Light sinking from a cleared row to the spot it maps; the contact of a hard drop.
    struct Mote { vec2 p0, p1, p; float t = 0, life = 1.4f, delay = 0, size = 0.18f, peak = 1.f, stretch = 1.f; vec3 col; bool glint = false; bool world = false; vec3 target; };
    std::vector<Mote> motes_;
    struct Sink { float x, y; vec3 col; float t, dur, delay; int kind; };
    std::vector<Sink> sinks_;

    // ---- Each block's bubbles keep their own seed for its whole life: handed over from the falling piece when it
    // locks, moved down with the rows a clear drops (0: not known yet).
    float seeds_[Game::H][Game::W] = {};
    struct FallCell { int x, y; float seed; };
    FallCell fall_[4] = {};
    int fallN_ = 0, fallType_ = -1;
    float pieceSeed_ = 1000.5f;

    // ---- Fish: small schools crossing the glide's path, passed by as the camera glides on.
    struct Fish { vec3 off, wob; float ph, rate, size, tail; };
    struct School { vec3 c, v; float age = 0, life = 30.f, turn = 0; std::vector<Fish> fish; };
    std::vector<School> schools_;
    std::vector<Inst> fishInst_;
    float schoolClock_ = 0.f;
    void updateFish(float wdt);

    // ---- Drawing.
    std::vector<Inst> cells_;
    std::vector<MoteInst> moteInst_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int kind, const vec3& col, float alpha, float flash, float prog, float seed = 0.f);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    void drawSub(const Frame& f);    // the wireframe submarine above the corner lettering
    void line(float x0, float y0, float x1, float y1, float w, const vec3& c, float a);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progWater_ = 0, progMesh_ = 0, progSnow_ = 0, progWell_ = 0, progPiece_ = 0, progMote_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progFish_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, moteVao_ = 0, moteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           meshVao_ = 0, meshVbo_ = 0, meshIbo_ = 0, snowVao_ = 0, snowVbo_ = 0, fishVao_ = 0, fishVbo_ = 0;
    GLsizei meshCount_ = 0, snowCount_ = 0;
    void setWorldUniforms(GLuint prog);
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant.
    int variant_ = 0;
    vec3 waterTop_, waterLow_, echo_, echoCity_, dropEcho_, stone_;
    vec3 waterTopT_, waterLowT_, echoT_, echoCityT_, dropEchoT_, stoneT_;
    vec3 pieceCol_[7], pieceColT_[7];
    bool colSet_ = false;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, swell_ = 0.f, quadGlow_ = 0.f;
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    double beat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    bool bare_ = false; // this frame is the world alone (a break): the sea and its echoes, no well, no blocks
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x50A4};
};

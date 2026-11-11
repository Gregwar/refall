#pragma once
// ISLANDS: one continuous flight over an endless tropical archipelago, like drone footage, behind the well; the
// pieces are polished sea glass (see islands.cpp).
#include <memory>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/islands/world.hpp"

class IslandsUniverse : public Universe {
public:
    ~IslandsUniverse() override;
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

private:
    using Inst = isl::Inst;
    // ---- The world.
    std::unique_ptr<isl::World> world_;
    int builtVariant_ = -1;
    void buildWorld();
    void uploadMeshes();

    // ---- The flight: the camera rides the route, its altitude, speed, heading and bank on critically damped
    // springs (smooth to the second order), from the music and what is ahead.
    struct Spring {
        float x = 0.f, v = 0.f;
        void step(float target, float omega, float dt);
    };
    double s_ = 0.0;                       // along the route
    Spring logAlt_, speed_, yawOff_, pitch_, bank_;
    float alt_ = 60.f, yaw_ = 0.f, vel_ = 20.f;
    float calmPhase_ = 0.f, cruisePhase_ = 0.f, diveT_ = 99.f, buildK_ = 0.f, heroW_ = 0.f, offTgt_ = 0.f;
    bool camInit_ = false;
    vec3 eye_{0, 60, 0}, fwd_{1, 0, 0}, up_{0, 1, 0}, right_{0, 0, 1};
    float curv_ = 0.f, near_ = 0.5f, far_ = 40000.f, fov_ = 50.f;
    void fly(float dt, float wdt, const std::string& section);
    float floorAhead(const vec3& from, float yaw, float v) const;

    mat4 view_, proj_, vp_, invVp_, mirVp_, lightVp_;
    mat4 bvp_;                             // the board's (the layout's)

    // ---- Life: boats, the seaplane, the birds, the clouds, the lanterns.
    struct Boat {
        int kind = 0;                      // 0 sailboat, 1 catamaran, 2 speedboat, 3 kayak
        vec3 p{0, 0, 0};
        float head = 0, speed = 0, cruise = 0, turn = 0, phase = 0, len = 10, beam = 3, age = 0;
        vec3 col{1, 1, 1};
        int stripe = 0;
        bool moored = false;
    };
    std::vector<Boat> boats_;
    void updateBoats(float dt);
    void spawnBoat(bool initial);
    void boatParts(const Boat& b);
    struct Plane { bool on = false; vec3 p{0, 0, 0}, v{0, 0, 0}; float bank = 0, t = 0; } plane_;
    float planeWait_ = 15.f;
    void updatePlane(float dt);
    struct Bird { vec3 off{0, 0, 0}; float phase = 0, glide = 0, size = 1; };
    struct Flock {
        vec3 c{0, 0, 0}, v{0, 0, 0};
        float age = 0, life = 10, scale = 1;
        bool frigate = false, follow = false;
        std::vector<Bird> birds;
    };
    std::vector<Flock> flocks_;
    int flockSide_ = 1;
    void releaseFlock(int n, bool big);
    void updateBirds(float dt);
    struct Lantern { vec3 p; float t; float seed; };
    std::vector<Lantern> lanterns_;
    void lightLanterns(int n);
    struct Puff { vec3 rel; float r, seed, alpha, base, shade, d2; };
    std::vector<Puff> puffs_;
    std::vector<float> cloudShadows_;
    void buildClouds(float worldTime);

    // ---- Per-frame geometry.
    std::vector<Inst> inst_[isl::M_COUNT];
    std::vector<float> glows_;             // x y z (relative), size, r g b, soft
    void glow(const vec3& world, float size, const vec3& col, float soft = 0.3f);
    void push(const isl::Part& p);
    void gatherIslands();
    struct Draw { isl::Island* I; float dist; bool shadow; };
    std::vector<Draw> draws_;

    // ---- The well.
    float seeds_[22][10] = {};
    void syncCells(const Game& g);
    void buildBoard(const Game& g);
    struct GlassInst { float a[4], b[4], c[4]; };
    std::vector<GlassInst> glass_;
    struct ClearCell { int x, y, type; float seed, t, delay; };
    std::vector<ClearCell> clearing_;
    struct Grain { vec3 p, v; float t, life, size; };
    std::vector<Grain> grains_;
    std::vector<float> flat_;
    void quad(const vec3& c, float hw, float hh, const vec3& col, float a, float sx, float sy);
    void line(float x0, float y0, float x1, float y1, float z, float w, const vec3& col, float a);
    vec3 pieceCol(int type) const;
    float overT_ = -1.f;
    bool hardPending_ = false;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontMed_, fontLight_, fontItal_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.7f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();
    const char* hourName() const;

    // ---- GL.
    GLuint progSky_ = 0, progTerrain_ = 0, progObj_ = 0, progWater_ = 0, progCloud_ = 0, progGlow_ = 0, progFlat_ = 0,
           progGlass_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint meshVao_[isl::M_COUNT] = {}, meshVbo_[isl::M_COUNT] = {}, instVbo_[isl::M_COUNT] = {};
    int meshCount_[isl::M_COUNT] = {};
    GLuint idx_[isl::LOD_COUNT] = {};
    int idxCount_[isl::LOD_COUNT] = {};
    GLuint quadVbo_ = 0, glowVao_ = 0, glowVbo_ = 0, cloudVao_ = 0, cloudVbo_ = 0, glassVao_ = 0, glassVbo_ = 0, flatVao_ = 0,
           flatVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    ugl::Target sceneT_, seabedT_, reflT_;
    GLuint halfDepth_ = 0;
    int samples_ = 4, halfW_ = 1, halfH_ = 1;
    static constexpr int SHADOW_SIZE = 4096;
    void drawTerrain(GLuint prog, int mode, bool shadowPass);
    void drawObjects(GLuint prog);
    void setCommon(GLuint p);

    // ---- Song, variant, the hour.
    int variant_ = 0;
    uint64_t seed_ = 1;
    float songLen_ = 200.f, songT_ = 0.f, tod_ = 0.f, todShown_ = 0.f;
    bool todInit_ = false;
    struct Sky { vec3 sunDir, sunCol, top, hor, low, fog; float haze; };
    Sky sky_;
    float lights_ = 0.f, night_ = 0.f;
    vec3 backdrop() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float swell_ = 0.f, kickIn_ = 0.f, sparkle_ = 0.f, gaugeShown_ = 0.f;
    double lastHalf_ = -1.0;
    float time_ = 0.f, worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f, dimT_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{91};
};

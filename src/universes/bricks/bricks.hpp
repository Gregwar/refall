#pragma once
// BRICKS: glossy toy bricks in a photo studio; every cleared row is ejected as bricks that build models turning on
// turntables beside the well (see bricks.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class BricksUniverse : public Universe {
public:
    ~BricksUniverse() override;
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
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

private:
    // ---- Solids: one instance format for every brick, stud, board and turntable (see shaders.hpp).
    struct Inst { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<Inst> boxes_, studs_;
    size_t previewStart_[2] = {0, 0}, previewEnd_[2] = {0, 0}; // the previews' instances (no shadow)
    struct Quat { float x = 0, y = 0, z = 0, w = 1; };
    void pushBox(const vec3& c, const vec3& size, const vec3& col, const Quat& q, int mask, float slope, float stack,
                 float mat, float bevel, float fog = 0.f, float glow = 0.f);
    void pushStud(const vec3& base, float r, float h, const vec3& col, const Quat& q, float stack, float fog = 0.f,
                  float glow = 0.f, float mat = 0.f);

    // ---- The well: piece ids (to merge a piece's cells), the lock's click.
    int ids_[22][10] = {};
    float press_[22][10] = {};   // a locked piece snapping onto the board (z offset, springing to 0)
    float pressV_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    void buildBoard(const Game& g);
    std::vector<float> flat_;    // the ghost's outline (x, y, z, r, g, b, a)

    // ---- The models: authored as voxel paintings, cut into bricks (bottom-up, in a running bond).
    struct Model {
        int id = 0, def = 0, side = 0;
        vec3 pos{0, 0, 0};       // the baseplate's foot, center (world)
        float yaw = 0.f;
        bool onTable = true;     // on its turntable (else in the gallery)
        std::vector<unsigned char> state; // per brick: 0 free, 1 reserved (in flight), 2 placed
        std::vector<float> snap;
        std::vector<int> rank;   // order among the bricks it has (the drop's figures)
        int reserved = 0, placed = 0;
        float readyAt = 0.f;     // when its baseplate is on the turntable (bricks land after it)
        float appear = 0.f;      // the baseplate dropping in
        float doneT = -1.f;      // since it was completed
        float glow = 0.f;
        float sail = 0.f, sailV = 0.f; // a windmill's sails
        // the move to the gallery
        int slot = -1;
        vec3 from{0, 0, 0}, to{0, 0, 0};
        float moveT = 1.f, yawFrom = 0.f, yawTo = 0.f, fog = 0.f, fogTo = 0.f;
        bool dying = false;
    };
    std::vector<Model> models_;
    int nextModelId_ = 1;
    int cur_[2] = {0, 0}, queued_[2] = {0, 0};   // model ids (0 none) per side (0 left, 1 right)
    int order_[2][16] = {}, orderPos_[2] = {0, 0};
    float tableYaw_[2] = {0.f, 0.f}, tableSpin_[2] = {0.f, 0.f}, extraSpin_[2] = {0.f, 0.f};
    float lastArrive_[2] = {0.f, 0.f};
    Model* model(int id);
    Model& newModel(int side, float readyAt);
    int targetModel(int side, float when);
    int pickBrick(Model& m, int colorType, int len);
    vec3 tableCenter(int side) const;
    vec3 brickWorld(const Model& m, int i, Quat* q, bool withMotion) const;
    vec3 brickSizeWorld(int def, int i) const;
    vec3 brickColor(int def, int i) const;
    void startGallery(Model& m);
    void layoutGallery(int side);
    void updateModels(float dt, float wdt);
    void buildModels();
    void liftModels(float amount);
    float liftT_[2] = {9.f, 9.f}, liftA_[2] = {0.f, 0.f}; // the whole model's gentle lift

    // ---- Bricks in flight: ejected from a cleared row, flying to a model; and the stack falling at game over.
    struct Flight {
        int model = 0, brick = 0, side = 0;
        vec3 p0{0, 0, 0}, slideTo{0, 0, 0}, arcFrom{0, 0, 0};
        vec3 size0{1, 1, 1}, col0{1, 1, 1};
        float t = 0.f, slideDur = 0.3f, arcDur = 1.2f, high = 1.f;
        vec3 axis{0, 1, 0};
        float spin = 0.f;
        int phase = 0;           // 0 sliding out of the row, 1 the arc
        float len = 1.f;
        bool park = false;       // waits above the turntable for the next baseplate
        vec3 parkAt{0, 0, 0};
        float leg1 = 1.f;
    };
    int parkCount_[2] = {0, 0};
    std::vector<Flight> flights_;
    struct Debris { vec3 p, v, axis, size, col; float ang, w, t, delay; };
    std::vector<Debris> debris_;
    void updateFlights(float dt, float wdt);

    // ---- The HUD (its lettering: hudLine, the host letters it).
    void drawHud(const std::vector<HudRect>& panels);
    void hudInks(vec3& ink, vec3& halo, vec3& accent) const;
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progStudio_ = 0, progBrick_ = 0, progShadow_ = 0, progFlat_ = 0, progFinal_ = 0, progShape_ = 0;
    GLuint boxVbo_ = 0, studVbo_ = 0, boxVao_ = 0, studVao_ = 0, instVbo_ = 0, studInstVbo_ = 0, flatVao_ = 0,
           flatVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    int studVerts_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0, shadowFbo_ = 0, shadowTex_ = 0, occTex_ = 0;
    ugl::Target sceneT_;
    mat4 vp_, invVp_, lightVp_;
    vec3 eye_{0, 5, 34};
    void drawSolids(const mat4& vp, bool shadow);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 pieceColor(int type) const; // linear
    vec3 backdrop() const;           // what is behind the HUD (linear)

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropRelease_ = 0.f;    // 1 while the drop's choreography runs, down to 0 after it
    double dropBeat0_ = 0.0;
    int dropCycle_ = -1, dropForm_ = 0;
    float dropS_ = 0.f;          // the choreography's spread now 0..1
    float swell_ = 0.f;
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    double lastHalf_ = -1.0, beat_ = 0.0;
    bool hasBeat_ = false;
    float beatClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    bool bare_ = false; // this frame is a break's: the studio and its models alone
    Bridge bridge_;
    Rng rng_{57};
};

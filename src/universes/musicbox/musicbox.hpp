#pragma once
// MUSIC BOX: a wooden cylinder music box under a warm lamp, in 3D; the cleared rows are pinned on its cylinder
// (see musicbox.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class MusicBoxUniverse : public Universe {
public:
    ~MusicBoxUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override { refreshK_ = std::max(refreshK_, strength); }
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

    struct Inst { float c[4], x[4], y[4], z[4], col[4], e[4]; };
    struct Sprite { float p[4], c[4]; };
    struct Line { float a[4], b[4], c[4]; };
    enum Mesh { MESH_BOX = 0, MESH_CYL = 1, MESH_DOME = 2, MESH_COUNT = 3 };
    enum Mat { M_WOOD = 0, M_VELVET = 1, M_BRASS = 2, M_STEEL = 3, M_PIN = 4, M_CELL = 5, M_TABLE = 6, M_WALL = 7, M_EMIT = 8, M_PORCELAIN = 9, M_IRON = 10 };

    // The cylinder's capacity: SLOTS angles (one passes the comb on each beat), TUNES lateral positions.
    static constexpr int SLOTS = 32, TUNES = 4, TRACKS = 10;

private:
    // ---- Drawing helpers (into `out_`).
    std::vector<Inst> stat_[MESH_COUNT], dyn_[MESH_COUNT];
    std::vector<Inst>* out_ = nullptr;
    void put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0 = 0, float p1 = 0,
             float p2 = 0, float seed = 0, const vec4& e = vec4(0.f, 0.f, 0.f, 0.f));
    void box(const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0) {
        put(MESH_BOX, c, vec3(half.x, 0, 0), vec3(0, half.y, 0), vec3(0, 0, half.z), col, mat, p0, p1, p2, seed);
    }
    void obox(const vec3& c, const vec3& u, const vec3& v, const vec3& w, const vec3& half, const vec3& col, int mat, float p0 = 0,
              float p1 = 0, float p2 = 0, float seed = 0, const vec4& e = vec4(0.f, 0.f, 0.f, 0.f)) {
        put(MESH_BOX, c, u * half.x, v * half.y, w * half.z, col, mat, p0, p1, p2, seed, e);
    }
    // a cylinder along `axis` (unit), radius r, half length h
    void cylAx(const vec3& c, const vec3& axis, float r, float h, const vec3& col, int mat, float p0 = 0, float p1 = 0, float p2 = 0,
               float seed = 0, const vec4& e = vec4(0.f, 0.f, 0.f, 0.f));
    void cyl(const vec3& c, float r, float halfH, const vec3& col, int mat, float p0 = 0, float seed = 0) {
        put(MESH_CYL, c, vec3(r, 0, 0), vec3(0, halfH, 0), vec3(0, 0, r), col, mat, p0, 0, 0, seed);
    }
    // a gear: its disc (spokes) and its teeth, in the plane normal to `axis`, turned by `angle`
    void gear(const vec3& c, const vec3& axis, float r, float halfW, int teeth, float angle, int spokes, const vec3& col);

    // ---- The world.
    void buildStatic();
    bool staticDirty_ = true;
    GLuint statVao_[MESH_COUNT] = {}, statVbo_[MESH_COUNT] = {};
    size_t statCount_[MESH_COUNT] = {};
    void buildMoving(float lidOpen);
    vec3 dancerBase() const;

    // ---- The cylinders: every pin a cleared cell.
    struct Pin { int slot = 0, tune = 0, track = 0, type = 0; bool landed = false; float glow = 0.f; float order = 0.f; };
    struct Cylinder {
        std::vector<Pin> pins;
        bool used[TUNES][SLOTS] = {};
        int rows = 0;
        float swapT = -1.f;   // set aside: 0..1 (moving to its rest), 1 at rest
        int rest = -1;        // its place on the table once set aside
        float inT = 1.f;      // a fresh one coming in: 0..1
        double phiAt = 0.0;   // its turn when set aside (slots)
    };
    std::vector<Cylinder> cyls_;
    int fillTune_ = 0, playTune_ = 0;
    float shiftX_ = 0.f;      // the cylinder's lateral shift (the tune under the teeth)
    double phi_ = 0.0;        // the cylinder's turn, in slots (a slot passes the comb when phi crosses it)
    double lastBeat_ = -1.0, clockBeat_ = 0.0;
    float lastBpm_ = 120.f;
    int lastRev_ = 0;
    bool bare_ = false;
    float rate_ = 1.f;        // slots per beat
    float speedK_ = 1.f;      // eases to 0 at game over
    float govAngle_ = 0.f;
    int rowsPinned_ = 0;      // rows on the current cylinder
    // the angle of slot s (radians, 0 at the comb's tips, rising over the top toward the back)
    float slotAngle(int slot) const;
    vec3 cylCenter(int ci) const;
    vec3 cylAxis(int ci, vec3* front) const;
    vec3 pinPos(int ci, const Pin& p, float lift, vec3* radial) const;
    int reserveSlot(float landBeats); // a free slot for a row landing in `landBeats`, in the current cylinder
    void newCylinder();
    void pinRow(const std::vector<GameEvent::CellInfo>& cells, int row, int rowsInClear, bool hold, float extraDelay);
    int hookLines_ = 0;
    bool hookPlaced_ = false;

    // ---- Cleared cells on their way: out of the well (board space), then to their pin.
    struct Flight {
        int type = 0;
        vec3 b0;
        float t = 0.f, delay = 0.f, dur = 1.8f;
        bool world = false;
        int cyl = 0, pin = -1;
        vec3 p0, p1, face0;
        float size0 = 0.3f;
        int kind = 0; // 0 a cleared cell, 1 game over (sinks)
        bool hold = false;
    };
    std::vector<Flight> flights_;
    void launch(Flight& fl);

    // ---- The board.
    void buildBoard(const Game& g);
    std::vector<Inst> cells_, flightInst_;
    std::vector<Line> cellLines_;
    std::vector<Sprite> sprites_;
    void cellInst(std::vector<Inst>& out, const vec3& c, float sc, int type, float glow, float seed, float dim = 1.f);

    // ---- Cameras.
    void updateCamera(float dt);
    mat4 vpB_, vpW_, invVpW_;
    vec3 eyeB_, eyeW_, fwdW_, rightW_, upW_, targetW_;
    float tanW_ = 0.37f, aspect_ = 1.f, shiftY_ = 0.f, camD_ = 90.f;
    vec2 viewC_, viewH_;

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
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.55f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progSolid_ = 0, progShadow_ = 0, progSprite_ = 0, progLine_ = 0, progVeil_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint meshVbo_[MESH_COUNT] = {}, meshIbo_[MESH_COUNT] = {};
    GLsizei meshN_[MESH_COUNT] = {};
    GLuint dynVao_[MESH_COUNT] = {}, dynVbo_[MESH_COUNT] = {};
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0,
           stripVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    void setupInstVao(GLuint vao, GLuint vbo, int mesh);
    void drawInst(GLuint vao, GLuint vbo, int mesh, const std::vector<Inst>* data, size_t count);

    // ---- Song, variant.
    int variant_ = 0;
    struct Palette {
        vec3 piece[7];
        float enamel[7];
        vec3 wood, woodLid, inlayA, inlayB, velvet, brass, steel, bed, table, wall, lamp, amb, ambLow, fill, veil, ink, accent, porcelain;
        float lampK = 1.f, exposure = 1.f, figure = 0.f, night = 0.f;
    };
    Palette pal_;
    bool colSet_ = false;
    uint64_t seed_ = 1;

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f;     // the drop's swell (eases in, settles over seconds)
    float dancer_ = 0.f;      // the dancer: 0 down (the trap closed) .. 1 up
    float dancerHold_ = 0.f;  // seconds she stays up after the drop ends
    float dancerTurn_ = 0.f;
    float kickIn_ = 0.f, kickK_ = 0.f, refreshK_ = 0.f, quadT_ = 99.f, levelT_ = 99.f, windAngle_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float dimT_ = 0.f;
    std::vector<float> toothLift_;   // per tooth (shown)
    std::vector<float> toothRing_;   // per tooth: seconds since its release
    Bridge bridge_;
    Rng rng_{0x3B0C};
};

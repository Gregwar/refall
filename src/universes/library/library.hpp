#pragma once
// LIBRARY: the blocks are small stacks of books; every cleared row's stacks are shelved, grown to full size, on the
// shelving walls of a contemporary library atrium that fills floor by floor, in 3D (see library.cpp).
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class LibraryUniverse : public Universe {
public:
    ~LibraryUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override { refreshK_ = std::max(refreshK_, strength); }
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

    // An instance: center + material, three half axes (+ material params), color + seed, (born, die, level, style).
    // Public for the world builder (world.cpp).
    struct Inst { float c[4], x[4], y[4], z[4], col[4], e[4]; };
    struct Sprite { float p[4], c[4]; };
    struct Line { float a[4], b[4], c[4]; };
    enum Mesh { MESH_BOX = 0, MESH_CYL = 1, MESH_DOME = 2, MESH_COUNT = 3 };
    enum Mat {
        M_PLASTER = 0, M_WOOD = 1, M_CASE = 2, M_BOOK = 3, M_FLOOR = 4, M_CONCRETE = 5, M_STEEL = 6, M_EMIT = 7, M_WINDOW = 8,
        M_GLASS = 9, M_SKY = 10, M_FELT = 11
    };
    struct Lamp {
        vec3 p, col;
        float r = 0.1f, k = 1.f;     // its glow's radius (0: no glow), intensity
        float range = 0.f;           // > 0: a light for the shading
        int level = 0;               // the floor it lights (on once that floor fills)
    };
    // A book of a stack, in a cell's units (the cell spans -0.5..0.5, its front at z = 0).
    struct BookDef {
        float cx, cy;     // center
        float hx, hy, hd; // half sizes: across, up, depth
        float z;          // its spine's z (a hair in or out)
        bool lying;
        float tone;       // its shade of the piece's color
        int style;        // the spine's design
        float seed;
    };
    struct Stack { int n = 0; BookDef b[5]; float top = 0.f; };

private:
    // ---- The world (world.cpp).
    void buildWorld();
    void put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0 = 0, float p1 = 0,
             float p2 = 0, float seed = 0);
    void box(const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0) {
        put(MESH_BOX, c, vec3(half.x, 0, 0), vec3(0, half.y, 0), vec3(0, 0, half.z), col, mat, p0, p1, p2, seed);
    }
    void cyl(const vec3& c, float r, float halfH, const vec3& col, int mat, float p0 = 0, float seed = 0) {
        put(MESH_CYL, c, vec3(r, 0, 0), vec3(0, halfH, 0), vec3(0, 0, r), col, mat, p0, 0, 0, seed);
    }
    void lamp(const vec3& p, const vec3& col, float r, float k, float range, int level);
    std::vector<Inst> stat_[MESH_COUNT], glass_;
    std::vector<Inst>* putTo_ = nullptr; // null: the static lists
    float stLevel_ = 0.f;                // the floor the builder is placing (for its lights)
    std::vector<Lamp> lamps_;
    bool worldDirty_ = true;
    GLuint statVao_[MESH_COUNT] = {}, statVbo_[MESH_COUNT] = {};
    size_t statCount_[MESH_COUNT] = {};
    GLuint glassVao_ = 0, glassVbo_ = 0;

    // ---- The stacks: one per cell, drawn from a seed.
    static Stack makeStack(uint32_t seed);
    uint32_t seedGrid_[22][10] = {};
    int8_t typeGrid_[22][10];
    uint32_t spawnN_ = 1;
    uint32_t cellSeed(int x, int y, int type);
    void pushStack(std::vector<Inst>& out, const Stack& s, int type, const vec3& origin, const vec3& along, const vec3& up, const vec3& out_,
                   float k, float grow, float glow, float fade);

    // ---- The collection: every cleared cell's stack, shelved in the order they came.
    struct Group {
        int type = 0;
        uint32_t seed = 0;
        int slot = 0;
        float t0 = 0.f;     // world time it starts to grow on its shelf (-1: there already)
    };
    std::vector<Group> groups_;
    int nextSlot_ = 0;
    // the settled stacks' instances, kept in their own buffer (uploaded only when a stack settles)
    std::vector<Inst> shelfInst_;
    size_t settled_ = 0;
    bool shelfDirty_ = true;
    GLuint shelfVao_ = 0, shelfVbo_ = 0;
    void shelve(int type, uint32_t seed, float delay);
    void slotFrame(int slot, vec3& origin, vec3& along, vec3& out) const;
    int hookLines_ = 0;
    bool hookPlaced_ = false;
    // a cleared cell, swelling into light in the well
    struct Fx { int type; uint32_t seed; vec3 p; float t; };
    std::vector<Fx> fx_;
    struct Tumble { int type; uint32_t seed; vec3 p, v; float t, delay, rotV; };
    std::vector<Tumble> tumbles_;
    float levelOn_[8] = {}; // each floor's lights (on once it starts to fill)

    // ---- Loose motes (dust in the daylight).
    struct Mote { vec3 p; float seed; };
    std::vector<Mote> motes_;

    // ---- The board: the stacks in the well.
    void buildBoard(const Game& g);
    vec3 pieceColor(int type) const;
    std::vector<Inst> dyn_[MESH_COUNT], cells_, fxInst_;
    std::vector<Line> cellLines_;
    std::vector<Sprite> sprites_;

    // ---- Camera (world) and the board's camera.
    void updateCamera(float dt);
    mat4 vpB_, vpW_;            // the board's (absolute), the world's (relative to eyeW_)
    vec3 eyeB_, eyeW_, fwdW_, rightW_, upW_, targetW_;
    float tanW_ = 0.47f, aspect_ = 1.f, camD_ = 4.f;
    vec3 camT_{0.f}, camTv_{0.f};
    float camDs_ = -1.f, camDv_ = 0.f;
    vec2 viewC_, viewH_;        // the board plane's view (for the veil)
    bool bare_ = false; // a break's frame: the room alone (no well, no gauge)

    // ---- Growth.
    int lines_ = 0, linesSeen_ = -1;
    float grow_ = 0.f;
    void resetGrowth();

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font reg_, semi_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.55f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progSolid_ = 0, progShadow_ = 0, progSprite_ = 0, progLine_ = 0, progVeil_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint meshVbo_[MESH_COUNT] = {}, meshIbo_[MESH_COUNT] = {};
    GLsizei meshN_[MESH_COUNT] = {};
    GLuint dynVao_[MESH_COUNT] = {}, dynVbo_[MESH_COUNT] = {};
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    void setupInstVao(GLuint vao, GLuint vbo, int mesh);
    void drawInst(GLuint vao, GLuint vbo, int mesh, const std::vector<Inst>* data, size_t count);

    // ---- Song, variant.
    int variant_ = 0;
    struct Palette {
        vec3 piece[7];
        vec3 oak, wall, slab, floor, steel, felt, paper, skyA, skyB, outside, ambSky, ambGround, key, led;
        vec3 veil, ink, accent, halo;
        float night = 0, keyK = 1, ledK = 1, exposure = 1, floorKind = 0;
        bool light = true;
    };
    Palette pal_;
    bool colSet_ = false;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f;
    float kickIn_ = 0.f, kickK_ = 0.f, refreshK_ = 0.f, tetrisT_ = 99.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beatPulse_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x11B7};
};

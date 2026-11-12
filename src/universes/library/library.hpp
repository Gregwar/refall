#pragma once
// LIBRARY: the blocks are books; every cleared line's two books fly, behind the board, to a cosy toon library room
// seen as a diorama, which fills slowly (see library.cpp).
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
    static constexpr int F_COUNT_ = 4; // the features that grow in (world.hpp's Feature)
    enum Mat {
        M_PLASTER = 0, M_WOOD = 1, M_CASE = 2, M_BOOK = 3, M_FLOOR = 4, M_CONCRETE = 5, M_STEEL = 6, M_EMIT = 7, M_WINDOW = 8,
        M_GLASS = 9, M_SKY = 10, M_TOY = 11, M_CELL = 12, M_BACKDROP = 13, M_ROUNDWIN = 14
    };
    struct Lamp {
        vec3 p, col;
        float r = 0.1f, k = 1.f;     // its glow's radius (0: no glow), intensity
        float range = 0.f;           // > 0: a light for the shading
        int level = 0;               // 0: always; 1: the mezzanine's, 2: the low bookcase's (on once it is there)
    };

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
    // a round toy shape (two hemispheres); glow > 0: a globe lit from within
    void sphere(const vec3& c, float r, const vec3& col, float glow) {
        put(MESH_DOME, c, vec3(r, 0, 0), vec3(0, r, 0), vec3(0, 0, r), col, M_TOY, glow);
        put(MESH_DOME, c, vec3(r, 0, 0), vec3(0, -r, 0), vec3(0, 0, r), col, M_TOY, glow);
    }
    void lamp(const vec3& p, const vec3& col, float r, float k, float range, int level);
    std::vector<Inst> stat_[MESH_COUNT], glass_;
    std::vector<Inst>* putTo_ = nullptr; // null: the static lists
    float stLevel_ = 0.f;                // the level the builder is placing (its lights; -1: no shelf wave)
    float stFeat_ = -1.f, stPivot_ = 0.f; // the feature the builder is placing (-1: none) and the height it grows from
    std::vector<Lamp> lamps_;
    bool worldDirty_ = true;
    GLuint statVao_[MESH_COUNT] = {}, statVbo_[MESH_COUNT] = {};
    size_t statCount_[MESH_COUNT] = {};
    GLuint glassVao_ = 0, glassVbo_ = 0;

    // ---- The board's cells' seeds (a book's look follows its cell).
    uint32_t seedGrid_[22][10] = {};
    int8_t typeGrid_[22][10];
    uint32_t spawnN_ = 1;
    uint32_t cellSeed(int x, int y, int type);
    // a cell of the board: one clean block, a bound volume (spine toward us)
    void pushCell(std::vector<Inst>& out, int type, const vec3& c, const vec3& along, const vec3& up, float k, float fade, float seed);

    // ---- The collection: three big books for every cleared line, placed in the order they came along runs (a shelf,
    // or a row of piles on the floor), never moved again.
    struct Run {
        vec3 start, along, out; // the front edge at the run's start, at the shelf's top; along the run; into the room
        float len = 0.f;
        int kind = 0;           // 0: a shelf (books standing), 1: a row of piles (books lying)
        int level = 0;          // 0 / 1: the mezzanine (its lights, the wave)
        int feat = -1;          // the feature it needs (grown in when the run is first used)
        float maxH = 0.9f;      // the tallest book it takes (a shelf's clearance)
    };
    std::vector<Run> runs_;
    void buildRuns();
    int runIdx_ = 0, pileN_ = 0;
    float runOff_ = 0.f, pileH_ = 0.f;
    struct Group {
        int type = 0;
        uint32_t seed = 0;
        float t0 = 0.f;      // world time it starts to settle on its shelf (1e9: in flight)
        vec3 c, along, out;  // its center and frame, full size
        float hx = 0.1f, hy = 0.3f, hd = 0.2f; // half sizes: along, up, depth (a lying book: its length along)
        bool lying = false;
        int level = 0, style = 0;
        float tone = 1.f;
        float landT = -100.f; // world time it landed (its pulse of light)
    };
    std::vector<Group> groups_;
    bool place(Group& g); // its place: the next free one (false: the room is full)
    // the settled books' instances, kept in their own buffer (uploaded only when a book settles)
    std::vector<Inst> shelfInst_;
    size_t settled_ = 0;
    bool shelfDirty_ = true;
    GLuint shelfVao_ = 0, shelfVbo_ = 0;
    bool shelve(int type, uint32_t seed);
    void pushBook(std::vector<Inst>& out, const Group& g, const vec3& c, const vec3& along, const vec3& outD, float k, float grow,
                  float glow, int index, float vivid = 0.f);
    // features: unlocked when their first run is used, then grown in
    bool featUnlocked_[F_COUNT_] = {};
    float featGrow_[F_COUNT_] = {};
    // a cleared line's books flying to their place, behind the well: launched from the cell's place on screen at a
    // depth behind the board, only ever going deeper
    struct Flight {
        int group = -1;
        vec3 board;            // the cell (board space)
        float t = 0.f, dur = 1.f;
        bool launched = false;
        vec3 p0, out0;
        float k0 = 0.f;
        float spin = 0.f;
        vec3 c1, c2;           // its arc's control points (with p0 and its place)
    };
    std::vector<Flight> flights_;
    void fly(int type, uint32_t seed, const vec3& board, float delay);
    int hookLines_ = 0;
    float bookAcc_ = 0.f;   // the books the lines have earned so far (a fraction: the song's pace)
    int booksFor();         // how many books the next cleared line sends (BOOKS_LINE x the pace, about)
    bool hookPlaced_ = false;
    // a cleared cell, swelling into light in the well
    struct Fx { int type; uint32_t seed; vec3 p; float t; };
    std::vector<Fx> fx_;
    struct Tumble { int type; uint32_t seed; vec3 p, v; float t, delay, rotV; };
    std::vector<Tumble> tumbles_;
    float levelOn_[8] = {}; // the lights: 0 the room's, 1 the mezzanine's, 2 the low bookcase's

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
        vec3 oak, bay, wall, wall2, cut, floor, floorB, steel, paper, skyA, skyB, ambSky, ambGround, key, led;
        vec3 chair, rug, rug2, leaf, pot, veil, veilTop, card, cardEdge, line, rim, cloud, cloudShade;
        vec3 ink, accent, halo;
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
    float kickIn_ = 0.f, kickK_ = 0.f, refreshK_ = 0.f, quadT_ = 99.f;
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

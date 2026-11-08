#pragma once
// LIBRARY: the blocks are books; cleared rows are put away, sorted, on the shelves of a growing personal library,
// in 3D (see library.cpp).
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

    // An instance: center + material, three half axes (+ material params), color + seed, (born, die, sort unit,
    // style). Public for the world builder (world.cpp).
    struct Inst { float c[4], x[4], y[4], z[4], col[4], e[4]; };
    struct Sprite { float p[4], c[4]; };
    struct Line { float a[4], b[4], c[4]; };
    enum Mesh { MESH_BOX = 0, MESH_CYL = 1, MESH_DOME = 2, MESH_COUNT = 3 };
    enum Mat {
        M_PLASTER = 0, M_WOOD = 1, M_SHELF = 2, M_BOOK = 3, M_FLOOR = 4, M_MARBLE = 5, M_BRASS = 6, M_EMIT = 7, M_WINDOW = 8,
        M_RUG = 9, M_LEATHER = 10, M_SHADE = 11, M_DOME = 12, M_CEIL = 13, M_BOOKWALL = 14, M_PAPER = 15, M_BALUSTRADE = 16,
        M_FIRE = 17, M_GLOBE = 18, M_IRON = 19, M_GHOST = 20
    };
    // What the builder is placing: when it appears and goes, how, and which sort unit it belongs to.
    struct Stamp { float born = 0.f, die = -1.f, unit = -1.f, style = 0.f; };
    struct Lamp {
        vec3 p, col;
        float r = 0.1f, k = 1.f;     // its glow's radius, intensity
        float range = 0.f;           // > 0: a light for the shading
        float born = 0.f, die = -1.f;
        float flick = 0.f;           // candle flicker
    };
    // A shelf of the collection (where the cleared rows' books are put away, sorted).
    struct Shelf {
        vec3 o;        // its left end, on the board, at the front
        vec3 along;    // left to right
        vec3 out;      // outward (toward the room)
        float len = 0.9f, clear = 0.4f;
        int bay = 0;
    };
    struct Bay {
        vec3 c;           // the bookcase's foot, at the middle of its front
        vec3 along, out;
        float born = -1.f; // lines when it appeared (-1: not yet)
        int filler = -1;   // the filler bookcase it replaces (its instances die), -1 none
        int firstShelf = 0;
    };
    // A sort unit: a filler shelf (or wall of shelves) a clear sorts into a rainbow.
    struct Unit { vec3 c; float born = 0.f; float order = 0.f; bool sorted = false; };

private:
    // ---- The world (world.cpp).
    void buildWorld();
    void buildStudy();
    void buildHall();
    void buildBookcase(const vec3& foot, const vec3& along, const vec3& out, float w, float h, int shelves, const vec3& wood, bool filler,
                       int fillerIdx, float seed);
    void put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0 = 0, float p1 = 0,
             float p2 = 0, float seed = 0);
    void box(const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0) {
        put(MESH_BOX, c, vec3(half.x, 0, 0), vec3(0, half.y, 0), vec3(0, 0, half.z), col, mat, p0, p1, p2, seed);
    }
    // an oriented box: its center, its axes (unit, orthogonal) and half sizes
    void obox(const vec3& c, const vec3& u, const vec3& v, const vec3& w, const vec3& half, const vec3& col, int mat, float p0 = 0,
              float p1 = 0, float p2 = 0, float seed = 0) {
        put(MESH_BOX, c, u * half.x, v * half.y, w * half.z, col, mat, p0, p1, p2, seed);
    }
    void cyl(const vec3& c, float r, float halfH, const vec3& col, int mat, float p0 = 0, float seed = 0) {
        put(MESH_CYL, c, vec3(r, 0, 0), vec3(0, halfH, 0), vec3(0, 0, r), col, mat, p0, 0, 0, seed);
    }
    void sphere(const vec3& c, float r, const vec3& col, int mat, float p0 = 0, float seed = 0) {
        put(MESH_DOME, c, vec3(r, 0, 0), vec3(0, r, 0), vec3(0, 0, r), col, mat, p0, 0, 0, seed);
        put(MESH_DOME, c, vec3(r, 0, 0), vec3(0, -r, 0), vec3(0, 0, r), col, mat, p0, 0, 0, seed);
    }
    void lamp(const vec3& p, const vec3& col, float r, float k, float range, float flick = 0.f);
    int unit(const vec3& c);
    std::vector<Inst> stat_[MESH_COUNT];
    std::vector<Inst>* putTo_ = nullptr; // null: the static lists
    Stamp st_;
    std::vector<Lamp> lamps_;
    std::vector<Unit> units_;
    std::vector<Bay> bays_;
    std::vector<Shelf> shelves_;
    std::vector<vec3> fillerFeet_;     // the filler bookcases on the collection's path (their instances' unit tag)
    bool worldDirty_ = true;
    GLuint statVao_[MESH_COUNT] = {}, statVbo_[MESH_COUNT] = {};
    size_t statCount_[MESH_COUNT] = {};

    // ---- The collection: every book put away, in its sorted order.
    struct Book {
        int id = 0, type = 0, style = 0;
        float w = 0.04f, h = 0.25f, d = 0.18f;
        float key = 0.f;           // the sort key (rainbow rank, then height)
        int shelf = -1;
        float x = 0.f, xT = 0.f;   // along the shelf (shown, target)
        int shelfShown = -1;
        float hop = 1.f;           // a hop to another shelf (0..1)
        int hopFrom = -1;
        float hopX = 0.f;
        bool flying = false;
        float glow = 0.f;
        bool gilt = false;
    };
    std::vector<Book> books_;
    int nextBookId_ = 1;
    void layoutBooks();
    const Book& addBook(int type, bool flying);
    int hookLines_ = 0;
    int ensureBays(int needShelves);
    vec3 bookPos(const Book& b, int shelf, float x) const;
    void newBay();

    // ---- Cleared books on their way: out of the well (board space), then across the library into their gap.
    struct Flight {
        int book = -1;              // the collection's book id (-1: none, it merges into a shelf wall)
        int type = 0, style = 0;
        vec3 b0;                    // the cell in the well (board space)
        float t = 0.f, delay = 0.f, dur = 2.f;
        bool world = false;         // in the library's space
        vec3 p0, p1, p2, p3;        // the flight's curve (world)
        vec3 dims0, dims1;          // half sizes, start (a cell seen from here) -> the book's
        vec3 out1;                  // the book's outward direction on its shelf
        vec3 face0;                 // toward the camera at the start
        float spin = 0.f;
        int kind = 0;               // 0 a cleared row's book, 1 game over (falls out of the well)
        vec3 vel;
        float rot = 0.f, rotV = 0.f;
        bool gilt = false;
        int unitTo = -1;
    };
    std::vector<Flight> flights_;
    void launch(Flight& fl);

    // ---- Loose pages (peaks, the drop).
    struct Page { vec3 p, v; float a = 0, av = 0, flap = 0, life = 0, maxLife = 6, size = 0.1f; float seed = 0; };
    struct Mote { vec3 p; float seed; };
    std::vector<Mote> motes_;

    // ---- The board: the books in the well.
    void buildBoard(const Game& g);
    vec3 pieceColor(int type) const;
    int pieceStyle(int type) const;
    std::vector<Inst> dyn_[MESH_COUNT], cells_, flightInst_;
    std::vector<Line> cellLines_;
    std::vector<Sprite> sprites_;

    // ---- Camera (world) and the board's camera.
    void updateCamera(float dt);
    mat4 vpB_, vpW_, invVpW_;   // the board's (absolute), the world's (relative to eyeW_)
    vec3 eyeB_, eyeW_, fwdW_, rightW_, upW_, targetW_;
    float tanW_ = 0.47f, aspect_ = 1.f, camD_ = 4.f;
    float camG_ = -1.f, camGv_ = 0.f;
    vec2 viewC_, viewH_;        // the board plane's view (for the veil)
    float fB_ = 1.f;             // the board camera's focal length (pixels)
    vec4 cut_{0, 0, 0, 1e9f};

    // ---- Growth.
    int lines_ = 0, linesSeen_ = -1;
    float grow_ = 0.f, growV_ = 0.f;
    int sortedCount_ = 0;
    std::vector<float> sortTimes_;
    GLuint sortTex_ = 0;
    bool sortDirty_ = true;
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
    Font caps_, ital_;
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
        vec3 piece[7], rain[7], fill[5];
        vec3 wood, woodDark, plaster, paper, gilt, floor, rug, leather, shade, skyA, skyB, ambSky, ambGround, key, lampCol, candle;
        vec3 fogLow, fogHigh, veil, ink, accent, section, marble;
        float night = 0, moon = 0, keyK = 1, lampK = 1, flicker = 0, exposure = 1, lift = 0;
    };
    Palette pal_, palT_;
    bool colSet_ = false;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, waveT_ = 99.f;
    vec3 waveO_;
    float kickIn_ = 0.f, kickK_ = 0.f, refreshK_ = 0.f, tetrisT_ = 99.f;
    double time_ = 0.0;
    float worldTime_ = 0.f, beatPulse_ = 0.f;
    float pageAcc_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x11B7};
};

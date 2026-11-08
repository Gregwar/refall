#pragma once
// GROCERIES: a supermarket checkout in 3D; the pieces are groceries, cleared rows are scanned and packed into bags,
// and the shopping grows over the song into an absurd hypermarket (see groceries.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class GroceriesUniverse : public Universe {
public:
    ~GroceriesUniverse() override;
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

    // Instances.
    struct Box { float c[4], x[4], y[4], z[4], col[4], e[4]; }; // center + material, half axes (+ params), color + seed, birth
    struct Sprite { float p[4], c[4]; };                        // a glow: position + radius, color + intensity
    struct Line { float a[4], b[4], c[4]; };                    // a hairline: from (+ width px) to (+ alpha), color
    struct Blob { float p[4], r[4]; };                          // a contact shadow: center + alpha, half sizes + softness + birth
    struct Flake { float p[4], c[4], r[4]; };                   // a confetti flake

    // Materials (mirrored in BOX_FS).
    enum Mat { M_PAINT = 0, M_STEEL = 1, M_FLOOR = 2, M_BELT = 3, M_EMIT = 4, M_SHELF = 5, M_PRODUCT = 6, M_BAG = 7, M_WIRE = 8,
               M_GLASS = 9, M_SIGN = 10, M_ASPHALT = 11, M_WALL = 12, M_VAN = 13, M_TILL = 14, M_WOOD = 15, M_GRASS = 16,
               M_CEIL = 17, M_CRATE = 18, M_CARTS = 19, M_SCANNER = 20 };
    // Light groups (their intensities eased per frame): the lanes' number lights 0..7, then:
    enum Group { G_SCAN = 8, G_TUBES = 9, G_LAMPS = 10, G_FRIDGE = 11, G_FLOOR = 12, G_TILL = 13, G_SIGN = 14, G_WIN = 15 };

    // A variant's colors (linear RGB) and light.
    struct Palette {
        vec3 zen, hor, ground, sunGlow, fog;
        vec3 inTop, inBot, inKey, outTop, outBot, outKey;
        vec3 floor, wall, wallOut, ceil, counter, steel, belt, gondola, shelfEdge, kraft, accent, accent2, tube, lamp, win,
            fridge, grass, asphalt, veil, cart;
        vec3 famA[7], famB[7], famC[7];
        vec3 goods[8];
        vec3 fabric[4];
        vec3 confetti[5];
        float sunAz, sunEl, night, gloss, stars, clouds, wood;
    };

private:
    // ---- Geometry helpers.
    void box(std::vector<Box>& out, const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0,
             float p2 = 0, float seed = 0, float birth = 0, int group = -1, float span = 0.8f, int channel = 0);
    void obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
              float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0, float birth = 0, int group = -1, float span = 0.8f,
              int channel = 0);
    void blob(std::vector<Blob>& out, const vec3& c, float hx, float hz, float a, float soft = 0.5f, float birth = 0);
    void line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha);
    void sprite(std::vector<Sprite>& out, const vec3& p, float r, const vec3& col, float k);

    // ---- The store (store.cpp).
    void buildWorld();
    void buildRoof();
    void buildLane(int k, bool open, float openL);
    void buildCart(std::vector<Box>& out, std::vector<Blob>* blobs, const vec3& p, float yaw, int bags, float seed, float birth = 0,
                   int channel = 0, bool lod = false);
    void buildBag(std::vector<Box>& out, const vec3& base, float yaw, float scale, int style, const vec3& fabric, float fill,
                  float seed, float birth = 0, int channel = 0, bool handles = true, float tilt = 0.f);
    void buildPile(const vec3& c, int n, float seed, float birth0, float perLine, int channel = 0);
    void buildMountain(const vec3& c, float height, float seed, float birth0, float perLine);
    void buildCartCity(float L);
    void buildModule(float x0, float birth, bool simple);
    void buildCarPark(float halfW);
    void buildTown();
    vec3 queueSlot(int i) const;
    int fabricStyle(float h, vec3& fabric) const;

    // ---- The world's state, from the lines (ZEN_GROCERIES_LINES / ZEN_GROW_RATE add to them).
    int lines_ = 0, linesShown_ = -1;
    float gL_ = 0.f, gLVel_ = 0.f;   // the growth shown (lines, eased: things pop in as it passes their birth)
    float zoom_ = 0.f, zoomVel_ = 0.f;
    int pending_ = 0;                // bags not yet in the cart (their products in flight, the bag on the stand)
    float bagsShown_ = 0.f;
    int standFill_ = 0;              // products in the bag on the stand
    int standStyle_ = 0;
    float standSeed_ = 0.f, standPop_ = 1.f;
    struct Lift { float t = 0; int style = 0; vec3 fabric; float seed = 0; vec3 from, to; };
    std::vector<Lift> lifts_;
    int cartBags_ = 0;               // bags in our lane's cart
    int cartsArrived_ = 0;           // full carts parked in the queue
    struct Roll { float t = 0; int slot = 0; float seed = 0; };
    std::vector<Roll> rolls_;
    float cartIn_ = 1.f;             // a new empty cart sliding in
    int builtL_ = -1, builtCarts_ = -1, builtVariant_ = -1;
    bool worldDirty_ = true;

    // ---- Our belt: products riding in toward the well, a divider between shoppers.
    struct Rider { float x = 0; int fam = 0; int kind = 0; float seed = 0; float rot = 0; };
    std::vector<Rider> riders_;
    float riderGap_ = 0.f;
    struct Hop { vec3 p0; float t = 0; int fam = 0; float seed = 0; };
    std::vector<Hop> hops_;          // a product taken off the belt as a piece spawns

    // ---- Cleared products: scanned in the row, then tossed into the bags.
    struct Flight {
        vec3 p0, cur, ctl;
        int type = 0, kind = 0;      // kind 0 a cleared product, 1 game over (falls), 2 a landing sparkle
        float t = 0, delay = 0, dur = 1.f, seed = 0, scale = 1.f, spin = 0.f;
        vec3 vel;
        bool landed = false;
    };
    std::vector<Flight> flights_;
    float scanY_ = -9.f, scanFrom_ = -9.f, scanTo_ = -9.f, scanT_ = 9.f, scanH_ = 0.5f;
    float beamX_ = 6.f;
    vec3 sinkW_{0.f};                // where the bags are filled (world)
    vec3 sinkBoard(float* scale) const;
    vec3 chooseSink();

    // ---- The rest of the store's life.
    float laneOn_[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    float laneBeep_[8] = {};
    struct Van { int bay = 0; int state = 0; float t = 0, fill = 0, seed = 0; bool lorry = false; };
    std::vector<Van> vans_;
    void updateVans(float dt, float wdt);
    void buildVans();
    struct FlakeP { vec2 p, v; float z, size, ang, angV, flip, flipV, age, life; vec3 col; };
    std::vector<FlakeP> flakes_;
    std::vector<Flake> flakeInst_;

    // ---- The products in the well.
    float seeds_[22][10] = {};
    void buildBoard(const Game& g);
    void buildFrame();
    void product(std::vector<Box>& out, const vec3& c, float half, int type, float seed, float flash = 0.f, float rot = 0.f);

    // ---- Instances.
    std::vector<Box> static_, roof_, boxes_, board_, cells_;
    int lastExtra_ = -1;
    std::vector<Blob> staticBlobs_, blobs_;
    std::vector<Sprite> sprites_, boardSprites_;
    std::vector<Line> worldLines_, boardLines_;
    size_t staticCount_ = 0, roofCount_ = 0, staticBlobCount_ = 0;
    struct Lamp { vec3 p, col; float r, k; int group; };
    std::vector<Lamp> lamps_;
    std::vector<vec4> pointP_, pointC_;

    // ---- Cameras: the board's (the layout's, raised to EYE_Y with a lens shift) and the world's (it pulls back).
    mat4 vpB_, vpW_, invVpW_;
    vec3 eyeB_, eyeW_, camR_{1, 0, 0}, camU_{0, 1, 0}, camF_{0, 0, -1};
    float focal_ = 1.f, shift0_ = 0.f, aspect_ = 1.78f;
    void updateCamera();
    vec2 viewC_{0, 0}, viewH_{21, 12};
    float pxPerUnit_ = 20.f;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font bold_, medium_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.5f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progBox_ = 0, progBlob_ = 0, progSky_ = 0, progSprite_ = 0, progLine_ = 0, progFlake_ = 0, progVeil_ = 0, progDown_ = 0,
           progBlur_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint cubeVbo_ = 0, quadVbo_ = 0, stripVbo_ = 0;
    GLuint staticVao_ = 0, staticVbo_ = 0, roofVao_ = 0, roofVbo_ = 0, boxVao_ = 0, boxVbo_ = 0, blobVao_ = 0, blobVbo_ = 0,
           sblobVao_ = 0, sblobVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, flakeVao_ = 0, flakeVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    void setupBoxVao(GLuint& vao, GLuint& vbo);
    void setupBlobVao(GLuint& vao, GLuint& vbo);

    // ---- Song, variant.
    int variant_ = 0;
    Palette pal_{}, palT_{};
    bool palSet_ = false;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false, wasDrop_ = false;
    float dropAmt_ = 0.f, dropT_ = 99.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickGlow_ = 0.f, refreshK_ = 0.f;
    float tetrisT_ = 99.f;
    double lastBeat_ = -1.0;
    float beatFlash_ = 0.f;
    float worldTime_ = 0.f, beltPhase_ = 0.f, beltK_ = 1.f;
    double time_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x6E0C};
};

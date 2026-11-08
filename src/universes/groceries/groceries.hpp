#pragma once
// GROCERIES: a cashier's checkout counter seen from above; the pieces are groceries on the belt, cleared rows are
// scanned and packed into bags, calmly (see groceries.cpp).
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
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

    // Instances.
    struct Box { float c[4], x[4], y[4], z[4], col[4], e[4]; }; // center + material, half axes (+ params), color + seed, birth
    struct Sprite { float p[4], c[4]; };                        // a glow: position + radius, color + intensity
    struct Line { float a[4], b[4], c[4]; };                    // a hairline: from (+ width px) to (+ alpha), color
    struct Blob { float p[4], r[4]; };                          // a contact shadow: center + alpha, half sizes + softness + birth

    // Materials (mirrored in BOX_FS).
    enum Mat { M_PAINT = 0, M_STEEL = 1, M_COUNTER = 2, M_BELT = 3, M_EMIT = 4, M_PRODUCT = 6, M_BAG = 7, M_WIRE = 8, M_SIGN = 10,
               M_TILL = 14, M_WOOD = 15, M_SCANNER = 20, M_PAPER = 21, M_DISC = 22 };
    // Light groups (their intensities eased per frame).
    enum Group { G_SCAN = 8, G_RAIL = 9, G_TILL = 13, G_LED = 14 };

    // A variant's colors (linear RGB) and light.
    struct Palette {
        vec3 light, amb, ambLow, counter, counter2, steel, belt, kraft, accent, accent2, paper, veil, cart;
        vec3 famA[7], famB[7], famC[7];
        vec3 fabric[4];
        float patch, wood, warmth, gloss;
    };

private:
    // ---- Geometry helpers.
    void box(std::vector<Box>& out, const vec3& c, const vec3& half, const vec3& col, int mat, float p0 = 0, float p1 = 0,
             float p2 = 0, float seed = 0, int group = -1);
    void obox(std::vector<Box>& out, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat,
              float p0 = 0, float p1 = 0, float p2 = 0, float seed = 0, int group = -1);
    void blob(std::vector<Blob>& out, const vec3& c, float hx, float hy, float a, float soft = 0.5f);
    void line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha);
    void sprite(std::vector<Sprite>& out, const vec3& p, float r, const vec3& col, float k);

    // ---- The counter (counter.cpp).
    void buildCounter();                                    // the static counter: belt, chute, scanner, till, printer, racks
    void buildBag(std::vector<Box>& out, const vec3& c, float scale, int style, const vec3& fabric, int filled, float seed,
                  float yaw = 0.f);                         // a bag from above, hollow, `filled` products stacked inside
    void buildReceipt(float length, float t);
    void buildThings(float L);                              // the customer's things on the counter
    int fabricStyle(float h, vec3& fabric) const;
    vec3 slotPos(int i) const;                              // a product's place in the bag being packed (relative)
    vec3 shelfPos(int i) const;                             // a full bag's place at the counter's edge

    // ---- The lines (ZEN_GROCERIES_LINES / ZEN_GROW_RATE add to them).
    int lines_ = 0, linesShown_ = -1, lastExtra_ = -1;
    float gL_ = 0.f;                 // the lines shown (eased), for the things that appear
    float receipt_ = 0.f, receiptT_ = 0.f, burst_ = 0.f;
    int pending_ = 0;                // bags whose products are still on their way
    int bagFill_ = 0;                // products in the bag being packed
    int bagStyle_ = 0;
    float bagSeed_ = 0.f, bagPop_ = 1.f;
    std::vector<int> bagTypes_;      // the families packed so far (their order)
    const std::vector<int>* bagTypesFor_ = nullptr; // the goods of the bag being built
    struct Full { int style = 0; float seed = 0; std::vector<int> types; float t = 1.f; vec3 from; int slot = 0; };
    std::vector<Full> full_;         // the full bags set aside
    float sweepT_ = -1.f;            // the counter's edge full: the bags are taken away
    int builtVariant_ = -1;
    bool counterDirty_ = true;

    // ---- Cleared products on their way: lifted off the belt, along the chute, across the scanner, into the bag.
    struct Flight {
        vec3 p0, cur;
        int type = 0, kind = 0;      // 0 a cleared product, 1 game over (falls)
        float t = 0, tChute = 0, seed = 0, spin = 0, flash = 0;
        float yRow = 0;
        vec3 vel;
        bool scanned = false, landed = false;
        int slot = -1;
    };
    std::vector<Flight> flights_;
    float nextChute_ = 0.f;          // when the chute is free next (render time)
    float beep_ = 0.f;
    float divider_ = -1.f;           // a divider bar riding the belt (its y), < -50: none
    float dividerWait_ = 6.f;

    // ---- The products in the well.
    float seeds_[22][10] = {};
    void buildBoard(const Game& g);
    void product(std::vector<Box>& out, const vec3& c, float half, int type, float seed, float flash = 0.f, float rot = 0.f);

    // ---- Instances.
    std::vector<Box> static_, boxes_, cells_;
    std::vector<Blob> staticBlobs_, blobs_;
    std::vector<Sprite> sprites_;
    std::vector<Line> hair_;
    size_t staticCount_ = 0, staticBlobCount_ = 0;
    std::vector<vec4> pointP_, pointC_;

    // ---- The camera: the layout's (straight down onto the counter).
    mat4 vp_;
    vec3 eye_;
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
    GLuint progBox_ = 0, progBlob_ = 0, progSprite_ = 0, progLine_ = 0, progVeil_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint cubeVbo_ = 0, quadVbo_ = 0, stripVbo_ = 0;
    GLuint staticVao_ = 0, staticVbo_ = 0, boxVao_ = 0, boxVbo_ = 0, blobVao_ = 0, blobVbo_ = 0, sblobVao_ = 0, sblobVbo_ = 0,
           spriteVao_ = 0, spriteVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
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
    float dropAmt_ = 0.f, swell_ = 0.f;
    float kickIn_ = 0.f, kickGlow_ = 0.f, refreshK_ = 0.f;
    float tetrisT_ = 99.f;
    double lastBeat_ = -1.0;
    float beatFlash_ = 0.f, beatAcc_ = 0.f;
    float worldTime_ = 0.f, beltPhase_ = 0.f, beltK_ = 0.5f;
    double time_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x6E0C};
};

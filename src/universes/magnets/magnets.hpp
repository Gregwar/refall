#pragma once
// MAGNETS: the pieces are magnets lying on a sheet of paper strewn with iron filings (see magnets.cpp).
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class MagnetsUniverse : public Universe {
public:
    ~MagnetsUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override { tapPending_ = std::max(tapPending_, 0.5f); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    // A cell of the stack: the magnet it belongs to (its axis and its polarity).
    struct Mag {
        uint64_t id = 0;
        int type = -1;
        vec2 axis{0, 1};   // the piece's south -> north axis (unit)
        float sign = 1.f;  // its own polarity (the drop's flip is global, see pol_)
        float shown = 1.f; // the halves as drawn (eases toward sign * pol_)
    };
    struct Dying { vec2 p; int type, mask; vec2 cen, axis; float t, delay; };
    struct CellInst { float a[4], n[4], s[4], m[4]; };
    struct ShapeInst { float p[4], q[4], c[4]; };
    struct Impulse { float r[4], k[4]; };
    struct Glyph { float x0 = 0, y0 = 0, x1 = 0, y1 = 0, ox = 0, oy = 0, w = 0, h = 0, adv = 0; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[97];
        void* info = nullptr;
    };

    void createTargets();
    void seedFilings();
    void buildFont();
    float textWidth(const std::string& s, float size) const;
    void text(const std::string& s, float x, float y, float size, vec3 col, float alpha, float track);
    int pickVariant(const SongLook& look) const;
    vec3 pieceColor(int type) const;
    void syncStack(const Game& game);
    void addCharges(vec2 c, vec2 axis, float q, std::vector<float>& out);
    void buildCharges(const Game& game);
    vec2 fieldAt(vec2 p) const;
    void addPieceCells(const int (*cells)[2], int n, int type, vec2 origin, float scale, bool gameCoords, vec2 axis,
                       float shown, float alpha, int mode, float lift, std::vector<CellInst>& out);
    void shape(vec2 a, vec2 b, float w0, float w1, vec3 col, float alpha);
    void impulse(float x0, float y0, float x1, float y1, float speed, float spin, float reach, float up);
    void tap(float amount) { tapNow_ = std::max(tapNow_, amount); }
    void simulate(float dt);

    // GL
    GLuint progSim_ = 0, progFiling_ = 0, progCell_ = 0, progShape_ = 0, progPaper_ = 0, progText_ = 0, progPost_ = 0;
    GLuint quadVbo_ = 0, emptyVao_ = 0, filingVao_ = 0, cellVao_ = 0, cellVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint textVao_ = 0, textVbo_ = 0;
    GLuint stateTex_[2][2] = {}, stateFbo_[2] = {}, chargeTex_ = 0;
    int cur_ = 0;
    static constexpr int FW = 256, FH = 256, MAXQ = 1024;
    ugl::Target sceneT_;
    Font font_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};
    float px_ = 0.05f; // board units per pixel
    bool seeded_ = false;

    // the song and its staging
    int variant_ = 0;
    int level_ = 1;
    int stage_ = 1;          // 0 calm, 1 mid, 2 chorus, 3 drop
    bool dropOn_ = false;
    float stg_[4] = {1, 0, 0, 0}; // stage weights (eased)
    float pal_ = 0.f;        // the drop's palette (eased 0..1)
    float pol_ = 1.f;        // global polarity: the drop turns it over
    bool slowOn_ = false;
    float slow_ = 0.f;
    float pauseFade_ = 0.f;
    float kickPrev_ = 0.f;
    bool kickArmed_ = true;
    long lastBar_ = -1;
    int barsInDrop_ = 0;
    float tapPending_ = 0.f, tapNow_ = 0.f, spinNow_ = 0.f, spinPending_ = 0.f;
    float flash_ = 0.f;      // the drop's swell (the filings darker for a moment)
    Bridge bridge_;
    float time_ = 0.f;

    // the stack, the falling piece
    Mag mag_[22][10];
    uint64_t serial_ = 1;
    uint64_t curId_ = 0;
    float curSign_ = 1.f;
    int curType_ = -1, curRot_ = 0;
    float curShown_ = 1.f;
    std::vector<Dying> dying_;
    float overT_ = -1.f;

    // per frame
    std::vector<float> charges_; // x y q 0
    int nStatic_ = 0, nPiece_ = 0;
    std::vector<Impulse> imps_;
    std::vector<float> carry_; // rows fallen after a clear (see SIM_FS)
    std::vector<CellInst> cells_;
    std::vector<ShapeInst> shapes_;
    std::vector<float> textVerts_;
    int occ_[22] = {};
    Rng rng_{0x6A6E7u};
};

#pragma once
// PLANKS: wooden plank blocks in a white studio; every cell a solid block of three stained planks, and every cleared
// row pushes its planks out, Jenga style, to build a plank village of thin planks beside the well: a
// tower, a temple, a track with a train, a pagoda, a house (see planks.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/planks/build.hpp"

class PlanksUniverse : public Universe {
public:
    ~PlanksUniverse() override;
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

private:
    using Q = planks_build::Q;
    // ---- Instances: one format for every plank, the board and the plinth (see shaders.hpp).
    struct Inst { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<Inst> inst_;
    size_t previewStart_ = 0, previewEnd_ = 0; // the previews (no shadow)
    void push(const vec3& c, const vec3& size, const Q& q, const vec3& tint, float tintK, float seed, float stack,
              float grain, float mat = 0.f, float glow = 0.f, float fog = 0.f);
    // A cell of the well: three thick planks laid flat (a unit block centered on c, scaled by sc, turned by q).
    void pushCell(const vec3& c, float sc, const Q& q, int type, float seed, float stack, float gap, int skipPlank = -1,
                  float glow = 0.f);
    vec3 cellPlankPos(int k, vec3* size, Q* q) const; // the cell's k-th plank (unit cell)
    int cellPlanks() const;

    // ---- The well.
    float seeds_[22][10] = {};
    float press_[22][10] = {}, pressV_[22][10] = {};
    float settle_[22][10] = {};   // since the cell locked (its courses settling)
    int lockCount_ = 0;
    void syncCells(const Game& g);
    void buildBoard(const Game& g);
    std::vector<float> flat_;     // the ghost's outline

    // ---- The village: each side's program of planks (build.hpp), the planks it has received.
    struct Slot {
        int state = 0;            // 0 waiting, 1 in flight, 2 placed
        float t = 0.f;            // since it landed
        int type = 0;
        bool painted = false;     // a quad's plank keeps its paint
        float seed = 0.f;
    };
    std::vector<planks_build::PDef> prog_[2];
    std::vector<Slot> slots_[2];
    int reserved_[2] = {0, 0}, extras_[2] = {0, 0};
    float lastArrive_[2] = {0.f, 0.f};
    void resetVillage();
    void ensureProgram(int side, int n);
    planks_build::PDef placed(int side, int i, bool withMotion) const; // the plank's pose now
    void placeNow(int side, int type);
    float slotTintK(bool painted) const;
    void buildVillage();
    // the train: its position along the track (arc length), speed
    float trainS_ = 0.f, trainV_ = 0.f, trainDir_ = 1.f;
    bool trainBuilt() const;

    // ---- Planks in flight from a cleared row to the village, the row's other planks pushed back into the board.
    struct Flight {
        int side = 0, idx = 0, type = 0;
        bool painted = false;
        vec3 p0{0, 0, 0}, runFrom{0, 0, 0}, slideTo{0, 0, 0};
        float t = 0.f, slideDur = 0.2f, runDur = 0.3f, arcDur = 1.2f, seed = 0.f, lane = -9.6f;
        int phase = 0;
    };
    std::vector<Flight> flights_;
    struct ClearCell { int x, y, type; float seed, t, delay; };
    std::vector<ClearCell> clearing_;
    struct Fall { vec3 p, v; Q q0, q1; float t, delay, seed; int type; float sc; bool stackCell; };
    std::vector<Fall> falls_;    // game over: the stack's cells dropping into the plinth
    void updateFlights(float dt);
    int hookLines_ = 0;

    // ---- The village's game over: every plank topples, from the top down.
    struct Topple { vec3 p0, p1; Q q0, q1; int type; bool painted; float seed, delay, dur, t; };
    std::vector<Topple> topple_;
    float toppleFade_ = 0.f;
    bool toppled_ = false;
    void toppleVillage();

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontMed_, fontLight_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progStudio_ = 0, progPlank_ = 0, progShadow_ = 0, progPre_ = 0, progPreTable_ = 0, progSsao_ = 0,
           progBlur_ = 0, progFlat_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint boxVbo_ = 0, boxVao_ = 0, instVbo_ = 0, flatVao_ = 0, flatVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_[2] = {0, 0}, shadowTex_[2] = {0, 0};
    GLuint preFbo_ = 0, prePos_ = 0, preNrm_ = 0, preDepth_ = 0;
    ugl::Target sceneT_, aoT_[2];
    int samples_ = 4;
    mat4 vp_, view_, proj_, invVp_, lightVp_[2];
    vec3 eye_{0, 5, 34};
    void drawPlanks(GLuint prog, bool shadow);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 pieceTint(int type) const; // linear
    vec3 backdrop() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropRelease_ = 0.f;
    float swell_ = 0.f;
    float kickIn_ = 0.f;
    double lastHalf_ = -1.0;
    float beatClock_ = 0.f, beatLen_ = 0.5f;
    double beat_ = 0.0;
    bool hasBeat_ = false;
    float rippleT_ = 9.f, rippleA_ = 0.f;   // a wave running up the constructions
    float glowAll_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    bool bare_ = false;
    int lines_ = -1;
    Bridge bridge_;
    Rng rng_{91};
};

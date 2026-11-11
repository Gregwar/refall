#pragma once
// ARCHITECT: a modern villa built over one song on an empty green field, its rooms lowered by two tower cranes, one
// for every cleared row; from a morning field to the finished villa lit at dusk, its pool glowing (see architect.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"
#include "universes/architect/site.hpp"

class ArchitectUniverse : public Universe {
public:
    ~ArchitectUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override {}
    void kick(float amount) override { kickIn_ = std::max(kickIn_, amount); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }

private:
    using Q = arch_site::Q;
    using Part = arch_site::Part;
    // ---- Instances: one format for every solid (see shaders.hpp), one list per mesh.
    struct Inst { float a[4], b[4], c[4], d[4], e[4]; };
    std::vector<Inst> inst_[arch_site::MESH_COUNT];
    std::vector<Inst> blocks_;                     // the well's pieces (boxes, the board's camera)
    void push(const Part& p, float glow = 0.f, float alt = 0.f);
    void pushRaw(int mesh, const vec3& c, const vec3& s, const Q& q, int mat, const vec3& col, float param = 0.f,
                 float glow = 0.f, float depth = 0.f, float alt = 0.f);

    // ---- The site: the program's elements and how far each one has come.
    std::vector<arch_site::Element> prog_;
    std::vector<Part> scenery_;
    struct ElState {
        int state = 0;          // 0 not yet, 1 waiting for its crane, 2 on the hook, 3 in place
        float t = 0.f;          // since it landed
        float vis = 0.f;        // 1 shown .. 0 gone (a new game takes the villa down)
        float wait = 0.f;       // how long it has waited for its crane (earned, not yet lowered)
        bool direct = false;    // set down without its crane (a backlog caught up): it settles from above
        vec3 slide{0, 0, 0};    // released beside its place (under an overhang): it slides in from there
    };
    std::vector<ElState> els_;
    arch_site::Palette pal_;
    int builtVariant_ = -1;
    void buildSite();
    float pTarget_ = 0.f, p_ = 0.f;                // the paced lines: the target, the displayed progress
    bool pInit_ = false;
    int lastLines_ = -1;
    float elementK(const arch_site::Element& e) const; // GROW / DIG / TREE / POP / DRIVE: 0..1 (with leaving)
    void drawElements();
    void placeDueNow();                            // a still: everything due is in place at once
    float tilesK() const, lightK(int room) const, poolLightK() const, lampK() const;
    void setDown(int i);                           // an element earned but late: set down at once (it settles from above)
    bool built() const;                            // every element the lines have earned stands in place
    float houseK_ = 0.f;                           // the finale (the lights), only once the house is complete
    float hillY_ = 0.f;

    // ---- The cranes.
    struct Crane {
        float a = 0, av = 0, r = 12, rv = 0, h = 14, hv = 0;   // slew (radians), trolley (m), hook height (m)
        float ta = 0, tr = 12, th = 14;
        float sx = 0, sz = 0, svx = 0, svz = 0;                 // the load's sway (m)
        vec3 lastT{0, 0, 0}, lastV{0, 0, 0};
        bool primed = false;
        int job = -1, phase = 0;
        float jobT = 0.f;       // how long the job has run (a watchdog)
        vec3 approach{0, 0, 0}; // where the load is lowered: its place, or beside it, clear of what stands above it
        float wait = 0.f, wob = 0.f, wobV = 0.f;
        float erect = 0.f, depart = 0.f;
        vec3 yard{0, 0, -16};
        std::vector<int> queue;
        float beacon = 0.f;
    };
    Crane cranes_[2];
    void updateCranes(float dt, float workDt);
    void drawCranes();
    vec3 trolleyPos(int i) const;
    vec3 hookPos(int i) const;
    float mastH(int i) const { return arch_site::MAST_H + 2.f * i; }
    float clearance(int i) const;
    vec3 hiddenYard(int i) const;
    vec3 approachOf(int e) const;                  // the shift that clears the descent of what stands above

    // ---- Life on the site: sparks on the beat, the mixer's drum, the floodlights.
    struct Spark { vec3 p, v; float t, life, size; };
    std::vector<Spark> sparks_;
    float drum_ = 0.f, flood_ = 0.f;
    std::vector<float> glows_;                    // x y z size, r g b soft
    void glow(const vec3& p, float size, const vec3& col, float soft = 0.3f);
    struct PLight { vec3 p; float r; vec3 col; };
    std::vector<PLight> lights_;

    // ---- The well.
    float seeds_[22][10] = {};
    void syncCells(const Game& g);
    void buildBoard(const Game& g);
    struct ClearCell { int x, y, type; float seed, t, delay; };
    std::vector<ClearCell> clearing_;
    struct Dust { vec3 p, v; float t, life, size; };
    std::vector<Dust> dust_;
    std::vector<float> flat_;                     // soft quads in board space: x y z, r g b a, qx qy sx sy
    void quad(const vec3& c, float hw, float hh, const vec3& col, float a, float sx, float sy);
    void line(float x0, float y0, float x1, float y1, float z, float w, const vec3& col, float a);
    vec3 pieceCol(int type) const;
    float overT_ = -1.f, dull_ = 0.f;
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
    Font fontReg_, fontLight_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.75f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();
    const char* stageName() const;

    // ---- GL.
    GLuint progBg_ = 0, progInst_ = 0, progShadow_ = 0, progPre_ = 0, progPreGround_ = 0, progSsao_ = 0, progBlur_ = 0,
           progGlow_ = 0, progBlock_ = 0, progFlat_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint meshVao_[arch_site::MESH_COUNT] = {}, meshVbo_[arch_site::MESH_COUNT] = {}, instVbo_[arch_site::MESH_COUNT] = {};
    int meshCount_[arch_site::MESH_COUNT] = {};
    GLuint blockVao_ = 0, blockVbo_ = 0;
    GLuint glowVao_ = 0, glowQuad_ = 0, glowVbo_ = 0, flatVao_ = 0, flatVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    GLuint preFbo_ = 0, prePos_ = 0, preNrm_ = 0, preDepth_ = 0;
    ugl::Target sceneT_, aoT_[2];
    int samples_ = 4, aoW_ = 1, aoH_ = 1;
    mat4 vp_, view_, proj_, invVp_, lightVp_;     // the world's camera
    vec3 eye_{0, 16, 50};
    mat4 bvp_;                                    // the board's (the layout's)
    vec3 beye_{0, 0, 40};
    float camYaw_ = 0.f;
    void updateCamera();
    void drawInstances(GLuint prog, bool shadowPass);

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;
    vec3 backdrop() const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropRelease_ = 0.f, swell_ = 0.f, kickIn_ = 0.f;
    double lastHalf_ = -1.0;
    float beatClock_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f, gaugeShown_ = 0.f, dimT_ = 0.f, night_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{77};
};

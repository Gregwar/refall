#pragma once
// SONAR: a dark sea where shapes surface from the black when an echo returns from them (see sonar.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SonarUniverse : public Universe {
public:
    ~SonarUniverse() override;
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

private:
    struct Inst { float a[4], b[4], c[4]; };
    struct MoteInst { float a[4], b[4]; };

    // ---- The echoes: pings on the beat grid, each lighting what lies around one point for a moment.
    struct Ping { float x = 0, y = 0, age = -1.f, strength = 0, reach = 4.f; };
    Ping pings_[8];
    int pingNext_ = 0;
    long long pingSlot_ = -1;
    int pingSide_ = 1;
    struct Anchor { float x, y; int layer; }; // layer 0 seabed, 1 wreck, 2 city
    std::vector<Anchor> anchors_;
    int lastAnchor_[2] = {-1, -1};
    void buildAnchors();
    void ping(float x, float y, float strength, float reach);
    void pingSomewhere(int side, float strength, float reach, int maxLayer, float cityChance);

    // ---- The chart: each cleared row maps a patch of the seabed (then of the city) for good.
    struct Chart { float x, y, r, amount = 0, delay = 0; bool fading = false; };
    std::vector<Chart> charts_;
    std::vector<vec3> chartOrder_; // x, y, radius: the spots in the order they are mapped
    size_t chartNext_ = 0;
    void buildChartOrder();
    int chartSpot(float delay);

    // ---- Light sinking from a cleared row to the spot it maps; the contact of a hard drop.
    struct Mote { vec2 p0, p1, p; float t = 0, life = 1.4f, delay = 0, size = 0.18f, peak = 1.f, stretch = 1.f; vec3 col; bool glint = false; };
    std::vector<Mote> motes_;
    struct Sink { float x, y; vec3 col; float t, dur, delay; int kind; };
    std::vector<Sink> sinks_;

    // ---- Drawing.
    std::vector<Inst> cells_;
    std::vector<MoteInst> moteInst_;
    void buildBoard(const Game& g);
    void addCell(float x, float y, float half, int kind, const vec3& col, float alpha, float flash, float prog);
    void drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontCap_, fontIt_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.45f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progWorld_ = 0, progWell_ = 0, progPiece_ = 0, progMote_ = 0, progDown_ = 0, progBlur_ = 0, progFinal_ = 0,
           progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, pieceVao_ = 0, pieceVbo_ = 0, moteVao_ = 0, moteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0,
           textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, bloomA_, bloomB_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant.
    int variant_ = 0;
    vec3 waterTop_, waterLow_, echo_, echoCity_, dropEcho_;
    vec3 waterTopT_, waterLowT_, echoT_, echoCityT_, dropEchoT_;
    vec3 pieceCol_[7], pieceColT_[7];
    bool colSet_ = false;
    uint64_t seed_ = 1;
    float seedF_ = 0.f;
    vec3 bedPhase_{0.f, 0.f, 0.f};
    float bedY(float x) const;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, swell_ = 0.f, tetrisGlow_ = 0.f;
    float kickIn_ = 0.f, kickEnv_ = 0.f;
    double time_ = 0.0;
    float worldTime_ = 0.f;
    double beat_ = 0.0;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{0x50A4};
};

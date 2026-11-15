#pragma once
// SWISS POSTER: a flat-design travel poster's landscape in parallax behind the well (see swiss.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SwissUniverse : public Universe {
public:
    ~SwissUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override { (void)strength; }
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override {}
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    HudInk hudInk(const HudText& h) override;

private:
    // ---- Flat shapes (pixels): rectangles, rounded plates, discs, birds, each clipped to a rectangle.
    struct Shape { float r[4], c[4], k[4], clip[4]; };
    std::vector<Shape> shapes_;
    void rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a);
    void rectPxClip(float x0, float y0, float x1, float y1, const vec3& c, float a, const float clip[4]);
    void rectB(float x0, float y0, float x1, float y1, const vec3& c, float a); // board units (y up)
    void plateB(float x0, float y0, float x1, float y1, float radius, const vec3& c, float a);
    void discB(float cx, float cy, float r, const vec3& c, float a);
    void birdB(float cx, float cy, float halfW, float beat, const vec3& c, float a);
    void flushShapes();
    vec2 px(float x, float y) const; // board -> pixels
    float unitPx_ = 45.f;            // pixels per board unit

    // ---- The landscape.
    struct El { float x, depth, size, kind; }; // kind: 0 chalet, 1 house, 2 church, 3 fir, 4 cypress, 5 bush, 6 larch,
                                               // 7 viaduct
    std::vector<El> els_;
    int elFg_ = 0;                   // els_[0, elFg_) stand on the meadow, the rest in the foreground
    void composeScene();
    void drawLandscape();
    float layerOff_[7] = {};
    float cloudOff_[3] = {};
    double cloudDrift_[3] = {};
    double scroll_ = 0.0;            // the camera's travel to the right (foreground units)
    float speed_ = 0.5f;
    // The meadow's stretches along its loop (SPAN units each from ST_ORIGIN): their lift, and the roads and rivers
    // running down from them (x, kind: 1 path, 2 zigzag road, 3 river).
    enum { ST_VILLAGE, ST_HAMLET, ST_PASTURE, ST_FOREST, ST_VALLEY, ST_VIADUCT, ST_CHAPEL, ST_PASS };
    static constexpr int STRETCHES = 16;
    static constexpr float SPAN = 24.f, ST_ORIGIN = 2.f;
    float lift_[16] = {};
    std::vector<vec2> roads_;
    // The drop: its camera travel (from its first beat), its push-in, the light sweeping across on its start.
    double dropBeat_ = 0.0;
    bool wasDrop_ = false;
    float zoom_ = 0.f, sweepT_ = 99.f, sweepX_ = 0.f, sweepA_ = 0.f;

    // Birds: a flock from the meadow on a clear, flocks crossing the sky in a drop.
    struct Bird { vec2 p, v; float phase, rate, size, glide, wob; };
    std::vector<Bird> birds_;
    void flock(float x, float y, int n, float dir, float climb);
    void updateBirds(float dt);
    float flockBar_ = -1.f;

    // ---- Pieces: one id per piece (its cells merge into one flat shape).
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct Wipe { float y; int type[10], mask[10]; float t, delay, dir; };
    std::vector<Wipe> wipes_;
    vec3 pieceColor(int type) const;
    void drawCell(float x, float y, float half, int mask, const vec3& c, float a, bool snap = true);
    void drawBoard(const Game& g);

    // ---- GL.
    GLuint progShape_ = 0, progLand_ = 0;
    GLuint quadVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{22, 12.4f};

    // ---- Song, variant.
    int variant_ = 0;
    uint64_t seed_ = 1;

    // ---- Staging.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float dropAmt_ = 0.f;            // the drop's look, eased (only while music.inDrop)
    bool dropHit_ = false;
    float lockLine_ = 0.f, lockX0_ = 0.f, lockX1_ = 0.f, lockY_ = 0.f;
    float time_ = 0.f, sceneT_ = 0.f;
    double beat_ = 0.0;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    bool bare_ = false; // this frame is the world alone (a break): the landscape, no game, no panel
    // around a song (Frame::setup / teardown): how far each depth is lowered out of place, the sky's dusk
    float sink_[7] = {}, dusk_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{4242};
};

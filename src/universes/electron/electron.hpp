#pragma once
// ELECTRON: solid cubes on a dark substrate, and charges finding them (see electron.cpp).
#include <cstdint>
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class ElectronUniverse : public Universe {
public:
    ~ElectronUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void levelUp() override { pour(0, 14); pour(1, 14); }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override;
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool bridgeable() const override { return true; }
    void setBridge(const Bridge& b) override { bridge_ = b; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;

    // One instanced quad (see SPRITE_FS for the kinds).
    struct Sprite { float a[4], b[4], c[4], d[4]; };

private:
    struct Ant {
        vec2 p;
        float a = 0, w = 0;       // heading, its wandering turn rate
        float speed = 1;          // its own pace
        float gait = 0;
        float alpha = 0;
        int nest = 0;
        int state = 0;            // 0 searching, 1 carrying home, 2 going home empty-handed
        bool retire = false;      // goes back into the nest for good
        vec3 crumb{1.f, 1.f, 1.f};
        float since = 0;          // seconds since it left the nest or found food
        float dist = 0;           // distance walked since (the pheromone it lays weakens with it)
        float life = 60;          // how long it searches before heading home
        float fid = 0.6f;         // how faithfully it follows a trail
        float stray = 0;          // seconds left wandering off the trail (a branch)
        float pause = 0;          // stopped, antennae touching another's
        float meetCd = 0;
        bool trail = false;       // a trail under its sensors
        float panic = 0;          // scattered by an impact
        float stuck = 0;          // seconds without a free step
        float lustre = 0.1f;
    };
    struct Grain {
        vec2 p, v;
        vec3 col;
        float size = 0.05f, rot = 0, spin = 0, t = 0, life = 1, delay = 0;
        bool food = false;        // lies on the ground for the ants (outside the well)
    };
    struct Spark { vec2 p, v; vec3 col; float t = 0, life = 0.2f, w = 0.012f; };
    struct Flash { vec2 p; vec3 col; float t, life, r; };
    struct Dying { float x, y; int type; float t, delay, seed, nib; bool over; };

    // sim
    void simulate(const Game& game, float wdt, float dt, const MusicState& music);
    void stepAnts(float dt, float speedK, float explore, float layK);
    void relax(std::vector<float>& f, float tau, float dt);
    void spawn(int nest, bool recruit);
    void pour(int nest, int n) { pourLeft_[nest] += n; }
    bool blocked(vec2 p) const;
    bool sugarAt(vec2 p, float half, int* gx = nullptr, int* gy = nullptr) const;
    float pher(const std::vector<float>& f, vec2 p) const;
    void deposit(std::vector<float>& f, vec2 p, float amount);
    void wear(vec2 p, float amount);
    void burst(float x, float y, int type, int n, float speedK, bool quadClear);
    void sparks(vec2 at, vec2 dir, const vec3& col, int n, float speed, float spread, float flash);
    void syncCells(const Game& game);
    void warmUp(const Game& game);

    // drawing
    void drawPanels(const std::vector<HudRect>& panels);
    void drawSprites(const std::vector<Sprite>& v);
    void cube(std::vector<Sprite>& out, float x, float y, float scale, const vec3& col, float alpha, float nib,
              float seed, float glint, bool ghost, float crumbled);
    void shadow(std::vector<Sprite>& out, float x, float y, float half, float blur, float alpha);
    vec3 pieceColor(int type) const;

    GLuint progGround_ = 0, progSprite_ = 0, progVeil_ = 0, progShape_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, shapeVao_ = 0, shapeVbo_ = 0;
    GLuint pherTex_ = 0;
    float veil_ = 0.f; // the veil over the frame (startup fade, pause / game-over dim): the HUD dims under it too
    mat4 vp_;
    vec2 viewC_{0, 0.4f}, viewH_{21, 11.9f};
    float px_ = 0.02f;

    // the pheromones: two coarse fields over the view (toward food, toward home)
    static constexpr float CELL = 0.2f;
    int gw_ = 1, gh_ = 1;
    vec2 g0_;
    std::vector<float> food_, home_, tmp_;
    std::vector<int> bucket_, chain_; // who is near whom
    // the worn paths the ground shows: where the ants walk a lot (a finer field)
    static constexpr float TCELL = 0.1f;
    int tw_ = 1, th_ = 1;
    vec2 t0_;
    std::vector<float> trace_;

    // the world
    vec2 nest_[2];
    float nestR_ = 1.6f;
    std::vector<Ant> ants_;
    std::vector<Grain> grains_;
    std::vector<Dying> dying_;
    std::vector<Spark> sparks_;
    std::vector<Flash> flashes_;
    float sparkTok_ = 0;       // sparking contacts allowed (refills over time)
    static constexpr int MAX_SPARKS = 600;
    float nib_[22][10] = {};
    float seed_[22][10] = {};
    int8_t occ_[22][10] = {};  // the board as the ants know it (-1 empty, else the piece type)
    bool hard_ = false;        // the next lock is a hard drop's
    float openTop_ = -8.7f;   // ants walk inside the well only below this (just above the stack)
    float spawnAcc_[2] = {0, 0}, wanderAcc_[2] = {0, 0}, recruitAcc_[2] = {0, 0};
    float recruits_[2] = {0, 0}; // ants called out by the crumbs brought home
    int pourLeft_[2] = {0, 0};
    bool warm_ = false;
    std::vector<vec2> lockAt_; // impacts this frame (ants scatter)

    // the song and its staging
    int variant_ = 0;
    int level_ = 1;
    bool inDrop_ = false;
    float lv_ = 1, lvTarget_ = 1;  // continuous level
    float dropMix_ = 0;            // the drop's colors
    float swell_ = 0;              // the drop's swell (the trails glow, the colony pours out)
    float energy_ = 0.5f;
    bool slowOn_ = false;
    float slow_ = 0;
    float dimT_ = 0;
    float overT_ = -1;
    float gaugeShown_ = 0;
    float clock_ = 0;
    Bridge bridge_;
    Rng rng_{0xA175};
    std::vector<Sprite> under_, stack_, antsV_, over_, top_;
};

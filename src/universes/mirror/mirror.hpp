#pragma once
// MIRROR: glass cubes in a liminal room, a frameless mirror flying around them (see mirror.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class MirrorUniverse : public Universe {
public:
    ~MirrorUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void drop(float strength) override;
    void refresh(float strength) override;
    void levelUp() override { sheen(0, 0.8f); }
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
    struct Inst { float pos[4], rot[4], tint[4], fx[4]; };
    std::vector<Inst> world_, ui_; // world_: seen in the mirrors too; ui_: the ghost, the previews, the gauge
    void cube(std::vector<Inst>& out, const vec3& c, float half, const float q[4], const vec3& tint, float kind,
              float alpha, float flash, float frost);
    void buildCubes(const Game& g);
    vec3 tintOf(int t) const;

    // ---- The mirrors: the main one always, two more in the drop (a triptych opening from behind it).
    static constexpr int MIRRORS = 3;
    struct Mirror { vec3 c, r, u, n; float alpha = 0.f, sheenT = 9.f, sheenAmp = 0.f, glint = 0.f; };
    Mirror mir_[MIRRORS];
    ugl::Target mirT_[MIRRORS];
    double path_ = 0.0;        // the main mirror's position along its path
    float spin_ = 0.f;         // a Tetris: one full turn, eased
    float spinT_ = 9.f;
    float mirScale_ = 1.f;   // the mirrors' size (a little smaller in the drop's triptych)
    float reach_ = 0.f;        // the path's width (the music's energy, smoothed)
    void updateMirrors(float dt, float wdt, const vec3& eye);
    void sheen(int i, float amp);

    // ---- Clears: shards of glass drifting back and dissolving.
    struct Shard { vec3 p, v, axis, tint; float spin, ang, t, life, size, delay; };
    std::vector<Shard> shards_;
    float flash_[Game::H][Game::W];
    float overT_ = -1.f;
    int overTop_ = 0;

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font fontLight_, fontMed_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& srgb, float alpha, float bias, float soft);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progRoom_ = 0, progCube_ = 0, progCopy_ = 0, progFinal_ = 0, progText_ = 0, progPanel_ = 0;
    GLuint cubeVao_ = 0, cubeVbo_ = 0, instVbo_ = 0, textVao_ = 0, textVbo_ = 0, panelVao_ = 0, panelVbo_ = 0;
    GLuint boardTex_ = 0;
    int cubeVerts_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    ugl::Target roomT_, sceneT_;
    mat4 vp_;
    vec3 eye_;
    float tanH_ = 0.34f;
    void drawCubes(const std::vector<Inst>& v);

    // ---- Variant, lights (eased).
    int variant_ = 0;
    bool palSet_ = false;
    vec3 keyCol_, accCol_[2], ambTop_, ambBot_, haze_, albedo_, panelCol_;
    float hazeK_ = 0.004f;

    // ---- Level, drop, music.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    bool inDrop_ = false;
    float dropWait_ = 0.f;
    float dropK_ = 0.f;        // the drop's state (0 .. 1, eased in, eased out after)
    float swell_ = 0.f;        // the drop's downbeat: one swell of the accents, settling
    float energy_ = 0.4f;
    float kickIn_ = 0.f, kickPrev_ = 0.f;
    float slow_ = 0.f;
    bool slowOn_ = false;
    float time_ = 0.f, world_t_ = 0.f;
    float dimT_ = 0.f;
    float gaugeShown_ = 0.f;
    bool bare_ = false;
    Bridge bridge_;
    Rng rng_{4242};
};

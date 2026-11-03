#pragma once
// SUSHI: maki on a kaiten counter, cleared rows eaten by kawaii cats (see sushi.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class SushiUniverse : public Universe {
public:
    ~SushiUniverse() override;
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

    // A variant's colors (linear RGB): floats only, so two palettes blend.
    struct Palette {
        vec3 wallTop, wallBot, wood, woodDark, belt, lacquer, rim, ink, accent, line, noren,
            norenText, warm;
        vec3 plate[7];
        float night;
    };

private:
    struct Inst { float a[4], b[4], c[4], d[4]; };
    struct CatInst { float v[8][4]; };

    // ---- The stack: each locked piece keeps an id, so its plate stays one shape.
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct Fading { float x, y; int type, mask; float t; }; // a cleared row's empty plate, fading
    std::vector<Fading> fading_;

    // ---- The cats along the counter, on both sides of the box.
    struct Cat {
        int uid = 0, side = 1, slot = 0;
        float x = 0.f;
        int coat = 0;
        vec3 base, patch, patch2, collar;
        float scale = 1.f;
        float appear = 0.f;  // 0..1: popping up from behind the counter
        float leave = -1.f;  // >= 0: ducking away (0..1)
        float chew = 0.f, happy = 0.f, mouth = 0.f, eye = 1.f, doze = 0.f, dozeAt = 0.5f, cheer = 0.f;
        float armL = 0.f, armR = 0.f, incoming = 0.f;
        float phase = 0.f, heartCd = 0.f, nomCd = 0.f, zCd = 0.f, look = 0.f;
        int blinkBeat = 0;
        bool maneki = false;
        int eaten = 0;
    };
    std::vector<Cat> cats_;
    std::vector<float> slotX_[2];  // each side's seats (distance from the middle; 0 left, 1 right)
    std::vector<int> slotOrder_[2];
    int nextCatUid_ = 1, coatCycle_ = 0;
    int eatenLines_ = 0, feastBonus_ = 0;
    float spawnCd_ = 0.f;
    void layoutSlots();
    bool addCat(int side, bool instant);
    int catIndex(int uid) const;
    int pickCat(int side, int seq);
    vec2 mouthOf(const Cat& c) const;
    float catLift(const Cat& c) const;
    void eat(Cat& c);

    // ---- Maki in the air: sliding out of a cleared row, then hopping to a cat.
    struct Flight {
        float x0, y0;
        int side, type, cat;
        float t = 0.f, delay = 0.f;
        bool slide = true, exited = false, plate = false;
        int mask = 0;
        float ex = 0.f, ey = 0.f, exitT = 0.f, arcDur = 0.6f, spin = 0.f, h = 1.5f;
        float x = 0.f, y = 0.f, s = 1.f, rot = 0.f;
    };
    std::vector<Flight> flights_;
    void updateFlights(float dt);

    // ---- The shop: the noren, the belt.
    struct Pop { float x, y, t, life; int kind; vec3 col; float size; }; // 0 heart, 1 sparkle, 2 "nom", 3 "z"
    std::vector<Pop> pops_;
    float beltOff_ = 0.f, beltSpeed_ = 0.f;
    float beltStart() const { return -(viewH_.x - 6.7f) - 1.4f; }

    // ---- Drawing.
    std::vector<Inst> back_, makis_, front_;
    std::vector<CatInst> catInst_;
    void buildBoard(const Game& g);
    void addMaki(float x, float y, float half, int mask, int type, float alpha, float plateA, float spin, float scale,
                 int kind, float makiA, float shadow);
    void buildShop(float beat, float wm, float wp);
    void buildCats(float beat, float wm, float wp, float dt);
    void drawInst(GLuint prog, GLuint vao, const std::vector<Inst>& v);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[100]; // ASCII 32..127 (127: the middle dot), then the kana su, shi, ne, ko
        void* info = nullptr;
    };
    Font fontB_, fontM_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, const vec3& halo, float alpha, float haloA = 0.35f);
    void drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    void drawPops();
    std::vector<float> shapes_;
    void flushShapes();

    // ---- GL.
    GLuint progBack_ = 0, progSprite_ = 0, progCat_ = 0, progMaki_ = 0, progFinal_ = 0, progShape_ = 0, progText_ = 0;
    GLuint quadVbo_ = 0, spriteVao_ = 0, spriteVbo_ = 0, makiVao_ = 0, makiVbo_ = 0, catVao_ = 0, catVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 12};

    // ---- Song, variant.
    int variant_ = 0;
    Palette pal_{}, target_{};
    bool palSet_ = false;
    uint64_t seed_ = 1;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropT_ = 99.f, fest_ = 0.f, swell_ = 0.f, feast_ = 0.f;
    float kickIn_ = 0.f;
    float time_ = 0.f, worldTime_ = 0.f;
    float beat_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    Bridge bridge_;
    Rng rng_{8128};
};

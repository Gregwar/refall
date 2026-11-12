#pragma once
// STAR CHART: an antique engraved celestial map that fills with constellations over the song (see starchart.cpp).
#include <string>
#include <vector>

#include "ugl.hpp"
#include "universe.hpp"

class StarChartUniverse : public Universe {
public:
    ~StarChartUniverse() override;
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

    // A chart's colors (linear RGB) and a few numbers.
    struct Palette {
        vec3 sky, sky2, well, line, faint, star, glow, accent, ink, rim, silver;
        vec3 piece[7];
        float light;     // 1: dark ink on a pale sheet
        float glowA;     // how much the stars and the lines glow
        float starStyle; // 0: engraved starbursts, 1: open circles (a Chinese chart)
        float enamel;    // the pieces' fill opacity
    };

    // A line-art stroke: a polyline in some local frame, its weight and how much ink it has.
    struct Stroke {
        std::vector<vec2> pts;
        float w = 1.f;     // width (multiples of the fine line)
        float alpha = 1.f;
        int role = 0;      // 0 line, 1 faint, 2 accent, 3 silver
        float glow = 0.f;  // a soft halo around it (in the dark charts)
    };

private:
    // ---- Pen strokes: segments in board units, gathered with MAX blending.
    struct LineInst { float a[4], b[4], c[4]; };
    std::vector<LineInst> lines_;
    float fineW() const;
    vec3 roleColor(int role) const;
    void emit(const Stroke& s, float reveal, float alpha);
    void emitAll(const std::vector<Stroke>& ss, float reveal, float alpha);
    // A stroke in a disc's frame (or any frame): points mapped by `map` before emitting.
    void segment(vec2 a, vec2 b, const vec3& c, float alpha, float w, float glow);

    // ---- The two hemispheres: planisphere discs in the margins, turning on their poles in a drop.
    struct Disc {
        vec2 c{0, 0};      // center (board units)
        float R = 4.f;     // the sky's radius (the graduated rim lies outside it)
        float rot = 0.f;   // turned on its pole (radians)
        float dir = 1.f;   // which way it turns
        int count = 0;     // constellations drawn in it (still lettered)
    };
    Disc disc_[2];
    float discW_ = 0.f;    // how far the discs turn per second at the drop (eased)
    vec2 toWorld(int d, vec2 local) const;   // a point of a disc's sky (unturned) -> board units
    void layoutDiscs();
    void buildDisc(int d, std::vector<Stroke>& out, float detail) const;
    void buildBorder(std::vector<Stroke>& out) const;
    void buildWell(std::vector<Stroke>& out) const;
    void buildRose(vec2 c, float r, std::vector<Stroke>& out) const;
    void buildCartouche(vec2 c, float w, float h, std::vector<Stroke>& out) const;
    void buildLegend(vec2 c, float w, float h, std::vector<Stroke>& out) const;
    void buildArmillary(vec2 c, float r, float t, std::vector<Stroke>& out) const;
    vec2 roseC_{0, 0}, cartC_{0, 0}, legendC_{0, 0}, rose2C_{0, 0};
    float roseR_ = 2.f, rose2R_ = 1.4f, cartW_ = 7.f, cartH_ = 2.f, legendW_ = 6.f, legendH_ = 2.f;
    bool ornaments_ = true;

    // ---- Stars: the fixed field of each disc, the stars the clears bring, and their flights there.
    struct Star {
        int disc = 0;
        vec2 p{0, 0};      // disc-local, unturned
        float mag = 4.f;   // 1 brightest .. 6 faintest
        float born = 0.f;  // its light coming in (0..1)
        float tw = 0.f;    // twinkle (decays)
        float phase = 0.f;
        int cons = -1;     // the constellation it belongs to, -1 none
        bool extra = false; // a faint star a clear brought around its constellation
        float fade = 1.f;  // 0: gone (a crowded sky lets its oldest faint stars go)
    };
    std::vector<Star> stars_;
    struct Flight {
        vec2 from{0, 0};   // board units
        int star = -1;     // where it goes (index in stars_)
        float t = 0.f, dur = 1.5f, delay = 0.f;
        float bend = 1.f;
    };
    std::vector<Flight> flights_;
    void seedField();

    // ---- Constellations: one per clear, drawn line by line on the bars; their figures come alive in a drop.
    struct Constellation {
        int disc = 0, figure = 0;
        vec2 c{0, 0};      // disc-local, unturned
        float scale = 1.f, ang = 0.f;
        bool mirror = false;
        std::vector<int> stars;              // indices in stars_ (anchors first, in the figure's order)
        std::vector<std::pair<int, int>> edges; // anchor pairs
        std::vector<float> edgeReveal;
        int nextEdge = 0;
        float ready = 0.f;   // its stars arrived (seconds since)
        float figure01 = 0.f; // the figure traced (0..1)
        float figureInk = 0.f; // how much of the figure stays between drops
        float nameA = 0.f;
        float age = 0.f;
        float big = 0.f;     // a quad's constellation
        bool retired = false; // a crowded hemisphere lets its oldest go: its lines fade, its stars stay
        float shown = 1.f;
        std::string name;
    };
    std::vector<Constellation> cons_;
    int figureBag_[16] = {};
    int figureBagN_ = 0;
    int nextFigure();
    void addConstellation(const std::vector<vec2>& from, bool big);
    vec2 consPoint(const Constellation& k, vec2 p) const;  // figure space -> disc-local
    void figureStrokes(int fig, float life, float t, std::vector<Stroke>& out) const;
    void penStep(float speed);   // a bar (or a beat): the pen draws the next lines

    // ---- Nebulae (a quad) and comets.
    struct Nebula { int disc; vec2 p; float r, ang, born; };
    std::vector<Nebula> nebulae_;
    struct Comet { int disc; vec2 a, b, ctl; float t, dur; };
    std::vector<Comet> comets_;
    struct Glint { vec2 p; float t, size; };  // a lock's small glint
    std::vector<Glint> glints_;

    // ---- Pieces.
    int ids_[22][10] = {};
    int nextId_ = 1;
    void syncIds(const Game& g);
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, float half, int mask, const vec3& tint, float alpha, int mode, float erase, float shine);
    void buildBoard(const Game& g);
    struct Fading { float x, y; int type, mask; float t; };
    std::vector<Fading> fading_;
    std::vector<Fading> overCells_;  // the stack as it was when it topped out (the game clears its board)

    // ---- Star sprites.
    struct StarInst { float a[4], b[4], c[4]; };
    std::vector<StarInst> sprites_;
    void sprite(vec2 p, float size, const vec3& c, float alpha, float rays, float glow, float spin = 0.f);

    // ---- Text.
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, scaleTT = 1.f, capH = 40.f;
        Glyph g[96];
        void* info = nullptr;
    };
    Font caps_, ital_;
    void buildFont(Font& f, const unsigned char* ttf);
    float textWidth(const Font& f, const std::string& s, float size, float track) const;
    void textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const;
    // Along an arc (pixels: center, radius; the text centered on angle `a`, reading clockwise over the top or
    // counterclockwise under the bottom).
    void arcQuads(const Font& f, const std::string& s, vec2 c, float r, float a, float size, float track, bool under,
                  std::vector<float>& out) const;
    void drawText(const Font& f, const std::vector<float>& quads, const vec3& ink, float alpha, float halo = 0.f);
    void drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels);
    struct Label { std::string text; vec2 pos; float size; float alpha; int role; bool italic; };
    std::vector<Label> labels_;
    void drawLabels(float world);

    // ---- GL.
    GLuint progSky_ = 0, progLine_ = 0, progLayer_ = 0, progCell_ = 0, progStar_ = 0, progShape_ = 0, progText_ = 0,
           progPost_ = 0;
    GLuint quadVbo_ = 0, lineVao_ = 0, lineVbo_ = 0, cellVao_ = 0, cellVbo_ = 0, starVao_ = 0, starVbo_ = 0,
           shapeVao_ = 0, shapeVbo_ = 0, textVao_ = 0, textVbo_ = 0;
    ugl::Target sceneT_, lineT_;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    float pxPerUnit_ = 20.f;

    // ---- Song, variant.
    int variant_ = 0;
    bool palSet_ = false;
    Palette pal_{}, basePal_{};
    uint64_t seed_ = 1;
    std::string chartNo_;

    // ---- Staging: continuous level 0 calm .. 2 peak.
    int level_ = 1;
    bool hasLevel_ = false;
    float lv_ = 1.f, lvFrom_ = 1.f, lvTo_ = 1.f, lvT_ = 1.f, lvDur_ = 1.f;
    float energy_ = 0.5f;
    bool dropMode_ = false;
    float dropAmt_ = 0.f, swell_ = 0.f, detail_ = 0.f;
    double lastBar_ = -1.0, lastHalf_ = -1.0;
    float barClock_ = 0.f, beatClock_ = 0.f;
    float kickIn_ = 0.f;
    float quadGlow_ = 0.f;

    bool slowOn_ = false;
    float slow_ = 0.f, armT_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float gaugeShown_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
    Bridge bridge_;
    Rng rng_{1606};
};

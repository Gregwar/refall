// FRACTAL: an infinite pastel flower, a Julia set zoomed for ever.
//
// The whole screen is the filled Julia set of z -> z^2 + c for c at the center of the Mandelbrot set's 1/4 bulb (a
// flower of four petals meeting at every pinch point), in flat pastel bands: the smooth convergence count inside the
// petals (pale, a fine line between them), the smooth escape count outside (one gradient step a band, every band one
// flat color). A fine line between bands and a soft shadow under each, like cut paper; the bands' edges filtered over
// their width on screen, and where they get finer than a pixel (along the frilled contour) they melt into the wash
// color, so the endless detail never shimmers nor frays into dots.
//
// The camera zooms for ever into a self-similar point of the set: a repelling periodic point (the alpha fixed point
// where four petals meet, the beta fixed point, a 2-cycle point) or one of its preimages. Near it the next level of
// zoom (K iterations, its exact multiplier) is the very same picture, so the view is drawn at a depth between D0 and
// D0 + 1 and loops seamlessly while the contour keeps revealing finer petals; the bands' count follows the levels
// zoomed, so each band keeps its flat color as it grows and the new ones bloom with the next colors of the gradient.
// Each song picks its point; now and then (a change of section) the camera glides to another one: it zooms out
// until both are in view, pans across, and zooms back in (the colors stay continuous all along). The flower's heart
// sits in a free zone beside the HUD columns, left or right by point.
//
// The music: the zoom never stops, very slow in calm sections; a line clear makes it accelerate (a burst eased in and
// out: about a level, x1.27, a line, more for a quad); a DROP section (music.inDrop only) zooms in steadily and
// faster, eased in and to a stop after it, the bands cross-fading into the palette's drop colors. No beat pulses.
// Slow motion is dusk (the colors drained toward lilac, the zoom slowed); game over greys the stack from the top.
//
// Around a song: the setup dives from the whole flower, small on the paper, down into its point; the teardown pulls
// the camera back out to the whole flower.
//
// The well is a calm pale panel inside a slim line, a soft shadow under it. The blocks are truly flat: a single flat
// color (no outline, no shading, no highlight), the cells of a piece one joined shape (each locked cell keeps its
// piece, its ids following the rows down on a clear; a side shared with a cell of the same piece runs on into it, an
// inner corner fills only where the diagonal cell is in the piece too), a hairline gap between pieces, only the
// shape's outer corners barely rounded; the falling piece, the previews and the cleared cells (which shrink away
// together) the same. Seven soft retro colors, one per piece type. The ghost is a faint wash of the piece's color
// inside one thin line of it, around the piece's shape.
//
// Seven palettes (SUNSHINE, APRICOT, SAGE, MIDNIGHT: a deep pastel night, RAINBOW, and two near-black ones, OBSIDIAN:
// black and graphite bands, silver lines turning opal in the drop, EMBERS: brown-black and oxblood bands, ember lines
// that glow like blown coals in the drop), from the key, mode, brightness and air: the darkest minor songs take the
// near-black ones (EMBERS for a warm key's hue, OBSIDIAN for a cool one). Cost: one
// fullscreen pass (the fractal: under a millisecond of GPU at 1080p; REFALL_GPUTIME=1 prints it), the blocks as
// instanced quads.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "layout.hpp"
#include "ugl.hpp"
#include "universes/fractal/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Fraunces Soft Italic (a soft, Windsor-like 70s serif) and Comfortaa Bold (rounded), SIL Open Font License, see
// OFL-Fraunces.txt and OFL-Comfortaa.txt: the universe's own lettering (REFALL_HUD=universe).
const unsigned char FONT_DISPLAY[] = {
#include "universes/fractal/Fraunces-SoftItalic.ttf.inc"
};
const unsigned char FONT_ROUND[] = {
#include "universes/fractal/Comfortaa-Bold.ttf.inc"
};

namespace sh = fractal_shaders;
using ugl::set1f;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using cplx = std::complex<double>;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A palette's colors (sRGB), by slot: the gradient's six stops and its drop colors, the lines between bands, the
// wash (the contour, too fine to band), the shadows' tint, the well and its line, the accent, the HUD's two inks, the
// seven pieces. A near-black palette also has drop lines: in the drop, the lines cycle through its three colors
// (every dropCycle seconds), an opal sheen, or embers breathing; and its petals' inside, which takes the line's color,
// takes it darkened (by inside), so that its lines can be bright.
enum Slot {
    BAND0, DROP0 = BAND0 + 6, LINE = DROP0 + 6, WASH, SHADE, WELL, WELL_LINE, ACCENT, HUD_PALE, HUD_DEEP, PIECE0,
    SLOTS = PIECE0 + 7
};
struct VariantDef {
    const char* name;
    unsigned c[SLOTS];
    unsigned dropLine[3];   // (none: 0, the line stays)
    float dropCycle;
    float inside;           // (none: 0, the line's color itself)
};
const VariantDef VARIANTS[] = {
    // peach, butter, coral, sky, pink; the drop: a sunset, saturated but still pastel
    {"SUNSHINE",
     {0xfbe2a4, 0xf9c7a3, 0xf5b0a5, 0xfbead7, 0xbddcec, 0xf6cad6,
      0xffd166, 0xffa25e, 0xff7f7a, 0xf47fb5, 0xb98be6, 0xffc2a8,
      0xfff8ee, 0xfff8ee, 0xc58f86, 0xfffaf2, 0xe0ad94, 0xd9573a, 0xfff7e8, 0x55392f,
      // I O T S Z J L
      0x7fb2d3, 0xedae2c, 0xa47aa6, 0x3f9a84, 0xe2553b, 0x3f6c9c, 0xf0893c}},
    // dusk: apricot, rose, lilac, sand; the drop: rose, violet, teal
    {"APRICOT",
     {0xf8c8a8, 0xf1b0b2, 0xd8bade, 0xf8dcc4, 0xf4bf9e, 0xe6c6df,
      0xf58fa8, 0xc48be0, 0x6fc9c2, 0xffb3a1, 0x9b8ef0, 0xf7c6d9,
      0xfff3ec, 0xfff3ec, 0xb07f8e, 0xfff6f0, 0xd7a3a3, 0xc4515b, 0xfff4ec, 0x4b2c37,
      0x93b0d8, 0xe6a43a, 0x9a7fc0, 0x6aa38d, 0xcf5a5e, 0x5a6ba4, 0xe8875c}},
    // sage, butter, seafoam, sand; the drop: turquoise, lime, lemon
    {"SAGE",
     {0xcfe3c6, 0xf2e5bd, 0xb7d7d1, 0xf5eddb, 0xe5cba8, 0xc8dcb2,
      0x5fd3c7, 0xb5e36b, 0xfbe97a, 0x8fe0b0, 0x43b8d1, 0xe3f59a,
      0xfbfaf0, 0xfbfaf0, 0x8c9c87, 0xfbfbf3, 0xa9bfa2, 0xb4572f, 0xfbf9ef, 0x2f3a31,
      0x7cb3ae, 0xd99f2b, 0x977aa0, 0x6a9a5a, 0xc0473a, 0x46708a, 0xe8894a}},
    // a deep pastel night: dusty indigo, violet, mauve, rose on a night wash; the drop: neon pastels between night bands
    {"MIDNIGHT",
     {0x4a4275, 0x5f4f86, 0x7c6594, 0x3f4b78, 0x8a6c95, 0x585a8b,
      0x241d45, 0x62c9dc, 0x2e2556, 0xf27fc2, 0x2a2350, 0xa995f2,
      0x8f80b4, 0x2b2347, 0x3c3160, 0x221c35, 0x8f80b4, 0xf3b6a0, 0xf6ead6, 0x221c35,
      0x7fb0d0, 0xeabb52, 0xc49ad4, 0x59ac8e, 0xe0604a, 0x6f8fd6, 0xf0904a}},
    // a pastel rainbow: peach, pink, lilac, sky, mint, butter; the drop: the same, a little richer
    {"RAINBOW",
     {0xf9c6a8, 0xf4adc4, 0xcdb5ea, 0xaed4f0, 0xb8e6d0, 0xf8e3a6,
      0xffb48f, 0xf795bb, 0xbb9ef0, 0x92c6f2, 0x99e0c0, 0xffd982,
      0xfff9f2, 0xfff9f2, 0xb8919c, 0xfffaf4, 0xd8b2bd, 0xd0566e, 0xfff7ee, 0x4a3242,
      0x7fb2d3, 0xedae2c, 0xa47aa6, 0x3f9a84, 0xe2553b, 0x3f6c9c, 0xf0893c}},
    // near black: black, charcoal, graphite (a faint warm / cool shift), pearl lines; the drop: an opal sheen (pink,
    // green, gold) between black bands, the lines shimmering through it; jewel tones, softened, for the pieces
    {"OBSIDIAN",
     {0x0c0c0d, 0x1c1b1a, 0x2b2a2b, 0x111111, 0x232120, 0x1a1b1b,
      0x0a0a0b, 0x2c2228, 0x0c0c0c, 0x202a26, 0x0b0b0b, 0x2c281e,
      0xcdc9c3, 0x1f1e1f, 0x3a3838, 0x0f0f10, 0x8a8682, 0xe6c2cc, 0xf0ece6, 0x0f0f10,
      0x8fbcc6, 0xdcc488, 0xb79ccb, 0x86bc9e, 0xd98c90, 0x939fd2, 0xe3a77c},
     {0xf2c4d6, 0xc4ead6, 0xf0dca8}, 3.5f, 0.42f},
    // near black, warm: black, brown-black, oxblood, aubergine-brown, thin ember lines; the drop: the coals blown on
    // (the bands warm up, the lines brighten and breathe); warm pieces
    {"EMBERS",
     {0x0b0807, 0x1d120e, 0x2b1315, 0x140c0b, 0x25151b, 0x1f140f,
      0x100706, 0x3a1209, 0x170a08, 0x50200b, 0x130807, 0x3a1215,
      0xe8823a, 0x26140e, 0x3c2620, 0x100b0a, 0x9a4f2c, 0xf2a04c, 0xf6e4d0, 0x100b0a,
      0xe6c792, 0xf0b23c, 0xd48ca2, 0xa6b86c, 0xe45e46, 0xa79ac2, 0xf28e4c},
     {0xf08a38, 0xffc472, 0xd85a26}, 1.7f, 0.45f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));
constexpr int V_SUNSHINE = 0, V_APRICOT = 1, V_SAGE = 2, V_MIDNIGHT = 3, V_RAINBOW = 4, V_OBSIDIAN = 5, V_EMBERS = 6;
bool darkVariant(int v) { return v == V_MIDNIGHT || v >= V_OBSIDIAN; }
// The near-black palette the darkest songs take, from the key's hue `h` (0 .. 1): a warm one (reds, oranges, yellows)
// the embers, a cool one the obsidian.
int nearBlackVariant(float h) { return h < 0.2f || h > 0.9f ? V_EMBERS : V_OBSIDIAN; }

constexpr float CLEAR_T = 0.4f;   // a cleared cell shrinks away in this long (real time)

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}

// ---- The fractal and the points the camera zooms into.
// The Julia parameter: the center of the 1/4 bulb (a super-attracting 4-cycle: four petals at every pinch point).
const cplx JULIA_C(0.2822, 0.5301);
constexpr double S0 = 0.08;        // complex units a board unit at depth 0
constexpr double VIEW_EFOLDS = 7.1; // a target is drawn this many e-folds deep (self-similar to within a hair there)
// The zoom's speeds, in e-folds (one level of the alpha point is 0.24: x1.27).
constexpr float ZOOM_CALM = 0.012f, ZOOM_DROP = 0.17f, ZOOM_LINE = 0.24f, ZOOM_LINE_MORE = 0.1f;

struct Target {
    cplx p;          // the point
    cplx m;          // the multiplier of a level (f^k's derivative at the periodic point it lands on)
    int k = 1;       // iterations a level
    double d0 = 0.0; // the depth drawn (levels)
    vec2 screen;     // where it sits on screen (board units)
};
struct Fractal {
    std::vector<Target> targets;
    // the attracting cycle (the petals' inside): its point by the critical point 0, where it squares the distance
    // each period (e -> gain e^2, gain the derivative over the rest of the cycle), and its period
    float sink[2] = {};
    float sinkGain = 1.f;
    int period = 0;
};
const Fractal& fractal() {
    static const Fractal F = [] {
        Fractal fr;
        const cplx c = JULIA_C;
        const cplx root = std::sqrt(1.0 - 4.0 * c);
        const cplx alpha = 0.5 * (1.0 - root), beta = 0.5 * (1.0 + root);
        const cplx r2 = std::sqrt(-3.0 - 4.0 * c);
        const cplx q1 = 0.5 * (-1.0 + r2);
        auto add = [&](cplx p, cplx m, int k, vec2 screen) {
            Target t;
            t.p = p;
            t.m = m;
            t.k = k;
            t.d0 = std::ceil(VIEW_EFOLDS / std::log(std::abs(m)));
            t.screen = screen;
            fr.targets.push_back(t);
        };
        const vec2 L{-15.f, 1.f}, R{15.f, 1.f};
        const cplx la4 = std::pow(2.0 * alpha, 4), lb = 2.0 * beta, l2 = 4.0 * (c + 1.0);
        add(alpha, la4, 4, L);   // where four petals meet (alpha: f^4 nearly without a turn)
        add(-alpha, la4, 4, R);  // the same junction elsewhere (f maps it onto alpha)
        add(beta, lb, 1, R);     // the tip at the end of the external ray 0 (a slow spiral)
        add(-beta, lb, 1, L);
        add(q1, l2, 2, L);       // a point of the 2-cycle (two arms)
        // the attracting cycle (the petals' inside)
        cplx z = 0.0;
        for (int i = 0; i < 4000; i++) z = z * z + c;
        std::vector<cplx> cyc{z};
        for (cplx q = z * z + c; std::abs(q - z) > 1e-7 && cyc.size() < 8; q = q * q + c) cyc.push_back(q);
        fr.period = (int)cyc.size();
        size_t k0 = 0;
        for (size_t k = 1; k < cyc.size(); k++)
            if (std::abs(cyc[k]) < std::abs(cyc[k0])) k0 = k;
        double gain = 1.0;
        for (size_t j = 1; j < cyc.size(); j++) gain *= std::abs(2.0 * cyc[(k0 + j) % cyc.size()]);
        fr.sink[0] = (float)cyc[k0].real();
        fr.sink[1] = (float)cyc[k0].imag();
        fr.sinkGain = (float)gain;
        return fr;
    }();
    return F;
}

// A view of the complex plane: w = center + mul * (p - screen).
struct View {
    cplx center, mul;
    vec2 screen;
};
// The view of target t at depth t.d0 + frac, turned by spin.
View targetView(const Target& t, double frac, double spin) {
    const double depth = t.d0 + frac;
    const cplx mul = S0 * std::exp(-depth * std::log(t.m)) * std::polar(1.0, spin);
    return {t.p, mul, t.screen};
}

// The neighbours of a cell of the same piece, as the blocks' mask (1 left, 2 right, 4 up, 8 down, 16 up left, 32 up
// right, 64 down left, 128 down right; up on screen: the row above).
template <class F> int neighbourMask(int x, int y, F same) {
    const int d[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    int m = 0;
    for (int k = 0; k < 8; k++)
        if (same(x + d[k][0], y + d[k][1])) m |= 1 << k;
    return m;
}
// A piece's cell's mask, from the piece's four cells.
int pieceMask(const int (*k)[2], int x, int y) {
    return neighbourMask(x, y, [&](int a, int b) {
        for (int i = 0; i < 4; i++)
            if (k[i][0] == a && k[i][1] == b) return true;
        return false;
    });
}

class FractalUniverse : public Universe {
public:
    ~FractalUniverse() override;
    bool init(int width, int height) override;
    void resize(int width, int height) override;
    void setSong(const SongLook& look, float seconds) override;
    void setLevel(int level, float energy, float seconds, float impact) override;
    void refresh(float strength) override;
    void onEvent(const GameEvent& ev, const Game& game) override;
    void setBonus(bool on) override { slowOn_ = on; }
    void render(const Frame& f, GLuint fbo) override;
    vec2 project(const vec3& p) const override { return layout::project(vp_, p, width_, height_); }
    TransitionHints hints() const override;
    int variant() const override { return variant_; }
    bool hostText() const override { return true; }
    void hudLine(const HudText& h, text::Line& out) override;
    HudInk hudInk(const HudText& h) override;

private:
    // ---- Blocks.
    struct CellInst { float a[4], b[4], c[4]; };
    std::vector<CellInst> cells_;
    void cell(float x, float y, int type, float alpha, int mode, float scale, float k, int mask);
    void buildBoard(const Game& g);
    vec3 lineColor() const;
    struct Pop { float x, y; int type; float t; int mask; float cx, cy; };   // (cx, cy: its shape's center)
    std::vector<Pop> pops_;
    // each cell's piece (its lock's number * 8 + its type + 1; 0 empty, < 0 alone): the cells of a piece one shape
    int ids_[Game::H][Game::W] = {};
    int lockCount_ = 0;
    void syncCells(const Game& g);
    int gridMask(const Game& g, int x, int y) const;

    // ---- The HUD's plates (its lettering: the host's).
    float fade_ = 1.f;
    void drawHud(const std::vector<HudRect>& panels);

    // ---- GL.
    GLuint progBg_ = 0, progCell_ = 0, progSolid_ = 0;
    GLuint quadVbo_ = 0, cellVao_ = 0, cellVbo_ = 0;
    mat4 vp_;
    vec2 viewC_{0, 0}, viewH_{21, 11.9f};
    void solid(float r, float g, float b, float a);

    // ---- Song, palette, colors (sRGB, eased on a change of song).
    int variant_ = 0;
    vec3 col_[SLOTS];
    bool colSet_ = false;
    Rng rng_{0xF2AC7u};

    // ---- The zoom.
    int target_ = 0;
    double frac_ = 0.0;       // the depth past the target's D0 (levels), wrapped at 1 between glides
    double colOff_ = 0.0;     // the bands' count offset (iterations: K more at each wrap)
    double spin_ = 0.0;       // the flower's slow turn
    float burst_ = 0.f;       // the clears' zoom still to come (e-folds)
    float burstV_ = 0.f;      // its speed (e-folds a second)
    float dropV_ = 0.f;       // the drop's zoom speed (e-folds a second)
    float holdHalo_ = 0.f;    // the hold preview's halo, shown while a piece is held
    // a glide to another target: from the view it left (frozen), 0..1
    bool gliding_ = false;
    float glide_ = 0.f;
    View from_{};
    float sinceGlide_ = 0.f;
    void glideTo(int target);
    int otherTarget();

    // ---- Staging.
    int level_ = 1;
    float lv_ = 1.f;
    float dropMix_ = 0.f;
    bool slowOn_ = false;
    float slow_ = 0.f;
    float overT_ = -1.f, over_ = 0.f;
    float dimT_ = 0.f;
    float time_ = 0.f;
};

// =====================================================================================================
// Setup
// =====================================================================================================

bool FractalUniverse::init(int width, int height) {
    const std::string head = "#version 330 core\n";
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, (head + sh::PAINT_GLSL + sh::FRACTAL_FS).c_str(), "fractal julia");
    progCell_ = ugl::program(sh::CELL_VS, (head + sh::PAINT_GLSL + sh::CELL_FS).c_str(), "fractal blocks");
    progSolid_ = ugl::program(ugl::FULLSCREEN_VS, sh::SOLID_FS, "fractal solid");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glBindVertexArray(0);

    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void FractalUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
}

FractalUniverse::~FractalUniverse() {
    for (GLuint p : {progBg_, progCell_, progSolid_}) glDeleteProgram(p);
    glDeleteVertexArrays(1, &cellVao_);
    for (GLuint b : {quadVbo_, cellVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level, events
// =====================================================================================================

int FractalUniverse::otherTarget() {
    const int n = (int)fractal().targets.size();
    return (target_ + 1 + rng_.irange(0, n - 2)) % n;
}

// Glides from the view on screen to target t: out until both points are in view, across, and back in.
void FractalUniverse::glideTo(int t) {
    if (gliding_) return;   // (one glide at a time: it ends where it was going)
    if (std::getenv("REFALL_FRACTAL_LOG")) std::fprintf(stderr, "[fractal] glide to point %d\n", t);   // (REFALL_FRACTAL_LOG=1)
    from_ = targetView(fractal().targets[target_], frac_, spin_);
    target_ = t;
    gliding_ = true;
    glide_ = 0.f;
    sinceGlide_ = 0.f;
}

void FractalUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.26f) v = nearBlackVariant(h);   // a very dark one: near black
        else if (t.hasSong && t.minor && t.brightness < 0.42f) v = V_MIDNIGHT;       // a dark song: night
        else if (t.hasSong && !t.minor && t.brightness > 0.6f) v = V_SUNSHINE;       // a bright, major one
        else if (t.hasSong && t.airWeight > 0.6f) v = V_SAGE;                        // airy
        else {
            static const int BY_HUE[] = {V_SUNSHINE, V_APRICOT, V_RAINBOW, V_SAGE, V_APRICOT, V_RAINBOW};
            v = BY_HUE[(int)(h * 6.f) % 6];
        }
        if (v == t.avoidVariant) // (a near-black one to the other; else never to a near-black one)
            v = v >= V_OBSIDIAN ? (v == V_OBSIDIAN ? V_EMBERS : V_OBSIDIAN) : (v + 1 + (int)(t.seed % 3)) % (V_RAINBOW + 1);
    }
    variant_ = v;
    rng_ = Rng((t.seed ? t.seed : 1) ^ 0xF2AC7ull);
    const int n = (int)fractal().targets.size();
    if (seconds <= 0.f) {
        // the first song: straight in, at its own point
        colSet_ = false;
        target_ = rng_.irange(0, n - 1);
        // (test hook: REFALL_FRACTAL_TARGET=i starts at point i)
        if (const char* e = std::getenv("REFALL_FRACTAL_TARGET")) target_ = std::clamp(std::atoi(e), 0, n - 1);
        frac_ = 0.0;
        gliding_ = false;
    } else glideTo(otherTarget()); // a new song in the same universe: off to another point
}

void FractalUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)impact;
    level = std::clamp(level, 0, 2);
    // a change of section (not into a drop, which zooms in instead): now and then, a glide to another point
    if (level != level_ && level < 2 && seconds > 0.1f && !gliding_ && sinceGlide_ > 30.f) glideTo(otherTarget());
    level_ = level;
}

void FractalUniverse::refresh(float strength) {
    (void)strength;
    if (level_ < 2 && !gliding_ && sinceGlide_ > 50.f) glideTo(otherTarget());
}

TransitionHints FractalUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(lerp(hex(V.c[WASH]), hex(V.c[BAND0 + 2]), 0.35f));
    h.ink = ugl::toLinear(hex(V.c[darkVariant(variant_) ? HUD_PALE : HUD_DEEP]));
    h.accent = ugl::toLinear(hex(V.c[ACCENT]));
    h.light = !darkVariant(variant_);
    h.styles = TS_RIPPLE | TS_INK;
    return h;
}

void FractalUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock:
    case GameEvent::Fill: {
        // each cell keeps its piece (the cells of a bigger lock, a filled board, each alone)
        lockCount_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = ev.cells.size() <= 4 ? lockCount_ * 8 + std::clamp(c.type, 0, 6) + 1 : -1 - (c.y * Game::W + c.x);
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // a popping cell stays joined to the cells of its piece that pop with it: they shrink together, toward the
        // center of their shape
        auto popping = [&](int a, int b, int id) {
            return id > 0 && a >= 0 && a < Game::W && b >= 0 && b < Game::H && rows[b] && ids_[b][a] == id;
        };
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            const int m = neighbourMask(c.x, c.y, [&](int a, int b) { return popping(a, b, id); });
            // its shape: the cells it joins, step by step
            bool seen[Game::H][Game::W] = {};
            int q[Game::H * Game::W][2], n = 0;
            float sx = 0.f, sy = 0.f;
            q[n][0] = c.x; q[n][1] = c.y; n++;
            seen[c.y][c.x] = true;
            for (int i = 0; i < n; i++) {
                const int x = q[i][0], y = q[i][1];
                sx += x; sy += y;
                const int d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                for (auto& k : d)
                    if (popping(x + k[0], y + k[1], id) && !seen[y + k[1]][x + k[0]]) {
                        seen[y + k[1]][x + k[0]] = true;
                        q[n][0] = x + k[0]; q[n][1] = y + k[1]; n++;
                    }
            }
            const vec3 o = layout::cellPos(sx / n, sy / n);
            pops_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), 0.f, m, o.x, o.y});
        }
        // the pieces' ids follow their rows down
        int ni[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (rows[y]) continue;
            std::memcpy(ni[dst], ids_[y], sizeof(ids_[y]));
            dst--;
        }
        std::memcpy(ids_, ni, sizeof(ids_));
        // the zoom accelerates: a burst of about a level a line (more for a quad), eased in and out (render)
        burst_ += ZOOM_LINE + ZOOM_LINE_MORE * (std::min(ev.count, 4) - 1);
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// Board
// =====================================================================================================

void FractalUniverse::syncCells(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool full = g.cell(x, y).type >= 0;
            // (a cell of no known piece, or not the one locked there (the board filled anew): alone)
            if (full && (ids_[y][x] == 0 || (ids_[y][x] > 0 && ids_[y][x] % 8 != g.cell(x, y).type + 1))) ids_[y][x] = -1 - (y * Game::W + x);
            if (!full) ids_[y][x] = 0;
        }
}

// A stack cell's mask (a row still falling after a clear joins its piece's cells once it has landed).
int FractalUniverse::gridMask(const Game& g, int x, int y) const {
    const int id = ids_[y][x];
    if (id <= 0) return 0;
    return neighbourMask(x, y, [&](int a, int b) {
        return a >= 0 && a < Game::W && b >= 0 && b < Game::H && ids_[b][a] == id &&
               std::fabs(g.rowOffset(b) - g.rowOffset(y)) < 1e-3f;
    });
}

void FractalUniverse::cell(float x, float y, int type, float alpha, int mode, float scale, float k, int mask) {
    if (alpha <= 0.003f || scale <= 0.f) return;
    const vec3 c = col_[PIECE0 + std::clamp(type, 0, 6)];
    cells_.push_back({{x, y, 0.5f, 0.f}, {(float)mask, alpha, (float)mode, scale}, {c.x, c.y, c.z, k}});
}

void FractalUniverse::buildBoard(const Game& g) {
    syncCells(g);
    const float overRows = over_ * 26.f;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            float k = -0.3f * std::pow(saturate(c.flash), 8.f);
            if (over_ > 0.f) k = 0.7f * saturate(overRows - (y - Game::HIDDEN));
            cell(p.x, p.y, c.type, 1.f, 0, 1.f, k, gridMask(g, x, y));
        }
    for (const Pop& p : pops_) {
        const float t = saturate(p.t / CLEAR_T), s = 1.f - 0.6f * ease(t);
        cell(p.cx + (p.x - p.cx) * s, p.cy + (p.y - p.cy) * s, p.type, 1.f - ease((t - 0.5f) / 0.5f), 0, s,
             -0.35f * (1.f - t), p.mask);
    }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cs[4][2];
        g.pieceCells(p, cs);
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, p.type, 1.f, 1, 1.f, 0.f, pieceMask(gc, c[0], c[1]));
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cs) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, p.type, 1.f, 0, 1.f, 0.f, pieceMask(cs, c[0], c[1]));
        }
    }
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cs[4][2];
        g.pieceCells(p, cs);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cs) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cs)
            cell(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc, type,
                 alpha, 0, sc, 0.f, pieceMask(cs, c[0], c[1]));
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// The lines between bands: the palette's, or in the drop (a near-black palette) cycling through its drop lines.
vec3 FractalUniverse::lineColor() const {
    const VariantDef& V = VARIANTS[variant_];
    const float m = ease(dropMix_);
    if (!V.dropLine[0] || m <= 0.f) return col_[LINE];
    const float t = std::fmod(time_ / V.dropCycle, 3.f);
    const int i = (int)t;
    const vec3 c = lerp(hex(V.dropLine[i]), hex(V.dropLine[(i + 1) % 3]), ease(t - i));
    return lerp(col_[LINE], c, m);
}

void FractalUniverse::solid(float r, float g, float b, float a) {
    if (a <= 0.002f) return;
    glUseProgram(progSolid_);
    set4f(progSolid_, "uColor", r * a, g * a, b * a, a);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Text
// =====================================================================================================

// The HUD's plates: plain rectangles in the well's color (its lettering: the host's).
void FractalUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    glUseProgram(progSolid_);
    glEnable(GL_SCISSOR_TEST);
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? r.color : r.accent ? col_[ACCENT] : col_[WELL];
        glScissor((GLint)r.x, (GLint)(height_ - r.y - r.h), (GLsizei)std::max(0.f, r.w), (GLsizei)std::max(0.f, r.h));
        solid(c.x, c.y, c.z, std::min(1.f, r.alpha));
    }
    glDisable(GL_SCISSOR_TEST);
}

// The unified HUD's inks: a warm cream, the palette's deep tone, its accent.
HudInk FractalUniverse::hudInk(const HudText& h) {
    (void)h;
    HudInk k;
    k.pale = col_[HUD_PALE];
    k.deep = col_[HUD_DEEP];
    k.accent = col_[ACCENT];
    return k;
}

// The universe's own lettering (REFALL_HUD=universe): the deep ink on the pastel bands (cream on the dark ones), a soft
// halo; Comfortaa for the labels and numbers, Fraunces' soft italics for the song, the notices and the toasts.
void FractalUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* disp = text::font(FONT_DISPLAY);
    text::Font* round = text::font(FONT_ROUND);
    const vec3 ink = darkVariant(variant_) ? col_[HUD_PALE] : col_[HUD_DEEP];
    const vec3 acc = col_[ACCENT];
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        if (!var.empty()) {
            size_t n = 1;
            while (n < var.size() && ((unsigned char)var[n] & 0xC0) == 0x80) n++;
            var = var.substr(0, n) + text::lower(var.substr(n));
        }
        const float sz = base * 0.95f, vsz = base * 1.3f, gap = base * 0.7f, a = saturate(0.35f + h.alpha * 1.6f);
        const float vdy = -sz * 0.22f + text::capHeight(disp) * vsz - text::capHeight(round) * sz;
        out.add(name, round, sz, ink, 0.18f, a);
        if (!var.empty()) {
            text::Span& v = out.add(var, disp, vsz, acc, 0.f, a);
            v.dy = vdy;
            v.gap = (gap - 0.18f * sz) / vsz;
        }
        break;
    }
    case HUD_SECTION: put(round, h.text, base * 0.9f, base * 0.3f, acc, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(disp, h.text, base * 1.7f, 0.f, ink, h.alpha * 1.3f, -base * 0.35f); break;
    case HUD_INFO: put(round, h.text, base * 0.7f, base * 0.08f, ink, alpha * 1.3f); break;
    case HUD_LABEL: put(round, h.text, base * 0.8f, base * 0.28f, acc, alpha * 1.2f); break;
    case HUD_VALUE: put(round, h.text, base * 1.1f, base * 0.04f, ink, alpha); break;
    case HUD_NOTICE: put(disp, h.text, base * 1.6f, 0.f, acc, alpha); break;
    case HUD_KEY: put(round, h.text, base * 0.75f, base * 0.06f, ink, alpha); break;
    case HUD_TOAST: put(disp, h.text, base * 1.6f, base * 0.06f, acc, alpha, -base * 0.3f); break;
    case HUD_TOAST_SUB: put(round, h.text, base * 1.1f, base * 0.1f, ink, alpha * 0.95f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * height_ / 1080.f) put(disp, h.text, base * 1.3f, 0.f, ink, alpha, -base * 0.1f);
        else put(round, h.text, base * 0.85f, base * 0.1f, h.accent ? acc : ink, alpha);
        break;
    default: put(round, h.text, base * 0.85f, base * 0.08f, h.accent ? acc : ink, alpha); break;
    }
    out.look.halo = darkVariant(variant_) ? col_[WELL] : col_[HUD_PALE];
    out.look.haloA = 0.6f;
    out.look.haloW = 0.14f;
    out.alpha = saturate(fade_);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void FractalUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);

    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 2.5f) : 0.f, overT_ >= 0.f ? 3.f : 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += wdt;

    // ---- The colors: the palette's, eased in on a change of song.
    {
        const VariantDef& V = VARIANTS[variant_];
        const float k = colSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        colSet_ = true;
        for (int i = 0; i < SLOTS; i++) col_[i] = lerp(col_[i], hex(V.c[i]), k);
    }

    // ---- Staging: the level eased, the drop (music.inDrop only).
    lv_ = approach(lv_, (float)level_, 0.5f, dt);
    const float wm = smoothstepf(0.2f, 1.f, lv_);
    const bool inDrop = music.inDrop && overT_ < 0.f;
    dropMix_ = approach(dropMix_, inDrop ? 1.f : 0.f, inDrop ? 3.f : 0.6f, dt);   // the drop's palette, in ~1 s

    // ---- The zoom (e-folds a second): always a little; the clears' bursts eased in and out; the drop's steady pace.
    dropV_ = approach(dropV_, inDrop ? ZOOM_DROP : 0.f, inDrop ? 0.8f : 0.5f, dt);
    burstV_ = approach(burstV_, burst_ * 1.6f, 3.f, wdt);
    const float burstStep = std::min(burst_, burstV_ * wdt);
    burst_ -= burstStep;
    const float efolds = (ZOOM_CALM * (1.f + 0.5f * wm) + dropV_) * wdt + burstStep;
    spin_ += wdt * (0.01 + 0.004 * wm);
    sinceGlide_ += wdt;
    const Fractal& F = fractal();
    const Target& tg = F.targets[target_];
    frac_ += efolds / std::log(std::abs(tg.m));
    View view;
    if (gliding_) {
        // out until both points are in view, across, back in (no wrap meanwhile: the colors stay continuous)
        glide_ = std::min(1.f, glide_ + wdt / 9.f);
        const View to = targetView(tg, frac_, spin_);
        const double la = std::log(std::abs(from_.mul)), lb = std::log(std::abs(to.mul));
        const double out = std::max({la, lb, std::log(std::max(std::abs(to.center - from_.center), 1e-3) / 9.0)});
        const double s = glide_;
        double lm;
        double pan = 0.0;
        if (s < 0.35) lm = la + (out - la) * ease((float)(s / 0.35));
        else if (s < 0.65) { lm = out; pan = ease((float)((s - 0.35) / 0.3)); }
        else { lm = out + (lb - out) * ease((float)((s - 0.65) / 0.35)); pan = 1.0; }
        double aa = std::arg(from_.mul), ab = std::arg(to.mul);
        ab = aa + std::remainder(ab - aa, 2.0 * 3.14159265358979);
        view.center = from_.center + (to.center - from_.center) * pan;
        view.mul = std::polar(std::exp(lm), aa + (ab - aa) * ease((float)s));
        view.screen = vec2{lerpf(from_.screen.x, to.screen.x, (float)pan), lerpf(from_.screen.y, to.screen.y, (float)pan)};
        if (glide_ >= 1.f) gliding_ = false;
    }
    if (!gliding_) {
        // a level deeper is the same picture: wrap, and count its iterations into the bands
        while (frac_ >= 1.0) {
            frac_ -= 1.0;
            colOff_ += tg.k;
        }
        colOff_ = std::fmod(colOff_, 12.0);   // (the gradient's period: 6 stops, half a stop a band)
        view = targetView(tg, frac_, spin_);
    }
    // ---- The scene's setup and teardown: the camera dives from the whole flower down to its point, and pulls back
    // out to the whole flower on the paper after the song.
    {
        const float away = std::max(1.f - smoothstepf(0.f, 0.8f, f.setup), smoothstepf(0.f, 1.f, f.teardown));
        if (away > 0.f) {
            const double la = std::log(std::abs(view.mul)), lo = std::max(la, std::log(0.15));
            view.mul *= std::exp((lo - la) * ease(away));
        }
    }

    // ---- Pops.
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > CLEAR_T; }), pops_.end());

    // =========================================================== the fractal
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        glUseProgram(progBg_);
        set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        float bands[36];
        for (int i = 0; i < 12; i++) {
            bands[i * 3] = col_[BAND0 + i].x;
            bands[i * 3 + 1] = col_[BAND0 + i].y;
            bands[i * 3 + 2] = col_[BAND0 + i].z;
        }
        glUniform3fv(ugl::U(progBg_, "uBand"), 6, bands);
        glUniform3fv(ugl::U(progBg_, "uDropBand"), 6, bands + 18);
        set1f(progBg_, "uDropMix", ease(dropMix_));
        const vec3 line = lineColor();
        set3f(progBg_, "uLine", line);
        set3f(progBg_, "uInside", line * (VARIANTS[variant_].inside > 0.f ? VARIANTS[variant_].inside : 1.f));
        set3f(progBg_, "uWash", col_[WASH]);
        set3f(progBg_, "uShade", col_[SHADE]);
        set3f(progBg_, "uWellFill", col_[WELL]);
        set3f(progBg_, "uWellLine", col_[WELL_LINE]);
        set2f(progBg_, "uCenter", view.screen.x, view.screen.y);
        set2f(progBg_, "uC", (float)JULIA_C.real(), (float)JULIA_C.imag());
        set2f(progBg_, "uFix", (float)view.center.real(), (float)view.center.imag());
        {
            // the view center's exact orbit (in double) for the shader's perturbation
            float ref[130];
            cplx z = view.center;
            for (int i = 0; i <= 64; i++) {
                ref[i * 2] = (float)z.real();
                ref[i * 2 + 1] = (float)z.imag();
                z = z * z + JULIA_C;
            }
            glUniform2fv(ugl::U(progBg_, "uRef"), 65, ref);
            glUniform1i(ugl::U(progBg_, "uRefN"), 64);
        }
        set2f(progBg_, "uMul", (float)view.mul.real(), (float)view.mul.imag());
        set1f(progBg_, "uColOff", (float)colOff_);
        set2f(progBg_, "uSink", F.sink[0], F.sink[1]);
        set1f(progBg_, "uSinkGain", F.sinkGain);
        glUniform1i(ugl::U(progBg_, "uPeriod"), F.period);
        set1f(progBg_, "uDesat", 0.45f * slow_ + 0.2f * over_);
        set1f(progBg_, "uGrain", darkVariant(variant_) ? 0.045f : 0.022f);
        set4f(progBg_, "uWell", 5.15f, 10.15f, 0.3f, f.bare ? 0.f : 1.f);
        {
            float pv[16];
            const vec3 h = layout::HOLD_CENTER;
            // (the hold's halo only while a piece is held, fading in and out)
            holdHalo_ = approach(holdHalo_, game.holdType() >= 0 ? 1.f : 0.f, 4.f, dt);
            pv[0] = h.x; pv[1] = h.y; pv[2] = 1.15f; pv[3] = holdHalo_;
            for (int i = 0; i < 3; i++) {
                const vec3 c = layout::nextCenter(i);
                pv[4 + i * 4] = c.x; pv[5 + i * 4] = c.y; pv[6 + i * 4] = i == 0 ? 1.15f : 0.95f; pv[7 + i * 4] = 1.f;
            }
            glUniform4fv(ugl::U(progBg_, "uPrev"), 4, pv);
        }
        // REFALL_GPUTIME: the fractal's GPU time, printed every 120 frames
        static const bool gpuTime = std::getenv("REFALL_GPUTIME") != nullptr;
        static GLuint query = 0;
        static int queries = 0;
        static double queryMs = 0.0;
        if (gpuTime && !query) glGenQueries(1, &query);
        if (query) glBeginQuery(GL_TIME_ELAPSED, query);
        ugl::drawFullscreen();
        if (query) {
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            queryMs += ns / 1e6;
            if (++queries % 120 == 0) {
                std::fprintf(stderr, "[fractal] gpu %.2f ms/frame (%dx%d)\n", queryMs / 120.0, width_, height_);
                queryMs = 0.0;
            }
        }
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (!f.bare) {
        // =========================================================== the blocks
        cells_.clear();
        buildBoard(game);
        if (!cells_.empty()) {
            glUseProgram(progCell_);
            ugl::setMat(progCell_, "uVP", vp_);
            set3f(progCell_, "uShine", vec3(1.f, 0.98f, 0.94f));
            set1f(progCell_, "uGap", 0.06f);
            set1f(progCell_, "uLine", 0.05f);
            glBindVertexArray(cellVao_);
            glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
            glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        }
    }
    solid(0.f, 0.f, 0.f, 0.4f * dimT_);
    drawHud(f.panels);
    fade_ = f.fade;
    solid(0.f, 0.f, 0.f, 1.f - saturate(f.fade));
    glDisable(GL_BLEND);
}

} // namespace

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_fractal() {
    static const UniverseEntry e{
        "FRACTAL", "an infinite pastel flower: a Julia set zoomed for ever, its contour revealing finer petals",
        [] { return std::unique_ptr<Universe>(new FractalUniverse()); },
        // Bright, happy, easy songs suit it best.
        [](const SongLook& l) { return 0.65f + 0.35f * l.brightness + (l.minor ? 0.f : 0.15f) + 0.1f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

// The journey styles: five variations on COSMIC (the 12 stages are places on a journey through space, each with its
// five songs as satellites, the focus gliding between them), each with its own identity:
//   GALAXY     a 3D spiral galaxy seen at an angle, the stages star systems along its arm, the camera flying along;
//   ORRERY     a solar system, the stages planets around a big sun (locked ones dark and frozen), songs as moons;
//   ATLAS      an antique celestial chart, the stages constellations drawn in gold, the songs their stars;
//   SYNTHWAVE  an outrun cosmos over a neon grid, retro ringed planets, a ship hopping along a neon route;
//   PASTEL     a candy cosmos, round kawaii planets (sleeping while locked) with bubbly moons, all bobbing gently.
// Same screens, logic and navigation as every style: only the look differs.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include "library.hpp"
#include "menu.hpp"

namespace {

using F = Painter::Font;
const float PI = 3.14159265f, TAU = 6.2831853f;

struct V3 {
    float x = 0, y = 0, z = 0;
};
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 norm(V3 a) { const float l = std::sqrt(dot(a, a)); return l > 1e-6f ? a * (1.f / l) : a; }
V3 lerp3(V3 a, V3 b, float t) { return a + (b - a) * t; }

// A pinhole camera looking at a target: world -> screen pixels, with the pixels per world unit at that depth.
struct Cam {
    V3 pos, r, up, f;
    float F = 1000, cx = 0, cy = 0;
    void look(V3 from, V3 at) {
        pos = from;
        f = norm(at - from);
        r = norm(cross(f, V3{0, 1, 0}));
        up = cross(r, f);
    }
    bool project(V3 w, float& sx, float& sy, float& scale, float* depth = nullptr) const {
        const V3 v = w - pos;
        const float z = dot(v, f);
        if (z < 0.2f) return false;
        sx = cx + dot(v, r) / z * F;
        sy = cy - dot(v, up) / z * F;
        scale = F / z;
        if (depth) *depth = z;
        return true;
    }
};

float near1(float a, int i) { return std::max(0.f, 1.f - std::fabs(a - (float)i)); }
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
std::string two(int n) { char b[16]; std::snprintf(b, sizeof(b), "%02d", n); return b; }
std::string roman(int n) {
    static const char* R[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV"};
    return n >= 1 && n <= 15 ? R[n - 1] : std::to_string(n);
}
std::string titleCase(const char* s) {
    std::string r = s;
    for (size_t i = 1; i < r.size(); i++) r[i] = (char)std::tolower((unsigned char)r[i]);
    return r;
}
std::string fmt1(float v) { char b[16]; std::snprintf(b, sizeof(b), "%.1f", v); return b; }
Col pastelize(Col c, float k = 0.4f) { return mixc(c, hex(0xffffff, c.a), k); }
Col neonize(Col c) { // full saturation, bright
    const float m = std::max({c.r, c.g, c.b}), n = std::min({c.r, c.g, c.b});
    if (m - n < 1e-3f) return c;
    auto f = [&](float v) { return std::clamp((v - n) / (m - n) * 1.05f, 0.f, 1.f) * 0.85f + 0.15f; };
    return {f(c.r), f(c.g), f(c.b), c.a};
}

// Constellation figures: 5 stars (the songs, in order) and the lines between them, in a unit box.
struct Figure { float p[5][2]; int e[6][2]; int ne; };
const Figure FIGS[12] = {
    {{{-1, .3f}, {-.4f, 0}, {.2f, -.2f}, {.8f, -.6f}, {.5f, .45f}}, {{0, 1}, {1, 2}, {2, 3}, {2, 4}}, 4},
    {{{-1, .4f}, {-.5f, -.4f}, {0, .2f}, {.5f, -.5f}, {1, .3f}}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 4},
    {{{0, -1}, {-.6f, 0}, {0, .3f}, {.6f, -.1f}, {.1f, 1}}, {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {2, 4}}, 5},
    {{{-.8f, -.6f}, {-.6f, .5f}, {.6f, .5f}, {.8f, -.6f}, {0, 1}}, {{0, 1}, {1, 2}, {2, 3}, {1, 4}, {2, 4}}, 5},
    {{{-1, 0}, {-.5f, -.5f}, {0, 0}, {.5f, .5f}, {1, 0}}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 4},
    {{{-1, -.4f}, {-.3f, 0}, {0, -.7f}, {.4f, .1f}, {1, -.3f}}, {{0, 1}, {1, 2}, {1, 3}, {3, 4}}, 4},
    {{{-.5f, -.9f}, {-.7f, .2f}, {0, .8f}, {.7f, .2f}, {.5f, -.9f}}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 4},
    {{{-1, .2f}, {0, -.6f}, {1, .2f}, {0, .2f}, {0, .9f}}, {{0, 1}, {1, 2}, {1, 3}, {3, 4}}, 4},
    {{{-1, 0}, {-.3f, -.5f}, {.5f, 0}, {-.3f, .5f}, {1, .1f}}, {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {2, 4}}, 5},
    {{{0, -1}, {-.4f, -.2f}, {.4f, -.2f}, {-.3f, .8f}, {.3f, .8f}}, {{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 4}}, 5},
    {{{-1, -.8f}, {-.4f, -.3f}, {.1f, .1f}, {.6f, .4f}, {1, .9f}}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 4},
    {{{-1, .2f}, {-.4f, -.4f}, {.4f, -.3f}, {1, -.6f}, {.2f, .5f}}, {{0, 1}, {1, 2}, {2, 3}, {2, 4}, {4, 0}}, 5},
};

// The galaxy's stars and dust, generated once.
struct Dust { float th, r, y; Col c; float size; bool big; };
const std::vector<Dust>& galaxyDust() {
    static std::vector<Dust> d;
    if (!d.empty()) return d;
    uint32_t s = 12345;
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.f; };
    for (int i = 0; i < 700; i++) { // the bulge
        const float r = 2.6f * std::pow(rnd(), 1.6f);
        d.push_back({rnd() * TAU, r, (rnd() - 0.5f) * 0.5f * (1.f - r / 2.8f), mixc(hex(0xffd9a0), hex(0xffffff), rnd() * 0.5f),
                     0.6f + rnd(), i % 9 == 0});
    }
    for (int arm = 0; arm < 2; arm++)
        for (int i = 0; i < 2600; i++) {
            const float th = rnd() * 8.6f;
            const float r = 1.56f + 1.48f * th + (rnd() - 0.5f) * 1.6f * (0.6f + 0.08f * th);
            const float q = rnd();
            Col c = q < 0.09f ? hex(0xff7ac8) : q < 0.14f ? hex(0xffd27a) : mixc(hex(0x8fb4ff), hex(0xe8f0ff), rnd());
            d.push_back({th + arm * PI + (rnd() - 0.5f) * 0.3f, r, (rnd() - 0.5f) * 0.22f, c, 0.5f + rnd() * 1.1f, i % 5 == 0});
        }
    return d;
}

} // namespace

struct Cosmos {
    Menu& m;
    Painter& p;
    float u, t;
    int kind; // 0 galaxy, 1 orrery, 2 atlas, 3 synthwave, 4 pastel
    struct Theme {
        F head, body, label;
        Col ink, dim, accent, accent2, gold;
        float track;
        bool upper;
    } th;

    explicit Cosmos(Menu& mm) : m(mm), p(mm.paint_), u(mm.u_), t(mm.time_), kind(mm.style_ - Menu::GALAXY) {
        static const Theme T[5] = {
            {F::JOSEFIN_LIGHT, F::JOSEFIN, F::JOSEFIN, hex(0xffffff), hex(0xc8d2ff, 0.6f), hex(0x8fe9ff), hex(0xff8ad8), hex(0xffd76a), 0.3f, true},
            {F::JURA_LIGHT, F::JURA, F::JURA, hex(0xfff4e0), hex(0xf0d8b0, 0.6f), hex(0xffb35c), hex(0x8fd0ff), hex(0xffd76a), 0.25f, true},
            {F::CORMORANT_ITALIC, F::MARCELLUS, F::MARCELLUS, hex(0xf3dfa8), hex(0xcdb98a, 0.65f), hex(0xe6c06a), hex(0x9fc3e8), hex(0xffd98a), 0.12f, false},
            {F::INTER_BOLD, F::MANROPE, F::MANROPE, hex(0xffffff), hex(0xe0c8ff, 0.65f), hex(0x3ff0ff), hex(0xff3fb4), hex(0xffe066), 0.15f, true},
            {F::COMFORTAA, F::QUICKSAND, F::QUICKSAND, hex(0x5b4a7a), hex(0x7a6a96, 0.75f), hex(0xff7eb6), hex(0x7ab8ff), hex(0xffb347), 0.02f, false},
        };
        th = T[kind];
    }
    float X(float v) const { return m.X(v); }
    float Y(float v) const { return m.Y(v); }
    std::string cs(const std::string& s) const { // the theme's case
        if (th.upper) { std::string r = s; for (char& c : r) c = (char)std::toupper((unsigned char)c); return r; }
        return s;
    }
    Col stageCol(int s) const { unsigned char c[3]; m.stageColor(s, c); return rgb8(c); }
    Col songCol(int s, int k) const {
        unsigned char c[3];
        menuUniverseColor(m.camp_, m.camp_->stages()[s].songs[k].universe, c);
        return rgb8(c);
    }
    bool open(int s) const { return !m.progress_ || m.progress_->unlocked(s); }
    bool beaten(int s, int k) const { return m.progress_ && m.progress_->beaten(s, k, m.diff_); }
    bool cleared(int s) const { return m.progress_ && m.progress_->cleared(s, m.diff_); }
    int nStages() const { return m.camp_ ? m.camp_->size() : 0; }
    int nSongs(int s) const { return (int)m.camp_->stages()[s].songs.size(); }
    float focusStage(int s) const { return m.aRow_ > 0.5f ? sstep(near1(m.aStage_, s)) * std::min(1.f, m.aRow_) : 0.f; }
    // the focus glides: a point between the songs around the eased cursor
    template <class Pos> void focusPoint(Pos pos, float& fx, float& fy) const {
        const int n = nStages();
        const int s0 = std::clamp((int)std::floor(m.aStage_), 0, n - 1), s1 = std::min(n - 1, s0 + 1);
        const int k0 = std::clamp((int)std::floor(m.aSong_), 0, 4), k1 = std::min(4, k0 + 1);
        const float fs = std::clamp(m.aStage_ - s0, 0.f, 1.f), fk = std::clamp(m.aSong_ - k0, 0.f, 1.f);
        float ax, ay, bx, by, cx, cy, dx, dy;
        pos(s0, k0, ax, ay); pos(s0, k1, bx, by); pos(s1, k0, cx, cy); pos(s1, k1, dx, dy);
        fx = lerpf(lerpf(ax, bx, fk), lerpf(cx, dx, fk), fs);
        fy = lerpf(lerpf(ay, by, fk), lerpf(cy, dy, fk), fs);
    }

    void render();
    // screens
    void titleScreen();
    void freeScreen();
    void campaignScreen();
    // shared pieces
    void tabs();
    void info();
    void menuList(const char* const* items, int n, float a, float y0, float step, float size);
    void hint(const std::string& s, float y);
    // worlds
    void galaxy(bool titleMode);
    void orrery();
    void atlas();
    void synth();
    void pastel();
    // single things
    void kawaii(float cx, float cy, float R, Col c, int seed, bool awake, float focus, bool happy);
    void retroPlanet(float cx, float cy, float R, Col c, Col ring, int seed, bool lit);
    void star4(float cx, float cy, float s, Col c, float glow);
    void ship(float x, float y, float s, float heading);
    void constellation(float cx, float cy, float sc, int fig, int s, float focus, bool isOpen, bool demo, int demoBeaten);
};

void Menu::renderCosmos() {
    Cosmos c(*this);
    c.render();
}

void Cosmos::render() {
    // the camera pan of the flat styles, also handed to the backgrounds for their parallax
    float camX = 0, camY = 0;
    if (m.screen_ == Menu::CAMPAIGN && (kind == 2 || kind == 3 || kind == 4)) camX = (m.aStage_ * (kind == 2 ? 420.f : kind == 3 ? 470.f : 430.f)) * u / m.fh_;
    if (m.screen_ == Menu::CAMPAIGN && kind == 0) camX = m.aStage_ * 0.08f, camY = m.aStage_ * 0.02f;
    if (m.screen_ == Menu::CAMPAIGN && kind == 1) camX = m.aStage_ * 0.05f;
    if (m.screen_ != Menu::CAMPAIGN) camX = std::sin(t * 0.05f) * 0.2f;
    static const int BG[5] = {Painter::BG_GALAXY, Painter::BG_ORRERY, Painter::BG_ATLAS, Painter::BG_SYNTH, Painter::BG_PASTEL};
    p.background(BG[kind], t, m.level_, camX, camY);
    if (m.screen_ == Menu::TITLE) titleScreen();
    else if (m.screen_ == Menu::FREE) freeScreen();
    else campaignScreen();
}

// ---------------------------------------------------------------------------------------------------------------------
// shared pieces

void Cosmos::hint(const std::string& s, float y) { p.text(th.body, cs(s), X(960), Y(y), 17 * u, th.dim, 0, th.track); }

void Cosmos::menuList(const char* const* items, int n, float a, float y0, float step, float size) {
    const float cx = X(960);
    float wsel = 0;
    for (int i = 0; i < n; i++) wsel += near1(a, i) * p.width(th.body, cs(items[i]), size * u, th.track);
    const float ys = Y(y0 + a * step);
    const float pulse = 0.85f + 0.15f * std::sin(t * 2.2f);
    if (kind == 0 || kind == 1 || kind == 3) // a soft shade keeps the items readable over the busy worlds
        p.rrect(cx - 330 * u, Y(y0) - size * 1.9f * u, 660 * u, step * (n - 1) * u + size * 2.8f * u, 70 * u, hex(0x05030f, 0.5f), 80 * u);
    switch (kind) {
    case 0:
        p.setAdditive(true);
        p.line(cx - wsel / 2, ys + 18 * u, cx + wsel / 2, ys + 18 * u, 1.6f * u, withA(th.accent, 0.9f));
        p.glow(cx - wsel / 2 - 36 * u, ys - size * 0.35f * u, 34 * u, withA(th.accent2, 0.7f * pulse));
        p.glow(cx + wsel / 2 + 36 * u, ys - size * 0.35f * u, 34 * u, withA(th.accent, 0.7f * pulse));
        p.setAdditive(false);
        break;
    case 1: { // brass brackets
        const float h = size * 1.2f * u, x0 = cx - wsel / 2 - 34 * u, x1 = cx + wsel / 2 + 34 * u, yc = ys - size * 0.35f * u;
        for (int sgn : {-1, 1}) {
            const float x = sgn < 0 ? x0 : x1;
            p.line(x, yc - h / 2, x, yc + h / 2, 2 * u, withA(th.accent, 0.9f));
            p.line(x, yc - h / 2, x - sgn * 12 * u, yc - h / 2, 2 * u, withA(th.accent, 0.9f));
            p.line(x, yc + h / 2, x - sgn * 12 * u, yc + h / 2, 2 * u, withA(th.accent, 0.9f));
        }
        p.setAdditive(true);
        p.glow(cx, yc, wsel * 0.6f, hex(0xff9a40, 0.18f));
        p.setAdditive(false);
        break;
    }
    case 2:
        star4(cx - wsel / 2 - 38 * u, ys - size * 0.3f * u, 16 * u, th.gold, pulse);
        star4(cx + wsel / 2 + 38 * u, ys - size * 0.3f * u, 16 * u, th.gold, pulse);
        p.line(cx - wsel / 2, ys + 14 * u, cx + wsel / 2, ys + 14 * u, 1.2f * u, withA(th.gold, 0.7f));
        break;
    case 3: {
        const float w = wsel + 90 * u, h = size * 1.55f * u, y = ys - size * 0.36f * u - h / 2;
        p.setAdditive(true);
        p.rrectLine(cx - w / 2, y, w, h, h / 2, 6 * u, withA(th.accent2, 0.35f), 16 * u);
        p.setAdditive(false);
        p.rrect(cx - w / 2, y, w, h, h / 2, hex(0xff3fb4, 0.18f));
        p.rrectLine(cx - w / 2, y, w, h, h / 2, 2.2f * u, withA(th.accent2, 0.95f));
        break;
    }
    default: {
        const float w = wsel + 80 * u, h = size * 1.6f * u, y = ys - size * 0.36f * u - h / 2;
        p.rrect(cx - w / 2, y + 6 * u, w, h, h / 2, hex(0xd77aa8, 0.25f), 14 * u);
        p.rrect(cx - w / 2, y, w, h, h / 2, hex(0xffffff, 0.9f));
        p.circle(cx - w / 2 + h * 0.5f, y + h * 0.5f, h * 0.16f, withA(th.accent, 0.9f));
        p.circle(cx + w / 2 - h * 0.5f, y + h * 0.5f, h * 0.16f, withA(th.accent2, 0.9f));
        break;
    }
    }
    for (int i = 0; i < n; i++) {
        const float f = near1(a, i);
        Col c = withA(th.ink, 0.45f + 0.55f * f);
        if (kind == 3 && f > 0.5f) c = hex(0xffffff);
        p.text(th.body, cs(items[i]), cx, Y(y0 + i * step), size * u, c, 0, th.track);
    }
}

void Cosmos::tabs() {
    p.text(th.body, cs("Campaign"), X(80), Y(92), 24 * u, withA(th.ink, 0.8f), -1, th.track + 0.2f);
    const float cx = X(1180), sp = 220 * u, y = Y(90), on = 1.f - std::min(1.f, m.aRow_);
    const float hx = cx + (m.aDiff_ - 1) * sp;
    const float w = 170 * u, h = 46 * u;
    switch (kind) {
    case 3:
        p.rrect(hx - w / 2, y - h * 0.68f, w, h, h / 2, hex(0xff3fb4, 0.22f));
        p.setAdditive(true);
        p.rrectLine(hx - w / 2, y - h * 0.68f, w, h, h / 2, 5 * u, withA(on > 0.5f ? th.accent : th.accent2, 0.3f + 0.3f * on), 14 * u);
        p.setAdditive(false);
        p.rrectLine(hx - w / 2, y - h * 0.68f, w, h, h / 2, 2 * u, withA(on > 0.5f ? th.accent : th.accent2, 0.9f));
        break;
    case 4:
        p.rrect(hx - w / 2, y - h * 0.68f + 4 * u, w, h, h / 2, hex(0xd77aa8, 0.25f), 10 * u);
        p.rrect(hx - w / 2, y - h * 0.68f, w, h, h / 2, on > 0.5f ? hex(0xff9ccb) : hex(0xffffff, 0.92f));
        break;
    case 2:
        star4(hx, y + 16 * u, 9 * u, th.gold, 0.6f + 0.4f * on);
        p.line(hx - 60 * u, y + 16 * u, hx - 16 * u, y + 16 * u, 1.2f * u, withA(th.gold, 0.8f));
        p.line(hx + 16 * u, y + 16 * u, hx + 60 * u, y + 16 * u, 1.2f * u, withA(th.gold, 0.8f));
        break;
    default:
        p.setAdditive(true);
        p.line(hx - 60 * u, y + 16 * u, hx + 60 * u, y + 16 * u, (1.6f + on) * u, withA(th.accent, 0.55f + 0.45f * on));
        if (on > 0.05f) p.glow(hx, y + 16 * u, 90 * u, withA(th.accent, 0.3f * on));
        p.setAdditive(false);
        break;
    }
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = near1(m.aDiff_, d);
        Col c = withA(th.ink, 0.45f + 0.55f * f);
        if (kind == 4 && f > 0.5f && on > 0.5f) c = hex(0xffffff);
        p.text(th.body, cs(titleCase(difficultyName(d)).c_str()), cx + (d - 1) * sp, y, 23 * u, c, 0, th.track);
    }
}

void Cosmos::info() {
    const float x0 = X(60), y0 = Y(838), w = 1800 * u, h = 212 * u;
    switch (kind) {
    case 0: case 1:
        p.rrect(x0 - 60 * u, y0 + 10 * u, w + 120 * u, h + 80 * u, 0, hex(0x000000, 0.55f), 60 * u);
        p.line(x0, y0, x0 + w, y0, 1 * u, withA(th.accent, 0.25f));
        break;
    case 2:
        p.rrect(x0, y0, w, h, 6 * u, hex(0x081226, 0.82f));
        p.rrectLine(x0, y0, w, h, 6 * u, 1.5f * u, withA(th.gold, 0.7f));
        p.rrectLine(x0 + 7 * u, y0 + 7 * u, w - 14 * u, h - 14 * u, 4 * u, 0.8f * u, withA(th.gold, 0.4f));
        for (float cx : {x0 + 7 * u, x0 + w - 7 * u})
            for (float cy : {y0 + 7 * u, y0 + h - 7 * u}) p.circle(cx, cy, 4 * u, th.gold);
        break;
    case 3:
        p.rrect(x0, y0, w, h, 16 * u, hex(0x12002a, 0.78f));
        p.setAdditive(true);
        p.rrectLine(x0, y0, w, h, 16 * u, 5 * u, withA(th.accent, 0.25f), 16 * u);
        p.setAdditive(false);
        p.rrectLine(x0, y0, w, h, 16 * u, 1.6f * u, withA(th.accent, 0.85f));
        break;
    default:
        p.rrect(x0, y0 + 10 * u, w, h, 40 * u, hex(0xd77aa8, 0.25f), 30 * u);
        p.rrect(x0, y0, w, h, 40 * u, hex(0xffffff, 0.88f));
        break;
    }
    const Menu::SongView v = m.songView();
    const float lx = x0 + 44 * u, rx = x0 + w - 44 * u;
    const Col ink = th.ink, dim = th.dim;
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "%s %s   /   %s %d %s %d", cs("Stage").c_str(), kind == 2 ? roman(v.stage + 1).c_str() : two(v.stage + 1).c_str(),
                      cs("Song").c_str(), v.song + 1, cs("of").c_str(), v.songs);
        p.text(th.label, b, lx, y0 + 46 * u, 18 * u, dim, -1, th.track);
        std::string title = v.title;
        const float ts = (kind == 2 ? 52.f : kind == 3 ? 40.f : 42.f) * u;
        while (p.width(th.head, title, ts) > 1150 * u && title.size() > 4) title.pop_back();
        if (kind == 3) p.textFx(th.head, title, lx, y0 + 108 * u, ts, hex(0xffffff), hex(0xffb0e8), -1, 0.01f, 0.18f);
        else p.text(th.head, title, lx, y0 + 108 * u, ts, ink, -1, kind == 2 ? 0.f : 0.02f);
        Col uc = rgb8(v.col);
        if (kind == 4) uc = mixc(uc, hex(0x5b4a7a), 0.35f);
        if (kind == 2) uc = mixc(uc, th.gold, 0.4f);
        const float uw = p.text(th.label, cs(titleCase(v.universe.c_str())), lx, y0 + 160 * u, 21 * u, uc, -1, th.track);
        p.text(th.label, cs("Velocity ") + fmt1(v.velocity), lx + uw + 44 * u, y0 + 160 * u, 21 * u, dim, -1, th.track);
        if (v.beaten) std::snprintf(b, sizeof(b), "%s    %s %d", cs("Beaten").c_str(), cs("Best").c_str(), v.best);
        else std::snprintf(b, sizeof(b), "%s %s", cs("Not beaten on").c_str(), cs(titleCase(difficultyName(m.diff_))).c_str());
        const Col bc = v.beaten ? (kind == 4 ? hex(0xf08a2a) : th.gold) : dim;
        const float bw = p.text(th.label, b, rx, y0 + 46 * u, 20 * u, bc, 1, th.track);
        if (v.beaten) star4(rx - bw - 22 * u, y0 + 40 * u, 9 * u, bc, 0.6f);
        p.text(th.label, cs(titleCase(v.preview.c_str())), rx, y0 + 108 * u, 18 * u, withA(kind == 4 ? hex(0x6a8fe0) : th.accent, 0.95f), 1, th.track);
        if (v.song > 0) p.text(th.label, cs("A run from here doesn't clear the stage"), rx, y0 + 196 * u, 14 * u, withA(dim, 0.7f), 1, th.track);
    } else {
        p.text(th.head, kind == 2 ? "Choose a difficulty, then a constellation" : cs("Choose a difficulty, then a song"), lx, y0 + 108 * u,
               (kind == 2 ? 44.f : 34.f) * u, ink, -1, kind == 2 ? 0.f : 0.05f);
        p.text(th.label, cs("Left / right: difficulty    Down: the stages"), lx, y0 + 160 * u, 19 * u, dim, -1, th.track);
    }
    p.text(th.label, cs("Enter play    Esc back"), rx, y0 + 160 * u, 17 * u, withA(dim, 0.8f), 1, th.track);
}

// ---------------------------------------------------------------------------------------------------------------------
// small things

void Cosmos::star4(float cx, float cy, float s, Col c, float glow) {
    p.setAdditive(true);
    if (glow > 0) p.glow(cx, cy, s * 2.2f, withA(c, 0.45f * glow));
    p.line(cx - s, cy, cx + s, cy, s * 0.14f, withA(c, 0.9f));
    p.line(cx, cy - s, cx, cy + s, s * 0.14f, withA(c, 0.9f));
    p.setAdditive(false);
    p.circle(cx, cy, s * 0.22f, mixc(c, hex(0xffffff), 0.6f));
}

void Cosmos::kawaii(float cx, float cy, float R, Col c, int seed, bool awake, float focus, bool happy) {
    p.circle(cx + R * 0.06f, cy + R * 0.12f, R * 1.02f, hex(0xa070b0, 0.18f), R * 0.25f); // a soft shadow
    p.sphere(cx, cy, R, c, (float)seed, 3, 0.f);
    const Col ink = hex(0x3b2a4a);
    const float ey = cy - R * 0.02f, ex = R * 0.32f;
    const bool blink = std::fmod(t + seed * 1.37f, 4.3f) < 0.13f;
    // blush
    p.circle(cx - R * 0.52f, cy + R * 0.2f, R * 0.13f, hex(0xff7aa8, 0.5f), R * 0.1f);
    p.circle(cx + R * 0.52f, cy + R * 0.2f, R * 0.13f, hex(0xff7aa8, 0.5f), R * 0.1f);
    for (int sgn : {-1, 1}) {
        const float x = cx + sgn * ex;
        if (!awake) p.arc(x, ey - R * 0.02f, R * 0.09f, R * 0.06f, 0.25f, PI - 0.25f, std::max(1.5f, R * 0.035f), ink);
        else if (happy) p.arc(x, ey + R * 0.04f, R * 0.09f, R * 0.08f, PI + 0.3f, TAU - 0.3f, std::max(1.5f, R * 0.04f), ink);
        else if (blink) p.line(x - R * 0.07f, ey, x + R * 0.07f, ey, std::max(1.5f, R * 0.035f), ink);
        else {
            const float eh = R * (0.2f + 0.04f * focus), ew = R * 0.15f;
            p.rrect(x - ew / 2, ey - eh / 2, ew, eh, ew / 2, ink);
            p.circle(x - ew * 0.15f, ey - eh * 0.22f, ew * 0.22f, hex(0xffffff));
            p.circle(x + ew * 0.18f, ey + eh * 0.18f, ew * 0.1f, hex(0xffffff, 0.8f));
        }
    }
    if (!awake) return;
    if (focus > 0.5f) {
        const float mw = R * 0.18f, mh = R * 0.14f;
        p.rrect(cx - mw / 2, cy + R * 0.17f, mw, mh, mw * 0.45f, hex(0x7a2a4a));
        p.circle(cx, cy + R * 0.17f + mh * 0.7f, mw * 0.28f, hex(0xff8aa8));
    } else {
        p.arc(cx, cy + R * 0.14f, R * 0.12f, R * 0.08f, 0.35f, PI - 0.35f, std::max(1.5f, R * 0.035f), ink);
    }
}

void Cosmos::retroPlanet(float cx, float cy, float R, Col c, Col ring, int seed, bool lit) {
    const float rx = R * 1.85f, ry = R * 0.42f;
    auto ringHalf = [&](float a0, float a1) {
        p.setAdditive(true);
        p.arc(cx, cy, rx, ry, a0, a1, 7 * u, withA(ring, lit ? 0.3f : 0.08f), 12 * u);
        p.setAdditive(false);
        p.arc(cx, cy, rx, ry, a0, a1, 2.4f * u, withA(ring, lit ? 0.95f : 0.25f));
        p.arc(cx, cy, rx * 0.86f, ry * 0.86f, a0, a1, 1.2f * u, withA(ring, lit ? 0.6f : 0.15f));
    };
    ringHalf(PI, TAU); // the back half
    if (lit) {
        p.setAdditive(true);
        p.glow(cx, cy, R * 2.2f, withA(c, 0.35f));
        p.setAdditive(false);
    }
    p.sphere(cx, cy, R, lit ? c : hex(0x3a2a55), (float)seed, lit ? 1 : 0, lit ? t * 0.2f + seed : 0.f);
    ringHalf(0, PI); // the front half
}

void Cosmos::ship(float x, float y, float s, float heading) {
    const float ca = std::cos(heading), sa = std::sin(heading);
    auto P = [&](float a, float b, float& ox, float& oy) { ox = x + (a * ca - b * sa) * s; oy = y + (a * sa + b * ca) * s; };
    float nx, ny, lx, ly, rx, ry, bx, by;
    P(1.2f, 0, nx, ny); P(-0.8f, -0.75f, lx, ly); P(-0.8f, 0.75f, rx, ry); P(-0.45f, 0, bx, by);
    p.setAdditive(true);
    float fx, fy;
    P(-1.4f - 0.2f * std::sin(t * 9.f), 0, fx, fy);
    p.glow(fx, fy, s * 1.6f, hex(0xff7a3c, 0.8f));
    p.glow(x, y, s * 2.6f, hex(0x3ff0ff, 0.35f));
    p.line(nx, ny, lx, ly, 6 * u, hex(0x3ff0ff, 0.35f), 8 * u);
    p.line(nx, ny, rx, ry, 6 * u, hex(0x3ff0ff, 0.35f), 8 * u);
    p.setAdditive(false);
    p.line(nx, ny, lx, ly, 2.4f * u, hex(0xe8ffff));
    p.line(nx, ny, rx, ry, 2.4f * u, hex(0xe8ffff));
    p.line(lx, ly, bx, by, 2.4f * u, hex(0xe8ffff));
    p.line(rx, ry, bx, by, 2.4f * u, hex(0xe8ffff));
}

// ---------------------------------------------------------------------------------------------------------------------
// title and free play

void Cosmos::titleScreen() {
    const float cx = X(960);
    static const char* const ITEMS[3] = {"Campaign", "Free play", "Quit"};
    switch (kind) {
    case 0: {
        galaxy(true);
        p.setAdditive(true);
        p.glow(cx, Y(290), 420 * u, hex(0x6a3cff, 0.15f));
        p.setAdditive(false);
        p.text(F::JOSEFIN_LIGHT, gameTitle(true), cx, Y(330), 170 * u, hex(0xffffff, 0.97f), 0, 0.32f);
        p.text(F::JOSEFIN, "", cx, Y(400), 32 * u, th.accent, 0, 0.9f);
        break;
    }
    case 1: {
        orrery();
        p.text(F::JURA_LIGHT, gameTitle(true), cx, Y(200), 110 * u, hex(0xfff4e0), 0, 0.2f);
        p.text(F::JURA, "AN ORRERY OF SIXTY WORLDS", cx, Y(258), 22 * u, withA(th.accent, 0.9f), 0, 0.6f);
        break;
    }
    case 2: {
        // a zodiac ring turning slowly behind the title
        const float rcx = cx, rcy = Y(330), R = 300 * u;
        p.ring(rcx, rcy, R, 1.5f * u, withA(th.gold, 0.6f));
        p.ring(rcx, rcy, R - 34 * u, 1 * u, withA(th.gold, 0.45f));
        p.ring(rcx, rcy, R + 10 * u, 0.8f * u, withA(th.gold, 0.35f));
        const float rot = t * 0.02f;
        for (int i = 0; i < 72; i++) {
            const float a = rot + i * TAU / 72, l = (i % 6 == 0 ? 16.f : 8.f) * u;
            p.line(rcx + std::cos(a) * R, rcy + std::sin(a) * R, rcx + std::cos(a) * (R - l), rcy + std::sin(a) * (R - l), 1 * u, withA(th.gold, 0.5f));
        }
        for (int i = 0; i < 12; i++) {
            const float a = rot + (i + 0.5f) * TAU / 12;
            p.text(F::MARCELLUS, roman(i + 1), rcx + std::cos(a) * (R - 17 * u), rcy + std::sin(a) * (R - 17 * u) + 6 * u, 15 * u, withA(th.gold, 0.7f), 0);
        }
        constellation(cx - 560 * u, Y(640), 120 * u, 11, 0, 0.f, true, true, 3);
        constellation(cx + 560 * u, Y(640), 120 * u, 6, 1, 0.f, true, true, 5);
        p.setAdditive(true);
        p.glow(cx, Y(320), 300 * u, hex(0xffcf7a, 0.1f));
        p.setAdditive(false);
        p.text(F::CORMORANT_ITALIC, GAME_TITLE, cx, Y(345), 130 * u, th.ink, 0);
        p.text(F::MARCELLUS, "A CELESTIAL ATLAS OF SIXTY WORLDS", cx, Y(405), 20 * u, withA(th.gold, 0.85f), 0, 0.4f);
        break;
    }
    case 3: {
        retroPlanet(X(300), Y(300 + 12 * std::sin(t * 0.6f)), 70 * u, hex(0xff3fb4), hex(0x3ff0ff), 3, true);
        retroPlanet(X(1650), Y(250 + 12 * std::sin(t * 0.5f + 1)), 50 * u, hex(0x7a5cff), hex(0xffe066), 7, true);
        // a chrome title: dark outline, a gradient fill, slanted
        for (int i = 0; i < 8; i++) {
            const float a = i * 0.785f;
            p.textFx(F::INTER_BOLD, gameTitle(true), cx + std::cos(a) * 5 * u, Y(330) + std::sin(a) * 5 * u, 190 * u, hex(0x1a0033), hex(0x1a0033), 0, 0.04f, 0.22f);
        }
        p.setAdditive(true);
        p.textFx(F::INTER_BOLD, gameTitle(true), cx, Y(330), 190 * u, hex(0xff3fb4, 0.25f), hex(0xff3fb4, 0.25f), 0, 0.04f, 0.22f);
        p.setAdditive(false);
        p.textFx(F::INTER_BOLD, gameTitle(true), cx, Y(330), 190 * u, hex(0xe8fbff), hex(0x6a5cff), 0, 0.04f, 0.22f);
        p.textFx(F::INTER_BOLD, gameTitle(true), cx, Y(330) - 70 * u, 190 * u, hex(0xffffff, 0.0f), hex(0xffffff, 0.0f), 0, 0.04f, 0.22f);
        p.line(cx - 330 * u, Y(262), cx + 360 * u, Y(262), 2 * u, hex(0xffffff, 0.5f));
        p.setAdditive(true);
        for (int i = 0; i < 8; i++) {
            const float a = i * 0.785f;
            p.textFx(F::MANROPE, "", cx + 40 * u + std::cos(a) * 4 * u, Y(410) + std::sin(a) * 4 * u, 52 * u, hex(0x3ff0ff, 0.2f), hex(0x3ff0ff, 0.2f), 0, 0.5f, 0.2f);
        }
        p.setAdditive(false);
        p.textFx(F::MANROPE, "", cx + 43 * u, Y(413), 52 * u, hex(0x12002a, 0.7f), hex(0x12002a, 0.7f), 0, 0.5f, 0.2f);
        p.textFx(F::MANROPE, "", cx + 40 * u, Y(410), 52 * u, hex(0xffffff), hex(0xa8f8ff), 0, 0.5f, 0.2f);
        break;
    }
    default: {
        kawaii(X(330), Y(330 + 14 * std::sin(t * 1.1f)), 110 * u, hex(0xffb3d1), 1, true, 0.f, false);
        kawaii(X(1600), Y(300 + 14 * std::sin(t * 1.0f + 2)), 90 * u, hex(0xb8e6ff), 2, true, 0.f, true);
        kawaii(X(1450), Y(700 + 10 * std::sin(t * 1.2f + 1)), 50 * u, hex(0xc8f2c8), 3, std::fmod(t, 9.f) > 3.f, 0.f, false);
        kawaii(X(470), Y(720 + 10 * std::sin(t * 0.9f + 4)), 44 * u, hex(0xffe0a0), 4, true, 0.f, true);
        p.textFx(F::COMFORTAA, GAME_TITLE, X(960), Y(310), 150 * u, hex(0xff7eb6), hex(0xb07aff), 0, 0.02f);
        p.textFx(F::COMFORTAA, "", X(960), Y(410), 80 * u, hex(0x7ab8ff), hex(0x9a7aff), 0, 0.04f);
        break;
    }
    }
    menuList(ITEMS, 3, m.aTitle_, kind == 1 ? 760.f : 640.f, kind == 1 ? 78.f : 88.f, 42);
    if (m.progress_ && m.camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d %s", m.progress_->clearedCount(), m.camp_->size(), cs("stages cleared").c_str());
        p.text(th.body, b, X(960), Y(kind == 1 ? 1010 : 960), 19 * u, th.dim, 0, th.track);
    }
    if (kind != 1) hint("Arrows move    Enter select    Esc quit", 1020);
}

void Cosmos::freeScreen() {
    const float cx = X(960);
    if (kind == 3) p.textFx(F::INTER_BOLD, "FREE PLAY", cx, Y(220), 110 * u, hex(0xe8fbff), hex(0xff6ad0), 0, 0.04f, 0.2f);
    else if (kind == 4) p.textFx(F::COMFORTAA, "Free play", cx, Y(220), 110 * u, hex(0xff7eb6), hex(0xb07aff), 0, 0.02f);
    else p.text(th.head, cs("Free play"), cx, Y(220), (kind == 2 ? 120.f : 96.f) * u, th.ink, 0, kind == 2 ? 0.f : 0.25f);
    p.text(th.body, cs("The playlist in a random order, levels rising with the lines"), cx, Y(290), 21 * u, th.dim, 0, th.track);
    static const uint32_t C[5][3] = {{0x6ff0c8, 0x9c86ff, 0xff6fc8}, {0x7ad0a0, 0xe0a050, 0xd05a3a},
                                     {0x9fc3e8, 0xe6c06a, 0xff9a7a}, {0x3ff0ff, 0x9a5cff, 0xff3fb4},
                                     {0xc8f2c8, 0xffe0a0, 0xffb3d1}};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(m.aFree_, d)), x = cx + (d - 1) * 470 * u, y = Y(560);
        const Col c = hex(C[kind][d]);
        const float speed = freePlayMultiplier(d);
        switch (kind) {
        case 0: case 1: {
            const float R = (44 + 10 * f) * u;
            p.setAdditive(true);
            p.glow(x, y, R * (2.5f + f), withA(c, 0.25f + 0.3f * f));
            p.setAdditive(false);
            for (int k = 0; k < 2 + d; k++) {
                const float ro = R * (1.9f + 0.55f * k), a = t * speed * (0.9f - 0.15f * k) + k * 2.f;
                p.arc(x, y, ro, ro * 0.35f, 0, TAU, 1 * u, withA(th.ink, 0.12f + 0.15f * f));
                const float mx = x + std::cos(a) * ro, my = y + std::sin(a) * ro * 0.35f;
                p.sphere(mx, my, (7 + 2 * k) * u * (1 + 0.2f * f), mixc(c, hex(0xffffff), 0.2f * k), (float)k, k % 2 ? 2 : 0, t * 0.3f);
            }
            p.sphere(x, y, R, kind == 0 ? mixc(c, hex(0xffffff), 0.3f) : c, (float)d, kind == 0 ? 4 : 1, t * 0.15f);
            break;
        }
        case 2: constellation(x, y, (95 + 15 * f) * u, d * 3 + 1, d, f, true, true, d + 2); break;
        case 3: {
            retroPlanet(x, y + 8 * std::sin(t + d) * u, (48 + 10 * f) * u, c, hex(C[3][(d + 1) % 3]), d + 2, true);
            break;
        }
        default: kawaii(x, y + 10 * std::sin(t * 1.2f + d) * u, (62 + 12 * f) * u, c, d + 5, true, f, d == 0); break;
        }
        const Col lc = withA(th.ink, 0.45f + 0.55f * f);
        p.text(th.body, cs(titleCase(difficultyName(d))), x, Y(745), (30 + 4 * f) * u, lc, 0, th.track + 0.1f);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", speed);
        p.text(th.body, b, x, Y(790), 24 * u, withA(th.dim, 0.5f + 0.5f * f), 0, 0.05f);
    }
    char b[96];
    std::snprintf(b, sizeof(b), "%s %d%% %s", cs("Gravity at").c_str(), (int)std::lround(freePlayMultiplier(m.freeDiff_) * 100),
                  cs("of its full speed, on every level").c_str());
    p.text(th.body, b, cx, Y(900), 21 * u, th.ink, 0, th.track);
    hint("Enter start    Esc back", 1020);
}

// ---------------------------------------------------------------------------------------------------------------------
// the campaign worlds

void Cosmos::campaignScreen() {
    if (nStages() > 0) {
        switch (kind) {
        case 0: galaxy(false); break;
        case 1: orrery(); break;
        case 2: atlas(); break;
        case 3: synth(); break;
        default: pastel(); break;
        }
    }
    tabs();
    info();
}

// GALAXY: a spiral galaxy in 3D; the camera flies to the focused system.
void Cosmos::galaxy(bool titleMode) {
    const int n = titleMode ? 12 : nStages();
    const float rotG = t * 0.012f;
    auto stagePos = [&](float s) {
        const float th0 = 0.62f * s + 0.5f + rotG, r = 2.3f + 0.92f * s;
        return V3{r * std::cos(th0), 0.15f * std::sin(s * 1.7f), r * std::sin(th0)};
    };
    Cam cam;
    cam.F = 900 * u;
    cam.cx = X(960);
    cam.cy = Y(titleMode ? 560 : 520);
    if (titleMode) { // a slow flight around the whole galaxy
        const float a = t * 0.03f;
        cam.look(V3{std::cos(a) * 15.f, 9.f, std::sin(a) * 15.f}, V3{0, -1.2f, 0});
    } else {
        const float s = std::clamp(m.aStage_, 0.f, (float)(n - 1));
        const V3 tgt = stagePos(s);
        const V3 out = norm(V3{tgt.x, 0, tgt.z});
        cam.look(tgt + out * 2.4f + V3{0, 2.1f, 0} + norm(cross(V3{0, 1, 0}, out)) * 1.3f, tgt + V3{0, -0.15f, 0});
    }
    // the galaxy: its core, then the stars and dust of its arms
    p.setAdditive(true);
    float sx, sy, sc;
    if (cam.project(V3{0, 0, 0}, sx, sy, sc)) {
        p.glow(sx, sy, 3.2f * sc, hex(0xffb070, 0.35f + 0.15f * m.level_));
        p.glow(sx, sy, 1.2f * sc, hex(0xfff0d0, 0.55f));
    }
    for (const Dust& d : galaxyDust()) {
        const float a = d.th + rotG;
        float depth = 0;
        if (!cam.project(V3{d.r * std::cos(a), d.y, d.r * std::sin(a)}, sx, sy, sc, &depth)) continue;
        if (sx < -200 || sx > m.fw_ + 200 || sy < -200 || sy > m.fh_ + 200) continue;
        const float near = std::clamp((depth - 0.6f) / 1.6f, 0.f, 1.f); // close ones fade instead of growing into blobs
        const float sz = std::clamp(0.014f * sc * d.size, 0.6f * u, 2.6f * u);
        p.circle(sx, sy, sz, withA(d.c, 0.7f * near), sz * 1.5f);
        if (d.big) p.glow(sx, sy, std::min(0.5f * sc, 140.f * u), withA(d.c, 0.06f * near));
    }
    p.setAdditive(false);
    // the journey along the arm
    for (int s = 0; s + 1 < n; s++) {
        const bool o = titleMode || open(s + 1);
        for (int i = 1; i < 10; i++) {
            if (!cam.project(stagePos(s + i / 10.f), sx, sy, sc)) continue;
            p.circle(sx, sy, std::max(1.2f * u, 0.02f * sc), hex(0xffffff, o ? 0.35f : 0.1f));
        }
    }
    // the systems, far ones first
    std::vector<std::pair<float, int>> order;
    for (int s = 0; s < n; s++) {
        float d = 0;
        if (cam.project(stagePos((float)s), sx, sy, sc, &d)) order.push_back({-d, s});
    }
    std::sort(order.begin(), order.end());
    auto planetPos = [&](int s, int k) {
        const bool o = titleMode || open(s);
        const float ro = 0.34f + 0.12f * k, a = k * 1.3f + (o ? t * 0.22f / (1.f + 0.3f * k) : 0.f);
        return stagePos((float)s) + V3{ro * std::cos(a), 0.06f * std::sin(a + k), ro * std::sin(a)};
    };
    for (auto& [negd, s] : order) {
        const bool o = titleMode || open(s);
        const float f = titleMode ? 0.f : focusStage(s);
        const Col sc0 = stageCol(s);
        if (!cam.project(stagePos((float)s), sx, sy, sc)) continue;
        // orbits
        for (int k = 0; k < 5 && !titleMode; k++) {
            const float ro = 0.34f + 0.12f * k;
            float px0 = 0, py0 = 0, s2;
            for (int i = 0; i <= 40; i++) {
                const float a = i * TAU / 40;
                float px, py;
                if (!cam.project(stagePos((float)s) + V3{ro * std::cos(a), 0, ro * std::sin(a)}, px, py, s2)) continue;
                if (i > 0) p.line(px0, py0, px, py, 1 * u, withA(sc0, o ? 0.12f + 0.2f * f : 0.05f));
                px0 = px; py0 = py;
            }
        }
        p.setAdditive(true);
        p.glow(sx, sy, std::min(0.9f * sc, 300 * u), withA(o ? sc0 : hex(0x803030), o ? 0.5f + 0.2f * f : 0.15f));
        p.setAdditive(false);
        p.sphere(sx, sy, std::max(2.f * u, 0.075f * sc), o ? mixc(sc0, hex(0xfff8e0), 0.6f) : hex(0x5a3a3a), (float)s, 4, t * 0.2f);
        if (o && !titleMode && cleared(s)) p.ring(sx, sy, 0.13f * sc, 1.6f * u, withA(th.gold, 0.85f));
        if (titleMode) continue;
        if (!o) p.lock(sx, sy - 0.2f * sc, std::max(10 * u, 0.12f * sc), hex(0xb0b4d0, 0.8f));
        for (int k = 0; k < nSongs(s) && k < 5; k++) {
            float px, py, ps;
            if (!cam.project(planetPos(s, k), px, py, ps)) continue;
            const Col c = o ? songCol(s, k) : hex(0x40445a);
            const bool b = o && beaten(s, k);
            const float pr = std::max(2.5f * u, (0.045f + 0.008f * k) * ps);
            if (b) {
                p.setAdditive(true);
                p.glow(px, py, pr * 3.5f, withA(c, 0.5f));
                p.setAdditive(false);
            }
            p.sphere(px, py, pr, b ? c : mixc(c, hex(0x101020), o ? 0.35f : 0.6f), (float)(s * 5 + k), k % 3 == 1 ? 1 : k % 3 == 2 ? 2 : 0, o ? t * 0.4f : 0.f);
        }
        p.text(th.body, two(s + 1), sx, sy + 0.42f * sc + 10 * u, std::clamp(0.12f * sc, 12 * u, 30 * u), withA(th.ink, o ? 0.5f + 0.5f * f : 0.25f), 0, 0.25f);
        if (o && cleared(s))
            p.text(th.body, std::to_string(m.progress_->stageBest(s, m.diff_)), sx, sy + 0.42f * sc + 36 * u, std::clamp(0.09f * sc, 11 * u, 22 * u), withA(th.gold, 0.85f), 0, 0.15f);
    }
    if (titleMode || m.aRow_ < 0.5f) return;
    float fx, fy;
    focusPoint([&](int s, int k, float& x, float& y) { float sc2; if (!cam.project(planetPos(s, k), x, y, sc2)) { x = cam.cx; y = cam.cy; } }, fx, fy);
    const float pulse = 0.8f + 0.2f * std::sin(t * 2.4f);
    p.setAdditive(true);
    p.glow(fx, fy, 50 * u, hex(0xffffff, 0.25f * pulse));
    p.setAdditive(false);
    p.ring(fx, fy, 22 * u, 2 * u, hex(0xffffff, 0.9f * pulse));
    p.arc(fx, fy, 30 * u, 30 * u, t * 0.8f, t * 0.8f + 1.2f, 1.5f * u, withA(th.accent, 0.9f));
    p.arc(fx, fy, 30 * u, 30 * u, t * 0.8f + PI, t * 0.8f + PI + 1.2f, 1.5f * u, withA(th.accent2, 0.9f));
}

// ORRERY: a big sun, twelve planets on brass-ringed orbits, the camera gliding outside the focused planet's orbit.
void Cosmos::orrery() {
    const bool title = m.screen_ == Menu::TITLE;
    const int n = title ? 12 : nStages();
    auto R = [](int s) { return 2.6f + 1.05f * s; };
    auto phi = [&](int s) { return 2.2f - 0.5f * s + ((title || open(s)) ? t * 0.025f / (1.f + 0.15f * s) : 0.f); };
    auto planetPos = [&](int s) { return V3{R(s) * std::cos(phi(s)), 0, R(s) * std::sin(phi(s))}; };
    auto prad = [](int s) { return 0.33f + 0.13f * ((s * 7) % 4) / 3.f; };
    Cam cam;
    cam.F = 950 * u;
    cam.cx = X(960);
    cam.cy = Y(title ? 640 : 420);
    if (title) {
        const float a = t * 0.02f;
        cam.look(V3{std::cos(a) * 20.f, 11.f, std::sin(a) * 20.f}, V3{0, -2.f, 0});
    } else {
        const float s = std::clamp(m.aStage_, 0.f, (float)(n - 1));
        const int s0 = (int)std::floor(s), s1 = std::min(n - 1, s0 + 1);
        const V3 tgt = lerp3(planetPos(s0), planetPos(s1), s - s0);
        const float l = std::sqrt(tgt.x * tgt.x + tgt.z * tgt.z);
        cam.look(tgt * (1.f + 4.6f / l) + V3{0, 2.2f, 0}, tgt * 0.8f + V3{0, -0.4f, 0});
    }
    float sx, sy, sc, sd = 0;
    const bool sunVis = cam.project(V3{0, 0, 0}, sx, sy, sc, &sd);
    // orbit rings in brass
    for (int s = 0; s < n; s++) {
        const bool o = title || open(s);
        const float f = title ? 0.f : focusStage(s);
        float px0 = 0, py0 = 0, ps;
        bool have = false;
        for (int i = 0; i <= 120; i++) {
            const float a = i * TAU / 120;
            float px, py;
            if (!cam.project(V3{R(s) * std::cos(a), 0, R(s) * std::sin(a)}, px, py, ps)) { have = false; continue; }
            if (have) p.line(px0, py0, px, py, (1 + f) * u, hex(0xe8b860, o ? 0.22f + 0.4f * f : 0.08f));
            px0 = px; py0 = py; have = true;
        }
    }
    // what to draw, by depth: the sun and the planets (their moons drawn with them)
    std::vector<std::pair<float, int>> order;
    if (sunVis) order.push_back({-sd, -1});
    for (int s = 0; s < n; s++) {
        float d;
        if (cam.project(planetPos(s), sx, sy, sc, &d)) order.push_back({-d, s});
    }
    std::sort(order.begin(), order.end());
    auto moonPos = [&](int s, int k) {
        const float pr = prad(s), ro = pr * (2.0f + 0.5f * k);
        const float a = k * 1.25f + ((title || open(s)) ? t * 0.35f / (1.f + 0.25f * k) : 0.f);
        return planetPos(s) + V3{ro * std::cos(a), ro * 0.28f * std::sin(a + 0.5f), ro * std::sin(a)};
    };
    for (auto& [negd, s] : order) {
        if (s < 0) {
            cam.project(V3{0, 0, 0}, sx, sy, sc);
            p.setAdditive(true);
            p.glow(sx, sy, 4.5f * sc, hex(0xff7a20, 0.35f + 0.1f * m.level_));
            p.glow(sx, sy, 2.0f * sc, hex(0xffc060, 0.55f));
            p.setAdditive(false);
            p.sphere(sx, sy, 1.1f * sc, hex(0xffa030), 3.f, 4, t * 0.05f);
            p.setAdditive(true);
            p.glow(sx, sy, 1.3f * sc, hex(0xfff0c0, 0.35f));
            p.setAdditive(false);
            continue;
        }
        const bool o = title || open(s);
        const float f = title ? 0.f : focusStage(s);
        cam.project(planetPos(s), sx, sy, sc);
        float sunx, suny, sunsc;
        if (cam.project(V3{0, 0, 0}, sunx, suny, sunsc) && o) p.line(sunx, suny, sx, sy, 1.4f * u, hex(0xe8b860, 0.18f)); // the brass arm
        const float pr = prad(s) * sc;
        const int type = std::array<int, 5>{1, 0, 2, 1, 0}[s % 5];
        Col c = title ? stageCol(s) : mixc(songCol(s, 0), stageCol(s), 0.4f);
        if (!o) c = hex(0x2a3040);
        // the moons behind the planet, then the planet, then the moons in front
        auto moons = [&](bool front) {
            if (title) return;
            for (int k = 0; k < nSongs(s) && k < 5; k++) {
                float mx, my, ms, md, pd;
                if (!cam.project(moonPos(s, k), mx, my, ms, &md)) continue;
                cam.project(planetPos(s), sx, sy, sc, &pd);
                if ((md < pd) != front) continue;
                const bool b = o && beaten(s, k);
                const Col mc = o ? songCol(s, k) : hex(0x3a4050);
                const float mr = std::max(2.f * u, 0.075f * ms);
                if (b) { p.setAdditive(true); p.glow(mx, my, mr * 3.2f, withA(mc, 0.5f)); p.setAdditive(false); }
                p.sphere(mx, my, mr, b ? mc : mixc(mc, hex(0x101018), 0.4f), (float)(k + s), k % 2 ? 2 : 0, o ? t * 0.3f : 0.f);
            }
        };
        const bool rings = s % 3 == 1;
        const Col ringC = title ? mixc(stageCol(s + 4), hex(0xffe8c0), 0.4f) : o ? mixc(songCol(s, 1 % nSongs(s)), hex(0xffe8c0), 0.4f) : hex(0x3a4050);
        moons(false);
        if (rings) p.arc(sx, sy, pr * 1.9f, pr * 0.5f, PI, TAU, std::max(2.f, pr * 0.12f), withA(ringC, o ? 0.75f : 0.4f));
        if (o) { p.setAdditive(true); p.glow(sx, sy, pr * 2.f, withA(c, 0.25f + 0.2f * f)); p.setAdditive(false); }
        p.sphere(sx, sy, pr, c, (float)s * 3.1f, o ? type : 0, o ? t * 0.25f + s : 0.f);
        if (!o) p.ring(sx, sy, pr, std::max(1.f, pr * 0.05f), hex(0x9fc8ff, 0.35f), 3 * u); // frost
        if (rings) p.arc(sx, sy, pr * 1.9f, pr * 0.5f, 0, PI, std::max(2.f, pr * 0.12f), withA(ringC, o ? 0.85f : 0.4f));
        moons(true);
        if (title) continue;
        if (!o) p.lock(sx, sy, std::max(12 * u, pr * 0.6f), hex(0xc0d0f0, 0.75f));
        const float ly = sy + pr + std::max(22 * u, pr * 0.5f);
        p.text(F::JURA, roman(s + 1), sx, ly, std::clamp(pr * 0.45f, 13 * u, 34 * u), withA(th.ink, o ? 0.55f + 0.45f * f : 0.3f), 0, 0.2f);
        if (o && cleared(s)) p.text(F::JURA, std::to_string(m.progress_->stageBest(s, m.diff_)), sx, ly + std::clamp(pr * 0.4f, 14 * u, 30 * u),
                                  std::clamp(pr * 0.3f, 11 * u, 22 * u), withA(th.gold, 0.9f), 0, 0.15f);
    }
    if (title || m.aRow_ < 0.5f) return;
    float fx, fy;
    focusPoint([&](int s, int k, float& x, float& y) { float sc2; if (!cam.project(moonPos(s, k), x, y, sc2)) { x = cam.cx; y = cam.cy; } }, fx, fy);
    const float pulse = 0.8f + 0.2f * std::sin(t * 2.4f);
    p.ring(fx, fy, 20 * u, 2 * u, withA(th.accent, 0.95f * pulse));
    for (int i = 0; i < 4; i++) {
        const float a = t * 0.6f + i * PI / 2;
        p.line(fx + std::cos(a) * 26 * u, fy + std::sin(a) * 26 * u, fx + std::cos(a) * 36 * u, fy + std::sin(a) * 36 * u, 2 * u, withA(th.accent, 0.9f));
    }
}

void Cosmos::constellation(float cx, float cy, float sc, int fig, int s, float focus, bool isOpen, bool demo, int demoBeaten) {
    const Figure& F0 = FIGS[((fig % 12) + 12) % 12];
    float px[5], py[5];
    for (int k = 0; k < 5; k++) { px[k] = cx + F0.p[k][0] * sc; py[k] = cy + F0.p[k][1] * sc * 0.8f; }
    for (int e = 0; e < F0.ne; e++) {
        const int a = F0.e[e][0], b = F0.e[e][1];
        if (!isOpen) {
            for (int i = 0; i < 10; i += 2)
                p.line(lerpf(px[a], px[b], i / 10.f), lerpf(py[a], py[b], i / 10.f), lerpf(px[a], px[b], (i + 1) / 10.f), lerpf(py[a], py[b], (i + 1) / 10.f),
                       1 * u, hex(0xa8b4c8, 0.22f));
            continue;
        }
        p.line(px[a], py[a], px[b], py[b], (1.2f + 0.6f * focus) * u, withA(th.gold, 0.45f + 0.35f * focus));
        if (focus > 0.02f) { // drawn again in light as the focus arrives
            const float k = sstep(focus * 1.3f - e * 0.08f);
            p.setAdditive(true);
            p.line(px[a], py[a], lerpf(px[a], px[b], k), lerpf(py[a], py[b], k), 4 * u, withA(th.gold, 0.35f * focus), 6 * u);
            p.setAdditive(false);
        }
        const float sp = std::fmod(t * 0.22f + e * 0.37f + s * 0.13f, 1.f); // a spark travelling along
        p.setAdditive(true);
        p.glow(lerpf(px[a], px[b], sp), lerpf(py[a], py[b], sp), 12 * u, withA(th.gold, 0.5f * std::sin(sp * PI)));
        p.setAdditive(false);
    }
    for (int k = 0; k < 5; k++) {
        if (!isOpen) { p.circle(px[k], py[k], 3 * u, hex(0xa8b4c8, 0.4f)); continue; }
        const bool b = demo ? k < demoBeaten : (k < nSongs(s) && beaten(s, k));
        const Col uc = demo ? th.gold : mixc(songCol(s, k), th.gold, 0.35f);
        const float tw = 0.8f + 0.2f * std::sin(t * 1.3f + k * 2.1f + s);
        if (b) {
            p.setAdditive(true);
            p.glow(px[k], py[k], 22 * u, withA(uc, 0.45f * tw));
            p.setAdditive(false);
            star4(px[k], py[k], (11 + 3 * focus) * u * tw, mixc(th.gold, hex(0xffffff), 0.3f), 0.5f);
        } else {
            p.ring(px[k], py[k], (6 + focus) * u, 1.4f * u, withA(uc, 0.9f));
            p.circle(px[k], py[k], 2 * u, withA(th.gold, 0.8f));
        }
    }
}

// ATLAS: constellations on an antique navy chart that scrolls to the focused one.
void Cosmos::atlas() {
    const int n = nStages();
    const float camX = m.aStage_ * 420.f;
    auto ctr = [&](int s, float& x, float& y) { x = X(960 + s * 420.f - camX); y = Y(450 + 60 * std::sin(s * 1.1f)); };
    // the journey: a dashed gold route from one constellation to the next
    for (int s = 0; s + 1 < n; s++) {
        float x0, y0, x1, y1;
        ctr(s, x0, y0);
        ctr(s + 1, x1, y1);
        const bool o = open(s + 1);
        for (int i = 2; i < 18; i += 2) {
            const float a = i / 20.f, b = (i + 1) / 20.f;
            const float bend = std::sin(a * PI) * 70 * u, bend2 = std::sin(b * PI) * 70 * u;
            p.line(lerpf(x0, x1, a), lerpf(y0, y1, a) + 265 * u - bend, lerpf(x0, x1, b), lerpf(y0, y1, b) + 265 * u - bend2, 1.2f * u,
                   withA(th.gold, o ? 0.4f : 0.12f));
        }
    }
    for (int s = 0; s < n; s++) {
        float cx, cy;
        ctr(s, cx, cy);
        if (cx < -400 * u || cx > m.fw_ + 400 * u) continue;
        const bool o = open(s);
        const float f = focusStage(s);
        if (o) { p.setAdditive(true); p.glow(cx, cy, (230 + 60 * f) * u, hex(0x4a6aa8, 0.12f + 0.12f * f)); p.setAdditive(false); }
        if (f > 0.02f) { // an astrolabe disc turning behind the focused constellation
            const float R = 235 * u, rot = t * 0.05f;
            p.ring(cx, cy, R, 1.2f * u, withA(th.gold, 0.4f * f));
            p.ring(cx, cy, R - 26 * u, 0.8f * u, withA(th.gold, 0.3f * f));
            for (int i = 0; i < 60; i++) {
                const float a = rot + i * TAU / 60, l = (i % 5 == 0 ? 14.f : 7.f) * u;
                p.line(cx + std::cos(a) * R, cy + std::sin(a) * R, cx + std::cos(a) * (R - l), cy + std::sin(a) * (R - l), 1 * u, withA(th.gold, 0.4f * f));
            }
            p.arc(cx, cy, R * 0.62f, R * 0.62f, -rot * 2.f, -rot * 2.f + 2.2f, 1 * u, withA(th.gold, 0.3f * f));
            p.arc(cx, cy, R * 0.62f, R * 0.62f, -rot * 2.f + PI, -rot * 2.f + PI + 2.2f, 1 * u, withA(th.gold, 0.3f * f));
        }
        constellation(cx, cy, (150 + 20 * f) * u, s, s, f, o, false, 0);
        p.text(F::CORMORANT_ITALIC, "Stage " + roman(s + 1), cx, cy + 175 * u, (34 + 6 * f) * u, withA(th.ink, o ? 0.6f + 0.4f * f : 0.3f), 0);
        if (!o) p.lock(cx, cy, 22 * u, hex(0xa8b4c8, 0.6f));
        else if (cleared(s)) p.text(F::MARCELLUS, "CLEARED   " + std::to_string(m.progress_->stageBest(s, m.diff_)), cx, cy + 210 * u, 16 * u, withA(th.gold, 0.85f), 0, 0.25f);
    }
    // the corners: astrolabe arcs
    for (int i = 0; i < 2; i++) {
        const float ax = i ? X(1920) : X(0), ay = Y(140);
        p.arc(ax, ay, 230 * u, 230 * u, i ? PI * 0.5f : 0.f, i ? PI : PI * 0.5f, 1.2f * u, withA(th.gold, 0.35f));
        p.arc(ax, ay, 250 * u, 250 * u, i ? PI * 0.5f : 0.f, i ? PI : PI * 0.5f, 0.8f * u, withA(th.gold, 0.25f));
    }
    if (m.aRow_ < 0.5f) return;
    float fx, fy;
    focusPoint([&](int s, int k, float& x, float& y) {
        float cx, cy;
        ctr(s, cx, cy);
        const Figure& F0 = FIGS[s % 12];
        const float sc = (150 + 20 * focusStage(s)) * u;
        x = cx + F0.p[k][0] * sc;
        y = cy + F0.p[k][1] * sc * 0.8f;
    }, fx, fy);
    const float rot = t * 0.4f;
    p.ring(fx, fy, 24 * u, 1.4f * u, withA(th.gold, 0.95f));
    p.ring(fx, fy, 30 * u, 0.8f * u, withA(th.gold, 0.5f));
    for (int i = 0; i < 8; i++) {
        const float a = rot + i * TAU / 8, l = (i % 2 ? 6.f : 12.f) * u;
        p.line(fx + std::cos(a) * 30 * u, fy + std::sin(a) * 30 * u, fx + std::cos(a) * (30 * u + l), fy + std::sin(a) * (30 * u + l), 1.2f * u, withA(th.gold, 0.9f));
    }
}

// SYNTHWAVE: ringed neon planets floating above the grid, a neon route, a ship hopping to the focused moon.
void Cosmos::synth() {
    const int n = nStages();
    const float camX = m.aStage_ * 470.f;
    auto ctr = [&](int s, float& x, float& y) { x = X(760 + s * 470.f - camX); y = Y(400 + 50 * std::sin(s * 1.3f) + 10 * std::sin(t * 0.8f + s)); };
    auto moonPos = [&](int s, int k, float& x, float& y) {
        float cx, cy;
        ctr(s, cx, cy);
        const float R = (58 + 12 * focusStage(s)) * u, a = k * TAU / 5 + (open(s) ? t * 0.25f : 0.f);
        x = cx + std::cos(a) * R * 2.15f;
        y = cy + std::sin(a) * R * 0.75f;
    };
    // the route
    p.setAdditive(true);
    for (int s = 0; s + 1 < n; s++) {
        float x0, y0, x1, y1;
        ctr(s, x0, y0);
        ctr(s + 1, x1, y1);
        const bool o = open(s + 1);
        p.line(x0, y0, x1, y1, 8 * u, withA(th.accent, o ? 0.15f : 0.04f), 14 * u);
        p.line(x0, y0, x1, y1, 2 * u, withA(th.accent, o ? 0.6f : 0.12f));
        if (o)
            for (int i = 0; i < 6; i++) {
                const float a = std::fmod(i / 6.f + t * 0.12f, 1.f);
                p.glow(lerpf(x0, x1, a), lerpf(y0, y1, a), 10 * u, withA(th.accent2, 0.6f));
            }
    }
    p.setAdditive(false);
    for (int s = 0; s < n; s++) {
        float cx, cy;
        ctr(s, cx, cy);
        if (cx < -300 * u || cx > m.fw_ + 300 * u) continue;
        const bool o = open(s);
        const float f = focusStage(s), R = (58 + 12 * f) * u;
        const Col c = neonize(stageCol(s)), ring = neonize(songCol(s, 2 % nSongs(s)));
        auto moons = [&](bool front) {
            for (int k = 0; k < nSongs(s) && k < 5; k++) {
                const float a = k * TAU / 5 + (o ? t * 0.25f : 0.f);
                if ((std::sin(a) > 0) != front) continue;
                float mx, my;
                moonPos(s, k, mx, my);
                const Col mc = o ? neonize(songCol(s, k)) : hex(0x4a3a66);
                const float mr = (8 + 2 * f) * u * (front ? 1.f : 0.8f);
                if (o && beaten(s, k)) {
                    p.setAdditive(true);
                    p.glow(mx, my, mr * 3.f, withA(mc, 0.7f));
                    p.setAdditive(false);
                    p.circle(mx, my, mr, mixc(mc, hex(0xffffff), 0.35f));
                } else {
                    p.circle(mx, my, mr, hex(0x12002a, 0.9f));
                    p.ring(mx, my, mr, 2 * u, withA(mc, o ? 0.95f : 0.4f));
                }
            }
        };
        moons(false);
        retroPlanet(cx, cy, R, c, ring, s, o);
        moons(true);
        if (!o) p.lock(cx, cy, 24 * u, hex(0xff3fb4, 0.8f));
        p.textFx(F::INTER_BOLD, "STAGE " + two(s + 1), cx + 2 * u, cy + R + 69 * u, (20 + 4 * f) * u, hex(0x12002a, 0.8f), hex(0x12002a, 0.8f), 0, 0.15f, 0.2f);
        p.textFx(F::INTER_BOLD, "STAGE " + two(s + 1), cx, cy + R + 66 * u, (20 + 4 * f) * u, withA(hex(0xffffff), o ? 0.9f : 0.35f),
                 withA(th.accent, o ? 0.9f : 0.35f), 0, 0.15f, 0.2f);
        if (o && cleared(s)) p.text(F::MANROPE, "CLEAR  " + std::to_string(m.progress_->stageBest(s, m.diff_)), cx, cy + R + 94 * u, 16 * u, th.gold, 0, 0.2f);
    }
    if (m.aRow_ < 0.5f) return;
    float fx, fy;
    focusPoint(moonPos, fx, fy);
    // a hop between targets: the ship rises on an arc while the eased cursor travels
    const float fr = std::max(std::fabs(m.aStage_ - std::round(m.aStage_)), std::fabs(m.aSong_ - std::round(m.aSong_)));
    const float hop = std::sin(std::min(1.f, fr * 2.f) * PI * 0.5f) * 60 * u;
    p.ring(fx, fy, 18 * u, 2 * u, withA(th.accent2, 0.9f));
    ship(fx, fy - 44 * u - hop, 22 * u, -PI / 2 + 0.25f * std::sin(t * 1.5f));
}

// PASTEL: kawaii planets bobbing along a candy path, sleeping while locked, with bubbly moons.
void Cosmos::pastel() {
    const int n = nStages();
    const float camX = m.aStage_ * 430.f;
    auto ctr = [&](int s, float& x, float& y) { x = X(760 + s * 430.f - camX); y = Y(420 + 55 * std::sin(s * 1.2f) + (open(s) ? 10 * std::sin(t * 1.1f + s * 0.7f) : 0.f)); };
    auto moonPos = [&](int s, int k, float& x, float& y) {
        float cx, cy;
        ctr(s, cx, cy);
        const float R = (70 + 14 * focusStage(s)) * u, a = k * TAU / 5 + (open(s) ? t * 0.18f : 0.f);
        x = cx + std::cos(a) * R * 1.85f;
        y = cy + std::sin(a) * R * 0.95f + 4 * u * std::sin(t * 2.f + k);
    };
    // the candy path
    for (int s = 0; s + 1 < n; s++) {
        float x0, y0, x1, y1;
        ctr(s, x0, y0);
        ctr(s + 1, x1, y1);
        const bool o = open(s + 1);
        for (int i = 1; i < 14; i++) {
            const float a = i / 14.f;
            const float yy = lerpf(y0, y1, a) + std::sin(a * PI) * 50 * u;
            p.circle(lerpf(x0, x1, a), yy, 5 * u, i % 2 ? hex(0xff9ccb, o ? 0.9f : 0.3f) : hex(0x8fd8c8, o ? 0.9f : 0.3f));
        }
    }
    for (int s = 0; s < n; s++) {
        float cx, cy;
        ctr(s, cx, cy);
        if (cx < -300 * u || cx > m.fw_ + 300 * u) continue;
        const bool o = open(s);
        const float f = focusStage(s), R = (70 + 14 * f + 4 * f * std::sin(t * 4.f)) * u;
        const Col c = o ? pastelize(stageCol(s), 0.22f) : hex(0xc9c2d8);
        auto moons = [&](bool front) {
            for (int k = 0; k < nSongs(s) && k < 5; k++) {
                const float a = k * TAU / 5 + (o ? t * 0.18f : 0.f);
                if ((std::sin(a) > 0) != front) continue;
                float mx, my;
                moonPos(s, k, mx, my);
                const bool b = o && beaten(s, k);
                const Col mc = o ? pastelize(songCol(s, k), 0.4f) : hex(0xd8d2e4);
                const float mr = (15 + 3 * f) * u;
                if (b) { p.setAdditive(true); p.glow(mx, my, mr * 2.4f, hex(0xffffff, 0.35f)); p.setAdditive(false); }
                p.sphere(mx, my, mr, mc, (float)k, 3, 0.f);
                const Col ink = hex(0x3b2a4a);
                if (!o) {
                    p.line(mx - mr * 0.45f, my, mx - mr * 0.15f, my, 1.5f * u, ink);
                    p.line(mx + mr * 0.15f, my, mx + mr * 0.45f, my, 1.5f * u, ink);
                } else if (b) {
                    p.arc(mx - mr * 0.3f, my + mr * 0.05f, mr * 0.16f, mr * 0.16f, PI + 0.3f, TAU - 0.3f, 1.6f * u, ink);
                    p.arc(mx + mr * 0.3f, my + mr * 0.05f, mr * 0.16f, mr * 0.16f, PI + 0.3f, TAU - 0.3f, 1.6f * u, ink);
                } else {
                    p.circle(mx - mr * 0.3f, my, mr * 0.11f, ink);
                    p.circle(mx + mr * 0.3f, my, mr * 0.11f, ink);
                }
            }
        };
        moons(false);
        kawaii(cx, cy, R, c, s, o, f, o && cleared(s));
        moons(true);
        if (!o) { // sleeping: little z's drifting up
            for (int i = 0; i < 3; i++) {
                const float ph = std::fmod(t * 0.35f + i / 3.f, 1.f);
                p.text(F::COMFORTAA, "z", cx + R * 0.7f + ph * 30 * u, cy - R * 0.6f - ph * 60 * u, (16 + 10 * ph) * u, hex(0x8a7aa8, 0.8f * std::sin(ph * PI)), 0);
            }
        }
        if (o && cleared(s))
            for (int i = 0; i < 3; i++) {
                const float a = t * 0.5f + i * TAU / 3;
                star4(cx + std::cos(a) * R * 1.35f, cy + std::sin(a) * R * 1.1f, 9 * u, hex(0xffc94a), 0.4f);
            }
        p.text(F::COMFORTAA, "Stage " + std::to_string(s + 1), cx, cy + R + 82 * u, (24 + 4 * f) * u, withA(th.ink, o ? 0.9f : 0.4f), 0);
        if (o && cleared(s)) p.text(F::QUICKSAND, "cleared!  " + std::to_string(m.progress_->stageBest(s, m.diff_)), cx, cy + R + 110 * u, 18 * u, hex(0xf06aa0), 0);
    }
    if (m.aRow_ < 0.5f) return;
    float fx, fy;
    focusPoint(moonPos, fx, fy);
    const float b = 1.f + 0.08f * std::sin(t * 4.f);
    p.ring(fx, fy, 26 * u * b, 4 * u, hex(0xffffff, 0.95f), 2 * u);
    p.ring(fx, fy, 26 * u * b, 2 * u, hex(0xff7eb6, 0.9f));
    // a little heart above the focused moon
    const float hx = fx, hy = fy - 44 * u - 4 * u * std::sin(t * 3.f), hs = 9 * u;
    p.circle(hx - hs * 0.5f, hy, hs * 0.62f, hex(0xff6aa0));
    p.circle(hx + hs * 0.5f, hy, hs * 0.62f, hex(0xff6aa0));
    p.line(hx - hs * 0.95f, hy + hs * 0.25f, hx, hy + hs * 1.25f, hs * 0.9f, hex(0xff6aa0));
    p.line(hx + hs * 0.95f, hy + hs * 0.25f, hx, hy + hs * 1.25f, hs * 0.9f, hex(0xff6aa0));
}

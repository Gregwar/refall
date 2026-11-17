// NOTEPAD's drop pictures: during a drop the hand leaves the small doodles and draws one big picture on the free
// page left and right of the well, in its notebook's tools: an outline first (and, on graph paper, the
// construction lines before it), then the colouring part by part, then the words; once it is finished a few of its
// parts move with the music, in stop motion. Each notebook has its own picture:
//
//  - FELT PENS: a rocket launch. On the left the rocket on its pad by the launch tower, a green hill, the moon; on
//    the right the space it is going to: a ringed planet, the Earth, a crescent moon, a flying saucer with its
//    little alien, a comet, stars. Black fineliner outlines, felt pens in back-and-forth bands. Alive: the engines
//    light, smoke billows, the tower's arms swing back and the rocket lifts off a step every bar; a star twinkles
//    on each beat; the saucer rocks.
//  - PENCIL: an engineering drawing of a hot-air balloon on graph paper. On the left its elevation: construction
//    lines first (the axis, the compass circle, tangents), then the envelope and its gores, the burner, the basket,
//    coloured pencils in hatching (graphite cross-hatching on the shaded side), dimensions and callouts; on the
//    right the sheet's details: the plan view, the burner's detail in a circle, a gore's pattern and the title
//    block. Alive: the burner's flames roar on the beat, the plan view turns a gore every bar.
//  - CHALK: a blackboard lesson on the solar system. On the left the Sun and its rays, the inner planets on dashed
//    orbits, the asteroid belt; on the right the giants on their orbits (Jupiter's bands and spot, Saturn's rings,
//    Uranus, Neptune), a title, labels; each planet lit from the Sun's side. Alive: the Sun's rays shimmer on the
//    beat, the Moon goes round the Earth, a shooting star now and then.
//
// Everything here is plain geometry in the picture's units (see Panels); the hand's imprecision is in it (lines a
// little off, closed shapes overshooting where they close, colouring never quite inside the lines), seeded by the
// part and its pose, so a part that doesn't move comes out the same on every drawing.
#include "universes/notepad/art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace notepad_art {
namespace {

using Poly = std::vector<vec2>;

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 unit(vec2 a) { const float l = len(a); return l > 1e-6f ? a * (1.f / l) : V(0, 0); }
vec2 rot(vec2 p, float a) { const float c = std::cos(a), s = std::sin(a); return {c * p.x - s * p.y, s * p.x + c * p.y}; }
uint64_t mixk(uint64_t a, uint64_t b) { return splitmix64(a ^ (b * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull)); }
float deg(float d) { return d * PI / 180.f; }

// ---------------------------------------------------------------- shapes
Poly arc(vec2 c, float rx, float ry, float a0, float a1, int n, float tilt = 0.f) {
    Poly p;
    for (int i = 0; i <= n; i++) {
        const float a = a0 + (a1 - a0) * i / n;
        p.push_back(c + rot(V(rx * std::cos(a), ry * std::sin(a)), tilt));
    }
    return p;
}
Poly circle(vec2 c, float r, int n = 0) { return arc(c, r, r, 0.f, TAU, n > 0 ? n : std::clamp((int)(r * 22.f), 14, 64)); }
Poly ellipse(vec2 c, float rx, float ry, float tilt = 0.f, int n = 0) {
    return arc(c, rx, ry, 0.f, TAU, n > 0 ? n : std::clamp((int)(std::max(rx, ry) * 20.f), 14, 64), tilt);
}
Poly cubic(vec2 a, vec2 b, vec2 c, vec2 d, int n = 12) {
    Poly p;
    for (int i = 0; i <= n; i++) {
        const float t = i / (float)n, u = 1.f - t;
        p.push_back(a * (u * u * u) + b * (3.f * u * u * t) + c * (3.f * u * t * t) + d * (t * t * t));
    }
    return p;
}
void cat(Poly& a, const Poly& b) {
    for (size_t i = 0; i < b.size(); i++)
        if (i > 0 || a.empty() || len(b[0] - a.back()) > 1e-4f) a.push_back(b[i]);
}
Poly rev(Poly p) { std::reverse(p.begin(), p.end()); return p; }
Poly mirror(Poly p, float cx) { for (vec2& q : p) q.x = 2.f * cx - q.x; return p; }
Poly shift(Poly p, vec2 d) { for (vec2& q : p) q = q + d; return p; }
Poly turn(Poly p, vec2 c, float a) { for (vec2& q : p) q = c + rot(q - c, a); return p; }
Poly rect(float x0, float y0, float x1, float y1) { return {V(x0, y0), V(x1, y0), V(x1, y1), V(x0, y1), V(x0, y0)}; }
float length(const Poly& p) {
    float l = 0.f;
    for (size_t i = 1; i < p.size(); i++) l += len(p[i] - p[i - 1]);
    return l;
}
vec2 centroid(const Poly& p) {
    vec2 c = V(0, 0);
    for (const vec2& q : p) c = c + q;
    return p.empty() ? c : c * (1.f / p.size());
}
bool inside(const Poly& P, vec2 q) {
    bool in = false;
    const size_t n = P.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const vec2 a = P[i], b = P[j];
        if ((a.y > q.y) != (b.y > q.y) && q.x < (b.x - a.x) * (q.y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}
// A band of an ellipse ring between two angles (outer arc out, inner arc back).
Poly ringBand(vec2 c, float rxo, float ryo, float rxi, float ryi, float a0, float a1, float tilt) {
    Poly p = arc(c, rxo, ryo, a0, a1, 30, tilt);
    cat(p, arc(c, rxi, ryi, a1, a0, 30, tilt));
    p.push_back(p[0]);
    return p;
}

// Keeps every point about `step` apart (resampled runs would make too many strokes).
Poly thin(const Poly& p, float step) {
    if (p.size() < 3) return p;
    Poly o{p[0]};
    float acc = 0.f;
    for (size_t i = 1; i + 1 < p.size(); i++) {
        acc += len(p[i] - p[i - 1]);
        if (acc >= step) { o.push_back(p[i]); acc = 0.f; }
    }
    o.push_back(p.back());
    return o;
}
// The runs of a line outside the occluders (or inside them, `in`).
std::vector<Poly> visible(const Poly& line, const std::vector<Poly>& occ, bool in = false) {
    std::vector<Poly> runs;
    Poly cur;
    auto hidden = [&](vec2 p) {
        bool h = false;
        for (const Poly& o : occ) if (inside(o, p)) { h = true; break; }
        return h != in;
    };
    for (size_t i = 0; i + 1 < line.size(); i++) {
        const vec2 a = line[i], b = line[i + 1];
        const int n = std::max(1, (int)std::ceil(len(b - a) / 0.05f));
        for (int k = i == 0 ? 0 : 1; k <= n; k++) {
            const vec2 p = a + (b - a) * (k / (float)n);
            if (hidden(p)) {
                if (cur.size() >= 2) runs.push_back(thin(cur, 0.22f));
                cur.clear();
            } else {
                cur.push_back(p);
            }
        }
    }
    if (cur.size() >= 2) runs.push_back(thin(cur, 0.22f));
    return runs;
}

// ---------------------------------------------------------------- the hand
// The line a hand draws: a slow drift off the true line along its length.
Poly hand(Poly p, Rng& r, float amp) {
    if (p.size() < 2 || amp <= 0.f) return p;
    const float f1 = r.range(0.3f, 0.6f), f2 = r.range(0.9f, 1.6f);
    const float a = r.range(0.f, TAU), b = r.range(0.f, TAU), c = r.range(0.f, TAU), d = r.range(0.f, TAU);
    float acc = 0.f;
    for (size_t i = 0; i < p.size(); i++) {
        if (i) acc += len(p[i] - p[i - 1]);
        p[i] = p[i] + V(0.7f * std::sin(acc * f1 + a) + 0.3f * std::sin(acc * f2 + b),
                        0.7f * std::sin(acc * f1 + c) + 0.3f * std::sin(acc * f2 + d)) * amp;
    }
    return p;
}
// A closed shape drawn in one go: it starts anywhere and goes on a little past where it began, off the line.
Poly loopOf(const Poly& c, Rng& r, float over = 0.1f) {
    Poly P = c;
    if (P.size() > 2 && len(P.front() - P.back()) < 1e-4f) P.pop_back();
    const int n = (int)P.size();
    if (n < 3) return c;
    const int s = r.irange(0, n - 1);
    const vec2 ce = centroid(P);
    Poly o;
    for (int i = 0; i <= n; i++) o.push_back(P[(s + i) % n]);
    const int m = std::max(1, (int)std::lround(n * over));
    const float drift = r.range(0.03f, 0.08f) * (r.chance(0.5f) ? 1.f : -1.f);
    for (int j = 1; j <= m; j++) {
        const vec2 q = P[(s + j) % n];
        o.push_back(q + unit(q - ce) * (drift * j / m));
    }
    o[0] = o[0] - unit(o[0] - ce) * (drift * 0.6f);
    return o;
}

// ---------------------------------------------------------------- colouring in
using Iv = std::vector<std::pair<float, float>>;
Iv spans(const Poly& P, float y) {
    std::vector<float> xs;
    const size_t n = P.size();
    for (size_t i = 0; i < n; i++) {
        const vec2 a = P[i], b = P[(i + 1) % n];
        if ((a.y <= y) == (b.y <= y)) continue;
        xs.push_back(a.x + (y - a.y) / (b.y - a.y) * (b.x - a.x));
    }
    std::sort(xs.begin(), xs.end());
    Iv o;
    for (size_t i = 0; i + 1 < xs.size(); i += 2) o.push_back({xs[i], xs[i + 1]});
    return o;
}
Iv unite(Iv v) {
    std::sort(v.begin(), v.end());
    Iv o;
    for (auto& s : v) {
        if (!o.empty() && s.first <= o.back().second) o.back().second = std::max(o.back().second, s.second);
        else o.push_back(s);
    }
    return o;
}
Iv minus(const Iv& a, const Iv& h) {
    Iv o;
    for (auto s : a) {
        float cur = s.first;
        for (auto q : h) {
            if (q.second <= cur) continue;
            if (q.first >= s.second) break;
            if (q.first > cur) o.push_back({cur, q.first});
            cur = std::max(cur, q.second);
            if (cur >= s.second) break;
        }
        if (cur < s.second) o.push_back({cur, s.second});
    }
    return o;
}
Iv cut(const Iv& a, const Iv& b) {
    Iv o;
    for (auto s : a)
        for (auto q : b) {
            const float lo = std::max(s.first, q.first), hi = std::min(s.second, q.second);
            if (hi > lo) o.push_back({lo, hi});
        }
    return o;
}
// An area to colour in: a polygon (even-odd), minus its holes, within its clips.
struct Area {
    Poly poly;
    std::vector<Poly> holes, clips;
};
// Strokes across an area, `sp` apart at the angle `ang`, back and forth, each overshooting or stopping short of the
// edge by up to `over`, a little off parallel.
std::vector<vec2> hatch(const Area& A, float ang, float sp, Rng& r, float over, float jit = 0.12f) {
    std::vector<vec2> out;
    if (A.poly.size() < 3 || sp <= 0.01f) return out;
    auto R = [&](const Poly& p) { Poly q; for (const vec2& v : p) q.push_back(rot(v, -ang)); return q; };
    const Poly a = R(A.poly);
    std::vector<Poly> holes, clips;
    for (const Poly& h : A.holes) holes.push_back(R(h));
    for (const Poly& c : A.clips) clips.push_back(R(c));
    float y0 = 1e9f, y1 = -1e9f;
    for (const vec2& v : a) y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
    int line = 0;
    for (float y = y0 + sp * r.range(0.3f, 0.7f); y < y1; y += sp) {
        Iv iv = spans(a, y);
        if (!holes.empty()) {
            Iv h;
            for (const Poly& p : holes) for (auto s : spans(p, y)) h.push_back(s);
            iv = minus(iv, unite(h));
        }
        for (const Poly& c : clips) iv = cut(iv, spans(c, y));
        for (auto s : iv) {
            if (s.second - s.first < sp * 0.3f) continue;
            const float x0 = s.first - r.range(-0.35f, 1.f) * over, x1 = s.second + r.range(-0.35f, 1.f) * over;
            const float yy = y + r.range(-jit, jit) * sp, dy = r.range(-0.1f, 0.1f) * sp;
            vec2 p0 = V(x0, yy - dy), p1 = V(x1, yy + dy);
            if (line & 1) std::swap(p0, p1);
            out.push_back(rot(p0, ang));
            out.push_back(rot(p1, ang));
            line++;
        }
    }
    return out;
}

// ---------------------------------------------------------------- the sheet
struct Sheet {
    std::vector<Elem>& out;
    uint64_t seed;
    int part = 0, k = 0, side = 0;
    uint32_t pose = 0;
    Rng r{1};
    Sheet(std::vector<Elem>& o, uint64_t s) : out(o), seed(s) {}
    void begin(int p, int sd, uint32_t ps = 0) {
        part = p;
        side = sd;
        pose = ps;
        k = 0;
        r = Rng(mixk(seed, (uint64_t)p * 131u + ps));
    }
    Elem& add(int kind, int phase) {
        out.emplace_back();
        Elem& e = out.back();
        e.id = (uint32_t)part * 256u + (uint32_t)k++;
        e.pose = pose;
        e.kind = kind;
        e.side = side;
        e.phase = phase;
        return e;
    }
    void line(const Poly& p, uint32_t col, int tool, float w, float a, int phase, float dash = 0.f) {
        if (p.size() < 2) return;
        Elem& e = add(E_LINE, phase);
        e.pts = p;
        e.col = col;
        e.tool = tool;
        e.width = w;
        e.alpha = a;
        e.dash = dash;
    }
    void segs(std::vector<vec2> s, uint32_t col, int tool, float w, float a, int phase) {
        if (s.size() < 2) return;
        Elem& e = add(E_SEGS, phase);
        e.pts = std::move(s);
        e.col = col;
        e.tool = tool;
        e.width = w;
        e.alpha = a;
    }
    void text(vec2 at, const std::string& t, uint32_t col, float size, float a = 1.f, int phase = 3) {
        Elem& e = add(E_TEXT, phase);
        e.pts = {at};
        e.text = t;
        e.col = col;
        e.width = size;
        e.alpha = a;
    }
};

// A notebook's hand: its pen, its colouring, its shading.
struct Hand {
    Sheet& S;
    int v;
    uint32_t ink;
    int tool;
    float w;
    // an open line, by hand
    void line(const Poly& p, float wk = 1.f, uint32_t col = 0, int phase = 1, float a = 1.f, int tl = -1, float drift = 1.f) {
        S.line(hand(p, S.r, 0.025f * drift), col ? col : ink, tl >= 0 ? tl : tool, w * wk, a, phase);
    }
    // a closed shape, drawn in one go
    void shape(const Poly& p, float wk = 1.f, uint32_t col = 0, int phase = 1, float a = 1.f, int tl = -1) {
        S.line(hand(loopOf(p, S.r), S.r, 0.022f), col ? col : ink, tl >= 0 ? tl : tool, w * wk, a, phase);
    }
    // the parts of a line outside the occluders
    void lineOut(const Poly& p, const std::vector<Poly>& occ, float wk = 1.f, uint32_t col = 0, int phase = 1, float a = 1.f,
                 bool in = false) {
        for (const Poly& run : visible(p, occ, in)) line(run, wk, col, phase, a);
    }
    // coloured in, the notebook's way
    void colour(const Area& A, uint32_t col, float ang, float dens = 1.f, float a = 1.f, int phase = 2) {
        Rng& r = S.r;
        if (v == 0) { // felt pens: wide bands back and forth, darker where they overlap
            S.segs(hatch(A, ang, 0.16f / dens, r, 0.09f, 0.1f), col, T_MARKER, 0.22f, 0.8f * a, phase);
        } else if (v == 1) { // coloured pencil: a soft layer, then hatching
            S.segs(hatch(A, ang + 0.6f, 0.2f, r, 0.03f), col, T_CPENCIL, 0.2f, 0.3f * a, phase);
            S.segs(hatch(A, ang, 0.075f / dens, r, 0.07f), col, T_CPENCIL, 0.05f, 0.9f * a, phase);
        } else { // chalk: rubbed in with its side, then lines
            S.segs(hatch(A, ang + 0.45f, 0.26f, r, 0.05f), col, T_CHALK, 0.36f, 0.3f * a, phase);
            S.segs(hatch(A, ang, 0.13f / dens, r, 0.08f), col, T_CHALK, 0.065f, 0.75f * a, phase);
        }
    }
    // shading over the colour: graphite cross-hatching, sparse chalk lines
    void shade(const Area& A, float ang, float dens = 1.f, float a = 1.f, int phase = 2) {
        Rng& r = S.r;
        if (v == 1) {
            S.segs(hatch(A, ang, 0.068f / dens, r, 0.05f), 0x45464c, T_PENCIL, 0.036f, 0.75f * a, phase);
        } else if (v == 2) {
            S.segs(hatch(A, ang, 0.2f / dens, r, 0.05f), 0x3a4a44, T_CHALK, 0.05f, 0.5f * a, phase);
        } else {
            S.segs(hatch(A, ang, 0.18f / dens, r, 0.06f, 0.1f), 0x2c5ce0, T_MARKER, 0.12f, 0.3f * a, phase);
        }
    }
};

// A five-pointed star (or, twinkling, a four-pointed sparkle).
Poly star(vec2 c, float r, float a0, bool sparkle) {
    Poly p;
    const int n = sparkle ? 8 : 10;
    for (int i = 0; i <= n; i++) {
        const float a = a0 + PI * 0.5f + i * TAU / n;
        const float rr = (i % 2) ? r * (sparkle ? 0.28f : 0.45f) : r * (sparkle ? 1.25f : 1.f);
        p.push_back(c + V(rr * std::cos(a), rr * std::sin(a)));
    }
    return p;
}
// A puffy cloud: the outline of a few overlapping circles.
void puffs(Hand& H, const std::vector<std::pair<vec2, float>>& c, uint32_t fill, float a, float ang) {
    std::vector<Poly> discs;
    for (auto& q : c) discs.push_back(circle(q.first, q.second));
    Poly hull; // the colouring: every disc (even-odd would leave holes: the union, disc by disc)
    for (size_t i = 0; i < discs.size(); i++) {
        std::vector<Poly> others;
        for (size_t j = 0; j < discs.size(); j++) if (j != i) others.push_back(discs[j]);
        for (const Poly& run : visible(discs[i], others)) H.line(run, 1.f);
    }
    for (size_t i = 0; i < discs.size(); i++) {
        Area A{discs[i], {}, {}};
        for (size_t j = 0; j < i; j++) A.holes.push_back(discs[j]);
        H.colour(A, fill, ang, 1.f, a);
    }
}

// ================================================================ FELT PENS: the rocket launch
const uint32_t F_RED = 0xec3a3a, F_BLUE = 0x14a4dc, F_YELLOW = 0xffd01c, F_GREEN = 0x38b648, F_ORANGE = 0xff891c,
               F_PURPLE = 0x9a4cc9, F_DBLUE = 0x2c5ce0, F_PINK = 0xff3d8b, F_GREY = 0xa9aebb;

void feltPens(Sheet& S, const Panels& P, float beat, int drop) {
    Hand H{S, 0, 0x1b2030, T_PEN, 0.075f};
    const float alive = std::max(0.f, beat - ALIVE);
    const int b = (int)std::floor(alive);
    const bool on = beat >= ALIVE;
    const uint32_t noseC[3] = {F_RED, F_DBLUE, F_PURPLE}, finC[3] = {F_RED, F_ORANGE, F_PINK};
    const uint32_t nose = noseC[drop % 3], fin = finC[(drop / 3 + drop) % 3];
    const float hw = P.hw[0], top = std::min(P.y1[0], 21.5f);

    // ---- the launch: ignition when the picture is finished, lift-off two bars later, a step every bar
    const int maxSteps = std::max(0, std::min(5, (int)((top - 0.8f - 16.3f) / 0.9f)));
    const int step = on && alive >= 8.f ? std::min(maxSteps, 1 + (int)((alive - 8.f) / 4.f)) : 0;
    const float lift = step * 0.9f;
    const float cx = 0.9f, yb = 2.6f + lift, yt = yb + 9.4f;

    // the hill
    S.begin(1, 0);
    Poly hill;
    for (int i = 0; i <= 18; i++) {
        const float x = -hw + 0.15f + (2.f * hw - 0.3f) * i / 18.f;
        hill.push_back(V(x, 0.72f + 0.16f * std::sin(x * 1.3f + 0.7f) + 0.08f * std::sin(x * 2.9f)));
    }
    H.line(hill);
    {
        Poly g = hill;
        g.push_back(V(hill.back().x, 0.f));
        g.push_back(V(hill.front().x, 0.f));
        H.colour({g, {}, {}}, F_GREEN, deg(12.f));
        for (int i = 0; i < 9; i++) { // tufts of grass
            const float x = -hw + 0.6f + (2.f * hw - 1.2f) * (i + 0.5f) / 9.f + S.r.range(-0.2f, 0.2f);
            if (x > -3.7f && x < 4.f) continue; // (the pad)
            const float y = 0.72f + 0.16f * std::sin(x * 1.3f + 0.7f) + 0.08f * std::sin(x * 2.9f);
            H.line({V(x - 0.18f, y + 0.3f), V(x, y - 0.02f), V(x + 0.05f, y + 0.38f), V(x + 0.12f, y), V(x + 0.3f, y + 0.26f)},
                   0.9f, F_GREEN, 3, 0.9f, T_MARKER);
        }
    }
    // the pad
    S.begin(2, 0);
    const Poly pad = {V(-3.5f, 0.85f), V(4.0f, 0.85f), V(3.6f, 1.8f), V(-3.1f, 1.8f), V(-3.5f, 0.85f)};
    H.shape(pad);
    H.colour({pad, {}, {}}, 0x8d93a3, deg(-8.f), 1.f, 0.9f);
    // the launch tower: a lattice, a hut on top, a light
    S.begin(3, 0);
    {
        const float x0 = -3.25f, x1 = -2.05f, y0 = 1.8f, y1 = 13.9f;
        const int bays = 10;
        H.line({V(x0, y0), V(x0, y1)});
        H.line({V(x1, y0), V(x1, y1)});
        for (int i = 0; i <= bays; i++) {
            const float y = y0 + (y1 - y0) * i / bays;
            H.line({V(x0 - 0.05f, y), V(x1 + 0.05f, y)}, 0.8f);
        }
        for (int i = 0; i < bays; i++) {
            const float ya = y0 + (y1 - y0) * i / bays, yb2 = y0 + (y1 - y0) * (i + 1) / bays;
            H.line({V(x0, ya), V(x1, yb2)}, 1.3f, F_RED, 2, 0.85f, T_MARKER);
            H.line({V(x1, ya), V(x0, yb2)}, 1.3f, F_RED, 2, 0.85f, T_MARKER);
        }
        const Poly hut = {V(x0 - 0.25f, y1), V(x1 + 0.25f, y1), V(x1 + 0.25f, y1 + 0.75f), V(x0 - 0.25f, y1 + 0.75f), V(x0 - 0.25f, y1)};
        H.shape(hut);
        H.colour({hut, {}, {}}, F_RED, deg(70.f));
        const float ax = 0.5f * (x0 + x1);
        H.line({V(ax, y1 + 0.75f), V(ax, y1 + 1.7f)});
        const Poly lamp = circle(V(ax, y1 + 1.9f), 0.22f);
        H.shape(lamp, 0.9f);
        H.colour({lamp, {}, {}}, on && (b % 2) ? F_RED : F_YELLOW, 0.3f);
    }
    // the arms: to the rocket, swung back for the lift-off
    S.begin(4, 0, step > 0 ? 1 : 0);
    for (int i = 0; i < 2; i++) {
        const float y = i ? 11.2f : 6.5f;
        const vec2 j = V(-2.05f, y);
        Poly arm = {V(-2.05f, y - 0.17f), V(cx - 1.3f, y - 0.17f), V(cx - 1.3f, y + 0.17f), V(-2.05f, y + 0.17f), V(-2.05f, y - 0.17f)};
        if (step > 0) arm = turn(arm, j, deg(62.f));
        H.shape(arm, 0.9f);
        H.colour({arm, {}, {}}, F_ORANGE, step > 0 ? deg(62.f) : 0.f);
    }
    // the rocket
    S.begin(5, 0, (uint32_t)step);
    {
        Poly body;
        cat(body, cubic(V(cx - 1.25f, yb), V(cx - 1.32f, yb + 3.f), V(cx - 1.32f, yt - 3.f), V(cx - 1.25f, yt), 10));
        cat(body, cubic(V(cx + 1.25f, yt), V(cx + 1.32f, yt - 3.f), V(cx + 1.32f, yb + 3.f), V(cx + 1.25f, yb), 10));
        body.push_back(body[0]);
        Poly noseP = cubic(V(cx - 1.25f, yt), V(cx - 1.25f, yt + 1.9f), V(cx - 0.55f, yt + 3.2f), V(cx, yt + 3.65f), 12);
        cat(noseP, cubic(V(cx, yt + 3.65f), V(cx + 0.55f, yt + 3.2f), V(cx + 1.25f, yt + 1.9f), V(cx + 1.25f, yt), 12));
        noseP.push_back(noseP[0]);
        Poly finL = {V(cx - 1.27f, yb + 3.1f)};
        cat(finL, cubic(V(cx - 1.27f, yb + 3.1f), V(cx - 1.95f, yb + 2.4f), V(cx - 2.55f, yb + 1.3f), V(cx - 2.55f, yb + 0.1f), 8));
        finL.push_back(V(cx - 2.55f, yb - 0.8f));
        finL.push_back(V(cx - 1.27f, yb + 0.05f));
        finL.push_back(finL[0]);
        const Poly finR = mirror(finL, cx);
        const Poly nozzle = {V(cx - 0.7f, yb), V(cx + 0.7f, yb), V(cx + 0.98f, yb - 0.78f), V(cx - 0.98f, yb - 0.78f), V(cx - 0.7f, yb)};
        const Poly band = rect(cx - 1.3f, yt - 0.85f, cx + 1.3f, yt - 0.3f);
        const vec2 wc = V(cx, yb + 6.55f);
        const Poly frame = circle(wc, 1.0f), glass = circle(wc, 0.72f);
        const Poly glint = ellipse(wc + V(-0.27f, 0.25f), 0.13f, 0.3f, 0.7f);
        // outlines, then the colouring
        H.shape(noseP);
        H.line(body);
        H.shape(band, 0.9f);
        H.shape(frame);
        H.shape(glass, 0.9f);
        H.shape(finL);
        H.shape(finR);
        H.shape(nozzle);
        H.colour({noseP, {}, {}}, nose, deg(-30.f));
        H.colour({band, {}, {}}, F_YELLOW, deg(5.f));
        H.colour({frame, {glass}, {}}, F_YELLOW, deg(40.f));
        H.colour({glass, {glint}, {}}, F_BLUE, deg(-40.f));
        H.colour({finL, {}, {}}, fin, deg(55.f));
        H.colour({finR, {}, {}}, fin, deg(-55.f));
        H.colour({nozzle, {}, {}}, F_PURPLE, 0.f, 1.f, 0.8f);
        // the body's roundness: a light blue down its right side
        H.colour({body, {}, {rect(cx + 0.55f, yb - 1.f, cx + 2.f, yt + 1.f)}}, F_BLUE, deg(80.f), 1.f, 0.4f);
        // its name, down the body
        const char* name[6] = {"R", "E", "F", "A", "L", "L"};
        for (int i = 0; i < 6; i++) S.text(V(cx, yb + 4.95f - i * 0.78f), name[i], nose, 0.95f);
    }
    // the flame and the smoke, from the ignition
    if (on) {
        const bool big = step > 0;
        S.begin(6, 0, (uint32_t)(b % 2) + 2u * (uint32_t)step);
        const float L = (big ? std::min(3.2f, yb - 0.78f - 1.7f) : 0.9f) * (b % 2 ? 1.12f : 1.f), W = big ? 0.95f : 0.75f;
        auto flame = [&](float k, float ph) {
            Poly p;
            for (int i = 0; i <= 10; i++) { // down the left side, up the right
                const float t = i / 10.f;
                p.push_back(V(cx - W * k * std::pow(1.f - t, 0.6f) * (1.f + 0.18f * std::sin(t * 11.f + ph)), yb - 0.78f - L * k * t));
            }
            for (int i = 9; i >= 0; i--) {
                const float t = i / 10.f;
                p.push_back(V(cx + W * k * std::pow(1.f - t, 0.6f) * (1.f + 0.18f * std::sin(t * 9.f + ph + 2.f)), yb - 0.78f - L * k * t));
            }
            p.push_back(p[0]);
            return p;
        };
        const float ph = (b % 2) ? 1.7f : 0.f;
        const Poly outer = flame(1.f, ph), inner = flame(0.55f, ph + 1.f);
        H.shape(outer, 1.2f, F_RED, 1, 0.9f, T_MARKER);
        H.colour({outer, {inner}, {}}, F_ORANGE, deg(85.f));
        H.colour({inner, {}, {}}, F_YELLOW, deg(80.f));
        // smoke rolling out over the pad, bigger at the lift-off, a new cloud every bar
        const int bar = (int)(alive / 4.f);
        S.begin(7, 0, (uint32_t)bar);
        std::vector<std::pair<vec2, float>> cl;
        const int n = big ? 7 : 4;
        for (int i = 0; i < n; i++) {
            const float sgn = (i % 2) ? 1.f : -1.f;
            const float d = 1.1f + 0.55f * (i / 2) * (big ? 1.3f : 1.f) + S.r.range(0.f, 0.3f);
            const float x = std::clamp(cx + sgn * d, -hw + 0.7f, hw - 0.7f);
            cl.push_back({V(x, 1.15f + S.r.range(0.f, 0.5f) + (i / 2) * 0.12f), S.r.range(0.55f, 0.85f) * (big ? 1.15f : 1.f)});
        }
        puffs(H, cl, 0xc9ccd4, 0.8f, deg(20.f));
    }
    // the sky: the moon, a few stars (one twinkling on each beat)
    S.begin(8, 0);
    {
        const vec2 mc = V(-2.6f, top - 2.3f);
        const Poly a = circle(mc, 1.05f), bcut = circle(mc + V(0.5f, 0.32f), 0.9f);
        std::vector<Poly> parts = visible(a, {bcut});
        for (const Poly& q : visible(bcut, {a}, true)) parts.push_back(q);
        for (const Poly& q : parts) H.line(q);
        H.colour({a, {bcut}, {}}, F_YELLOW, deg(30.f));
    }
    const vec2 starsL[3] = {V(3.2f, top - 1.6f), V(-0.9f, top - 0.9f), V(3.9f, top - 6.2f)};
    for (int i = 0; i < 3; i++) {
        const bool tw = on && b % 7 == i;
        S.begin(9 + i, 0, tw ? 1 : 0);
        const Poly st = star(starsL[i], 0.42f, S.r.range(-0.2f, 0.2f), tw);
        H.shape(st, 0.9f);
        H.colour({st, {}, {}}, i == 1 ? F_PINK : F_YELLOW, 0.5f);
    }

    // ---- right: space
    const float topR = std::min(P.y1[1] - 0.3f, 23.f);
    // the ringed planet
    S.begin(20, 1);
    {
        const vec2 pc = V(0.25f, topR - 4.6f);
        const float R = 2.55f, tilt = deg(-17.f);
        const uint32_t bands[3][5] = {{F_YELLOW, F_ORANGE, F_YELLOW, F_RED, F_ORANGE},
                                      {F_PINK, F_PURPLE, F_PINK, F_BLUE, F_PURPLE},
                                      {F_GREEN, F_BLUE, F_GREEN, F_YELLOW, F_BLUE}};
        const uint32_t* bc = bands[drop % 3];
        const uint32_t ringA = drop % 3 == 1 ? F_ORANGE : F_PURPLE, ringB = drop % 3 == 1 ? F_YELLOW : F_BLUE;
        const Poly disc = circle(pc, R, 56);
        const Poly front = ringBand(pc, 4.35f, 1.12f, 3.25f, 0.8f, PI, TAU, tilt);
        const Poly outerE = ellipse(pc, 4.35f, 1.12f, tilt, 60), midE = ellipse(pc, 3.82f, 0.97f, tilt, 56),
                   innerE = ellipse(pc, 3.25f, 0.8f, tilt, 52);
        // outlines: the disc where the ring doesn't pass in front, the ring where the planet doesn't hide it
        H.lineOut(disc, {front});
        for (const Poly* e : {&outerE, &innerE}) H.lineOut(*e, {disc, front}, 1.f);
        H.line(visible(arc(pc, 4.35f, 1.12f, PI, TAU, 40, tilt), {})[0]);
        H.line(visible(arc(pc, 3.25f, 0.8f, PI, TAU, 36, tilt), {})[0]);
        H.lineOut(midE, {disc, front}, 0.6f);
        H.line(arc(pc, 3.82f, 0.97f, PI, TAU, 36, tilt), 0.6f);
        // the bands, across the planet's tilt
        const float edges[6] = {-R - 0.1f, -1.5f, -0.45f, 0.5f, 1.5f, R + 0.1f};
        for (int i = 0; i < 5; i++) {
            const Poly strip = turn(rect(pc.x - R - 1.f, pc.y + edges[i], pc.x + R + 1.f, pc.y + edges[i + 1]), pc, tilt);
            if (i > 0)
                for (const Poly& run : visible(turn({V(pc.x - R, pc.y + edges[i]), V(pc.x + R, pc.y + edges[i])}, pc, tilt), {disc}, true))
                    H.lineOut(run, {front}, 0.6f);
            H.colour({disc, {front}, {strip}}, bc[i], tilt + deg(i % 2 ? 8.f : -8.f));
        }
        // the ring: behind the planet at the back, in front of it at the front
        const Poly backA = ringBand(pc, 4.35f, 1.12f, 3.82f, 0.97f, 0.f, PI, tilt), backB = ringBand(pc, 3.82f, 0.97f, 3.25f, 0.8f, 0.f, PI, tilt);
        const Poly frontA = ringBand(pc, 4.35f, 1.12f, 3.82f, 0.97f, PI, TAU, tilt), frontB = ringBand(pc, 3.82f, 0.97f, 3.25f, 0.8f, PI, TAU, tilt);
        H.colour({backA, {disc}, {}}, ringA, tilt + deg(60.f));
        H.colour({backB, {disc}, {}}, ringB, tilt + deg(-60.f));
        H.colour({frontA, {}, {}}, ringA, tilt + deg(60.f));
        H.colour({frontB, {}, {}}, ringB, tilt + deg(-60.f));
    }
    // the Earth
    S.begin(21, 1);
    {
        const vec2 c = V(2.45f, topR - 10.5f);
        const Poly disc = circle(c, 1.3f);
        H.shape(disc);
        std::vector<Poly> land;
        const vec2 lc[3] = {V(-0.45f, 0.35f), V(0.5f, -0.35f), V(0.2f, 0.75f)};
        for (int i = 0; i < 3; i++) {
            Poly blob;
            const float rr = i == 2 ? 0.28f : 0.5f;
            for (int k = 0; k <= 14; k++) {
                const float a = k * TAU / 14.f;
                const float q = rr * (1.f + 0.28f * std::sin(a * 3.f + i * 1.7f) + 0.12f * std::sin(a * 5.f + i));
                blob.push_back(c + lc[i] + V(q * std::cos(a), q * std::sin(a) * 0.85f));
            }
            land.push_back(blob);
            H.lineOut(blob, {}, 0.7f);
        }
        H.colour({disc, land, {}}, F_BLUE, deg(-20.f));
        for (const Poly& l : land) H.colour({l, {}, {disc}}, F_GREEN, deg(35.f));
    }
    // a crescent moon
    S.begin(22, 1);
    {
        const vec2 mc = V(-2.55f, topR - 9.8f);
        const Poly a = circle(mc, 1.2f), bcut = circle(mc + V(0.55f, 0.35f), 1.05f);
        for (const Poly& q : visible(a, {bcut})) H.line(q);
        for (const Poly& q : visible(bcut, {a}, true)) H.line(q);
        H.colour({a, {bcut}, {}}, F_YELLOW, deg(-35.f));
        // a sleepy eye and a smile
        H.line(arc(mc + V(-0.62f, 0.25f), 0.18f, 0.12f, PI * 1.1f, PI * 1.9f, 6), 0.8f, 0, 3);
        H.line(arc(mc + V(-0.55f, -0.35f), 0.25f, 0.18f, PI * 1.05f, PI * 1.7f, 6), 0.8f, 0, 3);
    }
    // the flying saucer, rocking from bar to bar
    {
        const int bar = on ? (int)(alive / 4.f) : 0;
        S.begin(23, 1, (uint32_t)(bar % 2));
        const vec2 c = V(0.2f, topR - 14.6f) + V(0.f, on && bar % 2 ? 0.25f : 0.f);
        const float tl = on ? (bar % 2 ? deg(7.f) : deg(-5.f)) : 0.f;
        const Poly saucer = turn(ellipse(c, 1.65f, 0.45f, 0.f, 40), c, tl);
        Poly dome = arc(c + V(0.f, 0.15f), 0.82f, 0.85f, 0.f, PI, 20);
        dome.push_back(dome[0]);
        dome = turn(dome, c, tl);
        const vec2 hc = c + rot(V(0.f, 0.55f), tl);
        const Poly head = circle(hc, 0.3f);
        H.lineOut(saucer, {dome});
        H.lineOut(dome, {}, 0.9f);
        H.shape(head, 0.8f);
        for (int s = -1; s <= 1; s += 2) { // antennae
            const vec2 a0 = hc + rot(V(s * 0.12f, 0.26f), tl), a1 = hc + rot(V(s * 0.28f, 0.62f), tl);
            H.line({a0, a1}, 0.7f, 0, 3);
            H.line(circle(a1, 0.06f, 8), 0.8f, 0, 3);
        }
        H.line({hc + rot(V(-0.12f, 0.06f), tl), hc + rot(V(-0.1f, 0.13f), tl)}, 1.1f, 0, 3);
        H.line({hc + rot(V(0.1f, 0.06f), tl), hc + rot(V(0.12f, 0.13f), tl)}, 1.1f, 0, 3);
        H.colour({saucer, {dome}, {}}, F_PINK, tl + deg(10.f));
        H.colour({dome, {head}, {}}, F_BLUE, tl + deg(-45.f), 1.f, 0.4f);
        H.colour({head, {}, {}}, F_GREEN, 0.4f);
        for (int i = 0; i < 4; i++) { // its lights
            const Poly l = circle(c + rot(V(-1.1f + i * 0.73f, -0.12f), tl), 0.13f, 10);
            H.shape(l, 0.7f, 0, 3);
            H.colour({l, {}, {}}, F_YELLOW, 0.f, 1.f, 1.f, 3);
        }
    }
    // a comet
    S.begin(24, 1);
    {
        const vec2 h = V(-2.4f, topR - 18.4f);
        const vec2 d = unit(V(0.85f, 0.55f));
        const vec2 n = V(-d.y, d.x);
        const uint32_t tc[3] = {F_RED, F_YELLOW, F_BLUE};
        for (int i = 0; i < 3; i++) {
            Poly t;
            for (int k = 0; k <= 12; k++) {
                const float u = k / 12.f;
                t.push_back(h + d * (0.4f + 3.6f * u) + n * ((i - 1) * (0.22f + 0.3f * u) + 0.12f * std::sin(u * 9.f + i)));
            }
            H.line(t, 2.f, tc[i], 2, 0.85f, T_MARKER);
        }
        const Poly st = star(h, 0.55f, 0.2f, false);
        H.shape(st);
        H.colour({st, {}, {}}, F_YELLOW, 0.3f);
    }
    // stars (one twinkling on each beat)
    // a satellite, its solar panels
    S.begin(25, 1);
    {
        const vec2 c = V(1.3f, topR - 21.0f);
        const float tl = deg(-18.f);
        if (c.y > P.y0[1] + 1.2f) {
            const Poly body = turn(rect(c.x - 0.45f, c.y - 0.38f, c.x + 0.45f, c.y + 0.38f), c, tl);
            H.shape(body);
            H.colour({body, {}, {}}, F_YELLOW, tl + deg(70.f));
            for (int s = -1; s <= 1; s += 2) {
                const float x0 = c.x + s * 0.62f, x1 = c.x + s * 2.35f;
                const Poly panel = turn(rect(std::min(x0, x1), c.y - 0.42f, std::max(x0, x1), c.y + 0.42f), c, tl);
                H.line(turn({V(c.x + s * 0.45f, c.y), V(x0, c.y)}, c, tl), 0.9f);
                H.shape(panel, 0.9f);
                for (int k = 1; k < 3; k++) {
                    const float x = x0 + (x1 - x0) * k / 3.f;
                    H.line(turn({V(x, c.y - 0.42f), V(x, c.y + 0.42f)}, c, tl), 0.6f, 0, 3);
                }
                H.line(turn({V(std::min(x0, x1), c.y), V(std::max(x0, x1), c.y)}, c, tl), 0.6f, 0, 3);
                H.colour({panel, {}, {}}, F_DBLUE, tl + deg(s * 30.f), 1.f, 0.85f);
            }
            const vec2 top2 = c + rot(V(0.f, 0.38f), tl);
            H.line({top2, top2 + rot(V(0.f, 0.55f), tl)}, 0.9f);
            const Poly dish = turn(arc(top2 + rot(V(0.f, 0.75f), tl), 0.42f, 0.22f, PI, TAU, 10), top2 + rot(V(0.f, 0.75f), tl), tl + 0.5f);
            H.line(dish, 0.9f);
        }
    }
    const vec2 starsR[7] = {V(-3.5f, topR - 1.1f), V(3.7f, topR - 8.4f), V(-3.8f, topR - 6.6f), V(3.6f, topR - 14.0f),
                            V(-3.3f, topR - 13.4f), V(3.8f, topR - 18.6f), V(-2.6f, topR - 22.2f)};
    for (int i = 0; i < 7; i++) {
        if (starsR[i].y < P.y0[1] + 0.6f) continue;
        const bool tw = on && b % 7 == i + 3;
        S.begin(30 + i, 1, tw ? 1 : 0);
        const Poly st = star(starsR[i], 0.36f + 0.06f * (i % 3), S.r.range(-0.3f, 0.3f), tw);
        H.shape(st, 0.9f);
        H.colour({st, {}, {}}, (i % 3 == 1) ? F_PINK : (i % 3 == 2 ? F_ORANGE : F_YELLOW), 0.5f);
    }
}

// ================================================================ PENCIL: the hot-air balloon
const uint32_t P_GRAPHITE = 0x404147, P_LIGHT = 0x7b7d84, P_RED = 0xd63c30, P_BLUE = 0x2a82cc, P_OCHRE = 0xd99c18,
               P_GREEN = 0x3f9944, P_ORANGE = 0xe25e12, P_BROWN = 0x8a5a2e, P_CREAM = 0xe6c34a;

// A dimension: a line with its arrowheads, broken where its figure is written.
void dimension(Hand& H, vec2 a, vec2 b, float gap) {
    const vec2 d = unit(b - a), n = V(-d.y, d.x);
    const float L = len(b - a);
    const vec2 m = (a + b) * 0.5f;
    if (gap > 0.f) {
        H.line({a, m - d * (gap * 0.5f)}, 0.5f, 0, 3, 0.85f, -1, 0.3f);
        H.line({m + d * (gap * 0.5f), b}, 0.5f, 0, 3, 0.85f, -1, 0.3f);
    } else {
        H.line({a, b}, 0.5f, 0, 3, 0.85f, -1, 0.3f);
    }
    (void)L;
    for (int e = 0; e < 2; e++) {
        const vec2 p = e ? b : a, dd = e ? d * -1.f : d;
        H.line({p + dd * 0.32f + n * 0.1f, p, p + dd * 0.32f - n * 0.1f}, 0.6f, 0, 3, 0.9f, -1, 0.2f);
    }
}

void pencil(Sheet& S, const Panels& P, float beat, int drop) {
    Hand H{S, 1, P_GRAPHITE, T_PENCIL, 0.06f};
    const float alive = std::max(0.f, beat - ALIVE);
    const int b = (int)std::floor(alive);
    const bool on = beat >= ALIVE;
    const int bar = on ? (int)(alive / 4.f) : 0;
    const uint32_t schemes[3][3] = {{P_RED, P_CREAM, P_BLUE}, {P_ORANGE, P_CREAM, P_GREEN}, {P_BLUE, P_CREAM, P_RED}};
    const uint32_t* gc = schemes[drop % 3];
    const float hw = P.hw[0];

    // ---- the elevation (left)
    const float c = -0.55f, yc = 13.35f, R = 3.5f, ym = 7.f, Xm = 0.95f, k = 0.1f;
    const float ys = yc - R * std::sin(deg(35.f)), Xs = R * std::cos(deg(35.f));
    auto X = [&](float y) {
        if (y >= ys) return std::sqrt(std::max(0.f, R * R - (y - yc) * (y - yc)));
        const float h = ys - ym, t = (y - ym) / h;
        const float m0 = 0.12f * h, m1 = (std::sin(deg(35.f)) / std::cos(deg(35.f))) * h;
        const float t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * Xm + (t3 - 2 * t2 + t) * m0 + (-2 * t3 + 3 * t2) * Xs + (t3 - t2) * m1;
    };
    // a point of the envelope at height y on the meridian phi (seen from a little below)
    auto at = [&](float y, float phi) { const float x = X(y); return V(c + x * std::sin(phi), y + k * x * std::cos(phi)); };
    auto meridian = [&](float phi) {
        Poly p;
        for (int i = 0; i <= 26; i++) {
            const float y = yc + R - (yc + R - ym) * i / 26.f;
            p.push_back(at(y, phi));
        }
        return p;
    };
    // the construction: the axis, the compass circle, the equator, the tangents to the mouth
    S.begin(40, 0);
    H.line({V(c, 0.6f), V(c, yc + R + 1.4f)}, 0.6f, P_LIGHT, 0, 0.7f, -1, 0.2f);
    S.out.back().dash = 0.55f;
    H.line(circle(V(c, yc), R, 64), 0.6f, P_LIGHT, 0, 0.7f, -1, 0.5f);
    H.line({V(c - R - 0.7f, yc), V(c + R + 0.7f, yc)}, 0.6f, P_LIGHT, 0, 0.7f, -1, 0.2f);
    H.line({V(c - Xs, ys), V(c - Xm, ym), V(c - Xm + 0.2f, ym - 0.9f)}, 0.6f, P_LIGHT, 0, 0.7f);
    H.line({V(c + Xs, ys), V(c + Xm, ym), V(c + Xm - 0.2f, ym - 0.9f)}, 0.6f, P_LIGHT, 0, 0.7f);
    H.line({V(c - 1.7f, ym), V(c + 1.7f, ym)}, 0.6f, P_LIGHT, 0, 0.7f, -1, 0.2f);
    // the envelope: its outline, its seams, its load tapes
    S.begin(41, 0);
    Poly env = meridian(-PI * 0.5f);
    env = rev(env);
    cat(env, meridian(PI * 0.5f));
    H.line(env, 1.15f);
    const Poly mouth = ellipse(V(c, ym + 0.02f), Xm, 0.2f, 0.f, 28);
    H.shape(mouth, 0.8f);
    for (int i = 0; i < 6; i++) H.line(meridian(deg(-75.f + 30.f * i)), 0.6f);
    for (float y : {15.4f, 11.6f, 9.0f}) {
        Poly lat;
        for (int i = 0; i <= 24; i++) lat.push_back(at(y, -PI * 0.5f + PI * i / 24.f));
        H.line(lat, 0.45f, 0, 1, 0.8f);
    }
    // the skirt, the cables, the burner, the basket
    S.begin(42, 0);
    const Poly skirt = {V(c - Xm, ym), V(c + Xm, ym), V(c + 0.78f, 6.25f), V(c - 0.78f, 6.25f), V(c - Xm, ym)};
    H.shape(skirt, 0.9f);
    const float bx0 = c - 0.55f, bx1 = c + 0.55f;
    for (float x : {-0.78f, -0.3f, 0.3f, 0.78f}) H.line({V(c + x, 6.25f), V(c + x * 0.66f, 4.3f)}, 0.5f);
    const Poly frameB = rect(bx0, 3.85f, bx1, 4.25f);
    H.shape(frameB, 0.8f);
    const Poly can1 = rect(c - 0.42f, 4.25f, c - 0.08f, 4.62f), can2 = rect(c + 0.08f, 4.25f, c + 0.42f, 4.62f);
    H.shape(can1, 0.6f);
    H.shape(can2, 0.6f);
    H.line({V(bx0, 3.85f), V(c - 0.85f, 3.12f)}, 0.5f);
    H.line({V(bx1, 3.85f), V(c + 0.85f, 3.12f)}, 0.5f);
    const Poly basket = {V(c - 0.92f, 3.12f), V(c + 0.92f, 3.12f), V(c + 0.78f, 1.5f), V(c - 0.78f, 1.5f), V(c - 0.92f, 3.12f)};
    const Poly rim = {V(c - 0.95f, 3.14f), V(c + 0.95f, 3.14f), V(c + 0.93f, 2.86f), V(c - 0.93f, 2.86f), V(c - 0.95f, 3.14f)};
    H.shape(basket);
    H.shape(rim, 0.7f);
    for (int s = -1; s <= 1; s += 2) { // sandbags
        const vec2 hook = V(c + s * 0.92f, 3.f), bag = V(c + s * 1.22f, 2.15f);
        H.line({hook, bag + V(0.f, 0.22f)}, 0.4f);
        Poly sb = arc(bag, 0.2f, 0.24f, PI * 0.5f + 0.35f, PI * 2.5f - 0.35f, 14);
        sb.push_back(bag + V(0.f, 0.36f));
        sb.push_back(sb[0]);
        H.shape(sb, 0.7f);
        H.colour({sb, {}, {}}, P_OCHRE, deg(30.f), 1.2f, 0.8f);
    }
    // the ground and the mooring line
    S.begin(43, 0);
    H.line({V(-hw + 0.3f, 0.4f), V(hw - 0.3f, 0.4f)}, 0.8f);
    for (float x = -hw + 0.55f; x < hw - 0.3f; x += 0.38f) H.line({V(x, 0.4f), V(x - 0.3f, 0.08f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
    H.line({V(c - 0.6f, 1.55f), V(c - 1.6f, 0.95f), V(c - 2.6f, 0.42f)}, 0.4f, 0, 3, 0.8f, -1, 0.3f);
    S.out.back().dash = 0.22f;
    // the colouring: the gores (one colour in three around the balloon), the shaded side cross-hatched
    S.begin(44, 0);
    for (int g = 0; g < 7; g++) {
        const float pa = g == 0 ? -PI * 0.5f : deg(-75.f + 30.f * (g - 1)), pb = g == 6 ? PI * 0.5f : deg(-75.f + 30.f * g);
        Poly gore = meridian(pa);
        cat(gore, rev(meridian(pb)));
        gore.push_back(gore[0]);
        const uint32_t col = gc[(g + 1) % 3];
        H.colour({gore, {}, {}}, col, deg(78.f + (g - 3) * 6.f), col == P_CREAM ? 0.8f : 1.f);
        if (g >= 4) H.shade({gore, {}, {}}, deg(-40.f + g * 4.f), 0.8f + 0.35f * (g - 4), 1.f);
        if (g == 0) H.shade({gore, {}, {}}, deg(-30.f), 0.6f, 0.8f);
    }
    {
        Poly sh = env; // the underside of the envelope, in shadow
        sh.push_back(sh[0]);
        H.shade({sh, {}, {rect(c - 5.f, ym - 1.f, c + 5.f, ys - 0.6f)}}, deg(35.f), 0.5f, 0.8f);
    }
    S.begin(45, 0);
    H.colour({skirt, {}, {}}, P_ORANGE, deg(10.f));
    H.colour({basket, {rim}, {}}, P_BROWN, deg(40.f), 0.9f);
    H.colour({basket, {rim}, {}}, P_OCHRE, deg(-40.f), 0.9f, 0.8f);
    H.colour({rim, {}, {}}, P_BROWN, deg(5.f), 1.4f);
    H.shade({basket, {}, {rect(c + 0.3f, 1.f, c + 2.f, 3.3f)}}, deg(60.f), 0.9f);
    // the burner's flame, roaring on the beat
    {
        const int pose = on ? 1 + b % 3 : 0;
        S.begin(46, 0, (uint32_t)pose);
        const float fl = pose == 0 ? 0.9f : (pose == 1 ? 1.5f : pose == 2 ? 1.15f : 1.7f);
        Poly f;
        for (int i = 0; i <= 8; i++) {
            const float t = i / 8.f;
            f.push_back(V(c - 0.3f * std::sin(PI * std::pow(t, 0.7f)) * (1.f + 0.2f * std::sin(t * 9.f + pose)), 4.62f + fl * t));
        }
        for (int i = 7; i >= 0; i--) {
            const float t = i / 8.f;
            f.push_back(V(c + 0.3f * std::sin(PI * std::pow(t, 0.7f)) * (1.f + 0.2f * std::sin(t * 7.f + pose * 2.f)), 4.62f + fl * t));
        }
        f.push_back(f[0]);
        H.shape(f, 0.7f, P_ORANGE, 1, 1.f, T_CPENCIL);
        H.colour({f, {}, {}}, P_OCHRE, deg(80.f), 1.2f);
        H.colour({f, {}, {rect(c - 1.f, 4.6f, c + 1.f, 4.62f + fl * 0.45f)}}, P_ORANGE, deg(-70.f), 1.2f);
    }
    // the dimensions, the callouts
    S.begin(47, 0);
    {
        const float xd = std::min(hw - 0.6f, 3.85f);
        H.line({V(c + 1.0f, 1.5f), V(xd + 0.3f, 1.5f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        H.line({V(c + 0.3f, yc + R), V(xd + 0.3f, yc + R)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        dimension(H, V(xd, 1.5f), V(xd, yc + R), 1.3f);
        S.text(V(xd, 0.5f * (1.5f + yc + R)), "21 m", P_GRAPHITE, 0.62f);
        const float yd = yc + R + 1.2f;
        H.line({V(c - R, yc + 0.4f), V(c - R, yd + 0.3f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        H.line({V(c + R, yc + 0.4f), V(c + R, yd + 0.3f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        dimension(H, V(c - R, yd), V(c + R, yd), 0.f);
        S.text(V(c + 0.25f, yd + 0.55f), "16 m", P_GRAPHITE, 0.62f);
        const vec2 dia = V(c - 0.75f, yd + 0.55f); // the diameter's sign, by hand
        H.line(circle(dia, 0.19f, 14), 0.5f, 0, 3, 0.9f, -1, 0.2f);
        H.line({dia + V(-0.24f, -0.26f), dia + V(0.24f, 0.26f)}, 0.5f, 0, 3, 0.9f, -1, 0.2f);
        // the detail's circle, its letter; the parts' numbers
        H.line(circle(V(c, 4.75f), 1.2f, 40), 0.45f, 0, 3, 0.85f, -1, 0.4f);
        H.line({V(c - 0.85f, 5.6f), V(c - 1.6f, 6.4f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        S.text(V(c - 1.85f, 6.65f), "A", P_GRAPHITE, 0.6f);
        const float nx = -std::min(hw, 4.9f) + 0.75f;
        const vec2 n1 = V(nx, 10.2f), n2 = V(nx, 2.6f);
        H.line(circle(n1, 0.34f, 20), 0.45f, 0, 3, 0.9f, -1, 0.2f);
        H.line({n1 + V(0.34f, 0.f), at(10.2f, deg(-60.f))}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        S.text(n1, "1", P_GRAPHITE, 0.5f);
        H.line(circle(n2, 0.34f, 20), 0.45f, 0, 3, 0.9f, -1, 0.2f);
        H.line({n2 + V(0.34f, 0.f), V(c - 0.6f, 2.3f)}, 0.4f, 0, 3, 0.8f, -1, 0.2f);
        S.text(n2, "2", P_GRAPHITE, 0.5f);
    }

    // ---- the details (right)
    const float topR = std::min(P.y1[1] - 0.4f, 23.f), botR = std::max(P.y0[1] + 0.4f, -2.3f);
    // the plan view: the gores seen from above, turning a gore every bar
    {
        const vec2 pc = V(0.f, topR - 3.1f);
        const float r = 2.45f, r0 = 0.32f;
        S.begin(50, 1);
        H.line({V(pc.x - r - 0.5f, pc.y), V(pc.x + r + 0.5f, pc.y)}, 0.4f, P_LIGHT, 0, 0.5f, -1, 0.2f);
        S.out.back().dash = 0.5f;
        H.line({V(pc.x, pc.y - r - 0.5f), V(pc.x, pc.y + r + 0.5f)}, 0.4f, P_LIGHT, 0, 0.5f, -1, 0.2f);
        S.out.back().dash = 0.5f;
        S.begin(51, 1, (uint32_t)(bar % 12));
        const float a0 = deg(30.f) * (float)(bar % 12) + deg(8.f);
        H.shape(circle(pc, r, 60), 1.1f);
        H.shape(circle(pc, r0, 16), 0.6f);
        for (int g = 0; g < 12; g++) {
            const float a = a0 + g * deg(30.f);
            H.line({pc + V(r0 * std::cos(a), r0 * std::sin(a)), pc + V(r * std::cos(a), r * std::sin(a))}, 0.55f);
        }
        for (int g = 0; g < 12; g++) {
            const float a = a0 + g * deg(30.f);
            Poly w = {pc};
            cat(w, arc(pc, r, r, a, a + deg(30.f), 8));
            w.push_back(pc);
            const uint32_t col = gc[g % 3];
            H.colour({w, {circle(pc, r0, 16)}, {}}, col, a + deg(105.f), col == P_CREAM ? 0.8f : 1.f, 0.9f);
        }
        H.shade({circle(pc, r, 40), {}, {turn(rect(pc.x - 4.f, pc.y - 4.f, pc.x + 4.f, pc.y), pc, deg(-40.f))}}, deg(50.f), 0.45f, 0.7f);
        S.begin(52, 1);
        S.text(V(0.f, topR - 6.25f), "PLAN", P_GRAPHITE, 0.62f);
        H.line({V(-0.75f, topR - 6.75f), V(0.8f, topR - 6.72f)}, 0.5f, 0, 3, 0.8f);
    }
    // detail A: the burner, twice the size, its flames roaring on the beat
    {
        const vec2 dc = V(0.f, topR - 11.2f);
        const float r = 2.5f;
        const Poly ring = circle(dc, r, 60);
        S.begin(53, 1);
        H.shape(ring, 0.9f);
        const auto in = [&](const Poly& p, float wk, uint32_t col = 0) { H.lineOut(p, {ring}, wk, col, 1, 1.f, true); };
        const Poly bar0 = rect(dc.x - 1.9f, dc.y - 1.45f, dc.x + 1.9f, dc.y - 1.0f);
        in(bar0, 0.8f);
        const Poly c1 = rect(dc.x - 1.25f, dc.y - 1.0f, dc.x - 0.25f, dc.y + 0.15f), c2 = rect(dc.x + 0.25f, dc.y - 1.0f, dc.x + 1.25f, dc.y + 0.15f);
        in(c1, 0.8f);
        in(c2, 0.8f);
        for (int s = -1; s <= 1; s += 2) // the coils
            for (int i = 0; i < 4; i++) in(ellipse(V(dc.x + s * 0.75f, dc.y - 0.8f + i * 0.28f), 0.62f, 0.11f, 0.f, 20), 0.5f);
        in(cubic(V(dc.x - 1.f, dc.y - 1.45f), V(dc.x - 1.2f, dc.y - 2.2f), V(dc.x - 2.2f, dc.y - 1.7f), V(dc.x - 2.8f, dc.y - 2.4f), 10), 0.6f);
        in(cubic(V(dc.x + 1.f, dc.y - 1.45f), V(dc.x + 1.2f, dc.y - 2.2f), V(dc.x + 2.2f, dc.y - 1.7f), V(dc.x + 2.8f, dc.y - 2.4f), 10), 0.6f);
        H.colour({bar0, {}, {ring}}, P_LIGHT, deg(45.f), 0.7f, 0.6f);
        H.shade({c1, {}, {ring}}, deg(70.f), 0.6f, 0.7f);
        H.shade({c2, {}, {ring}}, deg(70.f), 0.6f, 0.7f);
        const int pose = on ? 1 + b % 3 : 0;
        S.begin(54, 1, (uint32_t)pose);
        for (int s = -1; s <= 1; s += 2) {
            const float h = (pose == 0 ? 1.3f : pose == 1 ? 2.3f : pose == 2 ? 1.8f : 2.6f) * (s < 0 ? 1.f : 0.9f);
            const float x0 = dc.x + s * 0.75f, y0 = dc.y + 0.15f;
            Poly f;
            for (int i = 0; i <= 9; i++) {
                const float t = i / 9.f;
                f.push_back(V(x0 - 0.45f * std::sin(PI * std::pow(t, 0.65f)) * (1.f + 0.25f * std::sin(t * 10.f + pose + s)), y0 + h * t));
            }
            for (int i = 8; i >= 0; i--) {
                const float t = i / 9.f;
                f.push_back(V(x0 + 0.45f * std::sin(PI * std::pow(t, 0.65f)) * (1.f + 0.25f * std::sin(t * 8.f + pose * 2.f - s)), y0 + h * t));
            }
            f.push_back(f[0]);
            H.lineOut(f, {ring}, 0.7f, P_ORANGE, 1, 1.f, true);
            H.colour({f, {}, {ring}}, P_OCHRE, deg(80.f), 1.2f);
            H.colour({f, {}, {ring, rect(x0 - 1.f, y0, x0 + 1.f, y0 + h * 0.5f)}}, P_ORANGE, deg(-70.f), 1.3f);
            H.colour({f, {}, {ring, rect(x0 - 1.f, y0, x0 + 1.f, y0 + 0.3f)}}, P_BLUE, deg(10.f), 1.3f);
        }
        S.begin(55, 1);
        S.text(V(0.f, dc.y - r - 0.6f), "DETAIL A  2:1", P_GRAPHITE, 0.6f);
    }
    // a gore's pattern, cut flat
    {
        const float gy = topR - 17.0f, gx = std::min(P.hw[1] - 0.6f, 3.9f);
        S.begin(56, 1);
        Poly lens;
        for (int i = 0; i <= 20; i++) {
            const float t = i / 20.f;
            lens.push_back(V(-gx + 2.f * gx * t, gy + 0.62f * std::pow(std::max(0.f, std::sin(PI * t)), 0.8f)));
        }
        for (int i = 19; i >= 0; i--) {
            const float t = i / 20.f;
            lens.push_back(V(-gx + 2.f * gx * t, gy - 0.62f * std::pow(std::max(0.f, std::sin(PI * t)), 0.8f)));
        }
        lens.push_back(lens[0]);
        H.shape(lens);
        H.line({V(-gx - 0.4f, gy), V(gx + 0.4f, gy)}, 0.4f, P_LIGHT, 0, 0.55f, -1, 0.2f);
        S.out.back().dash = 0.45f;
        H.colour({lens, {}, {}}, gc[0], deg(8.f));
        dimension(H, V(-gx, gy - 1.1f), V(gx, gy - 1.1f), 1.4f);
        S.text(V(0.f, gy - 1.1f), "11 m", P_GRAPHITE, 0.55f);
        S.text(V(0.f, gy + 1.15f), "GORE x 12", P_GRAPHITE, 0.55f);
    }
    // the title block
    {
        const float x0 = -std::min(P.hw[1] - 0.4f, 4.4f), x1 = -x0, y0 = botR, y1 = botR + 3.2f;
        S.begin(57, 1);
        H.shape(rect(x0, y0, x1, y1), 1.2f);
        H.line({V(x0, y0 + 1.35f), V(x1, y0 + 1.35f)}, 0.7f);
        H.line({V(0.9f, y0), V(0.9f, y0 + 1.35f)}, 0.7f);
        S.text(V(0.f, y0 + 2.3f), "HOT-AIR BALLOON", P_GRAPHITE, 0.72f);
        S.text(V(0.5f * (x0 + 0.9f), y0 + 0.68f), "SCALE 1:200", P_GRAPHITE, 0.5f);
        char dwg[32];
        std::snprintf(dwg, sizeof(dwg), "DWG %02d", drop % 100);
        S.text(V(0.5f * (x1 + 0.9f), y0 + 0.68f), dwg, P_RED, 0.5f);
    }
}

// ================================================================ CHALK: the solar system
const uint32_t C_WHITE = 0xeeefe8, C_YELLOW = 0xf6e27a, C_PINK = 0xf79494, C_BLUE = 0x96d1f2, C_GREEN = 0xa2e39a,
               C_PURPLE = 0xd2a9f2, C_ORANGE = 0xf8b673, C_PERI = 0x8ca8f8, C_GREY = 0xb5bcb6;

// A planet lit from the Sun: coloured in on its lit side, a few lines in its shadow, a highlight on its limb.
void planet(Hand& H, vec2 c, float r, vec2 sun, uint32_t col, const std::vector<Poly>& holes = {}, float ang = 0.6f) {
    const vec2 d = unit(sun - c);
    const Poly disc = circle(c, r);
    const Poly lit = circle(c + d * (0.5f * r), r);
    std::vector<Poly> h = holes;
    H.colour({disc, h, {lit}}, col, ang);
    h.push_back(lit);
    H.colour({disc, h, {}}, col, ang + 0.4f, 0.45f, 0.5f);
    const float a = std::atan2(d.y, d.x);
    H.line(arc(c, r * 0.78f, r * 0.78f, a - 0.55f, a + 0.55f, 8), 0.8f, C_WHITE, 3, 0.8f);
}

void chalk(Sheet& S, const Panels& P, float beat, int drop) {
    Hand H{S, 2, C_WHITE, T_CHALK, 0.085f};
    const float alive = std::max(0.f, beat - ALIVE);
    const int b = (int)std::floor(alive);
    const bool on = beat >= ALIVE;
    const float hw = P.hw[0], top = std::min(P.y1[0], 21.5f);
    const vec2 sun = V(-0.5f, std::min(top - 6.f, 15.f));
    const vec2 sunR = sun - P.offset; // the Sun from the right panel
    const float sr = 2.65f;
    const std::vector<Poly> sunDisc = {circle(sun, sr + 0.15f)};
    // the Sun, its rays shimmering on the beat
    S.begin(60, 0);
    {
        const Poly disc = circle(sun, sr, 60);
        H.shape(disc, 1.f, C_YELLOW);
        H.colour({disc, {}, {}}, C_YELLOW, deg(30.f), 1.1f);
        H.colour({disc, {}, {circle(sun + V(0.9f, -0.9f), sr)}}, C_ORANGE, deg(-35.f), 0.8f, 0.8f);
        S.segs(hatch({circle(sun, sr + 0.75f, 60), {disc}, {}}, deg(20.f), 0.24f, S.r, 0.1f), C_YELLOW, T_CHALK, 0.34f, 0.16f, 2);
        for (int i = 0; i < 4; i++) { // spots
            const vec2 q = sun + rot(V(sr * 0.55f, 0.f), deg(30.f + 85.f * i));
            H.line(circle(q, 0.18f + 0.06f * (i % 2), 10), 0.7f, C_ORANGE, 3, 0.8f);
        }
    }
    {
        const int pose = on ? b % 2 : 0;
        S.begin(61, 0, (uint32_t)pose);
        for (int i = 0; i < 14; i++) {
            const float a = (i + 0.5f * pose) * TAU / 14.f + 0.1f;
            const float r0 = sr + 0.35f, r1 = sr + ((i + pose) % 2 ? 0.95f : 1.4f);
            Poly ray;
            for (int k = 0; k <= 8; k++) {
                const float t = k / 8.f, rr = r0 + (r1 - r0) * t;
                ray.push_back(sun + rot(V(rr, 0.035f * std::sin(t * 6.f + i)), a));
            }
            if (ray.back().x < -hw + 0.1f || ray.back().y > top - 0.1f) continue;
            H.line(ray, 0.9f, i % 2 ? C_ORANGE : C_YELLOW, 1, 0.9f);
        }
    }
    // the orbits: dashed arcs round the Sun
    const float orb[5] = {4.0f, 5.65f, 7.7f, 10.0f, 12.3f};
    S.begin(62, 0);
    const Poly boxL = rect(-hw + 0.15f, 0.2f, hw - 0.15f, top - 0.15f);
    for (int i = 0; i < 4; i++) {
        for (const Poly& q : visible(arc(sun, orb[i], orb[i], 0.f, TAU, 120), {boxL}, true)) {
            S.line(q, C_WHITE, T_CHALK, 0.04f, 0.5f, 0, 0.32f);
        }
    }
    // the inner planets
    struct Pl { float orbit, ang, r; uint32_t col; const char* name; vec2 label; };
    const Pl inner[4] = {{orb[0], deg(-30.f), 0.32f, C_GREY, "Mercury", V(0.f, -0.9f)},
                         {orb[1], deg(-55.f), 0.55f, C_ORANGE, "Venus", V(0.f, -1.05f)},
                         {orb[2], deg(-79.f), 0.8f, C_BLUE, "Earth", V(-2.05f, 0.35f)},
                         {orb[3], deg(-101.f), 0.6f, C_PINK, "Mars", V(-0.4f, 1.15f)}};
    for (int i = 0; i < 4; i++) {
        const Pl& p = inner[i];
        const vec2 c = sun + V(p.orbit * std::cos(p.ang), p.orbit * std::sin(p.ang));
        S.begin(63 + i, 0);
        H.shape(circle(c, p.r), 0.9f);
        if (i == 2) { // the Earth: its seas, its lands, a white cap
            std::vector<Poly> land;
            const vec2 lc[2] = {V(-0.25f, 0.2f), V(0.3f, -0.3f)};
            for (int k = 0; k < 2; k++) {
                Poly blob;
                for (int j = 0; j <= 12; j++) {
                    const float a = j * TAU / 12.f, q = 0.3f * (1.f + 0.3f * std::sin(a * 3.f + k * 2.f));
                    blob.push_back(c + lc[k] + V(q * std::cos(a), q * std::sin(a)));
                }
                land.push_back(blob);
            }
            planet(H, c, p.r, sun, p.col, land);
            for (const Poly& l : land) H.colour({l, {}, {circle(c, p.r)}}, C_GREEN, deg(40.f), 1.2f);
        } else {
            planet(H, c, p.r, sun, p.col);
        }
        S.text(c + p.label, p.name, C_WHITE, 0.74f, 0.95f);
        if (i == 2) { // the Moon, going round the Earth
            const int pose = on ? b % 8 : 0;
            S.begin(67, 0, (uint32_t)pose);
            const float a = deg(-20.f) + pose * TAU / 8.f;
            const vec2 m = c + V(1.3f * std::cos(a), 1.3f * std::sin(a));
            H.shape(circle(m, 0.22f, 12), 0.7f);
            H.colour({circle(m, 0.22f, 12), {}, {}}, C_GREY, 0.3f, 1.2f);
        }
    }
    // the asteroid belt
    S.begin(68, 0);
    {
        Rng r(mixk(S.seed, 68));
        for (int i = 0; i < 46; i++) {
            const float a = deg(r.range(-140.f, -40.f)), rr = orb[4] + r.range(-0.45f, 0.45f);
            const vec2 q = sun + V(rr * std::cos(a), rr * std::sin(a));
            if (!inside(boxL, q) || q.y < 0.6f) continue;
            const float l = r.range(0.04f, 0.1f);
            S.line({q, q + V(l, r.range(-0.04f, 0.04f))}, i % 3 ? C_GREY : C_WHITE, T_CHALK, 0.075f, 0.85f, 3);
        }
    }
    S.text(V(1.9f, std::max(0.9f, sun.y - orb[4] - 1.f)), "asteroids", C_GREY, 0.62f, 0.9f);
    S.text(sun + V(2.1f, sr + 1.3f), "Sun", C_WHITE, 0.9f);

    // ---- the giants (right)
    const float topR = std::min(P.y1[1] - 0.4f, 23.f);
    const float hwR = P.hw[1];
    const Poly boxR = rect(-hwR + 0.15f, P.y0[1] + 0.2f, hwR - 0.15f, topR - 1.6f);
    S.begin(70, 1);
    S.text(V(0.f, topR - 0.55f), "The Solar System", C_WHITE, 1.0f);
    {
        Poly u;
        for (int i = 0; i <= 16; i++) { const float t = i / 16.f; u.push_back(V(-3.3f + 6.6f * t, topR - 1.25f + 0.06f * std::sin(t * 14.f))); }
        H.line(u, 0.8f, C_YELLOW, 3, 0.9f);
    }
    struct Gi { vec2 c; float r; };
    const Gi giants[4] = {{V(-0.55f, topR - 5.2f), 2.05f}, {V(0.7f, topR - 12.0f), 1.4f}, {V(-1.75f, topR - 17.1f), 0.9f},
                          {V(2.0f, topR - 20.6f), 0.85f}};
    std::vector<Poly> discs;
    for (const Gi& g : giants) discs.push_back(circle(g.c, g.r + 0.2f));
    const Poly satFront = ringBand(giants[1].c, 3.0f, 0.72f, 2.0f, 0.48f, PI, TAU, deg(-14.f));
    const Poly satBack = ringBand(giants[1].c, 3.0f, 0.72f, 2.0f, 0.48f, 0.f, PI, deg(-14.f));
    // their orbits: dashed arcs round the far Sun
    S.begin(71, 1);
    for (int i = 0; i < 4; i++) {
        const float rr = len(giants[i].c - sunR);
        const float a = std::atan2(giants[i].c.y - sunR.y, giants[i].c.x - sunR.x);
        std::vector<Poly> occ = discs;
        occ.push_back(satFront);
        occ.push_back(satBack);
        const float span = (3.4f + giants[i].r) / rr;
        for (const Poly& q : visible(arc(sunR, rr, rr, a - span, a + span, 40), {boxR}, true))
            for (const Poly& q2 : visible(q, occ)) S.line(q2, C_WHITE, T_CHALK, 0.04f, 0.45f, 0, 0.32f);
    }
    // Jupiter: its bands, its red spot, its moons
    S.begin(72, 1);
    {
        const Gi& g = giants[0];
        const float tl = deg(-6.f);
        const Poly disc = circle(g.c, g.r, 56);
        H.shape(disc, 1.f);
        const float e[7] = {-g.r - 0.1f, -1.25f, -0.62f, -0.12f, 0.45f, 1.1f, g.r + 0.1f};
        const uint32_t bc[6] = {C_ORANGE, C_WHITE, C_ORANGE, C_YELLOW, C_ORANGE, C_WHITE};
        const vec2 spotC = g.c + rot(V(0.75f, -0.9f), tl);
        const Poly spot = ellipse(spotC, 0.45f, 0.24f, tl, 20);
        for (int i = 0; i < 6; i++) {
            const Poly strip = turn(rect(g.c.x - 3.f, g.c.y + e[i], g.c.x + 3.f, g.c.y + e[i + 1]), g.c, tl);
            H.colour({disc, {spot}, {strip}}, bc[i], tl + deg(i % 2 ? 4.f : -4.f), bc[i] == C_WHITE ? 0.7f : 1.f,
                      bc[i] == C_WHITE ? 0.55f : 0.9f);
            if (i > 0) {
                Poly l;
                for (int k = 0; k <= 14; k++) {
                    const float t = k / 14.f;
                    l.push_back(g.c + rot(V(-g.r + 2.f * g.r * t, e[i] + 0.05f * std::sin(t * 12.f + i)), tl));
                }
                H.lineOut(l, {disc}, 0.5f, C_WHITE, 2, 0.6f, true);
            }
        }
        // (the band lines only inside the disc)
        H.shape(spot, 0.7f, C_PINK);
        H.colour({spot, {}, {}}, C_PINK, 0.2f, 1.3f);
        H.line(arc(g.c, g.r * 0.85f, g.r * 0.85f, PI - 0.6f, PI + 0.5f, 10), 0.8f, C_WHITE, 3, 0.8f);
        const float mx[4] = {-3.4f, -2.75f, 2.7f, 3.45f};
        for (int i = 0; i < 4; i++) {
            const vec2 m = g.c + rot(V(mx[i], 0.f), tl);
            if (std::fabs(m.x) > hwR - 0.3f) continue;
            H.shape(circle(m, 0.1f, 8), 0.8f, C_WHITE, 3, 0.9f);
        }
        S.text(g.c + V(0.f, -g.r - 0.75f), "Jupiter", C_WHITE, 0.78f, 0.95f);
    }
    // Saturn and its rings
    S.begin(73, 1);
    {
        const Gi& g = giants[1];
        const float tl = deg(-14.f);
        const Poly disc = circle(g.c, g.r, 48);
        const Poly outerE = ellipse(g.c, 3.0f, 0.72f, tl, 56), gapE = ellipse(g.c, 2.55f, 0.61f, tl, 50), innerE = ellipse(g.c, 2.0f, 0.48f, tl, 44);
        H.lineOut(disc, {satFront}, 1.f, C_YELLOW);
        for (const Poly* e : {&outerE, &innerE}) H.lineOut(*e, {disc, satFront}, 0.9f);
        H.line(arc(g.c, 3.0f, 0.72f, PI, TAU, 36, tl), 0.9f);
        H.line(arc(g.c, 2.0f, 0.48f, PI, TAU, 30, tl), 0.9f);
        H.lineOut(gapE, {disc, satFront}, 0.5f);
        H.line(arc(g.c, 2.55f, 0.61f, PI, TAU, 32, tl), 0.5f);
        for (int i = 0; i < 3; i++) {
            const float e0 = -g.r - 0.1f + i * (2.f * g.r + 0.2f) / 3.f, e1 = e0 + (2.f * g.r + 0.2f) / 3.f;
            const Poly strip = turn(rect(g.c.x - 2.f, g.c.y + e0, g.c.x + 2.f, g.c.y + e1), g.c, tl);
            H.colour({disc, {satFront}, {strip, circle(g.c + unit(sunR - g.c) * (0.5f * g.r), g.r)}}, i == 1 ? C_ORANGE : C_YELLOW, tl + 0.5f);
        }
        H.colour({disc, {satFront, circle(g.c + unit(sunR - g.c) * (0.5f * g.r), g.r)}, {}}, C_YELLOW, tl + 0.9f, 0.45f, 0.5f);
        const Poly bA = ringBand(g.c, 3.0f, 0.72f, 2.55f, 0.61f, 0.f, PI, tl), bB = ringBand(g.c, 2.55f, 0.61f, 2.0f, 0.48f, 0.f, PI, tl);
        const Poly fA = ringBand(g.c, 3.0f, 0.72f, 2.55f, 0.61f, PI, TAU, tl), fB = ringBand(g.c, 2.55f, 0.61f, 2.0f, 0.48f, PI, TAU, tl);
        H.colour({bA, {disc}, {}}, C_ORANGE, tl + 1.1f, 1.1f, 0.85f);
        H.colour({bB, {disc}, {}}, C_YELLOW, tl - 1.1f, 1.1f, 0.85f);
        H.colour({fA, {}, {}}, C_ORANGE, tl + 1.1f, 1.1f, 0.9f);
        H.colour({fB, {}, {}}, C_YELLOW, tl - 1.1f, 1.1f, 0.9f);
        S.text(g.c + V(0.f, -g.r - 0.95f), "Saturn", C_WHITE, 0.78f, 0.95f);
    }
    // Uranus (its ring upright) and Neptune
    S.begin(74, 1);
    {
        const Gi& g = giants[2];
        H.shape(circle(g.c, g.r), 0.9f);
        planet(H, g.c, g.r, sunR, C_BLUE);
        H.lineOut(ellipse(g.c, 0.32f, g.r + 0.55f, deg(8.f), 30), {}, 0.6f, C_WHITE, 1, 0.8f);
        S.text(g.c + V(1.95f, 0.1f), "Uranus", C_WHITE, 0.74f, 0.95f);
    }
    S.begin(75, 1);
    {
        const Gi& g = giants[3];
        H.shape(circle(g.c, g.r), 0.9f);
        planet(H, g.c, g.r, sunR, C_PERI);
        H.line(ellipse(g.c + V(-0.15f, -0.25f), 0.22f, 0.12f, 0.f, 12), 0.6f, C_PURPLE, 3, 0.9f);
        S.text(g.c + V(-2.05f, 0.1f), "Neptune", C_WHITE, 0.74f, 0.95f);
    }
    // stars, both sides
    S.begin(76, 0);
    {
        Rng r(mixk(S.seed, 76 + (uint64_t)drop));
        std::vector<Poly> occL = sunDisc;
        for (int side = 0; side < 2; side++) {
            const float h = side ? hwR : hw, y0 = side ? P.y0[1] + 0.5f : 0.5f, y1 = side ? topR - 1.8f : top - 0.4f;
            int n = 0;
            for (int t = 0; t < 120 && n < 16; t++) {
                const vec2 q = V(r.range(-h + 0.3f, h - 0.3f), r.range(y0, y1));
                bool ok = true;
                if (side == 0) {
                    if (len(q - sun) < sr + 1.7f) ok = false;
                    for (int i = 0; i < 4 && ok; i++) {
                        const vec2 c = sun + V(inner[i].orbit * std::cos(inner[i].ang), inner[i].orbit * std::sin(inner[i].ang));
                        if (len(q - c) < inner[i].r + 1.3f) ok = false;
                    }
                } else {
                    for (const Gi& g : giants) if (len(q - g.c) < g.r + 1.4f) ok = false;
                    if (std::fabs(q.y - giants[1].c.y) < 1.3f && std::fabs(q.x - giants[1].c.x) < 3.4f) ok = false;
                }
                if (!ok) continue;
                S.side = side;
                if (n % 4 == 0) {
                    H.line({q + V(-0.14f, 0.f), q + V(0.14f, 0.f)}, 0.6f, C_WHITE, 3, 0.8f);
                    H.line({q + V(0.f, -0.14f), q + V(0.f, 0.14f)}, 0.6f, C_WHITE, 3, 0.8f);
                } else {
                    S.line({q, q + V(0.03f, 0.02f)}, C_WHITE, T_CHALK, 0.07f, 0.7f, 3);
                }
                n++;
            }
        }
        S.side = 0;
    }
    // a shooting star now and then (every other bar, for a beat)
    if (on && (b % 8) == 5) {
        S.begin(77, 1, (uint32_t)(b / 8));
        const vec2 a = V(S.r.range(-hwR + 0.6f, -1.8f), S.r.range(topR - 10.4f, topR - 9.4f));
        const vec2 e = a + V(2.2f, -0.9f);
        H.line({a, e}, 0.6f, C_WHITE, 1, 0.7f);
        H.line({a + V(0.f, 0.12f), e - V(0.8f, -0.25f)}, 0.4f, C_YELLOW, 1, 0.5f);
        H.line({e + V(-0.12f, 0.f), e + V(0.12f, 0.f)}, 0.7f, C_WHITE, 1, 1.f);
        H.line({e + V(0.f, -0.12f), e + V(0.f, 0.12f)}, 0.7f, C_WHITE, 1, 1.f);
    }
}

} // namespace

void build(int variant, int dropNo, uint64_t seed, float beat, const Panels& P, std::vector<Elem>& out) {
    out.clear();
    Sheet S(out, mixk(seed, (uint64_t)dropNo * 7919u + 13u));
    if (variant == 0) feltPens(S, P, beat, dropNo);
    else if (variant == 1) pencil(S, P, beat, dropNo);
    else chalk(S, P, beat, dropNo);
}

void schedule(std::vector<Elem>& e) {
    // the hand's pace: a line at 9 units a beat, the colouring at 55 units of strokes a beat, a word in a quarter beat
    std::vector<float> d(e.size());
    float total = 0.f;
    for (size_t i = 0; i < e.size(); i++) {
        const Elem& x = e[i];
        if (x.kind == E_LINE) d[i] = std::max(0.08f, length(x.pts) / 9.f);
        else if (x.kind == E_SEGS) {
            float l = 0.f;
            for (size_t k = 0; k + 1 < x.pts.size(); k += 2) l += len(x.pts[k + 1] - x.pts[k]);
            d[i] = std::clamp(l / 55.f, 0.2f, 4.f);
        } else d[i] = 0.25f;
        if (!std::isfinite(d[i])) d[i] = 0.2f;
        total += d[i];
    }
    const float k = total > ALIVE - 2.f ? (ALIVE - 2.f) / total : 1.f;
    std::vector<size_t> order(e.size());
    for (size_t i = 0; i < e.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return e[a].phase < e[b].phase; });
    float t = 0.f;
    for (size_t i : order) {
        e[i].t0 = t;
        e[i].dur = d[i] * k;
        t += d[i] * k;
    }
}

} // namespace notepad_art

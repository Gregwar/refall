// NOTEPAD's storyboard: a little stick figure lives in the margins of the page all song long, in the notebook's
// tools, like a flip-book drawn in the corner of an exercise book. His adventures follow one another, each from
// where the last one left him:
//
//  - he walks (in a break) or runs (from the verse on) along the ground, a step on each beat, and turns back;
//  - he hops from platform to platform up into the margin (each drawn in just before he needs it) and back down;
//  - a ladder is drawn, he climbs it rung by rung to a ledge;
//  - from a ledge he jumps off and his parachute opens, he sways down and lands; or he dives off with a somersault;
//  - something falls on him from the top of the page (an anvil, a piano, a flower pot, a safe): a "!", its shadow
//    growing, he leaps back just in time, it crashes where he stood;
//  - acrobatics on the bars: cartwheels, a backflip, a handstand and a walkover, a star jump;
//  - he crosses to the other margin through a hole drawn on the ground (out of another one over there);
//  - in a break he sits down and taps his foot, or lies down and sleeps (Zzz); a wave now and then.
//
// What he needs is drawn in before (a line growing along its length, the colouring stroke after stroke) and rubbed
// out once he is done with it, so the margins never fill up. The guy himself is drawn anew on every drawing, each
// pose rubbed out for the next one (an articulated skeleton: head, torso, two-segment arms and legs, posed from
// keyframes or from his motion, with a scarf trailing behind him).
//
// A drop stops the adventures: the margin he is in turns into a dance floor (a cardboard mat under a spotlight),
// the other margin gets the ambiance (a boombox throwing out notes and sound waves, a disco ball and its rays, a
// little crowd cheering on the beats, a shout now and then) and he dances big, breaking on the beats: toprock,
// footwork, windmills, headspins, a freeze to end each run. When the drop ends the ambiance is rubbed out, he
// bows, and the adventures go on from the dance floor.
//
// Everything here is plain geometry in board units; the hand's imprecision is in it (lines a little off, closed
// shapes overshooting where they close, colouring never quite inside the lines), seeded by the thing, so what
// doesn't move comes out the same on every drawing.
#include "universes/notepad/story.hpp"

#include <algorithm>
#include <cmath>

namespace notepad_story {
namespace {

using Poly = std::vector<vec2>;

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 unit(vec2 a) { const float l = len(a); return l > 1e-6f ? a * (1.f / l) : V(0, 0); }
vec2 rot(vec2 p, float a) { const float c = std::cos(a), s = std::sin(a); return {c * p.x - s * p.y, s * p.x + c * p.y}; }
vec2 mixv(vec2 a, vec2 b, float t) { return a + (b - a) * t; }
float ease(float t) { t = saturate(t); return t * t * (3.f - 2.f * t); }
float frac(double x) { return (float)(x - std::floor(x)); }
uint64_t mixk(uint64_t a, uint64_t b) { return splitmix64(a ^ (b * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull)); }

// ---------------------------------------------------------------- shapes
Poly arc(vec2 c, float rx, float ry, float a0, float a1, int n) {
    Poly p;
    for (int i = 0; i <= n; i++) {
        const float a = a0 + (a1 - a0) * i / n;
        p.push_back(c + V(rx * std::cos(a), ry * std::sin(a)));
    }
    return p;
}
Poly ellipse(vec2 c, float rx, float ry) { return arc(c, rx, ry, 0.f, TAU, std::clamp((int)(std::max(rx, ry) * 24.f), 12, 40)); }
Poly rect(float x0, float y0, float x1, float y1) { return {V(x0, y0), V(x1, y0), V(x1, y1), V(x0, y1), V(x0, y0)}; }
Poly xform(Poly p, vec2 at, float s, int dir = 1) {
    for (vec2& q : p) q = at + V(q.x * s * (float)dir, q.y * s);
    return p;
}
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
// The runs of a line above the height `y` (what of the guy is out of a hole).
std::vector<Poly> above(const Poly& p, float y) {
    std::vector<Poly> runs;
    Poly cur;
    for (size_t i = 0; i < p.size(); i++) {
        const bool in = p[i].y >= y;
        if (i > 0 && in != (p[i - 1].y >= y)) {
            const vec2 a = p[i - 1], b = p[i];
            const vec2 x = mixv(a, b, (y - a.y) / (b.y - a.y));
            cur.push_back(x);
            if (!in) { if (cur.size() >= 2) runs.push_back(cur); cur.clear(); }
        }
        if (in) cur.push_back(p[i]);
    }
    if (cur.size() >= 2) runs.push_back(cur);
    return runs;
}

// ---------------------------------------------------------------- the hand
// The line a hand draws: a slow drift off the true line along its length.
Poly hand(Poly p, Rng& r, float amp) {
    if (p.size() < 2 || amp <= 0.f) return p;
    const float f1 = r.range(0.4f, 0.8f), f2 = r.range(1.2f, 2.2f);
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
Poly loopOf(const Poly& c, Rng& r) {
    Poly P = c;
    if (P.size() > 2 && len(P.front() - P.back()) < 1e-4f) P.pop_back();
    const int n = (int)P.size();
    if (n < 3) return c;
    const int s = r.irange(0, n - 1);
    const vec2 ce = centroid(P);
    Poly o;
    for (int i = 0; i <= n; i++) o.push_back(P[(s + i) % n]);
    const int m = std::max(1, (int)std::lround(n * 0.1f));
    const float size = len(P[0] - ce);
    const float drift = r.range(0.04f, 0.1f) * size * (r.chance(0.5f) ? 1.f : -1.f);
    for (int j = 1; j <= m; j++) {
        const vec2 q = P[(s + j) % n];
        o.push_back(q + unit(q - ce) * (drift * j / m));
    }
    return o;
}
// Strokes across a closed polygon, `sp` apart at the angle `ang`, back and forth, each overshooting or stopping
// short of the edge by up to `over`, a little off parallel.
std::vector<vec2> hatch(const Poly& poly, float ang, float sp, Rng& r, float over) {
    std::vector<vec2> out;
    if (poly.size() < 3 || sp <= 0.005f) return out;
    Poly a;
    for (const vec2& v : poly) a.push_back(rot(v, -ang));
    float y0 = 1e9f, y1 = -1e9f;
    for (const vec2& v : a) y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
    int line = 0;
    std::vector<float> xs;
    for (float y = y0 + sp * r.range(0.3f, 0.7f); y < y1; y += sp) {
        xs.clear();
        const size_t n = a.size();
        for (size_t i = 0; i < n; i++) {
            const vec2 p = a[i], q = a[(i + 1) % n];
            if ((p.y <= y) == (q.y <= y)) continue;
            xs.push_back(p.x + (y - p.y) / (q.y - p.y) * (q.x - p.x));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            if (xs[i + 1] - xs[i] < sp * 0.3f) continue;
            const float x0 = xs[i] - r.range(-0.35f, 1.f) * over, x1 = xs[i + 1] + r.range(-0.35f, 1.f) * over;
            const float yy = y + r.range(-0.1f, 0.1f) * sp, dy = r.range(-0.1f, 0.1f) * sp;
            vec2 p0 = V(x0, yy - dy), p1 = V(x1, yy + dy);
            if (line & 1) std::swap(p0, p1);
            out.push_back(rot(p0, ang));
            out.push_back(rot(p1, ang));
            line++;
        }
    }
    return out;
}

// ---------------------------------------------------------------- the notebooks' pens
struct Pal {
    uint32_t ink;
    int tool;
    float w;
    uint32_t red, blue, yellow, green, orange, purple, grey, card, light;
};
const Pal PALS[3] = {
    // felt pens: a black fineliner, felt pens, a yellow highlighter
    {0x1b2030, T_PEN, 0.06f, 0xec3a3a, 0x14a4dc, 0xffd01c, 0x38b648, 0xff891c, 0x9a4cc9, 0x9aa0ad, 0xe8a24a, 0xfff07a},
    // graphite and coloured pencils
    {0x3a3b41, T_PENCIL, 0.058f, 0xd63c30, 0x2a82cc, 0xd99c18, 0x3f9944, 0xe25e12, 0x7a3cae, 0x6c6d73, 0xc79a52, 0xd9b030},
    // chalk on the blackboard
    {0xeeefe8, T_CHALK, 0.07f, 0xf79494, 0x96d1f2, 0xf6e27a, 0xa2e39a, 0xf8b673, 0xd2a9f2, 0xa4aba5, 0xe6c08e, 0xf6e27a},
};

// The hand drawing things: each thing (an id) drawn from a beat at a speed (0: all at once), one line after the
// other, in the notebook's pens.
struct Pen {
    std::vector<Elem>& out;
    int v;
    const Pal& P;
    uint32_t id = 0;
    int k = 0;
    Rng r{1};
    float at = -1e6f, speed = 0.f, ghost = 0.3f;
    Pen(std::vector<Elem>& o, int variant) : out(o), v(variant), P(PALS[variant]) {}
    void begin(uint32_t thing, float t0 = -1e6f, float sp = 0.f, float gh = 0.3f) {
        id = thing;
        k = 0;
        r = Rng(mixk(thing, 0x57A9));
        at = t0;
        speed = sp;
        ghost = gh;
    }
    Elem& add(int kind, float dur) {
        out.emplace_back();
        Elem& e = out.back();
        e.id = id * 128u + (uint32_t)std::min(k++, 127);
        e.kind = kind;
        e.t0 = at;
        e.dur = dur;
        e.residue = ghost;
        at += dur;
        return e;
    }
    void raw(const Poly& p, uint32_t col, int tool, float w, float a = 1.f) {
        if (p.size() < 2) return;
        Elem& e = add(E_LINE, speed > 0.f ? std::max(0.04f, length(p) / speed) : 0.f);
        e.pts = p;
        e.col = col;
        e.tool = tool;
        e.width = w;
        e.alpha = a;
    }
    // an open line, by hand
    void line(const Poly& p, float wk = 1.f, uint32_t col = 0, float a = 1.f, float drift = 1.f) {
        raw(hand(p, r, 0.018f * drift), col ? col : P.ink, P.tool, P.w * wk, a);
    }
    // a closed shape, drawn in one go
    void shape(const Poly& p, float wk = 1.f, uint32_t col = 0, float a = 1.f) {
        raw(hand(loopOf(p, r), r, 0.012f), col ? col : P.ink, P.tool, P.w * wk, a);
    }
    void segs(std::vector<vec2> s, uint32_t col, int tool, float w, float a) {
        if (s.size() < 2) return;
        float l = 0.f;
        for (size_t i = 0; i + 1 < s.size(); i += 2) l += len(s[i + 1] - s[i]);
        Elem& e = add(E_SEGS, speed > 0.f ? std::clamp(l / (speed * 4.f), 0.1f, 2.f) : 0.f);
        e.pts = std::move(s);
        e.col = col;
        e.tool = tool;
        e.width = w;
        e.alpha = a;
    }
    // coloured in, the notebook's way (`k`: the size of the strokes, for small things)
    void colour(const Poly& A, uint32_t col, float ang, float k = 1.f, float a = 1.f) {
        if (v == 0) { // felt pens: bands back and forth
            segs(hatch(A, ang, 0.12f * k, r, 0.05f * k), col, T_MARKER, 0.17f * k, 0.85f * a);
        } else if (v == 1) { // coloured pencil: a soft layer, then hatching
            segs(hatch(A, ang + 0.6f, 0.15f * k, r, 0.02f), col, T_CPENCIL, 0.15f * k, 0.3f * a);
            segs(hatch(A, ang, 0.065f * k, r, 0.04f * k), col, T_CPENCIL, 0.04f * k, 0.9f * a);
        } else { // chalk: rubbed in with its side, then lines
            segs(hatch(A, ang + 0.45f, 0.2f * k, r, 0.04f), col, T_CHALK, 0.26f * k, 0.3f * a);
            segs(hatch(A, ang, 0.1f * k, r, 0.05f * k), col, T_CHALK, 0.055f * k, 0.75f * a);
        }
    }
    // a dark area (a hole, an anvil's iron): black felt, graphite cross-hatching, grey chalk
    void dark(const Poly& A, float ang, float k = 1.f) {
        if (v == 0) segs(hatch(A, ang, 0.1f * k, r, 0.03f * k), P.ink, T_MARKER, 0.14f * k, 0.8f);
        else if (v == 1) {
            segs(hatch(A, ang, 0.06f * k, r, 0.03f * k), P.ink, T_PENCIL, 0.035f, 0.85f);
            segs(hatch(A, ang + 1.2f, 0.07f * k, r, 0.03f * k), P.ink, T_PENCIL, 0.035f, 0.7f);
        } else segs(hatch(A, ang, 0.12f * k, r, 0.03f * k), P.grey, T_CHALK, 0.05f, 0.6f);
    }
    void text(vec2 c, const std::string& t, uint32_t col, float size, float a = 1.f) {
        Elem& e = add(E_TEXT, 0.f);
        e.pts = {c};
        e.text = t;
        e.col = col;
        e.width = size;
        e.alpha = a;
    }
};

// ---------------------------------------------------------------- the guy
// His skeleton in his own units (about 1.9 tall), x forward (his facing), y up, his feet's ground at 0.
enum { HIP, NECK, HEAD, EL0, HA0, EL1, HA1, KN0, FT0, KN1, FT1, NJ };
struct Skel { vec2 j[NJ]; };
const float THIGH = 0.43f, SHIN = 0.43f, TORSO = 0.58f, NECKL = 0.25f, HEADR = 0.2f, UARM = 0.31f, FARM = 0.29f;

// A pose as angles: the spine's from upright (+ forward), each limb's segments from straight down (+ forward).
struct Ang {
    vec2 hip = V(0.f, 0.86f);
    float spine = 0.f, head = 0.f;
    float ua[2] = {0.12f, -0.12f}, fa[2] = {0.25f, -0.02f};
    float th[2] = {0.05f, -0.05f}, sh[2] = {0.f, -0.07f};
};
vec2 dn(float a) { return V(std::sin(a), -std::cos(a)); }
Skel fk(const Ang& A) {
    Skel S;
    const vec2 up = V(std::sin(A.spine), std::cos(A.spine));
    S.j[HIP] = A.hip;
    S.j[NECK] = A.hip + up * TORSO;
    S.j[HEAD] = S.j[NECK] + V(std::sin(A.spine + A.head), std::cos(A.spine + A.head)) * NECKL;
    for (int k = 0; k < 2; k++) {
        S.j[EL0 + 2 * k] = S.j[NECK] + dn(A.ua[k]) * UARM;
        S.j[HA0 + 2 * k] = S.j[EL0 + 2 * k] + dn(A.fa[k]) * FARM;
        S.j[KN0 + 2 * k] = A.hip + dn(A.th[k]) * THIGH;
        S.j[FT0 + 2 * k] = S.j[KN0 + 2 * k] + dn(A.sh[k]) * SHIN;
    }
    return S;
}
Ang mixA(const Ang& a, const Ang& b, float t) {
    Ang o;
    o.hip = mixv(a.hip, b.hip, t);
    o.spine = lerpf(a.spine, b.spine, t);
    o.head = lerpf(a.head, b.head, t);
    for (int k = 0; k < 2; k++) {
        o.ua[k] = lerpf(a.ua[k], b.ua[k], t);
        o.fa[k] = lerpf(a.fa[k], b.fa[k], t);
        o.th[k] = lerpf(a.th[k], b.th[k], t);
        o.sh[k] = lerpf(a.sh[k], b.sh[k], t);
    }
    return o;
}
Skel mixS(const Skel& a, const Skel& b, float t) {
    Skel o;
    for (int i = 0; i < NJ; i++) o.j[i] = mixv(a.j[i], b.j[i], t);
    return o;
}
void shiftS(Skel& S, vec2 d) { for (vec2& p : S.j) p = p + d; }
void turnS(Skel& S, vec2 c, float a) { for (vec2& p : S.j) p = c + rot(p - c, a); }
// on the ground: on his feet, or on whatever is lowest (hands, head, back)
void onFeet(Skel& S) { shiftS(S, V(0.f, -std::min(S.j[FT0].y, S.j[FT1].y))); }
void onGround(Skel& S) {
    float lo = S.j[HEAD].y - HEADR;
    for (int i = 0; i < NJ; i++) if (i != HEAD) lo = std::min(lo, S.j[i].y);
    shiftS(S, V(0.f, -lo));
}

// ---- poses
Ang crouch() {
    Ang a;
    a.hip = V(-0.05f, 0.55f);
    a.spine = 0.4f;
    a.th[0] = 1.3f; a.th[1] = 1.1f;
    a.sh[0] = -0.4f; a.sh[1] = -0.55f;
    a.ua[0] = -0.7f; a.ua[1] = -0.95f;
    a.fa[0] = -0.3f; a.fa[1] = -0.55f;
    return a;
}
Ang leap() { // in the air, knees up, arms up
    Ang a;
    a.spine = 0.15f;
    a.th[0] = 1.5f; a.th[1] = 0.6f;
    a.sh[0] = 0.1f; a.sh[1] = -0.5f;
    a.ua[0] = 2.4f; a.ua[1] = 1.9f;
    a.fa[0] = 2.7f; a.fa[1] = 2.3f;
    return a;
}
Ang tuck() {
    Ang a;
    a.spine = 0.3f;
    a.th[0] = 2.1f; a.th[1] = 1.9f;
    a.sh[0] = 0.15f; a.sh[1] = -0.05f;
    a.ua[0] = 1.1f; a.ua[1] = 0.9f;
    a.fa[0] = 1.9f; a.fa[1] = 1.7f;
    return a;
}
Ang tada() {
    Ang a;
    a.head = -0.15f;
    a.ua[0] = 2.55f; a.ua[1] = -2.55f;
    a.fa[0] = 2.8f; a.fa[1] = -2.8f;
    a.th[0] = 0.18f; a.th[1] = -0.18f;
    a.sh[0] = 0.18f; a.sh[1] = -0.18f;
    return a;
}
Ang star() {
    Ang a;
    a.ua[0] = 2.25f; a.ua[1] = -2.25f;
    a.fa[0] = 2.3f; a.fa[1] = -2.3f;
    a.th[0] = 0.5f; a.th[1] = -0.5f;
    a.sh[0] = 0.5f; a.sh[1] = -0.5f;
    return a;
}
Ang armsUp() { // arms straight up (a handstand's start)
    Ang a;
    a.ua[0] = 2.75f; a.ua[1] = -2.75f;
    a.fa[0] = 2.9f; a.fa[1] = -2.9f;
    return a;
}
// walking or running: a step each π of `ph`, `amp` 0.5 a walk, 1 a run
Ang gait(float ph, float amp) {
    Ang a;
    a.spine = 0.06f + 0.16f * amp;
    for (int k = 0; k < 2; k++) {
        const float p = ph + k * PI, s = std::sin(p);
        a.th[k] = amp * 0.75f * s;
        a.sh[k] = a.th[k] - 0.15f * amp - 1.15f * amp * std::max(0.f, std::cos(p - 0.5f));
        a.ua[k] = -amp * 0.85f * s;
        a.fa[k] = a.ua[k] + 0.3f + 1.0f * amp;
    }
    a.head = -0.05f * amp;
    return a;
}
// climbing a ladder, seen from behind: arms reaching up in turn, knees out
Ang climb(float ph) {
    Ang a;
    const float s = std::sin(ph);
    a.ua[0] = 2.55f + 0.32f * s; a.fa[0] = a.ua[0] + 0.3f;
    a.ua[1] = -(2.55f - 0.32f * s); a.fa[1] = a.ua[1] - 0.3f;
    a.th[0] = 0.3f + 0.6f * std::max(0.f, s); a.sh[0] = a.th[0] - 0.15f - 0.8f * std::max(0.f, s);
    a.th[1] = -(0.3f + 0.6f * std::max(0.f, -s)); a.sh[1] = a.th[1] + 0.15f + 0.8f * std::max(0.f, -s);
    return a;
}
// hanging from the parachute's risers, legs dangling
Ang hang(float t) {
    Ang a;
    const float w = std::sin(t * PI * 0.5f);
    a.ua[0] = 2.75f; a.fa[0] = 2.95f;
    a.ua[1] = -2.75f; a.fa[1] = -2.95f;
    a.th[0] = 0.12f + 0.12f * w; a.sh[0] = a.th[0] - 0.12f;
    a.th[1] = -0.1f + 0.12f * w; a.sh[1] = a.th[1] - 0.15f;
    return a;
}
Ang wave(float t) {
    Ang a;
    a.ua[0] = 2.5f;
    a.fa[0] = 2.95f + 0.45f * std::sin(t * TAU);
    a.head = -0.1f;
    return a;
}
Ang sit(float t) {
    Ang a;
    a.hip = V(0.f, 0.f);
    a.spine = -0.3f;
    a.head = 0.25f + 0.12f * std::fabs(std::sin(t * PI)); // nodding on the beat
    a.th[0] = 2.2f; a.th[1] = 2.0f;
    a.sh[0] = 0.45f; a.sh[1] = 0.3f + 0.35f * std::max(0.f, std::sin(t * TAU)); // the foot tapping
    a.ua[0] = -0.55f; a.ua[1] = -0.7f;
    a.fa[0] = -0.45f; a.fa[1] = -0.6f;
    return a;
}
Skel sleeping(float t) {
    Ang a;
    a.spine = 0.f;
    a.head = 0.15f;
    a.ua[0] = 2.7f; a.fa[0] = -2.2f; // a hand behind his head
    a.ua[1] = 0.25f; a.fa[1] = 0.6f;
    a.th[0] = 0.05f; a.sh[0] = 0.f;
    a.th[1] = 0.75f + 0.04f * std::sin(t * PI * 0.5f); a.sh[1] = -0.35f; // a knee up
    Skel S = fk(a);
    turnS(S, a.hip, PI * 0.5f); // lying on his back, his head behind him
    onGround(S);
    return S;
}
Ang relief(float t) { // wiping his brow
    Ang a;
    a.head = 0.15f;
    a.ua[0] = 2.2f + 0.15f * std::sin(t * TAU * 2.f);
    a.fa[0] = -1.6f;
    a.ua[1] = -0.3f; a.fa[1] = -0.1f;
    return a;
}
Ang flail(float t) { // jumping back, arms going
    Ang a = leap();
    a.spine = -0.3f;
    a.ua[0] = 2.5f + 0.5f * std::sin(t * 18.f); a.fa[0] = a.ua[0] + 0.4f;
    a.ua[1] = -2.3f + 0.5f * std::sin(t * 18.f + 2.f); a.fa[1] = a.ua[1] - 0.4f;
    return a;
}
Ang lookUp() {
    Ang a;
    a.head = -0.55f;
    a.spine = -0.12f;
    a.ua[0] = 0.5f; a.fa[0] = 1.0f;
    a.ua[1] = -0.5f; a.fa[1] = -0.2f;
    return a;
}
Skel standing() { Skel S = fk(Ang{}); onFeet(S); return S; }

// ---------------------------------------------------------------- the episodes
enum { K_WAVE, K_RUN, K_HOP, K_LADDER, K_CHUTE, K_DIVE, K_DODGE, K_ACRO, K_CROSS, K_REST, K_COUNT };
enum { TR_CARTWHEEL, TR_BACKFLIP, TR_HANDSTAND, TR_STARJUMP, TR_COUNT };
const float TRICK_MOVE[TR_COUNT] = {3.0f, -0.6f, 0.f, 0.f}; // how far a trick takes him (his units, forward)
enum { OB_ANVIL, OB_PIANO, OB_POT, OB_SAFE, OB_COUNT };
enum { PR_GROUND, PR_LEDGE, PR_LADDER, PR_HOLE };
const float RUNG = 0.42f; // the ladder's rungs (his units)

// Where the guy is and how he stands, in his margin's coordinates (board units from its bottom middle).
struct Fig {
    Skel S;          // his units
    vec2 at;         // his feet
    int dir = 1;
    int side = 0;
    float clip = -1e9f; // what is under this height is down a hole
    bool shown = true;
    float turn = 0.f;   // swaying (a parachute): about `pivot`
    vec2 pivot;
};
vec2 jointAt(const Fig& f, int j, float s) {
    const vec2 p = f.at + V(f.S.j[j].x * s * (float)f.dir, f.S.j[j].y * s);
    return f.turn != 0.f ? f.pivot + rot(p - f.pivot, f.turn) : p;
}

// The falling things (about 1.3 wide, on the ground at 0).
void fallingThing(Pen& pen, int kind, vec2 at, float s) {
    const Pal& P = pen.P;
    auto X = [&](Poly p) { return xform(std::move(p), at, s); };
    switch (kind) {
    case OB_ANVIL: {
        const Poly body = X({V(-0.42f, 0.f), V(0.42f, 0.f), V(0.42f, 0.08f), V(0.17f, 0.2f), V(0.17f, 0.52f), V(0.58f, 0.58f),
                             V(0.58f, 0.82f), V(-0.45f, 0.82f), V(-0.9f, 0.77f), V(-0.45f, 0.63f), V(-0.17f, 0.52f), V(-0.17f, 0.2f),
                             V(-0.42f, 0.08f), V(-0.42f, 0.f)});
        pen.shape(body, 0.9f);
        pen.dark(body, 0.5f, 0.8f * s);
        pen.line(X({V(-0.4f, 0.74f), V(0.5f, 0.74f)}), 0.5f, P.light, 0.8f);
        break;
    }
    case OB_PIANO: {
        const Poly body = X(rect(-0.62f, 0.12f, 0.62f, 1.1f));
        pen.shape(body);
        pen.shape(X(rect(-0.68f, 1.1f, 0.68f, 1.22f)), 0.9f);
        pen.colour(body, P.red, 0.4f, 0.8f * s);
        const Poly keys = X(rect(-0.62f, 0.5f, 0.62f, 0.66f));
        pen.shape(keys, 0.7f);
        for (int i = 0; i < 9; i++) {
            const float x = -0.5f + i * 0.125f;
            pen.line(X({V(x, 0.58f), V(x, 0.66f)}), 0.6f);
        }
        pen.line(X({V(-0.5f, 0.12f), V(-0.5f, 0.f)}), 0.9f);
        pen.line(X({V(0.5f, 0.12f), V(0.5f, 0.f)}), 0.9f);
        break;
    }
    case OB_POT: {
        const Poly pot = X({V(-0.32f, 0.f), V(0.32f, 0.f), V(0.42f, 0.52f), V(-0.42f, 0.52f), V(-0.32f, 0.f)});
        const Poly rim = X(rect(-0.5f, 0.52f, 0.5f, 0.66f));
        pen.shape(pot);
        pen.shape(rim, 0.9f);
        pen.colour(pot, P.orange, -0.5f, 0.8f * s);
        pen.colour(rim, P.orange, 0.3f, 0.6f * s, 0.9f);
        pen.line(X({V(0.f, 0.66f), V(0.03f, 0.95f), V(-0.02f, 1.25f)}), 0.8f, P.green);
        const Poly leaf1 = X({V(0.f, 0.85f), V(0.25f, 0.98f), V(0.4f, 0.95f), V(0.22f, 0.83f), V(0.f, 0.85f)});
        const Poly leaf2 = X({V(0.01f, 1.0f), V(-0.22f, 1.1f), V(-0.36f, 1.06f), V(-0.2f, 0.96f), V(0.01f, 1.0f)});
        pen.shape(leaf1, 0.7f, P.green);
        pen.shape(leaf2, 0.7f, P.green);
        const vec2 fc = at + V(-0.02f * s, 1.33f * s);
        for (int i = 0; i < 5; i++) {
            const float a = i * TAU / 5.f + 0.3f;
            const vec2 c = fc + V(std::cos(a), std::sin(a)) * (0.13f * s);
            pen.shape(ellipse(c, 0.09f * s, 0.09f * s), 0.6f, P.red);
        }
        pen.colour(ellipse(fc, 0.07f * s, 0.07f * s), P.yellow, 0.f, 0.4f * s);
        break;
    }
    default: { // a safe
        const Poly box = X(rect(-0.52f, 0.f, 0.52f, 1.f));
        pen.shape(box);
        pen.colour(box, P.grey, 0.7f, 0.8f * s);
        pen.shape(X(rect(-0.4f, 0.1f, 0.4f, 0.9f)), 0.6f);
        pen.shape(ellipse(at + V(0.f, 0.55f * s), 0.17f * s, 0.17f * s), 0.7f);
        pen.line(X({V(0.f, 0.55f), V(0.1f, 0.65f)}), 0.6f);
        pen.line(X({V(0.24f, 0.3f), V(0.24f, 0.42f)}), 0.8f);
        break;
    }
    }
}

// The parachute over the guy: a dome of gores (every other one coloured in), its rim, its risers to his hands.
void canopy(Pen& pen, vec2 c, float rx, float ry, vec2 hands, uint32_t ca, uint32_t cb) {
    auto meridian = [&](float a) {
        Poly m;
        for (int i = 0; i <= 8; i++) {
            const float ps = i / 8.f * PI * 0.5f;
            m.push_back(c + V(rx * std::cos(a) * std::sin(ps), ry * std::cos(ps)));
        }
        return m;
    };
    Poly dome = arc(c, rx, ry, 0.f, PI, 20);
    // the rim: scallops between the risers
    for (int k = 4; k > 0; k--) {
        const float x0 = rx * std::cos(PI * k / 4.f), x1 = rx * std::cos(PI * (k - 1) / 4.f);
        for (int i = 1; i <= 4; i++) {
            const float u = i / 4.f;
            dome.push_back(c + V(x0 + (x1 - x0) * u, 0.12f * ry * std::sin(u * PI)));
        }
    }
    pen.shape(dome, 0.9f);
    for (int k = 1; k < 4; k++) pen.line(meridian(PI * k / 4.f), 0.6f);
    for (int k = 0; k < 4; k++) {
        Poly g = meridian(PI * k / 4.f);
        Poly b = meridian(PI * (k + 1) / 4.f);
        std::reverse(b.begin(), b.end());
        g.insert(g.end(), b.begin(), b.end());
        pen.colour(g, (k % 2) ? cb : ca, 0.9f + 0.4f * k, 0.8f);
    }
    for (int k = 0; k <= 4; k++) pen.line({c + V(rx * std::cos(PI * k / 4.f), 0.f), hands}, 0.45f, 0, 0.8f, 0.3f);
}

struct Ctx {
    float s;   // the guy's size
    float top; // his margin's height (where things fall from)
};

float trickX(const Story::Ep& e, int upto, float s) {
    float x = e.p0.x;
    for (int i = 0; i < upto && i < (int)e.seq.size(); i++) x += e.dir * TRICK_MOVE[e.seq[i]] * s;
    return x;
}

// The guy at `t` beats into an episode, and what moves with him (drawn with `pen`, when given, in his margin's
// coordinates).
Fig figure(const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s;
    const float dur = (float)(e.b1 - e.b0);
    Fig f;
    f.side = e.side;
    f.dir = e.dir;
    f.at = e.p0;
    f.S = standing();
    switch (e.kind) {
    case K_WAVE: {
        f.S = fk(wave(t));
        onFeet(f.S);
        break;
    }
    case K_RUN: {
        const float u = saturate(t / dur);
        f.at = mixv(e.p0, e.p1, u);
        const float ph = std::min(t, dur) * e.rate * PI;
        f.S = fk(gait(ph, e.h));
        onFeet(f.S);
        if (e.h > 0.8f) shiftS(f.S, V(0.f, 0.08f * std::max(0.f, std::cos(2.f * ph))));
        // the last step: back to standing
        if (t > dur - 0.25f) f.S = mixS(f.S, standing(), saturate((t - dur + 0.25f) / 0.25f));
        break;
    }
    case K_HOP: {
        const int n = (int)e.pts.size() - 1;
        const int i = std::clamp((int)std::floor(t / 2.f), 0, n - 1);
        const float tl = t - 2.f * i;
        const vec2 a = e.pts[i], b = e.pts[i + 1];
        f.dir = b.x > a.x ? 1 : -1;
        Skel st = standing(), cr = fk(crouch()), lp = fk(leap());
        onFeet(cr);
        onFeet(lp);
        if (tl < 0.5f) { f.at = a; f.S = mixS(st, cr, ease(tl / 0.5f)); }
        else if (tl < 1.5f) {
            const float u = tl - 0.5f, hgt = std::max(0.f, b.y - a.y) + 0.9f * s;
            f.at = mixv(a, b, u) + V(0.f, hgt * 4.f * u * (1.f - u));
            f.S = u < 0.5f ? mixS(cr, lp, ease(u * 2.f)) : mixS(lp, cr, ease(u * 2.f - 1.f));
        } else if (tl < 2.f) { f.at = b; f.S = mixS(cr, st, ease((tl - 1.5f) / 0.5f)); }
        else { f.at = b; f.S = st; }
        break;
    }
    case K_LADDER: {
        const float lx = e.ea, h = e.h;
        const float climbT = h / (e.rate * RUNG * s);
        if (t < 1.f) {
            f.at = V(lerpf(e.p0.x, lx, t), 0.f);
            f.dir = lx >= e.p0.x ? 1 : -1;
            f.S = fk(gait(t * PI * 2.f, 0.5f));
            onFeet(f.S);
        } else if (t < 1.f + climbT) {
            // a rung at a time (each beat's step quick, then holding on)
            const float rungs = (t - 1.f) * e.rate, step = std::floor(rungs) + ease(frac(rungs) * 1.6f);
            f.at = V(lx, std::min(h, step * RUNG * s));
            f.dir = 1;
            f.S = fk(climb((t - 1.f) * e.rate * PI));
            onFeet(f.S);
        } else {
            const float u = saturate(t - 1.f - climbT);
            f.at = V(lerpf(lx, e.p1.x, ease(u)), h + 0.35f * s * std::sin(u * PI));
            f.S = mixS(fk(climb(0.f)), standing(), ease(u));
            onFeet(f.S);
        }
        break;
    }
    case K_CHUTE: {
        const float edge = e.ea, h = e.h;
        const float D = dur - 4.f;
        const float jumpX = edge + e.dir * 0.9f * s;
        if (t < 1.f) {
            f.at = V(lerpf(e.p0.x, edge, t), h);
            f.S = fk(gait(t * TAU, 0.6f));
            onFeet(f.S);
        } else if (t < 2.f) {
            const float u = t - 1.f;
            f.at = V(lerpf(edge, jumpX, u), h + 0.7f * s * u - 1.7f * s * u * u);
            f.S = fk(mixA(leap(), tada(), u));
            onFeet(f.S);
        } else if (t < 2.f + D) {
            const float u = (t - 2.f) / D;
            const float open = ease((t - 2.f) / 0.5f);
            f.at = V(lerpf(jumpX, e.p1.x, ease(u)), lerpf(h - 1.0f * s, 0.f, u));
            f.dir = 1;
            f.S = fk(hang(t));
            onFeet(f.S);
            f.pivot = f.at + V(0.f, 3.1f * s);
            f.turn = 0.2f * std::sin((t - 2.f) * PI * 0.25f) * (1.f - 0.6f * u);
            if (pen) {
                // the canopy over his hands, then both swaying together
                Fig still = f;
                still.turn = 0.f;
                const vec2 hands = (jointAt(still, HA0, s) + jointAt(still, HA1, s)) * 0.5f;
                const float rx = 1.3f * s * (0.35f + 0.65f * open), ry = 0.8f * s * (0.5f + 0.5f * open);
                const bool red = e.seq.empty() || e.seq[0] == 0;
                const size_t first = pen->out.size();
                canopy(*pen, f.at + V(0.f, 2.5f * s), rx, ry, hands, red ? pen->P.red : pen->P.blue, red ? pen->P.yellow : pen->P.green);
                for (size_t i = first; i < pen->out.size(); i++)
                    for (vec2& q : pen->out[i].pts) q = f.pivot + rot(q - f.pivot, f.turn);
            }
        } else {
            const float u = t - 2.f - D;
            f.at = e.p1;
            f.S = mixS(fk(crouch()), standing(), ease(u / 0.6f));
            if (u < 0.6f) onFeet(f.S);
            if (pen && u < 1.6f) { // the canopy collapsing behind him
                const float k = ease(u / 1.2f);
                Poly c;
                for (int i = 0; i <= 10; i++) {
                    const float x = -0.3f - 1.6f * i / 10.f;
                    c.push_back(e.p1 + V(e.dir * x * s, (0.9f * (1.f - k) + 0.25f) * s * std::sin(i / 10.f * PI) * (1.f - 0.3f * (i % 2))));
                }
                pen->line(c, 0.9f, e.seq.empty() || e.seq[0] == 0 ? pen->P.red : pen->P.blue);
            }
        }
        break;
    }
    case K_DIVE: {
        const float edge = e.ea, h = e.h;
        if (t < 1.f) {
            f.at = V(lerpf(e.p0.x, edge, t), h);
            f.S = fk(gait(t * TAU, 0.9f));
            onFeet(f.S);
        } else if (t < 3.f) {
            const float u = (t - 1.f) / 2.f;
            f.at = V(lerpf(edge, e.p1.x, u), lerpf(h, 0.f, u * u) + 1.4f * s * 4.f * u * (1.f - u));
            Skel tk = fk(tuck());
            onFeet(tk);
            const float in = ease(std::min(u, 1.f - u) * 5.f);
            f.S = mixS(standing(), tk, in);
            turnS(f.S, V(0.f, 0.9f), -TAU * ease(u));
        } else if (t < 3.6f) {
            f.at = e.p1;
            Skel cr = fk(crouch());
            onFeet(cr);
            f.S = mixS(cr, standing(), ease((t - 3.f) / 0.6f));
        } else {
            f.at = e.p1;
            f.S = fk(mixA(Ang{}, tada(), ease((t - 3.6f) / 0.3f)));
            onFeet(f.S);
        }
        break;
    }
    case K_DODGE: {
        const int n = std::max(1, (int)e.seq.size());
        const int i = std::clamp((int)std::floor(t / 6.f), 0, n - 1);
        const float tb = t - 6.f * i;
        const float x0 = e.p0.x + e.dir * 0.3f * s * i;
        const float xs = x0 + e.dir * 1.5f * s;              // where he stops
        const float xo = xs + e.dir * 0.3f * s;              // where it falls
        const float xb = x0 + e.dir * 0.3f * s;              // where he leaps back to
        const float tFall = 2.4f, tHit = 3.2f;
        if (tb < 2.f) {
            f.at = V(lerpf(x0, xs, tb / 2.f), 0.f);
            f.S = fk(gait(tb * PI * 1.5f, 0.55f));
            onFeet(f.S);
            if (tb > 1.8f) f.S = mixS(f.S, standing(), (tb - 1.8f) / 0.2f);
        } else if (tb < 2.7f) {
            f.at = V(xs, 0.f);
            f.S = fk(lookUp());
            onFeet(f.S);
        } else if (tb < 3.5f) {
            const float u = (tb - 2.7f) / 0.8f;
            f.at = V(lerpf(xs, xb, u), 1.0f * s * 4.f * u * (1.f - u));
            f.S = fk(flail(tb));
            onFeet(f.S);
        } else if (tb < 3.8f) {
            f.at = V(xb, 0.f);
            Skel cr = fk(crouch());
            onFeet(cr);
            f.S = mixS(cr, standing(), (tb - 3.5f) / 0.3f);
        } else if (tb < 5.f) {
            f.at = V(xb, 0.f);
            f.S = fk(relief(tb));
            onFeet(f.S);
        } else {
            f.at = V(xb, 0.f);
        }
        if (pen) {
            const int kind = e.seq.empty() ? 0 : e.seq[i];
            const Pal& P = pen->P;
            if (tb >= 2.f && tb < 3.f) pen->text(f.at + V(0.f, 2.55f * s), "!", P.red, 0.9f * s);
            if (tb >= tFall && tb < tHit) {
                const float u = (tb - tFall) / (tHit - tFall);
                pen->shape(ellipse(V(xo, 0.f), (0.2f + 0.5f * u) * s, 0.09f * s), 0.6f, 0, 0.55f);
                fallingThing(*pen, kind, V(xo, lerpf(c.top - 1.6f * s, 0.f, u * u)), s);
                pen->line({V(xo - 0.35f * s, lerpf(c.top - 1.6f * s, 0.f, u * u) + 1.5f * s),
                           V(xo - 0.35f * s, lerpf(c.top - 1.6f * s, 0.f, u * u) + 2.3f * s)}, 0.5f, 0, 0.6f);
                pen->line({V(xo + 0.35f * s, lerpf(c.top - 1.6f * s, 0.f, u * u) + 1.5f * s),
                           V(xo + 0.35f * s, lerpf(c.top - 1.6f * s, 0.f, u * u) + 2.3f * s)}, 0.5f, 0, 0.6f);
            } else if (tb >= tHit && tb < 5.6f) {
                fallingThing(*pen, kind, V(xo, 0.f), s);
                if (tb < tHit + 1.f) { // the crash: lines flying out, dust, a word
                    for (int k = 0; k < 3; k++)
                        for (int sd = -1; sd <= 1; sd += 2) {
                            const float a = 0.35f + 0.38f * k;
                            const vec2 o = V(xo + sd * 0.75f * s, 0.15f * s);
                            const vec2 d = V(sd * std::cos(a), std::sin(a));
                            pen->line({o + d * (0.15f * s), o + d * (0.55f * s)}, 0.7f);
                        }
                    for (int sd = -1; sd <= 1; sd += 2)
                        pen->line(arc(V(xo + sd * 1.0f * s, 0.f), 0.2f * s, 0.16f * s, 0.f, PI, 8), 0.6f, P.grey);
                    static const char* WORDS[OB_COUNT] = {"Clang!", "Crash!", "Smash!", "Bonk!"};
                    pen->text(V(xo, 2.1f * s), WORDS[kind], P.red, 0.6f * s);
                }
            }
        }
        break;
    }
    case K_ACRO: {
        const int n = std::max(1, (int)e.seq.size());
        const int i = std::clamp((int)std::floor(t / 4.f), 0, n - 1);
        const float tb = t - 4.f * i;
        const float x0 = trickX(e, i, s);
        const int tr = e.seq.empty() ? TR_STARJUMP : e.seq[i];
        Skel st = standing(), cr = fk(crouch()), td = fk(tada());
        onFeet(cr);
        onFeet(td);
        f.at = V(x0, 0.f);
        switch (tr) {
        case TR_CARTWHEEL: {
            Skel sr = fk(star());
            onFeet(sr);
            if (tb < 0.5f) f.S = mixS(st, sr, ease(tb / 0.5f));
            else if (tb < 3.5f) {
                const float u = (tb - 0.5f) / 3.f;
                f.S = fk(star());
                turnS(f.S, f.S.j[HIP], -TAU * 2.f * u);
                onGround(f.S);
                f.at = V(x0 + e.dir * TRICK_MOVE[tr] * s * u, 0.f);
            } else {
                f.at = V(x0 + e.dir * TRICK_MOVE[tr] * s, 0.f);
                f.S = mixS(sr, td, ease((tb - 3.5f) / 0.3f));
            }
            break;
        }
        case TR_BACKFLIP: {
            const float x1 = x0 + e.dir * TRICK_MOVE[tr] * s;
            if (tb < 0.6f) f.S = mixS(st, cr, ease(tb / 0.6f));
            else if (tb < 2.2f) {
                const float u = (tb - 0.6f) / 1.6f;
                Skel tk = fk(tuck());
                f.S = mixS(st, tk, ease(std::min(u, 1.f - u) * 4.f));
                shiftS(f.S, V(0.f, 0.86f - f.S.j[HIP].y));
                turnS(f.S, f.S.j[HIP], TAU * ease(u));
                f.at = V(lerpf(x0, x1, u), 1.9f * s * 4.f * u * (1.f - u));
            } else if (tb < 2.7f) { f.at = V(x1, 0.f); f.S = mixS(cr, st, ease((tb - 2.2f) / 0.5f)); }
            else { f.at = V(x1, 0.f); f.S = mixS(st, td, ease((tb - 2.7f) / 0.3f)); }
            break;
        }
        case TR_HANDSTAND: {
            // up onto his hands (turning over his hands), a few steps on them, and back down
            Ang a = armsUp();
            const float w0 = tb < 1.f ? ease(tb) : tb < 3.f ? 1.f : 1.f - ease(tb - 3.f); // (legs apart up there)
            const float walk = tb >= 1.f && tb < 3.f ? std::sin((tb - 1.f) * PI * 2.f) : 0.f;
            a.th[0] = 0.25f * walk + 0.35f * w0; a.sh[0] = a.th[0] - 0.1f;
            a.th[1] = -0.25f * walk - 0.35f * w0; a.sh[1] = a.th[1] + 0.35f * w0;
            Skel S = fk(a);
            const vec2 hands = (S.j[HA0] + S.j[HA1]) * 0.5f;
            turnS(S, hands, -PI * w0);
            onGround(S);
            const vec2 h2 = (S.j[HA0] + S.j[HA1]) * 0.5f;
            shiftS(S, V(hands.x - h2.x, 0.f));
            f.S = tb > 3.7f ? mixS(S, st, ease((tb - 3.7f) / 0.3f)) : S;
            break;
        }
        default: { // a star jump
            Skel sr = fk(star());
            onFeet(sr);
            if (tb < 0.6f) f.S = mixS(st, cr, ease(tb / 0.6f));
            else if (tb < 1.8f) {
                const float u = (tb - 0.6f) / 1.2f;
                f.S = u < 0.5f ? mixS(cr, sr, ease(u * 3.f)) : mixS(sr, cr, ease(u * 2.f - 1.f));
                f.at = V(x0, 1.7f * s * 4.f * u * (1.f - u));
            } else if (tb < 2.3f) f.S = mixS(cr, st, ease((tb - 1.8f) / 0.5f));
            else f.S = mixS(st, td, ease((tb - 2.3f) / 0.3f));
            break;
        }
        }
        break;
    }
    case K_CROSS: {
        const float hx = e.ea;
        Skel up = fk(tada());
        onFeet(up);
        if (t < 1.f) {
            // he throws the hole down in front of him
            Ang a;
            a.ua[0] = lerpf(2.2f, 0.9f, ease(t * 2.f));
            a.fa[0] = a.ua[0] + 0.2f;
            f.S = fk(a);
            onFeet(f.S);
        } else if (t < 2.f) {
            const float u = t - 1.f;
            f.at = V(lerpf(e.p0.x, hx, u), 0.8f * s * 4.f * u * (1.f - u));
            f.S = fk(leap());
            onFeet(f.S);
        } else if (t < 3.f) {
            const float u = t - 2.f;
            f.at = V(hx, -2.4f * s * u * u);
            f.S = up;
            f.clip = 0.f;
        } else if (t < 4.f) {
            f.shown = false;
            f.side = 1 - e.side;
        } else {
            f.side = 1 - e.side;
            f.clip = 0.f;
            const float u = t - 4.f;
            if (u < 1.5f) {
                const float k = u / 1.5f;
                f.at = V(e.p1.x, -2.4f * s + 3.6f * s * std::sin(k * PI * 0.62f));
                f.S = k < 0.7f ? up : mixS(up, fk(leap()), (k - 0.7f) / 0.3f);
                if (k >= 0.7f) onFeet(f.S);
                if (k > 0.75f) f.at.y = lerpf(f.at.y, 0.f, (k - 0.75f) / 0.25f);
            } else {
                f.at = V(e.p1.x, 0.f);
                Skel cr = fk(crouch());
                onFeet(cr);
                f.S = mixS(cr, standing(), ease((u - 1.5f) / 0.5f));
            }
        }
        break;
    }
    case K_REST: {
        const bool sleep = !e.seq.empty() && e.seq[0] == 1;
        Skel rest;
        if (sleep) rest = sleeping(t);
        else { rest = fk(sit(t)); onGround(rest); }
        if (t < 1.f) f.S = mixS(standing(), rest, ease(t));
        else if (t < dur - 1.f) f.S = rest;
        else f.S = mixS(rest, standing(), ease(t - (dur - 1.f)));
        if (pen && sleep && t >= 1.5f && t < dur - 1.f) {
            const vec2 head = jointAt(f, HEAD, s);
            for (int k = 0; k < 3; k++) {
                const float a = frac((t - 1.5f) / 3.f + k / 3.f) * 3.f; // three z rising, a beat apart
                pen->text(head + V(e.dir * (0.3f + 0.35f * a) * s, (0.35f + 0.55f * a) * s), "z", pen->P.blue, (0.45f + 0.2f * a) * s,
                          1.f - 0.25f * a);
            }
        }
        break;
    }
    default: break;
    }
    return f;
}

// The guy drawn: his head, his body, his arms and legs, his scarf trailing behind him (`vel`: how his neck moves,
// board units a beat).
void drawGuy(Pen& pen, const Fig& f, float s, vec2 vel, float flutter, float wk) {
    if (!f.shown) return;
    const Pal& P = pen.P;
    auto W = [&](int j) { return jointAt(f, j, s); };
    auto put = [&](const Poly& p, float k) {
        for (const Poly& run : above(p, f.clip)) pen.line(run, k, 0, 1.f, 0.6f);
    };
    // the scarf first (under the outline; not when he is upside down, it would hang over his face)
    if (f.S.j[HEAD].y > f.S.j[NECK].y + 0.1f) {
        const vec2 neck = W(NECK), head = W(HEAD);
        const float sp = len(vel);
        vec2 d = unit(vel * (-1.f) + V(0.f, -0.9f * s));
        if (len(d) < 0.5f) d = V(-(float)f.dir, -0.3f);
        const vec2 nrm = V(-d.y, d.x);
        const float amp = std::min(1.f, sp / (2.f * s)) * 0.09f * s;
        Poly sc;
        const vec2 root = mixv(neck, head, 0.25f);
        for (int i = 0; i <= 4; i++) {
            vec2 q = root + d * (0.15f * s * i) + nrm * (amp * std::sin(flutter + i * 1.7f) * (i / 4.f));
            q.y = std::max(q.y, f.at.y + 0.03f * s); // (not through the floor)
            sc.push_back(q);
        }
        for (const Poly& run : above(sc, f.clip)) pen.raw(hand(run, pen.r, 0.01f), P.red, pen.v == 0 ? T_MARKER : P.tool, 0.1f * s * wk);
    }
    Rng keep = pen.r;
    const vec2 hc = W(HEAD);
    Poly head = ellipse(hc, HEADR * s, HEADR * s * 1.04f);
    if (f.clip < -1e8f) pen.shape(head, wk);
    else for (const Poly& run : above(loopOf(head, pen.r), f.clip)) pen.line(run, wk);
    pen.r = keep;
    put({W(FT0), W(KN0), W(HIP), W(KN1), W(FT1)}, wk);
    put({W(HIP), W(NECK)}, wk);
    put({W(HA0), W(EL0), W(NECK), W(EL1), W(HA1)}, wk);
}

void drawProp(Pen& pen, const Story::Prop& p, float hw, float s) {
    const Pal& P = pen.P;
    switch (p.kind) {
    case PR_GROUND: {
        pen.begin(p.id, (float)p.t0, 16.f, 0.25f);
        Poly g;
        const int n = 12;
        for (int i = 0; i <= n; i++) {
            const float x = -hw + 0.3f + (2.f * hw - 0.6f) * i / n;
            g.push_back(V(x, -0.05f + 0.04f * std::sin(x * 1.7f + p.id)));
        }
        pen.line(g, 0.9f);
        // a few tufts of grass
        Rng r(mixk(p.id, 77));
        for (int k = 0; k < 4; k++) {
            const float x = -hw + 0.8f + (2.f * hw - 1.6f) * (k + r.range(0.2f, 0.8f)) / 4.f;
            pen.line({V(x - 0.12f * s, -0.02f), V(x - 0.04f * s, 0.2f * s)}, 0.6f, P.green);
            pen.line({V(x, -0.02f), V(x + 0.02f * s, 0.26f * s)}, 0.6f, P.green);
            pen.line({V(x + 0.1f * s, -0.02f), V(x + 0.16f * s, 0.17f * s)}, 0.6f, P.green);
        }
        break;
    }
    case PR_LEDGE: {
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        const float th = 0.24f * s;
        const Poly slab = rect(p.xa, p.h - th, p.xb, p.h);
        pen.shape(slab, 0.9f);
        const uint32_t cols[3] = {P.orange, P.green, P.blue};
        pen.colour(slab, cols[p.id % 3], 0.35f, 0.7f * s);
        break;
    }
    case PR_LADDER: {
        pen.begin(p.id, (float)p.t0, 16.f, 0.3f);
        const float x = p.xa, hwid = 0.3f * s;
        pen.line({V(x - hwid, 0.f), V(x - hwid, p.h + 0.55f * s)}, 0.9f, 0, 1.f, 0.6f);
        pen.line({V(x + hwid, 0.f), V(x + hwid, p.h + 0.55f * s)}, 0.9f, 0, 1.f, 0.6f);
        for (float y = 0.3f * s; y < p.h + 0.3f * s; y += RUNG * s) pen.line({V(x - hwid, y), V(x + hwid, y)}, 0.7f, 0, 1.f, 0.3f);
        break;
    }
    default: { // a hole
        pen.begin(p.id, (float)p.t0, 10.f, 0.3f);
        const Poly e = ellipse(V(p.xa, 0.f), 0.68f * s, 0.18f * s);
        pen.shape(e, 0.9f);
        pen.dark(e, 0.15f, 0.7f * s);
        break;
    }
    }
}

// ---------------------------------------------------------------- the drop's dance
enum { MV_TOPROCK, MV_FOOTWORK, MV_WINDMILL, MV_HEADSPIN, MV_FREEZE, MV_COUNT };

vec2 proj(const vec3& p) { return V(p.x, p.y); }
vec3 nrm3(vec3 a) {
    const float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return l > 1e-6f ? a * (1.f / l) : a;
}

Skel toprock(float tb) {
    // a step across on each beat, arms opening and closing, a bounce on the half beat
    Ang k[4];
    k[0].th[0] = 0.5f; k[0].sh[0] = 0.25f; k[0].th[1] = -0.2f; k[0].sh[1] = -0.3f;
    k[0].ua[0] = 1.4f; k[0].fa[0] = 2.2f; k[0].ua[1] = -1.3f; k[0].fa[1] = -0.6f;
    k[0].spine = 0.12f; k[0].head = 0.1f;
    k[1].th[0] = 0.22f; k[1].sh[0] = 0.0f; k[1].th[1] = -0.22f; k[1].sh[1] = -0.4f;
    k[1].ua[0] = 0.7f; k[1].fa[0] = 2.0f; k[1].ua[1] = 0.6f; k[1].fa[1] = 1.9f;
    k[1].spine = 0.05f; k[1].hip = V(0.f, 0.78f);
    k[2] = k[0];
    k[2].th[0] = -0.2f; k[2].sh[0] = -0.3f; k[2].th[1] = 0.5f; k[2].sh[1] = 0.25f;
    k[2].ua[0] = -1.3f; k[2].fa[0] = -0.6f; k[2].ua[1] = 1.4f; k[2].fa[1] = 2.2f;
    k[2].head = -0.1f;
    k[3] = k[1];
    k[3].ua[0] = 0.5f; k[3].ua[1] = 0.8f;
    const float q = tb * 2.f;
    const int i = ((int)std::floor(q)) & 3;
    Skel S = fk(mixA(k[i], k[(i + 1) & 3], ease(frac(q))));
    onFeet(S);
    return S;
}
Skel footwork(float tb) {
    // down on one hand, the legs stepping round him
    const float ph = tb * PI;
    Ang a;
    a.hip = V(0.f, 0.45f);
    a.spine = 1.05f;
    a.head = -0.5f;
    a.ua[0] = 0.15f; a.fa[0] = 0.1f;
    a.ua[1] = -1.4f + 0.5f * std::sin(ph); a.fa[1] = a.ua[1] + 0.6f;
    a.th[0] = 0.6f + 1.0f * std::sin(ph); a.sh[0] = a.th[0] - 1.1f + 0.5f * std::cos(ph);
    a.th[1] = 0.2f + 1.0f * std::sin(ph + PI); a.sh[1] = a.th[1] - 1.0f + 0.5f * std::cos(ph + PI);
    Skel S = fk(a);
    onGround(S);
    return S;
}
Skel windmill(float tb) {
    // rolling round on his shoulders, the legs flying round wide
    const float th = tb * PI;
    const float tau = 1.15f;
    const vec3 neck(0.f, 0.3f, 0.f);
    const vec3 d(std::sin(tau) * std::cos(th), std::cos(tau), std::sin(tau) * std::sin(th));
    const vec3 perp(-std::sin(th), 0.f, std::cos(th));
    Skel S;
    const vec3 hip = neck + d * TORSO;
    S.j[NECK] = proj(neck);
    S.j[HIP] = proj(hip);
    S.j[HEAD] = proj(neck - d * NECKL + vec3(0.f, -0.02f, 0.f));
    for (int k = 0; k < 2; k++) {
        const vec3 L = nrm3(d + perp * (k ? -0.95f : 0.95f) + vec3(0.f, 0.45f, 0.f));
        const vec3 knee = hip + L * THIGH;
        S.j[KN0 + 2 * k] = proj(knee);
        S.j[FT0 + 2 * k] = proj(knee + L * SHIN);
    }
    // one hand down pushing, the other tucked in
    S.j[EL0] = proj(neck + vec3(0.2f, -0.2f, 0.f));
    S.j[HA0] = proj(neck + vec3(0.45f, -0.3f, 0.f));
    const vec3 back = neck - d * 0.15f + perp * 0.25f;
    S.j[EL1] = proj(back);
    S.j[HA1] = proj(back + perp * 0.25f + vec3(0.f, 0.15f, 0.f));
    onGround(S);
    return S;
}
Skel headspin(float tb) {
    // upside down on his head, the legs in a V spinning round, a turn a beat
    const float th = tb * TAU;
    const float sg = 0.8f;
    Skel S;
    const vec3 head(0.f, HEADR, 0.f);
    const vec3 neck(0.f, HEADR + NECKL, 0.f);
    const vec3 hip = neck + vec3(0.04f * std::sin(th), TORSO, 0.04f * std::cos(th));
    S.j[HEAD] = proj(head);
    S.j[NECK] = proj(neck);
    S.j[HIP] = proj(hip);
    for (int k = 0; k < 2; k++) {
        const float a = th + k * PI;
        const vec3 L(std::sin(sg) * std::cos(a), std::cos(sg), std::sin(sg) * std::sin(a));
        const vec3 knee = hip + L * THIGH;
        S.j[KN0 + 2 * k] = proj(knee);
        S.j[FT0 + 2 * k] = proj(knee + L * SHIN);
        // the arms out, turning with him, a quarter turn behind the legs
        const float b = a + PI * 0.5f;
        const vec3 el = neck + vec3(0.28f * std::cos(b), 0.02f, 0.28f * std::sin(b));
        S.j[EL0 + 2 * k] = proj(el);
        S.j[HA0 + 2 * k] = proj(el + vec3(0.26f * std::cos(b), 0.12f, 0.26f * std::sin(b)));
    }
    return S;
}
Skel freeze(int kind) {
    Skel S;
    auto set = [&](std::initializer_list<vec2> v) { int i = 0; for (vec2 p : v) S.j[i++] = p; };
    switch (kind % 3) {
    case 0: // a baby freeze: on his hands and his head, the knees tucked up in the air
        set({V(0.35f, 0.72f), V(-0.15f, 0.42f), V(-0.38f, 0.22f), V(0.1f, 0.3f), V(0.05f, 0.f), V(-0.45f, 0.26f), V(-0.55f, 0.f),
             V(0.75f, 1.12f), V(1.15f, 0.92f), V(0.72f, 0.58f), V(1.08f, 0.72f)});
        break;
    case 1: // a handstand, one leg bent
        set({V(0.f, 1.16f), V(0.f, 0.58f), V(0.f, 0.36f), V(-0.24f, 0.29f), V(-0.22f, 0.f), V(0.24f, 0.29f), V(0.22f, 0.f),
             V(-0.36f, 1.52f), V(-0.74f, 1.78f), V(0.28f, 1.55f), V(0.08f, 1.95f)});
        break;
    default: // on one hand, body out flat, the other arm up
        set({V(0.62f, 0.78f), V(0.06f, 0.62f), V(-0.17f, 0.72f), V(0.03f, 0.31f), V(0.f, 0.f), V(-0.18f, 0.88f), V(-0.32f, 1.14f),
             V(1.02f, 1.06f), V(1.42f, 1.26f), V(1.03f, 0.82f), V(1.45f, 0.88f)});
        break;
    }
    return S;
}
Skel bow(float u) {
    Ang a;
    a.spine = 1.1f * ease(u);
    a.head = 0.3f * ease(u);
    a.ua[0] = lerpf(0.12f, 0.7f, ease(u)); a.fa[0] = lerpf(0.25f, 1.7f, ease(u));
    a.ua[1] = lerpf(-0.12f, -1.2f, ease(u)); a.fa[1] = lerpf(-0.02f, -1.0f, ease(u));
    Skel S = fk(a);
    onFeet(S);
    return S;
}
Skel move(int kind, int var, float tb) {
    switch (kind) {
    case MV_TOPROCK: return toprock(tb);
    case MV_FOOTWORK: return footwork(tb);
    case MV_WINDMILL: return windmill(tb);
    case MV_HEADSPIN: return headspin(tb);
    default: return freeze(var);
    }
}

// The dance's run of moves: toprock, down to the floor, a spin, a freeze, again with the other spin.
struct Seg { int kind, var; float beats; };
std::vector<Seg> routine(uint64_t seed, int round) {
    Rng r(mixk(seed, (uint64_t)round));
    const bool windFirst = r.chance(0.5f);
    const int f0 = r.irange(0, 2), f1 = (f0 + r.irange(1, 2)) % 3;
    return {{MV_TOPROCK, 0, 8.f},
            {MV_FOOTWORK, 0, 4.f},
            {windFirst ? MV_WINDMILL : MV_HEADSPIN, 0, 6.f},
            {MV_FREEZE, f0, 2.f},
            {MV_TOPROCK, 0, 4.f},
            {MV_FOOTWORK, 0, 4.f},
            {windFirst ? MV_HEADSPIN : MV_WINDMILL, 0, 6.f},
            {MV_FREEZE, f1, 2.f}};
}
Skel dancer(uint64_t seed, float rel) {
    const float cycle = 36.f;
    const int round = (int)std::floor(rel / cycle);
    const std::vector<Seg> R = routine(seed, round);
    float t = rel - round * cycle, start = 0.f;
    size_t i = 0;
    while (i + 1 < R.size() && t >= start + R[i].beats) start += R[i++].beats;
    const float tb = t - start;
    Skel S = move(R[i].kind, R[i].var, tb);
    // from the last move into this one, in half a beat
    if (tb < 0.5f && rel > 0.5f) {
        const Seg& p = i > 0 ? R[i - 1] : routine(seed, round - 1).back();
        S = mixS(move(p.kind, p.var, p.beats), S, ease(tb / 0.5f));
    }
    return S;
}

// A tiny stick figure in the crowd, cheering on the beats.
void cheer(Pen& pen, vec2 at, float s, int i, double beat) {
    const int b = (int)std::floor(beat * 2.0);
    Ang a;
    const bool up = ((b + i) & 1) == 0;
    a.ua[0] = up ? 2.6f : 2.0f; a.fa[0] = up ? 2.9f : 2.9f;
    a.ua[1] = up ? -2.6f : -0.4f; a.fa[1] = up ? -2.9f : 0.3f;
    if (i % 3 == 1) { a.ua[1] = up ? -2.0f : -2.6f; a.fa[1] = -2.9f; } // clapping over his head
    a.head = up ? -0.15f : 0.1f;
    Skel S = fk(a);
    onFeet(S);
    const float hop = (i % 2 == 0 && (b & 3) == 0) ? 0.25f : 0.f;
    Fig f;
    f.S = S;
    f.at = at + V(0.f, hop * s);
    f.dir = (i % 2) ? -1 : 1;
    auto W = [&](int j) { return jointAt(f, j, s); };
    pen.shape(ellipse(W(HEAD), HEADR * s, HEADR * s), 0.8f);
    pen.line({W(FT0), W(KN0), W(HIP), W(KN1), W(FT1)}, 0.8f, 0, 1.f, 0.4f);
    pen.line({W(HIP), W(NECK)}, 0.8f, 0, 1.f, 0.4f);
    pen.line({W(HA0), W(EL0), W(NECK), W(EL1), W(HA1)}, 0.8f, 0, 1.f, 0.4f);
}

// A music note (an eighth), `sz` tall.
void note(Pen& pen, vec2 at, float sz, uint32_t col, bool two) {
    const Poly head = ellipse(at, 0.17f * sz, 0.12f * sz);
    pen.shape(head, 0.7f, col);
    pen.colour(head, col, 0.4f, 0.35f);
    pen.line({at + V(0.15f * sz, 0.03f * sz), at + V(0.15f * sz, 0.8f * sz)}, 0.7f, col);
    if (two) {
        const vec2 b = at + V(0.5f * sz, 0.1f * sz);
        const Poly h2 = ellipse(b, 0.17f * sz, 0.12f * sz);
        pen.shape(h2, 0.7f, col);
        pen.colour(h2, col, 0.4f, 0.35f);
        pen.line({b + V(0.15f * sz, 0.03f * sz), b + V(0.15f * sz, 0.9f * sz)}, 0.7f, col);
        pen.line({at + V(0.15f * sz, 0.8f * sz), b + V(0.15f * sz, 0.9f * sz)}, 1.1f, col);
    } else {
        pen.line({at + V(0.15f * sz, 0.8f * sz), at + V(0.38f * sz, 0.55f * sz), at + V(0.36f * sz, 0.35f * sz)}, 0.7f, col);
    }
}

// ids of the dance's things (out of the props' range)
const uint32_t ID_GUY = 0x3F0000u, ID_DANCE = 0x3F1000u;

} // namespace

// =====================================================================================================
// The chain of episodes
// =====================================================================================================

void Story::reset(uint64_t seed) {
    seed_ = seed;
    rng_ = Rng(mixk(seed, 0x570C));
    eps_.clear();
    props_.clear();
    holding_.clear();
    chain0_ = -1;
    dancing_ = false;
    danceEnd_ = -1;
    danceNo_ = 0;
}

Story::Prop* Story::prop(uint32_t id) {
    for (Prop& p : props_) if (p.id == id) return &p;
    return nullptr;
}

void Story::leave(double when) {
    for (uint32_t id : holding_)
        if (Prop* p = prop(id)) p->t1 = std::min(p->t1, when + 1.5);
    holding_.clear();
}

void Story::restart(double beat, int side, float x) {
    eps_.clear();
    props_.clear();
    holding_.clear();
    chain0_ = beat;
    at_ = At{};
    at_.side = side;
    at_.x = x;
    at_.dir = rng_.chance(0.5f) ? 1 : -1;
    Prop g;
    g.kind = PR_GROUND;
    g.side = side;
    g.t0 = beat;
    g.id = serial_++;
    ground_ = g.id;
    props_.push_back(g);
    // a hello first, to the next even beat
    Ep e;
    e.kind = K_WAVE;
    e.b0 = beat;
    e.b1 = std::ceil((beat + 2.0) / 2.0) * 2.0;
    e.side = side;
    e.dir = at_.dir;
    e.p0 = e.p1 = V(x, 0.f);
    e.id = serial_++;
    eps_.push_back(e);
    at_.recent[0] = K_WAVE;
}

void Story::extend(double until, int stage, const Panel P[2]) {
    const float s = s_;
    while (!eps_.empty() && eps_.back().b1 < until) {
        const int side = at_.side;
        const float hw = 0.5f * (P[side].x1 - P[side].x0), H = P[side].y1 - P[side].y0;
        const float m = 0.9f * s, lo = -hw + m, hi = hw - m;
        const bool ledge = at_.y > 0.01f;
        Ep e;
        e.b0 = eps_.back().b1;
        e.side = side;
        e.dir = at_.dir;
        e.p0 = V(std::clamp(at_.x, lo, hi), at_.y);
        e.id = serial_++;
        const float x = e.p0.x, y = e.p0.y;
        auto fits = [&](float v) { return v >= lo && v <= hi; };
        // what he can do from here
        float w[K_COUNT] = {};
        if (ledge) {
            w[K_CHUTE] = 3.f;
            w[K_DIVE] = stage >= 1 ? 2.f : 0.6f;
            w[K_HOP] = 2.f;
            if (!fits(at_.lb + 2.4f * s) && !fits(at_.la - 2.4f * s)) w[K_DIVE] = 0.f;
        } else {
            w[K_RUN] = hi - lo > 2.5f * s ? 3.f : 0.f;
            w[K_HOP] = H > 8.f * s ? 2.f : 0.f;
            w[K_LADDER] = H > 9.f * s && hi - lo > 2.8f * s ? 2.f : 0.f;
            w[K_DODGE] = hi - lo > 2.8f * s ? (stage >= 1 ? 2.f : 1.f) : 0.f;
            w[K_ACRO] = stage >= 2 ? 3.f : stage == 1 ? 2.f : 0.6f;
            w[K_CROSS] = at_.sinceCross >= 3 ? 1.8f : 0.f;
            w[K_REST] = stage == 0 ? 3.f : 0.3f;
            w[K_WAVE] = 0.4f;
        }
        // not what he just did, and seldom what he did lately
        for (int i = 1; i < 4; i++)
            if (at_.recent[i] >= 0) w[at_.recent[i]] *= 0.25f;
        if (at_.recent[0] >= 0) {
            float rest = 0.f;
            for (int k = 0; k < K_COUNT; k++) if (k != at_.recent[0]) rest += w[k];
            if (rest > 0.f) w[at_.recent[0]] = 0.f;
        }
        e.kind = rng_.weighted(w);
        const float stepBeat = stage >= 1 ? 2.f : 1.f;
        auto ledgeOf = [&](float px, float py, double t0, uint32_t* id) {
            Prop p;
            p.kind = PR_LEDGE;
            p.side = side;
            p.xa = px - 0.85f * s;
            p.xb = px + 0.85f * s;
            p.h = py;
            p.t0 = t0;
            p.id = serial_++;
            props_.push_back(p);
            if (id) *id = p.id;
        };
        switch (e.kind) {
        case K_RUN: {
            float tx = x < 0.f ? rng_.range(std::max(lo, hi * 0.3f), hi) : rng_.range(lo, std::min(hi, lo * 0.3f));
            if (std::fabs(tx - x) < 1.5f * s) tx = x < 0.f ? hi : lo;
            const float dist = std::fabs(tx - x);
            const float stepLen = (stage >= 1 ? 1.1f : 0.7f) * s;
            const float d = std::max(2.f, std::round(dist / (stepLen * stepBeat)));
            e.dir = tx >= x ? 1 : -1;
            e.p1 = V(tx, 0.f);
            e.rate = stepBeat;
            e.h = std::clamp(dist / (d * stepBeat) / (1.15f * s), 0.4f, 1.1f);
            e.b1 = e.b0 + d;
            break;
        }
        case K_HOP: {
            int d = e.dir;
            e.pts.push_back(e.p0);
            vec2 p = e.p0;
            std::vector<uint32_t> ids;
            if (!ledge) {
                const int n = std::clamp((int)((H - 5.f * s) / (1.6f * s)), 1, rng_.irange(2, 4));
                for (int k = 0; k < n; k++) {
                    float dx = rng_.range(1.5f, 2.1f) * s;
                    if (!fits(p.x + d * dx)) d = -d;
                    if (!fits(p.x + d * dx)) dx = 0.8f * s;
                    p = V(std::clamp(p.x + d * dx, lo, hi), p.y + rng_.range(1.3f, 1.8f) * s);
                    e.pts.push_back(p);
                }
            } else {
                leave(e.b0 + 0.5);
                const int n = std::max(1, (int)std::ceil(y / (1.8f * s)));
                for (int k = 0; k < n; k++) {
                    float dx = rng_.range(1.4f, 2.f) * s;
                    if (!fits(p.x + d * dx)) d = -d;
                    p = V(std::clamp(p.x + d * dx, lo, hi), k == n - 1 ? 0.f : y * (1.f - (k + 1) / (float)n));
                    e.pts.push_back(p);
                }
            }
            const int n = (int)e.pts.size() - 1;
            for (int k = 0; k < n; k++) {
                const vec2 q = e.pts[k + 1];
                if (q.y <= 0.01f) continue;
                uint32_t id = 0;
                ledgeOf(q.x, q.y, e.b0 + 2.0 * k - 0.6, &id);
                if (k < n - 1) prop(id)->t1 = e.b0 + 2.0 * (k + 1) + 2.0;
                else holding_.push_back(id);
            }
            e.p1 = e.pts.back();
            e.dir = d;
            e.b1 = e.b0 + 2.0 * n;
            at_.y = e.p1.y;
            at_.la = e.p1.x - 0.85f * s;
            at_.lb = e.p1.x + 0.85f * s;
            break;
        }
        case K_LADDER: {
            const float maxH = std::min(H - 4.5f * s, 6.5f * s);
            float h = rng_.range(4.f * s, std::max(4.1f * s, maxH));
            h = std::max(2.f, std::round(h / (RUNG * s))) * RUNG * s;
            int d = e.dir;
            float lx = x + d * 0.9f * s;
            if (!fits(lx + d * 2.4f * s) || !fits(lx)) { d = -d; lx = x + d * 0.9f * s; }
            if (!fits(lx + d * 2.4f * s) || !fits(lx)) { d = x < 0.f ? 1 : -1; lx = std::clamp(x, lo, hi); }
            e.dir = d;
            e.ea = lx;
            e.h = h;
            e.rate = stage >= 1 ? 2.f : 1.f;
            const float climbT = h / (e.rate * RUNG * s);
            e.b1 = e.b0 + std::ceil(2.f + climbT);
            e.p1 = V(lx + d * 1.1f * s, h);
            Prop lad;
            lad.kind = PR_LADDER;
            lad.side = side;
            lad.xa = lx;
            lad.h = h;
            lad.t0 = e.b0 - 1.2;
            lad.id = serial_++;
            props_.push_back(lad);
            holding_.push_back(lad.id);
            Prop led;
            led.kind = PR_LEDGE;
            led.side = side;
            led.xa = std::min(lx - d * 0.35f * s, lx + d * 2.4f * s);
            led.xb = std::max(lx - d * 0.35f * s, lx + d * 2.4f * s);
            led.h = h + 0.02f;
            led.t0 = e.b0 - 0.2;
            led.id = serial_++;
            props_.push_back(led);
            holding_.push_back(led.id);
            at_.y = h;
            at_.la = led.xa;
            at_.lb = led.xb;
            break;
        }
        case K_CHUTE:
        case K_DIVE: {
            const float reach = e.kind == K_DIVE ? 2.4f * s : 0.6f * s;
            int d = (hi - at_.lb) >= (at_.la - lo) ? 1 : -1;
            if (e.kind == K_DIVE && !fits((d > 0 ? at_.lb : at_.la) + d * reach)) d = -d;
            const float edge = (d > 0 ? at_.lb : at_.la) - d * 0.15f * s;
            e.dir = d;
            e.ea = edge;
            e.h = y;
            leave(e.b0 + 1.0);
            if (e.kind == K_DIVE) {
                e.p1 = V(std::clamp(edge + d * 2.2f * s, lo, hi), 0.f);
                e.b1 = e.b0 + 5.0;
            } else {
                e.p1 = V(std::clamp(edge + d * rng_.range(0.8f, 2.5f) * s, lo, hi), 0.f);
                const float D = std::max(3.f, std::ceil((y - 1.f * s) / (1.15f * s)));
                e.b1 = e.b0 + 4.0 + D;
                e.seq.push_back(rng_.irange(0, 1));
            }
            at_.y = 0.f;
            break;
        }
        case K_DODGE: {
            const int n = stage >= 2 ? 2 : 1;
            int d = e.dir;
            const float need = (2.6f + 0.3f * (n - 1)) * s;
            if (!fits(x + d * need)) d = -d;
            if (!fits(x + d * need)) { e.kind = K_WAVE; e.b1 = e.b0 + 2.0; e.p1 = e.p0; break; }
            e.dir = d;
            const int first = rng_.irange(0, OB_COUNT - 1);
            for (int k = 0; k < n; k++) e.seq.push_back((first + k * rng_.irange(1, OB_COUNT - 1)) % OB_COUNT);
            e.p1 = V(x + d * 0.3f * s * n, 0.f);
            e.b1 = e.b0 + 6.0 * n;
            break;
        }
        case K_ACRO: {
            const int n = stage >= 2 ? 3 : 2;
            int d = (hi - x) >= (x - lo) ? 1 : -1;
            e.dir = d;
            float px = x;
            int last = -1;
            for (int k = 0; k < n; k++) {
                int tr = rng_.irange(0, TR_COUNT - 1);
                if (tr == last) tr = (tr + 1) % TR_COUNT;
                if (!fits(px + d * TRICK_MOVE[tr] * s)) tr = TR_STARJUMP;
                e.seq.push_back(tr);
                px += d * TRICK_MOVE[tr] * s;
                last = tr;
            }
            e.p1 = V(px, 0.f);
            e.b1 = e.b0 + 4.0 * n;
            break;
        }
        case K_CROSS: {
            int d = e.dir;
            if (!fits(x + d * 1.1f * s)) d = -d;
            e.dir = d;
            e.ea = x + d * 1.1f * s;
            const int other = 1 - side;
            const float hw2 = 0.5f * (P[other].x1 - P[other].x0);
            e.p1 = V(rng_.range(-0.45f, 0.45f) * std::max(0.f, hw2 - m), 0.f);
            e.b1 = e.b0 + 6.0;
            Prop a;
            a.kind = PR_HOLE;
            a.side = side;
            a.xa = e.ea;
            a.t0 = e.b0 + 0.3;
            a.t1 = e.b0 + 3.5;
            a.id = serial_++;
            props_.push_back(a);
            Prop b = a;
            b.side = other;
            b.xa = e.p1.x;
            b.t0 = e.b0 + 2.6;
            b.t1 = e.b0 + 6.8;
            b.id = serial_++;
            props_.push_back(b);
            if (Prop* g = prop(ground_)) g->t1 = e.b0 + 4.0;
            Prop g;
            g.kind = PR_GROUND;
            g.side = other;
            g.t0 = e.b0 + 2.0;
            g.id = serial_++;
            ground_ = g.id;
            props_.push_back(g);
            at_.side = other;
            at_.sinceCross = -1;
            break;
        }
        case K_REST: {
            e.seq.push_back(stage == 0 && rng_.chance(0.6f) ? 1 : 0);
            e.p1 = e.p0;
            e.b1 = e.b0 + 8.0;
            break;
        }
        default: { // a wave
            e.kind = K_WAVE;
            e.p1 = e.p0;
            e.b1 = e.b0 + 2.0;
            break;
        }
        }
        at_.x = e.p1.x;
        at_.dir = e.dir;
        for (int i = 3; i > 0; i--) at_.recent[i] = at_.recent[i - 1];
        at_.recent[0] = e.kind;
        at_.sinceCross++;
        eps_.push_back(e);
    }
}

void Story::update(double beat, int stage, const Panel P[2], float danceAt) {
    const float w = std::min(P[0].x1 - P[0].x0, P[1].x1 - P[1].x0);
    s_ = std::clamp(w / 7.4f, 0.7f, 1.3f);
    // the drop: the dance, until it ends and he has bowed
    if (stage >= 3) {
        if (!dancing_ || danceEnd_ >= 0.0) {
            int side = at_.side;
            for (const Ep& e : eps_)
                if (beat >= e.b0 && beat < e.b1) side = (e.kind == K_CROSS && beat >= e.b0 + 3.0) ? 1 - e.side : e.side;
            dancing_ = true;
            danceEnd_ = -1;
            dance0_ = beat - danceAt;
            danceSide_ = side;
            danceNo_++;
            danceSeed_ = mixk(seed_, (uint64_t)danceNo_);
            eps_.clear();
            props_.clear();
            holding_.clear();
        }
        return;
    }
    if (dancing_) {
        if (danceEnd_ < 0.0) danceEnd_ = beat;
        if (beat < danceEnd_ + 3.0 && beat >= danceEnd_) return;
        dancing_ = false;
        restart(beat, danceSide_, 0.f);
    }
    // a new run: at the start, after a seek
    if (eps_.empty() || beat < chain0_ - 0.5 || beat > eps_.back().b1 + 8.0)
        restart(beat, chain0_ < 0 ? (int)(seed_ & 1) : at_.side, 0.f);
    extend(beat + 16.0, stage, P);
    while (eps_.size() > 1 && eps_.front().b1 < beat - 1.0) eps_.erase(eps_.begin());
    props_.erase(std::remove_if(props_.begin(), props_.end(), [&](const Prop& p) { return p.t1 < beat - 1.0; }), props_.end());
}

// =====================================================================================================
// The drawing
// =====================================================================================================

void Story::build(int variant, double beat, const Panel P[2], std::vector<Elem>& out) const {
    out.clear();
    Pen pen(out, std::clamp(variant, 0, 2));
    const Pal& Pl = pen.P;
    const float s = s_;
    const vec2 org[2] = {V(0.5f * (P[0].x0 + P[0].x1), P[0].y0), V(0.5f * (P[1].x0 + P[1].x1), P[1].y0)};
    const float hw[2] = {0.5f * (P[0].x1 - P[0].x0), 0.5f * (P[1].x1 - P[1].x0)};
    const float H[2] = {P[0].y1 - P[0].y0, P[1].y1 - P[1].y0};
    auto place = [&](size_t from, int side) {
        for (size_t i = from; i < out.size(); i++)
            for (vec2& q : out[i].pts) q = q + org[side];
    };
    const float fb = (float)beat;
    const float chalky = pen.v == 2 ? 0.5f : 1.f; // (chalk dust shows more: what moves leaves less of it)

    if (dancing_) {
        const float rel = (float)(beat - dance0_);
        const float d0 = (float)dance0_;
        const int D = danceSide_, O = 1 - danceSide_;
        const float sd = std::clamp(std::min(1.75f * s, (hw[D] - 0.4f) / 1.8f), 0.6f * s, 2.2f);
        // what goes when the drop ends, one after the other
        int part = 0;
        auto alive = [&](float t0) {
            const bool ok = beat >= t0 && (danceEnd_ < 0.0 || beat < danceEnd_ + 0.3 * part);
            part++;
            return ok;
        };
        const float gone = danceEnd_ >= 0.0 ? (float)(beat - danceEnd_) : -1.f;
        // ---- the dance floor: the spotlight, the mat, the dancer
        size_t from = out.size();
        const float matW = std::min(2.9f * s, hw[D] - 0.4f);
        if (alive(d0)) {
            pen.begin(ID_DANCE + 1, d0, 12.f, 0.3f);
            const Poly mat = {V(-matW, 0.f), V(matW, 0.f), V(matW - 0.35f * s, 0.5f * s), V(-matW + 0.35f * s, 0.5f * s), V(-matW, 0.f)};
            pen.shape(mat, 0.9f);
            pen.colour(mat, Pl.card, 0.1f, 0.8f);
            pen.line({V(-matW * 0.4f, 0.03f), V(-matW * 0.4f + 0.12f * s, 0.47f * s)}, 0.5f, 0, 0.7f);
            pen.line({V(matW * 0.35f, 0.03f), V(matW * 0.35f - 0.1f * s, 0.47f * s)}, 0.5f, 0, 0.7f);
        }
        if (alive(d0 + 0.8f)) {
            pen.begin(ID_DANCE + 2, d0 + 0.8f, 18.f, 0.25f);
            // a lamp hanging over the floor
            const float top = H[D] - 0.4f;
            const vec2 lamp = V(-0.45f * hw[D] * (D == 0 ? 1.f : -1.f), std::min(top - 0.5f * s, 6.2f * sd));
            const vec2 mouth0 = lamp + V(-0.28f * s, -0.32f * s), mouth1 = lamp + V(0.28f * s, -0.32f * s);
            pen.line({V(lamp.x, top), lamp}, 0.7f);
            pen.shape({lamp + V(-0.18f * s, 0.12f * s), lamp + V(0.18f * s, 0.12f * s), mouth1, mouth0, lamp + V(-0.18f * s, 0.12f * s)}, 0.8f);
            const vec2 g0 = V(-matW * 0.95f, 0.25f * s), g1 = V(matW * 0.95f, 0.25f * s);
            pen.line({mouth0, g0}, 0.5f, 0, 0.5f, 0.3f);
            pen.line({mouth1, g1}, 0.5f, 0, 0.5f, 0.3f);
            const Poly cone = {mouth0, mouth1, g1, g0, mouth0};
            if (pen.v == 0) pen.segs(hatch(cone, 1.35f, 0.5f, pen.r, 0.f), Pl.light, T_HIGHLIGHT, 0.3f, 0.3f);
            else if (pen.v == 1) pen.segs(hatch(cone, 1.35f, 0.22f, pen.r, 0.f), Pl.light, T_CPENCIL, 0.04f, 0.3f);
            else pen.segs(hatch(cone, 1.35f, 0.32f, pen.r, 0.f), Pl.light, T_CHALK, 0.07f, 0.16f);
        }
        {
            Fig f;
            f.at = V(0.f, 0.22f * s);
            f.dir = 1;
            if (gone >= 0.f) {
                const Skel last = dancer(danceSeed_, (float)(danceEnd_ - dance0_));
                f.S = gone < 0.5f ? mixS(last, bow(0.f), ease(gone / 0.5f)) : bow(std::min(1.f, (gone - 0.6f) / 0.5f) * (gone < 2.4f ? 1.f : 0.f));
            } else {
                f.S = dancer(danceSeed_, rel);
            }
            // the scarf follows his neck
            Fig g = f;
            g.S = gone >= 0.f ? f.S : dancer(danceSeed_, std::max(0.f, rel - 0.1f));
            const vec2 vel = (jointAt(f, NECK, sd) - jointAt(g, NECK, sd)) * 10.f;
            pen.begin(ID_GUY, -1e6f, 0.f, chalky * 0.06f);
            drawGuy(pen, f, sd, vel, fb * 9.f, 1.6f);
        }
        place(from, D);
        // ---- the ambiance: the boombox and its notes, the disco ball, the crowd, a shout now and then
        from = out.size();
        const float side = O == 0 ? 1.f : -1.f; // towards the well
        const float bx = side * std::min(hw[O] - 1.6f * s, 0.45f * hw[O]);
        const float bw = 1.3f * s, bh = 0.75f * s;
        const bool hit = frac(beat) < 0.3f;
        if (alive(d0)) {
            pen.begin(ID_DANCE + 3, d0, 14.f, 0.3f);
            const Poly body = rect(bx - bw, 0.f, bx + bw, 2.f * bh);
            pen.shape(body);
            pen.colour(body, Pl.purple, 0.5f, 0.75f);
            pen.line(arc(V(bx, 2.f * bh), 0.8f * bw, 0.45f * bh, 0.15f, PI - 0.15f, 10), 0.8f);
            pen.shape(rect(bx - 0.28f * s, 0.95f * bh, bx + 0.28f * s, 1.55f * bh), 0.6f);
            pen.line({V(bx + 0.9f * bw, 2.f * bh), V(bx + 1.25f * bw, 3.4f * bh)}, 0.6f);
            // the speakers kick on the beat
            pen.begin(ID_DANCE + 4, d0 + 1.f, 0.f, 0.15f);
            for (int k = -1; k <= 1; k += 2) {
                const vec2 c = V(bx + k * 0.62f * bw, 0.9f * bh);
                const float r = (hit ? 0.5f : 0.42f) * bh;
                pen.shape(ellipse(c, r, r), 0.8f);
                pen.shape(ellipse(c, r * 0.35f, r * 0.35f), 0.6f);
                // the sound waves, one more on the beat
                for (int a = 0; a < (hit ? 3 : 2); a++) {
                    const float R = bw * 0.55f + 0.32f * s * a;
                    pen.line(arc(V(bx, bh), R + bw * 0.6f, R + 0.2f * s, k > 0 ? -0.5f : PI - 0.5f, k > 0 ? 0.5f : PI + 0.5f, 8), 0.6f, 0, 0.75f);
                }
            }
        }
        // notes flying out, one a beat
        if (alive(d0 + 1.f)) {
            const uint32_t cols[4] = {Pl.red, Pl.blue, Pl.green, Pl.orange};
            for (int k = 0; k < 3; k++) {
                const int nb = (int)std::floor(beat) - k;
                const float a = frac(beat) + k;
                if ((double)nb < dance0_ + 1.0) continue;
                Rng r(mixk(danceSeed_, (uint64_t)nb));
                const float drift = r.range(-1.f, 1.f) * 0.9f * s + side * 0.6f * s * a;
                pen.begin(ID_DANCE + 16 + (uint32_t)(nb % 64), -1e6f, 0.f, 0.04f);
                note(pen, V(bx + drift + 0.25f * s * std::sin(a * 3.f + nb), 2.f * bh + 0.4f * s + a * 1.2f * s), 0.75f * s,
                     cols[nb & 3], (nb % 3) == 0);
            }
        }
        // the disco ball, its rays turning on the beat
        if (alive(d0 + 2.5f)) {
            const float top = H[O] - 0.3f;
            const vec2 c = V(-side * 0.35f * hw[O], top - 2.3f * s);
            const float r = 0.62f * s;
            pen.begin(ID_DANCE + 5, d0 + 2.5f, 12.f, 0.3f);
            pen.line({V(c.x, top), c + V(0.f, r)}, 0.6f);
            const Poly ball = ellipse(c, r, r);
            pen.shape(ball, 0.9f);
            pen.colour(ball, Pl.grey, 0.8f, 0.6f, 0.8f);
            const int bb = (int)std::floor(beat);
            pen.begin(ID_DANCE + 6, d0 + 3.5f, 0.f, 0.12f);
            for (int k = -1; k <= 1; k++) pen.line(arc(c, r, r * 0.33f * std::fabs((float)k) + 0.02f, 0.f, PI, 10), 0.4f);
            for (int k = 0; k < 2; k++) {
                const float o = (bb & 1) ? 0.45f : -0.45f;
                const float ph = o + (k ? 0.5f : -0.5f);
                Poly m;
                for (int i = 0; i <= 10; i++) {
                    const float a = -PI * 0.5f + PI * i / 10.f;
                    m.push_back(c + V(r * std::sin(ph) * std::cos(a), r * std::sin(a)));
                }
                pen.line(m, 0.4f);
            }
            const uint32_t cols[4] = {Pl.yellow, Pl.red, Pl.blue, Pl.green};
            for (int k = 0; k < 7; k++) {
                const float a = k * TAU / 7.f + bb * 0.45f;
                const vec2 u = V(std::cos(a), std::sin(a));
                pen.line({c + u * (r * 1.25f), c + u * (r * (1.9f + 0.4f * ((k + bb) % 2)))}, 0.7f, cols[(k + bb) & 3]);
            }
        }
        // the crowd, cheering
        {
            const float cs = 0.95f * s;
            const float x0 = bx - side * (bw + 0.75f * s), x1 = -side * (hw[O] - 0.5f * s);
            const int n = std::clamp((int)(std::fabs(x1 - x0) / (0.85f * s)) + 1, 2, 6);
            for (int i = 0; i < n; i++) {
                if (!alive(d0 + 1.5f + 0.5f * i)) continue;
                const float px = lerpf(x0, x1, n > 1 ? i / (float)(n - 1) : 0.5f);
                pen.begin(ID_DANCE + 80 + (uint32_t)i, -1e6f, 0.f, 0.1f);
                cheer(pen, V(px, 0.f), cs, i, beat);
            }
        }
        if (gone < 0.f && rel > 6.f && std::fmod(rel, 8.f) >= 4.f && std::fmod(rel, 8.f) < 6.f) {
            static const char* SHOUTS[5] = {"Yeah!", "Go!", "Woo!", "Wow!", "Yo!"};
            const int k = (int)(rel / 8.f);
            Rng r(mixk(danceSeed_, 900 + (uint64_t)k));
            pen.begin(ID_DANCE + 200 + (uint32_t)(k % 32), -1e6f, 0.f, 0.15f);
            pen.text(V(r.range(-0.5f, 0.5f) * hw[O], 2.9f * s + r.range(0.f, 1.2f) * s), SHOUTS[r.irange(0, 4)], Pl.red, 0.75f * s);
        }
        place(from, O);
    } else {
        const Ctx c{s, 0.f};
        // ---- the props
        for (const Prop& p : props_) {
            if (beat < p.t0 || beat >= p.t1) continue;
            const size_t from = out.size();
            drawProp(pen, p, hw[p.side], s);
            place(from, p.side);
        }
        // ---- the guy
        for (const Ep& e : eps_) {
            if (beat < e.b0 || beat >= e.b1) continue;
            const float t = (float)(beat - e.b0);
            Ctx cc = c;
            cc.top = H[e.side];
            const size_t from = out.size();
            pen.begin(ID_GUY + 1 + (e.id % 4096), -1e6f, 0.f, chalky * 0.03f);
            const Fig f = figure(e, t, cc, &pen);
            const Fig g = figure(e, std::max(0.f, t - 0.12f), cc, nullptr);
            const vec2 vel = g.side == f.side ? (jointAt(f, NECK, s) - jointAt(g, NECK, s)) * (1.f / 0.12f) : V(0.f, 0.f);
            place(from, e.side);
            const size_t from2 = out.size();
            pen.begin(ID_GUY, -1e6f, 0.f, chalky * 0.08f);
            drawGuy(pen, f, s, vel, fb * 7.f, 1.f);
            place(from2, f.side);
            break;
        }
    }
    // each thing's pose: its drawing (what doesn't move keeps its key)
    for (Elem& e : out) {
        uint64_t h = 1469598103934665603ull;
        auto add = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
        add(e.col);
        add((uint64_t)e.tool);
        add((uint64_t)std::lround(e.width * 1000.f));
        for (const vec2& q : e.pts) { add((uint64_t)(int64_t)std::lround(q.x * 200.f)); add((uint64_t)(int64_t)std::lround(q.y * 200.f)); }
        for (char ch : e.text) add((uint64_t)(unsigned char)ch);
        e.pose = (uint32_t)(h ^ (h >> 32));
    }
}

} // namespace notepad_story

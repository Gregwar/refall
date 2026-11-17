// NOTEPAD's storyboard: a little stick figure lives in the margins of the page all song long, in the notebook's
// tools, like a flip-book drawn in the corner of an exercise book. His adventures follow one another, each from
// where the last one left him:
//
//  - he walks (in a break) or runs (from the verse on) along the ground, a step on each beat, and turns back; or
//    skips, moonwalks, tiptoes;
//  - he hops from platform to platform up into the margin (each drawn in just before he needs it) and back down;
//  - a ladder is drawn, he climbs it rung by rung to a ledge;
//  - from a ledge he jumps off and his parachute opens, he sways down and lands; or he dives off with a somersault;
//  - something falls on him from the top of the page (an anvil, a piano, a flower pot, a safe): a "!", its shadow
//    growing, he leaps back just in time, it crashes where he stood;
//  - acrobatics on the bars: cartwheels, a backflip, a handstand and a walkover, a star jump;
//  - gags: a skateboard (an ollie, a kickflip), a pogo stick, a bike (a wheelie) or a unicycle, a skipping rope, a
//    trampoline (flips), a football to a goal (or the post), a basketball (a shot, a dunk), a swarm of bees after
//    him, a banana peel, an umbrella in the rain (then a sun, a rainbow), a kite, fishing (a fish, a boot, a can),
//    juggling (balls, clubs, rings), a guitar or the drums, yoga, a telescope and a shooting star, painting at an
//    easel, a dog fetching a stick;
//  - he crosses to the other margin: through a hole drawn on the ground (out of another one over there), out of a
//    cannon, on a paper plane, under balloons, with a rocket pack (off out of sight, in from the top over there);
//  - in a break he sits down and taps his foot, reads, or lies down and sleeps (Zzz); a wave now and then.
//
// They are dealt like a deck: what he has done least this song comes first (the section choosing among it), each
// one's variants the same way, so that a song shows most of them before any comes back.
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
#include <cstdio>
#include <cstdlib>

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
enum {
    K_WAVE, K_RUN, K_HOP, K_LADDER, K_CHUTE, K_DIVE, K_DODGE, K_ACRO, K_CROSS, K_REST,
    K_SKATE, K_POGO, K_BIKE, K_ROPE, K_TRAMP, K_FOOTBALL, K_BASKET, K_BEES, K_BANANA, K_UMBRELLA,
    K_KITE, K_FISH, K_JUGGLE, K_MUSIC, K_YOGA, K_STARS, K_PAINT, K_DOG, K_COUNT
};
const char* const KIND_NAMES[K_COUNT] = {"wave", "run", "hop", "ladder", "chute", "dive", "dodge", "acro", "cross", "rest",
                                         "skate", "pogo", "bike", "rope", "trampoline", "football", "basket", "bees", "banana", "umbrella",
                                         "kite", "fish", "juggle", "music", "yoga", "stars", "paint", "dog"};
// each one's variants (what the log calls them; the scheduler takes the least seen)
const char* const VARIANTS[K_COUNT][6] = {
    {"hello"}, {"run", "skip", "moonwalk", "sneak"}, {"up"}, {"climb"}, {"red", "blue"}, {"somersault"},
    {"anvil", "piano", "pot", "safe"}, {"tricks"}, {"hole", "cannon", "plane", "balloons", "rocket"}, {"sit", "sleep", "read"},
    {"ollie", "kickflip"}, {"bounce", "early"}, {"bike", "wheelie", "unicycle"}, {"steady", "double"}, {"star", "backflip"},
    {"goal", "post"}, {"shot", "dunk"}, {"swarm"}, {"slip"}, {"sun", "rainbow"},
    {"diamond"}, {"fish", "boot", "can"}, {"balls", "clubs", "rings"}, {"guitar", "drums"}, {"tree", "warrior"},
    {"moon", "planet"}, {"house", "flower", "heart", "self", "boat", "cat"}, {"brown", "grey", "ginger"}};
const int NVAR[K_COUNT] = {1, 4, 1, 1, 2, 1, 4, 1, 5, 3, 2, 2, 3, 2, 2, 2, 2, 1, 1, 2, 1, 3, 3, 2, 2, 2, 6, 3};
enum { CR_HOLE, CR_CANNON, CR_PLANE, CR_BALLOONS, CR_ROCKET };
enum { RU_RUN, RU_SKIP, RU_MOONWALK, RU_SNEAK };
enum { TR_CARTWHEEL, TR_BACKFLIP, TR_HANDSTAND, TR_STARJUMP, TR_COUNT };
const float TRICK_MOVE[TR_COUNT] = {3.0f, -0.6f, 0.f, 0.f}; // how far a trick takes him (his units, forward)
enum { OB_ANVIL, OB_PIANO, OB_POT, OB_SAFE, OB_COUNT };
enum { PR_GROUND, PR_LEDGE, PR_LADDER, PR_HOLE, PR_TRAMP, PR_GOAL, PR_HOOP, PR_POND, PR_PEEL, PR_CLOUD, PR_SKY, PR_SCOPE, PR_EASEL,
       PR_PICTURE, PR_CANNON };
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
    float k = 1.f;      // his size (flying off out of sight, small, far away)
};
vec2 jointAt(const Fig& f, int j, float s) {
    s *= f.k;
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
    float s;      // the guy's size
    float hw[2];  // the margins' half widths
    float top[2]; // their heights (where things fall from)
};

float trickX(const Story::Ep& e, int upto, float s) {
    float x = e.p0.x;
    for (int i = 0; i < upto && i < (int)e.seq.size(); i++) x += e.dir * TRICK_MOVE[e.seq[i]] * s;
    return x;
}

// ---------------------------------------------------------------- the gags
// (each gag: where the guy is at `t` beats in and how he stands, and what moves with him, drawn with `pen`)

Skel posed(const Ang& a) { Skel S = fk(a); onFeet(S); return S; }
float jumpY(float u, float h) { return h * 4.f * u * (1.f - u); } // a jump's height, `u` from take-off to landing
uint32_t hue(const Pal& P, int i) {
    const uint32_t c[6] = {P.red, P.blue, P.green, P.orange, P.purple, P.yellow};
    return c[((i % 6) + 6) % 6];
}
int wellSide(int side) { return side == 0 ? 1 : -1; } // which way the well is from a margin
// A thing in the guy's frame (his units, x forward, from `at`, turned by `ang`) out in the margin.
vec2 his(vec2 q, vec2 at, float s, int dir, float ang = 0.f) {
    q = rot(q, ang);
    return at + V(q.x * s * (float)dir, q.y * s);
}
Poly his(Poly p, vec2 at, float s, int dir, float ang = 0.f) {
    for (vec2& q : p) q = his(q, at, s, dir, ang);
    return p;
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

Poly quad(vec2 a, vec2 c, vec2 b, int n = 10) { // a curve from a to b pulled toward c
    Poly p;
    for (int i = 0; i <= n; i++) {
        const float u = i / (float)n;
        p.push_back(a * ((1 - u) * (1 - u)) + c * (2 * u * (1 - u)) + b * (u * u));
    }
    return p;
}
vec2 bez(vec2 a, vec2 b, vec2 c, vec2 d, float u) {
    const float v = 1.f - u;
    return a * (v * v * v) + b * (3 * v * v * u) + c * (3 * v * u * u) + d * (u * u * u);
}
// A limb reaching for `to` (his units) from its root: two segments, the middle joint bent to `bend` (+1 forward,
// knees; -1 back, elbows).
void reach(Skel& S, int root, int mid, int end, vec2 to, float bend, float l1, float l2) {
    const vec2 a = S.j[root], u = unit(to - a);
    const float L = std::clamp(len(to - a), std::fabs(l1 - l2) + 1e-3f, l1 + l2 - 1e-4f);
    const float x = (l1 * l1 - l2 * l2 + L * L) / (2.f * L), y = std::sqrt(std::max(0.f, l1 * l1 - x * x));
    S.j[mid] = a + u * x + V(-u.y, u.x) * (y * bend);
    S.j[end] = a + u * L;
}
void feetAt(Skel& S, vec2 f0, vec2 f1) {
    reach(S, HIP, KN0, FT0, f0, 1.f, THIGH, SHIN);
    reach(S, HIP, KN1, FT1, f1, 1.f, THIGH, SHIN);
}
void handsAt(Skel& S, vec2 h0, vec2 h1) {
    reach(S, NECK, EL0, HA0, h0, -1.f, UARM, FARM);
    reach(S, NECK, EL1, HA1, h1, -1.f, UARM, FARM);
}
Ang hipAt(Ang a, float y) { a.hip = V(0.f, y); return a; }
// a little star (a sparkle, what he sees after a fall), a puff of smoke, a ball
void starShape(Pen& pen, vec2 c, float r, uint32_t col, float a0 = 0.f) {
    Poly p;
    for (int i = 0; i <= 10; i++) {
        const float a = a0 + PI * 0.5f + i * PI / 5.f, rr = (i & 1) ? r * 0.45f : r;
        p.push_back(c + V(std::cos(a), std::sin(a)) * rr);
    }
    pen.line(p, 0.7f, col);
}
void sparkle(Pen& pen, vec2 c, float r, uint32_t col) {
    pen.line({c + V(-r, 0.f), c + V(r, 0.f)}, 0.7f, col);
    pen.line({c + V(0.f, -r), c + V(0.f, r)}, 0.7f, col);
    pen.line({c + V(-0.5f * r, -0.5f * r), c + V(0.5f * r, 0.5f * r)}, 0.5f, col);
    pen.line({c + V(-0.5f * r, 0.5f * r), c + V(0.5f * r, -0.5f * r)}, 0.5f, col);
}
void smoke(Pen& pen, vec2 c, float r, float a = 0.8f) {
    Poly p;
    for (int i = 0; i <= 24; i++) {
        const float u = i / 24.f * TAU;
        p.push_back(c + V(std::cos(u), std::sin(u)) * (r * (1.f + 0.18f * std::cos(u * 5.f))));
    }
    pen.line(p, 0.7f, pen.P.grey, a);
}
void ballShape(Pen& pen, vec2 c, float r, uint32_t col, float spin = 0.f) {
    const Poly b = ellipse(c, r, r);
    pen.shape(b, 0.7f);
    pen.colour(b, col, 0.5f + spin, std::max(0.25f, r * 1.6f));
}
// the curve of a string from a to b, sagging
Poly sag(vec2 a, vec2 b, float d) { return quad(a, (a + b) * 0.5f + V(0.f, -d), b, 10); }

// ---- on wheels and springs
Ang balance() { // arms out
    Ang a;
    a.spine = 0.1f;
    a.ua[0] = 1.35f; a.fa[0] = 1.7f;
    a.ua[1] = -1.25f; a.fa[1] = -1.55f;
    return a;
}
void skateboard(Pen& pen, vec2 at, float s, int dir, float tilt, float flip, uint32_t col) {
    const float wy = 0.16f - 0.09f * std::cos(flip);
    const Poly deck = his({V(-0.64f, 0.26f), V(-0.5f, 0.16f), V(0.5f, 0.16f), V(0.64f, 0.26f)}, at, s, dir, tilt);
    pen.line(deck, 2.4f, col, 0.9f);
    pen.line(deck, 0.8f);
    for (int k = -1; k <= 1; k += 2) pen.shape(ellipse(his(V(0.36f * k, wy), at, s, dir, tilt), 0.07f * s, 0.07f * s), 0.7f);
}
void epSkate(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const bool kickflip = e.seq[0] == 1;
    const float u = saturate((t - 1.f) / (dur - 2.f));
    const float prog = u < 0.3f ? 0.18f * (u / 0.3f) * (u / 0.3f) : 0.18f + 0.82f * (u - 0.3f) / 0.7f;
    const float end = e.p1.x - e.dir * 0.75f * s, bx = lerpf(e.p0.x, end, prog);
    float air = 0.f, tilt = 0.f, flip = 0.f;
    const float tt = std::floor(dur * 0.5f); // the trick, on the bar's middle
    if (t >= tt && t < tt + 1.f) {
        const float v = t - tt;
        air = jumpY(v, 1.f);
        if (kickflip) flip = TAU * ease(v * 1.3f);
        tilt = (kickflip ? 0.15f : 0.45f) * std::sin(v * TAU) * (1.f - 0.5f * v);
    }
    const float deck = 0.16f;
    Ang a = balance();
    if (t < 1.f) { // stepping on
        f.at = V(bx, 0.f);
        Skel on = fk(hipAt(a, deck + 0.74f));
        feetAt(on, V(0.3f, deck), V(-0.3f, deck));
        f.S = mixS(standing(), on, ease(t));
    } else if (t < dur - 1.f) {
        f.at = V(bx, air * s);
        if (t < 3.f) { // two kicks along the ground
            const float ph = frac(t - 1.f);
            const vec2 kick = ph < 0.6f ? V(lerpf(0.05f, -0.5f, ph / 0.6f), 0.01f)
                                        : V(lerpf(-0.5f, 0.05f, (ph - 0.6f) / 0.4f), 0.2f * std::sin((ph - 0.6f) / 0.4f * PI));
            a.spine = 0.25f;
            a.ua[0] = 0.6f - 0.5f * std::sin(ph * TAU); a.fa[0] = a.ua[0] + 0.6f;
            f.S = fk(hipAt(a, deck + 0.62f));
            feetAt(f.S, V(0.18f, deck), kick);
        } else {
            const float k = air > 0.f ? std::sin((t - tt) * PI) : 0.f;
            if (k > 0.f) { a.ua[0] = 2.2f; a.fa[0] = 2.5f; a.ua[1] = -1.9f; a.fa[1] = -1.5f; }
            a.ua[0] += 0.12f * std::sin(t * PI);
            f.S = fk(hipAt(a, deck + 0.72f - 0.25f * k));
            feetAt(f.S, rot(V(0.3f, deck + 0.02f * k), tilt), rot(V(-0.3f, deck + 0.04f * k), tilt));
        }
    } else { // off, the board left behind
        const float v = t - (dur - 1.f);
        f.at = V(lerpf(end, e.p1.x, ease(v)), 0.f);
        Skel on = fk(hipAt(a, deck + 0.72f));
        feetAt(on, V(0.3f, deck), V(-0.3f, deck));
        f.S = v < 0.5f ? mixS(on, standing(), ease(v * 2.f)) : mixS(standing(), posed(tada()), ease((v - 0.5f) * 3.f));
    }
    if (pen) skateboard(*pen, V(bx, air * s), s, e.dir, tilt, flip, hue(pen->P, e.seq[1]));
    if (pen && air > 0.3f) pen->text(f.at + V(-e.dir * 0.9f * s, 2.4f * s), kickflip ? "Flip!" : "Ollie!", pen->P.red, 0.5f * s);
}

void epPogo(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    // a hop a beat (a big one each bar), moving along, then off it, the stick falling over
    const int i = (int)std::floor(t);
    const float v = frac(t);
    const bool big = i > 0 && (i % 4) == (e.seq[0] == 1 ? 1 : 3) && i < (int)dur - 1;
    const float end = e.p1.x - e.dir * 0.8f * s, x = lerpf(e.p0.x, end, saturate(t / (dur - 1.f)));
    const float cmp = 1.f - saturate(std::min(v, 1.f - v) / 0.18f); // the spring squashed
    const float pegs = 0.45f - 0.15f * cmp, bar = pegs + 0.95f;
    const float lean = -0.1f;
    const uint32_t col = pen ? hue(pen->P, e.seq[1]) : 0;
    if (t < dur - 1.f) {
        f.at = V(x, jumpY(v, big ? 1.9f : 0.6f) * s);
        Ang a;
        a.spine = 0.05f;
        f.S = fk(hipAt(a, pegs + 0.74f - 0.12f * cmp));
        feetAt(f.S, V(0.1f, pegs), V(-0.05f, pegs));
        handsAt(f.S, V(0.22f, bar), V(0.14f, bar - 0.02f));
        if (big && v > 0.3f && v < 0.7f) { // a hand off at the top
            reach(f.S, NECK, EL1, HA1, f.S.j[NECK] + V(-0.3f, 0.45f), -1.f, UARM, FARM);
        }
        turnS(f.S, V(0.f, 0.f), lean);
        if (pen) {
            auto X = [&](Poly p) { return his(std::move(p), f.at, s, e.dir, lean); };
            Poly spring;
            for (int k = 0; k <= 8; k++) spring.push_back(V((k & 1) ? 0.07f : -0.07f, 0.08f + (pegs - 0.12f) * k / 8.f));
            pen->line(X({V(0.f, 0.f), V(0.f, 0.1f)}), 0.9f);
            pen->line(X(spring), 0.6f);
            pen->line(X({V(0.f, pegs - 0.06f), V(0.f, bar)}), 1.6f, col);
            pen->line(X({V(-0.22f, bar), V(0.22f, bar)}), 1.1f);
            pen->line(X({V(-0.14f, pegs), V(0.14f, pegs)}), 0.9f);
            if (big && v > 0.15f && v < 0.6f) pen->text(f.at + V(e.dir * 1.0f * s, 1.f * s), "Boing!", pen->P.red, 0.5f * s);
        }
    } else {
        const float w = t - (dur - 1.f);
        f.at = V(lerpf(end, e.p1.x, w), jumpY(w, 0.6f) * s);
        f.S = w < 0.6f ? posed(leap()) : mixS(posed(leap()), posed(tada()), ease((w - 0.6f) / 0.3f));
        if (pen) { // the stick tipping over behind him
            const float fall = -PI * 0.5f * ease(w * 1.5f) - 0.1f;
            auto X = [&](Poly p) { return his(std::move(p), V(end, 0.f), s, -e.dir, -fall); };
            pen->line(X({V(0.f, 0.f), V(0.f, 1.4f)}), 1.6f, col);
            pen->line(X({V(-0.22f, 1.4f), V(0.22f, 1.4f)}), 1.1f);
        }
    }
}

void epBike(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const int kind = e.seq[0]; // 0 a bike, 1 a bike and a wheelie, 2 a unicycle
    const bool uni = kind == 2;
    const float prog = ease(saturate(t / dur) * 1.1f - 0.05f);
    f.at = V(lerpf(e.p0.x, e.p1.x, prog), 0.f);
    const float r = uni ? 0.42f : 0.34f;
    const float roll = -std::fabs(e.p1.x - e.p0.x) * prog / s / r;
    const vec2 crankC = uni ? V(0.f, r) : V(0.f, 0.3f);
    const float crank = uni ? roll : -t * PI, cr = uni ? 0.15f : 0.16f;
    const vec2 ped0 = crankC + V(std::cos(crank), std::sin(crank)) * cr, ped1 = crankC - V(std::cos(crank), std::sin(crank)) * cr;
    Ang a;
    if (uni) { // arms out, wobbling for balance
        a.ua[0] = 1.5f + 0.35f * std::sin(t * TAU); a.fa[0] = a.ua[0] + 0.4f;
        a.ua[1] = -1.5f + 0.35f * std::sin(t * TAU + 1.3f); a.fa[1] = a.ua[1] - 0.4f;
        a.spine = 0.04f;
    } else a.spine = 0.45f;
    const vec2 seat = uni ? V(0.f, 1.06f) : V(-0.2f, 0.98f);
    a.hip = seat + V(0.f, 0.03f);
    Skel S = fk(a);
    feetAt(S, ped0, ped1);
    const vec2 bar = V(0.43f, 1.12f);
    if (!uni) handsAt(S, bar, bar + V(-0.04f, 0.f));
    // the whole thing tilts: a wheelie (about the back wheel), a unicycle's sway
    vec2 piv = V(uni ? 0.f : -0.62f, 0.f);
    float ang = 0.f;
    if (uni) ang = 0.08f * std::sin(t * PI);
    else if (kind == 1) ang = 0.42f * ease(saturate(std::min(t - 2.f, dur - 2.f - t) / 0.8f));
    turnS(S, piv, ang);
    f.S = S;
    if (!pen) return;
    const Pal& P = pen->P;
    const uint32_t col = hue(P, e.seq[1]);
    auto X = [&](Poly p) {
        for (vec2& q : p) q = his(piv + rot(q - piv, ang), f.at, s, e.dir);
        return p;
    };
    auto wheel = [&](vec2 hub) {
        pen->shape(X(ellipse(hub, r, r)), 0.9f);
        for (int k = 0; k < 2; k++) {
            const vec2 d = V(std::cos(roll + k * PI * 0.5f), std::sin(roll + k * PI * 0.5f)) * (r * 0.9f);
            pen->line(X({hub - d, hub + d}), 0.4f, P.grey);
        }
    };
    if (uni) {
        wheel(V(0.f, r));
        pen->line(X({V(0.f, r), seat}), 1.4f, col);
        pen->line(X({seat + V(-0.14f, 0.f), seat + V(0.14f, 0.02f)}), 1.3f);
    } else {
        const vec2 R = V(-0.62f, r), F = V(0.62f, r), hd = V(0.45f, 0.86f);
        wheel(R);
        wheel(F);
        pen->line(X({R, crankC, seat, R}), 1.3f, col);
        pen->line(X({seat, hd, crankC}), 1.3f, col);
        pen->line(X({F, hd, bar, bar + V(-0.12f, 0.06f)}), 0.9f);
        pen->line(X({seat + V(-0.13f, 0.02f), seat + V(0.1f, 0.02f)}), 1.3f);
        pen->shape(X(ellipse(crankC, 0.07f, 0.07f)), 0.6f);
    }
    pen->line(X({ped0, crankC, ped1}), 0.7f);
    if (kind == 1 && ang > 0.3f) pen->text(f.at + V(-e.dir * 0.4f * s, 2.9f * s), "Wheelie!", P.red, 0.5f * s);
}

// ---- in place, to the beat
void epRope(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    // a hop each beat, the rope under him on the off-beat; once a big hop, the rope round twice
    const bool big = e.seq[0] == 1 && t >= 4.f && t < 6.f;
    float hop = 0.f, psi = TAU * t;
    if (big) { hop = jumpY((t - 4.f) / 2.f, 1.3f); psi = TAU * (4.f + 2.f * (t - 4.f)); }
    else if (t >= 1.f && t < dur - 1.f) hop = jumpY(frac(t), 0.45f);
    Ang a;
    a.ua[0] = 0.55f; a.fa[0] = 1.25f;
    a.ua[1] = -0.55f; a.fa[1] = -1.25f;
    const float air = saturate(hop / 0.2f);
    a.th[0] = 0.1f * air; a.sh[0] = a.th[0] - 0.5f * air;
    a.th[1] = -0.1f * air; a.sh[1] = a.th[1] - 0.5f * air;
    if (big) { a.th[0] = a.th[1] = 1.2f * air; a.sh[0] = a.sh[1] = -0.3f * air; }
    f.S = posed(a);
    if (t >= dur - 1.f) { f.S = mixS(f.S, posed(tada()), ease((t - dur + 1.f) * 2.f)); psi = PI; }
    f.at = V(e.p0.x, hop * s);
    if (!pen) return;
    const vec2 h0 = jointAt(f, HA0, s), h1 = jointAt(f, HA1, s);
    const vec2 m = V((h0.x + h1.x) * 0.5f, (h0.y + h1.y) * 0.5f + 1.05f * s * std::cos(psi));
    pen->line(quad(h0, m * 2.f - (h0 + h1) * 0.5f, h1, 12), 1.f, hue(pen->P, e.seq[1]));
    if (big && t < 5.5f) pen->text(f.at + V(0.f, 3.f * s), "x2!", pen->P.red, 0.55f * s);
}

void epTramp(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s;
    const float cx = e.ea, mat = 0.55f * s, edge = cx - e.dir * 1.35f * s, off = cx + e.dir * 1.7f * s;
    // on, bounces (1, 1.6, a flip, a trick), a small one, off
    struct B { float t0, t1, h; };
    const B bs[5] = {{2, 3, 1.f}, {3, 4, 1.7f}, {4, 6, 2.8f}, {6, 8, 2.8f}, {8, 9, 1.2f}};
    Skel st = standing(), cr = posed(crouch());
    float dip = 0.f;
    if (t < 1.f) {
        f.at = V(lerpf(e.p0.x, edge, t), 0.f);
        f.S = posed(gait(t * TAU, 0.6f));
    } else if (t < 2.f) {
        const float u = t - 1.f;
        f.at = V(lerpf(edge, cx, u), lerpf(0.f, mat, u) + jumpY(u, 1.f * s));
        f.S = posed(leap());
        if (u > 0.85f) dip = (u - 0.85f) / 0.15f;
    } else if (t < 9.f) {
        int i = 0;
        while (i < 4 && t >= bs[i].t1) i++;
        const float u = (t - bs[i].t0) / (bs[i].t1 - bs[i].t0);
        const float h = jumpY(u, bs[i].h) * s;
        f.at = V(cx, mat + h);
        const float nearMat = 1.f - saturate(h / (0.3f * s));
        dip = nearMat;
        f.S = mixS(standing(), cr, nearMat);
        if (i == 2) { // a front flip
            f.S = mixS(st, fk(tuck()), ease(std::min(u, 1.f - u) * 4.f));
            shiftS(f.S, V(0.f, 0.86f - f.S.j[HIP].y));
            turnS(f.S, f.S.j[HIP], -TAU * ease((u - 0.1f) / 0.8f));
        } else if (i == 3) {
            if (e.seq[0] == 0) { // a star at the top
                Skel sr = posed(star());
                f.S = mixS(f.S, sr, ease(std::min(u, 1.f - u) * 4.f));
            } else { // a backflip
                f.S = mixS(st, fk(tuck()), ease(std::min(u, 1.f - u) * 4.f));
                shiftS(f.S, V(0.f, 0.86f - f.S.j[HIP].y));
                turnS(f.S, f.S.j[HIP], TAU * ease((u - 0.1f) / 0.8f));
            }
        }
    } else if (t < 10.5f) {
        const float u = (t - 9.f) / 1.5f;
        f.at = V(lerpf(cx, off, u), lerpf(mat, 0.f, u) + jumpY(u, 1.6f * s));
        f.S = mixS(posed(leap()), cr, saturate((u - 0.8f) / 0.2f));
        if (u < 0.12f) dip = 1.f - u / 0.12f;
    } else {
        f.at = V(off, 0.f);
        f.S = mixS(cr, posed(tada()), ease((t - 10.5f) / 0.4f));
    }
    if (pen) {
        const float hw = 1.15f * s;
        const float dx = std::clamp(f.at.x - cx, -hw * 0.8f, hw * 0.8f);
        pen->line(quad(V(cx - hw, mat), V(cx + dx, mat - 0.7f * s * dip), V(cx + hw, mat), 8), 1.1f);
        if (t >= 4.2f && t < 5.4f) pen->text(V(cx, mat + 4.f * s), "Whee!", pen->P.red, 0.55f * s);
    }
}

// ---- ball games
void football(Pen& pen, vec2 c, float r, float spin) {
    pen.shape(ellipse(c, r, r), 0.8f);
    Poly patch;
    for (int i = 0; i <= 5; i++) patch.push_back(c + rot(V(0.f, 0.42f * r), spin + i * TAU / 5.f));
    pen.shape(patch, 0.5f);
    pen.dark(patch, 0.3f, 0.3f);
}
void epFootball(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, d = (float)e.dir;
    const bool goal = e.seq[0] == 0;
    const float gx = e.ea, ks = e.eb, r = 0.17f * s;
    const float bx0 = ks + d * 0.45f * s; // where the ball waits for the kick
    const vec2 target = goal ? V(gx + d * 0.25f * s, 0.6f * s) : V(gx, 1.25f * s);
    vec2 ball;
    float spin = 0.f;
    if (t < 4.f) { // dribbling up, a touch on each beat
        f.at = V(lerpf(e.p0.x, ks, t / 4.f), 0.f);
        f.S = posed(gait(t * TAU, 0.75f));
        const float lead = (0.35f + 0.35f * (1.f - frac(t))) * std::min(1.f, 4.f - t);
        ball = V(f.at.x + d * (lead * s + 0.1f * s), r);
        if (t > 3.6f) ball.x = lerpf(ball.x, bx0, (t - 3.6f) / 0.4f);
    } else if (t < 5.f) { // a step back, a run-up, the kick
        ball = V(bx0, r);
        if (t < 4.5f) {
            f.at = V(lerpf(ks, ks - d * 0.6f * s, ease((t - 4.f) * 2.f)), 0.f);
            f.S = standing();
        } else {
            const float u = (t - 4.5f) * 2.f;
            f.at = V(lerpf(ks - d * 0.6f * s, bx0 - d * 0.5f * s, u), 0.f);
            f.S = posed(gait(u * TAU, 0.9f));
        }
    } else {
        f.at = V(bx0 - d * 0.5f * s, 0.f);
        if (t < 5.6f) { // the kick: the leg through, the arms out
            Ang a;
            const float k = ease((t - 5.f) / 0.3f);
            a.spine = -0.25f * k;
            a.th[1] = lerpf(-0.5f, 1.4f, k); a.sh[1] = a.th[1] - 0.3f * (1.f - k);
            a.th[0] = -0.1f; a.sh[0] = -0.1f;
            a.ua[0] = -1.2f; a.fa[0] = -0.9f;
            a.ua[1] = 1.4f; a.fa[1] = 1.7f;
            f.S = posed(a);
        } else if (goal) { // jumping for joy on the beats
            f.at.y = t > 6.f && t < 9.f ? jumpY(frac(t), 0.6f) * s : 0.f;
            f.S = posed(t > 6.f ? tada() : Ang{});
        } else { // his head in his hands
            Ang a;
            a.head = 0.3f;
            a.ua[0] = 2.6f; a.fa[0] = -2.3f;
            a.ua[1] = -2.6f; a.fa[1] = 2.3f;
            f.S = mixS(standing(), posed(a), ease((t - 6.f) * 2.f));
        }
        const float u = saturate((t - 5.15f) / 0.85f);
        ball = mixv(V(bx0, r), target, u) + V(0.f, jumpY(u, goal ? 1.4f * s : 1.1f * s));
        spin = -u * 9.f * d;
        if (u >= 1.f) {
            if (goal) ball = V(gx + d * 0.32f * s, r);
            else { // off the post, back to his feet
                const float w = saturate((t - 6.f) / 1.2f);
                ball = mixv(target, V(f.at.x + d * 0.4f * s, r), w) + V(0.f, jumpY(w, 0.8f * s));
                spin = w * 7.f * d;
            }
        }
    }
    if (!pen) return;
    football(*pen, ball, r, spin + ball.x * (-d) / r * 0.5f);
    if (goal && t >= 6.f) {
        pen->line(quad(V(gx + d * 0.1f * s, 1.15f * s), V(gx + d * (0.95f - 0.25f * saturate(t - 6.5f)) * s, 0.6f * s), V(gx + d * 0.1f * s, 0.02f * s), 8), 0.5f, pen->P.grey);
        if (t < 8.5f) pen->text(V(gx - d * 1.3f * s, 3.2f * s), "GOAL!", pen->P.red, 0.8f * s);
    }
    if (!goal && t >= 5.9f && t < 7.f) pen->text(target + V(-d * 0.4f * s, 0.9f * s), "Doink!", pen->P.red, 0.55f * s);
}

void epBasket(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, d = (float)e.dir;
    const bool dunk = e.seq[0] == 1;
    const float hx = e.ea, r = 0.19f * s;
    const vec2 rim = V(hx - d * 0.55f * s, 2.6f * s);
    const float spot = dunk ? hx - d * 1.5f * s : e.eb;
    const float tDrib = dunk ? 3.5f : 4.f;
    vec2 ball;
    if (t < tDrib) { // dribbling, a bounce a beat
        f.at = V(lerpf(e.p0.x, spot, t / tDrib), 0.f);
        f.S = posed(gait(t * TAU, dunk ? 0.85f : 0.55f));
        const float by = 0.15f + 0.75f * std::fabs(std::cos(t * PI));
        ball = f.at + V(d * 0.45f * s, by * s);
        reach(f.S, NECK, EL0, HA0, V(0.45f, std::max(0.75f, by + 0.15f)), -1.f, UARM, FARM);
    } else if (!dunk) {
        f.at = V(spot, 0.f);
        Ang up = armsUp();
        up.ua[0] = up.ua[1] = 2.7f; up.fa[0] = up.fa[1] = 2.9f;
        if (t < 5.f) {
            f.S = mixS(standing(), posed(crouch()), ease(t - 4.f));
            ball = f.at + V(d * 0.3f * s, 1.f * s);
        } else if (t < 6.f) {
            const float u = t - 5.f;
            f.at.y = jumpY(u, 0.7f * s);
            f.S = mixS(posed(crouch()), posed(up), ease(u * 3.f));
        } else if (t < 7.5f) {
            f.S = mixS(posed(up), standing(), ease(t - 6.f));
        } else { // a fist pumped
            Ang a;
            a.ua[0] = 2.3f + 0.25f * std::sin(t * TAU); a.fa[0] = 3.1f;
            f.S = posed(a);
        }
        const vec2 from = f.at + V(d * 0.15f * s, 2.25f * s);
        if (t >= 5.3f && t < 6.4f) {
            const float u = (t - 5.3f) / 1.1f;
            ball = mixv(V(spot + d * 0.15f * s, 2.4f * s), rim + V(0.f, 0.2f * s), u) + V(0.f, jumpY(u, 1.6f * s));
        } else if (t >= 5.f && t < 5.3f) ball = from;
        else if (t >= 6.4f) {
            const float w = t - 6.4f;
            const float y = w < 0.5f ? lerpf(rim.y + 0.2f * s, r, ease(w * 2.f)) : r + jumpY(saturate((w - 0.5f) / 0.8f), 0.9f * s);
            ball = V(rim.x - d * 0.4f * s * saturate(w - 0.5f), y);
        }
    } else { // a run, a leap, the dunk, hanging on the rim
        const vec2 grip = V(rim.x - d * 0.25f * s, rim.y - 2.05f * s);
        if (t < 4.f) {
            f.at = V(spot, 0.f);
            f.S = posed(crouch());
            ball = f.at + V(d * 0.3f * s, 0.9f * s);
        } else if (t < 5.f) {
            const float u = t - 4.f;
            f.at = mixv(V(spot, 0.f), grip, u) + V(0.f, jumpY(u, 0.9f * s));
            f.S = posed(armsUp());
            ball = jointAt(f, HA0, s) + V(0.f, 0.15f * s);
        } else if (t < 6.6f) {
            f.at = grip;
            f.S = fk(hang(t));
            shiftS(f.S, V(0.f, -std::min(f.S.j[FT0].y, f.S.j[FT1].y)));
            turnS(f.S, V(0.f, 2.05f), 0.15f * std::sin((t - 5.f) * PI));
            ball = rim + V(0.f, lerpf(0.f, -2.3f * s, saturate((t - 5.f) / 0.5f)));
            if (t > 5.5f) ball = V(rim.x - d * 0.6f * s * (t - 5.5f), r + jumpY(saturate((t - 5.5f) / 0.8f), 0.8f * s));
        } else {
            const float u = saturate((t - 6.6f) / 0.4f);
            f.at = V(grip.x, grip.y * (1.f - u * u));
            f.S = u < 1.f ? posed(armsUp()) : mixS(posed(crouch()), posed(tada()), ease((t - 7.f) / 0.5f));
            ball = V(rim.x - d * 0.6f * s * 1.1f - d * 0.3f * s * saturate(t - 6.6f), r);
        }
    }
    if (!pen) return;
    const Poly b = ellipse(ball, r, r);
    pen->shape(b, 0.8f);
    pen->colour(b, pen->P.orange, 0.4f, 0.3f);
    pen->line({ball + V(-r, 0.f), ball + V(r, 0.f)}, 0.4f);
    pen->line(arc(ball + V(-r * 1.3f, 0.f), r, r * 1.05f, -0.9f, 0.9f, 6), 0.4f);
    const bool scored = dunk ? (t >= 5.1f && t < 6.6f) : (t >= 6.2f && t < 7.6f);
    if (scored) pen->text(rim + V(-d * 1.4f * s, 0.9f * s), dunk ? "Slam!" : "Swish!", pen->P.red, 0.6f * s);
}

// ---- trouble
void epBees(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s;
    const float A = e.p0.x, B = e.ea, C = e.p1.x;
    auto path = [&](float tau) {
        if (tau < 1.6f) return A;
        if (tau < 4.6f) return lerpf(A, B, (tau - 1.6f) / 3.f);
        return lerpf(B, C, saturate((tau - 4.6f) / 3.4f));
    };
    const float out = -(float)wellSide(e.side);
    const vec2 nest = V(out * (c.hw[e.side] - 0.6f * s), std::min(c.top[e.side] - 1.f * s, 6.5f * s));
    if (t < 1.6f) {
        f.at = V(A, 0.f);
        f.S = mixS(standing(), posed(lookUp()), ease(t - 0.6f));
        f.dir = nest.x > A ? 1 : -1;
    } else if (t < 8.f) {
        f.at = V(path(t), 0.f);
        f.dir = t < 4.6f ? (B > A ? 1 : -1) : (C > B ? 1 : -1);
        Ang a = gait(t * TAU * 1.2f, 1.f);
        a.ua[0] = 2.6f + 0.4f * std::sin(t * 20.f); a.fa[0] = a.ua[0] + 0.5f;
        a.ua[1] = 2.3f + 0.4f * std::sin(t * 20.f + 2.f); a.fa[1] = a.ua[1] + 0.5f;
        a.head = -0.3f;
        f.S = posed(a);
    } else { // ducking, hands over his head
        f.at = V(C, 0.f);
        Ang a = crouch();
        a.ua[0] = 2.3f; a.fa[0] = -2.6f;
        a.ua[1] = -2.2f; a.fa[1] = 2.7f;
        a.head = 0.4f;
        f.S = mixS(posed(crouch()), posed(a), ease((t - 8.f) * 3.f));
        if (t > 9.f) f.S = mixS(f.S, standing(), ease(t - 9.f));
    }
    if (!pen) return;
    const Pal& P = pen->P;
    const int n = 4 + e.seq[0];
    for (int k = 0; k < n; k++) {
        const float lag = 0.45f + 0.13f * k;
        const vec2 jit = V(0.3f * std::cos(t * 6.f + k * 2.3f), 0.25f * std::sin(t * 7.3f + k * 1.7f)) * s;
        const vec2 chase = V(path(std::max(1.6f, std::min(t, 8.f) - lag)), (1.9f + 0.25f * (k % 3)) * s) + jit;
        vec2 b;
        float k1 = 1.f;
        if (t < 2.f) b = mixv(nest + jit, chase, ease(saturate((t - 0.1f * k) / 1.6f)));
        else if (t < 8.f) b = chase;
        else { const float u = ease(saturate((t - 8.3f - 0.1f * k) / 1.5f)); b = mixv(chase, nest + jit, u); k1 = 1.f - 0.5f * u; }
        const float fl = ((int)std::floor(t * 8.f + k) & 1) ? 1.f : 0.6f;
        const Poly body = ellipse(b, 0.11f * s * k1, 0.075f * s * k1);
        pen->shape(body, 0.6f);
        pen->colour(body, P.yellow, 0.f, 0.25f);
        pen->line({b + V(0.f, -0.07f * s * k1), b + V(0.f, 0.07f * s * k1)}, 0.5f);
        pen->line(ellipse(b + V(-0.03f * s, 0.1f * s * fl) * k1, 0.05f * s * k1, 0.07f * s * fl * k1), 0.4f, 0, 0.7f);
    }
    if (t < 2.f) pen->text(nest + V(-out * 0.6f * s, 0.6f * s), "Bzz", P.ink, 0.5f * s);
    if (t >= 1.f && t < 1.8f) pen->text(f.at + V(0.f, 2.55f * s), "!", P.red, 0.9f * s);
    if (t >= 2.f && t < 4.f) pen->text(f.at + V(0.f, 2.9f * s), "Aaah!", P.red, 0.55f * s);
}

void epBanana(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, d = (float)e.dir;
    const float px = e.ea;
    const float slip = 3.f;
    if (t < slip) { // strolling along, whistling
        f.at = V(lerpf(e.p0.x, px - d * 0.25f * s, t / slip), 0.f);
        f.S = posed(gait(t * PI * 1.5f, 0.5f));
    } else if (t < 4.f) { // whoops: the feet out from under him, up in the air, down on his back
        const float u = t - slip;
        Ang a = leap();
        a.th[0] = a.th[1] = 1.6f; a.sh[0] = a.sh[1] = 1.4f;
        a.ua[0] = 2.6f; a.ua[1] = -2.4f;
        f.S = fk(a);
        turnS(f.S, f.S.j[HIP], 1.35f * ease(u * 1.5f));
        onGround(f.S);
        f.at = V(px + d * 0.3f * s * u, jumpY(u, 1.1f * s));
    } else if (t < 6.5f) { // flat on his back, seeing stars
        f.at = V(px + d * 0.3f * s, 0.f);
        f.S = sleeping(0.f);
    } else if (t < 7.3f) {
        f.at = V(px + d * 0.3f * s, 0.f);
        f.S = mixS(sleeping(0.f), posed(crouch()), ease((t - 6.5f) / 0.8f));
    } else {
        f.at = V(px + d * 0.3f * s, 0.f);
        Ang a; // a shrug
        a.ua[0] = 0.9f; a.fa[0] = 2.2f; a.ua[1] = -0.9f; a.fa[1] = -2.2f;
        a.head = 0.2f;
        f.S = mixS(posed(crouch()), posed(a), ease((t - 7.3f) / 0.4f));
    }
    if (!pen) return;
    const Pal& P = pen->P;
    if (t < slip) {
        const int k = (int)std::floor(t);
        const float a = frac(t);
        note(*pen, jointAt(f, HEAD, s) + V(d * (0.35f + 0.3f * a) * s, (0.2f + 0.6f * a) * s), 0.45f * s, P.blue, k & 1);
    } else if (t < 5.f) { // the peel flying off
        const float u = saturate((t - slip) / 1.3f);
        const vec2 at = V(px + d * 2.f * s * u, jumpY(u, 2.f * s));
        const float sp = u * 9.f;
        pen->line({at + rot(V(-0.25f, 0.f), sp) * s, at, at + rot(V(0.25f, 0.f), sp) * s}, 1.5f, P.yellow);
        if (t < 4.2f) pen->text(V(px, 2.9f * s), "Whoa!", P.red, 0.6f * s);
    }
    if (t >= 4.f && t < 6.8f) { // stars round his head
        const vec2 h = jointAt(f, HEAD, s) + V(0.f, 0.45f * s);
        for (int k = 0; k < 3; k++) {
            const float a = t * PI * 1.5f + k * TAU / 3.f;
            starShape(*pen, h + V(0.45f * s * std::cos(a), 0.12f * s * std::sin(a)), 0.12f * s, P.yellow);
        }
    }
}

void epUmbrella(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const float cloudY = e.h;
    const float tOpen = 2.f, tStop = dur - 3.f, tClose = dur - 2.f;
    const float open = t < tOpen ? 0.f : t < tClose ? ease((t - tOpen) * 2.5f) : 1.f - ease((t - tClose) * 2.f);
    f.at = e.p0;
    Ang a;
    a.ua[0] = lerpf(0.4f, 2.2f, ease(saturate(t - 1.5f))); a.fa[0] = a.ua[0] + 0.5f;
    if (t < tOpen) { a.head = -0.4f * ease(t); a.ua[0] = 0.2f; a.fa[0] = 0.4f; }
    if (t >= tOpen + 0.5f && t < tStop) { // a little dance under it: a step, a kick on the bar
        const int b = (int)std::floor(t);
        const float u = frac(t);
        a.th[0] = (b & 1) ? 0.35f * std::sin(u * PI) : 0.f;
        a.sh[0] = a.th[0] - ((b & 3) == 3 ? 0.f : 0.4f * std::sin(u * PI));
        a.th[1] = (b & 1) ? 0.f : -0.25f * std::sin(u * PI);
        a.sh[1] = a.th[1] - 0.6f * std::sin(u * PI) * ((b & 1) ? 0.f : 1.f);
        a.spine = 0.08f * std::sin(t * PI);
        f.at.x = e.p0.x + e.dir * 0.25f * s * std::sin(t * PI * 0.5f);
    }
    if (t >= tClose) { a = mixA(a, tada(), ease((t - tClose - 0.5f) * 2.f)); }
    f.S = posed(a);
    if (!pen) return;
    const Pal& P = pen->P;
    const uint32_t col = hue(P, e.seq[1]);
    const vec2 hand = jointAt(f, HA0, s);
    const vec2 tip = hand + V(e.dir * 0.05f * s, 0.85f * s);
    // the umbrella: its handle, its canopy opening
    pen->line({hand + V(-e.dir * 0.12f * s, -0.05f * s), hand + V(-e.dir * 0.1f * s, -0.15f * s), hand, tip + V(0.f, 0.15f * s)}, 0.8f);
    const float rx = lerpf(0.12f, 0.95f, open) * s, ry = lerpf(0.5f, 0.55f, open) * s;
    Poly can = arc(tip, rx, ry, 0.f, PI, 12);
    for (int k = 0; k < 4; k++) can.push_back(tip + V(-rx + rx * 0.5f * (k + 1), (open > 0.3f ? -0.08f : 0.f) * s * std::sin((k + 1) * PI * 0.5f)));
    pen->shape(can, 0.9f);
    pen->colour(can, col, 0.7f, 0.6f);
    // the rain, but not under the umbrella; then the sun (or a rainbow)
    if (t >= 0.8f && t < tStop) {
        const float shade0 = tip.x - rx, shade1 = tip.x + rx;
        for (int k = 0; k < 14; k++) {
            Rng r(mixk(e.id, (uint64_t)k));
            const float x = e.p0.x + r.range(-1.5f, 1.5f) * s;
            const float y = cloudY - 0.3f * s - frac(t * 1.3f + r.uniform()) * cloudY;
            const float stop = (open > 0.5f && x > shade0 && x < shade1) ? tip.y + 0.1f * s : 0.f;
            if (y < stop || y - 0.25f * s < 0.f) continue;
            pen->line({V(x, y), V(x - 0.04f * s, y - 0.25f * s)}, 0.5f, P.blue, 0.85f);
        }
        const Poly pud = ellipse(V(e.p0.x, 0.02f * s), 1.1f * s * saturate((t - 2.f) / 3.f) + 0.01f, 0.1f * s);
        if (t > 2.f) pen->colour(pud, P.blue, 0.f, 0.4f, 0.7f);
    }
    if (t >= tStop) {
        const float u = ease(saturate((t - tStop) / 0.6f));
        if (e.seq[0] == 0) {
            const vec2 sun = V(e.p0.x - e.dir * 1.2f * s, cloudY);
            pen->shape(ellipse(sun, 0.4f * s * u, 0.4f * s * u), 0.8f, P.orange);
            pen->colour(ellipse(sun, 0.4f * s * u, 0.4f * s * u), P.yellow, 0.3f, 0.35f);
            for (int k = 0; k < 8; k++) {
                const vec2 v = V(std::cos(k * TAU / 8.f + t), std::sin(k * TAU / 8.f + t));
                pen->line({sun + v * (0.55f * s * u), sun + v * (0.8f * s * u)}, 0.7f, P.orange);
            }
        } else {
            const uint32_t bands[4] = {P.red, P.orange, P.yellow, P.green};
            for (int k = 0; k < 4; k++)
                pen->line(arc(V(e.p0.x, 0.f), (2.6f - 0.18f * k) * s, (2.6f - 0.18f * k) * s * 1.3f, PI * (1.f - u), PI, 16), 1.6f, bands[k], 0.8f);
        }
    }
}

// ---- quiet things
void epKite(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    f.at = e.p0;
    Ang a;
    a.ua[0] = 2.05f; a.fa[0] = 2.35f; // the string held up
    a.ua[1] = -0.6f; a.fa[1] = 0.6f;  // a hand on his hip
    a.head = -0.35f;
    a.spine = -0.08f + 0.05f * std::sin(t * PI * 0.5f);
    const int stepB = (int)std::floor(t);
    a.th[0] = (stepB & 1) ? 0.1f : -0.05f; a.sh[0] = a.th[0] - 0.05f;
    f.S = mixS(standing(), posed(a), ease(t * 2.f));
    if (!pen) return;
    const Pal& P = pen->P;
    const vec2 hand = jointAt(f, HA0, s);
    const vec2 K = e.pts[0] + V(0.6f * s * std::sin(t * PI * 0.5f), 0.35f * s * std::sin(t * PI * 0.5f + 1.f));
    const float up = ease(saturate(t / 3.f)) * (1.f - ease(saturate((t - dur + 2.f) / 2.f)));
    const vec2 k = mixv(hand + V(0.f, 0.3f * s), K, up);
    const float tilt = 0.3f * std::sin(t * PI * 0.5f) * up - 0.2f * (float)e.dir;
    auto X = [&](Poly p) { return his(std::move(p), k, s, 1, tilt); };
    const Poly L = X({V(0.f, 0.45f), V(-0.32f, 0.1f), V(0.f, -0.45f), V(0.f, 0.45f)});
    const Poly R = X({V(0.f, 0.45f), V(0.32f, 0.1f), V(0.f, -0.45f), V(0.f, 0.45f)});
    pen->shape(X({V(0.f, 0.45f), V(0.32f, 0.1f), V(0.f, -0.45f), V(-0.32f, 0.1f), V(0.f, 0.45f)}), 0.9f);
    pen->colour(L, hue(P, e.seq[1]), 0.6f, 0.4f);
    pen->colour(R, hue(P, e.seq[1] + 2), -0.6f, 0.4f);
    pen->line(X({V(-0.32f, 0.1f), V(0.32f, 0.1f)}), 0.5f);
    const vec2 bottom = his(V(0.f, -0.45f), k, s, 1, tilt);
    Poly tail;
    for (int i = 0; i <= 8; i++) {
        const float u = i / 8.f;
        tail.push_back(bottom + V((-0.35f * (float)e.dir * u + 0.12f * std::sin(t * TAU * 0.5f + u * 5.f)) * s, -1.3f * s * u));
    }
    pen->line(tail, 0.6f);
    for (int i = 3; i <= 8; i += 3) {
        const vec2 b = tail[i];
        pen->shape({b, b + V(-0.12f * s, 0.07f * s), b + V(-0.12f * s, -0.07f * s), b, b + V(0.12f * s, 0.07f * s), b + V(0.12f * s, -0.07f * s), b}, 0.5f, hue(P, e.seq[1] + i));
    }
    pen->line(sag(hand, his(V(0.f, -0.1f), k, s, 1, tilt), 0.5f * s * up), 0.45f, 0, 0.8f, 0.3f);
    // wind
    for (int i = 0; i < 2; i++) {
        const float u = frac(t * 0.5f + i * 0.5f);
        const vec2 w = K + V((-1.5f + 2.6f * u) * s, (-1.3f - 0.8f * i) * s);
        pen->line({w, w + V(0.2f * s, 0.06f * s), w + V(0.4f * s, 0.f), w + V(0.6f * s, 0.06f * s)}, 0.5f, P.grey, 0.8f * (1.f - std::fabs(u * 2.f - 1.f)));
    }
}

void epFish(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0), d = (float)e.dir;
    const float px = e.ea, sx = e.p1.x;
    const float bite = dur - 5.f, yank = dur - 4.f, land = dur - 2.8f;
    Ang seat = sit(t);
    seat.ua[0] = 1.25f; seat.fa[0] = 1.75f;
    seat.ua[1] = 1.0f; seat.fa[1] = 1.55f;
    if (t >= yank && t < land) { seat.ua[0] = seat.ua[1] = 2.4f; seat.fa[0] = seat.fa[1] = 2.9f; seat.spine = -0.45f; }
    Skel sat = fk(seat);
    onGround(sat);
    if (t < 1.f) {
        f.at = V(lerpf(e.p0.x, sx, t), 0.f);
        f.S = posed(gait(t * TAU, 0.5f));
    } else if (t < 2.f) {
        f.at = V(sx, 0.f);
        f.S = mixS(standing(), sat, ease(t - 1.f));
    } else if (t < land) {
        f.at = V(sx, 0.f);
        f.S = sat;
    } else {
        f.at = V(sx, 0.f);
        const int what = e.seq[0];
        Ang a = what == 0 ? tada() : relief(t);
        if (what == 2) { a = Ang{}; a.ua[0] = 0.9f; a.fa[0] = 2.2f; a.ua[1] = -0.9f; a.fa[1] = -2.2f; }
        f.S = t < land + 0.8f ? mixS(sat, posed(a), ease(t - land)) : posed(a);
    }
    if (!pen) return;
    const Pal& P = pen->P;
    const float rodK = t < 2.f ? ease(t - 1.f) : 1.f;
    if (rodK <= 0.f) return;
    const vec2 hand = jointAt(f, HA0, s);
    const float rodA = t >= yank ? 0.9f * ease((t - yank) * 3.f) : 0.f;
    const vec2 tip = hand + V(d * std::cos(0.75f + rodA), std::sin(0.75f + rodA)) * (1.6f * s * rodK);
    if (t < land) pen->line({hand - V(d * 0.25f * s, 0.15f * s), tip}, 0.9f);
    const vec2 fl = V(px - d * 0.2f * s, (0.03f - 0.05f * std::pow(std::sin(t * PI), 2.f) - (t >= bite && t < yank ? 0.08f : 0.f)) * s);
    if (t < yank) {
        pen->line(sag(tip, fl, 0.2f * s), 0.4f, 0, 0.8f, 0.2f);
        pen->shape(ellipse(fl + V(0.f, 0.06f * s), 0.07f * s, 0.07f * s), 0.6f, P.red);
        const float rr = frac(t);
        pen->line(arc(V(fl.x, 0.03f * s), (0.12f + 0.3f * rr) * s, (0.04f + 0.06f * rr) * s, PI, TAU, 8), 0.5f, P.blue, 1.f - rr);
        if (t >= bite) pen->text(f.at + V(0.f, 2.f * s), "!", P.red, 0.9f * s);
        return;
    }
    // the catch: out of the water on the line, landing by him (a fish flapping, a boot, a can)
    const float u = saturate((t - yank) / (land - yank));
    const vec2 cpos = mixv(V(px, 0.f), V(sx - d * 0.9f * s, 0.f), u) + V(0.f, jumpY(u, 2.4f * s) + 0.15f * s);
    if (t < land) pen->line({tip, cpos + V(0.f, 0.15f * s)}, 0.4f, 0, 0.8f, 0.2f);
    const float spin = u < 1.f ? u * 6.f : (e.seq[0] == 0 ? 0.35f * std::sin(t * 14.f) : 0.f);
    auto X = [&](Poly p) { return his(std::move(p), cpos, s, e.dir, spin); };
    static const char* SAYS[3] = {"Yay!", "?", "Meh."};
    if (e.seq[0] == 0) {
        const Poly body = X(ellipse(V(0.f, 0.f), 0.3f, 0.13f));
        pen->shape(body, 0.8f);
        pen->colour(body, hue(P, e.seq[1]), 0.3f, 0.3f);
        pen->shape(X({V(-0.28f, 0.f), V(-0.48f, 0.14f), V(-0.48f, -0.14f), V(-0.28f, 0.f)}), 0.7f);
        pen->shape(X(ellipse(V(0.17f, 0.03f), 0.025f, 0.025f)), 0.6f);
    } else if (e.seq[0] == 1) {
        const Poly boot = X({V(-0.12f, 0.3f), V(0.08f, 0.3f), V(0.08f, 0.02f), V(0.3f, -0.02f), V(0.3f, -0.15f), V(-0.12f, -0.15f), V(-0.12f, 0.3f)});
        pen->shape(boot, 0.8f);
        pen->colour(boot, P.card, 0.5f, 0.3f);
    } else {
        const Poly can = X(rect(-0.13f, -0.18f, 0.13f, 0.18f));
        pen->shape(can, 0.8f);
        pen->colour(can, P.grey, 0.2f, 0.3f);
    }
    if (u < 1.f)
        for (int k = 0; k < 3; k++) // splashes
            pen->line({V(px + (k - 1) * 0.3f * s, 0.05f * s), V(px + (k - 1) * 0.5f * s, (0.35f - 0.1f * std::abs(k - 1)) * s * (1.f - u))}, 0.5f, P.blue);
    else pen->text(f.at + V(0.f, 2.6f * s), SAYS[e.seq[0]], P.red, 0.6f * s);
}

void epJuggle(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    // a cascade: a throw each half beat, from one hand to the other, each thing a beat up in the air
    const float T = std::min(t, dur - 1.f);
    auto handPos = [&](int k, float tt) {
        const float side = k == 0 ? 1.f : -1.f;
        const float ph = frac(tt - k * 0.5f);
        if (ph < 0.5f) return V(side * lerpf(0.12f, 0.42f, ph * 2.f), 0.95f + 0.1f * std::sin(ph * TAU));
        return V(side * lerpf(0.42f, 0.12f, (ph - 0.5f) * 2.f), 0.95f - 0.12f * std::sin((ph - 0.5f) * TAU));
    };
    Ang a;
    a.head = -0.2f;
    Skel S = posed(a);
    const vec2 h0 = handPos(0, T), h1 = handPos(1, T);
    S.j[EL0] = V(0.24f, 1.18f); S.j[HA0] = h0;
    S.j[EL1] = V(-0.24f, 1.18f); S.j[HA1] = h1;
    f.S = S;
    f.at = e.p0;
    if (t >= dur - 1.f) f.S = mixS(S, posed(tada()), ease((t - dur + 1.f) * 2.5f));
    if (!pen) return;
    const Pal& P = pen->P;
    const int kind = e.seq[0]; // balls, clubs, rings
    for (int i = 0; i < 3; i++) {
        vec2 q;
        float spin = 0.f;
        if (t < dur - 1.f) {
            const int m = i + 3 * (int)std::floor((T * 2.f - i) / 3.f);
            const float tau = T - m * 0.5f;
            const int from = ((m % 2) + 2) % 2, to = 1 - from;
            const float tm = m * 0.5f;
            if (tau < 1.f) {
                const vec2 a0 = handPos(from, tm), b0 = handPos(to, tm + 1.f);
                q = mixv(a0, b0, tau) + V(0.f, jumpY(tau, 1.2f));
                spin = tau * TAU;
            } else q = handPos(to, T);
        } else q = i < 2 ? f.S.j[HA0 + 2 * i] : f.S.j[HEAD] + V(0.f, HEADR + 0.11f); // two caught, one on his head
        const vec2 at = f.at + V(q.x * s * (float)e.dir, q.y * s);
        const uint32_t col = hue(P, e.seq[1] + i * 2);
        if (kind == 0) ballShape(*pen, at, 0.11f * s, col);
        else if (kind == 1) {
            const vec2 dd = rot(V(0.f, 1.f), spin + i);
            pen->line({at - dd * (0.18f * s), at + dd * (0.06f * s)}, 0.7f);
            const Poly bulb = ellipse(at + dd * (0.14f * s), 0.06f * s, 0.06f * s);
            pen->shape(bulb, 0.7f, col);
            pen->colour(bulb, col, 0.f, 0.25f);
        } else pen->shape(ellipse(at, 0.15f * s, 0.15f * s * (0.35f + 0.65f * std::fabs(std::cos(spin)))), 1.1f, col);
    }
}

void epMusic(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const bool drums = e.seq[0] == 1;
    const float in = ease(saturate(t)), outK = ease(saturate(t - (dur - 1.f)));
    f.at = e.p0;
    if (!drums) { // sitting with a guitar, strumming on the beats
        Ang a = sit(t);
        a.ua[1] = 1.05f; a.fa[1] = 2.0f;
        const float strum = std::sin(frac(t) * PI);
        a.ua[0] = 0.55f + 0.2f * strum; a.fa[0] = 1.75f - 0.6f * strum;
        Skel S = fk(a);
        onGround(S);
        f.S = mixS(mixS(standing(), S, in), standing(), outK);
    } else { // on a stool behind his drum kit
        Ang a;
        a.hip = V(0.f, 0.62f);
        a.th[0] = 1.45f; a.sh[0] = 0.05f + 0.25f * std::max(0.f, std::sin(t * TAU));
        a.th[1] = 1.3f; a.sh[1] = -0.05f;
        a.spine = 0.15f + 0.05f * std::sin(t * TAU);
        a.head = 0.2f * std::fabs(std::sin(t * PI));
        const int b = (int)std::floor(t * 2.f);
        const float hit = std::sin(frac(t * 2.f) * PI);
        a.ua[0] = 0.9f + ((b & 1) ? 0.6f * hit : 0.f); a.fa[0] = a.ua[0] + 0.9f;
        a.ua[1] = 0.8f + ((b & 1) ? 0.f : 0.6f * hit); a.fa[1] = a.ua[1] + 0.8f;
        if (t > dur - 2.f) { a.ua[0] = 1.9f; a.fa[0] = 1.9f; } // the cymbal, at the end
        Skel S = fk(a);
        f.S = mixS(mixS(standing(), S, in), standing(), outK);
    }
    if (!pen || outK >= 1.f) return;
    const Pal& P = pen->P;
    const uint32_t col = hue(P, e.seq[1]);
    if (!drums) {
        const vec2 lap = jointAt(f, HIP, s) + V((float)e.dir * 0.32f * s, 0.32f * s);
        const float ang = 0.5f;
        auto X = [&](Poly p) { return his(std::move(p), lap, s, e.dir, ang); };
        const Poly lo = X(ellipse(V(-0.05f, 0.f), 0.24f, 0.2f)), hi = X(ellipse(V(0.27f, 0.f), 0.17f, 0.15f));
        pen->shape(lo, 0.8f);
        pen->shape(hi, 0.8f);
        pen->colour(lo, col, 0.5f, 0.35f);
        pen->colour(hi, col, 0.5f, 0.35f);
        pen->shape(X(ellipse(V(0.12f, 0.f), 0.06f, 0.06f)), 0.5f);
        pen->line(X({V(0.42f, 0.f), V(1.05f, 0.f)}), 1.1f);
        pen->line(X({V(1.05f, -0.04f), V(1.22f, 0.04f)}), 1.6f);
    } else {
        const float d = (float)e.dir;
        const vec2 kick = V(e.p0.x + d * 0.75f * s, 0.38f * s);
        pen->shape(ellipse(kick, 0.36f * s, 0.36f * s), 0.9f);
        pen->colour(ellipse(kick, 0.36f * s, 0.36f * s), col, 0.4f, 0.4f);
        pen->shape(ellipse(kick, 0.14f * s, 0.14f * s), 0.5f);
        const vec2 snare = V(e.p0.x + d * 0.6f * s, 0.95f * s);
        pen->shape(rect(snare.x - 0.25f * s, snare.y - 0.08f * s, snare.x + 0.25f * s, snare.y + 0.08f * s), 0.8f);
        pen->line({V(snare.x, snare.y - 0.08f * s), V(snare.x - d * 0.1f * s, 0.f)}, 0.6f);
        const float tilt = t > dur - 2.f ? 0.25f * std::sin(t * 20.f) : 0.f;
        const vec2 cym = V(e.p0.x + d * 1.25f * s, 1.55f * s);
        pen->line({V(cym.x, 0.f), cym}, 0.6f);
        pen->line({cym + rot(V(-0.35f * s, 0.f), tilt), cym + rot(V(0.35f * s, 0.f), tilt)}, 1.4f, P.yellow);
        pen->line({jointAt(f, HA0, s), jointAt(f, HA0, s) + V(d * 0.35f * s, -0.1f * s)}, 0.7f);
        pen->line({jointAt(f, HA1, s), jointAt(f, HA1, s) + V(d * 0.35f * s, -0.15f * s)}, 0.7f);
        if (t > dur - 2.f) pen->text(V(e.p0.x, 3.f * s), "Ba-dum tss!", P.red, 0.5f * s);
    }
    // a note out each beat, rising
    for (int k = 0; k < 2; k++) {
        const int nb = (int)std::floor(t) - k;
        if (nb < 1) continue;
        const float u = frac(t) + k;
        const vec2 from = drums ? V(e.p0.x + (float)e.dir * 0.8f * s, 1.3f * s) : jointAt(f, HIP, s) + V((float)e.dir * 0.4f * s, 0.6f * s);
        note(*pen, from + V((0.3f * std::sin(nb * 2.1f) + 0.3f * u * (float)e.dir) * s, 0.7f * u * s), 0.5f * s, hue(P, nb), nb % 3 == 0);
    }
}

Ang lotus(float lift) {
    Ang a;
    a.hip = V(0.f, 0.32f + lift);
    a.th[0] = 1.35f; a.sh[0] = -1.15f;
    a.th[1] = -1.35f; a.sh[1] = 1.15f;
    a.ua[0] = 0.45f; a.fa[0] = 1.4f;
    a.ua[1] = -0.45f; a.fa[1] = -1.4f;
    return a;
}
void epYoga(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    f.at = e.p0;
    // sitting up in the air ("Om"), then a tree and a warrior (in either order), and a bow
    Ang tree;
    tree.ua[0] = 2.85f; tree.fa[0] = 3.1f; tree.ua[1] = -2.85f; tree.fa[1] = -3.1f;
    tree.th[1] = -1.2f; tree.sh[1] = 0.6f;
    Ang war;
    war.hip = V(0.f, 0.7f);
    war.th[0] = 1.f; war.sh[0] = 0.05f; war.th[1] = -0.75f; war.sh[1] = -0.75f;
    war.ua[0] = 1.57f; war.fa[0] = 1.57f; war.ua[1] = -1.57f; war.fa[1] = -1.57f;
    war.head = -0.1f;
    const bool swap = e.seq[0] == 1;
    const Ang& p1 = swap ? war : tree;
    const Ang& p2 = swap ? tree : war;
    const float lift = 0.35f * ease(saturate((t - 1.5f) / 1.5f)) * (1.f - ease(saturate(t - 5.f))) + 0.04f * std::sin(t * PI * 0.5f);
    auto P = [&](const Ang& a, bool ground) { Skel S = fk(a); if (ground) onFeet(S); return S; };
    Skel lot = fk(lotus(lift));
    if (t < 1.f) f.S = mixS(standing(), lot, ease(t));
    else if (t < 5.f) f.S = lot;
    else if (t < 6.f) f.S = mixS(lot, P(p1, true), ease(t - 5.f));
    else if (t < 8.5f) f.S = P(p1, true);
    else if (t < 9.f) f.S = mixS(P(p1, true), P(p2, true), ease((t - 8.5f) * 2.f));
    else if (t < dur - 1.5f) f.S = P(p2, true);
    else {
        Ang nm; // hands together, a bow
        nm.ua[0] = 0.6f; nm.fa[0] = 2.5f; nm.ua[1] = 0.6f; nm.fa[1] = 2.5f;
        nm.spine = 0.35f * std::sin(saturate(t - dur + 1.5f) * PI);
        f.S = mixS(P(p2, true), P(nm, true), ease((t - dur + 1.5f) * 2.f));
    }
    if (!pen) return;
    const Pal& Pl = pen->P;
    const Poly mat = rect(e.p0.x - 1.1f * s, -0.03f * s, e.p0.x + 1.1f * s, 0.07f * s);
    pen->shape(mat, 0.6f);
    pen->colour(mat, hue(Pl, e.seq[1]), 0.f, 0.3f);
    if (t >= 2.f && t < 5.f)
        for (int k = 0; k < 2; k++) {
            const float u = frac((t - 2.f) / 2.f + k * 0.5f);
            pen->text(jointAt(f, HEAD, s) + V((0.5f + 0.4f * u) * s * (k ? -1.f : 1.f), (0.4f + 0.8f * u) * s), "Om", Pl.purple, 0.45f * s, 1.f - u);
        }
}

void epStars(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0), d = (float)e.dir;
    const float ex = e.ea; // the eyepiece
    const float wish = std::floor(dur * 0.5f);
    if (t < 1.f) {
        f.at = V(lerpf(e.p0.x, ex - d * 0.15f * s, t), 0.f);
        f.S = posed(gait(t * TAU, 0.5f));
    } else {
        f.at = V(ex - d * 0.15f * s, 0.f);
        Ang look;
        look.spine = 0.45f; look.head = 0.2f;
        look.hip = V(-0.05f, 0.8f);
        look.th[0] = 0.25f; look.sh[0] = 0.f; look.th[1] = -0.15f; look.sh[1] = -0.2f;
        look.ua[0] = 1.2f; look.fa[0] = 1.9f; look.ua[1] = 0.9f; look.fa[1] = 1.5f;
        Ang point = lookUp();
        point.ua[0] = 2.4f; point.fa[0] = 2.6f;
        if (t < wish + 1.f) f.S = mixS(standing(), posed(look), ease(t - 1.f));
        else if (t < dur - 2.f) f.S = mixS(posed(look), posed(point), ease((t - wish - 1.f) * 3.f));
        else f.S = mixS(posed(point), posed(wave(t)), ease(t - dur + 2.f));
    }
    if (!pen) return;
    const Pal& P = pen->P;
    // a shooting star across the sky
    if (t >= wish && t < wish + 1.6f) {
        const float u = (t - wish) / 1.6f;
        const vec2 a0 = V(-d * 1.8f * s, e.h + 1.5f * s), a1 = V(d * 2.2f * s, e.h - 0.5f * s);
        const vec2 p = mixv(a0, a1, u) + V(e.p0.x * 0.f + ex, 0.f);
        starShape(*pen, p, 0.2f * s, P.yellow, t);
        pen->line({p - (a1 - a0) * 0.05f, p - (a1 - a0) * 0.3f}, 0.6f, P.yellow, 0.8f);
        pen->line({p - (a1 - a0) * 0.06f + V(0.f, 0.08f * s), p - (a1 - a0) * 0.22f + V(0.f, 0.08f * s)}, 0.4f, P.orange, 0.6f);
    }
    if (t >= wish + 1.2f && t < wish + 2.8f) pen->text(f.at + V(-d * 0.5f * s, 2.6f * s), "Wow!", P.red, 0.55f * s);
}

void epPaint(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0), d = (float)e.dir;
    const float stand = e.ea - d * 0.95f * s;
    if (t < 1.f) {
        f.at = V(lerpf(e.p0.x, stand, t), 0.f);
        f.S = posed(gait(t * TAU, 0.5f));
    } else if (t < dur - 3.f) { // painting
        f.at = V(stand, 0.f);
        Ang a;
        a.ua[0] = 1.45f + 0.25f * std::sin(t * TAU * 1.5f); a.fa[0] = 1.7f + 0.3f * std::sin(t * TAU * 1.5f + 1.f);
        a.ua[1] = 0.4f; a.fa[1] = 1.4f; // the palette
        a.head = 0.1f * std::sin(t * PI);
        f.S = posed(a);
    } else if (t < dur - 1.5f) { // a step back, a hand on his chin
        f.at = V(stand - d * 0.5f * s * ease((t - dur + 3.f) * 2.f), 0.f);
        Ang a;
        a.ua[0] = 0.5f; a.fa[0] = 2.8f; a.ua[1] = 0.3f; a.fa[1] = 1.4f;
        a.head = 0.15f; a.spine = -0.1f;
        f.S = posed(a);
    } else { // ta-da
        f.at = V(stand - d * 0.5f * s, 0.f);
        Ang a;
        a.ua[0] = 1.9f; a.fa[0] = 1.9f; a.ua[1] = -2.4f; a.fa[1] = -2.6f;
        f.S = mixS(standing(), posed(a), ease((t - dur + 1.5f) * 3.f));
    }
    if (!pen) return;
    const Pal& P = pen->P;
    // a beret, a brush, the palette
    const vec2 h = jointAt(f, HEAD, s);
    const Poly beret = {h + V(-0.24f * s, 0.12f * s), h + V(-0.05f * s, 0.27f * s), h + V(0.2f * s, 0.2f * s), h + V(0.25f * s, 0.1f * s), h + V(-0.24f * s, 0.12f * s)};
    pen->shape(beret, 0.7f);
    pen->colour(beret, P.red, 0.4f, 0.25f);
    if (t >= 1.f && t < dur - 1.5f) {
        const vec2 hd = jointAt(f, HA0, s);
        pen->line({hd, hd + V(d * 0.3f * s, 0.12f * s)}, 0.7f);
        pen->shape(ellipse(hd + V(d * 0.33f * s, 0.13f * s), 0.04f * s, 0.04f * s), 0.6f, hue(P, (int)std::floor(t)));
        const vec2 pl = jointAt(f, HA1, s);
        const Poly pal = ellipse(pl + V(0.f, 0.03f * s), 0.22f * s, 0.1f * s);
        pen->shape(pal, 0.6f);
        pen->colour(pal, P.card, 0.f, 0.25f);
        for (int k = 0; k < 3; k++) pen->shape(ellipse(pl + V((k - 1) * 0.11f * s, 0.05f * s), 0.03f * s, 0.03f * s), 0.6f, hue(P, k));
    }
}

void dog(Pen& pen, vec2 at, float s, int dir, float run, float wag, uint32_t col, bool stick) {
    const Pal& P = pen.P;
    auto X = [&](Poly p) { return his(std::move(p), at, s, dir); };
    const Poly body = X(ellipse(V(0.f, 0.42f), 0.36f, 0.15f));
    pen.shape(body, 0.8f);
    pen.colour(body, col, 0.3f, 0.3f);
    const Poly head = X(ellipse(V(0.42f, 0.64f), 0.15f, 0.13f));
    pen.shape(head, 0.8f);
    pen.colour(head, col, 0.3f, 0.25f);
    pen.line(X({V(0.52f, 0.6f), V(0.66f, 0.57f), V(0.64f, 0.5f), V(0.5f, 0.52f)}), 0.7f);
    pen.line(X({V(0.36f, 0.74f), V(0.28f, 0.6f), V(0.34f, 0.5f)}), 0.8f, P.ink);
    pen.shape(X(ellipse(V(0.47f, 0.67f), 0.015f, 0.015f)), 0.8f);
    for (int k = 0; k < 4; k++) {
        const float x = k < 2 ? 0.24f : -0.24f, sw = 0.3f * std::sin(run + k * PI * 0.5f + (k & 1) * PI);
        pen.line(X({V(x + (k & 1) * 0.04f, 0.32f), V(x + sw * 0.4f + (k & 1) * 0.04f, 0.f)}), 0.8f);
    }
    pen.line(X({V(-0.34f, 0.48f), V(-0.48f, 0.6f + 0.08f * wag), V(-0.55f, 0.72f + 0.12f * wag)}), 0.8f);
    if (stick) pen.line(X({V(0.5f, 0.53f), V(0.85f, 0.56f)}), 1.3f, P.card);
}
void epDog(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, d = (float)e.dir, dur = (float)(e.b1 - e.b0);
    // a dog comes running, he throws a stick, the dog brings it back, he pats it, it runs off
    const float x = e.p0.x, far = e.ea, edge = e.eb;
    const float tThrow = 3.f, tLand = 4.f, tBack = 7.5f;
    f.at = V(x, 0.f);
    if (t < 2.5f) f.S = mixS(standing(), posed(wave(t)), ease(t - 0.5f) * (1.f - ease(t - 2.f)));
    else if (t < tThrow + 0.5f) { // the throw, overhand
        Ang a;
        const float u = ease((t - 2.5f) / 0.8f);
        a.ua[0] = lerpf(-2.4f, 2.2f, u); a.fa[0] = a.ua[0] + 0.4f;
        a.spine = lerpf(-0.2f, 0.25f, u);
        a.th[0] = 0.3f; a.sh[0] = 0.2f; a.th[1] = -0.3f; a.sh[1] = -0.35f;
        f.S = posed(a);
    } else if (t < tBack + 0.3f) f.S = posed(Ang{});
    else if (t < dur - 2.f) { // a pat
        Ang a = crouch();
        a.ua[0] = 1.3f + 0.15f * std::sin(t * TAU * 2.f); a.fa[0] = 1.7f;
        f.S = mixS(standing(), posed(a), ease((t - tBack - 0.3f) * 3.f));
    } else f.S = mixS(posed(crouch()), posed(wave(t)), ease((t - dur + 2.f) * 3.f));
    if (!pen) return;
    const Pal& P = pen->P;
    const uint32_t col = e.seq[0] == 0 ? P.card : e.seq[0] == 1 ? P.grey : P.orange;
    // where the dog is
    const float meet = x + d * 0.9f * s;
    vec2 at;
    int dd = (int)d;
    float run = 0.f;
    bool stick = false;
    if (t < 2.f) { at = V(lerpf(edge, meet, ease(t / 2.f)), 0.f); dd = meet > edge ? 1 : -1; run = t * 9.f; }
    else if (t < tThrow + 0.3f) at = V(meet, 0.f);
    else if (t < 5.f) { at = V(lerpf(meet, far, ease((t - tThrow - 0.3f) / 1.7f)), jumpY(saturate((t - 4.2f) / 0.6f), 0.4f * s)); run = t * 10.f; }
    else if (t < tBack) { at = V(lerpf(far, meet, ease((t - 5.f) / 2.5f)), 0.f); dd = -(int)d; run = t * 10.f; stick = true; }
    else if (t < dur - 1.5f) at = V(meet, 0.f);
    else { const float u = (t - dur + 1.5f) / 1.5f; at = V(lerpf(meet, edge, ease(u)), 0.f); dd = edge > meet ? 1 : -1; run = t * 10.f; }
    if (t >= tBack && t < dur - 1.5f) dd = -(int)d;
    const float k = t > dur - 1.5f ? 1.f - 0.5f * ease((t - dur + 1.5f) / 1.5f) : 1.f;
    dog(*pen, at, s * k, dd, run, std::sin(t * 18.f), col, stick);
    // the stick: in his hand, flying, on the ground, at his feet
    if (t >= 2.5f && t < tThrow + 0.2f) {
        const vec2 hd = jointAt(f, HA0, s);
        pen->line({hd + V(-0.2f * s, -0.05f * s), hd + V(0.2f * s, 0.05f * s)}, 1.3f, P.card);
    } else if (t >= tThrow + 0.2f && t < 5.f) {
        const float u = saturate((t - tThrow - 0.2f) / (tLand - tThrow - 0.2f));
        const vec2 p = mixv(V(x + d * 0.3f * s, 2.2f * s), V(far + d * 0.6f * s, 0.05f * s), u) + V(0.f, jumpY(u, 1.5f * s));
        const vec2 q = rot(V(0.22f * s, 0.f), u * 12.f);
        pen->line({p - q, p + q}, 1.3f, P.card);
    } else if (t >= tBack && t < dur) {
        pen->line({V(meet - d * 0.6f * s, 0.04f * s), V(meet - d * 0.2f * s, 0.06f * s)}, 1.3f, P.card);
    }
    if (t >= tBack + 0.5f && t < dur - 1.5f) pen->text(V(meet, 2.2f * s), "Good boy!", P.red, 0.5f * s);
    if (t < 1.f) pen->text(V(edge, 1.3f * s), "Woof!", P.ink, 0.5f * s);
}

// ---- over to the other margin: off out of sight (shrinking away, a twinkle) and in over there (growing)
// The way out, toward the top of the margin on the well's side, and the way in, from there in the other margin.
vec2 awayPoint(const Ctx& c, int side) {
    return V((float)wellSide(side) * (c.hw[side] - 0.7f * c.s), c.top[side] - 1.2f * c.s);
}
void vanish(Pen& pen, const Fig& f, float u, float s) { // the twinkle where he went
    if (u <= 0.f || u >= 1.f) return;
    sparkle(pen, f.at + V(0.f, 0.4f * s), 0.35f * s * std::sin(u * PI), pen.P.yellow);
}
void epCannon(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, d = (float)e.dir;
    const float cx = e.ea, ang = 0.85f;
    const vec2 piv = V(cx, 0.5f * s), mouth = piv + V(d * std::cos(ang), std::sin(ang)) * (1.35f * s);
    const float tBoom = 4.f, tGone = 5.4f;
    const int o = 1 - e.side;
    if (t < 1.f) {
        f.at = V(lerpf(e.p0.x, mouth.x - d * 0.6f * s, t), 0.f);
        f.S = posed(gait(t * TAU, 0.6f));
    } else if (t < 2.f) { // in, feet first
        const float u = t - 1.f;
        f.at = mixv(V(mouth.x - d * 0.6f * s, 0.f), mouth + V(0.f, -0.3f * s), u) + V(0.f, jumpY(u, 1.2f * s));
        f.S = posed(leap());
        f.clip = u > 0.6f ? mouth.y - 0.1f * s : -1e9f;
        if (u > 0.6f) f.at.y -= (u - 0.6f) * 4.f * s;
    } else if (t < tBoom) {
        f.shown = false;
    } else if (t < tGone) { // out, flying off, getting small
        const float u = (t - tBoom) / (tGone - tBoom);
        const vec2 away = awayPoint(c, e.side);
        f.at = bez(mouth, mouth + V(d * std::cos(ang), std::sin(ang)) * (3.f * s), away + V(0.f, 1.5f * s), away, u);
        f.dir = away.x > mouth.x ? 1 : -1;
        f.k = 1.f - 0.82f * ease(u);
        Ang a = armsUp();
        f.S = fk(a);
        turnS(f.S, f.S.j[HIP], -ang + PI * 0.5f - 0.5f * u);
    } else if (t < e.flip) {
        f.shown = false;
        f.at = awayPoint(c, e.side);
    } else { // in from the top over there, a tumble, a landing
        f.side = o;
        const float u = saturate((t - e.flip) / 1.5f);
        const vec2 from = awayPoint(c, o);
        f.at = bez(from, from + V(-(float)wellSide(o) * 0.5f * s, 1.f * s), V(e.p1.x, 3.f * s), V(e.p1.x, 0.f), u);
        f.k = 0.2f + 0.8f * ease(u);
        if (u < 1.f) {
            f.S = fk(flail(t));
            turnS(f.S, f.S.j[HIP], -TAU * 1.5f * ease(u));
        } else f.S = mixS(posed(crouch()), posed(tada()), ease((t - e.flip - 1.5f) / 0.8f));
    }
    if (!pen) return;
    const Pal& P = pen->P;
    if (t >= tBoom && t < tBoom + 1.2f && f.side == e.side) {
        const float u = (t - tBoom) / 1.2f;
        for (int k = 0; k < 3; k++) smoke(*pen, mouth + V(d * (0.2f + 0.4f * k) * s * u, (0.2f + 0.3f * k) * s * u), (0.25f + 0.2f * k) * s * (0.5f + u), 1.f - u);
        pen->text(mouth + V(d * 0.6f * s, 1.2f * s), "Boom!", P.red, 0.75f * s);
    }
    if (t >= 2.f && t < tBoom) { // the fuse burning down
        const float u = (t - 2.f) / (tBoom - 2.f);
        const vec2 back = piv - V(d * std::cos(ang), std::sin(ang)) * (0.55f * s);
        const vec2 end = back + V(-d * 0.5f * s * (1.f - u), -0.1f * s);
        pen->line({back, end}, 0.6f);
        sparkle(*pen, end, 0.15f * s, P.orange);
        if (t >= 2.5f) pen->text(back + V(-d * 0.6f * s, 0.6f * s), "Tsss", P.ink, 0.4f * s);
    }
    if (f.side == e.side) vanish(*pen, f, (t - tGone) / 0.6f, s);
}

void plane(Pen& pen, vec2 at, float s, int dir, float ang, uint32_t col) {
    auto X = [&](Poly p) { return his(std::move(p), at, s, dir, ang); };
    const Poly body = X({V(0.95f, 0.15f), V(-0.8f, 0.5f), V(-0.62f, 0.15f), V(-0.75f, -0.05f), V(0.95f, 0.15f)});
    pen.shape(body, 0.9f);
    pen.line(X({V(0.95f, 0.15f), V(-0.62f, 0.15f)}), 0.6f);
    pen.colour(X({V(0.95f, 0.15f), V(-0.8f, 0.5f), V(-0.62f, 0.15f), V(0.95f, 0.15f)}), col, 0.2f, 0.4f, 0.8f);
}
void epPlane(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const int o = 1 - e.side;
    const float tOff = 2.f, tGone = 5.f;
    vec2 pa = e.p0;
    float pang = 0.f, k = 1.f;
    int pd = wellSide(e.side);
    bool on = false;
    Skel seat = fk(sit(t));
    onGround(seat);
    if (t < 1.f) {
        f.at = e.p0;
        f.S = mixS(standing(), posed(crouch()), ease(t));
        pa = e.p0 + V((float)pd * 1.2f * s, 0.f);
    } else if (t < tOff) { // he hops on
        const float u = t - 1.f;
        pa = e.p0 + V((float)pd * 1.2f * s, 0.f);
        f.at = mixv(e.p0, pa + V(0.f, 0.3f * s), u) + V(0.f, jumpY(u, 0.8f * s));
        f.S = u < 0.7f ? posed(leap()) : mixS(posed(leap()), seat, (u - 0.7f) / 0.3f);
        f.dir = pd;
    } else if (t < tGone) { // off: along the ground, up, away
        const float u = (t - tOff) / (tGone - tOff);
        const vec2 a0 = e.p0 + V((float)pd * 1.2f * s, 0.f), away = awayPoint(c, e.side);
        const vec2 c1 = a0 + V((float)pd * 2.5f * s, 0.f), c2 = V(away.x - (float)pd * 2.f * s, away.y - 2.f * s);
        pa = bez(a0, c1, c2, away, u);
        const vec2 tg = bez(a0, c1, c2, away, std::min(1.f, u + 0.02f)) - bez(a0, c1, c2, away, std::max(0.f, u - 0.02f));
        pang = std::atan2(tg.y, std::fabs(tg.x));
        k = 1.f - 0.8f * ease(saturate((u - 0.3f) / 0.7f));
        on = true;
    } else if (t < e.flip) {
        f.shown = false;
        f.at = awayPoint(c, e.side);
        k = 0.f;
    } else if (t < dur - 1.5f) { // in, gliding down, landing
        f.side = o;
        pd = -wellSide(o);
        const float u = saturate((t - e.flip) / (dur - 1.5f - e.flip));
        const vec2 a0 = awayPoint(c, o), a1 = V(e.p1.x - (float)pd * 1.2f * s, 0.f);
        const vec2 c1 = a0 + V((float)pd * 2.f * s, -0.5f * s), c2 = a1 + V(-(float)pd * 2.5f * s, 0.f);
        pa = bez(a0, c1, c2, a1, u);
        const vec2 tg = bez(a0, c1, c2, a1, std::min(1.f, u + 0.02f)) - bez(a0, c1, c2, a1, std::max(0.f, u - 0.02f));
        pang = std::atan2(tg.y, std::fabs(tg.x));
        k = 0.2f + 0.8f * ease(u);
        on = true;
    } else { // off it
        f.side = o;
        pd = -wellSide(o);
        pa = V(e.p1.x - (float)pd * 1.2f * s, 0.f);
        const float u = saturate((t - dur + 1.5f) / 0.8f);
        f.at = mixv(pa + V(0.f, 0.3f * s), V(e.p1.x, 0.f), u) + V(0.f, jumpY(u, 0.8f * s));
        f.dir = pd;
        f.S = u < 1.f ? posed(leap()) : mixS(posed(crouch()), posed(tada()), ease((t - dur + 0.7f) * 2.f));
    }
    if (on) { // riding it: sitting on its back, turned with it
        f.dir = pd;
        f.k = k;
        f.S = seat;
        turnS(f.S, V(0.f, 0.f), pang);
        f.at = his(V(-0.15f, 0.3f), pa, s * k, pd, pang);
    }
    if (!pen || k <= 0.f) {
        if (pen) vanish(*pen, f, (t - tGone) / 0.6f, s);
        return;
    }
    plane(*pen, pa, s * k, pd, pang, hue(pen->P, e.seq[1]));
}

void epBalloons(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const int o = 1 - e.side;
    const float tUp = 1.5f, tGone = 6.f, tLand = dur - 1.5f;
    Ang hold = hang(t);
    hold.ua[1] = -0.3f; hold.fa[1] = -0.1f; // one hand up holding the strings
    const float sway = std::sin(t * PI * 0.5f);
    float k = 1.f;
    if (t < tUp) {
        f.at = e.p0;
        f.S = mixS(standing(), posed(hold), ease(t));
    } else if (t < tGone) {
        const float u = (t - tUp) / (tGone - tUp);
        const vec2 away = awayPoint(c, e.side);
        f.at = bez(e.p0, e.p0 + V(0.f, 3.f * s), away - V((float)wellSide(e.side) * 1.f * s, 2.f * s), away, ease(u)) + V(0.4f * s * sway, 0.f);
        k = 1.f - 0.82f * ease(saturate((u - 0.35f) / 0.65f));
        f.S = fk(hold);
        onFeet(f.S);
    } else if (t < e.flip) {
        f.shown = false;
        f.at = awayPoint(c, e.side);
        k = 0.f;
    } else if (t < tLand) {
        f.side = o;
        const float u = (t - e.flip) / (tLand - e.flip);
        const vec2 from = awayPoint(c, o);
        f.at = bez(from, from + V(-(float)wellSide(o) * 1.f * s, -1.f * s), V(e.p1.x, 3.f * s), V(e.p1.x, 0.f), ease(u)) + V(0.3f * s * sway * (1.f - u), 0.f);
        k = 0.18f + 0.82f * ease(saturate(u / 0.7f));
        f.S = fk(hold);
        onFeet(f.S);
    } else {
        f.side = o;
        f.at = V(e.p1.x, 0.f);
        f.S = mixS(posed(hold), posed(tada()), ease((t - tLand) * 2.f));
    }
    f.k = k;
    if (!pen || !f.shown) {
        if (pen && t >= tGone) vanish(*pen, f, (t - tGone) / 0.6f, s);
        return;
    }
    const Pal& P = pen->P;
    const vec2 hand = jointAt(f, HA0, s);
    const float sk = s * k;
    const int n = t >= tLand ? 2 : 3;
    const float grow = ease(saturate(t * 1.5f));
    for (int i = 0; i < n; i++) {
        const float a = (i - 1) * 0.45f + 0.1f * std::sin(t * PI + i);
        const vec2 b = hand + V(std::sin(a), std::cos(a)) * (1.3f * sk) + V(0.f, 0.15f * sk * (i == 1)) + (t >= tLand ? V(0.f, (t - tLand) * 2.f * s) : V(0.f, 0.f));
        const Poly bl = ellipse(b, 0.3f * sk * grow, 0.37f * sk * grow);
        pen->shape(bl, 0.8f);
        pen->colour(bl, hue(P, e.seq[1] + i * 2), 0.5f, 0.4f);
        pen->line({hand, b - V(0.f, 0.37f * sk * grow)}, 0.4f, 0, 0.8f, 0.3f);
    }
    if (t >= tLand - 0.3f && t < tLand + 0.8f) {
        const vec2 b = hand + V(0.45f, 1.3f) * sk;
        sparkle(*pen, b, 0.35f * s, P.red);
        pen->text(b + V(0.5f * s, 0.3f * s), "Pop!", P.red, 0.55f * s);
    }
}

void epRocket(Fig& f, const Story::Ep& e, float t, const Ctx& c, Pen* pen) {
    const float s = c.s, dur = (float)(e.b1 - e.b0);
    const int o = 1 - e.side;
    const float tLift = 4.f, tGone = 5.6f, tLand = dur - 1.5f;
    Ang arms; // arms down by his sides, then a fist up
    arms.ua[0] = -0.25f; arms.fa[0] = -0.15f; arms.ua[1] = -0.35f; arms.fa[1] = -0.2f;
    Ang fly = armsUp();
    fly.ua[1] = -0.3f; fly.fa[1] = -0.2f;
    bool flames = false;
    float k = 1.f;
    if (t < tLift) {
        f.at = e.p0;
        f.S = posed(t < 3.f ? arms : mixA(arms, crouch(), ease((t - 3.f) * 2.f)));
        flames = t > 3.5f;
    } else if (t < tGone) {
        const float u = (t - tLift) / (tGone - tLift);
        const vec2 away = awayPoint(c, e.side);
        f.at = bez(e.p0, e.p0 + V(0.f, 4.f * s), away - V(0.f, 2.f * s), away, u * u);
        k = 1.f - 0.82f * ease(saturate((u - 0.3f) / 0.7f));
        f.S = posed(fly);
        flames = true;
    } else if (t < e.flip) {
        f.shown = false;
        f.at = awayPoint(c, e.side);
        k = 0.f;
    } else if (t < tLand) { // down feet first over there, the flames braking
        f.side = o;
        const float u = (t - e.flip) / (tLand - e.flip);
        const vec2 from = awayPoint(c, o);
        f.at = bez(from, from + V(-(float)wellSide(o) * 1.f * s, 0.f), V(e.p1.x, 2.f * s), V(e.p1.x, 0.f), 1.f - (1.f - u) * (1.f - u));
        k = 0.18f + 0.82f * ease(saturate(u / 0.7f));
        f.S = posed(arms);
        flames = true;
    } else {
        f.side = o;
        f.at = V(e.p1.x, 0.f);
        f.S = mixS(posed(crouch()), posed(tada()), ease((t - tLand) * 2.f));
    }
    f.k = k;
    if (!pen) return;
    const Pal& P = pen->P;
    if (!f.shown) { vanish(*pen, f, (t - tGone) / 0.6f, s); return; }
    // the jetpack on his back, its flames
    const float sk = s * k;
    const vec2 back = jointAt(f, HIP, s) * 0.45f + jointAt(f, NECK, s) * 0.55f - V((float)f.dir * 0.2f * sk, 0.f);
    const Poly pack = rect(back.x - 0.13f * sk, back.y - 0.3f * sk, back.x + 0.13f * sk, back.y + 0.25f * sk);
    pen->shape(pack, 0.8f);
    pen->colour(pack, hue(P, e.seq[1]), 0.5f, 0.3f);
    if (flames) {
        const float fl = (0.35f + 0.15f * std::sin(t * 30.f)) * sk;
        const vec2 n0 = V(back.x, back.y - 0.3f * sk);
        pen->line({n0 + V(-0.1f * sk, 0.f), n0 + V(0.f, -fl), n0 + V(0.1f * sk, 0.f)}, 0.8f, P.orange);
        pen->line({n0 + V(-0.05f * sk, 0.f), n0 + V(0.f, -fl * 0.6f), n0 + V(0.05f * sk, 0.f)}, 0.7f, P.red);
    }
    if (t < tLift && t >= 1.f) {
        static const char* COUNT[3] = {"3", "2", "1"};
        pen->text(f.at + V(0.f, 2.6f * s), COUNT[std::min(2, (int)std::floor(t - 1.f))], P.red, 0.7f * s);
    }
    if (t >= tLift && t < tLift + 1.2f) {
        const float u = t - tLift;
        for (int i = 0; i < 3; i++) smoke(*pen, e.p0 + V((i - 1) * 0.5f * s * (0.5f + u), 0.25f * s), (0.25f + 0.15f * u) * s, 1.f - u / 1.2f);
        pen->text(e.p0 + V(0.f, 2.6f * s), "Whoosh!", P.red, 0.6f * s);
    }
}

// What he paints on the easel (about 1.2 x 1, centred on `c`): a house in the sun, a flower, a heart, himself,
// a boat on the waves, a cat.
const int PICTURES = 6;
void picture(Pen& pen, int var, vec2 c, float s) {
    const Pal& P = pen.P;
    auto X = [&](Poly p) { return xform(std::move(p), c, s); };
    auto E = [&](float x, float y, float rx, float ry) { return X(ellipse(V(x, y), rx, ry)); };
    switch (var % PICTURES) {
    case 0: {
        const Poly wall = X(rect(-0.4f, -0.45f, 0.15f, -0.02f)), roof = X({V(-0.48f, -0.02f), V(-0.12f, 0.3f), V(0.23f, -0.02f), V(-0.48f, -0.02f)});
        pen.shape(wall, 0.7f);
        pen.colour(wall, P.yellow, 0.3f, 0.3f);
        pen.shape(roof, 0.7f);
        pen.colour(roof, P.red, 0.6f, 0.3f);
        pen.shape(X(rect(-0.2f, -0.45f, -0.06f, -0.2f)), 0.6f);
        pen.shape(E(0.35f, 0.3f, 0.13f, 0.13f), 0.6f, P.orange);
        pen.colour(E(0.35f, 0.3f, 0.13f, 0.13f), P.yellow, 0.f, 0.25f);
        pen.line(X({V(-0.55f, -0.45f), V(0.55f, -0.45f)}), 0.6f, P.green);
        break;
    }
    case 1: {
        pen.line(X({V(0.f, -0.48f), V(0.02f, -0.1f), V(0.f, 0.1f)}), 0.8f, P.green);
        pen.shape(X({V(0.01f, -0.3f), V(0.2f, -0.2f), V(0.28f, -0.25f), V(0.12f, -0.33f), V(0.01f, -0.3f)}), 0.6f, P.green);
        for (int k = 0; k < 6; k++) {
            const vec2 q = V(0.f, 0.22f) + V(std::cos(k * TAU / 6.f), std::sin(k * TAU / 6.f)) * 0.17f;
            pen.shape(E(q.x, q.y, 0.09f, 0.09f), 0.6f, P.purple);
            pen.colour(E(q.x, q.y, 0.09f, 0.09f), P.purple, 0.5f, 0.2f, 0.7f);
        }
        pen.colour(E(0.f, 0.22f, 0.08f, 0.08f), P.yellow, 0.f, 0.2f);
        break;
    }
    case 2: {
        Poly h;
        for (int i = 0; i <= 30; i++) {
            const float a = i / 30.f * TAU;
            h.push_back(V(0.32f * std::pow(std::sin(a), 3.f), 0.26f * std::cos(a) - 0.1f * std::cos(2 * a) - 0.04f * std::cos(3 * a)));
        }
        pen.shape(X(h), 0.8f, P.red);
        pen.colour(X(h), P.red, 0.6f, 0.3f);
        break;
    }
    case 3: { // himself, with his scarf
        pen.shape(E(0.f, 0.25f, 0.1f, 0.1f), 0.6f);
        pen.line(X({V(0.f, 0.15f), V(0.f, -0.15f)}), 0.6f);
        pen.line(X({V(-0.22f, 0.15f), V(0.f, 0.08f), V(0.22f, 0.22f)}), 0.6f);
        pen.line(X({V(-0.14f, -0.45f), V(0.f, -0.15f), V(0.14f, -0.45f)}), 0.6f);
        pen.line(X({V(0.f, 0.12f), V(-0.18f, 0.06f), V(-0.28f, 0.1f)}), 1.4f, P.red);
        pen.line(X({V(-0.4f, -0.47f), V(0.4f, -0.47f)}), 0.5f, P.green);
        break;
    }
    case 4: { // a boat on the waves
        const Poly hull = X({V(-0.35f, -0.15f), V(0.35f, -0.15f), V(0.25f, -0.3f), V(-0.25f, -0.3f), V(-0.35f, -0.15f)});
        pen.shape(hull, 0.7f);
        pen.colour(hull, P.card, 0.2f, 0.25f);
        pen.line(X({V(0.f, -0.15f), V(0.f, 0.35f)}), 0.6f);
        const Poly sail = X({V(0.02f, 0.33f), V(0.28f, -0.1f), V(0.02f, -0.1f), V(0.02f, 0.33f)});
        pen.shape(sail, 0.6f);
        pen.colour(sail, P.red, 0.5f, 0.2f);
        Poly w;
        for (int i = 0; i <= 16; i++) w.push_back(V(-0.55f + 1.1f * i / 16.f, -0.32f + 0.04f * std::sin(i * 1.6f)));
        pen.line(X(w), 0.7f, P.blue);
        break;
    }
    default: { // a cat
        const Poly head = X({V(-0.3f, 0.05f), V(-0.28f, 0.35f), V(-0.12f, 0.18f), V(0.12f, 0.18f), V(0.28f, 0.35f), V(0.3f, 0.05f), V(0.2f, -0.2f),
                             V(0.f, -0.27f), V(-0.2f, -0.2f), V(-0.3f, 0.05f)});
        pen.shape(head, 0.8f);
        pen.colour(head, P.orange, 0.4f, 0.3f);
        pen.shape(E(-0.11f, 0.03f, 0.04f, 0.05f), 0.6f);
        pen.shape(E(0.11f, 0.03f, 0.04f, 0.05f), 0.6f);
        pen.line(X({V(-0.05f, -0.1f), V(0.f, -0.13f), V(0.05f, -0.1f)}), 0.6f);
        for (int k = -1; k <= 1; k += 2) {
            pen.line(X({V(0.1f * k, -0.1f), V(0.42f * k, -0.05f)}), 0.4f);
            pen.line(X({V(0.1f * k, -0.13f), V(0.42f * k, -0.17f)}), 0.4f);
        }
        break;
    }
    }
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
        const int style = e.seq.empty() ? RU_RUN : e.seq[0];
        const float u = saturate(t / dur);
        f.at = mixv(e.p0, e.p1, u);
        const float ph = std::min(t, dur) * e.rate * PI;
        Ang a = gait(ph, e.h);
        if (style == RU_MOONWALK) { // facing back, gliding backwards
            a = gait(-ph, 0.45f);
            a.spine = -0.05f;
            f.dir = -e.dir;
        } else if (style == RU_SNEAK) { // on tiptoe, bent over, the hands up
            a = gait(ph, 0.55f);
            a.hip = V(0.f, 0.76f);
            a.spine = 0.5f; a.head = -0.45f;
            a.ua[0] = 1.1f; a.fa[0] = 2.6f; a.ua[1] = 0.7f; a.fa[1] = 2.3f;
        } else if (style == RU_SKIP) { // a skip a beat, a knee up
            const int k = (int)std::floor(t) & 1;
            const float v = std::sin(frac(t) * PI);
            a = Ang{};
            a.th[k] = 1.35f * v; a.sh[k] = a.th[k] - 1.5f * v;
            a.th[1 - k] = -0.25f * v; a.sh[1 - k] = -0.3f * v;
            a.ua[k] = -0.7f * v; a.fa[k] = a.ua[k] + 0.6f;
            a.ua[1 - k] = 1.3f * v; a.fa[1 - k] = a.ua[1 - k] + 0.8f;
            f.at.y = jumpY(frac(t), 0.4f * s);
        }
        f.S = fk(a);
        onFeet(f.S);
        if (style == RU_RUN && e.h > 0.8f) shiftS(f.S, V(0.f, 0.08f * std::max(0.f, std::cos(2.f * ph))));
        if (pen && style == RU_SNEAK && t > 0.5f && t < dur - 0.5f) pen->text(f.at + V(-(float)e.dir * 0.5f * s, 2.4f * s), "Shh...", pen->P.ink, 0.45f * s);
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
                fallingThing(*pen, kind, V(xo, lerpf(c.top[e.side] - 1.6f * s, 0.f, u * u)), s);
                pen->line({V(xo - 0.35f * s, lerpf(c.top[e.side] - 1.6f * s, 0.f, u * u) + 1.5f * s),
                           V(xo - 0.35f * s, lerpf(c.top[e.side] - 1.6f * s, 0.f, u * u) + 2.3f * s)}, 0.5f, 0, 0.6f);
                pen->line({V(xo + 0.35f * s, lerpf(c.top[e.side] - 1.6f * s, 0.f, u * u) + 1.5f * s),
                           V(xo + 0.35f * s, lerpf(c.top[e.side] - 1.6f * s, 0.f, u * u) + 2.3f * s)}, 0.5f, 0, 0.6f);
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
        const int how = e.seq.empty() ? CR_HOLE : e.seq[0];
        if (how == CR_CANNON) { epCannon(f, e, t, c, pen); break; }
        if (how == CR_PLANE) { epPlane(f, e, t, c, pen); break; }
        if (how == CR_BALLOONS) { epBalloons(f, e, t, c, pen); break; }
        if (how == CR_ROCKET) { epRocket(f, e, t, c, pen); break; }
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
        const int how = e.seq.empty() ? 0 : e.seq[0];
        const bool sleep = how == 1;
        Skel rest;
        if (sleep) rest = sleeping(t);
        else {
            Ang a = sit(t);
            if (how == 2) { // a book up in front of him
                a.ua[0] = 0.75f; a.fa[0] = 2.1f; a.ua[1] = 0.55f; a.fa[1] = 1.95f;
                a.head = 0.35f;
            }
            rest = fk(a);
            onGround(rest);
        }
        if (t < 1.f) f.S = mixS(standing(), rest, ease(t));
        else if (t < dur - 1.f) f.S = rest;
        else f.S = mixS(rest, standing(), ease(t - (dur - 1.f)));
        if (pen && how == 2 && t >= 1.f && t < dur - 1.f) { // the book open, a page turned each bar
            const vec2 b = (jointAt(f, HA0, s) + jointAt(f, HA1, s)) * 0.5f + V((float)e.dir * 0.05f * s, 0.12f * s);
            const float d = (float)e.dir;
            const Poly cover = {b + V(-d * 0.05f * s, -0.22f * s), b + V(d * 0.25f * s, -0.12f * s), b + V(d * 0.25f * s, 0.22f * s),
                                b + V(-d * 0.05f * s, 0.12f * s), b + V(-d * 0.05f * s, -0.22f * s)};
            pen->shape(cover, 0.7f);
            pen->colour(cover, hue(pen->P, (int)(e.id % 6)), 0.5f, 0.25f);
            const float u = frac((t - 1.f) / 4.f) * 4.f;
            if (u < 1.f) pen->line(quad(b + V(-d * 0.05f * s, 0.12f * s), b + V(d * lerpf(0.2f, -0.2f, u) * s, (0.2f + 0.15f * std::sin(u * PI)) * s), b + V(d * lerpf(0.25f, -0.3f, u) * s, 0.22f * s), 6), 0.5f);
        }
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
    case K_SKATE: epSkate(f, e, t, c, pen); break;
    case K_POGO: epPogo(f, e, t, c, pen); break;
    case K_BIKE: epBike(f, e, t, c, pen); break;
    case K_ROPE: epRope(f, e, t, c, pen); break;
    case K_TRAMP: epTramp(f, e, t, c, pen); break;
    case K_FOOTBALL: epFootball(f, e, t, c, pen); break;
    case K_BASKET: epBasket(f, e, t, c, pen); break;
    case K_BEES: epBees(f, e, t, c, pen); break;
    case K_BANANA: epBanana(f, e, t, c, pen); break;
    case K_UMBRELLA: epUmbrella(f, e, t, c, pen); break;
    case K_KITE: epKite(f, e, t, c, pen); break;
    case K_FISH: epFish(f, e, t, c, pen); break;
    case K_JUGGLE: epJuggle(f, e, t, c, pen); break;
    case K_MUSIC: epMusic(f, e, t, c, pen); break;
    case K_YOGA: epYoga(f, e, t, c, pen); break;
    case K_STARS: epStars(f, e, t, c, pen); break;
    case K_PAINT: epPaint(f, e, t, c, pen); break;
    case K_DOG: epDog(f, e, t, c, pen); break;
    default: break;
    }
    return f;
}

// The guy drawn: his head, his body, his arms and legs, his scarf trailing behind him (`vel`: how his neck moves,
// board units a beat).
void drawGuy(Pen& pen, const Fig& f, float s0, vec2 vel, float flutter, float wk) {
    if (!f.shown) return;
    const Pal& P = pen.P;
    auto W = [&](int j) { return jointAt(f, j, s0); };
    const float s = s0 * f.k;
    wk *= 0.4f + 0.6f * f.k;
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
    case PR_TRAMP: {
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        const float x = p.xa, hw = 1.15f * s, y = 0.55f * s;
        pen.line({V(x - hw, y), V(x - hw * 0.85f, 0.f)}, 0.9f);
        pen.line({V(x + hw, y), V(x + hw * 0.85f, 0.f)}, 0.9f);
        pen.line({V(x - 0.2f * s, y - 0.12f * s), V(x - 0.3f * s, 0.f)}, 0.7f);
        pen.line({V(x + 0.2f * s, y - 0.12f * s), V(x + 0.3f * s, 0.f)}, 0.7f);
        const Poly rim = rect(x - hw - 0.08f * s, y - 0.12f * s, x + hw + 0.08f * s, y);
        pen.shape(rim, 0.8f);
        pen.colour(rim, hue(P, p.var), 0.f, 0.5f * s);
        break;
    }
    case PR_GOAL: { // side on: the posts, the bar, the net behind
        pen.begin(p.id, (float)p.t0, 14.f, 0.3f);
        const float x = p.xa, d = (float)p.dir, h = 1.25f * s, back = x + d * 0.75f * s;
        pen.line({V(x, 0.f), V(x, h), V(back, h * 0.85f), V(back, 0.f)}, 1.1f);
        for (int k = 1; k < 5; k++) {
            const float u = k / 5.f;
            pen.line({V(x, h * u), V(back, h * 0.85f * u)}, 0.4f, P.grey);
            pen.line({V(lerpf(x, back, u), 0.f), V(lerpf(x, back, u), h * (1.f - 0.15f * u))}, 0.4f, P.grey);
        }
        break;
    }
    case PR_HOOP: { // a post, a board, the rim and its net
        pen.begin(p.id, (float)p.t0, 14.f, 0.3f);
        const float x = p.xa, d = (float)p.dir;
        pen.line({V(x, 0.f), V(x, 2.9f * s), V(x - d * 0.2f * s, 3.05f * s)}, 1.2f);
        const Poly board = rect(std::min(x - d * 0.2f * s, x - d * 0.28f * s), 2.45f * s, std::max(x - d * 0.2f * s, x - d * 0.28f * s), 3.5f * s);
        pen.shape(board, 0.8f);
        const vec2 rim = V(x - d * 0.55f * s, 2.6f * s);
        pen.shape(ellipse(rim, 0.3f * s, 0.07f * s), 1.1f, P.orange);
        for (int k = 0; k <= 4; k++) {
            const float u = k / 4.f;
            pen.line({rim + V((-0.3f + 0.6f * u) * s, 0.f), rim + V((-0.18f + 0.36f * u) * s, -0.45f * s)}, 0.4f, P.grey);
        }
        pen.line({rim + V(-0.24f * s, -0.22f * s), rim + V(0.24f * s, -0.22f * s)}, 0.4f, P.grey);
        break;
    }
    case PR_POND: {
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        const Poly w = ellipse(V(p.xa, 0.f), 1.05f * s, 0.24f * s);
        pen.shape(w, 0.8f);
        pen.colour(w, P.blue, 0.1f, 0.6f * s);
        for (int k = 0; k < 3; k++) { // reeds
            const float x = p.xa + (float)p.dir * (0.85f + 0.12f * k) * s;
            pen.line({V(x, 0.f), V(x + 0.05f * s, (0.7f + 0.15f * k) * s)}, 0.6f, P.green);
        }
        break;
    }
    case PR_PEEL: {
        pen.begin(p.id, (float)p.t0, 8.f, 0.3f);
        const float x = p.xa;
        const Poly peel = {V(x - 0.3f * s, 0.f), V(x - 0.18f * s, 0.12f * s), V(x, 0.08f * s), V(x + 0.18f * s, 0.12f * s), V(x + 0.3f * s, 0.f), V(x - 0.3f * s, 0.f)};
        pen.shape(peel, 0.8f);
        pen.colour(peel, P.yellow, 0.3f, 0.3f);
        pen.line({V(x, 0.08f * s), V(x + 0.05f * s, 0.2f * s)}, 0.7f);
        break;
    }
    case PR_CLOUD: {
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        Poly cl;
        const float x = p.xa, y = p.h;
        for (int i = 0; i <= 40; i++) {
            const float a = i / 40.f * TAU;
            const float bump = 1.f + 0.22f * std::fabs(std::sin(a * 3.f));
            cl.push_back(V(x + std::cos(a) * 1.5f * s * bump, y + std::sin(a) * 0.5f * s * bump));
        }
        pen.shape(cl, 0.9f);
        pen.colour(cl, P.grey, 0.3f, 0.6f * s, 0.6f);
        break;
    }
    case PR_SKY: { // the moon (or a ringed planet) and a few stars, slowly
        pen.begin(p.id, (float)p.t0, 4.f, 0.3f);
        const float x = p.xa, y = p.h;
        if (p.var == 0) {
            const Poly moon = {V(x, y + 0.5f * s), V(x - 0.35f * s, y + 0.3f * s), V(x - 0.45f * s, y), V(x - 0.35f * s, y - 0.3f * s),
                               V(x, y - 0.5f * s), V(x - 0.15f * s, y - 0.25f * s), V(x - 0.2f * s, y), V(x - 0.15f * s, y + 0.25f * s), V(x, y + 0.5f * s)};
            pen.shape(moon, 0.8f);
            pen.colour(moon, P.yellow, 0.4f, 0.3f);
        } else {
            const Poly pl = ellipse(V(x, y), 0.35f * s, 0.35f * s);
            pen.shape(pl, 0.8f);
            pen.colour(pl, P.orange, 0.4f, 0.4f);
            pen.line(arc(V(x, y), 0.65f * s, 0.15f * s, PI * 0.95f, TAU + PI * 0.05f, 16), 0.8f);
        }
        Rng r(mixk(p.id, 5));
        for (int k = 0; k < 5; k++)
            starShape(pen, V(x + r.range(-2.2f, 2.2f) * s * (k & 1 ? 1.f : -0.6f), y + r.range(-1.5f, 1.f) * s), r.range(0.1f, 0.18f) * s, P.yellow, r.range(0.f, 1.f));
        break;
    }
    case PR_SCOPE: { // a telescope on its tripod, pointing up
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        const float x = p.xa, d = (float)p.dir;
        const vec2 top = V(x, 1.15f * s);
        pen.line({top, V(x - 0.35f * s, 0.f)}, 0.8f);
        pen.line({top, V(x + 0.35f * s, 0.f)}, 0.8f);
        pen.line({top, V(x + 0.05f * s, 0.f)}, 0.6f);
        const vec2 ax = V(d * std::cos(0.75f), std::sin(0.75f)), nx = V(-ax.y, ax.x);
        const vec2 a = top - ax * (0.45f * s), b = top + ax * (0.8f * s);
        const Poly tube = {a + nx * (0.08f * s), b + nx * (0.13f * s), b - nx * (0.13f * s), a - nx * (0.08f * s), a + nx * (0.08f * s)};
        pen.shape(tube, 0.9f);
        pen.colour(tube, hue(P, p.var), 0.75f * d, 0.4f);
        break;
    }
    case PR_EASEL: { // three legs and a canvas
        pen.begin(p.id, (float)p.t0, 14.f, 0.3f);
        const float x = p.xa;
        pen.line({V(x - 0.45f * s, 0.f), V(x, 2.1f * s)}, 0.9f);
        pen.line({V(x + 0.45f * s, 0.f), V(x, 2.1f * s)}, 0.9f);
        pen.line({V(x + 0.1f * s, 0.f), V(x, 1.9f * s)}, 0.6f);
        pen.shape(rect(x - 0.65f * s, 0.85f * s, x + 0.65f * s, 1.95f * s), 0.9f);
        pen.line({V(x - 0.7f * s, 0.82f * s), V(x + 0.7f * s, 0.82f * s)}, 0.9f);
        break;
    }
    case PR_PICTURE: { // what he paints, slowly, then held up a while
        pen.begin(p.id, (float)p.t0, 1.f, 0.3f);
        const size_t from = pen.out.size();
        picture(pen, p.var, V(p.xa, 1.4f * s), s);
        // (its strokes spread over the time he paints)
        float t1 = (float)p.t0;
        for (size_t i = from; i < pen.out.size(); i++) t1 = std::max(t1, pen.out[i].t0 + pen.out[i].dur);
        const float k = p.h / std::max(0.1f, t1 - (float)p.t0);
        for (size_t i = from; i < pen.out.size(); i++) {
            pen.out[i].t0 = (float)p.t0 + (pen.out[i].t0 - (float)p.t0) * k;
            pen.out[i].dur *= k;
        }
        break;
    }
    case PR_CANNON: {
        pen.begin(p.id, (float)p.t0, 12.f, 0.3f);
        const float d = (float)p.dir, ang = 0.85f;
        const vec2 piv = V(p.xa, 0.5f * s), ax = V(d * std::cos(ang), std::sin(ang)), nx = V(-ax.y, ax.x);
        const vec2 a = piv - ax * (0.55f * s), b = piv + ax * (1.35f * s);
        const Poly barrel = {a + nx * (0.3f * s), b + nx * (0.25f * s), b - nx * (0.25f * s), a - nx * (0.3f * s), a + nx * (0.3f * s)};
        pen.shape(barrel, 1.f);
        pen.dark(barrel, 0.4f, 0.6f * s);
        pen.shape(ellipse(b, 0.1f * s, 0.27f * s), 0.7f);
        const Poly wheel = ellipse(piv + V(0.f, -0.12f * s), 0.38f * s, 0.38f * s);
        pen.shape(wheel, 0.9f);
        pen.colour(wheel, P.card, 0.f, 0.4f);
        for (int k = 0; k < 3; k++) {
            const vec2 dd = rot(V(0.36f * s, 0.f), k * PI / 3.f);
            pen.line({piv + V(0.f, -0.12f * s) - dd, piv + V(0.f, -0.12f * s) + dd}, 0.5f);
        }
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
    used_.assign(K_COUNT, 0);
    varUsed_.assign(K_COUNT * 8, 0);
    logged_ = 0;
    // (test hook: REFALL_NOTEPAD_EPISODES="kite,cross:cannon,..." shows these first, each as soon as it can be)
    forced_.clear();
    if (const char* v = std::getenv("REFALL_NOTEPAD_EPISODES")) {
        std::string all = v;
        for (size_t a = 0; a < all.size();) {
            size_t b = all.find(',', a);
            if (b == std::string::npos) b = all.size();
            const std::string item = all.substr(a, b - a), kind = item.substr(0, item.find(':'));
            const std::string var = item.find(':') == std::string::npos ? "" : item.substr(item.find(':') + 1);
            for (int k = 0; k < K_COUNT; k++) {
                if (kind != KIND_NAMES[k]) continue;
                int vi = 15;
                for (int i = 0; i < NVAR[k]; i++) if (var == VARIANTS[k][i]) vi = i;
                forced_.push_back(k * 16 + vi);
            }
            a = b + 1;
        }
    }
}

// What he does next, out of what he can do from here (`w`, how well each fits the music; the crossing's is the
// chance he crosses now): what he has done least this song first, so that nothing comes back before the rest
// has had its turn (a deck of gags, refilled as it runs out, the music choosing among what is left); seldom
// what he did lately, never the same twice running. Walks and waves fill in between, as they come.
int Story::choose(const float w[]) {
    for (size_t i = 0; i < forced_.size(); i++) // (the test hook's first one that can be, to the front)
        if (w[forced_[i] / 16] > 0.f) {
            std::rotate(forced_.begin(), forced_.begin() + (long)i, forced_.begin() + (long)i + 1);
            return forced_.front() / 16;
        }
    auto dealt = [](int k) { return k != K_RUN && k != K_WAVE && k != K_CROSS; };
    int least = 1 << 30;
    for (int k = 0; k < K_COUNT; k++)
        if (dealt(k) && w[k] > 0.f) least = std::min(least, used_[k]);
    float v[K_COUNT];
    for (int k = 0; k < K_COUNT; k++) {
        v[k] = k == K_CROSS ? 0.f : w[k];
        if (dealt(k) && used_[k] > least) v[k] *= std::pow(0.02f, (float)(used_[k] - least));
    }
    for (int i = 1; i < 4; i++)
        if (at_.recent[i] >= 0) v[at_.recent[i]] *= 0.3f;
    if (at_.recent[0] >= 0) v[at_.recent[0]] = 0.f;
    float sum = 0.f;
    for (int k = 0; k < K_COUNT; k++) sum += v[k];
    if (w[K_CROSS] > 0.f) v[K_CROSS] = std::max(sum, 1.f) * w[K_CROSS] / (1.f - w[K_CROSS]);
    if (sum <= 0.f && v[K_CROSS] <= 0.f) return K_WAVE;
    return rng_.weighted(v);
}

// Its variant, the same way: the one seen least (`w`: which can be, and how likely).
int Story::variant(int kind, const float w[], int n) {
    int least = 1 << 30;
    for (int i = 0; i < n; i++) if (w[i] > 0.f) least = std::min(least, varUsed_[kind * 8 + i]);
    if (least == 1 << 30) return 0;
    float v[8] = {};
    for (int i = 0; i < n; i++) v[i] = w[i] > 0.f && varUsed_[kind * 8 + i] == least ? w[i] : 0.f;
    const int r = rng_.weighted(v);
    varUsed_[kind * 8 + r]++;
    return r;
}

void Story::log(double beat) {
    static const bool on = std::getenv("REFALL_NOTEPAD_STORY_LOG") != nullptr;
    if (!on) return;
    for (const Ep& e : eps_) {
        if (beat < e.b0 || beat >= e.b1 || e.id == logged_) continue;
        logged_ = e.id;
        const int v = e.seq.empty() ? 0 : std::clamp(e.seq[0], 0, NVAR[e.kind] - 1);
        std::fprintf(stderr, "[story] beat %.1f  %s:%s  (%.0f beats, margin %d)\n", e.b0, KIND_NAMES[e.kind],
                     VARIANTS[e.kind][e.kind == K_ACRO ? 0 : v], e.b1 - e.b0, e.side);
    }
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
        // what he can do from here, and how well it fits the music (in a break, the verse, the chorus)
        float w[K_COUNT] = {};
        if (ledge) {
            w[K_CHUTE] = 3.f;
            w[K_DIVE] = stage >= 1 ? 2.f : 0.6f;
            w[K_HOP] = 2.f;
            if (!fits(at_.lb + 2.4f * s) && !fits(at_.la - 2.4f * s)) w[K_DIVE] = 0.f;
        } else {
            static const float FIT[K_COUNT][3] = {
                {0.3f, 0.3f, 0.3f}, {0.8f, 0.8f, 0.8f}, {1.f, 1.5f, 1.5f}, {1.f, 1.5f, 1.5f}, // wave, run, hop, ladder
                {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.6f, 2.f, 2.f}, {0.4f, 1.5f, 2.5f},      // chute, dive, dodge, acro
                {0.f, 0.f, 0.f}, {3.f, 0.2f, 0.f},                                         // cross (below), rest
                {0.3f, 2.f, 2.5f}, {0.3f, 1.5f, 2.5f}, {0.8f, 2.f, 2.f}, {0.2f, 1.5f, 2.5f},  // skate, pogo, bike, rope
                {0.3f, 1.5f, 2.5f}, {0.5f, 2.f, 2.f}, {0.5f, 2.f, 2.f}, {0.2f, 1.5f, 2.5f},   // trampoline, football, basket, bees
                {1.5f, 1.5f, 1.f}, {2.f, 1.2f, 0.4f}, {2.5f, 1.f, 0.3f}, {3.f, 0.6f, 0.f},    // banana, umbrella, kite, fish
                {1.5f, 2.f, 1.f}, {2.5f, 1.5f, 0.6f}, {3.f, 0.3f, 0.f}, {3.f, 0.3f, 0.f},     // juggle, music, yoga, stars
                {2.5f, 1.f, 0.2f}, {1.5f, 2.f, 1.f},                                       // paint, dog
            };
            const float span = hi - lo, room = std::max(hi - x, x - lo);
            for (int k = 0; k < K_COUNT; k++) w[k] = FIT[k][std::min(stage, 2)];
            auto need = [&](int k, bool ok) { if (!ok) w[k] = 0.f; };
            // (what wants more room than he has on either side here: a walk to the end of the margin first)
            float blocked = 0.f;
            auto roomFor = [&](int k, float r) {
                if (room > r * s) return;
                blocked += w[k] * (used_[k] == 0 ? 1.f : 0.2f);
                w[k] = 0.f;
            };
            need(K_RUN, span > 2.5f * s);
            need(K_HOP, H > 8.f * s);
            need(K_LADDER, H > 9.f * s && span > 2.8f * s);
            need(K_DODGE, span > 2.8f * s);
            need(K_TRAMP, H > 8.f * s);
            need(K_BASKET, H > 6.f * s);
            need(K_BEES, span > 3.f * s);
            need(K_UMBRELLA, H > 7.f * s);
            need(K_KITE, H > 8.f * s);
            need(K_STARS, H > 8.f * s);
            need(K_DOG, span > 3.5f * s);
            roomFor(K_SKATE, 3.4f);
            roomFor(K_POGO, 3.f);
            roomFor(K_BIKE, 3.4f);
            roomFor(K_TRAMP, 3.3f);
            roomFor(K_FOOTBALL, 2.8f);
            roomFor(K_BASKET, 2.6f);
            roomFor(K_BANANA, 2.6f);
            roomFor(K_FISH, 3.1f);
            roomFor(K_STARS, 1.4f);
            roomFor(K_PAINT, 1.6f);
            if (w[K_RUN] > 0.f) w[K_RUN] += 0.4f * blocked;
            // over to the other margin: the more likely the longer he has been in this one
            static const float CROSS[6] = {0.f, 0.f, 0.2f, 0.35f, 0.55f, 0.8f};
            w[K_CROSS] = CROSS[std::clamp(at_.sinceCross, 0, 5)];
        }
        e.kind = choose(w);
        int forcedVar = -1;
        if (!forced_.empty() && forced_.front() / 16 == e.kind) {
            forcedVar = forced_.front() % 16;
            forced_.erase(forced_.begin());
        }
        auto pick = [&](std::initializer_list<float> wv) {
            float v[8] = {};
            int n = 0;
            for (float x : wv) v[n++] = x;
            if (forcedVar >= 0 && forcedVar < n && v[forcedVar] > 0.f) { varUsed_[e.kind * 8 + forcedVar]++; return forcedVar; }
            return variant(e.kind, v, n);
        };
        // which way has more room
        const int roomy = (hi - x) >= (x - lo) ? 1 : -1;
        auto propAt = [&](int kind, float xa, double t0, double t1, int dir, int var, float h = 0.f) {
            Prop p;
            p.kind = kind;
            p.side = side;
            p.xa = xa;
            p.h = h;
            p.t0 = t0;
            p.t1 = t1;
            p.dir = dir;
            p.var = var;
            p.id = serial_++;
            props_.push_back(p);
        };
        auto colour = [&]() { return rng_.irange(0, 5); };
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
            e.seq.push_back(pick({1.f, stage >= 1 ? 1.f : 0.3f, 0.8f, stage >= 2 ? 0.f : 0.8f}));
            float tx = x < 0.f ? rng_.range(std::max(lo, hi * 0.3f), hi) : rng_.range(lo, std::min(hi, lo * 0.3f));
            if (std::fabs(tx - x) < 1.5f * s) tx = x < 0.f ? hi : lo;
            const float dist = std::fabs(tx - x);
            const float stepLen = (e.seq[0] == RU_RUN && stage >= 1 ? 1.1f : 0.7f) * s;
            const float rate = e.seq[0] == RU_SKIP ? 1.f : stepBeat;
            const float d = std::max(2.f, std::round(dist / (stepLen * rate)));
            e.dir = tx >= x ? 1 : -1;
            e.p1 = V(tx, 0.f);
            e.rate = rate;
            e.h = std::clamp(dist / (d * rate) / (1.15f * s), 0.4f, 1.1f);
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
            const float maxH = std::min(H - 4.5f * s, (stage == 0 ? 4.6f : 6.5f) * s);
            float h = rng_.range(4.f * s, std::max(4.1f * s, maxH));
            h = std::max(2.f, std::round(h / (RUNG * s))) * RUNG * s;
            int d = e.dir;
            float lx = x + d * 0.9f * s;
            if (!fits(lx + d * 2.4f * s) || !fits(lx)) { d = -d; lx = x + d * 0.9f * s; }
            if (!fits(lx + d * 2.4f * s) || !fits(lx)) { d = x < 0.f ? 1 : -1; lx = std::clamp(x, lo, hi); }
            e.dir = d;
            e.ea = lx;
            e.h = h;
            e.rate = 2.f;
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
                const float D = std::clamp(std::ceil((y - 1.f * s) / (1.15f * s)), 3.f, stage == 0 ? 4.f : 6.f);
                e.b1 = e.b0 + 4.0 + D;
                e.seq.push_back(pick({1.f, 1.f}));
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
            const int first = pick({1.f, 1.f, 1.f, 1.f});
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
            const int other = 1 - side, ws = side == 0 ? 1 : -1; // (toward the well)
            const float hw2 = 0.5f * (P[other].x1 - P[other].x0);
            const int how = pick({1.f, stage >= 1 ? 1.5f : 0.4f, fits(x + ws * 1.4f * s) ? 1.f : 0.f,
                                  H > 9.f * s ? (stage == 0 ? 2.f : 0.5f) : 0.f, stage >= 1 ? 1.5f : 0.4f});
            static const float FLIP[5] = {3.f, 6.f, 5.6f, 6.6f, 6.2f}, DUR[5] = {6.f, 10.f, 10.f, 12.f, 10.f};
            e.seq = {how, colour()};
            e.flip = FLIP[how];
            e.b1 = e.b0 + DUR[how];
            e.p1 = V(rng_.range(-0.45f, 0.45f) * std::max(0.f, hw2 - m), 0.f);
            if (how == CR_HOLE) { // a hole thrown down in front of him, another one over there
                int d = e.dir;
                if (!fits(x + d * 1.1f * s)) d = -d;
                e.dir = d;
                e.ea = x + d * 1.1f * s;
                propAt(PR_HOLE, e.ea, e.b0 + 0.3, e.b0 + 3.5, d, 0);
                Prop b = props_.back();
                b.side = other;
                b.xa = e.p1.x;
                b.t0 = e.b0 + 2.6;
                b.t1 = e.b0 + 6.8;
                b.id = serial_++;
                props_.push_back(b);
            } else {
                e.dir = ws;
                if (how == CR_CANNON) {
                    e.ea = std::clamp(x + ws * 0.3f * s, -hw + 1.5f * s, hw - 1.5f * s);
                    propAt(PR_CANNON, e.ea, e.b0 - 1.0, e.b0 + 6.5, ws, 0);
                }
            }
            if (Prop* g = prop(ground_)) g->t1 = e.b0 + e.flip + 1.0;
            Prop g;
            g.kind = PR_GROUND;
            g.side = other;
            g.t0 = e.b0 + e.flip - 1.0;
            g.id = serial_++;
            ground_ = g.id;
            props_.push_back(g);
            at_.side = other;
            at_.sinceCross = -1;
            break;
        }
        case K_REST: {
            e.seq.push_back(pick({1.f, stage == 0 ? 1.5f : 0.f, 1.f}));
            e.p1 = e.p0;
            e.b1 = e.b0 + 8.0;
            break;
        }
        case K_SKATE:
        case K_BIKE:
        case K_POGO: { // across the margin, the way there is room
            const int d = roomy;
            const float far = d > 0 ? hi : lo;
            const float len = std::min(std::fabs(far - x), rng_.range(3.2f, 5.5f) * s);
            e.dir = d;
            e.p1 = V(x + d * len, 0.f);
            if (e.kind == K_SKATE) e.seq = {pick({1.f, 1.f}), colour()};
            else if (e.kind == K_BIKE) e.seq = {pick({1.f, stage >= 1 ? 1.f : 0.5f, 1.f}), colour()};
            else e.seq = {pick({1.f, 1.f}), colour()};
            e.b1 = e.b0 + 8.0;
            break;
        }
        case K_ROPE:
        case K_JUGGLE:
        case K_YOGA: { // where he is
            e.p1 = e.p0;
            if (e.kind == K_ROPE) e.seq = {pick({1.f, 1.f}), colour()};
            else if (e.kind == K_JUGGLE) e.seq = {pick({1.f, 1.f, 1.f}), colour()};
            else e.seq = {pick({1.f, 1.f}), colour()};
            e.b1 = e.b0 + (e.kind == K_YOGA ? 12.0 : 8.0);
            break;
        }
        case K_MUSIC: {
            const bool kit = fits(x + roomy * 1.6f * s);
            e.dir = roomy;
            e.seq = {pick({1.f, kit && stage >= 1 ? 1.5f : kit ? 0.5f : 0.f}), colour()};
            e.p1 = e.p0;
            e.b1 = e.b0 + (stage >= 1 ? 8.0 : 12.0);
            break;
        }
        case K_TRAMP: {
            const int d = roomy;
            e.dir = d;
            e.ea = x + d * 1.6f * s;
            e.p1 = V(e.ea + d * 1.7f * s, 0.f);
            e.seq = {pick({1.f, 1.f})};
            e.b1 = e.b0 + 12.0;
            propAt(PR_TRAMP, e.ea, e.b0 - 1.0, e.b1 + 0.3, d, colour());
            break;
        }
        case K_FOOTBALL: {
            const int d = roomy;
            e.dir = d;
            e.ea = d > 0 ? hi : lo;         // the goal
            e.eb = e.ea - d * 2.3f * s;    // where he kicks from
            if (d * (e.eb - x) < 0.f) e.eb = x;
            e.p1 = V(e.eb - d * 0.05f * s, 0.f);
            e.seq = {pick({2.f, 1.f})};
            e.b1 = e.b0 + 10.0;
            propAt(PR_GOAL, e.ea, e.b0 - 1.5, e.b1 + 0.3, d, 0);
            break;
        }
        case K_BASKET: {
            const int d = roomy;
            e.dir = d;
            e.ea = (d > 0 ? hi : lo) + d * 0.2f * s; // the post
            e.eb = e.ea - d * 2.3f * s;               // where he shoots from
            if (d * (e.eb - x) < 0.f) e.eb = x;
            e.seq = {pick({1.f, stage >= 1 ? 1.f : 0.4f})};
            e.p1 = V(e.seq[0] == 1 ? e.ea - d * 0.8f * s : e.eb, 0.f);
            e.b1 = e.b0 + 10.0;
            propAt(PR_HOOP, e.ea, e.b0 - 1.5, e.b1 + 0.3, d, 0);
            break;
        }
        case K_BEES: {
            const int d = roomy;
            e.dir = d;
            e.ea = d > 0 ? hi : lo;
            e.p1 = V(lerpf(e.ea, d > 0 ? lo : hi, rng_.range(0.6f, 0.9f)), 0.f);
            e.seq = {rng_.irange(0, 3)};
            e.b1 = e.b0 + 10.0;
            break;
        }
        case K_BANANA: {
            const int d = roomy;
            e.dir = d;
            e.ea = x + d * 2.2f * s;
            e.p1 = V(e.ea + d * 0.3f * s, 0.f);
            e.b1 = e.b0 + 8.0;
            propAt(PR_PEEL, e.ea, e.b0 - 0.5, e.b0 + 3.0, d, 0);
            break;
        }
        case K_UMBRELLA: {
            e.h = std::min(H - 1.2f * s, 4.4f * s);
            e.seq = {pick({1.f, 1.f}), colour()};
            e.p1 = e.p0;
            e.b1 = e.b0 + 12.0;
            propAt(PR_CLOUD, x, e.b0 - 0.3, e.b1 - 2.8, e.dir, 0, e.h);
            break;
        }
        case K_KITE: {
            const int d = x > 0.f ? -1 : 1; // the kite over the middle of the margin
            e.dir = d;
            e.pts = {V(std::clamp(x + d * 1.6f * s, -hw + 1.f * s, hw - 1.f * s), std::min(H - 2.f * s, 6.5f * s))};
            e.seq = {0, colour()};
            e.p1 = e.p0;
            e.b1 = e.b0 + 12.0;
            break;
        }
        case K_FISH: {
            const int d = roomy;
            e.dir = d;
            e.ea = x + d * 2.25f * s;
            e.p1 = V(e.ea - d * 1.25f * s, 0.f);
            e.seq = {pick({1.5f, 1.f, 1.f}), colour()};
            e.b1 = e.b0 + 12.0;
            propAt(PR_POND, e.ea, e.b0 - 1.0, e.b1 + 0.3, d, 0);
            break;
        }
        case K_STARS: {
            const int d = roomy;
            e.dir = d;
            const float sx = x + d * 0.95f * s;
            e.ea = sx - d * 0.33f * s;
            e.p1 = V(e.ea - d * 0.15f * s, 0.f);
            e.h = std::min(H - 1.6f * s, 6.f * s);
            const int what = pick({1.f, 1.f});
            e.seq = {what};
            e.b1 = e.b0 + 12.0;
            propAt(PR_SCOPE, sx, e.b0 - 1.0, e.b1 + 0.3, d, colour());
            propAt(PR_SKY, std::clamp(sx + d * 0.5f * s, -hw + 1.f * s, hw - 1.f * s), e.b0, e.b1 + 0.3, d, what, e.h);
            break;
        }
        case K_PAINT: {
            const int d = roomy;
            e.dir = d;
            e.ea = x + d * 1.2f * s;
            e.p1 = V(e.ea - d * 1.45f * s, 0.f);
            e.seq = {pick({1.f, 1.f, 1.f, 1.f, 1.f, 1.f})};
            e.b1 = e.b0 + 12.0;
            propAt(PR_EASEL, e.ea, e.b0 - 1.0, e.b1 + 0.5, d, 0);
            propAt(PR_PICTURE, e.ea, e.b0 + 1.5, e.b1 + 0.5, d, e.seq[0], (float)(e.b1 - e.b0) - 5.f);
            break;
        }
        case K_DOG: {
            const int d = roomy;
            e.dir = d;
            e.ea = x + d * std::min(std::fabs((d > 0 ? hi : lo) - x), 3.2f * s); // where the stick lands
            e.eb = d > 0 ? -hw + 0.5f * s : hw - 0.5f * s;                         // where the dog comes from
            e.seq = {pick({1.f, 1.f, 1.f})};
            e.p1 = e.p0;
            e.b1 = e.b0 + 12.0;
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
        used_[e.kind]++;
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
                if (beat >= e.b0 && beat < e.b1) side = (e.kind == K_CROSS && beat >= e.b0 + e.flip) ? 1 - e.side : e.side;
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
    log(beat);
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
        const Ctx c{s, {hw[0], hw[1]}, {H[0], H[1]}};
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
            const size_t from = out.size();
            pen.begin(ID_GUY + 1 + (e.id % 4096), -1e6f, 0.f, chalky * 0.03f);
            const Fig f = figure(e, t, c, &pen);
            const Fig g = figure(e, std::max(0.f, t - 0.12f), c, nullptr);
            const vec2 vel = g.side == f.side ? (jointAt(f, NECK, s) - jointAt(g, NECK, s)) * (1.f / 0.12f) : V(0.f, 0.f);
            place(from, f.side);
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

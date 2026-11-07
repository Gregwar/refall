// STAR CHART's figures: original line-art drawings in the manner of the old celestial atlases, engraved in fine
// strokes around the constellations: a serpent, a fish, a swan, a laurel crown, a lyre, a bow drawn with its arrow,
// a pair of scales, a ship and a scorpion. Each is built from a few curves (Béziers, arcs, ellipses), its outline
// doubled where the body has a width, with a little hatching for the shade; in a drop it comes alive (the serpent
// winds, the fish beats its tail, the swan its wings, the bow is drawn, the scales rock, the ship rides the swell,
// the scorpion lifts its tail), always slowly and around its stars, which never move.
#include "universes/starchart/figures.hpp"

#include <cmath>

namespace starchart {
namespace {

using Line = std::vector<vec2>;

vec2 V(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 sub(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 norm(vec2 a) { const float l = len(a); return l > 1e-6f ? mul(a, 1.f / l) : V(1, 0); }
vec2 perp(vec2 a) { return {-a.y, a.x}; }
vec2 rotAround(vec2 p, vec2 c, float a) {
    const float cs = std::cos(a), sn = std::sin(a);
    const vec2 d = sub(p, c);
    return {c.x + cs * d.x - sn * d.y, c.y + sn * d.x + cs * d.y};
}

vec2 bez(vec2 a, vec2 b, vec2 c, vec2 d, float t) {
    const float u = 1.f - t;
    return add(add(mul(a, u * u * u), mul(b, 3 * u * u * t)), add(mul(c, 3 * u * t * t), mul(d, t * t * t)));
}
Line bezier(vec2 a, vec2 b, vec2 c, vec2 d, int n = 20) {
    Line l;
    for (int i = 0; i <= n; i++) l.push_back(bez(a, b, c, d, (float)i / n));
    return l;
}
Line ellipse(vec2 c, float rx, float ry, float rot = 0.f, int n = 28, float a0 = 0.f, float a1 = TAU) {
    Line l;
    for (int i = 0; i <= n; i++) {
        const float a = a0 + (a1 - a0) * i / n;
        l.push_back(rotAround(V(c.x + rx * std::cos(a), c.y + ry * std::sin(a)), c, rot));
    }
    return l;
}
// The two sides of a body of width `w(s)` along a spine (s 0..1 along it).
template <class W>
void body(const Line& spine, W w, Line& left, Line& right) {
    const int n = (int)spine.size();
    for (int i = 0; i < n; i++) {
        const vec2 t = norm(sub(spine[std::min(n - 1, i + 1)], spine[std::max(0, i - 1)]));
        const float s = n > 1 ? (float)i / (n - 1) : 0.f;
        const vec2 o = mul(perp(t), w(s));
        left.push_back(add(spine[i], o));
        right.push_back(sub(spine[i], o));
    }
}
// A leaf (a pointed lens) from `a` to `b`, `bulge` of its length on each side.
Line leaf(vec2 a, vec2 b, float bulge) {
    const vec2 d = sub(b, a), n = mul(perp(d), bulge);
    Line l = bezier(a, add(add(a, mul(d, 0.3f)), n), add(add(a, mul(d, 0.7f)), n), b, 8);
    Line r = bezier(b, sub(add(a, mul(d, 0.7f)), n), sub(add(a, mul(d, 0.3f)), n), a, 8);
    l.insert(l.end(), r.begin() + 1, r.end());
    return l;
}

struct Out {
    std::vector<Line>& lines;
    std::vector<float>& weights;
    void operator()(const Line& l, float w = 1.f) { lines.push_back(l); weights.push_back(w); }
};

// ---------------------------------------------------------------- SERPENS
// A serpent winding across, its body tapering to the tail, belly scales hatched across it, its head raised.
void serpens(float life, float t, Out out) {
    Line spine;
    const int N = 44;
    for (int i = 0; i <= N; i++) {
        const float s = (float)i / N;
        const float wave = std::sin(s * 7.2f + 0.6f - life * t * 1.6f) * life * 0.07f;
        spine.push_back(V(-0.95f + 1.72f * s, 0.34f * std::sin(s * 6.6f + 0.4f) * (0.55f + 0.45f * s) + wave));
    }
    Line L, R;
    body(spine, [](float s) { return 0.012f + 0.075f * std::sqrt(std::min(1.f, s * 1.25f)); }, L, R);
    out(L);
    out(R);
    // the belly scales, crossing the body
    for (int i = 3; i < N - 2; i += 2) out({L[i], R[i]}, 0.55f);
    // the head
    const vec2 h = spine.back(), d = norm(sub(spine[N], spine[N - 3]));
    const vec2 hc = add(h, mul(d, 0.09f));
    const float ang = std::atan2(d.y, d.x);
    out(ellipse(hc, 0.13f, 0.075f, ang, 24));
    out(ellipse(add(hc, add(mul(d, 0.03f), mul(perp(d), 0.03f))), 0.016f, 0.016f, 0.f, 10), 0.8f);
    const vec2 tip = add(hc, mul(d, 0.13f));
    const float flick = 0.06f + 0.03f * std::sin(t * 9.f) * life;
    out({tip, add(tip, mul(d, 0.08f + flick))}, 0.7f);
    out({add(tip, mul(d, 0.08f + flick)), add(add(tip, mul(d, 0.14f + flick)), mul(perp(d), 0.03f))}, 0.7f);
    out({add(tip, mul(d, 0.08f + flick)), add(add(tip, mul(d, 0.14f + flick)), mul(perp(d), -0.03f))}, 0.7f);
}

// ---------------------------------------------------------------- PISCIS
// A fish: a lens of a body, the gill's arc, an eye, a dorsal fin, rows of scales and a forked tail that beats.
void piscis(float life, float t, Out out) {
    const vec2 nose = V(0.85f, 0.f), root = V(-0.58f, 0.f);
    const float beat = std::sin(t * 2.4f) * 0.22f * life;
    out(bezier(nose, V(0.45f, 0.58f), V(-0.25f, 0.42f), root));
    out(bezier(nose, V(0.45f, -0.5f), V(-0.25f, -0.36f), root));
    // tail
    const vec2 t1 = rotAround(V(-0.95f, 0.4f), root, beat), t2 = rotAround(V(-0.95f, -0.4f), root, beat);
    const vec2 notch = rotAround(V(-0.78f, 0.f), root, beat);
    out(bezier(root, rotAround(V(-0.7f, 0.12f), root, beat), rotAround(V(-0.85f, 0.3f), root, beat), t1, 10));
    out(bezier(t1, rotAround(V(-0.86f, 0.2f), root, beat), notch, notch, 8));
    out(bezier(root, rotAround(V(-0.7f, -0.12f), root, beat), rotAround(V(-0.85f, -0.3f), root, beat), t2, 10));
    out(bezier(t2, rotAround(V(-0.86f, -0.2f), root, beat), notch, notch, 8));
    for (int i = -2; i <= 2; i++)
        out({rotAround(V(-0.64f, i * 0.03f), root, beat), rotAround(V(-0.86f, i * 0.12f), root, beat)}, 0.45f);
    // eye, gill, dorsal fin
    out(ellipse(V(0.6f, 0.08f), 0.05f, 0.05f, 0.f, 14), 0.9f);
    out(bezier(V(0.42f, 0.3f), V(0.32f, 0.12f), V(0.32f, -0.1f), V(0.42f, -0.26f), 10), 0.8f);
    const float fin = std::sin(t * 1.7f + 1.f) * 0.04f * life;
    out(bezier(V(0.15f, 0.42f), V(0.f, 0.62f + fin), V(-0.2f, 0.6f + fin), V(-0.32f, 0.3f), 10), 0.85f);
    for (int i = 0; i < 4; i++) {
        const float x = 0.08f - i * 0.11f;
        out({V(x, 0.43f - i * 0.025f), V(x - 0.06f, 0.56f - i * 0.03f + fin)}, 0.4f);
    }
    // scales: rows of small arcs
    for (int row = -1; row <= 1; row++)
        for (int k = 0; k < 5; k++) {
            const float x = 0.24f - k * 0.15f + (row & 1) * 0.075f, y = row * 0.15f;
            if (std::fabs(y) > 0.3f - k * 0.04f) continue;
            out(ellipse(V(x, y), 0.07f, 0.075f, 0.f, 8, -1.3f, 1.3f), 0.4f);
        }
}

// ---------------------------------------------------------------- CYGNUS
// A swan in flight seen from below, a cross of stars: a long neck forward, the wings spread wide and beating
// slowly, their feathers fanned, a short tail.
void cygnus(float life, float t, Out out) {
    const float flap = std::sin(t * 1.9f) * life;
    out(ellipse(V(-0.18f, 0.f), 0.36f, 0.12f, 0.f, 30));
    // neck and head
    out(bezier(V(0.15f, 0.05f), V(0.4f, 0.08f), V(0.6f, 0.02f + flap * 0.02f), V(0.82f, 0.08f), 14));
    out(bezier(V(0.15f, -0.05f), V(0.42f, -0.03f), V(0.6f, -0.06f + flap * 0.02f), V(0.8f, 0.0f), 14));
    out(ellipse(V(0.86f, 0.05f), 0.07f, 0.045f, 0.2f, 14));
    out({V(0.92f, 0.07f), V(1.0f, 0.04f)}, 0.8f);
    // tail
    out({V(-0.52f, 0.05f), V(-0.72f, 0.1f), V(-0.68f, 0.f), V(-0.72f, -0.1f), V(-0.52f, -0.05f)}, 0.9f);
    // the wings: leading edge to the tip, the trailing edge scalloped by the feathers
    for (int side = -1; side <= 1; side += 2) {
        const float sy = (float)side;
        const float lift = 1.f - 0.16f * flap;
        const vec2 tip = V(-0.32f, sy * 0.9f * lift);
        const vec2 base0 = V(0.02f, sy * 0.08f), base1 = V(-0.3f, sy * 0.08f);
        out(bezier(base0, V(0.05f, sy * 0.4f * lift), V(-0.12f, sy * 0.72f * lift), tip, 16));
        // feathers: from the tip back to the body
        const int F = 6;
        vec2 prev = tip;
        for (int k = 1; k <= F; k++) {
            const float u = (float)k / F;
            const vec2 q = V(lerpf(tip.x, base1.x, u) - 0.06f * std::sin(u * PI), lerpf(tip.y, base1.y, u));
            const vec2 mid = add(mul(add(prev, q), 0.5f), V(-0.07f, 0.f));
            out(bezier(prev, mid, mid, q, 5), 0.85f);
            // the feather's quill toward the leading edge
            out({q, add(q, V(0.12f, sy * 0.06f))}, 0.45f);
            prev = q;
        }
    }
}

// ---------------------------------------------------------------- CORONA
// A laurel crown: two branches of paired leaves along an arc, closed by a ribbon at the bottom; its jewels are the
// stars and the leaves stir.
void corona(float life, float t, Out out) {
    const vec2 c = V(0.f, -0.3f);
    const float R = 0.78f;
    for (int side = -1; side <= 1; side += 2) {
        Line stem;
        for (int i = 0; i <= 24; i++) {
            const float a = -PI * 0.5f + side * (0.25f + 2.55f * i / 24.f);
            stem.push_back(V(c.x + R * std::cos(a), c.y + R * std::sin(a)));
        }
        out(stem, 0.9f);
        for (int i = 2; i <= 22; i += 3) {
            const float a = -PI * 0.5f + side * (0.25f + 2.55f * i / 24.f);
            const vec2 p = V(c.x + R * std::cos(a), c.y + R * std::sin(a));
            const vec2 tg = V(-std::sin(a) * side, std::cos(a) * side), nr = V(std::cos(a), std::sin(a));
            const float stir = std::sin(t * 2.f + i * 0.7f) * 0.12f * life;
            for (int k = -1; k <= 1; k += 2) {
                const vec2 dir = norm(add(mul(tg, 0.9f), mul(nr, k * (0.75f + stir))));
                out(leaf(p, add(p, mul(dir, 0.2f)), 0.22f), 0.8f);
            }
        }
    }
    // the ribbon
    const vec2 b = V(c.x, c.y - R);
    out(ellipse(b, 0.06f, 0.04f, 0.f, 10), 0.9f);
    const float wave = std::sin(t * 1.5f) * 0.05f * life;
    out(bezier(b, V(-0.12f, b.y - 0.1f), V(-0.2f, b.y - 0.05f + wave), V(-0.3f, b.y - 0.22f + wave), 10), 0.85f);
    out(bezier(b, V(0.12f, b.y - 0.1f), V(0.2f, b.y - 0.05f - wave), V(0.3f, b.y - 0.22f - wave), 10), 0.85f);
}

// ---------------------------------------------------------------- LYRA
// A lyre: a rounded sound box, two horned arms ending in scrolls, a crossbar and five strings that tremble.
void lyra(float life, float t, Out out) {
    for (int side = -1; side <= 1; side += 2) {
        const float s = (float)side;
        out(bezier(V(0.f, -0.74f), V(s * 0.3f, -0.74f), V(s * 0.38f, -0.5f), V(s * 0.34f, -0.3f), 12));
        out(bezier(V(s * 0.34f, -0.3f), V(s * 0.3f, -0.05f), V(s * 0.72f, 0.2f), V(s * 0.52f, 0.62f), 16));
        out(bezier(V(s * 0.2f, -0.3f), V(s * 0.18f, -0.02f), V(s * 0.56f, 0.24f), V(s * 0.4f, 0.6f), 16), 0.7f);
        // the scroll at the tip of the arm
        Line sc;
        for (int i = 0; i <= 20; i++) {
            const float u = i / 20.f, a = PI * (0.1f + 1.6f * u) * (side > 0 ? 1.f : -1.f) + (side > 0 ? 0.f : PI);
            const float r = 0.12f * (1.f - 0.65f * u);
            sc.push_back(V(s * 0.44f + r * std::cos(a), 0.7f + r * std::sin(a)));
        }
        out(sc, 0.9f);
    }
    out({V(-0.5f, 0.5f), V(0.5f, 0.5f)});
    out({V(-0.5f, 0.45f), V(0.5f, 0.45f)}, 0.6f);
    out({V(-0.24f, -0.42f), V(0.24f, -0.42f)}, 0.9f);
    for (int i = 0; i < 5; i++) {
        const float x = -0.18f + i * 0.09f;
        Line st;
        for (int k = 0; k <= 8; k++) {
            const float u = k / 8.f;
            const float wob = std::sin(PI * u) * std::sin(t * 13.f + i * 1.7f) * 0.018f * life;
            st.push_back(V(x + wob, lerpf(-0.42f, 0.45f, u)));
        }
        out(st, 0.55f);
    }
    // the sound hole
    out(ellipse(V(0.f, -0.58f), 0.07f, 0.07f, 0.f, 16), 0.7f);
}

// ---------------------------------------------------------------- ARCUS
// A bow drawn with its arrow: a recurved bow, the string pulled back to the nock, the arrow's head and fletching;
// in a drop the bow is drawn and eased back.
void arcus(float life, float t, Out out) {
    const float draw = life * (0.5f + 0.5f * std::sin(t * 1.4f)) * 0.14f;
    const vec2 tip1 = V(0.02f, 0.9f), tip2 = V(0.02f, -0.9f), nock = V(-0.45f - draw, 0.f);
    out(bezier(tip1, V(0.18f, 0.72f), V(0.5f, 0.42f), V(0.48f, 0.f), 16));
    out(bezier(V(0.48f, 0.f), V(0.5f, -0.42f), V(0.18f, -0.72f), tip2, 16));
    out(bezier(V(0.08f, 0.86f), V(0.24f, 0.66f), V(0.42f, 0.4f), V(0.41f, 0.f), 14), 0.6f);
    out(bezier(V(0.41f, 0.f), V(0.42f, -0.4f), V(0.24f, -0.66f), V(0.08f, -0.86f), 14), 0.6f);
    out({tip1, V(-0.04f, 0.98f)}, 0.8f);
    out({tip2, V(-0.04f, -0.98f)}, 0.8f);
    out({tip1, nock, tip2}, 0.6f);
    // the grip
    for (int i = -2; i <= 2; i++) out({V(0.42f, i * 0.03f), V(0.52f, i * 0.03f)}, 0.5f);
    // the arrow
    const float ax0 = nock.x - 0.05f, ax1 = 0.86f - draw;
    out({V(ax0, 0.f), V(ax1, 0.f)}, 0.9f);
    out({V(ax1 + 0.12f, 0.f), V(ax1, 0.06f), V(ax1 + 0.02f, 0.f), V(ax1, -0.06f), V(ax1 + 0.12f, 0.f)});
    for (int k = 0; k < 3; k++) {
        const float x = ax0 + 0.02f + k * 0.05f;
        out({V(x, 0.f), V(x - 0.08f, 0.08f)}, 0.6f);
        out({V(x, 0.f), V(x - 0.08f, -0.08f)}, 0.6f);
    }
}

// ---------------------------------------------------------------- LIBRA
// A pair of scales: a post on a stepped foot, a finial, the beam rocking on its pivot, the pans hanging from it.
void libra(float life, float t, Out out) {
    const float tilt = std::sin(t * 1.1f) * 0.14f * life;
    const vec2 piv = V(0.f, 0.45f);
    out({V(-0.03f, -0.78f), V(-0.03f, 0.45f)});
    out({V(0.03f, -0.78f), V(0.03f, 0.45f)});
    out(ellipse(V(0.f, 0.6f), 0.07f, 0.07f, 0.f, 16), 0.9f);
    out({V(0.f, 0.67f), V(0.f, 0.74f)}, 0.8f);
    out({V(-0.28f, -0.92f), V(0.28f, -0.92f), V(0.18f, -0.84f), V(-0.18f, -0.84f), V(-0.28f, -0.92f)}, 0.9f);
    out({V(-0.12f, -0.84f), V(-0.06f, -0.78f), V(0.06f, -0.78f), V(0.12f, -0.84f)}, 0.8f);
    const vec2 e1 = rotAround(V(-0.78f, 0.45f), piv, tilt), e2 = rotAround(V(0.78f, 0.45f), piv, tilt);
    out({e1, e2});
    out({rotAround(V(-0.78f, 0.41f), piv, tilt), rotAround(V(0.78f, 0.41f), piv, tilt)}, 0.5f);
    for (const vec2& e : {e1, e2}) {
        const vec2 pc = V(e.x, e.y - 0.62f);
        out({e, V(pc.x - 0.2f, pc.y)}, 0.55f);
        out({e, V(pc.x + 0.2f, pc.y)}, 0.55f);
        out({e, V(pc.x, pc.y)}, 0.4f);
        out(ellipse(pc, 0.22f, 0.1f, 0.f, 18, PI, TAU));
        out({V(pc.x - 0.22f, pc.y), V(pc.x + 0.22f, pc.y)}, 0.8f);
    }
}

// ---------------------------------------------------------------- NAVIS
// A ship under sail: a curved hull with its wale and portholes, a mast, a billowing square sail, a pennant
// streaming from the masthead, engraved swell under her; in a drop she rides it.
void navis(float life, float t, Out out) {
    const float roll = std::sin(t * 1.2f) * 0.06f * life;
    const vec2 piv = V(0.f, -0.3f);
    std::vector<Line> ship;
    std::vector<float> w;
    auto S = [&](const Line& l, float wt = 1.f) { ship.push_back(l); w.push_back(wt); };
    S(bezier(V(-0.82f, -0.12f), V(-0.6f, -0.62f), V(0.5f, -0.6f), V(0.86f, -0.05f), 22));
    S({V(-0.82f, -0.12f), V(-0.78f, -0.2f), V(0.86f, -0.18f), V(0.86f, -0.05f)});
    S(bezier(V(-0.75f, -0.28f), V(-0.5f, -0.44f), V(0.45f, -0.42f), V(0.8f, -0.2f), 18), 0.55f);
    for (int i = 0; i < 6; i++) S(ellipse(V(-0.45f + i * 0.2f, -0.29f), 0.025f, 0.025f, 0.f, 8), 0.6f);
    S({V(0.86f, -0.05f), V(1.0f, 0.1f)}, 0.8f);
    S({V(-0.82f, -0.12f), V(-0.88f, 0.02f), V(-0.7f, 0.02f), V(-0.7f, -0.16f)}, 0.8f);
    S({V(0.f, -0.18f), V(0.f, 0.92f)});
    const float bill = 0.1f + 0.04f * std::sin(t * 1.6f) * life;
    S(bezier(V(-0.36f, 0.74f), V(-0.1f, 0.7f + bill * 0.2f), V(0.1f, 0.7f + bill * 0.2f), V(0.36f, 0.74f), 12));
    S(bezier(V(-0.36f, 0.74f), V(-0.36f - bill, 0.5f), V(-0.36f - bill, 0.25f), V(-0.32f, 0.08f), 12));
    S(bezier(V(0.36f, 0.74f), V(0.36f + bill, 0.5f), V(0.36f + bill, 0.25f), V(0.32f, 0.08f), 12));
    S(bezier(V(-0.32f, 0.08f), V(-0.1f, 0.03f), V(0.1f, 0.03f), V(0.32f, 0.08f), 12));
    for (int i = 1; i < 4; i++) {
        const float y = 0.08f + i * 0.165f;
        S(bezier(V(-0.33f - bill * 0.7f, y), V(-0.1f, y - 0.04f), V(0.1f, y - 0.04f), V(0.33f + bill * 0.7f, y), 10), 0.45f);
    }
    Line pen;
    for (int i = 0; i <= 10; i++) {
        const float u = i / 10.f;
        pen.push_back(V(-0.05f - 0.4f * u, 0.9f + 0.05f * std::sin(u * 5.f - t * 4.f * (0.3f + life)) * u));
    }
    S(pen, 0.8f);
    for (size_t i = 0; i < ship.size(); i++) {
        for (vec2& p : ship[i]) p = rotAround(p, piv, roll);
        out(ship[i], w[i]);
    }
    // the swell
    for (int r = 0; r < 3; r++) {
        Line sw;
        for (int i = 0; i <= 24; i++) {
            const float x = -0.95f + 1.9f * i / 24.f;
            sw.push_back(V(x, -0.62f - r * 0.09f + 0.03f * std::sin(x * 9.f + r + t * 1.5f * life)));
        }
        out(sw, 0.5f - r * 0.1f);
    }
}

// ---------------------------------------------------------------- SCORPIO
// A scorpion (a Tetris's great constellation): a head with its claws, a segmented body, the tail curling up over
// its back to the sting; in a drop the tail sways and the claws open.
void scorpio(float life, float t, Out out) {
    const float sway = std::sin(t * 1.3f) * 0.12f * life, open = (0.5f + 0.5f * std::sin(t * 2.f)) * 0.25f * life;
    // the body and tail as a chain of segments along a spine
    std::vector<vec2> sp = {V(0.72f, 0.05f), V(0.5f, 0.02f), V(0.3f, -0.02f), V(0.1f, -0.06f), V(-0.12f, -0.1f),
                            V(-0.36f, -0.1f), V(-0.58f, -0.02f), V(-0.74f, 0.14f), V(-0.82f, 0.36f), V(-0.78f, 0.58f),
                            V(-0.64f, 0.74f)};
    for (size_t i = 5; i < sp.size(); i++) {
        const float k = (i - 4) / 6.f;
        sp[i] = rotAround(sp[i], sp[4], sway * k);
    }
    for (size_t i = 0; i + 1 < sp.size(); i++) {
        const vec2 a = sp[i], b = sp[i + 1];
        const vec2 m = mul(add(a, b), 0.5f), d = sub(b, a);
        const float wid = i < 4 ? 0.13f - i * 0.012f : 0.075f - (i - 4) * 0.006f;
        out(ellipse(m, len(d) * 0.55f, wid, std::atan2(d.y, d.x), 20), i < 4 ? 1.f : 0.9f);
    }
    // the sting
    const vec2 e = sp.back(), d = norm(sub(sp.back(), sp[sp.size() - 2]));
    out(bezier(e, add(e, add(mul(d, 0.12f), mul(perp(d), -0.02f))), add(e, add(mul(d, 0.16f), mul(perp(d), -0.1f))),
               add(e, add(mul(d, 0.1f), mul(perp(d), -0.16f))), 10));
    // legs
    for (int i = 0; i < 4; i++) {
        const vec2 b = sp[1 + i];
        for (int s = -1; s <= 1; s += 2) {
            const vec2 k = add(b, V(-0.04f * i, s * 0.2f)), f = add(b, V(-0.1f - 0.05f * i, s * 0.3f));
            out({b, k, f}, 0.6f);
        }
    }
    // the claws
    for (int s = -1; s <= 1; s += 2) {
        const float sy = (float)s;
        const vec2 a = V(0.76f, sy * 0.06f), el = V(0.86f, sy * 0.3f), w = V(1.f, sy * 0.36f);
        out({a, el, w}, 0.9f);
        const float o = open * sy;
        out(bezier(w, add(w, V(0.06f, sy * 0.12f)), add(w, V(0.0f, sy * 0.22f)), add(w, V(-0.1f, sy * 0.2f + o * 0.3f)), 8), 0.9f);
        out(bezier(w, add(w, V(0.1f, sy * 0.02f)), add(w, V(0.12f, sy * 0.1f - o * 0.2f)), add(w, V(0.04f, sy * 0.14f - o * 0.2f)), 8), 0.9f);
    }
}

const FigureDef FIGURES[] = {
    {"Serpens",
     {V(-0.88f, 0.02f), V(-0.52f, 0.2f), V(-0.17f, -0.13f), V(0.17f, -0.18f), V(0.5f, 0.22f), V(0.86f, 0.28f)},
     {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}}},
    {"Piscis",
     {V(0.85f, 0.f), V(0.12f, 0.4f), V(0.08f, -0.34f), V(-0.58f, 0.f), V(-0.92f, 0.36f), V(-0.92f, -0.36f)},
     {{0, 1}, {1, 3}, {0, 2}, {2, 3}, {3, 4}, {3, 5}}},
    {"Cygnus",
     {V(0.88f, 0.05f), V(0.05f, 0.f), V(-0.68f, 0.f), V(-0.12f, 0.48f), V(-0.3f, 0.86f), V(-0.12f, -0.48f), V(-0.3f, -0.86f)},
     {{0, 1}, {1, 2}, {1, 3}, {3, 4}, {1, 5}, {5, 6}}},
    {"Corona",
     {V(-0.76f, -0.16f), V(-0.64f, 0.2f), V(-0.32f, 0.42f), V(0.f, 0.48f), V(0.32f, 0.42f), V(0.64f, 0.2f), V(0.76f, -0.16f)},
     {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}}},
    {"Lyra",
     {V(-0.48f, 0.66f), V(0.48f, 0.66f), V(-0.34f, -0.3f), V(0.34f, -0.3f), V(0.f, -0.74f), V(0.f, 0.48f)},
     {{0, 5}, {5, 1}, {0, 2}, {1, 3}, {2, 4}, {4, 3}}},
    {"Arcus",
     {V(0.02f, 0.9f), V(0.48f, 0.f), V(0.02f, -0.9f), V(-0.45f, 0.f), V(0.95f, 0.f)},
     {{0, 1}, {1, 2}, {0, 3}, {3, 2}, {1, 4}}},
    {"Libra",
     {V(0.f, 0.7f), V(-0.78f, 0.45f), V(0.78f, 0.45f), V(-0.78f, -0.22f), V(0.78f, -0.22f), V(0.f, -0.84f)},
     {{1, 0}, {0, 2}, {1, 3}, {2, 4}, {0, 5}}},
    {"Navis",
     {V(-0.82f, -0.12f), V(0.f, -0.48f), V(0.86f, -0.05f), V(0.f, 0.92f), V(-0.36f, 0.74f), V(0.32f, 0.08f)},
     {{0, 1}, {1, 2}, {1, 5}, {5, 3}, {3, 4}}},
    {"Scorpio",
     {V(1.f, 0.36f), V(1.f, -0.36f), V(0.72f, 0.05f), V(0.3f, -0.02f), V(-0.12f, -0.1f), V(-0.58f, -0.02f),
      V(-0.82f, 0.36f), V(-0.64f, 0.74f), V(-0.6f, 0.58f)},
     {{0, 2}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8}}},
};
constexpr int FIGURE_COUNT = (int)(sizeof(FIGURES) / sizeof(FIGURES[0]));

} // namespace

int figureCount() { return FIGURE_COUNT; }
const FigureDef& figure(int i) { return FIGURES[((i % FIGURE_COUNT) + FIGURE_COUNT) % FIGURE_COUNT]; }

void figureLines(int i, float life, float t, std::vector<std::vector<vec2>>& lines, std::vector<float>& weights) {
    Out out{lines, weights};
    switch (((i % FIGURE_COUNT) + FIGURE_COUNT) % FIGURE_COUNT) {
    case 0: serpens(life, t, out); break;
    case 1: piscis(life, t, out); break;
    case 2: cygnus(life, t, out); break;
    case 3: corona(life, t, out); break;
    case 4: lyra(life, t, out); break;
    case 5: arcus(life, t, out); break;
    case 6: libra(life, t, out); break;
    case 7: navis(life, t, out); break;
    default: scorpio(life, t, out); break;
    }
}

} // namespace starchart

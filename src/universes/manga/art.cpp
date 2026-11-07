// MANGA's page: a grid of panels around the well and six drawings of an evening in a seaside town, each one a list
// of elements inked one after the other (see manga.cpp).
//
// The grid: the well's tall panel in the middle; beside it, the HUD's quiet panels (the held piece, the score, the
// next pieces, the bonus gauge) and under them two small panels; the outer columns hold two large panels each, cut
// by a slanted gutter. Every drawing is original:
//   A  the town at night from a rooftop: a moon behind thin clouds, a skyline, a water tower, a utility pole and
//      its sagging wires, birds;
//   B  a railway crossing at street level: the road running into the distance, a block wall, the rails, the
//      signal with its crossbuck and its two lamps (they blink on the beat), the lowered barrier;
//   C  a wind chime under the eaves (it sways with the music);
//   D  a vending machine glowing in the dark;
//   E  a lighthouse on a black cliff over the sea under a big cumulus;
//   F  stone stairs climbing to a shrine gate against the setting sun, stone lanterns and dark trees.
// Coordinates are board units (y up): the outer columns run from x = +-12.1 out to the page's edge, the
// drawings are set from the inner edge outward, their skies and seas fill the whole panel.
#include "universes/manga/art.hpp"

#include <cmath>

#include "mathutil.hpp"

namespace manga {

namespace {

constexpr float PI_F = 3.14159265f;

std::vector<V2> rectPts(float x0, float y0, float x1, float y1) { return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}; }

std::vector<V2> ellipsePts(V2 c, float rx, float ry, int n, float a0 = 0.f, float a1 = 2.f * PI_F) {
    std::vector<V2> v;
    const bool full = a1 - a0 >= 2.f * PI_F - 1e-3f;
    const int m = full ? n : n + 1;
    for (int i = 0; i < m; i++) {
        const float a = a0 + (a1 - a0) * i / (float)n;
        v.push_back({c.x + std::cos(a) * rx, c.y + std::sin(a) * ry});
    }
    return v;
}

// A wire hanging between two points.
std::vector<V2> sag(V2 a, V2 b, float depth, int n = 18) {
    std::vector<V2> v;
    for (int i = 0; i <= n; i++) {
        const float t = i / (float)n;
        v.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t - depth * std::sin(PI_F * t)});
    }
    return v;
}

Tone solid(int color = C_INK) {
    Tone t;
    t.kind = TONE_SOLID;
    t.color = color;
    return t;
}
Tone dots(float d, float cell = 0.14f, int color = C_INK) {
    Tone t;
    t.kind = TONE_DOTS;
    t.d0 = t.d1 = d;
    t.cell = cell;
    t.color = color;
    return t;
}
// A gradient: density d0 at height y0 to d1 at height y1.
Tone gradDots(float d0, float y0, float d1, float y1, float cell = 0.14f) {
    Tone t = dots(d0, cell);
    t.d1 = d1;
    t.g0 = {0, y0};
    t.g1 = {0, y1};
    return t;
}
Tone lines(float d, float cell, float angle, int color = C_INK) {
    Tone t;
    t.kind = TONE_LINES;
    t.d0 = t.d1 = d;
    t.cell = cell;
    t.angle = angle;
    t.color = color;
    return t;
}
Tone gradLines(float d0, float y0, float d1, float y1, float cell, float angle = 0.f) {
    Tone t = lines(d0, cell, angle);
    t.d1 = d1;
    t.g0 = {0, y0};
    t.g1 = {0, y1};
    return t;
}

// Builds the elements of one panel.
struct Ink {
    Page& pg;
    int panel = 0;
    Element* e = nullptr;
    void begin(V2 center, int anim = A_NONE) {
        pg.elems.push_back(Element());
        e = &pg.elems.back();
        e->panel = panel;
        e->center = center;
        e->anim = anim;
    }
    void fill(std::vector<V2> pts, const Tone& t, bool strip = false) {
        Fill f;
        f.pts = std::move(pts);
        f.tone = t;
        f.strip = strip;
        e->fills.push_back(std::move(f));
    }
    void stroke(std::vector<V2> pts, float w, float taper = 0.2f, bool closed = false, int color = C_INK) {
        Stroke s;
        s.pts = std::move(pts);
        s.width = w;
        s.taper = taper;
        s.closed = closed;
        s.color = color;
        e->strokes.push_back(std::move(s));
    }
    // A filled shape with its ink outline.
    void shape(const std::vector<V2>& pts, const Tone& t, float w = 1.6f) {
        fill(pts, t);
        stroke(pts, w, 0.f, true);
    }
    void panelFill(const Tone& t) {
        const Panel& p = pg.panels[panel];
        fill({p.c[3], p.c[2], p.c[1], p.c[0]}, t);
    }
};

// A cloud: shallow bumps along its top over a flat base, as a strip (top, base) from left to right.
std::vector<V2> cloudStrip(float x0, float x1, float base, float h, int bumps, Rng& r) {
    std::vector<V2> v;
    const float w = (x1 - x0) / bumps;
    for (int b = 0; b < bumps; b++) {
        const float bh = h * (0.55f + 0.45f * std::sin(PI_F * (b + 0.5f) / bumps)) * r.range(0.85f, 1.1f);
        for (int i = 0; i <= 6; i++) {
            if (b > 0 && i == 0) continue;
            const float t = i / 6.f;
            const float x = x0 + (b + t) * w;
            // the top of a bump, its ends dipping a little between bumps
            const float y = base + bh * (0.72f + 0.28f * std::sin(PI_F * t)) * (x < x0 + 0.2f * w || x > x1 - 0.2f * w ? 0.5f : 1.f);
            v.push_back({x, y});
            v.push_back({x, base});
        }
    }
    return v;
}
std::vector<V2> stripTop(const std::vector<V2>& s) {
    std::vector<V2> v;
    for (size_t i = 0; i < s.size(); i += 2) v.push_back(s[i]);
    return v;
}

// ---------------------------------------------------------------- A: the town at night
void drawRooftops(Ink& k, float X, Rng& r) {
    (void)X;
    // the sky: a gradient of dots, dense at the top, fading down to the skyline
    k.begin({-16.f, 9.f});
    k.e->pencil = false;
    k.panelFill(gradDots(0.3f, 10.5f, 0.f, 4.6f, 0.13f));

    // the moon, three soft craters
    const V2 mc{-17.4f, 8.3f};
    k.begin(mc);
    k.shape(ellipsePts(mc, 1.2f, 1.2f, 56), solid(C_PAPER), 1.8f);
    k.fill(ellipsePts({mc.x - 0.35f, mc.y + 0.3f}, 0.34f, 0.28f, 20), dots(0.2f, 0.09f));
    k.fill(ellipsePts({mc.x + 0.35f, mc.y - 0.25f}, 0.24f, 0.2f, 18), dots(0.2f, 0.09f));
    k.fill(ellipsePts({mc.x - 0.15f, mc.y - 0.55f}, 0.15f, 0.12f, 14), dots(0.2f, 0.09f));

    // thin clouds across the moon
    k.begin({-17.f, 7.4f});
    {
        auto s = cloudStrip(-20.6f, -15.6f, 7.15f, 0.55f, 5, r);
        k.fill(s, solid(C_PAPER), true);
        k.stroke(stripTop(s), 1.5f, 0.15f);
        k.stroke({{-20.6f, 7.15f}, {-15.6f, 7.15f}}, 0.9f, 0.3f);
        auto s2 = cloudStrip(-15.2f, -12.f, 9.25f, 0.4f, 4, r);
        k.fill(s2, solid(C_PAPER), true);
        k.stroke(stripTop(s2), 1.3f, 0.15f);
        k.stroke({{-15.2f, 9.25f}, {-12.f, 9.25f}}, 0.8f, 0.3f);
    }

    // the far skyline: a lighter tone, its roofs drawn
    k.begin({-16.f, 4.8f});
    {
        float x = -20.6f;
        const float hs[] = {4.7f, 5.4f, 4.9f, 6.1f, 5.2f, 4.6f, 5.7f, 5.0f, 5.5f};
        int i = 0;
        while (x < -12.f) {
            const float w = r.range(0.7f, 1.3f), h = hs[i++ % 9];
            k.fill(rectPts(x, 1.4f, x + w, h), dots(0.22f, 0.1f));
            k.stroke({{x, h - 0.6f}, {x, h}, {x + w, h}, {x + w, h - 0.6f}}, 1.f, 0.f);
            if (i % 3 == 1) k.stroke({{x + w * 0.6f, h}, {x + w * 0.6f, h + 0.5f}}, 0.9f, 0.f); // an antenna
            x += w;
        }
    }

    // the near buildings: black, their windows knocked out, some lit
    k.begin({-15.f, 3.5f});
    {
        struct Bld { float x0, x1, h; };
        const Bld b[] = {{-20.6f, -18.9f, 4.1f}, {-18.9f, -17.6f, 3.4f}, {-17.6f, -15.3f, 4.55f}, {-15.3f, -14.2f, 3.7f},
                         {-14.2f, -12.f, 4.15f}};
        for (const Bld& d : b) {
            k.shape(rectPts(d.x0, 1.4f, d.x1, d.h), solid(), 1.4f);
            // a parapet
            k.fill(rectPts(d.x0 - 0.05f, d.h, d.x1 + 0.05f, d.h + 0.1f), solid());
            for (float y = d.h - 0.45f; y > 1.7f; y -= 0.48f)
                for (float x = d.x0 + 0.22f; x + 0.2f < d.x1 - 0.12f; x += 0.42f)
                    if (r.chance(0.42f)) k.fill(rectPts(x, y - 0.24f, x + 0.2f, y), solid(C_PAPER));
        }
    }

    // a water tower on the tallest roof
    k.begin({-16.4f, 5.3f});
    {
        const float cx = -16.4f, base = 4.65f;
        for (float dx : {-0.42f, -0.14f, 0.14f, 0.42f}) k.stroke({{cx + dx, base}, {cx + dx * 0.8f, base + 0.55f}}, 1.3f, 0.f);
        k.stroke({{cx - 0.4f, base + 0.25f}, {cx + 0.4f, base + 0.25f}}, 1.f, 0.f);
        k.shape(rectPts(cx - 0.5f, base + 0.55f, cx + 0.5f, base + 1.25f), lines(0.3f, 0.1f, 0.f), 1.5f);
        k.shape({{cx - 0.56f, base + 1.25f}, {cx + 0.56f, base + 1.25f}, {cx, base + 1.62f}}, solid(), 1.4f);
    }

    // the utility pole by the gutter, its crossarms and transformer
    k.begin({-13.f, 7.f});
    {
        const float px = -13.05f;
        k.shape({{px - 0.13f, 1.4f}, {px + 0.13f, 1.4f}, {px + 0.09f, 9.9f}, {px - 0.09f, 9.9f}}, solid(), 1.2f);
        for (float y : {9.45f, 8.85f}) {
            k.shape(rectPts(px - 1.05f, y - 0.09f, px + 1.05f, y + 0.07f), solid(), 1.f);
            for (float dx : {-0.9f, -0.45f, 0.45f, 0.9f}) k.fill(rectPts(px + dx - 0.05f, y + 0.07f, px + dx + 0.05f, y + 0.2f), solid());
        }
        k.shape(rectPts(px + 0.13f, 6.9f, px + 0.6f, 7.75f), dots(0.3f, 0.1f), 1.4f);
        k.stroke({{px + 0.13f, 7.6f}, {px + 0.13f, 7.6f}, {px + 0.13f, 7.1f}}, 1.f, 0.f);
        // a step bolt ladder
        for (float y = 3.f; y < 8.6f; y += 0.42f) k.stroke({{px - 0.22f, y}, {px - 0.12f, y}}, 1.f, 0.f);
    }

    // the wires, sagging away to the far side
    k.begin({-17.f, 8.6f});
    {
        const float px = -13.05f;
        k.stroke(sag({px - 0.9f, 9.55f}, {-20.8f, 10.1f}, 0.65f), 1.2f, 0.f);
        k.stroke(sag({px - 0.45f, 9.55f}, {-20.8f, 9.4f}, 0.8f), 1.2f, 0.f);
        k.stroke(sag({px - 0.9f, 8.95f}, {-20.8f, 8.f}, 0.55f), 1.2f, 0.f);
        k.stroke(sag({px - 0.45f, 8.95f}, {-20.8f, 7.3f}, 0.45f), 1.f, 0.f);
        k.stroke(sag({px + 0.9f, 9.55f}, {-11.9f, 9.25f}, 0.1f, 4), 1.2f, 0.f);
    }

    // birds crossing the moon
    k.begin({-18.6f, 9.4f});
    {
        const V2 bs[] = {{-19.2f, 9.55f}, {-18.55f, 9.85f}, {-18.85f, 9.2f}};
        float s = 0.26f;
        for (const V2& b : bs) {
            k.stroke({{b.x - s, b.y + s * 0.35f}, {b.x - s * 0.4f, b.y + s * 0.15f}, {b.x, b.y - s * 0.1f}, {b.x + s * 0.4f, b.y + s * 0.18f},
                      {b.x + s, b.y + s * 0.4f}}, 1.6f, 0.35f);
            s *= 0.82f;
        }
    }
}

// ---------------------------------------------------------------- B: the railway crossing
void drawCrossing(Ink& k, float X, Rng& r) {
    (void)X;
    const float hz = -0.5f;      // the horizon
    const V2 vp{-17.3f, hz};     // the road's vanishing point
    const float bottom = -11.2f;

    // a pale evening sky over the roofs
    k.begin({-16.f, 1.5f});
    k.e->pencil = false;
    k.fill({{-21.f, hz}, {-12.f, hz}, {-12.f, 3.2f}, {-21.f, 3.2f}}, gradDots(0.3f, 3.2f, 0.f, hz + 0.6f, 0.15f));

    // the houses far down the road: roofs in a dot tone
    k.begin({-16.f, 0.2f});
    {
        float x = -20.8f;
        int i = 0;
        while (x < -12.f) {
            const float w = r.range(0.9f, 1.5f), h = r.range(0.45f, 0.8f), roof = r.range(0.25f, 0.45f);
            const bool tall = i % 3 == 2;
            const float hh = tall ? h + 0.45f : h;
            k.shape(rectPts(x, hz - 0.05f, x + w, hz + hh), dots(0.22f, 0.1f), 1.f);
            k.shape({{x - 0.08f, hz + hh}, {x + w + 0.08f, hz + hh}, {x + w * 0.5f, hz + hh + roof}}, dots(0.55f, 0.1f), 1.f);
            if (tall) k.fill(rectPts(x + w * 0.3f, hz + 0.25f, x + w * 0.45f, hz + 0.45f), solid());
            x += w + r.range(0.05f, 0.3f);
            i++;
        }
    }

    // the road: its edges run to the vanishing point, a dashed center line
    k.begin({-17.f, -7.f});
    {
        k.stroke({vp, {-21.4f, -7.8f}}, 1.6f, 0.f);
        k.stroke({vp, {-13.1f, bottom}}, 1.6f, 0.f);
        // the dashes, longer as they come closer
        for (int i = 0; i < 7; i++) {
            const float t0 = 0.06f + 0.135f * i, t1 = t0 + 0.06f;
            auto at = [&](float t, float side) {
                const float y = lerpf(hz, bottom, t * t * 1.25f);
                const float s = (y - hz) / (bottom - hz);
                return V2{vp.x + (0.25f + side * 0.06f) * s * 2.f, y};
            };
            if (lerpf(hz, bottom, t1 * t1 * 1.25f) < -8.8f) break; // clear of the caption
            k.fill({at(t0, -1.f), at(t0, 1.f), at(t1, 1.f), at(t1, -1.f)}, solid());
        }
        // a curb along the right edge
        k.stroke({{vp.x + 0.15f, hz}, {-12.4f, bottom}}, 1.f, 0.f);
    }

    // the block wall on the left, its courses converging
    k.begin({-19.5f, -2.5f});
    {
        auto edge = [&](float y) { return vp.x + (y - hz) / (-7.8f - hz) * (-21.4f - vp.x); }; // the road's left edge at y
        const float topNear = -1.5f, topFar = hz + 0.25f;
        std::vector<V2> wall = {{-21.f, edge(-7.8f) < -21.f ? -7.6f : -7.8f}, {edge(hz - 0.2f), hz - 0.2f}, {edge(hz - 0.2f), topFar},
                                {-21.f, topNear}};
        k.shape(wall, dots(0.4f, 0.11f), 1.5f);
        for (int i = 1; i < 6; i++) {
            const float t = i / 6.f;
            k.stroke({{-21.f, lerpf(-7.6f, topNear, t)}, {edge(hz - 0.2f), lerpf(hz - 0.2f, topFar, t)}}, 0.8f, 0.f);
        }
        for (float x = -20.6f; x < edge(hz - 0.2f) - 0.1f; x += 0.35f + (x + 21.f) * 0.18f) {
            const float t = (x + 21.f) / (edge(hz - 0.2f) + 21.f);
            k.stroke({{x, lerpf(-7.6f, hz - 0.2f, t) + 0.05f}, {x, lerpf(topNear, topFar, t) - 0.05f}}, 0.7f, 0.f);
        }
        // its coping, a black line on top
        k.stroke({{-21.f, topNear}, {edge(hz - 0.2f), topFar}}, 2.6f, 0.f);
    }

    // the rails across the road, their sleepers
    k.begin({-16.f, -3.4f});
    {
        for (float y : {-3.05f, -3.75f}) {
            k.stroke({{-21.f, y}, {-12.f, y}}, 1.5f, 0.f);
            k.stroke({{-21.f, y - 0.09f}, {-12.f, y - 0.09f}}, 1.f, 0.f);
        }
        for (float x = -20.8f; x < -12.f; x += 0.42f) k.fill(rectPts(x, -3.95f, x + 0.16f, -3.85f), solid());
        k.fill({{-21.f, -3.25f}, {-12.f, -3.25f}, {-12.f, -3.55f}, {-21.f, -3.55f}}, lines(0.35f, 0.09f, 0.f));
    }

    // the crossing signal: its post, the crossbuck, the box of the barrier
    const float sx = -13.45f;
    k.begin({sx, -2.f});
    {
        k.shape(rectPts(sx - 0.1f, -9.f, sx + 0.1f, 1.7f), solid(), 1.f);
        k.shape(rectPts(sx - 0.35f, -9.4f, sx + 0.35f, -8.9f), solid(), 1.f);
        // the crossbuck: two striped bars
        const V2 c{sx, 1.15f};
        for (float a : {0.6f, -0.6f}) {
            const float ca = std::cos(a), sa = std::sin(a), L = 1.05f, W = 0.15f;
            auto P = [&](float u, float v) { return V2{c.x + ca * u - sa * v, c.y + sa * u + ca * v}; };
            std::vector<V2> bar = {P(-L, -W), P(L, -W), P(L, W), P(-L, W)};
            Tone t = lines(0.5f, 0.24f, a + PI_F * 0.5f);
            k.shape(bar, t, 1.4f);
        }
        // the lamps' crossbar and hoods
        k.shape(rectPts(sx - 0.95f, -0.55f, sx + 0.95f, -0.38f), solid(), 1.f);
        // a warning plate under it
        k.shape({{sx - 0.32f, -1.6f}, {sx + 0.32f, -1.6f}, {sx, -2.15f}}, solid(C_PAPER), 1.4f);
        k.fill({{sx - 0.06f, -1.7f}, {sx + 0.06f, -1.7f}, {sx + 0.03f, -1.9f}, {sx - 0.03f, -1.9f}}, solid());
        // the barrier's machine
        k.shape(rectPts(sx - 0.32f, -6.3f, sx + 0.32f, -5.3f), dots(0.35f, 0.1f), 1.5f);
    }

    // the two lamps (they blink)
    k.begin({sx, -0.9f}, A_LAMPS);
    {
        const float lr = 0.32f;
        k.pg.lamp[0] = {sx - 0.62f, -0.9f};
        k.pg.lamp[1] = {sx + 0.62f, -0.9f};
        k.pg.lampRadius = lr;
        for (const V2& l : k.pg.lamp) {
            k.shape(ellipsePts(l, lr + 0.12f, lr + 0.12f, 32), solid(), 1.f);
            k.shape(ellipsePts(l, lr, lr, 32), dots(0.7f, 0.08f), 1.f);
            // the hood above
            k.fill(ellipsePts(l, lr + 0.22f, lr + 0.22f, 16, 0.15f, PI_F - 0.15f), solid());
        }
    }

    // the barrier, lowered across the road: black and white stripes, a fringe
    k.begin({-16.f, -5.8f});
    {
        const float y0 = -5.92f, y1 = -5.68f, xl = -19.8f, xr = sx - 0.3f;
        k.shape(rectPts(xl, y0, xr, y1), solid(C_PAPER), 1.5f);
        int i = 0;
        for (float x = xr - 0.45f; x > xl; x -= 0.45f, i++)
            if (i % 2 == 0) k.fill({{std::max(xl, x - 0.45f), y0}, {x - 0.15f, y0}, {x, y1}, {std::max(xl, x - 0.3f), y1}}, solid());
        for (float x = xl + 0.3f; x < xr - 0.3f; x += 0.3f) k.stroke({{x, y0}, {x + 0.03f, y0 - 0.35f}}, 0.9f, 0.4f);
    }

    // wires over the crossing
    k.begin({-17.f, 2.f});
    k.stroke(sag({sx + 0.1f, 1.6f}, {-21.f, 2.4f}, 0.55f), 1.1f, 0.f);
    k.stroke(sag({sx + 0.1f, 1.3f}, {-21.f, 1.5f}, 0.45f), 1.1f, 0.f);
    k.stroke(sag({sx + 0.1f, 1.6f}, {-11.9f, 2.f}, 0.08f, 4), 1.1f, 0.f);
}

// ---------------------------------------------------------------- C: the wind chime
void drawChime(Ink& k) {
    k.begin({-8.8f, -8.f});
    k.e->pencil = false;
    k.fill(rectPts(-11.8f, -11.3f, -5.8f, -5.3f), gradDots(0.3f, -5.4f, 0.02f, -10.8f, 0.14f));

    // the eaves across the top, rafter ends knocked out
    k.begin({-8.8f, -5.9f});
    {
        k.shape({{-11.8f, -5.3f}, {-5.8f, -5.3f}, {-5.8f, -5.95f}, {-11.8f, -6.55f}}, solid(), 1.4f);
        for (int i = 0; i < 9; i++) {
            const float x = -11.5f + i * 0.68f, y = lerpf(-6.55f, -5.95f, (x + 11.8f) / 6.f);
            k.fill(rectPts(x, y + 0.08f, x + 0.24f, y + 0.3f), solid(C_PAPER));
        }
        k.stroke({{-11.8f, -6.72f}, {-5.8f, -6.12f}}, 1.2f, 0.f);
    }

    // the chime: a glass bell on a string, its clapper and a paper strip
    const V2 hook{-8.7f, -6.3f};
    k.begin({-8.7f, -7.8f}, A_CHIME);
    k.e->pivot = hook;
    {
        k.stroke({hook, {hook.x, -6.95f}}, 1.1f, 0.f);
        const V2 bc{hook.x, -7.55f};
        std::vector<V2> bell = ellipsePts(bc, 0.6f, 0.6f, 28, 0.f, PI_F);
        k.fill(bell, solid(C_PAPER));
        k.fill(ellipsePts(bc, 0.6f, 0.6f, 14, 0.f, PI_F * 0.42f), dots(0.25f, 0.08f));
        bell.push_back(bell.front());
        k.stroke(bell, 1.7f, 0.f);
        // a painted goldfish swirl
        k.stroke({{bc.x - 0.35f, bc.y + 0.22f}, {bc.x - 0.15f, bc.y + 0.32f}, {bc.x + 0.05f, bc.y + 0.2f}, {bc.x - 0.1f, bc.y + 0.1f}}, 1.3f, 0.3f);
        k.stroke({{bc.x + 0.12f, bc.y + 0.38f}, {bc.x + 0.32f, bc.y + 0.3f}}, 1.1f, 0.3f);
        k.fill(ellipsePts({bc.x, bc.y + 0.62f}, 0.07f, 0.05f, 10), solid());
        // the clapper, the strip
        k.stroke({{bc.x, bc.y}, {bc.x, -8.35f}}, 1.f, 0.f);
        k.fill(rectPts(bc.x - 0.05f, -7.9f, bc.x + 0.05f, -7.7f), solid());
        k.shape({{bc.x - 0.24f, -8.35f}, {bc.x + 0.24f, -8.35f}, {bc.x + 0.24f, -10.25f}, {bc.x - 0.24f, -10.25f}}, lines(0.22f, 0.1f, PI_F * 0.5f), 1.4f);
        k.fill(rectPts(bc.x - 0.12f, -9.9f, bc.x + 0.12f, -8.7f), solid(C_PAPER));
        for (float y = -8.85f; y > -9.8f; y -= 0.2f) k.stroke({{bc.x - 0.06f, y}, {bc.x + 0.06f, y - 0.03f}}, 1.f, 0.3f);
    }
}

// ---------------------------------------------------------------- D: the vending machine
void drawVending(Ink& k) {
    k.begin({8.8f, -8.5f});
    k.e->pencil = false;
    k.panelFill(dots(0.84f, 0.13f));

    const float x0 = 7.75f, x1 = 9.85f, yb = -10.5f, yt = -6.65f;
    // the glow around it: rays knocked out of the dark
    k.begin({8.8f, -8.5f});
    {
        const V2 c{(x0 + x1) * 0.5f, (yb + yt) * 0.5f};
        for (int i = 0; i < 38; i++) {
            const float a = i / 38.f * 2.f * PI_F + 0.05f * std::sin(i * 7.f);
            const float ca = std::cos(a), sa = std::sin(a);
            // from the machine's edge outward
            const float tx = ca != 0.f ? ((ca > 0 ? x1 : x0) - c.x) / ca : 1e9f, ty = sa != 0.f ? ((sa > 0 ? yt : yb) - c.y) / sa : 1e9f;
            const float t0 = std::min(std::fabs(tx), std::fabs(ty)) + 0.12f, t1 = t0 + 0.5f + 0.55f * std::fabs(std::sin(i * 2.3f));
            k.stroke({{c.x + ca * t0, c.y + sa * t0}, {c.x + ca * t1, c.y + sa * t1}}, 1.5f, 0.5f, false, C_PAPER);
        }
        k.fill(rectPts(5.8f, -11.2f, 11.8f, -10.5f), lines(0.5f, 0.12f, 0.f, C_PAPER));
    }

    // the machine
    k.begin({8.8f, -8.3f});
    {
        k.shape(rectPts(x0, yb, x1, yt), solid(C_PAPER), 2.f);
        k.shape(rectPts(x0 + 0.15f, -8.75f, x1 - 0.15f, yt - 0.15f), solid(C_PAPER), 1.3f);
        k.fill(rectPts(x1 - 0.12f, yb, x1, yt), dots(0.45f, 0.08f));
        k.shape(rectPts(x0 + 0.35f, yb + 0.25f, x1 - 0.55f, yb + 0.65f), solid(), 1.f);
        k.shape(rectPts(x1 - 0.48f, -9.6f, x1 - 0.25f, -9.0f), dots(0.3f, 0.07f), 1.f);
        k.stroke({{x1 - 0.4f, -9.15f}, {x1 - 0.33f, -9.15f}}, 1.f, 0.f);
        k.fill(rectPts(x0 - 0.1f, yt, x1 + 0.1f, yt + 0.12f), solid());
    }

    // its bottles in rows, the buttons under them
    k.begin({8.8f, -7.6f});
    for (int row = 0; row < 3; row++) {
        const float yb0 = -8.6f + row * 0.62f;
        for (int i = 0; i < 5; i++) {
            const float bx = x0 + 0.33f + i * 0.36f;
            k.shape({{bx - 0.11f, yb0 + 0.08f}, {bx + 0.11f, yb0 + 0.08f}, {bx + 0.11f, yb0 + 0.32f}, {bx + 0.05f, yb0 + 0.42f},
                     {bx + 0.05f, yb0 + 0.48f}, {bx - 0.05f, yb0 + 0.48f}, {bx - 0.05f, yb0 + 0.42f}, {bx - 0.11f, yb0 + 0.32f}},
                    (i + row) % 3 == 0 ? dots(0.55f, 0.07f) : (i + row) % 3 == 1 ? lines(0.4f, 0.07f, 0.f) : solid(C_PAPER), 1.f);
            k.fill(rectPts(bx - 0.06f, yb0 - 0.02f, bx + 0.06f, yb0 + 0.04f), solid());
        }
        k.stroke({{x0 + 0.15f, yb0 - 0.06f}, {x1 - 0.15f, yb0 - 0.06f}}, 0.9f, 0.f);
    }
}

// ---------------------------------------------------------------- E: the lighthouse
void drawLighthouse(Ink& k, float X, Rng& r) {
    const float hz = 6.1f;
    // a sky of fine horizontal lines, thinning down to the horizon
    k.begin({16.f, 9.f});
    k.e->pencil = false;
    k.fill({{12.f, hz}, {X + 0.5f, hz}, {X + 0.5f, 10.6f}, {12.f, 10.6f}}, gradLines(0.4f, 10.5f, 0.04f, hz + 0.4f, 0.17f));

    // a big cumulus over the sea
    k.begin({18.5f, 8.f});
    {
        auto lo = cloudStrip(15.4f, X + 0.6f, 6.55f, 1.25f, 5, r);
        auto hi = cloudStrip(16.4f, 19.9f, 7.6f, 1.2f, 4, r);
        k.fill(hi, solid(C_PAPER), true);
        k.stroke(stripTop(hi), 1.7f, 0.12f);
        // its lower tier in front (an element of its own: an element's lines go over all its fills)
        k.begin({17.5f, 7.f});
        k.fill(lo, solid(C_PAPER), true);
        k.stroke(stripTop(lo), 1.7f, 0.12f);
        k.stroke({{15.4f, 6.55f}, {X + 0.6f, 6.55f}}, 1.f, 0.f);
        // its shaded underside
        k.fill({{15.6f, 6.55f}, {X + 0.6f, 6.55f}, {X + 0.6f, 7.0f}, {15.9f, 6.85f}}, gradDots(0.3f, 6.55f, 0.f, 6.9f, 0.1f));
    }

    // the sea: lines thickening toward us
    k.begin({17.f, 4.5f});
    k.fill({{12.f, 2.f}, {X + 0.5f, 2.f}, {X + 0.5f, hz}, {12.f, hz}}, gradLines(0.12f, hz, 0.62f, 2.4f, 0.15f));
    k.stroke({{12.f, hz}, {X + 0.5f, hz}}, 1.3f, 0.f);

    // glints and crests on the water
    k.begin({17.5f, 4.f});
    for (int i = 0; i < 26; i++) {
        const float y = r.range(2.4f, hz - 0.25f), t = (hz - y) / (hz - 2.4f);
        const float x = r.range(15.f, X), w = 0.25f + 0.6f * t;
        k.stroke({{x - w, y}, {x + w, y + 0.02f}}, 1.2f + 1.2f * t, 0.45f, false, C_PAPER);
    }
    for (int i = 0; i < 9; i++) {
        const float y = r.range(2.6f, 4.6f), x = r.range(17.4f, X - 0.4f);
        k.stroke({{x - 0.35f, y}, {x - 0.1f, y + 0.08f}, {x + 0.2f, y + 0.02f}, {x + 0.4f, y + 0.1f}}, 1.3f, 0.4f);
    }

    // the black cliff, white hatching on its face, grass on top
    k.begin({14.f, 5.5f});
    {
        const V2 top[] = {{11.9f, 7.55f}, {12.6f, 7.68f}, {13.3f, 7.8f}, {14.4f, 7.78f}, {15.f, 7.5f}, {15.35f, 6.9f},
                          {15.6f, 6.1f}, {15.95f, 5.2f}, {16.3f, 4.5f}, {16.7f, 3.8f}, {17.2f, 3.1f}, {17.9f, 2.3f}};
        std::vector<V2> strip, edge;
        for (const V2& p : top) {
            strip.push_back(p);
            strip.push_back({p.x, 1.9f});
            edge.push_back(p);
        }
        k.fill(strip, solid(), true);
        k.stroke(edge, 1.6f, 0.f);
        for (int i = 0; i < 16; i++) {
            const float x = r.range(14.3f, 17.f);
            // under the edge at x
            float ytop = 2.f;
            for (int j = 0; j + 1 < 12; j++)
                if (x >= top[j].x && x <= top[j + 1].x) ytop = lerpf(top[j].y, top[j + 1].y, (x - top[j].x) / (top[j + 1].x - top[j].x));
            const float y = ytop - r.range(0.25f, 1.2f);
            if (y < 2.3f) continue;
            k.stroke({{x - 0.25f, y + 0.22f}, {x + 0.25f, y - 0.22f}}, 1.1f, 0.4f, false, C_PAPER);
        }
        for (float x = 12.3f; x < 15.f; x += 0.22f) k.stroke({{x, 7.6f + 0.1f * std::sin(x * 9.f)}, {x + 0.06f, 7.95f + 0.08f * std::sin(x * 5.f)}}, 1.1f, 0.5f);
    }

    // the lighthouse
    const float cx = 13.95f;
    k.begin({cx, 8.7f});
    {
        const float yb = 7.75f, yt = 9.25f;
        k.shape({{cx - 0.42f, yb}, {cx + 0.42f, yb}, {cx + 0.28f, yt}, {cx - 0.28f, yt}}, solid(C_PAPER), 1.6f);
        k.fill({{cx + 0.05f, yb}, {cx + 0.42f, yb}, {cx + 0.28f, yt}, {cx + 0.03f, yt}}, dots(0.3f, 0.08f));
        k.fill({{cx - 0.38f, 8.25f}, {cx + 0.38f, 8.25f}, {cx + 0.35f, 8.55f}, {cx - 0.35f, 8.55f}}, solid());
        k.fill(rectPts(cx - 0.08f, yb, cx + 0.08f, yb + 0.26f), solid());
        k.shape(rectPts(cx - 0.42f, yt, cx + 0.42f, yt + 0.1f), solid(), 1.f);
        k.shape(rectPts(cx - 0.24f, yt + 0.1f, cx + 0.24f, yt + 0.48f), solid(C_PAPER), 1.3f);
        k.stroke({{cx, yt + 0.1f}, {cx, yt + 0.48f}}, 1.f, 0.f);
        k.shape({{cx - 0.3f, yt + 0.48f}, {cx + 0.3f, yt + 0.48f}, {cx, yt + 0.76f}}, solid(), 1.f);
        k.stroke({{cx, yt + 0.76f}, {cx, yt + 0.92f}}, 1.2f, 0.f);
        k.pg.beamFrom = {cx, yt + 0.29f};
    }
    k.e->anim = A_BEAM;

    // gulls
    k.begin({16.f, 9.2f});
    {
        const V2 gs[] = {{15.6f, 9.4f}, {16.4f, 9.0f}, {17.3f, 9.7f}};
        float s = 0.3f;
        for (const V2& g : gs) {
            k.stroke({{g.x - s, g.y + s * 0.2f}, {g.x - s * 0.45f, g.y + s * 0.35f}, {g.x, g.y}, {g.x + s * 0.45f, g.y + s * 0.35f},
                      {g.x + s, g.y + s * 0.2f}}, 1.6f, 0.35f);
            s *= 0.85f;
        }
    }
}

// ---------------------------------------------------------------- F: the shrine stairs
void drawShrine(Ink& k, float X, Rng& r) {
    const float gx = 16.4f;       // the gate's axis
    // a dusk sky, dense at the top
    k.begin({16.f, 1.f});
    k.e->pencil = false;
    k.fill({{12.f, -3.f}, {X + 0.5f, -3.f}, {X + 0.5f, 3.3f}, {12.f, 3.3f}}, gradDots(0.58f, 3.2f, 0.06f, -2.2f, 0.15f));

    // the setting sun behind the gate, cut by thin lines of haze
    const V2 sc{gx, -0.55f};
    k.begin(sc);
    {
        k.fill(ellipsePts(sc, 1.75f, 1.75f, 64), solid(C_PAPER));
        k.stroke(ellipsePts(sc, 1.75f, 1.75f, 64), 1.f, 0.f, true);
        for (float y = -1.9f; y < 0.4f; y += 0.36f) {
            const float hw = std::sqrt(std::max(0.f, 1.75f * 1.75f - (y - sc.y) * (y - sc.y)));
            k.stroke({{sc.x - hw - 0.3f, y}, {sc.x + hw + 0.3f, y}}, 1.1f + (0.4f - y) * 0.25f, 0.f);
        }
    }

    // dark trees on both sides: scalloped masses of foliage, specks of light in them
    auto foliage = [&](float x0, float x1, float y0, float y1, bool left) {
        std::vector<V2> pts;
        const V2 c{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
        const float rx = (x1 - x0) * 0.5f, ry = (y1 - y0) * 0.5f;
        for (int i = 0; i < 72; i++) {
            const float a = i / 72.f * 2.f * PI_F;
            const float bump = 1.f + 0.07f * std::fabs(std::sin(a * 7.f)) + 0.04f * std::sin(a * 13.f + (left ? 1.f : 2.f));
            pts.push_back({c.x + std::cos(a) * rx * bump, c.y + std::sin(a) * ry * bump});
        }
        k.fill(pts, solid());
        pts.push_back(pts.front());
        k.stroke(pts, 1.4f, 0.f);
        Tone t = dots(0.1f, 0.16f, C_PAPER);
        t.kind = TONE_SAND;
        k.fill(ellipsePts({c.x + (left ? 0.4f : -0.4f), c.y + ry * 0.35f}, rx * 0.6f, ry * 0.4f, 24), t);
    };
    k.begin({13.2f, -1.f});
    foliage(11.f, 14.6f, -4.2f, 0.9f, true);
    foliage(11.4f, 13.9f, -0.6f, 2.8f, true);
    k.stroke({{13.1f, -4.f}, {13.25f, -6.2f}}, 3.f, 0.1f);
    k.begin({19.6f, -1.f});
    foliage(18.25f, X + 1.5f, -4.4f, 1.1f, false);
    foliage(19.f, X + 1.f, -0.4f, 3.f, false);
    k.stroke({{19.5f, -4.1f}, {19.4f, -6.3f}}, 3.f, 0.1f);

    // the gate (the spot color where there is one)
    k.begin({gx, 0.2f});
    {
        const int col = C_SPOT;
        for (float px : {gx - 1.12f, gx + 1.12f}) {
            k.fill({{px - 0.15f, -2.65f}, {px + 0.15f, -2.65f}, {px + 0.12f, 0.95f}, {px - 0.12f, 0.95f}}, solid(col));
            k.fill(rectPts(px - 0.2f, -2.75f, px + 0.2f, -2.5f), solid());
        }
        // the top beam, its ends sweeping up
        std::vector<V2> top, bot;
        for (int i = 0; i <= 20; i++) {
            const float t = i / 20.f * 2.f - 1.f, x = gx + t * 2.05f;
            const float lift = 0.28f * std::pow(std::fabs(t), 3.f);
            top.push_back({x, 1.32f + lift});
            bot.push_back({x, 1.1f + lift * 1.2f - (std::fabs(t) > 0.95f ? 0.05f : 0.f)});
        }
        std::vector<V2> beam;
        for (size_t i = 0; i < top.size(); i++) {
            beam.push_back(top[i]);
            beam.push_back(bot[i]);
        }
        k.fill(beam, solid(), true);
        k.fill(rectPts(gx - 1.75f, 0.92f, gx + 1.75f, 1.1f), solid(col));
        k.fill(rectPts(gx - 1.55f, 0.2f, gx + 1.55f, 0.42f), solid(col));
        k.fill(rectPts(gx - 0.1f, 0.42f, gx + 0.1f, 0.92f), solid(col));
        k.stroke(top, 1.2f, 0.f);
    }

    // the stairs: steps widening toward us, their risers in a tone, the stone balustrades
    k.begin({gx, -7.f});
    {
        const float yTop = -2.7f, yBot = -11.3f, wTop = 1.25f, wBot = 3.6f;
        auto half = [&](float y) { return lerpf(wTop, wBot, (yTop - y) / (yTop - yBot)); };
        float y = yTop, h = 0.16f;
        while (y > yBot) {
            const float y2 = y - h, rise = h * 0.42f;
            const float a = half(y), b = half(y - rise);
            k.fill({{gx - a, y}, {gx + a, y}, {gx + b, y - rise}, {gx - b, y - rise}}, dots(0.42f, 0.1f));
            k.stroke({{gx - a, y}, {gx + a, y}}, 1.f + h * 2.f, 0.f);
            y = y2;
            h *= 1.11f;
        }
        for (float s : {-1.f, 1.f}) {
            k.stroke({{gx + s * wTop, yTop}, {gx + s * wBot, yBot}}, 1.6f, 0.f);
            k.stroke({{gx + s * (wTop + 0.18f), yTop + 0.18f}, {gx + s * (wBot + 0.65f), yBot + 0.6f}}, 1.6f, 0.f);
            k.fill({{gx + s * wTop, yTop}, {gx + s * (wTop + 0.18f), yTop + 0.18f}, {gx + s * (wBot + 0.65f), yBot + 0.6f}, {gx + s * wBot, yBot}},
                   lines(0.35f, 0.1f, s > 0 ? -1.1f : 1.1f));
        }
    }

    // stone lanterns on both sides
    k.begin({gx, -5.5f});
    for (float s : {-1.f, 1.f}) {
        const float lx = gx + s * 3.05f, b = -7.3f, sc = 1.1f;
        auto R = [&](float x0, float y0, float x1, float y1) { return rectPts(lx + x0 * sc, b + y0 * sc, lx + x1 * sc, b + y1 * sc); };
        k.shape(R(-0.42f, 0.f, 0.42f, 0.22f), dots(0.4f, 0.09f), 1.4f);
        k.shape(R(-0.13f, 0.22f, 0.13f, 1.05f), lines(0.35f, 0.08f, 0.f), 1.4f);
        k.shape(R(-0.32f, 1.05f, 0.32f, 1.2f), dots(0.4f, 0.09f), 1.4f);
        k.shape(R(-0.26f, 1.2f, 0.26f, 1.72f), solid(), 1.4f);
        k.fill(R(-0.13f, 1.3f, 0.13f, 1.6f), solid(C_PAPER));
        k.shape({{lx - 0.6f * sc, b + 1.72f * sc}, {lx + 0.6f * sc, b + 1.72f * sc}, {lx + 0.18f * sc, b + 2.05f * sc}, {lx - 0.18f * sc, b + 2.05f * sc}},
                solid(), 1.4f);
        k.shape(ellipsePts({lx, b + 2.15f * sc}, 0.1f * sc, 0.12f * sc, 12), solid(), 1.f);
    }
}

} // namespace

Page buildPage(float halfW, float scoreBottom, uint64_t seed) {
    Page pg;
    const float X = std::max(12.6f, halfW - 0.6f); // the page's outer edge
    const float top = 10.5f, bot = -11.1f;
    auto quad = [&](int id, V2 tl, V2 tr, V2 br, V2 bl, bool art) {
        pg.panels[id].c[0] = tl;
        pg.panels[id].c[1] = tr;
        pg.panels[id].c[2] = br;
        pg.panels[id].c[3] = bl;
        pg.panels[id].art = art;
    };
    auto box = [&](int id, float x0, float y0, float x1, float y1, bool art) { quad(id, {x0, y1}, {x1, y1}, {x1, y0}, {x0, y0}, art); };
    box(P_WELL, -5.5f, bot, 5.5f, top, false);
    box(P_HOLD, -11.7f, 5.6f, -5.9f, top, false);
    const float sb = std::min(-5.f, scoreBottom);
    const bool chime = sb - 0.4f - bot >= 2.f; // room for the chime's panel under the score
    box(P_SCORE, -11.7f, chime ? sb : bot, -5.9f, 5.2f, false);
    box(P_C, -11.7f, bot, -5.9f, sb - 0.4f, chime);
    pg.panels[P_C].shown = chime;
    box(P_NEXT, 5.9f, -0.9f, 11.7f, top, false);
    box(P_GAUGE, 5.9f, -5.6f, 11.7f, -1.3f, false);
    box(P_D, 5.9f, bot, 11.7f, -6.0f, true);
    pg.outer = X - 12.1f > 3.f;
    quad(P_A, {-X, top}, {-12.1f, top}, {-12.1f, 3.4f}, {-X, 1.9f}, pg.outer);
    quad(P_B, {-X, 1.5f}, {-12.1f, 3.0f}, {-12.1f, bot}, {-X, bot}, pg.outer);
    quad(P_E, {12.1f, top}, {X, top}, {X, 2.4f}, {12.1f, 3.6f}, pg.outer);
    quad(P_F, {12.1f, 3.2f}, {X, 2.0f}, {X, bot}, {12.1f, bot}, pg.outer);
    for (int id : {P_A, P_B, P_E, P_F}) pg.panels[id].shown = pg.outer;

    Rng r(seed ^ 0x3A9Aull);
    Ink k{pg};
    // panel by panel, alternating sides: the town, the lighthouse, the crossing, the shrine, then the small ones
    if (pg.outer) {
        k.panel = P_A;
        drawRooftops(k, X, r);
        k.panel = P_E;
        drawLighthouse(k, X, r);
        k.panel = P_B;
        drawCrossing(k, X, r);
        k.panel = P_F;
        drawShrine(k, X, r);
    }
    // the chime is drawn for a panel from -5.4 down to the page's foot: fit it to its panel
    k.panel = P_C;
    const size_t c0 = pg.elems.size();
    if (chime) drawChime(k);
    const float cTop = pg.panels[P_C].c[0].y, cK = (cTop - bot) / (-5.4f - bot);
    auto fit = [&](V2& p) { p.y = cTop + (p.y + 5.4f) * cK; };
    for (size_t i = c0; i < pg.elems.size(); i++) {
        Element& e = pg.elems[i];
        for (Fill& f : e.fills) {
            for (V2& p : f.pts) fit(p);
            fit(f.tone.g0);
            fit(f.tone.g1);
        }
        for (Stroke& st : e.strokes)
            for (V2& p : st.pts) fit(p);
        fit(e.pivot);
        fit(e.center);
    }
    k.panel = P_D;
    drawVending(k);
    return pg;
}

} // namespace manga

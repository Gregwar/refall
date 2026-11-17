// MANGA's drops: a real drop takes the page into one of six manga moments, never the same twice in a row (a
// shuffled rotation, seeded per song), and a long drop moves on to another every 32 beats. Each is a full page in
// some color (the drawings may still be in pencil early in a song) and moves calmly, on the bar:
//
// - SPLASH: a full-page splash of color behind focus lines, a huge "DON!" and a rumble of "GO" (manga.cpp).
// - DASH: speed lines streaming past over a soft wash of the sky's and the burst's colors, a big sound effect
//   sliding across with the stream on every bar.
// - PANELS: the outer columns break into slanted panels, each a close-up of the drawings (crosshatched shadows,
//   focus lines), the cuts and close-ups changing on every bar.
// - IMPACT: impact frames on the bar: a bar of the page's negative (the drawings in white lines on a red-tinted
//   ink), three of the red page, focus lines, shock-wave rings and flying shards.
// - SHOJO: a soft pastel wash, screentone flowers blooming on the beats, twinkling sparkles, bubbles and petals.
// - BARRAGE: a warm halftone page, sound effects bursting out every two beats in spiky balloons over blots of
//   color, the older balloons shuddering as a new one bursts in.
//
// Everything is drawn behind the well's and the HUD's panels (painted over it), so the stack and the HUD stay
// readable. REFALL_MANGA_FX=N forces moment N (0 SPLASH .. 5 BARRAGE).
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "universes/manga/manga.hpp"

using manga::V2;

namespace {
float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}
float easeOut(float t) {
    t = saturate(t);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}
float fract(double x) { return (float)(x - std::floor(x)); }
// the dash's stream: leftward, turning around every sixteen beats
float dashDir(double rb) { return ((long)std::floor(rb / 16.0)) % 2 ? 1.f : -1.f; }

constexpr float INK_DONE = 1.1f * 3.f; // an element's inking long done (manga.cpp's INK_S x 3)
constexpr float TOP = 10.5f, BOT = -11.1f, COL_IN = 12.1f; // the page's art columns (art.cpp)

// katakana
const char* K_ZUBA = "\xE3\x82\xBA\xE3\x83\x90\xE3\x83\x83";                        // ズバッ
const char* K_BYUN = "\xE3\x83\x93\xE3\x83\xA5\xE3\x83\xB3";                        // ビュン
const char* K_GOO = "\xE3\x82\xB4\xE3\x82\xA9\xE3\x83\x83";                         // ゴォッ
const char* K_DODODO = "\xE3\x83\x89\xE3\x83\x89\xE3\x83\x89\xE3\x83\x89";          // ドドドド
const char* K_GOGOGO = "\xE3\x82\xB4\xE3\x82\xB4\xE3\x82\xB4\xE3\x82\xB4";          // ゴゴゴゴ
const char* K_DOGOON = "\xE3\x83\x89\xE3\x82\xB4\xE3\x82\xA9\xE3\x83\xB3\xEF\xBC\x81"; // ドゴォン！
const char* K_DON2 = "\xE3\x83\x89\xE3\x83\xB3\xE3\x83\x83";                        // ドンッ
const char* K_KIRAKIRA = "\xE3\x82\xAD\xE3\x83\xA9\xE3\x82\xAD\xE3\x83\xA9";        // キラキラ
const char* K_KIRA = "\xE3\x82\xAD\xE3\x83\xA9\xE3\x83\x83";                        // キラッ
const char* K_DON = "\xE3\x83\x89\xE3\x83\xB3\xEF\xBC\x81";                         // ドン！
const char* K_GO = "\xE3\x82\xB4";                                                  // ゴ
const char* BARRAGE_TEXTS[] = {
    "\xE3\x83\x89\xE3\x83\xB3",                         // ドン
    "\xE3\x83\x90\xE3\x83\xB3",                         // バン
    "\xE3\x83\x89\xE3\x82\xAB\xE3\x83\x83",             // ドカッ
    "\xE3\x82\xAC\xE3\x83\x83",                         // ガッ
    "\xE3\x82\xBA\xE3\x83\x89\xE3\x83\xB3",             // ズドン
    "\xE3\x83\x90\xE3\x82\xAD\xE3\x83\x83",             // バキッ
    "\xE3\x83\x80\xE3\x83\xB3",                         // ダン
    "\xE3\x82\xB4\xE3\x83\x83",                         // ゴッ
    "\xE3\x83\x89\xE3\x82\xAB\xE3\x83\xB3\xEF\xBC\x81", // ドカン！
    "\xE3\x83\x89\xE3\x83\x89\xE3\x83\xB3",             // ドドン
};
constexpr int BARRAGE_COUNT = (int)(sizeof(BARRAGE_TEXTS) / sizeof(BARRAGE_TEXTS[0]));
int letters(const char* s) {
    int n = 0;
    for (const char* p = s; *p; p++)
        if (((unsigned char)*p & 0xC0) != 0x80) n++;
    return n;
}
} // namespace

// =====================================================================================================
// Choosing and running the effects
// =====================================================================================================

int MangaUniverse::pickFx() {
    // a test hook: REFALL_MANGA_FX=N forces moment N (0 SPLASH .. 5 BARRAGE)
    static const int forced = std::getenv("REFALL_MANGA_FX") ? std::atoi(std::getenv("REFALL_MANGA_FX")) : -1;
    if (forced >= 0 && forced < DF_COUNT) return forced;
    for (int tries = 0; tries < 4 * DF_COUNT; tries++) {
        if (bagPos_ >= DF_COUNT) {
            for (int i = 0; i < DF_COUNT; i++) bag_[i] = i;
            for (int i = DF_COUNT - 1; i > 0; i--) std::swap(bag_[i], bag_[rng_.irange(0, i)]);
            bagPos_ = 0;
        }
        const int f = bag_[bagPos_++];
        if (f == lastFx_) continue;
        if (f == DF_PANELS && !page_.outer) continue; // no columns to break into panels
        return f;
    }
    return lastFx_ == DF_SPLASH ? DF_IMPACT : DF_SPLASH;
}

void MangaUniverse::startFx(int fx, double beat) {
    fx_ = lastFx_ = fx;
    fxT_[fx] = 0.f;
    fxBeat0_ = std::floor(beat + 0.3);
    fxStep_ = -1;
    if (fx == DF_SPLASH) rumble_ = 0;
}

// The active effect's beat n (since it started).
void MangaUniverse::fxBeat(int fx, long n, double beat, float bpm) {
    (void)beat;
    const float spb = 60.f / std::max(60.f, bpm);
    const float X = std::max(12.6f, viewH_.x - 0.6f), cx = 0.5f * (COL_IN + X);
    switch (fx) {
    case DF_SPLASH:
        if (n == 0) addSfx(K_DON, {-16.3f, 1.6f}, 3.3f, -0.08f, 1e9f, true, SFX_COLOR, 0.45f).fx = DF_SPLASH;
        if (n > 0 && n % 2 == 0 && fxAmt_[DF_SPLASH] > 0.5f) {
            // the rumble builds up the right column, a letter every two beats
            const int slot = rumble_ % 6;
            const V2 at{slot % 2 ? 18.4f : 15.3f, 7.6f - slot * 2.9f};
            addSfx(K_GO, at, 1.35f, (slot % 2 ? 0.12f : -0.12f), 12.f * spb, false, SFX_BLACK, 0.2f).fx = DF_SPLASH;
            rumble_++;
        }
        break;
    case DF_DASH:
        // a sound effect sliding across a column with the stream on every bar, a big one every other bar
        if (n % 4 == 0) {
            const bool big = n % 8 == 0;
            const float side = (n / 4) % 2 ? 1.f : -1.f;
            Rng r(seed_ ^ (uint64_t)(n * 977 + 13));
            const char* t = big ? (n % 16 == 0 ? K_ZUBA : K_GOO) : K_BYUN;
            const float size = big ? 2.3f : 1.6f;
            const float dir = dashDir((double)n);
            const V2 at{side * cx - dir * 2.f, big ? r.range(-3.f, 4.f) : r.range(-8.5f, 7.5f)};
            Sfx& s = addSfx(t, at, size, -0.06f * dir, 3.6f * spb, false, big ? SFX_COLOR : SFX_BLACK, 0.25f);
            s.vel = {dir * 4.f / (3.6f * spb), 0.f};
            s.fx = DF_DASH;
        }
        break;
    case DF_PANELS:
        if (n % 8 == 0) {
            const bool left = (n / 8) % 2 == 0;
            addSfx(left ? K_DODODO : K_GOGOGO, {(left ? -1.f : 1.f) * (X - 1.1f), 1.0f}, 1.25f, left ? -0.1f : 0.1f, 7.f * spb, true,
                   SFX_BLACK, 0.2f)
                .fx = DF_PANELS;
        }
        break;
    case DF_IMPACT:
        if (n == 0) addSfx(K_DOGOON, {-cx, 1.2f}, 3.1f, -0.1f, 1e9f, true, SFX_WHITE, 0.3f).fx = DF_IMPACT;
        if (n % 8 == 4) addSfx(K_DON2, {cx, n % 16 == 4 ? 5.5f : -4.5f}, 2.3f, 0.12f, 3.5f * spb, false, SFX_WHITE, 0.15f).fx = DF_IMPACT;
        if (n % 4 == 0) {
            // a shock wave out of the well on every bar, shards flying out with it
            const V2 c{0.f, 0.4f};
            rings_.push_back({c, 0.f, 1.1f, 9.f, 30.f});
            rings_.push_back({c, -0.12f, 1.f, 8.f, 24.f});
            const int count = n % 16 == 0 ? 26 : 14;
            for (int i = 0; i < count; i++) {
                Shard s;
                const float a = rng_.range(0.f, TAU);
                const float r0 = rng_.range(10.f, 13.f);
                s.p = {c.x + std::cos(a) * r0 * 1.15f, c.y + std::sin(a) * r0 * 0.8f};
                const float sp = rng_.range(6.f, 17.f);
                s.v = {std::cos(a) * sp * 1.2f, std::sin(a) * sp * 0.8f};
                s.rot = rng_.range(0.f, TAU);
                s.vr = rng_.range(-9.f, 9.f);
                s.size = rng_.range(0.15f, 0.6f);
                s.t = 0.f;
                s.life = rng_.range(0.9f, 1.6f);
                s.kind = rng_.chance(0.3f) ? 1 : 0;
                shards_.push_back(s);
            }
        }
        break;
    case DF_SHOJO:
        if (n == 0) addSfx(K_KIRAKIRA, {-cx - 2.2f, 1.f}, 1.7f, 0.f, 1e9f, true, SFX_THIN, 0.6f).fx = DF_SHOJO;
        if (n % 8 == 4) {
            Rng r(seed_ ^ (uint64_t)(n * 31 + 5));
            addSfx(K_KIRA, {cx + r.range(-1.5f, 1.5f), r.range(-7.f, 7.f)}, 1.3f, r.range(-0.2f, 0.2f), 3.f * spb, false, SFX_THIN, 0.3f).fx =
                DF_SHOJO;
        }
        break;
    case DF_BARRAGE: {
        // one sound effect every two beats in a spiky balloon, the columns in turn, a big one every other bar
        if (n % 2) break;
        const long m = n / 2;
        Rng r(seed_ ^ (uint64_t)(n * 7919 + 3));
        Pop p;
        const bool big = m % 4 == 0;
        const float side = m % 2 ? 1.f : -1.f;
        // the slots of a column in turn, so the balloons of the last bars stay clear
        const float ys[4] = {7.4f, -7.6f, 3.6f, -3.4f};
        p.at = {side * cx + (big ? 0.f : r.range(-0.9f, 0.9f)), big ? r.range(-1.f, 1.5f) : ys[(m / 2) % 4] + r.range(-0.6f, 0.6f)};
        p.size = big ? 2.2f : r.range(1.4f, 1.75f);
        p.text = BARRAGE_TEXTS[(m + (long)(seed_ % 5)) % BARRAGE_COUNT];
        // its balloon fits in the column
        p.size = std::min(p.size, (X - COL_IN - 0.6f) / (0.97f * letters(p.text.c_str()) + 1.5f) * (big ? 1.25f : 1.f));
        p.angle = r.range(-0.22f, 0.22f);
        p.t = 0.f;
        p.life = (big ? 9.f : 7.f) * spb;
        p.kind = big ? 2 : (int)(m / 2 % 2);
        pops_.push_back(p);
        jolt_ = big ? 1.f : 0.5f;
        const float a = r.range(0.f, TAU);
        joltDir_ = {std::cos(a), std::sin(a)};
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Drawing helpers
// =====================================================================================================

// The page's drawings again, for the drop: srcPanel's elements (-1: every art panel's), its point focus (board
// units) shown at centerB, zoom times bigger, into clip slot clipSlot (-1: each in its own panel); inked: fully
// inked and colored whatever the page's progress. cullPx: skip the elements outside this pixel box.
void MangaUniverse::drawArtAt(int srcPanel, int clipSlot, V2 focus, float zoom, V2 centerB, int mode, float alpha, bool inked,
                              const float* cullPx) {
    if (alpha <= 0.002f) return;
    if (elemBox_.size() != page_.elems.size() * 4) {
        elemBox_.assign(page_.elems.size() * 4, 0.f);
        for (size_t i = 0; i < page_.elems.size(); i++) {
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            auto add = [&](const V2& p) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); };
            for (const manga::Fill& f : page_.elems[i].fills)
                for (const V2& p : f.pts) add(p);
            for (const manga::Stroke& s : page_.elems[i].strokes)
                for (const V2& p : s.pts) add(p);
            float* b = &elemBox_[i * 4];
            b[0] = x0 - 0.3f; b[1] = y0 - 0.3f; b[2] = x1 + 0.3f; b[3] = y1 + 0.3f;
        }
    }
    const vec2 c0 = viewC_;
    const float u0 = unitPx_, s0 = s_;
    unitPx_ = u0 * zoom;
    s_ = s0 * std::sqrt(zoom);
    viewC_ = vec2{focus.x - (centerB.x - c0.x) / zoom, focus.y - (centerB.y - c0.y) / zoom};
    artMode_ = mode;
    clipOverride_ = clipSlot;
    for (size_t i = 0; i < page_.elems.size(); i++) {
        const manga::Element& e = page_.elems[i];
        if (!page_.panels[e.panel].art || !page_.panels[e.panel].shown) continue;
        if (srcPanel >= 0 && e.panel != srcPanel) continue;
        if (cullPx) {
            const float* b = &elemBox_[i * 4];
            const vec2 p0 = px({b[0], b[3]}), p1 = px({b[2], b[1]});
            if (p1.x < cullPx[0] || p0.x > cullPx[2] || p1.y < cullPx[1] || p0.y > cullPx[3]) continue;
        }
        const float it = inked ? INK_DONE : (i < inkT_.size() ? inkT_[i] : -1.f);
        drawElement(e, it, alpha, e.anim == manga::A_CHIME ? chime_ : 0.f);
    }
    artMode_ = 0;
    clipOverride_ = -1;
    viewC_ = c0;
    unitPx_ = u0;
    s_ = s0;
}

// A rectangle (board units) in a vertical gradient of washes.
void MangaUniverse::gradRect(float x0, float y0, float x1, float y1, const vec3& top, const vec3& bottom, float a) {
    if (a <= 0.002f) return;
    const vec2 p0 = px({x0, y1}), p1 = px({x1, y0});
    const ToneP t{manga::TONE_WASH, 4.f, 0.f, px({0.f, 0.f}), -1};
    const vec3 ca[3] = {top, top, bottom}, cb[3] = {top, bottom, bottom};
    const float al[3] = {a, a, a};
    triC({p0.x, p0.y}, {p1.x, p0.y}, {p1.x, p1.y}, t, 1.f, ca, al);
    triC({p0.x, p0.y}, {p1.x, p1.y}, {p0.x, p1.y}, t, 1.f, cb, al);
}

// A spiky shout balloon (board units), filled and outlined in ink.
void MangaUniverse::starburst(V2 c, float rx, float ry, int spikes, float inner, uint64_t seed, const vec3& fill, float a, float outlinePx) {
    Rng r(seed);
    std::vector<V2> pts;
    for (int i = 0; i < spikes * 2; i++) {
        const float ang = (i + r.range(-0.25f, 0.25f)) / (float)(spikes * 2) * TAU;
        const float k = i % 2 == 0 ? r.range(0.9f, 1.18f) : inner * r.range(0.92f, 1.06f);
        pts.push_back({c.x + std::cos(ang) * rx * k, c.y + std::sin(ang) * ry * k});
    }
    manga::Tone tn;
    fillPoly(pts, tn, 1.f, fill, a, -1);
    if (outlinePx > 0.f) strokeLine(pts, outlinePx, 0.f, true, 1.f, ink_, a, -1);
}

// A four-pointed sparkle.
void MangaUniverse::sparkle(V2 c, float r, float rot, const vec3& fill, const vec3& line, float a) {
    if (r < 0.02f || a <= 0.002f) return;
    std::vector<V2> pts;
    for (int i = 0; i < 8; i++) {
        const float ang = rot + i * (TAU / 8.f);
        const float k = i % 2 == 0 ? r : r * 0.16f;
        pts.push_back({c.x + std::cos(ang) * k, c.y + std::sin(ang) * k});
    }
    manga::Tone tn;
    fillPoly(pts, tn, 1.f, fill, a, -1);
    strokeLine(pts, 1.1f, 0.f, true, 1.f, line, a, -1);
}

// A screentone flower: two rings of petals in a soft color, a dot tone deepening toward the heart, ink outlines.
void MangaUniverse::flower(V2 c, float r, float rot, float open, const vec3& petal, const vec3& line, float a) {
    if (open <= 0.01f || a <= 0.002f) return;
    r *= open;
    const vec3 deep = shadeOf(petal);
    for (int ring = 0; ring < 2; ring++) {
        const int n = ring ? 5 : 6;
        const float rr = ring ? r * 0.62f : r, w = ring ? 0.36f : 0.32f;
        const vec3 col = ring ? lerp(petal, paper_, 0.35f) : petal;
        for (int p = 0; p < n; p++) {
            const float ang = rot + (p + ring * 0.5f) * TAU / n;
            const V2 d{std::cos(ang), std::sin(ang)}, nn{-d.y, d.x};
            std::vector<V2> pts;
            for (int k = 0; k < 14; k++) {
                const float t = k / 14.f * TAU;
                const float al = 0.5f - 0.5f * std::cos(t); // 0 at the heart, 1 at the tip
                const float ac = std::sin(t) * w * (0.35f + 0.65f * std::sqrt(al));
                pts.push_back(c + d * (al * rr) + nn * (ac * rr));
            }
            manga::Tone tn;
            fillPoly(pts, tn, 1.f, col, a, -1);
            manga::Tone dots;
            dots.kind = manga::TONE_DOTS;
            dots.d0 = 0.42f;
            dots.d1 = 0.f;
            dots.g0 = c;
            dots.g1 = c + d * (rr * 0.8f);
            dots.cell = 0.1f;
            dots.angle = 0.3f;
            fillPoly(pts, dots, 1.f, deep, a, -1);
            strokeLine(pts, 1.3f, 0.f, true, 1.f, line, a, -1);
        }
    }
    std::vector<V2> heart;
    for (int k = 0; k < 12; k++) heart.push_back({c.x + std::cos(k / 12.f * TAU) * r * 0.16f, c.y + std::sin(k / 12.f * TAU) * r * 0.16f});
    manga::Tone tn;
    fillPoly(heart, tn, 1.f, lerp(hue(manga::H_SUN), paper_, 0.2f), a, -1);
    strokeLine(heart, 1.2f, 0.f, true, 1.f, line, a, -1);
}

// =====================================================================================================
// The effects
// =====================================================================================================

// DASH: the page turns into speed lines streaming past over a soft wash of the sky's colors; the stream turns
// around every sixteen beats.
void MangaUniverse::drawDash(float a, float t, double beat) {
    const double rb = std::max(0.0, beat - fxBeat0_);
    const float dir = dashDir(rb);
    const float H = viewH_.y + 0.5f, W = viewH_.x + 0.5f;
    const ToneP solid{manga::TONE_SOLID, 4.f, 0.f, {0, 0}, -1};
    // the ground: the sky's middle wash at the top warming to the burst's glow at the foot, the stream's dot tone
    // thinning toward the middle of the page
    {
        const float mixK = ugl::luma(paper_) < 0.4f ? 0.4f : 0.5f;
        const vec3 top = lerp(paper_, hue(manga::H_SKY_MID), mixK), mid = lerp(paper_, cur_[K_BURST], mixK * 1.4f);
        const vec3 foot = lerp(paper_, cur_[K_BURST + 1], mixK);
        gradRect(-W, 0.5f, W, H, top, mid, a);
        gradRect(-W, -H, W, 0.5f, mid, foot, a);
        manga::Tone tn;
        tn.kind = manga::TONE_LINES;
        tn.d0 = 0.3f;
        tn.d1 = 0.f;
        tn.g0 = {0.f, H};
        tn.g1 = {0.f, 3.f};
        tn.cell = 0.18f;
        tn.angle = 0.f;
        fillPoly({{-W, 2.5f}, {W, 2.5f}, {W, H}, {-W, H}}, tn, 1.f, shadeOf(top), a, -1);
        tn.g0 = {0.f, -H};
        tn.g1 = {0.f, -3.5f};
        fillPoly({{-W, -H}, {W, -H}, {W, -3.f}, {-W, -3.f}}, tn, 1.f, shadeOf(foot), a, -1);
    }
    // streaks of color blurred by the speed: opaque at the front, fading out at the tail
    {
        Rng r(0x57EAull);
        const vec3 cols[4] = {cur_[K_BURST], cur_[K_BURST + 1], cur_[K_BURST + 2], hue(manga::H_SKY_MID)};
        for (int i = 0; i < 26; i++) {
            const float y = r.range(-H, H), hh = r.range(0.15f, 0.9f), L = r.range(8.f, 22.f), v = r.range(6.f, 13.f), ph = r.uniform();
            const vec3 c = lerp(paper_, cols[i % 4], r.range(0.45f, 0.85f));
            const float span = 2.f * W + L;
            const float x = dir < 0.f ? W - std::fmod(ph * span + v * t, span) : -W + std::fmod(ph * span + v * t, span);
            const float xt = x - dir * L;
            const vec2 f0 = px({x, y + hh}), f1 = px({x, y - hh}), t0 = px({xt, y + hh}), t1 = px({xt, y - hh});
            const vec3 cc[3] = {c, c, c};
            const float al0[3] = {0.75f * a, 0.75f * a, 0.f}, al1[3] = {0.75f * a, 0.f, 0.f};
            triC({f0.x, f0.y}, {f1.x, f1.y}, {t1.x, t1.y}, solid, 1.f, cc, al0);
            triC({f0.x, f0.y}, {t1.x, t1.y}, {t0.x, t0.y}, solid, 1.f, cc, al1);
        }
    }
    // the speed lines: blunt at the front, sharp at the tail
    Rng r(0xDA5Bull);
    const int N = 170;
    for (int i = 0; i < N; i++) {
        const float y = r.range(-H, H);
        const float L = r.range(2.5f, 12.f) * (r.chance(0.2f) ? 1.7f : 1.f);
        const float h = r.range(0.025f, 0.14f) * (r.chance(0.12f) ? 2.3f : 1.f);
        const float v = r.range(14.f, 34.f);
        const float ph = r.uniform();
        const float kind = r.uniform(); // ink mostly, some in the burst's deep color, a few in white
        if (i > N * a) continue;
        const float span = 2.f * W + L;
        const float x = dir < 0.f ? W - std::fmod(ph * span + v * t, span) : -W + std::fmod(ph * span + v * t, span);
        const vec3 c = kind < 0.14f ? paper_ : kind < 0.4f ? lerp(cur_[K_BURST + 2], ink_, 0.2f) : ink_;
        manga::Tone tn;
        fillPoly({{x, y + h}, {x - dir * L, y}, {x, y - h}}, tn, 1.f, c, a * 0.9f, -1);
    }
}

// PANELS: the outer columns break into slanted panels, each a close-up of the drawings, cut anew on every bar.
void MangaUniverse::drawPanelBreak(float a, double beat) {
    const double rb = std::max(0.0, beat - fxBeat0_);
    const long step = (long)std::floor(rb / 4.0);
    const float ph = fract(rb / 4.0);
    const float X = std::max(12.6f, viewH_.x - 0.6f);
    // the elements worth a close-up
    std::vector<int> subjects;
    for (size_t i = 0; i < page_.elems.size(); i++) {
        const manga::Element& e = page_.elems[i];
        if (e.pencil && page_.panels[e.panel].art && page_.panels[e.panel].shown && e.strokes.size() >= 2 && e.panel != manga::P_C &&
            e.panel != manga::P_D)
            subjects.push_back((int)i);
    }
    if (subjects.empty()) return;
    flushInk();
    for (int side = 0; side < 2; side++) {
        const float sx = side ? 1.f : -1.f;
        // the columns slide in from the page's edges, and back out
        const float slide = sx * (1.f - easeOut(a)) * (X - COL_IN + 1.f);
        const float xl = side ? COL_IN : -X, xr = side ? X : -COL_IN;
        const float xm = 0.5f * (xl + xr);
        // the gutters' paper over the column
        {
            const V2 q[4] = {{xl + slide - 0.2f, TOP + 0.2f}, {xr + slide + 0.2f, TOP + 0.2f}, {xr + slide + 0.2f, BOT - 0.2f}, {xl + slide - 0.2f, BOT - 0.2f}};
            manga::Tone tn;
            fillPoly({q[3], q[2], q[1], q[0]}, tn, 1.f, paper_, a, -1);
        }
        // the cuts: the last bar's sliding to this bar's over the downbeat
        auto cuts = [&](long s, float* yc, float* sl) {
            Rng r(seed_ ^ (uint64_t)(s * 7919 + side * 104729 + 17));
            yc[0] = r.range(2.f, 6.5f);
            yc[1] = r.range(-6.5f, -2.f);
            sl[0] = r.range(-0.5f, 0.5f);
            sl[1] = r.range(-0.5f, 0.5f);
        };
        float yA[2], sA[2], yB[2], sB[2];
        cuts(step - 1, yA, sA);
        cuts(step, yB, sB);
        const float k = ease(ph / 0.12f);
        float yc[2], sl[2];
        for (int i = 0; i < 2; i++) {
            yc[i] = lerpf(yA[i], yB[i], k);
            sl[i] = lerpf(sA[i], sB[i], k);
        }
        const float g = 0.24f;
        auto cutY = [&](int i, float x) { return yc[i] + sl[i] * (x - xm); };
        V2 frames[3][4];
        for (int j = 0; j < 3; j++) {
            auto yTop = [&](float x) { return j == 0 ? TOP : cutY(j - 1, x) - g; };
            auto yBot = [&](float x) { return j == 2 ? BOT : cutY(j, x) + g; };
            V2 q[4] = {{xl, yTop(xl)}, {xr, yTop(xr)}, {xr, yBot(xr)}, {xl, yBot(xl)}};
            for (V2& p : q) p.x += slide;
            const int slot = manga::P_COUNT + side * 3 + j;
            setClip(slot, q);
            const V2 cen{(q[0].x + q[1].x + q[2].x + q[3].x) * 0.25f, (q[0].y + q[1].y + q[2].y + q[3].y) * 0.25f};
            // its close-up: a subject, zoomed in, slowly pushing in through the bar
            Rng rc(seed_ ^ (uint64_t)(step * 131 + side * 31 + j * 7 + 1));
            const int si = subjects[(size_t)rc.irange(0, (int)subjects.size() - 1)];
            const manga::Element& e = page_.elems[(size_t)si];
            const float zoom = rc.range(1.9f, 3.4f) * (1.f + 0.1f * ph);
            const V2 focus = e.center + V2{rc.range(-0.4f, 0.4f), rc.range(-0.4f, 0.4f)};
            const int overlay = rc.irange(0, 3);
            // drawn under a scissor around the panel (cheap on software rasterizers)
            float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
            for (const V2& p : q) {
                const vec2 pp = px(p);
                bx0 = std::min(bx0, pp.x); by0 = std::min(by0, pp.y); bx1 = std::max(bx1, pp.x); by1 = std::max(by1, pp.y);
            }
            bx0 = std::max(0.f, std::floor(bx0) - 1.f); by0 = std::max(0.f, std::floor(by0) - 1.f);
            bx1 = std::min((float)width_, std::ceil(bx1) + 1.f); by1 = std::min((float)height_, std::ceil(by1) + 1.f);
            if (bx1 <= bx0 || by1 <= by0) continue;
            flushInk();
            glEnable(GL_SCISSOR_TEST);
            glScissor((int)bx0, height_ - (int)by1, (int)(bx1 - bx0), (int)(by1 - by0));
            const float cull[4] = {bx0, by0, bx1, by1};
            drawArtAt(e.panel, slot, focus, zoom, cen, 0, a, true, cull);
            flushInk();
            glDisable(GL_SCISSOR_TEST);
            // a dramatic tone over it
            if (overlay == 1) {
                // a heavy crosshatch shadow coming down from the top
                manga::Tone tn;
                tn.kind = manga::TONE_CROSS;
                tn.d0 = 0.75f;
                tn.d1 = 0.f;
                tn.g0 = {cen.x, std::max(q[0].y, q[1].y)};
                tn.g1 = {cen.x, cen.y - 0.5f};
                tn.cell = 0.16f;
                tn.angle = 0.6f;
                fillPoly({q[3], q[2], q[1], q[0]}, tn, 1.f, ink_, a, slot);
            } else if (overlay == 2) {
                // focus lines converging on the subject
                Rng rl(seed_ ^ (uint64_t)(step * 17 + j + side * 5));
                const float R = 9.f;
                for (float ang = 0.f; ang < TAU;) {
                    ang += rl.range(0.03f, 0.11f);
                    const float rin = rl.range(1.6f, 2.6f);
                    const V2 d{std::cos(ang), std::sin(ang)}, nn{-d.y, d.x};
                    const float half = rl.range(0.03f, 0.11f);
                    manga::Tone tn;
                    fillPoly({cen + d * rin, cen + d * R + nn * half * 4.f, cen + d * R - nn * half * 4.f}, tn, 1.f, ink_, a, slot);
                }
            } else if (overlay == 3) {
                // a gradient of dots rising from the bottom
                manga::Tone tn;
                tn.kind = manga::TONE_DOTS;
                tn.d0 = 0.6f;
                tn.d1 = 0.f;
                tn.g0 = {cen.x, std::min(q[2].y, q[3].y)};
                tn.g1 = {cen.x, cen.y + 0.8f};
                tn.cell = 0.17f;
                tn.angle = 0.785f;
                fillPoly({q[3], q[2], q[1], q[0]}, tn, 1.f, ink_, a, slot);
            }
            for (int c = 0; c < 4; c++) frames[j][c] = q[c];
        }
        // their frames over all three (a close-up's ground may reach past its panel, inside its scissor)
        for (int j = 0; j < 3; j++) inkQuad(frames[j], 3.4f, 0xB4EA0 + (uint64_t)(side * 3 + j), 1.f, ink_, a);
    }
    // the small panels between the HUD's: the burst's colors under a dot tone, so the page reads full
    for (const int i : {(int)manga::P_C, (int)manga::P_D}) {
        const manga::Panel& p = page_.panels[i];
        if (!p.art || !p.shown) continue;
        const float mixK = ugl::luma(paper_) < 0.4f ? 0.5f : 0.6f;
        const vec3 top = lerp(paper_, cur_[K_BURST + 1], mixK), foot = lerp(paper_, cur_[K_BURST + 2], mixK);
        const ToneP t{manga::TONE_WASH, 4.f, 0.f, px({0.f, 0.f}), i};
        const vec2 c0 = px(p.c[0]), c1 = px(p.c[1]), c2 = px(p.c[2]), c3 = px(p.c[3]);
        const vec3 ca[3] = {top, top, foot}, cb[3] = {top, foot, foot};
        const float al[3] = {a, a, a};
        triC({c0.x, c0.y}, {c1.x, c1.y}, {c2.x, c2.y}, t, 1.f, ca, al);
        triC({c0.x, c0.y}, {c2.x, c2.y}, {c3.x, c3.y}, t, 1.f, cb, al);
        manga::Tone tn;
        tn.kind = manga::TONE_DOTS;
        tn.d0 = 0.55f;
        tn.d1 = 0.f;
        tn.g0 = p.c[3];
        tn.g1 = p.c[0];
        tn.cell = 0.17f;
        tn.angle = 0.785f;
        fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, tn, 1.f, shadeOf(foot), a, i);
    }
}

// IMPACT: impact frames on the bar: a bar of the page's negative (tinted with the burst's red), three of the red
// page, focus lines, shock waves and shards.
void MangaUniverse::drawImpact(float a, double beat) {
    const double rb = std::max(0.0, beat - fxBeat0_);
    const long bar = (long)std::floor(rb / 4.0);
    const bool neg = bar % 4 == 0;
    const vec3 red = cur_[K_BURST + 2], line = neg ? paper_ : ink_;
    const vec3 ground = neg ? lerp(ink_, red, 0.3f) : red;
    // the red page glowing toward the burst's middle color at its foot, the negative darkening at its foot
    const vec3 foot = neg ? lerp(ink_, red, 0.12f) : lerp(red, cur_[K_BURST + 1], 0.45f);
    const float H = viewH_.y + 0.5f, W = viewH_.x + 0.5f;
    gradRect(-W, -H, W, H, ground, foot, a);
    drawArtAt(-1, -1, {0.f, 0.f}, 1.f, {0.f, 0.f}, neg ? 1 : 2, a, true, nullptr);
    // a soft flash of the paper as the frame flips
    if (fract(rb / 4.0) < 0.0625f && (bar % 4 == 0 || bar % 4 == 1))
        rectPx(0.f, 0.f, (float)width_, (float)height_, paper_, a * 0.4f * (1.f - fract(rb / 4.0) * 16.f));
    // focus lines in the frame's line color, lunging on the bar
    drawSplash(0.9f * a, (float)(rb / 4.0), line);
    // the shock waves: ellipses spreading out of the well
    for (const Ring& rg : rings_) {
        if (rg.t <= 0.f) continue;
        const float k = rg.t / rg.life;
        const float r = lerpf(rg.r0, rg.r1, easeOut(k));
        std::vector<V2> pts;
        for (int i = 0; i < 72; i++) {
            const float ang = i / 72.f * TAU;
            pts.push_back({rg.c.x + std::cos(ang) * r * 1.15f, rg.c.y + std::sin(ang) * r * 0.8f});
        }
        strokeLine(pts, 3.f + 16.f * (1.f - k), 0.f, true, 1.f, line, a * (1.f - k), -1);
        for (V2& p : pts) p = rg.c + (p - rg.c) * 0.94f;
        strokeLine(pts, 1.5f + 3.f * (1.f - k), 0.f, true, 1.f, line, 0.7f * a * (1.f - k), -1);
    }
    // shards and sparks flying out
    for (const Shard& s : shards_) {
        const float k = s.t / s.life, al = a * (1.f - smoothstepf(0.6f, 1.f, k));
        if (s.kind == 1) {
            sparkle(s.p, s.size * 1.4f, s.rot, line, ground, al);
            continue;
        }
        const float c = std::cos(s.rot), sn = std::sin(s.rot), z = s.size;
        auto P = [&](float x, float y) { return V2{s.p.x + (x * c - y * sn) * z, s.p.y + (x * sn + y * c) * z}; };
        const std::vector<V2> tri{P(1.2f, 0.f), P(-0.6f, 0.55f), P(-0.4f, -0.7f)};
        manga::Tone tn;
        fillPoly(tri, tn, 1.f, line, al, -1);
        strokeLine(tri, 1.6f, 0.f, true, 1.f, ground, al, -1);
    }
}

// SHOJO: a soft pastel page, screentone flowers blooming on the beats, twinkling sparkles, bubbles, petals.
void MangaUniverse::drawShojo(float a, double beat) {
    const double rb = std::max(0.0, beat - fxBeat0_);
    const float t = fxT_[DF_SHOJO];
    const bool dark = ugl::luma(paper_) < 0.4f;
    const float mixK = dark ? 0.3f : 0.42f;
    const vec3 pink = lerp(paper_, vec3(1.f, 0.55f, 0.74f), mixK), lav = lerp(paper_, vec3(0.7f, 0.6f, 1.f), mixK);
    const vec3 cream = lerp(paper_, vec3(1.f, 0.9f, 0.7f), mixK * 0.8f);
    const vec3 light = dark ? ink_ : paper_; // the sparkles' and the bubbles' highlights
    const float H = viewH_.y + 0.5f, W = viewH_.x + 0.5f;
    gradRect(-W, 1.f, W, H, lav, pink, 0.9f * a);
    gradRect(-W, -H, W, 1.f, pink, cream, 0.9f * a);
    // soft screentone gradients at the top and the foot of the page
    {
        manga::Tone tn;
        tn.kind = manga::TONE_DOTS;
        tn.d0 = 0.45f;
        tn.d1 = 0.f;
        tn.g0 = {0.f, H};
        tn.g1 = {0.f, 4.f};
        tn.cell = 0.16f;
        tn.angle = 0.26f;
        fillPoly({{-W, 3.5f}, {W, 3.5f}, {W, H}, {-W, H}}, tn, 1.f, shadeOf(lav), a, -1);
        tn.g0 = {0.f, -H};
        tn.g1 = {0.f, -5.f};
        fillPoly({{-W, -H}, {W, -H}, {W, -4.5f}, {-W, -4.5f}}, tn, 1.f, shadeOf(pink), a, -1);
    }
    const float X = std::max(12.6f, viewH_.x - 0.6f);
    const float cx = 0.5f * (COL_IN + X), hw = 0.5f * (X - COL_IN);
    // bubbles rising
    {
        Rng r(0xB0BB1Eull);
        for (int i = 0; i < 26; i++) {
            const float side = i % 2 ? 1.f : -1.f;
            const float x0 = side * cx + r.range(-hw, hw) * 1.1f, sp = r.range(0.5f, 1.3f), ph = r.range(0.f, 24.f);
            const float rad = r.range(0.2f, 0.65f);
            const float y = BOT - 1.f + std::fmod(ph + t * sp, 24.f);
            const V2 c{x0 + 0.35f * std::sin(t * sp * 1.3f + ph), y};
            std::vector<V2> pts;
            for (int k = 0; k < 20; k++) pts.push_back({c.x + std::cos(k / 20.f * TAU) * rad, c.y + std::sin(k / 20.f * TAU) * rad});
            manga::Tone tn;
            fillPoly(pts, tn, 1.f, paper_, 0.3f * a, -1);
            strokeLine(pts, 1.1f, 0.f, true, 1.f, ink_, 0.75f * a, -1);
            std::vector<V2> hl;
            for (int k = 0; k < 6; k++) {
                const float ang = 1.9f + k * 0.16f;
                hl.push_back({c.x + std::cos(ang) * rad * 0.7f, c.y + std::sin(ang) * rad * 0.7f});
            }
            strokeLine(hl, 2.2f, 0.3f, false, 1.f, light, a, -1);
        }
    }
    // the flowers, one blooming on each half beat, slowly turning
    {
        struct F { float x, y, r; };
        const F fl[6] = {{0.15f, 8.6f, 2.3f}, {-0.85f, 4.4f, 1.5f}, {0.55f, -0.6f, 1.9f}, {-0.75f, -6.4f, 2.5f}, {0.7f, -9.8f, 1.6f}, {-0.1f, 1.9f, 1.15f}};
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < 6; i++) {
                const int order = side ? (i * 5 + 3) % 6 : i;
                const float sx = side ? 1.f : -1.f;
                const V2 c{sx * (cx + fl[i].x * hw * (side ? -1.f : 1.f)), side ? -fl[i].y * 0.9f + 0.6f : fl[i].y};
                const float open = easeOut((float)(rb - (order * 2 + side) * 0.5) / 1.5f);
                const vec3 col = i % 3 == 1 ? lerp(lav, vec3(0.75f, 0.6f, 1.f), 0.5f) : lerp(pink, vec3(1.f, 0.5f, 0.7f), 0.55f);
                flower(c, fl[i].r, 0.4f * i + sx * 0.18f * t, open * (0.9f + 0.1f * a), col, ink_, a);
            }
    }
    // petals swirling down
    {
        Rng r(0x9E7A2ull);
        const vec3 pc = lerp(pink, vec3(1.f, 0.5f, 0.72f), 0.6f);
        for (int i = 0; i < 44; i++) {
            const float side = i % 2 ? 1.f : -1.f;
            const float ph = r.range(0.f, TAU), w = r.range(0.5f, 1.1f) * (r.chance(0.5f) ? 1.f : -1.f);
            const float orb = r.range(1.2f, hw * 1.1f), fall = r.range(0.5f, 1.1f), ph2 = r.range(0.f, 24.f);
            const float ang = ph + t * w;
            const V2 c{side * cx + std::cos(ang) * orb, TOP + 1.f - std::fmod(ph2 + t * fall, 24.f) + std::sin(ang) * orb * 0.4f};
            const float ro = ang * 1.7f, l = 0.24f, wd = 0.11f;
            std::vector<V2> pts;
            for (int k = 0; k < 10; k++) {
                const float tt = k / 10.f * TAU;
                const float lx = std::cos(tt) * l, ly = std::sin(tt) * wd * (1.f - 0.4f * std::cos(tt));
                pts.push_back({c.x + lx * std::cos(ro) - ly * std::sin(ro), c.y + lx * std::sin(ro) + ly * std::cos(ro)});
            }
            manga::Tone tn;
            fillPoly(pts, tn, 1.f, pc, a, -1);
            strokeLine(pts, 1.f, 0.f, true, 1.f, ink_, 0.85f * a, -1);
        }
    }
    // sparkles twinkling on the beats
    {
        Rng r(0x5A1C1ull);
        for (int i = 0; i < 40; i++) {
            const float side = i % 2 ? 1.f : -1.f;
            const V2 c{side * (cx + r.range(-hw, hw) * 1.15f), r.range(BOT + 0.5f, TOP - 0.3f)};
            const float base = r.range(0.25f, 0.6f) * (i % 7 == 0 ? 2.f : 1.f);
            const float q = (float)std::fmod(rb + 400.0 - (i % 4), 4.0);
            const float k = std::exp(-q * 1.6f);
            const float rr = base * (0.2f + 0.8f * k) * a;
            discPx(px(c).x, px(c).y, rr * unitPx_ * 0.9f, light, 0.55f * k * a);
            sparkle(c, rr, 0.f, light, dark ? paper_ : ink_, a);
            if (i % 7 == 0) sparkle(c, rr * 0.55f, PI * 0.25f, light, dark ? paper_ : ink_, a);
        }
    }
}

// BARRAGE: sound effects bursting out every two beats in spiky balloons over a warm halftone page, the balloons
// jolting as a new one bursts in.
void MangaUniverse::drawBarrage(float a, double beat) {
    (void)beat;
    const float H = viewH_.y + 0.5f, W = viewH_.x + 0.5f;
    // the ground: the burst's pale center at the top warming to its middle color at the foot, a halftone of dots
    // swelling toward the page's edges, pulsing a little as a balloon bursts in
    {
        const float mixK = ugl::luma(paper_) < 0.4f ? 0.5f : 0.72f;
        const vec3 top = lerp(paper_, cur_[K_BURST], std::min(1.f, mixK * 1.3f)), foot = lerp(paper_, cur_[K_BURST + 1], mixK);
        gradRect(-W, -H, W, H, top, foot, a);
        const vec3 dot = shadeOf(lerp(top, foot, 0.5f));
        for (int side = 0; side < 2; side++) {
            const float sx = side ? 1.f : -1.f, x0 = side ? 0.f : -W, x1 = side ? W : 0.f;
            manga::Tone tn;
            tn.kind = manga::TONE_DOTS;
            tn.d0 = 0.5f + 0.08f * jolt_;
            tn.d1 = 0.f;
            tn.g0 = {sx * W, 0.f};
            tn.g1 = {sx * (COL_IN - 1.f), 0.f};
            tn.cell = 0.17f;
            tn.angle = 0.785f;
            fillPoly({{x0, -H}, {x1, -H}, {x1, H}, {x0, H}}, tn, 1.f, dot, 0.7f * a, -1);
        }
    }
    // short focus lines onto a new big balloon, softly, for a moment
    if (!pops_.empty() && pops_.back().kind == 2 && pops_.back().t < 0.35f) {
        const Pop& p = pops_.back();
        const float al = 0.35f * a * (1.f - ease(p.t / 0.35f));
        Rng r(0xF0C1ull + (uint64_t)pops_.size());
        for (float ang = 0.f; ang < TAU;) {
            ang += r.range(0.04f, 0.12f);
            const V2 d{std::cos(ang), std::sin(ang)}, nn{-d.y, d.x};
            const float rin = p.size * r.range(3.f, 4.2f), half = r.range(0.03f, 0.12f);
            manga::Tone tn;
            fillPoly({p.at + d * rin, p.at + d * 14.f + nn * half * 4.f, p.at + d * 14.f - nn * half * 4.f}, tn, 1.f, ink_, al, -1);
        }
    }
}

// BARRAGE's balloons, over the drawing panels' frames (they burst across them) and under the HUD's panels.
void MangaUniverse::drawBalloons(float a) {
    // the older balloons shudder a little as a new one bursts in
    const V2 shake{joltDir_.x * 0.14f * jolt_, joltDir_.y * 0.14f * jolt_};
    for (const Pop& pop : pops_) {
        Pop p = pop;
        if (&pop != &pops_.back()) p.at = p.at + shake;
        const float al = a * (1.f - smoothstepf(p.life - 0.3f, p.life, p.t));
        if (al <= 0.002f) continue;
        const float in = easeOut(p.t / 0.18f);
        const float sc = in * (1.f + 0.15f * (1.f - easeOut(p.t / 0.4f)));
        const float n = (float)letters(p.text.c_str());
        const float rx = (0.5f * n * p.size * 0.97f + p.size * 0.75f) * sc, ry = p.size * 1.15f * sc;
        const vec3 fill = p.kind == 2 ? ink_ : p.kind == 1 ? cur_[K_BURST + 1] : paper_;
        const uint64_t seed = (uint64_t)(pop.at.x * 1000.f) ^ (uint64_t)(pop.at.y * 777.f);
        // a big blot of the burst's deep color behind it, a paler one around that
        starburst(p.at, rx * 1.7f, ry * 1.9f, 7 + (int)n, 0.8f, seed ^ 0xB10Bull, lerp(paper_, cur_[K_BURST + 2], 0.25f), 0.8f * al, 0.f);
        starburst(p.at, rx * 1.35f, ry * 1.45f, 7 + (int)n, 0.8f, seed ^ 0xB10Cull, lerp(paper_, cur_[K_BURST + 2], 0.5f), 0.8f * al, 0.f);
        // accent lines around it
        Rng r(seed ^ 0x51ull);
        for (int i = 0; i < 12; i++) {
            const float ang = i / 12.f * TAU + r.range(-0.15f, 0.15f);
            const float r0 = 1.25f + r.range(0.f, 0.2f), r1 = r0 + r.range(0.35f, 0.7f);
            strokeLine({{p.at.x + std::cos(ang) * rx * r0, p.at.y + std::sin(ang) * ry * r0}, {p.at.x + std::cos(ang) * rx * r1, p.at.y + std::sin(ang) * ry * r1}},
                       2.2f, 0.4f, false, 1.f, ink_, al, -1);
        }
        starburst(p.at, rx, ry, 9 + (int)n * 2, 0.74f, seed, fill, al, 2.6f);
        Sfx s{p.text, p.at, p.size * sc, p.angle, p.t, p.life, 0.12f, false, p.kind == 2 ? SFX_WHITE : p.kind == 1 ? SFX_OUTLINED : SFX_COLOR};
        drawSfx(s, al);
    }
}

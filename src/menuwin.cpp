// The menu: a kaleidoscope of the game's real universes. The focused song's live universe is folded into a slowly
// turning stained-glass kaleidoscope; the 12 stages are glass petals in a rosette around a dark glass medallion, each
// opening onto its 5 songs as smaller facets; focus turns the rosette to bring the chosen petal to the reading position
// (the right).
// Live universes come from a WorldPool (menuworlds.hpp): small frames, their own frame rate, snapshots otherwise.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

#include "library.hpp"
#include "menu.hpp"
#include "universes/registry.hpp"
#include "menuworlds.hpp"

namespace {
using F = Painter::Font;
float near1(float a, int i) { return std::max(0.f, 1.f - std::fabs(a - (float)i)); }
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
std::string two(int n) { char b[16]; std::snprintf(b, sizeof(b), "%02d", n); return b; }
std::string roman(int n) {
    static const char* R[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV"};
    return n >= 1 && n <= 15 ? R[n - 1] : std::to_string(n);
}
std::string upper(std::string s) { for (char& c : s) c = (char)std::toupper((unsigned char)c); return s; }
std::string fmt1(float v) { char b[16]; std::snprintf(b, sizeof(b), "%.1f", v); return b; }
// the kaleidoscope's crossfade between sources (one menu at a time)
std::string g_cur, g_prev;
float g_mix = 1.f;
double g_lastT = 0;
} // namespace

struct Worlds {
    Menu& m;
    Painter& p;
    WorldPool& pool;
    float u, t;
    Worlds(Menu& mm) : m(mm), p(mm.paint_), pool(*mm.worlds_), u(mm.u_), t(mm.time_) {}
    float X(float v) const { return m.X(v); }
    float Y(float v) const { return m.Y(v); }
    int nStages() const { return m.camp_ ? m.camp_->size() : 0; }
    int nSongs(int s) const { return (int)m.camp_->stages()[s].songs.size(); }
    std::string spec(int s, int k) const {
        if (!m.camp_ || s < 0 || s >= nStages() || k < 0 || k >= nSongs(s)) return "VECTOR";
        const std::string& sp = m.camp_->stages()[s].songs[k].universe;
        return sp.empty() ? "VECTOR" : sp;
    }
    bool open(int s) const { return !m.progress_ || m.progress_->unlocked(s); }
    bool beaten(int s, int k) const { return m.progress_ && m.progress_->beaten(s, k, m.diff_); }
    bool cleared(int s) const { return m.progress_ && m.progress_->cleared(s, m.diff_); }
    // a song's colour: its stage's hue, in a gradient across the stage's songs (song 1 lighter and a touch warmer,
    // song 5 deeper and a touch cooler), so a stage reads as one family
    Col songCol(int s, int k) const {
        const Col c = stageCol(s);
        float mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b}), h = 0.f;
        const float d = mx - mn;
        if (d > 1e-5f) {
            if (mx == c.r) h = std::fmod((c.g - c.b) / d + 6.f, 6.f);
            else if (mx == c.g) h = (c.b - c.r) / d + 2.f;
            else h = (c.r - c.g) / d + 4.f;
            h /= 6.f;
        }
        const float sat = mx > 0.f ? d / mx : 0.f;
        const float u = nSongs(s) > 1 ? (float)k / (float)(nSongs(s) - 1) : 0.5f;   // 0 .. 1 along the stage
        const float hh = std::fmod(h - 0.035f + 0.07f * u + 1.f, 1.f);
        const float ss = std::clamp(sat * (0.6f + 0.55f * u), 0.f, 1.f);
        const float vv = std::clamp(mx * (1.08f - 0.3f * u), 0.f, 1.f);
        const float hf = hh * 6.f, f = hf - std::floor(hf);
        const float P = vv * (1 - ss), Q = vv * (1 - ss * f), T = vv * (1 - ss * (1 - f));
        switch ((int)std::floor(hf) % 6) {
        case 0: return {vv, T, P, 1.f};
        case 1: return {Q, vv, P, 1.f};
        case 2: return {P, vv, T, 1.f};
        case 3: return {P, Q, vv, 1.f};
        case 4: return {T, P, vv, 1.f};
        default: return {vv, P, Q, 1.f};
        }
    }
    Col stageCol(int s) const { unsigned char c[3]; m.stageColor(s, c); return rgb8(c); }
    float focusStage(int s) const { return m.aRow_ > 0.5f ? sstep(near1(m.aStage_, s)) * std::min(1.f, m.aRow_) : 0.f; }
    int focusedStage() const { return std::clamp((int)std::lround(m.aStage_), 0, std::max(0, nStages() - 1)); }
    // all the campaign's universes, in order (the title's slow tour)
    std::vector<std::string> tour() const {
        std::vector<std::string> v;
        for (int s = 0; s < nStages(); s++)
            for (int k = 0; k < nSongs(s); k++) v.push_back(spec(s, k));
        if (v.empty()) v.push_back("VECTOR");
        return v;
    }
    // the tour's current universe, the next, and how far the crossfade between them is
    void tourAt(float period, std::string& cur, std::string& next, float& mix) const {
        const auto v = tour();
        const int i = (int)(t / period);
        cur = v[i % v.size()];
        next = v[(i + 1) % v.size()];
        mix = sstep((std::fmod(t, period) - (period - 1.6f)) / 1.6f);
    }

    void wants();
    void draw();
    // pieces
    void info(F head, F body, Col ink, Col dim, Col accent, int card);
    void tabs(F f, Col ink, Col accent, float track);
    void menuItems(F f, float y0, float step, float size, Col ink, Col accent, float track);
    void kalBackground(const std::string& cur);
    void kite(float cx, float cy, float a, float r0, float r1, float halfAng, Col c, float alpha, bool lead, Col leadC);
    void kalTitle();
    void kalFree();
    void kalCampaign();
};

void Menu::renderWorlds(int w, int h) {
    if (!worlds_) worlds_ = std::make_unique<WorldPool>();
    // a steady made-up beat for the live universes, the preview's loudness on top
    static MusicState mu;
    mu.hasSong = true;
    mu.bpm = 112.f;
    mu.beat = time_ * 112.0 / 60.0;
    mu.beatPhase = (float)(mu.beat - std::floor(mu.beat));
    mu.intensity = 0.45f + 0.35f * level_;
    mu.energy = 0.5f + 0.3f * level_;
    mu.beatPulse = 0.15f * (1.f - mu.beatPhase);
    mu.kick = 0.f;
    for (int b = 0; b < NUM_BANDS; b++) mu.audio.bands[b] = mu.bandsFast[b] = 0.35f + 0.25f * level_;
    mu.audio.bass = mu.audio.mid = mu.audio.high = mu.audio.loud = 0.4f + 0.3f * level_;
    Worlds wd(*this);
    wd.wants();
    const float dt = (float)std::clamp(time_ - g_lastT, 0.0, 0.1);
    g_lastT = time_;
    worlds_->update(time_, dt, mu);
    if (std::getenv("ZEN_MENU_STATS") && (worlds_->made || worlds_->snapped))
        std::printf("[menu] worlds: %d made (%.0f ms), %d frames (%.1f ms), %d snapshots\n", worlds_->made, worlds_->makeMs,
                    worlds_->rendered, worlds_->renderMs, worlds_->snapped);
    paint_.begin(w, h);
    wd.draw();
}

// ---------------------------------------------------------------------------------------------------------------------
// what is on screen: which universes, live at what rate

void Worlds::wants() {
    if (m.screen_ == Menu::TITLE || m.screen_ == Menu::FREE) {
        std::string cur, next;
        float mix;
        tourAt(11.f, cur, next, mix);
        pool.want(cur, 30);
        if (mix > 0.f || std::fmod(t, 11.f) > 11.f - 3.f) pool.want(next, 30);
        else pool.want(next, 0);
        return;
    }
    if (nStages() == 0) return;
    const int fs = focusedStage();
    const std::string cur = spec(fs, m.row_ == 1 ? m.song_ : 0);
    pool.want(cur, 30);
    if (!g_prev.empty() && g_mix < 1.f) pool.want(g_prev, 30);
    for (int k = 0; k < nSongs(fs); k++) pool.want(spec(fs, k), 0); // the next ones are ready to fold in
}

void Worlds::draw() {
    if (m.screen_ == Menu::TITLE) kalTitle();
    else if (m.screen_ == Menu::FREE) kalFree();
    else kalCampaign();
}

// ---------------------------------------------------------------------------------------------------------------------
// shared pieces

void Worlds::tabs(F f, Col ink, Col accent, float track) {
    p.text(f, "CAMPAIGN", X(80), Y(92), 22 * u, withA(ink, 0.8f), -1, track + 0.2f);
    const float cx = X(1180), sp = 220 * u, y = Y(90), on = 1.f - std::min(1.f, m.aRow_);
    const float hx = cx + (m.aDiff_ - 1) * sp;
    p.line(hx - 56 * u, y + 16 * u, hx + 56 * u, y + 16 * u, (1.5f + 1.5f * on) * u, withA(accent, 0.6f + 0.4f * on));
    if (on > 0.05f) { p.setAdditive(true); p.glow(hx, y + 16 * u, 80 * u, withA(accent, 0.25f * on)); p.setAdditive(false); }
    for (int d = 0; d < DIFF_COUNT; d++)
        p.text(f, difficultyName(d), cx + (d - 1) * sp, y, 21 * u, withA(ink, 0.45f + 0.55f * near1(m.aDiff_, d)), 0, track);
}

void Worlds::menuItems(F f, float y0, float step, float size, Col ink, Col accent, float track) {
    static const char* ITEMS[3] = {"CAMPAIGN", "FREE PLAY", "QUIT"};
    const float cx = X(960);
    float wsel = 0;
    for (int i = 0; i < 3; i++) wsel += near1(m.aTitle_, i) * p.width(f, ITEMS[i], size * u, track);
    const float ys = Y(y0 + m.aTitle_ * step) + 14 * u;
    p.line(cx - wsel / 2, ys, cx + wsel / 2, ys, 1.6f * u, withA(accent, 0.9f));
    p.setAdditive(true);
    p.glow(cx - wsel / 2 - 28 * u, ys - size * 0.4f * u, 22 * u, withA(accent, 0.8f));
    p.glow(cx + wsel / 2 + 28 * u, ys - size * 0.4f * u, 22 * u, withA(accent, 0.8f));
    p.setAdditive(false);
    for (int i = 0; i < 3; i++)
        p.text(f, ITEMS[i], cx, Y(y0 + i * step), size * u, withA(ink, 0.45f + 0.55f * near1(m.aTitle_, i)), 0, track);
}

void Worlds::info(F head, F body, Col ink, Col dim, Col accent, int card) {
    const float x0 = X(60), y0 = Y(838), w = 1800 * u, h = 212 * u;
    if (card == 0) {
        p.rrect(x0 - 60 * u, y0 + 10 * u, w + 120 * u, h + 80 * u, 0, hex(0x000000, 0.6f), 60 * u);
        p.line(x0, y0, x0 + w, y0, 1 * u, withA(ink, 0.18f));
    } else {
        p.rrect(x0, y0 + 10 * u, w, h, 22 * u, hex(0x000000, 0.4f), 30 * u);
        p.rrect(x0, y0, w, h, 22 * u, hex(0x0c0c14, 0.72f));
        p.rrectLine(x0, y0, w, h, 22 * u, 1.2f * u, hex(0xffffff, 0.22f));
    }
    const Menu::SongView v = m.songView();
    const float lx = x0 + 44 * u, rx = x0 + w - 44 * u;
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %s   /   SONG %d OF %d", two(v.stage + 1).c_str(), v.song + 1, v.songs);
        p.text(body, b, lx, y0 + 46 * u, 17 * u, dim, -1, 0.3f);
        std::string title = v.title;
        while (p.width(head, title, 42 * u) > 1150 * u && title.size() > 4) title.pop_back();
        p.text(head, title, lx, y0 + 108 * u, 42 * u, ink, -1, 0.01f);
        const float uw = p.text(body, upper(v.universe), lx, y0 + 160 * u, 19 * u, mixc(m.screen_ == Menu::CAMPAIGN ? songCol(v.stage, v.song) : rgb8(v.col), hex(0xffffff), 0.2f), -1, 0.3f);
        p.text(body, "VELOCITY " + fmt1(v.velocity), lx + uw + 44 * u, y0 + 160 * u, 19 * u, dim, -1, 0.3f);
        if (v.beaten) std::snprintf(b, sizeof(b), "BEATEN    BEST %d", v.best);
        else std::snprintf(b, sizeof(b), "NOT BEATEN ON %s", difficultyName(m.diff_));
        p.text(body, b, rx, y0 + 46 * u, 18 * u, v.beaten ? hex(0xffd27a) : dim, 1, 0.3f);
        p.text(body, upper(v.preview), rx, y0 + 108 * u, 17 * u, withA(accent, 0.95f), 1, 0.3f);
    } else if (m.screen_ == Menu::CAMPAIGN && m.row_ == 1 && nStages() > 0) { // a petal, closed
        const int fs = focusedStage();
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %d OF %d   /   %d SONGS", fs + 1, nStages(), nSongs(fs));
        p.text(body, b, lx, y0 + 46 * u, 17 * u, dim, -1, 0.3f);
        if (!open(fs)) p.text(head, "Locked", lx, y0 + 108 * u, 38 * u, ink, -1, 0.01f);
        int beat = 0;
        for (int k = 0; k < nSongs(fs); k++) beat += beaten(fs, k);
        std::snprintf(b, sizeof(b), "%d / %d SONGS BEATEN ON %s", beat, nSongs(fs), difficultyName(m.diff_));
        p.text(body, b, lx, y0 + 160 * u, 17 * u, dim, -1, 0.3f);
        if (cleared(fs)) {
            std::snprintf(b, sizeof(b), "CLEARED    BEST %d", m.progress_->stageBest(fs, m.diff_));
            p.text(body, b, rx, y0 + 46 * u, 18 * u, hex(0xffd27a), 1, 0.3f);
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// KALEIDO

void Worlds::kalBackground(const std::string& cur) {
    // a source change folds in over 1.2 s
    if (cur != g_cur) {
        g_prev = g_cur;
        g_cur = cur;
        g_mix = g_prev.empty() ? 1.f : 0.f;
    }
    g_mix = std::min(1.f, g_mix + 1.f / 72.f);
    unsigned char c[3];
    menuUniverseColor(m.camp_, cur, c);
    Col tint = rgb8(c);
    if (m.screen_ == Menu::CAMPAIGN) tint = songCol(focusedStage(), m.row_ == 1 ? m.song_ : 0);   // the stage's hue
    const GLuint a = pool.tex(g_prev.empty() ? g_cur : g_prev), b = pool.tex(g_cur);
    if (!a && !b) { p.rrect(0, 0, m.fw_, m.fh_, 0, hex(0x0a0a12)); return; }
    p.kaleido(a ? a : b, b ? b : a, g_prev.empty() ? 1.f : sstep(g_mix), X(720), Y(440), t * 0.025f + m.aStage_ * (TAU / 12.f) * 0.5f, 12,
              mixc(tint, hex(0xffffff), 0.3f), t, m.level_);
}

void Worlds::kite(float cx, float cy, float a, float r0, float r1, float halfAng, Col c, float alpha, bool lead, Col leadC) {
    const float rm = lerpf(r0, r1, 0.45f);
    const float ix = cx + std::cos(a) * r0, iy = cy + std::sin(a) * r0;
    const float ox = cx + std::cos(a) * r1, oy = cy + std::sin(a) * r1;
    const float lx = cx + std::cos(a - halfAng) * rm, ly = cy + std::sin(a - halfAng) * rm;
    const float rx = cx + std::cos(a + halfAng) * rm, ry = cy + std::sin(a + halfAng) * rm;
    const Col inner = withA(mixc(c, hex(0x000000), 0.45f), alpha), outer = withA(mixc(c, hex(0xffffff), 0.35f), alpha), side = withA(c, alpha);
    p.tri(ix, iy, lx, ly, ox, oy, inner, side, outer);
    p.tri(ix, iy, ox, oy, rx, ry, inner, outer, withA(mixc(c, hex(0x000000), 0.15f), alpha));
    if (lead) {
        const float w = 2.6f * u;
        p.line(ix, iy, lx, ly, w, leadC); p.line(lx, ly, ox, oy, w, leadC);
        p.line(ox, oy, rx, ry, w, leadC); p.line(rx, ry, ix, iy, w, leadC);
        p.line(ix, iy, ox, oy, 1 * u, withA(leadC, leadC.a * 0.5f));
    }
}

void Worlds::kalCampaign() {
    const int fs = focusedStage();
    const float ao = sstep(m.aOpen_); // the opened petal, 0 closed .. 1 open
    kalBackground(spec(fs, m.petal_ ? m.song_ : 0));
    const int n = nStages();
    const float cx = X(720), cy = Y(440);
    // the rosette turns so that the focused stage sits at the reading position (the right)
    const float base = -m.aStage_ * TAU / std::max(1, n);
    auto ang = [&](float s) { return base + s * TAU / n; };
    p.circle(cx, cy, 300 * u, hex(0x000000, 0.25f), 90 * u);
    // the order of the journey: an arc from petal to petal (1 -> 12), gold as far as the stages are open
    int nextStage = -1;
    for (int s = 0; s < n && nextStage < 0; s++)
        if (open(s) && !(m.progress_ && m.progress_->clearedAny(s))) nextStage = s;
    {
        const float r = 296 * u;
        for (int s = 0; s + 1 < n; s++) {
            const bool o = open(s + 1);
            const float a0 = ang((float)s) + 0.06f, a1 = ang((float)s + 1) - 0.06f;
            if (o) p.arc(cx, cy, r, r, a0, a1, 2.2f * u, hex(0xe8c06a, 0.7f));
            else
                for (int i = 0; i < 6; i += 2)
                    p.arc(cx, cy, r, r, lerpf(a0, a1, i / 6.f), lerpf(a0, a1, (i + 1) / 6.f), 1.5f * u, hex(0xb0b0c0, 0.3f));
            // a chevron in the middle points the way
            const float am = (a0 + a1) * 0.5f, mx = cx + std::cos(am) * r, my = cy + std::sin(am) * r;
            const float tx = -std::sin(am), ty = std::cos(am), nx = std::cos(am), ny = std::sin(am), k = 7 * u;
            const Col cc = o ? hex(0xffe0a0, 0.95f) : hex(0xb0b0c0, 0.35f);
            p.line(mx - tx * k + nx * k, my - ty * k + ny * k, mx + tx * k * 0.6f, my + ty * k * 0.6f, 2.2f * u, cc);
            p.line(mx - tx * k - nx * k, my - ty * k - ny * k, mx + tx * k * 0.6f, my + ty * k * 0.6f, 2.2f * u, cc);
        }
    }
    for (int s = 0; s < n; s++) {
        const float a = ang((float)s);
        const bool o = open(s), cl = o && cleared(s);
        const float f = focusStage(s);
        const float op = s == fs ? ao : 0.f; // this petal's opening
        const Col c = o ? stageCol(s) : hex(0x5a5c66);
        const bool focusClosed = s == fs && m.aRow_ > 0.5f && !m.petal_;
        const float pulse = 0.75f + 0.25f * std::sin(t * 2.4f);
        if (f > 0.05f) {
            p.setAdditive(true);
            p.glow(cx + std::cos(a) * 205 * u, cy + std::sin(a) * 205 * u, (110 + 60 * op) * u, withA(c, 0.25f * f));
            p.setAdditive(false);
        }
        kite(cx, cy, a, (135 + 10 * op) * u, (262 + 22 * f + 26 * op) * u, (0.2f + 0.03f * f + 0.1f * op), mixc(c, hex(0xffffff), 0.12f * op),
             o ? 0.88f : 0.6f, true, focusClosed ? hex(0xffffff, 0.95f * pulse) : cl ? hex(0xe8c06a, 0.95f) : hex(0x08080c, 0.9f));
        if (cl) { // a shine sweeping slowly across a cleared shard
            const float k = std::fmod(t * 0.18f + s * 0.29f, 1.f), r = lerpf(150.f, 255.f, k) * u, hw = 0.16f * (1.f - std::fabs(k - 0.45f));
            p.setAdditive(true);
            p.line(cx + std::cos(a - hw) * r, cy + std::sin(a - hw) * r, cx + std::cos(a + hw) * r, cy + std::sin(a + hw) * r, 5 * u, hex(0xffffff, 0.35f * std::sin(k * PI)), 6 * u);
            p.setAdditive(false);
        }
        // the songs: five small dots beyond a closed petal; the opened petal fans its five facets out
        for (int k = 0; k < nSongs(s) && k < 5; k++) {
            const bool b = o && beaten(s, k);
            const Col sc = o ? songCol(s, k) : hex(0x4a4c55);
            if (op < 0.98f) {
                const float da = 0.042f * (k - 2), r = 278 * u;
                p.circle(cx + std::cos(a + da) * r, cy + std::sin(a + da) * r, 4.2f * u, withA(b ? sc : mixc(sc, hex(0x101018), 0.5f), (1.f - op) * (o ? 1.f : 0.6f)));
            }
            if (op > 0.02f) {
                const float da = 0.14f * (k - 2) * op;
                const float r0 = (300 + 18 * op) * u, r1 = r0 + 74 * u * op;
                kite(cx, cy, a + da, r0, r1, 0.055f * op, b ? sc : mixc(sc, hex(0x101018), 0.4f), 0.95f * op, true, hex(0x08080c, 0.9f * op));
                if (b) {
                    const float r = (r0 + r1) * 0.5f;
                    p.setAdditive(true);
                    p.glow(cx + std::cos(a + da) * r, cy + std::sin(a + da) * r, 24 * u * op, withA(sc, 0.5f * op));
                    p.setAdditive(false);
                }
            }
        }
        // the stage's number, large, on a dark disc ringed in its hue
        {
            const float r = 205 * u, nx = cx + std::cos(a) * r, ny = cy + std::sin(a) * r;
            const float rad = (21 + 9 * f) * u;
            const bool next = s == nextStage;
            if (next) {
                p.setAdditive(true);
                p.glow(nx, ny, rad * 2.6f, withA(c, 0.35f + 0.25f * std::sin(t * 1.8f)));
                p.setAdditive(false);
            }
            p.circle(nx, ny, rad + 6 * u, hex(0x000000, 0.35f), 10 * u);
            p.circle(nx, ny, rad, hex(0x0a0a10, 0.82f));
            p.ring(nx, ny, rad, (1.6f + 1.4f * f) * u, o ? withA(mixc(c, hex(0xffffff), 0.25f), 0.95f) : hex(0x8a8c96, 0.7f));
            const float ts = (25 + 13 * f) * u;
            p.text(F::JOSEFIN, std::to_string(s + 1), nx, ny + ts * 0.36f, ts, o ? hex(0xffffff, 0.85f + 0.15f * f) : hex(0xa0a2ac, 0.75f), 0, 0.02f);
            if (!o) p.lock(nx + rad * 0.95f, ny - rad * 0.95f, 13 * u, hex(0xd0d0d8, 0.8f));
        }
    }
    // the focused song, when a petal is open
    if (m.aRow_ > 0.5f && ao > 0.5f) {
        const float a = ang(m.aStage_) + 0.14f * (m.aSong_ - 2.f) * ao;
        const float r = (318 + 37) * u;
        const float fx = cx + std::cos(a) * r, fy = cy + std::sin(a) * r;
        const float pulse = 0.8f + 0.2f * std::sin(t * 2.2f);
        p.setAdditive(true);
        p.glow(fx, fy, 52 * u, hex(0xffffff, 0.3f * pulse * ao));
        p.setAdditive(false);
        p.ring(fx, fy, 34 * u, 2.2f * u, hex(0xffffff, 0.9f * pulse * ao));
    }
    // the medallion: a plain dark glass disc, softly tinted by the focused stage
    p.circle(cx, cy, 128 * u, hex(0x08080c));
    {
        const Col sc = open(fs) ? songCol(fs, m.petal_ ? m.song_ : 0) : hex(0x5a5c66);
        p.circle(cx, cy, 118 * u, withA(sc, 0.16f + 0.1f * ao), 70 * u);
    }
    p.ring(cx, cy, 121 * u, 3 * u, hex(0xe8c06a, 0.8f));
    p.text(F::JOSEFIN_LIGHT, std::to_string(fs + 1), cx, cy + 30 * u, 84 * u, hex(0xffffff, 0.9f), 0, 0.02f);
    // the focused stage, on the right
    {
        const float x = X(1240);
        const bool o = open(fs);
        p.text(F::JOSEFIN_LIGHT, "STAGE " + std::to_string(fs + 1), x, Y(250), 64 * u, hex(0xffffff), -1, 0.15f);
        const char* st = !o ? "LOCKED" : cleared(fs) ? "CLEARED" : "OPEN";
        p.text(F::JOSEFIN, st, x, Y(292), 18 * u, !o ? hex(0xb0b0b8) : cleared(fs) ? hex(0xffd27a) : hex(0xffffff, 0.6f), -1, 0.4f);
        if (o && cleared(fs)) p.text(F::JOSEFIN, "BEST " + std::to_string(m.progress_->stageBest(fs, m.diff_)), x + 560 * u - 16 * u, Y(292), 18 * u, hex(0xffd27a), 1, 0.3f);
        for (int k = 0; k < nSongs(fs) && k < 5; k++) {
            const float y = Y(350 + k * 70);
            const float g = m.aRow_ > 0.5f ? sstep(near1(m.aSong_, k)) * ao : 0.f;
            p.rrect(x - 16 * u, y - 36 * u, 560 * u, 56 * u, 12 * u, hex(0xffffff, 0.04f + 0.1f * g));
            p.rrectLine(x - 16 * u, y - 36 * u, 560 * u, 56 * u, 12 * u, 1.2f * u, hex(0xffffff, 0.12f + 0.5f * g));
            const Col sc = o ? songCol(fs, k) : hex(0x5a5c66);
            p.rrect(x, y - 24 * u, 32 * u, 32 * u, 4 * u, sc);
            if (o && beaten(fs, k)) p.check(x + 16 * u, y - 8 * u, 18 * u, 3 * u, hex(0xffffff));
            int vi = 0;
            const int idx = universes::parse(spec(fs, k), &vi);
            std::string name = idx >= 0 ? universes::entry(idx).name : spec(fs, k);
            p.text(F::JOSEFIN, name, x + 52 * u, y, 20 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.25f);
            if (o && beaten(fs, k)) p.text(F::JOSEFIN, std::to_string(m.progress_->best(fs, k, m.diff_)), x + 520 * u, y, 18 * u, hex(0xffd27a), 1, 0.15f);
        }
        std::string prompt;
        if (!o) prompt = "LOCKED";
        p.text(F::JOSEFIN, prompt, x, Y(720), 17 * u, hex(0xffffff, 0.75f), -1, 0.35f);
    }
    tabs(F::JOSEFIN, hex(0xffffff), hex(0xffffff), 0.35f);
    info(F::JOSEFIN_LIGHT, F::JOSEFIN, hex(0xffffff), hex(0xe0e0f0, 0.65f), hex(0xffffff), 1);
}

void Worlds::kalTitle() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    const float cx = X(960), cy = Y(330);
    p.circle(cx, cy, 250 * u, hex(0x000000, 0.4f), 60 * u);
    for (int i = 0; i < 12; i++) {
        const float a = t * 0.03f + i * TAU / 12;
        kite(cx, cy, a, 200 * u, 290 * u, 0.2f, hex(i % 2 ? 0xffffff : 0xe8c06a), 0.18f, true, hex(0x08080c, 0.8f));
    }
    p.circle(cx, cy, 200 * u, hex(0x08080c));   // a plain dark glass disc, the title inside it
    p.ring(cx, cy, 191 * u, 4 * u, hex(0xe8c06a, 0.85f));
    {
        const std::string title = upper(GAME_TITLE);
        float size = 76 * u;   // as large as fits inside the disc
        const float w = p.width(F::JOSEFIN_LIGHT, title, size, 0.22f);
        if (w > 330 * u) size *= 330 * u / w;
        p.setAdditive(true);
        p.glow(cx, cy, 150 * u, hex(0xe8c06a, 0.08f + 0.03f * std::sin(t * 0.8f)));
        p.setAdditive(false);
        p.text(F::JOSEFIN_LIGHT, title, cx, cy + size * 0.36f, size, hex(0xffffff), 0, 0.22f);
    }
    p.rrect(cx - 260 * u, Y(652), 520 * u, 210 * u, 24 * u, hex(0x0c0c14, 0.6f));
    menuItems(F::JOSEFIN, 712, 62, 30, hex(0xffffff), hex(0xffffff), 0.35f);
}

void Worlds::kalFree() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    p.rrect(X(560), Y(90), 800 * u, 130 * u, 24 * u, hex(0x0c0c14, 0.65f));
    p.text(F::JOSEFIN_LIGHT, "FREE PLAY", X(960), Y(178), 72 * u, hex(0xffffff), 0, 0.22f);
    const bool rows = m.playlists_ != nullptr;
    const float onDiff = rows ? 1.f - std::min(1.f, m.aFreeRow_) : 1.f;
    static const uint32_t C[3] = {0x6fd8c0, 0x9c86ff, 0xff6f9c};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(m.aFree_, d)), x = X(960 + (d - 1) * 430.f), y = Y(420);
        const float R = (115 + 22 * f * onDiff) * u;
        for (int i = 0; i < 4; i++)
            kite(x, y, t * 0.05f * freePlayMultiplier(d) + i * TAU / 4, 0, R, 0.55f, hex(C[d]), 0.5f + 0.45f * f, true,
                 f > 0.5f && onDiff > 0.5f ? hex(0xffffff, 0.9f) : hex(0x08080c, 0.9f));
        p.rrect(x - 110 * u, y + R + 14 * u, 220 * u, 78 * u, 16 * u, hex(0x0c0c14, 0.7f));
        p.text(F::JOSEFIN, difficultyName(d), x, y + R + 50 * u, (24 + 4 * f) * u, hex(0xffffff, 0.5f + 0.5f * f), 0, 0.35f);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", freePlayMultiplier(d));
        p.text(F::JOSEFIN, b, x, y + R + 78 * u, 18 * u, hex(0xffffff, 0.45f + 0.4f * f), 0, 0.1f);
    }
    if (rows) {
        // the playlist: the remembered ones (most recent first), the game's own, and "add one"
        const float onPl = std::min(1.f, m.aFreeRow_);
        p.rrect(X(160), Y(712), 1600 * u, 196 * u, 24 * u, hex(0x0c0c14, 0.72f));
        p.rrectLine(X(160), Y(712), 1600 * u, 196 * u, 24 * u, 1.2f * u, hex(0xffffff, 0.15f + 0.35f * onPl));
        p.text(F::JOSEFIN, "PLAYLIST", X(200), Y(752), 17 * u, hex(0xffffff, 0.6f + 0.4f * onPl), -1, 0.45f);
        if (m.editing_) {
            const float fx = X(200), fy = Y(778), fw = 1520 * u, fh = 62 * u;
            p.rrect(fx, fy, fw, fh, 12 * u, hex(0xffffff, 0.08f));
            p.rrectLine(fx, fy, fw, fh, 12 * u, 2 * u, hex(0xffffff, 0.85f));
            std::string shown = m.editText_;
            const float ts = 24 * u;
            while (!shown.empty() && p.width(F::JOSEFIN, shown, ts) > fw - 60 * u) shown.erase(0, 1);
            if (shown.empty()) p.text(F::JOSEFIN, "Type or paste (Ctrl+V) a YouTube playlist link", fx + 24 * u, fy + 40 * u, ts, hex(0xffffff, 0.4f));
            const float tw = p.text(F::JOSEFIN, shown, fx + 24 * u, fy + 40 * u, ts, hex(0xffffff));
            if (std::fmod(t, 1.f) < 0.6f && !m.fetching())
                p.rrect(fx + 26 * u + tw, fy + 14 * u, 2.5f * u, 34 * u, 1 * u, hex(0xffffff, 0.9f));
        } else {
            const auto ch = m.plChoices();
            const int nc = (int)ch.size() + 1;
            std::vector<std::string> labels;
            for (const auto& e : ch) {
                std::string l = asciiFold(e.title.empty() ? e.url : e.title);
                if (l.size() > 34) l = l.substr(0, 33) + "...";
                if (e.count > 0) l += "   " + std::to_string(e.count);
                labels.push_back(l);
            }
            labels.push_back("+  ADD A PLAYLIST");
            std::vector<float> w(nc);
            float total = 0;
            for (int i = 0; i < nc; i++) { w[i] = p.width(F::JOSEFIN, labels[i], 21 * u, 0.12f) + 56 * u; total += w[i] + 18 * u; }
            // the row scrolls so that the selected chip stays in view
            float xs = 0, selX = 0;
            for (int i = 0; i < nc; i++) { if (i == m.plSel_) selX = xs + w[i] / 2; xs += w[i] + 18 * u; }
            const float avail = 1520 * u;
            float off = X(200);
            if (total > avail) off = X(960) - std::clamp(selX, avail / 2, total - avail / 2);
            float x = off;
            for (int i = 0; i < nc; i++) {
                const float g = sstep(near1(m.aPl_, i));
                const bool sel = i == m.plSel_;
                const float y = Y(780);
                if (x + w[i] > X(160) && x < X(1760)) {
                    p.rrect(x, y, w[i], 56 * u, 28 * u, hex(0xffffff, 0.06f + 0.16f * g * onPl));
                    p.rrectLine(x, y, w[i], 56 * u, 28 * u, (1.2f + 1.2f * g * onPl) * u, hex(0xffffff, 0.2f + 0.7f * g * onPl));
                    p.text(F::JOSEFIN, labels[i], x + 28 * u, y + 36 * u, 21 * u, hex(0xffffff, sel ? 1.f : 0.65f), -1, 0.12f);
                }
                x += w[i] + 18 * u;
            }
        }
        if (!m.plMsg_.empty())
            p.text(F::JOSEFIN, upper(m.plMsg_), X(200), Y(880), 17 * u, m.plMsgBad_ ? hex(0xff9a9a) : hex(0xffe0a0), -1, 0.3f);
        else if (!m.editing_) {
            const auto ch = m.plChoices();
            const std::string& url = m.plSel_ < (int)ch.size() ? ch[m.plSel_].url : std::string();
            p.text(F::JOSEFIN, url.empty() ? "A YOUTUBE PLAYLIST OR VIDEO LINK" : url, X(200), Y(880), 15 * u, hex(0xffffff, 0.45f), -1, 0.1f);
        }
    }
    // only the keys a player wouldn't guess (paste, remove); the arrows, Enter and Esc go without saying
    const char* hint = m.editing_ ? "CTRL+V  PASTE" : (rows && m.freeRow_ == 1) ? "DEL  REMOVE" : "";
    if (*hint) p.text(F::JOSEFIN, hint, X(960), Y(1020), 15 * u, hex(0xffffff, 0.55f), 0, 0.3f);
}

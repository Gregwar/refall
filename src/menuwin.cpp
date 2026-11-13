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
#include "playlists.hpp"

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
std::string linesStr(int n) { return n < 0 ? std::string("-") : std::to_string(n); } // lines, "-" when unknown
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
    bool open(int s) const { return !m.progress_ || m.progress_->unlocked(s, m.diff_); }
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
    void kalVoyages();
    void kalCampaign();
    void textField(float fx, float fy, float fw, float fh, const std::string& text, const char* placeholder);
};

void Menu::renderWorlds(int w, int h) {
    if (!worlds_) {
        worlds_ = std::make_unique<WorldPool>();
        worlds_->setSnapshotsOnly(true); // cached snapshots; the missing ones made one at a time while the menu is idle
        worlds_->setGenerate(true, 0.4f);
        Worlds wd0(*this);
        worlds_->prefetch(wd0.tour());
    }
    static const MusicState mu; // (the snapshots-only pool renders nothing)
    worlds_->setIdle(time_ - lastInput_ > 0.6f); // a universe's snapshot (100-500 ms) is never made under a key press
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
    if (m.screen_ == Menu::CAMPAIGN && nStages() > 0) { // the focused stage's universes, and its neighbours'
        const int fs = focusedStage();
        for (int s : {fs, fs + 1, fs - 1})
            if (s >= 0 && s < nStages())
                for (int k2 = 0; k2 < nSongs(s); k2++) pool.want(spec(s, k2), 0);
        return;
    }
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    pool.want(cur, 0);
    pool.want(next, 0);
}

void Worlds::draw() {
    if (m.screen_ == Menu::TITLE) kalTitle();
    else if (m.screen_ == Menu::FREE) kalFree();
    else if (m.screen_ == Menu::VOYAGES) kalVoyages();
    else kalCampaign();
}

// ---------------------------------------------------------------------------------------------------------------------
// shared pieces

void Worlds::tabs(F f, Col ink, Col accent, float track) {
    std::string name = m.voyageTitle_.empty() ? std::string("VOYAGE") : upper(asciiFold(m.voyageTitle_));
    while (name.size() > 4 && p.width(f, name, 22 * u, track + 0.2f) > 760 * u) name.pop_back();
    p.text(f, name, X(80), Y(92), 22 * u, withA(ink, 0.8f), -1, track + 0.2f);
    const float cx = X(1180), sp = 220 * u, y = Y(90), on = 1.f - std::min(1.f, m.aRow_);
    const float hx = cx + (m.aDiff_ - 1) * sp;
    p.line(hx - 56 * u, y + 16 * u, hx + 56 * u, y + 16 * u, (1.5f + 1.5f * on) * u, withA(accent, 0.6f + 0.4f * on));
    if (on > 0.05f) { p.setAdditive(true); p.glow(hx, y + 16 * u, 80 * u, withA(accent, 0.25f * on)); p.setAdditive(false); }
    for (int d = 0; d < DIFF_COUNT; d++)
        p.text(f, difficultyName(d), cx + (d - 1) * sp, y, 21 * u, withA(ink, 0.45f + 0.55f * near1(m.aDiff_, d)), 0, track);
}

void Worlds::menuItems(F f, float y0, float step, float size, Col ink, Col accent, float track) {
    static const char* ITEMS[Menu::TITLE_ITEMS] = {"VOYAGE", "FREE PLAY", "QUIT"};
    const float cx = X(960);
    // the focused entry's inked edges (eased between entries): the underline spans them, the two halos sit at the same
    // distance outside them, level with the middle of the capitals
    float inkL = 0, inkR = 0;
    for (int i = 0; i < Menu::TITLE_ITEMS; i++) {
        float l, r;
        p.inkExtent(f, ITEMS[i], size * u, track, l, r);
        const float start = cx - p.width(f, ITEMS[i], size * u, track) * 0.5f;
        inkL += near1(m.aTitle_, i) * (start + l);
        inkR += near1(m.aTitle_, i) * (start + r);
    }
    const float base = Y(y0 + m.aTitle_ * step), ys = base + 14 * u, yc = base - p.capHeight(f, size * u) * 0.5f;
    p.line(inkL, ys, inkR, ys, 1.6f * u, withA(accent, 0.9f));
    p.setAdditive(true);
    p.glow(inkL - 26 * u, yc, 22 * u, withA(accent, 0.8f));
    p.glow(inkR + 26 * u, yc, 22 * u, withA(accent, 0.8f));
    p.setAdditive(false);
    for (int i = 0; i < Menu::TITLE_ITEMS; i++)
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
        if (v.beaten) std::snprintf(b, sizeof(b), "BEATEN    BEST %d    %s LINES", v.best, linesStr(v.lines).c_str());
        else std::snprintf(b, sizeof(b), "NOT BEATEN ON %s", difficultyName(m.diff_));
        p.text(body, b, rx, y0 + 46 * u, 18 * u, v.beaten ? hex(0xffd27a) : dim, 1, 0.3f);
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
            std::snprintf(b, sizeof(b), "CLEARED    BEST %d    %s LINES", m.progress_->stageBest(fs, m.diff_),
                          linesStr(m.progress_->stageLines(fs, m.diff_)).c_str());
            p.text(body, b, rx, y0 + 46 * u, 18 * u, hex(0xffd27a), 1, 0.3f);
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// KALEIDO

// A playlist's identity hue (its card, and free play's background): seeded from its playlist ID; the featured one gold.
Col playlistHue(const std::string& url, bool featured) {
    if (featured) return hex(0xe8c06a);
    uint32_t seed = 2166136261u;
    const std::string key = playlistIdOf(url).empty() ? url : playlistIdOf(url);
    for (char c : key) seed = (seed ^ (unsigned char)c) * 16777619u;
    const float h = (seed % 360) / 360.f, s = 0.55f, v = 0.95f;
    auto f = [&](float n) { const float k2 = std::fmod(n + h * 6.f, 6.f); return v - v * s * std::max(0.f, std::min({k2, 4.f - k2, 1.f})); };
    return Col{f(5), f(3), f(1), 1.f};
}

// The background: on the title, the game's universes (cached snapshots) in turn; on Voyage, procedural glass in the
// focused stage's hue (the focused song's step of it in an open petal), the same cool grey for every locked stage; in
// free play, the focused playlist's hue. The hue and the brightness ease over ~0.7 s.
// The background: a universe of the game folded into the kaleidoscope (its cached snapshot; until it exists, procedural
// glass in its colours, cross-fading to the image when it arrives). The title and free play tour the voyage's
// universes; Voyage shows the focused stage's first universe (the focused song's in an open petal), tinted by the
// stage's hue (the song's step of it), greyed and dimmed for a locked stage. Tint and brightness ease over ~0.7 s.
void Worlds::kalBackground(const std::string& cur) {
    static Col tintNow{0.6f, 0.6f, 0.7f, 1.f};
    static float lvlNow = 0.f, satNow = 1.f, duoNow = 0.35f;
    static double lastT = -1;
    const bool first = lastT < 0;
    const float dt = first ? 1.f : (float)std::clamp(t - lastT, 0.0, 0.25);
    lastT = t;
    Col tint{0.6f, 0.6f, 0.7f, 1.f};
    float lvl = 0.f, sat = 1.f, duo = 0.35f; // duo: how far the frames are graded toward the tint
    std::string spec = cur;
    if (m.screen_ == Menu::CAMPAIGN && nStages() > 0) {
        const int fs = focusedStage();
        if (m.petal_) spec = this->spec(fs, m.song_);
        else { // a closed petal: the most colourful of its universes (among those with a snapshot)
            spec = this->spec(fs, 0);
            float best = -1.f;
            for (int k2 = 0; k2 < nSongs(fs); k2++) {
                float col, lo, hi;
                if (pool.stats(this->spec(fs, k2), col, lo, hi) && col > best) { best = col; spec = this->spec(fs, k2); }
            }
        }
        if (!open(fs)) { tint = hex(0x8c94a8); lvl = -1.3f; sat = 0.15f; duo = 0.f; } // locked: a cool, dim grey
        else {
            tint = m.petal_ ? songCol(fs, m.song_) : stageCol(fs);
            duo = 0.62f; // graded toward the stage's hue: a pale or grey world still comes out vivid
        }
    } else if (m.screen_ == Menu::FREE) {
        const auto ch = m.plChoices();
        if (m.plSel_ < (int)ch.size()) tint = playlistHue(ch[m.plSel_].url, m.plSel_ == 0);
    } else {
        unsigned char c[3];
        menuUniverseColor(m.camp_, cur, c);
        tint = rgb8(c);
    }
    // the source: the image when it exists, else procedural glass ("#"), and a change of either folds in over 0.8 s
    const std::string key = pool.tex(spec) ? spec : spec + "#";
    if (key != g_cur) {
        g_prev = g_cur;
        g_cur = key;
        g_mix = g_prev.empty() ? 1.f : 0.f;
    }
    g_mix = std::min(1.f, g_mix + dt / 0.8f);
    auto texOf = [&](const std::string& kk) -> GLuint { return kk.empty() || kk.back() == '#' ? 0 : pool.tex(kk); };
    const GLuint a = texOf(g_prev.empty() ? g_cur : g_prev), b = texOf(g_cur);
    const float k = first ? 1.f : 1.f - std::exp(-dt / 0.7f);
    tintNow = mixc(tintNow, tint, k);
    lvlNow += (lvl - lvlNow) * k;
    satNow += (sat - satNow) * k;
    duoNow += (duo - duoNow) * k;
    auto range = [&](const std::string& kk, float& lo, float& hi) {
        float col;
        lo = 0.f; hi = 1.f;
        if (!kk.empty() && kk.back() != '#') pool.stats(kk, col, lo, hi);
    };
    float loA, hiA, loB, hiB;
    range(g_prev.empty() ? g_cur : g_prev, loA, hiA);
    range(g_cur, loB, hiB);
    p.kaleido(a, b, g_prev.empty() ? 1.f : sstep(g_mix), X(720), Y(440), t * 0.025f + m.aStage_ * (TAU / 12.f) * 0.5f, 12,
              mixc(tintNow, hex(0xffffff), 0.15f), t, lvlNow, satNow, duoNow, loA, hiA, loB, hiB);
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

// The angle between the opened petal's song facets: wider for fewer songs (the fan spans about 0.56 rad).
static float fanStep(int songs) { return std::min(0.19f, 0.56f / std::max(1, songs - 1)); }

void Worlds::kalCampaign() {
    const int fs = focusedStage();
    const float ao = sstep(m.aOpen_); // the opened petal, 0 closed .. 1 open
    kalBackground(spec(fs, m.petal_ ? m.song_ : 0));
    const int n = nStages();
    const float cx = X(720), cy = Y(440);
    // the rosette turns so that the focused stage sits at the reading position (the right)
    const float base = -m.aStage_ * TAU / std::max(1, n);
    auto ang = [&](float s) { return base + s * TAU / n; };
    // angular sizes were drawn for 12 petals: narrower beyond, so that 15 still breathe
    const float wk = 12.f / std::max(12, n);
    p.circle(cx, cy, 300 * u, hex(0x000000, 0.25f), 90 * u);
    // the order of the journey: an arc from petal to petal (1 -> 12), gold as far as the stages are open
    int nextStage = -1;
    for (int s = 0; s < n && nextStage < 0; s++)
        if (open(s) && !cleared(s)) nextStage = s;
    {
        const float r = 296 * u;
        for (int s = 0; s + 1 < n; s++) {
            const bool o = open(s + 1);
            const float a0 = ang((float)s) + 0.06f * wk, a1 = ang((float)s + 1) - 0.06f * wk;
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
    // A stage just cleared (see Menu::celebrateClear) is drawn last, over a shade on everything else: it shakes,
    // faster and faster, then turns into its cleared look at once, a white copy of its contour spreading out as a wave.
    const int celS = m.celebrate_ >= 0 && m.celebrate_ < n && open(m.celebrate_) && cleared(m.celebrate_) ? m.celebrate_ : -1;
    const float celT = m.celebrateT_;
    constexpr float TURN = 2.3f; // when it turns
    for (int ii = 0; ii < n; ii++) {
        const int s = celS < 0 ? ii : ii < celS ? ii : ii < n - 1 ? ii + 1 : celS;
        const bool cel = s == celS;
        if (cel) {
            const float shade = 0.62f * sstep(std::clamp(celT / 0.5f, 0.f, 1.f)) * (1.f - sstep(std::clamp((celT - TURN - 0.6f) / 0.9f, 0.f, 1.f)));
            if (shade > 0.f) p.rrect(-10.f, -10.f, m.fw_ + 20.f, m.fh_ + 20.f, 0.f, hex(0x000000, shade));
        }
        const float cT = cel ? celT : 99.f;
        float a = ang((float)s);
        if (cel && cT > 0.35f && cT < TURN) { // the shake: wider and faster up to the turn
            const float tau = cT - 0.35f, q = tau / (TURN - 0.35f);
            a += 0.04f * q * std::sqrt(q) * std::sin(TAU * (5.f * tau + 4.6f * tau * tau));
        }
        const bool o = open(s), cl = o && cleared(s);
        const float ck = cl ? (cel ? sstep(std::clamp((cT - TURN) / 0.12f, 0.f, 1.f)) : 1.f) : 0.f;
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
        // three clear states: locked (grey), open but not cleared (its hue dimmed, like unlit glass), cleared (its
        // full hue lit, a gold rim, a gold number disc)
        const Col pcOpen = mixc(c, hex(0x14141c), 0.5f), pcClear = mixc(c, hex(0xffffff), 0.12f * op);
        const Col pc = !o ? c : mixc(pcOpen, pcClear, ck);
        const Col edgeCl = mixc(hex(0x08080c, 0.9f), hex(0xe8c06a, 0.95f), ck);
        kite(cx, cy, a, (135 + 10 * op) * u, (262 + 22 * f + 26 * op) * u, (0.2f + 0.03f * f + 0.1f * op) * wk, pc,
             !o ? 0.6f : lerpf(0.7f, 0.95f, ck), true, focusClosed && (!cel || cT > TURN + 1.f) ? hex(0xffffff, 0.95f * pulse) : edgeCl);
        if (cl) { // a cleared petal glows from within
            p.setAdditive(true);
            p.glow(cx + std::cos(a) * 215 * u, cy + std::sin(a) * 215 * u, 70 * u, withA(c, 0.22f * ck));
            p.setAdditive(false);
        }
        if (cel && cT >= TURN && cT < TURN + 2.f) {
            // the turn: a swell of its hue, and white copies of its contour spreading out over the screen, fading
            p.setAdditive(true);
            const float sw = std::exp(-std::pow((cT - TURN - 0.1f) / 0.35f, 2.f));
            p.glow(cx + std::cos(a) * 215 * u, cy + std::sin(a) * 215 * u, (140 + 80 * sw) * u, withA(c, 0.6f * sw));
            const float r0 = (135 + 10 * op) * u, r1 = (262 + 22 * f + 26 * op) * u, ha = (0.2f + 0.03f * f + 0.1f * op) * wk;
            const float rm = lerpf(r0, r1, 0.45f);
            float px[4] = {cx + std::cos(a) * r0, cx + std::cos(a - ha) * rm, cx + std::cos(a) * r1, cx + std::cos(a + ha) * rm};
            float py[4] = {cy + std::sin(a) * r0, cy + std::sin(a - ha) * rm, cy + std::sin(a) * r1, cy + std::sin(a + ha) * rm};
            const float mx = (px[0] + px[1] + px[2] + px[3]) * 0.25f, my = (py[0] + py[1] + py[2] + py[3]) * 0.25f;
            for (int w = 0; w < 3; w++) {
                const float age = cT - TURN - 0.16f * w, life = 1.5f;
                if (age <= 0.f || age >= life) continue;
                const float k = age / life, sc = 1.f + 7.f * (1.f - (1.f - k) * (1.f - k));
                const Col wc = hex(0xffffff, 0.85f * std::pow(1.f - k, 1.6f) * (1.f - 0.25f * w));
                for (int e = 0; e < 4; e++) {
                    const int g = (e + 1) % 4;
                    p.line(mx + (px[e] - mx) * sc, my + (py[e] - my) * sc, mx + (px[g] - mx) * sc, my + (py[g] - my) * sc,
                           (3.f + 2.f * k) * u, wc, (1.5f + 6.f * k) * u);
                }
            }
            p.setAdditive(false);
        }
        if (cl && ck > 0.99f) { // a shine sweeping slowly across a cleared shard
            const float k = std::fmod(t * 0.18f + s * 0.29f, 1.f), r = lerpf(150.f, 255.f, k) * u, hw = 0.16f * wk * (1.f - std::fabs(k - 0.45f));
            p.setAdditive(true);
            p.line(cx + std::cos(a - hw) * r, cy + std::sin(a - hw) * r, cx + std::cos(a + hw) * r, cy + std::sin(a + hw) * r, 5 * u, hex(0xffffff, 0.35f * std::sin(k * PI)), 6 * u);
            p.setAdditive(false);
        }
        // the songs: small dots beyond a closed petal; the opened petal fans its facets out (centred on the petal)
        const int ns = nSongs(s);
        const float mid = (ns - 1) * 0.5f;
        for (int k = 0; k < ns; k++) {
            const bool b = o && beaten(s, k);
            const Col sc = o ? songCol(s, k) : hex(0x4a4c55);
            if (op < 0.98f) {
                const float da = 0.042f * wk * (k - mid), r = 278 * u;
                p.circle(cx + std::cos(a + da) * r, cy + std::sin(a + da) * r, 4.2f * u, withA(b ? sc : mixc(sc, hex(0x101018), 0.5f), (1.f - op) * (o ? 1.f : 0.6f)));
            }
            if (op > 0.02f) {
                const float da = fanStep(ns) * (k - mid) * op;
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
            // cleared: a gold disc (a just cleared one turns gold as the light reaches it)
            const float dk = ck;
            p.circle(nx, ny, rad, mixc(hex(0x0a0a10, 0.82f), hex(0xe8c06a, 0.95f), dk));
            const Col ringOpen = o ? withA(mixc(c, hex(0xffffff), 0.25f), 0.95f) : hex(0x8a8c96, 0.7f);
            p.ring(nx, ny, rad, (1.6f + 1.4f * f) * u, mixc(ringOpen, hex(0xfff0c8, 0.95f), dk));
            const float ts = (25 + 13 * f) * u;
            const Col numOpen = o ? hex(0xffffff, 0.85f + 0.15f * f) : hex(0xa0a2ac, 0.75f);
            p.text(F::JOSEFIN, std::to_string(s + 1), nx, ny + ts * 0.36f, ts, mixc(numOpen, hex(0x1a1408), dk), 0, 0.02f);
            if (!o) p.lock(nx + rad * 0.95f, ny - rad * 0.95f, 13 * u, hex(0xd0d0d8, 0.8f));
        }
    }
    // the focused song, when a petal is open
    if (m.aRow_ > 0.5f && ao > 0.5f) {
        const float a = ang(m.aStage_) + fanStep(nSongs(fs)) * (m.aSong_ - (nSongs(fs) - 1) * 0.5f) * ao;
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
        // the scores and lines in two right-aligned columns (lines dimmer): the stage's on the header line
        const float colS = x + 430 * u, colL = x + 528 * u;
        if (o && cleared(fs)) {
            p.text(F::JOSEFIN, "BEST " + std::to_string(m.progress_->stageBest(fs, m.diff_)), colS, Y(292), 18 * u, hex(0xffd27a), 1, 0.3f);
            p.text(F::JOSEFIN, linesStr(m.progress_->stageLines(fs, m.diff_)) + " L", colL, Y(292), 18 * u, hex(0xffd27a, 0.7f), 1, 0.3f);
        }
        // the list of songs, spread over the room five rows took
        const int ns = nSongs(fs);
        const float rowH = ns > 0 ? std::min(70.f, 350.f / ns) : 70.f;
        for (int k = 0; k < ns; k++) {
            const float y = Y(350 + k * rowH);
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
            if (o && beaten(fs, k)) {
                p.text(F::JOSEFIN, std::to_string(m.progress_->best(fs, k, m.diff_)), colS, y, 18 * u, hex(0xffd27a), 1, 0.15f);
                p.text(F::JOSEFIN, linesStr(m.progress_->lines(fs, k, m.diff_)) + " L", colL, y, 18 * u, hex(0xffd27a, 0.7f), 1, 0.15f);
            }
        }
        std::string prompt;
        if (!o) prompt = "LOCKED";
        else if (!m.petal_) prompt = "SPACE  SONGS"; // the one key Enter doesn't explain: the petal's songs
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
        // the playlists as glass cards: the game's own first (featured: larger, a gold rim, the stages' hues), then the
        // player's, each with a hue and a small rosette of its own (seeded from its playlist ID), then "add one"
        const float onPl = std::min(1.f, m.aFreeRow_);
        p.rrect(X(120), Y(680), 1680 * u, 262 * u, 24 * u, hex(0x0c0c14, 0.72f));
        p.rrectLine(X(120), Y(680), 1680 * u, 262 * u, 24 * u, 1.2f * u, hex(0xffffff, 0.12f + 0.3f * onPl));
        p.text(F::JOSEFIN, "PLAYLIST", X(160), Y(714), 16 * u, hex(0xffffff, 0.55f + 0.45f * onPl), -1, 0.45f);
        if (m.editing_) {
            const float fx = X(160), fy = Y(760), fw = 1600 * u, fh = 62 * u;
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
            const float addW = 122 * u + std::max(p.width(F::JOSEFIN, "ADD A", 17 * u, 0.3f), p.width(F::JOSEFIN, "PLAYLIST", 17 * u, 0.3f)) + 30 * u;
            auto cardW = [&](int i) { return i == 0 ? 440.f * u : i == nc - 1 ? addW : 320.f * u; };
            auto cardH = [&](int i) { return (i == 0 ? 176.f : 144.f) * u; };
            const float gap = 22 * u;
            float total = 0, selX = 0;
            for (int i = 0; i < nc; i++) { if (i == m.plSel_) selX = total + cardW(i) / 2; total += cardW(i) + gap; }
            total -= gap;
            const float avail = 1600 * u;
            float x = X(160);
            if (total > avail) x = X(960) - std::clamp(selX, avail / 2, total - avail / 2);
            auto hueCol = [](float h, float s, float v) {
                auto f = [&](float n) { const float k2 = std::fmod(n + h * 6.f, 6.f); return v - v * s * std::max(0.f, std::min({k2, 4.f - k2, 1.f})); };
                return Col{f(5), f(3), f(1), 1.f};
            };
            for (int i = 0; i < nc; i++) {
                const float w = cardW(i), h = cardH(i);
                const float g = sstep(near1(m.aPl_, i)) * onPl;
                const float y = Y(i == 0 ? 728 : 744) - 10 * u * g;
                if (x + w < X(120) || x > X(1800)) { x += w + gap; continue; }
                const bool featured = i == 0, add = i == nc - 1;
                // the identity: a hue and a seed from the playlist's ID
                uint32_t seed = 2166136261u;
                const std::string key = add ? std::string("+") : playlistIdOf(ch[i].url).empty() ? ch[i].url : playlistIdOf(ch[i].url);
                for (char c : key) seed = (seed ^ (unsigned char)c) * 16777619u;
                const Col hue = featured ? hex(0xe8c06a) : add ? hex(0x9aa0b8) : hueCol((seed % 360) / 360.f, 0.55f, 0.95f);
                if (g > 0.02f || featured) {
                    p.setAdditive(true);
                    p.rrect(x - 6 * u, y - 6 * u, w + 12 * u, h + 12 * u, 26 * u, withA(featured ? hex(0xffc860) : hex(0xffffff), (featured ? 0.10f : 0.f) + 0.16f * g), 26 * u);
                    p.setAdditive(false);
                }
                p.rrect(x, y, w, h, 20 * u, withA(mixc(hue, hex(0x0a0a12), 0.78f), 0.92f));
                p.rrect(x, y, w, h * 0.45f, 20 * u, hex(0xffffff, 0.035f));
                p.rrectLine(x, y, w, h, 20 * u, (featured ? 2.6f : 1.4f) * u + 1.2f * g * u,
                            g > 0.5f ? hex(0xffffff, 0.95f) : featured ? hex(0xe8c06a, 0.95f) : withA(hue, 0.55f));
                const float mx = x + (featured ? 90.f : 70.f) * u, my = y + h / 2, mr = (featured ? 62.f : 46.f) * u;
                if (add) {
                    p.ring(mx, my, 30 * u, 2 * u, hex(0xffffff, 0.5f + 0.4f * g));
                    p.line(mx - 13 * u, my, mx + 13 * u, my, 2.4f * u, hex(0xffffff, 0.7f + 0.3f * g));
                    p.line(mx, my - 13 * u, mx, my + 13 * u, 2.4f * u, hex(0xffffff, 0.7f + 0.3f * g));
                    p.text(F::JOSEFIN, "ADD A", x + 122 * u, my - 4 * u, 17 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.3f);
                    p.text(F::JOSEFIN, "PLAYLIST", x + 122 * u, my + 20 * u, 17 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.3f);
                    x += w + gap;
                    continue;
                }
                // the motif: a small rosette of glass, turning slowly
                const int petals = featured ? 12 : 5 + (int)(seed >> 8) % 4;
                const float rot = t * (featured ? 0.06f : 0.04f + ((seed >> 4) % 5) * 0.01f) + (seed % 100) * 0.0628f;
                for (int k2 = 0; k2 < petals; k2++) {
                    Col pc;
                    if (featured) { unsigned char sc[3]; m.stageColor(k2, sc); pc = rgb8(sc); }
                    else pc = hueCol(std::fmod((seed % 360) / 360.f + (k2 % 2) * (0.08f + ((seed >> 12) % 7) * 0.03f), 1.f), 0.55f, 0.95f);
                    kite(mx, my, rot + k2 * TAU / petals, mr * 0.18f, mr, featured ? 0.24f : 0.42f, pc, 0.92f, true, hex(0x08080c, 0.9f));
                }
                p.circle(mx, my, mr * 0.2f, hex(0x0a0a12));
                p.ring(mx, my, mr * 0.2f, 1.5f * u, featured ? hex(0xe8c06a) : withA(hue, 0.9f));
                // the name and the count
                const float tx = mx + mr + 26 * u, tmax = x + w - tx - 18 * u;
                std::string title = asciiFold(featured ? ch[i].title : ch[i].title.empty() ? ch[i].url : ch[i].title);
                const float ts = (featured ? 40.f : 23.f) * u;
                const F tf = featured ? F::JOSEFIN_LIGHT : F::JOSEFIN;
                while (title.size() > 3 && p.width(tf, title, ts, 0.04f) > tmax) title.pop_back();
                float ty = my + ts * 0.25f;
                if (featured) {
                    p.text(F::JOSEFIN, "FEATURED", tx, y + 42 * u, 14 * u, hex(0xe8c06a), -1, 0.45f);
                    ty = my + 14 * u;
                }
                p.text(tf, title, tx, ty, ts, hex(0xffffff, 0.8f + 0.2f * g), -1, 0.04f);
                if (ch[i].count > 0)
                    p.text(F::JOSEFIN, std::to_string(ch[i].count) + " SONGS", tx, ty + (featured ? 34.f : 28.f) * u, 14 * u, withA(featured ? hex(0xe8c06a) : hue, 0.85f), -1, 0.35f);
                x += w + gap;
            }
        }
        if (!m.plMsg_.empty())
            p.text(F::JOSEFIN, upper(m.plMsg_), X(160), Y(930), 16 * u, m.plMsgBad_ ? hex(0xff9a9a) : hex(0xffe0a0), -1, 0.3f);
    }
    // only the keys a player wouldn't guess (paste, remove); the arrows, Enter and Esc go without saying
    const char* hint = m.editing_ ? "CTRL+V  PASTE" : (rows && m.freeRow_ == 1) ? "DEL  REMOVE" : "";
    if (*hint) p.text(F::JOSEFIN, hint, X(960), Y(1020), 15 * u, hex(0xffffff, 0.55f), 0, 0.3f);
}

// A text field being typed into: its frame, the text (its end kept in view), a placeholder while empty, the caret.
void Worlds::textField(float fx, float fy, float fw, float fh, const std::string& text, const char* placeholder) {
    p.rrect(fx, fy, fw, fh, 12 * u, hex(0xffffff, 0.08f));
    p.rrectLine(fx, fy, fw, fh, 12 * u, 2 * u, hex(0xffffff, 0.85f));
    std::string shown = text;
    const float ts = 24 * u;
    while (!shown.empty() && p.width(F::JOSEFIN, shown, ts) > fw - 60 * u) shown.erase(0, 1);
    if (shown.empty() && placeholder) p.text(F::JOSEFIN, placeholder, fx + 24 * u, fy + 40 * u, ts, hex(0xffffff, 0.4f));
    const float tw = p.text(F::JOSEFIN, shown, fx + 24 * u, fy + 40 * u, ts, hex(0xffffff));
    if (std::fmod(t, 1.f) < 0.6f && !m.fetching())
        p.rrect(fx + 26 * u + tw, fy + 14 * u, 2.5f * u, 34 * u, 1 * u, hex(0xffffff, 0.9f));
}

// The voyages, as free play's playlists: glass cards in a row, the game's own first (featured: larger, a gold rim),
// then the ones made from the player's playlists (the last played first, each with its own hue), then "add a
// playlist". Each card's small rosette has a kite per stage, the cleared ones lit. Adding one: a panel under the cards
// for the playlist's link, then the voyage's name.
void Worlds::kalVoyages() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    p.rrect(X(660), Y(90), 600 * u, 130 * u, 24 * u, hex(0x0c0c14, 0.65f));
    p.text(F::JOSEFIN_LIGHT, "VOYAGE", X(960), Y(178), 72 * u, hex(0xffffff), 0, 0.22f);
    if (!m.voyages_) return;
    const auto& vs = m.voyages_->list();
    const int nc = (int)vs.size() + 2;
    const bool making = m.vStep_ > 0;
    const float addW = 122 * u + std::max(p.width(F::JOSEFIN, "ADD A", 17 * u, 0.3f), p.width(F::JOSEFIN, "PLAYLIST", 17 * u, 0.3f)) + 30 * u;
    auto cardW = [&](int i) { return i == 0 ? 520.f * u : i == nc - 1 ? addW : 440.f * u; };
    auto cardH = [&](int i) { return (i == 0 ? 210.f : 170.f) * u; };
    const float gap = 24 * u;
    float total = 0, selX = 0;
    for (int i = 0; i < nc; i++) { selX += near1(m.aV_, i) * (total + cardW(i) / 2); total += cardW(i) + gap; }
    total -= gap;
    const float avail = 1680 * u;
    float x = total <= avail ? X(960) - total / 2 : X(960) - std::clamp(selX, avail / 2, total - avail / 2);
    auto hueCol = [](float h, float s, float v) {
        auto f = [&](float n) { const float k2 = std::fmod(n + h * 6.f, 6.f); return v - v * s * std::max(0.f, std::min({k2, 4.f - k2, 1.f})); };
        return Col{f(5), f(3), f(1), 1.f};
    };
    const float midY = 420.f;
    for (int i = 0; i < nc; i++) {
        const float w = cardW(i), h = cardH(i);
        const float g = sstep(near1(m.aV_, i)) * (making ? 0.4f : 1.f);
        const float y = Y(midY) - h / 2 - 10 * u * g;
        if (x + w < X(0) || x > X(1920)) { x += w + gap; continue; }
        const bool featured = i == 0, add = i == nc - 1;
        const CustomVoyage* v = !featured && !add ? &vs[i - 1] : nullptr;
        uint32_t seed = 2166136261u;
        for (char c : add ? std::string("+") : v ? v->id : std::string("refall")) seed = (seed ^ (unsigned char)c) * 16777619u;
        const Col hue = featured ? hex(0xe8c06a) : add ? hex(0x9aa0b8) : hueCol((seed % 360) / 360.f, 0.55f, 0.95f);
        if (g > 0.02f || featured) {
            p.setAdditive(true);
            p.rrect(x - 6 * u, y - 6 * u, w + 12 * u, h + 12 * u, 26 * u, withA(featured ? hex(0xffc860) : hex(0xffffff), (featured ? 0.10f : 0.f) + 0.16f * g), 26 * u);
            p.setAdditive(false);
        }
        p.rrect(x, y + 12 * u, w, h, 22 * u, hex(0x000000, 0.3f), 30 * u);
        p.rrect(x, y, w, h, 20 * u, withA(mixc(hue, hex(0x0a0a12), 0.78f), 0.92f));
        p.rrect(x, y, w, h * 0.45f, 20 * u, hex(0xffffff, 0.035f));
        p.rrectLine(x, y, w, h, 20 * u, (featured ? 2.6f : 1.4f) * u + 1.2f * g * u,
                    g > 0.5f ? hex(0xffffff, 0.95f) : featured ? hex(0xe8c06a, 0.95f) : withA(hue, 0.55f));
        const float mx = x + (featured ? 105.f : 85.f) * u, my = y + h / 2, mr = (featured ? 74.f : 58.f) * u;
        if (add) {
            p.ring(mx - 15 * u, my, 30 * u, 2 * u, hex(0xffffff, 0.5f + 0.4f * g));
            p.line(mx - 28 * u, my, mx - 2 * u, my, 2.4f * u, hex(0xffffff, 0.7f + 0.3f * g));
            p.line(mx - 15 * u, my - 13 * u, mx - 15 * u, my + 13 * u, 2.4f * u, hex(0xffffff, 0.7f + 0.3f * g));
            p.text(F::JOSEFIN, "ADD A", x + 122 * u, my - 4 * u, 17 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.3f);
            p.text(F::JOSEFIN, "PLAYLIST", x + 122 * u, my + 20 * u, 17 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.3f);
            x += w + gap;
            continue;
        }
        const Campaign& mc = m.voyages_->main();
        int songs = 0;
        if (featured) for (const CampaignStage& st : mc.stages()) songs += (int)st.songs.size();
        const int st = std::max(1, featured ? mc.size() : v->stages);
        if (!featured) songs = v->songs;
        const int cl = i < (int)m.vCleared_.size() ? m.vCleared_[i] : 0;
        // the rosette: a kite per stage in the stages' hues, the cleared ones lit
        const float rot = t * (featured ? 0.06f : 0.04f + ((seed >> 4) % 5) * 0.01f) + (seed % 100) * 0.0628f;
        for (int k = 0; k < st; k++) {
            unsigned char sc[3];
            m.stageColor(k, sc);
            const Col c = rgb8(sc);
            kite(mx, my, rot + k * TAU / st, mr * 0.22f, mr, std::min(0.42f, 2.8f / st), k < cl ? c : mixc(c, hex(0x14141c), 0.55f),
                 0.92f, true, hex(0x08080c, 0.9f));
        }
        p.circle(mx, my, mr * 0.22f, hex(0x0a0a12));
        p.ring(mx, my, mr * 0.22f, 1.5f * u, featured || cl >= st ? hex(0xe8c06a) : withA(hue, 0.9f));
        // the name, its songs and stages, how far it went
        const float tx = mx + mr + 28 * u, tmax = x + w - tx - 18 * u;
        std::string title = asciiFold(featured ? std::string(GAME_TITLE) : v->title);
        const float ts = (featured ? 42.f : 25.f) * u;
        const F tf = featured ? F::JOSEFIN_LIGHT : F::JOSEFIN;
        while (title.size() > 3 && p.width(tf, title, ts, 0.04f) > tmax) title.pop_back();
        float ty = my + ts * 0.2f;
        if (featured) {
            p.text(F::JOSEFIN, "FEATURED", tx, y + 48 * u, 14 * u, hex(0xe8c06a), -1, 0.45f);
            ty = my + 12 * u;
        }
        p.text(tf, title, tx, ty - 12 * u, ts, hex(0xffffff, 0.8f + 0.2f * g), -1, 0.04f);
        char b[96];
        std::snprintf(b, sizeof(b), "%d SONGS  /  %d STAGES", songs, st);
        p.text(F::JOSEFIN, b, tx, ty + (featured ? 26.f : 20.f) * u, 14 * u, withA(hue, 0.85f), -1, 0.3f);
        std::snprintf(b, sizeof(b), "%d CLEARED", cl);
        if (cl > 0) p.text(F::JOSEFIN, b, tx, ty + (featured ? 50.f : 42.f) * u, 14 * u, hex(0xffd27a, 0.9f), -1, 0.3f);
        x += w + gap;
    }
    if (making) {
        p.rrect(X(120), Y(650), 1680 * u, 230 * u, 24 * u, hex(0x0c0c14, 0.78f));
        p.rrectLine(X(120), Y(650), 1680 * u, 230 * u, 24 * u, 1.2f * u, hex(0xffffff, 0.4f));
        p.text(F::JOSEFIN, m.vStep_ == 1 ? "PLAYLIST" : "NAME", X(160), Y(686), 16 * u, hex(0xffffff), -1, 0.45f);
        textField(X(160), Y(712), 1600 * u, 62 * u, m.editText_,
                  m.vStep_ == 1 ? "Type or paste (Ctrl+V) a YouTube playlist link" : nullptr);
    }
    if (!m.plMsg_.empty())
        p.text(F::JOSEFIN, upper(m.plMsg_), making ? X(160) : X(960), Y(making ? 830 : 640), 16 * u,
               m.plMsgBad_ ? hex(0xff9a9a) : hex(0xffe0a0), making ? -1 : 0, 0.3f);
    // only the keys a player wouldn't guess (paste, delete)
    const char* hint = m.vStep_ == 1 ? "CTRL+V  PASTE" : !making && m.vSel_ >= 1 && m.vSel_ <= (int)vs.size() ? "DEL  DELETE" : "";
    if (*hint) p.text(F::JOSEFIN, hint, X(960), Y(1020), 15 * u, hex(0xffffff, 0.55f), 0, 0.3f);
}

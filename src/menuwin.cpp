// The menu: a slowly turning crystal kaleidoscope of procedural glass, its own pattern for each stage and song of each
// voyage, in their hues; the stages are faceted gem petals in a quiet rosette (one fine ring, no ornament)
// around a polished dark medallion, each opening onto its songs as smaller gems; focus turns the rosette to bring the
// chosen petal to the reading position (the right). Panels are frosted glass (Painter::glass).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "library.hpp"
#include "menu.hpp"
#include "universes/registry.hpp"
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
// The game's start picks the title's patterns, where its tour of universes' hues begins and how the kaleidoscope is
// turned (REFALL_MENU_SEED=N: a given one, e.g. for screenshots).
unsigned menuSeed() {
    static const unsigned seed = std::getenv("REFALL_MENU_SEED") ? (unsigned)std::atoi(std::getenv("REFALL_MENU_SEED")) : std::random_device{}();
    return seed;
}

// The title's disc, its height: the logo (its petals and fine ring reaching from about 20 to 640 for a disc at 330) and the
// menu (680..890) centred on the screen's height together.
constexpr float TITLE_CY = 330.f + 540.f - 0.5f * (20.f + 890.f);

// the kaleidoscope's crossfade between patterns (one menu at a time)
std::string g_cur, g_prev;
float g_curSeed = 0.f, g_prevSeed = 0.f, g_mix = 1.f;

uint64_t mixHash(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }
uint64_t strHash(uint64_t h, const std::string& s) {
    for (char c : s) h = mixHash(h, (unsigned char)c);
    return h;
}
// a pattern's seed, from a hash (the shader takes a float: a few thousand distinct ones)
float seedOf(uint64_t h) { return (float)((h >> 20) % 4096) + 0.5f; }
} // namespace

struct Worlds {
    Menu& m;
    Painter& p;
    float u, t;
    Worlds(Menu& mm) : m(mm), p(mm.paint_), u(mm.u_), t(mm.time_) {}
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
    // (round the wheel: the animated position may stand a little before the first stage or past the last)
    float focusStage(int s) const {
        const int n = std::max(1, nStages());
        const float d = std::fabs(std::remainder(m.aStage_ - (float)s, (float)n));
        return m.aRow_ > 0.5f ? sstep(std::max(0.f, 1.f - d)) * std::min(1.f, m.aRow_) : 0.f;
    }
    int focusedStage() const {
        const int n = std::max(1, nStages());
        return (((int)std::lround(m.aStage_) % n) + n) % n;
    }
    // all the campaign's universes, in order (the title's slow tour), from one picked at random when the game starts
    std::vector<std::string> tour() const {
        std::vector<std::string> v;
        for (int s = 0; s < nStages(); s++)
            for (int k = 0; k < nSongs(s); k++) v.push_back(spec(s, k));
        if (v.empty()) v.push_back("VECTOR");
        std::rotate(v.begin(), v.begin() + menuSeed() % v.size(), v.end());
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

    void draw();
    // pieces
    void info(F head, F body, Col ink, Col dim, Col accent, int card);
    void tabs(F f, Col ink, Col accent, float track);
    void menuItems(F f, float y0, float step, float size, Col ink, Col accent, float track, float alpha = 1.f);
    void kalBackground(const std::string& cur);
    void kite(float cx, float cy, float a, float r0, float r1, float halfAng, Col c, float alpha, bool lead, Col leadC);
    void wheel(float cx, float cy, float R, float rot, float alpha = 1.f);
    void medallion(float cx, float cy, float R, Col hue, float alpha = 1.f, float ring = 1.f);
    void kalTitle();
    void kalFree();
    void kalVoyages();
    void kalOptions();
    void kalGamepad();
    static constexpr float OPT_STEP = 90; // the options' panel: a row's step
    float optMid = 470; // its middle (from its height)
    void optionPanel(const char* title, int n);
    void optionRows(int n, const char* const* label, float focus);
    void optionLines(const std::vector<std::pair<std::string, float>>& lines, float size);
    bool optionNote();
    void kalCampaign();
    void textField(float fx, float fy, float fw, float fh, const std::string& text, const char* placeholder);
};

void Menu::renderWorlds(int w, int h) {
    paint_.begin(w, h);
    Worlds wd(*this);
    wd.draw();
}

// ---------------------------------------------------------------------------------------------------------------------

void Worlds::draw() {
    if (m.screen_ == Menu::TITLE) kalTitle();
    else if (m.screen_ == Menu::FREE) kalFree();
    else if (m.screen_ == Menu::VOYAGES) kalVoyages();
    else if (m.screen_ == Menu::OPTIONS) kalOptions();
    else if (m.screen_ == Menu::GAMEPAD) kalGamepad();
    else kalCampaign();
}

// ---------------------------------------------------------------------------------------------------------------------
// shared pieces

void Worlds::tabs(F f, Col ink, Col accent, float track) {
    std::string name = m.voyageTitle_.empty() ? std::string("VOYAGE") : upper(asciiFold(m.voyageTitle_));
    while (name.size() > 4 && p.width(f, name, 22 * u, track + 0.2f) > 700 * u) name.pop_back();
    // the difficulties, centred on cx (four of them, EASY .. EXPERT)
    const float cx = X(1250), sp = 195 * u, y = Y(90), on = 1.f - std::min(1.f, m.aRow_), mid = 0.5f * (DIFF_COUNT - 1);
    // a soft shade under the title and the tabs, so they read over a bright crystal
    p.rrect(X(40), y - 40 * u, p.width(f, name, 22 * u, track + 0.2f) + 80 * u, 64 * u, 32 * u, hex(0x02040c, 0.22f), 50 * u);
    p.rrect(cx - mid * sp - 110 * u, y - 40 * u, 2 * mid * sp + 220 * u, 70 * u, 35 * u, hex(0x02040c, 0.22f), 50 * u);
    p.text(f, name, X(80), Y(92), 22 * u, withA(ink, 0.8f), -1, track + 0.2f);
    const float hx = cx + (m.aDiff_ - mid) * sp;
    p.line(hx - 56 * u, y + 16 * u, hx + 56 * u, y + 16 * u, (1.5f + 1.5f * on) * u, withA(accent, 0.6f + 0.4f * on));
    if (on > 0.05f) { p.setAdditive(true); p.glow(hx, y + 16 * u, 80 * u, withA(accent, 0.25f * on)); p.setAdditive(false); }
    for (int d = 0; d < DIFF_COUNT; d++)
        p.text(f, difficultyName(d), cx + (d - mid) * sp, y, 21 * u, withA(ink, 0.45f + 0.55f * near1(m.aDiff_, d)), 0, track);
}

void Worlds::menuItems(F f, float y0, float step, float size, Col ink, Col accent, float track, float alpha) {
#ifdef REFALL_WEB
    static const char* ITEMS[Menu::TITLE_ITEMS] = {"VOYAGE", "OPTIONS"};
#else
    static const char* ITEMS[Menu::TITLE_ITEMS] = {"VOYAGE", "FREE PLAY", "OPTIONS", "QUIT"};
#endif
    const float cx = X(960);
    // the focused entry's inked edges (eased between entries): the underline spans them
    float inkL = 0, inkR = 0;
    for (int i = 0; i < Menu::TITLE_ITEMS; i++) {
        float l, r;
        p.inkExtent(f, ITEMS[i], size * u, track, l, r);
        const float start = cx - p.width(f, ITEMS[i], size * u, track) * 0.5f;
        inkL += near1(m.aTitle_, i) * (start + l);
        inkR += near1(m.aTitle_, i) * (start + r);
    }
    const float base = Y(y0 + m.aTitle_ * step), ys = base + 14 * u;
    p.line(inkL, ys, inkR, ys, 1.6f * u, withA(accent, 0.9f * alpha));
    for (int i = 0; i < Menu::TITLE_ITEMS; i++)
        p.text(f, ITEMS[i], cx, Y(y0 + i * step), size * u, withA(ink, (0.45f + 0.55f * near1(m.aTitle_, i)) * alpha), 0, track);
}

void Worlds::info(F head, F body, Col ink, Col dim, Col accent, int card) {
    const float x0 = X(60), y0 = Y(838), w = 1800 * u, h = 212 * u;
    if (card == 0) {
        p.rrect(x0 - 60 * u, y0 + 10 * u, w + 120 * u, h + 80 * u, 0, hex(0x000000, 0.6f), 60 * u);
        p.line(x0, y0, x0 + w, y0, 1 * u, withA(ink, 0.18f));
    } else {
        p.glass(x0, y0, w, h, 22 * u, u, 0.f, 0.55f);
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

// The background: a universe of the game folded into the kaleidoscope (its cached snapshot; until it exists, procedural
// glass in its colours, cross-fading to the image when it arrives). The title and free play tour the voyage's
// universes; Voyage shows the focused stage's first universe (the focused song's in an open petal), tinted by the
// stage's hue (the song's step of it), greyed and dimmed for a locked stage; free play takes the focused playlist's hue.
// Tint, brightness and the kaleidoscope's centre ease over ~0.7 s.
void Worlds::kalBackground(const std::string& cur) {
    static Col tintNow{0.6f, 0.6f, 0.7f, 1.f};
    static float lvlNow = 0.f, satNow = 1.f, duoNow = 0.35f, cxNow = 720.f, cyNow = 440.f;
    static double lastT = -1;
    const bool first = lastT < 0;
    const float dt = first ? 1.f : (float)std::clamp(t - lastT, 0.0, 0.25);
    lastT = t;
    Col tint{0.6f, 0.6f, 0.7f, 1.f};
    float lvl = 0.f, sat = 1.f, duo = 0.35f; // duo: how far the frames are graded toward the tint
    // the pattern: each stage's (and each song's, its petal open) from the voyage's songs, so another voyage has others;
    // elsewhere the title's tour, one every 11 s from the game's start seed
    std::string key;
    uint64_t h = 1469598103934665603ull;
    if (m.screen_ == Menu::CAMPAIGN && nStages() > 0) {
        const int fs = focusedStage();
        for (const CampaignStage& st : m.camp_->stages())
            for (const CampaignSong& sg : st.songs) h = strHash(h, sg.id);
        h = mixHash(mixHash(h, (uint64_t)fs + 1), m.petal_ ? (uint64_t)m.song_ + 1 : 0);
        key = "S" + std::to_string(fs) + (m.petal_ ? "." + std::to_string(m.song_) : std::string());
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
    if (key.empty()) {
        const int i = (int)((t + 0.8f) / 11.f); // (the tour's universe shown: it turns at its crossfade's middle)
        h = mixHash(mixHash(h, menuSeed()), (uint64_t)i + 1);
        key = "T" + std::to_string(i);
    }
    // a change of pattern folds in over 0.8 s
    if (key != g_cur) {
        g_prev = g_cur;
        g_prevSeed = g_curSeed;
        g_cur = key;
        g_curSeed = seedOf(h);
        g_mix = g_prev.empty() ? 1.f : 0.f;
    }
    g_mix = std::min(1.f, g_mix + dt / 0.8f);
    const float k = first ? 1.f : 1.f - std::exp(-dt / 0.7f);
    tintNow = mixc(tintNow, tint, k);
    lvlNow += (lvl - lvlNow) * k;
    // its centre: the rosette's on Voyage, the title's disc, above the screen on free play and the voyages
    const bool title = m.screen_ == Menu::TITLE, head = m.screen_ == Menu::FREE || m.screen_ == Menu::VOYAGES || m.screen_ == Menu::OPTIONS ||
                     m.screen_ == Menu::GAMEPAD;
    cxNow += ((title || head ? 960.f : 720.f) - cxNow) * k;
    cyNow += ((title ? TITLE_CY : head ? -80.f : 440.f) - cyNow) * k;
    satNow += (sat - satNow) * k;
    // the camera: each screen looks at its own part of space, a glide of about a second between them (the stages'
    // wheel never moves it)
    static float camNow[2] = {0.f, 0.f};
    float camT[2] = {0.f, 0.f};
    switch (m.screen_) {
    case Menu::CAMPAIGN: camT[0] = 0.55f; camT[1] = 0.12f; break;
    case Menu::VOYAGES: camT[0] = 0.35f; camT[1] = -0.3f; break;
    case Menu::FREE: camT[0] = -0.5f; camT[1] = 0.18f; break;
    case Menu::OPTIONS: camT[0] = -0.2f; camT[1] = 0.45f; break;
    case Menu::GAMEPAD: camT[0] = -0.05f; camT[1] = 0.7f; break;
    default: break;
    }
    const float kc = first ? 1.f : 1.f - std::exp(-dt / 0.45f);
    for (int i = 0; i < 2; i++) camNow[i] += (camT[i] - camNow[i]) * kc;
    // the nebula's shape and density: the pattern's, reached over some 2 s so it changes in place, calmly
    static float morphNow = -1.f;
    const float morph = std::fmod(g_curSeed * 0.6180339f, 1.f) * 6.f;
    morphNow = morphNow < 0.f ? morph : morphNow + (morph - morphNow) * (1.f - std::exp(-dt / 2.f));
    duoNow += (duo - duoNow) * k;
    p.kaleido(g_prev.empty() ? g_curSeed : g_prevSeed, g_curSeed, g_prev.empty() ? 1.f : sstep(g_mix), X(cxNow), Y(cyNow), t * 0.025f + (m.aStage_ + m.aStageTurns_) * (TAU / 12.f) * 0.5f + (menuSeed() >> 8) % 1000 * (TAU / 1000.f), 12,
              mixc(tintNow, hex(0xffffff), 0.15f), t, lvlNow, satNow, duoNow, (menuSeed() % 997) / 997.f, morphNow, camNow[0], camNow[1], m.theme_);
}

// A cut gem in the shape of a kite: four facets round a table at its waist, each lit by how it faces the light (from
// the top left), the table brightest; `lead` rims it in `leadC`, with fine lines between its facets.
void Worlds::kite(float cx, float cy, float a, float r0, float r1, float halfAng, Col c, float alpha, bool lead, Col leadC) {
    const float rm = lerpf(r0, r1, 0.45f), rt = lerpf(r0, r1, 0.52f);
    const float P[4][2] = {{cx + std::cos(a) * r0, cy + std::sin(a) * r0},
                           {cx + std::cos(a - halfAng) * rm, cy + std::sin(a - halfAng) * rm},
                           {cx + std::cos(a) * r1, cy + std::sin(a) * r1},
                           {cx + std::cos(a + halfAng) * rm, cy + std::sin(a + halfAng) * rm}};
    const float tx = cx + std::cos(a) * rt, ty = cy + std::sin(a) * rt;
    const Col table = withA(mixc(c, hex(0xffffff), 0.22f), alpha);
    for (int e = 0; e < 4; e++) {
        const int g = (e + 1) % 4;
        const float dx = (P[e][0] + P[g][0]) * 0.5f - tx, dy = (P[e][1] + P[g][1]) * 0.5f - ty;
        const float len = std::max(1e-3f, std::sqrt(dx * dx + dy * dy));
        const float lit = 0.5f + 0.5f * (dx * -0.55f + dy * -0.83f) / len; // 1: facing the light
        const Col edge = withA(mixc(c, hex(0x04040a), 0.62f - 0.5f * lit), alpha);
        const Col mid = withA(mixc(c, lit > 0.5f ? hex(0xffffff) : hex(0x04040a), std::fabs(lit - 0.5f) * 0.5f), alpha);
        p.tri(tx, ty, P[e][0], P[e][1], P[g][0], P[g][1], table, e % 2 ? mid : edge, e % 2 ? edge : mid);
    }
    if (lead) {
        const float w = 2.4f * u;
        for (int e = 0; e < 4; e++) p.line(P[e][0], P[e][1], P[(e + 1) % 4][0], P[(e + 1) % 4][1], w, leadC);
        const Col fine = hex(0xffffff, 0.22f * alpha);
        for (int e = 0; e < 4; e++) p.line(tx, ty, P[e][0], P[e][1], 0.9f * u, fine);
    }
}

// The quiet setting of a rosette (its petals reach to about R): a soft shade under it, so the petals read over a bright
// crystal, and one fine ring just beyond their tips. (`rot` is kept for the callers; nothing on it turns.)
void Worlds::wheel(float cx, float cy, float R, float rot, float alpha) {
    (void)rot;
    const float k = R / 300.f, A = alpha; // (drawn for petals reaching to 300)
    p.circle(cx, cy, 330 * k * u, hex(0x02040a, 0.32f * A), 110 * k * u);
    p.ring(cx, cy, 318 * k * u, 1.f * u, hex(0xf0dcb0, 0.35f * A));
}

// A big polished dark disc (radius R) in a gold ring: a deep glass, a faint hue of `hue` in it, a curved gloss on its top
// left; `ring` < 1 draws only that much of the gold ring, round from the top (the title's opening).
void Worlds::medallion(float cx, float cy, float R, Col hue, float alpha, float ring) {
    const float A = alpha;
    p.circle(cx, cy, R + 10 * u, hex(0x000000, 0.4f * A), 22 * u);
    p.circle(cx, cy, R, hex(0x06080e, A));
    p.circle(cx, cy + R * 0.1f, R * 0.85f, withA(hue, 0.16f * A), R * 0.6f);
    p.circle(cx - R * 0.25f, cy - R * 0.3f, R * 0.45f, hex(0x8098c0, 0.10f * A), R * 0.7f);
    p.setAdditive(true);
    p.arc(cx, cy, R * 0.84f, -2.75f, -1.25f, R * 0.07f, hex(0xffffff, 0.10f * A), R * 0.07f);
    p.arc(cx, cy, R * 0.92f, -2.5f, -1.6f, R * 0.02f, hex(0xffffff, 0.22f * A), R * 0.02f);
    p.setAdditive(false);
    if (ring <= 0.f) return;
    const float a0 = -PI * 0.5f, a1 = a0 + TAU * std::min(1.f, ring);
    p.arc(cx, cy, R - 3 * u, a0, a1, 2.2f * u, hex(0xe8c06a, 0.9f * A));
    p.setAdditive(true);
    p.arc(cx, cy, R - 3 * u, a0, a1, 6 * u, hex(0xffc860, 0.12f * A), 8 * u);
    p.setAdditive(false);
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
    p.frostSource();
    wheel(cx, cy, 300, base);
    // the order of the journey: an arc from petal to petal (1 -> 12), gold as far as the stages are open
    int nextStage = -1;
    for (int s = 0; s < n && nextStage < 0; s++)
        if (open(s) && !cleared(s)) nextStage = s;
    {
        const float r = 296 * u;
        for (int s = 0; s + 1 < n; s++) {
            const bool o = open(s + 1);
            const float a0 = ang((float)s) + 0.06f * wk, a1 = ang((float)s + 1) - 0.06f * wk;
            if (o) p.arc(cx, cy, r, a0, a1, 2.2f * u, hex(0xe8c06a, 0.7f));
            else
                for (int i = 0; i < 6; i += 2)
                    p.arc(cx, cy, r, lerpf(a0, a1, i / 6.f), lerpf(a0, a1, (i + 1) / 6.f), 1.5f * u, hex(0xb0b0c0, 0.3f));
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
        const Col c = o ? stageCol(s) : hex(0x1c1e26);
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
             !o ? 0.95f : lerpf(0.85f, 0.97f, ck), true,
             focusClosed && (!cel || cT > TURN + 1.f) ? hex(0xffffff, 0.95f * pulse) : o ? edgeCl : hex(0x8a90a0, 0.6f));
        if (focusClosed && f > 0.5f) { // the focused petal calls: arcs spreading from its tip
            const float tip = (262 + 22 * f + 26 * op) * u;
            for (int w = 0; w < 3; w++) {
                const float k = std::fmod(t * 0.7f + w / 3.f, 1.f);
                p.arc(cx, cy, tip + (8 + 26 * k) * u, a - 0.1f * wk, a + 0.1f * wk, 2 * u, withA(mixc(c, hex(0xffffff), 0.5f), 0.8f * std::sin(k * PI) * f));
            }
        }
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
    // the medallion: a polished dark disc, softly tinted by the focused stage
    medallion(cx, cy, 124 * u, open(fs) ? songCol(fs, m.petal_ ? m.song_ : 0) : hex(0x5a5c66), 1.f);
    p.text(F::JOSEFIN_LIGHT, std::to_string(fs + 1), cx, cy + 30 * u, 84 * u, hex(0xffffff, 0.9f), 0, 0.02f);
    // the focused stage, on the right
    {
        const float x = X(1240);
        const bool o = open(fs);
        const int ns = nSongs(fs);
        const float rowH = ns > 0 ? std::min(70.f, 350.f / ns) : 70.f;
        p.glass(X(1198), Y(160), 614 * u, (350 + std::max(0, ns - 1) * rowH + 20 + 32 - 160) * u, 26 * u, u, 0.f, 0.42f);
        p.text(F::JOSEFIN_LIGHT, "STAGE " + std::to_string(fs + 1), x, Y(250), 64 * u, hex(0xffffff), -1, 0.15f);
        const char* st = !o ? "LOCKED" : cleared(fs) ? "CLEARED" : "OPEN";
        p.text(F::JOSEFIN, st, x, Y(292), 18 * u, !o ? hex(0xb0b0b8) : cleared(fs) ? hex(0x86e8a8) : hex(0xffffff, 0.9f), -1, 0.4f);
        // the scores and lines in two right-aligned columns (lines dimmer): the stage's on the header line
        const float colS = x + 430 * u, colL = x + 528 * u;
        if (o && cleared(fs)) {
            p.text(F::JOSEFIN, "BEST " + std::to_string(m.progress_->stageBest(fs, m.diff_)), colS, Y(292), 18 * u, hex(0xffd27a), 1, 0.3f);
            p.text(F::JOSEFIN, linesStr(m.progress_->stageLines(fs, m.diff_)) + " L", colL, Y(292), 18 * u, hex(0xffd27a, 0.7f), 1, 0.3f);
        }
        // the list of songs, spread over the room five rows took
        for (int k = 0; k < ns; k++) {
            const float y = Y(350 + k * rowH);
            const float g = m.aRow_ > 0.5f ? sstep(near1(m.aSong_, k)) * ao : 0.f;
            // a darker pane of glass under each row, lit when focused
            p.glass(x - 16 * u, y - 36 * u, 560 * u, 56 * u, 12 * u, u, g, 0.62f);
            p.rrect(x - 16 * u, y - 36 * u, 560 * u, 56 * u, 12 * u, hex(0xa8d0ff, 0.08f * g));
            const Col sc = o ? songCol(fs, k) : hex(0x5a5c66);
            p.rrect(x, y - 24 * u, 32 * u, 32 * u, 4 * u, sc);
            if (o && beaten(fs, k)) p.check(x + 16 * u, y - 8 * u, 18 * u, 3 * u, hex(0xffffff));
            int vi = 0;
            const int idx = universes::parse(spec(fs, k), &vi);
            std::string name = idx >= 0 ? universes::entry(idx).name : spec(fs, k);
            p.text(F::JOSEFIN, name, x + 52 * u, y, 20 * u, hex(0xffffff, 0.85f + 0.15f * g), -1, 0.25f);
            if (o && beaten(fs, k)) {
                p.text(F::JOSEFIN, text::thousands(m.progress_->best(fs, k, m.diff_)), colS, y, 18 * u, hex(0xffd27a), 1, 0.15f);
                p.text(F::JOSEFIN, linesStr(m.progress_->lines(fs, k, m.diff_)) + " L", colL, y, 18 * u, hex(0xffd27a, 0.7f), 1, 0.15f);
            }
        }
        std::string prompt;
        if (!o) prompt = "LOCKED";
        else if (!m.petal_) prompt = "SPACE  SONGS"; // the one key Enter doesn't explain: the petal's songs
        if (!prompt.empty()) // (on a soft shade, as the tabs)
            p.rrect(x - 30 * u, Y(690), p.width(F::JOSEFIN, prompt, 17 * u, 0.35f) + 60 * u, 44 * u, 22 * u, hex(0x02040c, 0.22f), 40 * u);
        p.text(F::JOSEFIN, prompt, x, Y(720), 17 * u, hex(0xffffff, 0.85f), -1, 0.35f);
    }
    tabs(F::JOSEFIN, hex(0xffffff), hex(0xffffff), 0.35f);
    info(F::JOSEFIN_LIGHT, F::JOSEFIN, hex(0xffffff), hex(0xe0e0f0, 0.65f), hex(0xffffff), 1);
}

void Worlds::kalTitle() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    // the opening (Menu::startIntro), out of the black: the planet, its gold ring sweeping round, the name, the pieces
    // taking their orbits one by one, then the world and the menu
    const float k = m.introT_ < 0.f ? 99.f : m.introT_;
    const float worldA = sstep((k - 2.8f) / 2.2f), menuA = sstep((k - 4.8f) / 0.9f);
    if (worldA < 1.f) p.rrect(-10.f, -10.f, m.fw_ + 20.f, m.fh_ + 20.f, 0.f, hex(0x000000, 1.f - worldA));
    p.frostSource();
    const float dy = TITLE_CY - 330.f;
    const float cx = X(960), cy = Y(TITLE_CY);
    m.drawOrbitLogo(cx, cy, k, 1.f);
    if (menuA <= 0.f) return;
    p.setFade(menuA);
    // (fewer entries, the web build's one: a shorter panel, round the same middle)
    const float less = (3 - Menu::TITLE_ITEMS) * 62.f;
    p.glass(cx - 260 * u, Y(680 + dy + less * 0.5f), 520 * u, (210 - less) * u, 24 * u, u, 0.f, 0.5f);
    p.setFade(1.f);
    menuItems(F::JOSEFIN, 740 + dy + less * 0.5f, 62, 30, hex(0xffffff), hex(0xffffff), 0.35f, menuA);
}

// The options' pages: a title, a glass panel of short rows, an underline under the focused one (eased); the
// kaleidoscope behind tours as the title's. What a choice asks or tells (are you sure? the control a remap wants, a
// "done") takes the rows' place in the panel, in their lettering.
void Worlds::optionPanel(const char* title, int n) {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    p.frostSource();
    p.glass(X(660), Y(90), 600 * u, 130 * u, 24 * u, u, 0.f, 0.45f);
    p.text(F::JOSEFIN_LIGHT, title, X(960), Y(178), 72 * u, hex(0xffffff), 0, 0.22f);
    const float h = std::max(n, 3) * OPT_STEP + 70;
    // centred in the screen's height below the title's panel (which ends at 220)
    optMid = 220.f + (1080.f - 220.f) * 0.5f;
    p.glass(X(560), Y(optMid - h * 0.5f), 800 * u, h * u, 24 * u, u, 0.f, 0.5f);
}

// lines of text centred in the panel, a row's step apart (alpha each)
void Worlds::optionLines(const std::vector<std::pair<std::string, float>>& lines, float size) {
    const float y0 = optMid - (lines.size() - 1) * OPT_STEP * 0.5f + 10;
    for (size_t i = 0; i < lines.size(); i++)
        p.text(F::JOSEFIN, lines[i].first, X(960), Y(y0 + i * OPT_STEP), (i == 0 ? size : size * 0.8f) * u,
               hex(0xffffff, lines[i].second), 0, 0.35f);
}

void Worlds::optionRows(int n, const char* const* label, float focus) {
    const float size = 30, track = 0.35f, y0 = optMid - (n - 1) * OPT_STEP * 0.5f + 10;
    for (int i = 0; i < n; i++)
        p.text(F::JOSEFIN, label[i], X(960), Y(y0 + i * OPT_STEP), size * u, hex(0xffffff, 0.45f + 0.55f * near1(focus, i)), 0, track);
    float inkL = 0, inkR = 0;
    for (int i = 0; i < n; i++) {
        float l, r;
        p.inkExtent(F::JOSEFIN, label[i], size * u, track, l, r);
        const float start = X(960) - p.width(F::JOSEFIN, label[i], size * u, track) * 0.5f;
        inkL += near1(focus, i) * (start + l);
        inkR += near1(focus, i) * (start + r);
    }
    const float ys = Y(y0 + focus * OPT_STEP) + 14 * u;
    p.line(inkL, ys, inkR, ys, 1.6f * u, hex(0xffffff, 0.9f));
}

// "done": the panel tells it for a moment (a key goes back to the rows)
bool Worlds::optionNote() {
    if (m.noteT_ <= 0.f) return false;
    const float a = std::min(1.f, m.noteT_ / 0.3f) * std::min(1.f, (Menu::NOTE_TIME - m.noteT_) / 0.2f + 0.2f);
    optionLines({{m.note_, a}}, 30);
    return true;
}

// The options: unlocking every stage (as --unlock), forgetting the saved data (as --reset), either asking "are you
// sure?" first (YES / NO), and the gamepad's page.
void Worlds::kalOptions() {
    optionPanel("OPTIONS", Menu::OPTION_ITEMS);
    if (optionNote()) return;
    if (!m.optConfirm_) {
        const char* LABEL[Menu::OPTION_ITEMS] = {"UNLOCK STAGES", "RESET SAVED DATA", "GAMEPAD",
                                                 m.music_ ? "MENU MUSIC: ON" : "MENU MUSIC: OFF",
                                                 m.theme_ ? "MENU THEME: KALEIDOSCOPE" : "MENU THEME: SPACE"};
        optionRows(Menu::OPTION_ITEMS, LABEL, m.aOpt_);
        return;
    }
    // are you sure? then YES / NO, the answer underlined
    const float track = 0.35f, size = 30;
    optionLines({{m.optSel_ == 0 ? "UNLOCK STAGES?" : "RESET SAVED DATA?", 1.f}}, size);
    const float y = optMid + OPT_STEP * 0.5f + 10;
    const char* ANS[2] = {"NO", "YES"}; // (optYes_: 0 NO, 1 YES; YES on the left)
    const float ax[2] = {1080, 840};
    for (int i = 0; i < 2; i++)
        p.text(F::JOSEFIN, ANS[i], X(ax[i]), Y(y), size * u, hex(0xffffff, 0.45f + 0.55f * near1(m.aYes_, i)), 0, track);
    float l0, r0, l1, r1;
    p.inkExtent(F::JOSEFIN, ANS[0], size * u, track, l0, r0);
    p.inkExtent(F::JOSEFIN, ANS[1], size * u, track, l1, r1);
    const float s0 = X(ax[0]) - p.width(F::JOSEFIN, ANS[0], size * u, track) * 0.5f, s1 = X(ax[1]) - p.width(F::JOSEFIN, ANS[1], size * u, track) * 0.5f;
    const float k = std::clamp(m.aYes_, 0.f, 1.f);
    p.line(s0 + l0 + (s1 + l1 - s0 - l0) * k, Y(y) + 14 * u, s0 + r0 + (s1 + r1 - s0 - r0) * k, Y(y) + 14 * u, 1.6f * u, hex(0xffffff, 0.9f));
}

// The gamepad: remapping it control by control, or its own mapping back; a remap asks for each control in the panel.
void Worlds::kalGamepad() {
    const Input* in = m.input_;
    optionPanel("GAMEPAD", Menu::PAD_ITEMS);
    if (optionNote()) return;
    if (in && in->remapping()) {
        const int st = in->remapStep();
        optionLines({{std::string("PRESS ") + Input::remapName(st), 1.f},
                     {std::string(Input::remapUse(st)) + "   " + std::to_string(st + 1) + " / " + std::to_string(Input::REMAP_COUNT), 0.55f},
                     {in->gamepadConnected() ? "SPACE SKIPS   ESC CANCELS" : "CONNECT A GAMEPAD   ESC CANCELS", 0.4f}}, 30);
        return;
    }
    const char* LABEL[Menu::PAD_ITEMS] = {"REMAP", "RESTORE MAPPING"};
    optionRows(Menu::PAD_ITEMS, LABEL, m.aPad_);
}

void Worlds::kalFree() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    p.frostSource();
    p.glass(X(560), Y(90), 800 * u, 130 * u, 24 * u, u, 0.f, 0.45f);
    p.text(F::JOSEFIN_LIGHT, "FREE PLAY", X(960), Y(178), 72 * u, hex(0xffffff), 0, 0.22f);
    const bool rows = m.playlists_ != nullptr;
    const float onDiff = rows ? 1.f - std::min(1.f, m.aFreeRow_) : 1.f;
    // the difficulties' gems: teal, violet, rose, and EXPERT an ember orange
    static const uint32_t C[DIFF_COUNT] = {0x6fd8c0, 0x9c86ff, 0xff6f9c, 0xff8a3d};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(m.aFree_, d)), x = X(960 + (d - 0.5f * (DIFF_COUNT - 1)) * 400.f), y = Y(405);
        const float R = (108 + 20 * f * onDiff) * u;
        // a setting round it: a bronze ring and a blue orbit, gold when chosen
        p.circle(x, y, R + 30 * u, hex(0x0a1020, 0.35f), 30 * u);
        p.ring(x, y, R + 12 * u, 2.2f * u, mixc(hex(0x7a5c44, 0.8f), hex(0xe8c06a, 0.95f), f));
        p.ring(x, y, R + 24 * u, 1.2f * u, hex(0x9cc8ff, 0.3f + 0.4f * f));
        if (f > 0.05f) {
            p.setAdditive(true);
            p.ring(x, y, R + 24 * u, 6 * u, hex(0x3a7cff, 0.18f * f), 10 * u);
            p.setAdditive(false);
        }
        for (int i = 0; i < 4; i++)
            kite(x, y, t * 0.05f * freePlayMultiplier(d) + i * TAU / 4, 0, R, 0.55f, hex(C[d]), 0.5f + 0.45f * f, true,
                 f > 0.5f && onDiff > 0.5f ? hex(0xffffff, 0.9f) : hex(0x08080c, 0.9f));
        p.glass(x - 110 * u, y + R + 40 * u, 220 * u, 78 * u, 16 * u, u, f * onDiff, 0.5f);
        p.text(F::JOSEFIN, difficultyName(d), x, y + R + 76 * u, (24 + 4 * f) * u, hex(0xffffff, 0.5f + 0.5f * f), 0, 0.35f);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", freePlayMultiplier(d));
        p.text(F::JOSEFIN, b, x, y + R + 104 * u, 18 * u, hex(0xffffff, 0.45f + 0.4f * f), 0, 0.1f);
    }
    if (rows) {
        // the playlists as glass cards: the game's own first (featured: larger, a gold rim, the stages' hues), then the
        // player's, each with a hue and a small rosette of its own (seeded from its playlist ID), then "add one"
        const float onPl = near1(m.aFreeRow_, 1);
        p.glass(X(120), Y(680), 1680 * u, 262 * u, 24 * u, u, 0.4f * onPl, 0.55f);
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
                p.frost(x, y, w, h, 20 * u, mixc(hue, hex(0x0a0a12), 0.8f), 0.82f);
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
        // the starting level: a strip of coarse steps along the bottom (HARD's and EXPERT's hyperdrop levels in its colour)
        const float onLvl = std::clamp(m.aFreeRow_ - 1.f, 0.f, 1.f);
        const float ly = Y(962), lh = 78 * u, cw = 120 * u, lx = X(440);
        p.glass(X(120), ly, 1680 * u, lh, 24 * u, u, 0.4f * onLvl, 0.55f);
        p.text(F::JOSEFIN, "STARTING LEVEL", X(160), ly + lh / 2 + 6 * u, 16 * u, hex(0xffffff, 0.55f + 0.45f * onLvl), -1, 0.45f);
        const float sx = lx + (m.aLvl_ + 0.5f) * cw; // the selection, sliding under the numbers
        p.rrect(sx - cw / 2 + 8 * u, ly + 10 * u, cw - 16 * u, lh - 20 * u, 14 * u, hex(0xffffff, 0.08f + 0.08f * onLvl));
        p.rrectLine(sx - cw / 2 + 8 * u, ly + 10 * u, cw - 16 * u, lh - 20 * u, 14 * u, (1.4f + 1.2f * onLvl) * u, hex(0xffffff, 0.45f + 0.5f * onLvl));
        for (int i = 0; i < Menu::FREE_LEVEL_COUNT; i++) {
            const int lv = Menu::FREE_LEVELS[i];
            const float f = sstep(near1(m.aLvl_, i)), x = lx + (i + 0.5f) * cw;
            const float from = Game::hyperdropFrom(m.freeDiff_);
            const bool g20 = from > 0.f && lv >= from;
            const uint32_t gc = C[std::clamp(m.freeDiff_, 0, DIFF_COUNT - 1)];
            p.text(F::JOSEFIN, std::to_string(lv), x, ly + lh / 2 + (g20 ? 3 : 9) * u, (24 + 4 * f) * u,
                   withA(g20 ? hex(gc) : hex(0xffffff), 0.45f + 0.55f * f), 0, 0.1f);
            if (g20) p.text(F::JOSEFIN, "HYPERDROP", x, ly + lh / 2 + 21 * u, 10 * u, hex(gc, 0.5f + 0.4f * f), 0, 0.12f);
        }
    }
    // only the keys a player wouldn't guess (paste, remove); the arrows, Enter and Esc go without saying
    const char* hint = m.editing_ ? "CTRL+V  PASTE" : (rows && m.freeRow_ == 1) ? "DEL  REMOVE" : "";
    if (*hint) p.text(F::JOSEFIN, hint, X(1760), Y(714), 15 * u, hex(0xffffff, 0.55f), 1, 0.3f);
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

// The voyages, as free play's playlists: glass cards of one size in a grid of two columns (scrolling to the focused
// card's row, the rows beyond the three in view fading out), the game's own first (featured: a gold rim), then the ones
// made from the player's playlists (the last played first, each with its own hue), then "add a playlist". Each card
// has a small rosette, a kite per stage (the cleared ones lit), and how far the voyage went (its songs beaten, in %).
// Adding one: a panel over the lower rows for the playlist's link, then the voyage's name.
void Worlds::kalVoyages() {
    std::string cur, next;
    float mix;
    tourAt(11.f, cur, next, mix);
    kalBackground(mix > 0.5f ? next : cur);
    p.frostSource();
    p.glass(X(660), Y(90), 600 * u, 130 * u, 24 * u, u, 0.f, 0.45f);
    p.text(F::JOSEFIN_LIGHT, "VOYAGE", X(960), Y(178), 72 * u, hex(0xffffff), 0, 0.22f);
    if (!m.voyages_) return;
    const auto& vs = m.voyages_->list();
    const int nc = (int)vs.size() + 2, cols = Menu::VOYAGE_COLS, rows = (nc + cols - 1) / cols;
    const bool making = m.vStep_ > 0;
    struct Spin { float angle = 0, speed = 0; };
    static std::map<uint32_t, Spin> spins; // the cards' rosettes, by card
    static double lastT = t;
    const float dt = (float)std::clamp(t - lastT, 0.0, 0.1);
    lastT = t;
    constexpr float CW = 660, CH = 190, GAP = 40, STEP = CH + 44, TOP = 290;
    constexpr int SHOWN = 3; // rows in view
    const float scroll = std::clamp(m.aVRow_ - 1.f, 0.f, (float)std::max(0, rows - SHOWN));
    const float x0 = 960 - (cols * CW + (cols - 1) * GAP) / 2;
    const float w = CW * u, h = CH * u;
    auto hueCol = [](float hh, float s, float v) {
        auto f = [&](float n) { const float k2 = std::fmod(n + hh * 6.f, 6.f); return v - v * s * std::max(0.f, std::min({k2, 4.f - k2, 1.f})); };
        return Col{f(5), f(3), f(1), 1.f};
    };
    for (int i = 0; i < nc; i++) {
        const float rp = (float)(i / cols) - scroll; // the row's place in view (0 .. SHOWN - 1)
        const float fade = std::clamp(1.f + rp * 1.6f, 0.f, 1.f) * std::clamp((SHOWN - rp) * 1.6f, 0.f, 1.f);
        if (fade <= 0.01f) continue;
        p.setFade(fade);
        // the focused card is lit, the others dull (no easing between them: in a grid it would pass over other cards)
        const bool sel = i == m.vSel_ && !making;
        const float g = sel ? 1.f : 0.f;
        const float x = X(x0 + (i % cols) * (CW + GAP)), y = Y(TOP + rp * STEP);
        const bool featured = i == 0, add = i == nc - 1;
        const CustomVoyage* v = !featured && !add ? &vs[i - 1] : nullptr;
        uint32_t seed = 2166136261u;
        for (char c : add ? std::string("+") : v ? v->id : std::string("refall")) seed = (seed ^ (unsigned char)c) * 16777619u;
        const Col hue0 = featured ? hex(0xe8c06a) : add ? hex(0x9aa0b8) : hueCol((seed % 360) / 360.f, 0.55f, 0.95f);
        const Col hue = sel ? hue0 : mixc(hue0, hex(0x6a6c78), 0.6f); // dull: toward a grey
        if (sel) {
            p.setAdditive(true);
            p.rrect(x - 6 * u, y - 6 * u, w + 12 * u, h + 12 * u, 26 * u, withA(featured ? hex(0xffc860) : hex(0xffffff), 0.32f), 30 * u);
            p.setAdditive(false);
        }
        // frosted glass as the other panels (the focused one lit a little, in its hue)
        p.glass(x, y, w, h, 20 * u, u, sel ? 0.35f : 0.f, 0.45f);
        if (sel) p.rrect(x, y, w, h, 20 * u, withA(hue, 0.1f));
        p.rrectLine(x, y, w, h, 20 * u, (featured ? 2.6f : 1.4f) * u + 1.2f * g * u,
                    sel ? hex(0xffffff, 0.95f) : featured ? hex(0xe8c06a, 0.5f) : withA(hue, 0.35f));
        p.setFade(fade * (sel ? 1.f : 0.6f)); // the dull cards' contents dimmer
        const float mx = x + 100 * u, my = y + h / 2, mr = 70 * u;
        if (add) {
            p.ring(mx, my, 40 * u, 2 * u, hex(0xffffff, 0.5f + 0.4f * g));
            p.line(mx - 17 * u, my, mx + 17 * u, my, 2.6f * u, hex(0xffffff, 0.7f + 0.3f * g));
            p.line(mx, my - 17 * u, mx, my + 17 * u, 2.6f * u, hex(0xffffff, 0.7f + 0.3f * g));
            p.text(F::JOSEFIN, "ADD A PLAYLIST", mx + mr + 28 * u, my + 8 * u, 22 * u, hex(0xffffff, 0.6f + 0.4f * g), -1, 0.3f);
            continue;
        }
        const Campaign& mc = m.voyages_->main();
        const int st = std::max(1, featured ? mc.size() : v->stages);
        const VoyageProgress pr = i < (int)m.vProg_.size() ? m.vProg_[i] : VoyageProgress{};
        int songs = pr.songs;
        if (!songs && featured) for (const CampaignStage& s2 : mc.stages()) songs += (int)s2.songs.size();
        if (!songs && v) songs = v->songs;
        // the rosette: a kite per stage in the stages' hues, the cleared ones lit
        // only the focused one turns: each card keeps its own angle, its speed easing in and out
        Spin& sp = spins[seed];
        sp.speed += ((sel ? (featured ? 0.06f : 0.04f + ((seed >> 4) % 5) * 0.01f) : 0.f) - sp.speed) * (1.f - std::exp(-dt * 3.f));
        sp.angle += sp.speed * dt;
        const float rot = sp.angle + (seed % 100) * 0.0628f;
        for (int k = 0; k < st; k++) {
            unsigned char sc[3];
            m.stageColor(k, sc);
            const Col c = rgb8(sc);
            const Col lit = sel ? mixc(c, hex(0xffffff), 0.12f) : mixc(c, hex(0x6a6c78), 0.55f);
            // (the stages not cleared yet darker: on the focused card only a little)
            // (the focused card's rosette in full color, its stages cleared or not; a dull card's uncleared ones darker)
            kite(mx, my, rot + k * TAU / st, mr * 0.22f, mr, std::min(0.42f, 2.8f / st), k < pr.cleared || sel ? lit : mixc(lit, hex(0x14141c), 0.55f),
                 sel ? 1.f : 0.92f, true, hex(0x08080c, 0.9f));
        }
        p.circle(mx, my, mr * 0.22f, hex(0x0a0a12));
        p.ring(mx, my, mr * 0.22f, 1.5f * u, featured || pr.cleared >= st ? hex(0xe8c06a) : withA(hue, 0.9f));
        // how far it went, large on the right
        const int pc = pr.percent();
        const float pw = p.text(F::JOSEFIN_LIGHT, std::to_string(pc) + "%", x + w - 36 * u, my + 16 * u, 46 * u,
                                pc > 0 ? hex(0xffd27a, 0.95f) : hex(0xffffff, sel ? 0.8f : 0.35f), 1, 0.02f);
        // the name, its songs and stages, its stages cleared
        const float tx = mx + mr + 30 * u, tmax = x + w - 36 * u - pw - 24 * u - tx;
        std::string title = asciiFold(featured ? std::string(GAME_TITLE) : v->title);
        const float ts = (featured ? 38.f : 28.f) * u;
        const F tf = featured ? F::JOSEFIN_LIGHT : F::JOSEFIN;
        while (title.size() > 3 && p.width(tf, title, ts, 0.04f) > tmax) title.pop_back();
        if (featured) p.text(F::JOSEFIN, "FEATURED", tx, my - 44 * u, 14 * u, sel ? hex(0xffffff) : hex(0xe8c06a), -1, 0.45f);
        p.text(tf, title, tx, my + (featured ? 4.f : 0.f) * u, ts, hex(0xffffff, 0.8f + 0.2f * g), -1, 0.04f);
        char b[96];
        std::snprintf(b, sizeof(b), "%d SONGS  /  %d STAGES", songs, st);
        p.text(F::JOSEFIN, b, tx, my + 36 * u, 14 * u, sel ? hex(0xffffff, 0.92f) : withA(hue, 0.85f), -1, 0.3f);
        if (pr.cleared > 0) {
            std::snprintf(b, sizeof(b), "%d CLEARED", pr.cleared);
            p.text(F::JOSEFIN, b, tx, my + 60 * u, 14 * u, hex(0xffd27a, 0.9f), -1, 0.3f);
        }
    }
    p.setFade(1.f);
    if (rows > SHOWN) { // where the view is in the list: a thin track on the right
        const float tx = X(x0 + cols * CW + (cols - 1) * GAP + 34), ty = Y(TOP), th = (SHOWN * STEP - 44) * u;
        p.rrect(tx - 2 * u, ty, 4 * u, th, 2 * u, hex(0xffffff, 0.15f));
        const float kh = th * SHOWN / rows, ky = ty + (th - kh) * scroll / (float)(rows - SHOWN);
        p.rrect(tx - 2 * u, ky, 4 * u, kh, 2 * u, hex(0xffffff, 0.7f));
    }
    if (making) {
        p.glass(X(120), Y(650), 1680 * u, 230 * u, 24 * u, u, 0.5f, 0.85f);
        p.text(F::JOSEFIN, m.vStep_ == 1 ? "PLAYLIST" : "NAME", X(160), Y(686), 16 * u, hex(0xffffff), -1, 0.45f);
        textField(X(160), Y(712), 1600 * u, 62 * u, m.editText_,
                  m.vStep_ == 1 ? "Type or paste (Ctrl+V) a YouTube playlist link or ID" : nullptr);
    }
    if (!m.plMsg_.empty())
        p.text(F::JOSEFIN, upper(m.plMsg_), making ? X(160) : X(960), Y(making ? 830 : 1000), 16 * u,
               m.plMsgBad_ ? hex(0xff9a9a) : hex(0xffe0a0), making ? -1 : 0, 0.3f);
    // only the keys a player wouldn't guess (paste, delete)
    const char* hint = m.vStep_ == 1 ? "CTRL+V  PASTE" : !making && m.vSel_ >= 1 && m.vSel_ <= (int)vs.size() ? "DEL  DELETE" : "";
    if (*hint) p.text(F::JOSEFIN, hint, X(960), Y(1050), 15 * u, hex(0xffffff, 0.55f), 0, 0.3f);
}

// The game's logo (the title, the debriefs): a planet turning slowly under a light from the top left, banded in blues and violets, a thin sky
// on its rim; a tilted gold ring round it (its far half behind the planet, its near half in front), small pieces of
// the game riding it, passing behind and in front, a few glints twinkling about; the name under it. `k`: the time
// since the opening began (Menu::startIntro); `color` < 1 greys it toward silver (a game over).
void Menu::drawOrbitLogo(float cx, float cy, float k, float color) {
    Painter& p = paint_;
    const float u = u_, t = time_;
    const int nStages = camp_ ? camp_->size() : 0;
    // silver: each colour toward a light grey of its own brightness
    auto tone = [&](Col c) {
        const float g = std::min(1.f, 0.25f + 0.85f * (0.3f * c.r + 0.59f * c.g + 0.11f * c.b));
        return mixc(Col{g, g, g * 1.04f, c.a}, c, color);
    };
    auto H = [&](uint32_t x, float a = 1.f) { return tone(hex(x, a)); };
    auto stageCol = [&](int s) { unsigned char c3[3]; stageColor(s, c3); return rgb8(c3); };
    const float planetA = sstep((k - 0.5f) / 1.2f), ringA = sstep((k - 1.1f) / 1.6f), nameA = sstep((k - 1.6f) / 1.4f);
    const float R = 138 * u * lerpf(0.9f, 1.f, planetA), pcy = cy - 40 * u;
    const float tilt = -0.24f, ct = std::cos(tilt), st = std::sin(tilt);
    const float ra = 320 * u, rb = 70 * u; // the ring's ellipse, before its tilt
    auto onRing = [&](float th, float sc, float& x, float& y) {
        const float ex = ra * sc * std::cos(th), ey = rb * sc * std::sin(th);
        x = cx + ex * ct - ey * st;
        y = pcy + ex * st + ey * ct;
    };
    // a soft shade under it all, so it reads over a bright crystal
    p.circle(cx, pcy + 30 * u, 300 * u, hex(0x02040a, 0.34f * planetA), 160 * u); // (never silvered)
    // the glints
    for (int i = 0; i < 14; i++) {
        const float gx = cx + (std::fmod(i * 397.f, 760.f) - 380.f) * u, gy = pcy + (std::fmod(i * 241.f, 400.f) - 220.f) * u;
        const float tw = std::pow(0.5f + 0.5f * std::sin(t * (0.6f + 0.13f * i) + i * 1.7f), 6.f);
        if (std::hypot(gx - cx, gy - pcy) < R * 1.3f) continue;
        p.setAdditive(true);
        p.glow(gx, gy, (6 + 5 * tw) * u, H(0xfff2d8, 0.5f * tw * ringA));
        p.setAdditive(false);
    }
    // the ring: a few bands of gold, flat (thin where it is seen edge-on, at its near and far sides); `front` draws its
    // near half
    struct Band { float sc0, sc1, a; uint32_t c; };
    static const Band BANDS[] = {{0.76f, 0.80f, 0.22f, 0xb08850}, {0.81f, 0.90f, 0.42f, 0xc89a58}, {0.905f, 0.955f, 0.8f, 0xe8c06a},
                                 {0.975f, 0.99f, 0.9f, 0xf6dca0}};
    auto ringHalf = [&](bool front) {
        if (ringA <= 0.f) return;
        const int N = 90;
        for (const Band& b : BANDS)
            for (int i = 0; i < N; i++) {
                const float t0 = (front ? 0.f : PI) + PI * i / N, t1 = t0 + PI / N;
                // the opening: it sweeps round from the left
                if (std::fmod(t0 - PI + TAU, TAU) / TAU > ringA) continue;
                float xa, ya, xb, yb, xc, yc, xd, yd;
                onRing(t0, b.sc0, xa, ya);
                onRing(t1, b.sc0, xb, yb);
                onRing(t1, b.sc1, xc, yc);
                onRing(t0, b.sc1, xd, yd);
                // lit from the top left; the far half in the planet's shade
                const float lit = (front ? 1.f : 0.55f) * (0.75f + 0.25f * std::cos(t0 + 2.2f));
                const Col c = H(b.c, b.a * lit * ringA);
                p.tri(xa, ya, xb, yb, xc, yc, c, c, c);
                p.tri(xa, ya, xc, yc, xd, yd, c, c, c);
            }
        // a light running round it
        const float th = std::fmod(t * 0.35f, TAU);
        if ((th < PI) == front) {
            float x, y;
            onRing(th, 0.93f, x, y);
            p.setAdditive(true);
            p.glow(x, y, 26 * u, H(0xffd890, 0.22f * ringA));
            p.setAdditive(false);
        }
    };
    // the pieces on the ring: one shape each, tumbling slowly; drawn by halves (behind the planet, then in front)
    struct Piece { float cells[4][2]; int col; float th0, spin; };
    static const Piece PIECES[] = {
        {{{-1, 0}, {0, 0}, {1, 0}, {0, -1}}, 0, 0.3f, 0.5f},         // T
        {{{-1, 0}, {0, 0}, {1, 0}, {1, -1}}, 1, 1.55f, -0.4f},       // L
        {{{-1, 0}, {0, 0}, {0, -1}, {1, -1}}, 4, 2.8f, 0.35f},       // S
        {{{-1.5f, 0}, {-0.5f, 0}, {0.5f, 0}, {1.5f, 0}}, 5, 4.1f, -0.3f}, // I
        {{{-0.5f, -0.5f}, {0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f}}, 2, 5.3f, 0.45f}}; // O
    auto pieces = [&](bool front) {
        int i = 0;
        for (const Piece& pc : PIECES) {
            const float a = sstep((k - 2.3f - 0.25f * i) / 0.7f);
            const float th = std::fmod(pc.th0 + t * 0.09f + TAU * 4, TAU);
            const float depth = std::sin(th); // > 0: the near side
            i++;
            if (a <= 0.f || (depth >= 0.f) != front) continue;
            float x, y;
            onRing(th, 0.93f, x, y);
            // each cell a cube; the piece tumbles slowly about its own axes, seen straight on (orthographic): its faces
            // drawn far to near, each lit from the top left and the front, a sheen on the faces turned to the light,
            // the faces between two of its cells left out
            const float cs = (17.f + 4.f * depth) * u;
            const float ax = t * pc.spin * 0.9f + i * 1.3f, ay = t * pc.spin * 0.6f + i * 2.1f, az = t * pc.spin * 0.35f + i;
            const float cx_ = std::cos(ax), sx_ = std::sin(ax), cy_ = std::cos(ay), sy_ = std::sin(ay), cz_ = std::cos(az), sz_ = std::sin(az);
            auto R = [&](float vx, float vy, float vz, float o[3]) { // Rz * Ry * Rx
                const float y1 = vy * cx_ - vz * sx_, z1 = vy * sx_ + vz * cx_;
                const float x2 = vx * cy_ + z1 * sy_, z2 = -vx * sy_ + z1 * cy_;
                o[0] = x2 * cz_ - y1 * sz_;
                o[1] = x2 * sz_ + y1 * cz_;
                o[2] = z2;
            };
            const Col c = mixc(tone(stageCol(pc.col % std::max(1, nStages))), H(0x05060c), 0.25f - 0.25f * depth);
            float mxc = 0, myc = 0;
            for (const auto& ce : pc.cells) { mxc += ce[0] * 0.25f; myc += ce[1] * 0.25f; }
            struct Face { float P[4][2]; float z; Col col; };
            Face faces[24];
            int nf = 0;
            static const float N0[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            const float Lx = -0.45f, Ly = -0.6f, Lz = 0.66f; // the light: up-left and toward us (screen y down, z to us)
            for (const auto& ce : pc.cells) {
                const float ox = ce[0] - mxc, oy = ce[1] - myc;
                for (int fi = 0; fi < 6; fi++) {
                    const float* n0 = N0[fi];
                    bool inner = false; // a neighbour cell on that side
                    for (const auto& ce2 : pc.cells)
                        if (std::fabs(ce2[0] - (ce[0] + n0[0])) < 0.01f && std::fabs(ce2[1] - (ce[1] + n0[1])) < 0.01f && n0[2] == 0.f) inner = true;
                    if (inner) continue;
                    float n[3];
                    R(n0[0], n0[1], n0[2], n);
                    if (n[2] <= 0.f) continue; // facing away
                    // its corners: the face's centre +- two axes perpendicular to its normal
                    const float ux = n0[1] != 0.f || n0[2] != 0.f ? 1.f : 0.f, uy = n0[0] != 0.f ? 1.f : 0.f, uz = 0.f;
                    const float vx = n0[2] != 0.f ? 0.f : (n0[0] != 0.f ? 0.f : 0.f), vy = n0[2] != 0.f ? 1.f : 0.f, vz = n0[2] != 0.f ? 0.f : 1.f;
                    const float corner[4][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
                    Face& f = faces[nf++];
                    float zc = 0;
                    for (int e = 0; e < 4; e++) {
                        const float lx = ox + n0[0] * 0.5f + ux * corner[e][0] + vx * corner[e][1];
                        const float ly = oy + n0[1] * 0.5f + uy * corner[e][0] + vy * corner[e][1];
                        const float lz = n0[2] * 0.5f + uz * corner[e][0] + vz * corner[e][1];
                        float o[3];
                        R(lx, ly, lz, o);
                        f.P[e][0] = x + o[0] * cs;
                        f.P[e][1] = y - 4 * u + o[1] * cs;
                        zc += o[2] * 0.25f;
                    }
                    f.z = zc;
                    const float lam = std::max(0.f, n[0] * Lx + n[1] * Ly + n[2] * Lz);
                    const float hx = Lx, hy = Ly, hz = Lz + 1.f, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
                    const float spec = std::pow(std::max(0.f, (n[0] * hx + n[1] * hy + n[2] * hz) / hl), 24.f);
                    const float sh = 0.28f + 0.85f * lam;
                    Col fc{std::min(1.f, c.r * sh + 0.6f * spec), std::min(1.f, c.g * sh + 0.6f * spec), std::min(1.f, c.b * sh + 0.6f * spec), a};
                    f.col = fc;
                }
            }
            std::sort(faces, faces + nf, [](const Face& A, const Face& B) { return A.z < B.z; });
            for (int k2 = 0; k2 < nf; k2++) {
                const Face& f = faces[k2];
                // a face: its colour, a slightly lighter rim toward the light and a thin dark edge (the cube's bevel)
                const Col rim = withA(mixc(f.col, H(0xffffff), 0.18f), a), edge = withA(mixc(f.col, H(0x05060c), 0.45f), a);
                p.tri(f.P[0][0], f.P[0][1], f.P[1][0], f.P[1][1], f.P[2][0], f.P[2][1], edge, edge, edge);
                p.tri(f.P[0][0], f.P[0][1], f.P[2][0], f.P[2][1], f.P[3][0], f.P[3][1], edge, edge, edge);
                float Q[4][2];
                const float mx2 = (f.P[0][0] + f.P[1][0] + f.P[2][0] + f.P[3][0]) * 0.25f, my2 = (f.P[0][1] + f.P[1][1] + f.P[2][1] + f.P[3][1]) * 0.25f;
                for (int e = 0; e < 4; e++) { Q[e][0] = mx2 + (f.P[e][0] - mx2) * 0.88f; Q[e][1] = my2 + (f.P[e][1] - my2) * 0.88f; }
                p.tri(Q[0][0], Q[0][1], Q[1][0], Q[1][1], Q[2][0], Q[2][1], rim, f.col, f.col);
                p.tri(Q[0][0], Q[0][1], Q[2][0], Q[2][1], Q[3][0], Q[3][1], rim, f.col, f.col);
            }
        }
    };
    // the ring, the pieces and the planet are flat triangles: drawn at twice the resolution, then laid down filtered
    // (their edges anti-aliased)
    p.beginLayer(cx - ra - 60 * u, pcy - R - 60 * u, 2 * (ra + 60 * u), 2 * (R + 60 * u));
    ringHalf(false);
    pieces(false);
    // the planet: a sphere of small facets, each corner lit by its normal; its bands follow its own axis (the ring's)
    if (planetA > 0.f) {
        const int NR = 12, NS = 56;
        const float L[3] = {-0.52f, -0.62f, 0.59f};
        auto shade = [&](float r, float phi, float& x, float& y) {
            const float nx = r * std::cos(phi), ny = r * std::sin(phi), nz = std::sqrt(std::max(0.f, 1.f - r * r));
            x = cx + nx * R;
            y = pcy + ny * R;
            // in the planet's frame (its axis tilted as the ring)
            const float py = -nx * st + ny * ct, px = nx * ct + ny * st;
            const float lon = std::atan2(px, nz) + t * 0.12f;
            const float band = 0.5f + 0.5f * std::sin(py * 10.f + 0.7f * std::sin(lon * 2.f + py * 3.f));
            const float fine = std::pow(0.5f + 0.5f * std::sin(py * 31.f + 0.4f * std::sin(lon * 3.f)), 8.f);
            Col base = mixc(H(0x2032a8), H(0x6a80ff), band);
            base = mixc(base, H(0xb8c8ff), 0.35f * fine);
            base = mixc(base, H(0x7a4ad0), 0.35f * std::pow(std::fabs(py), 2.f));
            const float lam = std::max(0.f, nx * L[0] + ny * L[1] + nz * L[2]);
            const float rim = std::pow(1.f - nz, 3.f);
            Col c{base.r * (0.1f + 0.95f * lam), base.g * (0.1f + 0.95f * lam), base.b * (0.12f + 0.95f * lam), planetA};
            c = mixc(c, H(0x8cc4ff, planetA), rim * (0.25f + 0.6f * lam));
            return c;
        };
        for (int ri = 0; ri < NR; ri++) {
            const float r0 = (float)ri / NR, r1 = (float)(ri + 1) / NR;
            for (int si = 0; si < NS; si++) {
                const float f0 = TAU * si / NS, f1 = TAU * (si + 1) / NS;
                float x00, y00, x01, y01, x10, y10, x11, y11;
                const Col c00 = shade(r0, f0, x00, y00), c01 = shade(r0, f1, x01, y01);
                const Col c10 = shade(r1, f0, x10, y10), c11 = shade(r1, f1, x11, y11);
                if (ri > 0) p.tri(x00, y00, x01, y01, x11, y11, c00, c01, c11);
                p.tri(x00, y00, x11, y11, x10, y10, c00, c11, c10);
            }
        }
        // its sky: a thin light on the rim, brighter toward the sun; a soft sheen where the light falls
        p.setAdditive(true);
        p.glow(cx - 0.42f * R, pcy - 0.5f * R, 0.5f * R, H(0xc8d8ff, 0.12f * planetA));
        p.ring(cx, pcy, R, 2.5f * u, H(0x9cd0ff, 0.35f * planetA), 3 * u);
        p.arc(cx, pcy, R + 2 * u, -2.9f, -0.9f, 5 * u, H(0x9cd0ff, 0.18f * planetA), 10 * u);
        p.setAdditive(false);
    }
    ringHalf(true);
    pieces(true);
    p.endLayer();
    // the name
    const std::string title = upper(GAME_TITLE);
    const float size = 60 * u;
    p.setAdditive(true);
    p.glow(cx, pcy + 236 * u, 120 * u, H(0xe8c06a, (0.05f + 0.02f * std::sin(t * 0.8f)) * nameA));
    p.setAdditive(false);
    p.text(F::JOSEFIN_LIGHT, title, cx, pcy + 236 * u + size * 0.36f, size, H(0xffffff, nameA), 0, lerpf(0.8f, 0.5f, nameA));
}

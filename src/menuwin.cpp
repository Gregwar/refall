// WINDOWS and KALEIDO: the menus that show the game's real universes.
//   WINDOWS  a dark gallery wall of 12 framed windows (the stages), each opening onto a live universe, with a strip of
//            5 panes (the songs) under it; the focused pane grows; locked windows have their shutters closed, cleared
//            ones warm light and a brass plaque; the camera glides along the wall.
//   KALEIDO  the focused song's live universe folded into a slowly turning stained-glass kaleidoscope; the stages are
//            glass shards in a rosette around a plain dark glass medallion, the songs smaller facets; focus turns the rosette
//            to bring the chosen shard to the reading position (the right).
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
    bool kal;
    Worlds(Menu& mm) : m(mm), p(mm.paint_), pool(*mm.worlds_), u(mm.u_), t(mm.time_), kal(mm.style_ == Menu::KALEIDO) {}
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
    void showImage(const std::string& sp, float x, float y, float w, float h, Col tint, float r = 0.f);
    // WINDOWS
    void wall(float camX);
    void window(int s, float d);
    void winTitle();
    void winFree();
    void winCampaign();
    // KALEIDO
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
    if (m.screen_ == Menu::TITLE || (m.screen_ == Menu::FREE && kal)) {
        std::string cur, next;
        float mix;
        tourAt(kal ? 11.f : 9.f, cur, next, mix);
        pool.want(cur, 30);
        if (mix > 0.f || std::fmod(t, kal ? 11.f : 9.f) > (kal ? 11.f : 9.f) - 3.f) pool.want(next, 30);
        else pool.want(next, 0);
        return;
    }
    if (m.screen_ == Menu::FREE) {
        const std::string s3[3] = {spec(0, 0), spec(5, 2), spec(11, 4)};
        for (int d = 0; d < DIFF_COUNT; d++) pool.want(s3[d], d == m.freeDiff_ ? 30.f : 10.f);
        return;
    }
    if (nStages() == 0) return;
    const int fs = focusedStage();
    if (kal) {
        const std::string cur = spec(fs, m.row_ == 1 ? m.song_ : 0);
        pool.want(cur, 30);
        if (!g_prev.empty() && g_mix < 1.f) pool.want(g_prev, 30);
        for (int k = 0; k < nSongs(fs); k++) pool.want(spec(fs, k), 0); // the next ones are ready to fold in
        return;
    }
    for (int s = 0; s < nStages(); s++) {
        const float d = std::fabs(s - m.aStage_);
        if (d > 3.2f) continue;
        const bool visible = d < 1.7f;
        for (int k = 0; k < nSongs(s); k++) {
            float fps = 0;
            if (visible && open(s) && s == fs) fps = (m.row_ == 1 && k == m.song_) ? 30.f : 12.f;
            pool.want(spec(s, k), fps);
        }
    }
}

void Worlds::draw() {
    if (kal) {
        if (m.screen_ == Menu::TITLE) kalTitle();
        else if (m.screen_ == Menu::FREE) kalFree();
        else kalCampaign();
    } else {
        if (m.screen_ == Menu::TITLE) winTitle();
        else if (m.screen_ == Menu::FREE) winFree();
        else winCampaign();
    }
}

void Worlds::showImage(const std::string& sp, float x, float y, float w, float h, Col tint, float r) {
    const GLuint tx = pool.tex(sp);
    if (tx) p.image(tx, x, y, w, h, tint, r);
    else { // not ready yet: a soft placeholder in the universe's colour
        unsigned char c[3];
        menuUniverseColor(m.camp_, sp, c);
        p.rrect(x, y, w, h, r, mixc(rgb8(c, tint.a), hex(0x101014, tint.a), 0.7f));
    }
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
        if (v.song > 0) p.text(body, "A RUN FROM HERE DOESN'T CLEAR THE STAGE", rx, y0 + 196 * u, 13 * u, withA(dim, 0.7f), 1, 0.3f);
    } else if (kal && m.screen_ == Menu::CAMPAIGN && m.row_ == 1 && nStages() > 0) { // a petal, closed
        const int fs = focusedStage();
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %d OF %d   /   %d SONGS", fs + 1, nStages(), nSongs(fs));
        p.text(body, b, lx, y0 + 46 * u, 17 * u, dim, -1, 0.3f);
        p.text(head, open(fs) ? "Open the petal to choose a song" : "Locked: clear stage " + std::to_string(fs) + " first", lx, y0 + 108 * u, 38 * u, ink, -1, 0.01f);
        int beat = 0;
        for (int k = 0; k < nSongs(fs); k++) beat += beaten(fs, k);
        std::snprintf(b, sizeof(b), "%d / %d SONGS BEATEN ON %s", beat, nSongs(fs), difficultyName(m.diff_));
        p.text(body, b, lx, y0 + 160 * u, 17 * u, dim, -1, 0.3f);
        if (cleared(fs)) {
            std::snprintf(b, sizeof(b), "CLEARED    BEST %d", m.progress_->stageBest(fs, m.diff_));
            p.text(body, b, rx, y0 + 46 * u, 18 * u, hex(0xffd27a), 1, 0.3f);
        }
        p.text(body, open(fs) ? "ENTER  OPEN      ESC  BACK" : "ESC  BACK", rx, y0 + 160 * u, 16 * u, withA(dim, 0.8f), 1, 0.3f);
        return;
    } else {
        p.text(head, "Choose a difficulty, then a song", lx, y0 + 108 * u, 38 * u, ink, -1, 0.01f);
        p.text(body, "LEFT / RIGHT  DIFFICULTY      DOWN  THE STAGES", lx, y0 + 160 * u, 17 * u, dim, -1, 0.3f);
    }
    p.text(body, kal && m.petal_ ? "ENTER  PLAY      ESC  CLOSE" : "ENTER  PLAY      ESC  BACK", rx, y0 + 160 * u, 16 * u, withA(dim, 0.8f), 1, 0.3f);
}

// ---------------------------------------------------------------------------------------------------------------------
// WINDOWS: a gallery wall

void Worlds::wall(float camX) {
    // the wall, its panel joints sliding with the camera, the floor
    p.rrect(0, 0, m.fw_, m.fh_, 0, hex(0x2a1619));                 // an oxblood gallery wall
    p.rrect(0, 0, m.fw_, Y(260), 0, hex(0x120809, 0.65f), 260 * u);
    p.rrect(0, Y(560), m.fw_, Y(240), 0, hex(0x120809, 0.35f), 200 * u);
    for (int i = -2; i < 14; i++) {
        const float x = X(960 + i * 450.f - std::fmod(camX * 0.999f, 450.f) - 225.f);
        p.line(x, 0, x, Y(780), 1 * u, hex(0xffffff, 0.035f));
    }
    p.rrect(0, Y(780), m.fw_, m.fh_ - Y(780), 0, hex(0x1a0f0c));    // a dark oak floor
    for (int i = 0; i < 4; i++) p.line(0, Y(790 + i * 14.f), m.fw_, Y(790 + i * 14.f), 1 * u, hex(0x000000, 0.25f));
    p.line(0, Y(780), m.fw_, Y(780), 2 * u, hex(0xc8a070, 0.25f));
}

void Worlds::window(int s, float d) {
    const bool o = open(s);
    const float ad = std::min(1.f, std::fabs(d));
    const float sc = 1.f - 0.18f * ad;
    const float W = 800 * u * sc, H = W * 9.f / 16.f;
    const float cx = X(960 + d * 900.f), top = Y(150) + (1.f - sc) * 70 * u;
    const float x0 = cx - W / 2;
    const float f = focusStage(s);
    const bool cl = o && cleared(s);
    const int fs = focusedStage();
    // the light on the wall: warm over a cleared window, a cool spot over an open one, none when locked
    p.setAdditive(true);
    if (o) { // a spotlight from the ceiling, its cone and its pool on the wall
        const Col lc = cl ? hex(0xffc070) : hex(0xd8e0ff);
        p.tri(cx, Y(-40), x0 - 90 * u * sc, top + H + 40 * u, x0 + W + 90 * u * sc, top + H + 40 * u, withA(lc, 0.10f + 0.05f * f), withA(lc, 0.0f), withA(lc, 0.0f));
        p.glow(cx, top - 30 * u, 420 * u * sc, withA(lc, (cl ? 0.22f : 0.12f) + 0.1f * f));
        p.glow(cx, top + H / 2, W * 0.75f, withA(lc, cl ? 0.10f : 0.05f));
        p.glow(cx, Y(785), W * 0.6f, withA(lc, 0.08f)); // its light on the floor
    }
    p.setAdditive(false);
    // the frame
    const float pad = 16 * u * sc;
    p.rrect(x0 - pad + 8 * u, top - pad + 16 * u, W + 2 * pad, H + 2 * pad, 6 * u, hex(0x000000, 0.55f), 30 * u);
    p.rrect(x0 - pad, top - pad, W + 2 * pad, H + 2 * pad, 4 * u, cl ? hex(0x2a2218) : hex(0x0b0b0e));
    p.rrectLine(x0 - pad, top - pad, W + 2 * pad, H + 2 * pad, 4 * u, 1.2f * u, cl ? hex(0xd8b070, 0.6f) : hex(0xffffff, 0.12f));
    // through the window: the focused song's universe (live) on the focused stage, else the stage's universes in turn
    std::string sp;
    if (s == fs && m.row_ == 1) sp = spec(s, m.song_);
    else sp = spec(s, (int)(t / 6.f + s) % std::max(1, nSongs(s)));
    if (o) {
        showImage(sp, x0, top, W, H, hex(0xffffff, 1.f - 0.35f * ad), 2 * u);
        // glass: a faint sheen across the top left
        p.tri(x0, top, x0 + W * 0.55f, top, x0, top + H * 0.7f, hex(0xffffff, 0.07f), hex(0xffffff, 0.0f), hex(0xffffff, 0.0f));
    } else {
        showImage(spec(s, 0), x0, top, W, H, hex(0x8090a8, 0.14f), 2 * u); // a faint silhouette behind the shutters
        const int slats = 9;
        for (int i = 0; i < slats; i++) {
            const float y = top + i * H / slats;
            p.rrect(x0, y + 2 * u, W, H / slats - 4 * u, 2 * u, hex(0x26242c, 0.92f));
            p.line(x0, y + 3 * u, x0 + W, y + 3 * u, 1 * u, hex(0xffffff, 0.06f));
        }
        p.lock(cx, top + H / 2, 40 * u * sc, hex(0xb8b4c8, 0.7f));
    }
    // the panes: one per song
    const int n = nSongs(s);
    const float tw = 138 * u * sc, th = tw * 9.f / 16.f, gap = 16 * u * sc;
    const float rowW = n * tw + (n - 1) * gap, ty = top + H + pad + 26 * u * sc;
    for (int k = 0; k < n; k++) {
        const float g = (s == fs && m.aRow_ > 0.5f) ? sstep(near1(m.aSong_, k)) * f : 0.f;
        const float k2 = 1.f + 0.32f * g;
        const float pw = tw * k2, ph = th * k2;
        const float px = x0 + (W - rowW) / 2 + k * (tw + gap) + tw / 2 - pw / 2, py = ty - 12 * u * g - (ph - th) / 2;
        p.rrect(px - 5 * u, py - 5 * u, pw + 10 * u, ph + 10 * u, 4 * u, hex(0x0b0b0e));
        if (o) showImage(spec(s, k), px, py, pw, ph, hex(0xffffff, 0.75f + 0.25f * std::max(g, 1.f - ad)), 2 * u);
        else p.rrect(px, py, pw, ph, 2 * u, hex(0x2a2830));
        if (g > 0.05f) {
            p.setAdditive(true);
            p.rrectLine(px - 5 * u, py - 5 * u, pw + 10 * u, ph + 10 * u, 5 * u, 6 * u, hex(0xffffff, 0.25f * g), 14 * u);
            p.setAdditive(false);
            p.rrectLine(px - 5 * u, py - 5 * u, pw + 10 * u, ph + 10 * u, 5 * u, 2.4f * u, hex(0xffffff, 0.95f * g));
        }
        if (o && beaten(s, k)) {
            const float bx = px + pw - 4 * u, by = py + 4 * u;
            p.circle(bx, by, 11 * u * sc, hex(0xe8c06a));
            p.check(bx, by, 11 * u * sc, 2.4f * u, hex(0x1a1408));
        }
    }
    // the plaque
    char b[96];
    if (!o) std::snprintf(b, sizeof(b), "STAGE %s  -  LOCKED", roman(s + 1).c_str());
    else if (cl) std::snprintf(b, sizeof(b), "STAGE %s  -  CLEARED  %d", roman(s + 1).c_str(), m.progress_->stageBest(s, m.diff_));
    else std::snprintf(b, sizeof(b), "STAGE %s", roman(s + 1).c_str());
    const float pw = p.width(F::MARCELLUS, b, 16 * u * sc, 0.2f) + 40 * u * sc, ph = 34 * u * sc, py = ty + th + 34 * u * sc;
    p.rrect(cx - pw / 2, py, pw, ph, 3 * u, cl ? hex(0xc8a460) : o ? hex(0x2c2b33) : hex(0x1e1d22));
    p.text(F::MARCELLUS, b, cx, py + ph * 0.68f, 16 * u * sc, cl ? hex(0x241a08) : hex(0xffffff, o ? 0.8f : 0.4f), 0, 0.2f);
}

void Worlds::winCampaign() {
    const float camX = m.aStage_ * 900.f;
    wall(camX);
    for (int s = 0; s < nStages(); s++) {
        const float d = s - m.aStage_;
        if (std::fabs(d) < 1.75f) window(s, d);
    }
    tabs(F::MANROPE, hex(0xffffff), hex(0xffe0b0), 0.3f);
    info(F::MARCELLUS, F::MANROPE, hex(0xffffff), hex(0xc8c4d0, 0.6f), hex(0xffd8a0), 0);
}

void Worlds::winTitle() {
    wall(t * 4.f);
    std::string cur, next;
    float mix;
    tourAt(9.f, cur, next, mix);
    const float x0 = X(300), y0 = Y(70), W = 1320 * u, H = W * 9.f / 16.f * 0.92f;
    p.setAdditive(true);
    p.glow(X(960), y0 - 20 * u, 700 * u, hex(0xffd0a0, 0.12f));
    p.setAdditive(false);
    p.rrect(x0 - 18 * u + 10 * u, y0 - 18 * u + 20 * u, W + 36 * u, H + 36 * u, 6 * u, hex(0x000000, 0.6f), 40 * u);
    p.rrect(x0 - 18 * u, y0 - 18 * u, W + 36 * u, H + 36 * u, 4 * u, hex(0x0b0b0e));
    showImage(cur, x0, y0, W, H, hex(0xffffff), 2 * u);
    if (mix > 0.f) showImage(next, x0, y0, W, H, hex(0xffffff, mix), 2 * u);
    // the title, on a shade at the foot of the window
    p.rrect(x0, y0 + H * 0.58f, W, H * 0.42f, 2 * u, hex(0x000000, 0.45f), 120 * u);
    p.text(F::MARCELLUS, upper(GAME_TITLE), X(960), y0 + H * 0.8f, 104 * u, hex(0xffffff), 0, 0.14f);
    {   // which world is showing, on a small plaque
        int vi = 0;
        const int idx = universes::parse(mix > 0.5f ? next : cur, &vi);
        const std::string name = idx >= 0 ? universes::label(idx, std::max(0, vi)) : cur;
        std::string ascii;
        for (unsigned char c : name) ascii += c < 0x80 ? (char)c : ' ';
        const float pw = p.width(F::MANROPE, upper(ascii), 14 * u, 0.3f) + 36 * u;
        p.rrect(x0 + W - pw - 20 * u, y0 + H + 30 * u, pw, 30 * u, 3 * u, hex(0x2c2b33));
        p.text(F::MANROPE, upper(ascii), x0 + W - pw / 2 - 20 * u, y0 + H + 50 * u, 14 * u, hex(0xffffff, 0.8f), 0, 0.3f);
    }
    menuItems(F::MANROPE, 845, 58, 32, hex(0xffffff), hex(0xffe0b0), 0.3f);
    if (m.progress_ && m.camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d STAGES CLEARED", m.progress_->clearedCount(), m.camp_->size());
        p.text(F::MANROPE, b, X(80), Y(1040), 16 * u, hex(0xffffff, 0.45f), -1, 0.3f);
    }
    p.text(F::MANROPE, "ARROWS  MOVE      ENTER  SELECT      ESC  QUIT", X(1840), Y(1040), 15 * u, hex(0xffffff, 0.4f), 1, 0.3f);
}

void Worlds::winFree() {
    wall(t * 3.f);
    p.text(F::MARCELLUS, "FREE PLAY", X(960), Y(170), 84 * u, hex(0xffffff), 0, 0.16f);
    p.text(F::MANROPE, "THE PLAYLIST IN A RANDOM ORDER, LEVELS RISING WITH THE LINES", X(960), Y(225), 18 * u, hex(0xffffff, 0.55f), 0, 0.3f);
    const std::string s3[3] = {spec(0, 0), spec(5, 2), spec(11, 4)};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(m.aFree_, d));
        const float W = (470 + 50 * f) * u, H = W * 9.f / 16.f, cx = X(960 + (d - 1) * 560.f), top = Y(330) - 20 * u * f;
        p.setAdditive(true);
        p.glow(cx, top - 20 * u, 360 * u, hex(0xffd0a0, 0.06f + 0.16f * f));
        p.setAdditive(false);
        p.rrect(cx - W / 2 - 14 * u + 8 * u, top - 14 * u + 16 * u, W + 28 * u, H + 28 * u, 5 * u, hex(0x000000, 0.55f), 30 * u);
        p.rrect(cx - W / 2 - 14 * u, top - 14 * u, W + 28 * u, H + 28 * u, 4 * u, hex(0x0b0b0e));
        showImage(s3[d], cx - W / 2, top, W, H, hex(0xffffff, 0.55f + 0.45f * f), 2 * u);
        if (f > 0.05f) p.rrectLine(cx - W / 2 - 14 * u, top - 14 * u, W + 28 * u, H + 28 * u, 4 * u, 2 * u, hex(0xffe0b0, 0.8f * f));
        char b[48];
        std::snprintf(b, sizeof(b), "%s   x%.1f", difficultyName(d), freePlayMultiplier(d));
        const float pw = p.width(F::MARCELLUS, b, 18 * u, 0.2f) + 44 * u;
        p.rrect(cx - pw / 2, top + H + 40 * u, pw, 40 * u, 3 * u, f > 0.5f ? hex(0xc8a460) : hex(0x2c2b33));
        p.text(F::MARCELLUS, b, cx, top + H + 67 * u, 18 * u, f > 0.5f ? hex(0x241a08) : hex(0xffffff, 0.75f), 0, 0.2f);
    }
    char b[96];
    std::snprintf(b, sizeof(b), "GRAVITY AT %d%% OF ITS FULL SPEED, ON EVERY LEVEL", (int)std::lround(freePlayMultiplier(m.freeDiff_) * 100));
    p.text(F::MANROPE, b, X(960), Y(900), 19 * u, hex(0xffffff, 0.8f), 0, 0.3f);
    p.text(F::MANROPE, "ENTER  START      ESC  BACK", X(960), Y(1020), 15 * u, hex(0xffffff, 0.4f), 0, 0.3f);
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
        if (!o) prompt = "CLEAR STAGE " + std::to_string(fs) + " TO OPEN IT";
        else if (!m.petal_) prompt = "ENTER  OPEN THE PETAL";
        else prompt = "ENTER  PLAY      ESC  CLOSE";
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
    p.circle(cx, cy, 200 * u, hex(0x08080c));   // a plain dark glass disc (no preview)
    p.ring(cx, cy, 191 * u, 4 * u, hex(0xe8c06a, 0.85f));
    p.rrect(cx - 520 * u, Y(600), 1040 * u, 120 * u, 24 * u, hex(0x0c0c14, 0.7f));
    p.rrectLine(cx - 520 * u, Y(600), 1040 * u, 120 * u, 24 * u, 1.2f * u, hex(0xffffff, 0.25f));
    p.text(F::JOSEFIN_LIGHT, upper(GAME_TITLE), cx, Y(683), 74 * u, hex(0xffffff), 0, 0.22f);
    p.rrect(cx - 260 * u, Y(762), 520 * u, 210 * u, 24 * u, hex(0x0c0c14, 0.6f));
    menuItems(F::JOSEFIN, 822, 62, 30, hex(0xffffff), hex(0xffffff), 0.35f);
    p.text(F::JOSEFIN, "ARROWS  MOVE      ENTER  SELECT      ESC  QUIT", cx, Y(1035), 15 * u, hex(0xffffff, 0.5f), 0, 0.3f);
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
    char b[96];
    std::snprintf(b, sizeof(b), "GRAVITY AT %d%% OF ITS FULL SPEED, ON EVERY LEVEL", (int)std::lround(freePlayMultiplier(m.freeDiff_) * 100));
    p.text(F::JOSEFIN, b, X(960), Y(965), 17 * u, hex(0xffffff, 0.85f), 0, 0.3f);
    const char* hint = m.editing_ ? "ENTER  READ IT      CTRL+V  PASTE      ESC  CANCEL"
                     : (rows && m.freeRow_ == 1) ? "LEFT / RIGHT  CHOOSE      ENTER  START      DEL  REMOVE      UP  DIFFICULTY      ESC  BACK"
                     : "LEFT / RIGHT  DIFFICULTY      DOWN  PLAYLIST      ENTER  START      ESC  BACK";
    p.text(F::JOSEFIN, hint, X(960), Y(1020), 15 * u, hex(0xffffff, 0.55f), 0, 0.3f);
}

// The menus' smooth styles: COSMIC, SWISS, WATERCOLOUR and GLASS (8-BIT ARCADE is the pixel canvas of menu.cpp).
// Every style draws the same three screens from the same state: the title (CAMPAIGN, FREE PLAY, QUIT), the
// campaign's stages (5 songs each, the difficulty tabs, the song under the cursor) and free play's difficulty.
// Focus moves are eased (aTitle_, aStage_, ... follow the cursor), backgrounds drift slowly.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "library.hpp"
#include "menu.hpp"

namespace {

using F = Painter::Font;

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
// "Stage" -> "Stage" in title case for the styles that don't shout.
std::string title(const char* s) {
    std::string r = s;
    for (size_t i = 1; i < r.size(); i++) r[i] = (char)std::tolower((unsigned char)r[i]);
    return r;
}
float near1(float a, int i) { return std::max(0.f, 1.f - std::fabs(a - (float)i)); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
std::string two(int n) { char b[16]; std::snprintf(b, sizeof(b), "%02d", n); return b; }
std::string fmtVel(float v) { char b[16]; std::snprintf(b, sizeof(b), "%.1f", v); return b; }

std::vector<std::string> wrap(Painter& p, F f, const std::string& s, float size, float maxW, int maxLines) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        const std::string w = s.substr(i, j - i);
        const std::string t = cur.empty() ? w : cur + " " + w;
        if (!cur.empty() && p.width(f, t, size) > maxW) { lines.push_back(cur); cur = w; }
        else cur = t;
        i = j + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    if ((int)lines.size() > maxLines) {
        lines.resize(maxLines);
        std::string& l = lines.back();
        while (!l.empty() && p.width(f, l + "...", size) > maxW) l.pop_back();
        l += "...";
    }
    return lines;
}

// The stages' own colours (planets, map stops): a calm 12-step wheel.
const unsigned char STAGE_COL[12][3] = {{110, 200, 255}, {150, 130, 255}, {255, 120, 200}, {255, 170, 90},
                                        {120, 230, 170}, {90, 170, 255},  {220, 120, 255}, {255, 210, 110},
                                        {110, 220, 230}, {255, 130, 120}, {170, 230, 110}, {200, 200, 255}};

} // namespace

void Menu::stageColor(int stage, unsigned char out[3]) const {
    for (int i = 0; i < 3; i++) out[i] = STAGE_COL[((stage % 12) + 12) % 12][i];
}

Menu::SongView Menu::songView() const {
    SongView v;
    const CampaignSong* h = hovered();
    if (!h) return v;
    v.has = true;
    v.stage = stage_;
    v.song = song_;
    v.songs = (int)camp_->stages()[stage_].songs.size();
    v.locked = progress_ && !progress_->unlocked(stage_);
    v.beaten = progress_ && progress_->beaten(stage_, song_, diff_);
    v.best = progress_ ? progress_->best(stage_, song_, diff_) : 0;
    v.title = asciiFold(h->title);
    v.universe = h->universe.empty() ? "A UNIVERSE PICKED BY THE SONG" : h->universe;
    v.velocity = h->level[diff_];
    v.preview = previewStatus();
    menuUniverseColor(camp_, h->universe, v.col);
    return v;
}

void Menu::renderSmooth(int w, int h) {
    fw_ = (float)w;
    fh_ = (float)h;
    u_ = std::min(w / 1920.f, h / 1080.f);
    sx_ = (w - 1920.f * u_) * 0.5f;
    sy_ = (h - 1080.f * u_) * 0.5f;
    paint_.begin(w, h);
    if (style_ >= GALAXY) {
        renderCosmos();
        smoothMessage();
        paint_.end();
        return;
    }
    static const int BG[STYLE_COUNT] = {0, Painter::BG_COSMIC, Painter::BG_SWISS, Painter::BG_PAPER, Painter::BG_GLASS};
    paint_.background(BG[style_], time_, level_);
    switch (style_) {
    case COSMIC: screen_ == TITLE ? cosmicTitle() : screen_ == FREE ? cosmicFree() : cosmicCampaign(); break;
    case SWISS: screen_ == TITLE ? swissTitle() : screen_ == FREE ? swissFree() : swissCampaign(); break;
    case WATERCOLOUR: screen_ == TITLE ? paperTitle() : screen_ == FREE ? paperFree() : paperCampaign(); break;
    default: screen_ == TITLE ? glassTitle() : screen_ == FREE ? glassFree() : glassCampaign(); break;
    }
    smoothMessage();
    paint_.end();
}

// ---- the message over a screen (GAME OVER, STAGE CLEAR), in each style's colours

void Menu::smoothMessage() {
    if (msgT_ <= 0 || msg_.empty()) return;
    Painter& p = paint_;
    const float a = sstep(std::min(msgT_, 5.f - msgT_) * 3.f);
    const float cx = X(960), cy = Y(520);
    F big = F::JOSEFIN_LIGHT, small = F::JOSEFIN;
    Col ink = hex(0xffffff), sub = hex(0xc8d0ff), card = hex(0x0a0c22, 0.9f), edge = msgBad_ ? hex(0xff6a8a) : hex(0x8fe6ff);
    std::string m = msg_;
    if (style_ == SWISS) { big = F::INTER_BOLD; small = F::INTER; ink = hex(0xffffff); sub = hex(0xffffff, 0.85f); card = msgBad_ ? hex(0xe4321b) : hex(0x141414); edge = withA(card, 0); }
    if (style_ == WATERCOLOUR) { big = F::KALAM; small = F::YRSA; ink = hex(0x2b3a55); sub = hex(0x5a4632); card = hex(0xfbf6ea, 0.95f); edge = msgBad_ ? hex(0xb8322a, 0.7f) : hex(0x2b3a55, 0.5f); }
    if (style_ == GLASS || style_ == SYNTHWAVE) { big = F::MANROPE; small = F::MANROPE_LIGHT; card = hex(0x1a1640, 0.8f); edge = msgBad_ ? hex(0xff4fd8) : hex(0x4de8ff); }
    if (style_ == ATLAS) { big = F::CORMORANT_ITALIC; small = F::MARCELLUS; ink = hex(0xf0d79a); card = hex(0x0b1730, 0.92f); edge = hex(0xd8b45a); }
    if (style_ == ORRERY) { big = F::JURA_LIGHT; small = F::JURA; edge = msgBad_ ? hex(0xff8a6a) : hex(0xe8b860); }
    if (style_ == PASTEL) { big = F::COMFORTAA; small = F::QUICKSAND; ink = hex(0x5b4a7a); sub = hex(0x7a6a96); card = hex(0xffffff, 0.94f); edge = msgBad_ ? hex(0xff8fb0) : hex(0xb9a6ff); }
    if (style_ == WATERCOLOUR || style_ == SWISS) m = title(msg_.c_str());
    const float tw = std::max(p.width(big, m, 110 * u_, 0.05f), p.width(small, sub.a > 0 ? msgSub_ : "", 26 * u_, 0.1f));
    const float bw = tw + 140 * u_, bh = 250 * u_;
    p.rrect(cx - bw / 2, cy - bh / 2 + 14 * u_, bw, bh, 26 * u_, hex(0x000000, 0.35f * a), 40 * u_);
    p.rrect(cx - bw / 2, cy - bh / 2, bw, bh, style_ == SWISS ? 0 : 24 * u_, withA(card, card.a * a));
    p.rrectLine(cx - bw / 2, cy - bh / 2, bw, bh, style_ == SWISS ? 0 : 24 * u_, 2 * u_, withA(edge, edge.a * a));
    p.text(big, m, cx, cy + 20 * u_, 110 * u_, withA(ink, a), 0, 0.05f);
    p.text(small, msgSub_, cx, cy + 85 * u_, 26 * u_, withA(sub, sub.a * a), 0, 0.1f);
}

// =====================================================================================================================
// COSMIC: a slow nebula and its stars; the stages are planets along an orbit path, each with its five songs as moons.

void Menu::cosmicTitle() {
    Painter& p = paint_;
    const float cx = X(960), t = time_;
    p.setAdditive(true);
    p.glow(cx, Y(330), 520 * u_, hex(0x6a3cff, 0.22f));
    p.glow(cx + 260 * u_, Y(300), 300 * u_, hex(0x1fb6ff, 0.12f));
    // an orbit around the title, a small world travelling on it
    const float rx = 640 * u_, ry = 150 * u_, ocy = Y(340);
    for (int i = 0; i < 160; i++) {
        const float a0 = i / 160.f * 6.2831853f, a1 = (i + 1) / 160.f * 6.2831853f;
        const float back = 0.5f + 0.5f * std::sin(a0);
        p.line(cx + rx * std::cos(a0), ocy + ry * std::sin(a0), cx + rx * std::cos(a1), ocy + ry * std::sin(a1), 1.4f * u_,
               hex(0xb8c8ff, 0.06f + 0.14f * back));
    }
    const float pa = t * 0.12f + 0.6f;
    const float px = cx + rx * std::cos(pa), py = ocy + ry * std::sin(pa);
    p.glow(px, py, 60 * u_, hex(0x4de8ff, 0.5f));
    p.circle(px, py, 9 * u_, hex(0xdff8ff));
    p.setAdditive(false);
    p.text(F::JOSEFIN_LIGHT, "TETRIS", cx, Y(400), 170 * u_, hex(0xffffff, 0.96f), 0, 0.32f);
    p.text(F::JOSEFIN, "UNIVERSE", cx, Y(470), 32 * u_, hex(0x9fe8ff, 0.9f), 0, 0.9f);

    const char* items[3] = {"CAMPAIGN", "FREE PLAY", "QUIT"};
    float wsel = 0;
    for (int i = 0; i < 3; i++) {
        const float f = near1(aTitle_, i);
        const float y = Y(660 + i * 86);
        p.text(F::JOSEFIN_LIGHT, items[i], cx, y, 44 * u_, hex(0xffffff, 0.42f + 0.58f * f), 0, 0.3f);
        wsel += f * p.width(F::JOSEFIN_LIGHT, items[i], 44 * u_, 0.3f);
    }
    const float ys = Y(660 + aTitle_ * 86) + 20 * u_;
    p.setAdditive(true);
    p.line(cx - wsel / 2, ys, cx + wsel / 2, ys, 1.6f * u_, hex(0x7fe9ff, 0.85f));
    p.glow(cx - wsel / 2 - 34 * u_, ys - 16 * u_, 30 * u_, hex(0x7fe9ff, 0.6f));
    p.glow(cx + wsel / 2 + 34 * u_, ys - 16 * u_, 30 * u_, hex(0x7fe9ff, 0.6f));
    p.setAdditive(false);
    p.circle(cx - wsel / 2 - 34 * u_, ys - 16 * u_, 4 * u_, hex(0xffffff));
    p.circle(cx + wsel / 2 + 34 * u_, ys - 16 * u_, 4 * u_, hex(0xffffff));
    if (progress_ && camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d STAGES CLEARED", progress_->clearedCount(), camp_->size());
        p.text(F::JOSEFIN, b, cx, Y(960), 19 * u_, hex(0xc8d0ff, 0.55f), 0, 0.35f);
    }
    p.text(F::JOSEFIN, "ARROWS  MOVE      ENTER  SELECT      ESC  QUIT", cx, Y(1020), 17 * u_, hex(0xc8d0ff, 0.38f), 0, 0.3f);
}

void Menu::cosmicFree() {
    Painter& p = paint_;
    const float cx = X(960), t = time_;
    p.text(F::JOSEFIN_LIGHT, "FREE PLAY", cx, Y(250), 100 * u_, hex(0xffffff, 0.95f), 0, 0.3f);
    p.text(F::JOSEFIN, "THE PLAYLIST IN A RANDOM ORDER  /  LEVELS RISE WITH THE LINES", cx, Y(320), 20 * u_, hex(0xc8d0ff, 0.6f), 0, 0.3f);
    static const uint32_t C[3] = {0x6ff0c8, 0x9c86ff, 0xff6fc8};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(aFree_, d));
        const float x = cx + (d - 1) * 440 * u_, y = Y(590);
        const float R = (120 + 14 * f) * u_, r = (30 + 10 * f) * u_;
        const Col c = hex(C[d]);
        p.ring(x, y, R, 1.3f * u_, hex(0xc8d0ff, 0.12f + 0.25f * f));
        p.setAdditive(true);
        p.glow(x, y, (90 + 70 * f) * u_, withA(c, 0.18f + 0.35f * f));
        p.setAdditive(false);
        p.circle(x, y, r, mixc(hex(0x30344a), c, 0.45f + 0.55f * f));
        p.circle(x - r * 0.3f, y - r * 0.3f, r * 0.45f, hex(0xffffff, 0.12f + 0.1f * f), r * 0.6f);
        // a moon whose speed is the difficulty's
        const float a = t * 0.9f * freePlayMultiplier(d) + d;
        const float mx = x + R * std::cos(a), my = y + R * std::sin(a);
        p.glow(mx, my, 26 * u_, withA(c, 0.5f * (0.4f + 0.6f * f)));
        p.circle(mx, my, 6 * u_, hex(0xffffff, 0.5f + 0.5f * f));
        p.text(F::JOSEFIN, difficultyName(d), x, Y(790), 30 * u_, hex(0xffffff, 0.4f + 0.6f * f), 0, 0.4f);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", freePlayMultiplier(d));
        p.text(F::JOSEFIN_LIGHT, b, x, Y(835), 26 * u_, hex(0xc8d0ff, 0.35f + 0.4f * f), 0, 0.1f);
    }
    char b[96];
    std::snprintf(b, sizeof(b), "GRAVITY AT %d%% OF ITS FULL SPEED, ON EVERY LEVEL", (int)std::lround(freePlayMultiplier(freeDiff_) * 100));
    p.text(F::JOSEFIN, b, cx, Y(930), 20 * u_, hex(0xffffff, 0.75f), 0, 0.3f);
    p.text(F::JOSEFIN, "ENTER  START      ESC  BACK", cx, Y(1020), 17 * u_, hex(0xc8d0ff, 0.38f), 0, 0.3f);
}

void Menu::cosmicCampaign() {
    Painter& p = paint_;
    const float cx = X(960), t = time_;
    p.text(F::JOSEFIN, "CAMPAIGN", X(90), Y(100), 24 * u_, hex(0xffffff, 0.75f), -1, 0.6f);
    // the difficulty tabs, a glowing line sliding under the chosen one
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = near1(aDiff_, d);
        p.text(F::JOSEFIN, difficultyName(d), cx + (d - 1) * 230 * u_, Y(100), 24 * u_, hex(0xffffff, 0.4f + 0.6f * f), 0, 0.4f);
    }
    {
        const float x = cx + (aDiff_ - 1) * 230 * u_, y = Y(118);
        const float on = 1.f - std::min(1.f, aRow_);
        p.setAdditive(true);
        p.line(x - 60 * u_, y, x + 60 * u_, y, (1.5f + on) * u_, hex(0x7fe9ff, 0.45f + 0.5f * on));
        if (on > 0.05f) p.glow(x, y, 80 * u_, hex(0x7fe9ff, 0.25f * on));
        p.setAdditive(false);
    }
    if (!camp_ || camp_->size() == 0) return;
    const int n = camp_->size();
    auto planet = [&](int s, float& x, float& y) {
        x = X(170 + s * (1580.f / std::max(1, n - 1)));
        y = Y(470 + 105 * std::sin(s * 0.85f + 0.6f));
    };
    auto focusOf = [&](int s) { return aRow_ > 0.5f ? sstep(near1(aStage_, s)) * std::min(1.f, aRow_) : 0.f; };
    auto moon = [&](int s, int k, float& x, float& y) {
        float px, py;
        planet(s, px, py);
        const float f = focusOf(s), R = (50 + 14 * f) * u_;
        const float a = -1.5708f + k * 6.2831853f / 5.f + t * 0.05f;
        x = px + R * std::cos(a);
        y = py + R * std::sin(a);
    };
    // the path
    for (int s = 0; s + 1 < n; s++) {
        float x0, y0, x1, y1;
        planet(s, x0, y0);
        planet(s + 1, x1, y1);
        const bool open = !progress_ || progress_->unlocked(s + 1);
        for (int i = 0; i < 24; i++) {
            if (!open && i % 2) continue;
            const float a = i / 24.f, b = (i + 1) / 24.f;
            p.line(lerpf(x0, x1, a), lerpf(y0, y1, a), lerpf(x0, x1, b), lerpf(y0, y1, b), 1.3f * u_, hex(0xb8c8ff, open ? 0.22f : 0.08f));
        }
    }
    for (int s = 0; s < n; s++) {
        float x, y;
        planet(s, x, y);
        const bool open = !progress_ || progress_->unlocked(s);
        const float f = focusOf(s);
        unsigned char sc[3];
        stageColor(s, sc);
        const Col c = open ? rgb8(sc) : hex(0x50566e);
        const float r = (19 + 7 * f) * u_;
        if (open) {
            p.setAdditive(true);
            p.glow(x, y, (60 + 40 * f) * u_, withA(c, 0.22f + 0.25f * f));
            p.setAdditive(false);
        }
        p.ring(x, y, (50 + 14 * f) * u_, 1.f * u_, hex(0xc8d0ff, open ? 0.1f + 0.12f * f : 0.05f));
        p.circle(x, y, r, open ? mixc(c, hex(0x101530), 0.25f) : hex(0x262a3e));
        p.circle(x - r * 0.32f, y - r * 0.32f, r * 0.5f, hex(0xffffff, open ? 0.25f : 0.06f), r * 0.7f);
        if (!open) p.lock(x, y + 2 * u_, 16 * u_, hex(0x9aa0c0, 0.8f));
        if (progress_ && progress_->cleared(s, diff_)) p.ring(x, y, r + 7 * u_, 1.6f * u_, hex(0xffd76a, 0.9f));
        const auto& songs = camp_->stages()[s].songs;
        for (int k = 0; k < (int)songs.size(); k++) {
            float mx, my;
            moon(s, k, mx, my);
            unsigned char mc[3];
            menuUniverseColor(camp_, songs[k].universe, mc);
            const bool beat = progress_ && progress_->beaten(s, k, diff_);
            const float mr = (6 + 2.5f * f) * u_;
            if (!open) { p.circle(mx, my, mr * 0.8f, hex(0x3a3f55)); continue; }
            if (beat) {
                p.setAdditive(true);
                p.glow(mx, my, 22 * u_, rgb8(mc, 0.55f));
                p.setAdditive(false);
                p.circle(mx, my, mr, rgb8(mc));
            } else {
                p.circle(mx, my, mr, rgb8(mc, 0.22f));
                p.ring(mx, my, mr, 1.5f * u_, rgb8(mc, 0.9f));
            }
        }
        p.text(F::JOSEFIN, two(s + 1), x, y + (92 + 14 * f) * u_, (17 + 4 * f) * u_, hex(0xffffff, open ? 0.45f + 0.5f * f : 0.25f), 0, 0.25f);
        if (progress_ && progress_->cleared(s, diff_))
            p.text(F::JOSEFIN, std::to_string(progress_->stageBest(s, diff_)), x, y + (116 + 14 * f) * u_, 15 * u_, hex(0xffd76a, 0.8f), 0, 0.15f);
    }
    // the focused moon: a ring gliding from moon to moon
    if (aRow_ > 0.5f) {
        const int s0 = std::clamp((int)std::floor(aStage_), 0, n - 1), s1 = std::min(n - 1, s0 + 1);
        const int k0 = std::clamp((int)std::floor(aSong_), 0, 4), k1 = std::min(4, k0 + 1);
        const float fs = aStage_ - s0, fk = aSong_ - k0;
        float a0x, a0y, a1x, a1y, b0x, b0y, b1x, b1y;
        moon(s0, k0, a0x, a0y); moon(s0, k1, a1x, a1y); moon(s1, k0, b0x, b0y); moon(s1, k1, b1x, b1y);
        const float fx = lerpf(lerpf(a0x, a1x, fk), lerpf(b0x, b1x, fk), fs), fy = lerpf(lerpf(a0y, a1y, fk), lerpf(b0y, b1y, fk), fs);
        const float pulse = 0.75f + 0.25f * std::sin(t * 2.4f);
        p.setAdditive(true);
        p.glow(fx, fy, 40 * u_, hex(0xffffff, 0.25f * pulse));
        p.setAdditive(false);
        p.ring(fx, fy, 15 * u_, 2 * u_, hex(0xffffff, 0.9f * pulse * std::min(1.f, aRow_)));
    }
    // the song under the cursor
    p.line(X(90), Y(840), X(1830), Y(840), 1 * u_, hex(0xc8d0ff, 0.15f));
    const SongView v = songView();
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %d   /   SONG %d OF %d", v.stage + 1, v.song + 1, v.songs);
        p.text(F::JOSEFIN, b, X(90), Y(892), 18 * u_, hex(0xc8d0ff, 0.6f), -1, 0.35f);
        std::string ti = v.title;
        while (p.width(F::JOSEFIN_LIGHT, ti, 46 * u_) > 1150 * u_ && ti.size() > 4) ti.pop_back();
        p.text(F::JOSEFIN_LIGHT, ti, X(90), Y(955), 46 * u_, hex(0xffffff, 0.95f), -1, 0.02f);
        const float uw = p.text(F::JOSEFIN, v.universe, X(90), Y(1005), 20 * u_, rgb8(v.col), -1, 0.3f);
        p.text(F::JOSEFIN, "VELOCITY " + fmtVel(v.velocity), X(90) + uw + 40 * u_, Y(1005), 20 * u_, hex(0xffffff, 0.6f), -1, 0.3f);
        if (v.beaten) std::snprintf(b, sizeof(b), "BEATEN    BEST %d", v.best);
        else std::snprintf(b, sizeof(b), "NOT BEATEN ON %s", difficultyName(diff_));
        p.text(F::JOSEFIN, b, X(1830), Y(892), 18 * u_, v.beaten ? hex(0xffd76a) : hex(0xc8d0ff, 0.55f), 1, 0.3f);
        p.text(F::JOSEFIN, v.preview, X(1830), Y(950), 18 * u_, hex(0x7fe9ff, 0.85f), 1, 0.3f);
        if (v.song > 0) p.text(F::JOSEFIN, "A RUN FROM HERE DOESN'T CLEAR THE STAGE", X(1830), Y(1050), 14 * u_, hex(0xc8d0ff, 0.4f), 1, 0.3f);
    } else {
        p.text(F::JOSEFIN, "LEFT / RIGHT  DIFFICULTY      DOWN  THE STAGES", cx, Y(940), 18 * u_, hex(0xc8d0ff, 0.6f), 0, 0.3f);
    }
    p.text(F::JOSEFIN, "ENTER  PLAY      ESC  BACK", X(1830), Y(1005), 16 * u_, hex(0xc8d0ff, 0.4f), 1, 0.3f);
}

// =====================================================================================================================
// SWISS: a typographic poster: a strict grid, big bold grotesque, flat red and black on warm paper.

namespace {
const uint32_t INK = 0x141414, RED = 0xe4321b, GREY = 0x8c8a84, LIGHT = 0xcfccc4, PAPER = 0xeeebe3;
}

void Menu::swissTitle() {
    Painter& p = paint_;
    const float t = time_;
    // the poster: a red disc, a black bar and a T piece on the grid
    p.circle(X(1440), Y(470 + 8 * std::sin(t * 0.25f)), 330 * u_, hex(RED));
    p.rrect(X(1060), Y(800), 780 * u_, 24 * u_, 0, hex(INK));
    const float sq = 74 * u_, g = 4 * u_, tx = X(1230 + 10 * std::sin(t * 0.18f)), ty = Y(250);
    const int T[4][2] = {{0, 0}, {1, 0}, {2, 0}, {1, 1}};
    for (auto& c : T) p.rrect(tx + c[0] * (sq + g), ty + c[1] * (sq + g), sq, sq, 0, hex(INK));
    p.text(F::INTER_BOLD, "Tetris", X(104), Y(330), 200 * u_, hex(INK), -1, -0.03f);
    p.text(F::INTER_BOLD, "Universe", X(104), Y(540), 200 * u_, hex(RED), -1, -0.03f);
    p.text(F::INTER, "A music-driven Tetris. Sixty universes, twelve stages, five songs each.", X(112), Y(620), 24 * u_, hex(INK, 0.8f));
    const char* items[3] = {"Campaign", "Free play", "Quit"};
    float wsel = 0;
    for (int i = 0; i < 3; i++) wsel += near1(aTitle_, i) * p.width(F::INTER_BOLD, items[i], 56 * u_, -0.01f);
    const float by = Y(760 + aTitle_ * 82);
    p.rrect(X(96), by - 58 * u_, wsel + 140 * u_, 76 * u_, 0, hex(INK));
    for (int i = 0; i < 3; i++) {
        const float f = near1(aTitle_, i);
        const float y = Y(760 + i * 82);
        p.text(F::INTER_BOLD, two(i + 1), X(112), y - 4 * u_, 22 * u_, mixc(hex(GREY), hex(RED), f));
        p.text(F::INTER_BOLD, items[i], X(190), y, 56 * u_, mixc(hex(INK), hex(PAPER), f), -1, -0.01f);
    }
    p.line(X(104), Y(990), X(1816), Y(990), 2 * u_, hex(INK));
    p.text(F::INTER, "Arrows select    Enter confirm    Esc quit", X(104), Y(1030), 20 * u_, hex(INK, 0.7f));
    if (progress_ && camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d stages cleared", progress_->clearedCount(), camp_->size());
        p.text(F::INTER_BOLD, b, X(1816), Y(1030), 20 * u_, hex(INK), 1);
    }
}

void Menu::swissFree() {
    Painter& p = paint_;
    p.text(F::INTER_BOLD, "Free play", X(104), Y(240), 170 * u_, hex(INK), -1, -0.03f);
    p.text(F::INTER, "The playlist in a random order, a universe picked for each song.", X(112), Y(320), 26 * u_, hex(INK, 0.85f));
    p.text(F::INTER, "Levels rise with the lines you clear.", X(112), Y(360), 26 * u_, hex(INK, 0.85f));
    float wsel = 0;
    for (int d = 0; d < DIFF_COUNT; d++) wsel += near1(aFree_, d) * p.width(F::INTER_BOLD, title(difficultyName(d)), 90 * u_, -0.02f);
    const float by = Y(570 + aFree_ * 120);
    p.rrect(X(96), by - 86 * u_, wsel + 150 * u_, 112 * u_, 0, hex(INK));
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = near1(aFree_, d), y = Y(570 + d * 120);
        p.text(F::INTER_BOLD, two(d + 1), X(112), y - 8 * u_, 24 * u_, mixc(hex(GREY), hex(RED), f));
        p.text(F::INTER_BOLD, title(difficultyName(d)), X(190), y, 90 * u_, mixc(hex(INK), hex(PAPER), f), -1, -0.02f);
    }
    // the gravity of each difficulty as bars
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = near1(aFree_, d), m = freePlayMultiplier(d);
        const float h = 520 * m * u_, x = X(1180 + d * 200);
        p.rrect(x, Y(900) - h, 150 * u_, h, 0, mixc(hex(LIGHT), hex(RED), f));
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", m);
        p.text(F::INTER_BOLD, b, x, Y(940), 30 * u_, hex(INK));
    }
    p.line(X(1180), Y(900), X(1730), Y(900), 2 * u_, hex(INK));
    char b[96];
    std::snprintf(b, sizeof(b), "Gravity at %d%% of its full speed, on every level.", (int)std::lround(freePlayMultiplier(freeDiff_) * 100));
    p.text(F::INTER_BOLD, b, X(112), Y(940), 26 * u_, hex(INK));
    p.line(X(104), Y(990), X(1816), Y(990), 2 * u_, hex(INK));
    p.text(F::INTER, "Enter start    Esc back", X(104), Y(1030), 20 * u_, hex(INK, 0.7f));
}

void Menu::swissCampaign() {
    Painter& p = paint_;
    p.text(F::INTER_BOLD, "Campaign", X(104), Y(160), 110 * u_, hex(INK), -1, -0.03f);
    // difficulty tabs
    float tx[DIFF_COUNT], tw[DIFF_COUNT], x = X(1110);
    for (int d = 0; d < DIFF_COUNT; d++) {
        tw[d] = p.width(F::INTER_BOLD, title(difficultyName(d)), 34 * u_);
        tx[d] = x;
        p.text(F::INTER_BOLD, title(difficultyName(d)), x, Y(150), 34 * u_, mixc(hex(GREY), hex(INK), near1(aDiff_, d)));
        x += tw[d] + 48 * u_;
    }
    {
        const int d0 = std::clamp((int)std::floor(aDiff_), 0, DIFF_COUNT - 1), d1 = std::min(DIFF_COUNT - 1, d0 + 1);
        const float f = aDiff_ - d0;
        const float bx = lerpf(tx[d0], tx[d1], f), bw = lerpf(tw[d0], tw[d1], f);
        const float on = 1.f - std::min(1.f, aRow_);
        p.rrect(bx, Y(166), bw, (6 + 4 * on) * u_, 0, hex(RED));
    }
    p.rrect(X(104), Y(200), 1712 * u_, 3 * u_, 0, hex(INK));
    if (!camp_ || camp_->size() == 0) return;
    const int n = camp_->size();
    const float rowH = 56, top = 262;
    // the focused row: a pale band gliding
    if (aRow_ > 0.02f)
        p.rrect(X(96), Y(top + aStage_ * rowH) - 34 * u_, 940 * u_, 52 * u_, 0, hex(0xdedad0, std::min(1.f, aRow_)));
    for (int s = 0; s < n; s++) {
        const float y = Y(top + s * rowH);
        const bool open = !progress_ || progress_->unlocked(s);
        p.text(F::INTER_BOLD, two(s + 1), X(112), y + 4 * u_, 30 * u_, hex(open ? INK : LIGHT), -1, -0.02f);
        const auto& songs = camp_->stages()[s].songs;
        for (int k = 0; k < (int)songs.size(); k++) {
            const float qx = X(220 + k * 54), qy = y - 24 * u_, q = 38 * u_;
            unsigned char mc[3];
            menuUniverseColor(camp_, songs[k].universe, mc);
            const bool beat = progress_ && progress_->beaten(s, k, diff_);
            if (!open) p.rrectLine(qx, qy, q, q, 0, 1.5f * u_, hex(LIGHT));
            else if (beat) {
                p.rrect(qx, qy, q, q, 0, rgb8(mc));
                p.check(qx + q / 2, qy + q / 2, q * 0.55f, 3.5f * u_, hex(PAPER));
            } else {
                p.rrect(qx, qy, q, q, 0, rgb8(mc, 0.2f));
                p.rrectLine(qx, qy, q, q, 0, 3 * u_, rgb8(mc));
            }
        }
        const float sx = X(520);
        if (!open) p.text(F::INTER, "Locked", sx, y + 2 * u_, 22 * u_, hex(GREY));
        else if (progress_ && progress_->cleared(s, diff_)) {
            p.text(F::INTER_BOLD, "Cleared", sx, y + 2 * u_, 22 * u_, hex(RED));
            p.text(F::INTER, std::to_string(progress_->stageBest(s, diff_)), sx + 120 * u_, y + 2 * u_, 22 * u_, hex(INK));
        } else if (progress_ && progress_->clearedAny(s)) {
            std::string on = "Cleared on";
            for (int d = 0; d < DIFF_COUNT; d++)
                if (progress_->cleared(s, d)) on += " " + title(difficultyName(d));
            p.text(F::INTER, on, sx, y + 2 * u_, 22 * u_, hex(GREY));
        }
    }
    // the focused square
    if (aRow_ > 0.02f) {
        const float q = 38 * u_;
        p.rrectLine(X(220 + aSong_ * 54) - 7 * u_, Y(top + aStage_ * rowH) - 31 * u_, q + 14 * u_, q + 14 * u_, 0, 3 * u_,
                    hex(INK, std::min(1.f, aRow_)));
    }
    p.rrect(X(1060), Y(230), 3 * u_, 720 * u_, 0, hex(INK));
    const SongView v = songView();
    const float ix = X(1110);
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "Stage %s  /  Song %d of %d", two(v.stage + 1).c_str(), v.song + 1, v.songs);
        p.text(F::INTER_BOLD, b, ix, Y(282), 24 * u_, hex(GREY));
        float y = Y(370);
        for (const std::string& l : wrap(p, F::INTER_BOLD, v.title, 58 * u_, 700 * u_, 3)) {
            p.text(F::INTER_BOLD, l, ix, y, 58 * u_, hex(INK), -1, -0.02f);
            y += 66 * u_;
        }
        p.text(F::INTER_BOLD, title(v.universe.c_str()), ix, y + 8 * u_, 30 * u_, hex(RED));
        y += 90 * u_;
        auto row = [&](const char* k, const std::string& val, Col c) {
            p.line(ix, y - 34 * u_, X(1816), y - 34 * u_, 1 * u_, hex(INK, 0.25f));
            p.text(F::INTER, k, ix, y, 24 * u_, hex(GREY));
            p.text(F::INTER_BOLD, val, X(1330), y, 24 * u_, c);
            y += 52 * u_;
        };
        row("Velocity", fmtVel(v.velocity), hex(INK));
        row("Best", v.beaten ? std::to_string(v.best) : "-", hex(INK));
        row("Status", v.beaten ? "Beaten" : std::string("Not beaten on ") + title(difficultyName(diff_)), v.beaten ? hex(RED) : hex(INK));
        row("Preview", v.preview.empty() ? "-" : title(v.preview.c_str()), hex(INK));
        if (v.song > 0) p.text(F::INTER, "A run from here doesn't clear the stage.", ix, y + 6 * u_, 20 * u_, hex(GREY));
    } else {
        p.text(F::INTER_BOLD, "Choose a difficulty,", ix, Y(320), 44 * u_, hex(INK));
        p.text(F::INTER_BOLD, "then a song.", ix, Y(375), 44 * u_, hex(INK));
        p.text(F::INTER, "Left / right: difficulty. Down: the stages.", ix, Y(430), 24 * u_, hex(GREY));
    }
    p.line(X(104), Y(990), X(1816), Y(990), 2 * u_, hex(INK));
    p.text(F::INTER, "Enter play    Esc back    Up / down stage    Left / right song", X(104), Y(1030), 20 * u_, hex(INK, 0.7f));
}

// =====================================================================================================================
// WATERCOLOUR: a hand-painted journey map; the stages are stops along a winding dotted path, each song a painted tile.

namespace {
const uint32_t W_INK = 0x2b3a55, W_SEPIA = 0x5a4632, W_OCHRE = 0xe0a64a, W_RED = 0xb8322a;
Col paperize(const unsigned char c[3], float a = 1.f) { // pigment on paper: lighter, a little warm
    return mixc(rgb8(c, a), hex(0xf4ecdc, a), 0.32f);
}
// a wash: a few overlapping layers of pigment with crisp, uneven edges, darker where the water dried (the rim)
void wash(Painter& p, float x, float y, float r, Col c, int seed = 0) {
    const Col rim = mixc(c, hex(0x3a3050, c.a), 0.25f);
    for (int i = 0; i < 5; i++) {
        const float a = seed * 1.7f + i * 2.39996f, d = r * 0.11f * (1 + (i * 37 + seed * 11) % 4) / 4.f;
        const float rr = r * (0.84f + 0.04f * ((i + seed) % 4)), px = x + std::cos(a) * d, py = y + std::sin(a) * d;
        p.circle(px, py, rr, withA(c, c.a * 0.26f), 2.5f * (r / 60.f));
        p.ring(px, py, rr - r * 0.03f, r * 0.05f, withA(rim, c.a * 0.16f), r * 0.05f);
    }
    p.circle(x - r * 0.2f, y - r * 0.15f, r * 0.45f, hex(0xffffff, 0.10f * c.a), r * 0.4f); // the lighter, wetter centre
}
// a brush stroke under a word
void stroke(Painter& p, float x0, float x1, float y, float r, Col c) {
    const int n = std::max(2, (int)((x1 - x0) / (r * 0.5f)));
    for (int i = 0; i <= n; i++) {
        const float t = i / (float)n;
        const float yy = y + std::sin(t * 5.1f) * r * 0.12f;
        p.circle(x0 + (x1 - x0) * t, yy, r * (0.85f + 0.15f * std::sin(t * 9.f)), withA(c, c.a / 5.f), r * 0.18f);
    }
}
} // namespace

void Menu::paperTitle() {
    Painter& p = paint_;
    const float cx = X(960), t = time_;
    wash(p, cx - 360 * u_, Y(330), 230 * u_, hex(0x7fb2d6, 0.5f), 1);
    wash(p, cx + 300 * u_, Y(350), 210 * u_, hex(W_OCHRE, 0.45f), 2);
    wash(p, cx - 20 * u_, Y(420), 170 * u_, hex(0x9cc48a, 0.4f), 3);
    // painted tetrominoes drifting on the paper
    static const int SH[4][4][2] = {{{0, 0}, {1, 0}, {2, 0}, {1, 1}}, {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
                                    {{0, 1}, {1, 1}, {1, 0}, {2, 0}}, {{0, 0}, {0, 1}, {1, 1}, {2, 1}}};
    static const uint32_t PC[4] = {0xb06ab3, 0xe6c04a, 0x6fae6a, 0x4f7fc0};
    const float PX[4] = {180, 1600, 260, 1520}, PY[4] = {180, 220, 820, 800};
    for (int i = 0; i < 4; i++) {
        const float q = 44 * u_, bx = X(PX[i]), by = Y(PY[i] + 10 * std::sin(t * 0.2f + i));
        for (auto& c : SH[i]) {
            p.rrect(bx + c[0] * q, by + c[1] * q, q - 3 * u_, q - 3 * u_, 6 * u_, hex(PC[i], 0.55f), 3 * u_);
            p.rrectLine(bx + c[0] * q, by + c[1] * q, q - 3 * u_, q - 3 * u_, 6 * u_, 2 * u_, hex(PC[i], 0.4f), 2 * u_);
        }
    }
    p.text(F::KALAM, "Tetris Universe", cx, Y(400), 150 * u_, hex(W_INK, 0.92f), 0);
    p.text(F::YRSA, "a journey through sixty painted worlds", cx, Y(475), 34 * u_, hex(W_SEPIA, 0.85f), 0);
    const char* items[3] = {"Campaign", "Free play", "Quit"};
    float wsel = 0;
    for (int i = 0; i < 3; i++) wsel += near1(aTitle_, i) * p.width(F::KALAM, items[i], 66 * u_);
    const float ys = Y(660 + aTitle_ * 92) - 18 * u_;
    stroke(p, cx - wsel / 2 - 20 * u_, cx + wsel / 2 + 20 * u_, ys, 30 * u_, hex(W_OCHRE, 0.9f));
    for (int i = 0; i < 3; i++) {
        const float f = near1(aTitle_, i);
        p.text(F::KALAM, items[i], cx, Y(660 + i * 92), 66 * u_, hex(W_INK, 0.5f + 0.5f * f), 0);
    }
    if (progress_ && camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d of %d stages cleared", progress_->clearedCount(), camp_->size());
        p.text(F::YRSA, b, cx, Y(965), 28 * u_, hex(W_SEPIA, 0.8f), 0);
    }
    p.text(F::YRSA, "arrows to move  -  enter to choose  -  esc to leave", cx, Y(1022), 24 * u_, hex(W_SEPIA, 0.6f), 0);
}

void Menu::paperFree() {
    Painter& p = paint_;
    const float cx = X(960);
    p.text(F::KALAM, "Free play", cx, Y(240), 120 * u_, hex(W_INK, 0.92f), 0);
    p.text(F::YRSA, "the playlist in a random order, a world for each song, levels rising with the lines", cx, Y(310), 30 * u_,
           hex(W_SEPIA, 0.85f), 0);
    static const uint32_t C[3] = {0x8cc47a, 0xe0a64a, 0xd0605a};
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(aFree_, d)), x = cx + (d - 1) * 470 * u_, y = Y(600);
        wash(p, x, y, (130 + 25 * f) * u_, hex(C[d], 0.35f + 0.45f * f), d + 4);
        if (f > 0.02f) p.ring(x, y, 150 * u_, 3 * u_, hex(W_INK, 0.45f * f), 2.5f * u_);
        p.text(F::KALAM, title(difficultyName(d)), x, y + 18 * u_, 66 * u_, hex(W_INK, 0.45f + 0.55f * f), 0);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", freePlayMultiplier(d));
        p.text(F::YRSA, b, x, y + 70 * u_, 34 * u_, hex(W_SEPIA, 0.5f + 0.4f * f), 0);
    }
    char b[96];
    std::snprintf(b, sizeof(b), "gravity at %d%% of its full speed, on every level", (int)std::lround(freePlayMultiplier(freeDiff_) * 100));
    p.text(F::YRSA, b, cx, Y(900), 32 * u_, hex(W_INK, 0.85f), 0);
    p.text(F::YRSA, "enter to start  -  esc to go back", cx, Y(1022), 24 * u_, hex(W_SEPIA, 0.6f), 0);
}

void Menu::paperCampaign() {
    Painter& p = paint_;
    p.text(F::KALAM, "The journey", X(90), Y(118), 84 * u_, hex(W_INK, 0.92f));
    // difficulty: a brush stroke under the chosen one
    float tx[DIFF_COUNT], tw[DIFF_COUNT], x = X(760);
    for (int d = 0; d < DIFF_COUNT; d++) { tw[d] = p.width(F::KALAM, title(difficultyName(d)), 44 * u_); tx[d] = x; x += tw[d] + 60 * u_; }
    {
        const int d0 = std::clamp((int)std::floor(aDiff_), 0, DIFF_COUNT - 1), d1 = std::min(DIFF_COUNT - 1, d0 + 1);
        const float f = aDiff_ - d0, bx = lerpf(tx[d0], tx[d1], f), bw = lerpf(tw[d0], tw[d1], f);
        const float on = 1.f - std::min(1.f, aRow_);
        stroke(p, bx - 10 * u_, bx + bw + 10 * u_, Y(100), 24 * u_, hex(W_OCHRE, 0.55f + 0.4f * on));
    }
    for (int d = 0; d < DIFF_COUNT; d++) p.text(F::KALAM, title(difficultyName(d)), tx[d], Y(112), 44 * u_, hex(W_INK, 0.45f + 0.55f * near1(aDiff_, d)));
    if (!camp_ || camp_->size() == 0) return;
    const int n = camp_->size();
    // the stops on a serpentine: rows of four, every other one walked back
    auto stop = [&](int s, float& sx, float& sy) {
        const int row = s / 4, col = s % 4, c = row % 2 ? 3 - col : col;
        sx = X(190 + c * 320 + 18 * std::sin(s * 1.9f));
        sy = Y(270 + row * 245 + 22 * std::sin(s * 1.3f + 0.5f));
    };
    // the dotted path (a curve through the stops)
    for (int s = 0; s + 1 < n; s++) {
        float x0, y0, x1, y1;
        stop(s, x0, y0);
        stop(s + 1, x1, y1);
        const bool open = !progress_ || progress_->unlocked(s + 1);
        const bool turn = (s % 4) == 3;
        const float bend = turn ? ((s / 4) % 2 ? -150.f : 150.f) * u_ : 40 * u_ * std::sin(s * 2.1f);
        const float mx = (x0 + x1) / 2 + (turn ? bend : 0), my = (y0 + y1) / 2 + (turn ? 0 : bend);
        const float len = std::hypot(x1 - x0, y1 - y0) + std::fabs(bend);
        const int dots = (int)(len / (17 * u_));
        for (int i = 1; i < dots; i++) {
            const float t = i / (float)dots;
            const float bx = (1 - t) * (1 - t) * x0 + 2 * (1 - t) * t * mx + t * t * x1;
            const float by = (1 - t) * (1 - t) * y0 + 2 * (1 - t) * t * my + t * t * y1;
            p.circle(bx, by, 2.6f * u_, hex(W_SEPIA, open ? 0.5f : 0.18f), 1.5f * u_);
        }
    }
    auto tilePos = [&](int s, int k, float& tx2, float& ty2) {
        float sx, sy;
        stop(s, sx, sy);
        tx2 = sx + (k - 2) * 40 * u_;
        ty2 = sy + 84 * u_;
    };
    for (int s = 0; s < n; s++) {
        float sx, sy;
        stop(s, sx, sy);
        const bool open = !progress_ || progress_->unlocked(s);
        const float f = aRow_ > 0.5f ? sstep(near1(aStage_, s)) : 0.f;
        unsigned char sc[3];
        stageColor(s, sc);
        wash(p, sx, sy, (52 + 6 * f) * u_, open ? paperize(sc, 0.75f) : hex(0xb0aaa0, 0.35f), s);
        if (f > 0.02f) p.ring(sx, sy, 60 * u_, 2.5f * u_, hex(W_INK, 0.5f * f), 2.5f * u_);
        p.text(F::KALAM, std::to_string(s + 1), sx, sy + 17 * u_, 46 * u_, hex(W_INK, open ? 0.9f : 0.35f), 0);
        if (!open) p.lock(sx + 40 * u_, sy - 36 * u_, 20 * u_, hex(W_SEPIA, 0.6f));
        if (progress_ && progress_->cleared(s, diff_)) { // a red ink stamp
            p.ring(sx + 46 * u_, sy - 40 * u_, 17 * u_, 3 * u_, hex(W_RED, 0.8f), 1.5f * u_);
            p.check(sx + 46 * u_, sy - 40 * u_, 18 * u_, 3 * u_, hex(W_RED, 0.85f));
            p.text(F::YRSA, std::to_string(progress_->stageBest(s, diff_)), sx, sy + 136 * u_, 22 * u_, hex(W_RED, 0.8f), 0);
        }
        const auto& songs = camp_->stages()[s].songs;
        for (int k = 0; k < (int)songs.size(); k++) {
            float qx, qy;
            tilePos(s, k, qx, qy);
            const float q = 32 * u_;
            unsigned char mc[3];
            menuUniverseColor(camp_, songs[k].universe, mc);
            const bool beat = progress_ && progress_->beaten(s, k, diff_);
            const Col c = open ? paperize(mc, beat ? 0.95f : 0.45f) : hex(0xb0aaa0, 0.3f);
            p.rrect(qx - q / 2, qy - q / 2, q, q, 7 * u_, c, 2.5f * u_);
            p.rrectLine(qx - q / 2, qy - q / 2, q, q, 7 * u_, 2.2f * u_, withA(mixc(c, hex(W_INK), 0.3f), c.a * 0.5f), 2 * u_);
            if (beat && open) p.check(qx, qy, q * 0.6f, 2.6f * u_, hex(W_INK, 0.85f));
        }
    }
    if (aRow_ > 0.5f) { // the focused tile: an ink outline gliding
        const int s0 = std::clamp((int)std::floor(aStage_), 0, n - 1), s1 = std::min(n - 1, s0 + 1);
        const int k0 = std::clamp((int)std::floor(aSong_), 0, 4), k1 = std::min(4, k0 + 1);
        const float fs = aStage_ - s0, fk = aSong_ - k0;
        float a0x, a0y, a1x, a1y, b0x, b0y, b1x, b1y;
        tilePos(s0, k0, a0x, a0y); tilePos(s0, k1, a1x, a1y); tilePos(s1, k0, b0x, b0y); tilePos(s1, k1, b1x, b1y);
        const float fx = lerpf(lerpf(a0x, a1x, fk), lerpf(b0x, b1x, fk), fs), fy = lerpf(lerpf(a0y, a1y, fk), lerpf(b0y, b1y, fk), fs);
        const float q = 46 * u_;
        p.rrectLine(fx - q / 2, fy - q / 2, q, q, 10 * u_, 3 * u_, hex(W_INK, 0.85f * std::min(1.f, aRow_)), 1.5f * u_);
    }
    // the parchment card
    const float cx0 = X(1370), cy0 = Y(200), cw = 470 * u_, ch = 760 * u_;
    p.rrect(cx0 + 8 * u_, cy0 + 14 * u_, cw, ch, 20 * u_, hex(0x3a2a10, 0.14f), 30 * u_);
    p.rrect(cx0, cy0, cw, ch, 20 * u_, hex(0xfbf6ea, 0.92f), 3 * u_);
    p.rrectLine(cx0, cy0, cw, ch, 20 * u_, 2 * u_, hex(W_SEPIA, 0.3f), 2 * u_);
    const SongView v = songView();
    const float ix = cx0 + 36 * u_;
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "Stage %d, song %d of %d", v.stage + 1, v.song + 1, v.songs);
        p.text(F::YRSA, b, ix, cy0 + 62 * u_, 28 * u_, hex(W_SEPIA, 0.85f));
        float y = cy0 + 140 * u_;
        for (const std::string& l : wrap(p, F::KALAM, v.title, 48 * u_, cw - 72 * u_, 4)) {
            p.text(F::KALAM, l, ix, y, 48 * u_, hex(W_INK, 0.92f));
            y += 58 * u_;
        }
        wash(p, ix + 18 * u_, y + 4 * u_, 22 * u_, paperize(v.col, 0.9f), 9);
        p.text(F::KALAM, title(v.universe.c_str()), ix + 54 * u_, y + 16 * u_, 36 * u_, mixc(rgb8(v.col), hex(W_INK), 0.45f));
        y += 90 * u_;
        p.text(F::YRSA, "velocity " + fmtVel(v.velocity), ix, y, 30 * u_, hex(W_INK, 0.85f));
        y += 46 * u_;
        if (v.beaten) {
            p.check(ix + 12 * u_, y - 10 * u_, 22 * u_, 3 * u_, hex(W_RED, 0.85f));
            p.text(F::YRSA, "beaten, best " + std::to_string(v.best), ix + 40 * u_, y, 30 * u_, hex(W_RED, 0.9f));
        } else {
            p.text(F::YRSA, std::string("not beaten yet on ") + title(difficultyName(diff_)), ix, y, 30 * u_, hex(W_INK, 0.7f));
        }
        y += 46 * u_;
        p.text(F::YRSA, title(v.preview.c_str()), ix, y, 26 * u_, hex(0x3f7fa8, 0.9f));
        if (v.song > 0) p.text(F::YRSA, "a run from here doesn't clear the stage", ix, cy0 + ch - 80 * u_, 22 * u_, hex(W_SEPIA, 0.7f));
    } else {
        p.text(F::KALAM, "Pick a difficulty,", ix, cy0 + 120 * u_, 46 * u_, hex(W_INK, 0.9f));
        p.text(F::KALAM, "then a stop", ix, cy0 + 176 * u_, 46 * u_, hex(W_INK, 0.9f));
        p.text(F::YRSA, "left and right: difficulty; down: the map", ix, cy0 + 240 * u_, 24 * u_, hex(W_SEPIA, 0.8f));
    }
    p.text(F::YRSA, "enter to play  -  esc to go back", ix, cy0 + ch - 36 * u_, 24 * u_, hex(W_SEPIA, 0.65f));
}

// =====================================================================================================================
// GLASS: frosted glass cards over a slow colour gradient, neon accents, smooth focus.

namespace {
const uint32_t CYAN = 0x4de8ff, PINK = 0xff4fd8;
void card(Painter& p, float x, float y, float w, float h, float r, float u) {
    p.rrect(x, y + 14 * u, w, h, r, hex(0x000000, 0.35f), 44 * u);
    p.rrect(x, y, w, h, r, hex(0xffffff, 0.075f));
    p.rrectLine(x, y, w, h, r, 1.2f * u, hex(0xffffff, 0.2f));
    p.line(x + r, y + 1.2f * u, x + w - r, y + 1.2f * u, 1.2f * u, hex(0xffffff, 0.3f)); // the lit top edge
}
void neon(Painter& p, float x, float y, float w, float h, float r, Col c, float u, float a = 1.f) {
    p.setAdditive(true);
    p.rrectLine(x, y, w, h, r, 3 * u, withA(c, 0.35f * a), 16 * u);
    p.setAdditive(false);
    p.rrectLine(x, y, w, h, r, 1.8f * u, withA(c, 0.95f * a));
}
void glowText(Painter& p, F f, const std::string& s, float x, float y, float size, Col glow, Col ink, int align, float track, float u) {
    p.setAdditive(true);
    for (int i = 0; i < 8; i++) {
        const float a = i * 0.785f;
        p.text(f, s, x + std::cos(a) * 4 * u, y + std::sin(a) * 4 * u, size, withA(glow, glow.a / 8.f), align, track);
    }
    p.setAdditive(false);
    p.text(f, s, x, y, size, ink, align, track);
}
} // namespace

void Menu::glassTitle() {
    Painter& p = paint_;
    const float cx = X(960);
    glowText(p, F::MANROPE, "TETRIS", cx, Y(330), 150 * u_, hex(PINK, 0.9f), hex(0xffffff), 0, 0.18f, u_);
    p.text(F::MANROPE_LIGHT, "UNIVERSE", cx, Y(400), 36 * u_, hex(CYAN, 0.95f), 0, 0.8f);
    const float bx = cx - 310 * u_, by = Y(500), bw = 620 * u_;
    card(p, bx, by, bw, 330 * u_, 32 * u_, u_);
    const float py = Y(530 + aTitle_ * 92);
    p.rrect(bx + 30 * u_, py, bw - 60 * u_, 76 * u_, 38 * u_, hex(0xffffff, 0.1f));
    neon(p, bx + 30 * u_, py, bw - 60 * u_, 76 * u_, 38 * u_, hex(CYAN), u_);
    const char* items[3] = {"CAMPAIGN", "FREE PLAY", "QUIT"};
    for (int i = 0; i < 3; i++)
        p.text(F::MANROPE, items[i], cx, Y(582 + i * 92), 34 * u_, hex(0xffffff, 0.5f + 0.5f * near1(aTitle_, i)), 0, 0.25f);
    if (progress_ && camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d STAGES CLEARED", progress_->clearedCount(), camp_->size());
        p.text(F::MANROPE, b, cx, Y(930), 19 * u_, hex(0xffffff, 0.6f), 0, 0.3f);
    }
    p.text(F::MANROPE_LIGHT, "ARROWS  MOVE      ENTER  SELECT      ESC  QUIT", cx, Y(1010), 18 * u_, hex(0xffffff, 0.45f), 0, 0.25f);
}

void Menu::glassFree() {
    Painter& p = paint_;
    const float cx = X(960);
    glowText(p, F::MANROPE, "FREE PLAY", cx, Y(230), 96 * u_, hex(CYAN, 0.7f), hex(0xffffff), 0, 0.12f, u_);
    p.text(F::MANROPE_LIGHT, "The playlist in a random order. Levels rise with the lines you clear.", cx, Y(300), 26 * u_, hex(0xffffff, 0.65f), 0);
    for (int d = 0; d < DIFF_COUNT; d++) {
        const float f = sstep(near1(aFree_, d));
        const float s = 1.f + 0.07f * f, w = 340 * u_ * s, h = 380 * u_ * s;
        const float x = cx + (d - 1) * 420 * u_ - w / 2, y = Y(610) - h / 2 - 16 * u_ * f;
        card(p, x, y, w, h, 30 * u_, u_);
        if (f > 0.02f) neon(p, x, y, w, h, 30 * u_, hex(d == 2 ? PINK : CYAN), u_, f);
        char b[16];
        std::snprintf(b, sizeof(b), "x%.1f", freePlayMultiplier(d));
        p.text(F::MANROPE, b, x + w / 2, y + h * 0.48f, 88 * u_, hex(0xffffff, 0.55f + 0.45f * f), 0);
        p.text(F::MANROPE, difficultyName(d), x + w / 2, y + h * 0.66f, 26 * u_, hex(0xffffff, 0.5f + 0.5f * f), 0, 0.4f);
        for (int i = 0; i < 5; i++) { // a speed gauge
            const bool lit = i < (int)std::lround(freePlayMultiplier(d) * 5);
            p.rrect(x + w / 2 - 70 * u_ + i * 29 * u_, y + h * 0.78f, 22 * u_, 8 * u_, 4 * u_,
                    lit ? withA(hex(d == 2 ? PINK : CYAN), 0.5f + 0.5f * f) : hex(0xffffff, 0.12f));
        }
    }
    char b[96];
    std::snprintf(b, sizeof(b), "GRAVITY AT %d%% OF ITS FULL SPEED, ON EVERY LEVEL", (int)std::lround(freePlayMultiplier(freeDiff_) * 100));
    p.text(F::MANROPE, b, cx, Y(920), 20 * u_, hex(0xffffff, 0.75f), 0, 0.2f);
    p.text(F::MANROPE_LIGHT, "ENTER  START      ESC  BACK", cx, Y(1010), 18 * u_, hex(0xffffff, 0.45f), 0, 0.25f);
}

void Menu::glassCampaign() {
    Painter& p = paint_;
    const float cx = X(960), t = time_;
    p.text(F::MANROPE, "CAMPAIGN", X(100), Y(108), 30 * u_, hex(0xffffff, 0.9f), -1, 0.4f);
    // the segmented difficulty control
    {
        const float sx0 = cx - 280 * u_, sw = 560 * u_, sh = 62 * u_, sy0 = Y(60);
        card(p, sx0, sy0, sw, sh, 31 * u_, u_);
        const float hx = sx0 + 8 * u_ + aDiff_ * (sw - 16 * u_) / 3.f;
        p.rrect(hx, sy0 + 7 * u_, (sw - 16 * u_) / 3.f, sh - 14 * u_, 24 * u_, hex(PINK, 0.28f));
        const float on = 1.f - std::min(1.f, aRow_);
        neon(p, hx, sy0 + 7 * u_, (sw - 16 * u_) / 3.f, sh - 14 * u_, 24 * u_, hex(on > 0.5f ? CYAN : 0xffffff), u_, 0.35f + 0.65f * on);
        for (int d = 0; d < DIFF_COUNT; d++)
            p.text(F::MANROPE, difficultyName(d), sx0 + 8 * u_ + (d + 0.5f) * (sw - 16 * u_) / 3.f, sy0 + 39 * u_, 20 * u_,
                   hex(0xffffff, 0.5f + 0.5f * near1(aDiff_, d)), 0, 0.2f);
    }
    if (!camp_ || camp_->size() == 0) return;
    const int n = camp_->size();
    const float gx = X(100), gy = Y(165), gw = 1000 * u_, gh = 860 * u_;
    card(p, gx, gy, gw, gh, 28 * u_, u_);
    const float rowH = 68 * u_, top = gy + 50 * u_;
    if (aRow_ > 0.02f) p.rrect(gx + 24 * u_, top + aStage_ * rowH - 30 * u_, gw - 48 * u_, 60 * u_, 16 * u_, hex(0xffffff, 0.07f * std::min(1.f, aRow_)));
    for (int s = 0; s < n; s++) {
        const float y = top + s * rowH;
        const bool open = !progress_ || progress_->unlocked(s);
        if (!open) p.lock(gx + 58 * u_, y, 18 * u_, hex(0xffffff, 0.35f));
        p.text(F::MANROPE_LIGHT, "STAGE " + two(s + 1), gx + 88 * u_, y + 8 * u_, 22 * u_, hex(0xffffff, open ? 0.85f : 0.3f), -1, 0.2f);
        const auto& songs = camp_->stages()[s].songs;
        for (int k = 0; k < (int)songs.size(); k++) {
            const float f = aRow_ > 0.5f ? sstep(near1(aStage_, s)) * sstep(near1(aSong_, k)) : 0.f;
            const float q = (44 + 8 * f) * u_, qx = gx + (320 + k * 72) * u_, qy = y;
            unsigned char mc[3];
            menuUniverseColor(camp_, songs[k].universe, mc);
            const bool beat = progress_ && progress_->beaten(s, k, diff_);
            if (!open) {
                p.rrect(qx - q / 2, qy - q / 2, q, q, 11 * u_, hex(0xffffff, 0.05f));
                p.rrectLine(qx - q / 2, qy - q / 2, q, q, 11 * u_, 1 * u_, hex(0xffffff, 0.12f));
                continue;
            }
            if (beat) {
                p.setAdditive(true);
                p.rrect(qx - q / 2, qy - q / 2, q, q, 11 * u_, rgb8(mc, 0.35f), 18 * u_);
                p.setAdditive(false);
            }
            p.rrect(qx - q / 2, qy - q / 2, q, q, 11 * u_, rgb8(mc, beat ? 0.9f : 0.28f));
            p.rrect(qx - q / 2, qy - q / 2, q, q * 0.42f, 11 * u_, hex(0xffffff, beat ? 0.2f : 0.08f));
            p.rrectLine(qx - q / 2, qy - q / 2, q, q, 11 * u_, 1.4f * u_, rgb8(mc, beat ? 0.4f : 0.85f));
            if (beat) p.check(qx, qy, q * 0.5f, 3.2f * u_, hex(0xffffff, 0.95f));
        }
        const float sx = gx + 700 * u_;
        if (open && progress_ && progress_->cleared(s, diff_)) {
            p.text(F::MANROPE, "CLEAR", sx, y + 8 * u_, 18 * u_, hex(CYAN), -1, 0.3f);
            p.text(F::MANROPE_LIGHT, std::to_string(progress_->stageBest(s, diff_)), sx + 90 * u_, y + 8 * u_, 22 * u_, hex(0xffffff, 0.85f));
        } else if (open && progress_ && progress_->clearedAny(s)) {
            std::string on = "CLEARED ON";
            for (int d = 0; d < DIFF_COUNT; d++)
                if (progress_->cleared(s, d)) on += std::string(" ") + difficultyName(d);
            p.text(F::MANROPE_LIGHT, on, sx, y + 8 * u_, 16 * u_, hex(0xffffff, 0.45f), -1, 0.15f);
        }
    }
    if (aRow_ > 0.5f) {
        const float q = 60 * u_, fx = gx + (320 + aSong_ * 72) * u_, fy = top + aStage_ * rowH;
        neon(p, fx - q / 2, fy - q / 2, q, q, 15 * u_, hex(CYAN), u_, std::min(1.f, aRow_) * (0.85f + 0.15f * std::sin(t * 2.f)));
    }
    // the song card
    const float ix0 = X(1140), iy0 = gy, iw = 680 * u_, ih = gh;
    card(p, ix0, iy0, iw, ih, 28 * u_, u_);
    const SongView v = songView();
    const float ix = ix0 + 44 * u_;
    if (v.has) {
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %s   -   SONG %d / %d", two(v.stage + 1).c_str(), v.song + 1, v.songs);
        p.text(F::MANROPE, b, ix, iy0 + 70 * u_, 19 * u_, hex(0xffffff, 0.6f), -1, 0.3f);
        float y = iy0 + 150 * u_;
        for (const std::string& l : wrap(p, F::MANROPE, v.title, 46 * u_, iw - 88 * u_, 3)) {
            p.text(F::MANROPE, l, ix, y, 46 * u_, hex(0xffffff, 0.96f));
            y += 56 * u_;
        }
        const float uw = p.width(F::MANROPE, v.universe, 18 * u_, 0.3f);
        p.rrect(ix, y - 10 * u_, uw + 40 * u_, 44 * u_, 22 * u_, rgb8(v.col, 0.25f));
        p.rrectLine(ix, y - 10 * u_, uw + 40 * u_, 44 * u_, 22 * u_, 1.2f * u_, rgb8(v.col, 0.8f));
        p.text(F::MANROPE, v.universe, ix + 20 * u_, y + 19 * u_, 18 * u_, hex(0xffffff), -1, 0.3f);
        y += 80 * u_;
        const float cw = (iw - 88 * u_ - 24 * u_) / 2;
        auto stat = [&](float x, const char* k, const std::string& val, Col c) {
            card(p, x, y, cw, 120 * u_, 20 * u_, u_);
            p.text(F::MANROPE, k, x + 24 * u_, y + 40 * u_, 16 * u_, hex(0xffffff, 0.55f), -1, 0.3f);
            p.text(F::MANROPE, val, x + 24 * u_, y + 96 * u_, 44 * u_, c);
        };
        stat(ix, "VELOCITY", fmtVel(v.velocity), hex(0xffffff));
        stat(ix + cw + 24 * u_, "BEST", v.beaten ? std::to_string(v.best) : "-", v.beaten ? hex(CYAN) : hex(0xffffff, 0.5f));
        y += 170 * u_;
        if (v.beaten) {
            p.check(ix + 12 * u_, y - 8 * u_, 22 * u_, 3 * u_, hex(CYAN));
            p.text(F::MANROPE, "BEATEN", ix + 40 * u_, y, 20 * u_, hex(CYAN), -1, 0.3f);
        } else {
            p.text(F::MANROPE, std::string("NOT BEATEN ON ") + difficultyName(diff_), ix, y, 20 * u_, hex(0xffffff, 0.55f), -1, 0.3f);
        }
        y += 56 * u_;
        if (!v.preview.empty()) { // a small equaliser while it plays
            const bool playing = v.preview == "PREVIEW PLAYING";
            for (int i = 0; i < 4; i++) {
                const float hh = (playing ? 8 + 14 * (0.5f + 0.5f * std::sin(t * (3.1f + i) + i * 1.7f)) * (0.4f + level_) : 6) * u_;
                p.rrect(ix + i * 10 * u_, y - hh, 6 * u_, hh, 3 * u_, hex(PINK, 0.9f));
            }
            p.text(F::MANROPE, v.preview, ix + 52 * u_, y, 18 * u_, hex(PINK), -1, 0.25f);
        }
        if (v.song > 0) p.text(F::MANROPE_LIGHT, "A run from here doesn't clear the stage.", ix, iy0 + ih - 90 * u_, 20 * u_, hex(0xffffff, 0.5f));
    } else {
        p.text(F::MANROPE, "Choose a difficulty,", ix, iy0 + 150 * u_, 40 * u_, hex(0xffffff));
        p.text(F::MANROPE, "then a song.", ix, iy0 + 200 * u_, 40 * u_, hex(0xffffff));
        p.text(F::MANROPE_LIGHT, "Left / right: difficulty. Down: the stages.", ix, iy0 + 250 * u_, 22 * u_, hex(0xffffff, 0.6f));
    }
    p.text(F::MANROPE_LIGHT, "ENTER  PLAY      ESC  BACK", ix, iy0 + ih - 40 * u_, 18 * u_, hex(0xffffff, 0.5f), -1, 0.25f);
}

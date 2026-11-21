// The debrief at the end of a Voyage run, in place of a message over the menu: the REFALL logo a little above the middle
// (in silver after a game over, over the frozen and darkened game; turning from silver into its colours on the black
// for a stage cleared), the heading, the score and the lines, a small line of details.
// A key, after a moment (a key held from the game must not skip it), fades it out and the Voyage's screen fades in.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "menu.hpp"

namespace {
using F = Painter::Font;
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
// a calm ease, slow at both ends (the logo taking its colours)
float easeInOutSine(float t) { t = std::clamp(t, 0.f, 1.f); return 0.5f - 0.5f * std::cos(t * 3.14159265f); }

constexpr float INPUT_AFTER = 1.5f;  // keys are ignored this long (a key held from the game)
constexpr float LEAVE = 0.6f;        // the debrief fades to black...
constexpr float ENTER = 0.7f;        // ...then the Voyage's screen fades in
constexpr float COLOR_AT = 0.6f, COLOR_FOR = 1.2f; // a stage cleared: the logo takes its colours

} // namespace

void Menu::openDebrief(const Debrief& d) {
    screen_ = DEBRIEF;
    debrief_ = d;
    debT_ = 0;
    leaveT_ = -1;
    debSel_ = d.retryStage.kind != Choice::NONE && !d.songFirst ? 1 : 0; // (RETRY STAGE, or RETRY SONG once chosen in the run)
    debRetry_ = false;
    msgT_ = 0;
    msg_.clear();
}

void Menu::captureBackdrop(int w, int h) {
    dropBackdrop();
    if (w <= 0 || h <= 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glGenTextures(1, &backdrop_);
    glBindTexture(GL_TEXTURE_2D, backdrop_);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 0, 0, w, h, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    backW_ = w;
    backH_ = h;
}

void Menu::dropBackdrop() {
    if (backdrop_) glDeleteTextures(1, &backdrop_);
    backdrop_ = 0;
}

bool Menu::updateDebrief(const Input& in, float dt, Choice& out) {
    if (screen_ != DEBRIEF) return false;
    debT_ += dt;
    const bool choice = debrief_.retry.kind != Choice::NONE;
    if (leaveT_ < 0) {
        if (choice) { // RETRY SONG, RETRY STAGE or MENU
            if (debT_ > INPUT_AFTER) {
                const int d = in.repeatSteps(A_UI_RIGHT) + in.repeatSteps(A_UI_DOWN) - in.repeatSteps(A_UI_LEFT) - in.repeatSteps(A_UI_UP);
                const int items = debrief_.retryStage.kind != Choice::NONE ? 3 : 2;
                if (d) { debSel_ = std::clamp(debSel_ + d, 0, items - 1); lastInput_ = time_; }
                if (in.pressed(A_UI_OK) || in.pressed(A_UI_ENTER)) { leaveT_ = 0; debRetry_ = debSel_ < items - 1; }
                else if (in.pressed(A_UI_BACK)) { leaveT_ = 0; debRetry_ = false; }
            }
        } else {
            bool key = false;
            for (Action a : {A_LEFT, A_RIGHT, A_SOFT, A_HARD, A_CW, A_CCW, A_HOLD, A_PAUSE, A_UI_OK, A_UI_BACK})
                key |= in.pressed(a);
            if (key && debT_ > INPUT_AFTER) leaveT_ = 0;
        }
    } else if ((leaveT_ += dt) >= LEAVE && debRetry_) { // the run again
        out = debSel_ == 1 && debrief_.retryStage.kind != Choice::NONE ? debrief_.retryStage : debrief_.retry;
        screen_ = CAMPAIGN;
        leaveT_ = -1;
        debRetry_ = false;
        dropBackdrop();
    } else if (leaveT_ >= LEAVE) { // on to the Voyage's screen, on the black, fading in
        screen_ = CAMPAIGN;
        leaveT_ = -1;
        enterT_ = 0;
        lastInput_ = time_;
        dropBackdrop();
    }
    return true;
}

// (a stage cleared never holds still: its logo's pieces go on riding the ring until a key leaves)
bool Menu::still() const { return screen_ == DEBRIEF && !debrief_.clear && leaveT_ < 0 && debT_ > COLOR_AT + COLOR_FOR + 0.2f; }

void Menu::drawEnterFade() {
    if (enterT_ >= ENTER) return;
    paint_.rrect(0, 0, fw_, fh_, 0, hex(0x000000, 1.f - sstep(enterT_ / ENTER)), 0);
}

void Menu::drawDebrief(int w, int h) {
    Painter& p = paint_;
    p.begin(w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (portrait_) place(960, 570, 540, 0.5f * (pTop_ + pBottom_), std::min(0.88f, (pBottom_ - pTop_) / 920.f)); // (larger: its widest line across the column)
    const float u = u_, t = debT_, out = leaveT_ >= 0 ? 1.f - sstep(leaveT_ / LEAVE) : 1.f;
    // the frozen game, darkening (cover-fitted, should the window have changed)
    if (backdrop_) {
        const float k = std::max(w / (float)backW_, h / (float)backH_), bw = backW_ * k, bh = backH_ * k;
        const float d = sstep(t / 1.4f), b = (1.f - 0.8f * d) * out;
        p.image(backdrop_, (w - bw) * 0.5f, (h - bh) * 0.5f, bw, bh, Col{b, b, b, 1.f}, true);
        // a soft shade down the middle, under the logo and the words
        p.rrect(X(960 - 420), Y(120), 840 * u_, 900 * u_, 300 * u_, hex(0x000000, 0.55f * d * out), 260 * u_);
    }
    p.frostSource();
    // the logo: it fades in in silver; a stage cleared, it then takes its colours
    const float cx = X(960), cy = Y(370);
    const float turned = debrief_.clear ? easeInOutSine((t - COLOR_AT) / COLOR_FOR) : 0.f;
    if (turned > 0.f) { // a faint warm light behind it as it takes its colours
        p.setAdditive(true);
        p.glow(cx, cy, 300 * u, hex(0xe8c06a, 0.07f * turned * out));
        p.setAdditive(false);
    }
    p.setFade(sstep(t / 1.2f) * out);
    drawOrbitLogo(cx, cy, 99.f, turned);
    p.setFade(1.f);
    // the words, one after the other
    auto at = [&](float t0) { return sstep((t - t0) / 0.9f) * out; };
    const Col ink = hex(0xffffff), dim = hex(0xe0e0f0, 0.65f), gold = hex(0xffd27a);
    p.text(F::JOSEFIN_LIGHT, debrief_.heading, cx, Y(748), 72 * u, withA(ink, at(0.5f)), 0, 0.16f);
    const float a2 = at(0.9f), colX = 190 * u;
    p.setFade(a2); // the score's frosted card, a small gold gem between its two columns
    p.glass(cx - 340 * u, Y(774), 680 * u, 132 * u, 22 * u, u, 0.f, 0.5f);
    p.setFade(1.f);
    p.text(F::JOSEFIN, "SCORE", cx - colX, Y(812), 17 * u, withA(dim, dim.a * a2), 0, 0.35f);
    p.text(F::JOSEFIN, "LINES", cx + colX, Y(812), 17 * u, withA(dim, dim.a * a2), 0, 0.35f);
    p.text(F::JOSEFIN_LIGHT, text::thousands(debrief_.score), cx - colX, Y(874), 54 * u, withA(ink, a2), 0, 0.04f);
    p.text(F::JOSEFIN_LIGHT, std::to_string(debrief_.lines), cx + colX, Y(874), 54 * u, withA(ink, a2), 0, 0.04f);
    p.line(cx, Y(796), cx, Y(884), 1 * u, withA(hex(0xe8c06a), 0.45f * a2));
    p.tri(cx, Y(832), cx + 7 * u, Y(840), cx, Y(848), withA(hex(0xfff0c8), a2), withA(hex(0xe8c06a), a2), withA(hex(0x8a6a30), a2));
    p.tri(cx, Y(832), cx, Y(848), cx - 7 * u, Y(840), withA(hex(0xfff0c8), a2), withA(hex(0x8a6a30), a2), withA(hex(0xc8a050), a2));
    const float a3 = at(1.3f);
    // (a note over the choices: the three lines spread a little further apart)
    const bool crowded = debrief_.retry.kind != Choice::NONE && !debrief_.note.empty();
    p.text(F::JOSEFIN, debrief_.detail, cx, Y(crowded ? 940 : 950), 17 * u, withA(dim, dim.a * a3), 0, 0.3f);
    if (!debrief_.note.empty())
        p.text(F::JOSEFIN, debrief_.note, cx, Y(crowded ? 974 : 990), 17 * u, withA(gold, 0.85f * (debrief_.clear ? at(COLOR_AT + 1.2f) : a3)), 0, 0.3f);
    if (debrief_.retry.kind != Choice::NONE) { // RETRY SONG, RETRY STAGE or MENU, the chosen one underlined
        const bool stage = debrief_.retryStage.kind != Choice::NONE;
        const int n = stage ? 3 : 2;
        const char* ITEMS[3] = {debrief_.retryIsStage ? "RETRY STAGE" : "RETRY SONG", stage ? "RETRY STAGE" : "MENU", "MENU"};
        const float a4 = sstep((t - INPUT_AFTER + 0.4f) / 0.5f) * out, gap = 90 * u, y = Y(crowded ? 1038 : 1018), size = 26 * u;
        // (laid out by their widths, an even gap between them, centred as a row)
        float w[3], total = (n - 1) * gap;
        for (int i = 0; i < n; i++) total += w[i] = p.width(F::JOSEFIN, ITEMS[i], size, 0.35f);
        float left = cx - total * 0.5f;
        for (int i = 0; i < n; i++) {
            const float x = left + w[i] * 0.5f;
            left += w[i] + gap;
            const bool on = i == debSel_;
            p.text(F::JOSEFIN, ITEMS[i], x, y, size, withA(ink, (on ? 1.f : 0.45f) * a4), 0, 0.35f);
            if (!on) continue;
            float l, r;
            p.inkExtent(F::JOSEFIN, ITEMS[i], size, 0.35f, l, r);
            const float x0 = x - p.width(F::JOSEFIN, ITEMS[i], size, 0.35f) * 0.5f;
            p.line(x0 + l, y + 12 * u, x0 + r, y + 12 * u, 1.6f * u, withA(ink, 0.9f * a4));
        }
    }
}

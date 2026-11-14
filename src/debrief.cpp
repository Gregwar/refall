// The debrief at the end of a Voyage run, in place of a message over the menu: the REFALL logo a little above the middle
// (white contours after a game over, over the frozen and darkened game; turning from its contours into its colours on
// the black for a stage cleared, as the demo video ends), the heading, the score and the lines, a small line of details.
// A key, after a moment (a key held from the game must not skip it), fades it out and the Voyage's screen fades in.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "logo.inc"
#include "menu.hpp"

namespace {
using F = Painter::Font;
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
// a calm ease, slow at both ends (the logo taking its colours)
float easeInOutSine(float t) { t = std::clamp(t, 0.f, 1.f); return 0.5f - 0.5f * std::cos(t * 3.14159265f); }

constexpr float INPUT_AFTER = 1.5f;  // keys are ignored this long (a key held from the game)
constexpr float LEAVE = 0.6f;        // the debrief fades to black...
constexpr float ENTER = 0.7f;        // ...then the Voyage's screen fades in
constexpr float COLOR_AT = 1.4f, COLOR_FOR = 2.5f; // a stage cleared: the logo takes its colours

// tools/logo.py's run-length code: a byte h, then h + 1 literal bytes when h < 128, else one byte repeated h - 126 times
std::vector<unsigned char> unpack(const uint8_t* p, size_t n, size_t want) {
    std::vector<unsigned char> out;
    out.reserve(want);
    for (size_t i = 0; i < n && out.size() < want;) {
        const int h = p[i++];
        if (h < 128) {
            for (int k = 0; k <= h && i < n; k++) out.push_back(p[i++]);
        } else if (i < n) {
            out.insert(out.end(), (size_t)(h - 126), p[i++]);
        }
    }
    out.resize(want, 0);
    return out;
}

// a separable gaussian blur of an N x N plane
std::vector<float> blur(const std::vector<float>& in, int N, float sigma) {
    const int r = (int)std::ceil(sigma * 3.f);
    std::vector<float> k(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; i++) sum += k[i + r] = std::exp(-0.5f * i * i / (sigma * sigma));
    for (float& v : k) v /= sum;
    std::vector<float> tmp(in.size()), out(in.size());
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float s = 0;
            for (int i = -r; i <= r; i++) s += k[i + r] * in[(size_t)y * N + std::clamp(x + i, 0, N - 1)];
            tmp[(size_t)y * N + x] = s;
        }
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float s = 0;
            for (int i = -r; i <= r; i++) s += k[i + r] * tmp[(size_t)std::clamp(y + i, 0, N - 1) * N + x];
            out[(size_t)y * N + x] = s;
        }
    return out;
}

GLuint texture(const std::vector<unsigned char>& rgba, int N) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}
} // namespace

void Menu::loadLogo() {
    if (logoColor_) return;
    const int N = LOGO_SIZE;
    const size_t px = (size_t)N * N;
    // the colour logo: its planes interleaved
    const std::vector<unsigned char> planes = unpack(LOGO_COLOR_RLE, sizeof(LOGO_COLOR_RLE), px * 4);
    std::vector<unsigned char> rgba(px * 4);
    for (size_t i = 0; i < px; i++)
        for (int c = 0; c < 4; c++) rgba[i * 4 + c] = planes[c * px + i];
    logoColor_ = texture(rgba, N);
    // the contours: white lines with a soft glow over a faint dark halo, as tools/demo.py's ending draws them
    const std::vector<unsigned char> l8 = unpack(LOGO_CONTOURS_RLE, sizeof(LOGO_CONTOURS_RLE), px);
    std::vector<float> line(px);
    for (size_t i = 0; i < px; i++) line[i] = l8[i] / 255.f;
    const std::vector<float> glow = blur(line, N, N * 0.008f), shade = blur(line, N, N * 0.02f);
    for (size_t i = 0; i < px; i++) {
        const float l = std::min(1.f, line[i] + 0.6f * glow[i]), s = std::min(1.f, shade[i] * 1.5f) * 0.3f;
        const float a = l + s * (1.f - l);
        rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = (unsigned char)(l * 255.f + 0.5f);
        rgba[i * 4 + 3] = (unsigned char)(a * 255.f + 0.5f);
    }
    logoLines_ = texture(rgba, N);
}

void Menu::openDebrief(const Debrief& d) {
    screen_ = DEBRIEF;
    debrief_ = d;
    debT_ = 0;
    leaveT_ = -1;
    debSel_ = 0;
    debRetry_ = false;
    msgT_ = 0;
    msg_.clear();
    loadLogo();
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
        if (choice) { // RETRY or MENU
            if (debT_ > INPUT_AFTER) {
                const int d = in.repeatSteps(A_UI_RIGHT) + in.repeatSteps(A_UI_DOWN) - in.repeatSteps(A_UI_LEFT) - in.repeatSteps(A_UI_UP);
                if (d) { debSel_ = std::clamp(debSel_ + d, 0, 1); lastInput_ = time_; }
                if (in.pressed(A_UI_OK) || in.pressed(A_UI_ENTER)) { leaveT_ = 0; debRetry_ = debSel_ == 0; }
                else if (in.pressed(A_UI_BACK)) { leaveT_ = 0; debRetry_ = false; }
            }
        } else {
            bool key = false;
            for (Action a : {A_LEFT, A_RIGHT, A_SOFT, A_HARD, A_CW, A_CCW, A_HOLD, A_PAUSE, A_UI_OK, A_UI_BACK})
                key |= in.pressed(a);
            if (key && debT_ > INPUT_AFTER) leaveT_ = 0;
        }
    } else if ((leaveT_ += dt) >= LEAVE && debRetry_) { // the run again
        out = debrief_.retry;
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

bool Menu::still() const { return screen_ == DEBRIEF && leaveT_ < 0 && debT_ > COLOR_AT + COLOR_FOR + 0.2f; }

void Menu::drawEnterFade() {
    if (enterT_ >= ENTER) return;
    paint_.rrect(0, 0, fw_, fh_, 0, hex(0x000000, 1.f - sstep(enterT_ / ENTER)), 0);
}

void Menu::drawDebrief(int w, int h) {
    Painter& p = paint_;
    p.begin(w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    const float u = u_, t = debT_, out = leaveT_ >= 0 ? 1.f - sstep(leaveT_ / LEAVE) : 1.f;
    // the frozen game, darkening (cover-fitted, should the window have changed)
    if (backdrop_) {
        const float k = std::max(w / (float)backW_, h / (float)backH_), bw = backW_ * k, bh = backH_ * k;
        const float d = sstep(t / 1.4f), b = (1.f - 0.8f * d) * out;
        p.image(backdrop_, (w - bw) * 0.5f, (h - bh) * 0.5f, bw, bh, Col{b, b, b, 1.f}, true);
        // a soft shade down the middle, under the logo and the words
        p.rrect(X(960 - 420), Y(120), 840 * u_, 900 * u_, 300 * u_, hex(0x000000, 0.55f * d * out), 260 * u_);
    }
    // the logo: its contours fade in; a stage cleared, they then turn into the coloured logo
    const float cx = X(960), cy = Y(392), S = 486 * u;
    const float turned = debrief_.clear ? easeInOutSine((t - COLOR_AT) / COLOR_FOR) : 0.f;
    // (the colours come over the contours, which fade a little later: the logo never dims between the two)
    const float lines = sstep(t / 1.2f) * (1.f - turned * turned) * out, color = turned * out;
    if (color > 0.f) { // a faint warm light behind it as it takes its colours
        p.setAdditive(true);
        p.glow(cx, cy, 300 * u, hex(0xe8c06a, 0.07f * color));
        p.setAdditive(false);
    }
    p.image(logoLines_, cx - S / 2, cy - S / 2, S, S, Col{1, 1, 1, lines});
    p.image(logoColor_, cx - S / 2, cy - S / 2, S, S, Col{1, 1, 1, color});
    // the words, one after the other
    auto at = [&](float t0) { return sstep((t - t0) / 0.9f) * out; };
    const Col ink = hex(0xffffff), dim = hex(0xe0e0f0, 0.65f), gold = hex(0xffd27a);
    p.text(F::JOSEFIN_LIGHT, debrief_.heading, cx, Y(748), 72 * u, withA(ink, at(0.5f)), 0, 0.16f);
    const float a2 = at(0.9f), colX = 190 * u;
    p.text(F::JOSEFIN, "SCORE", cx - colX, Y(812), 17 * u, withA(dim, dim.a * a2), 0, 0.35f);
    p.text(F::JOSEFIN, "LINES", cx + colX, Y(812), 17 * u, withA(dim, dim.a * a2), 0, 0.35f);
    p.text(F::JOSEFIN_LIGHT, std::to_string(debrief_.score), cx - colX, Y(874), 54 * u, withA(ink, a2), 0, 0.04f);
    p.text(F::JOSEFIN_LIGHT, std::to_string(debrief_.lines), cx + colX, Y(874), 54 * u, withA(ink, a2), 0, 0.04f);
    p.line(cx, Y(822), cx, Y(878), 1 * u, withA(ink, 0.18f * a2));
    const float a3 = at(1.3f);
    p.text(F::JOSEFIN, debrief_.detail, cx, Y(950), 17 * u, withA(dim, dim.a * a3), 0, 0.3f);
    if (!debrief_.note.empty())
        p.text(F::JOSEFIN, debrief_.note, cx, Y(990), 17 * u, withA(gold, 0.85f * (debrief_.clear ? at(COLOR_AT + 1.2f) : a3)), 0, 0.3f);
    if (debrief_.retry.kind != Choice::NONE) { // RETRY or MENU, the chosen one underlined
        static const char* ITEMS[2] = {"RETRY", "MENU"};
        const float a4 = sstep((t - INPUT_AFTER + 0.4f) / 0.5f) * out, sp = 170 * u, y = Y(1018), size = 26 * u;
        for (int i = 0; i < 2; i++) {
            const float x = cx + (i - 0.5f) * 2.f * sp;
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

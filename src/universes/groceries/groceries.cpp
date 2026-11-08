// GROCERIES: a cashier's checkout counter seen from above. The blocks are groceries on the belt; cleared rows are
// scanned and packed into bags, calmly.
//
// One counter, filling the frame behind the well and the HUD, seen straight down: the belt runs down the middle
// (the well: black rubber, its ribs running toward the scanner, a divider bar riding it now and then, between steel
// rails); a steel lane along its right rail leads down to the scanner's glass with its soft red laser lines and the
// upright scanner behind it; beside it the bag being packed, open in its rack, and along the counter's right edge
// the full bags set aside; on the left the till with its screen, the receipt printer and its receipt, the card
// terminal; a rack of sweets at the top right. The variant's light falls from above (a window's light through
// blinds by day, a pendant lamp's pool by night).
//
// The pieces are products seen from above, one per cell, each tetromino its own family: cartons of milk (their
// gable, I), cereal boxes lying on their backs (O), punnets of berries under their film (T), trays of apples (S),
// packs of tins (their lids, Z), boxes of pasta with a window (J), cartons of juice (their cap, L). A cleared row
// is lifted off the belt; its products glide one after the other to the steel lane, down it, across the scanner
// (a soft red flash, a beep's glow) and into the open bag, where they stack neatly, six on the bottom and four on
// top, seen from above. A full bag is set aside at the counter's edge and a new one is opened; when the edge is
// full, the bags are taken away and the edge fills again. Nothing grows beyond the counter: over the song, the
// receipt prints longer and curls off the printer down the counter, and the customer's things are set down one
// after the other (a shopping list, keys, a wallet, a coffee, sunglasses, a loyalty card, a folded tote).
//
// The song: calm is a slow belt under a soft, slightly cool light; peaks run the belt a little faster under a
// warmer light. A real drop (the HUD's section DROP, else the music's pace) stays gentle: a soft glow runs along the
// belt's rails, the scanner pulses on the beat and the receipt prints a burst. A Tetris glows along the rails and
// prints a little more. Slow motion dims the counter to evening; game over sweeps the stack off the counter.
//
// Layout contract: the camera is the layout's reference camera (straight on the board's plane, which is the
// counter's top seen from above).
#include "universes/groceries/groceries.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/groceries/shaders.hpp"
#include "universes/groceries/world.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace groc;

namespace {

// Figtree, SIL Open Font License, see OFL-Figtree.txt.
const unsigned char FONT_BOLD[] = {
#include "universes/groceries/Figtree-SemiBold.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/groceries/Figtree-Medium.ttf.inc"
};

namespace shaders = groceries_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 lin(unsigned v) { return ugl::toLinear(vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// ---- The variants (sRGB hex): the hour and the store's style.
struct VariantDef {
    const char* name;
    unsigned light; float lightK;
    unsigned amb; float ambK;
    unsigned ambLow; float ambLowK;
    unsigned counter, steel, belt, kraft, accent, accent2, paper, veil, cart, pendant;
    float pendantK, patch, wood, gloss;
    unsigned famA[7], famB[7], famC[7], fabric[4];
};
const unsigned FAM_A[7] = {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a};
const unsigned FAM_B[7] = {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2};
const unsigned FAM_C[7] = {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a};
const VariantDef VARIANTS[] = {
    // a bright morning: a white laminate counter, the window's light through the blinds, a fresh green
    {"MORNING", 0xfff2e0, 0.95f, 0xdfe6ee, 0.42f, 0x8a8478, 0.25f,
     0xe9e5dc, 0xc4c8cc, 0x2a2c30, 0xc49a6c, 0x2e9a6a, 0xf2c14e, 0xf8f6f0, 0x161c24, 0xc9ced4, 0xfff0dc,
     0.55f, 1.f, 0.f, 0.35f,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48}},
    // late at night: a brushed steel counter under cool tubes, a red accent
    {"LATE NIGHT", 0xe8f0ff, 0.7f, 0x9aa8bc, 0.32f, 0x3a4048, 0.2f,
     0x8e959c, 0xb0b6bc, 0x1c1d20, 0xb8946a, 0xe2453c, 0x3fb2e8, 0xeef0ee, 0x0b0f16, 0xb0b6bc, 0xdfeaff,
     0.75f, 0.f, 2.f, 0.5f,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48}},
    // an organic co-op in the golden afternoon: an oak counter, kraft and earthy packaging, sage
    {"CO-OP", 0xffe0b8, 1.0f, 0xeadcc6, 0.4f, 0x7a6048, 0.25f,
     0xc2a27e, 0xb8b0a0, 0x2c2a26, 0xb88c5a, 0x5f8a4a, 0xd9a93f, 0xf2ead8, 0x1a1612, 0xb8b0a0, 0xffd8a8,
     0.45f, 1.f, 1.f, 0.3f,
     {0x8fb4c9, 0xd9a93f, 0x6d4c7d, 0x8aab5b, 0xc4573a, 0x3f5f8a, 0xe08d45},
     {0xf1e8d6, 0xf6ead0, 0xefe3cc, 0xc4a070, 0xf2e8d8, 0xf0e8d8, 0xf6ead2},
     {0x2f4f68, 0xa8452e, 0x3a2440, 0x3f5f30, 0xc0bcb4, 0xe2c88a, 0x4f7f3a},
     {0x6f8a5a, 0xb8643a, 0x3f5a6a, 0xa89060}},
    // the blue hour: a navy counter in the evening's blue, a warm lamp's pool over it, amber
    {"BLUE HOUR", 0x9aa8d8, 0.4f, 0x5a6a98, 0.36f, 0x2a2830, 0.2f,
     0x34425e, 0xb0b6bc, 0x202226, 0xc49a6c, 0xf08a3a, 0x8fb4c9, 0xf4f0e6, 0x0e1220, 0xc0c6cc, 0xffc890,
     1.35f, 0.f, 0.f, 0.45f,
     {0x86c4e6, 0xf2c14e, 0x6a52a6, 0x74bc5c, 0xd84a3f, 0x3164b4, 0xf08a2a},
     {0xf6f2ea, 0xfff3d8, 0xf3ecdf, 0xc9a272, 0xf8f4ee, 0xf6f2ea, 0xfff4e2},
     {0x214f7a, 0xd8493c, 0x2c2452, 0x2f6b3a, 0xc9ccd1, 0xebd08e, 0x3f8f3a},
     {0x3f6f8f, 0x5f8f5a, 0xc25a3a, 0x2f3a48}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

GroceriesUniverse::Palette makePalette(int v) {
    const VariantDef& V = VARIANTS[v];
    GroceriesUniverse::Palette P;
    P.light = lin(V.light) * V.lightK;
    P.amb = lin(V.amb) * V.ambK;
    P.ambLow = lin(V.ambLow) * V.ambLowK;
    P.counter = lin(V.counter); P.counter2 = lin(V.pendant); P.steel = lin(V.steel); P.belt = lin(V.belt); P.kraft = lin(V.kraft);
    P.accent = lin(V.accent); P.accent2 = lin(V.accent2); P.paper = lin(V.paper); P.veil = lin(V.veil); P.cart = lin(V.cart);
    for (int i = 0; i < 7; i++) { P.famA[i] = lin(V.famA[i]); P.famB[i] = lin(V.famB[i]); P.famC[i] = lin(V.famC[i]); }
    for (int i = 0; i < 4; i++) P.fabric[i] = lin(V.fabric[i]);
    P.patch = V.patch; P.wood = V.wood; P.warmth = V.pendantK; P.gloss = V.gloss;
    return P;
}

void lerpPal(GroceriesUniverse::Palette& a, const GroceriesUniverse::Palette& b, float k) {
    vec3* va = &a.light;
    const vec3* vb = &b.light;
    const int n = (int)((&a.fabric[3] - &a.light) + 1);
    for (int i = 0; i < n; i++) va[i] = lerp(va[i], vb[i], k);
    a.patch = lerpf(a.patch, b.patch, k);
    a.warmth = lerpf(a.warmth, b.warmth, k);
    a.gloss = lerpf(a.gloss, b.gloss, k);
    a.wood = b.wood;
}

// Test hooks: ZEN_GROCERIES_LINES=N adds N lines, ZEN_GROW_RATE=R adds R lines a second.
int extraLines(double t) {
    static const int n = std::getenv("ZEN_GROCERIES_LINES") ? std::atoi(std::getenv("ZEN_GROCERIES_LINES")) : 0;
    static const double rate = std::getenv("ZEN_GROW_RATE") ? std::atof(std::getenv("ZEN_GROW_RATE")) : 0.0;
    return n + (int)(rate * t);
}

std::string ascii(const std::string& s) {
    static const char* LATIN = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 128) { o += (char)c; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) { o += '\x7f'; i++; continue; }
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = (unsigned char)s[i + 1];
            if (d >= 0x80 && d <= 0xBF) { o += LATIN[d - 0x80]; i++; continue; }
        }
        if ((c & 0xE0) == 0xC0) i += 1;
        else if ((c & 0xF0) == 0xE0) i += 2;
        else if ((c & 0xF8) == 0xF0) i += 3;
        o += '?';
    }
    return o;
}
int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}
float easeOut(float t) { t = saturate(t); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
float easeInOut(float t) { t = saturate(t); return t * t * (3.f - 2.f * t); }

constexpr float GLIDE_V = 6.5f;        // the products' pace on their way to the bag (units a second)
constexpr float CHUTE_GAP = 0.17f;     // seconds between two products entering the lane
constexpr float HOP_T = 0.5f;          // the hop into the bag
constexpr float LANE_END_X = BAG_X - 2.3f;
constexpr int EDGE_BAGS = 8;           // full bags the counter's edge holds before they are taken away

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void GroceriesUniverse::setupBoxVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 6; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Box), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
}

void GroceriesUniverse::setupBlobVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Blob), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glBindVertexArray(0);
}


void GroceriesUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] groceries font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 8;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB7 : ch;
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f.ok = true;
}


float GroceriesUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void GroceriesUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        const Glyph& g = f.g[glyphIndex((unsigned char)s[i])];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void GroceriesUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void GroceriesUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void GroceriesUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    // clean white lettering, like a shop's digital price displays, on a soft dark halo; the accent the store's color
    const vec3 ink(0.95f, 0.95f, 0.93f), halo = ugl::toSrgb(pal_.veil) * 0.8f;
    const vec3 accent = lerp(ugl::toSrgb(pal_.accent), vec3(1.f), 0.2f);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a fine ring, its charge in the accent, ticked
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.2f, 1.6f * s), th2 = std::max(2.5f, 5.f * s);
        auto seg = [&](float a0, float a1, float rr, float w, const vec3& col, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = rr - w * 0.5f, ro = rr + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro}, {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, r, th, ink, (g.bonusReady() ? 0.55f : 0.25f) * dimk);
        for (int i = 0; i < Game::BONUS_LINES; i++) {
            const float a = TAU * i / Game::BONUS_LINES;
            seg(a - 0.012f, a + 0.012f, r - th2, th2 * 0.9f, ink, 0.35f * dimk);
        }
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, r, th2, accent, 0.95f * dimk);
        }
        flushShapes();
    }
    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(bold_, upper(h.text), base * 0.72f, base * 0.2f, base * 0.06f);
            drawText(bold_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(bold_, upper(h.text), base * 0.8f, base * 0.24f, -base * 0.04f);
            drawText(bold_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(bold_, h.text, base * 1.1f, base * 0.0f, -base * 0.1f);
            drawText(bold_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(medium_, upper(h.text), base * 0.64f, base * 0.16f);
            drawText(medium_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(bold_, upper(h.text), base * 0.64f, base * 0.28f);
            drawText(bold_, q, ink, halo, saturate(alpha * 1.2f) * 0.62f);
            continue;
        case HUD_VALUE:
            place(bold_, h.text, base * 1.0f, base * 0.04f, -base * 0.04f);
            drawText(bold_, q, ink, halo, alpha * 0.96f);
            continue;
        case HUD_NOTICE:
            place(bold_, upper(h.text), base * 0.9f, base * 0.16f);
            drawText(bold_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(bold_, h.text, base * 0.84f, base * 0.08f);
            drawText(bold_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(bold_, upper(h.text), base * 1.0f, base * 0.26f, -base * 0.1f);
            drawText(bold_, q, ink, halo, alpha, 0.65f);
            continue;
        case HUD_TOAST_SUB:
            place(bold_, h.text, base * 0.95f, base * 0.08f);
            drawText(bold_, q, accent, halo, alpha * 0.9f, 0.65f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(bold_, upper(h.text), base * 0.9f, base * 0.22f);
                drawText(bold_, q, ink, halo, alpha, 0.7f);
            } else {
                place(medium_, h.text, base * 0.95f, base * 0.02f);
                drawText(medium_, q, h.accent ? accent : ink, halo, alpha, 0.7f);
            }
            continue;
        default:
            place(medium_, h.text, base * 0.78f, base * 0.04f);
            drawText(medium_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}


void GroceriesUniverse::line(std::vector<Line>& out, const vec3& a, const vec3& b, float wpx, const vec3& col, float alpha) {
    Line l;
    l.a[0] = a.x; l.a[1] = a.y; l.a[2] = a.z; l.a[3] = wpx;
    l.b[0] = b.x; l.b[1] = b.y; l.b[2] = b.z; l.b[3] = alpha;
    l.c[0] = col.x; l.c[1] = col.y; l.c[2] = col.z; l.c[3] = 0.f;
    out.push_back(l);
}

void GroceriesUniverse::sprite(std::vector<Sprite>& out, const vec3& p, float r, const vec3& col, float k) {
    if (k <= 0.002f) return;
    Sprite s;
    s.p[0] = p.x; s.p[1] = p.y; s.p[2] = p.z; s.p[3] = r;
    s.c[0] = col.x; s.c[1] = col.y; s.c[2] = col.z; s.c[3] = k;
    out.push_back(s);
}



bool GroceriesUniverse::init(int width, int height) {
    using shaders::withCommon;
    progBox_ = ugl::program(shaders::BOX_VS, withCommon(shaders::BOX_FS).c_str(), "groceries solids");
    progBlob_ = ugl::program(shaders::BLOB_VS, shaders::BLOB_FS, "groceries shadows");
    progSprite_ = ugl::program(shaders::SPRITE_VS, shaders::SPRITE_FS, "groceries glows");
    progLine_ = ugl::program(shaders::LINE_VS, shaders::LINE_FS, "groceries lines");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "groceries veil");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "groceries down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "groceries blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "groceries final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "groceries shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "groceries text");
    {
        std::vector<float> v;
        const int faces[6][2] = {{0, 1}, {0, -1}, {1, 1}, {1, -1}, {2, 1}, {2, -1}};
        for (int f = 0; f < 6; f++) {
            const int ax = faces[f][0];
            const float sg = (float)faces[f][1];
            const int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 6; k++) {
                float p[3], n[3] = {0, 0, 0};
                p[ax] = sg;
                p[a1] = corners[k][0];
                p[a2] = corners[k][1] * sg;
                n[ax] = sg;
                v.insert(v.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
            }
        }
        glGenBuffers(1, &cubeVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    }
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    setupBoxVao(staticVao_, staticVbo_);
    setupBoxVao(boxVao_, boxVbo_);
    setupBlobVao(blobVao_, blobVbo_);
    setupBlobVao(sblobVao_, sblobVbo_);
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    {
        const float strip[8] = {0, -1, 1, -1, 0, 1, 1, 1};
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &stripVbo_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, stripVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(strip), strip, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Line), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    }
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(bold_, FONT_BOLD);
    buildFont(medium_, FONT_MEDIUM);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void GroceriesUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_, 1.f, 0.f, 0.5f, 200.f);
    vp_ = cam.vp;
    eye_ = cam.eye;
    {
        const vec2 a = layout::project(vp_, vec3(-10.f, -10.f, 0.f), width_, height_);
        const vec2 b = layout::project(vp_, vec3(10.f, 10.f, 0.f), width_, height_);
        const float sx = (b.x - a.x) / 20.f, sy = (a.y - b.y) / 20.f;
        pxPerUnit_ = sx;
        const float xl = -10.f + (0.f - a.x) / sx, xr = -10.f + (width_ - a.x) / sx;
        const float yb = -10.f + (a.y - height_) / sy, yt = -10.f + (a.y - 0.f) / sy;
        viewC_ = {(xl + xr) * 0.5f, (yb + yt) * 0.5f};
        viewH_ = {(xr - xl) * 0.5f, (yt - yb) * 0.5f};
    }
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GroceriesUniverse::~GroceriesUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progBox_, progBlob_, progSprite_, progLine_, progVeil_, progDown_, progBlur_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {staticVao_, boxVao_, blobVao_, sblobVao_, spriteVao_, lineVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, quadVbo_, stripVbo_, staticVbo_, boxVbo_, blobVbo_, sblobVbo_, spriteVbo_, lineVbo_, shapeVbo_, textVbo_})
        glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    for (Font* f : {&bold_, &medium_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void GroceriesUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 1;
        else if (t.hasSong && t.bassWeight < 0.45f && t.airWeight > 0.5f) v = 2;
        else if (t.hasSong && !t.minor && t.brightness > 0.55f) v = 0;
        else if (t.hasSong && t.minor) v = 3;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    palT_ = makePalette(v);
    const bool instant = !palSet_ || seconds <= 0.1f;
    if (instant) { pal_ = palT_; palSet_ = true; }
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x6E0Cull);
    counterDirty_ = true;
}

void GroceriesUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    (void)impact;
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

void GroceriesUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

void GroceriesUniverse::refresh(float strength) { refreshK_ = std::max(refreshK_, strength); }

TransitionHints GroceriesUniverse::hints() const {
    TransitionHints h;
    h.background = pal_.counter * (pal_.amb + pal_.light * 0.8f);
    h.light = ugl::luma(h.background) > 0.3f;
    h.ink = h.light ? pal_.veil : vec3(0.92f);
    h.accent = pal_.accent;
    h.styles = TS_REBUILD | TS_TILES | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void GroceriesUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock:
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) seeds_[c.y][c.x] = rng_.uniform();
        break;
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        if (ev.count >= 4) tetrisT_ = 0.f;
        // the products go to the lane one after the other: the lowest row first, each row from the right
        std::vector<GameEvent::CellInfo> cells(ev.cells.begin(), ev.cells.end());
        std::sort(cells.begin(), cells.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) {
            return a.y != b.y ? a.y > b.y : a.x > b.x;
        });
        const float now = (float)time_;
        for (auto& c : cells) {
            if (c.x < 0 || c.x >= Game::W || c.y < Game::HIDDEN || c.y >= Game::H) continue;
            Flight fl;
            fl.p0 = layout::cellPos((float)c.x, (float)c.y);
            fl.cur = fl.p0;
            fl.yRow = fl.p0.y;
            fl.type = c.type;
            fl.kind = 0;
            fl.seed = seeds_[c.y][c.x];
            fl.t = now;
            const float dist = CHUTE_X - fl.p0.x;
            fl.tChute = std::max(nextChute_, now + 0.3f + dist / GLIDE_V);
            nextChute_ = fl.tChute + CHUTE_GAP;
            fl.spin = rng_.range(-1.f, 1.f);
            flights_.push_back(fl);
        }
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            float ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) ns[dst][x] = seeds_[y][x];
                dst--;
            }
            std::memcpy(seeds_, ns, sizeof(seeds_));
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack is swept off the counter, product after product, from the top
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Flight fl;
            fl.p0 = fl.cur = layout::cellPos((float)c.x, (float)c.y);
            fl.type = c.type;
            fl.kind = 1;
            fl.seed = seeds_[c.y][c.x];
            fl.t = 0.f;
            fl.tChute = 0.06f * (c.y - top) + 0.2f * hashf((uint64_t)(c.x * 17 + c.y * 3));
            fl.vel = vec3(rng_.range(-1.5f, 1.5f), rng_.range(-9.f, -5.f), rng_.range(1.f, 3.f));
            fl.spin = rng_.range(-3.f, 3.f);
            flights_.push_back(fl);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The board
// =====================================================================================================

void GroceriesUniverse::product(std::vector<Box>& out, const vec3& c, float half, int type, float seed, float flash, float rot) {
    type = std::clamp(type, 0, 6);
    const float ca = std::cos(rot), sa = std::sin(rot);
    obox(out, c, vec3(ca, sa, 0.f) * half, vec3(-sa, ca, 0.f) * half, vec3(0.f, 0.f, half), vec3(1.f), M_PRODUCT, (float)type, flash, 0, seed);
}

void GroceriesUniverse::buildBoard(const Game& g) {
    const float half = CELL;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            product(cells_, layout::cellPos((float)x, y - g.rowOffset(y)), half, c.type, seeds_[y][x]);
        }
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            if (cells[i][1] + dy < Game::HIDDEN - 0.5f) continue;
            product(cells_, layout::cellPos(cells[i][0] + dx, cells[i][1] + dy), half, p.type, 0.13f * i + 0.07f * p.type);
        }
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto has = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 col = lerp(pal_.famA[std::clamp(p.type, 0, 6)], vec3(1.f), 0.5f);
            const float wpx = std::max(1.f, 1.5f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.46f, z = -0.4f;
                if (!has(c[0], c[1] - 1)) line(hair_, vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.75f);
                if (!has(c[0], c[1] + 1)) line(hair_, vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z), wpx, col, 0.75f);
                if (!has(c[0] - 1, c[1])) line(hair_, vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z), wpx, col, 0.75f);
                if (!has(c[0] + 1, c[1])) line(hair_, vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.75f);
            }
        }
    }
    // the previews: the held product in a basket, the next ones set down on the counter
    auto preview = [&](int type, vec2 center, float sc, float dim) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (int i = 0; i < 4; i++) {
            const float ox = cells[i][0] - (minx + maxx) * 0.5f, oy = cells[i][1] - (miny + maxy) * 0.5f;
            const vec3 pc(center.x + ox * sc, center.y - oy * sc, Z_TOP + half * sc + 0.02f);
            box(cells_, pc, vec3(half * sc), vec3(dim), M_PRODUCT, (float)type, 0, 0, 0.21f * i);
            blob(blobs_, vec3(pc.x + 0.12f, pc.y - 0.12f, Z_TOP + 0.01f), half * sc + 0.2f, half * sc + 0.2f, 0.35f, 0.6f);
        }
    };
    if (!g.over()) {
        const float hs = layout::previewScale(0);
        const vec3 hc = layout::HOLD_CENTER;
        preview(g.holdType(), vec2{hc.x, hc.y}, hs, g.holdUsed() ? 0.5f : 1.f);
        // the basket around the held product (seen from above: its wire walls, its floor, its handles)
        box(cells_, vec3(hc.x, hc.y, Z_TOP + 0.75f), vec3(1.75f, 1.05f, 0.75f), pal_.cart * 0.85f, M_WIRE);
        for (int s = -1; s <= 1; s += 2)
            box(cells_, vec3(hc.x, hc.y + s * 0.55f, Z_TOP + 1.55f), vec3(1.2f, 0.07f, 0.07f), pal_.accent, M_PAINT, 0.3f);
        blob(blobs_, vec3(hc.x + 0.3f, hc.y - 0.3f, Z_TOP + 0.01f), 2.f, 1.3f, 0.3f, 0.6f);
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
        }
    }
}

// =====================================================================================================
// Drawing
// =====================================================================================================

void GroceriesUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    // ZEN_GPUTIME: the GPU time of a frame, printed every 240 frames
    static const bool gpuTime = std::getenv("ZEN_GPUTIME") != nullptr;
    static GLuint gpuQ = 0;
    if (gpuTime && !gpuQ) glGenQueries(1, &gpuQ);
    if (gpuQ) glBeginQuery(GL_TIME_ELAPSED, gpuQ);
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    lerpPal(pal_, palT_, 1.f - std::exp(-dt * 1.2f));
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    if (!f.paused) time_ += (double)dt;
    worldTime_ += wdt;
    const float now = (float)time_;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && overT_ < 0.f && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    if (isDrop && !wasDrop_) burst_ += 1.5f;
    wasDrop_ = isDrop;
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.2f : 0.5f, dt);
    swell_ = approach(swell_, 0.f, 0.4f, dt);
    refreshK_ = approach(refreshK_, 0.f, 0.6f, dt);
    tetrisT_ += dt;
    const float kick = kickIn_;
    kickIn_ = 0.f;
    kickGlow_ = std::max(approach(kickGlow_, 0.f, 5.f, dt), kick);
    bool newBeat = false;
    if (music.hasSong) {
        const double b = std::floor(music.beat);
        if (b != lastBeat_) { newBeat = lastBeat_ >= 0.0; lastBeat_ = b; }
    } else {
        beatAcc_ += wdt * 2.f;
        if (beatAcc_ > 1.f) { beatAcc_ -= 1.f; newBeat = true; }
    }
    beatFlash_ = approach(beatFlash_, 0.f, 4.f, dt);
    if (newBeat) beatFlash_ = 1.f;
    // the belt's pace: slow when calm, a little faster at a peak
    const float beltT = (0.45f + 0.35f * wm + 0.4f * wp + 0.25f * dropAmt_) * (1.f - 0.6f * slow_);
    beltK_ = approach(beltK_, beltT, 0.8f, dt);
    const float beltV = 1.3f * beltK_;
    beltPhase_ += wdt * beltV;

    // ---- The lines: the receipt, the customer's things; lines added by the test hooks bring their bags at once.
    const int extra = extraLines(time_);
    const int lines = game.lines() + extra;
    if (linesShown_ >= 0 && lines < lines_ && !game.over()) {
        // a new game: a new customer
        full_.clear();
        bagFill_ = 0;
        bagTypes_.clear();
        receipt_ = 0.f;
        burst_ = 0.f;
        gL_ = 0.f;
    }
    if (lastExtra_ < 0) lastExtra_ = extra;
    for (int i = lastExtra_; i < extra && linesShown_ >= 0 && (int)full_.size() < 2 * EDGE_BAGS; i++) {
        Full b;
        b.seed = rng_.uniform();
        vec3 fab;
        b.style = fabricStyle(b.seed, fab);
        for (int k = 0; k < 10; k++) b.types.push_back(rng_.irange(0, 6));
        b.t = 0.f;
        b.from = vec3(BAG_X, BAG_Y, Z_TOP);
        b.slot = (int)full_.size();
        full_.push_back(b);
    }
    lastExtra_ = extra;
    lines_ = lines;
    if (linesShown_ < 0) {
        gL_ = (float)lines;
        receipt_ = 1.f + 0.3f * lines;
        // the bags already packed wait at the counter's edge
        for (int i = 0; i < lines % EDGE_BAGS; i++) {
            Full b;
            b.seed = rng_.uniform();
            vec3 fab;
            b.style = fabricStyle(b.seed, fab);
            for (int k = 0; k < 10; k++) b.types.push_back(rng_.irange(0, 6));
            b.slot = i;
            b.t = 1.f;
            full_.push_back(b);
        }
    }
    linesShown_ = lines;
    gL_ = approach(gL_, (float)lines, 2.f, dt);
    if (tetrisT_ < dt * 1.5f) burst_ += 1.2f;
    if (isDrop) burst_ += dt * 0.12f;
    const float receiptTarget = 1.f + 0.3f * lines + burst_;
    const bool printing = receipt_ < receiptTarget - 0.01f;
    receipt_ = std::min(receiptTarget, receipt_ + dt * 1.1f);
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The full bags at the counter's edge: set aside, then taken away when the edge is full.
    bagPop_ = approach(bagPop_, 1.f, 5.f, dt);
    for (Full& b : full_) b.t = std::min(1.f, b.t + dt / 1.0f);
    if (sweepT_ < 0.f && (int)full_.size() >= EDGE_BAGS) {
        bool settled = true;
        for (int i = 0; i < EDGE_BAGS; i++) settled = settled && full_[i].t >= 1.f;
        if (settled) sweepT_ = 0.f;
    }
    if (sweepT_ >= 0.f) {
        sweepT_ += wdt;
        if (sweepT_ > 4.f) {
            full_.erase(full_.begin(), full_.begin() + std::min((int)full_.size(), EDGE_BAGS));
            for (size_t i = 0; i < full_.size(); i++) {
                // the bags left on the second layer settle down into the free places
                full_[i].from = shelfPos(full_[i].slot);
                full_[i].slot = (int)i;
                full_[i].t = 0.f;
            }
            sweepT_ = -1.f;
        }
    }
    if (counterDirty_ || builtVariant_ != variant_) {
        buildCounter();
        glBindBuffer(GL_ARRAY_BUFFER, staticVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(static_.size() * sizeof(Box)), static_.data(), GL_STATIC_DRAW);
        staticCount_ = static_.size();
        glBindBuffer(GL_ARRAY_BUFFER, sblobVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(staticBlobs_.size() * sizeof(Blob)), staticBlobs_.data(), GL_STATIC_DRAW);
        staticBlobCount_ = staticBlobs_.size();
        builtVariant_ = variant_;
        counterDirty_ = false;
    }

    // ---- The cleared products on their way.
    boxes_.clear();
    cells_.clear();
    blobs_.clear();
    sprites_.clear();
    hair_.clear();
    beep_ = approach(beep_, 0.f, 7.f, dt);
    int hopping = 0;
    for (const Flight& fl : flights_)
        if (fl.kind == 0 && fl.slot >= 0 && !fl.landed) hopping++;
    for (Flight& fl : flights_) {
        if (fl.kind == 1) {
            fl.t += dt;
            if (fl.t > fl.tChute) {
                fl.vel.z -= 22.f * dt;
                fl.vel.y -= 14.f * dt;
                fl.cur = fl.cur + fl.vel * dt;
                if (fl.cur.z < 0.f) { fl.cur.z = 0.f; fl.vel.z = std::fabs(fl.vel.z) * 0.3f; }
            }
            product(cells_, fl.cur, CELL, fl.type, fl.seed, 0.f, fl.t > fl.tChute ? (fl.t - fl.tChute) * fl.spin : 0.f);
            continue;
        }
        // lifted off the belt, waiting its turn; along its row to the lane; down the lane; across the scanner; a hop
        const float lift = smoothstepf(fl.t, fl.t + 0.3f, now);
        const float a1 = std::max(0.f, CHUTE_X - fl.p0.x), a2 = std::fabs(fl.yRow - SCAN_Y), a3 = LANE_END_X - CHUTE_X;
        const float tStart = fl.tChute - a1 / GLIDE_V;
        const float s = std::max(0.f, now - tStart) * GLIDE_V;
        const float zLift = 0.95f, zLane = Z_TOP + 0.06f + CELL * 0.8f;
        float scale = lerpf(1.f, 0.8f, lift);
        vec3 p;
        float rot = 0.f;
        if (s < a1) {
            p = vec3(fl.p0.x + s, fl.yRow, lerpf(0.f, zLift, lift));
        } else if (s < a1 + a2) {
            const float u = s - a1;
            p = vec3(CHUTE_X, fl.yRow + (SCAN_Y > fl.yRow ? u : -u), lerpf(zLift, zLane, smoothstepf(0.f, 1.2f, u)));
        } else if (s < a1 + a2 + a3) {
            const float u = s - a1 - a2;
            p = vec3(CHUTE_X + u, SCAN_Y, zLane);
            if (!fl.scanned && p.x >= SCAN_X) { fl.scanned = true; beep_ = 1.f; }
            fl.flash = std::exp(-(p.x - SCAN_X) * (p.x - SCAN_X) * 2.5f);
        } else {
            if (fl.slot < 0) { fl.slot = (bagFill_ + hopping) % 10; hopping++; }
            const float th = tStart + (a1 + a2 + a3) / GLIDE_V;
            const float u = saturate((now - th) / HOP_T);
            const vec3 from(LANE_END_X, SCAN_Y, zLane);
            const vec3 to = vec3(BAG_X, BAG_Y, Z_TOP) + slotPos(fl.slot);
            const float e = easeInOut(u);
            p = lerp(from, to, e);
            p.z += std::sin(u * PI) * (BAG_H + 1.6f - 0.5f * (from.z + to.z));
            scale = lerpf(0.8f, PACK, e);
            rot = (hashf((uint64_t)(fl.seed * 3e5f) + fl.slot) - 0.5f) * 0.12f * e;
            fl.flash = 0.f;
            if (u >= 1.f && !fl.landed) {
                fl.landed = true;
                bagFill_++;
                bagTypes_.push_back(fl.type);
                if (bagFill_ >= 10) {
                    Full b;
                    b.style = bagStyle_;
                    b.seed = bagSeed_;
                    b.types = bagTypes_;
                    b.t = 0.f;
                    b.from = vec3(BAG_X, BAG_Y, Z_TOP);
                    b.slot = (int)full_.size();
                    full_.push_back(b);
                    bagFill_ = 0;
                    bagTypes_.clear();
                    bagSeed_ = rng_.uniform();
                    vec3 fab;
                    bagStyle_ = fabricStyle(bagSeed_, fab);
                    bagPop_ = 0.f;
                }
            }
        }
        fl.cur = p;
        if (!fl.landed) {
            product(cells_, p, CELL * scale, fl.type, fl.seed, fl.flash, rot);
            if (p.z < 0.2f && s >= a1)
                blob(blobs_, vec3(p.x + 0.1f, p.y - 0.1f, Z_TOP + 0.01f), CELL * scale + 0.2f, CELL * scale + 0.2f, 0.3f, 0.6f);
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(),
                                  [&](const Flight& fl) { return (fl.kind == 0 && fl.landed) || (fl.kind == 1 && (fl.t > fl.tChute + 3.f || overT_ < 0.f)); }),
                   flights_.end());
    if (nextChute_ < now) nextChute_ = now;

    // ---- The divider bar riding the belt now and then.
    if (divider_ < -50.f) {
        dividerWait_ -= wdt;
        if (dividerWait_ < 0.f) { divider_ = 12.8f; dividerWait_ = rng_.range(14.f, 26.f); }
    } else {
        divider_ -= wdt * beltV;
        if (divider_ < -10.3f) divider_ = -99.f;
    }

    // ---- The dynamic counter: the bag being packed, the full bags, the receipt, the customer's things.
    {
        vec3 fab;
        fabricStyle(bagSeed_, fab);
        bagTypesFor_ = &bagTypes_;
        buildBag(boxes_, vec3(BAG_X, BAG_Y, Z_TOP), 0.75f + 0.25f * easeOut(bagPop_), bagStyle_, fab, bagFill_, bagSeed_);
        bagTypesFor_ = nullptr;
        for (const Full& b : full_) {
            vec3 bf;
            fabricStyle(b.seed, bf);
            const vec3 to = shelfPos(b.slot);
            const float e = easeInOut(b.t);
            vec3 p = lerp(b.from, to, e);
            p.z += std::sin(b.t * PI) * 1.2f;
            if (sweepT_ >= 0.f && b.slot < EDGE_BAGS) p.x += 10.f * easeInOut(std::max(0.f, sweepT_ - 0.2f * (b.slot / 2)) / 2.4f);
            bagTypesFor_ = &b.types;
            buildBag(boxes_, p, 1.f, b.style, bf, 10, b.seed, (hashf((uint64_t)(b.seed * 1e6f)) - 0.5f) * 0.12f * e);
            bagTypesFor_ = nullptr;
        }
    }
    buildReceipt(receipt_, now);
    buildThings(gL_);
    if (divider_ > -50.f) {
        box(boxes_, vec3(0.f, divider_, Z_TOP + 0.2f), vec3(4.9f, 0.32f, 0.2f), vec3(0.16f, 0.17f, 0.18f), M_PAINT, 0.4f);
        box(boxes_, vec3(0.f, divider_, Z_TOP + 0.41f), vec3(4.6f, 0.12f, 0.01f), pal_.accent, M_PAINT, 0.2f);
    }
    buildBoard(game);

    // ---- Lights.
    const Palette& P = pal_;
    float groupK[16] = {};
    const float pulse = beatFlash_ * beatFlash_;
    const float tet = tetrisT_ < 3.f ? std::exp(-tetrisT_ * 1.2f) : 0.f;
    groupK[G_SCAN] = 0.55f + 1.2f * beep_ + dropAmt_ * (0.3f + 1.1f * pulse) + 0.2f * kickGlow_ * wp;
    groupK[G_RAIL] = dropAmt_ * (0.5f + 1.2f * pulse) + 1.6f * tet;
    groupK[G_TILL] = 0.85f;
    groupK[G_LED] = printing ? (std::fmod(now * 6.f, 1.f) < 0.5f ? 1.6f : 0.3f) : 0.8f;
    sprite(sprites_, vec3(SCAN_X, SCAN_Y, Z_TOP + 0.3f), 2.6f, vec3(1.f, 0.14f, 0.08f), 0.12f + 0.7f * beep_ + 0.5f * dropAmt_ * pulse);
    if (groupK[G_RAIL] > 0.01f)
        for (int s = -1; s <= 1; s += 2)
            for (int k = 0; k < 6; k++) {
                // the soft glow along the rails, running down with the beat
                const float y = 11.f - std::fmod(k * 4.f + beltPhase_ * 3.f, 24.f);
                sprite(sprites_, vec3(s * 5.18f, y, Z_TOP + 0.5f), 2.2f, P.accent * 0.5f + vec3(0.5f, 0.42f, 0.3f), 0.45f * groupK[G_RAIL]);
            }
    pointP_.clear();
    pointC_.clear();
    // the light from above: the variant's pendant (a soft pool), the scanner's red
    const float lightLv = (0.9f + 0.05f * wm + 0.1f * wp + 0.06f * dropAmt_ + 0.06f * swell_) * (1.f - 0.45f * slow_);
    const vec3 warm = lerp(vec3(0.94f, 0.98f, 1.04f), vec3(1.f), wm) * lerp(vec3(1.f), vec3(1.07f, 0.98f, 0.86f), 0.5f * wp + 0.3f * dropAmt_);
    pointP_.push_back(vec4(vec3(-1.f, 1.5f, 30.f), 75.f));
    pointC_.push_back(vec4(P.counter2 * warm, P.warmth * lightLv));
    pointP_.push_back(vec4(vec3(SCAN_X, SCAN_Y, Z_TOP + 1.2f), 4.5f));
    pointC_.push_back(vec4(vec3(1.f, 0.15f, 0.1f), 0.3f + 1.6f * beep_ + 0.8f * dropAmt_ * pulse));

    // ---- Uniforms.
    auto lighting = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set1f(p, "uTime", worldTime_);
        set1f(p, "uSat", 1.f + 0.04f * wp);
        set3f(p, "uAmbTop", P.amb * lightLv * warm);
        set3f(p, "uAmbBot", P.ambLow * lightLv);
        set3f(p, "uKeyDir", normalize(vec3(-0.35f, 0.4f, 0.85f)));
        set3f(p, "uKeyCol", P.light * lightLv * warm);
        const int n = (int)pointP_.size();
        set1i(p, "uNL", n);
        glUniform4fv(ugl::U(p, "uLP"), n, &pointP_[0].x);
        glUniform4fv(ugl::U(p, "uLC"), n, &pointC_[0].x);
        glUniform1fv(ugl::U(p, "uGroupK"), 16, groupK);
        glUniform3fv(ugl::U(p, "uFA"), 7, &P.famA[0].x);
        glUniform3fv(ugl::U(p, "uFB"), 7, &P.famB[0].x);
        glUniform3fv(ugl::U(p, "uFC"), 7, &P.famC[0].x);
        set3f(p, "uKraft", P.kraft);
        set3f(p, "uAccent", P.accent);
        set1f(p, "uFloorZ", Z_TOP);
        set1f(p, "uBeltPhase", beltPhase_);
        set1f(p, "uPatch", P.patch * (1.f - 0.6f * slow_));
        set1f(p, "uGloss", P.gloss);
        set1f(p, "uStackA", 0.f);
        setMat(p, "uVP", vp_);
    };
    auto drawBoxes = [&](GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count) {
        if (!count) return;
        glBindVertexArray(vao);
        if (data) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Box)), data->data(), GL_STREAM_DRAW);
        }
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)count);
    };
    auto drawBlobs = [&](GLuint vao, GLuint vbo, const std::vector<Blob>* data, size_t count) {
        if (!count) return;
        glUseProgram(progBlob_);
        setMat(progBlob_, "uVP", vp_);
        glBindVertexArray(vao);
        if (data) {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Blob)), data->data(), GL_STREAM_DRAW);
        }
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
    };

    // ---- The counter (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(progBox_);
    lighting(progBox_);
    drawBoxes(staticVao_, staticVbo_, nullptr, staticCount_);
    drawBoxes(boxVao_, boxVbo_, &boxes_, boxes_.size());
    // soft contact shadows on the counter
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.f, -4.f);
    drawBlobs(sblobVao_, sblobVbo_, nullptr, staticBlobCount_);
    drawBlobs(blobVao_, blobVbo_, &blobs_, blobs_.size());
    glDisable(GL_POLYGON_OFFSET_FILL);
    // the veil behind the well and the HUD's columns (under the products)
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progVeil_, "uVeil", P.veil);
    set3f(progVeil_, "uRail", vec3(0.f));
    set1f(progVeil_, "uWellA", 0.22f);
    set1f(progVeil_, "uColA", 0.5f);
    set1f(progVeil_, "uRailA", 0.f);
    set1f(progVeil_, "uPx", pxPerUnit_);
    set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], -5.6f, layout::LEFT_COLUMN[3]);
    set4f(progVeil_, "uColR", 7.1f, layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // the products (the stack's alpha, for the bridge), the hold's basket
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glUseProgram(progBox_);
    set1f(progBox_, "uStackA", 1.f);
    drawBoxes(boxVao_, boxVbo_, &cells_, cells_.size());
    set1f(progBox_, "uStackA", 0.f);
    // glows, the ghost
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE);
    if (!sprites_.empty()) {
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        set1f(progSprite_, "uSoft", 0.8f);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!hair_.empty()) {
        glUseProgram(progLine_);
        setMat(progLine_, "uVP", vp_);
        set2f(progLine_, "uRes", (float)width_, (float)height_);
        set1f(progLine_, "uAdd", 0.f);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(hair_.size() * sizeof(Line)), hair_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)hair_.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---- Resolve, the glow, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 0.7f);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float st = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
        ugl::drawFullscreen();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uBloomK", (0.45f + 0.15f * dropAmt_) * (1.f + 0.3f * slow_));
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uHalo", 0.f);
    set1f(progFinal_, "uSatF", 1.f + 0.03f * wp);
    set1f(progFinal_, "uSlow", slow_ * 0.5f);
    set3f(progFinal_, "uSlowTint", vec3(0.8f, 0.88f, 1.06f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
    if (gpuQ) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
        static double acc = 0.0;
        static int n = 0;
        acc += ns * 1e-6;
        n++;
        if (n == 240) {
            std::fprintf(stderr, "[groceries] gpu %.2f ms/frame (lines %d)\n", acc / n, lines_);
            acc = 0.0;
            n = 0;
        }
    }
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_groceries() {
    static const UniverseEntry e{
        "GROCERIES", "a checkout counter seen from above: the pieces are groceries on the belt, cleared rows are scanned and packed into bags",
        [] { return std::unique_ptr<Universe>(new GroceriesUniverse()); },
        // A calm, everyday scene: fits most songs, the gentler ones a little better.
        [](const SongLook& l) { return 0.85f + 0.15f * (1.f - l.density) + 0.1f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

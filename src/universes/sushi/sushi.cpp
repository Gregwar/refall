// SUSHI: the pieces are maki on little plates, and the cleared rows feed kawaii cats sitting at the counter.
//
// One world: a small kaiten sushi bar in soft pastel colors, drawn flat with rounded shapes and thick soft outlines.
// The well is a lacquered bento box standing on the counter; a conveyor belt runs along the counter behind it,
// carrying dishes. Each piece is a glazed plate in its own color (its cells merged into one rounded dish) with one
// maki per cell, seen from above: salmon, tuna, cucumber, tamago, avocado, crab, and an inside-out roll with sesame.
// The falling piece is exactly a locked one. Behind the counter, on both sides of the box, sit round cats (white,
// calico, black with white socks, ginger and grey tabbies, a cream colorpoint), each with a collar and a little bell.
// A cleared row's maki slide off their plates out of the box, the left half to the left and the right half to the
// right (out of the row before the stack settles), then hop in arcs to the cats, who open their mouths, catch them
// and chew, eyes squeezed shut with joy, a heart rising, a little "nom". The more lines, the more cats pop up from
// behind the counter (a Tetris is a feast and brings two more). Lanterns hang on cords on both sides, a noren hangs
// in the upper right, sakura petals drift down behind the box.
//
// The song: calm parts are a quiet shop (the belt still, the cats dozing, one lantern glowing); a chorus starts the
// belt and the cats bob their heads on the beat grid (every two beats), the noren and lanterns sway; a peak lights
// every lantern, the petals thicken, the cats bounce and the maneki-neko among them wave. The drop is a festival:
// eased in over a beat or two, every cat cheers with both paws up, the lanterns swell, a shower of petals, the belt
// runs faster; it settles back over the following bars. Slow motion is dusk at the counter (lanterns warm, the
// cats sleepy); game over feeds the whole stack to the cats, from the top down.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/sushi/sushi.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/sushi/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// M PLUS Rounded 1c (Bold: labels, numbers, the title; Medium: the small lines), SIL Open Font License, see
// OFL-MPLUSRounded1c.txt. Subset to ASCII, the middle dot and four kana.
const unsigned char FONT_BOLD[] = {
#include "universes/sushi/MPLUSRounded1c-Bold.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/sushi/MPLUSRounded1c-Medium.ttf.inc"
};

namespace shaders = sushi_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A shop (sRGB): the wall (top, bottom), the counter's wood (light, dark), the belt, the box's lacquer and rim, the
// HUD ink, an accent, the outlines, the petals, the lanterns' two papers, the noren and its lettering, the warm tint of
// a peak; the seven plates (I O T S Z J L); night (lanterns glow more), petal density.
struct VariantDef {
    const char* name;
    unsigned wallTop, wallBot, wood, woodDark, belt, lacquer, rim, ink, accent, line, petal, lanternA, lanternB, noren,
        norenText, warm;
    unsigned plate[7];
    float night, petals;
};
const VariantDef VARIANTS[] = {
    // strawberry milk: a pink shop, a plum lacquer box with a coral rim
    {"PASTEL", 0xfde9ef, 0xf8d2dd, 0xf0d3b0, 0xd9b386, 0xc9e8dc, 0x3a2532, 0xec7b80, 0x5a3346, 0xf0627f, 0x4a2b3a,
     0xffb3c6, 0xf26b63, 0xfff3e6, 0x5d6fb8, 0xfff6ee, 0xfff0e0,
     {0x8fd4f5, 0xffd866, 0xc4a6f0, 0x9fe2a2, 0xff8fa0, 0x8aa8f7, 0xffaf78}, 0.f, 1.f},
    // a night market: indigo dusk, warm lanterns, a black box with a vermilion rim
    {"NIGHT MARKET", 0x1c2044, 0x2f2756, 0x744c36, 0x553525, 0x3d4268, 0x100f1d, 0xc8463f, 0xf7e9d8, 0xffb65c, 0x1b1426,
     0xf5c3d6, 0xff7a45, 0xffe3b5, 0xb8433c, 0xfff1e0, 0xffd6a8,
     {0x7fd9f8, 0xffdf70, 0xcbaaff, 0x93e8a6, 0xff8c9c, 0x92b2ff, 0xffb37c}, 1.f, 0.8f},
    // under the cherry trees: white-pink paper, a cherry-wood box, petals everywhere
    {"SAKURA", 0xfff5f7, 0xfbdfe8, 0xf4ddc8, 0xdfbea2, 0xf7d6e0, 0x4a2431, 0xf2a0b4, 0x6b3a4d, 0xe5507a, 0x4e2533,
     0xff9fbb, 0xfff6f0, 0xe85a6e, 0xe8507a, 0xfff8fa, 0xffe6ee,
     {0x86cff2, 0xffd65c, 0xb89cf2, 0x8fdb9b, 0xff7f95, 0x7fa0f2, 0xffa66e}, 0.f, 2.f},
    // matcha: a soft green tea house, a dark green lacquer box with a moss rim
    {"MATCHA", 0xeef3df, 0xdae6c6, 0xead7a8, 0xceb47f, 0xd2e3bb, 0x1e2a22, 0x86a35a, 0x34452f, 0xe46a8a, 0x22301f,
     0xf5b8c8, 0xe2605a, 0xfff4dc, 0x3f5b3b, 0xf3f0dc, 0xfff2d6,
     {0x88d1ea, 0xffd45e, 0xc8a4ea, 0xb5e88f, 0xff8a96, 0x8aa3f2, 0xffad72}, 0.f, 0.9f},
    // ramune soda: a sky-blue shop, a navy box with a sky rim
    {"RAMUNE", 0xe6f6fc, 0xcbe8f4, 0xf2ddba, 0xdcc193, 0xc3e3f0, 0x1d2946, 0x55a9dc, 0x23405f, 0xff8a5c, 0x1c2b44,
     0xffc4d3, 0xff8a5c, 0xfff6e8, 0x2f6db3, 0xf4fbff, 0xfff0e2,
     {0x8ad6f6, 0xffd963, 0xc6a8f2, 0x9fe4a6, 0xff8f9f, 0x8aa8f7, 0xffb079}, 0.f, 1.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = SushiUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    Palette p;
    p.wallTop = lin(d.wallTop); p.wallBot = lin(d.wallBot); p.wood = lin(d.wood); p.woodDark = lin(d.woodDark);
    p.belt = lin(d.belt); p.lacquer = lin(d.lacquer); p.rim = lin(d.rim); p.ink = lin(d.ink); p.accent = lin(d.accent);
    p.line = lin(d.line); p.petal = lin(d.petal); p.lanternA = lin(d.lanternA); p.lanternB = lin(d.lanternB);
    p.noren = lin(d.noren); p.norenText = lin(d.norenText); p.warm = lin(d.warm);
    for (int i = 0; i < 7; i++) p.plate[i] = lin(d.plate[i]);
    p.night = d.night; p.petals = d.petals;
    return p;
}
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

// The cats' coats: white, calico, black (white socks, amber eyes), ginger tabby, grey tabby, cream colorpoint.
struct CoatDef { unsigned base, patch, patch2; };
const CoatDef COATS[6] = {
    {0xfffaf4, 0xd9dbe6, 0xd9dbe6},
    {0xfffaf3, 0xf6a25a, 0x4b4049},
    {0x463d4d, 0x463d4d, 0x5c5266},
    {0xf8b46c, 0xfff4e6, 0xe3893f},
    {0xbcc2d2, 0xfbfbff, 0x959db2},
    {0xf8ecd9, 0x8b6a55, 0x8b6a55},
};
const unsigned COLLARS[5] = {0xe8505b, 0x5bb5e8, 0x4fc49a, 0xb68be8, 0xff8fb0};
// The dishes on the belt (the plate, the topping, fat stripes or not).
const unsigned DISH_PLATES[3] = {0xfff3f5, 0xf7c8d4, 0xc9dcf5};
const unsigned DISH_TOPS[4] = {0xff8a52, 0xd8314a, 0xffd04d, 0xffb0a0};

// The well's box (its inside, a rim around it) and the counter.
constexpr float BOX_HX = 5.06f, BOX_HY = 10.06f, BOX_RIM = 0.32f;
constexpr float BELT_TOP = -9.9f, BELT_BOT = -10.3f;
constexpr float EXIT_X = 5.6f;          // a sliding maki is out of the box
constexpr float SLIDE_T = 0.27f, SLIDE_D = 5.6f; // the slide out of a cleared row (well within the stack's 0.3 s hold)
constexpr float CAT_PITCH = 2.7f;

// UTF-8 to the font's characters: the middle dot is glyph 127, accented Latin letters lose their accent, anything
// else outside ASCII becomes a '?'.
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
// Glyph slot of a character (control bytes 1..4 are the kana su, shi, ne, ko).
int glyphIndex(unsigned char ch) {
    if (ch >= 1 && ch <= 4) return 95 + ch;
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}

float easeOut(float u) { u = saturate(u); return 1.f - (1.f - u) * (1.f - u); }
float backOut(float u) {
    u = saturate(u);
    const float c1 = 1.9f, c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(u - 1.f, 3.f) + c1 * std::pow(u - 1.f, 2.f);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

namespace {
// A VAO over the unit quad with `n` per-instance vec4 attributes (locations 1..n) of `stride` bytes.
void instVao(GLuint quad, GLuint& vao, GLuint& vbo, int n, size_t stride) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < n; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}
} // namespace

bool SushiUniverse::init(int width, int height) {
    using shaders::withCommon;
    progBack_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::BACK_FS).c_str(), "sushi shop");
    progSprite_ = ugl::program(withCommon(shaders::SPRITE_VS).c_str(), withCommon(shaders::SPRITE_FS).c_str(), "sushi sprites");
    progCat_ = ugl::program(withCommon(shaders::CAT_VS).c_str(), withCommon(shaders::CAT_FS).c_str(), "sushi cats");
    progMaki_ = ugl::program(shaders::MAKI_VS, withCommon(shaders::MAKI_FS, true).c_str(), "sushi maki");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "sushi final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "sushi shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "sushi text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, spriteVao_, spriteVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, makiVao_, makiVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, catVao_, catVbo_, 8, sizeof(CatInst));
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

    buildFont(fontB_, FONT_BOLD);
    buildFont(fontM_, FONT_MEDIUM);
    pal_ = target_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void SushiUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] sushi font init failed\n");
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
    const int KANA[4] = {0x3059, 0x3057, 0x306D, 0x3053};
    for (int i = 0; i < 100; i++) {
        const int ch = 32 + i;
        const int cp = i >= 96 ? KANA[i - 96] : ch == 127 ? 0xB7 : ch;
        const float sc = i >= 96 ? f.scaleTT * 2.f : f.scaleTT; // the kana are large (the noren)
        Glyph& g = f.g[i];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, sc, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
        const float k = f.scaleTT / sc; // back to the font's size
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = xo * k; g.oy = yo * k; g.w = w * k; g.h = h * k;
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

void SushiUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    layoutSlots();
}

SushiUniverse::~SushiUniverse() {
    sceneT_.destroy();
    for (GLuint p : {progBack_, progSprite_, progCat_, progMaki_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, makiVao_, catVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, makiVbo_, catVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontB_, &fontM_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The cats
// =====================================================================================================

// The seats along the counter on one side (the other mirrors them), from beside the box to the screen's edge, and
// the order they fill in (spread out first, the gaps filled later).
void SushiUniverse::layoutSlots() {
    for (int sd = 0; sd < 2; sd++) {
        // on the left, the seats stop short of the corner where the song's section and the universe are written
        const float xa = 7.2f, xb = std::max(xa, sd == 0 ? viewH_.x - 6.7f : viewH_.x - 1.3f);
        const int n = std::max(1, (int)std::floor((xb - xa) / CAT_PITCH) + 1);
        slotX_[sd].clear();
        for (int i = 0; i < n; i++) slotX_[sd].push_back(n == 1 ? xa : lerpf(xa, xb, i / (float)(n - 1)));
        slotOrder_[sd].clear();
        const int pref[8] = {1, 3, 0, 4, 2, 5, 6, 7};
        for (int i : pref)
            if (i < n) slotOrder_[sd].push_back(i);
        for (int i = 8; i < n; i++) slotOrder_[sd].push_back(i);
    }
    // seats that are gone (a narrower window): their cats leave; the others move to their seat's new place
    for (Cat& c : cats_) {
        const std::vector<float>& sx = slotX_[c.side > 0 ? 1 : 0];
        if (c.slot >= (int)sx.size()) { if (c.leave < 0.f) c.leave = 0.f; continue; }
        c.x = c.side * sx[c.slot];
    }
}

bool SushiUniverse::addCat(int side, bool instant) {
    bool used[64] = {};
    for (const Cat& c : cats_)
        if (c.side == side && c.leave < 0.f && c.slot < 64) used[c.slot] = true;
    int slot = -1;
    for (int i : slotOrder_[side > 0 ? 1 : 0])
        if (!used[i]) { slot = i; break; }
    if (slot < 0) return false;
    Cat c;
    c.uid = nextCatUid_++;
    c.side = side;
    c.slot = slot;
    c.x = side * slotX_[side > 0 ? 1 : 0][slot];
    // coats in a shuffled cycle, so neighbors differ
    static const int CYCLE[12] = {1, 0, 3, 2, 4, 5, 3, 1, 2, 0, 5, 4};
    c.coat = CYCLE[(coatCycle_++ + (int)(seed_ % 5)) % 12];
    const CoatDef& cd = COATS[c.coat];
    c.base = hex(cd.base); c.patch = hex(cd.patch); c.patch2 = hex(cd.patch2);
    c.collar = hex(COLLARS[rng_.irange(0, 4)]);
    // under the left HUD column (which reaches lower than the right one) the cats are a little smaller
    c.scale = (side < 0 && slotX_[0][slot] < 11.8f) ? rng_.range(0.94f, 1.0f) : rng_.range(1.04f, 1.16f);
    c.appear = instant ? 1.f : 0.f;
    c.phase = (c.uid % 2) ? 0.f : PI;
    c.blinkBeat = rng_.irange(0, 7);
    c.dozeAt = rng_.range(0.25f, 0.8f);
    c.maneki = (c.uid % 3) == 0;
    c.zCd = rng_.range(0.f, 3.f);
    cats_.push_back(c);
    return true;
}

int SushiUniverse::catIndex(int uid) const {
    for (size_t i = 0; i < cats_.size(); i++)
        if (cats_[i].uid == uid) return (int)i;
    return -1;
}

// A cat to feed on that side: the one with the fewest maki coming, spread along the counter.
int SushiUniverse::pickCat(int side, int seq) {
    int best = -1;
    float bestScore = 1e9f;
    for (int pass = 0; pass < 2 && best < 0; pass++) {
        const int s = pass == 0 ? side : -side;
        for (const Cat& c : cats_) {
            if (c.side != s || c.leave >= 0.f) continue;
            int coming = 0;
            for (const Flight& f : flights_)
                if (f.cat == c.uid) coming++;
            const float score = coming + 0.01f * ((c.slot * 7 + seq) % 5);
            if (score < bestScore) { bestScore = score; best = c.uid; }
        }
    }
    return best;
}

float SushiUniverse::catLift(const Cat& c) const {
    float lift = -3.4f * (1.f - backOut(c.appear));
    if (c.leave >= 0.f) lift -= 3.6f * c.leave * c.leave;
    return lift;
}

vec2 SushiUniverse::mouthOf(const Cat& c) const {
    return {c.x, BELT_TOP + (catLift(c) + 1.98f - 0.3f) * c.scale};
}

void SushiUniverse::eat(Cat& c) {
    c.chew = 0.9f;
    c.happy = 1.5f;
    c.eaten++;
    c.doze = 0.f;
    const vec2 m = mouthOf(c);
    if (c.heartCd <= 0.f) {
        pops_.push_back({m.x - c.side * 0.45f * c.scale, m.y + 1.55f * c.scale, 0.f, 1.3f, 0, pal_.accent, 0.3f});
        c.heartCd = 0.55f;
    }
    if (c.nomCd <= 0.f) {
        pops_.push_back({m.x + c.side * 0.4f * c.scale, m.y + 1.75f * c.scale, 0.f, 0.9f, 2, pal_.ink, 1.f});
        c.nomCd = 0.8f;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SushiUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.48f) v = 1;
        else if (t.hasSong && t.airWeight > 0.62f && t.brightness > 0.55f) v = 2;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    target_ = paletteOf(v);
    if (!palSet_ || seconds <= 0.1f) { pal_ = target_; palSet_ = true; }
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5005ull);
    // a new song: the counter empties down to two cats (the others duck away) and fills up again with the lines
    eatenLines_ = 0;
    feastBonus_ = 0;
    if (seconds <= 0.1f) {
        cats_.clear();
        coatCycle_ = (int)(seed_ % 12);
        addCat(-1, true);
        addCat(1, true);
    } else {
        int keep[2] = {0, 0};
        for (Cat& c : cats_) {
            if (c.leave >= 0.f) continue;
            const int s = c.side > 0 ? 1 : 0;
            if (keep[s] < 1) keep[s]++;
            else c.leave = 0.f;
        }
    }
}

void SushiUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    (void)impact;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: a festival (eased in over the next beat or two: see render).
void SushiUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    for (int i = 0; i < 46; i++) spawnPetal(true);
    for (const Cat& c : cats_) {
        if (c.leave >= 0.f) continue;
        const vec2 m = mouthOf(c);
        for (int k = 0; k < 2; k++)
            pops_.push_back({m.x + rng_.range(-1.2f, 1.2f), m.y + rng_.range(0.8f, 1.8f), -rng_.range(0.f, 0.6f), 0.9f, 1,
                             pal_.lanternB * 0.5f + vec3(0.5f, 0.45f, 0.3f), rng_.range(0.18f, 0.28f)});
    }
}

// A new section: a cat on each side is pleased.
void SushiUniverse::refresh(float strength) {
    (void)strength;
    for (int s = -1; s <= 1; s += 2) {
        std::vector<int> idx;
        for (size_t i = 0; i < cats_.size(); i++)
            if (cats_[i].side == s && cats_[i].leave < 0.f) idx.push_back((int)i);
        if (!idx.empty()) cats_[idx[rng_.irange(0, (int)idx.size() - 1)]].happy = 1.2f;
    }
}

void SushiUniverse::levelUp() {
    feastBonus_++;
    swell_ = std::max(swell_, 0.3f);
}

vec2 SushiUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SushiUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(pal_.wallTop, pal_.wallBot, 0.5f);
    h.light = pal_.night < 0.5f;
    h.ink = pal_.ink;
    h.accent = pal_.accent;
    h.styles = TS_CURTAIN | TS_TILES | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void SushiUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) ids_[y][x] = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            while (sp > 0 && count < 4) {
                sp--;
                const int cx = stack[sp][0], cy = stack[sp][1];
                count++;
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& dd : d) {
                    const int nx = cx + dd[0], ny = cy + dd[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void SushiUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        int seq[2] = {0, 0};
        // outer cells first, so each side's maki leave the box in a stream
        std::vector<GameEvent::CellInfo> cells = ev.cells;
        std::sort(cells.begin(), cells.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) {
            const float da = std::fabs(a.x - 4.5f), db = std::fabs(b.x - 4.5f);
            return da != db ? da > db : a.y < b.y;
        });
        for (auto& c : cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
            }
            fading_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f});
            Flight f;
            f.x0 = p.x; f.y0 = p.y;
            f.side = c.x < Game::W / 2 ? -1 : 1;
            f.type = std::clamp(c.type, 0, 6);
            const int s = f.side > 0 ? 1 : 0;
            f.cat = pickCat(f.side, seq[s]);
            f.arcDur = 0.5f + 0.06f * (seq[s] % 6) + rng_.range(0.f, 0.08f);
            f.h = rng_.range(1.4f, 2.4f);
            f.spin = rng_.range(3.f, 7.f) * (rng_.chance(0.5f) ? 1.f : -1.f);
            f.x = p.x; f.y = p.y;
            seq[s]++;
            flights_.push_back(f);
        }
        {
            int nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
        }
        eatenLines_ += ev.count;
        if (ev.count >= 4) {
            // a Tetris: a feast (two more cats come, sparkles all along the counter)
            feast_ = 1.f;
            feastBonus_ += 2;
            for (const Cat& c : cats_) {
                if (c.leave >= 0.f) continue;
                const vec2 m = mouthOf(c);
                for (int k = 0; k < 3; k++)
                    pops_.push_back({m.x + rng_.range(-1.3f, 1.3f), m.y + rng_.range(0.6f, 1.9f), -rng_.range(0.3f, 1.0f), 0.8f, 1,
                                     vec3(1.f, 0.9f, 0.55f), rng_.range(0.16f, 0.26f)});
            }
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        // the whole stack goes to the cats, from the top down
        int seq[2] = {0, 0};
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
            }
            Flight f;
            f.x0 = p.x; f.y0 = p.y;
            f.side = c.x < Game::W / 2 ? -1 : 1;
            f.type = std::clamp(c.type, 0, 6);
            const int s = f.side > 0 ? 1 : 0;
            f.cat = pickCat(f.side, seq[s]++);
            f.slide = false;
            f.exited = true;
            f.plate = true;
            f.mask = m;
            f.ex = p.x; f.ey = p.y;
            f.delay = 0.6f + 0.09f * (c.y - Game::HIDDEN) + 0.02f * std::fabs(c.x - 4.5f);
            f.arcDur = 0.8f + rng_.range(0.f, 0.2f);
            f.h = rng_.range(1.5f, 2.6f);
            f.spin = rng_.range(3.f, 6.f) * (rng_.chance(0.5f) ? 1.f : -1.f);
            f.x = p.x; f.y = p.y;
            flights_.push_back(f);
        }
        std::memset(ids_, 0, sizeof(ids_));
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Flights
// =====================================================================================================

void SushiUniverse::updateFlights(float dt) {
    for (Cat& c : cats_) c.incoming = 0.f;
    for (size_t i = 0; i < flights_.size();) {
        Flight& f = flights_[i];
        f.t += dt;
        const float tt = f.t - f.delay;
        bool done = false;
        if (tt < 0.f) { f.x = f.x0; f.y = f.y0; f.s = 1.f; f.rot = 0.f; i++; continue; }
        int ci = catIndex(f.cat);
        if (ci < 0 || cats_[ci].leave >= 0.f) {
            f.cat = pickCat(f.side, (int)i);
            ci = catIndex(f.cat);
        }
        if (f.slide && !f.exited) {
            // out of the row, sideways (the row stays clear of the stack until it settles)
            const float u = saturate(tt / SLIDE_T);
            const float x = f.x0 + f.side * SLIDE_D * std::pow(u, 1.6f);
            f.x = x; f.y = f.y0;
            f.s = 1.f + 0.12f * std::sin(PI * std::min(1.f, u * 2.f)) * (1.f - u);
            f.rot = f.spin * 0.15f * u;
            if (std::fabs(x) >= EXIT_X || u >= 1.f) {
                f.exited = true;
                f.ex = f.side * std::max(std::fabs(x), EXIT_X);
                f.ey = f.y0;
                f.exitT = tt;
            }
        }
        if (f.exited) {
            const float u = (tt - f.exitT) / f.arcDur;
            if (ci < 0) {
                // nobody to catch it: it bounces away off the screen's side, fading
                f.x = f.ex + f.side * 9.f * u;
                f.y = f.ey + f.h * 4.f * u * (1.f - u) - 3.f * u * u;
                f.s = 1.f - 0.6f * saturate(u);
                done = u >= 1.f;
            } else {
                Cat& c = cats_[ci];
                const vec2 m = mouthOf(c);
                const float e = easeOut(u);
                f.x = lerpf(f.ex, m.x, e);
                f.y = lerpf(f.ey, m.y, e) + f.h * 4.f * u * (1.f - u);
                f.s = 0.85f - 0.4f * smoothstepf(0.7f, 1.f, u);
                f.rot = f.spin * 0.15f * SLIDE_T + f.spin * (tt - f.exitT);
                const float left = (1.f - u) * f.arcDur;
                if (left < 0.38f) c.incoming = std::max(c.incoming, 1.f - left / 0.38f);
                c.look = lerpf(c.look, saturate((f.x - c.x) * c.side * 0.4f) * c.side, 0.2f);
                if (u >= 1.f) { eat(c); done = true; }
            }
        }
        if (done) { flights_[i] = flights_.back(); flights_.pop_back(); }
        else i++;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void SushiUniverse::addMaki(float x, float y, float half, int mask, int type, float alpha, float plateA, float spin,
                            float scale, int kind, float makiA, float shadow) {
    if (alpha <= 0.003f) return;
    const vec3 c = pal_.plate[std::clamp(type, 0, 6)];
    makis_.push_back({{x, y, half, (float)mask}, {(float)type, alpha, plateA, spin}, {c.x, c.y, c.z, scale},
                      {(float)kind, makiA, shadow, 0.f}});
}

void SushiUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    // the ghost: the plate's outline where it will land
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto inGhost = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addMaki(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, 1.f, 0.f, 0.f, 1.f, 1, 0.f, 0.f);
            }
        }
    }
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return g.cell(nx, ny).type == c.type;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            // a lock: the maki give a small bounce on their plate
            const float t = (1.f - c.flash) * Game::LOCK_FX_SECONDS;
            const float bump = t < 0.32f ? std::sin(t / 0.32f * PI) * (1.f - t / 0.32f) * (c.dropped ? 0.2f : 0.12f) : 0.f;
            addMaki(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, 1.f, 1.f, 0.f, 1.f + bump, 0, 1.f, 1.f);
        }
    // a cleared row's plates, emptied, fading in place
    for (const Fading& fd : fading_) {
        const float k = saturate(fd.t / 0.2f);
        addMaki(fd.x, fd.y, 0.5f * (1.f - 0.85f * k * k), fd.mask, fd.type, 1.f - k * k * k, 1.f, 0.f, 1.f, 0, 0.f, 0.f);
    }
    // the falling piece: the same plate and maki
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            addMaki(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, 1.f, 1.f, 0.f, 1.f, 0, 1.f, 1.f);
        }
    }
    // the previews
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            addMaki(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, alpha, 1.f, 0.f, 1.f, 0, 1.f, 1.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void SushiUniverse::spawnPetal(bool top) {
    Petal p;
    p.x = rng_.range(-viewH_.x - 1.f, viewH_.x + 1.f);
    p.y = top ? viewC_.y + viewH_.y + rng_.range(0.3f, 4.f) : rng_.range(-viewH_.y, viewH_.y) + viewC_.y;
    p.vx = rng_.range(0.15f, 0.6f);
    p.vy = -rng_.range(0.9f, 1.6f);
    p.rot = rng_.range(0.f, TAU);
    p.spin = rng_.range(-1.4f, 1.4f);
    p.size = rng_.range(0.17f, 0.27f);
    p.phase = rng_.range(0.f, TAU);
    petals_.push_back(p);
}

// The shop's small things (behind the cats): the lanterns on their cords, the noren, the petals; the belt's dishes
// and the pops go to front_.
void SushiUniverse::buildShop(float beat, float wm, float wp) {
    auto push = [&](std::vector<Inst>& v, float x, float y, float hw, float hh, int kind, float rot, float alpha, float param,
                    const vec3& c, float pc = 0.f, const vec3& d = vec3(0.f), float pd = 0.f) {
        v.push_back({{x, y, hw, hh}, {(float)kind, rot, alpha, param}, {c.x, c.y, c.z, pc}, {d.x, d.y, d.z, pd}});
    };
    // ---- petals (behind everything, never inside the box)
    for (const Petal& p : petals_)
        push(back_, p.x, p.y, p.size * 0.75f, p.size, 0, p.rot, 0.9f, 0.f, pal_.petal);

    // ---- lanterns: a cord on each side, sagging, four paper lanterns; they light up with the song
    const float lit0 = 0.12f + 0.35f * pal_.night + 0.25f * wm + 0.45f * wp + 0.4f * fest_ + 0.5f * slow_;
    const float top = viewC_.y + viewH_.y;
    for (int side = -1; side <= 1; side += 2) {
        const float xa = side * 12.3f, xb = side * (viewH_.x - 0.2f);
        const float y0 = std::min(top - 2.4f, 9.9f);
        push(back_, (xa + xb) * 0.5f, y0 - 0.3f, std::fabs(xb - xa) * 0.5f + 0.1f, 0.45f, 9, 0.f, 1.f, 0.035f, pal_.line * 1.4f, 0.f,
             vec3(std::min(xa, xb), std::max(xa, xb), y0), 0.6f);
        const float us[4] = {0.13f, 0.38f, 0.63f, 0.88f};
        for (int i = 0; i < 4; i++) {
            const float u = us[i];
            const float ax = lerpf(xa, xb, u), ay = y0 - 0.6f * 4.f * u * (1.f - u);
            const float amp = 0.025f + 0.04f * wm + 0.04f * wp + 0.03f * fest_;
            const float ang = amp * (float)std::sin(PI * beat * 0.5 + i * 0.9 + (side > 0 ? 0.5 : 0.0)) +
                              0.015f * std::sin(worldTime_ * 0.7f + i);
            const float len = 0.38f;
            const vec2 dir{std::sin(ang), -std::cos(ang)};
            push(back_, ax + dir.x * len * 0.5f, ay + dir.y * len * 0.5f, 0.024f, len * 0.5f + 0.03f, 3, ang, 1.f, 0.f, pal_.line);
            const float cx = ax + dir.x * (len + 0.6f), cy = ay + dir.y * (len + 0.6f);
            const float lit = saturate(lit0 * 4.f - (side > 0 ? i : 3 - i) * 0.9f);
            const bool red = (i + (side > 0 ? 1 : 0)) % 2 == 0;
            const vec3 paper = red ? pal_.lanternA : pal_.lanternB;
            const vec3 band = red ? pal_.lanternB : pal_.lanternA;
            // their warm glow on the wall (added)
            const float g = lit * (0.2f + 0.16f * pal_.night + 0.2f * swell_);
            if (g > 0.01f) push(back_, cx, cy, 1.7f, 1.7f, 2, 0.f, g, 0.f, vec3(1.f, 0.66f, 0.32f));
            push(back_, cx, cy, 0.5f, 0.66f, 1, ang, 1.f, lit, paper, 0.f, band, 0.85f);
        }
    }

    // ---- the noren in the upper right: two panels with "su" "shi", swaying on the beat grid
    {
        const float x0 = 13.2f, pw = 2.55f, gap = 0.16f;
        const float rodY = top - 0.35f;
        if (x0 + 2 * pw + gap < viewH_.x - 1.5f) {
            push(back_, x0 + pw + gap * 0.5f, rodY, pw + 0.45f, 0.08f, 3, 0.f, 1.f, 0.f, pal_.woodDark * 0.8f);
            const float amp = 0.008f + 0.018f * wm + 0.02f * wp + 0.015f * fest_;
            for (int i = 0; i < 2; i++) {
                const float cx = x0 + pw * 0.5f + i * (pw + gap);
                const float ang = amp * (float)std::sin(PI * beat * 0.5 + i * 0.6);
                const float hh = 1.45f;
                // hung from the rod: rotate about its top
                const float ccx = cx + std::sin(ang) * hh, ccy = rodY - std::cos(ang) * hh;
                push(back_, ccx, ccy, pw * 0.5f, hh, 4, ang, 1.f, 0.f, pal_.noren);
            }
        }
    }
}

// The cats' instances (and their idle life: dozing, blinking, bobbing on the beat grid, cheering).
void SushiUniverse::buildCats(float beat, float wm, float wp, float dt) {
    const float half = PI * beat * 0.5f; // the two-beat grid
    for (Cat& c : cats_) {
        if (c.leave >= 0.f) c.leave += dt / 0.7f;
        else c.appear = std::min(1.f, c.appear + dt / 0.75f);
        c.chew = std::max(0.f, c.chew - dt);
        c.happy = std::max(0.f, c.happy - dt);
        c.heartCd -= dt;
        c.nomCd -= dt;
        // dozing in calm parts (each cat its own threshold), never while something is coming or tasting
        const bool busy = c.incoming > 0.f || c.chew > 0.f || c.happy > 0.f || fest_ > 0.2f;
        const float sleepy = std::max(1.f - smoothstepf(0.f, 0.9f, lv_), slow_ * 0.8f);
        const float dozeT = (!busy && sleepy > c.dozeAt) ? 1.f : 0.f;
        c.doze = approach(c.doze, dozeT, dozeT > c.doze ? 0.5f : 4.f, dt);
        c.cheer = approach(c.cheer, fest_, 3.f, dt);
        // eyes
        const long b = (long)std::floor(beat);
        const bool blink = (((b % 8) + 8) % 8) == c.blinkBeat && (beat - (float)b) < 0.18f;
        float eyeT = 1.f;
        if (c.doze > 0.5f) eyeT = 0.f;
        else if (c.happy > 0.f || c.chew > 0.f || c.cheer > 0.5f) eyeT = 0.f;
        else if (blink) eyeT = 0.f;
        if (c.incoming > 0.f && c.chew <= 0.f) eyeT = 1.25f;
        c.eye = approach(c.eye, eyeT, 14.f, dt);
        const float mouthT = std::max(c.incoming, c.cheer * 0.8f * (0.6f + 0.4f * std::fabs(std::sin(half + c.phase))));
        c.mouth = approach(c.mouth, mouthT, 10.f, dt);
        c.look = approach(c.look, 0.f, 1.5f, dt);
        // arms: both up in a cheer, a maneki-neko's wave in a peak
        const float wave = 0.5f + 0.5f * std::sin(half * 2.f + c.phase);
        const float maneki = c.maneki ? wp * (1.f - c.cheer) * 0.75f : 0.f;
        c.armL = approach(c.armL, std::max(c.cheer * (0.8f + 0.2f * wave), 0.f), 5.f, dt);
        c.armR = approach(c.armR, std::max(c.cheer * (0.8f + 0.2f * (1.f - wave)), maneki * (0.75f + 0.25f * wave)), 5.f, dt);
        // zzz
        if (c.doze > 0.9f && c.appear >= 1.f) {
            c.zCd -= dt;
            if (c.zCd <= 0.f) {
                const vec2 m = mouthOf(c);
                pops_.push_back({m.x + c.side * 0.7f, m.y + 1.2f, 0.f, 2.6f, 3, pal_.ink, 1.f});
                c.zCd = 3.2f + rng_.range(0.f, 1.5f);
            }
        }

        // motion: a slow breath when dozing, a bob on the beat grid from a chorus, a little bounce in a peak
        const float s2 = std::sin(half + c.phase);
        const float bob = (0.035f * wm + 0.03f * wp) * s2 * (1.f - c.doze);
        const float hop = (0.1f * wp + 0.12f * c.cheer) * std::pow(std::fabs(std::sin(half + c.phase)), 2.f) * (1.f - c.doze);
        const float breath = 0.012f * std::sin(worldTime_ * 1.6f + c.phase);
        const float sway = bob + 0.01f * std::sin(worldTime_ * 0.5f + c.phase);
        const float tilt = 0.05f * c.doze + 0.07f * s2 * (wm * 0.6f + wp * 0.4f) * (1.f - c.doze) +
                           (c.incoming > 0.f ? 0.06f * c.side * -1.f * c.incoming : 0.f);
        const float tail = (0.35f + 0.65f * wm) * std::sin(half + c.phase + 1.f) * (1.f - 0.7f * c.doze) +
                           0.25f * std::sin(worldTime_ * 0.8f + c.phase);
        const float chewK = c.chew > 0.f ? 0.5f + 0.5f * std::sin(c.chew * 22.f) : 0.f;
        const float squash = 1.f + breath - 0.03f * chewK * 0.f;
        const float lift = catLift(c) + hop;
        const float blush = 0.6f + 0.4f * std::max(saturate(c.happy), c.cheer);
        CatInst ci;
        const float alpha = 1.f;
        const float v[8][4] = {
            {c.x, BELT_TOP, c.scale, (float)-c.side},
            {c.base.x, c.base.y, c.base.z, (float)c.coat},
            {c.patch.x, c.patch.y, c.patch.z, 0.f},
            {c.patch2.x, c.patch2.y, c.patch2.z, std::min(1.f, c.eye)},
            {c.collar.x, c.collar.y, c.collar.z, (c.doze > 0.5f && c.happy <= 0.f && c.cheer < 0.5f) ? 0.f : 1.f},
            {c.mouth, chewK, blush, alpha},
            {c.armL, c.armR, tail, sway},
            {tilt, squash, lift, c.look * -c.side},
        };
        std::memcpy(ci.v, v, sizeof(v));
        catInst_.push_back(ci);
    }
    // the cats that ducked away are gone
    cats_.erase(std::remove_if(cats_.begin(), cats_.end(), [](const Cat& c) { return c.leave >= 1.f; }), cats_.end());
}

void SushiUniverse::drawInst(GLuint prog, GLuint vao, const std::vector<Inst>& v) {
    if (v.empty()) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vao == makiVao_ ? makiVbo_ : spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float SushiUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void SushiUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                              std::vector<float>& out) const {
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

void SushiUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
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

void SushiUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// "nom" and "z" floating by the cats (display colors, after the frame).
void SushiUniverse::drawPops() {
    const float vis = (1.f - smoothstepf(0.2f, 0.7f, bridge_.amount)) * (1.f - 0.5f * dimT_);
    if (vis <= 0.01f) return;
    const float s = height_ / 1080.f;
    const vec3 ink = ugl::toSrgb(pal_.ink), halo = ugl::toSrgb(lerp(pal_.wallTop, pal_.wallBot, 0.6f));
    std::vector<float> q;
    for (const Pop& p : pops_) {
        if (p.kind < 2 || p.t < 0.f) continue;
        const float u = p.t / p.life;
        const float a = smoothstepf(0.f, 0.12f, u) * (1.f - smoothstepf(0.6f, 1.f, u)) * vis;
        if (a <= 0.01f) continue;
        q.clear();
        const vec2 c = project(vec3(p.x, p.y, 0.f));
        if (p.kind == 2) {
            const float size = 30.f * s * (1.f + 0.25f * (1.f - smoothstepf(0.f, 0.2f, u)));
            const std::string t = "nom";
            const float w = textWidth(fontB_, t, size, 0.f);
            textQuads(fontB_, t, c.x - w * 0.5f, c.y - size * 0.5f, size, 0.f, q);
            drawText(fontB_, q, ugl::toSrgb(pal_.accent), halo, a, 0.6f);
        } else {
            const float size = 26.f * s * (0.7f + 0.5f * u);
            textQuads(fontM_, "z", c.x, c.y, size, 0.f, q);
            drawText(fontM_, q, ink, halo, a * 0.7f, 0.4f);
        }
    }
}

void SushiUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const vec3 ink = ugl::toSrgb(pal_.ink), halo = ugl::toSrgb(lerp(pal_.wallTop, pal_.wallBot, 0.5f)),
               accent = ugl::toSrgb(pal_.accent);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    // what is written over the box (the dialogs, a celebration's line) is light on its dark lacquer
    const vec3 boxInk = ugl::toSrgb(pal_.night > 0.5f ? pal_.ink : pal_.wallTop), boxHalo = ugl::toSrgb(pal_.lacquer);

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

    // the bonus gauge: a soft round track, its charge in the accent, a dot at its end
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 96;
        const float th = std::max(2.f, 5.f * s), th2 = std::max(3.f, 9.f * s);
        auto seg = [&](float a0, float a1, float w, const vec3& col, float al) {
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        };
        const float dimk = vis * (1.f - 0.5f * dimT_);
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, th, ink, (g.bonusReady() ? 0.4f : 0.18f) * dimk);
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, th2, accent, 0.95f * dimk);
        }
        flushShapes();
        if (fill > 0.01f) {
            // a round cap at the end of the charge
            const float a = TAU * fill;
            const vec2 e{c.x + std::sin(a) * r, c.y - std::cos(a) * r};
            const int M = 20;
            for (int i = 0; i < M; i++) {
                const float a0 = TAU * i / M, a1 = TAU * (i + 1) / M, rr = th2 * 0.62f;
                const float v[3][6] = {{e.x, e.y, accent.x, accent.y, accent.z, 0.95f * dimk},
                                       {e.x + std::cos(a0) * rr, e.y + std::sin(a0) * rr, accent.x, accent.y, accent.z, 0.95f * dimk},
                                       {e.x + std::cos(a1) * rr, e.y + std::sin(a1) * rr, accent.x, accent.y, accent.z, 0.95f * dimk}};
                for (auto& vv : v) shapes_.insert(shapes_.end(), vv, vv + 6);
            }
            flushShapes();
        }
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
            place(fontM_, upper(h.text), base * 0.85f, base * 0.14f, base * 0.06f);
            drawText(fontM_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * vis);
            continue;
        case HUD_SECTION:
            place(fontB_, upper(h.text), base * 0.9f, base * 0.12f, -base * 0.05f);
            drawText(fontB_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(fontB_, h.text, base * 1.0f, 0.f, -base * 0.08f);
            drawText(fontB_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(fontM_, h.text, base * 0.8f, base * 0.04f);
            drawText(fontM_, q, ink, halo, saturate(alpha * 1.2f) * 0.75f);
            continue;
        case HUD_LABEL:
            place(fontB_, upper(h.text), base * 0.78f, base * 0.2f);
            drawText(fontB_, q, ink, halo, saturate(alpha * 1.2f) * 0.68f);
            continue;
        case HUD_VALUE:
            place(fontB_, h.text, base * 1.08f, base * 0.02f, -base * 0.08f);
            drawText(fontB_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontB_, h.text, base * 0.98f, base * 0.04f);
            drawText(fontB_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontB_, h.text, base * 0.9f, 0.f);
            drawText(fontB_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontB_, h.text, base * 1.12f, base * 0.12f, -base * 0.12f);
            drawText(fontB_, q, accent, boxHalo, alpha, 0.6f);
            continue;
        case HUD_TOAST_SUB:
            place(fontB_, h.text, base * 0.9f, base * 0.04f);
            drawText(fontB_, q, boxInk, boxHalo, alpha * 0.9f, 0.6f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontB_, h.text, base * 1.0f, base * 0.06f);
                drawText(fontB_, q, boxInk, boxHalo, alpha, 0.7f);
            } else {
                place(fontM_, h.text, base * 0.86f, base * 0.03f);
                drawText(fontM_, q, h.accent ? accent : boxInk, boxHalo, alpha, 0.7f);
            }
            continue;
        default:
            place(fontM_, h.text, base * 0.84f, 0.f);
            drawText(fontM_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SushiUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    syncIds(game);
    lerpPalette(pal_, target_, 1.f - std::exp(-dt * 1.2f));
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& fl) { return fl.t < fl.delay; }),
                           flights_.end());
        } else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    if (music.hasSong) beat_ = (float)music.beat;
    else beat_ += wdt * 2.f;
    const float spb = 60.f / std::max(60.f, music.bpm);
    // the festival: eased in over about two beats after the drop's downbeat, held through the first bars, then
    // settling (and gone once the peak ends)
    dropT_ += dt;
    float festT = 0.f;
    if (dropMode_) festT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.55f * smoothstepf(16.f * spb, 32.f * spb, dropT_));
    fest_ = approach(fest_, festT, festT > fest_ ? 2.5f : 0.35f, dt);
    swell_ = approach(swell_, 0.f, 0.25f, dt);
    feast_ = approach(feast_, 0.f, 0.5f, dt);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- The counter fills with cats as the lines are eaten (a Tetris brings two more).
    {
        int active = 0, perSide[2] = {0, 0};
        for (const Cat& c : cats_)
            if (c.leave < 0.f) { active++; perSide[c.side > 0 ? 1 : 0]++; }
        const int want = std::min((int)(slotX_[0].size() + slotX_[1].size()), 2 + eatenLines_ / 2 + feastBonus_);
        spawnCd_ -= dt;
        if (active < want && spawnCd_ <= 0.f && overT_ < 0.f) {
            const int side = perSide[0] < perSide[1] ? -1 : perSide[1] < perSide[0] ? 1 : (rng_.chance(0.5f) ? -1 : 1);
            if (addCat(side, false) || addCat(-side, false)) spawnCd_ = 0.45f;
        }
    }

    // ---- The belt, the petals, the pops (real time for what the play drives, world time for the shop).
    const float beltT = 0.65f * smoothstepf(0.3f, 1.f, lv_) + 0.6f * wp + 0.9f * fest_;
    beltSpeed_ = approach(beltSpeed_, beltT * (1.f - 0.6f * slow_), 0.6f, wdt);
    beltOff_ = std::fmod(beltOff_ + beltSpeed_ * wdt, 3.45f * 64.f);
    {
        const float target = (5.f + 9.f * wm + 14.f * wp + 10.f * fest_) * pal_.petals;
        const float rate = target / 14.f;
        petalAcc_ += rate * wdt;
        while (petalAcc_ >= 1.f) { petalAcc_ -= 1.f; spawnPetal(true); }
        if (petals_.empty() && target > 0.f && worldTime_ < 0.5f)
            for (int i = 0; i < (int)target; i++) spawnPetal(false);
        for (Petal& p : petals_) {
            p.x += (p.vx + 0.35f * std::sin(worldTime_ * 1.1f + p.phase)) * wdt;
            p.y += p.vy * wdt;
            p.rot += p.spin * wdt;
        }
        const float bottom = viewC_.y - viewH_.y - 0.5f, right = viewH_.x + 1.5f;
        petals_.erase(std::remove_if(petals_.begin(), petals_.end(),
                                     [&](const Petal& p) { return p.y < bottom || p.x > right; }),
                      petals_.end());
    }
    for (Pop& p : pops_) {
        p.t += dt;
        if (p.t > 0.f) p.y += dt * (p.kind == 0 ? 0.9f : p.kind == 1 ? 0.25f : p.kind == 2 ? 0.7f : 0.45f);
        if (p.kind == 3) p.x += dt * 0.15f * std::sin(p.t * 2.f);
    }
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t > p.life; }), pops_.end());
    for (Fading& fd : fading_) fd.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& fd) { return fd.t > 0.2f; }), fading_.end());
    updateFlights(dt);
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Instances.
    back_.clear();
    makis_.clear();
    front_.clear();
    catInst_.clear();
    buildShop(beat_, wm, wp);
    buildCats(beat_, wm, wp, dt);
    // the dishes riding the belt (in front of the cats; the box hides them)
    {
        const float pitch = 3.45f;
        const float x0 = -viewH_.x - 2.f;
        const int n = (int)((2.f * viewH_.x + 4.f) / pitch) + 2;
        const float off = std::fmod(beltOff_, pitch);
        const long baseIdx = (long)std::floor(beltOff_ / pitch);
        // the belt starts with a rounded end left of the seats (the dishes come up there)
        const float hatch = beltStart();
        for (int i = 0; i < n; i++) {
            const float x = x0 + i * pitch + off;
            const float da = smoothstepf(hatch + 0.3f, hatch + 1.0f, x);
            if (da <= 0.01f) continue;
            const long k = i - baseIdx;
            const unsigned hsh = (unsigned)(splitmix64((uint64_t)(k * 2654435761LL) ^ seed_) & 0xffff);
            if (hsh % 5 == 0) continue; // an empty spot now and then
            const vec3 dish = lin(DISH_PLATES[hsh % 3]);
            const int top = (hsh / 3) % 4;
            front_.push_back({{x, BELT_TOP - 0.06f, 0.62f, 0.42f}, {7.f, 0.f, da, 0.f}, {dish.x, dish.y, dish.z, 0.f},
                              {lin(DISH_TOPS[top]).x, lin(DISH_TOPS[top]).y, lin(DISH_TOPS[top]).z, top == 0 ? 1.f : 0.f}});
        }
    }
    buildBoard(game);
    // maki in the air (over everything; plates waiting in place at a game over)
    std::vector<Inst> air;
    for (const Flight& fl : flights_) {
        if (fl.t < fl.delay) {
            addMaki(fl.x0, fl.y0, 0.5f, fl.mask, fl.type, 1.f, fl.plate ? 1.f : 0.f, 0.f, 1.f, 0, 1.f, 1.f);
            continue;
        }
        const vec3 c = pal_.plate[fl.type];
        air.push_back({{fl.x, fl.y, 0.5f, 0.f}, {(float)fl.type, 1.f, 0.f, fl.rot}, {c.x, c.y, c.z, fl.s}, {0.f, 1.f, 0.f, 0.f}});
    }
    for (const Pop& p : pops_) {
        if (p.t < 0.f || p.kind > 1) continue;
        const float u = p.t / p.life;
        const float a = smoothstepf(0.f, 0.15f, u) * (1.f - smoothstepf(0.55f, 1.f, u));
        const float sz = p.size * (p.kind == 0 ? (0.8f + 0.3f * smoothstepf(0.f, 0.25f, u)) : (0.5f + 0.7f * std::sin(PI * u)));
        front_.push_back({{p.x, p.y, sz, sz}, {(float)(p.kind == 0 ? 5 : 6), p.kind == 1 ? u * 1.5f : 0.f, a, 0.f},
                          {p.col.x, p.col.y, p.col.z, 0.f}, {0.f, 0.f, 0.f, 0.f}});
    }

    // ---- Draw.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progBack_);
    set4f(progBack_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progBack_, "uWallTop", pal_.wallTop);
    set3f(progBack_, "uWallBot", pal_.wallBot);
    set3f(progBack_, "uWood", pal_.wood);
    set3f(progBack_, "uWoodDark", pal_.woodDark);
    set3f(progBack_, "uBelt", pal_.belt);
    set3f(progBack_, "uLacq", pal_.lacquer);
    set3f(progBack_, "uRim", pal_.rim);
    set3f(progBack_, "uWarmCol", pal_.warm);
    set1f(progBack_, "uBeltOff", beltOff_);
    set1f(progBack_, "uWarm", 0.25f * wp + 0.35f * fest_ + 0.2f * swell_);
    set1f(progBack_, "uBeltTop", BELT_TOP);
    set1f(progBack_, "uBeltBot", BELT_BOT);
    set1f(progBack_, "uBeltX0", beltStart());
    set4f(progBack_, "uBox", BOX_HX, BOX_HY, 0.f, BOX_RIM);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    auto spriteUniforms = [&]() {
        glUseProgram(progSprite_);
        set3f(progSprite_, "uLine", pal_.line);
        set4f(progSprite_, "uWell", BOX_HX + BOX_RIM, BOX_HY + BOX_RIM, 0.f, 0.f);
        set4f(progSprite_, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progSprite_, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        // the song's title and info (top left), the section and the universe (bottom left)
        const float top = viewC_.y + viewH_.y, bot = viewC_.y - viewH_.y;
        set4f(progSprite_, "uTitle", -viewH_.x, top - 2.4f, -viewH_.x + 11.f, top + 1.f);
        set4f(progSprite_, "uCorner", -viewH_.x, bot - 1.f, -viewH_.x + 7.f, bot + 2.6f);
    };
    spriteUniforms();
    drawInst(progSprite_, spriteVao_, back_);
    // the noren's lettering
    {
        const float x0 = 13.2f, pw = 2.55f, gap = 0.16f;
        const float top = viewC_.y + viewH_.y;
        if (x0 + 2 * pw + gap < viewH_.x - 1.5f) {
            std::vector<float> q;
            const float pxPerUnit = height_ / (2.f * viewH_.y);
            const float size = 1.15f * pxPerUnit;
            const char* kana[2] = {"\x01", "\x02"};
            const float rodY = top - 0.35f;
            for (int i = 0; i < 2; i++) {
                const float cx = x0 + pw * 0.5f + i * (pw + gap);
                const vec2 c = project(vec3(cx, rodY - 1.62f, 0.f));
                const float w = textWidth(fontB_, kana[i], size * 2.f, 0.f);
                (void)w;
                const Glyph& g = fontB_.g[glyphIndex((unsigned char)kana[i][0])];
                const float k = size / fontB_.px;
                const float gx = c.x - (g.ox + g.w * 0.5f) * k, gy = c.y - (g.oy + g.h * 0.5f) * k - fontB_.capH * k;
                textQuads(fontB_, kana[i], gx, gy, size, 0.f, q);
            }
            drawText(fontB_, q, pal_.norenText, pal_.noren, 1.f, 0.f);
        }
    }
    // the cats
    if (!catInst_.empty()) {
        glUseProgram(progCat_);
        setMat(progCat_, "uVP", vp_);
        set3f(progCat_, "uLine", pal_.line);
        set3f(progCat_, "uBlush", lin(0xff8fa8));
        set3f(progCat_, "uEye", lin(0x2b2230));
        set3f(progCat_, "uPink", lin(0xf7a6b8));
        set3f(progCat_, "uMouth", lin(0x8c2e44));
        set1f(progCat_, "uClipY", BELT_TOP);
        glBindVertexArray(catVao_);
        glBindBuffer(GL_ARRAY_BUFFER, catVbo_);
        glBufferData(GL_ARRAY_BUFFER, catInst_.size() * sizeof(CatInst), catInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)catInst_.size());
    }
    spriteUniforms();
    {
        // dishes first (pops are drawn after the maki)
        std::vector<Inst> dishes, fx;
        for (const Inst& in : front_) (in.b[0] > 6.5f ? dishes : fx).push_back(in);
        drawInst(progSprite_, spriteVao_, dishes);
        front_.swap(fx);
    }
    // the maki: the stack writes the scene's alpha
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progMaki_);
    set3f(progMaki_, "uNori", lin(0x1f2b26));
    set3f(progMaki_, "uRice", lin(0xfdf9f1));
    set3f(progMaki_, "uLine", pal_.line);
    drawInst(progMaki_, makiVao_, makis_);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUseProgram(progMaki_);
    drawInst(progMaki_, makiVao_, air);
    spriteUniforms();
    drawInst(progSprite_, spriteVao_, front_);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // ---- The frame.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set3f(progFinal_, "uDusk", pal_.night > 0.5f ? vec3(0.78f, 0.74f, 0.95f) : vec3(0.62f, 0.6f, 0.86f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawPops();
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_sushi() {
    static const UniverseEntry e{
        "SUSHI", "maki on little plates in a bento box at a kaiten counter; cleared rows hop to kawaii cats who eat them",
        [] { return std::unique_ptr<Universe>(new SushiUniverse()); },
        // A cheerful world: bright, mid-tempo songs suit it best.
        [](const SongLook& l) { return 0.6f + 0.35f * l.brightness + 0.15f * (1.f - l.bassWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

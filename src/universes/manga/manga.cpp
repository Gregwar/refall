// MANGA: a manga's color page, crisp ink lines and screentones over washes of color.
//
// One world: a printed page, like the color pages at the start of a volume. The well is the tall panel in the
// middle of a grid of panels and white gutters; the HUD sits in its own quiet panels beside it (the held piece, the
// score, the next pieces, the bonus gauge), and the outer columns and two small panels under the HUD hold the
// drawings of an evening in a seaside town (art.cpp): the town at dusk from a rooftop, a lighthouse on a cliff, a
// railway crossing, stairs climbing to a shrine gate against the setting sun, a wind chime, a vending machine
// glowing in the dark. Everything is ink on paper: lines with tapered ends, solid blacks, white knock-outs, and
// screentones (halftone dots, gradients of dots, line tones, crosshatch), the tones anchored on the page and
// antialiased per pixel, their cells never smaller than a few pixels; under the ink, soft washes of color from the
// variant's palette (the sky's gradient from the zenith to the glow on the horizon, warm windows, the teal sea, the
// green trees, the vermilion gate...), gently textured like watercolor, the screentones printed over them in darker
// shades of their colors.
//
// The page is drawn as the song goes on: it starts as a pencil draft in non-photo blue over the colorist's faint
// flats (each panel's ground), inside panel frames inked by hand (each edge one brush stroke: its width swelling and
// thinning, a slight bow and wobble, running past the corners into a dry-brush flick, now and then a blot of ink
// where the brush came down; seeded, so they hold still); every cleared row inks the next element of the drawings
// (its lines drawn along their length, then its tones laid down as their dots grow), then its washes flood in under
// the ink through a noise with a wet edge, like a colorist working after the inker, panel by panel, so the page fills
// up over the song and keeps it.
//
// The pieces are plain, flat blocks of color, each of the seven types its own clean color: faint seams between its
// cells and one bold line of the same brush around each piece (a stroke per straight run of its outline, seeded by
// its place in the piece's shape, so it holds as the piece falls and locks). The falling piece is a locked one; the
// ghost is its draft in grey graphite: a loose outline gone over twice, light hatching inside. A hard drop draws a few short impact lines
// at the piece's feet. A cleared row breaks up into its color's dots, which burst out of the well sideways and fade
// (0.5 s, real time even in slow motion).
//
// The song: calm is the still page; mid sets the crossing's lamps glowing red in turn on the two-beat grid ("kan"
// lettered in red by the lit one) and the wind chime swaying; a peak adds the lighthouse's warm beam sweeping slowly
// over the sea and short focus lines reaching in from the page's edges. **The drop is one of six manga moments**
// (drops.cpp), never the same twice in a row: SPLASH (the drawings fade back to the paper, the frames of their
// panels break apart and drift away, a burst of color floods the page from the well out, focus lines burst in from
// the page's edges toward the well and a huge "DON!" is lettered down the left column in a bold color, a rumble of
// "GO" letters building up on the right on the two-beat grid, a few focus lines lunging in with it), DASH, PANELS,
// IMPACT, SHOJO and BARRAGE. After, the panels come back. A quad inks four elements, bursts color and focus lines
// in and letters "ZUDON!" over the right column, its toast in a spiky balloon. Slow motion is a quiet moment: a deep blue night settles over
// the drawings, pink petals drift down through them and "SHIIN" (the sound of silence) is lettered in outline. Game
// over turns the stack back to pencil from the top, then erases it. Around a song the drawing panels are ruled in and
// drawn out of the blank paper one after the other, and after it each goes dark in turn, the ink coming down over it.
//
// Variants: SHONEN (a bright summer evening), NEWSPRINT (a Sunday comic: muted inks printed in CMYK halftone on
// yellowed paper), AKA (dusk reds), YORU (the night page: pale ink on deep blues, warm windows and neon glows) and
// AI (an indigo watercolor).
//
// Cost: a few thousand triangles of ink and washes and some text into one 4x multisampled target, resolved once.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "title.hpp"
#include "universes/manga/manga.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/manga/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using manga::V2;

namespace {

// Comic Neue Bold (the HUD's lettering), Bangers (its numbers and shouts) and Dela Gothic One (the sound effects,
// in katakana), SIL Open Font License, see OFL-ComicNeue.txt, OFL-Bangers.txt and OFL-DelaGothicOne.txt.
const unsigned char FONT_COMIC[] = {
#include "universes/manga/ComicNeue-Bold.ttf.inc"
};
const unsigned char FONT_BANGERS[] = {
#include "universes/manga/Bangers-Regular.ttf.inc"
};
const unsigned char FONT_DELA[] = {
#include "universes/manga/DelaGothicOne-Regular.ttf.inc"
};

// The HUD's comic lettering: the letters, an ink line around them and a paper rim around that, all crisp (Look's
// outline and halo give their colors and widths; the rim starts where the line ends).
const text::Lettering INKED{R"(
vec4 letter(Letter L) {
    float fw = max(fwidth(L.dist), 1e-3);
    float lw = uOutline.a > 0.0 ? uOutlineW * L.em : 0.0;
    float cf = clamp(L.dist / fw + 0.5, 0.0, 1.0);
    float cl = uOutline.a > 0.0 ? clamp((L.dist + lw) / fw + 0.5, 0.0, 1.0) : 0.0;
    float ch = uHalo.a > 0.0 ? clamp((L.dist + lw + uHaloShape.x * L.em) / fw + 0.5, 0.0, 1.0) : 0.0;
    vec4 c = vec4(uHalo.rgb, 1.0) * uHalo.a * ch;
    vec4 l = vec4(uOutline.rgb, 1.0) * uOutline.a * cl;
    c = c * (1.0 - l.a) + l;
    vec4 f = vec4(L.ink.rgb, 1.0) * cf;
    c = c * (1.0 - f.a) + f;
    return c * L.ink.a;
}
)"};

namespace shaders = manga_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A variant: its paper, ink, spot (the HUD's accent) and pencil, its screentone scale, whether its washes are
// printed in halftone, then its palette (manga::Hue's order), its seven piece colors (I O T S Z J L), the drop's
// burst (center, middle, edge) and the sound effects' fill.
struct VariantDef {
    const char* name;
    unsigned paper, ink, spot, pencil;
    float toneK; // screentone cell scale
    bool print;
    unsigned hue[manga::H_COUNT];
    unsigned piece[7];
    unsigned burst[3];
    unsigned sfx;
};
const VariantDef VARIANTS[] = {
    // a bright summer evening: a blue sky through violet to an orange glow, warm windows, a teal sea
    {"SHONEN", 0xf7f6f2, 0x141414, 0xe8452c, 0x8fb9de, 1.f, false,
     {0x3553a6, 0xa985c9, 0xffb877, 0xffd66e, 0xfff6dc, 0xffd2b4, 0xa992cf, 0xffd25e, 0x8f86c0, 0x2c2c5e, 0x5b5a98,
      0x9a92b4, 0xd6c6b2, 0xf9c932, 0xe8392f, 0x22306a, 0xc8f6ff, 0x2f7fd8, 0x3fb7bd, 0x1d6c8c, 0x4d3d66, 0x74b84e,
      0x2e6e4e, 0xe8452c, 0x7a5240, 0xc4f0f2, 0x9fd3f2},
     {0x37b8e8, 0xffcb2e, 0x9a5ed6, 0x4cbc5c, 0xec4a3e, 0x3070dc, 0xf88c24},
     {0xfff3b0, 0xffb347, 0xf0503a}, 0xffd23a},
    // a Sunday comic: muted inks printed in halftone dots on yellowed paper
    {"NEWSPRINT", 0xe8e1cf, 0x2a2420, 0xc8463a, 0x8eabc6, 1.3f, true,
     {0x4a74a8, 0xa894b8, 0xf2b57a, 0xf4cc52, 0xf4ead0, 0xf2cfb2, 0xa89cbc, 0xf2cc58, 0x9a98b8, 0x3c3c5c, 0x6e6c90,
      0xa4a0ac, 0xcdbc9e, 0xf0c63e, 0xd84a3a, 0x2e3a5a, 0xb4e4ec, 0xc8463a, 0x58a8b4, 0x2e6e8e, 0x4c4254, 0x80a85c,
      0x3e6e52, 0xd84e34, 0x74523e, 0xc8e8ea, 0xe8a89c},
     {0x5aaed0, 0xeec64c, 0x9c72ac, 0x70aa62, 0xd45a4c, 0x4c70b0, 0xe08e48},
     {0xf8e6a8, 0xf0a850, 0xd4503c}, 0xf0c63e},
    // dusk reds: a wine sky burning down to orange, a red sea, the gate aflame
    {"AKA", 0xf7f5f0, 0x151515, 0xd0302a, 0x8fb9de, 1.f, false,
     {0x4e2148, 0xc2474a, 0xf9a05a, 0xffd982, 0xffeed8, 0xffbf94, 0x9a4766, 0xffd06a, 0x9c5068, 0x34152a, 0x6e2c44,
      0x9a6a70, 0xd4a890, 0xf4b638, 0xff3d2e, 0x2c1228, 0xffe4b0, 0xd03a2c, 0xd8745e, 0x6e2840, 0x3a1828, 0x9c9a42,
      0x3e2a2e, 0xe63c22, 0x5a2a24, 0xffd8c4, 0xf6d482},
     {0x2fa8a0, 0xf6c233, 0x8e3f94, 0x7aa23e, 0xd92e28, 0x2f4f92, 0xf07a2a},
     {0xffe7a0, 0xff8a3a, 0xc8242a}, 0xffd24a},
    // the night page: deep blues, pale ink, warm windows and neon glows
    {"YORU", 0x101320, 0xe2e8f4, 0x5af0ff, 0x4f6c8a, 1.f, false,
     {0x080c22, 0x1a2458, 0x4a3c8e, 0xf2ead0, 0xf0eedc, 0x3a4a86, 0x1c2552, 0xffc84a, 0x243266, 0x0c1028, 0x2a3a78,
      0x1e2448, 0x3c4878, 0xe8c040, 0xff3c5c, 0x0c1230, 0x5af0ff, 0xff4fa0, 0x1c4c80, 0x0a1a40, 0x0a0e20, 0x2a6a6a,
      0x0e2a34, 0xff4a3a, 0x1c1c38, 0x7ad8ff, 0xff70b8},
     {0x3ed2ec, 0xffd34c, 0xb46ef2, 0x52d88c, 0xff5a6e, 0x5a82ff, 0xff9a3c},
     {0x9af6ff, 0x6a5cff, 0xff4fa0}, 0x5af0ff},
    // indigo: a blue watercolor, the windows and the gate its only warm notes
    {"AI", 0xf4f4f1, 0x171c30, 0x2d68cc, 0x93b8db, 1.f, false,
     {0x22357a, 0x5d80c8, 0xbcd2ee, 0xf6dca0, 0xf2f6ff, 0xe2eafa, 0x8aa2d6, 0xf6d17e, 0x7e94c8, 0x1c2858, 0x3e5498,
      0x9eaacc, 0xbcc6de, 0xe9cc72, 0xd44a5c, 0x1a2552, 0xdcf4ff, 0x3a62b8, 0x4080c2, 0x1c3c80, 0x202c5c, 0x5c9ca2,
      0x1e3e68, 0xd44c3c, 0x2c3668, 0xd0e6ff, 0x90b4ea},
     {0x5ab6e4, 0xf0c654, 0x7a68d2, 0x48a89c, 0xd8566a, 0x2a48a6, 0xe8955a},
     {0xe8f2ff, 0x7aa4f0, 0x2a40a0}, 0xf6d17e},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float INK_S = 1.1f;    // one element's inking (lines, then tones; its washes flood in after)
constexpr float WASH0 = 0.75f, WASH1 = 2.f; // the washes, in INK_S
constexpr float FADE_S = 0.26f;  // a cleared cell breaking up
constexpr float CRUMB_S = 0.55f;

float ease(float t) {
    t = saturate(t);
    return t * t * (3.f - 2.f * t);
}
float easeOut(float t) {
    t = saturate(t);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}

// UTF-8 to codepoints.
std::vector<int> decode(const std::string& s) {
    std::vector<int> v;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        int cp = '?', n = 1;
        if (c < 0x80) cp = c;
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); n = 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) { cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F); n = 3; }
        else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) { cp = '?'; n = 4; }
        v.push_back(cp);
        i += n;
    }
    return v;
}

// The sound effects (katakana).
const char* SFX_ZUDON = "\xE3\x82\xBA\xE3\x83\x89\xE3\x83\xB3\xEF\xBC\x81"; // ズドン！
const char* SFX_KAN = "\xE3\x82\xAB\xE3\x83\xB3";                         // カン
const char* SFX_SHIIN = "\xE3\x82\xB7\xE3\x83\xBC\xE3\x83\xB3";           // シーン


} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MangaUniverse::init(int width, int height) {
    progInk_ = ugl::program(shaders::INK_VS, shaders::INK_FS, "manga ink");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "manga text");
    progPaper_ = ugl::program(ugl::FULLSCREEN_VS, shaders::PAPER_FS, "manga paper");

    glGenVertexArrays(1, &inkVao_);
    glGenBuffers(1, &inkVbo_);
    glBindVertexArray(inkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, inkVbo_);
    const int stride = sizeof(float) * 14;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 10));

    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    // (the sound effects' letters: the HUD is lettered by the host)
    std::vector<int> kana = {'!', 0xFF01, 0x2026};
    for (int c = 0x30A0; c <= 0x30FF; c++) kana.push_back(c);
    buildFont(dela_, FONT_DELA, 112.f, kana);

    GLint maxS = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples_ = std::max(1, std::min(4, (int)maxS));
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

const MangaUniverse::Glyph* MangaUniverse::Font::find(int cp) const {
    auto it = std::lower_bound(g.begin(), g.end(), cp, [](const Glyph& a, int c) { return a.cp < c; });
    if (it != g.end() && it->cp == cp) return &*it;
    return nullptr;
}

void MangaUniverse::buildFont(Font& f, const unsigned char* ttf, float pxSize, const std::vector<int>& cps) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] manga font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = pxSize;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1) && y1 > 0) f.capH = y1 * f.scaleTT;
    else if (stbtt_GetCodepointBox(info, 0x30C9, &x0, &y0, &x1, &y1)) f.capH = (y1 - y0) * f.scaleTT * 0.8f;
    const int PAD = (int)(f.px / 7.f);
    f.pad = (float)PAD;
    const int AW = 2048;
    // first pass: the glyphs' SDFs, packed in rows
    struct Bmp { int cp, w, h, xo, yo, x, y; unsigned char* data; float adv; };
    std::vector<Bmp> bmps;
    int x = 1, y = 1, rowH = 0;
    for (int cp : cps) {
        if (!stbtt_FindGlyphIndex(info, cp)) continue;
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        Bmp b{cp, 0, 0, 0, 0, 0, 0, nullptr, adv * f.scaleTT};
        b.data = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &b.w, &b.h, &b.xo, &b.yo);
        if (b.data) {
            if (x + b.w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
            b.x = x;
            b.y = y;
            x += b.w + 1;
            rowH = std::max(rowH, b.h);
        }
        bmps.push_back(b);
    }
    int AH = 64;
    while (AH < y + rowH + 2) AH *= 2;
    std::vector<unsigned char> atlas((size_t)AW * AH, 0);
    for (Bmp& b : bmps) {
        Glyph g{b.cp, 0, 0, 0, 0, 0, 0, 0, 0, b.adv};
        if (b.data) {
            for (int r = 0; r < b.h; r++) std::memcpy(&atlas[(size_t)(b.y + r) * AW + b.x], b.data + r * b.w, b.w);
            g.x0 = b.x / (float)AW; g.y0 = b.y / (float)AH; g.x1 = (b.x + b.w) / (float)AW; g.y1 = (b.y + b.h) / (float)AH;
            g.ox = (float)b.xo; g.oy = (float)b.yo; g.w = (float)b.w; g.h = (float)b.h;
            stbtt_FreeSDF(b.data, nullptr);
        }
        f.g.push_back(g);
    }
    std::sort(f.g.begin(), f.g.end(), [](const Glyph& a, const Glyph& b) { return a.cp < b.cp; });
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

void MangaUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    unitPx_ = height_ / (2.f * hh);
    s_ = height_ / 1080.f;

    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msColor_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA8, width_, height_);
    glGenFramebuffers(1, &msFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] manga: incomplete target\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // the page follows the screen (its inking state kept): the score's panel reaches under the speed gauge's label,
    // set by the host at 260 px (x its scale, max(1, height / 900)) under the score's anchor
    {
        const float hostS = std::max(1.f, height_ / 900.f);
        const float bottomPx = project(layout::SCORE_ANCHOR).y + 284.f * hostS;
        scoreBottom_ = viewC_.y + (height_ * 0.5f - bottomPx) / unitPx_ - 0.3f;
    }
    const size_t n = page_.elems.size();
    page_ = manga::buildPage(viewH_.x, scoreBottom_, seed_);
    if (page_.elems.size() != n) {
        inkT_.assign(page_.elems.size(), -1.f);
        nextInk_ = 0;
    }
    setClips();
}

MangaUniverse::~MangaUniverse() {
    for (GLuint p : {progInk_, progText_, progPaper_}) glDeleteProgram(p);
    for (GLuint v : {inkVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {inkVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (dela_.tex) glDeleteTextures(1, &dela_.tex);
    delete (stbtt_fontinfo*)dela_.info;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MangaUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        static const int LIGHT[3] = {0, 2, 4};
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;
        else if (t.hasSong && t.brightness < 0.4f && t.dynamics < 0.5f) v = 1;
        else v = LIGHT[(int)(h * 3) % 3];
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    // the inks and the palette: set at once on the first song, eased into on a new one
    tgt_[K_PAPER] = hex(V.paper);
    tgt_[K_INK] = hex(V.ink);
    tgt_[K_SPOT] = hex(V.spot);
    tgt_[K_PENCIL] = hex(V.pencil);
    for (int i = 0; i < manga::H_COUNT; i++) tgt_[K_HUE + i] = hex(V.hue[i]);
    for (int i = 0; i < 7; i++) tgt_[K_PIECE + i] = hex(V.piece[i]);
    for (int i = 0; i < 3; i++) tgt_[K_BURST + i] = hex(V.burst[i]);
    tgt_[K_SFX] = hex(V.sfx);
    colorRate_ = seconds > 0.1f ? 2.5f / std::clamp(seconds, 0.5f, 3.f) : -1.f;
    if (colorRate_ < 0.f)
        for (int i = 0; i < K_COUNT; i++) cur_[i] = tgt_[i];
    toneK_ = V.toneK;
    print_ = V.print;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x3A16Aull);
    // a new song: a fresh page in pencil (the ink of the last one fades off first)
    inkQ_.clear();
    sfx_.clear();
    if (seconds > 0.1f && nextInk_ > 0) {
        resetT_ = 0.f;
        resetDur_ = std::clamp(seconds, 0.5f, 3.f);
        return;
    }
    resetPage();
}

void MangaUniverse::resetPage() {
    resetT_ = -1.f;
    page_ = manga::buildPage(viewH_.x, scoreBottom_, seed_);
    inkT_.assign(page_.elems.size(), -1.f);
    nextInk_ = 0;
    setClips();
}

void MangaUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)impact;
    level = std::clamp(level, 0, 2);
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, 1.f, 5.f);
}

// The drop's downbeat: the splash starts (render checks it is a real drop, not a chorus).
void MangaUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
    dropT_ = 0.f;
    donShown_ = false;
}

vec2 MangaUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MangaUniverse::hints() const {
    TransitionHints h;
    h.background = ugl::toLinear(paper_);
    h.ink = ugl::toLinear(ink_);
    h.accent = ugl::toLinear(spot_);
    h.light = ugl::luma(paper_) > 0.4f;
    h.styles = TS_INK | TS_CURTAIN | TS_TILES;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void MangaUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) ids_[y][x] = 0;
    // cells found without an id: groups of up to four of the same type become a piece
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

void MangaUniverse::inkNext(float delay) { inkQ_.push_back(delay); }

void MangaUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        // short impact lines at the piece's feet, both sides
        float x0 = 99, x1 = -99, yl = 99;
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            x0 = std::min(x0, p.x - 0.5f);
            x1 = std::max(x1, p.x + 0.5f);
            yl = std::min(yl, p.y - 0.5f);
        }
        if (x1 < x0) break;
        for (int side = 0; side < 2; side++) {
            const float sx = side ? 1.f : -1.f, cx = side ? x1 : x0;
            for (int i = 0; i < 3; i++) {
                const float a = (side ? 0.f : PI) + sx * (0.35f - 0.3f * i);
                const V2 o{cx + sx * 0.08f, yl + 0.06f};
                impacts_.push_back({{o.x + std::cos(a) * 0.15f, o.y + std::sin(a) * 0.15f}, {o.x + std::cos(a) * (0.55f + 0.12f * (i == 1)), o.y + std::sin(a) * (0.55f + 0.12f * (i == 1))}, 0.f});
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the cells break up into their dots, bursting out sideways
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            fades_.push_back({c.x, c.y, std::clamp(c.type, 0, 6), 0.f, 0});
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const float side = p.x < 0.f ? -1.f : 1.f;
            for (int i = 0; i < 5; i++) {
                Crumb k;
                k.p = {p.x + rng_.range(-0.4f, 0.4f), p.y + rng_.range(-0.4f, 0.4f)};
                k.v = {side * rng_.range(3.f, 10.f) * (0.5f + std::fabs(p.x) / 5.f), rng_.range(-2.2f, 2.6f)};
                k.r = rng_.range(0.05f, 0.12f);
                k.t = 0.f;
                k.life = CRUMB_S * rng_.range(0.75f, 1.25f);
                k.type = std::clamp(c.type, 0, 6);
                crumbs_.push_back(k);
            }
        }
        // compact the ids like the game did
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
        // every cleared row inks the next element of the page
        for (int i = 0; i < ev.count; i++) inkNext(0.15f + 0.3f * i);
        if (ev.count >= 4) {
            quadT_ = 0.f;
            addSfx(SFX_ZUDON, {16.3f, 0.6f}, 1.85f, -0.12f, 2.6f, false, SFX_COLOR, 0.3f);
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack turns back to pencil from the top, then is erased
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            fades_.push_back({c.x, c.y, std::clamp(c.type, 0, 6), -0.4f - 0.07f * (c.y - Game::HIDDEN), 1});
        }
        break;
    default: break;
    }
}

MangaUniverse::Sfx& MangaUniverse::addSfx(const std::string& text, V2 at, float size, float angle, float life, bool vertical, int style, float in) {
    Sfx s{text, at, size, angle, 0.f, life, in, vertical, style};
    sfx_.push_back(s);
    return sfx_.back();
}

// =====================================================================================================
// Ink
// =====================================================================================================

vec2 MangaUniverse::px(const V2& b) const {
    return {(b.x - viewC_.x) * unitPx_ + width_ * 0.5f, height_ * 0.5f - (b.y - viewC_.y) * unitPx_};
}

void MangaUniverse::vtx(float x, float y, float dens, const ToneP& t, const vec3& c, float a) {
    const float v[14] = {x, y, (float)t.kind, dens, t.cell, t.angle, t.origin.x, t.origin.y, (float)t.clip, 0.f, c.x, c.y, c.z, a};
    verts_.insert(verts_.end(), v, v + 14);
}

void MangaUniverse::tri(const V2& a, const V2& b, const V2& c, const ToneP& t, float da, float db, float dc, const vec3& col, float al) {
    vtx(a.x, a.y, da, t, col, al);
    vtx(b.x, b.y, db, t, col, al);
    vtx(c.x, c.y, dc, t, col, al);
}

void MangaUniverse::triC(const V2& a, const V2& b, const V2& c, const ToneP& t, float d, const vec3* cols, const float* als) {
    vtx(a.x, a.y, d, t, cols[0], als[0]);
    vtx(b.x, b.y, d, t, cols[1], als[1]);
    vtx(c.x, c.y, d, t, cols[2], als[2]);
}

// A screentone printed over a color: a darker, a little richer shade of it.
vec3 MangaUniverse::shadeOf(const vec3& c) {
    const vec3 d = c * 0.58f;
    const float l = ugl::luma(d);
    return vec3(std::max(0.f, l + (d.x - l) * 1.2f), std::max(0.f, l + (d.y - l) * 1.2f), std::max(0.f, l + (d.z - l) * 1.2f));
}

void MangaUniverse::rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    if (a <= 0.002f || x1 <= x0 || y1 <= y0) return;
    const ToneP t{manga::TONE_SOLID, 4.f, 0.f, {0, 0}, -1};
    tri({x0, y0}, {x1, y0}, {x1, y1}, t, 1, 1, 1, c, a);
    tri({x0, y0}, {x1, y1}, {x0, y1}, t, 1, 1, 1, c, a);
}

void MangaUniverse::discPx(float cx, float cy, float r, const vec3& c, float a, int clip) {
    if (a <= 0.002f || r <= 0.1f) return;
    const ToneP t{manga::TONE_DISC, r, 0.f, {cx, cy}, clip};
    const float e = r + 1.5f;
    tri({cx - e, cy - e}, {cx + e, cy - e}, {cx + e, cy + e}, t, 1, 1, 1, c, a);
    tri({cx - e, cy - e}, {cx + e, cy + e}, {cx - e, cy + e}, t, 1, 1, 1, c, a);
}

namespace {
// A smooth noise along a line, -1..1 (values on integer knots, eased between them).
float wave(uint64_t seed, float x) {
    const float i = std::floor(x), f = x - i;
    auto knot = [&](float k) { return (float)(splitmix64(seed ^ (uint64_t)(int64_t)k * 0x9E3779B97F4A7C15ull) >> 40) / 8388608.f - 1.f; };
    return lerpf(knot(i), knot(i + 1.f), f * f * (3.f - 2.f * f));
}
} // namespace

// A hand-inked line: the brush pressed in a little before a (now and then pooling into a blot), its width swelling
// and thinning along the line, a slight bow and wobble, running on past b and lifting off in a dry-brush flick (three
// strands, each thinning out on its own). Its shape is the seed's and its noise runs along the line in pixels, so it
// holds still from frame to frame, and a part of it (from, to: 0..1 along a to b, the run-ups past the ends included
// at 0 and 1) lies exactly on the whole. loose: how freely it is drawn (1: a panel's frame; the pieces' outlines
// keep closer to their edges).
void MangaUniverse::brushPx(vec2 a, vec2 b, float widthPx, uint64_t seed, float from, float to, float loose, const vec3& col, float alpha,
                            int clip) {
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    if (alpha <= 0.002f || to <= from || len < 0.5f) return;
    const V2 d{(b.x - a.x) / len, (b.y - a.y) / len}, nrm{-d.y, d.x};
    const float w = std::max(1.f, widthPx * s_);
    Rng r(seed);
    // the run-ups past the ends cover the corners, where the next edge's stroke starts
    const float o0 = w * (0.5f + 0.8f * loose * r.uniform()), o1 = w * (0.6f + 2.6f * loose * r.uniform());
    // a slight bow and tilt off the straight line, a slow wobble and a faster tremor of the hand along it
    const float bow = r.range(-1.f, 1.f) * loose * std::min(3.f * s_, len * 0.004f);
    const float tilt = r.range(-1.f, 1.f) * loose * 1.6f * s_;
    const bool blot = r.chance(0.3f * loose * loose);
    float lift[3];
    for (float& l : lift) l = r.range(0.3f, 0.9f);
    lift[1] = 1.f;
    const uint64_t ws = r.next();
    const float s0 = from <= 0.f ? -o0 : from * len, s1 = to >= 1.f ? len + o1 : to * len;
    auto at = [&](float s, float side) {
        const float u = s / len;
        const float off = bow * std::sin(PI * saturate(u)) + tilt * (u - 0.5f) + 0.6f * s_ * wave(ws, s / (60.f * s_)) +
                          0.3f * s_ * wave(ws + 3, s / (24.f * s_)) + side;
        return V2{a.x + d.x * s + nrm.x * off, a.y + d.y * s + nrm.y * off};
    };
    // its width: the pressure swelling and easing along it, the bristles' grain; lighter as the brush comes down
    auto width = [&](float s) {
        const float k = w * (0.8f + 0.28f * wave(ws + 1, s / (40.f * s_))) * (1.f + 0.14f * loose * wave(ws + 2, s / (22.f * s_)));
        return s < 0.f ? k * lerpf(0.7f, 1.f, (s + o0) / o0) : k;
    };
    const ToneP t{manga::TONE_SOLID, 4.f, 0.f, {0, 0}, clip};
    // a run of the line [u0, u1], `side` off its middle, its width wf(s)
    auto ribbon = [&](float u0, float u1, auto side, auto wf) {
        const int n = std::max(3, (int)std::ceil((u1 - u0) / (10.f * s_)));
        V2 p0 = at(u0, side(u0));
        float w0 = 0.5f * wf(u0);
        for (int i = 1; i <= n; i++) {
            const float u = lerpf(u0, u1, i / (float)n);
            const V2 p1 = at(u, side(u));
            const float w1 = 0.5f * wf(u);
            tri(p0 + nrm * w0, p1 + nrm * w1, p1 - nrm * w1, t, 1, 1, 1, col, alpha);
            tri(p0 + nrm * w0, p1 - nrm * w1, p0 - nrm * w0, t, 1, 1, 1, col, alpha);
            p0 = p1;
            w0 = w1;
        }
    };
    // the body, up to b
    if (std::min(s1, len) > s0) ribbon(s0, std::min(s1, len), [](float) { return 0.f; }, width);
    // the flick past b: three strands side by side, splaying a little, each lifting off on its own
    if (s1 > len) {
        const float wb = width(len);
        for (int k = 0; k < 3; k++) {
            const float end = len + o1 * lift[k];
            if (std::min(s1, end) <= len) continue;
            auto q = [&](float s) { return (s - len) / (end - len); };
            ribbon(len, std::min(s1, end), [&](float s) { return (k - 1) * wb / 3.f * (1.f + 0.7f * q(s)); },
                   [&](float s) { return wb * 0.38f * (1.f - q(s)); });
        }
    }
    // the ink pooled where the brush came down
    if (blot && from <= 0.f) {
        const V2 c = at(-0.4f * o0, 0.f);
        const float rb = w * 0.62f;
        for (int i = 0; i < 10; i++) {
            const float a0 = i / 10.f * TAU, a1 = (i + 1) / 10.f * TAU;
            tri(c, c + V2{std::cos(a0), std::sin(a0)} * rb, c + V2{std::cos(a1), std::sin(a1)} * rb, t, 1, 1, 1, col, alpha);
        }
    }
}

void MangaUniverse::inkQuad(const V2* q, float widthPx, uint64_t seed, float upTo, const vec3& col, float a) {
    for (int e = 0; e < 4; e++) {
        const float k = saturate(upTo * 4.f - e);
        if (k <= 0.f) break;
        // each edge drawn one way or the other, so the flicks fall on different corners
        const uint64_t es = splitmix64(seed * 4 + e);
        vec2 A = px(q[e]), B = px(q[(e + 1) % 4]);
        if (es & 1) std::swap(A, B);
        brushPx(A, B, widthPx, es, 0.f, k, 1.f, col, a, -1);
    }
}

namespace {
V2 turn(const V2& p, const V2* pivot, float a) {
    if (!pivot || a == 0.f) return p;
    const float c = std::cos(a), s = std::sin(a);
    const V2 d = p - *pivot;
    return {pivot->x + c * d.x - s * d.y, pivot->y + s * d.x + c * d.y};
}
} // namespace

void MangaUniverse::fillPoly(const std::vector<V2>& pts, const manga::Tone& tone, float densK, const vec3& col, float a, int clip,
                             bool strip, const V2* pivot, float rotA) {
    if (pts.size() < 3 || a <= 0.002f) return;
    const vec2 o = px({0.f, 0.f});
    ToneP t{tone.kind, std::max(3.2f, tone.cell * toneK_ * unitPx_), tone.angle, o, clip};
    if (tone.kind == manga::TONE_SOLID) t.cell = 4.f;
    const V2 g = tone.g1 - tone.g0;
    const float gl = std::max(1e-6f, g.x * g.x + g.y * g.y);
    auto along = [&](const V2& p) { return saturate(((p.x - tone.g0.x) * g.x + (p.y - tone.g0.y) * g.y) / gl); };
    auto dens = [&](const V2& p) { return lerpf(tone.d0, tone.d1, along(p)) * densK; };
    if (tone.kind == manga::TONE_WASH) {
        // a wash: its colors and opacity per vertex, its density the flood's progress
        const vec3 c0 = this->col(tone.color), c1 = tone.color1 >= 0 ? this->col(tone.color1) : c0;
        std::vector<V2> P(pts.size());
        std::vector<vec3> C(pts.size());
        std::vector<float> A(pts.size());
        V2 c{0, 0}, cb{0, 0};
        for (size_t i = 0; i < pts.size(); i++) {
            const vec2 q = px(turn(pts[i], pivot, rotA));
            P[i] = {q.x, q.y};
            const float u = along(pts[i]);
            C[i] = lerp(c0, c1, u);
            A[i] = a * lerpf(tone.d0, tone.d1, u);
            c = c + P[i];
            cb = cb + pts[i];
        }
        if (strip) {
            for (size_t i = 0; i + 3 < P.size(); i += 2) {
                const vec3 ca[3] = {C[i], C[i + 1], C[i + 2]}, cc[3] = {C[i + 1], C[i + 3], C[i + 2]};
                const float aa[3] = {A[i], A[i + 1], A[i + 2]}, ac[3] = {A[i + 1], A[i + 3], A[i + 2]};
                triC(P[i], P[i + 1], P[i + 2], t, densK, ca, aa);
                triC(P[i + 1], P[i + 3], P[i + 2], t, densK, cc, ac);
            }
            return;
        }
        c = c * (1.f / P.size());
        cb = cb * (1.f / P.size());
        const float uc = along(cb);
        const vec3 cc = lerp(c0, c1, uc);
        const float acen = a * lerpf(tone.d0, tone.d1, uc);
        for (size_t i = 0; i < P.size(); i++) {
            const size_t j = (i + 1) % P.size();
            const vec3 cs[3] = {cc, C[i], C[j]};
            const float as[3] = {acen, A[i], A[j]};
            triC(c, P[i], P[j], t, densK, cs, as);
        }
        return;
    }
    std::vector<V2> P(pts.size());
    std::vector<float> D(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        const V2 b = turn(pts[i], pivot, rotA);
        const vec2 q = px(b);
        P[i] = {q.x, q.y};
        D[i] = dens(pts[i]);
    }
    if (strip) {
        for (size_t i = 0; i + 3 < P.size(); i += 2) {
            tri(P[i], P[i + 1], P[i + 2], t, D[i], D[i + 1], D[i + 2], col, a);
            tri(P[i + 1], P[i + 3], P[i + 2], t, D[i + 1], D[i + 3], D[i + 2], col, a);
        }
        return;
    }
    V2 c{0, 0}, cb{0, 0};
    for (size_t i = 0; i < P.size(); i++) { c = c + P[i]; cb = cb + pts[i]; }
    c = c * (1.f / P.size());
    cb = cb * (1.f / P.size());
    const float dc = dens(cb);
    for (size_t i = 0; i < P.size(); i++) {
        const size_t j = (i + 1) % P.size();
        tri(c, P[i], P[j], t, dc, D[i], D[j], col, a);
    }
}

void MangaUniverse::strokeLine(const std::vector<V2>& pts, float widthPx, float taper, bool closed, float upTo, const vec3& col, float a,
                               int clip, const V2* pivot, float rotA) {
    if (pts.size() < 2 || a <= 0.002f || upTo <= 0.f) return;
    std::vector<V2> P;
    for (const V2& b : pts) {
        const vec2 q = px(turn(b, pivot, rotA));
        P.push_back({q.x, q.y});
    }
    if (closed) P.push_back(P.front());
    const size_t n = P.size();
    std::vector<float> L(n, 0.f);
    for (size_t i = 1; i < n; i++) L[i] = L[i - 1] + std::hypot(P[i].x - P[i - 1].x, P[i].y - P[i - 1].y);
    const float total = L[n - 1];
    if (total < 0.5f) return;
    const float end = total * saturate(upTo);
    const ToneP t{manga::TONE_SOLID, 4.f, 0.f, {0, 0}, clip};
    float w0 = widthPx * s_;
    float alpha = a;
    if (w0 < 1.f) { alpha *= std::max(0.25f, w0); w0 = 1.f; }
    auto width = [&](float s) {
        if (taper <= 0.f) return w0;
        const float f = std::min(1.f, std::min(s, total - s) / (taper * total));
        return std::max(0.8f, w0 * (0.3f + 0.7f * std::sqrt(std::max(0.f, f))));
    };
    auto segDir = [&](size_t i) { // direction of segment i -> i+1
        const float dx = P[i + 1].x - P[i].x, dy = P[i + 1].y - P[i].y, l = std::max(1e-5f, std::hypot(dx, dy));
        return V2{dx / l, dy / l};
    };
    // the offset (normal times half width, with a miter) at point i
    auto offset = [&](size_t i, float w) {
        V2 d;
        if (i == 0) d = closed ? segDir(0) + segDir(n - 2) : segDir(0);
        else if (i == n - 1) d = closed ? segDir(0) + segDir(n - 2) : segDir(n - 2);
        else d = segDir(i - 1) + segDir(i);
        const float l = std::hypot(d.x, d.y);
        d = l > 1e-5f ? d * (1.f / l) : segDir(std::min(i, n - 2));
        const V2 nrm{-d.y, d.x};
        const V2 sd = segDir(std::min(i, n - 2));
        const float m = 1.f / std::max(0.35f, std::fabs(-sd.y * nrm.x + sd.x * nrm.y));
        return nrm * (0.5f * w * m);
    };
    for (size_t i = 0; i + 1 < n; i++) {
        if (L[i] >= end) break;
        V2 a0 = P[i], a1 = P[i + 1];
        float s1 = L[i + 1];
        bool cut = false;
        if (s1 > end) {
            const float k = (end - L[i]) / std::max(1e-5f, L[i + 1] - L[i]);
            a1 = {lerpf(a0.x, a1.x, k), lerpf(a0.y, a1.y, k)};
            s1 = end;
            cut = true;
        }
        V2 o0 = offset(i, width(L[i])), o1 = cut ? offset(i, width(s1)) : offset(i + 1, width(s1));
        // open ruled lines get square ends half a width out
        if (!closed && taper <= 0.f) {
            const V2 d = segDir(i);
            if (i == 0) a0 = a0 - d * (0.5f * w0);
            if (i + 2 == n && !cut) a1 = a1 + d * (0.5f * w0);
        }
        tri(a0 + o0, a1 + o1, a1 - o1, t, 1, 1, 1, col, alpha);
        tri(a0 + o0, a1 - o1, a0 - o0, t, 1, 1, 1, col, alpha);
    }
}

void MangaUniverse::flushInk() {
    if (verts_.empty()) return;
    glUseProgram(progInk_);
    set2f(progInk_, "uRes", (float)width_, (float)height_);
    glUniform3fv(ugl::U(progInk_, "uClip"), CLIP_SLOTS * 4, &clipEq_[0][0][0]);
    set1f(progInk_, "uScale", s_);
    set1f(progInk_, "uPrint", print_ ? 1.f : 0.f);
    ugl::set3f(progInk_, "uPaper", paper_);
    glBindVertexArray(inkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, inkVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(float), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts_.size() / 14));
    verts_.clear();
}

// Each panel's four edges as half-planes in pixels (inside: dot(n, p) + c >= 0).
void MangaUniverse::setClips() {
    for (int i = 0; i < CLIP_SLOTS; i++)
        for (int e = 0; e < 4; e++) {
            clipEq_[i][e][0] = clipEq_[i][e][1] = 0.f;
            clipEq_[i][e][2] = 1.f;
        }
    for (int i = 0; i < manga::P_COUNT; i++) setClip(i, page_.panels[i].c);
    elemBox_.clear();
}

void MangaUniverse::setClip(int i, const V2* c) {
    {
        for (int e = 0; e < 4; e++) {
            const vec2 a = px(c[e]), b = px(c[(e + 1) % 4]);
            // corners run clockwise on screen (y down): the inside is to the right of each edge
            float nx = -(b.y - a.y), ny = b.x - a.x;
            const float l = std::max(1e-5f, std::hypot(nx, ny));
            nx /= l;
            ny /= l;
            clipEq_[i][e][0] = nx;
            clipEq_[i][e][1] = ny;
            clipEq_[i][e][2] = -(nx * a.x + ny * a.y) + 0.5f;
        }
    }
}

// =====================================================================================================
// Lettering
// =====================================================================================================

float MangaUniverse::textWidth(const Font& f, const std::string& s, float capPx, float track) const {
    if (!f.ok) return s.size() * capPx * 0.7f;
    const float k = capPx / f.capH;
    const std::vector<int> cps = decode(s);
    float w = 0;
    for (size_t i = 0; i < cps.size(); i++) {
        const Glyph* g = f.find(cps[i]);
        if (!g) g = f.find('?');
        if (!g) continue;
        w += g->adv * k + (i + 1 < cps.size() ? track * capPx : 0.f);
        if (i + 1 < cps.size()) w += stbtt_GetCodepointKernAdvance((const stbtt_fontinfo*)f.info, cps[i], cps[i + 1]) * f.scaleTT * k;
    }
    return w;
}

void MangaUniverse::textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track, float angle,
                              std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = capPx / f.capH;
    const float ca = std::cos(angle), sa = std::sin(angle);
    const std::vector<int> cps = decode(s);
    float pen = 0.f;
    for (size_t i = 0; i < cps.size(); i++) {
        const Glyph* g = f.find(cps[i]);
        if (!g) g = f.find('?');
        if (!g) continue;
        if (g->w > 0) {
            const float u0 = pen + g->ox * k, v0 = g->oy * k, u1 = u0 + g->w * k, v1 = v0 + g->h * k;
            const float U[4] = {u0, u1, u1, u0}, Vv[4] = {v0, v0, v1, v1};
            const float T[4][2] = {{g->x0, g->y0}, {g->x1, g->y0}, {g->x1, g->y1}, {g->x0, g->y1}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int j : idx) {
                const float X = x + U[j] * ca - Vv[j] * sa, Y = base + U[j] * sa + Vv[j] * ca;
                const float v[4] = {X, Y, T[j][0], T[j][1]};
                out.insert(out.end(), v, v + 4);
            }
        }
        pen += g->adv * k + track * capPx;
        if (i + 1 < cps.size()) pen += stbtt_GetCodepointKernAdvance((const stbtt_fontinfo*)f.info, cps[i], cps[i + 1]) * f.scaleTT * k;
    }
}

void MangaUniverse::drawText(const Font& f, const std::vector<float>& q, float capPx, const TextStyle& st, float alpha) {
    if (q.empty() || !f.ok || alpha <= 0.002f) return;
    flushInk();
    // outline widths from pixels to SDF units
    const float k = capPx / f.capH;                // screen pixels per atlas pixel
    const float perPx = (128.f / f.pad) / 255.f;   // SDF units per atlas pixel
    const float lw = std::min(0.45f, st.linePx / k * perPx), hw = std::min(0.45f - lw, st.haloPx / k * perPx);
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set4f(progText_, "uFill", st.fill.x, st.fill.y, st.fill.z, st.fillA);
    set4f(progText_, "uLine", st.line.x, st.line.y, st.line.z, st.lineA);
    set4f(progText_, "uHalo", st.halo.x, st.halo.y, st.halo.z, st.haloA);
    set1f(progText_, "uLineW", st.lineA > 0.f ? lw : 0.f);
    set1f(progText_, "uHaloW", st.haloA > 0.f ? hw : 0.f);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// A sound effect: katakana set along a line or stacked down a column, each letter turned a little its own way.
void MangaUniverse::drawSfx(const Sfx& s, float alpha) {
    if (alpha <= 0.002f) return;
    const float k = easeOut(s.t / std::max(0.05f, s.in));
    const float grow = lerpf(1.22f, 1.f, k);
    const float cap = s.size * unitPx_ * grow;
    TextStyle st;
    switch (s.style) {
    case SFX_BLACK: st = {ink_, 1.f, paper_, 0.f, 0.f, paper_, 1.f, cap * 0.09f}; break;
    case SFX_THIN: st = {paper_, 1.f, ink_, 1.f, std::max(1.5f, cap * 0.028f), paper_, 0.f, 0.f}; break;
    case SFX_COLOR: st = {cur_[K_SFX], 1.f, ink_, 1.f, cap * 0.05f, paper_, 1.f, cap * 0.07f}; break;
    case SFX_LAMP: st = {hue(manga::H_LAMP), 1.f, ink_, 1.f, std::max(1.2f, cap * 0.05f), paper_, 1.f, cap * 0.09f}; break;
    case SFX_WHITE: st = {paper_, 1.f, ink_, 1.f, cap * 0.06f, paper_, 0.f, 0.f}; break;
    default: st = {paper_, 1.f, ink_, 1.f, cap * 0.05f, paper_, 1.f, cap * 0.07f}; break;
    }
    std::vector<float> q;
    const vec2 c = px(s.at + s.vel * s.t);
    const std::vector<int> cps = decode(s.text);
    // letters one by one, so each has its own tilt
    std::vector<std::string> letters;
    for (size_t i = 0, b = 0; i < s.text.size(); i = b) {
        b = i + 1;
        while (b < s.text.size() && ((unsigned char)s.text[b] & 0xC0) == 0x80) b++;
        letters.push_back(s.text.substr(i, b - i));
    }
    const float stepPx = cap * 1.05f;
    const int n = (int)letters.size();
    for (int i = 0; i < n; i++) {
        // the long vowel mark stands up in a column
        const bool bar = s.vertical && letters[i] == "\xE3\x83\xBC";
        const float tilt = s.angle + 0.09f * std::sin(i * 2.7f + s.at.x) + (bar ? PI * 0.5f : 0.f);
        const float sc = 1.f - 0.1f * (i == n - 1 && cps.back() == 0xFF01);
        float lx, ly;
        if (s.vertical) {
            lx = c.x + (i % 2 ? 0.08f : -0.08f) * cap;
            ly = c.y + (i - (n - 1) * 0.5f) * stepPx;
        } else {
            lx = c.x + (i - (n - 1) * 0.5f) * stepPx * 0.92f * std::cos(s.angle);
            ly = c.y + (i - (n - 1) * 0.5f) * stepPx * 0.92f * std::sin(s.angle);
        }
        // around the letter's center
        const float w = textWidth(dela_, letters[i], cap * sc, 0.f);
        const float ca = std::cos(tilt), sa = std::sin(tilt);
        const float ox = -w * 0.5f, oy = cap * sc * 0.55f;
        textQuads(dela_, letters[i], lx + ox * ca - oy * sa, ly + ox * sa + oy * ca, cap * sc, 0.f, tilt, q);
    }
    drawText(dela_, q, cap, st, alpha);
}

// =====================================================================================================
// The page
// =====================================================================================================

// One element: in pencil until it is inked; then its lines are drawn along their length and its tones laid down,
// their dots growing in, and last its washes of color flood in under the ink, like a colorist working after the
// inker. A panel's ground (no pencil) has its washes laid in faintly from the start (the colorist's flats). fade
// < 1 takes the ink and the color off again (a new song), leaving the pencil.
void MangaUniverse::drawElement(const manga::Element& e, float inkT, float fade, float rotA) {
    const int clip = clipOverride_ >= 0 ? clipOverride_ : e.panel;
    const V2* pv = rotA != 0.f ? &e.pivot : nullptr;
    const float k = inkT < 0.f ? -1.f : inkT / INK_S;
    if (artMode_ != 0) {
        // the drop's impact frames: the lines only, no color (negative: the ink's lines in paper, on an ink ground)
        const vec3 line = artMode_ == 1 ? paper_ : ink_, knock = artMode_ == 1 ? ink_ : paper_;
        if (k < 0.f) {
            for (const manga::Stroke& s : e.strokes) strokeLine(s.pts, 1.0f, 0.f, s.closed, 1.f, line, 0.45f * fade, clip, pv, rotA);
            return;
        }
        const float tones = smoothstepf(0.4f, 1.f, k) * fade, lineK = easeOut(k / 0.65f);
        for (const manga::Fill& f : e.fills) {
            if (f.tone.kind == manga::TONE_WASH) continue;
            if (f.tone.kind == manga::TONE_SOLID) {
                if (f.tone.color == manga::C_INK) fillPoly(f.pts, f.tone, 1.f, line, tones, clip, f.strip, pv, rotA);
                else if (f.tone.color == manga::C_PAPER) fillPoly(f.pts, f.tone, 1.f, knock, tones, clip, f.strip, pv, rotA);
                continue;
            }
            fillPoly(f.pts, f.tone, tones, line, fade, clip, f.strip, pv, rotA);
        }
        for (const manga::Stroke& s : e.strokes)
            strokeLine(s.pts, s.width, s.taper, s.closed, lineK, s.color == manga::C_PAPER ? knock : line, fade, clip, pv, rotA);
        return;
    }
    // the pencil sketch, fading as the ink covers it
    const float pencilA = k < 0.f ? 1.f : 1.f - smoothstepf(0.4f, 1.f, k) * fade;
    if (!e.pencil)
        for (const manga::Fill& f : e.fills)
            if (f.tone.kind == manga::TONE_WASH) fillPoly(f.pts, f.tone, 1.f, ink_, 0.26f, clip, f.strip, pv, rotA);
    if (e.pencil && pencilA > 0.01f)
        for (const manga::Stroke& s : e.strokes) strokeLine(s.pts, 1.0f, 0.f, s.closed, 1.f, pencil_, 0.85f * pencilA, clip, pv, rotA);
    if (k < 0.f) return;
    const float tones = smoothstepf(0.4f, 1.f, k) * fade, lineK = easeOut(k / 0.65f);
    const float wash = smoothstepf(WASH0, WASH1, k);
    for (const manga::Fill& f : e.fills) {
        if (f.tone.kind == manga::TONE_WASH) {
            if (wash > 0.001f) fillPoly(f.pts, f.tone, wash, ink_, fade, clip, f.strip, pv, rotA);
            continue;
        }
        const bool solid = f.tone.kind == manga::TONE_SOLID;
        fillPoly(f.pts, f.tone, solid ? 1.f : tones, col(f.tone.color), solid ? tones : fade, clip, f.strip, pv, rotA);
    }
    for (const manga::Stroke& s : e.strokes) strokeLine(s.pts, s.width, s.taper, s.closed, lineK, col(s.color), fade, clip, pv, rotA);
}

// The art panels' drawings with their moving parts (wm: the mid level's weight, wp: the peak's).
void MangaUniverse::drawPanels(float t, float wm, float wp) {
    const float fade = resetT_ >= 0.f ? 1.f - ease(resetT_ / resetDur_) : 1.f;
    for (size_t i = 0; i < page_.elems.size(); i++) {
        const manga::Element& e = page_.elems[i];
        if (!page_.panels[e.panel].art) continue;
        const float it = i < inkT_.size() ? inkT_[i] : -1.f;
        const bool inked = it >= INK_S * 0.6f;
        drawElement(e, it, fade, e.anim == manga::A_CHIME ? chime_ : 0.f);

        // the crossing's lamps, lit in turn
        if (e.anim == manga::A_LAMPS && inked) {
            const vec3 red = hue(manga::H_LAMP);
            for (int l = 0; l < 2; l++) {
                const float lit = lampT_[l];
                if (lit <= 0.01f) continue;
                const vec2 c = px(page_.lamp[l]);
                const float r = page_.lampRadius * unitPx_;
                // a red glow around it, its glass lit red, a hot white center
                discPx(c.x, c.y, r * 2.3f, red, 0.16f * lit, e.panel);
                discPx(c.x, c.y, r * 1.6f, red, 0.24f * lit, e.panel);
                discPx(c.x, c.y, r * 0.95f, red, lit, e.panel);
                discPx(c.x, c.y, r * 0.45f, lerp(red, vec3(1.f, 0.93f, 0.85f), 0.75f), lit, e.panel);
                std::vector<V2> pts;
                for (int k = 0; k < 10; k++) {
                    const float a = k / 10.f * TAU + 0.3f;
                    const V2 o = page_.lamp[l];
                    const float r0 = page_.lampRadius + 0.2f, r1 = r0 + 0.32f + 0.12f * (k % 2);
                    strokeLine({{o.x + std::cos(a) * r0, o.y + std::sin(a) * r0}, {o.x + std::cos(a) * r1, o.y + std::sin(a) * r1}}, 1.5f, 0.3f,
                               false, 1.f, red, lit, e.panel);
                }
            }
        }
        // the lighthouse's beam, sweeping slowly over the sea at a peak
        if (e.anim == manga::A_BEAM && inked && wp > 0.01f) {
            const float a = -0.25f + 0.32f * std::sin(t * 0.45f);
            const V2 o = page_.beamFrom;
            const float L = 9.f, hw = 0.07f;
            const V2 p1{o.x + std::cos(a - hw) * L, o.y + std::sin(a - hw) * L}, p2{o.x + std::cos(a + hw) * L, o.y + std::sin(a + hw) * L};
            // a warm light, fading out over the sea
            manga::Tone tn;
            tn.kind = manga::TONE_WASH;
            tn.color = tn.color1 = manga::pal(manga::H_WINDOW);
            tn.g0 = o;
            tn.g1 = {o.x + std::cos(a) * L * 0.8f, o.y + std::sin(a) * L * 0.8f};
            tn.d0 = 0.85f;
            tn.d1 = 0.15f;
            fillPoly({o, p1, p2}, tn, 1.f, ink_, 0.85f * wp, e.panel);
            strokeLine({o, p1}, 0.9f, 0.f, false, 1.f, shadeOf(hue(manga::H_WINDOW)), 0.5f * wp, e.panel);
            strokeLine({o, p2}, 0.9f, 0.f, false, 1.f, shadeOf(hue(manga::H_WINDOW)), 0.5f * wp, e.panel);
        }
    }
    (void)wm;
}

// The frames of the panels: bold ruled lines. On the drop the art panels' frames break into pieces that drift
// away from the well and fade (shatter 0..1), and come back after.
float MangaUniverse::panelIn(int panel) const {
    int j = 0;
    for (int i = manga::P_A; i < panel; i++) j += page_.panels[i].art ? 1 : 0;
    return saturate((setup_ - 0.04f - 0.075f * j) / 0.26f);
}

float MangaUniverse::panelOut(int panel) const {
    int j = 0;
    for (int i = manga::P_A; i < panel; i++) j += page_.panels[i].art ? 1 : 0;
    return ease(saturate((teardown_ - 0.32f - 0.065f * j) / 0.3f));
}

// The frames, inked by hand: each edge a brush stroke (the well's the boldest) running on past the corners.
void MangaUniverse::drawFrames(float shatter, bool art) {
    for (int i = 0; i < manga::P_COUNT; i++) {
        const manga::Panel& p = page_.panels[i];
        if (!p.shown || p.art != art || (bare_ && !p.art)) continue;
        const float w = i == manga::P_WELL ? 4.4f : 3.4f;
        const uint64_t seed = 0xF4A3E + (uint64_t)i;
        if (!p.art || shatter <= 0.001f) {
            // (the setup: a drawing panel's frame ruled in along its edges)
            const float upTo = p.art ? smoothstepf(0.f, 0.55f, panelIn(i)) : 1.f;
            if (upTo > 0.f) inkQuad(p.c, w, seed, upTo, ink_, 1.f);
            continue;
        }
        if (shatter >= 0.999f) continue;
        // the edges in shards: pieces of the same strokes drifting apart
        Rng r(1000 + i);
        for (int e = 0; e < 4; e++) {
            const uint64_t es = splitmix64(seed * 4 + e);
            const V2 a = p.c[e], b = p.c[(e + 1) % 4];
            const float len = std::hypot(b.x - a.x, b.y - a.y);
            const int n = std::max(1, (int)(len / 1.3f));
            for (int k = 0; k < n; k++) {
                const float t0 = k / (float)n, t1 = (k + 1) / (float)n;
                const V2 m = a + (b - a) * ((t0 + t1) * 0.5f);
                const float dist = r.range(0.4f, 1.4f) * shatter;
                const float sx = m.x < 0.f ? -1.f : 1.f;
                const V2 off{sx * dist, r.range(-0.5f, 0.5f) * shatter};
                const float rot = r.range(-0.5f, 0.5f) * shatter;
                // the shard's part of its edge's stroke, the whole stroke moved with it
                vec2 A = px(turn(a + off, &m, rot)), B = px(turn(b + off, &m, rot));
                float u0 = t0, u1 = t1;
                if (es & 1) {
                    std::swap(A, B);
                    u0 = 1.f - t1;
                    u1 = 1.f - t0;
                }
                brushPx(A, B, w, es, u0, u1, 1.f, ink_, 1.f - ease(shatter), -1);
            }
        }
    }
}

// The drop's burst of color behind the focus lines: a radial gradient from the well out to the page's edges, in
// the variant's three burst colors.
void MangaUniverse::drawBurst(float amt) {
    if (amt <= 0.002f) return;
    const vec2 c = px({0.f, 0.4f});
    const float R = std::hypot((float)width_, (float)height_) * 0.62f;
    const float U[5] = {0.f, 0.2f, 0.42f, 0.72f, 1.f};
    auto color = [&](float u) {
        return u < 0.42f ? lerp(cur_[K_BURST], cur_[K_BURST + 1], u / 0.42f) : lerp(cur_[K_BURST + 1], cur_[K_BURST + 2], (u - 0.42f) / 0.58f);
    };
    const vec2 o = px({0.f, 0.f});
    const ToneP t{manga::TONE_WASH, 4.f, 0.f, o, -1};
    const int N = 40;
    for (int r = 0; r < 4; r++) {
        const vec3 c0 = color(U[r]), c1 = color(U[r + 1]);
        for (int i = 0; i < N; i++) {
            const float a0 = i / (float)N * TAU, a1 = (i + 1) / (float)N * TAU;
            const V2 p00{c.x + std::cos(a0) * R * U[r], c.y + std::sin(a0) * R * U[r]}, p01{c.x + std::cos(a1) * R * U[r], c.y + std::sin(a1) * R * U[r]};
            const V2 p10{c.x + std::cos(a0) * R * U[r + 1], c.y + std::sin(a0) * R * U[r + 1]}, p11{c.x + std::cos(a1) * R * U[r + 1], c.y + std::sin(a1) * R * U[r + 1]};
            const vec3 ca[3] = {c0, c1, c1}, cb[3] = {c0, c1, c0};
            const float al[3] = {amt, amt, amt};
            triC(p00, p10, p11, t, 1.f, ca, al);
            if (r > 0) triC(p00, p11, p01, t, 1.f, cb, al);
        }
    }
}

// Focus lines bursting in from the page's edges toward the well. reach 0..1: how far in they come.
void MangaUniverse::drawSplash(float reach, float t, const vec3& color) {
    if (reach <= 0.002f) return;
    Rng r(0xF0C05ull);
    const V2 c{0.f, 0.4f};
    const float R = std::hypot(viewH_.x, viewH_.y) + 1.f;
    // lunges on the two-beat grid: a rotating quarter of the lines reaches in further
    const float lunge = std::max(0.f, 1.f - (t - std::floor(t)) * 2.f);
    const int phase = (int)std::floor(t) & 3;
    float a = 0.f;
    int i = 0;
    while (a < TAU) {
        // clustered: small gaps mostly, a wider one now and then
        a += r.chance(0.12f) ? r.range(0.04f, 0.09f) : r.range(0.004f, 0.022f);
        const float ca = std::cos(a), sa = std::sin(a);
        // the inner end on an ellipse around the well, its own depth
        const float ex = r.range(12.f, 14.5f), ey = r.range(9.6f, 11.5f);
        const float rin = 1.f / std::sqrt((ca * ca) / (ex * ex) + (sa * sa) / (ey * ey));
        const float wBase = r.range(0.02f, 0.12f) * (r.chance(0.15f) ? 2.2f : 1.f);
        // from where the ray leaves the screen
        const float tx = (ca > 0.f ? viewC_.x + viewH_.x - c.x : c.x - viewC_.x + viewH_.x) / std::max(1e-4f, std::fabs(ca));
        const float ty = (sa > 0.f ? viewC_.y + viewH_.y - c.y : c.y - viewC_.y + viewH_.y) / std::max(1e-4f, std::fabs(sa));
        const float rOut = std::min(tx, ty) + 0.2f;
        const float inner = lerpf(rOut, rin - ((i & 3) == phase ? 0.9f * lunge * reach : 0.f), reach);
        i++;
        if (inner >= R - 0.05f) continue;
        const V2 tip{c.x + ca * inner, c.y + sa * inner};
        const V2 nrm{-sa, ca};
        const V2 o{c.x + ca * R, c.y + sa * R};
        const float half = wBase * (R - inner) / 6.f;
        manga::Tone tn;
        fillPoly({tip, o + nrm * half, o - nrm * half}, tn, 1.f, color, 1.f, -1);
    }
}

// =====================================================================================================
// The board
// =====================================================================================================

// The outline: one brush stroke along each straight run of the piece's edge, on the edge (a stroke shared by two
// touching pieces is inked twice, one bold line). Each stroke's seed is its place in the piece's shape, so the outline
// holds as the piece falls and locks.
void MangaUniverse::inkOutline(const int cells[][2], int n, int minRow, float dx, float dy, float widthPx, uint64_t seed, float loose,
                               const vec3& col, float a) {
    auto in = [&](int x, int y) {
        for (int i = 0; i < n; i++)
            if (cells[i][0] == x && cells[i][1] == y) return y >= minRow;
        return false;
    };
    int mx = 99, my = 99;
    for (int i = 0; i < n; i++) {
        mx = std::min(mx, cells[i][0]);
        my = std::min(my, cells[i][1]);
    }
    // sides: above, below (runs along x), left, right (runs down, game y growing downward)
    const int NX[4] = {0, 0, -1, 1}, NY[4] = {-1, 1, 0, 0};
    for (int i = 0; i < n; i++) {
        const int cx = cells[i][0], cy = cells[i][1];
        if (cy < minRow) continue;
        for (int side = 0; side < 4; side++) {
            const int nx = NX[side], ny = NY[side];
            if (in(cx + nx, cy + ny)) continue;
            const int rx = side < 2 ? 1 : 0, ry = side < 2 ? 0 : 1; // along the run
            // only from a run's first cell
            if (in(cx - rx, cy - ry) && !in(cx - rx + nx, cy - ry + ny)) continue;
            int len = 1;
            while (in(cx + rx * len, cy + ry * len) && !in(cx + rx * len + nx, cy + ry * len + ny)) len++;
            const vec3 p0 = layout::cellPos(cx + dx, cy + dy), p1 = layout::cellPos(cx + rx * (len - 1) + dx, cy + ry * (len - 1) + dy);
            V2 A, B;
            if (side < 2) {
                const float y = p0.y - 0.5f * ny;
                A = {p0.x - 0.5f, y};
                B = {p1.x + 0.5f, y};
            } else {
                const float x = p0.x + 0.5f * nx;
                A = {x, p0.y + 0.5f};
                B = {x, p1.y - 0.5f};
            }
            const uint64_t ss = splitmix64(seed * 7919 + (uint64_t)(side * 131 + (cx - mx) * 17 + (cy - my) * 1031 + len * 31));
            vec2 pa = px(A), pb = px(B);
            if (ss & 1) std::swap(pa, pb);
            brushPx(pa, pb, widthPx, ss, 0.f, 1.f, loose, col, a, -1);
        }
    }
}

// A piece, flat and plain: its cells in its color, faint seams between them, one bold brush outline around it.
void MangaUniverse::drawPiece(const int cells[][2], int n, int type, float dx, float dy, float alpha, float inkK, int parts) {
    if (alpha <= 0.002f) return;
    const float br = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, br); };
    if (parts & PIECE_FILL) {
        const vec3 fc = tone(lerp(paper_, pieceColor(type), inkK));
        for (int i = 0; i < n; i++) {
            if (cells[i][1] < 0) continue;
            const vec3 p = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
            fillPoly({{p.x - 0.5f, p.y - 0.5f}, {p.x + 0.5f, p.y - 0.5f}, {p.x + 0.5f, p.y + 0.5f}, {p.x - 0.5f, p.y + 0.5f}}, manga::Tone(), 1.f, fc,
                     alpha, -1);
        }
    }
    if (!(parts & PIECE_INK)) return;
    auto in = [&](int x, int y) {
        for (int i = 0; i < n; i++)
            if (cells[i][0] == x && cells[i][1] == y) return true;
        return false;
    };
    const vec3 ic = tone(ink_);
    const float wIn = std::max(1.f, 0.9f * s_);
    for (int i = 0; i < n; i++) {
        if (cells[i][1] < 0) continue;
        const int cx = cells[i][0], cy = cells[i][1];
        const vec3 p = layout::cellPos(cx + dx, cy + dy);
        const vec2 a = px({p.x - 0.5f, p.y + 0.5f}), b = px({p.x + 0.5f, p.y - 0.5f}); // top-left, bottom-right
        if (in(cx + 1, cy)) rectPx(b.x - wIn * 0.5f, a.y, b.x + wIn * 0.5f, b.y, ic, 0.45f * alpha);
        if (in(cx, cy + 1)) rectPx(a.x, b.y - wIn * 0.5f, b.x, b.y + wIn * 0.5f, ic, 0.45f * alpha);
    }
    inkOutline(cells, n, 0, dx, dy, 4.4f, (uint64_t)type + 1, 0.4f, ic, alpha);
}

void MangaUniverse::drawBoard(const Game& g, float t) {
    (void)t;
    const float br = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, br); };

    // the well's floor
    {
        const vec2 a = px({-5.f, -10.f}), b = px({5.f, -10.f});
        brushPx({a.x - 2.f * s_, a.y + 1.3f * s_}, {b.x + 2.f * s_, a.y + 1.3f * s_}, 2.6f, 0xF1009, 0.f, 1.f, 0.5f, tone(ink_), 0.9f, -1);
    }

    // the ghost: the piece roughed out in grey graphite, a draft: a loose outline gone over twice (the strokes not
    // quite on each other), light diagonal hatching inside; seeded by the piece's shape, so it holds as it moves
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            const vec3 c = tone(lerp(paper_, vec3{0.24f, 0.24f, 0.26f}, 0.75f)); // graphite on this page's paper
            int mx = 99, my = 99;
            for (auto& cc : gc) { mx = std::min(mx, cc[0]); my = std::min(my, cc[1]); }
            for (auto& cc : gc) {
                if (cc[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)cc[0], (float)cc[1]);
                const vec2 a = px({w.x - 0.5f, w.y + 0.5f}), b = px({w.x + 0.5f, w.y - 0.5f});
                rectPx(a.x, a.y, b.x, b.y, c, 0.05f);
                const uint64_t hs = splitmix64((uint64_t)p.type * 977 + (uint64_t)(cc[0] - mx) * 31 + (uint64_t)(cc[1] - my) * 7);
                for (int k = 0; k < 3; k++) { // hatching, bottom-left to top-right, inside the cell
                    const float f = (k + 0.5f) / 3.f, inset = 0.14f * (b.x - a.x);
                    const vec2 h0{lerpf(a.x, b.x, f * 0.5f) + inset * 0.3f, lerpf(b.y, a.y, 0.5f + f * 0.5f) - inset * 0.3f};
                    const vec2 h1{lerpf(a.x, b.x, 0.5f + f * 0.5f) - inset * 0.3f, lerpf(b.y, a.y, f * 0.5f) + inset * 0.3f};
                    brushPx(h0, h1, 0.8f, hs + k, 0.f, 1.f, 0.6f, c, 0.38f, -1);
                }
            }
            inkOutline(gc, 4, Game::HIDDEN, 0.f, 0.f, 1.6f, (uint64_t)p.type + 1, 1.f, c, 0.85f);
            inkOutline(gc, 4, Game::HIDDEN, 0.f, 0.f, 1.2f, (uint64_t)p.type + 101, 1.f, c, 0.55f);
        }
    }

    // locked pieces, grouped by id: all their colors, then all their ink
    {
        struct Group { int cells[8][2]; int n, type; float off; };
        std::vector<Group> groups;
        bool done[Game::H][Game::W] = {};
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0 || done[y][x]) continue;
                Group gr{{}, 0, c.type, g.rowOffset(y)};
                const int id = ids_[y][x];
                for (int yy = Game::HIDDEN; yy < Game::H && gr.n < 8; yy++)
                    for (int xx = 0; xx < Game::W && gr.n < 8; xx++) {
                        if (done[yy][xx] || g.cell(xx, yy).type < 0) continue;
                        const bool same = (xx == x && yy == y) || (id && ids_[yy][xx] == id && std::fabs(g.rowOffset(yy) - gr.off) < 0.02f);
                        if (!same) continue;
                        gr.cells[gr.n][0] = xx;
                        gr.cells[gr.n][1] = yy;
                        gr.n++;
                        done[yy][xx] = true;
                    }
                groups.push_back(gr);
            }
        for (const Group& gr : groups) drawPiece(gr.cells, gr.n, gr.type, 0.f, -gr.off, 1.f, 1.f, PIECE_FILL);
        for (const Group& gr : groups) drawPiece(gr.cells, gr.n, gr.type, 0.f, -gr.off, 1.f, 1.f, PIECE_INK);
    }

    // cleared cells breaking up into dots of their color that thin out, the outline going; the stack's end turns to
    // pencil first
    for (const Fade& f : fades_) {
        if (f.t < 0.f) {
            const int cells[1][2] = {{f.x, f.y}};
            drawPiece(cells, 1, f.type, 0.f, 0.f, 1.f, 1.f);
            continue;
        }
        const vec3 p = layout::cellPos((float)f.x, (float)f.y);
        const vec2 a = px({p.x - 0.5f, p.y + 0.5f}), b = px({p.x + 0.5f, p.y - 0.5f});
        if (f.mask == 1) {
            // pencil, then erased
            const float k = f.t / 0.9f;
            const float pa = 1.f - smoothstepf(0.45f, 1.f, k);
            const float ia = 1.f - smoothstepf(0.f, 0.4f, k);
            const int cells[1][2] = {{f.x, f.y}};
            if (ia > 0.01f) drawPiece(cells, 1, f.type, 0.f, 0.f, ia, ia);
            const float th = std::max(1.f, 1.3f * s_);
            const vec3 c = tone(pencil_);
            rectPx(a.x, a.y, b.x, a.y + th, c, pa);
            rectPx(a.x, b.y - th, b.x, b.y, c, pa);
            rectPx(a.x, a.y, a.x + th, b.y, c, pa);
            rectPx(b.x - th, a.y, b.x, b.y, c, pa);
            continue;
        }
        const float k = f.t / FADE_S;
        const float al = 1.f - ease(k);
        manga::Tone tn;
        tn.kind = manga::TONE_DOTS;
        tn.d0 = tn.d1 = 0.85f;
        tn.cell = 0.16f;
        const float sh = 0.5f - 0.12f * ease(k);
        fillPoly({{p.x - sh, p.y - sh}, {p.x + sh, p.y - sh}, {p.x + sh, p.y + sh}, {p.x - sh, p.y + sh}}, tn, 1.f - 0.7f * ease(k),
                 tone(pieceColor(f.type)), al, -1);
        const float th = std::max(1.f, 2.f * s_ * (1.f - k));
        rectPx(a.x, a.y, b.x, a.y + th, tone(ink_), al * al);
        rectPx(a.x, b.y - th, b.x, b.y, tone(ink_), al * al);
    }

    // the crumbs: dots flying out of the well
    for (const Crumb& c : crumbs_) {
        const float k = c.t / c.life;
        if (k >= 1.f) continue;
        const vec2 p = px(c.p);
        const vec3 cc = pieceColor(c.type);
        discPx(p.x, p.y, c.r * unitPx_ * (1.f - 0.5f * k), tone(cc), 1.f - k * k);
    }

    // the falling piece: exactly a locked one
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        drawPiece(cells, 4, p.type, dx, dy, 1.f, 1.f);
    }

    // impact lines at the feet of a hard drop
    for (const Impact& im : impacts_) {
        const float k = im.t / 0.3f;
        if (k >= 1.f) continue;
        const V2 a = im.a + (im.b - im.a) * (0.6f * easeOut(k));
        strokeLine({a, im.b + (im.b - im.a) * (0.3f * easeOut(k))}, 2.f, 0.35f, false, 1.f, tone(ink_), 1.f - k, -1);
    }

    // previews: small pieces in the HUD panels
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
        // drawn at full size through a scaled board: place each cell, then scale about the center
        const size_t before = verts_.size();
        const vec3 base = layout::cellPos(0.f, (float)Game::HIDDEN + 4.f);
        int cc[4][2];
        for (int i = 0; i < 4; i++) { cc[i][0] = cells[i][0]; cc[i][1] = cells[i][1] + Game::HIDDEN + 4; }
        drawPiece(cc, 4, type, 0.f, 0.f, alpha, 1.f);
        const vec2 from = px({base.x + (minx + maxx) * 0.5f, base.y - (miny + maxy) * 0.5f});
        const vec2 to = px({center.x, center.y});
        for (size_t v = before; v < verts_.size(); v += 14) {
            verts_[v + 0] = to.x + (verts_[v + 0] - from.x) * sc;
            verts_[v + 1] = to.y + (verts_[v + 1] - from.y) * sc;
            if ((int)(verts_[v + 2] + 0.5f) == manga::TONE_DISC) continue;
            verts_[v + 4] *= std::max(0.75f, sc);
            verts_[v + 6] = to.x;
            verts_[v + 7] = to.y;
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    flushInk();
}

// =====================================================================================================
// The HUD
// =====================================================================================================

// Comic lettering in the HUD's panels (hudLine, the host's): the section and the universe sit in a caption box in the
// corner of the crossing's panel, a quad in a spiky balloon, both drawn here around the lines as the host letters
// them (f.hudBoxes).
void MangaUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<vec4>* boxes,
                            const std::vector<HudRect>& panels) {
    const float s = s_;
    const vec3 ink = ink_, paper = paper_, accent = spot_;
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : paper;
            rectPx(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        flushInk();
    }

    // (no boxes: a break's frame, its lines lettered over the mix)
    if (!boxes || boxes->size() != hud.size()) return;

    // the caption box under the section and the universe's name
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        float a = 0.f;
        for (size_t i = 0; i < hud.size(); i++) {
            const HudText& h = hud[i];
            const vec4& b = (*boxes)[i];
            if ((h.role != HUD_SECTION && h.role != HUD_UNIVERSE) || h.text.empty() || b.z <= b.x) continue;
            const float base = h.scale * 12.f;
            const float cap = h.role == HUD_SECTION ? base * 0.62f : base * 0.45f;
            x0 = std::min(x0, b.x);
            x1 = std::max(x1, b.z);
            y0 = std::min(y0, h.y);
            y1 = std::max(y1, h.y + cap * 1.25f);
            a = std::max(a, std::min(1.f, 0.4f + h.alpha * 1.5f));
        }
        if (x1 > x0 && vis > 0.01f && !bare_) {
            const float pad = 9.f * s;
            rectPx(x0 - pad, y0 - pad, x1 + pad, y1 + pad, paper, a * vis);
            // its frame inked by hand like the panels'
            const vec2 c[4] = {{x0 - pad, y0 - pad}, {x1 + pad, y0 - pad}, {x1 + pad, y1 + pad}, {x0 - pad, y1 + pad}};
            for (int e = 0; e < 4; e++) brushPx(c[e], c[(e + 1) % 4], 2.2f, 0xCA9710 + (uint64_t)e, 0.f, 1.f, 0.6f, ink, a * vis, -1);
            flushInk();
        }
    }

    // a quad's spiky shout balloon
    for (size_t i = 0; i < hud.size(); i++) {
        const HudText& h = hud[i];
        const vec4& b = (*boxes)[i];
        if (h.role != HUD_TOAST || h.alpha <= 0.01f || h.text.find(QUAD_NAME) == std::string::npos || b.z <= b.x) continue;
        const float alpha = saturate(h.alpha) * vis, cap = h.scale * 12.f * 0.75f;
        const float w = b.z - b.x, cx = (b.x + b.z) * 0.5f, cy = h.y + cap * 0.5f;
        const float rx = w * 0.5f + cap * 0.9f, ry = cap * 1.25f;
        std::vector<V2> pts;
        const int spikes = 18;
        auto toB = [&](float X, float Y) { return V2{(X - width_ * 0.5f) / unitPx_ + viewC_.x, (height_ * 0.5f - Y) / unitPx_ + viewC_.y}; };
        for (int k = 0; k < spikes * 2; k++) {
            const float an = k / (float)(spikes * 2) * TAU;
            const float r = (k % 2 == 0) ? 1.f + 0.18f * std::sin(k * 1.7f) : 0.74f;
            pts.push_back(toB(cx + std::cos(an) * rx * r, cy + std::sin(an) * ry * r));
        }
        manga::Tone tn;
        fillPoly(pts, tn, 1.f, paper, 0.92f * alpha, -1);
        strokeLine(pts, 2.4f, 0.f, true, 1.f, ink, alpha, -1);
        flushInk();
    }
}

// The lettering, by the host (text.hpp): labels in Comic Neue, numbers and shouts in Bangers (sizes: their capitals'
// height); the song and its tempo on a rim of paper, the notices and the toasts outlined like sound effects.
void MangaUniverse::hudLine(const HudText& h, text::Line& out) {
    // the capitals' height of each font, of its size
    constexpr float CAP_COMIC = 0.587f, CAP_BANGERS = 0.6955f;
    text::Font* comic = text::font(FONT_COMIC);
    text::Font* bangers = text::font(FONT_BANGERS);
    const vec3 ink = ink_, paper = paper_, accent = spot_;
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a run: its capitals' height and letter spacing (of it) in pixels, moved down by dy
    float cap = 0.f;
    auto put = [&](bool shout, const std::string& t, float c, float track, const vec3& col, float a, float dy = 0.f) {
        cap = c;
        const float k = shout ? CAP_BANGERS : CAP_COMIC;
        out.add(t, shout ? bangers : comic, c / k, col, track * k, saturate(a));
        out.dy = dy;
    };
    // the ink line and the paper rim around it (pixels), as a sound effect's
    auto inked = [&](float linePx, float rimPx) {
        const float em = out.spans[0].size;
        out.look.outline = ink;
        out.look.outlineA = linePx > 0.f ? 1.f : 0.f;
        out.look.outlineW = linePx / em;
        out.look.halo = paper;
        out.look.haloA = rimPx > 0.f ? 1.f : 0.f;
        out.look.haloW = rimPx / em;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(false, text::upper(h.text), base * 0.45f, 0.06f, ink, 0.4f + h.alpha * 1.5f, base * 0.12f); break;
    case HUD_SECTION: put(true, text::upper(h.text), base * 0.62f, 0.05f, accent, 0.4f + h.alpha * 1.5f, base * 0.02f); break;
    case HUD_TITLE:
        put(false, h.text, base * 0.6f, 0.01f, ink, h.alpha * 1.6f, base * 0.02f);
        inked(0.f, cap * 0.3f);
        break;
    case HUD_INFO:
        // fully opaque over the drop's burst
        put(false, h.text, base * 0.5f, 0.05f, ink, saturate(alpha * 1.3f) * lerpf(0.75f, 1.f, saturate(burst_ * 1.5f)), base * 0.1f);
        inked(0.f, cap * 0.35f);
        break;
    case HUD_LABEL: put(false, text::upper(h.text), base * 0.44f, 0.16f, ink, saturate(alpha * 1.3f) * 0.85f, base * 0.12f); break;
    case HUD_VALUE: put(true, h.text, base * 0.7f, 0.04f, ink, alpha, -base * 0.02f); break;
    case HUD_NOTICE:
        put(true, h.text, base * 0.6f, 0.05f, cur_[K_SFX], alpha);
        inked(std::max(1.5f, cap * 0.08f), cap * 0.08f);
        break;
    case HUD_KEY: put(false, h.text, base * 0.55f, 0.02f, ink, alpha); break;
    case HUD_TOAST:
        // (a quad's in its balloon: no rim)
        put(true, h.text, base * 0.75f, 0.06f, cur_[K_SFX], alpha, -base * 0.05f);
        inked(std::max(1.5f, cap * 0.07f), h.text.find(QUAD_NAME) != std::string::npos ? 0.f : cap * 0.08f);
        break;
    case HUD_TOAST_SUB: put(true, h.text, base * 0.55f, 0.05f, ink, alpha); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s_) {
            put(true, h.text, base * 0.7f, 0.06f, paper, alpha);
            inked(std::max(2.f, cap * 0.07f), cap * 0.08f);
        } else {
            put(false, h.text, base * 0.5f, 0.04f, h.accent ? accent : ink, alpha);
            inked(0.f, cap * 0.25f);
        }
        break;
    default: put(false, h.text, base * 0.5f, 0.03f, h.accent ? accent : ink, alpha); break;
    }
    // the ink line and the paper rim, crisp; together within the letters' field
    const float reach = out.spans[0].font == bangers ? 0.121f : 0.126f;
    out.look.outlineW = std::min(out.look.outlineW, reach);
    out.look.haloW = std::min(out.look.haloW, reach - (out.look.outlineA > 0.f ? out.look.outlineW : 0.f));
    out.lettering = &INKED;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MangaUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    bare_ = f.bare;
    setup_ = f.setup;
    teardown_ = f.teardown;
    if (!f.bare) syncIds(game); // (a break's blank board keeps the stack's ids)
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
        if (!game.over()) {
            overT_ = -1.f;
            fades_.erase(std::remove_if(fades_.begin(), fades_.end(), [](const Fade& x) { return x.mask == 1; }), fades_.end());
        } else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += wdt;
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);

    // ---- A real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace.
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    if (!dropMode_) dropT_ = -1.f;
    else if (dropT_ >= 0.f) dropT_ += dt;
    const bool splashing = isDrop && dropT_ >= 0.f;
    const double beatNow = music.hasSong ? music.beat : time_ * 2.0;
    // ---- The drop's effect: one per drop, another every 32 beats of a long one.
    if (splashing && !donShown_) {
        donShown_ = true;
        startFx(pickFx(), beatNow);
    }
    if (splashing && fx_ >= 0) {
        if (beatNow - fxBeat0_ >= 32.0) startFx(pickFx(), beatNow);
        const long n = (long)std::floor(beatNow - fxBeat0_);
        if (n - fxStep_ > 4) fxStep_ = n - 1;
        while (fxStep_ < n && !f.paused) {
            fxStep_++;
            if (fxStep_ >= 0) fxBeat(fx_, fxStep_, beatNow, music.bpm);
        }
    }
    if (!splashing) fx_ = -1;
    splash_ = 0.f;
    for (int i = 0; i < DF_COUNT; i++) {
        // eased in over half a second, and out over a second and a half (faster when the drop moves on)
        const bool on = splashing && i == fx_;
        fxAmt_[i] = on ? std::min(1.f, fxAmt_[i] + dt / 0.5f) : std::max(0.f, fxAmt_[i] - dt / (splashing ? 0.4f : 1.5f));
        if (on || fxAmt_[i] > 0.f) fxT_[i] += dt;
        splash_ = std::max(splash_, fxAmt_[i]);
    }

    // ---- The two-beat grid: the lamps, the chime, the rumble.
    {
        const double beat = music.hasSong ? music.beat : time_ * 2.0;
        const long step = (long)std::floor(beat / 2.0);
        if (step != step_ && !f.paused) {
            const bool first = step_ <= -1000;
            step_ = step;
            if (!first && overT_ < 0.f) {
                if (wm > 0.5f) {
                    const int l = (int)(step & 1);
                    lampT_[l] = 1.f;
                    // the lit lamp's sound, if the crossing is inked
                    for (size_t i = 0; i < page_.elems.size(); i++)
                        if (page_.elems[i].anim == manga::A_LAMPS && inkT_[i] >= INK_S && splash_ < 0.3f && slow_ < 0.3f) {
                            const V2 at{page_.lamp[0].x - 1.25f, page_.lamp[0].y + (l ? -0.75f : 0.55f)};
                            addSfx(SFX_KAN, at, 0.5f, l ? 0.18f : -0.18f, 0.7f, false, SFX_LAMP, 0.15f);
                        }
                    chimeV_ += (step & 1 ? 1.f : -1.f) * (0.35f + 0.35f * wp);
                }
            }
        }
        lampT_[0] = approach(lampT_[0], 0.f, 2.2f, dt);
        lampT_[1] = approach(lampT_[1], 0.f, 2.2f, dt);
        // the chime: a damped pendulum
        chimeV_ += (-chime_ * 9.f - chimeV_ * 1.6f) * wdt;
        chime_ += chimeV_ * wdt;
        chime_ = std::clamp(chime_, -0.45f, 0.45f);
    }

    // ---- A new song's inks and page.
    if (colorRate_ > 0.f) {
        const float k = 1.f - std::exp(-dt * colorRate_);
        for (int i = 0; i < K_COUNT; i++) cur_[i] = lerp(cur_[i], tgt_[i], k);
    }
    if (resetT_ >= 0.f) {
        resetT_ += dt;
        if (resetT_ >= resetDur_) resetPage();
    }
    for (float& q : inkQ_) q -= dt;
    while (!inkQ_.empty() && inkQ_.front() <= 0.f && resetT_ < 0.f) {
        inkQ_.erase(inkQ_.begin());
        // skip the elements of panels off screen
        while (nextInk_ < page_.elems.size() && !page_.panels[page_.elems[nextInk_].panel].art) nextInk_++;
        if (nextInk_ < page_.elems.size()) inkT_[nextInk_++] = 0.f;
    }
    for (float& t : inkT_)
        if (t >= 0.f && t < INK_S * (WASH1 + 0.2f)) t += dt;

    // ---- Effects (real time).
    for (Crumb& c : crumbs_) {
        c.t += dt;
        c.p = c.p + c.v * dt;
        c.v = c.v * std::exp(-dt * 2.5f);
        c.v.y -= dt * 2.f;
    }
    crumbs_.erase(std::remove_if(crumbs_.begin(), crumbs_.end(), [](const Crumb& c) { return c.t >= c.life; }), crumbs_.end());
    for (Fade& x : fades_) x.t += dt;
    fades_.erase(std::remove_if(fades_.begin(), fades_.end(), [](const Fade& x) { return x.t >= (x.mask == 1 ? 0.9f : FADE_S); }), fades_.end());
    for (Impact& im : impacts_) im.t += dt;
    impacts_.erase(std::remove_if(impacts_.begin(), impacts_.end(), [](const Impact& im) { return im.t >= 0.3f; }), impacts_.end());
    for (Sfx& s : sfx_) s.t += dt;
    sfx_.erase(std::remove_if(sfx_.begin(), sfx_.end(),
                              [&](const Sfx& s) { return s.t >= s.life || (s.fx >= 0 && fxAmt_[s.fx] <= 0.f && fx_ != s.fx); }),
               sfx_.end());
    // the drop's shock waves, shards, balloons, jolts
    for (Ring& r : rings_) r.t += dt;
    rings_.erase(std::remove_if(rings_.begin(), rings_.end(), [](const Ring& r) { return r.t >= r.life; }), rings_.end());
    for (Shard& x : shards_) {
        x.t += dt;
        x.p = x.p + x.v * dt;
        x.v = x.v * std::exp(-dt * 1.5f);
        x.rot += x.vr * dt;
    }
    shards_.erase(std::remove_if(shards_.begin(), shards_.end(), [](const Shard& x) { return x.t >= x.life; }), shards_.end());
    for (Pop& p : pops_) p.t += dt;
    pops_.erase(std::remove_if(pops_.begin(), pops_.end(), [](const Pop& p) { return p.t >= p.life; }), pops_.end());
    jolt_ = approach(jolt_, 0.f, 7.f, dt);
    if (quadT_ >= 0.f) {
        quadT_ += dt;
        if (quadT_ > 2.6f) quadT_ = -1.f;
    }
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- Draw.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(paper_.x, paper_.y, paper_.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // the drawings
    const float aS = ease(fxAmt_[DF_SPLASH]), aD = ease(fxAmt_[DF_DASH]), aP = ease(fxAmt_[DF_PANELS]);
    const float aI = ease(fxAmt_[DF_IMPACT]), aJ = ease(fxAmt_[DF_SHOJO]), aB = ease(fxAmt_[DF_BARRAGE]);
    drawPanels(time_, wm, wp);
    // slow motion: night settles over the drawings (a deep blue wash, night tones), petals drift down through them
    if (slow_ > 0.01f) {
        // the night's tone is the darker of the two inks, the petals the lighter, blushed pink
        const bool lightPage = ugl::luma(paper_) > 0.4f;
        const vec3 dark = lightPage ? ink_ : paper_, light = lerp(lightPage ? paper_ : ink_, vec3(0.97f, 0.66f, 0.76f), 0.45f);
        for (int i = manga::P_A; i < manga::P_COUNT; i++) {
            const manga::Panel& p = page_.panels[i];
            if (!p.art) continue;
            manga::Tone nt;
            nt.kind = manga::TONE_WASH;
            nt.color = manga::pal(manga::H_SKY_TOP);
            nt.color1 = manga::pal(manga::H_NIGHT);
            nt.d0 = 0.5f;
            nt.d1 = 0.22f;
            nt.g0 = {0, p.c[0].y};
            nt.g1 = {0, p.c[3].y};
            fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, nt, 1.f, ink_, ease(slow_), i);
            manga::Tone tn;
            tn.kind = manga::TONE_DOTS;
            tn.d0 = 0.32f;
            tn.d1 = 0.f;
            tn.g0 = {0, p.c[0].y};
            tn.g1 = {0, lerpf(p.c[0].y, p.c[3].y, 0.6f)};
            tn.cell = 0.12f;
            tn.angle = 0.26f; // clear of the sky's screen (no moire)
            fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, tn, ease(slow_), shadeOf(hue(manga::H_NIGHT)), 0.85f, i);
        }
        Rng r(0x9E7A1ull);
        for (int i = 0; i < 46; i++) {
            const bool left = i % 2 == 0;
            const float x0 = left ? r.range(-20.5f, -12.3f) : r.range(12.3f, 20.5f);
            const float sp = r.range(0.35f, 0.7f), ph = r.range(0.f, 30.f);
            const float y = 12.f - std::fmod(time_ * sp * 2.2f + ph, 24.f);
            const float x = x0 + 0.5f * std::sin(time_ * 0.8f * sp + ph);
            const vec2 c = px({x, y});
            const float rr = r.range(0.06f, 0.11f) * unitPx_;
            // in the panel it falls through
            int clip = -1;
            for (int k = manga::P_A; k < manga::P_COUNT && clip < 0; k++) {
                if (!page_.panels[k].art) continue;
                bool inside = true;
                for (int e = 0; e < 4; e++)
                    if (clipEq_[k][e][0] * c.x + clipEq_[k][e][1] * c.y + clipEq_[k][e][2] < 0.f) inside = false;
                if (inside) clip = k;
            }
            if (clip < 0) continue;
            discPx(c.x, c.y, rr + 1.4f * s_, dark, ease(slow_), clip);
            discPx(c.x, c.y, rr, light, ease(slow_), clip);
        }
    }
    // the drop: the drawings fade back to the paper (the splash's; the other moments paint their own full page)
    {
        const float k = 0.9f * aS;
        if (k > 0.001f)
            for (int i = manga::P_A; i < manga::P_COUNT; i++) {
                const manga::Panel& p = page_.panels[i];
                if (!p.art) continue;
                manga::Tone tn;
                fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, tn, 1.f, paper_, k, i);
            }
    }
    const double beat = music.hasSong ? music.beat : time_ * 2.0;
    if (aD > 0.001f) drawDash(aD, fxT_[DF_DASH], beat);
    if (aJ > 0.001f) drawShojo(aJ, beat);
    if (aB > 0.001f) drawBarrage(aB, beat);
    if (aI > 0.001f) drawImpact(aI, beat);
    if (aP > 0.001f) drawPanelBreak(aP, beat);
    // the burst of color behind the focus lines: the splash's, a quad's
    {
        float amt = aS;
        if (quadT_ >= 0.f) amt = std::max(amt, 0.6f * easeOut(quadT_ / 0.3f) * (1.f - smoothstepf(1.4f, 2.6f, quadT_)));
        burst_ = std::max(amt, aI);
        drawBurst(amt);
    }
    // focus lines: the splash's burst, a quad's, and short ones at a peak
    {
        float reach = aS;
        if (quadT_ >= 0.f) reach = std::max(reach, 0.7f * easeOut(quadT_ / 0.3f) * (1.f - smoothstepf(1.4f, 2.6f, quadT_)));
        reach = std::max(reach, 0.2f * wp * (1.f - slow_) * (1.f - splash_));
        drawSplash(reach, (float)(beat / 2.0), ink_);
    }
    flushInk();

    // the setup: the drawing panels blank but for their ground until their turn; the teardown: the ink coming down
    // over them one after the other (the darker of the page's two inks)
    {
        const vec3 dark = ugl::luma(paper_) > 0.4f ? ink_ : paper_;
        for (int i = manga::P_A; i < manga::P_COUNT; i++) {
            const manga::Panel& p = page_.panels[i];
            if (!p.art) continue;
            manga::Tone tn;
            const float in = panelIn(i), out = panelOut(i);
            if (in < 1.f) fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, tn, 1.f, paper_, 1.f - smoothstepf(0.4f, 1.f, in), i);
            if (out > 0.f) {
                const V2 b0 = p.c[0] + (p.c[3] - p.c[0]) * out, b1 = p.c[1] + (p.c[2] - p.c[1]) * out;
                fillPoly({b0, b1, p.c[1], p.c[0]}, tn, 1.f, dark, 1.f, i);
            }
        }
        flushInk();
    }
    // the drawing panels' frames, then the sound effects and BARRAGE's balloons over them (a word may run across
    // several panels, over their frames and gutters), then the HUD's panels and the well's over everything: plain paper and their frames
    // (a break: only the art panels' frames)
    const float shatter = std::max(aS, std::max(aP, aI));
    drawFrames(shatter, true);
    flushInk();
    if (aB > 0.001f) drawBalloons(aB);
    for (const Sfx& s : sfx_) {
        float a = s.life > 1e8f ? 1.f : 1.f - smoothstepf(s.life - 0.35f, s.life, s.t);
        if (s.fx >= 0) a *= ease(fxAmt_[s.fx]);
        drawSfx(s, a * std::min(1.f, s.t / std::max(0.05f, s.in * 0.5f)));
    }
    if (slow_ > 0.01f) {
        Sfx sh{SFX_SHIIN, {16.3f, -4.2f}, 1.9f, 0.f, 1.f, 1.f, 1.f, true, SFX_THIN};
        drawSfx(sh, ease(slow_));
    }
    for (int i = manga::P_WELL; i < manga::P_COUNT; i++) {
        const manga::Panel& p = page_.panels[i];
        if (p.art || !p.shown || bare_) continue;
        manga::Tone tn;
        fillPoly({p.c[3], p.c[2], p.c[1], p.c[0]}, tn, 1.f, paper_, 1.f, -1);
    }
    drawFrames(shatter, false);
    flushInk();

    // ---- The bridge: the world gives way to the bridge's color (the stack turns to its ink).
    if (bridge_.amount > 0.001f) {
        rectPx(0, 0, (float)width_, (float)height_, ugl::toSrgb(bridge_.color), smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        flushInk();
    }

    // ---- The game.
    if (!bare_) drawBoard(game, time_);

    // ---- Dim (pause, game over screen) and the startup fade.
    {
        const float d = 0.5f * dimT_;
        if (d > 0.002f) rectPx(0, 0, (float)width_, (float)height_, paper_, d);
        if (f.fade < 0.999f) rectPx(0, 0, (float)width_, (float)height_, vec3(0.f), 1.f - saturate(f.fade));
        flushInk();
    }
    drawHud(game, f.hud, f.hudBoxes, f.panels);
    flushInk();
    glDisable(GL_BLEND);

    // ---- Resolve into the host's framebuffer.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    // the paper's grain over it all (multiplied: dst * src * 2)
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    glUseProgram(progPaper_);
    ugl::set2f(progPaper_, "uRes", (float)width_, (float)height_);
    ugl::set1f(progPaper_, "uScale", s_);
    ugl::set1f(progPaper_, "uAmt", 1.f);
    ugl::drawFullscreen();
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_manga() {
    static const UniverseEntry e{
        "MANGA", "a manga color page in ink, screentones and watercolor washes, inked then colored panel by panel as rows clear; the drop is one of six manga moments: a splash of color, speed lines, a panel break, impact frames, a shojo bloom or a barrage of sound effects",
        [] { return std::unique_ptr<Universe>(new MangaUniverse()); },
        // Graphic and dramatic: it suits energetic songs with clear drops best.
        [](const SongLook& l) { return 0.75f + 0.25f * l.dynamics + 0.15f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

// SUNPRINT: a cyanotype photogram exposed under the stack.
//
// One world: a sheet of paper brushed with sensitizer, lying in the sun, the pieces glass plates laid on it. The
// paper darkens wherever the light reaches it, so the sheet records the stack's history: where the plates lay
// long, the paper stays pale, and a cleared row leaves its pale ghost on the print that the sun slowly fills in.
// The plates cast soft shadows that keep the glass's color; the falling plate, held above the sheet, casts a longer,
// softer shadow that tightens as it lands. Pressed ferns lie in the margins, as in Anna Atkins's prints: their white
// silhouettes come up as the sheet darkens around them over the song.
//
// The song is the sun. Calm parts are a low sun behind passing clouds (long soft shadows, the print slow); a chorus
// raises it (shorter shadows, the ferns stirring every two beats); a peak is full sun (crisp colored shadows, glints
// running along the plates' bevels on the kicks). The drop develops the print: the latent image deepens into the
// rinsed print's colors (Prussian blue on white) on the downbeat, and stays developed for the whole drop. A cleared
// row's plates are lifted off the sheet (they fade as they rise, their shadows growing soft); a Tetris is a burst of
// sun over the well that burns the moment into the print. Slow motion is the sun going behind a cloud (a cool, quiet
// afternoon); game over lifts the plates off from the top down, leaving the stack's white silhouette on the print.
// A new song rinses the sheet back to fresh paper over a few seconds.
//
// Cost: the shadows are two half-resolution coverage targets (blurred into penumbras), the exposure a
// half-resolution float target the light is added into each frame; one full-screen pass lights the sheet.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/sunprint/sunprint.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/sunprint/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Cormorant Garamond (Medium: the numbers and labels; Italic: the song, the section, the celebrations), SIL Open
// Font License, see OFL-CormorantGaramond.txt.
const unsigned char FONT_REG[] = {
#include "universes/sunprint/CormorantGaramond-Medium.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/sunprint/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = sunprint_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// The processes: the bare paper, the coat unexposed and fully exposed before the drop (the latent print) and after
// it (the developed print), the ink of the lettering, an accent, the sky's light in the shadows, the seven glasses.
struct VariantDef {
    const char* name;
    unsigned paper, base0, full0, base1, full1, ink, accent, sky;
    unsigned piece[7]; // I O T S Z J L
};
const VariantDef VARIANTS[] = {
    // the classic cyanotype: a grey-blue latent image, developed into Prussian blue on white
    {"PRUSSIAN", 0xf3f2ee, 0xf3f2ee, 0x86a2d2, 0xf6f6f3, 0x123f7d, 0x0f2748, 0xd9822b, 0xb7c4d6,
     {0x3db7d3, 0xebc33f, 0x9b62cc, 0x45ab68, 0xdb4a5f, 0xef8a35, 0xd76bb5}},
    // toned in tannin: mauve grey, developed into a violet black
    {"TANNIN", 0xf2f1ec, 0xf2f1ec, 0x9a8c98, 0xf4f2ee, 0x35213a, 0x2b1b2d, 0xe0a03a, 0xc8c0cc,
     {0x4fc4b6, 0xead06a, 0x6d90e2, 0x8ec95b, 0xe2675a, 0xf09b48, 0xc58ae2}},
    // a Van Dyke brown print: sand grey, developed into a deep chocolate
    {"VAN DYKE", 0xf2f0ea, 0xf2f0ea, 0xa79784, 0xf4f1ea, 0x4b2d1d, 0x3a2417, 0x3c8fbf, 0xd0c7bc,
     {0x4cb2dd, 0xf1d25c, 0x8b7ad8, 0x5ac08c, 0xe2566b, 0x3e7ed2, 0xe78fc6}},
    // an anthotype: paper dyed with berry juice, bleaching where the light reaches it (the drop deepens the dye)
    {"ANTHOTYPE", 0xf3f1ee, 0xa64873, 0xeed9e0, 0x6a1640, 0xd27aa2, 0x4a1230, 0xe8a23c, 0xd6c3cf,
     {0x45c3b1, 0xf1d469, 0x6e9ae8, 0x92d36b, 0xf28a52, 0x3c78c8, 0xe9e4d2}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The print's color at an exposure p (0..1), between the coat unexposed and fully exposed (mixed in display
// space, as the paper shader does), linear.
vec3 printColor(const vec3& base, const vec3& full, float p) {
    return ugl::toLinear(lerp(ugl::toSrgb(base), ugl::toSrgb(full), p));
}

// How much light each glass lets through to the paper (the print under it advances that much slower).
constexpr float DENSITY = 0.9f;
// Heights above the sheet (board units): a plate's thickness, the falling plate held up, a lifted plate.
constexpr float H_FLAT = 0.6f, H_RAISED = 2.2f;

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

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SunprintUniverse::init(int width, int height) {
    using shaders::withCommon;
    progOcc_ = ugl::program(shaders::CELL_VS, withCommon(shaders::OCC_FS, true).c_str(), "sunprint occluders");
    progPlate_ = ugl::program(shaders::CELL_VS, withCommon(shaders::PLATE_FS, true).c_str(), "sunprint plates");
    progLeaf_ = ugl::program(shaders::LEAF_VS, shaders::LEAF_FS, "sunprint ferns");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "sunprint blur");
    progExpose_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::EXPOSE_FS, false).c_str(), "sunprint exposure");
    progPaper_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::PAPER_FS, false).c_str(), "sunprint paper");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "sunprint final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "sunprint shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "sunprint text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &leafVao_);
    glGenBuffers(1, &leafVbo_);
    glBindVertexArray(leafVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, leafVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(LeafInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
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

    buildFont(fontReg_, FONT_REG);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void SunprintUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] sunprint font init failed\n");
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
        const int cp = ch == 127 ? 0xB7 : ch; // the middle dot in DEL's slot
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

void SunprintUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    const int hw2 = std::max(1, width_ / 2), hh2 = std::max(1, height_ / 2);
    occT_.create(hw2, hh2, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    fallT_.create(hw2, hh2, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    blurT_.create(hw2, hh2, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    expoT_.create(hw2, hh2, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    expoClear_ = true;
    placeFronds();
}

SunprintUniverse::~SunprintUniverse() {
    for (ugl::Target* t : {&occT_, &fallT_, &blurT_, &expoT_, &sceneT_}) t->destroy();
    for (GLuint p : {progOcc_, progPlate_, progLeaf_, progBlur_, progExpose_, progPaper_, progFinal_, progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {cellVao_, leafVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, leafVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontReg_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The ferns
// =====================================================================================================

// Two fronds pressed on the sheet in the outer margins (clear of the HUD columns), when the screen leaves room.
void SunprintUniverse::placeFronds() {
    fronds_.clear();
    const float room = viewH_.x - 11.8f;
    if (room < 2.2f) return;
    Rng r(seed_ ^ 0xFE4Eull);
    const float xm = 11.8f + room * 0.5f;
    const float len = std::min(13.f, 2.f * viewH_.y * 0.62f);
    // one rising from the bottom on one side, one hanging from the top on the other
    const float side = r.chance(0.5f) ? -1.f : 1.f;
    fronds_.push_back({vec2{side * (xm + r.range(-0.4f, 0.4f)), -viewH_.y - 0.6f}, PI * 0.5f + side * r.range(0.04f, 0.14f),
                       len * r.range(0.85f, 1.f), -side * r.range(0.025f, 0.05f), r.range(0.f, TAU)});
    fronds_.push_back({vec2{-side * (xm + r.range(-0.4f, 0.4f)), viewH_.y + 0.6f}, -PI * 0.5f - side * r.range(0.04f, 0.14f),
                       len * r.range(0.7f, 0.85f), side * r.range(0.025f, 0.05f), r.range(0.f, TAU)});
}

// The leaflets of every frond: a rachis curling slightly, pinnae in alternate pairs shortening toward the tip, each
// pinna a row of pinnules. `sway` bends the fronds (more toward the tip).
void SunprintUniverse::buildFronds(float sway) {
    leaves_.clear();
    for (const Frond& fr : fronds_) {
        const int N = 22;
        const float ds = fr.length / N;
        vec2 p = fr.base;
        float a = fr.angle;
        const float sw = sway * std::sin(fr.phase) + sway; // each frond its own share of the breeze
        for (int i = 0; i < N; i++) {
            const float t = (i + 0.5f) / N;
            a += fr.curl * ds + sw * 0.06f * t;
            const vec2 d{std::cos(a), std::sin(a)};
            p = vec2{p.x + d.x * ds, p.y + d.y * ds};
            // the rachis
            leaves_.push_back({{p.x - d.x * ds * 0.5f, p.y - d.y * ds * 0.5f, ds * 0.62f, 0.05f * (1.f - 0.6f * t) + 0.02f}, {a, 1.f, 0, 0}});
            if (i < 2) continue;
            // a pair of pinnae, alternate (one side a little ahead of the other)
            const float plen = fr.length * 0.2f * std::pow(1.f - t, 0.75f) * std::min(1.f, (t - 0.05f) * 5.f) + 0.12f;
            for (int sdi = 0; sdi < 2; sdi++) {
                const float sgn = sdi == 0 ? 1.f : -1.f;
                const float pa = a + sgn * (1.05f - 0.35f * t);
                const vec2 pd{std::cos(pa), std::sin(pa)};
                const vec2 o{p.x + d.x * ds * 0.3f * sdi, p.y + d.y * ds * 0.3f * sdi};
                const int M = std::max(2, (int)std::lround(plen / 0.2f));
                for (int k = 0; k < M; k++) {
                    const float u = (k + 0.5f) / M;
                    const float pl = 0.17f * (1.f - 0.55f * u) * std::min(1.f, plen * 1.4f);
                    const vec2 c{o.x + pd.x * plen * u, o.y + pd.y * plen * u};
                    // pinnules: small leaflets pointing forward along the pinna, alternating sides
                    const float qa = pa + (k % 2 == 0 ? 0.75f : -0.75f) * 0.8f;
                    leaves_.push_back({{c.x + std::cos(qa) * pl * 0.6f, c.y + std::sin(qa) * pl * 0.6f, pl, pl * 0.42f}, {qa, 1.f, 0, 0}});
                }
                // the pinna's own stem
                leaves_.push_back({{o.x + pd.x * plen * 0.5f, o.y + pd.y * plen * 0.5f, plen * 0.5f, 0.022f}, {pa, 1.f, 0, 0}});
            }
        }
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SunprintUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 1;
        else if (t.hasSong && t.bassWeight > 0.62f) v = 2;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5C1ull);
    sunAngle_ = rng_.range(-0.85f, -0.45f);
    if (old != seed_) placeFronds();
    // a new song: the sheet is rinsed back to fresh paper (at once for the first one)
    if (seconds <= 0.1f) { expoClear_ = true; marginE_ = 0.f; }
    else rinse_ = 1.f;
}

void SunprintUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
    (void)impact;
}

// The drop: the print develops (its deep colors come up on the downbeat), the sun swells, every plate catches it.
void SunprintUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x]) glint_[y][x] = std::max(glint_[y][x], 0.55f + 0.25f * strength);
}

// A new section: one plate catches the sun.
void SunprintUniverse::refresh(float strength) {
    int n = 0, pick = -1;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] && rng_.irange(0, n++) == 0) pick = ids_[y][x];
    if (pick < 0) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] == pick) glint_[y][x] = std::max(glint_[y][x], 0.4f + 0.3f * strength);
}

void SunprintUniverse::levelUp() { swell_ = std::max(swell_, 0.35f); }

vec2 SunprintUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SunprintUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    const float p = 1.f - std::exp(-marginE_);
    const vec3 m = printColor(lerp(lin(V.base0), lin(V.base1), dev_), lerp(lin(V.full0), lin(V.full1), dev_), p);
    TransitionHints h;
    h.background = m;
    h.light = ugl::luma(m) > 0.22f;
    h.ink = h.light ? lin(V.ink) : lin(V.paper);
    h.accent = lin(V.accent);
    h.styles = TS_LUMA | TS_INK | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the plates' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void SunprintUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; glint_[y][x] = 0.f; }
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

void SunprintUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            glint_[c.y][c.x] = 0.f;
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the plates of the cleared rows are lifted off the sheet
        int lowest = -1, highest = Game::H;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            lowest = std::max(lowest, c.y);
            highest = std::min(highest, c.y);
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && rows[c.y + 1] && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && rows[c.y - 1] && ids_[c.y - 1][c.x] == id) m |= 8;
            }
            lifted_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.f});
        }
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            float ng[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(ng[dst], glint_[y], sizeof(glint_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(glint_, ng, sizeof(glint_));
        }
        // a Tetris: a burst of sun over the cleared rows, burning the moment into the print
        if (ev.count >= 4 && lowest >= 0) {
            flare_ = 1.f;
            flareY_ = 0.5f * (layout::cellPos(0.f, (float)lowest).y + layout::cellPos(0.f, (float)highest).y);
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the plates stay on the sheet, then are lifted off from the top down (each row with its own delay: a
        // plate's cells keep their joins within a row)
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
            }
            lifted_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.8f + 0.11f * (c.y - Game::HIDDEN)});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void SunprintUniverse::buildBoard(const Game& g) {
    const VariantDef& V = VARIANTS[variant_];
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    // a plate: drawn on the sheet, and casting its shadow flat (on the sheet) or raised (above it)
    auto plate = [&](float x, float y, float half, int mask, int type, float alpha, float lift, float glint, bool raised) {
        if (alpha <= 0.003f) return;
        const vec3 c = lin(V.piece[std::clamp(type, 0, 6)]);
        const float shade = 0.f;
        cells_.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, 0.9f * alpha}, {0.f, lift, glint, shade}});
        (raised ? raised_ : flat_).push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, DENSITY * alpha}, {0.f, 0.f, 0.f, 0.f}});
    };
    // The ghost: a pencil outline (under everything).
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
                cells_.push_back({{w.x, w.y, 0.5f, (float)maskOf(c[0], c[1], inGhost)}, {0.f, 0.f, 0.f, 0.45f}, {1.f, 0.f, 0.f, 0.f}});
            }
        }
    }
    // Locked plates.
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
            plate(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, 1.f, 0.f, glint_[y][x], false);
        }
    // Lifted plates: rising off the sheet, fading (a clear in real time; game over from the top down).
    for (const Lifted& l : lifted_) {
        const float k = smoothstepf(l.delay, l.delay + (l.delay > 0.f ? 0.6f : 0.32f), l.t);
        if (l.t < l.delay) {
            plate(l.x, l.y, 0.5f, l.mask, l.type, 1.f, 0.f, 0.f, false);
            continue;
        }
        plate(l.x, l.y, 0.5f * (1.f + 0.1f * k), l.mask, l.type, 1.f - k, k, 0.f, true);
    }
    // The falling plate, held above the sheet.
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
            plate(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, 1.f, 0.f, 0.f, true);
        }
    }
    // Previews: small plates in the HUD columns (they leave no print there).
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
            plate(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, alpha, 0.f, 0.f, false);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void SunprintUniverse::drawCells(const std::vector<CellInst>& v, int pass) {
    if (v.empty()) return;
    const GLuint p = pass == 0 ? progOcc_ : progPlate_;
    glUseProgram(p);
    setMat(p, "uVP", vp_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(CellInst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

// A separable blur of a shadow target (through blurT_), `radius` in its texels.
void SunprintUniverse::blur(ugl::Target& t, float radius) {
    glDisable(GL_BLEND);
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    glActiveTexture(GL_TEXTURE0);
    const int passes = radius > 1.6f ? 2 : 1;
    const float step = radius / passes;
    for (int i = 0; i < passes; i++) {
        blurT_.bind();
        glBindTexture(GL_TEXTURE_2D, t.tex);
        set2f(progBlur_, "uDir", step / t.w, 0.f);
        ugl::drawFullscreen();
        t.bind();
        glBindTexture(GL_TEXTURE_2D, blurT_.tex);
        set2f(progBlur_, "uDir", 0.f, step / t.h);
        ugl::drawFullscreen();
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float SunprintUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
    }
    return w - track;
}

void SunprintUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                 std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void SunprintUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.3f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void SunprintUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in ink on the sheet: its color follows the paper behind it (dark ink while the margins are pale,
// paper white once the print has gone dark there), the bonus gauge a fine ring whose charge is drawn in the accent.
void SunprintUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const float p = 1.f - std::exp(-marginE_);
    const vec3 m = printColor(lerp(lin(V.base0), lin(V.base1), dev_), lerp(lin(V.full0), lin(V.full1), dev_), p);
    const float dark = smoothstepf(0.19f, 0.25f, ugl::luma(m)); // 1: dark ink on pale paper
    const vec3 inkL = lerp(lin(V.paper), lin(V.ink), dark), haloL = lerp(lin(V.ink), lin(V.paper), dark);
    const vec3 ink = ugl::toSrgb(inkL), halo = ugl::toSrgb(haloL), accent = hex(V.accent);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    // the bonus gauge
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 96;
        const float th = std::max(1.f, 1.2f * s), th2 = std::max(2.f, 3.2f * s);
        for (int i = 0; i < N; i++) {
            const float a0 = TAU * i / N, a1 = TAU * (i + 1) / N;
            const bool on = (i + 0.5f) / N <= fill;
            const float w = on ? th2 : th;
            const vec3 col = on ? accent : ink;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.65f : 0.32f)) * vis * (1.f - 0.5f * dimT_);
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        }
        flushShapes();
    }

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string t = h.text;
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            place(fontReg_, t, base * 0.9f, base * 0.2f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        }
        case HUD_SECTION:
            place(fontIt_, h.text, base * 1.0f, base * 0.12f, -base * 0.1f);
            drawText(fontIt_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.12f, 0.f, -base * 0.12f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontReg_, h.text, base * 0.82f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL: {
            std::string t = h.text;
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            place(fontReg_, t, base * 0.86f, base * 0.26f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        }
        case HUD_VALUE:
            place(fontReg_, h.text, base * 1.12f, base * 0.02f, -base * 0.1f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.05f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.95f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontIt_, h.text, base * 1.2f, base * 0.15f, -base * 0.15f);
            drawText(fontIt_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 0.95f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontIt_, h.text, base * 1.1f, base * 0.06f);
                drawText(fontIt_, q, ink, halo, alpha);
            } else {
                place(fontReg_, h.text, base * 0.88f, base * 0.04f);
                drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.86f, 0.f);
            drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SunprintUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            lifted_.erase(std::remove_if(lifted_.begin(), lifted_.end(), [](const Lifted& l) { return l.delay > 0.f; }), lifted_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 2.f : 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging: the sun.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the print develops on the drop's downbeat (quickly: no muddy in-between) and goes back slowly after it
    dev_ = approach(dev_, dropMode_ ? wp : 0.f, dropMode_ ? 4.f : 0.25f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    flare_ = approach(flare_, 0.f, 0.9f, dt);
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 3.5f, dt);
    const float sun = (lerpf(0.5f, 0.78f, wm) + 0.22f * wp + 0.12f * swell_) * (1.f - 0.65f * slow_);
    const float cloud = saturate(lerpf(0.9f, 0.35f, wm) * (1.f - wp) + slow_);
    const float shadowLen = lerpf(lerpf(0.95f, 0.55f, wm), 0.32f, wp) * (1.f + 0.6f * slow_);
    const float ang = sunAngle_ + 0.12f * std::sin(worldTime_ * 0.02f);
    const vec2 dir{std::cos(ang), std::sin(ang)}; // where the shadows fall
    const vec2 offFlat{dir.x * shadowLen * H_FLAT, dir.y * shadowLen * H_FLAT};
    const vec2 offRaised{dir.x * shadowLen * H_RAISED, dir.y * shadowLen * H_RAISED};
    const float shadowK = (lerpf(0.42f, 0.62f, wm) + 0.18f * wp) * (1.f - 0.6f * slow_) * (1.f - smoothstepf(0.f, 1.f, bridge_.amount));

    // ---- The kicks of a peak: a glint runs along one plate's bevel.
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f && wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f) {
        int n = 0, pick = -1;
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++)
                if (ids_[y][x] && rng_.irange(0, n++) == 0) pick = ids_[y][x];
        if (pick >= 0)
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++)
                    if (ids_[y][x] == pick) glint_[y][x] = std::max(glint_[y][x], 0.6f * kick + 0.25f * dev_);
    }
    kickPrev_ = kick;

    // ---- The ferns stir: a breath of wind in calm parts, on the beat grid (every two beats) from a chorus on.
    float sway = 0.006f * std::sin(worldTime_ * 0.45f);
    if (music.hasSong) {
        beatClock_ = music.beat;
        sway += (0.018f * wm + 0.035f * wp) * (float)std::sin(PI * beatClock_ * 0.5);
    }
    buildFronds(sway * (1.f - over_));

    // ---- Lifted plates (real time, even in slow motion).
    for (Lifted& l : lifted_) l.t += dt;
    lifted_.erase(std::remove_if(lifted_.begin(), lifted_.end(),
                                 [](const Lifted& l) { return l.t > l.delay + (l.delay > 0.f ? 0.7f : 0.4f); }),
                  lifted_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    cells_.clear();
    flat_.clear();
    raised_.clear();
    buildBoard(game);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const float coatW = viewH_.x - 0.9f, coatH = viewH_.y + 1.2f; // the brush ran across: ragged ends left and right
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    auto common = [&](GLuint p) {
        set4f(p, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set2f(p, "uCoat", coatW, coatH);
        set1f(p, "uSeed", seedU);
    };

    // ---- The shadows: coverage and tint of the plates on the sheet (and the ferns), of what is raised above it.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    occT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawCells(flat_, 0);
    if (!leaves_.empty()) {
        glUseProgram(progLeaf_);
        setMat(progLeaf_, "uVP", vp_);
        set3f(progLeaf_, "uColor", vec3(0.05f, 0.06f, 0.04f));
        set1f(progLeaf_, "uAlpha", 0.96f);
        glBindVertexArray(leafVao_);
        glBindBuffer(GL_ARRAY_BUFFER, leafVbo_);
        glBufferData(GL_ARRAY_BUFFER, leaves_.size() * sizeof(LeafInst), leaves_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)leaves_.size());
    }
    fallT_.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    drawCells(raised_, 0);
    const float texelPerUnit = occT_.h / (2.f * viewH_.y);
    blur(occT_, std::max(0.8f, texelPerUnit * shadowLen * 0.12f));
    blur(fallT_, std::max(1.2f, texelPerUnit * shadowLen * 0.45f));

    // ---- The exposure: the light reaching the paper this frame is added to the sheet's (a new song rinses it).
    expoT_.bind();
    if (expoClear_) {
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        expoClear_ = false;
    }
    float keep = 1.f;
    if (rinse_ > 0.f) {
        keep = std::exp(-dt * 1.4f);
        rinse_ = std::max(0.f, rinse_ - dt / 3.f);
        marginE_ *= keep;
    }
    // full sun darkens the open sheet over about a minute; a game over burns the stack's silhouette in
    const float rate = 0.034f * sun * (1.f + 0.6f * dev_ + 2.5f * over_) * (1.f - smoothstepf(0.f, 1.f, bridge_.amount)) * wdt;
    marginE_ += rate * (1.f - 0.3f * cloud);
    glEnable(GL_BLEND);
    glBlendColor(0.f, 0.f, 0.f, keep);
    glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA);
    glUseProgram(progExpose_);
    common(progExpose_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, occT_.tex);
    set1i(progExpose_, "uOcc", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fallT_.tex);
    set1i(progExpose_, "uFall", 1);
    glActiveTexture(GL_TEXTURE0);
    set2f(progExpose_, "uOffFlat", offFlat.x, offFlat.y);
    set2f(progExpose_, "uOffRaised", offRaised.x, offRaised.y);
    set1f(progExpose_, "uRate", rate);
    set1f(progExpose_, "uCloud", cloud);
    set1f(progExpose_, "uTime", worldTime_);
    set4f(progExpose_, "uFlare", 0.f, flareY_, 5.5f, 1.6f * flare_ * wdt);
    set4f(progExpose_, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progExpose_, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    ugl::drawFullscreen();

    // ---- The sheet in the sun.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progPaper_);
    common(progPaper_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, expoT_.tex);
    set1i(progPaper_, "uExpo", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, occT_.tex);
    set1i(progPaper_, "uOcc", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, fallT_.tex);
    set1i(progPaper_, "uFall", 2);
    glActiveTexture(GL_TEXTURE0);
    set2f(progPaper_, "uOffFlat", offFlat.x, offFlat.y);
    set2f(progPaper_, "uOffRaised", offRaised.x, offRaised.y);
    set3f(progPaper_, "uPaper", lin(V.paper));
    set3f(progPaper_, "uBase", lerp(lin(V.base0), lin(V.base1), dev_));
    set3f(progPaper_, "uFull", lerp(lin(V.full0), lin(V.full1), dev_));
    set3f(progPaper_, "uSky", lin(V.sky) * 0.62f);
    set1f(progPaper_, "uBright", lerpf(0.92f, 1.03f, saturate(sun)) + 0.05f * swell_);
    set1f(progPaper_, "uShadow", shadowK);
    set1f(progPaper_, "uGlassT", 0.75f);
    set1f(progPaper_, "uCloud", cloud);
    set1f(progPaper_, "uTime", worldTime_);
    set1f(progPaper_, "uVig", 0.16f);
    set4f(progPaper_, "uFlare", 0.f, flareY_, 6.5f, 0.22f * flare_);
    ugl::drawFullscreen();

    // ---- The ferns, the plates, the ghost, the previews.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!leaves_.empty()) {
        glUseProgram(progLeaf_);
        setMat(progLeaf_, "uVP", vp_);
        set3f(progLeaf_, "uColor", lin(0x7c8466));
        set1f(progLeaf_, "uAlpha", 0.34f);
        glBindVertexArray(leafVao_);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)leaves_.size());
    }
    glUseProgram(progPlate_);
    common(progPlate_);
    const vec2 toSun{-dir.x, -dir.y};
    set2f(progPlate_, "uSunDir", toSun.x, toSun.y);
    set1f(progPlate_, "uSunLvl", saturate(sun) * (1.f - 0.5f * slow_));
    set3f(progPlate_, "uInk", lin(V.ink) * 0.9f);
    drawCells(cells_, 1);

    // ---- The frame.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uSlow", slow_ * 0.8f);
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
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_sunprint() {
    static const UniverseEntry e{
        "SUNPRINT", "a cyanotype exposed under the stack: the paper records where the pieces lay, the drop develops it",
        [] { return std::unique_ptr<Universe>(new SunprintUniverse()); },
        // Bright, airy songs suit the sun best; it fits most songs.
        [](const SongLook& l) { return 0.75f + 0.25f * l.brightness + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

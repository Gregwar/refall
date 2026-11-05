// RIPPLES: cut stone blocks resting in still water, seen from above.
//
// One world: a still pond seen straight from above, lit by one light (the morning sky, the moon, a low sun...) that
// the water reflects; a slim stone kerb draws the well as a long basin in it. The pieces are cut stone blocks, one
// block per piece (its cells merged, crisp edges with a fine chamfer catching the light), each piece type its own
// stone (slate, granite, marble, jade, jasper, basalt, travertine), their shadows falling on the bed through the
// water. The falling piece is exactly a locked one at its place, the ghost a fine pale outline on the water.
//
// The water is a real height field (a wave equation on the GPU, about ten texels per cell): every motion disturbs
// it. The falling stone pushes a fine bow wave ahead of it when it slides or falls and leaves a slight trough behind,
// a rotation stirs a soft ring around it, a lock settles with a small ring at its foot, a hard drop sends one clear
// ring from the contact. The rings spread slowly, cross, reflect off the stack and the kerb and die away over a few
// seconds; the water darkens a little at the foot of the stones and the waves are damped there, so it never rings.
// A cleared row's stones sink: they darken into the water and are gone in a third of a second, the water closing
// over them in a line of fine waves that runs up the well and out into the pond. Each cleared row also opens a lily
// pad on the outer pond (a Tetris a lotus flower): the pond fills with them over the song and keeps them.
//
// The song: calm parts are still water, nothing but the game's own ripples; mid brings a drop of rain now and then,
// on the bar; a peak a steady fine breeze on the outer pond (the basin stays sheltered) and drops on every other beat
// and on the kicks, the light a little stronger. The drop is a gust of wind: patches of fine ripples drift over the
// whole pond and the light swells, settling over the following bars. A Tetris sends three perfect rings from the
// board over the whole pond. Slow motion is night: the light turns to a moon, the water deepens, the waves crawl at a
// third of their speed. Game over sinks the stack row by row from the top, each row closing in a ring.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/ripples/ripples.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/ripples/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Tenor Sans (capitals, numbers) and Cormorant Garamond Italic (the song, the celebrations), SIL Open Font License,
// see OFL-TenorSans.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/ripples/TenorSans-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/ripples/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = ripples_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

constexpr float SIM_RES = 12.f;     // water texels per cell
constexpr float SIM_HZ = 72.f;      // wave steps per second (of the world's time)
constexpr float CAM_H = 45.f;       // the virtual eye above the pond (for the light's reflection)
constexpr float SINK_T = 0.32f;     // a cleared row is gone in this long
constexpr int MAX_PADS = 46;

// A pond (sRGB): the water (its bed near and far, the sky it reflects), the light, the kerb, the floating things,
// the HUD, and the seven stones (base, vein) in piece order I O T S Z J L: slate, granite, marble, jade, jasper,
// basalt, travertine.
struct VariantDef {
    const char* name;
    bool light;
    unsigned bed, bedFar, zenith, horizon, lightC, kerb, pad, pad2, lotus, lotusCore, ink, accent, ghost;
    unsigned base[7], vein[7];
    float refl, spec, specW, polish, shadowK, caustK, aoK, glow, bloom, vignette;
    float lightX, lightY;
};
const VariantDef VARIANTS[] = {
    // a pale morning: grey-green water, soft white light, deep clean stones
    {"MORNING", true, 0x8fa3a3, 0xa3b4b2, 0xdfe6e4, 0xf3f1ea, 0xfff6e6, 0xd9d4c8, 0x5f7d5a, 0x86a079, 0xf6f1ea, 0xe8c15a,
     0x2a3436, 0x3d6f78, 0xf4f6f2,
     {0x4a6a8c, 0x7a7c80, 0xece7dd, 0x5f9a80, 0xc0664e, 0x3a3c40, 0xd0ac6a},
     {0x7890ac, 0x505256, 0xaea89e, 0x94c4ac, 0xe0987e, 0x5c5e64, 0xecd49c},
     0.22f, 0.9f, 3500.f, 0.10f, 0.5f, 0.35f, 0.16f, 0.f, 0.15f, 0.12f, 14.f, 7.f},
    // a moonlit night: ink-blue water, a cold moon, pale luminous stones
    {"MOONLIGHT", false, 0x0a1422, 0x0d1828, 0x101c2c, 0x3c4e6c, 0xdfe8ff, 0x3c4552, 0x1f3a32, 0x2d4c40, 0xf2f0ff, 0xf0d890,
     0xc8d0dc, 0x9fc0e8, 0xb8c8e0,
     {0x8fb0d8, 0xa9adb6, 0xeef0f4, 0x8cc8b4, 0xc8a0b8, 0x6a74a8, 0xd8c8a0},
     {0xb4cce8, 0x7c808a, 0xb8bcc6, 0xbce6d8, 0xe2c4d6, 0x949ccc, 0xece0c0},
     0.26f, 2.2f, 5000.f, 0.12f, 0.55f, 0.6f, 0.22f, 0.35f, 0.35f, 0.25f, 15.f, 6.5f},
    // a temple pond: deep moss-green water, warm afternoon light, warm granites
    {"MOSS", false, 0x163028, 0x1c382f, 0x2a4a40, 0x7a9280, 0xffe8c0, 0x8c8a7c, 0x3d6a3a, 0x5a8a4a, 0xfaf2ec, 0xf0c050,
     0xd8dccc, 0xe8c070, 0xc8d8c0,
     {0x5a7896, 0xa09a8c, 0xe6dfcf, 0xb8c8a0, 0xb8603e, 0x9a7a62, 0xd8a850},
     {0x8aa4bc, 0x6e6a60, 0xb8b0a0, 0xdce8c8, 0xdc9070, 0xc0a488, 0xf0cc86},
     0.2f, 1.4f, 3500.f, 0.10f, 0.5f, 0.55f, 0.2f, 0.1f, 0.3f, 0.22f, -15.f, 7.f},
    // dusk: dark water holding a warm sky, cool stones
    {"DUSK", false, 0x16141e, 0x1c1824, 0x2a2438, 0x8a5a4a, 0xffb27a, 0x4a4048, 0x2e4434, 0x48604a, 0xffeef0, 0xf0b850,
     0xe4d8d0, 0xf2a070, 0xe0c8c0,
     {0x6f8cc0, 0xc4b8bc, 0xf2e8e2, 0x86b8a8, 0xe07a6a, 0xa08cd0, 0xf0c078},
     {0x9cb4e0, 0x8c8286, 0xc8bcb6, 0xb4dece, 0xf4aa9a, 0xc6b6ec, 0xf8dca8},
     0.3f, 1.4f, 3000.f, 0.12f, 0.5f, 0.5f, 0.22f, 0.2f, 0.45f, 0.24f, 14.f, 4.5f},
    // frost: white winter water, a pale cold light, ink-dark stones
    {"FROST", true, 0xb4c2cc, 0xc4d0d8, 0xe8eef2, 0xfbfcfd, 0xffffff, 0xeef0f2, 0x6f8a86, 0x92aaa4, 0xffffff, 0xe6c870,
     0x26303a, 0x4a6a90, 0xffffff,
     {0x1c3550, 0x54504c, 0x7c8490, 0x1f5a50, 0x7e2a30, 0x1a1c20, 0x9a7638},
     {0x40587a, 0x2e2c2a, 0xdde2ea, 0x4a8a7c, 0xa8545a, 0x3a3c44, 0xc49e5e},
     0.24f, 0.7f, 3500.f, 0.10f, 0.45f, 0.3f, 0.14f, 0.f, 0.12f, 0.1f, -14.f, 7.5f},
    // candlelight: black lacquer water, a warm lantern glow, ivory and celadon stones
    {"CANDLELIGHT", false, 0x0c0a08, 0x100d0a, 0x16120e, 0x3e2e1e, 0xffc27e, 0x3a3028, 0x2a3a26, 0x3e5434, 0xfff2dc, 0xffc860,
     0xe8dcc8, 0xffc070, 0xd8c4a0,
     {0x7090a8, 0xb8ab94, 0xf4ead6, 0x9cb88c, 0xc8644a, 0x8c7484, 0xe8b052},
     {0x9cb4c8, 0x857a66, 0xcabca4, 0xc4dcb4, 0xe8987a, 0xb49cae, 0xf6d48c},
     0.24f, 1.6f, 4000.f, 0.12f, 0.6f, 0.6f, 0.24f, 0.45f, 0.4f, 0.28f, 15.f, 6.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

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
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

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

void set3v(GLuint p, const char* n, const vec3* v, int count) {
    float f[21];
    for (int i = 0; i < count && i < 7; i++) { f[i * 3] = v[i].x; f[i * 3 + 1] = v[i].y; f[i * 3 + 2] = v[i].z; }
    glUniform3fv(ugl::U(p, n), count, f);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RipplesUniverse::init(int width, int height) {
    using shaders::withCommon;
    progMask_ = ugl::program(shaders::MASK_VS, shaders::MASK_FS, "ripples mask");
    progBlur1_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR1_FS, "ripples soft");
    progSplat_ = ugl::program(shaders::SPLAT_VS, shaders::SPLAT_FS, "ripples splat");
    progStep_ = ugl::program(ugl::FULLSCREEN_VS, shaders::STEP_FS, "ripples step");
    progWater_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::WATER_FS).c_str(), "ripples water");
    progStone_ = ugl::program(shaders::STONE_VS, withCommon(shaders::STONE_FS, true).c_str(), "ripples stones");
    progPad_ = ugl::program(shaders::PAD_VS, withCommon(shaders::PAD_FS).c_str(), "ripples pads");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "ripples down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "ripples blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "ripples final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "ripples shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "ripples text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, maskVao_, maskVbo_, 1, sizeof(Rect));
    instVao(quadVbo_, splatVao_, splatVbo_, 2, sizeof(Splat));
    instVao(quadVbo_, stoneVao_, stoneVbo_, 3, sizeof(Inst));
    instVao(quadVbo_, padVao_, padVbo_, 2, sizeof(Inst));
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

    buildFont(fontCap_, FONT_CAPS);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void RipplesUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] ripples font init failed\n");
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

void RipplesUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the water: its resolution follows the world, not the window
    const int sw = std::max(16, (int)std::lround(2.f * hw * SIM_RES)), sh = std::max(16, (int)std::lround(2.f * hh * SIM_RES));
    if (sw != simW_ || sh != simH_ || !state_[0].fbo) {
        simW_ = sw;
        simH_ = sh;
        for (ugl::Target& t : state_) t.create(sw, sh, GL_RG32F, GL_RG, GL_FLOAT);
        src_.create(sw, sh, GL_R16F, GL_RED, GL_HALF_FLOAT, false, GL_NEAREST);
        mask_.create(sw, sh, GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, GL_NEAREST);
        softA_.create(sw / 2, sh / 2, GL_R16F, GL_RED, GL_HALF_FLOAT);
        softB_.create(sw / 2, sh / 2, GL_R16F, GL_RED, GL_HALF_FLOAT);
        for (ugl::Target* t : {&state_[0], &state_[1], &src_}) {
            t->bind();
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
}

RipplesUniverse::~RipplesUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_, &state_[0], &state_[1], &src_, &mask_, &softA_, &softB_}) t->destroy();
    for (GLuint p : {progMask_, progBlur1_, progSplat_, progStep_, progWater_, progStone_, progPad_, progDown_, progBlur_, progFinal_,
                     progShape_, progText_})
        glDeleteProgram(p);
    for (GLuint v : {maskVao_, splatVao_, stoneVao_, padVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, maskVbo_, splatVbo_, stoneVbo_, padVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontCap_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Looks
// =====================================================================================================

RipplesUniverse::Look RipplesUniverse::makeLook(int v) const {
    const VariantDef& V = VARIANTS[v];
    Look l;
    l.bed = lin(V.bed); l.bedFar = lin(V.bedFar); l.zenith = lin(V.zenith); l.horizon = lin(V.horizon);
    l.light = lin(V.lightC); l.kerb = lin(V.kerb); l.pad = lin(V.pad); l.pad2 = lin(V.pad2);
    l.lotus = lin(V.lotus); l.lotusCore = lin(V.lotusCore);
    l.ink = hex(V.ink); l.accent = hex(V.accent); l.ghost = lin(V.ghost);
    l.halo = ugl::toSrgb(lerp(l.bed, l.zenith, V.refl * 1.6f));
    for (int i = 0; i < 7; i++) { l.base[i] = lin(V.base[i]); l.vein[i] = lin(V.vein[i]); }
    l.refl = V.refl; l.spec = V.spec; l.specW = V.specW; l.polish = V.polish; l.shadowK = V.shadowK; l.caustK = V.caustK;
    l.aoK = V.aoK; l.glow = V.glow; l.bloom = V.bloom; l.vignette = V.vignette;
    l.lightAt = vec2{V.lightX, V.lightY};
    return l;
}

// Slow motion: the same pond at night, under a moon.
RipplesUniverse::Look RipplesUniverse::nightOf(const Look& a) const {
    Look l = a;
    const vec3 navy = lin(0x0a1424);
    l.bed = lerp(a.bed * 0.25f, navy, 0.8f);
    l.bedFar = lerp(a.bedFar * 0.25f, navy, 0.8f);
    l.zenith = lin(0x0e1a2c);
    l.horizon = lin(0x2a3c58);
    l.light = lin(0xe4ecff);
    l.kerb = a.kerb * 0.35f + lin(0x1a2230) * 0.5f;
    l.pad = a.pad * 0.4f;
    l.pad2 = a.pad2 * 0.4f;
    for (int i = 0; i < 7; i++) {
        // the stones keep their colors, cooled a little by the moon (dark stones lifted so the stack still reads)
        const vec3 b = a.base[i];
        const float L = ugl::luma(b);
        const float lift = L < 0.12f ? 0.12f / std::max(L, 0.01f) : 1.f;
        l.base[i] = b * std::min(2.5f, lift) * 0.85f * vec3(0.92f, 0.96f, 1.08f);
        l.vein[i] = a.vein[i] * std::min(2.5f, lift) * 0.85f * vec3(0.92f, 0.96f, 1.08f);
    }
    l.ink = hex(0xc8d2e0);
    l.halo = ugl::toSrgb(l.bed);
    l.accent = hex(0xb0c8f0);
    l.ghost = lin(0xa8b8d8);
    l.refl = 0.26f;
    l.spec = 2.4f;
    l.specW = 2000.f;
    l.glow = std::max(a.glow, 0.5f);
    l.bloom = std::max(a.bloom, 0.55f);
    l.vignette = 0.3f;
    return l;
}

RipplesUniverse::Look RipplesUniverse::mix(const Look& a, const Look& b, float t) {
    Look l;
    l.bed = lerp(a.bed, b.bed, t); l.bedFar = lerp(a.bedFar, b.bedFar, t); l.zenith = lerp(a.zenith, b.zenith, t);
    l.horizon = lerp(a.horizon, b.horizon, t); l.light = lerp(a.light, b.light, t); l.kerb = lerp(a.kerb, b.kerb, t);
    l.pad = lerp(a.pad, b.pad, t); l.pad2 = lerp(a.pad2, b.pad2, t); l.lotus = lerp(a.lotus, b.lotus, t);
    l.lotusCore = lerp(a.lotusCore, b.lotusCore, t); l.ink = lerp(a.ink, b.ink, t); l.halo = lerp(a.halo, b.halo, t);
    l.accent = lerp(a.accent, b.accent, t); l.ghost = lerp(a.ghost, b.ghost, t);
    for (int i = 0; i < 7; i++) { l.base[i] = lerp(a.base[i], b.base[i], t); l.vein[i] = lerp(a.vein[i], b.vein[i], t); }
    l.refl = lerpf(a.refl, b.refl, t); l.spec = lerpf(a.spec, b.spec, t); l.specW = lerpf(a.specW, b.specW, t);
    l.polish = lerpf(a.polish, b.polish, t); l.shadowK = lerpf(a.shadowK, b.shadowK, t); l.caustK = lerpf(a.caustK, b.caustK, t);
    l.aoK = lerpf(a.aoK, b.aoK, t); l.glow = lerpf(a.glow, b.glow, t); l.bloom = lerpf(a.bloom, b.bloom, t);
    l.vignette = lerpf(a.vignette, b.vignette, t);
    l.lightAt = vec2{lerpf(a.lightAt.x, b.lightAt.x, t), lerpf(a.lightAt.y, b.lightAt.y, t)};
    return l;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RipplesUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        // the song's light: bright major songs a morning (airy ones frost), dark minor ones the moon, heavy dynamic ones
        // candlelight, the others a moss pond or dusk by the key
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (!t.hasSong) v = 0;
        else if (!t.minor && t.brightness > 0.58f) v = t.airWeight > 0.55f ? 4 : 0;
        else if (t.minor && t.brightness < 0.45f) v = 1;
        else if (t.bassWeight > 0.6f && t.dynamics > 0.5f) v = 5;
        else v = h < 0.5f ? 2 : 3;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 4)) % VARIANT_COUNT;
    }
    variant_ = v;
    light_ = VARIANTS[v].light;
    look_to_ = makeLook(v);
    if (!lookSet_ || seconds <= 0.1f) {
        look_cur_ = look_to_;
        lookSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x51991Eull);
    const float wa = rng_.range(-0.5f, 0.5f);
    wind_ = vec2{std::cos(wa), std::sin(wa) * 0.6f};
    // a new song: the pond clears, its pads drift off and fade
    for (Pad& p : pads_)
        if (p.fade < 0.f) p.fade = 0.f;
    if (seconds <= 0.1f) pads_.clear();
}

void RipplesUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    (void)energy;
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
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: a gust of wind over the whole pond and a swell of the light (eased in, settling over the bars).
void RipplesUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    const float wa = rng_.range(-0.6f, 0.6f);
    wind_ = vec2{std::cos(wa) * (rng_.chance(0.5f) ? 1.f : -1.f), std::sin(wa) * 0.6f};
}

// A new section inside the same level: a single drop falls somewhere on the outer pond.
void RipplesUniverse::refresh(float strength) {
    const float s = rng_.chance(0.5f) ? -1.f : 1.f;
    ripple(s * rng_.range(13.f, viewH_.x - 1.f), rng_.range(-viewH_.y + 1.f, viewH_.y - 3.f), 0.15f, 0.08f + 0.05f * strength);
}

vec2 RipplesUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RipplesUniverse::hints() const {
    const Look& L = look_to_;
    TransitionHints h;
    h.background = lerp(L.bed, L.zenith, L.refl);
    h.light = light_;
    vec3 ink(0.f);
    for (int i = 0; i < 7; i++) ink += L.base[i];
    h.ink = ink * (1.f / 7.f);
    h.accent = L.light * (light_ ? 0.6f : 1.f);
    h.styles = TS_RIPPLE | TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// The water
// =====================================================================================================

void RipplesUniverse::splat(float x0, float y0, float x1, float y1, float r, float amp) {
    if (splats_.size() > 4000) return;
    splats_.push_back({{x0, y0, x1, y1}, {r, amp, 0.f, 0.f}});
}

// A soft push all around a piece's outline (outward, `out` from its faces): `amp` per face.
void RipplesUniverse::outlineRing(const int cells[][2], int n, float dx, float dy, float amp, float out) {
    auto inPiece = [&](int x, int y) {
        for (int i = 0; i < n; i++)
            if (cells[i][0] == x && cells[i][1] == y) return true;
        return false;
    };
    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < n; i++) {
        if (cells[i][1] < Game::HIDDEN) continue;
        const vec3 c = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
        for (auto& dd : d) {
            if (inPiece(cells[i][0] + dd[0], cells[i][1] + dd[1])) continue;
            // board y runs down, world y up
            const float nx = (float)dd[0], ny = -(float)dd[1];
            const float fx = c.x + nx * (0.5f + out), fy = c.y + ny * (0.5f + out);
            splat(fx - ny * 0.4f, fy - nx * 0.4f, fx + ny * 0.4f, fy + nx * 0.4f, 0.16f, amp);
        }
    }
}

void RipplesUniverse::simulate(int steps, float damp) {
    glDisable(GL_BLEND);
    // the obstacles
    mask_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!rects_.empty()) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glUseProgram(progMask_);
        setMat(progMask_, "uVP", vp_);
        set2f(progMask_, "uHalf", viewH_.x / simW_, viewH_.y / simH_);
        glBindVertexArray(maskVao_);
        glBindBuffer(GL_ARRAY_BUFFER, maskVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(rects_.size() * sizeof(Rect)), rects_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)rects_.size());
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);
    }
    // softened: shadows, contact darkening, the damping at the stones' foot
    glUseProgram(progBlur1_);
    set1i(progBlur1_, "uSrc", 0);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < 1; i++) {
        softA_.bind();
        glBindTexture(GL_TEXTURE_2D, i == 0 ? mask_.tex : softB_.tex);
        set2f(progBlur1_, "uDir", 1.f / softA_.w, 0.f);
        ugl::drawFullscreen();
        softB_.bind();
        glBindTexture(GL_TEXTURE_2D, softA_.tex);
        set2f(progBlur1_, "uDir", 0.f, 1.f / softA_.h);
        ugl::drawFullscreen();
    }
    if (steps <= 0) return;
    // the disturbances, added on the first step
    src_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!splats_.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSplat_);
        setMat(progSplat_, "uVP", vp_);
        glBindVertexArray(splatVao_);
        glBindBuffer(GL_ARRAY_BUFFER, splatVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(splats_.size() * sizeof(Splat)), splats_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)splats_.size());
        glDisable(GL_BLEND);
        splats_.clear();
    }
    glUseProgram(progStep_);
    set1i(progStep_, "uState", 0);
    set1i(progStep_, "uSrc", 1);
    set1i(progStep_, "uMask", 2);
    set1i(progStep_, "uSoft", 3);
    set1f(progStep_, "uK", 0.3f);
    set1f(progStep_, "uDamp", damp);
    set1f(progStep_, "uEdgeDamp", 0.08f);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, src_.tex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mask_.tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, softB_.tex);
    for (int i = 0; i < steps; i++) {
        set1f(progStep_, "uSrcK", i == 0 ? 1.f : 0.f);
        state_[1 - cur_].bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state_[cur_].tex);
        ugl::drawFullscreen();
        cur_ = 1 - cur_;
    }
    glActiveTexture(GL_TEXTURE0);
}

// =====================================================================================================
// The pond's lily pads
// =====================================================================================================

bool RipplesUniverse::freeSpot(float x, float y, float r) const {
    const float hw = viewH_.x, hh = viewH_.y;
    if (std::fabs(x) > hw - r - 0.3f || std::fabs(y) > hh - r - 0.3f) return false;
    // not on the basin, not under the HUD's columns, not on the song's title or the universe's name
    if (std::fabs(x) < 6.f + r) return false;
    const float L[4] = {layout::LEFT_COLUMN[0] - 0.6f, layout::LEFT_COLUMN[1] - 0.6f, layout::LEFT_COLUMN[2] + 0.6f, layout::LEFT_COLUMN[3] + 1.f};
    const float R[4] = {layout::RIGHT_COLUMN[0] - 0.6f, layout::RIGHT_COLUMN[1] - 0.6f, layout::RIGHT_COLUMN[2] + 0.6f, layout::RIGHT_COLUMN[3] + 1.f};
    auto inRect = [&](const float* q) { return x + r > q[0] && x - r < q[2] && y + r > q[1] && y - r < q[3]; };
    if (inRect(L) || inRect(R)) return false;
    if (x < 0.f && x + r > -12.4f) return false;        // under the score column: the numbers stay clear
    if (x < -hw + 8.5f && y > hh - 3.6f) return false;   // the title
    if (x < -hw + 9.5f && y < -hh + 4.6f) return false;  // the section and the universe
    for (const Pad& p : pads_) {
        if (p.fade >= 0.f) continue;
        const float dx = p.x - x, dy = p.y - y;
        if (dx * dx + dy * dy < (p.r + r + 0.12f) * (p.r + r + 0.12f)) return false;
    }
    return true;
}

// A new pad (or a lotus) near a place, or near the pads already there; false when the pond is full.
bool RipplesUniverse::placePad(int kind, float nearX, float nearY, float delay) {
    int alive = 0;
    for (const Pad& p : pads_) alive += p.fade < 0.f;
    if (alive >= MAX_PADS) return false;
    for (int tries = 0; tries < 60; tries++) {
        float x, y;
        const float r = kind == 1 ? rng_.range(0.55f, 0.7f) : rng_.range(0.38f, 0.78f);
        if (tries < 40 && !pads_.empty() && rng_.chance(0.8f)) {
            // next to one of the pads (a cluster grows)
            const Pad& o = pads_[(size_t)rng_.irange(0, (int)pads_.size() - 1)];
            if (o.fade >= 0.f) continue;
            const float a = rng_.range(0.f, TAU), dist = o.r + r + rng_.range(0.12f, 0.6f);
            x = o.x + std::cos(a) * dist;
            y = o.y + std::sin(a) * dist;
        } else {
            const float s = nearX < 0.f ? -1.f : 1.f;
            x = s * rng_.range(12.5f, viewH_.x - 1.f);
            if (rng_.chance(0.2f)) x = s * rng_.range(7.f, 11.f);
            y = nearY + rng_.range(-5.f, 5.f);
        }
        if (!freeSpot(x, y, r)) continue;
        Pad p;
        p.x = x; p.y = y; p.r = r;
        p.rot = rng_.range(0.f, TAU);
        p.grow = 0.f;
        p.seed = rng_.uniform();
        p.alpha = 1.f;
        p.delay = delay;
        p.kind = kind;
        p.fade = -1.f;
        pads_.push_back(p);
        return true;
    }
    return false;
}

// =====================================================================================================
// Game events
// =====================================================================================================

RipplesUniverse::CellLook RipplesUniverse::lookOf(int id, float relX, float relY) const {
    CellLook l;
    const uint64_t h = splitmix64((uint64_t)id * 0x9E3779B97F4A7C15ull ^ seed_);
    l.nx = hashf(h) * 180.f + relX;
    l.ny = hashf(h ^ 0x51) * 180.f - relY;
    l.seed = hashf(h ^ 0xA7);
    return l;
}

// Keeps the ids in step with the board: cells that appear behind our back (a new board, a stack inherited from
// another universe) are grouped into pieces of up to four cells of one type.
void RipplesUniverse::syncIds(const Game& g) {
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
                look_[cy][cx] = lookOf(id, (float)(cx - x), (float)(cy - y));
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

void RipplesUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Spawn:
    case GameEvent::Hold:
        serial_++;
        break;
    case GameEvent::Rotate: {
        // the stone turns in the water: a soft ring stirred around its new outline, the water filling where it was
        if (!game.hasPiece()) break;
        int cells[4][2];
        game.pieceCells(game.piece(), cells);
        if (rotated_ || trackSerial_ == serial_) {
            for (auto& o : rotFrom_) {
                bool still = false;
                for (auto& c : cells) still = still || (c[0] == o[0] && c[1] == o[1]);
                if (!still && o[1] >= Game::HIDDEN) {
                    const vec3 p = layout::cellPos((float)o[0], (float)o[1]);
                    ripple(p.x, p.y, 0.3f, -0.08f);
                }
            }
        }
        outlineRing(cells, 4, 0.f, game.pieceVisualY() - game.piece().y, 0.06f, 0.1f);
        rotated_ = true;
        break;
    }
    case GameEvent::Lock: {
        const int id = nextId_++;
        int minX = 99, minY = 99;
        for (auto& c : ev.cells) { minX = std::min(minX, c.x); minY = std::min(minY, c.y); }
        int cells[4][2];
        int n = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            look_[c.y][c.x] = lookOf(id, (float)(c.x - minX), (float)(c.y - minY));
            if (n < 4) { cells[n][0] = c.x; cells[n][1] = c.y; n++; }
        }
        // it settles: a small ring at its foot
        bool hard = false;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W && game.cell(c.x, c.y).dropped) hard = true;
        if (!hard) outlineRing(cells, n, 0.f, 0.f, 0.05f, 0.1f);
        trackSerial_ = -1;
        break;
    }
    case GameEvent::HardDrop: {
        // one clear ring from the contact, a fine wake along the way it slid
        int top = 99, low = -1;
        for (auto& c : ev.cells) { low = std::max(low, c.y); top = std::min(top, c.y); }
        int cells[4][2];
        int n = 0;
        for (auto& c : ev.cells)
            if (n < 4) { cells[n][0] = c.x; cells[n][1] = c.y; n++; }
        const float k = std::min(1.f, 0.4f + ev.count * 0.06f);
        for (int i = 0; i < n; i++) {
            bool below = false;
            for (int j = 0; j < n; j++) below = below || (cells[j][0] == cells[i][0] && cells[j][1] == cells[i][1] + 1);
            if (below || cells[i][1] < Game::HIDDEN) continue;
            const vec3 p = layout::cellPos((float)cells[i][0], (float)cells[i][1]);
            // the contact, just under the stone, and the water pushed out along its sides
            splat(p.x - 0.42f, p.y - 0.64f, p.x + 0.42f, p.y - 0.64f, 0.16f, 0.4f * k);
        }
        outlineRing(cells, n, 0.f, 0.f, 0.12f * k, 0.12f);
        // the wake: two fine troughs along its sides, fading toward where it started
        int minX = 99, maxX = -1;
        for (int i = 0; i < n; i++) { minX = std::min(minX, cells[i][0]); maxX = std::max(maxX, cells[i][0]); }
        const float y0 = layout::cellPos(0.f, (float)std::max(Game::HIDDEN, top - ev.count)).y, y1 = layout::cellPos(0.f, (float)top).y;
        if (ev.count > 1)
            for (int s = 0; s < 2; s++) {
                const float x = layout::cellPos(s ? maxX + 0.5f + 0.12f : minX - 0.5f - 0.12f, 0.f).x;
                const int segs = 4;
                for (int i = 0; i < segs; i++) {
                    const float a = y1 + (y0 - y1) * i / segs, b = y1 + (y0 - y1) * (i + 1) / segs;
                    splat(x, a, x, b, 0.16f, -0.1f * k * (1.f - (float)i / segs));
                }
            }
        trackSerial_ = -1;
        break;
    }
    case GameEvent::Clear: {
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool tetris = ev.count >= 4;
        for (int y : rows) {
            // the stones sink (drawn while they go) and the water closes over them in a line of fine waves
            for (auto& c : ev.cells) {
                if (c.y != y || c.x < 0 || c.x >= Game::W) continue;
                const vec3 p = layout::cellPos((float)c.x, (float)c.y);
                const int id = ids_[c.y][c.x];
                int m = 0;
                if (id) {
                    const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                    for (int i = 0; i < 8; i++) {
                        const int nx = c.x + dx[i], ny = c.y + dy[i];
                        if (nx >= 0 && nx < Game::W && ny >= 0 && ny < Game::H && ids_[ny][nx] == id && (dy[i] == 0)) m |= 1 << i;
                    }
                }
                const CellLook l = id ? look_[c.y][c.x] : lookOf(7000 + c.y * 10 + c.x, 0.f, 0.f);
                sinks_.push_back({p.x, p.y, m, std::max(0, c.type), l, 0.f, 0.f, SINK_T, true});
            }
            const float wy = layout::cellPos(0.f, (float)y).y;
            splat(-4.6f, wy, 4.6f, wy, 0.18f, -0.15f);
            // out in the pond, the basin's walls send it on
            for (int s = -1; s <= 1; s += 2) splat(s * 5.62f, wy - 0.45f, s * 5.62f, wy + 0.45f, 0.13f, 0.08f);
        }
        // the water over the stack: a line of fine waves up the well
        if (!rows.empty()) {
            for (int x = 0; x < Game::W; x++) {
                int y = Game::HIDDEN;
                while (y < Game::H && game.cell(x, y).type < 0) y++;
                // just above the topmost stone of this column (where it shows while the rows above come down)
                const float off = y < Game::H ? game.rowOffset(y) : 0.f;
                const vec3 p = layout::cellPos((float)x, (float)y - off - 1.f);
                if (p.y < 9.5f) splat(p.x - 0.5f, p.y + 0.0f, p.x + 0.5f, p.y + 0.0f, 0.16f, tetris ? 0.22f : 0.12f);
            }
        }
        // a lily pad opens on the outer pond for each row; a Tetris a lotus flower, and three rings over the pond
        const float my = rows.empty() ? 0.f : layout::cellPos(0.f, (float)rows[rows.size() / 2]).y;
        if (tetris) {
            placePad(1, rng_.chance(0.5f) ? -1.f : 1.f, my, 0.5f);
            ringT_ = 0.f;
            ringA_ = 1.f;
        } else {
            for (size_t k = 0; k < rows.size(); k++) placePad(0, (k % 2) ? 1.f : -1.f, my, 0.35f + 0.25f * k);
        }
        // the ids and looks of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            int nb[Game::H][Game::W] = {};
            CellLook nl[Game::H][Game::W];
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { nb[dst][x] = ids_[y][x]; nl[dst][x] = look_[y][x]; }
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) look_[y][x] = nl[y][x];
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack sinks row by row from the top, each row closing in a ring
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = c.x + dx[i], ny = c.y + dy[i];
                    if (nx >= 0 && nx < Game::W && ny >= 0 && ny < Game::H && ids_[ny][nx] == id) m |= 1 << i;
                }
            }
            const CellLook l = id ? look_[c.y][c.x] : lookOf(5000 + c.y * 10 + c.x, 0.f, 0.f);
            const float delay = 0.2f + 0.16f * (c.y - top) + 0.05f * hashf((uint64_t)(c.x * 31 + c.y));
            sinks_.push_back({p.x, p.y, m, std::max(0, c.type), l, 0.f, delay, 0.9f, false});
        }
        std::memset(ids_, 0, sizeof(ids_));
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void RipplesUniverse::addCell(float x, float y, float half, int mask, int type, const CellLook& l, float alpha, int kind, float prog) {
    if (alpha <= 0.003f) return;
    stones_.push_back({{x, y, half, (float)mask}, {l.nx, l.ny, l.seed, (float)std::clamp(type, 0, 6)}, {alpha, prog, (float)kind, 0.f}});
}

void RipplesUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto pieceLook = [&](int cells[4][2], int id, int cx, int cy) {
        int minX = 99, minY = 99;
        for (int i = 0; i < 4; i++) { minX = std::min(minX, cells[i][0]); minY = std::min(minY, cells[i][1]); }
        return lookOf(id, (float)(cx - minX), (float)(cy - minY));
    };
    auto rect = [&](float cx, float cy, float h) { rects_.push_back({{cx - h, cy - h, cx + h, cy + h}}); };
    // the ghost: a fine outline on the water where it will land
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
                addCell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, pieceLook(gc, nextId_, c[0], c[1]), 1.f, 1, 0.f);
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
                return false;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            addCell(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, look_[y][x], 1.f, 0, 0.f);
            rect(p.x, p.y, 0.5f);
        }
    // the falling piece: exactly a locked one at its place
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
            const float a = saturate((10.7f - w.y) / 0.7f);
            addCell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, pieceLook(cells, nextId_, c[0], c[1]), a, 0, 0.f);
            if (a > 0.5f) rect(w.x + (maskX_ - g.pieceVisualX()), w.y, 0.5f);
        }
    }
    // the previews: small stones resting in the outer pond
    auto preview = [&](int type, vec2 center, float sc, float alpha, int id) {
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
            const float ox = c[0] - (minx + maxx) * 0.5f, oy = c[1] - (miny + maxy) * 0.5f;
            const float px = center.x + ox * sc, py = center.y - oy * sc;
            addCell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, pieceLook(cells, id, c[0], c[1]), alpha, 0, 0.f);
            rect(px, py, 0.5f * sc);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.5f : 1.f,
            90000 + g.holdType());
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, 91000 + g.next(i));
    }
    // the kerb
    const float top = viewC_.y + viewH_.y + 1.f;
    rects_.push_back({{-5.36f, -10.36f, -5.06f, top}});
    rects_.push_back({{5.06f, -10.36f, 5.36f, top}});
    rects_.push_back({{-5.36f, -10.36f, 5.36f, -10.06f}});
}

void RipplesUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float RipplesUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) w += f.g[glyphIndex((unsigned char)s[i])].adv * k + track;
    return w - track;
}

void RipplesUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void RipplesUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha, float haloA) {
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

void RipplesUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void RipplesUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const Look& L = look_cur_;
    const vec3 ink = L.ink, halo = L.halo, accent = L.accent;
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
    // the bonus gauge: a fine ring drawn on the water, its charge in the accent
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 120;
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(2.f, 3.5f * s);
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
        for (int i = 0; i < N; i++) seg(TAU * i / N, TAU * (i + 1) / N, th, ink, (g.bonusReady() ? 0.55f : 0.25f) * dimk);
        flushShapes();
        for (int i = 0; i < N; i++) {
            if ((i + 0.5f) / N > fill) break;
            seg(TAU * i / N, TAU * (i + 1) / N, th2, accent, 0.9f * dimk);
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
            place(fontCap_, upper(h.text), base * 0.72f, base * 0.32f, base * 0.06f);
            drawText(fontCap_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.75f * vis);
            continue;
        case HUD_SECTION:
            place(fontCap_, upper(h.text), base * 0.8f, base * 0.36f, -base * 0.04f);
            drawText(fontCap_, q, accent, halo, saturate(0.3f + h.alpha * 1.7f) * vis);
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.2f, base * 0.02f, -base * 0.14f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f) * vis);
            continue;
        case HUD_INFO:
            place(fontCap_, upper(h.text), base * 0.64f, base * 0.26f);
            drawText(fontCap_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL:
            place(fontCap_, upper(h.text), base * 0.68f, base * 0.4f);
            drawText(fontCap_, q, ink, halo, saturate(alpha * 1.2f) * 0.62f);
            continue;
        case HUD_VALUE:
            place(fontCap_, h.text, base * 1.02f, base * 0.1f, -base * 0.06f);
            drawText(fontCap_, q, ink, halo, alpha * 0.95f);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.1f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontCap_, h.text, base * 0.85f, base * 0.1f);
            drawText(fontCap_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontCap_, upper(h.text), base * 1.0f, base * 0.42f, -base * 0.12f);
            drawText(fontCap_, q, ink, halo, alpha, 0.55f);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 1.0f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f, 0.55f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontCap_, upper(h.text), base * 0.9f, base * 0.3f);
                drawText(fontCap_, q, ink, halo, alpha, 0.65f);
            } else {
                place(fontIt_, h.text, base * 0.95f, base * 0.03f);
                drawText(fontIt_, q, h.accent ? accent : ink, halo, alpha, 0.65f);
            }
            continue;
        default:
            place(fontCap_, h.text, base * 0.76f, base * 0.08f);
            drawText(fontCap_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RipplesUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) { overT_ = -1.f; sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(), [](const Sink& s) { return !s.rung; }), sinks_.end()); }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.34f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- The look: a new variant eases in, slow motion turns it to night.
    {
        Look target = mix(look_to_, nightOf(look_to_), smoothstepf(0.f, 1.f, slow_));
        look_cur_ = mix(look_cur_, target, 1.f - std::exp(-dt * (slowOn_ || slow_ > 0.01f ? 2.5f : 1.2f)));
    }
    const Look& L = look_cur_;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float spb = 60.f / std::max(60.f, music.bpm);
    dropT_ += dt;
    float gustT = 0.f;
    if (dropMode_) gustT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.8f * smoothstepf(12.f * spb, 36.f * spb, dropT_));
    gust_ = approach(gust_, gustT, gustT > gust_ ? 1.5f : 0.35f, dt);
    swell_ = approach(swell_, 0.f, 0.22f, dt);
    const float calm = smoothstepf(0.f, 0.7f, bridge_.amount);
    // rain on the beat grid: a drop now and then on the bar at mid, every other beat at a peak
    if (music.hasSong && !f.paused && calm < 0.5f) {
        const double hb = std::floor(music.beat / 2.0);
        if (lastHalfBeat_ >= 0.0 && hb != lastHalfBeat_) {
            const bool bar = std::fmod(hb, 2.0) == 0.0;
            float p = 0.f;
            if (lv_ > 1.5f) p = 0.85f;
            else if (lv_ > 0.6f) p = bar ? 0.6f : 0.f;
            else p = (std::fmod(hb, 8.0) == 0.0) ? 0.5f : 0.f;
            if (gust_ > 0.3f) p = 1.f;
            const int n = lv_ > 1.5f && rng_.chance(0.4f) ? 2 : 1;
            for (int i = 0; i < n; i++) {
                if (!rng_.chance(p)) continue;
                float x, y;
                const bool inWell = lv_ > 1.5f && rng_.chance(0.2f);
                if (inWell) {
                    x = rng_.range(-4.3f, 4.3f);
                    y = rng_.range(2.f, 9.f);
                } else {
                    const float s = rng_.chance(0.5f) ? -1.f : 1.f;
                    x = s * rng_.range(12.2f, viewH_.x - 1.f);   // away from the HUD's columns
                    y = rng_.range(-viewH_.y + 1.f, viewH_.y - 1.f);
                }
                ripple(x, y, 0.15f, rng_.range(0.06f, 0.1f) * (0.7f + 0.3f * wm));
            }
        }
        lastHalfBeat_ = hb;
    }
    // a peak's kicks: a drop on the outer pond
    if (kickIn_ > 0.2f && wp > 0.5f && calm < 0.5f) {
        const float s = rng_.chance(0.5f) ? -1.f : 1.f;
        ripple(s * rng_.range(12.f, viewH_.x - 1.f), rng_.range(-viewH_.y + 1.f, viewH_.y - 1.f), 0.15f, 0.1f * kickIn_);
    }
    kickIn_ = 0.f;
    if (ringT_ >= 0.f) {
        ringT_ += dt;
        if (ringT_ > 6.f) ringT_ = -1.f;
    }

    // ---- The falling piece's wake: it pushes the water ahead of it and leaves a slight trough behind.
    if (game.hasPiece() && !game.over() && !f.paused) {
        const Piece& p = game.piece();
        const float vx = game.pieceVisualX(), vy = game.pieceVisualY();
        if (trackSerial_ == serial_ && trackRot_ == p.rot && trackType_ == p.type) {
            const float ddx = vx - trackX_, ddy = vy - trackY_;
            if ((std::fabs(ddx) > 1e-4f || std::fabs(ddy) > 1e-4f) && std::fabs(ddx) < 1.5f && std::fabs(ddy) < 2.5f) {
                int cells[4][2];
                game.pieceCells(p, cells);
                auto inPiece = [&](int x, int y) {
                    for (auto& c : cells)
                        if (c[0] == x && c[1] == y) return true;
                    return false;
                };
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& c : cells) {
                    if (c[1] < Game::HIDDEN) continue;
                    const vec3 w = layout::cellPos(c[0] + vx - p.x, c[1] + vy - p.y);
                    for (auto& dd : d) {
                        if (inPiece(c[0] + dd[0], c[1] + dd[1])) continue;
                        const float push = dd[0] * ddx + dd[1] * ddy;   // the face's motion along its normal
                        if (std::fabs(push) < 1e-5f) continue;
                        const float nx = (float)dd[0], ny = -(float)dd[1];
                        const float off = 0.5f + 0.12f;
                        const float fx = w.x + nx * off, fy = w.y + ny * off;
                        const float k = dd[1] != 0 ? 0.02f : 0.1f;      // sliding sideways pushes more than falling
                        const float amp = push > 0.f ? k * push : 0.55f * k * push;
                        splat(fx - ny * 0.4f, fy - nx * 0.4f, fx + ny * 0.4f, fy + nx * 0.4f, 0.16f, amp);
                    }
                }
            }
        }
        if (rotated_ || trackSerial_ != serial_ || trackRot_ != p.rot) {
            game.pieceCells(p, rotFrom_);
            rotated_ = false;
        }
        trackSerial_ = serial_;
        trackRot_ = p.rot;
        trackType_ = p.type;
        trackX_ = vx;
        trackY_ = vy;
    }

    // ---- Sinking stones (real time).
    for (Sink& s : sinks_) {
        if (s.delay > 0.f) {
            s.delay -= dt;
            continue;
        }
        if (!s.rung) {
            // game over: each row closes in a ring as it goes
            s.rung = true;
            if (s.mask == 0 || (s.mask & 1) == 0) ripple(s.x, s.y, 0.35f, -0.25f);
        }
        s.t += dt / s.dur;
    }
    sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(), [](const Sink& s) { return s.t >= 1.f; }), sinks_.end());

    // ---- The pads.
    for (Pad& p : pads_) {
        if (p.delay > 0.f) {
            p.delay -= dt;
            if (p.delay <= 0.f) ripple(p.x, p.y, 0.14f + p.r * 0.2f, 0.28f);   // it surfaces with a small ring
            continue;
        }
        p.grow = std::min(1.f, p.grow + wdt / (p.kind == 1 ? 2.4f : 1.4f));
        p.rot += wdt * 0.01f * (p.seed - 0.5f);
        if (p.fade >= 0.f) p.fade += dt / 4.f;
    }
    pads_.erase(std::remove_if(pads_.begin(), pads_.end(), [](const Pad& p) { return p.fade >= 1.f; }), pads_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Instances and obstacles.
    if (game.hasPiece()) {
        if (maskSerial_ != serial_ || std::fabs(maskX_ - game.pieceVisualX()) > 3.f) maskX_ = game.pieceVisualX();
        else maskX_ = approach(maskX_, game.pieceVisualX(), 28.f, dt);
        maskSerial_ = serial_;
    }
    stones_.clear();
    rects_.clear();
    for (const Sink& s : sinks_) {
        if (s.delay > 0.f) addCell(s.x, s.y, 0.5f, s.mask, s.type, s.l, 1.f, 0, 0.f);
        else addCell(s.x, s.y, 0.5f, s.mask, s.type, s.l, 1.f, 2, saturate(s.t));
        if (s.delay > 0.f) rects_.push_back({{s.x - 0.5f, s.y - 0.5f, s.x + 0.5f, s.y + 0.5f}});
    }
    buildBoard(game);
    padInst_.clear();
    for (const Pad& p : pads_) {
        if (p.delay > 0.f) continue;
        const float g = 1.f - (1.f - p.grow) * (1.f - p.grow);
        const float a = (p.fade >= 0.f ? 1.f - p.fade : 1.f) * smoothstepf(0.f, 0.15f, p.grow);
        padInst_.push_back({{p.x, p.y, p.r, p.rot}, {g, p.seed, (float)p.kind, a}, {0.f, 0.f, 0.f, 0.f}});
    }

    // ---- The water: fixed steps of the world's time (slow motion slows the waves).
    stepAcc_ += wdt * SIM_HZ;
    int steps = (int)std::floor(stepAcc_);
    stepAcc_ -= (float)steps;
    steps = std::min(steps, 4);
    const float damp = lerpf(0.9965f, 0.97f, calm);
    simulate(steps, damp);

    // ---- Draw.
    const vec3 Ldir = normalize(vec3(L.lightAt.x - viewC_.x, L.lightAt.y - viewC_.y, CAM_H));
    const vec2 shOff{-Ldir.x / std::max(0.05f, std::sqrt(Ldir.x * Ldir.x + Ldir.y * Ldir.y)) * 0.45f,
                     -Ldir.y / std::max(0.05f, std::sqrt(Ldir.x * Ldir.x + Ldir.y * Ldir.y)) * 0.45f};
    const float lightK = (0.85f + 0.08f * wm + 0.15f * wp + 0.45f * swell_ + 0.2f * gust_) * (1.f - 0.4f * calm);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progWater_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state_[cur_].tex);
    set1i(progWater_, "uState", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mask_.tex);
    set1i(progWater_, "uMask", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, softB_.tex);
    set1i(progWater_, "uSoft", 2);
    glActiveTexture(GL_TEXTURE0);
    set4f(progWater_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progWater_, "uSimTexel", 1.f / simW_, 1.f / simH_);
    set1f(progWater_, "uNormK", 0.45f);
    set3f(progWater_, "uBed", L.bed);
    set3f(progWater_, "uBedFar", L.bedFar);
    set3f(progWater_, "uZenith", L.zenith);
    set3f(progWater_, "uHorizon", L.horizon);
    set3f(progWater_, "uLight", L.light);
    set3f(progWater_, "uKerb", L.kerb);
    set3f(progWater_, "uLightDir", Ldir);
    set1f(progWater_, "uCamH", CAM_H);
    set1f(progWater_, "uRefl", L.refl);
    set1f(progWater_, "uSpec", L.spec);
    set1f(progWater_, "uSpecW", L.specW);
    set1f(progWater_, "uDepth", 0.6f);
    set1f(progWater_, "uShadowK", L.shadowK);
    set1f(progWater_, "uCaustK", L.caustK * 6.f);
    set1f(progWater_, "uAoK", L.aoK);
    set1f(progWater_, "uLightK", lightK);
    set1f(progWater_, "uSheen", light_ ? 1.8f : 2.4f);
    set1f(progWater_, "uEmboss", light_ ? 1.6f : 0.8f);
    set2f(progWater_, "uShadowOff", shOff.x, shOff.y);
    set1f(progWater_, "uTime", worldTime_);
    set1f(progWater_, "uSeed", seedF_);
    set1f(progWater_, "uBreeze", 0.55f * wp * (1.f - slow_));
    set1f(progWater_, "uGust", gust_ * (1.f - 0.6f * slow_));
    set2f(progWater_, "uWind", wind_.x, wind_.y);
    set4f(progWater_, "uRing", 0.f, 0.f, ringT_ >= 0.f ? ringT_ : 0.f, ringT_ >= 0.f ? ringA_ : 0.f);
    set1f(progWater_, "uKerbOn", 1.f);
    set1f(progWater_, "uCalm", calm);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    // the pads and flowers
    if (!padInst_.empty()) {
        glUseProgram(progPad_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state_[cur_].tex);
        set1i(progPad_, "uState", 0);
        set4f(progPad_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set2f(progPad_, "uShadowOff", shOff.x * 0.6f, shOff.y * 0.6f);
        set3f(progPad_, "uPad", L.pad);
        set3f(progPad_, "uPad2", L.pad2);
        set3f(progPad_, "uLotus", L.lotus);
        set3f(progPad_, "uLotusCore", L.lotusCore);
        set3f(progPad_, "uLight", L.light);
        set3f(progPad_, "uLightDir", Ldir);
        set1f(progPad_, "uLightK", lightK);
        set1f(progPad_, "uGlow", L.glow);
        set1f(progPad_, "uShadowA", light_ ? 0.18f : 0.35f);
        drawInst(progPad_, padVao_, padVbo_, padInst_.data(), padInst_.size() * sizeof(Inst), padInst_.size());
    }
    // the stones: they write the scene's alpha
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progStone_);
    set3v(progStone_, "uBase", L.base, 7);
    set3v(progStone_, "uVein", L.vein, 7);
    set3f(progStone_, "uLight", L.light);
    set3f(progStone_, "uLightDir", Ldir);
    set3f(progStone_, "uZenith", L.zenith);
    set3f(progStone_, "uHorizon", L.horizon);
    set3f(progStone_, "uBed", L.bed);
    set3f(progStone_, "uGhost", L.ghost);
    set1f(progStone_, "uLightK", lightK);
    set1f(progStone_, "uCamH", CAM_H);
    set1f(progStone_, "uPolish", L.polish);
    set4f(progStone_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    drawInst(progStone_, stoneVao_, stoneVbo_, stones_.data(), stones_.size() * sizeof(Inst), stones_.size());

    // ---- The glow (the light on the water, the lotus at night).
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThresh", light_ ? 0.8f : 0.7f);
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

    // ---- The frame.
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
    set1f(progFinal_, "uBloomK", L.bloom * (1.f + 0.3f * wp + 0.4f * swell_));
    set1f(progFinal_, "uVignette", L.vignette);
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

const UniverseEntry& universe_ripples() {
    static const UniverseEntry e{
        "RIPPLES", "cut stone blocks in still water seen from above; every move sends fine ripples, cleared rows sink and open lily pads",
        [] { return std::unique_ptr<Universe>(new RipplesUniverse()); },
        // A quiet world: suits calm, airy songs best, still fine for most.
        [](const SongLook& l) { return 0.75f + 0.25f * (1.f - l.density) + 0.15f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

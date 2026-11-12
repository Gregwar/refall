// MIRROR: glass cubes in a liminal room, a gallery of frameless mirrors floating around them.
//
// One world, in real 3D through the layout's camera: a quiet, empty room like a dream of a gallery, a seamless
// floor curving up into walls that round into each other (an infinity cove), a low two-tread step and one slim
// pillar far away, a soft haze. The light is the subject, and it stays minimal: one soft key from the upper left
// (a broad pool around the well, soft shadows, long gradients up the walls) and two colored accent lights out of
// frame on either side (a warm / cool split, or one saturated hue), glowing in the haze and gleaming on the floor.
//
// v3: the room is dark, and the light comes from the glass: every cube glows from within in its own color (the
// falling piece brightest), its light pooling on the glossy floor around the well's base (each column its own color,
// a soft reflection of the cubes in the floor in front) and on the smoked panel behind it; the mirrors bounce the
// stack's light into the room around them and their edges catch its color. The drop makes the light the star: the
// pools bloom and the cubes swell every two beats while the mirrors turn in their rings.
//
// The pieces are solid glass cubes, one per cell, standing in front of a smoked glass panel (a deep, uniform
// backing, not opaque, so the board reads in every variant): each cube refracts the room softly behind it, absorbs
// it in its tint over its path through the glass (seven saturated, elegant glass colors per variant), reflects the
// softbox and the accents on its rounded bevels and shows its back edges through the glass. The key light shines
// through the stack: colored shadows with caustic lines fall on the floor behind the well.
//
// The mirrors: a gallery of six frameless mirrors (rounded corners, a fine bevel, a crisp polished edge), three in
// each side band (never over the well or the HUD columns), each drifting and rolling gently on its own slow curves,
// always turned to show the stack (its normal halves the angle between the camera and the stack). Each is a real
// planar reflection (the room traced in its mirrored ray, the cubes drawn through the mirrored camera), rendered into
// its own tile of one atlas at twice the resolution over its footprint only, read back with a 2 x 2 box filter. Each
// throws the key light back into the room: a soft patch that drifts over the floor and the walls. The music's energy
// sets their speed and reach; a peak's kicks make their edges glint.
//
// Staging: calm is the plain room, dim accents and a slow drift; mid brightens the accents and the mirror ranges
// further; a peak brightens them again. The drop swings the lights to the variant's drop scheme (one swell on its
// downbeat that settles) and each side's mirrors gather into a ring that turns slowly, the mirrors rolling with it like
// the petals of a kaleidoscope, for the whole drop. A cleared row's cubes shatter into glinting shards
// that drift back into the room and dissolve (at the real speed even in slow motion), and the light of the clear
// sweeps across the mirrors; a quad turns the mirror once around, its thrown light sweeping the room. Slow motion
// is a cool moonlight from the right; game over frosts the stack, row by row from the top.
//
// Cost: one traced room pass (full resolution), six small reflection passes (each mirror's footprint, 2x
// supersampled), instanced glass cubes (4x MSAA), a mip-chain bloom.
#include "universes/mirror/mirror.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/mirror/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Manrope, SIL Open Font License, see OFL-Manrope.txt.
const unsigned char FONT_LIGHT[] = {
#include "universes/mirror/Manrope-Light.ttf.inc"
};
const unsigned char FONT_MED[] = {
#include "universes/mirror/Manrope-Medium.ttf.inc"
};

namespace shaders = mirror_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compileC(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON);
    return ugl::program(vs, f.c_str(), name);
}

vec3 lin(unsigned v) {
    return ugl::toLinear(vec3(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f));
}

// The variants: the room, its lights (and the drop's), the HUD's ink, the seven glasses (I O T S Z J L).
struct VariantDef {
    const char* name;
    unsigned albedo, ambTop, ambBot, haze, key, accA, accB, panel;
    float keyK, accAK, accBK, ambK, hazeK, panelTrans;
    unsigned dKey, dAccA, dAccB, dHaze;
    float dKeyK, dAccAK, dAccBK, dAmbK;
    unsigned ink, halo, hudAccent;
    bool pale;
    float density, body;
    unsigned tint[7];
};
const VariantDef VARIANTS[] = {
    // a warm dark: embers of amber light; the drop turns it to a rose and violet night
    {"DUSK", 0x8c8078, 0x6a5a4c, 0x3a3028, 0x0d0a09, 0xffd6b0, 0xff8a50, 0x5a74ff, 0x080607,
     0.07f, 0.4f, 0.2f, 0.03f, 0.0065f, 0.45f,
     0xffc8b0, 0xff3c64, 0x6a4cff, 0x12070d,
     0.06f, 0.55f, 0.45f, 0.03f,
     0xf2e8de, 0x070505, 0xffa070, false, 1.3f, 1.0f,
     {0x35c4ec, 0xffc02c, 0xa45cf0, 0x3fd27c, 0xff4a5e, 0x4474ff, 0xff8030}},
    // a blue dark, one cyan accent; the drop brings a magenta counter-light
    {"NOCTURNE", 0x707680, 0x5c6c90, 0x30364a, 0x05070c, 0xc8d4ff, 0x1fd2ff, 0x2a3f80, 0x05060a,
     0.07f, 0.5f, 0.1f, 0.025f, 0.0075f, 0.45f,
     0xd6e0ff, 0x38fff0, 0xff3c96, 0x0d0712,
     0.07f, 0.65f, 0.5f, 0.03f,
     0xe8eef8, 0x05070b, 0x5fe4ff, false, 1.3f, 1.0f,
     {0x2ee0ff, 0xffd142, 0xb46cff, 0x3ff096, 0xff4d6e, 0x4f7fff, 0xff913e}},
    // a violet dark with pastel glass, pink and blue accents; the drop deepens to violet and teal
    {"VAPOR", 0x9a8aa2, 0x6a5c80, 0x3a2c40, 0x0b0812, 0xffe0f0, 0xff7fc2, 0x6fc6ff, 0x08060b,
     0.07f, 0.42f, 0.36f, 0.03f, 0.0065f, 0.45f,
     0xffe6f4, 0xa65cff, 0x2fe0cc, 0x0d0816,
     0.06f, 0.6f, 0.5f, 0.03f,
     0xf4ecf4, 0x0a060c, 0xff8fc8, false, 1.3f, 1.0f,
     {0x50d8f0, 0xffcf5a, 0xc87af8, 0x62e2a6, 0xff6696, 0x7488ff, 0xffa066}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float CUBE_HALF = 0.46f, CUBE_Z = -0.47f;
const vec3 PANEL_MIN(-5.32f, -10.02f, -1.12f), PANEL_MAX(5.32f, 10.32f, -1.1f);
constexpr float MIR_HW = 2.6f, MIR_HH = 4.3f; // a mirror's half size
constexpr float FY = -10.02f;

float hashf(float x) {
    const float s = std::sin(x * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}
void quatAxis(const vec3& axis, float ang, float q[4]) {
    const float s = std::sin(ang * 0.5f);
    q[0] = axis.x * s; q[1] = axis.y * s; q[2] = axis.z * s; q[3] = std::cos(ang * 0.5f);
}
const float QID[4] = {0.f, 0.f, 0.f, 1.f};
vec3 rotY(const vec3& v, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}
// rotates v around the unit axis k by a (Rodrigues)
vec3 rotAxis(const vec3& v, const vec3& k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MirrorUniverse::init(int width, int height) {
    progRoom_ = compileC(ugl::FULLSCREEN_VS, shaders::ROOM_FS, "mirror room");
    progCube_ = compileC(shaders::CUBE_VS, shaders::CUBE_FS, "mirror cube");
    progCopy_ = ugl::program(ugl::FULLSCREEN_VS, shaders::COPY_FS, "mirror copy");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "mirror final");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "mirror text");
    progPanel_ = ugl::program(shaders::PANEL_VS, shaders::PANEL_FS, "mirror panel");

    // the rounded cube: a cube of 7 x 7 quads a face, its vertices pushed onto a rounded box (the bevels sampled at
    // 0, 15, 30, 45 degrees from each face)
    {
        const float c = 0.86f, rr = 1.f - c;
        const float g[8] = {-1.f, -(c + rr * std::tan(PI / 6.f)), -(c + rr * std::tan(PI / 12.f)), -c,
                            c, c + rr * std::tan(PI / 12.f), c + rr * std::tan(PI / 6.f), 1.f};
        struct F { vec3 n, u, v; };
        const F faces[6] = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                            {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                            {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}};
        std::vector<float> v;
        auto vert = [&](const F& f, float a, float b) {
            const vec3 p = f.n + f.u * a + f.v * b;
            const vec3 cl(clampf(p.x, -c, c), clampf(p.y, -c, c), clampf(p.z, -c, c));
            const vec3 n = normalize(p - cl);
            const vec3 q = cl + n * rr;
            const float o[6] = {q.x, q.y, q.z, n.x, n.y, n.z};
            v.insert(v.end(), o, o + 6);
        };
        for (const F& f : faces)
            for (int i = 0; i < 7; i++)
                for (int j = 0; j < 7; j++) {
                    vert(f, g[i], g[j]); vert(f, g[i + 1], g[j]); vert(f, g[i + 1], g[j + 1]);
                    vert(f, g[i], g[j]); vert(f, g[i + 1], g[j + 1]); vert(f, g[i], g[j + 1]);
                }
        cubeVerts_ = (int)(v.size() / 6);
        glGenVertexArrays(1, &cubeVao_);
        glGenBuffers(1, &cubeVbo_);
        glGenBuffers(1, &instVbo_);
        glBindVertexArray(cubeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    }
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &panelVao_);
    glGenBuffers(1, &panelVbo_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    glGenTextures(1, &boardTex_);
    glBindTexture(GL_TEXTURE_2D, boardTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Game::W, 20, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {0.f, 0.f, 0.f, 0.f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glGenTextures(1, &boardLin_);
    glBindTexture(GL_TEXTURE_2D, boardLin_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Game::W, 20, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    for (auto& row : flash_)
        for (float& v : row) v = 0.f;
    buildFont(fontLight_, FONT_LIGHT);
    buildFont(fontMed_, FONT_MED);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void MirrorUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] mirror font init failed\n");
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
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
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

void MirrorUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera cam = layout::camera(width_, height_, 1.f, 0.f, 0.5f, 600.f);
    vp_ = cam.vp;
    eye_ = cam.eye;
    tanH_ = std::tan(cam.fovDeg * 0.5f * PI / 180.f);
    roomT_.destroy();
    sceneT_.destroy();
    roomT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    // the reflections' atlas: six tiles (3 x 2), each a mirror's footprint at twice the screen's resolution
    tileW_ = 2 * std::max(8, (int)std::ceil(width_ * 0.2f));
    tileH_ = 2 * std::max(8, (int)std::ceil(height_ * 0.4f));
    atlas_.destroy();
    atlas_.create(3 * tileW_, 2 * tileH_, GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT, true);
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

MirrorUniverse::~MirrorUniverse() {
    roomT_.destroy();
    sceneT_.destroy();
    atlas_.destroy();
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (boardTex_) glDeleteTextures(1, &boardTex_);
    if (boardLin_) glDeleteTextures(1, &boardLin_);
    for (GLuint p : {progRoom_, progCube_, progCopy_, progFinal_, progText_, progPanel_}) glDeleteProgram(p);
    for (GLuint v : {cubeVao_, textVao_, panelVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, instVbo_, textVbo_, panelVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontLight_, &fontMed_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void MirrorUniverse::setSong(const SongLook& t, float seconds) {
    if (t.variant >= 0) variant_ = t.variant % VARIANT_COUNT;
    else {
        // dark, minor songs to the night; bright ones to the dawn or the pastels, by the key's hue
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        int v = h < 0.5f ? 0 : 2;
        if (t.hasSong && (t.brightness < 0.4f || (t.minor && t.brightness < 0.5f))) v = 1;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
        variant_ = v;
    }
    rng_ = Rng(t.seed ^ 0x51a7ull);
    path_ = (t.seed % 1000) * 0.0137;
    if (seconds <= 0.f) palSet_ = false;
}

void MirrorUniverse::setLevel(int level, float, float seconds, float) {
    level = std::clamp(level, 0, 2);
    if (level < 2) inDrop_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (rising && level >= 2) dropWait_ = 1.f; // the peak is announced just before its downbeat: wait for drop()
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 2.5f, 6.f);
}

void MirrorUniverse::drop(float strength) {
    if (level_ >= 2) inDrop_ = true;
    dropWait_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * saturate(strength));
    for (int i = 0; i < MIRRORS; i++) sheen(i, 0.7f);
}

void MirrorUniverse::refresh(float strength) { sheen(0, 0.4f + 0.4f * saturate(strength)); }

void MirrorUniverse::sheen(int i, float amp) {
    if (i < 0 || i >= MIRRORS) return;
    if (mir_[i].sheenT < 1.f && mir_[i].sheenAmp > amp) return;
    mir_[i].sheenT = 0.f;
    mir_[i].sheenAmp = amp;
}

vec2 MirrorUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MirrorUniverse::hints() const {
    const VariantDef& d = VARIANTS[variant_];
    TransitionHints h;
    h.background = d.pale ? lin(d.albedo) * 0.6f : lin(d.haze) + lin(d.accA) * 0.03f;
    h.ink = d.pale ? lin(0x2a2830) : lin(0xdce6f2);
    h.accent = lin(d.accA);
    h.light = d.pale;
    h.styles = TS_LUMA | TS_INK | TS_REBUILD;
    return h;
}

vec3 MirrorUniverse::tintOf(int t) const { return lin(VARIANTS[variant_].tint[std::clamp(t, 0, 6)]); }

// =====================================================================================================
// Play
// =====================================================================================================

void MirrorUniverse::onEvent(const GameEvent& ev, const Game&) {
    switch (ev.type) {
    case GameEvent::Lock:
    case GameEvent::HardDrop: {
        const float k = ev.type == GameEvent::HardDrop ? 0.5f : 0.28f;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) flash_[c.y][c.x] = std::max(flash_[c.y][c.x], k);
        break;
    }
    case GameEvent::Clear: {
        // the cubes shatter into shards that drift back into the room, glinting, and dissolve
        for (auto& c : ev.cells) {
            const vec3 w = layout::cellPos((float)c.x, (float)c.y);
            const vec3 tint = tintOf(c.type);
            for (int k = 0; k < 6; k++) {
                Shard s;
                s.p = vec3(w.x + rng_.range(-0.28f, 0.28f), w.y + rng_.range(-0.28f, 0.28f), CUBE_Z + rng_.range(-0.2f, 0.2f));
                s.v = vec3((s.p.x - w.x) * 6.f + rng_.range(-1.2f, 1.2f), rng_.range(0.4f, 3.2f), -rng_.range(1.5f, 4.f));
                s.axis = normalize(vec3(rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f), rng_.range(-1.f, 1.f)));
                s.tint = tint;
                s.spin = rng_.range(3.f, 8.f) * (rng_.uniform() < 0.5f ? -1.f : 1.f);
                s.ang = rng_.range(0.f, TAU);
                s.t = 0.f;
                s.life = rng_.range(0.38f, 0.5f);
                s.size = rng_.range(0.1f, 0.2f);
                s.delay = std::fabs(c.x - 4.5f) * 0.008f;
                shards_.push_back(s);
            }
        }
        for (int i = 0; i < MIRRORS; i++) sheen(i, ev.count >= 4 ? 1.f : 0.55f);
        if (ev.count >= 4 && spinT_ > 4.f) spinT_ = 0.f;
        for (auto& row : flash_)
            for (float& v : row) v = 0.f;
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        overTop_ = Game::H;
        for (auto& c : ev.cells) overTop_ = std::min(overTop_, c.y);
        break;
    }
    default: break;
    }
}

void MirrorUniverse::cube(std::vector<Inst>& out, const vec3& c, float half, const float q[4], const vec3& tint,
                          float kind, float alpha, float flash, float frost) {
    if (alpha <= 0.003f) return;
    Inst in = {{c.x, c.y, c.z, half}, {q[0], q[1], q[2], q[3]}, {tint.x, tint.y, tint.z, kind}, {alpha, flash, frost, 0.f}};
    out.push_back(in);
}

void MirrorUniverse::buildCubes(const Game& g) {
    world_.clear();
    ui_.clear();
    unsigned char board[20][Game::W][4];
    std::memset(board, 0, sizeof(board));
    vec3 colSum[Game::W];
    int colN[Game::W];
    float colTop[Game::W];
    for (int x = 0; x < Game::W; x++) { colSum[x] = vec3(0.f); colN[x] = 0; colTop[x] = -99.f; }
    pieceP_ = vec3(0.f, -99.f, 0.f);
    auto mark = [&](int x, int y, const vec3& t) {
        const int j = Game::H - 1 - y;
        if (j < 0 || j >= 20 || x < 0 || x >= Game::W) return;
        for (int k = 0; k < 3; k++) {
            const float v = k == 0 ? t.x : k == 1 ? t.y : t.z;
            board[j][x][k] = (unsigned char)(255.f * saturate(std::pow(v, 1.15f)));
        }
        board[j][x][3] = 255;
    };
    if (!bare_) {
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Cell& c = g.cell(x, y);
                if (c.type < 0) continue;
                const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
                float frost = 0.f;
                if (overT_ >= 0.f) frost = saturate((overT_ - (y - overTop_) * 0.06f) / 0.5f);
                cube(world_, vec3(p.x, p.y, CUBE_Z), CUBE_HALF, QID, tintOf(c.type), 0.f, 1.f, flash_[y][x], frost);
                // (the lower cells light the floor most)
                const float wgt = 1.f / (1.f + 0.25f * (p.y + 9.5f)) * (1.f - frost * 0.7f);
                colSum[x] += tintOf(c.type) * wgt;
                colN[x]++;
                colTop[x] = std::max(colTop[x], p.y + 0.5f);
                mark(x, y, tintOf(c.type));
            }
        if (g.hasPiece() && !g.over()) {
            const Piece& p = g.piece();
            int cells[4][2];
            g.pieceCells(p, cells);
            Piece gp = p;
            gp.y = g.ghostY();
            const vec3 tint = tintOf(p.type);
            if (gp.y > p.y) {
                int gc[4][2];
                g.pieceCells(gp, gc);
                for (int i = 0; i < 4; i++) {
                    if (gc[i][1] < Game::HIDDEN) continue;
                    const vec3 w = layout::cellPos((float)gc[i][0], (float)gc[i][1]);
                    cube(ui_, vec3(w.x, w.y, CUBE_Z), CUBE_HALF, QID, tint, 1.f, 1.f, 0.f, 0.f);
                }
            }
            const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
            vec3 pc(0.f);
            for (int i = 0; i < 4; i++) {
                const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
                pc += w * 0.25f;
            }
            pieceP_ = vec3(pc.x, pc.y, CUBE_Z);
            pieceC_ = tint;
            for (int i = 0; i < 4; i++) {
                if (cells[i][1] < Game::HIDDEN - 1) continue;
                const vec3 w = layout::cellPos(cells[i][0] + dx, cells[i][1] + dy);
                cube(world_, vec3(w.x, w.y, CUBE_Z), CUBE_HALF, QID, tint, 2.f, 1.f, 0.f, 0.f);
                mark(cells[i][0], (int)std::lround(cells[i][1] + dy), tint);
            }
        }
        auto preview = [&](int type, const vec3& center, float sc, float alpha) {
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
                const vec3 pos(center.x + (cells[i][0] - (minx + maxx) * 0.5f) * sc,
                               center.y - (cells[i][1] - (miny + maxy) * 0.5f) * sc, CUBE_Z * sc);
                cube(ui_, pos, CUBE_HALF * sc, QID, tintOf(type), 3.f, alpha, 0.f, 0.f);
            }
        };
        preview(g.holdType(), layout::HOLD_CENTER, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
        for (int i = 0; i < 3; i++)
            if (g.next(i) >= 0) preview(g.next(i), layout::nextCenter(i), layout::previewScale(i), 1.f);
        // the bonus gauge: a ring of small glass cubes, lit in the accent as it charges
        {
            const VariantDef& V = VARIANTS[variant_];
            const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
            const int N = Game::BONUS_LINES;
            const vec3 acc = lin(V.accA);
            for (int i = 0; i < N; i++) {
                const float a = TAU * i / N;
                const vec3 c(layout::GAUGE_CENTER.x + std::sin(a) * layout::GAUGE_R,
                             layout::GAUGE_CENTER.y + std::cos(a) * layout::GAUGE_R, -0.15f);
                const float part = saturate(fill * N - i);
                float q[4];
                quatAxis(vec3(0, 0, 1), -a + PI * 0.25f, q);
                if (part > 0.02f)
                    cube(ui_, c, 0.14f, q, acc, 3.f, part, g.bonusReady() ? 0.12f : 0.f, 0.f);
                else cube(ui_, c, 0.14f, q, acc, 1.f, 1.f, 0.f, 0.f);
            }
        }
    }
    // the shards
    for (const Shard& s : shards_) {
        if (s.t < s.delay) continue;
        const float u = saturate((s.t - s.delay) / s.life);
        float q[4];
        quatAxis(s.axis, s.ang, q);
        const float alpha = std::pow(1.f - u, 1.4f);
        cube(world_, s.p, s.size * (1.f - 0.4f * u), q, s.tint, 4.f, alpha, 0.22f * std::sin(u * PI), 0.f);
    }
    vec3 avg(0.f);
    float an = 0.f;
    for (int x = 0; x < Game::W; x++) {
        const float k = colN[x] > 0 ? std::min(1.f, 0.35f + 0.65f * std::sqrt(colN[x] / 6.f)) : 0.f;
        const vec3 c = colN[x] > 0 ? colSum[x] * (k / std::max(1e-3f, colSum[x].x + colSum[x].y + colSum[x].z) * 1.6f) : vec3(0.f);
        colLight_[x * 4 + 0] = c.x; colLight_[x * 4 + 1] = c.y; colLight_[x * 4 + 2] = c.z;
        colLight_[x * 4 + 3] = colTop[x];
        avg += c;
        an += colN[x] > 0 ? 1.f : 0.f;
    }
    stackAvg_ = an > 0.f ? avg * (1.f / an) * std::min(1.f, an / 5.f) : vec3(0.f);
    glBindTexture(GL_TEXTURE_2D, boardTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Game::W, 20, GL_RGBA, GL_UNSIGNED_BYTE, board);
    glBindTexture(GL_TEXTURE_2D, boardLin_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Game::W, 20, GL_RGBA, GL_UNSIGNED_BYTE, board);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

// =====================================================================================================
// The mirrors
// =====================================================================================================

void MirrorUniverse::updateMirrors(float dt, float wdt, const vec3& eye) {
    // the gallery: three mirrors in each side band, each drifting on its own slow curves (their speed and reach
    // follow the energy), each turned to show the stack (its normal halving the angle between the camera and the
    // stack), rolling a little; the drop gathers each side into a ring that turns slowly, the mirrors rolling with it
    // like the petals of a kaleidoscope
    path_ += wdt * (0.05f + 0.1f * reach_);
    const float s = (float)path_;
    const float w = 0.6f + 0.6f * reach_;
    const float dk = smoothstepf(0.f, 1.f, dropK_);
    ring_ += wdt * (0.06f + 0.16f * dk);
    spinT_ += dt;
    const vec3 stack(0.f, -4.f, CUBE_Z);
    static const float BASE[3][3] = {{18.f, 5.0f, -9.f}, {26.5f, -0.2f, -17.f}, {18.5f, -5.6f, -10.f}};
    for (int side = 0; side < 2; side++) {
        const float sd = side ? 1.f : -1.f;
        for (int k = 0; k < 3; k++) {
            Mirror& m = mir_[side * 3 + k];
            const float ph = k * 2.1f + side * 1.3f;
            vec3 g(BASE[k][0] + 1.6f * w * std::sin(0.43f * s + ph), BASE[k][1] + 0.7f * w * std::sin(0.61f * s + ph * 1.7f),
                   BASE[k][2] + 2.2f * w * std::sin(0.29f * s + ph * 0.6f));
            const float a = sd * ring_ + k * TAU / 3.f + PI * 0.5f;
            const vec3 rg(22.f + 4.3f * std::cos(a) * sd, -0.4f + 4.3f * std::sin(a), -12.5f + 1.2f * std::sin(a + ring_));
            vec3 c = lerp(g, rg, dk);
            // (never over the HUD column: its inner edge stays out of it)
            c.x = std::max(c.x, 11.6f * (34.6f - c.z) / 34.6f + MIR_HW + 0.3f);
            c.y = std::max(c.y, FY + MIR_HH + 0.6f);
            c.x *= sd;
            vec3 n = normalize(normalize(eye - c) + normalize(stack - c));
            const float spin = TAU * smoothstepf(0.f, 1.f, (spinT_ - 0.15f * (side * 3 + k)) / 2.6f);
            n = rotY(n, 0.03f * std::sin(0.37f * s + ph) + (spinT_ < 4.f ? spin : 0.f));
            const vec3 r = normalize(cross(vec3(0, 1, 0), n)), u = cross(n, r);
            const float roll = lerpf(0.1f * std::sin(0.31f * s + ph * 1.3f), sd * (a - PI * 0.5f), dk);
            m.c = c;
            m.n = n;
            m.r = r * std::cos(roll) + u * std::sin(roll);
            m.u = u * std::cos(roll) - r * std::sin(roll);
            m.alpha = 1.f;
        }
    }
    for (Mirror& m : mir_) {
        if (m.sheenT < 1.f) m.sheenT = std::min(1.f, m.sheenT + dt / 0.95f);
        m.glint = approach(m.glint, 0.f, 3.f, dt);
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

float MirrorUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.6f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void MirrorUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                               std::vector<float>& out) const {
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
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
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void MirrorUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& srgb, float alpha, float bias, float soft) {
    if (q.empty() || !f.ok || alpha <= 0.003f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set4f(progText_, "uInk", srgb.x, srgb.y, srgb.z, saturate(alpha));
    set1f(progText_, "uBias", bias);
    set1f(progText_, "uSoft", soft);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD in Manrope: light figures, widely tracked small capitals for the labels, each line on a soft halo of the
// room's tone; the section and the notices in the variant's accent.
void MirrorUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float bridge = saturate(bridge_.amount);
    const vec3 inkL = lerp(lin(V.ink), bridge_.ink, bridge), accL = lerp(lin(V.hudAccent), bridge_.ink, bridge);
    const vec3 haloS = ugl::toSrgb(lerp(lin(V.halo), bridge_.color, bridge));
    const vec3 inkS = ugl::toSrgb(inkL), accS = ugl::toSrgb(accL);
    if (!panels.empty()) {
        glUseProgram(progPanel_);
        set2f(progPanel_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(panelVao_);
        glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
        for (const HudRect& r : panels) {
            const float q[12] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
            glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
            const vec3 c = r.custom ? r.color : r.accent ? accS : haloS;
            const float a = saturate(r.accent ? r.alpha * 0.35f : r.alpha * 0.9f);
            glUniform4f(ugl::U(progPanel_, "uColor"), c.x * a, c.y * a, c.z * a, a);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    const float s = height_ / 1080.f;
    std::vector<float> q;
    auto upper = [](std::string t) {
        for (char& c : t) c = (char)std::toupper((unsigned char)c);
        return t;
    };
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        auto put = [&](const Font& f, const std::string& text, float size, float track, float dy, const vec3& c, float a, float bias) {
            q.clear();
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
            drawText(f, q, haloS, a * 0.7f, 0.24f, 0.3f);
            drawText(f, q, c, a, bias, 0.f);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            const float sz = base * 0.8f, tr = base * 0.34f, vsz = base * 0.66f, vtr = base * 0.26f;
            const float wName = textWidth(fontMed_, name, sz, tr), gap = base * 0.9f;
            const float wVar = var.empty() ? 0.f : textWidth(fontLight_, var, vsz, vtr);
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            q.clear();
            textQuads(fontMed_, name, x, h.y, sz, tr, q);
            drawText(fontMed_, q, haloS, a * 0.7f, 0.24f, 0.3f);
            drawText(fontMed_, q, inkS, a, 0.f, 0.f);
            if (!var.empty()) {
                q.clear();
                textQuads(fontLight_, var, x + wName + gap, h.y + (sz - vsz) * 0.7f, vsz, vtr, q);
                drawText(fontLight_, q, haloS, a * 0.7f, 0.24f, 0.3f);
                drawText(fontLight_, q, accS, a * 0.9f, 0.02f, 0.f);
            }
            continue;
        }
        case HUD_TITLE: put(fontLight_, h.text, base * 1.05f, base * 0.02f, 0.f, inkS, saturate(h.alpha * 1.3f), 0.02f); continue;
        case HUD_INFO: put(fontLight_, h.text, base * 0.78f, base * 0.08f, 0.f, inkS, alpha * 0.85f, 0.04f); continue;
        case HUD_SECTION: put(fontMed_, upper(h.text), base * 0.8f, base * 0.4f, 0.f, accS, saturate(0.2f + h.alpha * 1.9f), 0.f); continue;
        case HUD_LABEL: put(fontMed_, upper(h.text), base * 0.68f, base * 0.3f, base * 0.04f, inkS, alpha * 0.85f, 0.f); continue;
        case HUD_VALUE: put(fontLight_, h.text, base * 1.1f, base * 0.03f, 0.f, inkS, alpha, 0.03f); continue;
        case HUD_NOTICE: put(fontMed_, h.text, base * 0.95f, base * 0.1f, 0.f, accS, alpha, 0.f); continue;
        case HUD_KEY: put(fontMed_, h.text, base * 0.85f, 0.f, 0.f, inkS, alpha, 0.f); continue;
        case HUD_TOAST: put(fontLight_, upper(h.text), base * 1.25f, base * 0.34f, -base * 0.1f, inkS, alpha, 0.04f); continue;
        case HUD_TOAST_SUB: put(fontMed_, h.text, base * 0.85f, base * 0.12f, 0.f, accS, alpha, 0.f); continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) put(fontLight_, h.text, base * 1.1f, base * 0.12f, -base * 0.1f, inkS, alpha, 0.04f);
            else put(fontLight_, h.text, base * 0.85f, base * 0.04f, 0.f, h.accent ? accS : inkS, alpha, 0.03f);
            continue;
        default: put(fontLight_, h.text, base * 0.82f, base * 0.04f, 0.f, h.accent ? accS : inkS, alpha, 0.03f); continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MirrorUniverse::drawCubes(const std::vector<Inst>& v) {
    if (v.empty()) return;
    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, cubeVerts_, (GLsizei)v.size());
}

void MirrorUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    bare_ = f.bare;
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += f.paused ? 0.f : dt;
    world_t_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the drop: the HUD's section (else the song's drive), once the peak's downbeat has come
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    dropWait_ = std::max(0.f, dropWait_ - dt);
    const bool dropish = sectionKnown ? sectionDrop
                                      : (music.hasSong ? music.speed > 1.33f || (inDrop_ && music.speed > 1.26f) : inDrop_);
    const bool hot = level_ >= 2 && dropWait_ <= 0.f && dropish;
    dropK_ = approach(dropK_, hot ? 1.f : 0.f, hot ? 0.9f : 0.35f, dt);
    swell_ = approach(swell_, 0.f, 0.3f, dt);
    energy_ = approach(energy_, saturate(music.energy), 0.4f, dt);
    reach_ = approach(reach_, saturate(0.15f + 0.45f * energy_ + 0.25f * wm + 0.35f * wp), 0.15f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && wp > 0.5f && kick > 0.5f && kickPrev_ <= 0.3f)
        for (Mirror& m : mir_) m.glint = std::max(m.glint, 0.35f * kick * wp);
    kickPrev_ = kick;
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
    for (auto& row : flash_)
        for (float& v : row) v = approach(v, 0.f, 1.6f, dt);
    for (Shard& s : shards_) {
        s.t += dt;
        if (s.t < s.delay) continue;
        s.v.y -= 9.f * dt;
        s.p += s.v * dt;
        s.v *= std::exp(-1.5f * dt);
        s.ang += s.spin * dt;
    }
    shards_.erase(std::remove_if(shards_.begin(), shards_.end(), [](const Shard& s) { return s.t > s.delay + s.life; }), shards_.end());

    // ---- The palette (eased per variant), the drop's scheme blended in.
    const VariantDef& V = VARIANTS[variant_];
    const float dk = smoothstepf(0.f, 1.f, dropK_);
    const float accLevel = (0.62f + 0.25f * wm + 0.18f * wp) * (1.f + 0.35f * swell_);
    // the glass's light: a steady level per section; the drop's pools bloom, and its cubes swell every two beats
    {
        const float beat2 = music.hasSong ? (float)(music.beat * 0.5 - std::floor(music.beat * 0.5)) : 0.f;
        const float pulse = dk * std::exp(-beat2 * 5.f) * (music.hasSong ? 1.f : 0.f);
        emit_ = (0.34f + 0.04f * wm + 0.05f * wp) * (1.f + 0.25f * dk + 0.3f * pulse + 0.2f * swell_);
        glow_ = (0.9f + 0.15f * wm + 0.15f * wp) * (1.f + 0.6f * dk + 0.5f * pulse + 0.3f * swell_);
    }
    vec3 keyT = lerp(lin(V.key) * V.keyK, lin(V.dKey) * V.dKeyK, dk) * (0.92f + 0.08f * wm);
    vec3 accT[2] = {lerp(lin(V.accA) * V.accAK, lin(V.dAccA) * V.dAccAK, dk) * accLevel,
                    lerp(lin(V.accB) * V.accBK, lin(V.dAccB) * V.dAccBK, dk) * accLevel};
    const float ambK = lerpf(V.ambK, V.dAmbK, dk);
    vec3 ambTopT = lin(V.ambTop) * ambK, ambBotT = lin(V.ambBot) * ambK * 0.55f;
    vec3 hazeT = lerp(lin(V.haze), lin(V.dHaze), dk);
    // slow motion: a cool moonlight
    keyT = lerp(keyT, lin(0xa8c0ff) * (V.keyK * 0.7f), slow_);
    hazeT = lerp(hazeT, hazeT * vec3(0.7f, 0.8f, 1.0f), slow_);
    const float pr = palSet_ ? 1.f - std::exp(-2.5f * dt) : 1.f;
    palSet_ = true;
    keyCol_ = lerp(keyCol_, keyT, pr);
    for (int i = 0; i < 2; i++) accCol_[i] = lerp(accCol_[i], accT[i], pr);
    ambTop_ = lerp(ambTop_, ambTopT, pr);
    ambBot_ = lerp(ambBot_, ambBotT, pr);
    haze_ = lerp(haze_, hazeT, pr);
    albedo_ = lerp(albedo_, lin(V.albedo), pr);
    panelCol_ = lerp(panelCol_, lin(V.panel) * 0.5f, pr);
    hazeK_ = lerpf(hazeK_, V.hazeK * (1.15f - 0.15f * wm + 0.25f * slow_), pr);
    const vec3 keyDir = normalize(lerp(vec3(-0.62f, 0.62f, 0.48f), vec3(0.7f, 0.42f, 0.5f), smoothstepf(0.f, 1.f, slow_)));
    const float tw = world_t_;
    const float drift = 1.f + 1.2f * wp;
    const vec3 accPos[2] = {vec3(-34.f, -3.f + 2.5f * drift * std::sin(tw * 0.08f), -16.f + 5.f * std::sin(tw * 0.05f)),
                            vec3(34.f, 1.5f + 2.5f * drift * std::sin(tw * 0.067f + 2.f), -22.f + 5.f * std::sin(tw * 0.043f + 1.f))};

    updateMirrors(dt, wdt, eye_);
    buildCubes(game);

    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float mirVis = 1.f - smoothstepf(0.f, 0.6f, bridgeK);
    const float aspect = (float)width_ / height_;
    auto lights = [&](GLuint p) {
        set3f(p, "uKeyDir", keyDir);
        set3f(p, "uKeyCol", keyCol_);
        const float ap[6] = {accPos[0].x, accPos[0].y, accPos[0].z, accPos[1].x, accPos[1].y, accPos[1].z};
        const float ac[6] = {accCol_[0].x, accCol_[0].y, accCol_[0].z, accCol_[1].x, accCol_[1].y, accCol_[1].z};
        glUniform3fv(ugl::U(p, "uAccPos"), 2, ap);
        glUniform3fv(ugl::U(p, "uAccCol"), 2, ac);
        set3f(p, "uAmbTop", ambTop_);
        set3f(p, "uAmbBot", ambBot_);
        set3f(p, "uAlbedo", albedo_);
        set3f(p, "uHaze", haze_);
        set3f(p, "uPanelMin", PANEL_MIN);
        set3f(p, "uPanelMax", PANEL_MAX);
        set3f(p, "uPanelCol", panelCol_);
        set1f(p, "uPanelTrans", bare_ ? 1.f : lerpf(V.panelTrans, 1.f, bridgeK));
    };
    auto roomUniforms = [&](int mirCount, const vec2& res) {
        const GLuint p = progRoom_;
        glUseProgram(p);
        lights(p);
        set3f(p, "uEye", eye_);
        set2f(p, "uRes", res.x, res.y);
        set1f(p, "uTanH", tanH_);
        set1f(p, "uAspect", aspect);
        set1f(p, "uBoardOn", bare_ ? 0.f : 1.f);
        set1f(p, "uHazeK", hazeK_);
        set3f(p, "uPoolC", vec3(2.f, FY, -9.f));
        set1f(p, "uPoolR", 27.f);
        set3f(p, "uBridgeCol", bridge_.color);
        set1f(p, "uBridge", bridgeK);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, boardTex_);
        set1i(p, "uBoard", 0);
        float mc[3 * MIRRORS], mr[3 * MIRRORS], mu[3 * MIRRORS], mn[3 * MIRRORS], ma[MIRRORS], ms[MIRRORS], mg[MIRRORS];
        float mrect[4 * MIRRORS], mtile[4 * MIRRORS];
        for (int i = 0; i < MIRRORS; i++) {
            const Mirror& m = mir_[i];
            const vec3* src[4] = {&m.c, &m.r, &m.u, &m.n};
            float* dst[4] = {mc, mr, mu, mn};
            for (int k = 0; k < 4; k++) { dst[k][i * 3] = src[k]->x; dst[k][i * 3 + 1] = src[k]->y; dst[k][i * 3 + 2] = src[k]->z; }
            ma[i] = m.live ? m.alpha * mirVis : 0.f;
            ms[i] = m.sheenT < 1.f ? m.sheenT : 0.f;
            mg[i] = m.glint;
            for (int k = 0; k < 4; k++) { mrect[i * 4 + k] = m.rect[k]; mtile[i * 4 + k] = m.tile[k]; }
        }
        glUniform3fv(ugl::U(p, "uMirC"), MIRRORS, mc);
        glUniform3fv(ugl::U(p, "uMirR"), MIRRORS, mr);
        glUniform3fv(ugl::U(p, "uMirU"), MIRRORS, mu);
        glUniform3fv(ugl::U(p, "uMirN"), MIRRORS, mn);
        glUniform1fv(ugl::U(p, "uMirA"), MIRRORS, ma);
        glUniform1fv(ugl::U(p, "uMirSheen"), MIRRORS, ms);
        glUniform1fv(ugl::U(p, "uMirGlint"), MIRRORS, mg);
        glUniform4fv(ugl::U(p, "uMirRect"), MIRRORS, mrect);
        glUniform4fv(ugl::U(p, "uMirTile"), MIRRORS, mtile);
        set2f(p, "uMirHalf", MIR_HW, MIR_HH);
        set3f(p, "uMirTint", vec3(0.88f, 0.9f, 0.91f));
        set1f(p, "uThrow", 0.05f);
        glUniform4fv(ugl::U(p, "uCol"), Game::W, colLight_);
        set3f(p, "uPieceP", pieceP_);
        set3f(p, "uPieceC", bare_ ? vec3(0.f) : pieceC_);
        set3f(p, "uStackAvg", stackAvg_);
        set1f(p, "uGlow", bare_ ? 0.f : glow_ * (1.f - bridgeK));
        set1i(p, "uMirCount", mirCount);
        set4f(p, "uUvRect", 0.f, 0.f, 1.f, 1.f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mirCount > 0 ? atlas_.tex : 0);
        set1i(p, "uMirAtlas", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, boardLin_);
        set1i(p, "uBoardLin", 2);
        glActiveTexture(GL_TEXTURE0);
    };
    auto cubeUniforms = [&](const mat4& vp, const vec3& eye, int reflectPass, const vec3& pn, float pd) {
        const GLuint p = progCube_;
        glUseProgram(p);
        lights(p);
        setMat(p, "uVP", vp);
        set3f(p, "uEye", eye);
        set2f(p, "uRes", (float)width_, (float)height_);
        set1i(p, "uReflect", reflectPass);
        set1i(p, "uClip", reflectPass);
        set3f(p, "uPN", pn);
        set1f(p, "uPD", pd);
        set3f(p, "uFallback", panelCol_ + haze_ * (V.panelTrans * 0.5f));
        set1f(p, "uDensity", V.density);
        set1f(p, "uBody", V.body);
        set1f(p, "uRefract", 0.010f);
        set1f(p, "uEdgeK", reflectPass ? 0.3f : 0.55f);
        set1f(p, "uSpecK", reflectPass ? 0.25f : 1.f);
        set1f(p, "uEmit", emit_);
        set3f(p, "uInk", bridge_.ink);
        set1f(p, "uBridge", bridgeK);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, roomT_.tex);
        set1i(p, "uScene", 0);
    };

    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    // ---- The reflection passes: the room traced in the mirrored ray, the cubes through the mirrored camera.
    // Each mirror renders into its own tile of the atlas: its screen footprint at twice the resolution (the main
    // pass reads it back with a 2 x 2 box filter: anti-aliased, steady).
    glBindFramebuffer(GL_FRAMEBUFFER, atlas_.fbo);
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < MIRRORS; i++) {
        Mirror& m = mir_[i];
        m.live = false;
        if (m.alpha * mirVis <= 0.001f) continue;
        const float d = dot(m.n, m.c);
        if (dot(eye_, m.n) - d <= 0.1f) continue; // seen from behind
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        bool behind = false;
        for (int k = 0; k < 4; k++) {
            const vec3 cpt = m.c + m.r * ((k & 1) ? MIR_HW : -MIR_HW) + m.u * ((k & 2) ? MIR_HH : -MIR_HH);
            const vec4 h = vp_ * vec4(cpt, 1.f);
            if (h.w <= 0.01f) { behind = true; break; }
            const float sx = (h.x / h.w * 0.5f + 0.5f) * width_, sy = (h.y / h.w * 0.5f + 0.5f) * height_;
            x0 = std::min(x0, sx); x1 = std::max(x1, sx); y0 = std::min(y0, sy); y1 = std::max(y1, sy);
        }
        if (behind) continue;
        const int ix0 = std::max(0, (int)std::floor(x0) - 2), iy0 = std::max(0, (int)std::floor(y0) - 2);
        const int ix1 = std::min(width_, (int)std::ceil(x1) + 2), iy1 = std::min(height_, (int)std::ceil(y1) + 2);
        if (ix1 <= ix0 || iy1 <= iy0) continue;
        const int bw = ix1 - ix0, bh = iy1 - iy0;
        const float ss = std::min(2.f, std::min((float)tileW_ / bw, (float)tileH_ / bh));
        const int tw = std::max(1, (int)(bw * ss)), th = std::max(1, (int)(bh * ss));
        const int tx = (i % 3) * tileW_, ty = (i / 3) * tileH_;
        m.live = true;
        m.rect[0] = (float)ix0 / width_; m.rect[1] = (float)iy0 / height_;
        m.rect[2] = (float)bw / width_; m.rect[3] = (float)bh / height_;
        m.tile[0] = (float)tx / atlas_.w; m.tile[1] = (float)ty / atlas_.h;
        m.tile[2] = (float)tw / atlas_.w; m.tile[3] = (float)th / atlas_.h;
        glViewport(tx, ty, tw, th);
        glScissor(tx, ty, tw, th);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClearDepth(1.0);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        roomUniforms(0, vec2{(float)tw, (float)th});
        set1i(progRoom_, "uReflect", 1);
        set4f(progRoom_, "uUvRect", m.rect[0], m.rect[1], m.rect[2], m.rect[3]);
        // (seen from the mirror, the smoked panel is lighter: the stack stays the subject of the reflection)
        set1f(progRoom_, "uPanelTrans", bare_ ? 1.f : lerpf(std::max(0.8f, V.panelTrans), 1.f, bridgeK));
        set3f(progRoom_, "uPN", m.n);
        set1f(progRoom_, "uPD", d);
        ugl::drawFullscreen();
        if (!world_.empty()) {
            mat4 R = mat4::identity();
            const float nn[3] = {m.n.x, m.n.y, m.n.z};
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) R.m[c * 4 + r] = (r == c ? 1.f : 0.f) - 2.f * nn[r] * nn[c];
                R.m[12 + r] = 2.f * d * nn[r];
            }
            // the footprint's rectangle stretched over the tile (in clip space)
            mat4 S = mat4::identity();
            const float hx = m.rect[2], hy = m.rect[3], cx = 2.f * m.rect[0] + hx - 1.f, cy = 2.f * m.rect[1] + hy - 1.f;
            S.m[0] = 1.f / hx; S.m[12] = -cx / hx;
            S.m[5] = 1.f / hy; S.m[13] = -cy / hy;
            const vec3 eyeM = eye_ - m.n * (2.f * (dot(eye_, m.n) - d));
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glEnable(GL_CLIP_DISTANCE0);
            cubeUniforms(S * vp_ * R, eyeM, 1, m.n, d);
            glBindTexture(GL_TEXTURE_2D, 0);
            drawCubes(world_);
            glDisable(GL_CLIP_DISTANCE0);
            glDisable(GL_DEPTH_TEST);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    int mirCount = 0;
    for (int i = 0; i < MIRRORS; i++)
        if (mir_[i].live) mirCount = i + 1;

    // ---- The room (with the mirrors and the panel), full resolution.
    roomT_.bind();
    roomUniforms(mirCount, vec2{(float)width_, (float)height_});
    set1i(progRoom_, "uReflect", 0);
    ugl::drawFullscreen();

    // ---- The glass (multisampled), over the room.
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progCopy_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, roomT_.tex);
    set1i(progCopy_, "uTex", 0);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    cubeUniforms(vp_, eye_, 0, vec3(0, 0, 1), 0.f);
    drawCubes(world_);
    drawCubes(ui_);
    glDisable(GL_DEPTH_TEST);

    // ---- Resolve, then the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set2f(progFinal_, "uRes", (float)width_, (float)height_);
    set1f(progFinal_, "uBloom", 0.55f * (1.f + 0.3f * dk));
    set1f(progFinal_, "uSlow", slow_ * 0.35f);
    set3f(progFinal_, "uSlowTint", vec3(0.82f, 0.9f, 1.08f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_mirror() {
    static const UniverseEntry e{
        "MIRROR", "glass cubes in a liminal room, softly lit, among floating frameless mirrors that reflect them",
        [] { return std::unique_ptr<Universe>(new MirrorUniverse()); },
        // Calm, spacious songs suit the quiet room best; it fits most songs.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.density) + 0.1f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

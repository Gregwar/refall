// BIOME's frame (see biome.cpp): the shared GL resources, the camera, the passes around the world's own, the stack's
// glass cubes, the gauge and the HUD.
//
// The frame: the sun's shadow map (the world's casters); then the world, multisampled, with depth (the world's passes);
// the rain; the well's pane (and the bridge); what the world draws over the pane; resolved, a copy of it (what the
// stack's glass shows behind it); the ghost, the stack, the falling piece, the previews (glass cubes with the world's
// fill); what the world draws over the stack; the gauge, the HUD; the final pass.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/biome/biome.hpp"
#include "universes/biome/gl.hpp"
#include "universes/biome/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

using namespace biome;

namespace {

// Lora (Italic for titles, Medium for labels and numbers), SIL Open Font License, see OFL-Lora.txt.
const unsigned char FONT_ITALIC[] = {
#include "universes/biome/Lora-Italic.ttf.inc"
};
const unsigned char FONT_ROMAN[] = {
#include "universes/biome/Lora-Medium.ttf.inc"
};

namespace shaders = biome_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

} // namespace

// =====================================================================================================
// Shared GL
// =====================================================================================================

// Toward the sun (or moon): its azimuth and elevation; a negative elevation is a fraction of the sky shown above the
// horizon (so a low sun or the moon keeps its place in the picture as the eye rises).
vec3 Ctx::sunDir() const {
    const layout::Camera ref = layout::camera(width, height);
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    float el = pal.sunEl;
    if (el < 0.f) el = std::atan(-el * (1.f - horizon) * std::tan(half)) * 180.f / PI;
    const float a = pal.sunAz * PI / 180.f, e = el * PI / 180.f;
    return normalize(vec3(std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)));
}

void Ctx::instanced(GLuint& vao, GLuint& vbo, int vec4s) const {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < vec4s; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)(sizeof(float) * 4 * vec4s), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glBindVertexArray(0);
}

void Ctx::skyUniforms(GLuint p, float sunK, float cloudCov, vec2 cloudBand, float skyH, float dustK) const {
    const float w = world();
    set3f(p, "uEye", eye);
    set1f(p, "uFrontZ", FRONT_Z);
    set1f(p, "uTime", time);
    set1f(p, "uSlow", slow);
    set1f(p, "uNight", pal.night);
    set1f(p, "uFlash", flash * w);
    set1f(p, "uCloudT", cloudT);
    set1f(p, "uStars", pal.stars);
    set1f(p, "uStorm", storm * w);
    set3f(p, "uSunDir", sunDir());
    set3f(p, "uSunCol", pal.sun * (pal.lightK * sunK));
    const vec3 amb = lerp(pal.skyTop, pal.skyLow, 0.45f) * pal.ambK;
    set3f(p, "uAmb", lerp(amb, pal.stormSky * 1.45f, storm * 0.5f));
    set4f(p, "uSunDisc", pal.sunR * 1.65f * PI / 180.f, 0.f, pal.moon, 1.f - storm);
    set3f(p, "uSkyTop", pal.skyTop);
    set3f(p, "uSkyLow", pal.skyLow);
    set3f(p, "uDust", pal.dust);
    set3f(p, "uDusk", pal.dusk);
    set3f(p, "uStormSky", pal.stormSky);
    set3f(p, "uStormCloud", pal.stormCloud);
    set3f(p, "uCloud", pal.cloud);
    {
        const float a = flashAz * PI / 180.f, e = 6.5f * PI / 180.f;
        set3f(p, "uFlashDir", vec3(std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)));
    }
    set1f(p, "uSkyH", skyH);
    set1f(p, "uDustK", dustK);
    set1f(p, "uCloudCov", cloudCov);
    set2f(p, "uCloudBand", cloudBand.x, cloudBand.y);
    setMat(p, "uVP", vp);
}

void Ctx::drawParts(GLuint p, const std::vector<float>& v) const {
    if (v.empty()) return;
    set3f(p, "uCamR", camR);
    set3f(p, "uCamU", camU);
    set1f(p, "uPx", px);
    glBindVertexArray(partVao);
    glBindBuffer(GL_ARRAY_BUFFER, partVbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)(v.size() / 16));
}

// =====================================================================================================
// Setup
// =====================================================================================================

bool BiomeUniverse::init(int width, int height) {
    progRain_ = compile(ugl::FULLSCREEN_VS, shaders::RAIN_FS, "biome rain", {});
    progVeil_ = compile(ugl::FULLSCREEN_VS, shaders::VEIL_FS, "biome pane", {});
    progSprite_ = compile(shaders::SPRITE_VS, shaders::SPRITE_FS, "biome gauge", {});
    progText_ = compile(shaders::TEXT_VS, shaders::TEXT_FS, "biome text", {});
    progShape_ = compile(shaders::SHAPE_VS, shaders::SHAPE_FS, "biome shape", {});
    progPost_ = compile(ugl::FULLSCREEN_VS, shaders::POST_FS, "biome post", {});

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &c_.quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, c_.quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    c_.instanced(cellVao_, cellVbo_, 4);
    c_.instanced(spriteVao_, spriteVbo_, 4);
    c_.instanced(c_.partVao, c_.partVbo, 4);

    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    // the sun's shadow map
    glGenTextures(1, &c_.shadowTex);
    glBindTexture(GL_TEXTURE_2D, c_.shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Ctx::SHADOW_RES, Ctx::SHADOW_RES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, c_.shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] biome shadow FBO incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    samples_ = std::clamp((int)maxSamples, 1, 4);

    buildFont(fontItalic_, FONT_ITALIC);
    buildFont(fontRoman_, FONT_ROMAN);
    c_.width = std::max(1, width);
    c_.height = std::max(1, height);
    bool ok = true;
    for (auto& w : worlds_) ok = w->init() && ok;
    c_.pal = w_->palette(0);
    resize(width, height);
    return ok && glGetError() == GL_NO_ERROR;
}

// Glyphs baked as signed distance fields (they scale cleanly).
void BiomeUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] biome font init failed\n");
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

void BiomeUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    c_.width = width_;
    c_.height = height_;
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    c_.viewH = v2(hw, hh);
    c_.viewC = v2(cam.target.x, cam.target.y);
    c_.camDist = cam.dist;
    c_.px = 2.f * hh / height_;
    c_.sceneT.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    c_.bgT.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the world's multisampled target
    if (c_.msFbo) {
        glDeleteFramebuffers(1, &c_.msFbo);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &c_.msFbo);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, c_.msFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] biome world FBO incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    setCamera();
}

BiomeUniverse::~BiomeUniverse() {
    for (auto& w : worlds_) w.reset();
    c_.sceneT.destroy();
    c_.bgT.destroy();
    for (GLuint p : {progVeil_, progRain_, progSprite_, progText_, progShape_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, spriteVao_, c_.partVao, textVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {c_.quadVbo, cellVbo_, spriteVbo_, c_.partVbo, textVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    glDeleteFramebuffers(1, &c_.msFbo);
    glDeleteRenderbuffers(1, &msColor_);
    glDeleteRenderbuffers(1, &msDepth_);
    glDeleteFramebuffers(1, &shadowFbo_);
    glDeleteTextures(1, &c_.shadowTex);
    for (Font* f : {&fontItalic_, &fontRoman_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Camera
// =====================================================================================================

// The layout's reference camera, raised above the world (at the world's height: the sea raises it as it rises, so its
// surface stays seen from above) and looking straight ahead, its image shifted back down: the board plane lands on the
// reference camera's pixels.
void BiomeUniverse::setCamera() {
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float f = 1.f / std::tan(half);
    const float eyeY = w_->eyeY();
    c_.eye = vec3(ref.target.x, eyeY, ref.target.z + ref.dist);
    c_.horizon = f * (eyeY - ref.target.y) / ref.dist;
    c_.view = mat4::lookAt(c_.eye, c_.eye + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = c_.horizon;
    c_.vp = shift * mat4::perspective(2.f * half, aspect, 1.f, 900.f) * c_.view;
    c_.invVP = inverse(c_.vp);
    c_.camR = vec3(1.f, 0.f, 0.f);
    c_.camU = vec3(0.f, 1.f, 0.f);
}

// =====================================================================================================
// The stack's cubes
// =====================================================================================================

void BiomeUniverse::cube(float x, float y, float size, int type, float alpha, bool ghost, float seed, float crack) {
    if (alpha <= 0.003f) return;
    type = std::clamp(type, 0, 6);
    const vec3 col = c_.pal.cell[type];
    cells_.push_back({{x, y, size, (float)type}, {col.x, col.y, col.z, alpha}, {ghost ? 1.f : 0.f, crack, 0.f, 0.f}, {seed, 0.f, 0.1f, -0.13f}});
}

void BiomeUniverse::flushCells() {
    if (cells_.empty()) return;
    const GLuint p = w_->cellProgram();
    const Palette& pal = c_.pal;
    glUseProgram(p);
    setMat(p, "uVP", c_.vp);
    set1f(p, "uPx", c_.px);
    set1f(p, "uTime", c_.time);
    set1f(p, "uBridge", smoothstepf(0.3f, 0.95f, c_.bridge.amount));
    set3f(p, "uBridgeInk", c_.bridge.ink);
    set1f(p, "uLightPanel", pal.lightPanel);
    set2f(p, "uRes", (float)width_, (float)height_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, c_.bgT.tex);
    set1i(p, "uBg", 0);
    // where the cubes see the sun from: the variant's, or from the 3D sun's side, high
    if (pal.sunY != 0.f) set2f(p, "uSunP", pal.sunX, pal.sunY);
    else {
        const vec3 L = c_.sunDir();
        set2f(p, "uSunP", L.x * 14.f, 6.f + std::max(L.y, 0.2f) * 10.f);
    }
    set3f(p, "uSunCol", pal.sun * (0.9f + 0.2f * pal.lightK) * (1.f - 0.35f * c_.storm));
    set3f(p, "uSkyTop", pal.skyTop);
    set3f(p, "uSkyLow", pal.skyLow);
    set3f(p, "uEnvLow", pal.envLow);
    set1f(p, "uNight", pal.night);
    w_->cellUniforms(p);
    // the cubes are seen a little from above and from the well's middle: each one's top and inner side lie behind
    // its neighbours above and toward the middle, so those are drawn after it (bottom row first, outer ones first)
    std::stable_sort(cells_.begin(), cells_.end(), [](const CellInst& a, const CellInst& b) {
        const float ra = std::round(a.a0[1] * 4.f), rb = std::round(b.a0[1] * 4.f);
        if (ra != rb) return ra < rb;
        return std::fabs(a.a0[0]) > std::fabs(b.a0[0]);
    });
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    cells_.clear();
}

// =====================================================================================================
// Text
// =====================================================================================================

float BiomeUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
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

float BiomeUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                               std::vector<float>& out) const {
    if (!f.ok) return 0.f;
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
    return pen - track - x;
}

void BiomeUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha, float bias) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, c_.bridge.amount);
    const vec3 c = lerp(ink, c_.bridge.ink, b);
    // a soft halo of the other value under the letters
    const bool light = ugl::luma(c) > 0.3f;
    set3f(progText_, "uInk", c);
    set3f(progText_, "uHalo", light ? vec3(0.015f, 0.012f, 0.01f) : lerp(c_.pal.skyLow, vec3(1.f), 0.5f));
    set1f(progText_, "uHaloA", (light ? 0.42f : 0.35f) * (1.f - b));
    set1f(progText_, "uAlpha", alpha);
    set1f(progText_, "uBias", bias);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// How light the world is behind a point (board space): the world's own color there, dusk in slow motion, the well's
// pane over it.
float BiomeUniverse::bgLuma(float bx, float by) const {
    const Palette& pal = c_.pal;
    const float ndc = (by - c_.viewC.y) / c_.viewH.y;
    vec3 c = w_->behind(bx, by, ndc);
    c = lerp(c, pal.dusk, c_.slow * 0.6f);
    if (std::fabs(bx) < 5.1f && std::fabs(by) < 10.1f)
        c = lerp(c, pal.panel, lerpf(pal.panelA, std::min(0.88f, pal.panelA * 1.45f), smoothstepf(2.f, 10.f, w_->paneDeepY() - by)));
    return ugl::luma(c);
}

// The HUD in BIOME's typography: Lora, a book face like a naturalist's field notes; italic for the song, the section
// and the celebrations, spaced capitals for labels. The ink dark or light, whichever reads best over what is behind it,
// always on a soft halo.
void BiomeUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const Palette& pal = c_.pal;
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        auto quad = [&](float x0, float y0, float x1, float y1, const vec3& c, float a) {
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& r : q) v.insert(v.end(), r, r + 6);
        };
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal.accent : pal.hudPanel;
            quad(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const vec3 darkInk = pal.inkDark, lightInk = pal.inkLight;
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        auto inkAt = [&](float px, float py) {
            const float bx = c_.viewC.x + (px / width_ * 2.f - 1.f) * c_.viewH.x;
            const float by = c_.viewC.y - (py / height_ * 2.f - 1.f) * c_.viewH.y;
            // the ink with the most contrast against what is behind it
            const float l = (std::sqrt(bgLuma(bx, by - 0.3f)) + std::sqrt(bgLuma(bx, by)) + std::sqrt(bgLuma(bx, by + 0.3f))) / 3.f;
            return std::fabs(l - std::sqrt(ugl::luma(darkInk))) > std::fabs(l - std::sqrt(ugl::luma(lightInk))) ? darkInk : lightInk;
        };
        vec3 ink = inkAt(h.x < 0.f ? width_ + h.x : h.x, h.y + base * 0.5f);
        vec3 accent = lerp(pal.accent, ink, 0.25f);
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            const float x = ugl::hudLeft(h, w, width_);
            ink = inkAt(x + w * 0.5f, h.y + dy + size * 0.8f);
            accent = lerp(pal.accent, ink, 0.25f);
            textQuads(f, text, x, h.y + dy, size, track, q);
        };
        auto titleCase = [](const std::string& str) {
            std::string o = str;
            bool start = true;
            for (char& c : o) {
                if (std::isalpha((unsigned char)c)) {
                    c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
                    start = false;
                } else start = c == ' ' || c == '-';
            }
            return o;
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            // "Biome" in the italic, the variant's name in small spaced capitals after it.
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            name = titleCase(name);
            const float sz = base * 1.4f, vsz = base * 0.64f, vtr = vsz * 0.3f;
            const float wName = textWidth(fontItalic_, name, sz, 0.f);
            const float wVar = var.empty() ? 0.f : textWidth(fontRoman_, var, vsz, vtr);
            const float gap = base * 0.8f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            textQuads(fontItalic_, name, x, h.y - base * 0.12f, sz, 0.f, q);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            drawText(fontItalic_, q, inkAt(x + wName * 0.5f, h.y + sz * 0.4f), a, 0.03f);
            if (!var.empty()) {
                q.clear();
                textQuads(fontRoman_, var, x + wName + gap, h.y + base * 0.22f, vsz, vtr, q);
                drawText(fontRoman_, q, inkAt(x + wName + gap + wVar * 0.5f, h.y + base * 0.22f + vsz * 0.5f), a * 0.85f, 0.02f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontItalic_, titleCase(h.text), base * 1.35f, 0.f);
            drawText(fontItalic_, q, ink, saturate(0.2f + h.alpha * 1.9f), 0.03f);
            continue;
        case HUD_TITLE:
            place(fontItalic_, h.text, base * 1.25f, 0.f, -base * 0.12f);
            drawText(fontItalic_, q, ink, saturate(h.alpha * 1.3f), 0.03f);
            continue;
        case HUD_INFO:
            place(fontRoman_, h.text, base * 0.78f, base * 0.12f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.85f, 0.02f);
            continue;
        case HUD_LABEL:
            place(fontRoman_, h.text, base * 0.72f, base * 0.3f);
            drawText(fontRoman_, q, ink, saturate(alpha * 1.2f) * 0.78f, 0.02f);
            continue;
        case HUD_VALUE:
            place(fontRoman_, h.text, base * 1.08f, base * 0.04f);
            drawText(fontRoman_, q, ink, alpha, 0.02f);
            continue;
        case HUD_NOTICE:
            place(fontItalic_, h.text, base * 1.15f, 0.f);
            drawText(fontItalic_, q, accent, alpha, 0.03f);
            continue;
        case HUD_KEY:
            place(fontRoman_, h.text, base * 0.88f, base * 0.05f);
            drawText(fontRoman_, q, ink, alpha, 0.02f);
            continue;
        case HUD_TOAST:
            place(fontItalic_, titleCase(h.text), base * 1.5f, 0.f, -base * 0.2f);
            drawText(fontItalic_, q, ink, alpha, 0.04f);
            continue;
        case HUD_TOAST_SUB:
            place(fontRoman_, h.text, base * 0.82f, base * 0.2f);
            drawText(fontRoman_, q, accent, alpha * 0.9f, 0.02f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontItalic_, titleCase(h.text), base * 1.3f, 0.f, -base * 0.12f);
                drawText(fontItalic_, q, ink, alpha, 0.04f);
            } else {
                place(fontRoman_, h.text, base * 0.86f, base * 0.1f);
                drawText(fontRoman_, q, h.accent ? accent : ink, alpha, 0.02f);
            }
            continue;
        default:
            place(fontRoman_, h.text, base * 0.84f, base * 0.06f);
            drawText(fontRoman_, q, h.accent ? accent : ink, alpha, 0.02f);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

// The rain slanting across the world in a storm.
void BiomeUniverse::drawRain() {
    const float a = c_.storm * c_.world();
    if (a <= 0.01f) return;
    glDisable(GL_DEPTH_TEST);
    glUseProgram(progRain_);
    set4f(progRain_, "uView", c_.viewC.x, c_.viewC.y, c_.viewH.x, c_.viewH.y);
    set1f(progRain_, "uPx", c_.px);
    set1f(progRain_, "uRainA", a);
    set1f(progRain_, "uTimeR", c_.time);
    set1f(progRain_, "uSlant", w_->rainSlant());
    set1f(progRain_, "uFloorY", w_->rainFloorY());
    set3f(progRain_, "uRainCol", w_->rainColor());
    ugl::drawFullscreen();
}

// The well's pane over the world (and the bridge: the world fading to its color).
void BiomeUniverse::drawPane() {
    const float bridge = saturate(c_.bridge.amount);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progVeil_);
    set4f(progVeil_, "uView", c_.viewC.x, c_.viewC.y, c_.viewH.x, c_.viewH.y);
    set1f(progVeil_, "uPx", c_.px);
    set1f(progVeil_, "uPanelA", c_.pal.panelA * (1.f - bridge));
    set1f(progVeil_, "uDeepY", w_->paneDeepY());
    set1f(progVeil_, "uBridge", smoothstepf(0.05f, 0.85f, bridge));
    set3f(progVeil_, "uPanel", c_.pal.panel);
    set3f(progVeil_, "uEdge", c_.pal.edge);
    set3f(progVeil_, "uBridgeCol", c_.bridge.color);
    ugl::drawFullscreen();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void BiomeUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const float dt = f.dt;
    {
        const Palette target = w_->palette(localOf(variant_));
        if (!palSet_) { c_.pal = target; palSet_ = true; }
        else lerpPalette(c_.pal, target, 1.f - std::exp(-dt * 1.0f));
    }
    // the section, from the HUD (a drop brings the storm)
    sectionKnown_ = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown_ = true;
            sectionDrop_ = h.text == "DROP";
        }
    c_.slow = approach(c_.slow, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (c_.over && !game.over()) c_.over = false;
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, c_.slow);
    if (!f.paused) {
        c_.time += dt;
        watchBoard(game);
        simulate(game, f.music, dt, wdt);
    }
    const float bridge = saturate(c_.bridge.amount);
    setCamera();

    // ---- The sun's shadow map.
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, Ctx::SHADOW_RES, Ctx::SHADOW_RES);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.f);
    w_->shadowPass();
    glDisable(GL_POLYGON_OFFSET_FILL);

    // ---- The world, the rain, the well's pane, what flies over it.
    glBindFramebuffer(GL_FRAMEBUFFER, c_.msFbo);
    glViewport(0, 0, width_, height_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, c_.shadowTex);
    glActiveTexture(GL_TEXTURE0);
    w_->drawWorld();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    drawRain();
    drawPane();
    w_->drawAfterPane();
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // ---- Resolved, and a copy of it: what the stack's glass shows behind it.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, c_.msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, c_.sceneT.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, c_.sceneT.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, c_.bgT.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    c_.sceneT.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The ghost, the stack, the falling piece: every cell its own glass cube.
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        Piece gp = p;
        gp.y = game.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            game.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cube(w.x, w.y, 1.f, p.type, 1.f, true, 0.f);
            }
        }
    }
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const int t = game.cell(x, y).type;
            if (t < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
            cube(p.x, p.y, 1.f, t, 1.f, false, std::fmod((float)(x * 7 + y * 13 + t), 50.f));
        }
    for (const Dying& d : dying_) cube(d.x, d.y, 1.f, d.type, 1.f, false, d.x * 3.f + d.y, smoothstepf(d.delay - 0.15f, d.delay, d.t));
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        int cells[4][2];
        game.pieceCells(p, cells);
        const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cube(w.x, w.y, 1.f, p.type, 1.f, false, (float)(p.type * 7 + i * 13));
        }
    }
    // ---- Previews: four small cubes each.
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (int i = 0; i < 4; i++) {
            const auto& c = cells[i];
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            cube(pos.x, pos.y, sc, type, alpha, false, (float)(type * 3 + i * 11));
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (game.next(i) >= 0) preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
    }
    flushCells();
    w_->drawAfterStack();

    // ---- The bonus gauge: a round glass filling with the world's water or seeds.
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float R = layout::GAUGE_R;
        const float fill = game.bonusActive() ? saturate(game.bonusLeft() / 10.f) : saturate(gaugeShown_);
        const float ready = game.bonusReady() || game.bonusActive() ? 1.f : 0.f;
        const vec3 col = c_.pal.gauge;
        const float v[16] = {gc.x, gc.y, R, R, 0.f, 0.f, fill, ready, col.x, col.y, col.z, 1.f - 0.6f * smoothstepf(0.3f, 0.95f, bridge), 0.f, 0.f, 0.f, 0.f};
        glUseProgram(progSprite_);
        setMat(progSprite_, "uVP", c_.vp);
        set1f(progSprite_, "uTime", c_.time);
        set1f(progSprite_, "uPx", c_.px);
        set1f(progSprite_, "uLightPanel", c_.pal.lightPanel);
        set1f(progSprite_, "uSeeds", w_->seedGauge() ? 1.f : 0.f);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
    }
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);

    // ---- Post.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, c_.sceneT.tex);
    set1i(progPost_, "uScene", 0);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", dimT_);
    ugl::drawFullscreen();
}

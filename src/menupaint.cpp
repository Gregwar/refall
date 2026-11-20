#include "menupaint.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "mathutil.hpp"
#include "ugl.hpp"

namespace {

const unsigned char TTF_JOSEFIN_LIGHT[] = {
#include "universes/host/JosefinSans-Light.ttf.inc"
};
const unsigned char TTF_JOSEFIN[] = {
#include "universes/host/JosefinSans-Regular.ttf.inc"
};
const unsigned char* TTFS[Painter::FONT_COUNT] = {TTF_JOSEFIN_LIGHT, TTF_JOSEFIN};

const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aLocal;
layout(location = 2) in vec2 aHalf;
layout(location = 3) in vec4 aCol;
layout(location = 4) in vec4 aPrm; // radius, soft, line width, mode
uniform vec2 uRes;
uniform vec3 uView; // a layer's origin (pixels) and scale (1: none)
out vec2 vLocal;
out vec2 vHalf;
out vec4 vCol;
out vec4 vPrm;
void main() {
    vLocal = aLocal; vHalf = aHalf; vCol = aCol; vPrm = aPrm;
    vec2 p = (aPos - uView.xy) * uView.z; // (the shapes' own distances stay in the frame's pixels)
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
const char* FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vHalf;
in vec4 vCol;
in vec4 vPrm;
out vec4 o;
uniform sampler2D uTex;
uniform sampler2D uFrost; // the frame drawn so far, blurred (Painter::frostSource)
uniform vec2 uRes;
void main() {
    float a;
    if (vPrm.w > 2.5 && vPrm.w < 3.5) { // flat
        a = vCol.a;
        o = vec4(vCol.rgb * a, a);
        return;
    }
    if (vPrm.w > 1.5 && vPrm.w < 2.5) { // an image (premultiplied), tinted
        vec4 t = texture(uTex, vLocal);
        o = vec4(t.rgb * vCol.rgb, t.a) * vCol.a;
        return;
    }
    if (vPrm.w > 0.5 && vPrm.w < 1.5) {
        a = texture(uTex, vLocal).r;
    } else if (vPrm.w > 4.5) { // an arc: radius vPrm.x, width vPrm.z, centred on the angle vHalf.x, vHalf.y either side
        float d = mod(atan(vLocal.y, vLocal.x) - vHalf.x + 3.14159265, 6.2831853) - 3.14159265;
        float dist = abs(d) <= vHalf.y ? abs(length(vLocal) - vPrm.x)
                                       : length(vLocal - vPrm.x * vec2(cos(vHalf.x + sign(d) * vHalf.y), sin(vHalf.x + sign(d) * vHalf.y)));
        float s = max(vPrm.y, 0.5);
        a = 1.0 - smoothstep(-s * 0.5, s * 0.5, dist - vPrm.z * 0.5);
        if (s > 2.0) a = a * a * (3.0 - 2.0 * a);
    } else {
        float r = min(vPrm.x, min(vHalf.x, vHalf.y));
        vec2 q = abs(vLocal) - vHalf + r;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
        if (vPrm.z > 0.0 && vPrm.w < 3.5) d = abs(d + vPrm.z * 0.5) - vPrm.z * 0.5;
        float s = max(vPrm.y, 0.5);
        a = 1.0 - smoothstep(-s * 0.5, s * 0.5, d);
        if (s > 2.0) a = a * a * (3.0 - 2.0 * a); // glows fade smoothly
    }
    a *= vCol.a;
    if (vPrm.w > 3.5 && vPrm.w < 4.5) { // frosted glass: what lies behind, blurred and a little lifted, tinted (vPrm.z: how much)
        vec3 b = texture(uFrost, gl_FragCoord.xy / uRes).rgb;
        float g = fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
        o = vec4((mix(b * 1.08 + 0.015, vCol.rgb, vPrm.z) + g * 0.012) * a, a);
        return;
    }
    o = vec4(vCol.rgb * a, a);
}
)";

// the frost's blur: a 9-tap gaussian along uDir (texels), on a quarter of the frame
const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 o;
uniform sampler2D uT;
uniform vec2 uDir;
void main() {
    const float W[5] = float[](0.227, 0.194, 0.122, 0.054, 0.016);
    vec3 c = texture(uT, vUv).rgb * W[0];
    for (int i = 1; i < 5; i++)
        c += (texture(uT, vUv + uDir * float(i) * 1.5).rgb + texture(uT, vUv - uDir * float(i) * 1.5).rgb) * W[i];
    o = vec4(c, 1.0);
}
)";


} // namespace

Painter::~Painter() {
    if (prog_) glDeleteProgram(prog_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (upProg_) glDeleteProgram(upProg_);
    if (blurProg_) glDeleteProgram(blurProg_);
    kalTgt_.destroy();
    frostA_.destroy();
    frostB_.destroy();
    if (vao_) glDeleteVertexArrays(1, &vao_);
    for (auto& f : fonts_)
        if (f.tex) glDeleteTextures(1, &f.tex);
}

Painter::FontData& Painter::font(Font id) {
    FontData& f = fonts_[id];
    if (f.tried) return f;
    f.tried = true;
    // 128 px glyphs read down to ~12 px through mipmaps: at mip level k a texel covers 2^k atlas pixels, so the glyphs
    // are 16 px apart (mip 4 still keeps a texel between neighbours) and the mip chain stops at level 4.
    const int W = 2048, H = 2048, PAD = 16;
    std::vector<unsigned char> bmp((size_t)W * H);
    f.packed.resize(sizeof(stbtt_packedchar) * 95);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, bmp.data(), W, H, 0, PAD, nullptr)) return f;
    stbtt_PackSetOversampling(&pc, 1, 1);
    f.ok = stbtt_PackFontRange(&pc, TTFS[id], 0, f.px, 32, 95, (stbtt_packedchar*)f.packed.data()) != 0;
    stbtt_PackEnd(&pc);
    if (!f.ok) { std::fprintf(stderr, "[menu] could not pack font %d\n", (int)id); return f; }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, bmp.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return f;
}

void Painter::begin(int w, int h) {
    w_ = std::max(1, w);
    h_ = std::max(1, h);
    if (!prog_) {
        prog_ = ugl::program(VS, FS, "menu painter");
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        const int st = sizeof(V);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (void*)(10 * sizeof(float)));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w_, h_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    additive_ = false;
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    verts_.clear();
    curTex_ = 0;
}

void Painter::end() {
    flush();
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void Painter::flush() {
    if (verts_.empty()) return;
    glUseProgram(prog_);
    if (layer_) {
        ugl::set2f(prog_, "uRes", (float)layerTgt_.w, (float)layerTgt_.h);
        ugl::set3f(prog_, "uView", vec3(layerX_, layerY_, layerS_));
    } else {
        ugl::set2f(prog_, "uRes", (float)w_, (float)h_);
        ugl::set3f(prog_, "uView", vec3(0.f, 0.f, 1.f));
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, curTex_);
    ugl::set1i(prog_, "uTex", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, frostA_.tex);
    glActiveTexture(GL_TEXTURE0);
    ugl::set1i(prog_, "uFrost", 1);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(V), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts_.size());
    verts_.clear();
}

void Painter::frostSource() {
    flush();
    const int fw = std::max(1, w_ / 4), fh = std::max(1, h_ / 4);
    if (frostA_.w != fw || frostA_.h != fh) {
        frostA_.create(fw, fh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, GL_LINEAR);
        frostB_.create(fw, fh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, GL_LINEAR);
    }
    if (!blurProg_) blurProg_ = ugl::program(ugl::FULLSCREEN_VS, BLUR_FS, "menu frost blur");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frostB_.fbo);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, fw, fh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glDisable(GL_BLEND);
    glUseProgram(blurProg_);
    glActiveTexture(GL_TEXTURE0);
    ugl::set1i(blurProg_, "uT", 0);
    for (int pass = 0; pass < 2; pass++) { // B -> A across, A -> B down, B -> A across again: a wide, soft blur
        const ugl::Target& src = pass == 0 ? frostB_ : frostA_;
        const ugl::Target& dst = pass == 0 ? frostA_ : frostB_;
        dst.bind();
        glBindTexture(GL_TEXTURE_2D, src.tex);
        ugl::set2f(blurProg_, "uDir", pass == 0 ? 1.f / fw : 0.f, pass == 0 ? 0.f : 1.f / fh);
        ugl::drawFullscreen();
    }
    frostA_.bind();
    glBindTexture(GL_TEXTURE_2D, frostB_.tex);
    ugl::set2f(blurProg_, "uDir", 0.7f / fw, 0.7f / fh);
    ugl::drawFullscreen();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w_, h_);
    glEnable(GL_BLEND);
    curTex_ = 0;
}

void Painter::frost(float x, float y, float w, float h, float r, Col tint, float amount) {
    if (fade_ <= 0.f || w <= 0 || h <= 0) return;
    if (!frostA_.tex) { rrect(x, y, w, h, r, withA(tint, 0.85f)); return; } // (no frame grabbed yet)
    const float e = 2.f, hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    const float px[4][2] = {{cx - hx - e, cy - hy - e}, {cx + hx + e, cy - hy - e}, {cx + hx + e, cy + hy + e}, {cx - hx - e, cy + hy + e}};
    const float lc[4][2] = {{-hx - e, -hy - e}, {hx + e, -hy - e}, {hx + e, hy + e}, {-hx - e, hy + e}};
    quad(px, lc, hx, hy, withA(tint, 1.f), r, 1.f, std::clamp(amount, 0.f, 1.f), 4.f);
}

void Painter::glass(float x, float y, float w, float h, float r, float px, float lit, float tint) {
    rrect(x, y + 10 * px, w, h, r, hex(0x000000, 0.28f), 28 * px);
    frost(x, y, w, h, r, hex(0x0a1230), tint);
    line(x + r, y + 1.5f * px, x + w - r, y + 1.5f * px, 1.2f * px, hex(0xffffff, 0.16f + 0.2f * lit), 1.5f * px);
    if (lit > 0.02f) {
        setAdditive(true);
        rrect(x - 4 * px, y - 4 * px, w + 8 * px, h + 8 * px, r + 4 * px, hex(0x8fc4ff, 0.14f * lit), 18 * px);
        setAdditive(false);
    }
    rrectLine(x, y, w, h, r, (1.2f + 0.6f * lit) * px, mixc(hex(0xc8d8ff, 0.2f), hex(0xd8ecff, 0.85f), lit));
}

void Painter::beginLayer(float x, float y, float w, float h, int ss) {
    flush();
    layerX_ = std::floor(x);
    layerY_ = std::floor(y);
    layerW_ = std::ceil(x + w) - layerX_;
    layerH_ = std::ceil(y + h) - layerY_;
    layerS_ = (float)std::max(1, ss);
    const int lw = std::max(1, (int)(layerW_ * layerS_)), lh = std::max(1, (int)(layerH_ * layerS_));
    if (layerTgt_.w != lw || layerTgt_.h != lh) layerTgt_.create(lw, lh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, GL_LINEAR);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &layerPrevFbo_);
    layerTgt_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    layer_ = true;
    setAdditive(additive_, true);
}

void Painter::endLayer() {
    if (!layer_) return;
    flush();
    layer_ = false;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)layerPrevFbo_);
    glViewport(0, 0, w_, h_);
    setAdditive(additive_, true);
    const float f = fade_;
    fade_ = 1.f;
    image(layerTgt_.tex, layerX_, layerY_, layerW_, layerH_, Col{1, 1, 1, 1}, true);
    fade_ = f;
    flush();
}

void Painter::setAdditive(bool on, bool force) {
    if (on == additive_ && !force) return;
    flush();
    additive_ = on;
    // (in a layer, light adds to the colour only: laid down premultiplied, it then adds to what lies below)
    if (layer_ && on) glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
    else glBlendFunc(GL_ONE, on ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
}

void Painter::quad(const float px[4][2], const float lc[4][2], float hx, float hy, Col c, float rad, float soft, float lw, float mode,
                   const Col* bottom) {
    static const int IDX[6] = {0, 1, 2, 0, 2, 3};
    for (int k : IDX) {
        const Col& cc = bottom && k >= 2 ? *bottom : c;
        verts_.push_back({px[k][0], px[k][1], lc[k][0], lc[k][1], hx, hy, cc.r, cc.g, cc.b, cc.a * fade_, rad, soft, lw, mode});
    }
}

void Painter::image(GLuint tex, float x, float y, float w, float h, Col tint, bool flipY) {
    if (!tex || tint.a <= 0.f) return;
    if (curTex_ != tex) { flush(); curTex_ = tex; }
    const float px[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const float t0 = flipY ? 1.f : 0.f, t1 = 1.f - t0;
    const float lc[4][2] = {{0, t0}, {1, t0}, {1, t1}, {0, t1}};
    quad(px, lc, 0, 0, tint, 0, 0, 0, 2.f);
}

void Painter::tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c0, Col c1, Col c2) {
    const float P[3][2] = {{x0, y0}, {x1, y1}, {x2, y2}};
    const Col C[3] = {c0, c1, c2};
    for (int k = 0; k < 3; k++)
        verts_.push_back({P[k][0], P[k][1], 0, 0, 0, 0, C[k].r, C[k].g, C[k].b, C[k].a * fade_, 0, 0, 0, 3.f});
}

void Painter::kaleido(float seedA, float seedB, float mixAB, float cx, float cy, float rot, int segs, Col tint, float time,
                      float level, float saturation, float duo, float launch, float morph, float camX, float camY, int theme) {
    static const char* KFS = R"(#version 330 core
out vec4 o;
uniform vec2 uRes, uC;
uniform float uMix, uRot, uTime, uLevel, uScale, uSat, uDuo, uSeedA, uSeedB, uLaunch, uMorph;
uniform vec2 uCam;   // the camera's place in space (heights): each screen its own
uniform float uTheme; // 0 space, 1 the crystal kaleidoscope
uniform int uSeg;
uniform vec3 uTint;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y);
}
// a procedural pane of glass in the tint's hue, its neighbours and light streaks; each seed its own pattern (where in
// the noise, its scale and its turn)
vec3 proc(vec2 uv, float seed) {
    vec2 o = vec2(h21(vec2(seed, 1.7)), h21(vec2(seed, 9.3))) * 97.0;
    float a = h21(vec2(seed, 4.1)) * 6.2832, sc = 2.4 + 1.6 * h21(vec2(seed, 7.9));
    uv = mat2(cos(a), -sin(a), sin(a), cos(a)) * (uv - 0.5) + 0.5;
    vec2 q = uv * sc + o + vec2(uTime * 0.02, -uTime * 0.015);
    float n = vn(q) * 0.6 + vn(q * 2.1 + 5.0) * 0.3 + vn(q * 4.3 - 2.0) * 0.1;
    float s = vn(uv * 5.0 + o.yx - vec2(uTime * 0.01, 0.0));
    vec3 hue2 = uTint.gbr * 0.75 + 0.1, hue3 = uTint.brg * 0.6 + 0.15;
    vec3 c = mix(uTint * 0.25, uTint, smoothstep(0.25, 0.65, n));
    c = mix(c, hue2, smoothstep(0.62, 0.85, n) * 0.7);
    c = mix(c, hue3, smoothstep(0.7, 0.95, s) * 0.5);
    return c + smoothstep(0.78, 1.0, n) * 0.25;
}
// the glass graded toward the tint: its luminance stretched to its full range, then a duotone between a deep and a
// bright shade of the tint, mixed with its own (more saturated) colours
vec3 grade(vec3 c, vec2 range, vec2 uv) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    // (a stretch of at most x4, so the frame's 8-bit steps don't band; a little glassy noise for its flat areas)
    float ln = clamp((l - range.x) / max(0.25, range.y - range.x), 0.0, 1.0);
    ln = clamp(ln + (vn(uv * 7.0 + uTime * 0.01) - 0.5) * 0.28 + (vn(uv * 23.0) - 0.5) * 0.08, 0.0, 1.0);
    vec3 own = max(mix(vec3(l), c, 1.6), 0.0) * (ln / max(l, 0.02));
    vec3 deep = uTint * vec3(0.10, 0.08, 0.16) * 2.0, bright = mix(uTint, vec3(1.0), 0.25) * 1.15;
    vec3 duo = mix(deep, bright, smoothstep(0.0, 1.0, ln));
    return mix(own, duo, uDuo);
}
vec3 src(float seed, vec2 uv) { return grade(proc(uv, seed), vec2(0.05, 0.7), uv); }
// A triangle grid: the facet a point falls in (`cell`), where in it (`f`), and its distance to the nearest edge (in grid
// units) and which edge that is (0..2).
void facet(vec2 g, out vec2 cell, out vec2 f, out float edge, out float side) {
    vec2 tri = vec2(g.x - g.y * 0.57735, g.y * 1.1547);
    vec2 id = floor(tri);
    f = fract(tri);
    float up = step(1.0, f.x + f.y);
    cell = id * 2.0 + up;
    vec3 e = up > 0.5 ? vec3(1.0 - f.x, 1.0 - f.y, f.x + f.y - 1.0) : vec3(f.x, f.y, 1.0 - f.x - f.y);
    edge = min(e.x, min(e.y, e.z));
    side = edge == e.x ? 0.0 : edge == e.y ? 1.0 : 2.0;
}
// a colour at full strength (its largest channel at 1), more saturated by `sat`
vec3 vivid(vec3 c, float sat) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = max(mix(vec3(l), c, sat), 0.0);
    return c / max(max(c.r, max(c.g, c.b)), 1e-3);
}
// the crystal's colours: a ramp in `hue` (the universe's tint, a little of the world's own colour), from a near black
// of that hue through its full colour to a pale light of it
vec3 gem(float l, vec3 hue) {
    vec3 deep = hue * vec3(0.05, 0.05, 0.06) + vec3(0.006, 0.008, 0.02);
    vec3 mid = hue * 0.46;
    vec3 hi = mix(hue, vec3(1.0), 0.42);
    return l < 0.5 ? mix(deep, mid, smoothstep(0.0, 0.5, l)) : mix(mid, hi, smoothstep(0.5, 1.0, l));
}
void main() {
    vec2 fc = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    vec2 p = (fc - uC) / uRes.y;
    float r = length(p);
    float seg = 6.2831853 / float(uSeg);
    float an = atan(p.y, p.x) + uRot;
    float k = mod(an, seg);
    float m = abs(k - seg * 0.5);               // mirrored inside each segment
    vec2 q = vec2(cos(m), sin(m)) * r;
    // small facets: each refracts the world a little differently and has its own shade
    vec2 cell, f;
    float edge, side;
    facet(q * 7.0, cell, f, edge, side);
    vec2 uv = q * 0.85 + vec2(0.5 + 0.06 * sin(uTime * 0.05), 0.5 + 0.06 * cos(uTime * 0.04));
    uv += (vec2(h21(cell), h21(cell + 3.7)) - 0.5) * 0.035;
    uv = abs(fract(uv * 0.5) * 2.0 - 1.0);
    vec3 c = uMix <= 0.0 ? src(uSeedA, uv) : uMix >= 1.0 ? src(uSeedB, uv) : mix(src(uSeedA, uv), src(uSeedB, uv), uMix);
    // the theme (OPTIONS > MENU THEME): space (0) or the crystal kaleidoscope (1)
    vec3 tv = vivid(uTint, 1.6);
    vec3 space = vec3(0.0); // (added after the brightness cap: the stars)
    if (uTheme > 0.5) {
        // large shards over the small facets, slightly turned, each lit from its own side
        vec2 sc, sf;
        float se, ss;
        vec2 g2 = mat2(0.921, -0.389, 0.389, 0.921) * q * 4.8 + vec2(0.37, 0.11);
        facet(g2, sc, sf, se, ss);
        float hs = h21(sc + 9.7);
        vec2 L = vec2(cos(hs * 6.283), sin(hs * 6.283));
        float lit = clamp(0.5 + dot(sf - 0.4, L) * 1.2, 0.0, 1.0) * (0.55 + 0.6 * h21(sc + 2.2));
        // cut into gem: the world's light and the facets' read through the crystal's ramp, a little of its colour kept
        float l = dot(c, vec3(0.299, 0.587, 0.114));
        float gl = clamp(l * 0.45 + lit * 0.55 + (h21(cell + 9.1) - 0.5) * 0.3, 0.0, 1.0);
        // the ramp's hue: the tint's, drawn toward the world's own colour where it has one, each shard leaning a little to
        // one side of the hue wheel (a neighbouring hue), so a universe of one colour still shows a few
        float cmax = max(c.r, max(c.g, c.b)), wsat = clamp((cmax - min(c.r, min(c.g, c.b))) / max(cmax, 0.05), 0.0, 1.0);
        vec3 hue = mix(tv, vivid(c, 1.3), 0.4 * wsat);
        float lean = h21(sc + 6.6) - 0.5;
        hue = vivid(mix(hue, lean > 0.0 ? hue.brg : hue.gbr, abs(lean) * 0.45), 1.15);
        c = mix(gem(gl, hue), c * (0.5 + 0.5 * uTint) * (0.4 + lit), 0.22);
        c *= 0.92 + 0.16 * smoothstep(0.5, 0.0, edge);
        c *= 0.92 + 0.16 * vec3(h21(cell + 1.3), h21(cell + 2.7), h21(cell + 4.1));
        // some shards cut with a prism's rainbow
        if (h21(sc + 4.4) > 0.7) {
            float x = dot(sf, vec2(-L.y, L.x)) * 2.2 + uTime * 0.025 + hs * 3.0;
            float band = smoothstep(0.55, 0.0, abs(fract(x * 0.5) - 0.5) * 2.0);
            c += (0.5 + 0.5 * cos(6.2831853 * (x + vec3(0.0, 0.33, 0.67)))) * band * 0.14;
        }
    } else {
        // SPACE: no glass, deep space on the whole screen (unmirrored): a great nebula of wisps and filaments, cool on the
        // left in the stage's hue and its neighbours (magenta, blue, a little cyan), warm and golden on the right toward
        // the star in the corner, dark dust lanes through it; a dense field of stars, thicker in the clouds; asteroids
        // drifting by, tumbling. The sky holds still as the stages turn (its shape is the launch's; its colours the stage's).
        // a space palette (deep violet blue, magenta pink, a touch of cyan, gold), the stage's hue blended into it
        vec3 base = mix(vec3(0.45, 0.3, 1.0), tv, 0.5), n2 = mix(vec3(0.95, 0.32, 0.72), tv, 0.4);
        vec3 n3 = vec3(0.3, 0.85, 1.0), warm = vec3(1.0, 0.7, 0.36);
        float asp = uRes.x / uRes.y;
        vec2 sp = fc / uRes.y;
        vec2 drift = vec2(uTime * 0.003, uTime * 0.001);
        vec2 w = sp + drift + uCam;   // (the nebula at depth 1; the far stars move less, the asteroids more)
        float seedOff = uLaunch * 30.0;
        vec2 nw = w * 1.4 + vec2(seedOff, 0.0);
        // domain warping: the gas curls round itself
        // (uMorph: the stage's, eased over seconds: the gas curls and thickens differently in place, never sliding)
        vec2 mo = vec2(cos(uMorph * 1.3), sin(uMorph * 0.9)) * 0.3;
        vec2 q1 = vec2(vn(nw * 1.1 + uTime * 0.006 + mo), vn(nw * 1.1 + 5.2 - uTime * 0.005 - mo.yx));
        vec2 q2 = vec2(vn(nw * 2.3 + q1 * 2.0 + 1.7), vn(nw * 2.3 + q1 * 2.0 + 9.2));
        vec2 ww = nw + q2 * 1.4;
        float fb = 0.0, amp = 0.5, frq = 1.0;
        for (int i = 0; i < 5; i++) { fb += amp * vn(ww * frq); frq *= 2.07; amp *= 0.5; }
        // filaments: the ridges of a finer warped noise
        float rg = 0.0; amp = 0.55; frq = 3.0;
        for (int i = 0; i < 4; i++) { float n = vn(ww * frq + 3.3); rg += amp * pow(1.0 - abs(2.0 * n - 1.0), 3.0); frq *= 2.1; amp *= 0.5; }
        // where the clouds are: a broad sweep across from the lower left to the upper right, thicker at both ends
        float band = exp(-pow((sp.y - (0.62 - 0.32 * sp.x / asp + 0.08 * sin(sp.x * 2.3 + seedOff))) / 0.33, 2.0));
        float leftC = exp(-dot((sp - vec2(0.22 * asp, 0.48)) / vec2(0.55, 0.42), (sp - vec2(0.22 * asp, 0.48)) / vec2(0.55, 0.42)));
        float rightC = exp(-dot((sp - vec2(0.8 * asp, 0.3)) / vec2(0.5, 0.45), (sp - vec2(0.8 * asp, 0.3)) / vec2(0.5, 0.45)));
        float dens = clamp((band * 0.7 + leftC * 0.8 + rightC * 0.7) * (1.12 + 0.12 * (0.5 + 0.5 * sin(uMorph * 2.3))), 0.0, 1.4);
        float gas = smoothstep(0.26, 0.82, fb) * dens;
        float wisp = rg * smoothstep(0.25, 0.7, fb) * dens;
        float dust = smoothstep(0.42, 0.72, vn(ww * 2.6 - 4.0 + q1));
        // its colours: cool on the left (the tint, a neighbour, cyan where it is thinnest), warm on the right
        float warmth = smoothstep(0.35 * asp, 0.85 * asp, sp.x + 0.25 * (fb - 0.5));
        vec3 cool = mix(base, n2, smoothstep(0.38, 0.62, vn(nw * 0.7 + 2.0 + q1 * 0.8)));
        cool = mix(cool, n3, smoothstep(0.6, 0.9, q1.y) * 0.5);
        vec3 gc = vivid(mix(cool, warm, warmth), 1.35);   // (kept saturated: never a grey)
        vec3 col = vec3(0.006, 0.006, 0.022) + base * 0.04 * dens;
        col += gc * gas * 0.45;
        col += mix(gc, vec3(1.0), 0.15) * wisp * 0.4;
        col += mix(gc, vec3(1.0), 0.12) * pow(gas, 3.0) * 0.3;   // (the clouds' glowing cores)
        col *= 0.45 + 0.55 * mix(1.0, dust, smoothstep(0.1, 0.5, dens));
        for (int i = 0; i < 4; i++) {
            float fi = float(i);
            float ssc = 45.0 + fi * 60.0;
            vec2 sg = (sp + drift * (1.0 + fi * 0.5) + uCam * (0.35 + 0.2 * fi)) * ssc;
            vec2 si = floor(sg), sf = fract(sg) - 0.5;
            float sh = h21(si + fi * 31.0);
            if (sh > 0.955 - fi * 0.008 - 0.03 * dens) {
                vec2 off = (vec2(h21(si + 7.0), h21(si + 3.0)) - 0.5) * 0.6;
                float dd = length(sf - off) * (uRes.y / ssc) * (1080.0 / uRes.y);
                float twk = 0.65 + 0.35 * sin(uTime * (1.0 + 3.0 * h21(si)) + sh * 50.0);
                vec3 scol = mix(vec3(1.0), mix(vec3(1.0, 0.82, 0.62), vec3(0.72, 0.82, 1.0), h21(si + 1.0)), 0.55);
                float big = step(0.985, h21(si + 2.0)) * (1.0 - fi * 0.3);
                float sz = (0.9 + 1.4 * (1.0 - fi * 0.3)) * (1.0 + big * 1.5);
                space += scol * (exp(-dd * dd / sz) + big * 0.35 * (exp(-abs((sf - off).x) * ssc * 0.8) + exp(-abs((sf - off).y) * ssc * 0.8)) * exp(-dd * 0.25))
                         * twk * (1.0 - fi * 0.22);
            }
        }
        for (int i = 0; i < 6; i++) {   // asteroids, lit from the top right (the star)
            float fi = float(i);
            float speed = 0.008 + 0.01 * h21(vec2(fi, 2.0));
            vec2 ac = vec2(mod(h21(vec2(fi, 5.0)) * asp * 1.5 + uTime * speed, asp * 1.5) - asp * 0.25, 0.1 + 0.8 * h21(vec2(fi, 9.0)));
            float ar = 0.009 + 0.026 * h21(vec2(fi, 3.0));
            vec2 da = sp + uCam * 1.6 - ac;
            float ro = uTime * (0.08 + 0.2 * h21(vec2(fi, 6.0))) + fi;
            vec2 dr = mat2(cos(ro), -sin(ro), sin(ro), cos(ro)) * da;
            float lumpy = ar * (1.0 + 0.45 * (vn(vec2(atan(dr.y, dr.x) * 1.4 + fi * 7.0, fi)) - 0.5));
            float inA = smoothstep(lumpy + 1.5 / uRes.y, lumpy - 1.5 / uRes.y, length(dr));
            if (inA > 0.0) {
                vec2 nr = da / ar;
                float lt = clamp(0.4 + nr.x * 0.7 - nr.y * 0.4, 0.0, 1.0);
                float crat = vn(dr / ar * 3.0 + fi * 11.0);
                vec3 rock = vec3(0.36, 0.33, 0.31) * (0.06 + 0.94 * lt) * (0.7 + 0.55 * crat);
                col = mix(col, rock, inA);
                space *= 1.0 - inA;
            }
        }
        c = col;
    }
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, uSat);  // a locked stage: its world greyed
    c *= 0.9 + 0.2 * uLevel;
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c *= min(1.0, 0.45 / max(lum, 1e-3));                  // never bright behind the text (a white world stays a glow)
    c += space * (0.4 + 0.6 * uSat);
    // (no lead: the shards meet edge to edge, told apart by their own light and hue alone)
    c *= 1.0 - 0.5 * smoothstep(0.45, 1.2, r);
    // light on the glass, in the frame's own space (it doesn't turn): a soft flare in the top right corner, and glints
    // twinkling here and there
    vec2 fp = (fc - vec2(uRes.x - 0.04 * uRes.y, 0.04 * uRes.y)) / uRes.y;
    float fd = length(fp), fa = atan(fp.y, fp.x);
    float rays = pow(abs(cos(fa * 5.0 + 0.4)), 10.0) + 0.6 * pow(abs(cos(fa * 3.0 - uTime * 0.02)), 14.0);
    c += (uTheme > 0.5 ? mix(tv, vec3(1.0), 0.65) : vec3(1.0, 0.86, 0.66)) * (0.45 * exp(-fd * 8.0) + 0.14 * exp(-fd * 2.4) + 0.07 * rays * exp(-fd * 3.5)) * (0.5 + 0.5 * uSat);
    vec2 gp = fc / uRes.y * 24.0, gi = floor(gp);
    float gh = h21(gi + 17.0);
    if (gh > 0.86) {
        vec2 d = (fract(gp) - 0.5 - (vec2(h21(gi + 3.1), h21(gi + 5.9)) - 0.5) * 0.3) * (uRes.y / 24.0) * (1080.0 / uRes.y);
        float R = 2.5 + 4.0 * h21(gi + 2.0);
        float tw = pow(0.5 + 0.5 * sin(uTime * (0.8 + 1.6 * h21(gi + 8.0)) + gh * 40.0), 6.0);
        float star = exp(-dot(d, d) / (R * R * 0.12)) +
                     0.7 * (exp(-abs(d.y) / 0.6) * max(0.0, 1.0 - abs(d.x) / (R * 2.2)) + exp(-abs(d.x) / 0.6) * max(0.0, 1.0 - abs(d.y) / (R * 2.2)));
        c += vec3(0.85, 0.92, 1.0) * star * tw * (0.6 + 0.4 * uSat);
    }
    o = vec4(c, 1.0);
}
)";
    // the low-resolution glass, spread over the frame: bilinear with a small 4-tap blur
    static const char* UFS = R"(#version 330 core
in vec2 vUv;
out vec4 o;
uniform sampler2D uT;
uniform vec2 uTexel;
void main() {
    vec2 d = uTexel * 0.5;
    vec3 c = texture(uT, vUv + vec2(-d.x, -d.y)).rgb + texture(uT, vUv + vec2(d.x, -d.y)).rgb +
             texture(uT, vUv + vec2(-d.x, d.y)).rgb + texture(uT, vUv + vec2(d.x, d.y)).rgb;
    o = vec4(c * 0.25, 1.0);
}
)";
    flush();
    if (!kalProg_) {
        kalProg_ = ugl::program(ugl::FULLSCREEN_VS, KFS, "menu kaleidoscope");
        upProg_ = ugl::program(ugl::FULLSCREEN_VS, UFS, "menu kaleidoscope upscale");
    }
    // full resolution; REFALL_MENU_KALEIDO_DIV=N draws it at 1/N and spreads it over the frame (testing, weak GPUs)
    static const float div = std::getenv("REFALL_MENU_KALEIDO_DIV") ? std::max(1.f, (float)std::atof(std::getenv("REFALL_MENU_KALEIDO_DIV"))) : 1.f;
    const bool direct = div <= 1.f;
    const int lw = direct ? w_ : std::max(1, (int)(w_ / div)), lh = direct ? h_ : std::max(1, (int)(h_ / div));
    if (!direct && (kalTgt_.w != lw || kalTgt_.h != lh)) kalTgt_.create(lw, lh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, GL_LINEAR);
    const float sx = (float)lw / w_, sy = (float)lh / h_;
    if (!direct) kalTgt_.bind();
    glDisable(GL_BLEND);
    glUseProgram(kalProg_);
    ugl::set1f(kalProg_, "uSeedA", seedA);
    ugl::set1f(kalProg_, "uSeedB", seedB);
    ugl::set1f(kalProg_, "uLaunch", launch);
    ugl::set1f(kalProg_, "uMorph", morph);
    ugl::set2f(kalProg_, "uCam", camX, camY);
    ugl::set1f(kalProg_, "uTheme", (float)theme);
    ugl::set2f(kalProg_, "uRes", (float)lw, (float)lh);
    ugl::set2f(kalProg_, "uC", cx * sx, cy * sy);
    ugl::set1f(kalProg_, "uScale", (float)h_ / lh);
    ugl::set1f(kalProg_, "uMix", mixAB);
    ugl::set1f(kalProg_, "uRot", rot);
    ugl::set1f(kalProg_, "uTime", time);
    ugl::set1f(kalProg_, "uLevel", level);
    ugl::set1f(kalProg_, "uSat", saturation);
    ugl::set1f(kalProg_, "uDuo", duo);
    ugl::set1i(kalProg_, "uSeg", segs);
    glUniform3f(glGetUniformLocation(kalProg_, "uTint"), tint.r, tint.g, tint.b);
    ugl::drawFullscreen();
    if (!direct) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w_, h_);
        glUseProgram(upProg_);
        glBindTexture(GL_TEXTURE_2D, kalTgt_.tex);
        ugl::set1i(upProg_, "uT", 0);
        ugl::set2f(upProg_, "uTexel", 1.f / lw, 1.f / lh);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    curTex_ = 0;
}

void Painter::arc(float cx, float cy, float r, float a0, float a1, float width, Col c, float soft) {
    if (c.a <= 0.f || r <= 0.f) return;
    if (a1 < a0) std::swap(a0, a1);
    const float span = std::min(a1 - a0, TAU), mid = a0 + span * 0.5f, e = width * 0.5f + soft + 1.f;
    // the quad: the arc's bounding box (its ends, and the circle's extremes it passes)
    float x0 = std::min(std::cos(a0), std::cos(a0 + span)), x1 = std::max(std::cos(a0), std::cos(a0 + span));
    float y0 = std::min(std::sin(a0), std::sin(a0 + span)), y1 = std::max(std::sin(a0), std::sin(a0 + span));
    for (int q = (int)std::ceil(a0 / (PI * 0.5f)); q * PI * 0.5f <= a0 + span; q++) {
        const int k = ((q % 4) + 4) % 4;
        if (k == 0) x1 = 1.f; else if (k == 1) y1 = 1.f; else if (k == 2) x0 = -1.f; else y0 = -1.f;
    }
    const float l[4] = {x0 * r - e, y0 * r - e, x1 * r + e, y1 * r + e};
    const float px[4][2] = {{cx + l[0], cy + l[1]}, {cx + l[2], cy + l[1]}, {cx + l[2], cy + l[3]}, {cx + l[0], cy + l[3]}};
    const float lc[4][2] = {{l[0], l[1]}, {l[2], l[1]}, {l[2], l[3]}, {l[0], l[3]}};
    quad(px, lc, mid, span * 0.5f, c, r, soft, width, 5.f);
}

void Painter::rrect(float x, float y, float w, float h, float r, Col c, float soft) {
    if (c.a <= 0.f || w <= 0 || h <= 0) return;
    const float e = soft + 1.f, hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    const float px[4][2] = {{cx - hx - e, cy - hy - e}, {cx + hx + e, cy - hy - e}, {cx + hx + e, cy + hy + e}, {cx - hx - e, cy + hy + e}};
    const float lc[4][2] = {{-hx - e, -hy - e}, {hx + e, -hy - e}, {hx + e, hy + e}, {-hx - e, hy + e}};
    quad(px, lc, hx, hy, c, r, soft, 0.f, 0.f);
}

void Painter::rrectLine(float x, float y, float w, float h, float r, float width, Col c, float soft) {
    if (c.a <= 0.f || w <= 0 || h <= 0) return;
    const float e = soft + 1.f, hx = w * 0.5f, hy = h * 0.5f, cx = x + hx, cy = y + hy;
    const float px[4][2] = {{cx - hx - e, cy - hy - e}, {cx + hx + e, cy - hy - e}, {cx + hx + e, cy + hy + e}, {cx - hx - e, cy + hy + e}};
    const float lc[4][2] = {{-hx - e, -hy - e}, {hx + e, -hy - e}, {hx + e, hy + e}, {-hx - e, hy + e}};
    quad(px, lc, hx, hy, c, r, soft, width, 0.f);
}

void Painter::line(float x0, float y0, float x1, float y1, float width, Col c, float soft) {
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3f || c.a <= 0.f) return;
    const float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    const float hx = len * 0.5f + width * 0.5f, hy = width * 0.5f, e = soft + 1.f;
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
    float px[4][2], lc[4][2];
    for (int k = 0; k < 4; k++) {
        const float lx = sx[k] * (hx + e), ly = sy[k] * (hy + e);
        lc[k][0] = lx;
        lc[k][1] = ly;
        px[k][0] = cx + ux * lx + nx * ly;
        px[k][1] = cy + uy * lx + ny * ly;
    }
    quad(px, lc, hx, hy, c, hy, soft, 0.f, 0.f);
}

void Painter::check(float cx, float cy, float s, float width, Col c) {
    line(cx - s * 0.45f, cy, cx - s * 0.12f, cy + s * 0.32f, width, c);
    line(cx - s * 0.12f, cy + s * 0.32f, cx + s * 0.5f, cy - s * 0.38f, width, c);
}

void Painter::lock(float cx, float cy, float s, Col c) {
    rrectLine(cx - s * 0.3f, cy - s * 0.62f, s * 0.6f, s * 0.75f, s * 0.3f, s * 0.13f, c);
    rrect(cx - s * 0.45f, cy - s * 0.15f, s * 0.9f, s * 0.7f, s * 0.12f, c);
}

float Painter::width(Font id, const std::string& s, float size, float tracking) {
    FontData& f = font(id);
    if (!f.ok) return 0.f;
    const float k = size / f.px;
    float w = 0;
    const auto* pc = (const stbtt_packedchar*)f.packed.data();
    for (size_t i = 0; i < s.size(); i++) {
        int ch = (unsigned char)s[i];
        if (ch < 32 || ch > 126) ch = '?';
        w += pc[ch - 32].xadvance * k + (i + 1 < s.size() ? tracking * size : 0.f);
    }
    return w;
}

float Painter::text(Font id, const std::string& s, float x, float y, float size, Col c, int align, float tracking) {
    FontData& f = font(id);
    if (!f.ok || s.empty() || c.a <= 0.f) return 0.f;
    const float w = width(id, s, size, tracking);
    if (align == 0) x -= w * 0.5f;
    else if (align > 0) x -= w;
    if (curTex_ != f.tex) { flush(); curTex_ = f.tex; }
    const float k = size / f.px;
    const auto* pc = (const stbtt_packedchar*)f.packed.data();
    float pxx = 0, pyy = 0;
    for (size_t i = 0; i < s.size(); i++) {
        int ch = (unsigned char)s[i];
        if (ch < 32 || ch > 126) ch = '?';
        stbtt_aligned_quad q;
        float ax = 0, ay = 0;
        stbtt_GetPackedQuad(pc, 2048, 2048, ch - 32, &ax, &ay, &q, 0);
        const float x0 = x + pxx + q.x0 * k, x1 = x + pxx + q.x1 * k, y0 = y + pyy + q.y0 * k, y1 = y + pyy + q.y1 * k;
        const float px[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        const float lc[4][2] = {{q.s0, q.t0}, {q.s1, q.t0}, {q.s1, q.t1}, {q.s0, q.t1}};
        if (ch != ' ') quad(px, lc, 0, 0, c, 0, 0, 0, 1.f);
        pxx += ax * k + tracking * size;
    }
    return w;
}


void Painter::inkExtent(Font id, const std::string& s, float size, float tracking, float& left, float& right) {
    left = right = 0.f;
    FontData& f = font(id);
    if (!f.ok || s.empty()) return;
    const float k = size / f.px;
    const auto* pc = (const stbtt_packedchar*)f.packed.data();
    float pen = 0;
    bool first = true;
    for (size_t i = 0; i < s.size(); i++) {
        int ch = (unsigned char)s[i];
        if (ch < 32 || ch > 126) ch = '?';
        const stbtt_packedchar& g = pc[ch - 32];
        if (ch != ' ') {
            if (first) { left = pen + g.xoff * k; first = false; }
            right = pen + g.xoff2 * k;
        }
        pen += g.xadvance * k + tracking * size;
    }
}

float Painter::capHeight(Font id, float size) {
    FontData& f = font(id);
    if (!f.ok) return size * 0.7f;
    const auto* pc = (const stbtt_packedchar*)f.packed.data();
    return -pc['H' - 32].yoff * size / f.px;
}

#include "menupaint.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
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
out vec2 vLocal;
out vec2 vHalf;
out vec4 vCol;
out vec4 vPrm;
void main() {
    vLocal = aLocal; vHalf = aHalf; vCol = aCol; vPrm = aPrm;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
const char* FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vHalf;
in vec4 vCol;
in vec4 vPrm;
out vec4 o;
uniform sampler2D uTex;
void main() {
    float a;
    if (vPrm.w > 2.5) { // flat
        a = vCol.a;
        o = vec4(vCol.rgb * a, a);
        return;
    }
    if (vPrm.w > 1.5) { // an image (premultiplied), tinted
        vec4 t = texture(uTex, vLocal);
        o = vec4(t.rgb * vCol.rgb, t.a) * vCol.a;
        return;
    }
    if (vPrm.w > 0.5) {
        a = texture(uTex, vLocal).r;
    } else {
        float r = min(vPrm.x, min(vHalf.x, vHalf.y));
        vec2 q = abs(vLocal) - vHalf + r;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
        if (vPrm.z > 0.0) d = abs(d + vPrm.z * 0.5) - vPrm.z * 0.5;
        float s = max(vPrm.y, 0.5);
        a = 1.0 - smoothstep(-s * 0.5, s * 0.5, d);
        if (s > 2.0) a = a * a * (3.0 - 2.0 * a); // glows fade smoothly
    }
    a *= vCol.a;
    o = vec4(vCol.rgb * a, a);
}
)";


} // namespace

Painter::~Painter() {
    if (prog_) glDeleteProgram(prog_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (upProg_) glDeleteProgram(upProg_);
    kalTgt_.destroy();
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
    ugl::set2f(prog_, "uRes", (float)w_, (float)h_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, curTex_);
    ugl::set1i(prog_, "uTex", 0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(V), verts_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts_.size());
    verts_.clear();
}

void Painter::setAdditive(bool on) {
    if (on == additive_) return;
    flush();
    additive_ = on;
    glBlendFunc(GL_ONE, on ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
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

void Painter::kaleido(GLuint a, GLuint b, float mixAB, float cx, float cy, float rot, int segs, Col tint, float time, float level,
                      float saturation, float duo, float loA, float hiA, float loB, float hiB) {
    static const char* KFS = R"(#version 330 core
out vec4 o;
uniform sampler2D uA, uB;
uniform vec2 uRes, uC;
uniform float uMix, uRot, uTime, uLevel, uScale, uSat, uDuo;
uniform vec2 uRangeA, uRangeB; // each frame's luminance range (5th and 95th percentiles), stretched to 0..1
uniform int uSeg, uProcA, uProcB;
uniform vec3 uTint;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y);
}
// no snapshot of the universe: a procedural pane of glass in the stage's hue, its neighbours and light streaks
vec3 proc(vec2 uv) {
    vec2 q = uv * 3.0 + vec2(uTime * 0.02, -uTime * 0.015);
    float n = vn(q) * 0.6 + vn(q * 2.1 + 5.0) * 0.3 + vn(q * 4.3 - 2.0) * 0.1;
    float s = vn(uv * 5.0 - vec2(uTime * 0.01, 0.0));
    vec3 hue2 = uTint.gbr * 0.75 + 0.1, hue3 = uTint.brg * 0.6 + 0.15;
    vec3 c = mix(uTint * 0.25, uTint, smoothstep(0.25, 0.65, n));
    c = mix(c, hue2, smoothstep(0.62, 0.85, n) * 0.7);
    c = mix(c, hue3, smoothstep(0.7, 0.95, s) * 0.5);
    return c + smoothstep(0.78, 1.0, n) * 0.25;
}
// a frame graded toward the tint: its luminance stretched to its full range, then a duotone between a deep and a bright
// shade of the tint, mixed with its own (more saturated) colours
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
vec3 src(sampler2D t, int isProc, vec2 uv, vec2 range) { return grade(isProc == 1 ? proc(uv) : texture(t, uv).rgb, isProc == 1 ? vec2(0.05, 0.7) : range, uv); }
void main() {
    vec2 fc = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    vec2 p = (fc - uC) / uRes.y;
    float r = length(p);
    float seg = 6.2831853 / float(uSeg);
    float an = atan(p.y, p.x) + uRot;
    float k = mod(an, seg);
    float m = abs(k - seg * 0.5);               // mirrored inside each segment
    vec2 q = vec2(cos(m), sin(m)) * r;
    // glass facets: a triangle grid in the folded space, each facet its own shade (no lead: only the coloured cells)
    vec2 g = q * 7.0;
    vec2 tri = vec2(g.x - g.y * 0.57735, g.y * 1.1547);
    vec2 id = floor(tri);
    vec2 f = fract(tri);
    float up = step(1.0, f.x + f.y);
    vec2 cell = id * 2.0 + up;
    float edge = min(min(f.x, f.y), abs(1.0 - f.x - f.y));
    vec2 uv = q * 0.85 + vec2(0.5 + 0.06 * sin(uTime * 0.05), 0.5 + 0.06 * cos(uTime * 0.04));
    uv += (vec2(h21(cell), h21(cell + 3.7)) - 0.5) * 0.035;   // each facet refracts a little differently
    uv = abs(fract(uv * 0.5) * 2.0 - 1.0);
    vec3 c = uMix <= 0.0 ? src(uA, uProcA, uv, uRangeA) : uMix >= 1.0 ? src(uB, uProcB, uv, uRangeB)
                          : mix(src(uA, uProcA, uv, uRangeA), src(uB, uProcB, uv, uRangeB), uMix);
    c *= 0.82 + 0.3 * h21(cell + 9.1);
    c = mix(c, c * uTint * 1.5, 0.22);
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = max(mix(vec3(l), c, 1.35), 0.0);                    // art glass: richer colour
    c = c * 1.25 + 0.04 + uTint * (0.10 + 0.14 * h21(cell + 5.3)); // lit from behind, tinted glass even over a dark world
    c *= 0.85 + 0.3 * smoothstep(0.85, 0.0, edge);           // brighter toward each facet's middle
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, uSat);  // a locked stage: its world greyed
    c *= 0.9 + 0.2 * uLevel;
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    c *= min(1.0, 0.42 / max(lum, 1e-3));                  // never bright behind the text (a white world stays a glow)
    c *= 1.0 - 0.5 * smoothstep(0.45, 1.2, r);
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
    // full resolution; ZEN_MENU_KALEIDO_DIV=N draws it at 1/N and spreads it over the frame (testing, weak GPUs)
    static const float div = std::getenv("ZEN_MENU_KALEIDO_DIV") ? std::max(1.f, (float)std::atof(std::getenv("ZEN_MENU_KALEIDO_DIV"))) : 1.f;
    const bool direct = div <= 1.f;
    const int lw = direct ? w_ : std::max(1, (int)(w_ / div)), lh = direct ? h_ : std::max(1, (int)(h_ / div));
    if (!direct && (kalTgt_.w != lw || kalTgt_.h != lh)) kalTgt_.create(lw, lh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, GL_LINEAR);
    const float sx = (float)lw / w_, sy = (float)lh / h_;
    if (!direct) kalTgt_.bind();
    glDisable(GL_BLEND);
    glUseProgram(kalProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a ? a : b);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, b ? b : a);
    glActiveTexture(GL_TEXTURE0);
    ugl::set1i(kalProg_, "uA", 0);
    ugl::set1i(kalProg_, "uB", 1);
    ugl::set1i(kalProg_, "uProcA", a ? 0 : 1);
    ugl::set1i(kalProg_, "uProcB", b ? 0 : 1);
    ugl::set2f(kalProg_, "uRes", (float)lw, (float)lh);
    ugl::set2f(kalProg_, "uC", cx * sx, cy * sy);
    ugl::set1f(kalProg_, "uScale", (float)h_ / lh);
    ugl::set1f(kalProg_, "uMix", mixAB);
    ugl::set1f(kalProg_, "uRot", rot);
    ugl::set1f(kalProg_, "uTime", time);
    ugl::set1f(kalProg_, "uLevel", level);
    ugl::set1f(kalProg_, "uSat", saturation);
    ugl::set1f(kalProg_, "uDuo", duo);
    ugl::set2f(kalProg_, "uRangeA", loA, hiA);
    ugl::set2f(kalProg_, "uRangeB", loB, hiB);
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

void Painter::arc(float cx, float cy, float rx, float ry, float a0, float a1, float width, Col c, float soft) {
    const int n = std::max(4, (int)(std::fabs(a1 - a0) * std::max(rx, ry) / 6.f));
    for (int i = 0; i < n; i++) {
        const float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
        line(cx + rx * std::cos(t0), cy + ry * std::sin(t0), cx + rx * std::cos(t1), cy + ry * std::sin(t1), width, c, soft);
    }
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

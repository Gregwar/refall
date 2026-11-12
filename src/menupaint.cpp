#include "menupaint.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
const unsigned char TTF_INTER_BOLD[] = {
#include "universes/host/Inter-Bold.ttf.inc"
};
const unsigned char TTF_INTER[] = {
#include "universes/host/Inter-Regular.ttf.inc"
};
const unsigned char TTF_KALAM[] = {
#include "universes/host/Kalam-Regular.ttf.inc"
};
const unsigned char TTF_YRSA[] = {
#include "universes/host/Yrsa-Medium.ttf.inc"
};
const unsigned char TTF_MANROPE_LIGHT[] = {
#include "universes/host/Manrope-Light.ttf.inc"
};
const unsigned char TTF_MANROPE[] = {
#include "universes/host/Manrope-Medium.ttf.inc"
};
const unsigned char* TTFS[Painter::FONT_COUNT] = {TTF_JOSEFIN_LIGHT, TTF_JOSEFIN, TTF_INTER_BOLD, TTF_INTER,
                                                  TTF_KALAM,         TTF_YRSA,    TTF_MANROPE_LIGHT, TTF_MANROPE};

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

const char* BG_FS = R"(#version 330 core
out vec4 o;
uniform vec2 uRes;
uniform float uTime, uLevel;
uniform int uKind;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}
void main() {
    vec2 fc = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    vec2 uv = fc / uRes;
    vec2 p = (fc - 0.5 * uRes) / uRes.y;
    float t = uTime;
    vec3 c;
    if (uKind == 1) { // cosmic: a slow nebula and twinkling stars
        vec2 q = p * 1.5 + vec2(t * 0.006, -t * 0.003);
        float n1 = fbm(q + fbm(q * 1.3 + t * 0.01));
        float n2 = fbm(q * 2.2 - vec2(4.0, 1.0) - t * 0.008);
        c = vec3(0.012, 0.014, 0.045);
        c += vec3(0.30, 0.10, 0.48) * pow(n1, 3.0) * 1.3;
        c += vec3(0.04, 0.30, 0.42) * pow(n2, 3.5) * 1.2;
        c *= 0.85 + 0.35 * uLevel;
        for (int l = 0; l < 3; l++) {
            float sc = 90.0 + 70.0 * float(l);
            vec2 sp = p * sc + vec2(t * (0.15 + 0.12 * float(l)), 0.0);
            vec2 id = floor(sp), f = fract(sp) - 0.5;
            float hh = h21(id + float(l) * 17.0);
            if (hh > 0.93) {
                vec2 off = vec2(h21(id + 3.1), h21(id + 7.7)) - 0.5;
                float d = length(f - off * 0.6);
                float tw = 0.6 + 0.4 * sin(t * (0.6 + hh * 1.5) + hh * 40.0);
                c += vec3(0.85, 0.9, 1.0) * smoothstep(0.09, 0.0, d) * tw * (0.5 + 0.5 * float(2 - l) / 2.0);
            }
        }
        c *= 1.0 - 0.45 * dot(p, p);
    } else if (uKind == 2) { // swiss: warm off-white paper
        c = vec3(0.933, 0.922, 0.894) + (h21(fc) - 0.5) * 0.012;
    } else if (uKind == 3) { // paper with watercolour washes
        vec3 paper = vec3(0.957, 0.929, 0.871);
        float grain = fbm(fc * 0.35) * 0.06 + (h21(fc) - 0.5) * 0.02;
        c = paper - grain * vec3(0.35, 0.4, 0.5);
        vec2 q = p * 1.4 + vec2(t * 0.004, 0.0);
        float w1 = fbm(q * 1.2 + 2.0), w2 = fbm(q * 1.5 - 5.0), w3 = fbm(q * 2.0 + 11.0);
        c = mix(c, vec3(0.62, 0.78, 0.86), smoothstep(0.52, 0.72, w1) * 0.35);
        c = mix(c, vec3(0.70, 0.82, 0.60), smoothstep(0.55, 0.75, w2) * 0.30);
        c = mix(c, vec3(0.93, 0.78, 0.55), smoothstep(0.58, 0.78, w3) * 0.25);
        vec2 v = uv - 0.5;
        c *= 1.0 - 0.35 * pow(length(v * vec2(1.1, 1.3)), 2.5);
    } else { // glass: slow blurred colour blobs on deep navy
        c = vec3(0.035, 0.035, 0.085);
        vec2 b1 = vec2(0.55 * sin(t * 0.05), 0.30 * cos(t * 0.04));
        vec2 b2 = vec2(0.60 * cos(t * 0.035 + 1.0), 0.32 * sin(t * 0.045 + 2.0));
        vec2 b3 = vec2(0.50 * sin(t * 0.03 + 4.0), 0.28 * sin(t * 0.06 + 1.0));
        vec2 b4 = vec2(0.65 * cos(t * 0.025 + 3.0), 0.25 * cos(t * 0.05 + 5.0));
        c += vec3(0.95, 0.15, 0.62) * exp(-dot(p - b1, p - b1) * 5.0) * 0.55;
        c += vec3(0.10, 0.70, 1.00) * exp(-dot(p - b2, p - b2) * 4.0) * 0.45;
        c += vec3(0.45, 0.20, 1.00) * exp(-dot(p - b3, p - b3) * 3.0) * 0.50;
        c += vec3(1.00, 0.45, 0.20) * exp(-dot(p - b4, p - b4) * 7.0) * 0.25;
        c *= 0.85 + 0.3 * uLevel;
        c += (h21(fc + t) - 0.5) / 255.0 * 2.0;
    }
    o = vec4(c, 1.0);
}
)";

} // namespace

Painter::~Painter() {
    if (prog_) glDeleteProgram(prog_);
    if (bgProg_) glDeleteProgram(bgProg_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    for (auto& f : fonts_)
        if (f.tex) glDeleteTextures(1, &f.tex);
}

Painter::FontData& Painter::font(Font id) {
    FontData& f = fonts_[id];
    if (f.tried) return f;
    f.tried = true;
    const int W = 2048, H = 1024;
    std::vector<unsigned char> bmp((size_t)W * H);
    f.packed.resize(sizeof(stbtt_packedchar) * 95);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, bmp.data(), W, H, 0, 2, nullptr)) return f;
    stbtt_PackSetOversampling(&pc, 1, 1);
    f.ok = stbtt_PackFontRange(&pc, TTFS[id], 0, f.px, 32, 95, (stbtt_packedchar*)f.packed.data()) != 0;
    stbtt_PackEnd(&pc);
    if (!f.ok) { std::fprintf(stderr, "[menu] could not pack font %d\n", (int)id); return f; }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, bmp.data());
    glGenerateMipmap(GL_TEXTURE_2D);
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
        bgProg_ = ugl::program(ugl::FULLSCREEN_VS, BG_FS, "menu background");
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

void Painter::background(int kind, float time, float level) {
    flush();
    glDisable(GL_BLEND);
    glUseProgram(bgProg_);
    ugl::set2f(bgProg_, "uRes", (float)w_, (float)h_);
    ugl::set1f(bgProg_, "uTime", time);
    ugl::set1f(bgProg_, "uLevel", level);
    ugl::set1i(bgProg_, "uKind", kind);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
}

void Painter::quad(const float px[4][2], const float lc[4][2], float hx, float hy, Col c, float rad, float soft, float lw, float mode) {
    static const int IDX[6] = {0, 1, 2, 0, 2, 3};
    for (int k : IDX)
        verts_.push_back({px[k][0], px[k][1], lc[k][0], lc[k][1], hx, hy, c.r, c.g, c.b, c.a, rad, soft, lw, mode});
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
        stbtt_GetPackedQuad(pc, 2048, 1024, ch - 32, &ax, &ay, &q, 0);
        const float x0 = x + pxx + q.x0 * k, x1 = x + pxx + q.x1 * k, y0 = y + pyy + q.y0 * k, y1 = y + pyy + q.y1 * k;
        const float px[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        const float lc[4][2] = {{q.s0, q.t0}, {q.s1, q.t0}, {q.s1, q.t1}, {q.s0, q.t1}};
        if (ch != ' ') quad(px, lc, 0, 0, c, 0, 0, 0, 1.f);
        pxx += ax * k + tracking * size;
    }
    return w;
}

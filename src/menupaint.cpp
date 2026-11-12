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
const unsigned char TTF_MARCELLUS[] = {
#include "universes/host/Marcellus-Regular.ttf.inc"
};
const unsigned char TTF_CORMORANT_ITALIC[] = {
#include "universes/host/CormorantGaramond-Italic.ttf.inc"
};
const unsigned char TTF_JURA_LIGHT[] = {
#include "universes/host/Jura-Light.ttf.inc"
};
const unsigned char TTF_JURA[] = {
#include "universes/host/Jura-Medium.ttf.inc"
};
const unsigned char TTF_COMFORTAA[] = {
#include "universes/host/Comfortaa-Bold.ttf.inc"
};
const unsigned char TTF_QUICKSAND[] = {
#include "universes/host/Quicksand-Medium.ttf.inc"
};
const unsigned char* TTFS[Painter::FONT_COUNT] = {TTF_JOSEFIN_LIGHT, TTF_JOSEFIN, TTF_INTER_BOLD, TTF_INTER,
                                                  TTF_KALAM,         TTF_YRSA,    TTF_MANROPE_LIGHT, TTF_MANROPE,
                                                  TTF_MARCELLUS, TTF_CORMORANT_ITALIC, TTF_JURA_LIGHT, TTF_JURA,
                                                  TTF_COMFORTAA, TTF_QUICKSAND};

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
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), u.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), u.x), u.y);
}
float fbm3(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.07 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}
void main() {
    float a;
    if (vPrm.w > 3.5) { // an image
        vec3 c = texture(uTex, vLocal).rgb;
        vec2 lp = (vLocal - 0.5) * 2.0 * vHalf;
        float r = min(vPrm.x, min(vHalf.x, vHalf.y));
        vec2 q = abs(lp) - vHalf + r;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
        a = (1.0 - smoothstep(-0.7, 0.7, d * max(vHalf.x, 1.0) / max(vHalf.x, 1.0))) * vCol.a;
        o = vec4(c * vCol.rgb * a, a);
        return;
    }
    if (vPrm.w > 2.5) { // flat
        a = vCol.a;
        o = vec4(vCol.rgb * a, a);
        return;
    }
    if (vPrm.w > 1.5) { // a sphere
        vec2 q = vLocal / vHalf.x;
        float r = length(q), aa = 1.6 / vHalf.x;
        a = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);
        float z = sqrt(max(0.0, 1.0 - r * r));
        vec3 n = vec3(q.x, -q.y, z);
        vec3 L = normalize(vec3(-0.55, 0.5, 0.67));
        float dif = max(0.0, dot(n, L));
        float lon = atan(n.x, n.z) + vPrm.z, lat = n.y, seed = vPrm.x;
        int type = int(vPrm.y + 0.5);
        vec3 base = vCol.rgb, c;
        float light = 0.1 + 1.0 * dif;
        if (type == 1) {
            float w = fbm3(vec2(lon * 1.3, lat * 5.0) + seed) * 3.0;
            float b = sin(lat * 13.0 + w + seed * 3.0);
            vec3 c2 = mix(base.gbr, vec3(1.0, 0.92, 0.8), 0.35);
            c = mix(base, c2, 0.5 + 0.5 * b);
            c = mix(c, vec3(1.0), 0.15 * smoothstep(0.7, 1.0, fbm3(vec2(lon * 3.0, lat * 9.0) + seed)));
        } else if (type == 2) {
            float land = fbm3(vec2(lon * 1.4, lat * 2.2) + seed * 2.0);
            c = mix(base * 0.8, mix(vec3(0.45, 0.62, 0.32), base, 0.35), smoothstep(0.5, 0.54, land));
            c = mix(c, vec3(0.97), smoothstep(0.72, 0.86, abs(lat)) + 0.5 * smoothstep(0.62, 0.8, fbm3(vec2(lon * 3.0 + vPrm.z * 0.3, lat * 6.0) - seed)));
        } else if (type == 3) {
            c = base * (1.02 - 0.18 * smoothstep(-0.3, 1.0, -dot(n, L)));
            light = 1.0;
            c = mix(c, vec3(1.0), 0.75 * smoothstep(0.3, 0.12, length(q - vec2(-0.36, -0.42))));
        } else if (type == 4) {
            float g = fbm3(vec2(lon * 2.5, lat * 4.0) * 1.5 + seed + vPrm.z * 0.1);
            c = mix(base, vec3(1.0, 0.97, 0.82), g * 0.9) * (1.15 - 0.3 * (1.0 - z));
            light = 1.0;
        } else {
            float g = fbm3(vec2(lon * 1.6, lat * 2.6) * 2.0 + seed);
            float cr = smoothstep(0.62, 0.66, fbm3(vec2(lon * 4.0, lat * 6.0) - seed));
            c = base * (0.6 + 0.65 * g) * (1.0 - 0.25 * cr);
        }
        c *= light;
        if (type != 4) c += base * pow(1.0 - z, 2.5) * (type == 3 ? 0.15 : 0.55); // atmosphere
        a *= vCol.a;
        o = vec4(c * a, a);
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

const char* BG_FS = R"(#version 330 core
out vec4 o;
uniform vec2 uRes;
uniform float uTime, uLevel;
uniform vec2 uCam;
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
    } else if (uKind == 5) { // galaxy: rich nebula layers in parallax with the camera, star fields
        vec2 q = p * 1.1 + uCam * 0.12 + vec2(t * 0.004, 0.0);
        float n1 = fbm(q + fbm(q * 1.4 + t * 0.006));
        float n2 = fbm(q * 1.9 + vec2(3.0, -2.0) + uCam * 0.08);
        float n3 = fbm(q * 0.8 - vec2(6.0, 1.0));
        c = vec3(0.012, 0.010, 0.035);
        c += vec3(0.55, 0.12, 0.55) * pow(n1, 3.2) * 1.6;
        c += vec3(0.05, 0.35, 0.60) * pow(n2, 3.0) * 1.4;
        c += vec3(0.75, 0.32, 0.10) * pow(n3, 4.0) * 1.2;
        c *= 0.9 + 0.35 * uLevel;
        for (int l = 0; l < 3; l++) {
            float sc = 70.0 + 60.0 * float(l);
            vec2 sp = (p + uCam * (0.05 + 0.1 * float(l))) * sc;
            vec2 id = floor(sp), f = fract(sp) - 0.5;
            float hh = h21(id + float(l) * 17.0);
            if (hh > 0.92) {
                vec2 off = vec2(h21(id + 3.1), h21(id + 7.7)) - 0.5;
                float d = length(f - off * 0.6);
                float tw = 0.65 + 0.35 * sin(t * (0.5 + hh * 1.5) + hh * 40.0);
                vec3 sc3 = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.8, 0.6), h21(id + 9.0));
                c += sc3 * smoothstep(0.1, 0.0, d) * tw;
            }
        }
        c *= 1.0 - 0.35 * dot(p, p);
    } else if (uKind == 6) { // orrery: deep space warmed by the sun's light
        vec2 q = p * 1.3 + uCam * 0.1;
        float n1 = fbm(q * 1.2 + t * 0.003), n2 = fbm(q * 2.4 - 3.0);
        c = vec3(0.010, 0.012, 0.03);
        c += vec3(0.45, 0.20, 0.08) * pow(n1, 3.0) * 1.2;
        c += vec3(0.10, 0.18, 0.40) * pow(n2, 3.0);
        c *= 0.9 + 0.3 * uLevel;
        for (int l = 0; l < 2; l++) {
            vec2 sp = (p + uCam * (0.04 + 0.08 * float(l))) * (110.0 + 80.0 * float(l));
            vec2 id = floor(sp), f = fract(sp) - 0.5;
            float hh = h21(id + float(l) * 7.0);
            if (hh > 0.94) c += vec3(0.9, 0.9, 1.0) * smoothstep(0.1, 0.0, length(f)) * (0.6 + 0.4 * sin(t + hh * 50.0));
        }
    } else if (uKind == 7) { // atlas: navy paper, an engraved grid of the sky, faint stars
        vec2 w = p + uCam;
        float grain = fbm(fc * 0.25) * 0.05 + (h21(fc) - 0.5) * 0.02;
        c = mix(vec3(0.035, 0.07, 0.15), vec3(0.06, 0.11, 0.21), fbm(w * 1.5)) + grain * vec3(0.3, 0.3, 0.2);
        vec2 ctr = vec2(0.0, 2.6);
        float d = length(w - ctr), ang = atan(w.x - ctr.x, ctr.y - w.y);
        float l1 = abs(fract(d * 7.0) - 0.5) / fwidth(d * 7.0);
        float l2 = abs(fract(ang * 18.0) - 0.5) / fwidth(ang * 18.0);
        float lines = (1.0 - smoothstep(0.0, 1.2, min(l1, l2) - 0.0));
        c += vec3(0.75, 0.6, 0.3) * lines * 0.10;
        vec2 sp = (p + uCam * 0.6) * 140.0;
        vec2 id = floor(sp), f = fract(sp) - 0.5;
        float hh = h21(id);
        if (hh > 0.9) c += vec3(0.85, 0.82, 0.7) * smoothstep(0.08, 0.0, length(f)) * (0.35 + 0.25 * sin(t * 0.7 + hh * 30.0));
        c *= 1.0 - 0.5 * pow(length((uv - 0.5) * vec2(1.0, 1.2)), 2.2);
    } else if (uKind == 8) { // synthwave: a sunset sky, a striped sun, mountains and a scrolling neon grid
        float hor = 0.17;
        if (p.y < hor) {
            float k = clamp((p.y + 0.5) / (hor + 0.5), 0.0, 1.0);
            c = mix(vec3(0.03, 0.0, 0.10), vec3(0.20, 0.03, 0.32), smoothstep(0.0, 0.6, k));
            c = mix(c, vec3(1.0, 0.25, 0.55), smoothstep(0.55, 1.0, k) * 0.85);
            vec2 sp = p * 120.0;
            vec2 id = floor(sp), f = fract(sp) - 0.5;
            float hh = h21(id);
            if (hh > 0.95) c += vec3(1.0, 0.85, 1.0) * smoothstep(0.1, 0.0, length(f)) * (1.0 - k) * (0.6 + 0.4 * sin(t * 1.3 + hh * 40.0));
            vec2 sc = vec2(-uCam.x * 0.04, hor - 0.2);
            float sd = length(p - sc);
            float sunR = 0.27;
            float stripes = step(0.0, p.y - sc.y) * step(fract((p.y - sc.y) * 22.0 - t * 0.15), (p.y - sc.y) * 2.2);
            vec3 sunC = mix(vec3(1.0, 0.92, 0.35), vec3(1.0, 0.15, 0.55), smoothstep(-0.25, 0.22, p.y - sc.y));
            c = mix(c, sunC, (1.0 - smoothstep(sunR - 0.003, sunR + 0.003, sd)) * (1.0 - stripes));
            c += vec3(1.0, 0.3, 0.6) * exp(-max(0.0, sd - sunR) * 9.0) * 0.35 * (0.9 + 0.3 * uLevel);
            float mx = p.x + uCam.x * 0.25;
            float ridge = hor - 0.03 - 0.10 * pow(fbm(vec2(mx * 2.2, 1.0)), 1.6) - 0.05 * abs(sin(mx * 3.0));
            if (p.y > ridge) {
                c = mix(vec3(0.10, 0.02, 0.18), vec3(0.18, 0.03, 0.25), (p.y - ridge) * 8.0);
                c += vec3(1.0, 0.3, 0.8) * smoothstep(0.004, 0.0, p.y - ridge) * 0.8;
            }
        } else {
            float y = p.y - hor;
            float z = 0.22 / y;
            float x = p.x * z + uCam.x * 1.0;
            float zz = z + t * 0.8;
            float gx = abs(fract(x * 1.2) - 0.5) / fwidth(x * 1.2);
            float gz = abs(fract(zz * 1.2) - 0.5) / fwidth(zz * 1.2);
            float g = (1.0 - smoothstep(0.0, 1.6, min(gx, gz))) * smoothstep(0.0, 0.07, y);
            c = vec3(0.06, 0.0, 0.12);
            vec3 lc = mix(vec3(1.0, 0.2, 0.75), vec3(0.2, 0.9, 1.0), smoothstep(0.0, 0.5, y));
            c += lc * g * 1.2;
            c = mix(c, vec3(0.6, 0.12, 0.45), exp(-y * 18.0) * 0.8);
        }
    } else if (uKind == 9) { // pastel: a candy sky, soft clouds drifting, twinkling pastel stars
        c = mix(vec3(1.0, 0.85, 0.93), vec3(0.86, 0.83, 1.0), smoothstep(0.0, 0.7, uv.x + uv.y * 0.3));
        c = mix(c, vec3(0.80, 0.96, 0.93), smoothstep(0.5, 1.2, uv.y + uv.x * 0.4));
        for (int l = 0; l < 2; l++) {
            vec2 q = p * (1.2 + 0.8 * float(l)) + uCam * (0.15 + 0.25 * float(l)) + vec2(t * (0.008 + 0.006 * float(l)), float(l) * 4.0);
            float cl = fbm(q * vec2(1.0, 1.8));
            float m = smoothstep(0.52, 0.68, cl);
            c = mix(c, mix(vec3(1.0), vec3(0.95, 0.9, 1.0), smoothstep(0.68, 0.5, cl)), m * (0.55 + 0.2 * float(l)));
        }
        vec2 sp = (p + uCam * 0.1) * 26.0;
        vec2 id = floor(sp), f = fract(sp) - 0.5;
        float hh = h21(id);
        if (hh > 0.86) {
            vec2 off = vec2(h21(id + 2.0), h21(id + 5.0)) - 0.5;
            vec2 d = abs(f - off * 0.5);
            float tw = 0.5 + 0.5 * sin(t * (0.8 + hh) + hh * 30.0);
            float star = smoothstep(0.03, 0.0, min(d.x, d.y)) * smoothstep(0.16 * tw + 0.04, 0.0, d.x + d.y);
            vec3 sc = mix(vec3(1.0, 0.85, 0.4), vec3(1.0, 0.6, 0.85), h21(id + 9.0));
            sc = mix(sc, vec3(0.55, 0.75, 1.0), step(0.6, h21(id + 4.0)));
            c = mix(c, sc, star);
        }
    } else if (uKind == 4) { // glass: slow blurred colour blobs on deep navy
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

void Painter::background(int kind, float time, float level, float camX, float camY) {
    flush();
    glDisable(GL_BLEND);
    glUseProgram(bgProg_);
    ugl::set2f(bgProg_, "uRes", (float)w_, (float)h_);
    ugl::set1f(bgProg_, "uTime", time);
    ugl::set1f(bgProg_, "uLevel", level);
    ugl::set1i(bgProg_, "uKind", kind);
    ugl::set2f(bgProg_, "uCam", camX, camY);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
}

void Painter::quad(const float px[4][2], const float lc[4][2], float hx, float hy, Col c, float rad, float soft, float lw, float mode,
                   const Col* bottom) {
    static const int IDX[6] = {0, 1, 2, 0, 2, 3};
    for (int k : IDX) {
        const Col& cc = bottom && k >= 2 ? *bottom : c;
        verts_.push_back({px[k][0], px[k][1], lc[k][0], lc[k][1], hx, hy, cc.r, cc.g, cc.b, cc.a, rad, soft, lw, mode});
    }
}

void Painter::image(GLuint tex, float x, float y, float w, float h, Col tint, float radius, bool flipV) {
    if (!tex || w <= 0 || h <= 0 || tint.a <= 0.f) return;
    if (curTex_ != tex) { flush(); curTex_ = tex; }
    const float hx = w * 0.5f, hy = h * 0.5f;
    const float px[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    const float t0 = flipV ? 0.f : 1.f, t1 = 1.f - t0;
    const float lc[4][2] = {{0, t0}, {1, t0}, {1, t1}, {0, t1}};
    // the rounded corners work on pixel distances: the half size goes in vHalf, the uv in vLocal
    static const int IDX[6] = {0, 1, 2, 0, 2, 3};
    for (int k : IDX) {
        const float lx = lc[k][0], ly = lc[k][1];
        verts_.push_back({px[k][0], px[k][1], lx, ly, hx, hy, tint.r, tint.g, tint.b, tint.a, radius, 0, 0, 4.f});
    }
}

void Painter::tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c0, Col c1, Col c2) {
    const float P[3][2] = {{x0, y0}, {x1, y1}, {x2, y2}};
    const Col C[3] = {c0, c1, c2};
    for (int k = 0; k < 3; k++)
        verts_.push_back({P[k][0], P[k][1], 0, 0, 0, 0, C[k].r, C[k].g, C[k].b, C[k].a, 0, 0, 0, 3.f});
}

void Painter::kaleido(GLuint a, GLuint b, float mixAB, float cx, float cy, float rot, int segs, Col tint, float time, float level) {
    static const char* KFS = R"(#version 330 core
out vec4 o;
uniform sampler2D uA, uB;
uniform vec2 uRes, uC;
uniform float uMix, uRot, uTime, uLevel;
uniform int uSeg;
uniform vec3 uTint;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
void main() {
    vec2 fc = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    vec2 p = (fc - uC) / uRes.y;
    float r = length(p);
    float seg = 6.2831853 / float(uSeg);
    float an = atan(p.y, p.x) + uRot;
    float k = mod(an, seg);
    float m = abs(k - seg * 0.5);               // mirrored inside each segment
    vec2 q = vec2(cos(m), sin(m)) * r;
    // glass facets: a triangle grid in the folded space, each facet its own shade, lead between them
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
    vec3 c = mix(texture(uA, uv).rgb, texture(uB, uv).rgb, uMix);
    c *= 0.82 + 0.3 * h21(cell + 9.1);
    c = mix(c, c * uTint * 1.5, 0.22);
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = max(mix(vec3(l), c, 1.35), 0.0);                    // art glass: richer colour
    c = c * 1.25 + 0.04 + uTint * (0.10 + 0.14 * h21(cell + 5.3)); // lit from behind, tinted glass even over a dark world
    c *= 0.85 + 0.3 * smoothstep(0.85, 0.0, edge);           // brighter toward each facet's middle
    float lead = 1.0 - smoothstep(0.015, 0.04, edge);
    float segLine = 1.0 - smoothstep(0.0, 2.0, m * r * uRes.y);
    c = mix(c, vec3(0.03, 0.03, 0.05), max(lead * 0.7, segLine * 0.55));
    c *= 0.9 + 0.2 * uLevel;
    c *= 1.0 - 0.5 * smoothstep(0.45, 1.2, r);
    o = vec4(c, 1.0);
}
)";
    flush();
    if (!kalProg_) kalProg_ = ugl::program(ugl::FULLSCREEN_VS, KFS, "menu kaleidoscope");
    glDisable(GL_BLEND);
    glUseProgram(kalProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, b ? b : a);
    glActiveTexture(GL_TEXTURE0);
    ugl::set1i(kalProg_, "uA", 0);
    ugl::set1i(kalProg_, "uB", 1);
    ugl::set2f(kalProg_, "uRes", (float)w_, (float)h_);
    ugl::set2f(kalProg_, "uC", cx, cy);
    ugl::set1f(kalProg_, "uMix", mixAB);
    ugl::set1f(kalProg_, "uRot", rot);
    ugl::set1f(kalProg_, "uTime", time);
    ugl::set1f(kalProg_, "uLevel", level);
    ugl::set1i(kalProg_, "uSeg", segs);
    glUniform3f(glGetUniformLocation(kalProg_, "uTint"), tint.r, tint.g, tint.b);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    curTex_ = 0;
}

void Painter::sphere(float cx, float cy, float r, Col c, float seed, int type, float rot) {
    if (r <= 0.3f || c.a <= 0.f) return;
    const float e = 2.f;
    const float px[4][2] = {{cx - r - e, cy - r - e}, {cx + r + e, cy - r - e}, {cx + r + e, cy + r + e}, {cx - r - e, cy + r + e}};
    const float lc[4][2] = {{-r - e, -r - e}, {r + e, -r - e}, {r + e, r + e}, {-r - e, r + e}};
    quad(px, lc, r, r, c, seed, (float)type, rot, 2.f);
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
        stbtt_GetPackedQuad(pc, 2048, 1024, ch - 32, &ax, &ay, &q, 0);
        const float x0 = x + pxx + q.x0 * k, x1 = x + pxx + q.x1 * k, y0 = y + pyy + q.y0 * k, y1 = y + pyy + q.y1 * k;
        const float px[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
        const float lc[4][2] = {{q.s0, q.t0}, {q.s1, q.t0}, {q.s1, q.t1}, {q.s0, q.t1}};
        if (ch != ' ') quad(px, lc, 0, 0, c, 0, 0, 0, 1.f);
        pxx += ax * k + tracking * size;
    }
    return w;
}

float Painter::textFx(Font id, const std::string& s, float x, float y, float size, Col top, Col bottom, int align, float tracking,
                      float shear) {
    FontData& f = font(id);
    if (!f.ok || s.empty()) return 0.f;
    const float w = width(id, s, size, tracking);
    if (align == 0) x -= w * 0.5f;
    else if (align > 0) x -= w;
    if (curTex_ != f.tex) { flush(); curTex_ = f.tex; }
    const float k = size / f.px;
    const auto* pc = (const stbtt_packedchar*)f.packed.data();
    float pxx = 0;
    const float capTop = y - size * 0.72f;
    for (size_t i = 0; i < s.size(); i++) {
        int ch = (unsigned char)s[i];
        if (ch < 32 || ch > 126) ch = '?';
        stbtt_aligned_quad q;
        float ax = 0, ay = 0;
        stbtt_GetPackedQuad(pc, 2048, 1024, ch - 32, &ax, &ay, &q, 0);
        const float x0 = x + pxx + q.x0 * k, x1 = x + pxx + q.x1 * k, y0 = y + q.y0 * k, y1 = y + q.y1 * k;
        const float s0 = (y - y0) * shear, s1 = (y - y1) * shear;
        const float px[4][2] = {{x0 + s0, y0}, {x1 + s0, y0}, {x1 + s1, y1}, {x0 + s1, y1}};
        const float lc[4][2] = {{q.s0, q.t0}, {q.s1, q.t0}, {q.s1, q.t1}, {q.s0, q.t1}};
        auto at = [&](float yy) { return mixc(top, bottom, std::clamp((yy - capTop) / (size * 0.75f), 0.f, 1.f)); };
        const Col ct = at(y0), cb = at(y1);
        if (ch != ' ') quad(px, lc, 0, 0, ct, 0, 0, 0, 1.f, &cb);
        pxx += ax * k + tracking * size;
    }
    return w;
}

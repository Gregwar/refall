#pragma once
// MICROSCOPE's shaders (see microscope.cpp): a picture-book Petri dish.
//
// FIELD_FS paints the water of the dish (a saturated tone, a little deeper toward its rim, soft patches of light
// drifting in it far from the well) and the well's calm rounded pane; LIFE_FS the creatures (one signed distance per
// kind: a bean, a blob, a tadpole, a daisy, a rotifer with its two wheels, a volvox ball with its babies, and plain
// bubbles), toon shaded (a flat color, one cel shadow, a bold outline, a gloss) with big kawaii eyes always upright;
// PIECE_FS the pieces (one plump jelly cell per block, one color per type, a gloss, nothing inside); FINAL_FS the
// eyepiece (the round dish in its thick toy rim, on the variant's table color) and the frame. The water and the
// creatures' colors come from the variant's own palette and the drop's, soaking in unevenly (stainK).
#include <string>

namespace microscope_shaders {

inline const char* COMMON = R"(
uniform vec3 uWaterA[3], uWaterB[2];   // the water: center, edge, the well's pane; the drop's center, edge
uniform vec3 uPiece[7];
uniform vec3 uNight;                   // slow motion: the dish by moonlight (a multiplier)
uniform float uStain, uStainSeed;      // the drop's palette soaked in (0..1)
uniform float uSlow;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.875;
}
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdRoundBox(vec2 p, vec2 b, float r) { return sdBox(p, b - r) - r; }
// How far the drop's palette has soaked in here: unevenly, like a dye spreading through the dish.
float stainK(vec2 w) {
    float n = fbm(w * 0.06 + vec2(uStainSeed, 0.0)) * 0.8 + vnoise(w * 0.4 + uStainSeed) * 0.2;
    return smoothstep(n - 0.2, n + 0.2, uStain * 1.45 - 0.22);
}
float lumOf(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
// slow motion: the dish by moonlight, its colors drawn toward one deep blue
vec3 nightOf(vec3 c, float k) { return mix(c, lumOf(c) * uNight + c * 0.12, uSlow * k); }
vec3 nightOf(vec3 c) { return nightOf(c, 1.0); }
)";

// ---------------------------------------------------------------- the water (full screen)
inline const char* FIELD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec2 uDisk;          // the dish's center y, radius
uniform float uTime, uSwell, uSeed, uAA, uWell;
uniform vec3 uLine;          // the pane's edge
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float r = length(w - vec2(0.0, uDisk.x)) / uDisk.y;
    float k = stainK(w);
    vec3 wc = mix(uWaterA[0], uWaterB[0], k), we = mix(uWaterA[1], uWaterB[1], k);
    vec3 c = mix(wc, we, smoothstep(0.3, 1.0, r));
    // soft patches of light drifting slowly in the water, away from the well and the HUD's columns
    float away = smoothstep(5.5, 9.0, abs(w.x)) * (0.4 + 0.6 * smoothstep(11.0, 13.5, abs(w.x)));
    float n = fbm(w * 0.075 + vec2(uSeed, uTime * 0.012));
    c *= 1.0 + (smoothstep(0.45, 0.75, n) * 0.14 - 0.04) * away;
    c *= 1.0 + 0.12 * uSwell * away;
    // the well: a calm rounded pane, a soft shade around it
    float dw = sdRoundBox(w - vec2(0.0, 0.15), vec2(5.24, 10.37), 0.45);
    float pane = (1.0 - smoothstep(-uAA, uAA, dw)) * uWell;
    c *= 1.0 - 0.16 * exp(-max(dw, 0.0) / 0.5) * step(0.0, dw) * uWell;
    c = mix(c, uWaterA[2], pane);
    float edge = (1.0 - smoothstep(0.05 - uAA, 0.05 + uAA, abs(dw + 0.02))) * uWell;
    c = mix(c, uLine, edge * 0.7);
    frag = vec4(nightOf(c), 0.0);
}
)";

// ---------------------------------------------------------------- the creatures and bubbles (instanced)
// a: x, y, size, rotation. b: kind, seed, time, alpha. c: color (linear), stretch. d: the drop's color, lid (0 open,
// 1 closed). e: happy (0..1), depth (0 front, 1 back), spin (wheels, petals, babies), look angle.
inline const char* LIFE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
layout(location = 5) in vec4 aE;
uniform mat4 uVP;
out vec2 vL;
out vec2 vWorld;
flat out vec4 vA, vB, vC, vD, vE;
void main() {
    vec2 l = aPos * 0.72;
    float c = cos(aA.w), s = sin(aA.w);
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y) * aA.z;
    vL = l;
    vWorld = wp;
    vA = aA; vB = aB; vC = aC; vD = aD; vE = aE;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* LIFE_FS = R"(#version 330 core
in vec2 vL;
in vec2 vWorld;
flat in vec4 vA, vB, vC, vD, vE;
out vec4 frag;
uniform float uGlow;
uniform vec3 uEyeInk, uBlush;
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
float sdRoundCone(vec2 p, vec2 a, vec2 b, float r1, float r2) {
    vec2 ba = b - a; float l2 = dot(ba, ba); float rr = r1 - r2; float a2 = l2 - rr * rr; float il2 = 1.0 / l2;
    vec2 pa = p - a; float y = dot(pa, ba); float z = y - l2; vec2 xv = pa * l2 - ba * y; float x2 = dot(xv, xv);
    float y2 = y * y * l2; float z2 = z * z * l2; float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}
int kind;
float seed, T, spin;
// The body's outline (negative inside), in the creature's own frame (x: its heading), its size 1.
float body(vec2 q) {
    if (kind == 0) {
        // a bean (a paramecium): plump, a dimple on one side, a soft fringe of cilia rippling along its edge
        vec2 ab = vec2(0.43, 0.27);
        float d = (length(q / ab) - 1.0) * 0.3;
        d += 0.05 * exp(-pow((q.x - 0.04) / 0.13, 2.0)) * smoothstep(0.0, 0.25, q.y);
        float th = atan(q.y / ab.y, q.x / ab.x);
        return d - 0.013 * (0.5 + 0.5 * cos(th * 16.0 - T * 5.0));
    }
    if (kind == 1) {
        // a blob (an amoeba): round, slowly reaching out a lobe here and there
        float th = atan(q.y, q.x), r = length(q);
        float wk = smoothstep(0.05, 0.25, r);
        float tp = 1.3 * sin(T * 0.11 + seed * 4.0);
        float R = 0.34 + 0.03 * sin(2.0 * th + T * 0.35 + seed * 5.0) + 0.022 * sin(3.0 * th - T * 0.27 + seed * 3.0) +
                  0.08 * pow(max(cos(th - tp), 0.0), 5.0) + 0.05 * pow(max(cos(th - tp - 2.4), 0.0), 7.0);
        return (r - mix(0.34, R, wk)) * 0.88;
    }
    if (kind == 2) {
        // a tadpole (a flagellate): a round head, a long tail lashing in waves
        float head = length((q - vec2(0.13, 0.0)) * vec2(1.0, 1.06)) - 0.27;
        float u = clamp((0.0 - q.x) / 0.62, 0.0, 1.0);
        float yo = 0.11 * u * sin(u * 5.0 - T * 6.0);
        float tail = abs(q.y - yo) * 0.85 - mix(0.12, 0.025, u);
        tail = max(tail, -q.x - 0.62);
        tail = max(tail, q.x - 0.1);
        return smin(head, tail, 0.08);
    }
    if (kind == 3) {
        // a daisy (a centric diatom): a round flower of seven petals, turning slowly
        float th = atan(q.y, q.x);
        float r = length(q);
        return (r - 0.32 - 0.05 * cos(7.0 * (th - spin)) * smoothstep(0.05, 0.25, r)) * 0.86;
    }
    if (kind == 4) {
        // a rotifer: a pear of a body, two round wheels on its head
        float b = sdRoundCone(q, vec2(-0.38, 0.0), vec2(0.05, 0.0), 0.07, 0.25);
        float e = min(length(q - vec2(0.26, 0.19)), length(q - vec2(0.26, -0.19))) - 0.125;
        return smin(b, e, 0.07);
    }
    if (kind == 5) {
        // a volvox: a big round ball
        return length(q) - 0.42 - 0.008 * sin(T * 1.3 + seed * 6.0);
    }
    return length(q) - 0.42; // a bubble
}

void main() {
    kind = int(vB.x + 0.5);
    seed = vB.y;
    T = vB.z;
    spin = vE.z;
    float st = vC.w;
    // squash and stretch along its heading (its volume kept)
    vec2 q = vL / vec2(1.0 + st, 1.0 - st * 0.8);
    float d = body(q);
    float aa = max(length(fwidth(q)) * 0.7, 1e-4);
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.0) discard;
    float k = stainK(vWorld);
    vec3 col = mix(vC.rgb, vD.rgb, k);
    if (kind == 6) {
        // a bubble: a clear ring, a faint fill, a gloss
        float ring = 1.0 - smoothstep(0.03 - aa, 0.03 + aa, abs(d + 0.035));
        vec2 hp = vL - vec2(-0.16, 0.17);
        float gl = 1.0 - smoothstep(0.07 - aa, 0.07 + aa, length(hp * vec2(1.0, 1.6)));
        float a = cov * (0.12 + 0.55 * ring + 0.6 * gl) * vB.w;
        frag = vec4(nightOf(col) * a, a);
        return;
    }
    // a pillow normal (screen space): flat inside, rounding off toward the outline, for one cel shadow and a gloss
    vec2 g = vec2(dFdx(d), dFdy(d));
    vec2 n = g / max(length(g), 1e-6);
    float tin = smoothstep(0.0, 0.2, -d);
    vec3 N = normalize(vec3(n * (1.0 - tin), 0.3 + tin));
    const vec3 L = vec3(-0.5, 0.62, 0.6);
    float dif = dot(N, normalize(L));
    vec3 shade = col * vec3(0.7, 0.66, 0.86);
    vec3 c = mix(shade, col, smoothstep(0.2, 0.28, dif));
    // the screen-upright frame (the face never turns upside down)
    float ca = cos(vA.w), sa = sin(vA.w);
    vec2 s = vec2(ca * q.x - sa * q.y, sa * q.x + ca * q.y);
    // a few parts of their own: the daisy's heart, the rotifer's wheels, the volvox's babies
    if (kind == 3) {
        float h = length(q) - 0.2;
        c = mix(c, mix(col, vec3(1.0, 0.97, 0.85), 0.55), 1.0 - smoothstep(-aa, aa, h));
    }
    if (kind == 4) {
        for (int i = 0; i < 2; i++) {
            vec2 v = q - vec2(0.26, i == 0 ? 0.19 : -0.19);
            float r = length(v);
            float an = atan(v.y, v.x) * (i == 0 ? 1.0 : -1.0) + spin;
            float blade = smoothstep(0.1, 0.4, cos(3.0 * an));
            float wheel = 1.0 - smoothstep(0.075 - aa, 0.075 + aa, r);
            c = mix(c, mix(col, vec3(1.0), 0.45 - 0.25 * blade), wheel);
        }
    }
    if (kind == 5) {
        for (int i = 0; i < 3; i++) {
            float an = spin + float(i) * 2.094 + seed * 6.0;
            vec2 bc = vec2(cos(an), sin(an)) * 0.24;
            float bd = length(q - bc) - 0.085;
            vec3 bcol = mix(col, vec3(1.0), 0.5);
            c = mix(c, bcol, 1.0 - smoothstep(-aa, aa, bd));
            c = mix(c, col * 0.5, (1.0 - smoothstep(0.012 - aa, 0.012 + aa, abs(bd))) * 0.7);
        }
    }
    // the gloss: a soft white oval toward the light, and a thin bright line along the lit edge
    float spec = smoothstep(0.86, 0.9, dif) * (1.0 - tin);
    c = mix(c, vec3(1.0), spec * 0.55);
    // the face: two big shiny eyes, rosy cheeks, a small smile
    float fs = kind == 5 ? 1.25 : kind == 1 ? 1.15 : 1.0;
    vec2 fc = vec2(kind == 0 ? 0.17 : kind == 1 ? 0.06 : kind == 2 ? 0.13 : 0.0, 0.0);
    fc = vec2(ca * fc.x, sa * fc.x) + vec2(0.0, kind == 4 ? 0.0 : 0.02);
    if (kind == 4) fc = vec2(ca, sa) * 0.06;
    vec2 look = vec2(cos(vE.w), sin(vE.w)) * 0.016 * fs;
    float lid = vD.w, happy = vE.x;
    for (int i = 0; i < 2; i++) {
        float side = i == 0 ? -1.0 : 1.0;
        vec2 ec = fc + vec2(side * 0.105, 0.03) * fs;
        // the cheeks
        vec2 bp = (s - ec - vec2(side * 0.045, -0.085) * fs) / (vec2(0.05, 0.028) * fs);
        c = mix(c, uBlush, (1.0 - smoothstep(0.6, 1.0, length(bp))) * 0.5);
        // an open eye: a tall dark oval, two glints; closing with the lid, a happy arc on the drop
        float open = (1.0 - lid) * (1.0 - happy);
        vec2 er = vec2(0.052, 0.07) * fs;
        er.y *= max(open, 0.0);
        if (open > 0.08) {
            vec2 e = s - ec - look;
            float de = (length(e / er) - 1.0) * er.y;
            float eye = 1.0 - smoothstep(-aa, aa, de);
            float g1 = 1.0 - smoothstep(-aa, aa, length(e - vec2(-0.016, 0.022) * fs) - 0.021 * fs * min(1.0, open * 1.5));
            float g2 = 1.0 - smoothstep(-aa, aa, length(e - vec2(0.02, -0.024) * fs) - 0.009 * fs);
            c = mix(c, uEyeInk, eye);
            c = mix(c, vec3(1.0), (g1 + g2 * 0.9) * eye);
        } else {
            // closed: a curve, down (asleep) or up (happy)
            float up = happy > (1.0 - lid) ? 1.0 : 0.0;
            vec2 ac = ec + vec2(0.0, up > 0.5 ? -0.03 : 0.035) * fs;
            vec2 v = s - ac;
            float arc = abs(length(v) - 0.05 * fs) - 0.011 * fs;
            arc = max(arc, up > 0.5 ? -v.y : v.y);
            c = mix(c, uEyeInk, 1.0 - smoothstep(-aa, aa, arc));
        }
    }
    {
        vec2 v = s - fc - vec2(0.0, -0.02) * fs;
        float m = abs(length(v) - 0.032 * fs) - 0.009 * fs;
        m = max(m, v.y + 0.012 * fs);
        c = mix(c, uEyeInk, 1.0 - smoothstep(-aa, aa, m));
    }
    // the bold outline, a deep shade of its own color
    vec3 ink = mix(col * 0.32, uEyeInk, 0.35);
    c = mix(c, ink, smoothstep(-0.036 - aa, -0.036 + aa, d));
    c *= uGlow;
    // the far ones sit back in the water
    c = mix(c, mix(uWaterA[1], uWaterB[1], k) * 1.1, vE.y * 0.4);
    float a = cov * vB.w;
    frag = vec4(nightOf(c, 0.55) * a, a);
}
)";

// ---------------------------------------------------------------- the pieces (instanced per cell)
// a: x, y (cell center), half (board units), kind (0 cell, 1 ghost, 2 popping, 3 deflating). b: type, -, progress,
// alpha. c: lock flash, -, -, -. d: unused.
inline const char* PIECE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 l = aPos * 0.62;
    vLocal = l;
    vec2 wp = aA.xy + l * 2.0 * aA.z;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* PIECE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform vec3 uEyeInk;
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    int type = int(vB.x + 0.5);
    float t = vB.z;
    const float GAP = 0.045;
    float sc = 1.0, rc = 0.22;
    vec2 off = vec2(0.0);
    if (kind == 2) {
        // popping: it puffs up a little into a ball, then shrinks away
        float up = smoothstep(0.0, 0.35, t), down = smoothstep(0.3, 1.0, t);
        sc = (1.0 + 0.1 * up) * (1.0 - down * down);
        rc = mix(0.2, 0.45, up);
    }
    if (kind == 3) {
        // deflating: it sags and shrinks
        float e = smoothstep(0.0, 1.0, t);
        sc = 1.0 - 0.3 * e;
        off = vec2(0.0, -0.14 * e);
        rc = mix(0.2, 0.32, e);
    }
    if (sc <= 0.01) discard;
    float d = sdRoundBox((p - off) / sc, vec2(0.5 - GAP), rc) * sc;
    float aa = max(fwidth(d), 1e-4);
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.0) discard;
    vec3 col = uPiece[type];
    if (kind == 1) {
        // the ghost: a faint outline of the cells, a breath of their color
        float l = 1.0 - smoothstep(0.035 - aa, 0.035 + aa, abs(d + 0.04));
        float a = l * 0.7 * cov * vB.w;
        frag = vec4(col * a, a);
        return;
    }
    // a jelly cell: plump (a pillow normal), one cel shadow, a gloss, a bold outline in a deep shade of its color
    vec2 g = vec2(dFdx(d), dFdy(d));
    vec2 n = g / max(length(g), 1e-6);
    float tin = smoothstep(0.0, 0.16, -d);
    vec3 N = normalize(vec3(n * (1.0 - tin), 0.35 + tin));
    float dif = dot(N, normalize(vec3(-0.5, 0.62, 0.6)));
    vec3 c = mix(col * vec3(0.72, 0.68, 0.86), col, smoothstep(0.18, 0.26, dif));
    c *= 1.0 + 0.1 * smoothstep(-0.3, 0.4, p.y);
    vec2 hp = (p - off) / sc - vec2(-0.15, 0.17);
    hp = mat2(0.87, 0.5, -0.5, 0.87) * hp;
    float gloss = 1.0 - smoothstep(-0.12, 0.05, length(hp / vec2(0.17, 0.085)) - 1.0);
    c = mix(c, vec3(1.0), gloss * 0.6);
    c = mix(c, vec3(1.0), vC.x * 0.35);
    c = mix(c, mix(col * 0.34, uEyeInk, 0.3), smoothstep(-0.05 - aa, -0.05 + aa, d));
    float a = cov * vB.w;
    if (kind == 2) {
        c = mix(c, vec3(1.0), 0.25 * smoothstep(0.0, 0.4, t));
        a *= 1.0 - smoothstep(0.7, 1.0, t);
    }
    if (kind == 3) {
        float e = smoothstep(0.0, 0.8, t);
        c = mix(c, vec3(dot(c, vec3(0.2126, 0.7152, 0.0722))), e * 0.8);
        a *= 1.0 - smoothstep(0.6, 1.0, t);
    }
    if (a <= 0.002) discard;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform vec4 uView;
uniform vec2 uDisk;
uniform float uDim, uFade, uBridge, uRimW, uSlow;
uniform vec3 uBridgeCol, uBridgeInk, uTable, uRim, uRimInk, uNight;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    // the eyepiece: the dish in a thick toy rim (a lit and a shaded side, a bold line each side), on the table
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 cw = w - vec2(0.0, uDisk.x);
    float r = length(cw) - uDisk.y;
    float aa = 1.5 * uView.w / textureSize(uScene, 0).y * 2.0;
    vec3 table = uTable * (1.0 - 0.3 * smoothstep(0.0, 9.0, r));
    table *= 1.0 - 0.35 * exp(-max(r, 0.0) / 0.5);
    float lit = dot(normalize(cw), normalize(vec2(-0.6, 0.8)));
    vec3 rim = uRim * mix(0.78, 1.08, smoothstep(-0.2, 0.3, lit));
    rim = mix(rim, vec3(1.0), (1.0 - smoothstep(0.04, 0.09, abs(r + uRimW * 0.62))) * smoothstep(0.5, 0.85, lit) * 0.6);
    // inside, a soft shade of the rim on the water
    c *= 1.0 - 0.22 * exp(-max(-r - uRimW, 0.0) / 0.6);
    float inRim = 1.0 - smoothstep(-aa, aa, r);
    float inWater = 1.0 - smoothstep(-aa, aa, r + uRimW);
    vec3 o = mix(table, rim, inRim);
    float line = (1.0 - smoothstep(0.06 - aa, 0.06 + aa, abs(r + 0.03))) + (1.0 - smoothstep(0.06 - aa, 0.06 + aa, abs(r + uRimW)));
    o = mix(o, uRimInk, min(line, 1.0));
    o = mix(o, dot(o, vec3(0.2126, 0.7152, 0.0722)) * uNight + o * 0.12, uSlow);
    c = mix(o, c, inWater * (1.0 - min(line, 1.0)));
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
// The ink depends on the pixel: one inside the dish, one on the table around it (uMode 1: ink, 2: accent; 0: the
// vertex color).
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aCol;
uniform vec2 uRes;
out vec4 vCol;
out vec2 vPx;
void main() {
    vCol = aCol;
    vPx = aPos;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* INK_GLSL = R"(
uniform vec3 uDiskPx;          // the dish's center (pixels), radius
uniform vec3 uInkIn, uInkOut;  // sRGB
float inField(vec2 px) { return 1.0 - smoothstep(-6.0, 6.0, length(px - uDiskPx.xy) - uDiskPx.z); }
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec4 vCol;
in vec2 vPx;
out vec4 frag;
uniform int uMode;
void main() {
    vec3 c = uMode == 0 ? vCol.rgb : mix(uInkOut, uInkIn, inField(vPx));
    frag = vec4(c * vCol.a, vCol.a);
}
)";
inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
layout(location = 1) in vec2 aUv;
uniform vec2 uRes;
out vec2 vUv;
out vec2 vPx;
void main() {
    vUv = aUv;
    vPx = aPos;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
in vec2 vPx;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uHaloIn, uHaloOut;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float f = inField(vPx);
    float halo = smoothstep(0.2, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 ink = mix(uInkOut, uInkIn, f), hc = mix(uHaloOut, uHaloIn, f);
    vec3 c = mix(hc, ink, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a fragment shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src, const char* extra = COMMON) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + extra + s.substr(nl + 1);
}

} // namespace microscope_shaders

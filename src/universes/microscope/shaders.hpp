#pragma once
// MICROSCOPE's shaders (see microscope.cpp).
//
// FIELD_FS paints the field of view (the round field of the eyepiece on black: a pale lit field in bright-field, a
// near-black one in dark-field, faint out-of-focus debris, the eyepiece's reticle along the well); ORG_FS the
// microorganisms (one signed distance per kind: paramecium, amoeba, two diatoms, rotifer, volvox), shaded as light
// scattered by their edges in dark-field or as stained, absorbing bodies in bright-field, blurred when out of focus;
// PIECE_FS the pieces (one plant cell per block: a rounded cube of cytoplasm in its wall, with a nucleus); SPECK_FS
// fine debris; DOWN_FS / BLUR_FS the glow and FINAL_FS the frame. Every color comes from the stain: the variant's
// own and the drop's, soaking in unevenly (stainK) across the dish.
#include <string>

namespace microscope_shaders {

inline const char* COMMON = R"(
uniform vec3 uFieldA[2], uFieldB[2];   // field center, edge: the variant's stain, the drop's
uniform vec3 uPieceA[7], uPieceB[7];
uniform vec3 uOrgA[3], uOrgB[3];       // organisms: body, rim, organelles
uniform float uStain, uStainSeed;      // the drop's stain soaked in (0..1)
uniform float uDark, uSlow;            // dark-field amount, slow motion (silver dark-field)
uniform float uOrganic;                // SPECIMEN: no eyepiece, lumps of tissue on a dark bench, seen through loupes
uniform vec3 uBack[2];                 // SPECIMEN's bench: center, edge
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.9375;
}
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
// How far the drop's stain has soaked in here: unevenly, like a dye spreading through the dish.
float stainK(vec2 w) {
    float n = fbm(w * 0.06 + vec2(uStainSeed, 0.0)) * 0.8 + vnoise(w * 0.4 + uStainSeed) * 0.2;
    return smoothstep(n - 0.2, n + 0.2, uStain * 1.45 - 0.22);
}
float lumOf(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
// Slow motion: a silver dark-field, the stains drained.
vec3 silver(vec3 c) {
    float m = max(max(c.r, c.g), max(c.b, 0.03));
    vec3 n = c * (0.85 / m);
    n = mix(n, vec3(lumOf(n)) * vec3(0.86, 0.95, 1.12), 0.7);
    return mix(c, n, uSlow);
}
vec3 pieceCol(int t, float k) { return mix(uPieceA[t], uPieceB[t], k); }
)";

// ---------------------------------------------------------------- the field of view (full screen)
inline const char* FIELD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec2 uDisk;          // the field's center y, radius
uniform float uTime, uIllum, uSwell, uSeed, uAA, uReticle;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    if (uOrganic > 0.5) {
        // SPECIMEN: a dark, damp bench under one soft lamp; the well a darker backing, its walls fine pale lines
        float rb = length((w - vec2(0.0, 1.5)) / vec2(uView.z * 1.05, uView.w * 1.35));
        vec3 b = mix(uBack[0], uBack[1], smoothstep(0.0, 1.0, rb));
        b *= 0.9 + 0.14 * fbm(w * 0.16 + vec2(uSeed, 0.0)) + 0.06 * uSwell;
        b *= 0.94 + 0.06 * (uIllum - 0.93) / 0.075;
        float sw = sdBox(w, vec2(5.12, 10.12));
        b = mix(b, b * 0.42, (1.0 - smoothstep(-0.2, 0.35, sw)) * 0.85);
        float edge = (1.0 - smoothstep(-uAA, uAA, abs(sw) - 0.012)) * step(w.y, 10.0) * uReticle;
        b = mix(b, silver(mix(uPieceA[0], vec3(1.0), 0.6)) * 0.5, edge * 0.35);
        frag = vec4(silver(b), 0.0);
        return;
    }
    vec2 cw = w - vec2(0.0, uDisk.x);
    float r = length(cw) / uDisk.y;
    float k = stainK(w);
    vec3 fc = mix(uFieldA[0], uFieldB[0], k), fe = mix(uFieldA[1], uFieldB[1], k);
    // bright-field: the lamp's even field, a little darker toward the stop
    vec3 bf = mix(fc, fe, smoothstep(0.15, 1.0, r * r)) * (uIllum + 0.05 * uSwell);
    // dark-field: black, a faint glow of scattered light in the middle
    // (a pale field, in a bright-field variant's slow motion, turns to a black one, its tint kept)
    vec3 dfc = fc * min(1.0, 0.004 / max(lumOf(fc), 1e-4)), dfe = fe * min(1.0, 0.0012 / max(lumOf(fe), 1e-4));
    vec3 df = mix(dfc, dfe, smoothstep(0.0, 1.0, r)) * (0.5 + 0.5 * uIllum) + dfc * 1.5 * uSwell * (1.0 - r);
    df = mix(df, vec3(lumOf(df)) * vec3(0.9, 0.97, 1.1), uSlow * 0.8);
    // out-of-focus debris in the medium (soft, faint), never inside the well
    float quiet = 1.0 - smoothstep(-0.8, 0.8, sdBox(w, vec2(5.6, 10.8)));
    quiet = max(quiet, 1.0 - smoothstep(-0.8, 0.8, sdBox(w - vec2(-8.6, 1.8), vec2(2.8, 8.8))));
    quiet = max(quiet, 1.0 - smoothstep(-0.8, 0.8, sdBox(w - vec2(8.6, 2.5), vec2(2.8, 8.0))));
    float n = fbm(w * 0.12 + vec2(uSeed, uTime * 0.003));
    float blob = smoothstep(0.58, 0.85, n) * (1.0 - quiet);
    // a few round motes, out of focus, drifting very slowly
    vec2 mw = w * 0.7 + vec2(uTime * 0.01, uTime * 0.006);
    vec2 mc = floor(mw);
    float hm = hash12(mc + uSeed);
    vec2 mo = vec2(hash12(mc * 1.7 + 3.1), hash12(mc * 2.3 + 7.7)) * 0.6 + 0.2;
    float mote = step(0.86, hm) * exp(-pow(length(fract(mw) - mo) / (0.05 + 0.12 * fract(hm * 13.0)), 2.0)) * (1.0 - quiet);
    bf *= 1.0 - (blob * 0.035 + mote * 0.06);
    df += silver(mix(uOrgA[1], uOrgB[1], k)) * (blob * 0.003 + mote * 0.012);
    vec3 c = mix(bf, df, uDark);
    // the eyepiece's reticle along the well: two fine etched lines and a floor, a scale of ticks on the left
    float ax = abs(w.x) - 5.12;
    float wall = abs(ax) - 0.01;
    float la = (1.0 - smoothstep(-uAA, uAA, wall)) * step(-10.12, w.y) * (1.0 - smoothstep(6.0, 10.4, w.y)) * 0.8;
    float fl = abs(w.y + 10.12) - 0.01;
    la = max(la, (1.0 - smoothstep(-uAA, uAA, fl)) * step(abs(w.x), 5.13));
    float row = w.y + 10.0;
    float ti = abs(fract(row + 0.5) - 0.5) - 0.012;
    bool major = abs(mod(floor(row + 0.5), 5.0)) < 0.5;
    float tl = major ? 0.42 : 0.2;
    float tick = (1.0 - smoothstep(-uAA, uAA, ti)) * step(-5.12 - tl, w.x) * step(w.x, -5.12) * step(-10.05, w.y) *
                 step(w.y, 10.05) * (major ? 0.9 : 0.6);
    la = max(la, tick) * uReticle;
    vec3 ink = mix(fe * 0.35, silver(mix(uOrgA[1], uOrgB[1], k)) * 0.5, uDark);
    c = mix(c, ink, la * mix(0.5, 0.45, uDark));
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the organisms (instanced)
// a: x, y, size, angle. b: kind, seed, time, alpha. c: blur, cilia beat, tint, piece type. d: spin, simple
// (in its cell: one bold shape, its fine details faded out), -, -.
inline const char* ORG_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vL;
out vec2 vWorld;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 l = aPos * 0.7;
    float c = cos(aA.w), s = sin(aA.w);
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y) * aA.z;
    vL = l;
    vWorld = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* ORG_FS = R"(#version 330 core
in vec2 vL;
in vec2 vWorld;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform float uGlow;
float AA;
float disc(vec2 q, vec2 c, float r) { return 1.0 - smoothstep(r - AA, r + AA, length(q - c)); }
float ring(vec2 q, vec2 c, float r, float w) { return 1.0 - smoothstep(w - AA, w + AA, abs(length(q - c) - r)); }
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
float sdRoundCone(vec2 p, vec2 a, vec2 b, float r1, float r2) {
    vec2 ba = b - a; float l2 = dot(ba, ba); float rr = r1 - r2; float a2 = l2 - rr * rr; float il2 = 1.0 / l2;
    vec2 pa = p - a; float y = dot(pa, ba); float z = y - l2; vec2 xv = pa * l2 - ba * y; float x2 = dot(xv, xv);
    float y2 = y * y * l2; float z2 = z * z * l2; float k = sign(rr) * rr * rr * x2;
    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - r2;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - r1;
    return (sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// the nearest cell of a jittered 3D grid (volvox's surface cells)
float cells3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    float best = 9.0;
    for (int z = -1; z <= 1; z++)
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec3 g = vec3(x, y, z);
                vec3 o = hash33(i + g) * 0.7 + 0.15;
                best = min(best, length(g + o - f));
            }
    return best;
}
vec3 rotY(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z); }
vec3 rotX(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z); }

void main() {
    vec2 q = vL;
    int kind = int(vB.x + 0.5);
    float seed = vB.y, t = vB.z, blur = vC.x, cil = vC.y;
    AA = max(length(fwidth(q)) * 0.7, 1e-4) + blur * 0.03;
    float d = 1.0;        // the body's outline (negative inside)
    float org = 0.0;      // organelles
    float lines = 0.0;    // cilia, striae
    float gran = 0.0;     // granules
    float fillK = 1.0;    // how dense the body is
    float inner = 9.0;    // an inner contour (a diatom's double wall)
    if (kind == 0) {
        // paramecium: a slipper, a groove, a fringe of cilia beating in waves, its nucleus and vacuoles
        float a = 0.47;
        float bx = 0.15 * (1.0 + 0.14 * q.x / a) * (1.0 + 0.05 * sin(t * 2.1 + seed * 6.0));
        d = (length(vec2(q.x / a, q.y / bx)) - 1.0) * bx;
        d += 0.032 * exp(-pow((q.x - 0.07) / 0.13, 2.0)) * smoothstep(-0.03, 0.07, q.y);
        float s = atan(q.y / bx, q.x / a);
        float hairL = 0.04 * (0.85 + 0.7 * cil);
        float bend = sin(s * 5.0 + t * (4.0 + 6.0 * cil)) * (0.35 + 1.2 * cil);
        float st = abs(fract(s * 30.0 / 6.2831853 + d * bend * 7.0) - 0.5);
        lines = (1.0 - smoothstep(0.1, 0.25, st)) * smoothstep(-0.004, 0.006, d) * (1.0 - smoothstep(0.0, hairL, d)) * 0.8;
        float nuc = 1.0 - smoothstep(1.0 - AA * 12.0, 1.0 + AA * 12.0, length((q - vec2(-0.03, -0.012)) / vec2(0.11, 0.055)));
        float cv = 0.028 * (0.55 + 0.45 * sin(t * 1.4 + seed * 9.0));
        org = nuc * 0.75 + ring(q, vec2(0.3, 0.0), cv, 0.008) * 0.9 + ring(q, vec2(-0.3, 0.01), cv * 0.9, 0.008) * 0.9;
        for (int i = 0; i < 5; i++) {
            float hx = hash12(vec2(seed * 31.0, float(i) * 7.0)), hy = hash12(vec2(float(i) * 3.0, seed * 17.0));
            vec2 c = vec2(-0.32 + 0.6 * hx, (hy - 0.5) * 0.12);
            org += ring(q, c, 0.018 + 0.012 * hy, 0.006) * 0.6;
        }
        gran = vnoise(q * 70.0 + seed * 13.0) * 0.5;
        fillK = 0.8;
    } else if (kind == 1) {
        // amoeba: a slow blob reaching out with pseudopods, granular streaming, a nucleus and a vacuole
        float th = atan(q.y, q.x), r = length(q);
        float tp = 0.5 * sin(t * 0.13 + seed * 4.0), tp2 = 2.4 + 0.7 * sin(t * 0.09 + seed * 2.0);
        float R = 0.25 + 0.03 * sin(2.0 * th + t * 0.21 + seed * 5.0) + 0.026 * sin(3.0 * th - t * 0.17 + seed * 3.0) +
                  0.016 * sin(5.0 * th + t * 0.33 + seed) + 0.15 * pow(max(cos(th - tp), 0.0), 9.0) +
                  0.08 * pow(max(cos(th - tp2), 0.0), 14.0) + 0.06 * pow(max(cos(th + tp2), 0.0), 16.0);
        d = (r - R) * 0.85;
        vec2 nc = vec2(-0.05, 0.03);
        org = ring(q, nc, 0.06, 0.009) * 0.9 + disc(q, nc, 0.05) * 0.35 + disc(q, nc + vec2(0.01, 0.005), 0.016) * 0.6;
        float cv = 0.04 * (0.5 + 0.5 * sin(t * 0.9 + seed * 7.0));
        org += ring(q, vec2(-0.14, -0.08), cv, 0.007) * 0.8;
        float endo = smoothstep(-0.03, -0.07, d);
        gran = (vnoise(q * 60.0 + vec2(t * 0.6, 0.0) + seed * 11.0) * 0.7 + vnoise(q * 140.0 + seed) * 0.3) * endo;
        fillK = 0.7;
    } else if (kind == 2) {
        // pennate diatom: a glass boat, its double wall, fine transverse striae, the raphe and its central nodule
        float a = 0.5, b = 0.11;
        float Rv = (a * a + b * b) / (2.0 * b), c = Rv - b;
        d = max(length(q - vec2(0.0, -c)), length(q - vec2(0.0, c))) - Rv;
        d = d * 0.9 + 0.004;
        inner = abs(d + 0.022) - 0.004;
        float st = abs(fract(q.x * 34.0) - 0.5);
        float inside = smoothstep(-0.02, -0.03, d);
        lines = (1.0 - smoothstep(0.12, 0.22, st)) * inside * smoothstep(0.012, 0.02, abs(q.y)) * 0.55;
        float raphe = (1.0 - smoothstep(0.004, 0.004 + AA, abs(q.y))) * step(abs(q.x), a - 0.07) * step(0.035, abs(q.x));
        org = raphe * 0.8 + disc(q, vec2(0.0), 0.03) * 0.8 + disc(q, vec2(a - 0.09, 0.0), 0.014) * 0.6 + disc(q, vec2(-a + 0.09, 0.0), 0.014) * 0.6;
        // chloroplasts: two soft golden bands along the sides
        gran = smoothstep(0.03, 0.06, abs(q.y)) * smoothstep(-0.02, -0.04, d) * step(abs(q.x), 0.3) * 0.5;
        fillK = 0.45;
    } else if (kind == 3) {
        // centric diatom: a glass disc of radial striae, a ring of pores, a central rosette, turning slowly
        float r = length(q);
        float R = 0.36;
        d = r - R;
        inner = abs(r - (R - 0.03)) - 0.004;
        float th = atan(q.y, q.x) + vD.x;
        float st = abs(fract(th * 44.0 / 6.2831853) - 0.5);
        lines = (1.0 - smoothstep(0.1, 0.2, st)) * smoothstep(0.08, 0.11, r) * smoothstep(R - 0.035, R - 0.05, r) * 0.55;
        float pores = abs(fract(th * 22.0 / 6.2831853) - 0.5);
        org = ring(q, vec2(0.0), 0.2, 0.006) * 0.7 + (1.0 - smoothstep(0.15, 0.3, pores)) * ring(q, vec2(0.0), 0.255, 0.012) * 0.6;
        org += ring(q, vec2(0.0), 0.06, 0.008) * 0.8 + disc(q, vec2(0.0), 0.025) * 0.5;
        gran = 0.25 * smoothstep(R - 0.03, R - 0.1, r);
        fillK = 0.4;
    } else if (kind == 4) {
        // rotifer: a telescoping body on a forked foot, a head crowned with two wheels of cilia, its jaws
        vec2 p = q;
        p.x = (p.x - 0.05) / (1.0 + 0.06 * sin(t * 0.7 + seed * 5.0)) + 0.05;
        float body = sdRoundCone(p, vec2(-0.42, 0.0), vec2(0.04, 0.0), 0.035, 0.15);
        float neck = sdRoundCone(p, vec2(0.04, 0.0), vec2(0.25, 0.0), 0.14, 0.095);
        vec2 w1 = vec2(0.32, 0.07), w2 = vec2(0.32, -0.07);
        float wheels = min(length(p - w1), length(p - w2)) - 0.062;
        float toes = min(sdSeg(p, vec2(-0.43, 0.0), vec2(-0.52, 0.035)), sdSeg(p, vec2(-0.43, 0.0), vec2(-0.52, -0.035))) - 0.012;
        d = smin(smin(body, neck, 0.05), wheels, 0.03);
        d = min(d, toes);
        // the corona's cilia: hairs turning around each wheel, on its front
        float spin = t * (2.0 + 5.0 * cil);
        for (int i = 0; i < 2; i++) {
            vec2 c = i == 0 ? w1 : w2;
            vec2 v = p - c;
            float rr = length(v) - 0.062;
            float an = atan(v.y, v.x) * (i == 0 ? 1.0 : -1.0) + spin;
            float st = abs(fract(an * 16.0 / 6.2831853 + rr * 6.0) - 0.5);
            lines += (1.0 - smoothstep(0.1, 0.24, st)) * smoothstep(-0.004, 0.006, rr) * (1.0 - smoothstep(0.0, 0.035 + 0.02 * cil, rr)) *
                     smoothstep(-0.02, 0.02, v.x + 0.01) * 0.8;
        }
        float jaw = 0.035 + 0.006 * sin(t * 6.0 * (0.3 + cil));
        org = disc(p, vec2(0.15, 0.0), jaw) * 0.7 + ring(p, vec2(0.15, 0.0), jaw + 0.012, 0.006) * 0.6;
        org += ring(p, vec2(-0.08, 0.0), 0.07, 0.008) * 0.35;
        gran = vnoise(p * 50.0 + seed * 7.0) * smoothstep(-0.02, -0.06, body) * 0.5;
        fillK = 0.65;
    } else {
        // volvox: a rolling glass sphere of tiny green cells, daughter colonies inside
        float r = length(q);
        float R = 0.42;
        d = r - R;
        if (r < R + 0.02) {
            float z = sqrt(max(0.0, 1.0 - min(1.0, r * r / (R * R))));
            vec3 P = vec3(q / R, z);
            float ra = t * 0.25 + seed * 6.0;
            vec3 Pr = rotX(rotY(P, ra), 0.6 + seed);
            float c1 = cells3(Pr * 6.5 + seed * 10.0);
            float rim = smoothstep(0.55, 1.0, r / R);
            lines = (1.0 - smoothstep(0.12, 0.2, c1)) * (0.45 + 0.55 * rim) * step(r, R);
            for (int i = 0; i < 4; i++) {
                vec3 h = hash33(vec3(seed * 13.0, float(i), 3.7)) * 2.0 - 1.0;
                vec3 cpos = normalize(h + 0.001) * 0.45;
                cpos = rotX(rotY(cpos, -ra), -(0.6 + seed));
                float cr = 0.075 + 0.04 * fract(h.x * 7.3 + 0.5);
                vec2 cc = cpos.xy * R;
                org += ring(q, cc, cr, 0.008) * 0.8 + disc(q, cc, cr) * 0.35;
            }
        }
        gran = 0.15;
        fillK = 0.35;
    }
    // the shading
    float ew = AA + blur * 0.04;
    float inside = 1.0 - smoothstep(-ew, ew, d);
    float rimw = 0.01 + blur * 0.05 + 0.012 * vD.y;
    float rim = max(exp(-abs(d) / rimw) - 0.01, 0.0) * (1.0 - smoothstep(0.05, 0.16, d));
    float innerL = inner < 8.0 ? (1.0 - smoothstep(-AA, AA, inner)) : 0.0;
    float sharp = 1.0 - 0.75 * blur;
    lines *= sharp * sharp;
    org *= inside * (1.0 - 0.6 * blur);
    gran *= inside * sharp;
    innerL *= sharp;
    // in its cell (and while it leaves it): one bold, readable shape, its fine details faded out
    float simple = vD.y;
    lines *= 1.0 - 0.8 * simple;
    gran *= 1.0 - simple;
    org *= 1.0 - 0.55 * simple;
    innerL *= 1.0 - 0.5 * simple;
    float k = stainK(vWorld);
    int type = int(vC.w + 0.5);
    vec3 tint = pieceCol(type, k);
    vec3 body = mix(mix(uOrgA[0], uOrgB[0], k), tint, vC.z);
    vec3 rimc = mix(mix(uOrgA[1], uOrgB[1], k), tint, vC.z * 0.6);
    vec3 orgc = mix(mix(uOrgA[2], uOrgB[2], k), tint, vC.z * 0.5);
    if (kind == 5) { body = mix(body, mix(body, vec3(0.08, 0.5, 0.12), 0.5), 1.0 - uDark); }
    // dark-field: light scattered by the edges and the structures, on black
    vec3 df = silver(rimc) * (rim * (0.55 + 0.5 * simple) + innerL * 0.35 + lines * 0.4) +
              silver(body) * inside * (fillK * 0.05 + gran * 0.16 + 0.2 * simple) +
              silver(orgc) * org * 0.32;
    df *= uGlow * (1.0 - 0.45 * blur);
    // bright-field: a stained body absorbing the light, its edge a darker line, a pale halo just outside
    float A = inside * (0.22 * fillK + 0.4 * gran + 0.25 * simple) + rim * (0.75 + 0.25 * simple) + innerL * 0.55 + lines * 0.6 + org * 0.7;
    A = clamp(A * (1.0 - 0.45 * blur), 0.0, 0.9);
    vec3 dye = mix(body, orgc, clamp(org, 0.0, 1.0) * 0.6) * 0.75;
    float halo = exp(-max(d, 0.0) / (0.02 + blur * 0.05)) * step(0.0, d) * 0.06;
    vec4 bf = vec4(dye * A + vec3(halo), A);
    // nothing reaches the quad's edge
    float win = 1.0 - smoothstep(0.56, 0.69, max(abs(q.x), abs(q.y)));
    vec4 o = mix(bf, vec4(df, 0.0), uDark) * vB.w * win;
    if (o.r + o.g + o.b + o.a <= 0.0005) discard;
    frag = o;
}
)";

// ---------------------------------------------------------------- the pieces (instanced per cell)
// a: x, y (cell center), half (board units), kind (0 cell, 1 ghost, 2 bursting, 3 dying). b: type, seed, progress,
// alpha. c: nucleus x, y, lock flash, -. d: stain position x (previews), y, -, -.
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
uniform float uGlow;
uniform float uTime, uLife;   // the world's clock; how lively the cells are (calm .. peak)
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    int type = int(vB.x + 0.5);
    float seed = vB.y, t = vB.z;
    float aa = max(length(fwidth(p)) * 0.75, 1e-4);
    const float GAP = 0.05, RC = 0.1;
    float swell = 1.0, rc = RC;
    if (kind == 2) { swell = 1.0 + 0.14 * t; rc = mix(RC, 0.4, smoothstep(0.0, 0.7, t)); }
    vec2 hb = vec2(0.5 - GAP);
    if (uOrganic > 0.5) rc = mix(0.17, 0.4, kind == 2 ? smoothstep(0.0, 0.7, t) : 0.0);
    float d = (sdBox(p / swell, hb - rc) - rc) * swell;
    if (d > aa) discard;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    float in_ = max(-d, 0.0);
    float k = stainK(vD.xy + p);
    vec3 c = pieceCol(type, k);
    // the wall, the cytoplasm inside it, the nucleus
    float wallW = 0.055;
    float wall = 1.0 - smoothstep(wallW - aa, wallW + aa, in_);
    float proto = 1.0;    // the protoplast (shrinks away from the wall as the cell dies)
    if (kind == 3) {
        float s = 1.0 - 0.42 * smoothstep(0.0, 0.7, t);
        float r2 = mix(0.12, 0.26, smoothstep(0.0, 0.7, t));
        float dp = sdBox(p, (hb - wallW) * s - r2) - r2;
        proto = 1.0 - smoothstep(-aa, aa, dp);
    }
    // A calm stained cell: an even tint (a touch denser along its wall), the glassy wall and its fine bright line; no
    // nucleus, no granules. A cell with life in it is a clear pool around its organism (drawn over it), its wall
    // breathing a little.
    float live = vC.w;
    if (kind == 0 && live > 0.5) {
        float ts = uTime * (0.8 + 0.4 * fract(seed * 5.1)) + seed * 97.0;
        float wob = vnoise(vec2(atan(p.y, p.x) * 1.6 + seed * 9.0, ts * 0.18)) - 0.5;
        d += (0.012 * (wob + 0.5) + 0.003 * (1.0 + sin(ts * 0.5))) * uLife;   // only ever inward: the gap stays
        cov = 1.0 - smoothstep(-aa, aa, d);
        in_ = max(-d, 0.0);
        wall = 1.0 - smoothstep(wallW - aa, wallW + aa, in_);
    }
    float flash = vC.z;
    if (kind == 1) {
        // the ghost: the wall alone, faint
        float l = wall * cov;
        vec3 gb = mix(c * 0.55, silver(c) * 0.5, uDark);
        float a = l * mix(0.42, 0.55, uDark) * vB.w;
        frag = vec4(gb * a, a);
        return;
    }
    float inner = exp(-max(in_ - wallW, 0.0) / 0.025) * step(wallW, in_);
    float tl = smoothstep(-0.5, 0.5, -p.x * 0.6 + p.y * 0.8);
    vec3 field = uFieldA[0];
    vec3 col;
    if (uOrganic > 0.5) {
        // SPECIMEN: a plump, glossy lump of tissue, its fine cells showing through the skin
        vec2 q = p * 4.6 + vec2(seed * 31.0, seed * 17.0);
        vec2 ip = floor(q), fp = fract(q);
        float d1 = 8.0, d2 = 8.0, hn = 0.0;
        vec2 cc = vec2(0.0);
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                vec2 g = vec2(i, j);
                vec3 h = hash33(vec3(ip + g, 7.0));
                vec2 o = g + 0.15 + 0.7 * h.xy - fp;
                float l = dot(o, o);
                if (l < d1) { d2 = d1; d1 = l; hn = h.z; cc = o; }
                else if (l < d2) d2 = l;
            }
        float edgeC = sqrt(d2) - sqrt(d1);
        const float e = 0.01;
        vec2 gd = vec2(sdBox(p + vec2(e, 0.0), hb - rc) - sdBox(p - vec2(e, 0.0), hb - rc),
                       sdBox(p + vec2(0.0, e), hb - rc) - sdBox(p - vec2(0.0, e), hb - rc));
        vec2 ng = length(gd) > 1e-5 ? normalize(gd) : vec2(0.0);
        vec2 L = normalize(vec2(-0.6, 0.8));
        float dome = smoothstep(0.0, 0.17, in_);
        float rim = 1.0 - dome;
        vec3 o = c * (0.72 + 0.22 * dome) * (1.0 + 0.4 * rim * dot(ng, L)) * (1.0 + 0.16 * dot(p, L));
        o *= 0.93 + 0.12 * hn;
        float px = aa * 4.6;
        o *= 1.0 - 0.2 * (1.0 - smoothstep(0.03, 0.03 + px * 2.0, edgeC)) * dome;
        o *= 1.0 - 0.16 * (1.0 - smoothstep(0.1, 0.1 + px * 2.0, sqrt(d1))) * step(0.5, hn) * dome;
        o *= mix(0.5, 1.0, smoothstep(0.0, 0.035, in_));
        float spec = pow(clamp(1.0 - length((p - vec2(-0.15, 0.17)) / vec2(0.2, 0.13)), 0.0, 1.0), 2.0);
        o += mix(c, vec3(1.0), 0.7) * (spec * 0.32 * dome + 0.22 * exp(-in_ / 0.018) * max(dot(ng, L), 0.0));
        o = mix(o, vec3(1.0), flash * 0.3);
        col = silver(o * uGlow);
        if (kind == 3) col *= mix(0.8, 1.0, proto);
    } else {
    // the pool: clear in the middle, the stain fading in toward the wall
    float pool = live * smoothstep(0.1, 0.24, in_);
    // bright-field: the stain, a little denser along the wall, a darker wall
    float dens = 0.8 + 0.15 * exp(-in_ / 0.14);
    vec3 bf = mix(field, c, dens) * mix(0.8, 1.0, proto);
    bf = mix(bf, mix(field, c, 0.28), pool * 0.85 * proto);
    bf = mix(bf, vec3(1.0), inner * (0.1 + 0.25 * tl) * proto);
    bf = mix(bf, c * (0.4 - 0.12 * flash), wall);
    // dark-field: the wall glows, the stain a soft glow fading inward (darker in a pool)
    vec3 gcol = silver(c);
    vec3 df = gcol * (0.11 + exp(-in_ / 0.1) * 0.35) * proto;
    df *= 1.0 - 0.55 * pool;
    df += gcol * inner * (0.15 + 0.3 * tl) * proto;
    df = mix(df, gcol * (1.2 + 0.6 * flash), wall);
    df *= uGlow;
    col = mix(bf, df, uDark);
    }
    float a = cov * vB.w;
    if (kind == 2) {
        // bursting: the wall breaks open, the cell rounds and its content disperses
        float brk = vnoise(vec2(atan(p.y, p.x) * 3.0, seed * 9.0));
        a *= (1.0 - smoothstep(0.0, 1.0, t)) * mix(1.0, step(t * 1.3, brk + 0.2), wall);
        col = mix(col, mix(col, field, 0.5), t * (1.0 - uDark));
    }
    if (kind == 3) {
        // dying: the stain drains, the protoplast shrinks away from the wall, then the cell fades
        float L = lumOf(col);
        col = mix(col, vec3(L), smoothstep(0.0, 0.8, t) * 0.85);
        a *= mix(1.0, wall + proto, smoothstep(0.1, 0.5, t)) * (1.0 - smoothstep(0.6, 1.0, t));
        a = min(a, 1.0);
    }
    if (a <= 0.002) discard;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- SPECIMEN's loupes (instanced)
// a: x, y, radius, mode (0 the view with its bezel and shadow, 1 the bezel alone, over the organism). b: piece type,
// seed, alpha, a pool (the cell's organism lives in it). c: pan x, y (the slide drifting under the loupe), -, -.
inline const char* LENS_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC;
void main() {
    vL = aPos * 1.3;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(aA.xy + vL * aA.z, 0.0, 1.0);
}
)";
inline const char* LENS_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB, vC;
out vec4 frag;
uniform float uTime;
void main() {
    vec2 p = vL;
    float r = length(p);
    float aa = max(fwidth(r), 1e-4);
    int mode = int(vA.w + 0.5);
    int type = int(vB.x + 0.5);
    float seed = vB.y, alpha = vB.z, live = vB.w;
    const float R0 = 0.955, R1 = 1.04;   // the bezel
    float bez = smoothstep(R0 - aa, R0 + aa, r) * (1.0 - smoothstep(R1 - aa, R1 + aa, r));
    vec3 bc = mix(vec3(0.05, 0.055, 0.06), vec3(0.22, 0.23, 0.24), smoothstep(0.3, -0.6, p.y / max(r, 1e-3)));
    bc += vec3(0.5) * exp(-abs(r - R0 - 0.012) / 0.006) * 0.35;
    if (mode == 1) {
        float a = bez * alpha;
        if (a <= 0.002) discard;
        frag = vec4(silver(bc) * a, a);
        return;
    }
    // the shadow it casts on the bench
    float sh = (1.0 - smoothstep(R1, 1.28, length(p + vec2(-0.03, 0.06)))) * 0.55;
    vec3 col = vec3(0.0);
    float a = sh;
    if (r < R0 + aa) {
        // the slide under the loupe: the block's tissue, magnified, in bright-field
        vec2 w = vA.xy + p * vA.z;
        float k = stainK(w);
        vec3 c = pieceCol(type, k), field = mix(uFieldA[0], uFieldB[0], k);
        vec2 q = p * 2.6 + vC.xy + vec2(seed * 53.0, seed * 29.0);
        vec2 ip = floor(q), fp = fract(q);
        float d1 = 8.0, d2 = 8.0, hn = 0.0;
        vec2 cc = vec2(0.0);
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                vec2 g = vec2(i, j);
                vec3 h = hash33(vec3(ip + g, 3.0));
                vec2 o = g + 0.2 + 0.6 * h.xy - fp;
                float l = dot(o, o);
                if (l < d1) { d2 = d1; d1 = l; hn = h.z; cc = o; }
                else if (l < d2) d2 = l;
            }
        float edge = (sqrt(d2) - sqrt(d1)) * 0.5;   // to the cell's wall (cells about 0.8 across)
        float px = aa * 2.6;
        // cytoplasm: a light stain, denser toward the wall
        vec3 t = mix(field, c, 0.2 + 0.1 * hn);
        t = mix(t, mix(field, c, 0.55), exp(-edge / 0.07));
        // plastids streaming slowly along the wall
        float ang = atan(-cc.y, -cc.x);
        float u = ang / 6.2832 * 9.0 + uTime * 0.05 * (hn < 0.5 ? 1.0 : -1.0) + hn * 7.0;
        float du = (fract(u) - 0.5) * 0.6, dv = edge - 0.075;
        float gr = (1.0 - smoothstep(0.03 - px, 0.03 + px, length(vec2(du * length(cc) * 0.7, dv)))) * step(0.25, fract(u * 0.37 + hn * 3.0));
        t = mix(t, mix(field, c, 0.9) * 0.75, gr * 0.85);
        // the nucleus, in most cells, with its nucleolus
        vec2 nc = cc + vec2(hn - 0.5, fract(hn * 7.0) - 0.5) * 0.25;
        float nd = length(nc) - 0.13;
        float nuc = (1.0 - smoothstep(-px, px, nd)) * step(0.25, hn);
        t = mix(t, mix(field, c, 0.7) * 0.8, nuc * 0.9);
        t = mix(t, c * 0.35, (1.0 - smoothstep(-px, px, length(nc + vec2(0.03, -0.02)) - 0.04)) * step(0.25, hn) * 0.9);
        t = mix(t, mix(field, c, 0.95) * 0.65, (1.0 - smoothstep(0.0, px * 1.5, abs(nd) - 0.008)) * step(0.25, hn) * 0.6);
        // the walls: a fine double line
        float wl = 1.0 - smoothstep(0.012, 0.012 + px * 1.5, edge);
        t = mix(t, c * 0.42, wl);
        // the organism's pool: a clear, rounded cell in the middle (the organism is drawn over it)
        float pr = length(p) - 0.62 - 0.03 * sin(atan(p.y, p.x) * 3.0 + seed * 9.0 + uTime * 0.2);
        float pool = live * (1.0 - smoothstep(-px, px, pr));
        t = mix(t, mix(field, c, 0.1), pool);
        t = mix(t, c * 0.42, live * (1.0 - smoothstep(0.0, px * 1.5, abs(pr) - 0.014)));
        t = mix(t, mix(field, c, 0.4), live * exp(-max(-pr, 0.0) / 0.05) * step(pr, 0.0) * 0.5);
        // the loupe's edge: a darker vignette and a faint fringe
        t *= mix(1.0, 0.7, smoothstep(0.55, R0, r));
        t += vec3(0.25, 0.35, 0.7) * exp(-abs(r - R0 + 0.03) / 0.02) * 0.06;
        float inA = 1.0 - smoothstep(R0 - aa, R0 + aa, r);
        col = silver(t) * inA;
        a = mix(a, 1.0, inA);
    }
    col = mix(col, silver(bc), bez);
    a = max(a, bez);
    a *= alpha;
    if (a <= 0.002) discard;
    frag = vec4(col * alpha, a);
}
)";
// A fine line from a loupe to the block it looks into (a: from, to; b: width, alpha), in world units.
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
uniform float uAA;
out vec2 vS;
flat out float vLen, vW, vAl;
void main() {
    vec2 a = aA.xy, b = aA.zw, d = b - a;
    float len = max(length(d), 1e-4);
    vec2 t = d / len, n = vec2(-t.y, t.x);
    float hw = aB.x * 0.5 + uAA * 1.5;
    vec2 s = vec2((aPos.x * 0.5 + 0.5) * (len + 2.0 * hw) - hw, aPos.y * hw);
    vS = s;
    vLen = len; vW = aB.x; vAl = aB.y;
    gl_Position = uVP * vec4(a + t * s.x + n * s.y, 0.0, 1.0);
}
)";
inline const char* LINE_FS = R"(#version 330 core
in vec2 vS;
flat in float vLen, vW, vAl;
out vec4 frag;
uniform float uAA;
uniform vec3 uCol;
void main() {
    float d = length(vec2(vS.x - clamp(vS.x, 0.0, vLen), vS.y)) - vW * 0.5;
    float a = (1.0 - smoothstep(-uAA * 0.7, uAA * 0.7, d)) * vAl;
    if (a <= 0.002) discard;
    frag = vec4(uCol * a, a);
}
)";

// ---------------------------------------------------------------- fine debris (a hard drop)
inline const char* SPECK_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
void main() {
    float r = length(vLocal) / 0.5;
    float a = exp(-r * r * 4.0) * vB.w;
    if (a <= 0.002) discard;
    vec3 c = mix(uOrgA[2] * 0.5, silver(uOrgA[1]) * 0.7, uDark);
    frag = vec4(c * a, a * (1.0 - uDark));
}
)";

// ---------------------------------------------------------------- the glow
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb +
             texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    frag = vec4(max(c - vec3(0.06), 0.0), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    vec4 c = texture(uSrc, vUv) * w[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)) + texture(uSrc, vUv - uDir * float(i))) * w[i];
    frag = c;
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform vec4 uView;
uniform vec2 uDisk;
uniform float uBloomK, uDim, uFade, uBridge, uDarkF;
uniform vec3 uBridgeCol, uBridgeInk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // the eyepiece: the round field on black, its edge soft with a faint colored fringe
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float r = length(w - vec2(0.0, uDisk.x)) - uDisk.y;
    float m = 1.0 - smoothstep(-0.5, 0.12, r);
    vec3 fringe = vec3(0.35, 0.5, 0.9) * exp(-abs(r + 0.25) / 0.16) * 0.05 * (1.0 - uDarkF * 0.7);
    c = c * m + fringe * m;
    // a soft knee in dark-field: the glowing walls never burn to white
    c = mix(c, c / (1.0 + 0.35 * max(max(c.r, c.g), c.b)), uDarkF);
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
// The ink depends on the pixel: dark inside a pale field, pale on the black around it (uMode 1: ink, 2: accent;
// 0: the vertex color).
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
uniform vec3 uDiskPx;          // the field's center (pixels), radius
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
    float halo = smoothstep(0.22, 0.5, d) * uHaloA;
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

#pragma once
// MICROSCOPE's shaders (see microscope.cpp).
//
// FIELD_FS paints the field of view (the whole screen: a pale, softly vignetted field of light, its illumination a
// little uneven, out-of-focus haze, small round cells and bubbles ringed by phase halos drifting in the medium, fine
// debris, the counting chamber's etched lines around the well); ORG_FS the organisms wandering on the sides (one
// signed distance per kind: paramecium, amoeba, two diatoms, rotifer, volvox), translucent bodies with a darker rim,
// a pale phase halo and fine dark cilia, blurred when out of focus; PIECE_FS the pieces (each block a living cell:
// translucent stained cytoplasm in a darker membrane, a nucleus, the cells of a piece joined into one colony with
// thin walls between them, a phase halo around it; until a drop, livingCell: the colony one living body, its cells
// merged as metaballs; a cleared cell's burst: it swells round, lets go of its neighbors and breaks into droplets of
// itself, budding off and drifting out, its contents carried along); SPECK_FS loose granules; DOWN_FS / BLUR_FS a soft blur (the
// dark-field glow, the out-of-focus slide) and FINAL_FS the frame (the lamp, the focus).
// Each color comes from the variant's illumination and the drop's, blended by uMix; uDark shades in dark-field
// (light scattered on black) rather than bright-field (stains on a lit field).
#include <string>

namespace microscope_shaders {

inline const char* COMMON = R"(
uniform vec3 uFieldA[2], uFieldB[2];   // field center, edge: the variant's illumination, the drop's
uniform vec3 uPieceA[7], uPieceB[7];
uniform vec3 uOrgA[3], uOrgB[3];       // organisms: body, rim, organelles (in dark-field: their glows)
uniform float uDropK;                  // the drop's look, 0..1: it sweeps out from the well over the field
uniform vec2 uDarkAB, uPhaseAB;        // dark-field (1) or not, the phase halos' strength: own, drop's
float kMix, kDark, kPhase;             // the look here (lightAt)
void lightAt(vec2 w) {
    float r = length(w * vec2(0.8, 1.0)) / 20.0;
    kMix = smoothstep(0.0, 1.0, clamp(uDropK * 1.8 - r * 0.8, 0.0, 1.0));
    kDark = mix(uDarkAB.x, uDarkAB.y, kMix);
    kPhase = mix(uPhaseAB.x, uPhaseAB.y, kMix);
}
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
float lumOf(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
vec3 fieldCol(int i) { return mix(uFieldA[i], uFieldB[i], kMix); }
vec3 pieceCol(int t) { return mix(uPieceA[t], uPieceB[t], kMix); }
vec3 orgCol(int i) { return mix(uOrgA[i], uOrgB[i], kMix); }
// premultiplied "over": c at alpha a laid over acc
void over(inout vec4 acc, vec3 c, float a) { a = clamp(a, 0.0, 1.0); acc = acc * (1.0 - a) + vec4(c * a, a); }
)";

// ---------------------------------------------------------------- the field of view (full screen)
inline const char* FIELD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform float uTime, uIllum, uSwell, uSeed, uAA, uWell;
// A round body of the medium (a small cell, a bubble) at c, radius r, out of focus by b: its phase-contrast look
// (a dark ring, a pale halo just outside, a faintly lighter inside) added to sh (x: darkening, y: lightening,
// z: the ring's glow in dark-field).
void roundBody(vec2 w, vec2 c, float r, float b, float win, inout vec3 sh) {
    float d = length(w - c) - r;
    float wd = 0.018 + 0.01 * r + b * 0.07;
    float ring = exp(-pow(d / wd, 2.0)) * (1.0 - 0.6 * b);
    float halo = exp(-pow((d - wd * 2.0) / (wd * 1.4 + 0.02), 2.0)) * (1.0 - 0.5 * b);
    float core = (1.0 - smoothstep(-wd, wd, d)) * exp(-pow(length(w - c) / max(r, 0.05), 2.0) * 1.5);
    sh.x += (ring * 0.26 + core * 0.03) * win;
    sh.y += (halo * 0.4 + core * 0.1) * kPhase * win;
    sh.z += (ring + core * 0.15) * win;
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    lightAt(w);
    vec2 sn = vUv * 2.0 - 1.0;
    float r2 = dot(sn * vec2(1.0, 0.92), sn * vec2(1.0, 0.92));
    vec3 fc = fieldCol(0), fe = fieldCol(1);
    vec3 base = mix(fc, fe, smoothstep(0.05, 1.7, r2));
    // the places kept clear: the well, the HUD's columns
    float well = 1.0 - smoothstep(-0.6, 0.9, sdBox(w - vec2(0.0, 0.4), vec2(5.4, 10.8)));
    float quiet = max(well, 0.6 * (1.0 - smoothstep(-0.8, 0.8, sdBox(w - vec2(-8.6, 1.8), vec2(2.6, 8.6)))));
    quiet = max(quiet, 0.6 * (1.0 - smoothstep(-0.8, 0.8, sdBox(w - vec2(8.6, 3.4), vec2(2.6, 7.0)))));
    // the lamp's light, a little uneven; a soft haze of things out of focus drifting through
    float n1 = fbm(w * 0.045 + vec2(uSeed, uTime * 0.003));
    float n2 = fbm(w * 0.16 + vec2(uTime * 0.03, uSeed * 1.3 - uTime * 0.01));
    float haze = smoothstep(0.52, 0.82, n2) * (1.0 - quiet);
    vec3 sh = vec3(0.0);
    // small round cells and bubbles drifting in the medium (two sizes), and fine debris
    vec2 flow = vec2(uTime * 0.09, uTime * 0.03);
    for (int L = 0; L < 3; L++) {
        float S = L == 0 ? 4.2 : (L == 1 ? 2.6 : 1.3);
        float dens = L == 0 ? 0.19 : (L == 1 ? 0.15 : 0.22);
        vec2 g = (w + flow * (0.6 + 0.4 * float(L)) + vec2(float(L) * 17.3 + uSeed, float(L) * 5.1)) / S;
        vec2 ci = floor(g);
        float h = hash12(ci + vec2(uSeed * 3.1, float(L) * 7.7));
        if (h > dens) continue;
        vec2 jit = vec2(hash12(ci * 1.7 + 3.1 + float(L)), hash12(ci * 2.3 + 7.7 - float(L)));
        float tt = uTime * (0.45 + 0.5 * h);
        vec2 c = (ci + 0.4 + 0.2 * jit + 0.05 * vec2(sin(tt + h * 40.0), cos(tt * 0.8 + h * 23.0))) * S;
        c -= flow * (0.6 + 0.4 * float(L)) + vec2(float(L) * 17.3 + uSeed, float(L) * 5.1);
        float hr = fract(h * 31.7);
        if (L == 2) {
            // a speck of debris: a tiny dark grain, its pale halo
            float d = length(w - c) - (0.025 + 0.035 * hr);
            sh.x += (1.0 - smoothstep(-0.02, 0.03, d)) * 0.22;
            sh.y += exp(-pow((d - 0.04) / 0.035, 2.0)) * 0.25 * kPhase;
            sh.z += (1.0 - smoothstep(-0.02, 0.04, d)) * 0.6;
            continue;
        }
        float r = S * (L == 0 ? 0.035 + 0.075 * hr : 0.025 + 0.045 * hr);
        float b = smoothstep(0.45, 1.0, fract(h * 57.3));   // some out of focus
        float win = 1.0 - smoothstep(0.18, 0.28, length(w - c) / S);   // (kept inside its grid cell)
        roundBody(w, c, r, b, win, sh);
    }
    sh *= 1.0 - quiet;
    // bright-field: the lit field, darkened by what absorbs, lit up by the phase halos
    vec3 bf = base * (uIllum + 0.06 * (n1 - 0.5) + 0.05 * uSwell) * (1.0 - 0.045 * haze);
    bf = bf * (1.0 - sh.x) + vec3(sh.y) * 0.6 * lumOf(bf);
    // dark-field: black, a faint glow of scattered light in the middle, the edges of what floats in it glowing
    vec3 df = fc * (0.6 + 0.6 * (n1 - 0.5)) * (1.0 - 0.5 * smoothstep(0.1, 1.6, r2)) * (1.0 + 0.6 * uSwell);
    df += orgCol(1) * (sh.z * 0.07 + haze * 0.012);
    vec3 c = mix(bf, df, kDark);
    // the well: a faintly clearer chamber, its walls and floor etched in pairs of fine lines (a counting chamber)
    c = mix(c, c * mix(1.03, 0.85, kDark), well * uWell);
    float fade = 1.0 - smoothstep(5.0, 10.6, w.y);
    float la = 0.0;
    for (int i = 0; i < 2; i++) {
        float off = 5.08 + 0.13 * float(i);
        float wl = abs(abs(w.x) - off) - 0.008 * (1.0 - float(i) * 0.4);
        float a = (1.0 - smoothstep(-uAA, uAA, wl)) * step(-10.1 - 0.13 * float(i), w.y) * fade;
        float fl = abs(w.y + off + 4.92) - 0.008 * (1.0 - float(i) * 0.4);
        a = max(a, (1.0 - smoothstep(-uAA, uAA, fl)) * step(abs(w.x), off));
        la = max(la, a * (i == 0 ? 1.0 : 0.55));
    }
    // the chamber's fine ruling: a faint line every row and column
    vec2 gr = abs(fract(w + 0.5) - 0.5);
    float rule = (1.0 - smoothstep(0.0, uAA * 1.2, min(gr.x, gr.y) - 0.003)) * step(abs(w.x), 5.0) * step(-10.0, w.y) * fade;
    la = max(la, rule * mix(0.12, 0.025, kDark));
    la *= uWell;
    vec3 ink = mix(fe * 0.42, orgCol(1) * 0.16, kDark);
    c = mix(c, ink, la * 0.6);
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the organisms (instanced)
// a: x, y, size, angle. b: kind, seed, time, alpha. c: blur, cilia beat, -, -. d: spin, -, -, -.
inline const char* ORG_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 l = aPos * 0.72;
    float c = cos(aA.w), s = sin(aA.w);
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y) * aA.z;
    vL = l;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* ORG_FS = R"(#version 330 core
in vec2 vL;
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
    lightAt(vA.xy);
    int kind = int(vB.x + 0.5);
    float seed = vB.y, t = vB.z, blur = vC.x, cil = vC.y;
    AA = max(length(fwidth(q)) * 0.7, 1e-4) + blur * 0.025;
    float d = 1.0;        // the body's outline (negative inside)
    float org = 0.0;      // organelles (nucleus...)
    float vac = 0.0;      // vacuoles: clear, ringed
    float lines = 0.0;    // cilia, striae
    float gran = 0.0;     // granules
    float fillK = 1.0;    // how dense the body is
    float inner = 9.0;    // an inner contour (a diatom's double wall)
    vec3 tint = vec3(1.0);// a body's own color (a diatom's gold, volvox's green), mixed in a little
    float tintK = 0.0;
    if (kind == 0) {
        // paramecium: a slipper tapering to its ends, the oral groove, a fringe of cilia beating in waves, the
        // macronucleus, two contractile vacuoles and food vacuoles
        float a = 0.47;
        float u = q.x / a;
        float bx = 0.15 * (1.0 + 0.12 * u) * (1.0 - 0.14 * min(u * u, 1.2)) * (1.0 + 0.04 * sin(t * 2.1 + seed * 6.0));
        vec2 e = vec2(u, q.y / bx);
        float le = length(e);
        d = (le - 1.0) * le / max(length(vec2(u / a, q.y / (bx * bx))), 1e-3);
        d += 0.03 * exp(-pow((q.x - 0.06) / 0.12, 2.0)) * smoothstep(-0.03, 0.07, q.y);
        // the cilia: evenly spaced along the outline (its arc length, integrated), short fine hairs bending in waves
        float th = abs(atan(e.y, e.x));
        float arc = 0.0, per = 0.0;
        const float bm = 0.135;
        for (int i = 0; i < 8; i++) {
            float m0 = (float(i) + 0.5) / 8.0;
            arc += sqrt(pow(a * sin(th * m0), 2.0) + pow(bm * cos(th * m0), 2.0)) * th / 8.0;
            per += sqrt(pow(a * sin(3.14159 * m0), 2.0) + pow(bm * cos(3.14159 * m0), 2.0)) * 3.14159 / 8.0;
        }
        const float NH = 30.0;   // hairs on each side
        float hi = arc / per * NH;
        float hairL = 0.042 * (0.85 + 0.4 * cil);
        float hd = clamp(d / hairL, 0.0, 1.0);
        float wave = sin(hi * 0.9 * (q.y > 0.0 ? 1.0 : -1.0) - t * (2.5 + 7.0 * cil));
        float sp = per / NH;
        float td = abs(fract(hi + wave * (0.25 + 0.5 * cil) * hd * hd) - 0.5) * sp;
        float hw = 0.0035 * (1.0 - 0.6 * hd);
        lines = (1.0 - smoothstep(hw - AA, hw + AA, td)) * smoothstep(-0.004, 0.004, d) * (1.0 - smoothstep(hairL * 0.75, hairL, d)) * 0.9;
        float nd = length((q - vec2(-0.02, -0.01)) / vec2(0.12, 0.058));
        org = 1.0 - smoothstep(1.0 - AA * 14.0, 1.0 + AA * 14.0, nd);
        org *= 0.75 + 0.25 * vnoise(q * 90.0 + seed * 5.0);
        float cv = 0.03 * (0.6 + 0.4 * sin(t * 1.4 + seed * 9.0));
        vac = ring(q, vec2(0.29, 0.01), cv, 0.007) + ring(q, vec2(-0.3, 0.0), cv * 0.9, 0.007);
        for (int i = 0; i < 6; i++) {
            float hx = hash12(vec2(seed * 31.0, float(i) * 7.0)), hy = hash12(vec2(float(i) * 3.0, seed * 17.0));
            vec2 c = vec2(-0.34 + 0.66 * hx, (hy - 0.5) * 0.13);
            vac += ring(q, c, 0.016 + 0.014 * hy, 0.006) * 0.7;
        }
        gran = vnoise(q * 80.0 + seed * 13.0) * 0.5;
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
        org = disc(q, nc, 0.055) * 0.8 + disc(q, nc + vec2(0.01, 0.005), 0.018) * 0.4;
        float cv = 0.045 * (0.5 + 0.5 * sin(t * 0.9 + seed * 7.0));
        vac = ring(q, vec2(-0.14, -0.08), cv, 0.007);
        float endo = smoothstep(-0.03, -0.07, d);
        gran = (vnoise(q * 60.0 + vec2(t * 0.6, 0.0) + seed * 11.0) * 0.7 + vnoise(q * 140.0 + seed) * 0.3) * endo;
        fillK = 0.65;
    } else if (kind == 2) {
        // pennate diatom: a glass boat, its double wall, fine transverse striae, the raphe and its central nodule
        float a = 0.5, b = 0.11;
        float Rv = (a * a + b * b) / (2.0 * b), c = Rv - b;
        d = max(length(q - vec2(0.0, -c)), length(q - vec2(0.0, c))) - Rv;
        d = d * 0.9 + 0.004;
        inner = abs(d + 0.022) - 0.004;
        float st = abs(fract(q.x * 34.0) - 0.5);
        float inside = smoothstep(-0.02, -0.03, d);
        lines = (1.0 - smoothstep(0.12, 0.22, st)) * inside * smoothstep(0.012, 0.02, abs(q.y)) * 0.5;
        float raphe = (1.0 - smoothstep(0.004, 0.004 + AA, abs(q.y))) * step(abs(q.x), a - 0.07) * step(0.035, abs(q.x));
        org = raphe * 0.8 + disc(q, vec2(0.0), 0.03) * 0.8;
        // chloroplasts: two soft golden bands along the sides
        gran = smoothstep(0.03, 0.06, abs(q.y)) * smoothstep(-0.02, -0.04, d) * step(abs(q.x), 0.3) * 0.6;
        tint = vec3(0.62, 0.48, 0.16); tintK = 0.55;
        fillK = 0.45;
    } else if (kind == 3) {
        // centric diatom: a glass disc of radial striae, a ring of pores, a central rosette, turning slowly
        float r = length(q);
        float R = 0.36;
        d = r - R;
        inner = abs(r - (R - 0.03)) - 0.004;
        float th = atan(q.y, q.x) + vD.x;
        float st = abs(fract(th * 44.0 / 6.2831853) - 0.5);
        lines = (1.0 - smoothstep(0.1, 0.2, st)) * smoothstep(0.08, 0.11, r) * smoothstep(R - 0.035, R - 0.05, r) * 0.5;
        float pores = abs(fract(th * 22.0 / 6.2831853) - 0.5);
        org = ring(q, vec2(0.0), 0.2, 0.006) * 0.7 + (1.0 - smoothstep(0.15, 0.3, pores)) * ring(q, vec2(0.0), 0.255, 0.012) * 0.6;
        org += ring(q, vec2(0.0), 0.06, 0.008) * 0.8;
        gran = 0.3 * smoothstep(R - 0.03, R - 0.1, r);
        tint = vec3(0.6, 0.5, 0.2); tintK = 0.4;
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
        org = disc(p, vec2(0.15, 0.0), jaw) * 0.7;
        vac = ring(p, vec2(0.15, 0.0), jaw + 0.012, 0.006) * 0.6 + ring(p, vec2(-0.08, 0.0), 0.07, 0.008) * 0.4;
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
            lines = (1.0 - smoothstep(0.12, 0.2, c1)) * (0.45 + 0.55 * rim) * step(r, R) * 0.8;
            for (int i = 0; i < 4; i++) {
                vec3 h = hash33(vec3(seed * 13.0, float(i), 3.7)) * 2.0 - 1.0;
                vec3 cpos = normalize(h + 0.001) * 0.45;
                cpos = rotX(rotY(cpos, -ra), -(0.6 + seed));
                float cr = 0.075 + 0.04 * fract(h.x * 7.3 + 0.5);
                vec2 cc = cpos.xy * R;
                org += disc(q, cc, cr) * 0.45;
                vac += ring(q, cc, cr, 0.008) * 0.7;
            }
        }
        gran = 0.15;
        tint = vec3(0.25, 0.5, 0.2); tintK = 0.5;
        fillK = 0.35;
    }
    // the shading
    float ew = AA + blur * 0.03;
    float inside = 1.0 - smoothstep(-ew, ew, d);
    float rimw = 0.008 + blur * 0.04;
    float rim = exp(-pow((d + rimw) / (rimw * 1.2), 2.0));
    float halo = smoothstep(-ew, ew, d) * exp(-max(d, 0.0) / (0.025 + blur * 0.04));
    float innerL = inner < 8.0 ? (1.0 - smoothstep(-AA, AA, inner)) : 0.0;
    float sharp = 1.0 - 0.8 * blur;
    lines *= sharp * sharp;
    org *= inside * (1.0 - 0.5 * blur);
    vac *= inside * sharp;
    gran *= inside * sharp;
    innerL *= sharp;
    vec3 body = mix(orgCol(0), tint * lumOf(orgCol(0)) * 2.2, tintK * (1.0 - kDark));
    vec3 rimc = orgCol(1), orgc = orgCol(2);
    // bright-field: a translucent body a little darker than the field, its edge a darker line, a pale phase halo
    // just outside; dark organelles, fine dark cilia
    vec4 bf = vec4(0.0);
    over(bf, vec3(1.0), halo * 0.35 * kPhase * (1.0 - 0.5 * blur));
    over(bf, body, inside * (0.28 + 0.12 * fillK) * (1.0 - 0.3 * blur));
    over(bf, mix(body, rimc, 0.5), gran * 0.35);
    over(bf, orgc, org * 0.55);
    over(bf, mix(rimc, vec3(1.0), 0.15), vac * 0.6);
    over(bf, rimc, (rim * 0.75 + innerL * 0.5) * (1.0 - 0.4 * blur));
    over(bf, rimc, lines * 0.7);
    // dark-field: light scattered by the edges and the structures, on black
    vec3 df = rimc * (rim * 0.9 + innerL * 0.4 + lines * 0.55 + vac * 0.4) + body * inside * (fillK * 0.06 + gran * 0.2) +
              orgc * org * 0.4;
    df *= uGlow * (1.0 - 0.45 * blur);
    // nothing reaches the quad's edge
    float win = 1.0 - smoothstep(0.58, 0.71, max(abs(q.x), abs(q.y)));
    vec4 o = mix(bf, vec4(df, 0.0), kDark) * vB.w * win;
    if (o.r + o.g + o.b + o.a <= 0.0005) discard;
    frag = o;
}
)";

// ---------------------------------------------------------------- the pieces (instanced per cell)
// a: x, y (cell center), half (board units), kind (0 cell, 1 ghost, 3 dying). b: type, seed, progress,
// alpha. c: nucleus x, y, lock flash, joined neighbors (bits: left, right, up, down, up-left, up-right, down-left,
// down-right). d: the cell's place in its grid (for the membrane's wobble, continuous across a colony), -, -.
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
    vec2 l = aPos * 0.64;
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
uniform float uTime, uLife, uGlow;
uniform vec3 uNucGlow;   // the nuclei's glow in dark-field
float AA;
// a concave corner of the colony: the empty quadrant q > 0, its corner rounded
float notch(vec2 q, float rn) { vec2 a = vec2(rn) - q; return length(max(a, 0.0)) + min(max(a.x, a.y), 0.0) - rn; }
float blob(vec2 p, vec2 c, vec2 r) { return 1.0 - smoothstep(1.0 - AA * 2.0 / min(r.x, r.y), 1.0, length((p - c) / r)); }
// A colony's cell as a metaball: its field (x) and the field's gradient (yz) at q from the cell's center: 1 on a
// rounded core, falling to 0 within MB_W of it (so a cell reaches no farther than 1.5 cells: the two cells on each
// side of a wall see the same field, the colony's outline is seamless); the colony's membrane is where the summed
// fields cross MB_T: one body, its cells merged with soft waists and filleted inner corners, its corners round.
const float MB_W = 0.8, MB_T = 0.5, MB_CR = 0.1;
const float MB_H = 0.5 - 0.085 - MB_W * (1.0 - 0.70710678);   // the core: the membrane 0.085 inside the cell's square
vec3 cellField(vec2 q) {
    vec2 w = abs(q) - vec2(MB_H - MB_CR);
    vec2 wp = max(w, 0.0);
    float lw = length(wp);
    float e = lw + min(max(w.x, w.y), 0.0) - MB_CR;
    if (e <= 0.0) return vec3(1.0, 0.0, 0.0);
    if (e >= MB_W) return vec3(0.0);
    vec2 ge = lw > 1e-5 ? wp / lw : (w.x > w.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    ge *= sign(q + 1e-6);
    float k = 1.0 - e / MB_W;
    return vec3(k * k, -2.0 * k / MB_W * ge);
}
// round granules: one at most per cell of a jittered grid at x (a fraction dens of the cells), soft-edged
float granules(vec2 x, float seed, float dens, float px) {
    vec2 i = floor(x), f = fract(x);
    float h = hash12(i + seed);
    if (h > dens) return 0.0;
    vec2 c = 0.3 + 0.4 * vec2(hash12(i * 1.7 + seed + 3.1), hash12(i * 2.3 + seed + 7.7));
    float r = 0.16 + 0.1 * fract(h * 31.7);
    return 1.0 - smoothstep(r - px, r + px, length(f - c));
}
vec2 rot(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }
// a soft blob of radius r around c, its outline irregular (seeded) and slowly changing
float softBlob(vec2 p, vec2 c, vec2 r, float seed, float t) {
    vec2 q = (p - c) / r;
    float l = length(q);
    vec2 dir = q / max(l, 1e-4);
    l *= 1.0 + 0.32 * (vnoise(dir * 1.3 + seed * 17.0 + vec2(t * 0.04, 0.0)) - 0.5);
    float e = AA * 1.5 / min(r.x, r.y);
    return 1.0 - smoothstep(1.0 - e - 0.08, 1.0 + e, l);
}
// a fine ring (a vacuole's, a grain's edge) of radius r around c
float softRing(vec2 p, vec2 c, vec2 r, float w) { return 1.0 - smoothstep(w - AA, w + AA, abs(length((p - c) / r) - 1.0) * min(r.x, r.y)); }
// A cell of a living colony, as the organisms are seen (until the drop): p its place in the cell, gp in the colony's
// grid, d the colony's membrane; a deep stain (the type's) denser inward, the clear ectoplasm along the membrane, the
// membrane a strong dark line with a bright phase halo outside it, soft wavy walls
// between the colony's cells, an irregular nucleus off center with its chromatin and nucleolus, granules streaming
// slowly, the type's inner life (a vacuole, starch grains, a big nucleus, chloroplasts, two nuclei, dense granules,
// a large vacuole).
float gHalo = 1.0;   // the phase halo's strength (faint around a burst's droplets)
vec4 livingCell(vec2 p, vec2 gp, float d, int type, float seed, vec2 ncIn, int m, float flash, vec3 field) {
    float t = uTime;
    vec3 stain = uPieceA[type];
    vec3 deep = pow(stain, vec3(1.7)) * 0.9;           // the stain's dense, saturated tone
    vec3 rimc = mix(orgCol(1), deep * 0.3, 0.35);
    float inside = 1.0 - smoothstep(-AA, AA, d);
    float din = max(-d, 0.0);
    // the walls shared with the colony's other cells: soft, wavy (laid in the grid, so both sides agree)
    float iw = 9.0;
    if ((m & 1) != 0) iw = min(iw, abs(p.x + 0.5 + 0.1 * (vnoise(vec2(gp.y * 2.2, 3.1 + t * 0.03)) - 0.5)));
    if ((m & 2) != 0) iw = min(iw, abs(p.x - 0.5 + 0.1 * (vnoise(vec2(gp.y * 2.2, 3.1 + t * 0.03)) - 0.5)));
    if ((m & 4) != 0) iw = min(iw, abs(p.y - 0.5 + 0.1 * (vnoise(vec2(gp.x * 2.2, 7.3 + t * 0.03)) - 0.5)));
    if ((m & 8) != 0) iw = min(iw, abs(p.y + 0.5 + 0.1 * (vnoise(vec2(gp.x * 2.2, 7.3 + t * 0.03)) - 0.5)));
    float edge = min(din, iw);
    float wall = exp(-pow(iw / 0.017, 2.0)) * inside;
    float mem = exp(-pow((d + 0.022) / 0.017, 2.0));
    float halo = smoothstep(-AA, AA, d) * exp(-max(d, 0.0) / 0.04) * gHalo;
    // the cytoplasm: clear along the membrane, the stain dense inside, granules streaming around the cell
    float endo = smoothstep(0.03, 0.14, edge);
    float n = vnoise(gp * 7.0 + seed * 31.0 + vec2(t * 0.02, 0.0));
    vec2 sp = rot(p, t * 0.05 + seed * 6.0);
    float px = AA * 9.0;
    float grD = (granules(sp * 9.0, seed * 40.0, 0.35, px) * 0.8 + (vnoise(sp * 30.0 + seed * 9.0) - 0.4) * 0.5) * endo;
    float grL = granules(sp * 7.0 + 0.5, seed * 23.0 + 5.0, 0.18, px * 0.78) * endo;
    vec3 cyto = mix(mix(field, stain, 0.75), deep, 0.25 + 0.45 * endo + 0.15 * (n - 0.5));
    // the nucleus: off center, irregular, its chromatin, its nucleolus; the type's own inner life
    vec2 nc = ncIn * 2.2;
    float nr = 0.15, nuc = 0.0, nuc2 = 0.0, vac = 0.0, vacR = 0.0, gr = 0.0;
    if (type == 2) nr = 0.2;
    if (type == 6) {
        nc = vec2(nc.x > 0.0 ? 0.22 : -0.22, nc.y * 0.5); nr = 0.11;
        vec2 vc = vec2(-nc.x * 0.5, 0.02), vr = vec2(0.19, 0.24);
        vac = softBlob(p, vc, vr, seed + 3.0, t); vacR = softRing(p, vc, vr, 0.012);
    }
    if (type == 4) {
        nuc = softBlob(p, nc * 0.5 + vec2(-0.13, 0.09), vec2(0.12, 0.105), seed, t);
        nuc2 = softBlob(p, nc * 0.5 + vec2(0.13, -0.1), vec2(0.105, 0.12), seed + 5.0, t);
    } else nuc = softBlob(p, nc, vec2(nr, nr * 0.84), seed, t);
    if (type == 0) { vec2 vc = -nc * 1.3 + vec2(0.1, -0.12); vac = softBlob(p, vc, vec2(0.09), seed + 2.0, t); vacR = softRing(p, vc, vec2(0.09), 0.01); }
    if (type == 1)
        for (int i = 0; i < 5; i++) {
            float an = float(i) * 1.2566 + seed * 6.0 + 0.3 * sin(t * 0.05 + float(i));
            vec2 c = vec2(cos(an), sin(an)) * 0.27;
            vac = max(vac, softBlob(p, c, vec2(0.07, 0.056), seed + float(i), t));
            vacR = max(vacR, softRing(p, c, vec2(0.07, 0.056), 0.01));
        }
    if (type == 3)
        for (int i = 0; i < 7; i++) {
            float an = float(i) * 0.8976 + seed * 6.0 + t * 0.03;
            gr = max(gr, softBlob(p, vec2(cos(an), sin(an)) * (0.29 - 0.03 * float(i % 2)), vec2(0.065, 0.045), seed + float(i), t));
        }
    if (type == 5) gr = granules(sp * 11.0, seed * 3.0 + 1.0, 0.6, px * 1.22) * 0.9;
    gr *= inside;
    nuc *= inside; nuc2 *= inside; vac *= inside; vacR *= inside;   // (a burst's droplets carry them: kept within)
    float nucAll = clamp(nuc + nuc2, 0.0, 1.0);
    float nucRim = clamp(nucAll * (1.0 - nucAll) * 4.0, 0.0, 1.0);
    float chrom = vnoise(p * 38.0 + seed * 9.0 + t * 0.02);
    float nucl = type == 4 ? 0.0 : softBlob(p, nc + vec2(0.035, 0.02), vec2(0.04), seed + 9.0, t) * inside;
    vec3 nucC = mix(deep * 0.35, mix(orgCol(2), deep, 0.5) * 0.5, 0.3);
    // bright-field: the stained cell on the lit field, with real dark and light
    vec4 bf = vec4(0.0);
    over(bf, vec3(1.0), halo * (0.35 + 0.4 * kPhase) * (1.0 - 0.5 * flash));
    over(bf, cyto, inside * 0.9);
    over(bf, deep * 0.5, clamp(grD, 0.0, 1.0) * 0.55);
    over(bf, mix(cyto, vec3(1.0), 0.5), grL * 0.4 * kPhase);
    over(bf, mix(field, cyto, 0.35), vac * 0.85);
    over(bf, rimc, vacR * 0.7);
    over(bf, type == 3 ? deep * 0.55 : deep * 0.45, gr * 0.8);
    over(bf, mix(nucC, cyto, 0.25 * chrom), nucAll * 0.95);
    over(bf, nucC * 0.6, nucRim * 0.5 + nucl * 0.85);
    over(bf, mix(field, cyto, 0.3), exp(-pow(iw / 0.045, 2.0)) * inside * 0.25 * kPhase);   // (pale beside the walls)
    over(bf, rimc, wall * 0.85);
    over(bf, mix(rimc, vec3(1.0), 0.4 * flash), mem * 0.95);
    // dark-field: the membranes and walls glow in the stain, the cytoplasm a soft glow, granules and nuclei lit
    vec3 glow = stain * (0.05 + 0.12 * (1.0 - endo)) * inside + stain * (grL * 0.5 + grD * 0.15 + gr * 0.4 + vacR * 0.6);
    glow += mix(stain, uNucGlow, 0.6) * (nucAll * 0.45 + nucRim * 0.35 + nucl * 0.3);
    glow += stain * (wall * 0.55 + mem * (1.3 + 0.8 * flash));
    glow *= uGlow;
    vec4 df = vec4(glow, max(inside * 0.85, mem * 0.6));
    return mix(bf, df, kDark);
}
// A cleared cell bursting (t: 0..1): it swells round and comes apart from its neighbors, then its body breaks into
// droplets of itself, budding off (smoothly merged, their necks thinning until they part), drifting out and
// shrinking; the nucleus goes with the heaviest. BURST_N droplets: their centers and radii at t (q: center, radius).
const int BURST_N = 6;
vec3 burstDrop(int i, float t, float seed, vec2 nc) {
    float h1 = hash12(vec2(float(i) * 7.1, seed * 91.0)), h2 = hash12(vec2(float(i) * 3.7 + 5.0, seed * 53.0));
    float an = seed * 6.2832 + float(i) * 6.2832 / float(BURST_N) + (h1 - 0.5) * 0.7;
    vec2 dir = vec2(cos(an), sin(an) * 0.75);
    vec2 c0 = dir * 0.2;
    float r0 = 0.15 + 0.08 * h2, L = 0.35 + 0.45 * h1;
    if (i == 0) { dir = normalize(nc + vec2(0.01, 0.003)) * 0.8; c0 = dir * 0.12; r0 = 0.27; L = 0.22; }   // (the nucleus's)
    // out fast, then drifting slowly to rest
    float tau = clamp((t - 0.12) / 0.88, 0.0, 1.0);
    float go = (1.0 - exp(-4.5 * tau)) / (1.0 - exp(-4.5));
    float r = r0 * mix(1.0, 0.45 + 0.3 * h2, smoothstep(0.15, 1.0, t));
    return vec3(c0 + dir * L * go, r);
}
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
// the droplets' outline (their smooth union, merged less and less) and the warp carrying the cell's contents along
float burstShape(vec2 p, float t, float seed, vec2 nc, out vec2 disp) {
    float k = mix(0.22, 0.015, smoothstep(0.15, 0.65, t));
    float dd = 9.0, ws = 1e-4;
    disp = vec2(0.0);
    for (int i = 0; i < BURST_N; i++) {
        vec3 q = burstDrop(i, t, seed, nc);
        vec3 q0 = burstDrop(i, 0.0, seed, nc);
        vec2 v = p - q.xy;
        float l = length(v);
        // its membrane wobbles a little, like the living colony's
        float e = l - q.z * (1.0 + 0.12 * (vnoise(v / max(l, 1e-4) * 1.4 + float(i) * 5.3 + seed * 17.0 + vec2(uTime * 0.3, 0.0)) - 0.5));
        dd = i == 0 ? e : smin(dd, e, k);
        float w = exp(-pow(l / (q.z * 1.2), 2.0) * 2.0);
        disp += (q.xy - q0.xy) * w;
        ws += w;
    }
    disp /= ws;
    return dd;
}
void main() {
    vec2 p = vLocal;
    lightAt(vA.xy);
    int kind = int(vA.w + 0.5);
    int type = int(vB.x + 0.5);
    float seed = vB.y, t = vB.z;
    int m = int(vC.w + 0.5);
    bool L = (m & 1) != 0, R = (m & 2) != 0, U = (m & 4) != 0, D = (m & 8) != 0;
    // a burst: its quad wider than its cell (p still in cell units); its joins let go as it swells
    bool burst = kind == 2;
    float join = 1.0, swell = 0.0, drops = 0.0;
    if (burst) {
        p = vLocal * 2.0 * vA.z;
        join = 1.0 - smoothstep(0.0, 0.11, t);
        swell = smoothstep(0.0, 0.12, t);
        drops = smoothstep(0.11, 0.33, t);
    }
    // the colony's cells draw their own squares: past a joined side is the neighbor's (a burst's, until it lets go)
    bool past = (L && p.x < -0.5) || (R && p.x > 0.5) || (U && p.y > 0.5) || (D && p.y < -0.5);
    if (past && (!burst || join > 0.0)) discard;
    AA = max(length(fwidth(p)) * 0.75, 1e-4);
    const float G = 0.075;
    const float RC = 0.2;
    vec2 lo = vec2(L ? mix(-0.5 + G, -0.5 - RC, join) : -0.5 + G, D ? mix(-0.5 + G, -0.5 - RC, join) : -0.5 + G);
    vec2 hi = vec2(R ? mix(0.5 - G, 0.5 + RC, join) : 0.5 - G, U ? mix(0.5 - G, 0.5 + RC, join) : 0.5 - G);
)"
R"(    float d = sdBox(p - (lo + hi) * 0.5, (hi - lo) * 0.5 - RC) - RC;
    const float RN = 0.06;
    float dn = d;
    if (L && U && (m & 16) == 0) dn = max(dn, -notch(vec2(-0.5 + G - p.x, p.y - 0.5 + G), RN));
    if (R && U && (m & 32) == 0) dn = max(dn, -notch(vec2(p.x - 0.5 + G, p.y - 0.5 + G), RN));
    if (L && D && (m & 64) == 0) dn = max(dn, -notch(vec2(-0.5 + G - p.x, -0.5 + G - p.y), RN));
    if (R && D && (m & 128) == 0) dn = max(dn, -notch(vec2(p.x - 0.5 + G, -0.5 + G - p.y), RN));
    d = mix(d, dn, join);
    // the membrane breathes a little, the same along the whole colony
    vec2 gp = vD.xy + p;
    d += (vnoise(gp * 2.6 + vec2(uTime * 0.11, -uTime * 0.07)) - 0.5) * 0.045 * uLife;
    // a burst: the cell swells round (turgid), then its body becomes its droplets; its contents carried along
    vec2 disp = vec2(0.0);
    float bd = 9.0;
    if (burst) {
        bd = burstShape(p, t, seed, vC.xy, disp);
        disp *= drops;
    }
    // ---- Until the drop, a living colony (its look below): one body, the union of its cells' rounded squares
    // smoothly merged, inset from the cells' edges, its membrane wobbling and breathing slowly (the same along the
    // colony: in its grid's noise); between pieces stays a pale gap, a cell never leaves its square.
    float dl = 9.0;
    if (kMix < 0.999) {
        vec3 f = cellField(p);
        for (int i = 0; i < 8; i++) {
            if ((m & (1 << i)) == 0) continue;
            vec2 o = i == 0 ? vec2(-1, 0) : i == 1 ? vec2(1, 0) : i == 2 ? vec2(0, 1) : i == 3 ? vec2(0, -1) :
                     i == 4 ? vec2(-1, 1) : i == 5 ? vec2(1, 1) : i == 6 ? vec2(-1, -1) : vec2(1, -1);
            f += cellField(p - o) * join;
        }
        float gl = length(f.yz);
        dl = (MB_T - f.x) / max(gl, 0.8);   // (a distance near the membrane)
        float wob = (vnoise(gp * 1.5 + vec2(uTime * 0.06, -uTime * 0.05)) - 0.5) * 0.07 +
                    (vnoise(gp * 0.4 + vec2(uTime * 0.09, 0.0)) - 0.5) * 0.03;
        dl += wob * uLife;
        if (kind == 3) dl += 0.04 + 0.2 * smoothstep(0.0, 0.7, t) - 0.03 * vnoise(p * 9.0 + seed * 7.0);   // plasmolysis
    }
    if (burst) {
        // swelling: rounder and a little bigger, in either look; then the droplets
        float rd = length(p) - 0.47;
        d = mix(d, rd, 0.55 * swell) - 0.04 * swell;
        if (dl < 8.0) dl = mix(dl, rd, 0.55 * swell) - 0.04 * swell;
        d = mix(d, bd, drops);
        if (dl < 8.0) dl = mix(dl, bd, drops);
        p -= disp;
        gp -= disp;
        m = join > 0.5 ? m : 0;
        L = L && join > 0.5; R = R && join > 0.5; U = U && join > 0.5; D = D && join > 0.5;
    }
    if (d > 0.14 && dl > 0.14) discard;
    vec3 tint = pieceCol(type);
    vec3 field = fieldCol(0);
    float inside = 1.0 - smoothstep(-AA, AA, d);
    float din = max(-d, 0.0);
    // the walls shared with the colony's other cells
    float iw = 9.0;
    if (L) iw = min(iw, abs(p.x + 0.5));
    if (R) iw = min(iw, abs(p.x - 0.5));
    if (U) iw = min(iw, abs(p.y - 0.5));
    if (D) iw = min(iw, abs(p.y + 0.5));
    float wallD = min(din, iw);
    const float MW = 0.032;
    float mem = exp(-pow((d + MW) / (MW * 0.9), 2.0));
    float wall = (1.0 - smoothstep(0.011, 0.011 + AA * 1.5, iw)) * inside;
    float halo = smoothstep(-AA, AA, d) * exp(-d / 0.045);
    float flash = vC.z;
    if (burst) {
        flash = 0.6 * smoothstep(0.03, 0.14, t) * (1.0 - smoothstep(0.18, 0.45, t));   // (the membrane taut, lit)
        gHalo = 1.0 - 0.6 * drops;   // (the droplets' halos kept faint: they cross one another)
        halo *= gHalo;
    }
    // the protoplast (shrinks away from its membrane as the cell dies)
    float proto = 1.0;
    if (kind == 3) {
        float s = smoothstep(0.0, 0.7, t);
        proto = 1.0 - smoothstep(-AA, AA, d + 0.04 + 0.2 * s - 0.03 * vnoise(p * 9.0 + seed * 7.0));
    }
    if (kind == 1) {
        // the ghost: the colony's membrane alone, faint, a breath of its stain inside (the living colony's until the drop)
        if (dl < 8.0) {
            float dg = mix(dl, d, kMix);
            inside = 1.0 - smoothstep(-AA, AA, dg);
            mem = exp(-pow((dg + MW) / (MW * 0.9), 2.0));
        }
        vec4 g = vec4(0.0);
        over(g, mix(field, tint, 0.5), inside * 0.1);
        over(g, tint * 0.55, mem * 0.55);
        vec3 gd = tint * (mem * 0.55 + inside * 0.015) * uGlow;
        vec4 o = mix(g, vec4(gd, mem * 0.3), kDark) * vB.w;
        if (o.a + o.r <= 0.002) discard;
        frag = o;
        return;
    }
    // the cytoplasm: the stain, denser along the walls, finely mottled
    float n = vnoise(gp * 13.0 + seed * 31.0) * 0.6 + vnoise(gp * 31.0 - seed * 17.0) * 0.4;
    float dens = 0.8 + 0.14 * exp(-wallD / 0.07) + 0.08 * (n - 0.5);   // (strongly stained: the stack reads on the pale field)
    vec3 cyto = mix(field, tint, dens);
    // a pale line of light just inside the walls (the cell's volume)
    float lip = exp(-pow((wallD - 0.075) / 0.03, 2.0));
    cyto = mix(cyto, vec3(1.0), lip * 0.1 * kPhase);
    // the nucleus and the type's own inner life
    vec2 nc = vC.xy;
    float nr = 0.16;
    float nuc = 0.0, nuc2 = 0.0, vacu = 0.0, gr = 0.0;
    if (type == 2) nr = 0.21;
    if (type == 6) { nc = vec2(nc.x > 0.0 ? 0.2 : -0.2, nc.y * 0.5); nr = 0.11; vacu = blob(p, vec2(-nc.x * 0.55, 0.0), vec2(0.2, 0.24)); }
    if (type == 4) {
        nuc = blob(p, nc * 0.5 + vec2(-0.12, 0.08), vec2(0.12, 0.11));
        nuc2 = blob(p, nc * 0.5 + vec2(0.13, -0.09), vec2(0.11, 0.12));
    } else nuc = blob(p, nc, vec2(nr, nr * 0.86));
    if (type == 0) vacu = blob(p, -nc * 1.6 + vec2(0.1, -0.12), vec2(0.09, 0.09));
    if (type == 1)
        for (int i = 0; i < 5; i++) {
            float an = float(i) * 1.2566 + seed * 6.0;
            vec2 c = vec2(cos(an), sin(an)) * 0.27;
            gr += blob(p, c, vec2(0.075, 0.06));
        }
    if (type == 3)
        for (int i = 0; i < 7; i++) {
            float an = float(i) * 0.8976 + seed * 6.0;
            vec2 c = vec2(cos(an), sin(an)) * (0.3 - 0.03 * float(i % 2));
            gr += blob(p, c, vec2(0.06, 0.048));
        }
    if (type == 5) gr = smoothstep(0.68, 0.8, vnoise(gp * 24.0 + seed * 3.0)) * 0.6;
    nuc *= inside; nuc2 *= inside; vacu *= inside; gr *= inside;   // (a burst's droplets carry them: kept within)
    float nucAll = clamp(nuc + nuc2, 0.0, 1.0);
    float nucRim = nucAll * (1.0 - nucAll) * 4.0;
    float chrom = vnoise(p * 40.0 + seed * 9.0);
    // bright-field: stains on the lit field
    vec4 bf = vec4(0.0);
    over(bf, mix(field * 1.06 + 0.03, vec3(1.0), 0.3), halo * 0.55 * kPhase * (1.0 - flash * 0.5));
    over(bf, cyto, inside * 0.94 * mix(0.35, 1.0, proto));
    over(bf, mix(cyto, vec3(1.0), 0.35), vacu * 0.8 * proto);
    over(bf, tint * 0.5, vacu * (1.0 - vacu) * 3.0 * proto);
    if (type == 1) {
        over(bf, mix(cyto, vec3(1.0), 0.45), gr * 0.85 * proto * inside);
        over(bf, tint * 0.45, gr * (1.0 - gr) * 3.0 * proto * inside);
    } else over(bf, tint * (type == 3 ? 0.45 : 0.6), gr * 0.65 * proto * inside);
    over(bf, mix(tint * 0.32, cyto, 0.2 + 0.2 * chrom), nucAll * 0.9 * proto);
    over(bf, tint * 0.3, nucRim * 0.45 * proto);
    over(bf, tint * 0.25, blob(p, nc + vec2(0.03, 0.02), vec2(0.035, 0.035)) * 0.6 * proto * inside * step(nuc2, 0.01));
    over(bf, mix(tint * 0.38, field, 0.15), wall * 0.75);
    over(bf, mix(tint * 0.24, vec3(1.0), 0.4 * flash), mem);
    // dark-field: the membranes glow, the cytoplasm a soft glow, the nuclei lit
    vec3 glow = tint * (0.04 + 0.2 * exp(-wallD / 0.07) + 0.05 * n) * inside * proto;
    glow += tint * (gr * 0.35 + vacu * 0.08) * proto;
    glow += mix(tint, uNucGlow, 0.65) * (nucAll * 0.55 + nucRim * 0.3) * proto;
    glow += tint * wall * 0.45 + tint * mem * (1.2 + 0.8 * flash);
    glow *= uGlow;
    vec4 df = vec4(glow, max(inside * 0.85, mem * 0.6));
    vec4 o = mix(bf, df, kDark);
    if (dl < 8.0) o = mix(livingCell(p, gp, dl, type, seed, nc, m, flash, field), o, kMix);
    float a = vB.w;
    if (burst) a *= 1.0 - smoothstep(mix(0.5, 0.32, kMix), mix(1.0, 0.72, kMix), t);   // (gone as the rows above fall in)
    if (kind == 3) {
        // dying: the stain drains to grey, then the cell fades
        float Lm = lumOf(o.rgb / max(o.a, 1e-3));
        o.rgb = mix(o.rgb, vec3(Lm) * o.a, smoothstep(0.0, 0.8, t) * 0.85);
        a *= 1.0 - smoothstep(0.6, 1.0, t);
    }
    o *= a;
    if (o.a + o.r + o.g + o.b <= 0.002) discard;
    frag = o;
}
)";

// ---------------------------------------------------------------- loose granules (a hard drop, a burst cell)
// a: x, y, radius, -. b: -, -, -, alpha. c: tint rgb, -.
inline const char* SPECK_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
void main() {
    lightAt(vA.xy);
    float r = length(vLocal) / 0.5;
    float core = 1.0 - smoothstep(0.45, 0.65, r);
    float halo = exp(-pow((r - 0.8) / 0.18, 2.0));
    vec3 tint = vC.rgb;
    vec4 bf = vec4(0.0);
    over(bf, vec3(1.0), halo * 0.3 * kPhase);
    over(bf, tint * 0.45, core * 0.8);
    vec4 df = vec4(tint * (core * 0.5 + halo * 0.6), 0.0);
    vec4 o = mix(bf, df, kDark) * vB.w;
    if (o.a + o.r <= 0.002) discard;
    frag = o;
}
)";

// ---------------------------------------------------------------- the soft blur (glow, defocus)
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb +
             texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    frag = vec4(c * 0.25, 1.0);
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
uniform sampler2D uScene, uBlur;
uniform float uGlowK, uDefocus, uLamp, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 b = texture(uBlur, vUv).rgb;
    // out of focus (the slide coming into focus, going out of it)
    vec3 c = mix(s.rgb, b, uDefocus);
    // dark-field: a soft glow around what scatters the light
    c += max(b - vec3(0.03), 0.0) * uGlowK;
    // a soft knee: the glowing walls never burn to white
    c = c / (1.0 + 0.25 * max(max(c.r, c.g), c.b) * smoothstep(0.8, 1.6, max(max(c.r, c.g), c.b)));
    // the lamp: a halogen warming up from a dim orange glow
    vec3 warm = mix(vec3(1.0, 0.55, 0.22), vec3(1.0), smoothstep(0.0, 0.8, uLamp));
    c *= uLamp * warm;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the panels under the HUD (pixels)
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aCol;
uniform vec2 uRes;
out vec4 vCol;
void main() {
    vCol = aCol;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec4 vCol;
out vec4 frag;
void main() { frag = vec4(vCol.rgb * vCol.a, vCol.a); }
)";

// Builds a fragment shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace microscope_shaders

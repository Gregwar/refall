#pragma once
// GROCERIES' shaders (see groceries.cpp).
//
// Everything solid is an instanced box (BOX_VS / BOX_FS) with a procedural material, seen from above: the products
// and their packaging (boxes lying on their backs, cartons and tins standing), the counter's laminate or wood, the
// belt's rubber, steel, paper and fabric bags, the scanner's glass and its red laser lines, the till's screen, the
// receipt's thermal paper, a shopping list, discs (a cup's lid, sunglasses). BLOB_FS lays soft contact shadows on
// the counter; SPRITE_FS the glows; LINE_FS hairlines (the ghost); VEIL_FS the shade behind the well and the HUD's
// columns; DOWN / BLUR / FINAL the glow and the frame; SHAPE / TEXT the HUD. +z is up, toward the camera.
#include <string>

namespace groceries_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform float uTime, uSat;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
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
vec3 saturateC(vec3 c, float k) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return max(mix(vec3(l), c, k), 0.0);
}
)";

// ---------------------------------------------------------------- solids (instanced boxes)
inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
layout(location = 7) in vec4 iE;   // -, -, -, light group
uniform mat4 uVP;
out vec3 vP, vL;
flat out vec3 vLN, vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat;
flat out float vGroup;
void main() {
    vec3 P = iC.xyz + iX.xyz * aPos.x + iY.xyz * aPos.y + iZ.xyz * aPos.z;
    vHalf = vec3(length(iX.xyz), length(iY.xyz), length(iZ.xyz));
    vAX = iX.xyz / max(vHalf.x, 1e-6);
    vAY = iY.xyz / max(vHalf.y, 1e-6);
    vAZ = iZ.xyz / max(vHalf.z, 1e-6);
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    vGroup = iE.w;
    gl_Position = uVP * vec4(P, 1.0);
}
)";

inline const char* BOX_FS = R"(#version 330 core
in vec3 vP, vL;
flat in vec3 vLN, vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat;
flat in float vGroup;
out vec4 frag;
uniform vec3 uAmbTop, uAmbBot, uKeyDir, uKeyCol;
uniform int uNL;
uniform vec4 uLP[16];
uniform vec4 uLC[16];
uniform float uGroupK[16];
uniform vec3 uFA[7], uFB[7], uFC[7];
uniform vec3 uKraft, uAccent;
uniform float uFloorZ, uBeltPhase, uStackA, uPatch, uGloss;

float aa(float d, float px) { return 1.0 - smoothstep(-px, px, d); }
float rbox(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
float circ(vec2 p, vec2 c, float r) { return length(p - c) - r; }
// illegible "text": rows of rounded bars of random lengths
float bars(vec2 f, vec2 c, vec2 hs, int rows, float seed, float px) {
    float m = 0.0;
    float rh = hs.y * 2.0 / float(rows);
    for (int i = 0; i < 4; i++) {
        if (i >= rows) break;
        float y = c.y + hs.y - rh * (float(i) + 0.5);
        float len = hs.x * (0.55 + 0.45 * hash12(vec2(seed, float(i))));
        float x0 = c.x - hs.x;
        float d = rbox(f - vec2(x0 + len * 0.5 + (hash12(vec2(float(i), seed + 3.0)) - 0.5) * 0.0, y), vec2(len * 0.5, rh * 0.28), rh * 0.27);
        m = max(m, aa(d, px));
    }
    return m;
}
// a lit sphere's shading inside a disc (for fruit, berries)
vec3 sphere(vec2 f, vec2 c, float r, vec3 col, float px, inout float cov) {
    vec2 d = (f - c) / r;
    float l2 = dot(d, d);
    float m = aa(sqrt(l2) - 1.0, px / r);
    float z = sqrt(max(1.0 - l2, 0.0));
    float lit = 0.55 + 0.45 * clamp(dot(vec3(d.x, d.y, z), normalize(vec3(-0.4, 0.6, 0.7))), 0.0, 1.0);
    float hi = pow(clamp(1.0 - length(d - vec2(-0.35, 0.4)) * 2.2, 0.0, 1.0), 2.0) * 0.5;
    cov = max(cov, m);
    return (col * lit + vec3(hi)) * m;
}
// seven-segment digit (0..9) in a cell of half size (0.5, 1)
float digit(vec2 p, int d, float w, float px) {
    // segments: a top, b top-right, c bottom-right, d bottom, e bottom-left, f top-left, g middle
    int segs[10] = int[10](0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F);
    int s = segs[clamp(d, 0, 9)];
    float m = 0.0;
    if ((s & 1) != 0) m = max(m, aa(rbox(p - vec2(0.0, 0.9), vec2(0.38, w), w), px));
    if ((s & 2) != 0) m = max(m, aa(rbox(p - vec2(0.42, 0.45), vec2(w, 0.36), w), px));
    if ((s & 4) != 0) m = max(m, aa(rbox(p - vec2(0.42, -0.45), vec2(w, 0.36), w), px));
    if ((s & 8) != 0) m = max(m, aa(rbox(p - vec2(0.0, -0.9), vec2(0.38, w), w), px));
    if ((s & 16) != 0) m = max(m, aa(rbox(p - vec2(-0.42, -0.45), vec2(w, 0.36), w), px));
    if ((s & 32) != 0) m = max(m, aa(rbox(p - vec2(-0.42, 0.45), vec2(w, 0.36), w), px));
    if ((s & 64) != 0) m = max(m, aa(rbox(p, vec2(0.38, w), w), px));
    return m;
}

// ---- the products: f in -1..1 on the face; face 0 front / back, 1 top, 2 side, 3 bottom
vec3 product(int fam, int face, vec2 f, float px, float seed, out float gloss) {
    vec3 A = uFA[fam], B = uFB[fam], C = uFC[fam];
    vec3 c = A;
    gloss = 0.3;
    if (face == 3) return A * 0.6;
    if (fam == 0) {
        // MILK: a carton, blue below a white shoulder, a drop and a little text; the gable on top
        if (face == 1) {
            c = B;
            c *= f.y > 0.0 ? 1.0 : 0.86;
            c = mix(c, C, aa(abs(f.y) - 0.05, px) * 0.6);
            c = mix(c, C, aa(circ(f, vec2(0.45, 0.42), 0.18), px));
            c = mix(c, B, aa(circ(f, vec2(0.45, 0.42), 0.1), px));
        } else {
            float wave = 0.12 + 0.07 * sin(f.x * 3.0 + 0.8 + seed);
            c = mix(A, B, aa(wave - f.y, px));
            c = mix(c, C, aa(abs(f.y - wave) - 0.03, px) * 0.7);
            if (face == 0) {
                float drop = min(circ(f, vec2(-0.45, 0.5), 0.15), rbox((f - vec2(-0.45, 0.62)) * mat2(0.7, 0.7, -0.7, 0.7), vec2(0.105), 0.0));
                c = mix(c, C, aa(drop, px));
                c = mix(c, C, bars(f, vec2(0.22, 0.52), vec2(0.42, 0.2), 2, seed, px) * 0.8);
                c = mix(c, B, bars(f, vec2(0.0, -0.55), vec2(0.6, 0.12), 1, seed + 5.0, px) * 0.9);
            }
            c = mix(c, C, aa(-0.9 - f.y, px));
        }
        gloss = 0.35;
    } else if (fam == 1) {
        // CEREAL: a golden box, a cream oval with a bowl of flakes, a red band of text
        if (face == 0) {
            float ov = length((f - vec2(0.0, -0.12)) / vec2(0.74, 0.62)) - 1.0;
            c = mix(A, B, aa(ov * 0.6, px));
            float bowl = max(circ(f, vec2(0.0, -0.22), 0.46), f.y + 0.22);
            c = mix(c, C, aa(bowl, px));
            c = mix(c, C * 1.25 + 0.05, aa(abs(f.y + 0.22) - 0.03, px) * aa(abs(f.x) - 0.46, px));
            float fl = min(min(circ(f, vec2(-0.22, -0.08), 0.1), circ(f, vec2(0.04, -0.02), 0.11)), circ(f, vec2(0.27, -0.1), 0.09));
            c = mix(c, A * 0.8, aa(fl, px));
            c = mix(c, C, aa(0.66 - f.y, px));
            c = mix(c, B, bars(f, vec2(0.0, 0.82), vec2(0.62, 0.1), 1, seed, px));
        } else if (face == 2) {
            c = A;
            float panel = rbox(f - vec2(0.0, -0.15), vec2(0.62, 0.55), 0.08);
            c = mix(c, B, aa(panel, px));
            c = mix(c, C * 0.8, bars(f, vec2(0.0, -0.15), vec2(0.5, 0.45), 4, seed, px) * 0.45);
            c = mix(c, C, aa(0.66 - f.y, px));
        } else {
            c = A * (0.95 + 0.05 * step(0.0, f.y));
            c = mix(c, A * 0.8, aa(abs(f.y) - 0.02, px));
        }
        gloss = 0.25;
    } else if (fam == 2) {
        // BERRIES: a clear punnet full of berries, a cream band
        vec2 g = f * vec2(2.6, 2.6);
        if (face == 1) g = f * 2.6;
        vec2 cell = floor(g);
        vec2 off = vec2(mod(cell.y, 2.0) * 0.5, 0.0);
        vec2 gc = floor(g + off);
        vec2 local = fract(g + off) - 0.5;
        float hh = hash12(gc + seed);
        float cov = 0.0;
        vec3 berry = mix(A, C, hh * 0.6);
        vec3 bc = sphere(local, vec2((hh - 0.5) * 0.12, 0.0), 0.47, berry, px * 2.6, cov);
        c = mix(A * 0.25, bc, cov);
        // the plastic's sheen
        c += vec3(0.07) * smoothstep(0.2, 0.0, abs(f.x - f.y * 0.5 - 0.3));
        if (face != 1) {
            float band = aa(abs(f.y - 0.04) - 0.2, px);
            c = mix(c, B, band);
            c = mix(c, C, aa(abs(f.y - 0.04) - 0.2, px) * aa(abs(f.y + 0.1) - 0.04, px));
            if (face == 0) c = mix(c, C, bars(f, vec2(0.1, 0.14), vec2(0.5, 0.06), 1, seed, px) * band);
            c = mix(c, mix(c, vec3(0.95), 0.4), aa(0.86 - f.y, px));
        } else {
            float st = circ(f, vec2(0.48, 0.48), 0.28);
            c = mix(c, B, aa(st, px));
            c = mix(c, C, aa(circ(f, vec2(0.48, 0.48), 0.12), px));
        }
        gloss = 0.6;
    } else if (fam == 3) {
        // APPLES: green apples in a kraft tray, a round green label
        if (face == 1) {
            vec2 q = f;
            vec2 qc = sign(q + 1e-4) * 0.5;
            float cov = 0.0;
            vec3 ap = sphere(q, qc, 0.47, mix(A, A * vec3(1.15, 1.05, 0.7), hash12(qc + seed)), px, cov);
            c = mix(B * 0.55, ap, cov);
            c = mix(c, vec3(0.3, 0.2, 0.1), aa(circ(q, qc + vec2(0.05, 0.08), 0.05), px) * cov);
        } else {
            c = B * (0.94 + 0.06 * sin(f.x * 40.0));
            if (face == 0) {
                float lab = circ(f, vec2(0.0, 0.08), 0.48);
                c = mix(c, A, aa(lab, px));
                vec2 lp = (f - vec2(0.05, 0.12)) * mat2(0.8, -0.6, 0.6, 0.8);
                float leaf = length(lp / vec2(0.3, 0.14)) - 1.0;
                c = mix(c, C, aa(leaf * 0.14, px));
                c = mix(c, C * 0.8, aa(abs(lp.y) - 0.012, px) * aa(abs(lp.x) - 0.25, px));
                c = mix(c, C, bars(f, vec2(0.0, -0.66), vec2(0.5, 0.1), 1, seed, px));
            } else {
                c = mix(c, A, aa(abs(f.y - 0.55) - 0.1, px));
            }
            c = mix(c, A * 0.9, aa(0.9 - f.y, px));
        }
        gloss = 0.12;
    } else if (fam == 4) {
        // TINS: a shrink-wrapped pack of tins, red labels, a white stripe, silver rims; lids on top
        if (face == 1) {
            vec2 qc = sign(f + 1e-4) * 0.5;
            float r = length(f - qc);
            c = mix(A * 0.25, C, aa(r - 0.47, px));
            c *= 0.85 + 0.15 * sin(r * 70.0);
            c = mix(c, C * 0.7, aa(abs(r - 0.38) - 0.02, px));
            gloss = 0.7;
        } else {
            float h = f.x < 0.0 ? (f.x + 0.5) / 0.5 : (f.x - 0.5) / 0.5;
            float sh = 0.55 + 0.45 * sqrt(max(1.0 - h * h, 0.0));
            float cx = f.x < 0.0 ? -0.5 : 0.5;
            c = A;
            float wave = 0.33 + 0.04 * sin(f.x * 9.0 + seed);
            c = mix(c, B, aa(abs(f.y - wave) - 0.07, px));
            c = mix(c, B, aa(circ(f, vec2(cx, -0.22), 0.25), px));
            c = mix(c, A, aa(circ(f, vec2(cx, -0.24), 0.16), px));
            c = mix(c, vec3(0.25, 0.5, 0.22), aa(rbox(f - vec2(cx, -0.07), vec2(0.06, 0.035), 0.03), px));
            float rim = aa(0.78 - abs(f.y), px);
            c = mix(c, C * (0.85 + 0.15 * sin(f.y * 120.0)), rim);
            c *= sh;
            gloss = mix(0.3, 0.8, rim);
        }
    } else if (fam == 5) {
        // PASTA: a cobalt box with a window onto the pasta, a white band
        if (face == 0) {
            c = A;
            float win = rbox(f - vec2(0.0, -0.25), vec2(0.56, 0.38), 0.14);
            float s = fract((f.x + f.y * 0.6) * 5.0 + 0.25 * sin(f.y * 18.0));
            vec3 pasta = C * (0.75 + 0.35 * smoothstep(0.1, 0.45, s) * (1.0 - smoothstep(0.55, 0.9, s)));
            c = mix(c, pasta, aa(win, px));
            c = mix(c, B, aa(abs(win) - 0.025, px));
            c = mix(c, B, aa(abs(f.y - 0.46) - 0.15, px));
            c = mix(c, A, bars(f, vec2(0.0, 0.46), vec2(0.55, 0.1), 1, seed, px));
            c = mix(c, B, aa(circ(f, vec2(-0.66, 0.82), 0.1), px));
        } else if (face == 2) {
            c = A;
            c = mix(c, B, aa(abs(f.y - 0.46) - 0.15, px));
            c = mix(c, B * 0.9, bars(f, vec2(0.0, -0.3), vec2(0.5, 0.3), 3, seed, px) * 0.5);
        } else {
            c = A * 0.95;
        }
        gloss = 0.25;
    } else {
        // JUICE: an orange carton, a slice of orange, a leaf
        if (face == 1) {
            c = B * (f.y > 0.0 ? 1.0 : 0.86);
            c = mix(c, A * 0.8, aa(abs(f.y) - 0.04, px) * 0.6);
            c = mix(c, C, aa(circ(f, vec2(-0.42, 0.42), 0.2), px));
            c = mix(c, C * 1.3, aa(circ(f, vec2(-0.42, 0.42), 0.12), px));
        } else if (face == 0) {
            c = A;
            vec2 d = f - vec2(0.0, -0.12);
            float r = length(d);
            c = mix(c, A * 0.82, aa(r - 0.56, px));
            c = mix(c, B, aa(r - 0.5, px));
            vec3 flesh = mix(A, vec3(1.0, 0.85, 0.35), 0.35);
            c = mix(c, flesh, aa(r - 0.44, px));
            float ang = atan(d.y, d.x);
            float seg = abs(fract(ang / 6.28318 * 8.0 + 0.5) - 0.5) * r * 6.28 / 8.0;
            c = mix(c, B, aa(seg - 0.012, px) * aa(r - 0.44, px) * step(0.06, r));
            c = mix(c, B, aa(r - 0.05, px));
            vec2 lp = (f - vec2(0.44, 0.5)) * mat2(0.6, -0.8, 0.8, 0.6);
            c = mix(c, C, aa((length(lp / vec2(0.24, 0.1)) - 1.0) * 0.1, px));
            c = mix(c, B, aa(0.72 - f.y, px));
            c = mix(c, A, bars(f, vec2(-0.1, 0.86), vec2(0.5, 0.08), 1, seed, px));
        } else {
            c = A;
            c = mix(c, B, aa(abs(f.x) - 0.3, px));
            c = mix(c, A * 0.85, bars(f, vec2(0.0, -0.1), vec2(0.24, 0.4), 4, seed, px) * aa(abs(f.x) - 0.3, px));
            c = mix(c, B, aa(0.72 - f.y, px));
        }
        gloss = 0.35;
    }
    return c;
}



// The window's light on the counter: a slanted band through blinds.
float windowLight(vec2 p) {
    vec2 r = mat2(0.87, 0.5, -0.5, 0.87) * p;
    float band = smoothstep(-15.0, -10.0, r.x) * (1.0 - smoothstep(4.0, 10.0, r.x));
    float blinds = 0.55 + 0.45 * smoothstep(0.15, 0.45, abs(fract(r.y / 2.6) - 0.5) * 2.0);
    return band * blinds;
}

vec3 shade(vec3 P, vec3 N, vec3 alb, float spec, float rough) {
    vec3 V = normalize(uEye - P);
    vec3 c = alb * mix(uAmbBot, uAmbTop, 0.5 + 0.5 * N.z);
    float win = uPatch > 0.0 ? windowLight(P.xy + P.z * vec2(0.5, 0.3)) : 0.0;
    vec3 kc = uKeyCol * (1.0 - 0.35 * uPatch + 1.1 * uPatch * win);
    float nk = max(dot(N, uKeyDir), 0.0);
    c += alb * kc * nk;
    vec3 H = normalize(uKeyDir + V);
    c += kc * pow(max(dot(N, H), 0.0), rough) * spec * 0.5;
    for (int i = 0; i < 16; i++) {
        if (i >= uNL) break;
        vec3 L = uLP[i].xyz - P;
        float d2 = dot(L, L);
        float r = uLP[i].w;
        if (d2 > r * r) continue;
        float d = sqrt(d2);
        L /= d;
        float att = 1.0 / (1.0 + d2 / (r * r * 0.08)) * (1.0 - smoothstep(0.4 * r, r, d));
        float nl = max(dot(N, L), 0.0) * 0.85 + 0.15;
        c += alb * uLC[i].rgb * uLC[i].a * att * nl;
        vec3 Hl = normalize(L + V);
        c += uLC[i].rgb * uLC[i].a * att * pow(max(dot(N, Hl), 0.0), rough) * spec * 0.5;
    }
    return c;
}

void main() {
    int mat = int(vMat.x + 0.5);
    vec2 uv, sz;
    vec3 N;
    int face; // 0 x sides, 1 y sides, 2 z ends (2 with vLN.z > 0: the top, seen from above)
    if (abs(vLN.x) > 0.5) { uv = vec2(-vL.z * sign(vLN.x), vL.y); sz = vHalf.zy; N = vAX * sign(vLN.x); face = 0; }
    else if (abs(vLN.y) > 0.5) { uv = vec2(vL.x * sign(vLN.y), vL.z); sz = vHalf.xz; N = vAY * sign(vLN.y); face = 1; }
    else { uv = vec2(vL.x, vL.y); sz = vHalf.xy; N = vAZ * sign(vLN.z); face = 2; }
    bool top = face == 2 && vLN.z > 0.0;
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.1, rough = 20.0;
    float seed = vCol.a;
    float px = max(fwidth(q.x), fwidth(q.y));
    float gk = vGroup >= 0.0 ? uGroupK[int(vGroup + 0.5)] : 1.0;
    if (mat == 6) {
        // a product (vMat.y its family, vMat.z the scanner's flash): boxes lie on their backs (their front up),
        // cartons, tins, punnets and trays stand (their tops up)
        int fam = int(vMat.y + 0.5);
        int pf = top ? ((fam == 1 || fam == 5) ? 0 : 1) : (face == 2 ? 3 : 2);
        vec2 f = uv;
        float pxn = max(fwidth(f.x), fwidth(f.y)) * 1.2;
        float gl;
        vec3 c = product(fam, pf, f, max(pxn, 0.004), floor(seed * 10.0), gl);
        c = mix(c, uFA[fam] * 0.9 + uFB[fam] * 0.1, smoothstep(0.08, 0.3, pxn));
        float e = 1.0 - max(abs(f.x), abs(f.y));
        c *= 0.8 + 0.2 * smoothstep(0.0, 0.1, e);
        if (top) c += c * 0.12 * (1.0 - smoothstep(0.0, 0.08, e)) * step(0.0, f.y - f.x);
        alb = c * vCol.rgb;
        spec = gl;
        rough = 36.0;
        emit += vec3(1.0, 0.3, 0.25) * vMat.z * 0.6 + vec3(1.0) * vMat.z * vMat.z * 0.25;
    } else if (mat == 7) {
        // a bag's paper or fabric (vMat.y 0 kraft, 1 fabric (its color), 2 white; vMat.z 1: the inside, in shade)
        int st = int(vMat.y + 0.5);
        vec3 base = st == 0 ? uKraft : st == 1 ? vCol.rgb : vec3(0.9, 0.89, 0.86);
        alb = base * (0.93 + 0.07 * vnoise(q * 3.0 + seed * 9.0));
        if (st == 1) alb *= 0.94 + 0.06 * step(0.5, fract((q.x + q.y) * 4.0));
        if (vMat.z > 0.5) alb *= 0.45;
        // creases
        alb *= 1.0 - 0.1 * (1.0 - smoothstep(0.0, 0.05 + px, abs(fract(q.x * 0.9 + seed) - 0.5) - 0.45));
        if (top) alb *= 1.08;
        spec = 0.04;
        rough = 8.0;
    } else if (mat == 8) {
        // wire: a chrome grid, the top open
        if (top) discard;
        float sp = 0.4;
        vec2 g = abs(fract(q / sp) - 0.5) * sp;
        float w = 0.045;
        float wire = max(aa(g.x - w, px), aa(g.y - w, px));
        float rim = aa(min(sz.x - abs(q.x), sz.y - abs(q.y)) - 0.1, px);
        float cov = max(wire, rim);
        if (cov < 0.5) discard;
        alb = vCol.rgb * (0.85 + 0.15 * rim);
        spec = 0.9;
        rough = 60.0;
    } else if (mat == 2) {
        // the counter's top (vMat.y 0 a speckled laminate, 1 oak boards, 2 brushed steel)
        int st = int(vMat.y + 0.5);
        vec2 g = vP.xy;
        if (st == 1) {
            float board = floor(g.x / 3.1);
            float h = hash12(vec2(board, 3.0));
            alb *= 0.86 + 0.14 * h;
            alb *= 0.9 + 0.1 * vnoise(vec2(g.x * 2.0, g.y * 0.12) + board * 7.0);
            alb *= 0.93 + 0.07 * sin(g.y * 1.7 + vnoise(g * vec2(0.6, 0.15)) * 6.0 + h * 20.0);
            alb *= 1.0 - 0.3 * aa(abs(fract(g.x / 3.1) - 0.5) * 3.1 - 1.52, px);
            spec = 0.12;
            rough = 14.0;
        } else if (st == 2) {
            alb *= 0.9 + 0.1 * vnoise(vec2(g.x * 0.4, g.y * 30.0));
            spec = 0.6;
            rough = 40.0;
        } else {
            alb *= 0.97 + 0.03 * vnoise(g * 9.0);
            alb *= 1.0 - 0.08 * step(0.93, hash12(floor(g * 9.0)));
            alb *= 0.98 + 0.02 * fbm(g * 0.3);
            spec = 0.18;
            rough = 18.0;
        }
        spec *= uGloss * 2.0;
    } else if (mat == 3) {
        // the belt's rubber: ribs across it, running down toward the scanner
        if (top) {
            float s = fract((vP.y + uBeltPhase) / 0.7);
            alb = vCol.rgb * (0.7 + 0.5 * aa(abs(s - 0.5) - 0.09, px * 1.4));
            alb *= 0.92 + 0.08 * vnoise(vP.xy * vec2(0.5, 4.0));
            spec = 0.2;
            rough = 14.0;
        } else {
            alb = vec3(0.5);
        }
    } else if (mat == 1) {
        // brushed steel
        alb = vCol.rgb * (0.9 + 0.1 * vnoise(vec2(q.x * 0.4, q.y * 30.0) + seed * 7.0));
        spec = 0.8;
        rough = 50.0;
    } else if (mat == 4) {
        emit = vCol.rgb * vMat.y * gk;
        alb = vCol.rgb * 0.1;
        spec = 0.0;
    } else if (mat == 14) {
        // a screen or a keypad on top (vMat.y 0 a screen, 1 keys)
        if (top) {
            if (vMat.y < 0.5) {
                float scr = rbox(q, sz - 0.2, 0.06);
                float m = aa(scr, px);
                alb = mix(alb, vec3(0.02), m);
                vec2 f = q / sz;
                float rows = bars(f, vec2(-0.25, 0.25), vec2(0.5, 0.45), 4, seed * 7.0, 0.04);
                float tot = aa(rbox(f - vec2(0.45, -0.55), vec2(0.3, 0.12), 0.04), 0.04);
                emit += (vec3(0.5, 0.68, 0.82) * 0.55 + vec3(0.9) * rows * 0.5 + uAccent * tot * 0.8) * m * gk;
            } else {
                vec2 g = abs(fract(q / 0.42) - 0.5) * 0.42;
                float key = aa(max(g.x, g.y) - 0.15, px) * aa(max(abs(q.x) - sz.x + 0.15, abs(q.y) - sz.y + 0.15), px);
                alb = mix(alb, alb * 2.2 + 0.05, key);
            }
        }
        spec = 0.3;
    } else if (mat == 15) {
        // wood
        alb *= 0.88 + 0.12 * vnoise(vec2(q.x * 0.6, q.y * 9.0) + seed * 5.0);
        spec = 0.08;
    } else if (mat == 20) {
        // the scanner's window: dark glass, its red laser lines glowing
        alb = vec3(0.03, 0.03, 0.035);
        if (top) {
            vec2 f = uv;
            float l1 = abs(f.x * 0.6 + f.y * 0.8), l2 = abs(f.x * 0.6 - f.y * 0.8), l3 = abs(f.y), l4 = abs(f.x);
            float pxn = max(fwidth(f.x), fwidth(f.y)) + 0.01;
            float las = max(max(aa(l1 - 0.02, pxn), aa(l2 - 0.02, pxn)), max(aa(l3 - 0.015, pxn), aa(l4 - 0.015, pxn)) * 0.6);
            float glow = exp(-min(min(l1, l2), l3) * 7.0) * 0.4;
            emit += vec3(1.0, 0.12, 0.08) * (las + glow) * vMat.y * gk;
            spec = 1.0;
            rough = 90.0;
        }
    } else if (mat == 21) {
        // paper (vMat.y 0 the receipt (vMat.z: where it is along the strip), 1 a shopping list, 2 a card)
        int st = int(vMat.y + 0.5);
        alb = vCol.rgb * (0.97 + 0.03 * vnoise(q * 6.0));
        if (face == 2) {
            if (st == 0) {
                float s = vMat.z + q.y;
                float row = floor(s / 0.3);
                float rv = fract(s / 0.3);
                float len = 0.25 + 0.6 * hash12(vec2(row, 7.0));
                float x = q.x / sz.x;
                float txt = step(0.3, rv) * step(rv, 0.62) * step(-0.8, x) * step(x, -0.8 + 1.6 * len);
                float price = step(0.3, rv) * step(rv, 0.62) * step(0.55, x) * step(x, 0.82) * step(0.2, hash12(vec2(row, 3.0)));
                float dash = step(0.92, hash12(vec2(row, 11.0))) * step(0.42, rv) * step(rv, 0.5) * step(0.5, fract(x * 8.0));
                alb = mix(alb, vec3(0.32, 0.32, 0.34), max(max(txt, price), dash) * 0.75 * (1.0 - smoothstep(0.03, 0.08, px)));
            } else if (st == 1) {
                float lines = aa(abs(fract(q.y / 0.5) - 0.5) - 0.48, px) * step(q.y, sz.y - 0.6);
                alb = mix(alb, vec3(0.6, 0.72, 0.9), lines * 0.6);
                float row = floor(q.y / 0.5);
                float rv = fract(q.y / 0.5);
                float len = 0.3 + 0.55 * hash12(vec2(row, seed));
                float x = q.x / sz.x;
                float hand = aa(abs(rv - 0.62 - 0.08 * sin(q.x * 9.0 + row)) - 0.05, px) * step(-0.75, x) * step(x, -0.75 + 1.5 * len) * step(q.y, sz.y - 0.8);
                alb = mix(alb, vec3(0.15, 0.22, 0.45), hand * 0.85);
                float tick = aa(circ(vec2(x, rv), vec2(-0.86, 0.6), 0.06), px / sz.x) * step(0.5, hash12(vec2(row, 2.0))) * step(q.y, sz.y - 0.8);
                alb = mix(alb, vec3(0.15, 0.22, 0.45), tick);
            } else {
                alb = mix(alb, vec3(0.95), aa(abs(q.y + sz.y * 0.35) - sz.y * 0.12, px));
                alb = mix(alb, vec3(0.85, 0.75, 0.4), aa(rbox(q - vec2(-sz.x * 0.55, sz.y * 0.25), vec2(sz.x * 0.18, sz.y * 0.2), 0.05), px));
            }
        }
        spec = st == 2 ? 0.4 : 0.03;
    } else if (mat == 22) {
        // a disc seen from above (vMat.y 0 a cup's lid, 1 a dark lens)
        if (!top) discard;
        float r = length(uv);
        if (r > 1.0) discard;
        int st = int(vMat.y + 0.5);
        float pxn = max(fwidth(uv.x), fwidth(uv.y));
        if (st == 0) {
            alb = vCol.rgb * (0.9 + 0.1 * smoothstep(0.7, 0.75, r));
            alb *= 1.0 - 0.2 * aa(abs(r - 0.82) - 0.03, pxn);
            alb = mix(alb, vec3(0.08), aa(rbox(uv - vec2(0.0, 0.55), vec2(0.16, 0.06), 0.05), pxn));
            spec = 0.3;
        } else {
            alb = vec3(0.04, 0.05, 0.06);
            alb += vec3(0.25) * pow(clamp(1.0 - length(uv - vec2(-0.35, 0.4)) * 2.0, 0.0, 1.0), 3.0);
            spec = 1.0;
            rough = 80.0;
        }
        alb *= 1.0 - 0.3 * (1.0 - smoothstep(0.0, pxn * 2.0 + 0.02, 1.0 - r));
    } else {
        // paint, plastic, leather (vMat.y its gloss)
        alb *= 0.96 + 0.04 * vnoise(q * 1.3 + seed * 5.0);
        spec = 0.1 + vMat.y;
        rough = 24.0 + 40.0 * vMat.y;
    }
    // the counter's contact shade at the foot of everything standing on it
    float ao = top ? 1.0 : mix(0.6, 1.0, smoothstep(0.0, 1.2, vP.z - uFloorZ));
    vec3 c = shade(vP, normalize(N), alb, spec, rough) * ao + emit;
    c = saturateC(c, uSat);
    frag = vec4(c, uStackA);
}
)";

// ---------------------------------------------------------------- contact shadows (quads on the counter)
inline const char* BLOB_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // center, alpha
layout(location = 2) in vec4 iR;   // half x, half y, softness
uniform mat4 uVP;
out vec2 vQ;
flat out float vA, vSoft;
void main() {
    vQ = aCorner;
    vA = iP.w;
    vSoft = iR.z;
    gl_Position = uVP * vec4(iP.xyz + vec3(aCorner.x * iR.x, aCorner.y * iR.y, 0.0), 1.0);
}
)";
inline const char* BLOB_FS = R"(#version 330 core
in vec2 vQ;
flat in float vA, vSoft;
out vec4 frag;
void main() {
    vec2 q = abs(vQ);
    vec2 d = max(q - (1.0 - vSoft), 0.0) / vSoft;
    float a = vA * (1.0 - smoothstep(0.0, 1.0, length(d)));
    frag = vec4(vec3(1.0 - a), 1.0);
}
)";

// ---------------------------------------------------------------- glows (billboards facing the camera)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;
layout(location = 2) in vec4 iC;
uniform mat4 uVP;
out vec2 vC;
out vec4 vCol;
void main() {
    vC = aCorner;
    vCol = iC;
    gl_Position = uVP * vec4(iP.xyz + vec3(aCorner, 0.0) * iP.w, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
out vec4 frag;
uniform float uSoft;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 50.0);
    float halo = exp(-r2 * 6.0) * 0.35 + exp(-r2 * 2.0) * 0.1;
    float a = (core * (1.0 - uSoft) + halo) * (1.0 - r2);
    frag = vec4(vCol.rgb * vCol.a * a, 0.0);
}
)";

// ---------------------------------------------------------------- hairlines (the ghost, the scanner's beam)
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uRes;
out float vAcross;
out vec4 vCol;
out float vW;
void main() {
    vec4 c0 = uVP * vec4(iA.xyz, 1.0), c1 = uVP * vec4(iB.xyz, 1.0);
    vec2 s0 = c0.xy / c0.w * uRes * 0.5, s1 = c1.xy / c1.w * uRes * 0.5;
    vec2 dir = s1 - s0;
    dir = length(dir) > 1e-4 ? normalize(dir) : vec2(0.0, 1.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec4 c = mix(c0, c1, aCorner.x);
    float half_ = max(iA.w, 1.0) * 0.5 + 0.75;
    c.xy += nrm * aCorner.y * half_ / (uRes * 0.5) * c.w;
    vAcross = aCorner.y * half_;
    vW = iA.w;
    vCol = vec4(iC.rgb, iB.w);
    gl_Position = c;
}
)";
inline const char* LINE_FS = R"(#version 330 core
in float vAcross;
in vec4 vCol;
in float vW;
out vec4 frag;
uniform float uAdd;
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    frag = vec4(vCol.rgb * a, a * (1.0 - uAdd));
}
)";

// ---------------------------------------------------------------- the shade behind the well and the columns
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil, uRail;
uniform float uWellA, uColA, uRailA, uPx;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h) { vec2 q = abs(p - c) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.2, 0.7, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.2, 2.2, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- the glow and the frame
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThr;
void main() {
    vec4 s0 = texture(uSrc, vUv + uTexel * vec2(-1, -1)), s1 = texture(uSrc, vUv + uTexel * vec2(1, -1));
    vec4 s2 = texture(uSrc, vUv + uTexel * vec2(-1, 1)), s3 = texture(uSrc, vUv + uTexel * vec2(1, 1));
    vec3 c = s0.rgb * (1.0 - s0.a) + s1.rgb * (1.0 - s1.a) + s2.rgb * (1.0 - s2.a) + s3.rgb * (1.0 - s3.a);
    c *= 0.25;
    frag = vec4(max(c - vec3(uThr), 0.0), 0.25 * (s0.a + s1.a + s2.a + s3.a));
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
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF, uHalo;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
void main() {
    vec4 s = texture(uScene, vUv);
    vec4 b = texture(uBloom, vUv);
    vec3 c = s.rgb * uExposure + b.rgb * uBloomK;
    c += vec3(1.0, 0.98, 0.95) * uHalo * smoothstep(0.0, 0.5, b.a) * (1.0 - clamp(s.a, 0.0, 1.0));
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    c = mix(c, mix(vec3(l), c, 0.6) * uSlowTint, uSlow);
    c = c / (1.0 + 0.2 * max(max(c.r, c.g), c.b));
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.28 * dot(v, v) * 2.0;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
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
inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
uniform vec2 uRes;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.15, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";



// Builds a shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace groceries_shaders

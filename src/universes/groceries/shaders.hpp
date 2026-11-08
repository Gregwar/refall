#pragma once
// GROCERIES' shaders (see groceries.cpp).
//
// Everything solid is an instanced box (BOX_VS / BOX_FS) with a procedural material: the products and their
// packaging, paper and fabric bags, cart wire, the glossy store floor (it mirrors the ceiling's tube lights), the
// conveyor belts' rubber, gondolas full of goods, fridge doors, hanging signs, tills, asphalt with its parking bays,
// walls, vans, crates of fruit. A box can carry a birth (a growth value at which it pops into the world, from its
// base, with a small overshoot), so the static world grows on the GPU without being rebuilt. SKY_FS paints the sky
// where nothing stands; BLOB_FS lays soft contact shadows on the floor; SPRITE_FS the lights' glows; LINE_FS
// hairlines (the ghost); FLAKE_FS the drop's confetti; VEIL_FS the shade behind the well and the HUD's columns;
// DOWN / BLUR / FINAL the glow and the frame; SHAPE / TEXT the HUD.
#include <string>

namespace groceries_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform vec3 uZen, uHor, uGround, uSunDir, uSunGlow, uFogCol;
uniform float uFogD, uFogStart, uTime, uSat, uNight;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
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
vec3 skyGrad(vec3 d) {
    float e = d.y;
    vec3 c = mix(uHor, uZen, pow(smoothstep(-0.02, 0.6, e), 0.7));
    float s = max(dot(d, uSunDir), 0.0);
    c += uSunGlow * (pow(s, 8.0) * 0.6 + pow(s, 2.0) * 0.12) * smoothstep(-0.1, 0.1, e);
    // under the horizon: the far land's haze
    c = mix(c, mix(uGround, uHor, 0.55), smoothstep(0.0, -0.04, e));
    return c;
}
vec3 applyFog(vec3 c, vec3 P) {
    vec3 v = P - uEye;
    float dist = length(v);
    float f = 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD);
    vec3 fc = mix(uFogCol, skyGrad(normalize(vec3(v.x, max(v.y, -0.02 * dist) * 0.25, v.z))), 0.5);
    return mix(c, fc, clamp(f, 0.0, 0.92));
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
layout(location = 7) in vec4 iE;   // birth, channel, span, light group
uniform mat4 uVP;
uniform vec2 uGrow;
uniform vec3 uOffset;
out vec3 vP, vL;
flat out vec3 vLN, vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat;
flat out float vGroup;
void main() {
    float k = 1.0;
    if (iE.x > 0.0) {
        float g = iE.y > 0.5 ? uGrow.y : uGrow.x;
        float t = clamp((g - iE.x) / max(iE.z, 0.02), 0.0, 1.0);
        float u = t - 1.0;
        k = t <= 0.0 ? 0.0 : 1.0 + 2.2 * u * u * u + 1.2 * u * u;
    }
    vec3 Pl = iX.xyz * aPos.x + iY.xyz * aPos.y + iZ.xyz * aPos.z;
    vec3 base = -iY.xyz;
    vec3 P = iC.xyz + base + (Pl - base) * k + uOffset;
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
uniform vec3 uOutTop, uOutBot, uOutKey, uOutKeyDir, uLampCol;
uniform vec4 uStoreBox;
uniform float uLampK, uParkHalfW;
uniform int uNL;
uniform vec4 uLP[16];
uniform vec4 uLC[16];
uniform float uGroupK[16];
uniform vec3 uFA[7], uFB[7], uFC[7];
uniform vec3 uPal[8];
uniform vec3 uKraft, uAccent, uShelfEdge, uWinCol, uFridgeCol;
uniform float uFloorY, uCeilY, uTubeK, uTubeZ0, uTubeDZ, uStoreX, uStoreZ0, uStoreZ1, uGloss;
uniform float uBeltK, uStackA, uFogOn, uWinK;

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

// The glossy store floor mirrors the ceiling's rows of tubes (analytically).
vec3 floorReflection(vec3 P, out float fres) {
    vec3 Em = vec3(uEye.x, 2.0 * uFloorY - uEye.y, uEye.z);
    vec3 d = P - Em;
    vec3 V = normalize(uEye - P);
    fres = 0.04 + 0.96 * pow(1.0 - clamp(V.y, 0.0, 1.0), 5.0);
    vec3 r = mix(uAmbTop * 0.9, uAmbTop * 1.2, 0.5);
    if (uTubeK <= 0.001 || d.y <= 1e-3) return r;
    float t = (uCeilY - Em.y) / d.y;
    vec3 Q = Em + d * t;
    if (abs(Q.x) > uStoreX || Q.z > uStoreZ1 || Q.z < uStoreZ0) return r;
    float travel = length(Q - P);
    float w = 0.55 + travel * 0.012;
    float dz = abs(fract((Q.z - uTubeZ0) / uTubeDZ + 0.5) - 0.5) * uTubeDZ;
    float seg = smoothstep(0.0, 0.08, fract(Q.x / 16.0)) * (1.0 - smoothstep(0.62, 0.7, fract(Q.x / 16.0)));
    float streak = exp(-(dz * dz) / (w * w)) * seg;
    return r + uWinCol * 0.0 + vec3(1.0, 0.99, 0.96) * streak * uTubeK * 1.6;
}

// Inside the store (its lights) or out (the sky's light, the car park's lamps at night).
float insideK(vec3 P) {
    float d = min(min(uStoreBox.x - abs(P.x), P.z - uStoreBox.y), min(uStoreBox.z - P.z, uStoreBox.w - P.y));
    return smoothstep(-3.0, 3.0, d);
}
vec3 shade(vec3 P, vec3 N, vec3 alb, float spec, float rough) {
    vec3 V = normalize(uEye - P);
    float ins = insideK(P);
    vec3 top = mix(uOutTop, uAmbTop, ins), bot = mix(uOutBot, uAmbBot, ins);
    vec3 kc = mix(uOutKey, uKeyCol, ins), kd = normalize(mix(uOutKeyDir, uKeyDir, ins));
    vec3 c = alb * mix(bot, top, 0.5 + 0.5 * N.y);
    float nk = max(dot(N, kd), 0.0);
    c += alb * kc * nk;
    vec3 H = normalize(kd + V);
    c += kc * pow(max(dot(N, H), 0.0), rough) * spec * 0.5;
    if (uLampK > 0.001 && ins < 0.99 && P.z > 40.0 && P.y < uFloorY + 50.0) {
        // pools of light under the car park's lamps
        vec2 g = vec2(P.x + uParkHalfW - 65.0, P.z - 135.0);
        vec2 nearest = vec2(round(g.x / 130.0) * 130.0, clamp(round(g.y / 140.0), 0.0, 3.0) * 140.0);
        vec2 d = g - nearest;
        float inPark = step(abs(P.x), uParkHalfW) * step(P.z, 660.0);
        float pool = exp(-dot(d, d) / 1800.0) * inPark * (1.0 - smoothstep(0.0, 50.0, P.y - uFloorY));
        c += alb * uLampCol * uLampK * pool * (0.4 + 0.6 * max(N.y, 0.0)) * (1.0 - ins);
    }
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
    vec3 T, B, N;
    int face; // 0 x sides, 1 top / bottom, 2 z ends
    if (abs(vLN.x) > 0.5) { uv = vec2(-vL.z * sign(vLN.x), vL.y); sz = vHalf.zy; T = vAZ; B = vAY; N = vAX * sign(vLN.x); face = 0; }
    else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; N = vAY * sign(vLN.y); face = 1; }
    else { uv = vec2(vL.x * sign(vLN.z), vL.y); sz = vHalf.xy; T = vAX; B = vAY; N = vAZ * sign(vLN.z); face = 2; }
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.1, rough = 20.0;
    float seed = vCol.a;
    float px = max(fwidth(q.x), fwidth(q.y));
    float gk = vGroup >= 0.0 ? uGroupK[int(vGroup + 0.5)] : 1.0;
    float ao = 1.0;
    bool up = face == 1 && vLN.y > 0.0;
    if (mat == 6) {
        // a product (vMat.y its family, vMat.z the scanner's flash)
        int fam = int(vMat.y + 0.5);
        int pf = face == 1 ? (up ? 1 : 3) : (face == 0 ? 2 : 0);
        vec2 f = uv;
        float pxn = max(fwidth(f.x), fwidth(f.y)) * 1.2;
        float gl;
        vec3 c = product(fam, pf, f, max(pxn, 0.004), floor(seed * 10.0), gl);
        // far away: the packaging's average color
        c = mix(c, uFA[fam] * 0.9 + uFB[fam] * 0.1, smoothstep(0.08, 0.3, pxn));
        // a soft bevel: edges a touch darker, the top edge of the walls catching the light
        float e = 1.0 - max(abs(f.x), abs(f.y));
        c *= 0.8 + 0.2 * smoothstep(0.0, 0.1, e);
        if (pf != 1) c += c * 0.25 * (1.0 - smoothstep(0.0, 0.06, 1.0 - f.y));
        alb = c * vCol.rgb;
        spec = gl;
        rough = 36.0;
        emit += vec3(1.0, 0.25, 0.2) * vMat.z * 0.9 + vec3(1.0) * vMat.z * vMat.z * 0.3;
    } else if (mat == 7) {
        // a bag: 0 kraft paper, 1 fabric (its color), 2 white paper; vMat.z how full (its top shows the goods)
        int st = int(vMat.y + 0.5);
        vec3 base = st == 0 ? uKraft : st == 1 ? vCol.rgb : vec3(0.9, 0.89, 0.86);
        vec2 f = uv;
        float pxn = max(fwidth(f.x), fwidth(f.y)) * 1.2 + 0.003;
        if (face == 1 && up) {
            vec2 g = (f * 0.5 + 0.5) * vec2(3.0, 2.0);
            vec2 id = floor(g);
            vec2 l = fract(g) - 0.5;
            float h = hash12(id + seed * 17.0);
            int k = int(h * 7.0);
            vec3 pc = uFA[k];
            float has = step(h * 0.7 + 0.15, vMat.z);
            float prod = aa(rbox(l, vec2(0.4, 0.38), 0.08), pxn * 3.0) * has;
            alb = mix(base * 0.22, pc * (0.8 + 0.2 * hash12(id + 3.0)), prod);
            alb = mix(alb, base * 0.9, aa(0.92 - max(abs(f.x), abs(f.y)), pxn));
        } else {
            alb = base * (0.94 + 0.06 * vnoise(q * 3.0 + seed * 9.0));
            // the folded rim
            alb = mix(alb, base * 0.82, aa(0.84 - f.y, pxn) * aa(f.y - 0.9, pxn));
            alb *= 1.0 - 0.12 * aa(abs(f.y + 0.84) - 0.012, pxn);
            if (face == 0) {
                // the gusset: a crease down the middle, a V at the foot
                float cr = min(abs(f.x), abs(abs(f.x) - (f.y + 1.0) * 0.6));
                alb *= 1.0 - 0.14 * aa(cr - 0.015, pxn) * step(f.y, 0.84);
            } else if (st == 0) {
                // a stamped round mark, a leaf
                float r = circ(f, vec2(0.0, 0.0), 0.36);
                alb = mix(alb, uAccent * 0.9, aa(abs(r) - 0.03, pxn) * 0.85);
                vec2 lp = f * mat2(0.8, -0.6, 0.6, 0.8);
                alb = mix(alb, uAccent * 0.9, aa((length(lp / vec2(0.2, 0.09)) - 1.0) * 0.09, pxn) * 0.85);
            } else if (st == 1) {
                alb = mix(alb, mix(base, vec3(1.0), 0.65), aa(abs(f.y + 0.1) - 0.2, pxn));
                alb = mix(alb, base, aa(circ(f, vec2(0.0, -0.1), 0.13), pxn));
            } else {
                alb = mix(alb, uAccent, aa(abs(f.y + 0.45) - 0.06, pxn));
                alb = mix(alb, uAccent, aa(circ(f, vec2(0.0, 0.15), 0.2), pxn));
            }
        }
        spec = 0.04;
        rough = 8.0;
    } else if (mat == 8) {
        // cart wire: a chrome grid, the top open
        if (face == 1 && up) discard;
        float sp = 0.5;
        vec2 g = abs(fract(q / sp) - 0.5) * sp;
        float w = 0.05;
        float wire = max(aa(g.x - w, px), aa(g.y - w, px));
        float rim = aa(min(sz.x - abs(q.x), sz.y - abs(q.y)) - 0.12, px);
        float cov = max(wire, rim);
        // far away the wire reads as a light mesh
        cov = mix(cov, 0.5, smoothstep(0.05, 0.25, px));
        float dither = hash12(gl_FragCoord.xy);
        if (cov < 0.5 * (0.25 + 0.75 * dither)) discard;
        alb = vCol.rgb * (0.85 + 0.15 * rim);
        spec = 0.9;
        rough = 60.0;
    } else if (mat == 2) {
        // the store floor: tiles, a speckle, the gloss mirroring the tubes
        vec2 g = vP.xz;
        float ts = 6.0;
        vec2 tid = floor(g / ts);
        vec2 tf = abs(fract(g / ts) - 0.5) * ts;
        float ck = mod(tid.x + tid.y, 2.0);
        alb = vCol.rgb * (0.86 + 0.14 * ck) * (0.96 + 0.04 * hash12(tid));
        alb *= 0.96 + 0.06 * vnoise(g * 2.3);
        alb *= 1.0 - 0.18 * aa(ts * 0.5 - max(tf.x, tf.y) - 0.04, px);
        float fres;
        vec3 refl = floorReflection(vP, fres);
        emit += refl * fres * uGloss * vMat.y * gk;
        spec = 0.0;
    } else if (mat == 3) {
        // conveyor belt rubber, its ribs running (vMat.y its speed)
        if (face == 1 && up) {
            float s = fract((q.x - uTime * vMat.y * uBeltK) / 0.8);
            alb = vCol.rgb * (0.85 + 0.25 * aa(abs(s - 0.5) - 0.06, px * 1.25));
            alb *= 0.9 + 0.1 * smoothstep(sz.y, sz.y - 0.3, abs(q.y));
            spec = 0.25;
            rough = 16.0;
        } else {
            alb = vec3(0.6, 0.62, 0.64);
            spec = 0.5;
            rough = 40.0;
        }
    } else if (mat == 1) {
        // brushed stainless steel
        alb = vCol.rgb * (0.9 + 0.1 * vnoise(vec2(q.x * 0.4, q.y * 30.0) + seed * 7.0));
        spec = 0.8;
        rough = 50.0;
    } else if (mat == 4) {
        emit = vCol.rgb * vMat.y * gk;
        alb = vCol.rgb * 0.1;
        spec = 0.0;
    } else if (mat == 5) {
        // a gondola full of goods (vMat.y: 0 metal, 1 wood, 2 fridge; vMat.z: palette shift)
        int st = int(vMat.y + 0.5);
        if (face != 1) {
            float u = q.x, v = q.y + sz.y;
            float detail = 1.0 - smoothstep(0.15, 0.6, px);
            if (v < 1.6) {
                alb = vCol.rgb * 0.45;
            } else if (v > sz.y * 2.0 - 0.6) {
                alb = vCol.rgb * 0.85;
            } else {
                float rowH = 4.2;
                float row = floor((v - 1.6) / rowH);
                float rv = (v - 1.6) - row * rowH;
                float slotW = 1.25 + 0.5 * hash12(vec2(row, seed * 31.0 + float(face)));
                float sid = floor(u / slotW);
                float su = fract(u / slotW);
                float grp = floor(sid / (2.0 + floor(hash12(vec2(row, seed)) * 4.0)));
                float hg = hash12(vec2(grp, row + seed * 13.0 + vMat.z));
                vec3 pc = uPal[int(hg * 7.99)];
                float ph = 2.0 + 1.4 * hash12(vec2(grp + 7.0, row));
                float isProd = step(0.35, rv) * step(rv, 0.35 + ph) * step(0.08, su) * step(su, 0.92);
                // some shelves are half empty
                isProd *= step(0.12, hash12(vec2(sid, row + seed)));
                vec3 back = vCol.rgb * 0.38;
                vec3 prodC = pc * (0.88 + 0.12 * hash12(vec2(sid, row)));
                // a label band across the packs
                float lb = aa(abs(rv - 0.35 - ph * 0.55) - ph * 0.12, px);
                prodC = mix(prodC, mix(pc, vec3(0.97), 0.75), lb * 0.8 * detail);
                alb = mix(back, prodC, isProd * mix(0.85, 1.0, detail));
                // the shelf's edge with its price tags
                float edge = step(rv, 0.35);
                float tag = step(0.82, fract(u / 3.1 + row * 0.37)) * step(0.06, rv) * step(rv, 0.3);
                vec3 ec = mix(uShelfEdge, vec3(0.97), tag * detail);
                ec = mix(ec, vec3(0.98, 0.84, 0.25), tag * step(0.7, hash12(vec2(floor(u / 3.1), row))) * detail);
                alb = mix(alb, ec, edge);
                if (st == 2) {
                    // a fridge: the light inside, the doors' frames
                    emit += alb * uFridgeCol * 0.8 * gk * (1.0 - edge);
                    float fr = aa(abs(fract(u / 4.0) - 0.5) * 4.0 - 1.9, px);
                    alb = mix(alb, vec3(0.08), fr);
                    emit *= 1.0 - fr;
                    spec = 0.8;
                    rough = 80.0;
                }
            }
            if (st == 1) alb = mix(alb, alb * vec3(1.05, 0.95, 0.85), 0.3);
        } else {
            alb = vCol.rgb * 0.7;
        }
    } else if (mat == 9) {
        // glass: 0 the store's front glazing (sky and interior), 1 a building's windows
        float rr = vMat.y;
        vec3 V = normalize(vP - uEye);
        vec3 sky = skyGrad(reflect(V, N));
        if (rr < 0.5) {
            alb = vec3(0.05);
            emit = sky * 0.35 + uWinCol * 0.25 * uWinK;
        } else if (face == 1) {
            alb = vCol.rgb * 0.75;
        } else {
            vec2 cell = vec2(3.2, 3.6);
            vec2 g = q / cell;
            vec2 id = floor(g);
            vec2 fq = fract(g);
            float win = step(0.15, fq.x) * step(fq.x, 0.85) * step(0.2, fq.y) * step(fq.y, 0.8);
            float lit = step(hash12(id + seed * 11.0), 0.35 + 0.4 * uNight);
            alb = mix(vCol.rgb, vec3(0.06, 0.07, 0.09), win);
            emit = (sky * 0.2 * (1.0 - uNight) + uWinCol * lit * uWinK) * win * (1.0 - smoothstep(0.5, 2.0, px));
            emit += uWinCol * lit * uWinK * 0.25 * smoothstep(0.5, 2.0, px) * 0.4;
        }
        spec = 0.3;
    } else if (mat == 10) {
        // a hanging sign (vMat.y its icon, vMat.z > 0: a number in seven segments)
        vec2 f = uv;
        float pxn = max(fwidth(f.x), fwidth(f.y)) * 1.2 + 0.002;
        alb = vCol.rgb;
        if (face == 2) {
            float asp = sz.x / sz.y;
            vec2 p = vec2(f.x * asp, f.y);
            if (vMat.w > 0.5) {
                // a number: a lane's light (lit), or a price card (vMat.y < 0: red figures on the card)
                int n = int(vMat.w + 0.5);
                int d0 = n / 10, d1 = n - d0 * 10;
                float m = 0.0;
                bool price = vMat.y < -0.5;
                float ds = price ? 0.5 : 0.62;
                float dx = price ? 0.3 : 0.36;
                if (d0 > 0) {
                    m = max(m, digit((p - vec2(-dx, 0.1)) / ds, d0, 0.11, pxn / ds));
                    m = max(m, digit((p - vec2(dx, 0.1)) / ds, d1, 0.11, pxn / ds));
                } else m = digit(p / ds, d1, 0.11, pxn / ds);
                if (price) {
                    alb = mix(alb, vec3(0.75, 0.12, 0.1), m);
                    alb = mix(alb, vec3(0.75, 0.12, 0.1), aa(abs(f.y + 0.78) - 0.05, pxn) * aa(abs(f.x) - 0.7, pxn));
                } else {
                    alb = mix(alb, vec3(0.97), m);
                    emit += vec3(0.97) * m * vMat.y * gk;
                    emit += vCol.rgb * 0.25 * vMat.y * gk;
                }
            } else {
                int ic = int(vMat.y + 0.5);
                vec2 c = vec2(-asp + 1.0, 0.0);
                float disc = circ(p, c, 0.72);
                alb = mix(alb, vec3(0.97), aa(disc, pxn));
                vec2 l = p - c;
                float sh;
                if (ic == 0) sh = min(rbox(l - vec2(0.0, -0.12), vec2(0.17, 0.32), 0.08), rbox(l - vec2(0.0, 0.3), vec2(0.07, 0.14), 0.03));
                else if (ic == 1) sh = min(circ(l, vec2(0.0, -0.05), 0.3), rbox((l - vec2(0.1, 0.32)) * mat2(0.8, 0.6, -0.6, 0.8), vec2(0.12, 0.05), 0.04));
                else if (ic == 2) sh = rbox(l, vec2(0.28, 0.3), 0.06);
                else if (ic == 3) sh = max(circ(l, vec2(0.0, -0.18), 0.4), -l.y - 0.18);
                else if (ic == 4) sh = min(length(l / vec2(0.36, 0.18)) - 1.0, rbox(l - vec2(-0.36, 0.0), vec2(0.07, 0.15), 0.0)) * 0.2;
                else sh = min(circ(l, vec2(-0.13, 0.0), 0.2), circ(l, vec2(0.13, 0.0), 0.2));
                alb = mix(alb, vCol.rgb, aa(sh, pxn));
                // a line of "text" beside it
                alb = mix(alb, vec3(0.97), bars(p, vec2(0.6, 0.0), vec2(asp - 1.0, 0.3), 2, seed * 10.0, pxn) * step(c.x + 0.9, p.x));
            }
        }
        spec = 0.15;
    } else if (mat == 11) {
        // asphalt: the parking bays (vMat.y 1: a road)
        vec2 g = vP.xz;
        alb = vCol.rgb * (0.85 + 0.2 * fbm(g * 0.08)) * (0.95 + 0.05 * vnoise(g * 1.7));
        if (up) {
            float paint = 0.0;
            if (vMat.y < 0.5) {
                float lz = mod(g.y - 100.0, 140.0);
                float inRow = step(lz, 48.0) + step(92.0, lz);
                float bx = abs(fract(g.x / 26.0) - 0.5) * 26.0;
                paint = aa(12.6 - bx + 0.0, px) * inRow * 0.0 + aa(bx - 12.7 + 12.4, px) * 0.0;
                paint = aa(abs(bx - 13.0) - 0.35, px) * inRow;
                paint = max(paint, aa(abs(lz - 70.0) - 0.4, px) * step(0.5, fract(g.x / 20.0)));
            } else {
                paint = aa(abs(q.y) - 0.5, px) * step(0.5, fract(g.x / 24.0));
                paint = max(paint, aa(sz.y - abs(q.y) - 1.2, px) * aa(abs(sz.y - abs(q.y) - 2.0) - 0.4, px));
            }
            alb = mix(alb, vec3(0.86, 0.85, 0.8), paint * (1.0 - smoothstep(0.4, 1.5, px)) * 0.9);
        }
        spec = 0.05;
    } else if (mat == 12) {
        // walls: 0 the outside's cladding with the store's band, 1 inside, 2 a building
        int st = int(vMat.y + 0.5);
        if (face != 1) {
            float top = sz.y - q.y;
            if (st == 0) {
                alb *= 1.0 - 0.08 * aa(abs(fract(q.x / 8.0) - 0.5) * 8.0 - 3.9, px);
                alb = mix(alb, uAccent, step(top, 5.0) * step(1.2, top));
            } else if (st == 1) {
                alb = mix(alb, alb * 0.5, step(q.y + sz.y, 1.4));
                alb = mix(alb, uAccent * 0.85, step(top, 9.0) * step(3.0, top));
            }
        } else {
            alb *= 0.9;
        }
        spec = 0.05;
    } else if (mat == 13) {
        // vehicles: 0 a cargo box (a stripe, a round mark), 1 a cab (its windows)
        int st = int(vMat.y + 0.5);
        if (st == 1 && face != 1) {
            float wy = q.y / sz.y;
            float win = step(0.05, wy) * step(wy, 0.75) * step(0.12, 1.0 - abs(q.x / sz.x));
            if (face == 0 && abs(vMat.z) > 0.5) win *= step(0.0, q.x * sign(vMat.z));
            alb = mix(alb, vec3(0.08, 0.1, 0.13), win);
            emit += skyGrad(reflect(normalize(vP - uEye), N)) * 0.25 * win;
        } else if (st == 0 && face != 1) {
            alb = mix(alb, uAccent, aa(abs(q.y + sz.y * 0.35) - sz.y * 0.1, px));
            if (face == 0) alb = mix(alb, uAccent, aa(circ(q, vec2(0.0, sz.y * 0.25), sz.y * 0.3), px));
        }
        spec = 0.45;
        rough = 40.0;
    } else if (mat == 14) {
        // a till's screen, keys
        if (face == 2 && vLN.z > 0.0) {
            float scr = rbox(q, sz - 0.18, 0.05);
            alb = mix(alb, vec3(0.03), aa(scr, px));
            emit += uWinCol * 0.0 + vec3(0.55, 0.75, 0.9) * aa(scr, px) * (0.5 + 0.2 * step(0.0, q.y)) * gk;
            emit += vec3(0.9) * bars(q / sz, vec2(-0.2, 0.2), vec2(0.5, 0.4), 3, seed * 7.0, 0.05) * aa(scr, px) * 0.4 * gk;
        }
        spec = 0.3;
    } else if (mat == 15) {
        // wood
        alb *= 0.88 + 0.12 * vnoise(vec2(q.x * 0.6, q.y * 9.0) + seed * 5.0);
        spec = 0.08;
    } else if (mat == 16) {
        // grass (vMat.y 0 a lawn, 1 the land: a lawn near the store, a patchwork of fields beyond; 2 a hedge)
        vec2 g = vP.xz;
        float st = vMat.y;
        alb = vCol.rgb * (0.85 + 0.2 * fbm(g * 0.01)) * (0.94 + 0.06 * step(0.5, fract(g.x / 60.0)));
        if (st > 0.5 && st < 1.5 && up) {
            // fields: a turned, warped grid; soft colors; hedgerows along their edges
            vec2 rg = mat2(0.95, 0.31, -0.31, 0.95) * g;
            rg += 140.0 * vec2(fbm(g * 0.0011), fbm(g * 0.0011 + 5.3)) - 70.0;
            vec2 fg = rg / vec2(560.0, 430.0);
            vec2 fid = floor(fg);
            vec2 ff = fract(fg);
            float h = hash12(fid + 3.7);
            vec3 base = vCol.rgb;
            vec3 fc = h < 0.2 ? mix(base, vec3(0.6, 0.52, 0.26), 0.7) : h < 0.45 ? base * 0.88 : h < 0.6 ? mix(base, vec3(0.5, 0.42, 0.24), 0.5) : h < 0.85 ? base * 1.06 : mix(base, vec3(0.75, 0.66, 0.38), 0.5);
            float rows = 0.95 + 0.05 * step(0.5, fract((hash12(fid) > 0.5 ? ff.x : ff.y) * (16.0 + 24.0 * hash12(fid + 1.0))));
            fc *= rows * (0.93 + 0.1 * fbm(g * 0.004 + h * 10.0));
            vec2 ed = min(ff, 1.0 - ff) * vec2(560.0, 430.0);
            float hedge = aa(min(ed.x, ed.y) - 7.0, px * 2.0);
            fc = mix(fc, base * vec3(0.45, 0.6, 0.42), hedge * 0.9);
            float nearStore = step(abs(g.x), 2400.0) * step(-1150.0, g.y) * step(g.y, 1150.0);
            alb = mix(fc, alb, nearStore);
        } else if (st > 1.5) {
            alb *= 0.85 + 0.3 * vnoise(g * 0.08 + vP.y * 0.1);
        }
        alb *= 0.95 + 0.05 * vnoise(g * 0.4);
        spec = 0.0;
    } else if (mat == 17) {
        // the ceiling's tiles
        vec2 g = vP.xz;
        vec2 tf = abs(fract(g / 6.0) - 0.5) * 6.0;
        alb *= 1.0 - 0.15 * aa(2.96 - max(tf.x, tf.y), px);
        alb *= 0.96 + 0.04 * hash12(floor(g / 6.0));
        emit += alb * uAmbTop * 0.45 * uTubeK;
        spec = 0.0;
    } else if (mat == 18) {
        // a crate of fruit (vCol the fruit)
        if (face == 1 && up) {
            vec2 g = q / 1.6;
            vec2 id = floor(g);
            vec2 l = fract(g) - 0.5;
            float cov = 0.0;
            float h = hash12(id + seed * 9.0);
            vec3 fc = sphere(l, vec2((h - 0.5) * 0.1, 0.0), 0.46, vCol.rgb * (0.85 + 0.25 * h), px / 1.6, cov);
            alb = mix(uKraft * 0.35, fc, cov);
            alb = mix(alb, uKraft * 0.9, aa(min(sz.x - abs(q.x), sz.y - abs(q.y)) - 0.25, px));
            spec = 0.3;
        } else {
            float sl = abs(fract((q.y + sz.y) / 1.3) - 0.5);
            alb = uKraft * 1.05 * (0.9 + 0.1 * vnoise(vec2(q.x * 0.5, q.y * 6.0)));
            alb *= 1.0 - 0.55 * aa(0.5 - sl - 0.07, px);
            spec = 0.05;
        }
    } else if (mat == 19) {
        // a tower of stacked carts (vMat.y a layer's height): baskets of wire, their rims, a colored handle each
        float lay = vMat.y;
        if (face != 1) {
            float v = q.y + sz.y;
            float lv = fract(v / lay);
            float rim = aa(abs(lv - 0.04) - 0.04, px / lay);
            float handle = aa(abs(lv - 0.9) - 0.05, px / lay);
            float wires = aa(abs(fract(q.x / 3.0) - 0.5) * 3.0 - 1.35, px) * (1.0 - smoothstep(1.0, 3.0, px));
            float lid = floor(v / lay);
            vec3 hc = uPal[int(hash12(vec2(lid, seed * 9.0)) * 7.99)];
            alb = vCol.rgb * (0.82 + 0.18 * smoothstep(0.1, 0.8, lv));
            alb = mix(alb, alb * 0.72, wires * 0.6);
            alb = mix(alb, vCol.rgb * 1.2 + 0.05, rim);
            alb = mix(alb, hc, handle);
        } else {
            alb = vCol.rgb * 0.7;
        }
        spec = 0.5;
        rough = 40.0;
    } else if (mat == 20) {
        // a scanner's window: dark glass, the red laser lines glowing across it
        alb = vec3(0.03, 0.03, 0.035);
        if (face == 1 || face == 2) {
            vec2 f = uv;
            float l1 = abs(f.x * 0.6 + f.y * 0.8), l2 = abs(f.x * 0.6 - f.y * 0.8), l3 = abs(f.y);
            float pxn = max(fwidth(f.x), fwidth(f.y)) + 0.01;
            float las = max(max(aa(l1 - 0.025, pxn), aa(l2 - 0.025, pxn)), aa(l3 - 0.02, pxn));
            float glow = exp(-min(min(l1, l2), l3) * 9.0) * 0.35;
            emit += vec3(1.0, 0.1, 0.06) * (las + glow) * vMat.y * gk;
        }
        spec = 0.9;
        rough = 80.0;
    } else {
        // paint, plastic, laminate (vMat.y its gloss)
        alb *= 0.96 + 0.04 * vnoise(q * 1.3 + seed * 5.0);
        spec = 0.1 + vMat.y;
        rough = 24.0 + 40.0 * vMat.y;
    }
    // the floor's contact shade on everything that stands on it
    if (face != 1) ao = mix(0.62, 1.0, smoothstep(0.0, 7.0, vP.y - uFloorY));
    vec3 Nb = normalize(N);
    vec3 c = shade(vP, Nb, alb, spec, rough) * ao + emit;
    if (uFogOn > 0.5) c = applyFog(c, vP);
    c = saturateC(c, uSat);
    frag = vec4(c, uStackA);
}
)";

// ---------------------------------------------------------------- contact shadows (quads on the floor)
inline const char* BLOB_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // center, alpha
layout(location = 2) in vec4 iR;   // half x, half z, softness, birth
uniform mat4 uVP;
uniform vec2 uGrow;
out vec2 vQ;
flat out float vA, vSoft;
void main() {
    vQ = aCorner;
    float k = iR.w > 0.0 ? clamp((uGrow.x - iR.w) / 0.8, 0.0, 1.0) : 1.0;
    vA = iP.w * k;
    vSoft = iR.z;
    vec3 P = iP.xyz + vec3(aCorner.x * iR.x, 0.0, aCorner.y * iR.y) * (0.3 + 0.7 * k);
    gl_Position = uVP * vec4(P, 1.0);
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

// ---------------------------------------------------------------- the sky (behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uStars, uClouds;
vec3 rayDir(vec2 uv) {
    vec4 a = uInvVP * vec4(uv * 2.0 - 1.0, -1.0, 1.0), b = uInvVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(b.xyz / b.w - a.xyz / a.w);
}
void main() {
    vec3 d = rayDir(vUv);
    vec3 c = skyGrad(d);
    float e = d.y;
    float az = atan(d.x, -d.z);
    if (uStars > 0.0 && e > 0.03) {
        vec2 g = vec2(az * 220.0, e * 220.0);
        vec2 cell = floor(g);
        float h = hash12(cell);
        vec2 f = fract(g) - 0.5;
        c += vec3(0.85, 0.9, 1.0) * smoothstep(0.986, 1.0, h) * smoothstep(0.1, 0.0, length(f)) * uStars * smoothstep(0.03, 0.2, e);
    }
    if (uClouds > 0.0 && e > 0.0) {
        vec2 p = vec2(az * 2.5, 1.0 / (e + 0.1)) + vec2(uTime * 0.003, 0.0);
        float n = fbm(p * vec2(2.0, 0.5));
        float band = smoothstep(0.52, 0.8, n) * smoothstep(0.0, 0.08, e) * (1.0 - smoothstep(0.3, 0.6, e));
        vec3 cc = mix(uHor * 1.05 + uSunGlow * 0.3, uZen * 0.9 + vec3(0.05), 0.4);
        c = mix(c, cc, band * uClouds);
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- glows (billboards)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;
layout(location = 2) in vec4 iC;
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec2 vC;
out vec4 vCol;
out vec3 vP;
void main() {
    vC = aCorner;
    vCol = iC;
    vec3 c = iP.xyz;
    vec3 P = c + (uCamR * aCorner.x + uCamU * aCorner.y) * iP.w;
    P += normalize(uEye - c) * min(iP.w * 0.8, 2.0);
    vP = c;
    gl_Position = uVP * vec4(P, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
in vec3 vP;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 60.0);
    float halo = exp(-r2 * 7.0) * 0.32 + exp(-r2 * 2.2) * 0.08;
    float a = (core * 1.4 + halo) * (1.0 - r2);
    vec3 c = vCol.rgb * vCol.a * a;
    float dist = length(vP - uEye);
    c *= mix(1.0, 0.35, 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD));
    frag = vec4(c, 0.0);
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

// ---------------------------------------------------------------- the drop's confetti (paper flakes, board space)
inline const char* FLAKE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // x, y, z, size
layout(location = 2) in vec4 iC;   // color, alpha
layout(location = 3) in vec4 iR;   // angle, flip, aspect, -
uniform mat4 uVP;
out vec2 vQ;
out vec4 vCol;
out float vFlip;
void main() {
    float ca = cos(iR.x), sa = sin(iR.x);
    float fl = cos(iR.y);
    vec2 l = vec2(aCorner.x * iR.z, aCorner.y * fl) * iP.w;
    vec2 r = vec2(ca * l.x - sa * l.y, sa * l.x + ca * l.y);
    vQ = aCorner;
    vCol = iC;
    vFlip = fl;
    gl_Position = uVP * vec4(iP.xyz + vec3(r, 0.0), 1.0);
}
)";
inline const char* FLAKE_FS = R"(#version 330 core
in vec2 vQ;
in vec4 vCol;
in float vFlip;
out vec4 frag;
void main() {
    float a = vCol.a;
    vec3 c = vCol.rgb * (0.7 + 0.45 * abs(vFlip)) + vec3(0.25) * pow(abs(vFlip), 8.0);
    frag = vec4(c * a, a);
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

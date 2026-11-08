#pragma once
// LIBRARY's shaders (see library.cpp).
//
// SOLID_VS / SOLID_FS draw everything solid, instanced: boxes, cylinders and the hemisphere (domes, lamp globes),
// with their materials: wood with its grain and mouldings, plaster and wallpaper, parquet and marble floors, the
// books (cloth, gilt bands, a label, the page block on top), the shelves of books drawn procedurally (one box a
// shelf, or a whole wall of bookcases in one box: uprights, boards and spines, sorted into a rainbow when a clear
// reaches them), windows, rugs, leather, lamp shades, the coffered dome. Lit by the sky (hemisphere), a key light
// with its shadow map, up to 16 lamps, fogged toward the abyss (dark below, light above). SHADOW_FS writes the key
// light's depth. SPRITE_* are the glows (lamps, dust motes, sparks); VEIL_FS the shade behind the well and the
// HUD's columns; LINE_* screen-space lines (the ghost); DOWN / BLUR / FINAL the bloom and the frame; SHAPE / TEXT
// the HUD.
#include <string>

namespace library_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform float uTime;
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
vec3 saturateC(vec3 c, float k) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return max(mix(vec3(l), c, k), 0.0);
}
)";

// ---------------------------------------------------------------- solids (instanced meshes)
// Instance: c (center, material), x y z (half axes, params p0 p1 p2), col (color, seed), e (born, die, sort unit,
// style). Born / die are in lines: the part pops up (style 0: scaled up from its foot with a little overshoot), rises
// from below its foot (1) or comes down from above (2) as the growth passes them, and goes away the same way.
inline const char* SOLID_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
layout(location = 7) in vec4 iE;
uniform mat4 uVP;
uniform vec3 uOrigin;
uniform float uGrow, uSpan, uLift;
out vec3 vP, vL, vLN, vN;
flat out vec3 vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat, vE;
void main() {
    float born = iE.x, die = iE.y;
    float s = clamp((uGrow - born) / uSpan, 0.0, 1.0);
    float d = die > 0.0 ? clamp((uGrow - die) / uSpan, 0.0, 1.0) : 0.0;
    int style = int(iE.w + 0.5);
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    vec3 C = iC.xyz;
    vec3 P;
    if (style == 0) {
        // pop: scaled up from its foot, a little overshoot
        float e = s < 1.0 ? 1.0 + 2.2 * pow(s - 1.0, 3.0) + 1.2 * pow(s - 1.0, 2.0) : 1.0;
        float k = e * (1.0 - d * d * (3.0 - 2.0 * d));
        vec3 base = C - Y;
        P = base + (X * aPos.x + Y * (aPos.y + 1.0) + Z * aPos.z) * k;
        if (k <= 0.0) P = base;
    } else if (style == 1) {
        // rising out of the floor (sinking back into it)
        float k = s * s * (3.0 - 2.0 * s) * (1.0 - d * d * (3.0 - 2.0 * d));
        P = C + X * aPos.x + Y * aPos.y + Z * aPos.z - Y * 2.02 * (1.0 - k);
        if (k <= 0.0) P = C - Y * 2.0;
    } else {
        // coming down from above (lifted away)
        float k = s * s * (3.0 - 2.0 * s) * (1.0 - d * d * (3.0 - 2.0 * d));
        P = C + X * aPos.x + Y * aPos.y + Z * aPos.z + vec3(0.0, uLift * (1.0 - k) * (1.0 - k), 0.0);
        if (k <= 0.0) P = C;
    }
    vHalf = vec3(length(X), length(Y), length(Z));
    vAX = X / max(vHalf.x, 1e-6);
    vAY = Y / max(vHalf.y, 1e-6);
    vAZ = Z / max(vHalf.z, 1e-6);
    vN = normalize(vAX * aNrm.x / max(vHalf.x, 1e-6) + vAY * aNrm.y / max(vHalf.y, 1e-6) + vAZ * aNrm.z / max(vHalf.z, 1e-6));
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    vE = iE;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";

inline const char* LIGHTING = R"(
uniform vec3 uAmbSky, uAmbGround, uKeyDir, uKeyCol;
uniform int uNL;
uniform vec4 uLP[16];
uniform vec4 uLC[16];
uniform float uShadowOn, uShadowTexel, uShadowBias;
uniform sampler2DShadow uShadow;
uniform mat4 uShadowVP;
uniform vec3 uShadowO;
uniform float uFogD, uFogStart, uFogMid, uFogSpan;
uniform vec3 uFogLow, uFogHigh;
float shadowAt(vec3 P, vec3 N) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 s = uShadowVP * vec4(P - uShadowO + N * uShadowBias, 1.0);
    vec3 q = s.xyz / s.w * 0.5 + 0.5;
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
    float t = uShadowTexel;
    float r = 0.0;
    r += texture(uShadow, vec3(q.xy + vec2(-0.5, -0.5) * t, q.z));
    r += texture(uShadow, vec3(q.xy + vec2(0.5, -0.5) * t, q.z));
    r += texture(uShadow, vec3(q.xy + vec2(-0.5, 0.5) * t, q.z));
    r += texture(uShadow, vec3(q.xy + vec2(0.5, 0.5) * t, q.z));
    return r * 0.25;
}
vec3 shade(vec3 P, vec3 N, vec3 alb, float spec, float rough, float ao) {
    vec3 V = normalize(uEye - P);
    vec3 c = alb * mix(uAmbGround, uAmbSky, 0.5 + 0.5 * N.y) * ao;
    float nk = dot(N, uKeyDir);
    if (nk > 0.0) {
        float sh = shadowAt(P, N);
        c += alb * uKeyCol * nk * sh * mix(0.6, 1.0, ao);
        vec3 H = normalize(uKeyDir + V);
        c += uKeyCol * pow(max(dot(N, H), 0.0), rough) * spec * sh;
    }
    for (int i = 0; i < 16; i++) {
        if (i >= uNL) break;
        vec3 L = uLP[i].xyz - P;
        float d2 = dot(L, L);
        float r = uLP[i].w;
        if (d2 > r * r) continue;
        float d = sqrt(d2);
        L /= d;
        float att = 1.0 / (1.0 + d2 / (r * r * 0.05)) * (1.0 - smoothstep(0.5 * r, r, d));
        float nl = max(dot(N, L), 0.0) * 0.9 + 0.1;
        c += alb * uLC[i].rgb * uLC[i].a * att * nl * mix(0.5, 1.0, ao);
        vec3 Hl = normalize(L + V);
        c += uLC[i].rgb * uLC[i].a * att * pow(max(dot(N, Hl), 0.0), rough) * spec * 0.7;
    }
    return c;
}
vec3 fogCol(vec3 P) {
    return mix(uFogLow, uFogHigh, smoothstep(-uFogSpan, uFogSpan, P.y - uFogMid));
}
vec3 applyFog(vec3 c, vec3 P) {
    float dist = length(P - uEye);
    float f = 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD);
    return mix(c, fogCol(P), clamp(f, 0.0, 1.0));
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec3 vP, vL, vLN, vN;
flat in vec3 vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat, vE;
out vec4 frag;
uniform int uMesh;          // 0 box, 1 cylinder, 2 hemisphere
uniform float uBoard;       // 1: the well's books (no fog, no shadow)
uniform float uStackA;      // the frame's alpha (1 on the stack, for the bridge)
uniform vec4 uCut;          // a cutaway: fragments with dot(P, n) + d < 0 are dropped (uCut.w > 1e8: none)
uniform vec3 uFill[5], uRain[7];
uniform vec3 uGilt, uPaper, uSkyA, uSkyB, uSection;
uniform float uLampK, uNight, uFlicker, uMoon;
uniform sampler2D uSortTex;
uniform float uSortRate;
uniform vec3 uWaveO;
uniform float uWaveT, uWaveSpeed, uGlowAll, uBeat, uWorld;

float sortTime(float unit) {
    if (unit < 0.0) return -1.0;
    int id = int(unit + 0.5);
    float t0 = texelFetch(uSortTex, ivec2(id % 1024, id / 1024), 0).r;
    return t0 < 0.0 ? -1.0 : uTime - t0;
}
// The drop's light: a wave running out through the shelves, then a shimmer held on the beat.
float dropGlow(vec3 P, float ph) {
    float g = 0.0;
    if (uWaveT < 30.0) {
        float r = length(P - uWaveO);
        float front = uWaveT * uWaveSpeed;
        g += exp(-abs(r - front) / max(uWaveSpeed * 0.25, 0.5)) * step(r, front + uWaveSpeed * 0.6) * exp(-uWaveT * 0.12);
    }
    g += uGlowAll * (0.35 + 0.65 * pow(0.5 + 0.5 * sin(uTime * 2.3 + ph * 6.283), 3.0)) * (0.6 + 0.4 * uBeat);
    return g;
}

// A shelf of books (q: x along the shelf, y above its board, m; H the clear height). Returns the spine's color and in
// .a whether a book is there (else the dark back of the shelf); bend: the spine's roundness, gilt: its gilt, glowPh
// a phase per book.
vec4 spineAt(float x, float y, float H, float len, float seed, float r0, float r1, float sortT, out float bend, out float gilt,
             out float glowPh, out float flash) {
    float w0 = 0.042;
    float t = x / w0;
    float j = floor(t);
    float bj = j + 0.4 * (hash12(vec2(j, seed)) - 0.5);
    if (t < bj) { j -= 1.0; bj = j + 0.4 * (hash12(vec2(j, seed)) - 0.5); }
    float bn = j + 1.0 + 0.4 * (hash12(vec2(j + 1.0, seed)) - 0.5);
    float u = (t - bj) / max(bn - bj, 1e-3);
    float h1 = hash12(vec2(j * 1.7 + 0.3, seed + 3.1)), h2 = hash12(vec2(j + 0.7, seed * 1.3 + 7.7)), h3 = hash12(vec2(j * 0.37, seed + 19.0));
    float xc = (bj + bn) * 0.5 * w0;
    float rb = mix(r0, r1, clamp(xc / max(len, 1e-3), 0.0, 1.0));
    // sorted once the clear's sweep has passed this book
    float sorted = sortT >= 0.0 ? step(rb, sortT * uSortRate) : 0.0;
    flash = sortT >= 0.0 ? exp(-abs(rb - sortT * uSortRate) * 40.0) * step(rb, sortT * uSortRate + 0.02) : 0.0;
    vec3 col;
    float hb;
    float present = 1.0;
    if (sorted > 0.5) {
        float g = clamp(floor(rb * 7.0), 0.0, 6.0);
        float gf = fract(rb * 7.0);
        col = uRain[int(g)] * (0.78 + 0.3 * h2);
        hb = H * (0.94 - 0.3 * gf) * (0.97 + 0.03 * h1);
    } else {
        int k = int(min(h2 * 5.0, 4.0));
        col = uFill[k] * (0.7 + 0.45 * h3);
        hb = H * (0.6 + 0.36 * h1);
        if (h3 < 0.035) present = 0.0;
    }
    bend = u * 2.0 - 1.0;
    glowPh = h1 + h2;
    gilt = 0.0;
    if (y > hb || present < 0.5) return vec4(col, 0.0);
    // gilt bands near the head and the tail, a dark label
    float yb = y / hb;
    float px = fwidth(y) / hb;
    float bandA = 1.0 - smoothstep(0.006, 0.006 + px, abs(yb - 0.92));
    float bandB = 1.0 - smoothstep(0.006, 0.006 + px, abs(yb - 0.08));
    float bandC = (1.0 - smoothstep(0.004, 0.004 + px, abs(yb - 0.885))) * step(0.5, h1);
    float lab = step(0.62, yb) * step(yb, 0.78) * step(0.18, u) * step(u, 0.82) * step(0.3, h3);
    float detail = 1.0 - smoothstep(0.08, 0.3, fwidth(t));
    col = mix(col, col * 0.35 + vec3(0.02), lab * 0.8 * detail);
    gilt = clamp(bandA + bandB + bandC + lab * (1.0 - smoothstep(0.03, 0.03 + px * 2.0, abs(yb - 0.7))) * 0.0, 0.0, 1.0) * detail;
    // the gap between two books, darker
    float edge = min(u, 1.0 - u) * (bn - bj);
    col *= mix(1.0, 0.45 + 0.55 * smoothstep(0.0, 0.08, edge), detail);
    return vec4(col, 1.0);
}

void main() {
    if (uCut.w < 1e8 && dot(vP, uCut.xyz) + uCut.w < 0.0) discard;
    int mat = int(vMat.x + 0.5);
    // an open tube (a cylinder with p2 = 1): no caps
    if (uMesh == 1 && vMat.w > 0.5 && abs(vLN.y) > 0.5) discard;
    vec3 N = normalize(vN);
    bool back = !gl_FrontFacing;
    if (back && uMesh != 2 && mat != 15) {
        // a cut through a solid: its section
        vec3 c = uSection * (0.4 + 0.6 * max(dot(-N, uKeyDir), 0.0));
        frag = vec4(applyFog(c, vP), uStackA);
        return;
    }
    if (uMesh == 2) { if (dot(N, uEye - vP) < 0.0) N = -N; }
    else if (back) N = -N;
    vec2 uv = vec2(0.0), sz = vec2(1.0);
    vec3 T = vAX, B = vAY;
    int face = 0; // box: 0 x sides, 1 top / bottom, 2 front / back (z)
    if (uMesh == 0) {
        if (abs(vLN.x) > 0.5) { uv = vL.zy; sz = vHalf.zy; T = vAZ; B = vAY; face = 0; }
        else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; face = 1; }
        else { uv = vL.xy; sz = vHalf.xy; T = vAX; B = vAY; face = 2; }
    }
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.08, rough = 20.0, ao = 1.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    float px = max(fwidth(q.x), fwidth(q.y));
    float glow = 0.0;

    if (mat == 1) {
        // wood: grain along the longest side, a darker moulding line along the edges, raised panels (p0)
        vec2 g = sz.x > sz.y ? q : q.yx;
        float grain = fbm(vec2(g.x * 1.2, g.y * 18.0) + seed * 13.0);
        float rings = sin(g.y * 60.0 + grain * 9.0 + seed * 5.0) * 0.5 + 0.5;
        float detail = 1.0 - smoothstep(0.004, 0.03, px);
        alb *= mix(1.0, 0.82 + 0.22 * grain + 0.08 * rings, detail);
        float edge = min(sz.x - abs(q.x), sz.y - abs(q.y));
        alb *= mix(1.0, 0.72 + 0.28 * smoothstep(0.0, 0.012 + px, edge), detail);
        alb += alb * 0.25 * (1.0 - smoothstep(0.0, 0.006 + px, abs(edge - 0.018))) * detail;
        if (vMat.y > 0.5 && face == 2) {
            // a raised panel inset: its bevel catching the light
            vec2 r = sz - abs(q) - vec2(vMat.y * 0.06);
            float ins = min(r.x, r.y);
            float bev = smoothstep(0.0, 0.02, ins) * (1.0 - smoothstep(0.02, 0.04 + px, ins));
            bend = vec2(sign(q.x) * step(r.x, r.y), sign(q.y) * step(r.y, r.x)) * bev * 0.5;
            alb *= 1.0 - 0.12 * step(0.0, ins) * (1.0 - bev);
        }
        spec = 0.18; rough = 28.0;
    } else if (mat == 0) {
        // plaster, or wallpaper over a dado (p0 = 1): fine stripes and a small damask
        float n = fbm(vP.xz * 0.6 + vP.y * 0.4);
        alb *= 0.92 + 0.1 * n;
        if (vMat.y > 0.5 && face == 2) {
            vec2 w = vec2(dot(vP, vAX), vP.y);
            float st = smoothstep(0.42, 0.5, abs(fract(w.x / 0.18) - 0.5) * 2.0);
            vec2 dm = fract(w / vec2(0.36, 0.42)) - 0.5;
            float dam = smoothstep(0.17, 0.12, length(dm * vec2(1.0, 0.8)) + 0.04 * sin(atan(dm.y, dm.x) * 4.0));
            float detail = 1.0 - smoothstep(0.01, 0.05, px);
            alb = mix(alb, alb * 0.8, (st * 0.6 + dam * 0.5) * detail);
        }
        spec = 0.03;
    } else if (mat == 2 || mat == 14) {
        // books on shelves, drawn: 2 = one shelf's books (p0..p1 its share of the sort unit's run), 14 = a wall of
        // bookcases (p0 bay width, p1 shelves per bay; the wood in the instance color)
        if (face == 2 && vLN.z > 0.0) {
            float len = sz.x * 2.0, Ht = sz.y * 2.0;
            float x = q.x + sz.x, y = q.y + sz.y;
            float st = sortTime(vE.z);
            float bendS = 0.0, gilt = 0.0, ph = 0.0, flash = 0.0;
            vec4 s = vec4(0.0);
            vec3 wood = alb;
            if (mat == 2) {
                s = spineAt(x, y, Ht, len, seed * 37.0, vMat.y, vMat.z, st, bendS, gilt, ph, flash);
                if (s.a > 0.5) { alb = s.rgb; bend.x = bendS * 0.55; }
                else { alb = wood * 0.18; ao = 0.35; }
                ao *= 0.55 + 0.45 * smoothstep(Ht, Ht * 0.55, y);
            } else {
                float bayW = vMat.y, nSh = vMat.z;
                float bay = floor(x / bayW), bx = x - bay * bayW;
                float nBay = max(floor(len / bayW + 0.5), 1.0);
                float plinth = 0.14, crown = 0.16;
                float inner = Ht - plinth - crown;
                float pitch = inner / nSh;
                float yi = y - plinth;
                float sh = floor(yi / pitch), sy = yi - sh * pitch;
                float up = 0.035;  // the upright's half width
                float board = 0.028;
                float upright = 1.0 - smoothstep(up, up + px, min(bx, bayW - bx));
                bool frame = yi < 0.0 || yi > inner || upright > 0.5 || sy < board;
                if (!frame) {
                    float shelfIdx = nSh - 1.0 - sh;
                    float r0 = (bay * nSh + shelfIdx) / (nBay * nSh), r1 = (bay * nSh + shelfIdx + 1.0) / (nBay * nSh);
                    float H = pitch - board - 0.01;
                    s = spineAt(bx - up, sy - board, H, bayW - 2.0 * up, seed * 31.0 + bay * 7.13 + sh * 3.71, r0, r1, st, bendS, gilt, ph, flash);
                    if (s.a > 0.5) { alb = s.rgb; bend.x = bendS * 0.5; }
                    else { alb = wood * 0.16; }
                    // the board above shades the books' heads, the uprights their sides
                    ao = (0.5 + 0.5 * smoothstep(H + 0.02, H * 0.5, sy - board)) * (0.7 + 0.3 * smoothstep(up, up + 0.08, min(bx, bayW - bx)));
                    if (s.a < 0.5) ao *= 0.5;
                } else {
                    gilt = 0.0; flash = 0.0; ph = 0.0;
                    // the wood: lit board edges, darker joints
                    float grain = fbm(vec2(x * 2.0, y * 30.0) + seed * 5.0);
                    float detail = 1.0 - smoothstep(0.01, 0.05, px);
                    alb = wood * mix(1.0, 0.85 + 0.25 * grain, detail);
                    if (sy < board && yi > 0.0 && yi < inner && upright < 0.5) {
                        alb *= 1.12;
                        bend.y = 0.25;
                    }
                    if (yi < 0.0) alb *= 0.8 + 0.2 * smoothstep(0.0, 0.03, -yi) - 0.15 * (1.0 - smoothstep(0.0, 0.02 + px, abs(yi + plinth * 0.5)));
                    if (yi > inner) {
                        float cy = yi - inner;
                        alb *= 0.85 + 0.3 * smoothstep(0.02, 0.06, cy) * (1.0 - smoothstep(0.1, 0.14, cy));
                    }
                }
                gilt *= 1.0;
            }
            // the sort's sweep: a flash of gilt as the books fall into order
            emit += uGilt * flash * 2.5;
            alb = mix(alb, uGilt, gilt * 0.85);
            spec = 0.1 + 0.5 * gilt; rough = 30.0;
            glow = dropGlow(vP, ph) * (s.a > 0.5 ? 1.0 : 0.0);
            emit += (s.rgb * 1.3 + uGilt * (0.25 + gilt * 2.0)) * glow;
        } else {
            alb *= 0.25;
            ao = 0.5;
        }
    } else if (mat == 3) {
        // a book: the spine toward +z, the page block on top (and the fore-edge at -z), cloth covers. p0: its style,
        // p1: its glow (the drop, landing), p2: gilt page heads
        vec3 L = vL;
        vec3 h = vHalf;
        int sty = int(vMat.y + 0.5);
        float rim = 0.09 * min(h.x * 2.0, h.z * 2.0);
        float detail = 1.0 - smoothstep(0.2, 0.6, px / max(h.y, 1e-3) * 10.0);
        if (face == 2 && vLN.z > 0.0) {
            // the spine: rounded, gilt bands and a label
            float u = L.x, v = L.y;
            bend.x = u * 0.55;
            float pxv = fwidth(v);
            float g = 0.0;
            if (sty == 0) {
                g += 1.0 - smoothstep(0.02, 0.02 + pxv, abs(v - 0.8));
                g += 1.0 - smoothstep(0.012, 0.012 + pxv, abs(v - 0.88));
                g += 1.0 - smoothstep(0.02, 0.02 + pxv, abs(v + 0.8));
                g += 1.0 - smoothstep(0.012, 0.012 + pxv, abs(v + 0.88));
                float lab = step(abs(v - 0.35), 0.17) * step(abs(u), 0.72);
                alb = mix(alb, alb * 0.3 + vec3(0.015), lab);
                g += lab * (1.0 - smoothstep(0.02, 0.02 + pxv, abs(abs(v - 0.35) - 0.17)));
            } else if (sty == 1) {
                // raised bands
                for (int i = 0; i < 3; i++) {
                    float yb = -0.5 + 0.5 * float(i);
                    float d = abs(v - yb);
                    float band = 1.0 - smoothstep(0.04, 0.055 + pxv, d);
                    bend.y += sign(v - yb) * band * 0.8 * step(0.02, d);
                    g += (1.0 - smoothstep(0.01, 0.01 + pxv, abs(d - 0.05))) * 0.7;
                }
                g += 1.0 - smoothstep(0.015, 0.015 + pxv, abs(abs(v) - 0.86));
            } else if (sty == 2) {
                // a colored label panel framed in gilt, gilt head and tail
                float lab = step(abs(v - 0.3), 0.22) * step(abs(u), 0.78);
                alb = mix(alb, vec3(0.06, 0.05, 0.04) + alb * 0.15, lab);
                g += lab * (1.0 - smoothstep(0.015, 0.015 + pxv, min(0.22 - abs(v - 0.3), (0.78 - abs(u)) * h.x / h.y)));
                g += step(0.9, abs(v));
            } else {
                // gilt rules and a fleuron
                g += 1.0 - smoothstep(0.015, 0.015 + pxv, abs(abs(v) - 0.72));
                g += 1.0 - smoothstep(0.015, 0.015 + pxv, abs(abs(v) - 0.66));
                vec2 f = vec2(u * h.x / h.y, v + 0.1) * vec2(1.0, 1.0);
                g += 1.0 - smoothstep(0.06, 0.06 + pxv, length(f) + 0.03 * sin(atan(f.y, f.x) * 6.0));
                float lab = step(abs(v - 0.4), 0.12) * step(abs(u), 0.7);
                alb = mix(alb, alb * 0.35, lab);
            }
            // the cloth's weave
            alb *= 0.95 + 0.06 * vnoise(vec2(u * 60.0, v * 240.0) + seed * 9.0) * detail;
            alb = mix(alb, uGilt, clamp(g, 0.0, 1.0) * mix(0.4, 0.9, detail));
            spec = 0.12 + 0.6 * clamp(g, 0.0, 1.0); rough = 24.0 + 30.0 * clamp(g, 0.0, 1.0);
            glow = vMat.z + dropGlow(vP, seed) * uWorld;
            emit += (alb * 0.6 + uGilt * clamp(g, 0.0, 1.0) * 1.2) * glow;
        } else if (face == 1 || (face == 2 && vLN.z < 0.0)) {
            // the page block (top, bottom, fore-edge), the boards' edges around it
            float ax = h.x - abs(L.x * h.x);
            float covers = 1.0 - step(rim, ax);
            float spineSide = face == 1 ? step(h.z - rim * 1.2, L.z * h.z) : 0.0;
            float pages = 1.0 - max(covers, spineSide);
            float lines = 0.5 + 0.5 * sin(L.x * h.x * 2400.0 + seed * 3.0);
            float lp = fwidth(L.x * h.x * 2400.0);
            lines = mix(lines, 0.5, smoothstep(1.0, 3.0, lp));
            vec3 paper = uPaper * (0.88 + 0.12 * lines);
            if (vMat.w > 0.5 && face == 1) paper = mix(paper, uGilt * 1.1, 0.85);
            // the page block is set back a hair: shaded near the boards
            paper *= 0.82 + 0.18 * smoothstep(rim, rim * 2.5, ax);
            alb = mix(alb * 0.9, paper, pages);
            spec = mix(0.1, vMat.w > 0.5 ? 0.6 : 0.04, pages);
            glow = vMat.z;
            emit += uGilt * 0.25 * glow;
        } else {
            // a cover: cloth, a blind-stamped frame
            vec2 r = sz - abs(q) - vec2(min(sz.x, sz.y) * 0.12);
            float fr = 1.0 - smoothstep(0.0, max(px, 1e-4) * 1.5 + min(sz.x, sz.y) * 0.015, abs(min(r.x, r.y)));
            alb *= 0.9 - 0.12 * fr * detail;
            alb *= 0.95 + 0.06 * vnoise(q * 300.0 / max(sz.y, 0.1) * 0.1 + seed) * detail;
            spec = 0.1;
        }
        ao = 1.0;
    } else if (mat == 4) {
        // floor: parquet in herringbone (p0 = 0), or a marble checkerboard (p0 = 1)
        vec2 g = vP.xz;
        if (vMat.y < 0.5) {
            vec2 s = g / vec2(0.09, 0.09);
            float row = floor((s.x + s.y) / 4.0);
            vec2 r = vec2(s.x - s.y, s.x + s.y);
            float k = mod(floor(r.x / 1.0) + floor(r.y / 4.0), 2.0);
            vec2 pl = k > 0.5 ? vec2(fract(r.x), fract(r.y / 4.0)) : vec2(fract(r.y / 4.0), fract(r.x));
            float id = hash12(vec2(floor(r.x), floor(r.y / 4.0)) + k * 17.0);
            float detail = 1.0 - smoothstep(0.01, 0.04, px);
            float seam = smoothstep(0.0, 0.06, min(min(pl.x, 1.0 - pl.x), min(pl.y, 1.0 - pl.y) * 4.0));
            alb *= mix(1.0, (0.8 + 0.35 * id) * (0.65 + 0.35 * seam) * (0.9 + 0.15 * vnoise(vec2(pl.x * 3.0, pl.y * 30.0) + id * 30.0)), detail);
            spec = 0.35; rough = 40.0;
        } else {
            vec2 c = floor(g / 1.2);
            float k = mod(c.x + c.y, 2.0);
            float vein = fbm(g * 0.8 + k * 3.0);
            vec3 m = k > 0.5 ? vCol.rgb * 0.8 : vCol.rgb * vec3(0.32, 0.3, 0.3);
            alb = m * (0.85 + 0.2 * vein) * (1.0 - 0.6 * smoothstep(0.45, 0.5, abs(vnoise(g * 2.0 + 9.0) - 0.5)) * 0.2);
            float detail = 1.0 - smoothstep(0.02, 0.08, px);
            vec2 f = abs(fract(g / 1.2) - 0.5);
            alb *= mix(1.0, 0.85 + 0.15 * smoothstep(0.49, 0.47, max(f.x, f.y)), detail);
            spec = 0.6; rough = 70.0;
        }
    } else if (mat == 5) {
        // marble: veins
        vec2 g = uMesh == 0 ? q + seed : vec2(atan(vL.z, vL.x) * vHalf.x, vP.y);
        float v = fbm(g * 1.4);
        float vein = 1.0 - smoothstep(0.0, 0.05, abs(v - 0.5));
        alb *= 0.9 + 0.12 * fbm(g * 4.0);
        alb = mix(alb, alb * 0.6, vein * 0.5);
        if (uMesh == 1) {
            // fluting
            float a = atan(vL.z, vL.x) * 12.0 / 3.14159;
            bend.x = sin(a * 3.14159) * 0.35;
        }
        spec = 0.5; rough = 60.0;
    } else if (mat == 6) {
        // brass, gilt
        alb *= 0.9 + 0.1 * vnoise(vP.xy * 20.0);
        spec = 1.2; rough = 50.0;
        glow = dropGlow(vP, seed) * 0.3 * uWorld;
        emit += alb * glow;
    } else if (mat == 7) {
        // a light: lit by its own (p0 how bright, p1 = 1: follows the lamps' level, p2 flicker)
        float k = vMat.y * (vMat.z > 0.5 ? uLampK : 1.0) * (1.0 + vMat.w * uFlicker * (0.6 + 0.4 * sin(uTime * 13.0 + seed * 40.0)));
        emit = alb * k;
        alb = vec3(0.0);
    } else if (mat == 8) {
        // a window: its arched top (p0 = 1), mullions, the sky behind (a gradient, the moon, a few stars)
        if (face == 2) {
            vec2 r = q / sz;
            if (vMat.y > 0.5) {
                float ar = sz.x / sz.y;
                vec2 c = vec2(r.x * ar, r.y - (1.0 - ar));
                if (r.y > 1.0 - ar && length(c) > ar) discard;
            }
            float mx = abs(fract((r.x + 1.0) * 1.5) - 0.5) * 2.0;
            float my = abs(fract((r.y + 1.0) * 2.5) - 0.5) * 2.0;
            float frameW = 0.06;
            float mull = max(1.0 - smoothstep(0.9, 0.93, mx), 0.0);
            float m = step(mx, 1.0 - frameW * 2.0) * step(my, 1.0 - frameW * 3.0);
            float outer = step(sz.x - abs(q.x), 0.05) + step(sz.y - abs(q.y), 0.05);
            vec3 sky = mix(uSkyB, uSkyA, smoothstep(-1.0, 1.0, r.y));
            vec2 sm = vec2(r.x - 0.35, r.y - 0.45) * vec2(sz.x / sz.y, 1.0);
            sky += vec3(1.0, 0.97, 0.9) * smoothstep(0.1, 0.085, length(sm)) * uMoon * 2.0;
            sky += vec3(0.8, 0.85, 1.0) * step(0.996, hash12(floor(q * 90.0))) * uNight * 0.8;
            emit = sky * (1.0 - clamp(outer, 0.0, 1.0)) * m;
            alb = alb * (1.0 - m * (1.0 - clamp(outer, 0.0, 1.0)));
            spec = 0.2;
        }
    } else if (mat == 9) {
        // a rug: a border, a medallion, a field of small motifs (the instance color its field, the gilt its pattern)
        if (face == 1) {
            vec2 r = abs(q) / sz;
            vec2 d = sz - abs(q);
            float b = min(d.x, d.y);
            float border = step(b, 0.28) * step(0.04, b);
            float med = smoothstep(0.32, 0.3, length(q / sz * vec2(1.0, sz.y / sz.x)) * 1.0);
            vec2 m = fract(q * 3.0) - 0.5;
            float motif = smoothstep(0.2, 0.15, abs(m.x) + abs(m.y));
            vec3 c2 = uRain[int(mod(seed * 7.0, 7.0))] * 0.5;
            alb = mix(alb, mix(alb * 0.6, c2, 0.5), border);
            alb = mix(alb, uGilt * 0.55, motif * 0.25 * (1.0 - border));
            alb = mix(alb, c2 * 1.2, med * (1.0 - border));
            alb *= 0.9 + 0.12 * vnoise(q * 80.0);
            alb *= step(0.02, b) * 0.5 + 0.5;
        }
        spec = 0.02;
    } else if (mat == 10) {
        // leather (chairs): buttoned
        vec2 g = q * 7.0;
        vec2 f = fract(g + vec2(0.5 * mod(floor(g.y), 2.0), 0.0)) - 0.5;
        float btn = smoothstep(0.12, 0.05, length(f));
        alb *= 0.85 + 0.2 * smoothstep(0.0, 0.5, length(f));
        alb *= 1.0 - 0.4 * btn;
        bend = f * 0.6;
        spec = 0.4; rough = 26.0;
    } else if (mat == 11) {
        // a lamp shade, lit from inside (the instance color is the glass / silk)
        float k = vMat.y * uLampK;
        emit = alb * k * (0.6 + 0.4 * (face == 1 ? 0.3 : 1.0));
        alb *= 0.5;
        spec = 0.6; rough = 40.0;
    } else if (mat == 12) {
        // the dome: coffers in rings, gilt rosettes, ribs, the windows of its drum, the oculus
        vec3 s = vL;
        float el = asin(clamp(s.y, 0.0, 1.0));
        float az = atan(s.z, s.x);
        if (el > 1.36) discard;
        float nAz = 24.0;
        float a = az / 6.28318 * nAz;
        float fa = fract(a) - 0.5;
        float ringH = 0.17;
        float er = (el - 0.16) / ringH;
        float ring = floor(er);
        float fe = fract(er) - 0.5;
        vec3 base = alb;
        if (el < 0.16) {
            // the drum: arched windows between piers
            float w = step(abs(fa), 0.32) * step(el, 0.13) * step(0.025, el);
            float arch = step(length(vec2(fa / 0.32, (el - 0.10) / 0.035)), 1.0) + step(el, 0.10);
            float win = w * clamp(arch, 0.0, 1.0);
            emit += mix(uSkyB, uSkyA, el / 0.16) * win * 1.2;
            alb = mix(base, vec3(0.0), win);
        } else {
            // coffers diminishing toward the top: a stepped recess (two frames), a gilt rosette; ribs every
            // fourth column; the light falling in from the oculus brightening the top
            float cw = 0.4 * (1.0 - 0.3 * ring / 7.0);
            float pxa = fwidth(a) + fwidth(er);
            float d1 = max(abs(fa) / cw, abs(fe) / 0.4);
            float d2 = max(abs(fa) / (cw - 0.08), abs(fe) / 0.31);
            float frame1 = smoothstep(1.0, 1.0 - pxa * 3.0, d1);
            float frame2 = smoothstep(1.0, 1.0 - pxa * 3.0, d2);
            alb = base * (1.06 - 0.22 * frame1 - 0.3 * frame2);
            // the recess's lit and shaded sides (the light from above)
            float side = (frame1 - frame2) * sign(fe);
            alb *= 1.0 + 0.25 * side;
            vec2 ro = vec2(fa, fe * 0.9);
            float ros = smoothstep(0.13, 0.09, length(ro) + 0.025 * sin(atan(ro.y, ro.x) * 8.0));
            alb = mix(alb, uGilt * 1.1, ros * frame2);
            spec = 0.1 + ros * 0.8;
            float rib = 1.0 - smoothstep(0.08, 0.12 + pxa, abs(fract(a / 4.0) - 0.5) * 4.0 - 1.84);
            rib = clamp((abs(fract(a / 4.0 + 0.5) - 0.5) < 0.03) ? 1.0 : 0.0, 0.0, 1.0);
            alb = mix(alb, uGilt * 0.9, rib * 0.7);
            // the oculus' gilt ring, the light coming down from it
            alb = mix(alb, uGilt, (1.0 - smoothstep(0.02, 0.035, abs(el - 1.32))) * 0.9);
            emit += base * mix(uSkyB, vec3(1.0), 0.5) * 0.3 * smoothstep(0.5, 1.36, el) * (frame2 * 0.5 + 0.5);
        }
        // a warm wash from the lamps below, the daylight from the oculus and the drum
        emit += base * (uLampK * 0.16 * (1.0 - 0.6 * smoothstep(0.1, 1.0, el)) + mix(uSkyB, uSkyA, 0.5) * 0.08 + uAmbSky * 0.8);
        glow = dropGlow(vP, 0.0) * 0.4 * uWorld;
        emit += uGilt * glow * 0.3;
        // the coffers' 'normals' bend in local space: approximate with the tangents
        T = normalize(cross(vec3(0.0, 1.0, 0.0), N) + vec3(1e-4));
        B = normalize(cross(N, T));
    } else if (mat == 13) {
        // a ceiling with a round opening (p0 its radius around p1 p2): coffered squares
        float r = length(vP.xz - vec2(vMat.z, vMat.w));
        if (r < vMat.y) discard;
        vec2 g = fract(vP.xz / 2.0) - 0.5;
        float c = step(max(abs(g.x), abs(g.y)), 0.4);
        alb *= 1.0 - 0.2 * c;
        alb = mix(alb, uGilt, (1.0 - smoothstep(0.0, 0.25, abs(r - vMat.y - 0.3))) * 0.8);
    } else if (mat == 15) {
        // a loose page: paper, a few lines of text, bent
        alb = uPaper * 1.05;
        vec2 r = q / sz;
        float txt = step(abs(r.x), 0.75) * step(abs(r.y), 0.82) * step(0.5, fract(r.y * 12.0)) * step(0.35, hash12(vec2(floor(r.y * 12.0), floor(r.x * 4.0) + seed * 9.0)));
        alb *= 1.0 - 0.35 * txt;
        bend = vec2(r.x * 0.4, 0.0);
        emit += uPaper * (vMat.y + 0.12);
        spec = 0.05;
    } else if (mat == 16) {
        // a balustrade (a box: rail on top, a row of turned balusters drawn and cut out)
        if (face != 1) {
            float H = sz.y * 2.0;
            float y = q.y + sz.y;
            float along = uv.x * sz.x;
            float rail = step(H - 0.09, y) + step(y, 0.08);
            float f = fract(along / 0.16) - 0.5;
            float prof = 0.035 + 0.02 * sin(y / H * 6.283 * 1.5 + 1.2) + 0.012 * step(abs(y / H - 0.5), 0.05);
            float bal = step(abs(f) * 0.16, prof);
            if (rail + bal < 0.5) discard;
            bend.x = bal * (1.0 - rail) * f * 4.0;
        }
        spec = 0.3; rough = 30.0;
    } else if (mat == 17) {
        // a fire (the hearth): flames
        vec2 r = q / sz;
        float n = fbm(vec2(r.x * 3.0, r.y * 2.0 - uTime * 2.2) + seed);
        float f = smoothstep(0.2, 0.9, (1.0 - (r.y * 0.5 + 0.5)) * 1.3 + n * 0.8 - 0.6 - abs(r.x) * 0.6);
        emit = mix(vec3(1.0, 0.25, 0.04), vec3(1.0, 0.75, 0.35), f) * f * 3.0 * uLampK;
        alb = vec3(0.02);
    } else if (mat == 18) {
        // a globe: oceans, continents, a meridian
        vec3 s = normalize(vL);
        float lon = atan(s.z, s.x) + uTime * 0.05, lat = asin(clamp(s.y, -1.0, 1.0));
        float land = smoothstep(0.52, 0.56, fbm(vec2(lon * 1.6, lat * 2.2) + 3.0));
        alb = mix(vec3(0.12, 0.3, 0.42), vec3(0.62, 0.52, 0.3), land);
        spec = 0.5; rough = 40.0;
    } else if (mat == 19) {
        // iron, dark lacquer
        spec = 0.5; rough = 30.0;
    } else if (mat == 20) {
        // a book's glowing ghost (in flight trails): emissive gilt
        emit = alb * vMat.y;
        alb *= 0.2;
    }

    vec3 Nb = N;
    if (uMesh == 0 || mat == 12) Nb = normalize(N + T * bend.x + B * bend.y);
    else Nb = normalize(N + vAY * bend.y);
    vec3 c = shade(vP, Nb, alb, spec, rough, ao) + emit;
    if (uBoard < 0.5) c = applyFog(c, vP);
    frag = vec4(c, uStackA);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vP;
uniform vec4 uCut;
out vec4 frag;
void main() {
    if (uCut.w < 1e8 && dot(vP, uCut.xyz) + uCut.w < 0.0) discard;
    frag = vec4(1.0);
}
)";

// ---------------------------------------------------------------- glows (billboards): lamps, motes, sparks
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // position, radius
layout(location = 2) in vec4 iC;   // color, intensity
uniform mat4 uVP;
uniform vec3 uOrigin, uCamR, uCamU;
uniform float uMinR;
out vec2 vC;
out vec4 vCol;
out vec3 vP;
void main() {
    vC = aCorner;
    vCol = iC;
    vec3 c = iP.xyz;
    float d = length(c - uEye);
    float r = max(iP.w, d * uMinR);
    // a sprite smaller than its floor fades rather than shrinking below a pixel
    vCol.a *= min(1.0, (iP.w / r) * (iP.w / r) * 4.0);
    vec3 P = c + (uCamR * aCorner.x + uCamU * aCorner.y) * r;
    vP = c;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
in vec3 vP;
uniform float uFogD, uFogStart;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 40.0);
    float halo = exp(-r2 * 6.0) * 0.35 + exp(-r2 * 2.0) * 0.1;
    float a = (core * 1.4 + halo) * (1.0 - r2);
    vec3 c = vCol.rgb * vCol.a * a;
    float dist = length(vP - uEye);
    c *= mix(1.0, 0.25, 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD));
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- lines (screen-space): the ghost
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // x: 0 start .. 1 end, y: -1 .. 1 across
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
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    frag = vec4(vCol.rgb * a, a);
}
)";

// ---------------------------------------------------------------- the shade behind the well and the HUD's columns
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil;
uniform float uWellA, uColA;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h) { vec2 q = abs(p - c) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = box(w, vec2(0.0, 0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.3, 1.2, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.2, 2.4, dc)));
    }
    frag = vec4(uVeil * a, a);
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
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb
           + texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    float l = max(max(c.r, c.g), c.b);
    frag = vec4(c * max(l - uThr, 0.0) / max(l, 1e-4), 1.0);
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
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF, uVig;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint, uLift;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 b = texture(uBloom, vUv).rgb;
    vec3 c = s.rgb * uExposure + b * uBloomK;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    c = mix(c, mix(vec3(l), c, 0.6) * uSlowTint, uSlow);
    // a filmic shoulder that keeps the colors clean
    c = c * (1.0 + c / 9.0) / (1.0 + c);
    c = pow(c, vec3(0.92)) * 1.12;
    c += uLift * (1.0 - c);
    vec2 v = vUv - 0.5;
    c *= 1.0 - uVig * dot(v, v) * 2.0;
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
    float halo = smoothstep(0.12, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Inserts the common functions (and the lighting) after the version line.
inline std::string withCommon(const char* src, bool lighting = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (lighting ? LIGHTING : "") + s.substr(nl + 1);
}

} // namespace library_shaders

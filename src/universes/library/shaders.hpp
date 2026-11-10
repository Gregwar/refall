#pragma once
// LIBRARY's shaders (see library.cpp).
//
// SOLID_VS / SOLID_FS draw everything solid, instanced: boxes, cylinders and the hemisphere, with their materials:
// pale oak (fine straight grain, the slatted wall), the shelving's back panels washed by the strip lights, the books
// (a matte cloth or paper spine with its own modern design, pages, covers), polished concrete or oak floors, white
// concrete, steel, lights, the glazing (the sky and the trees or the city outside, mullions), the balustrades' glass
// (drawn last, blended) and the skylight. Lit by the sky (hemisphere), the sun with its shadow map, up to 16
// downlights, and on the shelves the strips' own light. SHADOW_FS writes the sun's depth. SPRITE_* are the glows
// (downlights, dust in the daylight); VEIL_FS the frosted panel behind the well and the HUD's columns; LINE_*
// screen-space lines (the ghost, a cleared row's light); DOWN / BLUR / FINAL the bloom and the frame; SHAPE / TEXT the
// HUD.
#include <string>

namespace library_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform float uTime;
// The drop's wave through the shelves: a pulse rolling along each shelf row once a beat, alternate rows the other
// way, a little later on each floor up. s: along the wall (m), y: height (m), level: the floor. 0..1.
uniform float uWaveK, uBeatPh;
float shelfWave(float s, float y, float level) {
    if (uWaveK <= 0.0) return 0.0;
    float row = floor(y / 0.32);
    float dir = mod(row, 2.0) < 0.5 ? 1.0 : -1.0;
    float d = fract(s * dir * 0.075 - uBeatPh - row * 0.035 - level * 0.12) - 0.5;
    return exp(-d * d / 0.011) * uWaveK;
}
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
)";

// ---------------------------------------------------------------- solids (instanced meshes)
// Instance: c (center, material), x y z (half axes, params p0 p1 p2), col (color, seed), e (-, -, floor, -).
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
out vec3 vP, vL, vLN, vN;
flat out vec3 vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat, vE;
flat out float vWave;
void main() {
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    vec3 C = iC.xyz;
    vWave = 0.0;
    if (uWaveK > 0.0 && int(iC.w + 0.5) == 3 && iE.z >= 0.0) {
        // a shelved book (its depth axis Z points out of its shelf): slid out and tipped forward about its foot
        vec3 out_ = normalize(Z);
        vec3 up = vec3(0.0, 1.0, 0.0);
        vec3 along = normalize(cross(up, out_));
        float w = shelfWave(dot(C, along), C.y, iE.z);
        vWave = w;
        if (w > 0.001) {
            float hu = abs(X.y) + abs(Y.y) + abs(Z.y);
            vec3 pivot = C - up * hu + out_ * length(Z);
            float a = 0.3 * w, ca = cos(a), sa = sin(a);
            // rotate in the (up, out) plane: the head comes forward
            vec3 v = C - pivot;
            float vy = dot(v, up), vo = dot(v, out_);
            vec3 vr = v + up * (vy * ca - vo * sa - vy) + out_ * (vo * ca + vy * sa - vo);
            C = pivot + vr + out_ * (0.11 * w);
            float xy, xo;
            xy = dot(X, up); xo = dot(X, out_); X += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
            xy = dot(Y, up); xo = dot(Y, out_); Y += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
            xy = dot(Z, up); xo = dot(Z, out_); Z += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
        }
    }
    vec3 P = C + X * aPos.x + Y * aPos.y + Z * aPos.z;
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
uniform float uFogD, uFogStart;
uniform vec3 uFogCol;
float shadowAt(vec3 P, vec3 N) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 s = uShadowVP * vec4(P - uShadowO + N * uShadowBias, 1.0);
    vec3 q = s.xyz / s.w * 0.5 + 0.5;
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
    float t = uShadowTexel;
    float r = 0.0;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) r += texture(uShadow, vec3(q.xy + vec2(i, j) * t * 1.2, q.z));
    return r / 9.0;
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
        float att = 1.0 / (1.0 + d2 / (r * r * 0.08)) * (1.0 - smoothstep(0.5 * r, r, d));
        // a downlight: a cone toward the floor
        att *= 0.2 + 0.8 * smoothstep(0.15, 0.8, L.y);
        float nl = max(dot(N, L), 0.0) * 0.85 + 0.15;
        c += alb * uLC[i].rgb * uLC[i].a * att * nl * mix(0.5, 1.0, ao);
        vec3 Hl = normalize(L + V);
        c += uLC[i].rgb * uLC[i].a * att * pow(max(dot(N, Hl), 0.0), rough) * spec * 0.6;
    }
    return c;
}
vec3 applyFog(vec3 c, vec3 P) {
    float dist = length(P - uEye);
    float f = 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD);
    return mix(c, uFogCol, clamp(f, 0.0, 1.0));
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec3 vP, vL, vLN, vN;
flat in vec3 vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat, vE;
flat in float vWave;
out vec4 frag;
uniform int uMesh;          // 0 box, 1 cylinder, 2 hemisphere
uniform float uBoard;       // 1: the well (no fog)
uniform float uStackA;      // the frame's alpha (1 on the stack, for the bridge)
uniform float uPremul;      // 1: a blended pass (the glass, a cleared row): premultiplied color and alpha
uniform vec3 uSkyA, uSkyB, uOutside, uLed, uPaper;
uniform float uLampK, uNight, uWorld, uShelfLight, uLevelH, uBeat;
uniform float uLevelOn[8];
uniform float uDropT, uDropK, uDropSpan, uGroupN;

float levelOn(float k) { int i = int(clamp(k, 0.0, 7.0) + 0.5); return uLevelOn[i]; }

// The view out of the glazing at world height y, x along it: the sky over a park's trees (day), the city (night).
vec3 outside(float x, float y) {
    // seen from far: everything out there a third of its size
    x *= 3.0;
    y = y * 3.0 - 1.0;
    vec3 sky = mix(uSkyB, uSkyA, smoothstep(0.0, 40.0, y));
    // a soft cloud or two
    sky = mix(sky, mix(uSkyB, vec3(1.0), 0.6), smoothstep(0.55, 0.8, fbm(vec2(x * 0.05, y * 0.12) + 4.0)) * 0.5 * (1.0 - uNight));
    // the canopy: rounded crowns of different heights, lit from above, hazier where it's lower
    float crown = 3.0 + 4.0 * fbm(vec2(x * 0.06, 2.7)) + 1.5 * fbm(vec2(x * 0.4, 9.1));
    float blk = floor(x / 3.1);
    float hB = 5.0 + 14.0 * pow(hash12(vec2(blk, 7.1)), 1.6);
    float h = mix(crown, hB, uNight);
    float edge = fwidth(y) + 0.04;
    float m = 1.0 - smoothstep(h - edge, h + edge, y);
    float leaf = fbm(vec2(x * 1.3, y * 1.3));
    vec3 trees = uOutside * mix(0.55, 1.25, leaf) * mix(0.7, 1.05, smoothstep(0.0, h, y));
    trees = mix(trees, uSkyB, 0.22);
    vec3 city = uOutside * (0.8 + 0.2 * hash12(vec2(blk, 2.0)));
    vec2 w = vec2(x / 0.45, y / 0.6);
    float lit = step(0.8, hash12(floor(w) + blk * 3.0)) * step(0.3, fract(w.x)) * step(0.35, fract(w.y));
    city += vec3(1.0, 0.78, 0.5) * lit * 0.22 * (1.0 - smoothstep(0.3, 1.0, fwidth(w.x)));
    vec3 c = mix(sky, mix(trees, city, uNight), m);
    // the lawn (the street at night) in front
    vec3 lawn = mix(uOutside * 1.15, uOutside * 0.6, uNight);
    c = mix(c, lawn, 1.0 - smoothstep(0.0, 0.6, y));
    return c;
}

void main() {
    int mat = int(vMat.x + 0.5);
    vec3 N = normalize(vN);
    if (!gl_FrontFacing && uMesh != 2) N = -N;
    if (uMesh == 2 && dot(N, uEye - vP) < 0.0) N = -N;
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
    float spec = 0.06, rough = 20.0, ao = 1.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    float px = max(fwidth(q.x), fwidth(q.y));
    float outA = 1.0;

    if (mat == 0) {
        // plaster: barely there
        alb *= 0.97 + 0.05 * fbm(vP.xy * 0.4 + vP.z * 0.3);
        spec = 0.02;
    } else if (mat == 1) {
        // pale oak: a fine straight grain along the longest side; p0 = 2: a wall of vertical slats on a dark ground
        vec2 g = sz.x > sz.y ? q : q.yx;
        float detail = 1.0 - smoothstep(0.003, 0.02, px);
        if (vMat.y > 1.5 && face == 2) {
            float period = 0.105;
            float f = fract((q.x + 50.0) / period);
            float sl = smoothstep(0.0, 0.06 + px / period, f) * (1.0 - smoothstep(0.62, 0.68 + px / period, f));
            float id = floor((q.x + 50.0) / period);
            float gr = fbm(vec2(q.y * 0.5, id * 3.1 + q.x * 40.0));
            alb = mix(vec3(0.025), alb * (0.88 + 0.16 * gr + 0.08 * hash12(vec2(id, 1.0))), sl);
            bend.x = (f / 0.65 - 0.5) * 0.8 * sl;
            ao = mix(0.5, 1.0, sl);
        } else {
            float grain = fbm(vec2(g.x * 0.7, g.y * 34.0) + seed * 13.0);
            float fine = vnoise(vec2(g.x * 2.0, g.y * 260.0) + seed * 7.0);
            alb *= mix(1.0, 0.9 + 0.14 * grain + 0.05 * fine, detail);
            float edge = min(sz.x - abs(q.x), sz.y - abs(q.y));
            alb *= mix(1.0, 0.92 + 0.08 * smoothstep(0.0, 0.004 + px, edge), detail);
        }
        spec = 0.1; rough = 22.0;
    } else if (mat == 2) {
        // the shelving's back panel: washed by the strip light under each shelf, darker low in each compartment
        float yb = q.y + sz.y;
        float cy = (yb - vMat.z) / vMat.y;
        float fr = fract(cy);
        float inside = step(0.0, cy) * step(cy, 8.0);
        float on = levelOn(vE.z);
        float wash = exp(-(1.0 - fr) * 3.2) * inside;
        alb *= 0.96 + 0.06 * fbm(q * vec2(1.5, 8.0) + seed);
        ao = mix(1.0, 0.72 + 0.28 * fr, inside);
        emit += uLed * wash * 0.22 * uShelfLight * on * alb / max(dot(alb, vec3(0.333)), 0.05);
        spec = 0.03;
    } else if (mat == 3) {
        // a book: the spine toward +z, pages at the ends and at -z, covers on the x sides. p0: its spine's design,
        // p1: its glow (shelved, the drop), p2: a cleared book's fade into light (0 none .. 1 gone)
        vec3 L = vL;
        vec3 h = vHalf;
        int sty = int(vMat.y + 0.5);
        float lum = dot(alb, vec3(0.2126, 0.7152, 0.0722));
        vec3 ink = lum > 0.22 ? vec3(0.035, 0.035, 0.04) : vec3(0.92, 0.9, 0.86);
        float rim = 0.08 * min(h.x * 2.0, h.z * 2.0);
        float detail = 1.0 - smoothstep(0.25, 0.7, px / max(min(h.x, h.y), 1e-4) * 2.0);
        if (face == 2 && vLN.z > 0.0) {
            float u = L.x, v = L.y;   // u across the thickness, v along the length
            float pu = fwidth(u) + 1e-4, pv = fwidth(v) + 1e-4;
            float m = 0.0;
            float r = hash12(vec2(seed * 3.1, 9.0));
            #define RECT(v0, v1, uw) (smoothstep(v0 - pv, v0 + pv, v) * (1.0 - smoothstep(v1 - pv, v1 + pv, v)) * (1.0 - smoothstep(uw - pu, uw + pu, abs(u))))
            // tonal designs, so the piece keeps its color: a lighter block, fine rules, two tones, a dark title
            vec3 light = mix(alb, vec3(0.95, 0.94, 0.9), 0.38), dark = alb * 0.78;
            if (sty == 0) {
                m = RECT(0.1, 0.6, 2.0);
                alb = mix(alb, light, m * detail);
                float t = RECT(0.22, 0.22 + 0.26 * (0.5 + 0.5 * r), 0.28);
                alb = mix(alb, ink, t * detail * 0.7);
            } else if (sty == 1) {
                m = RECT(-0.66, -0.61, 2.0) + RECT(-0.54, -0.49, 2.0);
                alb = mix(alb, light, clamp(m, 0.0, 1.0) * mix(0.3, 0.9, detail));
                alb = mix(alb, ink, RECT(0.18, 0.18 + 0.42 * (0.5 + 0.5 * r), 0.3) * detail * 0.75);
            } else if (sty == 2) {
                float lo = smoothstep(-0.22 - pv, -0.22 + pv, v);
                alb = mix(dark, alb, lo);
                alb = mix(alb, light, RECT(0.28, 0.66, 0.26) * detail * 0.9);
            } else {
                alb = mix(alb, ink, RECT(0.05, 0.05 + 0.5 * (0.5 + 0.5 * r), 0.26) * detail * 0.7);
                float dt = 1.0 - smoothstep(0.24 - pu, 0.24 + pu, length(vec2(u, (v + 0.78) * h.y / max(h.x, 1e-3))));
                alb = mix(alb, light, dt * detail);
            }
            // the cloth's tooth, the spine's edges catching the light
            alb *= 0.96 + 0.06 * vnoise(vec2(u * 30.0, v * 120.0) + seed * 9.0) * detail;
            bend.x = u * 0.35;
            alb *= 0.9 + 0.1 * smoothstep(1.0, 0.8, abs(u));
            spec = 0.06; rough = 14.0;
            emit += alb * vMat.z * 1.2 + uLed * vMat.z * 0.25;
        } else if (face == 1 || (face == 2 && vLN.z < 0.0)) {
            // the page block (head, tail, fore-edge), the boards' edges around it
            float ax = h.x - abs(L.x * h.x);
            float covers = 1.0 - smoothstep(rim * 0.8, rim, ax);
            float spineSide = face == 1 ? step(h.z - rim * 1.2, L.z * h.z) : 0.0;
            float pages = (1.0 - covers) * (1.0 - spineSide);
            float lines = 0.5 + 0.5 * sin(L.x * h.x * 2400.0 + seed * 3.0);
            float lp = fwidth(L.x * h.x * 2400.0);
            lines = mix(lines, 0.5, smoothstep(1.0, 3.0, lp));
            vec3 paper = uPaper * (0.9 + 0.1 * lines);
            paper *= 0.86 + 0.14 * smoothstep(rim, rim * 2.5, ax);
            alb = mix(alb * 0.85, paper, pages);
            spec = mix(0.06, 0.03, pages);
            emit += uPaper * vMat.z * 0.4 * pages;
        } else {
            // a cover: matte, a hair darker than the spine
            alb *= 0.9;
            alb *= 0.97 + 0.04 * vnoise(q * 400.0 + seed) * detail;
            spec = 0.08; rough = 18.0;
            emit += alb * vMat.z * 0.6;
        }
        if (uWorld > 0.5) {
            // on the shelves: the strip above lights its head more than its foot
            float up = dot(vAX, vec3(0, 1, 0)) * L.x + dot(vAY, vec3(0, 1, 0)) * L.y;
            emit += alb * uLed * uShelfLight * levelOn(vE.z) * (0.08 + 0.22 * smoothstep(-1.0, 1.0, up));
            // lit as the wave lifts it out
            emit += (alb * 0.25 + uLed * 0.1) * vWave;
            // the drop's light running along the collection
            float d = (vE.w + 0.5) / uGroupN - uDropT;
            emit += (alb * 1.6 + uLed * 0.3) * uDropK * exp(-d * d / (uDropSpan * uDropSpan));
        }
        if (vMat.w > 0.0) {
            // a cleared book: swelling and dissolving into light
            float f = vMat.w;
            // its own color, lit from within, then gone
            emit += (vCol.rgb * 1.4 + 0.25) * (2.2 * f);
            outA = (1.0 - f) * (1.0 - f * 0.5);
        }
    } else if (mat == 12) {
        // a cell of the board: one clean block, a bound volume with its spine toward us: a soft bevel, two fine
        // rules at head and tail, a small label; the page edge on its right side; p2: a clear's fade into light
        if (face == 2 && vLN.z > 0.0) {
            vec2 e = sz - abs(q);
            float ed = min(e.x, e.y);
            float rim = 1.0 - smoothstep(0.0, 0.03 + px, ed);
            alb *= 1.0 - 0.18 * rim;
            float pr = px + 0.004;
            float rule = (1.0 - smoothstep(0.012, 0.012 + pr, abs(q.y - sz.y * 0.7))) + (1.0 - smoothstep(0.012, 0.012 + pr, abs(q.y + sz.y * 0.7)));
            vec3 lite = mix(alb, vec3(1.0), 0.3);
            alb = mix(alb, lite, clamp(rule, 0.0, 1.0) * 0.75);
            // the pages showing down its right side, between the boards
            float pe = smoothstep(sz.x * 0.76 - pr, sz.x * 0.76 + pr, q.x) * (1.0 - smoothstep(sz.x * 0.93 - pr, sz.x * 0.93 + pr, q.x));
            pe *= smoothstep(sz.y * 0.86 + pr, sz.y * 0.86 - pr, abs(q.y));
            float pl = 0.5 + 0.5 * sin(q.y * 700.0);
            pl = mix(pl, 0.5, smoothstep(1.0, 3.0, fwidth(q.y * 700.0)));
            alb = mix(alb, uPaper * (0.88 + 0.1 * pl), pe * 0.92);
            spec = 0.1; rough = 22.0;
        } else if (face == 0 && vLN.x > 0.0) {
            float lines = 0.5 + 0.5 * sin(q.y * 900.0);
            lines = mix(lines, 0.5, smoothstep(1.0, 3.0, fwidth(q.y * 900.0)));
            float covers = 1.0 - smoothstep(0.02, 0.03, sz.y - abs(q.y));
            alb = mix(uPaper * (0.86 + 0.1 * lines), alb * 0.85, covers);
            spec = 0.04;
        } else {
            alb *= 0.86;
            spec = 0.06;
        }
        if (vMat.w > 0.0) {
            float f = vMat.w;
            emit += (vCol.rgb * 1.3 + 0.25) * (2.0 * f);
            outA = (1.0 - f) * (1.0 - f * 0.5);
        }
    } else if (mat == 4) {
        // floor: polished concrete (p0 = 0), or long planks of pale oak (p0 = 1)
        vec2 g = vP.xz;
        float detail = 1.0 - smoothstep(0.01, 0.05, px);
        if (vMat.y < 0.5) {
            float m = fbm(g * 0.35) * 0.7 + fbm(g * 2.3 + 4.0) * 0.3;
            alb *= 0.9 + 0.16 * m;
            // saw cuts on a 3 m grid
            vec2 f = abs(fract(g / 3.0 + 0.5) - 0.5) * 3.0;
            alb *= mix(1.0, 0.82 + 0.18 * smoothstep(0.0, 0.006 + px, min(f.x, f.y)), detail);
            spec = 0.4; rough = 60.0;
        } else if (vMat.y > 1.5) {
            // terrazzo: a warm pale ground, chips of stone in two sizes, a fine 2.4 m grid
            float m = fbm(g * 0.3) * 0.5 + 0.5;
            vec2 c1 = g * 38.0, c2 = g * 13.0;
            float h1 = hash12(floor(c1)), h2 = hash12(floor(c2) + 7.0);
            float chip1 = step(0.78, h1) * (1.0 - smoothstep(0.22, 0.38, length(fract(c1) - 0.5)));
            float chip2 = step(0.86, h2) * (1.0 - smoothstep(0.18, 0.34, length(fract(c2) - 0.5)));
            vec3 stoneA = vec3(0.32, 0.31, 0.3), stoneB = vec3(0.62, 0.52, 0.42);
            alb *= 0.94 + 0.08 * m;
            alb = mix(alb, mix(stoneA, stoneB, hash12(floor(c1) + 3.0)), chip1 * 0.8 * detail);
            alb = mix(alb, mix(stoneB, alb * 0.7, hash12(floor(c2) + 5.0)), chip2 * 0.7 * detail);
            vec2 f = abs(fract(g / 2.4 + 0.5) - 0.5) * 2.4;
            alb *= mix(1.0, 0.86 + 0.14 * smoothstep(0.0, 0.005 + px, min(f.x, f.y)), detail);
            spec = 0.45; rough = 70.0;
        } else {
            float w = 0.19;
            float row = floor(g.x / w);
            float off = hash12(vec2(row, 3.0)) * 2.4;
            float col = floor((g.y + off) / 2.4);
            float id = hash12(vec2(row, col));
            vec2 pl = vec2(fract(g.x / w), fract((g.y + off) / 2.4));
            float seam = smoothstep(0.0, 0.03 + px / w, min(pl.x, 1.0 - pl.x)) * smoothstep(0.0, 0.004 + px / 2.4, min(pl.y, 1.0 - pl.y));
            float gr = fbm(vec2(g.x * 30.0, g.y * 0.8) + id * 20.0);
            alb *= mix(1.0, (0.9 + 0.16 * id) * (0.75 + 0.25 * seam) * (0.92 + 0.12 * gr), detail);
            spec = 0.25; rough = 40.0;
        }
    } else if (mat == 5) {
        // white concrete: fine, quiet
        alb *= 0.96 + 0.05 * fbm(q * 2.5 + seed);
        spec = 0.04;
    } else if (mat == 6) {
        // dark steel
        spec = 0.35; rough = 40.0;
    } else if (mat == 7) {
        // a light: p0 how bright, p1 = 1: follows the lamps' level; its floor's lights must be on
        float k = vMat.y * (vMat.z > 0.5 ? uLampK : 1.0) * mix(0.15, 1.0, levelOn(vE.z));
        if (vHalf.y < 0.005 && vE.z >= 0.0) k *= 1.0 + 2.2 * shelfWave(dot(vP, vAX), vP.y + 0.05, vE.z);
        emit = alb * k;
        alb = vec3(0.0);
    } else if (mat == 8) {
        // glazing (its +z face): the view out between mullions. p0: 1 the curtain wall, 0 the slot
        if (face == 2 && vLN.z > 0.0) {
            float x = dot(vP, vAX);
            float y = vP.y;
            float mw = vMat.y > 0.5 ? 1.5 : 0.9;
            float fx = abs(fract(x / mw) - 0.5) * mw;           // distance to the nearest mullion's center
            float fy = abs(fract(y / (uLevelH * 0.5)) - 0.5) * uLevelH * 0.5;
            float mull = 1.0 - smoothstep(0.035, 0.035 + px * 1.5, mw * 0.5 - fx);
            float tr = 1.0 - smoothstep(0.03, 0.03 + px * 1.5, uLevelH * 0.25 - fy);
            float frameM = max(mull, tr);
            vec3 o = outside(x + seed * 40.0, y);
            emit = o * (1.0 - frameM);
            alb = alb * frameM;
            spec = 0.3 * frameM;
        } else alb *= 0.4;
    } else if (mat == 9) {
        // glass (blended): a faint tint, the sky's reflection at grazing angles, a bright edge
        vec3 V = normalize(uEye - vP);
        float fr = pow(1.0 - abs(dot(N, V)), 4.0);
        vec2 e = sz - abs(q);
        float edge = 1.0 - smoothstep(0.0, 0.01 + px, min(e.x, e.y));
        vec3 c = mix(alb * 0.2, mix(uSkyB, uSkyA, 0.5) * 0.45, fr) + vec3(0.6) * edge * 0.25;
        outA = 0.03 + 0.14 * fr + 0.25 * edge;
        frag = vec4(applyFog(c, vP) * outA, outA);
        return;
    } else if (mat == 10) {
        // the skylight: a grid of white beams over the sky (its underside)
        if (face == 1 && vLN.y < 0.0) {
            vec2 g = vP.xz;
            vec2 f = abs(fract(g / 2.4 + 0.5) - 0.5) * 2.4;
            float beam = 1.0 - smoothstep(0.12, 0.12 + px * 1.5, min(f.x, f.y));
            vec3 sky = mix(uSkyA, uSkyB, 0.25) * 1.2;
            emit = sky * (1.0 - beam);
            alb *= beam;
        }
    } else if (mat == 11) {
        // felt: soft, matte
        alb *= 0.95 + 0.08 * vnoise(q * 300.0);
        spec = 0.0;
    }

    vec3 Nb = normalize(N + T * bend.x + B * bend.y);
    vec3 c = shade(vP, Nb, alb, spec, rough, ao) + emit;
    if (uBoard < 0.5) c = applyFog(c, vP);
    if (uPremul > 0.5) frag = vec4(c * outA, outA);
    else frag = vec4(c, uStackA);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vP;
flat in vec4 vMat;
out vec4 frag;
void main() {
    // the skylight's glazing and the lights let the sun through
    int mat = int(vMat.x + 0.5);
    if (mat == 10 || mat == 7) discard;
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

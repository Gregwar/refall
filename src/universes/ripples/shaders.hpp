#pragma once
// RIPPLES's shaders (see ripples.cpp).
//
// The water is a height field simulated on the GPU at a reduced resolution (about ten texels per cell): MASK_* draws
// what stands in the water (the stones, the basin's kerb, the previews) into an obstacle mask, BLUR_FS softens it
// (the stones' shadows on the bed, the water darkening at their foot), SPLAT_* adds the disturbances (a moving
// stone's push, a drop, a sinking row), STEP_FS advances the wave equation (an isotropic 9-point Laplacian, slow
// damping, walls that reflect). WATER_FS shades the surface at full resolution: the bed seen through it (refracted,
// with the stones' shadows and faint caustics), the sky and the light reflected in it (a virtual eye above the
// pond), the kerb of the basin. STONE_* draws the cut stones (one per piece, its cells merged), PAD_* the lily pads
// and lotus flowers floating on the outer pond, DOWN/BLUR/FINAL the glow and the frame, SHAPE/TEXT the HUD.
#include <string>

namespace ripples_shaders {

inline const char* COMMON = R"(
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
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
)";

// ---------------------------------------------------------------- obstacles (instanced rectangles, board units)
inline const char* MASK_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aRect;   // x0, y0, x1, y1
uniform mat4 uVP;
uniform vec2 uHalf;                   // half a water texel (board units)
out vec2 vW;
flat out vec4 vR;
void main() {
    vec2 wp = mix(aRect.xy - uHalf * 2.0, aRect.zw + uHalf * 2.0, aPos * 0.5 + 0.5);
    vW = wp;
    vR = aRect;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
// The share of the texel the rectangle covers: a stone moving smoothly sweeps the water smoothly.
inline const char* MASK_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vR;
out vec4 frag;
uniform vec2 uHalf;
void main() {
    vec2 lo = max(vW - uHalf, vR.xy), hi = min(vW + uHalf, vR.zw);
    vec2 o = max(hi - lo, vec2(0.0));
    float c = o.x * o.y / (4.0 * uHalf.x * uHalf.y);
    frag = vec4(c, 0.0, 0.0, 1.0);
}
)";

// A separable blur (the obstacle mask -> shadows and contact darkening).
inline const char* BLUR1_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    float c = texture(uSrc, vUv).r * w[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)).r + texture(uSrc, vUv - uDir * float(i)).r) * w[i];
    frag = vec4(c, 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- disturbances (instanced capsules)
// a: x0, y0, x1, y1 (a segment, board units). b: radius, amplitude, -, -.
inline const char* SPLAT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uVP;
out vec2 vW;
flat out vec4 vA, vB;
void main() {
    float r = aB.x * 2.6;
    vec2 lo = min(aA.xy, aA.zw) - r, hi = max(aA.xy, aA.zw) + r;
    vec2 wp = mix(lo, hi, aPos * 0.5 + 0.5);
    vW = wp;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SPLAT_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vA, vB;
out vec4 frag;
void main() {
    vec2 a = vA.xy, b = vA.zw, pa = vW - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    float d = length(pa - ba * h);
    float r = vB.x;
    // a "Mexican hat" (no volume of water added): the rings stay fine, no broad swell follows them
    float q = d * d / (r * r);
    frag = vec4(vB.y * (1.0 - q) * exp(-q) * 1.6, 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- one step of the waves
// state: r = height now, g = height one step before. Walls (the mask) reflect: a neighbour inside one mirrors the
// cell itself. The damping is slow in open water, stronger at the foot of the stones and at the screen's edges.
inline const char* STEP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uState, uSrc, uMask, uSoft;
uniform float uK, uDamp, uSrcK, uEdgeDamp;
float hAt(ivec2 q, ivec2 sz, float h0) {
    q = clamp(q, ivec2(0), sz - 1);
    return mix(texelFetch(uState, q, 0).r, h0, texelFetch(uMask, q, 0).r);
}
// a neighbour's velocity (a wall's: the cell's own)
float vAt(ivec2 q, ivec2 sz, float v0) {
    q = clamp(q, ivec2(0), sz - 1);
    vec2 s = texelFetch(uState, q, 0).rg;
    return mix(s.r - s.g, v0, texelFetch(uMask, q, 0).r);
}
void main() {
    ivec2 sz = textureSize(uState, 0);
    ivec2 p = ivec2(gl_FragCoord.xy);
    float m = texelFetch(uMask, p, 0).r;
    if (m > 0.995) { frag = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec2 s = texelFetch(uState, p, 0).rg;
    float h = s.r;
    float n4 = hAt(p + ivec2(1, 0), sz, h) + hAt(p + ivec2(-1, 0), sz, h) + hAt(p + ivec2(0, 1), sz, h) + hAt(p + ivec2(0, -1), sz, h);
    float n8 = hAt(p + ivec2(1, 1), sz, h) + hAt(p + ivec2(-1, 1), sz, h) + hAt(p + ivec2(1, -1), sz, h) + hAt(p + ivec2(-1, -1), sz, h);
    float lap = (4.0 * n4 + n8 - 20.0 * h) / 6.0;
    // damping: slow in open water, more at the foot of the stones (no ringing between them) and at the screen's edges
    float soft = texture(uSoft, vUv).r;
    vec2 e = min(vUv, 1.0 - vUv);
    float edge = 1.0 - smoothstep(0.0, 0.05, min(e.x, e.y * 1.78));
    float damp = uDamp * (1.0 - 0.04 * smoothstep(0.3, 0.85, soft)) * (1.0 - uEdgeDamp * edge);
    // the velocity, its grid-scale part damped (a binomial blur's remainder): the finest wavelets of the grid die at
    // once, the ripples hardly feel it
    float v = h - s.g;
    float v4 = vAt(p + ivec2(1, 0), sz, v) + vAt(p + ivec2(-1, 0), sz, v) + vAt(p + ivec2(0, 1), sz, v) + vAt(p + ivec2(0, -1), sz, v);
    float v8 = vAt(p + ivec2(1, 1), sz, v) + vAt(p + ivec2(-1, 1), sz, v) + vAt(p + ivec2(1, -1), sz, v) + vAt(p + ivec2(-1, -1), sz, v);
    float vb = (4.0 * v + 2.0 * v4 + v8) / 16.0;
    v -= 0.12 * (v - vb);
    float hn = h + v * damp + uK * lap;
    // the level settles back to rest
    hn *= 0.9995;
    hn += texelFetch(uSrc, p, 0).r * uSrcK;
    frag = vec4(hn * (1.0 - m), h * (1.0 - m), 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- the surface (full screen)
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uState, uMask, uSoft;
uniform vec4 uView;            // center, half extents (board units)
uniform vec2 uSimTexel;        // one sim texel (uv)
uniform float uNormK;          // slope gain
uniform vec3 uBed, uBedFar, uZenith, uHorizon, uLight, uKerb;
uniform vec3 uLightDir;        // toward the light
uniform float uCamH, uRefl, uSpec, uSpecW, uDepth, uShadowK, uCaustK, uAoK, uLightK, uSheen, uEmboss;
uniform vec2 uShadowOff;
uniform float uTime, uSeed;
uniform float uBreeze, uGust;  // fine wind ripples: the peak's breeze (the outer pond), the drop's gust
uniform vec2 uWind;
uniform vec4 uRing;            // a quad: center, time since, strength
uniform float uKerbOn;
uniform float uCalm;           // 1: the surface settled (the bridge)

float simH(vec2 uv) { return texture(uState, uv).r; }
vec2 toUv(vec2 w) { return (w - (uView.xy - uView.zw)) / (2.0 * uView.zw); }

// the wind's fine ripples (analytic): travelling wavelets under drifting patches
float windH(vec2 w) {
    float a = 0.0;
    float k = uBreeze * smoothstep(5.8, 7.5, abs(w.x)) + uGust;
    if (k < 0.0005) return 0.0;
    // cat's paws: patches of fine, irregular roughness drifting with the wind
    vec2 d = normalize(uWind);
    float patch = fbm(w * 0.08 - d * uTime * 0.25 + uSeed);
    patch = smoothstep(0.48, 0.85, patch);
    if (patch <= 0.0) return 0.0;
    vec2 q = w - d * uTime * 1.2;
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    a += vnoise(q * 5.0 + uTime * 0.7) - 0.5;
    a += 0.6 * (vnoise(R * q * 9.0 - uTime * 0.9) - 0.5);
    a += 0.35 * (vnoise(R * R * q * 16.0 + uTime * 1.3) - 0.5);
    return a * k * patch * 0.005;
}
// a quad: three fine rings spreading from the board over the whole pond
float ringH(vec2 w) {
    if (uRing.w <= 0.0005) return 0.0;
    float r = length(w - uRing.xy);
    float h = 0.0;
    for (int i = 0; i < 3; i++) {
        float R = (uRing.z - float(i) * 0.45) * 5.5;
        if (R <= 0.0) continue;
        float x = (r - R) / 0.22;
        h += -x * exp(-x * x) * (1.0 - 0.25 * float(i));
    }
    return h * uRing.w * 0.14 * exp(-uRing.z * 0.4) / sqrt(1.0 + r * 0.08);
}
float extraH(vec2 w) { return windH(w) + ringH(w); }

// the basin's kerb: two long walls along the well and a bottom one, a thin cut stone just above the water
float kerbSdf(vec2 w) {
    float top = uView.y + uView.w + 1.0;
    float side = sdBox(vec2(abs(w.x) - 5.21, w.y - (top - 10.21) * 0.5), vec2(0.15, (top + 10.21) * 0.5 + 0.15));
    float bottom = sdBox(w - vec2(0.0, -10.21), vec2(5.36, 0.15));
    return min(side, bottom) - 0.02;
}

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 tx = uSimTexel;
    // the surface's slope: the simulated waves and the analytic ones
    float hx = simH(vUv + vec2(tx.x, 0.0)) - simH(vUv - vec2(tx.x, 0.0));
    float hy = simH(vUv + vec2(0.0, tx.y)) - simH(vUv - vec2(0.0, tx.y));
    vec2 cell = 2.0 * uView.zw * tx;   // one texel in board units
    vec2 g = vec2(hx / (2.0 * cell.x), hy / (2.0 * cell.y));
    const float E = 0.03;
    float e0 = extraH(w);
    g += vec2(extraH(w + vec2(E, 0.0)) - e0, extraH(w + vec2(0.0, E)) - e0) / E;
    g *= uNormK * (1.0 - uCalm);
    // a soft limit on the slope: a fresh drop never turns into a dark hole
    g /= 1.0 + 3.0 * length(g);
    vec3 n = normalize(vec3(-g, 1.0));
    // a virtual eye high above the pond (so the light has a place where it shines on the water)
    vec3 P = vec3(w, 0.0);
    vec3 V = normalize(vec3(uView.xy, uCamH) - P);
    vec3 R = reflect(-V, n);
    // the sky in the water: zenith overhead, the horizon's paler tone toward the edges, the light's glow
    float el = clamp(R.z, 0.0, 1.0);
    vec3 sky = mix(uHorizon, uZenith, smoothstep(0.45, 0.98, el));
    float ld = max(dot(R, uLightDir), 0.0);
    sky += uLight * uLightK * (0.04 * pow(ld, 30.0) + 0.12 * pow(ld, 700.0));
    // the sky is brighter toward the light: slopes facing it catch a fine line of it
    // (only what the slope adds: still water keeps its even tone)
    vec3 Lf = normalize(vec3(uLightDir.xy, 0.35));
    vec3 R0 = reflect(-V, vec3(0.0, 0.0, 1.0));
    float sheen = pow(max(dot(R, Lf), 0.0), 20.0) - pow(max(dot(R0, Lf), 0.0), 20.0);
    sky += uLight * uLightK * uSheen * sheen;
    // and the slopes turned away from it a little darker: the rings read in relief, lit from one side
    float tiltL = dot(R.xy - R0.xy, normalize(uLightDir.xy + vec2(1e-4)));
    sky *= 1.0 + uEmboss * clamp(tiltL, -0.5, 0.5);
    float spec = min(pow(ld, uSpecW) * uSpec * uLightK, 1.2);
    // the bed seen through the water: refracted, the stones' soft shadows, faint caustics
    vec2 bw = w - n.xy * uDepth;
    vec2 buv = toUv(bw);
    float sh = texture(uSoft, toUv(bw - uShadowOff)).r;
    float lap = simH(buv + vec2(tx.x, 0.0)) + simH(buv - vec2(tx.x, 0.0)) + simH(buv + vec2(0.0, tx.y)) + simH(buv - vec2(0.0, tx.y)) - 4.0 * simH(buv);
    float caust = clamp(-lap * uCaustK, -0.25, 0.4) * (1.0 - uCalm);
    float big = fbm(bw * 0.05 + uSeed * 0.3);
    float far = smoothstep(4.0, 16.0, abs(w.x)) * 0.6 + smoothstep(0.0, 1.0, abs(vUv.y - 0.5) * 2.0) * 0.4;
    vec3 bed = mix(uBed, uBedFar, far) * (0.95 + 0.1 * big);
    bed *= 1.0 - uShadowK * smoothstep(0.05, 0.9, sh);
    bed *= 1.0 + caust;
    // the water darkens a little at the foot of what stands in it
    float ao = texture(uSoft, vUv).r;
    float F = uRefl + (1.0 - uRefl) * pow(1.0 - max(dot(n, V), 0.0), 5.0);
    vec3 col = mix(bed, sky, clamp(F, 0.0, 1.0));
    col *= 1.0 - uAoK * smoothstep(0.1, 0.7, ao);
    col += uLight * spec;
    // the kerb
    if (uKerbOn > 0.5) {
        float d = kerbSdf(w);
        float aa = 1.5 * cell.x / 10.0 * 2.0;
        aa = max(fwidth(d), 1e-4);
        if (d < 0.08) {
            float cov = 1.0 - smoothstep(-aa, aa, d);
            // a cut stone: flat honed top, a fine chamfer catching the light on one side
            vec2 gd = vec2(dFdx(d), dFdy(d));
            gd = length(gd) > 1e-6 ? normalize(gd) : vec2(0.0);
            float ch = smoothstep(-0.05, 0.0, d);
            vec3 kn = normalize(vec3(gd * ch * 0.9, 1.0));
            float dif = clamp(dot(kn, normalize(uLightDir + vec3(0.0, 0.0, 0.6))), 0.0, 1.0);
            float grain = vnoise(w * 26.0) * 0.5 + vnoise(w * 61.0) * 0.5;
            vec3 kc = uKerb * (0.82 + 0.3 * dif) * (0.95 + 0.1 * grain) * (0.96 + 0.08 * fbm(w * 1.3));
            vec3 Rk = reflect(-V, kn);
            kc += uLight * uLightK * 0.05 * pow(max(dot(Rk, uLightDir), 0.0), 40.0);
            // a thin wet line where it meets the water
            float wetL = exp(-abs(d) / 0.012) * (1.0 - cov);
            col = mix(col, kc, cov);
            col = mix(col, sky, wetL * 0.35);
        }
    }
    frag = vec4(col, 0.0);
}
)";

// ---------------------------------------------------------------- the stones (instanced per cell)
// a: x, y (cell center), half (board units), mask. b: texture origin x, y, seed, type.
// c: alpha, progress (sinking), kind (0 stone, 1 ghost, 2 sinking), -. d: -.
inline const char* STONE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA, vB, vC;
void main() {
    vec2 l = aPos * 0.6;
    vLocal = l;
    float sink = aC.z > 1.5 ? aC.y : 0.0;
    vec2 wp = aA.xy + l * 2.0 * aA.z * (1.0 - 0.1 * sink);
    vWorld = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// One piece's outline: its own cells merge into one block with crisp, slightly rounded corners; a thin joint parts
// it from the others (mask bits: left right down up, down-left down-right up-left up-right).
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float unionRound(float a, float b, float r) {
    vec2 u = max(vec2(r - a, r - b), vec2(0.0));
    return max(r, min(a, b)) - length(u);
}
const float GAP = 0.03, RO = 0.075, RI = 0.03;
float shapeSdf(vec2 p, int m) {
    vec2 q = abs(p);
    bool hx = p.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = p.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = p.x < 0.0 ? (p.y < 0.0 ? 4 : 6) : (p.y < 0.0 ? 5 : 7);
    bool dg = has(m, db);
    float c = 0.5 - GAP;
    if (!hx && !vy) { vec2 k = q - (c - RO); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - RO; }
    if (hx && !vy) return dg ? unionRound(q.y - c, 0.5 + GAP - q.x, RI) : q.y - c;
    if (!hx && vy) return dg ? unionRound(q.x - c, 0.5 + GAP - q.y, RI) : q.x - c;
    if (dg) return -0.6;
    return unionRound(q.x - c, q.y - c, RI);
}
bool ownCell(vec2 p, int m) {
    if (p.x < -0.5 && has(m, 0)) return false;
    if (p.x > 0.5 && has(m, 1)) return false;
    if (p.y < -0.5 && has(m, 2)) return false;
    if (p.y > 0.5 && has(m, 3)) return false;
    return true;
}
)";

inline const char* STONE_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA, vB, vC;
out vec4 frag;
uniform vec3 uBase[7], uVein[7];
uniform vec3 uLight, uLightDir, uZenith, uHorizon, uBed, uGhost;
uniform float uLightK, uCamH, uPolish;
uniform vec4 uView;

// the stone's surface: each piece type its own stone (value variations around its base color)
vec3 stoneColor(int t, vec2 q, float seed) {
    vec3 b = uBase[t], v = uVein[t];
    float f;
    if (t == 0) {
        // slate: fine cleft lines along its length, matte
        float l = vnoise(vec2(q.x * 1.2, q.y * 18.0) + seed * 9.0) * 0.6 + vnoise(vec2(q.x * 3.0, q.y * 46.0)) * 0.4;
        f = smoothstep(0.35, 0.75, l);
        return mix(b, v, f * 0.55);
    } else if (t == 1) {
        // granite: a fine three-tone grain
        float g1 = vnoise(q * 34.0 + seed * 7.0), g2 = vnoise(q * 71.0 + 3.0);
        vec3 c = mix(b, v, smoothstep(0.62, 0.8, g1) * 0.45);
        c = mix(c, b * 1.15, smoothstep(0.7, 0.85, g2) * 0.3);
        return c * (0.96 + 0.08 * vnoise(q * 3.0));
    } else if (t == 2) {
        // marble: smooth, cloudy, thin veins
        float n = fbm(q * 0.9 + seed * 5.0);
        float vein = abs(sin((q.x * 0.8 + q.y * 1.3) * 2.2 + n * 6.5));
        vein = 1.0 - smoothstep(0.0, 0.06, vein);
        vec3 c = b * (0.95 + 0.1 * n);
        return mix(c, v, vein * 0.5);
    } else if (t == 3) {
        // jade: cloudy, deep, faintly translucent
        float n = fbm(q * 1.1 + seed * 3.0), m = fbm(q * 3.0 + 7.0);
        return mix(b, v, smoothstep(0.3, 0.8, n) * 0.55) * (0.94 + 0.12 * m);
    } else if (t == 4) {
        // jasper: fine curved bands
        float n = fbm(q * 0.7 + seed * 4.0);
        float band = sin(q.y * 5.5 + n * 7.0 + q.x * 0.6);
        return mix(b, v, smoothstep(0.4, 0.95, band) * 0.45) * (0.96 + 0.06 * vnoise(q * 12.0));
    } else if (t == 5) {
        // basalt: dense, even, a few pores
        float p = vnoise(q * 40.0 + seed * 11.0);
        return mix(b, v, smoothstep(0.82, 0.92, p) * 0.6) * (0.97 + 0.05 * vnoise(q * 4.0));
    }
    // travertine: soft bands, elongated pores
    float band = fbm(vec2(q.x * 0.6, q.y * 3.0) + seed * 6.0);
    float pore = vnoise(vec2(q.x * 9.0, q.y * 30.0) + seed * 2.0);
    vec3 c = mix(b, v, smoothstep(0.35, 0.8, band) * 0.45);
    return mix(c, v * 0.8, smoothstep(0.83, 0.93, pore) * 0.5);
}

void main() {
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    int kind = int(vC.z + 0.5);
    if (!ownCell(p, m)) discard;
    float d = shapeSdf(p, m);
    float aa = max(length(fwidth(p)) * 0.75, 1e-4);
    int t = int(vB.w + 0.5);
    vec2 q = vB.xy + vec2(p.x, p.y);
    if (kind == 1) {
        // the ghost: a fine pale outline drawn on the water where it will land
        float l = 1.0 - smoothstep(0.0, aa * 1.4, abs(d + 0.03) - 0.008);
        float a = l * 0.5 * vC.x;
        if (a <= 0.002) discard;
        frag = vec4(uGhost * a, a);
        return;
    }
    float sink = kind == 2 ? vC.y : 0.0;
    aa += sink * 0.05;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.002) discard;
    float inside = max(-d, 0.0);
    vec3 base = stoneColor(t, q, vB.z);
    // a cut block seen from above: a flat honed top, a fine chamfer along its edges (lit on the light's side)
    vec2 gd = vec2(dFdx(d), dFdy(d));
    gd = length(gd) > 1e-6 ? normalize(gd) : vec2(0.0);
    float ch = 1.0 - smoothstep(0.0, 0.065, inside);
    vec3 n = normalize(vec3(gd * ch * 1.3, 1.0));
    vec3 L = normalize(uLightDir + vec3(0.0, 0.0, 0.8));
    float dif = clamp(dot(n, L), 0.0, 1.0);
    vec3 col = base * (0.62 + 0.5 * dif);
    // the polish: the sky and the light reflected faintly in the top
    vec3 V = normalize(vec3(uView.xy, uCamH) - vec3(vWorld, 0.0));
    vec3 R = reflect(-V, n);
    vec3 sky = mix(uHorizon, uZenith, smoothstep(0.45, 0.98, R.z));
    float ld = max(dot(R, uLightDir), 0.0);
    col = mix(col, sky, uPolish * (1.0 - ch * 0.5));
    col += uLight * uLightK * uPolish * (0.5 * pow(ld, 60.0) + 1.5 * pow(ld, 600.0));
    // the edge's last hair: darker, so each block reads crisp on the water
    col *= 1.0 - 0.35 * exp(-inside / 0.012);
    // sinking: under the surface, it takes the water's color and goes
    if (kind == 2) {
        col = mix(col, uBed * 0.9, smoothstep(0.0, 0.8, sink));
        col = mix(col, sky, 0.15 * sink);
    }
    float a = cov * vC.x;
    if (kind == 2) a *= 1.0 - smoothstep(0.35, 1.0, sink);
    if (a <= 0.002) discard;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- lily pads and lotus flowers (instanced)
// a: x, y, radius, rotation. b: grow (0..1), seed, kind (0 pad, 1 lotus), alpha.
inline const char* PAD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uVP;
out vec2 vL;
out vec2 vW;
flat out vec4 vA, vB;
void main() {
    vec2 l = aPos * 1.45;
    vec2 wp = aA.xy + l * aA.z;
    vL = l;
    vW = wp;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* PAD_FS = R"(#version 330 core
in vec2 vL;
in vec2 vW;
flat in vec4 vA, vB;
out vec4 frag;
uniform sampler2D uState;
uniform vec4 uView;
uniform vec2 uShadowOff;
uniform vec3 uPad, uPad2, uLotus, uLotusCore, uLight, uLightDir;
uniform float uLightK, uGlow, uShadowA;
vec2 rot(vec2 p, float a) { float c = cos(a), s = sin(a); return vec2(c * p.x - s * p.y, s * p.x + c * p.y); }
float padSdf(vec2 p, float grow) {
    float r = 0.15 + 0.85 * grow;
    float l = length(p);
    float ang = atan(p.y, p.x);
    // a disc with its notch: a narrow wedge cut to the heart
    return max(l - r, -(abs(ang) - 0.1 * grow) * l);
}
// a lotus seen from above: two rings of pointed petals opening, a golden heart
float petals(vec2 p, float n, float len, float wid, float off) {
    float a = atan(p.y, p.x) + off;
    float s = 6.2831853 / n;
    float k = mod(a + s * 0.5, s) - s * 0.5;
    vec2 q = vec2(cos(k), sin(k)) * length(p);
    // a pointed ellipse along x from 0 to len
    float t = clamp(q.x / len, 0.0, 1.0);
    float hw = wid * sin(3.1415926 * pow(t, 0.8)) * (1.0 - 0.3 * t);
    return max(abs(q.y) - hw, max(-q.x, q.x - len));
}
void main() {
    float grow = vB.x, seed = vB.y;
    int kind = int(vB.z + 0.5);
    vec2 p = rot(vL, -vA.w);
    float aa = max(length(fwidth(vL)) * 0.8, 1e-3);
    // the water's slope where it floats: it tilts a little with the ripples
    vec2 uv = (vW - (uView.xy - uView.zw)) / (2.0 * uView.zw);
    vec2 tx = 1.0 / vec2(textureSize(uState, 0));
    float gx = texture(uState, uv + vec2(tx.x, 0.0)).r - texture(uState, uv - vec2(tx.x, 0.0)).r;
    float gy = texture(uState, uv + vec2(0.0, tx.y)).r - texture(uState, uv - vec2(0.0, tx.y)).r;
    float tilt = clamp(dot(vec2(gx, gy), normalize(uLightDir.xy + vec2(1e-4))) * 3.0, -0.25, 0.25);
    vec2 so = rot(uShadowOff / max(vA.z, 0.1), -vA.w);
    if (kind == 0) {
        float d = padSdf(p, grow);
        float ds = padSdf(p - so, grow);
        float cov = 1.0 - smoothstep(-aa, aa, d);
        float shA = (1.0 - smoothstep(-0.15, 0.25, ds)) * uShadowA * (1.0 - cov);
        float r = length(p);
        float ang = atan(p.y, p.x);
        // veins from the heart, a slightly raised, paler rim
        float vein = 1.0 - smoothstep(0.0, 0.05, abs(sin(ang * 11.0 + seed * 6.0)) * r);
        float rim = exp(d / 0.04);
        vec3 c = mix(uPad, uPad2, smoothstep(0.2, 0.9, fbm(p * 2.2 + seed * 9.0)) * 0.6);
        c *= 0.96 + 0.08 * vein * smoothstep(0.1, 0.4, r);
        c = mix(c, uPad2 * 1.2, rim * 0.35);
        c *= 1.0 + tilt;
        c += uLight * uLightK * 0.006;
        float a = cov * vB.w;
        float A = max(a, shA * vB.w);
        vec3 outc = c * a;
        frag = vec4(outc, A);
        return;
    }
    // the lotus
    float open = grow;
    float d1 = petals(p, 8.0, 0.25 + 0.6 * open, 0.18 + 0.05 * open, 0.0);
    float d2 = petals(p, 8.0, 0.18 + 0.38 * open, 0.13 + 0.03 * open, 0.39);
    float core = length(p) - (0.1 + 0.06 * open);
    float d = min(min(d1, d2), core);
    float ds = min(d1, d2);
    ds = min(petals(p - so * 0.6, 8.0, 0.25 + 0.6 * open, 0.2, 0.0), length(p - so * 0.6) - 0.3);
    float cov = 1.0 - smoothstep(-aa, aa, d);
    float shA = (1.0 - smoothstep(-0.1, 0.2, ds)) * uShadowA * (1.0 - cov);
    vec3 c = uLotus;
    // the petals' tips blush, the inner ring paler, the heart golden
    float r = length(p);
    c = mix(c, uLotus * vec3(1.0, 0.86, 0.9), smoothstep(0.3, 0.85, r) * 0.5);
    c *= d2 < 0.0 ? 1.08 : 0.92 + 0.08 * smoothstep(-0.05, 0.0, d1);
    c = mix(c, uLotusCore, 1.0 - smoothstep(-aa, aa, core));
    c *= 1.0 + tilt * 0.6;
    c *= 1.0 + uGlow;
    float a = cov * vB.w;
    float A = max(a, shA * vB.w);
    // its glow on the water at night
    float glow = exp(-r * r / 0.5) * uGlow * 0.12 * vB.w * (1.0 - cov);
    frag = vec4(c * a + uLotus * glow, max(A, glow * 0.0));
}
)";

// ---------------------------------------------------------------- the glow
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThresh;
void main() {
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb +
             texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    frag = vec4(max(c - vec3(uThresh), 0.0), 1.0);
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
uniform float uBloomK, uDim, uFade, uBridge, uVignette;
uniform vec3 uBridgeCol, uBridgeInk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    c = c / (1.0 + 0.18 * max(max(c.r, c.g), c.b));
    vec2 v = vUv - 0.5;
    c *= 1.0 - uVignette * dot(v, v) * 2.0;
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
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
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
    float halo = smoothstep(0.15, 0.47, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

inline std::string withCommon(const char* src, bool shape = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace ripples_shaders

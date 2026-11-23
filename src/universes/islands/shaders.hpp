#pragma once
// ISLANDS' shaders (see islands.cpp).
//
// The world is drawn relative to the eye (the camera at the origin, so every float stays small) and bent by the
// sea's curvature (each vertex dropped by curv * d^2 / 2, d its horizontal distance; the curvature grows with the
// altitude, so the horizon curves only from high up). All of it a flat vector illustration: each surface one flat
// fill in two tones by the sun (smooth normals, a crisp antialiased terminator), every colour border antialiased over
// about a pixel from its own screen derivative.
// Passes: the seabed (the terrain's height and distance, full resolution) for the sea's bands; the main pass (sky,
// terrain, objects, then the sea as one fullscreen pass: the curved surface hit per pixel, broad slowly flowing
// strokes, the shallows, the lagoon band and its line, the white surf rim, the boats' wakes), the clouds (flat
// puffs), additive glows; then the well and its flat tiles through the layout's camera, the frame, the HUD.
// Every colour has two paints: the variant's natural one, and the drop's neons (uNeon), blended per pixel by psyK
// (the drop's front washing in from the horizon): the same shapes, only the paint changes.
#include <string>

namespace islands_shaders {

// ---------------------------------------------------------------- common: noise, the sky, the haze, the two tones
inline const char* COMMON = R"(
uniform vec3 uSunDir, uSkyTop, uSkyHor, uSkyLow, uFogCol;
uniform vec3 uEye;          // the camera's position (world)
uniform float uCurv, uTime, uNight, uHaze, uHazeH, uDip;
// the two flat tones: lit (uFlatLit) or on the side away from the sun (uFlatShade)
uniform vec3 uFlatLit, uFlatShade;
// a colour border antialiased in screen space: a threshold of x at e over about a pixel (its width from x's own
// screen derivative, at least the authored minimum mn). (In uniform control flow only: its derivative is taken there.)
float aaStep(float e, float x, float mn) {
    float w = max(fwidth(x) * 0.75, mn);
    return smoothstep(e - w, e + w, x);
}
// (with the width given: w x's screen derivative, taken beforehand in uniform control flow)
float fStep(float e, float x, float w) { return smoothstep(e - w * 0.75, e + w * 0.75, x); }
// a band of stripes: 1 where fract(x) is in [0.5, 1), antialiased over w
float fStripe(float x, float w) {
    float d = abs(fract(x - 0.25) - 0.5) - 0.25;  // the signed distance to the band [0.5, 1) (periodic)
    return clamp(0.5 - d / max(w, 1e-6), 0.0, 1.0);
}
// the two tones by a smooth normal: a crisp terminator (antialiased over a pixel), a little past the sun's side
// (flatSide: 1 on the lit side, 0 on the other)
float flatSide(vec3 n) { return aaStep(0.0, dot(n, uSunDir) + 0.12, 1e-4); }
vec3 flatTone(vec3 n) { return mix(uFlatShade, uFlatLit, flatSide(n)); }
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float n2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
// The sky along a direction: its gradient by the hour (the horizon dips as the camera climbs).
vec3 skyCol(vec3 rd) {
    float e = rd.y + uDip;
    vec3 c = mix(uSkyHor, uSkyTop, pow(smoothstep(0.0, 0.45, e), 0.6));
    return mix(c, uSkyLow, smoothstep(0.0, -0.08, e));
}
// Aerial perspective: an exponential haze thinning with height, one flat grey blue.
float hazeK(float dist, vec3 rd) {
    float b = 1.0 / uHazeH;
    float dy = rd.y;
    float base = uHaze * exp(-max(uEye.y, 0.0) * b);
    float f = abs(dy) > 1e-4 ? base * (1.0 - exp(-dist * dy * b)) / (dy * b) : base * dist;
    return 1.0 - exp(-max(f, 0.0));
}
vec3 fogged(vec3 c, float dist, vec3 rd) { return mix(c, uFogCol, hazeK(dist, rd)); }
)";

// ---------------------------------------------------------------- the strokes and the drop's neons
// The sea's broad liquid strokes: a gently warped noise in world space (anchored to the sea, its size from the
// altitude) flowing slowly, its iso-bands mapped through three colours, crisp but antialiased (and a calm tint where
// they get finer than a pixel, and far away). The drop's palette (uNeon, linear) and its front (psyK).
inline const char* PSY_GLSL = R"(
uniform float uFlow;        // the strokes' clock (a little faster with the music's energy)
uniform float uPulse;       // a soft swell on the beat (smooth: no flicker)
uniform float uSwirl;       // the strokes' size (world meters per noise unit)
uniform vec2 uSwirlO;       // the field's coordinates under the eye (carried along as it flies, so the strokes stay
                            // anchored to the sea, and a change of size zooms about the eye, never slides)
// the drop's palette (the variant's neons), linear
#define N_SEA_A 0
#define N_SEA_B 1
#define N_SEA_C 2
#define N_SEA_CALM 3
#define N_LINE 4
#define N_LAGOON 5
#define N_FOAM 6
#define N_SKY_LOW 7
#define N_SKY_HIGH 8
#define N_CORE 9
#define N_RING0 10
#define N_RING1 11
#define N_RING2 12
#define N_RING3 13
#define N_RING4 14
#define N_ARM_A 15
#define N_ARM_B 16
#define N_HALO 17
#define N_RIM 18
#define N_SAND 19
#define N_GRASS 20
#define N_JUNGLE 21
#define N_SLATE 22
#define N_PEAK 23
#define N_CLIFF 24
#define N_CLIFF_HI 25
#define N_FAR 26
#define N_CLOUD_LIT 27
#define N_CLOUD_SHADE 28
#define N_TREE_A 29
#define N_TREE_B 30
#define N_TREE_HI 31
uniform vec3 uNeon[32];
// the field's coordinates of a point of the sea (k: how much larger the strokes are there, about the eye: 1 near it,
// where they stay anchored to the sea)
vec2 swirlP(vec2 wp, float k) { return uSwirlO + (wp - uEye.xz) / (uSwirl * k); }
vec3 tl(vec3 c) { return pow(c, vec3(2.2)); }
// a step from a to b at x = e, antialiased over w
vec3 tstep(vec3 a, vec3 b, float x, float e, float w) { return mix(a, b, smoothstep(e - w, e + w, x)); }
float fbm2(vec2 p) { return (n2(p) * 0.667 + n2(p * 2.03 + vec2(11.3, 7.9)) * 0.333); }
// the sea's bands in three colours on a path there and back (the first never meets the last), and a calm one
vec3 seaBands(float v, float w, float far, vec3 a, vec3 b, vec3 c3, vec3 calm) {
    float x = abs(fract(v) * 2.0 - 1.0);
    float aa = max(w * 2.0, 1e-4);
    vec3 c = tstep(a, b, x, 0.38, aa);
    c = tstep(c, c3, x, 0.74, aa);
    return mix(c, calm, max(smoothstep(0.08, 0.3, w), far));
}
// the sea's field at a point of the sea (t its distance, alt the eye's height, depth the water's under it: the
// bands bend round the shores, within about bend meters of depth)
float seaField(vec2 wp, float t, float alt, float depth, float bend) {
    float tf = t / (alt * 10.0 + 250.0);
    vec2 p = swirlP(wp, 1.7 * (1.0 + t / (alt * 4.0 + 60.0) + tf * tf));
    float ft = uFlow;
    vec2 q = vec2(fbm2(p + vec2(0.0, 0.05 * ft)), fbm2(p + vec2(5.2, 1.3) - vec2(0.04 * ft, 0.0)));
    return fbm2(p + 1.7 * q + vec2(0.025 * ft, -0.02 * ft)) * 5.5 + 0.45 * exp(-depth / bend) + 0.015 * ft + 0.03 * uPulse;
}
// the land far away in a drop: one colour, a flat silhouette over the sea (instead of the haze)
vec3 neonFar(vec3 c, float dist) {
    float k = smoothstep(uEye.y * 5.0 + 300.0, uEye.y * 16.0 + 2000.0, dist);
    return mix(c, uNeon[N_FAR], 0.85 * k);
}
// an object's colour pushed to neon: saturated, the whites to pale pastels (by its seed)
vec3 neonOf(vec3 a, float seed) {
    vec3 s = sqrt(max(a, 0.0));
    float l = dot(s, vec3(0.3, 0.55, 0.15));
    float mx = max(s.r, max(s.g, s.b)), mn = min(s.r, min(s.g, s.b));
    vec3 c = clamp(mix(vec3(l), s, 1.9), 0.0, 1.0);
    float h = fract(seed * 7.31);
    vec3 pastel = h < 0.33 ? vec3(1.0, 0.75, 0.95) : h < 0.66 ? vec3(1.0, 0.93, 0.55) : vec3(0.65, 0.95, 1.0);
    c = mix(c, pastel, smoothstep(0.7, 0.85, l) * (1.0 - smoothstep(0.08, 0.25, mx - mn)));
    return tl(c);
}
// The drop: how much of the neon paint a point takes (0 the natural colours .. 1 the neons). uPsy carries the drop's
// front: it washes in from the horizon toward the eye (the sky and the far sea first) as uPsy rises, and recedes the
// same way, back out to the horizon, as it falls; a soft, slowly wavering edge (never a flash: the colours cross over
// a broad band of the view, each point over about a quarter of a second)
uniform float uPsy;
float psyK(float dist, vec3 rd) {
    if (uPsy <= 0.0) return 0.0;
    if (uPsy >= 1.0) return 1.0;
    float s = clamp(log2(max(dist, 1.0) / (max(uEye.y, 1.0) + 20.0)) / 5.0, 0.0, 1.0);
    float az = atan(rd.z, rd.x);
    float wob = 0.05 * sin(az * 3.0 + uFlow * 0.35) + 0.03 * sin(az * 7.0 - uFlow * 0.6 + 1.3);
    const float W = 0.15, A = 0.08;
    float f = mix(1.0 + W + A, -W - A, uPsy);
    return smoothstep(f - W, f + W, s + wob);
}
)";

// ---------------------------------------------------------------- the sky (fullscreen, behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uStars;
uniform vec3 uRingSun;      // the ringed sun's direction
// A huge ringed sun (wavy rings flowing outward), spiral arms curling out of it, a halo ring hugging it, over the sky
// c: k 1 the drop's neons (opaque), 0 the natural paint (a pale sun in soft creams, see through; its arms and halo
// faint lighter tints of the sky)
vec3 ringedSky(vec3 rd, vec3 c, float k) {
    vec3 S = normalize(uRingSun);
    float r = acos(clamp(dot(rd, S), -1.0, 1.0));
    vec3 ax = normalize(cross(S, vec3(0.0, 1.0, 0.0))), ay = cross(ax, S);
    float th = atan(dot(rd, ay), dot(rd, ax));
    const float R = 0.24;
    float u = r / R;
    float aa = fwidth(u) * 1.2;
    // the rings (wavy, drifting out)
    float kr = u * 2.4 - 0.05 * uFlow + 0.1 * u * sin(th * 3.0 + u * 4.0 - 0.15 * uFlow) - 0.04 * uPulse;
    float x = fract(kr), aw = max(fwidth(kr), 1e-4);
    vec3 r0 = mix(tl(vec3(1.0, 0.95, 0.8)), uNeon[N_RING0], k);
    vec3 ring = r0;
    ring = tstep(ring, mix(tl(vec3(1.0, 0.9, 0.7)), uNeon[N_RING1], k), x, 0.22, aw);
    ring = tstep(ring, mix(tl(vec3(1.0, 0.97, 0.86)), uNeon[N_RING2], k), x, 0.45, aw);
    ring = tstep(ring, mix(tl(vec3(1.0, 0.88, 0.68)), uNeon[N_RING3], k), x, 0.62, aw);
    ring = tstep(ring, mix(tl(vec3(0.99, 0.93, 0.82)), uNeon[N_RING4], k), x, 0.84, aw);
    ring = tstep(ring, r0, x, 0.97, aw);
    vec3 core = mix(tl(vec3(1.0, 0.98, 0.9)), uNeon[N_CORE], k);
    vec3 sun = mix(core, ring, smoothstep(0.26 - aa, 0.26 + aa, u));
    vec3 sky = c;
    // (the natural one see through: its rings a soft glow over the sky, the core more solid)
    float op = mix(mix(0.55, 0.85, 1.0 - smoothstep(0.26 - aa, 0.26 + aa, u)), 1.0, k);
    c = mix(mix(c, sun, op), c, smoothstep(1.0 - aa, 1.0 + aa, u));
    // the arms: three spirals curling out to about three times its radius, thinning out
    float sp = th / 6.2831853 * 3.0 + log(max(u, 1e-3)) * 1.1 - 0.012 * uFlow;
    float f = fract(sp), fw = max(fwidth(sp), 1e-4);
    float band = smoothstep(0.0, fw, f) * (1.0 - smoothstep(0.07 - fw, 0.07, f));
    float reach = smoothstep(0.98, 1.05, u) * (1.0 - smoothstep(1.4, 2.3, u));
    vec3 arm = mix(uNeon[N_ARM_A], uNeon[N_ARM_B], step(0.5, fract(floor(sp) * 0.5)));
    c = mix(c, mix(mix(sky, vec3(1.0), 0.22), arm, k), band * reach);
    // a halo ring hugging it
    c = mix(c, mix(mix(sky, vec3(1.0), 0.3), uNeon[N_HALO], k), (smoothstep(1.0, 1.04, u) - smoothstep(1.1, 1.2, u)) * 0.8);
    return c;
}
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize(w.xyz / w.w);
    vec3 c = skyCol(rd);
    float e = rd.y + uDip;
    // stars at nightfall
    if (uStars > 0.01 && e > 0.0) {
        vec3 sp = rd * 260.0;
        vec3 si = floor(sp);
        float hs = fract(sin(dot(si, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        vec3 fp = fract(sp) - 0.5;
        float st = smoothstep(0.12, 0.0, length(fp)) * step(0.985, hs);
        c += vec3(0.9, 0.92, 1.0) * st * uStars * smoothstep(0.03, 0.3, e) * (0.6 + 0.4 * sin(uTime * 2.0 + hs * 40.0));
    }
    // the ringed sun in pale creams; in a drop the neon sky and rings
    float k = psyK(1e6, rd);
    vec3 neon = mix(uNeon[N_SKY_LOW], uNeon[N_SKY_HIGH], smoothstep(0.0, 0.4, e));
    frag = vec4(ringedSky(rd, mix(c, neon, k), k), 0.0);
}
)";

// ---------------------------------------------------------------- the terrain (one mesh per island)
inline const char* TERRAIN_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;
uniform mat4 uVP;
uniform vec3 uOff;      // the island's center, relative to the eye
uniform float uCurv;
out vec3 vRel, vLocal, vNrm;
out vec4 vAttrS;        // the materials, interpolated (their borders smooth curves, not the facets' edges)
void main() {
    vec3 rel = aPos + uOff;
    rel.y -= 0.5 * uCurv * dot(rel.xz, rel.xz);
    vLocal = aPos;
    vRel = rel;
    vNrm = aNrm;
    vAttrS = aAttr;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

// Big flat shapes by the smooth surface (no facets): the white surf rim, the sand, the greens (a brighter low band,
// the darker jungle above), the slate mountains and the cliffs, two tones by the sun (every border antialiased over
// about a pixel: aaStep). In a drop the same shapes take the neons (psyK): the rim, the sand, the two greens, the
// slate, the peaks, the cliffs; two tones 1 and 0.8 by the same terminator, far away one silhouette instead of the haze.
inline const char* TERRAIN_FS = R"(#version 330 core
in vec3 vRel, vLocal, vNrm;
in vec4 vAttrS;
layout(location = 0) out vec4 frag;
uniform int uMode;        // 0 main, 1 the seabed (its height and distance only, for the sea's bands)
uniform vec3 uSand, uSandWet, uGrass, uJungle, uRock, uCliff;
uniform float uSeed, uFallA, uFallW;
uniform vec3 uFoam;
void main() {
    float dist = length(vRel);
    if (uMode == 1) {
        // (red: the height over the sea; alpha: the distance, negative for the land above the water)
        frag = vec4(vLocal.y, 0.0, 0.0, vLocal.y > 0.15 ? -dist : dist);
        return;
    }
    vec3 n = normalize(vNrm);
    float y = vLocal.y;
    float steep = 1.0 - n.y;
    float sandK = vAttrS.x, vegK = vAttrS.y, rockK = vAttrS.z;
    float mSand = aaStep(0.7, y, 1e-3);
    float veg = aaStep(0.5, vegK, 0.01) * (1.0 - aaStep(0.75, sandK, 0.01));
    // (the bands' lines wander a little, as drawn by hand)
    float wob = n2(vLocal.xz * 0.018 + uSeed) - 0.5;
    float mJungle = aaStep(9.0, y + wob * 6.0, 1e-3);
    // the peaks: slate on the steep faces above the forest line (the mountains' darker shapes)
    float slate = aaStep(40.0, y + wob * 20.0, 1e-3) * aaStep(0.3, steep + wob * 0.2, 1e-3);
    float high = aaStep(110.0, y + wob * 30.0, 1e-3);
    // (the rock: the material, or any sheer face over the beach: the interpolated material dips between its
    // vertices at a cliff's foot, sand showing through there)
    float rk = max(aaStep(0.3, rockK, 0.01), aaStep(0.55, steep, 1e-3) * aaStep(1.5, y, 1e-3));
    float cliffHi = aaStep(48.0, y + wob * 20.0, 1e-3);
    // a waterfall down the cliff (under the water: the surf's white, no seam at the waterline)
    float fall = 0.0;
    if (uFallW > 0.0) {
        float a = atan(vLocal.z, vLocal.x);
        float da = mod(a - uFallA + 3.14159265, 6.2831853) - 3.14159265;
        fall = (1.0 - aaStep(0.8, abs(da / uFallW), 1e-4)) * aaStep(0.25, rockK, 0.01);
    }
    float side = flatSide(n);
    vec3 rdf = vRel / max(dist, 1e-3);
    float k = psyK(dist, rdf);
    vec3 colN = vec3(0.0), colP = vec3(0.0);
    if (k < 1.0) {
        vec3 col = mix(uSandWet, uSand, mSand);
        vec3 green = mix(uGrass, uJungle, mJungle);
        green = mix(green, uRock, max(slate, high));
        col = mix(col, green, veg);
        col = mix(col, mix(uCliff, uRock, cliffHi), rk);
        col = mix(col, uFoam, fall);
        colN = fogged(col * mix(uFlatShade, uFlatLit, side), dist, rdf);
    }
    if (k > 0.0) {
        vec3 col = mix(uNeon[N_RIM], uNeon[N_SAND], mSand);
        vec3 green = mix(uNeon[N_GRASS], uNeon[N_JUNGLE], mJungle);
        green = mix(green, uNeon[N_SLATE], slate);
        green = mix(green, uNeon[N_PEAK], high);
        col = mix(col, green, veg);
        col = mix(col, mix(uNeon[N_CLIFF], uNeon[N_CLIFF_HI], cliffHi), rk);
        col = mix(col, uNeon[N_FOAM], fall);
        colP = neonFar(col * mix(0.8, 1.0, side), dist);
    }
    frag = vec4(mix(colN, colP, k), 0.0);
}
)";

// ---------------------------------------------------------------- the objects (instanced meshes)
// Instance: a = position, seed; b = scale, material; c = color, param; d = rotation (quaternion).
inline const char* OBJ_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aAux;
layout(location = 3) in vec4 iA;
layout(location = 4) in vec4 iB;
layout(location = 5) in vec4 iC;
layout(location = 6) in vec4 iD;
uniform mat4 uVP;
uniform vec3 uEye;
uniform float uCurv, uTime, uWind;
out vec3 vRel, vObj, vNrm;
out vec2 vAux;
flat out vec3 vCol, vSize;
flat out float vMat, vParam, vSeed;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float hh(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
float nn(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hh(i), hh(i + vec3(1, 0, 0)), f.x), mix(hh(i + vec3(0, 1, 0)), hh(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hh(i + vec3(0, 0, 1)), hh(i + vec3(1, 0, 1)), f.x), mix(hh(i + vec3(0, 1, 1)), hh(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
    vec3 pos = aPos;
    vec3 nrm = aNrm;
    int mat = int(iB.w + 0.5);
    if (mat == 7 || mat == 12) {
        // foliage and rocks: a lumpy outline
        vec3 d = normalize(aPos);
        float k = nn(d * 2.4 + iA.w * 17.0) * 0.55 + nn(d * 5.5 + iA.w * 5.0) * 0.3 + nn(d * 11.0 + iA.w * 3.0) * 0.15;
        float amp = mat == 12 ? 0.35 : 0.5;
        pos = d * 0.5 * (1.0 - amp * 0.5 + amp * k);
        nrm = normalize(d + (vec3(nn(d * 6.0 + 1.0), nn(d * 6.0 + 2.0), nn(d * 6.0 + 3.0)) - 0.5) * 0.6);
    }
    if (mat == 8 && aAux.x > 0.5) {
        // the palm's fronds sway in the breeze, more toward their tips
        float t = aAux.y;
        float ph = uTime * (1.1 + 0.3 * fract(iA.w * 7.0)) + iA.w * 40.0 + pos.x * 0.3 + pos.z * 0.2;
        pos += vec3(sin(ph), sin(ph * 1.3 + 1.0) * 0.6, cos(ph * 0.9)) * (0.12 + 0.18 * uWind) * t * t;
    }
    if (mat == 16 && aAux.x > 0.5) {
        // a bird's wing beats (or glides: param 0)
        float fl = sin(uTime * 9.0 + iA.w * 30.0) * iC.a;
        pos.y += fl * aAux.y * 0.28;
        nrm = normalize(vec3(0.0, 1.0, -fl * sign(pos.z) * 0.6));
    }
    vec3 obj = pos * iB.xyz;
    vec3 w = iA.xyz + qrot(iD, obj);
    vec3 rel = w - uEye;
    rel.y -= 0.5 * uCurv * dot(rel.xz, rel.xz);
    vRel = rel;
    vObj = obj;
    vNrm = qrot(iD, normalize(nrm / max(iB.xyz, vec3(1e-4))));
    vAux = aAux;
    vCol = iC.rgb;
    vSize = iB.xyz;
    vMat = iB.w;
    vParam = iC.a;
    vSeed = iA.w;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

// The albedo (the material's flat colour, its painted bands antialiased: derivatives taken before the branches) as a
// flat fill in two tones by the mesh's smooth normal, the lights at dusk; the trees one flat green each (a clean
// silhouette). In a drop: the colours pushed to neon, the palms in zebra stripes, the trees in the palette's greens
// (another colour up the mountains), tones 1 and 0.8, the far silhouette.
inline const char* OBJ_FS = R"(#version 330 core
in vec3 vRel, vObj, vNrm;
in vec2 vAux;
flat in vec3 vCol, vSize;
flat in float vMat, vParam, vSeed;
layout(location = 0) out vec4 frag;
uniform vec3 uLeaf, uLeaf2, uTrunk, uFoliage, uFoliage2, uWater, uWaterGlow, uRoomNight;
uniform vec3 uSailPal[7];   // the fleet's sail colors (the pieces')
uniform float uLights;      // the lights at dusk (windows, the pool, the lighthouse)
void main() {
    vec3 world = vRel + uEye;
    int mat = int(vMat + 0.5);
    vec3 fwo = fwidth(vObj);
    float fwaux = fwidth(vAux.y);
    float fwang = fwidth(atan(vObj.z, vObj.x));
    fwang = fwang > 3.0 ? 0.0 : fwang;  // (not across the angle's wrap)
    // (the drop: the palms' zebra stripes across the trunk, wavy across the fronds, antialiased)
    float zebraT = 0.0, zebraF = 0.0;
    if (uPsy > 0.0) {
        float zt = vObj.y / max(vSize.y, 0.01) * 0.8 + sin(vObj.x * 4.0) * 0.3;
        float zf = vAux.y * 7.0 + sin(vObj.x * 2.3 + vObj.z * 1.7) * 0.6;
        zebraT = fStripe(zt, fwidth(zt));
        zebraF = fStripe(zf, fwidth(zf));
    }
    float dist = length(vRel);
    vec3 V = -vRel / max(dist, 1e-3);
    vec3 alb = vCol;
    float emit = 0.0;
    vec3 emitCol = vec3(0.0);
    if (mat == 1) {             // glass: a flat dark blue pane, the room's light at dusk
        alb = mix(vec3(0.06, 0.12, 0.2), skyCol(reflect(-V, normalize(vNrm))) * 0.6, 0.5);
        float on = uLights * step(0.5, vParam);
        emitCol = uRoomNight * on * (vParam > 1.5 ? 4.0 : 1.0);
        emit = 1.0;
    } else if (mat == 2) {      // deck: flat planks, every other one a shade darker
        vec2 ext = vSize.x > vSize.z ? vec2(vObj.x, vObj.z) : vec2(vObj.z, vObj.x);
        alb *= 0.9 + 0.1 * fStripe(ext.y / 0.32, (vSize.x > vSize.z ? fwo.z : fwo.x) / 0.32) * step(0.5, normalize(vNrm).y);
    } else if (mat == 4) {      // thatch: flat rings down the slope
        alb *= 0.86 + 0.14 * fStripe(vObj.y * 2.2, fwo.y * 2.2);
    } else if (mat == 5) {      // the pool: flat turquoise, a glow at night
        alb = uWater;
        emitCol = uWaterGlow * uLights;
        emit = 1.0;
    } else if (mat == 7) {      // foliage: one green per tree (param: 0 jungle, 1 olive, 2 cypress, 3 hedge)
        alb = vParam < 0.5 ? mix(uFoliage, uFoliage2, step(0.5, fract(vSeed * 3.7))) : vCol;
    } else if (mat == 8) {      // the palm: a flat trunk, fronds green to yellow toward their tips
        if (vAux.x < 0.5) alb = uTrunk;
        else if (vAux.x < 1.5) alb = mix(uLeaf, uLeaf2, fStep(0.7, vAux.y, fwaux) * 0.6 + 0.2 * step(0.5, fract(vSeed * 5.3)));
        else alb = vec3(0.32, 0.24, 0.1);
    } else if (mat == 9) {      // sail: flat cloth (the fleet's: bands of the cleared row's colors)
        if (vParam > 0.5) {
            float code = vParam - 1.0;
            float n = code < 8.0 ? 1.0 : code < 64.0 ? 2.0 : code < 512.0 ? 3.0 : 4.0;
            float band = min(floor(vAux.y * n * 1.02), n - 1.0);
            float d = mod(floor(code / pow(8.0, n - 1.0 - band)), 8.0);
            int ti = int(d + 0.5) - 1;
            vec3 c = uSailPal[0];
            for (int k = 0; k < 7; k++) if (k == ti) c = uSailPal[k];
            alb = c;
        }
    } else if (mat == 10) {     // a hull: its paint, a stripe along the sheer, the deck
        if (vAux.x > 0.5) alb = vec3(0.62, 0.45, 0.28);
        else {
            float y = vObj.y / max(vSize.y, 1e-3);
            vec3 stripe = vParam > 2.5 ? vec3(0.6, 0.08, 0.08) : vParam > 1.5 ? vec3(0.04, 0.32, 0.4) : vec3(0.04, 0.08, 0.2);
            alb = mix(alb, stripe, fStep(-0.165, y, fwo.y / max(vSize.y, 1e-3)) * (1.0 - fStep(-0.065, y, fwo.y / max(vSize.y, 1e-3))) * step(0.5, vParam));
        }
    } else if (mat == 11) {     // canvas: an umbrella's stripes
        float a = atan(vObj.z, vObj.x);
        alb = mix(vCol, vec3(0.97, 0.95, 0.9), fStripe(a / 6.2831853 * 8.0, fwang * 8.0 / 6.2831853));
    } else if (mat == 12) {     // rock: a boulder
        alb = vec3(0.46, 0.4, 0.34) * (0.85 + 0.25 * fract(vSeed * 7.7));
    } else if (mat == 13) {     // a lamp
        emitCol = vCol * 4.0;
        emit = 1.0;
    } else if (mat == 15) {     // the lighthouse: white with red bands
        float y = vObj.y / vSize.y + 0.5;
        alb = mix(vec3(0.95), vec3(0.75, 0.1, 0.08), fStripe(y * 3.0 + 0.25, fwo.y / vSize.y * 3.0) * fStep(0.15, y, fwo.y / vSize.y));
    } else if (mat == 16) {     // a seabird: white, a grey back, dark tips
        alb = vParam < 0.5 && vCol.r < 0.3 ? vCol : mix(vec3(0.95), vec3(0.55, 0.58, 0.62), step(0.5, vAux.x) * step(0.4, vAux.y));
        alb = mix(alb, vec3(0.08), step(0.5, vAux.x) * step(0.9, vAux.y));
    } else if (mat == 17) {     // roof tiles: flat rows
        alb *= 0.88 + 0.12 * fStripe(vObj.y * 4.0 / max(vSize.y, 0.1), fwo.y * 4.0 / max(vSize.y, 0.1));
    }
    vec3 n = normalize(vNrm);
    if (dot(n, vRel) > 0.0 && mat != 8 && mat != 9) n = -n;
    float side = flatSide(n);
    float k = psyK(dist, -V);
    vec3 colN = vec3(0.0), colP = vec3(0.0);
    if (k < 1.0) {
        // (the trees one flat tone: no lumpy texture)
        vec3 tone = mat == 7 ? mix(uFlatShade, uFlatLit, 0.6) : mix(uFlatShade, uFlatLit, side);
        colN = fogged(alb * tone + emitCol * emit, dist, -V);
    }
    if (k > 0.0) {
        vec3 a = neonOf(alb, vSeed);
        float sd = mix(0.8, 1.0, side);
        if (mat == 8) {
            float h = fract(vSeed * 3.17 + (vAux.x < 0.5 ? 0.0 : 0.37));
            vec3 c0 = h < 0.33 ? uNeon[N_SLATE] : h < 0.66 ? uNeon[N_RIM] : uNeon[N_SEA_A];
            vec3 c1 = h < 0.33 ? uNeon[N_CLIFF] : h < 0.66 ? uNeon[N_SLATE] : uNeon[N_HALO];
            a = mix(c0, c1, vAux.x < 0.5 ? zebraT : zebraF);
        } else if (mat == 7 && vParam < 0.5) {
            float y = world.y - vObj.y + 0.5 * uCurv * dot(vRel.xz, vRel.xz);
            a = y > 40.0 ? uNeon[N_TREE_HI] : fract(vSeed * 5.71) < 0.75 ? uNeon[N_TREE_A] : uNeon[N_TREE_B];
            sd = 0.9;
        }
        colP = neonFar(a * sd, dist) + emitCol * emit * 0.5;
    }
    frag = vec4(mix(colN, colP, k), 0.0);
}
)";

// ---------------------------------------------------------------- the sea (fullscreen: the curved surface per pixel)
// Broad slowly flowing strokes in close tints of the sea's blue (calm), the lighter cyan shallows round the islands
// (the strokes faint through them), a fine line round the pale lagoon band, the white surf rim round the beaches, the
// boats' wakes and the falls' feet; lit flat. In a drop the same shapes in the neons.
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uVP;
uniform sampler2D uSeabed;
uniform vec3 uShallow, uDeep, uFoam;
uniform vec2 uInvRes;
uniform vec4 uBoats[24];     // x z, heading, speed
uniform vec4 uBoatsB[24];    // length, beam, kind, -
uniform int uNumBoats;
uniform vec4 uFalls[4];      // x z, radius, -
uniform int uNumFalls;
// smoothstep over w on either side of e (the wakes: w the pixel's footprint)
float wstep(float e, float x, float w) { return smoothstep(e - w, e + w, x); }
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize(w.xyz / w.w);
    float alt = uEye.y;
    float dy = rd.y;
    if (dy >= -1e-5) discard;
    // the curved sea: alt + t dy + curv/2 (t h)^2 = 0, the near root
    float h2 = 1.0 - dy * dy;
    float a = 0.5 * uCurv * h2;
    float disc = dy * dy - 4.0 * a * alt;
    if (disc < 0.0) discard;
    float t = 2.0 * alt / (-dy + sqrt(disc));
    vec3 P = rd * t;
    vec4 clip = uVP * vec4(P, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    vec2 wp = P.xz + uEye.xz;
    // a pixel's footprint on the sea (stretched at grazing angles)
    float foot = t * 0.9 * uInvRes.y / max(-dy, 0.04);
    float mid = 1.0 - smoothstep(800.0 + alt * 4.0, 6000.0 + alt * 8.0, t);
    // the depth under the surface: the floor's height under the sea (the seabed pass), filtered (bilinear over the
    // valid texels: a texel of ground well in front of this water, another island's, is no floor of it; no terrain
    // there is the deep)
    vec2 hp = gl_FragCoord.xy - 0.5;
    ivec2 h0 = ivec2(floor(hp));
    vec2 hf = hp - vec2(h0);
    ivec2 hm = textureSize(uSeabed, 0) - 1;
    vec4 y4, v4;
    for (int i = 0; i < 4; i++) {
        vec4 s = texelFetch(uSeabed, clamp(h0 + ivec2(i % 2, i / 2), ivec2(0), hm), 0);
        y4[i] = s.a == 0.0 ? -60.0 : s.r;
        v4[i] = s.a != 0.0 && abs(s.a) < t * 0.96 - 2.0 ? 0.0 : 1.0;
    }
    vec4 bw = vec4((1.0 - hf.x) * (1.0 - hf.y), hf.x * (1.0 - hf.y), (1.0 - hf.x) * hf.y, hf.x * hf.y) * v4;
    float bsum = bw.x + bw.y + bw.z + bw.w;
    float depthF = bsum > 1e-4 ? clamp(-dot(y4, bw) / bsum, 0.0, 60.0) : 60.0;
    // (the surf rim's width breathing slowly)
    float foamW = 1.1 + 0.12 * sin(uTime * 0.6 + wp.x * 0.01 + wp.y * 0.008);
    // how much of the pixel is shallower than the shallows (9 m), the lagoon band (3.2 m), the surf rim: four samples
    // a pixel (at +-1/4 pixel, the floor filtered between its texels), each thresholded (smoothly over a little of the
    // depth's slope): the borders antialiased like the edges of geometry, even where the floor drops many meters
    // within a pixel (a shelf's edge seen far away)
    vec3 bands = vec3(0.0);
    ivec2 c0 = ivec2(floor(hp + 0.5));
    float yv[9], vv[9];
    for (int k = 0; k < 9; k++) {
        vec4 s = texelFetch(uSeabed, clamp(c0 + ivec2(k % 3 - 1, k / 3 - 1), ivec2(0), hm), 0);
        yv[k] = s.a == 0.0 ? -60.0 : s.r;
        vv[k] = s.a != 0.0 && abs(s.a) < t * 0.96 - 2.0 ? 0.0 : 1.0;
    }
    vec3 th = vec3(9.0, 3.2, foamW);
    float fwd = fwidth(depthF);
    vec3 wd = min(vec3(fwd * 0.5), th * 0.3);
    for (int q = 0; q < 4; q++) {
        vec2 p = vec2(1.0) + (hp - vec2(c0)) + vec2(q % 2 == 0 ? -0.25 : 0.25, q < 2 ? -0.25 : 0.25);
        ivec2 b = clamp(ivec2(floor(p)), ivec2(0), ivec2(1));
        vec2 f = clamp(p - vec2(b), 0.0, 1.0);
        int i0 = b.y * 3 + b.x;
        vec4 w4 = vec4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y) *
                  vec4(vv[i0], vv[i0 + 1], vv[i0 + 3], vv[i0 + 4]);
        float ws = w4.x + w4.y + w4.z + w4.w;
        float d = ws > 1e-4 ? clamp(-dot(vec4(yv[i0], yv[i0 + 1], yv[i0 + 3], yv[i0 + 4]), w4) / ws, 0.0, 60.0) : 60.0;
        bands += 1.0 - smoothstep(th - wd, th + wd, vec3(d));
    }
    bands *= 0.25;
    // a fine line round the lagoon band, outside it: about 1.6 m wide on the water (from the floor's slope under the
    // pixel: d - 3.2 m of depth over the slope is the distance to the band, in pixels), at least a pixel and a half;
    // none where the floor runs flat at that depth (it would spread into patches), nor across a jump of the floor out
    // in the deep
    float px = (depthF - 3.2) / max(fwd, 1e-4);
    float lw = max(1.6 / max(foot, 1e-4), 1.5);
    float line = (1.0 - smoothstep(lw - 0.75, lw + 0.75, px)) * (1.0 - bands.y) * smoothstep(0.02, 0.06, fwd / max(foot, 1e-4)) *
                 (1.0 - smoothstep(5.0, 7.0, depthF));
    // the strokes (bending round the shores only in the shallows: further out the floor ends with the island's mesh,
    // a seam in the strokes)
    float v = seaField(wp, t, alt, depthF, 3.5);
    float vw = fwidth(v);
    float farS = smoothstep(alt * 6.0 + 300.0, alt * 30.0 + 3500.0, t);
    vec3 mids = mix(uDeep, uShallow, 0.5);
    vec3 col = seaBands(v, vw, farS, uDeep * 0.86, uDeep, mids * 1.05, uDeep * 0.95);
    col = mix(col, mix(col, uShallow, 0.7), bands.x);
    col = mix(col, mix(uShallow, uDeep, 0.75), line);
    col = mix(col, mix(uShallow, vec3(1.0), 0.12), bands.y);
    // (the warm light late in the day only dims it, the sky's color tints it a little: never a green sea; at midday
    // its colors exactly)
    float warm = clamp((uFlatLit.r - uFlatLit.b) * 2.0, 0.0, 1.0);
    float lum = mix(1.0, dot(uFlatLit, vec3(0.3, 0.5, 0.2)), 0.5);
    col = mix(col * lum, uSkyHor * lum, 0.1 * warm);
    // (the drop: the strokes in three neons, the shallows no different, the line and the lagoon band in theirs)
    vec3 colP = vec3(0.0);
    if (uPsy > 0.0) {
        colP = seaBands(v, vw, farS, uNeon[N_SEA_A], uNeon[N_SEA_B], uNeon[N_SEA_C], uNeon[N_SEA_CALM]);
        colP = mix(colP, uNeon[N_LINE], line);
        colP = mix(colP, uNeon[N_LAGOON], bands.y);
    }
    // white: the surf rim round the beaches, the boats' wakes, the falls' feet (their edges over about a pixel)
    float foam = bands.z * mid;
    float aw0 = foot * 0.7;
    for (int i = 0; i < 24; i++) {
        if (i >= uNumBoats) break;
        vec2 d = wp - uBoats[i].xy;
        float L = uBoatsB[i].x;
        float sp = uBoats[i].w;
        bool sailing = uBoatsB[i].z < 1.5;  // (sailboats leave a lighter, shorter wake than motorboats)
        float wl = sailing ? 6.0 + sp * 4.0 + L * 1.5 : 10.0 + sp * 10.0;
        if (dot(d, d) > (wl + L) * (wl + L)) continue;
        vec2 fw = vec2(cos(uBoats[i].z), sin(uBoats[i].z));
        vec2 sd = vec2(-fw.y, fw.x);
        float along = -dot(d, fw) - L * 0.35;  // behind the stern: > 0
        float across = dot(d, sd);
        float B = uBoatsB[i].y;
        float sk = clamp(sp / 12.0, 0.0, 1.0);
        if (along > -L) {
            // a white wedge narrowing behind the stern, the two arms of the V (straight, tapering to a point)
            float al = max(along, 0.0);
            // (each faded out once thinner than a pixel: no broken dashes far away; seen from low down, the camera
            // near the water, the wedge shorter and the arms gone: long thin shapes there crossed the whole view)
            float low = smoothstep(6.0, 45.0, alt);
            float len = min(wl * (sailing ? 0.25 : 0.3), 6.0 + L * 2.0) * (0.4 + 0.6 * sk) * mix(0.35, 1.0, low);
            float tw = B * 0.45 * (1.0 - al / max(len, 1e-3));
            float trail = wstep(abs(across), tw, aw0) * wstep(-L * 0.5, along, aw0) * step(foot * 0.5, B * 0.4);
            float armL = min(wl * 0.25, 5.0 + L * 1.5) * sk;
            float aw = 0.12 + B * 0.12;
            float arm = abs(abs(across) - al * 0.36 - B * 0.4);
            float vv2 = wstep(arm, aw * (1.0 - al / max(armL, 1e-3)), aw0) * wstep(0.0, along, aw0) * step(foot * 0.7, aw) * step(25.0, alt);
            foam = max(foam, max(trail, vv2 * step(0.15, sk)));
        }
    }
    for (int i = 0; i < 4; i++) {
        if (i >= uNumFalls) break;
        foam = max(foam, wstep(length(wp - uFalls[i].xy), uFalls[i].z * 0.6, aw0));
    }
    col = fogged(mix(col, uFoam * uFlatLit, clamp(foam, 0.0, 1.0)), t, rd);
    if (uPsy > 0.0) {
        // (the drop: the surf rim, the wakes, the falls' feet in the palette's foam, no haze)
        colP = mix(colP, uNeon[N_FOAM], clamp(foam, 0.0, 1.0));
        col = mix(col, colP, psyK(t, rd));
    }
    frag = vec4(col, 0.0);
}
)";

// ---------------------------------------------------------------- clouds: flat puffs (instanced sprites)
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz (relative to the eye), radius
layout(location = 2) in vec4 iP;     // seed, alpha, flat bottom (y, relative), shade
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec3 vRel;
flat out vec4 vP;
flat out float vR;
void main() {
    vQ = aCorner;
    vP = iP;
    vR = iPos.w;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    vRel = w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
// Each puff a plain disc, the cluster's bottom cut flat, its lower part a flat pale blue grey (one crisp line), faded
// into the haze; in a drop the palette's lit colour over its shade, no haze.
inline const char* CLOUD_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vRel;
flat in vec4 vP;
flat in float vR;
out vec4 frag;
uniform vec3 uCloudLit, uCloudShade;
void main() {
    float r = length(vQ);
    float aa = fwidth(r) * 1.2;
    float a = 1.0 - smoothstep(0.96 - aa, 0.96, r);
    float wy = vRel.y + uEye.y;
    float wb = max(vR * 0.02, fwidth(wy) * 0.75);
    a *= smoothstep(vP.z - wb, vP.z + wb, wy);
    if (a < 0.003) discard;
    float lo = vRel.y + uEye.y - (vP.z + vR * 0.3);
    float wl = max(fwidth(lo), 1e-3);
    float top = smoothstep(-wl, wl, lo);
    vec3 fc = mix(uCloudShade, uCloudLit, top);
    vec3 rdc = normalize(vRel);
    fc = mix(fc, uFogCol, hazeK(length(vRel), rdc) * 0.7);
    float k = psyK(length(vRel), rdc);
    fc = mix(fc, mix(uNeon[N_CLOUD_SHADE], uNeon[N_CLOUD_LIT], top), k);
    a *= vP.y;
    frag = vec4(fc * a, a);
}
)";

// ---------------------------------------------------------------- glows: additive sprites (lights at dusk, the beam)
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz (relative), size
layout(location = 2) in vec4 iCol;   // rgb, softness
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec4 vCol;
void main() {
    vQ = aCorner;
    vCol = iCol;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
// A crisp disc; the softest ones (the beam, the mist) kept soft.
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vQ;
in vec4 vCol;
out vec4 frag;
void main() {
    float r = length(vQ);
    float a;
    if (vCol.a < 0.95) {
        float w = fwidth(r);
        a = (1.0 - smoothstep(0.32 - w, 0.32 + w, r)) * 0.55;
    } else {
        float halo = exp(-r * r * 6.0) * (1.0 - smoothstep(0.75, 1.0, r));
        a = halo * (0.3 + vCol.a);
    }
    frag = vec4(vCol.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well: flat quads (panes, the tiles, the ghost)
inline const char* FLAT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
layout(location = 2) in vec4 aQ;     // local coords (-1..1), softness x, y
uniform mat4 uVP;
out vec4 vCol;
out vec4 vQ;
void main() {
    vCol = aCol;
    vQ = aQ;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* FLAT_FS = R"(#version 330 core
in vec4 vCol;
in vec4 vQ;
out vec4 frag;
void main() {
    vec2 q = abs(vQ.xy);
    float a = 1.0;
    if (vQ.z > 0.0) a *= smoothstep(1.0, 1.0 - vQ.z, q.x);
    if (vQ.w > 0.0) a *= smoothstep(1.0, 1.0 - vQ.w, q.y);
    if (vQ.z < 0.0) {
        // rounded corners (-z, -w: the corner's radius over the half width, the half height)
        vec2 rr = -vQ.zw;
        float d = length(max(q - (1.0 - rr), 0.0) / rr);
        float aa = max(fwidth(d), 1e-4);
        a *= 1.0 - smoothstep(1.0 - aa, 1.0, d);
    }
    frag = vec4(vCol.rgb, vCol.a * a);
}
)";

// ---------------------------------------------------------------- the frame
// The colours as authored (no curve, no grade, no vignette); the slow motion's glassy blue, the bridge, the dimming.
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uSlow;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
void main() {
    vec4 s = texture(uScene, vUv);
    float pk = clamp(s.a, 0.0, 1.0);
    vec3 c = clamp(s.rgb, 0.0, 1.0);
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c * uSlowTint, 0.65), uSlow * (1.0 - pk));
    c = mix(c, uBridgeCol, uBridge * (1.0 - pk));
    c = mix(c, uBridgeInk, uBridge * pk);
    c *= (1.0 - 0.5 * uDim) * uFade;
    frag = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
)";

// ---------------------------------------------------------------- HUD
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

// A shader with the given blocks inserted after its version line.
inline std::string with(const char* fs, const char* a, const char* b = "", const char* c = "") {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + a + b + c + s.substr(nl + 1);
}

} // namespace islands_shaders

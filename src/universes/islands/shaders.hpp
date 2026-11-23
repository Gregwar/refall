#pragma once
// ISLANDS' shaders (see islands.cpp).
//
// The world is drawn relative to the eye (the camera at the origin, so every float stays small) and bent by the
// sea's curvature (each vertex dropped by curv * d^2 / 2, d its horizontal distance; the curvature grows with the
// altitude, so the horizon curves only from high up). All of it flattened low poly: flat facets, flat colors.
// Passes: the sun's shadow map; the seabed (the terrain's distance, at half resolution) for the sea's depth; the main
// pass (sky, terrain, objects, then the sea as one fullscreen pass: the curved surface hit per pixel, its flat bands
// of color by the depth, white glints, the foam line at the shores, the boats' wakes), the clouds (faceted puffs),
// additive glows; then the well and its low poly cubes through the layout's camera, a filmic tone map, the HUD.
#include <string>

namespace islands_shaders {

// ---------------------------------------------------------------- common: noise, the sky, the haze, the shadows
inline const char* COMMON = R"(
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uSkyLow, uFogCol, uAmb, uBounce;
uniform vec3 uEye;          // the camera's position (world)
uniform float uCurv, uTime, uNight, uHaze, uHazeH, uDip;
uniform vec4 uClouds[24];   // the clouds' shadows: x z (where the shadow falls, world), radius, opacity
uniform int uNumClouds;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float n2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * n2(p); p = p * 2.03 + vec2(17.1, 3.7); a *= 0.5; }
    return s / 0.9375;
}
// The sky along a direction (no sun disc): its gradient by the hour (the horizon dips as the camera climbs).
vec3 skyCol(vec3 rd) {
    float e = rd.y + uDip;
    vec3 c = mix(uSkyHor, uSkyTop, pow(smoothstep(0.0, 0.45, e), 0.6));
    c = mix(c, uSkyLow, smoothstep(0.0, -0.08, e));
    float sd = max(dot(rd, uSunDir), 0.0);
    float low = 1.0 - smoothstep(0.02, 0.45, uSunDir.y);
    c += uSunCol * (pow(sd, 5.0) * (0.06 + 0.3 * low) + pow(sd, 48.0) * (0.25 + 0.4 * low)) * smoothstep(-0.2, 0.05, e);
    return c;
}
// Aerial perspective: an exponential haze thinning with height, warmer toward the sun.
float hazeK(float dist, vec3 rd) {
    float b = 1.0 / uHazeH;
    float dy = rd.y;
    float base = uHaze * exp(-max(uEye.y, 0.0) * b);
    float f = abs(dy) > 1e-4 ? base * (1.0 - exp(-dist * dy * b)) / (dy * b) : base * dist;
    return 1.0 - exp(-max(f, 0.0));
}
vec3 hazeCol(vec3 rd) {
    float sd = max(dot(rd, uSunDir), 0.0);
    return mix(uFogCol, uFogCol + uSunCol * 0.18, pow(sd, 6.0));
}
vec3 fogged(vec3 c, float dist, vec3 rd) { return mix(c, hazeCol(rd), hazeK(dist, rd)); }
float cloudShadow(vec2 p) {
    float s = 1.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uNumClouds) break;
        vec2 d = p - uClouds[i].xy;
        float r = uClouds[i].z;
        float k = 1.0 - smoothstep(r * 0.35, r, length(d) * (0.9 + 0.2 * n2(p * 0.004 + float(i))));
        s *= 1.0 - k * uClouds[i].w;
    }
    return s;
}
)";

// ---------------------------------------------------------------- TRIP: the psychedelic variant's flat neon look
// The world drawn flat (one color per material, two flat tones by the facet's side, no haze), the sea and the greens
// turned into marbled bands of neon: a domain-warped noise in world space (anchored to the sea, its size from the
// altitude) flowing slowly, its iso-bands mapped through a neon palette, crisp but antialiased (and blended to the
// palette's mean where they get finer than a pixel). The colors are display colors (sRGB), made linear here.
inline const char* TRIP_GLSL = R"(
uniform float uTrip;        // 1: the TRIP look
uniform float uFlow;        // its clock: the swirls' flow (faster with the music's energy)
uniform float uPulse;       // a soft swell on the beat (smooth: no flicker)
uniform float uTripScale;   // the swirls' size (world meters per noise unit)
vec3 tl(vec3 c) { return pow(c, vec3(2.2)); }
float fbm3(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * n2(p); p = p * 2.07 + vec2(11.3, 7.9); a *= 0.5; }
    return s / 0.875;
}
// the marbled field: the noise seen through two flowing warps (r: the second warp, for the bands' drift)
float marble(vec2 p, float t, out vec2 r) {
    vec2 q = vec2(fbm3(p + vec2(0.0, 0.11 * t)), fbm3(p + vec2(5.2, 1.3) - vec2(0.07 * t, 0.0)));
    r = vec2(fbm3(p + 3.2 * q + vec2(1.7, 9.2) + 0.09 * t), fbm3(p + 3.2 * q + vec2(8.3, 2.8) - 0.06 * t));
    return fbm3(p + 3.6 * r);
}
// a step from a to b at x = e, antialiased over w
vec3 tstep(vec3 a, vec3 b, float x, float e, float w) { return mix(a, b, smoothstep(e - w, e + w, x)); }
// the sea's bands along v (period 1): acid green, a lime line, turquoise, a violet line, orange, a magenta line, back
// to green; w the band coordinate's footprint over a pixel
vec3 seaBands(float v, float w) {
    float x = fract(v);
    float aa = max(w, 1e-4);
    vec3 green = tl(vec3(0.53, 0.89, 0.18)), lime = tl(vec3(0.85, 0.95, 0.25)), turq = tl(vec3(0.13, 0.83, 0.70));
    vec3 violet = tl(vec3(0.45, 0.25, 0.85)), orange = tl(vec3(1.0, 0.53, 0.18)), magenta = tl(vec3(0.93, 0.32, 0.80));
    vec3 c = green;
    c = tstep(c, lime, x, 0.24, aa);
    c = tstep(c, turq, x, 0.275, aa);
    c = tstep(c, violet, x, 0.52, aa);
    c = tstep(c, orange, x, 0.55, aa);
    c = tstep(c, magenta, x, 0.84, aa);
    c = tstep(c, green, x, 0.865, aa);
    // finer than a pixel: the palette's mean
    vec3 mean = green * 0.35 + turq * 0.27 + orange * 0.29 + (lime + violet + magenta) * 0.03;
    return mix(c, mean, smoothstep(0.12, 0.4, w));
}
// the greens' bands: acid green, teal, a yellow line
vec3 landBands(float v, float w) {
    float x = fract(v);
    float aa = max(w, 1e-4);
    vec3 green = tl(vec3(0.49, 0.90, 0.18)), teal = tl(vec3(0.15, 0.78, 0.55)), yellow = tl(vec3(0.98, 0.86, 0.22));
    vec3 c = green;
    c = tstep(c, teal, x, 0.45, aa);
    c = tstep(c, yellow, x, 0.86, aa);
    c = tstep(c, green, x, 0.92, aa);
    vec3 mean = green * 0.5 + teal * 0.42 + yellow * 0.08;
    return mix(c, mean, smoothstep(0.12, 0.4, w));
}
// an object's color pushed to neon: saturated, the whites to pale pastels (by its seed)
vec3 tripNeon(vec3 a, float seed) {
    vec3 s = sqrt(max(a, 0.0));
    float l = dot(s, vec3(0.3, 0.55, 0.15));
    float mx = max(s.r, max(s.g, s.b)), mn = min(s.r, min(s.g, s.b));
    vec3 c = clamp(mix(vec3(l), s, 1.9), 0.0, 1.0);
    float h = fract(seed * 7.31);
    vec3 pastel = h < 0.33 ? vec3(1.0, 0.75, 0.95) : h < 0.66 ? vec3(1.0, 0.93, 0.55) : vec3(0.65, 0.95, 1.0);
    c = mix(c, pastel, smoothstep(0.7, 0.85, l) * (1.0 - smoothstep(0.08, 0.25, mx - mn)));
    return tl(c);
}
)";

inline const char* SHADOW_GLSL = R"(
uniform mat4 uLightVP;
uniform sampler2DShadow uShadow;
uniform float uSoft, uShadowOn;
float shadowAt(vec3 rel, float ndl) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 l = uLightVP * vec4(rel, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    float edge = smoothstep(0.0, 0.06, min(min(s.x, s.y), min(1.0 - s.x, 1.0 - s.y)));
    float a = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831;
    float sum = 0.0;
    float bias = 0.0004 + 0.0012 * (1.0 - ndl);
    for (int i = 0; i < 8; i++) {
        float fi = float(i) + 0.5;
        float rr = sqrt(fi / 8.0) * uSoft;
        float an = a + fi * 2.39996;
        sum += texture(uShadow, vec3(s.xy + vec2(cos(an), sin(an)) * rr, s.z - bias));
    }
    return mix(1.0, sum / 8.0, edge);
}
)";

// ---------------------------------------------------------------- the sky (fullscreen, behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uStars;
uniform vec3 uTripSun;      // TRIP: the ringed sun's direction
// TRIP: a deep violet sky, a huge ringed sun (yellow, orange, lime, magenta rings, wavy, flowing outward), spiral arms
// curling out of it
vec3 tripSky(vec3 rd) {
    float e = rd.y + uDip;
    vec3 c = mix(tl(vec3(0.42, 0.17, 0.80)), tl(vec3(0.20, 0.07, 0.45)), smoothstep(0.0, 0.4, e));
    vec3 S = normalize(uTripSun);
    float r = acos(clamp(dot(rd, S), -1.0, 1.0));
    vec3 ax = normalize(cross(S, vec3(0.0, 1.0, 0.0))), ay = cross(ax, S);
    float th = atan(dot(rd, ay), dot(rd, ax));
    const float R = 0.24;
    float u = r / R;
    float aa = fwidth(u) * 1.2;
    // the rings (wavy, drifting out): yellow, orange, lime, orange, magenta
    float k = u * 2.4 - 0.05 * uFlow + 0.1 * u * sin(th * 3.0 + u * 4.0 - 0.15 * uFlow) - 0.04 * uPulse;
    float x = fract(k), aw = max(fwidth(k), 1e-4);
    vec3 ring = tl(vec3(1.0, 0.86, 0.2));
    ring = tstep(ring, tl(vec3(1.0, 0.5, 0.15)), x, 0.22, aw);
    ring = tstep(ring, tl(vec3(0.72, 0.93, 0.2)), x, 0.45, aw);
    ring = tstep(ring, tl(vec3(1.0, 0.42, 0.2)), x, 0.62, aw);
    ring = tstep(ring, tl(vec3(0.92, 0.25, 0.6)), x, 0.84, aw);
    ring = tstep(ring, tl(vec3(1.0, 0.86, 0.2)), x, 0.97, aw);
    vec3 core = tl(vec3(1.0, 0.9, 0.3));
    vec3 sun = mix(core, ring, smoothstep(0.26 - aa, 0.26 + aa, u));
    c = mix(sun, c, smoothstep(1.0 - aa, 1.0 + aa, u));
    // the arms: three spirals curling out to about three times its radius, lime and orange, thinning out
    float sp = th / 6.2831853 * 3.0 + log(max(u, 1e-3)) * 1.1 - 0.012 * uFlow;
    float f = fract(sp), fw = max(fwidth(sp), 1e-4);
    float band = smoothstep(0.0, fw, f) * (1.0 - smoothstep(0.07 - fw, 0.07, f));
    float reach = smoothstep(0.98, 1.05, u) * (1.0 - smoothstep(1.4, 2.3, u));
    vec3 arm = mix(tl(vec3(0.62, 0.95, 0.2)), tl(vec3(1.0, 0.5, 0.18)), step(0.5, fract(floor(sp) * 0.5)));
    c = mix(c, arm, band * reach);
    // a violet halo ring hugging it
    c = mix(c, tl(vec3(0.55, 0.2, 0.9)), (smoothstep(1.0, 1.04, u) - smoothstep(1.1, 1.2, u)) * 0.8);
    return c;
}
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize(w.xyz / w.w);
    if (uTrip > 0.5) { frag = vec4(tripSky(rd), 0.0); return; }
    vec3 c = skyCol(rd);
    float e = rd.y + uDip;
    // the sun's disc (soft edged), under the horizon it sinks into the haze
    float sd = dot(rd, uSunDir);
    float disc = smoothstep(0.99955, 0.99975, sd) * smoothstep(-0.01, 0.01, e);
    c += uSunCol * disc * 6.0;
    // stars at nightfall
    if (uStars > 0.01 && e > 0.0) {
        vec3 sp = rd * 260.0;
        vec3 si = floor(sp);
        float hs = fract(sin(dot(si, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        vec3 fp = fract(sp) - 0.5;
        float st = smoothstep(0.12, 0.0, length(fp)) * step(0.985, hs);
        c += vec3(0.9, 0.92, 1.0) * st * uStars * smoothstep(0.03, 0.3, e) * (0.6 + 0.4 * sin(uTime * 2.0 + hs * 40.0));
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the terrain (one mesh per island)
inline const char* TERRAIN_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;
layout(location = 3) in float aAo;
uniform mat4 uVP;
uniform vec3 uOff;      // the island's center, relative to the eye
uniform float uCurv;
out vec3 vRel, vLocal;
flat out vec3 vFace;    // the facet's own vertex (its provoking one): its materials, its tone
flat out vec4 vAttr;
flat out float vAo;
void main() {
    vec3 rel = aPos + uOff;
    rel.y -= 0.5 * uCurv * dot(rel.xz, rel.xz);
    vLocal = aPos;
    vRel = rel;
    vFace = aPos;
    vAttr = aAttr;
    vAo = aAo;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

// Low poly: each triangle of the (jittered) grid one flat facet, its normal from the screen derivatives, one
// material and one flat color (the island's palette, a small tone of its own), no texture.
inline const char* TERRAIN_FS = R"(#version 330 core
in vec3 vRel, vLocal;
flat in vec3 vFace;
flat in vec4 vAttr;
flat in float vAo;
layout(location = 0) out vec4 frag;
uniform int uMode;        // 0 main, 1 the seabed (its distance only, for the sea's bands), 3 the shadow map
uniform vec3 uSand, uSandWet, uGrass, uJungle, uRock, uCliff, uSeabed, uReefA, uReefB, uDry;
uniform float uSeed, uIsleR, uFallA, uFallW, uDryK;
uniform vec3 uFoam;
void main() {
    if (uMode == 3) { frag = vec4(0.0); return; }
    float dist = length(vRel);
    float h = vFace.y;
    if (uMode == 1) {
        // (the land above the water: its distance negative)
        frag = vec4(0.0, 0.0, 0.0, vLocal.y > 0.15 ? -dist : dist);
        return;
    }
    vec3 N = normalize(cross(dFdx(vRel), dFdy(vRel)));
    if (dot(N, vRel) > 0.0) N = -N;
    vec2 fp = vFace.xz + uSeed * 37.0;
    float tone = h21(fp * 0.137);
    float sandK = vAttr.x, vegK = vAttr.y, rockK = vAttr.z;
    vec3 col;
    if (h < 0.0) col = uSeabed;
    else if (rockK > 0.45) {
        // the cliffs: flat dark brown walls, the gentler rock lighter
        col = mix(uRock, uCliff, smoothstep(0.3, 0.7, 1.0 - N.y)) * (0.94 + 0.12 * tone);
    } else if (vegK > 0.5 && sandK < 0.75) {
        // the greens: two tones, the darker higher up, or the dry maquis
        float k = step(0.8, tone * 0.5 + smoothstep(6.0, 60.0, h));
        vec3 veg = mix(uGrass, uJungle, k * 0.7);
        veg = mix(veg, uDry, uDryK * step(0.55, h21(fp * 0.031 + 2.0)));
        col = veg * (0.95 + 0.1 * tone);
    } else col = mix(uSandWet, uSand, step(0.5, h)) * (0.95 + 0.1 * tone);
    // a waterfall down the cliff: flat white water, bands streaming down
    if (uFallW > 0.0 && rockK > 0.25) {
        float a = atan(vLocal.z, vLocal.x);
        float da = mod(a - uFallA + 3.14159265, 6.2831853) - 3.14159265;
        if (abs(da / uFallW) < 0.8) col = mix(uFoam, uFoam * vec3(0.72, 0.86, 0.95), step(0.6, fract(vLocal.y * 0.08 + uTime * 0.9)));
    }
    if (uTrip > 0.5) {
        // TRIP: flat, two tones by the facet's side; the greens marbled, flowing
        // (the field and its footprint out of the material's branch: derivatives need every pixel of the quad)
        vec2 r;
        vec2 wp = vRel.xz + uEye.xz;
        float m = marble(wp / (uTripScale * 0.7) + 31.0, uFlow * 0.8, r);
        float v = m * 2.6 + r.y * 0.6 + 0.03 * uFlow;
        float vw = fwidth(v);
        if (h >= 0.0 && rockK <= 0.45 && vegK > 0.5 && sandK < 0.75) col = landBands(v, vw);
        float side = dot(N, uSunDir) > 0.25 ? 1.0 : 0.8;
        frag = vec4(col * side, 0.0);
        return;
    }
    // the light: per facet, the sun (crisp shadows), the sky ambient, the sea's bounce
    float ndl = max(dot(N, uSunDir), 0.0);
    float sh = shadowAt(vRel, ndl);
    vec3 amb = uAmb * (0.6 + 0.4 * N.y) * (0.7 + 0.3 * vAo) + uBounce * (0.5 - 0.5 * N.y);
    vec3 lit = col * (uSunCol * ndl * sh + amb);
    vec3 rd = vRel / max(dist, 1e-3);
    frag = vec4(fogged(lit, dist, rd), 0.0);
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
flat out vec4 vQuat;
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
    vQuat = iD;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

inline const char* OBJ_FS = R"(#version 330 core
in vec3 vRel, vObj, vNrm;
in vec2 vAux;
flat in vec3 vCol, vSize;
flat in float vMat, vParam, vSeed;
flat in vec4 vQuat;
layout(location = 0) out vec4 frag;
uniform int uMode;          // 0 main, 2 the reflection, 3 the shadow map (depth only)
uniform vec3 uLeaf, uLeaf2, uTrunk, uFoliage, uFoliage2, uWater, uWaterGlow, uRoomNight;
uniform vec3 uSailPal[7];   // the fleet's sail colors (the pieces')
uniform float uLights;      // the lights at dusk (windows, the pool, the lighthouse)
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float hb(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float nb(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hb(i), hb(i + vec2(1, 0)), f.x), mix(hb(i + vec2(0, 1)), hb(i + vec2(1, 1)), f.x), f.y);
}
float h3(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
float n3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
    vec3 world = vRel + uEye;
    if (uMode == 3) { frag = vec4(0.0); return; }
    int mat = int(vMat + 0.5);
    // low poly: each triangle one flat facet (its normal from the screen derivatives, toward the eye), flat colors
    vec3 N = normalize(cross(dFdx(vRel), dFdy(vRel)));
    if (dot(N, vRel) > 0.0) N = -N;
    float dist = length(vRel);
    vec3 V = -vRel / max(dist, 1e-3);
    vec3 alb = vCol;
    float spec = 0.0, emit = 0.0, trans = 0.0;
    vec3 emitCol = vec3(0.0);
    if (mat == 1) {             // glass: a flat dark blue pane, the room's light at dusk
        alb = mix(vec3(0.06, 0.12, 0.2), skyCol(reflect(-V, N)) * 0.6, 0.5);
        float on = uLights * step(0.5, vParam);
        emitCol = uRoomNight * on * (vParam > 1.5 ? 4.0 : 1.0);
        emit = 1.0;
    } else if (mat == 2) {      // deck: flat planks, every other one a shade darker
        vec2 ext = vSize.x > vSize.z ? vec2(vObj.x, vObj.z) : vec2(vObj.z, vObj.x);
        alb *= 0.9 + 0.1 * step(0.5, fract(ext.y / 0.32)) * step(0.5, N.y);
    } else if (mat == 4) {      // thatch: flat rings down the slope
        alb *= 0.86 + 0.14 * step(0.5, fract(vObj.y * 2.2));
    } else if (mat == 5) {      // the pool: flat turquoise, a glow at night
        alb = uWater;
        emitCol = uWaterGlow * uLights;
        emit = 1.0;
    } else if (mat == 7) {      // foliage: a faceted blob, one green per tree (param: 0 jungle, 1 olive, 2 cypress, 3 hedge)
        vec3 base = vParam < 0.5 ? mix(uFoliage, uFoliage2, step(0.5, fract(vSeed * 3.7))) : vParam > 2.5 ? uFoliage * 1.1 : vCol;
        alb = base * (0.92 + 0.16 * fract(vSeed * 13.1));
        trans = 0.2;
    } else if (mat == 8) {      // the palm: a flat trunk, fronds green to yellow toward their tips
        if (vAux.x < 0.5) alb = uTrunk;
        else if (vAux.x < 1.5) {
            alb = mix(uLeaf, uLeaf2, step(0.7, vAux.y) * 0.6 + 0.2 * step(0.5, fract(vSeed * 5.3)));
            trans = 0.4;
        } else alb = vec3(0.32, 0.24, 0.1);
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
        trans = 0.5;
    } else if (mat == 10) {     // a hull: its paint, a stripe along the sheer, the deck
        if (vAux.x > 0.5) alb = vec3(0.62, 0.45, 0.28);
        else {
            float y = vObj.y / max(vSize.y, 1e-3);
            vec3 stripe = vParam > 2.5 ? vec3(0.6, 0.08, 0.08) : vParam > 1.5 ? vec3(0.04, 0.32, 0.4) : vec3(0.04, 0.08, 0.2);
            alb = mix(alb, stripe, step(-0.165, y) * step(y, -0.065) * step(0.5, vParam));
        }
    } else if (mat == 11) {     // canvas: an umbrella's stripes
        float a = atan(vObj.z, vObj.x);
        alb = mix(vCol, vec3(0.97, 0.95, 0.9), step(0.5, fract(a / 6.2831853 * 8.0)));
    } else if (mat == 12) {     // rock: a faceted boulder
        alb = vec3(0.46, 0.4, 0.34) * (0.85 + 0.25 * fract(vSeed * 7.7));
    } else if (mat == 13) {     // a lamp
        emitCol = vCol * 4.0;
        emit = 1.0;
    } else if (mat == 14) {     // metal (masts, rails)
        spec = 0.3;
    } else if (mat == 15) {     // the lighthouse: white with red bands
        float y = vObj.y / vSize.y + 0.5;
        alb = mix(vec3(0.95), vec3(0.75, 0.1, 0.08), step(0.5, fract(y * 3.0 + 0.25)) * step(0.15, y));
    } else if (mat == 16) {     // a seabird: white, a grey back, dark tips
        alb = vParam < 0.5 && vCol.r < 0.3 ? vCol : mix(vec3(0.95), vec3(0.55, 0.58, 0.62), step(0.5, vAux.x) * step(0.4, vAux.y));
        alb = mix(alb, vec3(0.08), step(0.5, vAux.x) * step(0.9, vAux.y));
    } else if (mat == 17) {     // roof tiles: flat rows
        alb *= 0.88 + 0.12 * step(0.5, fract(vObj.y * 4.0 / max(vSize.y, 0.1)));
    }
    // TRIP: the trees take the greens' marbled bands where they stand (they melt into the swirl, their shapes kept by
    // the two tones), the field out of the material's branch (its derivatives)
    vec3 treeCol = vec3(0.0);
    if (uTrip > 0.5) {
        vec2 r;
        float m = marble(world.xz / (uTripScale * 0.7) + 31.0, uFlow * 0.8, r);
        float v = m * 2.6 + r.y * 0.6 + 0.03 * uFlow;
        treeCol = landBands(v + 0.18, fwidth(v));
    }
    if (uTrip > 0.5) {
        // TRIP: flat neon, two tones by the facet's side; the palms in zebra stripes of mixed neons (trunk and fronds
        // each a pair, by the palm's seed), the foliage in neon greens
        vec3 a = tripNeon(alb, vSeed);
        if (mat == 8) {
            float h = fract(vSeed * 3.17 + (vAux.x < 0.5 ? 0.0 : 0.37));
            vec3 c0 = h < 0.33 ? vec3(0.5, 0.18, 0.95) : h < 0.66 ? vec3(1.0, 0.85, 0.2) : vec3(0.15, 0.9, 0.95);
            vec3 c1 = h < 0.33 ? vec3(0.95, 0.3, 0.85) : h < 0.66 ? vec3(0.55, 0.2, 0.9) : vec3(0.2, 0.45, 0.95);
            // stripes across the trunk, wavy across the fronds
            float s = vAux.x < 0.5 ? vObj.y / max(vSize.y, 0.01) * 0.8 + sin(vObj.x * 4.0) * 0.3
                                   : vAux.y * 7.0 + sin(vObj.x * 2.3 + vObj.z * 1.7) * 0.6;
            a = tl(mix(c0, c1, step(0.5, fract(s))));
        } else if (mat == 7) a = treeCol;
        float side = dot(N, uSunDir) > 0.2 ? 1.0 : 0.8;
        frag = vec4(a * side + emitCol * emit * 0.5, 0.0);
        return;
    }
    float ndl = dot(N, uSunDir);
    float sh = shadowAt(vRel + N * 0.05, max(ndl, 0.0));
    float diff = max(ndl, 0.0) + trans * max(-ndl, 0.0) * 0.6;
    vec3 amb = uAmb * (0.65 + 0.35 * N.y) + uBounce * (0.5 - 0.5 * N.y);
    vec3 col = alb * (uSunCol * diff * sh + amb);
    if (spec > 0.0) col += uSunCol * pow(max(dot(N, normalize(uSunDir + V)), 0.0), 40.0) * spec * sh;
    col += emitCol * emit;
    frag = vec4(fogged(col, dist, -V), 0.0);
}
)";

// ---------------------------------------------------------------- the sea (fullscreen: the curved surface per pixel)
// Flattened: flat bands of color by the depth under the surface (the foam line at the shore, the turquoise
// shallows, a teal ledge, the deep blue), the open sea a mosaic of big flat triangles a shade apart, sparse white
// angular glints drifting, the boats' wakes as crisp white shapes.
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uVP;
uniform sampler2D uSeabed;
uniform vec3 uShallow, uDeep, uFoam;
uniform float uWave, uSparkle, uCalm;
uniform vec2 uInvRes;
uniform vec4 uBoats[24];     // x z, heading, speed
uniform vec4 uBoatsB[24];    // length, beam, kind, -
uniform int uNumBoats;
uniform vec4 uFalls[4];      // x z, radius, -
uniform int uNumFalls;
// a crisp edge (antialiased over a pixel) of a value against its threshold
// (over a pixel and a half: the depth comes from the half resolution seabed, its steps hidden, the edge steady)
float edge(float v, float th) { float w = clamp(fwidth(v) * 1.5, 1e-5, th * 0.4); return smoothstep(th - w, th + w, v); }
// the glints of one scale: in each cell (of size S, world) now and then a white sliver, or two in a V, flattened on
// the water, drifting with the swell, growing and shrinking over its life; fades out once it shrinks under a pixel
float glints(vec2 p, float S, float foot, float t) {
    if (foot > S * 0.035) return 0.0;
    p += vec2(0.6, 0.25) * t * S * 0.05;
    vec2 id = floor(p / S);
    float h = h21(id + S);
    if (h > 0.3 + 0.12 * uWave) return 0.0;
    float life = fract(t * (0.05 + 0.04 * h21(id + 3.1)) + h * 7.0);
    float k = sin(life * 3.14159) * (1.0 - smoothstep(S * 0.015, S * 0.035, foot));
    vec2 c = (id + 0.25 + 0.5 * vec2(h21(id + 1.7), h21(id + 5.3))) * S;
    float an = 0.35 + 0.5 * (h21(id + 9.1) - 0.5);
    vec2 q = p - c;
    q = vec2(cos(an) * q.x + sin(an) * q.y, -sin(an) * q.x + cos(an) * q.y) / (S * 0.26 * k + 1e-4);
    // a sliver: long along x, its width tapering from the head (-0.5) to a point (0.5)
    float g = step(abs(q.x), 0.5) * step(abs(q.y), 0.16 * (0.5 - q.x));
    // and a shorter one meeting its head, in a V
    vec2 q2 = vec2(0.86 * q.x + 0.5 * q.y + 0.38, -0.5 * q.x + 0.86 * q.y + 0.2) * 1.7;
    g = max(g, step(h21(id + 2.2), 0.6) * step(abs(q2.x), 0.5) * step(abs(q2.y), 0.16 * (0.5 - q2.x)));
    return g;
}
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
    // the depth under the surface (from the seabed's distance along the ray)
    // (filtered by hand: no terrain there is the deep, not a distance of 0; the land, its distance)
    vec2 hp = gl_FragCoord.xy * 0.5 - 0.5;
    ivec2 h0 = ivec2(floor(hp));
    vec2 hf = hp - vec2(h0);
    ivec2 hm = textureSize(uSeabed, 0) - 1;
    vec4 d4;
    for (int i = 0; i < 4; i++) {
        float a = texelFetch(uSeabed, clamp(h0 + ivec2(i % 2, i / 2), ivec2(0), hm), 0).a;
        d4[i] = a == 0.0 ? t + 60.0 / max(-dy, 0.02) : abs(a);  // (no terrain: as deep as the deepest band)
    }
    float sbd = mix(mix(d4.x, d4.y, hf.x), mix(d4.z, d4.w, hf.x), hf.y);
    float thick = sbd > t ? sbd - t : 1e4;
    float depthV = min(thick * (-dy), 60.0);
    // the bands: the deep blue (darker near, lighter toward the horizon), a teal ledge, the turquoise shallows
    // (their depths clear of a lagoon's floor, which would break into patches)
    float far = smoothstep(alt * 1.5 + 40.0, alt * 14.0 + 2500.0, t);
    vec3 col = mix(uDeep * 0.82, mix(uDeep, uShallow, 0.3), far);
    vec3 ledge = mix(uShallow, uDeep, 0.5);
    col = mix(col, ledge, 1.0 - edge(depthV, 16.0));
    col = mix(col, uShallow, 1.0 - edge(depthV, 7.5));
    // lit by the sky (flat: no glint, no reflection)
    float light = dot(uSunCol, vec3(0.3, 0.5, 0.2)) * 0.5 + dot(uAmb, vec3(0.3, 0.5, 0.2)) * 0.8;
    col *= light;
    col = mix(col, uSkyHor * light * 0.9, 0.12);
    if (uTrip > 0.5) {
        // TRIP: the sea all marbled neon bands, flowing, ringing the islands near their shores (the depth bends the
        // bands round them), an orange ring at the waterline
        vec2 r;
        // (the bands a little wider far away: calmer toward the horizon, like long strokes)
        // (marbling: parallel bands, combed by the warps; so there are bands everywhere, none of the noise's flat
        // patches)
        vec2 p = wp / (uTripScale * 0.5 * (1.0 + t / (alt * 7.0 + 100.0)));
        float m = marble(p, uFlow, r);
        float v = dot(p, vec2(0.6, 0.8)) * 2.2 + m * 4.0 + r.x * 1.2 + 1.1 * exp(-depthV / 7.0) + 0.035 * uFlow + 0.05 * uPulse;
        col = seaBands(v, fwidth(v));
        col = mix(col, tl(vec3(1.0, 0.53, 0.18)), 1.0 - edge(depthV, 1.6));
    }
    // white: the foam line at the shore (its width breathing slowly), the boats' wakes, the falls' feet
    // (TRIP: none; the orange ring is its shore)
    float foam = (1.0 - edge(depthV, 0.55 + 0.15 * sin(uTime * 0.9 + wp.x * 0.02 + wp.y * 0.015))) * mid * (1.0 - uTrip);
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
            float trail = step(abs(across), tw) * step(-L * 0.5, along) * step(foot * 0.5, B * 0.4);
            float armL = min(wl * 0.25, 5.0 + L * 1.5) * sk;
            float aw = 0.12 + B * 0.12;
            float arm = abs(abs(across) - al * 0.36 - B * 0.4);
            float v = step(arm, aw * (1.0 - al / max(armL, 1e-3))) * step(0.0, along) * step(foot * 0.7, aw) * step(25.0, alt);
            foam = max(foam, max(trail, v * step(0.15, sk)));
        }
    }
    for (int i = 0; i < 4; i++) {
        if (i >= uNumFalls) break;
        foam = max(foam, step(length(wp - uFalls[i].xy), uFalls[i].z * 0.6));
    }
    // the glints over the open water, two scales (the bigger one only from a little height: skimming the waves, it filled the view)
    float gl = max(glints(wp, 7.0, foot, uTime), glints(wp, 26.0, foot, uTime * 0.8) * step(15.0, alt)) * edge(depthV, 1.5) * (1.0 - uCalm) * (1.0 - uTrip);
    gl *= step(alt * 4.0 + 6.0, t);  // (none right under a low camera, where they would fill the view)
    foam = max(foam, gl);
    if (uTrip > 0.5) {
        // (the foam line, the wakes, the falls' feet: a pale yellow; no glints, no haze)
        col = mix(col, tl(vec3(1.0, 0.96, 0.62)), clamp(foam, 0.0, 1.0));
        frag = vec4(col, 0.0);
        return;
    }
    vec3 foamLit = uFoam * (uSunCol * (0.55 * max(uSunDir.y, 0.1) + 0.3) + uAmb * 0.9);
    col = mix(col, foamLit, clamp(foam, 0.0, 1.0));
    frag = vec4(fogged(col, t, rd), 0.0);
}
)";

// ---------------------------------------------------------------- clouds: soft lit puffs (instanced sprites)
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz (relative to the eye), radius
layout(location = 2) in vec4 iP;     // seed, alpha, flat bottom (y, relative), shade
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec3 vRel;
flat out vec4 vP;
flat out float vR, vCy;
void main() {
    vQ = aCorner;
    vP = iP;
    vR = iPos.w;
    vCy = iPos.y;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    vRel = w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* CLOUD_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vRel;
flat in vec4 vP;
flat in float vR, vCy;
out vec4 frag;
uniform vec3 uRight, uUp, uFwd;
uniform vec3 uCloudLit, uCloudShade;
// Low poly: each puff a faceted ball seen from the camera: a heptagon outline, a ring of triangles (pointing out,
// pointing in, alternately) round an inner heptagon fanned from its center; each facet flat shaded by its own
// normal; the cluster's bottom cut flat.
void main() {
    const float NS = 7.0, S = 6.2831853 / NS, RI = 0.58;
    float r = length(vQ);
    float ang = atan(vQ.y, vQ.x) + vP.x * 6.2831853;
    ang = mod(ang, 6.2831853);
    float k = floor(ang / S), u = ang - k * S;
    // the outline: the heptagon through the outer vertices (at k S)
    float ro = cos(S * 0.5) / cos(u - S * 0.5);
    float aa = fwidth(r) * 1.2;
    float a = 1.0 - smoothstep(ro * 0.96 - aa, ro * 0.96, r);
    float wy = vRel.y + uEye.y;
    a *= smoothstep(vP.z - vR * 0.02, vP.z + vR * 0.02, wy);
    if (a < 0.003) discard;
    // which facet: the point in the frame of its sector (the outer vertices at 0 and S, radius 1; the inner one at
    // S/2, radius RI)
    vec2 p = r * vec2(cos(u), sin(u));
    vec2 O0 = vec2(1.0, 0.0), O1 = vec2(cos(S), sin(S)), I0 = RI * vec2(cos(S * 0.5), sin(S * 0.5));
    float fa;     // the facet's direction (sector angle)
    float tilt;   // how far its normal leans out
    float ri = RI * cos(S * 0.5) / cos(mod(u + S * 0.5, S) - S * 0.5);
    if (r < ri) {
        // the inner heptagon, fanned from the center (its vertices at (k + 1/2) S)
        fa = (u < S * 0.5 ? k : k + 1.0) * S;
        tilt = 0.32;
    } else {
        vec2 e0 = I0 - O0, e1 = O1 - I0;
        bool outTri = (e0.x * (p.y - O0.y) - e0.y * (p.x - O0.x)) <= 0.0 && (e1.x * (p.y - I0.y) - e1.y * (p.x - I0.x)) <= 0.0;
        if (outTri) { fa = (k + 0.5) * S; tilt = 0.88; }
        else { fa = (u < S * 0.5 ? k : k + 1.0) * S; tilt = 0.72; }
    }
    fa -= vP.x * 6.2831853;
    vec3 N = normalize((uRight * cos(fa) + uUp * sin(fa)) * tilt - uFwd * (1.0 - tilt * 0.6));
    float ndl = dot(N, uSunDir) * 0.5 + 0.5;
    vec3 col = mix(uCloudShade, uCloudLit, 0.25 + 0.75 * ndl);
    col *= mix(0.9, 1.0, vP.w);
    vec3 rd = normalize(vRel);
    float dist = length(vRel);
    col = mix(col, hazeCol(rd), hazeK(dist, rd) * 0.9);
    // TRIP: flat puffs, yellow on the lit facets, orange, a red orange in the shade
    if (uTrip > 0.5) col = ndl > 0.72 ? tl(vec3(1.0, 0.84, 0.25)) : ndl > 0.5 ? tl(vec3(1.0, 0.62, 0.2)) : tl(vec3(1.0, 0.45, 0.25));
    a *= vP.y;
    frag = vec4(col * a, a);
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
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vQ;
in vec4 vCol;
out vec4 frag;
void main() {
    float r = length(vQ);
    float core = smoothstep(0.25, 0.0, r);
    float halo = exp(-r * r * 6.0) * (1.0 - smoothstep(0.75, 1.0, r));
    float a = core * (1.0 - vCol.a) + halo * (0.3 + vCol.a);
    frag = vec4(vCol.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well: flat quads (backing, shadows, the ghost)
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

// ---------------------------------------------------------------- the pieces: low poly cubes, one per cell
// The mesh: a chamfered cube, each vertex given as its corner's signs (aS) and which axes it is pulled in along by
// the chamfer (aK). Instance: a = center xyz, size (board units); b = color rgb, seed; c = dull, glow, alpha,
// neighbours (a mask of the cells of the same piece around it: 1 left, 2 right, 4 up, 8 down, 16 .. 128 the diagonals).
inline const char* GLASS_VS = R"(#version 330 core
layout(location = 0) in vec3 aS;
layout(location = 1) in vec3 aK;
layout(location = 2) in vec4 iA;
layout(location = 3) in vec4 iB;
layout(location = 4) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uCube;         // its depth, its chamfer (cell units)
out vec3 vPos;
flat out vec3 vCol;
flat out vec4 vE;
void main() {
    // its front face on the board's plane, its depth behind; a side shared with a cell of the same piece runs on to
    // the cell's edge (no gap, no chamfer: the piece one block), an open side inset by the gap and chamfered
    const float G = 0.04;
    float D = uCube.x, C = uCube.y;
    int m = int(iC.w + 0.5);
    bool mx = aS.x < 0.0 ? (m % 2) == 1 : ((m / 2) % 2) == 1;
    bool my = aS.y > 0.0 ? ((m / 4) % 2) == 1 : ((m / 8) % 2) == 1;
    vec3 p;
    p.x = aS.x * (mx ? 0.5 : 0.5 - G - aK.x * C);
    p.y = aS.y * (my ? 0.5 : 0.5 - G - aK.y * C);
    p.z = -D * 0.5 + aS.z * (D * 0.5 - aK.z * C);
    // an inner corner of the piece, mitred: the cell at the corner (both sides in the piece, the diagonal not) closes
    // on the line where the open sides of its two neighbours meet (inset by the gap, and by the chamfer on the front
    // and back faces), and those neighbours (one side in the piece, the diagonal too) run their open side on to that
    // line, so the edges and chamfers turn cleanly there (no step, no seam)
    int dg = aS.y > 0.0 ? (aS.x < 0.0 ? 16 : 32) : (aS.x < 0.0 ? 64 : 128);
    bool md = (m / dg) % 2 == 1;
    float miter = G + (1.0 - aK.z) * C;
    if (mx && my && !md) p.xy = aS.xy * (0.5 - miter);
    else if (mx && !my && md) p.x = aS.x * (0.5 + miter);
    else if (my && !mx && md) p.y = aS.y * (0.5 + miter);
    vPos = iA.xyz + p * iA.w;
    vCol = iB.rgb;
    vE = iC;
    gl_Position = uVP * vec4(vPos, 1.0);
}
)";
// Flat shaded like the islands: each facet one flat color, its normal from the screen derivatives, lit by the scene's
// sun (seen from the board's camera) and its sky.
inline const char* GLASS_FS = R"(#version 330 core
in vec3 vPos;
flat in vec3 vCol;
flat in vec4 vE;
out vec4 frag;
uniform vec3 uKeyDir, uKeyCol, uAmb, uEyeB;
uniform float uTrip;        // TRIP: soft pastel tiles, a slight soft gradient over each piece, no sun
void main() {
    vec3 n = normalize(cross(dFdx(vPos), dFdy(vPos)));
    if (dot(n, uEyeB - vPos) < 0.0) n = -n;
    float diff = max(dot(n, uKeyDir), 0.0);
    vec3 col = vCol * (uAmb * (0.75 + 0.25 * n.y) + uKeyCol * diff);
    if (uTrip > 0.5) {
        float g = 0.5 + 0.5 * sin(vPos.x * 0.9 + vPos.y * 0.7 + 1.3) * cos(vPos.y * 0.45 - vPos.x * 0.3);
        vec3 c = mix(vCol, mix(vCol, vec3(1.0, 0.86, 0.8), 0.3), g);
        // the front face full, the sides a shade darker (the bottom and the left the darkest)
        float front = smoothstep(0.7, 0.9, n.z);
        float side = n.y > 0.3 ? 0.92 : n.y < -0.3 ? 0.7 : 0.8;
        col = c * mix(side, 1.0, front);
    }
    // states: dulled (game over), glowing (a cleared row), held (dim)
    float g = dot(col, vec3(0.3, 0.55, 0.15));
    col = mix(col, vec3(g * 0.55 + 0.08), vE.x);
    col = mix(col, mix(vCol, vec3(1.0), 0.6) * 1.6, vE.y);
    frag = vec4(col, vE.z);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uExposure, uSlow, uSat, uTone;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
vec3 aces(vec3 x) {
    x = max(x, 0.0);
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec4 s = texture(uScene, vUv);
    float pk = clamp(s.a, 0.0, 1.0);
    // the world through a filmic curve, the pieces as they are (their colors authored)
    // (TRIP: no curve: its colors are display colors)
    vec3 c = mix(aces(s.rgb * uExposure), clamp(s.rgb, 0.0, 1.0), max(pk, 1.0 - uTone));
    // the world's saturation (a vivid tropical grade), the pieces kept as they are
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c, uSat), 1.0 - pk);
    c = mix(c, mix(vec3(g), c * uSlowTint, 0.65), uSlow * (1.0 - pk));
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.16 * uTone * smoothstep(0.45, 0.95, length(q * vec2(1.0, 0.85))) * (1.0 - pk);
    c = mix(c, uBridgeCol, uBridge * (1.0 - pk));
    c = mix(c, uBridgeInk, uBridge * pk);
    c *= (1.0 - 0.5 * uDim) * uFade;
    frag = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
)";

// ---------------------------------------------------------------- TRIP: the psychedelic filter (before the well)
// Over the flat neon render: a slow liquid warp of the whole picture (every edge wavering, melting), and a slow drift
// of the hues to and fro. Calm: a few pixels, over seconds.
inline const char* TRIP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFlow, uWarp, uHue;
uniform vec2 uAspect;
float hq(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float nq(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hq(i), hq(i + vec2(1, 0)), f.x), mix(hq(i + vec2(0, 1)), hq(i + vec2(1, 1)), f.x), f.y);
}
void main() {
    vec2 p = vUv * uAspect * 2.6;
    vec2 d = vec2(nq(p + vec2(0.0, 0.05 * uFlow)) + 0.5 * nq(p * 2.1 + vec2(3.1, -0.07 * uFlow)),
                  nq(p + vec2(7.3, 2.1) - vec2(0.04 * uFlow, 0.0)) + 0.5 * nq(p * 2.1 + vec2(-0.06 * uFlow, 5.7))) - 0.75;
    vec2 uv = vUv + d * uWarp / uAspect;
    vec3 c = sqrt(max(texture(uScene, uv).rgb, 0.0));
    // the hue drift (about the grey axis, in display space)
    float cs = cos(uHue), sn = sin(uHue);
    vec3 k = vec3(0.57735);
    c = c * cs + cross(k, c) * sn + k * dot(k, c) * (1.0 - cs);
    c = clamp(c, 0.0, 1.0);
    frag = vec4(c * c, 0.0);
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

#pragma once
// CORAL REEF's shaders. The world (meters, at a reduced resolution, with depth): the water (its column and the
// rippling surface seen from below), the seabed (white sand with ripples, rock outcrops that crust over as the reef
// matures), the corals, the fish and the manta, the bubbles and the coral seeds; every world shader writes its
// distance from the eye to alpha. Then, at full resolution: the composite (the shafts of sunlight integrated along
// each view ray up to that distance), the veil behind the well, the pieces, the lettering, the post.

namespace coralreef_shaders {

// Shared noise (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}
)";

// The world's water and light (uniforms shared by the world's shaders), the seabed's height (as reef::groundH),
// the caustics.
inline const char* REEF_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uShadeC, uRimC, uWaterC, uDeepC, uHazeC, uCausticC;
uniform float uFogD, uTime, uCaustK, uCaustPhase, uBridge, uNight, uGlowK;
uniform vec3 uBridgeCol;
uniform vec4 uMounds[16];
uniform float uMoundSeed[16];
uniform int uMoundN;
// The light is the shallows' (SURF0: the reef's own depth below the surface); the surface's plane itself rises as
// the camera pulls back (uSurfY), so that the camera, high above the reef late in the growth, stays in the water.
const float SURF0 = 7.5;
uniform float uSurfY;
#define SURF_Y uSurfY
float moundH(vec2 p, vec4 m, float seed) {
    vec2 d = p - m.xy;
    float dd = length(d) / m.z;
    if (dd > 1.6) return 0.0;
    float a = atan(d.y, d.x);
    float edge = 1.0 + 0.18 * sin(3.0 * a + seed * 6.0) + 0.1 * sin(5.0 * a + seed * 11.0);
    float q = dd / edge;
    float prof = 1.0 - smoothstep(0.15, 1.0, q);
    float lumps = 1.0 + 0.16 * sin(d.x * 2.3 / m.z + seed * 3.0) * sin(d.y * 1.9 / m.z + seed * 5.0)
                + 0.09 * sin(d.x * 5.3 / m.z + seed * 7.0) * sin(d.y * 4.7 / m.z + seed * 2.0) * smoothstep(1.0, 0.4, q);
    return m.w * prof * lumps;
}
float moundSum(vec2 p) {
    float s = 0.0;
    for (int i = 0; i < 16; i++) { if (i >= uMoundN) break; s += moundH(p, uMounds[i], uMoundSeed[i]); }
    return s;
}
float groundBase(vec2 p) {
    float h = max(-0.045 * max(0.0, -p.y - 4.0), -3.5);
    h += 0.22 * sin(0.23 * p.x + 0.11 * p.y + 0.5) * sin(0.19 * p.y - 0.08 * p.x + 1.3) +
         0.1 * sin(0.53 * p.x + 0.9) * sin(0.41 * p.y + 0.2);
    return h;
}
// ---- The tower (the growth's later stages, as reef::towerH in coralreef.hpp): the reef stacks up into a pinnacle,
// its summit the reef where it all began (a plateau of TOP_R around TOWER_C), its flanks plunging steeply in
// terraces and ledges, buttresses all round, down to the deep floor at -uTowerH (which sinks as the reef grows).
uniform float uTowerH;
const vec2 TOWER_C = vec2(3.0, -6.0);
const float TOP_R = 46.0, TOWER_SLOPE = 2.6;
float towerProfile(vec2 p, float floorY) {
    vec2 d = p - TOWER_C;
    float r = length(d), a = atan(d.y, d.x);
    float e = 1.0 + 0.2 * sin(3.0 * a + 0.7) + 0.11 * sin(5.0 * a + 2.0) + 0.06 * sin(11.0 * a + 0.3) + 0.03 * sin(23.0 * a);
    float t = r / e - TOP_R;
    float top = -1.6 + 0.4 * sin(0.21 * p.x + 0.4) * sin(0.17 * p.y + 1.1);
    if (t <= 0.0) return top;
    float z = t * TOWER_SLOPE;
    z = z + 5.6 * sin(z * 6.2831 / 38.0 + a * 0.6);   // terraces: steep cliffs and ledges, staggered round the tower
    z += 4.0 * sin(a * 7.0 + z * 0.05) * smoothstep(0.0, 20.0, z);
    float h = top - max(z, 0.0);
    // the floor at the tower's foot: a smooth bend
    float k = 12.0;
    return floorY + log(exp((h - floorY) * 1.0 / k) + 1.0) * k * step(-1e5, h) - 0.0;
}
float bigH(vec2 p) { return towerProfile(p, -max(uTowerH, 2.0)); }
// The whole seabed (meters): the reef's own ground near it, the big terrain beyond.
float groundH(vec2 p, out float rockAmt) {
    float r = length(p);
    float wN = 1.0 - smoothstep(34.0, 46.0, length(p - TOWER_C));
    rockAmt = 0.0;
    float near = 0.0;
    if (wN > 0.0) { rockAmt = r < 80.0 ? moundSum(p) : 0.0; near = max(groundBase(p), -1.6) + rockAmt; }
    float big = wN < 1.0 ? bigH(p) : 0.0;
    return mix(big, near, wN);
}
// ---- How deep the eye is: the water seen from far down is a deep, darkening blue.
uniform float uLensD;
vec3 depthTint(float depth) { return exp(-max(depth, 0.0) * vec3(0.0045, 0.0024, 0.0013)); }
// Caustics: a web of bright lines (the edges of drifting cells, warped), on what faces the sun.
float causticCells(vec2 p, float t) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = hash2(i + g);
            o = 0.5 + 0.38 * sin(t * (0.6 + 0.5 * o.yx) + 6.2831 * o);
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return sqrt(d2) - sqrt(d1);
}
float caustic(vec3 wp) {
    // the light enters at the surface, straight up the sun's way
    vec2 q = wp.xz + uSunDir.xz / max(uSunDir.y, 0.25) * (SURF0 - wp.y);
    q *= 0.55;
    q += 0.35 * vec2(vnoise(q * 0.6 + uCaustPhase * 0.13), vnoise(q * 0.6 + 7.3 - uCaustPhase * 0.11));
    float e = causticCells(q, uCaustPhase) * 0.65 + causticCells(q * 1.73 + 3.1, uCaustPhase * 1.2) * 0.35;
    float c = smoothstep(0.16, 0.0, e);
    return c * c * uCaustK * exp(-max(SURF0 - wp.y, 0.0) * 0.05);
}
// The water seen along a direction (no geometry): the deep below, the bright water toward the surface, the sun's glow.
vec3 waterColor(vec3 v) {
    vec3 c = mix(uDeepC, uWaterC, smoothstep(-0.6, 0.12, v.y));
    c = mix(c, uHazeC, smoothstep(0.12, 0.6, v.y) * 0.55);
    c += uSunC * pow(max(dot(v, uSunDir), 0.0), 6.0) * 0.12;
    // far down, the water around is darker and bluer (brighter looking up toward the light)
    float eyeD = SURF0 - uEye.y;
    c *= mix(depthTint(eyeD), depthTint(eyeD * 0.35), smoothstep(-0.1, 0.7, v.y));
    return c;
}
// The path from the eye to a point, under water: the deeper the point, the less light reaches it (red first, so
// the deep turns blue); the farther, the more of the water's own color.
vec3 waterFog(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    vec3 v = d / max(L, 1e-6);
    col *= mix(vec3(1.0), uDeepC / max(uWaterC, vec3(0.02)), sat(-wp.y * 0.12) * 0.45);
    col *= depthTint(SURF0 - wp.y);
    col = mix(waterColor(v), col, exp(-L / uFogD));
    return mix(col, uBridgeCol, uBridge);
}
// Toon light: the shade (cool, tinted by the water), the light, the full sun; crisp bands.
float toonBand(float ndl) { return smoothstep(-0.08, 0.02, ndl) * 0.55 + smoothstep(0.38, 0.46, ndl) * 0.45; }
vec3 toonLight(vec3 n, float ao) {
    float ndl = dot(n, uSunDir);
    return mix(uShadeC * mix(0.6, 1.0, ao), uSunC, toonBand(ndl)) * mix(0.75, 1.0, ao);
}
vec3 rimLight(vec3 n, vec3 wp) {
    vec3 v = normalize(uEye - wp);
    return uRimC * pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.55;
}
)";

// ---------------------------------------------------------------- the water and its surface
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform vec3 uSurfC;
void main() {
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    vec3 c;
    float dist = 1.0e7;
    {
        c = waterColor(d);
        dist = 400.0;
        if (d.y > 0.0) {
            // the surface from below: a rippling silver sheet (it mirrors the water), bright webs where the sun comes in
            float t = max(0.0, (SURF_Y - uEye.y) / d.y);
            vec3 p = uEye + d * t;
            vec2 q = p.xz * 0.32;
            float w1 = vnoise(q * 1.3 + vec2(uTime * 0.12, uTime * 0.05));
            float w2 = vnoise(q * 3.1 - vec2(uTime * 0.09, -uTime * 0.13));
            float web = smoothstep(0.14, 0.0, causticCells(q * 0.9 + vec2(w1, w2) * 0.6, uTime * 0.45));
            vec3 s = mix(uSurfC * 0.85, uSurfC * 1.15, w1 * 0.6 + w2 * 0.4);
            s += uSunC * web * 0.22;
            s += uSunC * pow(max(dot(d, uSunDir), 0.0), 24.0) * 0.8;
            float k = exp(-t / (uFogD * 1.3));
            c = mix(c, s, k * smoothstep(0.0, 0.08, d.y));
            dist = t;
        }
    }
    frag = vec4(mix(c, uBridgeCol, uBridge), dist);
}
)";

// ---------------------------------------------------------------- the seabed
inline const char* GROUND_VS = R"(#version 330 core
layout(location = 0) in vec2 aRA;   // ring 0..1 (log-spaced radius), angle
uniform mat4 uVP;
uniform vec4 uGrid;                 // center xz, inner radius, outer radius
out vec3 vWorld;
out vec3 vN;
out float vRock;
out float vR;
void main() {
    float r = uGrid.z * pow(uGrid.w / uGrid.z, aRA.x);
    vec2 xz = uGrid.xy + vec2(cos(aRA.y), sin(aRA.y)) * r;
    float rk, dummy;
    float h = groundH(xz, rk);
    float e = max(r * 0.03, 0.0008);
    float hx = groundH(xz + vec2(e, 0.0), dummy) - groundH(xz - vec2(e, 0.0), dummy);
    float hz = groundH(xz + vec2(0.0, e), dummy) - groundH(xz - vec2(0.0, e), dummy);
    vN = normalize(vec3(-hx, 2.0 * e, -hz));
    vWorld = vec3(xz.x, h, xz.y);
    vRock = rk;
    vR = length(xz);
    // the planet's curve, far away
    vec2 dc = xz - uEye.xz;
    vec3 wp = vWorld - vec3(0.0, dot(dc, dc) / (2.0 * 6.371e6), 0.0);
    gl_Position = uVP * vec4(wp - uEye, 1.0);
}
)";
inline const char* GROUND_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in float vRock;
in float vR;
out vec4 frag;
uniform vec3 uSandC, uSand2C, uRockC, uRock2C, uCrustC, uCrust2C;
uniform vec3 uReefA, uReefB, uReefC, uReefD;
uniform float uCover, uSpread;
// The reef carpet: rounded coral heads packed on the floor (a cell each, its own color among four), sand between;
// rgb its color, a its cover; `nrm` the heads' dome (to tilt the normal). Fades to its average when tiny on screen.
vec4 carpet(vec2 p, float cell, float px, out vec2 nrm) {
    vec2 q = p / cell;
    vec2 i = floor(q), f = fract(q);
    float d1 = 8.0; vec2 id = vec2(0.0), rel = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = 0.2 + 0.6 * hash2(i + g);
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d1 = d; id = i + g; rel = r; }
        }
    float d = sqrt(d1);
    float rad = 0.62 + 0.16 * hash(id + 5.0);
    float head = smoothstep(rad, rad * 0.8, d);
    float hs = hash(id);
    vec3 c = hs < 0.25 ? uReefA : hs < 0.5 ? uReefB : hs < 0.75 ? uReefC : uReefD;
    c = mix(c, uCrust2C, 0.25 * hash(id + 9.0));
    c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))), 0.2);
    c *= 0.85 + 0.3 * vnoise(q * 4.0 + id * 3.0);
    c *= 0.5 + 0.65 * sqrt(max(0.0, 1.0 - sq(d / rad)));          // the dome
    c *= 1.0 - 0.2 * smoothstep(0.6, 0.7, vnoise(q * 9.0 + id));   // polyps
    float lod = smoothstep(cell * 0.6, cell * 0.12, px);
    nrm = -rel / max(rad, 1e-3) * head * 0.9 * lod;
    vec3 avg = (uReefA + uReefB + uReefC + uReefD) * 0.25 * 0.85;
    return vec4(mix(avg, c, lod), mix(0.85, head, lod));
}
uniform vec4 uMark;   // the place it began (xz), its glow's radius, strength
uniform vec3 uMarkC;
void main() {
    vec3 wp = vWorld;
    vec3 n = normalize(vN);
    vec2 p = wp.xz;
    float px = max(length(fwidth(p)), 1e-6);   // meters a pixel: fine details fade out as they shrink
    float lodF = 1.0 - smoothstep(0.08, 0.6, px);
    float fb = fbm(p * 0.9);
    float rock = smoothstep(0.32, 0.55, smoothstep(0.08, 0.42, vRock) + (fb - 0.5) * 0.5);
    // sand: ripples (the light catching their crests), a few warm and cool patches, a fine grain
    vec2 rd = normalize(vec2(0.85, 0.53));
    float ph = dot(p, rd) * 5.5 + fbm(p * 0.35) * 5.0;
    float rip = sin(ph);
    vec3 ns = normalize(n + vec3(rd.x, 0.0, rd.y) * cos(ph) * 0.22);
    vec3 sand = mix(uSandC, uSand2C, smoothstep(0.3, 0.8, fbm(p * 0.12 + 3.0)) * 0.7);
    sand *= 0.97 + 0.05 * smoothstep(0.3, 1.0, rip);
    sand *= 0.98 + 0.04 * hash(floor(p * 40.0)) * lodF;
    // seen up close (a macro lens): the grains themselves, rounded, each its own tone
    float lodG = 1.0 - smoothstep(0.0004, 0.0016, px);
    if (lodG > 0.0) {
        vec2 gq = p * 1400.0;
        vec2 gi = floor(gq), gf = fract(gq);
        float d1 = 8.0; vec2 best = vec2(0.0);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 g = vec2(x, y);
                vec2 rr = g + 0.15 + 0.7 * hash2(gi + g) - gf;
                float dd = dot(rr, rr);
                if (dd < d1) { d1 = dd; best = gi + g; }
            }
        float grain = smoothstep(0.5, 0.15, sqrt(d1));
        vec3 gc = mix(uSandC, uSand2C, hash(best)) * (0.85 + 0.3 * hash(best + 3.1));
        gc = mix(gc, vec3(1.0, 0.82, 0.8), step(0.93, hash(best + 7.0)) * 0.5);
        sand = mix(sand, mix(sand * 0.62, gc * (0.8 + 0.35 * grain), grain * 0.9 + 0.1), lodG);
    }
    // rock: weathered reef limestone, pitted and lumpy, crusting over (pink coralline, green turf) as the reef matures
    float r1 = fbm(p * 1.1 + 9.0), r2 = vnoise(p * 4.3);
    float crev = smoothstep(0.7, 0.8, vnoise(p * 5.0 + 3.0));
    vec3 rockC = mix(uRockC, uRock2C, smoothstep(0.35, 0.7, r1));
    rockC *= 0.8 + 0.3 * r2;
    rockC *= 1.0 - 0.3 * crev;
    float cov = fbm(p * 0.55 + 21.0);
    float crust = smoothstep(1.0 - uCover * 0.55, 1.1 - uCover * 0.55, cov + 0.2 * max(n.y, 0.0) + 0.25 * (vnoise(p * 3.1) - 0.5));
    float turf = smoothstep(0.58, 0.68, fbm(p * 1.3 + 4.0)) * smoothstep(0.15, 0.6, uCover);
    vec3 crustC = uCrustC * mix(0.7, 1.1, smoothstep(0.3, 0.8, fbm(p * 2.7 + 5.0))) * (0.85 + 0.25 * r2);
    rockC = mix(rockC, crustC, crust * 0.8);
    rockC = mix(rockC, uCrust2C * mix(0.75, 1.05, r2), turf * (1.0 - crust * 0.6));
    // the rock's own lumps (a bumpy normal)
    vec2 gq = vec2(fbm(p * 1.7 + vec2(0.13, 0.0)) - fbm(p * 1.7 - vec2(0.13, 0.0)), fbm(p * 1.7 + vec2(0.0, 0.13)) - fbm(p * 1.7 - vec2(0.0, 0.13)));
    vec3 nr = normalize(n - vec3(gq.x, 0.0, gq.y) * 2.2);
    vec3 alb = mix(sand, rockC, rock);
    vec3 nn = normalize(mix(ns, nr, rock));
    nn = normalize(mix(n, nn, lodF));
    // beyond the reef: the reef carpet spreading over the lagoon and the ring (a toon patchwork of corals), the
    // beaches and the green of the motus above the water, the dark deep floor
    float depth = SURF0 - wp.y;
    float far = smoothstep(36.0, 46.0, length(p - TOWER_C));
    if (far > 0.0) {
        // the tower's own rock: reef limestone, crusted pink and green, lumpy (its life is the corals on it)
        vec2 tp = mix(p, vec2(p.x + p.y, wp.y), 1.0 - smoothstep(0.35, 0.8, n.y));
        float r1 = fbm(tp * 0.11 + 9.0), r2 = vnoise(tp * 0.9), r3 = fbm(tp * 0.03 + 2.0);
        vec3 rk = mix(uRockC, uRock2C, smoothstep(0.35, 0.7, r1)) * (0.8 + 0.3 * r2);
        float crust = smoothstep(0.45, 0.6, r3 + 0.25 * max(n.y, 0.0));
        rk = mix(rk, uCrustC * (0.75 + 0.35 * r2), crust * 0.75);
        rk = mix(rk, uCrust2C * (0.7 + 0.4 * r2), smoothstep(0.55, 0.7, fbm(tp * 0.07 + 4.0)) * (1.0 - crust) * 0.7);
        // the deep floor at its foot: pale sand
        float floorK = smoothstep(-uTowerH + 14.0, -uTowerH + 3.0, wp.y) * step(10.0, uTowerH);
        rk = mix(rk, uSandC * 0.9, floorK);
        alb = mix(alb, rk, far);
        vec2 g = vec2(fbm(tp * 0.25 + vec2(0.13, 0.0)) - fbm(tp * 0.25 - vec2(0.13, 0.0)), fbm(tp * 0.25 + vec2(0.0, 0.13)) - fbm(tp * 0.25 - vec2(0.0, 0.13)));
        nn = normalize(mix(nn, normalize(n - vec3(g.x, 0.0, g.y) * 1.6), far));
    }
    // the place it all began: a soft glow under it, seen from afar
    if (uMark.w > 0.0) {
        float dm = length(p - uMark.xy) / uMark.z;
        alb += uMarkC * uMark.w * exp(-dm * dm) * 0.6;
    }
    // the sand darkens a little at the rocks' feet
    float ao = 1.0 - 0.35 * smoothstep(0.02, 0.25, vRock) * (1.0 - rock);
    vec3 col = alb * toonLight(nn, ao);
    // (the light under water comes from all around too: the tower's flanks are never black)
    col = mix(col, alb * mix(uShadeC, uSunC, 0.3 + 0.3 * max(nn.x * 0.6 + 0.5, 0.0)), far * (1.0 - smoothstep(0.35, 0.8, n.y)) * 0.55);
    if (uMark.w > 0.0) col += uMarkC * uMark.w * exp(-sq(length(p - uMark.xy) / (uMark.z * 0.6))) * 0.35;
    float lit = toonBand(dot(nn, uSunDir));
    col += uCausticC * caustic(wp) * (0.3 + 0.7 * lit) * mix(0.75, 0.55, rock) * (1.0 - smoothstep(0.3, 2.0, px)) * step(0.0, depth);
    col = waterFog(col, wp);
    frag = vec4(col, length(wp - uEye));
}
)";

// ---------------------------------------------------------------- corals
inline const char* CORAL_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;  // along, param, part, phase
layout(location = 3) in vec4 iA;     // base xyz, size
layout(location = 4) in vec4 iB;     // yaw, grow, seed, kind
layout(location = 5) in vec4 iC;     // color A, sway
layout(location = 6) in vec4 iD;     // color B, glow
uniform mat4 uVP;
uniform vec2 uSurge;   // the water's surge (direction * amount)
uniform float uPulse;  // the drop: the corals breathe with the beat, polyps opening
out vec3 vWorld;
out vec3 vN;
out vec3 vLocal;
out vec4 vAttr;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float g = iB.y;
    float c = cos(iB.x), s = sin(iB.x);
    vec3 p = aPos;
    // growing: from a bud at the foot, the tips last
    float grow = g * (0.25 + 0.75 * smoothstep(0.0, 1.0, g));
    vec3 lp = p * grow * (1.0 + uPulse * 0.04 * aAttr.x);
    vec3 wp = iA.xyz + vec3(c * lp.x + s * lp.z, lp.y, -s * lp.x + c * lp.z) * iA.w;
    vec3 n = vec3(c * aNrm.x + s * aNrm.z, aNrm.y, -s * aNrm.x + c * aNrm.z);
    // the surge sways what is soft (fans, tentacles, whips), more toward the tips
    float sway = iC.w * aAttr.x * aAttr.x * iA.w * grow;
    float ph = aAttr.w * 6.2831 + iB.z * 6.2831;
    float wave = sin(uTime * 0.9 + wp.x * 0.25 + wp.z * 0.2) * 0.75 + 0.25 * sin(uTime * 2.1 + ph);
    wp.xz += uSurge * wave * sway;
    wp.y -= abs(wave) * sway * 0.08;
    vWorld = wp;
    vN = n;
    vLocal = aPos;
    vAttr = aAttr;
    vB = iB; vC = iC; vD = iD;
    gl_Position = uVP * vec4(wp - uEye, 1.0);
}
)";
inline const char* CORAL_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in vec3 vLocal;
in vec4 vAttr;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec3 uRockC, uRock2C, uCrustC, uCrust2C;
uniform float uCover, uPulse;
void main() {
    int kind = int(vB.w + 0.5);
    vec3 n = normalize(vN);
    if (!gl_FrontFacing) n = -n;
    vec3 A = vC.rgb, B = vD.rgb;
    float along = vAttr.x, part = vAttr.z;
    vec3 alb = A;
    float ao = 1.0, glow = 0.0;
    float seed = vB.z * 37.0;
    if (kind == 0) {
        // brain coral: meandering grooves (the iso-lines of a warped noise), rounded ridges between them
        vec3 q = vLocal * 10.0 + seed;
        vec3 w = q + 0.55 * vec3(vnoise3(q * 0.5), vnoise3(q * 0.5 + 7.1), vnoise3(q * 0.5 + 13.7));
        float v1 = vnoise3(w), v2 = vnoise3(w * 1.9 + 5.3);
        float d = min(abs(v1 - 0.5), abs(v2 - 0.5) * 1.25);
        float aaN = max(fwidth(d), 1e-3);
        float groove = 1.0 - smoothstep(0.02, 0.02 + 2.0 * aaN + 0.03, d);
        float ridge = smoothstep(0.04, 0.2, d);
        alb = mix(mix(A, B, 0.3), mix(A, B, 0.75) * 1.08, ridge);
        alb = mix(alb, A * 0.3, groove);
        ao = mix(0.5, 1.0, 1.0 - groove) * mix(0.55, 1.0, smoothstep(0.0, 0.35, along));
        glow = ridge * 0.35;
    } else if (kind == 1) {
        // staghorn: the color deepening down the branches, pale tips, fine corallites
        alb = mix(A, B, smoothstep(0.45, 0.95, along));
        alb = mix(alb, mix(B, vec3(1.0), 0.45), smoothstep(0.93, 1.0, along));
        float dots = smoothstep(0.62, 0.72, vnoise3(vLocal * 70.0 + seed));
        alb *= 1.0 - 0.18 * dots;
        ao = mix(0.5, 1.0, smoothstep(0.0, 0.5, along));
        glow = smoothstep(0.7, 1.0, along);
    } else if (kind == 2) {
        // table coral: radial ridges and growth rings on top, the rim paler, its underside in shade
        float rad = sin(vAttr.y * 6.2831 * 46.0 + vnoise3(vLocal * 6.0) * 3.0);
        float ring = sin(along * 34.0 + vnoise3(vLocal * 4.0 + seed) * 2.0);
        alb = mix(A, B, smoothstep(0.55, 1.0, along));
        alb *= 0.9 + 0.1 * rad;
        alb *= 0.94 + 0.06 * ring;
        if (part > 0.5) { alb = A * 0.62; ao = 0.6; }
        glow = smoothstep(0.7, 1.0, along) * (1.0 - part);
    } else if (kind == 3) {
        // sea fan: a lace of fine branches (cut out of its sheet), thicker main branches from the stem
        if (part < 0.5) {
            float r = along, a = vAttr.y;
            vec2 q = vec2(a * r * 24.0, r * 18.0) + seed;
            vec2 ci = floor(q), cf = fract(q);
            float d1 = 8.0, d2 = 8.0;
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++) {
                    vec2 gg = vec2(x, y);
                    vec2 rr = gg + 0.15 + 0.7 * hash2(ci + gg) - cf;
                    float d = dot(rr, rr);
                    if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
                }
            float edge = sqrt(d2) - sqrt(d1);
            float wv = 0.16 + 0.1 * (1.0 - r);
            float main = abs(fract(a * 2.2 + 0.5) - 0.5) / 2.2 * r * 8.5;
            float on = max(smoothstep(wv, wv * 0.4, edge), smoothstep(0.09, 0.04, main) * step(r, 0.92));
            on = max(on, smoothstep(0.955, 0.975, r));
            if (on < 0.5) discard;
            alb = mix(A, B, smoothstep(0.3, 1.0, r)) * (0.85 + 0.25 * smoothstep(wv, 0.0, edge));
            ao = mix(0.75, 1.0, r);
            glow = 0.6;
        }
    } else if (kind == 4) {
        // tube sponges: pitted walls, a lip catching the light, dark inside
        float pits = smoothstep(0.6, 0.7, vnoise3(vLocal * 46.0 + seed));
        alb = mix(A, B, smoothstep(0.75, 1.0, along) * 0.6) * (1.0 - 0.3 * pits);
        if (part > 0.5) { alb = A * 0.16; ao = 0.3; }
        ao *= mix(0.6, 1.0, smoothstep(0.0, 0.4, along));
        glow = smoothstep(0.85, 1.0, along) * 0.5 * (1.0 - part);
    } else if (kind == 5) {
        // anemone: a fleshy column, a crown of tentacles with bright tips
        if (part < 0.5) { alb = A * (0.75 + 0.1 * sin(atan(vLocal.z, vLocal.x) * 18.0)); ao = 0.75; }
        else {
            alb = mix(B, mix(A, vec3(1.0), 0.35), smoothstep(0.75, 1.0, along));
            ao = mix(0.65, 1.0, along);
            glow = smoothstep(0.6, 1.0, along);
        }
    } else if (kind == 7) {
        // a boulder of reef rock: mottled, pitted, crusting over (pink coralline, green turf) on its top
        vec3 lp = vLocal * 3.0 + seed;
        float r1 = vnoise3(lp * 1.3), r2 = vnoise3(lp * 5.0);
        alb = mix(uRockC, uRock2C, smoothstep(0.3, 0.75, r1)) * (0.82 + 0.3 * r2);
        float pit = smoothstep(0.7, 0.8, vnoise3(lp * 7.0 + 4.0));
        alb *= 1.0 - 0.35 * pit;
        float top = smoothstep(0.1, 0.8, n.y);
        float crust = smoothstep(1.0 - uCover * 0.7, 1.12 - uCover * 0.7, vnoise3(lp * 0.9 + 11.0) * 0.8 + 0.35 * top);
        float turf = smoothstep(0.6, 0.7, vnoise3(lp * 1.7 + 2.0) * 0.7 + 0.35 * top) * smoothstep(0.1, 0.5, uCover);
        alb = mix(alb, uCrustC * (0.75 + 0.35 * r2), crust * 0.85);
        alb = mix(alb, uCrust2C * (0.7 + 0.4 * r2), turf * (1.0 - 0.6 * crust));
        ao = mix(0.55, 1.0, smoothstep(0.0, 0.45, along));
    } else {
        // sea whips: rods bristling with polyps, paler toward their tips
        alb = mix(A, B, smoothstep(0.2, 1.0, along));
        alb *= 1.0 - 0.15 * smoothstep(0.6, 0.7, vnoise3(vLocal * 90.0 + seed));
        ao = mix(0.6, 1.0, along);
        glow = along;
    }
    if (kind == 3) n = dot(n, uSunDir) < 0.0 ? normalize(n - 2.0 * dot(n, uSunDir) * uSunDir * 0.7) : n; // (light comes through a fan)
    vec3 col = alb * toonLight(n, ao);
    float lit = toonBand(dot(n, uSunDir));
    col += uCausticC * caustic(vWorld) * alb * 1.6 * smoothstep(-0.2, 0.6, n.y) * (0.4 + 0.6 * lit);
    col += rimLight(n, vWorld) * mix(vec3(1.0), alb, 0.5);
    // fluorescence (a night variant), and a touch of glow in every one
    col += B * glow * (uGlowK * vD.w);
    col += mix(alb, B, 0.5) * (glow * 0.55 + 0.08) * uPulse * vD.w;
    col = waterFog(col, vWorld);
    frag = vec4(col, length(vWorld - uEye));
}
)";

// ---------------------------------------------------------------- fish and the manta
inline const char* FISH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;
layout(location = 3) in vec4 iA;    // position, size
layout(location = 4) in vec4 iB;    // direction, swim phase
layout(location = 5) in vec4 iC;    // species, swim amount, bank, alpha
uniform mat4 uVP;
out vec3 vWorld;
out vec3 vN;
out vec3 vLocal;
out vec4 vAttr;
flat out vec4 vC;
void main() {
    vec3 fwd = normalize(iB.xyz);
    vec3 side = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)) + vec3(1e-4, 0.0, 0.0));
    vec3 up = cross(side, fwd);
    float b = iC.z;
    vec3 up2 = up * cos(b) + side * sin(b), side2 = side * cos(b) - up * sin(b);
    vec3 p = aPos, n = aNrm;
    int sp = int(iC.x + 0.5);
    if (sp == 7) {
        // the manta: x the span, z the body (head +z); its wings beat slowly, a wave running out to the tips
        float ax = abs(p.x);
        float k = pow(ax, 1.5);
        float ph = iB.w - ax * 1.4;
        p.y += 0.24 * k * sin(ph) * (aAttr.z > 0.5 ? 0.0 : 1.0);
        float dy = 0.36 * pow(ax, 0.5) * sin(ph) * sign(p.x) - 0.24 * k * cos(ph) * 1.4 * sign(p.x);
        n = normalize(n + vec3(-dy, 0.0, 0.0) * (aAttr.z > 0.5 ? 0.0 : 1.0));
        if (aAttr.z > 0.5) p.x += 0.05 * sin(iB.w * 0.5 + p.z * 3.0) * (-p.z - 0.42);
        // local: x -> -side, y -> up, z -> forward (a right-handed frame: the faces keep their sides)
        vec3 wp = iA.xyz + (-side2 * p.x + up2 * p.y + fwd * p.z) * iA.w;
        vWorld = wp;
        vN = -side2 * n.x + up2 * n.y + fwd * n.z;
    } else {
        // a fish: x along the body (nose +x); it swims with a wave down its body, stronger at the tail
        float tailK = smoothstep(0.35, -0.75, p.x);
        float wave = sin(p.x * 7.0 - iB.w) * iC.y * tailK;
        p.z += wave * 0.16;
        n.x -= cos(p.x * 7.0 - iB.w) * iC.y * tailK * 0.5 * sign(n.z + 1e-4);
        vec3 wp = iA.xyz + (fwd * p.x + up2 * p.y + side2 * p.z) * iA.w;
        vWorld = wp;
        vN = fwd * n.x + up2 * n.y + side2 * n.z;
    }
    vLocal = aPos;
    vAttr = aAttr;
    vC = iC;
    gl_Position = uVP * vec4(vWorld - uEye, 1.0);
}
)";
inline const char* FISH_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in vec3 vLocal;
in vec4 vAttr;
flat in vec4 vC;
out vec4 frag;
uniform float uFishGlow;
vec3 C(float r, float g, float b) { return pow(vec3(r, g, b), vec3(2.2)); }
float band(float x, float a, float b, float w) { return smoothstep(a - w, a + w, x) * (1.0 - smoothstep(b - w, b + w, x)); }
void main() {
    int sp = int(vC.x + 0.5);
    vec3 n = normalize(vN);
    if (!gl_FrontFacing) n = -n;
    float part = vAttr.z;
    float x = vLocal.x, y = vLocal.y, ny = vAttr.y;
    vec3 alb;
    float belly = smoothstep(0.0, -0.8, ny) * (part < 0.5 ? 1.0 : 0.0);
    if (sp == 7) {
        // the manta: ink above with pale shoulder marks, white below
        bool top = gl_FrontFacing;
        alb = top ? C(0.12, 0.15, 0.22) : C(0.93, 0.95, 0.97);
        if (top) {
            float sh = exp(-sq((abs(x) - 0.3) / 0.13) - sq((vLocal.z - 0.12) / 0.12));
            alb = mix(alb, C(0.75, 0.8, 0.85), sh * 0.7);
        }
        if (part > 0.5) alb = C(0.1, 0.12, 0.16);
    } else if (sp == 0) { // chromis: blue-green, iridescent
        alb = mix(C(0.1, 0.75, 0.75), C(0.2, 0.45, 1.0), smoothstep(-0.5, 0.8, ny));
        alb = mix(alb, C(0.65, 0.95, 0.9), belly * 0.6);
        if (part > 0.5) alb = C(0.15, 0.6, 0.85);
    } else if (sp == 1) { // anthias: orange pink, lilac fins
        alb = mix(C(1.0, 0.45, 0.35), C(1.0, 0.62, 0.3), smoothstep(-0.6, 0.6, ny));
        alb = mix(alb, C(1.0, 0.85, 0.7), belly * 0.5);
        if (part > 0.5) alb = C(0.85, 0.45, 0.85);
    } else if (sp == 2) { // yellow tang
        alb = C(1.0, 0.86, 0.1);
        alb = mix(alb, C(1.0, 0.95, 0.6), belly * 0.3);
        if (part > 0.5) alb = C(1.0, 0.8, 0.05);
    } else if (sp == 3) { // blue tang: royal blue, a dark scribble, a yellow tail
        alb = C(0.1, 0.35, 1.0);
        float scrib = band(ny, 0.05, 0.75, 0.06) * band(x, -0.38, 0.28, 0.05) * smoothstep(0.1, 0.0, abs(ny - 0.45 - 0.35 * x) - 0.22);
        alb = mix(alb, C(0.04, 0.05, 0.15), scrib);
        alb = mix(alb, C(1.0, 0.85, 0.1), smoothstep(-0.36, -0.44, x) * (part < 0.5 ? 1.0 : 0.0));
        if (part > 0.5) alb = part < 1.5 ? C(1.0, 0.85, 0.1) : C(0.05, 0.08, 0.25);
    } else if (sp == 4) { // moorish idol: white and yellow under two black bands, a long white streamer
        alb = mix(C(1.0, 0.97, 0.88), C(1.0, 0.86, 0.25), smoothstep(-0.1, -0.35, x));
        float b1 = band(x, 0.05, 0.25, 0.015), b2 = band(x, -0.33, -0.2, 0.015);
        alb = mix(alb, C(0.06, 0.06, 0.08), max(b1, b2));
        if (part > 0.5) alb = part < 1.5 ? C(0.06, 0.06, 0.08) : C(1.0, 0.98, 0.92);
    } else if (sp == 5) { // parrotfish: teal with pink scale edges, a pink face
        float sc = smoothstep(0.35, 0.45, abs(fract(x * 14.0 + abs(ny) * 3.0) - 0.5));
        alb = mix(C(0.1, 0.8, 0.65), C(1.0, 0.5, 0.65), sc * 0.45);
        alb = mix(alb, C(1.0, 0.55, 0.7), smoothstep(0.3, 0.42, x) * 0.7);
        if (part > 0.5) alb = C(0.2, 0.55, 0.95);
    } else { // clownfish: orange, three white bands edged in black
        alb = C(1.0, 0.42, 0.05);
        float b1 = band(x, 0.12, 0.22, 0.01), b2 = band(x, -0.12, 0.0, 0.01), b3 = band(x, -0.46, -0.4, 0.01);
        float wb = max(max(b1, b2), b3);
        float edge = max(max(band(x, 0.1, 0.24, 0.01), band(x, -0.14, 0.02, 0.01)), band(x, -0.48, -0.38, 0.01)) - wb;
        alb = mix(alb, C(0.05, 0.03, 0.03), sat(edge) * (part < 0.5 ? 1.0 : 0.0));
        alb = mix(alb, C(1.0, 1.0, 1.0), wb * (part < 0.5 ? 1.0 : 0.0));
        if (part > 0.5) alb = mix(C(1.0, 0.45, 0.05), C(0.05, 0.03, 0.03), smoothstep(0.75, 0.95, abs(ny)));
    }
    // the tail's fork
    if (part > 0.5 && part < 1.5 && vAttr.w > 0.0) {
        float u = -vAttr.x;
        if (u > 1.0 - vAttr.w * 0.5 * (1.0 - abs(ny))) discard;
    }
    // an eye on each side
    if (sp != 7 && part < 0.5) {
        float ex = x - 0.36, ey = y - 0.045;
        float e = length(vec2(ex, ey * 1.1));
        alb = mix(alb, C(1.0, 1.0, 0.95), smoothstep(0.042, 0.032, e));
        alb = mix(alb, C(0.02, 0.02, 0.04), smoothstep(0.03, 0.022, e));
    }
    float ao = part > 0.5 ? 0.85 : 1.0;
    vec3 col = alb * toonLight(n, ao);
    col += uCausticC * caustic(vWorld) * alb * 0.8 * smoothstep(-0.3, 0.7, n.y);
    col += rimLight(n, vWorld) * 0.8;
    col += alb * uFishGlow * 0.25;
    col = waterFog(col, vWorld);
    frag = vec4(col, length(vWorld - uEye));
}
)";

// ---------------------------------------------------------------- bubbles, coral seeds, specks (billboards)
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;   // position, size
layout(location = 2) in vec4 aB;   // color, alpha
layout(location = 3) in vec4 aC;   // mode, seed
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
out vec3 vWorld;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float dist = length(aA.xyz - uEye);
    float size = max(aA.w, dist * uMinPx);
    vec3 wp = aA.xyz + (uCamR * aPos.x + uCamU * aPos.y) * size;
    vLocal = aPos;
    vWorld = aA.xyz;
    vB = vec4(aB.rgb, aB.a * min(1.0, aA.w / max(size, 1e-5)));
    vC = aC;
    gl_Position = uVP * vec4(wp - uEye, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vWorld;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    float r = length(vLocal);
    if (r > 1.0) discard;
    int mode = int(vC.x + 0.5);
    float L = length(vWorld - uEye);
    float fogK = exp(-L / uFogD);
    vec3 c;
    float a;
    if (mode == 0) {
        // a bubble: a clear ball, its rim bright, a highlight up toward the sun
        float rim = smoothstep(0.62, 0.93, r) * smoothstep(1.0, 0.93, r);
        float hi = smoothstep(0.32, 0.12, length(vLocal - vec2(-0.32, 0.38)));
        float body = 0.12 * smoothstep(1.0, 0.0, r);
        a = (rim * 0.85 + hi + body) * vB.a;
        c = mix(vB.rgb, vec3(1.0), 0.35 + 0.65 * hi);
    } else if (mode == 1) {
        // a coral seed: a soft glowing bead
        float core = exp(-r * r * 12.0), halo = exp(-r * r * 3.5) * 0.45;
        a = min(1.0, core * 1.2 + halo) * vB.a;
        c = mix(vB.rgb * 1.8, vec3(1.6), core * 0.6);
    } else if (mode == 3) {
        // a coral egg bundle: a little glowing bead (it keeps glowing through the water a while)
        float core = exp(-r * r * 10.0), halo = exp(-r * r * 2.6) * 0.5;
        a = min(1.0, core + halo) * vB.a;
        c = mix(vB.rgb * 1.6, vec3(1.5, 1.4, 1.3), core * 0.45);
        fogK = max(fogK, 0.4 * exp(-L / (uFogD * 3.0)));
    } else {
        // a speck drifting in the water
        a = exp(-r * r * 6.0) * vB.a;
        c = vB.rgb;
    }
    c = mix(waterColor(normalize(vWorld - uEye)), c, fogK);
    a *= 0.35 + 0.65 * fogK;
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- composite: the world + the shafts of sunlight
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform vec3 uRayC;
uniform vec2 uSunVP;
uniform float uRays, uSat, uRayScale;
uniform vec4 uWellUv;   // the well's rectangle (uv), where the world is blurred
uniform float uBlur, uBlurPx;
uniform float uFocus, uDof; // the macro lens: what lies beyond the focus melts away
void main() {
    vec4 w = texture(uWorld, vUv);
    vec3 c = w.rgb;
    {
        vec2 dw = max(max(uWellUv.xy - vUv, vUv - uWellUv.zw), 0.0);
        float k = (1.0 - smoothstep(0.0, 0.02, length(dw))) * uBlur;
        float coc = uDof * clamp(1.0 - uFocus / max(w.a, 1e-6), 0.0, 1.0);   // pixels of the world target
        float rad = max(k > 0.0 ? 1.0 : 0.0, coc);
        if (rad > 0.05) {
            vec2 px = 1.0 / vec2(textureSize(uWorld, 0));
            vec3 acc = c;
            float wsum = 1.0;
            for (int i = 0; i < 16; i++) {
                float an = float(i) * 2.39996;
                float r = (2.5 + float(i) * 0.9) * max(k, 0.0) * uBlurPx + coc * sqrt((float(i) + 0.5) / 16.0);
                vec4 s = texture(uWorld, vUv + vec2(cos(an), sin(an)) * r * px);
                // (a sharp foreground does not bleed into the blur)
                float ws = s.a < uFocus * 0.8 && coc > 0.5 ? 0.15 : 1.0;
                acc += s.rgb * ws;
                wsum += ws;
            }
            c = mix(c, acc / wsum, max(k, smoothstep(0.0, 1.5, coc)));
        }
    }
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    // the shafts: fanning down from the surface, from a point high above the view (toward the sun), drifting
    // slowly; they show over the water's depth in front of what they cross (none on what is near)
    vec2 uv = vUv;
    vec2 vp = uSunVP;
    vec2 dd = vec2((uv.x - vp.x) * uTan.x / uTan.y, uv.y - vp.y);
    float ang = atan(dd.x, -dd.y);
    float s1 = vnoise(vec2(ang * 17.0, uTime * 0.12));
    float s2 = vnoise(vec2(ang * 43.0 + 5.0, uTime * 0.2));
    float sh = smoothstep(0.45, 0.95, s1) * (0.55 + 0.45 * s2) + 0.25 * smoothstep(0.6, 1.0, s2);
    float depthK = (1.0 - exp(-min(w.a, 60.0 * uRayScale) / (14.0 * uRayScale))) * 1.0;
    float fall = mix(0.35, 1.0, smoothstep(0.1, 0.95, uv.y));
    c += uRayC * sh * depthK * fall * uRays * 0.16;
    c = mix(c, uBridgeCol, uBridge * 0.0);
    // the floor darkens a little toward the bottom of the view (the HUD's corner reads on it)
    c *= mix(0.8, 1.0, smoothstep(0.0, 0.3, uv.y));
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSat), 0.0);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the veil: the well and the HUD's columns
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
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0)) - 0.12;
    float a = uWellA * (1.0 - smoothstep(-0.1, 0.8, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    // the well's own glass: a little lighter toward its top, where the light comes from
    vec3 veil = uVeil * (1.0 + 0.12 * smoothstep(-10.0, 10.0, w.y) * step(dw, 0.0));
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.1;
    rail *= smoothstep(10.4, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = veil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: one chunk of living coral per cell
// Each block a chunk of real coral filling its cell, its edges soft, rounded and irregular, touching its neighbours
// like a living stack. Its species is its piece type's, each with its own relief, a height field over the cell:
//   0 (I) staghorn: branching antlers (capsule SDFs: a trunk, a side branch, a forked tip), a second layer behind
//         them in the shade, the dark between them the reef's depth;
//   1 (O) star coral: corallites packed like cells (Worley), each a dome of radiating septa round a central pit;
//   2 (T) porous coral: round pits of every size (Worley) in a pitted, lumpy crust;
//   3 (S) plate coral: one smooth encrusting plate over the whole colony (no seams between its blocks), a raised lip
//         along its wavy free edge, a few cup-like dimples;
//   4 (Z) tube coral: tubes packed tight, each a pale ring round a dark hole;
//   5 (J) sponge: deep irregular holes (a domain-warped Worley), fine pores between them;
//   6 (L) brain coral: meandering ridges and grooves (the contour lines of a domain-warped noise).
// Each block its own variation (its seed: its offset into the texture, a little of its tone), a piece one colony
// (its blocks' shared sides reach under each other, the swell between them a soft valley; between two colonies a
// dark seam). The relief is lit from above (a key light, a soft wrap and a bounce of the block's own color into the
// shade, as light scatters in the living tissue), the pits and grooves in ambient occlusion, a fine sugary grain all
// over; normals from the height field by finite differences a pixel apart (three samples a pixel). The detail fades
// out when a block is small on screen. A first pass lays a soft contact shadow under the stack (a soft glow at
// night), the second the blocks. The ghost is a thin crust of coral along the piece's cells' outlines.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, the sides shared with its colony (1 up, 2 down, 4 left, 8 right) + 16 x type
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 block, 1 ghost), erase 0..1, the colony's seed, the block's seed
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z * 1.36;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uTime, uBridge, uShimmer, uPass, uHaloA, uGlow;
uniform vec2 uHaloOff;
uniform vec3 uBridgeInk, uLightC, uHaloC;
int gNb;       // the sides it shares with its colony
float gAA;     // a pixel, in cell units
float gLod;    // 1: the fine detail drawn .. 0: too small on screen for it
vec2 gOff;     // the block's own offset into its species' texture
vec2 gCol;     // its place in its colony (cells): the colony's own textures run on across its blocks
vec2 gColOff;  // the colony's offset into them
bool gAlt;     // the colony's species in its second form (half the colonies)
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float smax(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (a - b) / k, 0.0, 1.0); return mix(b, a, h) + k * h * (1.0 - h); }
vec2 rot(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }
// the nearest of jittered points: x its distance, y the second nearest's, zw the vector to it; id its lattice cell
vec4 voro(vec2 p, float jit, out vec2 id) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    vec2 r1 = vec2(0.0);
    id = i;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 r = g + 0.5 + jit * (hash2(i + g) - 0.5) - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; r1 = r; id = i + g; }
            else if (d < d2) d2 = d;
        }
    return vec4(sqrt(d1), sqrt(d2), r1);
}
// the chunk's outline: the signed distance (cell units, < 0 inside) to a rounded square, its sides shared with its
// colony reaching past the cell (under its neighbour), its free sides inside it, wobbling (a wobble of the colony's,
// running on along a free side from one of its blocks to the next)
float outline(vec2 q, float shared, float free, float r, float wob, float freq) {
    float eR = (gNb & 8) != 0 ? shared : free, eL = (gNb & 4) != 0 ? shared : free;
    float eU = (gNb & 1) != 0 ? shared : free, eD = (gNb & 2) != 0 ? shared : free;
    vec2 c = vec2(eR - eL, eU - eD) * 0.5, b = vec2(eR + eL, eU + eD) * 0.5;
    vec2 d = abs(q - c) - b + r;
    float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
    vec2 p = (q + gCol) * freq + gColOff;
    return sd + wob * (vnoise(p) - 0.5) + wob * 0.5 * (vnoise(p * 2.4 + 3.7) - 0.5);
}

// ---- the species: each the relief at q (cell units): x its height (cell units), y its lightness (0 shaded tissue
// .. 1 the bright tips, rims, ridges), z its openness (0 deep in a pit .. 1 out in the light), w its cover (0 a gap)
// one branch: > 0 inside (its radius from its axis), tapering from a to b
float seg(vec2 p, vec2 a, vec2 b, float ra, float rb) {
    vec2 pa = p - a, ba = b - a;
    float t = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return mix(ra, rb, t) - length(pa - ba * t);
}
// an antler: a trunk, a side branch from its middle, a fork at its tip, the joints filleted
float antler(vec2 p, vec2 a, float ang, float len, float r, float side) {
    vec2 dir = vec2(cos(ang), sin(ang));
    vec2 b = a + dir * len, m = a + dir * len * 0.4;
    float f = seg(p, a, b, r, r * 0.95);
    f = smax(f, seg(p, m, m + rot(dir, side * 1.05) * len * 0.5, r * 0.9, r * 0.85), 0.03);
    f = smax(f, seg(p, b, b + rot(dir, 0.6) * len * 0.42, r * 0.9, r * 0.85), 0.03);
    f = smax(f, seg(p, b, b + rot(dir, -0.65) * len * 0.36, r * 0.88, r * 0.82), 0.03);
    return f;
}
vec4 staghorn(vec2 q, float d) {
    // the colony's antlers, laid out cell by cell in its own coordinates: a block draws the ones rooted in it and in
    // the block under it (so that those growing up across the seam run on unbroken)
    float front = -1.0, back = -1.0;
    for (int k = 0; k < 2; k++) {
        vec2 cc = gCol - vec2(0.0, float(k));
        vec2 o = vec2(0.0, -float(k));
        for (int i = 0; i < 2; i++) {
            float fi = float(i);
            vec2 sd = gColOff + cc * 7.13 + fi * 3.71;
            vec2 r = hash2(sd), r2 = hash2(sd.yx + 1.9), r3 = hash2(sd * 1.3 + 4.2);
            // in front: antlers rising, leaning apart
            vec2 a = o + vec2(-0.22 + 0.44 * fi + (r.x - 0.5) * 0.2, -0.62 + r.y * 0.6);
            front = max(front, antler(q, a, 1.571 + (r2.x - 0.5) * 1.0 - (fi - 0.5) * 0.9, 0.36 + 0.12 * r2.y, 0.138, fi > 0.5 ? 1.0 : -1.0));
            // behind them, in their shade: more, any way
            if (k == 0) {
                float ang = r3.x * 6.283;
                vec2 b = vec2(r3.y - 0.5, r.x - 0.5) * 0.4 - vec2(cos(ang), sin(ang)) * 0.24;
                back = max(back, antler(q, b, ang, 0.46, 0.12, r2.y > 0.5 ? 1.0 : -1.0));
            }
        }
    }
    vec2 c = q + gCol + gColOff;
    float lump = gLod * (0.02 * (vnoise(c * 16.0) - 0.5) + 0.01 * (vnoise(c * 40.0) - 0.5));
    front += lump; back += lump;
    float hF = sqrt(max(front, 0.0) * max(0.25 - front, 0.0)) + 0.08, hB = sqrt(max(back, 0.0) * max(0.22 - back, 0.0));
    float inF = smoothstep(-gAA, gAA, front), inB = smoothstep(-gAA, gAA, back);
    float h = mix(mix(-0.04, hB, inB), hF, inF);
    // the back's in the front's shade where they cross; the gaps the reef's dark depth
    float shade = 1.0 - smoothstep(-0.08, 0.0, front) * 0.6;
    float light = mix(mix(0.0, 0.72, inB), 1.0, inF);
    float open = mix(mix(0.05, shade * 0.85, inB), 1.0, inF);
    // the branches reach a little past the chunk; the gaps between them half open on the dark within it
    float cover = max(max(inF, inB) * (1.0 - smoothstep(0.0, 0.08, d)), (1.0 - smoothstep(-gAA, gAA, d)) * 0.14);
    return vec4(h, light, open, cover);
}
vec4 star(vec2 q) {
    // one great corallite in the block, its fine septa radiating from its central pit out to the block's edges (in
    // its second form a few smaller ones, packed, a valley between them)
    vec2 c0 = (hash2(gOff + 1.7) - 0.5) * 0.22, id;
    vec2 v = q - c0;
    float r = length(v), n = 30.0, valley = 1.0, R = 0.75;
    if (gAlt) {
        vec4 w = voro(q * 2.1 + gOff, 0.7, id);
        v = -w.zw / 2.1; r = w.x / 2.1; n = 18.0; R = 0.42;
        valley = smoothstep(0.0, 0.25, w.y - w.x);
    }
    float ang = atan(v.y, v.x);
    float sept = cos(ang * n + 3.0 * vnoise(q * 4.0 + gOff) + 1.5 * vnoise(q * 11.0 + gOff.yx) + r * 4.0);
    // (they fade out toward the pit, where they crowd closer than a few pixels)
    float fade = smoothstep(2.0, 4.0, r * 6.283 / n / gAA) * gLod;
    float pit = smoothstep(0.08, 0.015, r);
    float fuzz = vnoise(q * 34.0 + gOff) - 0.5;
    float sp = smoothstep(-0.6, 0.9, sept + fuzz * 0.8) * fade;
    float h = (0.022 * sp + 0.06 * smoothstep(R, 0.05, r)) * mix(0.5, 1.0, valley) - 0.04 * pit;
    // (bright toward its heart, its outer septa deeper in tone)
    float light = mix(0.45, 1.0, sp) * mix(0.55, 1.08, smoothstep(R * 0.8, R * 0.13, r)) * mix(0.6, 1.0, valley);
    return vec4(h, light, mix(mix(0.45, 1.0, smoothstep(-0.8, 0.3, sept * fade + fuzz * 0.3)), 0.12, pit) * mix(0.5, 1.0, valley), 1.0);
}
vec4 porous(vec2 q) {
    vec2 id, id2;
    vec4 v = voro(q * 5.6 + gOff, 0.85, id);
    float rad = 0.3 + 0.12 * hash(id + 3.1);
    float pit = smoothstep(rad, rad * 0.55, v.x);
    float lip = smoothstep(rad * 1.6, rad * 1.02, v.x) * (1.0 - pit);
    // and small pits between
    vec4 w = voro(q * 9.0 + gOff.yx, 0.9, id2);
    float pit2 = smoothstep(0.2, 0.1, w.x) * step(0.55, hash(id2)) * (1.0 - lip) * gLod;
    float lumps = vnoise(q * 9.0 + gOff) - 0.5;
    float h = 0.016 * lip - 0.06 * pit - 0.02 * pit2 + 0.025 * lumps * gLod;
    return vec4(h, 0.7 + 0.3 * lip - 0.3 * max(pit, pit2) + 0.3 * lumps, mix(1.0, 0.16, max(pit, pit2 * 0.7)), 1.0);
}
// the porous coral's second form: cup corals, raised rings round deep cups, here and there on a lumpy crust
vec4 cups(vec2 q) {
    vec2 id;
    vec4 v = voro(q * 3.5 + gOff, 0.8, id);
    float ro = 0.3 + 0.08 * hash(id + 2.3), ri = ro * 0.5;
    float has = step(0.2, hash(id + 6.6));
    float ring = exp(-sq((v.x - (ro + ri) * 0.5) / ((ro - ri) * 0.55))) * has;
    float cup = smoothstep(ri, ri * 0.4, v.x) * has;
    float lumps = vnoise(q * 9.0 + gOff) - 0.5;
    float h = 0.028 * ring - 0.05 * cup + 0.022 * lumps * gLod;
    return vec4(h, 0.5 + 0.5 * ring - 0.3 * cup + 0.3 * lumps, mix(1.0, 0.12, cup) * mix(0.7, 1.0, max(ring, 1.0 - has)), 1.0);
}
// the tube coral's second form: knobs, packed round bumps
vec4 knobs(vec2 q) {
    vec2 id;
    vec4 v = voro(q * 4.0 + gOff, 0.7, id);
    float r = 0.4 + 0.12 * hash(id + 1.1);
    float k = sqrt(max(1.0 - sq(v.x / r), 0.0));
    float lumps = vnoise(q * 14.0 + gOff) - 0.5;
    return vec4(0.04 * k + 0.008 * lumps * gLod, mix(0.25, 1.0, k), mix(0.2, 1.0, smoothstep(0.0, 0.45, k)), 1.0);
}
vec4 plate(vec2 q, float d) {
    // one plate over the whole colony: its swell, its cups, in the colony's own coordinates
    vec2 id;
    vec2 c = q + gCol + gColOff;
    float sw = fbm(c * 1.2) + 0.5 * fbm(c * 4.5) - 0.25;
    // its raised lip along the free edge
    float lip = smoothstep(0.0, -0.025, d) * smoothstep(-0.12, -0.04, d);
    vec4 v = voro(c * 2.6, 0.8, id);
    float has = step(hash(id + 7.7), 0.4);
    float rad = 0.1 + 0.14 * hash(id + 1.3);
    float cup = smoothstep(rad, rad * 0.5, v.x) * has;
    float rim = smoothstep(rad * 1.8, rad, v.x) * (1.0 - cup) * has;
    // a spongy crust: fine pores, grains
    vec2 id2;
    vec4 w = voro(c * 10.0, 0.9, id2);
    float pore = smoothstep(0.24, 0.08, w.x) * step(0.45, hash(id2)) * gLod;
    float grain = vnoise(c * 24.0) - 0.5;
    float h = 0.022 * sw + 0.04 * lip + 0.012 * rim - 0.05 * cup - 0.012 * pore + 0.014 * grain * gLod;
    return vec4(h, 0.4 + 0.85 * lip + 0.3 * rim - 0.3 * cup + 0.5 * (sw - 0.5) - 0.3 * pore + 0.4 * grain * gLod,
                mix(1.0, 0.12, cup) * mix(0.75, 1.0, lip) * (1.0 - 0.6 * pore), 1.0);
}
vec4 tubes(vec2 q) {
    vec2 id;
    vec4 v = voro(q * 3.1 + gOff, 0.35, id);
    // each tube: a wall from its hole out to its rim, rounded on top, the dark gaps between the tubes
    float ro = 0.45 + 0.05 * hash(id + 2.3), ri = ro * (0.3 + 0.12 * hash(id + 5.1));
    float wall = smoothstep(ro + 0.03, ro - 0.05, v.x) * smoothstep(ri - 0.03, ri + 0.05, v.x);
    float x = clamp((v.x - ri) / (ro - ri), 0.0, 1.0);
    float top = sqrt(max(1.0 - sq(2.0 * x - 1.0), 0.0));
    float hole = smoothstep(ri, ri * 0.4, v.x);
    float h = 0.03 * wall * (0.6 + 0.4 * sqrt(top)) - 0.05 * hole;
    return vec4(h, mix(0.3, 1.0, wall * (0.6 + 0.4 * sqrt(top))), mix(0.35, 1.0, wall) * (1.0 - 0.85 * hole), 1.0);
}
vec4 sponge(vec2 q) {
    vec2 id, id2;
    vec2 w = vec2(vnoise(q * 3.1 + gOff), vnoise(q * 3.1 + gOff.yx + 5.2)) - 0.5;
    vec4 v = voro(q * 3.4 + w * 0.7 + gOff, 0.8, id);
    float rad = 0.24 + 0.2 * hash(id + 4.4);
    float hole = smoothstep(rad, rad * 0.5, v.x) * step(0.1, hash(id + 9.1));
    vec4 p = voro(q * 11.0 + gOff, 0.9, id2);
    float pore = smoothstep(0.22, 0.06, p.x) * step(0.4, hash(id2)) * gLod;
    float sw = fbm(q * 2.2 + gOff);
    float h = 0.03 * sw - 0.07 * hole - 0.01 * pore;
    return vec4(h, 0.75 + 0.35 * (sw - 0.5) - 0.35 * hole - 0.25 * pore, mix(1.0 - 0.5 * pore, 0.16, hole), 1.0);
}
vec4 brain(vec2 q) {
    // the grooves: oriented waves (a Gabor noise) laid round a few jittered kernels, their direction a slow field,
    // so they run on in long parallel meanders, turning, forking and closing where two kernels meet
    vec2 p = q * 2.2 + gOff;
    vec2 pw = p + (vec2(vnoise(p * 0.9 + 2.0), vnoise(p * 0.9 + 7.0)) - 0.5) * 0.7;
    vec2 i = floor(p), f = fract(p);
    float g = 0.0, wsum = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 c = vec2(x, y);
            vec2 h = hash2(i + c);
            vec2 r = f - c - h;
            float w = exp(-dot(r, r) * 1.6);
            float a = vnoise((i + c + h) * 0.35) * 7.0;
            // (in the block's own frame: neighbouring kernels of a like direction lay the same grooves)
            g += w * cos(dot(pw, vec2(cos(a), sin(a))) * 6.2832 * 4.0);
            wsum += w * w;
        }
    g /= sqrt(wsum) + 1e-4;
    float fade = gLod;
    // rounded ridges, narrow grooves
    float ridge = mix(0.7, sqrt(clamp((g + 0.75) / 1.6, 0.0, 1.0)), fade);
    float h = 0.03 * ridge;
    return vec4(h, 0.3 + 0.7 * ridge, mix(0.15, 1.0, ridge), 1.0);
}

// the block's whole surface: its species' relief over the chunk's rounded swell
vec4 surface(int sp, vec2 q, out float d) {
    bool flat_ = sp == 3;
    float shared = 0.535, free = flat_ ? 0.43 : 0.465;
    float wob = flat_ ? 0.26 : 0.04, wf = flat_ ? 1.8 : 5.0;
    d = outline(q, shared + wob * 0.6, free, 0.18, wob, wf);
    // the swell: a rounded cushion, its shoulders falling off to its free edges (its shared sides far out: a soft
    // valley between two blocks of a colony; none on the plate)
    float R = flat_ ? 0.06 : 0.2;
    float dd = outline(q, flat_ ? 1.4 : 0.535, free, 0.22, wob, wf);
    float t = clamp(-dd / R, 0.0, 1.0);
    float dome = sqrt(1.0 - sq(1.0 - t));
    // and the whole block bulging a little, a pillow
    float bulge = flat_ ? 0.0 : sqrt(clamp(-dd / 0.45, 0.0, 1.0)) * 0.09;
    vec4 s;
    if (sp == 0) s = staghorn(q, d);
    else if (sp == 1) s = star(q);
    else if (sp == 2) s = gAlt ? cups(q) : porous(q);
    else if (sp == 3) s = plate(q, dd);
    else if (sp == 4) s = gAlt ? knobs(q) : tubes(q);
    else if (sp == 5) s = sponge(q);
    else s = brain(q);
    s.x = s.x * mix(0.3, 1.0, gLod) * mix(0.4, 1.0, t) + (dome * R + bulge) * (sp == 0 ? 0.4 : 1.0);
    // a fine sugary grain over it all
    s.x += gLod * 0.005 * (vnoise((flat_ ? q + gCol + gColOff : q + gOff.yx) * 62.0) - 0.5);
    // its rim rolling away into the shade, all round
    s.z *= mix(0.22, 1.0, pow(clamp(-dd / (flat_ ? 0.1 : 0.36), 0.0, 1.0), 0.7));
    if (sp != 0) s.w = 1.0 - smoothstep(-gAA, gAA, d);
    return s;
}

void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    int code = int(vA.w + 0.5);
    gNb = code & 15;
    int sp = (code >> 4) & 7;
    int place = code >> 7;
    gCol = vec2(float(place & 7), float(place >> 3));
    gAA = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    gLod = 1.0 - smoothstep(0.045, 0.1, gAA);
    gOff = hash2(vec2(vC.w * 91.7, vC.w * 13.3)) * 40.0 + 3.0;
    gColOff = hash2(vec2(vC.z * 57.3, vC.z * 21.1)) * 40.0 + 7.0;
    gAlt = fract(vC.z * 7.31) > 0.5;
    float shrink = 1.0 - 0.7 * smoothstep(0.0, 1.0, erase);
    vec3 t = vB.rgb;
    if (uPass < 0.5) {
        // the contact shadow (or the night's glow) under a block
        if (mode != 0) discard;
        vec2 q = (vLocal - uHaloOff) / shrink;
        float d = sdBox(q, vec2(0.47 - 0.13)) - 0.13;
        float a = (1.0 - smoothstep(-0.06, 0.2, d)) * uHaloA * vB.a * (1.0 - smoothstep(0.3, 1.0, erase)) * (1.0 - uBridge);
        vec3 c = mix(uHaloC, t * 0.8, uGlow);
        frag = vec4(c * a, a);
        return;
    }
    vec2 q = vLocal / shrink;
    vec3 L = normalize(vec3(-0.22, 0.6, 0.77));
    vec3 lightC = mix(vec3(1.0), uLightC, 0.25);
    if (mode == 1) {
        // the ghost: a thin crust of coral along each of its cells' outlines (two cells share theirs)
        float rd = abs(sdBox(q, vec2(0.5 - 0.12)) - 0.12);
        float g1 = vnoise(q * 24.0 + gOff), g2 = vnoise(q * 9.0 + gOff.yx);
        float w = 0.072 + 0.035 * (g1 - 0.5) + 0.035 * (g2 - 0.5);
        float ring = 1.0 - smoothstep(w - gAA, w + gAA, rd);
        if (ring <= 0.003) discard;
        // its grains lit from above
        float e = max(gAA, 0.004);
        float hh = 0.6 * sqrt(clamp(1.0 - rd / w, 0.0, 1.0)) + 0.25 * vnoise(q * 40.0 + gOff);
        float hx = 0.6 * sqrt(clamp(1.0 - abs(sdBox(q + vec2(e, 0.0), vec2(0.38)) - 0.12) / w, 0.0, 1.0)) + 0.25 * vnoise((q + vec2(e, 0.0)) * 40.0 + gOff);
        float hy = 0.6 * sqrt(clamp(1.0 - abs(sdBox(q + vec2(0.0, e), vec2(0.38)) - 0.12) / w, 0.0, 1.0)) + 0.25 * vnoise((q + vec2(0.0, e)) * 40.0 + gOff);
        vec3 n = normalize(vec3(-(hx - hh) / e * 0.012, -(hy - hh) / e * 0.012, 1.0));
        vec3 alb = mix(t, vec3(0.72, 0.46, 0.3), 0.5) * (0.7 + 0.6 * vnoise(q * 64.0 + gOff));
        vec3 c = alb * (0.45 + 0.75 * clamp((dot(n, L) + 0.3) / 1.3, 0.0, 1.0)) * lightC;
        c += t * uGlow * 0.25;
        c = mix(c, uBridgeInk, uBridge);
        float a = ring * vB.a;
        frag = vec4(c * a, a);
        return;
    }
    // the relief, three samples a pixel apart: its normal
    float d, dx_, dy_;
    float e = max(gAA * 0.85, 0.002);
    vec4 s = surface(sp, q, d);
    if (s.w <= 0.002) discard;
    vec4 sx = surface(sp, q + vec2(e, 0.0), dx_);
    vec4 sy = surface(sp, q + vec2(0.0, e), dy_);
    vec3 n = normalize(vec3(-(sx.x - s.x) / e, -(sy.x - s.x) / e, 1.0));
    // its tone: the species' color, a little of its own (each block, each colony, broad patches over it), lighter
    // on the tips and rims, a fine speckle
    // (the plate's, one over its colony, in the colony's coordinates)
    bool plate_ = sp == 3;
    vec2 tq = plate_ ? q + gCol : q, to = plate_ ? gColOff : gOff;
    float cs = plate_ ? 0.5 : hash(gOff), ks = vC.z;
    vec3 alb = t * (0.9 + 0.2 * cs) * mix(vec3(1.0), vec3(1.1, 0.92, 0.85), (ks - 0.5) * 0.5);
    alb *= mix(vec3(1.0), vec3(1.08, 0.94, 0.9), vnoise(tq * 2.2 + to.yx) - 0.3);
    // (the second forms in their own tones: brown cup corals, orange knobs, a peach brain, a teal sponge)
    if (gAlt) {
        if (sp == 2) alb *= vec3(0.74, 0.56, 0.48);
        else if (sp == 4) alb *= vec3(1.08, 0.86, 0.72);
        else if (sp == 6) alb *= mix(vec3(1.0), vec3(1.13, 2.4, 3.9), 0.45);
        else if (sp == 5) alb *= vec3(1.4, 1.5, 1.2);
        else if (sp == 1) alb *= vec3(1.0, 0.92, 0.8);
    }
    alb *= mix(0.5, 1.12, s.y);
    alb *= 1.0 + gLod * 0.32 * (vnoise(tq * 72.0 + to) - 0.5);
    // lit from above: the key light wrapped softly, the shade warmed by the light scattered in the tissue, the
    // pits and grooves occluded
    float nl = dot(n, L);
    float wrap = clamp((nl + 0.25) / 1.25, 0.0, 1.0);
    // (and the block's underside in the shade of its own swell)
    float ao = mix(0.1, 1.0, s.z) * (plate_ ? 1.0 : mix(0.72, 1.0, smoothstep(-0.55, 0.15, q.y)));
    alb = mix(vec3(dot(alb, vec3(0.3, 0.55, 0.15))), alb, 0.85);
    vec3 c = alb * (0.15 + 1.4 * pow(wrap, 1.6)) * lightC * ao;
    c += alb * alb * 0.6 * (1.0 - wrap) * ao * 0.5;
    c += alb * 0.22 * pow(clamp(nl, 0.0, 1.0), 8.0) * s.z;
    // a faint ripple of light drifting over the stack
    float w = sin(vWorld.x * 1.3 + vWorld.y * 0.7 + uTime * 0.8) + sin(vWorld.x * 0.6 - vWorld.y * 1.1 - uTime * 0.6);
    c *= 1.0 + uShimmer * 0.04 * smoothstep(1.2, 2.0, w);
    // at night the polyps glow
    c += t * uGlow * (0.08 + 0.3 * s.y * s.z);
    c = mix(c, mix(t, vec3(1.0), 0.6), smoothstep(0.0, 0.5, erase) * 0.6);
    c = mix(c, uBridgeInk, uBridge);
    float a = s.w * vB.a * (1.0 - smoothstep(0.45, 1.0, erase));
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- board sprites: tiny bubbles
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size
layout(location = 2) in vec4 aB; // rgb, alpha
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
void main() {
    vLocal = aPos;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
out vec4 frag;
void main() {
    float r = length(vLocal);
    vec3 c = vB.rgb;
    // a tiny bubble
    float rim = smoothstep(0.55, 0.85, r) * smoothstep(1.0, 0.85, r);
    float hi = smoothstep(0.3, 0.1, length(vLocal - vec2(-0.3, 0.35)));
    float a = (rim * 0.8 + hi) * vB.a;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- lettering
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
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount): the field as baked then (64 px, 8 px
// of reach); the fill's coverage and the halo's shaped as linear blending shaped them (light over dark, dark over
// light).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = pow(smoothstep(0.47 - w, 0.47 + w, d), 1.0 / 2.2);
    float halo = 1.0 - pow(1.0 - smoothstep(0.15, 0.45, d) * uP0.a, 1.0 / 2.2);
    float a = max(fill, halo);
    return vec4(L.ink.rgb * fill + uP0.rgb * (a - fill), a) * L.ink.a;
}
)";

// ---------------------------------------------------------------- post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.25 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.8);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace coralreef_shaders

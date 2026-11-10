#pragma once
// SKY WHALE's shaders: the world (the sky, the planet with its pond, river and sea, and space in one ray-traced pass
// at a reduced resolution with its depth; the creature, the props, the toon trees, clouds, the lights in the air, the
// aurora multisampled at the full resolution over it), a soft bloom, the stars at full resolution, then the veil behind the well, the koi-scale pieces, the
// lettering. Positions are in meters, drawn relative to the eye (uVP has the eye at its origin): the creature spans
// from a hand's width to a million kilometers.

namespace skywhale_shaders {

// Shared noise (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash1(float n) { return fract(sin(n * 127.1 + 0.37) * 43758.5453); }
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
// (no sin: it keeps its precision far out, where the planet's noises are sampled at large coordinates)
float hash3(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}
float fbm3(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise3(p); p = p * 2.07 + vec3(1.7, 9.2, 3.1); a *= 0.5; }
    return s / 0.9375;
}
vec3 rainbow(float h) { return clamp(abs(fract(h + vec3(0.0, 0.667, 0.333)) * 6.0 - 3.0) - 1.0, 0.0, 1.0); }
)";

// The world's light: uniforms shared by the world's shaders. Toon light: crisp bands, a cool tinted shade, the
// aerial perspective in color (an exponential atmosphere: nothing of it is left in space). The water: what lies
// under its surface is absorbed along its path in it (in body lengths, so the creature reads at every size), then
// the surface reflects the sky (Fresnel) and the sun's glint.
inline const char* LIGHT_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uShadowC, uSunGlow, uHazeC, uRimC;
uniform float uFogK, uAlt, uNight, uBridge, uTime;
uniform vec3 uBridgeCol;
uniform vec3 uZenithC, uHorizonC, uWaterC, uDeepC;
uniform float uWaterK, uRippleS, uRippleA, uFlowT, uReflK;
float airDensity(float h0, float h1) {
    const float H = 8500.0;
    float x0 = max(h0, 0.0) / H, x1 = max(h1, 0.0) / H;
    float dx = abs(x1 - x0);
    return dx > 1e-3 ? (exp(-min(x0, x1)) - exp(-max(x0, x1))) / dx : exp(-x0);
}
vec3 atmos(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    float hw = length(vec3(wp.x, wp.y + 6.371e6, wp.z)) - 6.371e6;
    float od = uFogK * L * airDensity(uAlt, hw);
    vec3 v = d / max(L, 1e-3);
    vec3 hc = mix(uHazeC, uSunGlow, pow(max(dot(v, uSunDir), 0.0), 6.0) * 0.45);
    col = mix(hc, col, exp(-od));
    return mix(col, uBridgeCol, uBridge);
}
// three tones: the shade (cool), the light, the full sun
float toonBand(float ndl) { return smoothstep(-0.05, 0.03, ndl) * 0.6 + smoothstep(0.42, 0.5, ndl) * 0.4; }
vec3 toonLight(float ndl, float ao) { return mix(uShadowC * mix(0.7, 1.0, ao), uSunC, toonBand(ndl)) * mix(0.82, 1.0, ao); }
// the sky seen in the water
vec3 skyRefl(vec3 r) {
    vec3 c = mix(uHorizonC, uZenithC, pow(clamp(r.y, 0.0, 1.0), 0.6));
    return c + uSunGlow * pow(max(dot(r, uSunDir), 0.0), 10.0) * 0.6 * (1.0 - 0.7 * uNight);
}
vec3 rippleN(vec2 q) {
    vec2 p = q / uRippleS + vec2(-uFlowT, uFlowT * 0.3);
    const float e = 0.2;
    float h0 = vnoise(p) + 0.5 * vnoise(p * 2.3 + 4.0), hx = vnoise(p + vec2(e, 0.0)) + 0.5 * vnoise((p + vec2(e, 0.0)) * 2.3 + 4.0);
    float hz = vnoise(p + vec2(0.0, e)) + 0.5 * vnoise((p + vec2(0.0, e)) * 2.3 + 4.0);
    return normalize(vec3(-(hx - h0) / e * uRippleA, 1.0, -(hz - h0) / e * uRippleA));
}
// The surface at s, seen along v: the sky's reflection (Fresnel, in two soft bands), the sun's glint in crisp streaks.
vec3 waterSurface(vec3 col, vec3 s, vec3 v) {
    vec3 n = rippleN(s.xz);
    float cosi = max(dot(-v, n), 0.0);
    float fr = 0.03 + 0.97 * pow(1.0 - cosi, 5.0);
    fr = mix(fr, smoothstep(0.1, 0.4, fr) * 0.6 + 0.03, 0.25) * uReflK;
    vec3 r = reflect(v, n);
    r.y = abs(r.y);
    col = mix(col, skyRefl(r), clamp(fr, 0.0, 1.0));
    float g = dot(r, uSunDir);
    col += (uSunC * 0.6 + uSunGlow) * smoothstep(0.99, 0.995, g) * 0.8 * (1.0 - 0.6 * uNight);
    return col;
}
// The height above the sea (the planet's curve, locally): the creature cruises kilometers out to sea.
float seaH(vec3 p) { return p.y + dot(p.xz, p.xz) / 1.2742e7; }
// A point under the water (below the sea) seen from above: absorbed along its path in the water, then the surface.
vec3 underWater(vec3 col, vec3 wp) {
    float he = seaH(uEye), hp = seaH(wp);
    if (he <= 0.0 || hp >= 0.0) return col;
    vec3 d = wp - uEye;
    float t0 = he / max(he - hp, 1e-6);
    vec3 s = uEye + d * t0;
    float path = length(d) * (1.0 - t0);
    col = mix(col, col * uWaterC, 1.0 - exp(-path * uWaterK * 2.0));
    col = mix(uDeepC, col, exp(-path * uWaterK));
    return waterSurface(col, s, normalize(d));
}
)";

// ---------------------------------------------------------------- the sky, the planet, the water, space
// Per pixel: the planet (a sphere under the pond: the pond, the river widening to the sea, the coast and the
// headland at local scale, with meadows in crisp two-tone patches, pebbles and caustics under the shallow water,
// lily pads; continents and oceans farther; the night side's lights), its atmosphere (a bright sky near the ground,
// deepening to space with the altitude, the limb glowing), far hills on the land's side, the sun, the moon, the
// polar aurora, and in space the nebula and a galaxy. Writes its depth (the water's floor, the land, the pads).
// Alpha: where the stars may shine.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uPix, uSunSize, uStars, uHills, uDeep, uGlow, uSpaceK, uAurora;
uniform vec3 uSpaceC, uNebulaC, uNebula2C, uAuroraC, uAurora2C;
uniform vec3 uGroundA, uGroundB, uOceanC, uLandC, uDesertC, uFlowerC, uSandC, uPadC, uLotusC;
uniform vec4 uMoon;               // center (meters), radius
uniform vec3 uGalaxyDir;
uniform float uCoastX, uHeadZ;    // the coast's line (x, meters) and the headland's place along it
uniform vec3 uWhaleA, uWhaleB;    // the creature's snout and tail (meters)
uniform float uWhaleW, uWakeK, uShadowK, uFoamBody;
uniform vec4 uSplash;             // a breach's splash: center, age (seconds)
uniform vec4 uLant[6];            // floating lanterns: position, light
uniform vec3 uLantC;
const float R = 6.371e6;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void basis(vec3 a, out vec3 x, out vec3 y) {
    x = normalize(cross(a, abs(a.y) < 0.95 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    y = cross(x, a);
}
// The local geography (the same functions as swgeo:: in skywhale.cpp): the pond, the river, the sea.
float pondR(vec2 q) { float a = atan(q.y, q.x); return 2.4 * (1.0 + 0.07 * sin(3.0 * a + 0.5) + 0.04 * sin(5.0 * a + 2.0)); }
float riverZ(float x) { return 0.16 * x * sin(0.95 * log(x + 4.0) + 0.6) * (1.0 - smoothstep(uCoastX * 0.6, uCoastX * 1.6, x)); }
float riverHW(float x) { return 0.55 + 0.17 * x; }
float coastX(float z) { return uCoastX + 14.0 * sin(z / 37.0) + 6.0 * sin(z / 11.0 + 1.0) + 70.0 * exp(-sq((z - uHeadZ) / 30.0)); }
// signed distance to the shore (meters, positive in the water), the water's depth, how calm it is
float waterE(vec2 q, out float depth, out float calm) {
    float ep = pondR(q) - length(q);
    float x = max(q.x, 0.0);
    float hw = riverHW(x);
    float er = q.x > 1.0 ? hw - abs(q.y - riverZ(x)) : -1e9;
    float es = q.x - coastX(q.y);
    float e = max(ep, max(er, es));
    depth = 0.0;
    calm = 0.0;
    if (ep > 0.0) { depth = 0.12 + 0.5 * smoothstep(0.0, 1.4, ep); calm = 1.0; }
    if (er > 0.0) { depth = max(depth, 0.08 + hw * 0.35 * smoothstep(0.0, hw * 0.7, er)); calm = max(calm, smoothstep(hw * 0.25, 0.0, er) * smoothstep(60.0, 5.0, x)); }
    if (es > 0.0) depth = max(depth, 0.3 + es * 0.4);
    return e;
}
// The ground's color at p (n: the planet's normal there, t: the distance, foot: a pixel's size there).
vec3 landColor(vec3 p, vec3 n, float t, float foot, float e) {
    float c0 = fbm3(n * 2.6 + 11.0);
    vec3 land = mix(uLandC, uDesertC, smoothstep(0.52, 0.62, fbm3(n * 5.0 + 3.0)) * 0.6);
    land *= mix(1.0, 0.62, smoothstep(0.55, 0.7, fbm3(n * 14.0 + 7.0)));   // forests
    // the meadow's patches, crisp, at every scale the pixel resolves
    float v = 0.0;
    float sc = 1.4;
    for (int i = 0; i < 7; i++) {
        float k = smoothstep(sc * 0.6, sc * 0.12, foot);
        vec2 q = p.xz / sc + float(i) * 17.3;
        v += (vnoise(q) - 0.5) * k * (i < 2 ? 0.8 : 1.0);
        sc *= 7.0;
    }
    float tone = smoothstep(-0.03, 0.03, v);
    vec3 meadow = mix(uGroundB, uGroundA, tone);
    float kNear = 1.0 - smoothstep(6.0e3, 8.0e4, length(p.xz));
    // (the patches are mapped on the plane: none at the planet's scale, where they would streak)
    land = mix(land * mix(0.85, 1.12, mix(0.5, tone, 1.0 - smoothstep(1.0e5, 6.0e5, length(p.xz)))), meadow, kNear);
    // the shore: a band of sand and pebbles along the water
    float band = max(0.35, 0.05 * max(p.x, 4.0));
    float sand = smoothstep(-band - foot, -band * 0.6, e) * kNear;
    land = mix(land, uSandC * (0.9 + 0.2 * vnoise(p.xz / max(band * 0.3, foot * 2.0))), sand);
    // snow on the far mountains and the poles
    float snow = smoothstep(0.82, 0.9, abs(n.z) + 0.1 * fbm3(n * 9.0)) + smoothstep(0.68, 0.72, c0) * 0.8;
    land = mix(land, vec3(0.97, 0.98, 1.0), clamp(snow, 0.0, 1.0) * (1.0 - kNear));
    // flowers in the near grass
    if (foot < 0.02 && e < -0.4) {
        vec2 cell = floor(p.xz / 0.16);
        vec2 o = (cell + 0.25 + 0.5 * hash2(cell)) * 0.16;
        float h = hash(cell + 3.7);
        float fl = step(h, 0.05) * smoothstep(0.035, 0.022, length(p.xz - o));
        vec3 fc = h < 0.02 ? vec3(1.0, 0.95, 0.4) : h < 0.035 ? uFlowerC : vec3(1.0);
        land = mix(land, fc, fl * smoothstep(0.02, 0.008, foot));
    }
    return land;
}
// the shallow floor: sand, pebbles, caustics, the creature's soft shadow
vec3 floorColor(vec3 fp, float foot, float depth) {
    vec2 q = fp.xz / 0.09;
    vec2 ci = floor(q);
    vec2 f = fract(q) - 0.5 - (hash2(ci) - 0.5) * 0.4;
    float stone = smoothstep(0.42, 0.3, length(f * vec2(1.0, 1.3))) * step(0.35, hash(ci + 2.0)) * smoothstep(0.05, 0.01, foot);
    vec3 c = mix(uSandC * 0.75, mix(uSandC * 0.55, vec3(0.55, 0.5, 0.5), hash(ci + 5.0)), stone);
    // caustics: a net of light lines drifting
    vec2 cq = fp.xz / max(0.35, depth * 1.2) + vec2(uFlowT * 0.4, -uFlowT * 0.25);
    float cn = abs(vnoise(cq) + vnoise(cq * 1.7 + 3.0) * 0.5 - 0.75);
    c += uSunC * smoothstep(0.06, 0.0, cn) * 0.35 * smoothstep(0.2 * max(depth, 0.1), 0.0, foot) * (1.0 - uNight * 0.6);
    // the creature's shadow, under it
    if (uShadowK > 0.0) {
        vec2 a = uWhaleA.xz, b = uWhaleB.xz, pq = fp.xz - b, ab = a - b;
        float h = clamp(dot(pq, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
        float dd = length(pq - ab * h);
        float w = uWhaleW * (0.35 + 0.65 * sin(3.14159 * clamp(1.0 - h * 0.9, 0.0, 1.0)));
        c *= 1.0 - 0.45 * uShadowK * smoothstep(w * 1.6, w * 0.6, dd);
    }
    return c;
}
// The sky along d (the atmosphere along the ray, its lowest point; space beyond; the aurora, the limb, far hills).
vec3 skyColor(vec3 d, float b, float disc, float alt, vec3 up, inout float stars) {
    vec3 col;
    // ---- the sky: the atmosphere along the ray (its lowest point), space beyond
    float hc = b < 0.0 ? -disc / (sqrt(R * R - disc) + R) : alt;
    float air = exp(-hc / 18000.0);
    float dip = atan(sqrt(alt * (2.0 * R + alt)), R);
    float el = asin(clamp(dot(d, up), -1.0, 1.0)) + dip;   // above the limb
    float zk = smoothstep(0.0, 0.75, el);
    vec3 zen = mix(uZenithC, uZenithC * vec3(0.25, 0.3, 0.55), smoothstep(2000.0, 30000.0, alt));
    vec3 sky = mix(uHorizonC, zen, pow(zk, 0.7));
    float a = clamp(pow(air, 0.55) * 1.15, 0.0, 1.0);
    vec3 sp = uSpaceC;
    float az = atan(d.x, -d.z);
    float nb = fbm3(d * 2.2 + 4.0);
    float neb = smoothstep(0.45, 0.8, nb) * (0.35 + 0.65 * smoothstep(0.3, 0.9, fbm3(d * 5.0 + 1.0)));
    sp += mix(uNebulaC, uNebula2C, fbm3(d * 3.7 + 9.0)) * neb * (0.08 + 0.5 * uDeep);
    if (uDeep > 0.0) {
        vec3 bx, by;
        basis(uGalaxyDir, bx, by);
        vec2 g = vec2(dot(d, bx), dot(d, by)) / 0.22;
        if (dot(d, uGalaxyDir) > 0.0) {
            vec2 gq = vec2(g.x * 0.9 + g.y * 0.43, (-g.x * 0.43 + g.y * 0.9) / 0.45);
            float gr = length(gq), ga = atan(gq.y, gq.x);
            float arms = 0.5 + 0.5 * cos(2.0 * (ga - log(gr + 0.05) * 2.6));
            float gb = exp(-gr * 2.6) * (0.35 + 0.65 * arms * smoothstep(0.0, 0.25, gr)) + exp(-gr * 14.0) * 1.2;
            gb *= 0.75 + 0.5 * vnoise(gq * 14.0);
            sp += mix(uNebula2C * 1.3, vec3(1.0, 0.92, 0.8), exp(-gr * 5.0)) * gb * uDeep * 0.9;
        }
    }
    col = mix(sp, sky, a);
    // the polar aurora, in curtains over the sky (the aurora variant)
    if (uAurora > 0.0 && el > 0.0) {
        float x = az * 2.2 + 0.5 * sin(az * 3.0 + uTime * 0.05);
        float curtain = smoothstep(0.25, 0.75, fbm(vec2(x * 1.3, uTime * 0.03))) ;
        float rays = 0.55 + 0.45 * vnoise(vec2(az * 70.0, uTime * 0.25));
        float h = (el - 0.06 - 0.08 * fbm(vec2(x * 2.0, 1.0))) / 0.45;
        float body = smoothstep(0.0, 0.06, h) * exp(-max(h, 0.0) * 2.4);
        vec3 ac = mix(uAuroraC, uAurora2C, smoothstep(0.1, 0.9, h));
        col += ac * body * curtain * rays * uAurora * a * 0.9;
        stars *= 1.0 - 0.6 * body * curtain;
    }
    // the limb, glowing (seen from high up)
    float limb = exp(-hc / 6000.0) * smoothstep(3000.0, 60000.0, alt) * step(0.0, -b + 0.0);
    col += uHorizonC * limb * 0.7;
    stars += (uStars + (1.0 - a)) * (1.0 - a * 0.85);
    // far hills along the horizon, on the land's side (seen from near the ground)
    if (uHills > 0.0) {
        float side = smoothstep(0.35, -0.25, d.x);
        float h1 = 0.012 + 0.022 * fbm(vec2(az * 2.6, 1.0)) + 0.012 * fbm(vec2(az * 9.0, 7.0));
        float h2 = 0.004 + 0.016 * fbm(vec2(az * 4.3 + 9.0, 3.0)) + 0.006 * fbm(vec2(az * 17.0, 2.0));
        float aa = uPix * 1.2;
        float m1 = smoothstep(h1 + aa, h1 - aa, el) * uHills * side, m2 = smoothstep(h2 + aa, h2 - aa, el) * uHills * side;
        vec3 far = mix(uHazeC, uGroundB * 0.9 + uZenithC * 0.25, 0.4);
        vec3 near = mix(uHazeC, uGroundB, 0.62);
        col = mix(col, far, m1);
        col = mix(col, near, m2);
        stars *= 1.0 - max(m1, m2);
    }
    return col;
}
void main() {
    vec3 d = rayDir(vUv);
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float c0 = dot(uEye.xz, uEye.xz) + uEye.y * (uEye.y + 2.0 * R);   // |oc|^2 - R^2, without cancellation
    float alt = c0 / (sqrt(c0 + R * R) + R);
    vec3 up = normalize(oc);
    float b = dot(d, oc);
    float disc = b * b - c0;
    vec3 col;
    float stars = 0.0;
    float depth = 1.0;
    bool hit = false;
    float sd = dot(d, uSunDir);
    if (disc > 0.0 && b < 0.0) {
        // ---- the planet
        hit = true;
        float t = c0 / (-b + sqrt(disc));
        vec3 p = uEye + d * t;
        vec3 n = normalize(oc + d * t);
        float foot = t * uPix;
        float ndl = dot(n, uSunDir);
        float lit = smoothstep(-0.08, 0.12, ndl);
        float band = smoothstep(-0.05, 0.03, ndl) * 0.55 + smoothstep(0.35, 0.45, ndl) * 0.45;
        vec3 light = mix(uShadowC, uSunC, band);
        // land or water: the local geography near the pond, the continents farther
        float dist0 = length(p.xz);
        float kLocal = 1.0 - smoothstep(3.0e4, 8.0e4, dist0);
        float wdepth, calm;
        float e = waterE(p.xz, wdepth, calm);
        float cont = fbm3(n * 2.6 + 11.0) - 0.75 * smoothstep(-2.0e5, 2.0e5, p.x - uCoastX) * (1.0 - smoothstep(1.5e6, 3.0e6, dist0));
        float cw = max(foot / 3.0e6, 0.004);
        float wFar = 1.0 - smoothstep(0.47 - cw, 0.47 + cw, cont);
        float aa = max(foot * 0.7, 1e-4);
        float water = mix(wFar, smoothstep(-aa, aa, e), kLocal);
        vec3 land = landColor(p, n, t, foot, mix(-1e3, e, kLocal)) * light;
        vec3 wcol;
        vec3 fp = p;
        {
            // under the water: the shallow floor (pebbles, caustics, shadow), the deep; then the surface
            float dep = mix(1e5, max(wdepth, 0.02), kLocal);
            float cosv = max(-dot(d, n), 0.03);
            fp = p + d * (dep / cosv);
            vec3 fl = floorColor(fp, foot, dep) * light;
            vec3 under = mix(uDeepC, mix(fl, fl * uWaterC, 1.0 - exp(-(dep / cosv) * uWaterK * 2.0)), exp(-(dep / cosv) * uWaterK));
            // far: the ocean seen from high up (its own blue, the glint)
            float hi = smoothstep(800.0, 2.0e4, alt);
            vec3 ocean = mix(uOceanC, uOceanC * vec3(0.55, 0.75, 1.0), smoothstep(0.47, 0.3, cont)) * light;
            ocean = mix(ocean, ocean * vec3(0.7, 1.15, 1.1) + vec3(0.0, 0.03, 0.03), smoothstep(-200.0, 0.0, -(p.x - coastX(p.z)) - 300.0) * kLocal);
            wcol = waterSurface(under, p, d);
            wcol = mix(wcol, ocean + uSunGlow * pow(max(dot(reflect(d, n), uSunDir), 0.0), 40.0) * 0.6 * lit, hi);
            // floating lanterns: their light on the water, in long reflections
            for (int i = 0; i < 6; i++) {
                if (uLant[i].w <= 0.0) continue;
                vec3 m = vec3(uLant[i].x, -uLant[i].y, uLant[i].z);
                vec3 lv = m - uEye;
                float tq = uEye.y / max(uEye.y - m.y, 1e-4);
                vec3 Q = uEye + lv * tq;
                vec2 dq = p.xz - Q.xz;
                vec2 ax = normalize(Q.xz - uEye.xz + 1e-5);
                float al = dot(dq, ax), pe = dot(dq, vec2(-ax.y, ax.x));
                float r = 0.06 + 0.02 * vnoise(vec2(al * 30.0, uTime * 2.0));
                wcol += uLantC * uLant[i].w * exp(-sq(pe / (r * 0.6)) - sq(al / (r * 4.0))) * 0.45;
                wcol += uLantC * uLant[i].w * exp(-dot(p.xz - uLant[i].xz, p.xz - uLant[i].xz) / 0.03) * 0.2;
            }
            // foam: the wake of the creature (a V of crests and a broken trail), a breach's splash ring
            if (uWakeK > 0.0) {
                vec2 hd = normalize(uWhaleA.xz - uWhaleB.xz + 1e-6);
                vec2 rel = p.xz - uWhaleB.xz;
                float back = -dot(rel, hd), side = abs(dot(rel, vec2(-hd.y, hd.x)));
                float Lw = length(uWhaleA - uWhaleB);
                float trail = smoothstep(-0.1 * Lw, 0.2 * Lw, back) * smoothstep(5.0 * Lw, 1.0 * Lw, back);
                float bw = uWhaleW * 1.2 + back * 0.12;
                float fn = vnoise(rel / max(uWhaleW * 0.25, foot * 2.0) + vec2(uFlowT * 0.3, 0.0));
                float ff = fn + 0.25 * smoothstep(bw, 0.0, side);
                float fw = max(fwidth(ff), 0.025);
                float foam = smoothstep(bw, bw * 0.5, side) * smoothstep(0.575 - fw, 0.575 + fw, ff) * trail;
                // the Kelvin V, from the snout
                vec2 relA = p.xz - uWhaleA.xz;
                float bA = -dot(relA, hd), sA = abs(dot(relA, vec2(-hd.y, hd.x)));
                float vline = abs(sA - bA * 0.36) / max(uWhaleW * 0.25 + bA * 0.02, foot);
                foam = max(foam, smoothstep(1.0, 0.4, vline) * smoothstep(0.0, 0.2 * Lw, bA) * smoothstep(2.5 * Lw, 0.3 * Lw, bA) * smoothstep(0.475 - fw - 0.05, 0.475 + fw + 0.05, fn) * 0.7);
                // around the body where it meets the water
                vec2 ab = uWhaleA.xz - uWhaleB.xz, pq = p.xz - uWhaleB.xz;
                float hh = clamp(dot(pq, ab) / max(dot(ab, ab), 1e-6), 0.0, 1.0);
                float dd = length(pq - ab * hh);
                foam = max(foam, smoothstep(uWhaleW * 1.7, uWhaleW * 1.0, dd) * smoothstep(0.425 - fw - 0.05, 0.425 + fw + 0.05, fn) * 0.8 * uFoamBody);
                wcol = mix(wcol, vec3(1.0, 0.99, 0.97) * mix(light, vec3(1.0), 0.3), foam * uWakeK * 0.85);
            }
            if (uSplash.w >= 0.0 && uSplash.w < 6.0) {
                float rr = length(p.xz - uSplash.xz);
                float R0 = uWhaleW * (1.5 + uSplash.w * 2.2);
                float ring = smoothstep(uWhaleW * 0.5, 0.0, abs(rr - R0)) * smoothstep(6.0, 1.0, uSplash.w);
                float fn = vnoise(p.xz / max(uWhaleW * 0.2, foot * 2.0));
                wcol = mix(wcol, vec3(1.0) * mix(light, vec3(1.0), 0.3), ring * step(0.4, fn) * 0.8);
            }
        }
        col = mix(land, wcol, water);
        // lily pads on the calm water, a lotus here and there (they hide what swims under them)
        if (water > 0.5 && calm > 0.0 && foot < 0.03 && kLocal > 0.5) {
            vec2 q = p.xz / 0.3;
            vec2 ci = floor(q);
            vec2 o = (ci + 0.3 + 0.4 * hash2(ci + 1.3));
            float hpad = hash(ci + 8.1);
            float rp = (0.06 + 0.05 * hash(ci + 3.3)) / 0.3;
            vec2 fq = q - o;
            float rr = length(fq);
            float ang = atan(fq.y, fq.x) - hash(ci + 4.4) * 6.283;
            float notch = smoothstep(0.0, 0.18, abs(mod(ang + 3.14159, 6.28318) - 3.14159));
            float on = step(hpad, 0.5 * calm);
            float pad = on * smoothstep(rp, rp * 0.92, rr) * notch * smoothstep(0.15, 0.4, e);
            if (pad > 0.01) {
                float vein = smoothstep(0.04, 0.0, abs(fract(atan(fq.y, fq.x) * 2.5) - 0.5) - 0.42);
                vec3 pc = uPadC * mix(0.85, 1.12, smoothstep(rp * 0.2, rp, rr)) * (1.0 - 0.12 * vein);
                pc = mix(pc, uPadC * 1.35 + 0.05, smoothstep(rp * 0.75, rp * 0.95, rr) * 0.5);
                float lotus = step(hash(ci + 6.6), 0.22) * smoothstep(0.33, 0.25, rr / rp);
                float pa = atan(fq.y, fq.x);
                float petal = smoothstep(0.5, 0.35, rr / rp / (0.7 + 0.3 * abs(cos(pa * 4.0))));
                pc = mix(pc, mix(uLotusC, vec3(1.0), smoothstep(0.3, 0.0, rr / rp) * 0.5), lotus * petal);
                pc = mix(pc, vec3(1.0, 0.85, 0.35), lotus * smoothstep(0.08, 0.04, rr / rp));
                col = mix(col, pc * light, pad);
                if (pad > 0.5) fp = p;
            }
        }
        // the night side: dark blue, the cities' lights
        float night = 1.0 - lit;
        col = mix(col, col * uShadowC * 0.3, night * smoothstep(1e4, 2e5, alt));
        if (foot > 800.0) {
            vec3 cn = n * 900.0;
            float city = step(0.78, vnoise3(cn)) * step(0.6, vnoise3(cn * 3.1 + 5.0)) * (1.0 - water);
            col += vec3(1.0, 0.8, 0.45) * city * night * smoothstep(1e5, 6e5, alt) * 0.9;
        }
        // the air between: the aerial perspective, the limb's glow
        float od = 1.0e-5 * t * airDensity(alt, 0.0);
        vec3 hc = mix(mix(uHazeC, uZenithC, 0.3), uSunGlow, pow(max(sd, 0.0), 6.0) * 0.5);
        col = mix(hc * mix(1.0, 0.25, night * smoothstep(1e4, 3e5, alt)), col, exp(-od));
        // depth: the floor under the water (so what swims there shows), else the surface
        vec3 dp = water > 0.5 ? fp : p;
        vec4 clip = uVP * vec4(dp - uEye, 1.0);
        depth = water > 0.5 && length(dp - uEye) > 5.0e4 ? 1.0 : clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
        // the limb antialiased: within a pixel and a half of it, toward the sky behind
        float gap = disc / (R + sqrt(max(R * R - disc, 0.0)));
        float wl = 1.0 - smoothstep(0.0, 1.6 * uPix * sqrt(max(c0, 1.0)), gap);
        if (wl > 0.002) {
            float st = 0.0;
            col = mix(col, skyColor(d, b, 0.0, alt, up, st), wl);
        }
    } else {
        col = skyColor(d, b, disc, alt, up, stars);
    }
    // the sun: a crisp disc, a halo
    {
        float ang = acos(clamp(sd, -1.0, 1.0));
        float disc = smoothstep(uSunSize * 1.04, uSunSize * 0.96, ang) * (1.0 - uNight);
        float glow = pow(max(sd, 0.0), 8.0) * 0.35 + exp(-ang / max(uSunSize * 3.0, 1e-3)) * 0.6;
        float ring = exp(-sq((ang - uSunSize * 2.2) / (uSunSize * 0.25))) * 0.12;
        col += uSunGlow * (glow + ring) * (1.0 + 0.6 * uGlow) * (1.0 - uNight * 0.8) * (hit ? 0.3 : 1.0);
        col = mix(col, uSunC * 2.2 + 0.4, disc * (hit ? 0.0 : 1.0));
        stars *= 1.0 - clamp(glow * 2.0 + disc, 0.0, 1.0);
    }
    // the moon: a sphere, its terminator crisp, its craters
    if (!hit) {
        vec3 mo = uEye - uMoon.xyz;
        float mb = dot(d, mo);
        float md = mb * mb - (dot(mo, mo) - uMoon.w * uMoon.w);
        if (md > 0.0 && mb < 0.0) {
            float t = -mb - sqrt(md);
            vec3 mn = normalize(mo + d * t);
            float cr = fbm3(mn * 3.0 + 2.0);
            float crater = 0.0;
            for (int i = 0; i < 3; i++) {
                vec3 cp = mn * (4.0 + 5.0 * float(i));
                vec3 ci = floor(cp);
                vec3 cf = fract(cp) - 0.5 - (vec3(hash3(ci), hash3(ci + 3.1), hash3(ci + 7.7)) - 0.5) * 0.5;
                float rr = length(cf);
                float s = 0.18 + 0.14 * hash3(ci + 1.7);
                crater += smoothstep(s, s * 0.7, rr) * 0.35 - smoothstep(s * 1.15, s, rr) * smoothstep(s * 0.85, s, rr) * 0.25;
            }
            vec3 alb = mix(vec3(0.93, 0.92, 1.0), vec3(0.62, 0.62, 0.78), smoothstep(0.5, 0.62, cr)) * (1.0 - crater * 0.6);
            float ml = smoothstep(-0.03, 0.03, dot(mn, uSunDir));
            vec3 mc = alb * mix(vec3(0.12, 0.13, 0.25), vec3(1.15, 1.12, 1.05), ml);
            float edge = smoothstep(0.0, 0.08, -dot(mn, d));
            float a = clamp(pow(exp(-alt / 9000.0), 0.55), 0.0, 1.0);
            col = mix(col, mc + col * 0.35 * a, edge * (1.0 - 0.55 * a));
            stars = 0.0;
        }
    }
    col = mix(col, uBridgeCol, uBridge);
    gl_FragDepth = depth;
    frag = vec4(col, clamp(stars, 0.0, 1.0) * (1.0 - uBridge) * (hit ? 0.0 : 1.0));
}
)";


// ---------------------------------------------------------------- the finale: the Moon's landscape
// Ray-marched per pixel (at the sky's reduced resolution) over the Moon's sphere, in Moon radii: grey regolith,
// darker maria, mountain ranges (ridged), craters at six scales (bowls, raised rims, ejecta blankets, central peaks in
// the great ones, the young ones bright with rays), rilles; a low sun, hard shadows marched toward it, the creature's
// own shadow on the ground; the black sky with the Earth hanging in it. Writes its depth.
inline const char* LUNAR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uPix;
uniform vec3 uLC;          // the Moon's center - the eye (meters)
uniform float uRL;         // its radius (meters)
uniform vec3 uEarthDir;
uniform float uEarthR;     // the Earth's angular radius
uniform float uEarthK;     // the Earth drawn (the finale)
uniform float uMoonK;      // the Moon's opacity
uniform float uDay;        // in the day's sky: a pale moon, its dark side lost in the sky's light
uniform vec3 uSpaceC, uNebulaC, uRegA, uRegB;
uniform vec3 uWhaleA, uWhaleB;
uniform float uWhaleW;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void basisL(vec3 a, out vec3 x, out vec3 y) {
    x = normalize(cross(a, abs(a.y) < 0.95 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    y = cross(x, a);
}
// The ground's height (Moon radii) at the direction n; alb: its albedo's offset. foot: a pixel's size there (the
// detail it resolves); lo: the cheap form for the marches.
float terrain(vec3 n, float foot, bool lo, out float alb) {
    alb = 0.0;
    float big = lo ? vnoise3(n * 2.3 + 5.0) : fbm3(n * 2.3 + 5.0);
    float maria = smoothstep(0.5, 0.4, big);
    float h = (big - 0.5) * 0.006;
    // mountain ranges: ridged, in the highlands
    float rg = 1.0 - abs(vnoise3(n * 7.0 + 2.0) * 2.0 - 1.0);
    float hl = smoothstep(0.45, 0.62, big);
    h += pow(rg, 3.0) * 0.0045 * hl;
    if (!lo) {
        float rg2 = 1.0 - abs(vnoise3(n * 26.0 + 7.0) * 2.0 - 1.0);
        h += pow(rg2, 3.0) * 0.0012 * (1.0 - 0.7 * maria) * smoothstep(0.02, 0.004, foot);
        alb += 0.04 * (vnoise3(n * 60.0) - 0.5) * smoothstep(0.004, 0.001, foot);
    }
    alb -= 0.13 * maria;
    float cell = 0.085;
    for (int i = 0; i < 6; i++) {
        if (lo && i >= 3) break;
        float k = smoothstep(cell * 0.6, cell * 0.15, foot);
        if (k <= 0.0) break;
        vec3 P = n / cell;
        vec3 ci = floor(P);
        if (hash3(ci + 1.7) < (i < 3 ? 0.6 : 0.62 - 0.1 * float(i))) {
            vec3 o = ci + 0.5 + (vec3(hash3(ci), hash3(ci + 3.1), hash3(ci + 7.7)) - 0.5) * 0.3;
            vec3 f = P - o;
            float R = 0.12 + 0.24 * pow(hash3(ci + 5.3), 1.5);
            float x = length(f) / R;
            if (x < 2.7) {
                float dr = i < 2 ? 0.09 : 0.16;
                float prof = x < 1.0 ? (x * x - 1.0) : 0.0;
                prof += 0.32 * exp(-sq((x - 1.0) / 0.17));
                prof += 0.14 * smoothstep(0.85, 1.05, x) * exp(-max(x - 1.0, 0.0) * 2.0);
                if (i < 3) prof += 0.5 * exp(-sq(x / 0.15)) * (1.0 - 0.5 * hash3(ci + 2.2));
                h += prof * R * cell * dr * 2.0 * k;
                if (!lo && hash3(ci + 9.9) < 0.3) {
                    vec3 fd = normalize(f + 1e-5);
                    float ray = pow(vnoise3(fd * 9.0 + ci * 0.37), 3.0) * 2.0;
                    alb += (ray * exp(-max(x - 1.0, 0.0) * 1.1) * smoothstep(0.95, 1.1, x) + 0.1 * step(x, 1.0)) * 0.3 * k;
                }
            }
        }
        cell *= 0.42;
    }
    if (!lo) {
        float rl = abs(vnoise3(n * 16.0 + 31.0) - 0.5);
        h -= 0.0011 * smoothstep(0.03, 0.0, rl) * smoothstep(0.55, 0.65, vnoise3(n * 3.0 + 9.0)) * smoothstep(0.01, 0.003, foot);
    }
    return h;
}
void main() {
    vec3 d = rayDir(vUv);
    vec3 o = -uLC / uRL;
    float b = dot(o, d);
    const float HM = 0.03;
    float c = dot(o, o) - sq(1.0 + HM);
    float disc = b * b - c;
    bool hit = false;
    float t = 0.0;
    float pixA = uPix;
    float gBest = 1e9, tBest = 0.0;   // the nearest miss (in pixels): the rim's coverage
    if (disc > 0.0) {
        float sq_ = sqrt(disc);
        float t1 = -b + sq_;
        t = max(0.0, -b - sq_);
        for (int i = 0; i < 72; i++) {
            vec3 p = o + d * t;
            float r = length(p);
            float a;
            float hh = terrain(p / r, t * pixA * 3.0, true, a);
            float gap = r - (1.0 + hh);
            if (gap < 0.0004 * t + 1e-6) { hit = true; break; }
            float gp = gap / max(t * pixA, 1e-9);
            if (gp < gBest) { gBest = gp; tBest = t; }
            t += max(gap * 0.6, 1e-5 + t * 0.001);
            if (t > t1) break;
        }
    }
    vec3 col = vec3(0.0);
    float depth = 1.0;
    // a ray passing within a pixel and a half of the ground: the rim, partly covered (antialiased)
    float cov = hit ? 1.0 : 1.0 - smoothstep(0.0, 1.5, gBest);
    if (!hit && cov > 0.0) t = tBest;
    if (cov > 0.0) {
        vec3 p = o + d * t;
        vec3 n = normalize(p);
        float foot = t * pixA;
        vec3 e1, e2;
        basisL(n, e1, e2);
        float eps = max(foot * 1.5, 1.5e-5);
        float a0, a1, a2;
        float h0 = terrain(n, foot, false, a0);
        vec3 n1 = normalize(n + e1 * eps), n2 = normalize(n + e2 * eps);
        float h1 = terrain(n1, foot, false, a1), h2 = terrain(n2, foot, false, a2);
        vec3 P0 = n * (1.0 + h0), P1 = n1 * (1.0 + h1), P2 = n2 * (1.0 + h2);
        vec3 N = normalize(cross(P1 - P0, P2 - P0));
        if (dot(N, n) < 0.0) N = -N;
        // the regolith: grey, darker maria, bright young craters
        vec3 alb = mix(uRegB, uRegA, clamp(0.55 + a0 * 2.2, 0.0, 1.0));
        // the low sun: crisp light, hard shadows marched toward it
        // near: crisp light, hard shadows; from afar a softer light (its relief would be ink blots), and its phase
        // gibbous (lit from nearly behind the eye: a bright disc, never a dark spot)
        float nearK = smoothstep(0.4, 0.06, length(o) - 1.0);
        vec3 sunM = normalize(mix(uSunDir, -d, 0.6 * (1.0 - nearK)));
        float ndl = dot(N, sunM);
        float lit = mix(smoothstep(-0.25, 0.7, ndl), smoothstep(-0.02, 0.06, ndl) * (0.4 + 0.6 * clamp(ndl * 2.2, 0.0, 1.0)), nearK);
        float sh = 1.0;
        // (hard shadows near; from afar the terminator alone, the marched shadows would be noise)
        if (lit > 0.0 && foot < 0.02 && nearK > 0.0) {
            vec3 q = P0 + n * 2e-5;
            float ts = 4e-5;
            for (int i = 0; i < 28; i++) {
                vec3 s = q + uSunDir * ts;
                float r = length(s);
                if (r > 1.0 + HM) break;
                float a;
                float gap = r - (1.0 + terrain(s / r, foot * 2.0 + ts * pixA, true, a));
                sh = min(sh, smoothstep(0.0, 0.0008 + ts * 0.02, gap));
                if (sh <= 0.0) break;
                ts += max(gap * 0.7, 2e-4 + ts * 0.05);
            }
        }
        // its own shadow on the ground
        vec3 rel = d * t * uRL;
        vec3 A = uWhaleA - uEye - rel, B = uWhaleB - uEye - rel;
        float wsh = 0.0;
        if (dot(A + B, uSunDir) > 0.0) {
            vec3 a2v = A - uSunDir * dot(A, uSunDir), b2v = B - uSunDir * dot(B, uSunDir);
            vec3 ab = b2v - a2v;
            float hq = clamp(-dot(a2v, ab) / max(dot(ab, ab), 1.0), 0.0, 1.0);
            float dd = length(a2v + ab * hq);
            float w = uWhaleW * (0.5 + 0.7 * sin(3.14159 * clamp(0.1 + hq * 0.85, 0.0, 1.0)));
            wsh = smoothstep(w * 1.25, w * 0.75, dd) * 0.85 * exp(-max(dot(A + B, uSunDir) * 0.5, 0.0) / (uWhaleW * 14.0));
        }
        sh = mix(1.0, sh, nearK * smoothstep(0.02, 0.006, foot));
        vec3 light = uShadowC * mix(0.25, 0.1, nearK) + vec3(0.05, 0.07, 0.12) * 0.35 + uSunC * 1.3 * lit * sh * (1.0 - wsh);
        col = alb * light;
        // the far ground fades a little toward the black (no air: only a touch, for depth)
        col *= mix(1.0, 0.75, smoothstep(0.05, 0.6, t));
        vec4 clip = uVP * vec4(rel, 1.0);
        depth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    }
    // the Earth over the Moon's horizon (the finale)
    float ce = 0.0;
    vec3 ec = vec3(0.0);
    if (uEarthK > 0.0 && cov < 1.0) {
        vec3 e1, e2;
        basisL(uEarthDir, e1, e2);
        float sr = sin(uEarthR);
        vec2 q = vec2(dot(d, e1), dot(d, e2)) / sr;
        float qr = length(q);
        float fw = uPix / sr * 1.5;
        if (qr < 1.0 + fw && dot(d, uEarthDir) > 0.0) {
            float z = sqrt(max(1.0 - qr * qr, 0.0));
            vec3 N = normalize(e1 * q.x + e2 * q.y - uEarthDir * z);
            float ndl = dot(N, uSunDir);
            vec3 Nr = vec3(N.x * cos(uTime * 0.01) - N.z * sin(uTime * 0.01), N.y, N.x * sin(uTime * 0.01) + N.z * cos(uTime * 0.01));
            float cont = fbm3(Nr * 2.2 + 11.0);
            vec3 sc = mix(vec3(0.05, 0.16, 0.42), mix(vec3(0.22, 0.42, 0.16), vec3(0.55, 0.48, 0.3), smoothstep(0.55, 0.7, fbm3(Nr * 5.0))), smoothstep(0.5, 0.53, cont));
            float cl = smoothstep(0.5, 0.68, fbm3(Nr * 4.2 + 7.0 + vec3(uTime * 0.004, 0.0, 0.0)));
            sc = mix(sc, vec3(0.95), cl * 0.85);
            float l = smoothstep(-0.08, 0.2, ndl);
            ec = sc * (0.03 + 1.2 * l);
            ec += vec3(1.0, 0.75, 0.4) * step(0.8, vnoise3(Nr * 40.0)) * smoothstep(0.5, 0.53, cont) * (1.0 - l) * 0.25;
            ec += vec3(0.35, 0.6, 1.0) * pow(1.0 - z, 3.0) * (0.15 + 0.85 * smoothstep(-0.2, 0.3, ndl));
            ce = smoothstep(1.0 + fw, 1.0 - fw, qr) * uEarthK;
        }
    }
    float a = cov * uMoonK;
    if (uDay > 0.0 && cov > 0.0) {
        // by day: pale and bright, washed into the sky, its shaded side lost in the sky's light
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
        col = mix(col, vec3(1.0, 0.98, 0.96) * (0.55 + 0.9 * lum), uDay);
        a *= mix(1.0, 0.25 + 0.55 * smoothstep(0.05, 0.35, lum), uDay);
    }
    if (a <= 0.001 && ce <= 0.001) discard;
    // the Moon over the Earth (it is nearer), the Earth over the sky
    vec3 outc = (col * a + ec * ce * (1.0 - a)) / max(a + ce * (1.0 - a), 1e-4);
    gl_FragDepth = a > 0.001 ? depth : 1.0;
    frag = vec4(outc, a + ce * (1.0 - a));
}
)";

// ---------------------------------------------------------------- the creature
// One mesh for its whole life, generated once (a grid per part: the body, the tail fin, the dorsal fin, the
// pectoral and pelvic fins); the vertex shader places every vertex against the morph uM (0 a koi .. 1 a whale), the
// swimming wave (lateral for the koi, vertical for the whale: its plane turns with the tail's, uTailR), the fins'
// beat and the spine's bend (an arc, around the planet in orbit). Normals from the deformed neighbors.
inline const char* WHALE_GLSL = R"(
uniform float uM, uTailR, uSwimPh, uSwimA, uBend, uFinPh, uFinA, uL, uCurv, uCurvV, uFinTurn;
uniform vec3 uRel;
uniform mat3 uRot;
const float X0 = 0.5, BL = 0.8;
float profileP(float u, float um, float rp) {
    if (u < um) { float t = 1.0 - u / um; return sqrt(max(1.0 - t * t, 0.0)); }
    float t = (u - um) / (1.0 - um);
    return rp + (1.0 - rp) * pow(cos(t * 1.5707963), 1.35);
}
// the cross-section's half sizes at u (0 the snout .. 1 the tail's root): upper, lower, width
vec3 section(float u) {
    vec3 koi = vec3(0.118 * profileP(u, 0.36, 0.2), 0.098 * profileP(u, 0.34, 0.2), 0.07 * profileP(u, 0.3, 0.22));
    vec3 wh = vec3(0.068 * profileP(u, 0.44, 0.1), 0.094 * profileP(u, 0.3, 0.085), 0.084 * profileP(u, 0.34, 0.1));
    return mix(koi, wh, uM);
}
vec3 waveDir() { float a = uTailR * 1.5707963; return vec3(0.0, sin(a), cos(a)); }
vec3 finSide() { float a = uTailR * 1.5707963; return vec3(0.0, cos(a), -sin(a)); }
// The swimming wave, travelling from the head to the tail, growing toward the tail: the koi's whole rear half
// sweeping (carangiform, about a wavelength along it), the whale's flexing mostly at its tail stock, its head steady.
float waveAt(float x) {
    float q = clamp(0.5 - x, 0.0, 1.3);
    float env = mix(0.06 + 1.5 * q * q, 0.015 + 2.1 * q * q * q, uM);
    return uSwimA * env * sin(uSwimPh - q * mix(6.0, 3.6, uM));
}
vec3 deform(vec3 p) {
    p += waveDir() * waveAt(p.x);
    // bending into its turns (sideways) and its climbs and dives
    float xc = p.x - 0.08;
    p.z += 0.5 * uCurv * xc * xc;
    p.y += 0.5 * uCurvV * xc * xc;
    float k = uBend;
    if (abs(k) > 1e-3) {
        float a = k * p.x;
        vec2 S = vec2(sin(a), cos(a) - 1.0) / k;
        vec2 N = vec2(sin(a), cos(a));
        p = vec3(S.x + N.x * p.y, S.y + N.y * p.y, p.z);
    }
    return p;
}
vec3 bodyPoint(float u, float th) {
    vec3 s = section(u);
    float c = cos(th), sn = sin(th);
    float hh = mix(s.y, s.x, smoothstep(-0.3, 0.3, c));
    return vec3(X0 - u * BL, c * hh, sn * s.z);
}
vec3 caudalPoint(float s, float t) {
    float xr = X0 - BL + 0.004;
    // the koi's broad fan, forked; the whale's flukes, swept back with a notch
    vec2 k = vec2(-0.21 * s * (1.0 - 0.42 * sq(1.0 - abs(t)) * smoothstep(0.25, 1.0, s)), t * (0.02 + 0.135 * pow(s, 0.85)));
    vec2 w = vec2(-s * (0.032 + 0.075 * pow(abs(t), 1.7)) + 0.014 * s * exp(-sq(t / 0.16)), t * 0.16 * (0.08 + 0.92 * pow(s, 0.42)));
    vec2 f = mix(k, w, uM);
    return vec3(xr + f.x, 0.0, 0.0) + finSide() * f.y;
}
vec3 dorsalPoint(float s, float t) {
    float uk = mix(0.3, 0.66, s), uw = mix(0.63, 0.72, s);
    float hk = 0.07 * (1.0 - 0.62 * s) * smoothstep(0.0, 0.1, s) * (1.0 - 0.6 * smoothstep(0.85, 1.0, s));
    float hw = 0.03 * pow(max(sin(3.14159 * s), 0.0), 0.6) * (1.0 - 0.35 * s);
    float u = mix(uk, uw, uM), h = mix(hk, hw, uM);
    vec3 sec = section(u);
    float sweep = mix(0.045 * t * (1.0 + s), 0.04 * t * t, uM);
    return vec3(X0 - u * BL - sweep, sec.x * 0.94 + t * h, 0.0);
}
vec3 pectoralPoint(float s, float t, float side) {
    float u = mix(0.2, 0.3, uM);
    vec3 sec = section(u);
    vec3 root = vec3(X0 - u * BL, -0.45 * sec.y, side * 0.72 * sec.z);
    vec3 dir = normalize(mix(vec3(-0.55, -0.45, 0.7 * side), vec3(-0.32, -0.42, 0.85 * side), uM));
    float flap = uFinA * sin(uFinPh + s * 0.9) * side + uFinTurn * (0.4 + 0.6 * s);
    float ca = cos(flap), sa = sin(flap);
    dir = vec3(dir.x, dir.y * ca - dir.z * sa, dir.y * sa + dir.z * ca);
    float len = mix(0.1, 0.31, uM);
    vec3 ch = normalize(vec3(-1.0, 0.0, 0.0) + dir * dir.x);
    float ck = 0.05 * sqrt(max(sin(3.14159 * (0.1 + 0.9 * s)), 0.0));
    float cw = 0.058 * (1.0 - 0.78 * s) + 0.008;
    float c = mix(ck, cw, uM);
    float bumps = t < 0.0 ? 0.005 * max(sin(s * 36.0), 0.0) * uM * (-t * 2.0) : 0.0;
    vec3 p = root + dir * (s * len) + ch * (t * c - bumps);
    p += normalize(cross(dir, ch)) * side * 0.12 * c * (0.25 - t * t);
    return p;
}
vec3 pelvicPoint(float s, float t, float side) {
    vec3 sec = section(0.52);
    vec3 root = vec3(X0 - 0.52 * BL, -0.8 * sec.y, side * 0.3 * sec.z);
    vec3 dir = normalize(vec3(-0.7, -0.55, 0.45 * side));
    float flap = 0.6 * uFinA * sin(uFinPh + 1.3 + s) * side;
    dir = vec3(dir.x, dir.y * cos(flap) - dir.z * sin(flap), dir.y * sin(flap) + dir.z * cos(flap));
    vec3 ch = normalize(vec3(-1.0, 0.0, 0.0) + dir * dir.x);
    float k = 1.0 - smoothstep(0.0, 0.5, uM);
    return root + (dir * (s * 0.06) + ch * (t * 0.028 * sqrt(max(sin(3.14159 * (0.1 + 0.9 * s)), 0.0)))) * k;
}
vec3 partPoint(float part, float a, float b, float side) {
    if (part < 0.5) return bodyPoint(a, b);
    if (part < 1.5) return caudalPoint(a, b);
    if (part < 2.5) return dorsalPoint(a, b);
    if (part < 3.5) return pectoralPoint(a, b, side);
    return pelvicPoint(a, b, side);
}
)";
inline const char* WHALE_VS = R"(#version 330 core
layout(location = 0) in vec4 aP; // part, a, b, side
uniform mat4 uVP;
out vec3 vW;
out vec3 vN;
out vec4 vP;
void main() {
    float part = aP.x, a = aP.y, b = aP.z, side = aP.w;
    float ea = part < 0.5 ? 0.0025 : 0.01, eb = part < 0.5 ? 0.012 : 0.02;
    float a2 = a + ea > 1.0 ? a - ea : a + ea;
    float b2 = part > 0.5 && b + eb > 1.0 ? b - eb : b + eb;
    vec3 p0 = deform(partPoint(part, a, b, side));
    vec3 pa = deform(partPoint(part, a2, b, side));
    vec3 pb = deform(partPoint(part, a, b2, side));
    vec3 n = cross(pa - p0, pb - p0) * sign(a2 - a) * sign(b2 - b);
    if (part > 2.5) n *= side;
    vN = uRot * normalize(n + vec3(1e-9));
    vec3 w = uRot * (p0 * uL);
    vW = uEye + uRel + w;
    vP = aP;
    gl_Position = uVP * vec4(uRel + w, 1.0);
}
)";
inline const char* WHALE_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
in vec4 vP;
out vec4 frag;
uniform vec3 uKoiC, uKoiPatch, uKoiPatch2, uBackC, uBellyC, uFinC, uGlowC, uEyeC;
uniform float uPatch2, uSeed, uGlowK, uSing, uSingPh, uSink, uFoam, uGloss, uDark;
void main() {
    float part = vP.x, a = vP.y, b = vP.z;
    vec3 n = normalize(vN);
    vec3 v = normalize(uEye - vW);
    bool fin = part > 0.5;
    if (fin && dot(n, v) < 0.0) n = -n;
    float ndl = dot(n, uSunDir);
    vec3 alb;
    float glow = 0.0;
    float gloss = uGloss;
    float ao = 1.0;
    if (!fin) {
        float u = a, th = b;
        vec3 sec = section(u);
        float c = cos(th);
        // the koi's markings (red patches on the back, a few ink spots), its scales
        vec3 P = vec3(u * 3.4, c * 0.6, sin(th) * 0.45) * 1.9 + uSeed * 7.0;
        float nz = fbm3(P) + 0.2 * c - 0.18 * smoothstep(0.8, 1.0, u) + 0.1 * smoothstep(0.12, 0.0, u);
        // (every edge antialiased: its width at least a pixel's)
        float wn = max(0.015, fwidth(nz) * 0.75);
        float red = smoothstep(0.505 - wn, 0.505 + wn, nz);
        float ni = fbm3(P * 1.7 + 13.0) + 0.1 * c;
        float wi = max(0.015, fwidth(ni) * 0.75);
        float ink = smoothstep(0.675 - wi, 0.675 + wi, ni) * uPatch2;
        vec3 koi = mix(uKoiC, uKoiPatch, red);
        koi = mix(koi, uKoiPatch2, ink);
        vec2 sq2 = vec2(u * 52.0, th * 7.5);
        sq2.y += step(1.0, mod(floor(sq2.x), 2.0)) * 0.5;
        vec2 sf = fract(sq2) - vec2(0.15, 0.5);
        float fs = max(fwidth(sq2.x), fwidth(sq2.y));
        float scl = smoothstep(0.38 - fs, 0.5 + fs, length(sf * vec2(1.25, 1.0)));
        koi *= 1.0 - 0.13 * scl * smoothstep(0.03, 0.12, u) * (1.0 - smoothstep(0.15, 0.4, fs));
        // the whale: a dark back, a pale belly, the throat's pleats; the markings become soft lights
        float cs = smoothstep(-0.45, 0.1, c);
        vec3 wh = mix(uBellyC, uBackC, cs);
        float pq = th * 12.0 / 3.14159;
        float fp = fwidth(pq);
        float pleat = smoothstep(0.1 + fp, 0.0, abs(fract(pq) - 0.5) - 0.36) * smoothstep(-0.25, -0.5, c) * smoothstep(0.55, 0.35, u) * (1.0 - smoothstep(0.15, 0.35, fp));
        wh *= 1.0 - 0.18 * pleat;
        wh *= 1.0 - 0.08 * smoothstep(0.55, 0.62, fbm3(P * 2.4 + 5.0)) * cs;
        alb = mix(koi, wh, smoothstep(0.15, 0.85, uM));
        float edge = 1.0 - smoothstep(0.0, max(0.03, wn * 1.5), abs(nz - 0.505));
        vec2 dq = fract(vec2(u * 30.0, th * 5.0)) - 0.5;
        float fd = max(fwidth(u * 30.0), fwidth(th * 5.0));
        float dots = smoothstep(0.11 + fd, 0.05, length(dq)) * (1.0 - smoothstep(0.2, 0.45, fd)) * step(0.55, hash(floor(vec2(u * 30.0, th * 5.0)))) * smoothstep(0.46, 0.52, nz + 0.05) * smoothstep(-0.6, 0.3, c);
        // (the old patches as fine lines of light along their edges, a faint wash inside: not a coat of paint)
        glow = (edge * 0.9 + red * 0.07 + dots * 0.7 * uDark) * smoothstep(0.2, 0.9, uM) * (0.3 + 0.7 * uDark) + red * 0.1 * (1.0 - uM) * uDark;
        // the song: light running from the head to the tail
        glow += uSing * exp(-sq((u - fract(uSingPh)) / 0.06)) * (0.5 + red);
        ao = mix(0.72, 1.0, smoothstep(-0.9, 0.3, c));
        // the eye (one on each side), a darker mouth line on the whale
        float ue = mix(0.075, 0.125, uM);
        float off = mix(0.55, -0.32, uM);
        float d1 = abs(th - (1.5707963 - off)), d2 = abs(th - (4.712389 + off));
        float dth = min(d1, d2);
        float rE = mix(0.013, 0.008, uM);
        vec2 eo = vec2((u - ue) * BL, dth * sec.z * 1.1);
        float er = length(eo);
        float fe = fwidth(er);
        float eyeM = smoothstep(rE + fe, rE * 0.82 - fe, er);
        vec3 ec = mix(uEyeC, vec3(0.02, 0.02, 0.04), smoothstep(rE * 0.75 + fe, rE * 0.55 - fe, er));
        float eh = length(eo - vec2(rE * 0.3, -rE * 0.25 * sign(th - 3.14159)));
        ec += vec3(1.0) * smoothstep(rE * 0.3 + fe, rE * 0.18 - fe, eh) * (1.0 - smoothstep(rE * 0.15, rE * 0.4, fe));
        float mc = c - (-0.12 - 0.3 * u / 0.2);
        float mline = smoothstep(0.012 + fwidth(mc), 0.0, abs(mc)) * (1.0 - smoothstep(0.16, 0.2, u)) * smoothstep(0.004, 0.02, u) * uM;
        alb = mix(alb, alb * 0.35, mline);
        alb = mix(alb, ec, eyeM);
        glow *= 1.0 - eyeM;
        gloss *= 1.0 - eyeM * 0.0;
    } else {
        float s = a, t = b;
        // fins: translucent, their rays fanning out, the tips paler; the whale's darker, lit at their edges
        float rq = t * (part < 1.5 ? 9.0 : 5.0);
        float fr = fwidth(rq);
        float ray = smoothstep(0.32 - fr, 0.5, abs(fract(rq) - 0.5)) * (1.0 - smoothstep(0.2, 0.45, fr));
        vec3 kf = mix(uFinC, uKoiPatch, (part < 1.5 ? 0.35 : 0.15) * smoothstep(0.2, 0.0, s)) * (1.0 - 0.12 * ray);
        kf = mix(kf, vec3(1.0), s * 0.25);
        vec3 wf = mix(uBackC, uBellyC, part > 2.5 ? 0.75 : 0.2);
        alb = mix(kf, wf, smoothstep(0.15, 0.85, uM));
        glow = smoothstep(0.6, 1.0, s) * smoothstep(0.3, 0.9, uM) * 0.7 + uSing * 0.3 * s;
        ao = 0.95;
        gloss *= 0.5;
        // the light through a fin, when the sun is behind it
        alb += uSunGlow * pow(max(dot(-v, uSunDir), 0.0), 3.0) * 0.35 * (1.0 - uM * 0.6);
    }
    // toon light, its full band reached sooner (the creature reads bright), a fill from the sky above
    float band = smoothstep(-0.05, 0.03, ndl) * 0.6 + smoothstep(0.2, 0.28, ndl) * 0.4;
    vec3 col = alb * (mix(uShadowC * mix(0.7, 1.0, ao), uSunC, band) * mix(0.85, 1.0, ao) + skyRefl(n) * 0.18 * (0.4 + 0.6 * max(n.y, 0.0)));
    // a soft gradient over the bands: its volume reads (in space above all, where the light is hard)
    col *= mix(0.78, 1.1, smoothstep(-0.6, 0.9, ndl)) * mix(1.0, mix(0.7, 1.15, smoothstep(-0.8, 0.8, ndl)), 0.6 * uDark);
    // a wet gloss: a crisp highlight
    float rs = dot(reflect(-v, n), uSunDir);
    float fsp = fwidth(rs);
    float spec = smoothstep(0.94 - fsp, 0.965 + fsp, rs);
    col += (uSunC * 0.5 + 0.5) * spec * gloss;
    // a warm rim along the silhouette on the sun's side, the creature's own soft light at night and in space
    float rim = smoothstep(0.6, 0.8, 1.0 - max(dot(n, v), 0.0));
    col += uRimC * rim * smoothstep(-0.3, 0.3, ndl + 0.2) * 0.4 * alb * (2.0 + 1.5 * uDark);
    col += uGlowC * (glow * uGlowK + rim * uGlowK * 0.25);
    // the deep, at game over
    col = mix(col, col * 0.25 + uDeepC * 0.3, uSink);
    // under the water: absorbed, under the surface; foam where it breaks the surface
    float hs = seaH(vW);
    float fh = max(fwidth(hs), 1e-6);
    // (the creature under the surface stays readable: not all the deep's absorption)
    if (hs < fh) col = mix(col, underWater(col, vW - vec3(0.0, max(hs, 0.0) + fh, 0.0)), smoothstep(fh, -fh, hs) * 0.8);
    float fn = vnoise(vec2(vW.x + vW.z, hs * 3.0) / (0.03 * uL) + uTime);
    float wl = uFoam * smoothstep(0.02 * uL + fh, 0.0, abs(hs)) * smoothstep(0.45 - max(fwidth(fn), 0.02), 0.45 + max(fwidth(fn), 0.02), fn);
    col = mix(col, vec3(1.0) * mix(uSunC, vec3(1.0), 0.4), wl * 0.8);
    frag = vec4(atmos(col, vW), 0.0);
}
)";

// ---------------------------------------------------------------- props: the lighthouse, stone lanterns
// Meshes turned on the CPU (meters), toon-lit, a material per vertex: 0 stone, 1 white paint, 2 red paint, 3 the
// lamp's glass (glowing), 4 a dark roof, 5 rock, 6 a lantern's paper window (glowing).
inline const char* PROP_VS = R"(#version 330 core
layout(location = 0) in vec4 aA; // position (model), material
layout(location = 1) in vec4 aB; // normal, height fraction
uniform mat4 uVP;
uniform vec3 uRel;      // model origin - eye
uniform float uScale;
out vec3 vW;
out vec3 vN;
flat out float vMat;
out float vH;
void main() {
    vec3 w = aA.xyz * uScale;
    vW = uEye + uRel + w;
    vN = aB.xyz;
    vMat = aA.w;
    vH = aB.w;
    gl_Position = uVP * vec4(uRel + w, 1.0);
}
)";
inline const char* PROP_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
flat in float vMat;
in float vH;
out vec4 frag;
uniform vec3 uMat[7];
uniform float uLamp;
uniform vec3 uLampC;
void main() {
    vec3 n = normalize(vN);
    vec3 v = normalize(uEye - vW);
    int m = int(vMat + 0.5);
    vec3 alb = uMat[m];
    if (m == 5) alb *= 0.85 + 0.3 * vnoise(vW.xz * 0.4 + vW.y * 0.3);
    float ndl = dot(n, uSunDir);
    vec3 col = alb * toonLight(ndl, mix(0.8, 1.0, vH));
    float rim = smoothstep(0.6, 0.78, 1.0 - max(dot(n, v), 0.0));
    col += uRimC * rim * smoothstep(-0.3, 0.3, ndl + 0.2) * 0.35 * alb * 2.0;
    if (m == 3 || m == 6) col = alb * 0.4 + uLampC * uLamp * (m == 3 ? 2.2 : 1.6);
    frag = vec4(atmos(col, vW), 0.0);
}
)";

// ---------------------------------------------------------------- toon trees and bushes
// One billboard per puff of foliage (a sphere with a scalloped leafy edge, its depth written as a sphere's), or a
// trunk (a cylinder impostor). Toon light: a cool shade, the leaf color, a sunlit band, painterly strokes, a warm rim.
inline const char* PUFF_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // position (meters), radius
layout(location = 2) in vec4 i1; // lit color, seed
layout(location = 3) in vec4 i2; // shade color, kind (0 a puff, 1 a trunk)
layout(location = 4) in vec4 i3; // trunk height, -, -, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uMinPx, uWindPhase, uWind;
out vec2 vLocal;
out vec3 vCtr;
flat out float vRad;
flat out vec4 vI1;
flat out vec4 vI2;
void main() {
    vec3 c = i0.xyz;
    float r = i0.w;
    float dist = length(c - uEye);
    float px = r / max(uMinPx * dist / 0.8, 1e-20);
    vI1 = i1;
    vI2 = i2;
    if (px < 0.25) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    if (i2.w > 0.5) {
        vec3 side = normalize(vec3(uCamR.x, 0.0, uCamR.z));
        vec3 w = c + side * aPos.x * r + vec3(0.0, 1.0, 0.0) * (aPos.y * 0.5 + 0.5) * i3.x;
        vLocal = aPos;
        vCtr = c;
        vRad = r;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    c += (uCamR * sin(uWindPhase * 1.3 + i1.w * 40.0)) * r * 0.03 * uWind;
    float s = max(r, uMinPx * dist * 0.6) * 1.15;
    vec3 w = c + (uCamR * aPos.x + uCamU * aPos.y) * s;
    vLocal = aPos * 1.15;
    vCtr = c;
    vRad = s / 1.15;
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* PUFF_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vCtr;
flat in float vRad;
flat in vec4 vI1;
flat in vec4 vI2;
out vec4 frag;
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
void main() {
    vec2 l = vLocal;
    float seed = vI1.w;
    if (vI2.w > 0.5) {
        // a trunk: a cylinder seen from the side
        float x = l.x;
        vec3 side = normalize(vec3(uCamR.x, 0.0, uCamR.z));
        vec3 fw = normalize(cross(side, vec3(0.0, 1.0, 0.0)));
        vec3 n = normalize(side * x - fw * sqrt(max(1.0 - x * x, 0.0)));
        vec3 col = vI2.rgb * toonLight(dot(n, uSunDir), 0.85);
        float cover = smoothstep(1.0, 0.85, abs(x));
        frag = vec4(atmos(col, vCtr) * cover, cover);
        return;
    }
    float rr = length(l);
    float th = atan(l.y, l.x);
    float edge = 1.0 + 0.075 * sin(th * 7.0 + seed * 40.0) + 0.04 * sin(th * 12.0 + seed * 17.0);
    float aa = max(fwidth(rr), 1e-4);
    float cover = clamp(0.5 - (rr - edge) / aa, 0.0, 1.0);
    if (cover <= 0.01) discard;
    float rn = min(rr / edge, 1.0);
    float z = sqrt(max(1.0 - rn * rn, 0.0));
    vec3 fwd = normalize(uEye - vCtr);
    vec3 nS = normalize(uCamR * l.x / edge + uCamU * l.y / edge + fwd * z);
    vec4 clip = uVP * vec4(vCtr + nS * vRad - uEye, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    vec3 n = normalize(nS + vec3(0.0, 0.5, 0.0));
    float ndl = dot(n, uSunDir);
    vec2 sp = l * 2.8 + seed * 13.0;
    vec2 ci = floor(sp);
    vec2 f = fract(sp) - 0.5 - (hash2(ci) - 0.5) * 0.5;
    float ang = hash(ci + 5.0) * 0.8 + 0.2;
    f = vec2(cos(ang) * f.x + sin(ang) * f.y, -sin(ang) * f.x + cos(ang) * f.y);
    float stroke = smoothstep(0.3, 0.22, length(f * vec2(1.0, 2.3)));
    float k = ndl + (stroke - 0.3) * 0.18;
    float b1 = smoothstep(-0.06, 0.02, k), b2 = smoothstep(0.5, 0.58, k);
    vec3 col = mix(vI2.rgb, vI1.rgb, b1);
    col = mix(col, vI1.rgb * 1.25 + 0.03, b2);
    col *= uSunC / max(max(uSunC.r, uSunC.g), max(uSunC.b, 0.5)) * 0.3 + 0.7;
    col *= mix(1.0, 0.62, smoothstep(0.88, 0.98, rn) * (1.0 - b1 * 0.7));
    col += uRimC * smoothstep(0.8, 0.97, rn) * smoothstep(0.1, 0.5, ndl) * 0.35;
    frag = vec4(atmos(col, vCtr) * cover, cover);
}
)";

// ---------------------------------------------------------------- glows: spray, sparkles, birds, balloons, rings...
// World billboards (meters), premultiplied: 0 an orb of light (additive), 1 a petal, 2 a spark, 3 a grass tuft,
// 4 a reed, 5 a seabird, 6 a satellite, 7 a hot-air balloon, 8 a streak, 9 a star burst, 10 a ring of song (in the
// plane given by i3), 11 a floating paper lantern, 12 the lighthouse's beam (along i3), 13 a drop of spray.
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, size (meters, radius)
layout(location = 2) in vec4 i1; // rgb, alpha
layout(location = 3) in vec4 i2; // mode, seed, spin, -
layout(location = 4) in vec4 i3; // an axis (rings: their normal; the beam: its direction), param
uniform mat4 uVP;
uniform vec3 uEye, uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
out float vDim;
void main() {
    float d = length(i0.xyz - uEye);
    vLocal = aPos;
    vB = i1;
    vC = i2;
    vD = i3;
    int mode = int(i2.x + 0.5);
    float px = i0.w / max(uMinPx * d / 0.8, 1e-20);
    if (mode == 10) {
        vec3 nn = normalize(i3.xyz);
        vec3 e1 = normalize(cross(nn, abs(nn.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
        vec3 e2 = cross(nn, e1);
        vDim = 1.0;
        vec3 w = i0.xyz + (e1 * aPos.x + e2 * aPos.y) * i0.w * 1.15;
        vLocal = aPos * 1.15;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 12) {
        vec3 ax = normalize(i3.xyz);
        vec3 toE = normalize(uEye - i0.xyz);
        vec3 sd = normalize(cross(ax, toE));
        vDim = 1.0;
        float t = aPos.y * 0.5 + 0.5;
        vec3 w = i0.xyz + ax * t * i3.w + sd * aPos.x * i0.w * (0.15 + t);
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 5 || mode == 6 || mode == 7 || mode == 11) {
        vDim = smoothstep(0.6, 2.0, px);
        vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * max(i0.w, uMinPx * d * 1.6) * (mode == 11 ? 2.2 : 1.0);
        if (mode == 11) vLocal = aPos * 2.2;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 3 || mode == 4) {
        vDim = smoothstep(1.5, 5.0, px);
        vec3 side = normalize(vec3(uCamR.x, 0.0, uCamR.z));
        vec3 w = i0.xyz + side * aPos.x * i0.w * 1.2 + vec3(0.0, 1.0, 0.0) * (aPos.y + 1.0) * i0.w;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 8) {
        float s = max(i0.w, uMinPx * d * 1.5);
        vDim = 1.0;
        vec3 ax = length(i3.xyz) > 0.5 ? normalize(i3.xyz) : vec3(0.0, 1.0, 0.0);
        vec3 sd = normalize(cross(ax, normalize(uEye - i0.xyz)));
        vec3 w = i0.xyz + sd * aPos.x * s * 0.25 + ax * aPos.y * s * 5.0;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    float minS = uMinPx * d * (mode == 1 || mode == 13 ? 1.6 : 2.2);
    float s = max(i0.w, minS);
    vDim = min(1.0, sq(i0.w / max(minS, 1e-20)) + 0.35);
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * s;
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
in float vDim;
out vec4 frag;
void main() {
    int mode = int(vC.x + 0.5);
    float r = length(vLocal);
    if (mode == 10) {
        // a ring of song: a soft band of light, its inner edge brightest, shimmering around
        float w = vD.w;
        float band = exp(-sq((r - 1.0) / w)) + 0.25 * exp(-sq((r - 1.0 + w * 1.5) / (w * 2.5)));
        float th = atan(vLocal.y, vLocal.x);
        band *= 0.75 + 0.25 * sin(th * 9.0 + vC.y * 30.0 + vC.z);
        vec3 c = mix(vB.rgb, rainbow(fract(th / 6.2831 + vC.y)) * 1.2 + 0.2, 0.25);
        frag = vec4(c * band * vB.a, 0.0);
        return;
    }
    if (mode == 12) {
        // the lighthouse's beam: a soft cone of light
        float t = vLocal.y * 0.5 + 0.5;
        float v = exp(-sq(vLocal.x * 1.8)) * (1.0 - t) * smoothstep(0.0, 0.04, t);
        frag = vec4(vB.rgb * v * vB.a, 0.0);
        return;
    }
    if (mode == 11) {
        // a floating paper lantern: a warm glowing box, its frame, its halo; its light on the water around
        vec2 q = vLocal;
        float box = step(abs(q.x), 0.42) * step(abs(q.y - 0.05), 0.5);
        float frame = box * (step(0.36, abs(q.x)) + step(0.46, abs(q.y - 0.05)) + step(abs(q.x), 0.03) * 0.6);
        float base = step(abs(q.x), 0.5) * step(abs(q.y + 0.52), 0.07);
        vec3 paper = vB.rgb * (1.3 - 0.5 * abs(q.x) - 0.25 * (q.y + 0.5));
        vec3 col = paper * box * (1.0 - frame * 0.7) + vec3(0.12, 0.06, 0.04) * (frame * box + base);
        float a = max(box, base) * vB.a * vDim;
        vec3 halo = vB.rgb * exp(-dot(q, q) * 3.0) * 0.22 * vB.a;
        frag = vec4(col * a + halo * (1.0 - a), a);
        return;
    }
    if (mode == 5) {
        // a seabird: two long wings gliding and beating, darker tips, banking into its turns
        float fl = mix(sin(vC.z), 0.25, vC.w);
        float ca = cos(vD.w), sa = sin(vD.w);
        vec2 q = vec2(ca * vLocal.x - sa * vLocal.y, sa * vLocal.x + ca * vLocal.y);
        float x = abs(q.x);
        float wingY = fl * 0.35 * x + 0.25 * x - 0.32 * x * x;
        float wing = smoothstep(0.13, 0.04, abs(q.y - wingY)) * step(x, 0.98) * (1.0 - smoothstep(0.75, 0.98, x) * 0.5);
        float body = smoothstep(0.18, 0.08, length(q * vec2(1.0, 1.8)));
        vec3 c = mix(vB.rgb, vB.rgb * 0.35, smoothstep(0.7, 0.9, x));
        float a = max(wing, body) * vB.a * vDim;
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 6) {
        vec2 q = vLocal;
        float body = step(abs(q.x), 0.16) * step(abs(q.y), 0.2);
        float panel = step(abs(q.x), 0.92) * step(0.24, abs(q.x)) * step(abs(q.y), 0.13);
        float strut = step(abs(q.y), 0.025) * step(abs(q.x), 0.9);
        vec3 pc = mix(vec3(0.15, 0.3, 0.85), vec3(0.4, 0.7, 1.0), step(0.5, fract(q.x * 6.0)));
        vec3 col = body > 0.5 ? vec3(1.0, 0.8, 0.3) : panel > 0.5 ? pc : vec3(0.85);
        float a = max(max(body, panel), strut) * vB.a * vDim;
        col += vec3(1.0) * exp(-dot(q, q) * 30.0) * (0.5 + 0.5 * sin(vC.z * 3.0));
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 7) {
        vec2 q = vLocal - vec2(0.0, 0.15);
        float env = length(q * vec2(1.0, 0.92 + 0.25 * step(q.y, 0.0) * (-q.y)));
        float in_ = smoothstep(0.66, 0.62, env + max(-q.y - 0.35, 0.0) * 0.9);
        float stripe = step(0.5, fract((q.x / max(sqrt(max(0.42 - q.y * q.y, 0.02)), 0.1)) * 2.5 + 0.25));
        vec3 col = mix(vB.rgb, vec3(1.0, 0.97, 0.9), stripe * 0.85);
        col *= 0.75 + 0.35 * smoothstep(0.4, -0.4, q.x - q.y * 0.3);
        float basket = step(abs(vLocal.x), 0.1) * step(abs(vLocal.y + 0.83), 0.07);
        float rope = step(abs(abs(vLocal.x) - 0.12 + (vLocal.y + 0.7) * 0.25), 0.012) * step(-0.78, vLocal.y) * step(vLocal.y, -0.5);
        col = basket > 0.5 ? vec3(0.55, 0.32, 0.15) : rope > 0.5 ? vec3(0.4) : col;
        // the burner's glow, at dusk
        col += vec3(1.0, 0.6, 0.25) * exp(-sq(vLocal.x / 0.08) - sq((vLocal.y + 0.62) / 0.08)) * vC.z;
        float a = max(max(in_, basket), rope) * vB.a * vDim;
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 8) {
        float v = exp(-sq(vLocal.x * 3.0)) * smoothstep(1.0, 0.2, abs(vLocal.y)) * vB.a;
        frag = vec4(vB.rgb * v, 0.0);
        return;
    }
    if (mode == 3 || mode == 4) {
        // blades curving in the wind (vC.z), darker at their foot; a reed with its brown head
        float y = vLocal.y * 0.5 + 0.5;
        float cov = 0.0, tip = 0.0;
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float h = 0.55 + 0.45 * hash1(vC.y * 31.0 + fi);
            float x0 = (hash1(vC.y * 17.0 + fi * 3.1) - 0.5) * 1.1;
            float t = y / h;
            float bend = (hash1(vC.y * 7.0 + fi) - 0.5) * 0.6 + vC.z;
            float x = x0 + bend * t * t;
            float w = (mode == 4 ? 0.07 : 0.12) * (1.0 - t);
            float inside = step(t, 1.0) * smoothstep(w, w * 0.6, abs(vLocal.x - x));
            if (inside > cov) { cov = inside; tip = t; }
        }
        vec3 c = vB.rgb * mix(0.55, 1.2, tip);
        float a = cov * vB.a * vDim;
        if (mode == 4) {
            vec2 hc = vec2(vC.z * 0.9 * 0.6, 0.62);
            float head = smoothstep(0.075, 0.055, length((vLocal - hc) * vec2(1.0, 0.33)));
            c = mix(c, vec3(0.45, 0.26, 0.14), head);
            a = max(a, head * vB.a * vDim);
        }
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 1) {
        float a = vC.z;
        vec2 q = vec2(cos(a) * vLocal.x + sin(a) * vLocal.y, -sin(a) * vLocal.x + cos(a) * vLocal.y);
        float e = length(q / vec2(0.9, 0.5 + 0.35 * abs(sin(vC.z * 0.7))));
        float in_ = smoothstep(1.0, 0.85, e);
        vec3 c = vB.rgb * (0.85 + 0.3 * q.x);
        float al = in_ * vB.a * vDim;
        frag = vec4(c * al, al);
        return;
    }
    if (mode == 13) {
        // a drop of spray: a soft white bead, a brighter edge
        float in_ = smoothstep(1.0, 0.8, r);
        vec3 c = vB.rgb * (0.8 + 0.3 * smoothstep(0.4, 0.95, r) + 0.2 * smoothstep(0.0, -0.6, vLocal.y));
        float al = in_ * vB.a * vDim * 0.9;
        frag = vec4(c * al, al);
        return;
    }
    if (mode == 9) {
        float rays = exp(-abs(vLocal.x) * 22.0) * exp(-abs(vLocal.y) * 2.5) + exp(-abs(vLocal.y) * 22.0) * exp(-abs(vLocal.x) * 2.5);
        float v = exp(-r * r * 30.0) * 1.5 + rays + 0.2 * exp(-r * r * 5.0);
        v *= smoothstep(1.0, 0.7, r);
        frag = vec4(vB.rgb * v * vB.a * vDim, 0.0);
        return;
    }
    float v = mode == 2 ? exp(-r * r * 22.0) + 0.25 * exp(-r * r * 5.0) : exp(-r * r * 14.0) * 1.2 + 0.4 * exp(-r * r * 3.0);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(vB.rgb * v * vB.a * vDim, 0.0);
}
)";

// ---------------------------------------------------------------- the aurora trailing from the fins
// Ribbons built on the CPU every frame (a strip per fin tip): curtains of light, green at their foot to violet at
// their top, rays running along them, fading toward their end.
inline const char* RIBBON_VS = R"(#version 330 core
layout(location = 0) in vec4 aA; // position - eye, u along
layout(location = 1) in vec4 aB; // v across, alpha, seed, -
uniform mat4 uVP;
out vec2 vUV;
out vec2 vS;
void main() {
    vUV = vec2(aA.w, aB.x);
    vS = aB.yz;
    gl_Position = uVP * vec4(aA.xyz, 1.0);
}
)";
inline const char* RIBBON_FS = R"(#version 330 core
in vec2 vUV;
in vec2 vS;
out vec4 frag;
uniform vec3 uAuroraC, uAurora2C;
uniform float uTime;
void main() {
    float u = vUV.x, v = vUV.y;
    float rays = 0.55 + 0.45 * vnoise(vec2(u * 60.0 + vS.y * 9.0, uTime * 0.6));
    float body = smoothstep(0.0, 0.12, v) * exp(-v * 2.2) * 1.6;
    vec3 c = mix(uAuroraC, uAurora2C, smoothstep(0.15, 0.9, v));
    float a = body * rays * vS.x * smoothstep(0.0, 0.05, u) * (1.0 - smoothstep(0.55, 1.0, u));
    frag = vec4(c * a, 0.0);
}
)";

// ---------------------------------------------------------------- clouds
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, radius
layout(location = 2) in vec4 i1; // seed, alpha, -, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec2 vLocal;
out vec3 vW;
flat out vec4 vI;
void main() {
    vLocal = aPos;
    vI = i1;
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * i0.w;
    vW = w;
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* CLOUD_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vW;
flat in vec4 vI;
out vec4 frag;
uniform vec3 uCloudLit, uCloudShade, uCamR, uCamU;
// A puff's cauliflower: rounded lobes over lobes (its outline and its relief), on a flat-ish base.
float lobes(vec2 q, float s) {
    vec2 o = vec2(s * 37.0, s * 11.0);
    float h = 0.0;
    h += (vnoise(q * 2.6 + o) - 0.5) * 0.16;
    h += (vnoise(q * 5.5 + o * 1.7) - 0.5) * 0.08;
    h += (vnoise(q * 11.0 + o * 2.3) - 0.5) * 0.035;
    return h;
}
// (its relief: the broad lobes only, the shading stays clean)
float lobes2(vec2 q, float s) {
    vec2 o = vec2(s * 37.0, s * 11.0);
    return (vnoise(q * 2.6 + o) - 0.5) * 0.16 + (vnoise(q * 5.5 + o * 1.7) - 0.5) * 0.08;
}
void main() {
    vec2 q = vLocal;
    float s = vI.x;
    float r = length(q);
    // the outline: lobed, the top bulging, the base flatter and cut
    float lb = lobes(q, s);
    float edge = 0.86 + lb + 0.05 * smoothstep(-0.2, 0.8, q.y);
    float dr = r - edge;
    float aa = max(fwidth(dr), 1e-4);
    float cover = clamp(0.5 - dr / aa, 0.0, 1.0);
    float base = -0.5 + 0.04 * (vnoise(vec2(q.x * 6.0 + s * 9.0, 0.0)) - 0.5);
    cover *= clamp(0.5 + (q.y - base) / max(fwidth(q.y), 1e-4), 0.0, 1.0);
    if (cover < 0.01) discard;
    vec3 fwd = normalize(uEye - vW);
    // the relief: a sphere's normal, bent by the lobes' slopes (the cauliflower's bumps)
    float rn = min(r / max(edge, 0.3), 1.0);
    float e = 0.03;
    float gx = (lobes2(q + vec2(e, 0.0), s) - lobes2(q, s)) / e, gy = (lobes2(q + vec2(0.0, e), s) - lobes2(q, s)) / e;
    vec3 n0 = uCamR * q.x + uCamU * q.y + fwd * sqrt(max(1.0 - rn * rn, 0.0));
    vec3 nrm = normalize(normalize(n0) - (uCamR * gx + uCamU * gy) * 0.9);
    // toon light, three bands: the cool shade, the body, the sunlit tops; softly self-shadowed toward the base
    // seen from above (the eye over it): its sunlit top faces the eye, no shaded base
    float above = smoothstep(0.02, 0.25, fwd.y);
    nrm = normalize(mix(nrm, normalize(vec3(0.0, 1.0, 0.0) + nrm * 0.35), above));
    float ndl = dot(nrm, uSunDir) + 0.2 * nrm.y + 0.35 * above;
    float band = smoothstep(-0.18, -0.08, ndl) * 0.45 + smoothstep(0.12, 0.2, ndl) * 0.3 + smoothstep(0.5, 0.58, ndl) * 0.25;
    vec3 shade = uCloudShade * mix(vec3(0.85, 0.9, 1.08), vec3(1.0), 0.4);
    vec3 col = mix(shade, uCloudLit, band);
    // the base, in its own shadow; the hollows between the lobes
    col *= mix(mix(0.72, 1.0, smoothstep(-0.55, 0.15, q.y)), 1.0, above);
    col *= 1.0 - 0.18 * smoothstep(0.0, -0.06, lb) * (1.0 - band) * (1.0 - 0.6 * above);
    // a warm rim where the sun grazes its silhouette, and the light through its edge when the sun is behind it
    float rim = smoothstep(0.72, 0.98, rn) * clamp(dot(normalize(n0), uSunDir) + 0.35, 0.0, 1.0);
    col += uSunGlow * rim * 0.45;
    float back = pow(max(dot(-fwd, uSunDir), 0.0), 4.0);
    col += uSunGlow * back * smoothstep(0.7, 0.98, rn) * 0.8;
    col = atmos(col, vW);
    // (never darker than the haze around it; from above, never darker than the sea of clouds' sunlit tops)
    col = max(col, mix(uHazeC * 0.92, atmos(uCloudLit * 0.8, vW), 0.45 + 0.55 * above));
    float a = cover * vI.y;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the sea of clouds (a shell, seen from above)
inline const char* SEA_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uCloudY, uDrift, uFade, uPix;
uniform vec3 uCloudLit, uCloudShade;
const float R = 6.371e6;
float dens(vec3 p, vec3 n, float foot) {
    float v = 0.0, w = 0.0;
    float sc = 900.0;
    vec2 q = p.xz + vec2(uDrift, uDrift * 0.3);
    // (swirled: the great octaves warped, so from orbit the cover streams in bands and swirls)
    vec3 wv = vec3(vnoise3(n * 3.0 + 1.0), vnoise3(n * 3.0 + 7.0), vnoise3(n * 3.0 + 13.0)) - 0.5;
    n = normalize(n + wv * 0.06 + vec3(0.0, 0.0, 0.02 * sin(n.y * 9.0 + n.x * 4.0)));
    for (int i = 0; i < 6; i++) {
        float k = smoothstep(sc * 0.8, sc * 0.2, foot);
        // (the planar octaves only near: far out their hash loses its precision)
        if (i < 3) k *= 1.0 - smoothstep(3.0e4, 1.5e5, length(q));
        float nz = i < 3 ? vnoise(q / sc + float(i) * 5.1) : vnoise3(n * (R / sc) + float(i) * 3.3);
        float amp = 0.6 + 0.1 * float(i);
        v += (nz - 0.5) * amp * k;
        w += sq(amp * k);
        sc *= 4.0;
    }
    return 0.5 + v / sqrt(max(w, 1e-4)) * 0.22;
}
void main() {
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    float Rc = R + uCloudY;
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float h = uEye.y - uCloudY;
    float c0 = dot(uEye.xz, uEye.xz) + h * (h + 2.0 * Rc);
    float b = dot(d, oc);
    float disc = b * b - c0;
    if (c0 < 0.0 || disc <= 0.0 || b > 0.0) discard;
    float t = c0 / (-b + sqrt(disc));
    vec3 p = uEye + d * t;
    vec3 n = normalize(oc + d * t);
    float foot = t * uPix;
    float v = dens(p, n, foot);
    float cover = smoothstep(0.5, 0.5 + max(0.02, min(foot / 4000.0, 0.12)), v);
    if (cover < 0.01) discard;
    float e = max(foot * 2.0, 60.0);
    float vx = dens(p + vec3(e, 0.0, 0.0), n, foot), vz = dens(p + vec3(0.0, 0.0, e), n, foot);
    vec3 nrm = normalize(n + vec3(-(vx - v), 0.0, -(vz - v)) * 18.0);
    float ndl = dot(nrm, uSunDir);
    // three toon bands (the thick tops brightest), a warm glow on the sunlit tops, cool in the hollows
    float band = smoothstep(0.0, 0.08, ndl) * 0.4 + smoothstep(0.3, 0.37, ndl + (v - 0.6) * 0.8) * 0.35 + smoothstep(0.55, 0.62, ndl + (v - 0.62) * 1.2) * 0.25;
    vec3 col = mix(uCloudShade * vec3(0.9, 0.94, 1.06), uCloudLit, band);
    col += uSunGlow * 0.18 * smoothstep(0.6, 0.75, v) * smoothstep(0.2, 0.6, ndl);
    // thick tops bright, thin edges and the hollows between the heads in their own shade
    col *= mix(0.74, 1.05, smoothstep(0.5, 0.75, v)) * mix(0.86, 1.0, smoothstep(0.5, 0.57, v));
    float lit = smoothstep(-0.1, 0.1, dot(n, uSunDir));
    col *= mix(0.12, 1.0, lit);
    col = atmos(col, p);
    float a = cover * uFade * smoothstep(0.0, 0.01, -dot(d, normalize(oc)));
    // (its depth pulled a little toward the eye: the planet's under it, at a nearly equal depth far out)
    vec4 clip = uVP * vec4((p - uEye) * 0.995, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- composite: the world upscaled, the bloom, stars
// The sky (reduced resolution) into the multisampled target, with its depth (so the pads still hide the koi).
inline const char* UPSAMPLE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSky, uDepth;
void main() {
    frag = texture(uSky, vUv);
    gl_FragDepth = texture(uDepth, vUv).r;
}
)";
// The world is drawn mirrored (it swims left to right, on the left of the well): uv.x flipped here.
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld, uBloomT;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan, uRes;
uniform float uBloomK, uTwinkle, uTime, uSat;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec2 uv = vec2(1.0 - vUv.x, vUv.y);
    vec4 w = texture(uWorld, uv);
    vec3 c = w.rgb + texture(uBloomT, uv).rgb * uBloomK;
    float vis = w.a;
    if (vis > 0.002) {
        vec3 d = rayDir(uv);
        vec3 ad = abs(d);
        vec2 face = ad.x > ad.y && ad.x > ad.z ? d.yz / ad.x + vec2(7.0 * sign(d.x), 0.0)
                  : ad.y > ad.z ? d.xz / ad.y + vec2(0.0, 7.0 * sign(d.y)) : d.xy / ad.z + vec2(3.5 * sign(d.z), 3.5);
        vec2 sp = face * 300.0 * max(uRes.y / 1080.0, 0.5);
        mat2 J = mat2(dFdx(sp), dFdy(sp));
        float det = J[0][0] * J[1][1] - J[1][0] * J[0][1];
        mat2 Ji = abs(det) > 1e-8 ? inverse(J) : mat2(0.0);
        float px = sqrt(abs(det)) + 1e-4;
        vec3 st = vec3(0.0);
        for (int L = 0; L < 3; L++) {
            float cell = L == 0 ? 1.0 : L == 1 ? 3.1 : 9.7;
            float fade = smoothstep(4.0, 7.0, cell / px);
            if (fade <= 0.0) continue;
            vec2 g = floor(sp / cell);
            float rr = hash(g + float(L) * 31.0);
            if (rr < (L == 0 ? 0.3 : 0.4)) {
                vec2 o = (g + 0.25 + 0.5 * hash2(g + 3.1)) * cell;
                vec2 dd = Ji * (sp - o);
                float b = pow(hash(g + 7.7), L == 0 ? 2.5 : 1.5);
                float rad = (L == 0 ? 0.6 : 1.0) + 0.5 * b;
                float tw = 1.0 + uTwinkle * sin(uTime * (1.2 + 3.0 * hash(g + 1.3)) + rr * 80.0);
                vec3 tint = mix(vec3(0.7, 0.85, 1.0), vec3(1.0, 0.88, 0.7), hash(g + 9.1));
                st += tint * exp(-dot(dd, dd) / (rad * rad)) * b * max(tw, 0.0) * (L == 0 ? 0.7 : 1.5) * fade;
            }
        }
        c += st * vis;
    }
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSat), 0.0);
    frag = vec4(c, 1.0);
}
)";
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(x, y) * uTexel).rgb;
    c /= 9.0;
    float l = max(c.r, max(c.g, c.b));
    frag = vec4(c * max(l - uThreshold, 0.0) / max(l, 1e-3), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.227, 0.194, 0.121, 0.054, 0.016);
    vec3 c = texture(uSrc, vUv).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uSrc, vUv + uDir * float(i) * 1.5).rgb * w[i];
        c += texture(uSrc, vUv - uDir * float(i) * 1.5).rgb * w[i];
    }
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- motes over the board, the gauge
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (radius, board units), mode
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // param, param2, -, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vLocal = aPos;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPxPerUnit;
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    if (mode == 1) {
        // the gauge: a faint ring, the charge a bright arc from the top, clockwise
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ring = exp(-sq((r - 0.8) / max(px * 1.2, 0.012)));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = exp(-sq((r - 0.8) / max(px * 2.5, 0.03))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.6 + 0.4 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    if (mode == 3) {
        // a bubble: a thin bright ring, a glint
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ring = exp(-sq((r - 0.75) / max(px * 1.5, 0.06)));
        float gl = exp(-dot(vLocal - vec2(-0.3, 0.3), vLocal - vec2(-0.3, 0.3)) * 40.0);
        frag = vec4(c * (ring * 0.8 + gl + 0.12 * smoothstep(0.8, 0.0, r)) * vB.a, 0.0);
        return;
    }
    float v = exp(-r * r * 14.0) + 0.3 * exp(-r * r * 3.5);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(c * v * vB.a, 0.0);
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
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.12;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: koi-scale enamel
// Each piece one shape (its cells merge): a glossy enamel, deeper toward its middle, covered in fine overlapping
// scales that catch the light row by row, a bright inner rim, a wet glint at the top. The ghost is the rim alone; a
// cleared cell melts into bubbles of light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, shine, drain
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp / (2.0 * aA.z);
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
uniform float uTime;
uniform vec3 uLightC;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.2, rc = 0.07;
    vec2 q = abs(p);
    bool hx = p.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = p.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = p.x < 0.0 ? (p.y < 0.0 ? 4 : 6) : (p.y < 0.0 ? 5 : 7);
    float c = 0.5 - g;
    if (!hx && !vy) { vec2 k = q - (c - r); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - r; }
    if (hx && !vy) return q.y - c;
    if (!hx && vy) return q.x - c;
    if (has(m, db)) return -1.0;
    return rc - length(max(vec2(c + rc) - q, 0.0));
}

void main() {
    int mode = int(vC.x + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    int mk = int(vA.w + 0.5);
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float sd = shapeSdf(vLocal, mk, 0.05);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    float rimW = max(aa * 1.6, 0.035);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.6);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.07) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // enamel: deeper toward the middle, lighter at the top
    vec3 col = tint * mix(1.0, 0.55, smoothstep(0.02, 0.4, e)) * (0.9 + 0.18 * clamp(vLocal.y + 0.5, 0.0, 1.0));
    // scales: rows of overlapping arcs, each lit along its lower edge
    vec2 q = vWorld * 3.2;
    q.x += step(1.0, mod(floor(q.y), 2.0)) * 0.5;
    vec2 f = fract(q) - vec2(0.5, 1.0);
    float sc = length(f * vec2(1.0, 0.85));
    float scaleEdge = smoothstep(0.62, 0.55, sc) * smoothstep(0.42, 0.55, sc);
    float scaleIn = smoothstep(0.58, 0.2, sc);
    col *= 0.88 + 0.16 * scaleIn;
    col += hi * scaleEdge * 0.22 * smoothstep(0.03, 0.12, e);
    // a slow shimmer drifting across the stack, like light under water
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.x * 0.9 - vWorld.y * 0.6) * 1.1 - uTime * 0.5 + vnoise(vWorld * 0.7) * 2.0)), 8.0);
    col += uLightC * sheen * 0.16 * smoothstep(0.0, 0.25, e);
    // a wet glint near the top left of each cell
    float glint = smoothstep(0.12, 0.04, length((vLocal - vec2(-0.2, 0.24)) * vec2(1.0, 1.8)));
    col += vec3(1.0) * glint * 0.35 * smoothstep(0.05, 0.15, e);
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        // melting into bubbles: holes open, round, growing
        vec2 bq = vWorld * 6.0;
        vec2 bi = floor(bq);
        float bd = length(fract(bq) - 0.5 - (hash2(bi) - 0.5) * 0.4);
        float br = erase * 1.2 * (0.6 + 0.6 * hash(bi + 3.0));
        keep = smoothstep(br - 0.05, br + 0.05, bd) * (1.0 - smoothstep(0.6, 1.0, erase));
        col = mix(col, hi * 1.4, smoothstep(0.0, 0.5, erase));
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * 0.97 * vB.a * keep;
    frag = vec4(col * a, a);
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
uniform float uAlpha, uHaloA, uGlowA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uHaloA;
    float glow = smoothstep(0.15, 0.47, d) * uGlowA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill) + uInk * glow * (1.0 - fill) * 0.35;
    frag = vec4(c, a) * uAlpha;
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
    c *= 1.0 - 0.22 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace skywhale_shaders

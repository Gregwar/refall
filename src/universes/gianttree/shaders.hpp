#pragma once
// GIANT TREE's shaders: the world (the sky, the planet and space in one ray-traced pass; the tree's bark and puffs
// of foliage placed against the growth; clouds, the lights in the air) at a reduced resolution with depth, a soft
// bloom, the stars at full resolution, then the veil behind the well, the seed-glass pieces, the lettering.
// Positions are in meters, drawn relative to the eye (uVP has the eye at its origin): the tree spans from a meter
// to millions of kilometers.

namespace gianttree_shaders {

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
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
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
// aerial perspective in color (an exponential atmosphere: nothing of it is left in space).
inline const char* LIGHT_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uShadowC, uSunGlow, uHazeC, uRimC;
uniform float uFogK, uAlt, uNight, uBridge, uTime;
uniform vec3 uBridgeCol;
float airDensity(float h0, float h1) {
    const float H = 8500.0;
    float x0 = max(h0, 0.0) / H, x1 = max(h1, 0.0) / H;
    float dx = abs(x1 - x0);
    return dx > 1e-3 ? (exp(-min(x0, x1)) - exp(-max(x0, x1))) / dx : exp(-x0);
}
vec3 atmos(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    float od = uFogK * L * airDensity(uAlt, wp.y);
    vec3 v = d / max(L, 1e-3);
    vec3 hc = mix(uHazeC, uSunGlow, pow(max(dot(v, uSunDir), 0.0), 6.0) * 0.45);
    col = mix(hc, col, exp(-od));
    return mix(col, uBridgeCol, uBridge);
}
// three tones: the shade (cool), the light, the full sun
float toonBand(float ndl) { return smoothstep(-0.05, 0.03, ndl) * 0.6 + smoothstep(0.42, 0.5, ndl) * 0.4; }
vec3 toonLight(float ndl, float ao) { return mix(uShadowC * mix(0.7, 1.0, ao), uSunC, toonBand(ndl)) * mix(0.82, 1.0, ao); }
)";

// The tree's shape against the growth (the same functions as gtree:: in gianttree.hpp).
inline const char* TREE_GLSL = R"(
uniform vec4 uWob;
uniform float uG, uWind, uWindPhase, uGust, uJelly;
float crownW(float G) { return exp(G) * (0.05 + 1.1 * exp(-G / 3.0)); }
vec3 trunkCenter(float y) {
    float l = log(max(y, 0.0) + 0.05);
    return vec3(y * uWob.x * sin(uWob.y * l + uWob.z), y, y * uWob.x * cos(uWob.y * l * 0.8 + uWob.w));
}
// thick at the foot (and flaring into the roots), slender under the crown, a pointed tip inside it
float trunkRadius(float y) {
    float T = exp(uG), w = crownW(uG);
    float rTop = mix(0.045, 0.062, smoothstep(2.0, 6.0, uG)) * w;
    float q = clamp(y / T, 0.0, 1.0);
    float r = rTop * (1.0 + mix(0.6, 1.4, smoothstep(1.0, 6.0, uG)) * sq(1.0 - q));
    r *= 1.0 + 0.45 * exp(-max(y, 0.0) / (rTop * 2.5));
    return r * sqrt(clamp((T - y) / (0.2 * w), 0.0, 1.0));
}
const vec3 WIND_DIR = vec3(0.94, 0.0, 0.33);
vec3 swayAt(float q) {
    float w = crownW(uG);
    return WIND_DIR * w * q * q * ((uWind * 0.025 + uGust * 0.08) * sin(uWindPhase * 0.6) + uGust * 0.05);
}
vec3 crownToWorld(vec3 c) {
    vec3 cs = c * vec3(1.0 - 0.4 * uJelly, 1.0 + uJelly, 1.0 - 0.4 * uJelly);
    float sw = (uWind * 0.03 + uGust * 0.1) * sin(uWindPhase * 0.6 + 0.4) + uGust * 0.05;
    cs.xz += cs.y * sw * WIND_DIR.xz * 0.5;
    return trunkCenter(exp(uG)) + swayAt(1.0) + cs * crownW(uG);
}
float trunkPath(float y) { return clamp((log(max(y, 0.0) + 0.01) + 4.6) / (uG + 4.6), 0.0, 1.0); }
)";

// ---------------------------------------------------------------- the sky, the planet, space
// Per pixel: the planet (a sphere under the tree's foot: the meadow near it in crisp two-tone patches, continents
// and oceans farther, the night side's lights), its atmosphere (a bright sky near the ground, deepening to space
// with the altitude, the limb glowing), the sun, the moon (a sphere floating high behind the tree), far hills at the
// horizon, and in space the nebula, a ringed planet and a galaxy. Writes its depth (the planet's surface).
// Alpha: where the stars may shine.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uPix, uSunSize, uStars, uHills, uDeep, uRinged, uGlow, uSpaceK;
uniform vec3 uZenithC, uHorizonC, uSpaceC, uNebulaC, uNebula2C;
uniform vec3 uGroundA, uGroundB, uOceanC, uLandC, uDesertC, uFlowerC;
uniform vec4 uMoon;               // center (meters), radius
uniform vec4 uCrownW;             // the crown's center and radius (meters): its shadow on the meadow
uniform vec3 uPlanetDir, uGalaxyDir;
uniform float uBeam;              // the drop's light rushing up into space
uniform vec3 uBeamPos;
const float R = 6.371e6;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void basis(vec3 a, out vec3 x, out vec3 y) {
    x = normalize(cross(a, abs(a.y) < 0.95 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    y = cross(x, a);
}
// The ground's color at p (n: the planet's normal there, t: the distance, foot: a pixel's size there).
vec3 landColor(vec3 p, vec3 n, float t, float foot, out float water) {
    // continents (on the sphere), the tree's foot always on land
    float c0 = fbm3(n * 2.6 + 11.0);
    float c = c0 + 0.4 * smoothstep(0.99985, 0.999995, n.y);
    float cw = max(foot / 3.0e6, 0.004);
    water = 1.0 - smoothstep(0.47 - cw, 0.47 + cw, c);
    float dry = smoothstep(0.52, 0.62, fbm3(n * 5.0 + 3.0)) * (1.0 - smoothstep(0.55, 0.8, c));
    vec3 land = mix(uLandC, uDesertC, dry);
    land *= mix(1.0, 0.62, smoothstep(0.55, 0.7, fbm3(n * 14.0 + 7.0)) * (1.0 - dry));   // forests
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
    float kNear = 1.0 - smoothstep(6.0e3, 8.0e4, t);
    land = mix(land * mix(0.85, 1.12, tone), meadow, kNear);
    // snow on the far mountains and the poles
    float snow = smoothstep(0.82, 0.9, abs(n.z) + 0.1 * fbm3(n * 9.0)) + smoothstep(0.68, 0.72, c0) * 0.8;
    land = mix(land, vec3(0.97, 0.98, 1.0), clamp(snow, 0.0, 1.0) * (1.0 - kNear));
    // flowers in the near grass
    if (foot < 0.02) {
        vec2 cell = floor(p.xz / 0.16);
        vec2 o = (cell + 0.25 + 0.5 * hash2(cell)) * 0.16;
        float h = hash(cell + 3.7);
        float fl = step(h, 0.05) * smoothstep(0.035, 0.022, length(p.xz - o));
        vec3 fc = h < 0.02 ? vec3(1.0, 0.95, 0.4) : h < 0.035 ? uFlowerC : vec3(1.0);
        land = mix(land, fc, fl * smoothstep(0.02, 0.008, foot));
    }
    vec3 ocean = mix(uOceanC, uOceanC * vec3(0.55, 0.75, 1.0), smoothstep(0.47, 0.3, c));
    return mix(land, ocean, water);
}
void skyAt(vec3 d, out vec4 fragC, out float fragD) {
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float c0 = dot(uEye.xz, uEye.xz) + uEye.y * (uEye.y + 2.0 * R);   // |oc|^2 - R^2, without cancellation
    float alt = c0 / (sqrt(c0 + R * R) + R);
    vec3 up = normalize(oc);
    float b = dot(d, oc);
    float disc = b * b - c0;
    vec3 col;
    float stars = 0.0;
    float depth = 1.0;
    vec3 sunGlowCol = uSunGlow;
    float sd = dot(d, uSunDir);
    if (disc > 0.0 && b < 0.0) {
        // ---- the planet
        float t = c0 / (-b + sqrt(disc));
        vec3 p = uEye + d * t;
        vec3 n = normalize(oc + d * t);
        float foot = t * uPix;
        float water;
        vec3 alb = landColor(p, n, t, foot, water);
        float ndl = dot(n, uSunDir);
        float lit = smoothstep(-0.08, 0.12, ndl);
        float band = smoothstep(-0.05, 0.03, ndl) * 0.55 + smoothstep(0.35, 0.45, ndl) * 0.45;
        vec3 light = mix(uShadowC, uSunC, band);
        // the crown's shadow on the meadow, along the sun
        vec3 toC = uCrownW.xyz - p;
        float along = dot(toC, uSunDir);
        float perp = length(toC - uSunDir * along);
        float shade = along > 0.0 ? smoothstep(uCrownW.w * 0.95, uCrownW.w * 0.8, perp) : 0.0;
        light = mix(light, uShadowC, shade * 0.75);
        col = alb * light;
        // the water's glint
        col += water * uSunGlow * pow(max(dot(reflect(d, n), uSunDir), 0.0), 40.0) * 0.6 * lit;
        // the night side: dark blue, the cities' lights
        float night = 1.0 - lit;
        col = mix(col, alb * uShadowC * 0.18, night * smoothstep(1e4, 2e5, alt));
        if (foot > 800.0) {
            vec3 cn = n * 900.0;
            float city = step(0.78, vnoise3(cn)) * step(0.6, vnoise3(cn * 3.1 + 5.0)) * (1.0 - water);
            col += vec3(1.0, 0.8, 0.45) * city * night * smoothstep(1e5, 6e5, alt) * 0.9;
        }
        // the air between: the aerial perspective, the limb's glow
        float od = 1.0e-5 * t * airDensity(alt, 0.0);
        vec3 hc = mix(mix(uHazeC, uZenithC, 0.3), sunGlowCol, pow(max(sd, 0.0), 6.0) * 0.5);
        col = mix(hc * mix(1.0, 0.25, night * smoothstep(1e4, 3e5, alt)), col, exp(-od));
        vec4 clip = uVP * vec4(p - uEye, 1.0);
        depth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    } else {
        // ---- the sky: the atmosphere along the ray (its lowest point), space beyond
        float hc = b < 0.0 ? -disc / (sqrt(R * R - disc) + R) : alt;
        float air = exp(-hc / 18000.0);
        float dip = atan(sqrt(alt * (2.0 * R + alt)), R);
        float el = asin(clamp(dot(d, up), -1.0, 1.0)) + dip;   // above the limb
        // ground level: the horizon's glow to a deep zenith; higher, the zenith darkens into space
        float zk = smoothstep(0.0, 0.75, el);
        vec3 zen = mix(uZenithC, uZenithC * vec3(0.25, 0.3, 0.55), smoothstep(2000.0, 30000.0, alt));
        vec3 sky = mix(uHorizonC, zen, pow(zk, 0.7));
        float a = clamp(pow(air, 0.55) * 1.15, 0.0, 1.0);
        // space
        vec3 sp = uSpaceC;
        float az = atan(d.x, -d.z);
        // the nebula: soft clouds of color across the sky
        float nb = fbm3(d * 2.2 + 4.0);
        float neb = smoothstep(0.45, 0.8, nb) * (0.35 + 0.65 * smoothstep(0.3, 0.9, fbm3(d * 5.0 + 1.0)));
        sp += mix(uNebulaC, uNebula2C, fbm3(d * 3.7 + 9.0)) * neb * (0.08 + 0.5 * uDeep);
        // a ringed planet and a galaxy, far out
        if (uDeep > 0.0) {
            vec3 bx, by;
            basis(uPlanetDir, bx, by);
            float pr = mix(0.04, 0.1, uRinged);
            vec2 q = vec2(dot(d, bx), dot(d, by)) / pr;
            float front = dot(d, uPlanetDir);
            if (front > 0.0) {
                // the ring plane, tilted
                vec2 rq = vec2(q.x * 0.94 + q.y * 0.34, (-q.x * 0.34 + q.y * 0.94) / 0.28);
                float rr = length(rq);
                float ring = smoothstep(1.35, 1.4, rr) * smoothstep(2.25, 2.2, rr) * (0.65 + 0.35 * sin(rr * 40.0)) * (1.0 - 0.6 * smoothstep(1.78, 1.8, rr) * smoothstep(1.86, 1.84, rr));
                float r2 = dot(q, q);
                vec3 pc = vec3(0.0);
                float pa = 0.0;
                if (r2 < 1.0) {
                    vec3 pn = normalize(bx * q.x + by * q.y - uPlanetDir * sqrt(1.0 - r2));
                    float lat = dot(pn, normalize(by * 0.94 - bx * 0.34));
                    float bands = sin(lat * 18.0 + 2.0 * fbm3(pn * 3.0)) * 0.5 + 0.5;
                    vec3 alb = mix(vec3(0.95, 0.72, 0.45), vec3(1.0, 0.92, 0.75), bands);
                    alb = mix(alb, vec3(0.85, 0.5, 0.35), smoothstep(0.75, 0.9, fbm3(pn * 6.0)));
                    pc = alb * mix(0.25, 1.15, smoothstep(-0.1, 0.1, dot(pn, -uSunDir) + 0.3));
                    pa = smoothstep(1.0, 0.98, r2);
                }
                vec3 rc = vec3(0.95, 0.85, 0.7) * ring;
                bool ringFront = rq.y < 0.0 || r2 >= 1.0;
                sp = mix(sp, pc, pa * uRinged);
                if (ringFront || pa < 0.5) sp = mix(sp, rc, ring * 0.85 * uRinged);
                stars -= (pa + ring) * 2.0 * uRinged;
            }
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
        // the limb, glowing (seen from high up)
        float limb = exp(-hc / 6000.0) * smoothstep(3000.0, 60000.0, alt) * step(0.0, -b + 0.0);
        col += uHorizonC * limb * 0.7;
        stars += (uStars + (1.0 - a)) * (1.0 - a * 0.85);
        // far hills along the horizon (seen from near the ground)
        if (uHills > 0.0) {
            float h1 = 0.012 + 0.022 * fbm(vec2(az * 2.6, 1.0)) + 0.012 * fbm(vec2(az * 9.0, 7.0));
            float h2 = 0.004 + 0.016 * fbm(vec2(az * 4.3 + 9.0, 3.0)) + 0.006 * fbm(vec2(az * 17.0, 2.0));
            float aa = uPix * 1.2;
            float m1 = smoothstep(h1 + aa, h1 - aa, el) * uHills, m2 = smoothstep(h2 + aa, h2 - aa, el) * uHills;
            vec3 far = mix(uHazeC, uGroundB * 0.9 + uZenithC * 0.25, 0.4);
            vec3 near = mix(uHazeC, uGroundB, 0.62);
            col = mix(col, far, m1);
            col = mix(col, near, m2);
            stars *= 1.0 - max(m1, m2);
        }
    }
    // the sun: a crisp disc, a halo
    {
        float ang = acos(clamp(sd, -1.0, 1.0));
        float disc = smoothstep(uSunSize * 1.04, uSunSize * 0.96, ang) * (1.0 - uNight);
        float glow = pow(max(sd, 0.0), 8.0) * 0.35 + exp(-ang / max(uSunSize * 3.0, 1e-3)) * 0.6;
        float ring = exp(-sq((ang - uSunSize * 2.2) / (uSunSize * 0.25))) * 0.12;
        col += sunGlowCol * (glow + ring) * (1.0 + 0.6 * uGlow) * (1.0 - uNight * 0.8) * (depth < 1.0 ? 0.3 : 1.0);
        col = mix(col, uSunC * 2.2 + 0.4, disc * (depth < 1.0 ? 0.0 : 1.0));
        stars *= 1.0 - clamp(glow * 2.0 + disc, 0.0, 1.0);
    }
    // the moon: a sphere high behind the tree, its terminator crisp, its craters
    if (depth >= 1.0) {
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
    // the drop's light: a beam rushing from the crown up into space
    if (uBeam > 0.0) {
        vec3 rel = uBeamPos - uEye;
        float tb = max(dot(rel, d), 0.0);
        vec3 cpos = uEye + d * tb;
        float dx = length((cpos - uBeamPos) - vec3(0.0, dot(cpos - uBeamPos, vec3(0, 1, 0)), 0.0));
        float above = smoothstep(0.0, uCrownW.w * 0.5, cpos.y - uBeamPos.y);
        float wdt = uCrownW.w * 0.4;
        float beam = (exp(-sq(dx / wdt)) * 0.8 + exp(-sq(dx / (wdt * 0.25))) * 0.8) * above * uBeam;
        float hue = cpos.y / (uCrownW.w * 6.0) - uTime * 0.8;
        col += mix(rainbow(fract(hue)), vec3(1.0), 0.15 + 0.5 * exp(-sq(dx / (wdt * 0.25)))) * beam * 1.6;
        stars *= 1.0 - clamp(beam, 0.0, 1.0);
    }
    col = mix(col, uBridgeCol, uBridge);
    fragD = depth;
    fragC = vec4(col, clamp(stars, 0.0, 1.0) * (1.0 - uBridge) * (depth < 1.0 ? 0.0 : 1.0));
}
// The planet's limb antialiased: within half a pixel of its edge, the planet and the sky beside it, blended by the
// pixel's coverage (the rest of the shading is per pixel, a hard edge otherwise).
void main() {
    vec3 d = rayDir(vUv);
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float c0 = dot(uEye.xz, uEye.xz) + uEye.y * (uEye.y + 2.0 * R);
    float b = dot(d, oc);
    float disc = b * b - c0;
    // (R - the ray's distance to the center) over a pixel's size at the limb: pixels inside the planet
    float k = c0 > 0.0 && b < 0.0 ? disc / ((R + sqrt(max(R * R - disc, 0.0))) * sqrt(c0) * uPix) : -1.0;
    if (abs(k) >= 0.5) {
        skyAt(d, frag, gl_FragDepth);
        return;
    }
    vec3 toC = normalize(-oc - d * dot(-oc, d));
    vec4 ci, co;
    float di, dout;
    skyAt(normalize(d + toC * (max(k, 0.1) - k) * uPix), ci, di);
    skyAt(normalize(d + toC * (min(k, -0.1) - k) * uPix), co, dout);
    float cov = clamp(k + 0.5, 0.0, 1.0);
    frag = mix(co, ci, cov);
    gl_FragDepth = cov > 0.5 ? di : dout;
}
)";

// ---------------------------------------------------------------- the bark
// The trunk's rings are placed on its axis with the radius it has at the current growth (the rings above the tip
// gathered into it); the crown's limbs and twigs grow from their base to their tip (crown space, scaled on the
// crown); the meadow's trees stand in meters. Toon: two bands of light, a cool shade, a warm rim, grooves winding
// up the wood, the drop's light running up it in rainbow colors.
inline const char* BARK_VS = R"(#version 330 core
layout(location = 0) in vec4 a0; // center, radius
layout(location = 1) in vec4 a1; // radial normal, u along the branch
layout(location = 2) in vec4 a2; // branch base, space (0 trunk, 1 crown, 2 world)
layout(location = 3) in vec4 a3; // birth, duration, path, sway phase
layout(location = 4) in vec4 a4; // bark u, bark v, seed, death
uniform mat4 uVP;
out vec3 vW;
out vec3 vN;
out vec3 vC;
out vec2 vBark;
out float vPath;
flat out float vSpace;
void main() {
    float space = a2.w;
    vec3 n = a1.xyz;
    vec3 p;
    float T = exp(uG);
    if (space < 0.5) {
        float y = a0.y;
        float yy = min(y, T);
        float r = y >= T ? 0.0 : trunkRadius(y);
        p = trunkCenter(yy) + swayAt(clamp(yy / T, 0.0, 1.0)) + n * r;
        vBark = vec2(a4.x, log(max(y, 0.0) + 0.3) * 2.2);
        vPath = trunkPath(yy);
        vC = vec3(0.0, (yy - T) / crownW(uG), 0.0);
    } else {
        float u = a1.w, tb = a3.x, dur = a3.y;
        float f = clamp((uG - tb) / dur, 0.0, 1.0);
        vec3 c = a0.xyz;
        float r = a0.w;
        if (u > f + 1e-4) {
            c = mix(a2.xyz, c, f / max(u, 1e-4));
            r = 0.0;
        } else if (f < 0.999) {
            r *= sqrt(clamp((f - u) / 0.25, 0.0, 1.0));
        }
        r *= mix(0.45, 1.0, clamp((uG - tb) / (dur * 2.5), 0.0, 1.0));
        vec3 q = c + n * r;
        if (space < 1.5) {
            p = crownToWorld(q);
            vC = q;
        } else {
            p = q;
            vC = vec3(0.0, 1.0, 0.0);
        }
        vBark = a4.xy;
        vPath = a3.z;
    }
    vW = p;
    vN = n;
    vSpace = space;
    gl_Position = uVP * vec4(p - uEye, 1.0);
}
)";
inline const char* BARK_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
in vec3 vC;
in vec2 vBark;
in float vPath;
flat in float vSpace;
out vec4 frag;
uniform vec3 uBarkC, uBarkDark, uGlowC;
uniform float uLeafGlow, uFlow, uFlowK, uBloom;
void main() {
    vec3 n = normalize(vN);
    vec3 v = normalize(uEye - vW);
    // grooves winding up the wood, a node band now and then (they recede down the trunk)
    float gx = vBark.x + 0.18 * sin(vBark.y * 2.3 + vBark.x * 1.7) + 0.1 * sin(vBark.y * 7.0);
    float fw = fwidth(gx) * 0.8 + 0.02;
    float groove = smoothstep(0.1 + fw, 0.1, abs(fract(gx) - 0.5));
    float node = vSpace < 0.5 ? smoothstep(0.035 + fwidth(vBark.y), 0.035, abs(fract(vBark.y * 0.5) - 0.5)) : 0.0;
    vec3 alb = mix(uBarkC, uBarkDark, groove * 0.55);
    alb = mix(alb, uBarkC * 1.2, node * 0.35);
    // in the crown's shade
    float ao = 1.0 - 0.5 * exp(-sq((vC.y + 0.18) / 0.3)) * step(vSpace, 1.5);
    float ndl = dot(n, uSunDir);
    vec3 col = alb * toonLight(ndl, ao);
    // a warm rim along the silhouette on the sun's side
    float rim = smoothstep(0.58, 0.72, 1.0 - max(dot(n, v), 0.0)) * smoothstep(-0.3, 0.3, ndl + 0.2);
    col += uRimC * rim * 0.45 * alb * 2.0;
    // the night trees: glowing grooves
    col += uGlowC * groove * uLeafGlow * 0.35 * (0.6 + 0.4 * sin(vBark.y * 3.0 - uTime * 1.2));
    // the drop's light running up from the roots, in rainbow colors
    float front = exp(-sq((vPath - uFlow) / 0.05));
    float trail = smoothstep(uFlow + 0.02, uFlow - 0.3, vPath) * 0.4;
    vec3 rc = mix(rainbow(fract(vPath * 2.0 - uTime * 0.7)), vec3(1.0), 0.3);
    col += rc * (front * 2.0 + trail * (0.4 + groove)) * uFlowK;
    col += uGlowC * uBloom * groove * 0.4;
    frag = vec4(atmos(col, vW), 0.0);
}
)";

// ---------------------------------------------------------------- the puffs of foliage
// One billboard per puff: a sphere with a scalloped, leafy edge, its depth written as a sphere's (so puffs cluster
// into round masses), lit as toon foliage: a cool shade, the leaf color, a sunlit band, painterly leaf strokes
// across the bands, a warm rim on the sun's side, a darker outline on the shade side. Blossoms dot it, fruits are
// glossy little spheres. Crown puffs are placed on the crown, vine leaves on the trunk's surface, the meadow's in
// meters. A puff pops open when born (an overshoot); every leaf falls at game over.
inline const char* LEAF_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // position (or the vine's ln height, angle, size), radius
layout(location = 2) in vec4 i1; // space, birth, seed, look
layout(location = 3) in vec4 i2; // clump center, path
layout(location = 4) in vec4 i3; // death, tint, sway, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uShed, uBurst, uMinPx;
out vec2 vLocal;
out vec3 vCtr;
out vec3 vClumpN;
out vec3 vCS;
flat out float vRad;
flat out vec4 vInfo; // seed, look, path, tint
void main() {
    float space = i1.x, birth = i1.y, seed = i1.z, look = i1.w;
    float open = clamp((uG - birth) / (look > 1.5 ? 0.06 : 0.14), 0.0, 1.0);
    float pop = open * (1.0 + 0.5 * sin(open * 3.14159));
    float fall = clamp((uG - i3.x) / 0.1, 0.0, 1.0);
    vec3 c;
    float r;
    vec3 cn;
    vec3 cs = vec3(0.0, 1.0, 0.0);
    float path = i2.w;
    float scale = 1.0;
    if (space < 0.5) {
        c = crownToWorld(i0.xyz);
        scale = crownW(uG);
        r = i0.w * scale * (1.0 + 0.25 * uJelly);
        cn = normalize(i0.xyz - i2.xyz + vec3(0.0, 0.03, 0.0));
        cs = i0.xyz;
    } else if (space < 1.5) {
        float y = exp(i0.x);
        float T = exp(uG);
        if (y > T * 0.985) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
        float tr = trunkRadius(y);
        vec3 rad = vec3(cos(i0.y), 0.0, sin(i0.y));
        c = trunkCenter(y) + swayAt(y / T) + rad * tr * 1.0;
        r = min(tr, 0.05 * crownW(uG)) * i0.z;
        scale = tr * 4.0;
        cn = normalize(rad + vec3(0.0, 0.25, 0.0));
        cs = vec3(0.0, (y - T) / crownW(uG), 0.0);
        path = trunkPath(y);
    } else {
        c = i0.xyz;
        r = i0.w;
        scale = r * 3.0;
        cn = normalize(i0.xyz - i2.xyz + vec3(0.0, 0.05, 0.0));
        path = -9.0;
    }
    // a flutter
    c += (uCamR * sin(uWindPhase * 2.6 + seed * 40.0) + uCamU * cos(uWindPhase * 2.2 + seed * 31.0)) * r * 0.05 * (uWind + uGust) * i3.z;
    // game over: every leaf falls, tumbling and drifting
    float sh = clamp(uShed * 1.6 - seed * 0.6, 0.0, 1.0) * step(space, 1.5);
    c += vec3(sh * 0.6 * (seed - 0.5), -sh * sh * 1.8, sh * 0.4 * sin(seed * 30.0)) * scale;
    r *= pop * (1.0 - fall) * (1.0 - sh * 0.6) * (1.0 + 0.18 * uBurst * step(space, 0.5) * step(look, 0.5));
    float dist = length(c - uEye);
    float px = r / max(uMinPx * dist / 0.8, 1e-20);
    if (px < 0.3 || r <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    // never thinner than a pixel or so
    float s = max(r, uMinPx * dist * 0.6) * 1.15;
    vec3 w = c + (uCamR * aPos.x + uCamU * aPos.y) * s;
    vLocal = aPos * 1.15;
    vCtr = c;
    vRad = s / 1.15;
    vClumpN = cn;
    vCS = cs;
    vInfo = vec4(seed, look, path, i3.y);
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* LEAF_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vCtr;
in vec3 vClumpN;
in vec3 vCS;
flat in float vRad;
flat in vec4 vInfo;
out vec4 frag;
uniform mat4 uVP;
uniform vec3 uLeafC, uLeaf2C, uLeafShadeC, uBlossomC, uFruitC, uGlowC, uCamR, uCamU;
uniform float uBlossom, uLeafGlow, uFlow, uFlowK, uBloom, uShimmer, uShed, uSpace;
void main() {
    vec2 l = vLocal;
    float rr = length(l);
    float seed = vInfo.x, look = vInfo.y;
    bool fruit = look > 0.5 && look < 1.5;
    float th = atan(l.y, l.x);
    float edge = fruit ? 1.0 : 1.0 + 0.075 * sin(th * 7.0 + seed * 40.0) + 0.04 * sin(th * 12.0 + seed * 17.0);
    float aa = max(fwidth(rr), 1e-4);
    float cover = clamp(0.5 - (rr - edge) / aa, 0.0, 1.0);
    if (cover <= 0.01) discard;
    float rn = min(rr / edge, 1.0);
    float z = sqrt(max(1.0 - rn * rn, 0.0));
    vec3 fwd = normalize(uEye - vCtr);
    vec3 nS = normalize(uCamR * l.x / edge + uCamU * l.y / edge + fwd * z);
    vec4 clip = uVP * vec4(vCtr + nS * vRad - uEye, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    vec3 n = normalize(nS + vClumpN * (fruit ? 0.0 : 0.9));
    float ndl = dot(n, uSunDir);
    // the crown's inside and underside in shade
    float ao = smoothstep(-0.32, 0.2, vCS.y + n.y * 0.15);
    vec3 col;
    if (fruit) {
        vec3 alb = uFruitC;
        col = alb * toonLight(ndl, 1.0);
        float spec = smoothstep(0.86, 0.9, dot(reflect(-fwd, nS), uSunDir));
        col += vec3(1.0) * spec * 0.8 + uGlowC * (uLeafGlow * 0.8 + uSpace * 0.9 * (0.75 + 0.25 * sin(uTime * 3.0 + seed * 40.0)));
    } else {
        // painterly leaf strokes shifting the bands
        vec2 sp = l * (look > 1.5 ? 2.0 : 2.8) + seed * 13.0;
        vec2 ci = floor(sp);
        vec2 f = fract(sp) - 0.5 - (hash2(ci) - 0.5) * 0.5;
        float ang = hash(ci + 5.0) * 0.8 - 0.4 + 0.6;
        f = vec2(cos(ang) * f.x + sin(ang) * f.y, -sin(ang) * f.x + cos(ang) * f.y);
        float stroke = smoothstep(0.3, 0.22, length(f * vec2(1.0, 2.3)));
        float k = ndl + (stroke - 0.3) * 0.18;
        float b1 = smoothstep(-0.06, 0.02, k), b2 = smoothstep(0.5, 0.58, k);
        vec3 tint = vec3(1.0) + (vInfo.w - 0.5) * vec3(0.1, 0.06, -0.08);
        vec3 lc = uLeafC * tint, l2 = uLeaf2C * tint;
        col = mix(uLeafShadeC * mix(0.6, 1.0, ao), lc * mix(0.85, 1.0, ao), b1);
        col = mix(col, l2, b2 * ao);
        col *= uSunC / max(max(uSunC.r, uSunC.g), max(uSunC.b, 0.5)) * 0.25 + 0.75;
        // blossoms dotting it
        if (uBlossom > 0.0 && look < 0.5) {
            vec2 bp = l * 3.3 + seed * 7.0;
            vec2 bi = floor(bp);
            vec2 bf = fract(bp) - 0.5 - (hash2(bi + 2.0) - 0.5) * 0.4;
            float on = step(hash(bi + 9.0), uBlossom * 0.55) * step(rn, 0.9);
            float br = length(bf), bt = atan(bf.y, bf.x);
            float petal = smoothstep(0.17, 0.14, br / (0.75 + 0.25 * cos(5.0 * bt)));
            vec3 bc = uBlossomC * mix(0.8, 1.15, b1);
            col = mix(col, bc, petal * on);
            col = mix(col, vec3(1.0, 0.9, 0.4), smoothstep(0.05, 0.03, br) * on);
        }
        // a darker outline on the shade side, a warm rim on the sun's
        col *= mix(1.0, 0.62, smoothstep(0.88, 0.98, rn) * (1.0 - b1 * 0.7));
        col += uRimC * smoothstep(0.8, 0.97, rn) * smoothstep(0.1, 0.5, ndl) * 0.35;
        // night leaves glowing at their edges, twinkling; a peak's shimmer
        float tw = 0.7 + 0.3 * sin(uTime * (0.7 + seed) + seed * 50.0);
        col += uGlowC * (uLeafGlow + uSpace * 0.25) * (0.25 + 0.75 * smoothstep(0.55, 1.0, rn)) * tw * 0.7;
        col += uGlowC * uShimmer * stroke * pow(max(sin(uTime * 2.3 + seed * 70.0 + ci.x * 3.0), 0.0), 10.0) * 0.8;
    }
    // the drop's light reaching the leaves (rainbow), then the crown blooming
    float path = vInfo.z;
    if (path > -1.0) {
        float front = exp(-sq((path - uFlow) / 0.08));
        float reached = smoothstep(uFlow + 0.05, uFlow - 0.1, path);
        vec3 rc = mix(rainbow(fract(path * 1.5 + seed * 0.3 - uTime * 0.5)), vec3(1.0), 0.35);
        col += rc * front * 1.2 * uFlowK;
        col = mix(col, col * 1.25 + uGlowC * 0.1, uBloom * reached);
    }
    // falling at game over: autumn colors
    col = mix(col, col * vec3(1.25, 0.75, 0.35), clamp(uShed * 1.5, 0.0, 1.0));
    frag = vec4(atmos(col, vCtr) * cover, cover);
}
)";

// ---------------------------------------------------------------- glows: sparkles, petals, grass, birds...
// World billboards (meters), premultiplied: 0 an orb of light (additive), 1 a petal, 2 a spark, 3 a grass tuft,
// 4 a wildflower, 5 a bird, 6 a satellite, 7 a hot-air balloon, 8 a streak (the speed of a spurt), 9 a star burst.
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, size (meters, radius)
layout(location = 2) in vec4 i1; // rgb, alpha
layout(location = 3) in vec4 i2; // mode, seed, spin, -
uniform mat4 uVP;
uniform vec3 uEye, uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
flat out vec4 vB;
flat out vec4 vC;
out float vDim;
void main() {
    float d = length(i0.xyz - uEye);
    vLocal = aPos;
    vB = i1;
    vC = i2;
    int mode = int(i2.x + 0.5);
    float px = i0.w / max(uMinPx * d / 0.8, 1e-20);
    if (mode == 5 || mode == 6 || mode == 7) {
        vDim = smoothstep(0.6, 2.0, px);
        vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * max(i0.w, uMinPx * d * 1.6);
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 3 || mode == 4) {
        // standing on its foot, facing the camera across
        vDim = smoothstep(1.5, 5.0, px);
        vec3 side = normalize(vec3(uCamR.x, 0.0, uCamR.z));
        vec3 w = i0.xyz + side * aPos.x * i0.w * 1.2 + vec3(0.0, 1.0, 0.0) * (aPos.y + 1.0) * i0.w;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    if (mode == 8) {
        // a streak along the vertical (the world rushing down past a spurt)
        float s = max(i0.w, uMinPx * d * 1.5);
        vDim = 1.0;
        vec3 w = i0.xyz + uCamR * aPos.x * s * 0.25 + vec3(0.0, 1.0, 0.0) * aPos.y * s * 5.0;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    float minS = uMinPx * d * (mode == 1 ? 1.6 : 2.2);
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
in float vDim;
out vec4 frag;
void main() {
    int mode = int(vC.x + 0.5);
    float r = length(vLocal);
    if (mode == 5) {
        // a bird: two wings beating
        float fl = sin(vC.z);
        vec2 q = vLocal;
        float x = abs(q.x);
        float wingY = fl * 0.45 * x - 0.15 * x * x;
        float wing = smoothstep(0.16, 0.05, abs(q.y - wingY)) * step(x, 0.95) * (1.0 - smoothstep(0.7, 0.95, x) * 0.6);
        float body = smoothstep(0.2, 0.1, length(q * vec2(1.0, 1.6)));
        float a = max(wing, body) * vB.a * vDim;
        frag = vec4(vB.rgb * a, a);
        return;
    }
    if (mode == 6) {
        // a satellite: a golden body, two blue panels, a glint
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
        // a hot-air balloon: a striped envelope, a basket
        vec2 q = vLocal * vec2(1.0, 1.0) - vec2(0.0, 0.15);
        float env = length(q * vec2(1.0, 0.92 + 0.25 * step(q.y, 0.0) * (-q.y))) ;
        float in_ = smoothstep(0.66, 0.62, env + max(-q.y - 0.35, 0.0) * 0.9);
        float stripe = step(0.5, fract((q.x / max(sqrt(max(0.42 - q.y * q.y, 0.02)), 0.1)) * 2.5 + 0.25));
        vec3 col = mix(vB.rgb, vec3(1.0, 0.97, 0.9), stripe * 0.85);
        col *= 0.75 + 0.35 * smoothstep(0.4, -0.4, q.x - q.y * 0.3);
        float basket = step(abs(vLocal.x), 0.1) * step(abs(vLocal.y + 0.83), 0.07);
        float rope = step(abs(abs(vLocal.x) - 0.12 + (vLocal.y + 0.7) * 0.25), 0.012) * step(-0.78, vLocal.y) * step(vLocal.y, -0.5);
        col = basket > 0.5 ? vec3(0.55, 0.32, 0.15) : rope > 0.5 ? vec3(0.4) : col;
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
        // blades curving in the wind (vC.z), darker at their foot; a wildflower on the tallest
        float y = vLocal.y * 0.5 + 0.5;
        float cov = 0.0, tip = 0.0;
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float h = 0.55 + 0.45 * hash1(vC.y * 31.0 + fi);
            float x0 = (hash1(vC.y * 17.0 + fi * 3.1) - 0.5) * 1.1;
            float t = y / h;
            float bend = (hash1(vC.y * 7.0 + fi) - 0.5) * 0.6 + vC.z;
            float x = x0 + bend * t * t;
            float w = 0.12 * (1.0 - t);
            float inside = step(t, 1.0) * smoothstep(w, w * 0.6, abs(vLocal.x - x));
            if (inside > cov) { cov = inside; tip = t; }
        }
        vec3 c = vB.rgb;
        if (mode == 4) {
            vec2 fc = vec2(vC.z * 0.5, 0.72);
            float fr = length(vLocal - fc), ft = atan(vLocal.y - fc.y, vLocal.x - fc.x);
            float fl = smoothstep(0.26, 0.21, fr / (0.8 + 0.2 * cos(5.0 * ft)));
            float heart = smoothstep(0.08, 0.06, fr);
            float stem = smoothstep(0.05, 0.02, abs(vLocal.x - fc.x * y * y)) * step(vLocal.y, 0.7);
            vec3 col = mix(vec3(0.2, 0.55, 0.2), c, fl);
            col = mix(col, vec3(1.0, 0.85, 0.3), heart);
            float a = max(fl, stem * 0.9) * vB.a * vDim;
            frag = vec4(col * a, a);
            return;
        }
        c *= mix(0.55, 1.2, tip);
        float a = cov * vB.a * vDim;
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
    if (mode == 9) {
        // a four-pointed star burst
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

// ---------------------------------------------------------------- clouds
// Toon cumulus puffs (world billboards, meters), sorted far to near: a round, scalloped edge, crisp bands of light
// (bright tops, lavender bellies), a warm rim when the sun is behind them.
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
void main() {
    float r = length(vLocal);
    float th = atan(vLocal.y, vLocal.x);
    float edge = 0.92 + 0.06 * sin(th * 6.0 + vI.x * 30.0) + 0.03 * sin(th * 11.0 + vI.x * 9.0);
    float aa = max(fwidth(r), 1e-4);
    float cover = clamp(0.5 - (r - edge) / aa, 0.0, 1.0);
    // a flat belly
    cover *= clamp(0.5 + (vLocal.y + 0.55) / aa, 0.0, 1.0);
    if (cover < 0.01) discard;
    vec3 fwd = normalize(uEye - vW);
    float rn = min(r / edge, 1.0);
    vec3 nrm = normalize(uCamR * vLocal.x + uCamU * vLocal.y + fwd * sqrt(max(1.0 - rn * rn, 0.0)));
    float ndl = dot(nrm, uSunDir) + 0.25 * nrm.y;
    float band = smoothstep(-0.12, -0.04, ndl) * 0.55 + smoothstep(0.32, 0.4, ndl) * 0.45;
    vec3 col = mix(uCloudShade, uCloudLit, band);
    float back = pow(max(dot(-fwd, uSunDir), 0.0), 4.0);
    col += uSunGlow * back * smoothstep(0.75, 0.98, rn) * 0.8;
    col = atmos(col, vW);
    float a = cover * vI.y;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the sea of clouds
// A shell of clouds around the planet at the clouds' height (ray traced per pixel, its depth written so the trunk
// passes through it): seen from above, billowing tops in crisp toon bands; from orbit, the planet's weather.
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
    // the octaves the pixel resolves, their spread kept the same whatever their number (crisp at every scale)
    float v = 0.0, w = 0.0;
    float sc = 900.0;
    vec2 q = p.xz + vec2(uDrift, uDrift * 0.3);
    for (int i = 0; i < 6; i++) {
        float k = smoothstep(sc * 0.8, sc * 0.2, foot);
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
    if (c0 < 0.0 || disc <= 0.0 || b > 0.0) discard;   // only from above
    float t = c0 / (-b + sqrt(disc));
    vec3 p = uEye + d * t;
    vec3 n = normalize(oc + d * t);
    float foot = t * uPix;
    float v = dens(p, n, foot);
    float cover = smoothstep(0.5, 0.5 + max(0.02, min(foot / 4000.0, 0.12)), v);
    if (cover < 0.01) discard;
    float e = max(foot * 2.0, 60.0);
    float vx = dens(p + vec3(e, 0.0, 0.0), n, foot), vz = dens(p + vec3(0.0, 0.0, e), n, foot);
    vec3 nrm = normalize(n + vec3(-(vx - v), 0.0, -(vz - v)) * 9.0);
    float ndl = dot(nrm, uSunDir);
    float band = smoothstep(0.0, 0.08, ndl) * 0.5 + smoothstep(0.35, 0.42, ndl + (v - 0.6) * 0.8) * 0.5;
    vec3 col = mix(uCloudShade, uCloudLit, band);
    float lit = smoothstep(-0.1, 0.1, dot(n, uSunDir));
    col *= mix(0.12, 1.0, lit);
    col = atmos(col, p);
    float a = cover * uFade * smoothstep(0.0, 0.01, -dot(d, normalize(oc)));
    vec4 clip = uVP * vec4(p - uEye, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- composite: the world upscaled, the bloom, stars
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld, uBloomT;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan, uRes;
uniform float uBloomK, uTwinkle, uTime, uSat;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec4 w = texture(uWorld, vUv);
    vec3 c = w.rgb + texture(uBloomT, vUv).rgb * uBloomK;
    float vis = w.a;
    if (vis > 0.002) {
        vec3 d = rayDir(vUv);
        // stars fixed on the sky (a cube-ish projection of the direction). Each pixel looks at its own cell only, so
        // a star keeps to the cell's middle and its glow is measured in screen pixels through the projection's
        // Jacobian (round on screen however the face stretches); a layer whose cells shrink under a few pixels fades
        // out (a wide view in space), or the grid aliases into streaks.
        vec3 ad = abs(d);
        vec2 face = ad.x > ad.y && ad.x > ad.z ? d.yz / ad.x + vec2(7.0 * sign(d.x), 0.0)
                  : ad.y > ad.z ? d.xz / ad.y + vec2(0.0, 7.0 * sign(d.y)) : d.xy / ad.z + vec2(3.5 * sign(d.z), 3.5);
        vec2 sp = face * 300.0 * max(uRes.y / 1080.0, 0.5);
        mat2 J = mat2(dFdx(sp), dFdy(sp));
        float det = J[0][0] * J[1][1] - J[1][0] * J[0][1];
        mat2 Ji = abs(det) > 1e-8 ? inverse(J) : mat2(0.0);
        float px = sqrt(abs(det)) + 1e-4; // sky units per pixel
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
    // a touch more color
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

// ---------------------------------------------------------------- motes over the board
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (radius, board units), mode
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
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    float v = exp(-r * r * 14.0) + 0.3 * exp(-r * r * 3.5);
    if (mode == 2) {
        float rays = exp(-abs(vLocal.x) * 40.0) * exp(-abs(vLocal.y) * 3.0) + exp(-abs(vLocal.y) * 40.0) * exp(-abs(vLocal.x) * 3.0);
        v = exp(-r * r * 30.0) + 0.6 * rays + 0.15 * exp(-r * r * 4.0);
    }
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

// ---------------------------------------------------------------- pieces: seed glass
// Each piece one shape (its cells merge): translucent glass with a warm seed of light at its heart, fine veins
// like a leaf's running through it, a bright inner rim. The ghost is the rim alone; a cleared cell dissolves into
// light.
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
uniform float uTime;
uniform vec3 uLightC;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.16, rc = 0.06;
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
        float a = (rim * 0.85 + inside * 0.08) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // glass: deeper toward the middle of the shape, a warm seed of light glowing at the heart of each cell
    vec3 col = tint * mix(0.95, 0.42, smoothstep(0.02, 0.38, e));
    float seed = smoothstep(0.12, 0.6, e) * (0.75 + 0.25 * vnoise(vWorld * 1.3 + uTime * 0.15));
    col += mix(tint, uLightC, 0.35) * seed * 0.4;
    // fine veins, like a leaf's, running through the glass
    vec2 wv = vWorld * 2.2;
    float vn = abs(vnoise(wv + vec2(0.0, vnoise(wv * 0.7) * 2.0)) - 0.5);
    col += hi * smoothstep(0.035, 0.0, vn) * 0.12 * smoothstep(0.04, 0.2, e);
    // a slow sheen drifting across the stack
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.x * 0.5 - vWorld.y * 0.85) * 0.7 - uTime * 0.3)), 10.0);
    col += uLightC * sheen * 0.14 * smoothstep(0.0, 0.25, e);
    col += tint * exp(-abs(e - 0.1) / 0.025) * 0.2;
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        float n = vnoise(vWorld * 2.3) * 0.65 + vnoise(vWorld * 5.1) * 0.35;
        keep = smoothstep(erase * 1.4 - 0.45, erase * 1.4 - 0.05, n) * (1.0 - smoothstep(0.55, 1.0, erase));
        col = mix(col, hi * 1.35, smoothstep(0.0, 0.4, erase));
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
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount) and in a glow of their ink (uP1.x),
// the field as baked then (64 px, 8 px of reach): the letter and its halo mixed in linear light, the glow added to
// the light under it (uP1.y: about the world's, linear).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uP0.a;
    float a = max(fill, halo);
    vec3 ink = pow(L.ink.rgb, vec3(2.2));
    vec3 c = ink * fill + pow(uP0.rgb, vec3(2.2)) * (a - fill);
    vec3 under = c + uP1.y * (1.0 - a);
    vec3 glow = ink * (smoothstep(0.15, 0.47, d) * uP1.x * (1.0 - fill) * 0.35);
    vec3 col = pow(c / max(a, 1e-4), vec3(1.0 / 2.2)) * a + pow(under + glow, vec3(1.0 / 2.2)) - pow(under, vec3(1.0 / 2.2));
    return vec4(col, a) * L.ink.a;
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
    // a soft vignette, a soft roll-off of the brightest lights
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.22 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace gianttree_shaders

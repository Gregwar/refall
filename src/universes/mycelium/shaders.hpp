#pragma once
// MYCELIUM's shaders: the world (the planet, its night sky, the moon and space in one ray-traced pass; the glowing
// threads, the mushrooms turned on a lathe, the ground cover, clouds, the lights in the air) at a reduced resolution
// with depth, a soft bloom, the stars at full resolution, then the veil behind the well, the felt pieces, the lettering.
// Positions are in meters, drawn relative to the eye (uVP has the eye at its origin): the world spans from a
// millimeter of moss to the whole Earth.

namespace mycelium_shaders {

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
)";

// The world's light: a moonlit night. Toon light (crisp bands, a cool tinted shade); a night mist that scales with
// the view (a macro's haze close up), the air's own perspective farther out; nothing of either in space.
inline const char* LIGHT_GLSL = R"(
uniform vec3 uEye, uMoonDir, uMoonC, uAmbC, uMistC, uNetC;
uniform float uFogK, uMistK, uAlt, uBridge, uTime, uMacro; // uMacro: 1 at the macro start (the first centimeter)
uniform vec3 uBridgeCol;
float airDensity(float h0, float h1) {
    const float H = 8500.0;
    float x0 = max(h0, 0.0) / H, x1 = max(h1, 0.0) / H;
    float dx = abs(x1 - x0);
    return dx > 1e-3 ? (exp(-min(x0, x1)) - exp(-max(x0, x1))) / dx : exp(-x0);
}
float fogOf(vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    return exp(-uFogK * L * airDensity(uAlt, wp.y) - uMistK * L);
}
vec3 atmos(vec3 col, vec3 wp) {
    col = mix(uMistC, col, fogOf(wp));
    return mix(col, uBridgeCol, uBridge);
}
float toonBand(float ndl) { return smoothstep(-0.05, 0.03, ndl) * 0.55 + smoothstep(0.4, 0.48, ndl) * 0.45; }
vec3 toonLight(float ndl, float ao) { return mix(uAmbC * mix(0.6, 1.0, ao), uMoonC, toonBand(ndl)) * mix(0.8, 1.0, ao); }
)";

// ---------------------------------------------------------------- the sky, the planet, space
// Per pixel: the planet (a sphere under the spore: the forest floor near it in moss and soil and fallen leaves at
// every scale the pixel resolves, forests and fields and a village farther, continents and oceans and the cities'
// lights from high up, the day side's crescent from space), lit by the moon, by the network's glow under it and by
// the biggest mushrooms' caps; its atmosphere (a night sky with a dusk glow along the horizon, deepening into space
// with the altitude, the limb glowing), the tree line along the horizon near the ground, the moon (a sphere), the
// nebula. Writes its depth (the planet's surface). Alpha: where the stars may shine.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uPix, uStars, uHills, uGlow, uReach, uPoolK, uOver;
uniform vec3 uSunDir, uMoonLit, uSkyTop, uSkyLow, uDusk, uSpaceC, uNebulaC;
uniform vec3 uMoss, uSoil, uLitter, uField, uForest, uLand, uOcean, uCity, uPoolC;
uniform vec4 uMoon;
uniform vec4 uVillage;
uniform vec4 uLightP[6];
uniform vec4 uLightC[6];
const float R = 6.371e6;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
// Rolling hills over the country (meters), the octaves the pixel resolves.
float hillsAt(vec2 q, float foot) {
    float h = 0.0, sc = 450.0, amp = 55.0;
    for (int i = 0; i < 5; i++) {
        h += (vnoise(q / sc + float(i) * 3.7) - 0.5) * amp * smoothstep(sc * 0.5, sc * 0.08, foot);
        sc *= 3.0;
        amp *= 3.0;
    }
    return h;
}
// A winding river through the valley: 0 on its axis.
float riverAt(vec2 q) {
    vec2 w = vec2(fbm(q / 9000.0 + 3.0), fbm(q / 9000.0 + 8.0)) - 0.5;
    return abs(vnoise(q / 6000.0 + w * 1.6 + 2.0) - 0.5);
}
// The ground's albedo at p (n: the planet's normal, t: the distance, foot: a pixel's size there, dist: along the
// surface from the spore). cityK: the lights of towns at night; wet: water reflecting the moon.
vec3 landColor(vec3 p, vec3 n, float t, float foot, float dist, out float water, out float cityK, out float wet) {
    // continents (the spore always on land)
    float c0 = fbm3(n * 2.6 + 11.0);
    float c = c0 + 0.5 * smoothstep(0.995, 0.99995, n.y);
    float cw = max(foot / 3.0e6, 0.004);
    water = 1.0 - smoothstep(0.47 - cw, 0.47 + cw, c);
    vec3 far = mix(uLand, uForest, smoothstep(0.45, 0.62, fbm3(n * 14.0 + 7.0)));
    far = mix(far, uField * 1.2, smoothstep(0.55, 0.7, fbm3(n * 6.0 + 2.0)) * 0.6);
    // the country: forests and fields (a patchwork, hedges between), the valley opening farther from the spore
    float fv = 0.0, sc = 30.0;
    for (int i = 0; i < 6; i++) {
        float k = smoothstep(sc * 0.7, sc * 0.12, foot);
        fv += (vnoise(p.xz / sc + float(i) * 7.3) - 0.5) * k;
        sc *= 3.6;
    }
    float open = smoothstep(250.0, 2500.0, dist) * 0.2 - 0.07;
    float forestK = smoothstep(-0.02, 0.02, fv - open);
    float fa = vnoise(p.xz / 3000.0) * 3.0;
    vec2 fq = mat2(cos(fa), sin(fa), -sin(fa), cos(fa)) * p.xz / vec2(140.0, 90.0);
    vec2 fc = floor(fq), ff = fract(fq);
    float hedge = smoothstep(0.06, 0.02, min(min(ff.x, 1.0 - ff.x), min(ff.y, 1.0 - ff.y))) * smoothstep(8.0, 2.0, foot);
    vec3 fieldC = uField * (0.7 + 0.6 * hash(fc)) * mix(vec3(1.0), uLitter / max(dot(uLitter, vec3(0.33)), 1e-3) * 0.4 + 0.6, step(0.7, hash(fc + 2.0)));
    fieldC = mix(fieldC, uForest * 0.8, hedge);
    // the forest's canopy, crown by crown
    float crown = vnoise(p.xz / 5.0) * 0.6 + vnoise(p.xz / 13.0) * 0.4;
    vec3 forestC = uForest * mix(1.0, mix(0.6, 1.45, smoothstep(0.35, 0.7, crown)), smoothstep(6.0, 0.8, foot));
    vec3 country = mix(fieldC, forestC, forestK);
    // the village's clearing
    float vd = length(p - uVillage.xyz);
    country = mix(country, uField * 1.1, smoothstep(uVillage.w * 1.5, uVillage.w * 0.9, vd));
    // a river, the moon in it
    float rv = riverAt(p.xz);
    float rw = 0.012, raa = max(foot / 6000.0, 0.002);
    wet = smoothstep(rw + raa, rw - raa, rv) * smoothstep(800.0, 150.0, foot) * smoothstep(120.0, 400.0, dist);
    country = mix(country, uOcean, wet);
    // the forest floor: moss, soil, fallen leaves, at every scale the pixel resolves
    float v = 0.0;
    sc = 0.003;
    for (int i = 0; i < 7; i++) {
        float k = smoothstep(sc * 0.7, sc * 0.12, foot);
        v += (vnoise(p.xz / sc + float(i) * 17.3) - 0.5) * k * (i == 0 ? 0.6 : 1.0);
        sc *= 3.4;
    }
    float tone = smoothstep(-0.05, 0.05, v);
    vec3 floorC = mix(uSoil, uMoss, tone);
    if (foot < 0.02) {
        // scale cues for the macro start: fallen leaves (a midrib, side veins, a darker curled rim), pine needles,
        // round soil grains, all far bigger than the first threads
        for (int L = 0; L < 2; L++) {
            float s = L == 0 ? 0.035 : 0.011;
            vec2 cell = floor(p.xz / s);
            vec2 o = (cell + 0.5 + (hash2(cell + float(L) * 5.0) - 0.5) * 0.6) * s;
            float a = hash(cell + 2.3) * 6.2832;
            vec2 q = p.xz - o;
            q = vec2(cos(a) * q.x + sin(a) * q.y, -sin(a) * q.x + cos(a) * q.y) / s;
            float on = step(hash(cell + 7.1 + float(L)), 0.28);
            float taper = 1.0 - 0.35 * q.x * 2.6;                        // pointed at the tip, broad at the stalk
            float lr = length(q * vec2(2.6, 5.2 / max(taper, 0.3)));
            float leaf = smoothstep(1.0, 0.85, lr) * on;
            float side = abs(fract(q.x * 7.0 - abs(q.y) * 9.0) - 0.5);
            float vein = max(smoothstep(0.03, 0.0, abs(q.y)), smoothstep(0.06, 0.0, side) * smoothstep(0.17, 0.02, abs(q.y)) * 0.6) * leaf;
            float stalk = smoothstep(0.025, 0.0, abs(q.y)) * step(-0.52, q.x) * step(q.x, -0.36) * on;
            vec3 lc = uLitter * (0.6 + 0.8 * hash(cell + 3.3)) * (1.0 - 0.35 * smoothstep(0.7, 1.0, lr));
            float kL = smoothstep(s * 0.5, s * 0.1, foot);
            floorC = mix(floorC, lc * (1.0 - 0.4 * vein), max(leaf, stalk) * kL);
        }
        for (int L = 0; L < 2; L++) {
            float s = L == 0 ? 0.045 : 0.028;
            vec2 cell = floor(p.xz / s + float(L) * 0.37);
            vec2 o = (cell + 0.5 + (hash2(cell + 11.0 + float(L)) - 0.5) * 0.7 - float(L) * 0.37) * s;
            float a = hash(cell + 5.9 + float(L)) * 6.2832;
            vec2 q = p.xz - o;
            q = vec2(cos(a) * q.x + sin(a) * q.y, -sin(a) * q.x + cos(a) * q.y);
            float on = step(hash(cell + 9.4 + float(L)), 0.45);
            float hl = s * 0.42, w = 0.00045;                          // a needle some 4 cm long, under a millimeter wide
            float dq = length(vec2(max(abs(q.x) - hl, 0.0), q.y));
            float aa = max(foot, 1e-5);
            float nd = smoothstep(w + aa, w - aa, dq) * on * smoothstep(w * 4.0, w * 0.8, foot);
            float ridge = smoothstep(w * 0.6, 0.0, abs(q.y + w * 0.25));
            vec3 nc = mix(uLitter * 0.55, uLitter * 1.15, hash(cell + 1.9)) * (0.8 + 0.4 * ridge);
            floorC = mix(floorC, nc, nd);
        }
        vec2 gc = floor(p.xz / 0.0012);
        vec2 go = (gc + 0.5 + (hash2(gc + 4.4) - 0.5) * 0.4) * 0.0012;
        float gr = 0.0012 * (0.22 + 0.2 * hash(gc + 8.8));
        vec2 gq = (p.xz - go) / gr;
        float gl = length(gq);
        float grain = step(0.55, hash(gc)) * smoothstep(1.0 + foot / gr, 1.0 - foot / gr, gl) * smoothstep(0.0006, 0.00015, foot);
        float gshade = 0.75 + 0.5 * clamp(-dot(gq, vec2(0.6, 0.6)), -0.5, 0.6);
        floorC = mix(floorC, mix(uSoil, vec3(dot(uSoil, vec3(0.33))) * 1.4, 0.4 + 0.4 * hash(gc + 1.0)) * gshade, grain);
    }
    float kFloor = 1.0 - smoothstep(0.25, 3.0, foot);
    vec3 col = mix(country, floorC, kFloor);
    col = mix(far, col, 1.0 - smoothstep(500.0, 5000.0, foot));
    // towns at night: the village's windows, then towns and cities from high up
    cityK = 0.0;
    if (foot > 2.0 && foot < 300.0 && vd < uVillage.w * 1.3) {
        vec2 hc = floor(p.xz / 16.0);
        vec2 ho = (hc + 0.5 + (hash2(hc) - 0.5) * 0.6) * 16.0;
        float on = step(hash(hc + 4.0), 0.3) * smoothstep(uVillage.w * 1.3, uVillage.w * 0.4, vd);
        cityK += on * smoothstep(max(foot * 0.8, 1.5), 0.0, length(p.xz - ho)) * 1.2;
    }
    if (foot > 1200.0) {
        float town = smoothstep(0.66, 0.82, vnoise3(n * 220.0)) * smoothstep(0.45, 0.6, c0 + 0.1);
        float dots = smoothstep(0.74, 0.8, vnoise3(n * 1300.0));
        cityK += town * dots * smoothstep(1200.0, 4000.0, foot) * 0.9;
    }
    vec3 ocean = mix(uOcean, uOcean * vec3(0.55, 0.75, 1.0), smoothstep(0.47, 0.3, c));
    return mix(col, ocean, water);
}
void skyAt(vec3 d, out vec4 fragC, out float fragD) {
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float c0 = dot(uEye.xz, uEye.xz) + uEye.y * (uEye.y + 2.0 * R);
    float alt = c0 / (sqrt(c0 + R * R) + R);
    vec3 up = normalize(oc);
    float b = dot(d, oc);
    float disc = b * b - c0;
    vec3 col;
    float stars = 0.0;
    float depth = 1.0;
    if (disc > 0.0 && b < 0.0) {
        // ---- the planet
        float t = c0 / (-b + sqrt(disc));
        vec3 p = uEye + d * t;
        vec3 n = normalize(oc + d * t);
        if (length(p) < 1.0e4) n = normalize(vec3(p.x / R, 1.0, p.z / R));
        float foot = t * uPix;
        float dist = length(p) < 1.0e5 ? length(p.xz) : R * atan(length(n.xz), n.y);
        float water, cityK, wet;
        vec3 alb = landColor(p, n, t, foot, dist, water, cityK, wet);
        // the hills' slopes catching the moon
        float e = max(foot * 1.5, 15.0);
        float h0 = hillsAt(p.xz, foot);
        vec3 nb = normalize(n - vec3(hillsAt(p.xz + vec2(e, 0.0), foot) - h0, 0.0, hillsAt(p.xz + vec2(0.0, e), foot) - h0) / e * (1.0 - water));
        float ndm = dot(nb, uMoonDir);
        vec3 light = mix(uAmbC, uMoonC, smoothstep(-0.05, 0.05, ndm) * 0.55 + smoothstep(0.24, 0.32, ndm) * 0.45);
        light += (uMoonC * 0.9 + 0.08) * smoothstep(2.0e4, 1.5e6, alt);   // the night side seen from space, softly lit
        float ndl = dot(n, uSunDir);
        float day = smoothstep(-0.06, 0.12, ndl);
        light = mix(light, vec3(1.25, 1.18, 1.05), day);
        col = alb * light;
        col += water * vec3(1.0, 0.9, 0.75) * pow(max(dot(reflect(d, n), uSunDir), 0.0), 40.0) * 0.6 * day;
        // the moon on the water
        float mg = pow(max(dot(reflect(d, nb), uMoonDir), 0.0), 60.0);
        col += uMoonC * mg * (wet + water * 0.5) * 1.6;
        // the network's glow under the land, in patches
        float pr = dist / max(uReach, 1e-6);
        float patchy = 0.55 + 0.9 * fbm(p.xz / max(uReach * 0.35, 1e-6) + 3.0);
        float pool = exp(-sq(pr / 0.85)) * patchy * uPoolK * (1.0 - day);
        col += (alb * 3.0 + 0.04) * uPoolC * pool;
        // the mushrooms' caps lighting the ground around them
        for (int i = 0; i < 6; i++) {
            if (uLightP[i].w <= 0.0) continue;
            vec3 L = uLightP[i].xyz - p;
            float dl = length(L);
            float rr = uLightP[i].w;
            float lam = max(dot(n, L / dl), 0.0) * 0.75 + 0.25;
            col += (alb * 2.5 + 0.015) * uLightC[i].rgb * lam / (1.0 + sq(dl / rr)) * smoothstep(rr * 9.0, rr * 4.0, dl);
        }
        col += uCity * cityK * (1.0 - day) * (1.0 - water);
        // the air between: the aerial perspective, the limb's glow
        float od = 1.0e-5 * t * airDensity(alt, 0.0);
        vec3 hc = mix(mix(uMistC, uSkyLow, 0.5), uSkyLow * 1.6 + vec3(0.25, 0.35, 0.5) * 0.4, day);
        col = mix(hc * mix(1.0, 0.4, smoothstep(1e4, 3e5, alt) * (1.0 - day)), col, exp(-od));
        col = mix(uMistC, col, exp(-uMistK * t));
        vec4 clip = uVP * vec4(p - uEye, 1.0);
        depth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    } else {
        // ---- the sky: the night's air along the ray (its lowest point), space beyond
        float hc = b < 0.0 ? -disc / (sqrt(R * R - disc) + R) : alt;
        float air = exp(-hc / 18000.0);
        float dip = atan(sqrt(alt * (2.0 * R + alt)), R);
        float el = asin(clamp(dot(d, up), -1.0, 1.0)) + dip;
        float zk = smoothstep(0.0, 0.8, el);
        vec3 zen = mix(uSkyTop, uSkyTop * vec3(0.3, 0.35, 0.6), smoothstep(2000.0, 30000.0, alt));
        vec3 sky = mix(uSkyLow, zen, pow(zk, 0.6));
        // a dusk glow along the horizon, toward the sun's side
        vec3 hz = normalize(d - up * dot(d, up) + 1e-5);
        vec3 sh = normalize(uSunDir - up * dot(uSunDir, up) + 1e-5);
        float dusk = (0.35 + 0.65 * pow(max(dot(hz, sh), 0.0), 2.0)) * exp(-max(el, 0.0) * 9.0);
        sky += uDusk * dusk * (1.0 - smoothstep(5000.0, 40000.0, alt));
        float a = clamp(pow(air, 0.55) * 1.15, 0.0, 1.0);
        vec3 sp = uSpaceC;
        float nb = fbm3(d * 2.2 + 4.0);
        float neb = smoothstep(0.45, 0.8, nb) * (0.35 + 0.65 * smoothstep(0.3, 0.9, fbm3(d * 5.0 + 1.0)));
        sp += mix(uNebulaC, uNetC * 0.5, fbm3(d * 3.7 + 9.0)) * neb * 0.12;
        col = mix(sp, sky, a);
        // the limb, glowing: airglow on the night side, a blue crescent on the day side
        vec3 tn = normalize(oc - d * b);
        float lday = smoothstep(-0.25, 0.25, dot(tn, uSunDir));
        float limb = exp(-hc / 7000.0) * smoothstep(3000.0, 80000.0, alt) * step(0.0, -b);
        col += mix(uSkyLow * 2.0 + uDusk * 0.8, vec3(0.45, 0.7, 1.0) * 1.6, lday) * limb * (0.8 + 1.2 * lday);
        stars += (uStars + (1.0 - a)) * (1.0 - a * 0.8) * (1.0 - 0.85 * uMacro);
        // the tree line along the horizon (seen from near the ground)
        if (uHills > 0.0) {
            float az = atan(d.x, -d.z);
            float h1 = 0.012 + 0.02 * fbm(vec2(az * 2.6, 1.0)) + 0.01 * fbm(vec2(az * 9.0, 7.0));
            float crowns = vnoise(vec2(az * 140.0, 3.0));
            h1 += 0.012 * crowns * crowns + 0.006 * vnoise(vec2(az * 420.0, 5.0));
            float h2 = 0.004 + 0.012 * fbm(vec2(az * 4.3 + 9.0, 3.0)) + 0.008 * sq(vnoise(vec2(az * 230.0, 9.0)));
            float aa = mix(uPix * 1.2, 0.03, uMacro);   // out of focus behind a macro shot
            float m1 = smoothstep(h1 + aa, h1 - aa, el) * uHills, m2 = smoothstep(h2 + aa, h2 - aa, el) * uHills;
            m1 *= 1.0 - 0.45 * uMacro; m2 *= 1.0 - 0.3 * uMacro;
            vec3 farC = mix(uMistC, uSkyLow, 0.35) * 0.8;
            vec3 nearC = uMistC * 0.45;
            col = mix(col, farC, m1);
            col = mix(col, nearC, m2);
            stars *= 1.0 - max(m1, m2);
        }
        // the sun (only from space, past the planet's limb)
        float sd = dot(d, uSunDir);
        float ang = acos(clamp(sd, -1.0, 1.0));
        float sunK = smoothstep(2.0e4, 2.0e5, alt);
        col += vec3(1.0, 0.92, 0.8) * (exp(-ang / 0.035) * 0.5 + smoothstep(0.009, 0.007, ang) * 2.5) * sunK;
    }
    // the moon: a big, soft-edged sphere, its terminator crisp, its craters
    if (depth >= 1.0) {
        vec3 mo = uEye - uMoon.xyz;
        float mb = dot(d, mo);
        float md = mb * mb - (dot(mo, mo) - uMoon.w * uMoon.w);
        vec3 tm = normalize(uMoon.xyz - uEye);
        float mang = acos(clamp(dot(d, tm), -1.0, 1.0)), mrad = asin(clamp(uMoon.w / length(mo), 0.0, 1.0));
        col += uMoonC * 0.35 * exp(-max(mang - mrad, 0.0) / (mrad * 1.6)) * (1.0 - 0.6 * smoothstep(1e4, 1e5, alt));
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
                crater += smoothstep(s, s * 0.7, rr) * 0.3 - smoothstep(s * 1.15, s, rr) * smoothstep(s * 0.85, s, rr) * 0.2;
            }
            vec3 alb = mix(vec3(1.0), vec3(0.72, 0.74, 0.86), smoothstep(0.5, 0.62, cr) * (1.0 - 0.8 * uMacro)) * (1.0 - crater * 0.5 * (1.0 - uMacro));
            float ml = smoothstep(-0.04 - 0.4 * uMacro, 0.04 + 0.4 * uMacro, dot(mn, uMoonLit));
            vec3 mc = alb * mix(uSkyTop * 0.6 + 0.01, uMoonC * 1.5 + 0.12, ml);
            float edge = smoothstep(0.0, mix(0.1, 0.7, uMacro), -dot(mn, d)) * (1.0 - 0.35 * uMacro);
            col = mix(col, mc, edge);
            stars = 0.0;
        }
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

// ---------------------------------------------------------------- the threads
// One ribbon per thread, facing the camera across its length, lying just over the ground: revealed from its base
// to its tip against the growth (the tip glowing while it grows), old cords thickening, never thinner than a pixel
// (a thinner one fades instead). Knots (fusions, branchings) are round dots. Pulses of light run out along the threads'
// pulse coordinate. Additive.
inline const char* THREAD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // A (world), width
layout(location = 2) in vec4 i1; // B (world), kind
layout(location = 3) in vec4 i2; // birth g0, g1, q0, q1
layout(location = 4) in vec4 i3; // hash, class, ln(distance), -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uG, uPx, uNear;
out vec2 vL;
out float vQ;
out float vT;
out float vFog;
flat out vec4 vInfo; // hash, kind, alpha, tip
void main() {
    float kind = i1.w;
    float f = clamp((uG - i2.x) / max(i2.y - i2.x, 1e-4), 0.0, 1.0);
    if (f <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    vec3 A = i0.xyz - uEye;
    vec3 B = (i0.xyz + (i1.xyz - i0.xyz) * f) - uEye;
    float age = uG - i2.y;
    float w = i0.w * (1.0 + 1.6 * clamp(age / 3.5, 0.0, 1.0) * step(kind, 1.5) * (0.4 + 0.6 * i3.y));
    vec3 M = (A + B) * 0.5;
    float dist = length(M);
    float px = uPx * dist;
    float wp = w / px;
    if (wp < (kind > 1.5 && kind < 2.5 ? 0.25 : 0.07)) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float alpha = pow(clamp(wp / 0.8, 0.0, 1.0), 0.85);
    float hw = max(w, px * 0.6) * 2.2;
    vec3 pw;
    if (kind > 3.5) {
        pw = A + (uCamR * aPos.x + uCamU * aPos.y) * hw * 1.3;
        vL = aPos;
        vT = 1.0;
    } else {
        vec3 dir = B - A;
        float len = length(dir);
        dir = len > 1e-12 ? dir / len : uCamR;
        vec3 side = cross(dir, M);
        float sl = length(side);
        side = sl > 1e-12 ? side / sl : uCamU;
        float along = aPos.x * 0.5 + 0.5;
        pw = A + (B - A) * along + side * aPos.y * hw;
        vL = vec2(aPos.x, aPos.y);
        vT = along;
    }
    vQ = mix(i2.z, mix(i2.z, i2.w, f), vT);
    float tip = (1.0 - smoothstep(0.0, 0.45, age)) * (kind > 3.5 ? 0.6 : 1.0);
    alpha *= smoothstep(0.3 * uNear, 0.65 * uNear, dist);  // none right in front of the lens
    vInfo = vec4(i3.x, kind, alpha, tip);
    vFog = exp(-uMistK * dist);
    gl_Position = uVP * vec4(pw * 0.995, 1.0);
}
)";
inline const char* THREAD_FS = R"(#version 330 core
in vec2 vL;
in float vQ;
in float vT;
in float vFog;
flat in vec4 vInfo;
out vec4 frag;
uniform vec3 uThreadC, uTipC, uPulseC;
uniform float uBright, uOver;
uniform vec4 uPulse[6];   // q, amp, width, fraction of the threads
uniform vec4 uWave;       // the drop's wave: q, amp, width, -
void main() {
    float kind = vInfo.y;
    float s = kind > 3.5 ? length(vL) : abs(vL.y);
    if (s > 1.0) discard;
    float prof = exp(-sq(s * 2.4) * 1.4) + 0.22 * exp(-sq(s * 1.2));
    float tip = vInfo.w * (kind > 3.5 ? 1.0 : smoothstep(0.5, 1.0, vT));
    float p = 0.0;
    for (int i = 0; i < 6; i++) {
        if (uPulse[i].y <= 0.0) continue;
        float on = step(fract(vInfo.x * 7.31 + float(i) * 0.37), uPulse[i].w);
        p += uPulse[i].y * exp(-sq((vQ - uPulse[i].x) / uPulse[i].z)) * on;
    }
    float wave = uWave.y * (exp(-sq((vQ - uWave.x) / uWave.z)) + 0.12 * smoothstep(uWave.x + 0.2, uWave.x - 1.5, vQ));
    float knot = kind > 3.5 ? 1.2 : kind > 1.5 && kind < 2.5 ? 0.55 : 1.0;
    float lb = 0.45 + 1.3 * pow(fract(vInfo.x * 13.7), 2.5);   // a few bright cords, a fine web of faint threads
    vec3 c = mix(uThreadC, uTipC, clamp(0.05 + 0.3 * exp(-sq(s * 5.0)) * lb + tip, 0.0, 1.0));
    c = mix(c, uPulseC, clamp(p + wave, 0.0, 1.0));
    float v = prof * vInfo.z * knot * lb * (1.0 + 2.2 * tip + 2.5 * p + 2.0 * wave) * uBright;
    v *= 1.0 - 0.85 * uOver;
    frag = vec4(c * v * vFog * (1.0 - uBridge), 0.0);
}
)";

// ---------------------------------------------------------------- the mushrooms
// Each one turned on a lathe in the vertex shader (u around, v along its profile: the stem from its foot, the gills
// under the cap, the cap from its rim to its top), its shape from its type and how far its cap has opened (a pin, an
// egg, an open cap): 0 a parasol (a broad flat cap with an umbo), 1 a dome with spots, 2 a slender bell.
inline const char* SHROOM_VS = R"(#version 330 core
layout(location = 0) in vec2 aUV;
layout(location = 1) in vec4 m0; // foot (world), height (m)
layout(location = 2) in vec4 m1; // up, cap radius (in heights)
layout(location = 3) in vec4 m2; // cap color, type
layout(location = 4) in vec4 m3; // open, glow, wilt, seed
layout(location = 5) in vec4 m4; // lean direction, climb (the drop's light up the stem)
uniform mat4 uVP;
out vec3 vW;
out vec3 vN;
out vec2 vUV;
out float vRegion;   // 0..1 the stem, 1..2 the gills, 2..3 the cap
flat out vec4 vCap;
flat out vec4 vInst; // open, glow, wilt, seed
flat out float vClimb;
vec2 profile(float v, float type, float open, float capR) {
    float rs = type < 0.5 ? 0.045 : type < 1.5 ? 0.075 : 0.035;
    float cr = mix(rs * 1.5, capR, open);
    float stemTop = type < 0.5 ? mix(0.5, 0.86, open) : type < 1.5 ? mix(0.45, 0.72, open) : mix(0.55, 0.8, open);
    float capH = (type < 0.5 ? mix(1.1, 0.32, open) : type < 1.5 ? mix(1.1, 0.7, open) : mix(1.4, 1.15, open)) * cr;
    float rimDrop = (type < 0.5 ? 0.12 : type < 1.5 ? 0.25 : 0.45) * capH;
    float yRim = stemTop - rimDrop;
    if (v < 0.42) {
        float t = v / 0.42;
        return vec2(rs * (1.0 + 0.55 * exp(-t * 9.0)) * (1.0 - 0.18 * t), t * stemTop);
    } else if (v < 0.55) {
        float t = (v - 0.42) / 0.13;
        return vec2(mix(rs * 0.9, cr, t), mix(stemTop, yRim, t * t));
    }
    float t = (v - 0.55) / 0.45;
    float a = t * 1.5708;
    float r, y;
    if (type < 0.5) {
        r = cr * pow(cos(a), 0.8);
        y = yRim + capH * pow(sin(a), 0.55) + capH * 0.35 * smoothstep(0.7, 1.0, t);
    } else if (type < 1.5) {
        r = cr * cos(a) * (1.0 + 0.08 * sin(a * 2.0));
        y = yRim + capH * sin(a);
    } else {
        r = cr * (1.0 - t) * (1.0 - 0.35 * t * (1.0 - t));
        y = yRim + capH * (1.0 - sq(1.0 - t)) * 1.05;
    }
    return vec2(r, y);
}
void main() {
    float type = m2.w, open = m3.x, wilt = m3.z;
    float h = m0.w;
    vec3 up = m1.xyz;
    vec3 bx = normalize(cross(up, abs(up.y) < 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 bz = cross(bx, up);
    float v = aUV.y, u = aUV.x;
    float phi = u * 6.2831853;
    vec2 P = profile(v, type, open, m1.w);
    vec2 P2 = profile(min(v + 0.004, 1.0), type, open, m1.w);
    vec2 P1 = profile(max(v - 0.004, 0.0), type, open, m1.w);
    vec2 tg = P2 - P1;
    vec2 n2 = normalize(vec2(tg.y, -tg.x) + 1e-6);
    if (v >= 0.999) n2 = vec2(0.0, 1.0);
    // wilting: the cap droops, the stem sags
    P.y *= 1.0 - 0.55 * wilt;
    vec3 radial = bx * cos(phi) + bz * sin(phi);
    vec3 lean = m4.xyz * (sq(P.y) * (0.25 + 0.8 * wilt));
    vec3 p = m0.xyz + (radial * P.x + up * P.y + lean) * h;
    vW = p;
    vN = radial * n2.x + up * n2.y;
    vUV = vec2(u, v);
    vRegion = v < 0.42 ? v / 0.42 : v < 0.55 ? 1.0 + (v - 0.42) / 0.13 : 2.0 + (v - 0.55) / 0.45;
    vCap = m2;
    vInst = m3;
    vClimb = m4.w;
    gl_Position = uVP * vec4(p - uEye, 1.0);
}
)";
inline const char* SHROOM_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
in vec2 vUV;
in float vRegion;
flat in vec4 vCap;
flat in vec4 vInst;
flat in float vClimb;
out vec4 frag;
uniform vec3 uStemC;
uniform float uCapGlow;
void main() {
    vec3 n = normalize(vN);
    vec3 vd = normalize(uEye - vW);
    if (dot(n, vd) < 0.0) n = -n;
    float fres = 1.0 - abs(dot(n, vd));
    float ndl = dot(n, uMoonDir);
    float glow = vInst.y * uCapGlow;
    float type = vCap.w;
    vec3 capC = vCap.rgb;
    vec3 col;
    float seed = vInst.w;
    if (vRegion < 1.0) {
        // the stem: pale, fibrous, lit from above by its own cap
        float t = vRegion;
        float fib = 0.85 + 0.15 * vnoise(vec2(vUV.x * 60.0, t * 6.0 + seed * 9.0));
        vec3 alb = uStemC * fib;
        col = alb * toonLight(ndl, 1.0) * 0.5;
        col += mix(uStemC, capC, 0.45) * glow * (0.12 + 0.75 * smoothstep(0.35, 1.0, t)) * fib;
        col += capC * fres * glow * 0.35;
        // the drop's light climbing it
        col += mix(capC, vec3(1.0), 0.5) * exp(-sq((t - vClimb) / 0.12)) * 2.2 * step(0.0, vClimb) * step(vClimb, 1.3);
    } else if (vRegion < 2.0) {
        // the gills: fine radial plates glowing under the cap
        float t = vRegion - 1.0;
        float g = pow(abs(sin(vUV.x * 3.14159 * (type < 1.5 ? 56.0 : 34.0))), 0.6);
        vec3 gc = mix(capC, vec3(1.0), 0.45);
        col = gc * glow * (0.35 + 0.9 * g) * (0.5 + 0.7 * t);
    } else {
        // the cap: translucent, brighter toward its rim and its edges, its own pattern
        float t = vRegion - 2.0;   // 0 the rim .. 1 the top
        float rim = smoothstep(0.55, 0.0, t);
        float e = 0.3 + 0.7 * rim + 0.6 * fres;
        if (type < 0.5) {
            float stri = 0.75 + 0.25 * sin(vUV.x * 6.2832 * 46.0 + vnoise(vec2(vUV.x * 30.0, t * 3.0)) * 3.0);
            e *= mix(1.0, stri, rim);
            e *= 1.0 - 0.35 * smoothstep(0.75, 1.0, t);
        } else if (type < 1.5) {
            vec2 sp = vec2(vUV.x * 14.0, t * 5.0) + seed * 11.0;
            vec2 ci = floor(sp);
            vec2 cf = fract(sp) - 0.5 - (hash2(ci) - 0.5) * 0.4;
            float spot = smoothstep(0.24, 0.17, length(cf * vec2(1.0, 1.0 + t))) * step(hash(ci + 3.0), 0.55) * step(0.1, t);
            e += spot * 1.3;
        } else {
            e *= 0.85 + 0.15 * sin(vUV.x * 6.2832 * 18.0);
            e += smoothstep(0.85, 1.0, t) * 0.6;
        }
        vec3 alb = capC;
        col = alb * glow * e;
        col += mix(alb, vec3(1.0), 0.6) * toonLight(ndl, 1.0) * 0.12;
        col += uMoonC * smoothstep(0.7, 0.95, fres) * smoothstep(0.0, 0.4, ndl) * 0.25;
    }
    // dying at game over: the light drains
    col *= 1.0 - 0.75 * vInst.z;
    frag = vec4(atmos(col, vW), 1.0);
}
)";

// ---------------------------------------------------------------- puffs: moss, bushes, tree crowns, pebbles
// One billboard per puff: a sphere (a scalloped leafy edge for foliage), its depth written as a sphere's so puffs
// cluster into round masses; toon light from the moon, a cool shade, the network's glow lighting it from below.
inline const char* PUFF_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // center, radius
layout(location = 2) in vec4 i1; // color, seed
layout(location = 3) in vec4 i2; // clump center, alpha
layout(location = 4) in vec4 i3; // kind (0 foliage, 1 moss, 2 pebble, 3 dew drop), glow from below, -, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uPx;
out vec2 vLocal;
out vec3 vCtr;
out vec3 vClumpN;
flat out float vRad;
flat out vec4 vCol;
flat out vec4 vK;
void main() {
    vec3 c = i0.xyz;
    float r = i0.w;
    float dist = length(c - uEye);
    float s = max(r, uPx * dist * 0.6) * 1.15;
    vec3 w = c + (uCamR * aPos.x + uCamU * aPos.y) * s;
    vLocal = aPos * 1.15;
    vCtr = c;
    vRad = s / 1.15;
    vClumpN = normalize(c - i2.xyz + vec3(0.0, 1e-9, 0.0));
    vCol = i1;
    vK = vec4(i3.x, i3.y, i2.w, 0.0);
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* PUFF_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vCtr;
in vec3 vClumpN;
flat in float vRad;
flat in vec4 vCol;
flat in vec4 vK;
out vec4 frag;
uniform mat4 uVP;
uniform vec3 uCamR, uCamU, uRimC;
void main() {
    vec2 l = vLocal;
    float rr = length(l);
    float seed = vCol.w;
    float kind = vK.x;
    float th = atan(l.y, l.x);
    float edge = kind > 1.5 ? 1.0 : 1.0 + (kind > 0.5 ? 0.05 : 0.08) * sin(th * (kind > 0.5 ? 11.0 : 7.0) + seed * 40.0) + 0.04 * sin(th * 13.0 + seed * 17.0);
    float aa = max(fwidth(rr), 1e-4);
    float cover = clamp(0.5 - (rr - edge) / aa, 0.0, 1.0) * vK.z;
    if (cover <= 0.01) discard;
    float rn = min(rr / edge, 1.0);
    float z = sqrt(max(1.0 - rn * rn, 0.0));
    vec3 fwd = normalize(uEye - vCtr);
    vec3 nS = normalize(uCamR * l.x / edge + uCamU * l.y / edge + fwd * z);
    vec4 clip = uVP * vec4(vCtr + nS * vRad - uEye, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    vec3 n = normalize(nS + vClumpN * (kind > 1.5 ? 0.2 : 0.8));
    float ndl = dot(n, uMoonDir);
    // painterly strokes shifting the bands
    vec2 sp = l * (kind > 0.5 ? 5.0 : 2.8) + seed * 13.0;
    vec2 ci = floor(sp);
    vec2 f = fract(sp) - 0.5 - (hash2(ci) - 0.5) * 0.5;
    float stroke = smoothstep(0.3, 0.2, length(f * vec2(1.0, kind > 0.5 ? 1.0 : 2.3)));
    float k = ndl + (stroke - 0.3) * (kind > 1.5 ? 0.04 : 0.2);
    float b1 = smoothstep(-0.06, 0.02, k), b2 = smoothstep(0.45, 0.53, k);
    vec3 alb = vCol.rgb;
    vec3 col = mix(uAmbC * alb * 1.4, alb * uMoonC * 0.75, b1);
    col = mix(col, alb * uMoonC * 1.25, b2);
    col *= mix(1.0, 0.6, smoothstep(0.88, 0.98, rn) * (1.0 - b1 * 0.7));
    // the network's glow from below, on its underside and its rim
    float below = smoothstep(0.3, -0.7, n.y);
    col += uRimC * vK.y * (below * 0.6 + smoothstep(0.75, 0.97, rn) * 0.25) * (0.3 + alb * 3.0);
    col += uMoonC * alb * smoothstep(0.85, 0.97, rn) * smoothstep(0.3, 0.7, ndl) * 0.5;
    if (kind > 2.5) {
        // a dew drop: clear water over the moss, the world seen through it upside down (a dark rim, a bright
        // lower crescent of focused light), a crisp highlight of the moon, the network's glow caught in it
        vec2 ld = normalize(vec2(dot(uMoonDir, uCamR), dot(uMoonDir, uCamU)) + 1e-4);
        float hi = smoothstep(0.2, 0.1, length(l / edge - ld * 0.45));
        float cres = smoothstep(0.55, 0.9, rn) * smoothstep(0.1, 0.6, dot(-l / edge, ld));
        vec3 body = mix(alb, vec3(dot(alb, vec3(0.33))), 0.45) * (0.55 + 0.45 * (1.0 - rn)) + uRimC * vK.y * 0.15;
        col = body * (1.0 - 0.6 * smoothstep(0.75, 1.0, rn)) + alb * cres * 0.9 + uRimC * vK.y * cres * 0.8 + uMoonC * hi * 2.2;
        cover *= 0.92;
    }
    frag = vec4(atmos(col, vCtr) * cover, cover);
}
)";

// ---------------------------------------------------------------- lights and things in the air
// World billboards (meters), premultiplied: 0 an orb of light (additive), 2 a spark, 3 a grass tuft, 6 a satellite,
// 7 a hot-air balloon glowing from its burner, 8 a streak, 9 a star burst, 10 a stalk (a trunk, a moss capsule's
// seta), 11 a house with lit windows, 12 an airliner's lights.
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, size (meters, radius)
layout(location = 2) in vec4 i1; // rgb, alpha
layout(location = 3) in vec4 i2; // mode, seed, spin, param
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uPx;
out vec2 vLocal;
flat out vec4 vB;
flat out vec4 vC;
out float vDim;
out float vFog;
void main() {
    vec3 rel = i0.xyz - uEye;
    float d = length(rel);
    vLocal = aPos;
    vB = i1;
    vC = i2;
    vFog = fogOf(i0.xyz);
    int mode = int(i2.x + 0.5);
    float px = i0.w / max(uPx * d, 1e-30);
    vec3 upW = normalize(i0.xyz + vec3(0.0, 6.371e6, 0.0));
    if (mode == 6 || mode == 7 || mode == 12) {
        vDim = smoothstep(0.6, 2.0, px);
        vec3 w = rel + (uCamR * aPos.x + uCamU * aPos.y) * max(i0.w, uPx * d * (mode == 12 ? 3.0 : 1.6));
        gl_Position = uVP * vec4(w, 1.0);
        return;
    }
    if (mode == 3 || mode == 10 || mode == 11) {
        // standing on its foot, facing the camera across
        vDim = smoothstep(0.8, 3.0, px);
        vec3 side = normalize(uCamR - upW * dot(uCamR, upW));
        float wk = mode == 3 ? 1.2 : mode == 10 ? i2.z : 1.5;
        vec3 w = rel + side * aPos.x * i0.w * wk + upW * (aPos.y + 1.0) * i0.w;
        gl_Position = uVP * vec4(w, 1.0);
        return;
    }
    float minS = uPx * d * (mode == 2 ? 2.2 : 1.6);
    float s = max(i0.w, minS);
    vDim = min(1.0, sq(i0.w / max(minS, 1e-30)) + 0.35);
    vec3 w = rel + (uCamR * aPos.x + uCamU * aPos.y) * s;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vB;
flat in vec4 vC;
in float vDim;
in float vFog;
out vec4 frag;
void main() {
    int mode = int(vC.x + 0.5);
    float r = length(vLocal);
    if (mode == 6) {
        vec2 q = vLocal;
        float body = step(abs(q.x), 0.16) * step(abs(q.y), 0.2);
        float panel = step(abs(q.x), 0.92) * step(0.24, abs(q.x)) * step(abs(q.y), 0.13);
        float strut = step(abs(q.y), 0.025) * step(abs(q.x), 0.9);
        vec3 pc = mix(vec3(0.1, 0.2, 0.55), vec3(0.3, 0.5, 0.8), step(0.5, fract(q.x * 6.0)));
        vec3 col = body > 0.5 ? vec3(0.8, 0.62, 0.25) : panel > 0.5 ? pc : vec3(0.6);
        float a = max(max(body, panel), strut) * vB.a * vDim;
        col += vec3(1.0) * exp(-dot(q, q) * 30.0) * (0.5 + 0.5 * sin(vC.z * 3.0));
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 7) {
        // a hot-air balloon at night, glowing from its burner
        vec2 q = vLocal - vec2(0.0, 0.15);
        float env = length(q * vec2(1.0, 0.92 + 0.25 * step(q.y, 0.0) * (-q.y)));
        float in_ = smoothstep(0.66, 0.62, env + max(-q.y - 0.35, 0.0) * 0.9);
        float stripe = step(0.5, fract((q.x / max(sqrt(max(0.42 - q.y * q.y, 0.02)), 0.1)) * 2.5 + 0.25));
        vec3 col = mix(vB.rgb, vec3(1.0, 0.92, 0.75), stripe * 0.6);
        float burn = 0.75 + 0.25 * sin(vC.z * 13.0) * sin(vC.z * 7.3);
        col *= (0.18 + 1.1 * smoothstep(0.55, -0.6, q.y) * burn) * (0.75 + 0.35 * smoothstep(0.4, -0.4, q.x));
        float basket = step(abs(vLocal.x), 0.1) * step(abs(vLocal.y + 0.83), 0.07);
        float flame = exp(-dot((vLocal - vec2(0.0, -0.66)) * vec2(9.0, 6.0), (vLocal - vec2(0.0, -0.66)) * vec2(9.0, 6.0))) * burn;
        col = basket > 0.5 ? vec3(0.12, 0.07, 0.04) : col;
        float a = max(in_, basket) * vB.a * vDim;
        frag = vec4((col * a + vec3(1.0, 0.7, 0.3) * flame * 1.5 * vB.a) * vFog, a);
        return;
    }
    if (mode == 12) {
        // an airliner at night: its lights (red and green at the wingtips, a white strobe, a beacon), the hull a
        // faint shape
        vec2 q = vLocal;
        float hull = step(abs(q.y), 0.05) * step(abs(q.x), 0.55) + step(abs(q.x + 0.05), 0.08) * step(abs(q.y), 0.5) * step(abs(q.y), 0.6 - abs(q.x) * 2.0);
        float blink = step(0.85, fract(vC.z * 0.9 + vC.y));
        vec3 c = vec3(1.0, 0.25, 0.2) * exp(-dot(q - vec2(-0.05, 0.52), q - vec2(-0.05, 0.52)) * 300.0) * 2.0
               + vec3(0.25, 1.0, 0.4) * exp(-dot(q - vec2(-0.05, -0.52), q - vec2(-0.05, -0.52)) * 300.0) * 2.0
               + vec3(1.0) * exp(-dot(q - vec2(0.5, 0.0), q - vec2(0.5, 0.0)) * 200.0) * 2.5
               + vec3(1.0, 0.3, 0.2) * exp(-dot(q, q) * 200.0) * 3.0 * blink;
        float a = clamp(hull, 0.0, 1.0) * 0.5 * vB.a * vDim;
        frag = vec4(vB.rgb * a + c * vB.a * vFog, a);
        return;
    }
    if (mode == 8) {
        float v = exp(-sq(vLocal.x * 3.0)) * smoothstep(1.0, 0.2, abs(vLocal.y)) * vB.a;
        frag = vec4(vB.rgb * v * vFog, 0.0);
        return;
    }
    if (mode == 3) {
        // grass blades curving, darker at their foot, moonlit tips
        float y = vLocal.y * 0.5 + 0.5;
        float cov = 0.0, tip = 0.0;
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float h = 0.55 + 0.45 * hash1(vC.y * 31.0 + fi);
            float x0 = (hash1(vC.y * 17.0 + fi * 3.1) - 0.5) * 1.1;
            float t = y / h;
            float bend = (hash1(vC.y * 7.0 + fi) - 0.5) * 0.7 + vC.z;
            float x = x0 + bend * t * t;
            float w = 0.11 * (1.0 - t);
            float inside = step(t, 1.0) * smoothstep(w, w * 0.6, abs(vLocal.x - x));
            if (inside > cov) { cov = inside; tip = t; }
        }
        vec3 c = mix(vB.rgb * 0.45, vB.rgb * 1.3 + uMoonC * 0.05, tip);
        c = mix(uMistC, c, vFog);
        float a = cov * vB.a * vDim;
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 10) {
        // a stalk: a tapered trunk or seta from its foot, a capsule at its top (vC.w > 0)
        float y = vLocal.y * 0.5 + 0.5;
        float bend = vC.z * 0.0;
        float w = mix(0.9, 0.35, y);
        float stalk = smoothstep(w, w * 0.6, abs(vLocal.x)) * step(y, 1.0);
        vec3 c = vB.rgb * (0.7 + 0.45 * smoothstep(0.6, -0.6, vLocal.x / max(w, 1e-3)));
        if (vC.w > 0.5) {
            stalk = smoothstep(0.22, 0.12, abs(vLocal.x)) * step(y, 0.86);
            vec2 cq = (vLocal - vec2(0.0, 0.84)) / vec2(0.55, 0.16);
            float cap = smoothstep(1.0, 0.8, length(cq));
            c = mix(c, vB.rgb * 1.6 + vec3(0.04), cap);
            stalk = max(stalk, cap);
        }
        c = mix(uMistC, c, vFog);
        float a = stalk * vB.a * vDim;
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 11) {
        // a house: walls, a pitched roof, warm windows
        vec2 q = vLocal;
        float wall = step(abs(q.x), 0.8) * step(q.y, 0.15);
        float roof = step(q.y, 0.15 + (0.8 - abs(q.x)) * 0.85) * step(0.15, q.y) * step(abs(q.x), 0.92);
        float chim = step(abs(q.x - 0.45), 0.07) * step(q.y, 0.75) * step(0.3, q.y);
        float sh = max(max(wall, roof), chim);
        vec3 c = vB.rgb * (wall > 0.5 ? 1.0 : 0.6);
        vec2 wg = vec2(q.x * 2.5, (q.y + 0.4) * 3.0);
        vec2 wi = floor(wg);
        vec2 wf = fract(wg);
        float win = step(abs(wf.x - 0.5), 0.2) * step(abs(wf.y - 0.5), 0.22) * wall * step(-0.85, q.y);
        float lit = step(hash(wi + vC.y * 31.0), 0.65);
        c += vec3(1.0, 0.72, 0.36) * win * lit * 2.2;
        c = mix(uMistC, c, vFog);
        float a = sh * vB.a * vDim;
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 9) {
        float rays = exp(-abs(vLocal.x) * 22.0) * exp(-abs(vLocal.y) * 2.5) + exp(-abs(vLocal.y) * 22.0) * exp(-abs(vLocal.x) * 2.5);
        float v = exp(-r * r * 30.0) * 1.5 + rays + 0.2 * exp(-r * r * 5.0);
        v *= smoothstep(1.0, 0.7, r);
        frag = vec4(vB.rgb * v * vB.a * vDim * vFog, 0.0);
        return;
    }
    float v = mode == 2 ? exp(-r * r * 22.0) + 0.25 * exp(-r * r * 5.0) : exp(-r * r * 14.0) * 1.2 + 0.4 * exp(-r * r * 3.0);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(vB.rgb * v * vB.a * vDim * vFog * (1.0 - uBridge), 0.0);
}
)";

// ---------------------------------------------------------------- clouds
// Toon cumulus puffs at night (world billboards, meters), sorted far to near: a round, scalloped edge, crisp moonlit
// bands, the network's glow on their bellies.
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, radius
layout(location = 2) in vec4 i1; // seed, alpha, glow from below, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uDepthMax;
out vec2 vLocal;
out vec3 vW;
flat out vec4 vI;
void main() {
    vLocal = aPos;
    vI = i1;
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * i0.w;
    vW = w;
    vec3 rel = w - uEye;
    gl_Position = uVP * vec4(rel, 1.0);
    // its depth from a distance smoothly compressed under uDepthMax (monotonic, ~the true one up close): with the
    // macro start's tiny near plane a puff kilometers away otherwise lands within a step of the far plane's 1.0 and
    // flips in and out against the sky's depth from frame to frame
    float L = max(length(rel), 1e-6);
    vec4 cd = uVP * vec4(rel * (1.0 / (1.0 + L / uDepthMax)), 1.0);
    gl_Position.z = cd.z / cd.w * gl_Position.w;
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
    cover *= clamp(0.5 + (vLocal.y + 0.55) / aa, 0.0, 1.0);
    if (cover < 0.01) discard;
    vec3 fwd = normalize(uEye - vW);
    float rn = min(r / edge, 1.0);
    vec3 nrm = normalize(uCamR * vLocal.x + uCamU * vLocal.y + fwd * sqrt(max(1.0 - rn * rn, 0.0)));
    float ndl = dot(nrm, uMoonDir) + 0.25 * nrm.y;
    float band = smoothstep(-0.12, -0.04, ndl) * 0.55 + smoothstep(0.32, 0.4, ndl) * 0.45;
    vec3 col = mix(uCloudShade, uCloudLit, band);
    col += uNetC * vI.z * smoothstep(0.2, -0.6, vLocal.y) * 0.35;
    col = atmos(col, vW);
    float a = cover * vI.y;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the sea of clouds
// A shell of clouds around the planet at the clouds' height (ray traced per pixel, its depth written): seen from
// above, billowing moonlit tops in crisp bands; from orbit, the planet's weather (bright on the day side).
inline const char* SEA_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU, uSunDir;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uCloudY, uDrift, uFade, uPix;
uniform vec3 uCloudLit, uCloudShade;
const float R = 6.371e6;
float dens(vec3 p, vec3 n, float foot) {
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
    return 0.47 + v / sqrt(max(w, 1e-4)) * 0.22;
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
    vec3 nrm = normalize(n + vec3(-(vx - v), 0.0, -(vz - v)) * 9.0);
    float ndl = dot(nrm, uMoonDir);
    float band = smoothstep(0.0, 0.08, ndl) * 0.5 + smoothstep(0.35, 0.42, ndl + (v - 0.6) * 0.8) * 0.5;
    vec3 col = mix(uCloudShade, uCloudLit, band);
    float day = smoothstep(-0.1, 0.12, dot(n, uSunDir));
    col = mix(col * 0.6, vec3(1.0, 0.98, 0.95), day);
    col = atmos(col, p);
    float a = cover * uFade * mix(0.75, 1.0, day) * smoothstep(0.0, 0.01, -dot(d, normalize(oc)));
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
        // stars fixed on the sky, round on screen however the projection stretches (see GIANT TREE)
        vec3 d = rayDir(vUv);
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

// ---------------------------------------------------------------- motes over the board
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (radius, board units)
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

// ---------------------------------------------------------------- pieces: mycelium felt
// Each piece one shape (its cells merge): a dense felt of glowing threads in the piece's color, woven in three
// directions over a deep translucent body, a bright inner rim. The ghost is the rim alone; a cleared cell frays into
// light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, -, drain
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
// fine threads along one direction, wandering
float fibers(vec2 p, float ang, float freq, float seed) {
    vec2 d = vec2(cos(ang), sin(ang));
    vec2 n = vec2(-d.y, d.x);
    float u = dot(p, n) * freq + vnoise(vec2(dot(p, d) * 2.2, seed)) * 2.2 + vnoise(p * 3.0 + seed) * 0.8;
    return smoothstep(0.75, 1.0, abs(sin(u * 3.14159)));
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
    float rimW = max(aa * 1.6, 0.04);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.55);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.07) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    vec3 col = tint * mix(0.85, 0.5, smoothstep(0.02, 0.36, e));
    vec2 p = vWorld;
    float f = fibers(p, 0.35, 5.0, 1.0) * 0.5 + fibers(p, 2.1, 6.0, 7.0) * 0.4 + fibers(p, 1.2, 9.0, 3.0) * 0.3;
    float fade = smoothstep(0.03, 0.12, e);
    col += mix(tint, uLightC, 0.4) * f * 0.32 * fade;
    vec2 cell = floor(p * 2.0);
    vec2 o = (cell + 0.25 + 0.5 * hash2(cell + 1.7)) / 2.0;
    float knot = exp(-dot(p - o, p - o) / 0.0035) * step(hash(cell + 4.2), 0.45);
    col += hi * knot * (0.25 + 0.1 * sin(uTime * 1.1 + hash(cell) * 30.0)) * fade;
    col += tint * exp(-abs(e - 0.1) / 0.03) * 0.18;
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        float n = vnoise(vWorld * 4.6) * 0.65 + vnoise(vWorld * 10.2) * 0.35;
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
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.25 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace mycelium_shaders

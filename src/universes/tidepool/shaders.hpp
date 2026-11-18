#pragma once
#include <string>
// TIDE POOL's shaders: the world (one continuous zoom through fifteen scales, from the cosmic web to strings, each a
// whole world drawn procedurally in its own frame), the veil behind the well, the sea-glass pieces, bubbles, lettering.
//
// The dive: every stage fills the plane with things of a fixed size in its world (16 times smaller from one stage to
// the next: S = 4 octaves), so zooming makes all of them grow and stream out from the focus (left of the HUD).
// The next stage's things are already there, tiny, at the focus of the current one (our galaxy at a node of the web,
// its star, the planet on its orbit, the rock pool among the tide pools, anemones dotting the pool's floor, the cell
// glowing through every pore of the diatom...); over the stage's last octave and a half the whole next world fades in
// under them. From the universe to the shore the stages nest exactly, so those crossfades are between two drawings of
// one picture. Every edge is anti-aliased against the pixel's size in the stage's units, and patterns fade out before
// they get under a few pixels (no shimmer).

namespace tidepool_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float hash1(float n) { return hash(vec2(n, n * 0.371 + 17.13)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
vec3 hsv(float h, float s, float v) { vec3 k = clamp(abs(fract(h + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0); return v * mix(vec3(1.0), k, s); }
)";

// ------------------------------------------------------------------------------------------------ the world
inline const std::string WORLD_FS_SRC = std::string(R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH, uFocus;
uniform float uPxP;           // p units per pixel of this target
uniform vec4 uLayer[2];       // the stage we are in and the next: stage, local zoom u, scale (stage units per p unit), angle
uniform float uTau;           // the crossfade into the next stage
uniform float uTime, uLayerT0, uLayerT1, uTide, uTideAmp, uLife, uPeak, uKick, uSlow, uBridge, uWorldA;
uniform vec4 uWave;           // the drop's wave: age (s), strength, -, -
uniform vec3 uDeep, uWater, uShallow, uLight, uRock, uSand, uWarm, uWarm2, uWeed, uCool, uGlow, uBridgeCol;
uniform float uLum, uFever, uDepth;
uniform float uEarthT;         // the planet's clock (from orbit to its shore)
uniform vec4 uBands;
// each stage's own clock: it slows as the zoom goes into it, so its life keeps a calm pace on the screen
float gT = 0.0;          // the music's bands (low, low-mid, high-mid, high), quickly smoothed

const float PI = 3.14159265, TAU = 6.2831853;

// ---- small helpers
float fillA(float d, float w) { return 1.0 - smoothstep(-w, w, d); }
float lineA(float d, float hw, float w) { return 1.0 - smoothstep(hw - w, hw + w, abs(d)); }
vec4 over(vec4 top, vec4 bot) { return top + bot * (1.0 - top.a); }
vec4 layerC(vec3 c, float a) { return vec4(c * a, a); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
float sdSegT(vec2 p, vec2 a, vec2 b, out float h) { vec2 pa = p - a, ba = b - a; h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
// the contrast a pattern of period `cell` keeps at a pixel of size px (fades out below ~3 pixels: no shimmer)
float lod(float cell, float px) { return smoothstep(2.0, 5.0, cell / px); }
// a smooth bump around 0
float bump(float x, float w) { return exp(-x * x / (w * w)); }

// Voronoi: (F1, F2 - F1) of a jittered grid, the cell's id
vec3 voronoi(vec2 p, float jit, float t, out vec2 id) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    id = i;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 h = hash2(i + g);
            vec2 o = g + 0.5 + jit * 0.5 * sin(t * (0.6 + h) + 6.2831 * h) - f;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; id = i + g; } else if (d < d2) d2 = d;
        }
    d1 = sqrt(d1); d2 = sqrt(d2);
    return vec3(d1, d2 - d1, 0.0);
}
// Water caustics: two warped Voronoi nets, bright along the cells' edges (a pattern of period ~1 at scale 1)
float caustic(vec2 p, float t, float px) {
    vec2 id;
    vec2 w = p + 0.25 * vec2(sin(p.y * 1.7 + t * 0.6), sin(p.x * 1.9 - t * 0.5));
    vec3 a = voronoi(w * 1.6, 1.0, t * 0.9, id);
    vec3 b = voronoi(w * 2.3 + 7.3, 1.0, t * 1.1, id);
    float pw = px * 2.0;
    float ea = exp(-a.y / max(0.07, pw * 1.6)), eb = exp(-b.y / max(0.06, pw * 2.3));
    float c = ea * 0.6 + eb * 0.5 + ea * eb * 0.9;
    return mix(0.42, c, lod(0.6, px));
}

// Log-polar lattice: (ln r, angle) scaled so that n cells go round. Cells are square on screen and as big as their
// distance from the focus times 2 pi / n: the same density of things at every zoom.
vec2 toLP(vec2 q, float n) { return vec2(log(max(length(q), 1e-6)), atan(q.y, q.x)) * (n / TAU); }
vec2 fromLP(vec2 c, float n) { float k = TAU / n; return exp(c.x * k) * vec2(cos(c.y * k), sin(c.y * k)); }

// ======================================================================= helpers
// the bed seen through d of clear sea water
vec3 waterOver(vec3 bed, float d) {
    vec3 T = exp(-d * vec3(5.0, 1.9, 1.4));
    return bed * T + uWater * (1.0 - T);
}
// slabs of rock split by fissures: (height offset, fissure darkness)
vec2 slabs(vec2 q, float px, float sc) {
    vec2 id;
    vec3 v = voronoi(q * sc + 0.3 * vec2(fbm(q * 1.7), fbm(q * 1.7 + 4.0)), 0.7, 0.0, id);
    float w = max(0.018, px * sc * 1.2);
    // only some of the cells' edges crack: irregular fissures, not tiles
    float on = smoothstep(0.45, 0.6, fbm(q * 3.1 + 11.0));
    float fis = exp(-v.y / w) * on * min(1.0, 0.02 / w * 1.5 + 0.3);
    return vec2(-0.02 * fis + 0.015 * (hash(id) - 0.5), fis);
}
float poolH(vec2 q, float px) {
    vec2 w = q + 0.5 * vec2(fbm(q * 0.6 + 3.1), fbm(q * 0.6 - 1.7)) - 0.25;
    float basin = smoothstep(0.5, 2.3, length((w - vec2(0.15, 0.05)) * vec2(0.72, 1.15)));
    float ridges = 1.0 - abs(2.0 * fbm(w * 1.2 + 2.0) - 1.0);
    float h = basin * (0.42 + 0.42 * ridges + 0.18 * fbm(w * 3.5)) + 0.035 * fbm(w * 9.0) * (0.3 + basin);
    h += slabs(q, px, 2.2).x * smoothstep(0.3, 0.7, basin);
    return h;
}
vec3 sandBed(vec2 q, float px, float t, float scale) {
    // fine sand with soft ripple marks, pebbles and gravel in the shallows
    vec2 s = q * scale;
    float rip = sin(s.x * 5.0 + s.y * 1.6 + 2.5 * fbm(s * 0.7)) * 0.5 + 0.5;
    vec3 c = uSand * (0.7 + 0.1 * rip * lod(1.2 / scale, px) + 0.16 * (fbm(s * 2.3) - 0.5));
    vec2 id;
    vec3 v = voronoi(s * 2.0, 0.9, 0.0, id);
    float hp = hash(id);
    if (hp > 0.5) {
        float rad = 0.28 + 0.14 * hash(id + 3.0);
        float pw = px * scale * 2.0;
        float inside = fillA(v.x - rad, pw);
        vec3 pc = mix(uRock * 1.7, mix(uWarm2, uSand, 0.6), hash(id + 5.0)) * (0.65 + 0.4 * hash(id + 9.0));
        float shade = 0.7 + 0.45 * (1.0 - v.x / rad);
        float k = lod(0.35 / scale, px);
        c *= 1.0 - 0.3 * fillA(v.x - rad - 0.06, pw * 2.0 + 0.05) * k;
        c = mix(c, pc * shade, inside * k);
    }
    return c;
}
// a beadlet anemone at c, radius R: a red column, blue beads at its rim, three whorls of soft tentacles
vec4 anemone(vec2 q, vec2 c, float R, float seed, float px, float t) {
    vec2 d = (q - c) / R;
    float rho = length(d);
    if (rho > 1.25) return vec4(0.0);
    float pw = px / R;
    float ang = atan(d.y, d.x);
    float open = 0.9 + 0.05 * uTide * uTideAmp + 0.03 * sin(t * 0.3 + seed * 9.0);
    // its soft shadow on the bed
    vec4 acc = vec4(0.0, 0.0, 0.0, 0.4 * fillA(length(d - vec2(0.1, -0.12)) - 0.85 * open, pw * 4.0 + 0.2));
    for (int wh = 0; wh < 3; wh++) {
        float fw = float(wh);
        float N = 15.0 + 5.0 * fw;
        float rb = 0.42 - 0.06 * fw;
        float len = (0.98 - 0.15 * fw) * open;
        float wb = 0.095 - 0.012 * fw;
        float off = fract(fw * 0.37);
        float sway = (0.25 * sin(t * 0.6 + seed * 5.0 + ang * 1.5) * (0.5 + uTideAmp) + 0.1 * sin(t * 1.1 + ang * 4.0 + seed)) * (1.0 - 0.2 * fw);
        float k0 = floor(ang * N / TAU - off);
        for (int j = 0; j < 3; j++) {
            float k = k0 + float(j) - 1.0;
            float hk = hash1(k + seed * 31.0 + fw * 7.0);
            float lj = len * (0.82 + 0.25 * hk);
            float along = (rho - rb) / (lj - rb);
            float th = (k + off + 0.5) * TAU / N + sway * sq(sat(along)) * 0.6;
            float dth = abs(mod(ang - th + PI, TAU) - PI);
            float across = dth * rho;
            float wdt = mix(wb, 0.035, sat(along));
            float dd = max(across - wdt, -along * (lj - rb));
            // a rounded tip
            vec2 tip = vec2(cos(th), sin(th)) * lj;
            dd = along > 1.0 ? length(d - tip) - 0.035 : dd;
            float a = fillA(dd, pw * 1.2);
            if (a <= 0.0) continue;
            float s = sat(across / wdt);
            float round = sqrt(max(0.0, 1.0 - s * s));
            vec3 tc = mix(uWarm * 0.5, uWarm * 1.05, smoothstep(0.0, 0.5, along));
            tc = mix(tc, mix(uWarm, vec3(1.0), 0.45), smoothstep(0.6, 1.05, along));
            tc *= 0.6 + 0.5 * round;
            tc += uLight * 0.15 * pow(round, 8.0);
            tc += uGlow * uLum * 1.3 * smoothstep(0.75, 1.05, along);   // glowing tips at night
            acc = over(layerC(tc, a * 0.92), acc);
        }
    }
    // the oral disc, its mouth, its beads
    float disc = fillA(rho - 0.34, pw);
    vec3 dc = uWarm * (0.5 + 0.15 * cos(ang * 18.0) * sat(rho * 3.0) * lod(0.06 * R, px)) * (0.7 + 0.4 * (1.0 - rho / 0.34));
    float mouth = fillA(length(rot(seed) * d * vec2(1.0, 2.4)) - 0.08, pw);
    dc = mix(dc, uDeep * 0.5 + uWarm * 0.15, mouth);
    float bt = floor(ang * 12.0 / TAU);
    vec2 bc = vec2(cos((bt + 0.5) * TAU / 12.0), sin((bt + 0.5) * TAU / 12.0)) * 0.3;
    dc = mix(dc, mix(uCool, vec3(1.0), 0.2), fillA(length(d - bc) - 0.035, pw) * 0.9);
    acc = over(layerC(dc, disc), acc);
    return acc;
}
// a limpet: a ribbed cone seen from above, its apex off center
vec4 limpet(vec2 q, vec2 c, float R, float a0, float px) {
    vec2 d = rot(a0) * (q - c) / R;
    d.x *= 1.18;
    float rho = length(d);
    if (rho > 1.4) return vec4(0.0);
    float pw = px / R * 1.3;
    vec2 ap = vec2(0.12, 0.05);
    vec2 da = d - ap * (1.0 - rho);
    float ra = length(da), th = atan(da.y, da.x);
    float ribs = 0.5 + 0.5 * cos(th * 17.0 + 0.6 * sin(th * 3.0));
    float rings = 0.5 + 0.5 * sin(ra * 38.0);
    vec3 c1 = mix(uSand * 0.9, uWarm2, 0.35 + 0.3 * sin(th * 3.0 + 1.0));
    float hl = sat(dot(normalize(da + 1e-4), normalize(vec2(-0.5, 0.6))));
    vec3 col = c1 * (0.55 + 0.35 * ribs * lod(0.18 * R, px) + 0.1 * rings * lod(0.05 * R, px)) * (0.6 + 0.55 * hl * ra);
    col = mix(col, uSand * 1.15, fillA(ra - 0.08, pw) * 0.7);
    float a = fillA(rho - 1.0, pw);
    float sh = fillA(length(d - vec2(0.12, -0.14)) - 1.06, pw * 6.0 + 0.1) * 0.45;
    return over(layerC(col, a), vec4(0.0, 0.0, 0.0, sh));
}
// a little shore crab walking sideways
vec4 crab(vec2 q, vec2 c, float R, float a0, float walk, float px) {
    vec2 d = rot(-a0) * (q - c) / R;
    if (dot(d, d) > 2.4) return vec4(0.0);
    float pw = px / R * 1.2;
    vec4 acc = vec4(0.0, 0.0, 0.0, 0.35 * fillA(length((d - vec2(0.08, -0.1)) * vec2(1.0, 1.3)) - 0.75, pw * 4.0 + 0.15));
    vec3 shell = mix(uWeed, uWarm2, 0.5) * 0.85;
    for (int s = 0; s < 2; s++) {
        float sd = s == 0 ? 1.0 : -1.0;
        for (int i = 0; i < 4; i++) {
            float fi = float(i);
            float ph = walk * 6.2831 + fi * 1.7 + (s == 0 ? 0.0 : 3.14);
            vec2 base = vec2(0.42 * sd, 0.12 - fi * 0.14);
            vec2 knee = base + vec2(0.33 * sd, 0.1 - fi * 0.08 + 0.05 * sin(ph));
            vec2 foot = knee + vec2(0.22 * sd, -0.24 - 0.06 * cos(ph));
            float dl = min(sdSeg(d, base, knee) - 0.045, sdSeg(d, knee, foot) - 0.03);
            acc = over(layerC(shell * (0.6 + 0.2 * fi * 0.25), fillA(dl, pw)), acc);
        }
    }
    for (int s = 0; s < 2; s++) {
        float sd = s == 0 ? 1.0 : -1.0;
        vec2 b = vec2(0.3 * sd, 0.3), k = vec2(0.45 * sd, 0.55), p = vec2(0.24 * sd, 0.8);
        float dl = min(sdSeg(d, b, k) - 0.06, sdSeg(d, k, p) - 0.1);
        float tipd = length(d - p - vec2(-0.05 * sd, 0.08)) - 0.06;
        acc = over(layerC(shell * 0.95, fillA(min(dl, tipd), pw)), acc);
    }
    vec2 cd = d * vec2(1.0, 1.32);
    float body = length(cd) - 0.5 - 0.05 * cos(atan(cd.y, cd.x) * 5.0);
    float n = fbm(d * 5.0 + c * 3.0);
    vec3 bc = shell * (0.75 + 0.45 * n) * (0.75 + 0.45 * sat(0.5 - d.y + d.x * 0.3));
    bc += uLight * 0.12 * exp(-dot(d - vec2(-0.15, 0.18), d - vec2(-0.15, 0.18)) * 12.0);
    acc = over(layerC(bc, fillA(body, pw)), acc);
    for (int s = 0; s < 2; s++) {
        vec2 e = vec2((s == 0 ? 0.12 : -0.12), 0.42);
        acc = over(layerC(uDeep * 0.4, fillA(length(d - e) - 0.045, pw)), acc);
    }
    return acc;
}
// a copepod, its body along +x, length 1 in d units
vec4 copepod(vec2 d, float pw, float hop, float seed) {
    if (abs(d.x) > 1.3 || abs(d.y) > 1.1) return vec4(0.0);
    vec4 acc = vec4(0.0);
    vec3 body = mix(uWarm2, uSand, 0.35 + 0.3 * hash1(seed));
    vec3 rim = mix(uLight, vec3(1.0), 0.3);
    // antennules: long, swept back while it hops
    float sw = 0.25 + 0.6 * hop;
    for (int s = 0; s < 2; s++) {
        float sd = s == 0 ? 1.0 : -1.0;
        vec2 a = vec2(0.28, 0.04 * sd);
        vec2 b = vec2(0.22 - 0.25 * sw, 0.45 * sd);
)") +
R"(        vec2 c = vec2(0.05 - 0.5 * sw, 0.92 * sd);
        float h1, h2;
        float da = min(sdSegT(d, a, b, h1), sdSegT(d, b, c, h2));
        float ant = fillA(da - 0.011, pw);
        acc = over(acc, layerC(rim * 0.9, ant * 0.8));
        // fine setae along the end of the antenna
        float st = sat(1.0 - abs(fract(h2 * 9.0) - 0.5) * 6.0) * step(0.2, h2);
        acc = over(acc, layerC(rim * 0.6, fillA(da - 0.03, pw) * st * 0.35 * lod(0.1, pw)));
    }
    // the tail (urosome) and its two forked setae
    float dt = sdSeg(d, vec2(-0.3, 0.0), vec2(-0.62, 0.0)) - 0.035;
    for (int s = 0; s < 2; s++) {
        float sd = s == 0 ? 1.0 : -1.0;
        dt = min(dt, sdSeg(d, vec2(-0.62, 0.02 * sd), vec2(-0.95, 0.09 * sd)) - 0.007);
    }
    acc = over(layerC(body * 0.8, fillA(dt, pw) * 0.75), acc);
    // the body: a translucent teardrop with segments, a gut, an oil drop and a red eye
    vec2 bd = d - vec2(0.02, 0.0);
    float egg = length(bd * vec2(1.0, 2.2 + 0.6 * smoothstep(-0.3, 0.3, -bd.x))) - 0.32;
    float inb = fillA(egg, pw);
    float edge = lineA(egg + 0.012, 0.012, pw);
    vec3 bc = body * (0.55 + 0.35 * smoothstep(-0.32, 0.0, egg));
    float seg = lineA(fract((d.x + 0.3) * 6.0) - 0.5, 0.06, pw * 6.0) * step(-0.25, d.x) * step(d.x, 0.2);
    bc = mix(bc, body * 0.45, seg * 0.5 * lod(0.16, pw));
    bc = mix(bc, uWarm2 * 0.8, fillA(length((d - vec2(-0.05, 0.0)) * vec2(1.0, 4.0)) - 0.16, pw) * 0.6);
    bc = mix(bc, mix(uWarm2, uLight, 0.4), fillA(length(d - vec2(0.05, 0.03)) - 0.05, pw));
    bc = mix(bc, uWarm * 1.2, fillA(length(d - vec2(0.28, 0.0)) - 0.03, pw));
    bc = mix(bc, rim, edge * 0.8);
    acc = over(layerC(bc, inb * 0.92), acc);
    return acc;
}
// a nauplius larva: a pear-shaped shield, three pairs of rowing limbs
vec4 nauplius(vec2 d, float pw, float t) {
    if (dot(d, d) > 2.0) return vec4(0.0);
    vec4 acc = vec4(0.0);
    vec3 rim = mix(uLight, vec3(1.0), 0.3);
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float row = sin(t * 3.0 + fi * 1.2);
        for (int s = 0; s < 2; s++) {
            float sd = s == 0 ? 1.0 : -1.0;
            vec2 a = vec2(0.15 - fi * 0.12, 0.18 * sd);
            vec2 b = a + vec2(0.25 - fi * 0.15 + 0.15 * row, (0.5 + 0.1 * fi) * sd);
            vec2 c = b + vec2(-0.25 - 0.1 * row, 0.25 * sd);
            float dl = min(sdSeg(d, a, b) - 0.022, sdSeg(d, b, c) - 0.008);
            acc = over(acc, layerC(rim * 0.8, fillA(dl, pw) * 0.8));
        }
    }
    float sh = length((d - vec2(-0.05, 0.0)) * vec2(1.0 + 0.4 * smoothstep(0.0, -0.4, d.x), 1.25)) - 0.33;
    vec3 bc = mix(uWarm2, uSand, 0.5) * (0.6 + 0.35 * smoothstep(-0.33, 0.0, sh));
    bc = mix(bc, rim, lineA(sh + 0.012, 0.012, pw) * 0.8);
    bc = mix(bc, uWarm * 1.1, fillA(length(d - vec2(0.18, 0.0)) - 0.04, pw));
    acc = over(layerC(bc, fillA(sh, pw) * 0.85), acc);
    return acc;
}
// a chain of little diatoms with long, crossing spines (Chaetoceros)
vec4 chain(vec2 d, float pw, float t) {
    if (abs(d.x) > 1.6 || abs(d.y) > 1.2) return vec4(0.0);
    vec4 acc = vec4(0.0);
    vec3 glass = mix(uLight, uWarm2, 0.25);
    for (int i = 0; i < 5; i++) {
        float x0 = -0.8 + float(i) * 0.4;
        vec2 cd = d - vec2(x0, 0.0);
        vec2 qd = abs(cd) - vec2(0.15, 0.1);
        float box = length(max(qd, 0.0)) + min(max(qd.x, qd.y), 0.0) - 0.04;
        vec3 cc = mix(glass * 0.5, uWarm2 * 0.9, 0.6) * (0.8 + 0.3 * smoothstep(0.0, -0.1, box));
        cc = mix(cc, glass, lineA(box + 0.01, 0.01, pw) * 0.8);
        acc = over(layerC(cc, fillA(box, pw) * 0.85), acc);
        // spines from each junction
        float w = sin(t * 0.8 + float(i)) * 0.06;
        for (int s = 0; s < 2; s++) {
            float sd = s == 0 ? 1.0 : -1.0;
            vec2 a = vec2(x0 + 0.2, 0.12 * sd);
            vec2 m = a + vec2(0.2 + w, 0.45 * sd);
            vec2 b = m + vec2(0.35 + w, 0.4 * sd);
            float ds = min(sdSeg(d, a, m), sdSeg(d, m, b)) - 0.006;
            acc = over(acc, layerC(glass * 0.8, fillA(ds, pw) * 0.55));
        }
    }
    return acc;
}
// an atom as an orb of light: a glowing core, a fine rim, translucent
vec4 orb(vec2 q, vec2 c, float R, vec3 tint, float px) {
    vec2 d = (q - c) / R;
    float rr = length(d);
    if (rr > 1.8) return vec4(0.0);
    float pw = px / R * 1.3;
    float body = fillA(rr - 1.0, pw);
    float core = exp(-rr * rr * 3.0);
    float rim = lineA(rr - 0.93, 0.05, pw) + 0.35 * exp(-sq((rr - 0.9) / 0.15));
    vec3 c2 = tint * (0.2 + 0.85 * core) + mix(tint, uLight, 0.6) * rim * 0.7;
    c2 += uLight * 0.35 * exp(-dot(d - vec2(-0.35, 0.4), d - vec2(-0.35, 0.4)) * 18.0);
    vec4 o = layerC(c2, body * 0.82);
    o.rgb += tint * 0.18 * exp(-sq(max(rr - 1.0, 0.0)) * 12.0) * (1.0 - body);   // a halo
    return o;
}
// ======================================================================= the stages
// Each stage is a whole world filling the plane, in its own units (the screen's half-height is 1 unit at the
// stage's start, 1/16 at its end): an opaque background `bg` and its things `obj` (premultiplied, over the
// background), all of a fixed size in the world, so that zooming in makes every one of them grow and stream out
// from the focus. `host`: where the next stage's things may be seen (the pool's water, the diatom's pores).
struct World { vec3 bg; vec4 obj; float host; };

// A scatter of things on a grid of cells `C` wide: the cell's id and the thing's center (in the stage's units).
#define SCATTER(C, SEED, P) \
    for (int sy = -1; sy <= 1; sy++) \
        for (int sx = -1; sx <= 1; sx++) { \
            vec2 id = floor(q / (C)) + vec2(sx, sy); \
            if (hash(id + SEED) < (P)) continue; \
            vec2 h2 = hash2(id + SEED * 1.7); \
            vec2 c = (id + 0.2 + 0.6 * h2) * (C);
#define END_SCATTER }

// ---- 6. the rock pool at low tide
World stagePool(vec2 q, float px) {
    World W;
    float t = gT;
    float level = 0.4 + 0.012 * uTide * uTideAmp + 0.04 * uWave.y * bump(uWave.x - 3.0, 2.2);
    float e = max(px * 1.5, 0.004);
    float h = poolH(q, px);
    vec2 g = vec2(poolH(q + vec2(e, 0.0), px) - h, poolH(q + vec2(0.0, e), px) - h) / e;
    vec3 n = normalize(vec3(-g * 0.5, 1.0));
    vec3 L = normalize(vec3(-0.5, 0.6, 0.65));
    float dep = level - h;
    float aw = max(px * 1.5, 0.002) * (length(g) + 0.3);
    vec3 col;
    {
        float dry = smoothstep(0.02, 0.3, -dep);
        float dif = sat(dot(n, L)) * 0.85 + 0.22;
        vec2 sl = slabs(q, px, 2.2);
        vec3 rc = uRock * (0.7 + 0.45 * fbm(q * 5.0 + 4.0)) * (1.0 - 0.55 * sl.y);
        rc *= 0.9 + 0.2 * vnoise(q * 40.0) * lod(1.0 / 40.0, px);
        float lich = smoothstep(0.6, 0.78, fbm(q * 2.0 + 9.0)) * smoothstep(0.15, 0.4, -dep);
        rc = mix(rc, uWarm2 * 0.85 * (0.8 + 0.3 * vnoise(q * 25.0)), lich * 0.6);
        float coral = smoothstep(0.5, 0.66, fbm(q * 2.6 - 3.0)) * (1.0 - dry);
        rc = mix(rc, mix(uWarm, uSand, 0.55) * 0.85 * (0.85 + 0.3 * vnoise(q * 30.0)), coral * 0.75);
        rc = mix(rc, uWeed * 0.75, smoothstep(0.55, 0.72, fbm(q * 3.0 - 2.0)) * (1.0 - dry) * 0.55);
        col = rc * dif * mix(0.62, 1.0, dry);
        col += uLight * pow(sat(dot(n, normalize(L + vec3(0.0, 0.0, 1.0)))), 50.0) * mix(0.9, 0.12, dry);
    }
    float waterA = smoothstep(-aw, aw, dep);
    if (dep > -aw) {
        vec2 rip = vec2(sin(q.x * 9.0 + t * 1.1 + 3.0 * fbm(q * 2.0)), cos(q.y * 8.0 - t * 0.9)) * 0.004 * (0.4 + uTideAmp);
        vec2 rq = q - g * 0.015 + rip;
        vec3 bed = sandBed(rq, px, t, 3.2);
        bed = mix(bed, col * 0.9, smoothstep(0.08, 0.0, dep) * 0.6);
        float c = caustic(rq * 2.6, t * 0.5, px * 2.6);
        float d = max(dep, 0.0);
        vec3 wc = waterOver(bed * (0.5 + 1.0 * c * exp(-d * 2.0)), d + 0.04);
        wc += uShallow * 0.06 * smoothstep(-1.5, 1.5, q.y - q.x * 0.4);
        wc = mix(wc, mix(uLight, uShallow, 0.5), lineA(dep, aw * 0.5, aw) * 0.35);
        col = mix(col, wc, waterA);
    }
    if (uLum > 0.0 && dep > 0.0) {
        vec2 id;
        vec3 v = voronoi(q * 14.0, 1.0, t * 0.3, id);
        float tw = pow(sat(sin(t * (0.7 + hash(id)) + hash(id + 2.0) * 6.28)), 6.0);
        col += uGlow * exp(-v.x * v.x / 0.02) * tw * uLum * 0.5 * lod(1.0 / 14.0, px);
    }
    W.bg = col;
    W.obj = vec4(0.0);
    W.host = waterA * smoothstep(0.0, 0.05, dep);
    return W;
}

// ---- 7. the pool's floor, close: stones with limpets and barnacles, anemones, weed, a crab or two
vec4 barnacleAt(vec2 q, vec2 c, float R, float seed, float px) {
    vec2 d = (q - c) / R;
    float rr = length(d);
    if (rr > 1.5) return vec4(0.0);
    float pw = px / R * 1.3;
    float ang = atan(d.y, d.x) + seed * 6.28;
    float fl = 0.5 + 0.5 * cos(ang * 6.0);
    float body = fillA(rr - (1.0 + 0.07 * fl), pw);
    vec3 n = normalize(vec3(d / max(rr, 1e-3) * 0.9 * sat(rr * 1.5), 1.0));
    float lit = 0.4 + 0.6 * sat(dot(n, normalize(vec3(-0.5, 0.6, 0.65))));
    vec3 bc = mix(uSand, vec3(0.92), 0.3) * (0.75 + 0.3 * seed) * lit;
    vec2 od = rot(seed * 3.14) * d;
    bc = mix(bc, bc * 0.6, fillA(abs(od.x) * 0.6 + abs(od.y) - 0.34, pw));
    bc = mix(bc, uDeep * 0.4, fillA(max(abs(od.y) - 0.05, abs(od.x) - 0.42), pw));
    float sh = fillA(length(d - vec2(0.18, -0.22)) - 1.12, pw * 3.0 + 0.25) * 0.4;
    return over(layerC(bc, body), vec4(0.0, 0.0, 0.0, sh));
}
World stageFloor(vec2 q, float px) {
    World W;
    float t = gT;
    float wave = uWave.y * bump(uWave.x - 3.0, 2.2);
    // the bed: sand and gravel, and stones (cells of a coarse Voronoi) standing out of it
    vec3 bed = sandBed(q, px, t, 2.6);
    vec2 sid;
    vec3 sv = voronoi(q * 1.1 + 0.4 * vec2(fbm(q * 0.8), fbm(q * 0.8 + 5.0)), 0.8, 0.0, sid);
    float isStone = step(0.55, hash(sid + 3.0));
    float stoneD = sv.x - 0.42 - 0.08 * fbm(q * 4.0);
    float stone = isStone * fillA(stoneD, px * 1.1 * 1.5 + 0.004);
    vec3 sc = uRock * (0.75 + 0.45 * fbm(q * 6.0 + sid)) * (0.75 + 0.45 * sat(-stoneD * 4.0));
    sc = mix(sc, mix(uWarm, uSand, 0.55) * 0.85, smoothstep(0.55, 0.7, fbm(q * 3.5 + sid * 3.0)) * 0.7);
    sc = mix(sc, uWeed * 0.8, smoothstep(0.6, 0.75, fbm(q * 2.5 - sid)) * 0.5);
    bed *= 1.0 - 0.35 * isStone * fillA(stoneD - 0.06, 0.06) * (1.0 - stone);
    bed = mix(bed, sc, stone);
    float caus = caustic(q * 1.6, t * 0.5, px * 1.6);
    W.bg = waterOver(bed * (0.5 + 1.0 * caus), 0.4);
    // weed: soft blades of sea lettuce lying on the sand
    vec4 acc = vec4(0.0);
    SCATTER(1.6, 11.0, 0.7)
        vec2 d = rot(hash(id + 4.0) * 6.28 + 0.15 * sin(t * 0.5 + h2.x * 6.0) * uTideAmp) * (q - c);
        float R = 0.35 + 0.25 * h2.y;
        d /= R;
        float blade = length(d * vec2(0.55, 1.6)) - 0.8 + 0.15 * fbm(d * 3.0 + id);
        float a = fillA(blade, px / R * 2.0 + 0.02);
        if (a <= 0.0) continue;
        vec3 wc = uWeed * (0.9 + 0.4 * fbm(d * 6.0 + id)) * (0.8 + 0.3 * sat(-blade * 3.0));
        wc += uLight * 0.06 * lineA(d.y, 0.02, px / R * 2.0);
        acc = over(acc, layerC(waterOver(wc * (0.6 + 0.8 * caus), 0.25), a * 0.85));
    END_SCATTER
    // limpets and barnacles on the stones
    SCATTER(0.32, 21.0, 0.3)
        vec2 sid2;
        vec3 sv2 = voronoi(c * 1.1 + 0.4 * vec2(fbm(c * 0.8), fbm(c * 0.8 + 5.0)), 0.8, 0.0, sid2);
        if (hash(sid2 + 3.0) < 0.55 || sv2.x > 0.36) continue;
        vec4 o = h2.x > 0.7 ? limpet(q, c, 0.07 + 0.03 * h2.y, h2.y * 6.0, px) : barnacleAt(q, c, 0.035 + 0.015 * h2.y, h2.x, px);
        acc = over(o * vec4(vec3(0.8 + 0.6 * caus), 1.0), acc);
    END_SCATTER
    // beadlet anemones, on stones and sand (none right at the focus: the water there is where we dive)
    SCATTER(0.9, 31.0, 0.35)
        if (length(c) < 0.35 || fbm(c * 0.35 + 3.0) < 0.56) continue;    // in colonies, not a carpet
        float R = 0.16 + 0.12 * h2.y;
        vec4 o = anemone(q, c, R, h2.x * 9.0 + id.x, px, t);
        acc = over(vec4(waterOver(o.rgb / max(o.a, 1e-3) * (0.8 + 0.5 * caus), 0.1) * o.a, o.a), acc);
    END_SCATTER
)" +
R"(    // a crab or two, walking slowly round their cells
    SCATTER(3.0, 41.0, 0.55)
        float cw = t * (0.04 + 0.03 * h2.x) + h2.y * 6.0;
        vec2 cp = c + vec2(cos(cw), sin(cw * 0.7)) * 0.6;
        vec4 o = crab(q, cp, 0.14, cw + 1.57, t * 0.35, px);
        acc = over(vec4(waterOver(o.rgb / max(o.a, 1e-3), 0.12) * o.a, o.a), acc);
    END_SCATTER
    // the drop's wave: a lace of foam drifting over
    float foam = wave * smoothstep(0.45, 0.7, fbm(q * 5.0 + t * 0.4));
    acc = over(layerC(vec3(0.92), foam * 0.5), acc);
    W.obj = acc;
    W.host = 1.0;
    return W;
}

// ---- 8. the water, teeming: plankton
// a diatom's frustule: rings of pores of one size, sectors raised and sunken, its girdle; (color, alpha, pore)
vec4 frustule(vec2 q, float R, float px, out float poreMask) {
    float r = length(q);
    poreMask = 0.0;
    if (r > R * 1.25) return vec4(0.0);
    float t = gT;
    float th = atan(q.y, q.x);
    float rn = r / R;
    // a jewel of glass: golden-green, lit as a shallow dome from the upper left, thin-film colors drifting over it
    vec2 L2 = normalize(vec2(-0.6, 0.7));
    float dome = sqrt(max(0.0, 1.0 - rn * rn));
    vec3 n = normalize(vec3(q / R * 0.8, dome + 0.2));
    float lit = 0.55 + 0.45 * sat(dot(n, normalize(vec3(L2, 0.8))));
    float sweep = 0.5 + 0.5 * cos(th - (t * 0.1 + 0.6 * uTide * uTideAmp));
    float film = 0.5 + 0.5 * sin(rn * 7.0 - t * 0.12);
    vec3 irid = hsv(fract(film * 0.45 + 0.3 + uFever * t * 0.02), 0.45, 1.0);
    vec3 glass = mix(mix(uLight, uShallow, 0.4), mix(uWarm2, uWeed, 0.4), 0.55) * 1.05;
    glass = mix(glass, glass * irid * 1.3, 0.3 + 0.2 * sweep);
    vec3 gcol = glass * lit * (0.75 + 0.2 * sweep);
    // the living cell inside, golden, its chloroplasts glowing through
    vec3 cell = mix(uWarm2, uWeed, 0.25 + 0.35 * vnoise(q / R * 4.0 + t * 0.05)) * (0.75 + 0.5 * smoothstep(0.55, 0.75, fbm(q / R * 3.0 + 4.0)));
    // pores in rings, all of one size, rows growing in number outward; finer near the rim
    float dr = R / 32.0;
    float ring = floor(r / dr);
    float rc = (ring + 0.5) * dr;
    float N = max(1.0, floor(TAU * rc / dr));
    float a = th / TAU * N + hash1(ring) * 7.0;
    vec2 f = vec2((r / dr - ring) - 0.5, fract(a) - 0.5);
    f.y *= TAU * rc / dr / N;
    float pw = px / dr * 1.2;
    float lodp = lod(dr, px * 1.8);
    float inField = smoothstep(0.09, 0.12, rn) * (1.0 - smoothstep(0.86, 0.9, rn));
    float pd = length(f) - 0.3;
    float pore = fillA(pd, pw) * inField;
    vec3 inPore = mix(cell * 0.3, cell * 1.2, smoothstep(0.3, 0.0, length(f)));
    float sieve = fillA(length(fract(f * 6.0) - 0.5) - 0.2, pw * 6.0) * lod(dr / 6.0, px * 1.5);
    inPore = mix(inPore, gcol * 0.8, sieve * 0.3 * (1.0 - smoothstep(0.22, 0.3, length(f))));
    float bev = 0.5 + 0.5 * dot(normalize(f + 1e-4), L2);
    vec3 cellMean = mix(uWarm2, uWeed, 0.4) * 0.85;
    vec3 col = mix(gcol, mix(gcol, cellMean, 0.4), inField * (1.0 - lodp) * 0.8);
    col = mix(col, inPore, pore * lodp);
    col += glass * 0.35 * bev * lineA(pd + 0.03, 0.035, pw) * lodp * inField;
    // a few fine radial costae, and a small rosette at the center
    col = mix(col, gcol * 1.2, lineA(fract(th / TAU * 16.0) - 0.5, 0.015, px / max(r, 1e-3) * 16.0 / TAU * 1.2) * 0.3 * lod(r * TAU / 16.0, px) * inField);
    col = mix(col, glass * (0.85 + 0.08 * sin(rn * 180.0) * lod(R / 180.0 * 6.0, px)) * lit, 1.0 - smoothstep(0.08, 0.1, rn));
    col = mix(col, cellMean * 1.1, fillA(rn - 0.025, px / R * 1.5) * 0.4);
    // the rim: a band of clear glass, a crisp bright edge, a dark inner line
    float band = smoothstep(0.88, 0.9, rn);
    col = mix(col, glass * (0.85 + 0.25 * sin(rn * 300.0) * lod(R / 300.0 * 4.0, px)) * lit, band);
    float ew = max(px, R * 0.002);
    col = mix(col, uDeep * 0.6, lineA(rn - 0.89, 0.004, ew / R) * 0.6);
    col += mix(uLight, vec3(1.0), 0.4) * lineA(r - R * 0.995, R * 0.004, ew) * (0.4 + 0.6 * sat(dot(normalize(q + 1e-5), L2)));
    col += uLight * 0.35 * uKick * step(0.985, hash(vec2(ring, floor(a)))) * pore * lodp;   // a few pores glint, softly
    poreMask = max(pore * lodp, inField * (1.0 - lodp) * 0.5);
    float alpha = fillA(r - R, max(px * 1.2, R * 0.002)) * 0.95;
    vec4 o = layerC(col, alpha);
    // a soft halo of light round the glass
    o.rgb += uLight * 0.12 * exp(-sq((r - R) / (R * 0.06))) * (1.0 - alpha);
    return o;
}
World stagePlankton(vec2 q, float px) {
    World W;
    float t = gT;
    // luminous water, lit from above left, clouds of fine matter
    vec3 col = mix(uWater, uShallow, 0.5 + 0.12 * sin(q.x * 0.7 + q.y * 0.9 + 1.0));
    col *= 0.9 + 0.14 * fbm(q * 1.4 + t * 0.03);
    vec4 acc = vec4(0.0);
    SCATTER(0.11, 51.0, 0.55)
        vec2 cc = c + 0.02 * vec2(sin(t * 0.3 + h2.x * 9.0), cos(t * 0.27 + h2.y * 9.0));
        float R = 0.004 + 0.008 * h2.y;
        float soft = hash(id + 9.0) > 0.75 ? R * 2.5 : 0.0;
        float a = fillA(length(q - cc) - R - soft * 0.5, max(px, soft)) * (soft > 0.0 ? 0.3 : 0.7);
        acc = over(acc, layerC(mix(uLight, uSand, h2.x), a));
    END_SCATTER
    // a gentle dart every two beats: eased out and back, never a jump
    float hopP = fract(uLife * 0.5);
    float hop = smoothstep(0.0, 0.4, hopP) * (1.0 - smoothstep(0.4, 1.0, hopP)) * (0.3 + 0.5 * uTideAmp);
    // diatoms drifting, little glass jewels
    SCATTER(0.55, 61.0, 0.72)
        if (length(c) < 0.25) continue;
        float R = 0.04 + 0.05 * h2.y;
        float pm;
        vec4 o = frustule(rot(t * 0.05 * (h2.x - 0.5)) * (q - c), R, px, pm);
        acc = over(o, acc);
    END_SCATTER
    // copepods, nauplii, chains
    SCATTER(0.5, 71.0, 0.25)
        if (length(c) < 0.25) continue;
        int kind = int(hash(id + 3.0) * 3.0);
        float size = (kind == 0 ? 0.15 : kind == 1 ? 0.09 : 0.1) * (0.7 + 0.5 * h2.y);
        float heading = hash(id + 5.0) * 6.28 + 0.3 * sin(t * 0.2 + h2.x * 6.0);
        vec2 dir = vec2(cos(heading), sin(heading));
        vec2 cc = c + dir * size * 0.4 * sin(t * 0.12 + h2.x * 9.0);    // (the beat only sweeps their antennae)
        vec2 d = rot(-heading) * (q - cc) / size;
        float pw = px / size * 1.3;
        vec4 o = kind == 0 ? copepod(d, pw, hop, id.x) : kind == 1 ? nauplius(d, pw, t + h2.x * 9.0) : chain(d, pw, t);
        acc = over(o, acc);
        if (uLum > 0.0) col += uGlow * uLum * uKick * 0.25 * exp(-dot(q - cc, q - cc) / sq(size * 0.6)) * step(0.6, hash(id + 13.0));
    END_SCATTER
    W.bg = col;
    W.obj = acc;
    W.host = 1.0;
    return W;
}

// ---- 9. one diatom, a jewel of glass among others, the living cell glowing through its pores
World stageDiatom(vec2 q, float px) {
    World W;
    float t = gT;
    vec3 col = mix(uWater, uShallow, 0.5 + 0.12 * sin(q.x * 0.7 / 16.0 + q.y * 0.9 / 16.0 + 1.0));
    col *= 0.92 + 0.1 * fbm(q * 0.3 + t * 0.02);
    vec4 acc = vec4(0.0);
    // motes in the water, out of focus
    SCATTER(0.5, 151.0, 0.6)
        float R = 0.02 + 0.04 * h2.y;
        acc = over(acc, layerC(mix(uLight, uSand, h2.x), fillA(length(q - c) - R, max(px, R)) * 0.25));
    END_SCATTER
    // other diatoms drifting near it
    SCATTER(1.3, 161.0, 0.7)
        if (length(c) < 1.6) continue;
        float R = 0.25 + 0.3 * h2.y;
        float pm;
        acc = over(frustule(rot(t * 0.03 * (h2.x - 0.5)) * (q - c), R, px, pm), acc);
    END_SCATTER
    float pm;
    vec4 o = frustule(rot(t * 0.004) * q, 1.0, px, pm);
    W.bg = col;
    W.obj = over(o, acc);
    W.host = pm;
    return W;
}
// ---- 10. the living cell: cytoplasm streaming
World stageCell(vec2 q0, float px) {
    World W;
    float t = gT;
    // (a constant pace: a pace that follows the music, multiplied by the clock, would make the stream jump)
    float sp = 0.035;
    vec2 q = q0 + vec2(t * sp, (0.04 + 0.02 * uTide * uTideAmp) * sin(q0.x * 1.3 + t * 0.15));     // the stream
    vec3 col = mix(mix(uShallow, uSand, 0.3), uWater, 0.4) * (0.8 + 0.15 * fbm(q * 1.5));
    float vf = fbm(q * 0.9 + 7.0);
    col = mix(col, mix(uShallow, uLight, 0.25), smoothstep(0.55, 0.7, vf) * 0.35);
    col = mix(col, mix(uShallow, uLight, 0.5), lineA(vf - 0.55, 0.004, px * 0.8 + 0.003) * 0.35);
    vec4 acc = vec4(0.0);
    // chloroplasts
    SCATTER(0.9, 81.0, 0.45)
        float R = 0.18 + 0.12 * hash(id + 8.0);
        vec2 d = rot(hash(id + 4.0) * 6.28 + 0.1 * sin(t * 0.1 + h2.x * 6.0)) * (q - c) / R;
        d.y *= 1.6 + 0.6 * hash(id + 9.0);
        float lob = length(d) - 1.0 + 0.1 * sin(atan(d.y, d.x) * 3.0 + h2.x * 6.0);
        float pw = px / R * 1.5;
        float a = fillA(lob, pw + 0.03);
        if (a <= 0.0) continue;
        vec3 pc = mix(uWarm2, uWeed, 0.25 + 0.3 * h2.y) * (0.6 + 0.4 * smoothstep(0.0, -0.7, lob));
        pc = mix(pc, pc * 0.7, lineA(fract(d.y * 2.2) - 0.5, 0.1, pw * 2.2) * lod(R / 2.2, px) * 0.5);
        pc = mix(pc, mix(uSand, uLight, 0.5), fillA(length(d) - 0.22, pw + 0.03) * 0.6);
        pc += uLight * 0.25 * lineA(lob + 0.05, 0.03, pw);
        acc = over(layerC(pc, a * 0.88), acc);
    END_SCATTER
    // granules and oil drops
    SCATTER(0.09, 91.0, 0.55)
        float big = step(0.93, hash(id + 2.0));
        float R = big > 0.5 ? 0.025 : 0.008 + 0.005 * h2.y;
        float dd = length(q - c) - R;
        vec3 gc = big > 0.5 ? mix(uWarm2, uLight, 0.6) : mix(uLight, uSand, 0.4);
        gc *= 0.8 + 0.5 * sat(1.0 - length(q - c + R * vec2(0.3, -0.3)) / R);
        acc = over(acc, layerC(gc, fillA(dd, max(px, R * 0.12)) * (big > 0.5 ? 0.9 : 0.55)));
    END_SCATTER
    W.bg = col;
    W.obj = acc;
    W.host = 1.0;
    return W;
}

// ---- 11. molecules jostling
World stageMolecules(vec2 q, float px) {
    World W;
    float t = gT;
    float jit = 0.5 + 0.5 * uTideAmp * (0.7 + 0.3 * uTide) + 0.2 * uKick;
    vec3 col = mix(uDeep, uWater, 0.32 + 0.1 * fbm(q * 0.7));
    vec4 acc = vec4(0.0);
    // double helices crossing the field, rungs of light between their strands
    for (int k = 0; k < 2; k++) {
        float fk = float(k);
        vec2 ax = normalize(vec2(cos(0.7 + fk * 1.9), sin(0.7 + fk * 1.9)));
        vec2 o = vec2(-0.4 + fk * 1.1, 0.5 - fk * 0.9);
        float along = dot(q - o, ax), across = dot(q - o, vec2(-ax.y, ax.x));
        across += 0.12 * sin(along * 0.9 + fk * 3.0);
        float hw = 0.07;
        float ph = along * 14.0 - t * 0.3;
        float s1 = sin(ph), s2 = -s1;
        float lw = max(px, 0.002);
        float f1 = 0.5 + 0.5 * cos(ph);
        vec3 c1 = mix(uWarm, uLight, 0.45), c2 = mix(uCool, uLight, 0.45);
        float rung = lineA(fract(ph * 6.0 / TAU) - 0.5, 0.05, 6.0 / TAU * 14.0 * lw) * step(abs(across), hw * abs(s1) * 0.95);
        acc = over(acc, layerC(mix(c1, c2, 0.5) * 0.8, rung * 0.45 * lod(0.02, px)));
        acc = over(layerC(c1 * (0.5 + 0.6 * f1), lineA(across - hw * s1, 0.004, lw) * (0.35 + 0.65 * f1)), acc);
        acc = over(layerC(c2 * (1.1 - 0.6 * f1), lineA(across - hw * s2, 0.004, lw) * (1.0 - 0.65 * f1)), acc);
        acc.rgb += uGlow * 0.06 * exp(-sq(across / (hw * 1.6))) * (1.0 - acc.a);
    }
    // water molecules: an oxygen orb, two hydrogens, bonds as threads of light, Brownian jitter
    SCATTER(0.34, 101.0, 0.35)
        if (length(c) < 0.3) continue;
        float hs = h2.x * 40.0 + h2.y * 13.0;
        vec2 jt = vec2(vnoise(vec2(t * 0.7 + hs, 1.0)), vnoise(vec2(1.0, t * 0.65 + hs))) - 0.5;
        vec2 cc = c + jt * 0.035 * jit;
        float R = 0.04;
        float ang = hash(id + 2.0) * 6.28 + (vnoise(vec2(t * 0.5 + hs, 5.0)) - 0.5) * 0.8 * jit;
        vec2 e1 = vec2(cos(ang), sin(ang)), e2 = vec2(cos(ang + 1.82), sin(ang + 1.82));
        vec2 h1p = cc + e1 * R * 2.2, h2p = cc + e2 * R * 2.2;
        float bw = max(px, 0.002);
        float bond = max(lineA(sdSeg(q, cc, h1p), 0.0015, bw), lineA(sdSeg(q, cc, h2p), 0.0015, bw));
        vec4 m = layerC(mix(uLight, uGlow, 0.3), bond * 0.75);
        m = over(orb(q, cc, R, mix(uWarm, uSand, 0.25), px), m);
        m = over(orb(q, h1p, R * 0.6, mix(uCool, uLight, 0.3), px), m);
)" +
R"(        m = over(orb(q, h2p, R * 0.6, mix(uCool, uLight, 0.3), px), m);
        acc = over(m, acc);
    END_SCATTER
    // the molecule at the focus: two hydrogens bonded to the atom we dive into
    {
        float ang = 0.6 + 0.15 * sin(t * 0.7);
        vec2 e1 = vec2(cos(ang), sin(ang)), e2 = vec2(cos(ang + 1.82), sin(ang + 1.82));
        float bw = max(px, 0.002);
        float bond = max(lineA(sdSeg(q, e1 * 0.1, e1 * 0.18), 0.0015, bw), lineA(sdSeg(q, e2 * 0.1, e2 * 0.18), 0.0015, bw));
        vec4 m = layerC(mix(uLight, uGlow, 0.3), bond * 0.8);
        m = over(orb(q, e1 * 0.2, 0.035, mix(uCool, uLight, 0.3), px), m);
        m = over(orb(q, e2 * 0.2, 0.035, mix(uCool, uLight, 0.3), px), m);
        acc = over(m, acc);
    }
    W.bg = col;
    W.obj = acc;
    W.host = 1.0;
    return W;
}

// ---- 12. the atom: breathing orbital clouds, drawn as contours of light
World stageAtom(vec2 q, float px) {
    World W;
    float t = gT;
    float r = length(q);
    float th = atan(q.y, q.x);
    float breathe = 1.0 + 0.012 * uTide * uTideAmp;
    float lr = log(max(r, 1e-4) / breathe);
    float psi = 0.0, dens = 0.0;
    for (int s = 0; s < 5; s++) {
        float fs = float(s);
        float rc = log(0.09) + fs * 0.69;          // shells at 0.09, 0.18, 0.36, 0.72, 1.44
        float rad = exp(-sq((lr - rc) / 0.3));
        float m = 1.0 + fs;
        float spin = t * (0.04 + 0.015 * fs) * (mod(fs, 2.0) * 2.0 - 1.0);
        float beat = 0.5 + 0.5 * sin(t * (0.17 + 0.04 * fs) + fs * 1.3);
        float y = mix(cos(m * (th - spin)), cos((m + 1.0) * (th + spin * 0.7) + fs), beat * 0.5);
        psi += rad * y;
        dens += sq(rad * y);
    }
    dens *= 0.8 + 0.2 * uPeak;
    vec3 warm = mix(uWarm, uLight, 0.3), cool = mix(uCool, uLight, 0.3);
    vec3 phase = mix(cool, warm, smoothstep(-0.04, 0.04, psi));
    float lv = sqrt(dens) * 7.0;
    float iso = lineA(fract(lv) - 0.5, 0.05, fwidth(lv) * 1.2 + 0.01) * smoothstep(0.3, 1.2, lv) * (1.0 - smoothstep(6.0, 7.0, lv));
    vec3 c = phase * (dens * 0.6 + iso * 0.75);
    float a = sat(sqrt(dens) * 1.6 + iso * 0.5) * (1.0 - smoothstep(1.6, 2.4, r));
    // the nucleus at the center: a tiny bright knot
    c += mix(uLight, uWarm, 0.3) * exp(-r * r / 0.0004) * 0.8;
    a = max(a, exp(-r * r / 0.0004));
    W.bg = mix(uDeep, uWater, 0.35);
    // bonds: threads of light to the neighbours
    for (int i = 0; i < 3; i++) {
        float ba = 0.8 + float(i) * 2.1;
        vec2 bd = vec2(cos(ba), sin(ba));
        float along = dot(q, bd);
        float dd = abs(dot(q, vec2(-bd.y, bd.x)));
        W.bg += mix(uLight, uGlow, 0.5) * lineA(dd, 0.003, max(px, 0.002)) * smoothstep(0.2, 0.6, along) * 0.5 * (0.6 + 0.4 * sin(along * 6.0 - t * 2.0));
    }
    W.obj = vec4(c * a / max(a, 1e-3), a);
    W.obj.rgb = c;
    W.host = 1.0;
    return W;
}

// ---- 13. inside the nucleus: quantum foam
World stageFoam(vec2 q, float px) {
    World W;
    float t = gT;
    float r = length(q);
    float boil = 0.08;                 // (constant: it multiplies the clock)
    float C = 0.14;
    vec2 i = floor(q / C), f = q / C - i;
    float d1 = 8.0, d2 = 8.0;
    vec2 o1 = vec2(0.0), o2 = vec2(0.0), cid = i;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 h = hash2(i + g);
            vec2 o = g + 0.5 + 0.38 * sin(t * 0.2 * (0.5 + h) + 6.2831 * h);
            vec2 dv = o - f;
            float d = dot(dv, dv);
            if (d < d1) { d2 = d1; o2 = o1; d1 = d; o1 = dv; cid = i + g; } else if (d < d2) { d2 = d; o2 = dv; }
        }
    float wallD = abs(dot(0.5 * (o1 + o2), normalize(o2 - o1)));
    float cd = sqrt(d1);
    float life = fract(t * boil * (0.5 + hash(cid)) + hash(cid + 3.0));
    float burst = smoothstep(0.8, 0.9, life) * (1.0 - smoothstep(0.95, 1.0, life));
    float pw = px / C;
    float wall = lineA(wallD, 0.025 + 0.02 * (1.0 - life), pw * 1.2) * (1.0 - burst);
    vec3 fc = hsv(fract(fract(hash(cid) * 3.0 + cd * 0.8 + life * 0.6) * 0.7 + 0.45 + uFever * t * 0.03), 0.45, 1.0) * mix(uLight, vec3(1.0), 0.2);
    float k = lod(C * 0.3, px);
    vec3 col = mix(uDeep, uWater, 0.34);
    col += fc * wall * 0.75 * k + fc * (1.0 - k) * 0.12;
    col += mix(uCool, uShallow, 0.4) * 0.08 * smoothstep(0.6, 0.0, cd) * (1.0 - burst) * life;
    float age = (life - 0.86) / 0.14;
    if (age > 0.0) {
        col += mix(uCool, uLight, 0.5) * cos((cd - age * 1.4) * 18.0) * exp(-sq((cd - age * 1.4) / 0.3)) * (1.0 - age) * 0.18 * k;
        col += uLight * 0.12 * exp(-sq(cd / 0.1)) * sin(sat(age / 0.5) * PI) * k;
    }
    // virtual pairs: two sparks born together, spiraling apart, annihilating (with the kicks)
    SCATTER(0.5, 111.0, 0.5)
        float ph = fract(t * 0.15 + h2.x);
        float sp = sin(ph * PI) * 0.06;
        float a0 = ph * 5.0 + h2.y * 6.28;
        vec2 o = vec2(cos(a0), sin(a0)) * sp;
        float sz = 0.006 + px;
        float g = exp(-dot(q - c - o, q - c - o) / sq(sz)) + exp(-dot(q - c + o, q - c + o) / sq(sz));
        col += mix(uWarm, uLight, 0.5) * g * (0.25 + 0.3 * uKick) * sin(ph * PI);
    END_SCATTER
    // the nucleus seen whole: a glowing knot of foam with a bright rim
    float R = 2.2;
    float rim = exp(-sq((r - R) / 0.25)) * 0.12;
    W.bg = mix(uDeep, uWater, 0.34);
    float fa = 1.0 - smoothstep(R * 0.55, R, r);
    W.obj = vec4((col + mix(uLight, uGlow, 0.5) * rim) * fa, fa);
    W.host = 1.0;
    return W;
}

// ---- 14. strings, ringing with the music, on a fabric that ripples like a pool
World stageStrings(vec2 q, float px) {
    World W;
    float t = gT;
    vec3 col = mix(uDeep, uWater, 0.3);
    // ripples from a few sources, interfering
    float rip = 0.0;
    for (int i = 0; i < 3; i++) {
        vec2 s = vec2(cos(float(i) * 2.1 + 0.4), sin(float(i) * 2.1 + 0.4)) * 0.8;
        rip += cos(length(q - s) * 40.0 - t * 1.2);
    }
    col += mix(uCool, uLight, 0.3) * 0.035 * (rip / 3.0 + 0.5) * lod(0.15, px * 2.0);
    vec4 acc = vec4(0.0);
    SCATTER(0.42, 121.0, 0.3)
        float R0 = 0.07 + 0.09 * h2.y;
        vec2 d = q - c;
        float r = length(d), th = atan(d.y, d.x);
        if (r > R0 * 1.6) continue;
        float rr = R0, drr = 0.0;
        float dir = hash(id + 6.0) > 0.5 ? 1.0 : -1.0;
        for (int n = 2; n < 8; n++) {
            float fn = float(n);
            float band = n < 3 ? uBands.x : n < 4 ? uBands.y : n < 6 ? uBands.z : uBands.w;
            float a = (0.03 + 0.05 * band * (0.4 + uTideAmp) + 0.01 * uKick) / fn * (0.5 + 0.8 * hash(id + fn));
            float arg = fn * th - t * (0.18 + 0.06 * fn) * dir + hash(id + fn * 3.0) * 6.28;
            rr += R0 * a * cos(arg);
            drr -= R0 * a * fn * sin(arg);
        }
        float dd = abs(r - rr) / sqrt(1.0 + sq(drr / max(r, 1e-3)));
        float lw = max(px, 0.0015);
        vec3 sc = mix(mix(uLight, mix(uGlow, uWarm, step(0.5, h2.x)), 0.45), hsv(fract(h2.x * 0.3 + 0.55 + uFever * t * 0.02), 0.3, 1.0), 0.25);
        float core = lineA(dd, 0.0012, lw);
        float glow = exp(-dd / 0.01) * 0.3 + exp(-dd / 0.003) * 0.25;
        acc.rgb += sc * (core * 0.9 + glow);
        acc.a = max(acc.a, core * 0.9);
    END_SCATTER
    W.bg = col;
    W.obj = acc;
    W.host = 1.0;
    return W;
}

// ======================================================================= the sky: from the whole universe to a shore
// Stages 0-5 nest exactly: each stage's frame is the one before it 16 times larger and turned by REL (4 octaves of
// the zoom's turn), so a stage can draw what the one before it shows there (its background) and the one after it
// what it shows at its focus: the galaxy we dive into is the one at the web's focus, then its star, its planet, the
// very coast and pool. Light glows in the dark: every source is added, a point stays a point (its light, not its
// size), and every extended thing is band-limited against the pixel (gaussians widened by it, octaves faded).
const float REL = 0.28;
vec2 up(vec2 q) { return rot(REL) * q * 0.0625; }       // a point of a stage in the stage before it
vec2 down(vec2 q) { return rot(-REL) * q * 16.0; }      // ... in the stage after it
float gNext = 0.0;            // 1 while the next stage is drawn (tiny at the focus, then crossfading in)
float gWantBg = 1.0;          // 0 when that next stage's background is not seen yet (before its crossfade)
float gTn = 0.0;              // the next stage's clock
vec2 gWebO = vec2(0.0);       // the cosmic web's offset that puts one of its nodes at the focus

vec3 spaceBg() { return vec3(0.0010, 0.0013, 0.0042) + uDeep * 0.035; }
// a gaussian blob of light of radius s seen through a pixel of size pw (its light kept when it is smaller)
float blob(float r2, float s, float pw) { float v = s * s + pw * pw; return s * s / v * exp(-r2 / v); }

// Points of light fixed in a plane, of every brightness: level k has one star in a cell of C0 / 2^k (where the
// density lets it), the stars of a finer level coming in, faint, as the zoom makes room for them.
vec3 starfield(vec2 g, float pg, float C0, float dens, float seed) {
    vec3 acc = vec3(0.0);
    float C = C0;
    for (int k = 0; k < 18; k++) {
        float cp = C / pg;
        if (cp < 2.5) break;
        float fk = float(k) * 13.7 + seed;
        vec2 id = floor(g / C);
        if (hash(id + fk) < dens) {
            vec2 c = (id + 0.2 + 0.6 * hash2(id + fk * 1.3)) * C;
            float d2 = dot(g - c, g - c) / (pg * pg);
            float hb = hash(id * 1.7 + fk + 5.0);
            float br = (0.1 + 3.2 * pow(hb, 8.0)) * smoothstep(2.5, 10.0, cp);
            br *= 1.0 + 0.6 * uKick * step(0.9, hb) + 0.12 * sin(uTime * (0.7 + hb) + hb * 40.0) * step(0.6, hb);
            vec3 sc = mix(vec3(1.0, 0.6, 0.36), vec3(0.6, 0.76, 1.0), hash(id + fk + 9.0));
            acc += sc * br * exp(-d2 * 0.9);
        }
        C *= 0.5;
    }
    return acc;
}

// ---- the cosmic web: the edges of a Voronoi net (filaments of galaxies), its nodes (clusters)
// (distance to the nearest edge, to the nearest node), in cells
vec2 vweb(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 o1 = vec2(0.0), o2 = vec2(0.0), o3 = vec2(0.0);
    float d1 = 8.0, d2 = 8.0, d3 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = g + 0.5 + 0.4 * (hash2(i + g) * 2.0 - 1.0) - f;
            float d = dot(o, o);
            if (d < d1) { d3 = d2; o3 = o2; d2 = d1; o2 = o1; d1 = d; o1 = o; }
            else if (d < d2) { d3 = d2; o3 = o2; d2 = d; o2 = o; }
            else if (d < d3) { d3 = d; o3 = o; }
        }
    return vec2(dot(0.5 * (o1 + o2), normalize(o2 - o1)), sqrt(d3) - sqrt(d1));
}
const float WS = 1.7;         // the coarse net's cells per unit
vec2 webP(vec2 q) { return q * WS + 0.5 * vec2(vnoise(q * 1.9 + 3.0), vnoise(q * 1.9 - 7.0)) + 0.12 * vec2(vnoise(q * 6.0), vnoise(q * 6.0 + 5.0)); }
// a node of the net: the circumcenter of the three sites nearest to a point (then put at the focus)
vec2 webNode() {
    vec2 p = vec2(0.37, 0.61), i = floor(p);
    vec2 s1 = vec2(0.0), s2 = vec2(0.0), s3 = vec2(0.0);
    float d1 = 8.0, d2 = 8.0, d3 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 s = i + vec2(x, y) + 0.5 + 0.4 * (hash2(i + vec2(x, y)) * 2.0 - 1.0);
            float d = dot(s - p, s - p);
            if (d < d1) { d3 = d2; s3 = s2; d2 = d1; s2 = s1; d1 = d; s1 = s; }
            else if (d < d2) { d3 = d2; s3 = s2; d2 = d; s2 = s; }
            else if (d < d3) { d3 = d; s3 = s; }
        }
    float D = 2.0 * (s1.x * (s2.y - s3.y) + s2.x * (s3.y - s1.y) + s3.x * (s1.y - s2.y));
    float a = dot(s1, s1), b = dot(s2, s2), c = dot(s3, s3);
    return vec2(a * (s2.y - s3.y) + b * (s3.y - s1.y) + c * (s1.y - s2.y),
                a * (s3.x - s2.x) + b * (s1.x - s3.x) + c * (s2.x - s1.x)) / D;
}
)" +
R"(// the coarse density (where the galaxies gather): filaments, nodes, the cluster at the focus
float webDens(vec2 q, out vec2 e) {
    e = vweb(webP(q) + gWebO);
    return exp(-e.x / 0.045) + 1.6 * exp(-e.y / 0.06) + 2.2 * exp(-dot(q, q) / 0.003);
}
// a little galaxy at d from its center, radius R: a warm core, a disc (spiral arms once it is big enough)
vec3 miniGalaxy(vec2 d, float R, float pw, float s) {
    vec2 e = rot(s * 6.2831) * d;
    e.y /= mix(0.28, 1.0, fract(s * 7.31));
    float r2 = dot(e, e);
    float ell = step(fract(s * 3.7), 0.35);
    float core = blob(r2, R * 0.16, pw), disk = blob(r2, R * 0.5, pw);
    float arms = 1.0;
    if (ell < 0.5) {
        float rr = sqrt(r2) / R;
        float th = atan(e.y, e.x);
        arms = mix(1.0, 0.3 + 1.4 * sq(0.5 + 0.5 * cos(2.0 * (th - 3.0 * log(rr + 0.05)) + s * 20.0)), lod(R * 0.35, pw));
    }
    vec3 dc = ell > 0.5 ? mix(uWarm2, uLight, 0.45) * 1.1 : mix(uCool, uLight, 0.3) * 1.2;
    return mix(uWarm2, uLight, 0.6) * core * 2.2 + dc * disk * arms * 0.85;
}
vec3 webLight(vec2 q, float px, bool gals) {
    vec2 e1;
    float d1 = webDens(q, e1);
    vec2 P = webP(q) + gWebO;
    vec2 e2 = vweb(rot(1.1) * P * 3.3 + 5.1);
    vec2 e3 = vweb(rot(2.3) * P * 10.7 + 1.7);
    float near = exp(-e1.x / 0.2);
    float f1 = exp(-e1.x / 0.045), f2 = exp(-e2.x / 0.05) * (0.04 + 0.96 * near);
    float f3 = exp(-e3.x / 0.06) * (0.02 + 0.98 * near) * lod(0.06 / (WS * 10.7), px);
    float clump = 0.25 + 1.2 * sq(mix(0.5, vnoise(q * 16.0 + 4.0), lod(1.0 / 16.0, px))) + 0.6 * mix(0.5, vnoise(q * 47.0), lod(1.0 / 47.0, px));
    vec3 fc = mix(uCool, vec3(0.42, 0.28, 1.0), 0.4);
    vec3 nc = mix(uWarm, uLight, 0.45);
    // (the glow thins as the zoom resolves it into galaxies: the sky between them stays dark)
    float kd = smoothstep(4e-5, 2e-3, px);
    vec3 col = fc * (0.2 * f1 + 0.1 * f2 + 0.06 * f3 + 0.012 * near) * clump * kd;
    col += mix(nc, fc, 0.3) * (0.3 * exp(-e1.y / 0.06) * clump + 0.18 * exp(-dot(q, q) / 0.0012)) * kd;
    if (gals) {
        // the galaxies, gathered along the filaments and at the nodes (none right at the focus: ours is there)
        for (int L = 0; L < 2; L++) {
            float C = L == 0 ? 0.055 : 0.016, fl = float(L);
            vec2 b0 = floor(q / C - 0.5);
            for (int j = 0; j < 4; j++) {
                vec2 id = b0 + vec2(float(j & 1), float(j >> 1));
                vec2 c = (id + 0.25 + 0.5 * hash2(id + 41.0 + fl * 17.0)) * C;
                if (length(c) < (L == 0 ? 0.085 : 0.06)) continue;
                vec2 ec;
                float dc = webDens(c, ec);
                if (hash(id + 7.0 + fl * 3.0) > dc * (L == 0 ? 0.3 : 0.45)) continue;
                float s = hash(id + 13.0 + fl * 5.0);
                float R = C * (L == 0 ? mix(0.1, 0.5, s * s * s) : mix(0.1, 0.38, s * s));
                col += miniGalaxy(q - c, R, px, s) * (L == 0 ? 1.0 : 0.7);
            }
        }
    }
    return col;
}

// ---- a galaxy: the one we dive into, a spiral seen a little tilted, the focus in one of its arms
const vec2 GC = vec2(0.30, 0.19);
const float GR = 0.55, GTILT = 0.74, GPA = 0.5;
const vec2 NEB = vec2(-0.011, -0.031);  // a nebula by the focus (seen whole at the star's stage)
vec2 galD(vec2 q) { vec2 d = rot(GPA) * (q - GC); d.y /= GTILT; return d / GR; }
float galPhase(vec2 d) { return 2.0 * (atan(d.y, d.x) - 3.0 * log(max(length(d), 1e-3))); }
vec3 heroGalaxy(vec2 q, float px) {
    vec2 d = galD(q);
    float pw = px / (GR * GTILT);
    float r = length(d);
    float psi = galPhase(d) - galPhase(galD(vec2(0.0)));     // (an arm's crest runs through the focus)
    float arm = sq(0.5 + 0.5 * cos(psi));
    arm = mix(0.4, arm, smoothstep(0.06, 0.2, r));
    float clump = 0.5 + mix(0.5, fbm(d * 7.0 + 3.0), lod(0.14, pw)) + 0.4 * mix(0.5, vnoise(d * 30.0), lod(0.035, pw));
    float disk = exp(-r / 0.3) * (1.0 - smoothstep(0.8, 1.35, r));
    float dl = pow(0.5 + 0.5 * cos(psi + 0.75), 6.0) * smoothstep(0.1, 0.3, r) * (1.0 - smoothstep(0.85, 1.15, r));
    dl *= 0.4 + mix(0.5, fbm(d * 13.0 - 2.0), lod(0.08, pw)) * 1.2;
    float absorb = exp(-dl * 2.4);
    // (its diffuse light thins as the zoom resolves it into stars: the sky between them stays dark)
    float kd = mix(0.04, 1.0, smoothstep(3e-4, 3e-3, px));
    vec3 col = (mix(uWarm2, uLight, 0.5) * disk * 0.3 + mix(uCool, uLight, 0.3) * disk * arm * clump * 1.3) * absorb * kd;
    col += vec3(0.25, 0.1, 0.03) * dl * disk * 0.15;                                   // the dust's warm rim
    // the bulge, golden
    float r2 = dot(d, d);
    col += mix(uWarm2, uLight, 0.62) * (2.4 * blob(r2, 0.1, pw) + 1.6 * blob(r2, 0.035, pw)) * (1.0 + 0.06 * uTide * uTideAmp);
    // rose knots of star birth along the arms, wisps in them once they are big
    {
        float C = 0.07;
        vec2 b0 = floor(d / C - 0.5);
        for (int j = 0; j < 4; j++) {
            vec2 id = b0 + vec2(float(j & 1), float(j >> 1));
            vec2 c = (id + 0.25 + 0.5 * hash2(id + 71.0)) * C;
            float rc = length(c);
            if (rc < 0.15 || rc > 1.0 || hash(id + 3.0) > 0.4) continue;
            if (sq(0.5 + 0.5 * cos(galPhase(c) - galPhase(galD(vec2(0.0))))) < 0.45) continue;
            float R = C * mix(0.08, 0.3, sq(hash(id + 5.0)));
            vec2 dd = d - c;
            float k = blob(dot(dd, dd), R, pw) * sq(0.3 + 1.2 * mix(0.5, fbm(dd / R * 2.0 + id), lod(R * 0.5, pw)));
            col += mix(mix(uWarm, vec3(1.0, 0.25, 0.6), 0.5), uLight, 0.15) * k * 1.6 * exp(-r / 0.5);
        }
    }
    // the nebula by the focus: rose and teal veils, filaments in them as the zoom comes near
    {
        vec2 dn = (q - NEB) / 0.013;
        float rn = dot(dn, dn);
        if (rn < 9.0) {
            float pn = px / 0.013;
            float w1 = mix(0.5, fbm(dn * 1.6 + 2.0), lod(0.6, pn)), w2 = mix(0.5, fbm(rot(1.3) * dn * 3.4 - 4.0), lod(0.3, pn));
            float fil = mix(0.0, 1.0 - abs(2.0 * fbm(dn * 2.4 + 9.0) - 1.0), lod(0.4, pn));
            float env = exp(-rn / 1.6);
            vec3 rose = mix(uWarm, vec3(1.0, 0.2, 0.55), 0.55), teal = mix(uCool, vec3(0.2, 1.0, 0.8), 0.45);
            col += rose * env * (0.25 + 1.1 * w1 * w1 + 0.9 * pow(fil, 6.0)) * 0.9;
            col += teal * exp(-rn / 0.7) * (0.15 + 0.9 * w2 * w2) * 0.5;
            col *= 1.0 - 0.6 * env * smoothstep(0.55, 0.75, w2) * lod(0.3, pn);                // dark globules
        }
    }
    // its stars, as many as its light
    col += starfield(q, px, 0.03, sat(disk * (0.6 + 2.6 * arm) * 0.9 + 0.01), 3.0) * 0.9;
    return col;
}

// ---- a star: the sun, its planets' orbits (the one we dive into crossing the focus)
const vec2 SUN = vec2(0.26, 0.56);
vec4 starSystem(vec2 q, float px) {
    vec2 d = q - SUN;
    float r = length(d), th = atan(d.y, d.x);
    const float SR = 0.042;
    vec3 sc = mix(uLight, vec3(1.0, 0.78, 0.45), 0.45);
    float br = 1.0 + 0.1 * uTide * uTideAmp;
    float o = max(r - SR, 0.0);
    vec3 col = sc * (1.1 * exp(-o / (0.01 * br)) + 0.4 * exp(-o / (0.05 * br)) + 0.06 * exp(-r / 0.6));
    col += sc * 0.3 * exp(-o / 0.06) * pow(0.5 + 0.5 * cos(th * 5.0 + 0.7 * sin(th * 3.0 + gT * 0.03)), 6.0);
    float a = 0.0;
    float disc = fillA(r - SR, px);
    if (disc > 0.0) {
        float mu = sqrt(max(0.0, 1.0 - sq(r / SR)));
        float gran = 0.85 + 0.3 * mix(0.5, vnoise(d / SR * 26.0 + gT * 0.02), lod(SR / 26.0, px));
        col = mix(col, sc * 5.0 * (0.35 + 0.65 * mu) * gran, disc);
        a = disc;
    }
    // the orbits: fine threads of light; the planets on them, lit from the sun
    float lw = max(px, 0.0011);
    for (int i = 0; i < 5; i++) {
        float Ro = i == 0 ? 0.17 : i == 1 ? 0.33 : i == 2 ? length(SUN) : i == 3 ? 1.02 : 1.5;
        float fade = i == 2 ? smoothstep(0.02, 0.2, length(q)) : 1.0;
        col += mix(uCool, uLight, 0.45) * 0.2 * lineA(r - Ro, 0.0, lw) * fade * (1.0 - a);
        if (i == 2) continue;
        float pa = hash1(float(i) * 3.7 + 1.0) * TAU + gT * 0.004 / Ro;
        vec2 pc = SUN + Ro * vec2(cos(pa), sin(pa));
        float pr = i == 3 ? 0.024 : i == 4 ? 0.015 : i == 1 ? 0.011 : 0.007;
        vec2 pd = (q - pc) / pr;
        float rr = length(pd);
        if (rr > 2.6) continue;
        vec2 ls = normalize(SUN - pc);
        vec3 pcol = i == 0 ? mix(uWarm2, uSand, 0.4) : i == 1 ? mix(uWarm, uSand, 0.3) * 1.2 : i == 4 ? mix(uCool, uLight, 0.4) : mix(uWarm2, uSand, 0.5) * 1.2;
        if (i == 3) pcol *= 0.75 + 0.35 * sin(pd.y * 9.0 + 1.5 * sin(pd.x * 3.0));     // the giant's bands
        float pdisc = fillA(rr - 1.0, px / pr);
        float lit = sat(dot(pd, ls) * 0.9 + 0.35) * (0.4 + 0.6 * sqrt(max(0.0, 1.0 - rr * rr)));
        // the giant's rings, behind it and in front
        if (i == 3) {
            vec2 rd = rot(0.4) * pd;
            float re = length(rd * vec2(1.0, 3.2));
            float ring = smoothstep(1.4, 1.5, re) * (1.0 - smoothstep(2.2, 2.35, re)) * (0.6 + 0.4 * sin(re * 30.0));
            float front = step(0.0, rd.y) + (1.0 - step(0.0, rd.y)) * (1.0 - pdisc);
            col += mix(uSand, uLight, 0.5) * ring * 0.5 * front;
        }
        col = mix(col, pcol * lit * 1.6, pdisc);
        a = max(a, pdisc);
    }
    return vec4(col, a);
}

// ---- the planet: oceans, continents, coasts, clouds; the same surface from orbit down to its shore
const float PR = 0.4;                         // its radius in its stage's units
const vec2 MOON = vec2(-0.2, 0.62);
const float MR = 0.055;
const float LIFT = 0.007;                     // the focus stands this high on the rocks, by the sea
vec3 sunL() { vec2 s = normalize(rot(-REL) * SUN); return normalize(vec3(s * 0.8, 0.6)); }
vec3 noised(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f), du = 6.0 * f * (1.0 - f);
    float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)), d = hash(i + vec2(1, 1));
    float k = a - b - c + d;
    return vec3(a + (b - a) * u.x + (c - a) * u.y + k * u.x * u.y, du * vec2(b - a + k * u.y, c - a + k * u.x));
}
// The land's height (sea level 0), its slope and a fine texture, along the surface g (planet units) from the focus:
// octaves of noise, each less its value at the focus (so the coast runs through the focus at every scale), down to
// the pixel; the focus itself stands LIFT high. The texture keeps only the octaves seen on the screen.
vec4 earthH(vec2 g, float pg) {
    vec4 h = vec4(LIFT, 0.0, 0.0, 0.0);
    float a = 0.5, f = 2.2;
    mat2 R = mat2(1.0);
    for (int k = 0; k < 24; k++) {
        float lpx = 1.0 / (f * pg);
        if (lpx < 1.0) break;
        float w = smoothstep(1.0, 3.0, lpx);
        vec2 o = vec2(float(k) * 7.31 + 0.5, float(k) * 3.17 + 0.3);
        vec3 n = noised(R * g * f + o);
        n.x -= vnoise(o);
        h.x += w * a * n.x;
        h.yz += w * a * f * (n.yz * R);
        h.w += w * (1.0 - smoothstep(60.0, 240.0, lpx)) * (n.x + vnoise(o) - 0.5);
        a *= 0.5;
        f *= 2.0;
        R = mat2(0.8, -0.6, 0.6, 0.8) * R;
    }
    return h;
}
float cloudCover(vec2 g, float pg, float t) {
    vec2 p = g + 0.07 * vec2(vnoise(g * 5.0 + 1.0), vnoise(g * 5.0 + 9.0));
    float c = 0.0, a = 0.5, f = 4.0;
    for (int k = 0; k < 11; k++) {
        float w = smoothstep(1.0, 3.0, 1.0 / (f * pg));
        if (w <= 0.0) { c += a; break; }      // (the rest: their mean)
        c += a * mix(0.5, vnoise(p * f + float(k) * 5.3 + vec2(0.05, 0.02) * t), w);
        a *= 0.5;
        f *= 2.03;
    }
    float cover = smoothstep(0.52, 0.74, c + 0.06 * sin(g.y * 9.0));
    return cover * smoothstep(0.012, 0.05, length(g)) * smoothstep(5e-6, 2e-5, pg);   // a clear sky over the coast at the focus
}
// the rocks of the shore, close: slate split into slabs, pink crust and weed by the water, lichen above it
vec3 shoreRock(vec2 g, float pg, float h, float tex) {
    vec2 id;
    vec3 v = voronoi(g * 3000.0 + 0.4 * vec2(vnoise(g * 5000.0), vnoise(g * 5000.0 + 3.0)), 0.8, 0.0, id);
)" +
R"(    float k = lod(1.0 / 3000.0, pg);
    float fis = exp(-v.y / max(0.05, pg * 3000.0 * 1.5)) * k;
    vec3 rc = uRock * (0.62 + 0.4 * hash(id) * k + 0.25 * tex) * (1.0 - 0.5 * fis);
    float wet = 1.0 - smoothstep(0.0005, 0.003, h);
    rc = mix(rc, mix(uWarm, uSand, 0.5) * 0.8, smoothstep(0.55, 0.7, vnoise(g * 9000.0 + 5.0)) * wet * 0.6 * lod(1.0 / 9000.0, pg));
    rc = mix(rc, uWeed * 0.95, smoothstep(0.55, 0.7, vnoise(g * 6000.0 - 3.0)) * wet * 0.7 * lod(1.0 / 6000.0, pg));
    rc = mix(rc, mix(uWarm2, uRock, 0.4), smoothstep(0.62, 0.75, vnoise(g * 7000.0 + 9.0)) * smoothstep(0.004, 0.009, h) * 0.35 * lod(1.0 / 7000.0, pg));
    return rc * mix(0.82, 0.6, wet * 0.5);
}
// The planet's surface at qp (its stage's units, inside its disc) seen through a pixel pp: lit by the sun, its
// night side sparkling with cities, clouds and their shadows, the air's haze toward the limb.
vec3 planetSurface(vec2 qp, float pp) {
    float s = length(qp) / PR;
    float z = sqrt(max(1.0 - s * s, 0.0));
    vec3 n = vec3(qp / PR, z);
    vec2 g = qp * (s < 1e-3 ? 1.0 + s * s / 6.0 : asin(min(s, 1.0)) / s);   // along the surface from the focus
    float pg = pp / max(z, 0.12);
    float t = uEarthT;
    vec3 L = sunL();
    float day = dot(n, L);
    vec4 H = earthH(g, pg);
    float h = H.x, tex = H.w;
    vec3 nt = normalize(n - vec3(H.yz * (h > 0.0 ? 0.22 : 0.0), 0.0));
    vec3 sunC = uLight * 1.25;
    vec3 col;
    // the rocks or the sand of the coast (the focus is on rocks)
    float rk = mix(0.5, vnoise(g * 260.0), lod(1.0 / 260.0, pg)) * 0.6 + mix(0.5, vnoise(g * 1100.0 + 4.0), lod(1.0 / 1100.0, pg)) * 0.4;
    rk = max(smoothstep(0.48, 0.58, rk), smoothstep(0.03, 0.012, length(g)));
    if (h > 0.0) {
        float m = fbm(g * 2.6 + 17.0);
        float lat = abs(g.y) / (PR * 1.5708);
        vec3 lush = mix(uWeed, vec3(0.05, 0.32, 0.04), 0.55) * 1.2, dry = mix(uWarm2, uSand, 0.35) * 1.05;
        float wetness = m + 0.12 * tex + 0.3 * exp(-dot(g, g) / 0.0025);   // (green by the coast at the focus)
        vec3 land = mix(dry, lush, smoothstep(0.4, 0.56, wetness));
        land = mix(land, lush * 0.5, smoothstep(0.56, 0.72, wetness) * 0.75);                    // forests
        land = mix(land, mix(uRock, uWarm2, 0.3) * 1.15, smoothstep(0.2, 0.36, h + 0.05 * tex));   // mountains
        land = mix(land, vec3(0.86, 0.91, 1.0), smoothstep(0.4, 0.5, h + 0.06 * tex));            // their snow
        land = mix(land, vec3(0.86, 0.92, 1.0), smoothstep(0.72, 0.8, lat + 0.08 * tex));        // the ice caps
        land *= 0.82 + 0.4 * tex;
        // the coast: a band of sand or rock above the water, wet by it
        vec3 sand = uSand * 1.1 * (0.9 + 0.25 * tex);
        vec3 coast = mix(sand, shoreRock(g, pg, h, tex), rk);
        coast = mix(coast, coast * 0.6 + uWater * 0.08, (1.0 - smoothstep(0.0, 0.0005, h)) * (1.0 - rk * 0.5));
        // (a broad shelf of rock round the focus, elsewhere a thin rocky coast or a beach)
        float shelf = rk * smoothstep(0.014, 0.005, length(g));
        land = mix(land, coast, 1.0 - smoothstep(mix(mix(0.0018, 0.004, rk), 0.022, shelf), mix(mix(0.003, 0.007, rk), 0.034, shelf), h));
        float lam = max(dot(nt, L), 0.0);
        col = land * sunC * (0.05 + 1.3 * lam);
        // tide pools in the rocks (not the one at the focus: it is drawn whole by the shore)
        if (rk > 0.5 && h < 0.012) {
            vec2 id;
            vec2 gw = g * 1600.0 + 0.2 * vec2(vnoise(g * 4000.0), vnoise(g * 4000.0 + 7.0));
            vec3 v = voronoi(gw, 0.7, 0.0, id);
            if (hash(id + 17.0) < 0.14 && length(g) > 0.0012) {
                float pw = pg * 1600.0;
                float pd = v.x - 0.13 - 0.1 * hash(id) + 0.05 * (vnoise(gw * 4.0) - 0.5);
                float pool = fillA(pd, pw * 1.5) * lod(1.0 / 1600.0, pg);
                vec3 bed = mix(uSand, uRock * 1.4, 0.35 + 0.3 * vnoise(gw * 6.0)) * (0.6 + 0.8 * caustic(gw * 5.0, t * 30.0, pw * 5.0));
                vec3 pc = waterOver(bed, 0.06 + 0.35 * smoothstep(0.0, -0.15, pd)) * sunC;
                pc = mix(pc, mix(uLight, uShallow, 0.5) * sunC * 0.8, lineA(pd + 0.015, 0.012, pw * 1.5) * 0.4);
                col *= 1.0 - 0.35 * fillA(pd - 0.06, pw * 2.0 + 0.03) * lod(1.0 / 1600.0, pg) * (rk - 0.5) * 2.0;   // wet rock round it
                col = mix(col, pc, pool * (rk - 0.5) * 2.0);
            }
        }
    } else {
        float dep = -h;
        vec3 oDeep = mix(uDeep, vec3(0.0, 0.01, 0.07), 0.5), oMid = mix(uWater, vec3(0.0, 0.07, 0.3), 0.5) * 0.7;
        vec3 oShelf = mix(uWater, uShallow, 0.5) * 1.25;
        vec3 sea = mix(oMid, oDeep, smoothstep(0.03, 0.25, dep));
        sea = mix(sea, oShelf, exp(-dep / 0.012));
        // the bed seen through the shallows
        sea = mix(sea, mix(uSand * 0.95, uRock * 0.9, rk) * (0.75 + 0.4 * tex), exp(-dep / 0.0015) * 0.5);
        // the swell's texture, and the surf: lines of foam along the coast, rolling in, a lace on the sand
        sea *= 0.92 + 0.16 * tex * (1.0 - smoothstep(0.02, 0.1, dep));
        float sp = 0.00045;
        float surfZ = 1.0 - smoothstep(0.0012, 0.0035, dep);
        float ph = dep / sp * TAU + t * 110.0 + 3.0 * mix(0.5, vnoise(g * 4000.0), lod(1.0 / 4000.0, pg));
        float k = lod(sp / 3.0, pg);
        float brk = smoothstep(0.3, 0.6, mix(0.5, vnoise(g * 2500.0 + vec2(t * 3.0, 0.0)), lod(1.0 / 2500.0, pg)));
        float foam = mix(0.1, pow(0.5 + 0.5 * sin(ph), 12.0) * brk, k) * surfZ * exp(-dep / 0.0012);
        float lace = (1.0 - smoothstep(0.0, 0.00035, dep)) * mix(0.5, smoothstep(0.35, 0.65, vnoise(g * 12000.0 + vec2(0.0, t * 40.0))), lod(1.0 / 12000.0, pg));
        foam = max(foam, lace * 0.9);
        sea = mix(sea, vec3(0.95, 0.97, 1.0), foam * 0.85);
        float lam = max(dot(n, L), 0.0);
        col = sea * sunC * (0.06 + 1.25 * lam);
        // the sun's glint on the sea
        vec3 Hv = normalize(L + vec3(0.0, 0.0, 1.0));
        float sp2 = max(dot(n, Hv), 0.0);
        col += sunC * (pow(sp2, 300.0) * 2.5 + pow(sp2, 40.0) * 0.12) * (1.0 - foam);
    }
    // the night side: the cities' lights, along the coasts
    float night = smoothstep(0.1, -0.12, day);
    if (night > 0.0 && h > 0.0) {
        vec3 lights = vec3(0.0);
        float C = 0.012;
        for (int k = 0; k < 4; k++) {
            if (C / pg < 2.0) break;
            vec2 id = floor(g / C);
            if (hash(id + 31.0 + float(k)) > 0.55 - 0.3 * exp(-h / 0.06)) {
                vec2 c = (id + 0.2 + 0.6 * hash2(id + float(k) * 3.0)) * C;
                lights += exp(-dot(g - c, g - c) / sq(max(pg, C * 0.06)) * 0.8) * (0.4 + hash(id + 9.0));
            }
            C *= 0.5;
        }
        col += mix(uWarm2, vec3(1.0, 0.7, 0.3), 0.5) * lights * night * 1.4;
    }
    // clouds, their shadows on the ground
    float cov = cloudCover(g, pg, t);
    vec3 cc = vec3(0.96, 0.97, 1.0) * sunC * (0.05 + 1.25 * max(day, 0.0)) * (0.82 + 0.18 * tex);
    col = mix(col, cc, cov * 0.92);
    // the air: blue toward the limb, a thin haze from high up (it thins as we come down)
    vec3 atmo = mix(uCool, vec3(0.25, 0.55, 1.0), 0.55);
    float limb = pow(1.0 - z, 2.0);
    col = mix(col, atmo * sunC * (0.08 + 1.1 * max(day + 0.15, 0.0)), limb * 0.7);
    col += atmo * 0.05 * smoothstep(1e-5, 1e-3, pg) * max(day, 0.0);
    return col;
}
vec4 moon(vec2 q, float px, vec3 L) {
    vec2 d = (q - MOON) / MR;
    float r = length(d);
    float a = fillA(r - 1.0, px / MR);
    if (a <= 0.0) return vec4(0.0);
    vec3 n = vec3(d, sqrt(max(0.0, 1.0 - r * r)));
    float pw = px / MR;
    vec2 cid;
    vec3 v = voronoi(d * 4.5 + 3.0, 0.9, 0.0, cid);
    float cr = step(0.45, hash(cid + 2.0)) * lod(0.22, pw);
    vec3 alb = vec3(0.6, 0.58, 0.55) * (0.75 + 0.35 * fbm(d * 3.0));
    alb = mix(alb, alb * 0.55, smoothstep(0.5, 0.62, fbm(d * 1.4 + 7.0)));
    alb *= 1.0 - 0.3 * cr * (1.0 - smoothstep(0.22, 0.3, v.x));
    alb += 0.25 * cr * lineA(v.x - 0.3, 0.025, pw * 4.5);
    vec3 col = alb * uLight * (0.015 + 1.3 * max(dot(n, L), 0.0));
    return vec4(col * a, a);
}

// ---- 0. the universe: the cosmic web, galaxies strung along its filaments, a cluster at the focus
World stageWeb(vec2 q, float px) {
    World W;
    vec3 col = webLight(q, px, true);
    // (deep in the strings, the whole universe is a knot of light at the focus that opens as it comes)
    if (gNext > 0.5) col *= exp(-dot(q, q) / sq(mix(1.0, 40.0, uTau * uTau)));
    W.bg = spaceBg();
    W.obj = vec4(col, 0.0);
    W.host = 1.0;
    return W;
}
// ---- 1. a galaxy among its neighbours
World stageGalaxy(vec2 q, float px) {
    World W;
    W.bg = spaceBg() + (gWantBg > 0.5 ? webLight(up(q), px * 0.0625, true) : vec3(0.0));
    W.obj = vec4(heroGalaxy(q, px), 0.0);
    W.host = 1.0;
    return W;
}
// ---- 2. a star, in an arm of the galaxy, by a nebula
World stageStar(vec2 q, float px) {
    World W;
    W.bg = spaceBg();
    if (gWantBg > 0.5) W.bg += heroGalaxy(up(q), px * 0.0625);      // (the web's glow is all resolved by now)
    W.obj = starSystem(q, px);
    W.host = 1.0;
    return W;
}
// ---- 3. the planet, its moon
World stagePlanet(vec2 q, float px) {
    World W;
    W.bg = spaceBg();
    if (gWantBg > 0.5) W.bg += heroGalaxy(up(up(q)), px / 256.0) + starSystem(up(q), px * 0.0625).rgb;
    vec3 L = sunL();
    float r = length(q);
    vec3 atmo = mix(uCool, vec3(0.25, 0.55, 1.0), 0.55);
    vec2 l2 = normalize(L.xy);
    vec3 col = atmo * uLight * 0.9 * exp(-max(r - PR, 0.0) / 0.014) * sat(0.1 + 0.9 * (dot(q / max(r, 1e-4), l2) * 0.6 + 0.45));
    float a = fillA(r - PR, px);
    if (a > 0.0) col = mix(col, planetSurface(q, px), a);
    vec4 m = moon(q, px, L);
    W.obj = over(m, vec4(col, a));
    W.host = 1.0;
    return W;
}
// ---- 4. the coast, from high above
World stageCoast(vec2 q, float px) {
    World W;
    W.bg = gWantBg > 0.5 ? planetSurface(up(q), px * 0.0625) : vec3(0.0);
    W.obj = vec4(0.0);
    W.host = 1.0;
    return W;
}
// ---- 5. the shore: rocks by the sea, tide pools in them, the one at the focus the very rock pool of the next stage
World stageShore(vec2 q, float px) {
    World W;
    W.bg = gWantBg > 0.5 ? planetSurface(up(up(q)), px / 256.0) : vec3(0.0);
    W.obj = vec4(0.0);
    vec2 qd = down(q);
    float rr = length(qd);
    if (rr < 4.2) {
        float t0 = gT;
        gT = gTn;
        World P = stagePool(qd, px * 16.0);
        gT = t0;
        float m = 1.0 - smoothstep(2.6, 4.2, rr);
        W.obj = vec4(P.bg * m, m);
    }
    W.host = 1.0;
    return W;
}

World stage(int s, vec2 q, float px) {
    if (s == 0) return stageWeb(q, px);
    if (s == 1) return stageGalaxy(q, px);
    if (s == 2) return stageStar(q, px);
    if (s == 3) return stagePlanet(q, px);
    if (s == 4) return stageCoast(q, px);
    if (s == 5) return stageShore(q, px);
    if (s == 6) return stagePool(q, px);
    if (s == 7) return stageFloor(q, px);
    if (s == 8) return stagePlankton(q, px);
    if (s == 9) return stageDiatom(q, px);
    if (s == 10) return stageCell(q, px);
    if (s == 11) return stageMolecules(q, px);
    if (s == 12) return stageAtom(q, px);
    if (s == 13) return stageFoam(q, px);
    return stageStrings(q, px);
}

void main() {
    vec2 b = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    vec2 p = (b - uFocus) / uViewH.y;
    // the drop's wave: a slow ripple through the fabric, out from the focus
    if (uWave.y > 0.0) {
        float pr = length(p);
        float front = uWave.x * 0.45;
        float k = exp(-sq((pr - front) / 0.5)) * uWave.y * exp(-uWave.x * 0.3);
        p += p / max(pr, 1e-3) * sin((pr - front) * 6.0) * 0.005 * k;
    }
    // a little softness behind the well and the HUD's columns (the stack and the score read over a calmer world)
    vec2 qw = abs(b) - vec2(5.0, 10.0);
    float hud = 1.0 - smoothstep(-1.0, 2.5, length(max(qw, 0.0)) + min(max(qw.x, qw.y), 0.0));
    // and behind the HUD's columns (the score, the next pieces)
    for (int i = 0; i < 2; i++) {
)" +
R"(        vec4 cr = i == 0 ? vec4(-11.2, -6.8, -6.0, 10.4) : vec4(6.0, -3.6, 11.2, 10.4);
        vec2 qc = abs(b - (cr.xy + cr.zw) * 0.5) - (cr.zw - cr.xy) * 0.5;
        hud = max(hud, 0.8 * (1.0 - smoothstep(-0.5, 2.0, length(max(qc, 0.0)) + min(max(qc.x, qc.y), 0.0))));
    }
    float blurP = 0.003 * hud;
    // the stage we are in, and the next one, its things already there, tiny; then a crossfade into it
    vec4 L0 = uLayer[0], L1 = uLayer[1];
    gWebO = webNode() - webP(vec2(0.0));
    gT = uLayerT0;
    gTn = uLayerT1;
    World A = stage(int(L0.x + 0.5), rot(L0.w) * p * L0.z, (uPxP + blurP) * L0.z);
    gT = uLayerT1;
    gNext = 1.0;
    gWantBg = uTau > 0.0 ? 1.0 : 0.0;
    World B = stage(int(L1.x + 0.5), rot(L1.w) * p * L1.z, (uPxP + blurP) * L1.z);
    float appear = smoothstep(-4.0, -3.3, L1.y);
    vec4 nb = B.obj * appear * mix(A.host, 1.0, uTau);
    vec3 a = over(nb, over(A.obj, vec4(A.bg, 1.0))).rgb;
    vec3 bb = over(B.obj, vec4(B.bg, 1.0)).rgb;
    vec3 c = mix(a, bb, uTau);
    // FEVER: the world's hues turn slowly with the depth
    if (uFever > 0.0) {
        float an = uFever * (uDepth * 0.08 + uTime * 0.015);
        vec3 k = vec3(0.57735);
        float ca = cos(an), sa = sin(an);
        c = max(c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca), 0.0);
    }
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(l), hud * 0.25);                                        // calmer behind the well and the HUD
    c = mix(c, vec3(l) * vec3(0.85, 0.95, 1.12), uSlow * 0.55);            // slow motion: a hush
    c = mix(uBridgeCol, c, uWorldA);
    frag = vec4(c, 1.0);
}
)";
inline const char* WORLD_FS = WORLD_FS_SRC.c_str();

// ------------------------------------------------------------------------------------------------ composite
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec2 uTexel;
void main() {
    // a light sharpening of the upscaled world
    vec3 c = texture(uWorld, vUv).rgb;
    vec3 n = texture(uWorld, vUv + vec2(uTexel.x, 0.0)).rgb + texture(uWorld, vUv - vec2(uTexel.x, 0.0)).rgb +
             texture(uWorld, vUv + vec2(0.0, uTexel.y)).rgb + texture(uWorld, vUv - vec2(0.0, uTexel.y)).rgb;
    c = max(c + (c - n * 0.25) * 0.25, 0.0);
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ bubbles
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
uniform float uPxPerUnit;
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 2) {
        // a bubble: a thin bright ring, a glint
        float ring = 1.0 - smoothstep(px * 0.8, px * 2.0, abs(r - 0.82));
        float g = exp(-dot(vLocal - vec2(-0.35, 0.35), vLocal - vec2(-0.35, 0.35)) * 40.0);
        float v = ring * 0.8 + g * 0.9 + (1.0 - smoothstep(0.7, 0.85, r)) * 0.06;
        frag = vec4(c * v * vB.a, v * vB.a * 0.6);
        return;
    }
    // a mote of silt
    float v = exp(-r * r * 9.0) * smoothstep(1.0, 0.7, r);
    frag = vec4(c * v * vB.a, v * vB.a);
}
)";

// ------------------------------------------------------------------------------------------------ the veil
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
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.5 + exp(-abs(dw) * 3.0) * 0.1;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ pieces: sea glass
// Each piece one pebble of sea glass (its cells merge): frosted, translucent, worn round, the light of the water
// moving through it, a bright worn edge. The ghost is its edge alone; a cleared cell melts into bubbles.
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
    const float r = 0.2, rc = 0.08;
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
float vor(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 h = hash2(i + g);
            vec2 o = g + 0.5 + 0.45 * sin(uTime * (0.4 + h * 0.4) + 6.2831 * h) - f;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return sqrt(d2) - sqrt(d1);
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
    float sd = shapeSdf(vLocal, mk, 0.06);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    vec3 hi = mix(tint, vec3(1.0), 0.55);
    float rimW = max(aa * 1.5, 0.03);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    if (mode == 1) {
        float a = (rim * 0.8 + inside * 0.07) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // frosted glass: the body's tint deepening inward (thicker), worn edges brighter
    float thick = smoothstep(0.0, 0.32, e);
    vec3 col = tint * mix(1.05, 0.62, thick);
    // the frost: a fine fixed grain, and the water's light moving through the glass
    float grain = hash(floor(vWorld * 26.0)) * 0.5 + vnoise(vWorld * 9.0) * 0.5;
    col *= 0.93 + 0.12 * grain;
    float cst = exp(-vor(vWorld * 0.9 + vec2(0.0, uTime * 0.05)) / 0.09);
    col += mix(tint, uLightC, 0.5) * cst * 0.16 * smoothstep(0.05, 0.25, e);
    // a soft light from the upper left: brighter there, a little shade bottom right
    vec2 g = vec2(dFdx(sd), dFdy(sd));
    float lit = dot(normalize(g + 1e-6), normalize(vec2(-0.6, 0.7)));
    col *= 1.0 + 0.18 * lit * (1.0 - thick);
    col = mix(col, hi * 1.1, rim * 0.85);
    col += hi * 0.12 * exp(-abs(e - 0.09) / 0.03);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.5, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        // the glass melts into bubbles: holes open and grow
        float n = vnoise(vWorld * 3.1) * 0.6 + vnoise(vWorld * 7.3) * 0.4;
        keep = smoothstep(erase * 1.3 - 0.35, erase * 1.3 - 0.05, n) * (1.0 - smoothstep(0.6, 1.0, erase));
        col = mix(col, hi * 1.2, smoothstep(0.0, 0.5, erase) * 0.6);
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * 0.95 * vB.a * keep;
    frag = vec4(col * a, a);
}
)";

// ------------------------------------------------------------------------------------------------ lettering
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

// ------------------------------------------------------------------------------------------------ post
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

} // namespace tidepool_shaders

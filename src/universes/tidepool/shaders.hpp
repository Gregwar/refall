#pragma once
// TIDE POOL's shaders: the world (one continuous zoom through nine scales, each drawn procedurally in its own
// frame and nested in the one before it), the veil behind the well, the sea-glass pieces, bubbles, lettering.
//
// The nesting: every scale (a "stage") is drawn in local coordinates q with the focus of the zoom at its origin.
// Stage k+1 lives inside stage k, 16 times smaller (S = 4 octaves), inside a disc of radius RB = 3.3 of its own
// units (0.21 of stage k's): the drop on the boulder, the diatom in the drop, the pore in the diatom... Each stage
// draws its world over the annulus it is seen in (radius 3.3 down to 0.21) and is transparent outside RB; the
// compositor draws the innermost first and stops as soon as a stage is opaque. Things in a stage are sized in
// proportion to their distance from the focus (often scattered on a log-polar lattice), so the screen keeps the same
// density of detail at every zoom, and every edge is anti-aliased against the pixel's size in the stage's units.

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
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH, uFocus;
uniform float uPxP;           // p units per pixel of this target
uniform vec4 uLayer[4];       // stage, local zoom u, scale (stage units per p unit), angle
uniform int uLayers;
uniform float uTime, uTide, uTideAmp, uLife, uPeak, uKick, uSlow, uBridge, uWorldA;
uniform vec4 uWave;           // the drop's wave: age (s), strength, -, -
uniform vec3 uDeep, uWater, uShallow, uLight, uRock, uSand, uWarm, uWarm2, uWeed, uCool, uGlow, uBridgeCol;
uniform float uLum, uFever, uDepth;
uniform vec4 uBands;          // the music's bands (low, low-mid, high-mid, high), quickly smoothed

const float RB = 3.3;         // a stage's disc, in its own units
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

// ======================================================================= 0. the rock pool at low tide
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
vec4 stagePool(vec2 q, float px, float lu, bool inner) {
    float r = length(q);
    float t = uTime;
    float level = 0.4 + 0.022 * uTide * uTideAmp + 0.07 * uWave.y * bump(uWave.x - 2.5, 1.8);
    float e = max(px * 1.5, 0.004);
    float h = poolH(q, px);
    float hx = poolH(q + vec2(e, 0.0), px), hy = poolH(q + vec2(0.0, e), px);
    vec2 g = vec2(hx - h, hy - h) / e;
    vec3 n = normalize(vec3(-g * 0.5, 1.0));
    vec3 L = normalize(vec3(-0.5, 0.6, 0.65));
    float dep = level - h;
    float aw = max(px * 1.5, 0.002) * (length(g) + 0.3);
    vec3 col;
    {
        // rock: dark slate, lichen high and dry, pink coralline crust and green weed in the wet band near the water
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
        vec3 hv = normalize(L + vec3(0.0, 0.0, 1.0));
        col += uLight * pow(sat(dot(n, hv)), 50.0) * mix(0.9, 0.12, dry);
    }
    float waterA = smoothstep(-aw, aw, dep);
    if (dep > -aw) {
        // the pool: sand and pebbles through clear water, caustics, a breath of the sky on its surface
        vec2 rip = vec2(sin(q.x * 9.0 + t * 1.1 + 3.0 * fbm(q * 2.0)), cos(q.y * 8.0 - t * 0.9)) * 0.004 * (0.4 + uTideAmp);
        vec2 rq = q - g * 0.015 + rip;
        vec3 bed = sandBed(rq, px, t, 3.2);
        // the rock continues under the water where it is shallow
        bed = mix(bed, col * 0.9, smoothstep(0.08, 0.0, dep) * 0.6);
        float c = caustic(rq * 2.6, t * 0.5, px * 2.6);
        float d = max(dep, 0.0);
        vec3 lit = bed * (0.5 + 1.0 * c * exp(-d * 2.0));
        vec3 wc = waterOver(lit, d + 0.04);
        wc += uShallow * 0.06 * smoothstep(-1.5, 1.5, q.y - q.x * 0.4);
        float men = lineA(dep, aw * 0.5, aw);
        wc = mix(wc, mix(uLight, uShallow, 0.5), men * 0.35);
        col = mix(col, wc, waterA);
    }
    // bioluminescence (MIDNIGHT): faint sparks in the pool
    if (uLum > 0.0 && dep > 0.0) {
        vec2 id;
        vec3 v = voronoi(q * 14.0, 1.0, t * 0.3, id);
        float tw = pow(sat(sin(t * (0.7 + hash(id)) + hash(id + 2.0) * 6.28)), 6.0);
        col += uGlow * exp(-v.x * v.x / 0.02) * tw * uLum * 0.5 * lod(1.0 / 14.0, px);
    }
    float a = 1.0;
    if (inner) {
        // seen from far inside the strings: only the pool's water glows; the rocks come with the arrival
        a = 1.0 - smoothstep(1.7, RB, r);
        a *= mix(smoothstep(-3.2, -0.6, lu), 1.0, waterA * (1.0 - smoothstep(1.4, 2.4, r)));
    }
    return layerC(col, a);
}

// ======================================================================= 1. a boulder in the pool: anemones, limpets
float boulderH(vec2 q) {
    float r = length(q);
    float w = 0.16 * fbm(q * 1.4 + 5.0) + 0.05 * fbm(q * 4.0);
    float dome = 1.0 - smoothstep(0.35, 1.3, r + w);
    return dome * 0.9 + 0.07 * fbm(q * 3.0 + 1.0) + 0.025 * fbm(q * 9.0);
}
// a beadlet anemone at c, radius R: a red column, blue beads at its rim, three whorls of soft tentacles
vec4 anemone(vec2 q, vec2 c, float R, float seed, float px, float t) {
    vec2 d = (q - c) / R;
    float rho = length(d);
    if (rho > 1.25) return vec4(0.0);
    float pw = px / R;
    float ang = atan(d.y, d.x);
    float open = 0.9 + 0.1 * uTide * uTideAmp + 0.04 * sin(t * 0.35 + seed * 9.0);
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
vec4 barnacles(vec2 q, float px, float level) {
    float cs = 22.0;
    vec2 s = q * cs;
    vec2 i = floor(s);
    vec4 acc = vec4(0.0);
    float k = lod(0.5 / cs, px);
    if (k <= 0.0) return acc;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 id = i + vec2(x, y);
            if (hash(id + 1.0) < 0.4) continue;
            vec2 c = id + 0.3 + 0.4 * hash2(id);
            vec2 cw = c / cs;
            float hc = boulderH(cw);
            float band = smoothstep(level + 0.03, level + 0.08, hc) * (1.0 - smoothstep(level + 0.28, level + 0.42, hc)) * smoothstep(0.26, 0.32, length(cw));
            if (band <= 0.0) continue;
            float rad = 0.36 * (0.65 + 0.5 * hash(id));
            vec2 d = (s - c) / rad;
            float rr = length(d);
            if (rr > 1.6) continue;
            float pw = px * cs / rad * 1.3;
            float ang = atan(d.y, d.x) + hash(id + 4.0) * 6.28;
            float fl = 0.5 + 0.5 * cos(ang * 6.0);
            float body = fillA(rr - (1.0 + 0.07 * fl), pw);
            vec3 n = normalize(vec3(d / max(rr, 1e-3) * 0.9 * sat(rr * 1.5), 1.0));
            float lit = 0.4 + 0.6 * sat(dot(n, normalize(vec3(-0.5, 0.6, 0.65))));
            vec3 bc = mix(uSand, vec3(0.92), 0.35) * (0.75 + 0.3 * hash(id + 7.0)) * lit * (0.85 + 0.2 * fl * smoothstep(0.4, 1.0, rr));
            vec2 od = rot(hash(id + 5.0) * 3.14) * d;
            bc = mix(bc, bc * 0.6, fillA(abs(od.x) * 0.6 + abs(od.y) - 0.34, pw));
            bc = mix(bc, uDeep * 0.4, fillA(max(abs(od.y) - 0.045, abs(od.x) - 0.42), pw));
            float sh = fillA(length(d - vec2(0.18, -0.22)) - 1.15, pw * 3.0 + 0.25) * 0.4;
            acc = over(layerC(bc, body * band * k), over(acc, vec4(0.0, 0.0, 0.0, sh * band * k)));
        }
    return acc;
}
vec4 stageBoulder(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    float wave = uWave.y * bump(uWave.x - 2.2, 1.6);
    float level = 0.42 + 0.03 * uTide * uTideAmp + 0.22 * wave;
    float e = max(px * 1.5, 0.002);
    float h = boulderH(q);
    vec2 g = vec2(boulderH(q + vec2(e, 0.0)) - h, boulderH(q + vec2(0.0, e)) - h) / e;
    vec3 n = normalize(vec3(-g * 0.35, 1.0));
    vec3 L = normalize(vec3(-0.5, 0.6, 0.65));
    float dep = level - h;
    float aw = max(px * 1.5, 0.002) * (length(g) * 0.5 + 0.4);
    // the wet rock of the boulder: slate, pink coralline crust, a film of green weed
    float dif = sat(dot(n, L)) * 0.8 + 0.3;
    vec3 col = uRock * (0.7 + 0.45 * fbm(q * 5.0 + 2.0)) * (0.9 + 0.2 * vnoise(q * 60.0) * lod(1.0 / 60.0, px));
    float coral = smoothstep(0.52, 0.68, fbm(q * 2.4 - 7.0));
    col = mix(col, mix(uWarm, uSand, 0.55) * 0.8 * (0.85 + 0.3 * vnoise(q * 40.0)), coral * 0.7);
    col = mix(col, uWeed * 0.7, smoothstep(0.55, 0.75, fbm(q * 2.5 - 4.0)) * 0.45);
    col *= dif * 0.75;
    col += uLight * pow(sat(dot(n, normalize(L + vec3(0, 0, 1)))), 60.0) * 0.7;
    // barnacles: little fluted cones crowding the band above the water, a diamond of plates and a slit in each
    {
        vec4 bn = barnacles(q, px, level);
        col = col * (1.0 - bn.a) + bn.rgb;
    }
    // limpets
    {
        vec4 lc = limpet(q, vec2(-0.5, 0.33), 0.2, 0.4, px);
        lc = over(lc, limpet(q, vec2(0.25, -0.55), 0.14, 2.1, px));
        lc = over(lc, limpet(q, vec2(-0.3, -0.36), 0.1, -0.7, px));
        col = col * (1.0 - lc.a) + lc.rgb;
    }
    // under the drop: its shadow and its focused light
    col *= 1.0 - 0.45 * fillA(length(q - vec2(0.05, -0.06)) - 0.22, 0.1);
    col += uLight * 0.9 * exp(-dot(q - vec2(0.09, -0.1), q - vec2(0.09, -0.1)) / 0.004);
    if (dep > -aw) {
        // the water round the boulder: sand, caustics, anemones on its flanks, a crab
        vec3 bed = sandBed(q, px, t, 2.0);
        float d = max(dep, 0.0);
        float c = caustic(q * 1.6, t * 0.5, px * 1.6);
        vec3 lit = mix(bed, col * 1.1, smoothstep(0.08, 0.32, h)) * (0.5 + 1.0 * c * exp(-d * 1.5));
        vec4 an = anemone(q, vec2(-1.2, 0.55), 0.4, 1.0, px, t);
        an = over(an, anemone(q, vec2(-0.55, 1.2), 0.32, 2.0, px, t));
        an = over(an, anemone(q, vec2(-1.95, -0.45), 0.55, 3.0, px, t));
        an = over(an, anemone(q, vec2(-0.95, -1.15), 0.34, 4.0, px, t));
        an = over(an, anemone(q, vec2(0.7, 1.1), 0.28, 5.0, px, t));
        float cw = t * 0.05;
        vec2 cp = vec2(cos(-0.9 + 0.25 * sin(cw)), sin(-0.9 + 0.25 * sin(cw))) * 1.55 + vec2(-0.2, 0.0);
        vec4 cr = crab(q, cp, 0.24, -0.6 + 0.4 * cos(cw), t * 0.35, px);
        vec4 life = over(cr, an);
        vec3 wc = mix(waterOver(lit, d * 0.8 + 0.06), waterOver(life.rgb / max(life.a, 1e-3) * (0.8 + 0.5 * c), 0.1), life.a);
        float men = lineA(dep, aw * 0.5, aw);
        wc = mix(wc, mix(uLight, uShallow, 0.5), men * 0.4);
        // the drop's wave: a lace of foam on its edge
        float foam = wave * smoothstep(0.06, 0.0, dep) * smoothstep(0.4, 0.65, fbm(q * 9.0 + t));
        wc = mix(wc, vec3(0.92), foam * 0.6);
        col = mix(col, wc, smoothstep(-aw, aw, dep));
    }
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}
// ======================================================================= 2. inside the drop: plankton
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
        vec2 c = vec2(0.05 - 0.5 * sw, 0.92 * sd);
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
vec4 stageDrop(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB + 0.05) return vec4(0.0);
    float t = uTime;
    // the drop is a lens: the water inside compressed toward its rim
    float rr = r / RB;
    vec2 qc = q * (1.0 + 0.35 * rr * rr * rr);
    // luminous water, lit through the drop from the upper left
    vec2 ld = normalize(vec2(-0.6, 0.7));
    vec3 col = mix(uWater, uShallow, 0.55 + 0.35 * dot(q / RB, ld) - 0.25 * rr * rr);
    col += uLight * 0.12 * exp(-sq(length(q - ld * 1.6)) / 1.5);
    // murk: soft clouds of fine matter drifting, at a scale following the zoom
    col *= 0.9 + 0.14 * fbm(qc * 1.2 / max(0.3, r) + t * 0.05);
    vec4 life = vec4(0.0);
    // dust and tiny cells: a log-polar scatter, some in focus, some soft
    {
        float N = 44.0;
        vec2 lp = toLP(qc, N) + vec2(-t * 0.03, t * 0.12);
        vec2 ci = floor(lp);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 cell = ci + vec2(x, y);
                vec2 wc = vec2(cell.x, mod(cell.y, N));
                float hb = hash(wc + 3.0);
                if (hb < 0.5) continue;
                vec2 cc = fromLP(cell + hash2(wc) - vec2(-t * 0.03, t * 0.12), N);
                float rad = length(cc) * TAU / N * (0.08 + 0.1 * hash(wc + 7.0));
                float blur = rad * (hb > 0.8 ? 1.6 : 0.15);
                float dd = length(qc - cc) - rad;
                float a = fillA(dd, max(px, blur)) * (hb > 0.8 ? 0.25 : 0.6);
                life = over(life, layerC(mix(uLight, uSand, hash(wc + 1.0)) * 0.9, a));
            }
    }
    // the plankton, their places scaled on their distance from the focus
    float hopP = fract(uLife * 0.5);            // a hop every two beats
    float hop = exp(-hopP * 7.0) * (0.3 + 0.7 * uTideAmp);
    for (int i = 0; i < 9; i++) {
        float fi = float(i);
        float hs = hash1(fi * 3.7 + 1.0);
        float rad = 0.5 * pow(1.3, fi) * (0.9 + 0.2 * hs);          // log-spaced distances from the focus
        float a0 = fi * 2.39996 + 0.7 + t * (0.01 + 0.015 * hs) * (mod(fi, 2.0) * 2.0 - 1.0);
        vec2 c = vec2(cos(a0), sin(a0)) * rad;
        int kind = fi < 4.0 ? int(mod(fi, 4.0)) : int(mod(fi, 3.0));
        float size = rad * (0.3 + 0.1 * hs) * (kind == 3 ? 0.55 : 1.0);
        float heading = a0 + 1.9 + 0.3 * sin(t * 0.2 + fi);
        vec2 dir = vec2(cos(heading), sin(heading));
        c += dir * size * (0.25 * sin(t * 0.15 + fi) + 0.12 * hop);
        vec2 d = rot(-heading) * (qc - c) / size;
        float pw = px / size * 1.3;
        vec4 o;
        if (kind == 0 || kind == 2) o = copepod(d, pw, hop, fi);
        else if (kind == 1) o = nauplius(d * 1.3, pw * 1.3, t + fi);
        else o = chain(d * 0.9, pw * 0.9, t);
        life = over(life, o);
        // bioluminescent flashes at night: a dinoflagellate sparks on the kicks
        if (uLum > 0.0) col += uGlow * uLum * uKick * 0.6 * exp(-dot(qc - c, qc - c) / sq(size * 0.6)) * step(0.5, hash1(fi + floor(uLife)));
    }
    col = col * (1.0 - life.a) + life.rgb;
    // a halo of light around the diatom at the focus
    col += uLight * 0.25 * exp(-sq(r - 0.22) / 0.004) * (0.6 + 0.4 * sin(atan(q.y, q.x) * 3.0 + t * 0.2));
    // the drop's rim: refraction darkens toward it, a bright crescent of reflected light, a faint second one
    float edgeW = max(px * 1.2, 0.004);
    float rim = smoothstep(RB * 0.78, RB, r);
    col = mix(col, uDeep * 0.9, rim * rim * 0.85);
    float th = atan(q.y, q.x);
    float cres = lineA(r - RB * 0.93, RB * 0.025, edgeW) * smoothstep(0.4, 1.0, cos(th - 2.3));
    col += mix(uLight, vec3(1.0), 0.5) * cres * 0.9;
    col += uLight * 0.35 * lineA(r - RB * 0.97, RB * 0.01, edgeW) * smoothstep(0.3, 1.0, cos(th + 0.8));
    float a = fillA(r - RB, edgeW);
    return layerC(col, a);
}

// ======================================================================= 3. a diatom's glass: the rose window
vec4 stageDiatom(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB + 0.05) return vec4(0.0);
    float t = uTime;
    float th = atan(q.y, q.x);
    // behind the glass: the living cell, golden, softly moving
    vec3 cell = mix(uWarm2, uWeed, 0.3 + 0.35 * fbm(q * 1.3 / max(r, 0.25) + t * 0.03));
    cell = mix(uDeep, cell, 0.8);
    float N = 96.0;
    vec2 lp = toLP(q, N);
    float row = floor(lp.x);
    vec2 hp = vec2(lp.x, lp.y + 0.5 * mod(row, 2.0));          // alternate rows offset
    vec2 ci = floor(hp), f = fract(hp) - 0.5;
    float cellLen = r * TAU / N;                                // a pore's size (stage units)
    float pw = px / cellLen * 1.2;                              // a pixel in pore units
    float lodp = lod(cellLen, px * 1.3);
    // the glass: sectors alternately raised and sunken (light and shade), thin-film colors, a light turning slowly
    float sector = cos(th * 4.0 + 0.4);
    float sh = 0.78 + 0.22 * smoothstep(-0.25, 0.25, sector);
    float film = 0.5 + 0.5 * sin(log(r) * 3.0 + th * 2.0 + t * 0.12);
    vec3 irid = hsv(fract(film * 0.5 + 0.45 + uFever * t * 0.02), 0.4, 1.0);
    vec3 glass = mix(uLight, uShallow, 0.45) * mix(vec3(1.0), irid, 0.4);
    float sweep = 0.5 + 0.5 * cos(th - (t * 0.1 + 0.6 * uTide * uTideAmp));
    vec3 gcol = mix(uDeep * 0.8, glass, 0.22 + 0.2 * sweep) * sh;
    // a pore: round, a bevelled rim catching the light, the golden cell glowing deep inside
    float pd = length(f) - 0.33;
    float pore = fillA(pd, pw);
    float bev = 0.5 + 0.5 * dot(normalize(f + 1e-4), normalize(vec2(-0.6, 0.7) * rot(-th)));
    vec3 inPore = mix(uDeep * 0.7, cell * (0.6 + 0.6 * sweep), smoothstep(0.33, 0.0, length(f)));
    // a sieve of tiny holes in each pore, once it is big
    float sieveA = lod(cellLen / 7.0, px * 1.5);
    float sieve = fillA(length(fract(f * 7.0) - 0.5) - 0.2, pw * 7.0) * sieveA;
    inPore = mix(inPore, gcol * 0.5, sieve * 0.4 * (1.0 - smoothstep(0.25, 0.33, length(f))));
    vec3 rimC = glass * (0.35 + 0.45 * bev) * (0.5 + 0.6 * sweep);
    float rim = lineA(pd + 0.03, 0.035, pw) ;
    vec3 col = mix(gcol, inPore, pore * lodp + (1.0 - lodp) * 0.45);
    col = mix(col, rimC, rim * lodp * 0.8);
    // radial ribs every twelve pores, concentric costae every eight rows: fine threads of brighter glass
    float rib = lineA(fract((lp.y + 0.25) / 12.0) - 0.5, 0.02, pw / 12.0);
    float cost = lineA(fract(lp.x / 8.0) - 0.5, 0.03, pw / 8.0);
    col = mix(col, gcol * 1.15, max(rib, cost) * 0.55 * lod(cellLen * 4.0, px));
    // glints in the pores with the kicks
    col += uLight * uKick * step(0.985, hash(ci + vec2(0.0, floor(uLife * 0.5)))) * pore * lodp;
    // the central pore (the next scale inside it) in a ring of plain glass with fine radial striae
    float hy = smoothstep(0.212, 0.222, r) * (1.0 - smoothstep(0.3, 0.33, r));
    vec3 hyal = glass * (0.6 + 0.2 * sin(th * 60.0) * lod(r * TAU / 60.0, px)) * (0.6 + 0.5 * sweep);
    col = mix(col, hyal, hy);
    col = mix(col, uDeep * 0.5, lineA(r - 0.214, 0.004, max(px, 0.002)));
    // the girdle: a thick band of glass at the rim
    float gb = smoothstep(RB - 0.3, RB - 0.26, r);
    vec3 gir = glass * (0.55 + 0.2 * sin(r * 160.0) * lod(RB / 160.0 * 6.0, px)) * (0.6 + 0.5 * sweep);
    gir += glass * 0.4 * lineA(r - (RB - 0.04), 0.012, max(px, 0.003));
    col = mix(col, gir, gb);
    float a = fillA(r - RB, max(px * 1.2, 0.004));
    return layerC(col, a);
}

// ======================================================================= 4. inside the cell: cytoplasm streaming
vec4 stageCell(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    float stream = t * (0.04 + 0.03 * uTideAmp * (0.5 + 0.5 * uTide));
    vec2 sq0 = rot(stream * 0.6) * q;
    float sc = 1.0 / max(r, 0.25);
    // pale cytoplasm, clouded by vacuoles
    vec3 col = mix(mix(uShallow, uSand, 0.3), uWater, 0.4) * (0.8 + 0.15 * fbm(sq0 * 1.3 * sc));
    float vac = smoothstep(0.55, 0.7, fbm(sq0 * 0.9 * sc + 7.0));
    col = mix(col, mix(uShallow, uLight, 0.25), vac * 0.35);
    col = mix(col, mix(uShallow, uLight, 0.5), lineA(fbm(sq0 * 0.9 * sc + 7.0) - 0.55, 0.004, px * sc * 0.6 + 0.004) * 0.35);
    vec4 life = vec4(0.0);
    // chloroplasts: golden-brown plates with stacked thylakoids and a pale pyrenoid, streaming round
    {
        float N = 13.0;
        vec2 qq = rot(stream) * q;
        vec2 lp = toLP(qq, N);
        vec2 ci = floor(lp);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 cell = ci + vec2(x, y);
                vec2 wc = vec2(cell.x, mod(cell.y, N));
                if (hash(wc + 1.0) < 0.5) continue;
                vec2 cc = fromLP(cell + 0.1 + 0.8 * hash2(wc), N);
                float R = length(cc) * TAU / N * (0.22 + 0.2 * hash(wc + 8.0));
                vec2 d = rot(hash(wc + 4.0) * 6.28 + t * 0.05 * (hash(wc + 3.0) - 0.5)) * (qq - cc) / R;
                d.y *= 1.6 + 0.6 * hash(wc + 9.0);
                float lob = length(d) - 1.0 + 0.1 * sin(atan(d.y, d.x) * 3.0 + hash(wc) * 6.0);
                float pw = px / R * 1.5;
                float a = fillA(lob, pw + 0.05);
                if (a <= 0.0) continue;
                vec3 pc = mix(uWarm2, uWeed, 0.25 + 0.3 * hash(wc + 2.0)) * (0.6 + 0.4 * smoothstep(0.0, -0.7, lob));
                float lam = lineA(fract(d.y * 2.2) - 0.5, 0.1, pw * 2.2) * lod(R / 2.2, px);
                pc = mix(pc, pc * 0.7, lam * 0.5);
                pc = mix(pc, mix(uSand, uLight, 0.5), fillA(length(d) - 0.22, pw + 0.04) * 0.6);
                pc += uLight * 0.25 * lineA(lob + 0.05, 0.03, pw);
                life = over(layerC(pc, a * 0.85), life);
            }
    }
    // granules streaming along spiral paths, oil droplets
    {
        float N = 60.0;
        float sp = t * (0.5 + 0.5 * uTideAmp);
        vec2 lp = toLP(q, N);
        lp.y += sp - lp.x * 0.35;
        vec2 ci = floor(lp);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 cell = ci + vec2(x, y);
                vec2 wc = vec2(cell.x, mod(cell.y, N));
                float hb = hash(wc + 6.0);
                if (hb < 0.62) continue;
                vec2 c0 = cell + 0.5 + 0.3 * (hash2(wc) - 0.5);
                vec2 cc = fromLP(vec2(c0.x, c0.y - sp + c0.x * 0.35), N);
                float R = length(cc) * TAU / N * (hb > 0.94 ? 0.42 : 0.15);
                float dd = length(q - cc) - R;
                float a = fillA(dd, max(px, R * 0.15));
                vec3 gc = hb > 0.94 ? mix(uWarm2, uLight, 0.6) : mix(uLight, uSand, 0.4);
                gc *= 0.8 + 0.5 * sat(1.0 - length(q - cc + R * vec2(0.3, -0.3)) / R);
                life = over(life, layerC(gc, a * (hb > 0.94 ? 0.9 : 0.55)));
            }
    }
    col = col * (1.0 - life.a) + life.rgb;
    // the vesicle at the focus (the next scale inside it): a double membrane
    float mem = lineA(r - 0.226, 0.006, max(px, 0.002)) + lineA(r - 0.244, 0.005, max(px, 0.002)) * 0.7;
    col = mix(col, mix(uLight, uShallow, 0.3), mem * 0.7);
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}

// ======================================================================= 5. molecules jostling
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
vec4 stageMolecules(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    float jit = 0.6 + 0.9 * uTideAmp * (0.6 + 0.4 * uTide) + 0.5 * uKick;
    vec3 col = mix(uDeep, uWater, 0.3 + 0.12 * fbm(q * 0.8 / max(r, 0.3)));
    vec4 acc = vec4(0.0);
    // the double helix: a log spiral staircase of light winding into the focus
    {
        float lr = log(max(r, 1e-4));
        float th = atan(q.y, q.x);
        float b = 0.3;
        float ds = mod(lr - th * b + PI * b, TAU * b) - PI * b;  // ln r to the arm
        float hw = 0.13;
        float ph = lr * 11.0 - t * 0.5;
        float s1 = sin(ph), s2 = sin(ph + PI);
        float lw = max(px, r * 0.006);
        float d1 = abs(ds - hw * s1) * r, d2 = abs(ds - hw * s2) * r;
        float f1 = 0.5 + 0.5 * cos(ph), f2 = 1.0 - f1;
        vec3 c1 = mix(uWarm, uLight, 0.45), c2 = mix(uCool, uLight, 0.45);
        // base pairs: rungs across, twelve a turn
        float rp = fract(ph * 6.0 / TAU);
        float rw = 6.0 / TAU * 11.0 * lw / r;
        float rung = lineA(rp - 0.5, 0.05, rw) * step(abs(ds), hw * abs(s1) * 0.95);
        acc = over(acc, layerC(mix(c1, c2, 0.5) * 0.8, rung * 0.4 * lod(r * 0.03, px)));
        acc = over(layerC(c1 * (0.5 + 0.6 * f1), lineA(d1, r * 0.008, lw) * (0.35 + 0.65 * f1)), acc);
        acc = over(layerC(c2 * (0.5 + 0.6 * f2), lineA(d2, r * 0.008, lw) * (0.35 + 0.65 * f2)), acc);
        acc.rgb += uGlow * 0.08 * exp(-sq(ds / (hw * 1.5))) * (1.0 - acc.a);
    }
    // water molecules: an oxygen orb and two hydrogens, bonds as threads of light, Brownian jitter
    {
        float N = 22.0;
        vec2 lp = toLP(q, N);
        vec2 ci = floor(lp);
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 cell = ci + vec2(x, y);
                vec2 wc = vec2(cell.x, mod(cell.y, N));
                if (hash(wc + 9.0) < 0.45) continue;
                vec2 h2 = hash2(wc);
                float hs = h2.x * 40.0 + h2.y * 13.0;
                vec2 jt = vec2(vnoise(vec2(t * 3.1 + hs, 1.0)), vnoise(vec2(1.0, t * 2.9 + hs))) - 0.5;
                vec2 cc = fromLP(cell + 0.25 + 0.5 * h2 + jt * 0.3 * jit, N);
                float R = length(cc) * TAU / N * 0.17;
                if (length(cc) < 0.45) continue;
                float ang = hash(wc + 2.0) * 6.28 + (vnoise(vec2(t * 2.0 + hs, 5.0)) - 0.5) * 2.0 * jit;
                vec2 e1 = vec2(cos(ang), sin(ang)), e2 = vec2(cos(ang + 1.82), sin(ang + 1.82));
                vec2 h1p = cc + e1 * R * 2.1, h2p = cc + e2 * R * 2.1;
                float bw = max(px, R * 0.04);
                float bond = max(lineA(sdSeg(q, cc, h1p), R * 0.03, bw), lineA(sdSeg(q, cc, h2p), R * 0.03, bw));
                vec4 m = layerC(mix(uLight, uGlow, 0.3), bond * 0.75);
                m = over(orb(q, cc, R, mix(uWarm, uSand, 0.25), px), m);
                m = over(orb(q, h1p, R * 0.6, mix(uCool, uLight, 0.3), px), m);
                m = over(orb(q, h2p, R * 0.6, mix(uCool, uLight, 0.3), px), m);
                acc = over(m, acc);
            }
    }
    // the molecule at the focus: two hydrogens bonded to the atom we dive into
    {
        float ang = 0.6 + 0.15 * sin(t * 0.7);
        vec2 e1 = vec2(cos(ang), sin(ang)), e2 = vec2(cos(ang + 1.82), sin(ang + 1.82));
        float bw = max(px, 0.003);
        float bond = max(lineA(sdSeg(q, e1 * 0.2, e1 * 0.36), 0.004, bw), lineA(sdSeg(q, e2 * 0.2, e2 * 0.36), 0.004, bw));
        vec4 m = layerC(mix(uLight, uGlow, 0.3), bond * 0.8);
        m = over(orb(q, e1 * 0.42, 0.075, mix(uCool, uLight, 0.3), px), m);
        m = over(orb(q, e2 * 0.42, 0.075, mix(uCool, uLight, 0.3), px), m);
        acc = over(m, acc);
        acc.rgb += uWarm * 0.35 * exp(-sq(r - 0.21) / 0.0015) * (1.0 - acc.a);
    }
    col = col * (1.0 - acc.a) + acc.rgb;
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}

// ======================================================================= 6. an atom: breathing orbital clouds
vec4 stageAtom(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    float th = atan(q.y, q.x);
    float breathe = 1.0 + 0.035 * uTide * uTideAmp;
    float lr = log(max(r, 1e-4) / breathe);
    // nested shells, each with more lobes, a slow quantum beat between two states, the phase in color
    float psi = 0.0, dens = 0.0;
    for (int s = 0; s < 4; s++) {
        float fs = float(s);
        float rc = log(0.36) + fs * 0.62;
        float rad = exp(-sq((lr - rc) / 0.3));
        float m = 1.0 + fs;
        float spin = t * (0.04 + 0.015 * fs) * (mod(fs, 2.0) * 2.0 - 1.0);
        float beat = 0.5 + 0.5 * sin(t * (0.17 + 0.04 * fs) + fs * 1.3);
        float y = mix(cos(m * (th - spin)), cos((m + 1.0) * (th + spin * 0.7) + fs), beat * 0.5);
        float amp = rad * y;
        psi += amp;
        dens += amp * amp;
    }
    dens = dens * (0.8 + 0.2 * uPeak);
    vec3 warm = mix(uWarm, uLight, 0.3), cool = mix(uCool, uLight, 0.3);
    float ph = smoothstep(-0.04, 0.04, psi);
    vec3 phase = mix(cool, warm, ph);
    vec3 col = mix(uDeep, uWater * 0.8, 0.45);
    col += phase * dens * 0.5;
    // contour lines of light (the density's level sets), drawn against the pixel's size
    float lv = sqrt(dens) * 7.0;
    float w = fwidth(lv) + px * 7.0 / max(r, 0.05) * 0.0;
    float iso = lineA(fract(lv) - 0.5, 0.05, w * 1.2 + 0.01) * smoothstep(0.3, 1.2, lv) * (1.0 - smoothstep(6.0, 7.0, lv));
    col += phase * iso * 0.75;
    // bonds: threads of light to the neighbors, out of the frame
    for (int i = 0; i < 3; i++) {
        float ba = 0.8 + float(i) * 2.1 + 0.05 * sin(t * 0.3 + float(i));
        vec2 bd = vec2(cos(ba), sin(ba));
        float along = dot(q, bd);
        float dd = abs(dot(q, vec2(-bd.y, bd.x)));
        float th2 = lineA(dd, r * 0.003, max(px, r * 0.003)) * smoothstep(0.6, 1.6, along);
        col += mix(uLight, uGlow, 0.5) * th2 * 0.6 * (0.6 + 0.4 * sin(along * 6.0 - t * 2.0));
    }
    // the nucleus at the focus: a tiny bright knot
    col += mix(uLight, uWarm, 0.3) * exp(-sq(r - 0.215) / 0.0008) * 0.7;
    col += uGlow * 0.25 * exp(-r * r / 0.03);
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}

// ======================================================================= 7. inside the nucleus: quantum foam
// foam cells on the log-polar lattice (periodic round the focus): (distance to the wall, cell id)
vec3 foamCells(vec2 lp, float n, float t, out vec2 cid, out vec2 cpos) {
    vec2 i = floor(lp), f = fract(lp);
    float d1 = 8.0, d2 = 8.0;
    vec2 o1 = vec2(0.0), o2 = vec2(0.0);
    cid = i; cpos = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 id = vec2(i.x + g.x, mod(i.y + g.y, n));
            vec2 h = hash2(id);
            vec2 o = g + 0.5 + 0.38 * sin(t * (0.3 + 0.4 * h) + 6.2831 * h);
            vec2 dv = o - f;
            float d = dot(dv, dv);
            if (d < d1) { d2 = d1; o2 = o1; d1 = d; o1 = dv; cid = id; cpos = i + o; }
            else if (d < d2) { d2 = d; o2 = dv; }
        }
    // the distance to the wall between the two nearest cells (their bisector)
    float wall = dot(0.5 * (o1 + o2), normalize(o2 - o1));
    return vec3(abs(wall), sqrt(d1), 0.0);
}
vec4 stageFoam(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    vec3 col = mix(uDeep, uWater, 0.34);
    float N = 26.0;
    vec2 lp = toLP(q, N);
    float cellLen = r * TAU / N;
    float pw = px / cellLen;
    float boil = 0.1 + 0.1 * uTideAmp * (0.5 + 0.5 * uTide) + 0.06 * uPeak;
    vec2 cid, cpos;
    vec3 v = foamCells(lp, N, t * (0.5 + boil * 3.0), cid, cpos);
    // each cell's life: it swells, its film thins to colors, it bursts and its walls let go
    float life = fract(t * boil * (0.5 + hash(cid)) + hash(cid + 3.0));
    float burst = smoothstep(0.86, 0.9, life) * (1.0 - smoothstep(0.97, 1.0, life));
    float lw = 0.025 + 0.02 * (1.0 - life);
    float wall = lineA(v.x, lw, pw * 1.2) * (1.0 - burst);
    float film = fract(hash(cid) * 3.0 + v.y * 0.8 + life * 0.6);
    vec3 fc = hsv(fract(film * 0.7 + 0.45 + uFever * t * 0.03), 0.45, 1.0) * mix(uLight, vec3(1.0), 0.2);
    float k = lod(cellLen * 0.3, px);
    col += fc * wall * 0.75 * k + fc * (1.0 - k) * 0.12;
    col += mix(uCool, uShallow, 0.4) * 0.08 * smoothstep(0.6, 0.0, v.y) * (1.0 - burst) * life;
    // its ripple once it bursts, interfering with its neighbours' like rain on a pool
    {
        float age = (life - 0.86) / 0.14;
        if (age > 0.0) {
            float rr = v.y;
            float ring = cos((rr - age * 1.4) * 18.0) * exp(-sq((rr - age * 1.4) / 0.3)) * (1.0 - age);
            col += mix(uCool, uLight, 0.5) * ring * 0.18 * k;
            col += uLight * 0.5 * exp(-sq(rr / 0.08)) * (1.0 - smoothstep(0.0, 0.3, age)) * k;
        }
    }
    // virtual pairs: two sparks born together, spiraling apart, annihilating (with the kicks)
    {
        float M = 10.0;
        vec2 lp2 = toLP(q, M);
        vec2 c2 = floor(lp2);
        vec2 wc = vec2(c2.x, mod(c2.y, M));
        float ph = fract(t * 0.3 + hash(wc + 3.0));
        vec2 cc = fromLP(c2 + 0.5, M);
        float R = length(cc) * TAU / M;
        float sp = sin(ph * PI) * R * 0.3;
        float a0 = ph * 5.0 + hash(wc) * 6.28;
        vec2 o = vec2(cos(a0), sin(a0)) * sp;
        float sz = R * 0.025 + px;
        float g = exp(-dot(q - cc - o, q - cc - o) / sq(sz)) + exp(-dot(q - cc + o, q - cc + o) / sq(sz));
        col += mix(uWarm, uLight, 0.5) * g * (0.25 + 0.9 * uKick) * sin(ph * PI);
    }
    // the strings at the focus glow through
    col += uGlow * 0.3 * exp(-sq(r - 0.22) / 0.002);
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}

// ======================================================================= 8. strings, vibrating to the music
vec4 stageStrings(vec2 q, float px, float lu) {
    float r = length(q);
    if (r > RB) return vec4(0.0);
    float t = uTime;
    float th = atan(q.y, q.x);
    float lr = log(max(r, 1e-4));
    // the fabric: log-spaced rings drifting out from the focus, like ripples on a pool
    vec3 col = mix(uDeep, uWater, 0.3);
    float rings = cos(lr * 9.0 - t * 0.5);
    col += mix(uCool, uLight, 0.3) * 0.045 * (0.5 + 0.5 * rings) * lod(r * 0.6, px * 3.0);
    // closed strings: loops round the focus, each ringing in its own harmonics, fed by the song's bands
    for (int k = 0; k < 8; k++) {
        float fk = float(k);
        float R0 = 0.3 * pow(1.45, fk);
        float rr = R0;
        float dir = mod(fk, 2.0) * 2.0 - 1.0;
        float drr = 0.0;
        for (int n = 2; n < 8; n++) {
            float fn = float(n);
            float band = n < 3 ? uBands.x : n < 4 ? uBands.y : n < 6 ? uBands.z : uBands.w;
            float a = (0.01 + 0.045 * band * (0.4 + uTideAmp) + 0.02 * uKick) / fn * (0.6 + 0.4 * hash1(fk * 7.0 + fn));
            float arg = fn * th - t * (0.35 + 0.12 * fn) * dir + hash1(fk + fn) * 6.28;
            rr += R0 * a * cos(arg);
            drr -= R0 * a * fn * sin(arg);
        }
        // the distance to the curve r = rr(th): divided by its slope
        float d = abs(r - rr) / sqrt(1.0 + sq(drr / max(r, 1e-3)));
        float lw = max(px, R0 * 0.003);
        float core = lineA(d, R0 * 0.0035, lw);
        float glow = exp(-d / (R0 * 0.03)) * 0.16 + exp(-d / (R0 * 0.004 + px)) * 0.1;
        vec3 sc = hsv(fract(fk * 0.11 + 0.55 + uFever * t * 0.02), 0.3, 1.0);
        sc = mix(mix(uLight, mix(uGlow, uWarm, mod(fk, 2.0)), 0.45), sc, 0.25);
        col += sc * (core * 0.9 + glow);
    }
    // the pool, far inside the smallest loop
    col += uShallow * 0.2 * exp(-sq(r - 0.22) / 0.002);
    float a = 1.0 - smoothstep(2.0, RB, r);
    return layerC(col, a);
}
vec4 stage(int s, vec2 q, float px, float lu, bool inner) {
    if (s == 0) return stagePool(q, px, lu, inner);
    if (s == 1) return stageBoulder(q, px, lu);
    if (s == 2) return stageDrop(q, px, lu);
    if (s == 3) return stageDiatom(q, px, lu);
    if (s == 4) return stageCell(q, px, lu);
    if (s == 5) return stageMolecules(q, px, lu);
    if (s == 6) return stageAtom(q, px, lu);
    if (s == 7) return stageFoam(q, px, lu);
    return stageStrings(q, px, lu);
}

void main() {
    vec2 b = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    vec2 p = (b - uFocus) / uViewH.y;
    // the drop's wave: a slow ripple through the fabric, out from the focus
    if (uWave.y > 0.0) {
        float pr = length(p);
        float front = uWave.x * 0.9;
        float k = exp(-sq((pr - front) / 0.35)) * uWave.y * exp(-uWave.x * 0.35);
        p += p / max(pr, 1e-3) * sin((pr - front) * 9.0) * 0.025 * k;
    }
    // depth of field, as through a macro lens: sharp at the focus, softer toward the edges and behind the well
    float pr = length(p);
    vec2 qw = abs(b) - vec2(5.0, 10.0);
    float hud = 1.0 - smoothstep(-1.0, 2.5, length(max(qw, 0.0)) + min(max(qw.x, qw.y), 0.0));
    float blurP = 0.016 * smoothstep(0.45, 2.6, pr) + 0.01 * hud;
    // innermost first: it is in front; stop once opaque
    vec4 acc = vec4(0.0);
    for (int i = 3; i >= 0; i--) {
        if (i >= uLayers) continue;
        vec4 L = uLayer[i];
        vec2 q = rot(L.w) * p * L.z;
        float px = (uPxP + blurP) * L.z;
        if (i > 0 && length(q) > RB + 0.1) continue;
        vec4 c = stage(int(L.x + 0.5), q, px, L.y, i > 0);
        acc = over(acc, c);
        if (acc.a > 0.999) break;
    }
    vec3 c = acc.rgb;
    // FEVER: the world's hues turn slowly with the depth
    if (uFever > 0.0) {
        float a = uFever * (uDepth * 0.35 + uTime * 0.03);
        vec3 k = vec3(0.57735);
        float ca = cos(a), sa = sin(a);
        c = c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca);
        c = max(c, 0.0);
    }
    // slow motion: a hush, silver and cool
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(l) * vec3(0.85, 0.95, 1.12), uSlow * 0.55);
    c = mix(uBridgeCol, c, uWorldA);
    frag = vec4(c, 1.0);
}
)";

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

// ------------------------------------------------------------------------------------------------ bubbles, the gauge
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
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 1) {
        // the gauge: a fine ring, the charge an arc of light from the top, clockwise
        float ring = 1.0 - smoothstep(px * 0.6, px * 1.6, abs(r - 0.8));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = (1.0 - smoothstep(px * 1.2, px * 2.6, abs(r - 0.8))) * on;
        float halo = exp(-sq((r - 0.8) / 0.1)) * on * 0.3 * (0.5 + 0.5 * vC.y);
        float v = ring * 0.3 + arc + halo;
        frag = vec4(c * v * vB.a, v * vB.a * 0.5);
        return;
    }
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
    float halo = smoothstep(0.18, 0.45, d) * uHaloA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill);
    frag = vec4(c, a) * uAlpha;
}
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

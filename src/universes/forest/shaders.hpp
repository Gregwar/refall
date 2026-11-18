#pragma once
// GLOWING FOREST's shaders (see forest.cpp): a relief print.
//
// The world is not painted but printed: every pass up to the frame writes plates, not colors. A plate texel holds how
// much of the key block is carved away there (r: 0 solid black ink .. 1 bare paper) and the density of the two glow
// inks rolled under it (g, b): an ink shows only where the key is carved. HALO_FS first gathers the light the plants
// give (a quarter-resolution field); BG_FS carves the key block, every cut as wide as the light there (the night sky's
// horizontal cuts thickening toward the moon and around each plant, the moon cut out of it, stars, the far glade, three depths of
// trunks cut lighter the farther they stand, the canopy and its fan-cut leaves, the near trunks' long bark strokes, the
// floor's stipples and grass tufts, the stones and the rock) and rolls the glow inks low over the floor and around the
// plants; PLANT_FS cuts the life itself (each plant a small SDF, a margin of uncut key around it, its body shaded by
// cuts and inked); MOTE_FS cuts fireflies, spores and chips. FINAL_FS pulls the print: the paper, the glow inks a
// little out of register (the drop's third ink in the second's place), the black key over them (its carved edges
// pressed into the paper, its ink starved here and there), a fluorescent glow (DOWN_FS / BLUR_FS), the pool's
// reflection, and on the drop the forest's neon line art (from the second texture BG_FS, PLANT_FS and MOTE_FS write).
// PIECE_FS then draws the blocks in register over it, as neon tubes.
#include <string>

namespace forest_shaders {

inline const char* COMMON = R"(
uniform float uGS;     // the ground's phase (the forest's seed)
uniform vec3 uRock;    // the rock (the bottom-left corner): center, half width, height
uniform vec4 uPool;    // the pool on the right: x0, x1, its water's level (none when x1 <= x0)
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm3(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.875;
}
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
float segT(vec2 p, vec2 a, vec2 b, out float h) { vec2 pa = p - a, ba = b - a; h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// Gouge cuts: long parallel cuts `sp` apart across uv.y, broken every `len` or so along uv.x where the gouge lifts out;
// the tone t (0 solid ink .. 1 bare paper) is the carving: a cut swells where the light is, thins to a point and stops
// where it fades, and the cuts run together where it is brightest. Returns how much is carved away at uv.
float gouge(vec2 uv, float sp, float len, float t, float seed, float aa) {
    if (t <= 0.05) return 0.0;
    float rv = uv.y / sp;
    float row = floor(rv);
    float hr = hash12(vec2(row, seed));
    float cu = uv.x / len + hr * 7.0;
    float ci = floor(cu), fu = fract(cu);
    float h = hash12(vec2(ci + seed * 1.7, row));
    // the gouge lifts out between cuts: a short gap, closing in the brightest light
    float gap = (0.05 + 0.15 * h) * (1.0 - smoothstep(0.55, 0.95, t));
    float s = fu / (1.0 - gap);
    if (s >= 1.0) return 0.0;
    float taper = pow(max(sin(3.14159 * s), 0.0), 0.3);
    // a hand cut: the line wanders a little across its row
    float off = 0.08 * sin(uv.x / len * 3.0 + h * 6.3 + row * 1.7);
    float hw = 0.56 * clamp((t - 0.05) * (0.85 + 0.3 * h), 0.0, 1.0) * taper;
    float d = (abs(fract(rv) - 0.5 - off) - hw) * sp;
    return 1.0 - smoothstep(-aa, aa, d);
}
// U-gouge dabs: short rounded nicks scattered on a grid `sp` apart, more and larger as the tone t rises.
float stipple(vec2 p, float sp, float t, float seed, float aa) {
    if (t <= 0.004) return 0.0;
    vec2 g = p / sp;
    vec2 id = floor(g), f = fract(g) - 0.5;
    float h = hash12(id + seed);
    if (h > 0.25 + 1.5 * t) return 0.0;
    vec2 o = (vec2(hash12(id + 1.7), hash12(id + 4.3)) - 0.5) * 0.4;
    float an = h * 9.0;
    vec2 q = f - o;
    q = vec2(cos(an) * q.x - sin(an) * q.y, sin(an) * q.x + cos(an) * q.y);
    float k = 0.55 + 0.6 * t;
    float d = (length(q / vec2(0.3 * k, 0.12 * k)) - 1.0) * 0.12 * k * sp;
    return 1.0 - smoothstep(-aa, aa, d);
}
// a blade cut from a to b: narrow where the gouge goes in, widest a third along, lifting out to a point
float blade(vec2 p, vec2 a, vec2 b, float w) {
    float h;
    float d = segT(p, a, b, h);
    return d - w * pow(max(sin(3.14159 * h), 0.0), 0.6) * (1.0 - 0.35 * h);
}
uniform vec4 uBox[24];  // the HUD's lines (board units: x0, y0, x1, y1)
uniform int uBoxes;
// The HUD's text: 1 behind a line, falling off around it.
float textQuietAt(vec2 w) {
    float q = 0.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uBoxes) break;
        vec4 b = uBox[i];
        q = max(q, 1.0 - smoothstep(0.1, 1.0, sdBox(w - (b.xy + b.zw) * 0.5, (b.zw - b.xy) * 0.5)));
    }
    return q;
}
// The HUD's columns above the floor: 1 inside, falling off over half a unit.
float columnsAt(vec2 w) {
    return smoothstep(5.6, 6.2, abs(w.x)) * (1.0 - smoothstep(11.3, 11.9, abs(w.x)))
         * smoothstep(w.x < 0.0 ? -7.6 : -4.4, w.x < 0.0 ? -6.6 : -3.4, w.y);
}
// The well, the HUD's columns and its text: the key stays solid there (1), the cuts thinning out toward them.
float quietAt(vec2 w) {
    float q = 1.0 - smoothstep(0.0, 0.8, sdBox(w - vec2(0.0, 0.6), vec2(5.3, 11.0)));
    q = max(q, 1.0 - smoothstep(0.0, 1.6, sdBox(w - vec2(-8.6, 2.0), vec2(2.6, 8.6))));
    q = max(q, 1.0 - smoothstep(0.0, 1.6, sdBox(w - vec2(8.6, 3.8), vec2(2.6, 6.8))));
    return max(q, textQuietAt(w));
}
// The floor's height at x (mirrors groundY in forest.cpp): a gently rolling floor, the rock rising on the left.
float baseY(float x) {
    float ax = abs(x);
    float k = smoothstep(4.5, 8.0, ax);
    return -10.55 + k * (0.22 * sin(0.37 * x + uGS) + 0.12 * sin(1.13 * x + 2.0 * uGS)) + 0.35 * smoothstep(14.0, 21.0, x);
}
float rockTop(float x) {
    float bx = (x - uRock.x) / uRock.y;
    float k = max(1.0 - bx * bx, 0.0);
    return -11.3 + (uRock.z + 0.18 * sin(x * 0.9 + uGS)) * pow(k, 0.55);
}
float poolK(float x) {
    return uPool.y > uPool.x ? smoothstep(uPool.x - 0.7, uPool.x + 0.5, x) * smoothstep(uPool.y + 0.7, uPool.y - 0.5, x) : 0.0;
}
float groundY(float x) {
    float a = baseY(x), b = rockTop(x);
    float h = max(0.3 - abs(a - b), 0.0) / 0.3;
    return mix(max(a, b) + h * h * 0.3 * 0.25, -11.9, poolK(x));
}
// The drop's neon at x: a gradient on each side, from the HUD's column out to the edge (three inks a side).
uniform vec3 uNeonL[3], uNeonR[3], uNeonWarm, uNeonMoon;
vec3 neonAt(float x, float halfW) {
    float t = clamp((abs(x) - 11.0) / max(halfW - 11.0, 1.0), 0.0, 1.0);
    return x < 0.0 ? (t < 0.5 ? mix(uNeonL[0], uNeonL[1], t * 2.0) : mix(uNeonL[1], uNeonL[2], t * 2.0 - 1.0))
                   : (t < 0.5 ? mix(uNeonR[0], uNeonR[1], t * 2.0) : mix(uNeonR[1], uNeonR[2], t * 2.0 - 1.0));
}
// how far the drop's neon scene has come in at w: a wave out from the well, its front rollR from it
float neonIn(vec2 w, float rollR) { return smoothstep(max(abs(w.x) - 5.0, 0.0), max(abs(w.x) - 5.0, 0.0) + 2.5, rollR); }
// how far the drop's third ink has rolled out from the well (rollR: its front's distance), 1 behind its front
float dropRoll(vec2 w, float rollR) {
    return smoothstep(rollR, rollR - 3.0, max(abs(w.x) - 4.0, 0.0) + 0.1 * max(w.y + 10.0, 0.0));
}
// The drop's light: uDropK how far the drop's neon is in; each kick sends a wave of light out from the well's foot,
// outward and up the trees (uWaveAge its age in seconds, uWaveAmp its strength, the four latest).
uniform float uDropK;
uniform vec4 uWaveAge, uWaveAmp;
float dropWave(vec2 p) {
    float d = max(abs(p.x) - 5.5, 0.0) * 0.8 + max(p.y + 11.0, 0.0) * 0.7;
    vec4 r = uWaveAge * 10.0, e = d - r;
    vec4 k = uWaveAmp * exp(-e * e / 5.0) * exp(-uWaveAge * 0.8);
    return k.x + k.y + k.z + k.w;
}
// how much of the pool's water is at w (0 none)
float waterAt(vec2 w) {
    if (poolK(w.x) <= 0.0) return 0.0;
    return smoothstep(0.0, 0.45, uPool.z - groundY(w.x)) * (1.0 - smoothstep(-0.012, 0.012, w.y - uPool.z));
}
)";

// ---------------------------------------------------------------- the key block and the glow inks (full screen)
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uMoon;    // the moon cut out of the sky: center, radius
uniform vec4 uGlade;   // a far glade glowing between the trunks on the left: center, radii
uniform float uTime, uSeed, uMistK, uLife, uWaveR, uWaveK, uAA, uWind, uMoonK;
uniform float uShaftK;  // the moonlight's shafts through the trees
uniform sampler2D uLight;  // the plants' light and inks (HALO_FS)
// the drop's wave of light where it passes
float waveAt(vec2 w) {
    float dist = max(abs(w.x) - 5.5, 0.0) + 0.25 * max(w.y + 10.0, 0.0);
    return uWaveK * exp(-pow(dist - uWaveR, 2.0) / 3.0);
}
// A depth of trunks (L 0 the farthest): coverage, across the trunk (-1..1), its center x. None crosses the moon.
vec3 layerTrunk(vec2 w, float L, float sp, float wid0, float soft) {
    float ox = uSeed * 13.0 + L * 7.31;
    float id = floor((w.x + ox) / sp);
    if (hash12(vec2(id, L * 17.0 + 3.0)) < 0.22) return vec3(0.0);
    float x0 = (id + 0.5 + (hash12(vec2(id, L + 5.0)) - 0.5) * 0.4) * sp - ox;
    if (abs(x0 - uMoon.x) < uMoon.z * 1.3 + wid0 * 2.0) return vec3(0.0);
    float slope = (hash12(vec2(id, L + 9.0)) - 0.5) * 0.035;
    float y = w.y + 11.0;
    float xc = x0 + slope * y + 0.06 * L * sin(y * 0.19 + id * 1.7);
    float wid = wid0 * (0.65 + 0.7 * hash12(vec2(id, L + 11.0))) * (1.0 - 0.011 * y);
    float foot = groundY(w.x) + 1.1 - 0.35 * L;
    wid *= 1.0 + 0.8 * exp(-max(w.y - foot, 0.0) / 0.9);
    float dx = w.x - xc;
    float d = abs(dx) - wid + (vnoise(vec2(w.x * 2.3, w.y * 0.45) + id * 3.1) - 0.5) * 0.2 * wid0;
    if (L > 0.5) {
        // a limb reaching up and out
        float hb = 4.0 + 10.0 * hash12(vec2(id, L + 13.0));
        float s = hash12(vec2(id, L + 15.0)) < 0.5 ? -1.0 : 1.0;
        vec2 a = vec2(x0 + slope * hb + 0.06 * L * sin(hb * 0.19 + id * 1.7), hb - 11.0);
        vec2 b = a + vec2(s * 0.25 * sp, 1.4 + 0.8 * hash12(vec2(id, L + 17.0)));
        float h;
        float dl = segT(w, a, b, h) - wid0 * 0.45 * (1.0 - 0.75 * h);
        d = min(d, dl);
    }
    float cov = 1.0 - smoothstep(-soft, soft, d);
    return vec3(cov, clamp(dx / max(wid, 1e-3), -1.0, 1.0), xc);
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float aa = uAA * 0.75;
    float top = uView.y + uView.w;
    float gy = groundY(w.x);
    float open = 1.0 - quietAt(w);
    // ---- the glow inks: rolled low over the floor where the life is, the two inks in long soft bands
    float above = max(w.y - gy, 0.0);
    float roll = smoothstep(0.35, 0.65, fbm3(vec2(w.x * 0.07 + uSeed, 1.3)));
    float inkK = exp(-above / 6.0);
    vec4 lt = texture(uLight, vUv);
    float inkA = clamp(inkK * (1.0 - roll) + lt.g, 0.0, 1.0), inkB = clamp(inkK * roll + lt.b, 0.0, 1.0);
    // the light low in the forest, in long banks: the life's glow in the air, which
    float bank = 0.45 + 0.8 * fbm3(vec2(w.x * 0.08 - uTime * 0.03, w.y * 0.22 + 0.15 * sin(uTime * 0.05)) + uSeed);
    // rises with the song (and blazes under the drop's blacklight); the drop's wave lighting it as it passes; and the
    // light of each plant around it
    float plantL = lt.r;
    float mistL = uMistK * exp(-above / 3.2) * bank * (0.5 + 0.5 * uLife) + 0.35 * waveAt(w) * exp(-above / 3.0) + plantL;
    // the glade far away on the left: a bank of the life's light seen between the trunks
    vec2 gq = (w - uGlade.xy) / uGlade.zw;
    float glade = exp(-dot(gq, gq) * 1.7) * (0.6 + 0.5 * bank);
    mistL += uMistK * 1.5 * glade * (0.6 + 0.4 * uLife);
    // moonlight falling in a few slow shafts from the moon down through the trees, shifting as the canopy sways
    {
        vec2 sd = w - uMoon.xy;
        float sl = length(sd);
        float sa = atan(sd.x, -sd.y);
        float beam = vnoise(vec2(sa * 7.0 + 0.35 * sin(uTime * 0.07 + uSeed), uTime * 0.025 + uSeed * 3.0));
        float shaft = smoothstep(0.58, 0.85, beam) * exp(-sl / 16.0) * smoothstep(uMoon.z * 1.4, uMoon.z * 3.0, sl)
                    * step(w.y, uMoon.y) * (0.75 + 0.25 * sin(uTime * 0.3 + sa * 5.0));
        mistL += uShaftK * 1.1 * shaft;
    }
    inkA = max(inkA, smoothstep(0.0, 0.5, glade) * (1.0 - roll));
    inkB = max(inkB, smoothstep(0.0, 0.5, glade) * roll);
    // ---- the sky: solid night, cut in long horizontal strokes where the moon lights it and low in the forest
    vec2 mq = w - uMoon.xy;
    float mr = length(mq);
    float moonL = uMoonK * exp(-max(mr - uMoon.z, 0.0) / (uMoon.z * 1.2));
    float tSky = (0.8 * moonL * smoothstep(uMoon.z * 1.12, uMoon.z * 1.3, mr) + 0.62 * (1.0 - exp(-mistL * 1.6))) * open;
    float carve = gouge(vec2(w.x, w.y + 0.05 * sin(w.x * 0.35 + uSeed)), 0.2, 7.0, tSky, 1.0, aa);
    // stars where the sky is dark: a few nicks of the gouge, a cross for the bright ones
    {
        vec2 g = w / 1.7 + uSeed;
        vec2 id = floor(g);
        float h = hash12(id * 1.37 + 5.0);
        if (h > 0.62) {
            vec2 sc = (id + 0.25 + 0.5 * vec2(hash12(id + 2.1), hash12(id + 6.3)) - uSeed) * 1.7;
            float r = 0.035 + 0.06 * pow(hash12(id + 9.4), 3.0);
            float ds = length(w - sc) - r;
            if (h > 0.9) ds = min(ds, min(blade(w, sc - vec2(r * 3.5, 0.0), sc + vec2(r * 3.5, 0.0), r * 0.4),
                                          blade(w, sc - vec2(0.0, r * 3.5), sc + vec2(0.0, r * 3.5), r * 0.4)));
            float k = smoothstep(-2.0, 3.0, w.y) * (1.0 - smoothstep(0.05, 0.25, tSky)) * open;
            carve = max(carve, (1.0 - smoothstep(-aa, aa, ds)) * step(0.5, k));
        }
    }
    // the moon: bare paper in a ring of solid ink, two thin strokes of cloud left across it
    float moonCov = (1.0 - smoothstep(-aa, aa, mr - uMoon.z)) * open;
    if (moonCov > 0.0) {
        float cl = 0.0;
        for (int k = 0; k < 2; k++) {
            float fk = float(k);
            float yy = uMoon.y + uMoon.z * (-0.35 + 0.5 * fk + 0.1 * (hash12(vec2(fk, uSeed)) - 0.5));
            float x0 = uMoon.x + uMoon.z * (fk - 1.1 + 0.3 * hash12(vec2(fk + 3.0, uSeed)));
            vec2 a = vec2(x0, yy), b = a + vec2(uMoon.z * (1.1 + 0.4 * hash12(vec2(fk, 9.0))), 0.04 * uMoon.z);
            cl = max(cl, 1.0 - smoothstep(-aa, aa, blade(w, a, b, 0.035 * uMoon.z)));
        }
        carve = mix(carve, 1.0 - cl, moonCov);
        inkA *= 1.0 - moonCov;
        inkB *= 1.0 - moonCov;
    }
    // ---- three depths of trunks against it, cut in vertical strokes: the farther the more carved away (they keep
    // more of the light around them), a lit edge on the moon's side
    for (int L = 0; L < 3; L++) {
        float fl = float(L);
        float sp = L == 0 ? 1.7 : L == 1 ? 2.9 : 4.4;
        float wd = L == 0 ? 0.09 : L == 1 ? 0.17 : 0.3;
        vec3 tr = layerTrunk(w, fl, sp, wd, aa * 1.2);
        if (tr.x <= 0.0) continue;
        float far = L == 0 ? 0.55 : L == 1 ? 0.3 : 0.12;
        float rim = smoothstep(0.35, 0.95, tr.y * sign(uMoon.x - tr.z));
        float tt = (tSky * far + rim * 0.9 * moonL) * open;
        float tc = gouge(vec2(w.y, w.x - tr.z), 0.1 + 0.025 * fl, 4.0, tt, 4.0 + fl, aa);
        carve = mix(carve, tc, tr.x);
    }
    // ---- the canopy: black masses of leaves over the top, their edges cut into fans of leaves where the moon catches
    // them; vines hanging at the sides
    {
        float side = smoothstep(8.0, 16.0, abs(w.x));
        float edge = top + 0.6 - (0.6 + side * (1.0 + 1.3 * fbm3(vec2(w.x * 0.13 + uSeed, 3.0))));
        float d = edge - w.y;
        float gx = w.x / 0.95 + uSeed;
        vec2 fanC = vec2(0.0);
        float fanR = 1.0;
        for (int i = -1; i <= 1; i++) {
            float cid = floor(gx) + float(i);
            float hl = hash12(vec2(cid, 81.0));
            float lx = (cid + 0.5 + (hash12(vec2(cid, 83.0)) - 0.5) * 0.6 - uSeed) * 0.95;
            float ls = smoothstep(8.0, 16.0, abs(lx));
            float le = top + 0.6 - (0.6 + ls * (1.0 + 1.3 * fbm3(vec2(lx * 0.13 + uSeed, 3.0))));
            float rad = (0.35 + 0.55 * hl) * (0.4 + 0.6 * ls);
            vec2 lc = vec2(lx, le - rad * (0.1 + 0.9 * hash12(vec2(cid, 85.0))));
            float dc = length(w - lc) - rad;
            if (dc < d) { fanC = lc; fanR = rad; }
            d = min(d, dc);
        }
        float cov = 1.0 - smoothstep(-aa, aa, d + (vnoise(w * 7.0) - 0.5) * 0.05);
        if (cov > 0.0) {
            // blades radiating from the clump's heart, cut along its lower edge
            vec2 q = w - fanC;
            float ang = atan(q.y, q.x);
            float ai = floor(ang / 6.2832 * 18.0);
            float ac = (ai + 0.5) / 18.0 * 6.2832 + (hash12(vec2(ai, fanC.x)) - 0.5) * 0.15;
            vec2 dir = vec2(cos(ac), sin(ac));
            float tl = moonL * open * smoothstep(0.3, -0.6, dir.y);
            float lb = blade(w, fanC + dir * fanR * 0.5, fanC + dir * fanR * 0.96, fanR * 0.08 * min(tl * 2.0, 1.0));
            float lc = (1.0 - smoothstep(-aa, aa, lb)) * step(0.4, hash12(vec2(ai, fanC.y + 7.0))) * step(0.12, tl);
            carve = mix(carve, lc, cov);
        }
        float vx = w.x * 1.1 + uSeed;
        float vid = floor(vx);
        float hv = hash12(vec2(vid, 41.0));
        float cxv = (vid + 0.5 + (hash12(vec2(vid, 43.0)) - 0.5) * 0.5 - uSeed) / 1.1;
        if (hv > 0.4 && abs(cxv) > 12.6 && abs(cxv) < uView.z - 0.3 && abs(cxv - uMoon.x) > uMoon.z * 1.2) {
            float len = 1.2 + 6.0 * pow(hash12(vec2(vid, 47.0)), 1.6);
            float y0 = top + 0.3 - (0.6 + smoothstep(8.0, 16.0, abs(cxv)) * (1.0 + 1.3 * fbm3(vec2(cxv * 0.13 + uSeed, 3.0))));
            float yy = y0 - w.y;
            if (yy > -0.2 && yy < len + 0.4) {
                float sw = uWind * 0.05 * sin(uTime * 0.45 + hv * 20.0) / 6.0;
                float xs = cxv + sw * yy * yy + 0.07 * sin(yy * 1.2 + hv * 9.0);
                float dv = abs(w.x - xs) - 0.03 * (1.0 - 0.4 * yy / len);
                float k = floor(yy / 0.32);
                float sd = mod(k, 2.0) * 2.0 - 1.0;
                float yk = (k + 0.5) * 0.32;
                vec2 at = vec2(cxv + sw * yk * yk + 0.07 * sin(yk * 1.2 + hv * 9.0), y0 - yk);
                float lf = blade(w, at, at + vec2(sd * 0.2, -0.1), 0.06);
                if (yk > len) lf = 9.0;
                float vc = (1.0 - smoothstep(-aa, aa, min(dv, lf))) * step(yy, len + 0.1);
                // a leaf's midrib cut where the moon catches it
                float rib = 1.0 - smoothstep(-aa, aa, sdSeg(w, at + vec2(sd * 0.04, -0.02), at + vec2(sd * 0.16, -0.08)) - 0.01);
                carve = mix(carve, rib * step(0.25, moonL) * open, vc);
            }
        }
    }
    // ---- the floor: solid ink nicked with U-gouge dabs where the light falls, black grass tufts standing on its edge
    float rockK = smoothstep(0.0, 0.25, rockTop(w.x) - baseY(w.x));
    float edge = gy + ((fbm3(vec2(w.x * 1.1, 0.0) + uSeed) - 0.5) * 0.25) * (1.0 - 0.8 * rockK);
    float floorCov = 1.0 - smoothstep(-aa, aa, w.y - edge);
    float near = smoothstep(5.2, 6.4, abs(w.x));
    {
        float gx = w.x / 0.42 + uSeed;
        float gid = floor(gx);
        float cxg = (gid + 0.5 + (hash12(vec2(gid, 5.0)) - 0.5) * 0.5 - uSeed) * 0.42;
        float hg = (0.18 + 0.35 * pow(hash12(vec2(gid, 7.0)), 2.0)) * (1.0 - 0.7 * rockK) * near;
        float gb = 9.0;
        float sway = uWind * 0.06 * sin(uTime * 0.8 + gid);
        for (int k = 0; k < 3; k++) {
            float fk = float(k) - 1.0;
            vec2 b0 = vec2(cxg + fk * 0.05, edge - 0.05);
            vec2 b1 = b0 + vec2(fk * 0.16 + sway, hg * (1.0 - 0.25 * abs(fk)));
            gb = min(gb, blade(w, b0, b1, 0.035));
        }
        floorCov = max(floorCov, (1.0 - smoothstep(-aa, aa, gb)) * step(0.0, hg - 0.01));
    }
    if (floorCov > 0.0) {
        float depth = max(edge - w.y, 0.0);
        float tf = mistL * exp(-depth / 0.5) * near;
        float fc = stipple(w * vec2(1.0, 1.6), 0.22, tf, 11.0, aa);
        // the rock: broad facets cut in strokes on its lit top only; its face stays solid (the HUD's corner)
)"
R"(        if (rockK > 0.0) {
            float facet = floor(fbm3(w * vec2(0.6, 0.9) + 4.0) * 4.0);
            float tr = (0.45 * exp(-depth / 0.3) + 0.3 * mistL) * (1.0 - smoothstep(0.5, 0.9, depth));
            float rc = gouge(vec2(w.x * 0.5 + w.y * (facet - 1.5) * 0.4, w.y + w.x * (facet - 1.5) * 0.3), 0.12, 1.2, tr, 13.0 + facet, aa);
            fc = mix(fc, rc, rockK);
        }
        // the glowing moss: specks cut and inked in patches over the floor
        float pn = fbm3(vec2(w.x * 0.3, 0.0) + uSeed * 2.0);
        float patch = smoothstep(0.3, 0.5, pn) * near * exp(-depth / 0.6);
        float sp = stipple(w + 3.7, 0.16, (0.15 + 0.35 * uLife) * patch * (1.0 + 1.5 * waveAt(w)), 17.0, aa);
        fc = max(fc, sp);
        inkA = mix(inkA, max(inkA, patch * (1.0 - roll)), floorCov);
        inkB = mix(inkB, max(inkB, patch * roll), floorCov);
        carve = mix(carve, fc * open, floorCov);
    }
    // ---- stones on the floor: black, contour cuts over their lit tops
    {
        float sx = w.x / 2.6 + uSeed * 0.37;
        float base = floor(sx - 0.5);
        for (int k = 0; k < 2; k++) {
            float id = base + float(k);
            float hs = hash12(vec2(id, 61.0));
            if (hs < 0.5) continue;
            float scx = (id + 0.5 + (hash12(vec2(id, 63.0)) - 0.5) * 0.5 - uSeed * 0.37) * 2.6;
            if (abs(scx) < 6.6 || abs(scx) > uView.z - 0.4 || rockTop(scx) > baseY(scx) - 0.1 || poolK(scx) > 0.0) continue;
            float rx = 0.22 + 0.4 * hash12(vec2(id, 65.0)), ry = rx * (0.5 + 0.25 * hash12(vec2(id, 67.0)));
            vec2 q = w - vec2(scx, groundY(scx) - ry * 0.3);
            float ds = (length(q / vec2(rx, ry)) - 1.0) * ry + (vnoise(q * 6.0 + id) - 0.5) * 0.03;
            ds = max(ds, -(q.y + ry * 0.2));
            float sc = 1.0 - smoothstep(-aa, aa, ds);
            if (sc <= 0.0) continue;
            float up = clamp(q.y / ry, -1.0, 1.0);
            float ts = (0.2 + 0.6 * mistL) * smoothstep(0.0, 0.8, up);
            float cc = gouge(vec2(q.x * 2.0, length(q / vec2(rx, ry)) * ry), 0.07, 0.8, ts, id, aa);
            carve = mix(carve, cc * open, sc);
        }
    }
    // ---- the pool: solid ink; the print's reflection is cut into it when the print is pulled (FINAL_FS)
    carve *= 1.0 - waterAt(w);
    frag = vec4(clamp(carve, 0.0, 1.0), inkA, inkB, 1.0);
}
)";

// ---------------------------------------------------------------- the light the plants give (a quarter-resolution field)
// a: x, y, radius, light. b: ink B's share, -, -, its height over its width (fireflies and spores: their shape, angle).
// The key block reads it: the cuts widen around each plant (the sky's, the trunks', the floor's), and its ink is rolled
// under them.
inline const char* HALO_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB;
void main() {
    vL = aPos;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z * vec2(1.0, max(aB.w, 1.0)), 0.0, 1.0);
}
)";
inline const char* HALO_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB;
out vec4 frag;
void main() {
    float r2 = dot(vL, vL);
    if (r2 >= 1.0) discard;
    // a little more of it upward
    vec2 q = vL * vec2(1.0, vL.y > 0.0 ? 0.8 : 1.15);
    float light = vA.w * (exp(-dot(q, q) * 3.0) - exp(-3.0)) / (1.0 - exp(-3.0));
    // its ink, rolled wider than its light
    float ink = smoothstep(1.0, 0.3, sqrt(r2));
    frag = vec4(light, ink * (1.0 - vB.x), ink * vB.x, 0.0);
}
)";

// ---------------------------------------------------------------- the life (instanced quads)
// a: base x, y, scale, kind. b: seed, grow (0..1: cut in as the print is pulled), glow, flip. c: ink B's share, -, -,
// wave. d: -, -, -, alpha.
inline const char* PLANT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vP;
out vec2 vW;
flat out vec4 vA, vB, vC, vD;
void main() {
    int kind = int(aA.w + 0.5);
    vec2 lo = vec2(-1.7, -0.3), hi = vec2(1.7, 3.1);
    if (kind == 2) { lo = vec2(-1.0, -5.6); hi = vec2(1.0, 0.2); }
    if (kind == 5) { lo = vec2(-1.3, -0.3); hi = vec2(1.3, 0.8); }
    if (kind == 6) { lo = vec2(-0.25, -0.9); hi = vec2(0.8, 0.9); }
    if (kind == 8) { lo = vec2(-1.4, -0.2); hi = vec2(1.4, 3.0); }
    if (kind == 7) { lo = vec2(-1.3, -0.2); hi = vec2(1.3, 2.6); }
    vec2 l = mix(lo, hi, aPos * 0.5 + 0.5);
    vP = l;
    vA = aA; vB = aB; vC = aC; vD = aD;
    vec2 lp = l;
    if (aB.w > 0.5) lp.x = -lp.x;
    vW = aA.xy + lp * aA.z;
    gl_Position = uVP * vec4(vW, 0.0, 1.0);
}
)";

// Each plant is cut out of the key and inked: its bright parts (a cap's rim, a bell's mouth, a bead, a seed head)
// carved bare, its body shaded by cuts across it (dense where it glows, sparse in its shadow), its dark parts (stems,
// the underside) left in solid ink.
inline const char* PLANT_FS = R"(#version 330 core
in vec2 vP;
in vec2 vW;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform float uTime, uAAp, uWind;
uniform float uOut;  // the outline pass: how far the shapes are grown (board units); 0: the plants themselves
float gAA = 0.01, gOut = 0.0;
// a shape's coverage from its distance (grown by the outline in the outline pass)
float cov(float d) { return 1.0 - smoothstep(-gAA, gAA, d - gOut); }
float h1(float s, float k) { return fract(sin(s * 91.7 + k * 47.3) * 43758.5453); }
// a quadratic curve's nearest point: distance, parameter (sampled, plants are small)
vec2 bezDist(vec2 p, vec2 a, vec2 b, vec2 c) {
    float best = 1e9, bt = 0.0;
    vec2 prev = a;
    for (int i = 1; i <= 10; i++) {
        float t = float(i) / 10.0;
        vec2 q = mix(mix(a, b, t), mix(b, c, t), t);
        vec2 pa = p - prev, ba = q - prev;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        float d = length(pa - ba * h);
        if (d < best) { best = d; bt = (float(i - 1) + h) / 10.0; }
        prev = q;
    }
    return vec2(best, bt);
}
vec2 bez(vec2 a, vec2 b, vec2 c, float t) { return mix(mix(a, b, t), mix(b, c, t), t); }
// layering: a shape in front covers what is behind it (its light replaces the light behind)
void lay(inout float e, inout float a, float cov, float em) { e = mix(e, em, cov); a = max(a, cov); }
// a thin bright thing: carved bare
void lit(inout float e, inout float a, float cov, float em) { e = max(e, em * cov); a = max(a, cov); }
// a coil (a fern's fiddlehead): distance to an opening spiral of `turns` around c, radius R
float coil(vec2 p, vec2 c, float R, float turns, float ang0) {
    vec2 q = p - c;
    float r = length(q);
    if (r > R * 1.2) return r - R;
    float a = atan(q.y, q.x) - ang0;
    float b = R / (6.2832 * turns);
    float s = (r / b - a) / 6.2832;
    float k = clamp(floor(s + 0.5), 0.0, turns);
    float rr = b * (a + 6.2832 * k);
    if (rr < 0.0) rr += b * 6.2832;
    return abs(r - rr);
}
// One mushroom: a stem from `base` to `tp`, a cap of the given form (0 dome, 1 conical bell, 2 flat parasol) of
// radius cr.
void mushroom(vec2 p, vec2 base, vec2 tp, float cr, float form, float seed, float aa, float wob, inout float e, inout float a) {
    float hs;
    float ds = segT(p, base, tp, hs);
    float sw = (0.035 + 0.06 * cr) * (1.0 - 0.3 * hs) + 0.05 * cr * exp(-hs * 9.0);
    float cs = cov(ds - sw + wob);
    lay(e, a, cs, 0.1 + 0.55 * hs * hs);
    // a ring under the large caps
    if (cr > 0.3 && form > 0.5) {
        vec2 rq = p - mix(base, tp, 0.82);
        float dr = (length(rq / vec2(sw * 2.4, 0.035)) - 1.0) * 0.035;
        lay(e, a, cov(dr), 0.9);
    }
    vec2 q = p - tp;
    float d, ry, rx;
    if (form < 0.5) {
        rx = cr; ry = cr * 0.62;
        d = (length(q / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -(q.y + 0.1 * ry - 0.4 * ry * (q.x / rx) * (q.x / rx)));
    } else if (form < 1.5) {
        rx = cr * 0.8; ry = cr * 1.1;
        float wq = rx * (1.0 - 0.7 * clamp(q.y / ry, 0.0, 1.0)) * (1.0 + 0.25 * smoothstep(0.0, -0.2 * ry, q.y));
        d = (length(vec2(q.x / max(wq, 1e-3), q.y / ry)) - 1.0) * min(wq, ry) * 0.8;
        d = max(d, -(q.y + 0.18 * ry));
    } else {
        rx = cr * 1.3; ry = cr * 0.3;
        d = (length(q / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -(q.y + 0.08 * cr - 0.25 * ry * (q.x / rx) * (q.x / rx)));
        d = min(d, length(q - vec2(0.0, ry * 0.7)) - cr * 0.16);
    }
    // gills under the wide caps: fine cuts converging on the stem
    if (form != 1.0) {
        float y0 = -0.08 * ry, gh = 0.22 * cr;
        float yy = y0 - q.y;
        if (yy > 0.0 && yy < gh) {
            float hw = rx * (0.98 - 0.55 * yy / gh);
            float gc = (cov(abs(q.x) - hw)) * (1.0 - smoothstep(gh - aa * 2.0, gh, yy));
            float ang = atan(q.x, q.y + gh * 3.0 + 0.0001);
            float lines = smoothstep(0.55, 0.8, 0.5 + 0.5 * cos(ang * (11.0 + 5.0 * h1(seed, 4.0)) * 6.2832 / 3.1416));
            lay(e, a, gc, lines * (0.4 + 0.6 * abs(q.x) / max(hw, 1e-3)) * 1.2);
        }
    }
    d += wob;
    float cc = cov(d);
    if (cc > 0.0) {
        float in_ = clamp(-d / (ry * 0.5), 0.0, 1.0);
        // brightest at the rim, shaded toward the crown; spots on the domes left in ink
        float rimL = exp(-max(q.y + 0.08 * ry, 0.0) / (ry * 0.35));
        float body = 0.4 + 0.35 * (1.0 - in_) + 0.7 * rimL;
        if (form < 0.5) {
            vec2 sg = q / cr * 3.0 + seed * 13.0;
            vec2 sid = floor(sg), sf = fract(sg) - 0.5;
            if (hash12(sid) > 0.55) body *= smoothstep(0.03, 0.06, dot(sf, sf));
        }
        lay(e, a, cc, body);
    }
}
void main() {
    vec2 p = vP;
    int kind = int(vA.w + 0.5);
    float seed = vB.x, grow = vB.y, glow = vB.z;
    float aa = max(uAAp / vA.z, 0.004);
    gAA = aa;
    gOut = uOut / vA.z;
    float e = 0.0;      // how bright (how much is carved away)
    float a = 0.0;      // coverage of the plant
    float g = smoothstep(0.0, 1.0, grow);
    // the wind: a slow sway, more at the top
    if (kind != 6 && kind != 2) {
        float s = uWind * (0.6 * sin(uTime * 0.7 + seed * 2.0) + 0.4 * sin(uTime * 1.23 + seed * 5.0));
        p.x -= s * 0.03 * p.y * p.y / (kind == 0 ? 2.5 : 1.0);
    }
    // a hand-cut edge: a little wobble
    float wob = (vnoise(p * 9.0 + seed * 31.0) - 0.5) * 0.02;
    if (kind == 0) {
        // a cluster of mushrooms of one form, the small ones behind, the largest in front; pins at their feet
        float form = floor(h1(seed, 20.0) * 2.999);
        int n = 3 + int(h1(seed, 21.0) * 2.999);
        for (int j = 0; j < 3; j++) {
            float fj = float(j);
            float gj = smoothstep(0.1 + fj * 0.1, 0.5 + fj * 0.1, grow);
            if (gj <= 0.0) continue;
            vec2 pc = vec2((h1(seed, fj + 30.0) - 0.5) * 1.8, 0.0);
            float pr = (0.05 + 0.04 * h1(seed, fj + 33.0)) * gj;
            mushroom(p, pc, pc + vec2(0.0, 0.1 + 0.1 * h1(seed, fj + 36.0)) * gj, pr, 0.0, seed + fj, aa, 0.0, e, a);
        }
        for (int i = 0; i < 5; i++) {
            if (i >= n) break;
            float fi = float(i);
            bool main_ = i == n - 1;
            float gi = smoothstep(fi * 0.12, 0.55 + fi * 0.12, grow);
            if (gi <= 0.0) continue;
            float x0 = main_ ? (h1(seed, 2.0) - 0.5) * 0.2 : (h1(seed, fi) - 0.5) * 1.5;
            float sz = main_ ? 1.0 : 0.45 + 0.35 * h1(seed, fi + 3.0);
            float hh = (form > 1.5 ? 1.35 : form > 0.5 ? 1.15 : 1.05) * sz * gi * (0.9 + 0.2 * h1(seed, fi + 9.0));
            float cr = (form > 1.5 ? 0.36 : form > 0.5 ? 0.28 : 0.42) * sz * (0.4 + 0.6 * gi);
            float lean = (h1(seed, fi + 7.0) - 0.5) * 0.3 + x0 * 0.18;
            mushroom(p, vec2(x0, 0.0), vec2(x0 + lean, hh), cr, form, seed + fi * 3.0, aa, wob, e, a);
        }
    } else if (kind == 1) {
        // mycena: slender threads with small glowing bells
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.1, 0.5 + fi * 0.1, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 2.5) * 0.22 + (h1(seed, fi) - 0.5) * 0.18;
            float hh = (0.6 + 1.35 * h1(seed, fi + 2.0)) * gi;
            float bend = (h1(seed, fi + 4.0) - 0.5) * 0.6;
            vec2 tip = vec2(x0 + bend, hh);
            vec2 bd = bezDist(p, vec2(x0, 0.0), vec2(x0, hh * 0.6), tip);
            float cs = cov(bd.x - 0.02);
            lit(e, a, cs, 0.2 + 0.7 * bd.y * bd.y);
            float br = (0.11 + 0.07 * h1(seed, fi + 6.0)) * (0.3 + 0.7 * gi);
            vec2 q = p - tip - vec2(0.0, -br * 0.15);
            float wq = br * (1.0 - 0.45 * clamp(q.y / br, 0.0, 1.0)) * (1.0 + 0.3 * smoothstep(0.0, -0.3 * br, q.y));
            float cb = cov((length(vec2(q.x / wq, q.y / (br * 1.15))) - 1.0) * br);
            cb *= smoothstep(-br * 0.42, -br * 0.3, q.y);
            float rimL = exp(-abs(q.y + br * 0.32) / (br * 0.3));
            lay(e, a, cb, 0.55 + 0.6 * rimL);
        }
    } else if (kind == 2) {
        // glowworm silk: threads hanging from the canopy, beaded with drops of light, brightest at their tips
        for (int i = 0; i < 5; i++) {
            float fi = float(i);
            if (fi > 1.0 + 3.0 * h1(seed, 9.0)) break;
            float gi = smoothstep(fi * 0.12, 0.55 + fi * 0.12, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 1.5) * 0.3 + (h1(seed, fi) - 0.5) * 0.22;
            float len = (1.6 + 3.6 * h1(seed, fi + 2.0)) * gi;
            float sway = (0.04 + 0.04 * uWind) * sin(uTime * 0.4 + seed + fi * 1.7);
            float yy = clamp(p.y, -len, 0.0);
            float xs = x0 + sway * (-yy / 3.0);
            float th = (cov(abs(p.x - xs) - 0.012)) * step(-len, p.y) * step(p.y, 0.0);
            lit(e, a, th, 0.25 + 0.7 * (-p.y / max(len, 0.1)));
            float sp = 0.3;
            float k = floor(-p.y / sp);
            for (int j = 0; j < 2; j++) {
                float kk = k + float(j);
                float dy = -(kk + 0.5) * sp;
                float tipK = smoothstep(0.3, 1.0, -dy / max(len, 0.1));
                if (dy < -len || hash12(vec2(kk, seed * 10.0 + fi)) > 0.25 + 0.4 * tipK) continue;
                float br = 0.022 + 0.03 * tipK + 0.014 * hash12(vec2(kk + 3.0, fi));
                float xd = x0 + sway * (-dy / 3.0);
                float cb = cov(length(p - vec2(xd, dy)) - br);
                lit(e, a, cb, 1.0);
            }
            vec2 qt = p - vec2(x0 + sway * (len / 3.0), -len);
            float ct = cov(length(qt) - 0.07 * gi);
            lit(e, a, ct, 1.2);
        }
    } else if (kind == 3) {
        // a fern: fronds in layers, each arching from the crown with a cut midrib and leaflets cut along their edges;
        // cut in, each frond unrolls from a fiddlehead coil; a young coil at the crown
        for (int j = 0; j < 5; j++) {
            float fj = float(j);
            float gj = smoothstep(fj * 0.1, 0.6 + fj * 0.1, grow);
            if (gj <= 0.0) continue;
            float side = mod(fj, 2.0) * 2.0 - 1.0;
            float ang = j == 4 ? (h1(seed, 4.0) - 0.5) * 0.3 : side * (j < 2 ? 1.05 : 0.55) + (h1(seed, fj) - 0.5) * 0.25;
            float dim = j < 2 ? 0.6 : j < 4 ? 0.85 : 1.0;
            float L = (j == 4 ? 2.5 : j < 2 ? 1.6 : 2.1) * (0.9 + 0.2 * h1(seed, fj + 2.0));
            vec2 tip = vec2(sin(ang) * L * 0.95, cos(ang) * L * 0.72);
            vec2 ctrl = vec2(sin(ang) * L * 0.2, L * 0.95);
            float tm = 0.2 + 0.8 * smoothstep(0.0, 0.92, gj);
            vec2 a0 = vec2(0.0), c1 = mix(a0, ctrl, tm), e1 = bez(a0, ctrl, tip, tm);
            vec2 bd = bezDist(p, a0, c1, e1);
            float t = bd.y * tm;
            float s = t * L * 6.0;
            float lw = 0.34 * pow(1.0 - t, 0.8) * smoothstep(0.0, 0.12, t) * smoothstep(tm, tm - 0.08, t) * (0.6 + 0.4 * gj);
            float u = fract(s - bd.x * 1.8) - 0.5;
            float dl = bd.x - lw * (1.0 - 4.0 * u * u) + wob;
            float cl = cov(dl);
            float rib = cov(bd.x - 0.03 * (1.0 - 0.5 * t));
            // each leaflet: bare along its outer edge, a black cut between leaflets
            float edgeL = exp(-abs(dl) / 0.05) * cl;
            float split = smoothstep(0.36, 0.48, abs(u));
            lay(e, a, max(cl, rib), dim * max(rib, (0.3 + 0.8 * edgeL) * (1.0 - split)));
            if (tm < 0.99) {
                vec2 dir = normalize(e1 - bez(a0, ctrl, tip, tm - 0.02));
                vec2 nrm = vec2(-dir.y, dir.x) * -side;
                float R = (0.1 + 0.22 * (1.0 - tm)) * (0.8 + 0.2 * gj);
                vec2 cc = e1 + nrm * R;
                float dc = coil(p, cc, R, 1.5, atan(-nrm.y, -nrm.x)) - 0.035;
                lit(e, a, cov(dc), dim);
            }
        }
        float gy0 = smoothstep(0.5, 1.0, grow);
        if (gy0 > 0.0) {
            float sx = (h1(seed, 11.0) - 0.5) * 0.5;
            vec2 b0 = vec2(sx, 0.0), b1 = vec2(sx + 0.05, 0.55 * gy0);
)"
R"(            lit(e, a, cov(sdSeg(p, b0, b1) - 0.022), 0.6);
            float dc = coil(p, b1 + vec2(0.09, 0.0), 0.1, 1.6, 3.1416) - 0.026;
            lit(e, a, cov(dc), 1.0);
        }
    } else if (kind == 4) {
        // bell flowers: an arching stem with flared bells hanging from it, a bright pistil at each mouth; two blades
        for (int b = 0; b < 2; b++) {
            float fb = float(b) * 2.0 - 1.0;
            vec2 bt = vec2(fb * 0.75, 1.0) * (0.4 + 0.6 * g) * (0.8 + 0.3 * h1(seed, 9.0 + float(b)));
            vec2 bd2 = bezDist(p, vec2(0.0), vec2(fb * 0.05, bt.y * 0.8), bt);
            float dbl = bd2.x - 0.07 * (1.0 - bd2.y);
            lay(e, a, cov(dbl), 0.15 + 0.6 * exp(-abs(dbl) / 0.02) * bd2.y);
        }
        float L = 2.1 * g;
        vec2 tip = vec2(0.9 * L / 2.1, L * 0.82);
        vec2 ctrl = vec2(-0.1, L * 1.05);
        vec2 bd = bezDist(p, vec2(0.0), ctrl, tip);
        lay(e, a, cov(bd.x - 0.025), 0.2);
        for (int k = 0; k < 6; k++) {
            float fk = float(k);
            float tk = 0.42 + fk * 0.11;
            float gk = smoothstep(0.3 + fk * 0.09, 0.6 + fk * 0.09, grow);
            if (gk <= 0.0) continue;
            vec2 at = bez(vec2(0.0), ctrl, tip, tk);
            float hang = 0.1 + 0.07 * h1(seed, fk);
            vec2 bc = at - vec2(0.0, hang);
            float br = (0.09 + 0.04 * h1(seed, fk + 3.0)) * gk * (1.0 - 0.25 * fk / 5.0);
            lay(e, a, cov(sdSeg(p, at, bc) - 0.012), 0.2);
            vec2 q = p - bc;
            // the bell: a dome flaring out to a scalloped mouth, bare at the mouth, shaded up the bell
            float flare = smoothstep(0.0, -1.3 * br, q.y);
            float wq = br * (0.62 + 0.5 * flare);
            float mouth = -1.25 * br + 0.06 * br * cos(q.x / br * 9.0);
            float db = max((length(vec2(q.x / wq, max(q.y, 0.0) / br)) - 1.0) * br * 0.7, abs(q.x) - wq);
            db = max(db, mouth - q.y);
            float cb = cov(db);
            float rimL = exp(-max(q.y - mouth, 0.0) / (br * 0.35));
            lay(e, a, cb, 0.35 + 0.35 * flare + 0.7 * rimL);
            float pc = cov(length(q - vec2(0.0, mouth - br * 0.25)) - br * 0.2);
            lit(e, a, pc, 1.2);
        }
    } else if (kind == 8) {
        // a young tree: a slender stem forking into bare branches, each with a twig
        float H = 2.5 * (0.3 + 0.7 * g);
        vec2 tp = vec2(0.2 * (h1(seed, 1.0) - 0.5), H);
        lay(e, a, cov(sdSeg(p, vec2(0.0), tp) - 0.04 * (1.0 - 0.5 * clamp(p.y / H, 0.0, 1.0))), 0.6);
        for (int k = 0; k < 4; k++) {
            float fk = float(k);
            float hb = H * (0.32 + 0.16 * fk);
            float sd = (mod(fk + floor(h1(seed, 2.0) * 2.0), 2.0) * 2.0 - 1.0);
            float L = H * (0.46 - 0.08 * fk) * (0.8 + 0.4 * h1(seed, fk + 3.0));
            vec2 b0 = vec2(tp.x * hb / H, hb);
            vec2 b1 = b0 + vec2(sd * 0.72, 0.7) * L;
            lay(e, a, cov(sdSeg(p, b0, b1) - 0.024), 0.62);
            lay(e, a, cov(sdSeg(p, mix(b0, b1, 0.6), mix(b0, b1, 0.6) + vec2(sd * 0.1, 0.32) * L) - 0.018), 0.62);
        }
        lay(e, a, cov(sdSeg(p, tp, tp + vec2(-0.22, 0.32)) - 0.018), 0.62);
        lay(e, a, cov(sdSeg(p, tp, tp + vec2(0.24, 0.28)) - 0.018), 0.62);
    } else if (kind == 5) {
        // a cushion of moss: a low black mound nicked all over, spore heads cut bare on it
        float rx = 0.95 * (0.4 + 0.6 * g), ry = 0.3 * g + 0.02;
        float d = (length(p / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -p.y - 0.04);
        d += (vnoise(vec2(p.x * 6.0, seed * 9.0)) - 0.5) * 0.05 + (vnoise(vec2(p.x * 22.0, seed)) - 0.5) * 0.02;
        float cv = cov(d);
        float in_ = clamp(-d / max(ry, 0.01), 0.0, 1.0);
        e = cv * (0.2 + 0.5 * (1.0 - in_));
        a = cv;
        for (int k = 0; k < 8; k++) {
            float fk = float(k);
            float sx = (h1(seed, fk) - 0.5) * 1.6 * rx;
            float top = ry * sqrt(max(1.0 - sx * sx / (rx * rx), 0.0));
            float sh = 0.06 + 0.14 * h1(seed, fk + 7.0);
            vec2 hc = vec2(sx + 0.03 * (h1(seed, fk + 3.0) - 0.5), top + sh * g);
            float th = cov(sdSeg(p, vec2(sx, top - 0.02), hc) - 0.012);
            float ch = cov(length(p - hc) - 0.04 * g);
            lay(e, a, th, 0.0);
            lit(e, a, ch, 1.1);
        }
    } else if (kind == 6) {
        // bracket fungi: shelves stacked up the bark, growth rings cut across them, the rim bare
        for (int i = 0; i < 4; i++) {
            float fi = float(i);
            if (fi > 0.6 + 2.2 * h1(seed, 9.0)) break;
            float gi = smoothstep(fi * 0.15, 0.5 + fi * 0.15, grow);
            if (gi <= 0.0) continue;
            float y0 = -0.55 + fi * (0.36 + 0.24 * h1(seed, 12.0)) + (h1(seed, fi) - 0.5) * 0.16;
            float rx = (0.3 + 0.32 * h1(seed, fi + 2.0)) * (1.0 - 0.15 * fi) * (0.3 + 0.7 * gi);
            float ry = rx * 0.48;
            vec2 q = p - vec2(-0.08 + 0.06 * h1(seed, fi + 5.0), y0);
            float xr = clamp(q.x / rx, 0.0, 1.0);
            float under = q.y + ry * (0.3 - 0.35 * xr * xr);
            float d = (length(q / vec2(rx, ry)) - 1.0) * ry * 0.8;
            d = max(d, -q.x);
            d = max(d, -under);
            d += wob * 0.6;
            float cv = cov(d);
            float rr = length(q / vec2(rx, ry));
            float rings = smoothstep(0.6, 0.85, 0.5 + 0.5 * cos(rr * 20.0 + seed));
            float rimL = exp(-max(-d, 0.0) / (ry * 0.18)) * smoothstep(0.3, 0.8, xr);
            lay(e, a, cv, 0.15 + 0.6 * rings * rr + 1.0 * rimL);
        }
    } else {
        // glowing reeds: a tuft of slender black blades, some tipped with a seed head cut bare
        for (int i = 0; i < 7; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.07, 0.5 + fi * 0.07, grow);
            if (gi <= 0.0) continue;
            float x0 = (h1(seed, fi) - 0.5) * 0.45;
            float hh = (0.8 + 1.3 * h1(seed, fi + 2.0)) * gi;
            float bend = (h1(seed, fi + 4.0) - 0.5) * 0.9 + x0 * 1.2;
            vec2 tip = vec2(x0 + bend, hh);
            vec2 bd = bezDist(p, vec2(x0, 0.0), vec2(x0 + bend * 0.1, hh * 0.7), tip);
            float cb = cov(bd.x - (0.03 * (1.0 - bd.y) + 0.008));
            lay(e, a, cb, 0.08 + 0.5 * bd.y * bd.y);
            if (h1(seed, fi + 6.0) > 0.35) {
                vec2 dir = normalize(tip - bez(vec2(x0, 0.0), vec2(x0 + bend * 0.1, hh * 0.7), tip, 0.9));
                vec2 q = p - tip - dir * 0.08 * gi;
                q = vec2(dot(q, vec2(dir.y, -dir.x)), dot(q, dir));
                float ch = cov((length(q / vec2(0.034, 0.11)) - 1.0) * 0.034);
                lit(e, a, ch * gi, 1.2);
            }
        }
    }
    a *= vD.w;
    if (a <= 0.003) discard;
    // the outline pass: a margin of solid ink left around the plant, so that it stands out of the light around it
    if (uOut > 0.0) {
        frag = vec4(0.0, 0.0, 0.0, a);
        return;
    }
    // the light it gives: its bright parts carved bare, its body shaded by cuts across it (in world space, so every
    // plant is cut with the same gouge), its shadows left in ink
    float wave = vC.w;
    float tone = smoothstep(0.15, 0.85, e * (0.65 + 0.5 * glow) * (1.0 + 0.6 * wave));
    float cut = max(gouge(vec2(vW.x, vW.y), 0.11, 0.5, tone, seed, uAAp * 0.6), smoothstep(0.6, 0.85, tone));
    // the two glow inks: the plant's own, the other creeping into its tips
    float tip = clamp((kind == 2 ? -p.y * 0.5 : kind == 6 ? p.x * 1.2 : p.y) / 2.6, 0.0, 1.0) * 0.6;
    float mB = mix(vC.x, 1.0 - vC.x, tip);
    frag = vec4(cut, 1.0 - mB, mB, 1.0) * a;
}
)";

// ---------------------------------------------------------------- fireflies, spores and chips (instanced)
// a: x, y, radius, alpha. b: ink B's share, kind (0 a spark, 1 a dot, 2 a chip), angle, -.
inline const char* MOTE_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB;
out vec4 frag;
uniform float uAA;
void main() {
    float r2 = dot(vL, vL);
    if (r2 >= 1.0) discard;
    vec2 q = vL * vA.z;
    float aa = uAA * 0.75;
    float d;
    if (vB.y < 0.5) {
        // a spark: four short cuts crossing
        float an = vB.z;
        vec2 u = vec2(cos(an), sin(an)), v = vec2(-u.y, u.x);
        float R = vA.z * 0.9;
        d = min(blade(q, -u * R, u * R, R * 0.13), blade(q, -v * R * 0.7, v * R * 0.7, R * 0.11));
        d = min(d, length(q) - R * 0.2);
    } else if (vB.y < 1.5) {
        d = length(q) - vA.z * 0.55;
    } else if (vB.y > 2.5) {
        // a moth: a slim body, two pairs of wings beating (vB.w: their beat, -1..1)
        vec2 u = vec2(cos(vB.z), sin(vB.z)), v = vec2(-u.y, u.x);
        float R = vA.z;
        float open = 0.25 + 0.75 * abs(vB.w);
        vec2 l = vec2(dot(q, u), dot(q, v));
        d = blade(q, -u * R * 0.35, u * R * 0.3, R * 0.07);
        vec2 fw = vec2(l.x - R * 0.08, abs(l.y) - R * 0.42 * open) / vec2(R * 0.3, R * 0.42 * open + 0.02 * R);
        vec2 hw = vec2(l.x + R * 0.22, abs(l.y) - R * 0.26 * open) / vec2(R * 0.2, R * 0.28 * open + 0.02 * R);
        d = min(d, (length(fw) - 1.0) * R * 0.3);
        d = min(d, (length(hw) - 1.0) * R * 0.2);
    } else {
        // a chip of the block, curled
        vec2 u = vec2(cos(vB.z), sin(vB.z));
        d = blade(q, -u * vA.z * 0.8, u * vA.z * 0.8, vA.z * 0.3);
    }
    float c = (1.0 - smoothstep(-aa, aa, d)) * vA.w;
    // its ink a little wider than its cut
    float ink = smoothstep(1.0, 0.4, sqrt(r2)) * vA.w;
    frag = vec4(c, ink * (1.0 - vB.x), ink * vB.x, 0.0);
}
)";

// ---------------------------------------------------------------- the print (full screen)
// The plates pulled onto the paper: the glow inks (a little out of register), the drop's third ink rolled over them,
// the black key over all, its carved edges pressed into the paper, its ink starved here and there.
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPlates, uBloom;
uniform vec2 uTexel;
uniform vec2 uOffA, uOffB;            // the glow inks' plates out of register (uv)
uniform vec3 uPaper, uKey, uInkA, uInkB, uInkC; // linear
uniform float uC, uRollR;             // the drop's third ink: its amount, how far it has rolled out from the well
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uTime, uSeed, uAA, uInkK, uKeyK;
uniform float uBlaze;                 // the drop: the neon forest comes in
uniform sampler2D uNeon;              // the drop's neon forest (NEON_FS): its light, how much it hides of what is behind
uniform vec3 uAura;                   // the violet aura behind the left's trees: center, radius
uniform vec3 uBridgeCol, uSlowInk;
uniform vec4 uView;
vec3 ink(vec3 c, vec3 k, float d) { return c * mix(vec3(1.0), k, clamp(d, 0.0, 1.0)); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec4 p = texture(uPlates, vUv);
    float carve = p.r;
    float a = texture(uPlates, vUv + uOffA).g, b = texture(uPlates, vUv + uOffB).b;
    // the pool: its surface cut in long horizontal strokes, as wide as the print above it is light (rippling)
    float wa = waterAt(w);
    if (wa > 0.0) {
        float dpt = uPool.z - w.y;
        float rip = (vnoise(vec2(w.y * 20.0, uTime * 0.5 + w.x * 0.3)) - 0.5) * 0.12 * (0.4 + dpt * 2.0);
        vec2 rw = vec2(w.x + rip, uPool.z + 0.04 + dpt * 2.4);
        vec2 ruv = (rw - uView.xy) / uView.zw * 0.5 + 0.5;
        vec4 rp = texture(uPlates, ruv);
        float t = rp.r * 0.75 * exp(-dpt / 1.4) + 0.05;
        carve = mix(carve, gouge(vec2(w.x, w.y), 0.12, 1.8, t, 31.0, uAA * 0.75), wa);
        a = mix(a, rp.g, wa);
        b = mix(b, rp.b, wa);
    }
    // the paper: its tooth, and fibres here and there
    float fib = vnoise(vec2(w.x * 3.0 + w.y * 40.0, w.y * 3.0 - w.x * 9.0) + uSeed) * vnoise(w * 1.7 + 3.0);
    vec3 c = uPaper * (0.965 + 0.05 * vnoise(gl_FragCoord.xy * 0.45) - 0.05 * smoothstep(0.55, 0.8, fib));
    // the glow inks, a little out of register; the drop's third ink takes the second's place, rolled out from the
    // well; slow motion prints both in one cold ink
    a *= uInkK;
    b *= uInkK;
    vec3 inkA = mix(uInkA, uSlowInk, uSlow);
    vec3 inkB = mix(mix(uInkB, uInkC, uC * dropRoll(w, uRollR)), uSlowInk, uSlow);
    // under the drop's blacklight the bare paper sinks to a dim violet
    float inked = clamp(a + b, 0.0, 1.0);
    c *= mix(vec3(1.0), vec3(0.2, 0.16, 0.36), uBlaze * (1.0 - inked));
    c = ink(c, inkA, a);
    c = ink(c, inkB, b);
    // fluorescent inks are a little brighter than the paper under them
    c += (inkA * a + inkB * b) * 0.1 * (1.0 - 0.6 * uSlow) * carve;
    // the key block over all: its ink starved where the roller was light (paper speckling through a solid, open areas
    // only), its carved edges pressed into the paper (a shadow on one side of every cut)
    float open = 1.0 - quietAt(w);
    float starve = smoothstep(0.97, 1.0, vnoise(gl_FragCoord.xy * 0.7 + uSeed * 7.0) * (0.75 + 0.3 * vnoise(w * 0.4 + 1.0))) * open * 0.6;
    float key = (1.0 - carve) * (1.0 - starve) * uKeyK;
    float cx = texture(uPlates, vUv + vec2(uTexel.x, 0.0)).r, cy = texture(uPlates, vUv + vec2(0.0, uTexel.y)).r;
    float press = clamp((cx - carve) * 0.7 - (cy - carve) * 0.7, -1.0, 1.0);
    c *= 1.0 - 0.18 * max(press, 0.0) * carve;
    c = mix(c, uKey * (0.85 + 0.3 * vnoise(w * 0.6 + uSeed)), key);
    // the drop: the neon forest (NEON_FS) comes in over the print with a wave out from the well, on a clean night: a
    // violet aura behind the left's trees, a glow on the ground (warm on the left, green on the right) and a field of
    // glowing specks on it, neon tubes up the well's sides; behind the HUD's text the neon fades
    if (uBlaze > 0.001) {
        float lit = smoothstep(0.0, 0.7, uBlaze) * neonIn(w, uRollR);
        vec4 n = texture(uNeon, vUv);
        float gy = groundY(w.x);
        vec3 dc = uKey * 0.4 + vec3(0.003, 0.004, 0.014);
        vec2 aq = (w - uAura.xy) / uAura.z;
        dc += uNeonL[1] * (0.16 * exp(-dot(aq, aq) * 2.2) + 0.08 * smoothstep(1.0, 0.85, length(aq)) * smoothstep(0.55, 0.9, length(aq)));
        vec3 ground = w.x < 0.0 ? uNeonWarm : uNeonR[1];
        float side = smoothstep(5.5, 7.5, abs(w.x));
        // the ground breathes with the beat, the waves running over it
        float gw = dropWave(w);
        float gl = 1.0 + uDropK * 1.2 * gw;
        dc += ground * 0.07 * gl * exp(-max(w.y - gy, 0.0) / 1.4) * smoothstep(gy - 2.0, gy, w.y) * side;
        if (w.y < gy + 0.6) {
            vec2 g = w / 0.16;
            vec2 id = floor(g), f = fract(g) - 0.5;
            float h = hash12(id + 31.0);
            vec2 o = (vec2(hash12(id + 7.0), hash12(id + 13.0)) - 0.5) * 0.6;
            float dot_ = exp(-dot(f - o, f - o) / 0.012) * step(0.4, h) * side * smoothstep(gy + 0.6, gy - 0.2, w.y);
            // each speck twinkles slowly, and flares as a wave passes
            float tw = 0.65 + 0.35 * sin(uTime * (1.3 + 2.0 * hash12(id + 5.0)) + h * 40.0);
            dc += mix(ground, vec3(1.0), 0.3 + 0.3 * gw) * dot_ * (0.5 + 0.8 * hash12(id + 3.0)) * mix(1.0, tw * (0.85 + 1.6 * gw), uDropK);
        }
        // (the HUD's columns stay dark above the floor, its text too)
        float shown = 1.0 - max(textQuietAt(w), columnsAt(w));
        dc = dc * (1.0 - n.a * shown) + n.rgb * shown;
        float tx = abs(abs(w.x) - 5.32);
        vec3 tcol = w.x < 0.0 ? mix(uNeonL[1], uNeonL[2], 0.5) : uNeonR[0];
        dc += (tcol * 1.2 + vec3(0.3)) * (1.0 - smoothstep(0.025, 0.025 + uAA * 1.5, tx)) * step(-10.4, w.y);
        dc += tcol * exp(-tx / 0.25) * 0.08 * step(-10.4, w.y);
        c = mix(c, dc, lit);
    }
    // the fluorescence: a soft light the inks give off into the dark around them
    c += texture(uBloom, vUv).rgb * uBloomK;
    c = c / (1.0 + 0.25 * max(max(c.r, c.g), c.b));
    c = mix(c, uBridgeCol, uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the fluorescence
// The visible glow ink (cut bare, inked) at a quarter of the resolution.
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform vec3 uInkA, uInkB, uInkC;
uniform float uC, uRollR, uInkK, uBlaze;
uniform vec4 uView;
uniform sampler2D uNeon;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec3 inkB = mix(uInkB, uInkC, uC * dropRoll(w, uRollR));
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec2 o = vec2(i == 1 || i == 3 ? 1.0 : -1.0, i >= 2 ? 1.0 : -1.0);
        vec4 s = texture(uSrc, vUv + uTexel * o);
        float a = s.g * uInkK, b = s.b * uInkK;
        c += (uInkA * a + inkB * b) * s.r;
    }
    c *= 0.25;
    // the drop's neon forest glowing
    if (uBlaze > 0.001)
        c = mix(c, texture(uNeon, vUv).rgb * 0.9 * (1.0 - columnsAt(w)), smoothstep(0.0, 0.7, uBlaze) * neonIn(w, uRollR));
    frag = vec4(c, 1.0);
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

// ---------------------------------------------------------------- the drop's neon forest (instanced strokes)
// a: from x, y, to x, y. b: radius at each end, kind (0 a neon tube, 1 a dark tinted fill, 3 a drifting dot), color (0 the gradient at the x in c.w, 1 the warm neon, 2 the moon's, 3 a coral). c: seed, light,
// tint, the x whose neon it takes. d: its ink in the print (0 none, 1 the glow inks), 1 if it is the drop's alone.
inline const char* NEON_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
uniform vec4 uView;
uniform float uTime;
uniform float uSway;   // the breeze in the trees
uniform float uCycle;  // the drop's palette cycling (0 the forest's own gradient)
out vec2 vW;
flat out vec4 vA, vB, vC, vD;
flat out vec3 vCol;
// the breeze: a tree bends more the higher up it is (its crown swaying slowly), the floor's life rocks a little
vec2 sway(vec2 p, float cx, float ink) {
    float h = max(p.y - groundY(p.x), 0.0);
    if (ink < 0.5) return vec2(uSway * h * h * 0.0011 * (sin(uTime * 0.55 + cx * 0.37) + 0.35 * sin(uTime * 1.37 + cx + p.y * 0.2)), 0.0);
    return vec2(uSway * min(h, 2.0) * 0.06 * sin(uTime * 0.9 + cx * 1.3 + p.y * 0.8), 0.0);
}
// the drop's gradient at its own place t (0 the HUD's column .. 1 the edge)
vec3 neonT(float x, float t) {
    return x < 0.0 ? (t < 0.5 ? mix(uNeonL[0], uNeonL[1], t * 2.0) : mix(uNeonL[1], uNeonL[2], t * 2.0 - 1.0))
                   : (t < 0.5 ? mix(uNeonR[0], uNeonR[1], t * 2.0) : mix(uNeonR[1], uNeonR[2], t * 2.0 - 1.0));
}
void main() {
    vec2 a = aA.xy, b = aA.zw;
    int kind = int(aB.z + 0.5);
    int mode = int(aB.w + 0.5);
    if (kind == 3) {
        vec2 o = 0.18 * vec2(sin(uTime * 0.5 + aC.x * 7.0), cos(uTime * 0.37 + aC.x * 5.0));
        a += o;
        b += o;
    } else if (mode != 2 && aD.y < 0.5) {
        a += sway(a, aC.w, aD.x);
        b += sway(b, aC.w, aD.x);
    }
    float m = max(aB.x, aB.y) + (kind == 0 || kind == 3 ? 0.2 : 0.03);
    vec2 d = b - a;
    float L = length(d);
    vec2 u = L > 1e-4 ? d / L : vec2(1.0, 0.0), n = vec2(-u.y, u.x);
    vec2 p = mix(a - u * m, b + u * m, aPos.x * 0.5 + 0.5) + n * m * aPos.y;
    vW = p;
    vA = vec4(a, b);
    vB = aB;
    vC = aC;
    vD = aD;
    // the colors cycle through the drop's palette: each tree's neon slides along its side's gradient (back and forth),
    // the warm life swaps between its warm and its coral
    vec3 coral = mix(uNeonWarm, vec3(1.0, 0.1, 0.32), 0.55);
    float sw = 0.5 + 0.5 * sin(uCycle * 12.566 + aC.w * 0.9);
    float t0 = clamp((abs(aC.w) - 11.0) / max(uView.z - 11.0, 1.0), 0.0, 1.0);
    float t = 1.0 - abs(fract((t0 + uCycle * 2.0) * 0.5) * 2.0 - 1.0);
    vCol = mode == 1 ? mix(uNeonWarm, coral, 0.7 * sw * step(0.001, uCycle))
         : mode == 2 ? uNeonMoon
         : mode == 3 ? mix(coral, uNeonWarm, 0.7 * sw * step(0.001, uCycle))
         : neonT(aC.w, t);
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
// A stroke of the forest. On the drop: a neon tube (a near-white core, the saturated color around it, a glow falling
// off beyond) or a dark fill faintly tinted (a trunk's or a cap's inside), both lit by the music's waves. In the print
// (uPrint, written to the plates): a fill is solid key with the bark cut along it, a tube a cut line (a plant's inked).
inline const char* NEON_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vA, vB, vC, vD;
flat in vec3 vCol;
out vec4 frag;
uniform float uAA, uPrint;
uniform vec3 uMoon;
uniform float uFlick, uFlickSeed;   // the backbeat: a few of the tubes flicker
uniform float uSeq, uSeqEnv;        // the floor's life lit band by band out from the well, one band a beat
uniform float uTime;
uniform vec4 uView;
// how bright the drop's neon is here: the waves of light passing, the floor's life lit in turn, a few objects
// flickering on the snare (no swell of the whole forest on the beat)
float dropLight(vec2 p, float cx, float ink) {
    float wv = dropWave(p);
    float band = clamp(floor((abs(cx) - 6.0) / max(uView.z - 6.0, 1.0) * 4.0), 0.0, 3.0);
    float seq = ink > 0.5 && abs(band - uSeq) < 0.5 ? uSeqEnv : 0.0;
    float hf = hash12(vec2(floor(cx * 2.0) + 0.5, uFlickSeed));
    float fl = hf < 0.1 ? uFlick * (0.55 + 0.45 * sin(uTime * 63.0 + hf * 40.0)) : 0.0;
    float k = (0.9 + 1.0 * wv + 0.8 * seq) * (1.0 - 0.3 * fl);
    return mix(1.0, k, uDropK);
}
void main() {
    float h;
    float dist = segT(vW, vA.xy, vA.zw, h);
    float r = mix(vB.x, vB.y, h);
    float dd = dist - r;
    int kind = int(vB.z + 0.5);
    if (uPrint > 0.5) {
        if (kind == 3 || vD.y > 0.5) discard;
        float a = 1.0 - smoothstep(-uAA, uAA, kind == 0 ? dd + 0.008 : dd);
        if (a <= 0.002) discard;
        float cut = 0.0, inked = 0.0;
        if (kind == 0) { cut = 1.0; inked = vD.x; }
        else if (kind == 1 && r > 0.1) {
            // the bark: a few long cuts along the trunk, more on the moon's side
            vec2 d = normalize(vA.zw - vA.xy + vec2(1e-5));
            vec2 q = vec2(dot(vW - vA.xy, d), dot(vW - vA.xy, vec2(-d.y, d.x)));
            float side = q.y / max(r, 1e-3) * sign(uMoon.x - vA.x);
            cut = gouge(vec2(q.x, q.y), 0.11, 2.5, 0.25 * smoothstep(-0.2, 1.0, side) + 0.1, vC.w * 3.1, uAA);
        }
        // (the HUD's columns and its text stay solid ink)
        a *= 1.0 - max(textQuietAt(vW), columnsAt(vW));
        frag = vec4(cut, inked * 0.8, inked * 0.5, 1.0) * a;
        return;
    }
    // (the trees stand on the ground: what of their feet reaches under it fades into the floor's glow)
    float foot = vD.x < 0.5 ? smoothstep(-0.45, 0.05, vW.y - groundY(vW.x)) : 1.0;
    if (kind == 1) {
        float a = (1.0 - smoothstep(-uAA, uAA, dd)) * foot;
        if (a <= 0.002) discard;
        // a trunk's inside lights up as the waves climb it
        float lit = 1.0 + uDropK * 1.4 * dropWave(vW);
        frag = vec4(vCol * vC.z * lit * a, a * 0.9);
        return;
    }
    float core = 1.0 - smoothstep(-uAA, uAA, dd);
    float hot = 1.0 - smoothstep(0.0, max(r * 0.6, uAA), dist);
    float halo = exp(-max(dd, 0.0) / 0.06);
    float L = dropLight(vW, vC.w, vD.x);
    // (the halo widens a little with the light)
    halo = exp(-max(dd, 0.0) / (0.06 * (0.8 + 0.4 * L)));
    vec3 c = (vCol * (core * 1.2 + halo * 0.4) + vec3(0.6) * hot * core) * vC.y * L * foot;
    if (max(c.r, max(c.g, c.b)) < 0.002) discard;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the blocks (instanced per cell, in register)
// a: x, y, half, kind (0 block, 1 ghost, 2 clearing, 3 going out). b: noise origin x, y, seed, pattern.
// c: color (linear), alpha. d: progress, -, -, -. The quad reaches a little past the cell, for its glow.
inline const char* PIECE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 l = aPos * 0.62;
    vLocal = l;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(aA.xy + l * 2.0 * aA.z, 0.0, 1.0);
}
)";

// A block: a neon-tube square, its rim a bright line in the piece's color (a near-white core, a tight glow just
// outside), its inside dark and faintly tinted, the type's pattern drawn in it in thin neon lines (I rows, O a square
// within, T chevrons, S and Z diagonals both ways, J dots, L columns). The ghost is the same tube, unlit.
inline const char* PIECE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform vec3 uKey, uBridgeInk;
uniform float uBridge, uFade, uDim;
float sdRound(vec2 p, float b, float r) { vec2 q = abs(p) - (b - r); return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    float aa = max(length(fwidth(p)) * 0.7, 1e-4);
    vec2 nw = vB.xy + p;
    float seed = vB.z;
    vec3 col = vC.rgb;
    float d = sdRound(p, 0.42, 0.05);
    float inside = 1.0 - smoothstep(-aa, aa, d + 0.02);
    // the tube along the rim, and its glow
    float rimD = abs(d + 0.035);
    float rim = 1.0 - smoothstep(0.03, 0.03 + aa * 1.5, rimD);
    float core = 1.0 - smoothstep(0.006, 0.006 + aa * 1.5, rimD);
    float glow = exp(-max(rimD - 0.02, 0.0) / 0.045);
    // the pattern (in the piece's frame, so that it runs on across its cells), thin neon lines inside
    int pat = int(vB.w + 0.5);
    float line;
    vec2 q = p;
    if (pat == 0) line = abs(fract(q.y * 3.0 + 0.5) - 0.5) / 3.0;
    else if (pat == 1) line = abs(max(abs(q.x), abs(q.y)) - 0.19);
    else if (pat == 2) line = abs(fract((q.y + abs(q.x) * 0.9) * 2.6) - 0.5) / 2.6;
    else if (pat == 3) line = abs(fract((nw.x + nw.y) * 2.4) - 0.5) / 2.4 * 0.707;
    else if (pat == 4) line = abs(fract((nw.x - nw.y) * 2.4) - 0.5) / 2.4 * 0.707;
    else if (pat == 5) { vec2 f = fract(p * 3.0 + 0.5) - 0.5; line = length(f) / 3.0 - 0.018; }
    else line = abs(fract(q.x * 3.0 + 0.5) - 0.5) / 3.0;
    float pin = 1.0 - smoothstep(-0.07, -0.05, d);
    float pl = (1.0 - smoothstep(0.016, 0.016 + aa * 1.5, line)) * pin;
    float pg = exp(-max(line - 0.01, 0.0) / 0.03) * pin;
    // lit: 1 a block, the ghost unlit
    float lit = kind == 1 ? 0.28 : 1.0;
    float a = vC.w;
    vec3 tube = col * 1.35 + vec3(0.5) * core;
    vec3 c = col * 0.12 * inside;
    float alpha = inside * 0.92;
    c = mix(c, tube * lit, rim);
    alpha = max(alpha, rim);
    c += col * glow * 0.45 * lit * (1.0 - rim);
    c = mix(c, (col * 1.1 + vec3(0.12)) * lit, pl * 0.9);
    c += col * pg * 0.18 * lit * (1.0 - pl);
    if (kind == 1) {
        // the ghost: dark, its rim and pattern dim, a little less opaque
        alpha *= 0.75;
    }
    if (kind == 2) {
        // cleared: the tube flares white and goes out in strokes, row by row of its pattern
        float t = vD.x;
        float row = floor((p.y + 0.5) * 5.0);
        float h = hash12(vec2(row, seed * 13.0));
        float s = (p.x + 0.5) * 0.6 + h * 0.4;
        float k = step(t * 1.45 - 0.05, s * 0.9 + 0.1 * vnoise(nw * 6.0));
        a *= k;
        c *= k;
        c = mix(c, vec3(1.0) * (inside + glow * 0.5), 0.5 * (1.0 - t) * smoothstep(0.0, 0.2, t));
    }
    if (kind == 3) {
        // the game is over: the tube goes out from the top, flickering down to a dark cell
        float t = vD.x;
        float s = (0.5 - p.y) * 0.6 + 0.4 * vnoise(vec2(nw.x * 2.0, seed * 7.0));
        float off = smoothstep(s - 0.1, s + 0.05, (t - 0.2) * 1.6);
        c = mix(c, col * 0.05 * inside + col * 0.15 * rim, off);
    }
    // a premultiplied color: the glow beyond the rim adds light (no alpha), the cell covers what is behind it
    a *= max(alpha, 0.0);
    vec3 outc = mix(c, uBridgeInk * (inside + rim), uBridge) * uFade * (1.0 - uDim * 0.5) * vC.w;
    if (dot(outc, vec3(1.0)) + a <= 0.002) discard;
    frag = vec4(pow(max(outc, vec3(0.0)), vec3(1.0 / 2.2)), a);
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

// Builds a shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace forest_shaders

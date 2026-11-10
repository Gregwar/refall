#pragma once
// LOST PANDA's shaders: the world (eight places along the way home, side by side on one long road, drawn as
// illustrated parallax layers), the little panda and its family (soft shapes with clean outlines), the board's
// cells, the lights, the lettering.
//
// The road: a coordinate j along the way (board units at the ground's depth). Every place is LEN long; the next one
// takes over across the last BL units of the one before (a soft crossfade drifting by with the walk). Each place
// draws its own layers at their own parallax (a layer at parallax k scrolls by k times the walk).

namespace lostpanda_shaders {

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
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
float sdEll(vec2 p, vec2 r) { float k = length(p / r); return (k - 1.0) * min(r.x, r.y) * (k < 1.0 ? 1.0 : min(1.0, k)); }
)";

// ------------------------------------------------------------------------------------------------ the world
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform float uPxP;          // board units per pixel of this target
uniform float uD;            // the walk: where the camera is along the way
uniform float uXP;           // the panda's place on the screen (board x)
uniform float uTime, uBeat, uPeak, uDrop, uKick, uRain, uStars, uClouds, uOver, uWorldA, uSlow, uMid;
uniform vec3 uSkyTop, uSkyHor, uFog, uSunCol, uBridgeCol;
uniform vec4 uSun;           // x, y, radius, strength
uniform float uTrain;        // the subway train's place along its track (board units, its layer)
uniform float uLamp;         // the night lights' level (lamps, windows, neon)

const float GY = -7.4;       // the ground line (the panda's feet)
const float LEN = 48.0, BL = 10.0;
const float PI = 3.14159265, TAU = 6.2831853;
float gPx = 0.02;
float gGlow = 0.0;           // the light of lamps at the pixel (it lights the rain)
vec3 gGlowC = vec3(1.0);

float fillA(float d) { return 1.0 - smoothstep(-gPx, gPx, d); }
float fillS(float d, float s) { return 1.0 - smoothstep(-s, s, d); }
float lod(float cell) { return smoothstep(2.0, 5.0, cell / gPx); }
float bump(float x, float w) { return exp(-x * x / (w * w)); }
vec3 lay(vec3 c, vec3 col, float a) { return mix(c, col, sat(a)); }
// haze toward the fog with the distance (0 near .. 1 lost in it)
vec3 haze(vec3 c, float h) { return mix(c, uFog, h); }

// ---------------------------------------------------------------- the sky (shared: its colors follow the walk)
vec3 sky(vec2 p) {
    float h = sat((p.y - GY) / (12.0 - GY));
    vec3 c = mix(uSkyHor, uSkyTop, pow(h, 0.65));
    if (uStars > 0.0) {
        vec2 q = p * 1.7 + vec2(uD * 0.01, 0.0);
        vec2 i = floor(q), f = fract(q) - 0.5;
        vec2 o = hash2(i) - 0.5;
        float on = step(0.86, hash(i + 3.1));
        float d = length(f - o * 0.6);
        float tw = 0.6 + 0.4 * sin(uTime * (0.7 + hash(i + 9.0)) + hash(i) * 20.0);
        c += vec3(0.9, 0.92, 1.0) * on * exp(-d * d * 500.0) * uStars * smoothstep(0.25, 0.8, h) * tw * 0.8;
    }
    if (uSun.w > 0.0) {
        float r = length(p - uSun.xy);
        c += uSunCol * (exp(-r * r / sq(uSun.z * 3.0)) * 0.45 + exp(-r / (uSun.z * 6.0)) * 0.35) * uSun.w;
        c = mix(c, uSunCol * 1.5 + 0.2, fillS(r - uSun.z, gPx * 1.5) * uSun.w * 0.9);
    }
    if (uClouds > 0.0) {
        // puffy clouds, flat bottomed, lit from the top, drifting slowly
        for (int i = 0; i < 2; i++) {
            float fi = float(i);
            float x = p.x + uD * (0.03 + 0.03 * fi) + uTime * (0.1 + 0.06 * fi) + fi * 37.0;
            float cw = 15.0 - fi * 4.0;
            float id = floor(x / cw);
            if (hash1(id + fi * 9.0) > 0.7) continue;
            vec2 ctr = vec2((id + 0.5 + (hash1(id + 2.0) - 0.5) * 0.4) * cw, 3.0 + 4.0 * fi + 2.5 * hash1(id + 4.0));
            float sc = (0.8 + 0.5 * hash1(id + 6.0)) * (1.0 - 0.3 * fi);
            vec2 q = (vec2(x, p.y) - ctr) / sc;
            float d = 1e3;
            for (int k = 0; k < 5; k++) {
                float fk = float(k);
                float mid = 2.0 - abs(fk - 2.0);
                vec2 o = vec2((fk - 2.0) * 1.05, 0.25 * sin(fk * 2.3 + id) + mid * 0.42);
                d = min(d, length(q - o) - (0.75 + 0.35 * mid + 0.2 * hash1(id + fk)));
            }
            d = max(d, -(q.y + 0.55));
            float a = fillS(d * sc, 0.06 + gPx) * uClouds * (0.92 - 0.25 * fi);
            vec3 lit = mix(mix(uSkyHor, uSkyTop, 0.35) * 0.95, mix(uSunCol, vec3(1.0), 0.55) * 1.05, smoothstep(-0.7, 1.5, q.y));
            c = mix(c, lit, a);
        }
    }
    return c;
}

// ---------------------------------------------------------------- shared pieces of scenery

// lit windows: a grid in local coords q (cell size, window half size), returns the light (0..1, faded with lod)
float windowsLit(vec2 q, vec2 cell, vec2 half, float seed, float prob, out float glass) {
    vec2 g = q / cell;
    vec2 gi = floor(g), gf = (fract(g) - 0.5) * cell;
    float d = sdBox(gf, half);
    float w = fillA(d);
    float lit = step(hash(gi + seed), prob) * (0.55 + 0.45 * hash(gi + seed + 4.7));
    float l = lod(min(cell.x, cell.y));
    glass = mix(half.x * half.y * 4.0 / (cell.x * cell.y), w, l);
    return mix(prob * 0.75 * glass, w * lit, l);
}

// a far skyline of towers in layer k: their lit windows sparse
vec3 skyline(vec2 p, vec3 c, float k, float base, float hmin, float hmax, float cw, vec3 bc, float prob, float seed, float hz) {
    float u = p.x + uD * k + seed * 13.0;
    float id = floor(u / cw);
    float lx = (u / cw - id) * cw;
    float h = hmin + (hmax - hmin) * pow(hash1(id + seed), 1.4);
    float w = cw * (0.62 + 0.3 * hash1(id * 1.3 + seed));
    float x0 = 0.2 * cw * hash1(id + 5.0);
    float top = base + h;
    float d = max(abs(lx - x0 - w * 0.5) - w * 0.5, p.y - top);
    // a stepped crown on some
    if (hash1(id + 11.0) > 0.6) d = min(d, max(abs(lx - x0 - w * 0.5) - w * 0.25, p.y - top - 0.9));
    float a = fillA(d);
    if (a <= 0.0) return c;
    vec3 b = bc * (0.85 + 0.3 * hash1(id + 2.0));
    b *= 0.9 + 0.25 * sat((p.y - base) / max(h, 1.0));
    float gl;
    float lit = windowsLit(vec2(lx - x0, p.y - base), vec2(0.36, 0.5), vec2(0.1, 0.13), id * 7.3 + seed, prob, gl);
    b += vec3(1.0, 0.68, 0.36) * lit * 0.55 * uLamp;
    return mix(c, haze(b, hz), a);
}

// a street lamp (layer k, every `sp` units): returns color over c, adds its light to gGlow
vec3 lamps(vec2 p, vec3 c, float k, float sp, float off, vec3 lc, float hgt, float on) {
    float u = p.x + uD * k - off;
    float n = floor(u / sp + 0.5);
    float lx = u - n * sp;
    vec2 head = vec2(0.7, GY + hgt);
    // the pole and its arm
    float d = sdBox(vec2(lx, p.y - (GY + hgt * 0.5)), vec2(0.07, hgt * 0.5));
    d = min(d, sdSeg(vec2(lx, p.y), vec2(0.0, GY + hgt - 0.1), vec2(0.35, GY + hgt + 0.32)) - 0.05);
    d = min(d, sdSeg(vec2(lx, p.y), vec2(0.35, GY + hgt + 0.32), head + vec2(0.0, 0.2)) - 0.05);
    d = min(d, sdBox(vec2(lx, p.y - (GY + 0.25)), vec2(0.13, 0.25)));
    vec3 pole = vec3(0.03, 0.035, 0.05) + lc * 0.04 * on;
    // the lamp's head
    float dh = sdBox(vec2(lx - head.x, p.y - head.y - 0.12), vec2(0.28 - (p.y - head.y) * 0.25, 0.1));
    c = lay(c, pole, fillA(min(d, dh)));
    vec2 lp = vec2(lx - head.x, p.y - head.y);
    float r = length(lp * vec2(1.0, 1.3));
    // the glass, the halo, the cone of light down to the ground
    float glass = fillA(sdEll(lp + vec2(0.0, 0.02), vec2(0.22, 0.09)));
    float halo = exp(-r * r / 0.5) * 0.9 + exp(-r / 1.8) * 0.35;
    float coneX = abs(lp.x) / max(0.2, -lp.y * 0.55 + 0.25);
    float cone = (1.0 - smoothstep(0.6, 1.0, coneX)) * step(lp.y, 0.0) * smoothstep(GY - 0.5, GY + 1.5, p.y) * 0.12 * exp(lp.y * 0.08);
    float g = (halo + cone) * on;
    c += lc * g;
    c = mix(c, lc * 2.2 + 0.3, glass * on);
    gGlow += g; gGlowC = lc;
    return c;
}
// the lamps' light seen in a wet ground: mirrored under the line y0 and stretched, rippled
float lampReflect(vec2 p, float k, float sp, float off, float hgt, float y0) {
    float u = p.x + uD * k - off + 0.04 * sin(p.y * 9.0 + uTime * 2.3) + 0.03 * sin(p.y * 23.0 - uTime * 3.1);
    float n = floor(u / sp + 0.5);
    float lx = u - n * sp - 0.7;
    float my = (y0 - p.y);
    float streak = exp(-lx * lx / 0.08) * exp(-my / 3.2) * 0.55;
    float pool = exp(-lx * lx / 3.0) * exp(-my * 1.2) * 0.25;
    return streak + pool;
}

// rain: thin slanted streaks falling in three depths; lit by the lamps' light at the pixel
vec3 rain(vec2 p, vec3 c, float amt) {
    if (amt <= 0.0) return c;
    float acc = 0.0;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float sc = 1.0 + fi * 0.55;
        vec2 q = p;
        q.x += q.y * 0.12 + uD * (0.6 + 0.3 * fi);
        float cw = 0.33 * sc;
        float ci = floor(q.x / cw);
        float fx = q.x / cw - ci - 0.5;
        float h = hash1(ci * 1.37 + fi * 11.0);
        float len = 0.9 * sc, per = 5.5 * sc + h * 3.0;
        float y = q.y + uTime * (16.0 + 5.0 * fi) * (1.0 - 0.6 * uSlow) + h * 97.0;
        float fy = mod(y, per) / len;
        float s = smoothstep(0.0, 0.25, fy) * (1.0 - smoothstep(0.7, 1.0, fy));
        float wid = max(0.012 * sc, gPx * 0.7);
        float xo = (h - 0.5) * 0.6;
        float line = 1.0 - smoothstep(wid, wid + gPx * 1.2, abs(fx - xo) * cw);
        acc += line * s * (0.45 + 0.25 * fi) * step(hash1(ci + fi * 5.0), 0.75 * amt + 0.1);
    }
    vec3 rc = mix(uFog * 1.6 + 0.04, gGlowC * 1.4, sat(gGlow * 1.2));
    return c + rc * acc * amt * (0.25 + 1.4 * sat(gGlow));
}

// ripples of rain in puddles (ground coords)
float puddleRings(vec2 p, float k) {
    vec2 q = vec2(p.x + uD * k, (p.y - GY) * 3.2);
    float acc = 0.0;
    for (int i = 0; i < 2; i++) {
        vec2 g = q / 1.1 + float(i) * 0.5;
        vec2 gi = floor(g), gf = fract(g) - 0.5;
        float h = hash(gi + float(i) * 7.0);
        float t = fract(uTime * 0.7 + h);
        vec2 o = (hash2(gi) - 0.5) * 0.5;
        float r = length(gf - o) * 1.1;
        float rr = t * 0.45;
        acc += (1.0 - smoothstep(0.0, 0.03 + gPx, abs(r - rr))) * (1.0 - t) * 0.6;
    }
    return acc;
}

// out of focus lights drifting in front (a layer at parallax 1.5)
vec3 bokeh(vec2 p, vec3 c, float amt, vec3 c1, vec3 c2) {
    if (amt <= 0.0) return c;
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        vec2 q = vec2(p.x + uD * (1.4 + 0.3 * fi) + fi * 3.0, p.y);
        float cw = 6.5;
        float id = floor(q.x / cw);
        float h = hash1(id + fi * 31.0);
        if (h > 0.55) continue;
        vec2 ctr = vec2((id + 0.2 + 0.6 * hash1(id + 3.0)) * cw, mix(-10.0, 9.0, hash1(id + 7.0)) + sin(uTime * 0.2 + h * 9.0) * 0.4);
        float r = 0.7 + 1.1 * hash1(id + 9.0);
        float d = length(q - ctr) - r;
        float disc = 1.0 - smoothstep(-0.15, 0.05, d);
        float rim = exp(-sq(d / 0.08)) * 0.5;
        vec3 bc = mix(c1, c2, step(0.5, hash1(id + 13.0)));
        c += bc * (disc * 0.1 + rim * 0.06) * amt;
    }
    return c;
}

// ---------------------------------------------------------------- the facades across the street (layer k)
// shop: how lit the shop windows are; neon: neon signs
vec3 facades(vec2 p, vec3 c, float k, float base, float shop, float neon, float prob, float hz) {
    float u = p.x + uD * k + 21.0;
    float cw = 6.2;
    float id = floor(u / cw);
    float lx = (u / cw - id) * cw;
    float h1 = hash1(id * 3.1);
    float top = base + 6.0 + 7.0 * h1;
    float bw = cw - 0.35;
    vec3 bc = mix(vec3(0.03, 0.04, 0.07), vec3(0.06, 0.04, 0.045), hash1(id + 1.0));
    bc = mix(bc, vec3(0.035, 0.06, 0.065), step(0.7, hash1(id + 12.0)));
    bc *= 0.9 + 0.2 * hash1(id + 4.0);
    // the gap between two buildings: a dark alley
    float inB = fillA(max(abs(lx - bw * 0.5) - bw * 0.5, p.y - top));
    float roof = fillA(max(abs(lx - bw * 0.5) - bw * 0.5 - 0.12, abs(p.y - top + 0.1) - 0.12));
    if (inB + roof <= 0.0) return c;
    vec3 b = bc * (0.85 + 0.35 * sat((p.y - base) / 10.0));
    b = mix(b, bc * 1.6, roof);
    // windows on the floors above the shop
    float gl;
    float wl = windowsLit(vec2(lx - 0.45, p.y - base - 3.2), vec2(1.32, 1.75), vec2(0.34, 0.5), id * 3.7, prob, gl);
    gl *= step(base + 3.0, p.y) * step(p.y, top - 0.7) * step(0.3, lx) * step(lx, bw - 0.3);
    wl *= step(base + 3.0, p.y) * step(p.y, top - 0.7) * step(0.3, lx) * step(lx, bw - 0.3);
    vec3 warm = mix(vec3(1.0, 0.62, 0.3), vec3(1.0, 0.82, 0.55), hash1(id + 8.0));
    b = mix(b, mix(uFog * 0.5, b * 1.2, 0.5), gl * 0.5);
    b += warm * wl * 0.8 * uLamp;
    // the shop on the ground floor: a window glowing, an awning
    float sx = lx - bw * 0.5;
    float shopW = fillA(sdBox(vec2(sx, p.y - base - 1.15), vec2(bw * 0.38, 1.05)));
    float doorW = fillA(sdBox(vec2(sx + bw * 0.43, p.y - base - 1.0), vec2(0.0, 1.0)));
    vec3 shopC = mix(vec3(1.0, 0.75, 0.45), vec3(0.75, 0.9, 1.0), step(0.6, hash1(id + 21.0)));
    float slat = 0.8 + 0.2 * smoothstep(0.3, 0.5, abs(fract((p.y - base) * 4.0) - 0.5)) * lod(0.25);
    float lit = shop * step(0.3, hash1(id + 17.0));
    // closed: a steel shutter catching a little light; open: a warm window, shelves inside
    vec3 shutter = vec3(0.05, 0.055, 0.07) * slat + uFog * 0.15;
    float shelves = 0.7 + 0.3 * step(0.5, fract((p.y - base) * 1.6));
    vec3 window = shopC * shelves * (0.4 + 0.6 * uLamp) * 0.9;
    b = mix(b, mix(shutter, window, lit), shopW);
    gGlow += lit * shopW * 0.3;
    // the awning, striped
    float aw = fillA(sdBox(vec2(sx, p.y - base - 2.45), vec2(bw * 0.44, 0.22)));
    vec3 awC = mix(vec3(0.35, 0.08, 0.08), vec3(0.08, 0.2, 0.22), step(0.5, hash1(id + 31.0)));
    awC = mix(awC, awC * 1.8 + 0.03, step(0.5, fract(sx * 1.5)));
    b = mix(b, awC * (0.5 + 0.5 * uLamp), aw);
    // neon: a vertical sign beside the shop, glowing outlines
    if (neon > 0.0 && hash1(id + 41.0) < 0.7) {
        vec3 nc = hash1(id + 43.0) < 0.5 ? vec3(1.0, 0.25, 0.55) : (hash1(id + 44.0) < 0.5 ? vec3(0.25, 0.85, 1.0) : vec3(1.0, 0.65, 0.2));
        vec2 nq = vec2(lx - bw + 0.65, p.y - base - 5.0 - 2.5 * hash1(id + 47.0));
        float sign = sdBox(nq, vec2(0.38, 1.4));
        float tube = 1.0 - smoothstep(0.03, 0.03 + gPx * 1.5, abs(sign + 0.1));
        // letters: little bars of light down the sign
        float letters = (1.0 - smoothstep(0.04, 0.04 + gPx * 1.5, abs(nq.x))) * step(0.35, fract(nq.y * 1.1)) * step(abs(nq.y), 1.2);
        float flick = 0.85 + 0.15 * sin(uTime * 0.8 + id);
        b = mix(b, vec3(0.02), fillA(sign) * 0.8);
        float glow = exp(-max(sign, 0.0) * 1.6) * 0.35;
        b += nc * (tube + letters + glow) * neon * flick * (0.6 + 0.4 * uLamp);
        gGlow += glow * neon * 0.8; gGlowC = nc;
    }
    return mix(c, haze(b, hz), sat(inB + roof));
}

// ---------------------------------------------------------------- passers-by under umbrellas (layer k)
vec3 passers(vec2 p, vec3 c, float k, float amt, vec3 rim) {
    if (amt <= 0.0) return c;
    for (int g = 0; g < 2; g++) {
        float dir = g == 0 ? 1.0 : -1.0;
        float speed = 1.05 + 0.2 * float(g);
        float u = p.x + uD * k - dir * uTime * speed * (1.0 - 0.6 * uSlow) + float(g) * 4.3;
        float cw = 8.5;
        float id = floor(u / cw);
        float h = hash1(id + float(g) * 17.0);
        if (h > amt) continue;
        float sx = (u / cw - id) * cw - cw * 0.5 - (hash1(id + 3.0) - 0.5) * 3.0;
        vec2 q = vec2(sx * dir, p.y - GY - 0.1);
        float s = 0.9 + 0.25 * hash1(id + 5.0);
        q /= s;
        // walk cycle
        float ph = (uTime * speed * (1.0 - 0.6 * uSlow) * 2.0 + h * 10.0) * 1.1;
        float sw = sin(ph) * 0.45;
        float d = 1e3;
        // legs
        d = min(d, sdSeg(q, vec2(0.0, 3.6), vec2(sw * 0.9, 0.15)) - 0.17);
        d = min(d, sdSeg(q, vec2(0.0, 3.6), vec2(-sw * 0.9, 0.15)) - 0.17);
        // coat
        float coat = sdBox(q - vec2(0.0, 5.3), vec2(0.62 - (q.y - 5.3) * 0.12, 1.75)) - 0.12;
        d = min(d, coat);
        // head, arm up to the umbrella
        d = min(d, length(q - vec2(0.15, 7.55)) - 0.42);
        d = min(d, sdSeg(q, vec2(0.25, 6.6), vec2(0.45, 7.3)) - 0.13);
        // the umbrella: a dome and its handle
        vec2 uq = q - vec2(0.45, 8.6);
        float dome = max(length(uq * vec2(1.0, 1.55)) - 1.95, -uq.y);
        float handle = sdBox(uq - vec2(0.0, -0.7), vec2(0.035, 0.7));
        float um = min(dome, handle);
        vec3 uc = hash1(id + 9.0) < 0.35 ? vec3(0.45, 0.06, 0.08) : (hash1(id + 10.0) < 0.5 ? vec3(0.04, 0.05, 0.07) : vec3(0.1, 0.16, 0.22));
        float body = fillA(d * s);
        vec3 bc = vec3(0.022, 0.024, 0.032) + rim * 0.06 * smoothstep(-0.25, 0.0, d) ;
        c = lay(c, bc, body);
        float ua = fillA(um * s);
        vec3 ucc = uc * (0.7 + 0.5 * smoothstep(-0.6, 0.6, uq.y)) + rim * 0.08 * exp(-abs(dome) * 6.0);
        c = lay(c, ucc, ua);
    }
    return c;
}

// =========================================================================== 0. a rainy street at night
vec3 stStreet(vec2 p, vec3 c, float crossing) {
    c = skyline(p, c, 0.12, GY + 2.0, 4.0, 15.0, 2.9, vec3(0.03, 0.04, 0.07), 0.1, 3.0, 0.35);
    c = facades(p, c, 0.42, GY + 1.0, 0.25 + 0.75 * crossing, crossing, 0.3 + 0.1 * crossing, 0.12);
    // the road: a dark band, wet, the facades' light in it
    if (p.y < GY + 1.0) {
        float my = GY + 1.0 - p.y;
        vec3 road = mix(vec3(0.03, 0.035, 0.05), vec3(0.015, 0.018, 0.028), sat(my / 1.0));
        road += uFog * 0.3 * exp(-my * 2.0);
        float refl = lampReflect(p, 0.42, 6.2, 0.0, 3.0, GY + 1.0) * 0.4;
        road += vec3(1.0, 0.7, 0.4) * refl * uLamp * 0.5;
        // a zebra crossing on the shopping street
        if (crossing > 0.0) {
            float u = p.x + uD * 0.75;
            float cx = mod(u, 34.0) - 17.0;
            float z = step(abs(cx), 4.0) * step(0.5, fract(my * 4.5 / (0.6 + my)));
            road = mix(road, vec3(0.25, 0.26, 0.3) * (0.6 + 0.4 * uLamp), z * crossing * 0.8 * fillA(abs(cx) - 4.0));
        }
        c = mix(c, road, fillA(p.y - (GY + 1.0)));
    }
    // a traffic light by the crossing
    if (crossing > 0.0) {
        float u = p.x + uD - 6.0;
        float lx = mod(u, 34.0) - 17.0 + 5.2;
        float d = min(sdBox(vec2(lx, p.y - GY - 2.6), vec2(0.07, 2.6)), sdBox(vec2(lx, p.y - GY - 5.6), vec2(0.32, 0.65)) - 0.06);
        c = lay(c, vec3(0.025, 0.028, 0.035), fillA(d) * crossing);
        float cyc = fract(uTime / 14.0);
        float red = step(cyc, 0.55), grn = 1.0 - red;
        vec2 r1 = vec2(lx, p.y - GY - 5.9), r2 = vec2(lx, p.y - GY - 5.3);
        vec3 rc = vec3(1.0, 0.18, 0.12), gc = vec3(0.2, 1.0, 0.6);
        c += rc * red * (fillA(length(r1) - 0.17) * 1.5 + exp(-dot(r1, r1) / 0.4) * 0.6) * crossing;
        c += gc * grn * (fillA(length(r2) - 0.17) * 1.5 + exp(-dot(r2, r2) / 0.4) * 0.6) * crossing;
        gGlow += (red * exp(-dot(r1, r1) / 1.2) + grn * exp(-dot(r2, r2) / 1.2)) * 0.5 * crossing;
    }
    // people under umbrellas hurrying by (the shopping street)
    c = passers(p, c, 0.8, 0.75 * crossing, vec3(1.0, 0.6, 0.5));
    // the street lamps
    c = lamps(p, c, 1.0, 15.0, 4.0, vec3(1.0, 0.72, 0.38), 6.6, uLamp);
    // the pavement: wet stone, the lamps and the sky in it, puddles rippling with rain
    if (p.y < GY + 0.1) {
        float my = GY - p.y;
        vec3 pv = mix(vec3(0.05, 0.055, 0.07), vec3(0.025, 0.028, 0.038), sat(my / 3.5));
        // the curb
        float curb = fillA(abs(p.y - GY + 0.06) - 0.06);
        float pud = smoothstep(0.55, 0.62, fbm(vec2((p.x + uD) * 0.25, my * 1.4)));
        float refl = lampReflect(p, 1.0, 15.0, 4.0, 6.6, GY) * (0.35 + 0.65 * pud) * uLamp;
        pv += vec3(1.0, 0.72, 0.4) * refl;
        pv += mix(uSkyHor, uSkyTop, 0.3) * pud * 0.4;
        pv += vec3(0.6, 0.7, 0.8) * puddleRings(p, 1.0) * smoothstep(0.62, 0.7, fbm(vec2((p.x + uD) * 0.25, my * 1.4))) * 0.12 * uRain;
        pv = mix(pv, vec3(0.09, 0.09, 0.11), curb);
        c = mix(c, pv, fillA(p.y - GY));
    }
    return c;
}

// =========================================================================== 2. the subway station
vec3 stStation(vec2 p, vec3 c) {
    // the wall across the tracks: glazed tiles under cool light
    float u = p.x + uD * 0.6;
    float wallTop = GY + 10.5;
    vec3 tile = vec3(0.13, 0.16, 0.15);
    vec2 tq = vec2(u / 0.9, (p.y - GY) / 0.45);
    vec2 tf = abs(fract(tq + vec2(0.5 * step(0.5, fract(tq.y * 0.5)), 0.0)) - 0.5);
    float grout = (1.0 - smoothstep(0.43, 0.48, max(tf.x * 0.9 / 0.45 * 0.5, tf.y))) ;
    float gl = lod(0.45);
    vec3 w = tile * mix(0.93, mix(0.82, 1.0, grout), gl);
    w *= 0.9 + 0.1 * vnoise(tq * vec2(1.0, 0.5));
    // a band of color along the wall, the station's name in it
    float band = fillA(abs(p.y - GY - 4.4) - 0.5);
    w = mix(w, vec3(0.02, 0.07, 0.06), band);
    float nx = mod(u, 26.0) - 13.0;
    float nameB = fillA(sdBox(vec2(nx, p.y - GY - 6.4), vec2(3.0, 0.65)) - 0.05);
    w = mix(w, vec3(0.03, 0.05, 0.12), nameB);
    float letters = fillA(sdBox(vec2(fract(nx * 0.9) - 0.5, p.y - GY - 6.4), vec2(0.24, 0.3))) * step(abs(nx), 2.4) * nameB;
    w = mix(w, vec3(0.85, 0.88, 0.95), letters * 0.9);
    // posters, framed, their pictures soft
    float px = mod(u + 9.0, 13.0) - 6.5;
    vec2 pq = vec2(px, p.y - GY - 2.1);
    float frame = fillA(sdBox(pq, vec2(1.25, 1.55)));
    float pid = floor((u + 9.0) / 13.0);
    vec3 pc1 = 0.5 + 0.5 * cos(TAU * (hash1(pid) + vec3(0.0, 0.33, 0.67)));
    vec3 pic = mix(pc1 * 0.5, (0.5 + 0.5 * cos(TAU * (hash1(pid + 3.0) + vec3(0.1, 0.4, 0.7)))) * 0.7, smoothstep(-1.0, 1.0, pq.y + sin(pq.x * 2.0 + pid) * 0.5));
    pic += 0.25 * fillA(length(pq - vec2(0.3 * sin(pid), 0.4)) - 0.55);
    w = mix(w, vec3(0.05), frame);
    w = mix(w, pic * 0.35, fillA(sdBox(pq, vec2(1.1, 1.4))));
    // the ceiling, the strip lights
    float ceil = smoothstep(wallTop - 0.05, wallTop + 0.05, p.y);
    vec3 cl = mix(vec3(0.03, 0.035, 0.035), vec3(0.008, 0.01, 0.01), sat((p.y - wallTop) / 4.0));
    w = mix(w, cl, ceil);
    float lu = p.x + uD * 0.7;
    float lx = mod(lu, 8.0) - 4.0;
    float tube = fillA(sdBox(vec2(lx, p.y - wallTop - 0.5), vec2(2.2, 0.12)));
    vec3 lc = vec3(0.85, 1.0, 0.92);
    w = mix(w, lc * 1.8, tube);
    float tg = exp(-sq(sdBox(vec2(lx, p.y - wallTop - 0.5), vec2(2.2, 0.0)) / 0.5)) * 0.25;
    w += lc * tg * 0.5;
    // the light falling down the wall from the strips
    w *= 0.55 + 0.6 * exp(-sq(lx / 3.5)) * smoothstep(GY, wallTop, p.y);
    c = w;
    // the track pit
    if (p.y < GY + 1.2) {
        float pit = sat((GY + 1.2 - p.y) / 1.0);
        vec3 tc = vec3(0.02, 0.022, 0.025);
        float rail = fillA(abs(p.y - GY - 0.55) - 0.04) + fillA(abs(p.y - GY - 0.3) - 0.04);
        tc += vec3(0.5, 0.55, 0.6) * rail * 0.25;
        c = mix(c, tc, fillA(p.y - GY - 1.2));
    }
    // the train: it glides in, waits with its doors open, leaves
    {
        float tx = p.x + uD * 0.75 - uTrain;
        float carL = 14.0;
        float ci = floor(tx / carL);
        float lx2 = tx - ci * carL;
        float onTrain = step(0.0, tx) * step(tx, carL * 6.0);
        if (onTrain > 0.0) {
            float body = sdBox(vec2(lx2 - carL * 0.5, p.y - GY - 4.0), vec2(carL * 0.5 - 0.15, 3.6)) - 0.3;
            vec3 tcol = vec3(0.12, 0.14, 0.16) * (0.8 + 0.4 * smoothstep(GY, GY + 7.0, p.y));
            float stripe = fillA(abs(p.y - GY - 2.3) - 0.25);
            tcol = mix(tcol, vec3(0.5, 0.08, 0.08), stripe);
            float wx = mod(lx2 - 1.0, 3.0) - 1.5;
            float win = fillA(sdBox(vec2(wx, p.y - GY - 5.0), vec2(0.9, 0.85)) - 0.15) * step(1.0, lx2) * step(lx2, carL - 1.0);
            vec3 inside = vec3(1.0, 0.86, 0.62) * 0.9;
            // people in the windows, a silhouette here and there
            float pid = floor((lx2 - 1.0) / 3.0) + ci * 7.0;
            float pers = fillA(length(vec2(wx - 0.3, p.y - GY - 5.25)) - 0.32) + fillA(sdBox(vec2(wx - 0.3, p.y - GY - 4.3), vec2(0.45, 0.6)));
            inside = mix(inside, vec3(0.1, 0.08, 0.07), sat(pers) * step(hash1(pid), 0.4));
            // the station's light along its roof, a line of shade between cars
            tcol += vec3(0.5, 0.6, 0.55) * 0.12 * exp(-sq((p.y - GY - 7.6) / 0.35));
            tcol *= 1.0 - 0.6 * exp(-sq(min(lx2, carL - lx2) / 0.12));
            float doorX = mod(lx2 - 2.5, 4.5) - 2.25;
            tcol = mix(tcol, tcol * 0.6, (1.0 - smoothstep(0.02, 0.05, abs(abs(doorX) - 0.7))) * step(p.y, GY + 6.2) * (1.0 - win));
            tcol = mix(tcol, inside, win);
            c = mix(c, tcol, fillA(body) * onTrain);
            gGlow += win * 0.3;
        }
    }
    // the platform: polished stone, the lights in it, the yellow line at its edge
    if (p.y < GY + 0.1) {
        float my = GY - p.y;
        vec3 pl = mix(vec3(0.07, 0.075, 0.075), vec3(0.03, 0.032, 0.033), sat(my / 3.5));
        float lx3 = mod(p.x + uD * 0.7, 8.0) - 4.0;
        pl += lc * exp(-lx3 * lx3 / 2.0) * exp(-my * 0.6) * 0.1;
        float edge = fillA(abs(my - 0.35) - 0.17);
        pl = mix(pl, vec3(0.45, 0.33, 0.04), edge * 0.85);
        float tiles = lod(1.5) * (1.0 - smoothstep(0.0, 0.03 + gPx, abs(fract((p.x + uD) / 1.5) - 0.5) * 1.5 - 0.72)) * step(0.6, my);
        pl *= 1.0 - 0.15 * tiles;
        c = mix(c, pl, fillA(p.y - GY));
    }
    return c;
}

// =========================================================================== 3. the night train (inside a car)
vec3 outsideView(vec2 p, float dawn) {
    // through the windows: the tunnel, lamps streaming by; then up on the viaduct, the city at dawn
    float spd = 7.0 * (1.0 - 0.6 * uSlow);
    float x = p.x + uD * 0.3 + uTime * spd;
    vec3 tun = vec3(0.012, 0.012, 0.016);
    float lx = mod(x, 9.0) - 4.5;
    float lampT = exp(-sq(lx / 0.35)) * exp(-sq((p.y - GY - 6.2) / 0.25));
    tun += vec3(1.0, 0.75, 0.45) * lampT * 1.2 + vec3(1.0, 0.75, 0.45) * exp(-abs(lx) / 1.5) * 0.04;
    float cables = fillA(abs(p.y - GY - 4.8 - 0.06 * sin(x * 0.5)) - 0.02) * 0.3;
    tun += vec3(0.05) * cables;
    // dawn: the sky and a city skyline far away, moving slowly
    vec3 sk = mix(vec3(0.5, 0.36, 0.42), vec3(0.12, 0.14, 0.3), sat((p.y - GY - 4.0) / 4.0));
    float sx = p.x + uD * 0.3 + uTime * 0.9;
    float id = floor(sx / 1.4);
    float hh = GY + 4.2 + 2.2 * hash1(id) * hash1(id + 1.0);
    float bld = fillA(p.y - hh);
    float gl;
    float wl = windowsLit(vec2(sx, p.y), vec2(0.28, 0.35), vec2(0.07, 0.09), id, 0.25, gl);
    sk = mix(sk, vec3(0.08, 0.07, 0.11) + vec3(1.0, 0.7, 0.4) * wl * 0.5, bld);
    return mix(tun, sk, dawn);
}
vec3 stTrain(vec2 p, vec3 c, float jl) {
    float u = p.x + uD;
    float carL = 21.0;
    float ci = floor(u / carL);
    float lx = u - ci * carL;
    float dawn = smoothstep(LEN * 0.35, LEN * 0.8, jl);
    // the car's wall, the ceiling's curve
    float roof = GY + 9.8;
    vec3 wallC = vec3(0.11, 0.105, 0.09);
    vec3 col = wallC * (0.8 + 0.2 * smoothstep(GY, roof, p.y));
    // windows, rounded, the outside through them
    float wx = mod(lx - 2.0, 4.2) - 2.1;
    float winD = sdBox(vec2(wx, p.y - GY - 5.4), vec2(1.45, 1.5)) - 0.3;
    float inCar = step(1.4, lx) * step(lx, carL - 1.4);
    float win = fillA(winD) * inCar;
    vec3 outv = outsideView(p, dawn);
    col = mix(col, vec3(0.08, 0.085, 0.08), fillA(winD - 0.12) * inCar);
    col = mix(col, outv + vec3(0.03, 0.04, 0.05) * (1.0 - dawn), win);
    // reflections in the glass: the car's lights, faint
    col += vec3(0.08, 0.09, 0.08) * win * (1.0 - dawn * 0.7) * smoothstep(-1.2, 1.2, wx + p.y - GY - 5.4) * 0.6;
    // an advert strip above the windows
    float ad = fillA(sdBox(vec2(mod(lx, 4.2) - 2.1, p.y - GY - 8.4), vec2(1.6, 0.42)));
    float aid = floor(lx / 4.2) + ci * 5.0;
    vec3 adC = 0.45 + 0.4 * cos(TAU * (hash1(aid) + vec3(0.0, 0.33, 0.67)));
    col = mix(col, adC * 0.3, ad * inCar);
    // the ceiling and its light strip
    float ceilA = smoothstep(roof - 0.05, roof + 0.05, p.y);
    vec3 ceilC = mix(vec3(0.09, 0.085, 0.075), vec3(0.015, 0.015, 0.013), sat((p.y - roof) / 3.5));
    vec2 lq = vec2(mod(lx, 5.25) - 2.625, p.y - roof - 0.7);
    float strip = fillA(sdBox(lq, vec2(1.7, 0.13)) - 0.04);
    ceilC = mix(ceilC, vec3(1.0, 0.9, 0.7) * 1.5, strip);
    ceilC += vec3(1.0, 0.85, 0.6) * exp(-sq(sdBox(lq, vec2(1.7, 0.0)) / 0.7)) * 0.12;
    col = mix(col, ceilC, ceilA);
    // the bench along the wall
    float bench = fillA(sdBox(vec2(mod(lx, carL) - carL * 0.5, p.y - GY - 1.6), vec2(carL * 0.5 - 1.6, 0.9)) - 0.25);
    vec3 benchC = mix(vec3(0.05, 0.08, 0.2), vec3(0.09, 0.14, 0.3), smoothstep(GY + 1.0, GY + 2.5, p.y));
    float seatLines = step(0.5, fract((lx) / 1.4)) * 0.08;
    col = mix(col, benchC * (1.0 - seatLines), bench);
    // the gangway between two cars: a door with a little window
    float gx = lx < carL * 0.5 ? lx : lx - carL;
    float gang = fillA(sdBox(vec2(gx, p.y - GY - 4.9), vec2(1.3, 4.9)));
    vec3 gangC = vec3(0.07, 0.07, 0.065);
    gangC = mix(gangC, outsideView(p + vec2(3.0, 0.0), dawn) * 0.6 + 0.02, fillA(sdBox(vec2(gx, p.y - GY - 5.8), vec2(0.45, 0.9)) - 0.1));
    gangC = mix(gangC, vec3(0.08), fillA(abs(abs(gx) - 1.2) - 0.08));
    col = mix(col, gangC, gang);
    // poles, steel, and the hand straps swaying with the car
    float sway = sin(uTime * 0.9) * 0.08 + sin(uTime * 2.3) * 0.03;
    float px = mod(lx - 5.25, 10.5) - 5.25;
    float pole = fillA(abs(px) - 0.07) * step(GY, p.y) * step(p.y, roof + 0.3);
    vec3 poleC = mix(vec3(0.85, 0.75, 0.2), vec3(1.0, 0.95, 0.6), exp(-sq((px + 0.03) / 0.03)));
    col = mix(col, poleC * 0.45, pole * step(0.3, abs(gx) - 1.4));
    float railY = roof - 0.35;
    col = mix(col, vec3(0.3, 0.3, 0.28), fillA(abs(p.y - railY) - 0.05) * inCar);
    float sx2 = mod(lx, 1.6) - 0.8;
    float sid = floor(lx / 1.6);
    float ang = sway * (1.0 + 0.3 * sin(sid * 1.7)) + 0.03 * sin(uTime * 1.7 + sid);
    vec2 sq0 = vec2(sx2, p.y - railY);
    vec2 sqr = rot(-ang) * sq0;
    float strap = min(sdBox(sqr - vec2(0.0, -0.45), vec2(0.04, 0.45)), abs(length(sqr - vec2(0.0, -1.05)) - 0.17) - 0.04);
    col = mix(col, vec3(0.5, 0.48, 0.42), fillA(strap) * inCar);
    // the floor
    if (p.y < GY + 0.1) {
        float my = GY - p.y;
        vec3 fl = mix(vec3(0.09, 0.085, 0.075), vec3(0.04, 0.038, 0.034), sat(my / 3.0));
        fl += vec3(0.1) * exp(-my * 1.5) * 0.4;
        c = mix(col, fl, fillA(p.y - GY));
        return c;
    }
    return col;
}

// =========================================================================== 4. the canal at dawn
vec3 stCanal(vec2 p, vec3 c) {
    // far: chimneys, gas holders, cranes, their smoke
    {
        float u = p.x + uD * 0.1;
        float cw = 9.0;
        float id = floor(u / cw);
        float lx = (u / cw - id) * cw - cw * 0.5;
        float hx = hash1(id + 2.0);
        vec3 sil = mix(vec3(0.12, 0.09, 0.14), uFog, 0.45);
        float base = GY + 2.5;
        float d = 1e3;
        // a chimney or two
        float ch = 6.0 + 6.0 * hx;
        d = min(d, sdBox(vec2(lx - 1.5, p.y - base - ch * 0.5), vec2(0.28 - (p.y - base) * 0.01, ch * 0.5)));
        // a gas holder's lattice drum
        if (hash1(id + 5.0) > 0.5) {
            vec2 g = vec2(lx + 2.0, p.y - base - 1.6);
            float drum = sdBox(g, vec2(1.8, 1.6));
            float lat = min(abs(fract(g.x * 1.2 + g.y * 0.6) - 0.5), abs(fract(g.x * 1.2 - g.y * 0.6) - 0.5)) / 1.2;
            d = min(d, max(drum, -(lat - 0.05)));
            d = min(d, max(sdBox(g, vec2(1.9, 1.62)), -sdBox(g, vec2(1.75, 1.5))));
        }
        // a crane
        if (hash1(id + 8.0) > 0.55) {
            vec2 g = vec2(lx + 3.4, p.y - base);
            d = min(d, sdBox(g - vec2(0.0, 3.5), vec2(0.15, 3.5)));
            d = min(d, sdSeg(g, vec2(-1.5, 7.0), vec2(4.0, 7.0)) - 0.09);
            d = min(d, sdSeg(g, vec2(0.0, 7.6), vec2(3.6, 7.0)) - 0.03);
            d = min(d, sdSeg(g, vec2(2.8, 7.0), vec2(2.8, 5.2)) - 0.02);
        }
        // low sheds
        d = min(d, p.y - base - 1.0 - 0.6 * step(0.5, fract(u * 0.15)));
        c = lay(c, sil, fillA(d));
        // the smoke, drifting
        vec2 sp = vec2(lx - 1.5, p.y - base - ch);
        float sm = fbm(vec2(sp.x * 0.5 - uTime * 0.12 + id, sp.y * 0.35 - uTime * 0.08));
        float plume = smoothstep(0.0, 0.5, sp.y) * exp(-sq((sp.x - sp.y * 0.6) / (0.4 + sp.y * 0.35))) * exp(-sp.y * 0.12);
        c = mix(c, mix(uFog, vec3(1.0, 0.85, 0.8), 0.25), sat(plume * sm * 1.6) * 0.6);
        // the red lights on top of the chimneys, slow
        vec2 rl = vec2(lx - 1.5, p.y - base - ch - 0.15);
        float blink = 0.5 + 0.5 * sin(uTime * 1.3 + id * 2.0);
        c += vec3(1.0, 0.15, 0.1) * exp(-dot(rl, rl) / 0.03) * blink * 0.8;
    }
    // mid: warehouses with saw-tooth roofs, sodium lamps, a freight train on the embankment
    {
        float u = p.x + uD * 0.4 + 7.0;
        float cw = 11.0;
        float id = floor(u / cw);
        float lx = (u / cw - id) * cw;
        float base = GY + 1.3;
        float hgt = 2.6 + 1.4 * hash1(id);
        float saw = fract(lx / 1.6);
        float roofY = base + hgt + 0.9 * saw;
        float d = max(p.y - roofY, abs(lx - cw * 0.45) - cw * 0.42);
        vec3 wh = mix(vec3(0.15, 0.1, 0.12), vec3(0.24, 0.15, 0.14), hash1(id + 3.0));
        wh = haze(wh, 0.25);
        // the saw-tooth glass catching the dawn
        wh = mix(wh, mix(uSkyHor, vec3(1.0, 0.7, 0.55), 0.3), smoothstep(0.75, 0.95, saw) * step(roofY - 0.9 * saw - 0.01, p.y) * 0.6);
        float gl;
        float wl = windowsLit(vec2(lx, p.y - base - 0.6), vec2(1.1, 1.2), vec2(0.28, 0.22), id * 9.0, 0.35, gl);
        wl *= step(p.y, base + hgt - 0.2) * step(base + 0.2, p.y);
        wh += vec3(1.0, 0.6, 0.25) * wl * 0.7 * uLamp;
        c = lay(c, wh, fillA(d));
        // freight wagons
        float wu = p.x + uD * 0.55;
        float wid = floor(wu / 5.0);
        float wx = mod(wu, 5.0) - 2.5;
        float wag = sdBox(vec2(wx, p.y - GY - 1.55), vec2(2.25, 0.75));
        vec3 wc = mix(vec3(0.3, 0.12, 0.08), vec3(0.12, 0.2, 0.25), step(0.5, hash1(wid)));
        wc *= 0.7 + 0.3 * step(0.5, fract(wx * 1.6 + 0.25));
        c = lay(c, haze(wc, 0.15), fillA(wag) * step(hash1(wid + 40.0), 0.75));
        c = lay(c, vec3(0.05, 0.04, 0.05), fillA(abs(p.y - GY - 0.75) - 0.08));
    }
    // sodium lamps along the towpath
    c = lamps(p, c, 1.0, 13.0, 2.0, vec3(1.0, 0.55, 0.2), 5.6, uLamp);
    // the towpath, then the canal in front: the dawn in the water, the lamps, ripples
    if (p.y < GY + 0.1) {
        float my = GY - p.y;
        vec3 path = mix(vec3(0.16, 0.12, 0.12), vec3(0.09, 0.07, 0.08), sat(my / 1.2));
        float cob = lod(0.6) * (1.0 - smoothstep(0.32, 0.5, length(fract(vec2((p.x + uD) / 0.6, my / 0.35)) - 0.5))) * 0.15;
        path *= 1.0 - cob;
        // bollards along the edge
        float bx = mod(p.x + uD, 6.5) - 3.25;
        float boll = fillA(sdBox(vec2(bx, p.y - GY + 1.05), vec2(0.22, 0.3)) - 0.08);
        float edge = fillA(abs(my - 1.35) - 0.08);
        path = mix(path, vec3(0.3, 0.27, 0.27), edge);
        if (my > 1.4) {
            float wy = my - 1.4;
            vec2 rp = vec2(p.x + 0.12 * sin(p.y * 6.0 + uTime * 1.3) * sat(wy), GY + 4.0 + wy * 2.2);
            vec3 water = mix(uSkyHor, uSkyTop, sat(wy / 3.0)) * 0.7;
            water = mix(water, vec3(0.04, 0.04, 0.07), sat(wy / 3.0) * 0.6);
            float refl = lampReflect(vec2(p.x, p.y), 1.0, 13.0, 2.0, 5.6, GY - 1.4) * uLamp;
            water += vec3(1.0, 0.55, 0.2) * refl * 0.8;
            float rip = vnoise(vec2((p.x + uD) * 0.8, wy * 6.0 - uTime * 0.6));
            water *= 0.85 + 0.3 * rip;
            // mist on the water
            water = mix(water, uFog * 1.1, exp(-wy * 1.5) * 0.3);
            path = water;
        }
        path = mix(path, vec3(0.06, 0.05, 0.06), boll * step(my, 1.4));
        c = mix(c, path, fillA(p.y - GY));
    }
    return c;
}

// =========================================================================== 5. the fields at the city's edge
float grassTufts(vec2 p, float k, float y0, float h, float dens, float seed) {
    float u = p.x + uD * k;
    float cw = 0.22;
    float id = floor(u / cw);
    float lx = (u / cw - id) * cw - cw * 0.5;
    float hh = h * (0.4 + 0.6 * hash1(id + seed)) * step(hash1(id * 1.7 + seed), dens);
    float t = sat((p.y - y0) / max(hh, 1e-3));
    float lean = (hash1(id + seed + 3.0) - 0.5) * 0.25 + 0.06 * sin(uTime * 1.2 + u * 0.3) * t;
    float w = 0.05 * (1.0 - t);
    float d = abs(lx - lean * t * hh) - w;
    d = max(d, p.y - y0 - hh);
    d = max(d, y0 - p.y);
    return fillA(d);
}
vec3 bambooClump(vec2 p, vec3 c, float k, float base, float scale, float dens, vec3 col, float hz, float seed);
vec3 stFields(vec2 p, vec3 c, float jl) {
    // far blue mountains, two ridges
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        float u = p.x + uD * (0.05 + 0.07 * fi) + fi * 50.0;
        float hgt = GY + 4.5 - fi * 1.6 + 3.0 * fbm(vec2(u * 0.05, fi)) + 1.0 * vnoise(vec2(u * 0.2, fi));
        vec3 mc = mix(uFog * 0.95, mix(vec3(0.35, 0.45, 0.55), vec3(0.3, 0.42, 0.38), fi), 0.35 + 0.25 * fi);
        c = lay(c, mc, fillA(p.y - hgt));
    }
    // rolling hills, patchwork fields, a few trees
    {
        float u = p.x + uD * 0.25;
        float hgt = GY + 2.2 + 1.2 * sin(u * 0.11) + 0.6 * sin(u * 0.27 + 1.0);
        float a = fillA(p.y - hgt);
        float pid = floor(u / 4.0 + floor((hgt - p.y) / 0.6) * 0.5);
        vec3 fc = mix(vec3(0.42, 0.55, 0.25), vec3(0.62, 0.6, 0.3), hash1(pid));
        fc = mix(fc, vec3(0.3, 0.48, 0.28), step(0.7, hash1(pid + 2.0)));
        fc *= 0.9 + 0.1 * smoothstep(hgt - 1.0, hgt, p.y);
        fc = haze(fc, 0.3);
        c = lay(c, fc, a);
        float tid = floor(u / 3.0);
        float tx = mod(u, 3.0) - 1.5 + (hash1(tid) - 0.5);
        float th = GY + 2.2 + 1.2 * sin((tid * 3.0 + 1.5) * 0.11) + 0.6 * sin((tid * 3.0 + 1.5) * 0.27 + 1.0);
        float tree = fillA(length(vec2(tx, (p.y - th - 0.45) * 1.2)) - 0.42) * step(hash1(tid + 7.0), 0.4);
        c = lay(c, haze(vec3(0.16, 0.3, 0.16), 0.3), tree);
    }
    // power lines on their pylons, sagging wires
    {
        float u = p.x + uD * 0.45;
        float sp = 18.0;
        float n = floor(u / sp + 0.5);
        float lx = u - n * sp;
        float base = GY + 1.0;
        vec2 q = vec2(lx, p.y - base);
        float tw = 0.9 - q.y * 0.07;
        float d = max(abs(abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)) - 0.05, -q.y);
        d = max(d, q.y - 8.5);
        d = min(d, sdBox(q - vec2(0.0, 7.4), vec2(1.6, 0.06)));
        d = min(d, sdBox(q - vec2(0.0, 6.3), vec2(1.2, 0.06)));
        float lat = min(abs(fract(q.y * 0.9 + q.x * 0.6) - 0.5), abs(fract(q.y * 0.9 - q.x * 0.6) - 0.5)) / 0.9;
        d = min(d, max(lat - 0.03, abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)));
        d = max(d, -q.y);
        d = max(d, q.y - 8.6);
        vec3 pc = haze(vec3(0.3, 0.33, 0.36), 0.2);
        c = lay(c, pc, fillA(d) * 0.9);
        float fx = (lx + sp * 0.5) / sp;
        for (int w = 0; w < 2; w++) {
            float wy = base + 7.35 - float(w) * 1.1 - 1.4 * 4.0 * fx * (1.0 - fx);
            c = lay(c, pc * 0.8, (1.0 - smoothstep(0.012, 0.012 + gPx, abs(p.y - wy))) * 0.7);
        }
    }
    // the first bamboo, more of it toward the forest
    c = bambooClump(p, c, 0.6, GY + 0.6, 0.8, smoothstep(LEN * 0.35, LEN, jl) * 0.28, vec3(0.32, 0.5, 0.25), 0.2, 3.0);
    // the meadow along the path, wild flowers, grass
    {
        float u = p.x + uD * 0.85;
        float top = GY + 1.0 + 0.25 * sin(u * 0.4);
        vec3 mc = mix(vec3(0.3, 0.5, 0.18), vec3(0.45, 0.62, 0.22), vnoise(vec2(u * 0.3, p.y)));
        c = lay(c, mc, fillA(p.y - top));
        float fl = 0.0;
        vec2 fq = vec2(u / 0.55, (p.y - GY) / 0.5);
        vec2 fi = floor(fq), ff = fract(fq) - 0.5;
        vec2 fo = (hash2(fi) - 0.5) * 0.6;
        float on = step(0.72, hash(fi + 2.0)) * step(p.y, top - 0.1);
        float fd = length(ff - fo) * 0.5;
        vec3 flc = hash(fi + 5.0) < 0.5 ? vec3(1.0, 0.95, 0.85) : (hash(fi + 6.0) < 0.5 ? vec3(1.0, 0.8, 0.25) : vec3(1.0, 0.55, 0.7));
        c = lay(c, flc, fillS(fd - 0.06, gPx) * on);
        c = lay(c, vec3(0.35, 0.55, 0.2), grassTufts(p, 0.85, top - 0.15, 0.7, 0.55, 1.0));
    }
    // the dirt path
    if (p.y < GY + 0.1) {
        float my = GY - p.y;
        vec3 dirt = mix(vec3(0.6, 0.48, 0.32), vec3(0.42, 0.33, 0.22), sat(my / 2.5));
        dirt *= 0.92 + 0.12 * vnoise(vec2((p.x + uD) * 1.5, my * 4.0));
        float pebble = lod(0.5) * step(0.85, hash(floor(vec2((p.x + uD) / 0.5, my / 0.3)))) * 0.1;
        dirt *= 1.0 - pebble;
        float edge = smoothstep(2.4, 2.8, my + 0.2 * sin((p.x + uD) * 0.9));
        dirt = mix(dirt, vec3(0.28, 0.45, 0.16), edge);
        c = mix(c, dirt, fillA(p.y - GY));
        c = lay(c, vec3(0.3, 0.5, 0.18), grassTufts(p, 1.0, GY - 2.9, 0.9, 0.7, 7.0));
    }
    return c;
}

// =========================================================================== 6. the bamboo forest
// one layer of bamboo culms: segmented by nodes, swaying, leaves in sprays at their tops and sides
vec3 bambooClump(vec2 p, vec3 c, float k, float base, float scale, float dens, vec3 col, float hz, float seed) {
    if (dens <= 0.0) return c;
    float u = p.x + uD * k + seed * 17.0;
    float cw = 1.25 * scale;
    float id0 = floor(u / cw);
    for (int o = -1; o <= 1; o++) {
        float id = id0 + float(o);
        float h = hash1(id + seed);
        if (h > dens) continue;
        float x0 = (id + 0.5 + (hash1(id + seed + 1.0) - 0.5) * 0.6) * cw;
        float w = (0.13 + 0.1 * hash1(id + seed + 2.0)) * scale;
        float lean = (hash1(id + seed + 3.0) - 0.5) * 0.06;
        float yy = p.y - base;
        float sway = (0.15 * sin(uTime * 0.5 + id * 1.3) + 0.06 * sin(uTime * 1.1 + id)) * scale * sq(max(yy, 0.0) / 20.0);
        float x = u - x0 - lean * yy - sway;
        float d = abs(x) - w;
        d = max(d, base - p.y);
        float a = fillA(d);
        if (a > 0.0) {
            // nodes, a light down one side
            float seg = 1.6 * scale;
            float ny = fract((yy + hash1(id) * seg) / seg);
            float node = (1.0 - smoothstep(0.0, 0.05 + gPx / seg, abs(ny - 0.5) * seg / scale * 0.5 - 0.0)) ;
            node = exp(-sq((ny - 0.5) * seg / (0.06 * scale + gPx)));
            vec3 cc = col * (0.75 + 0.45 * smoothstep(-w, w, -x)) ;
            cc = mix(cc, col * 0.55, node * 0.7);
            cc = mix(cc, col * 1.35 + 0.03, node * smoothstep(0.5, 1.0, -x / w) * 0.4);
            c = mix(c, haze(cc, hz), a);
        }
        // leaves: slender blades in sprays off the culm, high up
        for (int l = 0; l < 3; l++) {
            float fl = float(l);
            float ly = (6.0 + 5.0 * hash1(id + fl * 7.0 + seed)) * scale;
            float side = hash1(id + fl * 3.0) < 0.5 ? -1.0 : 1.0;
            vec2 at = vec2(x0 + lean * ly + sway * 0.8, base + ly);
            vec2 q = vec2(u, p.y) - at;
            q.x *= side;
            for (int b = 0; b < 3; b++) {
                float fb = float(b);
                float ang = -0.35 - 0.35 * fb + 0.05 * sin(uTime * 0.9 + id + fb);
                vec2 r = rot(ang) * q;
                float L = (1.3 + 0.4 * fb) * scale;
                float t = sat(r.x / L);
                float lw = 0.17 * scale * sin(t * PI) * (1.0 - t * 0.3);
                float ld = max(abs(r.y + 0.1 * scale * t * t) - lw, max(-r.x, r.x - L));
                vec3 lc = col * (0.85 + 0.35 * fb / 2.0);
                c = mix(c, haze(lc, hz), fillA(ld));
            }
        }
    }
    return c;
}
vec3 stBamboo(vec2 p, vec3 c, float jl, float home) {
    // light from the upper left through the canopy, layers of culms receding into the green haze
    vec3 far = vec3(0.3, 0.42, 0.2);
    c = bambooClump(p, c, 0.12, GY + 2.0, 0.55, 0.8, mix(far, uFog, 0.5), 0.55, 1.0);
    c = bambooClump(p, c, 0.28, GY + 1.5, 0.75, 0.75 * (1.0 - 0.5 * home), vec3(0.16, 0.27, 0.08), 0.3, 2.0);
    c = bambooClump(p, c, 0.5, GY + 1.0, 1.0, 0.55 * (1.0 - 0.6 * home), vec3(0.08, 0.16, 0.04), 0.12, 5.0);
    // shafts of light
    float sh = 0.0;
    vec2 sq0 = rot(-0.45) * (p - vec2(-30.0, 14.0));
    float sx = sq0.x + uD * 0.15;
    sh = sq(sat(vnoise(vec2(sx * 0.35, uTime * 0.05)) * 1.4 - 0.45)) * smoothstep(GY - 2.0, GY + 6.0, p.y);
    c += uSunCol * sh * 0.55;
    c = bambooClump(p, c, 0.8, GY + 0.5, 1.35, 0.4 * (1.0 - 0.7 * home), vec3(0.035, 0.07, 0.02), 0.03, 9.0);
    // the mossy ground, fallen leaves, the path
    if (p.y < GY + 0.5) {
        float my = GY - p.y;
        vec3 g = mix(vec3(0.14, 0.2, 0.05), vec3(0.05, 0.08, 0.02), sat(my / 3.0));
        g *= 0.85 + 0.25 * vnoise(vec2((p.x + uD) * 1.2, my * 3.0));
        float path = 1.0 - smoothstep(0.8, 1.4, abs(my - 1.0 + 0.2 * sin((p.x + uD) * 0.4)));
        g = mix(g, vec3(0.3, 0.22, 0.1) * (0.9 + 0.1 * vnoise(vec2((p.x + uD) * 2.0, my * 5.0))), path * 0.75);
        vec2 lq = vec2((p.x + uD) / 0.45, my / 0.25);
        vec2 li = floor(lq), lf = fract(lq) - 0.5;
        float leaf = fillS(sdEll(rot(hash(li) * 6.0) * lf, vec2(0.38, 0.12)), 0.08) * step(0.75, hash(li + 1.0)) * lod(0.25);
        g = mix(g, mix(vec3(0.75, 0.6, 0.25), vec3(0.55, 0.45, 0.2), hash(li + 2.0)), leaf * 0.7);
        g += uSunCol * sh * 0.25;
        c = mix(c, g, fillA(p.y - GY - 0.25 * sin((p.x + uD) * 0.3) * 0.0));
        c = lay(c, vec3(0.28, 0.42, 0.16), grassTufts(p, 1.0, GY - 0.05, 0.45, 0.35 + 0.4 * home, 4.0));
    }
    return c;
}

// =========================================================================== 7. home: a glade at sunset
vec3 stHome(vec2 p, vec3 c, float jl) {
    // far hills in the golden haze
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        float u = p.x + uD * (0.06 + 0.08 * fi) + fi * 30.0;
        float hgt = GY + 3.0 - fi * 1.2 + 2.5 * fbm(vec2(u * 0.06, fi + 5.0));
        vec3 mc = mix(uFog, vec3(0.45, 0.3, 0.32), 0.25 + 0.2 * fi);
        c = lay(c, mc, fillA(p.y - hgt));
    }
    // the bamboo grove framing the glade, thinner
    c = stBamboo(p, c, jl, 1.0);
    // a little stream, the sky in it
    // tall grass and flowers in the warm light
    {
        float u = p.x + uD * 0.9;
        float top = GY + 0.6 + 0.2 * sin(u * 0.5);
        c = lay(c, vec3(0.42, 0.42, 0.16), grassTufts(p, 0.9, GY + 0.2, 1.4, 0.6, 12.0));
        vec2 fq = vec2(u / 0.7, (p.y - GY) / 0.6);
        vec2 fi = floor(fq), ff = fract(fq) - 0.5;
        vec2 fo = (hash2(fi) - 0.5) * 0.6;
        float on = step(0.7, hash(fi + 2.0)) * step(p.y, top) * step(GY - 0.5, p.y);
        vec3 flc = hash(fi + 5.0) < 0.5 ? vec3(1.0, 0.85, 0.7) : vec3(1.0, 0.6, 0.65);
        c = lay(c, flc, fillS(length(ff - fo) * 0.6 - 0.06, gPx) * on);
    }
    return c;
}

// ---------------------------------------------------------------- one place
vec3 place(int i, vec2 p, vec3 c, float jl) {
    if (i <= 0) return stStreet(p, c, 0.0);
    if (i == 1) return stStreet(p, c, 1.0);
    if (i == 2) return stStation(p, c);
    if (i == 3) return stTrain(p, c, jl);
    if (i == 4) return stCanal(p, c);
    if (i == 5) return stFields(p, c, jl);
    if (i == 6) return stBamboo(p, c, jl, 0.0);
    return stHome(p, c, jl);
}

void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    gPx = uPxP;
    vec3 s = sky(p);
    // where this pixel is along the way, and the places there
    float j = uD + (p.x - uXP) * 0.4;   // compressed: the next place only shows as the panda nears it
    float t = j / LEN;
    float si = floor(t);
    float jl = (t - si) * LEN;
    float w = smoothstep(LEN - BL, LEN, jl);
    int a = int(clamp(si, 0.0, 7.0)), b = int(clamp(si + 1.0, 0.0, 7.0));
    if (j < 0.0) { a = 0; b = 0; w = 0.0; }
    vec3 c;
    float g0 = 0.0;
    vec3 gc0 = vec3(1.0);
    if (w < 0.999) { c = place(a, p, s, jl); g0 = gGlow; gc0 = gGlowC; }
    if (w > 0.001 && b != a) {
        gGlow = 0.0;
        vec3 c2 = place(b, p, s, jl - LEN);
        c = w >= 0.999 ? c2 : mix(c, c2, w);
        gGlow = mix(g0, gGlow, w);
        gGlowC = mix(gc0, gGlowC, w);
    } else if (w >= 0.999) {
        c = place(b, p, s, jl - LEN);
    } else {
        gGlow = g0; gGlowC = gc0;
    }
    c = rain(p, c, uRain);
    c = bokeh(p, c, uRain * 0.9 + uDrop * 0.3, vec3(1.0, 0.7, 0.4), vec3(0.5, 0.7, 1.0));
    // the drop: a warm glow welling up from the ground (a steady swell, settling over seconds)
    c += uSunCol * uDrop * 0.05 * smoothstep(4.0, -10.0, p.y);
    c = mix(c, c * vec3(0.3, 0.32, 0.4), uOver * 0.75);
    c = mix(uBridgeCol, c, uWorldA);
    frag = vec4(max(c, 0.0), 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ composite
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uWorld, vUv).rgb;
    vec3 n = texture(uWorld, vUv + vec2(uTexel.x, 0.0)).rgb + texture(uWorld, vUv - vec2(uTexel.x, 0.0)).rgb +
             texture(uWorld, vUv + vec2(0.0, uTexel.y)).rgb + texture(uWorld, vUv - vec2(0.0, uTexel.y)).rgb;
    c = max(c + (c - n * 0.25) * 0.2, 0.0);
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ the pandas
// A kawaii panda, a sticker mascot seen from the front: few shapes, flat fills (one soft step of shade), one uniform
// outline: a round head (half its height), a bean of a body, little limbs, round ears, oval patches leaning outward
// with a dark eye and a single dot of light, a tiny nose, a minimal mouth (a w when happy), light blush. It waddles
// (rocking from foot to foot, a little bounce). Local units: its height is 1, feet at the origin, facing +x. One
// quad per panda; the reflection in a wet ground is the same panda mirrored under its feet.
inline const char* PANDA_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uVP;
uniform vec4 uRect; // x0, y0, x1, y1 (board units)
out vec2 vP;
void main() {
    vec2 p = mix(uRect.xy, uRect.zw, aPos * 0.5 + 0.5);
    vP = p;
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
inline const char* PANDA_FS = R"(#version 330 core
in vec2 vP;
out vec4 frag;
uniform vec2 uPos;           // feet (board)
uniform float uH, uFace, uPx, uTime;
uniform float uAdult;        // 0 the cub's proportions .. 1 a grown panda's
uniform float uWalk, uPhase, uMood, uBreath, uTurn, uTilt, uBlink, uSit, uWipe, uTear, uHop, uLean, uArms, uJoyEyes;
uniform vec2 uLook;
uniform vec2 uEars;          // the ears' extra lift (secondary motion)
uniform vec3 uAmb, uKey, uRimC;
uniform vec2 uKeyDir;
uniform float uReflect, uShadow, uAlpha, uHaze;
uniform vec3 uHazeC;
uniform float uHug;
uniform int uPart;

float gAA, gLW;
vec3 gCol;
float gA;
const vec3 INK = vec3(0.045, 0.035, 0.045);
const vec3 BLACK = vec3(0.06, 0.055, 0.065);
const vec3 WHITE = vec3(0.95, 0.93, 0.89);

float fillP(float d) { return 1.0 - smoothstep(-gAA, gAA, d); }
// a sticker's part: a uniform outline, then a flat fill
void paint(float d, vec3 c) {
    float o = fillP(d - gLW);
    gCol = mix(gCol, INK, o);
    gA = max(gA, o);
    gCol = mix(gCol, c, fillP(d));
}
// a flat color under the scene's light, with one soft step of shade on the side away from the light
vec3 tone(vec3 base, vec2 q, vec2 ctr, float r) {
    vec2 n = (q - ctr) / r;
    float l = dot(n, uKeyDir * vec2(uFace, 1.0));
    vec3 c = base * (uAmb + uKey * 0.85);
    return mix(c * 0.84, c, smoothstep(-0.5, -0.3, l));
}
float capsule(vec2 q, vec2 a, vec2 b, float r) { return sdSeg(q, a, b) - r; }

vec4 panda(vec2 q) {
    gCol = vec3(0.0); gA = 0.0;
    float A = uAdult, walk = uWalk, ph = uPhase, sit = uSit, mood = uMood, sad = 1.0 - mood;
    q.y -= uHop;
    // the waddle: rocking from foot to foot about the feet, a little bounce
    float rock = sin(ph) * 0.08 * walk + uLean * 0.06;
    vec2 qr = rot(-rock) * q;
    qr.y -= walk * 0.028 * abs(cos(ph)) + 0.005 * sin(uBreath);
    // a round head (half the height), a bean of a body, little limbs
    vec2 Br = vec2(mix(0.23, 0.255, A), mix(0.2, 0.23, A) * (1.0 + 0.012 * sin(uBreath))) * mix(vec2(1.0), vec2(1.06, 0.92), sit);
    vec2 B = vec2(0.0, mix(0.07 + Br.y, 0.035 + Br.y, sit));
    float headR = mix(0.3, 0.27, A);
    vec2 Hc = B + vec2(0.0, Br.y + headR * 0.62 - sad * 0.02);
    float tilt = uTilt - sad * 0.04;
    vec2 qh = rot(tilt) * (qr - Hc);
    float turn = uTurn;
    // feet, arms
    float lift = 0.035 * walk;
    vec2 fN = mix(vec2(0.12, 0.055 + max(0.0, -sin(ph)) * lift), vec2(0.15, 0.055), sit);
    vec2 fF = mix(vec2(-0.12, 0.055 + max(0.0, sin(ph)) * lift), vec2(-0.15, 0.055), sit);
    vec2 fr = vec2(0.085, 0.062);
    float armR = mix(0.062, 0.07, A);
    vec2 shN = B + vec2(Br.x * 0.85, Br.y * 0.15), shF = B + vec2(-Br.x * 0.85, Br.y * 0.15);
    float swing = sin(ph) * 0.3 * walk;
    vec2 hN = shN + rot(swing) * vec2(0.06, -0.11), hF = shF + rot(swing) * vec2(-0.06, -0.11);
    vec2 eyeN = Hc + rot(-tilt) * vec2(0.11 + 0.07 * turn, 0.0);
    hN = mix(hN, eyeN + vec2(0.035 + 0.01 * sin(uBreath * 7.0), -0.05), uWipe);
    hN = mix(hN, shN + vec2(0.1, 0.14), uArms); hF = mix(hF, shF + vec2(-0.1, 0.14), uArms);
    hN = mix(hN, shN + vec2(0.14, 0.02), uHug);
    float ear = mix(0.1, 0.085, A);
    float earDrop = sad * 0.06;
    vec2 eF = Hc + rot(-tilt) * vec2(-headR * 0.74 - 0.025 * turn - earDrop * 0.6, headR * 0.7 - earDrop + uEars.y * 0.05);
    vec2 eN = Hc + rot(-tilt) * vec2(headR * 0.74 - 0.025 * turn + earDrop * 0.6, headR * 0.7 - earDrop + uEars.x * 0.05);
    bool armOver = uWipe > 0.3;

    // back to front: far arm, ears, feet, body, near arm, head, face
    paint(capsule(qr, shF, hF, armR), tone(BLACK, qr, hF, 0.1));
    paint(length(qr - eF) - ear, tone(BLACK, qr, eF, ear));
    paint(length(qr - eN) - ear, tone(BLACK, qr, eN, ear));
    paint(sdEll(qr - fF, fr), tone(BLACK, qr, fF, 0.1));
    paint(sdEll(qr - fN, fr), tone(BLACK, qr, fN, 0.1));
    paint(sdEll(qr - B, Br), tone(WHITE, qr, B, Br.x));
    if (!armOver) paint(capsule(qr, shN, hN, armR), tone(BLACK, qr, hN, 0.1));
    float headD = sdEll(qh, vec2(headR * 1.07, headR * 0.95));
    paint(headD, tone(WHITE, qh, vec2(0.0), headR));

    // the face, set low on the head: oval patches leaning outward (more when sad), pure black; in each a glossy
    // black eye, large, low and to the inner side, its shine (a big dot of light above, a small one below, the same
    // side on both eyes) what makes it read; a soft blink to a curved line; ^ ^ in joy
    vec2 fc = vec2(0.06 * turn, -0.06);
    float sp = mix(0.13, 0.11, abs(turn)) * mix(1.0, 0.92, A);
    float lean = mix(0.55, 0.3, mood);
    vec2 pNc = fc + vec2(sp, 0.025), pFc = fc + vec2(-sp, 0.025);
    vec2 pr = vec2(0.064, 0.082) * mix(1.0, 0.88, A);
    float pN = sdEll(rot(-lean) * (qh - pNc - vec2(0.008, -0.008)), pr);
    float pF = sdEll(rot(lean) * (qh - pFc - vec2(-0.008, -0.008)), pr);
    vec3 patchC = BLACK * (uAmb + uKey * 0.85);
    gCol = mix(gCol, patchC, fillP(min(pN, pF)));
    float open = 1.0 - max(uBlink, 0.0);
    float esz = mix(0.92, 1.0, mood) * mix(1.0, 0.9, A);
    for (int i = 0; i < 2; i++) {
        float inner = i == 0 ? -1.0 : 1.0;
        vec2 ec = (i == 0 ? pNc : pFc) + vec2(0.007 * inner, -0.012) + uLook * vec2(0.003, 0.002);
        vec2 eq = (qh - ec) / esz;
        float k = 1.0 - uJoyEyes;
        // open: a glossy black bean, a faint dark-grey edge apart from the patch
        float o = smoothstep(0.2, 0.45, open);
        float eye = sdEll(eq, vec2(0.04, 0.047 * max(open, 0.15)));
        float ea = fillP(eye * esz) * k * o;
        gCol = mix(gCol, vec3(0.11, 0.105, 0.12) * (uAmb + uKey * 0.85), fillP(eye * esz - gLW * 0.5) * k * o);
        gCol = mix(gCol, vec3(0.012, 0.01, 0.014), ea);
        float big = length(eq - vec2(-0.013, 0.017)) - mix(0.016, 0.0135, mood);
        float small = length(eq - vec2(0.014, -0.017)) - 0.0065;
        gCol = mix(gCol, vec3(1.0), fillP(min(big, small) * esz) * ea * smoothstep(0.45, 0.7, open));
        // closed (blinking): a soft curved line
        float line = abs(eq.y + 0.004 - 9.0 * eq.x * eq.x) - 0.0045;
        line = max(line, abs(eq.x) - 0.036);
        gCol = mix(gCol, vec3(0.5, 0.48, 0.5) * (uAmb + uKey * 0.85), fillP(line * esz) * k * (1.0 - o));
        // joy: ^ ^
        float arc = abs(length((eq + vec2(0.0, 0.02)) * vec2(1.0, 1.15)) - 0.03) - 0.0075;
        arc = max(arc, -eq.y);
        gCol = mix(gCol, WHITE * (uAmb + uKey * 0.85), fillP(arc * esz) * uJoyEyes);
    }
    // a single tear
    if (uTear > 0.01) {
        vec2 tq = qh - pNc - vec2(0.03, -0.09 - 0.05 * (1.0 - uTear));
        float drop = min(length(tq) - 0.015, sdEll(tq - vec2(0.0, 0.014), vec2(0.008, 0.018)));
        gCol = mix(gCol, vec3(0.6, 0.82, 1.0) * (uAmb + uKey), fillP(drop) * smoothstep(0.0, 0.4, uTear));
    }
    // blush
    float ck = min(sdEll(qh - fc - vec2(sp + 0.055, -0.085), vec2(0.04, 0.022)), sdEll(qh - fc - vec2(-sp - 0.055, -0.085), vec2(0.04, 0.022)));
    gCol = mix(gCol, vec3(1.0, 0.6, 0.62) * (uAmb + uKey * 0.85), fillP(ck) * (0.35 + 0.3 * mood) * fillP(headD + 0.01));
    // nose
    vec2 nq = qh - fc - vec2(0.0, -0.06);
    gCol = mix(gCol, BLACK, fillP(sdEll(nq, vec2(0.022, 0.015))));
    // mouth: a small w when happy, a small frown when sad (open in joy)
    vec2 mq = qh - fc - vec2(0.0, -0.09);
    float happyM = smoothstep(0.35, 0.8, mood);
    float wArc = abs(length(vec2(abs(mq.x) - 0.013, mq.y - 0.003)) - 0.013) - 0.0035;
    wArc = max(wArc, mq.y - 0.003);
    float frown = abs(length(vec2(mq.x, mq.y + 0.026)) - 0.02) - 0.0035;
    frown = max(frown, -(mq.y + 0.01));
    float mouth = mix(frown, wArc, happyM);
    float openM = smoothstep(0.85, 1.0, mood) * uJoyEyes;
    if (openM > 0.02) mouth = min(mouth, sdEll(mq - vec2(0.0, -0.01), vec2(0.014, 0.014 * openM)));
    gCol = mix(gCol, INK, fillP(mouth));
    if (armOver) paint(capsule(qr, shN, hN, armR), tone(BLACK, qr, hN, 0.1));
    return vec4(gCol, gA);
}

void main() {
    gAA = uPx / uH * 0.85;
    gLW = uPx / uH * 2.2;
    vec2 q = (vP - uPos) / uH;
    q.x *= uFace;
    vec4 c = vec4(0.0);
    if (q.y >= -0.02) {
        c = panda(q);
        c.rgb = mix(c.rgb, uHazeC, uHaze);
    }
    // the reflection in a wet ground
    if (q.y < 0.0 && uReflect > 0.0) {
        vec2 r = vec2(q.x + 0.01 * sin(q.y * 60.0 + uBreath * 3.0), -q.y * 1.05);
        vec4 m = panda(r);
        c = vec4(m.rgb * 0.4, m.a * uReflect * exp(q.y * 3.5));
    }
    vec3 rgb = c.rgb * c.a;
    float a = c.a;
    float sh = exp(-sq(q.x / 0.3) - sq(q.y / 0.03)) * uShadow * (1.0 - 0.6 * sat(uHop * 10.0)) * (1.0 - a);
    a += sh * 0.5;
    frag = vec4(rgb, a) * uAlpha;
}
)";

// ------------------------------------------------------------------------------------------------ lights, butterflies, the gauge
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
    if (mode == 3) {
        // a butterfly: two pairs of wings flapping (param: the flap 0 shut .. 1 open), a body
        vec2 q = vLocal;
        float open = 0.25 + 0.75 * vC.x;
        vec2 w = vec2(abs(q.x) / max(open, 0.05), q.y);
        float up = length((w - vec2(0.42, 0.22)) * vec2(1.0, 1.15)) - 0.38;
        float lo = length((w - vec2(0.3, -0.25)) * vec2(1.0, 1.0)) - 0.25;
        float wing = min(up, lo);
        float body = length(q * vec2(4.0, 1.0)) - 0.32;
        float aa = px * 1.5 / max(open, 0.3);
        float wa = 1.0 - smoothstep(-aa, aa, wing);
        float ba = 1.0 - smoothstep(-px, px, body);
        vec3 col = c * (0.75 + 0.25 * smoothstep(0.6, 0.0, length(w - vec2(0.4, 0.2))));
        col = mix(col, c * 0.5, (1.0 - smoothstep(-0.06, -0.02, wing)) * 0.0);
        float edge = smoothstep(-0.08, -0.0, wing) * wa;
        col = mix(col, c * 0.45, edge * 0.6);
        col = mix(col, vec3(0.12, 0.1, 0.1), ba);
        float a = max(wa, ba) * vB.a;
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 4) {
        // a firefly / a light of hope: a bright core, a warm halo, gently twinkling
        float core = exp(-r * r * 60.0);
        float halo = exp(-r * r * 6.0) * 0.45;
        float v = (core + halo) * smoothstep(1.0, 0.75, r);
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    // a mote of dust
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
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = sdBox(w, vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = sdBox(w - (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.2, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.5 + exp(-abs(dw) * 3.0) * 0.08;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ the cells
// One clean, solid cell per block: a soft rounded tile in its piece's color, lit from above (a gentle gradient, a
// fine light edge on top, a slightly deeper base), a thin darker line of its own color around it. The ghost is the
// outline alone; a cleared cell shrinks softly away.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, -
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 block, 1 ghost), erase 0..1, glow, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vec2 wp = aA.xy + aPos * aA.z;
    vLocal = aPos * 0.5;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uBridge;
uniform vec3 uBridgeInk, uLightC;
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float shrink = 1.0 - 0.6 * smoothstep(0.0, 1.0, erase);
    vec2 q = vLocal / shrink;
    float d = sdBox(q, vec2(0.44 - 0.12)) - 0.12;
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(d + 0.03));
        float a = (ring * 0.75 + inside * 0.06) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // a gentle top light, a deeper base
    vec3 c = t * mix(0.82, 1.08, smoothstep(-0.45, 0.45, q.y));
    c = mix(c, mix(t, uLightC, 0.45) * 1.15, (1.0 - smoothstep(0.0, 0.05, abs(d + 0.06))) * smoothstep(0.1, 0.4, q.y) * 0.55);
    // its own thin darker edge
    c = mix(c, t * 0.55, 1.0 - smoothstep(0.0, 0.035 + aa, -d));
    c += t * vC.z * 0.6;
    c = mix(c, mix(t, vec3(1.0), 0.5), smoothstep(0.0, 0.6, erase) * 0.5);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * (1.0 - smoothstep(0.55, 1.0, erase));
    frag = vec4(c * a, a);
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
    float halo = smoothstep(0.15, 0.45, d) * uHaloA;
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
    c *= 1.0 - 0.3 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.8);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace lostpanda_shaders

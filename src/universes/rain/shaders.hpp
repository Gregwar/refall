#pragma once
// RAIN shaders: the rainy street at dusk behind everything (a procedural, rain-blurred city with its neon or
// lanterns, haze and bokeh, a cold light shaft, the far rain lit by the lights it falls through, the wet street
// mirroring it in puddles rippled by the drops, the well's dark slate glass with its faint grid, a vignette),
// instanced flat shapes (the near rain's streaks, splashes, drips, the glass case's edges and beads, the ghost and the
// HUD's marks), the porous sugar cubes (flat on the board, cubes seen up close in the foreground), and SDF type.

namespace rain_shaders {

// ---- The backdrop, in board units (y up), drawn once per frame on a fullscreen triangle.
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uRes;
uniform vec2 uViewC;
uniform float uUnitPx;
uniform float uTime;
uniform vec3 uSkyTop, uSkyLow, uFar, uNear, uWin, uMist, uStreet, uPanel;
uniform vec3 uNeon[4];
uniform int uStyle;          // 0 neon street, 1 flower market, 2 monsoon
uniform float uSat;          // the drop: colors intensified
uniform float uGlow;         // the lights' level (calm dim .. peak bright)
uniform vec3 uFlash;         // the soft lightning glow's color x level
uniform vec2 uFlashPos;      // where it glows (board units)
uniform float uRain;         // the rain's intensity 0..1
uniform float uSlant;        // dx per unit of fall
uniform float uRainT;        // the rain's clock (slowed in slow motion)
uniform float uWell;         // 1: the well's backing and the HUD's veil drawn
uniform vec3 uBridge;        // the bridge's color
uniform float uBridgeK;
uniform float uDim;
uniform float uStorm;        // the drop's storm (darker between the strikes)
uniform float uNeonGain;     // the neon signs surging and flickering in the storm
uniform float uLights;       // the setup / teardown: the street's lights coming on, going out (each its own threshold)
uniform float uSweepX;       // a colored light sweeping across the wet street
uniform vec3 uSweepCol;
uniform float uFlood;        // the storm: the street flooding (more water, stronger reflections)
uniform float uGust;         // the storm: sheets of rain sweeping across in gusts
uniform vec2 uSignPos[2];    // the two big blade signs over the heaps (board units): their light falls on the tiles
uniform vec3 uSignCol[2];
// the drop's grade: the whole street swung to another colour mood (a hue rotation in YIQ, which keeps the luminance,
// a saturation gain, a duotone wash from a shadow to a highlight colour, an exposure); identity outside the drop
uniform vec2 uGradeRot;      // cos, sin of the hue rotation
uniform float uGradeSat;
uniform vec3 uDuoLo, uDuoHi;
uniform float uDuoK;
uniform float uGradeExpo;
// the HUD's lines beside the well (board units): the lightning never lights the sky behind them
uniform vec4 uHudRect[24];
uniform int uHudN;
const float H = -8.2;        // the street's far edge (the sidewalk's line, the horizon of the reflections)

float h11(float x) { return fract(sin(x * 127.1) * 43758.5453); }
float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float box(vec2 p, vec2 c, vec2 hs, float soft) {
    vec2 d = abs(p - c) - hs;
    float s = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    return smoothstep(soft, -soft, s);
}
float sdBox(vec2 p, vec2 c, vec2 hs) {
    vec2 d = abs(p - c) - hs;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// The street, `blur` its softness (units; the reflections blur more). From back to front: the sky (lightning glows
// inside the clouds and silhouettes the skyline), the far skyline (windows as bokeh), the street's buildings (window
// grids lit in many colors, ledges, the variant's signs), the shop fronts at street level (a lit shop window with its
// shelves, a door, an awning, a sign board), the passers-by under their umbrellas, the two big blade signs over the
// heaps, bokeh, mist.
float hudShield(vec2 p) {
    float m = 0.0;
    if (dot(uFlash, uFlash) <= 0.0) return 1.0;
    for (int i = 0; i < uHudN; i++) {
        vec4 r = uHudRect[i];
        vec2 d = max(r.xy - p, p - r.zw);
        m = max(m, 1.0 - smoothstep(-0.5, 4.0, max(d.x, d.y)));
    }
    return 1.0 - 0.8 * m;
}
vec3 grade(vec3 c) {
    const mat3 toYiq = mat3(0.299, 0.596, 0.211, 0.587, -0.274, -0.523, 0.114, -0.322, 0.312);
    const mat3 toRgb = mat3(1.0, 1.0, 1.0, 0.956, -0.272, -1.106, 0.621, -0.647, 1.703);
    vec3 y = toYiq * c;
    y.yz = vec2(uGradeRot.x * y.y - uGradeRot.y * y.z, uGradeRot.y * y.y + uGradeRot.x * y.z) * uGradeSat;
    c = max(toRgb * y, 0.0);
    float l = y.x;
    vec3 m = mix(uDuoLo, uDuoHi, smoothstep(0.02, 0.45, l));
    vec3 duo = m * (l / max(dot(m, vec3(0.299, 0.587, 0.114)), 0.02));
    return mix(c, duo, uDuoK) * uGradeExpo;
}
vec3 city(vec2 p, float blur) {
    float t = clamp((p.y - H) / 24.0, 0.0, 1.0);
    vec3 col = mix(uSkyLow, uSkyTop, pow(t, 0.6));
    float cl = 0.5 + 0.5 * sin(p.x * 0.13 + uTime * 0.02 + sin(p.y * 0.3 + p.x * 0.05) * 1.5);
    col += uSkyLow * 0.22 * cl * smoothstep(22.0, 6.0, p.y - H);
    // lightning: a wide soft glow inside the clouds, brightest where it struck (the skyline stands out against it)
    vec3 fl = uFlash * (0.55 + exp(-abs(p.x - uFlashPos.x) * 0.05) * 0.8 + 0.8 * exp(-length((p - uFlashPos) * vec2(0.09, 0.12)))) * (0.6 + 0.4 * cl);
    col += fl * smoothstep(H + 2.0, H + 12.0, p.y) * hudShield(p);
    // the far skyline
    {
        float w = 2.3;
        float id = floor((p.x + 200.0) / w), fx = fract((p.x + 200.0) / w);
        float top = H + 9.0 + 9.0 * h11(id + 3.0);
        float m = smoothstep(blur + 0.12, -blur - 0.12, p.y - top) * smoothstep(-0.05, 0.2, fx) * smoothstep(1.0, 0.85, fx);
        vec3 b = mix(col, mix(uFar, uMist, 0.35), 0.62);
        vec2 g = vec2(0.62, 0.9);
        vec2 wid = floor(p / g), wf = fract(p / g) - 0.5;
        float lit = step(0.66, h21(wid + id * 1.7)) * step(p.y, top - 0.6) * step(h21(wid + id * 2.3 + 31.0), uLights);
        float bok = smoothstep(0.3 + blur * 0.5, 0.06, length(wf));
        vec3 wc = mix(uWin, uNeon[int(h21(wid + 4.0) * 3.99)], step(0.55, h21(wid + 2.0)) * 0.8);
        b += wc * lit * bok * (0.25 + 0.3 * h21(wid + 9.0)) * uGlow;
        col = mix(col, b, m);
    }
    // the street's buildings: facades with lit windows above the shops
    float shopTop = H + 3.6;
    {
        float w = 4.4;
        float id = floor((p.x + 300.0) / w), fx = fract((p.x + 300.0) / w);
        float top = H + 7.0 + 11.0 * h11(id * 1.31 + 11.0);
        float m = smoothstep(blur + 0.04, -blur - 0.04, p.y - top) * smoothstep(0.0, 0.03, fx) * smoothstep(1.0, 0.97, fx);
        vec3 fac = mix(uNear, uFar, 0.25 * h11(id + 1.0)) * (0.85 + 0.3 * h11(id + 6.0));
        vec3 b = mix(col, fac, 0.93);
        // windows: a grid per building, each lit or dark, warm or tinted, some with blinds
        vec2 g = vec2(0.9 + 0.3 * h11(id + 2.0), 1.25);
        vec2 lp = vec2(p.x + 300.0 - id * w, p.y - shopTop);
        vec2 wid = floor(lp / g), wf = fract(lp / g);
        float inside = step(0.4, lp.x) * step(lp.x, w - 0.4) * step(0.3, lp.y) * step(p.y, top - 0.6);
        float win = box(wf, vec2(0.5, 0.5), vec2(0.28, 0.32), 0.03 + blur * 0.4) * inside;
        float hl = h21(wid + id * 3.7);
        vec3 wc = hl < 0.45 ? uWin : uNeon[int(h21(wid + id) * 3.99)] * 0.8 + uWin * 0.2;
        float lit = step(0.35, h21(wid * 1.7 + id)) * (0.55 + 0.45 * h21(wid + 5.0)) * step(h21(wid + id * 2.9 + 17.0), uLights);
        float blinds = step(0.8, h21(wid + 13.0)) * (0.6 + 0.4 * step(0.5, fract(wf.y * 6.0)));
        vec3 wl = wc * lit * (1.0 - 0.5 * blinds) * (0.55 + 0.45 * smoothstep(0.0, 0.6, wf.y));
        b = mix(b, mix(fac * 0.5, wl * uGlow, lit), win * 0.95);
        // ledges between the floors
        b *= 1.0 - 0.25 * smoothstep(0.05, 0.0, abs(fract(lp.y / g.y) - 0.04)) * step(0.3, lp.y);
        float hs = h11(id * 7.7 + 2.0);
        int k = int(floor(h11(id * 3.3) * 4.0));
        if (uStyle == 0 && hs > 0.45) {
            // a vertical neon sign
            float sx = (id + 0.2 + 0.6 * h11(id + 5.0)) * w - 300.0;
            float y0 = shopTop + 0.6 + 2.0 * h11(id + 8.0), y1 = min(top - 0.6, y0 + 3.0 + 3.0 * hs);
            float sd = sdBox(p, vec2(sx, 0.5 * (y0 + y1)), vec2(0.3, 0.5 * (y1 - y0)));
            float core = smoothstep(0.07 + blur, 0.0, abs(sd + 0.1) - 0.035);
            float letters = step(0.35, fract((p.y - y0) * 1.3)) * smoothstep(0.0, -0.05, sd + 0.12);
            b = mix(b, uNeon[k] * 0.35, smoothstep(0.02, -0.02, sd));
            float on = uNeonGain * step(h11(id + 23.0), uLights);
            b += uNeon[k] * (core * 1.3 + letters * 0.8 + 0.5 * exp(-max(sd, 0.0) * 1.2)) * uGlow * on;
            col += uNeon[k] * 0.15 * exp(-max(sd, 0.0) * 0.7) * uGlow * (1.0 - m) * step(h11(id + 23.0), uLights);
            b += uNeon[k] * 0.1 * exp(-max(sd, 0.0) * 0.35) * uGlow * on;
        } else if (uStyle == 2 && hs > 0.3) {
            // monsoon: strings of paper lanterns across the facades
            float ly = shopTop + 1.5 + 4.0 * h11(id + 4.0);
            float sag = ly - 0.4 * sin(fx * 3.14159);
            vec2 lc = vec2(fract(p.x * 0.9) - 0.5, p.y - sag);
            float d = length(lc * vec2(1.0, 0.75));
            vec3 nc = uNeon[int(h11(floor(p.x * 0.9) + id) * 3.99)];
            b = mix(b, nc * (0.9 + 0.3 * smoothstep(0.2, 0.0, d)), smoothstep(0.24 + blur, 0.2, d));
            b += nc * 0.35 * exp(-d * 3.0) * uGlow;
            col += nc * 0.12 * exp(-d * 1.5) * uGlow * (1.0 - m);
        }
        col = mix(col, b, m);
    }
    // the shop fronts at street level
    if (p.y < shopTop + 0.4) {
        float w = 3.7;
        float id = floor((p.x + 500.0) / w), fx = fract((p.x + 500.0) / w);
        float lx = fx * w;
        vec3 fr = uNear * 0.7;
        vec3 b = fr;
        int k = int(floor(h11(id * 5.1) * 4.0));
        vec3 sc = uNeon[k];
        // the shop window: a warm lit interior, shelves and goods in silhouette
        float wx0 = 0.25, wx1 = 2.35, wy0 = H + 0.35, wy1 = H + 2.35;
        float win = box(vec2(lx, p.y), vec2(0.5 * (wx0 + wx1), 0.5 * (wy0 + wy1)), vec2(0.5 * (wx1 - wx0), 0.5 * (wy1 - wy0)), 0.02 + blur * 0.3);
        vec3 interior = mix(uWin, sc, 0.35 + 0.3 * h11(id + 3.0)) * (0.55 + 0.45 * smoothstep(wy0, wy1, p.y)) * uGlow *
                        mix(0.12, 1.0, step(h11(id + 41.0), uLights));
        float shelf = smoothstep(0.04, 0.0, abs(fract((p.y - wy0) / 0.6) - 0.1));
        float goods = step(0.45, h21(vec2(floor(lx * 4.0), floor((p.y - wy0) / 0.6)) + id)) * step(fract((p.y - wy0) / 0.6), 0.45) * step(0.15, fract((p.y - wy0) / 0.6));
        vec3 gc = mix(uNeon[int(h21(vec2(floor(lx * 4.0), id)) * 3.99)], vec3(1.0), 0.3) * 0.7 * uGlow;
        interior = mix(interior, interior * 0.35, shelf);
        interior = mix(interior, gc, goods * 0.7);
        b = mix(b, interior, win);
        // the door, dimly lit
        float door = box(vec2(lx, p.y), vec2(2.95, H + 1.15), vec2(0.38, 1.15), 0.02 + blur * 0.3);
        b = mix(b, mix(uWin, sc, 0.5) * 0.3 * uGlow, door);
        // the awning: striped in the shop's colors, scalloped
        float ay = p.y - (H + 2.75);
        float scal = 0.08 * abs(sin(lx * 7.0));
        float aw = smoothstep(blur + 0.03, -0.01, max(-ay - 0.32 + scal, ay - 0.42)) * step(0.1, lx) * step(lx, w - 0.1);
        float hasAw = uStyle == 1 ? 1.0 : step(0.4, h11(id + 9.0));
        vec3 stripe = mix(sc, vec3(0.95, 0.92, 0.86), step(0.5, fract(lx * 1.6)));
        b = mix(b, stripe * (0.45 + 0.55 * smoothstep(-0.3, 0.42, ay)) * (0.75 + 0.3 * uGlow), aw * hasAw);
        // the sign board with its glowing lettering
        float sy = H + 3.3;
        float board = box(vec2(lx, p.y), vec2(w * 0.5, sy), vec2(w * 0.45, 0.24), 0.02 + blur * 0.3);
        float glyph = step(0.42, h21(vec2(floor(lx * 5.0), id))) * box(vec2(fract(lx * 5.0), p.y), vec2(0.5, sy), vec2(0.32, 0.12), 0.03 + blur * 0.3) * step(0.6, lx) * step(lx, w - 0.6);
        b = mix(b, uNear * 0.4, board);
        b += sc * glyph * 1.2 * uGlow + sc * 0.25 * board * uGlow;
        // flower buckets in front of the market stalls
        if (uStyle == 1) {
            vec2 fc = vec2(fract(p.x * 2.4) - 0.5, p.y - (H + 0.45 + 0.2 * h11(floor(p.x * 2.4))));
            float fl = smoothstep(0.3 + blur, 0.12, length(fc * vec2(1.0, 1.3)));
            b = mix(b, uNeon[int(h11(floor(p.x * 2.4) + 3.0) * 3.99)] * (0.8 + 0.3 * uGlow), fl * step(lx, 2.6));
        }
        float m = step(p.y, shopTop) * smoothstep(-0.02, 0.02, shopTop - p.y);
        col = mix(col, b, m);
        // the sidewalk glows under the shop windows
        col += mix(uWin, sc, 0.4) * 0.12 * uGlow * exp(-(p.y - H) * 2.5) * step(H, p.y);
    }
    // passers-by under their umbrellas, walking slowly along the sidewalk
    for (int i = 0; i < 9; i++) {
        float fi = float(i);
        float dir = h11(fi * 3.7) < 0.5 ? -1.0 : 1.0;
        float sp = 0.25 + 0.3 * h11(fi * 1.3);
        float x = mod(h11(fi * 7.9) * 64.0 + dir * sp * uTime, 64.0) - 32.0;
        if (abs(p.x - x) > 1.0 || p.y > H + 2.6) continue;
        vec2 q = p - vec2(x, H + 0.05);
        float stride = sin(uTime * sp * 6.0 + fi) * 0.08;
        float body = sdBox(q, vec2(0.0, 0.95), vec2(0.16, 0.45)) - 0.06;
        float legs = min(sdBox(q, vec2(-0.07 + stride, 0.25), vec2(0.04, 0.25)), sdBox(q, vec2(0.07 - stride, 0.25), vec2(0.04, 0.25)));
        float head = length(q - vec2(0.0, 1.55)) - 0.13;
        float stick = sdBox(q, vec2(0.05, 1.75), vec2(0.012, 0.3));
        float ur = length((q - vec2(0.05, 1.95)) * vec2(1.0, 1.6)) - 0.62;
        float umb = max(ur, -(q.y - 1.92));
        float sil = min(min(body, legs), min(head, stick));
        vec3 uc = uNeon[int(h11(fi * 5.3) * 3.99)];
        col = mix(col, uNear * 0.35, smoothstep(blur + 0.02, -0.01, sil));
        vec3 umbC = uc * (0.55 + 0.35 * uGlow) * (0.7 + 0.3 * smoothstep(1.92, 2.3, q.y));
        umbC = mix(umbC, umbC * 1.3, step(0.5, fract(atan(q.x - 0.05, q.y - 1.92) * 1.6)));
        col = mix(col, umbC, smoothstep(blur + 0.02, -0.01, umb));
    }
    // the big blade signs over the heaps
    for (int i = 0; i < 2; i++) {
        float sd = sdBox(p, uSignPos[i], vec2(0.45, 2.6));
        vec3 c = uSignCol[i];
        float core = smoothstep(0.08 + blur, 0.0, abs(sd + 0.12) - 0.05);
        float letters = step(0.3, fract((p.y - uSignPos[i].y) * 0.9)) * smoothstep(0.0, -0.06, sd + 0.2) * step(abs(p.x - uSignPos[i].x), 0.18);
        col = mix(col, c * 0.25 + uNear * 0.3, smoothstep(0.03, -0.03, sd));
        col += c * (core * 1.4 + letters * 1.1) * uGlow + c * 0.45 * exp(-max(sd, 0.0) * 0.6) * uGlow + c * 0.14 * exp(-max(sd, 0.0) * 0.18) * uGlow;
    }
    // bokeh: lights out of focus, drifting a little
    {
        vec2 g = vec2(3.1, 2.7);
        vec2 bp = p + vec2(uTime * 0.05, 0.0);
        vec2 id = floor(bp / g), f = fract(bp / g) - 0.5;
        vec2 o = vec2(h21(id + 1.3), h21(id + 7.1)) - 0.5;
        float r = 0.28 + 0.25 * h21(id + 3.9);
        float d = length((f - o * 0.6) * g);
        float on = step(0.62, h21(id + 11.0)) * smoothstep(H + 1.0, H + 4.0, p.y);
        col += uNeon[int(h21(id + 5.0) * 3.99)] * on * 0.12 * smoothstep(r, r * 0.75, d) * uGlow;
    }
    if (blur < 0.2) {
        vec2 g = vec2(5.3, 4.6);
        vec2 bp = p + vec2(-uTime * 0.08, uTime * 0.02);
        vec2 id = floor(bp / g), f = fract(bp / g) - 0.5;
        vec2 o = vec2(h21(id + 2.3), h21(id + 8.1)) - 0.5;
        float r = 0.55 + 0.6 * h21(id + 4.9);
        float d = length((f - o * 0.5) * g);
        float on = step(0.55, h21(id + 13.0)) * smoothstep(H + 2.0, H + 6.0, p.y);
        float disc = smoothstep(r, r * 0.88, d) * (0.6 + 0.4 * smoothstep(r * 0.5, r * 0.95, d));
        col += mix(uNeon[int(h21(id + 6.0) * 3.99)], uWin, 0.25) * on * 0.07 * disc * uGlow;
    }
    // haze: the air thick with rain, glowing with the street's lights low down, softer in the distance
    col = mix(col, mix(uMist, uSkyLow, 0.35) * 1.1, (uStyle == 1 ? 0.5 : 1.0) * (0.12 + 0.1 * smoothstep(H + 4.0, H + 16.0, p.y)));
    col += mix(uMist, uWin, 0.3) * 0.08 * uGlow * exp(-abs(p.y - (H + 3.0)) * 0.35);
    // mist over the street
    col = mix(col, uMist, (uStyle == 2 ? 0.32 : uStyle == 1 ? 0.1 : 0.18) * exp(-(p.y - H) * 0.25));
    return col;
}

// Far rain: thin slanted streaks on a scrolling grid (two depths).
float rainLayer(vec2 p, float cw, float ch, float speed, float dens, float wpx) {
    vec2 q = vec2(p.x - uSlant * p.y, p.y + uRainT * speed);
    vec2 id = floor(q / vec2(cw, ch));
    vec2 f = fract(q / vec2(cw, ch));
    float h = h21(id);
    if (h > dens) return 0.0;
    float x = 0.15 + 0.7 * h21(id + 7.1);
    float y0 = 0.6 * h21(id + 3.3);
    float dx = abs(f.x - x) * cw * uUnitPx;
    float along = (f.y - y0) / 0.35;
    if (along < 0.0 || along > 1.0) return 0.0;
    return smoothstep(wpx, 0.0, dx) * along * (0.6 + 0.4 * h21(id + 1.7));
}

void main() {
    vec2 pix = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
    vec2 p = uViewC + vec2(pix.x - uRes.x * 0.5, uRes.y * 0.5 - pix.y) / uUnitPx;
    vec3 col;
    float blur = 0.12;
    if (p.y >= H) {
        col = city(p, blur);
    } else {
        // the wet street: puddles mirror the city, rippled by the drops
        float depth = (H - p.y);
        float pud = smoothstep(-0.2, 0.5, sin(p.x * 0.43 + 1.7 * sin(p.y * 0.9)) * 0.6 + sin(p.x * 1.13 - p.y * 0.7) * 0.4);
        vec2 cell = floor(vec2(p.x * 1.2, p.y * 3.0));
        vec2 cf = fract(vec2(p.x * 1.2, p.y * 3.0)) - 0.5;
        float ph = fract(uRainT * 0.9 + h21(cell));
        float ring = length(cf * vec2(1.0, 0.45)) - ph * 0.5;
        float rip = sin(ring * 40.0) * exp(-abs(ring) * 18.0) * (1.0 - ph) * step(h21(cell + 5.0), 0.25 + 0.6 * uRain + 0.3 * uFlood);
        // the reflections stretched down the wet street (lights become long streaks), blurred with the depth
        vec2 q = vec2(p.x + rip * 0.08 + 0.05 * sin(p.y * 7.0 + uTime * 0.7), H + depth * 2.2 + 0.4 + rip * 0.1);
        vec3 refl = city(q, 0.3 + depth * 0.1);
        float k = mix(mix(0.4, 0.8, pud), 0.95, uFlood);
        col = mix(uStreet, refl * 0.9, k);
        // the storm: a colored light sweeping across the wet street
        col += uSweepCol * 0.45 * uStorm * exp(-pow((p.x - uSweepX) * 0.22, 2.0)) * (0.4 + 0.6 * k);
        col += vec3(0.5) * max(rip, 0.0) * 0.12 * uRain;
        col *= mix(1.0, 0.75, smoothstep(0.0, 4.5, depth));
        // the curb's soft line at the far edge
        col = mix(col, mix(uMist, uSkyLow, 0.4) * 0.8, exp(-depth * 6.0) * 0.6);
    }
    // the storm: dark and moody between the strikes; a strike lights the whole street from the sky
    col *= 1.0 - 0.28 * uStorm;
    col += uFlash * 0.12 * (0.5 + 0.5 * clamp((p.y - H) / 20.0, 0.0, 1.0)) * hudShield(p);
    // saturation boost (the drop)
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = max(mix(vec3(l), col, uSat), 0.0);
    // the far rain
    float wpx = 1.1;
    float r = rainLayer(p, 0.33, 2.6, 21.0, 0.25 + 0.55 * uRain, wpx) * 0.55 + rainLayer(p + 13.0, 0.21, 1.9, 15.0, 0.15 + 0.5 * uRain, wpx) * 0.35;
    // the storm: sheets of rain sweeping across in gusts
    float gust = mix(1.0, 0.35 + 1.6 * smoothstep(-0.2, 0.9, sin(p.x * 0.11 + p.y * 0.04 + uRainT * 1.7) * 0.7 + sin(p.x * 0.05 - uRainT * 0.6) * 0.3), uGust);
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col += mix(uMist, vec3(0.9), 0.5) * r * (0.35 + 0.4 * uRain) * gust * (0.6 + 2.2 * lum) + col * r * 0.6 * lum;
    col += mix(uMist, vec3(0.8), 0.4) * 0.07 * uGust * smoothstep(0.6, 1.4, gust); // the veil of a passing sheet
    // a cold light from high above the well, a shaft through the rain (the rain lit in it)
    {
        float top = uViewC.y + 13.0;
        float dd = top - p.y;
        float w = 2.2 + dd * 0.32;
        float shaft = exp(-pow(p.x / w, 2.0)) * smoothstep(26.0, 2.0, dd);
        vec3 cool = mix(vec3(0.55, 0.7, 1.0), uMist, 0.3);
        col += cool * shaft * (0.1 + 0.25 * r) * uGlow * (1.0 - 0.5 * uStorm);
    }
    // the drop: the whole street swung to its colour mood (before the well's glass and the HUD's veil)
    col = grade(col);
    if (uWell > 0.5) {
        // the HUD columns veiled a little, the well's backing dark glass
        vec2 lc = vec2(-8.6, 1.8), rc = vec2(8.6, 3.4);
        float veil = max(box(p, lc, vec2(2.8, 8.8), 1.2), box(p, rc, vec2(2.8, 7.2), 1.2));
        col = mix(col, uPanel * 1.4, veil * 0.42);
        float pan = box(p, vec2(0.0, 0.0), vec2(5.05, 10.05), 0.5 / uUnitPx);
        float soft = box(p, vec2(0.0, 0.0), vec2(5.3, 10.3), 0.6);
        col *= 1.0 - 0.35 * soft;
        float gy = clamp((p.y + 10.0) / 20.0, 0.0, 1.0);
        vec3 slate = mix(uMist, vec3(0.45, 0.52, 0.66), 0.6);
        vec3 panel = uPanel * mix(0.8, 1.15, gy) + uNeon[0] * 0.025 * smoothstep(-10.0, -4.0, -p.y) + slate * mix(0.07, 0.13, gy);
        // a faint grid etched in the glass, a soft reflection sweeping across it
        vec2 gc = abs(fract(p + vec2(0.0, 0.0)) - 0.5);
        float grid = smoothstep(0.5 - 1.3 / uUnitPx, 0.5 - 0.2 / uUnitPx, max(gc.x, gc.y)) * step(abs(p.x), 4.98) * step(abs(p.y), 9.98);
        panel += vec3(0.6, 0.66, 0.82) * 0.055 * grid;
        panel += mix(uNeon[1], vec3(1.0), 0.5) * 0.018 * smoothstep(1.2, 0.0, abs(p.x * 0.8 + p.y * 0.35 - 2.0));
        col = mix(col, panel + col * 0.04, pan);
    }
    // the lens: a soft vignette
    {
        vec2 v = (vUv - 0.5) * vec2(1.0, 0.85);
        col *= 1.0 - 0.38 * smoothstep(0.32, 0.75, length(v));
    }
    col = mix(col, uBridge, uBridgeK);
    col *= 1.0 - uDim;
    frag = vec4(col, 1.0);
}
)";

// Instanced shapes in pixels (top-left origin): a rounded box (or its outline), a disc / ellipse, a capsule
// segment whose alpha fades toward its tail (a rain streak).
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // -1..1
layout(location = 1) in vec4 aGeom;     // box / disc: center, half sizes; segment: head x, y, tail x, y
layout(location = 2) in vec4 aCol;      // sRGB, straight alpha
layout(location = 3) in vec4 aKind;     // kind (0 box, 1 disc, 2 segment), radius / width, outline, tail alpha
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vGeom, vCol, vKind;
void main() {
    vec2 p;
    if (aKind.x > 1.5) {
        vec2 a = aGeom.xy, b = aGeom.zw;
        vec2 c = 0.5 * (a + b);
        vec2 d = b - a;
        float L = max(length(d), 0.001);
        vec2 u = d / L, n = vec2(-u.y, u.x);
        float w = aKind.y + 1.5;
        p = c + u * aCorner.x * (0.5 * L + w) + n * aCorner.y * w;
    } else {
        float b = max(aGeom.z, aGeom.w) + 1.5;
        p = aGeom.xy + aCorner * vec2(aGeom.z + 1.5, aGeom.w + 1.5);
    }
    vPix = p;
    vGeom = aGeom; vCol = aCol; vKind = aKind;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vGeom, vCol, vKind;
out vec4 frag;
void main() {
    float dist, a = vCol.a;
    if (vKind.x < 0.5) {
        vec2 h = vGeom.zw;
        float r = min(vKind.y, min(h.x, h.y));
        vec2 q = abs(vPix - vGeom.xy) - h + r;
        dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
        if (vKind.z > 0.0) dist = abs(dist + vKind.z * 0.5) - vKind.z * 0.5;
    } else if (vKind.x < 1.5) {
        vec2 q = (vPix - vGeom.xy) / vGeom.zw;
        float l = length(q);
        dist = (l - 1.0) * min(vGeom.z, vGeom.w);
        a *= mix(1.0, smoothstep(1.0, 0.0, l), vKind.w); // soft: a glow
    } else {
        vec2 pa = vPix - vGeom.xy, ba = vGeom.zw - vGeom.xy;
        float t = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-4), 0.0, 1.0);
        dist = length(pa - ba * t) - vKind.y * 0.5;
        a *= mix(1.0, vKind.w, t);
    }
    float cov = clamp(0.5 - dist, 0.0, 1.0);
    a *= cov;
    frag = vec4(vCol.rgb * a, a);
}
)";

// Soft porous sugar cubes: one rounded tile per cell, a matte grainy body in the piece's color (pores, a few
// glinting crystals), soft bevelled edges lit from above, the street's neon caught faintly, a film of water along the
// top edge, beads of water and a rivulet sliding down the face (both with the tile's wetness). A foreground tile is a
// cube seen from close: the faces its depth reveals toward the vanishing point, and as it melts it sags, crumbles
// (holes eaten into it from the top and the edges, a dark wet rim around them) and slumps.
inline const char* TILE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aGeom;   // center x, y (pixels), half size x, y (pixels)
layout(location = 2) in vec4 aCol;    // sRGB body color, alpha
layout(location = 3) in vec4 aP;      // seed, wetness, plain (preview: no water), angle (radians, counterclockwise)
layout(location = 4) in vec4 aE;      // the cube's depth (pixels, toward the vanishing point), melt (0..1), foreground (1)
layout(location = 5) in vec4 aF;      // -, outside (1: a heaped tile, lit by the street), soggy (0 firm .. 1 pulp)
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vGeom, vCol, vP, vE, vF;
void main() {
    float ext = max(aGeom.z, aGeom.w) * (aE.w > 0.5 ? 1.75 : 1.42) + length(aE.xy) + 1.5;
    vec2 p = aGeom.xy + 0.5 * aE.xy + aCorner * ext;
    vPix = p;
    vGeom = aGeom; vCol = aCol; vP = aP; vE = aE; vF = aF;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* TILE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vGeom, vCol, vP, vE, vF;
out vec4 frag;
uniform float uTime;
uniform vec2 uLightPx[2];  // the blade signs (pixels): they light the heaped tiles
uniform vec3 uLightCol[2];
uniform float uLightR;     // their reach (pixels)
uniform float uGroundPx;   // the street (pixels): the heaps darken toward it
uniform vec3 uFlashRim;    // a lightning strike: a rim of cold light on the tiles' upper edges
uniform vec3 uSheen;   // the street's light caught on the body
uniform vec3 uSheen2;
uniform float uGlow;
float h(float x) { return fract(sin(x * 91.3458) * 47453.5453); }
float h2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h2(i), h2(i + vec2(1.0, 0.0)), f.x), mix(h2(i + vec2(0.0, 1.0)), h2(i + vec2(1.0, 1.0)), f.x), f.y);
}
float ca, sa;
vec2 toLocal(vec2 v) { return vec2(ca * v.x - sa * v.y, sa * v.x + ca * v.y); }
vec2 toScreen(vec2 v) { return vec2(ca * v.x + sa * v.y, -sa * v.x + ca * v.y); }
float sdR(vec2 l, vec2 hs, float r) {
    vec2 b = abs(l) - hs + r;
    return length(max(b, 0.0)) + min(max(b.x, b.y), 0.0) - r;
}
void main() {
    vec2 hs = vGeom.zw;
    vec2 d0 = vPix - vGeom.xy;
    ca = cos(vP.w); sa = sin(vP.w);
    bool fore = vE.w > 0.5;
    float melt = vE.z;
    vec2 o = vE.xy;
    float soak = vF.z;
    vec2 qs = d0 / hs;                       // screen frame: the paint runs down the screen whatever the tile's turn
    vec2 d;
    float sd, tDepth = 0.0;
    int face = 0;                            // 0 the front face, 1 a face its depth reveals
    vec2 nFace = vec2(0.0);                  // that face's normal (screen, y down)
    float r = (0.2 + 0.3 * soak) * min(hs.x, hs.y);
    if (fore) {
        // the mush sags toward the ground (screen space): the top caves in, the base spreads, the edges go uneven
        if (soak > 0.0) {
            vec2 qn = clamp(qs, -1.0, 1.0);
            d0.x /= 1.0 + 0.16 * soak * (0.5 + 0.5 * qn.y);
            d0.y -= soak * 0.3 * hs.y * (1.0 - qn.x * qn.x) * smoothstep(0.4, -1.0, qn.y);
            d0 += soak * 0.08 * hs * (vec2(vnoise(qn * 3.0 + vP.x * 9.0), vnoise(qn * 3.0 - vP.x * 7.0)) - 0.5);
        }
        // the cube: the front face swept toward the vanishing point (its distance: the nearest point of the sweep)
        float lo = 0.0, hi = 1.0;
        for (int i = 0; i < 9; i++) {
            float m1 = mix(lo, hi, 0.382), m2 = mix(lo, hi, 0.618);
            if (sdR(toLocal(d0 - m1 * o), hs, r) < sdR(toLocal(d0 - m2 * o), hs, r)) hi = m2; else lo = m1;
        }
        sd = sdR(toLocal(d0 - 0.5 * (lo + hi) * o), hs, r);
        d = toLocal(d0);
        if (sdR(d, hs, r) > 0.0 && length(o) > 0.5) {
            // where the line of sight enters the front face's box going back: the face it sees
            vec2 L0 = d, D = -toLocal(o);
            float tE = -1e9, tX = 1e9;
            int ax = 0;
            for (int i = 0; i < 2; i++) {
                float di = D[i];
                if (abs(di) < 1e-4) { if (abs(L0[i]) > hs[i]) tE = 1e9; continue; }
                float t1 = (-hs[i] - L0[i]) / di, t2 = (hs[i] - L0[i]) / di;
                float tn = min(t1, t2), tf = max(t1, t2);
                if (tn > tE) { tE = tn; ax = i; }
                tX = min(tX, tf);
            }
            if (tE <= tX && tE > -0.05 && tE < 1.05) {
                face = 1;
                tDepth = clamp(tE, 0.0, 1.0);
                vec2 n = vec2(0.0);
                n[ax] = -sign(D[ax]);
                nFace = toScreen(n);
                vec2 lp = L0 + tE * D;
                // the face's own coordinates: across its edge, and its depth
                d = ax == 0 ? vec2(tDepth * 2.0 - 1.0, lp.y / hs.y) * hs : vec2(lp.x / hs.x, tDepth * 2.0 - 1.0) * hs;
            }
        }
    } else {
        d = toLocal(d0); // into the tile's frame (a heaped tile is turned)
        if (soak > 0.0) {
            // soggy: the top sags in the middle, the sides bulge, the edges go soft and uneven
            vec2 qn = clamp(d / hs, -1.0, 1.0);
            d.x /= 1.0 + 0.12 * soak * (1.0 - qn.y * qn.y);
            d.y -= soak * 0.3 * hs.y * (1.0 - qn.x * qn.x) * smoothstep(0.4, -1.0, qn.y);
            d += soak * 0.05 * hs * (vec2(vnoise(qn * 3.0 + vP.x * 9.0), vnoise(qn * 3.0 - vP.x * 7.0)) - 0.5);
        }
        sd = sdR(d, hs, r);
    }
    float cov = clamp(0.5 - sd, 0.0, 1.0);
    if (cov <= 0.0) discard;
    vec2 q = d / hs;                         // -1..1 on its face, y down
    float sdF = face == 0 ? sdR(d, hs, r) : -min(hs.x, hs.y);
    vec3 base = vCol.rgb;
    bool outside = vF.y > 0.5;
    float s = vP.x;
    if (outside || fore) {
        // the rain soaks it: dark water stains spread down from the top with a tide line at their edge, and the soggy
        // part goes dark and dull
        float n = vnoise(qs * 2.3 + s) * 0.65 + vnoise(qs * 5.1 - s) * 0.35;
        float st = soak * 2.4 - (qs.y + 1.0) * 0.6 - n * (fore ? 0.3 : 0.8);
        float stain = smoothstep(-0.05, 0.08, st);
        float tide = smoothstep(0.1, 0.0, abs(st + 0.03)) * step(0.02, soak) * (1.0 - smoothstep(0.85, 1.0, soak));
        vec3 dull = mix(vec3(dot(base, vec3(0.299, 0.587, 0.114))), base, fore ? 1.05 : 0.85) * (fore ? 0.78 : 0.8);
        base = mix(base, dull, stain * (fore ? 0.6 : 1.0));
        base *= 1.0 - 0.25 * tide;
    }
    // the light from above: the body darker low on the face, soft bevelled edges (lit on top, shaded below)
    vec3 c = base * mix(1.08, 0.86, (q.y + 1.0) * 0.5);
    vec2 L = normalize(vec2(-0.4, -1.0));
    if (face == 0) {
        float bw = 0.24 * min(hs.x, hs.y);
        float e = smoothstep(-bw, 0.0, sdF);
        vec2 nb = normalize(sign(q) * pow(abs(q) + 1e-3, vec2(6.0)) + vec2(1e-5));
        c *= 1.0 + e * (0.34 * dot(nb, L) - 0.06) * (1.0 - 0.6 * soak);
    } else {
        // a face its depth reveals: the top one in the light, the sides in the shade, darker going back
        c *= (0.62 + 0.42 * max(dot(nFace, L), 0.0)) * (1.0 - 0.22 * tDepth);
        c += (uSheen * 0.06 * uGlow) * max(-nFace.y, 0.0);
    }
    // the porous body: a grain, pores, a few glinting sugar crystals (the grain grows with the tile: coarser up close)
    {
        vec2 g = q + vec2(float(face) * 3.7, s * 0.37);
        float grain = vnoise(g * 4.0 + s) * 0.55 + vnoise(g * 9.0 - s) * 0.45;
        float pore = smoothstep(0.8, 0.88, vnoise(g * 10.0 + s * 2.3)) + 0.7 * smoothstep(0.84, 0.92, vnoise(g * 17.0 - s * 1.3));
        c *= 0.95 + 0.1 * grain;
        c *= 1.0 - 0.14 * min(pore, 1.0);
        float glint = step(0.975, h2(floor(g * 11.0 + s))) * smoothstep(0.5, 0.2, length(fract(g * 11.0 + s) - 0.5));
        c += vec3(1.0) * glint * 0.18 * (1.0 - soak);
    }
    // the sheen: a faint diagonal band of the street's neon (the body is matte)
    float band = exp(-pow((q.x * 0.55 + q.y * 0.85 + 0.45) * 2.6, 2.0));
    float band2 = exp(-pow((q.x * 0.7 - q.y * 0.4 - 0.6) * 4.0, 2.0));
    float wet = vP.y;
    float gloss = 1.0 - 0.8 * soak;          // a soggy tile has no shine left
    if (face == 0) c += (uSheen * band * (0.08 + 0.12 * wet) * uGlow + uSheen2 * band2 * 0.05 * uGlow) * gloss;
    wet *= gloss;
    if (vP.z < 0.5 && face == 0) {
        // the water film along the top edge
        float film = smoothstep(-0.98, -0.86, q.y) * smoothstep(-0.7, -0.84, q.y) * (1.0 - smoothstep(0.75, 0.95, abs(q.x)));
        c += vec3(1.0) * film * (0.08 + 0.26 * wet);
        // beads of water: a darker lens with a bright point
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            vec2 bc = vec2(h(s + fi * 3.1) * 1.3 - 0.65, h(s + fi * 5.7 + 1.0) * 1.2 - 0.5);
            float br = 0.07 + 0.06 * h(s + fi * 9.1);
            float l = length((q - bc) / vec2(1.0, 1.15));
            float m = smoothstep(br, br - 0.03, l) * smoothstep(0.15, 0.6, wet + 0.2 * h(s + fi));
            vec3 bead = c * 0.78 + vec3(1.0) * 0.6 * smoothstep(br * 0.45, 0.0, length(q - bc + vec2(br * 0.35, br * 0.4)));
            bead += base * 0.25 * smoothstep(br * 0.2, br, l) * step(0.0, q.y - bc.y);
            c = mix(c, bead, m);
        }
        // a rivulet sliding down the face now and then
        float rx = h(s + 17.0) * 1.2 - 0.6;
        float sp = 0.25 + 0.25 * h(s + 23.0);
        float head = mod(uTime * sp + h(s + 29.0) * 4.0, 4.0) - 1.2;      // -1.2..2.8 (on the face -1..1)
        float on = step(0.45, h(s + 31.0)) * smoothstep(0.2, 0.7, wet);
        float along = clamp((head - q.y) / 0.9, 0.0, 1.0) * step(q.y, head) * step(-0.95, q.y);
        float line = smoothstep(0.07, 0.0, abs(q.x - rx - 0.04 * sin(q.y * 5.0 + s))) * (1.0 - along * 0.6) * step(0.0, along);
        float tip = smoothstep(0.11, 0.0, length(vec2(q.x - rx, (q.y - head) * 0.8)));
        c = mix(c, c * 0.8 + vec3(0.55), (line * 0.3 * float(along > 0.0 || abs(q.y - head) < 0.1) + tip * 0.6) * on);
    }
    if (vP.z < 0.5) {
        float up = smoothstep(-0.55, -0.95, qs.y) + 0.5 * smoothstep(-0.5, -0.95, qs.x) * smoothstep(0.2, -0.6, qs.y);
        c += uFlashRim * (0.25 + 0.75 * smoothstep(-0.2 * hs.x, 0.0, sd)) * up * 0.9;
    }
    float a = vCol.a * cov;
    if (fore && melt > 0.0) {
        // melting: holes eaten in from the top and the edges, a dark wet rim around them, the body going to mush
        vec2 qu = d / hs;
        float ero = vnoise(qu * 2.1 + s * 5.0) * 0.7 + vnoise(qu * 4.6 - s * 3.0) * 0.3;
        float edgeF = max(abs(qu.x), abs(qu.y));
        edgeF *= edgeF * edgeF;
        float topF = smoothstep(0.2, -1.0, qs.y);
        float thr = (melt - 0.25) * 1.5 * (0.25 + 0.75 * edgeF + 0.5 * topF);
        a *= smoothstep(thr - 0.035, thr + 0.035, ero);
        c *= 1.0 - 0.5 * smoothstep(thr + 0.16, thr, ero) * step(0.0, thr);
        c = mix(c, c * 0.8, smoothstep(0.15, 0.7, melt));
        c += vec3(1.0) * 0.06 * smoothstep(0.3, 0.9, melt) * smoothstep(0.7, 0.9, vnoise(qu * 10.0 + uTime * 0.3)); // wet glints on the mush
    }
    if (outside) {
        // lit by the street: darker in the shade of the heap, the blade signs' light spilling on it, a rim on the
        // side facing each sign
        vec3 lit = vec3(0.0);
        for (int i = 0; i < 2; i++) {
            vec2 toL = uLightPx[i] - vGeom.xy;
            float dist = length(toL);
            vec2 dl = toL / max(dist, 1.0);
            float fall = 1.0 / (1.0 + pow(dist / uLightR, 2.0));
            float facing = clamp(dot(normalize(d0 + vec2(1e-3)), dl), 0.0, 1.0) * smoothstep(0.55, 0.95, max(abs(qs.x), abs(qs.y)));
            lit += uLightCol[i] * fall * (0.3 + 1.6 * facing);
        }
        float shade = mix(0.55, 1.0, clamp((uGroundPx - vPix.y) / (8.0 * hs.y), 0.0, 1.0));
        c = c * shade * 0.62 + c * lit * 0.5 + lit * 0.08;
    }
    frag = vec4(c * a, a);
}
)";


} // namespace rain_shaders

#pragma once
// RAIN shaders: the rainy street at dusk behind everything (a procedural, rain-blurred city with its neon or
// lanterns, the far rain, the wet street mirroring it in puddles rippled by the drops, the well's dark backing),
// instanced flat shapes (the near rain's streaks, splashes, drips, the ghost and the HUD's marks), the wet glazed
// tiles, and SDF type.

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
uniform float uFlood;        // the storm: the street flooding (more water, stronger reflections)
uniform float uGust;         // the storm: sheets of rain sweeping across in gusts
uniform vec2 uSignPos[2];    // the two big blade signs over the heaps (board units): their light falls on the tiles
uniform vec3 uSignCol[2];
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
vec3 city(vec2 p, float blur) {
    float t = clamp((p.y - H) / 24.0, 0.0, 1.0);
    vec3 col = mix(uSkyLow, uSkyTop, pow(t, 0.6));
    float cl = 0.5 + 0.5 * sin(p.x * 0.13 + uTime * 0.02 + sin(p.y * 0.3 + p.x * 0.05) * 1.5);
    col += uSkyLow * 0.22 * cl * smoothstep(22.0, 6.0, p.y - H);
    // lightning: a wide soft glow inside the clouds, brightest where it struck (the skyline stands out against it)
    vec3 fl = uFlash * (exp(-abs(p.x - uFlashPos.x) * 0.05) * 0.8 + 0.6 * exp(-length((p - uFlashPos) * vec2(0.09, 0.12)))) * (0.6 + 0.4 * cl);
    col += fl * smoothstep(H + 2.0, H + 12.0, p.y);
    // the far skyline
    {
        float w = 2.3;
        float id = floor((p.x + 200.0) / w), fx = fract((p.x + 200.0) / w);
        float top = H + 9.0 + 9.0 * h11(id + 3.0);
        float m = smoothstep(blur + 0.12, -blur - 0.12, p.y - top) * smoothstep(-0.05, 0.2, fx) * smoothstep(1.0, 0.85, fx);
        vec3 b = mix(col, uFar, 0.78);
        vec2 g = vec2(0.62, 0.9);
        vec2 wid = floor(p / g), wf = fract(p / g) - 0.5;
        float lit = step(0.66, h21(wid + id * 1.7)) * step(p.y, top - 0.6);
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
        float lit = step(0.35, h21(wid * 1.7 + id)) * (0.55 + 0.45 * h21(wid + 5.0));
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
            b += uNeon[k] * (core * 1.3 + letters * 0.8 + 0.5 * exp(-max(sd, 0.0) * 1.2)) * uGlow;
            col += uNeon[k] * 0.15 * exp(-max(sd, 0.0) * 0.7) * uGlow * (1.0 - m);
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
        vec3 interior = mix(uWin, sc, 0.35 + 0.3 * h11(id + 3.0)) * (0.55 + 0.45 * smoothstep(wy0, wy1, p.y)) * uGlow;
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
        col += c * (core * 1.4 + letters * 1.1) * uGlow + c * 0.45 * exp(-max(sd, 0.0) * 0.6) * uGlow;
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
        col += vec3(0.5) * max(rip, 0.0) * 0.12 * uRain;
        col *= mix(1.0, 0.75, smoothstep(0.0, 4.5, depth));
        // the curb's soft line at the far edge
        col = mix(col, mix(uMist, uSkyLow, 0.4) * 0.8, exp(-depth * 6.0) * 0.6);
    }
    // saturation boost (the drop)
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = max(mix(vec3(l), col, uSat), 0.0);
    // the far rain
    float wpx = 1.1;
    float r = rainLayer(p, 0.33, 2.6, 21.0, 0.25 + 0.55 * uRain, wpx) * 0.55 + rainLayer(p + 13.0, 0.21, 1.9, 15.0, 0.15 + 0.5 * uRain, wpx) * 0.35;
    // the storm: sheets of rain sweeping across in gusts
    float gust = mix(1.0, 0.35 + 1.6 * smoothstep(-0.2, 0.9, sin(p.x * 0.11 + p.y * 0.04 + uRainT * 1.7) * 0.7 + sin(p.x * 0.05 - uRainT * 0.6) * 0.3), uGust);
    col += mix(uMist, vec3(0.9), 0.5) * r * (0.35 + 0.4 * uRain) * gust;
    col += mix(uMist, vec3(0.8), 0.4) * 0.07 * uGust * smoothstep(0.6, 1.4, gust); // the veil of a passing sheet
    if (uWell > 0.5) {
        // the HUD columns veiled a little, the well's backing dark glass
        vec2 lc = vec2(-8.6, 1.8), rc = vec2(8.6, 3.4);
        float veil = max(box(p, lc, vec2(2.8, 8.8), 1.2), box(p, rc, vec2(2.8, 7.2), 1.2));
        col = mix(col, uPanel * 1.4, veil * 0.42);
        float pan = box(p, vec2(0.0, 0.0), vec2(5.05, 10.05), 0.5 / uUnitPx);
        float soft = box(p, vec2(0.0, 0.0), vec2(5.3, 10.3), 0.6);
        col *= 1.0 - 0.35 * soft;
        float gy = clamp((p.y + 10.0) / 20.0, 0.0, 1.0);
        vec3 panel = uPanel * mix(0.8, 1.15, gy) + uNeon[0] * 0.025 * smoothstep(-10.0, -4.0, -p.y);
        col = mix(col, panel + col * 0.015, pan);
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

// Wet glazed ceramic tiles: one rounded tile per cell, a deep glaze in the piece's color, the light pooling at
// the rim, the street's neon caught in a soft sheen, a film of water along the top edge, beads of water and a
// rivulet sliding down the face (both with the tile's wetness).
inline const char* TILE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aGeom;   // center x, y (pixels), half size x, y (pixels)
layout(location = 2) in vec4 aCol;    // sRGB glaze, alpha
layout(location = 3) in vec4 aP;      // seed, wetness, plain (preview: no water), angle (radians, counterclockwise)
layout(location = 4) in vec4 aE;      // a heaped tile: the paint dripping onto it from above (rgb, how far it ran)
layout(location = 5) in vec4 aF;      // its paint left (0 bare .. 1), outside (1: a heaped tile, lit by the street)
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vGeom, vCol, vP, vE, vF;
void main() {
    vec2 p = aGeom.xy + aCorner * (max(aGeom.z, aGeom.w) * 1.42 + 1.0);
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
uniform vec3 uSheen;   // the street's light caught in the glaze
uniform vec3 uSheen2;
uniform float uGlow;
float h(float x) { return fract(sin(x * 91.3458) * 47453.5453); }
float h2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h2(i), h2(i + vec2(1.0, 0.0)), f.x), mix(h2(i + vec2(0.0, 1.0)), h2(i + vec2(1.0, 1.0)), f.x), f.y);
}
void main() {
    vec2 hs = vGeom.zw;
    vec2 d0 = vPix - vGeom.xy;
    float ca = cos(vP.w), sa = sin(vP.w);
    vec2 d = vec2(ca * d0.x - sa * d0.y, sa * d0.x + ca * d0.y); // into the tile's frame (a heaped tile is turned)
    vec2 q = d / hs;                         // -1..1, y down
    float r = 0.2 * min(hs.x, hs.y);
    vec2 b = abs(d) - hs + r;
    float sd = length(max(b, 0.0)) + min(max(b.x, b.y), 0.0) - r;
    float cov = clamp(0.5 - sd, 0.0, 1.0);
    if (cov <= 0.0) discard;
    vec3 base = vCol.rgb;
    bool outside = vF.y > 0.5;
    vec2 qs = d0 / hs;                       // screen frame: the paint runs down the screen whatever the tile's turn
    if (outside) {
        // The rain washes the paint off, alive: rivulets of paint of their own width, speed and meander run down the
        // face (a darker trail behind a bead of paint that flows over the bottom edge), leaving bare ceramic along
        // their paths as the tile washes; blotchy bare patches spread from the top. Paint from the tile above lands
        // as spots and runs of its color.
        float wash = 1.0 - vF.x;                     // 0 freshly painted .. 1 bare
        float sd0 = vP.x;
        vec3 bare = vec3(0.84, 0.83, 0.8);
        vec3 paintC = base;
        // blotches from the top
        float n = vnoise(qs * 2.1 + sd0) * 0.65 + vnoise(qs * 4.7 - sd0) * 0.35;
        float top = wash * 2.2 - (qs.y + 1.0) * 0.55 - n * 0.9;
        float bareM = smoothstep(-0.04, 0.06, top);
        float runs = 0.0;
        vec3 runC = paintC;
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float thr = i == 0 ? 0.0 : h(sd0 + fi * 4.1) * 0.7 + fi * 0.04;   // when this rivulet starts (by wash)
            if (wash < thr - 0.02 && i > 1) continue;
            float x0 = h(sd0 + fi * 7.3) * 1.6 - 0.8;
            float w = 0.04 + 0.08 * h(sd0 + fi * 2.9);
            float v = 0.18 + 0.35 * h(sd0 + fi * 5.3);
            float path = x0 + 0.07 * sin(qs.y * 3.1 + sd0 + fi) + 0.035 * sin(qs.y * 7.7 + fi * 2.0);
            float dx = abs(qs.x - path);
            // the bead flowing down (and over the edge), the trail behind it
            float head = -1.15 + mod(uTime * v + h(sd0 + fi * 9.1) * 3.0, 2.6);
            float trail = step(qs.y, head) * step(-1.0, qs.y) * smoothstep(w * 0.7, w * 0.2, dx) * smoothstep(-1.4, head, qs.y);
            float bead = smoothstep(w * 1.5, w * 0.6, length(vec2(qs.x - path, (qs.y - head) * 0.7)));
            float active = smoothstep(thr - 0.02, thr + 0.05, wash) + (i < 2 ? 0.6 : 0.0);
            runs = max(runs, (trail * 0.55 + bead) * min(active, 1.0) * (1.0 - 0.7 * smoothstep(0.85, 1.0, wash)));
            // the path washed bare behind the runs, widening as the tile washes
            float reach = clamp((wash - thr) * 5.0, 0.0, 2.2);
            float pw = w * (0.8 + 2.5 * wash) * (0.7 + 0.6 * n);
            bareM = max(bareM, smoothstep(pw, pw * 0.5, dx) * step(qs.y + 1.0, reach) * step(thr, wash));
        }
        base = mix(paintC, bare, bareM);
        base = mix(base, paintC * 0.78, runs);    // the runs: deeper, wet paint
        // paint from the tile above: spots where its drips landed, a run or two of its color
        float got = vE.w;
        if (got > 0.01) {
            float sp = 0.0;
            for (int i = 0; i < 3; i++) {
                float fi = float(i);
                vec2 c0 = vec2(h(sd0 + fi * 3.3 + 50.0) * 1.4 - 0.7, -0.75 + 0.4 * h(sd0 + fi * 6.1 + 50.0));
                float r0 = (0.08 + 0.14 * h(sd0 + fi + 50.0)) * (0.5 + got);
                sp = max(sp, smoothstep(r0, r0 * 0.6, length(qs - c0) + 0.05 * vnoise(qs * 9.0 + fi)));
                float path = c0.x + 0.05 * sin(qs.y * 4.0 + fi);
                float head = -1.1 + mod(uTime * (0.25 + 0.2 * h(fi + sd0)) + fi, 2.4);
                float len = got * 1.8;
                float tr = smoothstep(0.06, 0.02, abs(qs.x - path)) * step(c0.y, qs.y) * step(qs.y, min(head, c0.y + len));
                sp = max(sp, tr * 0.9);
            }
            base = mix(base, vE.rgb * 0.92, sp * min(1.0, got * 2.5));
        }
    }
    float rim = smoothstep(-0.16 * hs.x, 0.0, sd);           // the glaze pooling darker at the rim
    vec3 c = base * mix(1.1, 0.82, (q.y + 1.0) * 0.5);
    c *= 1.0 - 0.32 * rim;
    // a thin bright bevel just inside the rim, top-left
    float bev = smoothstep(-0.14 * hs.x, -0.06 * hs.x, sd) * (1.0 - smoothstep(-0.06 * hs.x, 0.0, sd));
    c += vec3(1.0) * bev * 0.22 * clamp(-(q.x + q.y) * 0.6 + 0.4, 0.0, 1.0);
    // the sheen: a soft diagonal band of the street's neon
    float band = exp(-pow((q.x * 0.55 + q.y * 0.85 + 0.45) * 2.6, 2.0));
    float band2 = exp(-pow((q.x * 0.7 - q.y * 0.4 - 0.6) * 4.0, 2.0));
    float wet = vP.y;
    c += uSheen * band * (0.16 + 0.2 * wet) * uGlow + uSheen2 * band2 * 0.08 * uGlow;
    if (vP.z < 0.5) {
        float s = vP.x;
        // the water film along the top edge
        float film = smoothstep(-0.98, -0.86, q.y) * smoothstep(-0.7, -0.84, q.y) * (1.0 - smoothstep(0.75, 0.95, abs(q.x)));
        c += vec3(1.0) * film * (0.1 + 0.3 * wet);
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
    float a = vCol.a * cov;
    frag = vec4(c * a, a);
}
)";

// SDF type, one flat ink.
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
uniform vec3 uInk;
uniform float uAlpha;
uniform float uSoft;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-4) * 0.7;
    float a = smoothstep(0.5 - w - uSoft, 0.5 + w + uSoft * 0.3, d) * uAlpha;
    frag = vec4(uInk * a, a);
}
)";

} // namespace rain_shaders

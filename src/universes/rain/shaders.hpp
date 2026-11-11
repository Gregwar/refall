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
const float H = -8.2;        // the street's far edge (the horizon of the reflections)

float h11(float x) { return fract(sin(x * 127.1) * 43758.5453); }
float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float box(vec2 p, vec2 c, vec2 hs, float soft) {
    vec2 d = abs(p - c) - hs;
    float s = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    return smoothstep(soft, -soft, s);
}

// The city above the street, `blur` its softness (units): the rain blurs it, the reflections more.
vec3 city(vec2 p, float blur) {
    float t = clamp((p.y - H) / 24.0, 0.0, 1.0);
    vec3 col = mix(uSkyLow, uSkyTop, pow(t, 0.6));
    // a slow band of cloud light over the roofs
    col += uSkyLow * 0.18 * (0.5 + 0.5 * sin(p.x * 0.11 + uTime * 0.02)) * smoothstep(20.0, 6.0, p.y - H);
    col += uFlash * exp(-length((p - uFlashPos) * vec2(0.05, 0.08)));
    // far buildings: soft blocks, their windows blurred into bokeh
    {
        float w = 2.3;
        float id = floor((p.x + 200.0) / w);
        float top = H + 7.0 + 9.0 * h11(id + 3.0);
        float m = smoothstep(blur + 0.1, -blur - 0.1, p.y - top) * smoothstep(-0.05, 0.25, fract((p.x + 200.0) / w)) *
                  smoothstep(1.0, 0.85, fract((p.x + 200.0) / w));
        vec3 b = mix(col, uFar, 0.75);
        vec2 g = vec2(0.62, 0.9);
        vec2 wid = floor(p / g), wf = fract(p / g) - 0.5;
        float lit = step(0.7, h21(wid + id * 1.7)) * step(p.y, top - 0.6);
        float bok = smoothstep(0.3 + blur * 0.5, 0.08, length(wf * vec2(1.0, 0.9)));
        vec3 wc = mix(uWin, uNeon[int(h21(wid + 4.0) * 3.99)], step(0.6, h21(wid + 2.0)) * 0.7);
        b += wc * lit * bok * (0.3 + 0.35 * h21(wid + 9.0)) * uGlow;
        col = mix(col, b, m);
    }
    // near buildings: darker, taller, with the street's lights on them
    {
        float w = 4.6;
        float id = floor((p.x + 300.0) / w);
        float fx = fract((p.x + 300.0) / w);
        float top = H + 4.0 + 13.0 * h11(id * 1.31 + 11.0);
        float m = smoothstep(blur + 0.06, -blur - 0.06, p.y - top) * smoothstep(0.0, 0.05, fx) * smoothstep(1.0, 0.95, fx);
        vec3 b = mix(col, uNear, 0.88);
        // windows: a few warm ones, soft
        vec2 g = vec2(1.15, 1.5);
        vec2 wid = floor(p / g), wf = fract(p / g) - 0.5;
        float lit = step(0.7, h21(wid * 1.3 + id)) * step(p.y, top - 1.0) * step(H + 2.5, p.y);
        vec3 wc = mix(uWin, uNeon[int(h21(wid + 4.0) * 3.99)], step(0.55, h21(wid + 2.0)) * 0.6);
        b += wc * lit * box(wf, vec2(0.0), vec2(0.16, 0.22), 0.04 + blur * 0.5) * 0.5 * uGlow;
        float hs = h11(id * 7.7 + 2.0);
        int k = int(floor(h11(id * 3.3) * 4.0));
        vec3 nc = uNeon[k];
        if (uStyle == 0) {
            // a vertical neon sign on every other building, its glow on the wet facade
            if (hs > 0.4) {
                float sx = (id + 0.25 + 0.5 * h11(id + 5.0)) * w - 300.0;
                float y0 = H + 2.5 + 3.0 * h11(id + 8.0), y1 = min(top - 0.8, y0 + 3.0 + 4.0 * hs);
                vec2 d = vec2(abs(p.x - sx) - 0.28, max(y0 - p.y, p.y - y1));
                float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
                float core = smoothstep(0.08 + blur, -0.02, abs(sd + 0.12) - 0.04);
                b += nc * (core * 1.4 + 0.55 * exp(-max(sd, 0.0) * 1.3)) * uGlow;
                col += nc * 0.18 * exp(-max(sd, 0.0) * 0.8) * uGlow * (1.0 - m);
            }
        } else if (uStyle == 1) {
            // the market: striped awnings over the stalls, string lights along the street
            // one awning per stall (not every building), its lower edge scalloped, flowers in buckets under it
            float stall = step(0.35, hs) * smoothstep(0.1, 0.16, fx) * smoothstep(0.9, 0.84, fx);
            float ay = p.y - (H + 2.2);
            float scal = 0.12 * abs(sin(p.x * 4.5));
            float aw = smoothstep(blur + 0.04, -0.02, max(-ay - 0.5 + scal, ay - 0.6)) * stall;
            vec3 stripe = mix(uNeon[k], vec3(0.96, 0.92, 0.85), step(0.5, fract(p.x * 1.25)));
            b = mix(b, stripe * (0.5 + 0.5 * smoothstep(H + 1.6, H + 2.8, p.y)) * (0.75 + 0.35 * uGlow), aw * 0.92);
            vec2 fc = vec2(fract(p.x * 2.2) - 0.5, p.y - (H + 0.75 + 0.25 * h11(floor(p.x * 2.2))));
            float fl = smoothstep(0.32 + blur, 0.1, length(fc * vec2(1.0, 1.4))) * stall * step(p.y, H + 1.4);
            b = mix(b, uNeon[int(h11(floor(p.x * 2.2) + 3.0) * 3.99)] * 0.95, fl);
            vec2 sp = vec2(fract(p.x * 0.8) - 0.5, p.y - (H + 3.4 + 0.35 * sin(p.x * 0.5)));
            float bulb = smoothstep(0.16 + blur, 0.02, length(sp * vec2(1.6, 1.0)));
            b += uWin * bulb * 0.9 * uGlow;
            col += uWin * bulb * 0.9 * uGlow * (1.0 - m);
        } else {
            // monsoon: paper lanterns hanging in the mist
            for (int j = 0; j < 2; j++) {
                float fj = float(j);
                if (h11(id * 7.7 + 2.0 + fj * 3.0) < 0.25) continue;
                nc = uNeon[int(h11(id * 3.3 + fj) * 3.99)];
                float sx = (id + 0.2 + 0.25 * fj + 0.3 * h11(id + 5.0 + fj)) * w - 300.0;
                float ly = H + 2.5 + 7.0 * h11(id + 4.0 + fj * 2.0);
                float d = length((p - vec2(sx, ly)) * vec2(1.0, 0.8));
                float core = smoothstep(0.55 + blur, 0.35, d);
                b += nc * (core * 1.1 + 0.5 * exp(-d * 1.2)) * uGlow;
                col += nc * 0.35 * exp(-d * 0.9) * uGlow * (1.0 - m);
            }
        }
        col = mix(col, b, m);
    }
    // mist over the street
    col = mix(col, uMist, (uStyle == 2 ? 0.4 : uStyle == 1 ? 0.12 : 0.25) * exp(-(p.y - H) * 0.25));
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
        float rip = sin(ring * 40.0) * exp(-abs(ring) * 18.0) * (1.0 - ph) * step(h21(cell + 5.0), 0.25 + 0.6 * uRain);
        // the reflections stretched down the wet street (lights become long streaks), blurred with the depth
        vec2 q = vec2(p.x + rip * 0.08 + 0.05 * sin(p.y * 7.0 + uTime * 0.7), H + depth * 2.2 + 0.4 + rip * 0.1);
        vec3 refl = city(q, 0.3 + depth * 0.1);
        float k = mix(0.4, 0.8, pud);
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
    col += mix(uMist, vec3(0.9), 0.5) * r * (0.35 + 0.4 * uRain);
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
layout(location = 3) in vec4 aP;      // seed, wetness, plain (preview: no water), squash
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vGeom, vCol, vP;
void main() {
    vec2 p = aGeom.xy + aCorner * (aGeom.zw + 1.0);
    vPix = p;
    vGeom = aGeom; vCol = aCol; vP = aP;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* TILE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vGeom, vCol, vP;
out vec4 frag;
uniform float uTime;
uniform vec3 uSheen;   // the street's light caught in the glaze
uniform vec3 uSheen2;
uniform float uGlow;
float h(float x) { return fract(sin(x * 91.3458) * 47453.5453); }
void main() {
    vec2 hs = vGeom.zw;
    vec2 d = vPix - vGeom.xy;
    vec2 q = d / hs;                         // -1..1, y down
    float r = 0.2 * min(hs.x, hs.y);
    vec2 b = abs(d) - hs + r;
    float sd = length(max(b, 0.0)) + min(max(b.x, b.y), 0.0) - r;
    float cov = clamp(0.5 - sd, 0.0, 1.0);
    if (cov <= 0.0) discard;
    vec3 base = vCol.rgb;
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

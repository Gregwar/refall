#pragma once
// LANTERNS' shaders (see lanterns.cpp).
//
// LAKE_FS draws the night: the sky, a few faint stars, the far shore, the lake (the sky mirrored in it, a soft sheen)
// and the well's two bamboo poles with their reflections, in one full-screen pass; CELL_FS the pieces (paper panels
// lit from inside, a thin frame around each piece, its candle at its heart), their rippled reflection under the
// waterline, the ghost and the cells going out; SPRITE_FS the floating lanterns, their reflections, the sky lanterns,
// the far lights and the embers; BRIGHT_FS / BLUR_FS the glow; FINAL_FS composes and encodes the frame.
#include <string>

namespace lanterns_shaders {

// ---------------------------------------------------------------- common
inline const char* COMMON = R"(
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}
)";

// ---------------------------------------------------------------- the sky, the lake, the poles
inline const char* LAKE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec2 uRes;
uniform float uHorizon, uWater, uTime, uRipple, uStars, uPole, uMoon;
uniform vec3 uSkyTop, uSkyHor, uLakeNear, uShore, uGlow, uPoleCol;
uniform vec4 uColL, uColR;   // the HUD columns (x0, y0, x1, y1): no stars behind them
float shoreH(float x) {
    // low hills along the far shore, here and there
    float h = fbm(vec2(x * 0.09, 3.1)) - 0.42;
    return max(0.0, h) * 2.2 + 0.06 + 0.05 * vnoise(vec2(x * 0.7, 1.0));
}
vec3 sky(float y) {
    float t = clamp((y - uHorizon) / max(uView.y + uView.w - uHorizon, 1.0), 0.0, 1.0);
    vec3 c = mix(uSkyHor, uSkyTop, pow(t, 0.55));
    c += uGlow * exp(-(y - uHorizon) * 0.32);
    return c;
}
float inRect(vec2 w, vec4 r) {
    vec2 d = max(vec2(r.x, r.y) - w, w - vec2(r.z, r.w));
    return 1.0 - smoothstep(-0.6, 0.4, -max(d.x, d.y));
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / uRes.y;
    vec3 c;
    if (w.y >= uHorizon) {
        c = sky(w.y);
        // a few faint stars, high up, never behind the HUD
        vec2 g = w * 1.15;
        vec2 gi = floor(g);
        float h = hash(gi);
        if (h > 0.9) {
            vec2 sp = gi + vec2(hash(gi + 3.1), hash(gi + 7.7)) * 0.8 + 0.1;
            float d = length(g - sp) / 1.15;
            float tw = 0.75 + 0.25 * sin(uTime * (0.3 + h) + h * 40.0);
            float s = (1.0 - smoothstep(0.0, max(px * 1.1, 0.03), d)) * (h - 0.9) * 10.0 * tw;
            s *= smoothstep(0.8, 4.0, w.y - uHorizon) * inRect(w, uColL) * inRect(w, uColR);
            c += vec3(0.75, 0.8, 0.95) * s * 0.35 * uStars;
        }
        // slow motion: a full moon rises in the right margin
        if (uMoon > 0.0) {
            vec2 mc = vec2(16.0, uHorizon + 2.2 + 3.2 * uMoon);
            float md = length(w - mc);
            float disc = 1.0 - smoothstep(1.0 - px, 1.0 + px, md);
            float mare = 0.9 + 0.1 * vnoise((w - mc) * 2.2 + 4.0);
            c = mix(c, vec3(0.6, 0.64, 0.72) * mare, disc * uMoon);
            c += vec3(0.28, 0.33, 0.45) * exp(-md * 0.9) * 0.18 * uMoon;
        }
        // the far shore
        float sh = shoreH(w.x);
        float a = 1.0 - smoothstep(sh - px, sh + px, w.y - uHorizon);
        c = mix(c, uShore, a);
    } else {
        float d = uHorizon - w.y;                      // toward the viewer
        float s = d / max(uHorizon - (uView.y - uView.w), 1.0); // perspective scale 0 (far) .. 1 (near)
        // the sky mirrored, darkening toward the viewer
        vec3 refl = sky(uHorizon + d * 0.7) * 0.62;
        c = mix(refl, uLakeNear, smoothstep(0.0, 0.85, s));
        // the far shore mirrored, softened by the ripples
        float wob = sin(w.y / max(s, 0.02) * 0.9 + uTime * 0.8) * 0.15 * uRipple;
        float sh = shoreH(w.x + wob) * 0.9;
        c = mix(c, uShore * 0.8, (1.0 - smoothstep(sh - px * 3.0, sh + px * 3.0, d)) * 0.85);
        // a soft sheen: long, slow ripples, compressed toward the horizon
        float k = 1.0 / max(s, 0.03);
        vec2 ground = vec2(w.x, 15.0) * k; // the lake plane: across, depth
        float sheen = vnoise(vec2(ground.x * 0.06 + uTime * 0.03, ground.y * 0.5 - uTime * 0.12 * (0.3 + uRipple)));
        c *= 1.0 + (sheen - 0.5) * (0.1 + 0.12 * uRipple) * (1.0 - s * 0.5) * smoothstep(0.03, 0.2, s);
        // the moon's path on the water, broken into glints
        if (uMoon > 0.0) {
            float spread = 0.25 + 1.6 * s;
            float across = exp(-pow((w.x - 16.0) / spread, 2.0));
            float gl = smoothstep(0.62, 0.85, vnoise(vec2(ground.x * 0.9, ground.y * 3.5 + uTime * 0.5)));
            gl *= smoothstep(0.04, 0.15, s);
            c += vec3(0.55, 0.62, 0.78) * across * (0.2 + 0.8 * gl) * 0.22 * uMoon * (1.0 - smoothstep(0.6, 1.0, s));
        }
    }
    // the well: two bamboo poles standing in the water, and their reflections
    {
        float top = 10.25, bot = uWater - 0.25;
        float hw = 0.065;
        float ax = abs(abs(w.x) - 5.12);
        if (w.y > bot && w.y < top) {
            float node = 1.0 - smoothstep(0.0, 0.05, abs(fract((w.y + 10.0) / 2.5) - 0.5) * 2.5 - 0.0);
            float r = hw * (1.0 + 0.35 * node);
            float cov = 1.0 - smoothstep(r - px, r + px, ax);
            float sh = 0.75 + 0.25 * sign(w.x) * (abs(w.x) - 5.12) / r; // lit from the stack's side
            c = mix(c, uPoleCol * (0.7 + 0.3 * sh), cov * uPole);
        }
        if (w.y < uWater) {
            float dd = uWater - w.y;
            float wob = sin(w.y * 7.0 + uTime * 2.1) * 0.05 * uRipple * smoothstep(0.0, 1.0, dd);
            float bx = abs(abs(w.x + wob) - 5.12);
            float cov = (1.0 - smoothstep(hw - px, hw + px * 2.0, bx)) * exp(-dd * 0.8) * 0.55;
            c = mix(c, uPoleCol * 0.6, cov * uPole);
        }
        // the waterline across the well, barely there
        if (abs(w.x) < 5.1) {
            float l = (1.0 - smoothstep(0.0, px * 1.5, abs(w.y - uWater))) * 0.05;
            c += uPoleCol * l * uPole;
        }
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the pieces (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // paper tint (linear), alpha
layout(location = 3) in vec4 aC; // kind, progress, light, seed
layout(location = 4) in vec4 aD; // the piece's candle x, y, radius, ripple
uniform mat4 uVP;
uniform float uWater;
out vec2 vLocal;
out vec2 vW;
out vec2 vSrc;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    int kind = int(aC.x + 0.5);
    float ext = kind == 3 ? 1.25 : 1.0;
    vec2 off = aPos * aA.z * ext;
    vec2 wp;
    if (kind == 3) {
        // mirrored under the waterline
        wp = vec2(aA.x, 2.0 * uWater - aA.y) + off;
        vSrc = aA.xy + vec2(off.x, -off.y);
        vLocal = vec2(aPos.x, -aPos.y) * ext * 0.5;
    } else {
        wp = aA.xy + off;
        vSrc = wp;
        vLocal = aPos * ext * 0.5;
    }
    vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
in vec2 vSrc;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform float uPx, uTime, uWater;
uniform vec3 uFlame;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    float cell = vA.z * 2.0;
    float aa = uPx / max(cell, 1e-3);
    float rip = vD.w;
    if (kind == 3) {
        float dd = uWater - vW.y;
        p.x += sin(vW.y * 8.0 + uTime * 2.3) * rip * 0.07 * smoothstep(0.0, 0.8, dd) / cell;
        p.x += sin(vW.y * 19.0 - uTime * 3.1) * rip * 0.025 / cell;
    }
    float scale = 1.0;
    if (kind == 2) scale = 1.0 - 0.3 * smoothstep(0.25, 1.0, vC.y);
    p /= scale;
    if (abs(p.x) > 0.5 || abs(p.y) > 0.5) discard;
    // distance to the piece's frame (the sides with no cell of the piece)
    float d = 9.0;
    if (!has(m, 0)) d = min(d, p.x + 0.5);
    if (!has(m, 1)) d = min(d, 0.5 - p.x);
    if (!has(m, 2)) d = min(d, p.y + 0.5);
    if (!has(m, 3)) d = min(d, 0.5 - p.y);
    vec3 tint = vB.rgb;
    if (kind == 1) {
        // the ghost: a fine outline of paper light where the piece will land
        float line = 1.0 - smoothstep(0.035 - aa, 0.035 + aa, abs(d - 0.06));
        float fill = 0.05 * smoothstep(0.05, 0.2, d);
        float a = max(line * 0.55, fill) * vB.w;
        frag = vec4(tint * a, 0.0);
        return;
    }
    const float ft = 0.075;
    float frame = 1.0 - smoothstep(ft - aa, ft + aa, d);
    // the light falling off toward the frame: a product over the open sides (smooth, no mitred creases)
    float g = 1.0;
    if (!has(m, 0)) g *= smoothstep(ft, ft + 0.5, p.x + 0.5);
    if (!has(m, 1)) g *= smoothstep(ft, ft + 0.5, 0.5 - p.x);
    if (!has(m, 2)) g *= smoothstep(ft, ft + 0.5, p.y + 0.5);
    if (!has(m, 3)) g *= smoothstep(ft, ft + 0.5, 0.5 - p.y);
    // paper lit from inside: darker toward the frame, the candle at the piece's heart
    float light = vC.z;
    float hd = length(vSrc - vD.xy) / max(vD.z, 0.1);
    float hot = exp(-hd * hd * 1.6);
    float paper = 0.14 + 0.42 * pow(g, 0.8) + 0.4 * hot * (0.4 + 0.6 * g);
    // the ribs seen through the paper (between the cells' junctions, never on them) and the fibers
    float rib = abs(fract(vSrc.y * 2.0 + 0.5) - 0.5) * 0.5; // distance to a rib, board units
    paper *= 1.0 - 0.1 * (1.0 - smoothstep(0.0, 0.03 + uPx, rib));
    // washi: a cloudy thickness and a few long fibers
    float cloud = vnoise(vSrc * vec2(1.3, 1.9) + vC.w) * 0.6 + vnoise(vSrc * 4.1 + vC.w * 3.0) * 0.4;
    float fib = vnoise(vSrc * vec2(1.6, 9.0) + vC.w) * 0.55 + vnoise(vSrc * vec2(5.0, 30.0) + vC.w * 3.0) * 0.45;
    float strand = 1.0 - abs(vnoise(vSrc * vec2(2.2, 5.5) + vC.w * 7.0) * 2.0 - 1.0);
    paper *= 0.8 + 0.2 * cloud + 0.14 * (fib - 0.5) + 0.12 * smoothstep(0.9, 0.98, strand);
    // deeper, more saturated toward the frame, warmer at the candle
    vec3 deep = pow(tint, vec3(1.7));
    vec3 col = mix(deep, tint, smoothstep(0.0, 0.5, g + hot * 0.5)) * paper;
    col = mix(col, col * uFlame * 1.25 + uFlame * 0.05, 0.45 * hot * hot);
    if (kind == 2) light *= 1.0 - smoothstep(0.0, 0.6, vC.y);
    vec3 lit = col * light;
    // an unlit lantern still shows its paper, faintly
    vec3 dark = tint * 0.05 * paper;
    vec3 c = max(lit, dark);
    vec3 wood = vec3(0.03, 0.022, 0.018) + tint * 0.05 * light;
    c = mix(c, wood, frame);
    float a = vB.w;
    if (kind == 2) a *= 1.0 - smoothstep(0.55, 1.0, vC.y);
    if (kind == 3) {
        float dd = uWater - vW.y;
        float band = 0.72 + 0.28 * sin(vW.y * 24.0 + uTime * 1.6 + sin(vW.x * 1.3) * 2.0);
        float f = exp(-dd * 1.1) * 0.4 * mix(1.0, band, clamp(rip * 1.5, 0.0, 1.0));
        frag = vec4(c * a * f, 0.0);
        return;
    }
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the floating lanterns, the sky lanterns, lights
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, width, kind
layout(location = 2) in vec4 aB; // tint (linear), alpha
layout(location = 3) in vec4 aC; // light, ripple, seed, length (reflection)
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    int kind = int(aA.w + 0.5);
    float w = aA.z;
    vec2 c, h;
    if (kind == 0) { c = vec2(aA.x, aA.y + 0.58 * w); h = vec2(0.6, 0.66) * w; }
    else if (kind == 1) { float L = aC.w * w; c = vec2(aA.x, aA.y - 0.5 * L); h = vec2(0.75 * w + aC.y * 0.2 * w, 0.5 * L + 0.05 * w); }
    else if (kind == 2) { c = aA.xy; h = vec2(w); }
    else { c = aA.xy; h = vec2(0.6, 0.8) * w; }
    vec2 wp = c + aPos * h;
    vQ = (wp - aA.xy) / max(w, 1e-4);
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPx, uTime;
uniform vec3 uFlame;
void main() {
    int kind = int(vA.w + 0.5);
    float w = max(vA.z, 1e-4);
    float aa = uPx / w;
    vec3 tint = vB.rgb;
    float light = vC.x;
    float seed = vC.z;
    vec2 q = vQ;
    if (kind == 0) {
        // a box lantern floating on the water (its base at the waterline): a paper body swelling a little, a
        // dark wooden cap and base, its ribs, the candle low inside
        q.y -= 0.58;
        float y = q.y / 0.5;
        float hx = 0.4 + 0.05 * (1.0 - y * y);
        float body = (1.0 - smoothstep(hx - aa, hx + aa, abs(q.x))) * (1.0 - smoothstep(0.5 - aa, 0.5 + aa, abs(q.y)));
        float cap = (1.0 - smoothstep(0.34 - aa, 0.34 + aa, abs(q.x))) *
                    max(1.0 - smoothstep(0.075 - aa, 0.075 + aa, abs(q.y - 0.55)), 1.0 - smoothstep(0.05 - aa, 0.05 + aa, abs(q.y + 0.54)));
        float side = q.x / hx;
        float shade = 1.0 - 0.55 * side * side;
        float rib = abs(fract(q.y * 3.0 + 0.5) - 0.5) / 3.0;
        shade *= 1.0 - 0.18 * (1.0 - smoothstep(0.0, 0.025 + aa, rib));
        float hot = exp(-(q.x * q.x * 9.0 + (q.y + 0.12) * (q.y + 0.12) * 7.0));
        vec3 col = mix(tint, uFlame, 0.35 * hot) * (0.45 + 0.45 * shade + 0.5 * hot) * light;
        col = max(col, tint * 0.05);
        vec3 wood = vec3(0.025, 0.02, 0.016) + tint * 0.04 * light;
        float a = max(body, cap) * vB.w;
        vec3 c = mix(col, wood, cap);
        frag = vec4(c * a, a);
        return;
    }
    if (kind == 1) {
        // its reflection: a streak of its light under it, broken by the ripples
        float L = max(vC.w, 0.1);
        float v = -q.y;
        if (v < 0.0) discard;
        float rip = vC.y;
        float x = q.x + sin(v * 7.0 + uTime * 2.0 + seed * 6.0) * 0.12 * rip + sin(v * 17.0 - uTime * 2.7 + seed) * 0.05 * rip;
        float width = 0.3 * (1.0 + 0.5 * v / L);
        float across = 1.0 - smoothstep(0.0, width, abs(x));
        float band = 0.5 + 0.5 * sin(v * (12.0 + 6.0 * rip) - uTime * (1.2 + rip) + seed * 9.0);
        float k = exp(-v / L * 3.2) * smoothstep(0.0, 0.08, v) * mix(1.0, band, clamp(rip * 1.2 + 0.3, 0.0, 1.0));
        float a = across * k * vB.w;
        frag = vec4(tint * light * a * 0.32, 0.0);
        return;
    }
    if (kind == 2) {
        // a point of light (a far lantern, an ember)
        float r = length(q);
        float core = exp(-r * r * 30.0);
        float halo = exp(-r * r * 5.0) * 0.25;
        frag = vec4(mix(tint, uFlame, core * 0.4) * (core + halo) * light * vB.w, 0.0);
        return;
    }
    // a sky lantern: a paper bag wider at the top, the flame in its open mouth
    float t = clamp(q.y / 1.1 + 0.5, 0.0, 1.0);
    float hx = mix(0.3, 0.42, t) - 0.08 * smoothstep(0.85, 1.0, t);
    float body = (1.0 - smoothstep(hx - aa, hx + aa, abs(q.x))) * (1.0 - smoothstep(0.55 - aa, 0.55 + aa, abs(q.y)));
    float side = q.x / hx;
    float glow = exp(-(q.y + 0.55) * (q.y + 0.55) * 5.0);
    vec3 col = mix(tint, uFlame, 0.4 * glow) * (0.4 + 0.35 * (1.0 - side * side) + 0.7 * glow) * light;
    float a = body * vB.w;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the glow
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform vec2 uTexel; // of the scene
void main() {
    vec3 c = vec3(0.0);
    for (int j = -1; j <= 1; j += 2)
        for (int i = -1; i <= 1; i += 2) c += texture(uScene, vUv + vec2(i, j) * uTexel).rgb;
    c *= 0.25;
    float l = max(c.r, max(c.g, c.b));
    c *= smoothstep(0.3, 1.0, l);
    frag = vec4(c, 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uDir; // texel step
void main() {
    vec3 c = texture(uTex, vUv).rgb * 0.227;
    c += (texture(uTex, vUv + uDir * 1.385).rgb + texture(uTex, vUv - uDir * 1.385).rgb) * 0.316;
    c += (texture(uTex, vUv + uDir * 3.23).rgb + texture(uTex, vUv - uDir * 3.23).rgb) * 0.07;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uHorizonV;
uniform vec3 uMist, uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: moonlight and a mist lying on the lake (never over the pieces)
    float stack = clamp(s.a, 0.0, 1.0);
    float band = exp(-pow((vUv.y - uHorizonV) * 4.0, 2.0)) * 0.9 + smoothstep(0.35, 0.0, vUv.y) * 0.6;
    vec3 cool = c * vec3(0.72, 0.86, 1.12);
    c = mix(c, cool, uSlow * (1.0 - stack * 0.85));
    c += uMist * band * uSlow * (1.0 - stack);
    c = mix(c, mix(uBridgeCol, uBridgeInk, stack), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = c / (1.0 + max(c - 1.0, 0.0)); // a soft shoulder over 1
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD
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
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
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
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.25, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// A fragment shader with the common functions inserted after its version line.
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace lanterns_shaders

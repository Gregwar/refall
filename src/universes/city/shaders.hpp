#pragma once
// CITY's shaders (see city.cpp). Everything is drawn in board units on a flat orthographic view, into an HDR target:
//  - sky: the gradient, the city's glow on the horizon, a few stars, fog, the searchlights of a peak;
//  - buildings: instanced rectangles from the back to the front, each one a building with its own grid of small
//    windows (lit slowly one by one, or switched into a pattern by a drop) and, at a peak, a lit crown;
//  - tower: the big facade in front, whose 10 x 20 windows are the board (two small textures carry each window's
//    light, its interior, the ghost), with its piers, its lobby and its crown;
//  - windows: the previews, small lit windows in the HUD columns;
//  - dots: additive lights (beacons, traffic, the glow of the lit windows, fireworks, rain, snow, the gauge);
//  - final: a soft shoulder, sRGB, dim and fade, before the HUD.

namespace city_shaders {

inline const char* COMMON_GLSL = R"(
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float lumv(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
// A lit room seen through its window (uv 0..1 over the glass, y up). L: the light (color x level, linear), s: the
// interior (one per piece type), open: 1 for the falling piece (blinds raised: the room itself, brighter).
vec3 room(vec2 uv, int s, vec3 L, float seed, float t, float open) {
    float yy = uv.y;
    vec3 c = L * (0.5 + 0.5 * smoothstep(0.15, 1.0, yy));
    vec2 lp = vec2(0.22 + 0.56 * fract(seed * 7.31), 0.66);
    c += L * 0.5 * exp(-dot(uv - lp, uv - lp) * 9.0);
    c *= 1.0 - 0.5 * (1.0 - smoothstep(0.08, 0.2, yy)); // the floor
    // a diffuser (blind, curtain, paper): flatter and a little paler than the room
    vec3 diff = mix(L, vec3(lumv(L)), 0.18) * 1.05;
    float k = 1.0 - 0.65 * open;
    if (s == 0) { // I: venetian blinds, light between the slats
        float f = fract(yy * 6.0 + 0.1);
        float slat = smoothstep(0.06, 0.16, f) * (1.0 - smoothstep(0.66, 0.78, f));
        c = mix(c, mix(c * 0.3, diff * 1.1, slat), k);
    } else if (s == 1) { // O: an open living room, a lamp and a sofa
        float sx = 0.5 + (seed - 0.5) * 0.3;
        float sofa = step(0.17, yy) * step(yy, 0.33) * step(abs(uv.x - sx), 0.3);
        c *= 1.0 - 0.6 * sofa;
        vec2 q = uv - vec2(lp.x, 0.86);
        c += L * 0.6 * exp(-dot(q, q) * 40.0);
    } else if (s == 2) { // T: drapes drawn to both sides
        float inD = step(uv.x, 0.27) + step(0.73, uv.x);
        float folds = 0.7 + 0.3 * sin(uv.x * 60.0 + seed * 6.0);
        c = mix(c, diff * folds * 0.95, inD * k);
    } else if (s == 3) { // S: vertical blinds
        float f = fract(uv.x * 5.0);
        float slat = smoothstep(0.04, 0.14, f) * (1.0 - smoothstep(0.8, 0.92, f));
        c = mix(c, mix(c * 0.35, diff, slat), k);
    } else if (s == 4) { // Z: a roller blind half down, its hem
        float hy = 0.42 + 0.2 * seed;
        float inB = step(hy, yy);
        c = mix(c, diff * 1.15, inB * k);
        c *= 1.0 - 0.7 * k * (1.0 - smoothstep(0.0, 0.035, abs(yy - hy)));
    } else if (s == 5) { // J: a paper screen on a lattice
        vec2 g = abs(fract(uv * vec2(2.0, 3.0)) - 0.5);
        float bar = smoothstep(0.42, 0.46, max(g.x, g.y));
        c = mix(c, diff * mix(1.05, 0.3, bar), k);
    } else { // L: an office, a strip light, desks and screens
        c += L * 0.9 * exp(-pow((yy - 0.86) / 0.05, 2.0)) * step(0.08, uv.x) * step(uv.x, 0.92);
        float desk = step(0.2, yy) * step(yy, 0.27);
        float scr = step(0.27, yy) * step(yy, 0.4) * step(0.55, fract(uv.x * 3.0 + seed));
        c *= 1.0 - 0.55 * desk;
        c = mix(c, vec3(lumv(L)) * vec3(0.7, 0.9, 1.25) * 1.2, scr * 0.8);
    }
    // someone in the room now and then
    if (s == 1 || s == 2 || s == 6) {
        float ep = floor(t / 9.0 + seed * 5.0);
        if (hash12(vec2(seed * 31.0, ep)) < 0.22) {
            float px = 0.3 + 0.4 * hash12(vec2(ep, seed * 17.0)) + 0.06 * sin(t * 0.35 + seed * 9.0);
            vec2 hq = uv - vec2(px, 0.6);
            float head = 1.0 - smoothstep(0.065, 0.08, length(hq));
            vec2 bq = vec2(max(abs(uv.x - px) - 0.05, 0.0), max(yy - 0.46, 0.0));
            float body = (1.0 - smoothstep(0.07, 0.085, length(bq))) * step(yy, 0.5);
            c *= 1.0 - 0.7 * max(head, body);
        }
    }
    return c;
}
)";

inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uTop, uMid, uHor, uGlowCol, uFogCol;
uniform float uHorY, uGlow, uStars, uFog, uTime;
uniform vec4 uBeam[4];   // origin x, angle from vertical, width, intensity
uniform vec3 uBeamCol[4];
uniform float uBridge;
uniform vec3 uBridgeColor;
float fbm3(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = p * 2.07 + vec2(17.1, 9.7); a *= 0.5; }
    return s / 0.875;
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float h = p.y - uHorY;
    vec3 c = mix(uHor, uMid, smoothstep(0.0, 10.0, h));
    c = mix(c, uTop, smoothstep(8.0, 24.0, h));
    c += uGlowCol * uGlow * exp(-max(h, 0.0) / 4.0);
    if (uStars > 0.0) {
        vec2 g = p * 2.6;
        vec2 i = floor(g);
        float r = hash12(i);
        if (r > 0.982) {
            vec2 f = fract(g) - 0.5 - (vec2(hash12(i + 3.1), hash12(i + 7.7)) - 0.5) * 0.6;
            float s = exp(-dot(f, f) * 500.0) * (0.75 + 0.25 * sin(uTime * (0.6 + r * 2.0) + r * 50.0));
            c += vec3(0.75, 0.8, 1.0) * s * uStars * smoothstep(6.0, 16.0, h);
        }
    }
    for (int i = 0; i < 4; i++) {
        if (uBeam[i].w <= 0.001) continue;
        vec2 o = vec2(uBeam[i].x, uHorY - 2.0);
        vec2 d = vec2(sin(uBeam[i].y), cos(uBeam[i].y));
        vec2 v = p - o;
        float along = dot(v, d);
        if (along <= 0.0) continue;
        float perp = abs(dot(v, vec2(-d.y, d.x)));
        float w = uBeam[i].z * (0.2 + along * 0.05);
        float k = exp(-perp * perp / (w * w)) * exp(-along / 26.0) * smoothstep(0.0, 3.0, along);
        c += uBeamCol[i] * k * uBeam[i].w * (0.85 + 0.15 * vnoise(vec2(along * 0.4 - uTime * 0.3, perp)));
    }
    if (uFog > 0.0) {
        float fg = fbm3(vec2(p.x * 0.07 + uTime * 0.008, p.y * 0.3));
        c = mix(c, uFogCol, clamp(uFog * smoothstep(16.0, -2.0, h) * (0.65 + 0.35 * fg), 0.0, 1.0));
    }
    c = mix(c, uBridgeColor, uBridge);
    frag = vec4(c, 1.0);
}
)";

// Instanced rectangles (buildings, the tower, previews): a0 = the rectangle (x0, y0, x1, y1), board units.
inline const char* RECT_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA0;
layout(location = 2) in vec4 aA1;
layout(location = 3) in vec4 aA2;
layout(location = 4) in vec4 aA3;
uniform mat4 uVP;
out vec2 vP;
out vec2 vUv;
flat out vec4 vA0, vA1, vA2, vA3;
void main() {
    vA0 = aA0; vA1 = aA1; vA2 = aA2; vA3 = aA3;
    vec2 t = aCorner * 0.5 + 0.5;
    vUv = t;
    vP = mix(aA0.xy, aA0.zw, t);
    gl_Position = uVP * vec4(vP, 0.0, 1.0);
}
)";

// a1 = body (rgb), haze; a2 = window light (rgb), lit fraction; a3 = seed, style, layer, crown
// styles: 0 flat top, 1 setback, 2 slanted top, 3 solid (masts, crane), 4 pointed spire
inline const char* BLDG_FS = R"(#version 330 core
in vec2 vP;
in vec2 vUv;
flat in vec4 vA0, vA1, vA2, vA3;
out vec4 frag;
uniform vec3 uHaze;
uniform float uTime, uDropK, uPatEpoch, uPatPhase, uBlack, uFlash, uSnow, uStreetY;
uniform float uEq[8];
uniform vec3 uDropA, uDropB, uCrownCol, uSnowCol;
uniform float uBridge;
uniform vec3 uBridgeColor;
void main() {
    vec2 p = vP;
    vec2 r0 = vA0.xy, r1 = vA0.zw;
    float W = r1.x - r0.x, H = r1.y - r0.y;
    int st = int(vA3.y + 0.5);
    int layer = int(vA3.z + 0.5);
    float top = r1.y;
    if (st == 1) {
        float sb = r1.y - min(1.4, H * 0.14);
        if (p.y > sb && (p.x < r0.x + W * 0.2 || p.x > r1.x - W * 0.2)) discard;
        if (p.y <= sb) top = sb;
    } else if (st == 2) {
        float cut = r1.y - 1.1 * (p.x - r0.x) / max(W, 0.1);
        if (p.y > cut) discard;
        top = cut;
    } else if (st == 4) {
        float sh = min(2.5, H * 0.3);
        if (p.y > r1.y - sh) {
            float half = W * 0.5 * (r1.y - p.y) / sh;
            if (abs(p.x - (r0.x + r1.x) * 0.5) > half) discard;
        }
        top = r1.y - sh;
    }
    vec3 body = vA1.rgb * (0.85 + 0.25 * smoothstep(r0.y, r1.y, p.y));
    float haze = vA1.a;
    vec3 light = vec3(0.0);
    if (st != 3) {
        vec2 sp = layer == 0 ? vec2(0.42, 0.5) : (layer == 1 ? vec2(0.32, 0.36) : vec2(0.24, 0.27));
        float mx = sp.x * 0.45;
        vec2 o = vec2(r0.x + mx, uStreetY);
        vec2 q = (p - o) / sp;
        vec2 id = floor(q);
        vec2 f = fract(q);
        vec2 aa = fwidth(q) * 0.8;
        // the building's windows: punched, ribbons along the floors, or tall slots
        int ws = int(fract(vA3.x * 3.31) * 3.0);
        vec2 lo = ws == 0 ? vec2(0.3, 0.32) : (ws == 1 ? vec2(0.05, 0.36) : vec2(0.36, 0.14));
        vec2 hi = vec2(1.0) - lo;
        vec2 m2 = smoothstep(lo - aa, lo + aa, f) * (1.0 - smoothstep(hi - aa, hi + aa, f));
        float inW = m2.x * m2.y;
        float cols = floor((W - 2.0 * mx) / sp.x);
        inW *= step(0.0, id.x) * step(id.x, cols - 1.0) * step(p.y, top - sp.y * 0.55) * step(0.0, id.y);
        if (inW > 0.0) {
            float s = vA3.x;
            vec2 key = id + vec2(s * 37.0, s * 11.0);
            float h = hash12(key);
            float per = 22.0 + 55.0 * h;
            float ep = floor((uTime + h * per) / per);
            float lit = step(hash12(key * 1.37 + ep * 3.7), vA2.w);
            vec3 wc = vA2.rgb * (0.55 + 0.7 * hash12(key + ep * 3.1));
            if (hash12(key * 2.1 + ep) < 0.22) wc = vec3(lumv(wc)) * vec3(0.72, 0.88, 1.15);
            if (uDropK > 0.001) {
                float rows = max(1.0, floor((top - sp.y * 0.55 - uStreetY) / sp.y));
                int pat = int(mod(floor(s * 7.0) + uPatEpoch, 5.0));
                float on;
                if (pat == 0) on = mod(id.y + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
                else if (pat == 1) on = mod(id.x + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
                else if (pat == 2) on = mod(id.x + id.y + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
                else if (pat == 3) on = id.y < uEq[int(mod(floor(s * 13.0), 8.0))] * rows ? 1.0 : 0.0;
                else on = mod(id.x + id.y - uPatPhase * 2.0, 4.0) < 1.5 ? 1.0 : 0.0;
                vec3 dc = mod(floor(s * 5.0), 2.0) < 1.0 ? uDropA : uDropB;
                // the near buildings and some of the blocks behind switch into the pattern, the rest keep their
                // scattered windows in the drop's colors
                bool patterned = layer == 0 || (layer == 1 && fract(s * 5.7) < 0.4);
                if (patterned) lit = mix(lit, on, uDropK);
                wc = mix(wc, dc * (patterned ? 1.0 + 1.2 * uFlash : 0.8 + 0.6 * uFlash), uDropK * (patterned ? 1.0 : 0.75));
            }
            light = wc * lit * inW * (1.0 - uBlack);
            body *= 1.0 - 0.3 * inW;
        }
    }
    if (vA3.w > 0.001 && st != 3) {
        float e = smoothstep(0.09, 0.0, abs(top - 0.045 - p.y));
        light += uCrownCol * vA3.w * e * (1.0 - uBlack);
    }
    if (uSnow > 0.0 && st != 3) body = mix(body, uSnowCol, uSnow * smoothstep(0.09, 0.02, top - p.y));
    vec3 c = mix(body, uHaze, haze) + light * (1.0 - 0.6 * haze);
    c = mix(c, uBridgeColor, uBridge);
    frag = vec4(c, 1.0);
}
)";

// The tower: the facade whose windows are the board.
inline const char* TOWER_FS = R"(#version 330 core
in vec2 vP;
in vec2 vUv;
flat in vec4 vA0, vA1, vA2, vA3;
out vec4 frag;
uniform sampler2D uCellA; // 10 x 20 (row 0 at the top): rgb the window's light (color x level), a style + 1 (0: dark)
uniform sampler2D uCellB; // r ghost, g falling, b seed
uniform vec3 uFrame, uGlass, uReflTop, uReflBot, uReflLights, uGhostCol, uPierCol, uCrownCol, uLobby, uSnowCol;
uniform float uMull, uSpand, uRecess, uReflK, uSpill, uPier, uCrown, uSnow, uRain, uTime, uChase;
uniform vec4 uContact[4];  // x0, x1, y, intensity
uniform vec3 uContactCol[4];
uniform float uBridge;
uniform vec3 uBridgeColor, uBridgeInk;
uniform vec3 uChaseCol[7];

float boxSd(vec2 p, vec2 c, vec2 h) { vec2 d = abs(p - c) - h; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }

vec3 glassAt(vec2 p, vec2 uv, float seed) {
    vec3 refl = mix(uReflBot, uReflTop, clamp(uv.y * 0.35 + (p.y + 11.0) / 26.0, 0.0, 1.0));
    float sheen = 0.5 + 0.5 * sin(p.x * 0.32 + p.y * 0.11 + 1.3);
    vec3 c = uGlass + refl * uReflK * (0.55 + 0.45 * sheen);
    vec2 g = p * vec2(1.6, 2.2) + vec2(3.1, 1.7);
    vec2 i = floor(g);
    vec2 f = fract(g) - 0.5;
    float r = hash12(i);
    if (r > 0.88) c += uReflLights * exp(-dot(f, f) * 90.0) * (r - 0.88) * 6.0;
    c *= 0.85 + 0.3 * seed;
    if (uRain > 0.0) {
        float s = vnoise(vec2(p.x * 26.0, p.y * 1.3 + uTime * 1.6));
        c += (refl * 0.5 + uReflLights * 0.3) * uRain * smoothstep(0.72, 0.95, s);
    }
    return c;
}

void main() {
    vec2 p = vP;
    vec3 c;
    float px = fwidth(p.x);
    if (abs(p.x) > 5.5) discard;
    if (p.y > 11.1) {
        bool box = p.x > -3.4 && p.x < 0.4 && p.y < 11.75;
        bool mast = abs(p.x - 2.6) < 0.06 + 0.03 * step(p.y, 11.5) && p.y < 13.5;
        if (!box && !mast) discard;
        c = uFrame * (box ? 0.55 : 0.45);
        if (box) c += uCrownCol * uCrown * 0.6 * smoothstep(0.06, 0.0, 11.75 - p.y);
    } else if (p.y > 10.0) {
        if (p.y < 10.2) c = uFrame * 0.9;
        else if (p.y < 10.95) {
            float l = fract(p.y * 12.0);
            c = uFrame * mix(0.45, 0.9, smoothstep(0.2, 0.5, l) * (1.0 - smoothstep(0.75, 0.95, l)));
            c += uCrownCol * 0.04 * uCrown;
        } else {
            c = uFrame * 1.05;
            c += uCrownCol * uCrown * smoothstep(0.05, 0.0, abs(p.y - 11.02));
        }
        if (uSnow > 0.0) c = mix(c, uSnowCol, uSnow * smoothstep(0.06, 0.0, 11.1 - p.y));
    } else if (p.y < -10.0) {
        if (p.y > -10.28) c = uFrame * 0.9;
        else {
            float m = abs(fract((p.x + 5.5) / 1.375) - 0.5) * 1.375;
            float mull = smoothstep(0.05, 0.03, m);
            c = uLobby * (0.55 + 0.45 * smoothstep(-11.8, -10.5, p.y));
            c = mix(c, uFrame * 0.7, mull);
        }
    } else if (abs(p.x) > 5.0) {
        // the piers: stone, a vertical light strip in each
        float ax = abs(p.x);
        c = uFrame * (1.05 + 0.1 * smoothstep(5.5, 5.0, ax));
        float strip = smoothstep(0.045, 0.015, abs(ax - 5.25));
        vec3 sc = uPierCol * uPier;
        if (uChase > 0.001) {
            int k = int(mod(floor((p.y + 10.0) / 2.0) - uTime * 6.0, 7.0));
            sc += uChaseCol[k] * uChase * 1.5;
        }
        c += sc * strip;
        // the light of the floors next to it
        vec2 tc = vec2(p.x < 0.0 ? 0.5 / 10.0 : 9.5 / 10.0, (10.0 - p.y) / 20.0);
        c += texture(uCellA, tc).rgb * uSpill * 0.35 * smoothstep(5.5, 5.0, ax);
    } else {
        int col = clamp(int(floor(p.x + 5.0)), 0, 9);
        int row = clamp(int(floor(10.0 - p.y)), 0, 19);
        vec2 ctr = vec2(float(col) - 4.5, 9.5 - float(row));
        vec2 lp = p - ctr;
        vec4 A = texelFetch(uCellA, ivec2(col, row), 0);
        vec4 B = texelFetch(uCellB, ivec2(col, row), 0);
        float gx = 0.5 - uMull * 0.5;
        float gt = 0.5 - uSpand * 0.3, gb = -(0.5 - uSpand * 0.7);
        vec2 gc = vec2(0.0, (gt + gb) * 0.5), gh = vec2(gx, (gt - gb) * 0.5);
        float sd = boxSd(lp, gc, gh);
        float inG = 1.0 - smoothstep(-px * 0.7, px * 0.7, sd);
        vec2 uv = (lp - (gc - gh)) / (2.0 * gh);
        // the frame: mullions and spandrels, lit by the rooms next to it
        vec3 fr = uFrame;
        float spd = smoothstep(gb - 0.02, gb - 0.06, lp.y); // the spandrel band, a shade darker
        fr *= 1.0 - 0.12 * spd;
        fr *= 1.0 - 0.4 * uRecess * smoothstep(0.1, 0.0, sd); // a deep reveal around the window
        vec3 spill = texture(uCellA, vec2((p.x + 5.0) / 10.0, (10.0 - p.y) / 20.0)).rgb;
        fr += spill * uSpill * (0.35 + 0.65 * smoothstep(0.12, 0.0, sd));
        if (uSnow > 0.0) fr = mix(fr, uSnowCol, uSnow * smoothstep(0.035, 0.0, abs(lp.y - (gb - 0.03))) * step(0.0, gx - abs(lp.x) + 0.04));
        // the glass: a lit room or a dark pane
        vec3 g;
        int st = int(A.a + 0.5) - 1;
        float seed = B.b;
        if (st >= 0) {
            g = room(uv, st, A.rgb, seed, uTime, B.g);
            // a deep-set window: the top of the room in the frame's shadow
            g *= 1.0 - uRecess * 0.6 * smoothstep(0.75, 1.0, uv.y);
            g += uGlass * 0.2;
        } else {
            g = glassAt(p, uv, seed);
            g *= 1.0 - uRecess * 0.5 * smoothstep(0.7, 1.0, uv.y);
        }
        // the ghost: the window's frame lit in the piece's light, a dim room behind
        if (B.r > 0.001) {
            float line = 1.0 - smoothstep(0.0, max(px * 1.2, 0.02), abs(sd + 0.045));
            g += uGhostCol * B.r * (0.13 + 0.03 * smoothstep(0.2, 1.0, uv.y));
            g = mix(g, uGhostCol * 1.4, line * B.r);
            fr += uGhostCol * B.r * 0.25 * smoothstep(0.1, 0.0, sd);
        }
        c = mix(fr, g, inG);
    }
    // the contact light under a piece that just landed
    for (int i = 0; i < 4; i++) {
        if (uContact[i].w <= 0.001) continue;
        float hx = smoothstep(uContact[i].x - 0.25, uContact[i].x + 0.1, p.x) * (1.0 - smoothstep(uContact[i].y - 0.1, uContact[i].y + 0.25, p.x));
        float dy = (p.y - uContact[i].z) / 0.13;
        c += uContactCol[i] * uContact[i].w * hx * exp(-dy * dy);
    }
    // the bridge: the world to the bridge's color, the lit windows to its ink
    if (uBridge > 0.0) {
        float lit = 0.0;
        if (abs(p.x) < 5.0 && abs(p.y) < 10.0) {
            int col = clamp(int(floor(p.x + 5.0)), 0, 9), row = clamp(int(floor(10.0 - p.y)), 0, 19);
            vec4 A = texelFetch(uCellA, ivec2(col, row), 0);
            lit = A.a > 0.5 ? clamp(lumv(A.rgb) * 3.0, 0.0, 1.0) : 0.0;
        }
        c = mix(c, mix(uBridgeColor, uBridgeInk, lit), uBridge);
    }
    frag = vec4(c, 1.0);
}
)";

// The previews: small lit windows. a1 = light (rgb), style; a2 = seed, alpha
inline const char* WIN_FS = R"(#version 330 core
in vec2 vP;
in vec2 vUv;
flat in vec4 vA0, vA1, vA2, vA3;
out vec4 frag;
uniform vec3 uFrame;
uniform float uTime, uBridge;
uniform vec3 uBridgeInk;
void main() {
    vec2 uv = vUv;
    float e = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float fw = max(fwidth(uv.x), fwidth(uv.y));
    float inG = smoothstep(0.09 - fw, 0.09 + fw, e);
    vec2 g = (uv - 0.09) / 0.82;
    vec3 c = room(clamp(g, 0.0, 1.0), int(vA1.a + 0.5), vA1.rgb, vA2.x, uTime, 0.0);
    c = mix(uFrame, c, inG);
    c = mix(c, uBridgeInk, uBridge);
    float a = vA2.y;
    frag = vec4(c * a, a);
}
)";

// Additive lights. a0 = center, radius, streak length; a1 = color, alpha; a2 = direction, kind (0 soft dot, 1 a
// trail fading behind, 2 a crisp square, 3 a core and its glow)
inline const char* DOT_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA0;
layout(location = 2) in vec4 aA1;
layout(location = 3) in vec4 aA2;
layout(location = 4) in vec4 aA3;
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vA0, vA1, vA2;
void main() {
    vA0 = aA0; vA1 = aA1; vA2 = aA2;
    vec2 d = length(aA2.xy) > 1e-4 ? normalize(aA2.xy) : vec2(1.0, 0.0);
    vec2 n = vec2(-d.y, d.x);
    float r = aA0.z * (aA2.z > 1.5 ? 1.0 : 2.2);
    float hl = aA0.w * 0.5 + r, hw = r;
    vQ = vec2(aCorner.x * hl, aCorner.y * hw);
    vec2 w = aA0.xy + d * vQ.x + n * vQ.y;
    gl_Position = uVP * vec4(w, 0.0, 1.0);
}
)";
inline const char* DOT_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vA0, vA1, vA2;
out vec4 frag;
void main() {
    float r = vA0.z, L = vA0.w * 0.5;
    int kind = int(vA2.z + 0.5);
    float a;
    if (kind == 2) {
        vec2 q = abs(vQ) / r;
        float fw = fwidth(q.x);
        a = (1.0 - smoothstep(1.0 - fw, 1.0 + fw, max(q.x, q.y)));
    } else {
        float dx = max(abs(vQ.x) - L, 0.0);
        float d = length(vec2(dx, vQ.y)) / r;
        a = exp(-d * d * 2.0);
        if (kind == 3) a = a * 0.35 + (1.0 - smoothstep(0.35, 0.5, d));
        if (kind == 1 && L > 0.0) a *= mix(0.05, 1.0, clamp((vQ.x + L) / (2.0 * L), 0.0, 1.0));
    }
    a *= vA1.a;
    frag = vec4(vA1.rgb * a, 0.0);
}
)";

inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uMul;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    const float K = 0.55;
    float L = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float Lm = L < K ? L : K + (1.0 - K) * (1.0 - exp(-(L - K) / (1.0 - K)));
    c *= Lm / max(L, 1e-5);
    float mx = max(c.r, max(c.g, c.b));
    if (mx > 1.0) c = mix(c / mx, vec3(1.0), clamp((mx - 1.0) * 0.5, 0.0, 0.6));
    c *= uMul;
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash12(gl_FragCoord.xy) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
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
uniform vec4 uInk;
uniform float uBias;
uniform float uSoft;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75 + uSoft;
    float fill = smoothstep(0.5 - uBias - w, 0.5 - uBias + w, d);
    frag = vec4(uInk.rgb * uInk.a * fill, uInk.a * fill);
}
)";
inline const char* PANEL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uRes;
void main() { gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0); }
)";
inline const char* PANEL_FS = R"(#version 330 core
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace city_shaders

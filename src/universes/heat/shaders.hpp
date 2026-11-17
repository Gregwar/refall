#pragma once
// HEAT's shaders (see heat.cpp).
//
// The frame is one thermal image: BG_FS and CELL_FS write a temperature (r, 0 cold .. 1 white hot), the stack's
// coverage (g) and the light of the flames and the sparks (b, hidden by the pieces) into a float target, MAP_FS then
// maps the temperature through the camera's colormap (a 256 x 2 lookup texture: the variant's palette and the
// drop's) and adds the light and the hot things' bloom in its hot colors, and the overlay (the camera's lettering and
// scale bar) is drawn on top in display colors.

namespace heat_shaders {

// ---------------------------------------------------------------- the cave (temperatures)
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uAir, uHeat;
uniform vec4 uAirGrid;   // x0, y0, 1 / spacing
uniform vec4 uView;      // center, half extents (board units)
uniform vec2 uRes;
uniform float uTime, uWall, uAirGain, uShimmer, uGrain, uWallHeat, uVig, uWell;
uniform float uFlame, uEmbers; // the flames over the hot stack, the sparks in the air (0 .. 1)
uniform float uLava;           // the drop: lava light in the rock (0 .. 1)
uniform vec4 uBlob[4];   // x, y, rx, ry
uniform vec4 uBlobT;     // their temperatures
uniform vec4 uBox[4];    // the preview boxes: center, half extents
uniform vec4 uQuiet[2];  // the HUD columns: x0, y0, x1, y1
uniform float uSeed;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}
vec4 cubicW(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x, y = s.y - 4.0 * s.x, z = s.z - 4.0 * s.y + 6.0 * s.x;
    return vec4(x, y, z, 6.0 - x - y - z) * (1.0 / 6.0);
}
// B-spline sampling of the air (smooth between the coarse grid's samples).
float airAt(vec2 w) {
    vec2 tc = (w - uAirGrid.xy) * uAirGrid.z + 0.5;
    vec2 size = vec2(textureSize(uAir, 0));
    tc -= 0.5;
    vec2 f = fract(tc);
    tc -= f;
    vec4 xc = cubicW(f.x), yc = cubicW(f.y);
    vec4 c = tc.xxyy + vec2(-0.5, 1.5).xyxy;
    vec4 s = vec4(xc.xz + xc.yw, yc.xz + yc.yw);
    vec4 o = (c + vec4(xc.yw, yc.yw) / s) / size.xxyy;
    float s0 = texture(uAir, o.xz).r, s1 = texture(uAir, o.yz).r, s2 = texture(uAir, o.xw).r, s3 = texture(uAir, o.yw).r;
    float sx = s.x / (s.x + s.y), sy = s.z / (s.z + s.w);
    return mix(mix(s3, s2, sx), mix(s1, s0, sx), sy);
}
// the heat of the well's walls and floor (columns 0 and 11, row 22 of the heat texture)
float wallHeat(float col, float y) {
    float row = clamp(21.0 - (y + 9.5), 2.0, 21.0);
    return texture(uHeat, vec2((col + 0.5) / 12.0, (row + 0.5) / 23.0)).r;
}
// a thin line along the outline of a box (center c, half extents h): 1 on the line, its width w
float boxLine(vec2 p, vec2 c, vec2 h, float w, float px) {
    vec2 q = abs(p - c) - h;
    float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
    return 1.0 - smoothstep(w - px, w + px, abs(sd));
}
float boxIn(vec2 p, vec2 c, vec2 h, float px) {
    vec2 q = abs(p - c) - h;
    return 1.0 - smoothstep(-px, px, max(q.x, q.y));
}
// sparks rising from the lava: a few per tile of three drifting layers, each a short streak that flickers
float sparks(vec2 w, float px) {
    float s = 0.0;
    for (int l = 0; l < 3; l++) {
        float fl = float(l);
        float tile = 2.2 + 1.3 * fl, rise = 1.1 + 0.6 * fl;
        vec2 p = w / tile - vec2(0.0, uTime * rise / tile);
        vec2 id = floor(p), f = fract(p);
        float r = hash(id + fl * 31.7);
        float dens = mix(0.10, 0.45, smoothstep(9.0, -12.0, w.y)) * uEmbers;
        if (r > dens) continue;
        vec2 at = vec2(hash(id + 3.1), hash(id + 7.7)) * 0.8 + 0.1;
        at.x += 0.08 * sin(uTime * (0.9 + r * 3.0) + r * 40.0);
        vec2 d = (f - at) * tile / px;
        d.y *= 0.45; // a short streak: it moves up
        float size = 1.4 + 1.0 * hash(id + 5.3) - 0.3 * fl;
        float flick = 0.55 + 0.45 * sin(uTime * (3.0 + 6.0 * r) + r * 60.0);
        s += exp(-dot(d, d) / (size * size)) * flick * (1.0 - 0.25 * fl);
    }
    return s;
}

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / uRes.y;
    // convection shimmer: the air bends the view behind it where it is hot (slow, flowing up)
    float a0 = airAt(w) * 4.0;
    vec2 sh = vec2(sin(w.y * 2.1 - uTime * 1.7 + 1.3 * sin(w.x * 0.9 + uTime * 0.3)),
                   sin(w.x * 1.7 + uTime * 0.6)) * vec2(0.09, 0.03) * uShimmer * (1.0 - exp(-a0 * 1.5));
    vec2 ws = w + sh;
    // the cave: great rock walls around a deep hollow, their faceted faces catching a faint light from above
    float side = 0.55 * smoothstep(4.0, 22.0, abs(ws.x)) - 0.12;
    const float e = 0.8;
    vec2 rp = ws * vec2(0.05, 0.065) + vec2(3.1, 0.0);
    float m0 = fbm(rp), mx = fbm(rp + vec2(e * 0.05, 0.0)), my = fbm(rp + vec2(0.0, e * 0.065));
    float mass = m0 + side;
    float rock = smoothstep(0.42, 0.45, mass);
    // the light comes down over the rock: its upper faces lit, broken into angular facets (a cellular noise: each
    // facet its own tilt), cracked between them
    vec2 grad = vec2(mx - m0, my - m0) / e;
    vec2 fq = ws * vec2(0.17, 0.13) + 0.5 * vec2(fbm(ws * 0.2), fbm(ws * 0.2 + 7.0)), fi = floor(fq), ff = fract(fq);
    float f1 = 8.0, f2 = 8.0, tilt = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y), o = vec2(hash(fi + g), hash(fi + g + 17.3));
            float dd = length(g + o - ff);
            if (dd < f1) { f2 = f1; f1 = dd; tilt = hash(fi + g + 5.1); } else if (dd < f2) f2 = dd;
        }
    float seam = smoothstep(0.0, 0.15, f2 - f1);
    float lit = clamp(0.22 + dot(grad, vec2(0.3, -0.95)) * 12.0 + 0.35 * (tilt - 0.5) + 0.4 * (fbm(ws * 0.8) - 0.5), 0.0, 1.0);
    lit *= 0.7 + 0.3 * seam;
    float d = uWall * 0.6;
    d = mix(d, uWall * 0.7 + 0.12 * lit * lit, rock);
    // smoke drifting up through the cave
    vec2 sp = ws * vec2(0.09, 0.06) + vec2(uTime * 0.012, -uTime * 0.03);
    float smoke = fbm(sp + 1.4 * vec2(fbm(sp * 0.8 + 4.0 + uTime * 0.01), fbm(sp * 0.8 - uTime * 0.015)));
    d += 0.028 * smoothstep(0.45, 0.85, smoke) * (0.3 + 0.7 * smoothstep(12.0, -12.0, ws.y));
    // the HUD columns stay quiet
    float quiet = 0.0;
    for (int i = 0; i < 2; i++) {
        vec2 q = max(uQuiet[i].xy - ws, ws - uQuiet[i].zw);
        float inCol = 1.0 - smoothstep(-1.5, 1.0, max(q.x, q.y));
        quiet = max(quiet, inCol);
        d = mix(d, min(d, uWall * 0.8 + 0.035), inCol * 0.7);
    }
    // far shapes: lava glowing deep in the rock
    float tb[4] = float[4](uBlobT.x, uBlobT.y, uBlobT.z, uBlobT.w);
    for (int i = 0; i < 4; i++) {
        vec2 q = (ws - uBlob[i].xy) / max(uBlob[i].zw, vec2(0.01));
        float r2 = dot(q, q);
        d += tb[i] * (exp(-r2 * 1.6) * 0.75 + exp(-r2 * 6.0) * 0.35);
    }
    // the well: dark glass etched with the cells' grid, in a thin frame warmed by the stack
    vec2 qw = abs(w) - vec2(5.0, 10.0);
    float inWell = (1.0 - smoothstep(-px, px, max(qw.x, qw.y))) * uWell;
    vec2 gl = abs(fract(w + 0.5) - 0.5) / px;
    float grid = 1.0 - smoothstep(0.35, 1.1, min(gl.x, gl.y));
    d = mix(d, 0.018 + 0.012 * smoothstep(-10.0, 10.0, w.y + 2.0 * w.x), inWell * 0.92);
    d = mix(d, 0.125, grid * inWell * 0.85);
    float fw = 0.045;
    float frame = boxLine(w, vec2(0.0), vec2(5.06, 10.06), fw, px) * uWell;
    float fh = texture(uHeat, vec2((clamp(w.x + 5.0, 0.0, 9.999) + 1.0) / 12.0, 22.5 / 23.0)).r;
    float wh = wallHeat(w.x < 0.0 ? 0.0 : 11.0, w.y);
    float fheat = w.y < -9.9 ? fh : (abs(w.x) > 4.9 ? wh : 0.0);
    d = mix(d, 0.13 + uWallHeat * (1.0 - exp(-fheat * 1.8)), frame);
    // the previews' boxes: dark glass in a thin frame
    for (int i = 0; i < 4; i++) {
        d = mix(d, 0.02, boxIn(w, uBox[i].xy, uBox[i].zw, px) * 0.85 * uWell);
        d = mix(d, 0.125, boxLine(w, uBox[i].xy, uBox[i].zw, 0.025, px) * uWell);
    }
    // the air, hot over the stack, and the flames it carries: tongues rising and flickering out of the hot faces
    float a = airAt(ws);
    d += uAirGain * (1.0 - exp(-a * 5.0));
    // (light, not temperature: they glow in the camera's hot colors and fade to the dark, see MAP_FS)
    vec2 fp = vec2(ws.x * 1.4, ws.y * 0.4 - uTime * 1.4);
    float tongue = fbm(fp + vec2(0.9 * fbm(fp * 0.7 + vec2(0.0, uTime * 0.3)), 0.0));
    float fire = smoothstep(0.55, 1.05, (1.0 - exp(-a * 6.0)) * (0.1 + 1.3 * tongue));
    float light = min(uFlame * fire * (0.6 + 0.6 * fire), 0.2); // (capped: a sudden plume never flares)
    // the drop wakes the cave: lava glows in the cracks between the rock's facets, slowly breathing, and the smoke
    // is lit warm from below
    float vein = (1.0 - smoothstep(0.0, 0.05, f2 - f1)) * rock * smoothstep(0.48, 0.62, mass)
               * smoothstep(0.45, 0.65, fbm(ws * 0.12 + 11.0));
    float breath = 0.6 + 0.4 * sin(uTime * 0.7 + 3.0 * fbm(ws * 0.1));
    light += uLava * (0.3 * vein * breath + 0.14 * smoothstep(0.45, 0.85, smoke) * smoothstep(12.0, -10.0, ws.y))
           * (1.0 - inWell) * (1.0 - 0.8 * quiet);
    // sparks
    light += sparks(w, px) * 1.3;
    // the lens: colder corners, a fine still grain
    vec2 c = vUv * 2.0 - 1.0;
    d *= 1.0 - uVig * dot(c * vec2(0.8, 1.0), c * vec2(0.8, 1.0));
    d += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * uGrain;
    frag = vec4(d, 0.0, light, 1.0);
}
)";

// ---------------------------------------------------------------- the pieces (temperatures)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // game x, game y (heat lookup; x < 0: use heat), heat, alpha
layout(location = 3) in vec4 aC; // base temperature (the ghost: its contour's light), kind, shrink, pad
layout(location = 4) in vec4 aD; // the cell's place in its piece's lava (xy), pad
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec2 vTex;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vA = aA; vB = aB; vC = aC; vTex = aD.xy;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec2 vTex;
out vec4 frag;
uniform sampler2D uHeat;
uniform float uRim, uGain, uTime, uCap;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the piece the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right): its own cells merge, a thin seam parts it from the others.
float shapeSdf(vec2 p, int m) {
    const float g = 0.04, r = 0.16, rc = 0.06;
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

vec2 hash2(vec2 p) { return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i).x, hash2(i + vec2(1, 0)).x, f.x), mix(hash2(i + vec2(0, 1)).x, hash2(i + vec2(1, 1)).x, f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.07 + vec2(3.1, 1.7); a *= 0.5; }
    return s / 0.9375;
}
// the crust's cracks: how far from the nearest crack (the gap between the two nearest plates of a cellular noise)
float cracks(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            float d = length(g + hash2(i + g) - f);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return d2 - d1;
}

void main() {
    int kind = int(vC.y + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float sd = shapeSdf(vLocal, int(vA.w + 0.5)) + vC.z;
    if (kind == 1) {
        // the ghost: a crisp contour of the piece's outline drawn in light (the colormap's warm color, vC.x its
        // strength, so the thin line never greys out as a temperature would), over a very faint fill
        float line = 1.0 - smoothstep(0.9 * aa, 2.0 * aa, abs(sd + 0.05));
        vec2 own = abs(vLocal) - 0.5;
        float inside = (1.0 - smoothstep(-aa, aa, sd)) * step(max(own.x, own.y), 0.0);
        float a = inside * 0.3 * vB.w;
        frag = vec4(0.14 * a, a, line * vC.x * vB.w, a);
        return;
    }
    // the heat: from the stack's heat map (smooth across the piece's cells) or given
    float h = vB.z;
    if (vB.x >= 0.0) {
        vec2 tc = vec2(vB.x + 1.5 + vLocal.x, vB.y + 0.5 - vLocal.y);
        h = texture(uHeat, tc / vec2(12.0, 23.0)).r;
    }
    float base = vC.x;
    float hv = min(1.0, 0.88 * h * uGain);
    float core = base + (1.02 - base) * hv;
    // the piece's lava (continuous over its cells), flowing slowly, faster when hotter
    vec2 p = vLocal + vTex;
    float ft = uTime * (0.03 + 0.12 * hv);
    vec2 wq = p * 2.0;
    vec2 warp = vec2(fbm(wq + vec2(0.0, ft)), fbm(wq + vec2(4.3, -ft)));
    float swirl = fbm(wq * 1.3 + 2.4 * warp + vec2(ft, 0.0));
    float molten = core - 0.1 * hv + (swirl - 0.5) * (0.16 + 0.34 * hv);
    // as it cools, a dark crust: plates of the material's own temperature, cracked by fissures that glow with the
    // heat underneath (wide and bright when warm, hairlines when cold)
    float crust = 1.0 - smoothstep(0.2, 0.62, hv);
    float e = cracks(p * 1.15 + 0.8 * (warp - 0.5));
    float cw = mix(0.04, 0.2, hv);
    float crack = 1.0 - smoothstep(cw * 0.3, cw, e);
    float plate = base * (0.85 + 0.4 * swirl) + (1.02 - base) * hv * 0.5;
    float fissure = base + (1.02 - base) * min(1.0, 0.25 + 1.3 * hv);
    float d = mix(molten, mix(plate, fissure, crack), crust);
    // the block: a molten edge glowing round it, a hairline cooler outline keeping the seam crisp
    float depth = clamp(-sd, 0.0, 0.5);
    float rim = (1.0 - smoothstep(0.03, 0.14, depth)) * uRim;
    d = mix(d, max(d, base + (1.02 - base) * min(1.0, 0.22 + 1.2 * hv)), rim * 0.85);
    d *= 1.0 - 0.35 * uRim * (1.0 - smoothstep(0.0, 0.022, depth));
    // the camera's highlight compression (the drop): above a knee the heat rolls off softly to the cap, so the
    // hottest lava tops out bright orange with its swirls and fissures still showing
    float knee = uCap - 0.25;
    if (d > knee) d = knee + 0.25 * (1.0 - exp(-(d - knee) / 0.25));
    float a = (1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd)) * vB.w;
    frag = vec4(d * a, a, 0.0, a);
}
)";

// ---------------------------------------------------------------- the camera: temperature -> colors
inline const char* MAP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTemp, uLut, uGlow;
uniform float uGlowAmt, uBloom;
uniform float uLo, uHi, uMix, uIso, uIsoThr, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk; // display colors
uniform float uFocus;  // the setup's focus pull: the blur's radius (uv height)
uniform float uCool;   // the teardown: the image drains to the cold end of the scale
uniform float uAspect; // width / height
vec3 lut(float t, float row) { return texture(uLut, vec2(t * (255.0 / 256.0) + 0.5 / 256.0, row)).rgb; }
void main() {
    vec4 T = texture(uTemp, vUv);
    if (uFocus > 0.0005) { // out of focus: a disc of taps (a golden-angle spiral)
        for (int i = 1; i < 16; i++) {
            float r = sqrt(float(i) / 15.0) * uFocus, a = float(i) * 2.39996;
            T += texture(uTemp, vUv + r * vec2(cos(a) / uAspect, sin(a)));
        }
        T /= 16.0;
    }
    // the halo around hot things (the lens and the warmed air), hotter pieces glowing farther
    vec4 G = texture(uGlow, vUv);
    float gl = G.r * (0.3 + 0.7 * G.r) * uGlowAmt * (1.0 - 0.8 * clamp(T.g, 0.0, 1.0));
    T.r += gl;
    float t = clamp((T.r - uLo) / max(uHi - uLo, 1e-3), 0.0, 1.0) * (1.0 - uCool);
    vec3 c = mix(lut(t, 0.25), lut(t, 0.75), uMix);
    // the lens's bloom: hot things bleed their own color around them
    float tg = clamp((G.r / max(G.g, 1e-3) - uLo) / max(uHi - uLo, 1e-3), 0.0, 1.0) * (1.0 - uCool);
    float bloom = G.g * smoothstep(0.45, 0.9, tg) * uBloom * (1.0 - 0.85 * clamp(T.g, 0.0, 1.0));
    bloom *= 1.0 - 0.7 * smoothstep(0.55, 0.95, G.g); // (inside the stack the seams between pieces stay dark)
    c += mix(lut(tg, 0.25), lut(tg, 0.75), uMix) * bloom;
    // the flames and the sparks: light in the hot colors, the brightest the hottest
    float l = clamp(T.b, 0.0, 1.5) * (1.0 - uCool);
    float tl = 0.5 + 0.42 * smoothstep(0.0, 1.2, l);
    c += mix(lut(tl, 0.25), lut(tl, 0.75), uMix) * min(l, 1.0);
    // slow motion: the isotherm mode, everything grey but the hottest band
    float g = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 grey = vec3(pow(t, 0.9) * 0.82 + 0.02);
    float keep = smoothstep(uIsoThr - 0.03, uIsoThr + 0.03, t);
    c = mix(c, mix(grey, c, keep), uIso);
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(T.g, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the halo (a separable blur)
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

// ---------------------------------------------------------------- the overlay
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

} // namespace heat_shaders

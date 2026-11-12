#pragma once
// PINK VIBE's shaders. Flat design in display (sRGB) colors, every shape a signed distance field without outlines:
// the flowing bands behind everything (with their sparse sparkles and bubbles), the sprites (the well's backing,
// petals, hearts, bubbles, the bonus ring), the blocks (one soft rounded tile per cell and one small motif per piece
// type), the lettering on a feathered blush halo.

namespace pinkvibe_shaders {

// Shared painting helpers (prepended after the #version line).
inline const char* PAINT_GLSL = R"(
const float TAU = 6.2831853;
float AA = 0.01;
void layerA(inout vec4 acc, float d, vec3 col, float k) {
    float a = (1.0 - smoothstep(-AA, AA, d)) * k;
    acc.rgb = mix(acc.rgb, col, acc.a > 0.0 ? a : 1.0);
    acc.a = max(acc.a, a);
}
void layer(inout vec4 acc, float d, vec3 col) { layerA(acc, d, col, 1.0); }
float dEll(vec2 p, vec2 r) { return (length(p / r) - 1.0) * min(r.x, r.y); }
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float dot2(vec2 v) { return dot(v, v); }
// A heart about 2 wide, its point at the bottom.
float dHeart(vec2 q) {
    vec2 p = q * 0.62 + vec2(0.0, 0.58);
    p.x = abs(p.x);
    float d;
    if (p.y + p.x > 1.0) d = sqrt(dot2(p - vec2(0.25, 0.75))) - sqrt(2.0) / 4.0;
    else d = sqrt(min(dot2(p - vec2(0.0, 1.0)), dot2(p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
    return d / 0.62;
}
// A four-pointed sparkle (radius 1), its arms slender.
float dSparkle(vec2 q) {
    vec2 p = abs(q);
    float v = sqrt(p.x + 0.002) + sqrt(p.y + 0.002);
    return (v - 1.0) * 0.55;
}
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
)";

// ---------------------------------------------------------------- the bands
inline const char* BG_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;            // center, half extents (board units)
uniform vec3 uBand[5];         // blush .. the deepest band (mirrored: 0 1 2 3 4 3 2 1)
uniform float uTime, uFlow, uAmp, uSway, uSwirl, uDesat, uDecor;
uniform vec4 uColL, uColR;     // the HUD columns (board units): the motifs fade out there
vec3 band(float i) {
    float k = mod(i, 8.0);
    k = k > 4.0 ? 8.0 - k : k;
    return k < 0.5 ? uBand[0] : k < 1.5 ? uBand[1] : k < 2.5 ? uBand[2] : k < 3.5 ? uBand[3] : uBand[4];
}
float boxD(vec2 p, vec4 b) {
    vec2 c = (b.xy + b.zw) * 0.5, h = (b.zw - b.xy) * 0.5;
    return dBox(p - c, h, 0.8);
}
// The band coordinate at p: diagonal, waving along the bands; the drop swirls it around the well.
float bandCoord(vec2 p) {
    vec2 c = p - vec2(0.0, -1.0);
    float r = length(c);
    float a = uSwirl * (1.9 * exp(-r / 15.0) + 0.25) + uSwirl * 0.04 * uTime;
    float cs = cos(a), sn = sin(a);
    c = mat2(cs, sn, -sn, cs) * c;
    const vec2 dir = vec2(0.5, 0.866);            // the bands run from the top left to the bottom right
    vec2 nrm = vec2(dir.y, -dir.x);
    float s = dot(c, dir), t = dot(c, nrm);
    float w = uAmp * (1.25 * sin(t * 0.15 + uFlow * 0.9 + s * 0.05)
                    + 0.55 * sin(t * 0.31 - uFlow * 0.6 + 1.7)
                    + 0.3 * sin(t * 0.07 + s * 0.11 + uFlow * 0.35 + 4.0));
    w += uSway * sin(t * 0.12 + s * 0.2 + 0.6);
    // the drop: the bands roll into one big wave, a slow long swell
    w += uSwirl * 2.2 * sin(t * 0.09 + uTime * 0.12);
    return s + w;
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    const float BW = 3.3;                              // a band's width (board units)
    float s = bandCoord(p) / BW + 0.5;
    float i = floor(s), f = fract(s);
    vec3 col = band(i);
    // a soft flat shade under each band's upper edge (the band above lies over it), a slight lift at its lower edge
    float px = fwidth(s);
    float edge = 1.0 - smoothstep(0.0, 0.16, f);
    col = mix(col, col * vec3(0.9, 0.84, 0.88), edge * 0.32);
    float aa = smoothstep(1.0 - px * 1.2, 1.0, f);
    col = mix(col, band(i + 1.0), aa);
    // sparse sparkles and bubbles, toned to their band, drifting very slowly upward
    if (uDecor > 0.01) {
        const float G = 4.6;
        vec2 q = p + vec2(0.0, -uTime * 0.18);
        vec2 gc = floor(q / G);
        float h = hash12(gc);
        if (h > 0.82) {
            vec2 o = (vec2(hash12(gc + 7.3), hash12(gc + 2.1)) - 0.5) * G * 0.6;
            o += vec2(sin(uTime * 0.21 + h * 40.0), cos(uTime * 0.17 + h * 23.0)) * 0.35;
            vec2 cc = (gc + 0.5) * G + o;
            vec2 l = q - cc;
            vec2 world = cc + vec2(0.0, uTime * 0.18);
            float vis = smoothstep(0.6, 2.4, min(boxD(world, uColL), boxD(world, uColR)));
            vis *= smoothstep(5.6, 7.2, abs(world.x));                 // (none behind the well)
            float tw = 0.75 + 0.25 * sin(uTime * 0.9 + h * 31.0);
            vec3 bc = band(floor(bandCoord(world) / BW + 0.5));
            vec3 tone = mix(bc, vec3(1.0, 0.97, 0.98), 0.5);
            float AAl = fwidth(q.x) * 0.8;
            float d;
            float kind = fract(h * 17.0);
            if (kind < 0.45) d = dSparkle(l / (0.42 * tw)) * 0.42 * tw;
            else if (kind < 0.75) d = abs(length(l) - 0.34) - 0.07;     // a little bubble ring
            else d = length(l) - 0.16;                                  // a dot
            float k = (1.0 - smoothstep(-AAl, AAl, d)) * vis * uDecor * 0.75;
            col = mix(col, tone, k);
        }
    }
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.86, 0.8, 0.98), uDesat);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- sprites
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;  // x, y, half width, half height (board units)
layout(location = 2) in vec4 iB;  // rotation, kind, alpha, p1
layout(location = 3) in vec4 iC;  // color A, p2
layout(location = 4) in vec4 iD;  // color B, p3
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 ext = iA.zw * 1.08 + 0.25;
    vec2 l = aCorner * ext;
    float c = cos(iB.x), s = sin(iB.x);
    vL = l;
    vA = iA; vB = iB; vC = iC; vD = iD;
    gl_Position = uVP * vec4(iA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y), 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(
in vec2 vL;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
void main() {
    int kind = int(vB.y + 0.5);
    vec3 A = vC.rgb, B = vD.rgb;
    float p1 = vB.w, p2 = vC.w, p3 = vD.w;
    vec4 acc = vec4(0.0);
    if (kind == 0) {
        // the well's backing (board units): a translucent deep panel, a vertical gradient, a soft pale rim
        vec2 l = vL;
        AA = max(fwidth(l.x), 1e-4) * 0.7;
        float d = dBox(l, vA.zw, 0.55);
        float g = clamp(l.y / vA.w * 0.5 + 0.5, 0.0, 1.0);
        layerA(acc, d - 0.07, B, 0.55);
        layerA(acc, d, mix(A * 0.82, A, g), p1);
        frag = vec4(acc.rgb * acc.a * vB.z, acc.a * vB.z);
        return;
    }
    vec2 q = vL / vA.w;
    AA = max(fwidth(q.x), 1e-4) * 0.8;
    if (kind == 1) {
        // a petal: a soft pointed oval, a paler vein
        float d = max(dEll(q, vec2(1.0, 0.55)), dEll(q - vec2(0.35, 0.0), vec2(1.3, 0.62)));
        layer(acc, d, A);
        layerA(acc, max(abs(q.y) - 0.035, abs(q.x + 0.1) - 0.5), B, 0.5);
    } else if (kind == 2) {
        layer(acc, dHeart(q), A);
        layerA(acc, length(q - vec2(-0.38, 0.32)) - 0.13, B, 0.7);
    } else if (kind == 3) {
        // a bubble: a pale disc, a fine ring, a small highlight
        layerA(acc, length(q) - 0.95, A, 0.35);
        layerA(acc, abs(length(q) - 0.9) - 0.06, A, 0.9);
        layerA(acc, dEll(q - vec2(-0.38, 0.4), vec2(0.22, 0.13)), B, 0.9);
    } else if (kind == 4) {
        layer(acc, dSparkle(q), A);
    } else if (kind == 5) {
        // the bonus ring: a track, its filled arc (p1), a soft heart in its middle when it's ready (p2)
        float r = length(q);
        float ring = abs(r - 0.82) - 0.11;
        layerA(acc, ring, B, 0.7);
        float an = fract(0.25 - atan(q.y, q.x) / TAU);
        float arc = max(ring, an > p1 ? 1.0 : -1.0);
        layer(acc, arc, A);
        if (p2 > 0.5) layerA(acc, dHeart(q * 2.6) / 2.6, A, 0.9);
    }
    float al = acc.a * vB.z;
    frag = vec4(acc.rgb * al, al);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // type, alpha, mode (0 block, 1 ghost, 2 preview), scale
layout(location = 3) in vec4 iC; // color, k (< 0: brightening, > 0: game over greying)
uniform mat4 uVP;
out vec2 vU;
flat out vec4 vB, vC;
void main() {
    vec2 l = aCorner * 0.56;
    float c = cos(iA.w), s = sin(iA.w);
    vec2 w = vec2(c * l.x - s * l.y, s * l.x + c * l.y) * iA.z * 2.0 * iB.w;
    vU = l;
    vB = iB; vC = iC;
    gl_Position = uVP * vec4(iA.xy + w, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(
in vec2 vU;
flat in vec4 vB, vC;
out vec4 frag;
uniform vec3 uRim;
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.75;
    int type = int(vB.x + 0.5);
    vec3 col = vC.rgb;
    float d = dBox(u, vec2(0.45), 0.08);  // nearly square corners, like the flat bands
    vec4 acc = vec4(0.0);
    if (abs(vB.z - 1.0) < 0.5) {
        // the ghost: a soft fill and a clear line of the piece's color, lightened so it reads on the plum well
        vec3 lc = mix(col, vec3(1.0), 0.3);
        layerA(acc, d, col, 0.26);
        layerA(acc, abs(d + 0.035) - 0.032, lc, 0.95);
        frag = vec4(acc.rgb * acc.a * vB.y, acc.a * vB.y);
        return;
    }
    // (a preview over the bands sits on a soft rim of the well's color)
    if (vB.z > 1.5) layerA(acc, d - 0.06, uRim, 0.4);
    // one flat tile: a single color, no lift, no shine (flat design, like the bands)
    layer(acc, d, col);
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 M = lum > 0.8 ? mix(col, vec3(0.86, 0.45, 0.58), 0.38) : mix(col, vec3(1.0, 0.97, 0.98), 0.5);
    // the motif, calm, a little larger, one per type, low right of the center
    vec2 q = (u - vec2(0.02, -0.02)) / 0.21;
    if (type == 0) {           // I: a pearl, a row of three dots
        float dm = min(length(q - vec2(-0.75, 0.0)), min(length(q), length(q - vec2(0.75, 0.0)))) - 0.24;
        layer(acc, dm * 0.21, M);
    } else if (type == 1) {    // O: a tiny heart
        layer(acc, dHeart(q * 1.05) * 0.21, M);
    } else if (type == 2) {    // T: a little flower
        float an = atan(q.y, q.x);
        float rr = 0.62 + 0.3 * cos(an * 5.0);
        layer(acc, (length(q) - rr) * 0.6 * 0.21, M);
        layer(acc, (length(q) - 0.24) * 0.21, col);
    } else if (type == 3) {    // S: a sparkle
        layer(acc, dSparkle(q * 0.85) * 0.21 / 0.85, M);
    } else if (type == 4) {    // Z: a crescent moon
        float cm = max(length(q) - 0.78, -(length(q - vec2(0.36, 0.26)) - 0.62));
        layer(acc, cm * 0.21, M);
    } else if (type == 5) {    // J: a bow
        vec2 b = vec2(abs(q.x), q.y);
        float wing = max(dEll(b - vec2(0.52, 0.0), vec2(0.5, 0.42)), -b.x + 0.1 + abs(q.y) * 0.4);
        float knot = length(q) - 0.2;
        layer(acc, min(wing, knot) * 0.21, M);
    } else {                   // L: a bubble ring
        layer(acc, (abs(length(q) - 0.62) - 0.14) * 0.21, M);
        layer(acc, (length(q - vec2(-0.2, 0.2)) - 0.12) * 0.21, M);
    }
    if (vC.w < 0.0) acc.rgb = mix(acc.rgb, vec3(1.0, 0.96, 0.97), -vC.w);
    else {
        float l = dot(acc.rgb, vec3(0.299, 0.587, 0.114));
        acc.rgb = mix(acc.rgb, vec3(l) * vec3(0.7, 0.6, 0.68), vC.w);
    }
    float al = acc.a * vB.y;
    frag = vec4(acc.rgb * al, al);
}
)";

// ---------------------------------------------------------------- lettering
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
// SDF letters: the fill over a wide feathered halo (they read over any band).
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uFill, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.7;
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.1, 0.49, d) * uHaloA;
    float a = max(fill, halo);
    vec3 c = mix(uHalo, uFill, fill / max(a, 1e-4));
    frag = vec4(c * a, a) * uAlpha;
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace pinkvibe_shaders

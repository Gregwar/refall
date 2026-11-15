#pragma once
// FRACTAL's shaders, in display (sRGB) colors: the infinite Julia flower behind everything (one fullscreen pass, with
// the well's calm panel and its soft shadow), the flat blocks.

namespace fractal_shaders {

// Shared helpers (prepended after the #version line).
inline const char* PAINT_GLSL = R"(
float AA = 0.01;
float cov(float d) { return 1.0 - smoothstep(-AA, AA, d); }
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
)";

// ---------------------------------------------------------------- the fractal
// The filled Julia set of z -> z^2 + c in flat pastel bands, seen through w = uFix + uMul * (p - uCenter): the host
// zooms for ever into a self-similar point of it (a repelling periodic point or one of its preimages), drawing it at
// a depth between D0 and D0 + 1 so that the picture loops seamlessly, and the contour keeps revealing finer petals.
// The bands are the smooth escape count outside and the convergence count inside (toward the attracting cycle), plus
// uColOff, which the host raises by the iterations of a level each time it wraps the depth: each band keeps one flat
// color of the gradient while it grows, and the new ones bloom with the next colors. A fine line between bands and a
// soft shadow under each, like cut paper; where the bands get finer than a pixel (along the contour) they melt into
// the wash color, so the endless detail never shimmers.
inline const char* FRACTAL_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec3 uBand[6], uDropBand[6];  // the gradient's stops, calm and in the drop
uniform float uDropMix;
uniform vec3 uLine, uWash, uShade, uWellFill, uWellLine;
uniform vec2 uCenter;        // where uFix sits on screen (board units)
uniform vec2 uC;             // the Julia parameter
uniform vec2 uFix;           // the view's center in the complex plane
uniform vec2 uMul;           // board units -> the complex plane (scale and turn)
uniform float uColOff;       // the bands' count offset (the levels zoomed so far, in iterations)
uniform vec2 uCycle[8];      // the attracting cycle (the inside) and its period
uniform int uPeriod;
uniform float uDesat, uGrain;
uniform vec4 uWell;          // the well's half extents, its corner, 1 shown / 0 not (a break between songs)
uniform vec3 uPrev[4];       // the hold and next previews: center, half size (a soft pale halo behind each)
const int MAXIT = 200;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }
vec3 stopc(float i) {
    int k = int(mod(i, 6.0));
    return mix(uBand[k], uDropBand[k], uDropMix);
}
vec3 grad(float t) {
    float i = floor(t);
    return mix(stopc(i), stopc(i + 1.0), smoothstep(0.0, 1.0, t - i));
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 z = uFix + cmul(uMul, p - uCenter);
    float g = -1.0;
    bool inside = false;
    const float EPS = 1e-3;
    for (int i = 0; i < MAXIT; i++) {
        z = cmul(z, z) + uC;
        float m = dot(z, z);
        if (m > 1e4) {
            g = float(i) + 1.0 - log2(log(m) * 0.5 / log(100.0));
            break;
        }
        for (int k = 0; k < 8; k++) {
            if (k >= uPeriod) break;
            vec2 e = z - uCycle[k];
            if (dot(e, e) < EPS * EPS) {
                inside = true;
                g = float(i);
                break;
            }
        }
        if (inside) break;
    }
    vec3 col;
    if (g < 0.0) col = uWash;                 // (no answer within MAXIT: on the contour itself)
    else {
        float G = g + uColOff;                // the band count in the world (a band an iteration)
        float bi = floor(G), f = G - bi;
        col = grad(bi * 0.5 + (inside ? 3.0 : 0.0));
        float w = max(fwidth(G), 1e-4);       // bands a pixel
        // cut paper: the band inside lies over this one (its soft shadow), a fine line between them
        col = mix(col, col * uShade, 0.22 * exp(-f / (w * 9.0)));
        col = mix(col, uLine, 0.8 * (1.0 - smoothstep(0.6, 1.4, min(f, 1.0 - f) / w)));
        // finer than a pixel (along the contour): the wash, still
        col = mix(col, uWash, smoothstep(0.15, 0.5, w));
    }
    col *= 1.0 + uGrain * (hash(floor(gl_FragCoord.xy)) - 0.5);
    AA = max(fwidth(p.x), 1e-4) * 0.75;
    if (uWell.w > 0.5) {
        // the previews: a soft pale halo behind each, so the small pieces read over the flower (no edge, no panel)
        for (int i = 0; i < 4; i++) {
            float dp = dBox(p - uPrev[i].xy, vec2(uPrev[i].z * 1.35, uPrev[i].z * 0.8), uPrev[i].z * 0.8);
            col = mix(col, uWellFill, 0.78 * (1.0 - smoothstep(-0.5, 0.9, dp)));
        }
        float ds = dBox(p - vec2(0.18, -0.3), uWell.xy, uWell.z);
        col = mix(col, col * uShade, 0.35 * (1.0 - smoothstep(-0.3, 1.1, ds)));
        float dw = dBox(p, uWell.xy, uWell.z);
        col = mix(col, uWellLine, cov(dw));
        col = mix(col, uWellFill, cov(dw + 0.07));
    }
    // slow motion: dusk (the colors drained toward lilac)
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.88, 0.86, 1.02), uDesat);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // type, alpha, mode (0 block, 1 ghost), scale
layout(location = 3) in vec4 iC; // color, k (< 0 a lock's brightening, > 0 the game over's greying)
uniform mat4 uVP;
out vec2 vU;
flat out vec4 vB, vC;
void main() {
    vec2 l = aCorner * 0.58;           // the cell spans -0.5..0.5
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
uniform vec3 uShine;
uniform float uGap;        // the hairline between cells (board units, the same at any scale)
uniform float uLine;       // the ghost's line (board units)
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.6;
    float sc = max(vB.w, 0.3);
    vec3 col = vC.rgb;
    // one flat square of the piece's color, a hairline gap around it, corners barely rounded
    float d = dBox(u, vec2(0.5 - uGap * 0.5 / sc), 0.035);
    float a = cov(d);
    vec3 c = col;
    if (abs(vB.z - 1.0) < 0.5) {
        // the ghost: a faint wash of the piece's color inside a thin line of it
        float W = uLine / sc;
        a = max(a * 0.16, cov(abs(d + W * 0.5) - W * 0.5) * 0.7);
    } else if (vC.w < 0.0) c = mix(c, uShine, -vC.w);   // a lock's brief brightening
    else {
        float l = dot(c, vec3(0.299, 0.587, 0.114));     // the game over's greying
        c = mix(c, vec3(l) * 0.8 + 0.1, vC.w);
    }
    float al = a * vB.y;
    frag = vec4(c * al, al);
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace fractal_shaders

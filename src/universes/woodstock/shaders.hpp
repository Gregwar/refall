#pragma once
// WOODSTOCK's shaders, in display (sRGB) colors: the pastel groovy bands behind everything (one fullscreen pass,
// with the well's calm panel and its soft shadow), the flat blocks.

namespace woodstock_shaders {

// Shared helpers (prepended after the #version line). Distances in board units; AA set per fragment.
inline const char* PAINT_GLSL = R"(
const float TAU = 6.2831853;
float AA = 0.01;
float cov(float d) { return 1.0 - smoothstep(-AA, AA, d); }
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
)";

// ---------------------------------------------------------------- the bands
// Huge rainbow arcs rising from below the screen, layered like cut paper (each band laid over the next one out,
// a soft shadow along its edge, a thin pale line between them), melting into slow waves that travel along them.
inline const char* BG_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec3 uBand[6], uDropBand[6];  // the bands' colors, calm and in the drop
uniform float uDropMix;      // 0 the calm palette .. 1 the drop's
uniform vec3 uLine, uShade, uWellFill, uWellLine;
uniform vec2 uCenter;        // the arcs' center (below the screen, wandering slowly)
uniform float uFlow;         // the bands rising (in bands)
uniform float uPhase;        // the waves' clock
uniform float uWobble;       // the waves' depth (board units)
uniform float uBreath;       // the bands swelling with the bars (in bands)
uniform float uTrip;         // the drop: 0 the arcs .. 1 the kaleidoscope (petals, a spiral, rings around the well)
uniform float uSpin;         // the petals' turn
uniform float uZoom;         // the pattern's scale (the drop's pulse on the beat)
uniform vec3 uGlow;
uniform float uGlowAmt;
uniform float uDesat;        // slow motion's dusk
uniform float uGrain;
uniform vec4 uRipple[4];     // the clears' soft ripples: front radius, strength
uniform vec4 uWell;          // the well's half extents, its corner, 1 shown / 0 not (a break between songs)
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    AA = max(fwidth(p.x), 1e-4) * 0.75;
    // (the drop's zoom pulse on the beat, around the center)
    vec2 d = (p - uCenter) * uZoom;
    float an = atan(d.y, d.x);
    float r0 = length(d * vec2(mix(0.84, 1.0, uTrip), 1.0));
    // the melt: slow waves along the arcs and a few low harmonics across them, deeper further out
    float w = uWobble * mix(smoothstep(3.0, 14.0, r0), smoothstep(1.5, 8.0, r0), uTrip)
            * (0.6 + 0.4 * smoothstep(10.0, 30.0, r0));
    // (the drop: longer, simpler waves, fewer bumps along each ring)
    float r = r0 + w * (0.9 * sin(p.x * mix(0.33, 0.17, uTrip) + r0 * 0.12 - uPhase * 1.3)
                      + (1.0 - 0.75 * uTrip) * (0.6 * sin(3.0 * an + uPhase * 0.7 + r0 * 0.13)
                                              + 0.4 * sin(5.0 * an - uPhase * 0.45 - r0 * 0.07)));
    // the drop: the rings fold into six slow petals (a kaleidoscope), turning
    float ad = atan(-d.x, d.y) + uSpin;      // from straight up (its seam points down, under the well)
    r += uTrip * (0.25 + 0.025 * r0) * smoothstep(1.0, 6.0, r0) * sin(6.0 * ad - uPhase * 0.5 + r0 * 0.06);
    r = max(r, 0.0);
    const float R0 = 5.0, K = 6.3;
    float s = log(1.0 + r / R0) * K - uFlow - uBreath;
    // ... and twist into a six-armed spiral (six bands a turn: the colors meet again)
    float sBase = s;                         // (its derivatives, free of the spiral's seam)
    s += uTrip * 6.0 * (atan(-d.x, d.y) / TAU);
    // the clears' ripples: the bands pushed out softly as a front passes
    for (int k = 0; k < 4; k++) {
        float wd = (r0 - uRipple[k].x) / 2.2;
        s -= uRipple[k].y * exp(-wd * wd);
    }
    float i = floor(s), f = fract(s);
    float bw = (R0 + r) / K;                                  // a band's width (board units)
    // the colors sliding from band to band (the drop's slow cycle; still otherwise)
    int bi = int(mod(i, 6.0));
    vec3 col = mix(uBand[bi], uDropBand[bi], uDropMix);   // (each band one whole flat color)
    // cut paper: the band inside lies over this one, its soft shadow along the edge
    float de = f * bw;
    col = mix(col, col * uShade, 0.22 * exp(-de / 0.55));
    // the thin pale line between two bands
    float dl = min(f, 1.0 - f) * bw;
    float aa = fwidth(sBase) * bw * 0.8 + 0.004;
    col = mix(col, uLine, 0.8 * (1.0 - smoothstep(0.05 - aa, 0.05 + aa, dl)));
    // the drop's light, glowing from the center behind the well
    col = mix(col, uGlow, clamp(uGlowAmt * exp(-r0 / 10.0), 0.0, 1.0));
    // the paper's faint grain
    col *= 1.0 + uGrain * (hash(floor(gl_FragCoord.xy)) - 0.5);
    // the well: a soft shadow under it, a calm panel inside a slim line
    if (uWell.w > 0.5) {
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

} // namespace woodstock_shaders

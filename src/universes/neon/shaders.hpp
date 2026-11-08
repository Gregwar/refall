#pragma once
// NEON's shaders (see neon.cpp).
//
// SKY_FS paints the night (a near-black gradient, faint stars and dust, the colored mist of a peak, the drop's glow
// over the city); BUILD_FS the skyline, one watercolor wash per building (instanced quads); VEIL_FS the well (a dark
// veil over the city behind the board, two faint posts and a floor line); PIECE_FS the pieces, one luminous wash per
// piece (its cells merged into one rounded shape, instanced per cell); WISP_FS the pigment lifting off a cleared row;
// DOWN_FS / BLUR_FS the glow and FINAL_FS the frame. Every color of the world comes from one spectrum: the variant's
// five stops interpolated in OKLab across the screen, its hue turned by a phase (the song's sections).
#include <string>

namespace neon_shaders {

inline const char* COMMON = R"(
uniform vec3 uLab[5];   // the variant's spectrum, five stops in OKLab
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.9375;
}
float fbm3(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.875;
}
vec3 fromLab(vec3 L) {
    float l = L.x + 0.3963377774 * L.y + 0.2158037573 * L.z;
    float m = L.x - 0.1055613458 * L.y - 0.0638541728 * L.z;
    float s = L.x - 0.0894841775 * L.y - 1.2914855480 * L.z;
    l = l * l * l; m = m * m * m; s = s * s * s;
    return vec3(4.0767416621 * l - 3.3077085540 * m + 0.2309624716 * s,
                -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
                -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s);
}
// The spectrum at u (0 left .. 1 right) in the phase phi (the song's sections): the phase slides the spectrum
// sideways (its end colors held) and turns its hue a little; linear RGB.
vec3 spectrum(float u, float phi) {
    u = clamp(u - phi * 0.5, 0.0, 1.0) * 4.0;
    phi *= 0.45;
    int i = int(min(floor(u), 3.0));
    float f = u - float(i);
    vec3 a = uLab[i], b = uLab[i + 1];
    vec3 lab = mix(a, b, f);
    float C = mix(length(a.yz), length(b.yz), f);
    float h = atan(lab.z, lab.y) + phi;
    lab.yz = vec2(cos(h), sin(h)) * C;
    vec3 c = fromLab(lab);
    // back into the gamut without going grey: lower the chroma's excess, keep the hue
    float mn = min(min(c.r, c.g), c.b);
    if (mn < 0.0) { float L = dot(c, vec3(0.2126, 0.7152, 0.0722)); c = mix(vec3(max(L, 0.0)), c, max(L, 0.0) / max(L - mn, 1e-4)); }
    return max(c, 0.0);
}
float fillA(float d, float aa) { return 1.0 - smoothstep(-aa, aa, d); }
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
)";

// ---------------------------------------------------------------- the night (full screen)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec3 uSkyTop, uSkyLow;
uniform float uTime, uTwinkle, uMist, uDrop, uPhi, uSeed, uSkyline, uSlow;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float u = (w.x / uView.z) * 0.5 + 0.5;
    float t = smoothstep(uView.y - uView.w, uView.y + uView.w, w.y);
    vec3 c = mix(uSkyLow, uSkyTop, pow(t, 0.8));
    // the city's faint glow low in the sky, in the spectrum, and a soft dusty haze
    float low = exp(-max(w.y - uSkyline, 0.0) / 6.0);
    c += spectrum(u, uPhi) * 0.0035 * low;
    float dust = fbm(w * vec2(0.09, 0.16) + vec2(uSeed, 0.0) + vec2(uTime * 0.004, 0.0));
    c *= 0.86 + 0.32 * dust;
    // stars: faint, sparse, a few a little brighter; twinkling slowly from a chorus on
    vec2 g = w * 2.2;
    vec2 id = floor(g), f = fract(g) - 0.5;
    float h = hash12(id + uSeed);
    if (h > 0.86) {
        vec2 o = (vec2(hash12(id + 7.1), hash12(id + 3.3)) - 0.5) * 0.7;
        float r = length(f - o);
        float b = (h - 0.86) / 0.14;
        float tw = 1.0 + uTwinkle * 0.45 * sin(uTime * (0.5 + 0.7 * hash12(id + 1.7)) + h * 40.0);
        float s = exp(-r * r / 0.0035) * (0.012 + 0.07 * b * b * b) * tw;
        // fewer stars low down where the city's glow is
        s *= smoothstep(uSkyline - 2.0, uSkyline + 6.0, w.y);
        c += vec3(0.85, 0.88, 1.0) * s;
    }
    // a peak: colored mist rising slowly behind the skyline
    if (uMist > 0.001) {
        float m = fbm(vec2(w.x * 0.11, w.y * 0.22 - uTime * 0.05) + uSeed * 3.0);
        m = smoothstep(0.35, 0.85, m);
        float band = smoothstep(uSkyline + 9.0, uSkyline - 1.0, w.y);
        c += spectrum(u, uPhi) * 0.02 * uMist * m * band;
    }
    // the drop: the spectrum glows over the whole city
    if (uDrop > 0.001) {
        float m = 0.6 + 0.6 * fbm(vec2(w.x * 0.08, w.y * 0.12 - uTime * 0.03) + 5.0);
        float band = exp(-max(w.y - uSkyline + 2.0, 0.0) / 5.0);
        c += spectrum(u, uPhi) * 0.05 * uDrop * band * m;
    }
    // a fine grain under a step of value
    c *= 1.0 + (hash12(floor(gl_FragCoord.xy) + fract(uSeed) * 91.0) - 0.5) * 0.06;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the skyline (instanced quads)
// a: center x, base y, half width, height. b: roof kind, seed, paint (0..1), alpha.
// c: phase A, phase B, A->B blend, tint (0 a dark ghost .. 1 painted in color). d: layer (0 back, 1 front), windows, -, -.
inline const char* BUILD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vW;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 lo = vec2(aA.x - aA.z - 1.0, aA.y - 0.2);
    vec2 hi = vec2(aA.x + aA.z + 1.0, aA.y + aA.w + 3.2);
    vec2 wp = mix(lo, hi, aPos * 0.5 + 0.5);
    vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

inline const char* BUILD_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uGhost;
uniform float uBeat, uWinStatic, uWinVar, uWinGlow, uDrop, uRich, uPhi, uKick, uAA;
// the building's outline (negative inside), in local units from its foot
float shape(vec2 p, float hw, float h, int kind, float seed) {
    float d = sdBox(p - vec2(0.0, h * 0.5), vec2(hw, h * 0.5));
    if (kind == 1) {
        // a setback: the upper floors narrower
        float h1 = h * 0.68;
        d = min(sdBox(p - vec2(0.0, h1 * 0.5), vec2(hw, h1 * 0.5)),
                sdBox(p - vec2(hw * 0.12, h * 0.5), vec2(hw * 0.62, h * 0.5)));
    } else if (kind == 2) {
        // a spire on a slender block
        float h1 = h * 0.86;
        d = sdBox(p - vec2(0.0, h1 * 0.5), vec2(hw, h1 * 0.5));
        d = min(d, sdBox(p - vec2(0.0, h1 + 0.25), vec2(hw * 0.55, 0.25)));
        d = min(d, sdBox(p - vec2(0.0, h1 + 1.0), vec2(0.045, 1.0)));
    } else if (kind == 3) {
        // a dome
        float h1 = h - hw * 0.75;
        d = min(sdBox(p - vec2(0.0, h1 * 0.5), vec2(hw, h1 * 0.5)), length(p - vec2(0.0, h1)) - hw * 0.8);
    } else if (kind == 4) {
        // a slanted roof
        float top = h - hw * 0.7 * (0.5 + 0.5 * p.x / hw) * (seed > 0.5 ? 1.0 : -1.0) - hw * 0.35 * (seed > 0.5 ? -1.0 : 1.0);
        d = max(abs(p.x) - hw, max(-p.y, p.y - top));
    } else if (kind == 5) {
        // stepped twins: a tall middle, lower shoulders
        d = min(sdBox(p - vec2(0.0, h * 0.5), vec2(hw * 0.45, h * 0.5)),
                sdBox(p - vec2(0.0, h * 0.36), vec2(hw, h * 0.36)));
    }
    return d;
}
void main() {
    float hw = vA.z, h = vA.w;
    vec2 p = vW - vA.xy;
    int kind = int(vB.x + 0.5);
    float seed = vB.y;
    float paint = vB.z;
    float front = vD.x;
    vec2 nw = vW * 0.9 + seed * 37.0;
    // a ragged, bleeding edge
    float d = shape(p, hw, h, kind, fract(seed * 7.3));
    float topY = vA.y + h;
    // toward its top the wash grows wetter and more ragged, then dissolves into the night
    float wet = smoothstep(topY - 3.5, topY + 0.5, vW.y);
    d += (fbm3(nw * 0.9) - 0.5) * (0.22 + 0.5 * wet) + (vnoise(nw * 5.0) - 0.5) * (0.05 + 0.08 * wet);
    float cov = smoothstep(uAA + 0.12 * wet, -uAA - 0.12 * wet, d);
    // wet bleed beyond the edge: a soft feather of the wash
    float feather = exp(-max(d, 0.0) / 0.22) * (1.0 - cov) * 0.35;
    // the top fades into the night
    float fade = mix(0.0, 1.0, smoothstep(topY + 0.3, topY - 3.2 - 0.25 * h, vW.y + (fbm3(nw * 0.6 + 7.0) - 0.5) * 1.6));
    // painted in from the foot up, a wet front with a darker tide
    float front_y = vA.y - 0.5 + (h + 4.0) * paint;
    float pn = (fbm3(nw * 1.3 + 4.0) - 0.5) * 0.9;
    float reveal = smoothstep(front_y + 0.25, front_y - 0.35, vW.y + pn);
    float tide = exp(-abs(vW.y + pn - front_y + 0.1) / 0.18) * (1.0 - step(0.999, paint));
    float a = (cov + feather) * fade * reveal * vB.w;
    if (a <= 0.002) discard;
    // the wash: cloudy, blooms, granulation, a darker dried rim
    float big = fbm(nw * 0.45);
    float dens = 0.7 + 0.6 * (big - 0.5);
    float bl = fbm3(nw * 0.8 + 11.0);
    float bloomIn = smoothstep(0.6, 0.7, bl);
    dens *= 1.0 - 0.3 * bloomIn;
    dens += 0.25 * exp(-abs(bl - 0.62) / 0.02) * (1.0 - bloomIn);
    dens += 0.45 * exp(min(d, 0.0) / 0.12) * cov;
    dens += 0.5 * tide;
    dens *= 0.88 + 0.24 * vnoise(nw * 14.0);
    // the corner where the section and the universe are written stays quiet (fainter, no windows)
    bool quiet = vW.x < uView.x - uView.z + 6.2 && vW.y < uView.y - uView.w + 4.2;
    // its color: the spectrum at its place on the screen, of the song's phase it was painted in, dark
    float u = (vW.x / uView.z) * 0.5 + 0.5;
    vec3 sp = mix(spectrum(u + (big - 0.5) * 0.08, vC.x), spectrum(u + (big - 0.5) * 0.08, vC.y), vC.z);
    // dark tones of it: deepened (dark yellows turn to warm browns, not olive)
    sp = sp * sp * 1.25 + sp * 0.05;
    float lvl = mix(0.042, 0.066, front) * (1.0 + 0.45 * uRich);
    vec3 col = mix(uGhost * (0.75 + 0.5 * big), sp * lvl, vC.w);
    if (quiet) a *= 0.7;
    col *= 0.35 + 0.95 * dens;
    // the drop: the spectrum washes over the city, luminous
    if (uDrop > 0.001) {
        vec3 dc = spectrum(u + (big - 0.5) * 0.1, uPhi) * (0.2 + 0.16 * front) * (0.6 + 0.5 * dens) * (0.8 + 0.4 * bloomIn);
        col = mix(col, dc, uDrop * 0.85);
    }
    // windows: a grid on the body, a few lit; some come and go on the beat grid, eased
    float win = 0.0;
    if (vD.y > 0.5 && d < -0.15 && p.y > 0.35 && !quiet) {
        vec2 pitch = vec2(0.42, 0.6);
        vec2 g = (p - vec2(-hw, 0.35)) / pitch;
        vec2 id = floor(g), f = fract(g);
        vec2 wo = (vec2(hash12(id + 5.0), hash12(id + 9.0)) - 0.5) * 0.04;
        float r = sdBox((f - 0.5) * pitch - wo, vec2(0.055, 0.085)) - 0.02;
        r += (vnoise(vW * 18.0) - 0.5) * 0.03;
        float shapeW = 1.0 - smoothstep(-0.03, 0.03, r);
        float hs = hash12(id + seed * 113.0);
        float on = smoothstep(hs - 0.015, hs + 0.015, uWinStatic);
        float period = hs > 0.66 ? 4.0 : (hs > 0.33 ? 8.0 : 16.0);
        float off = hash12(id + seed * 51.0 + 3.0) * period;
        float tb = uBeat + off;
        float slot = floor(tb / period);
        float k = smoothstep(0.0, 1.2, tb - slot * period);
        float v0 = step(hash12(id + vec2(slot - 1.0, seed * 7.0)), uWinVar);
        float v1 = step(hash12(id + vec2(slot, seed * 7.0)), uWinVar);
        on = max(on, mix(v0, v1, k));
        float kickW = step(0.9, hash12(id + seed * 3.0 + 9.0)) * uKick;
        win = shapeW * clamp(on + kickW * 0.8, 0.0, 1.2) * smoothstep(0.85, 1.0, paint) * smoothstep(topY - 0.4, topY - 1.6, vW.y);
    }
    vec3 wc = mix(vec3(1.0, 0.72, 0.38), sp, 0.22) * uWinGlow * (0.7 + 0.6 * hash12(floor(p * 3.0) + seed));
    col = mix(col, wc, win * 0.85 * a);
    float A = clamp(a + win * 0.3 * a, 0.0, 1.0);
    frag = vec4(col * A, A);
}
)";

// ---------------------------------------------------------------- the well (full screen)
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uSkyLow, uLine;
uniform float uAA, uLineA;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // a dark veil over the city behind the board (soft-edged)
    float d = sdBox(w, vec2(5.1, 10.1)) - 0.25;
    float veil = 0.82 * (1.0 - smoothstep(-0.2, 0.7, d));
    vec3 c = uSkyLow * 0.6;
    float a = veil;
    // two faint posts fading upward and a floor line
    float post = abs(abs(w.x) - 5.2) - 0.012;
    float pa = (1.0 - smoothstep(-uAA, uAA, post)) * step(-10.2, w.y) * smoothstep(10.2, -6.0, w.y);
    float floorL = abs(w.y + 10.2) - 0.012;
    float fa = (1.0 - smoothstep(-uAA, uAA, floorL)) * step(abs(w.x), 5.212);
    float l = max(pa, fa) * uLineA;
    c = mix(c, uLine, l);
    a = max(a, l);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the pieces (instanced per cell)
// a: x, y (cell center), half (board units), mask. b: noise origin x, y, seed, phase.
// c: x offset of the color (previews), brightness, alpha, kind (0 wash, 1 ghost, 2 lifting, 3 running).
// d: progress (lift / run), hue jitter, -, -.
inline const char* PIECE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vLocal;   // cell units, the cell spans -0.5 .. 0.5
out vec2 vWorld;
flat out vec4 vA, vB, vC, vD;
void main() {
    float down = aC.w > 2.5 ? 3.2 * clamp(aD.x * 1.5, 0.0, 1.0) : 0.0;
    vec2 lo = vec2(-0.56, -0.56 - down), hi = vec2(0.56, 0.56);
    vec2 l = mix(lo, hi, aPos * 0.5 + 0.5);
    vLocal = l;
    vec2 wp = aA.xy + l * 2.0 * aA.z;
    vWorld = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// One piece's outline: its own cells merge into one shape with round outer corners and filleted inner ones, a gap
// parts it from the others (mask bits: left right down up, down-left down-right up-left up-right). Exact enough
// within the cell (the fillets are shared with the neighbours, so no seam shows inside a piece).
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float unionRound(float a, float b, float r) {
    vec2 u = max(vec2(r - a, r - b), vec2(0.0));
    return max(r, min(a, b)) - length(u);
}
const float GAP = 0.065, RO = 0.24, RI = 0.11;
float shapeSdf(vec2 p, int m) {
    vec2 q = abs(p);
    bool hx = p.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = p.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = p.x < 0.0 ? (p.y < 0.0 ? 4 : 6) : (p.y < 0.0 ? 5 : 7);
    bool dg = has(m, db);
    float c = 0.5 - GAP;
    if (!hx && !vy) { vec2 k = q - (c - RO); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - RO; }
    if (hx && !vy) return dg ? unionRound(q.y - c, 0.5 + GAP - q.x, RI) : q.y - c;
    if (!hx && vy) return dg ? unionRound(q.x - c, 0.5 + GAP - q.y, RI) : q.x - c;
    if (dg) return -0.6;
    return unionRound(q.x - c, q.y - c, RI);
}
bool ownCell(vec2 p, int m) {
    if (p.x < -0.5 && has(m, 0)) return false;
    if (p.x > 0.5 && has(m, 1)) return false;
    if (p.y < -0.5 && has(m, 2)) return false;
    if (p.y > 0.5 && has(m, 3)) return false;
    return true;
}
)";

inline const char* PIECE_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform float uGlow;
void main() {
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    int kind = int(vC.w + 0.5);
    if (!ownCell(p, m)) discard;
    float aa = max(length(fwidth(p)) * 0.8, 1e-4);
    vec2 nw = vB.xy + vec2(p.x, p.y);   // the piece's own wash, continuous across its cells
    float seed = vB.z;
    float u = (vWorld.x + vC.x + 5.0) / 10.0;
    float d = shapeSdf(p, m);
    // a slightly ragged edge
    d += (fbm3(nw * 2.1 + seed) - 0.5) * 0.05 + (vnoise(nw * 11.0 + seed) - 0.5) * 0.014;
    float run = kind == 3 ? vD.x : 0.0;
    if (kind == 3 && p.y < 0.0) {
        // running down: drips under the cell, thin runs ending in a bead
        for (int i = 0; i < 3; i++) {
            float hx = fract(sin(seed * 12.9 + float(i) * 78.2 + vB.x * 3.1) * 43758.5);
            float xd = -0.34 + 0.68 * hx;
            float len = run * (0.6 + 2.2 * fract(hx * 7.13));
            float w = 0.05 + 0.05 * fract(hx * 3.7);
            float yy = clamp(p.y, -0.42 - len, -0.3);
            float cap = length(vec2(p.x - xd, p.y - yy)) - w;
            float bead = length(vec2(p.x - xd, p.y + 0.42 + len)) - w * 1.6;
            d = min(d, min(cap, bead));
        }
    }
    if (kind == 1) {
        // the ghost: the piece's outline, a thin line of its color
        float l = 1.0 - smoothstep(0.0, aa * 1.5, abs(d + 0.02) - 0.012);
        vec3 gc = spectrum(u, vB.w) * 0.55;
        float a = l * 0.55 * vC.z;
        if (a <= 0.002) discard;
        frag = vec4(gc * a, a);
        return;
    }
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.002) discard;
    float inside = max(-d, 0.0);
    // the wash: large cloudy blotches, wet-in-wet blooms with darker ragged fringes, granulation, a dried rim
    float big = fbm(nw * 0.5 + seed * 7.1);
    float mid = fbm3(nw * 1.4 + seed * 3.3 + 2.0);
    float dens = 0.5 + 1.1 * (big - 0.5) + 0.45 * (mid - 0.5);
    float bl = fbm3(nw * 0.95 + seed * 5.7 + 11.0);
    float bloomIn = smoothstep(0.58, 0.64, bl);
    float fringe = exp(-abs(bl - 0.59) / 0.012) * (1.0 - bloomIn);
    dens = mix(dens, dens * 0.35 - 0.05, bloomIn);
    dens += 0.35 * fringe;
    float rim = exp(-inside / 0.06);
    dens += 0.75 * rim + 0.2 * exp(-inside / 0.18);
    // pigment pooled toward the bottom of the shape
    dens += 0.08 * clamp(-p.y, -0.5, 0.5);
    float gran = vnoise(nw * 19.0 + seed) * 0.55 + vnoise(nw * 41.0 + seed * 2.0) * 0.45;
    dens += (gran - 0.5) * (0.18 + 0.25 * clamp(dens, 0.0, 1.0));
    dens = clamp(dens, -0.1, 1.5);
    // the color: the spectrum where it is, a little separated in the wash (wet-in-wet), the piece's own nuance
    float ju = u + vD.y + (mid - 0.5) * 0.1 + (big - 0.5) * 0.06;
    vec3 c = spectrum(ju, vB.w);
    // luminous where the pigment is thin, deep and saturated where it pooled (never toward grey)
    float k = clamp(dens / 1.2, 0.0, 1.0);
    float lum = mix(1.45, 0.3, pow(k, 0.85));
    vec3 deep = c * c * 1.15;   // pooled pigment: deeper and more saturated
    vec3 col = mix(c * lum, deep * lum * 1.6, k * 0.5);
    float L = dot(c, vec3(0.2126, 0.7152, 0.0722));
    col = mix(col, (c * 0.8 + vec3(L) * 0.35) * 1.5, 0.12 * bloomIn * (1.0 - rim));
    col *= vC.y;
    float a = cov * vC.z;
    if (kind == 2) {
        // lifting: the pigment dissolves, the thinnest first, while it rises away as wisps
        float t = vD.x;
        float g = fbm3(nw * 1.7 + seed * 3.1) * 0.7 + vnoise(nw * 6.0 + seed) * 0.3;
        a *= smoothstep(t * 1.25 - 0.18, t * 1.25 + 0.02, g) * (1.0 - t * t);
        col *= 1.0 + 0.4 * t;
    }
    if (kind == 3) a *= 1.0 - smoothstep(0.35, 1.0, run);
    if (a <= 0.002) discard;
    frag = vec4(col * uGlow * a, a);
}
)";

// ---------------------------------------------------------------- pigment in the air
// a: x, y, radius, rotation. b: u, phase, alpha, stretch. c: seed, -, -, -.
inline const char* WISP_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC;
void main() {
    vec2 l = aPos * vec2(aA.z * aB.w, aA.z);
    float c = cos(aA.w), s = sin(aA.w);
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vL = aPos;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* WISP_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB, vC;
out vec4 frag;
void main() {
    float seed = vC.x;
    float n = fbm3(vL * 1.6 + seed * 17.0);
    float r = length(vL) + (n - 0.5) * 0.55;
    float body = 1.0 - smoothstep(0.25, 0.95, r);
    float edge = exp(-abs(r - 0.7) / 0.06) * 0.35;
    float a = (body * (0.55 + 0.6 * n) + edge * body) * vB.z;
    if (a <= 0.002) discard;
    vec3 c = spectrum(vB.x + (n - 0.5) * 0.08, vB.y) * (0.5 + 0.45 * n);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the glow
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uBlockCut;   // how much of the blocks' light is kept out of the glow (the scene's alpha is the stack)
void main() {
    vec4 s0 = texture(uSrc, vUv + uTexel * vec2(-1, -1)), s1 = texture(uSrc, vUv + uTexel * vec2(1, -1)),
         s2 = texture(uSrc, vUv + uTexel * vec2(-1, 1)), s3 = texture(uSrc, vUv + uTexel * vec2(1, 1));
    vec3 c = s0.rgb * (1.0 - uBlockCut * s0.a) + s1.rgb * (1.0 - uBlockCut * s1.a) +
             s2.rgb * (1.0 - uBlockCut * s2.a) + s3.rgb * (1.0 - uBlockCut * s3.a);
    c *= 0.25;
    frag = vec4(max(c - vec3(0.05), 0.0), 1.0);
}
)";
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

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: deep night, the colors drained
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c, 0.35) * vec3(0.7, 0.78, 1.0) * 0.8, uSlow);
    // a soft knee: the glowing washes never burn to white
    c = c / (1.0 + 0.3 * max(max(c.r, c.g), c.b));
    // a soft vignette
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.28 * dot(v, v) * 2.0;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
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
// Pale letters with a soft halo of the night around them.
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.45 - w, 0.45 + w, d);
    float halo = smoothstep(0.2, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a fragment shader with the common functions (and the piece outline) inserted after the version line.
inline std::string withCommon(const char* src, bool shape = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace neon_shaders

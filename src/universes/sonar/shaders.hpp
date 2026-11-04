#pragma once
// SONAR's shaders (see sonar.cpp).
//
// WORLD_FS paints the sea (near-black water, the seabed, its rocks and a wreck in front, a sunken city behind),
// every surface dark until an echo returns from it: the pings (local, on the beat grid) light the surfaces facing
// them for a moment, the charted patches (one per cleared row) keep them faintly lit for good, with survey lines
// along the seabed's strata; a few particles in the water show only where an echo passes. PIECE_FS draws the
// blocks (glowing solid cubes, instanced per cell), MOTE_FS the soft points of light (a cleared row's light sinking
// to the seabed, a hard drop's contact), DOWN_FS / BLUR_FS the glow and FINAL_FS the frame.
#include <string>

namespace sonar_shaders {

inline const char* COMMON = R"(
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
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
)";

// ---------------------------------------------------------------- the sea (full screen)
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;             // center, half extents (board units)
uniform vec3 uWaterTop, uWaterLow, uEcho, uEchoCity;
uniform float uTime, uSeed, uDecay;
uniform vec3 uS;                // the seabed's phases (the song)
uniform vec4 uPing[8];          // x, y, age (s; < 0: none), strength
uniform float uPingR[8];        // reach
uniform vec4 uChart[64];        // x, y, radius, amount
uniform int uCharts;
uniform float uBed, uCity, uWin, uSlow, uFade;

float bedY(float x) {
    float ax = abs(x);
    float y = -9.2 + 0.42 * sin(0.29 * x + uS.x) + 0.22 * sin(0.77 * x + uS.y) + 0.1 * sin(1.9 * x + uS.z);
    y -= 1.6 * (1.0 - smoothstep(4.5, 9.5, ax));   // a trench under the well
    float r = smoothstep(12.0, 23.0, ax);
    return y + 3.0 * r * r;                         // slopes rising at the edges
}
const float WRECK_X = 12.6, TOWER_X = 14.4, SPIRE_X = 15.9, DOME_X = 17.7, COL_X = 20.2, PYR_X = -19.2, ARCH_X = -14.8;
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
float sdSeg(vec2 p, vec2 a, vec2 b, float r) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h) - r; }
float sdTrapezoid(vec2 p, float r1, float r2, float he) {
    vec2 k1 = vec2(r2, he), k2 = vec2(r2 - r1, 2.0 * he);
    p.x = abs(p.x);
    vec2 ca = vec2(p.x - min(p.x, (p.y < 0.0) ? r1 : r2), abs(p.y) - he);
    vec2 cb = p - k1 + k2 * clamp(dot(k1 - p, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}
float rock(vec2 p, float x, float r, float sq) {
    vec2 c = vec2(x, bedY(x) + r * 0.25);
    vec2 q = (p - c) * vec2(1.0, sq);
    return (length(q) - r + (vnoise(p * 2.3 + x) - 0.5) * 0.35 * r) / sq;
}

// The front: the seabed, its rocks and a wreck lying on its side.
float wreck(vec2 p) {
    const float K = 0.8;
    vec2 c = vec2(WRECK_X, bedY(WRECK_X) + 0.45);
    float a = 0.12;
    vec2 q = mat2(cos(a), -sin(a), sin(a), cos(a)) * (p - c) / K;
    float hull = sdTrapezoid(q, 2.6, 3.5, 0.68);
    hull = max(hull, -(length(q - vec2(1.35, 0.95)) - 0.62));          // a breach in the hull
    for (int i = 0; i < 4; i++) hull = max(hull, -(length(q - vec2(-2.35 + 0.62 * float(i), 0.12)) - 0.13)); // portholes
    float cabin = sdBox(q - vec2(-1.45, 1.05), vec2(0.85, 0.38)) - 0.03;
    cabin = max(cabin, -sdBox(q - vec2(-1.45, 1.1), vec2(0.55, 0.12)));
    float mast = sdSeg(q, vec2(0.25, 0.65), vec2(0.95, 3.3), 0.07);
    float spar = sdSeg(q, vec2(0.15, 2.5), vec2(1.55, 2.8), 0.05);
    return min(min(hull, cabin), min(mast, spar)) * K;
}
float front(vec2 p) {
    float bed = (p.y - bedY(p.x)) * 0.8 + (vnoise(p * vec2(0.9, 1.4) + uSeed) - 0.5) * 0.12;
    float r = min(min(rock(p, -9.3, 0.75, 1.5), rock(p, 8.3, 0.6, 1.4)), min(rock(p, -16.8, 0.8, 1.6), rock(p, 21.4, 1.3, 1.3)));
    r = min(r, min(rock(p, -12.0, 0.55, 1.3), rock(p, -20.6, 1.2, 1.2)));
    float d = smin(bed, r, 0.35);
    return min(d, wreck(p));
}

// The back: a sunken city (a tower, a dome, a colonnade on the right; a stepped pyramid and an arch on the left; a
// few broken columns under the left lettering).
float city(vec2 p, out float win) {
    win = 0.0;
    float d = 1e3;
    // right
    float b = bedY(TOWER_X) - 0.6;
    float top = -0.6;
    float tw = sdBox(p - vec2(TOWER_X, (b + top) * 0.5), vec2(0.72, (top - b) * 0.5));
    tw = min(tw, sdBox(p - vec2(TOWER_X, top + 0.35), vec2(0.46, 0.35)));
    tw = min(tw, sdSeg(p, vec2(TOWER_X, top + 0.7), vec2(TOWER_X, top + 1.9), 0.035));
    // the tower's windows
    {
        vec2 q = p - vec2(TOWER_X, b);
        vec2 g = vec2(q.x / 0.42, q.y / 0.75);
        vec2 id = floor(g + vec2(0.5, 0.0));
        vec2 f = g - vec2(id.x, id.y) - vec2(0.0, 0.5);
        if (abs(id.x) <= 1.0 && id.y >= 2.0 && q.y < top - b - 0.4) {
            float w = sdBox(f * vec2(0.42, 0.75), vec2(0.07, 0.15));
            float h = hash12(id + 17.0);
            win = max(win, (1.0 - smoothstep(-0.02, 0.02, w)) * step(0.45, h));
        }
    }
    d = min(d, tw);
    float b2 = bedY(DOME_X) - 0.4;
    float drum = sdBox(p - vec2(DOME_X, b2 + 1.4), vec2(1.55, 1.4));
    float dome = max(length(p - vec2(DOME_X, b2 + 2.8)) - 1.55, b2 + 2.8 - p.y);
    float lan = sdBox(p - vec2(DOME_X, b2 + 4.55), vec2(0.18, 0.25));
    d = min(d, min(min(drum, dome), lan));
    // arched openings in the drum
    for (int i = 0; i < 3; i++) {
        vec2 c = vec2(DOME_X - 0.9 + 0.9 * float(i), b2 + 1.2);
        float o = min(sdBox(p - c, vec2(0.2, 0.5)), length(p - c - vec2(0.0, 0.5)) - 0.2);
        d = max(d, -o);
    }
    float b3 = bedY(COL_X) - 0.3;
    float colT = b3 + 3.3;
    float cols = 1e3;
    for (int i = 0; i < 4; i++) cols = min(cols, sdBox(p - vec2(COL_X - 1.4 + 0.95 * float(i), (b3 + colT) * 0.5), vec2(0.16, (colT - b3) * 0.5)));
    cols = max(cols, -(sdBox(p - vec2(COL_X + 0.5, colT - 0.6), vec2(0.4, 1.0))));     // one column broken
    float arch = sdBox(p - vec2(COL_X, colT + 0.2), vec2(1.85, 0.2));
    arch = max(arch, -(sdBox(p - vec2(COL_X + 1.3, colT + 0.2), vec2(0.5, 0.4))));
    float ped = max(sdBox(p - vec2(COL_X - 0.3, colT + 0.6), vec2(1.55, 0.25)), (p.y - colT - 0.4) * 1.0 + abs(p.x - COL_X + 0.3) * 0.45 - 0.75);
    d = min(d, min(cols, min(arch, ped)));
    float sp = sdBox(p - vec2(SPIRE_X, (bedY(SPIRE_X) + 2.2) * 0.5), vec2(0.28, (2.2 - bedY(SPIRE_X)) * 0.5));
    sp = min(sp, max(length(p - vec2(SPIRE_X, 2.2)) - 0.28, 2.2 - p.y));
    d = min(d, sp);
    // left
    float b4 = bedY(PYR_X) - 0.3;
    float pyr = 1e3;
    for (int i = 0; i < 5; i++) pyr = min(pyr, sdBox(p - vec2(PYR_X, b4 + 0.32 + 0.64 * float(i)), vec2(2.7 - 0.5 * float(i), 0.32)));
    pyr = min(pyr, sdBox(p - vec2(PYR_X, b4 + 3.55), vec2(0.4, 0.35)));
    d = min(d, pyr);
    float b5 = bedY(ARCH_X) - 0.3;
    vec2 ac = vec2(ARCH_X, b5 + 2.4);
    float ring = max(abs(length(p - ac) - 1.05) - 0.24, ac.y - p.y);
    float pil = min(sdBox(p - vec2(ARCH_X - 1.05, (b5 + ac.y) * 0.5), vec2(0.24, (ac.y - b5) * 0.5)),
                    sdBox(p - vec2(ARCH_X + 1.05, (b5 + ac.y - 1.0) * 0.5), vec2(0.24, (ac.y - 1.0 - b5) * 0.5)));
    d = min(d, min(ring, pil));
    // broken columns under the lettering
    for (int i = 0; i < 3; i++) {
        float xl = -7.6 - 1.4 * float(i);
        float hb2 = bedY(xl) - 0.3;
        float hl2 = 0.8 + 0.6 * float(i == 2) + 0.3 * float(i == 0);
        d = min(d, sdBox(p - vec2(xl, hb2 + hl2 * 0.5), vec2(0.17, hl2 * 0.5)));
    }
    return d;
}

// What the echoes light at p, for a surface of normal n: the pings, the charted patches, a base level.
float echoAt(vec2 p, vec2 n, float base, out float pings, out float chart) {
    pings = 0.0;
    float lit = 0.0;
    for (int i = 0; i < 8; i++) {
        vec4 P = uPing[i];
        if (P.z < 0.0) continue;
        vec2 dv = P.xy - p;
        float dist = length(dv);
        float a = P.z - dist / 11.0;           // the echo returns from here a little later
        if (a <= 0.0) continue;
        float r = uPingR[i];
        float q = dist / r;
        float env = (1.0 - exp(-a / 0.14)) * exp(-a / uDecay) * exp(-q * q * q * q) * P.w;
        pings += env;
        float face = max(dot(n, dv / max(dist, 1e-3)), 0.0);
        lit += env * (0.3 + 0.7 * face);
    }
    chart = 0.0;
    for (int i = 0; i < 64; i++) {
        if (i >= uCharts) break;
        vec4 C = uChart[i];
        float dist = length(p - C.xy);
        chart = max(chart, C.w * (1.0 - smoothstep(C.z * 0.45, C.z, dist)));
    }
    lit += chart * 0.32 * (0.55 + 0.45 * max(n.y, 0.0));
    return lit + base;
}

// A surface returning the echo: a crisp line along its edge, a soft halo, the body fading into the dark below it.
float surfaceLight(float d, float aa, float grain) {
    float rim = exp(-abs(d) / max(0.03, aa));
    float halo = exp(-max(d, 0.0) / 0.35) * 0.16;
    float fill = d < 0.0 ? exp(d / 0.55) * 0.26 * grain : 0.0;
    return 1.25 * rim + halo + fill;
}

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float aa = length(fwidth(w));
    // the water: near black, a breath lighter far above, a slow murk
    float t = smoothstep(uView.y - uView.w, uView.y + uView.w, w.y);
    vec3 c = mix(uWaterLow, uWaterTop, t * t);
    c *= 0.88 + 0.24 * fbm(w * vec2(0.07, 0.11) + vec2(uTime * 0.006, uSeed));
    // the well and the lettering keep a quiet, darker ground
    float well = 1.0 - smoothstep(-0.2, 0.9, sdBox(w, vec2(5.2, 10.2)));
    float colL = 1.0 - smoothstep(-0.4, 1.2, sdBox(w - vec2(-8.6, 1.8), vec2(2.6, 8.6)));
    float colR = 1.0 - smoothstep(-0.4, 1.2, sdBox(w - vec2(8.6, 3.4), vec2(2.6, 7.0)));
    float corner = 1.0 - smoothstep(-0.4, 1.4, sdBox(w - vec2(-uView.z + 3.6, uView.y - uView.w + 2.6), vec2(3.4, 1.7)));
    float quiet = (1.0 - 0.86 * well) * (1.0 - 0.55 * max(max(colL, colR), corner));

    float grainF = 0.75 + 0.5 * vnoise(w * vec2(2.2, 7.0) + uSeed);   // the sand's ripples, as a sonar shows them
    // the city, behind
    float win;
    float dc = city(w, win);
    vec2 nc = normalize(vec2(dFdx(dc), dFdy(dc)) + 1e-6);
    float pc, cc;
    float ec = echoAt(w, nc, uCity, pc, cc);
    float lc = surfaceLight(dc, aa, 0.85 + 0.3 * vnoise(w * 3.0));
    vec3 cityCol = uEchoCity * 0.75 * lc * ec;
    // the lit windows (the kicks at a peak), and a faint glow of them in the dark
    cityCol += uEchoCity * win * (uWin * 1.4 + 0.25 * ec);
    // the front, over it
    float df = front(w);
    vec2 nf = normalize(vec2(dFdx(df), dFdy(df)) + 1e-6);
    float pf, cf;
    float ef = echoAt(w, nf, uBed, pf, cf);
    float lf = surfaceLight(df, aa, grainF);
    // charted seabed: survey lines along its strata, under the surface
    float strata = 0.0;
    if (df < 0.0 && cf > 0.001) {
        float s = -df / 0.42;
        float line = exp(-abs(fract(s) - 0.5) * 2.0 / max(0.08, aa * 2.5 / 0.42));
        strata = line * exp(df / 2.2) * cf * 0.12;
    }
    vec3 frontCol = uEcho * (lf * ef + strata);
    float occl = smoothstep(0.03, -0.03, df);
    vec3 scene = cityCol * (1.0 - occl) + frontCol;
    // particles in the water, seen only where an echo passes
    float pw = pf;
    if (pw > 0.01 && df > 0.0) {
        vec2 g = w * 0.85 + vec2(0.0, uTime * 0.03);
        vec2 id = floor(g), f = fract(g);
        float h = hash12(id + uSeed * 7.0);
        vec2 o = vec2(hash12(id + 3.1), hash12(id + 9.7)) * 0.8 + 0.1;
        float r = length((f - o) / 0.85);
        float m = exp(-r * r / 0.0015) * step(0.72, h) * (0.4 + h);
        scene += uEcho * m * pw * 0.45;
    }
    c += scene * quiet;
    c = mix(c, c * vec3(0.7, 0.8, 1.15), uSlow * 0.5);
    frag = vec4(c * uFade, 0.0);
}
)";

// ---------------------------------------------------------------- the blocks (instanced per cell)
// a: x, y (cell center), half (board units), kind (0 block, 1 ghost, 2 sinking, 3 game over).
// b: color (linear), alpha. c: flash, progress, glow, -.
inline const char* PIECE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA, vB, vC;
void main() {
    float k = aA.w;
    float t = aC.y;
    vec2 l = aPos * 0.5;
    vLocal = l;
    vec2 wp = aA.xy + l * 2.0 * aA.z;
    // sinking into the dark: down, a little smaller
    if (k > 1.5) {
        float fall = k > 2.5 ? 2.6 : 0.75;
        wp = aA.xy + l * 2.0 * aA.z * (1.0 - 0.18 * t) - vec2(0.0, fall * t * t);
    }
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* PIECE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC;
out vec4 frag;
uniform float uGlow;
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    float aa = max(length(fwidth(p)) * 0.8, 1e-4);
    float d = sdBox(p, vec2(0.415)) - 0.05;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    vec3 col = vB.rgb;
    float inside = max(-d, 0.0);
    if (kind == 1) {
        // the ghost: the block's edge only, faint
        float l = 1.0 - smoothstep(0.0, aa * 1.5, abs(d + 0.03) - 0.014);
        float a = l * 0.5 * vB.a;
        if (a <= 0.002) discard;
        frag = vec4(col * 0.7 * a, a);
        return;
    }
    if (cov <= 0.002) discard;
    // a solid block lit from within: a deep face, brighter toward its top, a crisp luminous edge
    float edge = exp(-inside / 0.03);
    float inner = exp(-inside / 0.16);
    float up = clamp(p.y + 0.5, 0.0, 1.0);
    vec3 face = col * (0.36 + 0.2 * up + 0.05 * (0.5 - abs(p.x)));
    vec3 c = face + col * (0.95 * edge + 0.22 * inner);
    // the top face catches the light a little: the block reads as a cube
    c += col * 0.18 * smoothstep(0.24, 0.40, p.y) * (1.0 - edge);
    float fl = vC.x;
    c *= 1.0 + 0.5 * fl;
    c *= vC.z;
    float a = cov * vB.a;
    if (kind >= 2) {
        // into the dark: the light goes out as it sinks
        float t = clamp(vC.y, 0.0, 1.0);
        float k = (1.0 - t) * (1.0 - t);
        c *= k;
        a *= 1.0 - t * t;
    }
    if (a <= 0.002) discard;
    frag = vec4(c * uGlow * a, a);
}
)";

// ---------------------------------------------------------------- soft points of light
// a: x, y, radius, alpha. b: color (linear), stretch (x).
inline const char* MOTE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB;
void main() {
    vL = aPos * 3.0;
    vA = aA; vB = aB;
    vec2 wp = aA.xy + aPos * 3.0 * aA.z * vec2(aB.w, 1.0);
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* MOTE_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB;
out vec4 frag;
void main() {
    float r2 = dot(vL, vL);
    float v = exp(-r2 * 1.6) + 0.18 * exp(-r2 * 0.25);
    float a = v * vA.w;
    if (a <= 0.002) discard;
    frag = vec4(vB.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well (full screen)
inline const char* WELL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uLine;
uniform float uAA, uLineA;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // two faint posts fading upward and a floor line
    float post = abs(abs(w.x) - 5.2) - 0.01;
    float pa = (1.0 - smoothstep(-uAA, uAA, post)) * step(-10.2, w.y) * smoothstep(10.2, -7.0, w.y);
    float floorL = abs(w.y + 10.2) - 0.01;
    float fa = (1.0 - smoothstep(-uAA, uAA, floorL)) * step(abs(w.x), 5.21);
    float l = max(pa, fa) * uLineA;
    if (l <= 0.001) discard;
    frag = vec4(uLine * l, 0.0);
}
)";

// ---------------------------------------------------------------- the glow
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb +
             texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    frag = vec4(max(c - vec3(0.04), 0.0), 1.0);
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
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: silent running, the colors drawn toward one cold light
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g) * uSlowTint * 1.25, c, 0.4), uSlow * (1.0 - 0.75 * clamp(s.a, 0.0, 1.0)));
    // a soft knee: the glow never burns to white
    c = c / (1.0 + 0.25 * max(max(c.r, c.g), c.b));
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.3 * dot(v, v) * 2.0;
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
// Pale letters with a soft halo of the dark water around them.
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

// Builds a fragment shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace sonar_shaders

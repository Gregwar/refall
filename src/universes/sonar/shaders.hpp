#pragma once
// SONAR's shaders (see sonar.cpp).
//
// WATER_FS paints the deep water (near black, a breath lighter far above); MESH_FS the sunken world in 3D (the
// seabed, rocks, the wreck, the city), every surface dark until an echo returns from it: the pings (local, on the
// beat grid) light the faces turned toward them, the polygons' edges as fine lines of light; the charted spots
// (one per cleared row) stay faintly lit for good, the seabed showing its depth contours there; SNOW_VS / SNOW_FS
// the particles drifting around the camera. PIECE_FS draws the blocks (solid cubes lit from within, instanced per
// cell, crisp: the glow is the world's only), MOTE_FS the soft points of light (a cleared row's light sinking to
// the seabed, a hard drop's contact), DOWN_FS / BLUR_FS the world's glow and FINAL_FS the frame.
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

// ---------------------------------------------------------------- the sunken world (3D)
// Shared by the water, the world's mesh and the particles: the pings, the quiet ground behind the well and the
// HUD, the water's own faint light.
inline const char* WORLD_COMMON = R"(
uniform vec4 uPing[8];          // x, y, z (world), age (s; < 0: none)
uniform vec2 uPingS[8];         // strength, reach
uniform float uDecay;
uniform vec4 uView;             // the board's view: center, half extents (board units)
uniform vec2 uRes;
uniform vec3 uWaterTop, uWaterLow;
uniform vec3 uCamF, uCamR, uCamU; // the world camera: forward, right and up scaled to the view's half extents
uniform float uTime, uSeed;

// An echo returning from p (it leaves the ping's point and reaches farther surfaces a little later).
float pingEnv(int i, vec3 p, out vec3 dir) {
    vec4 P = uPing[i];
    dir = vec3(0.0, 1.0, 0.0);
    if (P.w < 0.0) return 0.0;
    vec3 dv = P.xyz - p;
    float dist = length(dv);
    float a = P.w - dist / 14.0;
    if (a <= 0.0) return 0.0;
    float q = dist / uPingS[i].y;
    dir = dv / max(dist, 1e-3);
    return (1.0 - exp(-a / 0.14)) * exp(-a / uDecay) * exp(-q * q * q * q) * uPingS[i].x;
}
// The well, the lettering's columns and the bottom-left corner keep a quiet, darker ground.
float quietAt(vec2 frag) {
    vec2 w = uView.xy + (frag / uRes * 2.0 - 1.0) * uView.zw;
    float well = 1.0 - smoothstep(-0.2, 0.9, sdBox(w, vec2(5.2, 10.2)));
    float colL = 1.0 - smoothstep(-0.4, 1.2, sdBox(w - vec2(-8.6, 1.8), vec2(2.6, 8.6)));
    float colR = 1.0 - smoothstep(-0.4, 1.2, sdBox(w - vec2(8.6, 3.4), vec2(2.6, 7.0)));
    float corner = 1.0 - smoothstep(-0.4, 1.4, sdBox(w - vec2(-uView.z + 3.6, uView.y - uView.w + 2.6), vec2(3.4, 1.7)));
    return (1.0 - 0.86 * well) * (1.0 - 0.55 * max(max(colL, colR), corner));
}
// Deep water: near black, a breath lighter far above, a slow murk.
vec3 waterAt(vec2 frag) {
    vec2 ndc = frag / uRes * 2.0 - 1.0;
    vec3 d = normalize(uCamF + ndc.x * uCamR + ndc.y * uCamU);
    float t = smoothstep(-0.3, 0.75, d.y);
    vec3 c = mix(uWaterLow, uWaterTop, t * t);
    vec2 m = d.xz / (d.y + 1.6) * 3.0;
    return c * (0.88 + 0.24 * fbm(m + vec2(uTime * 0.01, uSeed)));
}
)";

inline const char* WATER_FS = R"(#version 330 core
out vec4 frag;
uniform float uSlow, uFade;
void main() {
    vec3 c = waterAt(gl_FragCoord.xy);
    c = mix(c, c * vec3(0.7, 0.8, 1.15), uSlow * 0.5);
    frag = vec4(c * uFade, 0.0);
}
)";

inline const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 aP;
layout(location = 1) in vec3 aN;
layout(location = 2) in vec4 aE;
layout(location = 3) in vec4 aI;
uniform mat4 uWVP;
out vec3 vP, vN;
out vec4 vE, vI;
void main() {
    vP = aP; vN = aN; vE = aE; vI = aI;
    gl_Position = uWVP * vec4(aP, 1.0);
}
)";
// Every surface is dark until an echo returns from it: the faces turned toward the ping light up softly, the
// polygons' edges as fine lines of light (the hull's ribs, the courses of the stones), the seabed shows its sand
// ripples and, where charted, its depth contours. The charted spots stay faintly lit for good.
inline const char* MESH_FS = R"(#version 330 core
in vec3 vP, vN;
in vec4 vE, vI;
out vec4 frag;
uniform vec3 uCam;
uniform vec4 uChart[64];        // x, y, z, radius
uniform float uChartA[64];      // amount
uniform int uCharts;
uniform vec3 uEcho, uEchoCity;
uniform float uBed, uCity, uWin, uSlow, uFade;
void main() {
    vec3 V = uCam - vP;
    float dist = length(V);
    V /= dist;
    vec3 n = normalize(vN);
    if (dot(n, V) < 0.0) n = -n;
    int layer = int(vI.x + 0.5), flags = int(vI.y + 0.5);
    float lit = 0.0, pings = 0.0;
    for (int i = 0; i < 8; i++) {
        vec3 dir;
        float e = pingEnv(i, vP, dir);
        if (e <= 0.0) continue;
        pings += e;
        lit += e * (0.25 + 0.75 * max(dot(n, dir), 0.0));
    }
    float chart = 0.0;
    for (int i = 0; i < 64; i++) {
        if (i >= uCharts) break;
        vec4 C = uChart[i];
        chart = max(chart, uChartA[i] * (1.0 - smoothstep(C.w * 0.4, C.w, length(vP - C.xyz))));
    }
    float chartL = chart * 0.3 * (0.55 + 0.45 * max(n.y, 0.0));
    // the drop, a peak, a quad: the whole world surfacing, lit from above and toward the eye
    float base = layer == 3 ? uCity : uBed;
    float baseL = base * (0.3 + 0.5 * max(dot(n, normalize(vec3(0.35, 1.0, 0.25))), 0.0) + 0.2 * max(dot(n, V), 0.0));

    // the outline: a fine line of light along the polygon's edges
    float me = min(min(vE.x, vE.y), min(vE.z, vE.w));
    float fw = max(fwidth(me), 1e-4);
    float far = 1.0 - smoothstep(22.0, 70.0, dist);
    float line = (1.0 - smoothstep(0.35 * fw, 1.35 * fw, me)) * far;
    float soft = exp(-me / 0.12) * 0.22 * far;
    float grain = layer == 0 ? 0.7 + 0.6 * vnoise(vec2(vP.x * 0.6 + vP.z * 0.9, vP.z * 0.6 - vP.x * 0.3) + uSeed)
                             : 0.8 + 0.4 * vnoise(vP.xz * 1.7 + vP.y * 2.3);
    float fill = (layer == 0 ? 0.2 : 0.16) * grain;
    // the seabed's depth contours, where it is charted (and faintly where an echo passes)
    float cont = 0.0;
    if ((flags & 2) != 0) {
        float s = vP.y / 0.45;
        float fs = max(fwidth(s), 1e-4);
        float dd = abs(fract(s + 0.5) - 0.5);
        cont = (1.0 - smoothstep(0.4 * fs, 1.4 * fs, dd)) * (1.0 - smoothstep(18.0, 50.0, dist)) * (1.0 - smoothstep(0.25, 0.5, fs));
    }
    // the windows: dark openings, a few lit by the kicks at a peak
    float glowW = 0.0;
    if ((flags & 1) != 0) {
        vec2 g = vI.zw / vec2(1.15, 1.9);
        vec2 id = floor(g), f = fract(g) - 0.5;
        float fg = max(fwidth(g.x), fwidth(g.y));
        float wd = max(abs(f.x) - 0.17, abs(f.y) - 0.21);
        float wm = (1.0 - smoothstep(-fg, fg, wd)) * step(1.0, id.y) * step(1.0, me);
        fill *= 1.0 - 0.85 * wm;
        glowW = wm * step(0.55, hash12(id + vec2(17.0, 3.0))) * uWin * far;
    }
    vec3 ec = layer == 3 ? uEchoCity : uEcho;
    // the world surfacing (drop, peak, quad) shows mostly its lines: faint faces, crisp edges and contours
    vec3 col = ec * (lit * (fill + 1.25 * line + soft) + (baseL + chartL) * (0.28 * fill + 1.1 * line + soft) + cont * (0.35 * chart + 0.5 * base));
    col += uEchoCity * glowW * 0.9;
    float fog = exp(-dist / 48.0);
    col *= fog * smoothstep(0.4, 2.0, dist);
    // near surfaces hide the water's faint light: the world stands out as a darker shape when unlit
    vec3 c = waterAt(gl_FragCoord.xy) * (1.0 - 0.6 * fog) + col * quietAt(gl_FragCoord.xy);
    c = mix(c, c * vec3(0.7, 0.8, 1.15), uSlow * 0.5);
    frag = vec4(c * uFade, 0.0);
}
)";

// Particles drifting in the water around the camera: barely there, glinting where an echo passes. They show the
// glide through the water.
inline const char* SNOW_VS = R"(#version 330 core
layout(location = 0) in vec4 aP;
uniform mat4 uWVP;
uniform vec3 uCam;
uniform float uBase, uFocal;
out float vA;
void main() {
    const float B = 36.0;
    vec3 p0 = aP.xyz * B + vec3(0.3 * sin(uTime * 0.07 + aP.w * 20.0), -uTime * 0.05, 0.3 * cos(uTime * 0.05 + aP.w * 13.0));
    vec3 rel = mod(p0 - uCam + 0.5 * B, B) - 0.5 * B;
    vec3 p = uCam + rel;
    float d = length(rel);
    float lit = 0.0;
    for (int i = 0; i < 8; i++) { vec3 dir; lit += pingEnv(i, p, dir); }
    float box = max(max(abs(rel.x), abs(rel.y)), abs(rel.z));
    vA = (uBase * (0.35 + 0.65 * aP.w) + lit * 0.8) * (1.0 - smoothstep(0.35 * B, 0.5 * B, box)) * smoothstep(0.6, 2.5, d) * exp(-d / 22.0);
    gl_Position = uWVP * vec4(p, 1.0);
    gl_PointSize = clamp(0.035 * uFocal / max(d, 0.5), 1.5, 5.0);
}
)";
inline const char* SNOW_FS = R"(#version 330 core
in float vA;
out vec4 frag;
uniform vec3 uEcho;
uniform float uFade;
void main() {
    vec2 q = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(q, q);
    float a = exp(-r2 * 3.0) * vA * quietAt(gl_FragCoord.xy) * uFade;
    if (a <= 0.001) discard;
    frag = vec4(uEcho * a, 0.0);
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

// Builds a shader with the common functions (and the world's, if asked) inserted after the version line.
inline std::string withCommon(const char* src, bool world = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (world ? WORLD_COMMON : "") + s.substr(nl + 1);
}

} // namespace sonar_shaders

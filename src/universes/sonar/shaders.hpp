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
    return (1.0 - 0.86 * well) * (1.0 - 0.68 * max(max(colL, colR), corner));
}
// The water: a teal murk, lighter far above, soft shafts of light slanting down from the surface.
vec3 waterAt(vec2 frag) {
    vec2 ndc = frag / uRes * 2.0 - 1.0;
    vec3 d = normalize(uCamF + ndc.x * uCamR + ndc.y * uCamU);
    float t = smoothstep(-0.35, 0.8, d.y);
    vec3 c = mix(uWaterLow, uWaterTop, t * sqrt(t));
    vec2 m = d.xz / (d.y + 1.6) * 3.0;
    c *= 0.86 + 0.28 * vnoise(m * 1.7 + vec2(uTime * 0.012, uSeed));
    // the shafts: bands across the view, fading downward and toward the sides
    float sx = d.x / (d.y + 1.35) + 0.25 * d.z;
    float sh = vnoise(vec2(sx * 7.0 + uTime * 0.03, uSeed)) * vnoise(vec2(sx * 2.3 - uTime * 0.017, uSeed + 4.0));
    c += uWaterTop * (0.75 * sh * smoothstep(-0.25, 0.55, d.y) * (1.0 - 0.6 * abs(ndc.x)));
    return c;
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
uniform vec3 uEcho, uEchoCity, uStone;
uniform float uBed, uCity, uWin, uSlow, uFade, uAmb, uPaths;
void main() {
    vec3 V = uCam - vP;
    float dist = length(V);
    V /= dist;
    vec3 n = normalize(vN);
    if (dot(n, V) < 0.0) n = -n;
    int layer = int(vI.x + 0.5), flags = int(vI.y + 0.5);
    float pw = max(length(fwidth(vP)), 1e-4);  // world units per pixel here
    float rag = vnoise(vP.xz * 0.42 + vec2(uSeed, 2.0)); // the echo patches' ragged outlines
    float far = 1.0 - smoothstep(22.0, 70.0, dist);

    // the pings: a patch of light around each point, sharp-edged with a bright rim, the faces turned to it brighter
    float lit = 0.0, rim = 0.0;
    for (int i = 0; i < 8; i++) {
        vec4 P = uPing[i];
        if (P.w < 0.0) continue;
        vec3 dv = P.xyz - vP;
        float d = length(dv);
        float a = P.w - d / 14.0;
        if (a <= 0.0) continue;
        float env = (1.0 - exp(-a / 0.14)) * exp(-a / uDecay) * uPingS[i].x;
        float R = uPingS[i].y * (0.78 + 0.5 * rag);
        float f = 1.0 - d / R;
        if (f < -0.4) continue;
        float fw = 1.5 * pw / R;
        float inside = smoothstep(-fw, fw, f);
        vec3 dir = dv / max(d, 1e-3);
        lit += env * inside * (0.3 + 0.7 * max(dot(n, dir), 0.0)) * (0.45 + 0.55 * f);
        rim += env * ((1.0 - smoothstep(0.6 * fw, 2.2 * fw, abs(f))) + 0.35 * exp(-abs(f) * R / 0.35));
    }
    // the charted spots: lit for good, ragged-edged too
    float chart = 0.0, chartRim = 0.0;
    for (int i = 0; i < 64; i++) {
        if (i >= uCharts) break;
        vec4 C = uChart[i];
        float R = C.w * (0.75 + 0.5 * rag);
        float f = 1.0 - length(vP - C.xyz) / R;
        float fw = 1.5 * pw / R;
        chart = max(chart, uChartA[i] * smoothstep(-fw, fw, f) * (0.5 + 0.5 * smoothstep(0.0, 0.5, f)));
        chartRim = max(chartRim, uChartA[i] * (1.0 - smoothstep(0.6 * fw, 2.2 * fw, abs(f))));
    }
    // the drop, a peak, a quad: the whole world surfacing. On the seabed, as winding paths of light (sharp-edged,
    // a bright rim); on everything else, the faces and edges.
    float base = layer == 3 ? uCity : uBed;
    float pf = fbm(vP.xz * 0.075 + vec2(uSeed * 0.3, 7.0)) - (0.62 - 0.04 * uPaths);
    float pfw = max(fwidth(pf), 1e-5);
    if (layer == 0) {
        float fw = pfw;
        float inP = smoothstep(-fw, fw, pf);
        float rP = 1.0 - smoothstep(0.6 * fw, 2.2 * fw, abs(pf));
        float nearP = 1.0 - smoothstep(14.0, 40.0, dist);
        rim += base * 1.6 * rP * nearP;
        base *= (0.04 + 0.96 * inP) * nearP;
    }
    float baseL = base * (0.35 + 0.45 * max(dot(n, normalize(vec3(0.35, 1.0, 0.25))), 0.0) + 0.2 * max(dot(n, V), 0.0));
    float chartL = chart * 0.32 * (0.55 + 0.45 * max(n.y, 0.0));
    rim += chartRim * 0.5;

    // the echo's texture: dense rows of dots, across the seabed (gently winding) and along the walls (level rows)
    vec2 uv;
    if (abs(n.y) > 0.6) uv = vec2(vP.x, vP.z + 0.7 * sin(vP.x * 0.21 + 0.4 * sin(vP.z * 0.13)));
    else { vec2 tg = normalize(vec2(-n.z, n.x) + 1e-5); uv = vec2(dot(vP.xz, tg), vP.y); }
    vec2 g = uv / vec2(0.075, 0.12);
    vec2 fg = fwidth(g);
    vec2 cp = abs(fract(g) - 0.5);
    float rowL = 1.0 - smoothstep(0.06, 0.06 + 0.14 + fg.y, cp.y);
    float dotL = 1.0 - smoothstep(0.1, 0.1 + 0.16 + fg.x, cp.x);
    float pat = rowL * mix(1.0, dotL, 0.8);
    pat = mix(pat, 0.14, smoothstep(0.3, 0.75, max(fg.x, fg.y)));

    // the outline: a fine line of light along the polygon's edges
    float me = min(min(vE.x, vE.y), min(vE.z, vE.w));
    float fw = max(fwidth(me), 1e-4);
    float line = (1.0 - smoothstep(0.35 * fw, 1.35 * fw, me)) * far;
    float soft = exp(-me / 0.12) * 0.22 * far;
    // the seabed's depth contours, where it is charted
    float cont = 0.0;
    if ((flags & 2) != 0) {
        float s = vP.y / 0.45;
        float fs = max(fwidth(s), 1e-4);
        float dd = abs(fract(s + 0.5) - 0.5);
        cont = (1.0 - smoothstep(0.4 * fs, 1.4 * fs, dd)) * (1.0 - smoothstep(18.0, 50.0, dist)) * (1.0 - smoothstep(0.25, 0.5, fs));
    }
    // the stone, in the water's own light: dark weathered blocks lit from above, melting into the murk with distance
    vec2 suv = abs(n.y) > 0.6 ? vP.xz : uv;
    float tex = 0.55 + 0.6 * vnoise(suv * 0.7 + 3.0) * (0.6 + 0.8 * vnoise(suv * 3.1 + 11.0));
    if (layer == 0) tex = 0.7 + 0.5 * vnoise(vec2(vP.x * 0.5 + vP.z * 0.8, vP.z * 0.5 - vP.x * 0.3) * 1.3 + uSeed);
    float sky = 0.16 + 0.7 * max(n.y, 0.0) + 0.3 * max(dot(n, normalize(vec3(0.55, 0.35, -0.45))), 0.0);
    float amb = (layer == 0 ? 0.75 : 1.0) * sky * tex;
    // the masonry: courses of dressed stone on the temples and towers, their joints a little darker
    if (layer == 3 && abs(n.y) < 0.6) {
        vec2 mq = vec2(uv.x / 1.1 + 0.5 * floor(uv.y / 0.55), uv.y / 0.55);
        vec2 mf = abs(fract(mq) - 0.5);
        vec2 mw = fwidth(mq);
        float joint = max(1.0 - smoothstep(0.0, mw.x * 1.5 + 0.02, 0.5 - mf.x), 1.0 - smoothstep(0.0, mw.y * 1.5 + 0.03, 0.5 - mf.y));
        joint *= 1.0 - smoothstep(0.3, 0.6, max(mw.x, mw.y));
        amb *= (1.0 - 0.45 * joint) * (0.85 + 0.3 * hash12(floor(mq) + 5.0));
    }
    // the windows: dark openings, a few lit by the kicks at a peak
    float glowW = 0.0;
    if ((flags & 1) != 0) {
        vec2 gw = vI.zw / vec2(1.15, 1.9);
        vec2 id = floor(gw), f = fract(gw) - 0.5;
        float fgw = max(fwidth(gw.x), fwidth(gw.y));
        float wd = max(abs(f.x) - 0.17, abs(f.y) - 0.21);
        float wm = (1.0 - smoothstep(-fgw, fgw, wd)) * step(1.0, id.y) * step(1.0, me);
        amb *= 1.0 - 0.8 * wm;
        pat *= 1.0 - 0.9 * wm;
        glowW = wm * step(0.55, hash12(id + vec2(17.0, 3.0))) * uWin * far;
    }
    vec3 ec = layer == 3 ? uEchoCity : uEcho;
    float E = lit + (layer == 0 ? 1.0 : 0.35) * baseL + chartL;
    vec3 col = ec * (E * (0.03 + (layer == 0 ? 1.45 : 1.1) * pat) + (lit + 0.9 * baseL + chartL) * (1.25 * line + soft) + rim * (layer == 0 ? 1.25 : 0.9)
                     + cont * (0.35 * chart + 0.3 * base));
    col += uEchoCity * glowW * 0.9;
    col *= exp(-dist / 46.0) * smoothstep(0.4, 2.0, dist);
    float fog = exp(-dist / 30.0);
    vec3 w = waterAt(gl_FragCoord.xy);
    vec3 c = mix(w, uStone * amb * uAmb, fog * smoothstep(0.3, 1.5, dist)) + col * quietAt(gl_FragCoord.xy);
    c = mix(c, c * vec3(0.7, 0.8, 1.15), uSlow * 0.5);
    frag = vec4(c * uFade, 0.0);
}
)";

// Particles drifting in the water around the camera, pale motes in the water's light, glinting where an echo
// passes (they show the glide through the water); with uBubbles, the same points as bubbles rising in thin streams.
inline const char* SNOW_VS = R"(#version 330 core
layout(location = 0) in vec4 aP;
uniform mat4 uWVP;
uniform vec3 uCam;
uniform float uBase, uFocal, uBubbles;
out float vA, vLit;
out float vBub;
void main() {
    const float B = 36.0;
    vec3 p0;
    float size;
    if (uBubbles > 0.5) {
        // streams: each bubble belongs to one of a few sources on the seabed lattice, rising and wobbling
        float sid = floor(aP.w * 23.0);
        vec3 src = vec3(hash12(vec2(sid, 1.7)), 0.0, hash12(vec2(sid, 9.3))) * B;
        float rise = fract(aP.y + uTime * (0.025 + 0.02 * aP.x));
        p0 = src + vec3(0.25 * sin(uTime * 2.1 + aP.z * 40.0) * rise + (aP.x - 0.5) * 0.6, rise * B, 0.25 * cos(uTime * 1.7 + aP.z * 31.0) * rise + (aP.z - 0.5) * 0.6);
        size = 0.04 + 0.1 * aP.z * aP.z;
    } else {
        p0 = aP.xyz * B + vec3(0.3 * sin(uTime * 0.07 + aP.w * 20.0), -uTime * 0.05, 0.3 * cos(uTime * 0.05 + aP.w * 13.0));
        size = 0.035;
    }
    vec3 rel = mod(p0 - uCam + 0.5 * B, B) - 0.5 * B;
    vec3 p = uCam + rel;
    float d = length(rel);
    float lit = 0.0;
    for (int i = 0; i < 8; i++) { vec3 dir; lit += pingEnv(i, p, dir); }
    float box = max(max(abs(rel.x), abs(rel.y)), abs(rel.z));
    float shape = (1.0 - smoothstep(0.35 * B, 0.5 * B, box)) * smoothstep(0.6, 2.5, d) * exp(-d / 22.0);
    vA = uBase * (0.35 + 0.65 * aP.w) * shape;
    vLit = lit * 0.8 * shape;
    vBub = uBubbles;
    gl_Position = uWVP * vec4(p, 1.0);
    gl_PointSize = clamp(size * uFocal / max(d, 0.5), 1.5, uBubbles > 0.5 ? 22.0 : 5.0);
}
)";
inline const char* SNOW_FS = R"(#version 330 core
in float vA, vLit, vBub;
out vec4 frag;
uniform vec3 uEcho, uMote;
uniform float uFade;
void main() {
    vec2 q = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(q, q);
    float qa = quietAt(gl_FragCoord.xy) * uFade;
    if (vBub > 0.5) {
        // a bubble: a fine bright ring, a glint at its top
        float r = sqrt(r2);
        float ring = exp(-pow((r - 0.72) / 0.13, 2.0)) + 0.8 * exp(-dot(q - vec2(-0.3, 0.32), q - vec2(-0.3, 0.32)) * 30.0);
        ring *= step(r, 1.0);
        vec3 c = (uMote * vA + uEcho * vLit) * ring * (0.45 + 0.55 * qa);
        if (max(c.r, max(c.g, c.b)) <= 0.0005) discard;
        frag = vec4(c * uFade, 0.0);
        return;
    }
    float s = exp(-r2 * 3.0);
    vec3 c = (uMote * vA + uEcho * vLit) * s * qa;
    if (max(c.r, max(c.g, c.b)) <= 0.0005) discard;
    frag = vec4(c, 0.0);
}
)";

// Fish: small schools swimming past in the water, billboards turned along their heading, a slim pale body that
// catches the water's light from above and glints where an echo passes; the tail beats.
// a: position (world), size. b: heading (world), phase.
inline const char* FISH_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uWVP;
uniform vec3 uFR, uFU, uCam;
out vec2 vL;
out float vLit, vFog, vPh;
void main() {
    vec3 h = aB.xyz;
    vec2 h2 = vec2(dot(h, uFR), dot(h, uFU));
    float hl = length(h2);
    h2 = hl > 1e-4 ? h2 / hl : vec2(1.0, 0.0);
    float fore = clamp(hl / max(length(h), 1e-4), 0.35, 1.0);
    vec3 ax = uFR * h2.x + uFU * h2.y, ay = uFU * h2.x - uFR * h2.y;
    vL = aPos;
    vec3 p = aA.xyz + ax * aPos.x * aA.w * fore + ay * aPos.y * aA.w * 0.5;
    float lit = 0.0;
    for (int i = 0; i < 8; i++) { vec3 dir; lit += pingEnv(i, aA.xyz, dir); }
    float d = length(aA.xyz - uCam);
    vLit = lit;
    vFog = exp(-d / 26.0) * smoothstep(1.0, 3.0, d);
    vPh = aB.w;
    gl_Position = uWVP * vec4(p, 1.0);
}
)";
inline const char* FISH_FS = R"(#version 330 core
in vec2 vL;
in float vLit, vFog, vPh;
out vec4 frag;
uniform vec3 uFishCol, uEcho;
uniform float uFade;
void main() {
    // x along the body (+1 the head), y across; the tail beats
    vec2 p = vL;
    p.y -= 0.12 * sin(vPh) * smoothstep(0.2, -1.0, p.x);
    float body = length(vec2((p.x - 0.12) / 0.62, p.y / 0.42)) - 1.0;
    vec2 tp = p - vec2(-0.72, 0.0);
    float tail = max(abs(tp.y) - (0.08 + 0.55 * max(-tp.x, 0.0)), max(tp.x - 0.05, -tp.x - 0.28));
    float d = min(body, tail * 3.0);
    float aa = fwidth(d) * 1.2 + 1e-4;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.01) discard;
    float back = smoothstep(-0.3, 0.4, p.y);
    vec3 c = uFishCol * (0.45 + 0.75 * back) + uEcho * vLit * 1.2;
    c *= (1.0 + 0.6 * (1.0 - smoothstep(0.0, 0.15, abs(d + 0.08))));
    float a = cov * vFog * uFade * (0.25 + 0.75 * quietAt(gl_FragCoord.xy));
    frag = vec4(c * a, 0.0);
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
uniform float uGlow, uTime;
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    float aa = max(length(fwidth(p)) * 0.8, 1e-4);
    float d = sdBox(p, vec2(0.415)) - 0.05;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    vec3 col = vB.rgb;
    float inside = max(-d, 0.0);
    if (kind == 1) {
        // the ghost: the block's glass edge, faint, a breath of its color inside
        float l = 1.0 - smoothstep(0.0, aa * 1.5, abs(d + 0.03) - 0.016);
        float a = (l * 0.6 + 0.05 * cov) * vB.a;
        if (a <= 0.002) discard;
        frag = vec4(col * 0.75 * a, a);
        return;
    }
    if (cov <= 0.002) discard;
    // a cube of colored glass lit from within: a deep, clear body, brighter toward its top, a bright rim, the glass's
    // thickness as a second, fainter line inside, a sheen across its upper corner, a few bubbles rising inside
    float edge = exp(-inside / 0.028);
    float inner = exp(-inside / 0.14);
    float up = clamp(p.y + 0.5, 0.0, 1.0);
    vec3 deep = mix(col, col * col * 1.5, 0.45);   // the glass's body, a little deeper in hue than its rim
    vec3 face = deep * (0.42 + 0.24 * up + 0.05 * (0.5 - abs(p.x)));
    float bevel = exp(-pow((inside - 0.085) / 0.012, 2.0));
    vec3 c = face + col * (1.15 * edge + 0.18 * inner + 0.22 * bevel);
    c += col * 0.16 * smoothstep(0.24, 0.40, p.y) * (1.0 - edge);
    float sheen = smoothstep(0.12, 0.0, abs(p.x + p.y - 0.42)) * smoothstep(-0.1, 0.3, p.y - p.x * 0.2) * (1.0 - edge);
    c += mix(col, vec3(1.0), 0.6) * 0.12 * sheen;
    // bubbles: three tiny rings per cell, rising slowly and looping
    float seed = vC.w;
    float bub = 0.0;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float hx = fract(sin(seed * 12.9898 + fi * 78.233) * 43758.5453);
        float hy = fract(sin(seed * 39.3468 + fi * 11.135) * 24634.6345);
        float y = fract(hy + uTime * (0.035 + 0.02 * hx)) ;
        vec2 bc = vec2((hx - 0.5) * 0.56, -0.32 + y * 0.6);
        float r = 0.022 + 0.022 * fract(hx * 7.0 + hy);
        float dd = length(p - bc);
        float ring = 1.0 - smoothstep(0.0, aa * 1.2 + 0.006, abs(dd - r) - 0.005);
        bub += ring * smoothstep(0.0, 0.15, y) * smoothstep(1.0, 0.8, y);
    }
    c += mix(col, vec3(1.0), 0.55) * 0.42 * min(bub, 1.0) * (1.0 - edge);
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
uniform vec3 uLine, uPane;
uniform float uAA, uLineA, uPaneA;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // a pane of dark glass behind the stack, a fine frame around it
    float bd = sdBox(w - vec2(0.0, 0.25), vec2(5.2, 10.45));
    float pane = (1.0 - smoothstep(-uAA, uAA, bd)) * uPaneA * (0.92 + 0.08 * smoothstep(-10.0, 10.0, w.y));
    float frame = (1.0 - smoothstep(0.0, uAA * 1.2, abs(bd) - 0.006)) * uLineA * (0.55 + 0.45 * smoothstep(10.0, -10.0, w.y));
    float a = max(pane, 0.0);
    vec3 c = uPane * a + uLine * frame;
    if (a + frame <= 0.001) discard;
    frag = vec4(c, a);
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
uniform vec4 uView;
uniform float uPaneA;
void main() {
    vec4 s = texture(uScene, vUv);
    // (the world's glow stays behind the well's dark pane)
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 bd = abs(w - vec2(0.0, 0.25)) - vec2(5.2, 10.45);
    float pane = 1.0 - smoothstep(-0.2, 0.2, max(bd.x, bd.y));
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK * (1.0 - uPaneA * pane);
    // slow motion: silent running, the colors drawn toward one cold light
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g) * uSlowTint * 1.25, c, 0.4), uSlow * (1.0 - 0.75 * clamp(s.a, 0.0, 1.0)));
    // a soft knee: the glow never burns to white
    c = c / (1.0 + 0.25 * max(max(c.r, c.g), c.b));
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.45 * dot(v, v) * 2.0;
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

// Builds a shader with the common functions (and the world's, if asked) inserted after the version line.
inline std::string withCommon(const char* src, bool world = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (world ? WORLD_COMMON : "") + s.substr(nl + 1);
}

} // namespace sonar_shaders

#pragma once
// LIMBO's shaders (see limbo.cpp).
//
// SKY_FS paints the fog itself (its glow around the light, the far landscape the drop reveals, the light shafts);
// SIL_FS draws the world's silhouettes (instanced primitives: tapered branches, discs, boxes, gears, triangles),
// each faded toward the fog's own color at its pixel by its depth; BANK_FS lays a drifting fog bank between two
// depths; PIECE_FS draws the pieces (one black silhouette per piece, rim-lit by the fog's glow) and the ghost;
// MOTE_FS the drifting motes of a cleared row; FINAL_FS grades and encodes the frame (vignette, a fine grain).
#include <string>

namespace limbo_shaders {

// ---------------------------------------------------------------- common: the fog's color, noise
inline const char* COMMON = R"(
uniform vec4 uView;     // center, half extents (board units)
uniform vec3 uFogLo;    // the fog near the ground (linear)
uniform vec3 uFogHi;    // the fog high up
uniform vec3 uGlowCol;  // the light behind the fog
uniform vec4 uSun;      // x, y, glow radius, glow strength
uniform float uSeed;
uniform float uTime;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 7.1; a *= 0.5; }
    return s;
}
// How much of the light reaches a point through the fog (0..1, a wide soft halo around the light).
float glowAt(vec2 w) {
    vec2 d = (w - uSun.xy) * vec2(0.75, 1.0);
    float r = length(d) / uSun.z;
    return exp(-r * r) * 0.65 + exp(-r * 0.9) * 0.35;
}
// The fog's own color at a point: brighter near the horizon and around the light, dimmer high up.
vec3 fogColor(vec2 w) {
    float t = smoothstep(-9.0, 13.0, w.y);
    vec3 c = mix(uFogLo, uFogHi, t);
    return c + uGlowCol * uSun.w * glowAt(w);
}
vec2 worldOf(vec2 uv) { return uView.xy + (uv * 2.0 - 1.0) * uView.zw; }
)";

// ---------------------------------------------------------------- the sky: fog, light, the far landscape
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform float uShaft;   // light shafts
uniform float uDisk;    // the light's disk through the fog
uniform float uDiskR;   // the light's disk radius
void main() {
    vec2 w = worldOf(vUv);
    vec3 fog = fogColor(w);
    vec3 c = fog;
    // the light's disk, softened by the fog (a low sun behind the well)
    float dr = length(w - uSun.xy);
    c += uGlowCol * uDisk * (smoothstep(uDiskR + 0.3, uDiskR, dr) * 0.6 + exp(-dr * dr * 0.35 / (uDiskR * uDiskR / 2.0)) * 0.4);
    // light shafts fanning out from the light (slowly turning)
    vec2 d = w - uSun.xy;
    float a = atan(d.y, d.x);
    float rays = vnoise(vec2(a * 4.0 + uTime * 0.01, 1.7)) * 0.65 + vnoise(vec2(a * 9.0 - uTime * 0.016, 4.3)) * 0.35;
    rays = smoothstep(0.35, 0.9, rays);
    float fall = exp(-length(d) * 0.09) * smoothstep(1.0, 5.0, length(d));
    c += uGlowCol * uShaft * rays * fall * 0.3;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- silhouettes (instanced primitives)
inline const char* SIL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x0, y0, x1, y1
layout(location = 2) in vec4 aB; // r0, r1, kind, fog
layout(location = 3) in vec4 aC; // alpha, angle, extra, tone
uniform mat4 uVP;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    int kind = int(aB.z + 0.5);
    vec2 c, u;
    float hl, hw;
    if (kind == 0) {        // tapered branch from (x0,y0) to (x1,y1), radii r0 r1
        vec2 d = aA.zw - aA.xy;
        float l = length(d);
        u = l > 1e-5 ? d / l : vec2(1.0, 0.0);
        c = (aA.xy + aA.zw) * 0.5;
        float r = max(aB.x, aB.y);
        hl = l * 0.5 + r + 0.05;
        hw = r + 0.05;
    } else if (kind == 4) { // triangle: apex (x0,y0), base center (x1,y1), half width r0
        vec2 d = aA.zw - aA.xy;
        float l = length(d);
        u = l > 1e-5 ? d / l : vec2(0.0, -1.0);
        c = (aA.xy + aA.zw) * 0.5;
        hl = l * 0.5 + 0.08;
        hw = aB.x + 0.08;
    } else {                // disc, box, gear, glow: centered on (x0,y0)
        c = aA.xy;
        float ang = kind == 2 ? aC.y : 0.0;
        u = vec2(cos(ang), sin(ang));
        float r = kind == 2 ? max(aB.x, aB.y) * 1.0 : aB.x * 1.3;
        hl = (kind == 2 ? aB.x : r) + 0.06;
        hw = (kind == 2 ? aB.y : r) + 0.06;
        if (kind == 5) { hl = aB.x; hw = aB.x; }
    }
    vec2 v = vec2(-u.y, u.x);
    vec2 wp = c + u * aPos.x * hl + v * aPos.y * hw;
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

inline const char* SIL_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec3 uSil;      // the silhouettes' black (linear)
uniform vec3 uLamp;     // a lantern's light (linear)
uniform float uVeil;    // the light washing out what stands before it
uniform vec4 uQuiet;    // the well's rectangle (x0, y0, x1, y1): the fog stays thick behind it
uniform vec4 uHudL, uHudR;
uniform vec4 uQuietTL; // the song's title
uniform float uQuietK;  // the least fog behind the well and the HUD columns
uniform float uRimLit;  // the low sun rim-lighting the silhouettes (the drop)
float box(vec2 w, vec4 r, float soft) {
    vec2 q = max(vec2(r.x, r.y) - w, w - vec2(r.z, r.w));
    return 1.0 - smoothstep(0.0, soft, max(q.x, q.y));
}
void main() {
    int kind = int(vB.z + 0.5);
    vec2 p = vW;
    float d;
    float thick = 1.0; // the shape's half thickness (thin twigs get no rim: it would sparkle)
    if (kind == 0) {
        vec2 a = vA.xy, b = vA.zw, ba = b - a;
        float h = clamp(dot(p - a, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        thick = mix(vB.x, vB.y, h);
        d = length(p - a - ba * h) - thick;
    } else if (kind == 1) {
        d = length(p - vA.xy) - vB.x;
    } else if (kind == 2) {
        float ca = cos(vC.y), sa = sin(vC.y);
        vec2 q = p - vA.xy;
        q = vec2(ca * q.x + sa * q.y, -sa * q.x + ca * q.y);
        vec2 k = abs(q) - vec2(vB.x, vB.y);
        d = length(max(k, 0.0)) + min(max(k.x, k.y), 0.0);
    } else if (kind == 3) {
        // a gear: teeth around a rim, a hub, spokes between them (the fog shows through)
        vec2 q = p - vA.xy;
        float r = length(q), th = atan(q.y, q.x) + vC.y;
        float R = vB.x, n = vB.y;
        float teeth = smoothstep(-0.25, 0.25, cos(th * n));
        float outer = r - (R + 0.13 * R * teeth);
        float inner = 0.72 * R - r;
        float ring = max(outer, inner);
        float spokes = abs(sin(th * 3.0)) * r - 0.07 * R;
        float web = max(r - 0.75 * R, spokes);
        float hub = r - 0.2 * R;
        d = min(min(ring, web), hub);
        d = max(d, -(r - 0.07 * R));
    } else if (kind == 4) {
        vec2 a = vA.xy, b = vA.zw;
        vec2 ax = b - a;
        float l = length(ax);
        vec2 u = ax / max(l, 1e-5);
        vec2 q = vec2(dot(p - a, vec2(u.y, -u.x)), dot(p - a, u));
        // isosceles triangle, apex at q = 0, base at q.y = l, half width vB.x
        vec2 t = vec2(vB.x, l);
        q.x = abs(q.x);
        vec2 e1 = q - t * clamp(dot(q, t) / dot(t, t), 0.0, 1.0);
        vec2 e2 = q - t * vec2(clamp(q.x / t.x, 0.0, 1.0), 1.0);
        float s = -sign(t.y);
        vec2 dd = min(vec2(dot(e1, e1), s * (q.x * t.y - q.y * t.x)), vec2(dot(e2, e2), s * (q.y - t.y)));
        d = -sqrt(dd.x) * sign(dd.y);
    } else {
        // a lantern's glow (additive): a soft halo in the lamp's light
        float r = length(p - vA.xy) / vB.x;
        float g = exp(-r * r * 4.0) * 0.7 + exp(-r * 9.0) * 0.6;
        frag = vec4(uLamp * g * vC.x, 0.0);
        return;
    }
    float aa = max(fwidth(d), 1e-4);
    float cov = (1.0 - smoothstep(-aa * 0.7, aa * 0.7, d)) * vC.x;
    if (cov <= 0.002) discard;
    vec3 fog = fogColor(p);
    // depth: the instance's fog, the light washing out what stands before it, the quiet behind the well and HUD
    float k = vB.w;
    k += (1.0 - k) * uVeil * glowAt(p);
    // (behind the well: everything above the ground; soft edges, no visible box)
    float well = box(vec2(p.x, max(p.y, uQuiet.y)), uQuiet, 3.0) * smoothstep(uQuiet.y - 0.2, uQuiet.y + 1.2, p.y);
    float quiet = max(well, max(box(p, uHudL, 3.0), box(p, uHudR, 3.0)));
    quiet = max(quiet, box(p, uQuietTL, 2.5));
    // (there the shapes fade away, showing what is behind them, rather than turning to fog in front of it)
    cov *= 1.0 - quiet * uQuietK;
    if (cov <= 0.002) discard;
    // a lit tone (a lantern's glass) or the black
    vec3 ink = mix(uSil, uLamp, vC.w);
    vec3 col = mix(ink, fog, clamp(k, 0.0, 1.0));
    // the low sun's rim on the edges facing it (thin twigs glow whole)
    if (uRimLit > 0.0 && kind != 5 && kind != 1) {
        vec2 gd = vec2(dFdx(d), dFdy(d));
        vec2 n = length(gd) > 1e-6 ? normalize(gd) : vec2(0.0);
        vec2 toS = normalize(uSun.xy - p);
        float band = 1.0 - smoothstep(aa, aa + 0.035, -d);
        float face = smoothstep(0.0, 0.8, dot(n, toS));
        col += uGlowCol * (0.35 + glowAt(p)) * band * face * uRimLit * (1.0 - k) * (1.0 - k) * 0.6 * smoothstep(3.0 * aa, 7.0 * aa, thick);
    }
    frag = vec4(col * cov, cov);
}
)";

// ---------------------------------------------------------------- a fog bank between two depths
inline const char* BANK_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform float uDensity;  // 0..1
uniform float uY;        // where the bank lies (board y): it thins above
uniform float uThick;    // its vertical extent
uniform vec2 uDrift;     // its drift (board units)
uniform float uScale;    // its puffs' size
uniform float uLayer;
uniform float uBright;   // lit a little (1) or plain
uniform float uPart;     // the drop: the bank tears open from the middle and rolls away to the sides
void main() {
    vec2 w = worldOf(vUv);
    float side = w.x < 0.0 ? -1.0 : 1.0;
    vec2 q = (w + uDrift - vec2(side * uPart * 10.0, 0.0)) * uScale + vec2(uLayer * 13.7, uLayer * 5.1);
    // wisps stretched sideways, rolling slowly
    float n = fbm(q * vec2(0.55, 1.25) + vec2(0.0, uTime * 0.015));
    n = smoothstep(0.32, 0.78, n);
    float h = exp(-max(w.y - uY, 0.0) / max(uThick, 0.1)) * smoothstep(uY - uThick * 3.0, uY, w.y + uThick * 2.0);
    float edge = uPart * (uView.z + 4.0);
    float gap = mix(1.0, smoothstep(edge - 7.0, edge + 1.0, abs(w.x)) * (1.0 + 0.8 * uPart), uPart);
    float a = clamp(n * h * uDensity * gap, 0.0, 0.92);
    vec3 c = fogColor(w) * uBright;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the pieces (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // (unused), alpha
layout(location = 3) in vec4 aC; // kind (0 piece, 1 ghost), dissolve, glint, seed
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// Signed distance (cell units) to the outline of the piece the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right): its own cells merge into one shape, a thin gap parts it from the others.
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m) {
    const float g = 0.035, r = 0.12, rc = 0.06;
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
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec3 uInk;      // the pieces' black (linear)
uniform vec3 uMist;     // the mist a dissolving piece turns to
uniform float uGhostA;
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float sd = shapeSdf(vLocal, m);
    if (kind == 1) {
        // the ghost: a faint shadow of the piece in the fog, its outline a little firmer
        if (!ownCell(vLocal, m)) discard;
        float inside = 1.0 - smoothstep(-aa, aa, sd);
        float line = 1.0 - smoothstep(0.5 * aa, 1.6 * aa, abs(sd + 0.02));
        float a = (inside * 0.07 + line * 0.5) * uGhostA * vB.w;
        frag = vec4(uInk * a, a);
        return;
    }
    if (!ownCell(vLocal, m)) discard;
    float a = (1.0 - smoothstep(-aa, aa * 0.5, sd)) * vB.w;
    // a row leaving: its edges erode into the mist
    float k = vC.y;
    float depth = clamp(-sd, 0.0, 0.5);
    if (k > 0.0) {
        float n = vnoise(vW * 3.1 + vC.w) * 0.55 + vnoise(vW * 8.3 + vC.w * 1.7) * 0.3;
        float e = depth * 1.1 + n * 0.8;
        float thr = k * 1.45 - 0.05;
        a *= smoothstep(thr - 0.05, thr + 0.05, e) * (1.0 - smoothstep(0.75, 1.0, k));
    }
    if (a <= 0.002) discard;
    // the body: flat deep black, a sharp edge (no rim, no tint)
    vec3 c = uInk;
    if (k > 0.0) c = mix(c, uMist, smoothstep(0.0, 0.8, k) * 0.7);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- motes and mist puffs (instanced soft discs)
inline const char* MOTE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, radius, alpha
layout(location = 2) in vec4 aB; // color (linear), softness
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vA;
flat out vec4 vB;
void main() {
    vec2 wp = aA.xy + aPos * aA.z;
    vQ = aPos;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* MOTE_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vA;
flat in vec4 vB;
out vec4 frag;
void main() {
    float r = length(vQ);
    float a = (1.0 - smoothstep(1.0 - vB.w, 1.0, r)) * vA.w;
    if (a <= 0.002) discard;
    frag = vec4(vB.rgb * a, a);
}
)";

// ---------------------------------------------------------------- the light shafts (quarter resolution)
// The bright fog behind everything smeared toward the sun: every silhouette (and the stack, through the frame's
// alpha) casts its shadow into the shafts; a slow angular modulation makes them sweep.
inline const char* RAYS_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform vec2 uSunUv;
uniform float uThr, uTime, uAspect;
float h1(float x) { return fract(sin(x * 91.7) * 43758.5453); }
float n1(float x) { float i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f); return mix(h1(i), h1(i + 1.0), f); }
void main() {
    vec2 d = uSunUv - vUv;
    const int N = 48;
    vec2 stp = d / float(N) * 0.92;
    vec2 uv = vUv;
    float acc = 0.0, wgt = 1.0;
    for (int i = 0; i < N; i++) {
        uv += stp;
        vec4 s = texture(uScene, uv);
        float l = dot(s.rgb, vec3(0.2126, 0.7152, 0.0722));
        acc += smoothstep(uThr, uThr + 1.4, l) * (1.0 - s.a) * wgt;
        wgt *= 0.972;
    }
    acc /= float(N) * 0.5;
    float a = atan(d.y, d.x * uAspect);
    float sweep = 0.55 + 0.45 * n1(a * 5.0 + uTime * 0.06) * n1(a * 11.0 - uTime * 0.04 + 7.0) * 1.6;
    frag = vec4(vec3(acc * sweep), 1.0);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uRays;
uniform float uRayK;
uniform vec3 uRayCol;
uniform float uSlow, uDim, uFade, uBridge, uVig, uGrain, uTime;
uniform vec3 uBridgeCol, uBridgeInk; // linear
uniform vec3 uNight;                 // the slow motion's cool light
uniform float uInvert;               // the drop: the whole scene in negative
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    // the light shafts, never over the stack
    c += uRayCol * texture(uRays, vUv).r * uRayK * 0.4 * (1.0 - clamp(s.a, 0.0, 1.0));
    // slow motion: the fog goes still under a cold, moonlit light
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(g) * uNight, uSlow);
    // the vignette (the fog closing in at the edges)
    vec2 v = vUv * 2.0 - 1.0;
    float vig = 1.0 - uVig * smoothstep(0.35, 1.5, dot(v * vec2(0.82, 1.0), v * vec2(0.82, 1.0)));
    c *= vig;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c = mix(c, 1.0 - c, uInvert);
    // a film grain, finer than a pixel's worth of value: it only keeps the fog from banding
    vec2 px = floor(gl_FragCoord.xy);
    float n = fract(sin(dot(px + fract(uTime * 7.31) * 113.0, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    c += n * uGrain;
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
// Lettering in the fog: the ink with a soft, wide halo of the fog's light around it.
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
    float halo = smoothstep(0.08, 0.48, d);
    halo = halo * halo * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a fragment shader from its source with the common functions inserted after the version line.
inline std::string withCommon(const char* fs, bool shape = false) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace limbo_shaders

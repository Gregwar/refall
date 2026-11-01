#pragma once
// CRYSTAL CAVE's shaders (see crystal.cpp).
//
// BG_FS paints the cave's rock (lit by the section's light, the crystals' glow and the drop) into bgT_; CRYS_* draws
// the cave's crystals and GLOW_* the light sprites over it (glows, sparkles, the falling water). POOL_FS copies the
// cave into the scene and turns everything under the waterline into the pool (the cave mirrored, rippled by the
// drops). SHARD_* draws a cleared row's shards, CELL_* the gem pieces; DOWN_FS / BLUR_FS make the bloom and
// FINAL_FS encodes the frame.
#include <string>

namespace crystal_shaders {

inline const char* COMMON = R"(
uniform vec4 uView;   // center, half extents (board units)
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + vec2(17.3, 5.1))); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.1 + 3.7) * 0.28 + vnoise(p * 4.3 + 9.1) * 0.17; }
vec2 worldAt(vec2 uv) { return uView.xy + (uv * 2.0 - 1.0) * uView.zw; }
vec2 viewUv(vec2 w) { return (w - uView.xy) / uView.zw * 0.5 + 0.5; }
// A Voronoi: distance to the nearest edge, the cell's two ids (inner facets, chunks of rock).
vec3 facets(vec2 p, float seed) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    vec2 best = vec2(0.0), r1 = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 o = 0.15 + 0.7 * hash2(i + g + seed);
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; best = i + g; r1 = r; }
            else if (d < d2) d2 = d;
        }
    return vec3(sqrt(d2) - sqrt(d1), hash(best + seed * 1.7), hash(best + seed + 7.1));
}
float box(vec2 w, vec4 r, float soft) {
    return smoothstep(r.x - soft, r.x, w.x) * smoothstep(r.z + soft, r.z, w.x) * smoothstep(r.y - soft, r.y, w.y) * smoothstep(r.w + soft, r.w, w.y);
}
)";

// ---------------------------------------------------------------- the rock
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uRock, uRockLit, uBand;  // the rock in the dark, where light falls, the geode's agate shell
uniform vec3 uAmb;                    // the section's light (already scaled)
uniform vec3 uDropCol;
uniform float uDrop;                  // the drop's illumination 0..1 (eased)
uniform vec4 uL[8];                   // the clusters' light: x, y, radius, intensity
uniform vec3 uLC[8];
uniform int uNL;
uniform vec4 uHudL, uHudR;
uniform float uWater;
uniform vec3 uWellCol;                // the well's edges: two fine veins of the mineral's light
void main() {
    vec2 w = worldAt(vUv);
    float ex = abs(w.x - uView.x) / uView.z;
    float ey = (w.y - uView.y) / uView.w;
    // the cavity: its walls come closer toward the sides (lit, rough), the depth of the cave behind the well
    float wall = smoothstep(0.42, 1.0, ex);
    float forms = fbm(w * vec2(0.11, 0.16) + 2.0);
    float ridge = 1.0 - abs(2.0 * fbm(w * vec2(0.32, 0.4) + 5.0) - 1.0);
    float grit = fbm(w * 0.9 + 11.0);
    float lit = clamp(wall * 0.7 + (forms - 0.5) * 0.45 + (ridge - 0.6) * 0.35 * (0.3 + wall) + (grit - 0.5) * 0.12, 0.0, 1.0);
    vec3 rock = mix(uRock, uRockLit, lit);
    // keep it quiet behind the well and the HUD's columns
    float well = box(w, vec4(-5.6, -11.0, 5.6, 11.0), 1.2);
    float hud = max(box(w, uHudL, 0.8), box(w, uHudR, 0.8));
    float quiet = max(well, hud);
    // the rock breaks in angular chunks, each tilted its own way (barely a crevice between them), toward the walls
    vec3 ch = facets(w * 0.38 + 0.6 * vec2(forms, ridge), 3.0);
    float relief = (0.25 + 0.75 * wall) * (1.0 - 0.8 * quiet);
    vec3 Nr = normalize(vec3((ch.y - 0.5) * 1.3 * relief, ((ch.z - 0.5) * 1.3 + 0.15) * relief, 1.0));
    float crev = 1.0 - 0.1 * relief * (1.0 - smoothstep(0.0, 0.06, ch.x));
    // the geode's shell at the far sides: thin agate bands following the wall
    float shell = smoothstep(0.86, 1.0, ex + (forms - 0.5) * 0.12);
    float bands = 0.5 + 0.5 * sin((ex + (forms - 0.5) * 0.08) * 150.0);
    rock = mix(rock, uBand * (0.6 + 0.4 * bands), shell * 0.55);
    // the light: the section's, the crystals' glow, the drop's
    vec3 light = uAmb * (0.55 + 0.6 * wall + 0.25 * max(ey, 0.0)) * (0.55 + 0.6 * max(dot(Nr, normalize(vec3(-0.3, 0.7, 0.65))), 0.0));
    for (int i = 0; i < 8; i++) {
        if (i >= uNL) break;
        vec2 d = (w - uL[i].xy) / uL[i].z;
        vec3 toL = normalize(vec3(uL[i].xy - w, 2.5));
        light += uLC[i] * uL[i].w * exp(-dot(d, d)) * (0.35 + 1.0 * max(dot(Nr, toL), 0.0));
    }
    light += uDropCol * uDrop * (0.7 + 1.1 * wall + 0.4 * max(ey, 0.0));
    vec3 c = rock * light * crev;
    c *= 1.0 - 0.42 * well;
    c *= 1.0 - 0.3 * hud;
    // the far shore of the pool: wet stone catching the light
    float shore = exp(-pow((w.y - uWater - 0.12) * 6.0, 2.0));
    c += light * uRockLit * 0.12 * shore * (1.0 - well);
    // the well's edges: fine veins of light, fading out toward the top
    float px = max(fwidth(w.x), 1e-4);
    float vein = exp(-pow((abs(w.x) - 5.1) / (0.9 * px), 2.0)) + 0.2 * exp(-pow((abs(w.x) - 5.1) / (4.0 * px), 2.0));
    vein *= smoothstep(10.6, 6.0, w.y) * step(-10.05, w.y);
    c += uWellCol * vein;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the cave's crystals
inline const char* CRYS_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // base x, y, angle, length
layout(location = 2) in vec4 aB; // width, tip, depth, alpha
layout(location = 3) in vec4 aC; // rgb, glint
uniform mat4 uVP;
out vec2 vQ;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float u = aPos.x * 1.08;
    float v = aPos.y * 0.52 + 0.5;
    vec2 dir = vec2(cos(aA.z), sin(aA.z));
    vec2 perp = vec2(-dir.y, dir.x);
    vec2 wp = aA.xy + dir * (v * aA.w) + perp * (u * aB.x);
    vQ = vec2(u, v);
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CRYS_FS = R"(#version 330 core
in vec2 vQ;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec3 uLight;   // toward the light (world)
uniform vec3 uFog;     // the cave's depth
uniform float uGlow;   // the crystals' inner light
uniform vec4 uHudL, uHudR, uQuietA, uQuietB;
void main() {
    float u = vQ.x, v = vQ.y;
    float tip = vB.y, t0 = 1.0 - tip;
    // the point's apex sits a little off the axis (each crystal its own)
    float off = (fract(sin(dot(vA.xy, vec2(41.3, 17.9))) * 4375.85) - 0.5) * 0.9;
    float s = clamp((v - t0) / tip, 0.0, 1.0);
    float lo = mix(-1.0, off, s), hi = mix(1.0, off, s);
    float d = max(lo - u, u - hi);
    float aa = max(fwidth(u) * 1.2, 1e-3);
    float cov = (1.0 - smoothstep(-aa, aa, d)) * step(0.0, v) * step(v, 1.0);
    if (cov <= 0.0) discard;
    // three faces of a hexagonal prism, three facets of its point (their edges meet at the apex)
    float bl = mix(-0.38, off, s), br = mix(0.38, off, s);
    float face = u < bl ? 0.0 : (u > br ? 2.0 : 1.0);
    vec3 N = face < 0.5 ? vec3(-0.82, 0.0, 0.57) : (face > 1.5 ? vec3(0.82, 0.0, 0.57) : vec3(0.0, 0.0, 1.0));
    if (v > t0) N = normalize(N + vec3(0.0, 0.8, 0.0));
    // the light in the crystal's frame
    vec2 dir = vec2(cos(vA.z), sin(vA.z));
    vec2 perp = vec2(-dir.y, dir.x);
    vec3 L = normalize(vec3(dot(uLight.xy, perp), dot(uLight.xy, dir), uLight.z));
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, normalize(L + vec3(0, 0, 1))), 0.0), 30.0);
    vec3 tint = vC.rgb;
    vec3 white = vec3(1.0, 0.98, 0.95);
    // a quartz point: milky at its root, clearer and deeper in color toward its point
    float milk = 1.0 - smoothstep(0.0, 0.5, v);
    vec3 body = mix(tint, mix(tint, vec3(0.75), 0.6), milk * 0.75);
    float along = 0.5 + 0.5 * smoothstep(0.0, 1.0, v);
    vec3 c = body * (0.05 + 0.36 * diff) * along;
    // light inside: each face glows a little differently (the light refracted through the prism)
    float fg = face < 0.5 ? 0.55 : (face > 1.5 ? 0.8 : 1.0);
    c += body * uGlow * (0.14 + 0.3 * fg * smoothstep(0.1, 0.9, v)) * along;
    // growth striations across the prism's faces (fine, faint)
    if (v < t0) c *= 1.0 - 0.08 * smoothstep(0.6, 1.0, sin(v * vA.w * 9.0 + vB.x * 40.0));
    c += white * spec * 0.45 * (1.0 - vB.z);
    // the edges between faces catch the light; the silhouette a fine rim; the point's edge
    float ew = 0.05;
    float edge = max(1.0 - smoothstep(0.0, ew, abs(u - bl)), 1.0 - smoothstep(0.0, ew, abs(u - br)));
    float rim = 1.0 - smoothstep(0.0, 0.07, -d);
    float ridge = (v < t0) ? 1.0 - smoothstep(0.0, 0.015, abs(v - t0)) : 0.0;
    c += mix(body, white, 0.3) * (0.3 * edge + 0.24 * rim + 0.25 * ridge) * (0.35 + 0.65 * diff) * (0.6 + 0.6 * uGlow);
    // a glint (the highs, a Tetris's ring): the edges and the point light up
    c += mix(tint, white, 0.5) * vC.a * (0.55 * edge + 0.45 * rim * s + 0.3 * s + 0.08);
    // depth: the far crystals sink into the cave's dark
    c = mix(c, uFog, vB.z * 0.7);
    // translucent bodies, solid edges
    float a = cov * vB.w * smoothstep(0.0, 0.2, v) * (0.66 + 0.34 * max(edge, rim));
    // quiet behind the HUD
    float q = max(max(box(vW, uHudL, 0.6), box(vW, uHudR, 0.6)), max(box(vW, uQuietA, 0.8), box(vW, uQuietB, 0.8)));
    a *= 1.0 - 0.75 * q;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- light sprites (additive)
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, rx, ry
layout(location = 2) in vec4 aB; // rgb, intensity
layout(location = 3) in vec4 aC; // kind, rotation
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float c = cos(aC.y), s = sin(aC.y);
    vec2 l = aPos * aA.zw;
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vQ = aPos;
    vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    int kind = int(vC.x + 0.5);
    float r2 = dot(vQ, vQ);
    float k = 0.0;
    if (kind == 0) {
        // a soft glow
        k = exp(-r2 * 3.5) * (1.0 - smoothstep(0.7, 1.0, sqrt(r2)));
    } else if (kind == 1) {
        // a sparkle: a small star of light
        vec2 q = abs(vQ);
        float arms = exp(-q.x * 28.0) * exp(-q.y * 3.2) + exp(-q.y * 28.0) * exp(-q.x * 3.2);
        k = exp(-r2 * 30.0) + 0.55 * arms;
        k *= 1.0 - smoothstep(0.8, 1.0, max(q.x, q.y));
    } else {
        // a falling drop of water: a bright bead, a faint streak above it
        float bead = exp(-dot(vQ * vec2(1.0, 4.0) + vec2(0.0, 3.0), vQ * vec2(1.0, 4.0) + vec2(0.0, 3.0)) * 1.6);
        float streak = exp(-vQ.x * vQ.x * 9.0) * smoothstep(-0.75, 1.0, vQ.y) * (1.0 - smoothstep(0.7, 1.0, vQ.y)) * 0.12;
        k = bead + streak;
    }
    vec3 c = vB.rgb * vB.a * k;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the pool
inline const char* POOL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uBg;
uniform float uWater;
uniform vec4 uRip[16];   // x, depth, age (s), amplitude
uniform int uNR;
uniform vec3 uDeep;      // the water's own dark
uniform vec3 uGlintCol;
uniform float uTime;
void main() {
    vec2 w = worldAt(vUv);
    vec3 bg = texture(uBg, vUv).rgb;
    float depth = uWater - w.y;
    if (depth < -0.05) { frag = vec4(bg, 0.0); return; }
    float dz = max(depth, 0.0);
    // the surface seen from a little above: rings flattened by the distance
    float persp = mix(4.5, 2.2, smoothstep(0.0, 2.5, dz));
    vec2 n = vec2(0.0);
    float crest = 0.0;
    for (int i = 0; i < 16; i++) {
        if (i >= uNR) break;
        vec4 r = uRip[i];
        vec2 q = vec2(w.x - r.x, (dz - r.y) * persp);
        float dist = length(q);
        float rad = 0.12 + r.z * 1.1;
        float env = exp(-r.z * 2.0) * r.w * smoothstep(0.0, 0.08, r.z);
        float x = (dist - rad) * 9.0;
        float wave = exp(-x * x * 0.5) * env;
        n += (dist > 1e-3 ? q / dist : vec2(0.0)) * wave * sin(x * 2.4);
        crest += wave * max(sin(x * 2.4 + 1.2), 0.0);
    }
    // a faint breath on the water
    n += 0.03 * (vec2(vnoise(vec2(w.x * 1.3, dz * 5.0 + uTime * 0.25)), vnoise(vec2(w.x * 1.1 + 7.0, dz * 4.0 - uTime * 0.2))) - 0.5);
    // the cave mirrored in it
    vec2 rw = vec2(w.x + n.x * 0.35, uWater + dz * 1.15 + 0.05 + n.y * 0.5);
    vec3 refl = texture(uBg, viewUv(rw)).rgb;
    float fres = mix(0.85, 0.45, smoothstep(0.0, 2.2, dz));
    vec3 c = uDeep * (1.0 + 0.4 * smoothstep(0.0, 2.0, dz)) + refl * fres;
    c += uGlintCol * crest * 0.18;
    // the waterline: a thin, soft line of light (not behind the well)
    float well = box(w, vec4(-5.6, -11.0, 5.6, 11.0), 1.2);
    c += uGlintCol * 0.05 * exp(-dz * dz * 400.0) * (1.0 - well);
    c *= 1.0 - 0.42 * well;
    float k = smoothstep(-0.05, 0.02, depth);
    frag = vec4(mix(bg, c, k), 0.0);
}
)";

// ---------------------------------------------------------------- shards
inline const char* SHARD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size, rotation
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // tumble phase, underwater, seed
uniform mat4 uVP;
out vec2 vQ;
out vec2 vW;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float c = cos(aA.w), s = sin(aA.w);
    vec2 l = aPos * aA.z;
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vQ = aPos;
    vW = wp;
    vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SHARD_FS = R"(#version 330 core
in vec2 vQ;
in vec2 vW;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec3 uDeep;
float sdTri(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 e0 = b - a, e1 = c - b, e2 = a - c;
    vec2 v0 = p - a, v1 = p - b, v2 = p - c;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)), vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                 vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}
void main() {
    float sd = vC.z * 6.2831;
    vec2 a = vec2(cos(sd), sin(sd)) * 0.95;
    vec2 b = vec2(cos(sd + 2.3 + 0.4 * sin(sd * 3.0)), sin(sd + 2.3 + 0.4 * sin(sd * 3.0))) * 0.8;
    vec2 c = vec2(cos(sd + 4.2), sin(sd + 4.2)) * 0.9;
    float d = sdTri(vQ, a, b, c);
    float aa = max(fwidth(d), 1e-3);
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.0) discard;
    // it tumbles: its face turns to the light and away, flashing as it passes
    float face = 0.5 + 0.5 * sin(vC.x);
    float flash = pow(face, 14.0);
    vec3 col = vB.rgb * (0.55 + 0.75 * face) + vec3(1.0, 0.97, 0.92) * flash * 0.9;
    col += vB.rgb * 0.5 * (1.0 - smoothstep(0.0, 0.12, -d));
    col *= 1.0 - 0.3 * vC.y;
    float al = cov * vB.a;
    frag = vec4(col * al, 0.0) + vec4(0.0, 0.0, 0.0, 0.0);
}
)";

// ---------------------------------------------------------------- the gems (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // type, glint, kind, dull
layout(location = 4) in vec4 aD; // facet pattern origin
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// The shape of a piece (mask bits: left right down up, down-left down-right up-left up-right): its own cells merge
// into one stone, a thin seam parts it from the others.
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m) {
    const float g = 0.035, r = 0.09, rc = 0.06;
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

inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec3 uLight;   // toward the light
uniform float uSpark;  // the gems' sparkle (the highs, a peak)
void main() {
    int m = int(vA.w + 0.5);
    int kind = int(vC.z + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float sd = shapeSdf(vLocal, m);
    if (kind == 1) {
        // the ghost: a fine line of the mineral's light where the piece will land
        float line = 1.0 - smoothstep(0.4 * aa, 1.6 * aa, abs(sd + 0.04));
        float a = line * vB.w;
        frag = vec4(vB.rgb * a, a);
        return;
    }
    if (!ownCell(vLocal, m)) discard;
    float cov = (1.0 - smoothstep(-aa, aa * 0.5, sd)) * vB.w;
    if (cov <= 0.0) discard;
    float type = vC.x;
    // (clamped to half a cell: every cell then agrees with its neighbours where they meet)
    float depth = clamp(-sd, 0.0, 0.465);
    // the outline's normal (outward), cut into facets every 45 degrees
    const float e = 0.02;
    vec2 n = vec2(shapeSdf(vLocal + vec2(e, 0.0), m) - shapeSdf(vLocal - vec2(e, 0.0), m),
                  shapeSdf(vLocal + vec2(0.0, e), m) - shapeSdf(vLocal - vec2(0.0, e), m));
    n = length(n) > 1e-4 ? normalize(n) : vec2(0.0, 1.0);
    float ang = atan(n.y, n.x);
    ang = floor(ang / 0.785398 + 0.5) * 0.785398;
    vec2 nq = vec2(cos(ang), sin(ang));
    // A step cut (as an emerald is cut): two tiers of crown facets following the outline, mitred at the corners,
    // then the table, through which the pavilion's steps are seen as concentric reflections.
    const float T1 = 0.07, T2 = 0.16;
    float tA = smoothstep(T1 - 0.6 * aa, T1 + 0.6 * aa, depth); // 0: outer tier
    float tB = smoothstep(T2 - 0.6 * aa, T2 + 0.6 * aa, depth); // 1: the table
    vec3 N0 = normalize(vec3(nq * 0.92, 0.4)), N1 = normalize(vec3(nq * 0.52, 0.85)), N2 = vec3(0.0, 0.0, 1.0);
    vec3 N = mix(mix(N0, N1, tA), N2, tB);
    vec3 L = normalize(uLight);
    float diff = dot(N, L);
    float spec = pow(max(dot(N, normalize(L + vec3(0.0, 0.0, 1.0))), 0.0), 60.0) * (1.0 - tB);
    vec3 tint = vB.rgb;
    vec3 white = vec3(1.0, 0.98, 0.94);
    // the crown: lit facets bright, the others deep in the stone's color
    float lam = max(diff, 0.0);
    vec3 crownC = tint * (0.1 + 1.3 * lam * lam) + white * spec * (0.6 + 0.5 * uSpark);
    // the table: the pavilion's steps (their spacing the piece type's own), light glowing from the heart
    float period = 4.2 + 1.6 * mod(type, 3.0);
    float stepv = abs(fract((depth - T2) * period + 0.3 * mod(type, 2.0)) - 0.5) * 2.0;
    stepv = smoothstep(0.15, 0.85, stepv);
    float heart = smoothstep(0.1, 0.465, depth);
    vec3 tableC = tint * (0.3 + 0.2 * stepv + 0.75 * heart);
    // faint inclusions drifting in the stone (cut in its own frame: they travel with it)
    float ta = type * 0.9;
    mat2 rot = mat2(cos(ta), sin(ta), -sin(ta), cos(ta));
    vec2 pp = rot * (vD.xy + vLocal);
    tableC *= 1.0 + 0.12 * (vnoise(pp * 1.7 + type * 3.1) - 0.5);
    // a soft reflection band across the table (in the piece's frame)
    float band = exp(-pow(fract(dot(pp, vec2(0.6, 0.8)) * 0.22 + 0.13 * type) - 0.5, 2.0) * 70.0);
    tableC += mix(tint, white, 0.5) * band * 0.1;
    vec3 c = mix(crownC, tableC, tB);
    // fine lines where the tiers meet (light on the lit side, dark on the other)
    float lines = exp(-pow((depth - T1) / 0.009, 2.0)) + exp(-pow((depth - T2) / 0.011, 2.0));
    float litSide = dot(nq, normalize(L.xy));
    c = mix(c, litSide > 0.0 ? mix(tint, white, 0.6) : tint * 0.12, 0.45 * lines * abs(litSide));
    // the very edge: a thin rim of light on the lit side
    float rim = 1.0 - smoothstep(0.0, 0.022, depth);
    c += mix(tint, white, 0.5) * rim * max(litSide, 0.0) * 0.5;
    // a glint (a lock, the highs): the crown flashes softly, a star on its lit corner
    c += mix(tint, white, 0.6) * vC.y * (0.3 * (1.0 - tB) + 0.1 + 0.8 * spec);
    // game over: the light goes out of the stone
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(g) * vec3(0.36, 0.36, 0.4), vC.w);
    frag = vec4(c * cov, cov);
}
)";

// ---------------------------------------------------------------- bloom
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(float(x), float(y)) * uTexel).rgb;
    c /= 9.0;
    frag = vec4(max(c - vec3(0.16), vec3(0.0)), 1.0);
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
uniform vec3 uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: time stands still in a cold cave (silver, the minerals drained)
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c, 0.6) * vec3(0.82, 0.92, 1.08), uSlow);
    // a soft knee: glowing stones never burn to white
    c = c / (1.0 + 0.35 * max(max(c.r, c.g), c.b));
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
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
// Lettering in pale crystal light, a soft dark halo of the cave around it.
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
    float halo = smoothstep(0.22, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a fragment shader from its source with the common functions inserted after the version line.
inline std::string withCommon(const char* fs, bool shape) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace crystal_shaders

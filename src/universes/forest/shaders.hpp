#pragma once
// GLOWING FOREST's shaders (see forest.cpp).
//
// BG_FS paints the night forest (a dark painted backdrop, two hazy layers of far trunks, the near trunks, banks of
// mist and the mossy floor with its specks of glowing moss); HALO_FS the soft light a glowing plant casts on the mist
// around it; PLANT_FS the bioluminescent life (mushrooms, mycena, glowworm threads, ferns, glowing bells, moss cushions:
// instanced quads, one per plant, each drawn as a small painted SDF); VEIL_FS the well; MOTE_FS fireflies and spores;
// PIECE_FS the blocks (instanced per cell); DOWN_FS / BLUR_FS the glow and FINAL_FS the frame.
#include <string>

namespace forest_shaders {

inline const char* COMMON = R"(
uniform float uGS;   // the ground's phase (the forest's seed)
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
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// The forest floor's height at x (mirrors groundY in forest.cpp).
float groundY(float x) {
    float ax = abs(x);
    float k = smoothstep(4.5, 8.0, ax);
    return -10.55 + k * (0.22 * sin(0.37 * x + uGS) + 0.12 * sin(1.13 * x + 2.0 * uGS)) + 0.35 * smoothstep(14.0, 21.0, x);
}
)";

// ---------------------------------------------------------------- the forest (full screen)
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uSkyTop, uSkyLow, uMist, uMoss, uGlowA, uGlowB;
uniform vec4 uTrunk[8];
uniform int uTrunks;
uniform float uTime, uSeed, uMistK, uLife, uWaveR, uWaveK, uAA, uSlow;
// a column of a far layer: its trunk's distance (negative inside)
float farTrunk(vec2 w, float sc, float layer, out float id) {
    float gx = (w.x + uSeed * 13.0 + layer * 7.3) / sc;
    id = floor(gx);
    float f = fract(gx);
    float h = hash12(vec2(id, layer * 17.0 + 3.0));
    if (h < 0.38) return 9.0;
    float cx = 0.5 + (hash12(vec2(id, layer + 5.0)) - 0.5) * 0.5;
    float lean = (hash12(vec2(id, layer + 9.0)) - 0.5) * 0.03;
    float wid = (layer < 0.5 ? 0.06 : 0.09) * (0.6 + 0.8 * hash12(vec2(id, layer + 11.0)));
    float g = groundY(w.x) + 1.5 - layer * 0.6;
    wid *= 1.0 + 0.5 * exp(-max(w.y - g, 0.0) / 1.0);
    return (abs(f - cx - lean * (w.y + 10.0)) - wid) * sc;
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float t = smoothstep(uView.y - uView.w, uView.y + uView.w, w.y);
    float gy = groundY(w.x);
    // the night between the trees: a dark wash, the mist lying luminous low down
    vec3 c = mix(uSkyLow, uSkyTop, pow(t, 0.7));
    float wash = fbm(w * vec2(0.05, 0.09) + vec2(uSeed, 0.0));
    c *= 0.75 + 0.5 * wash;
    float lowMist = exp(-max(w.y - gy, 0.0) / 6.5);
    float hazeN = 0.7 + 0.6 * fbm(vec2(w.x * 0.06 + uTime * 0.004, w.y * 0.12) + uSeed * 1.7);
    c += uMist * (0.05 + 0.22 * lowMist) * hazeN * uMistK;
    // faint shafts of moonlight slanting through the canopy, broken by the leaves
    for (int k = 0; k < 3; k++) {
        float fk = float(k);
        float x0 = (fk - 1.0) * 13.0 + (hash12(vec2(fk, uSeed)) - 0.5) * 4.0;
        float sx = w.x - x0 - (w.y - 12.0) * -0.32;
        float wdt = 1.2 + 0.8 * hash12(vec2(fk + 3.0, uSeed));
        float beam = exp(-sx * sx / (wdt * wdt)) * smoothstep(-11.0, 10.0, w.y);
        beam *= 0.5 + 0.8 * fbm3(vec2(sx * 0.6, w.y * 0.08 + uTime * 0.01) + fk * 5.0);
        c += uMist * beam * 0.06 * uMistK;
    }
    // two hazy layers of far trunks, each veiled by mist
    for (int L = 0; L < 2; L++) {
        float layer = float(L);
        float id;
        float sc = L == 0 ? 1.3 : 2.2;
        float wob = (fbm3(vec2(w.x * 1.3, w.y * 0.35) + layer * 4.0 + uSeed) - 0.5) * (0.12 + 0.08 * layer);
        float d = farTrunk(w, sc, layer, id) + wob;
        float soft = L == 0 ? 0.1 : 0.04;
        float cov = 1.0 - smoothstep(-soft, soft, d);
        float bark = 0.85 + 0.3 * vnoise(vec2(w.x * 4.0, w.y * 0.4) + id);
        vec3 tc = c * (L == 0 ? 0.6 : 0.32) * bark;
        c = mix(c, tc, cov);
        // mist drifting in front of the layer
        float m = fbm(vec2(w.x * 0.07 + uTime * 0.008 * (1.0 + layer), w.y * 0.16) + layer * 9.0 + uSeed);
        float band = exp(-max(w.y - gy - 0.5, 0.0) / (3.0 + 2.0 * layer));
        c += uMist * smoothstep(0.3, 0.85, m) * band * 0.12 * uMistK;
    }
    // the near trunks: dark painted bark, a mossy foot, a faint glow on their edges from the life around them
    for (int i = 0; i < 8; i++) {
        if (i >= uTrunks) break;
        vec4 T = uTrunk[i];
        float g = groundY(T.x);
        float y = w.y - g;
        float cx = T.x + T.z * y;
        float hw = T.y * (1.0 + T.w * exp(-max(y, 0.0) / 0.9));
        float wob = (fbm3(vec2(w.x * 0.8, w.y * 0.25) + float(i) * 3.1) - 0.5) * 0.14;
        float d = abs(w.x - cx) - hw + wob;
        float cov = 1.0 - smoothstep(-uAA * 1.5, uAA * 1.5, d);
        if (cov <= 0.0) continue;
        float u = clamp((w.x - cx) / hw, -1.0, 1.0);
        float streak = vnoise(vec2(u * 3.0 + float(i) * 7.0, w.y * 0.45)) * 0.6 + vnoise(vec2(u * 9.0, w.y * 1.4)) * 0.4;
        vec3 bark = uSkyLow * 0.5 + uMoss * 0.012 * (0.5 + streak);
        // moss climbing the foot, a little lit
        float moss = smoothstep(0.5, 0.75, fbm3(vec2(u * 2.0, w.y * 0.7) + float(i))) * exp(-max(y, 0.0) / 3.0);
        bark += uMoss * 0.03 * moss;
        // the round of the trunk: its edges catch the mist's light and the forest's glow
        float rim = pow(abs(u), 5.0);
        bark += uMist * 0.12 * rim * (0.35 + lowMist) * (0.6 + 0.6 * vnoise(vec2(u * 2.0, w.y * 0.8) + float(i)));
        bark += mix(uGlowA, uGlowB, 0.5 + 0.5 * sin(w.y * 0.3 + float(i))) * 0.008 * rim * uLife * exp(-max(y, 0.0) / 6.0);
        c = mix(c, bark, cov);
    }
    // the floor: dark moss, ragged against the mist, specks of glowing moss along its top
    float edge = gy + (fbm3(vec2(w.x * 1.1, 0.0) + uSeed) - 0.5) * 0.35 + (vnoise(vec2(w.x * 6.0, 1.0)) - 0.5) * 0.06;
    float floorCov = 1.0 - smoothstep(-uAA * 1.5, uAA * 1.5, w.y - edge);
    if (floorCov > 0.0) {
        float depth = edge - w.y;
        vec3 fc = uSkyLow * 0.4 + uMoss * (0.006 + 0.01 * fbm3(w * 1.3)) * exp(-depth / 0.8);
        fc += uMist * 0.05 * exp(-depth / 0.12);
        vec2 g = w * vec2(5.0, 5.0);
        vec2 id = floor(g), f = fract(g) - 0.5;
        float h = hash12(id + uSeed * 3.0);
        float near = smoothstep(4.8, 6.5, abs(w.x));
        if (h > 0.8 && depth < 1.4) {
            vec2 o = (vec2(hash12(id + 1.3), hash12(id + 7.7)) - 0.5) * 0.6;
            float r = length(f - o);
            float s = exp(-r * r / 0.012) * exp(-depth / 0.5);
            float dist = abs(w.x) - 5.5;
            float wave = uWaveK * exp(-pow(dist - uWaveR, 2.0) / 2.5);
            float live = 0.25 * uLife + 2.5 * wave;
            fc += mix(uGlowA, uGlowB, hash12(id + 2.0)) * s * 0.09 * live * near;
        }
        c = mix(c, fc, floorCov);
    }
    // ground fog over everything, low and slow
    float fog = fbm(vec2(w.x * 0.09 - uTime * 0.012, w.y * 0.3) + uSeed * 2.0);
    float fb = exp(-max(w.y - gy, 0.0) / 1.6) * (1.0 - 0.5 * floorCov);
    c += mix(uMist, uGlowA * 0.4, 0.2 * uLife) * smoothstep(0.3, 0.9, fog) * fb * 0.14 * uMistK;
    // the drop's wave of light lights the fog it passes through, low over the floor
    float wv = uWaveK * exp(-pow(abs(w.x) - 5.5 - uWaveR + 0.25 * max(w.y + 10.0, 0.0), 2.0) / 3.0) * smoothstep(5.0, 6.5, abs(w.x));
    c += mix(uGlowA, uGlowB, 0.3) * wv * exp(-max(w.y - gy, 0.0) / 2.5) * (0.4 + 0.6 * fog) * 0.012;
    // a painted grain: pigment settling in the paper
    c *= 0.9 + 0.2 * fbm3(gl_FragCoord.xy * 0.08 + uSeed);
    c *= 1.0 + (hash12(floor(gl_FragCoord.xy) + fract(uSeed) * 91.0) - 0.5) * 0.07;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the light a plant casts on the mist
// a: x, y, radius, alpha. b: color.
inline const char* HALO_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB;
void main() {
    vL = aPos;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z, 0.0, 1.0);
}
)";
inline const char* HALO_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB;
out vec4 frag;
void main() {
    float r2 = dot(vL, vL);
    if (r2 >= 1.0) discard;
    float a = (exp(-r2 * 4.0) - exp(-4.0)) * vA.w;
    frag = vec4(vB.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the life (instanced quads)
// a: base x, y, scale, kind. b: seed, grow (0..1), glow, flip. c: glow color, wave. d: second color, alpha.
inline const char* PLANT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vP;
flat out vec4 vA, vB, vC, vD;
void main() {
    int kind = int(aA.w + 0.5);
    vec2 lo = vec2(-1.5, -0.3), hi = vec2(1.5, 3.0);
    if (kind == 2) { lo = vec2(-1.0, -5.6); hi = vec2(1.0, 0.2); }
    if (kind == 5) { lo = vec2(-1.3, -0.3); hi = vec2(1.3, 0.8); }
    vec2 l = mix(lo, hi, aPos * 0.5 + 0.5);
    vP = l;
    vA = aA; vB = aB; vC = aC; vD = aD;
    vec2 lp = l;
    if (aB.w > 0.5) lp.x = -lp.x;
    gl_Position = uVP * vec4(aA.xy + lp * aA.z, 0.0, 1.0);
}
)";

inline const char* PLANT_FS = R"(#version 330 core
in vec2 vP;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform float uSlow, uTime, uAAp;
uniform vec3 uMoon;
float h1(float s, float k) { return fract(sin(s * 91.7 + k * 47.3) * 43758.5453); }
// a quadratic curve's nearest point: distance, parameter (sampled, plants are small)
vec2 bezDist(vec2 p, vec2 a, vec2 b, vec2 c) {
    float best = 1e9, bt = 0.0;
    vec2 prev = a;
    for (int i = 1; i <= 12; i++) {
        float t = float(i) / 12.0;
        vec2 q = mix(mix(a, b, t), mix(b, c, t), t);
        vec2 pa = p - prev, ba = q - prev;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        float d = length(pa - ba * h);
        if (d < best) { best = d; bt = (float(i - 1) + h) / 12.0; }
        prev = q;
    }
    return vec2(best, bt);
}
void main() {
    vec2 p = vP;
    int kind = int(vA.w + 0.5);
    float seed = vB.x, grow = vB.y, glow = vB.z;
    vec3 col = vC.rgb, col2 = vD.rgb;
    float aa = max(uAAp / vA.z, 0.004);
    float e = 0.0;      // emission (0.. a little over 1)
    float a = 0.0;      // coverage of the plant's dark matter
    vec3 tint = col;
    float g = smoothstep(0.0, 1.0, grow);
    // a painted edge: a little wobble
    float wob = (vnoise(p * 9.0 + seed * 31.0) - 0.5) * 0.02;
    if (kind == 0) {
        // a cluster of mushrooms: domed caps glowing, gills brighter under them, stems pale
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.2, 0.6 + fi * 0.2, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 1.0) * 0.52 + (h1(seed, fi) - 0.5) * 0.25;
            float hh = (i == 1 ? 1.15 : 0.55 + 0.4 * h1(seed, fi + 3.0)) * gi;
            float cr = (i == 1 ? 0.42 : 0.26 + 0.12 * h1(seed, fi + 5.0)) * (0.4 + 0.6 * gi);
            float lean = (h1(seed, fi + 7.0) - 0.5) * 0.35 + (fi - 1.0) * 0.12;
            vec2 top = vec2(x0 + lean, hh);
            float sw = 0.05 + 0.02 * cr;
            float ds = sdSeg(p, vec2(x0, 0.0), top) - sw * (1.0 - 0.3 * clamp(p.y / max(hh, 0.01), 0.0, 1.0)) + wob;
            vec2 q = p - top;
            float ch = cr * 0.62;
            float dc = (length(q / vec2(cr, ch)) - 1.0) * min(cr, ch);
            float under = -(q.y + 0.08 * cr - 0.35 * ch * (q.x / cr) * (q.x / cr));
            dc = max(dc, under) + wob;
            float cs = 1.0 - smoothstep(-aa, aa, ds), cc = 1.0 - smoothstep(-aa, aa, dc);
            // the stem: a faint glow, lit up toward the cap
            float sl = clamp(p.y / max(hh, 0.01), 0.0, 1.0);
            e = max(e, cs * (0.12 + 0.35 * sl * sl));
            a = max(a, cs * 0.9);
            if (cc > 0.0) {
                float in_ = clamp(-dc / (ch * 0.5), 0.0, 1.0);
                float rimL = exp(-max(q.y + 0.05 * cr, 0.0) / (ch * 0.35));            // the bright gill line
                float spots = 0.0;
                vec2 sg = q / cr * 3.2 + seed * 13.0 + fi * 5.0;
                vec2 sid = floor(sg), sf = fract(sg) - 0.5;
                if (hash12(sid) > 0.55) spots = exp(-dot(sf, sf) / 0.03);
                float body = 0.45 + 0.35 * (1.0 - in_) + 0.15 * vnoise(q * 14.0 + seed);
                e = max(e, cc * (body + 0.55 * rimL + 0.35 * spots));
                a = max(a, cc);
            }
        }
    } else if (kind == 1) {
        // mycena: slender threads with small glowing bells
        for (int i = 0; i < 5; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.12, 0.5 + fi * 0.12, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 2.0) * 0.26 + (h1(seed, fi) - 0.5) * 0.18;
            float hh = (0.7 + 1.3 * h1(seed, fi + 2.0)) * gi;
            float bend = (h1(seed, fi + 4.0) - 0.5) * 0.6;
            vec2 top = vec2(x0 + bend, hh);
            vec2 bd = bezDist(p, vec2(x0, 0.0), vec2(x0, hh * 0.6), top);
            float ds = bd.x - 0.018;
            float cs = 1.0 - smoothstep(-aa, aa, ds);
            e = max(e, cs * (0.12 + 0.3 * bd.y));
            a = max(a, cs * 0.8);
            float br = (0.1 + 0.07 * h1(seed, fi + 6.0)) * (0.3 + 0.7 * gi);
            vec2 q = p - top - vec2(0.0, -br * 0.15);
            // a bell: a pointed dome, open below
            float cb = 1.0 - smoothstep(-aa, aa, (length(q / vec2(br, br * 1.1)) - 1.0) * br);
            cb *= smoothstep(-br * 0.45, -br * 0.3, q.y);
            float rimL = exp(-abs(q.y + br * 0.3) / (br * 0.25));
            e = max(e, cb * (0.6 + 0.5 * rimL));
            a = max(a, cb);
        }
    } else if (kind == 2) {
        // glowworm silk: threads hanging from the canopy, beaded with drops of light, brightest at their tips
        for (int i = 0; i < 5; i++) {
            float fi = float(i);
            if (fi > 1.0 + 3.0 * h1(seed, 9.0)) break;
            float gi = smoothstep(fi * 0.12, 0.55 + fi * 0.12, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 1.5) * 0.3 + (h1(seed, fi) - 0.5) * 0.22;
            float len = (1.6 + 3.6 * h1(seed, fi + 2.0)) * gi;
            float sway = 0.04 * sin(uTime * 0.4 + seed + fi * 1.7);
            float yy = clamp(p.y, -len, 0.0);
            float xs = x0 + sway * (-yy / 3.0);
            float dl = abs(p.x - xs) - 0.007;
            float th = (1.0 - smoothstep(-aa, aa, dl)) * step(-len, p.y) * step(p.y, 0.0);
            e = max(e, th * 0.3 * (0.3 + 0.7 * (-p.y / max(len, 0.1))));
            a = max(a, th * 0.3);
            // the drops of light along it
            float sp = 0.3;
            float k = floor(-p.y / sp);
            for (int j = 0; j < 2; j++) {
                float kk = k + float(j);
                float dy = -(kk + 0.5) * sp;
                float tipK = smoothstep(0.3, 1.0, -dy / max(len, 0.1));
                if (dy < -len || hash12(vec2(kk, seed * 10.0 + fi)) > 0.25 + 0.4 * tipK) continue;
                float br = 0.014 + 0.026 * tipK + 0.014 * hash12(vec2(kk + 3.0, fi));
                float xd = x0 + sway * (-dy / 3.0);
                vec2 q = p - vec2(xd, dy);
                float cb = 1.0 - smoothstep(-aa, aa, length(q) - br);
                float core = exp(-dot(q, q) / (br * br * 0.5));
                e = max(e, cb * (0.5 + 0.5 * tipK + 0.5 * core));
                a = max(a, cb);
            }
            // the tip: a larger drop
            vec2 qt = p - vec2(x0 + sway * (len / 3.0), -len);
            float bt = 0.06 * gi;
            float ct = 1.0 - smoothstep(-aa, aa, length(qt) - bt);
            e = max(e, ct * (1.0 + 0.6 * exp(-dot(qt, qt) / (bt * bt * 0.4))));
            a = max(a, ct);
        }
    } else if (kind == 3) {
        // a fern: arching fronds, a glowing midrib, leaflets lit along their edges, dots of glowing spores
        for (int j = 0; j < 3; j++) {
            float fj = float(j);
            float gj = smoothstep(fj * 0.15, 0.7 + fj * 0.15, grow);
            if (gj <= 0.0) continue;
            float ang = (fj - 1.0) * 0.62 + (h1(seed, fj) - 0.5) * 0.3;
            float L = (j == 1 ? 2.4 : 1.8 + 0.4 * h1(seed, fj + 2.0)) * gj;
            vec2 tip = vec2(sin(ang) * L * 0.95, cos(ang) * L * 0.75);
            vec2 ctrl = vec2(sin(ang) * L * 0.2, L * 0.95);
            vec2 bd = bezDist(p, vec2(0.0), ctrl, tip);
            float t = bd.y;
            // leaflets: a row each side, smaller toward the tip, swept forward
            float s = t * L * 6.5;
            float lw = 0.34 * pow(1.0 - t, 0.8) * smoothstep(0.0, 0.12, t) * (0.6 + 0.4 * gj);
            float u = fract(s - bd.x * 1.8) - 0.5;
            float leaf = bd.x - lw * (1.0 - 4.0 * u * u);
            float dl = max(leaf, -0.0) + wob;
            float cl = 1.0 - smoothstep(-aa, aa, dl);
            float rib = 1.0 - smoothstep(-aa, aa, bd.x - 0.022);
            float edgeL = exp(-abs(dl) / 0.025) * cl;
            float sori = 0.0;
            if (lw > 0.05) sori = exp(-pow(bd.x - lw * 0.45, 2.0) / 0.0012 - u * u / 0.01) * step(0.3, fract(s * 0.5 + 0.25));
            e = max(e, cl * (0.16 + 0.45 * edgeL + 0.8 * sori));
            e = max(e, rib * (0.5 + 0.3 * t));
            a = max(a, max(cl * 0.85, rib));
        }
    } else if (kind == 4) {
        // glowing bells: an arching stem, a few small orbs hanging from it, two blades
        float L = 2.1 * g;
        vec2 tip = vec2(0.9 * L / 2.1, L * 0.82);
        vec2 ctrl = vec2(-0.1, L * 1.05);
        vec2 bd = bezDist(p, vec2(0.0), ctrl, tip);
        float cs = 1.0 - smoothstep(-aa, aa, bd.x - 0.02);
        e = max(e, cs * 0.25);
        a = max(a, cs);
        for (int k = 0; k < 5; k++) {
            float fk = float(k);
            float tk = 0.5 + fk * 0.12;
            float gk = smoothstep(0.3 + fk * 0.1, 0.6 + fk * 0.1, grow);
            if (gk <= 0.0) continue;
            vec2 at = mix(mix(vec2(0.0), ctrl, tk), mix(ctrl, tip, tk), tk);
            float hang = 0.12 + 0.06 * h1(seed, fk);
            vec2 bc = at - vec2(0.0, hang);
            float br = (0.075 + 0.035 * h1(seed, fk + 3.0)) * gk;
            float th = 1.0 - smoothstep(-aa, aa, sdSeg(p, at, bc) - 0.008);
            float d = length((p - bc) / vec2(1.0, 1.2)) - br;
            float cb = 1.0 - smoothstep(-aa, aa, d);
            float core = exp(-dot(p - bc, p - bc) / (br * br * 0.4));
            e = max(e, max(th * 0.3, cb * (0.75 + 0.6 * core)));
            a = max(a, max(th, cb));
        }
        for (int b = 0; b < 2; b++) {
            float fb = float(b) * 2.0 - 1.0;
            vec2 bt = vec2(fb * 0.75, 1.0) * (0.4 + 0.6 * g) * (0.8 + 0.3 * h1(seed, 9.0 + float(b)));
            vec2 bd2 = bezDist(p, vec2(0.0), vec2(fb * 0.05, bt.y * 0.8), bt);
            float wdt = 0.07 * (1.0 - bd2.y);
            float dbl = bd2.x - wdt;
            float cbl = 1.0 - smoothstep(-aa, aa, dbl);
            e = max(e, cbl * (0.06 + 0.25 * exp(-abs(dbl) / 0.02)));
            a = max(a, cbl);
        }
    } else {
        // a cushion of moss: a low soft mound glowing from within, a few brighter spore heads on it
        float rx = 0.95 * (0.4 + 0.6 * g), ry = 0.3 * g + 0.02;
        float d = (length(p / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -p.y - 0.04);
        d += (vnoise(vec2(p.x * 6.0, seed * 9.0)) - 0.5) * 0.05;
        float cv = 1.0 - smoothstep(-aa * 2.0, aa * 2.0, d);
        float in_ = clamp(-d / max(ry, 0.01), 0.0, 1.0);
        e = cv * (0.1 + 0.35 * smoothstep(0.0, 0.6, in_) * (0.6 + 0.4 * vnoise(p * 5.0 + seed)));
        a = cv * 0.9;
        for (int k = 0; k < 6; k++) {
            float fk = float(k);
            float sx = (h1(seed, fk) - 0.5) * 1.6 * rx;
            float top = ry * sqrt(max(1.0 - sx * sx / (rx * rx), 0.0));
            float sh = 0.06 + 0.12 * h1(seed, fk + 7.0);
            vec2 hc = vec2(sx, top + sh * g);
            float th = 1.0 - smoothstep(-aa, aa, sdSeg(p, vec2(sx, top - 0.02), hc) - 0.008);
            float hd = length(p - hc) - 0.035 * g;
            float ch = 1.0 - smoothstep(-aa, aa, hd);
            e = max(e, max(th * 0.25, ch * 1.1));
            a = max(a, max(th * 0.8, ch));
        }
    }
    a *= vD.w;
    if (a <= 0.003) discard;
    // the second color creeps into the tips (two-tone life), slow motion pales it under the moon
    if (kind != 5) tint = mix(col, col2, clamp((kind == 2 ? -p.y * 0.5 : p.y) / 2.6, 0.0, 1.0) * 0.6);
    float L = dot(tint, vec3(0.2126, 0.7152, 0.0722));
    tint = mix(tint, uMoon * (L * 1.2 + 0.1), uSlow * 0.45);
    float wave = vC.w;
    vec3 emit = tint * e * glow * (1.0 + 1.4 * wave) + vec3(1.0) * e * e * 0.15 * glow * wave;
    vec3 dark = tint * 0.02;
    vec3 c = dark * a + emit;
    frag = vec4(c * vD.w, a);
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
    float d = sdBox(w - vec2(0.0, 0.1), vec2(5.1, 10.2)) - 0.25;
    float veil = 0.86 * (1.0 - smoothstep(-0.2, 0.8, d));
    vec3 c = uSkyLow * 0.5;
    float a = veil;
    float post = abs(abs(w.x) - 5.2) - 0.012;
    float pa = (1.0 - smoothstep(-uAA, uAA, post)) * step(-10.2, w.y) * smoothstep(10.2, -5.0, w.y);
    float floorL = abs(w.y + 10.2) - 0.012;
    float fa = (1.0 - smoothstep(-uAA, uAA, floorL)) * step(abs(w.x), 5.212);
    float l = max(pa, fa) * uLineA;
    c = mix(c, uLine, l);
    a = max(a, l);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- fireflies and spores
// a: x, y, radius, alpha. b: color, core (0 a soft spore .. 1 a firefly's bright core).
inline const char* MOTE_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vA, vB;
out vec4 frag;
void main() {
    float r2 = dot(vL, vL);
    if (r2 >= 1.0) discard;
    float halo = exp(-r2 * 5.0) - exp(-5.0);
    float core = exp(-r2 * 60.0);
    float a = (halo * 0.55 + core * (0.6 + 1.2 * vB.w)) * vA.w;
    vec3 c = vB.rgb * a + vec3(1.0) * core * vB.w * 0.35 * vA.w;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the blocks (instanced per cell)
// a: x, y, half, kind (0 block, 1 ghost, 2 clearing, 3 going out). b: noise origin x, y, seed, glow.
// c: color, alpha. d: progress, -, -, -.
inline const char* PIECE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 l = aPos * 0.5;
    vLocal = l;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(aA.xy + l * 2.0 * aA.z, 0.0, 1.0);
}
)";

// A block of luminous fungus flesh: a solid little cube with soft rounded corners, lit from within (darker toward its
// sides, like a translucent body), threaded with brighter bioluminescent veins and a few glowing spots, a pale line of
// moonlight along its top edge.
inline const char* PIECE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
uniform float uGlow, uSlow;
uniform vec3 uMoon;
float sdRound(vec2 p, float b, float r) { vec2 q = abs(p) - (b - r); return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
void main() {
    vec2 p = vLocal;
    int kind = int(vA.w + 0.5);
    float aa = max(length(fwidth(p)) * 0.8, 1e-4);
    float d = sdRound(p, 0.445, 0.1);
    vec3 col = vC.rgb;
    float Lc = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(col, uMoon * (Lc * 1.3 + 0.1), uSlow * 0.35);
    if (kind == 1) {
        float l = 1.0 - smoothstep(0.0, aa * 1.5, abs(d + 0.03) - 0.012);
        float a = l * 0.5 * vC.w;
        if (a <= 0.002) discard;
        frag = vec4(col * 0.6 * a, a);
        return;
    }
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.002) discard;
    float inside = max(-d, 0.0);
    vec2 nw = vB.xy + p;
    float seed = vB.z;
    // the flesh: a soft marbling, lit from within
    float marble = fbm(nw * 2.4 + seed * 7.0);
    float body = smoothstep(0.0, 0.26, inside);
    vec3 c = col * (0.30 + 0.22 * marble) * (0.5 + 0.62 * body);
    // veins: fine branching lines, brighter at the heart of the block
    float v1 = abs(fbm3(nw * 1.9 + seed * 3.0) - 0.5);
    float v2 = abs(fbm3(nw * 4.1 + seed * 5.0 + 4.0) - 0.5);
    float vein = exp(-v1 / 0.018) + 0.35 * exp(-v2 / 0.012);
    c += mix(col, vec3(1.0), 0.2) * vein * (0.3 + 0.6 * body) * 0.75;
    // a soft light at the heart of the block, as if lit from inside
    c += col * 0.22 * smoothstep(0.05, 0.4, inside) * (0.7 + 0.6 * marble);
    // a few glowing spots
    vec2 sg = nw * 4.0 + seed * 11.0;
    vec2 sid = floor(sg), sf = fract(sg) - 0.5;
    if (hash12(sid) > 0.72) {
        vec2 o = (vec2(hash12(sid + 1.7), hash12(sid + 5.1)) - 0.5) * 0.4;
        float s = exp(-dot(sf - o, sf - o) / 0.012);
        c += mix(col, vec3(1.0), 0.35) * s * 0.7 * body;
    }
    // the moon along its top edge, the underside in shadow
    float topE = exp(-inside / 0.03) * smoothstep(0.05, 0.4, p.y);
    c += (col * 0.5 + vec3(0.12)) * topE;
    c *= 1.0 - 0.3 * exp(-inside / 0.05) * smoothstep(0.0, -0.4, p.y);
    float a = cov * vC.w;
    float glow = vB.w;
    if (kind == 2) {
        // clearing: the flesh flares, then breaks into spores, the thinnest first
        float t = vD.x;
        float n = fbm3(nw * 3.0 + seed * 3.1) * 0.7 + vnoise(nw * 9.0 + seed) * 0.3;
        a *= smoothstep(t * 1.2 - 0.15, t * 1.2 + 0.02, n) * (1.0 - t * t);
        glow *= 1.0 + 0.8 * t;
    }
    if (kind == 3) {
        // the game is over: the glow goes out, then the block crumbles away
        float t = vD.x;
        float out_ = smoothstep(0.0, 0.45, t);
        c = mix(c, col * 0.08 * (0.6 + 0.6 * marble) + c * 0.06, out_);
        float n = fbm3(nw * 2.4 + seed * 3.1);
        a *= smoothstep((t - 0.45) * 1.9 - 0.1, (t - 0.45) * 1.9 + 0.05, n);
    }
    if (a <= 0.002) discard;
    frag = vec4(c * glow * uGlow * a, a);
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

inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk, uMoon;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: the forest under the moon, its colors cooled
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 moon = uMoon / max(max(uMoon.r, uMoon.g), uMoon.b);
    c = mix(c, mix(vec3(g), c, 0.45) * mix(vec3(1.0), moon, 0.7) * 0.85, uSlow);
    // a soft knee: the glow never burns to white
    c = c / (1.0 + 0.32 * max(max(c.r, c.g), c.b));
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
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
uniform vec2 uRes;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
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

} // namespace forest_shaders

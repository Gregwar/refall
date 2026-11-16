#pragma once
// GLOWING FOREST's shaders (see forest.cpp).
//
// BG_FS paints the night forest: a dark backdrop with a luminous mist lying low, four depths of trunks fading into
// it (the far ones thin and pale with mist, the nearer ones darker, with limbs), banks of mist between the depths,
// shafts of pale light with motes of dust drifting in them, the canopy's leaves and vines hanging over the top, the
// near trunks (ridged bark, buttress roots, moss, veins and dots of glowing fungus that climb them as the forest
// grows), the mossy floor (grass, stones, a large mossy rock in the bottom-left corner, a carpet of glowing moss that
// spreads with the song) and the ground fog. HALO_FS is the soft light a glowing plant casts on the mist around it;
// PLANT_FS the bioluminescent life (mushrooms, mycena, glowworm threads, ferns, bell flowers, moss cushions, bracket
// fungi, glowing reeds: instanced quads, one per plant, each drawn as a small SDF); VEIL_FS the well; MOTE_FS
// fireflies and spores; PIECE_FS the blocks (instanced per cell); DOWN_FS / BLUR_FS the glow and FINAL_FS the frame.
#include <string>

namespace forest_shaders {

inline const char* COMMON = R"(
uniform float uGS;     // the ground's phase (the forest's seed)
uniform vec3 uRock;    // the mossy rock (the bottom-left corner): center, half width, height
uniform vec4 uPool;    // the still pool on the right: x0, x1, its water's level (none when x1 <= x0)
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
float segT(vec2 p, vec2 a, vec2 b, out float h) { vec2 pa = p - a, ba = b - a; h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// The floor's height at x (mirrors groundY in forest.cpp): a gently rolling floor, the mossy rock rising on the left.
float baseY(float x) {
    float ax = abs(x);
    float k = smoothstep(4.5, 8.0, ax);
    return -10.55 + k * (0.22 * sin(0.37 * x + uGS) + 0.12 * sin(1.13 * x + 2.0 * uGS)) + 0.35 * smoothstep(14.0, 21.0, x);
}
float rockTop(float x) {
    float bx = (x - uRock.x) / uRock.y;
    float k = max(1.0 - bx * bx, 0.0);
    return -11.3 + (uRock.z + 0.18 * sin(x * 0.9 + uGS)) * pow(k, 0.55);
}
float poolK(float x) {
    return uPool.y > uPool.x ? smoothstep(uPool.x - 0.7, uPool.x + 0.5, x) * smoothstep(uPool.y + 0.7, uPool.y - 0.5, x) : 0.0;
}
float groundY(float x) {
    float a = baseY(x), b = rockTop(x);
    float h = max(0.3 - abs(a - b), 0.0) / 0.3;
    return mix(max(a, b) + h * h * 0.3 * 0.25, -11.9, poolK(x));
}
// how much of the pool's water is at w (0 none)
float waterAt(vec2 w) {
    if (poolK(w.x) <= 0.0) return 0.0;
    return smoothstep(0.0, 0.45, uPool.z - groundY(w.x)) * (1.0 - smoothstep(-0.012, 0.012, w.y - uPool.z));
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
uniform float uTime, uSeed, uMistK, uLife, uWaveR, uWaveK, uAA, uSlow, uGrow, uWind;
// the HUD's columns, softly: what lies behind them stays quiet
float hudQuiet(vec2 w) {
    float l = smoothstep(-11.9, -11.1, w.x) * (1.0 - smoothstep(-6.1, -5.5, w.x)) * smoothstep(-7.5, -6.6, w.y);
    float r = smoothstep(5.5, 6.1, w.x) * (1.0 - smoothstep(11.1, 11.9, w.x)) * smoothstep(-4.1, -3.3, w.y);
    return max(l, r);
}
// the drop's wave of light where it passes
float waveAt(vec2 w) {
    float dist = max(abs(w.x) - 5.5, 0.0) + 0.25 * max(w.y + 10.0, 0.0);
    return uWaveK * exp(-pow(dist - uWaveR, 2.0) / 3.0);
}
vec3 glowMix(float k) { return mix(uGlowA, uGlowB, k); }
// A depth of trunks in the mist (L 0 the farthest): coverage, across the trunk (-1..1), its id.
vec3 layerTrunk(vec2 w, float L, float sp, float wid0, float soft) {
    float ox = uSeed * 13.0 + L * 7.31;
    float id = floor((w.x + ox) / sp);
    if (hash12(vec2(id, L * 17.0 + 3.0)) < 0.2) return vec3(0.0, 0.0, id);
    float x0 = (id + 0.5 + (hash12(vec2(id, L + 5.0)) - 0.5) * 0.4) * sp - ox;
    float slope = (hash12(vec2(id, L + 9.0)) - 0.5) * 0.035;
    float y = w.y + 11.0;
    float xc = x0 + slope * y + 0.05 * L * sin(y * 0.19 + id * 1.7);
    float wid = wid0 * (0.65 + 0.7 * hash12(vec2(id, L + 11.0))) * (1.0 - 0.011 * y);
    float foot = groundY(w.x) + 1.1 - 0.35 * L;
    wid *= 1.0 + 0.7 * exp(-max(w.y - foot, 0.0) / 0.9);
    float dx = w.x - xc;
    float d = abs(dx) - wid + (vnoise(vec2(w.x * 2.3, w.y * 0.45) + id * 3.1) - 0.5) * 0.25 * wid0;
    if (L > 1.5) {
        // a limb reaching up and out
        float hb = 3.0 + 10.0 * hash12(vec2(id, L + 13.0));
        float s = hash12(vec2(id, L + 15.0)) < 0.5 ? -1.0 : 1.0;
        vec2 a = vec2(x0 + slope * hb + 0.05 * L * sin(hb * 0.19 + id * 1.7), hb - 11.0);
        vec2 b = a + vec2(s * 0.22 * sp, 1.3 + 0.8 * hash12(vec2(id, L + 17.0)));
        float h;
        float dl = segT(w, a, b, h) - wid0 * 0.45 * (1.0 - 0.75 * h);
        d = min(d, dl);
    }
    float cov = 1.0 - smoothstep(-soft, soft, d);
    return vec3(cov, clamp(dx / max(wid, 1e-3), -1.0, 1.0), id);
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float top = uView.y + uView.w;
    float t = smoothstep(uView.y - uView.w, top, w.y);
    float gy = groundY(w.x);
    float hud = hudQuiet(w);
    float lowMist = exp(-max(w.y - gy, 0.0) / 7.0);
    // the mist takes on the life's light as the forest grows
    vec3 mistC = mix(uMist, uMist * 0.7 + glowMix(0.4) * 0.2, 0.18 * uGrow * min(uLife, 1.2));
    // ---- the night: dark above, a luminous mist lying low between the trees
    vec3 c = mix(uSkyLow, uSkyTop, pow(t, 0.75));
    c *= 0.8 + 0.4 * fbm3(w * vec2(0.05, 0.09) + vec2(uSeed, 0.0));
    float hazeN = 0.7 + 0.6 * fbm3(vec2(w.x * 0.06 + uTime * 0.004, w.y * 0.12) + uSeed * 1.7);
    c += mistC * (0.04 + 0.34 * lowMist) * hazeN * uMistK * (1.0 - 0.35 * hud);
    // ---- shafts of pale light slanting down through the canopy
    float shaft = 0.0;
    for (int k = 0; k < 4; k++) {
        float fk = float(k);
        float x0 = (fk - 1.5) * 10.5 + (hash12(vec2(fk, uSeed)) - 0.5) * 4.0;
        float sx = w.x - x0 + (w.y - 12.0) * 0.34;
        float wd = 0.8 + 1.0 * hash12(vec2(fk + 3.0, uSeed));
        float b = exp(-sx * sx / (wd * wd)) * smoothstep(-11.5, 8.0, w.y);
        b *= 0.35 + 0.9 * fbm3(vec2(sx * 0.5, w.y * 0.07 + uTime * 0.008) + fk * 5.0);
        shaft += b;
    }
    shaft *= 1.0 - 0.6 * hud;
    // a faint pale light above the canopy (where the shafts come from)
    c += mix(mistC, vec3(dot(mistC, vec3(0.333))), 0.4) * 0.45 * exp(-(top - w.y) / 2.0) * (0.6 + 0.5 * fbm3(vec2(w.x * 0.2, uSeed))) * uMistK;
    vec3 shaftC = mix(mistC, vec3(dot(mistC, vec3(0.333))), 0.35);
    // ---- four depths of trunks, each farther one paler with mist, a bank of mist drifting in front of each
    for (int L = 0; L < 4; L++) {
        float fl = float(L);
        float sp = L == 0 ? 1.15 : L == 1 ? 1.9 : L == 2 ? 3.0 : 4.6;
        float wd = L == 0 ? 0.05 : L == 1 ? 0.1 : L == 2 ? 0.18 : 0.3;
        float fog = L == 0 ? 0.78 : L == 1 ? 0.55 : L == 2 ? 0.33 : 0.16;
        vec3 tr = layerTrunk(w, fl, sp, wd, max(uAA * 1.5, 0.09 - 0.025 * fl));
        if (tr.x > 0.0) {
            float u = tr.y;
            float bark = 0.8 + 0.4 * vnoise(vec2(u * 2.5 + tr.z * 7.0, w.y * (0.3 + 0.3 * fl)));
            vec3 tc = mix(uSkyLow * 0.3 * bark, c, fog);
            // the nearer depths' round catches the mist
            tc += mistC * pow(abs(u), 5.0) * 0.06 * (fl / 3.0) * (0.3 + lowMist) * (1.0 - 0.7 * hud);
            c = mix(c, tc, tr.x);
        }
        // the light's shafts lie in front of the far trunks
        if (L == 1) c += shaftC * shaft * 0.1 * uMistK;
        float m = fbm3(vec2(w.x * 0.07 + uTime * 0.007 * (1.0 + fl), w.y * 0.17) + fl * 9.0 + uSeed);
        float band = exp(-max(w.y - gy - 0.3, 0.0) / (2.2 + 1.2 * fl));
        c += mistC * smoothstep(0.3, 0.85, m) * band * 0.1 * uMistK * (1.0 - 0.4 * hud);
    }
    // motes of dust drifting slowly in the shafts (never behind the HUD's text or the well)
    {
        vec2 dp = w * 2.0 + vec2(uTime * 0.05, -uTime * 0.035);
        vec2 id = floor(dp), f = fract(dp) - 0.5;
        float h = hash12(id + 17.0 + floor(uSeed));
        if (h > 0.84) {
            vec2 o = (vec2(hash12(id + 3.1), hash12(id + 8.7)) - 0.5) * 0.55;
            o += 0.1 * vec2(sin(uTime * 0.3 + h * 40.0), cos(uTime * 0.23 + h * 30.0));
            float r = length(f - o);
            float s = exp(-r * r / 0.006) * (0.4 + 0.6 * hash12(id + 5.5));
            c += shaftC * s * shaft * 0.45 * (1.0 - hud) * smoothstep(5.4, 6.4, abs(w.x)) * uMistK;
        }
    }
    // ---- the canopy: dark leaves over the top of the view, their fringe catching the mist, vines hanging from it
    {
        // the line the foliage hangs from: out of view over the middle, lower toward the sides
        float side = smoothstep(8.0, 16.0, abs(w.x));
        float edge = top + 0.6 - (0.7 + side * (1.0 + 1.2 * fbm3(vec2(w.x * 0.13 + uSeed, 3.0))));
        float d = edge - w.y;
        // round, ragged clumps of leaves hanging from it
        float gx = w.x / 0.9 + uSeed;
        for (int i = -1; i <= 1; i++) {
            float cid = floor(gx) + float(i);
            float hl = hash12(vec2(cid, 81.0));
            float lx = (cid + 0.5 + (hash12(vec2(cid, 83.0)) - 0.5) * 0.6 - uSeed) * 0.9;
            float ls = smoothstep(8.0, 16.0, abs(lx));
            float le = top + 0.6 - (0.7 + ls * (1.0 + 1.2 * fbm3(vec2(lx * 0.13 + uSeed, 3.0))));
            float rad = (0.3 + 0.5 * hl) * (0.45 + 0.55 * ls);
            vec2 lc = vec2(lx, le - rad * (0.1 + 0.9 * hash12(vec2(cid, 85.0))));
            vec2 q = w - lc;
            float ang = atan(q.y, q.x);
            d = min(d, length(q) - rad * (1.0 + 0.1 * sin(ang * 7.0 + hl * 30.0)));
        }
        d += (vnoise(w * 9.0) - 0.5) * 0.07 + (vnoise(w * 23.0) - 0.5) * 0.03;
        // gaps in the leaves where the shafts come through
        for (int k = 0; k < 4; k++) {
            float fk = float(k);
            float x0 = (fk - 1.5) * 10.5 + (hash12(vec2(fk, uSeed)) - 0.5) * 4.0;
            vec2 q = (w - vec2(x0 + 0.3, top - 0.5)) / vec2(1.0, 0.75);
            float rh = 0.6 + 0.5 * hash12(vec2(fk + 3.0, uSeed));
            d = max(d, rh - length(q) + (vnoise(w * 6.0) - 0.5) * 0.25);
        }
        float cov = 1.0 - smoothstep(-uAA * 1.5, uAA * 1.5, d);
        if (cov > 0.0) {
            float inner = max(-d, 0.0);
            vec3 cc = uSkyTop * (0.5 + 0.3 * vnoise(w * 3.0)) + uSkyLow * 0.05;
            cc += mistC * 0.05 * exp(-inner / 0.12) * (1.0 - 0.5 * hud);
            cc += glowMix(0.6) * 0.012 * exp(-inner / 0.2) * uLife * uGrow;
            c = mix(c, cc, cov);
        }
        // vines, out at the sides
        float vx = w.x * 1.1 + uSeed;
        float vid = floor(vx);
        float hv = hash12(vec2(vid, 41.0));
        float cxv = (vid + 0.5 + (hash12(vec2(vid, 43.0)) - 0.5) * 0.5 - uSeed) / 1.1;
        if (hv > 0.35 && abs(cxv) > 12.2 && abs(cxv) < uView.z - 0.3) {
            float len = 1.2 + 6.5 * pow(hash12(vec2(vid, 47.0)), 1.6);
            float y0 = top + 0.3 - (0.7 + smoothstep(8.0, 16.0, abs(cxv)) * (1.0 + 1.2 * fbm3(vec2(cxv * 0.13 + uSeed, 3.0))));
            float yy = y0 - w.y;
            if (yy > -0.2 && yy < len + 0.3) {
                float sway = uWind * 0.05 * sin(uTime * 0.45 + hv * 20.0) * yy * yy / 6.0;
                float xs = cxv + sway + 0.07 * sin(yy * 1.2 + hv * 9.0);
                float dv = abs(w.x - xs) - 0.016 * (1.0 - 0.4 * yy / len);
                float k = floor(yy / 0.3);
                float sd = mod(k, 2.0) * 2.0 - 1.0;
                float yk = (k + 0.5) * 0.3;
                vec2 q = w - vec2(cxv + uWind * 0.05 * sin(uTime * 0.45 + hv * 20.0) * yk * yk / 6.0 + 0.07 * sin(yk * 1.2 + hv * 9.0) + sd * 0.07, y0 - yk);
                float ang = sd * 0.7;
                q = vec2(cos(ang) * q.x - sin(ang) * q.y, sin(ang) * q.x + cos(ang) * q.y);
                float lf = (length(q / vec2(0.09, 0.04)) - 1.0) * 0.04;
                if (yk > len) lf = 9.0;
                float dd = min(dv, lf);
                float vc = (1.0 - smoothstep(-uAA, uAA, dd)) * step(yy, len + 0.1);
                vec3 vcol = uSkyTop * 0.6 + mistC * 0.03;
                c = mix(c, vcol, vc * 0.95);
                // a drop of light at its tip, as the forest grows
                vec2 tq = w - vec2(cxv + uWind * 0.05 * sin(uTime * 0.45 + hv * 20.0) * len * len / 6.0 + 0.07 * sin(len * 1.2 + hv * 9.0), y0 - len - 0.04);
                float tip = exp(-dot(tq, tq) / 0.004) * step(0.6 + 0.35 * (1.0 - uGrow), hash12(vec2(vid, 53.0)));
                c += glowMix(hash12(vec2(vid, 59.0))) * tip * 0.35 * uLife;
            }
        }
    }
    // ---- the near trunks
    for (int i = 0; i < 8; i++) {
        if (i >= uTrunks) break;
        vec4 T = uTrunk[i];
        float fi = float(i);
        float g = groundY(T.x);
        float y = w.y - g;
        float cx = T.x + T.z * y + 0.16 * sin(y * 0.16 + fi * 2.3) - 0.16 * sin(fi * 2.3);
        float hw = T.y * (1.0 - 0.007 * y) * (1.0 + 0.05 * sin(y * 0.7 + fi));
        float dx = w.x - cx;
        if (abs(dx) > hw * 1.8 + 3.2) continue;
        float wob = (fbm3(vec2(w.x * 0.9, w.y * 0.3) + fi * 3.1) - 0.5) * 0.1;
        float d = abs(dx) - hw * (1.0 + 0.45 * T.w * exp(-max(y, 0.0) / 1.2)) + wob;
        // buttress roots spreading over the floor
        float rootK = 0.0;
        if (y < 3.2) {
            for (int r = 0; r < 4; r++) {
                float fr = float(r);
                float s = r < 2 ? -1.0 : 1.0;
                float hr = hash12(vec2(fi, fr + 20.0));
                float reach = hw + (0.7 + 1.5 * hr) * (0.6 + 0.6 * T.w) * (r == 0 || r == 3 ? 1.0 : 0.6);
                // a curve leaving the trunk's side steeply and flattening out along the floor (three segments)
                vec2 a = vec2(T.x + s * hw * 0.45, g + 1.5 + 1.3 * hash12(vec2(fi, fr + 30.0)));
                vec2 cc = vec2(T.x + s * (hw + 0.1 + 0.15 * hr), g + 0.15);
                vec2 b = vec2(T.x + s * reach, g - 0.2);
                vec2 p1 = mix(mix(a, cc, 0.33), mix(cc, b, 0.33), 0.33), p2 = mix(mix(a, cc, 0.66), mix(cc, b, 0.66), 0.66);
                float h1, h2, h3;
                float d1 = segT(w, a, p1, h1) - mix(hw * 0.42, hw * 0.3, h1);
                float d2 = segT(w, p1, p2, h2) - mix(hw * 0.3, hw * 0.16, h2);
                float d3 = segT(w, p2, b, h3) - mix(hw * 0.16, 0.025, h3);
                float dr = min(d1, min(d2, d3)) + wob * 0.4;
                if (dr < d) rootK = 1.0 - smoothstep(-0.05, 0.05, dr - d);
                d = min(d, dr);
            }
        }
        float cov = 1.0 - smoothstep(-uAA * 1.5, uAA * 1.5, d);
        if (cov <= 0.0) continue;
        float u = clamp(dx / hw, -1.0, 1.0);
        float inner = -sign(T.x) * u;
        // the bark: long ridges and cracks following the trunk
        float warp = fbm3(vec2(u * 1.4, y * 0.12) + fi * 5.0);
        float ridge = vnoise(vec2(u * 4.5 + warp * 2.0 + fi * 7.0, y * 0.28)) * 0.6 + vnoise(vec2(u * 11.0 + warp * 3.0, y * 0.9)) * 0.4;
        float crack = smoothstep(0.07, 0.0, abs(vnoise(vec2(u * 6.0 + warp * 2.5, y * 0.2) + fi) - 0.5));
        vec3 bark = uSkyLow * (0.26 + 0.3 * ridge) * (1.0 - 0.55 * crack);
        // a faint light on the bark's plates from the mist all around
        bark += mistC * 0.025 * smoothstep(0.35, 0.8, ridge) * (1.0 - crack) * (0.4 + lowMist);
        // its round: the edges catch the mist, the side facing the well the life's glow at its foot
        float rim = pow(abs(u), 2.5) * (1.0 - 0.6 * rootK);
        bark += mistC * 0.15 * rim * (0.35 + lowMist) * (0.3 + 1.0 * ridge) * (1.0 - 0.6 * crack);
        float footGlow = smoothstep(-0.4, 1.0, inner) * exp(-max(y, 0.0) / 3.5) * uLife * (0.3 + 0.7 * uGrow);
        bark += glowMix(0.3) * 0.03 * footGlow * (0.5 + ridge);
        // moss on the foot, the roots and one side
        float mossM = smoothstep(0.5, 0.75, fbm3(vec2(u * 2.2, y * 0.5) + fi) + 0.2 * u * (hash12(vec2(fi, 2.0)) - 0.5) * 2.0 + 0.25 * rootK)
                    * exp(-max(y, 0.0) / 4.5);
        bark = mix(bark, uMoss * 0.035 * (0.6 + 0.8 * ridge) + uSkyLow * 0.25, mossM * 0.85);
        // veins and dots of glowing fungus, climbing the bark as the forest grows
        float reachY = 2.5 + 15.0 * uGrow;
        // threads climbing the bark: wavy lines along the trunk, broken into lengths, a few finer branches
        float vl = u * 2.2 + 0.9 * fbm3(vec2(u * 1.1 + fi * 2.0, y * 0.3)) + 0.25 * sin(y * 0.9 + fi * 4.0 + u * 3.0);
        float vn = abs(fract(vl) - 0.5) / 2.2;
        float vb = abs(fract(u * 4.5 + 1.4 * fbm3(vec2(u * 2.0, y * 0.8) + fi * 7.0)) - 0.5) / 4.5;
        float seg = smoothstep(0.4, 0.6, vnoise(vec2(floor(vl) * 3.1 + fi, y * 0.35)));
        float segB = smoothstep(0.7, 0.82, vnoise(vec2(u * 3.0 + fi, y * 0.7)));
        float vein = (exp(-vn / 0.01) * seg + 0.4 * exp(-vb / 0.006) * segB) * smoothstep(reachY, reachY - 3.0, y)
                   * smoothstep(0.98, 0.5, abs(u)) * (1.0 - rootK * 0.5);
        vec2 lg = vec2(u * 3.0, y * 1.7) + fi * 13.0;
        vec2 lid = floor(lg), lf = fract(lg) - 0.5;
        float dots = hash12(lid) > 0.93 - 0.08 * uGrow ? exp(-dot(lf, lf) / 0.025) * step(y, reachY + 1.0) * (0.3 + mossM) : 0.0;
        float tw = waveAt(vec2(T.x, w.y));
        bark += glowMix(hash12(vec2(fi, 3.0)) * 0.6 + 0.2 * u) * (vein * 0.1 + dots * 0.14) * uLife * (0.5 + 0.5 * uGrow) * (1.0 + 2.5 * tw);
        c = mix(c, bark, cov);
    }
    // ---- the floor: dark moss, ragged against the mist, a fringe of grass along its top
    float rockK = smoothstep(0.0, 0.25, rockTop(w.x) - baseY(w.x));
    float edge = gy + ((fbm3(vec2(w.x * 1.1, 0.0) + uSeed) - 0.5) * 0.3 + (vnoise(vec2(w.x * 6.0, 1.0)) - 0.5) * 0.06) * (1.0 - 0.8 * rockK);
    float gb = w.x / 0.085;
    float gid = floor(gb), gf = fract(gb);
    float bh = (0.04 + 0.3 * pow(hash12(vec2(gid, 5.0)), 3.0)) * smoothstep(5.6, 7.0, abs(w.x)) * (1.0 - 0.7 * rockK);
    float dF = w.y - edge - bh * max(1.0 - abs(gf - 0.5 + (w.y - edge) * 0.6 * uWind * 0.3) * 2.2, 0.0);
    float floorCov = 1.0 - smoothstep(-uAA * 1.5, uAA * 1.5, dF);
    if (floorCov > 0.0) {
        float depth = max(edge - w.y, 0.0);
        vec3 fc = uSkyLow * 0.36 + uMoss * (0.008 + 0.012 * fbm3(w * 1.3)) * exp(-depth / 0.8);
        fc += mistC * 0.05 * exp(-depth / 0.1);
        float near = smoothstep(5.0, 6.6, abs(w.x));
        // the rock: smooth dark stone with broad facets, moss over its top; its face stays quiet (the HUD's corner)
        if (rockK > 0.0) {
            float facet = fbm3(w * vec2(0.9, 1.2) + 4.0);
            vec3 rc = uSkyLow * (0.28 + 0.14 * facet) + mistC * 0.04 * exp(-depth / 0.25) + mistC * 0.06 * exp(-depth / 0.05);
            float mossTop = smoothstep(0.5, 0.15, depth + (vnoise(vec2(w.x * 2.0, 3.0)) - 0.5) * 0.3);
            rc = mix(rc, uSkyLow * 0.3 + uMoss * 0.03, mossTop * 0.8);
            fc = mix(fc, rc, rockK);
            near *= mix(1.0, smoothstep(0.55, 0.2, depth), rockK);
        }
        // the carpet of glowing moss: soft patches and specks, spreading over the floor as the forest grows
        float pn = fbm3(vec2(w.x * 0.3, 0.0) + uSeed * 2.0);
        float patch = smoothstep(1.0 - uGrow * 0.85, 1.25 - uGrow * 0.85, pn) * near;
        float wv = waveAt(w);
        fc += glowMix(pn) * patch * exp(-depth / 0.22) * 0.05 * uLife * (1.0 + 2.0 * wv);
        vec2 g = w * 5.0;
        vec2 id = floor(g), f = fract(g) - 0.5;
        float h = hash12(id + uSeed * 3.0);
        if (h > 0.83 - 0.1 * uGrow - 0.08 * patch && depth < 1.4) {
            vec2 o = (vec2(hash12(id + 1.3), hash12(id + 7.7)) - 0.5) * 0.6;
            float r = length(f - o);
            float s = exp(-r * r / 0.012) * exp(-depth / 0.5);
            float live = (0.2 + 0.25 * uGrow) * uLife + 2.5 * wv;
            fc += glowMix(hash12(id + 2.0)) * s * 0.09 * live * near;
        }
        c = mix(c, fc, floorCov);
    }
    // ---- the pool: dark still water, a faint sheen of the mist along its surface
    {
        float wa = waterAt(w);
        if (wa > 0.0) {
            float dpt = uPool.z - w.y;
            vec3 wc = uSkyLow * 0.22 + mistC * 0.05 * exp(-dpt / 0.06);
            c = mix(c, wc, wa);
        }
        // glowing pads afloat on it, more as the forest grows
        if (poolK(w.x) > 0.5 && abs(w.y - uPool.z) < 0.12) {
            float px = w.x / 0.75 + uSeed;
            float pid = floor(px);
            float hp = hash12(vec2(pid, 91.0));
            if (hp < 0.25 + 0.65 * uGrow) {
                float pcx = (pid + 0.5 + (hash12(vec2(pid, 93.0)) - 0.5) * 0.4 - uSeed) * 0.75;
                if (poolK(pcx - 0.3) > 0.6 && poolK(pcx + 0.3) > 0.6) {
                    vec2 q = w - vec2(pcx + 0.04 * sin(uTime * 0.3 + hp * 20.0), uPool.z - 0.005);
                    float rx = 0.16 + 0.14 * hash12(vec2(pid, 95.0));
                    float dp = (length(q / vec2(rx, 0.035)) - 1.0) * 0.035;
                    float pc = 1.0 - smoothstep(-uAA, uAA, dp);
                    float rim = exp(-max(-dp, 0.0) / 0.012);
                    vec3 pcol = glowMix(hash12(vec2(pid, 97.0)));
                    c = mix(c, uSkyLow * 0.3 + pcol * (0.05 + 0.25 * rim) * uLife * (1.0 + 2.0 * waveAt(w)), pc);
                    c += pcol * exp(-dot(q, q) / 0.03) * 0.03 * uLife;
                }
            }
        }
    }
    // ---- stones on the floor, moss and glowing specks over their tops
    {
        float sx = w.x / 2.6 + uSeed * 0.37;
        float base = floor(sx - 0.5);
        for (int k = 0; k < 2; k++) {
            float id = base + float(k);
            float hs = hash12(vec2(id, 61.0));
            if (hs < 0.5) continue;
            float scx = (id + 0.5 + (hash12(vec2(id, 63.0)) - 0.5) * 0.5 - uSeed * 0.37) * 2.6;
            if (abs(scx) < 6.6 || abs(scx) > uView.z - 0.4 || rockTop(scx) > baseY(scx) - 0.1 || poolK(scx) > 0.0) continue;
            float rx = 0.22 + 0.4 * hash12(vec2(id, 65.0)), ry = rx * (0.5 + 0.25 * hash12(vec2(id, 67.0)));
            vec2 q = w - vec2(scx, groundY(scx) - ry * 0.3);
            float ds = (length(q / vec2(rx, ry)) - 1.0) * ry + (vnoise(q * 6.0 + id) - 0.5) * 0.04;
            ds = max(ds, -(q.y + ry * 0.2));
            float sc = 1.0 - smoothstep(-uAA * 1.2, uAA * 1.2, ds);
            if (sc <= 0.0) continue;
            float up = clamp(q.y / ry, -1.0, 1.0);
            vec3 st = uSkyLow * (0.3 + 0.12 * vnoise(q * 4.0 + id));
            st += mistC * 0.07 * exp(-max(-ds, 0.0) / 0.05) * smoothstep(0.0, 0.8, up);
            float mossM = smoothstep(0.2, 0.6, up + (vnoise(vec2(q.x * 5.0, id)) - 0.5) * 0.6);
            st = mix(st, uSkyLow * 0.3 + uMoss * 0.03, mossM * 0.8);
            vec2 sg = q * 9.0 + id * 7.0;
            vec2 sid = floor(sg), sf = fract(sg) - 0.5;
            float sp = hash12(sid) > 0.86 - 0.2 * uGrow ? exp(-dot(sf, sf) / 0.03) : 0.0;
            st += glowMix(hash12(vec2(id, 69.0))) * sp * mossM * 0.12 * uLife * (0.3 + uGrow) * (1.0 + 2.0 * waveAt(w));
            c = mix(c, st, sc);
        }
    }
    // ---- ground fog over everything, low and slow
    float fog = fbm(vec2(w.x * 0.09 - uTime * 0.012, w.y * 0.3) + uSeed * 2.0);
    float fb = exp(-max(w.y - gy, 0.0) / 1.6) * (1.0 - 0.5 * floorCov);
    c += mix(mistC, uGlowA * 0.4, 0.2 * uLife) * smoothstep(0.3, 0.9, fog) * fb * 0.11 * uMistK;
    // the drop's wave of light lights the fog it passes through, low over the floor
    float wv = uWaveK * exp(-pow(abs(w.x) - 5.5 - uWaveR + 0.25 * max(w.y + 10.0, 0.0), 2.0) / 3.0) * smoothstep(5.0, 6.5, abs(w.x));
    c += glowMix(0.3) * wv * exp(-max(w.y - gy, 0.0) / 2.5) * (0.4 + 0.6 * fog) * 0.012;
    // a fine painted grain
    c *= 0.93 + 0.14 * fbm3(gl_FragCoord.xy * 0.08 + uSeed);
    c *= 1.0 + (hash12(floor(gl_FragCoord.xy) + fract(uSeed) * 91.0) - 0.5) * 0.06;
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
    vec2 lo = vec2(-1.7, -0.3), hi = vec2(1.7, 3.1);
    if (kind == 2) { lo = vec2(-1.0, -5.6); hi = vec2(1.0, 0.2); }
    if (kind == 5) { lo = vec2(-1.3, -0.3); hi = vec2(1.3, 0.8); }
    if (kind == 6) { lo = vec2(-0.25, -0.9); hi = vec2(0.8, 0.9); }
    if (kind == 7) { lo = vec2(-1.3, -0.2); hi = vec2(1.3, 2.6); }
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
uniform float uSlow, uTime, uAAp, uWind;
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
vec2 bez(vec2 a, vec2 b, vec2 c, float t) { return mix(mix(a, b, t), mix(b, c, t), t); }
// layering: a shape in front covers what is behind it (its own light replaces the light behind)
void lay(inout float e, inout float a, float cov, float em) { e = mix(e, em, cov); a = max(a, cov); }
// a thin glowing thing: its light adds up
void lit(inout float e, inout float a, float cov, float em) { e = max(e, em * cov); a = max(a, cov); }
// a coil (a fern's fiddlehead): distance to an opening spiral of `turns` around c, radius R
float coil(vec2 p, vec2 c, float R, float turns, float ang0) {
    vec2 q = p - c;
    float r = length(q);
    if (r > R * 1.2) return r - R;
    float a = atan(q.y, q.x) - ang0;
    float b = R / (6.2832 * turns);
    float s = (r / b - a) / 6.2832;
    float k = clamp(floor(s + 0.5), 0.0, turns);
    float rr = b * (a + 6.2832 * k);
    if (rr < 0.0) rr += b * 6.2832;
    return abs(r - rr);
}
// One mushroom: a stem from `base` to `tp`, a cap of the given form (0 dome, 1 conical bell, 2 flat parasol) of
// radius cr. Its emission and coverage are laid over what is behind it.
void mushroom(vec2 p, vec2 base, vec2 tp, float cr, float form, float seed, float aa, float wob, inout float e, inout float a) {
    float hs;
    float ds = segT(p, base, tp, hs);
    float sw = (0.035 + 0.06 * cr) * (1.0 - 0.3 * hs) + 0.05 * cr * exp(-hs * 9.0);
    float cs = 1.0 - smoothstep(-aa, aa, ds - sw + wob);
    lay(e, a, cs * 0.95, (0.08 + 0.4 * hs * hs) * (0.8 + 0.35 * vnoise(vec2(p.x * 50.0, p.y * 5.0 + seed))));
    // a ring under the large caps
    if (cr > 0.3 && form > 0.5) {
        vec2 rq = p - mix(base, tp, 0.82);
        float dr = (length(rq / vec2(sw * 2.4, 0.035)) - 1.0) * 0.035;
        lay(e, a, (1.0 - smoothstep(-aa, aa, dr)) * 0.9, 0.35);
    }
    vec2 q = p - tp;
    float d, ry, rx;
    if (form < 0.5) {
        rx = cr; ry = cr * 0.62;
        d = (length(q / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -(q.y + 0.1 * ry - 0.4 * ry * (q.x / rx) * (q.x / rx)));
    } else if (form < 1.5) {
        rx = cr * 0.8; ry = cr * 1.1;
        float wq = rx * (1.0 - 0.7 * clamp(q.y / ry, 0.0, 1.0)) * (1.0 + 0.25 * smoothstep(0.0, -0.2 * ry, q.y));
        d = (length(vec2(q.x / max(wq, 1e-3), q.y / ry)) - 1.0) * min(wq, ry) * 0.8;
        d = max(d, -(q.y + 0.18 * ry));
    } else {
        rx = cr * 1.3; ry = cr * 0.3;
        d = (length(q / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -(q.y + 0.08 * cr - 0.25 * ry * (q.x / rx) * (q.x / rx)));
        d = min(d, length(q - vec2(0.0, ry * 0.7)) - cr * 0.16);
    }
    // gills under the wide caps: fine lines converging on the stem, brighter toward the rim
    if (form != 1.0) {
        float y0 = -0.08 * ry, gh = 0.22 * cr;
        float yy = y0 - q.y;
        if (yy > 0.0 && yy < gh) {
            float hw = rx * (0.98 - 0.55 * yy / gh);
            float gc = (1.0 - smoothstep(-aa, aa, abs(q.x) - hw)) * (1.0 - smoothstep(gh - aa * 2.0, gh, yy));
            float ang = atan(q.x, q.y + gh * 3.0 + 0.0001);
            float lines = 0.5 + 0.5 * cos(ang * (22.0 + 10.0 * h1(seed, 4.0)) * 6.2832 / 3.1416 * 0.5);
            lay(e, a, gc, (0.3 + 0.55 * lines * (abs(q.x) / max(hw, 1e-3))) * (1.0 - 0.5 * yy / gh));
        }
    }
    d += wob;
    float cc = 1.0 - smoothstep(-aa, aa, d);
    if (cc > 0.0) {
        float in_ = clamp(-d / (ry * 0.5), 0.0, 1.0);
        // translucent: brightest at the rim, denser toward the crown; a bright line along the gills' edge
        float rimL = exp(-max(q.y + 0.08 * ry, 0.0) / (ry * 0.3));
        float fib = vnoise(vec2(atan(q.x, q.y + ry) * 9.0, length(q) * 6.0) + seed);
        float body = 0.42 + 0.38 * (1.0 - in_) + 0.12 * fib;
        float spots = 0.0;
        if (form < 0.5) {
            vec2 sg = q / cr * 3.4 + seed * 13.0;
            vec2 sid = floor(sg), sf = fract(sg) - 0.5;
            if (hash12(sid) > 0.55) spots = exp(-dot(sf, sf) / 0.025);
        }
        lay(e, a, cc, body + 0.5 * rimL + 0.45 * spots);
    }
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
    // the wind: a slow sway, more at the top
    if (kind != 6 && kind != 2) {
        float s = uWind * (0.6 * sin(uTime * 0.7 + seed * 2.0) + 0.4 * sin(uTime * 1.23 + seed * 5.0));
        p.x -= s * 0.03 * p.y * p.y / (kind == 0 ? 2.5 : 1.0);
    }
    // a painted edge: a little wobble
    float wob = (vnoise(p * 9.0 + seed * 31.0) - 0.5) * 0.02;
    if (kind == 0) {
        // a cluster of mushrooms of one form, the small ones behind, the largest in front; pins at their feet
        float form = floor(h1(seed, 20.0) * 2.999);
        int n = 3 + int(h1(seed, 21.0) * 2.999);
        for (int j = 0; j < 3; j++) {
            float fj = float(j);
            float gj = smoothstep(0.1 + fj * 0.1, 0.5 + fj * 0.1, grow);
            if (gj <= 0.0) continue;
            vec2 pc = vec2((h1(seed, fj + 30.0) - 0.5) * 1.8, 0.0);
            float pr = (0.04 + 0.04 * h1(seed, fj + 33.0)) * gj;
            mushroom(p, pc, pc + vec2(0.0, 0.1 + 0.1 * h1(seed, fj + 36.0)) * gj, pr, 0.0, seed + fj, aa, 0.0, e, a);
        }
        for (int i = 0; i < 5; i++) {
            if (i >= n) break;
            float fi = float(i);
            bool main_ = i == n - 1;
            float gi = smoothstep(fi * 0.12, 0.55 + fi * 0.12, grow);
            if (gi <= 0.0) continue;
            float x0 = main_ ? (h1(seed, 2.0) - 0.5) * 0.2 : (h1(seed, fi) - 0.5) * 1.5;
            float sz = main_ ? 1.0 : 0.45 + 0.35 * h1(seed, fi + 3.0);
            float hh = (form > 1.5 ? 1.35 : form > 0.5 ? 1.15 : 1.05) * sz * gi * (0.9 + 0.2 * h1(seed, fi + 9.0));
            float cr = (form > 1.5 ? 0.36 : form > 0.5 ? 0.28 : 0.42) * sz * (0.4 + 0.6 * gi);
            float lean = (h1(seed, fi + 7.0) - 0.5) * 0.3 + x0 * 0.18;
            mushroom(p, vec2(x0, 0.0), vec2(x0 + lean, hh), cr, form, seed + fi * 3.0, aa, wob, e, a);
        }
    } else if (kind == 1) {
        // mycena: slender threads with small glowing bells, a fine gill line inside each
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.1, 0.5 + fi * 0.1, grow);
            if (gi <= 0.0) continue;
            float x0 = (fi - 2.5) * 0.22 + (h1(seed, fi) - 0.5) * 0.18;
            float hh = (0.6 + 1.35 * h1(seed, fi + 2.0)) * gi;
            float bend = (h1(seed, fi + 4.0) - 0.5) * 0.6;
            vec2 tip = vec2(x0 + bend, hh);
            vec2 bd = bezDist(p, vec2(x0, 0.0), vec2(x0, hh * 0.6), tip);
            float cs = 1.0 - smoothstep(-aa, aa, bd.x - 0.016);
            lit(e, a, cs * 0.8, 0.12 + 0.3 * bd.y);
            float br = (0.1 + 0.07 * h1(seed, fi + 6.0)) * (0.3 + 0.7 * gi);
            vec2 q = p - tip - vec2(0.0, -br * 0.15);
            float wq = br * (1.0 - 0.45 * clamp(q.y / br, 0.0, 1.0)) * (1.0 + 0.3 * smoothstep(0.0, -0.3 * br, q.y));
            float cb = 1.0 - smoothstep(-aa, aa, (length(vec2(q.x / wq, q.y / (br * 1.15))) - 1.0) * br);
            cb *= smoothstep(-br * 0.42, -br * 0.3, q.y);
            float rimL = exp(-abs(q.y + br * 0.32) / (br * 0.22));
            float ribs = 0.85 + 0.15 * cos(q.x / wq * 9.0);
            lay(e, a, cb, (0.55 + 0.55 * rimL) * ribs);
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
            float sway = (0.04 + 0.04 * uWind) * sin(uTime * 0.4 + seed + fi * 1.7);
            float yy = clamp(p.y, -len, 0.0);
            float xs = x0 + sway * (-yy / 3.0);
            float dl = abs(p.x - xs) - 0.007;
            float th = (1.0 - smoothstep(-aa, aa, dl)) * step(-len, p.y) * step(p.y, 0.0);
            lit(e, a, th * 0.3, 1.0 * (0.3 + 0.7 * (-p.y / max(len, 0.1))));
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
                lit(e, a, cb, 0.5 + 0.5 * tipK + 0.5 * core);
            }
            vec2 qt = p - vec2(x0 + sway * (len / 3.0), -len);
            float bt = 0.06 * gi;
            float ct = 1.0 - smoothstep(-aa, aa, length(qt) - bt);
            lit(e, a, ct, 1.0 + 0.6 * exp(-dot(qt, qt) / (bt * bt * 0.4)));
        }
    } else if (kind == 3) {
        // a fern: fronds in layers (the back ones dimmer), each arching from the crown with a glowing midrib and
        // leaflets lit along their edges; growing, each frond unrolls from a fiddlehead coil; a young coil at the crown
        for (int j = 0; j < 5; j++) {
            float fj = float(j);
            float gj = smoothstep(fj * 0.1, 0.6 + fj * 0.1, grow);
            if (gj <= 0.0) continue;
            float side = mod(fj, 2.0) * 2.0 - 1.0;
            float ang = j == 4 ? (h1(seed, 4.0) - 0.5) * 0.3 : side * (j < 2 ? 1.05 : 0.55) + (h1(seed, fj) - 0.5) * 0.25;
            float dim = j < 2 ? 0.55 : j < 4 ? 0.8 : 1.0;
            float L = (j == 4 ? 2.5 : j < 2 ? 1.6 : 2.1) * (0.9 + 0.2 * h1(seed, fj + 2.0));
            vec2 tip = vec2(sin(ang) * L * 0.95, cos(ang) * L * 0.72);
            vec2 ctrl = vec2(sin(ang) * L * 0.2, L * 0.95);
            // the unrolled length; the rest is still a coil at its end
            float tm = 0.2 + 0.8 * smoothstep(0.0, 0.92, gj);
            vec2 a0 = vec2(0.0), c1 = mix(a0, ctrl, tm), e1 = bez(a0, ctrl, tip, tm);
            vec2 bd = bezDist(p, a0, c1, e1);
            float t = bd.y * tm;
            float s = t * L * 6.5;
            float lw = 0.34 * pow(1.0 - t, 0.8) * smoothstep(0.0, 0.12, t) * smoothstep(tm, tm - 0.08, t) * (0.6 + 0.4 * gj);
            float u = fract(s - bd.x * 1.8) - 0.5;
            float dl = bd.x - lw * (1.0 - 4.0 * u * u) + wob;
            float cl = 1.0 - smoothstep(-aa, aa, dl);
            float rib = 1.0 - smoothstep(-aa, aa, bd.x - 0.022 * (1.0 - 0.5 * t));
            float edgeL = exp(-abs(dl) / 0.025) * cl;
            float vein = exp(-abs(u) / 0.06) * cl * 0.25;
            float sori = 0.0;
            if (lw > 0.05) sori = exp(-pow(bd.x - lw * 0.45, 2.0) / 0.0012 - u * u / 0.01) * step(0.3, fract(s * 0.5 + 0.25));
            lay(e, a, max(cl * 0.9, rib), dim * max(rib * (0.5 + 0.3 * t), 0.12 + 0.45 * edgeL + vein + 0.8 * sori));
            if (tm < 0.99) {
                vec2 dir = normalize(e1 - bez(a0, ctrl, tip, tm - 0.02));
                vec2 nrm = vec2(-dir.y, dir.x) * -side;
                float R = (0.1 + 0.22 * (1.0 - tm)) * (0.8 + 0.2 * gj);
                vec2 cc = e1 + nrm * R;
                float dc = coil(p, cc, R, 1.5, atan(-nrm.y, -nrm.x)) - 0.03;
                lit(e, a, 1.0 - smoothstep(-aa, aa, dc), dim * 0.75);
            }
        }
        // the young fiddlehead at the crown
        float gy0 = smoothstep(0.5, 1.0, grow);
        if (gy0 > 0.0) {
            float sx = (h1(seed, 11.0) - 0.5) * 0.5;
            vec2 b0 = vec2(sx, 0.0), b1 = vec2(sx + 0.05, 0.55 * gy0);
            float ds = sdSeg(p, b0, b1) - 0.018;
            lit(e, a, 1.0 - smoothstep(-aa, aa, ds), 0.4);
            float dc = coil(p, b1 + vec2(0.09, 0.0), 0.1, 1.6, 3.1416) - 0.022;
            lit(e, a, 1.0 - smoothstep(-aa, aa, dc), 0.8);
        }
    } else if (kind == 4) {
        // bell flowers: an arching stem with flared bells hanging from it, a bright pistil at each mouth; two blades
        for (int b = 0; b < 2; b++) {
            float fb = float(b) * 2.0 - 1.0;
            vec2 bt = vec2(fb * 0.75, 1.0) * (0.4 + 0.6 * g) * (0.8 + 0.3 * h1(seed, 9.0 + float(b)));
            vec2 bd2 = bezDist(p, vec2(0.0), vec2(fb * 0.05, bt.y * 0.8), bt);
            float dbl = bd2.x - 0.07 * (1.0 - bd2.y);
            lay(e, a, 1.0 - smoothstep(-aa, aa, dbl), 0.05 + 0.2 * exp(-abs(dbl) / 0.02));
        }
        float L = 2.1 * g;
        vec2 tip = vec2(0.9 * L / 2.1, L * 0.82);
        vec2 ctrl = vec2(-0.1, L * 1.05);
        vec2 bd = bezDist(p, vec2(0.0), ctrl, tip);
        lit(e, a, 1.0 - smoothstep(-aa, aa, bd.x - 0.02), 0.25);
        for (int k = 0; k < 6; k++) {
            float fk = float(k);
            float tk = 0.42 + fk * 0.11;
            float gk = smoothstep(0.3 + fk * 0.09, 0.6 + fk * 0.09, grow);
            if (gk <= 0.0) continue;
            vec2 at = bez(vec2(0.0), ctrl, tip, tk);
            float hang = 0.1 + 0.07 * h1(seed, fk);
            vec2 bc = at - vec2(0.0, hang);
            float br = (0.08 + 0.04 * h1(seed, fk + 3.0)) * gk * (1.0 - 0.25 * fk / 5.0);
            lit(e, a, 1.0 - smoothstep(-aa, aa, sdSeg(p, at, bc) - 0.008), 0.3);
            vec2 q = p - bc;
            // the bell: a dome flaring out to a scalloped mouth
            float flare = smoothstep(0.0, -1.3 * br, q.y);
            float wq = br * (0.62 + 0.5 * flare);
            float mouth = -1.25 * br + 0.06 * br * cos(q.x / br * 9.0);
            float db = max((length(vec2(q.x / wq, max(q.y, 0.0) / br)) - 1.0) * br * 0.7, abs(q.x) - wq);
            db = max(db, mouth - q.y);
            float cb = 1.0 - smoothstep(-aa, aa, db);
            // the rim's light peaks at the mouth and fades up the bell; below the mouth (the edge's antialiasing,
            // which is wide around a bell just budding) it stays at the rim's, not growing past it
            float rimL = exp(-max(q.y - mouth, 0.0) / (br * 0.25));
            lay(e, a, cb, 0.5 + 0.4 * flare + 0.5 * rimL);
            vec2 pq = q - vec2(0.0, mouth - br * 0.25);
            float pc = 1.0 - smoothstep(-aa, aa, length(pq) - br * 0.18);
            lit(e, a, pc, 1.4);
        }
    } else if (kind == 5) {
        // a cushion of moss: a low soft mound glowing from within, brighter spore heads on it
        float rx = 0.95 * (0.4 + 0.6 * g), ry = 0.3 * g + 0.02;
        float d = (length(p / vec2(rx, ry)) - 1.0) * ry;
        d = max(d, -p.y - 0.04);
        d += (vnoise(vec2(p.x * 6.0, seed * 9.0)) - 0.5) * 0.05 + (vnoise(vec2(p.x * 22.0, seed)) - 0.5) * 0.02;
        float cv = 1.0 - smoothstep(-aa * 2.0, aa * 2.0, d);
        float in_ = clamp(-d / max(ry, 0.01), 0.0, 1.0);
        e = cv * (0.1 + 0.35 * smoothstep(0.0, 0.6, in_) * (0.6 + 0.4 * vnoise(p * 5.0 + seed)));
        a = cv * 0.9;
        for (int k = 0; k < 8; k++) {
            float fk = float(k);
            float sx = (h1(seed, fk) - 0.5) * 1.6 * rx;
            float top = ry * sqrt(max(1.0 - sx * sx / (rx * rx), 0.0));
            float sh = 0.06 + 0.14 * h1(seed, fk + 7.0);
            vec2 hc = vec2(sx + 0.03 * (h1(seed, fk + 3.0) - 0.5), top + sh * g);
            float th = 1.0 - smoothstep(-aa, aa, sdSeg(p, vec2(sx, top - 0.02), hc) - 0.007);
            float ch = 1.0 - smoothstep(-aa, aa, length(p - hc) - 0.03 * g);
            lit(e, a, th * 0.8, 0.3);
            lit(e, a, ch, 1.1);
        }
    } else if (kind == 6) {
        // bracket fungi: shelves stacked up the bark, each a half disc with growth rings, its rim glowing brightest
        for (int i = 0; i < 4; i++) {
            float fi = float(i);
            if (fi > 0.6 + 2.2 * h1(seed, 9.0)) break;
            float gi = smoothstep(fi * 0.15, 0.5 + fi * 0.15, grow);
            if (gi <= 0.0) continue;
            float y0 = -0.55 + fi * (0.36 + 0.24 * h1(seed, 12.0)) + (h1(seed, fi) - 0.5) * 0.16;
            float rx = (0.3 + 0.32 * h1(seed, fi + 2.0)) * (1.0 - 0.15 * fi) * (0.3 + 0.7 * gi);
            float ry = rx * 0.48;
            vec2 q = p - vec2(-0.08 + 0.06 * h1(seed, fi + 5.0), y0);
            float xr = clamp(q.x / rx, 0.0, 1.0);
            // a dome on top, a gently curved underside lifting toward the rim
            float under = q.y + ry * (0.3 - 0.35 * xr * xr);
            float d = (length(q / vec2(rx, ry)) - 1.0) * ry * 0.8;
            d = max(d, -q.x);
            d = max(d, -under);
            d += wob * 0.6;
            float cv = 1.0 - smoothstep(-aa, aa, d);
            float rr = length(q / vec2(rx, ry));
            float rings = smoothstep(0.55, 0.9, 0.5 + 0.5 * cos(rr * 22.0 + seed));
            float rimL = exp(-max(-d, 0.0) / (ry * 0.12)) * smoothstep(0.3, 0.8, xr);
            float pores = exp(-abs(under) / (ry * 0.1)) * (0.6 + 0.4 * cos(q.x * 90.0));
            lay(e, a, cv, 0.07 + 0.12 * rings * rr + 0.8 * rimL + 0.45 * pores);
        }
    } else {
        // glowing reeds: a tuft of slender blades, some tipped with a glowing seed head
        for (int i = 0; i < 7; i++) {
            float fi = float(i);
            float gi = smoothstep(fi * 0.07, 0.5 + fi * 0.07, grow);
            if (gi <= 0.0) continue;
            float x0 = (h1(seed, fi) - 0.5) * 0.45;
            float hh = (0.8 + 1.3 * h1(seed, fi + 2.0)) * gi;
            float bend = (h1(seed, fi + 4.0) - 0.5) * 0.9 + x0 * 1.2;
            vec2 tip = vec2(x0 + bend, hh);
            vec2 bd = bezDist(p, vec2(x0, 0.0), vec2(x0 + bend * 0.1, hh * 0.7), tip);
            float cb = 1.0 - smoothstep(-aa, aa, bd.x - (0.026 * (1.0 - bd.y) + 0.004));
            lay(e, a, cb * 0.9, 0.05 + 0.35 * bd.y * bd.y);
            if (h1(seed, fi + 6.0) > 0.35) {
                vec2 dir = normalize(tip - bez(vec2(x0, 0.0), vec2(x0 + bend * 0.1, hh * 0.7), tip, 0.9));
                vec2 q = p - tip - dir * 0.08 * gi;
                q = vec2(dot(q, vec2(dir.y, -dir.x)), dot(q, dir));
                float sh = (length(q / vec2(0.028, 0.1)) - 1.0) * 0.028;
                float ch = 1.0 - smoothstep(-aa, aa, sh);
                lit(e, a, ch * gi, 1.0 + 0.5 * exp(-dot(q, q) / 0.002));
            }
        }
    }
    a *= vD.w;
    if (a <= 0.003) discard;
    // the second color creeps into the tips (two-tone life), slow motion pales it under the moon
    if (kind != 5) tint = mix(col, col2, clamp((kind == 2 ? -p.y * 0.5 : kind == 6 ? p.x * 1.2 : p.y) / 2.6, 0.0, 1.0) * 0.6);
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
uniform float uBlockCut;   // how much of the blocks' light is kept out of the glow
void main() {
    // the blocks (the scene's alpha) feed the glow only faintly: they stay crisp, the life around them glows
    vec4 s0 = texture(uSrc, vUv + uTexel * vec2(-1, -1)), s1 = texture(uSrc, vUv + uTexel * vec2(1, -1));
    vec4 s2 = texture(uSrc, vUv + uTexel * vec2(-1, 1)), s3 = texture(uSrc, vUv + uTexel * vec2(1, 1));
    vec3 c = s0.rgb * (1.0 - uBlockCut * s0.a) + s1.rgb * (1.0 - uBlockCut * s1.a) +
             s2.rgb * (1.0 - uBlockCut * s2.a) + s3.rgb * (1.0 - uBlockCut * s3.a);
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
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uTime;
uniform vec3 uBridgeCol, uBridgeInk, uMoon;
uniform vec4 uView;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // the pool mirrors the glow on its bank, a little stretched, broken by slow ripples
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float wa = waterAt(w);
    if (wa > 0.0) {
        float dpt = uPool.z - w.y;
        float rip = (vnoise(vec2(w.y * 26.0, uTime * 0.6 + w.x * 0.3)) - 0.5) * 0.1 * (0.4 + dpt * 2.0)
                  + 0.025 * sin(w.x * 3.0 + w.y * 50.0 + uTime * 1.2);
        vec2 rw = vec2(w.x + rip, uPool.z + 0.04 + dpt * 2.6);
        vec2 ruv = (rw - uView.xy) / uView.zw * 0.5 + 0.5;
        vec3 refl = texture(uScene, ruv).rgb + texture(uBloom, ruv).rgb * uBloomK * 1.3;
        float sheen = 0.75 + 0.5 * smoothstep(0.45, 0.9, vnoise(vec2(w.x * 1.6, w.y * 55.0 - uTime * 0.4)));
        c += refl * wa * 0.8 * sheen * exp(-dpt / 1.2);
    }
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

// Builds a fragment shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace forest_shaders

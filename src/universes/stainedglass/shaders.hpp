#pragma once
// STAINED GLASS's shaders (see stainedglass.cpp).
//
// WALL_FS paints the chapel (its stone wall, the two side windows' tracery and panes, the well's own clear glass,
// the sill and the floor with the colored pools); run at a quarter of the resolution with uEmit = 1, it only keeps
// the light coming through the glass (emitT_), which is blurred into the light falling on the stone (glowT_) and
// streaked from the sun into the shafts (SHAFT_FS). CELL_* draws the glass pieces (also into emitT_), SHARD_* a
// cleared row's shards, SPRITE_* the small lights (dust in the beams, candle flames, glints); DOWN_FS / BLUR_FS make
// the bloom and the blurred glass light, FINAL_FS adds the shafts and encodes the frame.
#include <string>

namespace stainedglass_shaders {

inline const char* COMMON = R"(
uniform vec4 uView;   // center, half extents (board units)
uniform float uSeed;
uniform vec4 uSun;    // the sun behind the glass: x, y (where it glares through), radius, strength of its glare
uniform vec3 uSunCol; // the daylight behind the window (linear, scaled by the section)
// (an arithmetic hash: no sine, cheap on software renderers too)
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx + uSeed) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + vec2(17.3, 5.1))); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.1 + 3.7) * 0.28 + vnoise(p * 4.3 + 9.1) * 0.17; }
vec2 worldAt(vec2 uv) { return uView.xy + (uv * 2.0 - 1.0) * uView.zw; }
vec2 viewUv(vec2 w) { return (w - uView.xy) / uView.zw * 0.5 + 0.5; }
float box(vec2 w, vec4 r, float soft) {
    return smoothstep(r.x - soft, r.x, w.x) * smoothstep(r.z + soft, r.z, w.x) * smoothstep(r.y - soft, r.y, w.y) * smoothstep(r.w + soft, r.w, w.y);
}
// A pointed arch (p from the middle of its spring line, half width a, the radius of its two arcs R): about the
// signed distance to its outline.
float archSd(vec2 p, float a, float R) {
    if (p.y <= 0.0) return abs(p.x) - a;
    return length(vec2(abs(p.x) + R - a, p.y)) - R;
}
// The light through the glass at w: the daylight, brighter where the sun glares through.
vec3 lightAt(vec2 w) {
    vec2 d = (w - uSun.xy) / uSun.z;
    return uSunCol * (0.6 + uSun.w * exp(-dot(d, d)));
}
// Hand-blown glass: x its uneven thickness (0..1), y a streak along the sheet's draw (-0.5..0.5), z a seed (a small
// bubble: its rim > 0, its dark inside < 0).
vec3 glassTex(vec2 t, float seed) {
    float th = fbm(t * 1.2 + seed * 3.1);
    float a = seed * 2.4;
    vec2 r = vec2(cos(a) * t.x + sin(a) * t.y, -sin(a) * t.x + cos(a) * t.y);
    float st = vnoise(r * vec2(0.9, 9.0) + seed) * 0.7 + vnoise(r * vec2(2.0, 23.0) + seed * 1.7) * 0.3 - 0.5;
    vec2 g = t * 4.0 + seed;
    vec2 gi = floor(g), gf = fract(g);
    float bub = 0.0;
    if (hash(gi + 0.37) > 0.7) {
        vec2 c = 0.25 + 0.5 * hash2(gi + 3.0);
        float rad = 0.05 + 0.09 * hash(gi + 9.0);
        float d = length(gf - c) / rad;
        bub = smoothstep(0.45, 0.75, d) * (1.0 - smoothstep(0.85, 1.1, d)) - 0.5 * (1.0 - smoothstep(0.3, 0.6, d));
    }
    return vec3(th, st, bub);
}
// A painted line (grisaille) along a distance field: d its distance, wd its half width, aa a pixel.
float paintLine(float d, float wd, float aa) { return 1.0 - smoothstep(wd - aa, wd + aa, abs(d)); }
)";

// ---------------------------------------------------------------- the chapel
inline const char* WALL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform int uEmit;             // 1: only the light through the glass (for the shafts and the light on the stone)
uniform sampler2D uGlow;       // the glass light, blurred: what the stone around the windows catches
uniform vec3 uStone, uStoneLit, uClear, uWellGlass, uLeadCol;
uniform vec3 uAmb;             // the nave's own dim light
uniform float uGlowK;          // how much of the windows' light falls on the stone
uniform float uPool;           // the pools of colored light on the floor
uniform vec2 uPoolShift;       // where the sun throws the windows on the floor: sideways and up, per unit of depth
uniform vec4 uWinA;            // the side windows: center |x|, base y, spring y, half width
uniform vec4 uWinB;            // the outer arch's radius, a lancet's half width, its arch's radius, the mullion's half width
uniform vec4 uRose;            // the rose: center (above the spring), radius, its center disc's radius, on (0/1)
uniform vec2 uPanel;           // a panel's height, panels per lancet
uniform int uNL, uSlots;       // lancets per window, panes per window
uniform float uWinOn;
uniform vec4 uPaneA[50];       // each pane: its ground color (linear), how far it is filled
uniform vec4 uPaneB[50];       // its medallion's color, its glint
uniform vec4 uHudL, uHudR;
uniform float uFloorY;
uniform vec4 uCandle[12];      // candles: x, y of the flame, intensity, flicker
uniform int uNC;
uniform vec3 uCandleCol;
uniform float uMotif;          // the grisaille painting (none on the modern window)
uniform float uWellLight;      // the well's clear glass

// The glass of a pane: clear grisaille until a cleared row fills it, then its color spreads in from its middle.
vec3 paneGlass(int slot, float region, vec2 pq, vec2 w, float dIn, out float glint, out float filled) {
    vec4 A = uPaneA[slot], B = uPaneB[slot];
    glint = B.w;
    vec3 col = region > 1.5 ? B.rgb * 0.75 : (region > 0.5 ? B.rgb : A.rgb);
    vec3 gt = glassTex(w * 1.1, float(slot) * 0.73 + 1.0);
    float fr = length(pq) * 0.6 + 0.12 * gt.x;
    float f = clamp((A.w * 1.25 - fr) / 0.18, 0.0, 1.0);
    filled = f;
    vec3 clearG = uClear * (0.75 + 0.45 * gt.x + 0.35 * gt.y);
    float dens = clamp(gt.x * 1.25 - 0.12 + gt.y * 0.55, 0.0, 1.0);
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    vec3 colG = mix(max(mix(vec3(lum), col, 1.3), vec3(0.0)) * 0.4, col * 1.2 + 0.02, smoothstep(0.1, 0.95, dens)) * 1.35;
    vec3 g = mix(clearG, colG, f);
    g *= 1.0 + 0.5 * max(gt.z, 0.0) - 0.25 * max(-gt.z, 0.0);
    // matting: the painter's shading darkens the glass toward the lead and the stone
    g *= 0.55 + 0.45 * smoothstep(0.0, 0.22, dIn);
    return g;
}

void main() {
    vec2 w = worldAt(vUv);
    vec3 glow = uEmit == 1 ? vec3(0.0) : texture(uGlow, vUv).rgb;
    float px = max(fwidth(w.x), 1e-4);
    vec3 L = lightAt(w);

    // ---- the side windows: the stone opening, two lancets parted by a mullion, a rose in the head
    float dOut = 99.0;
    if (uWinOn > 0.5) {
        float side = w.x < 0.0 ? -1.0 : 1.0;
        vec2 p = vec2(w.x - side * uWinA.x, w.y - uWinA.z);
        float yb = uWinA.y - uWinA.z;
        float a0 = uWinA.w, R0 = uWinB.x, a1 = uWinB.y, R1 = uWinB.z, m = uWinB.w;
        dOut = max(archSd(p, a0, R0), yb - p.y);
        if (dOut < 0.0) {
            int base = (side < 0.0 ? 0 : 1) * uSlots;
            int K = int(uPanel.y + 0.5);
            const float T = 0.11; // the tracery's bars (half width)
            int slot = -1;
            float region = 0.0, dIn = 0.0, leadL = 0.0;
            vec2 pq = vec2(0.0);
            vec2 pr = p - vec2(0.0, uRose.x);
            float r = length(pr);
            if (uRose.w > 0.5 && r < uRose.y + T) {
                float ang = atan(pr.y, pr.x);
                const float SEC = 0.785398;
                float j = floor(ang / SEC + 0.5);
                float da = ang - j * SEC;
                float dBar = r * sin(SEC * 0.5 - abs(da)) - T * 0.7;
                float dRing = uRose.y - T - r;
                float dCore = abs(r - uRose.z) - T * 0.75;
                if (r < uRose.z - T * 0.75) {
                    slot = base + uNL * (K + 1);
                    region = 1.0;
                    dIn = uRose.z - T * 0.75 - r;
                    pq = pr / uRose.z;
                    // a sexfoil painted in its middle
                    float fa = atan(pr.y, pr.x);
                    leadL = paintLine(r - uRose.z * (0.55 + 0.1 * cos(fa * 6.0)), 0.018, px) * 0.7;
                } else {
                    dIn = min(min(dBar, dRing), dCore);
                    if (dIn > 0.0) {
                        int pj = int(mod(j + 8.0, 8.0));
                        slot = base + uNL * (K + 1) + 1 + pj;
                        float rm = (uRose.z + uRose.y) * 0.5;
                        vec2 c = vec2(cos(j * SEC), sin(j * SEC)) * rm;
                        float fr = (uRose.y - uRose.z) * 0.3;
                        float dm = length(pr - c) - fr;
                        region = dm < 0.0 ? 1.0 : 0.0;
                        leadL = paintLine(dm, 0.02, px);
                        pq = (pr - c) / (fr * 2.2);
                    }
                }
            } else {
                // the lancets
                float xl = uNL == 1 ? 0.0 : (p.x < 0.0 ? -(m + a1) : (m + a1));
                vec2 pl = vec2(p.x - xl, p.y);
                float dl = max(archSd(pl, a1, R1), yb - p.y);
                if (dl < 0.0) {
                    int li = (uNL == 1 || p.x < 0.0) ? 0 : 1;
                    float ph = uPanel.x;
                    float k = floor((p.y - yb) / ph);
                    bool head = p.y > 0.0 || k >= float(K);
                    if (head) k = float(K);
                    slot = base + li * (K + 1) + int(k);
                    dIn = -dl;
                    float border = -dl - 0.24;
                    if (!head) {
                        float yc = yb + (k + 0.5) * ph;
                        vec2 q = vec2(pl.x, p.y - yc);
                        pq = q / vec2(a1, ph * 0.5);
                        // the saddle bar between two panels (iron), a lead around the medallion, the border
                        float bar = abs(fract((p.y - yb) / ph + 0.5) - 0.5) * ph;
                        dIn = min(dIn, k > 0.5 ? bar - 0.035 : 99.0);
                        float rMed = min(a1 - 0.3, ph * 0.5 - 0.12);
                        // the medallions alternate: a roundel, a quatrefoil
                        float dm = mod(k + float(li), 2.0) < 0.5 ? length(q) - rMed
                                 : min(length(abs(q) - vec2(rMed * 0.42, 0.0)), length(abs(q) - vec2(0.0, rMed * 0.42))) - rMed * 0.58;
                        region = border < 0.0 ? 2.0 : (dm < 0.0 ? 1.0 : 0.0);
                        leadL = max(paintLine(dm, 0.022, px), paintLine(border, 0.022, px));
                        if (uMotif <= 0.0) {
                            // the modern window: a mosaic of small squares in the pane's two glasses
                            vec2 gq = (q + vec2(a1, ph * 0.5)) / vec2(a1 * 0.5, ph * 0.25);
                            vec2 gi = floor(gq), gf = abs(fract(gq) - 0.5);
                            region = hash(gi + float(slot) * 7.1) < 0.55 ? 0.0 : 1.0;
                            leadL = paintLine(0.5 - max(gf.x, gf.y), 0.03, px * 4.0 / a1);
                        } else if (region < 0.5) {
                            // the ground: diamond quarries
                            vec2 dq = vec2(q.x + q.y, q.x - q.y) * 1.6;
                            vec2 fq = abs(fract(dq) - 0.5);
                            leadL = max(leadL, paintLine(min(fq.x, fq.y) / 1.6, 0.014, px));
                        }
                    } else {
                        pq = vec2(pl.x / a1, (p.y - max(0.0, yb + float(K) * ph)) / (a1 * 1.6) * 2.0 - 1.0);
                        float dm = length(vec2(pl.x, p.y - a1 * 0.75)) - a1 * 0.5;
                        region = dm < 0.0 ? 1.0 : 0.0;
                        leadL = paintLine(dm, 0.022, px);
                        if (uMotif <= 0.0) {
                            vec2 gq = vec2(pl.x + a1, p.y) / (a1 * 0.5);
                            vec2 gi = floor(gq), gf = abs(fract(gq) - 0.5);
                            region = hash(gi + float(slot) * 7.1) < 0.55 ? 0.0 : 1.0;
                            leadL = paintLine(0.5 - max(gf.x, gf.y), 0.03, px * 2.0 / a1);
                        }
                    }
                }
            }
            if (slot >= 0) {
                float glint, filled;
                vec3 g = paneGlass(slot, region, pq, w, dIn, glint, filled);
                // painted grisaille: fine leaf trails on the clear glass, a few strokes on the colored
                float paint = 0.0;
                if (uMotif > 0.0) {
                    float vine = sin(w.x * 5.0 + sin(w.y * 3.1) * 1.6) * 0.5 + 0.5;
                    paint = paintLine(vine - 0.5, 0.05, px) * 0.35 * (region < 0.5 ? 1.0 : 0.3) * uMotif;
                }
                g *= 1.0 - paint * 0.6;
                vec3 c = g * L * (1.0 + 0.9 * glint);
                c += mix(g, vec3(1.0), 0.6) * glint * 0.25;
                float aaE = px * 1.2;
                float onGlass = smoothstep(-aaE, aaE, dIn);
                if (uEmit == 1) { frag = vec4(c * onGlass * (0.45 + 0.55 * filled), 0.0); return; } // clear glass throws less
                // the leads inside the pane (dark) and the stone around it
                c = mix(c, uLeadCol * (0.5 + 0.6 * glow), leadL * 0.92);
                vec3 stone = uStoneLit * (uAmb * 0.8 + glow * uGlowK * 1.4);
                frag = vec4(mix(stone, c, onGlass), 0.0);
                return;
            }
        }
    }

    // ---- the well's own glass: dim clear quarries in lead (the central light of the window)
    float wellGlass = step(abs(w.x), 5.0) * step(-10.0, w.y);
    if (wellGlass > 0.5) {
        vec2 dq = vec2(w.x + w.y * 0.5, w.x - w.y * 0.5) * 0.7071;
        vec2 fq = abs(fract(dq) - 0.5);
        float ql = paintLine(min(fq.x, fq.y), 0.02, px);
        float bar = abs(fract((w.y + 10.0) / 5.0 + 0.5) - 0.5) * 5.0;
        // (so dim that one ripple of the glass is enough)
        vec3 g = uWellGlass * (0.75 + 0.5 * vnoise(w * vec2(1.1, 0.5) + 7.0)) * uWellLight;
        vec3 c = g * L;
        if (uEmit == 1) { frag = vec4(c * 0.6, 0.0); return; }
        c = mix(c, uLeadCol * 0.4, ql * 0.35);
        c = mix(c, uLeadCol * 0.3, paintLine(bar, 0.03, px) * 0.8 * step(w.y, 10.5));
        frag = vec4(c, 0.0);
        return;
    }
    if (uEmit == 1) { frag = vec4(0.0); return; }

    // ---- the stone: courses of ashlar, the well's jambs, the windows' mouldings, the sill, the floor
    float ex = abs(w.x - uView.x) / uView.z;
    float quiet = max(box(w, uHudL, 0.8), box(w, uHudR, 0.8));
    float grit = vnoise(w * 0.8 + 11.0) * 0.6 + vnoise(w * 1.9 + 4.0) * 0.4;
    float tone = vnoise(w * vec2(0.12, 0.18) + 2.0);
    vec3 alb = mix(uStone, uStoneLit, clamp(0.35 + (tone - 0.5) * 0.6 + (grit - 0.5) * 0.25, 0.0, 1.0));
    vec3 light = uAmb * (0.7 + 0.4 * ex) + glow * uGlowK;
    // the candles' warm light
    for (int i = 0; i < 12; i++) {
        if (i >= uNC) break;
        vec2 d = (w - uCandle[i].xy) / 2.4;
        light += uCandleCol * uCandle[i].z * (0.75 + 0.25 * uCandle[i].w) * exp(-dot(d, d)) ;
    }
    vec3 c;
    if (w.y > uFloorY) {
        // ashlar courses (faint joints, each block its own tone), quiet behind the HUD
        float ch = 1.15;
        float row = floor((w.y - uFloorY) / ch);
        float bx = w.x / 2.3 + 0.5 * mod(row, 2.0);
        float blk = hash(vec2(floor(bx), row));
        float jy = abs(fract((w.y - uFloorY) / ch + 0.5) - 0.5) * ch;
        float jx = abs(fract(bx + 0.5) - 0.5) * 2.3;
        float joint = 1.0 - smoothstep(0.015, 0.05, min(jx, jy));
        alb *= 0.88 + 0.22 * blk;
        alb *= 1.0 - 0.4 * joint * (1.0 - 0.75 * quiet);
        // the well's jambs: a slender column on each side, lit by the glass beside it
        float jb = abs(w.x) - 5.0;
        if (jb > 0.0 && jb < 0.75 && w.y > -10.6) {
            float u = jb / 0.75;
            float col = sqrt(max(0.0, 1.0 - pow(abs(u - 0.45) / 0.45, 2.0)));
            alb = uStoneLit * (0.5 + 0.55 * col);
            light += glow * uGlowK * 0.8;
        }
        // the side windows: a moulded arch around the opening (a splay that catches the light, a roll, a hollow)
        if (uWinOn > 0.5 && dOut < 0.75) {
            float u = dOut / 0.75;
            float roll = exp(-pow((u - 0.45) / 0.14, 2.0));
            float hollow = exp(-pow((u - 0.78) / 0.08, 2.0));
            alb = mix(alb, uStoneLit * (0.75 + 0.3 * grit), 1.0 - smoothstep(0.85, 1.0, u));
            alb *= 1.0 + 0.45 * roll - 0.45 * hollow;
            light += glow * uGlowK * (1.0 - u) * 1.2;
        }
        // the sills under the side windows and under the well: a ledge with a lit top
        float sillY = uWinOn > 0.5 && abs(abs(w.x) - uWinA.x) < uWinA.w + 0.7 ? uWinA.y : (abs(w.x) < 5.75 ? -10.0 : -99.0);
        float sd = sillY - w.y;
        if (sd > 0.0 && sd < 0.42) {
            alb = uStoneLit * (0.7 + 0.25 * grit) * (sd < 0.06 ? 1.35 : 0.85 - 0.6 * sd);
            light += glow * uGlowK * 0.7;
        }
        c = alb * light;
        // a little darker behind the HUD's columns
        c *= 1.0 - 0.3 * quiet;
    } else {
        // the floor: flagstones in perspective, the windows' colors thrown on them
        float dz = clamp((uFloorY - w.y) / max(uFloorY - (uView.y - uView.w), 0.5), 0.0, 1.0); // 0 at the wall, 1 at us
        float depth = 0.6 / max(1.0 - dz * 0.82, 0.12) - 0.6;
        float fx = w.x / (1.0 + depth * 0.45);
        float jl = min(abs(fract(fx / 2.6 + 0.5) - 0.5) * 2.6 * (1.0 - dz * 0.4), abs(fract(depth * 1.2 + 0.5) - 0.5) / 1.2 * 6.0);
        float joint = 1.0 - smoothstep(0.02, 0.07, jl);
        float fl = hash(vec2(floor(fx / 2.6 + 0.5), floor(depth * 1.2 + 0.5)));
        alb = mix(uStone, uStoneLit, 0.3 + 0.2 * grit) * (0.85 + 0.25 * fl) * (1.0 - 0.35 * joint);
        // the pools: the glass's light, carried down and sideways by the sun
        vec2 src = vec2(w.x - uPoolShift.x * depth, uFloorY + 1.2 + uPoolShift.y * depth);
        vec3 pool = texture(uGlow, viewUv(src)).rgb;
        light += pool * uPool * (0.6 + 0.4 * dz);
        light += glow * uGlowK * 0.4;
        c = alb * light;
        // the wall's foot in shadow
        c *= 0.75 + 0.25 * smoothstep(0.0, 0.08, dz);
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the glass pieces (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // type, glint, kind (0 glass, 1 ghost, 2 cracking), dark
layout(location = 4) in vec4 aD; // the sheet's pattern origin x, y, its seed, crack
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    vec2 wp = aA.xy + aPos * aA.z;
    vLocal = aPos * 0.5;
    vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
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
uniform int uEmit;
uniform vec3 uLeadCol;
uniform float uMinLight;  // the glass never goes darker than this (the stack always reads)
uniform float uMotif;
uniform vec3 uRoom;       // the room's light on the lead (faint)
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// The grisaille painted on a piece type's glass: a pattern of its own running across the whole sheet (never one
// mark per cell), as the distance to its lines (sheet units).
float motif(vec2 t, int type) {
    if (type == 0) { // a diaper of small quatrefoils
        vec2 q = abs(fract(t * 1.4 + 0.25) - 0.5);
        return (min(length(q - vec2(0.16, 0.0)), length(q - vec2(0.0, 0.16))) - 0.13) / 1.4;
    }
    if (type == 1) { // a trellis of diagonal lines
        vec2 d = vec2(t.x + t.y, t.x - t.y) * 0.9;
        vec2 f = abs(fract(d) - 0.5);
        return min(f.x, f.y) / 1.27;
    }
    if (type == 2) { // fish scales
        vec2 s = t * vec2(2.2, 2.6);
        float row = floor(s.y);
        vec2 c = vec2(fract(s.x + 0.5 * mod(row, 2.0)) - 0.5, fract(s.y));
        return (length(c) - 0.5) / 2.4;
    }
    if (type == 3) { // a vine: a waving stem and its curls
        float y = t.y * 1.3 + 0.28 * sin(t.x * 4.2);
        float f = fract(y) - 0.5;
        vec2 cc = vec2(fract(t.x * 0.67 + 0.25) - 0.5, f) * vec2(1.5, 1.0);
        return min(abs(f) / 1.3, abs(length(cc) - 0.22) / 1.4);
    }
    if (type == 4) { // rings, rippling out from a point of the sheet
        float r = length(t - vec2(0.7, 0.4));
        return abs(fract(r * 2.4) - 0.5) / 2.4;
    }
    if (type == 5) { // feathered strokes
        vec2 q = vec2(t.x * 3.0, t.y * 1.4 + 0.5 * floor(t.x * 3.0));
        vec2 f = vec2(fract(q.x) - 0.5, fract(q.y) - 0.5);
        return max(abs(f.x + f.y * 0.5) / 3.0, abs(f.y) - 0.32);
    }
    // a powdering of small rosettes
    vec2 g = fract(t * vec2(1.6, 1.6) + vec2(0.5 * mod(floor(t.y * 1.6), 2.0), 0.0)) - 0.5;
    return (abs(length(g) - 0.1)) / 1.6;
}
void main() {
    int m = int(vA.w + 0.5);
    int kind = int(vC.z + 0.5);
    int type = int(vC.x + 0.5);
    vec2 l = vLocal;
    float aa = max(fwidth(l.x), 1e-4);
    // the lead: a broad came around the piece, a fine one between its own cells
    const float LO = 0.12, LI = 0.022;
    float hwL = has(m, 0) ? LI : LO, hwR = has(m, 1) ? LI : LO, hwD = has(m, 2) ? LI : LO, hwU = has(m, 3) ? LI : LO;
    float eL = l.x + 0.5, eR = 0.5 - l.x, eD = l.y + 0.5, eU = 0.5 - l.y;
    float gIn = min(min(eL - hwL, eR - hwR), min(eD - hwD, eU - hwU));
    // the outline's inner corners (both sides in the piece, the diagonal not)
    float ix = l.x < 0.0 ? eL : eR, iy = l.y < 0.0 ? eD : eU;
    bool hx = l.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = l.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = l.x < 0.0 ? (l.y < 0.0 ? 4 : 6) : (l.y < 0.0 ? 5 : 7);
    bool notch = hx && vy && !has(m, db);
    // distance to the piece's outline only (its broad came, the painter's matting)
    float gOut = 9.0;
    if (!has(m, 0)) gOut = min(gOut, eL - LO);
    if (!has(m, 1)) gOut = min(gOut, eR - LO);
    if (!has(m, 2)) gOut = min(gOut, eD - LO);
    if (!has(m, 3)) gOut = min(gOut, eU - LO);
    if (notch) gOut = min(gOut, max(ix - LO, iy - LO));
    gIn = min(gIn, gOut);
    float onGlass = smoothstep(-aa, aa, gIn);
    vec3 tint = vB.rgb;

    if (kind == 1) {
        // the ghost: the empty came where the piece will be set, a faint line of its color
        float outl = gOut > 8.0 ? 0.0 : 1.0 - smoothstep(0.0, aa * 1.5, abs(gOut + LO * 0.55) - LO * 0.16);
        float a = outl * vB.w;
        if (uEmit == 1) { frag = vec4(0.0); return; }
        frag = vec4(mix(tint, vec3(1.0), 0.35) * a, a);
        return;
    }

    // the glass: one sheet per piece (its texture runs on across the cells), lit from behind
    vec2 t = vD.xy + l;
    float seed = vD.z;
    vec3 gt = glassTex(t, seed);
    float lum = dot(tint, vec3(0.2126, 0.7152, 0.0722));
    vec3 deep = max(mix(vec3(lum), tint, 1.35), vec3(0.0)) * 0.42;
    // pot metal: its color densest where the glass is thick, clear and bright where it is thin
    float dens = clamp(gt.x * 1.25 - 0.12 + gt.y * 0.55, 0.0, 1.0);
    vec3 g = mix(deep, tint * 1.15 + vec3(0.03), smoothstep(0.1, 0.95, dens));
    // the modern window's flashed glass: a second layer of the color, thinned in broad strokes
    if (uMotif <= 0.0) g = mix(g, tint * 1.4 + 0.06, smoothstep(0.55, 0.8, vnoise(t * 0.8 + seed)) * 0.55);
    // the seeds catch the light (more when the glass glints)
    g *= 1.0 + (0.6 + 2.0 * vC.y) * max(gt.z, 0.0) - 0.35 * max(-gt.z, 0.0);
    // the painter's matting: a soft shade toward the piece's lead, barely along the fine leads inside
    g *= (0.42 + 0.58 * smoothstep(0.0, 0.24, gOut)) * (0.9 + 0.1 * smoothstep(0.0, 0.05, gIn));
    // grisaille: the type's pattern, painted thin and brown over the sheet
    if (uMotif > 0.0) g *= 1.0 - 0.42 * uMotif * paintLine(motif(t, type), 0.014, aa);
    vec3 Lt = max(lightAt(vW), vec3(uMinLight));
    vec3 c = g * Lt * 1.55 * (1.0 + 0.6 * vC.y);
    c += mix(tint, vec3(1.0), 0.55) * vC.y * 0.1;
    // cracking (a cleared row, a few hundredths of a second): bright fractures run through it
    float crack = vD.w;
    if (kind == 2 && crack > 0.0) {
        vec2 cp = t * 2.6 + seed;
        vec2 ci = floor(cp), cf = fract(cp);
        float d1 = 8.0, d2 = 8.0;
        for (int y = -1; y <= 1; y++)
            for (int x = -1; x <= 1; x++) {
                vec2 o = vec2(float(x), float(y)) + hash2(ci + vec2(float(x), float(y))) - cf;
                float d = dot(o, o);
                if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
            }
        float edge = sqrt(d2) - sqrt(d1);
        float line = 1.0 - smoothstep(0.0, 0.05 + 0.08 * crack, edge);
        c = c * (1.0 + 0.6 * crack) + vec3(1.0, 0.97, 0.9) * line * crack * 1.5;
    }
    // game over: the light goes out of it
    float gray = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(gray) * vec3(0.16, 0.16, 0.19), vC.w);
    if (uEmit == 1) { frag = vec4(c * onGlass * vB.w, 0.0); return; }
    // the came: dark lead, rounded (a faint sheen along its middle)
    float prof;
    if (gOut < 0.0 && gOut < gIn + 1e-4) prof = clamp(1.0 - pow((gOut + LO * 0.5) / (LO * 0.5), 2.0), 0.0, 1.0);
    else prof = clamp(1.0 - pow(min(min(eL, eR), min(eD, eU)) / LI, 2.0), 0.0, 1.0) * 0.6;
    vec3 lead = uLeadCol * (0.6 + 0.8 * prof) + uRoom * prof * prof * 0.6;
    vec3 col = mix(lead, c, onGlass);
    float a = vB.w;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- shards
inline const char* SHARD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size, rotation
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // tumble phase, seed
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float c = cos(aA.w), s = sin(aA.w);
    vec2 l = aPos * aA.z;
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vQ = aPos;
    vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SHARD_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec3 uLeadCol;
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
    float sd = vC.y * 6.2831;
    vec2 a = vec2(cos(sd), sin(sd)) * 0.95;
    vec2 b = vec2(cos(sd + 2.1 + 0.5 * sin(sd * 3.0)), sin(sd + 2.1 + 0.5 * sin(sd * 3.0))) * 0.75;
    vec2 c = vec2(cos(sd + 4.3), sin(sd + 4.3)) * 0.9;
    float d = sdTri(vQ, a, b, c);
    float aa = max(fwidth(d), 1e-3);
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.0) discard;
    // it tumbles: its face turns to the light and away, flashing as it passes
    float face = 0.5 + 0.5 * sin(vC.x);
    float flash = pow(face, 12.0);
    vec3 col = vB.rgb * (0.45 + 0.8 * face) + vec3(1.0, 0.97, 0.9) * flash * 0.8;
    // a fresh break: a bright edge
    col += mix(vB.rgb, vec3(1.0), 0.5) * 0.5 * (1.0 - smoothstep(0.0, 0.14, -d));
    float al = cov * vB.a;
    frag = vec4(col * al, al * 0.6);
}
)";

// ---------------------------------------------------------------- small lights and the candles
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, rx, ry
layout(location = 2) in vec4 aB; // rgb, intensity
layout(location = 3) in vec4 aC; // kind, rotation, a, b
uniform mat4 uVP;
uniform vec4 uView;
out vec2 vQ;
flat out vec4 vB;
flat out vec4 vC;
flat out vec2 vUvC; // the sprite's center on screen (the dust reads the shafts there)
void main() {
    float c = cos(aC.y), s = sin(aC.y);
    vec2 l = aPos * aA.zw;
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vQ = aPos;
    vB = aB; vC = aC;
    vUvC = (aA.xy - uView.xy) / uView.zw * 0.5 + 0.5;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vB;
flat in vec4 vC;
flat in vec2 vUvC;
out vec4 frag;
uniform sampler2D uShaft;
uniform float uDustK;
void main() {
    int kind = int(vC.x + 0.5);
    float r2 = dot(vQ, vQ);
    if (kind == 0) {
        // a soft glow
        float k = exp(-r2 * 3.5) * (1.0 - smoothstep(0.7, 1.0, sqrt(r2)));
        frag = vec4(vB.rgb * vB.a * k, 0.0);
    } else if (kind == 1) {
        // a glint: a small star of light
        vec2 q = abs(vQ);
        float arms = exp(-q.x * 26.0) * exp(-q.y * 3.0) + exp(-q.y * 26.0) * exp(-q.x * 3.0);
        float k = (exp(-r2 * 28.0) + 0.5 * arms) * (1.0 - smoothstep(0.8, 1.0, max(q.x, q.y)));
        frag = vec4(vB.rgb * vB.a * k, 0.0);
    } else if (kind == 2) {
        // a mote of dust, seen only where a beam of light crosses it
        vec3 sh = texture(uShaft, vUvC).rgb;
        float k = exp(-r2 * 4.0);
        frag = vec4((sh * uDustK + vB.rgb * 0.02) * vB.a * k, 0.0);
    } else if (kind == 3) {
        // a candle's flame: a bright teardrop, a warm halo
        vec2 q = vQ * vec2(1.0, 1.0);
        float body = exp(-dot(q * vec2(3.2, 1.6) - vec2(0.0, -0.3), q * vec2(3.2, 1.6) - vec2(0.0, -0.3)) * 2.0);
        float core = exp(-dot(q * vec2(6.0, 3.5) + vec2(0.0, 1.1), q * vec2(6.0, 3.5) + vec2(0.0, 1.1)));
        vec3 c = vB.rgb * body * 1.4 + vec3(1.0, 0.95, 0.85) * core * 1.2;
        frag = vec4(c * vB.a, 0.0);
    } else if (kind == 4) {
        // a candle's wax (opaque), lit from its flame
        vec2 q = vQ;
        float a = (1.0 - smoothstep(0.85, 1.0, abs(q.x))) * (1.0 - smoothstep(0.92, 1.0, abs(q.y)));
        float shade = 0.55 + 0.45 * (1.0 - q.x * q.x) ;
        vec3 c = vB.rgb * shade * (0.75 + 0.35 * smoothstep(-1.0, 1.0, q.y));
        a *= vB.a;
        frag = vec4(c * a, a);
    } else {
        // a puff of dust from under a piece
        float k = exp(-r2 * 2.5) * (1.0 - smoothstep(0.6, 1.0, sqrt(r2)));
        frag = vec4(vB.rgb * vB.a * k, 0.0);
    }
}
)";

// ---------------------------------------------------------------- the shafts (quarter resolution)
// The glass light streaked from the sun: each pixel gathers the light of the glass between it and the sun.
inline const char* SHAFT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uSunUv;   // where the rays come from (off screen, above)
uniform vec2 uSunW;    // the same, in board units
uniform float uSpan;   // how far back toward the sun a pixel looks
uniform float uTime;
void main() {
    const int N = 28;
    vec2 d = (uSunUv - vUv) * uSpan / float(N);
    vec2 p = vUv;
    vec3 acc = vec3(0.0);
    float wsum = 0.0, wk = 1.0;
    float j = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    p += d * j;
    for (int i = 0; i < N; i++) {
        acc += texture(uSrc, p).rgb * wk;
        wsum += wk;
        wk *= 0.965;
        p += d;
    }
    // separate rays fanning from the sun, the dust drifting in them
    vec2 w = worldAt(vUv);
    vec2 dr = w - uSunW;
    float ang = atan(dr.x, -dr.y);
    float rays = vnoise(vec2(ang * 90.0, uTime * 0.04)) * 0.65 + vnoise(vec2(ang * 230.0 + 3.0, uTime * 0.07)) * 0.35;
    float dust = (0.35 + 1.3 * smoothstep(0.25, 0.85, rays)) * (0.6 + 0.8 * fbm(w * vec2(0.25, 0.15) + vec2(uTime * 0.05, -uTime * 0.02)));
    frag = vec4(acc / wsum * dust, 1.0);
}
)";

// ---------------------------------------------------------------- bloom, blur
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(float(x), float(y)) * uTexel).rgb;
    c /= 9.0;
    frag = vec4(max(c - vec3(uThreshold), vec3(0.0)), 1.0);
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
uniform sampler2D uScene, uBloom, uShaft, uEmitT;
uniform vec4 uQuietA, uQuietB, uQuietC; // the corners' lettering
uniform float uBloomK, uShaftK, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk; // linear
uniform vec4 uHudL, uHudR;
void main() {
    vec4 s = texture(uScene, vUv);
    vec2 w = worldAt(vUv);
    // the shafts in the dusty air, kept faint over the well and the HUD
    float quiet = max(box(w, vec4(-5.3, -11.0, 5.3, 12.5), 0.8) * 0.92, max(box(w, uHudL, 1.0), box(w, uHudR, 1.0)) * 0.6);
    quiet = max(quiet, max(max(box(w, uQuietA, 1.0), box(w, uQuietB, 1.0)), box(w, uQuietC, 1.0)) * 0.8);
    // the beams show in the air in front of the stone, barely over the glass they come through
    vec3 em = texture(uEmitT, vUv).rgb;
    quiet = max(quiet, smoothstep(0.03, 0.25, dot(em, vec3(0.3, 0.5, 0.2))) * 0.6);
    vec3 shaft = texture(uShaft, vUv).rgb * uShaftK * (1.0 - quiet);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK + shaft * (1.0 - 0.6 * clamp(s.a, 0.0, 1.0));
    // a soft knee: the blazing glass never burns to white
    c = c / (1.0 + 0.3 * max(max(c.r, c.g), c.b));
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
// Lettering cut in the stone: pale, a soft dark halo around it.
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
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace stainedglass_shaders

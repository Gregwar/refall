#pragma once
// LANTERNS' shaders (see lanterns.cpp).
//
// ENV_FS draws the night per pixel from the camera's ray: the sky (a gradient, a few stars, the far shore's hills, the
// moon in slow motion) and the lake, a true horizontal plane at the well's floor: rippled normals reflect the sky and
// distort the planar reflection of the scene (a mirrored render, sampled in screen space, drawn out into wobbly
// streaks), a finer chop near, a low moon's path of glitter off the left of the frame. BOX_FS shades every box: the
// pieces (paper volumes lit from inside, each cell glowing at its core, a small candle flame in some cells, a thin
// frame along the piece's edges), the lanterns (paper boxes with a dark cap and base, rounding into sky lanterns as
// they rise), the frames of light (the well's, the hold box's), the well's glass and its grid; SPRITE_FS the points of light (far lanterns, embers); BRIGHT_FS /
// BLUR_FS the glow; FINAL_FS composes and encodes the frame.
#include <string>

namespace lanterns_shaders {

// ---------------------------------------------------------------- common
inline const char* COMMON = R"(
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}
)";

// ---------------------------------------------------------------- the sky and the lake
inline const char* ENV_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // the board plane on screen: center, half extents (board units)
uniform vec3 uEye;
uniform float uFocal, uAspect, uShift; // the camera: 1 / tan(half fov), aspect, lens shift (NDC)
uniform float uWater, uTime, uRipple, uStars, uMoon, uPxAng, uGlitter;
uniform float uStarsOn; // the scene's setup and teardown: the share of the stars out (they come out one by one)
uniform vec3 uSkyTop, uSkyHor, uLake, uShore, uGlow;
uniform vec4 uColL, uColR, uCorner;   // HUD rectangles (board plane x0, y0, x1, y1): no stars behind them
uniform sampler2D uRefl;
float inRect(vec2 w, vec4 r) {
    vec2 d = max(vec2(r.x, r.y) - w, w - vec2(r.z, r.w));
    return smoothstep(-0.6, 0.4, max(d.x, d.y));
}
vec3 moonDir() { return normalize(vec3(0.43, 0.12 + 0.1 * uMoon, -1.0)); }
// The water's height: slow swells and, near (by `near`), a finer chop.
float waves(vec2 q, float tt, float near) {
    return vnoise(q + vec2(tt, -tt * 0.7)) + 0.5 * vnoise(q * 2.3 - vec2(tt * 1.3, tt)) +
           near * 0.35 * vnoise(q * 6.1 + vec2(tt * 2.1, -tt * 1.4));
}
// The sky in a direction (dir normalized), the far shore and the moon included.
vec3 sky(vec3 dir, bool stars, float hudClear, float glint) {
    float e = dir.y;
    float t = clamp(e * 2.4, 0.0, 1.0);
    vec3 c = mix(uSkyHor, uSkyTop, pow(t, 0.55));
    c += uGlow * exp(-max(e, 0.0) * 14.0);
    if (stars && e > 0.03) {
        vec2 a = vec2(atan(dir.x, -dir.z), asin(clamp(e, -1.0, 1.0))) * 38.0;
        vec2 gi = floor(a);
        float h = hash(gi);
        if (h > 0.9 && hash(gi + 11.3) < uStarsOn * 1.02 - 0.01) {
            vec2 sp = gi + vec2(hash(gi + 3.1), hash(gi + 7.7)) * 0.8 + 0.1;
            float d = length(a - sp) / 38.0;
            float tw = 0.75 + 0.25 * sin(uTime * (0.3 + h) + h * 40.0);
            float s = (1.0 - smoothstep(0.0, max(uPxAng * 1.1, 0.0008), d)) * (h - 0.9) * 10.0 * tw;
            c += vec3(0.75, 0.8, 0.95) * s * 0.35 * uStars * smoothstep(0.03, 0.12, e) * hudClear;
        }
    }
    if (uMoon > 0.0 && glint >= 0.0) {
        // on the water: the moon's path, a column of glints under it
        vec3 m = moonDir();
        float dx = atan(dir.x, -dir.z) - atan(m.x, -m.z);
        float dy = abs(dir.y) - m.y;
        float path = exp(-dx * dx / 0.0012) * exp(-dy * dy / 0.03);
        c += vec3(0.55, 0.62, 0.78) * path * (0.12 + 0.88 * glint) * 0.5 * uMoon;
    } else if (uMoon > 0.0) {
        // (measured on the image plane, so the disc stays round off axis)
        vec3 m = moonDir();
        float md = length(dir.xy / max(-dir.z, 1e-3) - m.xy / -m.z);
        float r = 0.03;
        float disc = 1.0 - smoothstep(r - uPxAng, r + uPxAng, md);
        float mare = 0.9 + 0.1 * vnoise(dir.xy * 70.0 + 4.0);
        c = mix(c, vec3(0.6, 0.64, 0.72) * mare, disc * uMoon);
        c += vec3(0.28, 0.33, 0.45) * exp(-md * 30.0) * 0.18 * uMoon;
    }
    // the far shore: low hills along the horizon, here and there
    float az = atan(dir.x, -dir.z);
    float hill = max(0.0, fbm(vec2(az * 9.0, 3.1)) - 0.42) * 0.07 + 0.002 + 0.0015 * vnoise(vec2(az * 60.0, 1.0));
    float sh = 1.0 - smoothstep(hill - uPxAng, hill + uPxAng, e);
    c = mix(c, uShore, sh);
    return c;
}
void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 dir = normalize(vec3(ndc.x * uAspect / uFocal, (ndc.y - uShift) / uFocal, -1.0));
    vec2 bw = uView.xy + ndc * uView.zw; // the board plane under this pixel (for the HUD)
    float hudClear = inRect(bw, uColL) * inRect(bw, uColR) * inRect(bw, uCorner);
    vec3 c;
    if (dir.y >= -1e-4) {
        c = sky(dir, true, hudClear, -1.0);
    } else {
        float t = (uWater - uEye.y) / dir.y;
        vec3 P = uEye + dir * t;
        // the water's ripples: a slow height field, its finer chop near, fading out with the distance (no shimmer
        // far away)
        float fade = clamp(25.0 / t, 0.05, 1.0), near = clamp(40.0 / t - 0.3, 0.0, 1.0);
        float amp = (0.016 + 0.05 * uRipple) * fade;
        vec2 q = P.xz * vec2(0.35, 0.8);
        float tt = uTime * (0.25 + 0.35 * uRipple);
        float h0 = waves(q, tt, near);
        float hx = waves(q + vec2(0.04, 0.0), tt, near);
        float hz = waves(q + vec2(0.0, 0.04), tt, near);
        vec3 n = normalize(vec3(-(hx - h0) / 0.04 * amp * 0.55, 1.0, -(hz - h0) / 0.04 * amp * 1.1));
        vec3 r = reflect(dir, n);
        r.y = abs(r.y);
        float fres = 0.03 + 0.97 * pow(1.0 - max(dot(-dir, n), 0.0), 5.0);
        float glint = smoothstep(0.6, 0.85, vnoise(P.xz * vec2(1.4, 4.0) + vec2(0.0, uTime * 0.4))) * clamp(30.0 / t, 0.2, 1.0);
        vec3 sr = sky(normalize(r), false, 1.0, glint);
        c = mix(uLake, sr, fres);
        // the scene mirrored in the water, distorted by the same ripples and drawn out into short wobbly streaks
        // toward the eye (a lantern's reflection a soft shimmer under it)
        float reach = clamp(6.0 / t, 0.05, 0.35);
        vec2 uv = vUv + n.xz * vec2(0.6, 0.9) * reach;
        vec4 rs = vec4(0.0);
        float st = 0.0015 + 0.006 * reach;
        for (int i = -3; i <= 4; i++) {
            float fi = float(i);
            vec2 o = vec2(n.x * 0.04 * reach * sin(fi * 2.1 + P.z * 1.7), -fi * st);
            rs += texture(uRefl, uv + o) * exp(-fi * fi * 0.12);
        }
        rs *= 1.0 / 4.6;
        rs.rgb *= 0.7; // a gentle shimmer under each lantern, not a bar of light
        float k = 0.35 + 0.55 * fres;
        c = c * (1.0 - min(rs.a * k, 1.0)) + rs.rgb * k;
        // the low moon off the left of the frame: its path of glitter on the small waves, fanning out toward the eye,
        // and a few glints across the whole lake (far away, a soft sheen: no sparkle finer than a pixel)
        float az = atan(dir.x, -dir.z) + atan(0.86 * uAspect / uFocal); // (near the left edge, whatever the aspect)
        float wpath = 0.035 + 0.05 * near;
        float path = exp(-az * az / (wpath * wpath)) * smoothstep(-0.002, -0.03, dir.y);
        float sp = vnoise(P.xz * vec2(2.6, 6.0) + n.xz * 30.0 + vec2(tt * 0.6, 0.0)) *
                   vnoise(P.xz * vec2(4.1, 9.0) - vec2(0.0, tt * 0.8));
        float sparkle = mix(0.12, smoothstep(0.3, 0.6, sp) * 1.4, clamp(50.0 / t, 0.0, 1.0));
        c += vec3(0.62, 0.68, 0.82) * sparkle * (path + 0.03 * near) * uGlitter;
        // haze toward the far shore
        c = mix(c, mix(uSkyHor, uShore, 0.5) * 0.9, smoothstep(250.0, 1400.0, t) * 0.8);
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- boxes: pieces, lanterns, frames, glass
inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;    // unit cube, -0.5 .. 0.5
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aA;      // center x, y, z, mask
layout(location = 3) in vec4 aB;      // size x, y, z, kind
layout(location = 4) in vec4 aC;      // paper (linear), alpha
layout(location = 5) in vec4 aD;      // light, progress (cap; a rod: its middle's glow), seed, flame (0: none, else its phase)
layout(location = 6) in vec4 aE;      // candle x, y, z, radius (a lantern: x its rounding into a sky lantern)
uniform mat4 uVP;
out vec3 vW;
out vec3 vL;
flat out vec3 vN;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
flat out vec4 vE;
void main() {
    vec3 w = aA.xyz + aPos * aB.xyz;
    vW = w; vL = aPos; vN = aNrm;
    vA = aA; vB = aB; vC = aC; vD = aD; vE = aE;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* BOX_FS = R"(#version 330 core
in vec3 vW;
in vec3 vL;
flat in vec3 vN;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
flat in vec4 vE;
out vec4 frag;
uniform float uMirror, uWater, uStackA, uTime;
uniform vec3 uFlame, uWood;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
void main() {
    float uPx = max(length(fwidth(vW)) * 0.6, 1e-4); // board units per pixel here
    if (uMirror > 0.5 && vW.y < uWater - 0.002) discard;
    int kind = int(vB.w + 0.5);
    int m = int(vA.w + 0.5);
    int face = vN.z > 0.5 ? 0 : vN.z < -0.5 ? 1 : vN.x > 0.5 ? 2 : vN.x < -0.5 ? 3 : vN.y > 0.5 ? 4 : 5;
    vec3 tint = vC.rgb;
    float light = vD.x;
    // the face's own coordinates (q in -0.5 .. 0.5) and the board units per q unit
    vec2 q;
    vec2 qs;
    vec2 fc; // a world position on the face, for the paper's texture
    if (face <= 1) { q = vL.xy; qs = vB.xy; fc = vW.xy; }
    else if (face <= 3) { q = vec2(-vL.z * (face == 2 ? 1.0 : -1.0), vL.y); qs = vB.zy; fc = vec2(vW.z * 1.3 + vW.x, vW.y); }
    else { q = vec2(vL.x, -vL.z); qs = vB.xz; fc = vec2(vW.x, vW.z * 1.3); }

    if (kind == 3) {
        // a rod of a frame of light (the well's, the hold box's): a thread of light, brightest along its middle line,
        // frosted (a fine glitter along it), brighter toward its middle by vD.y
        bool vert = qs.y > qs.x;
        float across = vert ? q.x : q.y, along = vert ? q.y : q.x;
        float core = exp(-across * across * 14.0);
        float frost = 0.8 + 0.6 * smoothstep(0.6, 0.95, vnoise(vW.xy * 21.0 + vD.z));
        float mid = vD.y * exp(-along * along * 30.0);
        float sh = face == 0 ? 1.0 : 0.45;
        frag = vec4(vC.rgb * (0.5 + 1.7 * core) * frost * (1.0 + mid) * sh * vD.x, uMirror);
        return;
    }
    if (kind == 4) {
        // the well's glass: the fine grid of the cells' lines etched in it, light threads frosted here and there with
        // a glint, over a faint haze dimming the night behind it a little
        if (face != 0) discard;
        vec2 gd = abs(fract(vW.xy) - 0.5);
        gd = 0.5 - gd; // the distance to the nearest cell line
        float line = 1.0 - smoothstep(0.3 * uPx, 1.3 * uPx, min(gd.x, gd.y));
        float frost = 0.5 + 0.8 * vnoise(vW.xy * vec2(23.0, 19.0));
        float glint = smoothstep(0.8, 0.97, vnoise(vW.xy * 47.0 + 5.0));
        float grain = vnoise(vW.xy * 80.0);
        vec3 c = vC.rgb * (line * (frost + 2.0 * glint) * 0.12 + 0.004 * grain);
        frag = vec4(c, 0.12) * vC.w;
        return;
    }
    if (kind == 0 || kind == 1) {
        // a cell of a piece: hidden faces between the piece's own cells go
        if (face == 1) discard;
        if (face == 2 && has(m, 1)) discard;
        if (face == 3 && has(m, 0)) discard;
        if (face == 4 && has(m, 3)) discard;
        if (face == 5 && has(m, 2)) discard;
        if (kind == 1 && face != 0) discard;
        // the frame runs where the piece ends: which of this face's four edges are framed
        bool eL, eR, eB, eT;
        if (face == 0) { eL = !has(m, 0); eR = !has(m, 1); eB = !has(m, 2); eT = !has(m, 3); }
        else if (face <= 3) { eL = true; eR = true; eB = !has(m, 2); eT = !has(m, 3); }
        else { eL = !has(m, 0); eR = !has(m, 1); eB = true; eT = true; }
        float d = 9.0;
        if (eL) d = min(d, (q.x + 0.5) * qs.x);
        if (eR) d = min(d, (0.5 - q.x) * qs.x);
        if (eB) d = min(d, (q.y + 0.5) * qs.y);
        if (eT) d = min(d, (0.5 - q.y) * qs.y);
        // inner corners of the piece (two sides of the piece, the diagonal cell not): the outline turns there too,
        // so it runs on unbroken around the whole piece (the distance to the corner, square like the edges')
        vec2 e0 = (q + 0.5) * qs, e1 = (0.5 - q) * qs;
        float kc[4] = float[4](9.0, 9.0, 9.0, 9.0);
        if (face == 0) {
            if (has(m, 0) && has(m, 3) && !has(m, 4)) kc[0] = max(e0.x, e1.y);
            if (has(m, 1) && has(m, 3) && !has(m, 5)) kc[1] = max(e1.x, e1.y);
            if (has(m, 0) && has(m, 2) && !has(m, 6)) kc[2] = max(e0.x, e0.y);
            if (has(m, 1) && has(m, 2) && !has(m, 7)) kc[3] = max(e1.x, e0.y);
        }
        if (kind == 1) {
            // the ghost: a fine outline of the paper's light where the piece will land, one closed line around the
            // whole piece (the edges and the inner corners, nothing between its own cells)
            d = min(d, min(min(kc[0], kc[1]), min(kc[2], kc[3])));
            float line = 1.0 - smoothstep(0.035 - uPx, 0.035 + uPx, abs(d - 0.06));
            float fill = 0.05 * smoothstep(0.05, 0.2, d);
            float a = max(line * 0.55, fill) * vC.w;
            frag = vec4(tint * a, 0.0);
            return;
        }
        const float ft = 0.07;
        if (face != 0) {
            // the rim (the top and the sides of a thin panel): plain, darker paper by its frame
            float rimL = face == 4 ? 0.3 : face == 5 ? 0.12 : 0.2;
            vec3 c = mix(uWood * 0.8, tint * light, rimL);
            if (uMirror > 0.5) c *= uStackA; // the stack's reflection: faint
            frag = vec4(c, uStackA * vC.w);
            return;
        }
        // the inner corners (kc, above): a frame corner too
        float g = 1.0;
        for (int i = 0; i < 4; i++)
            if (kc[i] < 9.0) { d = min(d, kc[i]); g *= smoothstep(ft, ft + 0.3, kc[i]); }
        float frame = 1.0 - smoothstep(ft - uPx, ft + uPx, d);
        if (eL) g *= smoothstep(ft, ft + 0.45, e0.x);
        if (eR) g *= smoothstep(ft, ft + 0.45, e1.x);
        if (eB) g *= smoothstep(ft, ft + 0.45, e0.y);
        if (eT) g *= smoothstep(ft, ft + 0.45, e1.y);
        // paper lit from inside: darker toward the frame, each cell glowing at its core, a soft warmth at the candle
        // in the piece's heart
        float hd = length(vW.xy - vE.xy) / max(vE.w, 0.1);
        float hot = exp(-hd * hd * 1.2);
        float core = exp(-dot(q, q) * 5.0);
        float paper = 0.26 + 0.56 * pow(g, 0.7) + 0.2 * hot * g + 0.4 * core * g;
        // the ribs seen through the paper (between the cells' junctions, never on them), a fine fiber
        float rib = abs(fract(vW.y * 2.0 + 0.5) - 0.5) * 0.5;
        paper *= 1.0 - 0.07 * (1.0 - smoothstep(0.0, 0.025 + uPx, rib));
        float fib = vnoise(fc * vec2(3.0, 16.0) + vD.z) * 0.6 + vnoise(fc * vec2(9.0, 40.0) + vD.z * 3.0) * 0.4;
        paper *= 0.95 + 0.08 * fib;
        vec3 deep = pow(tint, vec3(1.5));
        vec3 col = mix(deep, tint, smoothstep(0.0, 0.6, g)) * paper;
        col = mix(col, col * uFlame * 1.15, 0.35 * hot);
        vec3 c = max(col * light, tint * 0.05 * paper);
        vec3 wood = uWood * 0.6 + tint * 0.04 * light;
        // a fine seam of the frame between the piece's own cells, the paper a little darker along it: each cell
        // reads as its own small lantern inside the lit piece
        float ds = 9.0;
        if (!eL) ds = min(ds, e0.x);
        if (!eR) ds = min(ds, e1.x);
        if (!eB) ds = min(ds, e0.y);
        if (!eT) ds = min(ds, e1.y);
        c *= 1.0 - 0.28 * (1.0 - smoothstep(0.02, 0.16, ds));
        c = mix(c, wood, (1.0 - smoothstep(0.016 - uPx, 0.016 + uPx, ds)) * 0.9);
        c = mix(c, wood, frame);
        if (vD.w > 0.0) {
            // the small flame of a candle standing in this cell: a teardrop of light, swaying a little, its glow
            // warming the paper around it
            float ph = vD.w * 40.0;
            float sway = 0.012 * sin(uTime * 5.3 + ph) + 0.006 * sin(uTime * 13.1 + ph * 1.7);
            float fh = 0.19 * (0.92 + 0.08 * sin(uTime * 9.7 + ph * 2.3));
            vec2 fp = q - vec2(sway * (q.y + 0.2) / 0.3, -0.12);
            float fw = 0.055 * (1.0 - 0.75 * smoothstep(-0.2, 1.0, fp.y / fh));
            float fd = length(vec2(fp.x / fw, fp.y / fh));
            float flame = 1.0 - smoothstep(0.75, 1.15, fd);
            float glow = exp(-dot(fp, fp) * 26.0);
            c += (uFlame * vec3(1.0, 0.82, 0.45) * flame * 1.8 + mix(tint, uFlame, 0.6) * glow * 0.4) * light;
        }
        if (uMirror > 0.5) c *= uStackA; // the stack's reflection: faint
        frag = vec4(c, uStackA * vC.w);
        return;
    }
    // kind 2: a paper lantern (and a run of a cleared row on its way to becoming one: cap 0 .. 1), rounding into a sky
    // lantern as it rises (vE.x 0 .. 1: only its front face then, a paper balloon wider at its top)
    float cap = vD.y;
    float a = vC.w;
    float round = vE.x;
    if (face != 0) {
        a *= 1.0 - smoothstep(0.0, 0.5, round);
        if (a < 0.003) discard;
    }
    if (face == 4 || face == 5) {
        // the lid and the base: dark wood once it is a lantern, paper before
        vec3 c = mix(tint * light * 0.55, uWood * (face == 4 ? 0.9 : 0.4) + tint * 0.04 * light, cap);
        frag = vec4(c * a, a);
        return;
    }
    float ex = (0.5 - abs(q.x)) * qs.x;
    float ey = (0.5 - abs(q.y)) * qs.y;
    float frame = 1.0 - smoothstep(0.05 - uPx, 0.05 + uPx, min(ex, ey));
    float capT = (1.0 - smoothstep(0.0, uPx * 2.0, (0.5 - q.y) * qs.y - 0.12 * cap)) * step(0.01, cap);
    float capB = (1.0 - smoothstep(0.0, uPx * 2.0, (q.y + 0.5) * qs.y - 0.09 * cap)) * step(0.01, cap);
    float side = q.x * 2.0;
    float shade = 1.0 - 0.45 * side * side;
    float rib = abs(fract(q.y * 3.0 + 0.5) - 0.5) / 3.0 * qs.y;
    shade *= 1.0 - 0.15 * (1.0 - smoothstep(0.0, 0.02 + uPx, rib)) * cap;
    float hot = exp(-(q.x * q.x * 7.0 + (q.y + 0.1) * (q.y + 0.1) * 6.0));
    float fsh = face == 0 ? 1.0 : 0.72;
    vec3 col = mix(tint, uFlame, 0.3 * hot) * (0.5 + 0.5 * shade + 0.8 * hot) * light * fsh;
    col = max(col, tint * 0.04);
    vec3 wood = uWood * 0.6 + tint * 0.04 * light;
    vec3 c = mix(col, wood, max(frame, max(capT, capB)));
    if (round > 0.0) {
        // the sky lantern: the box's outline rounding into a balloon, wider at its rounded top, its base a dark ring
        // around the flame's glow
        float px = uPx / qs.x;
        float w = mix(0.5, mix(0.31, 0.47, smoothstep(-0.5, 0.25, q.y)), round);
        float ty = mix(0.5, 0.16, round), r = 0.48 - ty;
        float dt = q.y > ty ? length(vec2(q.x, (q.y - ty) * w / max(r, 1e-3))) - w : abs(q.x) - w;
        float cov = 1.0 - smoothstep(-px, px, max(dt, -mix(0.5, 0.46, round) - q.y));
        float sx = q.x / w;
        float glow = exp(-(sx * sx * 1.6 + (q.y + 0.36) * (q.y + 0.36) * 8.0));
        float ribs = 1.0 - 0.1 * (1.0 - smoothstep(0.0, 0.08 + px * 3.0, abs(fract(sx * 1.5 + 0.5) - 0.5)));
        float body = (0.4 + 0.4 * (1.0 - sx * sx)) * (0.7 + 0.3 * smoothstep(0.45, -0.3, q.y));
        vec3 sky = mix(tint, uFlame, 0.3 * glow) * (body + 0.9 * glow) * ribs * light;
        sky = mix(sky, uWood * 0.6 + uFlame * 0.5 * light, (1.0 - smoothstep(-0.44, -0.4, q.y)) * 0.6);
        c = mix(c, sky, round);
        a *= cov;
    }
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- points of light (quads facing the camera)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, z, size
layout(location = 2) in vec4 aB; // color (linear), alpha
uniform mat4 uVP;
out vec2 vQ;
out vec3 vW;
flat out vec4 vB;
void main() {
    vec3 w = aA.xyz + vec3(aPos * aA.w, 0.0);
    vQ = aPos; vW = w; vB = aB;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vW;
flat in vec4 vB;
out vec4 frag;
uniform float uMirror, uWater;
uniform vec3 uFlame;
void main() {
    if (uMirror > 0.5 && vW.y < uWater) discard;
    float r = length(vQ);
    float core = exp(-r * r * 30.0);
    float halo = exp(-r * r * 5.0) * 0.25;
    frag = vec4(mix(vB.rgb, uFlame, core * 0.4) * (core + halo) * vB.w, 0.0);
}
)";

// ---------------------------------------------------------------- the glow
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform vec2 uTexel; // of the scene
void main() {
    vec3 c = vec3(0.0);
    for (int j = -1; j <= 1; j += 2)
        for (int i = -1; i <= 1; i += 2) c += texture(uScene, vUv + vec2(i, j) * uTexel).rgb;
    c *= 0.25;
    float l = max(c.r, max(c.g, c.b));
    c *= smoothstep(0.6, 1.4, l);
    frag = vec4(c, 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uDir; // texel step
void main() {
    vec3 c = texture(uTex, vUv).rgb * 0.227;
    c += (texture(uTex, vUv + uDir * 1.385).rgb + texture(uTex, vUv - uDir * 1.385).rgb) * 0.316;
    c += (texture(uTex, vUv + uDir * 3.23).rgb + texture(uTex, vUv - uDir * 3.23).rgb) * 0.07;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uHorizonV;
uniform vec3 uMist, uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb + texture(uBloom, vUv).rgb * uBloomK;
    // slow motion: moonlight and a mist lying on the lake (never over the pieces)
    float stack = clamp(s.a, 0.0, 1.0);
    float band = exp(-pow((vUv.y - uHorizonV) * 4.0, 2.0)) * 0.9 + smoothstep(0.35, 0.0, vUv.y) * 0.5;
    vec3 cool = c * vec3(0.72, 0.86, 1.12);
    c = mix(c, cool, uSlow * (1.0 - stack * 0.85));
    c += uMist * band * uSlow * (1.0 - stack);
    c = mix(c, mix(uBridgeCol, uBridgeInk, stack), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = c / (1.0 + max(c - 1.0, 0.0)); // a soft shoulder over 1
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

// A fragment shader with the common functions inserted after its version line.
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace lanterns_shaders

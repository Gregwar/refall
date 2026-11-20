#pragma once
// DOCKS' shaders (see docks.cpp).
//
// SKY_FS paints the sky (the time of day's gradient, the sun's glow at the horizon, stars, thin clouds, the far shore's
// hills with the town's lights); WATER_FS the harbor basin (a plane traced per pixel: ripples, the mirrored port from
// the reflection pass, fresnel); BOX_VS / BOX_FS everything solid (instanced boxes: containers with their doors and
// corrugations, painted steel, concrete with its markings, lit buildings, hulls, deck houses, crane cabs), lit by the
// sky, the work lights over the well and the port's lamps, fogged by distance; SPRITE_FS the lamps' glows; LINE_FS
// the cables (screen-space hairlines); VEIL_FS the soft shade behind the well; DOWN_FS / BLUR_FS the glow and
// FINAL_FS the frame.
#include <string>

namespace docks_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform vec3 uZen, uMid, uHor, uGlow, uSunDir, uFogCol;
uniform float uFogD, uHaze, uTime, uSat;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
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
// The sky's gradient in a direction (no stars, no shore): zenith, a middle band, the horizon, the sun's glow.
vec3 skyGrad(vec3 d) {
    float e = d.y;
    float eu = max(e, 0.0);
    vec3 c = mix(uHor, uMid, smoothstep(0.0, 0.16, eu));
    c = mix(c, uZen, smoothstep(0.1, 0.55, eu));
    vec2 sh = normalize(uSunDir.xz + vec2(1e-5));
    vec2 dh = normalize(d.xz + vec2(1e-5));
    float az = dot(sh, dh) * 0.5 + 0.5;
    float g = pow(az, 6.0) * exp(-max(e - uSunDir.y, 0.0) * 9.0) + 0.35 * pow(az, 2.0) * exp(-eu * 14.0);
    c += uGlow * g;
    // below the horizon (the far water's haze)
    c = mix(c, uFogCol * 0.9 + uGlow * 0.15 * pow(az, 4.0), smoothstep(0.0, -0.05, e));
    return c;
}
vec3 fogColor(vec3 d) {
    vec3 h = skyGrad(normalize(vec3(d.x, max(d.y, -0.02) * 0.3 + 0.012, d.z)));
    return mix(uFogCol, h, 0.7);
}
vec3 applyFog(vec3 c, vec3 P) {
    vec3 v = P - uEye;
    float dist = length(v);
    // nothing near the well is hazed: the haze starts across the basin
    float dd = max(dist - 55.0, 0.0);
    float f = 1.0 - exp(-dd * uFogD);
    // the low haze over the water is thicker
    f = max(f, (1.0 - exp(-dd * uFogD * 2.0)) * uHaze * 0.5 * smoothstep(2.0, -13.0, P.y));
    return mix(c, fogColor(v / dist), clamp(f, 0.0, 1.0));
}
vec3 saturateC(vec3 c, float k) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return max(mix(vec3(l), c, k), 0.0);
}
)";

inline const char* LIGHTING = R"(
uniform vec3 uAmbSky, uAmbGround, uKeyDir, uKeyCol, uSunCol;
uniform float uKeyFar;
uniform int uNL;
uniform vec4 uLP[16];
uniform vec4 uLC[16];
// The work lights over the well (a directional from above and in front of it), only near the well.
float keyMask(vec3 P) {
    return mix(uKeyFar, 1.0, smoothstep(-34.0, -8.0, P.z) * (1.0 - smoothstep(18.0, 34.0, abs(P.x))));
}
vec3 shade(vec3 P, vec3 N, vec3 alb, float spec, float rough) {
    vec3 V = normalize(uEye - P);
    vec3 c = alb * mix(uAmbGround, uAmbSky, 0.5 + 0.5 * N.y);
    float km = keyMask(P);
    float nk = max(dot(N, uKeyDir), 0.0);
    c += alb * uKeyCol * nk * km;
    vec3 H = normalize(uKeyDir + V);
    float sp = pow(max(dot(N, H), 0.0), rough) * spec;
    c += uKeyCol * sp * km * 0.5;
    // the sky's low light (the sun gone, the moon): a soft rim from behind
    c += alb * uSunCol * max(dot(N, uSunDir), 0.0);
    for (int i = 0; i < 16; i++) {
        if (i >= uNL) break;
        vec3 L = uLP[i].xyz - P;
        float d2 = dot(L, L);
        float r = uLP[i].w;
        if (d2 > r * r) continue;
        float d = sqrt(d2);
        L /= d;
        float att = 1.0 / (1.0 + d2 / (r * r * 0.12)) * (1.0 - smoothstep(0.45 * r, r, d));
        float nl = max(dot(N, L), 0.0) * 0.85 + 0.15 * max(N.y, 0.0);
        c += alb * uLC[i].rgb * uLC[i].a * att * nl;
        vec3 Hl = normalize(L + V);
        c += uLC[i].rgb * uLC[i].a * att * pow(max(dot(N, Hl), 0.0), rough) * spec * 0.6;
    }
    return c;
}
)";

// ---------------------------------------------------------------- the sky (full screen)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uMirror, uStars, uMoon, uSeed, uShoreLights, uClouds;
uniform vec3 uMoonDir, uShore, uTown;
vec3 rayDir(vec2 uv) {
    vec4 a = uInvVP * vec4(uv * 2.0 - 1.0, -1.0, 1.0), b = uInvVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(b.xyz / b.w - a.xyz / a.w);
}
void main() {
    vec3 d = rayDir(vUv);
    if (uMirror > 0.5) d.y = -d.y;
    float e = d.y;
    vec3 c = skyGrad(d);
    float az = atan(d.x, -d.z);
    // stars, fading toward the horizon's glow
    if (uStars > 0.0 && e > 0.02) {
        vec2 g = vec2(az * 260.0, e * 260.0);
        vec2 cell = floor(g);
        float h = hash12(cell + uSeed);
        vec2 f = fract(g) - 0.5 - (vec2(hash12(cell + 7.1), hash12(cell + 3.3)) - 0.5) * 0.6;
        float s = smoothstep(0.985, 1.0, h) * smoothstep(0.09, 0.0, length(f)) * smoothstep(0.02, 0.25, e);
        float tw = 0.75 + 0.25 * sin(uTime * (1.0 + 3.0 * hash12(cell)) + h * 30.0);
        c += vec3(0.85, 0.9, 1.0) * s * uStars * tw;
    }
    // the moon, low over the shore
    if (uMoon > 0.0) {
        float m = dot(d, uMoonDir);
        float disc = smoothstep(0.99993, 0.99996, m);
        c += vec3(1.0, 0.95, 0.85) * disc * uMoon * 2.2 + vec3(0.6, 0.65, 0.8) * pow(max(m, 0.0), 2000.0) * uMoon * 0.3;
    }
    // thin bands of cloud, lit by the glow
    if (uClouds > 0.0 && e > -0.01) {
        vec2 p = vec2(az * 3.0, 1.0 / (e + 0.08)) * vec2(1.0, 0.9) + vec2(uTime * 0.004, 0.0);
        float n = fbm(p * vec2(2.2, 0.55) + uSeed);
        float band = smoothstep(0.5, 0.78, n) * smoothstep(0.0, 0.06, e) * (1.0 - smoothstep(0.25, 0.5, e));
        vec2 sh = normalize(uSunDir.xz + vec2(1e-5));
        float sa = pow(dot(sh, normalize(d.xz + vec2(1e-5))) * 0.5 + 0.5, 3.0);
        vec3 cc = mix(uMid * 0.75, uGlow * 1.2 + uHor * 0.4, sa * 0.8);
        c = mix(c, cc, band * uClouds);
    }
    // the far shore: low hills, the town's lights
    float hz = 0.0045 + 0.012 * fbm(vec2(az * 5.0 + uSeed, 0.0)) + 0.008 * smoothstep(0.3, 0.9, fbm(vec2(az * 13.0 - uSeed, 2.0)));
    if (e < hz && e > -0.004) {
        float k = smoothstep(hz, hz - 0.0015, e);
        vec3 sh = mix(uShore, skyGrad(vec3(d.x, 0.0, d.z)), 0.35);
        c = mix(c, sh, k);
        vec2 g = vec2(az * 900.0, e * 2600.0);
        vec2 cell = floor(g);
        float h = hash12(cell + uSeed * 3.0);
        float town = smoothstep(0.35, 0.75, fbm(vec2(az * 9.0, 1.0) + uSeed));
        float lit = step(1.0 - 0.18 * town * uShoreLights, h) * smoothstep(0.35, 0.0, length(fract(g) - 0.5)) * k;
        c += uTown * lit * (0.6 + 0.6 * hash12(cell + 1.7));
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the water (full screen, traced)
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uVP;
uniform sampler2D uRefl;
uniform float uWaterY, uRipple, uReflK;
uniform vec3 uDeep, uAmbSky;
uniform vec2 uWake[2];   // the ships' wakes (x, strength)
vec3 rayDir(vec2 uv) {
    vec4 a = uInvVP * vec4(uv * 2.0 - 1.0, -1.0, 1.0), b = uInvVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(b.xyz / b.w - a.xyz / a.w);
}
float waves(vec2 p) {
    float t = uTime;
    float h = 0.0;
    h += 0.5 * sin(p.y * 0.9 + t * 0.9 + 1.7 * sin(p.x * 0.07));
    h += 0.35 * sin(p.y * 1.7 - p.x * 0.25 + t * 1.3);
    h += 0.25 * sin(p.y * 2.9 + p.x * 0.4 - t * 1.7);
    h += 0.6 * (vnoise(vec2(p.x * 0.25, p.y * 1.4) + vec2(t * 0.05, t * 0.4)) - 0.5);
    h += 0.3 * (vnoise(vec2(p.x * 0.6, p.y * 3.1) - vec2(t * 0.1, t * 0.7)) - 0.5);
    return h;
}
void main() {
    vec3 d = rayDir(vUv);
    if (d.y > -1e-4) discard;
    float t = (uWaterY - uEye.y) / d.y;
    vec3 P = uEye + d * t;
    vec2 w = P.xz;
    float e = 0.05;
    float h0 = waves(w), hx = waves(w + vec2(e, 0.0)), hz = waves(w + vec2(0.0, e));
    float amp = uRipple * 0.05;
    // the ripples fade out with distance (no aliasing far away)
    float fade = 1.0 / (1.0 + t * 0.012);
    vec3 N = normalize(vec3(-(hx - h0) / e * amp * fade, 1.0, -(hz - h0) / e * amp * fade));
    // the mirrored port, shifted by the ripples (mostly up and down: long streaks of light)
    vec2 uv = vUv + vec2(N.x * 0.25, N.z * 1.6) * 0.06 * fade;
    vec3 refl = texture(uRefl, uv).rgb * 0.5;
    refl += texture(uRefl, uv + vec2(0.0, 0.004)).rgb * 0.25 + texture(uRefl, uv - vec2(0.0, 0.004)).rgb * 0.25;
    float cosT = clamp(dot(-d, N), 0.0, 1.0);
    float fres = 0.03 + 0.97 * pow(1.0 - cosT, 5.0);
    vec3 c = uDeep * (0.5 + 0.5 * uAmbSky * 4.0);
    c = mix(c, refl, clamp(uReflK * (0.35 + fres), 0.0, 1.0));
    // a ship's wake: churned white water behind its stern
    for (int i = 0; i < 2; i++) {
        if (uWake[i].y <= 0.0) continue;
        float dx = abs(P.x - uWake[i].x), dz = abs(P.z + 54.5);
        float foam = smoothstep(9.0, 0.0, dz) * exp(-dx * 0.05) * smoothstep(0.55, 0.85, vnoise(P.xz * vec2(0.5, 1.2) + uTime * 0.6));
        c += vec3(0.5, 0.55, 0.6) * foam * uWake[i].y * 0.25;
    }
    c = applyFog(c, P);
    c = saturateC(c, uSat);
    vec4 cp = uVP * vec4(P, 1.0);
    gl_FragDepth = clamp(cp.z / cp.w * 0.5 + 0.5, 0.0, 1.0);
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- solids (instanced boxes)
inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
uniform mat4 uVP;
out vec3 vP, vL;
flat out vec3 vLN, vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat;
void main() {
    vec3 P = iC.xyz + iX.xyz * aPos.x + iY.xyz * aPos.y + iZ.xyz * aPos.z;
    vHalf = vec3(length(iX.xyz), length(iY.xyz), length(iZ.xyz));
    vAX = iX.xyz / max(vHalf.x, 1e-6);
    vAY = iY.xyz / max(vHalf.y, 1e-6);
    vAZ = iZ.xyz / max(vHalf.z, 1e-6);
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    gl_Position = uVP * vec4(P, 1.0);
}
)";
inline const char* BOX_FS = R"(#version 330 core
in vec3 vP, vL;
flat in vec3 vLN, vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat;
out vec4 frag;
uniform float uClipY, uWaterY, uStackA, uDeckLights;
uniform vec3 uWinCol, uQuayLine;
uniform float uBay; // the well's footprint painted on the quay (0: the world alone, a break)
uniform int uBlock;     // the well's containers' look (1..5, see mat 10)
uniform float uCellGap; // the gap round a piece (cell units)
uniform vec3 uSodium;   // the port's lamps' light, on the ribs (look 5)
// A trapezoidal corrugation along x (period 1): x its height (0 inner .. 1 outer), y its slope (1 rising, -1 falling).
vec2 corrug(float x) {
    float f = fract(x);
    return vec2(smoothstep(0.0, 0.2, f) - smoothstep(0.5, 0.7, f), f < 0.2 ? 1.0 : f < 0.5 ? 0.0 : f < 0.7 ? -1.0 : 0.0);
}
void main() {
    if (vP.y < uClipY) discard;
    int mat = int(vMat.x + 0.5);
    vec2 uv, sz;
    vec3 T, B, N;
    int face; // 0 x sides, 1 top / bottom, 2 ends
    if (abs(vLN.x) > 0.5) { uv = vL.zy; sz = vHalf.zy; T = vAZ; B = vAY; N = vAX * sign(vLN.x); face = 0; }
    else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; N = vAY * sign(vLN.y); face = 1; }
    else { uv = vL.xy; sz = vHalf.xy; T = vAX; B = vAY; N = vAZ * sign(vLN.z); face = 2; }
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.12, rough = 24.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    vec3 sheen = vec3(0.0); // light added after the stack's paint (look 5)
    float px = max(fwidth(q.x), fwidth(q.y));
    if (mat == 1) {
        // a shipping container: corner posts and rails, corrugated walls, the doors at its ends
        float detail = 1.0 - smoothstep(0.012, 0.04, px);
        float edge = min(sz.x - abs(q.x), sz.y - abs(q.y));
        float lp = 0.5 + 0.5 * sin(seed * 40.0);
        if (face == 1) {
            // the roof: faint dents
            alb *= 0.96 + 0.04 * sin(q.x * 9.0 + seed * 7.0) * detail;
        } else if (face == 2 && vMat.y < 0.5) {
            // the doors: two leaves, their locking rods, horizontal panels
            float xr = abs(q.x) / sz.x;
            bend.y = 0.3 * sin(q.y / sz.y * 3.14159 * 3.5) * detail;
            float rd = min(abs(xr - 0.24), abs(xr - 0.64)) * sz.x;
            float rod = (1.0 - smoothstep(0.012, 0.02 + px, rd)) * smoothstep(sz.y - 0.07, sz.y - 0.1, abs(q.y));
            float sh = (1.0 - smoothstep(0.0, 0.03 + px, rd - 0.02)) * 0.25 * detail;
            alb *= 1.0 - sh;
            alb = mix(alb, alb * 1.3 + 0.03, rod * mix(0.3, 0.8, detail));
            // the handles
            float hy = abs(q.y + sz.y * 0.12);
            float hx = abs(xr - 0.24) < 0.12 || abs(xr - 0.64) < 0.12 ? 1.0 : 0.0;
            float handle = hx * (1.0 - smoothstep(0.015, 0.025 + px, hy)) * step(0.0, (xr - 0.24) * sign(q.x) * 0.0 + 1.0);
            alb = mix(alb, vec3(0.62) * alb + 0.12, handle * 0.8 * detail);
            float seam = 1.0 - smoothstep(0.006, 0.012 + px, abs(q.x));
            alb *= 1.0 - 0.6 * seam;
        } else {
            // the walls: trapezoidal corrugation
            float pitch = 0.14;
            float ph = fract(q.x / pitch);
            float prof = smoothstep(0.05, 0.15, ph) - smoothstep(0.45, 0.55, ph) - (smoothstep(0.55, 0.65, ph) - smoothstep(0.95, 1.0, ph));
            float s = (ph < 0.15 ? 1.0 : ph < 0.45 ? 0.0 : ph < 0.65 ? -1.0 : 0.0);
            bend.x = 0.45 * s * detail;
            alb *= 1.0 - 0.06 * (1.0 - prof) * detail;
        }
        // posts and rails, a little darker; the corner castings
        float frame = 1.0 - smoothstep(0.05, 0.06 + px, edge);
        alb = mix(alb, alb * 0.62, frame * mix(0.5, 1.0, detail));
        vec2 cq = sz - abs(q);
        float castg = step(cq.x, 0.09) * step(cq.y, 0.08);
        alb = mix(alb, vec3(0.16, 0.16, 0.17) + alb * 0.2, castg * 0.85 * detail);
        // a pale bevel of light along the top edge of the doors and walls
        if (face != 1) alb += alb * 0.35 * (1.0 - smoothstep(0.0, 0.02 + px, sz.y - q.y)) * detail;
        // grime, very light, toward the bottom
        alb *= 0.93 + 0.07 * smoothstep(-sz.y, sz.y * 0.4, q.y);
        spec = 0.22;
        rough = 30.0;
        // contact shade: the light is occluded toward the foot of each container (the one below, the quay), the sides
        // in the shade
        // (the well's stack, on its dark hull, keeps more of its paint)
        float stack = step(0.5, uStackA);
        if (face != 1) alb *= mix(mix(0.42, 0.62, stack), 1.0, smoothstep(-sz.y, -sz.y + 0.4, q.y));
        if (face == 0) alb *= 0.6;
        alb = saturateC(alb, mix(uSat, 1.0, stack));
    } else if (mat == 10) {
        // the well's containers in an alternative look (uBlock 1..5): vMat.y packs the cell's links to its piece (L R U
        // D UL UR DL DR) and its place in its row of the piece, vMat.z the piece's seed, vMat.w the cell's size
        int m = int(vMat.y + 0.5);
        int mask = m & 255, runI = (m >> 8) & 3, runN = max((m >> 10) & 7, 1);
        float S = max(vMat.w, 1e-3), ps = vMat.z;
        float g = uCellGap;
        bool jL = (mask & 1) != 0, jR = (mask & 2) != 0, jU = (mask & 4) != 0, jD = (mask & 8) != 0;
        bool jUL = (mask & 16) != 0, jUR = (mask & 32) != 0, jDL = (mask & 64) != 0, jDR = (mask & 128) != 0;
        vec2 gA = vec2(jL ? 0.0 : g, jD ? 0.0 : g), gB = vec2(jR ? 0.0 : g, jU ? 0.0 : g); // the gaps left/bottom, right/top
        vec2 off = 0.5 * (gA - gB);
        // c: the cell's own frame (cell units, -0.5 .. 0.5, y up); the sides and the roof read their edge's
        vec2 c;
        if (face == 2) c = off + q / S;
        else if (face == 0) c = vec2(vLN.x > 0.0 ? 0.5 - gB.x : -0.5 + gA.x, off.y + q.y / S);
        else c = vec2(off.x + q.x / S, vLN.y > 0.0 ? 0.5 - gB.y : -0.5 + gA.y);
        float pw = px / S;
        // the distance to the piece's outline (round its inner corners), to the cell's own edges
        float od = min(min(jL ? 9.0 : c.x + 0.5 - g, jR ? 9.0 : 0.5 - g - c.x), min(jU ? 9.0 : 0.5 - g - c.y, jD ? 9.0 : c.y + 0.5 - g));
        if (jL && jU && !jUL) od = min(od, length(max(vec2(c.x + 0.5 - g, 0.5 - g - c.y), 0.0)));
        if (jR && jU && !jUR) od = min(od, length(max(vec2(0.5 - g - c.x, 0.5 - g - c.y), 0.0)));
        if (jL && jD && !jDL) od = min(od, length(max(vec2(c.x + 0.5 - g, c.y + 0.5 - g), 0.0)));
        if (jR && jD && !jDR) od = min(od, length(max(vec2(0.5 - g - c.x, c.y + 0.5 - g), 0.0)));
        float cl = c.x + 0.5 - gA.x, cr = 0.5 - gB.x - c.x, cb = c.y + 0.5 - gA.y, ct = 0.5 - gB.y - c.y;
        float ce = min(min(cl, cr), min(cb, ct));
        float rib = 1.0 - smoothstep(0.04, 0.08, pw);    // the fine detail fades out when small (the previews)
        vec3 base = alb;
        float lum = dot(base, vec3(0.2126, 0.7152, 0.0722));
        spec = 0.2;
        rough = 30.0;
        if (face == 2) {
            if (uBlock == 1) {
                // container ends: a frame of posts, header and sill; two corrugated door leaves, their locking bars,
                // cam keepers and handles; corner castings; a faint stencilled number
                vec2 cg = corrug(c.x / 0.105 + 0.3);
                bend.x = 0.55 * cg.y * rib;
                alb *= mix(1.0, 0.86 + 0.14 * cg.x, rib);
                float frame = max(1.0 - smoothstep(0.07, 0.07 + pw, min(cl, cr)), max(1.0 - smoothstep(0.085, 0.085 + pw, ct),
                                  1.0 - smoothstep(0.065, 0.065 + pw, cb)));
                bend.x *= 1.0 - frame;
                alb = mix(alb, base * 0.62, frame);
                alb += base * 0.4 * (1.0 - smoothstep(0.0, 0.015 + pw, ct));      // the header's lit lip
                // the leaves' seam
                alb *= 1.0 - 0.55 * (1.0 - smoothstep(0.006, 0.006 + pw, abs(c.x))) * (1.0 - frame);
                // the locking bars: two per leaf
                float bx = min(abs(abs(c.x) - 0.115), abs(abs(c.x) - 0.3));
                float inDoor = 1.0 - frame;
                float bar = (1.0 - smoothstep(0.013, 0.013 + pw, bx)) * inDoor;
                float barSh = (1.0 - smoothstep(0.0, 0.02 + pw, bx - 0.013)) * step(0.0, (abs(c.x) - 0.115) * sign(c.x)) * inDoor;
                alb *= 1.0 - 0.3 * barSh * rib;
                alb = mix(alb, vec3(0.42, 0.42, 0.4) * 0.5 + base * 0.35, bar * mix(0.5, 1.0, rib));
                // the cam keepers (top and bottom of each bar) and the handles
                float kx = bx, ky = abs(abs(c.y) - 0.34);
                float keeper = (1.0 - smoothstep(0.03, 0.03 + pw, kx)) * (1.0 - smoothstep(0.022, 0.022 + pw, ky)) * inDoor;
                float hy = abs(c.y + 0.06), hx = abs(abs(c.x) - 0.115 + 0.05) ;
                float hx2 = abs(abs(c.x) - 0.3 + 0.05);
                float handle = (1.0 - smoothstep(0.016, 0.016 + pw, hy)) * (1.0 - smoothstep(0.05, 0.05 + pw, min(hx, hx2))) * inDoor;
                alb = mix(alb, vec3(0.05, 0.05, 0.055), keeper * rib);
                alb = mix(alb, vec3(0.3, 0.3, 0.29) * 0.6, handle * rib);
                // the stencil: two short rows of characters on the right leaf
                vec2 sp = vec2((c.x - 0.04) / 0.032, (c.y - 0.2) / 0.05);
                float row = floor(sp.y), col = floor(sp.x);
                float glyph = step(0.0, sp.x) * step(col, row < 0.5 ? 6.0 : 3.0) * step(0.0, sp.y) * step(sp.y, 1.9)
                    * step(0.3, hash12(vec2(col, row) + seed * 37.0)) * step(0.2, fract(sp.x)) * step(fract(sp.y), 0.62);
                alb = mix(alb, vec3(0.85), glyph * 0.45 * rib);
                // the corner castings: dark steel, an oval hole
                vec2 cc = vec2(min(cl, cr), min(cb, ct));
                float cast = (1.0 - smoothstep(0.11, 0.11 + pw, cc.x)) * (1.0 - smoothstep(0.095, 0.095 + pw, cc.y));
                vec2 ho = (cc - vec2(0.055, 0.048)) / vec2(0.032, 0.02);
                alb = mix(alb, vec3(0.07, 0.07, 0.075) + base * 0.08, cast);
                alb *= 1.0 - 0.85 * cast * (1.0 - smoothstep(0.8, 1.0, length(ho))) * rib;
                // grime: a little, low
                alb *= 0.9 + 0.12 * fbm(c * 3.0 + seed * 11.0) ;
                alb *= 0.82 + 0.18 * smoothstep(-0.5, 0.1, c.y);
                // the piece: its outline in shadow, a hair of dark between its containers
                alb *= mix(0.12, 1.0, smoothstep(0.0, 0.065, od));
                alb *= 1.0 - 0.5 * (1.0 - smoothstep(0.0, 0.01 + pw, ce));
                spec = 0.25;
            } else if (uBlock == 2) {
                // the long sides: a row of the piece one container (2 cells a 20 ft, 4 a 40 ft), its corrugation
                // running on, posts and castings at its ends only; rails top and bottom
                float rx = float(runI) + c.x + 0.5;                 // along the container, 0 .. runN
                float rl = jL ? 9.0 : cl, rr = jR ? 9.0 : cr;
                float endD = min(rl, rr);
                vec2 cg = corrug(rx / 0.1);
                float rails = max(1.0 - smoothstep(0.075, 0.075 + pw, ct), 1.0 - smoothstep(0.075, 0.075 + pw, cb));
                float posts = 1.0 - smoothstep(0.06, 0.06 + pw, endD);
                float frame = max(rails, posts);
                bend.x = 0.7 * cg.y * rib * (1.0 - frame);
                alb *= mix(1.0, 0.74 + 0.26 * cg.x, rib * (1.0 - frame));
                alb = mix(alb, base * 0.6, frame);
                alb += base * 0.45 * (1.0 - smoothstep(0.0, 0.016 + pw, ct));     // the top rail's lit lip
                alb *= 1.0 - 0.3 * (1.0 - smoothstep(0.0, 0.03 + pw, cb - 0.075)) * (1.0 - frame); // under the bottom rail's lip
                bend.y = 0.5 * (1.0 - smoothstep(0.0, 0.02, ct - 0.06)) * step(ct, 0.075);
                // the corner castings, at the ends only
                vec2 cc = vec2(endD, min(cb, ct));
                float cast = (1.0 - smoothstep(0.1, 0.1 + pw, cc.x)) * (1.0 - smoothstep(0.095, 0.095 + pw, cc.y));
                vec2 ho = (cc - vec2(0.05, 0.048)) / vec2(0.03, 0.02);
                alb = mix(alb, vec3(0.07, 0.07, 0.075) + base * 0.08, cast);
                alb *= 1.0 - 0.85 * cast * (1.0 - smoothstep(0.8, 1.0, length(ho))) * rib;
                // the lettering: the owner's name along a long one, the number near its right end
                float N = float(runN);
                if (runN >= 2) {
                    float lx = rx - N * 0.5;
                    // block capitals: each a left stem and, by the piece's seed, a top, middle, bottom bar, a right stem
                    float nl = runN >= 4 ? 6.0 : runN == 3 ? 5.0 : 4.0;
                    float lw = 0.15, lh = 0.2;
                    vec2 lp = vec2((lx + nl * lw * 0.5) / lw, (c.y + lh * 0.5) / lh);
                    float li = floor(lp.x);
                    vec2 lf = vec2(fract(lp.x) / 0.72, lp.y);
                    float st = 0.2;
                    float h0 = hash12(vec2(li, 1.7) + ps * 91.0), h1 = hash12(vec2(li, 4.3) + ps * 91.0);
                    float stroke = step(lf.x, st);
                    stroke = max(stroke, step(0.5, h0) * step(1.0 - st * 0.7, lf.y));
                    stroke = max(stroke, step(0.3, h1) * step(abs(lf.y - 0.5), st * 0.4) * step(lf.x, 0.85));
                    stroke = max(stroke, step(h0, 0.75) * step(lf.y, st * 0.7));
                    stroke = max(stroke, step(0.6, h1) * step(1.0 - st, lf.x));
                    float letter = step(0.0, lp.x) * step(li, nl - 1.0) * step(0.0, lf.y) * step(lf.y, 1.0) * step(lf.x, 1.0) * stroke;
                    alb = mix(alb, vec3(0.86, 0.86, 0.84), letter * 0.55 * rib);
                } else {
                    // a short one: its doors' bars (it is seen end on)
                    float bx = min(abs(abs(c.x) - 0.12), abs(abs(c.x) - 0.3));
                    float bar = (1.0 - smoothstep(0.013, 0.013 + pw, bx)) * (1.0 - frame);
                    alb = mix(alb, vec3(0.2) + base * 0.3, bar * rib);
                    alb *= 1.0 - 0.55 * (1.0 - smoothstep(0.006, 0.006 + pw, abs(c.x))) * (1.0 - frame);
                }
                vec2 sp = vec2((rx - (N - 0.42)) / 0.03, (c.y - 0.22) / 0.05);
                float glyph = step(0.0, sp.x) * step(sp.x, 9.0) * step(0.0, sp.y) * step(sp.y, 1.9)
                    * step(0.3, hash12(floor(sp) + ps * 37.0)) * step(0.2, fract(sp.x)) * step(fract(sp.y), 0.62);
                alb = mix(alb, vec3(0.85), glyph * 0.4 * rib);
                alb *= 0.9 + 0.12 * fbm(vec2(rx, c.y) * 3.0 + ps * 11.0);
                // the piece's outline in shadow; a dark seam between two containers stacked
                alb *= mix(0.12, 1.0, smoothstep(0.0, 0.065, od));
                alb *= 1.0 - 0.6 * (1.0 - smoothstep(0.0, 0.012 + pw, min(cb, ct)));
                alb *= 1.0 - 0.6 * (1.0 - smoothstep(0.0, 0.012 + pw, endD));
            } else if (uBlock == 3) {
                // flat, bold: clean flat ribs, a few graphic details, a crisp ink outline round each piece
                float pitch = 0.1;
                float fr = fract(c.x / pitch + 0.25);
                float band = smoothstep(0.5 - pw / pitch, 0.5 + pw / pitch, fr) - smoothstep(1.0 - pw / pitch, 1.0, fr);
                alb *= 1.0 - 0.1 * band * rib;
                float frame = max(1.0 - smoothstep(0.06, 0.06 + pw, min(cl, cr)), max(1.0 - smoothstep(0.07, 0.07 + pw, ct),
                                  1.0 - smoothstep(0.06, 0.06 + pw, cb)));
                alb = mix(alb, base * 0.72, frame);
                // two door bars
                float bar = (1.0 - smoothstep(0.02, 0.02 + pw, abs(abs(c.x) - 0.17))) * (1.0 - frame);
                alb = mix(alb, base * 0.6, bar);
                // the castings: small dark squares
                vec2 cc = vec2(min(cl, cr), min(cb, ct));
                float cast = (1.0 - smoothstep(0.085, 0.085 + pw, cc.x)) * (1.0 - smoothstep(0.08, 0.08 + pw, cc.y));
                alb = mix(alb, vec3(0.04, 0.04, 0.05), cast * 0.9);
                // between the containers of a piece a thin line, round the piece the ink
                alb = mix(alb, base * 0.45, 1.0 - smoothstep(0.008, 0.008 + pw, ce));
                alb = mix(alb, vec3(0.012, 0.012, 0.02), 1.0 - smoothstep(0.05, 0.05 + pw, od));
                spec = 0.06;
            } else if (uBlock == 4) {
                // weathered: sun-faded paint, rust streaks running down from the rails and castings, rust patches,
                // scratches, dents, a repainted patch; each container its own by its seed
                vec2 cg = corrug(c.x / 0.11 + 0.3);
                float frame = max(1.0 - smoothstep(0.07, 0.07 + pw, min(cl, cr)), max(1.0 - smoothstep(0.08, 0.08 + pw, ct),
                                  1.0 - smoothstep(0.065, 0.065 + pw, cb)));
                // faded toward the top, more on some
                float fade = (0.15 + 0.35 * seed) * smoothstep(-0.6, 0.5, c.y);
                alb = mix(alb, vec3(lum) * 1.1 + 0.035, fade);
                // a repainted patch, a near shade
                vec2 pc = vec2(hash12(vec2(seed, 3.1)), hash12(vec2(seed, 7.7))) * 0.5 - 0.25;
                float patch = step(0.55, hash12(vec2(seed, 1.3))) * step(abs(c.x - pc.x), 0.14) * step(abs(c.y - pc.y), 0.1);
                alb = mix(alb, base * vec3(1.05, 0.95, 0.9) * 0.86, patch);
                // dents: a few, small
                vec2 dp = c * 5.0 + seed * 17.0;
                float d0 = vnoise(dp);
                float dent = smoothstep(0.5, 0.8, vnoise(c * 2.5 + seed * 31.0));
                bend += vec2(vnoise(dp + vec2(0.03, 0.0)) - d0, vnoise(dp + vec2(0.0, 0.03)) - d0) / 0.03 * 0.18 * dent * rib;
                bend.x += 0.5 * cg.y * rib * (1.0 - frame);
                alb *= mix(1.0, 0.85 + 0.15 * cg.x, rib * (1.0 - frame));
                alb = mix(alb, alb * 0.66, frame);
                // the doors' seam and bars
                alb *= 1.0 - 0.5 * (1.0 - smoothstep(0.006, 0.006 + pw, abs(c.x))) * (1.0 - frame);
                float bx = min(abs(abs(c.x) - 0.12), abs(abs(c.x) - 0.31));
                float bar = (1.0 - smoothstep(0.013, 0.013 + pw, bx)) * (1.0 - frame);
                alb = mix(alb, vec3(0.16, 0.08, 0.05), bar * rib);
                // rust: streaks down from the header (and the bars' keepers), patches at the edges and the foot
                const vec3 RUST = vec3(0.30, 0.085, 0.025), RUSTD = vec3(0.09, 0.03, 0.012);
                float sx = vnoise(vec2(c.x * 34.0 + seed * 40.0, 0.5));
                float run = smoothstep(0.5, 0.9, sx) * exp(-max(0.42 - c.y, 0.0) * mix(1.5, 5.0, vnoise(vec2(c.x * 9.0, seed * 5.0))));
                run += (1.0 - smoothstep(0.0, 0.03, bx)) * smoothstep(0.1, 0.3, c.y) * 0.5;
                float rustP = smoothstep(0.66, 0.8, fbm(c * 5.0 + seed * 23.0) + 0.22 * (1.0 - smoothstep(0.0, 0.12, ce))
                                         + 0.2 * (1.0 - smoothstep(-0.5, -0.3, c.y)) - 0.12 * seed);
                alb = mix(alb, mix(RUST, alb * 0.6, 0.2), clamp(run * 0.9, 0.0, 1.0) * rib);
                alb = mix(alb, mix(RUST, RUSTD, smoothstep(0.7, 0.95, fbm(c * 9.0 + seed * 5.0))), rustP);
                // scratches: bare steel
                for (int i = 0; i < 3; i++) {
                    vec2 a = vec2(hash12(vec2(seed * 13.0, float(i))), hash12(vec2(float(i), seed * 7.0))) - 0.5;
                    float an = hash12(vec2(seed + float(i), 2.0)) * 3.14159;
                    vec2 dir = vec2(cos(an), sin(an));
                    vec2 r = c - a;
                    float along = dot(r, dir), across = abs(dot(r, vec2(-dir.y, dir.x)));
                    float sc = (1.0 - smoothstep(0.004, 0.004 + pw, across)) * step(abs(along), 0.12);
                    alb = mix(alb, vec3(0.32, 0.3, 0.28), sc * 0.7 * rib);
                }
                // castings, rusted
                vec2 cc = vec2(min(cl, cr), min(cb, ct));
                float cast = (1.0 - smoothstep(0.11, 0.11 + pw, cc.x)) * (1.0 - smoothstep(0.095, 0.095 + pw, cc.y));
                alb = mix(alb, mix(vec3(0.06), RUSTD, 0.6), cast);
                alb *= 0.85 + 0.2 * fbm(c * 2.0 + seed * 3.0);
                // the piece: a strong dark outline, its containers apart by a seam
                alb *= mix(0.12, 1.0, smoothstep(0.01, 0.06, od));
                alb *= 1.0 - 0.6 * (1.0 - smoothstep(0.0, 0.012 + pw, ce));
                spec = 0.12;
                rough = 14.0;
            } else {
                // under the port's lights: painted steel with a sheen; the ribs catch the sodium light, a highlight
                // sweeps along the stack; the gaps between the pieces in deep shadow
                vec2 cg = corrug(c.x / 0.125 + 0.3);
                float frame = max(1.0 - smoothstep(0.06, 0.06 + pw, min(cl, cr)), max(1.0 - smoothstep(0.07, 0.07 + pw, ct),
                                  1.0 - smoothstep(0.06, 0.06 + pw, cb)));
                bend.x = 0.8 * cg.y * rib * (1.0 - frame);
                alb *= 0.5 * mix(1.0, 0.6 + 0.4 * cg.x, rib * (1.0 - frame));
                alb = mix(alb, base * 0.3, frame);
                // ambient occlusion toward the piece's outline (deep gaps), a seam between its containers
                float ao = mix(0.12, 1.0, smoothstep(0.0, 0.2, od));
                alb *= ao;
                alb *= 1.0 - 0.75 * (1.0 - smoothstep(0.0, 0.014 + pw, ce));
                // the sodium light: on the flanks of the ribs facing it (brighter up the stack, toward the lamps), the
                // header's lip, the posts' edges; and a highlight sweeping along the stack
                vec3 W = vP;
                float ph = fract((W.x * 0.7 + W.y * 0.35 - uTime * 2.2) / 18.0) - 0.5;
                float sweep = exp(-ph * ph * 18.0 * 18.0 / 3.0);
                float up = 0.55 + 0.45 * smoothstep(-9.0, 9.0, W.y);
                float flank = max(cg.y, 0.0) * rib * (1.0 - frame);
                float crest = (1.0 - abs(cg.y)) * cg.x * rib * (1.0 - frame);
                float lip = (1.0 - smoothstep(0.0, 0.022 + pw, ct)) * smoothstep(0.06, 0.07 + pw, min(cl, cr));
                vec3 tint = mix(base * 1.8, vec3(1.0), 0.5);
                sheen = uSodium * tint * (flank * (0.4 * up + 2.4 * sweep) + crest * (0.05 + 0.8 * sweep) + lip * (0.6 + 1.5 * sweep)) * ao;
                spec = 0.9;
                rough = 70.0;
            }
        } else if (face == 0) {
            // the long sides seen at the edges of the stack: corrugated, in the shade
            if (uBlock == 3) alb = vec3(0.012, 0.012, 0.02);
            else {
                vec2 cg = corrug(q.x / S / 0.1);
                bend.x = 0.0;
                alb *= (0.86 + 0.14 * cg.x) * 0.5;
                alb *= mix(0.4, 1.0, smoothstep(0.0, 0.06, min(cb, ct)));
                if (uBlock == 4) alb = mix(alb, vec3(0.1, 0.03, 0.012), smoothstep(0.55, 0.7, fbm(vec2(q.x / S, c.y) * 5.0 + seed * 9.0)));
            }
        } else {
            // the roof
            if (uBlock == 3) alb = mix(base * 0.8, vec3(0.012, 0.012, 0.02), 1.0 - smoothstep(0.05, 0.05 + pw, min(cl, cr)));
            else {
                alb *= 0.8 + 0.08 * sin(c.x * 60.0 + seed * 7.0) * rib;
                alb *= mix(0.4, 1.0, smoothstep(0.0, 0.06, min(cl, cr)));
            }
        }
    } else if (mat == 2) {
        // concrete: slabs, tyre marks, the painted lines
        vec2 g = vP.xz;
        float n = fbm(g * 0.35);
        alb *= 0.82 + 0.25 * n;
        if (face == 1 && N.y > 0.5) {
            vec2 sl = abs(fract(g / 6.0) - 0.5) * 6.0;
            float joint = 1.0 - smoothstep(0.02, 0.05 + px, min(3.0 - sl.x, 3.0 - sl.y));
            alb *= 1.0 - 0.25 * joint;
            alb *= 1.0 - 0.12 * smoothstep(0.55, 0.8, vnoise(vec2(g.x * 0.08, g.y * 1.6)));
            // lines: vMat.y = 1 near quay, 2 far quay
            if (vMat.y > 0.5) {
                float l = 1e3;
                float zEdge = vMat.y < 1.5 ? -13.2 : -75.2;
                l = min(l, abs(g.y - zEdge));
                float dash = step(0.45, fract(g.x / 4.0));
                float lane = abs(g.y - (vMat.y < 1.5 ? -6.5 : -97.0));
                float paint = (1.0 - smoothstep(0.1, 0.14 + px, l)) + (1.0 - smoothstep(0.06, 0.1 + px, lane)) * dash * 0.8;
                // the well's footprint, painted on the quay
                if (vMat.y < 1.5) {
                    vec2 r = abs(g - vec2(0.0, -0.45)) - vec2(5.35, 0.85);
                    float fr = abs(max(r.x, r.y));
                    paint += (1.0 - smoothstep(0.05, 0.09 + px, fr)) * 0.9 * uBay;
                    // the crane's rails
                    float rail = min(abs(abs(g.x) - 15.5) - 0.0, 9.0);
                    alb = mix(alb, vec3(0.35, 0.33, 0.3) * 0.5, (1.0 - smoothstep(0.06, 0.1 + px, abs(rail - 0.25))) * 0.0);
                    float rl = min(abs(abs(g.x) - 17.75), abs(abs(g.x) - 18.25));
                    alb = mix(alb, vec3(0.08), (1.0 - smoothstep(0.03, 0.06 + px, rl)) * 0.9);
                    emit += vec3(0.0);
                    spec = 0.05;
                }
                alb = mix(alb, uQuayLine, clamp(paint, 0.0, 1.0) * 0.85);
            }
        }
        spec = 0.05;
        rough = 12.0;
    } else if (mat == 3) {
        // a building: rows of windows, some lit (vMat.y how many, vMat.z their light)
        if (face != 1) {
            vec2 cell = vec2(1.3, 1.1);
            vec2 g = q / cell;
            vec2 id = floor(g);
            vec2 f = fract(g);
            float win = step(0.18, f.x) * step(f.x, 0.82) * step(0.25, f.y) * step(f.y, 0.75);
            float inside = step(0.8, sz.x - abs(q.x)) * step(0.8, sz.y - abs(q.y));
            float lit = step(hash12(id + seed * 13.0 + float(face) * 7.0), vMat.y);
            float detail = 1.0 - smoothstep(0.1, 0.4, px);
            alb = mix(alb, vec3(0.03, 0.035, 0.05), win * inside * detail);
            emit += uWinCol * vMat.z * lit * win * inside * (0.6 + 0.4 * hash12(id + 3.0)) * mix(0.35, 1.0, detail);
        }
        spec = 0.05;
    } else if (mat == 4) {
        emit = vCol.rgb * vMat.y;
        alb = vec3(0.0);
    } else if (mat == 5) {
        // a hull: the red bottom below its boot top, welded plates
        float wl = uWaterY + vMat.y;
        if (vP.y < wl) alb = vec3(0.32, 0.07, 0.05);
        else if (vP.y < wl + 0.25) alb = vec3(0.85, 0.85, 0.82) * 0.6;
        float plates = 1.0 - smoothstep(0.02, 0.05 + px, abs(fract(vP.y / 1.3) - 0.5) * 1.3 - 0.6);
        alb *= 1.0 - 0.15 * plates * (1.0 - smoothstep(0.05, 0.2, px));
        if (face == 1 && N.y > 0.5) alb = vec3(0.16, 0.2, 0.18);
        spec = 0.2;
    } else if (mat == 6) {
        // a deck house: white, rows of windows lit; the bridge's band at the top
        if (face != 1) {
            float fl = 1.15;
            float row = floor((q.y + sz.y) / fl);
            float fy = fract((q.y + sz.y) / fl);
            float top = step(sz.y * 2.0 - fl, q.y + sz.y);
            float wx = fract(q.x / 0.9);
            float win = step(0.45, fy) * step(fy, 0.75) * step(0.25, wx) * step(wx, 0.75) * step(0.3, sz.x - abs(q.x));
            float band = top * step(0.4, fy) * step(fy, 0.8) * step(0.15, sz.x - abs(q.x));
            float lit = step(hash12(vec2(floor(q.x / 0.9), row) + seed * 5.0), 0.45 + 0.4 * uDeckLights);
            float detail = 1.0 - smoothstep(0.08, 0.3, px);
            alb = mix(alb, vec3(0.05, 0.06, 0.08), max(win, band) * detail);
            emit += uWinCol * (win * lit * 0.9 + band * 0.35) * mix(0.4, 1.0, detail);
        }
        spec = 0.15;
    } else if (mat == 7) {
        alb *= 1.0;
        spec = 0.05;
    } else if (mat == 8) {
        // a crane's cab: its window glows
        if (face != 1) {
            float win = step(0.12, sz.x - abs(q.x)) * step(0.1, sz.y - abs(q.y - sz.y * 0.15)) * step(q.y, sz.y * 0.7);
            emit += uWinCol * win * 0.8;
            alb = mix(alb, vec3(0.04), win);
        }
    } else if (mat == 9) {
        // a deck: hatch covers' seams
        float seam = 1.0 - smoothstep(0.03, 0.06 + px, abs(fract(vP.x / 2.6) - 0.5) * 2.6 - 1.25);
        alb *= 1.0 - 0.3 * seam;
        spec = 0.08;
    } else {
        // painted steel
        alb *= 0.92 + 0.08 * vnoise(vP.xz * 0.7 + vP.y * 0.3);
        spec = 0.3;
        rough = 40.0;
    }
    vec3 Nb = normalize(N + T * bend.x + B * bend.y);
    vec3 c = shade(vP, Nb, alb, spec, rough) + emit;
    if ((mat == 1 || mat == 10) && uStackA > 0.5) {
        // the stack's containers in their own paint: the hour's light sets how bright they are, not their hue, and
        // they stand a little brighter than the port's
        const vec3 LUM = vec3(0.2126, 0.7152, 0.0722);
        float lit = dot(c, LUM) / max(dot(alb, LUM), 1e-4);
        c = mix(c, alb * lit, 0.65) * 1.12 + sheen;
    }
    c = applyFog(c, vP);
    frag = vec4(c, uStackA);
}
)";

// ---------------------------------------------------------------- lamps (billboards)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;
layout(location = 2) in vec4 iC;
uniform mat4 uVP;
uniform float uClipY, uMirror;
out vec2 vC;
out vec4 vCol;
out vec3 vP;
void main() {
    vC = aCorner;
    vCol = iC;
    vec3 c = iP.xyz;
    vec3 P = c + vec3(aCorner.x, aCorner.y * (uMirror > 0.5 ? -1.0 : 1.0), 0.0) * iP.w;
    // a little toward the eye, so the lamp's own housing never cuts its glow
    vec3 toEye = normalize(uEye - c);
    P += toEye * min(iP.w * 0.8, 1.5);
    vP = c;
    if (c.y < uClipY) vCol = vec4(0.0);
    gl_Position = uVP * vec4(P, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
in vec3 vP;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 70.0);
    float halo = exp(-r2 * 7.0) * 0.32 + exp(-r2 * 2.2) * 0.08;
    float a = (core * 1.6 + halo) * (1.0 - r2);
    vec3 c = vCol.rgb * vCol.a * a;
    // far lamps sink into the haze
    float dist = length(vP - uEye);
    c *= mix(1.0, 0.45, 1.0 - exp(-dist * uFogD * 0.8));
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- cables (screen-space hairlines)
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // x: 0 start .. 1 end, y: -1 .. 1 across
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uRes;
out float vAcross;
out vec4 vCol;
out float vW;
out vec3 vP;
void main() {
    vec4 c0 = uVP * vec4(iA.xyz, 1.0), c1 = uVP * vec4(iB.xyz, 1.0);
    vec2 s0 = c0.xy / c0.w * uRes * 0.5, s1 = c1.xy / c1.w * uRes * 0.5;
    vec2 dir = s1 - s0;
    dir = length(dir) > 1e-4 ? normalize(dir) : vec2(0.0, 1.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec4 c = mix(c0, c1, aCorner.x);
    float half_ = max(iA.w, 1.0) * 0.5 + 0.75;
    c.xy += nrm * aCorner.y * half_ / (uRes * 0.5) * c.w;
    vAcross = aCorner.y * half_;
    vW = iA.w;
    vCol = vec4(iC.rgb, iB.w);
    vP = mix(iA.xyz, iB.xyz, aCorner.x);
    gl_Position = c;
}
)";
inline const char* LINE_FS = R"(#version 330 core
in float vAcross;
in vec4 vCol;
in float vW;
in vec3 vP;
out vec4 frag;
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    vec3 c = applyFog(vCol.rgb, vP);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- light cones in the haze (camera-facing quads)
inline const char* CONE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // x: 0 apex .. 1 base, y: -1 .. 1 across
layout(location = 1) in vec4 iA;        // apex, its radius
layout(location = 2) in vec4 iB;        // base, its radius
layout(location = 3) in vec4 iC;        // color, intensity
uniform mat4 uVP;
uniform float uClipY;
out vec2 vUV;
out vec4 vCol;
out vec3 vP;
void main() {
    vec3 d = iB.xyz - iA.xyz;
    vec3 mid = mix(iA.xyz, iB.xyz, aCorner.x);
    vec3 side = normalize(cross(d, normalize(uEye - mid)));
    vec3 P = mid + side * mix(iA.w, iB.w, aCorner.x) * aCorner.y;
    vUV = vec2(aCorner.y, aCorner.x);
    vCol = iC;
    if (iA.y < uClipY) vCol = vec4(0.0);
    vP = P;
    gl_Position = uVP * vec4(P, 1.0);
}
)";
inline const char* CONE_FS = R"(#version 330 core
in vec2 vUV;
in vec4 vCol;
in vec3 vP;
out vec4 frag;
void main() {
    float u = vUV.x, v = vUV.y;
    float across = pow(max(1.0 - u * u, 0.0), 1.6);
    float along = smoothstep(0.0, 0.12, v) * pow(1.0 - v, 0.8) * (1.0 - 0.5 * v);
    // a little drifting haze in the beam
    float n = 0.75 + 0.25 * vnoise(vP.xz * 0.35 + vec2(uTime * 0.15, vP.y * 0.2));
    float a = across * along * n * vCol.a;
    float dist = length(vP - uEye);
    a *= mix(1.0, 0.5, 1.0 - exp(-max(dist - 50.0, 0.0) * uFogD));
    frag = vec4(vCol.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well's soft shade (a quad behind the stack)
inline const char* VEIL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
uniform mat4 uVP;
uniform vec4 uRect;   // x0 y0 x1 y1
uniform float uZ;
out vec2 vQ;
void main() {
    vec2 p = mix(uRect.xy, uRect.zw, aCorner * 0.5 + 0.5);
    vQ = p;
    gl_Position = uVP * vec4(p, uZ, 1.0);
}
)";
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vQ;
out vec4 frag;
uniform vec4 uRect;
uniform vec3 uTint, uRim;
uniform float uA, uRimW;
void main() {
    vec2 d = min(vQ - uRect.xy, uRect.zw - vQ);
    // the hull: opaque, its lit coaming along the sides and the top
    float e = min(d.x, uRect.w - vQ.y);
    vec3 c = mix(uTint, uRim, 1.0 - smoothstep(uRimW * 0.6, uRimW, e));
    frag = vec4(c * uA, uA);
}
)";

// ---------------------------------------------------------------- the glow and the frame
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThr;
void main() {
    vec4 s0 = texture(uSrc, vUv + uTexel * vec2(-1, -1)), s1 = texture(uSrc, vUv + uTexel * vec2(1, -1));
    vec4 s2 = texture(uSrc, vUv + uTexel * vec2(-1, 1)), s3 = texture(uSrc, vUv + uTexel * vec2(1, 1));
    // the containers (the scene's alpha) stay out of the glow: they stay crisp, the lamps glow
    vec3 c = s0.rgb * (1.0 - s0.a) + s1.rgb * (1.0 - s1.a) + s2.rgb * (1.0 - s2.a) + s3.rgb * (1.0 - s3.a);
    c *= 0.25;
    // (the alpha carries the containers, blurred: their cast shadow)
    frag = vec4(max(c - vec3(uThr), 0.0), 0.25 * (s0.a + s1.a + s2.a + s3.a));
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
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF;
uniform vec2 uShadowOff;    // the stack's cast shadow, offset on screen (uv)
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
void main() {
    vec4 s = texture(uScene, vUv);
    vec4 b = texture(uBloom, vUv);
    vec3 c = s.rgb * uExposure + b.rgb * uBloomK;
    // the stack's soft shadow on the well's backing (the key light from above left), and the gaps between boxes
    // (the bloom's alpha is the stack's coverage, blurred), outside the containers only
    float m = texture(uBloom, vUv + uShadowOff).a;
    float occ = smoothstep(0.0, 0.5, m) * 0.7 + smoothstep(0.4, 0.9, b.a) * 0.3;
    c *= 1.0 - min(occ, 0.85) * (1.0 - clamp(s.a, 0.0, 1.0));
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    // slow motion: the fog rolls in, the port cools
    c = mix(c, mix(vec3(l), c, 0.55) * uSlowTint, uSlow);
    // a soft knee
    c = c / (1.0 + 0.22 * max(max(c.r, c.g), c.b));
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.32 * dot(v, v) * 2.0;
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

// Builds a shader with the common functions (and the lighting) inserted after the version line.
inline std::string withCommon(const char* src, bool lighting = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (lighting ? LIGHTING : "") + s.substr(nl + 1);
}

} // namespace docks_shaders

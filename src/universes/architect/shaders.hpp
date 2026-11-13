#pragma once
// ARCHITECT's shaders (see architect.cpp).
//
// The world is an architectural rendering of one building site in real 3D, seen through its own camera (a slow
// orbit): every solid is an instance of one of four meshes (a box with rounded arrises, a sphere, a cylinder, a
// cone), shaded per fragment by its material (white render, board-formed concrete, wood cladding, glass with the
// room seen behind it, stone, pool water with caustics, foliage, bark, painted steel lattice, tiles...), under one
// sun with a soft shadow map, a sky and ground ambient, screen-space ambient occlusion and a few point lights at
// dusk; aerial perspective toward the horizon. WORLD_BG draws the sky (its gradient by the hour, the sun, soft
// clouds, far hills, the sea in some variants, stars at dusk) and the ground (the meadow, the levelled site, the
// new lawn, the pool's pit cut out of it) with its depth, so the instances meet it.
// The well is drawn after it through the layout's camera: a deep backing, then the pieces as pigmented
// architectural concrete panels (BLOCK_FS), their contact shadows and the ghost's outline.
#include <string>

namespace architect_shaders {

// ---------------------------------------------------------------- common: noise, the sky and the hour
inline const char* COMMON = R"(
uniform float uSeed, uTime;
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uSkyLow, uFogCol, uAmbSky, uAmbGround;
uniform vec3 uEye;
uniform vec2 uInvRes;
uniform float uFogDist, uNight, uSea, uSeaZ;
uniform vec3 uSeaCol;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32 + uSeed * 0.01); return fract(p.x * p.y); }
float h31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}
float n2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
float n3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm2(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * n2(p); p = p * 2.03 + vec2(17.1, 3.7); a *= 0.5; }
    return s;
}
// The sky's color along a direction (no sun disc): its gradient by the hour, the glow around the sun.
vec3 skyCol(vec3 rd) {
    float e = rd.y;
    vec3 c = mix(uSkyHor, uSkyTop, smoothstep(0.0, 0.55, e));
    c = mix(c, uSkyLow, smoothstep(0.02, -0.25, e));
    float sd = max(dot(rd, normalize(uSunDir)), 0.0);
    float low = 1.0 - smoothstep(0.0, 0.5, uSunDir.y);
    c += uSunCol * (pow(sd, 6.0) * (0.12 + 0.35 * low) + pow(sd, 64.0) * 0.4) * (1.0 - smoothstep(0.1, -0.35, e));
    return c;
}
vec3 fogged(vec3 c, float dist, vec3 rd) {
    float f = 1.0 - exp(-max(dist - 60.0, 0.0) / uFogDist);
    vec3 fc = mix(uFogCol, skyCol(vec3(rd.x, max(rd.y, 0.02), rd.z)), 0.5);
    return mix(c, fc, f);
}
)";

// ---------------------------------------------------------------- the sun's shadow (one map, soft spiral taps)
inline const char* SHADOW_GLSL = R"(
uniform mat4 uLightVP;
uniform sampler2DShadow uShadow;
uniform float uSoft;
float shadowAt(vec3 p, int taps) {
    vec4 l = uLightVP * vec4(p, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    float a = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831;
    float sum = 0.0;
    for (int i = 0; i < 16; i++) {
        if (i >= taps) break;
        float fi = float(i) + 0.5;
        float rr = sqrt(fi / float(taps)) * uSoft;
        float an = a + fi * 2.39996;
        sum += texture(uShadow, vec3(s.xy + vec2(cos(an), sin(an)) * rr, s.z - 0.0007));
    }
    return sum / float(taps);
}
)";

// ---------------------------------------------------------------- point lights (dusk: rooms, the pool, lamps)
inline const char* LIGHTS_GLSL = R"(
uniform int uNumLights;
uniform vec4 uLightPos[24];   // xyz, radius
uniform vec3 uLightCol[24];
vec3 pointLights(vec3 p, vec3 n) {
    vec3 s = vec3(0.0);
    for (int i = 0; i < 24; i++) {
        if (i >= uNumLights) break;
        vec3 d = uLightPos[i].xyz - p;
        float r = uLightPos[i].w;
        float dl = length(d);
        float att = 1.0 / (1.0 + dl * dl / (r * r * 0.15));
        att *= 1.0 - smoothstep(r * 0.6, r, dl);
        s += uLightCol[i] * att * (0.35 + 0.65 * max(dot(n, d / max(dl, 1e-3)), 0.0));
    }
    return s;
}
)";

// ---------------------------------------------------------------- the pool's excavation (the ground and its prepass)
// A convex pit: its opening the pool's rectangle widened by uPitM (the working margin, closed again by the backfill),
// its sides sloping in (PIT_S horizontal per vertical) down to a floor uPitD deep. A ray that enters it through the
// opening leaves it through the first of its five planes it crosses.
inline const char* PIT_GLSL = R"(
uniform vec4 uPool;
uniform float uPitD, uPitM, uShell;   // uShell: the concrete shell stands (the ground then opens onto it)
bool inShell(vec3 p) { return uShell > 0.5 && p.x > uPool.x && p.x < uPool.z && p.z > uPool.y && p.z < uPool.w; }
const float PIT_S = 0.6;
vec4 pitRect() { return uPool + vec4(-uPitM, -uPitM, uPitM, uPitM); }
float pitExit(vec3 ro, vec3 rd, out vec3 n) {
    vec4 r = pitRect();
    vec3 g[5];
    float k[5];
    g[0] = vec3(1.0, PIT_S, 0.0);  k[0] = -r.x;
    g[1] = vec3(-1.0, PIT_S, 0.0); k[1] = r.z;
    g[2] = vec3(0.0, PIT_S, 1.0);  k[2] = -r.y;
    g[3] = vec3(0.0, PIT_S, -1.0); k[3] = r.w;
    g[4] = vec3(0.0, 1.0, 0.0);    k[4] = uPitD;
    float best = 1e9;
    n = vec3(0.0, 1.0, 0.0);
    for (int i = 0; i < 5; i++) {
        float fd = dot(g[i], rd);
        if (fd < -1e-6) {
            float tt = -(dot(g[i], ro) + k[i]) / fd;
            if (tt < best) { best = tt; n = normalize(g[i]); }
        }
    }
    return best;
}
)";

// ---------------------------------------------------------------- the world's background: sky and ground
inline const char* WORLD_BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uVP;
uniform sampler2D uAO;
uniform vec3 uGrassA, uGrassB, uDry, uSoil, uLawn, uHill, uHill2;
uniform vec4 uSite;      // the levelled site: x0, z0, x1, z1

uniform float uLevel, uLawnK, uPitMark, uHillK, uCloudK, uStarK, uFlowers;
uniform vec4 uFoot[8];   // the buildings' footprints (the lawn keeps off them)
uniform int uNumFoot;
float boxD(vec2 p, vec4 b) {
    vec2 c = (b.xy + b.zw) * 0.5, h = abs(b.zw - b.xy) * 0.5;
    vec2 d = abs(p - c) - h;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
vec3 sky(vec3 rd) {
    vec3 c = skyCol(rd);
    // the sun's disc
    float sd = dot(rd, normalize(uSunDir));
    c += uSunCol * smoothstep(0.99955, 0.9998, sd) * 6.0 * (1.0 - uNight * 0.8);
    // soft clouds on a high layer
    if (rd.y > 0.0) {
        vec2 q = rd.xz / (rd.y + 0.08) * 1.4 + vec2(uTime * 0.004, 0.0);
        float cl = fbm2(q * 0.9 + uSeed);
        cl = smoothstep(0.52, 0.8, cl) * uCloudK * smoothstep(0.0, 0.12, rd.y);
        float lit = 0.6 + 0.4 * max(dot(rd, normalize(uSunDir)), 0.0);
        vec3 cc = mix(uSkyHor * 1.05, uSunCol * 0.9 + uSkyHor * 0.4, lit * (1.0 - uSunDir.y * 0.6));
        c = mix(c, cc, cl * 0.75);
        // stars at dusk
        vec2 g = rd.xz / (rd.y + 0.3) * 120.0;
        vec2 gi = floor(g);
        float h = h21(gi);
        vec2 gp = fract(g) - 0.5 - (vec2(h21(gi + 3.1), h21(gi + 7.7)) - 0.5) * 0.6;
        float st = smoothstep(0.08, 0.0, length(gp)) * step(0.985, h) * uStarK * smoothstep(0.08, 0.4, rd.y) * (1.0 - cl);
        c += vec3(0.9, 0.92, 1.0) * st * 1.5;
    }
    // far hills along the horizon (two layers), lower in front where the sea opens
    float az = atan(rd.x, -rd.z);
    float hk = uHillK * (1.0 - uSea * 0.9 * exp(-az * az / 1.4));
    float h1 = (0.018 + 0.05 * fbm2(vec2(az * 3.0, 1.3))) * hk;
    float h2 = (0.008 + 0.03 * fbm2(vec2(az * 6.0 + 5.0, 7.1))) * hk;
    if (rd.y < h1 && rd.y > -0.002) {
        vec3 hc = mix(uHill, skyCol(vec3(rd.x, 0.02, rd.z)), 0.55);
        c = hc;
    }
    if (rd.y < h2 && rd.y > -0.002) c = mix(uHill2, skyCol(vec3(rd.x, 0.02, rd.z)), 0.3);
    return c;
}
void main() {
    vec4 a = uInvVP * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
    vec4 b = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ro = a.xyz / a.w, rd = normalize(b.xyz / b.w - ro);
    float t = rd.y < -1e-4 ? -ro.y / rd.y : 1e9;
    if (t > 2500.0) {
        frag = vec4(sky(rd), 0.0);
        gl_FragDepth = 1.0;
        return;
    }
    vec3 p = ro + rd * t;
    // the pool's excavation: an earth pit with sloped sides, deepening with the lines, lit and shaded as soil
    if (inShell(p)) discard;
    if (uPitD > 0.001 && boxD(p.xz, pitRect()) < 0.0) {
        vec3 n;
        float tp = pitExit(ro, rd, n);
        vec3 q = ro + rd * tp;
        vec4 cq = uVP * vec4(q, 1.0);
        gl_FragDepth = clamp(cq.z / cq.w * 0.5 + 0.5, 0.0, 1.0);
        float depthK = clamp(-q.y / 2.0, 0.0, 1.0);
        // the earth: darker and redder deeper down (its strata), clods and stones, the bucket's scrapes on the sides
        float strata = n2(vec2(q.y * 7.0, (q.x + q.z) * 0.3));
        vec3 earth = mix(uSoil * 0.82, uSoil * vec3(0.72, 0.6, 0.5), depthK) * (0.85 + 0.25 * strata);
        earth *= 0.88 + 0.22 * n2(q.xz * 3.0 + q.y * 2.0) + 0.06 * (n2(q.xz * 11.0 + q.y * 9.0) - 0.5);
        if (n.y < 0.9) earth *= 0.92 + 0.12 * smoothstep(0.3, 0.7, n2(vec2((q.x + q.z) * 2.5, q.y * 1.2)));
        vec3 Lp = normalize(uSunDir);
        float shp = shadowAt(q + n * 0.03, 12);
        float aop = texture(uAO, gl_FragCoord.xy * uInvRes).r;
        float occ = mix(1.0, 0.62, depthK);
        vec3 litp = earth * (uSunCol * max(dot(n, Lp), 0.0) * shp + (uAmbSky * 0.85 + uAmbGround) * occ * aop + pointLights(q, n));
        frag = vec4(fogged(litp, length(q - ro), rd), 0.0);
        return;
    }
    vec4 cl = uVP * vec4(p, 1.0);
    gl_FragDepth = clamp(cl.z / cl.w * 0.5 + 0.5, 0.0, 1.0);
    vec3 c;
    vec3 N = vec3(0, 1, 0);
    float rough = 1.0;
    if (uSea > 0.5 && p.z < uSeaZ + 30.0 * (n2(vec2(p.x * 0.01, 3.0)) - 0.5)) {
        // the sea: its blue, the sky in it, the sun's glitter
        float fr = 0.02 + 0.98 * pow(1.0 - max(-rd.y, 0.0), 5.0);
        vec3 r = reflect(rd, vec3(0, 1, 0));
        float w = n2(p.xz * vec2(0.08, 0.3) + uTime * 0.2) * n2(p.xz * vec2(0.13, 0.5) - uTime * 0.15);
        c = mix(uSeaCol, skyCol(r), fr * 0.8);
        float sg = pow(max(dot(normalize(r + vec3(0, 0, 0) + vec3((w - 0.25) * 0.08, 0.0, 0.0)), normalize(uSunDir)), 0.0), 300.0);
        c += uSunCol * sg * 3.0 * smoothstep(0.15, 0.4, w);
        frag = vec4(fogged(c, 60.0 + (t - 60.0) * 0.35, rd), 0.0);
        return;
    }
    // the meadow: soft patches of two greens, drier tufts, a fine grain
    float big = fbm2(p.xz * 0.035 + uSeed);
    float mid = fbm2(p.xz * 0.22 + 4.0);
    float fine = n2(p.xz * 3.1) * 0.5 + n2(p.xz * 9.0) * 0.5;
    vec3 grass = mix(uGrassA, uGrassB, smoothstep(0.3, 0.7, big));
    grass = mix(grass, uDry, smoothstep(0.55, 0.8, mid) * 0.55);
    float fineK = 1.0 - smoothstep(30.0, 140.0, t);
    float blades = n2(vec2(p.x * 14.0, p.z * 3.5)) * 0.6 + n2(vec2(p.x * 31.0 + 7.0, p.z * 8.0)) * 0.4;
    grass *= 1.0 + (fine - 0.5) * 0.25 * fineK + (blades - 0.5) * 0.3 * (1.0 - smoothstep(15.0, 60.0, t));
    grass *= 0.88 + 0.24 * smoothstep(0.2, 0.8, fbm2(p.xz * 0.09 + 11.0));
    c = grass;
    // wildflowers in the meadow, near (white, yellow, lilac), in drifts
    if (t < 55.0 && uFlowers > 0.0) {
        vec2 g = p.xz * 2.2;
        vec2 gi = floor(g);
        vec2 gp = fract(g) - 0.5 - (vec2(h21(gi + 1.3), h21(gi + 5.9)) - 0.5) * 0.7;
        float drift = smoothstep(0.45, 0.75, fbm2(p.xz * 0.12 + 3.0));
        float fl = smoothstep(0.1, 0.05, length(gp)) * step(1.0 - 0.35 * drift, h21(gi + 9.1)) * (1.0 - smoothstep(35.0, 55.0, t));
        float hue = h21(gi + 2.2);
        vec3 fc = hue < 0.45 ? vec3(0.95, 0.94, 0.88) : (hue < 0.75 ? vec3(0.98, 0.8, 0.2) : vec3(0.72, 0.55, 0.9));
        c = mix(c, fc * uFlowers, fl * 0.85);
    }
    // the levelled site: compacted soil spreading from the middle, its edge soft and ragged
    float sd = boxD(p.xz, uSite) + (n2(p.xz * 0.35) - 0.5) * 2.5;
    float lev = uLevel * 1.15;
    float siteR = mix(-30.0, 4.0, lev);
    float soilK = smoothstep(siteR + 1.5, siteR - 1.5, sd + max(0.0, 0.0)) * step(0.001, uLevel);
    soilK *= smoothstep(1.5, -1.0, sd);
    vec3 soil = uSoil * (0.9 + 0.2 * n2(p.xz * 0.8)) * (1.0 + (fine - 0.5) * 0.12 * fineK);
    // tyre tracks in the soil
    float tr = abs(fract(p.x * 0.18 + n2(p.xz * 0.05) * 0.8) - 0.5);
    soil *= 1.0 - 0.06 * smoothstep(0.06, 0.0, abs(tr - 0.12)) * fineK;
    c = mix(c, soil, soilK);
    // the new lawn: fresh green with mowing stripes, keeping off the buildings
    float foot = 1e9;
    for (int i = 0; i < 8; i++) { if (i >= uNumFoot) break; foot = min(foot, boxD(p.xz, uFoot[i])); }
    float lawnR = mix(-26.0, 6.0, uLawnK);
    float lk = smoothstep(lawnR + 2.0, lawnR - 2.0, boxD(p.xz, uSite) - 30.0 + length(p.xz - vec2(0.0, 6.0)) * 0.5)
             * step(0.001, uLawnK) * smoothstep(0.0, 0.6, foot);
    lk *= soilK;
    float stripe = step(0.5, fract(p.x * 0.25));
    vec3 lawn = uLawn * (0.94 + 0.12 * stripe) * (1.0 + (fine - 0.5) * 0.1 * fineK);
    c = mix(c, lawn, lk);
    // the excavation: its outline sprayed on the soil before the dig, loose dark earth round its lip while it is open
    {
        float dp = boxD(p.xz, pitRect());
        float mark = smoothstep(0.08, 0.03, abs(dp)) * step(0.3, fract((p.x + p.z) * 1.3)) * uPitMark;
        c = mix(c, vec3(0.95, 0.42, 0.1), mark * 0.85);
        float lip = (1.0 - smoothstep(0.0, 1.0 + 0.8 * n2(p.xz * 2.0), dp)) * smoothstep(0.001, 0.3, uPitD);
        c = mix(c, uSoil * 0.7 * (0.85 + 0.3 * n2(p.xz * 6.0)), lip * 0.7);
    }
    // the light: sun with the shadows, sky, occlusion, lamps
    float sh = shadowAt(p + vec3(0, 0.02, 0), 12);
    float ao = texture(uAO, gl_FragCoord.xy * uInvRes).r;
    vec3 L = normalize(uSunDir);
    vec3 lit = c * (uSunCol * max(L.y, 0.0) * sh * mix(1.0, ao, 0.4) + uAmbSky * ao * 0.9 + pointLights(p, N));
    frag = vec4(fogged(lit, t, rd), 0.0);
}
)";

// ---------------------------------------------------------------- the instances
// a = center, seed; b = size, material; c = color (linear), parameter; d = rotation (quaternion);
// e = glow (lights on), depth of the room behind a glass, alt (0..1, a material's second state), cut (0..1).
inline const char* INST_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iA;
layout(location = 3) in vec4 iB;
layout(location = 4) in vec4 iC;
layout(location = 5) in vec4 iD;
layout(location = 6) in vec4 iE;
uniform mat4 uVP;
uniform mat4 uView;
uniform int uMesh;     // 0 box, 1 sphere, 2 cylinder, 3 cone
uniform float uWind;
out vec3 vWorld, vObj, vViewPos, vNrmO;
out float vLump;
flat out vec3 vSize, vCol;
flat out vec4 vQuat, vE;
flat out float vSeed, vMat, vParam;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float hh(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
float nn(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hh(i), hh(i + vec3(1, 0, 0)), f.x), mix(hh(i + vec3(0, 1, 0)), hh(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hh(i + vec3(0, 0, 1)), hh(i + vec3(1, 0, 1)), f.x), mix(hh(i + vec3(0, 1, 1)), hh(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
    vec3 pos = aPos;
    vec3 nrm = aNrm;
    vLump = 0.5;
    if (uMesh == 1 && iB.w > 4.5 && iB.w < 5.5) {
        // foliage: a lumpy clump of leaves (its outline broken by noise), swaying a little
        vec3 d = normalize(aPos);
        float k = nn(d * 2.6 + iA.w * 17.0) * 0.55 + nn(d * 6.0 + iA.w * 5.0) * 0.3 + nn(d * 13.0 + iA.w * 3.0) * 0.15;
        pos = d * 0.5 * (0.62 + 0.55 * k);
        vLump = k;
    }
    vec3 obj = pos * iB.xyz;
    vec3 w = iA.xyz + qrot(iD, obj);
    if (uMesh == 1 && iB.w > 4.5 && iB.w < 5.5) {
        float sw = sin(uWind * 0.7 + iA.w * 9.0) * 0.04;
        w.x += sw * max(obj.y + iB.y * 0.5, 0.0) * 0.3;
    }
    vWorld = w;
    vObj = obj;
    vNrmO = nrm;
    vViewPos = (uView * vec4(w, 1.0)).xyz;
    vSize = iB.xyz;
    vMat = iB.w;
    vCol = iC.rgb;
    vParam = iC.a;
    vQuat = iD;
    vE = iE;
    vSeed = iA.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";

// shared by the main pass, the prepass and the shadow: the normal, the cut-outs (lattices, fronds)
inline const char* INST_COMMON = R"(
uniform int uMesh;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
vec3 roundedNormal(vec3 p, vec3 face, vec3 size, float r) {
    vec3 h = size * 0.5;
    vec3 q = clamp(p, -h + r, h - r);
    vec3 d = p - q;
    float l = length(d);
    return l > 1e-5 ? d / l : face;
}
vec3 boxFace(vec3 p, vec3 size) {
    vec3 q = abs(p) / max(size * 0.5, vec3(1e-4));
    if (q.x > q.y && q.x > q.z) return vec3(sign(p.x), 0, 0);
    if (q.y > q.z) return vec3(0, sign(p.y), 0);
    return vec3(0, 0, sign(p.z));
}
// A face's 2D coordinates (meters) and its extent.
vec2 faceUV(vec3 p, vec3 f, vec3 s, out vec2 ext) {
    if (abs(f.x) > 0.5) { ext = s.zy; return p.zy; }
    if (abs(f.y) > 0.5) { ext = s.xz; return p.xz; }
    ext = s.xy; return p.xy;
}
// The lattice of a crane's mast or jib: chords along the edges, diagonals between them (the rest cut away).
bool latticeCut(vec3 p, vec3 size) {
    vec3 f = boxFace(p, size);
    vec2 ext;
    vec2 uv = faceUV(p, f, size, ext);
    // the long axis
    bool alongU = ext.x > ext.y;
    float L = alongU ? uv.x : uv.y, W = alongU ? uv.y : uv.x, ew = alongU ? ext.y : ext.x;
    float t = 0.065 * ew + 0.03;
    float hw = ew * 0.5;
    if (abs(W) > hw - t) return false;             // the chords
    float bay = ew;
    float u = fract(L / bay);
    float x = (W / ew + 0.5);                       // 0..1 across
    float dd = abs(fract(L / bay + 0.0) - x);
    float dg = min(dd, 1.0 - dd) * bay;
    if (dg < t * 0.8) return false;                 // the diagonals
    if (abs(u - 0.0) * bay < t * 0.7 || abs(u - 1.0) * bay < t * 0.7) return false; // the struts
    return true;
}
// A palm frond's leaflets: a comb along its length, tapered at the tip.
bool frondCut(vec3 p, vec3 size) {
    float u = p.x / size.x + 0.5;                   // along the frond
    float v = abs(p.z) / (size.z * 0.5);            // across
    float w = mix(1.0, 0.15, u) * smoothstep(0.0, 0.12, u);
    if (v > w) return true;
    if (v < 0.08) return false;                     // the rib
    float leaf = fract(u * 26.0 + v * 1.4);
    return leaf > 0.55;
}
bool cutAway(vec3 p, vec3 size, float mat) {
    if (mat > 6.5 && mat < 7.5 && uMesh == 0) return latticeCut(p, size);
    if (mat > 14.5 && mat < 15.5) return frondCut(p, size);
    return false;
}
vec3 objNormal(vec3 p, vec3 nrmO, vec3 size, float mat) {
    if (uMesh == 0) {
        float r = mat > 6.5 && mat < 7.5 ? 0.0 : min(0.03, min(min(size.x, size.y), size.z) * 0.3);
        if (mat > 15.5 && mat < 16.5) r = 0.0;
        vec3 f = boxFace(p, size);
        return r > 0.0 ? roundedNormal(p, f, size, r) : f;
    }
    return normalize(nrmO / max(size, vec3(1e-4)));
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos, vNrmO;
flat in vec3 vSize, vCol;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vParam;
void main() {
    if (cutAway(vObj, vSize, vMat)) discard;
    if (vMat > 3.5 && vMat < 4.5) discard;   // water casts no shadow
}
)";

inline const char* PRE_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos, vNrmO;
flat in vec3 vSize, vCol;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vParam;
layout(location = 0) out vec4 oPos;
layout(location = 1) out vec4 oNrm;
uniform mat4 uView;
void main() {
    if (cutAway(vObj, vSize, vMat)) discard;
    vec3 N = normalize(qrot(vQuat, objNormal(vObj, vNrmO, vSize, vMat)));
    oPos = vec4(vViewPos, 1.0);
    oNrm = vec4(normalize(mat3(uView) * N), 1.0);
}
)";

// The ground in the prepass (one ray per pixel), with the pool's excavation.
inline const char* PRE_GROUND_FS = R"(#version 330 core
in vec2 vUv;
layout(location = 0) out vec4 oPos;
layout(location = 1) out vec4 oNrm;
uniform mat4 uInvVP, uView, uVP;
void main() {
    vec4 a = uInvVP * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
    vec4 b = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ro = a.xyz / a.w, rd = normalize(b.xyz / b.w - ro);
    if (rd.y > -1e-4) discard;
    float t = -ro.y / rd.y;
    if (t > 2500.0) discard;
    vec3 p = ro + rd * t;
    vec3 n = vec3(0.0, 1.0, 0.0);
    if (inShell(p)) discard;
    if (uPitD > 0.001 && p.x > pitRect().x && p.x < pitRect().z && p.z > pitRect().y && p.z < pitRect().w) p = ro + rd * pitExit(ro, rd, n);
    vec4 c = uVP * vec4(p, 1.0);
    gl_FragDepth = clamp(c.z / c.w * 0.5 + 0.5, 0.0, 1.0);
    oPos = vec4((uView * vec4(p, 1.0)).xyz, 1.0);
    oNrm = vec4(normalize(mat3(uView) * n), 1.0);
}
)";

// Materials: 0 render (smooth painted plaster), 1 wood cladding (boards along the param's axis), 2 glass (the room
// behind it), 3 stone, 4 pool water, 5 foliage, 6 bark, 7 painted steel lattice, 8 painted steel, 9 tiles (alt:
// raw concrete to mosaic), 10 soil, 11 dark metal (frames), 12 a lamp (emissive), 13 fabric, 14 cable,
// 15 palm frond, 16 board-formed concrete, 17 rough concrete (fresh pour), 18 deck boards.
inline const char* INST_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos, vNrmO;
flat in vec3 vSize, vCol;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vParam;
out vec4 frag;
in float vLump;
uniform sampler2D uAO;
uniform vec3 uRoomDay, uRoomNight, uWater, uWaterDeep, uWaterGlow;
uniform float uPoolLight;
// the room seen through a glass pane: a box behind the pane (interior mapping), lit by the day or its lamps
vec3 roomBehind(vec3 p, vec3 V, vec3 f, float depth, float lights, float seed) {
    // pane-local: x across, y up, z into the room
    vec3 ax = abs(f.x) > 0.5 ? vec3(0, 0, -f.x) : vec3(f.z, 0, 0);
    vec3 ay = vec3(0, 1, 0);
    vec3 az = -f;
    vec2 ext = vec2(abs(f.x) > 0.5 ? vSize.z : vSize.x, vSize.y);
    vec3 o = vec3(dot(p, ax), dot(p, ay), 0.0);
    vec3 d = vec3(dot(-V, ax), dot(-V, ay), dot(-V, az));
    vec3 tt = vec3(1e9);
    if (abs(d.x) > 1e-4) tt.x = ((d.x > 0.0 ? ext.x * 0.5 : -ext.x * 0.5) - o.x) / d.x;
    if (abs(d.y) > 1e-4) tt.y = ((d.y > 0.0 ? ext.y * 0.5 : -ext.y * 0.5) - o.y) / d.y;
    if (d.z > 1e-4) tt.z = depth / d.z;
    float t = min(tt.x, min(tt.y, tt.z));
    vec3 h = o + d * t;
    vec3 day = uRoomDay, night = uRoomNight;
    vec3 c;
    float lamp = 0.0;
    if (t == tt.y && d.y < 0.0) {
        // the floor: warm oak boards, a rug
        float b = fract(h.x * 1.6 + seed);
        c = vec3(0.42, 0.3, 0.2) * (0.9 + 0.12 * step(0.5, b));
        float rug = step(abs(h.x), ext.x * 0.25) * step(abs(h.z - depth * 0.5), depth * 0.22);
        c = mix(c, vec3(0.55, 0.52, 0.48), rug);
        lamp = 0.6;
    } else if (t == tt.y) {
        // the ceiling, with its lamps
        c = vec3(0.75);
        vec2 q = vec2(fract(h.x / 2.2 + seed) - 0.5, fract(h.z / 2.4) - 0.5);
        lamp = 1.0 + 6.0 * smoothstep(0.16, 0.05, length(q)) * lights;
    } else if (t == tt.z) {
        // the back wall: a painting, a shelf
        c = vec3(0.7, 0.68, 0.64);
        float art = step(abs(h.x - ext.x * (fract(seed * 7.0) - 0.5) * 0.4), 0.45) * step(abs(h.y - 0.2), 0.4);
        c = mix(c, mix(vec3(0.25, 0.32, 0.4), vec3(0.6, 0.3, 0.2), fract(seed * 13.0)), art);
        float sofa = step(h.y, -ext.y * 0.5 + 0.8) * step(abs(h.x), ext.x * 0.3);
        c = mix(c, vec3(0.5, 0.48, 0.44), sofa);
        lamp = 0.8;
    } else {
        c = vec3(0.68, 0.66, 0.62);
        lamp = 0.7;
    }
    // light from the window in the day (darker deeper in), the lamps at night (warm, brighter near the ceiling)
    float depthK = clamp(h.z / max(depth, 0.1), 0.0, 1.0);
    vec3 dayL = day * mix(1.0, 0.45, depthK);
    vec3 nightL = night * (0.6 + 0.4 * (h.y / ext.y + 0.5)) * lamp;
    return c * (dayL + nightL * lights);
}
void main() {
    if (cutAway(vObj, vSize, vMat)) discard;
    vec3 nO = objNormal(vObj, vNrmO, vSize, vMat);
    vec3 N = normalize(qrot(vQuat, nO));
    vec3 V = normalize(uEye - vWorld);
    vec3 face = uMesh == 0 ? boxFace(vObj, vSize) : nO;
    vec3 fW = normalize(qrot(vQuat, face));
    vec2 ext;
    vec2 uv = faceUV(vObj, face, vSize, ext);
    vec3 alb = vCol;
    float rough = 0.85, spec = 0.3, refl = 0.0;
    vec3 emit = vec3(0.0);
    int m = int(vMat + 0.5);
    float s1 = fract(vSeed * 7.13);
    if (m == 0) {
        // render: smooth plaster, a faint trowel cloud
        alb *= 0.96 + 0.06 * n3(vWorld * 1.3 + s1 * 9.0) + 0.02 * n3(vWorld * 7.0);
        rough = 0.75;
    } else if (m == 1 || m == 18) {
        // wood boards: along the param's axis (0 vertical, 1 horizontal), each board its own tone, fine grain
        bool hor = vParam > 0.5 || m == 18;
        float w = m == 18 ? 0.16 : 0.12;
        float along = hor ? (abs(face.x) > 0.5 ? vObj.z : vObj.x) : vObj.y;
        float across = hor ? (abs(face.y) > 0.5 ? (m == 18 ? vObj.z : vObj.y) : vObj.y) : (abs(face.x) > 0.5 ? vObj.z : vObj.x);
        if (m == 18) { along = vObj.x; across = vObj.z; }
        float bi = floor(across / w);
        float bf = fract(across / w);
        float tone = 0.85 + 0.3 * h21(vec2(bi, s1 * 31.0));
        float grain = n2(vec2(along * 1.5, across * 40.0) + bi * 3.1);
        alb *= tone * (0.92 + 0.16 * grain);
        alb *= 1.0 - 0.45 * smoothstep(0.08, 0.0, bf) - 0.3 * smoothstep(0.92, 1.0, bf);
        rough = 0.7;
    } else if (m == 2) {
        // glass: the sky and the site in it, the room behind
        float fr = 0.06 + 0.94 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
        vec3 r = reflect(-V, N);
        vec3 refl = skyCol(r);
        if (r.y < 0.0) refl = mix(uAmbGround * 1.2, refl, 0.3);
        vec3 room = roomBehind(vObj, normalize(qrot(vec4(-vQuat.xyz, vQuat.w), V)), face, max(vE.y, 0.5), vE.x, vSeed);
        if (abs(face.y) > 0.5) room = vCol * 0.2;
        vec3 c = mix(room * vCol, refl, clamp(fr * 0.85 + 0.12, 0.0, 1.0) * (1.0 - 0.5 * vE.x));
        // a soft sun glint
        vec3 H = normalize(normalize(uSunDir) + V);
        c += uSunCol * pow(max(dot(N, H), 0.0), 400.0) * 3.0 * (1.0 - uNight);
        float ao = texture(uAO, gl_FragCoord.xy * uInvRes).r;
        c *= mix(1.0, ao, 0.4);
        frag = vec4(fogged(c, length(uEye - vWorld), -V), 0.0);
        return;
    } else if (m == 3) {
        // stone: ashlar courses of travertine, each block its tone, fine pits
        float course = 0.32 + 0.1 * vParam;
        float row = floor(vWorld.y / course);
        float along = abs(fW.x) > 0.5 ? vWorld.z : vWorld.x;
        float bl = (along + h21(vec2(row, 3.0)) * 2.0) / (0.9 + 0.3 * vParam);
        float bi = floor(bl);
        float tone = 0.86 + 0.24 * h21(vec2(bi, row));
        alb *= tone * (0.94 + 0.1 * n3(vWorld * 3.0));
        float jy = fract(vWorld.y / course), jx = fract(bl);
        if (abs(fW.y) < 0.5) alb *= 1.0 - 0.35 * (smoothstep(0.05, 0.0, jy) + smoothstep(0.035, 0.0, min(jx, 1.0 - jx)));
        alb *= 1.0 - 0.15 * step(0.93, h31(floor(vWorld * 22.0)));
        rough = 0.8;
    } else if (m == 4) {
        // the pool's water: clear turquoise over the tiles, caustics, the sky in it, its lights at dusk
        float fr = 0.03 + 0.97 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
        vec2 q = vWorld.xz;
        float ca = 0.0;
        for (int i = 0; i < 2; i++) {
            vec2 g = q * (1.4 + float(i) * 0.9) + vec2(uTime * 0.25, -uTime * 0.2) * (1.0 + float(i) * 0.4);
            float a1 = n2(g), a2 = n2(g * 1.7 + 4.0);
            ca += pow(max(1.0 - abs(a1 - a2) * 1.8, 0.0), 5.0);
        }
        ca *= 1.0 - smoothstep(25.0, 70.0, length(uEye - vWorld));
        vec3 deep = mix(uWater, uWaterDeep, smoothstep(0.0, 1.0, abs(vObj.x) / (vSize.x * 0.5)) * 0.4);
        float sh = shadowAt(vWorld + vec3(0, 0.05, 0), 8);
        vec3 c = deep * (uAmbSky * 0.9 + uSunCol * max(uSunDir.y, 0.0) * (0.55 + 0.6 * ca * sh));
        vec3 wn = normalize(vec3((n2(q * 2.0 + uTime * 0.6) - 0.5) * 0.08, 1.0, (n2(q * 2.3 - uTime * 0.5) - 0.5) * 0.08));
        vec3 r = reflect(-V, wn);
        c = mix(c, skyCol(r), fr * 0.9);
        vec3 H = normalize(normalize(uSunDir) + V);
        c += uSunCol * pow(max(dot(wn, H), 0.0), 600.0) * 4.0 * sh;
        c += uWaterGlow * uPoolLight * (0.7 + 0.5 * ca);
        c *= vE.w > 0.0 ? 1.0 : 1.0;
        frag = vec4(fogged(c, length(uEye - vWorld), -V), 0.0);
        return;
    } else if (m == 5) {
        // foliage: leaf clusters (a noisy normal), translucent in the light
        float k = n3(vWorld * 2.6 + s1 * 10.0);
        float k2 = n3(vWorld * 7.0);
        N = normalize(N + (vec3(k, k2, n3(vWorld * 2.6 + 7.0)) - 0.5) * 1.1);
        alb *= (0.62 + 0.3 * k * k2 + 0.1 * vParam) * mix(0.55, 1.15, smoothstep(0.25, 0.75, vLump));
        rough = 0.9;
        spec = 0.05;
    } else if (m == 6) {
        alb *= 0.85 + 0.25 * n2(vec2(vObj.y * 6.0, atan(vObj.x, vObj.z) * 3.0 + s1 * 10.0));
    } else if (m == 7 || m == 8) {
        rough = 0.55;
        spec = 0.5;
    } else if (m == 9) {
        // tiles: raw concrete (alt 0) turning into small mosaic tiles (alt 1)
        vec2 g = uv / 0.25;
        vec2 gf = fract(g);
        float grout = smoothstep(0.06, 0.0, min(min(gf.x, 1.0 - gf.x), min(gf.y, 1.0 - gf.y)));
        vec3 tile = vCol * (0.92 + 0.12 * h21(floor(g) + s1)) * (1.0 - 0.25 * grout);
        vec3 raw = vec3(0.5, 0.49, 0.46) * (0.9 + 0.15 * n3(vWorld * 2.0));
        alb = mix(raw, tile, vE.z);
        rough = mix(0.9, 0.35, vE.z);
    } else if (m == 10) {
        alb *= 0.8 + 0.3 * n3(vWorld * 2.0) + 0.1 * n3(vWorld * 9.0);
        rough = 1.0;
        spec = 0.0;
    } else if (m == 11) {
        rough = 0.5;
        spec = 0.4;
    } else if (m == 12) {
        emit = vCol * vE.x;
        alb = vec3(0.6);
    } else if (m == 13) {
        alb *= 0.95 + 0.08 * n3(vWorld * 20.0);
        rough = 0.95;
    } else if (m == 14) {
        rough = 0.6;
    } else if (m == 19) {
        // a cactus: vertical ribs
        float an = atan(vObj.x, vObj.z) * 7.0;
        N = normalize(N + qrot(vQuat, vec3(cos(an), 0.0, -sin(an))) * 0.0 + fW * 0.0);
        alb *= 0.8 + 0.25 * (0.5 + 0.5 * cos(an * 2.0));
        rough = 0.7;
        spec = 0.2;
    } else if (m == 16 || m == 17) {
        // board-formed concrete: the boards' imprint, tie holes; a fresh pour is darker and even
        float yb = fract(vWorld.y / 0.18 + s1);
        alb *= 0.93 + 0.08 * n3(vWorld * vec3(0.6, 6.0, 0.6)) + 0.05 * n3(vWorld * 3.0);
        if (m == 16 && abs(fW.y) < 0.5) {
            alb *= 1.0 - 0.1 * smoothstep(0.06, 0.0, min(yb, 1.0 - yb));
            float along = abs(fW.x) > 0.5 ? vWorld.z : vWorld.x;
            vec2 th = vec2(fract(along / 0.9 + 0.5) - 0.5, fract(vWorld.y / 0.9) - 0.5) * 0.9;
            alb *= 1.0 - 0.35 * smoothstep(0.035, 0.015, length(th));
        }
        if (m == 17) alb *= 0.85;
        rough = 0.9;
    }
    // ---- light: the sun (wrapped a little), the sky, the ground's bounce, the lamps
    vec3 L = normalize(uSunDir);
    float sh = shadowAt(vWorld + N * 0.04, 12);
    float ao = texture(uAO, gl_FragCoord.xy * uInvRes).r;
    float ndl = dot(N, L);
    float wrap = m == 5 ? max((ndl + 0.45) / 1.45, 0.0) : max(ndl, 0.0);
    vec3 amb = mix(uAmbGround, uAmbSky, N.y * 0.5 + 0.5);
    vec3 col = alb * (uSunCol * wrap * sh * mix(1.0, ao, 0.3) + amb * ao + pointLights(vWorld, N) * mix(1.0, ao, 0.5));
    if (m == 5) col += alb * uSunCol * 0.35 * pow(max(dot(-V, L), 0.0), 3.0) * sh;   // light through the leaves
    vec3 H = normalize(L + V);
    float nh = max(dot(N, H), 0.0);
    float a2 = pow(rough, 4.0);
    float dd = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * dd * dd);
    float F = 0.04 + 0.96 * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    col += uSunCol * D * F * 0.25 * max(ndl, 0.0) * sh * spec;
    col += emit;
    frag = vec4(fogged(col, length(uEye - vWorld), -V), 0.0);
}
)";

// ---------------------------------------------------------------- ambient occlusion (PLANKS', in meters)
inline const char* SSAO_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPos, uNrm;
uniform mat4 uProj;
uniform float uRadius, uStrength;
float hash2(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 45758.5453); }
void main() {
    vec4 P = texture(uPos, vUv);
    if (P.w < 0.5) { frag = vec4(1.0); return; }
    vec3 n = normalize(texture(uNrm, vUv).xyz);
    vec3 p = P.xyz;
    float rad = uRadius * (1.0 + max(-p.z - 40.0, 0.0) * 0.02);
    float a0 = hash2(gl_FragCoord.xy) * 6.2831;
    vec3 t = normalize(abs(n.y) < 0.9 ? cross(n, vec3(0, 1, 0)) : cross(n, vec3(1, 0, 0)));
    vec3 bt = cross(n, t);
    float occ = 0.0;
    const int S = 12;
    for (int i = 0; i < S; i++) {
        float fi = float(i) + 0.5;
        float an = a0 + fi * 2.39996;
        float r = sqrt(fi / float(S));
        float h = sqrt(max(1.0 - r * r, 0.0));
        float sc = mix(0.15, 1.0, fract(fi * 0.618 + hash2(gl_FragCoord.xy + fi)));
        vec3 dir = (t * cos(an) * r + bt * sin(an) * r + n * h) * rad * sc;
        vec3 s = p + dir + n * 0.02;
        vec4 c = uProj * vec4(s, 1.0);
        vec2 uv = c.xy / c.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) continue;
        vec4 q = texture(uPos, uv);
        if (q.w < 0.5) continue;
        float dz = q.z - s.z;
        float range = smoothstep(0.0, 1.0, rad / max(abs(p.z - q.z), 1e-3));
        occ += (dz > 0.03 ? 1.0 : 0.0) * range;
    }
    frag = vec4(clamp(1.0 - occ / float(S) * uStrength, 0.0, 1.0));
}
)";

inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uAO, uPos;
uniform vec2 uDir;
void main() {
    float z0 = texture(uPos, vUv).z;
    float sum = 0.0, wsum = 0.0;
    for (int i = -4; i <= 4; i++) {
        vec2 uv = vUv + uDir * float(i);
        float z = texture(uPos, uv).z;
        float w = exp(-float(i * i) / 10.0) * exp(-abs(z - z0) * 2.0);
        sum += texture(uAO, uv).r * w;
        wsum += w;
    }
    frag = vec4(sum / max(wsum, 1e-4));
}
)";

// ---------------------------------------------------------------- glows (lamps, beacons, sparks): additive sprites
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz, size
layout(location = 2) in vec4 iCol;   // rgb, softness
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec4 vCol;
void main() {
    vQ = aCorner;
    vCol = iCol;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vQ;
in vec4 vCol;
out vec4 frag;
void main() {
    float r = length(vQ);
    float core = smoothstep(0.22, 0.0, r);
    float halo = exp(-r * r * 7.0) * (1.0 - smoothstep(0.7, 1.0, r));
    float a = core * (1.0 - vCol.a) + halo * (0.35 + vCol.a);
    frag = vec4(vCol.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well: the pieces as concrete panels
// Instance as above: a = center, seed; b = size, (unused); c = color; d = rotation; e = dim, lift, -, -.
inline const char* BLOCK_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos, vNrmO;
flat in vec3 vSize, vCol;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vParam;
out vec4 frag;
uniform vec3 uKeyDir, uKeyCol, uAmbTop, uAmbBot, uEyeB, uRimCol;
uniform float uDull;
float hb(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float nb(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hb(i), hb(i + vec2(1, 0)), f.x), mix(hb(i + vec2(0, 1)), hb(i + vec2(1, 1)), f.x), f.y);
}
float fb(vec2 p) { return nb(p) * 0.55 + nb(p * 2.1 + 3.0) * 0.3 + nb(p * 4.3 + 7.0) * 0.15; }
void main() {
    vec3 h = vSize * 0.5;
    float r = 0.075 * vSize.x;
    vec3 q = clamp(vObj, -h + r, h - r);
    vec3 d = vObj - q;
    float l = length(d);
    vec3 nO = l > 1e-5 ? d / l : vec3(0, 0, 1);
    vec2 p = vObj.xy / vSize.x;                // -0.5..0.5 on the front face
    float s1 = fract(vSeed * 7.13), s2 = fract(vSeed * 3.71);
    // the panel: pigmented concrete cast in a board formwork: the boards' faint imprint (three boards, each its
    // own tone, a fine grain), a cloudy mottle of the pour, rare small blowholes, and the four tie holes of the
    // formwork (small dimples with a pale lip): the architect's concrete, in the piece's pigment
    float lum = dot(vCol, vec3(0.3, 0.55, 0.15));
    vec3 alb = mix(vCol, vec3(lum), 0.1);
    float mott = fb(p * 2.2 + vec2(s1, s2) * 20.0);
    float cloud = fb(p * 5.5 + vec2(s2, s1) * 11.0);
    alb *= 0.86 + 0.2 * mott + 0.08 * (cloud - 0.5);
    vec3 bump = vec3(0.0);
    if (nO.z > 0.5) {
        float yb = (p.y + 0.5) * 3.0;
        float bi = floor(yb), bf = fract(yb);
        alb *= 0.97 + 0.06 * hb(vec2(bi, s1 * 17.0));
        float grain = nb(vec2(p.x * 3.0 + bi * 7.0, p.y * 60.0 + s2 * 9.0));
        alb *= 0.97 + 0.06 * grain;
        float seam = smoothstep(0.035, 0.0, min(bf, 1.0 - bf)) * step(0.5, bi) * step(bi, 1.5) + smoothstep(0.035, 0.0, bf) * step(1.5, bi);
        alb *= 1.0 - 0.08 * seam;
        bump.y += (smoothstep(0.0, 0.05, bf) - smoothstep(0.95, 1.0, bf)) * 0.0;
        // blowholes: a few small dark pits, irregular
        vec2 g = p * 9.0 + s1 * 31.0;
        vec2 gi = floor(g);
        vec2 gp = fract(g) - 0.5 - (vec2(hb(gi + 1.7), hb(gi + 4.3)) - 0.5) * 0.6;
        float bh = smoothstep(0.09, 0.04, length(gp)) * step(0.86, hb(gi));
        alb *= 1.0 - 0.3 * bh;
        // the tie holes
        for (int i = 0; i < 4; i++) {
            vec2 c = vec2(i % 2 == 0 ? -0.27 : 0.27, i < 2 ? -0.27 : 0.27);
            float dd = length(p - c);
            alb *= 1.0 - 0.32 * smoothstep(0.03, 0.018, dd);
            float lip = smoothstep(0.055, 0.035, dd) * smoothstep(0.02, 0.035, dd);
            bump.xy += (p - c) / max(dd, 1e-3) * lip * 0.25;
        }
        bump.xy += (vec2(nb(p * 7.0 + s1 * 3.0), nb(p * 7.0 + 5.0)) - 0.5) * 0.08;
    }
    vec3 nn = normalize(nO + bump);
    vec3 N = normalize(qrot(vQuat, nn));
    vec3 V = normalize(uEyeB - vWorld);
    vec3 L = normalize(uKeyDir);
    float ndl = max(dot(N, L), 0.0);
    vec3 amb = mix(uAmbBot, uAmbTop, N.y * 0.5 + 0.5);
    // a soft occlusion toward the panel's lower edge and the chamfers
    float edge = smoothstep(0.5, 0.36, max(abs(p.x), abs(p.y)));
    float occ = mix(0.8, 1.0, edge) * (0.92 + 0.08 * smoothstep(-0.5, 0.2, p.y));
    vec3 col = alb * (uKeyCol * ndl + amb) * occ;
    // a satin sheen, low
    vec3 H = normalize(L + V);
    col += uKeyCol * pow(max(dot(N, H), 0.0), 40.0) * 0.06;
    // a crisp darker rim of its own color on the silhouette
    float rim = smoothstep(0.43, 0.5, max(abs(p.x), abs(p.y)));
    col = mix(col, vCol * uRimCol, rim * 0.55);
    col = mix(col, vec3(dot(col, vec3(0.3, 0.55, 0.15))) * 0.5, uDull * vE.x);
    col *= 1.0 + 0.4 * vE.y;
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- flat: the backing, shadows, the ghost (soft quads)
inline const char* FLAT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
layout(location = 2) in vec4 aQ;     // local coords (-1..1), softness x, y
uniform mat4 uVP;
out vec4 vCol;
out vec4 vQ;
void main() {
    vCol = aCol;
    vQ = aQ;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* FLAT_FS = R"(#version 330 core
in vec4 vCol;
in vec4 vQ;
out vec4 frag;
void main() {
    vec2 q = abs(vQ.xy);
    float a = 1.0;
    if (vQ.z > 0.0) a *= smoothstep(1.0, 1.0 - vQ.z, q.x);
    if (vQ.w > 0.0) a *= smoothstep(1.0, 1.0 - vQ.w, q.y);
    frag = vec4(vCol.rgb, vCol.a * a);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uExposure, uSlow;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
float uPieceK = 0.0;
vec3 tonemap(vec3 c) {
    c *= mix(uExposure, 1.0, uPieceK);
    // a soft filmic shoulder (ACES fit), the toe kept
    vec3 x = max(c, 0.0);
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec4 s = texture(uScene, vUv);
    uPieceK = s.a;
    vec3 c = tonemap(s.rgb);
    float g = dot(c, vec3(0.3, 0.55, 0.15));
    c = mix(c, mix(vec3(g), c * uSlowTint, 0.7), uSlow * (1.0 - s.a));
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.18 * smoothstep(0.45, 0.95, length(q * vec2(1.0, 0.85))) * (1.0 - s.a);
    c = mix(c, uBridgeCol, uBridge * (1.0 - s.a));
    c = mix(c, uBridgeInk, uBridge * s.a);
    c *= (1.0 - 0.5 * uDim) * uFade;
    frag = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
)";

// ---------------------------------------------------------------- HUD
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

// A shader with the given blocks inserted after its version line.
inline std::string with(const char* fs, const char* a, const char* b = "", const char* c = "", const char* d = "") {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + a + b + c + d + s.substr(nl + 1);
}

} // namespace architect_shaders

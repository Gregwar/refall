#pragma once
// TERRARIUM's shaders: the world as meshes at the full resolution (the sky; the lab and the city around it, and what
// grows over them; the jar's contents: soil, moss, ferns, mushrooms, a pond, a sapling, flowers, succulents, sprouts,
// the beetle, the cork; the jar's glass, back and front, the front refracting the world behind it and cracking; the
// glassware and the shards; the lights in the air), the veil behind the well, the glass shard pieces, lettering.

namespace terrarium_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float hash1(float n) { return hash(vec2(n, n * 0.371 + 17.13)); }
float hash3(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(hash3(i), hash3(i + vec3(1, 0, 0)), u.x), mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), u.x), u.y);
    float b = mix(mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), u.x), mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), u.x), u.y);
    return mix(a, b, u.z);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float sdBox2(vec2 p, vec2 b) { vec2 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
)";

// ------------------------------------------------------------------------------------------------ the world
// The scene's uniforms and functions, shared by the lab and the city, the jar's contents, its glass and the glassware:
// the light (the lab's cold key and tubes, the day outside), the sky, the room's reflection, the fireflies' light, the
// soil, the condensation, the painted vines, the sun's shadow map.
inline const char* SCENE_GLSL = R"(
uniform vec3 uEye;
uniform float uTime, uPx, uSway, uSpots;
uniform vec3 uSunDir, uSunCol, uSkyZ, uSkyH, uAmbS, uAmbG, uFill, uGlowSun;   // (the lab's key light: uSunDir, uSunCol)
uniform vec3 uOutDir, uOutCol, uOutAmbS, uOutAmbG, uHaze;                       // the day outside
uniform vec2 uSunSky;
uniform float uSunDisc, uMoon, uStars, uRain, uFog, uCond, uNight, uFlyAmt;
uniform vec3 uBugC, uBugSpot, uFlyC, uSill, uFrame, uWall, uHill, uHill2;
uniform int uFlies;
uniform vec4 uFly[32];                   // the jar's fireflies: x, y, z, glow (world)
uniform mat4 uLightVP;
uniform sampler2DShadow uShadow;
uniform float uSoft;
// ---- the jar being drawn: its place, its palette, its pond and moss
uniform vec3 uFoot;
uniform float uScale;
uniform int uKind;
uniform vec4 uPondP;                     // pond center xz, radius, depth
uniform float uPondY, uPond, uMossCover, uInVine;
uniform vec3 uSoil, uSoil2, uGravel, uStr1, uStr2, uMoss, uMoss2, uLeaf, uLeaf2, uStem, uCap, uCap2, uFlower, uFlower2, uBark, uWater, uStone;

const float JR = 1.0, JHB = 0.8, JRN = 0.62, JHN = 0.93, JT = 0.022;
const float PI = 3.14159265, TAU = 6.2831853;
// the lab (world units, as lab.cpp's)
const vec3 LAB_LO = vec3(-30.3, -6.3, -20.3), LAB_HI = vec3(30.3, 14.3, 33.3);
const float GROUND = -53.0;
bool inLab(vec3 p) { return all(greaterThan(p, LAB_LO)) && all(lessThan(p, LAB_HI)); }

// the soil (the same function as the CPU's)
float soilH(vec2 q) {
    float h = 0.2;
    vec2 a = q - vec2(-0.5, -0.45), b = q - vec2(0.55, -0.5), c = q - vec2(0.0, -0.72);
    h += 0.055 * exp(-dot(a, a) * 5.0) + 0.045 * exp(-dot(b, b) * 6.0) + 0.035 * exp(-dot(c, c) * 4.0);
    h -= 0.02 * q.y;
    h += 0.006 * sin(q.x * 9.0 + 1.0) * sin(q.y * 8.0 + 2.0);
    float dp = length(q - uPondP.xy) / uPondP.z;
    h -= uPondP.w * (1.0 - smoothstep(0.5, 1.15, dp));
    return h;
}

// the jar's outer surface (a revolved profile: a wide body with round shoulders, a neck, a lip)
float sdRoundBox2(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}
float smin(float a, float b, float k) { float h = sat(0.5 + 0.5 * (b - a) / k); return mix(b, a, h) - k * h * (1.0 - h); }
float jarOuter(vec3 p) {
    vec2 q = vec2(length(p.xz), p.y);
    float body = sdRoundBox2(vec2(q.x, q.y - JHB * 0.5), vec2(JR, JHB * 0.5), vec4(0.36, 0.12, 0.36, 0.12));
    float neck = sdBox2(vec2(q.x, q.y - (JHB - 0.2 + JHN) * 0.5), vec2(JRN, (JHN - JHB + 0.2) * 0.5));
    float d = smin(body, neck, 0.07);
    d = min(d, length(vec2(q.x - JRN + 0.004, q.y - JHN + 0.012)) - 0.02); // the lip
    return d;
}

// the key light's shadow (soft: a dozen taps; offset toward the light, thin leaves lit from either side)
float shadowAt(vec3 wp, vec3 n) {
    vec3 no = dot(n, uSunDir) >= 0.0 ? n : -n;
    vec4 l = uLightVP * vec4(wp + no * 0.006 * uScale, 1.0);
    vec3 s = l.xyz * 0.5 + 0.5;
    if (s.x <= 0.0 || s.x >= 1.0 || s.y <= 0.0 || s.y >= 1.0 || s.z >= 1.0) return 1.0;
    const vec2 P[12] = vec2[12](vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
                                vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
                                vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));
    float r = 0.0;
    for (int i = 0; i < 12; i++) r += texture(uShadow, vec3(s.xy + P[i] * uSoft, s.z - 0.0003));
    return smoothstep(0.05, 0.95, r / 12.0);
}

// ---------------------------------------------------------------- the sky (over the city)
vec3 sky(vec3 rd) {
    // elevations a little stretched (E = 2.5 x the slope)
    float az = atan(rd.x, -rd.z);
    float E = rd.y / max(length(rd.xz), 1e-3) * 2.5;
    vec3 c = mix(uSkyH, uSkyZ, smoothstep(0.0, 1.4, E));
    c = mix(c, uSkyZ * 0.85, smoothstep(1.2, 2.5, E) * 0.5);
    // the sun's glow and disc
    vec2 sd = vec2(az - uSunSky.x, (E - uSunSky.y) * 0.4);
    float sr = length(sd);
    c += uGlowSun * (exp(-sr * 6.0) * 0.6 + exp(-sr * 30.0) * 0.6);
    c += uSunCol * 0.0 + uGlowSun * uSunDisc * smoothstep(0.03, 0.024, sr) * 1.4;
    // the moon
    vec2 md = vec2(az - 0.35, (E - 0.9) * 0.4);
    float mr = length(md);
    float moon = smoothstep(0.03, 0.025, mr) * (1.0 - 0.6 * smoothstep(0.025, 0.0, length(md - vec2(0.011, 0.003))));
    c += vec3(0.9, 0.93, 1.0) * uMoon * (moon * 1.4 + exp(-mr * 22.0) * 0.18);
    // stars
    if (uStars > 0.01) {
        vec2 sc = vec2(az * 90.0, E * 40.0);
        vec2 ci = floor(sc);
        vec2 h = hash2(ci);
        float star = smoothstep(0.2, 0.0, length(fract(sc) - h)) * step(0.84, hash(ci + 3.1));
        star *= 0.6 + 0.4 * sin(uTime * (0.8 + 2.0 * h.x) + h.y * 30.0);
        c += vec3(0.8, 0.85, 1.0) * star * uStars * smoothstep(0.3, 0.9, E) * 1.2;
    }
    // toon clouds drifting slowly: a lit top, a shaded belly
    {
        vec2 cp = vec2(az * 3.0 + uTime * 0.004, E * 0.9);
        float band = smoothstep(0.15, 0.5, E);
        float n = fbm(cp * vec2(1.4, 2.4) + vec2(0.0, 3.0)) + 0.08 * band - 0.04;
        float aa = max(fwidth(n), 0.004);
        float cl = smoothstep(0.6 - aa, 0.6 + aa, n) * band;
        float lit = smoothstep(0.59, 0.63, fbm(cp * vec2(1.4, 2.4) + vec2(0.0, 3.0) + vec2(0.0, -0.1)) + 0.08 * band - 0.04);
        vec3 lightC = mix(uGlowSun, vec3(1.0), 0.55) * (1.0 - 0.7 * uNight);
        vec3 cc = mix(mix(uSkyH, uSkyZ, 0.35) * 1.05, lightC * 1.2, 0.35 + 0.65 * lit);
        cc = mix(cc, uSkyZ * 1.4, uNight * 0.6);
        c = mix(c, cc, cl);
    }
    // the haze over the city, low
    c = mix(c, uHaze, smoothstep(0.35, -0.05, E));
    return c;
}

// the lab's reflection in the glass: the tubes overhead, the bright street window behind us, the walls, the bench
vec3 env(vec3 r) {
    vec3 wall = uWall * (uAmbS * 0.9 + uFill * 0.6) * 0.8;
    vec3 c = mix(vec3(0.03, 0.033, 0.036) * (uAmbS + uFill), wall, smoothstep(-0.25, 0.05, r.y));
    float az = atan(r.x, r.z);
    // the street window behind us: the day
    float win = smoothstep(0.62, 0.5, abs(az)) * smoothstep(-0.1, 0.05, r.y) * smoothstep(0.55, 0.4, r.y);
    c = mix(c, (uSkyH * 0.7 + uSkyZ * 0.3) * (1.0 - 0.6 * uNight), win * 0.8);
    // the tubes: bright bars overhead
    float tube = smoothstep(0.55, 0.75, r.y) * step(0.55, fract(r.x / max(r.y, 0.1) * 0.9 + 0.2));
    c += uFill * 1.2 * tube;
    return c;
}

// ---------------------------------------------------------------- painted greenery (vines up the glass)
// the light on painted things (and the jar's fireflies glowing around it at night)
vec3 paintLight(vec3 p, float up) {
    vec3 l = uAmbS * 0.85 + uFill * 0.7 + uSunCol * 0.3 * (0.55 + 0.45 * up);
    vec3 dj = p - vec3(0.0, 0.55, 0.0);
    l += uFlyC * uFlyAmt * 0.4 * exp(-dot(dj, dj) / 1.6);
    return l;
}
// a pointed leaf, local coordinates in its own radii: color, alpha
vec4 leafShape(vec2 l, vec3 c1, vec3 c2, float aa) {
    float w = 1.0 - l.x * l.x;
    float e = w - abs(l.y) * 1.15;
    float a = smoothstep(-aa, aa, e) * step(abs(l.x), 1.0);
    vec3 c = mix(c2, c1, 0.45 + 0.4 * l.y + 0.2 * l.x);
    c = mix(c, c1 * 1.2, smoothstep(0.07, 0.0, abs(l.y)) * step(-0.85, l.x) * 0.35);
    c *= 1.0 - 0.35 * smoothstep(0.18, 0.0, e);
    return vec4(c, a);
}
// a little flower: five round petals, a golden heart
vec4 flowerShape(vec2 l, vec3 pc, float aa) {
    float r = length(l);
    float ang = atan(l.y, l.x);
    float petal = 0.62 + 0.38 * cos(ang * 5.0);
    float a = smoothstep(petal + aa, petal - aa, r);
    vec3 c = mix(pc * 1.1, pc * 0.75, smoothstep(0.3, 1.0, r));
    c = mix(c, vec3(1.0, 0.82, 0.25), smoothstep(0.3, 0.2, r));
    return vec4(c, a);
}
// vines: strands along v from v0 (dir +1 up, -1 down), lane apart across u, each up to len long, leaves on alternate
// sides, flowers at some of them (aa: the antialiasing width in the leaves' own units, from the pixel's footprint)
vec4 vines(vec2 uv, float lane, float v0, float dir, float len, float leafS, float seed, float flAmt, float aaUv) {
    vec4 res = vec4(0.0);
    if (len <= 0.005) return res;
    float aa = max(aaUv, 1e-4) * 1.5 / leafS;
    float li = floor(uv.x / lane + 0.5);
    for (int j = -1; j <= 1; j++) {
        float idx = li + float(j);
        float h = hash1(idx * 7.1 + seed);
        if (h > 0.8) continue;
        float L = len * (0.55 + 0.45 * hash1(idx * 3.3 + seed));
        float sAl = (uv.y - v0) * dir;
        if (sAl < -leafS || sAl > L + leafS) continue;
        float xs = idx * lane + lane * 0.15 * (h - 0.5);
        float xc = xs + lane * 0.22 * sin(sAl * 7.0 + h * 20.0);
        float du = uv.x - xc;
        float stemA = smoothstep(0.006 + aaUv, 0.006 - aaUv, abs(du)) * step(0.0, sAl) * step(sAl, L);
        if (stemA > 0.0) res = vec4(mix(uStem, uLeaf2, 0.5), stemA);
        float gap = leafS * 1.25;
        float k = floor(sAl / gap + 0.5);
        for (int kk = -1; kk <= 0; kk++) {
            float kj = k + float(kk);
            float sk = kj * gap;
            if (sk < 0.0 || sk > L) continue;
            float side = mod(kj, 2.0) * 2.0 - 1.0;
            float xk = xs + lane * 0.22 * sin(sk * 7.0 + h * 20.0);
            float sc = leafS * (0.55 + 0.45 * smoothstep(L, L - 0.15, sk));
            vec2 cpos = vec2(xk + side * sc * 0.85, v0 + dir * (sk + sc * 0.3));
            vec2 lc = rot(side * 0.6 - dir * 0.3) * (uv - cpos);
            lc.x *= side;
            vec4 lf;
            if (hash1(kj * 1.7 + idx * 13.1 + seed) < flAmt) lf = flowerShape(lc / (sc * 0.62), hash1(kj + idx) < 0.5 ? uFlower : uCap, aa);
            else lf = leafShape(lc / vec2(sc, sc * 0.55), uLeaf, uLeaf2, aa);
            if (lf.a > res.a) res = lf;
        }
    }
    return res;
}

// ---------------------------------------------------------------- condensation on the glass
// droplets on the glass at (u, v) (u around the jar, v up): x, y the droplet's slope, z its mask, w the mist
vec4 drops(vec2 uv, float cov) {
    vec4 res = vec4(0.0, 0.0, 0.0, 0.0);
    if (cov < 0.01) return res;
    // trickles: a few drops running down, leaving clear trails
    float trail = 0.0;
    for (int k = 0; k < 5; k++) {
        float fk = float(k);
        float lane = -2.6 + fk * 1.25 + 0.3 * hash1(fk * 3.0);
        float speed = 0.012 + 0.008 * hash1(fk + 5.0);
        float yh = 1.0 - mod(uTime * speed + hash1(fk) * 2.0, 1.3);
        float du = uv.x - lane - 0.004 * sin(uv.y * 40.0 + fk);
        float inTrail = smoothstep(0.009, 0.004, abs(du)) * step(yh, uv.y) * smoothstep(yh + 0.6, yh, uv.y);
        trail = max(trail, inTrail);
        vec2 dq = vec2(du, uv.y - yh) / vec2(0.012, 0.015);
        float r2 = dot(dq, dq);
        if (r2 < 1.0 && cov > 0.3) res = vec4(dq * 0.9, 1.0, 0.0);
    }
    if (res.z > 0.5) return res;
    for (int l = 0; l < 2; l++) {
        float sc = l == 0 ? 0.045 : 0.017;
        vec2 p = uv / sc;
        vec2 ci = floor(p);
        vec2 h = hash2(ci + float(l) * 17.0);
        if (h.x > cov * (l == 0 ? 0.55 : 0.9)) continue;
        vec2 c = ci + 0.25 + 0.5 * hash2(ci + 3.3 + float(l));
        float r = (0.16 + 0.26 * h.y) * (0.6 + 0.5 * cov);
        vec2 dq = (p - c) / r;
        float r2 = dot(dq, dq);
        if (r2 < 1.0 && trail < 0.5) { res = vec4(dq, 1.0, 0.0); break; }
    }
    res.w = cov * (1.0 - res.z) * (1.0 - trail);
    return res;
}

// the lab along a ray, roughly (what a ray sees past a jar's thick glass edge): the bench below, the wall
vec3 background(vec3 ro, vec3 rd) {
    vec3 lab = uAmbS * 0.9 + uFill * 0.9 + uSunCol * 0.25;
    vec3 wall = uWall * lab * 0.85;
    if (rd.y < -0.02) return mix(wall, vec3(0.05, 0.052, 0.056) * lab, 0.35);
    return mix(wall, (uSkyH * 0.6 + uSkyZ * 0.4) * 0.6, smoothstep(0.0, 0.3, rd.y) * 0.3);
}

// the fireflies' light on a point of the jar being drawn (jar units)
vec3 flies(vec3 p, vec3 n) {
    vec3 c = vec3(0.0);
    for (int i = 0; i < uFlies; i++) {
        vec4 f = uFly[i];
        if (f.w <= 0.01) continue;
        vec3 L = (f.xyz - uFoot) / uScale - p;
        float d2 = dot(L, L);
        float w = sat(dot(n, L * inversesqrt(d2 + 1e-6)) * 0.6 + 0.4);
        c += f.w * w * (exp(-d2 / 0.01) * 0.7 + 0.00012 / (d2 + 0.002));
    }
    return c * uFlyC;
}
)";

// ---- the meshes: jar units, placed by the jar's foot and size; ferns and broad leaves sway about their slot; what
// creeps over the lab and the city grows out of its pivot from its line (two clocks: the bench's, the rest's)
inline const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aM;     // material, ao, seed, two-sided
layout(location = 3) in vec4 aF;     // the material's parameters
layout(location = 4) in vec4 aS;     // sway: pivot x, z, phase, amplitude
layout(location = 5) in vec4 aG;     // growth: pivot, the line it appears at (<= -999: always there)
uniform mat4 uVP;
uniform vec3 uFoot;
uniform float uScale, uTime, uSway;
uniform vec3 uLines;                 // the bench's clock, the room's and the city's, the surge (the drop)
out vec3 vL;
out vec3 vW;
out vec3 vN;
out vec4 vM;
out vec4 vF;
void main() {
    vec3 p = aPos, n = aNrm;
    if (aS.w > 0.0) {
        float a = aS.w * (uSway + uLines.z * 2.5) * sin(uTime * (0.7 + uLines.z * 1.5) + aS.z);
        vec2 c = aS.xy;
        p.xz = c + rot(a) * (p.xz - c);
        n.xz = rot(a) * n.xz;
    }
    if (aG.w > -999.0) {
        float L = aG.w < 26.5 ? uLines.x : uLines.y;
        float k = smoothstep(aG.w, aG.w + 0.6, L);
        p = aG.xyz + (p - aG.xyz) * k;
    }
    vL = p;
    vN = n;
    vM = aM;
    vF = aF;
    vW = uFoot + p * uScale;
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* SHADOW_FS = R"(#version 330 core
void main() {}
)";

// ---- the jars' contents: soil, moss, ferns, mushrooms, flowers, succulents, stones, the beetle, the snail, the cork
inline const char* CONTENT_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
out vec4 frag;
uniform int uOccN;
uniform vec4 uOcc[24];                   // the slots' feet: x, z, radius, strength (contact shadows on the soil)
uniform vec4 uBug2;                      // the beetle: walk phase, moving, look, blink
uniform int uClip;                       // keep inside the jar's cavity (what grows never shows through the glass)
vec3 toonOut(vec3 n, vec3 rd, vec3 alb) {
    float ndl = dot(n, uSunDir);
    float band = smoothstep(-0.05, 0.12, ndl) * 0.78 + smoothstep(0.5, 0.62, ndl) * 0.22;
    vec3 c = alb * (uSunCol * band + mix(uAmbG, uAmbS, n.y * 0.5 + 0.5) + uFill * 0.55 * sat(dot(n, normalize(vec3(0.3, 0.45, 1.0))) * 0.6 + 0.4));
    c += mix(uSunCol, uGlowSun, 0.5) * pow(1.0 - sat(dot(n, -rd)), 3.0) * 0.25;
    return c;
}
void main() {
    vec3 p = vL;
    float m = vM.x;
    if (uClip == 1 && (m < 18.5 || m > 19.5) && jarOuter(p) + JT > 0.0) discard;
    vec3 rd = normalize(vW - uEye);
    vec4 inf = vF;
    vec3 n = normalize(vN);
    if (vM.w > 0.5 && dot(n, rd) > 0.0) n = -n;
    vec3 alb = uSoil;
    float trans = 0.0, spec = 0.0, rim = 1.0;
    if (m > 18.5 && m < 19.5) {
        // the cork
        float sp = vnoise3(p * 180.0);
        vec3 a = mix(vec3(0.62, 0.42, 0.24), vec3(0.8, 0.6, 0.38), sp);
        a = mix(a, vec3(0.32, 0.2, 0.1), step(0.86, hash3(floor(p * 220.0))));
        frag = vec4(toonOut(n, rd, a) + a * uFlyC * uFlyAmt * 0.08, 1.0);
        return;
    }
    if (m > 1.5 && m < 2.5) {
        // the soil in section against the glass: gravel, a dark layer, the earth (sand art in the desert)
        float u = atan(p.x, p.z) * JR;
        vec2 sp = vec2(u, p.y);
        if (uKind == 1) {
            float w = 0.012 * sin(u * 6.0 + 1.0) + 0.008 * sin(u * 13.0);
            float y = p.y + w;
            alb = y < 0.065 ? uGravel : (y < 0.105 ? uStr1 : (y < 0.14 ? uStr2 : (y < 0.165 ? uStr1 * 1.1 : uSoil)));
            alb *= 0.92 + 0.12 * hash(floor(sp * 300.0));
        } else if (p.y < 0.072) {
            vec2 g = sp * 38.0;
            vec2 gi = floor(g);
            vec2 gh = hash2(gi);
            float r = length(fract(g) - 0.5 - (gh - 0.5) * 0.3);
            float aa = max(fwidth(r), 0.01);
            float stone = smoothstep(0.46 + aa, 0.46 - aa, r);
            alb = mix(uSoil2 * 0.5, uGravel * (0.7 + 0.5 * gh.x) * mix(vec3(1.0), vec3(0.85, 0.95, 1.1), gh.y), stone);
            alb *= 1.0 + 0.25 * smoothstep(0.35, 0.0, length(fract(g) - vec2(0.38, 0.62)));
        } else if (p.y < 0.088) {
            alb = mix(vec3(0.07, 0.065, 0.075), uMoss2 * 0.8, smoothstep(0.6, 0.8, vnoise(sp * vec2(90.0, 30.0))));
        } else {
            alb = uSoil * (0.75 + 0.4 * vnoise(sp * 60.0));
            float perl = smoothstep(0.18, 0.1, length(fract(sp * 70.0) - hash2(floor(sp * 70.0)))) * step(0.85, hash(floor(sp * 70.0) + 2.0));
            alb = mix(alb, vec3(0.85, 0.82, 0.75), perl);
            float root = smoothstep(0.004, 0.0, abs(fbm(sp * vec2(14.0, 9.0)) - 0.5 + 0.06 * sin(u * 30.0)) - 0.002);
            alb = mix(alb, uSoil2 * 1.6, root * 0.6 * step(0.11, p.y));
        }
        vec3 sc = alb * (uAmbS * 0.9 + uFill * 1.3 + uSunCol * 0.12) * (0.75 + 0.25 * smoothstep(0.0, 0.2, p.y));
        frag = vec4(sc + alb * flies(p, normalize(vec3(p.x, 0.0, p.z))) * 0.6, 1.0);
        return;
    }
    float o = vM.y;
    if (m < 1.5) {
        float nn = vnoise(p.xz * 60.0) * 0.6 + vnoise(p.xz * 160.0) * 0.4;
        alb = mix(uSoil2, uSoil, nn);
        alb *= 0.92 + 0.16 * smoothstep(0.55, 0.8, vnoise(p.xz * 120.0));
        if (uKind == 1) alb = mix(uSoil, uSoil * 1.15, nn);
        // the moss carpet spreading over the soil as the jar fills
        float cover = smoothstep(0.55, 0.5, fbm(p.xz * 5.0 + 3.0) * 0.6 + vnoise(p.xz * 18.0) * 0.4 + 0.55 - uMossCover);
        if (uKind != 1 && cover > 0.0) {
            vec3 mc = mix(uMoss2, uMoss, smoothstep(0.3, 0.8, vnoise(p.xz * 45.0)) * 0.7 + 0.3 * vnoise(p.xz * 130.0));
            alb = mix(alb, mc, cover);
            n = normalize(n + (vec3(vnoise(p.xz * 70.0), 0.0, vnoise(p.xz * 70.0 + 4.0)) - 0.5) * 1.0 * cover);
            trans = 0.2 * cover;
        }
        // contact shadows: the soil darker under what grows on it
        for (int i = 0; i < uOccN; i++) {
            vec4 c = uOcc[i];
            vec2 d = p.xz - c.xy;
            o *= 1.0 - c.w * exp(-dot(d, d) / max(c.z * c.z, 1e-5));
        }
    } else {
        // (things standing on the soil: darker at their foot)
        o *= mix(0.62, 1.0, smoothstep(-0.005, 0.07, p.y - soilH(p.xz)));
    }
    if (m < 1.5) {}
    else if (m < 3.2) {
        // moss: a fuzzy cushion, its tips bright
        vec3 q = inf.xyz;
        float f = vnoise3(q * 18.0 + inf.w * 7.0);
        alb = mix(uMoss2, uMoss, smoothstep(0.25, 0.8, f) * 0.7 + 0.3 * sat(q.y * 2.0 + 0.3));
        vec3 pn = p * 140.0;
        n = normalize(n + (vec3(vnoise3(pn), vnoise3(pn + 5.2), vnoise3(pn + 9.7)) - 0.5) * 1.1);
        alb *= 0.85 + 0.3 * smoothstep(0.35, 0.75, vnoise3(p * 260.0));
        float spore = step(0.985, hash3(floor(p * 160.0))) * smoothstep(0.35, 0.5, uMossCover);
        alb = mix(alb, uCap * 1.1, spore * smoothstep(0.1, 0.4, q.y));
        trans = 0.35;
    } else if (m < 3.8) {
        // the sapling's tufts of leaves
        alb = mix(uLeaf2, uLeaf, smoothstep(-0.4, 0.8, inf.y) * 0.8 + 0.2 * vnoise3(inf.xyz * 8.0));
        vec3 pn = p * 110.0;
        n = normalize(n + (vec3(vnoise3(pn), vnoise3(pn + 5.2), vnoise3(pn + 9.7)) - 0.5) * 0.9);
        trans = 0.6;
    } else if (m < 4.5) {
        // a frond: deep green at the stem, bright at the tips, a pale midrib; the curl brown and fuzzy
        alb = mix(uLeaf2, uLeaf, smoothstep(0.0, 0.9, inf.x) * 0.6 + 0.4 * smoothstep(0.1, 0.9, inf.y));
        alb = mix(alb, uLeaf * 1.25, smoothstep(0.08, 0.0, inf.y) * step(0.001, inf.y) * 0.4);
        if (inf.z > 0.5) alb = mix(uLeaf2, uStem, 0.45);
        trans = 0.8;
        spec = 0.3;
    } else if (m < 5.5) { alb = uStem; trans = 0.4; }
    else if (m < 6.5) {
        // a cap: amanita red with white warts in the forest; gills beneath
        vec3 q = inf.xyz;
        vec3 dir = normalize(q);
        vec2 sp = vec2(atan(dir.z, dir.x) * 2.0, dir.y * 4.0) + inf.w * 3.0;
        vec2 si = floor(sp * 1.6);
        float wd = length(fract(sp * 1.6) - hash2(si) * 0.5 - 0.25);
        float aa = max(fwidth(wd), 0.01);
        float wart = smoothstep(0.25 + aa, 0.25 - aa, wd) * step(0.0, q.y);
        alb = mix(uCap, uCap * 1.15, sat(q.y));
        alb = mix(alb, uCap2, wart * (uKind == 0 ? 1.0 : 0.0));
        if (q.y < 0.02) {
            float gill = 0.5 + 0.5 * sin(atan(q.z, q.x) * 40.0);
            alb = mix(uCap2 * 0.85, uCap2 * 0.6, gill);
        }
        spec = 0.5;
    } else if (m < 7.5) { alb = mix(uCap2 * 0.95, uCap2 * 0.75, sat(1.0 - inf.x)); trans = 0.3; }
    else if (m < 8.5) {
        float st = vnoise(vec2(atan(p.x, p.z) * 20.0, p.y * 140.0));
        alb = uBark * (0.75 + 0.45 * st);
        if (uKind != 1) alb = mix(alb, uMoss, smoothstep(0.62, 0.7, vnoise3(p * 60.0)) * smoothstep(0.3, 0.6, uMossCover) * 0.8);
    }
    else if (m < 9.5) { alb = mix(uFlower2, uFlower, smoothstep(0.15, 0.7, inf.x)); trans = 0.7; spec = 0.2; }
    else if (m < 10.5) { alb = vec3(1.0, 0.72, 0.15); }
    else if (m < 11.5) {
        alb = uStone * (0.85 + 0.25 * vnoise3(p * 40.0));
        alb = mix(alb, uStone * 1.35, step(0.9, hash3(floor(p * 250.0))) * 0.5);
        spec = 0.6;
    }
    else if (m < 12.5) {
        // the pond: the sky's light on it, its depth dark and clear, rings from drops falling from the lid
        vec2 q = p.xz - uPondP.xy;
        float ring = 0.0;
        for (int k = 0; k < 2; k++) {
            float per = 5.0 + float(k) * 2.3;
            float ph = mod(uTime + float(k) * 3.1, per);
            vec2 c0 = (hash2(vec2(floor((uTime + float(k) * 3.1) / per), float(k))) - 0.5) * uPondP.z;
            float dr = length(p.xz - uPondP.xy - c0) - ph * 0.04;
            ring += sin(dr * 220.0) * exp(-dr * dr * 4000.0) * exp(-ph * 0.6);
        }
        vec3 nw = normalize(vec3(ring * 0.25, 1.0, ring * 0.2));
        float fres = 0.08 + 0.92 * pow(1.0 - sat(dot(nw, -rd)), 4.0);
        float depth = sat((uPondY - soilH(p.xz)) / 0.05);
        vec3 deep = mix(uSoil2 * 0.6, uWater * 0.45, depth);
        vec3 refl = mix(uSkyH, uSkyZ, 0.4) * (0.8 - 0.5 * uNight) + uFlyC * uFlyAmt * 0.05;
        vec3 col = mix(deep * (uAmbS + uSunCol * 0.4), refl, fres);
        float spk = pow(sat(dot(reflect(rd, nw), uSunDir)), 200.0);
        col += uSunCol * spk * 2.0;
        // lily pads
        for (int k = 0; k < 3; k++) {
            vec2 c0 = (hash2(vec2(float(k) * 3.7, 2.0)) - 0.5) * uPondP.z * 1.0;
            vec2 dq = q - c0;
            float pr = 0.028 + 0.012 * hash1(float(k));
            float notch = smoothstep(0.25, 0.15, abs(atan(dq.y, dq.x) - float(k) * 2.0 + 1.0));
            float aa = max(fwidth(length(dq)), 0.0005);
            float pad = smoothstep(pr + aa * 0.5, pr - aa * 0.5, length(dq)) * (1.0 - notch * step(length(dq), pr)) * smoothstep(0.55, 0.85, uPond);
            vec3 pc = mix(uMoss, uMoss2, length(dq) / pr) * (uAmbS * 0.8 + uSunCol * 0.7 * sat(uSunDir.y));
            col = mix(col, pc, pad);
        }
        col += flies(p + vec3(0.0, 0.02, 0.0), vec3(0.0, 1.0, 0.0)) * 0.5;
        frag = vec4(col, 1.0);
        return;
    }
    else if (m < 13.5) {
        // succulent and tropical leaves: blue green, rosy tips; aloe dots; calathea stripes
        float x = inf.x, y = inf.y, style = inf.w;
        alb = mix(uLeaf2, uLeaf, smoothstep(0.0, 1.0, x) * 0.7 + 0.3 * inf.z * 0.5);
        if (style < 0.5) alb = mix(alb, uFlower2, smoothstep(0.75, 1.0, x) * 0.8);
        else if (style < 1.5) alb = mix(alb, vec3(0.92, 0.95, 0.9), step(0.75, hash(floor(vec2(x * 30.0, y * 8.0)))) * 0.7);
        else {
            float stripe = smoothstep(0.4, 0.55, abs(fract(x * 5.0 + abs(y) * 1.5) - 0.5) * 2.0);
            alb = mix(alb, mix(uLeaf, uFlower2, 0.5), stripe * smoothstep(0.35, 0.0, abs(y)) * 0.7 * step(0.3, x));
            alb = mix(alb, uLeaf * 1.3, smoothstep(0.03, 0.0, abs(y)) * step(0.3, x) * 0.6);
        }
        trans = style > 1.5 ? 0.85 : 0.35;
        spec = style < 0.5 ? 0.45 : 0.3;
    }
    else if (m < 14.5) {
        alb = mix(uLeaf2, uLeaf, inf.x * 0.5 + 0.25 + 0.25 * sat(inf.y));
        float sp = step(0.6, inf.x) * smoothstep(0.35, 0.05, length(fract(vec2(inf.z * 11.0 / TAU, inf.y * 9.0)) - 0.5));
        alb = mix(alb, vec3(1.0, 0.97, 0.9), sp * 0.8);
        spec = 0.3;
    }
    else if (m < 15.5) {
        // the beetle's shell: glossy, spotted (a ladybird in the forest), a seam down its back
        vec3 qs = inf.xyz;
        vec3 dir = normalize(qs / vec3(0.041, 0.031, 0.05));
        alb = uBugC;
        if (uSpots > 0.5) {
            float sp = 0.0;
            vec3 S[5] = vec3[5](vec3(0.55, 0.65, 0.3), vec3(0.62, 0.55, -0.35), vec3(0.3, 0.85, -0.05), vec3(0.4, 0.45, -0.75), vec3(0.75, 0.3, 0.05));
            for (int k = 0; k < 5; k++) {
                vec3 s = normalize(S[k]);
                float dd = length(vec3(abs(dir.x), dir.y, dir.z) - s);
                float aa = max(fwidth(dd), 0.01);
                sp = max(sp, smoothstep(0.22 + aa, 0.22 - aa, dd));
            }
            alb = mix(alb, uBugSpot, sp);
        } else {
            alb = mix(alb, uBugSpot, smoothstep(0.6, 0.9, sin(dir.x * 18.0) * 0.5 + 0.5) * 0.35);
        }
        alb = mix(alb, uBugSpot * 0.6, smoothstep(0.08, 0.04, abs(dir.x)) * step(0.2, dir.y));
        alb = mix(alb, vec3(0.08, 0.07, 0.1), smoothstep(0.62, 0.72, dir.z) * (uSpots > 0.5 ? 1.0 : 0.6));
        spec = 1.0;
    }
    else if (m < 16.5) { alb = vec3(0.07, 0.065, 0.09); spec = 0.6; }
    else if (m < 17.5) {
        // the face: kawaii, a pale face, two big glossy black eyes with their shines, a blush, a tiny smile
)"
R"(        vec3 hq = inf.xyz;
        vec3 dir = normalize(hq / vec3(0.031, 0.027, 0.025));
        alb = vec3(0.09, 0.08, 0.11);
        float face = smoothstep(0.05, 0.2, dir.z) * smoothstep(0.75, 0.5, dir.y);
        alb = mix(alb, uCap2 * vec3(1.0, 0.94, 0.88), face);
        float blink = uBug2.w;
        float eye = 0.0, shine = 0.0;
        float aa = max(fwidth(dir.x) + fwidth(dir.y), 0.008);
        for (int s = -1; s <= 1; s += 2) {
            vec3 e = normalize(vec3(float(s) * 0.4, 0.12, 0.9));
            vec3 dd = dir - e;
            dd.y /= max(1.0 - blink, 0.06);
            eye = max(eye, smoothstep(0.22 + aa, 0.22 - aa, length(dd * vec3(1.0, 0.8, 1.0))));
            shine = max(shine, smoothstep(0.062 + aa, 0.062 - aa, length(dd - vec3(-0.06, 0.08, 0.0))));
            shine = max(shine, smoothstep(0.03 + aa, 0.03 - aa, length(dd - vec3(0.06, -0.08, 0.0))));
            float bl = smoothstep(0.15, 0.06, length((dir - normalize(vec3(float(s) * 0.62, -0.2, 0.75))) * vec3(1.0, 1.4, 1.0)));
            alb = mix(alb, vec3(1.0, 0.45, 0.55), bl * 0.8 * face);
        }
        alb = mix(alb, vec3(0.03, 0.025, 0.05), eye);
        alb = mix(alb, vec3(1.0), shine * eye * (1.0 - blink));
        vec2 mq = vec2(dir.x, dir.y + 0.3);
        float smile = smoothstep(0.012 + aa, 0.012 - aa, abs(length(mq - vec2(0.0, 0.06)) - 0.065)) * step(mq.y, 0.04) * step(abs(mq.x), 0.06);
        alb = mix(alb, vec3(0.35, 0.12, 0.15), smile * face);
        spec = 0.5;
        rim = 0.5;
    }
    else if (m > 20.5 && m < 21.5) {
        alb = uStone * (0.8 + 0.3 * vnoise3(p * 30.0));
        float mossy = smoothstep(0.25, 0.55, n.y + 0.2 * (vnoise3(p * 40.0) - 0.5));
        alb = mix(alb, mix(uMoss2, uMoss, vnoise3(p * 90.0)), mossy * (uKind == 1 ? 0.0 : 1.0));
        trans = 0.2 * mossy;
    }
    else if (m > 22.5 && m < 23.5) { alb = vec3(0.86, 0.78, 0.7); trans = 0.3; spec = 0.4; }
    else if (m > 23.5 && m < 24.5) {
        vec3 qs = inf.xyz;
        float ang = atan(qs.y, qs.z);
        float r = length(qs.yz) / 0.044;
        float spiral = 0.5 + 0.5 * sin(ang * 1.0 + r * 14.0);
        alb = mix(vec3(0.9, 0.55, 0.25), vec3(0.55, 0.25, 0.12), smoothstep(0.4, 0.7, spiral));
        alb = mix(alb, vec3(1.0, 0.85, 0.6), smoothstep(0.1, 0.0, r - 0.05) * 0.5);
        spec = 0.8;
    }
    else if (m > 24.5) { alb = vec3(0.05, 0.04, 0.06); spec = 1.0; }
    else if (m > 21.5) {
        // the trickle: bright, clear water running down
        float st = smoothstep(0.4, 0.9, vnoise(vec2(inf.y * 300.0, inf.x * 9.0 - uTime * 3.0)));
        vec3 wc = mix(uWater * 1.3, vec3(0.92, 0.98, 1.0), 0.45 + 0.4 * st);
        frag = vec4(wc * (uAmbS * 1.1 + uFill * 0.9 + uSunCol * 0.35) + uFlyC * uFlyAmt * 0.1, 1.0);
        return;
    }
    else if (m < 18.5) { alb = mix(uLeaf2, uLeaf * 1.15, 0.5 + 0.5 * inf.x); alb = mix(alb, vec3(0.75, 1.0, 0.45), 0.25); trans = 0.8; }
    float sh = shadowAt(vW, n);
    float ndl = dot(n, uSunDir);
    float band = smoothstep(-0.05, 0.12, ndl) * 0.78 + smoothstep(0.5, 0.62, ndl) * 0.22;
    vec3 col = alb * uSunCol * band * sh;
    col += alb * uSunCol * trans * sat(-ndl * 0.8 + 0.25) * (0.35 + 0.65 * sh) * (0.5 + 0.8 * sat(dot(rd, uSunDir)));
    vec3 amb = mix(uAmbG, uAmbS, n.y * 0.5 + 0.5);
    col += alb * amb * o;
    col += alb * uFill * sat(dot(n, normalize(vec3(0.3, 0.45, 1.0))) * 0.6 + 0.4) * o * 0.55;
    float fr = pow(1.0 - sat(dot(n, -rd)), 3.0);
    col += mix(uSunCol, uGlowSun, 0.5) * fr * 0.3 * sh * rim * (0.4 + 0.6 * sat(dot(rd, uSunDir) + 0.3));
    if (spec > 0.0) {
        vec3 hv = normalize(uSunDir - rd);
        float sp = smoothstep(0.8, 0.86, pow(sat(dot(n, hv)), 60.0)) * sh;
        col += uSunCol * sp * spec * 0.6;
        // the room's window in a glossy shell
        float sr = smoothstep(0.9, 0.94, dot(reflect(rd, n), normalize(vec3(0.5, 0.45, 0.75))));
        col += mix(uSkyH, vec3(1.0), 0.5) * sr * spec * 0.35 * (1.0 - 0.7 * uNight);
    }
    vec3 fl = flies(p, n);
    col += alb * fl + fl * 0.08 * spec;
    frag = vec4(col, 1.0);
}
)";

// ---- the lab and the city (materials 50 and up; see lab.cpp): the room, its furniture and equipment, the tag, the
// buildings, the street, the neon; what grows over all of it (vines, roots, leaves, flowers, bark, crowns, moss)
inline const char* LAB_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
out vec4 frag;
uniform vec3 uLabC;                      // the ceiling tubes' cold light
uniform float uLabOn, uFlicker;          // how many tubes still work; the drop's flicker
uniform float uMossR, uCityGreen, uLitWin, uNeonOn;
uniform vec3 uCrackW;                    // where the moss spreads from (the crack)
uniform int uLightN;
uniform vec4 uLightP[16];                // the city's lights: where, reach
uniform vec4 uLightC[16];                // their color (0..2 neon, 3 a street lamp), sign
uniform vec3 uNeon0, uNeon1, uNeon2, uLampC;
uniform float uTagNo;                    // the specimen's number
uniform vec3 uLeafA, uLeafB;             // the growth's leaves (the variant's)

vec3 neonCol(float i) { return i < 0.5 ? uNeon0 : (i < 1.5 ? uNeon1 : uNeon2); }
// a sign's flicker: most burn steady, a few buzz, one is half dead; the drop makes them all stutter
float neonFlick(float s) {
    float h = hash1(s * 7.31 + 0.4);
    float t = uTime;
    float buzz = step(0.72, h) * step(0.8, hash1(floor(t * 14.0) + s * 3.7));
    float dead = step(0.9, h) * step(0.45, fract(t * 0.11 + h * 5.0)) * step(0.3, hash1(floor(t * 9.0) + s));
    float f = 1.0 - 0.75 * buzz - 0.85 * dead;
    f *= 1.0 - uFlicker * 0.8 * step(0.45, hash1(floor(t * 22.0) + s * 1.3));
    return f;
}
float band(float ndl) { return smoothstep(-0.05, 0.12, ndl) * 0.78 + smoothstep(0.5, 0.62, ndl) * 0.22; }

// the light at p (normal n, toward the eye -rd) on an albedo: in the lab its cold tubes and key, the windows' day;
// outside the sun (or the moon) and the sky; everywhere the city's lights and the fireflies
vec3 light(vec3 p, vec3 n, vec3 rd, vec3 alb, float trans, float spec, float ao) {
    vec3 col;
    vec3 keyD, keyC;
    if (inLab(p)) {
        keyD = uSunDir; keyC = uSunCol * uLabOn;
        float sh = shadowAt(p, n);
        col = alb * keyC * band(dot(n, keyD)) * sh;
        col += alb * uLabC * uLabOn * (0.45 + 0.55 * sat(n.y * 0.6 + 0.4)) * ao;
        col += alb * mix(uAmbG, uAmbS, n.y * 0.5 + 0.5) * ao;
        // the day through the windows behind (warm at the golden hour), stronger near them
        float wz = smoothstep(16.0, -19.0, p.z);
        col += alb * uGlowSun * 0.5 * sat(dot(n, normalize(vec3(0.0, 0.3, 1.0))) * 0.7 + 0.3) * (0.25 + 0.75 * wz) * ao;
        col += alb * keyC * trans * sat(-dot(n, keyD) * 0.8 + 0.25) * 0.4;
    } else {
        keyD = uOutDir; keyC = uOutCol;
        col = alb * keyC * band(dot(n, keyD));
        col += alb * mix(uOutAmbG, uOutAmbS, n.y * 0.5 + 0.5) * ao;
        col += alb * keyC * trans * sat(-dot(n, keyD) * 0.8 + 0.25) * 0.45;
    }
    // the city's lights
    for (int i = 0; i < uLightN; i++) {
        vec3 L = uLightP[i].xyz - p;
        float R = uLightP[i].w;
        float d2 = dot(L, L);
        if (d2 > R * R * 6.0) continue;
        float d = sqrt(d2);
        vec4 lc = uLightC[i];
        vec3 c = lc.x > 2.5 ? uLampC : neonCol(lc.x) * neonFlick(lc.y) * 1.6;
        float w = sat(dot(n, L / max(d, 1e-3)) * 0.75 + 0.25) * exp(-d2 / (R * R));
        col += alb * c * w * uNeonOn;
    }
    if (spec > 0.0) {
        vec3 hv = normalize(keyD - rd);
        col += keyC * spec * 0.5 * smoothstep(0.8, 0.86, pow(sat(dot(n, hv)), 60.0));
        float fr = pow(1.0 - sat(dot(n, -rd)), 4.0);
        col += env(reflect(rd, n)) * fr * spec * 0.5;
    }
    // a rim of light
    col += mix(keyC, uGlowSun, 0.5) * pow(1.0 - sat(dot(n, -rd)), 3.0) * 0.12 * ao;
    col += alb * flies(p, n);
    return col;
}

// the moss spreading: in the lab from the crack (its reach uMossR), in the city over everything facing up
float mossAt(vec3 p, vec3 n) {
    if (inLab(p) && (uMossR < 0.01 || length(p - uCrackW) * 0.7 > uMossR)) return 0.0;
    float nz = fbm(p.xz * 0.9 + p.y * 0.3) * 0.7 + vnoise(p.xz * 4.0 + p.y) * 0.3;
    if (inLab(p)) {
        if (uMossR < 0.01) return 0.0;
        float d = length(p - uCrackW) * (0.7 + 0.6 * nz);
        float up = n.y > 0.5 ? 1.0 : 0.55;
        float patches = smoothstep(0.42, 0.62, fbm(p.xz * 1.7 + p.y * 0.5) + 0.25 * smoothstep(uMossR * 0.6, 0.0, d));
        return smoothstep(uMossR, uMossR * 0.45, d) * up * patches;
    }
    if (uCityGreen < 0.001) return 0.0;
    float up = n.y > 0.5 ? 1.0 : 0.45;
    float low = smoothstep(60.0, 0.0, p.y - GROUND) * 0.4;
    float nzc = fbm(vec2(p.x + p.z, p.y) * 0.05) * 0.7 + vnoise(vec2(p.x + p.z, p.y) * 0.4) * 0.3;
    return smoothstep(0.62, 0.4, nzc - uCityGreen * 0.45 - low * uCityGreen) * up * smoothstep(0.0, 0.2, uCityGreen);
}
// moss: deep and fuzzy, its tips catching the light (sc: its scale, 1 in the lab, smaller in the city)
vec3 mossCol(vec3 p, float sc) {
    vec2 q = (p.xz + p.y * vec2(0.7, -0.4)) * sc;
    float a = vnoise(q * 9.0), b = vnoise(q * 37.0), c = hash(floor(q * 140.0));
    vec3 m = mix(uMoss2 * 0.42, uMoss * 0.58, sat(a * 0.75 + b * 0.35 - 0.1));
    m = mix(m, vec3(dot(m, vec3(0.3, 0.5, 0.2))), 0.22);
    m *= 0.78 + 0.3 * step(0.55, c) + 0.12 * step(0.9, c);
    return m;
}

// the windows of a facade (u along it, v up from the street; its storey and style): x the glass (0..1), y lit, z its
// shade at the head (the inset's shadow), w a frame
vec4 windows(float u, float v, float storey, float style, float seed, bool street, float aa) {
    float fl = floor(v / storey), fv = v - fl * storey;
    vec4 r = vec4(0.0);
    float gx, gy0, gy1, cw;
    if (style < 0.5) { cw = 8.6; gx = 0.45; gy0 = 4.0; gy1 = 18.0; }            // ribbon windows
    else if (style < 1.5) { cw = 9.0; gx = 2.3; gy0 = 5.0; gy1 = 17.0; }        // punched
    else { cw = 6.0; gx = 0.3; gy0 = 1.4; gy1 = storey - 0.3; }                 // curtain glass
    if (street && fl < 0.5) { cw = 14.0; gx = 0.6; gy0 = 1.5; gy1 = 13.5; }    // the shop fronts
    float cu = u / cw, ci = floor(cu), cf = (cu - ci) * cw;
    float inX = smoothstep(gx - aa, gx + aa, cf) * smoothstep(cw - gx + aa, cw - gx - aa, cf);
    float inY = smoothstep(gy0 - aa, gy0 + aa, fv) * smoothstep(gy1 + aa, gy1 - aa, fv);
    r.x = inX * inY;
    float h = hash(vec2(ci, fl) + seed * 37.0);
    r.y = step(1.0 - uLitWin * (street && fl < 0.5 ? 0.8 : 0.2), h) * (0.3 + 0.7 * hash(vec2(fl, ci) + seed * 11.0));
    r.z = smoothstep(gy1 - 2.5, gy1, fv) * r.x;
    r.w = (1.0 - inX) * inY;
    // a transom and a mullion in each (the shop fronts and the curtain glass without)
    if (!(street && fl < 0.5) && style < 1.5) {
        float mul = smoothstep(0.25 + aa, 0.25, abs(cf - cw * 0.5)) + smoothstep(0.2 + aa, 0.2, abs(fv - gy1 + 3.5));
        r.w = max(r.w, sat(mul) * r.x);
        r.x *= 1.0 - sat(mul);
    }
    r.y *= 0.6 + 0.4 * smoothstep(gy1, gy0, fv);
    return r;
}

// the specimen tag's print: a manila card, a red band, SPECIMEN as blocks, the number in seven segments, a
// barcode, ruled lines and a handwritten note, a stamp
float seg7(vec2 q, int d) {
    // q in the digit's box (0..1 x 0..1.6)
    const int M[10] = int[10](0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F);
    int m = M[d];
    float r = 0.0;
    float w = 0.13;
    vec2 a[7] = vec2[7](vec2(0.5, 1.6), vec2(1.0, 1.2), vec2(1.0, 0.4), vec2(0.5, 0.0), vec2(0.0, 0.4), vec2(0.0, 1.2), vec2(0.5, 0.8));
    for (int i = 0; i < 7; i++) {
        if (((m >> i) & 1) == 0) continue;
        vec2 c = a[i];
        bool hz = (i == 0 || i == 3 || i == 6);
        vec2 d2 = abs(q - c) - (hz ? vec2(0.38, w * 0.5) : vec2(w * 0.5, 0.34));
        r = max(r, step(max(d2.x, d2.y), 0.0));
    }
    return r;
}
vec3 tagPrint(vec2 uv) {
    vec2 q = uv * vec2(1.65, 1.0);
    vec3 card = vec3(0.62, 0.45, 0.22);
    card *= 0.92 + 0.12 * vnoise(uv * 60.0);
    vec3 ink = vec3(0.04, 0.035, 0.05), red = vec3(0.55, 0.05, 0.04);
    vec3 c = card;
    // the red band at the top, SPECIMEN as letter blocks on it
    if (uv.y > 0.8) {
        c = red;
        float lx = (q.x - 0.25) / 0.09;
        float li = floor(lx);
        if (li >= 0.0 && li < 8.0 && uv.y > 0.85 && uv.y < 0.95) {
            float f = fract(lx);
            vec2 g = vec2(f, (uv.y - 0.85) / 0.1);
            float hh = hash(vec2(li, 3.0));
            float letter = step(0.15, f) * step(f, 0.8) * (step(g.y, 0.18) + step(0.82, g.y) + step(g.x, 0.32) * step(0.2, hh) + step(0.66, g.x) * step(0.5, hh)
                           + step(abs(g.y - 0.5), 0.1) * step(0.35, hh));
            c = mix(c, card * 1.3, sat(letter));
        }
    }
    // the eyelet (its ring) at the top left
    float er = length((q - vec2(0.12, 0.85)) * vec2(1.0, 1.0));
    c = mix(c, vec3(0.75, 0.7, 0.6), smoothstep(0.065, 0.055, er));
    c = mix(c, vec3(0.06), smoothstep(0.03, 0.025, er));
    // the number: No. and two digits
    int n = int(uTagNo);
    vec2 dq = (q - vec2(0.2, 0.32)) / vec2(0.16, 0.16);
    if (dq.x > 0.0 && dq.x < 1.0 && dq.y > 0.0 && dq.y < 1.6) c = mix(c, ink, seg7(dq, (n / 10) % 10));
    dq.x -= 1.35;
    if (dq.x > 0.0 && dq.x < 1.0 && dq.y > 0.0 && dq.y < 1.6) c = mix(c, ink, seg7(dq, n % 10));
    // a barcode on the right
    if (q.x > 1.0 && q.x < 1.52 && uv.y > 0.3 && uv.y < 0.7) {
        float b = hash(vec2(floor(q.x * 140.0), 1.0));
        c = mix(c, ink, step(0.45, b));
    }
    // ruled lines and a scribbled note
    for (int k = 0; k < 2; k++) {
        float y = 0.18 - float(k) * 0.08;
        float line = smoothstep(0.006, 0.0, abs(uv.y - y)) * step(0.08, q.x) * step(q.x, 1.55);
        c = mix(c, ink * 2.0 + card * 0.5, line * 0.6);
        float sx = q.x;
        float scrib = smoothstep(0.012, 0.0, abs(uv.y - y - 0.03 - 0.018 * sin(sx * 90.0 + float(k) * 3.0) * sin(sx * 23.0)))
                      * step(0.12, sx) * step(sx, 0.9 - float(k) * 0.3);
        c = mix(c, vec3(0.05, 0.08, 0.3), scrib);
    }
    // a stamp, askew: a red ring
    vec2 sq = rot(0.3) * (q - vec2(1.2, 0.4));
    float st = abs(length(sq * vec2(1.0, 1.4)) - 0.2);
    c = mix(c, red * 1.2, smoothstep(0.03, 0.015, st) * 0.55 * step(0.3, vnoise(q * 40.0)));
    return c;
}
// a notebook's page: ruled lines, a margin, notes
vec3 pagePrint(vec2 p) {
    vec3 c = vec3(0.78, 0.77, 0.72);
    float ly = fract(p.y * 12.0);
    c = mix(c, vec3(0.35, 0.5, 0.75), smoothstep(0.06, 0.0, abs(ly - 0.5)) * 0.5);
    c = mix(c, vec3(0.8, 0.2, 0.2), smoothstep(0.012, 0.0, abs(p.x - 0.18)) * 0.6);
    float row = floor(p.y * 12.0);
    float wrote = step(hash(vec2(row, 7.0)), 0.75) * step(0.2, p.x) * step(p.x, 0.3 + 0.6 * hash(vec2(row, 2.0)));
    float sc = smoothstep(0.1, 0.0, abs(ly - 0.62 - 0.1 * sin(p.x * 140.0 + row) * sin(p.x * 37.0 + row * 2.0)));
    c = mix(c, vec3(0.08, 0.1, 0.25), sc * wrote);
    return c;
}

void main() {
    vec3 p = vW;
    vec3 rd = normalize(p - uEye);
    vec3 n = normalize(vN);
    if (vM.w > 0.5 && dot(n, rd) > 0.0) n = -n;
    float m = vM.x;
    vec4 f = vF;
    vec3 alb = vec3(0.5);
    vec3 emit = vec3(0.0);
    float trans = 0.0, spec = 0.0, ao = 1.0, moss = 0.0;
    bool lab = inLab(p);
    float aa = max(length(fwidth(p)), 1e-4);
    if (m < 50.5) {
        // the bench's top: dark resin, a fine speckle, satin
        alb = vec3(0.03, 0.033, 0.036) * (0.85 + 0.3 * vnoise(p.xz * 30.0)) + vec3(0.02) * step(0.97, hash(floor(p.xz * 90.0)));
        spec = 0.6;
        moss = mossAt(p, n);
    } else if (m < 51.5) {
        // the cabinets: pale laminate, doors and drawers (their seams)
        alb = vec3(0.5, 0.53, 0.52);
        float u = abs(n.z) > 0.5 ? p.x : p.z;
        float seamX = smoothstep(0.035 + aa, 0.035, abs(fract((u + 0.2) / 3.9 + 0.5) - 0.5) * 3.9);
        float seamY = smoothstep(0.035 + aa, 0.035, abs(p.y + 1.6));
        alb *= 1.0 - 0.6 * max(seamX, seamY);
        moss = mossAt(p, n) * 0.6;
    } else if (m < 52.5) {
        alb = vec3(0.4, 0.42, 0.44) * (0.9 + 0.15 * vnoise(vec2(p.x * 40.0, p.z * 2.0)));
        spec = 0.7;
    } else if (m < 53.5) {
        // the lab's walls: white tiles to the waist, pale paint above
        float u = abs(n.z) > 0.5 ? p.x : p.z;
)"
R"(        if (p.y < 1.8) {
            vec2 t = vec2(u, p.y) / 1.0;
            vec2 g = abs(fract(t) - 0.5);
            float grout = smoothstep(0.47, 0.49, max(g.x, g.y));
            alb = mix(vec3(0.7, 0.73, 0.72), vec3(0.3, 0.32, 0.32), grout);
            spec = 0.3;
        } else alb = uWall * (0.9 + 0.08 * vnoise(vec2(u, p.y) * 0.8));
        moss = mossAt(p, n) * 0.7;
    } else if (m < 54.5) {
        // the floor: vinyl tiles
        vec2 t = p.xz / 2.0;
        vec2 ti = floor(t);
        vec2 g = abs(fract(t) - 0.5);
        alb = vec3(0.22, 0.24, 0.24) * (0.9 + 0.15 * hash(ti)) * (1.0 - 0.3 * smoothstep(0.47, 0.5, max(g.x, g.y)));
        spec = 0.3;
        moss = mossAt(p, n);
    } else if (m < 55.5) {
        vec2 g = abs(fract(p.xz / 4.0) - 0.5);
        alb = vec3(0.6, 0.61, 0.6) * (1.0 - 0.25 * smoothstep(0.48, 0.5, max(g.x, g.y)));
    } else if (m < 56.5) {
        // a ceiling tube's diffuser: lit while it still works (they fail one by one)
        float on = step(hash1(f.x * 3.7 + 1.0) * 0.9, uLabOn - 0.1) * (1.0 - uFlicker * step(0.5, hash1(floor(uTime * 18.0) + f.x)));
        emit = uLabC * (0.4 + 2.6 * on);
        alb = vec3(0.0);
    } else if (m < 57.5) {
        alb = f.z > 0.5 ? pagePrint(vec2(f.x, f.y)) : tagPrint(vec2(f.x, f.y));
        if (f.z > 0.5) alb = pagePrint(vec2((p.x + 2.95) / 1.35, (p.z - 0.4) / 0.95));
        trans = 0.15;
    } else if (m < 58.5) {
        alb = f.rgb;
        spec = f.w;
        moss = mossAt(p, n) * 0.5;
    } else if (m < 59.5) {
        // the monitor: a cold screen, lines of text, a graph
        vec2 s = vec2(p.x + 21.2, p.y - 1.5) / vec2(4.4, 2.6);
        vec3 bg = vec3(0.02, 0.06, 0.08);
        float rowi = floor(s.y * 14.0);
        float txt = step(0.08, s.x) * step(s.x, 0.15 + 0.5 * hash(vec2(rowi, 1.0))) * step(0.3, fract(s.y * 14.0)) * step(0.5, hash(vec2(floor(s.x * 40.0), rowi)));
        float graph = smoothstep(0.02, 0.0, abs(s.y - 0.35 - 0.15 * sin(s.x * 20.0 + uTime) * s.x)) * step(0.6, s.x);
        emit = bg + vec3(0.2, 0.8, 0.7) * (txt * 0.6 + graph) * uLabOn * 0.8;
        alb = vec3(0.0);
    } else if (m < 60.5 || (m > 60.5 && m < 62.5)) {
        if (m < 60.5) {
            // concrete: panels, rain streaks, the windows; the roofs gravel
            float seed = f.x, storey = f.y, style = f.z;
            alb = mix(vec3(0.34, 0.33, 0.31), vec3(0.46, 0.44, 0.41), seed) * (0.88 + 0.16 * vnoise(p.xz * 0.3 + p.y * 0.2));
            if (abs(n.y) > 0.5) {
                alb = vec3(0.26, 0.26, 0.25) * (0.8 + 0.3 * vnoise(p.xz * 2.0));
                moss = mossAt(p, n) * 1.2;
            } else {
                float u = abs(n.z) > 0.5 ? p.x : p.z;
                float v = p.y - GROUND;
                float streak = smoothstep(0.55, 0.85, vnoise(vec2(u * 0.4, v * 0.012))) * smoothstep(0.3, 0.7, vnoise(vec2(u * 0.1, v * 0.05)));
                alb *= 1.0 - 0.3 * streak;
                if (style < 8.5) {
                    float waa = max(aa * 1.2, 0.05);
                    vec4 w = windows(u, v, storey, style, seed, f.w > 0.5 && n.z > 0.5, waa);
                    // far away, the pattern melts into its average
                    float far = smoothstep(0.6, 2.0, waa);
                    vec3 glass = mix(uHaze * 0.4, uSkyZ * 0.35, 0.5 + 0.5 * reflect(rd, n).y) * 0.55 + vec3(0.012);
                    vec3 litC = mix(vec3(1.0, 0.58, 0.24), vec3(0.55, 0.78, 1.0), step(0.7, hash(vec2(floor(u / 9.0), floor(v / storey)) + 1.3))) * 1.1;
                    float wx = mix(w.x, 0.45, far);
                    alb = mix(alb, glass, wx);
                    alb *= 1.0 - 0.45 * w.z * (1.0 - far);
                    emit += litC * w.y * mix(w.x, 0.45, far) * uLitWin;
                    alb = mix(alb, vec3(0.12, 0.13, 0.13), w.w * 0.4 * (1.0 - far));
                    spec = 0.6 * wx;
                    moss = mossAt(p, n) * (1.0 - 0.7 * wx);
                } else moss = mossAt(p, n);
            }
        } else if (m < 61.5) {
            // asphalt: its lane lines, its cracks with grass
            alb = vec3(0.07, 0.072, 0.075) * (0.8 + 0.35 * vnoise(p.xz * 0.5));
            float lane = smoothstep(0.5, 0.3, abs(p.z - 88.0)) * step(0.5, fract(p.x / 12.0));
            alb = mix(alb, vec3(0.6, 0.55, 0.35), lane * 0.6);
            float crack = smoothstep(0.03, 0.0, abs(fbm(p.xz * 0.05) - 0.5));
            moss = max(mossAt(p, n) * 0.6, crack * smoothstep(0.1, 0.5, uCityGreen));
        } else {
            alb = vec3(0.3, 0.29, 0.27);
            vec2 g = abs(fract(p.xz / 6.0) - 0.5);
            alb *= 1.0 - 0.3 * smoothstep(0.46, 0.5, max(g.x, g.y));
            moss = mossAt(p, n) * 1.2;
        }
    } else if (m < 63.5) {
        // a neon tube: its color, burning (at night), flickering
        vec3 c = neonCol(f.x);
        float on = neonFlick(f.y) * uNeonOn;
        emit = c * (0.25 + 3.0 * on) + vec3(1.0) * 0.6 * on * pow(sat(dot(n, -rd)), 3.0);
        alb = c * 0.2;
    } else if (m < 64.5) {
        emit = uLampC * (0.2 + 3.0 * uNeonOn);
        alb = vec3(0.0);
    } else if (m < 65.5) {
        alb = mix(uSoil2, uSoil, vnoise(p.xz * 40.0));
        moss = smoothstep(0.55, 0.7, vnoise(p.xz * 18.0)) * 0.7;
    } else if (m < 66.5) {
        // agar in a petri dish: colonies
        vec2 q = p.xz * 9.0 + f.x * 7.0;
        vec2 qi = floor(q);
        float col = smoothstep(0.32, 0.22, length(fract(q) - 0.5 - (hash2(qi) - 0.5) * 0.3)) * step(0.7, hash(qi));
        alb = mix(vec3(0.62, 0.5, 0.22), mix(vec3(0.85, 0.8, 0.7), uCap, step(0.5, hash(qi + 3.0))), col);
        spec = 0.8;
    } else if (m < 70.5) {
        alb = mix(uStem, uLeafB, 0.4) * (0.85 + 0.2 * vnoise(vec2(f.y * 30.0, 0.0)));
        trans = 0.3;
    } else if (m < 71.5) {
        // a leaf: deep at its base, bright at its tip, a pale midrib, veins
        float u = f.x, v = f.y;
        float hue = f.z;
        alb = mix(uLeafB, uLeafA, smoothstep(0.0, 1.0, u) * 0.6 + 0.4 * (1.0 - abs(v)));
        alb *= 0.85 + 0.3 * hue;
        alb = mix(alb, uLeafA * 1.25, smoothstep(0.08, 0.0, abs(v)) * 0.5);
        float vein = smoothstep(0.06, 0.0, abs(fract(u * 5.0 - abs(v) * 1.5) - 0.5) - 0.42) * (1.0 - abs(v));
        alb = mix(alb, uLeafA * 1.15, vein * 0.25);
        trans = 0.7;
        spec = 0.25;
    } else if (m < 72.5) {
        alb = mix(uFlower2, uFlower, smoothstep(0.15, 0.8, f.x));
        alb = mix(alb, vec3(1.0, 0.82, 0.25), smoothstep(0.2, 0.0, f.x));
        trans = 0.7;
    } else if (m < 73.5) {
        // a root: pale at its young tip, dark and corky toward the crack
        alb = mix(vec3(0.16, 0.1, 0.06), vec3(0.42, 0.33, 0.22), smoothstep(0.3, 1.0, f.x)) * (0.8 + 0.3 * vnoise(vec2(f.y * 60.0, 1.0)));
        trans = 0.1;
    } else if (m < 74.5) {
        // bark
        float st = vnoise(vec2(atan(n.x, n.z) * 3.0, f.y * 0.8));
        alb = mix(vec3(0.16, 0.12, 0.09), vec3(0.32, 0.27, 0.22), st);
        moss = smoothstep(0.55, 0.7, vnoise(p.xz * 0.3 + p.y * 0.2)) * 0.7;
    } else if (m < 75.5) {
        // a crown of leaves: lumpy clusters, lit at their tops, leaf flecks
        vec3 q = f.xyz;
        float fl = vnoise3(p * 0.9 + f.w);
        alb = mix(uLeafB, uLeafA, smoothstep(0.2, 0.8, fl) * 0.6 + 0.4 * sat(q.y * 0.7 + 0.4));
        vec3 pn = p * 1.6;
        n = normalize(n + (vec3(vnoise3(pn), vnoise3(pn + 5.2), vnoise3(pn + 9.7)) - 0.5) * 0.9);
        ao = 0.6 + 0.4 * sat(q.y * 0.6 + 0.6);
        trans = 0.5;
    } else {
        // a moss cushion
        vec3 q = f.xyz;
        alb = mix(uMoss2, uMoss, smoothstep(0.25, 0.8, vnoise3(q * 6.0 + f.w)) * 0.7 + 0.3 * sat(q.y * 2.0 + 0.3));
        vec3 pn = p * 120.0;
        n = normalize(n + (vec3(vnoise3(pn), vnoise3(pn + 5.2), vnoise3(pn + 9.7)) - 0.5) * 1.0);
        trans = 0.35;
    }
    if (moss > 0.01) {
        // (its edge fuzzy, broken into tufts)
        float sc = lab ? 1.0 : 0.06;
        float tuft = vnoise((p.xz + p.y) * 45.0 * sc);
        float mk = smoothstep(0.3, 0.6, moss + (tuft - 0.5) * 0.35);
        alb = mix(alb, mossCol(p, sc), mk);
        n = normalize(n + (vec3(vnoise(p.xz * 80.0 * sc), 0.0, vnoise(p.zx * 80.0 * sc + 3.0)) - 0.5) * 0.8 * mk);
        spec *= 1.0 - mk;
        ao *= 1.0 - 0.25 * mk * (1.0 - tuft);
        trans = max(trans, 0.2 * mk);
    }
    vec3 col = light(p, n, rd, alb, trans, spec, ao) + emit;
    if (!lab) {
        // the city's air: a haze with the distance
        float d = length(p - uEye);
        col = mix(col, uHaze, (1.0 - exp(-max(d - 120.0, 0.0) / 1400.0)) * 0.9);
    }
    frag = vec4(col, 1.0);
}
)";

// ---- the sky behind everything (a full-screen pass after the world, at the far plane: only where nothing is)
inline const char* SKY_VS = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);
}
)";
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uCamF, uCamR, uCamU;
uniform float uTanH, uAspect;
void main() {
    vec2 q = vUv * 2.0 - 1.0;
    vec3 rd = normalize(uCamF + uCamR * (q.x * uTanH * uAspect) + uCamU * (q.y * uTanH));
    frag = vec4(sky(rd), 1.0);
}
)";

// ---- the jar's glass. The back (uFront 0: the cavity's far side, seen through the jar): the room's faint
// reflection, mist and drops, the vines painted up its inside, laid over what is behind. The front (uFront 1): the
// outer surface, refracting what is behind it (the world drawn so far, read back with its depth), the drops acting
// as lenses, the low mist inside the jar integrated over the ray's way through it, the vines, the tint thicker toward
// the edges, the condensation, the fresnel reflection of the room, a rim; and the crack: an impact's star of
// fractures spreading from a point low on the left, then a hole where pieces fell out (nothing drawn there).
inline const char* GLASS_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
out vec4 frag;
uniform int uFront;
uniform sampler2D uBack;                 // the world behind the glass (resolved)
uniform sampler2D uBackDepth;
uniform mat4 uVP;
uniform vec3 uFwd;
uniform vec2 uNF;                        // near, far
uniform vec2 uCrackUV;                   // the crack's center on the glass (u: angle, v: height)
uniform float uCrack, uHole;             // how far the fractures reach (0..1), how much fell out (0..1)
float viewZ(float d) { float z = d * 2.0 - 1.0; return 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x)); }

// the fractures at uv: x the distance to the nearest (uv units), y 1 where the glass is gone, z 1 on the hole's rim
vec3 cracks(vec2 uv, float aaUv) {
    vec2 d = uv - uCrackUV;
    float r = length(d) + 1e-4;
    float reach = uCrack * 0.62, holeR = uHole * 0.16;
    if (r > reach * 1.7 + 0.05) return vec3(1.0, 0.0, 0.0);
    float a = atan(d.y, d.x);
    // cells in (angle, log radius): wedges, longer outward, like an impact's star
    const float NA = 11.0;
    vec2 q = vec2(a / TAU * NA, log(r / 0.012) * 2.0);
    vec2 qi = floor(q), qf = q - qi;
    float F1 = 9.0, F2 = 9.0;
    vec2 id1 = vec2(0.0), id2 = vec2(0.0);
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 id = qi + g;
            id.x = mod(id.x, NA);
            vec2 o = 0.15 + 0.7 * hash2(id + 4.0);
            float dd = length(g + o - qf);
            if (dd < F1) { F2 = F1; id2 = id1; F1 = dd; id1 = id; }
            else if (dd < F2) { F2 = dd; id2 = id; }
        }
    float rc1 = 0.012 * exp((id1.y + 0.5) / 2.0), rc2 = 0.012 * exp((id2.y + 0.5) / 2.0);
    float scale = r * 0.42;
    float edge = (F2 - F1) * scale;
    // a fracture shows where both cells are within the reach
    float show = smoothstep(reach, reach * 0.7, max(rc1, rc2));
    float dist = mix(1.0, edge, show);
    // gone: cells near the center (most of them), the hole's rim where a gone cell meets a kept one
    float gone1 = step(rc1, holeR) * step(0.22, hash(id1 + 9.0));
    float gone2 = step(rc2, holeR) * step(0.22, hash(id2 + 9.0));
    float rim = gone2 * (1.0 - gone1) * smoothstep(0.012 + aaUv, 0.0, edge);
    // long fractures running on past the star
    for (int k = 0; k < 5; k++) {
        float ak = hash1(float(k) * 3.7 + 1.0) * TAU;
        vec2 dir = vec2(cos(ak), sin(ak));
        float along = dot(d, dir);
        float len = reach * (1.2 + 0.6 * hash1(float(k) + 9.0));
        if (along < 0.0 || along > len) continue;
        float perp = dot(d, vec2(-dir.y, dir.x)) + 0.012 * sin(along * 55.0 + float(k)) + 0.006 * sin(along * 140.0);
        dist = min(dist, abs(perp) + 0.004 * along / len);
    }
    return vec3(dist, gone1, rim);
}

void main() {
    vec3 rd = normalize(vW - uEye);
    vec3 N = normalize(vN);
    vec3 p0 = vL;
    vec2 uv = vec2(atan(p0.x, p0.z) * JR, p0.y);
    float aaUv = length(fwidth(uv));
    if (uFront == 0) {
        if (dot(N, rd) < 0.0) discard;
        vec3 NB = -N;
        float fb = 0.04 + 0.5 * pow(1.0 - sat(dot(-NB, rd)), 5.0);
        vec3 c = env(reflect(rd, NB)) * 0.6 * fb;
        float a = fb;
        float covb = uCond * (0.15 + 0.85 * smoothstep(0.45, 0.8, p0.y)) * smoothstep(0.36, 0.5, p0.y) * 0.6;
        vec4 db = drops(uv, covb);
        vec3 mist = mix(uSkyH, vec3(1.0), 0.4) * (0.75 - 0.5 * uNight) * 0.6 + uAmbS * 0.5 + uFill * 0.3;
        float mk = db.w * 0.3 + db.z * 0.15;
        c = c * (1.0 - mk) + mist * mk;
        a = 1.0 - (1.0 - a) * (1.0 - mk);
        vec4 vb = vines(uv, 0.24, 0.19, 1.0, uInVine * 0.62, 0.04, 1.0, 0.15 * uFlyAmt, aaUv);
        if (vb.a > 0.0) {
            c = mix(c, vb.rgb * paintLight(p0, 0.4) * 0.85, vb.a);
            a = mix(a, 1.0, vb.a);
        }
        frag = vec4(c, a);
        return;
    }
    if (dot(N, rd) > 0.0) discard;
    vec3 ck = uCrack > 0.001 ? cracks(uv, aaUv) : vec3(1.0, 0.0, 0.0);
    if (ck.y > 0.5) discard;
    float crackL = smoothstep(0.0025 + aaUv * 0.8, 0.0008, ck.x);
    float nearHole = smoothstep(0.32, 0.1, length(uv - uCrackUV)) * smoothstep(0.0, 0.3, uHole);
    float cosv = sat(dot(N, -rd));
    float cov = uCond * (0.15 + 0.85 * smoothstep(0.45, 0.8, p0.y)) * smoothstep(0.36, 0.5, p0.y) * (1.0 - nearHole);
    vec4 dr = drops(uv, cov);
    vec3 tng = normalize(cross(vec3(0.0, 1.0, 0.0), N));
    vec3 rdIn = normalize(refract(rd, N, 0.94) + (tng * dr.x + vec3(0.0, 1.0, 0.0) * dr.y) * 0.06 * dr.z + tng * crackL * 0.05);
    vec3 pIn = p0 + rdIn * (JT * 1.6);
    // the bent ray through the jar, marched against the world's depth: where it meets something, or where it leaves
    // the jar's body (then the world behind, as it shows there); a ray still in the glass's thickness (its edges) sees
    // the room past it
    float a2 = dot(rdIn.xz, rdIn.xz), b2 = dot(pIn.xz, rdIn.xz), c2 = dot(pIn.xz, pIn.xz) - 0.978 * 0.978;
    float h2 = b2 * b2 - a2 * c2;
    float tExit = h2 > 0.0 ? (-b2 + sqrt(h2)) / max(a2, 1e-5) : 0.0;
    if (rdIn.y > 0.0) tExit = min(tExit, max(JHN - pIn.y, 0.0) / rdIn.y);
    if (rdIn.y < 0.0) tExit = min(tExit, max(pIn.y - JT, 0.0) / -rdIn.y);
    tExit = clamp(tExit, 0.0, 2.2);
    float ti = tExit;
    vec2 suv;
    vec3 inner;
    bool edge = jarOuter(pIn) + JT > 0.0 && pIn.y > soilH(pIn.xz) + 0.004;
    if (edge) {
        ti = 0.0;
        inner = background(uFoot + (pIn + rdIn * 0.02) * uScale, rdIn);
        vec3 NB = -N;
        float fb = 0.04 + 0.5 * pow(1.0 - sat(dot(-NB, rdIn)), 5.0);
        inner = mix(inner, env(reflect(rdIn, NB)) * 0.6, fb);
    } else {
        const int STEPS = 24;
        float tPrev = 0.0, tBehind = -1.0;
        bool found = false;
        for (int i = 1; i <= STEPS; i++) {
            float t = tExit * float(i) / float(STEPS);
            vec4 c = uVP * vec4(uFoot + (pIn + rdIn * t) * uScale, 1.0);
            float zs = viewZ(texture(uBackDepth, c.xy / c.w * 0.5 + 0.5).r);
            if (c.w <= zs + 0.002 * uScale) { tPrev = t; continue; }
            float t0 = tPrev, t1 = t, diff = 0.0;
            for (int k = 0; k < 6; k++) {
                float tm = (t0 + t1) * 0.5;
                vec4 cm = uVP * vec4(uFoot + (pIn + rdIn * tm) * uScale, 1.0);
                float d = cm.w - viewZ(texture(uBackDepth, cm.xy / cm.w * 0.5 + 0.5).r);
                if (d > 0.0) { t1 = tm; diff = d; }
                else t0 = tm;
            }
            if (diff < 0.05 * uScale) { ti = t1; found = true; break; }
            if (tBehind < 0.0) tBehind = t1;
            tPrev = t;
        }
        vec4 c = uVP * vec4(uFoot + (pIn + rdIn * ti) * uScale, 1.0);
        suv = clamp(c.xy / c.w * 0.5 + 0.5, vec2(0.0), vec2(1.0));
        if (!found && tBehind >= 0.0) {
            suv = gl_FragCoord.xy / vec2(textureSize(uBack, 0));
            ti = tBehind;
        }
        inner = texture(uBack, suv).rgb;
    }
    {
        // the low mist over the soil (exponential in height), over the way through the jar (it leaks out once
        // the glass is broken)
        float y0 = pIn.y, dy = rdIn.y;
        float H = 0.16;
        float a = exp(-(y0 - 0.22) / H);
        float integ = abs(dy) > 1e-4 ? a * H * (1.0 - exp(-dy * ti / H)) / dy : a * ti;
        float wisp = 0.6 + 0.8 * vnoise(vec2(pIn.x * 3.0 + uTime * 0.03, pIn.y * 2.0 + uTime * 0.01));
        float fogA = 1.0 - exp(-max(integ, 0.0) * uFog * 1.2 * wisp);
        vec3 fogC = mix(uSkyH, vec3(1.0), 0.45) * (0.85 - 0.65 * uNight) * 0.5 + uAmbS * 0.5 + uFill * 0.3 + uFlyC * uFlyAmt * 0.025;
        inner = mix(inner, fogC, fogA * 0.6 * (1.0 - 0.6 * uHole));
    }
    {
        vec4 vi = vines(uv, 0.24, 0.19, 1.0, uInVine * 0.62, 0.04, 1.0, 0.15 * uFlyAmt, aaUv);
        if (vi.a > 0.0) inner = mix(inner, vi.rgb * paintLight(p0, 0.5), vi.a);
    }
    vec3 tint = vec3(0.9, 0.97, 0.95);
    inner *= mix(vec3(1.0), tint * tint * 0.85, pow(1.0 - cosv, 2.5));
    vec3 mist = mix(uSkyH, vec3(1.0), 0.5) * (0.85 - 0.6 * uNight) * 0.5 + uAmbS * 0.55 + uFill * 0.35 + uFlyC * uFlyAmt * 0.05;
    inner = mix(inner, mist, dr.w * 0.3);
    if (dr.z > 0.5) {
        float r2 = dot(dr.xy, dr.xy);
        vec3 lightC = mix(uSkyH, vec3(1.0), 0.6) * (1.0 - 0.6 * uNight) * 0.5 + uFill * 0.5 + uFlyC * uFlyAmt * 0.35;
        inner = mix(inner, mist, 0.25 * cov);
        inner *= 1.0 - 0.22 * smoothstep(0.7, 1.0, r2);
        float hi = smoothstep(0.16, 0.04, length(dr.xy - vec2(-0.3, 0.38)));
        float lo = smoothstep(0.45, 0.1, length(dr.xy - vec2(0.15, -0.5))) * smoothstep(0.3, 0.9, r2);
        inner += lightC * (hi * 0.8 + lo * 0.22);
    }
    float F = 0.035 + 0.965 * pow(1.0 - cosv, 5.0);
    vec3 col = mix(inner, env(reflect(rd, N)), F);
    col += mix(uSkyH, vec3(1.0), 0.4) * (1.0 - 0.6 * uNight) * smoothstep(0.16, 0.0, cosv) * 0.25 + uFill * smoothstep(0.16, 0.0, cosv) * 0.15;
    // the fractures: bright where their faces catch the light, a dark line in them; the hole's rim a thick edge
    {
        vec3 catchC = uFill * 1.3 + uSunCol * 0.5 + uAmbS * 0.6;
        float halo = smoothstep(0.012 + aaUv, 0.0, ck.x) * 0.25;
        col = mix(col, catchC, crackL * 0.75 + halo * 0.4);
        col *= 1.0 - 0.5 * smoothstep(0.0009 + aaUv * 0.4, 0.0, ck.x) * crackL;
        col = mix(col, catchC * 1.3 + vec3(0.1, 0.14, 0.13), ck.z * 0.85);
    }
    col += uGlowSun * 0.25 * smoothstep(0.995, 1.0, sat(1.0 - abs(p0.y - JHN + 0.01) * 18.0)) * step(length(p0.xz), JRN + 0.03);
    frag = vec4(col, 1.0);
}
)";

// ---- glass laid over the world (the lab's glassware, the windows' panes, the jar's shards): its near side
// (dual-source: a rim and the room's reflection added, the rest tinted)
inline const char* TUBE_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
layout(location = 0, index = 0) out vec4 fragAdd;
layout(location = 0, index = 1) out vec4 fragMul;
uniform int uTwoSided;                   // (flat shards: either face; closed vessels: their near side)
void main() {
    vec3 rd = normalize(vW - uEye);
    vec3 n = normalize(vN);
    if (dot(n, rd) > 0.0) {
        if (uTwoSided == 0) discard;
        n = -n;
    }
    float cosv = abs(dot(n, rd));
    float F = 0.05 + 0.95 * pow(1.0 - cosv, 4.0);
    vec3 lightC = inLab(vW) ? uFill * 1.1 + uAmbS * 0.6 + uSunCol * 0.2 : mix(uSkyH, vec3(1.0), 0.5) * (1.0 - 0.6 * uNight);
    vec3 rim = (lightC + uFlyC * uFlyAmt * 0.25) * smoothstep(0.4, 0.0, cosv) * 0.6;
    vec3 acc = env(reflect(rd, n)) * F * 1.4 + rim;
    // a glint of the key light
    acc += uSunCol * 0.8 * smoothstep(0.96, 0.99, dot(reflect(rd, n), uSunDir));
    vec3 thru = vec3(0.9, 0.97, 0.96) * (1.0 - F);
    fragAdd = vec4(acc, 1.0);
    fragMul = vec4(thru, 1.0);
}
)";

// ---- the lights in the air (the fireflies, the seeds, the spores and pollen, the neon's halo, the street lamps):
// screen-facing glows at their depth, each its size and color
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aI;     // x, y, z, glow (world)
layout(location = 2) in vec4 aJ;     // size, color
uniform mat4 uVP;
uniform float uTanH, uAspect, uR;
out vec2 vOff;
flat out vec4 vI;
flat out vec4 vJ;
void main() {
    vec4 c = uVP * vec4(aI.xyz, 1.0);
    vOff = aPos * uR;
    vI = aI;
    vJ = aJ;
    vec2 k = vec2(1.0 / (uTanH * uAspect), 1.0 / uTanH);
    gl_Position = c + vec4(aPos * uR * aJ.x * k, 0.0, 0.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vOff;
flat in vec4 vI;
flat in vec4 vJ;
out vec4 frag;
uniform vec3 uEye;
uniform vec3 uGA, uGB;               // the falloff: three terms, their numerators and their widths
uniform float uPx, uR, uPxK;
void main() {
    float d2 = dot(vOff, vOff);
    float tc = length(vI.xyz - uEye);
    float px2 = sq(uPx * tc * uPxK / vJ.x);
    float g = uGA.x / (d2 + uGB.x + px2) + uGA.y / (d2 + uGB.y) + uGA.z / (d2 + uGB.z);
    g *= smoothstep(uR * uR, uR * uR * 0.3, d2);
    frag = vec4(vJ.yzw * vI.w * g, 0.0);
}
)";

// ---- the world, graded (slow motion's hush, the bridge's color), to the scene
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform float uSlow, uWorldA;
uniform vec3 uBridgeCol;
void main() {
    vec3 col = texture(uWorld, vUv).rgb;
    col *= mix(1.0, 0.85, uSlow);
    col = mix(col, mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * vec3(0.8, 0.9, 1.2), 0.4), uSlow);
    col = mix(uBridgeCol, col, uWorldA);
    frag = vec4(max(col, 0.0), 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ dust
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (radius, board units), mode
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // param, param2, -, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vLocal = aPos;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    // a puff of soil
    float v = exp(-r * r * 6.0) * smoothstep(1.0, 0.7, r);
    frag = vec4(c * v * vB.a, v * vB.a);
}
)";

// ------------------------------------------------------------------------------------------------ the veil
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil, uRail;
uniform float uWellA, uColA, uRailA, uPx;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h) { vec2 q = abs(p - c) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.1, 0.7, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.6 + exp(-abs(dw) * 4.0) * 0.08;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ pieces: glass
// Each cell a shard of the jar's fogged glass: a clean rounded square of its piece's tint, frosted (a fine grain, soft
// clouds), deeper in tint at its thick edges, a cool sheen across it and a bright bevel on its top left, a crisp
// darker rim; a crack or two across it, and moss growing in the cracks (more of it as the song goes on), now and then
// a cushion of it in a corner; a soft contact shadow under it.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, seed
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 block, 1 ghost), erase 0..1, dull, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
const float EXT = 1.2; // (room for the contact shadow)
void main() {
    vec2 wp = aA.xy + aPos * aA.z * EXT;
    vLocal = aPos * 0.5 * EXT;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uBridge, uMossAmt;
uniform vec3 uBridgeInk, uLightC, uMossA, uMossB;
float sdBoxR(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
float segD(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// a point on the square's edge (t: 0..4 around it)
vec2 onEdge(float t) {
    t = mod(t, 4.0);
    float f = fract(t) * 0.8 - 0.4;
    if (t < 1.0) return vec2(f, -0.46);
    if (t < 2.0) return vec2(0.46, f);
    if (t < 3.0) return vec2(-f, 0.46);
    return vec2(-0.46, -f);
}
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float shrink = 1.0 - 0.7 * smoothstep(0.0, 1.0, erase);
    vec2 q = vLocal / shrink;
    float d = sdBoxR(q, vec2(0.45), 0.1);
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.022, 0.022 + aa * 1.5, abs(d + 0.035));
        float a = (ring * 0.85 + inside * 0.12) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // the contact shadow, down and right, outside the shard only
    float ds = sdBoxR(q - vec2(0.045, -0.06), vec2(0.45), 0.1);
    float shadow = (1.0 - smoothstep(-0.02, 0.08, ds)) * (1.0 - inside) * 0.55 * (1.0 - smoothstep(0.0, 0.5, erase));
    vec2 sd = vA.xy * vec2(0.173, 0.291) + vA.w;
    float h0 = hash(floor(sd * 13.0) + 0.5), h1 = hash(floor(sd * 7.0) + 3.1), h2 = hash(floor(sd * 5.0) + 7.7);
    // frosted glass: milky in its middle (its tint softened), clear and deep in tint at its thick edges, lighter where
    // the light comes through (top left), soft clouds, a fine grain
    float e = sat(-d / 0.16);
    float lum = dot(t, vec3(0.3, 0.5, 0.2));
    vec3 milk = mix(t, vec3(lum) * 1.15 + 0.08, 0.32);
    float cloud = fbm(q * 2.4 + sd * 11.0);
    vec3 c = milk * (0.88 + 0.22 * smoothstep(0.25, 0.8, cloud));
    c *= 0.86 + 0.24 * sat(dot(q, vec2(-0.55, 0.55)) + 0.5);
    c *= 0.95 + 0.07 * hash(floor((q + sd) * 80.0));
    // the thick edge: clear, its tint deep, a bright line inside it where its facet catches the light
    vec3 deep = t * t * 0.95 + t * 0.06;
    c = mix(deep, c, smoothstep(0.0, 0.75, e));
    float facet = smoothstep(0.09, 0.0, abs(-d - 0.075)) * sat(dot(normalize(q + 1e-5), vec2(-0.6, 0.8)) + 0.2);
    c += mix(uLightC, t, 0.25) * facet * 0.35;
    // a soft sheen across it
    float sheen = smoothstep(0.1, 0.0, abs(dot(q, vec2(0.7071, 0.7071)) - 0.16 + 0.2 * h0)) * 0.12;
    c += mix(uLightC, t, 0.4) * sheen * e;
    // the cracks: one across it, a second branching off on some
    vec2 a0 = onEdge(h0 * 4.0), b0 = onEdge(h0 * 4.0 + 1.6 + 0.9 * h1);
    vec2 k0 = mix(a0, b0, 0.4 + 0.2 * h2) + vec2(h1 - 0.5, h2 - 0.5) * 0.22;
    float dc = min(segD(q, a0, k0), segD(q, k0, b0));
    float along = length(q - a0);
    if (h1 > 0.6) {
        vec2 c1 = onEdge(h0 * 4.0 + 2.9 + 0.5 * h2);
        dc = min(dc, segD(q, k0, mix(k0, c1, 0.6)));
    }
    float crackW = 0.007 + aa * 0.6;
    float crk = smoothstep(crackW, crackW * 0.3, dc) * inside;
    // moss in the cracks (fuzzy, in tufts along them, wider as the song goes on), a cushion in a corner on some
    float fuzz = vnoise(q * 30.0 + sd * 5.0);
    float tufts = smoothstep(0.35, 0.65, vnoise(vec2(along * 7.0, h0 * 9.0)));
    float wM = (0.01 + 0.055 * uMossAmt * tufts) * (0.65 + 0.7 * fuzz);
    float mossC = smoothstep(wM, wM * 0.45, dc) * smoothstep(0.02, 0.18, uMossAmt) * (0.35 + 0.65 * tufts);
    vec2 corner = vec2(h2 > 0.5 ? 0.45 : -0.45, -0.45);
    float cr = length(q - corner) - (0.05 + 0.2 * uMossAmt * step(0.55, h1)) * (0.7 + 0.6 * fuzz);
    float mossK = smoothstep(0.012, -0.012, cr) * step(0.55, h1) * smoothstep(0.1, 0.35, uMossAmt);
    float mossM = max(mossC, mossK) * inside;
    // the crack itself: a bright line where its faces catch the light, a dark hairline beside it
    c = mix(c, mix(t, vec3(1.0), 0.8), crk * 0.8);
    float side = segD(q - vec2(0.012, -0.012), a0, k0);
    c *= 1.0 - 0.35 * smoothstep(crackW, 0.0, min(side, segD(q - vec2(0.012, -0.012), k0, b0))) * inside * (1.0 - crk);
    vec3 mc = mix(uMossB * 0.7, uMossA * 0.85, smoothstep(0.3, 0.8, vnoise(q * 55.0 + sd)) * 0.7 + 0.3 * fuzz);
    mc *= 0.8 + 0.35 * step(0.72, hash(floor(q * 95.0 + sd)));
    c = mix(c, mc, mossM);
    // the rim: a thin bright bevel on its top left, a crisp darker line all round
    float edgeIn = 1.0 - smoothstep(0.0, 0.04, -d);
    c = mix(c, mix(t, vec3(1.0), 0.75), edgeIn * sat(dot(normalize(q + 1e-5), vec2(-0.6, 0.8))) * 0.65);
    c = mix(c, t * 0.32, 1.0 - smoothstep(0.0, 0.026 + aa, -d));
    c = mix(c, mix(t, vec3(1.0), 0.5), smoothstep(0.0, 0.6, erase) * 0.5);
    float dull = vC.z;
    c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))) * 0.55, dull);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * (1.0 - smoothstep(0.55, 1.0, erase));
    float sa = shadow * vB.a * (1.0 - uBridge);
    frag = vec4(c * a, a + sa * (1.0 - a));
}
)";

// ------------------------------------------------------------------------------------------------ lettering
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

// ------------------------------------------------------------------------------------------------ post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.3 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.9);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace terrarium_shaders

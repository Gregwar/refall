#pragma once
// TERRARIUM's shaders: the world as meshes at the full resolution (the room: the window wall, the sky and the garden
// through it, the sill, the shelves; each jar's contents: soil, moss, ferns, mushrooms, a pond, a sapling, flowers,
// succulents, sprouts, the beetle, the snail, the cork; the jars' glass, back and front, the front refracting the
// world behind it; the glass tubes; the lights in the air), the veil behind the well, the pebble pieces, lettering.

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
// The scene's uniforms and functions, shared by the room, the jars' contents, their glass and the tubes: the light
// outside, the sky through the window, the room's reflection, the fireflies' light, the soil, the condensation, the
// painted vines, the sun's shadow map.
inline const char* SCENE_GLSL = R"(
uniform vec3 uEye;
uniform float uTime, uPx, uSway, uSpots;
uniform vec3 uSunDir, uSunCol, uSkyZ, uSkyH, uAmbS, uAmbG, uFill, uGlowSun;
uniform vec2 uSunSky;
uniform float uSunDisc, uMoon, uStars, uRain, uFog, uCond, uNight, uFlyAmt;
uniform vec3 uBugC, uBugSpot, uFlyC, uSill, uFrame, uWall, uHill, uHill2;
uniform int uFlies;
uniform vec4 uFly[32];                   // the jars' fireflies: x, y, z, glow (world)
uniform int uJarN;
uniform sampler2D uJarTex;               // the jars' rows (19: foot and size; see terrarium.cpp)
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
const float ZW = -2.3;                   // the window wall
const float SF = 1.3;                    // the sill's front edge
const float WX = 2.4, WY = 2.7;          // the window opening (half width, height above the sill)
const float WCX = 0.0;                   // its center
const float PI = 3.14159265, TAU = 6.2831853;

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
vec4 jarTexel(int j, int i) { return texelFetch(uJarTex, ivec2(i, j), 0); }

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

// the sun's shadow (soft: a dozen taps; offset toward the sun, thin leaves lit from either side)
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

// ---------------------------------------------------------------- the outside: sky, hills, the garden
vec3 sky(vec3 rd) {
    // the window shows only a few degrees above the horizon: its elevations are stretched (E = 8 x the angle)
    float az = atan(rd.x, -rd.z);
    float E = rd.y / max(length(rd.xz), 1e-3) * 8.0;
    vec3 c = mix(uSkyH, uSkyZ, smoothstep(0.05, 0.75, E));
    c = mix(c, uSkyZ * 0.9, smoothstep(0.6, 1.2, E) * 0.5);
    // the sun's glow and disc (soft: out of focus through the window)
    vec2 sd = vec2(az - uSunSky.x, (E - uSunSky.y) * 0.125);
    float sr = length(sd);
    c += uGlowSun * (exp(-sr * 9.0) * 0.6 + exp(-sr * 40.0) * 0.6);
    c += uSunCol * uSunDisc * smoothstep(0.024, 0.019, sr) * 0.9;
    // the moon
    vec2 md = vec2(az - 0.2, (E - 0.36) * 0.125);
    float mr = length(md);
    float moon = smoothstep(0.022, 0.018, mr) * (1.0 - 0.6 * smoothstep(0.018, 0.0, length(md - vec2(0.008, 0.002))));
    c += vec3(0.9, 0.93, 1.0) * uMoon * (moon * 1.4 + exp(-mr * 30.0) * 0.18);
    // stars
    if (uStars > 0.01) {
        vec2 sc = vec2(az * 110.0, E * 14.0);
        vec2 ci = floor(sc);
        vec2 h = hash2(ci);
        float star = smoothstep(0.2, 0.0, length(fract(sc) - h)) * step(0.82, hash(ci + 3.1));
        star *= 0.6 + 0.4 * sin(uTime * (0.8 + 2.0 * h.x) + h.y * 30.0);
        c += vec3(0.8, 0.85, 1.0) * star * uStars * smoothstep(0.25, 0.5, E) * 1.4;
    }
    // toon clouds drifting slowly: a lit top, a shaded belly
    {
        vec2 cp = vec2(az * 3.0 + uTime * 0.004, E * 1.6);
        float band = smoothstep(0.3, 0.5, E);
        float n = fbm(cp * vec2(1.4, 2.4) + vec2(0.0, 3.0)) + 0.08 * band - 0.04;
        float aa = max(fwidth(n), 0.004);
        float cl = smoothstep(0.59 - aa, 0.59 + aa, n) * band;
        float lit = smoothstep(0.58, 0.62, fbm(cp * vec2(1.4, 2.4) + vec2(0.0, 3.0) + vec2(0.0, -0.1)) + 0.08 * band - 0.04);
        vec3 lightC = mix(uGlowSun, vec3(1.0), 0.55) * (1.0 - 0.7 * uNight);
        vec3 cc = mix(mix(uSkyH, uSkyZ, 0.35) * 1.05, lightC * 1.2, 0.35 + 0.65 * lit);
        cc = mix(cc, uSkyZ * 1.4, uNight * 0.6);
        c = mix(c, cc, cl * (1.0 - 0.5 * uRain));
    }
    // a summer shower: a slate sky
    c = mix(c, mix(vec3(0.2, 0.27, 0.36), uSkyZ, 0.25) * (1.0 - 0.5 * uNight), uRain * 0.85);
    // the hills, two layers, a line of round trees on the near one
    float h1 = 0.24 + 0.08 * sin(az * 3.0 + 1.3) + 0.05 * sin(az * 7.0 + 0.4);
    float h2 = 0.1 + 0.07 * sin(az * 2.2 - 0.7) + 0.035 * sin(az * 5.3 + 2.0);
    float cx = az * 60.0;
    float cell = floor(cx);
    float rr = 0.5 + 0.4 * hash1(cell);
    float tree = sqrt(max(0.0, sq(rr * 0.5) - sq(fract(cx) - 0.5))) * 0.16 * (0.6 + 0.8 * hash1(cell + 3.0)) * step(0.35, hash1(cell + 9.0));
    vec3 hl = clamp((uSunCol * 0.35 + uAmbS * 1.1 + uFill * 0.3) / 1.55, 0.0, 1.3);
    vec3 far = mix(uHill2 * hl, uSkyH, 0.4);
    far = mix(far, uSkyH * 0.45 + uSkyZ * 0.35, uNight * 0.7);
    float ea = max(fwidth(E), 0.002);
    c = mix(c, far, smoothstep(ea, -ea, E - h1));
    vec3 near = mix(uHill, uHill * 0.6 + uSkyZ * 0.12, smoothstep(0.0, -0.3, E - h2));
    near *= hl * (1.0 - 0.35 * uRain);
    near = mix(near, uSkyZ * 0.5 + uSkyH * 0.1, uNight * 0.75);
    float nm = smoothstep(ea, -ea, E - h2 - tree);
    c = mix(c, near, nm);
    // the garden's fireflies, far away at night
    if (uNight > 0.05 && nm > 0.5) {
        vec2 gp = vec2(az * 140.0, E * 140.0 / 8.0 + uTime * 0.03);
        vec2 gi = floor(gp);
        vec2 gh = hash2(gi + 7.0);
        float f = smoothstep(0.2, 0.0, length(fract(gp) - 0.2 - gh * 0.6)) * step(0.965, hash(gi + 1.7));
        f *= smoothstep(-0.2, 0.8, sin(uTime * (0.5 + gh.x) + gh.y * 20.0));
        c += uFlyC * f * uNight * 1.1 * smoothstep(-0.02, -0.12, E - h2);
    }
    return c;
}

// the room's reflection in the glass: a dim room, the window behind us a soft bright strip
vec3 env(vec3 r) {
    vec3 c = mix(uWall * (uFill + uAmbS * 0.4) * 0.8, (uSkyH * 0.6 + uSkyZ * 0.4) * 0.9, sat(r.y * 0.8 + 0.4));
    float az = atan(r.x, r.z);
    float strip = smoothstep(0.5, 0.42, abs(az - 0.55)) * smoothstep(-0.25, 0.1, r.y) * smoothstep(0.95, 0.6, r.y);
    float strip2 = smoothstep(0.09, 0.05, abs(az + 0.85)) * smoothstep(-0.1, 0.2, r.y) * smoothstep(0.9, 0.5, r.y);
    vec3 light = mix(uSkyH, vec3(1.0), 0.5) * (1.0 - 0.75 * uNight) + uFlyC * 0.1 * uNight;
    c += light * (strip * 1.3 + strip2 * 0.8);
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
)"
R"(        float lane = -2.6 + fk * 1.25 + 0.3 * hash1(fk * 3.0);
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

// the sill: painted wood, the first jar's shadow and caustic, the others' shadows and glow
vec3 sillColor(vec3 p) {
    float grain = fbm(vec2(p.x * 2.5, p.z * 28.0)) * 0.6 + vnoise(vec2(p.x * 40.0, p.z * 3.0)) * 0.4;
    vec3 alb = uSill * (0.88 + 0.2 * grain);
    vec3 L = uSunDir;
    float tmax = 1.0 / max(L.y, 0.05);
    vec2 a = p.xz, b = p.xz + L.xz * tmax;
    vec2 ba = b - a;
    float hseg = sat(dot(-a, ba) / max(dot(ba, ba), 1e-6));
    float dmin = length(a + ba * hseg);
    float ylev = L.y * tmax * hseg;
    float jarSh = smoothstep(1.02, 0.96, dmin);
    float soilSh = jarSh * smoothstep(0.45, 0.25, ylev);
    float caus = exp(-sq((dmin - 0.86) / 0.08)) * smoothstep(0.25, 0.6, ylev) * jarSh;
    float sh = 1.0 - 0.75 * soilSh - 0.25 * jarSh * (1.0 - soilSh);
    vec3 lightC = uSunCol * sat(L.y) * sh * 0.55 + uAmbS * 0.65 + uFill * 0.6;
    vec3 col = alb * lightC + uSunCol * alb * caus * 0.35 * (1.0 - soilSh);
    col += uFlyC * alb * uFlyAmt * 0.12 * exp(-sq(max(length(p.xz) - 0.9, 0.0) / 0.5));
    for (int j = 1; j < uJarN; j++) {
        vec4 f = jarTexel(j, 19);
        if (f.w <= 0.0 || abs(f.y) > 0.05) continue;
        vec2 dq = p.xz - f.xz - uSunDir.xz / max(uSunDir.y, 0.2) * 0.25 * f.w;
        col *= 1.0 - 0.3 * smoothstep(1.05 * f.w, 0.8 * f.w, length(dq));
        col += uFlyC * alb * uFlyAmt * 0.1 * exp(-sq(max(length(p.xz - f.xz) - 0.9 * f.w, 0.0) / (0.5 * f.w)));
    }
    return col;
}
vec3 shelfColor(vec3 p, vec3 n) {
    float grain = vnoise(vec2(p.x * 2.0, p.z * 30.0 + p.y * 30.0)) * 0.5 + vnoise(vec2(p.x * 30.0, p.y * 4.0)) * 0.5;
    vec3 alb = uSill * (0.85 + 0.25 * grain) * (n.y > 0.5 ? 1.0 : 0.72);
    return alb * (uAmbS * 0.7 + uFill * 0.8 + uSunCol * 0.18 * max(n.y, 0.0)) + alb * uFlyC * uFlyAmt * 0.15;
}
// the room: the sill (m 1), its front (2), the window wall with the panes, the sky through them and the frame (3),
// the shelves (4); aaW: the antialiasing width on the wall
vec3 roomColor(vec3 p, vec3 rd, float m, vec3 n, float aaW) {
    vec3 col;
    if (m < 1.5) col = sillColor(p);
    else if (m < 2.5) {
        vec3 alb = uSill * mix(0.62, 0.95, smoothstep(-0.1, -0.02, p.y));
        col = alb * (uAmbS * 0.7 + uFill * 0.75) + alb * uFlyC * uFlyAmt * 0.08;
    } else if (m > 3.5) col = shelfColor(p, n);
    else {
        vec3 W = p;
        vec2 Wq = vec2(W.x - WCX, W.y);
        float dOpen = sdBox2(Wq - vec2(0.0, WY * 0.5), vec2(WX, WY * 0.5));
        if (dOpen < 0.0) {
            col = sky(rd);
            if (uRain > 0.01) {
                float u = (W.x + W.y * 0.12) * 60.0;
                float ci = floor(u);
                float fx = fract(u) - 0.5;
                float v = W.y * 2.2 + uTime * (5.0 + 2.0 * hash1(ci)) + hash1(ci + 7.0) * 20.0;
                float fv = fract(v);
                float streak = smoothstep(0.12, 0.0, abs(fx)) * smoothstep(0.0, 0.08, fv) * smoothstep(0.5, 0.1, fv) * step(0.45, hash1(ci + 3.0));
                col = mix(col, vec3(0.75, 0.82, 0.9) * (1.0 - 0.6 * uNight), streak * uRain * 0.22);
                vec2 dp = W.xy * vec2(9.0, 6.0);
                vec2 di = floor(dp);
                vec2 dh = hash2(di);
                vec2 off = vec2(0.25 + 0.5 * dh.x, fract(dh.y - uTime * (0.03 + 0.06 * dh.x)));
                vec2 dq = (fract(dp) - off) / (vec2(0.08, 0.11) * (0.7 + 0.6 * dh.y));
                float r = length(dq);
                float has = step(0.62, hash(di + 4.0));
                float drop = smoothstep(1.0, 0.85, r) * has;
                col = mix(col, mix(uSkyZ, uSkyH, 0.5) * 0.85, drop * uRain * 0.4);
                col *= 1.0 - smoothstep(0.7, 1.0, r) * smoothstep(1.05, 0.95, r) * has * uRain * 0.35;
            }
        } else {
            col = uWall * (0.92 + 0.1 * vnoise(W.xy * 3.0)) * (uAmbS * 0.75 + uFill * 0.8 + uSunCol * 0.04);
        }
        float blur = max(0.012, aaW);
        float fx = abs(abs(Wq.x) - 1.62) - 0.05;
        float fr = min(min(fx, W.y - 0.09), abs(W.y - 1.5) - 0.045);
        fr = dOpen < 0.0 ? fr : abs(dOpen - 0.07) - 0.08;
        float fm = smoothstep(blur, -blur, fr);
        vec3 frameC = uFrame * (uFill * 0.9 + uAmbS * 0.8 + uSunCol * 0.08);
        frameC *= 1.0 - smoothstep(-0.05, 0.0, fr) * 0.2;
        col = mix(col, frameC, fm);
    }
    return col;
}
// the room along a ray, analytically (what the old ray saw past a jar's thick glass edge): the sill, its front, the
// window wall, the shelves
vec3 background(vec3 ro, vec3 rd) {
    float tbest = 1e4, m = 3.0;
    vec3 n = vec3(0.0, 1.0, 0.0);
    if (rd.y < 0.0) {
        float ts = -ro.y / rd.y;
        vec3 p = ro + rd * ts;
        if (p.z <= SF && p.z >= ZW) { tbest = ts; m = 1.0; }
        else if (p.z > SF && rd.z < 0.0) {
            float tf = (SF - ro.z) / rd.z;
            if ((ro + rd * tf).y < 0.0) { tbest = tf; m = 2.0; }
        }
    }
    if (m > 2.5) tbest = (ZW - ro.z) / rd.z;
    vec3 inv = 1.0 / rd;
    for (int i = 0; i < 3; i++) {
        vec3 sa = i == 0 ? vec3(2.95, -6.6, 6.6) : (i == 1 ? vec3(1.35, -7.0, -2.75) : vec3(1.35, 2.75, 7.0));
        vec3 bmin = vec3(sa.y, sa.x - 0.09, ZW), bmax = vec3(sa.z, sa.x, ZW + 0.72);
        vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
        vec3 tn = min(t0, t1), tf = max(t0, t1);
        float tin = max(max(tn.x, tn.y), tn.z), tout = min(min(tf.x, tf.y), tf.z);
        if (tout > max(tin, 0.0) && tin > 0.001 && tin < tbest) {
            tbest = tin; m = 4.0;
            n = tin == tn.y ? vec3(0.0, -sign(rd.y), 0.0) : (tin == tn.z ? vec3(0.0, 0.0, -sign(rd.z)) : vec3(-sign(rd.x), 0.0, 0.0));
        }
    }
    return roomColor(ro + rd * tbest, rd, m, n, 0.012);
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

// ---- the meshes: jar units, placed by the jar's foot and size; ferns and broad leaves sway about their slot
inline const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aM;     // material, ao, seed, two-sided
layout(location = 3) in vec4 aF;     // the material's parameters
layout(location = 4) in vec4 aS;     // sway: pivot x, z, phase, amplitude
uniform mat4 uVP;
uniform vec3 uFoot;
uniform float uScale, uTime, uSway;
out vec3 vL;
out vec3 vW;
out vec3 vN;
out vec4 vM;
out vec4 vF;
void main() {
    vec3 p = aPos, n = aNrm;
    if (aS.w > 0.0) {
        float a = aS.w * uSway * sin(uTime * 0.7 + aS.z);
        vec2 c = aS.xy;
        p.xz = c + rot(a) * (p.xz - c);
        n.xz = rot(a) * n.xz;
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

// ---- the room: the window wall (the panes, the sky and the garden through them, the frame), the sill, the shelves
inline const char* ROOM_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
out vec4 frag;
void main() {
    vec3 rd = normalize(vW - uEye);
    frag = vec4(roomColor(vW, rd, vM.x, normalize(vN), length(fwidth(vW.xy)) * 0.7), 1.0);
}
)";

// ---- the jars' glass. The back (uFront 0: the cavity's far side, seen through the jar): the room's faint
// reflection, mist and drops, the vines painted up its inside, laid over what is behind. The front (uFront 1): the
// outer surface, refracting what is behind it (the world drawn so far, read back with its depth), the drops acting
// as lenses, the low mist inside the jar integrated over the ray's way through it, the vines, the tint thicker toward
// the edges, the condensation, the fresnel reflection of the room, a rim.
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
float viewZ(float d) { float z = d * 2.0 - 1.0; return 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x)); }
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
        vec3 mist = mix(uSkyH, vec3(1.0), 0.4) * (0.75 - 0.5 * uNight) + uAmbS * 0.3;
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
    float cosv = sat(dot(N, -rd));
    float cov = uCond * (0.15 + 0.85 * smoothstep(0.45, 0.8, p0.y)) * smoothstep(0.36, 0.5, p0.y);
    vec4 dr = drops(uv, cov);
    vec3 tng = normalize(cross(vec3(0.0, 1.0, 0.0), N));
    vec3 rdIn = normalize(refract(rd, N, 0.94) + (tng * dr.x + vec3(0.0, 1.0, 0.0) * dr.y) * 0.06 * dr.z);
    vec3 pIn = p0 + rdIn * (JT * 1.6);
    // the bent ray through the jar, marched against the world's depth: where it meets something, or where it leaves
    // the jar's body (then the world behind, as it shows there); a ray still in the glass's thickness (its edges) sees
    // the room past it, as the old ray did
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
            // it went behind what the world shows: find where, and keep it if the surface is really there (a ray
            // passing behind a cap or a leaf goes on)
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
            // (what it would meet is hidden behind something in the world as drawn: the straight view there, and
            // the mist to it)
            suv = gl_FragCoord.xy / vec2(textureSize(uBack, 0));
            ti = tBehind;
        }
        inner = texture(uBack, suv).rgb;
    }
    {
        // the low mist over the soil (exponential in height), over the way through the jar
        float y0 = pIn.y, dy = rdIn.y;
        float H = 0.16;
        float a = exp(-(y0 - 0.22) / H);
        float integ = abs(dy) > 1e-4 ? a * H * (1.0 - exp(-dy * ti / H)) / dy : a * ti;
        float wisp = 0.6 + 0.8 * vnoise(vec2(pIn.x * 3.0 + uTime * 0.03, pIn.y * 2.0 + uTime * 0.01));
        float fogA = 1.0 - exp(-max(integ, 0.0) * uFog * 1.2 * wisp);
        vec3 fogC = mix(uSkyH, vec3(1.0), 0.45) * (0.85 - 0.65 * uNight) + uAmbS * 0.3 + uFlyC * uFlyAmt * 0.025;
        inner = mix(inner, fogC, fogA * 0.6);
    }
    {
        vec4 vi = vines(uv, 0.24, 0.19, 1.0, uInVine * 0.62, 0.04, 1.0, 0.15 * uFlyAmt, aaUv);
        if (vi.a > 0.0) inner = mix(inner, vi.rgb * paintLight(p0, 0.5), vi.a);
    }
    vec3 tint = vec3(0.9, 0.97, 0.95);
    inner *= mix(vec3(1.0), tint * tint * 0.85, pow(1.0 - cosv, 2.5));
    vec3 mist = mix(uSkyH, vec3(1.0), 0.5) * (0.85 - 0.6 * uNight) + uAmbS * 0.35 + uFlyC * uFlyAmt * 0.05;
    inner = mix(inner, mist, dr.w * 0.3);
    if (dr.z > 0.5) {
        float r2 = dot(dr.xy, dr.xy);
        vec3 lightC = mix(uSkyH, vec3(1.0), 0.6) * (1.0 - 0.6 * uNight) + uFlyC * uFlyAmt * 0.35;
        inner = mix(inner, mist, 0.25 * cov);
        inner *= 1.0 - 0.22 * smoothstep(0.7, 1.0, r2);
        float hi = smoothstep(0.16, 0.04, length(dr.xy - vec2(-0.3, 0.38)));
        float lo = smoothstep(0.45, 0.1, length(dr.xy - vec2(0.15, -0.5))) * smoothstep(0.3, 0.9, r2);
        inner += lightC * (hi * 0.8 + lo * 0.22);
    }
    float F = 0.035 + 0.965 * pow(1.0 - cosv, 5.0);
    vec3 col = mix(inner, env(reflect(rd, N)), F);
    col += mix(uSkyH, vec3(1.0), 0.4) * (1.0 - 0.6 * uNight) * smoothstep(0.16, 0.0, cosv) * 0.35;
    col += uGlowSun * 0.25 * smoothstep(0.995, 1.0, sat(1.0 - abs(p0.y - JHN + 0.01) * 18.0)) * step(length(p0.xz), JRN + 0.03);
    frag = vec4(col, 1.0);
}
)";

// ---- the glass tubes: their near side laid over the world (dual-source: a rim and the room's reflection added,
// the rest tinted), water trickling along some
inline const char* TUBE_FS = R"(#version 330 core
in vec3 vL;
in vec3 vW;
in vec3 vN;
in vec4 vM;
in vec4 vF;
layout(location = 0, index = 0) out vec4 fragAdd;
layout(location = 0, index = 1) out vec4 fragMul;
uniform float uGrow, uTubeWater, uTubeR;
void main() {
    if (vF.x > uGrow) discard;
    vec3 rd = normalize(vW - uEye);
    vec3 n = normalize(vN);
    if (dot(n, rd) > 0.0) discard;
    float cosv = abs(dot(n, rd));
    float F = 0.05 + 0.95 * pow(1.0 - cosv, 4.0);
    vec3 rim = (mix(uSkyH, vec3(1.0), 0.5) * (1.0 - 0.6 * uNight) + uFlyC * uFlyAmt * 0.25) * smoothstep(0.4, 0.0, cosv) * 0.6;
    vec3 acc = env(reflect(rd, n)) * F * 1.4 + rim;
    vec3 thru = vec3(0.9, 0.97, 0.96) * (1.0 - F);
    if (uTubeWater > 0.5) {
        // water trickling along the tube's floor
        vec3 T = normalize(vF.yzw);
        vec3 v = rd - T * dot(rd, T);
        float st = 0.6 + 0.4 * smoothstep(0.3, 0.8, vnoise(vec2(dot(vW, T) * 30.0 - uTime * 2.0, 0.0)));
        float low = smoothstep(0.0, -0.5, n.y) + 0.4 * smoothstep(0.2, -0.2, v.y);
        acc += thru * uWater * st * (uAmbS + uFill * 0.6 + uSunCol * 0.2) * 0.45 * sat(low);
        thru *= 1.0 - 0.45 * sat(low);
    }
    fragAdd = vec4(acc, 1.0);
    fragMul = vec4(thru, 1.0);
}
)";

// ---- the lights in the air (the fireflies, those along the tubes, the seeds): screen-facing glows at their depth
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aI;     // x, y, z, glow (world)
uniform mat4 uVP;
uniform float uTanH, uAspect, uR;
out vec2 vOff;
flat out vec4 vI;
void main() {
    vec4 c = uVP * vec4(aI.xyz, 1.0);
    vOff = aPos * uR;
    vI = aI;
    vec2 k = vec2(1.0 / (uTanH * uAspect), 1.0 / uTanH);
    gl_Position = c + vec4(aPos * uR * k, 0.0, 0.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vOff;
flat in vec4 vI;
out vec4 frag;
uniform vec3 uEye, uCol;
uniform vec3 uGA, uGB;               // the falloff: three terms, their numerators and their widths
uniform float uPx, uR, uPxK;
void main() {
    float d2 = dot(vOff, vOff);
    float tc = length(vI.xyz - uEye);
    float px2 = sq(uPx * tc * uPxK);
    float g = uGA.x / (d2 + uGB.x + px2) + uGA.y / (d2 + uGB.y) + uGA.z / (d2 + uGB.z);
    g *= smoothstep(uR * uR, uR * uR * 0.3, d2);
    frag = vec4(uCol * vI.w * g, 0.0);
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

// ------------------------------------------------------------------------------------------------ pieces: stones
// Each cell one polished stone from the jar's gravel: a clean rounded square of its piece's color, a natural stone
// surface in restrained relief (soft mottling, a few fine pale veins, speckles), a satin dome lit from the window
// (top left), a crisp darker rim of its own color with a thin light bevel, and a soft contact shadow under it.
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
uniform float uBridge;
uniform vec3 uBridgeInk, uLightC;
float sdBoxR(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
// the stone's own pattern, fixed to the stone (its seed from its place)
float stoneH(vec2 q, vec2 sd) { return fbm(q * 3.2 + sd * 17.0); }
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float shrink = 1.0 - 0.7 * smoothstep(0.0, 1.0, erase);
    vec2 q = vLocal / shrink;
    float d = sdBoxR(q, vec2(0.45), 0.17);
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.022, 0.022 + aa * 1.5, abs(d + 0.035));
        float a = (ring * 0.85 + inside * 0.1) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.3), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // the contact shadow, down and right, outside the stone only
    float ds = sdBoxR(q - vec2(0.045, -0.06), vec2(0.45), 0.17);
    float shadow = (1.0 - smoothstep(-0.02, 0.08, ds)) * (1.0 - inside) * 0.55 * (1.0 - smoothstep(0.0, 0.5, erase));
    vec2 sd = vA.xy * vec2(0.173, 0.291) + vA.w;
    // relief: the stone's mottling as a gentle height, lit like the dome
    float h = stoneH(q, sd);
    float hx = stoneH(q + vec2(0.02, 0.0), sd) - h, hy = stoneH(q + vec2(0.0, 0.02), sd) - h;
    float e = sat(-d / 0.22);
    vec2 dome = normalize(q + 1e-5) * (1.0 - e) * 0.9;
    vec3 n = normalize(vec3(dome - vec2(hx, hy) * 1.4, 1.0));
    vec3 L = normalize(vec3(-0.55, 0.65, 0.75));
    float lit = dot(n, L);
    // the stone's colors: its tint in soft cloudy patches, faint strata at its own angle, now and then one fine pale
    // vein, a few speckles
    vec3 c = t * mix(0.84, 1.08, smoothstep(0.32, 0.72, h));
    float ang = fract(sd.x * 3.7) * 3.1416;
    float band = sin(dot(q, vec2(cos(ang), sin(ang))) * 9.0 + h * 4.0 + sd.y * 6.0);
    c *= 1.0 + 0.05 * band;
    float vn = fbm(q * 1.6 + sd * 9.0 + 4.0);
    float vein = (1.0 - smoothstep(0.0, 0.012 + aa, abs(vn - 0.5))) * step(0.55, fract(sd.x * 7.3 + sd.y * 3.1));
    c = mix(c, mix(t, vec3(1.0), 0.5), vein * 0.3);
    vec2 sp = (q + sd) * 26.0;
    float sk = step(0.88, hash(floor(sp))) * smoothstep(0.38, 0.18, length(fract(sp) - 0.5));
    c = mix(c, t * 0.62, sk * 0.35);
    // satin light: a rounder dome (lit top left, its foot darker bottom right), a broad sheen, no glint
    c *= 0.72 + 0.42 * sat(lit * 0.5 + 0.5);
    c *= 1.0 - 0.14 * smoothstep(0.1, 0.5, dot(q, vec2(0.7, -0.7)));
    float sheen = smoothstep(0.82, 0.98, dot(reflect(-L, n), vec3(0.0, 0.0, 1.0)));
    c += mix(t, uLightC, 0.6) * sheen * 0.12;
    // the bevel: a thin light edge top left, a crisp darker rim all round
    float edgeIn = 1.0 - smoothstep(0.0, 0.06, -d);
    c = mix(c, mix(t, vec3(1.0), 0.45), edgeIn * sat(dot(normalize(q + 1e-5), vec2(-0.6, 0.8))) * 0.35);
    c = mix(c, t * 0.42, 1.0 - smoothstep(0.0, 0.03 + aa, -d));
    c = mix(c, mix(t, vec3(1.0), 0.5), smoothstep(0.0, 0.6, erase) * 0.4);
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

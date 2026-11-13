#pragma once
// ORBITAL STATION's shaders (see orbital.cpp).
//
// SKY_FS ray-traces the planet below (terrain, oceans and their glint, clouds and their shadows, the terminator, the
// cities' lights, an aurora), its atmosphere (the limb's glow, blue by day, red at the terminator, a thin airglow by
// night), the sun, the moon and space, at a reduced resolution. BG_FS lays it under the station at full resolution
// with the stars (measured in screen pixels through the sky projection's Jacobian, as GIANT TREE does). PART_FS lights
// every solid of the station (instanced boxes and parametric cylinders, spheres, tori and bands: white hull panels,
// gold foil, solar cells, radiators, lattice trusses, the forest ring's land), with the sun's shadow map, the
// planet's light from below and its reflection. GLOW_FS draws the lights (lamps, thrusters, sparks); then the bloom,
// the veil behind the well, the pieces (panels of colored light), the lettering.
// World positions are meters in the station's frame (y away from the planet), drawn relative to the eye.

namespace orbital_shaders {

inline const char* COMMON_GLSL = R"(
const float PI = 3.14159265;
const float TAU = 6.2831853;
float sq(float x) { return x * x; }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
float hash3(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}
float fbm3(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise3(p); p = p * 2.07 + vec3(1.7, 9.2, 3.1); a *= 0.5; }
    return s / 0.9375;
}
)";

// ---------------------------------------------------------------- the planet, its atmosphere, space (reduced res)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform float uPix;            // radians per pixel of this target
uniform vec3 uEyeP;            // the camera relative to the planet's center (meters)
uniform float uAlt, uR;
uniform vec3 uSunDir, uSunC, uSunGlow;
uniform float uSunSize, uSunVis;
uniform vec3 uSpaceC, uMilkyC;
uniform float uMilky;
uniform mat3 uPlanetRot, uCloudRot;
uniform float uType;           // 0 an earth, 1 a desert world
uniform vec3 uOcean, uShallow, uLand, uLand2, uDesert, uIce, uCloudC, uCityC, uAtmoDay, uAtmoTwi, uAirglow;
uniform float uClouds, uCity, uAurora, uGlow, uNightLift, uHaze;
uniform vec3 uAurora1, uAurora2;
uniform vec3 uMoonDir;
uniform float uMoonSize;
uniform float uTime, uBridge, uDyson;
uniform vec3 uBridgeCol;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }

// a fractal whose octaves fade out as they shrink under a few pixels (wl: the first octave's wavelength, meters)
float fbmD(vec3 p, float foot, float wl, int oct) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 12; i++) {
        if (i >= oct) break;
        float k = smoothstep(1.2 * foot, 3.0 * foot, wl);
        if (k <= 0.0) break;
        s += a * (vnoise3(p) - 0.5) * k;
        p = p * 2.03 + vec3(1.7, 9.2, 3.1);
        a *= i < 3 ? 0.5 : 0.58;
        wl *= 0.4926;
    }
    return s;
}
float ridges(vec3 p, float foot, float wl) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        float k = smoothstep(2.0 * foot, 5.0 * foot, wl);
        if (k <= 0.0) break;
        float n = 1.0 - abs(vnoise3(p) * 2.0 - 1.0);
        s += a * n * n * k;
        p = p * 2.1 + vec3(3.1, 1.7, 5.3);
        a *= 0.55;
        wl *= 0.476;
    }
    return s;
}
// the land's height (0.5 the shore) at a surface direction, its detail by the pixel's footprint
float terrain(vec3 s, float foot) {
    return 0.5 + fbmD(s * 2.8 + 3.1, foot, 1.4e7, 12) * 1.25 - 0.01;
}
// craters: a bowl, a bright rim, one per cell where there is one (two scales)
float craters(vec3 s, float foot) {
    float v = 0.0;
    float freq = 60.0;
    for (int i = 0; i < 3; i++) {
        float wl = 4.0e7 / freq;
        float k = smoothstep(3.0 * foot, 9.0 * foot, wl);
        if (k <= 0.0) break;
        vec3 p = s * freq + float(i) * 17.0;
        vec3 c = floor(p);
        vec3 o = vec3(hash3(c), hash3(c + 3.1), hash3(c + 7.7)) * 0.5 + 0.25;
        float r = length(fract(p) - o);
        float R = 0.08 + 0.17 * hash3(c + 1.3);
        float on = step(0.45, hash3(c + 5.9));
        float bowl = smoothstep(R, R * 0.6, r);
        float rim = exp(-sq((r - R) / (R * 0.18)));
        v += (rim * 0.5 - bowl * 0.35) * on * k;
        freq *= 2.7;
    }
    return v;
}
float clouds(vec3 c, float foot, bool detail) {
    vec3 q = c * 2.4;
    vec3 w = vec3(fbm3(q + 1.7), fbm3(q + 9.2), fbm3(q + 5.3));
    float v = fbm3(c * 3.6 + w * 1.8);
    // bands: the tropics clearer, the storm belts fuller
    float lat = abs(c.y);
    v += 0.07 * sin(lat * 9.0) - 0.02;
    if (detail) v += fbmD(c * 14.0 + w * 3.0, foot, 2.8e6, 10) * 0.6;
    return v;
}
void main() {
    vec3 d = rayDir(vUv);
    vec3 E = uEyeP;
    float b = dot(E, d);
    float c0 = uAlt * (uAlt + 2.0 * uR);
    float disc = b * b - c0;
    vec3 col;
    float stars = 0.0;
    float sd = dot(d, uSunDir);
    bool hit = disc > 0.0 && b < 0.0;
    float tHit = hit ? c0 / (-b + sqrt(disc)) : 1e30;
    if (hit) {
        vec3 p = E + d * tHit;
        vec3 n = normalize(p);
        float foot = tHit * uPix;
        vec3 s = uPlanetRot * n;
        float ndl = dot(n, uSunDir);
        float day = smoothstep(-0.06, 0.16, ndl);
        float tw = exp(-sq((ndl - 0.02) / 0.11));
        float night = 1.0 - smoothstep(-0.12, 0.04, ndl);
        float h = terrain(s, foot);
        float cw = max(foot / 2.0e6, 0.004);
        float water, ice;
        vec3 alb;
        if (uType < 0.5) {
            water = 1.0 - smoothstep(0.5 - cw, 0.5 + cw, h);
            float lat = abs(s.y);
            float moist = fbm3(s * 2.6 + 11.0) + 0.12 * (1.0 - smoothstep(0.1, 0.25, abs(lat - 0.4))) - 0.1 * smoothstep(0.15, 0.3, abs(lat - 0.28)) * 0.0;
            moist += fbmD(s * 30.0, foot, 1.3e6, 8) * 0.7;
            float rg = ridges(s * 22.0 + 4.0, foot, 1.8e6) * smoothstep(0.5, 0.56, h);
            float dry = smoothstep(0.47, 0.37, moist) * smoothstep(0.08, 0.2, lat) * smoothstep(0.62, 0.45, lat);
            vec3 forest = uLand, grass = mix(uLand, uLand2, 0.45) * vec3(1.05, 1.08, 0.85), savanna = uLand2;
            vec3 land = mix(savanna, grass, smoothstep(0.42, 0.52, moist));
            land = mix(land, forest, smoothstep(0.55, 0.66, moist));
            land = mix(land, uDesert * (0.9 + 0.2 * fbmD(s * 200.0, foot, 2.e5, 6)), dry);
            land = mix(land, vec3(0.32, 0.3, 0.24), smoothstep(0.62, 0.74, lat) * (1.0 - smoothstep(0.78, 0.84, lat)));
            land = mix(land, vec3(0.36, 0.32, 0.28), smoothstep(0.3, 0.7, rg) * 0.7 * smoothstep(0.5, 0.62, fbm3(s * 3.0 + 20.0)));
            land *= 0.72 + 0.56 * (fbmD(s * 400.0 + 2.0, foot, 1.e5, 8) + 0.5);
            ice = smoothstep(0.82, 0.87, lat + 0.06 * fbm3(s * 8.0)) + smoothstep(0.66, 0.85, rg) * 0.85 * (1.0 - dry) * smoothstep(0.58, 0.68, fbm3(s * 3.0 + 20.0));
            vec3 sea = mix(uShallow, uOcean, smoothstep(0.5, 0.44, h));
            alb = mix(land, sea, water);
            alb = mix(alb, uIce, clamp(ice, 0.0, 1.0));
        } else {
            // a desert world: rust plains, dark basaltic maria, pale highlands, canyons, polar caps
            water = 0.0;
            float m = smoothstep(0.47, 0.41, h);
            alb = mix(uLand, uLand2, smoothstep(0.45, 0.62, fbm3(s * 7.0 + 4.0)));
            alb = mix(alb, uOcean, m * 0.8);
            alb = mix(alb, uDesert, smoothstep(0.6, 0.72, h));
            float canyon = smoothstep(0.03, 0.0, abs(fbm3(s * 6.0 + 2.0) - 0.5)) * smoothstep(0.3, 0.7, fbm3(s * 3.0));
            alb *= 1.0 - 0.45 * canyon;
            alb *= 0.75 + 0.5 * (fbmD(s * 40.0 + 2.0, foot, 1.0e6, 7) + 0.5);
            alb *= 1.0 + craters(s, foot);
            ice = smoothstep(0.86, 0.9, abs(s.y) + 0.05 * fbm3(s * 9.0));
            alb = mix(alb, uIce, ice);
        }
        // the clouds and their shadows
        vec3 sc = uCloudRot * n;
        float cv = clouds(sc, foot, true);
        float ce = max(0.018, foot / 2.0e5);
        float cl = smoothstep(0.53 - ce, 0.53 + ce * 1.5, cv) * uClouds;
        float cthick = smoothstep(0.53, 0.8, cv);
        float clS = smoothstep(0.5, 0.6, clouds(uCloudRot * normalize(n - uSunDir * 0.006), foot, false)) * uClouds;
        vec3 lightC = uSunC * (max(ndl, 0.0) * 1.15 + 0.03 * day);
        lightC = mix(lightC, lightC * vec3(1.15, 0.75, 0.55), tw * 0.6);
        col = alb * lightC * (1.0 - 0.55 * clS);
        // the sea's glint
        vec3 rf = reflect(d, n);
        float gl = pow(max(dot(rf, uSunDir), 0.0), 90.0) * 1.6 + pow(max(dot(rf, uSunDir), 0.0), 9.0) * 0.12;
        col += uSunGlow * gl * water * (1.0 - cl) * day;
        // the cities by night, under the clouds
        if (uCity > 0.0) {
            float region = smoothstep(0.5, 0.78, fbm3(s * 7.0 + 4.0));
            float coast = uType < 0.5 ? exp(-abs(h - 0.5) * 30.0) : 0.3;
            float belt = fbm3(s * 5.0 + 4.0);
            float pop = clamp(smoothstep(0.6, 0.75, belt) + coast * 0.7 * smoothstep(0.42, 0.6, belt), 0.0, 1.0) * (1.0 - water) * (1.0 - ice);
            // cities (some fifty km across) in the populated regions, each a cluster of lights, a soft glow
            float cn = vnoise3(s * 1500.0);
            float cities = smoothstep(0.7, 0.93, cn) * (0.6 + 0.4 * vnoise3(s * 4000.0));
            float core = smoothstep(0.86, 0.97, cn);
            float kd = smoothstep(3.0 * foot, 8.0 * foot, 2500.0);
            float dots = mix(0.25, smoothstep(0.6, 0.85, vnoise3(s * 16000.0)), kd);
            float spread = smoothstep(0.75, 0.95, vnoise3(s * 3000.0 + 7.0)) * mix(0.12, smoothstep(0.7, 0.9, vnoise3(s * 14000.0 + 3.0)), kd) * 0.4;
            float big = 0.25 + 2.2 * pow(vnoise3(s * 420.0 + 9.0), 3.0);
            float city = pop * (cities * (dots * 0.8 + 0.1) + core * 0.8 + spread) * big;
            col += uCityC * city * night * uCity * (1.0 - 0.75 * cl) * 1.6;
        }
        // the clouds over it all, lit by the sun, pink at the terminator, faintly by the moon at night
        vec3 cloudLit = uCloudC * (max(ndl, 0.0) * 1.2 + 0.04 * day) + uCloudC * vec3(1.0, 0.5, 0.35) * tw * 0.35 + vec3(0.55, 0.62, 0.8) * uNightLift * 0.09 * night;
        cloudLit *= (0.66 + 0.34 * cthick) * (0.8 + 0.4 * (fbmD(sc * 80.0 + 5.0, foot, 5.e5, 8) + 0.5));
        col = mix(col, cloudLit, cl * (0.75 + 0.2 * cthick));
        col += (alb * 0.4 + vec3(0.25, 0.3, 0.45)) * uNightLift * 0.03 * night * (1.0 - cl);
        // the air over the surface: blue on the day side, thicker toward the limb
        float mu = max(dot(n, -d), 0.0);
        float rim = pow(1.0 - mu, 3.0);
        vec3 airC = mix(uAtmoTwi, uAtmoDay, smoothstep(-0.02, 0.35, ndl)) * smoothstep(-0.22, 0.06, ndl);
        col = col * (1.0 - 0.35 * rim * day * uHaze) + airC * (0.07 + 0.6 * rim * rim) * uHaze;
    } else {
        // space: a deep sky, the Milky Way across it
        col = uSpaceC;
        vec3 mn = normalize(vec3(0.35, 0.55, 0.76));
        float bd = dot(d, mn);
        float band = exp(-sq(bd / 0.22));
        float dust = smoothstep(0.35, 0.7, fbm3(d * 7.0 + 3.0));
        float lane = 1.0 - 0.7 * smoothstep(0.55, 0.75, fbm3(d * 12.0 + 8.0)) * exp(-sq(bd / 0.06));
        col += uMilkyC * band * (0.25 + 0.75 * dust) * lane * uMilky;
        stars = 1.0;
        // the limb: the air seen edge-on over the planet's rim
        float tca = -b;
        if (tca > 0.0) {
            vec3 pt = E + d * tca;
            float hm = length(pt) - uR;
            vec3 nt = normalize(pt);
            float ndl = dot(nt, uSunDir);
            float dayT = smoothstep(-0.25, 0.25, ndl);
            float twT = exp(-sq((ndl + 0.02) / 0.16));
            float k1 = exp(-max(hm, 0.0) / 9000.0), k2 = exp(-max(hm, 0.0) / 32000.0);
            vec3 airC = mix(uAtmoTwi * 1.2, uAtmoDay, smoothstep(-0.05, 0.3, ndl));
            float lit = dayT + twT * 0.8;
            vec3 limb = airC * (k1 * 1.4 + k2 * 0.45) * lit;
            // forward scattering toward the sun, across the terminator
            limb += uSunGlow * k2 * twT * pow(max(sd, 0.0), 16.0) * 1.2;
            // the airglow: a thin line on the night side
            limb += uAirglow * exp(-sq((hm - 95000.0) / 7000.0)) * (1.0 - dayT) * 0.8;
            col += limb * (1.0 + 0.3 * uGlow);
            stars *= 1.0 - clamp(k2 * lit * 2.0, 0.0, 1.0);
        }
    }
    // the aurora: curtains of light over the night side, around the pole's oval
    if (uAurora > 0.0) {
        float Ro = uR + 320000.0;
        float c1 = dot(E, E) - Ro * Ro;
        float dd = b * b - c1;
        if (dd > 0.0) {
            float tq = sqrt(dd);
            float t0 = max(-b - tq, 0.0), t1 = min(-b + tq, tHit);
            if (t1 > t0) {
                vec3 acc = vec3(0.0);
                const int N = 14;
                float dt = (t1 - t0) / float(N);
                for (int i = 0; i < N; i++) {
                    float t = t0 + (float(i) + 0.5) * dt;
                    vec3 P = E + d * t;
                    float r = length(P);
                    float h = r - uR;
                    if (h < 90000.0) continue;
                    vec3 s = uPlanetRot * (P / r);
                    float colat = acos(clamp(s.y, -1.0, 1.0));
                    float lon = atan(s.z, s.x);
                    float wave = 0.05 * sin(lon * 4.0 + uTime * 0.05) + 0.025 * sin(lon * 11.0 - uTime * 0.09);
                    float oval = exp(-sq((colat - 0.42 - wave) / 0.022)) + 0.5 * exp(-sq((colat - 0.47 - wave * 1.3) / 0.012));
                    float rays = pow(vnoise(vec2(lon * 160.0, uTime * 0.2)), 2.0) * 0.8 + 0.2 * vnoise(vec2(lon * 40.0, uTime * 0.1));
                    float prof = smoothstep(90000.0, 110000.0, h) * exp(-(h - 110000.0) / 70000.0);
                    float nightP = 1.0 - smoothstep(-0.1, 0.1, dot(P / r, uSunDir));
                    vec3 ac = mix(uAurora1, uAurora2, smoothstep(120000.0, 260000.0, h));
                    acc += ac * oval * rays * prof * nightP;
                }
                col += acc * dt / 1000000.0 * uAurora;
            }
        }
    }
    // the sun: a white disc, a soft glow (behind the planet, only its glow over the limb)
    {
        float ang = acos(clamp(sd, -1.0, 1.0));
        float dsk = smoothstep(uSunSize * 1.05, uSunSize * 0.95, ang) * (hit ? 0.0 : 1.0);
        float glow = exp(-ang / max(uSunSize * 2.5, 1e-3)) * 0.55 + pow(max(sd, 0.0), 60.0) * 0.07 + pow(max(sd, 0.0), 600.0) * 0.5;
        col += uSunGlow * glow * uSunVis * (hit ? 0.25 : 1.0) * (1.0 + 0.4 * uGlow);
        col = mix(col, uSunC * 6.0, dsk * uSunVis);
        stars *= 1.0 - clamp(glow * 3.0 * uSunVis + dsk, 0.0, 1.0);
        if (uDyson > 0.0 && !hit && sd > 0.9) {
            vec3 bx = normalize(cross(uSunDir, vec3(0.0, 1.0, 0.0)));
            vec3 by = cross(bx, uSunDir);
            vec2 q = vec2(dot(d, bx), dot(d, by)) / uSunSize;
            float acc = 0.0;
            for (int k = 0; k < 4; k++) {
                float tilt = 0.25 + 0.2 * float(k);
                vec2 e = vec2(q.x, q.y / tilt);
                float rr = length(e), R = 3.0 + 2.6 * float(k);
                float a = atan(e.y, e.x) + uTime * (0.02 + 0.01 * float(k)) * (k % 2 == 0 ? 1.0 : -1.0);
                float n = 60.0 + 30.0 * float(k);
                float cell = floor(a / TAU * n);
                float ca = (cell + 0.5) / n * TAU;
                float on = step(0.35, hash(vec2(cell, float(k))));
                vec2 pc = vec2(cos(ca), sin(ca)) * R;
                float dd = length(e - pc);
                acc += exp(-dd * dd / 0.02) * on * (0.6 + 0.4 * sin(uTime * 2.0 + cell));
                acc += exp(-sq((rr - R) / 0.08)) * 0.08;
            }
            col += vec3(1.0, 0.9, 0.72) * acc * uDyson * uSunVis * 1.5;
        }
    }
    // the moon
    if (uMoonSize > 0.0 && !hit) {
        float m = dot(d, uMoonDir);
        float ang = acos(clamp(m, -1.0, 1.0));
        if (ang < uMoonSize * 1.1) {
            vec3 bx = normalize(cross(uMoonDir, vec3(0.0, 1.0, 0.0)));
            vec3 by = cross(bx, uMoonDir);
            vec2 q = vec2(dot(d, bx), dot(d, by)) / sin(uMoonSize);
            float r2 = dot(q, q);
            if (r2 < 1.0) {
                vec3 mn = normalize(bx * q.x + by * q.y - uMoonDir * sqrt(1.0 - r2));
                float mar = smoothstep(0.45, 0.65, fbm3(mn * 2.5 + 5.0));
                vec3 alb = mix(vec3(0.85, 0.85, 0.88), vec3(0.5, 0.5, 0.56), mar);
                float ml = smoothstep(-0.05, 0.1, dot(mn, uSunDir)) * 0.9 + 0.04;
                col = mix(col, alb * ml * 1.3, smoothstep(1.0, 0.96, r2));
                stars = 0.0;
            }
            col += vec3(0.6, 0.65, 0.8) * exp(-ang / (uMoonSize * 1.5)) * 0.05;
        }
    }
    col = mix(col, uBridgeCol, uBridge);
    frag = vec4(col, clamp(stars, 0.0, 1.0) * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the background under the station, with the stars
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSky;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan, uRes;
uniform float uTwinkle, uTime, uStars;
uniform vec3 uEyeP;
uniform float uAlt, uR;
uniform mat4 uVP;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec4 w = texelFetch(uSky, ivec2(gl_FragCoord.xy), 0);
    {
        // the planet's depth: what passes behind it is hidden
        vec3 d = rayDir(vUv);
        float b = dot(uEyeP, d);
        float c0 = uAlt * (uAlt + 2.0 * uR);
        float disc = b * b - c0;
        float depth = 1.0;
        if (disc > 0.0 && b < 0.0) {
            float t = c0 / (-b + sqrt(disc));
            vec4 cp = uVP * vec4(d * t, 1.0);
            depth = clamp(cp.z / cp.w * 0.5 + 0.5, 0.0, 1.0);
        }
        gl_FragDepth = depth;
    }
    vec3 c = w.rgb;
    float vis = w.a * uStars;
    if (vis > 0.002) {
        vec3 d = rayDir(vUv);
        // stars fixed on the sky: each pixel looks at its own cell, the star kept off the cell's edges, its glow
        // measured in screen pixels through the projection's Jacobian; a layer whose cells shrink under a few
        // pixels fades out (or the grid aliases into streaks)
        vec3 ad = abs(d);
        vec2 face = ad.x > ad.y && ad.x > ad.z ? d.yz / ad.x + vec2(7.0 * sign(d.x), 0.0)
                  : ad.y > ad.z ? d.xz / ad.y + vec2(0.0, 7.0 * sign(d.y)) : d.xy / ad.z + vec2(3.5 * sign(d.z), 3.5);
        vec2 sp = face * 300.0 * max(uRes.y / 1080.0, 0.5);
        mat2 J = mat2(dFdx(sp), dFdy(sp));
        float det = J[0][0] * J[1][1] - J[1][0] * J[0][1];
        mat2 Ji = abs(det) > 1e-8 ? inverse(J) : mat2(0.0);
        float px = sqrt(abs(det)) + 1e-4;
        vec3 st = vec3(0.0);
        for (int L = 0; L < 3; L++) {
            float cell = L == 0 ? 1.0 : L == 1 ? 3.1 : 9.7;
            float fade = smoothstep(4.0, 7.0, cell / px);
            if (fade <= 0.0) continue;
            vec2 g = floor(sp / cell);
            float rr = hash(g + float(L) * 31.0);
            if (rr < (L == 0 ? 0.06 : L == 1 ? 0.08 : 0.1)) {
                vec2 o = (g + 0.25 + 0.5 * hash2(g + 3.1)) * cell;
                vec2 dd = Ji * (sp - o);
                float b = pow(hash(g + 7.7), 2.2);
                float rad = (L == 0 ? 0.6 : 1.0) + 0.5 * b;
                float tw = 1.0 + uTwinkle * sin(uTime * (1.2 + 3.0 * hash(g + 1.3)) + rr * 80.0);
                vec3 tint = mix(vec3(0.7, 0.85, 1.0), vec3(1.0, 0.88, 0.7), hash(g + 9.1));
                st += tint * exp(-dot(dd, dd) / (rad * rad)) * b * max(tw, 0.0) * (L == 0 ? 0.6 : 1.3) * fade;
            }
        }
        c += st * vis;
    }
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the station's solids
// One instance: center + shape, axis X + material, axis Y + p0, axis Z + p1, color + seed, (p2, p3, glow, -).
// Shapes: 0 box (its own mesh), 1 cylinder (p0: the top's radius scale, p1 < 0: open), 2 sphere, 3 torus arc (X/Z:
// the ring, Y: the tube radius, p0/p1: the arc's start and length), 5 band arc (an open cylinder of radius |X|, half
// width |Y|, p0/p1 the arc, p2 its rim walls' height).
inline const char* PART_HEAD = R"(
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iX;
layout(location = 3) in vec4 iY;
layout(location = 4) in vec4 iZ;
layout(location = 5) in vec4 iC;
layout(location = 6) in vec4 iE;
uniform mat4 uVP;
uniform vec3 uEye;
out vec3 vP, vN;
out vec2 vUV;
out vec3 vLoc;
flat out vec4 vMat;   // material, p0, p1, p2
flat out vec4 vCol;
flat out vec4 vExt;   // glow, size u, size v, face/region
flat out float vSpin; // its ring's turn (taken off the drop's chase)
)";

inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 7) in vec3 aNrm;
void main() {
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    vec3 hs = vec3(length(X), length(Y), length(Z));
    vec3 ux = X / max(hs.x, 1e-9), uy = Y / max(hs.y, 1e-9), uz = Z / max(hs.z, 1e-9);
    vec3 P = iA.xyz + X * aPos.x + Y * aPos.y + Z * aPos.z;
    vP = P;
    vN = ux * aNrm.x + uy * aNrm.y + uz * aNrm.z;
    vLoc = aPos * hs;
    float face;
    vec2 sz;
    if (abs(aNrm.x) > 0.5) { vUV = vec2(aPos.z * hs.z, aPos.y * hs.y); sz = hs.zy; face = 0.0; }
    else if (abs(aNrm.y) > 0.5) { vUV = vec2(aPos.x * hs.x, aPos.z * hs.z); sz = hs.xz; face = 1.0 + (aNrm.y > 0.0 ? 0.0 : 0.5); }
    else { vUV = vec2(aPos.x * hs.x, aPos.y * hs.y); sz = hs.xy; face = 2.0; }
    vMat = vec4(iX.w, iY.w, iZ.w, iE.y);
    vCol = iC;
    vExt = vec4(iE.z, sz, face);
    vSpin = iE.w;
    gl_Position = uVP * vec4(P - uEye, 1.0);
}
)";

inline const char* PARAM_VS = R"(#version 330 core
layout(location = 0) in vec3 aPar;   // u around (0..1), t along the region (0..1), region (0 cap, 1 side, 2 cap)
void main() {
    int shape = int(iA.w + 0.5);
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    float lx = length(X), ly = length(Y), lz = length(Z);
    vec3 ux = X / max(lx, 1e-9), uy = Y / max(ly, 1e-9), uz = Z / max(lz, 1e-9);
    float u = aPar.x, t = aPar.y;
    int reg = int(aPar.z + 0.5);
    vec3 pl = vec3(0.0), nl = vec3(0.0, 1.0, 0.0);
    vec3 P, N;
    vec2 uv = vec2(0.0);
    vec2 sz = vec2(1.0);
    float region = float(reg);
    if (shape == 1) {
        float a = u * TAU;
        vec2 cs = vec2(cos(a), sin(a));
        float top = iY.w;
        bool open = iZ.w < 0.0;
        if (reg == 1) {
            float y = t * 2.0 - 1.0;
            float r = mix(1.0, top, t);
            pl = vec3(cs.x * r, y, cs.y * r);
            nl = normalize(vec3(cs.x / lx, -(top - 1.0) * 0.5 / ly, cs.y / lz));
            P = iA.xyz + ux * pl.x * lx + uy * pl.y * ly + uz * pl.z * lz;
            N = normalize(ux * nl.x + uy * nl.y + uz * nl.z);
            float rm = 0.5 * (lx + lz) * mix(1.0, top, 0.5);
            uv = vec2(u * TAU * rm, y * ly);
            sz = vec2(PI * rm, ly);
        } else {
            float y = reg == 0 ? -1.0 : 1.0;
            float r = (reg == 0 ? 1.0 : top) * (open ? 1.0 : t);
            pl = vec3(cs.x * r, y, cs.y * r);
            P = iA.xyz + ux * pl.x * lx + uy * pl.y * ly + uz * pl.z * lz;
            N = uy * y;
            uv = vec2(cs.x * r * lx, cs.y * r * lz);
            sz = vec2(lx, lz);
        }
    } else if (shape == 2) {
        float a = u * TAU;
        float lat = (reg == 1 ? t * 2.0 - 1.0 : (reg == 0 ? -1.0 : 1.0)) * PI * 0.5;
        pl = vec3(cos(lat) * cos(a), sin(lat), cos(lat) * sin(a));
        P = iA.xyz + ux * pl.x * lx + uy * pl.y * ly + uz * pl.z * lz;
        N = normalize(ux * pl.x / lx + uy * pl.y / ly + uz * pl.z / lz);
        uv = vec2(u * TAU * lx, lat * ly);
        sz = vec2(PI * lx, PI * 0.5 * ly);
    } else if (shape == 3) {
        float R = lx, r = ly;
        float a0 = iY.w, al = iZ.w;
        if (reg == 1) {
            float a = a0 + u * al, bb = t * TAU;
            vec3 e = ux * cos(a) + uz * sin(a);
            N = e * cos(bb) + uy * sin(bb);
            P = iA.xyz + e * R + N * r;
            uv = vec2(u * al * R, (t - 0.5) * TAU * r);
            sz = vec2(al * R * 0.5, PI * r);
        } else {
            float a = reg == 0 ? a0 : a0 + al;
            vec3 e = ux * cos(a) + uz * sin(a);
            vec3 tg = -ux * sin(a) + uz * cos(a);
            float bb = u * TAU;
            P = iA.xyz + e * R + (e * cos(bb) + uy * sin(bb)) * r * t;
            N = tg * (reg == 0 ? -1.0 : 1.0);
            uv = vec2(cos(bb), sin(bb)) * r * t;
            sz = vec2(r);
        }
    } else {
        // band: an open cylinder arc, its rim walls at the caps
        float R = lx, hw = ly;
        float a0 = iY.w, al = iZ.w;
        float a = a0 + u * al;
        vec3 e = ux * cos(a) + uz * sin(a);
        if (reg == 1) {
            float y = t * 2.0 - 1.0;
            P = iA.xyz + e * R + uy * (y * hw);
            N = e;
            uv = vec2(u * al * R, y * hw);
            sz = vec2(al * R * 0.5, hw);
        } else {
            float y = reg == 0 ? -1.0 : 1.0;
            float wh = iE.x;
            P = iA.xyz + e * (R - wh * t) + uy * (y * hw);
            N = uy * y;
            uv = vec2(u * al * R, t * wh);
            sz = vec2(al * R * 0.5, wh);
        }
    }
    vP = P;
    vN = N;
    vUV = uv;
    vLoc = pl;
    vMat = vec4(iX.w, iY.w, iZ.w, iE.y);
    vCol = iC;
    vExt = vec4(iE.z, sz, region + 4.0 + float(shape) * 10.0);
    vSpin = iE.w;
    gl_Position = uVP * vec4(P - uEye, 1.0);
}
)";

// The truss lattice (shared by the lit pass and the shadow pass): coverage of the members at a face point.
inline const char* LATTICE_GLSL = R"(
float lattice(vec2 uv, vec2 sz, float px) {
    sz = max(sz, vec2(1e-4));
    // bays as long as the face is wide, along its long side; members are a frame, the bays' struts, a diagonal each
    bool longX = sz.x >= sz.y;
    vec2 q = longX ? uv : uv.yx;
    vec2 s = longX ? sz : sz.yx;
    float w = s.y * 0.11;
    float bay = s.y * 2.0;
    float n = max(1.0, floor(s.x * 2.0 / bay + 0.5));
    bay = s.x * 2.0 / n;
    float fx = (q.x + s.x) / bay;
    float cx = fract(fx);
    float frame = s.y - abs(q.y);
    float strut = min(cx, 1.0 - cx) * bay;
    float dir = mod(floor(fx), 2.0) < 1.0 ? 1.0 : -1.0;
    float yy = (q.y / s.y) * 0.5 + 0.5;
    float diag = abs((cx - (dir > 0.0 ? yy : 1.0 - yy))) * bay / sqrt(1.0 + sq(bay / (2.0 * s.y)));
    float m = min(frame, min(strut, diag));
    float aa = max(px, 1e-6);
    float cov = 1.0 - smoothstep(w - aa, w + aa, m);
    // too fine to resolve: a mean coverage
    float far = smoothstep(0.25, 1.0, aa / w);
    return mix(cov, 0.55, far);
}
)";

inline const char* PART_FS = R"(#version 330 core
in vec3 vP, vN;
in vec2 vUV;
in vec3 vLoc;
flat in vec4 vMat;
flat in vec4 vCol;
flat in vec4 vExt;
flat in float vSpin;
out vec4 frag;
uniform vec3 uEye;
uniform vec3 uKeyDir, uKeyC, uEarthC, uAmbC, uEnvUp, uEnvDown;
uniform float uDipS;           // sin of the horizon's dip: below it, the planet
uniform vec3 uPlanetC;         // the planet's center
uniform float uPlanetR;
uniform sampler2DShadow uShadow;
uniform mat4 uShadowVP;
uniform vec3 uShadowO;
uniform float uShadowTexel, uShadowBias;
uniform float uTime, uLights, uBridge, uNight;
uniform vec3 uBridgeCol;
uniform vec3 uWindowC, uLandA, uLandB, uLake, uRock, uTownC, uCloudC;
uniform float uRingDay;
uniform vec4 uChase;     // the drop's heads of light: wheel's head - its turn, great ring's head - its turn, strength, base
uniform vec3 uChaseC;
// the chase at a ring's local angle (heads of light sweeping round, each with a tail)
float chase(float a, float head, float heads) {
    float x = (head - a) / TAU * heads;
    x -= floor(x);
    return exp(-x * 9.0) + 0.6 * exp(-(1.0 - x) * 40.0);
}
float shadowAt(vec3 P, vec3 N) {
    vec4 s = uShadowVP * vec4(P - uShadowO + N * uShadowBias * 2.0, 1.0);
    vec3 q = s.xyz / s.w * 0.5 + 0.5;
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
    float r = 0.0;
    vec2 o[5] = vec2[5](vec2(0.0), vec2(1.2, 0.4), vec2(-0.4, 1.2), vec2(-1.2, -0.4), vec2(0.4, -1.2));
    for (int i = 0; i < 5; i++) r += texture(uShadow, vec3(q.xy + o[i] * uShadowTexel, q.z - uShadowBias * 0.0));
    return r / 5.0;
}
vec3 env(vec3 R, vec3 P) {
    // what a shiny surface mirrors: the planet where it fills the sky, deep space elsewhere
    vec3 toE = uPlanetC - P;
    float dE = length(toE);
    float cosA = sqrt(max(0.0, 1.0 - sq(uPlanetR / dE)));
    float e = smoothstep(cosA - 0.03, cosA + 0.03, dot(R, toE / dE));
    return mix(uEnvUp, uEnvDown, e);
}
void main() {
    int mat = int(vMat.x + 0.5);
    float seed = vCol.a;
    vec3 N = normalize(vN);
    vec3 V = normalize(uEye - vP);
    bool inner = dot(N, V) < 0.0;
    if (inner) N = -N;
    vec2 uv = vUV;
    vec2 sz = vExt.yz;
    float px = max(length(fwidth(uv)), 1e-6);
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.15, gloss = 40.0, refl = 0.0;
    float alpha = 1.0;
    vec3 bump = vec3(0.0);
    if (mat == 0) {
        // white hull: insulation blankets in quilted panels, seams, a few darker straps
        vec2 g = uv / vec2(1.25, 0.9);
        vec2 cell = floor(g);
        vec2 f = fract(g);
        float seam = 1.0 - smoothstep(0.0, 0.03 + px / 1.0, min(min(f.x, 1.0 - f.x) * 1.25, min(f.y, 1.0 - f.y) * 0.9));
        float det = 1.0 - smoothstep(0.02, 0.15, px);
        alb *= 0.92 + 0.08 * hash(cell + seed * 13.0);
        alb *= 1.0 - 0.22 * seam * det;
        float strap = smoothstep(0.08, 0.0, abs(fract(uv.y / 3.7 + seed) - 0.5) - 0.44) * det;
        alb = mix(alb, alb * 0.72, strap * 0.6);
        bump = vec3(vnoise(uv * 3.0) - 0.5, vnoise(uv * 3.0 + 7.0) - 0.5, 0.0) * 0.12 * det;
        spec = 0.12;
        gloss = 30.0;
    } else if (mat == 1) {
        // gold foil: crinkled, bright, a broad warm sheen
        float det = 1.0 - smoothstep(0.03, 0.2, px);
        vec2 q = uv * 2.5;
        float n1 = vnoise(q), n2 = vnoise(q * 2.3 + 4.0);
        bump = vec3(n1 - 0.5, n2 - 0.5, 0.0) * 0.9 * det;
        alb *= 0.75 + 0.35 * n2;
        spec = 0.9;
        gloss = 22.0;
        refl = 0.35;
    } else if (mat == 2) {
        // solar cells: deep blue squares, silver lines, a bronze frame every few meters; a sun glint
        vec2 g = uv / 0.55;
        vec2 f = abs(fract(g) - 0.5);
        float line = 1.0 - smoothstep(0.43, 0.47 + px / 0.55, max(f.x, f.y));
        float det = 1.0 - smoothstep(0.05, 0.25, px);
        vec2 gb = uv / vec2(4.2, 3.0);
        vec2 fb = abs(fract(gb) - 0.5);
        float frame = smoothstep(0.47 - px / 3.0, 0.49, max(fb.x * 1.0, fb.y));
        vec3 cellC = alb * (0.85 + 0.25 * hash(floor(gb) + seed));
        alb = mix(cellC, vec3(0.75, 0.78, 0.82), (1.0 - line) * 0.55 * det + (1.0 - det) * 0.1);
        alb = mix(alb, vec3(0.75, 0.52, 0.22), frame * det);
        spec = 1.4;
        gloss = 120.0;
        refl = 0.45;
    } else if (mat == 3) {
        // radiator: white with fine ribs
        float rib = smoothstep(0.3, 0.5, abs(fract(uv.x / 0.22) - 0.5)) * (1.0 - smoothstep(0.02, 0.1, px));
        alb *= 1.0 - 0.12 * rib;
        bump.x = (fract(uv.x / 0.22) - 0.5) * 0.25 * (1.0 - smoothstep(0.02, 0.1, px));
        spec = 0.3;
        gloss = 50.0;
        refl = 0.1;
    } else if (mat == 4) {
        // lattice truss
        alpha = lattice(uv, sz, px);
        alb *= 0.9 + 0.1 * hash(floor(uv * 0.5) + seed);
        spec = 0.5;
        gloss = 40.0;
        refl = 0.12;
    } else if (mat == 5) {
        // dark metal
        spec = 0.6;
        gloss = 50.0;
        refl = 0.15;
    } else if (mat == 6) {
        // a light
        emit = alb * vExt.x;
        alb *= 0.15;
    } else if (mat == 7) {
        // a habitat's hull: rows of windows, lit from inside
        vec2 g = uv / vec2(1.6, 1.0);
        vec2 cell = floor(g);
        vec2 f = abs(fract(g) - 0.5);
        float row = smoothstep(0.24, 0.18, abs(abs(uv.y) / max(sz.y, 1e-3) - 0.32));
        float win = (1.0 - smoothstep(0.28, 0.34 + px, f.x)) * (1.0 - smoothstep(0.3, 0.36 + px, f.y)) * row;
        float lit = step(0.25, hash(cell + seed * 7.0));
        float far = smoothstep(0.15, 0.5, px);
        win = mix(win, row * 0.35, far);
        vec2 gp = uv / vec2(2.4, 1.6);
        vec2 fp = fract(gp);
        float seam = 1.0 - smoothstep(0.0, 0.04 + px / 2.0, min(min(fp.x, 1.0 - fp.x), min(fp.y, 1.0 - fp.y)));
        alb *= 1.0 - 0.18 * seam * (1.0 - far);
        alb = mix(alb, vec3(0.05, 0.06, 0.08), win);
        emit = uWindowC * win * mix(lit, 0.75, far) * (0.35 + 0.65 * uLights) * vExt.x;
        // on the wheel: a strip of light along each face of the tube, the drop's chase running round it
        if (floor((vExt.w - 4.0) / 10.0 + 0.01) == 3.0) {
            float r = sz.y / PI;
            float aa = vMat.y + uv.x * vMat.z / max(2.0 * sz.x, 1e-3);
            float sd = abs(abs(uv.y) - 0.5 * PI * r);
            float strip = 1.0 - smoothstep(0.18, 0.18 + px * 1.5, sd);
            float ch = chase(aa, uChase.x + vSpin, 2.0);
            emit += uChaseC * strip * (uChase.w + ch * uChase.z * 3.0);
        }
        spec = 0.2;
        gloss = 40.0;
    } else if (mat == 8) {
        // the ring's land (inside), its hull (outside), its rim walls
        float reg = mod(vExt.w - 4.0, 10.0);
        if (reg > 0.5 && reg < 1.5 && inner) {
            vec2 w = uv;
            float hw = sz.y;
            float across = w.y / hw;
            vec2 q = w / hw;
            float foot = px / hw;
            float k1 = 1.0 - smoothstep(0.01, 0.04, foot);
            float n = fbm(q * 0.7 + seed * 10.0) * 0.7 + fbm(q * 2.9) * 0.3;
            // a river winding along the ring, lakes, meadows and forests, rocks and snow up the walls
            float riv = abs(across - 0.3 * sin(q.x * 0.9 + 1.3 * sin(q.x * 0.31)) - 0.05);
            float river = 1.0 - smoothstep(0.018, 0.03 + foot * 1.5, riv);
            float lake = smoothstep(0.64, 0.67, fbm(q * 0.7 + 7.0)) * (1.0 - smoothstep(0.5, 0.75, abs(across)));
            float meadow = smoothstep(0.3, 0.72, n);
            vec3 land = mix(uLandA, uLandB, meadow);
            land *= 0.9 + 0.2 * fbm(q * 9.0) * k1;
            float rock = smoothstep(0.86, 0.95, abs(across) + 0.04 * fbm(q * 3.0));
            land = mix(land, uRock, rock);
            land = mix(land, vec3(0.92, 0.95, 1.0), smoothstep(0.93, 0.98, abs(across) + 0.04 * fbm(q * 5.0)));
            float wat = max(river * (1.0 - rock), lake);
            alb = mix(land, uLake, wat);
            spec = mix(0.05, 1.2, wat);
            gloss = mix(10.0, 90.0, wat);
            refl = 0.25 * wat;
            // its own daylight (mirrored in), and towns lighting up as it fills
            emit = alb * vExt.x * uRingDay;
            float town = smoothstep(0.7, 0.82, vnoise(q * 4.0 + 3.0)) * (1.0 - wat) * (1.0 - rock);
            float dots = smoothstep(0.6, 0.9, vnoise(q * 40.0)) * k1 + (1.0 - k1) * 0.08;
            emit += uTownC * town * dots * vMat.w * (0.3 + 0.7 * uLights) * 1.6;
            // a few clouds drifting inside
            float cl = smoothstep(0.66, 0.8, fbm(q * vec2(0.5, 0.9) + vec2(uTime * 0.003, 0.0) + 3.0)) * (1.0 - smoothstep(0.5, 0.8, abs(across)));
            alb = mix(alb, uCloudC, cl * 0.75);
            emit = mix(emit, uCloudC * vExt.x * uRingDay, cl * 0.75);
        } else if (reg > 0.5 && reg < 1.5) {
            // outside: a dark hull in long bands, its seams catching the light, rows of windows
            float y = uv.y / sz.y;
            float bandF = abs(fract(y * 1.5 + 0.5) - 0.5);
            float seam = 1.0 - smoothstep(0.0, 0.03 + px / sz.y * 2.0, 0.5 - bandF);
            float panel = abs(fract(uv.x / (sz.y * 0.6)) - 0.5);
            float pseam = 1.0 - smoothstep(0.0, 0.01 + px / sz.y, 0.5 - panel);
            alb = vec3(0.07, 0.075, 0.09) * (0.85 + 0.3 * hash(floor(vec2(uv.x / (sz.y * 0.6), y * 1.5)) + seed));
            alb = mix(alb, vec3(0.4), max(seam, pseam * 0.5) * 0.5);
            spec = 0.8;
            gloss = 60.0;
            refl = 0.2;
            float rows = exp(-sq((abs(y) - 0.18) / 0.012)) + exp(-sq((abs(y) - 0.55) / 0.01));
            float dotsL = smoothstep(0.3, 0.0, abs(fract(uv.x / (sz.y * 0.025)) - 0.5) - 0.2);
            float far = smoothstep(0.1, 0.4, px / (sz.y * 0.025));
            emit = uWindowC * rows * mix(dotsL, 0.5, far) * 0.6 * (0.35 + 0.65 * uLights);
            // a band of light along the hull, the drop's chase running round it
            float aa = vMat.y + uv.x * vMat.z / max(2.0 * sz.x, 1e-3);
            float band2 = 1.0 - smoothstep(0.035, 0.035 + px / sz.y * 1.5, abs(abs(y) - 0.36));
            emit += uChaseC * band2 * (uChase.w * 0.6 + chase(aa, uChase.y + vSpin, 4.0) * uChase.z * 3.0);
        } else {
            // the rim walls: glass and steel
            alb = vec3(0.08, 0.1, 0.13);
            spec = 1.0;
            gloss = 90.0;
            refl = 0.45;
            float rib = 1.0 - smoothstep(0.0, 0.02 + px / max(sz.y, 1.0), abs(fract(uv.x / (sz.y * 1.5)) - 0.5) - 0.46);
            alb = mix(alb, vec3(0.6), rib * 0.6);
        }
    } else if (mat == 9) {
        // a heat shield: dark brown, scorched
        alb *= 0.7 + 0.3 * vnoise(uv * 4.0);
        spec = 0.05;
    } else if (mat == 10) {
        // glass: dark, mirroring, a warm room behind it
        spec = 2.0;
        gloss = 160.0;
        refl = 0.6;
        emit = uWindowC * 0.35 * (0.4 + 0.6 * uLights) * vExt.x;
        alb *= 0.2;
    } else if (mat == 11) {
        // polished metal, a mirror
        spec = 2.0;
        gloss = 200.0;
        refl = 0.85;
        alb *= 0.35;
    } else if (mat == 12) {
        // the Moon: seas and craters; its works: a grid of glowing seams, the great pit of its hollowing
        vec3 q = normalize(vLoc);
        float mar = smoothstep(0.42, 0.62, fbm3(q * 2.3 + 5.0));
        float cr = 0.0;
        for (int i = 0; i < 3; i++) {
            vec3 pp = q * (5.0 + 8.0 * float(i)) + float(i) * 7.0;
            vec3 cc = floor(pp);
            vec3 o = vec3(hash3(cc), hash3(cc + 3.1), hash3(cc + 7.7)) * 0.5 + 0.25;
            float r = length(fract(pp) - o);
            float Rc = 0.12 + 0.16 * hash3(cc + 1.3);
            cr += (exp(-sq((r - Rc) / (Rc * 0.22))) * 0.35 - smoothstep(Rc, Rc * 0.6, r) * 0.28) * step(0.45, hash3(cc + 5.9));
        }
        alb = mix(vec3(0.8, 0.8, 0.82), vec3(0.4, 0.4, 0.45), mar) * (1.0 + cr) * (0.85 + 0.3 * fbm3(q * 40.0));
        spec = 0.03;
        gloss = 8.0;
        float work = vExt.x;
        float lon = atan(q.z, q.x), lat = asin(clamp(q.y, -1.0, 1.0));
        vec2 g = vec2(lon * 18.0 / TAU, lat * 12.0 / PI);
        vec2 gw = fwidth(g) + 1e-4;
        vec2 gd = abs(fract(g) - 0.5);
        float seam = max(smoothstep(0.5 - gw.x * 1.5 - 0.008, 0.5, gd.x), smoothstep(0.5 - gw.y * 1.5 - 0.008, 0.5, gd.y));
        float pit = dot(q, normalize(vec3(0.56, 0.37, 0.74)));
        float hole = smoothstep(0.8, 0.84, pit);
        float rimP = exp(-sq((pit - 0.8) / 0.012));
        alb *= 1.0 - 0.85 * hole * work;
        emit = (uWindowC * seam * 0.9 + vec3(1.0, 0.55, 0.22) * (hole * (0.6 + 0.4 * fbm3(q * 60.0)) * 1.6 + rimP * 0.8)) * work;
    }
    // light: the key (the sun, or the moon), its shadow, the planet's glow from below, a faint fill
    vec3 T = normalize(cross(N, abs(N.y) < 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 B = cross(N, T);
    vec3 Nb = normalize(N + T * bump.x + B * bump.y);
    float ndl = dot(Nb, uKeyDir);
    float sh = ndl > 0.0 ? shadowAt(vP, N) : 0.0;
    {
        // the planet's shadow
        vec3 o = vP - uPlanetC;
        float bb = dot(o, uKeyDir);
        if (bb < 0.0) sh *= smoothstep(uPlanetR * 0.985, uPlanetR * 1.015, sqrt(max(dot(o, o) - bb * bb, 0.0)));
    }
    vec3 H = normalize(uKeyDir + V);
    float fres = 0.04 + 0.96 * pow(1.0 - max(dot(Nb, V), 0.0), 5.0);
    vec3 c = alb * uKeyC * max(ndl, 0.0) * sh;
    c += uKeyC * spec * pow(max(dot(Nb, H), 0.0), gloss) * (gloss + 8.0) / 64.0 * sh * step(0.0, ndl);
    // a rim of light on the edges turned from a light behind them
    c += uKeyC * pow(1.0 - max(dot(N, V), 0.0), 4.0) * max(dot(-V, uKeyDir), 0.0) * 0.5 * max(max(alb.r, alb.g), alb.b);
    vec3 fromE = vP - uPlanetC;
    float dPl = length(fromE);
    float down = 0.5 - 0.5 * dot(Nb, fromE / dPl);
    c += alb * (uEarthC * down * down * min(1.0, sq(uPlanetR * 1.07 / dPl)) + uAmbC);
    vec3 Rf = reflect(-V, Nb);
    c += env(Rf, vP) * refl * mix(0.5, 1.0, fres);
    c += emit;
    c = mix(c, uBridgeCol, uBridge);
    if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
    frag = vec4(min(c, vec3(64.0)), alpha);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vP, vN;
in vec2 vUV;
in vec3 vLoc;
flat in vec4 vMat;
flat in vec4 vCol;
flat in vec4 vExt;
out vec4 frag;
void main() {
    int mat = int(vMat.x + 0.5);
    if (mat == 4) {
        float px = max(length(fwidth(vUV)), 1e-6);
        if (lattice(vUV, vExt.yz, px) < 0.5) discard;
    }
    if (mat == 6) discard;
    frag = vec4(1.0);
}
)";

// ---------------------------------------------------------------- lights: lamps, thrusters, sparks
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0;   // position, radius (meters)
layout(location = 2) in vec4 i1;   // rgb, intensity
layout(location = 3) in vec4 i2;   // direction (thrusters: the plume's), mode
uniform mat4 uVP;
uniform vec3 uEye, uCamR, uCamU;
uniform float uMinPx;              // the size of a pixel at one meter
out vec2 vLocal;
flat out vec4 vB;
flat out float vMode;
out float vDim;
void main() {
    float d = max(length(i0.xyz - uEye), 1e-3);
    vLocal = aPos;
    vB = i1;
    int mode = int(i2.w + 0.5);
    vMode = float(mode);
    float minS = uMinPx * d * 2.4;
    float s = max(i0.w, minS);
    vDim = min(1.0, sq(i0.w / minS) + 0.3);
    vec3 w;
    if (mode == 2) {
        // a plume along its direction, seen across
        vec3 ax = normalize(i2.xyz);
        vec3 toE = normalize(uEye - i0.xyz);
        vec3 side = cross(ax, toE);
        float sl = length(side);
        side = sl > 1e-3 ? side / sl : uCamR;
        float len = s * 3.5;
        w = i0.xyz + ax * (aPos.y + 1.0) * 0.5 * len + side * aPos.x * s * 0.6 - ax * s * 0.3;
    } else {
        w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * s;
    }
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vB;
flat in float vMode;
in float vDim;
out vec4 frag;
void main() {
    int mode = int(vMode + 0.5);
    float r = length(vLocal);
    float v;
    if (mode == 2) {
        float along = vLocal.y * 0.5 + 0.5;
        float wdt = mix(0.35, 1.0, along);
        float core = exp(-sq(vLocal.x / (0.18 * wdt))) * exp(-along * 3.0);
        float halo = exp(-sq(vLocal.x / (0.6 * wdt))) * exp(-along * 1.6) * 0.35;
        v = (core + halo) * smoothstep(1.0, 0.7, along) * smoothstep(-0.05, 0.05, along);
    } else if (mode == 3) {
        // a glint: a small four-pointed star
        float rays = exp(-abs(vLocal.x) * 30.0) * exp(-abs(vLocal.y) * 2.5) + exp(-abs(vLocal.y) * 30.0) * exp(-abs(vLocal.x) * 2.5);
        v = exp(-r * r * 40.0) + 0.5 * rays + 0.1 * exp(-r * r * 5.0);
    } else {
        v = exp(-r * r * 22.0) + 0.22 * exp(-r * r * 4.0);
    }
    v *= smoothstep(1.0, 0.85, r) * (mode == 2 ? 1.0 : 1.0);
    vec3 gc = vB.rgb * vB.a * v * vDim;
    if (any(isnan(gc)) || any(isinf(gc))) gc = vec3(0.0);
    frag = vec4(min(gc, vec3(64.0)), 0.0);
}
)";

// ---------------------------------------------------------------- bloom, composite
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(x, y) * uTexel).rgb;
    c /= 9.0;
    if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
    c = min(c, vec3(64.0));
    float l = max(c.r, max(c.g, c.b));
    frag = vec4(c * max(l - uThreshold, 0.0) / max(l, 1e-3), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.227, 0.194, 0.121, 0.054, 0.016);
    vec3 c = texture(uSrc, vUv).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uSrc, vUv + uDir * float(i) * 1.5).rgb * w[i];
        c += texture(uSrc, vUv - uDir * float(i) * 1.5).rgb * w[i];
    }
    frag = vec4(c, 1.0);
}
)";
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloomT, uBloom2;
uniform float uBloomK, uSat;
void main() {
    vec3 c = texture(uScene, vUv).rgb + texture(uBloomT, vUv).rgb * uBloomK + texture(uBloom2, vUv).rgb * uBloomK * 0.6;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSat), 0.0);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- motes over the board, the gauge
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
uniform float uPxPerUnit;
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    if (mode == 1) {
        // the gauge: a faint ring of segments, the charge lit from the top clockwise
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float seg = abs(fract(ang / 6.2831853 * 24.0) - 0.5);
        float gap = smoothstep(0.38, 0.32, seg);
        float ring = exp(-sq((r - 0.8) / max(px * 1.5, 0.02))) * gap;
        float on = step(ang, vC.x * 6.2831853);
        float halo = exp(-sq((r - 0.8) / 0.1)) * on * 0.3 * (0.6 + 0.4 * vC.y);
        float v = ring * (0.25 + 0.95 * on) + halo;
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    float v = exp(-r * r * 14.0) + 0.3 * exp(-r * r * 3.5);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(c * v * vB.a, 0.0);
}
)";

// ---------------------------------------------------------------- the veil: the well and the HUD's columns
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
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.12;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: panels of colored light
// Each piece one shape (its cells merge): a satin panel of its color, a line of light running just inside its edge
// like an edge-lit pane, a soft glow at its heart, a sheen. The ghost is the line of light alone; a cleared cell
// flares and shrinks into its light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, glow, drain
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uTime;
uniform vec3 uLightC;
uniform float uBridge;
uniform vec3 uBridgeInk;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.14, rc = 0.05;
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
void main() {
    int mode = int(vC.x + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    int mk = int(vA.w + 0.5);
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float erase = vC.y;
    float g = 0.045 + erase * 0.25;
    float sd = shapeSdf(vLocal, mk, g);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    vec3 hi = mix(tint, vec3(1.0), 0.55);
    float lineD = abs(e - 0.075);
    float line = (1.0 - smoothstep(0.012, 0.012 + aa * 1.5, lineD)) * inside;
    float lineGlow = exp(-lineD / 0.03) * inside;
    if (mode == 1) {
        float a = (line * 0.8 + lineGlow * 0.15 + inside * 0.05) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // satin: lit from the top left, deeper toward the heart, a glow at the heart
    float lightG = 0.5 + 0.5 * dot(normalize(vec2(-0.6, 0.8)), vLocal * 1.6);
    vec3 col = tint * mix(0.55, 0.92, lightG) * mix(1.0, 0.72, smoothstep(0.05, 0.4, e));
    col += tint * exp(-sq(max(0.5 - e, 0.0) / 0.12)) * 0.0;
    float heart = smoothstep(0.1, 0.5, e) * (0.85 + 0.15 * vnoise(vWorld * 1.1 + uTime * 0.1));
    col += mix(tint, uLightC, 0.2) * heart * 0.25 * (1.0 + vC.z);
    // the edge light
    col = mix(col, hi * 1.25, line);
    col += hi * lineGlow * 0.3 * (1.0 + vC.z);
    // a pale bevel on the outer rim
    float rim = smoothstep(0.022 + aa, 0.0, e) * inside;
    col = mix(col, tint * 0.4, rim * 0.6);
    // a slow sheen drifting over the stack
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.x * 0.45 + vWorld.y * 0.8) * 0.6 - uTime * 0.35)), 12.0);
    col += uLightC * sheen * 0.1 * smoothstep(0.0, 0.2, e);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.3, drain);
    if (erase > 0.0) col = mix(col, hi * 1.6, smoothstep(0.0, 0.5, erase));
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * vB.a * (1.0 - smoothstep(0.6, 1.0, erase));
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- lettering
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
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount) and in a glow of their ink (uP1.x),
// the field as baked then (64 px, 8 px of reach): the letter and its halo mixed in linear light, the glow added to
// the light under it (uP1.y: about the world's, linear).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uP0.a;
    float a = max(fill, halo);
    vec3 ink = pow(L.ink.rgb, vec3(2.2));
    vec3 c = ink * fill + pow(uP0.rgb, vec3(2.2)) * (a - fill);
    vec3 under = c + uP1.y * (1.0 - a);
    vec3 glow = ink * (smoothstep(0.15, 0.47, d) * uP1.x * (1.0 - fill) * 0.35);
    vec3 col = pow(c / max(a, 1e-4), vec3(1.0 / 2.2)) * a + pow(under + glow, vec3(1.0 / 2.2)) - pow(under, vec3(1.0 / 2.2));
    return vec4(col, a) * L.ink.a;
}
)";

// ---------------------------------------------------------------- post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.2 * dot(q, q) * 2.0;
    // a soft shoulder for the brightest lights
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.8);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace orbital_shaders

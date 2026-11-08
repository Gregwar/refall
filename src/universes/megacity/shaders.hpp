#pragma once
// MEGACITY's shaders: the world (the sky, the planet, its ground and the city's carpet of streets and lights in one
// ray-traced pass; the buildings, instanced; the lights in the air) at a reduced resolution with depth, a soft bloom,
// the stars at full resolution, then the veil behind the well, the glass pieces and the lettering.
// Positions are in meters, drawn relative to the eye (uVP has the eye at its origin): the city spans from a hut to
// the whole planet.

namespace megacity_shaders {

// Shared noise and the city's grid (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash1(float n) { return fract(sin(n * 127.1 + 0.37) * 43758.5453); }
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
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
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
// the same hash as mcity::hashLot on the CPU
float hashLot(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
// Points of light anchored on a surface (uv, meters), kept about a pixel wide whatever the distance (px: meters a
// pixel): two layers of cells a few pixels wide, cross-faded as the scale changes (the stars' trick), so a far city
// glitters instead of averaging into a grey wash. dens: the share of cells with a light.
float lightDots(vec2 uv, float px, float dens, float seed) {
    float L = log2(max(px, 1e-4) * 3.5);
    float l0 = floor(L);
    float sum = 0.0;
    for (int i = 0; i < 2; i++) {
        float li = l0 + float(i);
        float cell = exp2(li);
        float w = i == 0 ? 1.0 - fract(L) : fract(L);
        vec2 g = floor(uv / cell);
        vec2 hs = g + vec2(li * 7.31 + seed, li * 3.17);
        if (hash(hs) < dens) {
            vec2 o = (g + 0.2 + 0.6 * hash2(hs + 3.1)) * cell;
            vec2 dd = (uv - o) / px;
            sum += w * exp(-dot(dd, dd) * 1.2) * (0.45 + 0.8 * hash(hs + 1.7));
        }
    }
    return sum;
}
const float R_EARTH = 6.371e6;
float riverX(float z) {
    float a = abs(z);
    return 14.0 + z * 0.16 * sin(log(a + 30.0) * 2.6) + 18.0 * sin(z * 0.011) * min(1.0, a / 150.0);
}
float riverHalfW(float z) { float a = abs(z); return (7.0 + 0.022 * a) * (1.0 - smoothstep(50000.0, 80000.0, a)); }
// the organic field (the same as mcity:: on the CPU, see megacity.hpp)
uniform vec3 uSats[8];
float vnL(vec2 p) {
    vec2 i = floor(p), f = p - i;
    f = f * f * (3.0 - 2.0 * f);
    float a = hashLot(i), b = hashLot(i + vec2(1.0, 0.0)), c = hashLot(i + vec2(0.0, 1.0)), d = hashLot(i + vec2(1.0, 1.0));
    float ab = a + (b - a) * f.x, cd = c + (d - c) * f.x;
    return ab + (cd - ab) * f.y;
}
float ofbm(vec2 p) { return 0.5 * vnL(p) + 0.3 * vnL(p * 2.1 + vec2(5.3, 1.1)) + 0.2 * vnL(p * 4.3 + vec2(1.7, 9.2)); }
float roadAngle(int k, float r) {
    return float(k) * 6.2831853 / 6.0 + 0.5 * (hashLot(vec2(float(k), 3.0)) - 0.5) + 0.22 * sin(log(r + 600.0) * 2.1 + 1.7 * float(k));
}
float coastSigned(vec2 q) {
    float along = q.x * 0.96 + q.y * 0.28, across = -q.x * 0.28 + q.y * 0.96;
    return along - (16000.0 + 0.42 * abs(across) + 9000.0 * (ofbm(q / 11000.0) - 0.5) * 2.0);
}
float roadDist(vec2 q, float r) {
    float ph = atan(q.y, q.x);
    float d = 1e30;
    for (int k = 0; k < 6; k++) {
        float a = ph - roadAngle(k, r);
        a -= 6.2831853 * floor(a / 6.2831853 + 0.5);
        d = min(d, abs(a) * r);
    }
    return d;
}
float field(vec2 q) {
    float r = length(q) + 1e-3;
    float cd = abs(q.y) < 80000.0 ? max(0.0, abs(q.x - riverX(q.y)) - riverHalfW(q.y)) : 1e9;
    cd = min(cd, roadDist(q, r));
    if (r < 5.0e5) cd = min(cd, abs(coastSigned(q)));
    float c = 0.58 + 0.72 * clamp(cd / (0.1 * r + 150.0), 0.0, 1.0);
    float lr = log(r + 50.0);
    vec2 u = q / r;
    float n1 = ofbm(u * 2.6 + lr * vec2(1.1, 0.6));
    float n2 = ofbm(vec2(u.x * 8.0 - lr * 2.3 + 17.0, u.y * 8.0 + lr * 1.7));
    float n = exp((n1 - 0.5) * 1.5 + (n2 - 0.5) * 0.7);
    float D = r * c * n;
    for (int j = 0; j < 8; j++) D = min(D, uSats[j].z + length(q - uSats[j].xy) * c * n * 1.15);
    float k = clamp((lr - 12.6) / 2.4, 0.0, 1.0);
    return D + (r * (0.8 + 0.3 * n1) - D) * k * k * (3.0 - 2.0 * k);
}
)";

// The world's light, shared by the world's shaders.
inline const char* LIGHT_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uShadowC, uSunGlow, uHazeC, uSkyAmb;
uniform float uFogK, uAlt, uNightK, uBridge, uTime;
uniform vec3 uBridgeCol;
// the windows' colors by the era of the ring (old: warm lamps, modern: cool white, arcologies: the accent)
uniform vec3 uWinWarm, uWinCool, uWinAccent;
uniform float uCityR, uLit, uGlowK, uOverR, uNeon;
uniform vec4 uDrop;     // the drop's district: r0, r1, the share of its blocks lit, -
float airDensity(float h0, float h1) {
    const float H = 8500.0;
    float x0 = max(h0, 0.0) / H, x1 = max(h1, 0.0) / H;
    float dx = abs(x1 - x0);
    return dx > 1e-3 ? (exp(-min(x0, x1)) - exp(-max(x0, x1))) / dx : exp(-x0);
}
vec3 atmos(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    float od = uFogK * L * airDensity(uAlt, wp.y);
    vec3 v = d / max(L, 1e-3);
    vec3 hc = mix(uHazeC, uSunGlow, pow(max(dot(v, uSunDir), 0.0), 5.0) * 0.5 * (1.0 - uNightK * 0.7));
    col = mix(hc, col, exp(-od));
    return mix(col, uBridgeCol, uBridge);
}
vec3 eraColor(float r, float seed) {
    vec3 c = mix(uWinWarm, uWinCool, smoothstep(4000.0, 12000.0, r) * (0.55 + 0.45 * seed));
    c = mix(c, uWinAccent, smoothstep(30000.0, 90000.0, r) * smoothstep(0.45, 0.85, seed) * 0.6 + uNeon * 0.55 * smoothstep(5000.0, 20000.0, r) * step(0.75, seed));
    return c;
}
// how lit a district is: the staging, the night, the drop's blocks coming on, the game over's blackout
float districtLit(float r, float key) {
    float on = 1.0;
    if (r > uDrop.x && r < uDrop.y) {
        // the drop's district: its blocks come on one share a beat, each a little brighter as it comes on
        float since = uDrop.z - key;
        on = smoothstep(-0.02, 0.02, since) * (1.0 + 0.8 * exp(-max(since, 0.0) * 25.0));
    }
    on *= 1.0 - smoothstep(uOverR * 0.97, uOverR, r);
    return on;
}
float toonBand(float ndl) { return smoothstep(-0.05, 0.05, ndl) * 0.6 + smoothstep(0.4, 0.5, ndl) * 0.4; }
)";

// ---------------------------------------------------------------- the sky, the planet, the city's carpet
// Per pixel: the planet (the ground near the hut: grass, the river, paths and fields; the city's carpet inside its
// radius: the log-polar lots, their streets and street lights, the avenues' traffic, the lots too small to draw as
// buildings as rooftops with lit windows; farther the continents and oceans, the night side), its atmosphere (the
// dusk sky deepening to night with the growth, the city's glow on the haze, the limb from space), the sun under the
// horizon, an aurora in one variant, the moon, space. Writes the planet's depth. Alpha: where the stars may shine.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uPix, uStars, uHills, uSpaceK, uAurora, uSnow;
uniform vec3 uDuskZ, uDuskH, uNightZ, uNightH, uSpaceC, uNebulaC, uMoonC, uCityGlowC;
uniform vec3 uGrass, uGrass2, uFieldC, uLandC, uDesertC, uOceanC, uRiverC, uForestC, uStreetC, uTrafficC;
uniform vec4 uMoon;
uniform float uDrawR0, uTraffic, uTrafficPhase, uBuild;
const float R = R_EARTH;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }

// the sky's color along d (no sun): dusk to night, the glow toward the sun under the horizon, the city's dome
vec3 skyColor(vec3 d, vec3 up, float el) {
    float zk = smoothstep(-0.02, 0.8, el);
    vec3 zen = mix(uDuskZ, uNightZ, uNightK);
    vec3 hor = mix(uDuskH, uNightH, uNightK);
    vec3 sky = mix(hor, zen, pow(zk, 0.55));
    // the afterglow on the sun's side, low
    vec3 sh = normalize(uSunDir - up * dot(uSunDir, up) + vec3(1e-4, 0.0, 0.0));
    vec3 dh = normalize(d - up * dot(d, up) + vec3(0.0, 1e-4, 1e-4));
    float toward = clamp(0.5 + 0.5 * dot(sh, dh), 0.0, 1.0);
    float glowBand = exp(-max(el, 0.0) * 5.5) * pow(toward, 2.2) * (1.0 - uNightK * 0.85);
    sky += uSunGlow * glowBand * 0.9;
    // the city's glow on the haze, low all around
    sky += uCityGlowC * uGlowK * exp(-max(el, 0.0) * 16.0) * 0.3;
    return sky;
}

// The city seen from far away (lots of kilometers): no grid, organic clusters of light at every scale the pixel
// resolves, glittering with points in their cores, warm with cooler hearts.
vec3 cityFar(vec2 q, vec3 n, float r, float foot, float lit) {
    // on the sphere (no stretching far from the hut): its points in meters, and a cube map for the points of light
    vec3 P = n * R_EARTH;
    vec3 an = abs(n);
    vec2 cuv = (an.x > an.y && an.x > an.z ? n.yz / an.x + vec2(7.0 * sign(n.x), 0.0)
              : an.y > an.z ? n.xz / an.y + vec2(0.0, 7.0 * sign(n.y)) : n.xy / an.z + vec2(3.5 * sign(n.z), 3.5)) * R_EARTH;
    float v = 0.0, a = 0.5;
    float sc = max(r * 0.07, 3000.0);
    for (int i = 0; i < 6; i++) {
        float k = smoothstep(sc * 0.5, sc * 0.12, foot);
        v += (vnoise3(P / sc + float(i) * 7.13) - 0.5) * a * k;
        a *= 0.6;
        sc *= 0.38;
    }
    float field = clamp(0.5 + v * 1.5, 0.0, 1.0);
    float glow = smoothstep(0.3, 0.85, field);
    vec3 c = mix(uWinWarm, mix(uWinCool, vec3(1.0), 0.3), smoothstep(0.72, 0.95, field) * 0.7);
    c = mix(c, uWinAccent, smoothstep(0.55, 0.8, vnoise3(P / max(r * 0.03, 1.0) + 31.0)) * (0.35 + 0.45 * uNeon) * smoothstep(8.0e4, 4.0e5, r));
    c = mix(c, uWinCool, uNeon * 0.3 * smoothstep(0.4, 0.7, vnoise3(P / max(r * 0.05, 1.0) + 7.0)));
    float dots = lightDots(r < 2.0e6 ? q : cuv, foot, glow * glow * 0.5 * lit, 9.0);
    return c * (0.025 + 0.32 * glow * glow) * lit + mix(c, vec3(1.0), 0.4) * dots * 1.1;
}

// The city's carpet at q (meters, polar around the hut), seen at a pixel's size `foot`, its organic distance D:
// returns the albedo, adds the lights to `em`. The lots are square, sized by D (an octave of D a level), on grids
// turned district by district (the same as city.cpp), plazas where the districts meet.
vec3 cityGround(vec2 q, vec3 n, float r, float foot, vec3 base, float D, inout vec3 em, out float built) {
    built = 0.0;
    float edge = max(foot * 1.5, D * 0.012);
    float inside = 1.0 - smoothstep(uCityR - edge, uCityR + edge, D);
    // the meadow kept open between the early camera and the hut (city.cpp's inView): no lots there yet
    if (D < 700.0 && r < 470.0) inside *= 1.0 - smoothstep(cos(1.0) - 0.04, cos(1.0) + 0.04, dot(q / max(r, 1.0), vec2(-0.86, -0.51))) * (1.0 - smoothstep(420.0, 470.0, r)) * step(300.0, r);
    if (inside <= 0.0) return base;
    float night = 0.25 + 0.75 * uNightK;
    vec3 pave = mix(vec3(0.3, 0.26, 0.22), vec3(0.08, 0.08, 0.09), smoothstep(1500.0, 6000.0, D));
    vec3 col = base;
    float ks = 0.12 * D + 40.0;
    float lit = districtLit(D, vnL(q / ks));
    if (D < 300.0 && r < 300.0) {
        // the village: packed earth near the houses, the road lamps once it is a town
        float trod = smoothstep(0.35, 0.75, vnoise(q / 7.0)) * 0.5 + 0.5 * (1.0 - smoothstep(2.0, 4.0, abs(q.y - 2.0))) * step(q.x, 0.0)
                   + (1.0 - smoothstep(2.0, 4.0, abs(q.x - (riverX(q.y) - riverHalfW(q.y) - 14.0))));
        col = mix(base, mix(base, vec3(0.42, 0.33, 0.22), 0.6), clamp(trod - 0.45, 0.0, 1.0) * inside * smoothstep(20.0, 60.0, uCityR));
        float lampK = smoothstep(150.0, 600.0, uCityR);
        vec2 cell = floor(q / 14.0);
        vec2 o = (cell + 0.3 + 0.4 * hash2(cell)) * 14.0;
        float lamp = step(hash(cell + 5.1), 0.08) * exp(-dot(q - o, q - o) / (0.08 + foot * foot)) * min(1.0, 0.08 / (foot * foot + 1e-4));
        em += uWinWarm * lamp * lampK * night * 0.8 * inside * lit;
        built = inside;
        return col;
    }
    // the lot: its level, its district's grid
    float lv = floor(log2(max(14.0, 0.07 * D) / 14.0));
    float s = 14.0 * exp2(lv);
    float C = 12.0 * s;
    vec2 cell = floor(q / C);
    vec2 cc = (cell + 0.5) * C;
    float ang = (hashLot(cell + vec2(31.0 * lv, -17.0 * lv)) - 0.5) * 1.3;
    float ca = cos(ang), sa = sin(ang);
    vec2 dq = q - cc;
    vec2 loc = vec2(ca * dq.x + sa * dq.y, -sa * dq.x + ca * dq.y);
    vec2 lot = floor(loc / s);
    vec2 f = loc / s - lot;
    vec2 lc = (lot + 0.5) * s;
    vec2 lcw = cc + vec2(ca * lc.x - sa * lc.y, sa * lc.x + ca * lc.y);
    bool wide = s > 4000.0;
    bool valid = wide || (floor(lcw.x / C) == cell.x && floor(lcw.y / C) == cell.y && abs(lot.x + 0.5) < 6.0 && abs(lot.y + 0.5) < 6.0);
    float px = foot / s;
    vec2 lid = lot + cell * 13.0;
    float hl = hashLot(lid);
    vec3 win = eraColor(D, wide ? vnoise(q / max(r * 0.05, 1.0) + 2.0) : hash(lid + 0.37));
    // districts brighter or darker (organic clusters)
    float clusterSc = max(D * 0.12, 300.0);
    float cluster = smoothstep(0.25, 0.75, fbm(q / clusterSc + 4.0) * 0.7 + vnoise(q / (clusterSc * 0.3) + 9.0) * 0.3);
    lit *= mix(0.4, 1.2, cluster);
    float gridK = 1.0 - 0.75 * smoothstep(1.5e6, 6.0e6, r);
    // the highways: wide roads of light along their winding bearings
    float hwW = max(9.0, 0.0035 * r);
    float rd = roadDist(q, r);
    float hwy = 1.0 - smoothstep(hwW - foot, hwW + foot, rd);
    // the streets around the lot, their lamps
    float dEdge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
    float sw = min(0.055, 90.0 / s);
    float street = (1.0 - smoothstep(sw - px, sw + px, dEdge)) * min(1.0, sw / max(px, 1e-6));
    float along = (min(f.x, 1.0 - f.x) < min(f.y, 1.0 - f.y)) ? f.y : f.x;
    float lampD = abs(fract(along / 0.25) - 0.5) * 0.25;
    float lamps = exp(-(sq(lampD) + sq(dEdge - sw * 0.5)) / max(sq(0.012) + sq(px * 0.8), 1e-6)) * min(1.0, sq(0.012) / max(sq(px * 0.8), 1e-6) + 0.03);
    float detail = 1.0 - smoothstep(0.08, 0.3, px);
    float avgStreet = 2.0 * sw;
    vec3 lotCol = pave;
    vec3 lotEm = vec3(0.0);
    bool park = ofbm(vec2(lcw.x / (s * 9.0) + 31.0, lcw.y / (s * 9.0))) > 0.7 && !wide;
    if (!valid) {
        // where two districts' grids meet: a plaza, a few lamps
        lotCol = pave * 0.8;
        vec2 pc = floor(q / (s * 0.5));
        vec2 po = (pc + 0.3 + 0.4 * hash2(pc)) * s * 0.5;
        float pl = exp(-dot(q - po, q - po) / (sq(s * 0.02) + foot * foot)) * min(1.0, sq(s * 0.02) / (foot * foot + 1e-4) + 0.05);
        lotEm += uStreetC * pl * 0.8 * gridK;
        street = 0.0;
        lamps = 0.0;
    } else if (park) {
        // a park, a green finger: dark grass, a few lamps along its paths
        lotCol = uForestC * mix(0.5, 0.8, vnoise(q / (s * 0.2)));
        float path = 1.0 - smoothstep(0.02 - px, 0.02 + px, abs(f.x - f.y) * 0.7);
        lotCol = mix(lotCol, vec3(0.2, 0.18, 0.15), path * 0.6);
        lotEm += uWinWarm * path * 0.15 * detail;
    } else {
        // a lot: its buildings are drawn (the ground under them is a dark plaza), or too small to: rooftops with lit
        // windows; big lots cut by lanes into blocks, each with its lamps: a web of lights at every scale.
        float drawn = smoothstep(uDrawR0 * 0.9, uDrawR0 * 1.1, D) * (1.0 - smoothstep(1.2e6, 1.3e6, D));
        float lotLit = uLit * mix(0.2 + 1.3 * sq(hash(lid + 4.4)), 0.75 + 0.4 * hash(lid + 4.4), smoothstep(2000.0, 20000.0, s));
        vec3 roof = mix(vec3(0.035, 0.035, 0.045), vec3(0.07, 0.065, 0.07), hl);
        lotCol = mix(pave * 0.6 * (1.0 - 0.6 * smoothstep(3000.0, 6000.0, D)), roof, 1.0 - drawn);
        float lane = 0.0, laneLamp = 0.0, laneCar = 0.0;
        vec3 carCol = vec3(1.0);
        vec2 fl = f;
        float pxl = px;
        for (int k = 0; k < 3; k++) {
            if (s / pow(4.0, float(k + 1)) < 25.0) break;
            vec2 fk = fract(fl * 4.0);
            float pk = pxl * 4.0;
            float dk = min(min(fk.x, 1.0 - fk.x), min(fk.y, 1.0 - fk.y));
            float res = 1.0 - smoothstep(0.12, 0.45, pk);
            float wl = min(0.05, 40.0 * pow(4.0, float(k + 1)) / s);
            float lk = 1.0 - smoothstep(wl - pk, wl + pk, dk);
            float alongK = (min(fk.x, 1.0 - fk.x) < min(fk.y, 1.0 - fk.y)) ? fk.y : fk.x;
            float lampK = exp(-(sq(abs(fract(alongK * 2.0) - 0.5) * 0.5) + sq(dk - 0.025)) / max(sq(0.015) + sq(pk * 0.7), 1e-6));
            lane = max(lane, lk * res);
            laneLamp += mix(0.015, lampK, res) * (0.55 / float(k + 1)) * (0.4 + 0.9 * hash(floor(fl * 4.0) + lid * 3.1 + float(k)));
            if (k == 0) {
                // the first lanes are streets with traffic: headlights one way, tail lights the other
                bool alongY = min(fk.x, 1.0 - fk.x) < min(fk.y, 1.0 - fk.y);
                float acrossK = alongY ? fk.x : fk.y;
                float sideK = acrossK < 0.5 ? 1.0 : -1.0;
                float uu = (alongY ? (fl.y + lot.y) : (fl.x + lot.x)) * s / 9.0 + sideK * uTrafficPhase * 6.0 + hash(floor(fl * 4.0) + lid) * 50.0;
                float cr = smoothstep(0.62, 0.8, vnoise(vec2(uu, sideK * 3.0 + float(alongY))));
                laneCar = lk * res * cr * (1.0 - smoothstep(0.3, 0.7, pk / 0.1));
                carCol = sideK > 0.0 ? vec3(1.0, 0.95, 0.85) : uTrafficC;
            }
            fl = fk;
            pxl = pk;
        }
        float farK = smoothstep(2500.0, 6000.0, s);
        lotCol = mix(lotCol, vec3(0.03, 0.03, 0.04), lane * (1.0 - drawn * 0.5) * (1.0 - farK));
        float speck;
        {
            float wn = max(9.0, floor(s / 9.0));
            vec2 wc = floor(f * wn);
            float pw = px * wn;
            float on = step(hash(lid * 9.1 + wc), lotLit) * step(0.3, fract(f.x * wn)) * step(0.3, fract(f.y * wn));
            float planet = smoothstep(4.0e5, 4.0e6, r);
            speck = mix(on, lotLit * mix(0.04, 0.22, planet) + lightDots(q, foot, lotLit * mix(0.45, 0.55, planet), 5.0) * mix(0.9, 1.5, planet), smoothstep(0.2, 0.7, pw));
        }
        float density = smoothstep(200.0, 2000.0, D) * 0.5 + 0.5;
        lotEm += win * speck * (1.0 - lane) * (1.0 - drawn) * 0.55 * density * (1.0 - farK);
        lotEm += mix(uStreetC, win, 0.3) * laneLamp * 0.4 * gridK * (1.0 - farK);
        lotEm += carCol * laneCar * uTraffic * 1.2 * gridK * (1.0 - farK);
        lotEm += win * 0.02 * drawn;
        if (farK > 0.0) {
            lotEm += cityFar(q, n, r, foot, uLit * 1.6) * farK * (1.0 - drawn * 0.7);
            lotCol = mix(lotCol, vec3(0.03, 0.03, 0.04), farK);
        }
    }
    // street life where the pixel resolves it: trees lining the pavements, parked cars, the lamps' pools of light
    if (valid && !park && detail > 0.0) {
        float sm = dEdge * s;                       // meters from the lot's edge (the street's middle at 0)
        float am = along * s;                       // meters along the street
        float swm = sw * s;
        // parked cars along both kerbs
        float kc = 1.0 - smoothstep(0.35, 1.0, foot);
        if (kc > 0.0 && swm > 4.0) {
            float ci = floor(am / 5.2);
            float cf = fract(am / 5.2);
            float lane = step(swm - 2.6, sm) * step(sm, swm - 0.6);
            float car = lane * step(0.12, cf) * step(cf, 0.86) * step(hash(vec2(ci, sm > swm * 0.5 ? 1.0 : 2.0) + lid), 0.55);
            float ch = hash(vec2(ci, 7.0) + lid);
            vec3 carC = ch < 0.25 ? vec3(0.6, 0.05, 0.04) : ch < 0.45 ? vec3(0.85, 0.85, 0.82) : ch < 0.6 ? vec3(0.05, 0.12, 0.35) : ch < 0.8 ? vec3(0.08) : vec3(0.5, 0.45, 0.35);
            lotCol = mix(lotCol, carC, car * kc);
            street *= 1.0 - car * kc;
        }
        // trees just inside the kerb, every nine meters
        float kt = 1.0 - smoothstep(1.2, 3.0, foot);
        if (kt > 0.0) {
            float ti = floor(am / 9.0);
            vec2 tq = vec2((fract(am / 9.0) - 0.5) * 9.0, sm - swm - 2.0);
            float tree = (1.0 - smoothstep(1.6, 1.9, length(tq))) * step(0.25, hash(vec2(ti, 3.0) + lid));
            lotCol = mix(lotCol, uForestC * (0.75 + 0.35 * vnoise(q * 1.3)), tree * kt);
            lotEm *= 1.0 - tree * kt;
        }
    }
    // the streets around the lot and their lamps
    vec3 stCol = mix(lotCol, vec3(0.03, 0.03, 0.04), street * detail);
    float pw6 = 7.0 / s;
    float pool = exp(-(sq(lampD) + sq(dEdge - sw * 0.5)) / max(sq(pw6), sq(px))) * min(1.0, sq(pw6) / max(sq(px), 1e-9)) * detail;
    vec3 stEm = lotEm * (1.0 - street * detail) + (uStreetC * lamps * detail + uStreetC * pool * 0.07 + uStreetC * avgStreet * 0.09 * (1.0 - detail)) * gridK;
    // the highway over it: dark asphalt, two streams of traffic, lamps; a line of light from far away
    if (hwy > 0.0) {
        float acr = rd / hwW;   // 0 on the axis .. 1 at the edge
        float res = 1.0 - smoothstep(1.0, 4.0, foot / hwW);
        float u = r / (hwW * 5.0) - uTrafficPhase * 3.0 * (acr < 0.5 ? 1.0 : -1.0);
        float car = smoothstep(0.5, 0.85, vnoise(vec2(u, acr < 0.5 ? 3.0 : 9.0)));
        vec3 tc = acr < 0.5 ? vec3(1.0, 0.95, 0.85) : uTrafficC;
        vec3 hEm = tc * car * (0.6 + 0.8 * uTraffic) * 1.3 * res + mix(uStreetC, tc, 0.3) * (0.35 + 0.4 * uTraffic) * (1.0 - res);
        hEm += uStreetC * exp(-sq((acr - 0.95) / 0.08)) * 0.6 * res;
        stCol = mix(stCol, vec3(0.04, 0.04, 0.05), hwy);
        stEm = mix(stEm, hEm * gridK, hwy);
    }
    col = mix(base, stCol, inside);
    em += stEm * inside * night * lit;
    built = inside;
    return col;
}

// The ground's color at p (n: the planet's normal there, q: polar meters around the hut, t: the distance, foot: a
// pixel's size there).
vec3 landColor(vec3 p, vec3 n, vec2 q, float r, float t, float foot, out float water, inout vec3 em, out float built) {
    float D = field(q);
    // continents (on the sphere), the hut's region always on land
    float c0 = fbm3(n * 2.6 + 11.0);
    float c = c0 + 0.45 * (1.0 - smoothstep(1.2e6, 2.2e6, r));
    float cw = max(foot / 3.0e6, 0.004);
    water = 1.0 - smoothstep(0.47 - cw, 0.47 + cw, c);
    // the bay east of the hut (built over only once the city covers the planet)
    float bay = 0.0;
    if (r < 5.0e5) {
        float cs = coastSigned(q);
        float aa = max(foot * 1.2, 1.0);
        bay = smoothstep(-aa, aa, cs) * (1.0 - smoothstep(3.5e5, 5.0e5, r));
        water = max(water, bay);
    }
    float dry = smoothstep(0.52, 0.62, fbm3(n * 5.0 + 3.0)) * (1.0 - smoothstep(0.55, 0.8, c));
    vec3 land = mix(uLandC, uDesertC, dry);
    land *= mix(1.0, 0.7, smoothstep(0.55, 0.7, fbm3(n * 14.0 + 7.0)) * (1.0 - dry));
    // near: grass in soft patches at every scale the pixel resolves, fields and woods around the town
    float v = 0.0;
    float sc = 1.5;
    for (int i = 0; i < 7; i++) {
        float k = smoothstep(sc * 0.6, sc * 0.12, foot);
        v += (vnoise(q / sc + float(i) * 17.3) - 0.5) * k * (i < 2 ? 0.7 : 1.0);
        sc *= 6.0;
    }
    vec3 near = mix(uGrass2, uGrass, smoothstep(-0.12, 0.12, v));
    // fields: a patchwork in the ring just outside the town
    {
        float fs = max(60.0, uCityR * 0.06);
        vec2 fq = q / fs;
        vec2 fc = floor(fq + 0.3 * vec2(vnoise(fq * 0.5), vnoise(fq * 0.5 + 9.0)));
        float fh = hash(fc);
        float ring = smoothstep(uCityR * 0.9, uCityR * 1.3, D) * (1.0 - smoothstep(uCityR * 2.5, uCityR * 5.0, D)) * smoothstep(40.0, 120.0, r);
        float rows = 0.5 + 0.5 * sin(dot(q, fh < 0.5 ? vec2(1.0, 0.3) : vec2(-0.3, 1.0)) / max(fs * 0.04, foot * 1.5) * 6.28);
        vec3 fcol = mix(uFieldC, mix(uGrass, uFieldC, 0.5), step(0.5, fract(fh * 7.0)));
        fcol *= 0.9 + 0.12 * rows * smoothstep(fs * 0.04, fs * 0.01, foot);
        near = mix(near, fcol, ring * step(0.25, fh) * smoothstep(fs * 0.5, fs * 0.05, foot));
        // woods
        float wood = smoothstep(0.58, 0.62, fbm(q / max(400.0, uCityR * 0.3) + 3.0)) * smoothstep(60.0, 200.0, r);
        near = mix(near, uForestC * (0.8 + 0.3 * vnoise(q / 6.0)), wood * 0.9);
    }
    float kNear = 1.0 - smoothstep(2.0e4, 2.0e5, t);
    land = mix(land * mix(0.9, 1.1, smoothstep(-0.1, 0.1, v)), near, kNear);
    // snow on the poles and the far mountains; all over in the snowy variant
    float snow = smoothstep(0.82, 0.9, abs(n.z) + 0.1 * fbm3(n * 9.0)) + smoothstep(0.68, 0.72, c0) * 0.8;
    land = mix(land, vec3(0.92, 0.95, 1.0), clamp(snow, 0.0, 1.0) * (1.0 - kNear));
    land = mix(land, near * (0.9 + 0.1 * smoothstep(-0.1, 0.1, v)), uSnow * (1.0 - water) * (0.7 + 0.3 * kNear));
    // the river: dark water between sandy banks
    float riv = 0.0;
    if (abs(q.y) < 80000.0) {
        float hw = riverHalfW(q.y);
        float dx = abs(q.x - riverX(q.y));
        float aa = foot * 1.2;
        riv = 1.0 - smoothstep(hw - aa, hw + aa, dx);
        float bank = (1.0 - smoothstep(hw + 0.8 - aa, hw + 0.8 + 0.06 * hw + aa, dx)) * (1.0 - riv);
        land = mix(land, vec3(0.62, 0.52, 0.36), bank * 0.7);
    }
    vec3 ocean = mix(uOceanC, uOceanC * vec3(0.55, 0.75, 1.0), smoothstep(0.47, 0.3, c));
    // the city over it all
    float b;
    vec3 col = cityGround(q, n, r, foot, land, D, em, b);
    built = b;
    water = max(water * (1.0 - b * smoothstep(4.0e6, 1.2e7, uCityR)), riv);
    if (water > 0.0 && riv > 0.0) ocean = uRiverC;
    // the city on the oceans at the end: floating platforms (dimmer)
    em *= mix(1.0, 0.45, water * (1.0 - riv));
    return mix(col, ocean, water);
}

void main() {
    vec3 d = rayDir(vUv);
    vec3 oc = vec3(uEye.x, uEye.y + R, uEye.z);
    float c0 = dot(uEye.xz, uEye.xz) + uEye.y * (uEye.y + 2.0 * R);
    float alt = c0 / (sqrt(c0 + R * R) + R);
    vec3 up = normalize(oc);
    float b = dot(d, oc);
    float disc = b * b - c0;
    vec3 col;
    float stars = 0.0;
    float depth = 1.0;
    float sd = dot(d, uSunDir);
    float dip = atan(sqrt(alt * (2.0 * R + alt)), R);
    if (disc > 0.0 && b < 0.0) {
        // ---- the planet
        float t = c0 / (-b + sqrt(disc));
        vec3 p = uEye + d * t;
        vec3 n = normalize(oc + d * t);
        float foot = t * uPix / max(abs(dot(d, n)), 0.08);
        // polar coordinates around the hut (meters along the ground)
        float hl = length(p.xz);
        float th = atan(hl, R + p.y);
        vec2 q = p.xz * (hl > 1e-3 ? th * R / hl : 1.0);
        float r = th * R;
        float water;
        vec3 em = vec3(0.0);
        float built;
        vec3 alb = landColor(p, n, q, r, t, foot, water, em, built);
        float ndl = dot(n, uSunDir);
        float lit = smoothstep(-0.12, 0.1, ndl);
        // the light: the low sun's warm band, the sky's light, the moon's at night
        vec3 light = uSkyAmb * (0.55 + 0.45 * n.y) + uSunC * toonBand(ndl) * (1.0 - uNightK * 0.9);
        light = mix(light, uShadowC * 0.6, smoothstep(1e4, 4e5, alt) * (1.0 - lit));
        col = alb * light;
        // the water: the sky mirrored, the lights shimmering in it
        if (water > 0.0) {
            vec3 rd = reflect(d, n);
            float rel = asin(clamp(dot(rd, n), -1.0, 1.0));
            vec3 refl = skyColor(rd, n, rel) * 0.75;
            refl += uSunGlow * pow(max(dot(rd, uSunDir), 0.0), 30.0) * 0.6 * (1.0 - uNightK * 0.8);
            float fres = 0.3 + 0.7 * pow(1.0 - max(dot(-d, n), 0.0), 3.0);
            vec3 wc = mix(alb * light, refl, fres * (1.0 - smoothstep(2e4, 2e5, t)) + 0.15);
            // the city's lights in the water, broken by the ripples
            float rip = vnoise(vec2(q.x * 0.5, q.y * 0.08 + uTime * 0.6)) * vnoise(vec2(q.x * 0.13 + uTime * 0.2, q.y * 0.5));
            wc += uWinWarm * built * smoothstep(0.25, 0.6, rip) * 0.5 * uNightK * (1.0 - smoothstep(1e3, 3e4, t));
            col = mix(col, wc, water);
        }
        // the night side from space
        float nightSide = 1.0 - lit;
        col = mix(col, alb * uShadowC * 0.12, nightSide * smoothstep(1e4, 2e5, alt));
        // the lights: the city, the scattered villages beyond it
        float Dg = field(q);
        float spread = smoothstep(uCityR, uCityR * 1.05, Dg) * (1.0 - smoothstep(uCityR * 1.2, uCityR * 2.2, Dg)) * (1.0 - water);
        if (spread > 0.0) {
            float cs = max(uCityR * 0.02, 30.0);
            vec2 cell = floor(q / cs);
            vec2 o = (cell + 0.25 + 0.5 * hash2(cell)) * cs;
            float vil = step(hash(cell + 2.3), 0.12) * exp(-dot(q - o, q - o) / sq(cs * 0.04 + foot * 0.5)) * min(1.0, sq(cs * 0.04) / sq(foot * 0.5 + 1e-3) + 0.1);
            em += uWinWarm * vil * spread * 0.7 * (0.3 + 0.7 * uNightK);
        }
        // the construction frontier: a faint warm line of work lights at the city's edge
        em += uWinWarm * uBuild * exp(-sq((Dg - uCityR) / max(uCityR * 0.012, foot * 1.5))) * 0.6 * (1.0 - water);
        col += em * mix(1.0, 1.3, smoothstep(1e5, 1e6, alt));
        // the aerial perspective, the limb's glow
        float od = 1.0e-5 * t * airDensity(alt, 0.0);
        vec3 hc = skyColor(normalize(d + up * 0.1), up, 0.1) * 0.8;
        col = mix(hc * mix(1.0, 0.25, nightSide * smoothstep(1e4, 3e5, alt)), col, exp(-od * mix(1.0, 0.6, uNightK)));
        vec4 clip = uVP * vec4(p - uEye, 1.0);
        depth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    } else {
        // ---- the sky: the atmosphere along the ray, space beyond
        float hc = b < 0.0 ? -disc / (sqrt(R * R - disc) + R) : alt;
        float air = exp(-hc / 18000.0);
        float el = asin(clamp(dot(d, up), -1.0, 1.0)) + dip;
        vec3 sky = skyColor(d, up, el);
        sky = mix(sky, sky * vec3(0.35, 0.4, 0.6), smoothstep(3000.0, 40000.0, alt));
        float a = clamp(pow(air, 0.55) * 1.15, 0.0, 1.0);
        vec3 sp = uSpaceC;
        float nb = fbm3(d * 2.2 + 4.0);
        float neb = smoothstep(0.45, 0.8, nb) * (0.35 + 0.65 * smoothstep(0.3, 0.9, fbm3(d * 5.0 + 1.0)));
        sp += uNebulaC * neb * 0.12;
        col = mix(sp, sky, a);
        // the limb, glowing (seen from high up): the sun's side bright, the night side's city glow
        float limb = exp(-hc / 6000.0) * smoothstep(3000.0, 60000.0, alt) * step(0.0, -b);
        float sunSide = smoothstep(-0.3, 0.4, dot(normalize(oc + d * max(-b, 0.0)), uSunDir));
        col += mix(uNightH * 1.6 + uCityGlowC * uGlowK * 0.6, uDuskH * 1.4, sunSide) * limb * 0.8;
        // the aurora: curtains of light, slowly folding (night only)
        if (uAurora > 0.0 && el > 0.0) {
            float az = atan(d.x, -d.z);
            float h = el;
            float curtain = 0.0;
            for (int i = 0; i < 2; i++) {
                float fi = float(i);
                float base = 0.18 + 0.1 * fi + 0.06 * sin(az * 2.0 + uTime * 0.03 + fi * 2.0) + 0.04 * fbm(vec2(az * 3.0 + fi * 7.0, uTime * 0.02));
                float rays = 0.55 + 0.45 * vnoise(vec2(az * 60.0 + fi * 13.0, uTime * 0.08));
                float band = smoothstep(base - 0.02, base + 0.02, h) * exp(-max(h - base, 0.0) * (5.0 - fi * 1.5));
                curtain += band * rays * smoothstep(0.3, 0.7, fbm(vec2(az * 1.7 + fi * 3.0 + uTime * 0.01, 2.0)));
            }
            vec3 ac = mix(vec3(0.15, 1.0, 0.6), vec3(0.75, 0.3, 1.0), smoothstep(0.25, 0.5, h));
            col += ac * curtain * uAurora * uNightK * a * 0.55;
        }
        stars += (uStars * uNightK + (1.0 - a)) * (1.0 - a * 0.85) * (1.0 - 0.6 * uGlowK * exp(-max(el, 0.0) * 4.0));
        // far hills along the horizon (seen from near the ground)
        if (uHills > 0.0) {
            float az = atan(d.x, -d.z);
            float h1 = 0.01 + 0.02 * fbm(vec2(az * 2.6, 1.0)) + 0.01 * fbm(vec2(az * 9.0, 7.0));
            float h2 = 0.004 + 0.012 * fbm(vec2(az * 4.3 + 9.0, 3.0)) + 0.005 * fbm(vec2(az * 17.0, 2.0));
            float aa = uPix * 1.2;
            float m1 = smoothstep(h1 + aa, h1 - aa, el) * uHills, m2 = smoothstep(h2 + aa, h2 - aa, el) * uHills;
            vec3 hz = skyColor(d, up, 0.0);
            vec3 far = mix(hz, uGrass2 * 0.35 + uNightZ * 0.4, 0.45);
            vec3 nearH = mix(hz, uGrass2 * 0.25 + uNightZ * 0.3, 0.7);
            col = mix(col, far, m1);
            col = mix(col, nearH, m2);
            stars *= 1.0 - max(m1, m2);
        }
    }
    // the sun: under the horizon at first a sliver of its glow; from space a disc behind the limb
    {
        float ang = acos(clamp(sd, -1.0, 1.0));
        float disc = smoothstep(0.034, 0.03, ang) * (depth < 1.0 ? 0.0 : 1.0);
        float glow = pow(max(sd, 0.0), 10.0) * 0.3 + exp(-ang / 0.09) * 0.5;
        col += uSunGlow * glow * (depth < 1.0 ? 0.2 : 1.0) * (1.0 - uNightK * 0.5);
        col = mix(col, uSunC * 2.0 + 0.3, disc);
        stars *= 1.0 - clamp(glow * 2.0 + disc, 0.0, 1.0);
    }
    // the moon
    if (depth >= 1.0) {
        vec3 mo = uEye - uMoon.xyz;
        float mb = dot(d, mo);
        float md = mb * mb - (dot(mo, mo) - uMoon.w * uMoon.w);
        if (md > 0.0 && mb < 0.0) {
            float t = -mb - sqrt(md);
            vec3 mn = normalize(mo + d * t);
            float cr = fbm3(mn * 3.0 + 2.0);
            vec3 alb = mix(vec3(1.0), vec3(0.72, 0.72, 0.82), smoothstep(0.5, 0.62, cr));
            float ml = smoothstep(-0.05, 0.05, dot(mn, uSunDir));
            vec3 mc = alb * uMoonC * mix(0.06, 1.25, ml);
            float edge = smoothstep(0.0, 0.1, -dot(mn, d));
            col = mix(col, mc + col * 0.3, edge);
            stars = 0.0;
        }
    }
    col = mix(col, uBridgeCol, uBridge);
    gl_FragDepth = depth;
    frag = vec4(col, clamp(stars, 0.0, 1.0) * (1.0 - uBridge) * (depth < 1.0 ? 0.0 : 1.0));
}
)";

// ---------------------------------------------------------------- the buildings
// One instanced draw: a box with a gable roof (flat when its height is 0), tapered toward the top, standing on the
// sphere. It rises out of the ground as the city's edge passes its birth radius. Its faces carry the windows of its
// era (lit ones glow), its walls the low sun's warm band, the sky's light, the city's glow from below.
inline const char* BLD_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;   // x, z in [-1, 1], y: 0..1 the body, 1..2 the roof
layout(location = 1) in vec3 aNrm;   // the face's direction (local, before the taper)
layout(location = 2) in vec4 i0;     // base (world), birth radius
layout(location = 3) in vec4 i1;     // x axis, half width
layout(location = 4) in vec4 i2;     // half depth, height, roof height, taper
layout(location = 5) in vec4 i3;     // kind, seed, light key, base height
layout(location = 6) in vec4 i4;     // death, twist, -, -
uniform mat4 uVP;
uniform float uMinPx;
out vec3 vW;
out vec2 vUV;          // along the face (m), up the building (m)
flat out vec4 vK;      // kind, seed, key, birth
flat out vec4 vS;      // half width, half depth, height (whole, with its base), face (0 side x, 1 side z, 2 roof)
out float vRoofY;
void main() {
    float birth = i0.w;
    float k = smoothstep(birth, birth * 1.06 + 1.5, uCityR) * (1.0 - smoothstep(i4.x, i4.x * 1.08, uCityR));
    vec3 base = i0.xyz;
    float dist = length(base - uEye);
    float size = max(i1.w, i2.x) * 2.0 + i2.y * 0.3;
    if (k <= 0.001 || size < uMinPx * dist * 0.35) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    // rising: an ease with a slight overshoot as it tops out
    float rise = k + 0.06 * sin(k * 3.14159) * k;
    vec3 n = normalize(base + vec3(0.0, R_EARTH, 0.0));
    vec3 ax = i1.xyz;
    vec3 az = normalize(cross(ax, n));
    float hw = i1.w, hd = i2.x, h = i2.y, roofH = i2.z, taper = i2.w, y0 = i3.w;
    bool roof = aPos.y > 1.0001;
    float yl = min(aPos.y, 1.0);
    float tw = 1.0 - taper * yl;
    float x = aPos.x * hw * tw, z = aPos.z * hd * tw;
    if (i4.y != 0.0) {
        // a twisted tower: each floor turned a little more
        float a = i4.y * yl, c = cos(a), sn = sin(a);
        float x2 = c * x - sn * z;
        z = sn * x + c * z;
        x = x2;
    }
    float y = roof ? y0 + h + (aPos.y - 1.0) * roofH * tw : y0 + aPos.y * h;
    if (!roof && aPos.y < 0.001 && y0 < 0.001) y = -max(0.4, 0.04 * max(hw, hd));
    y *= rise;
    vec3 w = base + ax * x + n * y + az * z;
    vW = w - uEye;
    gl_Position = uVP * vec4(vW, 1.0);
    float face = roof ? 2.0 : (abs(aNrm.x) > 0.5 ? 0.0 : 1.0);
    vUV = vec2(face < 0.5 ? aPos.z * hd * tw : aPos.x * hw * tw, roof ? y0 + h : y0 + aPos.y * h);
    vRoofY = roof ? aPos.y - 1.0 : -1.0;
    vK = vec4(i3.x, i3.y, i3.z, birth);
    vS = vec4(hw, hd, y0 + h, face);
}
)";
inline const char* BLD_FS = R"(#version 330 core
in vec3 vW;
in vec2 vUV;
flat in vec4 vK;
flat in vec4 vS;
in float vRoofY;
out vec4 frag;
uniform vec3 uWallA, uWallB, uRoofA, uRoofB, uStone, uGlassC, uArcoC, uCityGlowC, uStreetC;
uniform float uBeat;
uniform vec3 uMoonDir, uMoonL;
// A line at every integer of x, w wide (in x's units), faded out once finer than a pixel.
float gridLine(float x, float w) {
    float d = abs(fract(x + 0.5) - 0.5);
    float fw = fwidth(x);
    return (1.0 - smoothstep(w - fw, w + fw, d)) * (1.0 - smoothstep(0.25, 0.5, fw));
}
// The windows: a grid of cells (cw wide, ch high), each lit or not, their light given its room's tint (warm lamps,
// cool white, an orange shade, now and then a television's soft blue), curtains half drawn, a mullion cross; averaged
// once smaller than a pixel, then glittering points from afar. frame: the window's opening (for the walls around).
vec3 windowsC(vec2 uv, float cw, float ch, float ww, float wh, float seed, float frac, vec3 win, float mull, out float frame) {
    vec2 c = uv / vec2(cw, ch);
    vec2 cell = floor(c);
    vec2 f = fract(c) - 0.5;
    vec2 fw = fwidth(c);
    float inside = (1.0 - smoothstep(ww * 0.5 - fw.x, ww * 0.5 + fw.x, abs(f.x))) * (1.0 - smoothstep(wh * 0.5 - fw.y, wh * 0.5 + fw.y, abs(f.y)));
    // lights switch on and off now and then; whole groups of floors (offices, flats) lit or dark together
    vec2 grp = floor(cell / vec2(5.0, 3.0));
    float gk = mix(hash(grp * 0.71 + seed * 13.0), hash(vec2(cell.y * 0.37, seed * 7.0)), 0.55);
    float gFrac = clamp(frac * (0.5 + 0.9 * gk), 0.0, 1.0);
    float slot = floor(uTime / 23.0 + hash(cell + seed * 17.0) * 4.0);
    float on = step(hash(cell * 1.37 + seed * 31.0 + slot * 0.123), gFrac);
    float avg = ww * wh;
    float blur = smoothstep(0.35, 0.8, max(fw.x, fw.y));
    frame = mix(inside, avg, blur);
    // the room behind: its tint, curtains, the ceiling light brighter at the top, the mullions
    vec2 q = f / vec2(ww, wh);
    vec2 fq = fw / vec2(ww, wh);
    float h1 = hash(cell * 3.7 + seed * 5.1);
    vec3 tint = h1 < 0.55 ? win : h1 < 0.8 ? mix(win, vec3(1.0, 0.96, 0.9), 0.6) : h1 < 0.94 ? win * vec3(1.05, 0.72, 0.45)
              : vec3(0.42, 0.58, 1.0) * (0.75 + 0.25 * sin(uTime * 1.1 + h1 * 40.0));
    float curtain = step(0.55, hash(cell * 1.9 + seed)) * smoothstep(0.22 - fq.x, 0.3 + fq.x, abs(q.x)) * 0.65;
    float grad = 0.8 + 0.35 * q.y;
    float mu = mull * clamp((1.0 - smoothstep(0.035 - fq.x, 0.035 + fq.x, abs(q.x))) + (1.0 - smoothstep(0.035 - fq.y, 0.035 + fq.y, abs(q.y - 0.18))), 0.0, 1.0);
    vec3 nearC = tint * (inside * on * grad * (1.0 - curtain) * (1.0 - mu * 0.85));
    vec3 midC = win * (avg * gFrac * 0.3);
    vec3 farC = win * (avg * frac * 0.03 + lightDots(uv, max(fw.x * cw, fw.y * ch), frac * 0.55, seed * 91.0) * 0.9);
    return mix(nearC, mix(midC, farC, blur), blur);
}
float windows(vec2 uv, float cw, float ch, float ww, float wh, float seed, float frac, out float frame) {
    return windowsC(uv, cw, ch, ww, wh, seed, frac, vec3(1.0), 0.0, frame).r;
}
// a disc of radius rd at c (uv meters), antialiased
float disc(vec2 uv, vec2 c, float rd) { float d = length(uv - c); float fw = fwidth(d); return 1.0 - smoothstep(rd - fw, rd + fw, d); }
void main() {
    int kind = int(vK.x + 0.5);
    float seed = vK.y;
    vec3 Nf = normalize(cross(dFdx(vW), dFdy(vW)));
    if (dot(Nf, vW) > 0.0) Nf = -Nf;
    vec3 wp = vW + uEye;
    vec3 up = normalize(wp + vec3(0.0, R_EARTH, 0.0));
    float ndl = dot(Nf, uSunDir);
    float nUp = dot(Nf, up);
    bool roofFace = vS.w > 1.5 || nUp > 0.7;
    float lit = districtLit(vK.w, vK.z);
    float night = 0.2 + 0.8 * uNightK;
    vec3 win = eraColor(vK.w, seed);
    vec3 alb;
    vec3 em = vec3(0.0);
    float gloss = 0.0;
    float frame;
    float hy = vUV.y;
    if (kind == 0) {
        // the hut: planks, a thatched roof, a small window with a cross and a door glowing with the fire inside
        alb = roofFace ? uRoofB * 1.1 + vec3(0.12, 0.08, 0.02) : uWallB * (0.8 + 0.2 * step(0.5, fract(hy * 3.2)));
        if (!roofFace) {
            float side = vS.w < 0.5 ? 1.0 : 0.0;
            vec2 wq = vec2(vUV.x - (side > 0.5 ? 0.0 : 0.35), hy - 1.3);
            float wnd = (1.0 - smoothstep(0.3, 0.33, abs(wq.x))) * (1.0 - smoothstep(0.26, 0.29, abs(wq.y)));
            float mull = (1.0 - smoothstep(0.02, 0.04, abs(wq.x))) + (1.0 - smoothstep(0.02, 0.04, abs(wq.y)));
            float frameW = wnd * (1.0 - (1.0 - smoothstep(0.24, 0.27, abs(wq.x))) * (1.0 - smoothstep(0.2, 0.23, abs(wq.y))));
            wnd *= 1.0 - clamp(mull, 0.0, 1.0) * 0.85;
            wnd *= 1.0 - frameW;
            vec2 dq = vec2(vUV.x + 0.9, hy - 0.85);
            float door = (1.0 - side) * (1.0 - smoothstep(0.4, 0.43, abs(dq.x))) * (1.0 - smoothstep(0.85, 0.88, abs(dq.y)));
            float flick = 0.85 + 0.15 * vnoise(vec2(uTime * 3.0, 1.0));
            em += uWinWarm * (wnd * 2.6 + door * 1.2 * (0.6 + 0.4 * smoothstep(0.85, -0.6, dq.y))) * flick;
            alb = mix(alb, uRoofB * 0.35, frameW + door * 0.3);
            alb *= 1.0 - wnd * 0.6;
            // the log ends at the corners
            float corner = smoothstep(0.88, 0.95, abs(vUV.x) / (vS.w < 0.5 ? vS.y : vS.x));
            alb = mix(alb, uWallB * 1.15, corner * step(0.5, fract(hy * 3.2 + 0.25)));
        } else {
            // thatch: bundles in rows, ragged
            float rows = gridLine(vRoofY * 7.0 + vnoise(vec2(vUV.x * 3.0, 0.0)) * 0.3, 0.12);
            alb *= (0.8 + 0.25 * vnoise(vec2(vUV.x * 6.0, hy * 20.0 + vRoofY * 30.0))) * (1.0 - rows * 0.3);
        }
    } else if (kind == 1 || kind == 2) {
        // houses: plaster walls with a plinth, timber framing, shutters, a door, tiled roofs, warm windows
        vec3 wall = mix(uWallA, uWallB, step(0.5, seed)) * (0.92 + 0.12 * fract(seed * 23.0));
        float rs = fract(seed * 7.0);
        vec3 roofC = rs < 0.6 ? mix(uRoofA, uRoofB, rs / 0.6 * 0.6) : rs < 0.85 ? mix(uStone * 0.5, vec3(0.1, 0.13, 0.2), 0.45) : uRoofB * 1.2;
        alb = roofFace ? roofC : wall;
        if (!roofFace) {
            alb *= 1.0 - 0.25 * (1.0 - smoothstep(0.45, 0.55, hy));   // the plinth
            if (kind == 2) {
                // half-timbering: posts, rails, braces
                float u = vUV.x / 1.2;
                float beam = gridLine(hy / 3.0, 0.05) + gridLine(u * 0.5, 0.06) + gridLine((vUV.x + hy * 0.8) / 2.4, 0.04) * step(0.5, fract(floor(u * 0.5) * 0.5));
                alb = mix(alb, uRoofB * 0.38, clamp(beam, 0.0, 1.0) * 0.75 * step(0.45, fract(seed * 3.0)) * step(0.5, hy));
            }
            vec2 wuv = vUV + vec2(0.0, -0.4);
            vec3 wv = windowsC(wuv, 2.4, 3.0, 0.38, 0.45, seed, uLit * (kind == 1 ? 0.75 : 0.85), win, 1.0, frame);
            // shutters on either side of the windows, painted
            vec2 cq = fract(wuv / vec2(2.4, 3.0)) - 0.5;
            float sh = step(0.2, abs(cq.x)) * step(abs(cq.x), 0.29) * step(abs(cq.y), 0.23) * step(0.35, fract(seed * 11.0)) * step(1.0, wuv.y);
            vec3 shC = fract(seed * 17.0) < 0.5 ? vec3(0.12, 0.3, 0.2) : fract(seed * 17.0) < 0.75 ? vec3(0.15, 0.22, 0.4) : uRoofA * 0.6;
            alb = mix(alb, shC, sh * (1.0 - smoothstep(0.3, 0.6, fwidth(cq.x) * 4.0)));
            // a door in the middle of the front, its fanlight warm
            float door = (1.0 - smoothstep(0.55, 0.6, abs(vUV.x))) * (1.0 - smoothstep(2.1, 2.15, hy)) * step(vS.w, 0.5);
            alb = mix(alb, uRoofB * 0.45, door);
            em += uWinWarm * door * smoothstep(1.75, 1.85, hy) * 1.2 * lit * step(0.4, fract(seed * 5.0));
            wv *= 1.0 - door;
            em += wv * 1.8 * lit;
            alb *= 1.0 - frame * 0.5;
        } else {
            // tiles in rows, staggered joints, a darker ridge
            float tilesY = vRoofY * max(vS.x, 2.0) / 0.32;
            float row = gridLine(tilesY, 0.08);
            float joint = gridLine(vUV.x / 0.3 + 0.5 * floor(tilesY), 0.06);
            float ridge = smoothstep(0.93, 0.97, vRoofY);
            alb *= (1.0 - 0.28 * row - 0.12 * joint * (1.0 - row)) * (0.9 + 0.2 * hash(floor(vec2(vUV.x / 0.3, tilesY)))) * (1.0 - 0.4 * ridge);
        }
    } else if (kind == 3 || kind == 4) {
        // stone: the church (tall windows of colored glass, a rose window, a clock, the belfry), the walls (torches)
        float ash = gridLine(hy / 0.6, 0.04) + gridLine(vUV.x / 1.2 + 0.5 * floor(hy / 0.6), 0.03);
        alb = roofFace && kind == 3 ? uRoofB * 0.7 : uStone * (0.85 + 0.15 * vnoise(vUV * vec2(0.6, 1.4))) * (1.0 - 0.12 * clamp(ash, 0.0, 1.0));
        if (!roofFace && kind == 3) {
            vec3 stained = mix(mix(vec3(1.0, 0.3, 0.3), vec3(0.3, 0.5, 1.0), vnoise(vUV * 2.0)), vec3(1.0, 0.8, 0.3), vnoise(vUV * 3.0 + 4.0));
            if (seed < 0.5) {
                // the nave: tall lancets, and a rose window high on the gable ends
                float wv = windows(vUV + vec2(0.0, -2.0), 3.0, 7.0, 0.3, 0.6, seed, 0.9, frame);
                em += mix(win, stained, 0.6) * wv * 1.6 * lit * step(hy, vS.z - 1.0);
                if (vS.w < 0.5) {
                    vec2 rc = vec2(0.0, vS.z * 0.72);
                    float rr = length(vUV - rc), ra = atan(vUV.y - rc.y, vUV.x);
                    float rose = disc(vUV, rc, 1.7) * (0.55 + 0.45 * step(0.5, fract(ra * 12.0 / 6.2831853)) * step(0.3, rr));
                    em += stained * rose * 1.8 * lit;
                    alb *= 1.0 - disc(vUV, rc, 1.95) * 0.4;
                }
            } else if (seed < 0.65) {
                // the bell tower: a clock face (no numbers: twelve ticks and its hands), the belfry's openings
                vec2 cc = vec2(0.0, vS.z * 0.72);
                vec2 d = vUV - cc;
                float rr = length(d);
                float face = disc(vUV, cc, 1.25);
                float ang = atan(d.x, d.y);
                float ticks = step(0.94, abs(cos(ang * 6.0))) * smoothstep(0.85, 0.95, rr) * face;
                float hA = uTime * 0.01, mA = uTime * 0.12;
                vec2 hd = vec2(sin(hA), cos(hA)), md = vec2(sin(mA), cos(mA));
                float hand = (1.0 - smoothstep(0.05, 0.09, abs(d.x * hd.y - d.y * hd.x))) * step(0.0, dot(d, hd)) * step(rr, 0.65)
                           + (1.0 - smoothstep(0.04, 0.07, abs(d.x * md.y - d.y * md.x))) * step(0.0, dot(d, md)) * step(rr, 1.0);
                alb = mix(alb, vec3(0.92, 0.88, 0.78), face);
                alb = mix(alb, vec3(0.08), clamp(ticks + hand, 0.0, 1.0) * face);
                em += uWinWarm * face * (1.0 - clamp(ticks + hand, 0.0, 1.0)) * 0.35 * lit;
                float bel = (1.0 - smoothstep(0.5, 0.6, abs(vUV.x))) * smoothstep(vS.z * 0.84, vS.z * 0.85, hy) * (1.0 - smoothstep(vS.z * 0.95, vS.z * 0.96, hy));
                alb *= 1.0 - bel * 0.85;
                em += uWinWarm * bel * 0.4 * lit;
            }
        }
        if (!roofFace && kind == 4) {
            float torch = exp(-(sq(fract(vUV.x / 9.0) - 0.5) * 80.0 + sq(hy - vS.z * 0.75) * 0.8));
            em += uWinWarm * torch * 1.5 * lit * step(vS.z, 20.0);
            float crenel = step(0.5, fract(vUV.x / 1.6)) * smoothstep(vS.z - 1.0, vS.z - 0.9, hy);
            alb *= 1.0 - crenel * 0.5;
            // arrow slits on the towers
            alb *= 1.0 - (1.0 - smoothstep(0.08, 0.12, abs(vUV.x))) * step(abs(hy - vS.z * 0.5), 1.0) * step(vS.x, 5.0) * 0.8;
        }
    } else if (kind == 5) {
        // stone blocks: pilasters, string courses, a cornice, tall windows with balconies, shopfronts and awnings on
        // the ground floor, lit dormers in the mansard
        vec3 wallC = mix(uStone, uWallA, 0.4 + 0.3 * seed);
        alb = roofFace ? uRoofA * 0.55 : wallC;
        if (seed > 0.94) {
            alb = uGlassC * 1.5 + 0.1; gloss = 0.6;   // a dome
            alb *= 1.0 - 0.3 * gridLine(vUV.x / 2.5, 0.05);
        } else if (!roofFace) {
            float pil = gridLine(vUV.x / 5.6 + 0.5, 0.05);
            float course = gridLine(hy / 3.6, 0.03) * step(4.6, hy);
            float cornice = smoothstep(vS.z - 1.4, vS.z - 1.2, hy);
            float under = smoothstep(vS.z - 1.9, vS.z - 1.7, hy) * (1.0 - cornice);
            alb *= 1.0 + 0.12 * pil + 0.18 * cornice - 0.35 * under - 0.2 * course;
            if (hy > 4.6) {
                vec2 wuv = vUV + vec2(0.0, -1.0);
                vec3 wv = windowsC(wuv, 2.8, 3.6, 0.42, 0.55, seed, uLit * 0.8, win, 1.0, frame);
                em += wv * 1.6 * lit;
                alb *= 1.0 - frame * 0.45;
                // balconies: a ledge and its railing under the windows, on some buildings
                vec2 cq = fract(wuv / vec2(2.8, 3.6)) - 0.5;
                float bal = step(0.5, fract(seed * 9.0)) * step(abs(cq.x), 0.32) * (1.0 - smoothstep(0.02, 0.04, abs(cq.y + 0.3))) * (1.0 - smoothstep(0.3, 0.6, fwidth(cq.y) * 6.0));
                alb = mix(alb, vec3(0.05), bal * 0.8);
            } else {
                // the shopfront: big windows warm with light, an awning, a glowing sign band above (no lettering)
                float sx = fract(vUV.x / 4.2) - 0.5;
                float sid = floor(vUV.x / 4.2);
                float glassF = (1.0 - smoothstep(0.38, 0.42, abs(sx))) * step(0.3, hy) * (1.0 - smoothstep(3.0, 3.1, hy));
                float shopOn = step(0.25, hash(vec2(sid, seed * 7.0)));
                vec3 shopC = mix(uWinWarm, vec3(1.0, 0.95, 0.85), hash(vec2(sid, 3.0 + seed)));
                em += shopC * glassF * shopOn * 1.4 * lit * (0.85 + 0.15 * smoothstep(3.0, 0.3, hy));
                alb *= 1.0 - glassF * 0.7;
                float aw = step(3.1, hy) * (1.0 - step(3.7, hy)) * (1.0 - smoothstep(0.42, 0.46, abs(sx)));
                float ah = hash(vec2(sid, 11.0 + seed));
                vec3 awC = ah < 0.3 ? vec3(0.6, 0.1, 0.1) : ah < 0.55 ? vec3(0.1, 0.35, 0.2) : ah < 0.8 ? vec3(0.12, 0.2, 0.45) : vec3(0.75, 0.7, 0.6);
                awC *= 0.8 + 0.25 * step(0.5, fract(vUV.x / 0.5));
                alb = mix(alb, awC, aw);
                float sign = step(3.8, hy) * (1.0 - step(4.3, hy)) * (1.0 - smoothstep(0.3, 0.34, abs(sx))) * step(0.45, ah);
                vec3 sc = ah < 0.6 ? uWinWarm : ah < 0.8 ? vec3(1.0, 0.85, 0.7) : uWinAccent;
                em += sc * sign * 0.9 * lit;
            }
        } else {
            // the mansard: slate, a row of lit dormers
            float slate = gridLine(vRoofY * 8.0, 0.08);
            alb *= 1.0 - 0.25 * slate;
            vec2 dq = vec2(fract(vUV.x / 2.8) - 0.5, vRoofY - 0.4);
            float dorm = (1.0 - smoothstep(0.14, 0.17, abs(dq.x))) * (1.0 - smoothstep(0.16, 0.2, abs(dq.y))) * (1.0 - smoothstep(0.3, 0.6, fwidth(dq.x) * 5.0));
            float don = step(1.0 - uLit * 0.8, hash(vec2(floor(vUV.x / 2.8), seed * 13.0)));
            em += win * dorm * don * 1.3 * lit;
            alb *= 1.0 - dorm * 0.5;
        }
    } else if (kind == 6 || kind == 7 || kind == 9) {
        // glass towers: curtain-wall panels with their seams and spandrels, the city mirrored in them, rooms lit in
        // their own tints, a bright lobby at the foot, a lit crown; on the roofs a helipad or a garden
        alb = roofFace ? vec3(0.06) : uGlassC;
        gloss = roofFace ? 0.0 : 0.85;
        if (!roofFace) {
            float cw = kind == 6 ? 2.2 : 3.2, ch = kind == 6 ? 3.6 : 4.2;
            float style = fract(seed * 3.3);
            vec3 wv = windowsC(vUV, cw, ch, style < 0.5 ? 0.7 : 0.9, style < 0.5 ? 0.62 : 0.72, seed, uLit * 0.75, win, 0.0, frame);
            em += wv * lit;
            // the spandrels (an opaque band at each floor slab) and the mullions' seams
            float span = gridLine(vUV.y / ch, style < 0.5 ? 0.18 : 0.12);
            float mull = gridLine(vUV.x / cw, 0.05);
            alb = mix(alb, alb * 1.6 + 0.02, span * 0.6);
            gloss *= 1.0 - 0.5 * span;
            alb *= 1.0 + 0.4 * mull;
            if (kind == 7) alb *= 1.0 + 0.3 * smoothstep(0.85, 0.95, abs(fract(vUV.x / 9.0) * 2.0 - 1.0));
            // bays: a darker structural column every few windows; lit corner strips on some towers
            float bay = gridLine(vUV.x / (cw * (3.0 + floor(fract(seed * 6.1) * 3.0))) + 0.5, 0.06);
            em *= 1.0 - 0.85 * bay;
            alb *= 1.0 - 0.4 * bay;
            float halfW = vS.w < 0.5 ? vS.y : vS.x;
            float edgeK = smoothstep(0.965, 0.99, abs(vUV.x) / max(halfW, 1.0)) * step(0.6, fract(seed * 8.3)) * (1.0 - smoothstep(0.3, 0.6, fwidth(vUV.x) / max(halfW * 0.04, 0.01)));
            em += mix(vec3(1.0, 0.97, 0.92), uWinAccent, step(0.85, fract(seed * 8.3))) * edgeK * 0.6 * lit;
            // the lobby: tall, bright glass at the foot of the tower, a canopy over the entrance
            if (vS.z - hy > 0.0 && hy < 7.0 && vS.z > 40.0) {
                float lob = step(0.4, hy) * (1.0 - smoothstep(5.5, 5.8, hy));
                em += mix(uWinWarm, vec3(1.0, 0.97, 0.9), 0.5) * lob * 1.1 * lit * (1.0 - 0.6 * gridLine(vUV.x / 3.0, 0.03));
                float can = step(5.8, hy) * (1.0 - step(6.3, hy)) * (1.0 - smoothstep(4.0, 4.3, abs(vUV.x)));
                alb = mix(alb, vec3(0.1), can);
                em += vec3(1.0, 0.95, 0.85) * can * 0.5 * lit;
            }
            // a lit crown on some towers, its color their own (warm, white, the accent)
            float cs = fract(seed * 13.7);
            float crown = smoothstep(vS.z * 0.95, vS.z * 0.96, hy) * (1.0 - smoothstep(vS.z * 0.985, vS.z, hy));
            float crown2 = smoothstep(0.85, 0.95, abs(fract(hy / max(vS.z * 0.04, 6.0)) * 2.0 - 1.0)) * smoothstep(vS.z * 0.86, vS.z * 0.88, hy);
            float crown3 = gridLine(vUV.x / 4.0, 0.06) * smoothstep(vS.z * 0.9, vS.z * 0.92, hy) * step(0.7, fract(seed * 2.9));
            vec3 cc = cs < 0.33 ? uWinWarm * 1.2 : cs < 0.6 ? vec3(1.0, 0.97, 0.92) : uWinAccent;
            em += cc * (crown + crown2 * 0.5 * step(0.5, fract(seed * 5.3)) + crown3 * 0.7) * (kind == 6 ? 0.6 : 1.1) * lit * step(60.0, vS.z) * step(0.35, fract(seed * 7.1));
            if (kind == 9) em += uWinAccent * 0.6 * smoothstep(0.9, 0.98, abs(fract(hy / 2500.0) * 2.0 - 1.0));
        } else if (vS.x > 8.0) {
            // the roof: a helipad (a lit ring, its touchdown circle), a garden with lamps, or plant and gravel
            float rt = fract(seed * 4.7);
            if (rt < 0.25) {
                float ring = disc(vUV, vec2(0.0), vS.x * 0.5) - disc(vUV, vec2(0.0), vS.x * 0.44);
                alb = mix(alb, vec3(0.3), ring);
                float dots = step(0.8, abs(cos(atan(vUV.y, vUV.x) * 8.0))) * ring;
                em += vec3(0.6, 1.0, 0.7) * dots * 1.2 * lit * (0.6 + 0.4 * step(0.5, fract(uTime * 0.5 + seed)));
            } else if (rt < 0.45) {
                alb = mix(vec3(0.05, 0.12, 0.06), vec3(0.08, 0.2, 0.08), vnoise(vUV * 0.5));
                vec2 g = floor(vUV / 6.0);
                vec2 o = (g + 0.5) * 6.0;
                em += uWinWarm * exp(-dot(vUV - o, vUV - o) * 3.0) * 0.8 * lit * step(0.5, hash(g + seed));
            } else {
                alb *= 0.8 + 0.4 * vnoise(vUV * 0.3);
            }
        }
    } else if (kind == 10) {
        // rooftop clutter and building-site gear: brick chimneys, metal plant, water tanks, antennas, cranes
        if (seed < 0.3) {
            alb = uRoofA * 0.55 * (1.0 - 0.2 * gridLine(hy / 0.25, 0.1));
        } else if (seed < 0.6) {
            alb = vec3(0.32, 0.33, 0.35) * (0.8 + 0.3 * gridLine(vUV.x / 0.6, 0.15));
        } else if (seed < 0.8) {
            alb = vec3(0.35, 0.28, 0.22) * (1.0 - 0.3 * gridLine(hy / 0.8, 0.08));
        } else {
            // a crane: lattice steel, yellow, a red light at its top
            float lat = gridLine(hy / max(vS.x * 2.0, 1.0), 0.07) + gridLine((hy + vUV.x) / max(vS.x * 2.0, 1.0), 0.05);
            alb = vec3(0.75, 0.55, 0.08) * (0.55 + 0.6 * clamp(lat, 0.0, 1.0));
            em += vec3(1.0, 0.1, 0.05) * smoothstep(vS.z * 0.985, vS.z, hy) * 2.0 * lit;
        }
    } else if (kind == 11) {
        // a bridge: stone or steel, arches below the deck, a lit parapet
        alb = uStone * 1.1;
        if (!roofFace) {
            float span = max(vS.z * 2.2, 8.0);
            float ax = fract(vUV.x / span) - 0.5;
            float arch = step(hy, vS.z - 1.6) * step(sq(ax * span) / sq(span * 0.42) + sq((hy) / max(vS.z - 2.0, 0.5)), 1.0);
            alb *= 1.0 - arch;
            em += uStreetC * exp(-sq(hy - (vS.z - 0.2)) * 8.0) * 0.25 * lit;
        } else {
            alb = vec3(0.16, 0.15, 0.15);
            em += uStreetC * gridLine(vUV.x / 12.0, 0.03) * 0.3 * lit;
        }
    } else {
        // arcologies: pale terraces, bands of light every few floors, windows too fine to tell apart
        alb = uArcoC * (0.1 + 0.06 * seed);
        float band = smoothstep(0.86, 0.94, abs(fract(hy / 160.0) * 2.0 - 1.0));
        float bw = fwidth(hy / 160.0);
        band = mix(band, 0.05, smoothstep(0.2, 0.6, bw));
        float wv = windows(vUV, 12.0, 8.0, 0.6, 0.6, seed, uLit, frame);
        em += win * (wv * 0.6 + band * 1.1) * lit;
        em += uWinAccent * band * 0.4 * lit * step(0.5, seed);
        alb *= 1.0 - band * 0.4;
    }
    // the giant ones seen from orbit: dark bodies, their lights doing the work
    {
        float farB = smoothstep(6.0e4, 2.5e5, vK.w);
        alb *= 1.0 - 0.6 * farB;
        gloss *= 1.0 - farB;
        em *= 1.0 - 0.45 * farB;
    }
    // the light: the sky's light and the low sun's band; at night the moon's, the city's glow from below
    vec3 light = uSkyAmb * (0.6 + 0.4 * nUp) + uSunC * toonBand(ndl) * (1.0 - uNightK * 0.92);
    light += uMoonL * toonBand(dot(Nf, uMoonDir)) * uNightK;
    {
        // the afterglow rimming the edges turned toward the low sun
        vec3 V = normalize(-vW);
        float rim = pow(1.0 - max(dot(Nf, V), 0.0), 3.0) * smoothstep(-0.2, 0.4, dot(Nf, uSunDir) + 0.3);
        light += uSunGlow * rim * 0.6 * (1.0 - uNightK);
    }
    float low = exp(-hy / max(12.0, 0.12 * vS.z)) * (1.0 - max(nUp, 0.0));
    light += uCityGlowC * (0.08 + 0.35 * low) * uGlowK * uNightK;
    vec3 col = alb * light;
    if (gloss > 0.0) {
        // the sky in the glass (a cheap gradient), the sun's sheen at dusk
        vec3 v = normalize(vW);
        vec3 rd = reflect(v, Nf);
        float rel = dot(rd, up);
        vec3 refl = mix(mix(uHazeC, uSkyAmb * 2.2, smoothstep(-0.1, 0.6, rel)), uCityGlowC * uGlowK * 0.25, step(rel, 0.0) * uNightK) * (1.0 - 0.6 * uNightK);
        refl += uSunGlow * pow(max(dot(rd, uSunDir), 0.0), 16.0) * (1.0 - uNightK * 0.8) * 1.4;
        {
            // the neighbours' lights mirrored: soft warm and white points low in the reflection
            float az = atan(rd.x, rd.z);
            vec2 g = vec2(az * 40.0, rel * 60.0);
            vec2 gi = floor(g);
            float pt = step(0.72, hash(gi + floor(seed * 50.0))) * exp(-dot(fract(g) - 0.5, fract(g) - 0.5) * 18.0);
            refl += mix(uWinWarm, vec3(1.0), hash(gi + 3.0)) * pt * 0.6 * uNightK * smoothstep(0.35, -0.05, rel) * smoothstep(-0.4, -0.05, rel);
        }
        float fres = 0.15 + 0.85 * pow(1.0 - max(dot(-v, Nf), 0.0), 4.0);
        col = mix(col, refl, gloss * fres * 0.7);
    }
    col += em * night;
    col = atmos(col, wp);
    frag = vec4(col, 0.0);
}
)";

// ---------------------------------------------------------------- lights in the air
// Billboards (meters): soft glows (sparks, flying traffic, fireflies), beacons kept a pixel wide, trees standing
// near the hut, a beam along a direction (the space elevator's tether, the drop's searchlights), smoke.
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, size (meters, radius)
layout(location = 2) in vec4 i1; // rgb, alpha
layout(location = 3) in vec4 i2; // mode, seed, param, -
layout(location = 4) in vec4 i3; // direction, length (beams)
uniform mat4 uVP;
uniform vec3 uEye, uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
flat out vec4 vB;
flat out vec4 vC;
out float vDim;
void main() {
    float d = length(i0.xyz - uEye);
    vLocal = aPos;
    vB = i1;
    vC = i2;
    int mode = int(i2.x + 0.5);
    if (mode == 3) {
        // a beam: from its foot along its direction, at least a pixel and a half wide
        vec3 dir = i3.xyz;
        vec3 mid = i0.xyz + dir * (i3.w * (aPos.y * 0.5 + 0.5));
        float dm = length(mid - uEye);
        vec3 side = normalize(cross(dir, mid - uEye));
        float wt = i0.w * mix(1.0, max(i2.w, 1.0), aPos.y * 0.5 + 0.5);
        float w = max(wt, uMinPx * dm * 1.5);
        vDim = min(1.0, wt / max(uMinPx * dm * 1.5, 1e-20) + 0.5);
        vec3 wp = mid + side * aPos.x * w;
        gl_Position = uVP * vec4(wp - uEye, 1.0);
        return;
    }
    if (mode == 2) {
        // a tree, standing on its foot, facing the camera across
        float px = i0.w / max(uMinPx * d / 0.8, 1e-20);
        vDim = smoothstep(0.5, 2.0, px);
        vec3 up = normalize(i0.xyz + vec3(0.0, R_EARTH, 0.0));
        vec3 side = normalize(cross(up, normalize(i0.xyz - uEye)));
        vec3 w = i0.xyz + side * aPos.x * i0.w * 0.75 + up * (aPos.y + 1.0) * i0.w;
        gl_Position = uVP * vec4(w - uEye, 1.0);
        return;
    }
    float minS = uMinPx * d * (mode == 1 ? 1.8 : 2.2);
    float s = max(i0.w, minS);
    vDim = mode == 1 ? 1.0 : min(1.0, sq(i0.w / max(minS, 1e-20)) + 0.35);
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * s;
    gl_Position = uVP * vec4(w - uEye, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vB;
flat in vec4 vC;
in float vDim;
out vec4 frag;
uniform vec3 uTreeC, uTreeShade, uTrunkC;
void main() {
    int mode = int(vC.x + 0.5);
    float r = length(vLocal);
    if (mode == 2) {
        // a round toon tree: a trunk, two or three balls of foliage, lit from the sun's side
        vec2 q = vLocal;
        float y = q.y * 0.5 + 0.5;
        float trunk = step(abs(q.x), 0.09) * step(y, 0.45);
        float s1 = vC.y;
        vec2 c1 = vec2(0.0, 0.25), c2 = vec2(-0.32 + 0.1 * s1, -0.05), c3 = vec2(0.3 - 0.1 * s1, 0.0);
        float b1 = length(q - c1) / (0.62 - 0.08 * s1), b2 = length(q - c2) / 0.4, b3 = length(q - c3) / 0.42;
        float m = min(b1, min(b2, b3));
        float aa = fwidth(m) * 1.2 + 1e-3;
        float leaf = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, m);
        float sunSide = clamp(0.5 + 0.9 * (q.x * vC.z + q.y * 0.5), 0.0, 1.0);
        vec3 lc = mix(uTreeShade, uTreeC, smoothstep(0.45, 0.55, sunSide));
        vec3 col = leaf > 0.0 ? lc : uTrunkC;
        col *= vB.rgb;
        float a = max(leaf, trunk) * vB.a * vDim;
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 3) {
        // a beam: bright core, soft edge, fading along it
        float t = vLocal.y * 0.5 + 0.5;
        float v = (exp(-sq(vLocal.x * 2.2)) * 0.6 + exp(-sq(vLocal.x * 7.0))) * vDim;
        v *= mix(1.0, sq(1.0 - t), vC.z) * smoothstep(0.0, 0.02, t);
        if (vC.w > 1.5) v = (exp(-sq(vLocal.x * 1.6)) * 0.7 + exp(-sq(vLocal.x * 5.0)) * 0.5) * sq(1.0 - t) * smoothstep(0.0, 0.05, t);
        frag = vec4(vB.rgb * v * vB.a, 0.0);
        return;
    }
    if (mode == 5) {
        // a bird: two wings beating
        float fl = sin(vC.z);
        vec2 q = vLocal;
        float x = abs(q.x);
        float wingY = fl * 0.45 * x - 0.15 * x * x;
        float wing = smoothstep(0.16, 0.05, abs(q.y - wingY)) * step(x, 0.95) * (1.0 - smoothstep(0.7, 0.95, x) * 0.6);
        float body = smoothstep(0.2, 0.1, length(q * vec2(1.0, 1.6)));
        float a = max(wing, body) * vB.a * vDim;
        frag = vec4(vB.rgb * a, a);
        return;
    }
    if (mode == 4) {
        // smoke: a soft grey puff (alpha blended)
        float a = exp(-r * r * 3.0) * smoothstep(1.0, 0.6, r) * vB.a * vDim;
        frag = vec4(vB.rgb * a, a);
        return;
    }
    float v = mode == 1 ? exp(-r * r * 9.0) + 0.25 * exp(-r * r * 2.5) : exp(-r * r * 14.0) * 1.2 + 0.4 * exp(-r * r * 3.0);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(vB.rgb * v * vB.a * vDim, 0.0);
}
)";

// ---------------------------------------------------------------- composite: the world upscaled, the bloom, stars
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld, uBloomT;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan, uRes;
uniform float uBloomK, uTwinkle, uTime, uSat;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec4 w = texture(uWorld, vUv);
    if (any(isnan(w)) || any(isinf(w))) w = vec4(0.0);
    vec3 c = w.rgb + texture(uBloomT, vUv).rgb * uBloomK;
    float vis = w.a;
    if (vis > 0.002) {
        vec3 d = rayDir(vUv);
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
            if (rr < (L == 0 ? 0.3 : 0.4)) {
                vec2 o = (g + 0.25 + 0.5 * hash2(g + 3.1)) * cell;
                vec2 dd = Ji * (sp - o);
                float b = pow(hash(g + 7.7), L == 0 ? 2.5 : 1.5);
                float rad = (L == 0 ? 0.6 : 1.0) + 0.5 * b;
                float tw = 1.0 + uTwinkle * sin(uTime * (1.2 + 3.0 * hash(g + 1.3)) + rr * 80.0);
                vec3 tint = mix(vec3(0.7, 0.85, 1.0), vec3(1.0, 0.88, 0.7), hash(g + 9.1));
                st += tint * exp(-dot(dd, dd) / (rad * rad)) * b * max(tw, 0.0) * (L == 0 ? 0.7 : 1.5) * fade;
            }
        }
        c += st * vis;
    }
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSat), 0.0);
    frag = vec4(c, 1.0);
}
)";
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
        // the gauge: a faint ring, the charge a bright arc from the top, clockwise
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ring = exp(-sq((r - 0.8) / max(px * 1.2, 0.012)));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = exp(-sq((r - 0.8) / max(px * 2.5, 0.03))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.6 + 0.4 * vC.y);
        float v = ring * 0.35 + arc + halo;
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

// ---------------------------------------------------------------- pieces: lit glass
// Each piece one shape (its cells merge): tinted glass lit from within like a window at night, a soft light pooling
// at its heart, a cool sheen sliding down it like the reflections on a tower's glass, a bright rim. The ghost is the
// rim alone; a cleared cell dissolves into light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, -, drain
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
    float sd = shapeSdf(vLocal, mk, 0.05);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    float rimW = max(aa * 1.6, 0.035);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.55);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.08) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // glass lit from within: deeper toward the middle, a soft light pooling at the heart of each cell
    vec3 col = tint * mix(1.0, 0.5, smoothstep(0.02, 0.36, e));
    float glow = smoothstep(0.1, 0.55, e);
    col += mix(tint, uLightC, 0.4) * glow * 0.42;
    // a cool sheen sliding slowly down the stack, like the sky on a tower's glass
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.y * 0.9 + vWorld.x * 0.35) * 0.55 + uTime * 0.25)), 8.0);
    col += mix(vec3(0.8, 0.9, 1.0), uLightC, 0.3) * sheen * 0.13 * smoothstep(0.0, 0.25, e);
    col += tint * exp(-abs(e - 0.1) / 0.025) * 0.22;
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.3, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        float n = vnoise(vWorld * 2.3) * 0.65 + vnoise(vWorld * 5.1) * 0.35;
        keep = smoothstep(erase * 1.4 - 0.45, erase * 1.4 - 0.05, n) * (1.0 - smoothstep(0.55, 1.0, erase));
        col = mix(col, hi * 1.35, smoothstep(0.0, 0.4, erase));
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * 0.97 * vB.a * keep;
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
uniform float uAlpha, uHaloA, uGlowA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uHaloA;
    float glow = smoothstep(0.15, 0.47, d) * uGlowA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill) + uInk * glow * (1.0 - fill) * 0.35;
    frag = vec4(c, a) * uAlpha;
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
    c *= 1.0 - 0.22 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace megacity_shaders

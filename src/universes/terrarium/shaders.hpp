#pragma once
// TERRARIUM's shaders: the world (a glass jar on a windowsill, ray marched at a reduced resolution: the window and
// the sky behind it, the sill, the jar's glass with its fresnel, thick edges, highlights and condensation, and the
// little world inside it, its soil seen in section through the glass, moss, ferns, mushrooms, a pond, a sapling,
// flowers, sprouts, a beetle, fireflies), the veil behind the well, the pebble pieces, lettering.

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
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uEye, uFwd, uRight, uUp;
uniform float uTanH, uAspect, uTime, uPx;
uniform int uKind;                       // 0 the mossy forest, 1 the desert, 2 the tropics
uniform int uSlots;
uniform vec4 uSA[24];                    // x, y (the soil there), z, size
uniform vec4 uSB[24];                    // type, growth, seed, rotation
uniform vec4 uSC[24];                    // param, -, -, -
uniform int uSprouts;
uniform vec4 uSp[40];                    // x, y, z, growth
uniform int uFlies;
uniform vec4 uFly[32];                   // x, y, z, glow
uniform int uSeeds;
uniform vec4 uSeedP[32];                 // x, y, z, alpha
uniform vec4 uBug;                       // x, y, z, heading
uniform vec4 uBug2;                      // walk phase, moving, look, blink
uniform vec4 uPondP;                     // x, z, radius, depth
uniform float uPondY, uPond, uMossCover, uSway, uSpots;
uniform vec3 uSunDir, uSunCol, uSkyZ, uSkyH, uAmbS, uAmbG, uFill, uGlowSun;
uniform vec2 uSunSky;
uniform float uSunDisc, uMoon, uStars, uRain, uFog, uCond, uNight, uFlyAmt;
uniform vec3 uSoil, uSoil2, uGravel, uStr1, uStr2, uMoss, uMoss2, uLeaf, uLeaf2, uStem, uCap, uCap2, uFlower, uFlower2,
             uBark, uWater, uStone, uBugC, uBugSpot, uFlyC, uSill, uFrame, uWall, uHill, uHill2;
uniform vec3 uBridgeCol;
uniform float uWorldA, uSlow;
// the growth out of the jar: the cork's pose, the tree from the neck, vines inside the glass and spilling out of it,
// the greenery over the sill, up the window frame and over the wall, vines hanging over the panes and the sill's edge
uniform vec3 uCorkP;
uniform float uCorkR;
uniform float uTreeG, uInVine, uSpill, uSillR, uClimb, uWallR, uHang, uDrape, uBloomAmt;
uniform int uBlooms;
uniform vec4 uBloom[24];                 // x, y, z, growth (on the sill: y = 0; on the wall: z = ZW)
uniform int uSwarm;
uniform vec4 uSw[32];                    // the fireflies drifting out of the window: x, y, z, glow

const float JR = 1.0, JHB = 0.8, JRN = 0.62, JHN = 0.93, JT = 0.022;
const float ZW = -2.3;                   // the window wall
const float SF = 1.3;                    // the sill's front edge
const float WX = 2.4, WY = 2.7;          // the window opening (half width, height above the sill)
const float WCX = -1.2;                  // its center (the jar stands on the right of the sill)
const float BS = 1.9;                    // the beetle's scale
const float PI = 3.14159265, TAU = 6.2831853;

// ---------------------------------------------------------------- primitives
float sdEll(vec3 p, vec3 r) { float k0 = length(p / r); float k1 = length(p / (r * r)); return k0 * (k0 - 1.0) / max(k1, 1e-6); }
float sdCap(vec3 p, vec3 a, vec3 b, float r) { vec3 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / max(dot(ba, ba), 1e-8)); return length(pa - ba * h) - r; }
float sdCapT(vec3 p, vec3 a, vec3 b, float ra, float rb) {
    vec3 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / max(dot(ba, ba), 1e-8));
    return length(pa - ba * h) - mix(ra, rb, h);
}
float smin(float a, float b, float k) { float h = sat(0.5 + 0.5 * (b - a) / k); return mix(b, a, h) - k * h * (1.0 - h); }
float sdRoundBox2(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

// ---------------------------------------------------------------- the jar
float jarOuter(vec3 p) {
    vec2 q = vec2(length(p.xz), p.y);
    float body = sdRoundBox2(vec2(q.x, q.y - JHB * 0.5), vec2(JR, JHB * 0.5), vec4(0.36, 0.12, 0.36, 0.12));
    float neck = sdBox2(vec2(q.x, q.y - (JHB - 0.2 + JHN) * 0.5), vec2(JRN, (JHN - JHB + 0.2) * 0.5));
    float d = smin(body, neck, 0.07);
    d = min(d, length(vec2(q.x - JRN + 0.004, q.y - JHN + 0.012)) - 0.02); // the lip
    return d;
}
float cork(vec3 p) {
    vec3 q3 = p - uCorkP;
    q3.xy = rot(uCorkR) * q3.xy;
    vec2 q = vec2(length(q3.xz), q3.y);
    float r = JRN - 0.03 + 0.03 * sat(q.y / 0.12);
    return sdRoundBox2(vec2(q.x, q.y), vec2(r, 0.11), vec4(0.03, 0.0, 0.03, 0.0));
}
vec3 jarNormal(vec3 p) {
    const vec2 e = vec2(0.0015, 0.0);
    return normalize(vec3(jarOuter(p + e.xyy) - jarOuter(p - e.xyy), jarOuter(p + e.yxy) - jarOuter(p - e.yxy), jarOuter(p + e.yyx) - jarOuter(p - e.yyx)));
}

// ---------------------------------------------------------------- the tree out of the neck
// A sapling rooted in the jar pushes the cork off, grows out of the neck and becomes a small tree arching over the
// window: its trunk and branches are revealed along their length as uTreeG grows, its crown is leafy masses.
vec3 growTo(vec3 a, vec3 b, float g0, float g1) { return mix(a, b, smoothstep(g0, g1, uTreeG)); }
float treeD(vec3 p, out float m, out vec4 inf) {
    m = 8.0; inf = vec4(0.0);
    if (uTreeG <= 0.001) return 1e3;
    const vec3 P0 = vec3(0.0, 0.12, 0.0), P1 = vec3(0.06, 0.97, -0.02), P2 = vec3(-0.22, 1.5, -0.45);
    const vec3 P3 = vec3(-0.35, 2.1, -1.0), P4 = vec3(-1.75, 2.0, -1.25), P5 = vec3(0.75, 2.4, -1.45);
    vec3 a1 = growTo(P0, P1, 0.0, 0.2), a2 = growTo(P1, P2, 0.18, 0.45), a3 = growTo(P2, P3, 0.4, 0.7);
    vec3 a4 = growTo(P2, P4, 0.5, 0.8), a5 = growTo(P3, P5, 0.6, 0.9);
    float th = (0.45 + 0.55 * smoothstep(0.15, 1.0, uTreeG)) * 1.5;
    float d = sdCapT(p, P0, a1, 0.05 * th, 0.04 * th);
    if (uTreeG > 0.18) d = smin(d, sdCapT(p, P1, a2, 0.04 * th, 0.03 * th), 0.02);
    if (uTreeG > 0.4) d = smin(d, sdCapT(p, P2, a3, 0.03 * th, 0.02 * th), 0.015);
    if (uTreeG > 0.5) d = smin(d, sdCapT(p, P2, a4, 0.024 * th, 0.014 * th), 0.012);
    if (uTreeG > 0.6) d = smin(d, sdCapT(p, P3, a5, 0.02 * th, 0.012 * th), 0.01);
    inf = vec4(p.y, 0.0, 0.0, 0.0);
    // leaves: a tuft at the growing tip, then the crown's leafy masses
    float gc = smoothstep(0.55, 1.0, uTreeG);
    vec3 tip = uTreeG < 0.45 ? a2 : a3;
    float rt = 0.09 * smoothstep(0.08, 0.3, uTreeG) * (1.0 - gc);
    if (rt > 0.004) {
        vec3 qt = p - tip - vec3(0.0, rt * 0.5, 0.0);
        float dl = sdEll(qt, vec3(rt, rt * 0.7, rt));
        if (dl < d) { d = dl; m = 3.5; inf = vec4(qt / rt, 1.0); }
    }
    if (gc > 0.01) {
        for (int k = 0; k < 12; k++) {
            float fk = float(k);
            // tufts along the branches' arc, a little scattered
            float u = fk / 11.0;
            vec3 C = u < 0.5 ? mix(P4, P3, u * 2.0) : mix(P3, P5, u * 2.0 - 1.0);
            C += vec3(sin(fk * 2.3) * 0.18, 0.18 + 0.2 * sin(fk * 1.7 + 1.0) + 0.25 * sin(u * PI), cos(fk * 1.9) * 0.2);
            float R = (0.24 + 0.1 * sin(fk * 3.1)) * smoothstep(abs(u - 0.5) * 0.5, abs(u - 0.5) * 0.5 + 0.5, gc);
            if (R < 0.01) continue;
            vec3 q = p - C;
            float dl = length(q * vec3(1.0, 1.3, 1.0)) - R;
            if (dl < d + 0.1) dl += R * 0.3 * (vnoise3(q / R * 2.4 + fk * 3.1) - 0.5) + R * 0.12 * (vnoise3(q / R * 6.0 + fk) - 0.5);
            if (dl < d) { d = dl; m = 3.5; inf = vec4(q / R, 1.0); }
        }
    }
    return d;
}

// ---------------------------------------------------------------- the soil (the same function as the CPU's)
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

// ---------------------------------------------------------------- the material of the closest thing
float gMat;      // 1 soil, 3 moss, 4 frond, 5 stem, 6 cap, 7 mushroom stem, 8 bark, 9 petal, 10 flower heart, 11 stone,
                 // 12 water, 13 succulent, 14 cactus, 15 shell, 16 beetle dark, 17 beetle head, 18 sprout, 19 cork
vec4 gInf;
int gSl[12];     // the slots this ray comes near
vec4 gSb[12];    // and their bounds
int gNs;
int gSpL[12];    // the sprouts
int gNsp;
bool gBug;

void take(inout float d, float nd, float m, vec4 inf) { if (nd < d) { d = nd; gMat = m; gInf = inf; } }

// a fern's frond in its own plane: l = (u outward, v up, w across)
float frond(vec3 l, float L, float g, float sway, inout vec4 inf) {
    float unf = smoothstep(0.25, 1.0, g);
    float S = L * (0.3 + 0.7 * smoothstep(0.0, 0.6, g));
    float Rf = L * (1.25 + 0.1 * sway);
    float th0 = 0.2 + 0.75 * inf.w;
    float Rc = 0.011 * L / 0.35 * (1.0 + 0.5 * (1.0 - unf));
    float sc = min(S * (1.0 - unf), Rc * 5.4);
    float s1 = S - sc;
    float psi = sc / Rc;
    vec2 q = l.xy;
    vec2 c1 = Rf * vec2(cos(th0), -sin(th0));
    vec2 dq = q - c1;
    float a = atan(dq.y, dq.x);
    if (a < -1.2) a += TAU;
    float a0 = PI - th0;
    float tc = clamp((a0 - a) * Rf, 0.0, s1);
    float ang = a0 - tc / Rf;
    vec2 Nr = vec2(cos(ang), sin(ang));
    vec2 P = c1 + Rf * Nr;
    vec2 T = vec2(sin(ang), -cos(ang));
    vec2 dp = q - P;
    float al = dot(dp, T), rad = dot(dp, Nr);
    float aw = abs(l.z);
    // the rachis
    float d = length(vec3(al, rad, l.z)) - 0.0045 * (1.0 - 0.6 * tc / max(L, 1e-3));
    inf = vec4(tc / L, 0.0, 0.0, 0.0);
    // the pinnae: along the open part, longest a third of the way up, young ones small near the curl
    // (far from the blade's envelope, pinnae and their droop included: a safe lower bound is enough)
    float env = length(vec3(al, max(abs(rad + 0.03 * L) - 0.045 * L, 0.0), max(aw - 0.36 * L, 0.0))) - 0.004;
    if (env > 0.015 && psi <= 0.05) return min(d, env);
    float dl = 0.034 * L / 0.35;
    float tau = tc + al - 0.4 * aw;
    float k = floor(tau / dl + 0.5);
    float tk = k * dl;
    float x = tk / L;
    float xs = clamp(x - 0.03, 0.0, 1.0);
    float Lp = L * 0.34 * sqrt(sat(4.0 * xs * (1.0 - xs))) * (1.0 - 0.3 * x);
    Lp *= smoothstep(0.0, 0.18 * L, s1 - tk) * step(0.04 * L, tk);
    if (Lp > 0.004) {
        float lob = fract(aw / max(Lp, 1e-3) * 3.0) - 0.5;
        float hw = dl * 0.36 * (0.75 + 0.5 * lob * lob);
        float a2 = tau - tk;
        float rr = rad + 0.5 * aw * aw / L - 0.02 * aw;
        float e = length(vec2(a2 / hw, (aw - Lp * 0.5) / (Lp * 0.5)));
        float dpn = max((e - 1.0) * min(hw, Lp * 0.5), abs(rr) - 0.0025);
        if (dpn < d) { d = dpn; inf = vec4(tc / L, aw / Lp, 0.0, 0.0); }
    }
    // the fiddlehead: the rest, still curled
    if (psi > 0.05) {
        vec2 P1 = c1 + Rf * vec2(cos(a0 - s1 / Rf), sin(a0 - s1 / Rf));
        vec2 c2 = P1 + (c1 - P1) * (Rc / Rf);
        vec2 d2 = q - c2;
        float a1 = a0 - s1 / Rf;
        float b2 = atan(d2.y, d2.x);
        float dl2 = mod(a1 - b2, TAU);
        float rt = 0.0075 * L / 0.35;
        float dc;
        if (dl2 <= psi) dc = length(vec2(length(d2) - Rc, l.z)) - rt * (1.0 - 0.4 * dl2 / max(psi, 1e-3));
        else {
            vec2 E = c2 + Rc * vec2(cos(a1 - psi), sin(a1 - psi));
            dc = min(length(vec3(q - E, l.z)), length(vec3(q - P1, l.z))) - rt * 0.6;
        }
        if (dc < d) { d = dc; inf = vec4(1.0, 0.0, 1.0, 0.0); }
    }
    return d;
}

float fern(vec3 p, vec3 B, float s, float g, float seed, float rt, inout vec4 inf) {
    const int N = 7;
    vec3 r = p - B;
    float beta = atan(r.z, r.x);
    float fi = (beta - rt) / (TAU / float(N));
    float i0 = floor(fi);
    float d = 1e3;
    for (int j = 0; j < 2; j++) {
        float ii = mod(i0 + float(j), float(N));
        float h = hash1(seed * 31.0 + ii);
        float phi = rt + (ii + 0.5 + 0.3 * (h - 0.5)) * (TAU / float(N));
        phi += 0.04 * uSway * sin(uTime * 0.7 + ii * 1.7 + seed * 5.0);
        vec2 dir = vec2(cos(phi), sin(phi));
        vec3 l = vec3(dot(r.xz, dir), r.y, dot(r.xz, vec2(-dir.y, dir.x)));
        float L = s * (0.72 + 0.4 * hash1(seed * 17.0 + ii * 3.1));
        float gi = sat((g - 0.5 * ii / float(N)) / 0.5);
        if (gi <= 0.001) continue;
        vec4 fi4 = vec4(0.0, 0.0, 0.0, hash1(seed * 13.0 + ii * 5.3));
        float df = frond(l, L, gi, sin(uTime * 0.6 + ii) * uSway, fi4);
        if (df < d) { d = df; inf = fi4; }
    }
    return d;
}

float mushrooms(vec3 p, vec3 B, float s, float g, float seed, inout vec4 inf, inout float m) {
    float d = 1e3;
    for (int j = 0; j < 3; j++) {
        float fj = float(j);
        vec2 hh = hash2(vec2(seed * 7.0 + fj, fj * 3.7 + 1.0));
        float gj = sat((g - fj * 0.22) / 0.56);
        if (gj <= 0.001) continue;
        vec2 off = (hh - 0.5) * s * (j == 0 ? 0.25 : 0.9);
        float H = s * (0.55 + 0.45 * hh.y) * (j == 0 ? 1.15 : 0.8) * smoothstep(0.0, 0.6, gj);
        float Rc = s * (0.32 + 0.16 * hh.x) * (j == 0 ? 1.1 : 0.85) * smoothstep(0.2, 1.0, gj);
        vec3 q = p - (B + vec3(off.x, -0.01, off.y));
        vec2 lean = (hh.yx - 0.5) * 0.5;
        float qy = clamp(q.y, 0.0, H);
        q.xz -= lean * qy * qy / (s * 0.8);
        float ds = sdCapT(q, vec3(0.0), vec3(0.0, H, 0.0), 0.16 * Rc + 0.006, 0.11 * Rc + 0.004);
        vec3 qc = q - vec3(0.0, H, 0.0);
        float dc = Rc > 0.003 ? max(sdEll(qc, vec3(Rc, 0.62 * Rc, Rc)), -(qc.y + 0.08 * Rc)) : 1e3;
        if (ds < d) { d = ds; m = 7.0; inf = vec4(q.y / max(H, 1e-3), 0.0, 0.0, 0.0); }
        if (dc < d) { d = dc; m = 6.0; inf = vec4(qc / max(Rc, 1e-3), fj + seed); }
    }
    return d;
}

// a petal disc: 5 petals around n (the flower's facing), pr its radius
float petals(vec3 q, vec3 n, float pr, float big, inout vec4 inf, inout float m) {
    vec3 t1 = normalize(cross(n, vec3(0.0, 0.0, 1.0)));
    vec3 t2 = cross(n, t1);
    vec3 l = vec3(dot(q, t1), dot(q, t2), dot(q, n));
    float r = length(l.xy);
    float a = atan(l.y, l.x);
    float step = TAU / 5.0;
    float ak = floor(a / step + 0.5) * step;
    vec2 pl = rot(ak) * l.xy;
    float h = l.z - 0.35 * r * r / max(pr, 1e-3) * (big > 0.5 ? 1.6 : 1.0);
    float e = length(vec2((pl.x - pr * 0.55) / (pr * 0.55), pl.y / (pr * (big > 0.5 ? 0.42 : 0.33))));
    float dp = max((e - 1.0) * pr * 0.3, abs(h) - 0.0022);
    float dcn = length(l - vec3(0.0, 0.0, 0.004)) - pr * 0.24;
    float d = dp;
    m = 9.0; inf = vec4(r / max(pr, 1e-3), pl.y / max(pr, 1e-3), 0.0, 0.0);
    if (dcn < d) { d = dcn; m = 10.0; }
    return d;
}

float flowers(vec3 p, vec3 B, float s, float g, float seed, float big, inout vec4 inf, inout float m) {
    float d = 1e3;
    int n = big > 0.5 ? 3 : 5;
    for (int j = 0; j < 5; j++) {
        if (j >= n) break;
        float fj = float(j);
        vec2 hh = hash2(vec2(seed * 5.0 + fj, fj * 1.3 + 7.0));
        float gj = sat((g - fj * 0.1) / 0.6);
        if (gj <= 0.001) continue;
        vec2 off = (hh - 0.5) * s * 0.9;
        float H = s * (0.55 + 0.45 * hh.y) * (big > 0.5 ? 1.4 : 1.0) * smoothstep(0.0, 0.55, gj);
        vec3 q = p - (B + vec3(off.x, -0.005, off.y));
        vec2 lean = (hh.yx - 0.5) * 0.6 + vec2(0.0, 0.15);
        vec3 top = vec3(lean.x * H * 0.5, H, lean.y * H * 0.5);
        vec3 mid = vec3(lean.x * H * 0.1, H * 0.55, lean.y * H * 0.1);
        float ds = min(sdCap(q, vec3(0.0), mid, 0.0032), sdCap(q, mid, top, 0.0028));
        // two leaves at the foot
        vec3 ql = q - vec3(0.0, 0.012, 0.0);
        ql.xz = rot(hh.x * 6.0) * ql.xz;
        float dlf = sdEll(vec3(abs(ql.x) - s * 0.12, ql.y - abs(ql.x) * 0.35, ql.z), vec3(s * 0.13, 0.004, s * 0.05) * smoothstep(0.0, 0.4, gj));
        if (min(ds, dlf) < d) { d = min(ds, dlf); m = 5.0; inf = vec4(0.0); }
        float pr = s * (big > 0.5 ? 0.3 : 0.17) * smoothstep(0.5, 1.0, gj);
        if (pr > 0.003) {
            vec3 nf = normalize(vec3(lean.x * 0.8, 1.0, 0.7 + lean.y));
            vec4 pi; float pm;
            float dp = petals(q - top, nf, pr, big, pi, pm);
            if (dp < d) { d = dp; m = pm; inf = pi; }
        }
    }
    return d;
}

// a sapling: a crooked little trunk, a branch, tufts of leaves (none on the desert's driftwood)
float sapling(vec3 p, vec3 B, float s, float g, float seed, float leafy, inout vec4 inf, inout float m) {
    float gg = smoothstep(0.0, 1.0, g);
    vec3 q = p - B;
    q.xz = rot(seed * 6.0) * q.xz;
    vec3 P1 = vec3(0.05, 0.42, 0.02) * s * gg, P2 = vec3(-0.06, 0.85, -0.03) * s * gg, P3 = vec3(0.32, 0.7, 0.05) * s * gg;
    vec3 P4 = vec3(-0.25, 1.0, 0.04) * s * gg;
    float r0 = 0.028 * s * (0.4 + 0.6 * gg);
    float d = sdCapT(q, vec3(0.0, -0.02, 0.0), P1, r0, r0 * 0.75);
    d = smin(d, sdCapT(q, P1, P2, r0 * 0.75, r0 * 0.45), 0.01);
    d = smin(d, sdCapT(q, P1 + (P2 - P1) * 0.2, P3, r0 * 0.5, r0 * 0.3), 0.008);
    d = smin(d, sdCapT(q, P2, P4, r0 * 0.4, r0 * 0.25), 0.006);
    m = 8.0; inf = vec4(q.y / max(s, 1e-3), 0.0, 0.0, 0.0);
    if (leafy > 0.5) {
        float gl = smoothstep(0.35, 1.0, g);
        for (int j = 0; j < 3; j++) {
            vec3 c = j == 0 ? P2 : (j == 1 ? P3 : P4);
            float R = s * (j == 0 ? 0.24 : 0.18) * gl;
            if (R < 0.003) continue;
            vec3 qc = q - c - vec3(0.0, R * 0.35, 0.0);
            float dl = sdEll(qc, vec3(R, R * 0.7, R));
            dl += R * 0.12 * (vnoise3(qc / max(R, 1e-3) * 3.0 + seed * 9.0 + float(j)) - 0.5);
            if (dl < d) { d = dl; m = 3.5; inf = vec4(qc / max(R, 1e-3), 1.0); }
        }
    }
    return d;
}

// a rosette of leaves: style 0 an echeveria, 1 an aloe, 2 broad tropical leaves
float rosette(vec3 p, vec3 B, float s, float g, float seed, float rt, float style, inout vec4 inf, inout float m) {
    vec3 r = p - B;
    float beta = atan(r.z, r.x);
    float d = 1e3;
    int rings = style > 1.5 ? 1 : (style > 0.5 ? 2 : 3);
    for (int k = 0; k < 3; k++) {
        if (k >= rings) break;
        float fk = float(k);
        int n; float el, len, wid, th, r0;
        if (style < 0.5) { n = k == 0 ? 10 : (k == 1 ? 8 : 5); el = k == 0 ? 0.32 : (k == 1 ? 0.75 : 1.15); len = s * (k == 0 ? 0.95 : (k == 1 ? 0.7 : 0.42)); wid = len * 0.5; th = len * 0.2; r0 = s * 0.05; }
        else if (style < 1.5) { n = k == 0 ? 9 : 6; el = k == 0 ? 0.62 : 1.05; len = s * (k == 0 ? 1.45 : 1.05); wid = len * 0.17; th = len * 0.09; r0 = s * 0.04; }
        else { n = 6; el = 0.95; len = s * 1.5; wid = len * 0.42; th = 0.006; r0 = s * 0.05; }
        float gk = sat((g - (2.0 - fk) * 0.12) / 0.7);
        if (style > 1.5) gk = g;
        if (gk <= 0.001) continue;
        len *= 0.25 + 0.75 * smoothstep(0.0, 1.0, gk);
        wid *= 0.4 + 0.6 * smoothstep(0.0, 1.0, gk);
        th *= 0.5 + 0.5 * gk;
        float stepA = TAU / float(n);
        float off = rt + fk * stepA * 0.5;
        float fi = floor((beta - off) / stepA);
        for (int j = 0; j < 2; j++) {
            float ii = fi + float(j);
            float a = off + (ii + 0.5) * stepA + 0.15 * (hash1(seed + ii * 7.0 + fk) - 0.5);
            float e = el + 0.12 * (hash1(seed * 3.0 + ii + fk * 11.0) - 0.5);
            if (style > 1.5) e += 0.05 * uSway * sin(uTime * 0.6 + ii);
            vec3 dir = vec3(cos(a), 0.0, sin(a));
            vec3 A = dir * cos(e) + vec3(0.0, sin(e), 0.0);
            vec3 W = vec3(-sin(a), 0.0, cos(a));
            vec3 Nn = cross(A, W);
            vec3 q = r - dir * r0;
            vec3 l = vec3(dot(q, A), dot(q, W), dot(q, Nn));
            // leaves arch over a little
            // (succulents cup upward, broad leaves arch over and droop)
            l.z += (style > 1.5 ? -1.2 : 0.45) * l.x * l.x / max(len, 1e-3);
            float dl;
            if (style < 0.5) dl = sdEll(l - vec3(len * 0.5, 0.0, 0.0), vec3(len * 0.5, wid * 0.5 * (1.0 - 0.3 * sat(l.x / len)), th * 0.5));
            else if (style < 1.5) dl = sdCapT(vec3(l.x, l.y, l.z * wid / th), vec3(0.0), vec3(len, 0.0, 0.0), wid * 0.5, 0.002) * (th / wid);
            else {
                float stem = len * 0.3;
                dl = sdCap(l, vec3(0.0), vec3(stem, 0.0, 0.0), 0.004);
                float u = (l.x - stem) / (len - stem);
                float ew = length(vec2((u - 0.5) * 2.0, l.y / (wid * 0.5)));
                float db = max((ew - 1.0) * wid * 0.4, abs(l.z + 0.06 * abs(l.y)) - th);
                dl = min(dl, db);
            }
            if (dl < d) { d = dl; m = 13.0; inf = vec4(sat(l.x / max(len, 1e-3)), l.y / max(wid, 1e-3), fk, style); }
        }
    }
    return d;
}

float cactus(vec3 p, vec3 B, float s, float g, float seed, inout vec4 inf, inout float m) {
    float gg = smoothstep(0.0, 1.0, g);
    float rc = s * 0.5 * (0.35 + 0.65 * gg);
    float Hc = s * (0.5 + 0.9 * gg) * (0.8 + 0.4 * hash1(seed));
    vec3 q = p - B;
    float ang = atan(q.z, q.x);
    float rib = 0.5 + 0.5 * cos(11.0 * ang + seed * 3.0);
    float d = sdCap(q, vec3(0.0, rc * 0.7, 0.0), vec3(0.0, max(Hc - rc, rc * 0.7), 0.0), rc) + rc * 0.07 * rib;
    m = 14.0; inf = vec4(rib, q.y / max(Hc, 1e-3), ang, 0.0);
    float pr = s * 0.32 * smoothstep(0.7, 1.0, g);
    if (pr > 0.003) {
        vec4 pi; float pm;
        float dp = petals(q - vec3(0.0, max(Hc - rc, rc * 0.7) + rc * 0.95, 0.0), vec3(0.0, 1.0, 0.25), pr, 0.0, pi, pm);
        if (dp < d) { d = dp; m = pm; inf = pi; }
    }
    return d;
}

// the slots' bounds (for the rays' masks)
vec4 slotBound(int i) {
    vec4 A = uSA[i], Bv = uSB[i];
    int t = int(Bv.x + 0.5);
    float s = A.w, g = max(Bv.y, 0.0);
    if (g <= 0.001) return vec4(0.0, -10.0, 0.0, 0.0);
    if (t == 0) return vec4(A.x, A.y, A.z, s * 1.2 * g + 0.01);
    if (t == 1) return vec4(A.x, A.y + s * 0.4, A.z, s * 0.88);
    if (t == 2) return vec4(A.x, A.y + s * 0.6, A.z, s * 1.35);
    if (t == 3) return vec4(A.x, A.y + s * 0.55, A.z, s * 0.85);
    if (t == 4) return vec4(A.x, A.y + s * 0.6, A.z, s * (uSC[i].x > 0.5 ? 1.45 : 1.05));
    if (t == 5) return vec4(A.x, A.y + s * 0.5, A.z, s * 1.6);
    if (t == 6) return vec4(A.x, A.y + s * 0.8, A.z, s * 1.4);
    if (t == 8) return vec4(A.x, A.y + s * 0.3, A.z, s * 1.6);
    return vec4(A.x, A.y, A.z, s * 1.2);
}

float rayBall(vec3 ro, vec3 rd, vec4 b, float tmax) {
    vec3 oc = ro - b.xyz;
    float bb = dot(oc, rd), c = dot(oc, oc) - b.w * b.w;
    float h = bb * bb - c;
    if (h < 0.0) return 0.0;
    h = sqrt(h);
    return (-bb + h > 0.0 && -bb - h < tmax) ? 1.0 : 0.0;
}

void buildMasks(vec3 ro, vec3 rd, float tmax) {
    gNs = 0;
    for (int i = 0; i < uSlots; i++) {
        vec4 b = slotBound(i);
        if (gNs < 12 && rayBall(ro, rd, b, tmax) > 0.5) { gSl[gNs] = i; gSb[gNs] = b; gNs++; }
    }
    gNsp = 0;
    for (int k = 0; k < uSprouts; k++) {
        vec4 sp = uSp[k];
        if (sp.w <= 0.001) continue;
        if (rayBall(ro, rd, vec4(sp.x, sp.y + 0.035, sp.z, 0.065), tmax) > 0.5) {
            if (gNsp < 12) { gSpL[gNsp] = k; gNsp++; }
        }
    }
    gBug = rayBall(ro, rd, vec4(uBug.xyz + vec3(0.0, 0.04, 0.0), 0.125 * BS), tmax) > 0.5;
}

// the beetle, in its own frame (forward +z), unit ~ 1/BS
float beetle(vec3 p, inout vec4 inf, inout float m) {
    vec3 q = p - uBug.xyz;
    q.xz = rot(-uBug.w) * q.xz;
    q /= BS;
    float mv = uBug2.y;
    q.y -= 0.0012 * mv * abs(sin(uBug2.x * 2.0));
    float d = 1e3;
    vec3 qs = q - vec3(0.0, 0.028, -0.006);
    float shell = max(sdEll(qs, vec3(0.041, 0.031, 0.05)), -(q.y - 0.012));
    d = shell; m = 15.0; inf = vec4(qs, 0.0);
    float under = sdEll(q - vec3(0.0, 0.017, 0.0), vec3(0.034, 0.013, 0.046));
    if (under < d) { d = under; m = 16.0; }
    // the head turns to look around
    vec3 hp = q - vec3(0.0, 0.021, 0.04);
    hp.xz = rot(-uBug2.z) * hp.xz;
    vec3 hq = hp - vec3(0.0, 0.004, 0.019);
    float head = sdEll(hq, vec3(0.031, 0.027, 0.025));
    if (head < d) { d = head; inf = vec4(hq, 0.0); m = 17.0; }
    // antennae
    for (int s = -1; s <= 1; s += 2) {
        float fs = float(s);
        float wig = 0.004 * sin(uTime * 2.3 + fs) + 0.003 * sin(uTime * 5.1 + fs * 2.0);
        vec3 a0 = vec3(fs * 0.01, 0.02, 0.008), a1 = vec3(fs * 0.024 + wig, 0.042, 0.022);
        float da = min(sdCap(hq, a0, a1, 0.0026), length(hq - a1) - 0.0062);
        if (da < d) { d = da; m = 16.0; }
    }
    // six legs, a tripod gait
    // (front left, middle right and back left swing together, then the other three; a foot on the ground moves back
    // at the body's speed, so it stays planted; the feet fan out, 0.045 apart, and never swing more than 0.009)
    for (int k = 0; k < 3; k++) {
        float fk = float(k);
        float zk = 0.02 - 0.02 * fk;
        float fz = k == 0 ? 0.046 : (k == 1 ? 0.0 : -0.044);
        for (int s = -1; s <= 1; s += 2) {
            float fs = float(s);
            float c = fract(uBug2.x / TAU + (((k == 1) == (s > 0)) ? 0.0 : 0.5));
            float A = 0.009 * mv;
            float off, lift;
            if (c < 0.5) { off = A - 4.0 * A * c; lift = 0.0; }
            else { float u = (c - 0.5) * 2.0; off = -A + 2.0 * A * smoothstep(0.0, 1.0, u); lift = sin(PI * u) * 0.007 * mv; }
            vec3 hip = vec3(fs * 0.026, 0.015, zk);
            vec3 foot = vec3(fs * (k == 1 ? 0.066 : 0.06), 0.0015 + lift, fz + off);
            vec3 knee = vec3(fs * 0.052, 0.028 + lift * 0.6, mix(zk, foot.z, 0.55));
            float dl = min(sdCap(q, hip, knee, 0.005), sdCap(q, knee, foot, 0.0042));
            if (dl < d) { d = dl; m = 16.0; }
        }
    }
    return d * BS;
}

float sprouts(vec3 p, float dIn, inout vec4 inf, inout float m) {
    float d = 1e3;
    float dCut = dIn;
    for (int j = 0; j < gNsp; j++) {
        int k = gSpL[j];
        vec4 sp = uSp[k];
        if (length(p - sp.xyz - vec3(0.0, 0.035, 0.0)) - 0.065 > min(d, dCut)) continue;
        float g = sp.w;
        float sd = hash1(float(k) * 7.31 + floor(sp.x * 97.0));
        vec3 q = p - sp.xyz;
        q.xz = rot(sd * 6.28) * q.xz;
        float gg = smoothstep(0.0, 1.0, g);
        float ds;
        if (uKind == 1) {
            // a little round succulent pup
            float R = 0.02 * gg * (0.8 + 0.5 * sd);
            float a = atan(q.z, q.x);
            ds = length(q - vec3(0.0, R * 0.7, 0.0)) - R + R * 0.08 * (0.5 + 0.5 * cos(a * 8.0));
            if (ds < d) { d = ds; m = 14.0; inf = vec4(0.5 + 0.5 * cos(a * 8.0), 0.5, a, 0.0); }
            continue;
        }
        int ty = sd < 0.45 ? 0 : (sd < 0.75 ? 1 : 2);
        float H = 0.06 * gg * (0.75 + 0.5 * fract(sd * 13.0));
        if (ty == 1) {
            // a tuft of grass
            for (int b = 0; b < 3; b++) {
                float fb = float(b);
                float ab = fb * 2.1 + sd * 3.0;
                vec3 tip = vec3(cos(ab) * 0.022, H * (1.1 + 0.2 * fb), sin(ab) * 0.022) * (0.4 + 0.6 * gg);
                ds = sdCapT(q, vec3(0.0), tip, 0.0035, 0.0008);
                if (ds < d) { d = ds; m = 18.0; inf = vec4(q.y / max(H, 1e-3), 0.0, 0.0, 0.0); }
            }
            continue;
        }
        vec3 top = vec3(0.004, H, 0.0);
        ds = sdCap(q, vec3(0.0), top, 0.0024);
        if (ds < d) { d = ds; m = 5.0; inf = vec4(0.0); }
        vec3 ql = q - top;
        float L = 0.016 * smoothstep(0.2, 1.0, g);
        if (L < 0.002) continue;
        if (ty == 0) {
            // two seed leaves
            vec3 l2 = vec3(abs(ql.x) - L * 0.9, ql.y - abs(ql.x) * 0.3, ql.z);
            ds = sdEll(l2, vec3(L, 0.0028, L * 0.62));
        } else {
            // a clover
            float a = atan(ql.z, ql.x);
            float ak = floor(a / (TAU / 3.0) + 0.5) * (TAU / 3.0);
            vec2 lq = rot(ak) * ql.xz;
            ds = sdEll(vec3(lq.x - L * 0.75, ql.y - length(ql.xz) * 0.25, lq.y), vec3(L * 0.8, 0.0026, L * 0.7));
        }
        if (ds < d) { d = ds; m = 18.0; inf = vec4(1.0, 0.0, 0.0, 0.0); }
    }
    return d;
}

float mapC(vec3 p) {
    float d = (p.y - soilH(p.xz)) * 0.8;
    gMat = 1.0; gInf = vec4(0.0);
    // the pond's water
    if (uPond > 0.001) {
        float dw = max(p.y - uPondY, length(p.xz - uPondP.xy) - uPondP.z * 1.1);
        take(d, dw, 12.0, vec4(0.0));
    }
    for (int j = 0; j < gNs; j++) {
        // (nothing in a slot can be nearer than its bound)
        if (length(p - gSb[j].xyz) - gSb[j].w > d) continue;
        int i = gSl[j];
        vec4 A = uSA[i], Bv = uSB[i];
        int t = int(Bv.x + 0.5);
        float s = A.w, g = Bv.y, seed = Bv.z, rt = Bv.w;
        vec3 B = A.xyz;
        vec4 inf = vec4(0.0);
        float m = 0.0;
        float ds = 1e3;
        if (t == 0) {
            float r = max(s * g, 0.004);
            vec3 q = p - (B - vec3(0.0, 0.25 * r, 0.0));
            ds = sdEll(q, vec3(r, 0.55 * r, r * 0.9));
            if (ds < 0.05) ds += r * 0.16 * (vnoise3(q / max(r, 1e-3) * 3.5 + seed * 10.0) - 0.5);
            // moss melts into the soil
            float h = sat(0.5 + 0.5 * (d - ds) / 0.02);
            float dm = mix(d, ds, h) - 0.02 * h * (1.0 - h);
            if (ds < d + 0.004) { gMat = 3.0; gInf = vec4(q / max(r, 1e-3), seed); }
            d = dm;
            continue;
        }
        else if (t == 1) { m = 4.0; ds = fern(p, B, s, g, seed, rt, inf); }
        else if (t == 2) ds = mushrooms(p, B, s, g, seed, inf, m);
        else if (t == 3) ds = sapling(p, B, s, g, seed, uSC[i].x, inf, m);
        else if (t == 4) ds = flowers(p, B, s, g, seed, uSC[i].x, inf, m);
        else if (t == 5) ds = rosette(p, B, s, g, seed, rt, uSC[i].x, inf, m);
        else if (t == 6) ds = cactus(p, B, s, g, seed, inf, m);
        else if (t == 8) {
            // a mossy rock, a thin waterfall trickling from its lip into the pond
            vec3 q = p - B;
            q.xz = rot(rt) * q.xz;
            float r = s * (0.35 + 0.65 * smoothstep(0.0, 1.0, g));
            ds = sdEll(q - vec3(0.0, 0.12 * r, 0.0), vec3(r, 0.78 * r, 0.85 * r));
            if (ds < 0.06) ds += r * 0.12 * (vnoise3(q / r * 3.0 + seed) - 0.5);
            m = 21.0; inf = vec4(q / max(r, 1e-3), seed);
            if (uSC[i].x > 0.5 && uPond > 0.3 && g > 0.5) {
                vec2 dir = normalize(uPondP.xy - B.xz);
                vec3 a = B + vec3(dir.x * r * 0.62, r * 0.72, dir.y * r * 0.62);
                vec3 b = vec3(B.x + dir.x * (r * 0.95 + 0.035), uPondY, B.z + dir.y * (r * 0.95 + 0.035));
                vec3 pa = p - a, ba = b - a;
                float h = sat(dot(pa, ba) / dot(ba, ba));
                vec3 cc = pa - ba * h;
                vec3 side = normalize(cross(ba, vec3(0.0, 1.0, 0.0)));
                float across = dot(cc, side);
                float dw = length(vec2(across * 0.4, length(cc - side * across))) - 0.005 * smoothstep(0.5, 0.9, g);
                if (dw < ds) { ds = dw; m = 22.0; inf = vec4(h, across, 0.0, 0.0); }
            }
        }
        else {
            vec3 q = p - B;
            q.xz = rot(rt) * q.xz;
            float r = s * (0.3 + 0.7 * smoothstep(0.0, 1.0, g));
            ds = sdEll(q - vec3(0.0, 0.1 * r, 0.0), vec3(r, 0.62 * r, 0.8 * r)) + 0.006 * (vnoise3(q * 30.0) - 0.5);
            m = 11.0; inf = vec4(q / max(r, 1e-3), seed);
        }
        take(d, ds, m, inf);
    }
    if (gNsp > 0) {
        vec4 inf = vec4(0.0); float m = 0.0;
        float ds = sprouts(p, d, inf, m);
        take(d, ds, m, inf);
    }
    if (gBug && length(p - uBug.xyz - vec3(0.0, 0.04, 0.0)) - 0.125 * BS < d) {
        vec4 inf = vec4(0.0); float m = 0.0;
        float db = beetle(p, inf, m);
        take(d, db, m, inf);
    }
    // the cork, seen through the neck
    if (p.y > JHN - 0.15) take(d, cork(p), 19.0, vec4(0.0));
    if (uTreeG > 0.001 && length(p.xz) < 0.3) { float tm; vec4 ti; float dt = treeD(p, tm, ti); take(d, dt, tm, ti); }
    return d;
}

float cav(vec3 p) { return -(jarOuter(p) + JT); }

vec3 nrmC(vec3 p) {
    const vec2 k = vec2(1.0, -1.0);
    const float h = 0.0008;
    return normalize(k.xyy * mapC(p + k.xyy * h) + k.yyx * mapC(p + k.yyx * h) + k.yxy * mapC(p + k.yxy * h) + k.xxx * mapC(p + k.xxx * h));
}

float softShadow(vec3 p, vec3 L) {
    float res = 1.0, t = 0.012;
    for (int i = 0; i < 16; i++) {
        vec3 q = p + L * t;
        if (q.y > JHN - 0.16 || length(q.xz) > 1.0) break;
        float h = mapC(q);
        res = min(res, 9.0 * h / t);
        if (res < 0.02) break;
        t += clamp(h, 0.012, 0.1);
    }
    return smoothstep(0.05, 0.7, res);
}

float ao(vec3 p, vec3 n) {
    float o = 0.0, w = 1.0;
    for (int i = 1; i <= 4; i++) {
        float h = 0.012 * float(i * i);
        o += w * max(h - mapC(p + n * h), 0.0);
        w *= 0.6;
    }
    return sat(1.0 - 4.5 * o);
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
        float cl = smoothstep(0.58, 0.6, n) * band;
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
    float day = 1.0 - uNight;
    vec3 hl = clamp((uSunCol * 0.35 + uAmbS * 1.1 + uFill * 0.3) / 1.55, 0.0, 1.3);
    vec3 far = mix(uHill2 * hl, uSkyH, 0.4);
    far = mix(far, uSkyH * 0.45 + uSkyZ * 0.35, uNight * 0.7);
    c = mix(c, far, smoothstep(0.012, -0.012, E - h1));
    vec3 near = mix(uHill, uHill * 0.6 + uSkyZ * 0.12, smoothstep(0.0, -0.3, E - h2));
    near *= hl * (1.0 - 0.35 * uRain);
    near = mix(near, uSkyZ * 0.5 + uSkyH * 0.1, uNight * 0.75);
    float nm = smoothstep(0.012, -0.012, E - h2 - tree);
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

// ---------------------------------------------------------------- painted greenery (the room being reclaimed)
// The jar's life spills out over flat surfaces (the sill, the wall, the window's frame and panes, the jar's own glass):
// there it is painted as toon foliage, cheap however much of it there is: pointed leaves on jittered grids, each
// drawn if the surface's coverage there beats its own random threshold, a darker rim and a pale midrib, flowers in
// some cells; vines are strands with leaves on alternate sides.

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

// the coverage of greenery: on the sill (mode 0, q = x, z) and on the window wall (mode 1, q = x, y)
float covAt(vec2 q, int mode) {
    float n = vnoise(q * 2.1) * 0.65 + vnoise(q * 5.3) * 0.35;
    if (mode == 0) {
        float r = length(q);
        return smoothstep(0.0, 0.3, uSillR - r - (n - 0.5) * 0.7) * smoothstep(0.92, 1.0, r);
    }
    q.x -= WCX;
    float dOpen = sdBox2(q - vec2(0.0, WY * 0.5), vec2(WX, WY * 0.5));
    // up the casing and the mullions, then out over the wall in clumps
    float casing = step(-0.03, dOpen) * step(dOpen, 0.2);
    float mull = smoothstep(0.12, 0.05, abs(abs(q.x) - 1.62)) * step(dOpen, 0.0);
    float climb = smoothstep(0.0, 0.35, uClimb - q.y - (n - 0.5) * 0.4) * max(casing, mull);
    float top = casing * step(WY - 0.05, q.y) * smoothstep(0.0, 0.4, uClimb - WY - abs(q.x) * 0.6);
    float clump = smoothstep(0.25, 0.65, vnoise(q * 0.9 + 3.0) * 0.7 + vnoise(q * 2.3) * 0.3 + 0.25 * smoothstep(1.2, 0.0, dOpen));
    float wall = step(0.0, dOpen) * smoothstep(0.0, 0.4, uWallR - dOpen - (n - 0.5) * 1.6) * clump;
    return max(max(climb, top), wall);
}
// the clears' blooms near a point of a surface
float bloomAt(vec3 p, int mode) {
    float b = 0.0;
    for (int i = 0; i < uBlooms; i++) {
        vec4 bl = uBloom[i];
        bool onSill = bl.y < 0.05;
        if ((mode == 0) != onSill) continue;
        vec2 d = mode == 0 ? p.xz - bl.xz : p.xy - bl.xy;
        b = max(b, bl.w * smoothstep(0.32, 0.05, length(d)));
    }
    return b;
}
// (a cheap test first: nothing can grow within reach of this point)
bool bareAt(vec2 q, int mode, float bloom) {
    if (bloom > 0.001) return false;
    if (mode == 0) return length(q) > uSillR + 0.5;
    q.x -= WCX;
    float dOpen = sdBox2(q - vec2(0.0, WY * 0.5), vec2(WX, WY * 0.5));
    bool nearFrame = (dOpen > -0.3 && dOpen < 0.5) || (dOpen < 0.0 && abs(abs(q.x) - 1.62) < 0.45);
    bool climbing = nearFrame && q.y < uClimb + 0.5 && uClimb > 0.0;
    bool topping = nearFrame && q.y > WY - 0.5 && uClimb > WY;
    return !climbing && !topping && dOpen > uWallR + 1.0;
}
vec4 leafMass(vec2 q, int mode, float cs, float bloom, vec3 c1, vec3 c2) {
    vec4 res = vec4(0.0);
    if (bareAt(q, mode, bloom)) return res;
    float best = -1.0;
    for (int l = 0; l < 2; l++) {
        float sz = cs * (l == 0 ? 1.0 : 0.7);
        float aa = uPx * 6.0 / sz;
        vec2 g = q / sz + float(l) * 0.37;
        vec2 gi = floor(g);
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                vec2 id = gi + vec2(float(i), float(j));
                vec2 h2 = hash2(id + float(l) * 31.7);
                vec2 c = (id + 0.5 + (h2 - 0.5) * 0.7 - float(l) * 0.37) * sz;
                float cov = covAt(c, mode) + bloom;
                float h = hash(id * 1.37 + float(l) * 7.1);
                if (h * 0.98 > cov) continue;
                float z = hash(id + 91.3 + float(l));
                if (z < best) continue;
                vec2 lc = rot(h2.x * 6.2831) * (q - c);
                bool fl = hash(id + 5.5 + float(l)) < (0.1 + 0.6 * bloom) * uBloomAmt;
                vec4 lf;
                if (fl) {
                    float pick = hash(id + 2.2);
                    vec3 pc = pick < 0.45 ? uFlower : (pick < 0.75 ? uCap : uFlower2);
                    lf = flowerShape(lc / (sz * 0.42), pc, aa);
                } else {
                    vec3 a1 = mix(uLeaf, uMoss, h2.y), a2 = mix(uLeaf2, uMoss2, h2.y);
                    lf = leafShape(lc / vec2(sz * 0.85, sz * 0.4), a1 * c1, a2 * c2, aa);
                }
                if (lf.a > 0.5) { best = z; res = lf; res.rgb *= 0.7 + 0.45 * sat(cov - h) + 0.15 * z; }
            }
    }
    return res;
}
// vines: strands along v from v0 (dir +1 up, -1 down), lane apart across u, each up to len long, leaves on alternate
// sides, flowers at some of them
vec4 vines(vec2 uv, float lane, float v0, float dir, float len, float leafS, float seed, float flAmt) {
    vec4 res = vec4(0.0);
    if (len <= 0.005) return res;
    float aa = uPx * 6.0 / leafS;
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
        if (abs(du) < 0.006 && sAl >= 0.0 && sAl <= L) res = vec4(mix(uStem, uLeaf2, 0.5), 1.0);
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
            if (lf.a > 0.5) res = lf;
        }
    }
    return res;
}

// the sill: painted wood, the jar's shadow and caustic, moss, leaves and flowers spreading from the jar
vec3 sillColor(vec3 p, vec3 rd) {
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
    if (uSillR > 1.0 || uBlooms > 0) {
        float bloom = bloomAt(p, 0);
        float cov = covAt(p.xz, 0);
        // a carpet of moss under the leaves
        vec3 moss = mix(uMoss2, uMoss, smoothstep(0.3, 0.8, vnoise(p.xz * 30.0)) * 0.7 + 0.3 * vnoise(p.xz * 90.0));
        col = mix(col, moss * paintLight(p, 1.0) * (0.65 + 0.35 * sh), smoothstep(0.35, 0.6, cov));
        vec4 lm = leafMass(p.xz, 0, 0.12, bloom, vec3(1.0), vec3(1.0));
        if (lm.a > 0.5) col = lm.rgb * paintLight(p, 1.0) * (0.7 + 0.3 * sh);
        // vines running out from the jar's foot
        vec2 pol = vec2(atan(p.x, p.z) * 1.0, length(p.xz));
        vec4 vn = vines(pol, 0.42, 0.95, 1.0, uSillR - 0.95, 0.05, 3.0, 0.25 * uBloomAmt);
        if (vn.a > 0.5) col = vn.rgb * paintLight(p, 1.0);
    }
    return col;
}

// the window wall, the panes and the sky through them, the frame; the sill and its front below
vec3 background(vec3 ro, vec3 rd, out float tHit) {
    tHit = 1e4;
    if (rd.y < 0.0) {
        float ts = -ro.y / rd.y;
        vec3 p = ro + rd * ts;
        if (p.z <= SF && p.z >= ZW) { tHit = ts; return sillColor(p, rd); }
        if (p.z > SF && rd.z < 0.0) {
            // the front of the sill: a painted panel, its rounded nose, vines draping over it
            float tf = (SF - ro.z) / rd.z;
            vec3 pf = ro + rd * tf;
            tHit = tf;
            vec3 alb = uSill * mix(0.62, 0.95, smoothstep(-0.1, -0.02, pf.y));
            vec3 col = alb * (uAmbS * 0.7 + uFill * 0.75);
            vec4 vn = vines(pf.xy, 0.3, 0.0, -1.0, uDrape, 0.055, 9.0, 0.3 * uBloomAmt);
            if (vn.a > 0.5) col = vn.rgb * paintLight(pf, 0.3);
            return col;
        }
    }
    float tw = (ZW - ro.z) / rd.z;
    vec3 W = ro + rd * tw;
    vec3 col;
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
        tHit = tw;
        // the wall: soft plaster lit by the room
        col = uWall * (0.92 + 0.1 * vnoise(W.xy * 3.0)) * (uAmbS * 0.75 + uFill * 0.8 + uSunCol * 0.04);
    }
    // the frame: the casing round the opening, a bottom rail, two mullions, a transom (painted wood)
    float blur = 0.012;
    float fx = abs(abs(Wq.x) - 1.62) - 0.05;
    float fr = min(min(fx, W.y - 0.09), abs(W.y - 1.5) - 0.045);
    fr = dOpen < 0.0 ? fr : abs(dOpen - 0.07) - 0.08;
    float fm = smoothstep(blur, -blur, fr);
    vec3 frameC = uFrame * (uFill * 0.9 + uAmbS * 0.8 + uSunCol * 0.08);
    frameC *= 1.0 - smoothstep(-0.05, 0.0, fr) * 0.2;
    col = mix(col, frameC, fm);
    if (dOpen < 0.0) tHit = fm > 0.5 ? tw : tHit;
    // the greenery: up the frame, over the wall, vines hanging over the panes
    if (uClimb > 0.0 || uWallR > 0.0 || uBlooms > 0) {
        float bloom = bloomAt(W, 1);
        vec4 lm = leafMass(W.xy, 1, 0.19, bloom, vec3(1.0), vec3(1.0));
        if (lm.a > 0.5) { col = lm.rgb * paintLight(W, 0.7); tHit = tw; }
    }
    if (uHang > 0.0) {
        vec4 vn = abs(Wq.x) < WX + 0.2 ? vines(W.xy, 0.36, WY + 0.1, -1.0, uHang * 1.9, 0.075, 5.0, 0.3 * uBloomAmt) : vec4(0.0);
        if (vn.a > 0.5) { col = vn.rgb * paintLight(W, 0.5); tHit = tw; }
    }
    return col;
}

// ---------------------------------------------------------------- the things outside the glass: the jar, the cork, the tree
float objMap(vec3 p, bool withJar, out float id, out float m, out vec4 inf) {
    float d = 1e3;
    id = 0.0; m = 0.0; inf = vec4(0.0);
    if (withJar) { float dj = jarOuter(p); if (dj < d) { d = dj; id = 1.0; } }
    float dk = cork(p);
    if (dk < d) { d = dk; id = 2.0; }
    if (uTreeG > 0.001) {
        float tm; vec4 ti;
        float dt = treeD(p, tm, ti);
        if (dt < d) { d = dt; id = 3.0; m = tm; inf = ti; }
    }
    return d;
}
bool objMarch(vec3 ro, vec3 rd, bool withJar, out float t, out float id, out float m, out vec4 inf) {
    vec3 bmin = uTreeG > 0.0 ? vec3(-2.7, -0.05, -2.25) : vec3(-2.0, -0.05, -1.1);
    vec3 bmax = uTreeG > 0.0 ? vec3(2.7, 3.2, 1.4) : vec3(2.0, 1.45, 1.4);
    vec3 inv = 1.0 / rd;
    vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
    vec3 tn = min(t0, t1), tf = max(t0, t1);
    float tin = max(max(tn.x, tn.y), tn.z), tout = min(min(tf.x, tf.y), tf.z);
    t = 0.0; id = 0.0; m = 0.0; inf = vec4(0.0);
    if (tout < max(tin, 0.0)) return false;
    t = max(tin, 0.0);
    for (int i = 0; i < 110; i++) {
        vec3 p = ro + rd * t;
        float d = objMap(p, withJar, id, m, inf);
        if (d < 0.0004 + t * 0.0002) return true;
        t += d * 0.9;
        if (t > tout) break;
    }
    return false;
}
vec3 objNormal(vec3 p) {
    const vec2 e = vec2(0.0015, 0.0);
    float i, m; vec4 f;
    return normalize(vec3(objMap(p + e.xyy, false, i, m, f) - objMap(p - e.xyy, false, i, m, f),
                          objMap(p + e.yxy, false, i, m, f) - objMap(p - e.yxy, false, i, m, f),
                          objMap(p + e.yyx, false, i, m, f) - objMap(p - e.yyx, false, i, m, f)));
}
vec3 toonOut(vec3 p, vec3 n, vec3 rd, vec3 alb, float trans) {
    float ndl = dot(n, uSunDir);
    float band = smoothstep(-0.05, 0.12, ndl) * 0.78 + smoothstep(0.5, 0.62, ndl) * 0.22;
    vec3 c = alb * (uSunCol * band + mix(uAmbG, uAmbS, n.y * 0.5 + 0.5) + uFill * 0.55 * sat(dot(n, normalize(vec3(0.3, 0.45, 1.0))) * 0.6 + 0.4));
    c += alb * uSunCol * trans * sat(-ndl * 0.8 + 0.25) * 0.5;
    c += mix(uSunCol, uGlowSun, 0.5) * pow(1.0 - sat(dot(n, -rd)), 3.0) * 0.25;
    vec3 dj = p - vec3(0.0, 0.6, 0.0);
    c += alb * uFlyC * uFlyAmt * 0.5 * exp(-dot(dj, dj) / 1.2);
    return c;
}
vec3 shadeObj(vec3 p, vec3 rd, float id, float m, vec4 inf) {
    vec3 n = objNormal(p);
    if (id < 2.5) {
        // the cork
        float sp = vnoise3(p * 180.0);
        vec3 alb = mix(vec3(0.62, 0.42, 0.24), vec3(0.8, 0.6, 0.38), sp);
        alb = mix(alb, vec3(0.32, 0.2, 0.1), step(0.86, hash3(floor(p * 220.0))));
        return toonOut(p, n, rd, alb, 0.0);
    }
    if (m < 4.0) {
        // the crown: leaves, flowers dotting it
        vec3 q = inf.xyz;
        vec3 pn = p * 26.0;
        n = normalize(n + (vec3(vnoise3(pn), vnoise3(pn + 5.2), vnoise3(pn + 9.7)) - 0.5) * 0.9);
        float lv = vnoise3(p * 34.0);
        vec3 alb = mix(uLeaf2, uLeaf, smoothstep(-0.6, 0.7, q.y) * 0.6 + 0.4 * smoothstep(0.35, 0.75, lv));
        alb *= 0.85 + 0.35 * smoothstep(0.55, 0.8, vnoise3(p * 70.0));
        float fl = step(1.0 - 0.1 * uBloomAmt, hash3(floor(p * 40.0))) * smoothstep(-0.2, 0.4, q.y);
        alb = mix(alb, uFlower * 1.1, fl);
        return toonOut(p, n, rd, alb, 0.7) + alb * (uAmbS * 0.35 + uFill * 0.3);
    }
    float st = vnoise(vec2(atan(p.x, p.z) * 8.0, p.y * 40.0));
    return toonOut(p, n, rd, uBark * (0.75 + 0.45 * st), 0.0);
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

// ---------------------------------------------------------------- shading the little world
vec3 flies(vec3 p, vec3 n) {
    vec3 c = vec3(0.0);
    for (int i = 0; i < uFlies; i++) {
        vec4 f = uFly[i];
        if (f.w <= 0.01) continue;
        vec3 L = f.xyz - p;
        float d2 = dot(L, L);
        float w = sat(dot(n, L * inversesqrt(d2 + 1e-6)) * 0.6 + 0.4);
        c += f.w * w * (exp(-d2 / 0.01) * 0.7 + 0.00012 / (d2 + 0.002));
    }
    return c * uFlyC;
}

vec3 shade(vec3 p, vec3 rd, float m, vec4 inf) {
    vec3 n = nrmC(p);
    vec3 alb = uSoil;
    float trans = 0.0, spec = 0.0, rim = 1.0, glow = 0.0;
    float edgeW = cav(p);
    if (m < 1.5) {
        if (edgeW < 0.012 && p.y < soilH(p.xz) - 0.003) {
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
                float stone = smoothstep(0.5, 0.42, r);
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
            return sc + alb * flies(p, normalize(vec3(p.x, 0.0, p.z))) * 0.6;
        } else {
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
        }
    } else if (m < 3.2) {
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
        float wart = smoothstep(0.3, 0.2, length(fract(sp * 1.6) - hash2(si) * 0.5 - 0.25)) * step(0.0, q.y);
        alb = mix(uCap, uCap * 1.15, sat(q.y));
        alb = mix(alb, uCap2, wart * (uKind == 0 ? 1.0 : 0.0));
        if (q.y < 0.02) {
            float gill = 0.5 + 0.5 * sin(atan(q.z, q.x) * 40.0);
            alb = mix(uCap2 * 0.85, uCap2 * 0.6, gill);
        }
        spec = 0.5;
        if (uKind == 2) glow = 0.0;
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
        float rr = length(q);
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
            float pad = smoothstep(pr, pr - 0.002, length(dq)) * (1.0 - notch * step(length(dq), pr)) * smoothstep(0.55, 0.85, uPond);
            vec3 pc = mix(uMoss, uMoss2, length(dq) / pr) * (uAmbS * 0.8 + uSunCol * 0.7 * sat(uSunDir.y));
            col = mix(col, pc, pad);
        }
        col += flies(p + vec3(0.0, 0.02, 0.0), vec3(0.0, 1.0, 0.0)) * 0.5;
        return col;
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
                sp = max(sp, smoothstep(0.24, 0.2, length(vec3(abs(dir.x), dir.y, dir.z) - s)));
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
        vec3 hq = inf.xyz;
        vec3 dir = normalize(hq / vec3(0.031, 0.027, 0.025));
        alb = vec3(0.09, 0.08, 0.11);
        float face = smoothstep(0.05, 0.2, dir.z) * smoothstep(0.75, 0.5, dir.y);
        alb = mix(alb, uCap2 * vec3(1.0, 0.94, 0.88), face);
        float blink = uBug2.w;
        float eye = 0.0, shine = 0.0;
        for (int s = -1; s <= 1; s += 2) {
            vec3 e = normalize(vec3(float(s) * 0.4, 0.12, 0.9));
            vec3 dd = dir - e;
            dd.y /= max(1.0 - blink, 0.06);
            eye = max(eye, smoothstep(0.235, 0.205, length(dd * vec3(1.0, 0.8, 1.0))));
            shine = max(shine, smoothstep(0.075, 0.05, length(dd - vec3(-0.06, 0.08, 0.0))));
            shine = max(shine, smoothstep(0.04, 0.02, length(dd - vec3(0.06, -0.08, 0.0))));
            float bl = smoothstep(0.15, 0.06, length((dir - normalize(vec3(float(s) * 0.62, -0.2, 0.75))) * vec3(1.0, 1.4, 1.0)));
            alb = mix(alb, vec3(1.0, 0.45, 0.55), bl * 0.8 * face);
        }
        alb = mix(alb, vec3(0.03, 0.025, 0.05), eye);
        alb = mix(alb, vec3(1.0), shine * eye * (1.0 - blink));
        vec2 mq = vec2(dir.x, dir.y + 0.3);
        float smile = smoothstep(0.02, 0.0, abs(length(mq - vec2(0.0, 0.06)) - 0.065)) * step(mq.y, 0.04) * step(abs(mq.x), 0.06);
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
    else if (m > 21.5) {
        // the trickle: bright, clear water running down
        float st = smoothstep(0.4, 0.9, vnoise(vec2(inf.y * 300.0, inf.x * 9.0 - uTime * 3.0)));
        vec3 wc = mix(uWater * 1.3, vec3(0.92, 0.98, 1.0), 0.45 + 0.4 * st);
        return wc * (uAmbS * 1.1 + uFill * 0.9 + uSunCol * 0.35) + uFlyC * uFlyAmt * 0.1;
    }
    else if (m < 18.5) { alb = mix(uLeaf2, uLeaf * 1.15, 0.5 + 0.5 * inf.x); alb = mix(alb, vec3(0.75, 1.0, 0.45), 0.25); trans = 0.8; }
    else {
        float sp = vnoise3(p * 180.0);
        alb = vec3(0.72, 0.5, 0.3) * (0.8 + 0.35 * sp);
        alb = mix(alb, vec3(0.35, 0.22, 0.12), step(0.85, hash3(floor(p * 220.0))));
        rim = 0.5;
    }
    float sh = softShadow(p + n * 0.004, uSunDir);
    float o = ao(p, n);
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
    return col;
}

void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 rd = normalize(uFwd + uRight * ndc.x * uTanH * uAspect + uUp * ndc.y * uTanH);
    vec3 ro = uEye;
    vec3 col;
    float tTot = 1e4;
    float t, oid, om; vec4 oinf;
    bool hitObj = objMarch(ro, rd, true, t, oid, om, oinf);
    if (hitObj && oid > 1.5) {
        col = shadeObj(ro + rd * t, rd, oid, om, oinf);
        tTot = t;
    } else if (hitObj) {
        vec3 p0 = ro + rd * t;
        vec3 N = jarNormal(p0);
        float cosv = sat(dot(N, -rd));
        bool mouth = distance(uCorkP, vec3(0.0, JHN + 0.02, 0.0)) > 0.15 && p0.y > JHN - 0.04 && length(p0.xz) < JRN - 0.012;
        // the condensation where the ray meets the glass
        vec2 uv = vec2(atan(p0.x, p0.z) * JR, p0.y);
        float cov = uCond * (0.15 + 0.85 * smoothstep(0.45, 0.8, p0.y)) * smoothstep(0.36, 0.5, p0.y);
        vec4 dr = drops(uv, cov);
        vec3 tng = normalize(cross(vec3(0.0, 1.0, 0.0), N));
        vec3 rdIn = normalize(refract(rd, N, 0.94) + (tng * dr.x + vec3(0.0, 1.0, 0.0) * dr.y) * 0.06 * dr.z);
        if (mouth) { rdIn = rd; dr = vec4(0.0); cov = 0.0; }
        vec3 pIn = p0 + rdIn * (JT * 1.6);
        // ---- inside: march the little world until the far wall
        float ti = 0.0;
        float tmax = 3.0;
        buildMasks(pIn, rdIn, tmax);
        bool hit = false;
        float hm = 0.0; vec4 hinf = vec4(0.0);
        for (int i = 0; i < 96; i++) {
            vec3 p = pIn + rdIn * ti;
            float dc = mapC(p);
            float dw = cav(p);
            if (dc < 0.0005 + ti * 0.0008) { hit = true; hm = gMat; hinf = gInf; break; }
            if (dw < 0.0008 && i > 2) break;
            ti += max(min(dc * 0.75, max(dw, 0.0015)), 0.0006);
            if (ti > tmax) break;
        }
        vec3 pEnd = pIn + rdIn * ti;
        vec3 inner;
        tTot = t + ti;
        if (hit) inner = shade(pEnd, rdIn, hm, hinf);
        else {
            // through the far wall: the vines pressed against it, the tree behind, the window and the sill, the
            // wall's own faint reflection and mist
            vec3 NB = -jarNormal(pEnd);
            float tb;
            inner = background(pEnd, rdIn, tb);
            float t2, i2, m2; vec4 f2;
            if (uTreeG > 0.0 && objMarch(pEnd + rdIn * 0.03, rdIn, false, t2, i2, m2, f2) && t2 < tb) {
                inner = shadeObj(pEnd + rdIn * (0.03 + t2), rdIn, i2, m2, f2);
                tb = t2;
            }
            tTot = t + ti + tb;
            float fb = 0.04 + 0.5 * pow(1.0 - sat(dot(-NB, rdIn)), 5.0);
            inner = mix(inner, env(reflect(rdIn, NB)) * 0.6, fb);
            vec2 uvb = vec2(atan(pEnd.x, pEnd.z) * JR, pEnd.y);
            float covb = uCond * (0.15 + 0.85 * smoothstep(0.45, 0.8, pEnd.y)) * smoothstep(0.36, 0.5, pEnd.y) * 0.6;
            vec4 db = drops(uvb, covb);
            vec3 mist = mix(uSkyH, vec3(1.0), 0.4) * (0.75 - 0.5 * uNight) + uAmbS * 0.3;
            inner = mix(inner, mist, db.w * 0.3 + db.z * 0.15);
            vec4 vb = vines(uvb, 0.24, 0.19, 1.0, uInVine * 0.62, 0.04, 1.0, 0.15 * uBloomAmt);
            if (vb.a > 0.5) inner = vb.rgb * paintLight(pEnd, 0.4) * 0.85;
        }
        // the mist inside, low over the soil, thicker in the calm
        {
            float y0 = pIn.y, dy = rdIn.y;
            float H = 0.16;
            float base = 0.22;
            float a = exp(-(y0 - base) / H);
            float integ = abs(dy) > 1e-4 ? a * H * (1.0 - exp(-dy * ti / H)) / dy : a * ti;
            float wisp = 0.6 + 0.8 * vnoise(vec2(pIn.x * 3.0 + uTime * 0.03, pIn.y * 2.0 + uTime * 0.01));
            float fogA = 1.0 - exp(-max(integ, 0.0) * uFog * 1.2 * wisp);
            vec3 fogC = mix(uSkyH, vec3(1.0), 0.45) * (0.85 - 0.65 * uNight) + uAmbS * 0.3 + uFlyC * uFlyAmt * 0.025;
            inner = mix(inner, fogC, fogA * 0.6);
        }
        // vines climbing the inside of the glass, from the soil up
        {
            vec4 vi = vines(uv, 0.24, 0.19, 1.0, uInVine * 0.62, 0.04, 1.0, 0.15 * uBloomAmt);
            if (vi.a > 0.5) inner = vi.rgb * paintLight(p0, 0.5);
        }
        // the glass: tinted, thicker toward its edges, the drops' lenses, its fresnel reflection, a bright rim
        vec3 tint = vec3(0.9, 0.97, 0.95);
        float edge = pow(1.0 - cosv, 2.5);
        inner *= mix(vec3(1.0), tint * tint * 0.85, edge);
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
        float F = mouth ? 0.0 : 0.035 + 0.965 * pow(1.0 - cosv, 5.0);
        vec3 refl = env(reflect(rd, N));
        if (mouth) inner /= mix(vec3(1.0), tint * tint * 0.85, edge);
        col = mix(inner, refl, F);
        if (!mouth) col += mix(uSkyH, vec3(1.0), 0.4) * (1.0 - 0.6 * uNight) * smoothstep(0.16, 0.0, cosv) * 0.35;
        col += uGlowSun * 0.25 * smoothstep(0.995, 1.0, sat(1.0 - abs(p0.y - JHN + 0.01) * 18.0)) * step(length(p0.xz), JRN + 0.03);
        // flowering vines spilling out of the neck and down the outside of the glass
        vec4 vo = vines(uv + vec2(0.11, 0.0), 0.3, JHN + 0.03, -1.0, uSpill * (JHN + 0.05), 0.05, 7.0, 0.35 * uBloomAmt);
        if (vo.a > 0.5) col = vo.rgb * paintLight(p0, N.y * 0.5 + 0.5);
    } else {
        col = background(ro, rd, tTot);
    }
    // the lights in the air: the jar's fireflies, the seeds drifting down, the fireflies leaving through the window
    {
        vec3 glow = vec3(0.0);
        for (int i = 0; i < uFlies; i++) {
            vec4 f = uFly[i];
            if (f.w <= 0.01) continue;
            vec3 v = f.xyz - ro;
            float tc = dot(v, rd);
            if (tc < 0.0 || tc > tTot + 0.01) continue;
            float d2 = dot(v, v) - tc * tc;
            float px2 = sq(uPx * tc * 1.2);
            glow += uFlyC * f.w * (0.00007 / (d2 + 0.000025 + px2) + 0.00028 / (d2 + 0.0008) + 0.0012 / (d2 + 0.006) * 0.1);
        }
        for (int i = 0; i < uSwarm; i++) {
            vec4 f = uSw[i];
            if (f.w <= 0.01) continue;
            vec3 v = f.xyz - ro;
            float tc = dot(v, rd);
            if (tc < 0.0 || tc > tTot + 0.01) continue;
            float d2 = dot(v, v) - tc * tc;
            float px2 = sq(uPx * tc * 1.2);
            glow += uFlyC * f.w * (0.00016 / (d2 + 0.00006 + px2) + 0.0008 / (d2 + 0.0025) + 0.003 / (d2 + 0.015) * 0.1);
        }
        for (int i = 0; i < uSeeds; i++) {
            vec4 sd = uSeedP[i];
            if (sd.w <= 0.01) continue;
            vec3 v = sd.xyz - ro;
            float tc = dot(v, rd);
            if (tc < 0.0 || tc > tTot + 0.01) continue;
            float d2 = dot(v, v) - tc * tc;
            float px2 = sq(uPx * tc * 1.5);
            vec3 sc = mix(vec3(1.0, 0.95, 0.82), uFlyC, 0.2);
            glow += sc * sd.w * (0.00005 / (d2 + 0.00004 + px2) + 0.0002 / (d2 + 0.0008) + 0.0005 / (d2 + 0.004) * 0.15);
        }
        col += glow;
    }
    col *= mix(1.0, 0.85, uSlow);
    col = mix(col, mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * vec3(0.8, 0.9, 1.2), 0.4), uSlow);
    col = mix(uBridgeCol, col, uWorldA);
    frag = vec4(max(col, 0.0), 1.0);
}
)";

inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uWorld, vUv).rgb;
    vec3 n = texture(uWorld, vUv + vec2(uTexel.x, 0.0)).rgb + texture(uWorld, vUv - vec2(uTexel.x, 0.0)).rgb +
             texture(uWorld, vUv + vec2(0.0, uTexel.y)).rgb + texture(uWorld, vUv - vec2(0.0, uTexel.y)).rgb;
    c = max(c + (c - n * 0.25) * 0.2, 0.0);
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ dust, the gauge
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
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 1) {
        // the gauge: a fine ring, the charge an arc of light from the top, clockwise
        float ring = 1.0 - smoothstep(px * 0.6, px * 1.6, abs(r - 0.8));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = (1.0 - smoothstep(px * 1.4, px * 3.0, abs(r - 0.8))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.5 + 0.5 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, v * vB.a * 0.6);
        return;
    }
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
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.15, 0.45, d) * uHaloA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill);
    frag = vec4(c, a) * uAlpha;
}
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

#pragma once
// RUINS' shaders: the world (ancient ruins in a jungle, ray marched at a reduced resolution: the temple of the
// variant, an avenue of columns, fallen drums and blocks, great trunks, roots; the stone carved, jointed, weathered
// and mossy; soft sun shadows dappled by the canopy, ambient occlusion, warm bounce light; foliage as layered
// planes (jungle walls, undergrowth, lianas, the canopy) for rich silhouettes; mist and light shafts integrated
// along each ray), the backing behind the well, the column drum pieces, dust, chips, birds, lettering.

namespace ruins_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float hash1(float n) { return hash(vec2(n, n * 0.371 + 17.13)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
float fbm2(vec2 p) { return (vnoise(p) * 0.66 + vnoise(p * 2.07 + 3.1) * 0.34); }
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float sdBox2(vec2 p, vec2 b) { vec2 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
)";

// ------------------------------------------------------------------------------------------------ the world
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uEye, uFwd, uRight, uUp;
uniform float uTanH, uAspect, uTime, uPx;
uniform int uKind;
uniform vec3 uSunDir, uSunCol, uSkyZ, uSkyH, uAmb, uFog;
uniform vec3 uStone, uStone2, uMoss, uMoss2, uLeaf, uLeafLit, uBark, uGround;
uniform float uFogK, uMist, uShaft, uGap, uSway, uGold, uWorldA;
uniform vec3 uBridgeCol;

const vec3 TC = vec3(0.0, 0.0, -52.0);   // the temple
const float CAN_Y = 31.0;                // the canopy

float sdBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float smin(float a, float b, float k) { float h = sat(0.5 + 0.5 * (b - a) / k); return mix(b, a, h) - k * h * (1.0 - h); }
float sdCap(vec3 p, vec3 a, vec3 b, float ra, float rb) {
    vec3 pa = p - a, ba = b - a;
    float h = sat(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h) - mix(ra, rb, h);
}
// stairs: q.x the run from the foot (inward), q.y the height; n steps of tread tr and rise ri, solid back to uMax
float sdStairs(vec2 q, float tr, float ri, float n, float uMax) {
    float d = 1e9;
    float k0 = floor(q.x / tr), k2 = floor(q.y / ri);
    for (int i = 0; i < 3; i++) {
        float k = clamp(i == 0 ? k0 : (i == 1 ? k0 + 1.0 : k2), 0.0, n - 1.0);
        vec2 lo = vec2(k * tr, 0.0), hi = vec2(uMax, (k + 1.0) * ri);
        d = min(d, sdBox2(q - (lo + hi) * 0.5, (hi - lo) * 0.5));
    }
    return d;
}

// ---- the restoration (time running backwards over the song): the jungle's cover, each avenue column's repair,
// the temple's repair, the share of blocks still missing, the wear (erosion, moss, lichen, streaks), the prime
uniform float uJungle, uTemple, uBroken, uWear, uPrime;
uniform float uCol[8];
float hash3(vec3 p) { p = fract(p * 0.1031); p += dot(p, p.zyx + 31.32); return fract((p.x + p.y) * p.z); }

// missing blocks: a wall's coursed blocks carved away where their hash is under uBroken; a block coming back grows
// from its edges inward out of the dust (k: 1 missing .. 0 whole). The distance of the carved box (or far away).
float missing(vec3 p, out float k) {
    const vec3 cs = vec3(1.25, 0.62, 1.25);
    float row = floor(p.y / cs.y);
    vec3 pp = p + vec3(row * 0.62, 0.0, row * 0.41);
    vec3 c = floor(pp / cs);
    float h = hash3(c + 0.5);
    k = sat((uBroken - h) / 0.015);
    if (k <= 0.0) return 1e9;
    return sdBox(pp - (c + 0.5) * cs, cs * 0.5 * (0.2 + 0.85 * k));
}
float carveMissing(float d, vec3 p) {
    if (d > 0.9 || uBroken <= 0.0) return d;
    float k;
    return max(d, -missing(p, k));
}

// ---- a column standing on y = 0 of q: height h, broken at hb (< h: a break as rough as `rough`, no capital),
// radius r; style 0 a square carved pillar (Maya), 1 an octagonal pillar with lotus rings (Khmer), 2 a fluted
// Doric column
float column(vec3 q, float h, float hb, float r, int style, float seed, float rough) {
    float y = q.y;
    float taper = 1.0 - 0.07 * sat(y / h);
    float d;
    if (style == 0) {
        d = sdBox(q - vec3(0.0, h * 0.5, 0.0), vec3(r * taper, h * 0.5, r * taper * 0.78));
        d = min(d, sdBox(q - vec3(0.0, 0.18, 0.0), vec3(r * 1.25, 0.18, r * 1.05)));
        if (hb >= h) d = min(d, sdBox(q - vec3(0.0, h - 0.2, 0.0), vec3(r * 1.3, 0.2, r * 1.1)));
    } else if (style == 1) {
        float a = atan(q.z, q.x);
        const float seg = 6.2831853 / 8.0;
        float f = cos(mod(a + seg * 0.5, seg) - seg * 0.5);
        float ring = abs(fract(y / (h * 0.25) + 0.5) - 0.5) * h * 0.25;
        d = length(q.xz) * f - r * taper - 0.07 * smoothstep(0.16, 0.05, ring);
        d = max(d, abs(y - h * 0.5) - h * 0.5);
        d = min(d, sdBox(q - vec3(0.0, 0.25, 0.0), vec3(r * 1.3, 0.25, r * 1.3)));
        if (hb >= h) d = min(d, sdBox(q - vec3(0.0, h - 0.15, 0.0), vec3(r * 1.35, 0.15, r * 1.35)));
    } else {
        float a = atan(q.z, q.x);
        float fl = 0.055 * sqrt(abs(sin(a * 10.0)));
        d = (length(q.xz) - r * taper + fl) * 0.9;
        d = max(d, abs(y - h * 0.5) - h * 0.5);
        if (hb >= h) {
            float e = y - (h - 0.62);
            float ech = max(length(q.xz) - (r * 0.95 + sat(e / 0.36) * r * 0.42), abs(e - 0.18) - 0.18);
            d = min(d, ech);
            d = min(d, sdBox(q - vec3(0.0, h - 0.13, 0.0), vec3(r * 1.42, 0.13, r * 1.42)));
        }
    }
    if (hb < h) {
        float cut = hb + rough * (0.45 * (vnoise(q.xz * 2.3 + seed * 13.1) - 0.5) + 0.22 * dot(q.xz, vec2(cos(seed * 7.0), sin(seed * 7.0))));
        d = max(d, (y - cut) * 0.8);
    }
    return d;
}

// a drum lying on the ground (along its local x), length l, radius r
float lyingDrum(vec3 q, float r, float l, int style) {
    float rr = r;
    if (style == 2) rr -= 0.05 * sqrt(abs(sin(atan(q.z, q.y - r) * 10.0)));
    float d = length(q.yz - vec2(r, 0.0)) - rr;
    return max(d, abs(q.x) - l);
}

// a piece of a column (from hb0 up to its capital) flying from where it lay on the ground back onto its stump:
// lifted, turned upright and set down as e goes 0 .. 1
float flyingTop(vec3 q, float H, float hb0, float R, int style, float seed, float e, vec3 lying, float yaw) {
    float a = (1.0 - e) * 1.5708;
    vec3 pos = mix(lying, vec3(0.0, hb0, 0.0), e) + vec3(0.0, sin(3.14159 * e) * 1.6, 0.0);
    vec3 dd = q - pos;
    dd.xz = rot(yaw * (1.0 - e)) * dd.xz;
    vec3 u = vec3(sin(a), cos(a), 0.0), s = vec3(cos(a), -sin(a), 0.0);
    vec3 qq = vec3(dot(dd, s), dot(dd, u), dd.z);
    float d = column(qq + vec3(0.0, hb0, 0.0), H, H, R, style, seed, 0.0);
    return max(d, -qq.y);
}
// a block flying from the ground (pos a, tumbled by rotations ra) to its place b
float flyingBlock(vec3 p, vec3 a, vec3 b, vec2 ra, vec3 size, float e) {
    vec3 pos = mix(a, b, e) + vec3(0.0, sin(3.14159 * e) * 2.0, 0.0);
    vec3 g = p - pos;
    g.xz = rot(ra.x * (1.0 - e)) * g.xz;
    g.xy = rot(ra.y * (1.0 - e)) * g.xy;
    return sdBox(g, size) - 0.04 * (1.0 - e);
}

// ---- the avenue of columns on their stylobates, at x = +-7.5, from z = -3.5 back to z = -24.5; each broken or
// fallen column rebuilds as time rewinds (its fallen top flies back onto it), and a block fallen beside it flies
// back up to rest on it
float avenue(vec3 p) {
    float ax = abs(p.x);
    float b = sdBox(vec3(ax - 10.0, p.y - 4.0, p.z + 14.0), vec3(5.6, 6.0, 15.0));
    if (b > 1.0) return b;
    float side = p.x > 0.0 ? 1.0 : -1.0;
    float d = sdBox(vec3(ax - 7.5, p.y - 0.25, p.z + 14.0), vec3(1.9, 0.25, 13.5));
    d = min(d, sdBox(vec3(ax - 7.5, p.y - 0.62, p.z + 14.0), vec3(1.45, 0.13, 13.0)));
    int style = uKind == 0 ? 0 : (uKind == 1 ? 1 : 2);
    float H = uKind == 0 ? 5.4 : (uKind == 1 ? 4.8 : 7.6);
    float R = uKind == 0 ? 0.62 : (uKind == 1 ? 0.55 : 0.68);
    float j = clamp(floor((-3.5 - p.z) / 7.0 + 0.5), 0.0, 3.0);
    float zc = -3.5 - 7.0 * j;
    int ci = int(j) * 2 + (side > 0.0 ? 1 : 0);
    float rep = uCol[ci];
    vec3 q = vec3(ax - 7.5, p.y - 0.75, p.z - zc);
    float h0 = hash(vec2(j * 3.1 + 0.7, side * 1.7 + 2.0));
    float h1 = hash(vec2(j * 1.3 + 5.1, side * 3.3 + 0.4));
    if (h0 < 0.62 && rep < 0.999) {
        float hb0 = h0 < 0.28 ? 0.7 + h1 * 0.5 : H * (0.32 + 0.5 * h1);
        float e = smoothstep(0.05, 0.6, rep);
        d = min(d, column(q, H, hb0, R, style, h1, 1.0 - e));
        d = min(d, flyingTop(q, H, hb0, R, style, h1, e, vec3(2.4, -0.75 + R, 0.4 + h1), (h1 - 0.5) * 1.6));
    } else {
        d = min(d, column(q, H, H, R, style, h1, 0.0));
    }
    if (h1 > 0.45) {
        float e = smoothstep(0.55, 1.0, rep);
        d = min(d, flyingBlock(q, vec3(3.9, -0.35, -3.0), vec3(0.0, H + 0.42, 0.0), vec2(h1 * 5.0, (h1 - 0.7) * 0.5),
                               vec3(0.95, 0.42, 0.62), e));
    }
    return d;
}

// ---- braziers lit in the temple's prime (bowls on stands, rising from the ground as it comes)
const vec3 BRZ[12] = vec3[12](vec3(-4.8, 21.0, -48.0), vec3(4.8, 21.0, -48.0), vec3(-5.6, 0.0, -24.5), vec3(5.6, 0.0, -24.5),
                              vec3(-5.0, 4.0, -33.4), vec3(5.0, 4.0, -33.4), vec3(-7.0, 4.0, -40.0), vec3(7.0, 4.0, -40.0),
                              vec3(-6.0, 2.7, -42.0), vec3(6.0, 2.7, -42.0), vec3(-12.5, 0.0, -38.5), vec3(12.5, 0.0, -38.5));
float braziers(vec3 p) {
    if (uPrime <= 0.01) return 1e9;
    float d = 1e9;
    for (int i = 0; i < 4; i++) {
        vec3 b = BRZ[uKind * 4 + i] - vec3(0.0, 1.4 * (1.0 - uPrime), 0.0);
        vec3 q = p - b;
        float st = max(length(q.xz) - 0.14, abs(q.y - 0.5) - 0.5);
        float bowl = max(length(q - vec3(0.0, 1.25, 0.0)) - 0.45, q.y - 1.3);
        bowl = max(bowl, -(length(q - vec3(0.0, 1.4, 0.0)) - 0.38));
        st = min(st, max(length(q.xz) - 0.38, abs(q.y - 0.06) - 0.06));
        d = min(d, min(st, bowl));
    }
    return d;
}

// ---- MAYA: a stepped pyramid of seven terraces (sloping talud, carved tablero bands), its grand staircase
// between two ramps, the shrine on top with its roof comb. In ruin, a front corner has collapsed (its blocks
// tumbled at the foot) and the shrine is broken down; as time rewinds the blocks fly back, the corner and the
// shrine rebuild
vec2 maya(vec3 p) {
    vec3 q = p - TC;
    float bd = sdBox(q - vec3(0.0, 16.0, 3.0), vec3(23.5, 16.5, 27.0));
    if (bd > 1.5) return vec2(bd, 1.0);
    const float H = 3.0;
    float d = 1e9;
    float k0 = clamp(floor(q.y / H), 0.0, 6.0);
    for (int j = 0; j < 2; j++) {
        float k = clamp(k0 - float(j), 0.0, 6.0);
        float s = 22.0 - k * 2.6;
        float yl = clamp(q.y - k * H, 0.0, H);
        float sl = s - yl * 0.32;
        d = min(d, sdBox(q - vec3(0.0, k * H + H * 0.5, 0.0), vec3(sl, H * 0.5, sl)) * 0.92);
        d = min(d, sdBox(q - vec3(0.0, k * H + H - 0.45, 0.0), vec3(s - 0.7, 0.45, s - 0.7)));
    }
    float sh = sdBox(q - vec3(0.0, 23.6, -0.5), vec3(4.4, 2.6, 3.6));
    sh = max(sh, -sdBox(q - vec3(0.0, 22.5, 3.4), vec3(0.95, 1.5, 1.4)));
    sh = min(sh, sdBox(q - vec3(0.0, 26.45, -0.5), vec3(4.75, 0.25, 3.95)));
    float comb = sdBox(q - vec3(0.0, 28.9, -1.6), vec3(3.5, 2.2, 0.35));
    vec2 hc = vec2(mod(q.x + 0.85, 1.7) - 0.85, mod(q.y - 27.6, 1.5) - 0.75);
    comb = max(comb, -sdBox2(hc, vec2(0.42, 0.4)));
    sh = min(sh, comb);
    float ru = 1.0 - smoothstep(0.25, 0.85, uTemple);
    sh = max(sh, (q.y - mix(31.5, 23.4 + 1.8 * vnoise(q.xz * 0.8) + 0.06 * q.x, ru)) * 0.8);
    d = min(d, sh);
    float u = 26.0 - q.z;
    float st = max(sdStairs(vec2(u, q.y), 0.58, 0.6, 35.0, 30.0), abs(q.x) - 3.6);
    float ramp = (q.y - (u * 1.0345 + 0.95)) * 0.69;
    float alf = max(max(abs(abs(q.x) - 4.15) - 0.55, ramp), max(-u, q.y - 21.3));
    d = min(d, min(st, alf));
    // the collapsed corner, closing as its blocks come back
    float hole = 1.0 - smoothstep(0.05, 0.7, uTemple);
    if (hole > 0.0) d = max(d, -(length((q - vec3(16.0, 7.0, 18.5)) * vec3(1.0, 1.3, 1.0)) - 8.0 * hole - 0.6 * vnoise(q.xy * 0.9)) * 0.75);
    d = carveMissing(d, q);
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float e = smoothstep(0.08 + fi * 0.18, 0.4 + fi * 0.18, uTemple);
        if (e >= 1.0) continue;
        vec3 a = vec3(10.0 + fi * 3.5, 0.45, 26.0 - fi * 1.4);
        vec3 b = vec3(13.0 + fi * 2.0, 4.0 + fi * 2.2, 19.5 - fi * 0.8);
        d = min(d, flyingBlock(q, a, b, vec2(0.6 + fi, 0.15 - fi * 0.2), vec3(1.0, 0.45, 0.7), e));
    }
    return vec2(min(d, braziers(p)), 1.0);
}

// ---- KHMER: a terrace with its stairs, a gallery of balustered windows with a gate tower, the central prang and
// two side towers (redented tiers up to a lotus bud: the towers lose their upper tiers in ruin and regain them,
// tier by tier), a strangler fig straddling the gallery (its roots withdrawing as time rewinds)
float prang(vec3 q, float sc, float tiers) {
    q /= sc;
    float b = sdBox(q - vec3(0.0, 10.0, 0.0), vec3(5.5, 11.0, 6.0));
    if (b > 1.0) return b * sc;
    float d = sdBox(q - vec3(0.0, 3.0, 0.0), vec3(4.5, 3.0, 4.5));
    d = max(d, (abs(q.x) + abs(q.z)) - 7.6);
    d = min(d, sdBox(q - vec3(0.0, 2.2, 4.6), vec3(1.5, 2.2, 0.9)));
    d = max(d, -sdBox(q - vec3(0.0, 1.6, 5.0), vec3(0.75, 1.6, 1.5)));
    float y0 = 6.0;
    for (int i = 0; i < 6; i++) {
        float g = sat(tiers - float(i));
        if (g <= 0.0) break;
        float s = 4.1 * pow(0.84, float(i));
        float h = 2.0 * pow(0.9, float(i)) * g;
        float t = sdBox(q - vec3(0.0, y0 + h * 0.5, 0.0), vec3(s, h * 0.5, s));
        t = max(t, (abs(q.x) + abs(q.z)) - s * 1.55);
        float rim = sdBox(q - vec3(0.0, y0 + 0.16, 0.0), vec3(s + 0.3, 0.16, s + 0.3));
        rim = max(rim, (abs(q.x) + abs(q.z)) - s * 1.6 - 0.3);
        d = min(d, min(t, rim));
        y0 += h;
    }
    float bg = sat(tiers - 6.0);
    if (bg > 0.01) {
        vec3 bq = q - vec3(0.0, y0 + 1.1 * bg, 0.0);
        d = min(d, (length(bq / (vec3(1.25, 2.1, 1.25) * bg)) - 1.0) * 1.2 * bg);
    }
    return d * sc;
}
float roots(vec3 p) {
    float rk = sat(max(uJungle, uWear * 1.2 - 0.1));
    if (rk <= 0.02) return 1e9;
    float b = sdBox(p - vec3(-10.0, 12.0, -39.0), vec3(8.0, 24.0, 6.0));
    if (b > 1.0) return b;
    float w0 = 0.25 + 0.75 * rk;
    float d = sdCap(p, vec3(-10.0, 9.0, -41.0), vec3(-11.8, 33.0, -42.5), 1.7 * w0, 1.1 * w0);
    for (int i = 0; i < 6; i++) {
        // each root withdraws from its tip back toward the fig
        float reach = sat(rk * 1.6 - float(i) * 0.08);
        if (reach <= 0.02) continue;
        float x0 = -3.6 + float(i) * 1.45 + 0.4 * sin(float(i) * 2.7);
        float w = (0.62 - 0.05 * abs(float(i) - 2.5)) * w0;
        vec3 A = vec3(-10.0 + x0 * 0.3, 10.2, -40.6);
        vec3 B = vec3(-10.0 + x0 * 0.8, 7.0, -39.25);
        vec3 C = vec3(-10.0 + x0 + 0.3 * sin(float(i) * 1.9), 4.3, -39.0);
        vec3 D = vec3(-10.0 + x0 * 1.15, 3.8, -37.75);
        vec3 E = vec3(-10.0 + x0 * 1.3, 0.25, -37.3);
        vec3 F = vec3(-10.0 + x0 * 1.6, 0.1, -34.0 + 0.8 * sin(float(i) * 3.3));
        float r = sdCap(p, A, mix(A, B, sat(reach * 5.0)), w, w * 0.8);
        if (reach > 0.2) r = min(r, sdCap(p, B, mix(B, C, sat(reach * 5.0 - 1.0)), w * 0.8, w * 0.65));
        if (reach > 0.4) r = min(r, sdCap(p, C, mix(C, D, sat(reach * 5.0 - 2.0)), w * 0.65, w * 0.55));
        if (reach > 0.6) r = min(r, sdCap(p, D, mix(D, E, sat(reach * 5.0 - 3.0)), w * 0.55, w * 0.45));
        if (reach > 0.8) r = min(r, sdCap(p, E, mix(E, F, sat(reach * 5.0 - 4.0)), w * 0.45, w * 0.18));
        d = smin(d, r, 0.35);
    }
    return d;
}
vec2 khmer(vec3 p) {
    vec3 q = p - TC;
    float bd = sdBox(q - vec3(0.0, 17.0, 1.0), vec3(28.0, 18.0, 20.0));
    if (bd > 1.5) return vec2(bd, 1.0);
    float d = sdBox(q - vec3(0.0, 1.8, -2.0), vec3(25.0, 1.8, 16.0));
    d = min(d, sdBox(q - vec3(0.0, 3.75, -2.0), vec3(25.4, 0.25, 16.4)));
    float u = 18.4 - q.z;
    d = min(d, max(sdStairs(vec2(u, q.y), 0.55, 0.5, 8.0, 8.0), abs(q.x) - 4.0));
    vec3 g = q - vec3(0.0, 6.8, 11.0);
    float gal = sdBox(g, vec3(24.0, 2.8, 1.4));
    float wx = mod(g.x + 1.5, 3.0) - 1.5;
    float win = sdBox(vec3(wx, g.y + 0.2, g.z), vec3(0.85, 1.05, 2.0));
    float bal = length(vec2(mod(g.x + 0.33, 0.66) - 0.33, g.z)) - 0.11;
    gal = max(gal, -max(win, -bal));
    float roof = max(sdBox(g - vec3(0.0, 3.5, 0.0), vec3(24.2, 0.8, 1.8)), (abs(g.z) * 0.9 + (g.y - 3.0)) - 1.6);
    gal = min(gal, roof);
    gal = max(gal, -sdBox(g - vec3(0.0, -0.7, 0.0), vec3(1.7, 2.2, 3.0)));
    // a fallen stretch of the gallery, rebuilding from the bottom up
    float fall = 1.0 - smoothstep(0.1, 0.75, uTemple);
    if (fall > 0.0) {
        float top = 4.6 - 6.5 * fall + 0.8 * fall * vnoise(g.xz * 1.1);
        gal = max(gal, -max(sdBox(g - vec3(12.0, 2.0, 0.0), vec3(6.0 * sqrt(fall) + 0.5, 3.0, 3.0)), top - g.y));
    }
    d = min(d, gal);
    d = carveMissing(d, q);
    float tw = smoothstep(0.0, 1.0, uTemple);
    d = min(d, prang(q - vec3(0.0, 9.6, 11.0), 0.5, mix(1.0, 7.0, tw)));
    d = min(d, prang(q - vec3(0.0, 4.0, -6.0), 1.35, mix(4.2, 7.0, tw)));
    d = min(d, prang(q - vec3(-15.0, 4.0, -1.0), 0.85, mix(2.0, 7.0, tw)));
    d = min(d, prang(q - vec3(15.0, 4.0, -1.0), 0.85, mix(1.4, 7.0, tw)));
    d = min(d, braziers(p));
    float r = roots(p);
    return r < d ? vec2(r, 2.0) : vec2(d, 1.0);
}

// ---- GREEK: a Doric temple on its three steps. In ruin its colonnade is broken on the right, only the left part
// of the entablature and half the pediment stand, the cella's walls are ruined, pieces lie on the ground; as time
// rewinds the columns grow back to their capitals, the pieces fly back, the entablature and the pediment close
vec2 greek(vec3 p) {
    vec3 q = p - TC;
    float bd = sdBox(q - vec3(0.0, 9.0, 1.0), vec3(17.0, 10.0, 14.0));
    if (bd > 1.5) return vec2(bd, 1.0);
    float T = uTemple;
    float d = sdBox(q - vec3(0.0, 0.45, 0.0), vec3(15.6, 0.45, 11.6));
    d = min(d, sdBox(q - vec3(0.0, 1.35, 0.0), vec3(14.8, 0.45, 10.8)));
    d = min(d, sdBox(q - vec3(0.0, 2.25, 0.0), vec3(14.0, 0.45, 10.0)));
    float cel = sdBox(q - vec3(0.0, 7.5, -1.0), vec3(8.0, 4.8, 6.5));
    cel = max(cel, -sdBox(q - vec3(0.0, 7.5, -1.0), vec3(7.2, 6.0, 5.7)));
    float wc = smoothstep(0.0, 0.6, T);
    cel = max(cel, (q.y - mix(7.0 + 4.5 * vnoise(q.xz * 0.3) + 1.2 * sin(q.x * 0.4), 12.4, wc)) * 0.7);
    cel = max(cel, -sdBox(q - vec3(0.0, 5.5, 5.5), vec3(1.6, 3.0, 1.5)));
    d = carveMissing(min(d, cel), q);
    vec2 r = vec2(d, 1.0);
    const float CH = 10.0;
    float cg = smoothstep(0.15, 0.65, T);
    float i = clamp(floor((q.x - 2.0) / 4.0 + 0.5), -3.0, 2.0);
    float hb = mix(i > 1.5 ? 5.2 : (i > 0.5 ? 2.4 : CH), CH, cg);
    if (cg > 0.999) hb = CH;
    float c = column(vec3(q.x - (4.0 * i + 2.0), q.y - 2.7, q.z - 8.0), CH, hb, 0.8, 2, i + 3.0, 1.0 - cg);
    float j = clamp(floor((8.0 - q.z) / 4.0 + 0.5), 1.0, 4.0);
    float sd = q.x > 0.0 ? 1.0 : -1.0;
    float hb2 = mix(sd > 0.0 ? (j < 2.5 ? 3.6 : 1.4 + j * 1.6) : CH, CH, cg);
    if (cg > 0.999) hb2 = CH;
    c = min(c, column(vec3(abs(q.x) - 10.0, q.y - 2.7, q.z - (8.0 - 4.0 * j)), CH, hb2, 0.8, 2, j + sd * 7.0, 1.0 - cg));
    if (c < r.x) r = vec2(c, 3.0);
    // the entablature and the pediment, closing from the left as the columns stand again
    float ex = mix(2.5, 12.0, smoothstep(0.55, 0.9, T));
    float ent = sdBox(q - vec3(0.0, 13.8, 8.0), vec3(11.0, 1.1, 1.0));
    ent = min(ent, sdBox(vec3(abs(q.x) - 10.0, q.y - 13.8, q.z), vec3(1.0, 1.1, 9.0)));
    ent = min(ent, sdBox(q - vec3(0.0, 15.05, 8.1), vec3(11.2, 0.16, 1.2)));
    ent = max(ent, q.x - ex - 0.8 * vnoise(q.yz * 1.3) * (1.0 - T));
    float px = mix(-1.6, 12.0, smoothstep(0.7, 1.0, T));
    float ped = max(abs(q.z - 8.0) - 0.9, (q.y - 15.2 - (11.2 - abs(q.x)) * 0.27) * 0.96);
    ped = max(ped, max(15.2 - q.y, q.x - px + 1.2 * vnoise(q.yz * 0.8) * (1.0 - T)));
    ped = max(ped, abs(q.x) - 11.2);
    ent = min(ent, ped);
    if (ent < r.x) r = vec2(ent, 1.0);
    // the fallen pieces fly back up to the broken columns
    float f = 1e9;
    float e1 = smoothstep(0.3, 0.6, T), e2 = smoothstep(0.2, 0.5, T), e3 = smoothstep(0.6, 0.9, T);
    if (e3 < 1.0) f = min(f, flyingBlock(q, vec3(10.5, 0.75, 14.2), vec3(8.5, 13.8, 8.0), vec2(0.35, -0.12), vec3(1.9, 0.75, 0.9), e3));
    if (e1 < 1.0) {
        vec3 g2 = q - mix(vec3(13.5, 0.0, 9.0), vec3(10.0, 6.0, 8.0), e1) - vec3(0.0, sin(3.14159 * e1) * 2.0, 0.0);
        g2.xz = rot(1.2 * (1.0 - e1)) * g2.xz;
        f = min(f, lyingDrum(g2, 0.8 * (1.0 - e1 * 0.3), 0.75, 2));
    }
    if (e2 < 1.0) {
        vec3 g2 = q - mix(vec3(6.8, 0.0, 13.6), vec3(6.0, 4.0, 8.0), e2) - vec3(0.0, sin(3.14159 * e2) * 2.0, 0.0);
        g2.xz = rot(-0.4 * (1.0 - e2)) * g2.xz;
        f = min(f, lyingDrum(g2, 0.79 * (1.0 - e2 * 0.3), 0.7, 2));
    }
    f = min(f, braziers(p));
    if (f < r.x) r = vec2(f, 3.0);
    return r;
}

// ---- the great trunks, with their buttress roots (x, z, radius, seed); the jungle's own trees stand in the avenue
// and around the temple at the start and shrink back as it recedes
const vec4 TREES[10] = vec4[10](vec4(-36.0, -2.0, 1.6, 0.1), vec4(37.0, -6.0, 1.5, 0.7), vec4(-34.0, -22.0, 1.8, 0.3),
                                vec4(35.0, -26.0, 1.7, 0.9), vec4(-40.0, 6.0, 2.1, 0.5), vec4(41.0, 2.0, 2.0, 0.2),
                                vec4(-27.0, -50.0, 1.4, 0.6), vec4(29.0, -52.0, 1.5, 0.4), vec4(-46.0, -14.0, 2.2, 0.8),
                                vec4(47.0, -16.0, 2.0, 0.15));
const vec4 JTREES[6] = vec4[6](vec4(3.0, -9.0, 1.2, 0.3), vec4(-4.5, -19.0, 1.4, 0.8), vec4(11.0, -30.0, 1.6, 0.5),
                               vec4(-13.0, -33.0, 1.8, 0.1), vec4(15.0, -13.0, 1.1, 0.6), vec4(-1.0, -27.5, 1.3, 0.9));
float trunk(vec3 p, vec4 t, float r, float buttK) {
    vec2 o = p.xz - t.xy + vec2(0.06, -0.04) * p.y;
    float l = length(o);
    if (l > r * 4.5 + 2.0) return l - r * 4.5 - 1.0;
    float flare = exp(-max(p.y, 0.0) * 0.55);
    float a = atan(o.y, o.x);
    float butt = pow(abs(cos(a * 2.5 + t.w * 6.0)), 6.0) * flare * r * 2.2 * buttK;
    float rr = r * (1.0 + 0.5 * flare) + butt + 0.05 * sin(p.y * 1.3 + a * 3.0);
    return (l - rr) * 0.6;
}
float trees(vec3 p) {
    float d = 1e9;
    int n = uKind == 2 ? 6 : 10;
    for (int i = 0; i < 10; i++) {
        if (i >= n) break;
        d = min(d, trunk(p, TREES[i], TREES[i].z * (uKind == 2 ? 0.55 : 1.0), uKind == 2 ? 0.2 : 1.0));
    }
    if (uJungle > 0.02) {
        // shrinking back: thinner and lower, the crowns' wall sinking with them
        float k = smoothstep(0.0, 1.0, uJungle);
        for (int i = 0; i < 6; i++) d = min(d, max(trunk(p, JTREES[i], JTREES[i].z * sqrt(k), uKind == 2 ? 0.4 : 1.0), p.y - 34.0 * k));
    }
    return d;
}

float ground(vec3 p) {
    float e = smoothstep(5.6, 7.5, abs(p.x)) + smoothstep(3.0, 1.0, p.z) * 0.0;
    float h = (0.32 * vnoise(p.xz * 0.16) + 0.12 * vnoise(p.xz * 0.6) - 0.12) * e;
    return (p.y - h) * 0.9;
}

vec2 map(vec3 p) {
    vec2 r = vec2(ground(p), 0.0);
    float a = avenue(p);
    if (a < r.x) r = vec2(a, 3.0);
    vec2 t = uKind == 0 ? maya(p) : (uKind == 1 ? khmer(p) : greek(p));
    if (t.x < r.x) r = t;
    float tr = trees(p);
    if (tr < r.x) r = vec2(tr, 2.0);
    return r;
}

vec3 calcN(vec3 p, float t) {
    float e = 0.0012 * t + 0.002;
    const vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x + k.yxy * map(p + k.yxy * e).x +
                     k.xxx * map(p + k.xxx * e).x);
}
float softShadow(vec3 ro, vec3 rd, float tmax) {
    float res = 1.0, t = 0.06;
    for (int i = 0; i < 40; i++) {
        float h = map(ro + rd * t).x;
        res = min(res, 7.0 * h / t);
        t += clamp(h, 0.05, 2.4);
        if (res < 0.01 || t > tmax) break;
    }
    return smoothstep(0.0, 1.0, sat(res));
}
float calcAO(vec3 p, vec3 n) {
    float o = 0.0, s = 1.0;
    for (int i = 0; i < 5; i++) {
        float h = 0.05 + 0.42 * float(i) / 4.0;
        o += (h - map(p + n * h).x) * s;
        s *= 0.72;
    }
    return sat(1.0 - 1.5 * o);
}

// ---- the canopy: leaf masses over everything but the clearing around the temple; its light at any point
float clearing(vec2 q) {
    vec2 c = (q - vec2(0.0, -50.0)) / (uKind == 2 ? vec2(46.0, 40.0) : vec2(33.0, 28.0));
    return smoothstep(0.82, 1.15, length(c) * (1.0 + 0.9 * uJungle));
}
float canopy(vec2 q, float detail) {
    vec2 s = q + uSway * vec2(sin(uTime * 0.37 + q.y * 0.05), cos(uTime * 0.29 + q.x * 0.05)) * 0.7;
    float n = fbm(s * 0.045) * 0.68 + mix(0.16, vnoise(s * 0.42), detail) * 0.32;
    float th = 0.43 - uGap + 0.1 * uJungle;
    return smoothstep(th - 0.05, th + 0.05, n) * clearing(q);
}
float sunThrough(vec3 p) {
    if (p.y > CAN_Y) return 1.0;
    // (a low sun comes in under the canopy's edge: its light is cut by the leaves nearer by)
    float tc = min((CAN_Y - p.y) / max(uSunDir.y, 0.08), 40.0);
    vec2 q = p.xz + uSunDir.xz * tc;
    return 1.0 - 0.94 * canopy(q, 0.6) * (1.0 - 0.45 * uGold);
}

// ---- the sky, a few clouds, far jungle hills at the horizon
vec3 sky(vec3 rd) {
    float y = rd.y;
    vec3 c = mix(uSkyH, uSkyZ, pow(sat(y), 0.6));
    float mu = sat(dot(rd, uSunDir));
    c += uSunCol * (0.05 * pow(mu, 4.0) + 0.3 * pow(mu, 40.0)) + uSunCol * 6.0 * smoothstep(0.9993, 0.9997, mu);
    if (y > 0.0) {
        vec2 cq = rd.xz / (y + 0.12) * 1.3 + vec2(uTime * 0.003, 0.0);
        float cl = smoothstep(0.52, 0.85, fbm(cq));
        c = mix(c, mix(uSkyH * 1.05, uSunCol * 0.45 + uSkyH * 0.5, mu * mu), cl * 0.45);
    }
    float az = atan(rd.x, -rd.z);
    float h = 0.03 + 0.05 * fbm(vec2(az * 3.0, 1.0));
    float far = 1.0 - smoothstep(h - 0.004, h + 0.004, y);
    return mix(c, mix(uFog, uLeaf * 2.0, 0.22), far * 0.85);
}

// ---- foliage planes: jungle walls, undergrowth, lianas
// a crown line along u: soft rounded crowns (jungle) or umbrella pines and cypresses (Mediterranean); returns the
// coverage of (u, v)
float crowns(vec2 uv, float base, float amp, float seed, float fw, int pine) {
    float u = uv.x;
    float top = base + amp * 0.55 * fbm2(vec2(u * 0.06 + seed, seed));
    float cu = u * 0.11 + seed;
    float ci = floor(cu), cf = fract(cu) - 0.5;
    float ch = hash1(ci + seed * 7.0);
    if (pine == 0) {
        top += amp * (0.25 + 0.5 * ch) * sqrt(max(0.0, 0.25 - cf * cf)) * 2.0;
        top += amp * 0.12 * (vnoise(uv * vec2(0.9, 0.9) + seed) - 0.5) + 0.35 * (vnoise(uv * 3.1 + seed) - 0.5);
        return 1.0 - smoothstep(-fw, fw, uv.y - top);
    }
    // the maquis low, a pine's umbrella or a cypress over it
    float low = base * 0.45 + amp * 0.25 * fbm2(vec2(u * 0.2, seed)) + 0.3 * (vnoise(uv * 2.7) - 0.5);
    float cov = 1.0 - smoothstep(-fw, fw, uv.y - low);
    float H = base + amp * (0.6 + 0.5 * ch);
    if (ch > 0.45) {
        float w = 0.42 + 0.1 * hash1(ci + 3.0);
        H -= amp * 0.35;
        float e = 1.0 - sq(cf / w);
        float topC = H + 1.6 * sqrt(max(e, 0.0)) + 0.9 * (fbm2(uv * 0.5 + seed) - 0.5);
        float botC = H - 2.2 * sqrt(max(e, 0.0)) * (0.5 + 0.6 * vnoise(uv * 0.9)) + 0.6 * (vnoise(uv * 2.0) - 0.5) + 0.3;
        float crown = (1.0 - smoothstep(-fw, fw, uv.y - topC)) * smoothstep(-fw, fw, uv.y - botC) * step(0.0, e);
        float trunk = (1.0 - smoothstep(0.035, 0.035 + fw / 9.0, abs(cf - 0.06 * sin(uv.y * 0.3 + ci)))) * step(uv.y, H);
        cov = max(cov, max(crown, trunk));
    } else if (ch < 0.12) {
        float w = 0.07 * (1.0 - sat(uv.y / (H + 3.0)));
        float cyp = (1.0 - smoothstep(w * 9.0 - fw, w * 9.0 + fw, abs(cf) * 9.0 + 0.2 * (vnoise(uv * 3.0) - 0.5))) * step(uv.y, H + 3.0);
        cov = max(cov, cyp);
    }
    return cov;
}
// ferns: arching fronds along u, serrated
float ferns(vec2 uv, float base, float amp, float seed, float fw) {
    float u = uv.x * 0.9 + seed;
    float c = 0.0;
    for (int k = 0; k < 3; k++) {
        float uu = u + float(k) * 0.37;
        float i = floor(uu), f = fract(uu) - 0.5;
        float h = base + amp * hash1(i + seed * 3.0 + float(k) * 17.0);
        float top = h * (1.0 - sq(f * 1.9)) + 0.09 * sin(uv.x * 34.0 + i) * (0.4 + 0.6 * sat(uv.y / max(h, 0.1)));
        c = max(c, 1.0 - smoothstep(-fw, fw, uv.y - top));
    }
    return c;
}
float lianas(vec2 uv, float seed, float fw) {
    float u = uv.x * 0.7 + seed;
    float i = floor(u), f = fract(u) - 0.5;
    float h = hash1(i * 1.7 + seed);
    if (h < 0.6 - 0.35 * uJungle) return 0.0;
    float bottom = 4.0 + 16.0 * hash1(i + 9.0);
    float x = f + 0.12 * sin(uv.y * 0.18 + i * 3.0 + uSway * sin(uTime * 0.5 + i) * 0.6);
    float leafy = smoothstep(0.55, 0.75, vnoise(vec2(i * 3.0, uv.y * 1.6)));
    float w = (0.035 + 0.11 * leafy) * 0.7;
    float a = 1.0 - smoothstep(w - fw * 0.7, w + fw * 0.7, abs(x) * 1.43);
    return a * smoothstep(bottom - 0.3, bottom + 0.3, uv.y);
}

struct Lay { float t, a, kind, seed; vec3 p; vec2 uv; };

void consider(inout Lay best, inout Lay edge, float t, float a, float kind, float seed, vec3 p, vec2 uv) {
    if (a > 0.5) {
        if (t < best.t) { best.t = t; best.a = a; best.kind = kind; best.seed = seed; best.p = p; best.uv = uv; }
    } else if (a > 0.02 && t < edge.t) {
        edge.t = t; edge.a = a; edge.kind = kind; edge.seed = seed; edge.p = p; edge.uv = uv;
    }
}

void foliage(vec3 ro, vec3 rd, float tmax, inout Lay best, inout Lay edge) {
    int pine = uKind == 2 ? 1 : 0;
    // jungle walls: left, right, back, and a far back row (vertical planes, u along them, v up)
    for (int i = 0; i < 4; i++) {
        float t;
        vec2 uv;
        if (i < 2) {
            float x = i == 0 ? -42.0 : 42.0;
            t = (x - ro.x) / rd.x;
            vec3 p = ro + rd * t;
            uv = vec2(p.z, p.y);
        } else {
            float z = i == 2 ? -96.0 : -140.0;
            t = (z - ro.z) / rd.z;
            vec3 p = ro + rd * t;
            uv = vec2(p.x, p.y);
        }
        if (t <= 0.0 || t > tmax) continue;
        float fw = t * uPx * 1.2 + 0.05;
        float base = (i == 3 ? 36.0 : 24.0) + (pine == 1 ? -10.0 : 0.0);
        float a = crowns(uv, base, pine == 1 ? 8.0 : 10.0, float(i) * 13.7, fw, pine);
        consider(best, edge, t, a, 1.0, float(i), ro + rd * t, uv);
    }
    // at the start the jungle buries the temple: a mass of crowns in front of it, sinking away as it recedes
    if (uJungle > 0.01) {
        float t = (-27.0 - ro.z) / rd.z;
        if (t > 0.0 && t < tmax) {
            vec3 p = ro + rd * t;
            float fw = t * uPx * 1.2 + 0.03;
            float a = crowns(vec2(p.x, p.y), -12.0 + 33.0 * smoothstep(0.0, 1.0, uJungle), 7.0, 41.0, fw, 0);
            consider(best, edge, t, a, 1.0, 7.0, p, vec2(p.x, p.y));
        }
        float t2 = (-10.0 - ro.z) / rd.z;
        if (t2 > 0.0 && t2 < tmax) {
            vec3 p = ro + rd * t2;
            float fw = t2 * uPx * 1.2 + 0.02;
            float a = crowns(vec2(p.x * 1.6, p.y), -6.0 + 12.0 * uJungle, 5.0, 13.0, fw, 0) * step(10.0 * (1.0 - uJungle), abs(p.x) + 6.0 * uJungle);
            consider(best, edge, t2, a, 2.0, 8.0, p, vec2(p.x, p.y));
        }
    }
    // undergrowth: rows of ferns across the ground beside the avenue, and along the stylobates
    for (int i = 0; i < 5; i++) {
        float z = 10.0 - float(i) * 11.0;
        float t = (z - ro.z) / rd.z;
        if (t <= 0.0 || t > tmax) continue;
        vec3 p = ro + rd * t;
        float jg = i == 0 ? 0.0 : uJungle;   // (the nearest row stays low: the camera stands by it)
        if (abs(p.x) < 10.2 * (1.0 - jg) || abs(p.x) > 41.0) continue;
        float fw = t * uPx * 1.2 + 0.01;
        float a = pine == 1 ? crowns(vec2(p.x, p.y), 1.1 + 4.0 * jg, 1.2 + 2.0 * jg, float(i) * 5.3, fw, 0)
                            : ferns(vec2(p.x, p.y), 0.9 + 3.6 * jg, 1.5 + 2.5 * jg, float(i) * 3.1, fw);
        a *= 1.0 - 0.6 * uPrime * step(abs(p.x), 14.0);
        consider(best, edge, t, a, 2.0, float(i), p, vec2(p.x, p.y));
    }
    for (int i = 0; i < 2; i++) {
        float x = i == 0 ? -11.0 : 11.0;
        float t = (x - ro.x) / rd.x;
        if (t <= 0.0 || t > tmax) continue;
        vec3 p = ro + rd * t;
        if (p.z > 6.0 || p.z < -34.0) continue;
        float fw = t * uPx * 1.2 + 0.01;
        float a = ferns(vec2(p.z, p.y), 0.5 + 3.0 * uJungle, 1.0 + 2.0 * uJungle, 7.0 + float(i) * 2.3, fw) * (1.0 - uPrime);
        consider(best, edge, t, a, 2.0, 5.0 + float(i), p, vec2(p.z, p.y));
    }
    // lianas hanging from the canopy beside the avenue (none in the Mediterranean)
    if (pine == 0) {
        for (int i = 0; i < 2; i++) {
            float z = i == 0 ? -2.0 : -18.0;
            float t = (z - ro.z) / rd.z;
            if (t <= 0.0 || t > tmax) continue;
            vec3 p = ro + rd * t;
            if (abs(p.x) < 11.0 * (1.0 - uJungle) || abs(p.x) > 40.0 || p.y > CAN_Y) continue;
            float fw = t * uPx * 1.2 + 0.01;
            float a = lianas(vec2(p.x, p.y), float(i) * 4.7, fw);
            consider(best, edge, t, a, 3.0, float(i), p, vec2(p.x, p.y));
        }
    }
    // the canopy's edge hanging over the avenue, leafy along its bottom; the far one only beside the clearing
    for (int i = 0; i < 2; i++) {
        float z = i == 0 ? -8.0 : -30.0;
        float t = (z - ro.z) / rd.z;
        if (t <= 0.0 || t > tmax) continue;
        vec3 p = ro + rd * t;
        if (i == 1 && abs(p.x) < 24.0) continue;
        float fw = t * uPx * 1.2 + 0.01;
        float u = p.x;
        float bottom = (i == 0 ? 15.5 : 21.0) - 8.0 * uJungle + 3.5 * fbm2(vec2(u * 0.07 + float(i) * 5.0, 0.5));
        // hanging leaf clumps and trailing fronds
        float cu = u * 0.35 + float(i) * 3.0;
        float ci = floor(cu), cf = fract(cu) - 0.5;
        bottom -= 2.2 * hash1(ci) * sqrt(max(0.0, 0.25 - cf * cf)) * 2.0;
        bottom += 0.45 * (vnoise(vec2(u, p.y) * 2.4 + float(i)) - 0.5);
        float a = smoothstep(-fw, fw, p.y - bottom) * (uKind == 2 ? 0.0 : 1.0);
        consider(best, edge, t, a, 4.0, float(i), p, vec2(p.x, p.y));
    }
}

vec3 foliageCol(Lay l, vec3 rd) {
    float n = fbm(l.uv * (l.kind == 4.0 ? 0.25 : 0.55) + l.seed * 3.0);
    float light = sunThrough(l.p + vec3(0.0, 0.5, 0.0));
    if (l.kind == 4.0) {
        // the canopy from below: dark leaves, translucent where the sun shines through them
        // the canopy's edge from below: dark leaves, a little light through them toward their tips
        float tip = 1.0 - smoothstep(0.0, 3.0, l.uv.y - (l.seed == 0.0 ? 15.0 : 20.0));
        vec3 c = uLeaf * (0.35 + 0.4 * n) * uAmb;
        c += uLeafLit * uSunCol * (0.03 + 0.08 * tip) * n;
        return c * (0.8 + 0.4 * vnoise(l.uv * 3.0));
    }
    float v = l.uv.y;
    vec3 base = uLeaf * mix(0.55, 1.25, n);
    if (l.kind == 3.0) base = mix(uBark * 0.5, uLeaf, 0.6);
    // the light from above (the tops lit), and the sun's translucency through the leaves when it is behind them
    float top = l.kind == 1.0 ? smoothstep(0.0, 14.0, v - 10.0) : sat(v / 2.0);
    float back = pow(sat(dot(rd, uSunDir)), 3.0);
    vec3 c = base * (uAmb * (0.55 + 0.45 * top));
    c += base * uSunCol * light * (0.25 + 0.45 * top);
    c += uLeafLit * uSunCol * light * back * 0.35;
    c *= 0.75 + 0.25 * vnoise(l.uv * 4.0 + l.seed);
    // leaf clumps: rounded masses, their upper edges catching the light
    float clump = fbm(l.uv * 0.32 + l.seed * 7.0);
    float rim = smoothstep(0.5, 0.75, clump) * (1.0 - smoothstep(0.75, 0.95, clump));
    c = c * (0.55 + 0.8 * clump) + uLeafLit * (uSunCol * light * 0.22 + uAmb * 0.1) * rim;
    return c;
}

// ---- carvings: a height field (1 the surface, 0 the deepest cut) for the bands and panels of each style
float glyph(vec2 g, float id) {
    // a Maya cartouche: a rounded frame, an inner sign made of a few blobs and bars
    float d = sdBox2(g, vec2(0.36, 0.36)) - 0.06;
    float frame = smoothstep(0.035, 0.0, abs(d + 0.04) - 0.03);
    float h1 = hash1(id), h2 = hash1(id + 7.1), h3 = hash1(id + 3.3);
    float inner = 0.0;
    inner = max(inner, smoothstep(0.04, 0.0, length(g - vec2(-0.12, 0.1) * (h1 - 0.3)) - 0.09 - 0.05 * h2));
    inner = max(inner, smoothstep(0.03, 0.0, sdBox2(g - vec2(0.0, -0.14 + 0.1 * h3), vec2(0.22 * h2 + 0.06, 0.035))));
    inner = max(inner, smoothstep(0.03, 0.0, length(g - vec2(0.14, 0.12) * (h3 * 2.0 - 1.0)) - 0.06));
    float cut = smoothstep(0.0, -0.03, d);
    return 1.0 - 0.8 * cut * (1.0 - max(frame, inner));
}
float carve(vec2 uv, vec3 p, float id) {
    vec3 q = p - TC;
    if (uKind == 0) {
        if (id == 3.0 && p.y > 1.4 && p.y < 5.0) {
            vec2 g = vec2(uv.x / 0.46, (p.y - 1.4) / 0.46);
            return glyph(fract(g) - 0.5, floor(g.x) * 13.0 + floor(g.y) * 7.0 + floor(p.z));
        }
        float yl = mod(q.y, 3.0);
        if (id == 1.0 && yl > 2.12 && yl < 2.94 && q.y < 21.0) {
            vec2 g = vec2(uv.x / 0.82, (yl - 2.12) / 0.82);
            return glyph(fract(g) - 0.5, floor(g.x) + floor(q.y / 3.0) * 31.0);
        }
        return 1.0;
    }
    if (uKind == 1) {
        // lotus petals in rows, devata panels on the pillars
        float yl = mod(q.y + 0.3, 2.0);
        if (id == 1.0 && yl < 0.55) {
            vec2 g = vec2(fract(uv.x / 0.5) - 0.5, yl / 0.55);
            float petal = abs(g.x) * 2.0 + sq(g.y - 0.1) * 1.4;
            return 1.0 - 0.75 * smoothstep(0.05, 0.0, abs(petal - 0.75) - 0.06) - 0.4 * step(0.9, g.y);
        }
        if (id == 3.0 && p.y > 1.6 && p.y < 3.6) {
            vec2 g = vec2(uv.x, p.y - 2.6);
            float niche = sdBox2(g * vec2(1.0, 0.5), vec2(0.18, 0.4)) - 0.05;
            float fig = length(g * vec2(1.0, 0.45) - vec2(0.0, 0.05)) - 0.12;
            return 1.0 - 0.7 * smoothstep(0.02, -0.02, niche) * smoothstep(-0.02, 0.03, fig);
        }
        return 1.0;
    }
    // Greek: the triglyph frieze, the drums' joints only elsewhere
    if (id == 1.0 && q.y > 13.85 && q.y < 14.85) {
        float u = mod(uv.x + 1.0, 2.0);
        if (u < 0.9) {
            float g = fract(u / 0.3);
            return 1.0 - 0.7 * smoothstep(0.12, 0.05, abs(g - 0.5) - 0.2) * step(q.y, 14.75);
        }
        return 0.94;
    }
    return 1.0;
}

// ---- masonry: joints of coursed blocks (distance to the nearest joint, the block's hash)
vec2 masonry(vec2 uv, float ch, float bl) {
    float row = floor(uv.y / ch);
    float u = uv.x / bl + hash1(row) * 0.7 + row * 0.5;
    float c = floor(u);
    vec2 f = vec2(fract(u) * bl, fract(uv.y / ch) * ch);
    float e = min(min(f.x, bl - f.x), min(f.y, ch - f.y));
    return vec2(e, hash(vec2(c, row)));
}

vec3 shadeSolid(vec3 p, vec3 n, float id, vec3 rd, float t) {
    vec3 an = abs(n);
    vec2 uv;
    vec3 tu, tv;
    if (an.y > 0.75) { uv = p.xz; tu = vec3(1.0, 0.0, 0.0); tv = vec3(0.0, 0.0, 1.0); }
    else if (an.x > an.z) { uv = vec2(p.z * sign(n.x), p.y); tu = vec3(0.0, 0.0, sign(n.x)); tv = vec3(0.0, 1.0, 0.0); }
    else { uv = vec2(-p.x * sign(n.z), p.y); tu = vec3(-sign(n.z), 0.0, 0.0); tv = vec3(0.0, 1.0, 0.0); }
    float ao = calcAO(p, n);
    float lod = sat(t / 90.0);
    vec3 alb;
    float moss = 0.0, rough = 1.0;
    vec3 nn = n;
    if (id == 1.0 || id == 3.0) {
        // stone: blocks in courses (drums on the columns), each its own tone, weathered, carved, mossy
        vec2 m = id == 3.0 ? vec2(0.5 - abs(fract(p.y / 1.15) - 0.5), hash(floor(vec2(p.y / 1.15, p.x + p.z)))) * vec2(1.15, 1.0)
                           : masonry(uv, uKind == 2 ? 0.9 : 0.62, uKind == 2 ? 1.8 : 1.25);
        float joint = (1.0 - smoothstep(0.012, 0.05 + lod * 0.06, m.x)) * (1.0 - lod * 0.6);
        float wn = fbm(p.xz * 0.35 + p.y * vec2(0.27, -0.21));
        alb = mix(uStone2, uStone, 0.45 + 0.55 * smoothstep(0.2, 0.8, wn));
        alb *= 1.0 + (0.24 * m.y - 0.14) * (0.35 + 0.65 * uWear);
        float grain = vnoise(uv * 9.0) * 0.5 + vnoise(uv * 23.0) * 0.5;
        alb *= 0.9 + 0.2 * grain * (1.0 - lod);
        // dark streaks of rain down the faces
        float streak = smoothstep(0.55, 0.85, fbm(vec2(dot(uv, vec2(1.0, 0.0)) * 1.7, p.y * 0.08))) * (1.0 - an.y);
        alb *= 1.0 - 0.3 * streak * uWear;
        float h = carve(uv, p, id);
        if (h < 0.999 && lod < 0.8) {
            const float E = 0.015;
            float hx = carve(uv + vec2(E, 0.0), p + tu * E, id) - h, hy = carve(uv + vec2(0.0, E), p + tv * E, id) - h;
            nn = normalize(n - (tu * hx + tv * hy) / E * 0.012);
        }
        h = mix(1.0, h, 1.0 - 0.55 * uWear);
        alb *= mix(0.5, 1.0, h);
        alb *= 1.0 - 0.45 * joint;
        // worn edges of the blocks: rounded toward the joints
        float wear = smoothstep(0.12, 0.02, m.x) * (1.0 - joint) * (1.0 - lod) * uWear;
        nn = normalize(nn + (tv * (fract(uv.y / 0.62) > 0.5 ? 1.0 : -1.0)) * wear * 0.25 * (1.0 - an.y));
        // moss: on the tops, in the crevices and the joints, low on the walls; lichen
        float mn = fbm(p.xz * 0.3 + p.y * 0.23 + 3.0);
        float mossAmt = uKind == 2 ? 0.55 : 1.0;
        moss = smoothstep(0.42, 0.7, mn) * (0.25 + 0.9 * sat(n.y)) + (1.0 - ao) * 0.8 * smoothstep(0.25, 0.55, mn) +
               joint * 0.6 * mn + (1.0 - h) * 0.5 * mn + smoothstep(1.5, 0.0, p.y) * smoothstep(0.3, 0.6, mn);
        moss = sat(moss * mossAmt * (0.12 + 0.88 * uWear) + uJungle * 0.7 * smoothstep(0.2, 0.5, mn));
        moss *= smoothstep(0.25, 0.5, vnoise(uv * 5.0) * 0.6 + moss * 0.6);
        float lich = smoothstep(0.72, 0.8, vnoise(uv * 2.6 + 5.0)) * (1.0 - moss);
        alb = mix(alb, uStone * 1.25 + vec3(0.04, 0.035, 0.0), lich * 0.35 * uWear);
        // ivy and vines hanging down the vertical faces
        if (an.y < 0.5) {
            float iv = fbm(vec2(uv.x * 0.32, p.y * 0.1) + vec2(id * 3.0, 0.0)) + 0.25 * sin(uv.x * 0.21);
            float ivy = smoothstep(0.66 - 0.5 * uJungle, 0.72 - 0.5 * uJungle, iv) * smoothstep(0.25, 0.55, vnoise(uv * 7.0) + 0.2 + uJungle) * (uKind == 2 ? 0.4 : 1.0) * max(uWear, uJungle);
            if (ivy > 0.01) {
                vec2 lh = vec2(vnoise(uv * 5.0), vnoise(uv * 5.0 + 17.0));
                nn = normalize(mix(nn, normalize(n + (tu * (lh.x - 0.5) + tv * (lh.y - 0.3)) * 1.2), ivy));
                alb = mix(alb, uLeaf * (0.8 + 0.6 * lh.x), ivy);
                moss *= 1.0 - ivy;
            }
        }
        // the prime: the colors temples had
        if (uPrime > 0.0) {
            vec3 paint = alb;
            bool cut = h < 0.9;
            if (uKind == 0) paint = cut ? (fract(uv.x * 0.6) < 0.5 ? vec3(0.08, 0.25, 0.42) : vec3(0.85, 0.62, 0.18)) : vec3(0.52, 0.09, 0.05) * (0.9 + 0.2 * m.y);
            else if (uKind == 1) paint = p.y > 17.0 ? vec3(0.95, 0.66, 0.24) : (cut ? vec3(0.5, 0.16, 0.08) : uStone * 1.08);
            else paint = cut ? vec3(0.06, 0.12, 0.42) : (h < 0.97 && p.y > 13.5 ? vec3(0.5, 0.08, 0.05) : uStone * 1.1);
            if (id == 3.0 && uKind != 0) paint = mix(paint, uStone * 1.1, 0.7);
            alb = mix(alb, paint, uPrime * 0.85);
        }
        rough = 1.0;
    } else if (id == 2.0) {
        // bark (and the fig's pale roots), furrowed, mossy at the foot
        float f = vnoise(vec2(atan(n.z, n.x) * 6.0, p.y * 0.6)) * 0.6 + vnoise(uv * 4.0) * 0.4;
        alb = uBark * (0.65 + 0.5 * f);
        moss = smoothstep(4.0, 0.0, p.y) * smoothstep(0.35, 0.7, fbm(uv * 0.8)) * (uKind == 2 ? 0.3 : 0.9);
        moss += smoothstep(0.62, 0.8, fbm(uv * 0.5 + 9.0)) * 0.5 * sat(n.y + 0.3);
        moss = sat(moss);
    } else {
        // the ground: the avenue's flagstones (joints full of moss and grass), the forest floor elsewhere
        float paved = (1.0 - smoothstep(5.4, 5.9, abs(p.x))) * step(-36.0, p.z);
        vec2 m = masonry(p.xz * vec2(1.0, 1.0) + vec2(0.0, 0.3), 1.1, 1.6);
        float joint = 1.0 - smoothstep(0.03, 0.12 + lod * 0.2, m.x);
        float wn = fbm(p.xz * 0.25);
        vec3 flag = mix(uStone2, uStone, 0.3 + 0.5 * wn) * (0.8 + 0.25 * m.y);
        float fm = smoothstep(0.35, 0.75, wn + joint * 0.6);
        vec3 floorC = mix(uGround, uMoss2, smoothstep(0.35, 0.7, fbm(p.xz * 0.4 + 7.0)));
        float litter = step(0.78, hash(floor(p.xz * 3.0)));
        floorC = mix(floorC, uGround * vec3(1.4, 1.05, 0.7), litter * 0.4 * (1.0 - lod));
        alb = mix(floorC, flag, paved);
        moss = paved * sat(fm * 0.9 + joint * 0.6) * (uKind == 2 ? 0.5 : 1.0) * (0.1 + 0.9 * uWear) + uJungle * 0.6;
        moss = max(moss, (1.0 - paved) * 0.5 * smoothstep(0.4, 0.7, fbm(p.xz * 0.6)));
    }
    vec3 mossC = mix(uMoss2, uMoss, smoothstep(0.3, 0.8, vnoise(p.xz * 1.7 + p.y)));
    alb = mix(alb, mossC, moss);

    // ---- light
    vec3 L = uSunDir;
    float dif = max(dot(nn, L), 0.0);
    float sh = 0.0;
    float through = sunThrough(p);
    if (dif > 0.001 && through > 0.02) sh = softShadow(p + n * 0.04, L, 70.0);
    vec3 lin = uSunCol * dif * sh * through;
    lin += uAmb * ao * (0.55 + 0.45 * nn.y);
    vec3 bounce = mix(uGround, uStone, 0.6) * (uSunCol * 0.16 + uAmb * 0.25);
    lin += bounce * ao * sat(0.6 - 0.6 * nn.y);
    // the warm light thrown back by the sunlit stones around
    lin += uSunCol * uStone * 0.12 * ao * sat(dot(nn, normalize(vec3(-L.x, 0.25, -L.z))));
    // the braziers' fire on the stones around
    if (uPrime > 0.0) {
        for (int i = 0; i < 4; i++) {
            vec3 Lb = BRZ[uKind * 4 + i] + vec3(0.0, 1.8, 0.0) - p;
            float att = 1.0 / (1.0 + dot(Lb, Lb) * 0.12);
            lin += vec3(2.4, 1.1, 0.35) * uPrime * att * (0.25 + max(dot(nn, normalize(Lb)), 0.0)) * ao;
        }
    }
    vec3 col = alb * lin;
    // the dust of a block re-forming: golden motes swirling on it
    if (id == 1.0 && uBroken > 0.0) {
        float k;
        float dm = missing(p - TC - n * 0.25, k);
        float f = k * (1.0 - k) * 4.0;
        if (f > 0.01 && dm < 0.5) {
            float sp = vnoise(uv * 14.0 + uTime * 3.0) * vnoise(uv * 5.0 - uTime);
            col += vec3(1.0, 0.75, 0.42) * f * (0.25 + 2.5 * sp * sp) * (0.6 + uGold);
        }
    }
    // a soft sheen on the moss in the sun
    col += mossC * uSunCol * moss * sh * through * 0.12 * pow(sat(dot(reflect(rd, nn), L)), 4.0);
    return col;
}

void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 rd = normalize(uFwd + uRight * (ndc.x * uTanH * uAspect) + uUp * (ndc.y * uTanH));
    vec3 ro = uEye;
    // ---- the solid world
    float t = 0.05, id = -1.0;
    for (int i = 0; i < 170; i++) {
        vec3 p = ro + rd * t;
        vec2 h = map(p);
        if (h.x < 0.0011 * t) { id = h.y; break; }
        t += h.x * 0.9;
        if (t > 190.0 || (p.y > 36.0 && rd.y > 0.0)) break;
    }
    float tS = id >= 0.0 ? t : 1e9;
    // ---- the foliage planes in front of it
    Lay best, edge;
    best.t = tS; best.a = 0.0; best.kind = 0.0; best.seed = 0.0; best.p = vec3(0.0); best.uv = vec2(0.0);
    edge = best;
    edge.t = 1e9;
    foliage(ro, rd, min(tS, 400.0), best, edge);
    vec3 col;
    float tEnd;
    if (best.kind > 0.0) {
        col = foliageCol(best, rd);
        tEnd = best.t;
    } else if (id >= 0.0) {
        vec3 p = ro + rd * t;
        col = shadeSolid(p, calcN(p, t), id, rd, t);
        tEnd = t;
    } else {
        col = sky(rd);
        tEnd = 1e9;
    }
    if (edge.t < tEnd) col = mix(col, foliageCol(edge, rd), edge.a * 1.6);
    if (uPrime > 0.01) {
        for (int i = 0; i < 4; i++) {
            vec3 fp = BRZ[uKind * 4 + i] + vec3(0.0, 1.55 + 0.35 * uPrime - 1.4 * (1.0 - uPrime), 0.0);
            float tc = dot(fp - ro, rd);
            if (tc < 0.0 || tc > tEnd + 0.5) continue;
            float dd = length(ro + rd * tc - fp);
            float fl = 0.85 + 0.15 * sin(uTime * 9.0 + float(i) * 2.0) * sin(uTime * 5.3 + float(i));
            float px = max(tc * uPx, 0.02);
            col += vec3(3.0, 1.4, 0.4) * uPrime * fl * (exp(-sq(dd / max(0.32, px))) + 0.08 * exp(-dd * 0.9));
        }
    }

    // ---- mist and light shafts along the ray (the canopy's light at each sample)
    float tm = min(tEnd, 170.0);
    const int N = 22;
    float dt = tm / float(N);
    float jit = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float mu = dot(rd, uSunDir);
    const float g = 0.45;
    float hg = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5) * 0.12;
    float phase = 0.05 + hg;
    vec3 scat = vec3(0.0);
    float trans = 1.0;
    for (int i = 0; i < N; i++) {
        vec3 p = ro + rd * (dt * (float(i) + jit));
        float y = max(p.y, 0.0);
        float mist = uMist * 0.6 * exp(-y / 2.0) * (0.3 + 1.2 * vnoise(p.xz * 0.07 + vec2(uTime * 0.045, uTime * 0.017)));
        float dens = uFogK * (exp(-y / 16.0) + mist);
        float sun = sunThrough(p);
        vec3 Ls = uSunCol * sun * sun * phase * uShaft * 2.2 + uAmb * 0.4;
        float a = dens * dt;
        scat += trans * a * Ls * uFog;
        trans *= exp(-a);
    }
    if (tEnd > 1e8) {
        // the sky beyond: the rest of the haze, uniform
        trans *= 1.0;
    }
    col = col * trans + scat;
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
    c = max(c + (c - n * 0.25) * 0.15, 0.0);
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ sprites
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
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
void main() {
    int mode = int(vA.w + 0.5);
    vec2 q = vLocal;
    float r = length(q);
    vec3 c = vB.rgb;
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 1) {
        // the gauge: a fine ring, the charge an arc of light from the top, clockwise
        float ring = 1.0 - smoothstep(px * 0.6, px * 1.6, abs(r - 0.8));
        float ang = atan(q.x, q.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = (1.0 - smoothstep(px * 1.4, px * 3.0, abs(r - 0.8))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.5 + 0.5 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, v * vB.a * 0.6);
        return;
    }
    if (mode == 2) {
        // a stone chip: an irregular angular fragment, lit from the top left
        vec2 p = rot(vC.x) * q;
        float k = vC.y;
        float d = max(max(dot(p, normalize(vec2(1.0, 0.3 + k))), dot(p, normalize(vec2(-0.8, 1.0)))),
                      max(dot(p, normalize(vec2(-0.5 - k, -1.0))), dot(p, normalize(vec2(0.9, -0.7))))) - 0.55;
        float a = 1.0 - smoothstep(-px * 1.5, px * 1.5, d);
        float lit = 0.75 + 0.35 * sat(dot(normalize(p + vec2(0.001)), vec2(-0.6, 0.8)) * (1.0 - sat(-d / 0.3)));
        frag = vec4(c * lit * a * vB.a, a * vB.a);
        return;
    }
    if (mode == 3) {
        // a bird: body and two beating wings, a dark silhouette against the light
        float ph = vC.x;
        float wy = sin(ph) * 0.5;
        float d = length(q * vec2(1.0, 2.2)) - 0.16;
        for (int s = -1; s <= 1; s += 2) {
            vec2 tip = vec2(float(s) * 0.92, wy + 0.08);
            vec2 mid = vec2(float(s) * 0.45, 0.12 + wy * 0.35 + 0.06);
            d = min(d, min(sdSeg(q, vec2(0.0, 0.03), mid) - 0.09, sdSeg(q, mid, tip) - 0.035));
        }
        float a = 1.0 - smoothstep(-px * 1.2, px * 1.2, d);
        frag = vec4(c * a * vB.a, a * vB.a);
        return;
    }
    if (mode == 4) {
        // a butterfly seen from the front: two pairs of wings folding and opening
        float open = 0.25 + 0.75 * abs(cos(vC.x));
        vec2 p = vec2(abs(q.x) / max(open, 0.08), q.y);
        float up = length((p - vec2(0.45, 0.22)) * vec2(1.0, 1.25)) - 0.36;
        float lo = length((p - vec2(0.32, -0.3)) * vec2(1.2, 1.0)) - 0.24;
        float d = min(min(up, lo), length(q * vec2(5.0, 1.0)) - 0.3);
        float a = 1.0 - smoothstep(-px * 1.5 / max(open, 0.3), px * 1.5 / max(open, 0.3), d);
        vec3 col = mix(c, c * 0.35, smoothstep(-0.12, 0.0, max(up, lo) + 0.06));
        frag = vec4(col * a * vB.a, a * vB.a);
        return;
    }
    // a puff of dust
    float v = exp(-r * r * 4.0) * smoothstep(1.0, 0.6, r) * (0.7 + 0.3 * vnoise(q * 3.0 + vC.x * 9.0));
    frag = vec4(c * v * vB.a, v * vB.a);
}
)";

// ------------------------------------------------------------------------------------------------ the backing
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
    float a = uWellA * (1.0 - smoothstep(-0.1, 0.6, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.6 + exp(-abs(dw) * 4.0) * 0.06;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ pieces: drums
// Each cell one carved stone column drum seen from the front: a clean silhouette (square, its corners worn), the
// cylinder's roundness in its shading (lit from the top left), shallow vertical fluting, a carved band near its
// top, the joint lines top and bottom, the stone's own grain and pits, a touch of moss low in the flutes now and
// then, and a soft contact shadow. A cleared drum cracks and crumbles away, piece by piece.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, seed
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 drum, 1 ghost), erase 0..1, dull, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
const float EXT = 1.2;
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
uniform vec3 uBridgeInk, uLightC, uMossC;
float sdBoxR(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
// cracks: the distance to the borders of a few fragments, and the fragment's id
vec3 voro(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 9.0, d2 = 9.0;
    vec2 best = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 o = hash2(i + g) * 0.8 + 0.1;
            float d = length(g + o - f);
            if (d < d1) { d2 = d1; d1 = d; best = i + g; }
            else if (d < d2) d2 = d;
        }
    return vec3(d2 - d1, best);
}
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    vec2 q = vLocal;
    vec2 sd = vA.xy * vec2(0.173, 0.291) + vA.w;
    // the silhouette: a square drum, its corners worn (each its own)
    float wear = 0.06 + 0.05 * hash(sd * 7.0);
    float d = sdBoxR(q, vec2(0.465, 0.465), wear);
    d += 0.012 * (vnoise(q * 16.0 + sd * 31.0) - 0.5);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(d + 0.035));
        float flutes = (1.0 - smoothstep(0.0, aa * 1.5, abs(fract((q.x + 0.465) / 0.93 * 5.0) - 0.5) - 0.47)) * step(d, -0.06) * 0.25;
        float a = (ring * 0.8 + flutes + step(d, 0.0) * 0.07) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // crumbling: cracks open, then the fragments fall away one after the other
    float frag_ = 1.0;
    float crack = 0.0;
    if (erase > 0.0) {
        vec3 v = voro(q * 3.2 + sd * 5.0);
        float delay = hash(v.yz + sd) * 0.5;
        float k = sat((erase - delay) / 0.5);
        crack = 1.0 - smoothstep(0.0, 0.05 + 0.25 * erase, v.x);
        frag_ = smoothstep(k * 0.42, k * 0.42 + 0.03, v.x) * step(k, 0.98);
    }
    float inside = 1.0 - smoothstep(-aa, aa, d);
    float ds = sdBoxR(q - vec2(0.04, -0.055), vec2(0.465), wear);
    float shadow = (1.0 - smoothstep(-0.02, 0.09, ds)) * (1.0 - inside) * 0.55 * (1.0 - smoothstep(0.0, 0.3, erase));
    // the drum's roundness (a cylinder across x), the flutes, the carved band, the joints
    float x = clamp(q.x / 0.5, -1.0, 1.0);
    float fl = (q.x + 0.465) / 0.93 * 5.0;
    float ff = fract(fl) - 0.5;
    float inFlute = step(-0.33, q.y) * step(q.y, 0.24);
    vec3 n = vec3(x * 0.8, 0.0, sqrt(max(1.0 - x * x * 0.64, 0.0)));
    n.x += -ff * 1.1 * inFlute * (1.0 - smoothstep(0.38, 0.5, abs(ff)));
    float band = smoothstep(0.035, 0.0, abs(q.y - 0.32) - 0.035);
    float groove = smoothstep(0.012, 0.0, abs(q.y - 0.27) - 0.008) + smoothstep(0.012, 0.0, abs(q.y - 0.37) - 0.008);
    n.y += band * sign(q.y - 0.32) * 0.6 + smoothstep(0.08, 0.0, abs(q.y + 0.36)) * -0.3;
    // the stone's grain and its pits
    float h = fbm(q * 5.0 + sd * 17.0);
    n.xy += (vec2(fbm(q * 9.0 + sd * 11.0 + 0.03), fbm(q * 9.0 + sd * 11.0 + 7.0)) - 0.5) * 0.35;
    n = normalize(n);
    vec3 L = normalize(vec3(-0.55, 0.6, 0.7));
    float lit = max(dot(n, L), 0.0);
    vec3 c = t * mix(0.86, 1.1, smoothstep(0.3, 0.75, h));
    float pit = step(0.9, hash(floor((q + sd) * 30.0))) * smoothstep(0.4, 0.15, length(fract((q + sd) * 30.0) - 0.5));
    c *= 1.0 - 0.3 * pit;
    c *= 0.5 + 0.62 * lit + 0.1 * n.z;
    c *= 1.0 - 0.35 * groove;
    // the fluting's arrises catch the light, the flute bottoms hold a little shade
    c *= 1.0 - 0.12 * inFlute * smoothstep(0.25, 0.48, abs(ff));
    // moss low in the flutes, on some drums
    float mossy = step(0.55, hash(sd * 3.1)) * smoothstep(0.62, 0.78, fbm(q * 4.5 + sd * 9.0) + smoothstep(0.1, -0.5, q.y) * 0.35);
    mossy *= smoothstep(-0.05, -0.45, q.y);
    c = mix(c, uMossC * (0.7 + 0.5 * lit), mossy * 0.75);
    // a crisp edge: a thin light top-left bevel, a darker rim all round
    float edgeIn = 1.0 - smoothstep(0.0, 0.05, -d);
    c = mix(c, mix(t, vec3(1.0), 0.4), edgeIn * sat(dot(normalize(q + 1e-5), vec2(-0.6, 0.8))) * 0.4);
    c = mix(c, t * 0.45, 1.0 - smoothstep(0.0, 0.018 + aa, -d));
    c *= 1.0 - 0.6 * crack;
    c = mix(c, c * 0.45, smoothstep(0.2, 1.0, erase));
    float dull = vC.z;
    c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))) * 0.5, dull);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * frag_;
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
    c *= 1.0 - 0.32 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.7, 0.0) * 0.85);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace ruins_shaders

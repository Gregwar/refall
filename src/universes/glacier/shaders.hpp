#pragma once
// GLACIER's shaders: the ice cave (arches of translucent ice receding to a bright opening, icicles, a snowy floor,
// ice chunks in the foreground, light shafts, the drop's light from within the ice), the water that fills the cave
// (its body mirroring the ice, its rippled surface), the glass case of the well and the frosted frames of the
// previews, the ice cubes, the particles, and the post (the water in front: what is under it looks submerged, its
// surface a thin bright line across the whole screen).

namespace glacier_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
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
float fbm3(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.875;
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float rbox(vec2 p, vec2 h, float r) { vec2 q = abs(p) - h + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
// distance to the nearest edge of a Voronoi cell (0 on the edges)
float vorEdge(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = g + hash2(i + g) - f;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return sqrt(d2) - sqrt(d1);
}
)";

// The water's surface (board units): its level, a slow swell, and the ripples (x, age, amplitude, width). The same
// function in C++ (GlacierUniverse::surface) for the drips that fall into it.
inline const char* WATER_GLSL = R"(
uniform float uLevel, uWaveA, uWaveT;
uniform vec4 uRip[16];
float surf(float x) {
    float s = uLevel + uWaveA * (0.6 * sin(0.55 * x + 0.5 * uWaveT) + 0.3 * sin(1.7 * x - 0.8 * uWaveT + 1.3) +
                                 0.12 * sin(4.1 * x + 1.6 * uWaveT));
    for (int i = 0; i < 16; i++) {
        vec4 r = uRip[i];
        if (r.z <= 0.0) continue;
        float d = abs(x - r.x), front = 2.2 * r.y;
        float env = exp(-sq((d - front) / (r.w + 0.5 * r.y))) * exp(-1.4 * r.y);
        s += r.z * env * cos((d - front) * 3.0 / r.w);
    }
    return s;
}
)";

// ------------------------------------------------------------------------------------------------ the cave
// Front to back: the near arch (the screen's sides and top, its icicles hanging across the top), the middle arch,
// the floor, the far arch, and the opening, bright, behind the well. Each wall is translucent ice: deep where it is
// thick, glowing at its thin edges where the light from the opening comes through, draped in long vertical folds,
// frosted on the faces turned up. Light shafts fan down from the opening; the drop lights the ice from within.
inline const char* CAVE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform float uTime, uBreath, uDrop, uShaft, uSlow, uAurora, uNight, uGlint, uWorldA, uPx;
uniform vec4 uReveal; // the light reached: the opening, the far arch, the middle one, the near one
uniform vec3 uDeep, uEdge, uFar, uOpen, uFloor, uShaftC, uGlowA, uGlowB, uBridgeCol;
uniform float uWarm;   // the drop's low sun: warm rays through the cave (0 none)
uniform vec3 uWarmC;
uniform vec4 uColL, uColR;
uniform int uSamples;

const vec2 O = vec2(0.0, 4.0); // the opening's heart

float archD(vec2 p, vec2 ab, float yc, float seed, float rough) {
    float d;
    if (p.y < yc) d = abs(p.x) - ab.x;
    else {
        vec2 q = vec2(p.x / ab.x, (p.y - yc) / ab.y);
        float l = length(q);
        d = (l - 1.0) * mix(ab.x, ab.y, q.y / max(l, 1e-3));
    }
    d += (fbm3(vec2(p.x * 0.16, p.y * 0.11) + seed) - 0.5) * rough;
    d += (vnoise(vec2(p.x * 0.55, p.y * 0.3) + seed * 3.0) - 0.5) * rough * 0.4;
    return d; // > 0: in the wall
}

// icicles hanging under an arch's top edge: x > 0 inside one (column units), y across it (-1..1), z along it (0 at
// its root, 1 at its tip)
vec3 icicle(vec2 p, float d, float w, float L, float seed, float top) {
    vec3 best = vec3(-1.0, 0.0, 0.0);
    if (d > 0.0 || top <= 0.0) return best;
    for (int k = 0; k < 2; k++) {
        float ww = k == 0 ? w : w * 0.5;
        float x = p.x / ww + float(k) * 0.37;
        float c = floor(x), fx = fract(x) - 0.5;
        float h = hash(vec2(c, seed + float(k) * 7.1));
        float keep = step(k == 0 ? 0.45 : 0.25, hash(vec2(c + 3.7, seed + float(k))));
        float len = (k == 0 ? L : L * 0.45) * (0.25 + 0.75 * h * h * h) * top * keep;
        float t = -d;
        if (t > len) continue;
        float along = t / max(len, 1e-3);
        float hw = 0.36 * pow(1.0 - along, 1.25) * (0.88 + 0.12 * sin(t * 6.0 + h * 30.0));
        float v = (hw - abs(fx)) * ww / w;
        if (v > best.x) best = vec3(v, fx / max(hw, 1e-3), along);
    }
    return best;
}

vec3 innerGlow(vec2 p, float thick, float k) {
    if (uDrop <= 0.002) return vec3(0.0); // (no drop: none of its cost)
    vec2 q = p * (0.32 + 0.08 * k) + vec2(k * 3.1, uTime * 0.025);
    float vein = exp(-vorEdge(q) / 0.07);
    float vein2 = exp(-vorEdge(q * 2.4 + 5.3 - vec2(0.0, uTime * 0.02)) / 0.05);
    float body = 0.4 + 0.6 * (1.0 - thick);
    vec3 gc;
    float amt = 1.0;
    if (uAurora > 0.5) {
        // aurora curtains through the ice: long vertical bands drifting, green to violet
        float cur = fbm3(vec2(p.x * 0.16 + uTime * 0.035, p.y * 0.035 + k));
        gc = mix(uGlowA, uGlowB, smoothstep(0.3, 0.75, fbm3(vec2(p.x * 0.06 - uTime * 0.015, p.y * 0.1 + 2.0))));
        amt = 0.35 + 1.1 * smoothstep(0.35, 0.75, cur);
    } else {
        // the sun through the ice: warm at the opening, cold caustics farther out
        gc = mix(uGlowB, uGlowA, exp(-length(p - O) * 0.06));
    }
    // the light gathers in drifting patches: veins lit here and there, never a net over everything
    float patch = smoothstep(0.35, 0.75, fbm3(p * 0.11 + vec2(uTime * 0.02 + k * 5.0, -uTime * 0.015)));
    return gc * amt * ((vein * 0.6 + vein2 * 0.25) * (0.15 + 0.85 * patch) + body * 0.75 * (0.45 + 0.55 * patch));
}

// A wall of the cave: translucent ice in great bulging masses, its face dimpled with the scallops of melting ice
// (each a shallow bowl: a crescent of light on its far side, a fine bright ridge between them), deep where it is
// thick, the light of the opening coming through where it is thin.
vec3 iceWall(vec2 p, float d, float k, float seed) {
    vec2 q = p + vec2(seed * 17.3, seed * 5.1);
    float thick = 1.0 - exp(-max(d, 0.0) * (0.3 + 0.25 * k));
    // the big forms
    float sc = 0.2 + 0.07 * k;
    float h0 = fbm3(q * sc), hx = fbm3(q * sc + vec2(0.12, 0.0)), hy = fbm3(q * sc + vec2(0.0, 0.12));
    vec2 gBig = vec2(h0 - hx, h0 - hy) * 9.0;
    // long vertical folds
    float fold = fbm3(vec2(q.x * 0.7, q.y * 0.12));
    gBig.x += (fold - 0.5) * 1.6;
    // the scallops
    vec2 sp = q * (1.25 + 0.6 * k) * vec2(1.0, 0.7);
    vec2 ip = floor(sp), fp = fract(sp);
    float d1 = 8.0, d2 = 8.0;
    vec2 rel = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = g + 0.15 + 0.7 * hash2(ip + g) - fp;
            float dd = dot(o, o);
            if (dd < d1) { d2 = d1; d1 = dd; rel = o; } else if (dd < d2) d2 = dd;
        }
    float edge = sqrt(d2) - sqrt(d1);
    vec3 n = normalize(vec3(gBig + rel * 0.85 * vec2(1.0, 0.7) * smoothstep(0.0, 0.3, edge), 1.0));
    vec3 L = normalize(vec3(O - p, 5.0));
    float lam = max(dot(n, L), 0.0);
    float spec = pow(max(dot(reflect(-L, n), vec3(0.0, 0.0, 1.0)), 0.0), 18.0);
    // brighter toward the opening, darker toward the frame's edges
    float near = exp(-length((p - O) * vec2(0.8, 1.0)) * (0.07 - 0.015 * k));
    vec3 base = mix(uEdge, uDeep, thick);
    vec3 c = base * (0.18 + 1.1 * lam) * (0.42 + 0.85 * near);
    c += uOpen * spec * 0.22 * (0.4 + near);
    c += mix(uEdge, uOpen, 0.5) * (1.0 - smoothstep(0.0, 0.05, edge)) * 0.05 * (0.5 + near);
    // the light through the thin edge (uneven)
    float thin = exp(-max(d, 0.0) * 1.4) * (0.45 + 0.9 * vnoise(q * 0.9));
    c += mix(uEdge, uOpen, 0.4) * thin * 0.38;
    // frost, glints
    float fr = smoothstep(0.62, 0.92, vnoise(q * 4.5)) * smoothstep(-0.2, 0.5, n.y);
    c += uOpen * fr * 0.07;
    float gl = smoothstep(0.985, 1.0, hash(floor(q * 6.0))) * (0.5 + 0.5 * sin(uTime * 2.0 + hash(floor(q * 6.0) + 3.0) * 40.0)) *
               smoothstep(0.3, 0.0, length(fract(q * 6.0) - 0.5));
    c += uOpen * gl * (0.12 + 0.6 * uGlint) * (1.0 - thick * 0.5);
    c *= uBreath;
    c += innerGlow(p, thick, k) * uDrop * (0.8 - 0.12 * k) * (0.5 + 0.7 * lam) * (0.45 + 0.9 * near);
    // the haze of the cave's depth
    return mix(c, uFar * (0.7 + 0.3 * uBreath), 0.03 + 0.14 * k);
}

vec3 icicleCol(vec3 ic, float k) {
    float across = ic.y, along = ic.z;
    vec3 c = mix(uDeep, uEdge, 0.45 + 0.4 * along) * (0.55 + 0.45 * (0.5 - 0.5 * across));
    c += uOpen * exp(-sq((across + 0.45) / 0.18)) * 0.4;
    c += uOpen * pow(along, 5.0) * 0.4;
    c *= uBreath;
    c += innerGlow(vec2(across, along * 3.0), 0.2, k) * uDrop * 0.4;
    return mix(c, uFar, 0.05 + 0.2 * k);
}

vec4 chunk(vec2 p, vec4 ch) {
    vec2 q = rot(ch.w) * (p - ch.xy) / ch.z;
    float d = max(rbox(q, vec2(0.62, 0.42), 0.05), dot(q, normalize(vec2(0.75, 0.66))) - 0.5);
    float aa = uPx / ch.z * 1.5;
    float a = 1.0 - smoothstep(-aa, aa, d);
    if (a <= 0.0) return vec4(0.0);
    float top = step(0.16 + 0.2 * q.x, q.y);
    float side = step(0.28, q.x - 0.35 * q.y);
    vec3 c = mix(uDeep, uEdge, 0.5) * uBreath;
    c *= top > 0.5 ? 1.45 : side > 0.5 ? 0.62 : 0.95;
    c += mix(uEdge, uOpen, 0.5) * exp(d * 16.0) * 0.55;
    c += uOpen * (1.0 - smoothstep(0.0, 0.025, abs(q.x * 0.8 - q.y * 0.5 + 0.1 + 0.06 * sin(q.y * 9.0)))) * 0.25;
    c += uOpen * smoothstep(0.03, 0.0, abs(q.y - 0.16 - 0.2 * q.x)) * 0.25 * step(q.x, 0.28 + 0.35 * q.y);
    c += innerGlow(p, 0.5, 0.0) * uDrop * 0.5;
    return vec4(c, a);
}

// The cave's layers at p: its color and the light reached there (a), how near the ice is (for the shafts), and how
// far the nearest edge between two layers is (board units: the pixels near one are supersampled).
vec4 caveAt(vec2 p, out float nearMask, out float edge) {
    vec3 col;
    float reveal;
    nearMask = 0.0;
    float d0 = archD(p, vec2(17.2, 11.4), 0.0, 0.0, 3.4);
    vec3 i0 = icicle(p, d0, 1.0, 4.2, 1.0, smoothstep(2.0, 8.0, p.y));
    edge = min(abs(d0), i0.x > -0.5 ? abs(i0.x) : 1e3);
    if (d0 > 0.0) {
        col = iceWall(p, d0, 0.0, 0.0);
        reveal = uReveal.w;
        nearMask = 1.0;
    } else if (i0.x > 0.0) {
        col = icicleCol(i0, 0.0);
        reveal = uReveal.w;
        nearMask = 1.0;
    } else {
        // a soft shadow of the nearer ice on what is behind it
        float sh0 = exp(d0 * 0.9) * 0.45;
        float d1 = archD(p, vec2(12.8, 10.0), -0.5, 5.3, 2.6);
        vec3 i1 = icicle(p, d1, 0.7, 2.8, 2.0, smoothstep(1.0, 7.0, p.y));
        float floorTop = -6.6 - 0.025 * p.x * p.x + (fbm3(vec2(p.x * 0.22, 3.0)) - 0.5) * 1.2;
        edge = min(edge, min(abs(d1), i1.x > -0.5 ? abs(i1.x) * 0.7 : 1e3));
        if (d1 > 0.0 || i1.x > 0.0) {
            col = d1 > 0.0 ? iceWall(p, d1, 1.0, 5.3) : icicleCol(i1, 1.0);
            reveal = uReveal.z;
            nearMask = 0.6;
        } else {
            float sh1 = exp(d1 * 0.9) * 0.35;
            edge = min(edge, abs(p.y - floorTop));
            if (p.y < floorTop) {
                float far = smoothstep(-11.5, -6.0, p.y);
                col = uFloor * (0.32 + 0.5 * far) * uBreath;
                col *= 0.82 + 0.18 * fbm3(vec2(p.x * 0.5, p.y * 2.4));
                col += uFloor * smoothstep(0.55, 0.85, vnoise(vec2(p.x * 1.4, p.y * 5.0))) * 0.1;
                col += uOpen * exp(-(floorTop - p.y) * 3.0) * 0.15;
                col += mix(uFar, uOpen, 0.5) * exp(-abs(p.x) * 0.25) * far * 0.35;
                col = mix(col, uFar, far * 0.3);
                col += innerGlow(p, 0.8, 1.0) * uDrop * 0.2;
                reveal = uReveal.y;
            } else {
                float d2 = archD(p, vec2(8.2, 7.4), 0.5, 11.7, 1.8);
                vec3 i2 = icicle(p, d2, 0.45, 1.8, 3.0, smoothstep(1.0, 6.0, p.y));
                edge = min(edge, min(abs(d2), i2.x > -0.5 ? abs(i2.x) * 0.45 : 1e3));
                float d3 = d2 > 0.0 || i2.x > 0.0 ? -1.0 : archD(p, vec2(4.8, 5.4), 1.5, 21.3, 1.3);
                if (d2 > 0.0 || i2.x > 0.0) {
                    col = d2 > 0.0 ? iceWall(p, d2, 2.0, 11.7) : icicleCol(i2, 2.0);
                    reveal = uReveal.y;
                } else if (d3 > 0.0) {
                    // the far end of the cave, in its haze
                    edge = min(edge, d3);
                    col = iceWall(p, d3, 3.0, 21.3) * (1.0 - exp(d2 * 1.2) * 0.25);
                    reveal = uReveal.x;
                } else {
                    // the opening: the light at the end of the cave
                    edge = min(edge, -d3);
                    float sh2 = exp(d2 * 1.2) * 0.25;
                    float r = length((p - O) * vec2(0.55, 1.0));
                    col = mix(uFar, uOpen, exp(-r * 0.3));
                    col += uOpen * exp(-r * 0.7) * 0.5;
                    col *= 0.88 + 0.12 * fbm3(vec2(p.x * 0.45, p.y * 0.09));
                    col *= 1.0 - sh2;
                    col *= mix(1.0, uBreath, 0.5);
                    col += mix(uGlowA, uOpen, 0.3) * uDrop * exp(-r * 0.18) * 1.1;
                    reveal = uReveal.x;
                }
                col *= 1.0 - sh1;
            }
        }
        col *= 1.0 - sh0;
    }
    return vec4(col, reveal);
}

void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    // the silhouettes antialiased: the pixels by an edge between two layers take four samples (a loop over a
    // uniform count, so that the layers' code is there once)
    float nearMask = 0.0;
    vec4 acc = vec4(0.0);
    float n = 0.0;
    for (int i = 0; i < uSamples; i++) {
        vec2 o = i == 0 ? vec2(0.125, 0.375) : i == 1 ? vec2(0.375, -0.125) : i == 2 ? vec2(-0.125, -0.375) : vec2(-0.375, 0.125);
        float nm, e;
        acc += caveAt(p + o * uPx, nm, e);
        n += 1.0;
        if (i == 0) {
            nearMask = nm;
            if (e > uPx) break;
        }
    }
    vec4 cr = acc / n;
    vec3 col = cr.rgb;
    float reveal = cr.a;
    // ice chunks in the foreground
    {
        vec4 C[6] = vec4[6](vec4(-8.1, -10.7, 1.0, 0.35), vec4(-6.4, -11.3, 0.65, -0.4), vec4(6.9, -11.0, 0.8, 0.6),
                            vec4(9.4, -10.5, 1.25, -0.2), vec4(14.6, -10.9, 1.5, 0.25), vec4(19.0, -10.3, 1.15, -0.5));
        for (int i = 0; i < 6; i++) {
            vec4 c = chunk(p, C[i]);
            col = mix(col, c.rgb, c.a);
            if (c.a > 0.0) reveal = mix(reveal, uReveal.w, c.a);
        }
    }
    // shafts of light fanning down from the opening
    {
        vec2 v = p - vec2(0.0, 15.0);
        float ang = atan(v.x, -v.y), rr = length(v);
        float n = vnoise(vec2(ang * 15.0, uTime * 0.05)) * 0.65 + vnoise(vec2(ang * 37.0, uTime * 0.08 + 5.0)) * 0.35;
        float sh = smoothstep(0.45, 0.95, n) * exp(-rr * 0.045) * smoothstep(3.0, 9.0, rr);
        col += uShaftC * sh * uShaft * (1.0 - 0.65 * nearMask);
        // the drop: a low sun's warm rays, wider and slower, across the ice they cross (the rest of the cave stays
        // cold): the ice in them turned orange-red, a warm haze along them
        if (uWarm > 0.002) {
            // (the rays turn slowly round the opening, about 3 degrees a second)
            float aw = ang + uTime * 0.05;
            float nw = vnoise(vec2(aw * 7.0, uTime * 0.03 + 11.0)) * 0.7 + vnoise(vec2(aw * 19.0, uTime * 0.05 + 3.0)) * 0.3;
            float ray = smoothstep(0.42, 0.85, nw) * exp(-rr * 0.03) * smoothstep(2.0, 7.0, rr) * uWarm;
            float lc = dot(col, vec3(0.2126, 0.7152, 0.0722));
            col = mix(col, uWarmC * lc * 1.6, 0.75 * ray);
            col += uWarmC * ray * 0.18 * (1.0 - 0.5 * nearMask);
        }
    }
    // the HUD's columns: the ice a little quieter and deeper behind them
    {
        float m = 0.0;
        for (int i = 0; i < 2; i++) {
            vec4 r = i == 0 ? uColL : uColR;
            vec2 c = (r.xy + r.zw) * 0.5, h = (r.zw - r.xy) * 0.5;
            m = max(m, 1.0 - smoothstep(-0.5, 2.5, rbox(p - c, h, 0.5)));
        }
        vec3 calm = mix(uDeep, uEdge, 0.3) * 0.7;
        col = mix(col, mix(calm, col, 0.4), m * 0.6);
    }
    col *= mix(0.04, 1.0, reveal);
    // slow motion: a deep blue hush
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    col = mix(col, vec3(0.35, 0.55, 1.0) * l * 0.8, uSlow * 0.6);
    col = mix(uBridgeCol, col, uWorldA);
    frag = vec4(col, 1.0);
}
)";


// ------------------------------------------------------------------------------------------------ the water body
// The cave at the full resolution, the water over its bottom: dark and still, the ice mirrored near the surface
// (rippled), the submerged floor seen through it just under the surface, rings spreading where drips fall.
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uCave;
uniform vec2 uViewC, uViewH;
uniform vec3 uWater, uWaterHi, uOpen, uDeep, uEdge;
uniform float uTime, uWaterA, uLight;
uniform vec2 uRes;
uniform vec4 uFloat[5]; // the floating chunks: x, -, size, tilt
void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    vec3 c = texture(uCave, vUv).rgb;
    float s = surf(p.x);
    float depth = s - p.y;
    if (depth > 0.0 && uWaterA > 0.0) {
        float rings = 0.0, bend = 0.0;
        for (int i = 0; i < 16; i++) {
            vec4 r = uRip[i];
            if (r.z <= 0.0) continue;
            float front = 2.2 * r.y;
            float rr = length(vec2(p.x - r.x, depth * 5.0));
            float k = exp(-sq((rr - front) / (0.25 + 0.3 * r.y))) * exp(-1.2 * r.y) * min(r.z * 6.0, 1.0);
            rings += k;
            bend += k * sin((rr - front) * 9.0);
        }
        float wob = (0.06 * sin(p.y * 5.0 + uTime * 1.1 + p.x * 0.3) + 0.03 * sin(p.x * 2.3 - uTime * 0.7)) * exp(-depth * 0.25) + bend * 0.12;
        vec2 rp = vec2(p.x + wob, s + depth * 1.5);
        vec2 ruv = (rp - uViewC) / uViewH * 0.5 + 0.5;
        vec3 refl = textureLod(uCave, clamp(ruv, 0.0, 1.0), 1.5).rgb;
        float clear = exp(-depth * 0.8);
        vec3 w = uWater * (0.75 + 0.45 * exp(-depth * 0.35));
        w = mix(w, c * uWaterHi * 1.3, clear * 0.45);
        w += refl * uWaterHi * 0.3 * exp(-depth * 0.3);
        w += uWaterHi * rings * 0.12;
        c = mix(c, w, uWaterA);
    }
    // ice floating on the water: chunks bobbing at the surface, half under it
    for (int i = 0; i < 5; i++) {
        vec4 ch = uFloat[i];
        float sy = surf(ch.x);
        float tilt = (surf(ch.x + 0.4) - surf(ch.x - 0.4)) * 0.8 + ch.w;
        vec2 q = rot(tilt) * (p - vec2(ch.x, sy - 0.05 * ch.z)) / ch.z;
        float d = max(max(rbox(q, vec2(0.62, 0.46), 0.07), dot(q, normalize(vec2(-0.6, 0.8))) - 0.42), dot(q, normalize(vec2(0.8, 0.6))) - 0.55);
        float aa = 2.0 * uViewH.y / uRes.y / ch.z * 1.2;
        float a = (1.0 - smoothstep(-aa, aa, d)) * uWaterA;
        if (a <= 0.0) continue;
        bool above = p.y > surf(p.x);
        float top = step(0.2, q.y + 0.15 * q.x);
        float side = step(0.3, q.x - 0.3 * q.y);
        vec3 ic = mix(uDeep, uEdge, 0.55) * (top > 0.5 ? 1.55 : side > 0.5 ? 0.7 : 1.0);
        ic += mix(uEdge, uOpen, 0.6) * exp(d * 14.0) * 0.6;
        ic += uOpen * smoothstep(0.03, 0.0, abs(q.y - 0.2 + 0.15 * q.x)) * 0.3;
        if (!above) ic = mix(ic * uWaterHi * 1.6, uWater, 0.35);
        c = mix(c, ic * uLight, a);
    }
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ the case
// The well: a clear glass case, a dark blue back with a fine grid of the cells, a thick bevelled glass rim with a
// sheen down its top left; the previews in frosted ice frames (the cave blurred through them, frost at the rims).
inline const char* CASE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uCave;
uniform vec2 uViewC, uViewH;
uniform float uPx, uA, uTime, uFrameA;
uniform vec3 uWell, uGrid, uFrame, uEdge;
uniform vec4 uBoxH, uBoxN; // the frames: center, half size
void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    vec4 o = vec4(0.0);
    // the well
    float dw = rbox(p, vec2(5.0, 10.0), 0.02);
    float dr = rbox(p, vec2(5.32, 10.3), 0.3);
    if (dr < uPx * 2.0) {
        float aRim = 1.0 - smoothstep(-uPx, uPx, dr);
        if (dw < 0.0) {
            // the back: dark blue, a fine grid, a soft sheen at the top
            vec3 c = uWell * (0.8 + 0.35 * smoothstep(-10.0, 10.0, p.y));
            vec2 g = abs(fract(p + 0.5) - 0.5);
            float line = 1.0 - smoothstep(uPx * 0.4, uPx * 1.4, min(g.x, g.y));
            c += uGrid * line * 0.22;
            c += uEdge * exp(-(-dw) * 2.5) * 0.08;
            // the glass in front: a long diagonal reflection
            float sheen = exp(-sq((p.x + p.y * 0.35 - 1.2) / 0.9)) * smoothstep(-6.0, 8.0, p.y) * 0.05;
            c += vec3(sheen);
            float a = 0.96;
            o = vec4(c * a, a);
        } else {
            // the rim: thick clear glass, bright at its bevels
            float e = -dr;
            vec3 base = textureLod(uCave, vUv, 2.5).rgb;
            vec3 c = mix(base * 0.7, uFrame, 0.22);
            float bev = exp(-e / (uPx * 1.5)) + exp(-abs(dw) / (uPx * 1.5)) * 0.8;
            vec2 ax = abs(p) - vec2(5.0, 10.0);
            vec2 side = ax.x > ax.y ? vec2(sign(p.x), 0.0) : vec2(0.0, sign(p.y));
            float lit = 0.6 + 0.4 * dot(vec2(-0.6, 0.8), side);
            c += uFrame * bev * 0.55 * lit;
            c += uFrame * 0.1 * (1.0 - smoothstep(0.0, 0.3, e));
            float a = aRim * 0.85;
            o = vec4(c * a, a);
        }
    }
    // the frosted frames of the previews
    for (int i = 0; i < 2; i++) {
        vec4 b = i == 0 ? uBoxH : uBoxN;
        float d = rbox(p - b.xy, b.zw, 0.22);
        if (d > uPx * 2.0) continue;
        float a = 1.0 - smoothstep(-uPx, uPx, d);
        float e = -d;
        vec3 blur = textureLod(uCave, vUv, 4.0).rgb;
        float fr = vnoise(p * 9.0) * 0.6 + vnoise(p * 23.0) * 0.4;
        float rim = exp(-e / 0.18);
        vec3 c = blur * 0.55 + uFrame * (0.07 + 0.25 * rim * (0.6 + 0.6 * fr));
        c += uFrame * exp(-e / (uPx * 1.2)) * 0.45;
        c *= 0.85 + 0.15 * fr;
        float al = a * (0.8 + 0.15 * rim) * uFrameA;
        o = o * (1.0 - al) + vec4(c * al, al);
    }
    frag = o * uA;
}
)";

// ------------------------------------------------------------------------------------------------ the ice cubes
// One thick cube of clear ice per cell, seen a little from the front: rounded edges and corners frosted almost white
// (a cap of rime along the top), a glossy translucent body that lets the eye in deep: the colour deepening toward the
// centre, the cube's back faces refracted into four facets meeting near its middle, a few fracture planes, tiny
// trapped bubbles, caustic filaments of focused light, a soft highlight at the top left. Under the water a cube goes a
// deeper, more saturated teal. The ghost is a frosted outline. A cleared cube melts (mode 2, its progress 0..1 and the
// height left free above it): it turns clear and glossy, rounds off, and shrinks down into a thin film that fades.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, seed
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 cube, 1 ghost, 2 melting), melt progress, flash (melting: the room above), frost
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z * 1.04;
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
uniform float uTime, uBridge, uDrop, uLevel, uWaterA;
uniform vec3 uBridgeInk, uGlow, uDeepTint;
void main() {
    int mode = int(vC.x + 0.5);
    vec2 p = vLocal;
    float aa = max(length(fwidth(p)) * 0.7071, 1e-4);
    float seed = vA.w;
    vec3 tint = vB.rgb;
    // the melt: clear and glossy first, then down into a film, then gone (smooth eases, crisp shapes)
    float u = mode == 2 ? vC.y : 0.0;
    float gloss = smoothstep(0.0, 0.3, u);
    float s = smoothstep(0.18, 0.75, u);
    float hgt = mode == 2 ? min(mix(1.0, 0.11, s), max(vC.z, 0.11)) : 1.0;
    vec2 hs = vec2(mix(0.47, 0.44, s), 0.47 * hgt);
    vec2 q = p - vec2(0.0, -0.47 + hs.y);
    float r = min(mix(0.15, 0.24, gloss), hs.y);
    float sd = rbox(q, hs, r);
    float inside = 1.0 - smoothstep(-aa, aa, sd);
    if (inside <= 0.0) discard;
    float e = -sd;
    vec3 light = mix(tint, vec3(1.0), 0.3);
    if (mode == 1) {
        // the ghost: a frosted outline and a faint fill
        float rim = smoothstep(0.075 + aa, 0.05 - aa, e);
        float fr = 0.75 + 0.25 * vnoise(p * 30.0 + seed);
        float a = (rim * 0.75 + 0.08) * inside * vB.a;
        vec3 col = mix(mix(light, vec3(1.0), 0.3) * fr, uBridgeInk, uBridge);
        frag = vec4(col * a, a);
        return;
    }
    vec2 n = q / hs; // -1..1 over the cube, whatever its height
    // the body: bright and clear near the faces, the colour deepening toward the centre
    float depth = smoothstep(0.0, 0.3, e);
    float centre = 1.0 - smoothstep(0.0, 1.0, length(n));
    vec3 col = mix(mix(tint, vec3(1.0), 0.14) * 1.1, tint * 0.8, depth * 0.38 + centre * 0.26);
    // the ice's inside: cloudy, warped, lighter frosty clouds and deeper refractions
    vec2 w = n * 1.25 + seed * 2.13;
    float cloud = fbm3(w + 1.4 * vec2(fbm3(w * 1.7 + 3.1), fbm3(w * 1.7 + 7.3)));
    col *= mix(1.0, 0.74 + 0.5 * cloud, depth * (1.0 - 0.7 * gloss));
    col += vec3(0.9, 0.97, 1.0) * smoothstep(0.55, 0.82, cloud) * 0.16 * depth * (1.0 - gloss);
    // the back faces refracted: four facets meeting near the middle, the top and left ones lit
    vec2 m = n - (hash2(vec2(seed, 2.7)) - 0.5) * 0.3;
    float facet = abs(m.x) > abs(m.y) ? (m.x < 0.0 ? 0.1 : -0.12) : (m.y > 0.0 ? 0.14 : -0.16);
    float inner = smoothstep(0.95, 0.72, max(abs(n.x), abs(n.y)));
    col *= 1.0 + facet * inner * (1.0 - gloss * 0.6);
    float diag = abs(abs(m.x) - abs(m.y));
    col += light * exp(-diag / 0.05) * 0.1 * inner * (1.0 - 0.6 * centre) * (1.0 - gloss);
    col += light * exp(-abs(max(abs(n.x), abs(n.y)) - 0.66) / 0.025) * 0.09 * (1.0 - gloss);
    // fracture planes: one or two thin bright sheets inside (not in every cube)
    for (int i = 0; i < 2; i++) {
        vec2 cd = normalize(hash2(vec2(seed * 3.7, 1.3 + float(i) * 5.1)) - 0.5);
        vec2 co = (hash2(vec2(seed * 5.9, 2.1 + float(i) * 3.3)) - 0.5) * 0.7;
        float cl = abs(dot(n - co, cd));
        float k = step(0.35 + 0.3 * float(i), hash(vec2(seed, 9.1 + float(i)))) * smoothstep(0.6, 0.1, length(n - co));
        col += vec3(0.9, 0.97, 1.0) * (exp(-cl / 0.035) * 0.16 + exp(-cl / 0.16) * 0.07) * k * depth * (1.0 - gloss);
    }
    // caustics: filaments of light focused through the ice, drifting slowly
    float ca = exp(-vorEdge(n * 1.4 + seed * 3.1 + vec2(0.0, uTime * 0.04)) / 0.06) * smoothstep(0.35, 0.75, vnoise(n * 1.7 + seed * 1.3));
    col += mix(light, vec3(1.0), 0.5) * ca * 0.2 * depth;
    // tiny trapped bubbles
    for (int i = 0; i < 4; i++) {
        vec2 h = hash2(vec2(seed * 13.7, float(i) * 3.3));
        if (h.x < 0.3) continue;
        vec2 bc = (hash2(vec2(float(i) * 1.7, seed * 5.1)) - 0.5) * 1.1;
        float br = (0.014 + 0.014 * h.y) / hs.x;
        float bd = length((n - bc) * hs / 0.47);
        float ring = smoothstep(br + aa * 2.0, br, bd) * (0.4 + 0.6 * smoothstep(br * 0.3, br, bd));
        col += vec3(0.92, 0.98, 1.0) * ring * 0.35 * depth;
    }
    // the rounded edges, frosted almost white, a cap of rime along the top, lit from the top left
    vec2 kq = abs(q) - (hs - r);
    vec2 nb = kq.x > 0.0 && kq.y > 0.0 ? normalize(kq) * sign(q) : (kq.x > kq.y ? vec2(sign(q.x), 0.0) : vec2(0.0, sign(q.y)));
    float lit = dot(nb, vec2(-0.6, 0.8));
    float frost = vnoise(q * 28.0 + seed * 7.0) * 0.5 + vnoise(q * 10.0 + seed) * 0.5;
    float rimW = 0.07 + 0.05 * max(nb.y, 0.0);
    float rim = exp(-e / rimW);
    vec3 rimC = mix(light, vec3(1.0), 0.62 + 0.25 * frost + 0.12 * lit) * (0.95 + 0.15 * lit);
    col = mix(col, rimC, sat(rim * (0.85 + 0.35 * frost)) * (1.0 - 0.75 * gloss));
    // seen a little from the front: the top face shows as a frosted band, rime grains on it
    float topFace = smoothstep(0.62, 0.82, n.y) * smoothstep(1.02, 0.9, abs(n.x));
    col = mix(col, mix(light, vec3(1.0), 0.7 + 0.2 * frost), topFace * 0.55 * (1.0 - 0.8 * gloss));
    float speck = smoothstep(0.66, 0.86, vnoise(q * 26.0 + seed * 5.0));
    col += vec3(1.0) * speck * sat(rim * 1.2) * (0.3 + 0.7 * max(nb.y, 0.0)) * 0.2 * (1.0 - gloss);
    col += vec3(0.9, 0.97, 1.0) * smoothstep(0.02, 0.0, e) * 0.2 * (1.0 - gloss);
    // a soft highlight at the top left, sharper and brighter as the cube melts into gloss
    vec2 hp = n - vec2(-0.42, 0.48);
    col += vec3(1.0) * exp(-dot(hp, hp) / mix(0.06, 0.02, gloss)) * mix(0.28, 0.45, gloss);
    col += vec3(0.9, 0.97, 1.0) * exp(-sq((n.x + n.y * 0.8 - 0.25) / 0.12)) * 0.05 * (1.0 - centre);
    // the drop: a cold light caught inside
    col += uGlow * uDrop * (0.05 + 0.12 * centre + 0.15 * ca);
    // a fresh lock frosts it white for a moment; game over: rime creeps over it
    col = mix(col, mix(tint, vec3(1.0), 0.75), (mode == 2 ? 0.0 : vC.z) * 0.4);
    col = mix(col, vec3(0.86, 0.91, 0.95) * (0.78 + 0.22 * frost), vC.w * 0.8);
    // melting: clear water, a bright meniscus along the film's top
    col = mix(col, mix(tint, vec3(1.0), 0.3) * 1.05, gloss * 0.45);
    col += vec3(1.0) * exp(-sq((n.y - 0.7) / 0.12)) * 0.22 * gloss * smoothstep(0.0, 0.03, e);
    // under the water: a deeper, more saturated teal
    float under = smoothstep(uLevel + 0.04, uLevel - 0.04, vWorld.y) * uWaterA;
    float l = dot(col, vec3(0.2126, 0.7152, 0.0722));
    vec3 sat2 = max(mix(vec3(l), col, 1.45), 0.0) * uDeepTint * 1.12;
    col = mix(col, sat2, under * 0.75);
    col = mix(col, uBridgeInk, uBridge);
    // translucent: the well's grid shows faintly through the clear middle
    float a = inside * vB.a * (0.86 + 0.12 * sat(rim + topFace) - 0.2 * gloss) * (1.0 - smoothstep(0.78, 1.0, u));
    frag = vec4(col * a, a);
}
)";

// ------------------------------------------------------------------------------------------------ particles
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size, mode
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // stretch, seed
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vLocal = aPos;
    vA = aA; vB = aB; vC = aC;
    vec2 off = aPos * vec2(aA.z, aA.z * aC.x);
    if (int(aA.w + 0.5) == 2) off.y += aA.z * aC.x; // a streak rises from its foot
    gl_Position = uVP * vec4(aA.xy + off, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uTime, uPxPerUnit;
void main() {
    int mode = int(vA.w + 0.5);
    vec2 q = vLocal;
    float r = length(q);
    vec3 c = vB.rgb;
    float v;
    if (mode == 1) {
        // a drip: a little glossy drop of water, a bright point
        float px = 2.0 / max(vA.z * uPxPerUnit, 1e-3);
        float d = length(q - vec2(0.0, -0.3)) - 0.5;
        float cone = max(abs(q.x) - 0.5 * (0.85 - q.y) / 1.15, q.y - 0.85);
        float sd = min(d, max(cone, -0.3 - q.y));
        float inside = 1.0 - smoothstep(-px, px, sd);
        float hi = exp(-dot(q - vec2(-0.18, -0.25), q - vec2(-0.18, -0.25)) * 25.0);
        float rim = exp(-abs(sd) / (px * 2.0)) * inside;
        v = inside;
        frag = vec4((c * 0.55 + rim * 0.35 + hi * 0.8) * v * vB.a, v * vB.a * 0.85);
        return;
    }
    if (mode == 2) {
        // a streak of cold light, rising from its foot and fading up
        float x = exp(-q.x * q.x * 5.0);
        float y = (1.0 - smoothstep(-1.0, 1.0, q.y)) * smoothstep(-1.0, -0.8, q.y);
        v = x * y;
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    if (mode == 0) {
        // a frost mote: a soft point with a faint twinkle
        float tw = 0.6 + 0.4 * sin(uTime * 7.0 + vC.y * 40.0);
        v = exp(-r * r * 10.0) + tw * 0.35 * (exp(-q.x * q.x * 300.0) + exp(-q.y * q.y * 300.0)) * exp(-r * 3.0);
        frag = vec4(c * v * vB.a, v * vB.a * 0.5);
        return;
    }
    if (mode == 4) {
        // a breath of mist
        v = exp(-r * r * 3.0) * (1.0 - smoothstep(0.7, 1.0, r));
        frag = vec4(c * v * vB.a, v * vB.a);
        return;
    }
    // a flake
    v = exp(-r * r * 6.0) * (1.0 - smoothstep(0.6, 1.0, r));
    frag = vec4(c * v * vB.a, v * vB.a * 0.7);
}
)";

// ------------------------------------------------------------------------------------------------ panels
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
// The water in front of everything: under its surface the scene looks submerged (deeper, tinted), the surface a thin
// bright line across the whole screen (faded behind the HUD's lines, which stay legible); then the tone, the fade.
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform vec2 uViewC, uViewH, uRes;
uniform float uFade, uDim, uWaterA, uSunK;
uniform vec3 uTint, uLine;
uniform vec4 uBox[24];
uniform int uBoxN;
void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    float px = 2.0 * uViewH.y / uRes.y;
    float s = surf(p.x);
    float depth = s - p.y;
    // just under the surface, the scene bends a little (a pixel or two)
    float bend = depth > 0.0 ? sin(p.x * 3.0 + uWaveT * 1.4 + p.y * 2.0) * exp(-depth * 1.5) * 0.6 * uWaterA : 0.0;
    vec3 c = texture(uScene, vUv + vec2(bend / uRes.x, 0.0)).rgb;
    float under = smoothstep(-px, px, depth) * uWaterA;
    c = mix(c, c * uTint, under * (0.3 + 0.15 * sat(depth * 0.15)));
    // the surface line, hidden behind the HUD's text
    vec2 fc = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    float hide = 0.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uBoxN) break;
        vec4 b = uBox[i];
        vec2 dd = max(max(b.xy - fc, fc - b.zw), 0.0);
        hide = max(hide, 1.0 - smoothstep(0.0, 14.0, length(dd)));
    }
    float line = exp(-sq(depth / (px * 1.0))) * 0.75 + exp(-abs(depth) / (px * 5.0)) * 0.12;
    line += step(0.0, depth) * exp(-depth / (px * 16.0)) * 0.06;
    c += uLine * line * (1.0 - 0.9 * hide) * uWaterA;
    // the cave a little darker at rest; the drop (its warm rays are the cave's): a modest lift on the lit ice and the
    // shadows behind deeper: light and shade, not a white-out
    float k = min(uSunK, 1.0);
    c *= 0.86;
    float l0 = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float lit = smoothstep(0.12, 0.5, l0);
    c *= mix(1.0 - 0.45 * k, 1.0 + 0.18 * uSunK, lit);
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.3 * (1.0 + 0.4 * k) * dot(q, q) * 2.0;
    // a soft shoulder on the luminance (the hues kept: lit ice stays blue), then a little on each channel
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float lt = lum / (1.0 + max(lum - 0.7, 0.0) * 0.8);
    c *= lt / max(lum, 1e-4);
    c = c / (1.0 + max(c - 0.9, 0.0) * 0.9);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace glacier_shaders

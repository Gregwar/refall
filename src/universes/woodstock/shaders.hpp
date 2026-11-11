#pragma once
// WOODSTOCK's shaders. Everything is flat cartoon in display (sRGB) colors, every shape a signed distance field
// with a thick white outline: the kaleidoscope behind everything, the sprites (flowers, mandalas, peace signs,
// hearts, rainbows, the kawaii sun and mushrooms, doves, lava blobs, petals, stems, the well's backing, the bonus
// flower), the blocks (a candy tile with a white outline and one bold motif per piece type), the lettering.

namespace woodstock_shaders {

// Shared painting helpers (prepended after the #version line).
inline const char* PAINT_GLSL = R"(
const float TAU = 6.2831853;
float AA = 0.01;
// Paints `col` over `acc` where d < 0 (antialiased); the first layer sets the color outright.
void layer(inout vec4 acc, float d, vec3 col) {
    float a = 1.0 - smoothstep(-AA, AA, d);
    acc.rgb = mix(acc.rgb, col, acc.a > 0.0 ? a : 1.0);
    acc.a = max(acc.a, a);
}
void layerA(inout vec4 acc, float d, vec3 col, float k) {
    float a = (1.0 - smoothstep(-AA, AA, d)) * k;
    acc.rgb = mix(acc.rgb, col, acc.a > 0.0 ? a : 1.0);
    acc.a = max(acc.a, a);
}
float dEll(vec2 p, vec2 r) { return (length(p / r) - 1.0) * min(r.x, r.y); }
float dSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float dot2(vec2 v) { return dot(v, v); }
// A heart about 2 wide, its point at the bottom.
float dHeart(vec2 q) {
    vec2 p = q * 0.62 + vec2(0.0, 0.58);
    p.x = abs(p.x);
    float d;
    if (p.y + p.x > 1.0) d = sqrt(dot2(p - vec2(0.25, 0.75))) - sqrt(2.0) / 4.0;
    else d = sqrt(min(dot2(p - vec2(0.0, 1.0)), dot2(p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
    return d / 0.62;
}
// The peace symbol: a ring and its three bars (radius 1), bar width w.
float dPeace(vec2 q, float w) {
    float ring = abs(length(q) - (1.0 - w * 0.5)) - w * 0.5;
    float r = 1.0 - w * 0.5;
    float bars = min(dSeg(q, vec2(0.0, -r), vec2(0.0, r)),
                     min(dSeg(q, vec2(0.0), vec2(-0.7071 * r, -0.7071 * r)), dSeg(q, vec2(0.0), vec2(0.7071 * r, -0.7071 * r))));
    return min(ring, bars - w * 0.5);
}
// A four-pointed sparkle (radius 1).
float dSparkle(vec2 q) {
    vec2 p = abs(q);
    float v = sqrt(p.x + 0.002) + sqrt(p.y + 0.002);
    return (v - 1.0) * 0.55;
}
// Petals around a center: the petal index (k) and the point in the petal's own frame (x along the petal).
vec2 petalFrame(vec2 q, float n, float turn, out float k) {
    float an = atan(q.y, q.x) - turn;
    float sec = TAU / n;
    k = floor(an / sec + 0.5);
    float la = an - k * sec;
    return length(q) * vec2(cos(la), sin(la));
}
// A kawaii face (s: its scale): glossy eyes, a smile, pink cheeks.
void face(inout vec4 acc, vec2 q, float s, vec3 ink, float blink) {
    vec2 e = vec2(abs(q.x) - 0.34 * s, q.y - 0.06 * s);
    float h = mix(0.17, 0.025, blink) * s;
    layer(acc, dEll(e, vec2(0.11 * s, h)), ink);
    if (blink < 0.5) layer(acc, length(e - vec2(-0.035, 0.06) * s) - 0.04 * s, vec3(1.0));
    vec2 c = vec2(0.0, -0.02 * s);
    float smile = max(abs(length(q - c) - 0.2 * s) - 0.04 * s, q.y - c.y + 0.1 * s);
    layer(acc, smile, ink);
    layerA(acc, dEll(vec2(abs(q.x) - 0.5 * s, q.y + 0.13 * s), vec2(0.11, 0.065) * s), vec3(1.0, 0.55, 0.68), 0.85);
}
vec3 hueRot(vec3 c, float a) {
    const mat3 toYIQ = mat3(0.299, 0.596, 0.211, 0.587, -0.274, -0.523, 0.114, -0.322, 0.312);
    const mat3 toRGB = mat3(1.0, 1.0, 1.0, 0.956, -0.272, -1.106, 0.621, -0.647, 1.703);
    vec3 y = toYIQ * c;
    float cs = cos(a), sn = sin(a);
    y.yz = mat2(cs, sn, -sn, cs) * y.yz;
    return clamp(toRGB * y, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- the kaleidoscope
inline const char* BG_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;            // center, half extents (board units)
uniform vec3 uBand[5];
uniform vec3 uLine, uDot;
uniform float uFlow, uBreath, uMorph, uMandala, uShift, uTime, uFolds, uCalm, uDesat, uDim, uWarp;
uniform vec4 uColL, uColR;     // the HUD columns (board units), veiled a little
uniform vec3 uVeil;
vec3 band(float i) {
    float k = mod(i, 5.0);
    return k < 1.0 ? uBand[0] : k < 2.0 ? uBand[1] : k < 3.0 ? uBand[2] : k < 4.0 ? uBand[3] : uBand[4];
}
float boxD(vec2 p, vec4 b) {
    vec2 c = (b.xy + b.zw) * 0.5, h = (b.zw - b.xy) * 0.5;
    return dBox(p - c, h, 0.8);
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 d = p - vec2(0.0, -0.5);
    float rc = length(d);
    vec2 a4 = abs(d);
    float rs = pow(pow(a4.x, 4.0) + pow(a4.y * 1.12, 4.0), 0.25);
    float r = max(mix(rc, rs, uMorph), 0.05);
    float an = atan(d.y, d.x) + 0.025 * uTime;
    float sec = TAU / uFolds;
    float si = floor(an / sec);
    float la = abs(an - (si + 0.5) * sec) / (sec * 0.5);   // 0 in a sector's middle, 1 at its edges
    const float K = 7.0;
    float s = log(r) * K - uFlow;
    // the bands scallop into petals as the music breathes; the drop folds them into a mandala
    s += uBreath * 0.42 * (1.0 - la * la);
    s += uMandala * 1.1 * sqrt(max(0.0, 1.0 - la * la)) - uMandala * 0.5;
    s += uWarp * 0.16 * sin(an * 3.0 + uTime * 0.21 + log(r) * 2.0);
    float i = floor(s), f = fract(s);
    float checker = 0.0;
    float sh = uShift + checker * 2.0;
    float fl = floor(sh);
    vec3 col = mix(band(i + fl), band(i + fl + 1.0), fract(sh));
    // white lines between the bands, a constant width on screen
    float bw = r / K;                                         // a band's width in board units
    float lineW = 0.075;
    float dl = min(f, 1.0 - f) * bw;
    float aa = fwidth(s) * bw * 0.8 + 0.004;
    col = mix(col, uLine, 1.0 - smoothstep(lineW - aa, lineW + aa, dl));
    // op-art dots in every other band, one per sector (sparkles in the drop's mandala)
    if (mod(i, 2.0) > 0.5 && bw > 0.5) {
        vec2 q = vec2(la * sec * 0.5 * r, (f - 0.5) * bw);
        float rr = min(0.26 * bw, 0.55);
        float kk;
        vec2 pq = petalFrame(q, 5.0, 1.5708 + uTime * 0.3, kk);
        float daisy = min(dEll(pq - vec2(0.55 * rr, 0.0), vec2(0.45, 0.33) * rr), length(q) - 0.3 * rr);
        float dd = mix(daisy, dSparkle(q / (rr * 1.25)) * rr * 1.25, uMandala);
        float a0 = 1.0 - smoothstep(-aa, aa, dd);
        col = mix(col, mix(uLine, uDot, uMandala), a0);
        float c0 = 1.0 - smoothstep(-aa, aa, length(q) - 0.26 * rr);
        col = mix(col, uDot, c0 * (1.0 - uMandala));
    }
    // calm: the colors softer; slow motion: dusk
    col = mix(col, mix(col, vec3(dot(col, vec3(0.3333))), 0.5) * 0.92 + 0.08, uCalm);
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.62, 0.66, 0.92), uDesat);
    // a soft veil behind the HUD columns
    float v = max(1.0 - smoothstep(-0.6, 1.4, boxD(p, uColL)), 1.0 - smoothstep(-0.6, 1.4, boxD(p, uColR)));
    col = mix(col, uVeil, v * 0.0);
    frag = vec4(col * uDim, 1.0);
}
)";

// ---------------------------------------------------------------- sprites
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;  // x, y, half width, half height (board units)
layout(location = 2) in vec4 iB;  // rotation, kind, alpha, p1
layout(location = 3) in vec4 iC;  // color A, p2
layout(location = 4) in vec4 iD;  // color B, p3
layout(location = 5) in vec4 iE;  // color C, outline width (board units)
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC, vD, vE;
void main() {
    vec2 ext = iA.zw * (iB.y > 12.5 ? 1.0 : 1.12) + iE.w * 1.6 + 0.06;
    vec2 l = aCorner * ext;
    float c = cos(iB.x), s = sin(iB.x);
    vL = l;
    vA = iA; vB = iB; vC = iC; vD = iD; vE = iE;
    gl_Position = uVP * vec4(iA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y), 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(
in vec2 vL;
flat in vec4 vA, vB, vC, vD, vE;
out vec4 frag;
uniform vec3 uInk, uWhite;
uniform float uTime, uFade;
void main() {
    int kind = int(vB.y + 0.5);
    float hy = vA.w;
    vec2 q = vL / hy;
    AA = max(fwidth(q.x), 1e-4) * 0.7;
    float W = min(vE.w / hy, 0.22);
    vec3 A = vC.rgb, B = vD.rgb, C = vE.rgb;
    float p1 = vB.w, p2 = vC.w, p3 = vD.w;
    vec4 acc = vec4(0.0);
    vec3 WH = uWhite;
    if (kind == 0 || kind == 11) {
        // a daisy (p1 petals, p2 > 0.5: a face, p3: how open); the bonus flower (p1: its fill, p2: ready)
        float n = kind == 11 ? 12.0 : p1;
        float open = kind == 11 ? 1.0 : p3;
        float k;
        vec2 pp = petalFrame(q, n, kind == 11 ? 1.5708 : 0.0, k);
        float sec = TAU / n;
        float pc = 0.6 * open + 0.05;
        float dP = dEll(pp - vec2(pc, 0.0), vec2(0.4 * open + 0.02, min(0.42, pc * tan(sec * 0.5) * 1.05) * open + 0.02));
        float dC = length(q) - 0.36;
        layer(acc, min(dP, dC) - W, WH);
        vec3 pcol;
        if (kind == 11) {
            float idx = mod(-k, n);
            pcol = idx + 0.5 < p1 * n ? A : B;
        } else pcol = mod(k, 2.0) < 0.5 ? A : B;
        layer(acc, dP, pcol);
        layer(acc, dEll(pp - vec2(pc * 0.92, 0.0), vec2(0.2, 0.08) * open), mix(pcol, vec3(1.0), 0.42));
        layer(acc, dC, WH);
        layer(acc, dC + W * 0.8, C);
        layerA(acc, abs(length(q) - 0.2) - 0.035, C * 0.82, 1.0);
        if ((kind == 0 && p2 > 0.5) || (kind == 11 && p2 > 0.5)) {
            layer(acc, length(q) - 0.33, C);
            face(acc, q, 0.36, uInk, 0.0);
        }
    } else if (kind == 1) {
        // a mandala flower: pointed outer petals, round inner petals turned half a petal, rings of dots
        float n = p1, k, k2;
        vec2 o = petalFrame(q, n, p2, k);
        float sec = TAU / n;
        float dO = max(dEll(o - vec2(0.62, 0.0), vec2(0.38, 0.62 * tan(sec * 0.5))), abs(o.y) - (1.02 - o.x) * 0.9);
        vec2 m = petalFrame(q, n, p2 * -0.6 + sec * 0.5, k2);
        float dM = dEll(m - vec2(0.45, 0.0), vec2(0.24, 0.45 * tan(sec * 0.5) * 0.95));
        float dC = length(q) - 0.3;
        layer(acc, min(min(dO, dM), dC) - W, WH);
        layer(acc, dO, mod(k, 2.0) < 0.5 ? A : mix(A, B, 0.35));
        layer(acc, dEll(o - vec2(0.66, 0.0), vec2(0.18, 0.06)), mix(A, vec3(1.0), 0.5));
        layer(acc, dM, WH);
        layer(acc, dM + W * 0.7, B);
        layer(acc, dC, WH);
        layer(acc, dC + W * 0.7, C);
        float kd;
        vec2 dp = petalFrame(q, n, p2 * 0.4, kd);
        layer(acc, length(dp - vec2(0.2, 0.0)) - 0.045, WH);
        layer(acc, length(q) - 0.09, A);
    } else if (kind == 2) {
        // a peace sign
        float d = dPeace(q, 0.24);
        layer(acc, d - W, WH);
        layer(acc, d, A);
        layerA(acc, max(d + 0.07, -(q.x + q.y) * 0.7 - 0.2), mix(A, vec3(1.0), 0.4), 0.8);
    } else if (kind == 3) {
        // a heart
        float d = dHeart(q);
        layer(acc, d - W, WH);
        layer(acc, d, A);
        layer(acc, dEll(q - vec2(-0.4, 0.3), vec2(0.16, 0.1)), mix(A, vec3(1.0), 0.6));
    } else if (kind == 4) {
        // a rainbow and its two clouds
        vec2 c = vec2(0.0, -0.45);
        float r = length(q - c);
        float d = max(abs(r - 0.72) - 0.28, c.y - q.y);
        layer(acc, d - W, WH);
        float bi = clamp(floor((1.0 - r) / (0.56 / 6.0)), 0.0, 5.0);
        vec3 bc = bi < 1.0 ? vec3(0.96, 0.29, 0.43) : bi < 2.0 ? vec3(0.99, 0.6, 0.24) : bi < 3.0 ? vec3(0.99, 0.85, 0.3)
                : bi < 4.0 ? vec3(0.36, 0.8, 0.55) : bi < 5.0 ? vec3(0.29, 0.66, 0.92) : vec3(0.62, 0.45, 0.9);
        layer(acc, d, bc);
        float fb = fract((1.0 - r) / (0.56 / 6.0));
        layerA(acc, max(d, (min(fb, 1.0 - fb) * 0.56 / 6.0) - W * 0.25), WH, step(0.48, r) * step(r, 0.98));
        for (int s = -1; s <= 1; s += 2) {
            vec2 cq = vec2(q.x * float(s), q.y);
            float cl = min(min(length(cq - vec2(0.72, -0.47)) - 0.22, length(cq - vec2(0.97, -0.55)) - 0.15),
                           min(length(cq - vec2(0.48, -0.55)) - 0.15, dBox(cq - vec2(0.72, -0.62), vec2(0.38, 0.08), 0.08)));
            layer(acc, cl - W, A);
            layer(acc, cl, WH);
        }
    } else if (kind == 5) {
        // the kawaii sun: wavy rays turning slowly, a face
        float n = 12.0, k;
        vec2 pp = petalFrame(q, n, p2, k);
        float wob = 0.06 * sin(length(q) * 9.0 - uTime * 2.0 + k);
        float dR = max(abs(pp.y + wob * (pp.x - 0.5)) - (1.0 - pp.x) * 0.36, pp.x - 1.0);
        float dD = length(q) - 0.62;
        layer(acc, min(dR, dD) - W, WH);
        layer(acc, dR, mod(k, 2.0) < 0.5 ? B : C);
        layer(acc, dD, WH);
        layer(acc, dD + W * 0.8, A);
        layer(acc, dEll(q - vec2(-0.25, 0.3), vec2(0.14, 0.08)), mix(A, vec3(1.0), 0.55));
        face(acc, q + vec2(0.0, 0.04), 0.62, uInk, step(0.97, fract(uTime * 0.23 + p3)));
    } else if (kind == 6) {
        // a mushroom with a face
        float cap = max(dEll(q - vec2(0.0, -0.05), vec2(1.0, 0.85)), -(q.y + 0.05));
        cap = min(cap, dBox(q - vec2(0.0, -0.08), vec2(0.98, 0.1), 0.1));
        float stem = dBox(q - vec2(0.0, -0.55), vec2(0.36, 0.42), 0.2);
        layer(acc, min(cap, stem) - W, WH);
        layer(acc, stem, B);
        face(acc, q - vec2(0.0, -0.52), 0.42, uInk, step(0.96, fract(uTime * 0.19 + p3)));
        layer(acc, cap + W * 0.0, A);
        float dots = min(min(length(q - vec2(-0.5, 0.28)) - 0.16, length(q - vec2(0.12, 0.52)) - 0.13),
                         min(length(q - vec2(0.58, 0.22)) - 0.14, length(q - vec2(-0.08, 0.15)) - 0.09));
        layer(acc, max(dots, cap), WH);
    } else if (kind == 7) {
        // a dove (white, lilac outline): p2 the wing's beat
        vec3 O = A;
        if (p3 < 0.0) q.x = -q.x;
        float body = dEll(q - vec2(-0.05, -0.1), vec2(0.62, 0.3));
        float head = length(q - vec2(0.52, 0.12)) - 0.24;
        float tail = max(dSeg(q, vec2(-0.55, -0.1), vec2(-0.95, 0.05)) - 0.16, -q.x - 1.0);
        float fl = sin(p2);
        vec2 wq = q - vec2(-0.05, 0.12);
        float wa = 0.5 + 0.5 * fl;
        wq = mat2(cos(wa), -sin(wa), sin(wa), cos(wa)) * wq;
        float wing = dEll(wq - vec2(-0.05, 0.32), vec2(0.28, 0.5));
        float beak = max(dSeg(q, vec2(0.7, 0.12), vec2(0.92, 0.08)) - 0.07, 0.0);
        float sil = min(min(body, head), min(tail, wing));
        layer(acc, min(sil, beak) - W, O);
        layer(acc, beak, vec3(1.0, 0.68, 0.25));
        layer(acc, sil, WH);
        layer(acc, wing + W * 0.0, WH);
        layer(acc, abs(wing) - W * 0.35, O);
        layer(acc, length(q - vec2(0.6, 0.18)) - 0.05, uInk);
    } else if (kind == 8) {
        // a lava-lamp blob
        float an = atan(q.y, q.x);
        float rr = 0.82 + 0.1 * sin(3.0 * an + uTime * 0.9 + p3 * 6.0) + 0.06 * sin(5.0 * an - uTime * 1.3 + p3 * 3.0);
        float d = (length(q) - rr) * 0.9;
        layer(acc, d - W, WH);
        layer(acc, d, A);
        layer(acc, dEll(q - vec2(-0.3, 0.32), vec2(0.2, 0.12)), mix(A, vec3(1.0), 0.6));
    } else if (kind == 9) {
        // a petal
        float d = dEll(q, vec2(1.0, 0.5));
        layer(acc, d - W, WH);
        layer(acc, d, A);
        layer(acc, dEll(q - vec2(0.15, 0.0), vec2(0.5, 0.14)), mix(A, vec3(1.0), 0.45));
    } else if (kind == 10) {
        // a sparkle
        float d = dSparkle(q);
        layer(acc, d - W, WH);
        layer(acc, d, A);
    } else if (kind == 12) {
        // a stem with two leaves (board units, p2 its bend)
        vec2 l = vL;
        float h = vA.w;
        float t = clamp((l.y + h) / (2.0 * h), 0.0, 1.0);
        float bx = p2 * t * t;
        float stem = max(abs(l.x - bx) - 0.075, abs(l.y) - h);
        float ly1 = -h * 0.35, ly2 = h * 0.05;
        vec2 a1 = l - vec2(p2 * 0.1 - 0.42, ly1);
        a1 = mat2(0.86, 0.5, -0.5, 0.86) * a1;
        vec2 a2 = l - vec2(p2 * 0.3 + 0.42, ly2);
        a2 = mat2(0.86, -0.5, 0.5, 0.86) * a2;
        float lf = min(dEll(a1, vec2(0.44, 0.17) * p1), dEll(a2, vec2(0.4, 0.16) * p1));
        AA = max(fwidth(l.x), 1e-4) * 0.7;
        layer(acc, min(stem, lf) - vE.w, WH);
        layer(acc, stem, A);
        layer(acc, lf, B);
        layer(acc, max(lf, min(abs(a1.y), abs(a2.y)) - 0.02), mix(B, vec3(1.0), 0.3));
    } else if (kind == 13) {
        // the well's backing (board units): a calm dark panel, a white cartoon outline, an ink rim
        vec2 l = vL;
        AA = max(fwidth(l.x), 1e-4) * 0.7;
        float d = dBox(l, vA.zw, 0.45);
        layer(acc, d - vE.w - 0.09, uInk);
        layer(acc, d - vE.w, WH);
        layer(acc, d, mix(B, A, clamp(l.y / vA.w * 0.5 + 0.5, 0.0, 1.0)));
        // a faint inner glow ring
        layerA(acc, abs(d + 0.35) - 0.03, mix(A, C, 0.5), 0.35);
    }
    float al = acc.a * vB.z * uFade;
    frag = vec4(acc.rgb * al, al);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // type, alpha, mode (0 block, 1 ghost), scale
layout(location = 3) in vec4 iC; // color, darkening
uniform mat4 uVP;
out vec2 vU;
flat out vec4 vB, vC;
void main() {
    vec2 l = aCorner * 0.56;           // the cell spans -0.5..0.5
    float c = cos(iA.w), s = sin(iA.w);
    vec2 w = vec2(c * l.x - s * l.y, s * l.x + c * l.y) * iA.z * 2.0 * iB.w;
    vU = l;
    vB = iB; vC = iC;
    gl_Position = uVP * vec4(iA.xy + w, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(
in vec2 vU;
flat in vec4 vB, vC;
out vec4 frag;
uniform vec3 uInk, uWhite;
uniform float uFade;
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.75;
    int type = int(vB.x + 0.5);
    vec3 col = vC.rgb;
    float d = dBox(u, vec2(0.4), 0.15);
    vec4 acc = vec4(0.0);
    if (vB.z > 0.5) {
        // the ghost: the white outline alone, dashed
        float dash = step(0.5, fract((u.x + u.y) * 4.0 + 0.25));
        layerA(acc, abs(d - 0.03) - 0.028, uWhite, 0.4 + 0.3 * dash);
        frag = vec4(acc.rgb * acc.a * vB.y * uFade, acc.a * vB.y * uFade);
        return;
    }
    layer(acc, d - 0.095, uInk);
    layer(acc, d - 0.06, uWhite);
    // the candy fill: lighter toward the top left, a faint tie-dye swirl
    float r = length(u), an = atan(u.y, u.x);
    vec3 fill = mix(col * 0.86, mix(col, vec3(1.0), 0.16), clamp(0.5 + (u.y - u.x) * 0.9, 0.0, 1.0));
    fill *= 1.0 + 0.055 * sin(r * 26.0 - an * 2.0);
    layer(acc, d, fill);
    // the motif: one bold pale shape per type, over a soft shadow of the hue
    vec3 M = mix(col, vec3(1.0), 0.86);
    vec3 S = col * 0.62;
    float sc = 0.27;
    vec2 q = u / sc;
    vec2 qs = (u - vec2(0.022, -0.026)) / sc;
    float dm, ds;
    if (type == 0) {           // I: a daisy
        float k;
        vec2 pp = petalFrame(q, 5.0, 1.5708, k);
        vec2 ps = petalFrame(qs, 5.0, 1.5708, k);
        dm = min(dEll(pp - vec2(0.6, 0.0), vec2(0.42, 0.34)), length(q) - 0.36);
        ds = min(dEll(ps - vec2(0.6, 0.0), vec2(0.42, 0.34)), length(qs) - 0.36);
        layerA(acc, ds * sc, S, 0.7);
        layer(acc, dm * sc, M);
        layer(acc, (length(q) - 0.3) * sc, vec3(1.0, 0.78, 0.2));
    } else if (type == 1) {    // O: a heart
        layerA(acc, dHeart(qs * 1.08) * sc, S, 0.7);
        layer(acc, dHeart(q * 1.08) * sc, M);
    } else if (type == 2) {    // T: a peace sign
        layerA(acc, dPeace(qs * 0.98, 0.26) * sc, S, 0.7);
        layer(acc, dPeace(q * 0.98, 0.26) * sc, M);
    } else if (type == 3) {    // S: a sparkle
        layerA(acc, dSparkle(qs * 0.92) * sc, S, 0.7);
        layer(acc, dSparkle(q * 0.92) * sc, M);
    } else if (type == 4) {    // Z: a tie-dye spiral
        float pitch = 0.46;
        float rr = length(q), aa = atan(q.y, q.x) / TAU;
        float sp = abs(fract(rr / pitch - aa + 0.5) - 0.5) * pitch - 0.12;
        sp = max(sp, rr - 1.0);
        sp = min(sp, rr - 0.16);
        float rs = length(qs), as = atan(qs.y, qs.x) / TAU;
        float sps = min(max(abs(fract(rs / pitch - as + 0.5) - 0.5) * pitch - 0.12, rs - 1.0), rs - 0.16);
        layerA(acc, sps * sc, S, 0.7);
        layer(acc, sp * sc, M);
    } else if (type == 5) {    // J: a rainbow
        vec2 c = vec2(0.0, -0.4);
        float r0 = length(q - c), r1 = length(qs - c);
        float arc = 1e3, arcs = 1e3;
        for (int i = 0; i < 3; i++) {
            float R = 0.38 + 0.32 * float(i);
            arc = min(arc, abs(r0 - R) - 0.11);
            arcs = min(arcs, abs(r1 - R) - 0.11);
        }
        arc = max(arc, c.y - 0.05 - q.y);
        arcs = max(arcs, c.y - 0.05 - qs.y);
        layerA(acc, arcs * sc, S, 0.7);
        layer(acc, arc * sc, M);
    } else {                   // L: a kawaii face
        vec2 e = vec2(abs(q.x) - 0.42, q.y - 0.12);
        layer(acc, dEll(e, vec2(0.15, 0.22)) * sc, col * 0.25);
        layer(acc, (length(e - vec2(-0.05, 0.08)) - 0.06) * sc, vec3(1.0));
        vec2 c = vec2(0.0, -0.05);
        float sm = max(abs(length(q - c) - 0.28) - 0.07, q.y - c.y + 0.12);
        layer(acc, sm * sc, col * 0.25);
        layerA(acc, dEll(vec2(abs(q.x) - 0.66, q.y + 0.2), vec2(0.15, 0.09)) * sc, vec3(1.0, 0.6, 0.72), 0.9);
    }
    // a lock's brief brightening, the game over's fading
    acc.rgb = mix(acc.rgb, vec3(1.0), vC.w > 0.0 ? 0.0 : -vC.w);
    if (vC.w > 0.0) {
        float l = dot(acc.rgb, vec3(0.299, 0.587, 0.114));
        acc.rgb = mix(acc.rgb, vec3(l) * 0.55, vC.w);
    }
    float al = acc.a * vB.y * uFade;
    frag = vec4(acc.rgb * al, al);
}
)";

// ---------------------------------------------------------------- lettering
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
// SDF letters: the fill, a thick dark cartoon stroke.
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uFill, uStroke;
uniform float uAlpha, uStrokeW;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.7;
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float edge = 0.5 - uStrokeW;
    float stroke = smoothstep(edge - w, edge + w, d);
    vec3 c = mix(uStroke, uFill, fill);
    frag = vec4(c * stroke, stroke) * uAlpha;
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace woodstock_shaders

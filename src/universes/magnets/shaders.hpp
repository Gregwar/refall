#pragma once
// MAGNETS' shaders: the filings (a GPU simulation in two state textures and their instanced strokes), the magnets
// (instanced cells merged into one shape per piece), a few vector shapes (the well, the compass), the paper, the
// lettering and the final pass (fade, dim, bridge).

namespace magnets_shaders {

inline const char* HASH_GLSL = R"(
float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
)";

// ---------------------------------------------------------------- the field and the filings' physics
// State A: position (board units), angle (the filing's north end), angular velocity.
// State B: velocity (impulses: clears, locks, taps), field strength, how ordered the filing is (0..1).
// Each filing turns toward the local field with a damped torque (smooth, no jitter), creeps slowly toward stronger
// field (never into a magnet), and is thrown about by impulses (rows cleared, pieces landing, the paper tapped).
inline const char* SIM_FS = R"(#version 330 core
layout(location = 0) out vec4 oA;
layout(location = 1) out vec4 oB;
uniform sampler2D uA, uB, uQ;
uniform int uNS, uNP;       // static charges [0, uNS), the falling piece's [uNS, uNS + uNP)
uniform float uDt, uThr, uMig, uK, uSeed, uTap, uSpin;
uniform vec4 uBounds;       // x0 y0 x1 y1 (board units)
uniform int uOcc[22];       // the stack's cells, a bit per column
uniform int uNImp;
uniform vec4 uImp[8];       // impulse rectangles x0 y0 x1 y1
uniform vec4 uImpK[8];      // speed, spin, reach, upward bias
uniform int uNCarry;
uniform vec4 uCarry[4];     // rows fallen after a clear: filings above y (x) slide down by (y) rows with the stack

vec2 field(vec2 p, int a, int b) {
    vec2 B = vec2(0.0);
    for (int i = a; i < b; i++) {
        vec4 c = texelFetch(uQ, ivec2(i, 0), 0);
        vec2 d = p - c.xy;
        B += c.z * d / (dot(d, d) + 0.012);
    }
    return B;
}
bool occ(vec2 p) {
    int col = int(floor(p.x + 5.0)), gy = int(floor(12.0 - p.y));
    if (col < 0 || col > 9 || gy < 0 || gy > 21) return false;
    return ((uOcc[gy] >> col) & 1) != 0;
}
float wrapA(float a) { return a - 6.2831853 * floor((a + 3.1415927) / 6.2831853); }

void main() {
    ivec2 ij = ivec2(gl_FragCoord.xy);
    vec4 A = texelFetch(uA, ij, 0), S = texelFetch(uB, ij, 0);
    vec2 p = A.xy, v = S.xy;
    float th = A.z, om = A.w;
    float id = float(ij.y * 4096 + ij.x);
    float h1 = hash11(id * 0.618 + 0.3), h2 = hash11(id * 1.37 + 7.1), h3 = hash11(id * 2.11 + uSeed);

    vec2 Bs = field(p, 0, uNS);
    vec2 B = Bs + field(p, uNS, uNS + uNP);
    float m = length(B);
    float gate = smoothstep(uThr * 0.45, uThr, m);
    bool pinned = occ(p);

    // impulses (one frame): away from the middle of their rectangle, faster inside it
    for (int i = 0; i < uNImp; i++) {
        vec4 r = uImp[i], k = uImpK[i];
        vec2 q = clamp(p, r.xy, r.zw);
        float dist = length(p - q);
        if (dist > k.z) continue;
        float w = 1.0 - dist / max(k.z, 1e-3);
        vec2 c = 0.5 * (r.xy + r.zw);
        vec2 dir = (p - c) / max(0.5 * (r.zw - r.xy), vec2(0.3)) + (vec2(h1, h2) - 0.5) * 1.6;
        dir.y += k.w;
        float a = 6.2831853 * hash11(id * 0.77 + float(i) * 3.1 + uSeed);
        dir = normalize(dir + 0.6 * vec2(cos(a), sin(a)));
        v += dir * k.x * w * (0.45 + 0.9 * h3);
        om += (h2 - 0.5) * 2.0 * k.y * w;
    }
    // the filings clinging to the stack slide down with the rows that fall after a clear
    for (int i = 0; i < uNCarry; i++)
        if (p.y > uCarry[i].x && abs(p.x) < 5.7) v.y -= uCarry[i].y * 6.5;
    // the paper tapped: a small hop and a twist, more where the field holds the filings in lines
    if (uTap > 0.0) {
        float a = 6.2831853 * h3;
        v += vec2(cos(a), sin(a)) * uTap * (0.25 + 0.75 * gate) * (0.4 + 0.6 * h1);
        om += (h2 - 0.5) * uTap * 14.0;
    }
    // the polarity turned over: every filing spins the same way at once (the ones in the field the most)
    if (uSpin != 0.0) om += uSpin * (0.75 + 0.5 * h1) * (0.35 + 0.65 * gate);

    // turning toward the field: a damped torque (half steps keep the stiff ones stable)
    float k = uK * gate * clamp(m / uThr, 0.0, 3.0);
    float damp = 2.0 * sqrt(k) * 0.9 + 3.5;
    float tgt = atan(B.y, B.x);
    for (int s = 0; s < 3; s++) {
        float h = uDt / 3.0;
        float d = wrapA(tgt - th);
        om += (k * sin(d) - damp * om) * h;
        om = clamp(om, -40.0, 40.0);
        th += om * h;
    }
    th = wrapA(th);

    // creeping toward stronger field (static magnets only), gated by friction, never into a magnet
    vec2 vm = vec2(0.0);
    float ms = length(Bs);
    if (uMig > 0.0 && !pinned && ms > uThr * 0.6) {
        float e = 0.06;
        float l0 = log(dot(Bs, Bs) + 0.01);
        float lx = log(dot(field(p + vec2(e, 0.0), 0, uNS), field(p + vec2(e, 0.0), 0, uNS)) + 0.01);
        float ly = log(dot(field(p + vec2(0.0, e), 0, uNS), field(p + vec2(0.0, e), 0, uNS)) + 0.01);
        vec2 g = vec2(lx - l0, ly - l0) / e;
        float hold = 1.0 - smoothstep(1.4, 3.2, ms); // caught already: stays
        vm = uMig * smoothstep(uThr * 0.6, uThr * 1.6, ms) * hold * g / (1.0 + length(g)) * (0.5 + h1);
    }
    vec2 np = p + (v + vm) * uDt;
    if (!pinned && occ(np) && dot(v, v) < 0.25) np = p; // a magnet's edge stops a creeping filing
    v *= exp(-6.5 * uDt);
    if (np.x < uBounds.x || np.x > uBounds.z) { v.x = -v.x; np.x = clamp(np.x, uBounds.x, uBounds.z); }
    if (np.y < uBounds.y || np.y > uBounds.w) { v.y = -v.y; np.y = clamp(np.y, uBounds.y, uBounds.w); }

    oA = vec4(np, th, om);
    oB = vec4(v, m, gate);
}
)";

// ---------------------------------------------------------------- the filings drawn
// One instanced quad each: a short needle along its angle, a little heavier at its north end. Longer where the
// field is strong (filings chain up into bristles along the lines). Shown by density (a hash threshold, fading).
inline const char* FILING_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
uniform sampler2D uA, uB;
uniform int uW;
uniform mat4 uVP;
uniform float uDensity, uLen, uChain, uThr, uPx, uWidth, uFarA;
uniform vec4 uColL, uColR; // the HUD columns (board x0 y0 x1 y1): quieter there
uniform int uNQuiet;
uniform vec4 uQuiet[32];   // the HUD's lines (board x0 y0 x1 y1): no filings over the lettering
out vec2 vLocal;
out float vHalf, vW, vAlpha, vTone;
float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float inRect(vec2 p, vec4 r, float soft) {
    vec2 a = smoothstep(r.xy - soft, r.xy + soft, p) * (1.0 - smoothstep(r.zw - soft, r.zw + soft, p));
    return a.x * a.y;
}
void main() {
    int id = gl_InstanceID;
    ivec2 ij = ivec2(id % uW, id / uW);
    vec4 A = texelFetch(uA, ij, 0), S = texelFetch(uB, ij, 0);
    float fid = float(ij.y * 4096 + ij.x);
    float h = hash11(fid * 0.618 + 0.3), h4 = hash11(fid * 3.17 + 1.9), h5 = hash11(fid * 5.3 + 2.7);
    float quiet = 1.0 - 0.45 * max(inRect(A.xy, uColL, 0.8), inRect(A.xy, uColR, 0.8));
    for (int i = 0; i < uNQuiet; i++) quiet *= 1.0 - 0.94 * inRect(A.xy, uQuiet[i], 0.25);
    float vis = clamp((uDensity * quiet - h) / 0.07, 0.0, 1.0);
    if (vis <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float chain = smoothstep(uThr, uThr * 5.0, S.z);
    float len = uLen * (0.55 + 0.9 * h4) * (1.0 + uChain * chain);
    float hw = uWidth * uPx * (0.8 + 0.4 * h5) * 0.5;
    vec2 d = vec2(cos(A.z), sin(A.z)), n = vec2(-d.y, d.x);
    float pad = uPx * 1.5;
    vLocal = aCorner * vec2(len * 0.5 + pad + hw, hw * 1.4 + pad);
    vHalf = len * 0.5;
    vW = hw;
    // filings the field holds in lines stand out; the ones scattered at random stay faint
    // (three tones: scattered, lined up in a weak field, held hard near the poles)
    float order = smoothstep(0.2, 1.0, S.z);
    vAlpha = vis * (0.7 + 0.3 * h5) * mix(mix(uFarA, uFarA * 1.6, S.w), 1.0, order);
    vTone = h4;
    vec2 w = A.xy + d * vLocal.x + n * vLocal.y;
    gl_Position = uVP * vec4(w, 0.0, 1.0);
}
)";
inline const char* FILING_FS = R"(#version 330 core
in vec2 vLocal;
in float vHalf, vW, vAlpha, vTone;
out vec4 frag;
uniform vec3 uInk;
uniform float uInkA, uPx;
void main() {
    float t = clamp(vLocal.x / max(vHalf, 1e-4), -1.0, 1.0);
    float w = vW * (1.0 + 0.4 * t);
    vec2 q = vec2(vLocal.x - t * vHalf, vLocal.y);
    float d = length(q) - w;
    // thin needles stay visible: coverage of a sub-pixel width is spread over a pixel, lighter
    float cov = 1.0 - smoothstep(-0.5 * uPx, 0.5 * uPx, d);
    cov *= min(1.0, w / (0.5 * uPx) + 0.15);
    float a = cov * vAlpha * uInkA * (0.82 + 0.18 * vTone);
    if (a <= 0.003) discard;
    frag = vec4(uInk, a);
}
)";

// ---------------------------------------------------------------- the magnets
// A cell: a box whose sides touching the same piece reach into the neighbour (no junction inside a piece), its
// free sides inset with rounded convex corners. Two halves split across the piece's axis through its center (north
// and south). Modes: 0 the magnet, 1 its soft shadow on the paper, 2 an outline (the ghost).
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA; // center xy, half size, neighbour mask
layout(location = 2) in vec4 aN; // north color, alpha
layout(location = 3) in vec4 aS; // south color, lift (0..1)
layout(location = 4) in vec4 aM; // piece center xy, axis xy (times the polarity, -1..1)
uniform mat4 uVP;
uniform int uMode;
uniform float uPx;
out vec2 vP;
flat out vec4 vA, vN, vS, vM;
out vec2 vW;
void main() {
    int mask = int(aA.w + 0.5);
    float h = aA.z;
    float pad = uMode == 1 ? h * 0.45 : uPx * 2.0;
    // sides that touch the piece stop exactly at the cell's edge (no overlap, no seam)
    float l = (mask & 1) != 0 ? h : h + pad, r = (mask & 2) != 0 ? h : h + pad;
    float t = (mask & 8) != 0 ? h : h + pad, b = (mask & 4) != 0 ? h : h + pad;
    if (uMode == 1) { l = h + pad; r = h + pad; t = h + pad; b = h + pad; }
    vec2 lp = vec2(aCorner.x < 0.0 ? -l : r, aCorner.y < 0.0 ? -b : t);
    vP = lp;
    vec2 off = uMode == 1 ? vec2(0.05, -0.08) * aA.z * 2.0 * (1.0 + 1.5 * aS.w) : vec2(0.0);
    vW = aA.xy + lp + off;
    vA = vec4(aA.xy, h, aA.w);
    vN = aN; vS = aS; vM = aM;
    gl_Position = uVP * vec4(vW, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vP;
flat in vec4 vA, vN, vS, vM;
in vec2 vW;
out vec4 frag;
uniform int uMode;
uniform float uPx, uShadowA, uOutlineW;
uniform vec3 uShadow;
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main() {
    int mask = int(vA.w + 0.5);
    float h = vA.z;
    float gap = h * 0.075, rad = h * 0.2;
    bool L = (mask & 1) != 0, R = (mask & 2) != 0, D = (mask & 4) != 0, T = (mask & 8) != 0;
    float x0 = L ? -h - 1.0 : -h + gap, x1 = R ? h + 1.0 : h - gap;
    float y0 = D ? -h - 1.0 : -h + gap, y1 = T ? h + 1.0 : h - gap;
    vec2 c = vec2(0.5 * (x0 + x1), 0.5 * (y0 + y1)), b = vec2(0.5 * (x1 - x0), 0.5 * (y1 - y0));
    vec2 q = vP - c;
    bool freeX = q.x > 0.0 ? !R : !L, freeY = q.y > 0.0 ? !T : !D;
    float r = (freeX && freeY) ? rad : 0.0;
    vec2 e = abs(q) - b + r;
    float d = min(max(e.x, e.y), 0.0) + length(max(e, 0.0)) - r;
    if (uMode == 1) {
        float blur = h * (0.28 + 0.4 * vS.w);
        float a = (1.0 - smoothstep(-blur, blur, d)) * uShadowA * vN.a * (1.0 - 0.5 * vS.w);
        frag = vec4(mix(vec3(1.0), uShadow, a), 1.0);
        return;
    }
    float aa = uPx * 0.75;
    if (uMode == 2) {
        float w = uOutlineW * uPx;
        float a = (1.0 - smoothstep(w - aa, w + aa, abs(d + w))) * vN.a;
        if (a <= 0.003) discard;
        frag = vec4(vN.rgb, a);
        return;
    }
    float cov = 1.0 - smoothstep(-aa, aa, d);
    float a = cov * vN.a;
    if (a <= 0.003) discard;
    // north / south halves across the axis through the piece's center
    vec2 ax = vM.zw;
    float s = length(ax);
    float side = dot(vW - vM.xy, ax / max(s, 1e-4));
    float k = smoothstep(-aa, aa, side);
    float north = mix(0.5, k, s);           // a turning polarity passes through the blend
    north = s > 0.0 ? north : 0.5;
    vec3 col = mix(vS.rgb, vN.rgb, north);
    // matte ferrite: a soft light from the top left across the piece, a fine fixed grain
    vec2 rel = (vW - vM.xy) / max(h * 2.0, 1e-3);
    col *= 1.0 + 0.035 * clamp(rel.y - rel.x, -2.0, 2.0);
    col *= 0.985 + 0.03 * hash21(floor(vW / max(uPx, 1e-4)));
    // a thin lighter edge on the free top faces, the paper's light catching the chamfer
    float rim = (1.0 - smoothstep(0.0, uPx * 2.2, -d)) * (T ? 0.0 : smoothstep(0.0, h * 0.6, q.y));
    col = mix(col, vec3(1.0), 0.16 * rim);
    frag = vec4(col, a);
}
)";

// ---------------------------------------------------------------- vector shapes (the well, the compass)
// kind 0: a segment a -> b, width w0 -> w1 (board units); kind 1: a ring around a, radius b.x, width w0, drawn
// over a fraction `extra` of the turn clockwise from the top.
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aP;
layout(location = 2) in vec4 aQ; // w0, w1, kind, extra
layout(location = 3) in vec4 aC;
uniform mat4 uVP;
uniform float uPx;
out vec2 vW;
flat out vec4 vP, vQ, vC;
void main() {
    float pad = max(aQ.x, aQ.y) + uPx * 2.0;
    vec2 lo, hi;
    if (aQ.z < 0.5) { lo = min(aP.xy, aP.zw) - pad; hi = max(aP.xy, aP.zw) + pad; }
    else { lo = aP.xy - aP.z - pad; hi = aP.xy + aP.z + pad; }
    vW = mix(lo, hi, aCorner * 0.5 + 0.5);
    vP = aP; vQ = aQ; vC = aC;
    gl_Position = uVP * vec4(vW, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vP, vQ, vC;
out vec4 frag;
uniform float uPx;
void main() {
    float d;
    if (vQ.z < 0.5) {
        vec2 a = vP.xy, ba = vP.zw - vP.xy;
        float t = clamp(dot(vW - a, ba) / max(dot(ba, ba), 1e-8), 0.0, 1.0);
        d = length(vW - a - ba * t) - 0.5 * mix(vQ.x, vQ.y, t);
    } else {
        vec2 r = vW - vP.xy;
        d = abs(length(r) - vP.z) - 0.5 * vQ.x;
        float f = fract(atan(r.x, r.y) / 6.2831853 + 1.0);
        float edge = (f - vQ.w) * 6.2831853 * vP.z;
        if (vQ.w < 0.999) d = max(d, edge);
    }
    float a = (1.0 - smoothstep(-0.5 * uPx, 0.5 * uPx, d)) * vC.a;
    if (a <= 0.003) discard;
    frag = vec4(vC.rgb, a);
}
)";

// ---------------------------------------------------------------- the paper
inline const char* PAPER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uPaper;
uniform float uLight;
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main() {
    vec2 c = vUv - 0.5;
    float vig = 1.0 - 0.07 * dot(c, c) * 2.0;
    vec3 col = uPaper * mix(1.0, vig, uLight > 0.5 ? 0.6 : 1.0);
    col += (hash21(floor(gl_FragCoord.xy)) - 0.5) * 0.012;
    frag = vec4(col, 0.0);
}
)";

// ---------------------------------------------------------------- lettering (signed distance glyphs)
inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec4 aPos; // pixels (top-left origin), uv
layout(location = 1) in vec4 aCol;
uniform vec2 uRes;
out vec2 vUv;
out vec4 vCol;
void main() {
    vUv = aPos.zw;
    vCol = aCol;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
in vec4 vCol;
out vec4 frag;
uniform sampler2D uFont;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.7;
    float a = smoothstep(0.5 - w, 0.5 + w, d) * vCol.a;
    if (a <= 0.003) discard;
    frag = vec4(vCol.rgb, a);
}
)";

// ---------------------------------------------------------------- the print: fade, dim, the bridge
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim, uBridge;
uniform vec3 uBridgeCol, uBridgeInk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    float cov = clamp(s.a, 0.0, 1.0);
    c = mix(c, mix(uBridgeCol, uBridgeInk, cov), uBridge);
    c *= 1.0 - 0.45 * uDim;
    frag = vec4(c * uFade, 1.0);
}
)";

} // namespace magnets_shaders

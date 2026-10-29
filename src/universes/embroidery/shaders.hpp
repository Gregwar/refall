#pragma once
// EMBROIDERY's shaders (see embroidery.cpp).
//
// CLOTH_FS draws the aida cloth, the sampler stitched in the margins (from a small texture: one texel a stitch) and
// the well's backstitched outline in one full-screen pass; CELL_FS the pieces (4 x 4 cross stitches a cell, a
// backstitch around each piece), the falling piece's shadow, the running-stitch ghost and the cells being unpicked;
// SEG_FS the needle and its thread; FINAL_FS encodes the frame.
#include <string>

namespace embroidery_shaders {

// ---------------------------------------------------------------- common: the cloth and a cross stitch
inline const char* COMMON = R"(
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
// Aida: square blocks of woven threads, a quarter of a cell each, with holes at their corners. px: board units
// per pixel (the weave fades out where it would alias). Returns the light the cloth reflects (around 1).
float aida(vec2 w, float px) {
    vec2 q = fract(w * 4.0);
    float detail = 1.0 - smoothstep(0.035, 0.09, px);
    vec2 e = min(q, 1.0 - q);
    float block = smoothstep(0.0, 0.16, e.x) * smoothstep(0.0, 0.16, e.y);
    float hole = 1.0 - smoothstep(0.07, 0.15, length(e));
    // the woven threads inside a block: over and under
    vec2 f = fract(w * 16.0);
    float weave = (step(0.5, f.x) == step(0.5, fract(w.y * 8.0)) ? 0.6 : 0.4) + 0.4 * abs(f.y - 0.5);
    float c = 1.0 - (0.035 * (1.0 - block) + 0.07 * hole) * detail + 0.03 * (weave - 0.5) * detail * block;
    c *= 1.0 + 0.03 * (vnoise(w * 0.7) - 0.5) + 0.02 * (vnoise(w * 6.0) - 0.5);
    return c;
}
float segDist(vec2 p, vec2 a, vec2 b, out float t, out float side) {
    vec2 ab = b - a;
    t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 d = p - (a + ab * t);
    side = (ab.x * d.y - ab.y * d.x) / length(ab);
    return length(d);
}
// One strand of floss (two plies twisted) from a to b, in stitch units: its color and coverage (premultiplied).
vec4 strand(vec2 u, vec2 a, vec2 b, vec3 col, float aa, float sheen, float seed) {
    const float hw = 0.2;
    float t, side;
    float d = segDist(u, a, b, t, side);
    float cov = 1.0 - smoothstep(hw - aa, hw + aa, d);
    if (cov <= 0.0) return vec4(0.0);
    float x = clamp(side / hw, -1.0, 1.0);
    float round_ = sqrt(max(0.0, 1.0 - x * x));
    // the twist: plies crossing the strand on a slant
    float ply = fract(t * 4.2 + x * 0.55 + seed);
    float ridge = smoothstep(0.0, 0.18, ply) * smoothstep(1.0, 0.7, ply);
    vec3 c = col * (0.55 + 0.45 * round_) * (0.82 + 0.18 * ridge);
    // the sheen: the light catching the plies' crests
    c += vec3(1.0, 0.98, 0.94) * pow(round_, 6.0) * ridge * sheen * (0.18 + 0.25 * dot(col, vec3(0.33)));
    return vec4(c * cov, cov);
}
// A cross stitch in a square (u in 0..1): the bottom strand, then the top one over it (each 0..1 there), its soft
// shadow on the cloth. Premultiplied.
vec4 crossStitch(vec2 u, vec3 col, float bottom, float top, float aa, float sheen, float seed) {
    const float k = 0.11;
    vec4 o = vec4(0.0);
    // the shadow of the threads on the cloth
    float tt, ss;
    float dA = segDist(u, vec2(k), vec2(1.0 - k), tt, ss), dB = segDist(u, vec2(k, 1.0 - k), vec2(1.0 - k, k), tt, ss);
    float sh = (1.0 - smoothstep(0.12, 0.34, dA)) * bottom * 0.5 + (1.0 - smoothstep(0.12, 0.34, max(dB, 0.0))) * top * 0.5;
    sh = min(sh, 0.6) * 0.45;
    o = vec4(0.0, 0.0, 0.0, sh);
    if (bottom > 0.0) {
        vec4 a = strand(u, vec2(k), vec2(1.0 - k), col * 0.9, aa, sheen, seed) * bottom;
        // the top strand shades the bottom one where it crosses it
        a.rgb *= 1.0 - 0.35 * top * (1.0 - smoothstep(0.1, 0.26, dB));
        o = a + o * (1.0 - a.a);
    }
    if (top > 0.0) {
        vec4 b = strand(u, vec2(k, 1.0 - k), vec2(1.0 - k, k), col, aa, sheen, seed + 0.37) * top;
        o = b + o * (1.0 - b.a);
    }
    return o;
}
// A backstitch along a straight line (units: board), stitches of `len` with a hole between them.
vec4 backstitch(float across, float along, vec3 col, float hw, float len, float aa) {
    float f = fract(along / len);
    float gap = smoothstep(0.0, 0.08, f) * smoothstep(1.0, 0.92, f);
    float cov = (1.0 - smoothstep(hw - aa, hw + aa, abs(across))) * gap;
    float x = clamp(across / hw, -1.0, 1.0);
    vec3 c = col * (0.6 + 0.4 * sqrt(max(0.0, 1.0 - x * x)));
    return vec4(c * cov, cov);
}
)";

// ---------------------------------------------------------------- the cloth, the sampler, the well
inline const char* CLOTH_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;           // center, half extents (board units)
uniform vec2 uRes;
uniform vec3 uCloth;
uniform sampler2D uDesign;    // r: palette index / 16, a: 0 none, ~0.5 half, 1 whole
uniform vec4 uGrid;           // origin x, y, size w, h (stitches)
uniform vec3 uPal[10];
uniform float uDesignA, uSheen, uLight, uVig;
uniform vec3 uWell;           // the well's backstitch thread
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / uRes.y;
    vec3 c = uCloth * aida(w, px);
    // the sampler
    vec2 g = (w - uGrid.xy) * 4.0;
    vec2 gi = floor(g);
    if (gi.x >= 0.0 && gi.y >= 0.0 && gi.x < uGrid.z && gi.y < uGrid.w) {
        vec4 s = texture(uDesign, (gi + 0.5) / uGrid.zw);
        if (s.a > 0.01) {
            int pi = int(s.r * 255.0 / 16.0 + 0.5);
            float bottom = s.a > 0.25 ? 1.0 : 0.0, top = s.a > 0.75 ? 1.0 : 0.0;
            vec4 st = crossStitch(g - gi, uPal[clamp(pi, 0, 9)], bottom, top, 4.0 * px * 0.9, uSheen, hash(gi));
            st *= uDesignA;
            c = st.rgb + c * (1.0 - st.a);
        }
    }
    // the well: a backstitched outline in one dark thread along its sides and its floor
    {
        float hw = 0.045;
        vec4 b = vec4(0.0);
        if (w.y > -10.1 && w.y < 10.0) {
            float ax = abs(w.x) - 5.06;
            if (abs(ax) < 0.1) b = backstitch(ax, w.y + 10.0, uWell, hw, 0.25, px);
        }
        if (abs(w.y + 10.06) < 0.1 && abs(w.x) < 5.06) {
            vec4 f = backstitch(w.y + 10.06, w.x + 5.0, uWell, hw, 0.25, px);
            b = f + b * (1.0 - f.a);
        }
        c = b.rgb + c * (1.0 - b.a);
    }
    vec2 v = vUv * 2.0 - 1.0;
    // a soft daylight from the upper left
    float light = uLight * (1.0 + 0.05 * dot(v, vec2(-0.6, 0.8))) * (1.0 - uVig * dot(v * vec2(0.75, 1.0), v * vec2(0.75, 1.0)));
    frag = vec4(c * light, 0.0);
}
)";

// ---------------------------------------------------------------- the pieces (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // kind, progress, glint, seed
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z * (aC.x > 1.5 && aC.x < 2.5 ? 1.5 : 1.06);
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uSheen, uPxCell;   // board units per pixel, over the cell's size
uniform vec3 uInk;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    float aa = uPxCell / max(vA.z * 2.0, 1e-3);
    if (kind == 2) {
        // the falling piece's shadow on the cloth: a soft square
        vec2 q = abs(p) - 0.42;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
        float a = (1.0 - smoothstep(-0.1, 0.32, d)) * vB.w;
        frag = vec4(0.0, 0.0, 0.0, a);
        return;
    }
    if (kind == 1) {
        // the ghost: a running stitch around the piece's outline (the sides with no cell of the piece)
        vec4 o = vec4(0.0);
        for (int s = 0; s < 4; s++) {
            if (has(m, s)) continue;
            float across = s == 0 ? p.x + 0.45 : s == 1 ? p.x - 0.45 : s == 2 ? p.y + 0.45 : p.y - 0.45;
            float along = (s < 2 ? vW.y : vW.x) * 2.0 + 0.25;
            if (abs(along - floor(along) - 0.5) > 0.27) continue; // the thread is under the cloth there
            if (abs(s < 2 ? p.y : p.x) > 0.56) continue;
            float cov = 1.0 - smoothstep(0.035 - aa, 0.035 + aa, abs(across));
            o = max(o, vec4(uInk * cov, cov));
        }
        frag = o * vB.w;
        return;
    }
    if (abs(p.x) > 0.5 || abs(p.y) > 0.5) discard;
    // 4 x 4 cross stitches
    vec2 g = (p + 0.5) * 4.0;
    vec2 gi = min(floor(g), vec2(3.0));
    float h = hash(gi + vC.w * 17.0);
    float bottom = 1.0, top = 1.0;
    if (kind == 3) {
        // unpicked: each cross in its turn, the top strand first
        float k = clamp((vC.y - h * 0.62) / 0.3, 0.0, 1.0);
        top = 1.0 - smoothstep(0.0, 0.45, k);
        bottom = 1.0 - smoothstep(0.5, 0.95, k);
        if (bottom <= 0.0) discard;
    }
    vec3 col = vB.rgb;
    float sheen = uSheen + vC.z * 2.5;
    vec4 o = crossStitch(g - gi, col, bottom, top, aa * 4.0, sheen, h);
    // the backstitched outline of the piece (in a dark shade of its thread), where it meets something else
    vec4 b = vec4(0.0);
    float bo = kind == 3 ? 1.0 - smoothstep(0.0, 0.4, vC.y) : 1.0;
    for (int s = 0; s < 4; s++) {
        if (has(m, s)) continue;
        float across = s == 0 ? p.x + 0.5 : s == 1 ? p.x - 0.5 : s == 2 ? p.y + 0.5 : p.y - 0.5;
        float along = (s < 2 ? p.y : p.x) + 0.5;
        vec4 e = backstitch(across, along, col * 0.38, 0.05, 0.25, aa);
        b = e + b * (1.0 - e.a);
    }
    o = b * bo + o * (1.0 - b.a * bo);
    frag = o * vB.w;
}
)";

// ---------------------------------------------------------------- the needle and its thread
inline const char* SEG_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x0, y0, x1, y1
layout(location = 2) in vec4 aB; // color, alpha
layout(location = 3) in vec4 aC; // half width, kind
uniform mat4 uVP;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vec2 a = aA.xy, b = aA.zw;
    vec2 d = b - a;
    float l = max(length(d), 1e-4);
    vec2 t = d / l, n = vec2(-t.y, t.x);
    float r = aC.x + 0.06;
    vec2 wp = mix(a - t * r, b + t * r, aPos.x * 0.5 + 0.5) + n * r * aPos.y;
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SEG_FS = R"(#version 330 core
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPx;
float sd(vec2 p, vec2 a, vec2 b, out float side) {
    vec2 ab = b - a;
    float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
    vec2 d = p - (a + ab * t);
    side = (ab.x * d.y - ab.y * d.x) / max(length(ab), 1e-4);
    return length(d);
}
void main() {
    float side;
    float d = sd(vW, vA.xy, vA.zw, side);
    int kind = int(vC.y + 0.5);
    float hw = vC.x;
    if (kind == 2) {
        // a soft shadow
        float a = (1.0 - smoothstep(0.0, hw, d)) * vB.w;
        frag = vec4(0.0, 0.0, 0.0, a);
        return;
    }
    float cov = (1.0 - smoothstep(hw - uPx, hw + uPx, d)) * vB.w;
    float x = clamp(side / max(hw, 1e-4), -1.0, 1.0);
    float r = sqrt(max(0.0, 1.0 - x * x));
    vec3 c = vB.rgb * (0.55 + 0.45 * r);
    // the needle's steel: a bright line along it
    if (kind == 1) c += vec3(0.9) * pow(max(0.0, 1.0 - abs(x + 0.35) * 3.0), 3.0);
    frag = vec4(c * cov, cov);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uSlow, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    // slow motion: evening, a warm lamp over the frame
    vec2 v = (vUv - vec2(0.5, 0.45)) * vec2(1.6, 1.0);
    float lamp = 0.32 + 0.85 * exp(-dot(v, v) * 3.2);
    c = mix(c, c * lamp * vec3(1.05, 0.86, 0.64), uSlow);
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD
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
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
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
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.3, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// A fragment shader with the common functions inserted after its version line.
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace embroidery_shaders

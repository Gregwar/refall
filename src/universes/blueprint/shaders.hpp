#pragma once
// BLUEPRINT's shaders: the paper, the pen lines, the drafted pieces, the lettering.

namespace blueprint_shaders {

// ---------------------------------------------------------------- paper
// The sheet: an even paper tone with a very soft, slow mottle (the wash of a cyanotype, the tooth of drafting film)
// and a fine grain; the well's ground stays perfectly even.
inline const char* PAPER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uPaper, uPaper2;
uniform vec4 uView;   // center x y, half extent x y (board units)
uniform float uMottle, uGrain, uVignette, uSeed;
uniform float uBridge;
uniform vec3 uBridgeCol;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
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
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw; // board units
    float m = fbm(w * 0.09) * 0.65 + fbm(w * 0.33 + 4.1) * 0.35;
    // the well's ground stays calm
    vec2 q = abs(w) - vec2(5.6, 10.6);
    float well = 1.0 - smoothstep(0.0, 1.5, max(q.x, q.y));
    m = mix(m, 0.5, well * 0.85);
    vec3 c = mix(uPaper, uPaper2, clamp((m - 0.35) * 1.6, 0.0, 1.0) * uMottle);
    // a slight fall-off toward the sheet's corners
    vec2 e = (vUv - 0.5) * 2.0;
    c *= 1.0 - uVignette * dot(e * vec2(0.8, 1.0), e * vec2(0.8, 1.0)) * 0.5;
    // fine grain
    float g = hash(floor(gl_FragCoord.xy)) - 0.5;
    c *= 1.0 + g * uGrain;
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- pen lines
// One instance per segment: endpoints in board units, a color, a width in pixels and a pattern (solid, hidden line
// dashes, center line dash-dot) continued along the polyline (dist0: the distance from the stroke's start).
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x0 y0 x1 y1
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // width px, pattern, dist0, dash scale
uniform mat4 uVP;
uniform float uPxPerUnit;
out vec2 vSeg;   // along, across (board units)
flat out float vLen;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vec2 p0 = aA.xy, p1 = aA.zw;
    vec2 d = p1 - p0;
    float L = length(d);
    vec2 t = L > 1e-6 ? d / L : vec2(1.0, 0.0);
    vec2 n = vec2(-t.y, t.x);
    float hw = (aC.x * 0.5 + 1.0) / uPxPerUnit;
    float along = (aPos.x * 0.5 + 0.5) * (L + 2.0 * hw) - hw;
    vec2 wp = p0 + t * along + n * aPos.y * hw;
    vSeg = vec2(along, aPos.y * hw);
    vLen = L;
    vB = aB;
    vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* LINE_FS = R"(#version 330 core
in vec2 vSeg;
flat in float vLen;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPxPerUnit;
void main() {
    float dx = max(max(-vSeg.x, vSeg.x - vLen), 0.0);
    float d = length(vec2(dx, vSeg.y)) * uPxPerUnit;
    float hw = vC.x * 0.5;
    // thin lines keep their darkness (coverage spread over a pixel instead of fading out)
    float a = clamp(hw + 0.5 - d, 0.0, 1.0);
    if (hw < 0.5) a = clamp(1.0 - d, 0.0, 1.0) * (hw * 2.0);
    int pat = int(vC.y + 0.5);
    if (pat > 0) {
        float s = max(vC.w, 1e-3);
        float u = (vC.z + clamp(vSeg.x, 0.0, vLen)) / s;
        float px = uPxPerUnit * s; // pixels per pattern unit
        float on;
        if (pat == 1) {
            // hidden line: dash 0.6, gap 0.4
            float f = fract(u);
            on = clamp((0.6 - f) * px, 0.0, 1.0) * clamp(f * px + 0.5, 0.0, 1.0);
        } else if (pat == 2) {
            // center line: long dash, gap, short dash, gap (over 2 units)
            float f = fract(u * 0.5) * 2.0;
            float long_ = clamp((1.3 - f) * px, 0.0, 1.0) * clamp(f * px + 0.5, 0.0, 1.0);
            float short_ = clamp((f - 1.5) * px + 0.5, 0.0, 1.0) * clamp((1.7 - f) * px, 0.0, 1.0);
            on = max(long_, short_);
        } else {
            // dotted: short dots
            float f = fract(u);
            on = clamp((0.25 - f) * px, 0.0, 1.0) * clamp(f * px + 0.5, 0.0, 1.0);
        }
        a *= on;
    }
    a *= vB.a;
    frag = vec4(vB.rgb * a, a);
}
)";

// Composites a premultiplied layer (the pen lines, gathered with MAX blending so that joints and crossings of
// the same ink never darken) over the scene.
inline const char* LAYER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uLayer;
void main() { frag = texture(uLayer, vUv); }
)";

// ---------------------------------------------------------------- pieces
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // kind, hatch style, hatch alpha, erase 0..1
layout(location = 4) in vec4 aD; // line rgb, hatch front (board y: hatching drawn below it)
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform float uLineW, uHatchW, uFillA, uSpacing, uPxPerUnit, uHatchShade;
uniform vec3 uPaper, uRing, uAccent;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the part the cell belongs to: its neighbors of the same piece
// (mask bits: left right down up, down-left down-right up-left up-right) extend it, the others leave a gap.
float shapeSdf(vec2 p, int m) {
    const float g = 0.035, r = 0.02, rc = 0.02;
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
float hashv(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 15731.7); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hashv(i), hashv(i + vec2(1, 0)), f.x), mix(hashv(i + vec2(0, 1)), hashv(i + vec2(1, 1)), f.x), f.y);
}
// A line of a family of parallel lines (coordinate u in board units, spacing sp): coverage, anti-aliased.
float lines(float u, float sp, float wpx) {
    float d = abs(fract(u / sp + 0.5) - 0.5) * sp * uPxPerUnit; // pixels to the nearest line
    return clamp(wpx * 0.5 + 0.5 - d, 0.0, 1.0);
}
// The section hatching of each piece type (I O T S Z J L), in board space so a part's cells continue each other.
float hatch(int style, vec2 w) {
    float sp = uSpacing, wp = uHatchW;
    const float k = 0.70710678;
    if (style == 0) return lines((w.x + w.y) * k, sp, wp);                         // 45 degrees
    if (style == 1) return max(lines((w.x + w.y) * k, sp * 1.35, wp), lines((w.x - w.y) * k, sp * 1.35, wp)); // cross
    if (style == 2) {                                                                // dots
        vec2 g = w / (sp * 1.1);
        vec2 o = vec2(mod(floor(g.y), 2.0) * 0.5, 0.0);
        vec2 f = fract(g + o) - 0.5;
        float d = length(f) * sp * 1.1 * uPxPerUnit;
        return clamp(wp * 1.25 + 0.5 - d, 0.0, 1.0);
    }
    if (style == 3) return lines(w.y, sp, wp);                                      // horizontal
    if (style == 4) return lines((w.x - w.y) * k, sp, wp);                          // 135 degrees
    if (style == 5) return lines(w.x, sp, wp);                                      // vertical
    // steel: pairs of 45 degree lines
    float u = (w.x + w.y) * k;
    return max(lines(u, sp * 2.0, wp), lines(u + sp * 0.45, sp * 2.0, wp));
}

void main() {
    int kind = int(vC.x + 0.5);
    float pxL = length(fwidth(vLocal)) * 0.7071; // cell units per pixel
    float aa = max(pxL, 1e-4);
    vec3 tint = mix(vB.rgb, uBridgeInk, uBridge);
    vec3 lineC = mix(vD.rgb, uBridgeInk, uBridge);
    if (kind == 3) {
        // the gauge: a drafted circle, its fill hatched from the bottom up as it charges
        float r = length(vLocal) * 2.0;
        float pr = pxL * 2.0;
        float level = vD.w * 2.0 - 1.0;
        float rim = 1.0 - smoothstep(uLineW * pr * 0.5, uLineW * pr * 0.5 + pr, abs(r - 1.0));
        float inside = 1.0 - smoothstep(0.92 - pr, 0.92, r);
        float filled = inside * smoothstep(level + pr, level - pr, vLocal.y * 2.0);
        float h = lines((vWorld.x + vWorld.y) * 0.7071, uSpacing * 0.9, uHatchW) * filled;
        float cm = (1.0 - smoothstep(0.0, pr, abs(vLocal.x * 2.0))) * step(abs(vLocal.y * 2.0), 1.18)
                 + (1.0 - smoothstep(0.0, pr, abs(vLocal.y * 2.0))) * step(abs(vLocal.x * 2.0), 1.18);
        float on = vB.a;
        float a = clamp(rim * 0.9 + h * (0.5 + 0.5 * on) + min(cm, 1.0) * 0.35, 0.0, 1.0);
        vec3 c = mix(uRing, uAccent, on * filled * 0.6);
        frag = vec4(c * a, a) * vC.z;
        return;
    }
    int mk = int(vA.w + 0.5);
    // each cell paints only its own square toward its neighbors of the same part (no overlap: no seams)
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float sd = shapeSdf(vLocal, mk);
    if (kind == 1) {
        // the ghost: a hidden line, dashed along the part's outline
        float lw = uLineW * aa;
        float band = 1.0 - smoothstep(lw * 0.5, lw * 0.5 + aa, abs(sd + lw * 0.5 + aa * 0.5));
        // dash along x on horizontal edges, along y on vertical ones
        float ex = abs(abs(vLocal.x) - 0.5), ey = abs(abs(vLocal.y) - 0.5);
        float along = ex < ey ? vWorld.y : vWorld.x;
        float f = fract(along * 3.0);
        float fa = max(fwidth(along * 3.0), 1e-4);
        float dash = smoothstep(0.62 + fa, 0.62 - fa, f);
        float a = band * dash * vB.a;
        frag = vec4(lineC * a, a);
        return;
    }
    // a drafted part: a light wash of its tint, its section hatching, a crisp outline
    float inside = 1.0 - smoothstep(-aa, aa * 0.5, sd);
    float lw = uLineW * aa;
    float edge = 1.0 - smoothstep(lw * 0.5, lw * 0.5 + aa, abs(sd + lw * 0.5 + aa * 0.5));
    float h = hatch(int(vC.y + 0.5), vWorld) * (1.0 - smoothstep(-lw * 2.0, -lw, sd));
    // the hatching is drawn up from the impact: only below the front
    float front = vD.w;
    h *= smoothstep(front + 0.04, front - 0.04, vWorld.y);
    float ha = h * vC.z;
    float erase = vC.w;
    float keep = 1.0;
    if (erase > 0.0) {
        // rubbed out: the eraser's strokes take the lines away in streaks, then the rest lifts
        float n = vnoise(vec2(vWorld.x * 2.2, vWorld.y * 11.0)) * 0.7 + vnoise(vWorld * 23.0) * 0.3;
        keep = smoothstep(erase * 1.5 - 0.25, erase * 1.5 - 0.05, n) * (1.0 - smoothstep(0.6, 1.0, erase));
    }
    float wash = inside * uFillA;
    float la = clamp(max(edge, ha), 0.0, 1.0);
    vec3 lc = mix(tint * uHatchShade, lineC, edge);
    // premultiplied: the wash under the lines
    float a = (wash * (1.0 - la) + la) * vB.a * keep;
    vec3 col = (tint * wash * (1.0 - la) + lc * la) * vB.a * keep;
    frag = vec4(col, a);
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
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
layout(location = 1) in vec2 aUv;
uniform vec2 uRes;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
// Lettering with a thin halo of paper, so it stays clean where it crosses a line.
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.45 - w, 0.45 + w, d);
    float halo = smoothstep(0.28 - w, 0.28 + w, d) * uHaloA;
    // the ink over its halo (premultiplied), so thin strokes keep their color
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill);
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
    frag = vec4(pow(max(c, 0.0), vec3(1.0 / 2.2)), 1.0);
}
)";

} // namespace blueprint_shaders

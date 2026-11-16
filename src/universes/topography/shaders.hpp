#pragma once
// TOPOGRAPHY's shaders (see topography.cpp).
//
// MAP_FS draws the whole sheet from one small float texture computed on the CPU (r: the elevation in contour
// intervals, g: how deep a pond the hollow there holds, b: woodland): it is sampled with a B-spline (four bilinear taps)
// so the contours stay smooth between the grid's samples, and the isolines are drawn at screen-space width with
// fwidth. CELL_FS draws the pieces as small tinted plateaus (a crisp outline and two form lines inside, the cells of
// one piece merged into one shape), the ghost as a dashed boundary and the hovering piece's shadow.
// Colors are linear; the post pass encodes them.

namespace topography_shaders {

// ---------------------------------------------------------------- the map
inline const char* MAP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uField;
uniform vec4 uGrid;      // x0, y0, 1 / spacing (board units)
uniform vec4 uView;      // center, half extents (board units)
uniform vec2 uRes;
uniform float uLineW;    // the minor contour's width (pixels)
uniform vec3 uPaper, uLine, uIndex, uWaterLine, uLake, uTint0, uTint1, uTint2, uWood;
uniform float uLineAlpha, uTintAmt, uShade, uWoodAmt, uHMin, uHMax;
uniform float uPond, uPondMin;
uniform float uWater, uWaterOn, uPhase, uWellCalm, uSlow, uDark, uFurn, uGameOn;
uniform float uBridge;
uniform vec3 uBridgeCol;
uniform vec4 uLeftCol, uRightCol;
uniform float uHalf, uShoreKick;
uniform float uDraw; // the setup: the survey drawn in, from the valleys up to the peaks (0 blank paper .. 1 done)
uniform vec4 uHud[40];
uniform int uHudN;

vec4 cubicW(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x, y = s.y - 4.0 * s.x, z = s.z - 4.0 * s.y + 6.0 * s.x;
    return vec4(x, y, z, 6.0 - x - y - z) * (1.0 / 6.0);
}
// B-spline sampling from texel coordinates (texel k's center at k + 0.5).
vec4 bspline(vec2 tc) {
    vec2 size = vec2(textureSize(uField, 0));
    tc -= 0.5;
    vec2 f = fract(tc);
    tc -= f;
    vec4 xc = cubicW(f.x), yc = cubicW(f.y);
    vec4 c = tc.xxyy + vec2(-0.5, 1.5).xyxy;
    vec4 s = vec4(xc.xz + xc.yw, yc.xz + yc.yw);
    vec4 o = (c + vec4(xc.yw, yc.yw) / s) / size.xxyy;
    vec4 s0 = texture(uField, o.xz), s1 = texture(uField, o.yz), s2 = texture(uField, o.xw), s3 = texture(uField, o.yw);
    float sx = s.x / (s.x + s.y), sy = s.z / (s.z + s.w);
    return mix(mix(s3, s2, sx), mix(s1, s0, sx), sy);
}
vec4 fieldAt(vec2 w) { return bspline((w - uGrid.xy) * uGrid.z + 0.5); }

float box(vec2 p, vec4 r) { // inside a rectangle (x0 y0 x1 y1), soft edge
    vec2 c = (r.xy + r.zw) * 0.5, h = (r.zw - r.xy) * 0.5;
    vec2 d = abs(p - c) - h;
    return 1.0 - smoothstep(-0.6, 0.4, max(d.x, d.y));
}
// a line `w` px wide at distance d (px)
float stroke(float d, float w) { return 1.0 - smoothstep(w * 0.5 - 0.5, w * 0.5 + 0.5, d); }

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / uRes.y; // board units per pixel
    vec4 F = fieldAt(w);
    float h = F.r;
    // the survey being drawn: what lies below the surveyor's level is on the sheet, the rest still blank
    float drawn = uDraw >= 1.0 ? 1.0 : 1.0 - smoothstep(-1.5, 0.0, h - mix(uHMin + 0.5, 6.0, uDraw));
    float fw = max(fwidth(h), 1e-5);
    // the slope, for the relief shading
    float e = 0.5 / uGrid.z;
    float gx = (fieldAt(w + vec2(e, 0.0)).r - fieldAt(w - vec2(e, 0.0)).r) / (2.0 * e);
    float gy = (fieldAt(w + vec2(0.0, e)).r - fieldAt(w - vec2(0.0, e)).r) / (2.0 * e);

    // ---- The ground: paper, woodland, layer tints, relief shading.
    vec3 col = uPaper;
    float wood = smoothstep(0.5 - fwidth(F.b), 0.5 + fwidth(F.b), F.b) * uWoodAmt;
    col = mix(col, uWood, wood * drawn);
    float f = fract(h);
    float hq = floor(h) + smoothstep(1.0 - fw, 1.0, f);
    float band = clamp((hq + 0.5 - uHMin) / max(uHMax - uHMin, 1.0), 0.0, 1.0);
    vec3 tint = band < 0.5 ? mix(uTint0, uTint1, band * 2.0) : mix(uTint1, uTint2, band * 2.0 - 1.0);
    col = mix(col, tint, uTintAmt);
    float z = 0.55 + 0.25 * uSlow;
    vec3 n = normalize(vec3(-gx * z, -gy * z, 1.0));
    vec3 L = normalize(mix(vec3(-0.55, 0.65, 0.75), vec3(-0.85, 0.35, 0.3), uSlow));
    float sh = min(dot(n, L) - L.z, 0.12);
    col *= 1.0 + sh * uShade * drawn * (uDark > 0.5 ? 1.4 : 1.0);
    col = max(col, vec3(0.0));

    // ---- Where the world keeps quiet: the well, the HUD columns.
    float inWell = box(w, vec4(-5.0, -10.0, 5.0, 10.0));
    float inCols = max(box(w, uLeftCol), box(w, uRightCol));
    float quiet = mix(1.0, uWellCalm, inWell) * mix(1.0, 0.5, inCols * uGameOn);
    // behind the HUD's text and the previews the map nearly fades out
    vec2 pix = vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y);
    float hud = 0.0;
    for (int i = 0; i < uHudN; i++) {
        vec4 r = uHud[i];
        vec2 dd = max(r.xy - pix, pix - r.zw);
        float k = length(max(dd, 0.0)) + min(max(dd.x, dd.y), 0.0); // rounded box
        hud = max(hud, 1.0 - smoothstep(-6.0, 16.0, k));
    }
    quiet *= 1.0 - 0.85 * hud;

    // ---- Water: the lakes (with the lines along their shores).
    // the ponds the rain fills in the hollows (their spill level, rising with the rain) and the flood
    float pondD = F.g - uPondMin * 0.6 - (1.0 - uPond) * 1.5;
    float depth = max(uWater - h, pondD);
    // the shore at screen-space width by the depth's own gradient: a pond's (from g) is not the elevation's
    float dfw = max(fwidth(depth), 1e-5);
    float wet = smoothstep(-dfw, dfw, depth) * uWaterOn * drawn;
    vec3 lake = mix(uLake, uLake * 0.93, smoothstep(0.0, 3.0, depth));
    col = mix(col, lake, wet);

    // ---- The contours (index contours every fifth line), drawn in the water's ink under the lakes.
    float d = abs(fract(h + 0.5) - 0.5) / fw;
    float idx = abs(mod(floor(h + 0.5), 5.0)) < 0.5 ? 1.0 : 0.0;
    float lw = uLineW * mix(1.0, 2.1, idx);
    float lwp = max(lw, 0.9);
    float line = stroke(d, lwp) * min(1.0, lw / 0.9);
    float spacing = 1.0 / fw; // pixels between two lines
    line *= mix(smoothstep(2.5, 6.0, spacing), smoothstep(1.2, 2.5, spacing), idx);
    vec3 lc = mix(uLine, uIndex, idx);
    float la = uLineAlpha * quiet * mix(1.0, 0.8, uSlow) * drawn;
    col = mix(col, mix(lc, uWaterLine, wet), line * la * mix(1.0, 0.22, wet));
    // the drop's half-interval contours: the map gets denser
    float dh = abs(fract(h) - 0.5) / fw;
    float hl = stroke(dh, max(uLineW * 0.85, 0.8)) * smoothstep(4.0, 9.0, spacing) * uHalf;
    col = mix(col, mix(uLine, uWaterLine, wet), hl * la * 0.6 * mix(1.0, 0.22, wet));

    // shoreline and water lines (parallel to the shore, sparser and fainter offshore, moving out with the music)
    float sd = abs(depth) / dfw;
    float shore = stroke(sd, max(uLineW * (1.6 + 1.6 * uShoreKick), 1.0)) * uWaterOn * drawn;
    float wd = depth * 3.0 - uPhase;
    float wfw = max(fwidth(wd), 1e-5);
    float wl = stroke(abs(fract(wd + 0.5) - 0.5) / wfw, max(uLineW, 0.85)) * smoothstep(0.05, 0.2, depth) *
               exp(-depth * 1.1) * smoothstep(2.5, 6.0, 1.0 / wfw);
    col = mix(col, uWaterLine, clamp(shore * 0.9 + wl * wet, 0.0, 1.0) * mix(1.0, 0.55, inWell) * (1.0 - 0.85 * hud));

    // ---- Map furniture: the well's neat line with ticks, a scale bar and a north arrow.
    {
        vec2 q = abs(w) - vec2(5.12, 10.12);
        float nl = stroke(abs(max(q.x, q.y)) / px, max(uLineW * 1.3, 1.0)) * step(max(q.x, q.y), 0.3) * step(min(q.x, q.y), 0.0);
        float tick = 0.0;
        if (q.x > 0.0 && q.x < 0.32 && abs(w.y) < 10.1) tick = stroke(abs(fract(w.y / 5.0 + 0.5) - 0.5) * 5.0 / px, max(uLineW, 0.9));
        if (q.y > 0.0 && q.y < 0.32 && abs(w.x) < 5.1) tick = max(tick, stroke(abs(fract(w.x / 5.0 + 0.5) - 0.5) * 5.0 / px, max(uLineW, 0.9)));
        col = mix(col, uIndex, (nl + tick * (1.0 - nl)) * 0.8 * uFurn * uGameOn);
        // scale bar: four units, alternately filled
        vec2 sb = w - vec2(13.0, -9.7);
        if (sb.x > -0.2 && sb.x < 4.2 && sb.y > -0.3 && sb.y < 0.3) {
            float inBar = step(0.0, sb.x) * step(sb.x, 4.0) * step(-0.09, sb.y) * step(sb.y, 0.09);
            float edge = stroke(abs(max(abs(sb.x - 2.0) - 2.0, abs(sb.y) - 0.09)) / px, max(uLineW, 0.9));
            float fillB = inBar * step(0.5, mod(floor(sb.x), 2.0));
            col = mix(col, uIndex, clamp(edge + fillB, 0.0, 1.0) * 0.85 * uFurn);
        }
        // north arrow: a slim needle, its west half filled
        vec2 na = w - vec2(18.4, -9.95);
        if (abs(na.x) < 0.5 && na.y > -0.2 && na.y < 1.6) {
            float t = clamp(na.y / 1.4, 0.0, 1.0);
            float half_ = 0.24 * (1.0 - t);
            float inside = step(abs(na.x), half_) * step(0.0, na.y) * step(na.y, 1.4);
            float ed = max(abs(na.x) - half_, max(-na.y, na.y - 1.4));
            float outline = stroke(abs(ed) / px, max(uLineW, 0.9));
            float west = inside * step(na.x, 0.0);
            col = mix(col, uIndex, clamp(outline + west, 0.0, 1.0) * 0.85 * uFurn);
        }
    }

    // slow motion: a dusk over the relief
    col = mix(col, col * vec3(0.78, 0.84, 1.0), uSlow * 0.55);
    col = mix(col, uBridgeCol, uBridge);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- pieces, ghost, shadow
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // fill rgb, alpha
layout(location = 3) in vec4 aC; // kind, form-line inset, form-line alpha, shrink
layout(location = 4) in vec4 aD; // line rgb, -
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float pad = aC.x > 1.5 && aC.x < 2.5 ? 0.45 : 0.08;
    float ext = aA.z + pad;
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
uniform float uLineW;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the shape the cell belongs to: its neighbors of the same piece
// (mask bits: left right down up, down-left down-right up-left up-right) extend it, the others leave a gap.
float shapeSdf(vec2 p, int m) {
    const float g = 0.045, r = 0.13, rc = 0.07;
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
    int kind = int(vC.x + 0.5);
    float pxL = length(fwidth(vLocal)) * 0.7071; // cell units per pixel
    vec3 fillC = mix(vB.rgb, uBridgeInk, uBridge);
    vec3 lineC = mix(vD.rgb, uBridgeInk, uBridge);
    float sd = shapeSdf(vLocal, int(vA.w + 0.5)) + vC.w;
    float aa = max(pxL, 1e-4);
    if (kind == 2) {
        // the hovering piece's soft shadow on the map, each place drawn once: by the nearest of the piece's cells (the
        // cells' padded quads overlap, and the shadows would add up into darker squares and seams)
        int m = int(vA.w + 0.5);
        const vec2 nb[8] = vec2[8](vec2(-1, 0), vec2(1, 0), vec2(0, -1), vec2(0, 1), vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
        float own = length(vLocal);
        for (int i = 0; i < 8; i++)
            if (has(m, i) && length(vLocal - nb[i]) < own) discard;
        float a = (1.0 - smoothstep(-0.2, 0.3, sd)) * vB.a;
        frag = vec4(fillC * a, a);
        return;
    }
    if (kind == 1) {
        // the ghost: a dashed boundary, like a border on the map
        float band = 1.0 - smoothstep(uLineW * 0.75 * aa, uLineW * 0.75 * aa + aa, abs(sd + 0.06));
        float along = (vWorld.x + vWorld.y) * 3.0;
        float fa = max(fwidth(along), 1e-4);
        float dash = smoothstep(0.42 - fa, 0.42 + fa, abs(fract(along) - 0.5) * 2.0);
        float a = band * dash * vB.a;
        frag = vec4(fillC * a, a);
        return;
    }
    // a plateau: the tint, a crisp outline, two form lines inside it
    float fill = 1.0 - smoothstep(-aa, aa * 0.5, sd);
    float lw = uLineW * aa;
    float edge = 1.0 - smoothstep(lw * 0.6, lw * 0.6 + aa, abs(sd + lw * 0.6 + aa * 0.5));
    float in1 = 1.0 - smoothstep(lw * 0.5, lw * 0.5 + aa, abs(sd + vC.y));
    float in2 = 1.0 - smoothstep(lw * 0.5, lw * 0.5 + aa, abs(sd + vC.y * 2.0));
    float la = clamp(edge * 0.85 + (in1 * 0.55 + in2 * 0.0) * vC.z, 0.0, 1.0) * (1.0 - uBridge);
    vec3 c = mix(fillC, lineC, la);
    float a = fill * vB.a;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- HUD
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

} // namespace topography_shaders

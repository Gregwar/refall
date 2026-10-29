#pragma once
// SUNPRINT's shaders (see sunprint.cpp).
//
// The sheet keeps a tint map (EXPOSE_FS turns the open paper to the sheet's current tint, and keeps the tint under
// what lies on it or is held right above it);
// PAPER_FS turns it into the print's colors and lights the sheet with the sun (the live shadows, tinted by the glass);
// PLATE_FS draws the glass plates over it, LEAF_FS the pressed ferns; FINAL_FS encodes the frame. The occluders
// (the stack and what is raised above the sheet) are first drawn as coverage and tint into two half-resolution
// targets (OCC_FS, LEAF_OCC_FS) and blurred into penumbras.
#include <string>

namespace sunprint_shaders {

// ---------------------------------------------------------------- common: the sheet's coating, the clouds
inline const char* COMMON = R"(
uniform vec4 uView;   // center, half extents (board units)
uniform vec2 uCoat;   // the coated area's half extents
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
vec2 viewUv(vec2 w) { return (w - uView.xy) / uView.zw * 0.5 + 0.5; }
// The sensitizer brushed on the sheet: a wide rectangle with ragged, streaky edges (the brush ran horizontally).
float coated(vec2 w) {
    float side = w.x < 0.0 ? 3.1 : 7.7;
    float ex = uCoat.x + 0.7 * (vnoise(vec2(w.y * 0.32, side)) - 0.5) + 0.35 * (vnoise(vec2(w.y * 2.7, side + 1.0)) - 0.5);
    float top = w.y < 0.0 ? 11.3 : 13.9;
    float ey = uCoat.y + 0.5 * (vnoise(vec2(w.x * 0.27, top)) - 0.5) + 0.22 * (vnoise(vec2(w.x * 3.1, top + 1.0)) - 0.5);
    float d = max(abs(w.x) - ex, abs(w.y) - ey);
    float c = 1.0 - smoothstep(-0.35, 0.08, d);
    // bristle streaks thinning the coat toward the edges
    float streak = vnoise(vec2(w.x * 0.08, w.y * 5.5));
    c *= mix(1.0, 0.55 + 0.45 * streak, smoothstep(-2.2, 0.0, d));
    return c;
}
float clouds(vec2 w, float t) {
    vec2 q = w * 0.045 + vec2(t * 0.012, t * 0.004);
    float n = vnoise(q) * 0.65 + vnoise(q * 2.3 + 4.1) * 0.35;
    return smoothstep(0.45, 0.78, n);
}
)";

// ---------------------------------------------------------------- the plates (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // kind, lift, glint, shade
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vW = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// Signed distance (cell units) to the outline of the plate the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right): its own cells merge, a thin seam parts it from the others.
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m) {
    const float g = 0.03, r = 0.1, rc = 0.05;
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
// Each cell draws only its own square where it joins a cell of the same plate (no double coverage at the seams).
bool ownCell(vec2 p, int m) {
    if (p.x < -0.5 && has(m, 0)) return false;
    if (p.x > 0.5 && has(m, 1)) return false;
    if (p.y < -0.5 && has(m, 2)) return false;
    if (p.y > 0.5 && has(m, 3)) return false;
    return true;
}
)";

// Coverage and tint of the occluders (premultiplied), into the half-resolution shadow targets.
inline const char* OCC_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    if (!ownCell(vLocal, int(vA.w + 0.5))) discard;
    float sd = shapeSdf(vLocal, int(vA.w + 0.5));
    float a = (1.0 - smoothstep(-aa, aa, sd)) * vB.w;
    frag = vec4(vB.rgb * a, a);
}
)";

inline const char* PLATE_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform vec2 uSunDir;    // toward the sun, on the sheet
uniform float uSunLvl;   // 0 overcast .. 1 full sun
uniform vec3 uInk;
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float sd = shapeSdf(vLocal, m);
    if (kind == 1) {
        // the ghost: a fine pencil outline where the plate will lie
        float line = 1.0 - smoothstep(0.4 * aa, 1.4 * aa, abs(sd + 0.03));
        float a = line * vB.w;
        frag = vec4(uInk * a, a);
        return;
    }
    if (!ownCell(vLocal, m)) discard;
    float a = (1.0 - smoothstep(-aa, aa * 0.5, sd)) * vB.w;
    if (a <= 0.0) discard;
    // the outline's normal (outward, cell units)
    const float e = 0.02;
    vec2 n = vec2(shapeSdf(vLocal + vec2(e, 0.0), m) - shapeSdf(vLocal - vec2(e, 0.0), m),
                  shapeSdf(vLocal + vec2(0.0, e), m) - shapeSdf(vLocal - vec2(0.0, e), m));
    n = length(n) > 1e-4 ? normalize(n) : vec2(0.0);
    float depth = clamp(-sd, 0.0, 0.5);
    float bevel = 1.0 - smoothstep(0.0, 0.09, depth);
    float facing = dot(n, uSunDir);
    vec3 tint = vB.rgb;
    // the glass: its color lit through, a little lighter on the side away from the sun (the light it carries)
    float across = dot(vW, -uSunDir);
    vec3 c = tint * (0.82 + 0.08 * sin(across * 0.9) + 0.1 * vC.w);
    // a soft reflection band across the glass (in world space: no seam between a plate's cells)
    float band = exp(-pow(fract(dot(vW, normalize(vec2(1.0, 0.55))) * 0.06 + 0.3) - 0.5, 2.0) * 90.0);
    c += vec3(0.05) * band * uSunLvl;
    // the bevel: lit on the sun's side, dark on the other, a thin dark line at the very edge
    c += vec3(0.5) * bevel * max(facing, 0.0) * (0.35 + 0.65 * uSunLvl);
    c *= 1.0 - 0.38 * bevel * max(-facing, 0.0);
    float edge = 1.0 - smoothstep(0.0, 0.025, depth);
    c = mix(c, tint * 0.35, 0.55 * edge);
    // a glint running along the sunlit bevel (the kicks of a peak)
    c += vec3(1.0, 0.97, 0.9) * vC.z * (bevel * max(facing, 0.0) * 1.6 + 0.12);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the ferns (instanced leaflets)
inline const char* LEAF_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half length, half width
layout(location = 2) in vec4 aB; // angle, alpha
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vA;
flat out vec4 vB;
void main() {
    vec2 h = aA.zw + 0.04;
    vec2 local = aPos * h;
    float c = cos(aB.x), s = sin(aB.x);
    vec2 wp = aA.xy + vec2(c * local.x - s * local.y, s * local.x + c * local.y);
    vQ = local;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* LEAF_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vA;
flat in vec4 vB;
out vec4 frag;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    // a leaflet: an ellipse tapering to its tip
    vec2 q = vQ / vA.zw;
    float taper = 1.0 - 0.45 * max(q.x, 0.0);
    float d = length(vec2(q.x, q.y / max(taper, 0.2))) - 1.0;
    float aa = max(fwidth(d), 1e-4);
    float a = (1.0 - smoothstep(-aa, aa, d)) * vB.y * uAlpha;
    frag = vec4(uColor * a, a);
}
)";

// ---------------------------------------------------------------- the penumbra (a separable blur)
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    vec4 c = texture(uSrc, vUv) * w[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)) + texture(uSrc, vUv - uDir * float(i))) * w[i];
    frag = c;
}
)";

// ---------------------------------------------------------------- the exposure: light reaching the paper
inline const char* EXPOSE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uOcc, uFall, uPrev;
uniform float uTint;       // the tint the open paper goes to (0, 1)
uniform float uRate;       // how far it gets there this frame
uniform float uKeep;       // the rinse of a new song (1: none)
uniform vec4 uHudL, uHudR; // the HUD columns (x0, y0, x1, y1): the previews there leave no print
float hud(vec2 w, vec4 r) { return step(r.x, w.x) * step(w.x, r.z) * step(r.y, w.y) * step(w.y, r.w); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // what lies on the sheet (the plates, the ferns) and the falling plate right above it keep the paper's tint
    float lying = smoothstep(0.45, 0.7, texture(uOcc, vUv).a);
    float held = smoothstep(0.45, 0.7, texture(uFall, vUv).a);
    float cover = max(lying, held) * (1.0 - max(hud(w, uHudL), hud(w, uHudR)));
    float t = texture(uPrev, vUv).r;
    t = mix(t, uTint, 1.0 - uKeep);
    t = mix(t, uTint, uRate * (1.0 - cover));
    frag = vec4(t, 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- the sheet in the sun
inline const char* PAPER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uExpo, uOcc, uFall;
uniform vec2 uOffFlat, uOffRaised;
uniform vec3 uPaper;          // the bare paper (outside the coat)
uniform vec3 uTintA, uTintB;  // the coat's two tints
uniform vec3 uDropA, uDropB;  // the same, for the drop
uniform float uDev;           // the drop's tints 0..1
uniform float uBright, uShadow, uGlassT, uCloud, uTime, uVig;
uniform vec3 uSky;            // the light in the shadows (a cool sky)
uniform vec4 uFlare;          // x, y, radius, brightness
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / float(textureSize(uExpo, 0).y);
    // the print
    float p = texture(uExpo, vUv).r;
    float coat = coated(w);
    // (mixed in display space)
    vec3 a = pow(mix(uTintA, uDropA, uDev), vec3(1.0 / 2.2)), b = pow(mix(uTintB, uDropB, uDev), vec3(1.0 / 2.2));
    vec3 print_ = pow(mix(a, b, p), vec3(2.2));
    // the pigment settles unevenly: a faint mottling
    print_ *= 1.0 + 0.035 * (vnoise(w * 1.4) - 0.5) + 0.02 * (vnoise(w * 4.3 + 9.0) - 0.5);
    vec3 c = mix(uPaper, print_, coat);
    // the paper: a fine fiber, a soft tooth
    float fib = vnoise(w * vec2(9.0, 3.0)) * 0.5 + vnoise(w * 23.0) * 0.5;
    c *= 1.0 - 0.035 * (fib - 0.5);
    // the sun: the live shadows of the plates (the light through the glass keeps its color) and of what is raised
    vec4 oF = max(max(texture(uOcc, viewUv(w)), texture(uOcc, viewUv(w - uOffFlat * 0.5))), texture(uOcc, viewUv(w - uOffFlat)));
    vec4 oR = texture(uFall, viewUv(w - uOffRaised));
    vec3 tF = oF.rgb / max(oF.a, 1e-3), tR = oR.rgb / max(oR.a, 1e-3);
    vec3 trF = (1.0 - oF.a) + oF.a * tF * uGlassT;
    vec3 trR = (1.0 - oR.a) + oR.a * tR * uGlassT;
    vec3 tr = trF * trR;
    float cl = 1.0 - 0.14 * uCloud * clouds(w, uTime);
    vec3 sunlit = mix(uSky, vec3(1.0), (1.0 - uShadow) + uShadow * tr) * cl;
    // a Tetris: a burst of sun over the well
    vec2 fq = (w - uFlare.xy) / max(uFlare.z, 0.01);
    sunlit += vec3(1.0, 0.96, 0.86) * uFlare.w * exp(-dot(fq, fq));
    vec2 v = vUv * 2.0 - 1.0;
    c *= sunlit * uBright * (1.0 - uVig * dot(v * vec2(0.7, 1.0), v * vec2(0.7, 1.0)));
    frag = vec4(c, 0.0);
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
    // slow motion: the sun behind a cloud, a cool, quiet afternoon
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c, 0.55) * vec3(0.82, 0.88, 1.0), uSlow);
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
// Lettering on the sheet: ink with a soft halo of the paper around it.
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

// Builds a fragment shader from its source with the common functions inserted after the version line.
inline std::string withCommon(const char* fs, bool shape) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace sunprint_shaders

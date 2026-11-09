#pragma once
// HEAT's shaders (see heat.cpp).
//
// The frame is one thermal image: BG_FS and CELL_FS write a temperature (r, 0 cold .. 1 white hot) and the stack's
// coverage (g) into a float target, MAP_FS then maps it through the camera's colormap (a 256 x 2 lookup texture:
// the variant's palette and the drop's), and the overlay (the camera's lettering and scale bar) is
// drawn on top in display colors.

namespace heat_shaders {

// ---------------------------------------------------------------- the room (temperatures)
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uAir, uHeat;
uniform vec4 uAirGrid;   // x0, y0, 1 / spacing
uniform vec4 uView;      // center, half extents (board units)
uniform vec2 uRes;
uniform float uTime, uWall, uAirGain, uShimmer, uGrain, uWallHeat, uVig, uWell;
uniform vec4 uBlob[4];   // x, y, rx, ry
uniform vec4 uBlobT;     // their temperatures
uniform float uSeed;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
vec4 cubicW(float v) {
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x, y = s.y - 4.0 * s.x, z = s.z - 4.0 * s.y + 6.0 * s.x;
    return vec4(x, y, z, 6.0 - x - y - z) * (1.0 / 6.0);
}
// B-spline sampling of the air (smooth between the coarse grid's samples).
float airAt(vec2 w) {
    vec2 tc = (w - uAirGrid.xy) * uAirGrid.z + 0.5;
    vec2 size = vec2(textureSize(uAir, 0));
    tc -= 0.5;
    vec2 f = fract(tc);
    tc -= f;
    vec4 xc = cubicW(f.x), yc = cubicW(f.y);
    vec4 c = tc.xxyy + vec2(-0.5, 1.5).xyxy;
    vec4 s = vec4(xc.xz + xc.yw, yc.xz + yc.yw);
    vec4 o = (c + vec4(xc.yw, yc.yw) / s) / size.xxyy;
    float s0 = texture(uAir, o.xz).r, s1 = texture(uAir, o.yz).r, s2 = texture(uAir, o.xw).r, s3 = texture(uAir, o.yw).r;
    float sx = s.x / (s.x + s.y), sy = s.z / (s.z + s.w);
    return mix(mix(s3, s2, sx), mix(s1, s0, sx), sy);
}
// the heat of the well's walls and floor (columns 0 and 11, row 22 of the heat texture)
float wallHeat(float col, float y) {
    float row = clamp(21.0 - (y + 9.5), 2.0, 21.0);
    return texture(uHeat, vec2((col + 0.5) / 12.0, (row + 0.5) / 23.0)).r;
}

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float px = 2.0 * uView.w / uRes.y;
    // convection shimmer: the air bends the view behind it where it is hot (slow, flowing up)
    float a0 = airAt(w) * 4.0;
    vec2 sh = vec2(sin(w.y * 2.1 - uTime * 1.7 + 1.3 * sin(w.x * 0.9 + uTime * 0.3)),
                   sin(w.x * 1.7 + uTime * 0.6)) * vec2(0.09, 0.03) * uShimmer * (1.0 - exp(-a0 * 1.5));
    vec2 ws = w + sh;
    // the wall: cold, its own slow temperature variations (very low contrast)
    float n = vnoise(ws * 0.09 + vec2(0.0, uTime * 0.004)) * 0.6 + vnoise(ws * 0.23 - vec2(uTime * 0.003, 0.0)) * 0.4;
    float d = uWall + 0.028 * (n - 0.5) + 0.018 * smoothstep(-12.0, 12.0, ws.y);
    // the floor: a little warmer, a soft horizon below the well
    d += 0.025 * smoothstep(-10.6, -12.8, ws.y);
    // far shapes: soft warm forms
    float tb[4] = float[4](uBlobT.x, uBlobT.y, uBlobT.z, uBlobT.w);
    for (int i = 0; i < 4; i++) {
        vec2 q = (ws - uBlob[i].xy) / max(uBlob[i].zw, vec2(0.01));
        float r2 = dot(q, q);
        d += tb[i] * (exp(-r2 * 1.6) * 0.75 + exp(-r2 * 6.0) * 0.35);
    }
    // the well: a pane a shade colder than the wall, its walls and floor thin frames warmed by the stack
    vec2 qw = abs(w - vec2(0.0, 0.0)) - vec2(5.0, 10.0);
    float inWell = (1.0 - smoothstep(-0.3, 0.3, max(qw.x, qw.y))) * uWell;
    d -= 0.012 * inWell;
    float fx = abs(abs(w.x) - 5.13) - 0.07;
    float wallMask = (1.0 - smoothstep(-px, px, fx)) * (1.0 - smoothstep(-px, px, w.y - 10.0)) * smoothstep(-10.2 - px, -10.2 + px, w.y);
    float fy = abs(w.y + 10.13) - 0.07;
    float floorMask = (1.0 - smoothstep(-px, px, fy)) * (1.0 - smoothstep(-px, px, abs(w.x) - 5.2));
    float wh = wallHeat(w.x < 0.0 ? 0.0 : 11.0, w.y);
    float fh = texture(uHeat, vec2((clamp(w.x + 5.0, 0.0, 9.999) + 1.0) / 12.0, 22.5 / 23.0)).r;
    float frame = max(wallMask, floorMask) * uWell;
    float fheat = floorMask > wallMask ? fh : wh;
    d = mix(d, 0.085 + uWallHeat * (1.0 - exp(-fheat * 1.8)), frame);
    // the air, hot over the stack
    float a = airAt(ws);
    d += uAirGain * (1.0 - exp(-a * 5.0));
    // the lens: colder corners, a fine still grain
    vec2 c = vUv * 2.0 - 1.0;
    d *= 1.0 - uVig * dot(c * vec2(0.8, 1.0), c * vec2(0.8, 1.0));
    d += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * uGrain;
    frag = vec4(d, 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- the pieces (temperatures)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // game x, game y (heat lookup; x < 0: use heat), heat, alpha
layout(location = 3) in vec4 aC; // base temperature, kind, shrink, pad
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
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
uniform sampler2D uHeat;
uniform float uRim, uGain;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the piece the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right): its own cells merge, a thin seam parts it from the others.
float shapeSdf(vec2 p, int m) {
    const float g = 0.035, r = 0.16, rc = 0.06;
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
    int kind = int(vC.y + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float sd = shapeSdf(vLocal, int(vA.w + 0.5)) + vC.z;
    if (kind == 1) {
        // the ghost: a cold outline, the piece's imprint a shade colder than the room
        float line = 1.0 - smoothstep(0.6 * aa, 1.6 * aa, abs(sd + 0.05));
        vec2 own = abs(vLocal) - 0.5;
        float inside = (1.0 - smoothstep(-aa, aa, sd)) * step(max(own.x, own.y), 0.0);
        float a = max(line, inside * 0.55) * vB.w;
        float d = mix(0.0, vC.x, line);
        frag = vec4(d * a, a, 0.0, a);
        return;
    }
    // the heat: from the stack's heat map (smooth across the piece's cells) or given
    float h = vB.z;
    if (vB.x >= 0.0) {
        vec2 tc = vec2(vB.x + 1.5 + vLocal.x, vB.y + 0.5 - vLocal.y);
        h = texture(uHeat, tc / vec2(12.0, 23.0)).r;
    }
    float base = vC.x;
    // the piece radiates through its outline: a cooler rim, a warmer core
    float depth = clamp(-sd, 0.0, 0.5);
    float rim = 1.0 - smoothstep(0.0, 0.22, depth);
    float hv = h * (1.0 - 0.45 * uRim * rim);
    float d = base * (1.0 - 0.35 * uRim * rim) + (1.02 - base) * min(1.0, 0.88 * hv * uGain);
    d += 0.025 * smoothstep(0.15, 0.5, depth) * (0.4 + h);
    float a = (1.0 - smoothstep(-aa, aa * 0.5, sd)) * vB.w;
    frag = vec4(d * a, a, 0.0, a);
}
)";

// ---------------------------------------------------------------- the camera: temperature -> colors
inline const char* MAP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTemp, uLut, uGlow;
uniform float uGlowAmt;
uniform float uLo, uHi, uMix, uIso, uIsoThr, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk; // display colors
vec3 lut(float t, float row) { return texture(uLut, vec2(t * (255.0 / 256.0) + 0.5 / 256.0, row)).rgb; }
void main() {
    vec4 T = texture(uTemp, vUv);
    // the halo around hot things (the lens and the warmed air), hotter pieces glowing farther
    vec4 G = texture(uGlow, vUv);
    float gl = G.r * (0.3 + 0.7 * G.r) * uGlowAmt * (1.0 - 0.8 * clamp(T.g, 0.0, 1.0));
    T.r += gl;
    float t = clamp((T.r - uLo) / max(uHi - uLo, 1e-3), 0.0, 1.0);
    vec3 c = mix(lut(t, 0.25), lut(t, 0.75), uMix);
    // slow motion: the isotherm mode, everything grey but the hottest band
    float g = dot(c, vec3(0.299, 0.587, 0.114));
    vec3 grey = vec3(pow(t, 0.9) * 0.82 + 0.02);
    float keep = smoothstep(uIsoThr - 0.03, uIsoThr + 0.03, t);
    c = mix(c, mix(grey, c, keep), uIso);
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(T.g, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the halo (a separable blur)
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

// ---------------------------------------------------------------- the overlay
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
// The camera's lettering: light characters with a soft dark outline, readable over any temperature.
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
    float halo = smoothstep(0.22, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

} // namespace heat_shaders

#pragma once
// SMOKE's shaders (see smoke.cpp). Everything is drawn in linear HDR (the beam's core and the rims go above 1 and
// bloom), then tone-mapped in the post.
//
// SMOKE_FS ray-marches the smoke at a reduced resolution: a slab of curling smoke behind the well (a tileable 3D
// noise, domain-warped and swirled), lit by one hard spotlight (a cone with a crisp edge), with self-shadowing toward
// the lamp (the shafts inside the beam) and the well's own shadow. COMPOSITE_FS brings it to full resolution over
// the dark stage and draws the well, a slab of smoked black glass with a hairline catching the light.

namespace smoke_shaders {

// Shared helpers, inserted after each fragment shader's #version line.
inline const char* COMMON_GLSL = R"(
float hash11(float p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdRound(vec2 p, vec2 b, float r) { vec2 d = abs(p) - b + r; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r; }
// The spotlight: a cone from uLPos along uLAxis, tan(half angle) uCone; a crisp edge with a slightly hot ring
// (a Fresnel lens), a gentle falloff with distance.
uniform vec3 uLPos, uLAxis;
uniform float uCone;
float spot(vec3 p, float soft) {
    vec3 v = p - uLPos;
    float a = dot(v, uLAxis);
    if (a <= 0.5) return 0.0;
    float r = length(v - uLAxis * a) / (a * uCone);
    float edge = 1.0 - smoothstep(1.0 - 0.07 - soft, 1.0 + soft, r);
    float prof = mix(1.0, 0.7, r * r) + 0.22 * exp(-pow((r - 0.9) / 0.07, 2.0));
    return edge * prof / (1.0 + a * a * 0.0004);
}
)";

// ---------------------------------------------------------------- the smoke (reduced resolution)

inline const char* SMOKE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler3D uVol;       // tileable: r fbm, g curls (ridged), b a warp field
uniform vec4 uView;           // center xy, half extents (board units)
uniform vec3 uLCol, uFill;
uniform float uGain;
uniform float uHaze, uSmoke, uWarp, uCov, uExt, uShadowK;
uniform vec3 uFlow;
uniform float uSwirl;
uniform vec4 uWell;           // the well's flag (board x0 y0 x1 y1, at z = 0)
uniform float uBlast, uBlastA;
uniform float uThick;
uniform float uSeed;
uniform float uSpill;         // a drop: billows drifting out of the beam keep some of its light

float dens(vec3 p) {
    vec3 q = p * vec3(0.022, 0.017, 0.022) + uFlow;
    vec3 w = textureLod(uVol, q * 0.5 + vec3(0.31, 0.17, 0.63) + uFlow.zxy * 0.3, 0.0).gba - 0.5;
    float c = cos(uSwirl + p.y * 0.05), s = sin(uSwirl + p.y * 0.05);
    w.xz = mat2(c, s, -s, c) * w.xz;
    q += w * uWarp;
    vec4 n = textureLod(uVol, q, 0.0);
    vec4 m = textureLod(uVol, q * 2.7 + vec3(0.41, 0.07, 0.23), 0.0);
    // billows (the fbm over a threshold), carved by thin curling sheets (the ridges of a finer noise)
    float bill = smoothstep(uCov, uCov + 0.22, n.r + (m.r - 0.5) * 0.25);
    float sheet = 1.0 - abs(m.g * 2.0 - 1.0);
    sheet = sheet * sheet;
    sheet = sheet * sheet;
    float d = bill * (0.1 + 2.2 * sheet);
    d *= mix(1.25, 0.4, smoothstep(-14.0, 13.0, p.y)); // the smoke pools low and thins as it rises
    if (uBlastA > 0.001) {
        float r = length(p.xy * vec2(0.8, 0.6));
        d *= 1.0 - uBlastA * (1.0 - smoothstep(uBlast - 4.0, uBlast, r));
        d += uBlastA * 1.6 * exp(-pow((r - uBlast) / 1.6, 2.0)) * n.r;
    }
    return uHaze + d * uSmoke + uThick * (0.12 + 0.3 * n.r);
}

// Where an orthographic ray (from o, toward -z, length len) is inside the beam's cone: [t0, t1] (empty if t0 >= t1).
vec2 coneSpan(vec3 o, float len, float tanA) {
    vec3 V = vec3(0.0, 0.0, -1.0);
    float cos2 = 1.0 / (1.0 + tanA * tanA);
    vec3 co = o - uLPos;
    float vd = dot(V, uLAxis), cd = dot(co, uLAxis);
    float a = vd * vd - cos2, b = 2.0 * (vd * cd - cos2 * dot(V, co)), c = cd * cd - cos2 * dot(co, co);
    // the lamp's side of the double cone: h(t) = cd + t vd > 0
    float h0 = -1e9, h1 = 1e9;
    if (abs(vd) < 1e-6) { if (cd <= 0.0) return vec2(1.0, 0.0); }
    else if (vd > 0.0) h0 = -cd / vd;
    else h1 = -cd / vd;
    float disc = b * b - 4.0 * a * c;
    float t0, t1;
    if (abs(a) < 1e-7) {
        // one root
        if (abs(b) < 1e-7) return vec2(1.0, 0.0);
        float r = -c / b;
        if (b > 0.0) { t0 = r; t1 = 1e9; } else { t0 = -1e9; t1 = r; }
    } else if (disc < 0.0) {
        if (a < 0.0) return vec2(1.0, 0.0);
        t0 = -1e9; t1 = 1e9;
    } else {
        float sq = sqrt(disc);
        float r0 = (-b - sq) / (2.0 * a), r1 = (-b + sq) / (2.0 * a);
        if (r0 > r1) { float tmp = r0; r0 = r1; r1 = tmp; }
        if (a < 0.0) { t0 = r0; t1 = r1; }
        else {
            // inside outside the roots: keep the side on the lamp's nappe
            float hl = cd + (r0 - 1.0) * vd;
            if (hl > 0.0 && cd + (r1 + 1.0) * vd <= 0.0) { t0 = -1e9; t1 = r0; }
            else if (hl <= 0.0) { t0 = r1; t1 = 1e9; }
            else { t0 = -1e9; t1 = 1e9; }
        }
    }
    t0 = max(max(t0, h0), 0.0);
    t1 = min(min(t1, h1), len);
    return vec2(t0, t1);
}

void main() {
    vec2 xy = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    const float Z0 = 9.0, Z1 = -26.0;
    float j = hash12(gl_FragCoord.xy + uSeed);
    vec2 wc = (uWell.xy + uWell.zw) * 0.5, wh = (uWell.zw - uWell.xy) * 0.5;
    // the whole ray, coarsely: how much smoke stands in it (the far wall shows through the rest), lit by the fill
    const int NC = 8;
    float dzc = (Z0 - Z1) / float(NC);
    float Tall = 1.0;
    vec3 acc = vec3(0.0);
    for (int i = 0; i < NC; i++) {
        float d = dens(vec3(xy, Z0 - (float(i) + j) * dzc));
        float a = 1.0 - exp(-d * dzc * uExt);
        acc += Tall * a * uFill;
        Tall *= 1.0 - a;
    }
    // the beam: march only where the ray crosses the cone (an exact span, so the edge stays crisp)
    vec2 span = coneSpan(vec3(xy, Z0), Z0 - Z1, uCone * (1.04 + 0.6 * uSpill));
    if (span.y > span.x) {
        const int N = 22;
        float dz = (span.y - span.x) / float(N);
        float T = 1.0;
        vec3 L = vec3(0.0);
        for (int i = 0; i < N; i++) {
            vec3 p = vec3(xy, Z0 - span.x - (float(i) + j) * dz);
            float d = dens(p);
            float s = spot(p, 0.02 + uThick * 0.6);
            if (uSpill > 0.001) {
                // a drop: just outside the cone, the thick billows (not the haze) still hold some of the light,
                // fading with the distance from its edge: smoke curling out of the beam
                vec3 v = p - uLPos;
                float ax = dot(v, uLAxis);
                float r = ax > 0.5 ? length(v - uLAxis * ax) / (ax * uCone) : 9.0;
                float halo = smoothstep(0.95, 1.05, r) * (1.0 - smoothstep(1.05, 1.6, r));
                float wisp = smoothstep(0.6, 1.6, (d - uHaze) / max(uSmoke, 1e-4));
                s += uSpill * halo * wisp;
            }
            vec3 toL = normalize(uLPos - p);
            float od = dens(p + toL * 1.3) * 1.3 + dens(p + toL * 3.6) * 2.3 + dens(p + toL * 8.0) * 4.4;
            float sh = exp(-od * uShadowK);
            // the well, a black flag standing in the light, shades the smoke behind it
            if (p.z < 0.0) {
                float t = -p.z / max(uLPos.z - p.z, 1e-3);
                vec2 q = p.xy + (uLPos.xy - p.xy) * t;
                sh *= mix(0.05, 1.0, smoothstep(-0.2, 0.5, sdBox(q - wc, wh)));
            }
            float a = 1.0 - exp(-d * dz * uExt);
            L += T * a * s * sh;
            T *= 1.0 - a;
        }
        acc += uLCol * uGain * L * mix(1.0, Tall, 0.5);
    }
    frag = vec4(acc, Tall);
}
)";

// ---------------------------------------------------------------- the stage at full resolution

inline const char* COMPOSITE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSmokeT, uNoise;
uniform vec2 uSmokeTexel;
uniform vec4 uView;
uniform float uPx, uTime;
uniform vec4 uWell;
uniform vec3 uPanel, uRim, uFill, uLCol, uWallCol;
uniform float uGain, uWall, uPanelAlpha;
uniform float uOverFog;
uniform vec3 uMist;
uniform float uBridge;
uniform vec3 uBridgeCol;
uniform float uWorld;

void main() {
    vec2 uv = vUv;
    vec2 p = uView.xy + (uv * 2.0 - 1.0) * uView.zw;
    // the smoke, upsampled softly (a small tent over the reduced buffer)
    vec4 sm = texture(uSmokeT, uv) * 0.4;
    sm += texture(uSmokeT, uv + uSmokeTexel * vec2(0.7, 0.4)) * 0.15;
    sm += texture(uSmokeT, uv + uSmokeTexel * vec2(-0.4, 0.7)) * 0.15;
    sm += texture(uSmokeT, uv + uSmokeTexel * vec2(-0.7, -0.4)) * 0.15;
    sm += texture(uSmokeT, uv + uSmokeTexel * vec2(0.4, -0.7)) * 0.15;
    // the far wall of the stage: black, with the pool of the beam where it lands
    vec3 back = uFill * 0.15 + uWallCol * uLCol * uGain * uWall * spot(vec3(p, -24.0), 0.35);
    vec3 col = back * sm.a + sm.rgb;

    // the well: a slab of smoked black glass, the beam grazing it, a hairline catching the light
    vec2 wc = (uWell.xy + uWell.zw) * 0.5, wh = (uWell.zw - uWell.xy) * 0.5;
    float sW = sdRound(p - wc, wh, 0.06);
    float inW = 1.0 - smoothstep(-uPx, uPx, sW);
    if (inW > 0.0) {
        float lit = spot(vec3(p, 0.2), 0.25);
        vec3 glass = uPanel + uLCol * uGain * lit * 0.008 + sm.rgb * 0.035;
        col = mix(col, glass, inW * uPanelAlpha);
    }
    {
        vec2 q = abs(p - wc) - wh;
        vec2 n = q.x > q.y ? vec2(sign(p.x - wc.x), 0.0) : vec2(0.0, sign(p.y - wc.y));
        vec3 toL = uLPos - vec3(p, 0.0);
        float facing = max(dot(n, normalize(toL.xy)), 0.0);
        float lit = spot(vec3(p, 0.3), 0.5);
        float line = exp(-abs(sW) / (uPx * 0.9));
        col += uRim * line * (0.05 + (0.25 + 1.1 * lit) * facing) * uWorld;
    }

    // game over: the smoke rises from below and swallows the stage
    if (uOverFog > 0.001) {
        float n = texture(uNoise, p * 0.04 + vec2(uTime * 0.008, -uTime * 0.025)).r * 0.6
                + texture(uNoise, p * 0.11 + vec2(-uTime * 0.015, -uTime * 0.04)).g * 0.4;
        float top = mix(uView.y - uView.w - 3.0, uView.y + uView.w * 0.9, uOverFog);
        float m = smoothstep(top + 3.0, top - 3.0, p.y + (n - 0.5) * 7.0);
        col = mix(col, uMist * (0.7 + 0.6 * n), m * 0.94);
    }
    // the bridge (a change of universe): the smoke thickens until it fills the frame
    if (uBridge > 0.0) {
        float n = texture(uNoise, p * 0.03 + vec2(uTime * 0.01, uTime * 0.02)).r * 0.55
                + texture(uNoise, p * 0.08 + vec2(-uTime * 0.02, uTime * 0.01)).g * 0.45;
        float w = smoothstep(0.0, 1.0, uBridge * 2.0 - 0.5 + (n - 0.5) * 1.0);
        w = max(w, smoothstep(0.88, 0.99, uBridge));
        col = mix(col, uBridgeCol, w);
    }
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- flat triangles (refall-scene's panels)

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

// ---------------------------------------------------------------- tiles: the stack, the piece, previews

// a0: center xy, half size xy (board). a1: rgb, alpha. a2: kind, age, angle, unused. a3: extra.
inline const char* TILE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 a0;
layout(location = 2) in vec4 a1;
layout(location = 3) in vec4 a2;
layout(location = 4) in vec4 a3;
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 v0, v1, v2, v3;
void main() {
    vec2 l = aCorner * a0.zw;
    float c = cos(a2.z), s = sin(a2.z);
    vec2 p = a0.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vLocal = l;
    vWorld = p;
    v0 = a0; v1 = a1; v2 = a2; v3 = a3;
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
inline const char* TILE_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 v0, v1, v2, v3;
out vec4 frag;
uniform sampler2D uNoise;
uniform float uPx, uBridge;
uniform vec3 uTint, uKey, uFill, uBridgeInk;
uniform float uKeyGain, uSlow;

vec2 boxGrad(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    vec2 g = (q.x > 0.0 && q.y > 0.0) ? normalize(q) : (q.x > q.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    return g * sign(p + 1e-6);
}

// A satin block caught in the key light: a hard light from the lamp's side models it like a small object on a set
// (the face falls off from the lit corner to the far one, a broad soft sheen near the lit corner), a crisp narrow
// bevel catches the lamp on the edges facing it and the far edges fall into shadow. The colors stay true (the light's
// tint is only a touch).
vec3 satin(vec2 p, float s, vec3 pc, vec2 wpos, float lightK) {
    vec2 b = vec2(0.45 * s);
    float rad = 0.06 * s, bev = 0.075 * s;
    float sd = sdRound(p, b, rad);
    float inset = clamp(-sd / bev, 0.0, 1.0);
    vec2 g = boxGrad(p, b, rad);
    vec3 toL = uLPos - vec3(wpos, 0.5);
    vec2 l2 = normalize(toL.xy);
    float key = clamp(spot(vec3(wpos, 0.4), 0.9), 0.0, 1.0);
    float k = mix(0.7, 1.0, key) * lightK;
    // the face: lit from the lamp's corner
    float across = dot(p / b.x, l2);                 // -1.4 far corner .. 1.4 lit corner
    float face = 0.8 + 0.2 * across;
    float sheen = exp(-dot(p / b.x - l2 * 0.75, p / b.x - l2 * 0.75) * 1.4) * 0.22;
    // the bevel: toward the lamp lit, away from it in shadow
    float facing = dot(g, l2);
    float bevel = 1.0 - inset;
    vec3 base = pc * uTint;
    vec3 col = base * mix(face, 0.75 + 0.8 * facing, bevel * 0.9) * k;
    col += uKey * (sheen * (1.0 - bevel) + bevel * max(facing, 0.0) * 0.28) * k * uKeyGain;
    col += base * uFill * 2.0;
    return col;
}

// ---- The alternative block looks (uBlock 1..6, REFALL_SMOKE_BLOCK). A block's links to the cells of its piece
// (bits L R U D UL UR DL DR), its place in the piece and the piece's seed come with it, so the look of a piece is
// continuous across its cells and never changes once it locks or rows fall.
uniform int uBlock;
uniform float uTime, uConeTrue, uLampOn;
uniform vec3 uEye;             // the reference camera (board xy, distance): the glass cubes' perspective
uniform vec4 uView;            // the view's center and half extents (the smoke buffer's mapping)
uniform sampler2D uSmokeT;     // the smoke, as the stage shows it behind the well
uniform sampler2D uOcc;        // the stack's heights (style 1)
uniform sampler3D uVol;        // the smoke's noise volume
uniform vec3 uFall[4];         // the falling piece's cells (board xy, height): style 1
uniform int uFallN;

bool bit(int m, int b) { return (m & b) != 0; }

// The distance from p (cell-local, -0.5..0.5) to its piece's outline, positive inside: the sides with no link are the
// outline (their corners rounded by r), a corner whose two sides link but not its diagonal a notch.
float pieceDist(vec2 p, int m, float r) {
    float dl = bit(m, 1) ? 9.0 : p.x + 0.5;
    float dr = bit(m, 2) ? 9.0 : 0.5 - p.x;
    float du = bit(m, 4) ? 9.0 : 0.5 - p.y;
    float dd = bit(m, 8) ? 9.0 : p.y + 0.5;
    float d = min(min(dl, dr), min(du, dd));
    if (dl < r && du < r) d = min(d, r - length(vec2(r - dl, r - du)));
    if (dr < r && du < r) d = min(d, r - length(vec2(r - dr, r - du)));
    if (dl < r && dd < r) d = min(d, r - length(vec2(r - dl, r - dd)));
    if (dr < r && dd < r) d = min(d, r - length(vec2(r - dr, r - dd)));
    if (bit(m, 1) && bit(m, 4) && !bit(m, 16)) d = min(d, length(p - vec2(-0.5, 0.5)));
    if (bit(m, 2) && bit(m, 4) && !bit(m, 32)) d = min(d, length(p - vec2(0.5, 0.5)));
    if (bit(m, 1) && bit(m, 8) && !bit(m, 64)) d = min(d, length(p - vec2(-0.5, -0.5)));
    if (bit(m, 2) && bit(m, 8) && !bit(m, 128)) d = min(d, length(p - vec2(0.5, -0.5)));
    return d;
}
// toward the inside of the piece (the gradient of pieceDist)
vec2 pieceIn(vec2 p, int m, float r) {
    const float e = 0.01;
    vec2 g = vec2(pieceDist(p + vec2(e, 0.0), m, r) - pieceDist(p - vec2(e, 0.0), m, r),
                  pieceDist(p + vec2(0.0, e), m, r) - pieceDist(p - vec2(0.0, e), m, r));
    return g / max(length(g), 1e-4);
}
// the spotlight with a given cone (the real one, with its hard edge)
float spotC(vec3 p, float soft, float cone) {
    vec3 v = p - uLPos;
    float a = dot(v, uLAxis);
    if (a <= 0.5) return 0.0;
    float r = length(v - uLAxis * a) / (a * cone);
    float edge = 1.0 - smoothstep(1.0 - soft, 1.0 + soft, r);
    return edge * mix(1.0, 0.75, r * r) / (1.0 + a * a * 0.0004);
}
vec3 vivid(vec3 c) { return c / max(max(c.r, max(c.g, c.b)), 1e-3); }
// a gel's color, deeper (the pale ones keep a hue)
vec3 gel(vec3 c) {
    c = vivid(c);
    float l = dot(c, vec3(0.333));
    return clamp(mix(vec3(l), c, 1.7), 0.0, 1.0) / max(max(c.r, max(c.g, c.b)), 1e-3);
}

// Style 1: is the point P (board, z up toward the viewer) in the shadow of a prop between it and the lamp?
float propShadow(vec3 P) {
    vec2 d2 = uLPos.xy - P.xy;
    float dist = max(length(d2), 1e-3);
    vec2 dir = d2 / dist;
    float slope = (uLPos.z - P.z) / dist;
    float lit = 1.0;
    for (int i = 1; i <= 44; i++) {
        float t = float(i) * 0.075;
        vec2 q = P.xy + dir * t;
        float z = P.z + slope * t;
        if (z > 0.8) break;
        ivec2 c = ivec2(floor(q.x + 5.0), floor(12.0 - q.y));
        float hc = 0.0;
        if (c.x >= 0 && c.x < 10 && c.y >= 0 && c.y < 22) hc = texelFetch(uOcc, c, 0).r;
        for (int k = 0; k < uFallN; k++) {
            vec2 a = abs(q - uFall[k].xy);
            if (max(a.x, a.y) < 0.5) hc = max(hc, uFall[k].z);
        }
        lit = min(lit, smoothstep(-0.015, 0.015, z - hc));
    }
    return lit;
}

// A block in one of the looks: q cell-local (-0.5..0.5), s its scale, px a pixel in q's units, wpos the point (board),
// cc the cell's center, m its links, rel its place in its piece, ps the piece's seed. Returns premultiplied color and coverage.
vec4 blockLook(vec2 q, float s, float px, vec3 pc, vec2 wpos, vec2 cc, float lightK, int m, vec2 rel, float ps, bool board) {
    vec3 tint = pc * uTint;
    float on = uKeyGain * lightK;
    if (uBlock == 1) {
        // PROPS: matte painted props standing in the hard spotlight. Each piece its own height (from its seed): the
        // lamp's cone lights them with a crisp edge and a strong falloff, and the taller ones throw hard shadows over
        // their neighbours (and on the glass, drawn under them).
        float D = pieceDist(q, m, 0.1);
        const float gap = 0.035;
        float cov = smoothstep(gap - px, gap + px, D);
        if (cov <= 0.0) return vec4(0.0);
        float h = floor((0.2 + 0.6 * ps) * 255.0) / 255.0;
        float e = 1.0 - clamp((D - gap) / 0.11, 0.0, 1.0);
        vec2 gin = pieceIn(q, m, 0.1);
        vec3 n = normalize(vec3(-gin * e * e * 2.6, 1.0));
        vec3 P = vec3(wpos, h * (1.0 - 0.6 * e * e));
        vec3 L = normalize(uLPos - P);
        float lam = max(dot(n, L), 0.0);
        // the hard cone, and a dim spill of the lens round it
        float sp = max(spotC(vec3(wpos, h), 0.025, uConeTrue), board ? 0.0 : 0.75);
        sp = max(sp, 0.32 * spotC(vec3(wpos, h), 0.6, uConeTrue * 2.2));
        float sh = board ? propShadow(vec3(wpos, h)) : 1.0;
        float key = sp * (0.18 + 0.82 * sh) * on;
        // a matte paint, faintly mottled (in the piece's own space)
        float grain = texture(uNoise, (vec2(rel.x, -rel.y) + q) * 0.21 + ps * 7.3).r;
        vec3 base = tint * (0.9 + 0.2 * grain);
        vec3 col = base * (0.12 + 0.05 * n.z) * (0.4 + 0.6 * lightK) + uFill * base * 2.0;
        col += base * uKey * key * lam * 2.1;
        col += uKey * key * pow(max(dot(reflect(-L, n), vec3(0.0, 0.0, 1.0)), 0.0), 24.0) * 0.2;
        // the gap between props: a dark crease
        col *= mix(1.0, 0.55, smoothstep(0.03, 0.0, D - gap));
        return vec4(col * cov, cov);
    }
    if (uBlock == 2) {
        // ACRYLIC: tinted cast acrylic, one slab per piece. The smoke behind the well shows through it (bent a little
        // at its edges), the beam lights it from inside where it crosses, its polished rim catches the lamp.
        float D = pieceDist(q, m, 0.12);
        const float gap = 0.04;
        float cov = smoothstep(gap - px, gap + px, D);
        if (cov <= 0.0) return vec4(0.0);
        vec2 gin = pieceIn(q, m, 0.12);
        float edge = exp(-(D - gap) / 0.07);
        vec2 suv = (wpos - uView.xy) / uView.zw * 0.5 + 0.5 + gin * edge * 0.006;
        vec3 sm = texture(uSmokeT, suv).rgb;
        float beam = max(spotC(vec3(wpos, 0.3), 0.04, uConeTrue), board ? 0.0 : 0.6) * on;
        vec3 l2 = normalize(uLPos - vec3(wpos, 0.0));
        float facing = max(dot(-gin, l2.xy), 0.0);
        vec3 col = tint * (0.17 + 0.12 * edge) + sm * vivid(tint) * 2.2;
        col += tint * beam * (0.24 + 1.0 * edge);
        float rim = exp(-pow((D - gap - 0.016) / 0.011, 2.0));
        col += mix(tint, vec3(1.0), 0.55) * rim * (0.26 + (0.5 + 2.4 * beam) * facing) * (0.5 + 0.5 * lightK);
        float bev = exp(-pow((D - gap - 0.085) / 0.012, 2.0));
        col += tint * bev * (0.06 + 0.5 * beam * (1.0 - facing));
        // the lamp's reflection, a soft streak across the face
        vec2 u = vec2(rel.x, -rel.y) + q;
        float streak = exp(-pow(dot(u, normalize(vec2(1.0, 1.0))) - 0.2 - fract(ps * 3.7) * 1.2, 2.0) * 8.0);
        col += uKey * streak * (0.02 + 0.08 * beam);
        return vec4(col * cov, cov);
    }
    if (uBlock == 3) {
        // NEON: each piece outlined by one glowing tube in its gel, on a dark backplate, its halo bleeding into the
        // haze; small clips hold the tube along each side.
        float D = pieceDist(q, m, 0.16);
        const float gap = 0.04, inset = 0.13;
        float cov = smoothstep(gap - px, gap + px, D);
        float x = D - inset;
        float core = exp(-x * x / (0.014 * 0.014));
        float tube = exp(-x * x / (0.03 * 0.03));
        float halo = exp(-abs(x) / 0.09);
        vec3 c = gel(pc) * mix(vec3(1.0), uTint, 0.5);
        float lit = (0.08 + 0.92 * lightK) * (0.92 + 0.16 * ps);
        // the clips at each free side's middle
        float clip = 0.0;
        if (!bit(m, 1)) clip = max(clip, step(abs(q.y), 0.035) * step(abs(q.x + 0.5 - inset), 0.05));
        if (!bit(m, 2)) clip = max(clip, step(abs(q.y), 0.035) * step(abs(q.x - 0.5 + inset), 0.05));
        if (!bit(m, 4)) clip = max(clip, step(abs(q.x), 0.035) * step(abs(q.y - 0.5 + inset), 0.05));
        if (!bit(m, 8)) clip = max(clip, step(abs(q.x), 0.035) * step(abs(q.y + 0.5 - inset), 0.05));
        vec3 glow = (c * tube * 0.85 + mix(c, vec3(1.0), 0.6) * core * 1.05) * (1.0 - 0.8 * clip) + c * halo * 0.16;
        glow *= lit;
        // the backplate: near black, the tube's light on it
        float beam = max(spotC(vec3(wpos, 0.3), 0.1, uConeTrue), board ? 0.0 : 0.6) * on;
        vec3 plate = c * 0.002 + uKey * beam * 0.006 + c * exp(-max(x, 0.0) / 0.1) * 0.035 * lit;
        plate += vec3(0.03) * clip;
        vec3 col = mix(glow, plate + glow, cov);
        return vec4(col, cov);
    }
    if (uBlock == 4) {
        // SMOKE BOXES: each piece a thin-framed box of dense smoke of its color, curling slowly inside (its own swirl
        // from its seed, in the piece's own space), lit by the beam where it crosses.
        float D = pieceDist(q, m, 0.08);
        const float gap = 0.03;
        float cov = smoothstep(gap - px, gap + px, D);
        if (cov <= 0.0) return vec4(0.0);
        vec2 u = vec2(rel.x, -rel.y) + q;
        float t = uTime * 0.035;
        vec3 sp3 = vec3(u * 0.14 + ps * 17.0, t + ps * 3.1);
        vec3 w = textureLod(uVol, sp3 * 0.8 + vec3(0.31, 0.17, 0.63), 0.0).gba - 0.5;
        float a = t * 2.0 + ps * 6.28;
        w.xy = mat2(cos(a), sin(a), -sin(a), cos(a)) * w.xy;
        vec4 n = textureLod(uVol, sp3 + w * 0.35, 0.0);
)"
R"(        vec4 n2 = textureLod(uVol, sp3 * 2.2 + w * 0.6 + vec3(0.4, 0.1, 0.2), 0.0);
        float dens = smoothstep(0.3, 0.75, n.r * 0.75 + n2.r * 0.25 + (n2.g - 0.5) * 0.12) * 1.1 + 0.08;
        float inner = smoothstep(gap, gap + 0.22, D);
        float beam = max(spotC(vec3(wpos, 0.3), 0.08, uConeTrue), board ? 0.0 : 0.6) * on;
        vec3 smokeC = tint * (0.03 + (0.34 + 0.55 * beam) * dens * (0.4 + 0.6 * inner)) + uKey * beam * dens * 0.1;
        smokeC *= 0.35 + 0.65 * lightK;
        // the frame: a thin dark metal edge, its side toward the lamp catching it
        vec2 gin = pieceIn(q, m, 0.08);
        vec3 l2 = normalize(uLPos - vec3(wpos, 0.0));
        float facing = max(dot(-gin, l2.xy), 0.0);
        float fr = smoothstep(gap + 0.035 + px, gap + 0.035 - px, D);
        float hl = exp(-pow((D - gap - 0.012) / 0.008, 2.0));
        vec3 frame = vec3(0.006) + tint * 0.02 + mix(tint, uKey, 0.5) * hl * (0.12 + (0.3 + 1.0 * beam) * facing) * (0.4 + 0.6 * lightK);
        vec3 col = mix(smokeC, frame, fr);
        // the glass front: a faint streak
        col += uKey * 0.015 * exp(-pow(dot(u, vec2(0.7, 0.7)) - 0.5 - fract(ps * 5.3), 2.0) * 10.0);
        return vec4(col * cov, cov);
    }
    if (uBlock == 5) {
        // LENSES: the stack a lighting rig. Each piece one fixture bar in a dark housing, each cell a fresnel lens of
        // the piece's gel glowing from its lamp (the stepped rings, a hot core), in a bezel; screws at its corners.
        float D = pieceDist(q, m, 0.1);
        const float gap = 0.035;
        float cov = smoothstep(gap - px, gap + px, D);
        if (cov <= 0.0) return vec4(0.0);
        vec2 gin = pieceIn(q, m, 0.1);
        vec3 l2 = normalize(uLPos - vec3(wpos, 0.0));
        float facing = max(dot(-gin, l2.xy), 0.0);
        float beam = max(spotC(vec3(wpos, 0.3), 0.08, uConeTrue), board ? 0.0 : 0.6) * on;
        float hl = exp(-pow((D - gap - 0.012) / 0.01, 2.0));
        vec3 housing = vec3(0.008) * (1.0 + 2.0 * beam) + uFill * 0.15 + mix(vec3(0.6), uKey, 0.5) * hl * (0.05 + (0.15 + 0.6 * beam) * facing);
        float hn = texture(uNoise, (vec2(rel.x, -rel.y) + q) * 0.9 + ps * 3.1).b;
        housing *= 0.85 + 0.3 * hn; // a matte crackle paint
        vec3 c = gel(pc) * mix(vec3(1.0), uTint, 0.5);
        float lit = (0.06 + 0.94 * lightK) * (0.94 + 0.12 * fract(ps * 7.7 + rel.x * 0.31 + rel.y * 0.53));
        float r = length(q);
        const float R = 0.33;
        float lens = 1.0 - smoothstep(R - px, R + px, r);
        float k = r / R;
        float step5 = fract(k * 5.0);
        float rings = 0.7 + 0.3 * smoothstep(0.0, 0.9, step5) - 0.25 * smoothstep(0.9, 1.0, step5);
        float hot = exp(-r * r / (0.07 * 0.07));
        vec3 glass = c * (0.5 + 0.7 * (1.0 - k * k)) * rings * 0.95 + mix(c, vec3(1.0), 0.75) * hot * 1.5;
        glass *= lit;
        glass += c * 0.03 + uKey * beam * 0.04;
        // the bezel: a dark ring with a highlight on the lamp's side
        float bz = smoothstep(R - px, R + px, r) * (1.0 - smoothstep(R + 0.06 - px, R + 0.06 + px, r));
        float bzl = max(dot(normalize(q + 1e-4), l2.xy), 0.0);
        vec3 bezel = vec3(0.002) + mix(vec3(0.5), uKey, 0.5) * exp(-pow((r - R - 0.045) / 0.01, 2.0)) * (0.05 + 0.4 * bzl * (0.3 + beam));
        vec3 col = mix(housing, bezel, bz);
        col = mix(col, glass, lens);
        // the lens's light spills on the housing round it
        col += c * exp(-max(r - R, 0.0) / 0.05) * 0.08 * lit * (1.0 - lens);
        // screws in the corners
        vec2 sc = abs(q) - vec2(0.41);
        float screw = 1.0 - smoothstep(0.022 - px, 0.022 + px, length(sc));
        col = mix(col, vec3(0.03) + uKey * beam * 0.08, screw);
        return vec4(col * cov, cov);
    }
    // GLASS CUBES (6): every cell its own small glass cube seen in slight perspective from the camera (its far face
    // drawn toward the eye, the inner walls between), the glass faintly tinted, a bright glassy rim and a soft halo,
    // and inside a curl of the piece's smoke drifting slowly (each cube its own seed and phase).
    const float F = 0.47, B = 0.41;
    vec2 o = clamp((uEye.xy - cc) * 0.009, vec2(-0.07), vec2(0.07)) + vec2(0.0, -0.02);
    float sdF = sdRound(q, vec2(F), 0.05);
    float cov = 1.0 - smoothstep(-px, px, sdF);
    vec3 c = mix(pc * uTint, gel(pc), 0.3);
    float cs = fract(ps * 13.7 + rel.x * 0.317 + rel.y * 0.571);
    float beam = max(spotC(vec3(wpos, 0.4), 0.3, uConeTrue * 1.15), board ? 0.0 : 0.6) * on;
    float lit = 0.25 + 0.75 * lightK;
    // the halo round the cube
    vec3 halo = c * exp(-max(sdF, 0.0) / 0.035) * 0.26 * lit;
    if (cov <= 0.0) return vec4(halo, 0.0);
    // where the point lies between the front face (f 0) and the far one (f 1): the inner wall it sees
    float fxp = (F - q.x) / max((F - B) - o.x, 1e-3), fxn = (q.x + F) / max((F - B) + o.x, 1e-3);
    float fyp = (F - q.y) / max((F - B) - o.y, 1e-3), fyn = (q.y + F) / max((F - B) + o.y, 1e-3);
    float f = min(min(fxp, fxn), min(fyp, fyn));
    vec2 wn = f == fxp ? vec2(1.0, 0.0) : f == fxn ? vec2(-1.0, 0.0) : f == fyp ? vec2(0.0, 1.0) : vec2(0.0, -1.0);
    vec3 l3 = normalize(uLPos - vec3(wpos, 0.0));
    vec3 col;
    if (f < 1.0) {
        // an inner wall, lit by the lamp when it faces it (the walls face inward: -wn)
        float wl = max(dot(-wn, l3.xy), 0.0);
        col = c * (0.3 + 0.16 * wl + 0.3 * wl * beam) + c * 0.05 * f;
    } else {
        col = c * 0.17;
    }
    // inside: a curl of smoke, two layers at two depths (a little parallax), the densest in the middle
    float t = uTime * 0.045;
    float dens = 0.0;
    for (int i = 0; i < 2; i++) {
        float dz = 0.35 + 0.4 * float(i);
        vec2 u = (q - o * dz * 2.0) * 0.15;
        vec3 sp3 = vec3(u + cs * 9.1 + float(i) * 0.37, t * (1.0 + 0.3 * float(i)) + cs * 4.7);
        vec3 w = textureLod(uVol, sp3 * 0.9 + vec3(0.31, 0.17, 0.63), 0.0).gba - 0.5;
        float a = t * 1.7 + cs * 6.28 + float(i);
        w.xy = mat2(cos(a), sin(a), -sin(a), cos(a)) * w.xy;
        vec4 n = textureLod(uVol, sp3 + vec3(w.xy * 0.45, w.z * 0.2), 0.0);
        float sheet = 1.0 - abs(n.g * 2.0 - 1.0);
        dens += smoothstep(0.36, 0.62, n.r) * (0.15 + 1.3 * sheet * sheet) * (i == 0 ? 0.62 : 0.46);
    }
    dens *= smoothstep(0.56, 0.2, length(q - o * 0.5));
    vec3 wisp = mix(c, vec3(1.0), 0.38) * (1.0 + 0.5 * beam);
    col = mix(col, wisp, clamp(dens, 0.0, 1.0) * 0.85);
    // the edges: the front's bright rim (the side toward the lamp brighter), the far face's thinner one, the corner
    // edges joining them
    float rimF = exp(-abs(sdF + 0.008) / (px * 1.0 + 0.002));
    vec2 gq = abs(q).x > abs(q).y ? vec2(sign(q.x), 0.0) : vec2(0.0, sign(q.y));
    float rimL = 0.45 + 0.9 * max(dot(gq, l3.xy), 0.0);
    float sdB = sdRound(q - o, vec2(B), 0.03);
    float rimB = exp(-abs(sdB) / (px * 0.9 + 0.0015));
    float rimC = 0.0;
    for (int k = 0; k < 4; k++) {
        vec2 sg = vec2(k == 0 || k == 2 ? 1.0 : -1.0, k < 2 ? 1.0 : -1.0);
        vec2 a = sg * (F - 0.015), b = o + sg * B;
        vec2 pa = q - a, ba = b - a;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        rimC = max(rimC, exp(-length(pa - ba * h) / (px * 0.9 + 0.0015)));
    }
    // glints at the front's upper corners
    float glint = exp(-dot(abs(q) - vec2(F - 0.035), abs(q) - vec2(F - 0.035)) / 0.0009) * step(0.0, q.y);
    vec3 edgeC = mix(c, vec3(1.0), 0.4);
    col += edgeC * (rimF * rimL * (0.85 + 0.8 * beam) + (rimB * 0.4 + rimC * 0.45) * (0.6 + 0.5 * beam)) * lit;
    col += vec3(1.0) * glint * (0.5 + 0.6 * beam) * lit;
    // the front glass: a faint sheen across it
    col += vec3(1.0) * 0.03 * smoothstep(0.2, 0.0, abs(q.x + q.y - 0.25)) * lit;
    col *= 0.3 + 0.7 * lit;
    return vec4(col * cov + halo * (1.0 - cov), cov);
}

void main() {
    int kind = int(v2.x + 0.5);
    vec3 pc = v1.rgb;
    float alpha = v1.a;
    vec2 p = vLocal;
    float px = uPx;
    float bridgeK = smoothstep(0.2, 0.85, uBridge);
    // the block looks: v2.w the links and place in the piece (+ 4096 on the board), v3.w the piece's seed
    int pk = int(v2.w + 0.5);
    int links = pk & 255;
    vec2 rel = vec2(float((pk >> 8) & 3), float((pk >> 10) & 3));
    bool board = (pk & 4096) != 0;
    if (kind == 0 && uBlock > 0) {
        float s = v3.x;
        vec4 r = blockLook(p / s, s, px / s, pc, vWorld, v0.xy, v3.y, links, rel, v3.w, board);
        vec3 col = r.rgb;
        float age = v3.z;
        if (age >= 0.0) col += mix(pc, uKey, 0.5) * exp(-age * 5.0) * 0.45 * r.a;
        col = mix(col, uBridgeInk * r.a, bridgeK);
        frag = vec4(col * alpha, r.a * alpha);
        return;
    }
    if (kind == 3 && uBlock == 1) {
        // a prop's hard shadow on the glass: v3.x scale, v3.y its height, v3.zw its center. A point of the glass is in
        // it when its ray to the lamp enters the prop's column below its top.
        float s = v3.x, h = v3.y;
        vec2 q = vWorld, d = uLPos.xy - q;
        d = vec2(abs(d.x) < 1e-5 ? 1e-5 : d.x, abs(d.y) < 1e-5 ? 1e-5 : d.y);
        vec2 t0 = (v3.zw - 0.5 * s - q) / d, t1 = (v3.zw + 0.5 * s - q) / d;
        vec2 tn = min(t0, t1), tf = max(t0, t1);
        float te = max(max(tn.x, tn.y), 0.0), tx = min(tf.x, tf.y);
        float lz = max(uLPos.z, h + 0.5);
        float a = smoothstep(0.0, 0.004, tx - te) * smoothstep(-0.02, 0.02, h - te * lz);
        a *= step(abs(q.x), 5.2) * step(abs(q.y), 10.2);
        frag = vec4(vec3(0.0), a * alpha * 0.8 * (1.0 - uBridge));
        return;
    }
    if (kind == 1 && uBlock > 0) {
        // the ghost: the piece's outline as a hairline in the light (6: the cube, colorless, a faint gray smoke inside)
        float f = 1.0 - uBridge;
        if (uBlock == 6) {
            vec4 r = blockLook(p, 1.0, px, vec3(0.5), vWorld, v0.xy, 1.0, 0, rel, v3.w, false);
            frag = vec4(r.rgb, r.a) * 0.28 * alpha * f;
            return;
        }
        float D = pieceDist(p, links, 0.1);
        float line = (1.0 - smoothstep(0.0, px * 1.1, abs(D - 0.06))) * 0.4;
        float fill = smoothstep(0.06 - px, 0.06 + px, D) * 0.04;
        vec3 col = mix(pc, uKey, 0.25) * (line + fill);
        frag = vec4(col * alpha * f, (line * 0.8 + fill) * alpha * f);
        return;
    }
    if (kind == 0) {
        // a block: v3.x scale, v3.y light (1 lit, 0 dark), v3.z lock flash age (< 0 none)
        float s = v3.x;
        float sd = sdRound(p, vec2(0.45 * s), 0.07 * s);
        float cov = 1.0 - smoothstep(-px * 0.7, px * 0.7, sd);
        if (cov <= 0.0) discard;
        vec3 col = satin(p, s, pc, v0.xy, v3.y);
        float age = v3.z;
        if (age >= 0.0) col += mix(pc, uKey, 0.5) * exp(-age * 5.0) * 0.45;
        col = mix(col, uBridgeInk, bridgeK);
        float a = cov * alpha;
        frag = vec4(col * a, a);
        return;
    }
    if (kind == 3) {
        // the soft shadow a block casts on the glass, away from the lamp
        float s = v3.x;
        float sd = sdRound(p, vec2(0.45 * s), 0.1 * s);
        float a = (1.0 - smoothstep(-0.08 * s, 0.22 * s, sd)) * alpha * 0.75 * (1.0 - uBridge);
        frag = vec4(vec3(0.0), a);
        return;
    }
    if (kind == 1) {
        // the ghost: a hairline outline in the light, an almost-nothing fill
        float sd = sdRound(p, vec2(0.45), 0.07);
        float line = (1.0 - smoothstep(0.0, px * 1.1, abs(sd + 0.03))) * 0.38;
        float fill = (1.0 - smoothstep(-px, px, sd)) * 0.04;
        vec3 col = mix(pc, uKey, 0.25) * (line + fill);
        float a = (line * 0.8 + fill) * alpha * (1.0 - uBridge);
        frag = vec4(col * alpha * (1.0 - uBridge), a);
        return;
    }
    if (kind == 2) {
        // a cleared block turning to smoke: v3.y progress 0..1, v3.z seed. The noise eats it from its edges inward
        // (its top going last, as if it lifted away), and the eroding rim thins into smoke of its own color.
        float k = v3.y;
        float sd = sdRound(p, vec2(0.45), 0.07);
)"
R"(        float cov = 1.0 - smoothstep(-px * 0.7, px * 0.7, sd);
        if (cov <= 0.0 && uBlock == 0) discard;
        float n = texture(uNoise, vWorld * 0.13 + v3.z).r * 0.5 + texture(uNoise, vWorld * 0.09 + v3.z * 1.7).b * 0.5;
        float inner = clamp(-sd / 0.45, 0.0, 1.0);            // 0 at the edge, 1 at the heart
        float field = inner * 0.8 + (n - 0.5) * 0.8 + 0.2 + p.y * 0.12;
        float th = k * 1.25 - 0.08;
        float keep = smoothstep(th, th + 0.07, field);
        float rim = smoothstep(th - 0.12, th, field) * (1.0 - keep); // the frayed rim, already smoke
        vec3 col = satin(p, 1.0, pc, v0.xy, 1.0);
        if (uBlock > 0) {
            vec4 r = blockLook(p, 1.0, px, pc, vWorld, v0.xy, 1.0, links, rel, v3.w, board);
            col = r.rgb / max(r.a, 1e-3);
            cov = r.a;
        }
        vec3 smokeC = pc * uTint * (0.55 + 0.35 * uKeyGain);
        col = mix(smokeC, col, keep);
        float a = cov * (keep + rim * 0.45) * alpha * (1.0 - uBridge);
        frag = vec4(col * a, a);
        return;
    }
    discard;
}
)";

// ---------------------------------------------------------------- sprites: smoke puffs and wisps, the ring, motes

// a0: center xy, radius, stretch y. a1: rgb, alpha. a2: kind (0 puff, 2 mote, 3 blade, 4 a cleared block's smoke), seed,
// progress, swirl (4: seconds).
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 a0;
layout(location = 2) in vec4 a1;
layout(location = 3) in vec4 a2;
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 v0, v1, v2;
void main() {
    vLocal = aCorner;
    v0 = a0; v1 = a1; v2 = a2;
    vec2 ext = int(a2.x + 0.5) == 3 ? vec2(a0.z, a0.w) : vec2(a0.z, a0.z * a0.w);
    vec2 p = a0.xy + aCorner * ext;
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 v0, v1, v2;
out vec4 frag;
uniform sampler2D uNoise;
uniform float uBridge;
void main() {
    int kind = int(v2.x + 0.5);
    vec2 q = vLocal;
    float fade = 1.0 - uBridge;
    if (kind == 2) {
        // a mote of dust glinting in the beam
        float r = length(q);
        float a = exp(-r * r * 9.0) * v1.a * fade;
        frag = vec4(v1.rgb * a, 0.0);
        return;
    }
    if (kind == 3) {
        // a blade of light running along a cleared row
        float a = exp(-q.y * q.y * 18.0) * (1.0 - smoothstep(0.75, 1.0, abs(q.x))) * v1.a * fade;
        frag = vec4(v1.rgb * a, 0.0);
        return;
    }
    if (kind == 4) {
        // a cleared block's body streaming up as smoke: a column over the block, v2.y the row's seed, v2.z the rising
        // speed, v2.w seconds. A parcel of smoke at height h left the block h / V seconds ago: the block gives off
        // smoke while it erodes, so the column is a rising band of tendrils whose pattern moves with the smoke.
        float seed = v2.y, V = v2.z, t = v2.w;
        vec2 ext = vec2(v0.z, v0.z * v0.w);
        vec2 wp = v0.xy + q * ext;
        float rowY = v0.y - ext.y + 1.0;            // the quad reaches 1.0 under the row's middle (its foot fades out)
        float h = max(wp.y - rowY, 0.0);
        // some of the smoke climbs faster than the rest: fingers, not a bar
        V *= 0.5 + 1.0 * texture(uNoise, vec2(wp.x * 0.035 + seed * 0.3, 0.37 + seed)).g;
        float te = t - h / V;                       // when this parcel left the block
        const float E = 0.42;                       // the block erodes for that long
        float env = smoothstep(-0.03, 0.08, te) * (1.0 - smoothstep(0.15, E + 0.3, te));
        if (env <= 0.0) discard;
        float age = h / V;
        // it curls sideways as it climbs, more the higher it gets
        float up = smoothstep(0.0, 2.5, h);
        float curl = (texture(uNoise, vec2(wp.x * 0.05 + seed, h * 0.09 + seed * 0.7)).g - 0.5) * 2.4 * up;
        float xo = wp.x + curl;
        float wcol = 0.55 + 0.3 * h;
        float col = 1.0 - smoothstep(wcol * 0.3, wcol, abs(xo - v0.x));
        // tendrils: thin sheets stretched along the rise, carried with the smoke (keyed on te)
        float n1 = texture(uNoise, vec2(xo * 0.09 + seed * 1.3, te * 0.3 + h * 0.02 + seed)).b;
        float n2 = texture(uNoise, vec2(xo * 0.05 + seed * 0.4, te * 0.2 + h * 0.03)).g;
        float sheet = 1.0 - abs(n1 * 2.0 - 1.0);
        float hk = smoothstep(0.0, 0.8, h);
        float tend = mix(1.0, smoothstep(0.4, 0.95, sheet) * (0.3 + 1.2 * n2), hk);
        // it tears open as it rises: holes that widen with the parcel's age
        float n3 = texture(uNoise, vec2(xo * 0.03 + seed * 0.9, te * 0.09 + h * 0.012 + seed * 0.2)).r;
        tend *= mix(1.0, smoothstep(0.15 + 0.3 * age, 0.5 + 0.3 * age, n3), hk);
        // its foot: under the row's middle the smoke thins out over a ragged edge (cut flat at the quad's bottom, it
        // drew a straight line across the row below where the rows above had fallen)
        float nb = texture(uNoise, vec2(wp.x * 0.11 + seed * 0.7, te * 0.25 + seed * 0.3)).r;
        float foot = 1.0 - smoothstep(-0.05, 0.45, rowY - wp.y + (nb - 0.5) * 0.5);
        float a = col * env * tend * foot * exp(-age * 1.3) * v1.a * fade;
        frag = vec4(v1.rgb * a, a);
        return;
    }
    float seed = v2.y, k = v2.z, sw = v2.w;
    // the smoke curls: the sprite's own domain warped by the noise, turning as it ages
    vec2 w = vec2(texture(uNoise, q * 0.21 + seed + vec2(0.0, k * 0.2)).r, texture(uNoise, q * 0.21 + seed * 1.3 + 0.5).b) - 0.5;
    float c = cos(sw * k), s = sin(sw * k);
    vec2 r2 = mat2(c, s, -s, c) * q + w * (0.6 + 0.8 * k);
    float r = length(r2);
    float n1 = texture(uNoise, r2 * 0.35 + seed).g;
    float n2 = texture(uNoise, r2 * 0.9 + seed * 1.7).r;
    // thin curling sheets rather than a soft ball
    float sheet = 1.0 - abs(n2 * 2.0 - 1.0);
    float m = smoothstep(1.0, 0.1, r + (n1 - 0.5) * 0.9) * (0.35 + 0.9 * sheet * sheet);
    float a = m * v1.a * fade;
    frag = vec4(v1.rgb * a, a);
}
)";

// ---------------------------------------------------------------- bloom and the post

inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uTexel;
uniform float uThreshold;   // > 0 on the first step only
void main() {
    vec3 c = texture(uTex, vUv + vec2(-uTexel.x, -uTexel.y)).rgb + texture(uTex, vUv + vec2(uTexel.x, -uTexel.y)).rgb
           + texture(uTex, vUv + vec2(-uTexel.x, uTexel.y)).rgb + texture(uTex, vUv + vec2(uTexel.x, uTexel.y)).rgb;
    c *= 0.25;
    if (uThreshold > 0.0) {
        float m = max(c.r, max(c.g, c.b));
        c *= max(m - uThreshold, 0.0) / max(m, 1e-4);
    }
    frag = vec4(c, 1.0);
}
)";
inline const char* UP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uTex, vUv).rgb * 4.0;
    c += texture(uTex, vUv + vec2(uTexel.x, 0)).rgb * 2.0 + texture(uTex, vUv - vec2(uTexel.x, 0)).rgb * 2.0;
    c += texture(uTex, vUv + vec2(0, uTexel.y)).rgb * 2.0 + texture(uTex, vUv - vec2(0, uTexel.y)).rgb * 2.0;
    c += texture(uTex, vUv + uTexel).rgb + texture(uTex, vUv - uTexel).rgb;
    c += texture(uTex, vUv + vec2(uTexel.x, -uTexel.y)).rgb + texture(uTex, vUv + vec2(-uTexel.x, uTexel.y)).rgb;
    frag = vec4(c / 16.0, 1.0);
}
)";

// A photographic finish: a filmic curve with deep blacks, a soft bloom, a strong vignette (the edges of the stage
// fall into the dark), a fine grain.
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform vec2 uRes;
uniform float uTime, uFade, uDim, uBloomAmt, uExposure, uBridge, uGrain;
uniform vec3 uToe;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c += texture(uBloom, vUv).rgb * uBloomAmt;
    vec3 raw = c;
    c *= uExposure;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.55 * dot(q * vec2(1.0, 1.25), q * vec2(1.0, 1.25)) * 1.7 * (1.0 - uBridge);
    // a shoulder: the beam's core rolls to white, the colors stay
    vec3 over = max(c - 1.0, 0.0);
    c += (over.r + over.g + over.b) * 0.1;
    const float K = 0.6;
    c = mix(c, K + (1.0 - K) * (1.0 - exp(-(c - K) / (1.0 - K))), step(K, c));
    // the blacks keep a whisper of the stage's own tone
    c += uToe * (1.0 - uBridge);
    c = mix(c, raw, uBridge);
    float g = hash12(gl_FragCoord.xy + fract(uTime * 7.13) * 431.0) - 0.5;
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += g * uGrain * (1.0 - uBridge);
    c *= 1.0 - uDim * 0.55;
    c *= uFade;
    frag = vec4(max(c, 0.0), 1.0);
}
)";

} // namespace smoke_shaders

#pragma once
// RAINY WINDOW's shaders (see rainy.cpp).
//
// SKY_FS and BOKEH_* draw the city out of focus (a night gradient and soft discs of light) at half resolution,
// BLUR_FS blurs it (the city through the mist); WATER_* writes the water on the glass as a height field (one dome
// per drop, max-blended); GLASS_FS composes the pane: the city refracted by the water, misted where the glass is
// fogged, the window frame; CELL_FS the stained glass tiles (one slab of tinted glass per piece, leaded around),
// the ghost, the tiles melting into water and the water running over them; FINAL_FS grades and encodes the frame.
#include <string>

namespace rainy_shaders {

inline const char* COMMON = R"(
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
// The HUD's places (board space boxes: the two columns, the corners with the song and the section): 1 inside,
// feathered.
uniform vec4 uHud[4];
float boxMask(vec2 w, vec4 b, float f) {
    vec2 d = max(b.xy - w, w - b.zw);
    return 1.0 - smoothstep(0.0, f, max(d.x, d.y));
}
float hudMask(vec2 w) {
    float m = 0.0;
    for (int i = 0; i < 4; i++) m = max(m, boxMask(w, uHud[i], 1.6));
    return m;
}
)";

// ---------------------------------------------------------------- the night behind the glass
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uTop, uBottom, uGlow;
uniform float uGlowA;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float t = smoothstep(-13.0, 13.0, w.y);
    vec3 c = mix(uBottom, uTop, t);
    // the city's glow low behind the glass, a few huge soft patches of far light
    c += uGlow * uGlowA * exp(-pow((w.y + 8.0) / 6.0, 2.0)) * (0.7 + 0.6 * vnoise(vec2(w.x * 0.08, 3.1)));
    c *= 1.0 - 0.35 * hudMask(w);
    frag = vec4(c, 1.0);
}
)";

inline const char* BOKEH_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, r, intensity
layout(location = 2) in vec4 aB; // rgb, softness
uniform mat4 uVP;
out vec2 vP;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
void main() {
    vec2 wp = aA.xy + aPos * aA.z;
    vP = aPos;
    vW = wp;
    vA = aA; vB = aB;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* BOKEH_FS = R"(#version 330 core
in vec2 vP;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
out vec4 frag;
void main() {
    float d = length(vP);
    float soft = vB.w;
    float disc = 1.0 - smoothstep(1.0 - soft, 1.0, d);
    if (disc <= 0.0) discard;
    // a lens's bokeh: a flat disc, a little brighter at its rim, a faint darker core
    float rim = smoothstep(0.55, 0.97, d) * 0.35;
    float k = (0.72 + rim) * disc * vA.w * (1.0 - 0.72 * hudMask(vW));
    frag = vec4(vB.rgb * k, 1.0);
}
)";

inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uDir; // texel step
void main() {
    vec3 c = texture(uTex, vUv).rgb * 0.227;
    c += (texture(uTex, vUv + uDir * 1.385).rgb + texture(uTex, vUv - uDir * 1.385).rgb) * 0.316;
    c += (texture(uTex, vUv + uDir * 3.231).rgb + texture(uTex, vUv - uDir * 3.231).rgb) * 0.070;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the water's height field
inline const char* WATER_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, r, elong
layout(location = 2) in vec4 aB; // height scale
uniform mat4 uVP;
out vec2 vP;
flat out float vH;
void main() {
    // a running drop is longer, its tail up
    float ry = aA.z * (1.0 + aA.w);
    vec2 wp = vec2(aA.x + aPos.x * aA.z, aA.y + aA.w * aA.z * 0.45 + aPos.y * ry);
    vP = aPos;
    vH = aA.z * aB.x;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* WATER_FS = R"(#version 330 core
in vec2 vP;
flat in float vH;
out vec4 frag;
void main() {
    // a teardrop: the upper half narrows
    vec2 p = vP;
    if (p.y > 0.0) p.x /= 1.0 - 0.28 * p.y;
    float d2 = dot(p, p);
    if (d2 >= 1.0) discard;
    frag = vec4(vH * sqrt(1.0 - d2), 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------- the pane
inline const char* GLASS_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec2 uRes;
uniform sampler2D uCity, uBlur, uWater, uMist;
uniform vec3 uMistTint, uFrame, uFrameRim;
uniform float uMistA, uLens, uSpec;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float pxw = 2.0 * uView.w / uRes.y;           // board units per pixel
    vec2 px = 1.0 / uRes;
    float hud = hudMask(w);
    float h = texture(uWater, vUv).r;
    float hx = texture(uWater, vUv + vec2(px.x, 0.0)).r - texture(uWater, vUv - vec2(px.x, 0.0)).r;
    float hy = texture(uWater, vUv + vec2(0.0, px.y)).r - texture(uWater, vUv - vec2(0.0, px.y)).r;
    vec2 g = vec2(hx, hy) / (2.0 * pxw);
    float gl = length(g);
    if (gl > 3.0) g *= 3.0 / gl;
    gl = min(gl, 3.0);
    float wet = smoothstep(0.0, 0.012, h);

    // the glass: the city, veiled where the pane is misted
    float m = texture(uMist, vUv).r * uMistA;
    vec3 city = texture(uCity, vUv).rgb;
    vec3 blur = texture(uBlur, vUv).rgb;
    vec3 c = mix(city, blur * 0.8 + uMistTint, m);

    // the water: each drop a small lens, the city upside down inside it, sharper than through the mist
    if (wet > 0.0) {
        vec2 off = g * uLens * (1.0 - 0.5 * hud);
        vec2 uv = vUv + off / (2.0 * uView.zw);
        vec3 inside = texture(uCity, uv).rgb * 1.2 + blur * 0.35 + uMistTint * 0.2;
        // the light gathered in the lower part of the lens, the rim reflecting the dark room, a small highlight
        inside += (blur * 2.4 + uMistTint * 0.6) * smoothstep(0.6, 2.6, g.y) * (1.0 - 0.5 * hud);
        inside *= 1.0 - 0.6 * smoothstep(1.2, 2.9, gl) * smoothstep(-0.5, -2.5, g.y + 0.6 * abs(g.x));
        vec3 n = normalize(vec3(-g, 1.0));
        vec3 hv = normalize(vec3(-0.35, 0.55, 1.0) + vec3(0.0, 0.0, 1.0));
        float spec = pow(max(dot(n, hv), 0.0), 90.0) * uSpec * smoothstep(0.03, 0.09, h);
        inside += vec3(spec);
        c = mix(c, inside, wet * (1.0 - 0.45 * hud));
    }

    // the window frame: two thin mullions along the well's sides, the bottom rail, the sill in the room below
    float fr = 0.0;
    vec3 fc = uFrame;
    float mx = abs(abs(w.x) - 5.17) - 0.13;
    if (w.y > -10.05) {
        float a = 1.0 - smoothstep(-pxw, pxw, mx);
        // a faint rim of city light on the mullion's edges
        float rim = (1.0 - smoothstep(0.0, 0.05, abs(mx + 0.025))) * smoothstep(-13.0, 6.0, w.y);
        fc = uFrame + uFrameRim * rim * 0.9;
        fr = a;
    }
    float rail = 1.0 - smoothstep(-pxw, pxw, w.y + 10.02);
    if (rail > 0.0) {
        float ry = -10.02 - w.y;
        vec3 rc = uFrame * (1.0 + 0.4 * (1.0 - smoothstep(0.0, 0.06, ry)));
        rc += uFrameRim * 0.6 * (1.0 - smoothstep(0.0, 0.035, ry));
        // the sill below the rail: the dark room, a faint reflection of the city's glow
        float sill = smoothstep(0.32, 0.36, ry);
        vec3 sc = uFrame * 0.7 + blur * 0.18 * (1.0 - smoothstep(0.3, 1.8, ry));
        rc = mix(rc, sc, sill);
        c = mix(c, rc, rail);
    }
    c = mix(c, fc, fr);

    // a soft vignette
    vec2 v = vUv * 2.0 - 1.0;
    c *= 1.0 - 0.22 * dot(v * vec2(0.7, 1.0), v * vec2(0.7, 1.0));
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the stained glass tiles
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
    vec2 wp = aA.xy + aPos * aA.z;
    vLocal = aPos * 0.5;
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
uniform vec2 uRes;
uniform float uPxCell;    // board units per pixel
uniform sampler2D uCity, uBlur;
uniform vec3 uLead;
uniform float uBack, uGain;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Distance from p (cell units, -0.5..0.5) inward to the piece's outline, from the neighbour mask (bits: 0 -x,
// 1 +x, 2 +y, 3 -y; diagonals 4 (-x,+y), 5 (+x,+y), 6 (-x,-y), 7 (+x,-y)). Rounded outer corners, filleted inner ones.
float outline(vec2 p, int m) {
    float dl = has(m, 0) ? 9.0 : p.x + 0.5;
    float dr = has(m, 1) ? 9.0 : 0.5 - p.x;
    float du = has(m, 2) ? 9.0 : 0.5 - p.y;
    float dd = has(m, 3) ? 9.0 : p.y + 0.5;
    float d = min(min(dl, dr), min(du, dd));
    if (has(m, 0) && has(m, 2) && !has(m, 4)) d = min(d, length(vec2(p.x + 0.5, 0.5 - p.y)));
    if (has(m, 1) && has(m, 2) && !has(m, 5)) d = min(d, length(vec2(0.5 - p.x, 0.5 - p.y)));
    if (has(m, 0) && has(m, 3) && !has(m, 6)) d = min(d, length(vec2(p.x + 0.5, p.y + 0.5)));
    if (has(m, 1) && has(m, 3) && !has(m, 7)) d = min(d, length(vec2(0.5 - p.x, p.y + 0.5)));
    const float R = 0.16;
    vec2 q;
    if (!has(m, 0) && !has(m, 2)) { q = vec2(dl, du); if (q.x < R && q.y < R) d = min(d, R - length(R - q)); }
    if (!has(m, 1) && !has(m, 2)) { q = vec2(dr, du); if (q.x < R && q.y < R) d = min(d, R - length(R - q)); }
    if (!has(m, 0) && !has(m, 3)) { q = vec2(dl, dd); if (q.x < R && q.y < R) d = min(d, R - length(R - q)); }
    if (!has(m, 1) && !has(m, 3)) { q = vec2(dr, dd); if (q.x < R && q.y < R) d = min(d, R - length(R - q)); }
    return d;
}
// Tinted cathedral glass lit from behind by the city: the city's light through the slab, a hammered ripple, a
// brighter line where the glass meets the lead.
vec3 glassAt(vec3 tint, float d, float seed, float glint) {
    vec2 rip = vec2(vnoise(vW * 2.1 + seed * 3.1), vnoise(vW * 2.1 + seed * 3.1 + 17.0)) - 0.5;
    vec2 suv = gl_FragCoord.xy / uRes + rip * 0.025;
    vec3 back = mix(texture(uBlur, suv).rgb, texture(uCity, suv).rgb, 0.5);
    float ham = vnoise(vW * 3.4 + rip * 2.5 + seed);
    vec3 c = tint * (uBack + uGain * back);
    c *= 0.95 + 0.1 * ham;
    // a broad reflection of the room across the pane
    float band = sin((vW.x * 0.8 + vW.y) * 0.45 + 1.3);
    c *= 1.0 + 0.1 * smoothstep(0.3, 1.0, band);
    c *= 1.0 + 0.3 * exp(-max(d - 0.06, 0.0) / 0.045);
    // the light from the room catching the slab from the upper left
    c *= 1.0 + 0.1 * clamp((-vW.x + vW.y) * 0.02, -1.0, 1.0);
    return c * (1.0 + glint);
}
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    float aa = uPxCell / max(vA.z * 2.0, 1e-3);
    if (kind == 4) {
        // water running over the tiles: a bead with a dark rim and a highlight (no color of its own)
        vec2 p = vLocal * 2.0;
        p.x /= 1.0 - 0.25 * max(p.y, 0.0);
        float d = length(p);
        if (d >= 1.0) discard;
        float rimA = smoothstep(0.55, 1.0, d) * 0.45 * (1.0 - smoothstep(0.96, 1.0, d));
        float spec = exp(-dot(p - vec2(-0.3, 0.38), p - vec2(-0.3, 0.38)) * 28.0) * 0.8;
        frag = vec4(vec3(spec), rimA + spec * 0.3) * vB.w;
        return;
    }
    vec2 p = vLocal;
    float d = outline(p, m);
    if (kind == 1) {
        // the ghost: a pale line traced in the glass where the piece will land
        float cov = 1.0 - smoothstep(0.022 - aa, 0.022 + aa, abs(d - 0.07));
        vec3 c = mix(vec3(1.0), vB.rgb, 0.45) * 0.9;
        frag = vec4(c * cov, cov) * vB.w;
        return;
    }
    if (d < -aa) discard;
    float edge = smoothstep(-aa, aa, d);
    float lead = 1.0 - smoothstep(0.075 - aa, 0.075 + aa, d);
    float alpha = vB.w;
    vec3 tint = vB.rgb;
    if (kind == 3) {
        // melting: the slab runs down in drips, clearing to water, gone in a third of a second
        float k = vC.y;
        float sp = 0.85 + 0.3 * vnoise(vec2(vW.x * 4.0, vC.w * 7.0));
        float top = 0.52 - k * 1.35 * sp;
        float cut = smoothstep(top + aa, top - aa, p.y);
        if (cut <= 0.0) discard;
        alpha *= cut * (1.0 - 0.7 * smoothstep(0.3, 0.85, k));
        tint = mix(tint, tint * 0.6 + 0.25, 0.3 * smoothstep(0.0, 0.6, k));
        lead *= 1.0 - smoothstep(0.0, 0.25, k);
        // the meniscus: a bright line along the falling surface of the water
        tint *= 1.0 + 0.6 * k * exp(-pow((top - p.y) / 0.035, 2.0));
    }
    vec3 c = glassAt(tint, d, vC.w, vC.z);
    // the lead came: dark, a soft sheen along its middle
    vec3 lc = uLead * (1.0 + 1.6 * exp(-pow((d - 0.035) / 0.02, 2.0)));
    c = mix(c, lc, lead);
    float a = alpha * edge;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uSlow, uDim, uFade, uBridge, uSwell;
uniform vec3 uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb * (1.0 + 0.25 * uSwell);
    // slow motion: the deep blue of a later hour, the city's colors drained
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(l) * vec3(0.55, 0.75, 1.25) * 1.05, uSlow * (1.0 - clamp(s.a, 0.0, 1.0) * 0.6));
    // a soft shoulder for the brightest lights
    c = mix(c, 0.8 + 0.2 * (1.0 - exp(-(c - 0.8) / 0.2)), step(vec3(0.8), c));
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
    float halo = smoothstep(0.25, 0.5, d) * uHaloA;
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

} // namespace rainy_shaders

#pragma once
// STAR CHART's shaders: the sheet and its skies, the engraved lines, the stars, the enamelled pieces, the lettering.

namespace starchart_shaders {

// ---------------------------------------------------------------- the sheet
// A deep, even ground with a very slow mottle (the plate's ink, or the parchment's tone); two hemispheres a shade
// deeper, lit a little toward their poles, each crossed by the Milky Way as a soft band of fine stippling that turns
// with its disc; the nebulae a Tetris leaves, stippled the same way; the well a calm, darker panel.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uSky, uSky2, uWell, uGlow, uInk;
uniform vec4 uView;    // center x y, half extent x y (board units)
uniform vec4 uDisc0, uDisc1; // center x y, radius, turn
uniform float uLight, uSeed, uMilky, uPx, uSwell;
uniform vec4 uNeb[8];  // board x y, radius, amount
uniform vec2 uNebAng[8]; // turn, seed
uniform int uNebN;
uniform float uBridge;
uniform vec3 uBridgeCol;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.07 + 1.7; a *= 0.5; }
    return s / 0.875;
}
// Engraver's stippling: one dot per small cell of the plate (a random place, a random size), kept where `dens`
// asks for it; anti-aliased so that the dots glide without flicker when the disc turns.
float stipple(vec2 q, float dens) {
    const float cellU = 0.13;
    // a dot stays well inside its own cell: only that cell is looked at
    vec2 c = floor(q / cellU);
    vec2 h = hash2(c);
    if (h.x > dens) return 0.0;
    vec2 o = (c + 0.25 + 0.5 * hash2(c + 7.3)) * cellU;
    float r = (0.55 + 0.6 * h.y) / uPx; // board units (~ a pixel)
    return clamp((r - length(q - o)) * uPx + 0.5, 0.0, 1.0) * (0.5 + 0.5 * h.y);
}
vec2 rot(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw; // board units
    float m = fbm(w * 0.07) * 0.7 + fbm(w * 0.29 + 3.1) * 0.3;
    vec3 c = mix(uSky, uSky2, clamp((m - 0.3) * 1.5, 0.0, 1.0));
    float ink = 0.0;  // light (glow) on dark charts, ink on pale ones
    // the hemispheres
    for (int k = 0; k < 2; k++) {
        vec4 D = k == 0 ? uDisc0 : uDisc1;
        vec2 q = w - D.xy;
        float r = length(q) / D.z;
        float inside = clamp((1.0 - r) * D.z * uPx + 0.5, 0.0, 1.0);
        if (inside <= 0.0) continue;
        vec3 deep = mix(uSky * 0.78, uSky * 1.06 + uGlow * 0.012, (1.0 - r * r) * (1.0 - uLight));
        if (uLight > 0.5) deep = mix(uSky * 1.035, uSky * 0.985, r * r);
        c = mix(c, deep, inside);
        // the Milky Way: a soft winding band through the disc, turning with it
        vec2 l = rot(q, -D.w) / D.z;
        if (k == 1) l = vec2(-l.y, l.x);
        float bandY = 0.32 * sin(l.x * 2.1 + 0.7) - 0.08;
        float band = exp(-pow((l.y - bandY) / 0.2, 2.0));
        float clumps = fbm(l * 4.5 + float(k) * 9.1);
        float mw = band * (0.45 + 0.75 * clumps) * uMilky * smoothstep(1.0, 0.86, r);
        ink += mw * 0.05 * inside;
        ink += stipple(rot(q, -D.w), mw * 0.42) * 0.36 * inside * uMilky;
    }
    // nebulae (a Tetris), turning with their disc
    for (int n = 0; n < 8; n++) {
        if (n >= uNebN) break;
        vec4 N = uNeb[n];
        vec2 q = rot(w - N.xy, -uNebAng[n].x) / N.z;
        if (dot(q, q) > 2.0) continue;
        float f = fbm(q * 2.3 + uNebAng[n].y);
        float neb = exp(-dot(q * vec2(1.0, 1.7), q * vec2(1.0, 1.7)) * 2.2) * (0.5 + 0.9 * f) * N.w;
        ink += neb * 0.16;
        ink += stipple(q * N.z + uNebAng[n].y, neb * 0.9) * 0.6 * N.w;
    }
    // the well: a calm, deeper panel
    vec2 qw = abs(w - vec2(0.0, 0.0)) - vec2(5.0, 10.0);
    float well = clamp(0.5 - max(qw.x, qw.y) * uPx, 0.0, 1.0);
    c = mix(c, uWell, well);
    ink *= 1.0 - well;
    ink *= 1.0 + uSwell * 0.6;
    if (uLight > 0.5) c = mix(c, uInk, clamp(ink, 0.0, 1.0) * 0.55);
    else c += uGlow * ink;
    // a slight fall-off toward the sheet's corners
    vec2 e = (vUv - 0.5) * 2.0;
    c *= 1.0 - 0.14 * dot(e * vec2(0.8, 1.0), e * vec2(0.8, 1.0)) * 0.5;
    // fine plate grain
    c *= 1.0 + (hash(floor(gl_FragCoord.xy)) - 0.5) * (uLight > 0.5 ? 0.02 : 0.03);
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- engraved lines
// One instance per segment: endpoints in board units, a color and alpha, a width in pixels and a glow (a soft
// halo, for the dark charts). Round caps, anti-aliased; hairlines under a pixel keep their weight as coverage.
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x0 y0 x1 y1
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // width px, glow, -, -
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
    float hw = (aC.x * 0.5 + 1.0 + (aC.y > 0.0 ? 7.0 : 0.0)) / uPxPerUnit;
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
    float a = clamp(hw + 0.5 - d, 0.0, 1.0);
    if (hw < 0.5) a = clamp(1.0 - d, 0.0, 1.0) * (hw * 2.0);
    // the halo adds light without covering (alpha 0): it only shows on the dark charts
    float halo = vC.y * exp(-max(d - hw, 0.0) * 0.55) * 0.32;
    a *= vB.a;
    halo *= vB.a;
    frag = vec4(vB.rgb * (a + halo * (1.0 - a)), a);
}
)";

// Composites a premultiplied layer (the lines, gathered with MAX blending so that joints and crossings never
// darken or double) over the scene.
inline const char* LAYER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uLayer;
void main() { frag = texture(uLayer, vUv); }
)";

// ---------------------------------------------------------------- stars
// An engraved star: a round core with fine tapering rays (eight for the brightest, four, or none for the faintest),
// or an open circle on the Chinese chart; and a soft glow around it on the dark charts (light added, no cover).
inline const char* STAR_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (board units: the rays' length), spin
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // rays 0..2, glow, style, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = 1.0 + aC.y * 2.2;
    vLocal = aPos * ext;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(aA.xy + aPos * ext * aA.z, 0.0, 1.0);
}
)";
inline const char* STAR_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPxPerUnit;
uniform vec3 uGlowCol;
float ray(vec2 p, float ang, float len, float wid, float aa) {
    float c = cos(ang), s = sin(ang);
    vec2 q = vec2(c * p.x + s * p.y, -s * p.x + c * p.y);
    if (q.x < 0.0) return 0.0;
    float half_ = wid * max(0.0, 1.0 - q.x / len);
    return clamp((half_ - abs(q.y)) / aa + 0.5, 0.0, 1.0) * step(q.x, len);
}
void main() {
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3); // local units per pixel
    float aa = px;
    vec2 p = vLocal;
    float r = length(p);
    float rays = vC.x;
    float core;
    if (vC.z > 0.5) {
        // an open circle (bright stars filled)
        float rr = 0.42;
        float ring = clamp((max(px * 0.9, 0.07) - abs(r - rr)) / aa + 0.5, 0.0, 1.0);
        float fill = clamp((rr - r) / aa + 0.5, 0.0, 1.0) * step(1.2, rays);
        core = max(ring, fill);
    } else {
        float rc = 0.2 + 0.06 * rays;
        core = clamp((max(rc, px * 0.8) - r) / aa + 0.5, 0.0, 1.0);
        float spin = vA.w;
        if (rays > 0.05) {
            float wid = max(0.075, px * 0.7);
            for (int i = 0; i < 4; i++) core = max(core, ray(p, spin + float(i) * 1.5707963, 1.0, wid, aa) * min(rays, 1.0));
            if (rays > 1.0)
                for (int i = 0; i < 4; i++)
                    core = max(core, ray(p, spin + 0.785398 + float(i) * 1.5707963, 0.58, wid * 0.85, aa) * (rays - 1.0));
        }
    }
    float halo = vC.y * exp(-r * r * 4.0) * 0.5;
    float a = core * vB.a;
    frag = vec4(vB.rgb * a + uGlowCol * halo * vB.a * (1.0 - a), a);
}
)";

// ---------------------------------------------------------------- pieces
// Enamelled plaques: each piece a field of its enamel inside one gold rim (the cells of a piece merge, no seams), a
// fine inner rule following the rim and a small engraved star on every cell; the enamel deepens toward the rim
// and catches a slow sheen. On the pale chart: a hand-colored wash inside an ink outline.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost, 2 gauge), erase 0..1, shine, drain
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
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
uniform float uRimW, uLight, uEnamel, uTime, uPxPerUnit;
uniform vec3 uRim, uRimHi, uInk;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the piece the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right: the neighbors of the same piece extend it).
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.08, rc = 0.05;
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
// A small four-pointed engraved star (cell units, centered).
float starMark(vec2 p, float aa) {
    vec2 q = abs(p);
    float d = pow(pow(q.x, 0.55) + pow(q.y, 0.55), 1.0 / 0.55) - 0.13;
    return clamp(0.5 - d / aa, 0.0, 1.0);
}

void main() {
    int mode = int(vC.x + 0.5);
    float pxL = length(fwidth(vLocal)) * 0.7071; // cell units per pixel
    float aa = max(pxL, 1e-4);
    vec3 tint = vB.rgb;
    int mk = int(vA.w + 0.5);
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float sd = shapeSdf(vLocal, mk, 0.045);
    float rw = uRimW * aa;                       // the rim's width (cell units)
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float band = inside * smoothstep(-rw - aa * 0.5, -rw + aa * 0.5, sd);
    float rule = 1.0 - smoothstep(0.0, aa, abs(sd + rw + 0.075) - aa * 0.35);
    rule *= inside;
    vec3 rimC = uRim;
    // the gold catches a slow sheen drifting across the stack
    float sheen = pow(0.5 + 0.5 * sin((vWorld.x * 0.6 + vWorld.y * 0.9) * 0.9 - uTime * 0.45), 6.0);
    rimC = mix(uRim, uRimHi, clamp(sheen * 0.55 + vC.z, 0.0, 1.0));
    float bridge = uBridge;
    if (mode == 1) {
        // the ghost: the rim alone, faint
        float a = band * vB.a;
        vec3 col = mix(rimC, uBridgeInk, bridge);
        frag = vec4(col * a, a);
        return;
    }
    // the enamel: deeper toward the rim, lighter in the middle of the field
    float depth = smoothstep(-0.42, -rw, sd);
    // translucent enamel over engine-turned gold (guilloché): very fine lines seen through it, in the piece's own
    // frame (five a cell, so they run on from cell to cell and travel with the piece)
    float guil = 0.5 + 0.5 * sin((vLocal.x + vLocal.y) * 31.415926);
    vec3 en = tint * (0.93 + 0.08 * guil) * (1.08 - 0.36 * depth);
    float drain = vC.w;
    en = mix(en, vec3(dot(en, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    if (uLight > 0.5) {
        // a hand-colored wash: paler in the middle, pooled at its edge
        en = mix(vec3(1.0), tint, 0.88 + 0.12 * depth);
        en = mix(en, vec3(0.8), drain);
    }
    float star = starMark(vLocal, aa) * inside * (1.0 - band);
    float erase = vC.y;
    float keep = 1.0;
    float flare = 0.0;
    if (erase > 0.0) {
        // the gold leaf flakes off: the enamel goes in flakes, the rim flares and fades
        float n = vnoise(vWorld * 9.0) * 0.6 + vnoise(vWorld * 23.0) * 0.4;
        keep = smoothstep(erase * 1.6 - 0.3, erase * 1.6 - 0.1, n) * (1.0 - smoothstep(0.55, 1.0, erase));
        flare = (1.0 - erase) * smoothstep(0.0, 0.25, erase);
    }
    vec3 gold = mix(rimC, uRimHi, flare);
    float lineA = max(band, max(rule * 0.75, star * 0.9));
    vec3 col = mix(en, gold, lineA);
    float enA = uEnamel * (1.0 - 0.85 * drain);  // drained: the gold rim and its rule are left
    float a = inside * mix(enA, 1.0, lineA);
    col = mix(col, uBridgeInk, bridge);
    a *= vB.a * keep;
    frag = vec4(col * a, a);
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
// Engraved lettering, with an optional thin halo of the ground so it stays clean where it crosses a line.
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.26 - w, 0.26 + w, d) * uHaloA;
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

} // namespace starchart_shaders

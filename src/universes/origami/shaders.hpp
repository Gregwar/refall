#pragma once
// ORIGAMI's shaders (see origami.cpp).
//
// PAPER_FS draws the sheet (a soft light, a fine grain, the soft shadows of everything lying on it from a small
// target) and the well, a shallow folded tray; CELL_VS / CELL_FS the folded pieces (a lattice of diamond facets lit
// by the light, its creases), their shadows, the ghost (a dashed valley-fold line, as in folding diagrams) and the
// cells folding away; TRI_FS the cranes' facets; FINAL_FS encodes the frame.
#include <string>

namespace origami_shaders {

inline const char* COMMON = R"(
uniform float uSeed;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
)";

// ---------------------------------------------------------------- the sheet and the well
inline const char* PAPER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;           // center, half extents (board units)
uniform vec2 uRes;
uniform vec3 uSheet;          // linear
uniform float uLight, uDark;  // the light's level; 1 on a dark sheet
uniform vec2 uLightXY;        // where the light comes from (screen direction)
uniform sampler2D uShadow;    // r: shadow coverage
uniform vec2 uShadowTexel;
uniform float uCrease, uPattern, uWarm;
uniform float uWell;          // 0 at a break: no tray
// The sheet was folded before: a giant crease pattern over the whole view, each facet tilted a little and lit by the
// light (pattern 0: a square base, 1: a sixteen-fold star, 2: a square base set lower and turned, and so on).
vec2 creaseTilt(vec2 w, out float edge) {
    float k = mod(uPattern, 3.0);
    vec2 cen = k < 0.5 ? vec2(0.0, 0.4) : k < 1.5 ? vec2(0.0, -3.0) : vec2(0.0, 2.5);
    float rot = k < 1.5 ? 0.0 : 0.3927;
    vec2 d = w - cen;
    d = vec2(d.x * cos(rot) - d.y * sin(rot), d.x * sin(rot) + d.y * cos(rot));
    vec2 q = d / 21.0;
    float n = k > 0.5 && k < 1.5 ? 16.0 : 8.0;
    float a = atan(q.y, q.x) + 3.14159265;
    float sw = 6.2831853 / n;
    float sec = floor(a / sw);
    float ac = (sec + 0.5) * sw - 3.14159265;
    float sg = mod(sec, 2.0) < 1.0 ? 1.0 : -1.0;
    // rings: the pleats turn over at a square around the centre and again further out
    vec2 r45 = vec2(q.x + q.y, q.x - q.y) * 0.7071;
    float ring = max(abs(r45.x), abs(r45.y));
    float band = floor(ring / 0.42);
    if (mod(band, 2.0) > 0.5) sg = -sg;
    float fa = fract(a / sw);
    edge = min(min(fa, 1.0 - fa) * sw * length(q) * 21.0, abs(fract(ring / 0.42 + 0.5) - 0.5) * 0.42 * 21.0);
    return vec2(cos(ac), sin(ac)) * sg;
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec3 c = uSheet;
    // the paper's fibres: a fine, soft grain (no visible pattern)
    float grain = (vnoise(w * 9.0) - 0.5) * 0.018 + (vnoise(w * 1.3 + 7.0) - 0.5) * 0.03 + (hash(floor(w * 60.0)) - 0.5) * 0.008;
    c *= 1.0 + grain;
    // the old creases: facets catching the light, a fine pressed line between them
    {
        float edge;
        vec2 tilt = creaseTilt(w, edge);
        vec2 ld = normalize(uLightXY);
        float inWell = (abs(w.x) < 5.0 && w.y > -10.0 && uWell > 0.5) ? 0.5 : 1.0;
        float lit = uCrease * inWell * dot(tilt, ld);
        c *= 1.0 + lit * 1.6;
        c += uDark * lit * 0.12 * (uSheet + vec3(0.05)); // a dark sheet: the facets catch a little light of their own
        float pxw = 2.0 * uView.w / uRes.y;
        float line = 1.0 - smoothstep(0.4 * pxw, 1.4 * pxw, edge);
        c *= 1.0 - line * uCrease * inWell * 1.2;
        c += uDark * line * uCrease * inWell * 0.25 * (uSheet + vec3(0.05));
    }
    // the well: a shallow tray folded into the sheet, open at the top
    float inX = 5.0 - abs(w.x), inY = w.y + 10.0;
    if (inX > 0.0 && inY > 0.0 && uWell > 0.5) {
        c *= mix(0.955, 1.08, uDark);
        // the inner walls in the light's shade: the wall facing away from it shades the floor near it
        float sl = exp(-max(0.0, w.x + 5.0) * 3.2) * (uLightXY.x < 0.0 ? 1.0 : 0.3) + exp(-max(0.0, 5.0 - w.x) * 3.2) * (uLightXY.x < 0.0 ? 0.3 : 1.0);
        c *= 1.0 - 0.06 * sl * smoothstep(-0.2, 0.4, w.y + 10.0) - 0.05 * exp(-inY * 3.5);
    }
    // the creases of the tray: a valley line and its lit lip
    float px = 2.0 * uView.w / uRes.y;
    float dSide = abs(abs(w.x) - 5.0), dFloor = abs(w.y + 10.0);
    float side = (w.y > -10.0 && w.y < 10.6) ? dSide : 1e3;
    float floor_ = abs(w.x) < 5.0 ? dFloor : 1e3;
    if (w.y < -10.0 && abs(w.x) > 5.0) side = min(side, length(vec2(abs(w.x) - 5.0, w.y + 10.0)));
    float d = min(side, floor_);
    float topFade = 1.0 - smoothstep(9.6, 10.6, w.y);
    float line = (1.0 - smoothstep(0.5 * px, 1.6 * px, d)) * topFade * uWell;
    c *= 1.0 - line * mix(0.26, 0.0, uDark);
    c += line * uDark * 0.1 * (vec3(1.0) - c);
    // a lip of light just outside the crease
    float outX = abs(w.x) - 5.0;
    float lip = (outX > 0.0 && w.y > -10.0 ? exp(-outX / (3.0 * px)) * (1.0 - smoothstep(0.0, 1.5 * px, -outX)) : 0.0) + (w.y < -10.0 && abs(w.x) < 5.0 ? exp(-(-10.0 - w.y) / (3.0 * px)) : 0.0);
    c *= 1.0 + 0.05 * lip * topFade * (1.0 - line) * uWell;
    // the soft shadows of what lies on the sheet
    vec2 st = vUv;
    float sh = texture(uShadow, st).r * 0.4;
    sh += (texture(uShadow, st + vec2(uShadowTexel.x, 0.0) * 1.5).r + texture(uShadow, st - vec2(uShadowTexel.x, 0.0) * 1.5).r
         + texture(uShadow, st + vec2(0.0, uShadowTexel.y) * 1.5).r + texture(uShadow, st - vec2(0.0, uShadowTexel.y) * 1.5).r) * 0.15;
    c *= 1.0 - sh * mix(0.32, 0.5, uDark);
    // a soft light across the sheet, from its side
    vec2 v = vUv * 2.0 - 1.0;
    float light = uLight * (1.0 + 0.06 * dot(v, uLightXY)) * (1.0 - 0.14 * dot(v * vec2(0.7, 1.0), v * vec2(0.7, 1.0)));
    // a two-tone light: warm where it comes from, cool across the sheet, warmer at a peak
    float lside = clamp(0.5 + 0.45 * dot(v, normalize(uLightXY)), 0.0, 1.0);
    vec3 tint = mix(vec3(0.965, 0.985, 1.03), vec3(1.04, 1.0, 0.95), lside);
    c *= mix(vec3(1.0), tint, uWarm);
    frag = vec4(c * light, 0.0);
}
)";

// ---------------------------------------------------------------- the pieces (instanced cells)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // kind, progress, fold, lift
layout(location = 4) in vec4 aD; // ridge mask (cells folding away), 0, 0, 0
uniform mat4 uVP;
uniform float uPx;               // board units per pixel
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    int kind = int(aC.x + 0.5);
    float ext = aA.z + (kind == 2 ? aA.z * 0.6 : 1.5 * uPx);
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform float uPx;
uniform vec3 uLightDir;  // toward the light (x right, y up, z toward the viewer)
uniform vec3 uInk, uBack;
uniform float uFoil;
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// The fold: every piece is a strip of paper folded along its length, a ridge running through its cells' centres
// from neighbour to neighbour (mitred where the strip turns, a valley crease in the inner corner), its two sides
// sloping down to the cut edges. r: the cell's neighbours of the same piece (bits: left, right, down, up); fold: the
// slope (tight when falling, flatter once settled).
vec3 facetNormal(vec2 p, int r, float fold, out float dist, out vec2 g) {
    bool L = has(r, 0), R = has(r, 1), D = has(r, 2), U = has(r, 3);
    // a strip's end: the ridge runs through to the cut
    if ((L || R) && !(D || U)) { L = true; R = true; }
    if ((D || U) && !(L || R)) { D = true; U = true; }
    float best = 1e3;
    g = vec2(0.0);
    if ((R && p.x >= 0.0) || (L && p.x <= 0.0)) { best = abs(p.y); g = vec2(0.0, sign(p.y)); }
    if (((U && p.y >= 0.0) || (D && p.y <= 0.0)) && abs(p.x) < best) { best = abs(p.x); g = vec2(sign(p.x), 0.0); }
    if (best > 1e2) {
        // no ridge over this corner: two flat facets down to the corner (a mitre); a lone cell is a square folded
        // on its diagonal
        best = max(abs(p.x), abs(p.y));
        if (r == 0) g = (p.x - p.y > 0.0 ? vec2(0.7071, -0.7071) : vec2(-0.7071, 0.7071));
        else g = abs(p.x) > abs(p.y) ? vec2(sign(p.x), 0.0) : vec2(0.0, sign(p.y));
    }
    dist = best;
    return normalize(vec3(g * fold * 0.75, 1.0));
}
vec3 litPaper(vec3 col, vec3 n, float foil) {
    vec3 L = normalize(uLightDir);
    float k = 1.0 + 1.25 * (dot(n, L) - L.z);
    vec3 c = col * max(k, 0.15);
    // foil paper: a metallic glint on the facets turned to the light
    vec3 h = normalize(L + vec3(0.0, 0.0, 1.0));
    float sp = pow(max(dot(n, h), 0.0), 60.0);
    c += foil * (col * 0.6 + 0.25) * sp * 1.4;
    return c;
}
void main() {
    int kind = int(vC.x + 0.5);
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    float aa = uPx / max(vA.z * 2.0, 1e-3);
    if (kind == 2) {
        // a shadow on the sheet (into the shadow target, max blended): a soft square
        vec2 q = abs(p) - 0.44;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
        float a = (1.0 - smoothstep(-0.06, 0.06 + vC.w * 0.5, d)) * vB.w;
        frag = vec4(a, 0.0, 0.0, a);
        return;
    }
    if (kind == 1) {
        // the ghost: a dashed valley-fold line around the outline (the sides with no cell of the piece)
        vec4 o = vec4(0.0);
        for (int s = 0; s < 4; s++) {
            if (has(m, s)) continue;
            float across = s == 0 ? p.x + 0.46 : s == 1 ? p.x - 0.46 : s == 2 ? p.y + 0.46 : p.y - 0.46;
            float along = (s < 2 ? p.y : p.x);
            if (abs(along) > 0.54) continue;
            float f = fract((along + 0.5) * 3.0 + 0.25);
            float dash = smoothstep(0.0, 0.06, f) * smoothstep(0.62, 0.56, f);
            float cov = (1.0 - smoothstep(0.022 - aa, 0.022 + aa, abs(across))) * dash;
            o = max(o, vec4(uInk * cov, cov));
        }
        frag = o * vB.w;
        return;
    }
    // own square on the sides where the same piece continues (no double cover), an antialiased edge elsewhere
    if ((has(m, 0) && p.x < -0.5) || (has(m, 1) && p.x > 0.5) || (has(m, 2) && p.y < -0.5) || (has(m, 3) && p.y > 0.5)) discard;
    float edgeD = 1.0;      // distance to the piece's outline
    float cov = 1.0;
    float rim = 0.0;        // + lit edge, - shaded edge
    for (int s = 0; s < 4; s++) {
        if (has(m, s)) continue;
        float d = s == 0 ? p.x + 0.5 : s == 1 ? 0.5 - p.x : s == 2 ? p.y + 0.5 : 0.5 - p.y;
        cov = min(cov, smoothstep(-aa, aa, d));
        edgeD = min(edgeD, d);
        float e = 1.0 - smoothstep(aa * 0.5, aa * 2.2, d);
        // the paper's cut edge: lit on the sides toward the light, in shade on the others
        vec2 nrm = s == 0 ? vec2(-1, 0) : s == 1 ? vec2(1, 0) : s == 2 ? vec2(0, -1) : vec2(0, 1);
        float facing = dot(nrm, normalize(uLightDir.xy));
        rim += e * (facing > 0.0 ? 0.5 : -1.0) * (0.4 + 0.6 * abs(facing));
    }
    if (cov <= 0.0) discard;
    vec3 col = vB.rgb;
    float fold = vC.z;
    if (kind == 3) {
        // folding away: the cell's upper half folds down over its lower half (its white back turning up), then the
        // folded half closes toward the crease
        float k = vC.y;
        float a = k * 3.14159;
        float top = 0.5 * cos(min(a, 3.14159)); // projected height of the flap above the crease
        float squash = 1.0 - smoothstep(0.55, 1.0, k);
        float y = p.y / max(squash, 1e-3);
        if (abs(y) > 0.5) discard;
        if (y > 0.0 && y > top) discard;
        bool flap = y > 0.0 || (k > 0.5 && y > top);
        float dd; vec2 gg;
        vec3 n = facetNormal(vec2(p.x, y), int(vD.x + 0.5), fold, dd, gg);
        vec3 c = litPaper(col, n, uFoil);
        if (flap) {
            if (k > 0.5) c = uBack * (0.8 + 0.25 * (1.0 - k));
            else c *= 0.8 + 0.2 * cos(a);
        }
        frag = vec4(c * cov, cov) * vB.w;
        return;
    }
    float dist; vec2 g;
    vec3 n = facetNormal(p, m, fold, dist, g);
    // the paper's width across its fold: a tightly folded strip is narrower, it widens as it opens
    float w = vD.y;
    if (w < 0.999) {
        float e = 0.5 * w - dist;
        float cw = smoothstep(-aa, aa, e);
        if (cw <= 0.0) discard;
        cov = min(cov, cw);
        float facing = dot(g, normalize(uLightDir.xy));
        rim = min(rim, 0.0) + (1.0 - smoothstep(aa * 0.5, aa * 2.2, e)) * (facing > 0.0 ? 0.5 : -1.0) * (0.4 + 0.6 * abs(facing));
    }
    vec3 c = litPaper(col, n, uFoil * (0.4 + 0.6 * fold));
    // the creases themselves: a fine pressed line where the facets meet
    float cl = clamp(length(fwidth(n.xy)) * 4.0, 0.0, 1.0);
    c *= 1.0 - 0.1 * cl;
    // the cut edge
    c *= 1.0 + clamp(rim, -1.0, 1.0) * 0.16;
    frag = vec4(c * cov, cov) * vB.w;
}
)";

// ---------------------------------------------------------------- the cranes' facets (board space triangles)
inline const char* TRI_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aCol;
uniform mat4 uVP;
out vec4 vCol;
void main() {
    vCol = aCol;
    gl_Position = uVP * vec4(aPos, 0.0, 1.0);
}
)";
inline const char* TRI_FS = R"(#version 330 core
in vec4 vCol;
out vec4 frag;
uniform float uShadowPass;
void main() {
    if (uShadowPass > 0.5) { frag = vec4(vCol.a, 0.0, 0.0, vCol.a); return; }
    frag = vec4(vCol.rgb * vCol.a, vCol.a);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uSlow, uDim, uFade, uBridge, uOver;
uniform vec3 uBridgeCol, uBridgeInk, uSheet; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    // slow motion: the blue hour, a low light from the side
    vec2 v = (vUv - vec2(0.62, 0.5)) * vec2(1.4, 1.0);
    float glow = 0.55 + 0.6 * exp(-dot(v, v) * 1.6);
    vec3 dusk = c * glow * vec3(0.55, 0.64, 0.92);
    c = mix(c, dusk, uSlow);
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, vec3(g) * 0.92, uOver * 0.45);
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    // dimmed (paused, behind the game over): the world fades into the sheet, the text over it stays readable
    c = mix(c, uSheet, uDim * 0.7);
    c *= (1.0 - uDim * 0.15) * uFade;
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

} // namespace origami_shaders

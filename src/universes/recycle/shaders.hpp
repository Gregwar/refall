#pragma once
// RECYCLE's shaders (see recycle.cpp).
//
// TABLE_FS draws the surface seen from above (a matte table, its tray for the well, a fine grain) and darkens it
// with the cubes' shadows, rendered first into a coverage target (SHADOW_FS, max-blended so overlapping shadows of
// one light never double). CUBE_FS draws everything else on the table as instanced quads: the cubes (a flat top
// face with softly rounded edges, lit from the upper left; matte, glossy or wooden), the soft contact shadow a cube
// casts on the cubes under it, the flat printed silhouettes of the previews and the ghost's outline. Colors are
// linear; the post pass encodes them.

namespace recycle_shaders {

// ---------------------------------------------------------------- the table
inline const char* TABLE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uShadow;
uniform vec4 uView;          // center, half extents (board units)
uniform vec2 uRes;
uniform vec3 uTable, uTray, uShadowTint;
uniform float uShadowAmt, uMottle, uGrain, uLamp, uTrayDepth, uDark, uWell;
uniform vec2 uLight;         // toward the light (board plane)
uniform float uBridge;
uniform vec3 uBridgeCol;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)), d = hash(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; }
    return s;
}
vec2 hash2(vec2 p) { return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453); }
// Terrazzo: small irregular stone chips set in the surface (sparse, muted), at one scale (chips per board unit).
vec3 chips(vec2 p, vec3 c, float scale, float rmax, float px, float amt) {
    vec2 g = p * scale, i = floor(g);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 cell = i + vec2(x, y);
            vec2 h = hash2(cell), h2 = hash2(cell + 41.7);
            if (h2.x > 0.38) continue;
            vec2 q = g - cell - h;
            float r = rmax * (0.35 + 0.65 * h2.y * h2.y);
            float a = atan(q.y, q.x);
            float d = length(q) * (1.0 + 0.12 * sin(a * 3.0 + h.x * 6.28) + 0.06 * sin(a * 5.0 + h.y * 6.28));
            float m = 1.0 - smoothstep(r - px * scale, r + px * scale, d);
            float k = fract(h.x * 7.13 + h.y * 3.71);
            vec3 stone = k < 0.3 ? vec3(0.42, 0.20, 0.13) : k < 0.55 ? vec3(0.25, 0.29, 0.22) : k < 0.8 ? vec3(0.13, 0.12, 0.11) : vec3(0.92, 0.90, 0.87);
            c = mix(c, stone, m * amt);
        }
    return c;
}
// Signed distance to a rectangle.
float boxSd(vec2 p, vec2 h) { vec2 q = abs(p) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }

void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec3 c = uTable;
    // a broad, still light from the upper left (never moving)
    c *= 1.0 + uLamp * (0.5 - length((p - vec2(-14.0, 16.0)) / vec2(62.0, 44.0)));
    // concrete's mottling / the fine tooth of the surface
    float px0 = 2.0 * uView.w / uRes.y;
    if (uMottle > 0.0) {
        // terrazzo: a soft tonal drift and sparse chips of stone, quieter in the tray (the stack stays clean)
        // (quiet in the middle band too: the tray and the HUD's columns stay clean)
        float mid = 1.0 - smoothstep(11.0, 12.5, abs(p.x));
        c *= 1.0 + 0.06 * uMottle * (fbm(p * 0.3) - 0.5);
        float amt = uMottle * mix(1.0, 0.3, mid);
        c = chips(p, c, 1.3, 0.13, px0, 0.38 * amt);
        c = chips(p + 13.1, c, 3.6, 0.11, px0, 0.3 * amt);
    }
    // the tray: a shallow recess the well's cubes rest in; its upper-left walls shade the floor
    float sd = boxSd(p, vec2(5.12, 10.12));
    float px = 2.0 * uView.w / uRes.y;
    float inside = (1.0 - smoothstep(-px, px, sd)) * uWell;
    vec3 tray = uTray * (1.0 + uLamp * 0.5 * (0.5 - length((p - vec2(-14.0, 16.0)) / vec2(62.0, 44.0))));
    // the walls on the light's side cast their shade inward
    vec2 w = max(vec2(5.12 - abs(p.x), 10.12 - abs(p.y)), 0.0);
    float sideX = (sign(p.x) == sign(uLight.x)) ? 1.0 : 0.2;
    float sideY = (sign(p.y) == sign(uLight.y)) ? 1.0 : 0.2;
    float wsh = max(sideX * exp(-w.x / 0.32), sideY * exp(-w.y / 0.32));
    tray *= 1.0 - uTrayDepth * 0.55 * wsh;
    c = mix(c, tray, inside);
    // the rim: a hairline catching the light on the far walls, a fine dark line on the near ones
    float rim = 1.0 - smoothstep(0.0, 1.2 * px, abs(sd));
    float lit = clamp(0.5 - 0.5 * dot(normalize(p * vec2(1.0, 0.5) + 1e-4), uLight), 0.0, 1.0);
    c = mix(c, c * mix(0.78, 1.18, lit), rim * uTrayDepth * uWell);
    // shadows of the cubes
    float s = texture(uShadow, vUv).r;
    c = mix(c, c * uShadowTint, clamp(s * uShadowAmt, 0.0, 1.0));
    // grain
    c *= 1.0 + uGrain * (hash(floor(vUv * uRes)) - 0.5);
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- instances (shadows and cubes)
// a: x, y, half size, rotation; b: rgb, alpha; c: kind, height, seed, mask; d: gloss, wood, squash, edge.
inline const char* INST_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;
layout(location = 2) in vec4 aB;
layout(location = 3) in vec4 aC;
layout(location = 4) in vec4 aD;
uniform mat4 uVP;
uniform float uPad;   // extra margin (board units) for soft edges
uniform float uBlurPad; // 1: the table shadows (d.z is their blur, added to the margin)
out vec2 vLocal;      // the instance's own frame, its square spanning -0.5..0.5
out vec2 vDir;        // the same point's offset in board axes (unrotated), in instance units
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float ext = aA.z + uPad + aD.z * uBlurPad;
    vec2 off = aPos * ext;
    float cr = cos(aA.w), sr = sin(aA.w);
    vec2 wp = aA.xy + vec2(cr * off.x - sr * off.y, sr * off.x + cr * off.y);
    vLocal = off / (2.0 * aA.z);
    vDir = vec2(cr * vLocal.x - sr * vLocal.y, sr * vLocal.x + cr * vLocal.y);
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// A cube's shadow on the table: a rounded square blurred with its height (coverage, max-blended).
inline const char* SHADOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
float rbox(vec2 p, float h, float r) { vec2 k = abs(p) - (h - r); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - r; }
void main() {
    float blur = vD.z / (2.0 * vA.z) + 0.02;  // in cube units
    float sd = rbox(vLocal, 0.5 - 0.04, 0.12);
    float a = 1.0 - smoothstep(-blur, blur, sd);
    frag = vec4(a * vB.y, 0.0, 0.0, 1.0);
}
)";

inline const char* CUBE_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vDir;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec2 uLight;      // toward the light, board plane
uniform float uLift;      // brighter with height (the light is above)
uniform float uLineW;     // pixels
uniform vec3 uSpec;       // the light's color in highlights
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float rbox(vec2 p, float h, float r) { vec2 k = abs(p) - (h - r); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - r; }
// The outline of a piece's silhouette (cell units): its neighbors of the same piece (mask bits: left right down
// up, down-left down-right up-left up-right) extend it.
float shapeSdf(vec2 p, int m) {
    const float g = 0.05, r = 0.16, rc = 0.06;
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
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i), b = hash(i + vec2(1, 0)), c = hash(i + vec2(0, 1)), d = hash(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    int kind = int(vC.x + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4); // instance units per pixel
    vec3 base = mix(vB.rgb, uBridgeInk, uBridge);
    if (kind == 1) {
        // the soft shadow a cube casts on the cubes under it
        float sd = rbox(vLocal, 0.5, 0.14);
        float a = (1.0 - smoothstep(-0.14, 0.2, sd)) * vB.a;
        frag = vec4(0.0, 0.0, 0.0, a);
        return;
    }
    if ((kind == 2 || kind == 3) && max(abs(vLocal.x), abs(vLocal.y)) > 0.5) discard; // each cell draws its own part
    if (kind == 2) {
        // a preview: the piece's silhouette printed flat on the table
        float sd = shapeSdf(vLocal, int(vC.w + 0.5));
        float a = (1.0 - smoothstep(-aa, aa, sd)) * vB.a;
        frag = vec4(base * a, a);
        return;
    }
    if (kind == 3) {
        // the ghost: the silhouette's outline, a fine line on the tray
        float sd = shapeSdf(vLocal, int(vC.w + 0.5));
        float lw = uLineW * aa;
        float band = 1.0 - smoothstep(lw * 0.5, lw * 0.5 + aa, abs(sd + lw));
        float a = band * vB.a;
        frag = vec4(base * a, a);
        return;
    }
    // a cube seen from above: its top face, the rounded edges catching the light on one side
    float sq = vD.z;
    vec2 p = vLocal / (1.0 + sq);
    float sd = rbox(p, 0.5 - 0.035, 0.11);
    float fill = 1.0 - smoothstep(-aa, aa, sd);
    if (fill <= 0.0) discard;
    vec3 col = base;
    // the face: a faint gradient toward the light, brighter as it rises
    col *= 1.0 + 0.07 * dot(vDir, uLight) + uLift * clamp(vC.y / 3.0, 0.0, 1.0);
    // wood: a fine grain along the cube, its own for every cube
    if (vD.y > 0.0) {
        float s = vC.z * 13.7;
        vec2 g = vec2(p.x, p.y * 0.12) + vec2(s, s * 0.37);
        float w = vnoise(g * vec2(3.0, 9.0)) * 2.2;
        float rings = sin((p.x + 0.3 * sin(p.y * 3.0 + s)) * 22.0 + w * 3.0 + s);
        col *= 1.0 + vD.y * (0.032 * rings + 0.05 * (vnoise(g * vec2(2.0, 40.0)) - 0.5));
    }
    // the rounded edges: lit on the light's side, shaded on the other
    float edge = smoothstep(-0.11, -0.005, sd);
    vec2 n = vDir;
    n = normalize(n * (1.0 + 4.0 * smoothstep(0.25, 0.5, abs(n))) + 1e-4);
    float lit = dot(n, uLight);
    col *= 1.0 + edge * vD.w * (0.22 * lit - 0.10);
    // gloss: a soft highlight toward the light and a bright lip on the lit edges
    if (vD.x > 0.0) {
        float h = smoothstep(0.55, 0.0, length(vDir - uLight * 0.3));
        col += uSpec * vD.x * (0.10 * h * h + 0.35 * edge * max(lit, 0.0) * smoothstep(-0.05, -0.02, sd - 0.0) );
        col += uSpec * vD.x * 0.06 * smoothstep(0.2, 0.5, dot(vDir, uLight));
    }
    float a = fill * vB.a;
    frag = vec4(col * a, a);
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

} // namespace recycle_shaders

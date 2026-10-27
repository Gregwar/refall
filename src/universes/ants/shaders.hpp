#pragma once
// ANTS's shaders (see ants.cpp). Everything is drawn flat, top down, in display (sRGB) colors.
namespace ants_shaders {

inline const char* COMMON_GLSL = R"(
float hash1(float n) { return fract(sin(n) * 43758.5453123); }
float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123); }
vec2 hash22(vec2 p) {
    return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i), hash2(i + vec2(1, 0)), u.x), mix(hash2(i + vec2(0, 1)), hash2(i + vec2(1, 1)), u.x), u.y);
}
float sdRBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
float sdEll(vec2 p, vec2 r) {
    float k0 = length(p / r), k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / max(k1, 1e-5);
}
)";

// ---------------------------------------------------------------- the ground

// The ground: a plain tone with a fine static grain and a very soft mottling, the worn paths of the trails, the two
// nests (a dark hole in a ring of dug-out pellets) and the well's hairline frame.
inline const char* GROUND_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center x, y, half w, half h (board units)
uniform float uPx;           // board units per pixel
uniform vec3 uGround, uMottle, uTrail, uHole, uMound, uFrame;
uniform float uGrain, uTrailAmt, uWorld, uFrameA;
uniform sampler2D uPher;
uniform vec4 uPherRect;      // x0, y0, w, h
uniform vec4 uNest0, uNest1; // x, y, radius, -
uniform vec3 uBridgeCol;
uniform float uBridge;
uniform float uLight;        // light ground (worn paths darker) or dark (paths paler)

vec3 nest(vec3 col, vec2 p, vec4 n) {
    vec2 d = p - n.xy;
    float r = length(d), R = n.z;
    float ang = atan(d.y, d.x);
    // the mound of dug-out soil: a soft ring, a few crumbs on it
    float wob = 1.0 + 0.08 * sin(ang * 3.0 + n.y) + 0.05 * sin(ang * 7.0 + n.x);
    float rr = r / wob;
    float ring = smoothstep(R * 1.75, R * 0.6, rr);
    col = mix(col, uMound, ring * 0.55);
    vec2 g = p * 6.0;
    vec2 cell = floor(g);
    float best = 9.0, id = 0.0, sz = 0.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 c = cell + vec2(i, j);
            vec2 o = c + 0.25 + 0.5 * hash22(c);
            float dd = length(g - o);
            if (dd < best) { best = dd; id = hash2(c * 1.7); sz = hash2(c * 2.3); }
        }
    float crumb = smoothstep(0.12 + 0.12 * sz, 0.08 + 0.12 * sz, best) * step(1.0 - ring * 0.5, id);
    col = mix(col, uMound * (0.82 + 0.3 * sz), crumb * 0.8);
    // the hole: dark, its rim falling away inside
    float hr = R * 0.36;
    float hole = smoothstep(hr + uPx * 1.5, hr - uPx * 1.5, rr);
    float inner = smoothstep(hr * 1.5, hr * 0.6, rr);
    col = mix(col, uHole, inner * 0.25);
    col = mix(col, uHole, hole);
    return col;
}

void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec3 col = uGround;
    // a very soft mottling, large and low (never a pattern)
    float m = vnoise(p * 0.18) * 0.6 + vnoise(p * 0.5 + 7.3) * 0.4;
    col = mix(col, uMottle, smoothstep(0.25, 0.85, m) * 0.6);
    // the worn paths
    vec2 tq = (p - uPherRect.xy) / uPherRect.zw;
    float t = texture(uPher, tq).r;
    float path = smoothstep(0.16, 0.75, t) * uTrailAmt * uWorld;
    col = mix(col, uTrail, path);
    // nests
    vec3 withNests = nest(nest(col, p, uNest0), p, uNest1);
    col = mix(col, withNests, uWorld);
    // the well: a hairline frame
    float fx = abs(abs(p.x) - 5.0), fy = abs(p.y + 10.0);
    float lw = max(uPx * 0.6, 0.012);
    float fr = 0.0;
    if (p.y > -10.0 && p.y < 10.0) fr = max(fr, 1.0 - smoothstep(lw, lw + uPx, fx));
    if (abs(p.x) < 5.0) fr = max(fr, 1.0 - smoothstep(lw, lw + uPx, fy));
    col = mix(col, uFrame, fr * uFrameA);
    col = mix(col, uBridgeCol, uBridge);
    // fine grain, static (on the pixel grid)
    float gr = hash2(floor(gl_FragCoord.xy)) - 0.5;
    col += gr * uGrain;
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- sprites

inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA; // x, y, half w, half h (board units)
layout(location = 2) in vec4 aB; // angle, kind, seed, alpha
layout(location = 3) in vec4 aC; // rgb, p0
layout(location = 4) in vec4 aD; // p1 .. p4
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vB, vC, vD;
flat out vec2 vH;
void main() {
    vec2 l = aCorner * aA.zw;
    float c = cos(aB.x), s = sin(aB.x);
    vec2 w = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    gl_Position = uVP * vec4(w, 0.0, 1.0);
    vL = l;
    vB = aB; vC = aC; vD = aD; vH = aA.zw;
}
)";

// Kinds: 0 a sugar cube (p0 nibbled, p1 half size, p2 glint, p3 ghost, p4 crumbled), 1 a soft shadow (p1 half size,
// p2 blur), 2 an ant (p1 length, p2 gait phase, p3 carrying, p0 lustre), 3 a grain of sugar (p1 size),
// 4 a soft round dot (p1 radius, p2 ring width: 0 a disc).
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vB, vC, vD;
flat in vec2 vH;
out vec4 frag;
uniform vec3 uAnt;
uniform float uPx;
uniform vec3 uLightDir;
uniform float uBridge;
uniform vec3 uBridgeInk;
uniform float uSparkle;

vec4 cube(vec2 q) {
    float h = vD.x;
    float nib = vC.w;
    float seed = vB.z;
    float fw = max(fwidth(q.x), 1e-4);
    float d = sdRBox(q, vec2(h), h * 0.14);
    // nibbled: a few bites along the edges and corners, growing with the nibbling, never past a quarter of the cube
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        float grow = clamp((nib - fi * 0.15) / 0.25, 0.0, 1.0);
        if (grow <= 0.0) break;
        vec2 r = vec2(hash1(seed * 3.1 + fi * 7.7), hash1(seed * 5.3 + fi * 3.9));
        float side = floor(r.x * 4.0);
        float t = (r.y * 2.0 - 1.0) * h;
        if (fi < 2.0) t = sign(r.y - 0.5) * h; // the first bites at corners
        vec2 c = side < 1.0 ? vec2(t, h) : side < 2.0 ? vec2(h, t) : side < 3.0 ? vec2(t, -h) : vec2(-h, t);
        float rad = h * (0.12 + 0.13 * hash1(seed + fi * 1.3)) * grow;
        // a crumbly bite edge
        float a = atan(q.y - c.y, q.x - c.x);
        rad *= 1.0 + 0.18 * sin(a * 7.0 + fi * 2.0 + seed);
        d = max(d, rad - length(q - c));
    }
    // the sugar's grains: small crystals, each a facet of its own
    vec2 g = q / h * 4.6 + seed * 13.0;
    vec2 cell = floor(g);
    float f1 = 9.0, f2 = 9.0;
    vec2 id = vec2(0.0);
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 c = cell + vec2(i, j);
            vec2 o = c + 0.15 + 0.7 * hash22(c);
            float dd = length(g - o);
            if (dd < f1) { f2 = f1; f1 = dd; id = c; }
            else if (dd < f2) f2 = dd;
        }
    float hId = hash2(id + 0.37);
    float crumbled = vD.w;
    // crumbling (a cleared cube): grain by grain, from the outside in
    float edgeK = clamp(-d / h, 0.0, 1.0);
    if (crumbled > 0.0 && hId * 0.7 + edgeK * 0.5 < crumbled * 1.25) d = 1.0;
    vec3 n = normalize(vec3(hash22(id + 3.1) * 2.0 - 1.0, 2.4));
    float lit = dot(n, uLightDir);
    vec3 tint = vC.rgb;
    // the body: a soft light across the cube's top, each crystal a touch brighter or darker
    float across = dot(q / h, normalize(uLightDir.xy)) * 0.5;
    vec3 col = tint * (0.9 + 0.1 * across + 0.12 * (lit - 0.8) + 0.05 * (hId - 0.5));
    col *= 0.96 + 0.04 * smoothstep(0.0, 0.1, f2 - f1);
    // the edges catch the light (translucent sugar)
    float rim = smoothstep(-h * 0.16, 0.0, d);
    col = mix(col, mix(tint, vec3(1.0), 0.55), rim * 0.4);
    // sparkles: a few facets turned to the light
    float sp = step(0.8, hash2(id + 9.1)) * pow(max(lit, 0.0), 6.0);
    col += vec3(1.0) * sp * (0.3 + 0.6 * vD.y + 0.35 * uSparkle) * smoothstep(0.3, 0.0, f1);
    col = mix(col, uBridgeInk, uBridge);
    float a = clamp(0.5 - d / fw, 0.0, 1.0);
    if (vD.z > 0.5) {
        // the ghost: the cube's outline, a breath of its color inside
        float line = clamp(0.5 - (abs(d + fw * 1.2) - fw * 0.9) / fw, 0.0, 1.0);
        return vec4(mix(tint, uBridgeInk, uBridge), 1.0) * (line * 0.75 + a * 0.1);
    }
    return vec4(col, 1.0) * a;
}

vec4 ant(vec2 l) {
    float L = vD.x;
    vec2 p = l / L;
    float fw = max(fwidth(p.x), 1e-4);
    float gait = vD.y;
    float lw = max(0.022, fw * 0.55);
    // gaster, petiole, thorax, head
    float d = sdEll(p - vec2(-0.27, 0.0), vec2(0.2, 0.135));
    d = min(d, length(p - vec2(-0.075, 0.0)) - 0.035);
    d = min(d, sdEll(p - vec2(0.04, 0.0), vec2(0.12, 0.058)));
    d = min(d, sdEll(p - vec2(0.255, 0.0), vec2(0.095, 0.085)));
    // six legs, two segments each, walking in tripods
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        vec2 hip = vec2(0.09 - fi * 0.05, 0.0);
        vec2 knee0 = i == 0 ? vec2(0.12, 0.13) : i == 1 ? vec2(0.02, 0.15) : vec2(-0.06, 0.135);
        vec2 foot0 = i == 0 ? vec2(0.24, 0.22) : i == 1 ? vec2(0.0, 0.28) : vec2(-0.25, 0.25);
        for (int s = 0; s < 2; s++) {
            float sg = s == 0 ? 1.0 : -1.0;
            float ph = gait + ((i + s) % 2 == 0 ? 0.0 : 3.14159);
            float sw = sin(ph);
            float lift = max(cos(ph), 0.0);
            vec2 knee = vec2(knee0.x + sw * 0.03, sg * knee0.y);
            vec2 foot = vec2(foot0.x + sw * 0.075, sg * (foot0.y - lift * 0.025));
            d = min(d, sdSeg(p, hip, knee) - lw);
            d = min(d, sdSeg(p, knee, foot) - lw * 0.8);
        }
    }
    // antennae: elbowed, feeling ahead
    for (int s = 0; s < 2; s++) {
        float sg = s == 0 ? 1.0 : -1.0;
        float w = sin(gait * 0.5 + sg * 1.3) * 0.02;
        vec2 b = vec2(0.31, sg * 0.03), e = vec2(0.38, sg * 0.13), t = vec2(0.5 + w, sg * (0.1 + w));
        d = min(d, sdSeg(p, b, e) - lw * 0.75);
        d = min(d, sdSeg(p, e, t) - lw * 0.7);
    }
    float a = clamp(0.5 - d / fw, 0.0, 1.0);
    // a faint lustre on the gaster
    float lus = smoothstep(0.1, 0.0, length((p - vec2(-0.3, 0.045)) / vec2(1.0, 0.6))) * vC.w;
    vec3 col = uAnt + vec3(lus);
    vec4 o = vec4(col, 1.0) * a;
    // the crumb it carries
    if (vD.z > 0.5) {
        vec2 c = p - vec2(0.42, 0.0);
        vec2 r = vec2(c.x + c.y, c.y - c.x) * 0.7071;
        float dc = sdRBox(r, vec2(0.075), 0.02);
        float ac = clamp(0.5 - dc / fw, 0.0, 1.0);
        vec3 cc = vC.rgb * (r.x + r.y > 0.0 ? 1.08 : 0.86);
        o = mix(o, vec4(cc, 1.0), ac);
    }
    return o;
}

void main() {
    float kind = vB.y;
    float alpha = vB.w;
    vec4 o;
    if (kind < 0.5) {
        o = cube(vL);
    } else if (kind < 1.5) {
        float h = vD.x, blur = vD.y;
        float d = sdRBox(vL, vec2(h), h * 0.2);
        o = vec4(vC.rgb, 1.0) * (1.0 - smoothstep(-blur, blur, d));
    } else if (kind < 2.5) {
        o = ant(vL);
    } else if (kind < 3.5) {
        float s = vD.x;
        float fw = max(fwidth(vL.x), 1e-4);
        vec2 r = vec2(vL.x + vL.y, vL.y - vL.x) * 0.7071;
        float d = sdRBox(vL, vec2(s), s * 0.25);
        float shade = vL.x - vL.y > 0.0 ? 0.88 : 1.08;
        vec3 col = mix(vC.rgb * shade, uBridgeInk, uBridge);
        o = vec4(col, 1.0) * clamp(0.5 - d / fw, 0.0, 1.0);
    } else {
        float r = length(vL), R = vD.x, ring = vD.y;
        float fw = max(fwidth(vL.x), 1e-4);
        float d = ring > 0.0 ? abs(r - R) - ring : r - R;
        o = vec4(mix(vC.rgb, uBridgeInk, uBridge * vC.w), 1.0) * clamp(0.5 - d / fw, 0.0, 1.0);
    }
    frag = o * alpha;
}
)";

// ---------------------------------------------------------------- text and the veil

inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
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
uniform vec3 uColor, uHalo;
uniform float uAlpha, uHaloAmt, uBias;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.5 - uBias - w, 0.5 - uBias + w, d);
    float halo = smoothstep(0.12, 0.5, d);
    halo = halo * halo * uHaloAmt;
    float a = fill + (1.0 - fill) * halo;
    vec3 col = uColor * fill + uHalo * halo * (1.0 - fill);
    frag = vec4(col * uAlpha, a * uAlpha);
}
)";

// The HUD's rectangles (pixels, top-left origin; premultiplied color).
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

inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uVeil; // rgb, alpha
void main() { frag = vec4(uVeil.rgb * uVeil.a, uVeil.a); }
)";

} // namespace ants_shaders

#pragma once
// ELECTRON's shaders (see electron.cpp). Everything is drawn flat, top down, in display (sRGB) colors.
namespace electron_shaders {

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
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
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
)";

// ---------------------------------------------------------------- the ground

// The substrate: a dark, softly textured slab (a slow fbm mottling, a finer grain of fibres, a fine static grain,
// a vignette), the connections the charges have worn (a glow, and a thin hot filament along the ridge of the field),
// the two terminals (a dark disc with a ring and a glowing core) and the well's hairline frame. The well's own panel
// stays calm: its texture is mostly smoothed out and the connections show only faintly through it.
inline const char* GROUND_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center x, y, half w, half h (board units)
uniform float uPx;           // board units per pixel
uniform vec3 uGround, uMottle, uTrace, uCore, uTerm, uCharge, uFrame, uPanel;
uniform float uGrain, uTraceAmt, uWorld, uFrameA, uTime, uTermGlow, uWell;
uniform sampler2D uPher;
uniform vec4 uPherRect;      // x0, y0, w, h
uniform vec2 uPherTexel;     // one texel in uv
uniform vec4 uNest0, uNest1; // x, y, radius, -
uniform vec3 uBridgeCol;
uniform float uBridge;

vec3 terminal(vec3 col, vec2 p, vec4 n) {
    vec2 d = p - n.xy;
    float r = length(d), R = n.z;
    float fw = uPx * 1.2;
    // a dark plate, a little raised (its rim catches a line of light)
    float plate = smoothstep(R * 0.95 + fw, R * 0.95 - fw, r);
    col = mix(col, uTerm, plate * 0.9);
    float rim = 1.0 - smoothstep(0.0, fw * 1.5, abs(r - R * 0.95));
    col += uCharge * rim * 0.12;
    // two thin rings and a ring of contact pads
    for (int i = 0; i < 2; i++) {
        float rr = R * (0.72 - 0.16 * float(i));
        col += uCharge * (1.0 - smoothstep(0.0, fw * 1.2, abs(r - rr))) * (0.1 + 0.08 * uTermGlow);
    }
    float ang = atan(d.y, d.x);
    float pads = 16.0;
    float sa = (fract(ang / 6.28318 * pads) - 0.5) * 6.28318 / pads * R * 0.84;
    float pad = sdRBox(vec2(sa, r - R * 0.84), vec2(R * 0.035, R * 0.05), R * 0.01);
    col = mix(col, uTerm * 1.6 + uCharge * 0.06, smoothstep(fw, -fw, pad) * 0.8);
    // the core: hot in the middle, its glow spilling out a little (brighter with the colony's surge)
    float core = R * 0.3;
    float disc = smoothstep(core + fw, core - fw, r);
    float glow = exp(-r * r / (core * core * 2.5));
    col = mix(col, uCharge * 0.35, disc * 0.8);
    col += uCharge * glow * (0.25 + 0.45 * uTermGlow);
    col += vec3(1.0) * exp(-r * r / (core * core * 0.15)) * (0.35 + 0.35 * uTermGlow);
    // and a soft light on the substrate around the plate
    col += uCharge * exp(-max(r - R, 0.0) / (R * 0.5)) * (1.0 - plate) * 0.05 * (1.0 + uTermGlow);
    return col;
}

float field(vec2 q) { return texture(uPher, q).r; }

void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // inside the well (soft edge): the panel, calm and even
    float inWell = smoothstep(0.05, -0.05, max(abs(p.x) - 5.0, max(-10.0 - p.y, p.y - 10.0))) * uWell;
    // the substrate's texture: a slow mottling and finer fibres (soft, never a pattern)
    float m = fbm(p * 0.22 + 3.1);
    float fib = fbm(vec2(p.x * 1.6 + p.y * 0.4, p.y * 3.5 - p.x * 0.9) + 11.0);
    vec3 col = mix(uGround, uMottle, smoothstep(0.3, 0.75, m));
    col *= 1.0 + (fib - 0.5) * 0.16 * (1.0 - 0.8 * inWell);
    col = mix(col, uPanel, inWell * 0.85);
    // a vignette toward the picture's edges
    vec2 vq = (p - uView.xy) / uView.zw;
    col *= 1.0 - 0.28 * smoothstep(0.55, 1.45, length(vq * vec2(0.85, 1.0)));

    // the connections
    vec2 tq = (p - uPherRect.xy) / uPherRect.zw;
    float t = field(tq);
    vec2 e = uPherTexel * 1.3;
    float lap = field(tq + vec2(e.x, 0.0)) + field(tq - vec2(e.x, 0.0)) + field(tq + vec2(0.0, e.y)) +
                field(tq - vec2(0.0, e.y)) - 4.0 * t;
    float ridge = clamp(-lap / (t * 0.22 + 0.015), 0.0, 1.0);
    float on = smoothstep(0.1, 0.55, t);
    // the current running along them: a slow shimmer drifting through the filament (local, never the whole scene)
    float flow = 0.75 + 0.25 * vnoise(p * 5.0 + vec2(uTime * 2.3, -uTime * 1.7));
    float amt = uTraceAmt * uWorld * (1.0 - 0.6 * inWell);
    vec3 glowC = uTrace * smoothstep(0.04, 0.9, t) * 0.24;
    float wire = ridge * on * flow;
    col += glowC * amt;
    col = mix(col, uTrace, min(wire * 0.75 * amt, 0.8));
    col = mix(col, uCore, min(wire * smoothstep(0.6, 1.4, t) * 0.5 * amt, 0.55));

    // terminals
    vec3 withT = terminal(terminal(col, p, uNest0), p, uNest1);
    col = mix(col, withT, uWorld);
    // the well: a hairline frame
    float fx = abs(abs(p.x) - 5.0), fy = abs(p.y + 10.0);
    float lw = max(uPx * 0.6, 0.012);
    float fr = 0.0;
    if (p.y > -10.0 && p.y < 10.0) fr = max(fr, 1.0 - smoothstep(lw, lw + uPx, fx));
    if (abs(p.x) < 5.0) fr = max(fr, 1.0 - smoothstep(lw, lw + uPx, fy));
    col = mix(col, uFrame, fr * uFrameA * uWell);
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
flat out float vY; // the sprite's center height (board units)
void main() {
    vec2 l = aCorner * aA.zw;
    float c = cos(aB.x), s = sin(aB.x);
    vec2 w = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    gl_Position = uVP * vec4(w, 0.0, 1.0);
    vL = l;
    vB = aB; vC = aC; vD = aD; vH = aA.zw;
    vY = aA.y;
}
)";

// Kinds: 0 a block, a bare die (p0 charge: its edges lit, p1 half size, p3 ghost, p4 shattered), 1 a soft shadow (p1 half
// size, p2 blur), 2 a charge (p1 radius, p2 tail length, p3 carrying, p0 flicker), 3 a fragment (p1 size, p0 glow),
// 4 a soft round dot (p1 radius, p2 ring width: 0 a disc), 5 a spark streak (p1 length, p2 width, p0 heat),
// 6 a flash (p1 radius).
// Glows are written with less alpha than color, so they add light (the blending is premultiplied).
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vB, vC, vD;
flat in vec2 vH;
flat in float vY;
out vec4 frag;
uniform float uPx;
uniform vec3 uLightDir;
uniform float uBridge;
uniform vec3 uBridgeInk;
uniform vec3 uRim;           // the color of a cube's edges when a charge touches it
uniform float uPower;        // a DROP: the dies powered, their circuitry glowing in uRim
uniform float uWave;         // the height of the wave of current running up the stack (board units)

// A bare silicon die's top (u: -1..1 across it; lit: the chamfer's light, 1 on the flat): the piece's color shimmering
// with the angle of view, a fine grid of circuitry, a border of gold bond pads.
vec3 dieTop(vec2 u, float seed, vec3 base, float lit, float px) {
    vec3 col = base * (0.7 + 0.25 * lit);
    float sh = 0.5 + 0.5 * sin((u.x + u.y) * 3.0 + seed * 6.0);
    col = mix(col, col * vec3(0.85, 1.05, 1.15), sh * 0.35);
    vec2 g = u * 7.0;
    vec2 lf = abs(fract(g) - 0.5);
    float lines = smoothstep(0.06 + px * 7.0, 0.06 - px * 7.0, min(lf.x, lf.y)) * step(0.35, hash2(floor(g) + seed));
    float inner = step(sdRBox(u, vec2(0.66), 0.02), 0.0);
    col *= 1.0 - 0.18 * lines * inner;
    float rim = step(0.0, sdRBox(u, vec2(0.7), 0.02)) * step(sdRBox(u, vec2(0.9), 0.04), 0.0);
    vec2 pd = abs(fract(u * 3.5) - 0.5);
    float pad = rim * smoothstep(0.26 + px * 3.5, 0.26 - px * 3.5, max(pd.x, pd.y));
    col = mix(col, vec3(0.95, 0.76, 0.36) * (0.6 + 0.5 * lit), pad);
    // powered (a DROP): a wave of current runs up the stack, the die it passes lit up (its circuitry and pads in the
    // charges' color, the whole top a little brighter), a faint glow left in its circuitry between two waves
    float wv = (vY - uWave) / 1.8;
    float wave = exp(-wv * wv) * uPower;
    col *= 1.0 + 0.35 * wave;
    return col + uRim * (lines * inner * (0.3 * uPower + 2.4 * wave) + pad * (0.2 * uPower + 1.5 * wave) + 0.12 * wave);
}

// A block: a bare silicon die (dieTop) seen from above and a little in front, its top with a narrow chamfer lit from
// the upper left, a shaded front wall below it.
vec4 cube(vec2 q) {
    float h = vD.x;
    float seed = vB.z;
    float fw = max(fwidth(q.x), 1e-4);
    float wall = h * 0.16;
    float d = sdRBox(q, vec2(h), h * 0.06);
    float ds = sdRBox(q - vec2(0.0, -wall * 0.5), vec2(h, h + wall * 0.5), h * 0.06);
    if (vD.z > 0.5) {
        // the ghost: the footprint's outline, the top's edge fainter, a breath of its color inside
        vec3 tint = mix(vC.rgb, uBridgeInk, uBridge);
        float line = clamp(0.5 - (abs(ds + fw * 1.2) - fw * 0.9) / fw, 0.0, 1.0);
        float lineT = clamp(0.5 - (abs(d + fw * 0.6) - fw * 0.5) / fw, 0.0, 1.0) * step(ds, -fw * 2.0);
        float a = clamp(0.5 - ds / fw, 0.0, 1.0);
        return vec4(tint, 1.0) * (line * 0.7 + lineT * 0.3 + a * 0.08);
    }
    // shattering (a cleared cube): sharp shards drop out from the outside in
    float shat = vD.w;
    if (shat > 0.0) {
        vec2 g = q / h * 2.6 + seed * 7.0;
        vec2 cell = floor(g);
        float f1 = 9.0;
        vec2 id = vec2(0.0);
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                vec2 c = cell + vec2(i, j);
                vec2 o = c + 0.1 + 0.8 * hash22(c);
                float dd = length(g - o);
                if (dd < f1) { f1 = dd; id = c; }
            }
        float edgeK = clamp(-ds / h, 0.0, 1.0);
        if (hash2(id + 0.37) * 0.6 + edgeK * 0.7 < shat * 1.3) return vec4(0.0);
    }
    vec3 L = uLightDir;
    vec3 base = vC.rgb;
    // the top: flat, a faint gradient from the light, the chamfer catching it or not
    float bev = h * 0.11;
    float onBev = 1.0 - smoothstep(-0.5, 0.5, (-d - bev) / max(fw, bev * 0.1));
    vec2 out2 = vec2(0.0);
    {
        float e = h * 0.02;
        vec2 gr = vec2(sdRBox(q + vec2(e, 0.0), vec2(h), h * 0.06) - d, sdRBox(q + vec2(0.0, e), vec2(h), h * 0.06) - d) / e;
        float gl = length(gr);
        out2 = gl > 1e-4 ? gr / gl : vec2(0.0);
    }
    vec3 n = normalize(mix(vec3(0.0, 0.0, 1.0), vec3(out2 * 0.9, 1.0), onBev));
    float dif = max(dot(n, L), 0.0) / L.z;
    vec3 col = dieTop(q / h, seed, base, mix(1.0, 0.55 + 0.6 * dif, onBev), fw / h);
    col += vec3(1.0) * pow(max(reflect(-L, n).z, 0.0), 30.0) * 0.22 * onBev;
    // the front wall: in the shade
    float onTop = clamp(0.5 - d / fw, 0.0, 1.0);
    float wy = clamp((-h - q.y) / wall, 0.0, 1.0);
    vec3 wcol = base * (0.48 - 0.12 * wy);
    col = mix(wcol, col, onTop);
    // charged: its edges lit where the charges touch it
    float charge = vC.w;
    if (charge > 0.0) {
        float edge = 1.0 - smoothstep(0.0, h * 0.12, -ds);
        col = mix(col, uRim, edge * charge * 0.8);
        col += uRim * (1.0 - smoothstep(0.0, h * 0.5, -ds)) * charge * 0.12;
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = clamp(0.5 - ds / fw, 0.0, 1.0);
    return vec4(col, 1.0) * a;
}

// A charge: a hot point with a halo and a short tail behind it (it moves toward +x).
vec4 charge(vec2 p) {
    float r = vD.x, tail = vD.y;
    float d = length(p);
    float core = smoothstep(r, r * 0.35, d);
    float ds = sdSeg(p, vec2(0.0), vec2(-tail, 0.0));
    float along = clamp(-p.x / max(tail, 1e-3), 0.0, 1.0);
    float tl = exp(-ds * ds / (r * r * 0.35)) * (1.0 - along) * (1.0 - along) * step(p.x, 0.0);
    float halo = exp(-d * d / (r * r * 7.0));
    float fl = 1.0 - 0.35 * abs(vC.w);
    float carry = vD.z;
    vec3 c = vC.rgb;
    vec3 hot = mix(c, vec3(1.0), 0.2 + 0.55 * carry);
    vec3 col = hot * core + c * (tl * (0.55 + 0.25 * carry) + halo * (0.35 + 0.25 * carry));
    col *= 1.0 - 0.35 * (1.0 - carry) * core;
    float a = core * 0.85 + tl * 0.15;
    return vec4(col * fl, a);
}

void main() {
    float kind = vB.y;
    float alpha = vB.w;
    vec4 o;
    if (kind < 0.5) {
        o = cube(vL);
    } else if (kind < 1.5) {
        float h = vD.x, blur = vD.y;
        float d = sdRBox(vL, vec2(h), h * 0.12);
        o = vec4(vC.rgb, 1.0) * (1.0 - smoothstep(-blur, blur, d));
    } else if (kind < 2.5) {
        o = charge(vL);
        o.rgb = mix(o.rgb, uBridgeInk * o.a, uBridge);
    } else if (kind < 3.5) {
        // a fragment: a small solid square, a faint glow of the charge it still holds
        float s = vD.x;
        float fw = max(fwidth(vL.x), 1e-4);
        float d = sdRBox(vL, vec2(s), s * 0.12);
        float shade = vL.x - vL.y > 0.0 ? 0.8 : 1.05;
        vec3 col = mix(vC.rgb * shade, uBridgeInk, uBridge);
        float a = clamp(0.5 - d / fw, 0.0, 1.0);
        float g = exp(-max(d, 0.0) / (s * 0.9)) * (1.0 - a) * vC.w;
        o = vec4(col * a + vC.rgb * g * 0.6, a);
    } else if (kind < 4.5) {
        float r = length(vL), R = vD.x, ring = vD.y;
        float fw = max(fwidth(vL.x), 1e-4);
        float d = ring > 0.0 ? abs(r - R) - ring : r - R;
        o = vec4(mix(vC.rgb, uBridgeInk, uBridge * vC.w), 1.0) * clamp(0.5 - d / fw, 0.0, 1.0);
    } else if (kind < 5.5) {
        // a spark: a thin streak, white hot at its head (+x), cooling to its color along the tail
        float Lh = vD.x, w = vD.y;
        float x = vL.x / Lh; // -1 tail .. 1 head
        float k = clamp((x + 1.0) * 0.5, 0.0, 1.0);
        float wy = w * (0.35 + 0.65 * k);
        float i = exp(-vL.y * vL.y / (wy * wy)) * smoothstep(-1.0, -0.2, x) * smoothstep(1.0, 0.8, x);
        vec3 col = mix(vC.rgb, vec3(1.0), k * k * vC.w);
        o = vec4(col * i * (0.6 + 0.6 * k), i * 0.4);
    } else {
        // a flash: a hot point and its bloom
        float R = vD.x;
        float d = length(vL);
        float core = exp(-d * d / (R * R * 0.04));
        float bloom = exp(-d * d / (R * R * 0.3));
        vec3 col = vec3(1.0) * core + vC.rgb * bloom * 0.8;
        o = vec4(col, core * 0.5);
    }
    frag = o * alpha;
}
)";

// ---------------------------------------------------------------- text and the veil

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

} // namespace electron_shaders

#pragma once
// PINK VIBE's shaders, in display (sRGB) colors, every shape a signed distance field: the satin ribbons behind
// everything (their relief lit from the top left, with their sparse sparkles and bubbles), the sprites (the well's
// pane and its neon frame, petals, hearts, bubbles, sparkle dust), the blocks (one chunky pearl cube per cell, traced
// a little from above, with an embossed gold inlay per piece type, their soft shadows) and the lettering on a feathered blush halo.

namespace pinkvibe_shaders {

// Shared painting helpers (prepended after the #version line).
inline const char* PAINT_GLSL = R"(
const float TAU = 6.2831853;
float AA = 0.01;
void layerA(inout vec4 acc, float d, vec3 col, float k) {
    float a = (1.0 - smoothstep(-AA, AA, d)) * k;
    acc.rgb = mix(acc.rgb, col, acc.a > 0.0 ? a : 1.0);
    acc.a = max(acc.a, a);
}
void layer(inout vec4 acc, float d, vec3 col) { layerA(acc, d, col, 1.0); }
float dEll(vec2 p, vec2 r) { return (length(p / r) - 1.0) * min(r.x, r.y); }
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float dSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}
float dot2(vec2 v) { return dot(v, v); }
// A heart about 2 wide, its point at the bottom.
float dHeart(vec2 q) {
    vec2 p = q * 0.62 + vec2(0.0, 0.58);
    p.x = abs(p.x);
    float d;
    if (p.y + p.x > 1.0) d = sqrt(dot2(p - vec2(0.25, 0.75))) - sqrt(2.0) / 4.0;
    else d = sqrt(min(dot2(p - vec2(0.0, 1.0)), dot2(p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
    return d / 0.62;
}
// A four-pointed sparkle (radius 1), its arms slender.
float dSparkle(vec2 q) {
    vec2 p = abs(q);
    float v = sqrt(p.x + 0.002) + sqrt(p.y + 0.002);
    return (v - 1.0) * 0.55;
}
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
)";

// ---------------------------------------------------------------- the ribbons
inline const char* BG_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;            // center, half extents (board units)
uniform vec3 uBand[5];         // blush .. the deepest band (mirrored: 0 1 2 3 4 3 2 1)
uniform float uTime, uFlow, uAmp, uSway, uSwirl, uDesat, uDecor;
uniform vec4 uColL, uColR;     // the HUD columns (board units): the motifs fade out there
uniform float uLaid, uDusk;    // the setup / teardown fronts (in bands): laid up to uLaid, under the dusk up to uDusk
uniform vec3 uDuskCol;         // the deep plum the bands go under after a song
uniform vec3 uLight;           // the warm light glowing in the top left corner
const vec3 LIGHT = vec3(-0.313, 0.716, 0.627); // from the top left, a little above
vec3 band(float i) {
    float k = mod(i, 8.0);
    k = k > 4.0 ? 8.0 - k : k;
    return k < 0.5 ? uBand[0] : k < 1.5 ? uBand[1] : k < 2.5 ? uBand[2] : k < 3.5 ? uBand[3] : uBand[4];
}
float boxD(vec2 p, vec4 b) {
    vec2 c = (b.xy + b.zw) * 0.5, h = (b.zw - b.xy) * 0.5;
    return dBox(p - c, h, 0.8);
}
// The band coordinate at p: diagonal, waving along the bands; the drop swirls it around the well.
float bandCoord(vec2 p) {
    vec2 c = p - vec2(0.0, -1.0);
    float r = length(c);
    float a = uSwirl * (1.9 * exp(-r / 15.0) + 0.25) + uSwirl * 0.04 * uTime;
    float cs = cos(a), sn = sin(a);
    c = mat2(cs, sn, -sn, cs) * c;
    const vec2 dir = vec2(0.5, 0.866);            // the bands run from the top left to the bottom right
    vec2 nrm = vec2(dir.y, -dir.x);
    float s = dot(c, dir), t = dot(c, nrm);
    float w = uAmp * (1.25 * sin(t * 0.15 + uFlow * 0.9 + s * 0.05)
                    + 0.55 * sin(t * 0.31 - uFlow * 0.6 + 1.7)
                    + 0.3 * sin(t * 0.07 + s * 0.11 + uFlow * 0.35 + 4.0));
    w += uSway * sin(t * 0.12 + s * 0.2 + 0.6);
    // the drop: the bands roll into one big wave, a slow long swell
    w += uSwirl * 2.2 * sin(t * 0.09 + uTime * 0.12);
    return s + w;
}
// A ribbon's slope across it (f from its lower edge to its upper one, radians): each ribbon lies over the one below
// it, its lower edge a rounded lip rolling down onto it, then a satin face sloping gently toward the light and
// curving away under the ribbon above.
float tilt(float f) {
    float x = f / 0.17;
    return x < 1.0 ? -1.2 * pow(1.0 - x, 1.7) : 0.12 * smoothstep(0.17, 0.32, f) + 0.3 * smoothstep(0.32, 1.0, f);
}
// A ribbon's color at f, lit; gn: the direction across the ribbons (toward the one above).
vec3 ribbon(vec3 c, float f, vec2 gn) {
    float a = tilt(f);
    vec3 n = vec3(gn * sin(a), cos(a));
    float lit = max(dot(n, LIGHT), -0.15) / LIGHT.z;               // 1 on a flat face
    vec3 col = c * (0.74 + 0.27 * lit);
    // the ribbon above casts a soft shadow over this one's upper edge, longer where the light comes across them
    float reach = 0.09 + 0.2 * max(dot(gn, normalize(LIGHT.xy)), 0.0);
    float sh = smoothstep(1.0 - reach, 1.0, f);
    col *= mix(vec3(1.0), vec3(0.74, 0.62, 0.7), sh * sh);
    // a satin sheen where the face turns to the light
    float sp = max(dot(n, normalize(LIGHT + vec3(0.0, 0.0, 1.0))), 0.0);
    col += vec3(1.0, 0.95, 0.92) * (0.1 * pow(sp, 90.0) + 0.05 * pow(sp, 12.0));
    return col;
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    const float BW = 3.3;                              // a band's width (board units)
    float s = bandCoord(p) / BW + 0.5;
    vec2 gs = vec2(dFdx(s), dFdy(s));
    vec2 gn = gs / max(length(gs), 1e-6);
    float i = floor(s), f = fract(s);
    // around a song: the bands laid one after another over the blush (the setup), going under the plum dusk one
    // after another (the teardown)
    float laid = clamp(uLaid - i, 0.0, 1.0), laidN = clamp(uLaid - i - 1.0, 0.0, 1.0);
    float dusk = clamp(uDusk - i, 0.0, 1.0), duskN = clamp(uDusk - i - 1.0, 0.0, 1.0);
    vec3 col = ribbon(mix(mix(uBand[0], band(i), laid), uDuskCol, dusk), f, gn);
    float px = length(gs);
    float aa = smoothstep(1.0 - px * 1.2, 1.0, f);
    col = mix(col, ribbon(mix(mix(uBand[0], band(i + 1.0), laidN), uDuskCol, duskN), 0.0, gn), aa);
    // the warm light in the top left corner, the far bottom right a little deeper
    vec2 sc = (vUv - vec2(0.0, 1.0)) * vec2(uView.z / uView.w, 1.0);
    float r2 = dot(sc, sc);
    float glow = (1.0 - max(dusk, uDesat)) * (0.5 * exp(-r2 * 2.2) + 0.18 * exp(-r2 * 0.5));
    col = mix(col, uLight, glow);
    col *= 1.0 - 0.1 * smoothstep(1.2, 3.2, r2);
    // sparse bubbles, sparkles and glints drifting very slowly upward
    if (uDecor > 0.01) {
        const float G = 4.6;
        vec2 q = p + vec2(0.0, -uTime * 0.18);
        vec2 gc = floor(q / G);
        float h = hash12(gc);
        if (h > 0.8) {
            vec2 o = (vec2(hash12(gc + 7.3), hash12(gc + 2.1)) - 0.5) * G * 0.6;
            o += vec2(sin(uTime * 0.21 + h * 40.0), cos(uTime * 0.17 + h * 23.0)) * 0.35;
            vec2 cc = (gc + 0.5) * G + o;
            vec2 l = q - cc;
            vec2 world = cc + vec2(0.0, uTime * 0.18);
            float vis = smoothstep(0.6, 2.4, min(boxD(world, uColL), boxD(world, uColR)));
            vis *= smoothstep(5.8, 7.4, abs(world.x));                 // (none behind the well)
            vis *= uDecor * laid * (1.0 - dusk);
            float tw = 0.75 + 0.25 * sin(uTime * 0.9 + h * 31.0);
            const vec3 PEARL = vec3(1.0, 0.97, 0.96);
            float AAl = fwidth(q.x) * 0.8;
            float kind = fract(h * 17.0);
            if (kind < 0.4) {
                // a sparkle and its soft halo
                float ds = dSparkle(l / (0.4 * tw)) * 0.4 * tw;
                col = mix(col, PEARL, vis * (0.35 * exp(-dot(l, l) * 9.0) + 0.85 * (1.0 - smoothstep(-AAl, AAl, ds))));
            } else if (kind < 0.8) {
                // a soap bubble: its soft shadow on the ribbon, a faint pearly disc, a fine bright rim, a highlight
                float R = 0.3 + 0.12 * fract(h * 53.0);
                float dl = length(l);
                col *= 1.0 - 0.12 * vis * exp(-dot2(l - vec2(0.1, -0.16)) / (R * R));
                col = mix(col, PEARL, vis * 0.16 * (1.0 - smoothstep(R - AAl, R + AAl, dl)));
                col = mix(col, PEARL, vis * 0.6 * smoothstep(0.55, 1.0, dl / R) * (1.0 - smoothstep(-AAl, AAl, dl - R)));
                col = mix(col, vec3(1.0), vis * 0.9 * (1.0 - smoothstep(-AAl, AAl, dEll(l - vec2(-0.4, 0.42) * R, vec2(0.22, 0.13) * R))));
            } else {
                // a glint
                col = mix(col, PEARL, vis * exp(-dot(l, l) * 30.0 / tw));
            }
        }
    }
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.86, 0.8, 0.98), uDesat);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- sprites
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;  // x, y, half width, half height (board units)
layout(location = 2) in vec4 iB;  // rotation, kind, alpha, p1
layout(location = 3) in vec4 iC;  // color A, -
layout(location = 4) in vec4 iD;  // color B, -
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 ext = iA.zw * 1.08 + 0.25;
    vec2 l = aCorner * ext;
    float c = cos(iB.x), s = sin(iB.x);
    vL = l;
    vA = iA; vB = iB; vC = iC; vD = iD;
    gl_Position = uVP * vec4(iA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y), 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(
in vec2 vL;
flat in vec4 vA, vB, vC, vD;
out vec4 frag;
// over: premultiplied src over premultiplied dst
vec4 over(vec4 dst, vec3 c, float a) { return vec4(c * a + dst.rgb * (1.0 - a), a + dst.a * (1.0 - a)); }
void main() {
    int kind = int(vB.y + 0.5);
    vec3 A = vC.rgb, B = vD.rgb;
    float p1 = vB.w;
    vec4 acc = vec4(0.0);
    if (kind == 0) {
        // the well (board units): a translucent deep wine pane (p1: its opacity), deeper toward the floor, a faint
        // diagonal sheen of glass, and a rounded neon frame of color B glowing softly inside and out
        vec2 l = vL;
        AA = max(fwidth(l.x), 1e-4) * 0.7;
        float d = dBox(l, vA.zw, 0.6);
        float g = clamp(l.y / vA.w * 0.5 + 0.5, 0.0, 1.0);
        vec3 pane = mix(A * 0.62, A * 1.02, g);
        float sd = dot(l, vec2(0.45, 0.89));
        pane = mix(pane, B, 0.035 * exp(-dot2(vec2(sd - 2.5, 0.0)) / 6.0) + 0.16 * exp(d / 0.3));
        float inside = 1.0 - smoothstep(-AA, AA, d);
        acc = vec4(pane * inside * p1, inside * p1);
        float ad = abs(d);
        acc = over(acc, B, 0.6 * exp(-ad / 0.12) + 0.28 * exp(-ad / 0.45));
        acc = over(acc, mix(B, vec3(1.0), 0.6), 1.0 - smoothstep(0.03, 0.03 + 2.0 * AA, ad));
        frag = acc * vB.z;
        return;
    }
    vec2 q = vL / vA.w;
    AA = max(fwidth(q.x), 1e-4) * 0.8;
    if (kind == 1) {
        // a petal: a soft pointed oval, a paler vein
        float d = max(dEll(q, vec2(1.0, 0.55)), dEll(q - vec2(0.35, 0.0), vec2(1.3, 0.62)));
        layer(acc, d, A);
        layerA(acc, max(abs(q.y) - 0.035, abs(q.x + 0.1) - 0.5), B, 0.5);
    } else if (kind == 2) {
        layer(acc, dHeart(q), A);
        layerA(acc, length(q - vec2(-0.38, 0.32)) - 0.13, B, 0.7);
    } else if (kind == 3) {
        // a bubble: a pale disc, a fine ring, a small highlight
        layerA(acc, length(q) - 0.95, A, 0.35);
        layerA(acc, abs(length(q) - 0.9) - 0.06, A, 0.9);
        layerA(acc, dEll(q - vec2(-0.38, 0.4), vec2(0.22, 0.13)), B, 0.9);
    } else if (kind == 4) {
        // a sparkle in its soft halo
        acc = vec4(A, 0.4 * exp(-dot(q, q) * 4.0));
        layer(acc, dSparkle(q), mix(A, B, 0.6));
    } else {
        // a mote of sparkle dust: a soft glowing point
        float gl = exp(-dot(q, q) * 2.5);
        acc = vec4(mix(A, B, gl * gl), gl);
    }
    float al = acc.a * vB.z;
    frag = vec4(acc.rgb * al, al);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // type, alpha, mode (0 block, 1 ghost, 2 preview, 3 the falling piece), scale
layout(location = 3) in vec4 iC; // color, k (< 0: brightening, > 0: game over greying)
uniform mat4 uVP;
out vec2 vU, vW;
flat out vec4 vB, vC;
void main() {
    vec2 l = aCorner * 0.68;   // (room for the soft shadow, the falling piece's glow)
    float c = cos(iA.w), s = sin(iA.w);
    vec2 w = vec2(c * l.x - s * l.y, s * l.x + c * l.y) * iA.z * 2.0 * iB.w;
    vU = l;
    vW = iA.xy + w;
    vB = iB; vC = iC;
    gl_Position = uVP * vec4(iA.xy + w, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(
in vec2 vU, vW;
flat in vec4 vB, vC;
out vec4 frag;
uniform int uPass;            // 0: the soft shadows (and the falling piece's glow), 1: the blocks
uniform vec3 uShade, uGlow;   // the shadows' deep tint, the falling piece's glow
uniform vec3 uVivid[7];
uniform float uWave, uBeat;
uniform int uGlass;           // the type cast in rose glass (-1: none)
// Each block is a chunky sugar cube seen a little from above: a rounded box (cell units) whose front face fills
// most of the cell and whose lit top face shows as a thick rounded lip above it. The view is tilted down by TILT.
const float TILT_S = 0.407, TILT_C = 0.9135;           // 24 degrees
const vec3 BOX = vec3(0.474, 0.421, 0.22);              // half extents: 0.95 wide, 0.95 tall on screen
const float RAD = 0.19;                                // the edges' rounding (a thick soft bevel)
const float FOOT = 0.474, FOOT_R = 0.19;               // the screen footprint (the ghost, the shadows)
const vec3 LIGHT = vec3(-0.42, 0.72, 0.55);            // from the top left, above and in front
const vec3 GOLD = vec3(1.0, 0.87, 0.58), GOLD_DEEP = vec3(0.56, 0.36, 0.17);
const vec3 MILK = vec3(1.0, 0.96, 0.95);
float dRBox(vec3 p) {
    vec3 q = abs(p) - BOX + RAD;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - RAD;
}
vec3 nRBox(vec3 p) {
    vec3 q = abs(p) - BOX + RAD;
    vec3 m = max(q, 0.0);
    if (dot(m, m) > 1e-9) return normalize(m) * sign(p);
    return q.x > q.y && q.x > q.z ? vec3(sign(p.x), 0.0, 0.0) : q.y > q.z ? vec3(0.0, sign(p.y), 0.0) : vec3(0.0, 0.0, sign(p.z));
}
// The gold inlay on the front face (face coordinates, cell units): a fine frame set in from the edge, and one
// small motif per type in its middle.
float motif(int type, vec2 u) {
    if (type == 0) {           // I: three pearls in a row
        vec2 q = vec2(abs(u.x), u.y);
        return min(length(q), length(q - vec2(0.16, 0.0))) - 0.048;
    }
    if (type == 1) return dSparkle(u / 0.24) * 0.24;                        // O: a four-pointed star
    if (type == 2) return abs(abs(u.x) + abs(u.y) - 0.15) * 0.7071 - 0.026; // T: a diamond
    if (type == 3) return dSeg(abs(u), vec2(0.0), vec2(0.13)) - 0.03;     // S: an X
    if (type == 4) return length(abs(u) - vec2(0.11)) - 0.045;               // Z: four dots
    if (type == 5) return dHeart(u / 0.17) * 0.17;                         // J: a heart
    return abs(length(u) - 0.12) - 0.028;                                  // L: a ring
}
float inlay(int type, vec2 f, float frameK) {
    float d = motif(type, f);
    float fr = abs(dBox(f, vec2(0.275, 0.26), 0.07)) - 0.017;
    return min(d, mix(1.0, fr, frameK));
}
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.75;
    int type = int(vB.x + 0.5), mode = int(vB.z + 0.5);
    float dFoot = dBox(u, vec2(FOOT), FOOT_R);
    if (uPass == 0) {
        // a soft ambient shadow pooled under each cube (darker just below it, in the gaps); under the falling piece
        // a soft rose glow
        if (mode == 1) discard;
        vec4 o;
        if (mode == 3) o = vec4(uGlow, 0.5 * exp(-max(dFoot, 0.0) * max(dFoot, 0.0) / 0.006));
        else {
            float ds = dBox(u - vec2(0.02, -0.06), vec2(FOOT - 0.01), FOOT_R);
            o = vec4(uShade, (type == uGlass ? 0.35 : 0.7) * (1.0 - smoothstep(-0.1, 0.12, ds)));
        }
        float al = o.a * vB.y;
        frag = vec4(o.rgb * al, al);
        return;
    }
    vec3 col = vC.rgb;
    if (uWave > 0.0 && (mode == 0 || mode == 3)) {
        // the drop: every two beats a wave of the drop's vivid colors rises from the well's floor through the stack,
        // a crisp front and a long fading tail, a gradient crawling across the still blocks
        float dist = length((vW - vec2(0.0, -11.5)) * vec2(1.3, 1.0));
        float s = fract(uBeat * 0.5 - dist / 9.0);
        float band = smoothstep(0.0, 0.06, s) * (1.0 - smoothstep(0.06, 0.55, s));
        col = mix(col, uVivid[type], uWave * (0.12 + 0.88 * band));
    }
    if (mode == 1) {
        // the ghost: a soft fill and a clear line of the piece's color, lightened so it reads on the deep well
        vec4 acc = vec4(0.0);
        vec3 lc = mix(col, vec3(1.0), 0.4);
        layerA(acc, dFoot, col, 0.2);
        layerA(acc, abs(dFoot + 0.035) - 0.03, lc, 0.9);
        frag = vec4(acc.rgb * acc.a * vB.y, acc.a * vB.y);
        return;
    }
    // trace the cube: an orthographic ray from the front, tilted down
    vec3 rd = vec3(0.0, -TILT_S, -TILT_C);
    vec3 up = vec3(0.0, TILT_C, -TILT_S);
    vec3 ro = vec3(u.x, 0.0, 0.0) + u.y * up - rd * 1.0;
    float t = 0.0, dmin = 1e3, d = 1.0;
    for (int i = 0; i < 28; i++) {
        d = dRBox(ro + rd * t);
        dmin = min(dmin, d);
        if (d < 1e-4) break;
        t += d;
        if (t > 2.0) break;
    }
    float cover = 1.0 - smoothstep(-AA, AA, dmin - AA * 0.5);
    if (cover <= 0.0) discard;
    vec3 p = ro + rd * t;
    vec3 n = nRBox(p);
    // the front face is softly cushioned (a hair domed), the top face too
    n = normalize(n + vec3(p.xy * 0.35, 0.0) * smoothstep(0.6, 1.0, n.z) + vec3(p.x * 0.3, 0.0, p.z * 0.3) * smoothstep(0.6, 1.0, n.y));
    bool glass = type == uGlass;
    vec3 V = -rd, L = normalize(LIGHT), H = normalize(L + V);
    float ndl = dot(n, L);
    // satin: soft wrapped light, the top face brighter, the bottom of the front face a little in the shade of the
    // cube below's bevel (ambient occlusion), warm bounce light in the bottom bevel
    float wrap = clamp(ndl * 0.5 + 0.5, 0.0, 1.0);
    float ao = mix(0.8, 1.0, smoothstep(-BOX.y, BOX.y * 0.6, p.y));
    vec3 c = col * (0.66 + 0.5 * wrap) * ao;
    c = mix(c, MILK, 0.22 * smoothstep(0.45, 1.0, n.y));
    // a soft satin lustre across the front face, brighter toward its top left
    c = mix(c, MILK, 0.14 * smoothstep(0.35, -0.45, dot(p.xy, vec2(0.7, -0.7)) / BOX.x) * smoothstep(0.6, 1.0, n.z));
    c += col * vec3(1.0, 0.75, 0.75) * 0.12 * smoothstep(-0.2, -0.8, n.y);
    // nacre: a faint play between a lilac and a peach sheen, shifting with the slope and the position
    float ph = 0.5 + 0.5 * cos(TAU * (dot(n, vec3(1.1, 0.7, 0.4)) + dot(vW, vec2(0.08, 0.05))));
    c *= mix(vec3(0.97, 0.94, 1.05), vec3(1.05, 0.99, 0.94), ph);
    // a milky pearl lustre on the edges turned away from the eye (fresnel), stronger toward the light
    float fres = pow(1.0 - clamp(dot(n, V), 0.0, 1.0), 2.5);
    c = mix(c, MILK, fres * (0.25 + 0.35 * wrap));
    // the glossy highlight: a tight bright one on the lit bevel and a broad satin one
    float nh = max(dot(n, H), 0.0);
    float gloss = pow(nh, 70.0) * 0.75 + pow(nh, 9.0) * 0.1;
    // the gold inlay, raised: a soft shadow under it, then the gold bumped by its own slope, warm, with a glint
    float onFace = smoothstep(0.75, 0.95, n.z), gA = 0.0;
    if (onFace > 0.0) {
        vec2 fp = p.xy - vec2(0.0, 0.01);
        float frameK = 1.0 - smoothstep(0.03, 0.05, AA / 0.75);
        float dm = inlay(type, fp, frameK);
        float ds = inlay(type, fp - vec2(0.012, -0.02), frameK);
        c *= 1.0 - 0.32 * onFace * (1.0 - smoothstep(-0.01, 0.02, ds));
        float e = 0.004;
        vec2 g = vec2(inlay(type, fp + vec2(e, 0.0), frameK) - inlay(type, fp - vec2(e, 0.0), frameK),
                      inlay(type, fp + vec2(0.0, e), frameK) - inlay(type, fp - vec2(0.0, e), frameK)) / (2.0 * e);
        float w = 0.022;
        float k = clamp(-dm / w, 0.0, 1.0);
        vec3 gn = normalize(vec3(g * (1.0 - k) * 1.6, 1.0));
        gn = normalize(gn + n - vec3(0.0, 0.0, 1.0));
        float gd = clamp(dot(gn, L) * 0.6 + 0.45, 0.0, 1.0);
        vec3 gold = mix(GOLD_DEEP, GOLD, gd);
        gold += vec3(1.0, 0.95, 0.8) * (pow(max(dot(gn, H), 0.0), 40.0) * 0.9);
        // (the frame a quieter line than the motif, so the motifs tell the pieces apart)
        float dfr = abs(dBox(fp, vec2(0.275, 0.26), 0.07)) - 0.017;
        float dmo = motif(type, fp);
        // a fine dark bed around the motif, so the gold reads on the palest cubes too
        c = mix(c, GOLD_DEEP * 0.7, 0.45 * onFace * (1.0 - smoothstep(0.0, max(0.016, AA * 1.5), dmo)));
        float ga = max(1.0 - smoothstep(-AA, AA, dmo), 0.55 * frameK * (1.0 - smoothstep(-AA, AA, dfr))) * onFace;
        c = mix(c, gold, ga);
        gloss *= 1.0 - ga;
        gA = ga;
    }
    float alpha = 1.0;
    if (glass) {
        // rose glass: the well shows through the face, the edges denser and brighter, a bright inner rim
        float thick = 1.0 - pow(clamp(dot(n, V), 0.0, 1.0), 3.0);
        alpha = mix(0.8, 0.97, thick);
        c = mix(c, mix(col, MILK, 0.3) * 1.08, 0.5 * (1.0 - thick)) + col * 0.08;
        c += col * 0.3 * smoothstep(-0.05, -0.38, p.y) * onFace;        // light gathered in the bottom of the glass
        gloss *= 1.4;
        alpha = max(alpha, gA);
    }
    c += MILK * gloss;
    if (mode == 3) c = mix(c, vec3(1.0, 0.95, 0.96), 0.06);             // the falling piece, a little aglow
    if (vC.w < 0.0) c = mix(c, vec3(1.0, 0.96, 0.97), -vC.w);
    else {
        float l = dot(c, vec3(0.299, 0.587, 0.114));
        c = mix(c, vec3(l) * vec3(0.7, 0.6, 0.68), vC.w);
    }
    float al = cover * alpha * vB.y;
    frag = vec4(c * al, al);
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace pinkvibe_shaders

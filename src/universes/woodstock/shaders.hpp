#pragma once
// WOODSTOCK's shaders. Flat 70s sticker art in display (sRGB) colors: every shape a signed distance field in flat
// color with one even ink outline (OUTLINE board units, the same on the motifs, the petals and the blocks). The
// paper behind everything (its grain, the thin double border, the sunburst peace disc, the two daisies, the small
// heart and bolts, the well's panel), the drifting petals and hearts, the blocks.

namespace woodstock_shaders {

// Shared painting helpers (prepended after the #version line). Distances in board units; AA set per fragment.
inline const char* PAINT_GLSL = R"(
const float TAU = 6.2831853;
float AA = 0.01;
float cov(float d) { return 1.0 - smoothstep(-AA, AA, d); }
// Paints `c` where d < 0 (antialiased).
void paint(inout vec3 col, float d, vec3 c) { col = mix(col, c, cov(d)); }
// Paints `c` where d < 0 with an ink outline of width w centered on its edge.
void sticker(inout vec3 col, float d, vec3 c, vec3 ink, float w) {
    col = mix(col, ink, cov(d - w * 0.5));
    col = mix(col, c, cov(d + w * 0.5));
}
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float dEll(vec2 p, vec2 r) { return (length(p / r) - 1.0) * min(r.x, r.y); }
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
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
// The point in the frame of its nearest of n petals around the origin (x along the petal), k its index.
vec2 petalFrame(vec2 q, float n, float turn, out float k) {
    float an = atan(q.y, q.x) - turn;
    float sec = TAU / n;
    k = floor(an / sec + 0.5);
    float la = an - k * sec;
    return length(q) * vec2(cos(la), sin(la));
}
)";

// ---------------------------------------------------------------- the paper and its motifs
inline const char* BG_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform float uW;            // the outline (board units)
uniform vec3 uGround, uFrame, uHalo, uWellFill, uInk;
uniform vec3 uScallop, uScallop2, uRing, uPeace, uSky, uGlint;
uniform vec3 uPetal, uCenter, uCenter2, uLeaf, uHeart, uBolt;
uniform vec4 uDisc;          // the sunburst peace disc: center, radius, the scallops' turn
uniform vec2 uDiscFx;        // its rays (0 calm .. 1 the drop), its scallops' pulse
uniform vec4 uHead[2];       // the daisies' heads: center, radius, turn
uniform vec4 uStem[2];       // their stems' foot, the leaves' size, the flower's lean
uniform vec4 uHeartAt, uBoltAt[2]; // center, size, tilt
uniform vec4 uWell;          // the well's half extents, its corner, 1 shown / 0 not (a break between songs)
uniform float uDesat;        // slow motion's dusk
uniform float uGrain;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
// iq's polygon distance: the lightning bolt (unit height 2)
float dBolt(vec2 p) {
    vec2 v[7] = vec2[7](vec2(-0.22, 1.0), vec2(0.5, 1.0), vec2(0.12, 0.18), vec2(0.5, 0.18), vec2(-0.36, -1.0),
                        vec2(-0.08, -0.12), vec2(-0.46, -0.12));
    float d = dot(p - v[0], p - v[0]);
    float s = 1.0;
    for (int i = 0, j = 6; i < 7; j = i, i++) {
        vec2 e = v[j] - v[i], w = p - v[i];
        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
        d = min(d, dot(b, b));
        bvec3 c = bvec3(p.y >= v[i].y, p.y < v[j].y, e.x * w.y > e.y * w.x);
        if (all(c) || all(not(c))) s *= -1.0;
    }
    return s * sqrt(d);
}
// A leaf (a vesica pointing up its local y), length L, width w.
float dLeaf(vec2 p, float L, float w) {
    float b = L * 0.5;
    float r = (b * b + w * w * 0.25) / w, d = r - w * 0.5;
    p = abs(p);
    return ((p.y - b) * d > p.x * b) ? length(p - vec2(0.0, b)) : length(p - vec2(-d, 0.0)) - r;
}
void daisy(inout vec3 col, vec2 p, vec4 head, vec4 stem) {
    // the stem: one slim ink stroke from its foot to the head, two leaves on it (all leaning with the flower)
    vec2 foot = stem.xy;
    vec2 h = foot + rot(stem.w) * (head.xy - foot);
    vec2 ax = normalize(h - foot);
    float len = length(h - foot);
    vec2 rel = p - foot;
    float t = clamp(dot(rel, ax), 0.0, len);
    float dStem = length(rel - ax * t) - uW * 0.75;
    paint(col, dStem, uInk);
    vec2 nrm = vec2(-ax.y, ax.x);
    for (int s = 0; s < 2; s++) {
        float side = s == 0 ? 1.0 : -1.0;
        float at = len * (s == 0 ? 0.3 : 0.5);
        vec2 dir = normalize(ax * 0.75 + nrm * side * 0.9);
        float L = stem.z * (s == 0 ? 1.0 : 0.85);
        vec2 c = foot + ax * at + dir * (L * 0.5 + uW);
        vec2 lp = p - c;
        lp = vec2(dot(lp, vec2(dir.y, -dir.x)), dot(lp, dir));
        float dl = dLeaf(lp, L, L * 0.42);
        sticker(col, dl, uLeaf, uInk, uW);
        // its midrib
        paint(col, max(abs(lp.x) - uW * 0.28, abs(lp.y) - L * 0.36), uInk);
    }
    // the head: eight round petals, a two-tone center
    vec2 q = p - h;
    float r = head.z;
    if (length(q) > r * 1.1 + uW) return;
    float k;
    vec2 pp = petalFrame(q, 8.0, head.w, k);
    float dP = dEll(pp - vec2(0.56 * r, 0.0), vec2(0.44 * r, 0.21 * r));
    sticker(col, dP, uPetal, uInk, uW);
    sticker(col, length(q) - 0.36 * r, uCenter, uInk, uW);
    paint(col, length(q) - 0.2 * r, uCenter2);
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    AA = max(fwidth(p.x), 1e-4) * 0.75;
    vec3 col = uGround;
    // the paper: a faint grain
    col *= 1.0 + uGrain * (hash(floor(gl_FragCoord.xy)) - 0.5);
    // the thin double border framing the screen
    {
        float d = dBox(p - uView.xy, uView.zw, 0.0);
        float dl = min(abs(d + 0.3) - 0.035, abs(d + 0.48) - 0.035);
        paint(col, dl, uFrame);
    }
    // ---- the sunburst peace disc
    {
        vec2 q = p - uDisc.xy;
        float R = uDisc.z, rr = length(q);
        if (rr < R * 1.6) {
            // the halo, which a drop opens into a burst of soft pointed rays
            float an = atan(q.y, q.x) - uDisc.w * 0.5;
            float ray = abs(fract(an / TAU * 16.0) - 0.5) * TAU / 16.0 * rr;    // the arc to a ray's middle
            float tip = R * mix(1.12, 1.42, uDiscFx.x);
            float wr = mix(R * 0.2, 0.0, clamp((rr - R) / (tip - R), 0.0, 1.0));
            paint(col, max(ray - wr, rr - tip), uHalo);
            paint(col, rr - R * 1.12, uHalo);
            // two rings of scallops, turning slowly
            float k;
            float rs = R * 0.12 * (1.0 + 0.18 * uDiscFx.y);
            float rb = R - R * 0.12;
            vec2 pp = petalFrame(q, 16.0, uDisc.w, k);
            float dS = min(rr - rb, length(pp - vec2(rb, 0.0)) - rs);
            sticker(col, dS, uScallop, uInk, uW);
            float rs2 = R * 0.085 * (1.0 + 0.18 * uDiscFx.y);
            float rb2 = R * 0.86 - R * 0.085;
            pp = petalFrame(q, 16.0, uDisc.w + TAU / 32.0, k);
            float dS2 = min(rr - rb2, length(pp - vec2(rb2, 0.0)) - rs2);
            sticker(col, dS2, uScallop2, uInk, uW);
            // the yellow ring, the sky inside, its pale glint, the peace sign (upright)
            sticker(col, rr - R * 0.74, uRing, uInk, uW);
            float Rp = R * 0.63;
            sticker(col, rr - Rp, uSky, uInk, uW);
            paint(col, rr - R * 0.27, uGlint);
            float w = Rp * 0.2;
            float ring = abs(rr - (Rp - w * 0.5)) - w * 0.5;
            float rp = Rp - w * 0.5;
            vec2 a = abs(q);
            float bars = min(max(a.x - w * 0.5, a.y - rp), 1e3);
            vec2 dq = vec2(a.x, q.y);
            vec2 bd = normalize(vec2(0.7071, -0.7071));
            float tt = clamp(dot(dq, bd), 0.0, rp);
            bars = min(bars, length(dq - bd * tt) - w * 0.5);
            float dPeace = min(ring, bars);
            sticker(col, dPeace, uPeace, uInk, uW);
        }
    }
    // ---- the daisies
    daisy(col, p, uHead[0], uStem[0]);
    daisy(col, p, uHead[1], uStem[1]);
    // ---- a heart and two bolts, small
    {
        vec2 q = rot(-uHeartAt.w) * (p - uHeartAt.xy);
        sticker(col, dHeart(q / uHeartAt.z) * uHeartAt.z, uHeart, uInk, uW);
        for (int i = 0; i < 2; i++) {
            vec2 b = rot(-uBoltAt[i].w) * (p - uBoltAt[i].xy);
            sticker(col, dBolt(b / uBoltAt[i].z) * uBoltAt[i].z, uBolt, uInk, uW);
        }
    }
    // ---- the well's panel
    if (uWell.w > 0.5) {
        float d = dBox(p, uWell.xy, uWell.z);
        sticker(col, d, uWellFill, uInk, uW);
    }
    // slow motion: dusk (the colors drained toward lilac)
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.86, 0.84, 1.02), uDesat);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- the petals and hearts (instanced)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;  // x, y, half width, half height (board units)
layout(location = 2) in vec4 iB;  // rotation, kind (0 petal, 1 heart), alpha, outline (board units)
layout(location = 3) in vec4 iC;  // fill
layout(location = 4) in vec4 iD;  // ink
uniform mat4 uVP;
out vec2 vL;
flat out vec4 vA, vB, vC, vD;
void main() {
    vec2 ext = iA.zw * 1.15 + iB.w + 0.04;
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
void main() {
    AA = max(fwidth(vL.x), 1e-4) * 0.75;
    float d = vB.y < 0.5 ? dEll(vL, vA.zw) : dHeart(vL / vA.w) * vA.w;
    float w = vB.w;
    float a = cov(d - w * 0.5);
    vec3 col = mix(vD.rgb, vC.rgb, cov(d + w * 0.5));
    float al = a * vB.z;
    frag = vec4(col * al, al);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // type, alpha, mode (0 block, 1 ghost), scale
layout(location = 3) in vec4 iC; // color, k (< 0 a lock's brightening, > 0 the game over's greying)
uniform mat4 uVP;
out vec2 vU;
flat out vec4 vB, vC;
void main() {
    vec2 l = aCorner * 0.58;           // the cell spans -0.5..0.5
    float c = cos(iA.w), s = sin(iA.w);
    vec2 w = vec2(c * l.x - s * l.y, s * l.x + c * l.y) * iA.z * 2.0 * iB.w;
    vU = l;
    vB = iB; vC = iC;
    gl_Position = uVP * vec4(iA.xy + w, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(
in vec2 vU;
flat in vec4 vB, vC;
out vec4 frag;
uniform vec3 uInk, uShine;
uniform float uW;          // the outline (in cells, at scale 1)
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.75;
    float W = uW / max(vB.w, 0.3);
    vec3 col = vC.rgb;
    float d = dBox(u, vec2(0.47), 0.16);
    float a = cov(d);
    vec3 c;
    if (abs(vB.z - 1.0) < 0.5) {
        // the ghost: a faint wash of the piece's color inside a thin line of it
        float line = cov(abs(d + W * 0.5) - W * 0.3);
        c = col;
        a = max(a * 0.18, line * 0.75);
    } else {
        // a flat sticker: the color inside the ink line, one small pale glint in its corner
        c = mix(uInk, col, cov(d + W));
        float g = length(u - vec2(-0.22, 0.22)) - 0.065;
        c = mix(c, mix(col, uShine, 0.6), cov(g));
        // a lock's brief brightening, the game over's greying
        if (vC.w < 0.0) c = mix(c, uShine, -vC.w);
        else {
            float l = dot(c, vec3(0.299, 0.587, 0.114));
            c = mix(c, vec3(l) * 0.8 + 0.1, vC.w);
        }
    }
    float al = a * vB.y;
    frag = vec4(c * al, al);
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace woodstock_shaders

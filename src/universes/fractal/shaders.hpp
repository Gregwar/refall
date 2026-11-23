#pragma once
// FRACTAL's shaders, in display (sRGB) colors: the infinite Julia flower behind everything (one fullscreen pass, with
// the well's calm panel and its soft shadow), the flat blocks.

namespace fractal_shaders {

// Shared helpers (prepended after the #version line).
inline const char* PAINT_GLSL = R"(
float AA = 0.01;
float cov(float d) { return 1.0 - smoothstep(-AA, AA, d); }
float dBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
)";

// ---------------------------------------------------------------- the fractal
// The filled Julia set of z -> z^2 + c in flat pastel bands, seen through w = uFix + uMul * (p - uCenter): the host
// zooms for ever into a self-similar point of it (a repelling periodic point or one of its preimages), drawing it at
// a depth between D0 and D0 + 1 so that the picture loops seamlessly, and the contour keeps revealing finer petals.
// The bands are the smooth escape count outside and the smooth convergence count inside (toward the attracting
// cycle), plus uColOff, which the host raises by the iterations of a level each time it wraps the depth: each band
// keeps one flat color of the gradient while it grows, and the new ones bloom with the next colors. A fine line
// between bands and a soft shadow under each, like cut paper; the bands' edges filtered over their width on screen
// (from the orbit's derivative), and where they get finer than a pixel (along the contour) they melt into the wash
// color, so the endless detail never shimmers nor frays into dots.
inline const char* FRACTAL_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;          // center, half extents (board units)
uniform vec3 uBand[6], uDropBand[6];  // the gradient's stops, calm and in the drop
uniform float uDropMix;
uniform vec3 uLine, uWash, uShade, uWellFill, uWellLine;
uniform vec3 uInside;        // the petals' inside (the line's color, darker on a near-black palette)
uniform vec2 uCenter;        // where uFix sits on screen (board units)
uniform vec2 uC;             // the Julia parameter
uniform vec2 uFix;           // the view's center in the complex plane
uniform vec2 uMul;           // board units -> the complex plane (scale and turn)
uniform float uColOff;       // the bands' count offset (the levels zoomed so far, in iterations)
uniform vec2 uSink;          // the attracting cycle's point by the critical point (the inside), where the distance to
uniform float uSinkGain;     // it squares each period (e -> uSinkGain e^2), and the period
uniform int uPeriod;
uniform float uDesat, uGrain;
uniform vec4 uWell;          // the well's half extents, its corner, 1 shown / 0 not (a break between songs)
uniform vec4 uPrev[4];       // the hold and next previews: center, half size, strength (a soft pale halo behind each)
uniform vec2 uRef[65];       // the exact orbit of uFix (its first uRefN + 1 points; uRefN 0: none)
uniform int uRefN;
const int MAXIT = 200;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }
vec3 stopc(float i) {
    int k = int(mod(i, 6.0));
    return mix(uBand[k], uDropBand[k], uDropMix);
}
vec3 grad(float t) {
    float i = floor(t);
    return mix(stopc(i), stopc(i + 1.0), smoothstep(0.0, 1.0, t - i));
}
void main() {
    vec2 p = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // Deep in the zoom the point is uFix plus a tiny offset: iterated as is, each step's rounding (relative to the point)
    // grows into the offset and the picture turns to pixels. So the offset is iterated against uFix's exact orbit
    // (perturbation: (r + d)^2 + c - (r^2 + c) = (2r + d) d, its error relative to d itself) until it is large enough.
    // Along with z, its derivative dz (by the point in the plane, as dz * 2^lz: it outgrows a float near the contour),
    // for the bands' width on screen, a pixel's own (fwidth's, by 2x2 pixels, would break the finest into blocks).
    vec2 d = cmul(uMul, p - uCenter), dz = vec2(1.0, 0.0);
    float lz = 0.0;
    int i0 = 0;
    if (uRefN > 0) {
        while (i0 < uRefN && dot(d, d) < 1e-4) {
            dz = 2.0 * cmul(uRef[i0] + d, dz);
            d = cmul(2.0 * uRef[i0] + d, d);
            i0++;
        }
    }
    vec2 z = (uRefN > 0 ? uRef[i0] : uFix) + d;
    float pix = 1.27 * length(uMul) * abs(dFdx(p.x));   // the plane's units a pixel (x 4/pi: as wide as fwidth's)
    float gw = 0.0;                              // the count's change a pixel
    float g = -1.0;
    bool inside = false;
    // the inside: once the orbit comes within R of uSink, it converges, squaring (gain times) its distance each period:
    // u = log(gain e) doubles each period, so i - period log2(u / log(gain R)) is a smooth count (a whole count steps
    // from pixel to pixel, its bands' width unknown, and frays into dashes and dots where they crowd), held about the
    // preimages of the cycle (where u plunges); + 6: the bands then fall about where the orbit's first step within
    // 1e-3 of the cycle drew them (the look the palettes were tuned on)
    const float R = 0.05;
    float uR = log(uSinkGain * R);
    for (int i = i0; i < MAXIT; i++) {
        dz = 2.0 * cmul(z, dz);
        float dm = dot(dz, dz);
        if (dm > 1e20 || dm < 1e-20) {
            float s = dm > 1e20 ? -32.0 : 32.0;
            dz *= exp2(s);
            lz -= s;
        }
        z = cmul(z, z) + uC;
        float m = dot(z, z);
        if (m > 1e4) {
            float lm = 0.5 * log(m);              // log |z|
            g = float(i) + 1.0 - log2(lm / log(100.0));
            gw = exp2(log2(length(dz) * pix / (sqrt(m) * lm * log(2.0))) + lz);
            break;
        }
        vec2 e = z - uSink;
        float e2 = dot(e, e);
        if (e2 < R * R) {
            inside = true;
            float u = log(uSinkGain) + 0.5 * log(max(e2, 1e-30));
            float q = log2(u / uR);
            g = float(i) + 6.0 - float(uPeriod) * min(q, 1.0);
            // (where the count is held, about a preimage of the cycle, still its width as if not: a petal finer
            // than a pixel goes to the wash with the rest)
            gw = exp2(log2(float(uPeriod) * length(dz) * pix / (sqrt(e2) * abs(u) * log(2.0))) + lz);
            break;
        }
    }
    vec3 col;
    if (g < 0.0) col = uWash;                 // (no answer within MAXIT: on the contour itself)
    else {
        // the bands: an iteration each outside; a period each inside (where the orbit squares its way in), shifted
        // by uColOff in the gradient (the band count in the world)
        float unit = inside ? float(uPeriod) : 1.0;
        float t = g / unit, bi = floor(t), f = t - bi;
        float w = max(gw / unit, 1e-4);       // bands a pixel
        // each band one flat color, its edges filtered over the pixel (the next band's color over the last w)
        float io = uColOff * 0.5 + (inside ? 3.0 : 0.0);
        col = grad(bi * unit * 0.5 + io);
        col = mix(col, grad((bi - 1.0) * unit * 0.5 + io), clamp(0.5 - f / w, 0.0, 0.5));
        col = mix(col, grad((bi + 1.0) * unit * 0.5 + io), clamp(0.5 - (1.0 - f) / w, 0.0, 0.5));
        float edge = 0.8 * (1.0 - smoothstep(0.6, 1.4, min(f, 1.0 - f) / w));
        if (inside) {
            // the petals pale (the line's color over a shaded band), a fine line of the wash between their bands
            col = mix(col * mix(vec3(1.0), uShade, 0.22), uInside, 0.8);
            col = mix(col, uWash, edge);
        } else {
            // cut paper: the band inside lies over this one (its soft shadow), a fine line between them
            col = mix(col, col * uShade, 0.22 * exp(-f / (w * 9.0)));
            col = mix(col, uLine, edge);
        }
        // finer than a pixel (along the contour): the wash, still
        col = mix(col, uWash, smoothstep(0.15, 0.5, w));
    }
    col *= 1.0 + uGrain * (hash(floor(gl_FragCoord.xy)) - 0.5);
    AA = max(fwidth(p.x), 1e-4) * 0.75;
    if (uWell.w > 0.5) {
        // the previews: a soft pale halo behind each, so the small pieces read over the flower (no edge, no panel)
        for (int i = 0; i < 4; i++) {
            float dp = dBox(p - uPrev[i].xy, vec2(uPrev[i].z * 1.35, uPrev[i].z * 0.8), uPrev[i].z * 0.8);
            col = mix(col, uWellFill, 0.78 * uPrev[i].w * (1.0 - smoothstep(-0.5, 0.9, dp)));
        }
        float ds = dBox(p - vec2(0.18, -0.3), uWell.xy, uWell.z);
        col = mix(col, col * uShade, 0.35 * (1.0 - smoothstep(-0.3, 1.1, ds)));
        float dw = dBox(p, uWell.xy, uWell.z);
        col = mix(col, uWellLine, cov(dw));
        col = mix(col, uWellFill, cov(dw + 0.07));
    }
    // slow motion: dusk (the colors drained toward lilac)
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(l) * vec3(0.88, 0.86, 1.02), uDesat);
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------- blocks
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA; // x, y, half, rotation
layout(location = 2) in vec4 iB; // the same-piece neighbours' mask, alpha, mode (0 block, 1 ghost), scale
layout(location = 3) in vec4 iC; // color, k (< 0 a lock's brightening, > 0 the game over's greying)
uniform mat4 uVP;
out vec2 vU;
flat out vec4 vB, vC;
void main() {
    // the cell spans -0.5..0.5, the quad a margin past it (the antialiased edge), but only to the cell's side toward
    // a neighbour of the same piece: the two quads meet there, each pixel drawn once (no seam, even translucent)
    int m = int(iB.x + 0.5);
    vec2 l = aCorner * vec2(aCorner.x < 0.0 ? ((m & 1) != 0 ? 0.5 : 0.58) : ((m & 2) != 0 ? 0.5 : 0.58),
                            aCorner.y > 0.0 ? ((m & 4) != 0 ? 0.5 : 0.58) : ((m & 8) != 0 ? 0.5 : 0.58));
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
uniform vec3 uShine;
uniform float uGap;        // the hairline between pieces (board units, the same at any scale)
uniform float uLine;       // the ghost's line (board units)
void main() {
    vec2 u = vU;
    AA = max(fwidth(u.x), 1e-4) * 0.6;
    float sc = max(vB.w, 0.3);
    vec3 col = vC.rgb;
    // one flat square of the piece's color, a hairline gap around it, corners barely rounded; the cells of a piece
    // one shape: a side shared with a cell of the same piece runs on past the cell's edge (into the neighbour, which
    // does the same), a corner fills only where the diagonal cell is in the piece too (an inner corner stays clean,
    // square), so only the shape's outer corners are rounded
    int m = int(vB.x + 0.5);
    float h = 0.5 - uGap * 0.5 / sc, E = 0.7;
    vec2 lo = vec2((m & 1) != 0 ? -E : -h, (m & 8) != 0 ? -E : -h);
    vec2 hi = vec2((m & 2) != 0 ? E : h, (m & 4) != 0 ? E : h);
    // a row (run on left and right), a column (up and down), and the corners filled (the cell's quarter, square)
    float d = min(dBox(u - vec2(lo.x + hi.x, 0.0) * 0.5, vec2(hi.x - lo.x, 2.0 * h) * 0.5, 0.035),
                  dBox(u - vec2(0.0, lo.y + hi.y) * 0.5, vec2(2.0 * h, hi.y - lo.y) * 0.5, 0.035));
    if ((m & 21) == 21) d = min(d, dBox(u - vec2(-0.5, 0.5), vec2(0.5), 0.0));
    if ((m & 38) == 38) d = min(d, dBox(u - vec2(0.5, 0.5), vec2(0.5), 0.0));
    if ((m & 73) == 73) d = min(d, dBox(u - vec2(-0.5, -0.5), vec2(0.5), 0.0));
    if ((m & 138) == 138) d = min(d, dBox(u - vec2(0.5, -0.5), vec2(0.5), 0.0));
    float a = cov(d);
    vec3 c = col;
    if (abs(vB.z - 1.0) < 0.5) {
        // the ghost: a faint wash of the piece's color inside a thin line of it (one line around the piece's shape)
        float W = uLine / sc;
        a = max(a * 0.16, cov(abs(d + W * 0.5) - W * 0.5) * 0.7);
    } else if (vC.w < 0.0) c = mix(c, uShine, -vC.w);   // a lock's brief brightening
    else {
        float l = dot(c, vec3(0.299, 0.587, 0.114));     // the game over's greying
        c = mix(c, vec3(l) * 0.8 + 0.1, vC.w);
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

} // namespace fractal_shaders

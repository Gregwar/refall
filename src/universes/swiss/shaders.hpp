#pragma once
// SWISS POSTER shaders: the travel poster's landscape (one full-screen pass: sky, clouds, the ranges, the lake, the
// meadow and its village, the foreground) and flat, crisp shapes (rectangles, rounded plates, discs, birds) for the
// board and what moves over the landscape.

namespace swiss_shaders {

// Instanced shapes in pixels (top-left origin), clipped to a rectangle. Kinds: 0 rectangle, 1 rounded rectangle
// (p1: corner radius, pixels), 2 disc (p1: radius, pixels), 3 bird (p1: wing beat -1 down .. 1 up).
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // -1..1
layout(location = 1) in vec4 aRect;     // x0, y0, x1, y1 (pixels)
layout(location = 2) in vec4 aCol;      // sRGB, straight alpha
layout(location = 3) in vec4 aKind;     // kind, p1, p2, p3
layout(location = 4) in vec4 aClip;     // x0, y0, x1, y1 (pixels)
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vRect, vCol, vKind, vClip;
void main() {
    vec2 lo = aRect.xy - 1.0, hi = aRect.zw + 1.0;
    vec2 p = mix(lo, hi, aCorner * 0.5 + 0.5);
    vPix = p;
    vRect = aRect; vCol = aCol; vKind = aKind; vClip = aClip;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vRect, vCol, vKind, vClip;
out vec4 frag;
float span(float p, float a, float b) { return clamp(min(p - a, b - p) + 0.5, 0.0, 1.0); }
void main() {
    float cov;
    vec2 c = 0.5 * (vRect.xy + vRect.zw), hs = 0.5 * (vRect.zw - vRect.xy);
    if (vKind.x < 0.5) {
        cov = span(vPix.x, vRect.x, vRect.z) * span(vPix.y, vRect.y, vRect.w);
    } else if (vKind.x < 1.5) {
        float r = min(vKind.y, min(hs.x, hs.y));
        vec2 q = abs(vPix - c) - (hs - r);
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
        cov = clamp(0.5 - d, 0.0, 1.0);
    } else if (vKind.x < 2.5) {
        cov = clamp(vKind.y - length(vPix - c) + 0.5, 0.0, 1.0);
    } else {
        // a bird: two wings from a small body, the inner wing raised by the beat, the outer one trailing
        vec2 l = (vPix - c) / hs.x;
        l.y = -l.y;
        float ax = abs(l.x);
        float f = vKind.y;
        float yw = 0.5 * f * min(ax / 0.45, 1.0) * 0.45 + (0.15 * f - 0.28) * max(ax - 0.45, 0.0)
                 - 0.06 * smoothstep(0.0, 0.2, ax);
        float th = mix(0.15, 0.035, smoothstep(0.0, 1.0, ax));
        float d = (abs(l.y - yw) - th) * hs.x;
        d = max(d, (ax - 1.0) * hs.x);
        cov = clamp(0.5 - d, 0.0, 1.0);
    }
    cov *= span(vPix.x, vClip.x, vClip.z) * span(vPix.y, vClip.y, vClip.w);
    float a = vCol.a * cov;
    frag = vec4(vCol.rgb * a, a);
}
)";

// The landscape, in board units (y up): back to front, each depth shifted by its own parallax offset.
inline const char* LAND_FS = R"(#version 330 core
out vec4 frag;
uniform vec2 uRes;
uniform vec3 uView;             // the view's center (board units), pixels per unit
uniform float uTime;
uniform float uOff[7];          // the camera's travel in each depth's units (sky, far range, hero + first range, second,
                                // third + lake, meadow, foreground), wrapped to each depth's period
uniform vec3 uCloudOff;         // each cloud band's drift and travel, wrapped to its period
uniform float uSeed;
uniform vec3 uSkyTop, uSkyBot, uHaze, uCloudC, uSunC;
uniform vec3 uSnow, uSnowSh, uRock, uRockSh;
uniform vec3 uMid1, uMid2, uMid3;
uniform vec3 uHeroLit, uHeroSh, uHeroGrass;
uniform vec3 uLake, uLakeLit;
uniform vec3 uMeadow, uMeadowLit, uMeadowDk, uFg, uDot;
uniform vec3 uPath, uWall, uWallSh, uRoof, uRoofSh, uTimber, uWindow;
uniform vec3 uTree, uTreeLit, uTree2, uTree2Lit;
uniform vec4 uP0;               // far range: base, snow line, height, summits' rounding
uniform vec4 uP1;               // hero kind (0 none, 1 snow horn, 2 limestone), hero x, lake level, mist
uniform vec4 uP2;               // hero x, lake everywhere (1) or in stretches (0), clouds' flatness, cloud cover
uniform vec4 uP3;               // meadow level, foreground level, -, mid ranges' height
uniform vec3 uSun;              // x, y, radius
uniform vec4 uGrade;            // warmth, calm haze, cool (slow motion), the sun's rays
uniform float uWind;
uniform int uElCount, uElFg;    // elements: [0, uElFg) on the meadow, [uElFg, uElCount) in the foreground
uniform vec4 uEl[160];          // x (layer units), depth under the crest, size, kind
// The meadow's stretches (village, forest, valley, viaduct, chapel hill, pass...): 16 of SPAN units from uStOrg along
// its period, each lifting or sinking the ground; the roads and rivers running down from them (x, kind 1 path,
// 2 zigzag road, 3 river).
uniform float uLift[16];
uniform float uStOrg;
uniform int uRoadN;
uniform vec4 uRoad[16];
uniform float uGold;            // the drop's golden hour 0..1
uniform float uZoom;            // the drop's push-in: each depth scaled by 1 + uZoom x its parallax share
uniform vec2 uShadow;           // the cast shadows' direction (-1 left, 1 right) and length
uniform vec2 uSweep;            // a light sweeping across the layers on the drop: its position (board x), strength

float PU; // board units per pixel

float H(float n) { return fract(sin(n * 12.9898 + uSeed * 0.731) * 43758.5453); }
float H2(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233)) + uSeed * 0.317) * 43758.5453); }
float lnoise(float x) { float i = floor(x); return mix(H(i), H(i + 1.0), fract(x)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(H2(i), H2(i + vec2(1, 0)), f.x), mix(H2(i + vec2(0, 1)), H2(i + vec2(1, 1)), f.x), f.y);
}
// The scenery loops: every depth is periodic in x (its noises' lattices wrap at its period, n cells).
float lnoiseP(float x, float n) { float i = floor(x); return mix(H(mod(i, n)), H(mod(i + 1.0, n)), fract(x)); }
float vnoiseP(vec2 p, float n) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float x0 = mod(i.x, n), x1 = mod(i.x + 1.0, n);
    return mix(mix(H2(vec2(x0, i.y)), H2(vec2(x1, i.y)), f.x), mix(H2(vec2(x0, i.y + 1.0)), H2(vec2(x1, i.y + 1.0)), f.x), f.y);
}
// x - c folded into [-P/2, P/2): the nearest copy of something at c in a world of period P.
float wrapd(float x, float c, float P) { return mod(x - c + 0.5 * P, P) - 0.5 * P; }
const float TAU = 6.2831853;
const float P_MEADOW = 384.0, P_FG = 576.0, SPAN = 24.0;
// Coverage of an implicit field (positive inside), antialiased over a pixel (uniform control flow only).
float fill(float v) { return clamp(v / max(fwidth(v), 1e-5) + 0.5, 0.0, 1.0); }
// Coverage of a signed distance in board units (negative inside).
float cov(float d) { return clamp(0.5 - d / PU, 0.0, 1.0); }
float side(vec2 p, vec2 a, vec2 b) { vec2 e = b - a; return dot(p - a, normalize(vec2(e.y, -e.x))); }
float sdTri(vec2 p, vec2 a, vec2 b, vec2 c) { return max(max(side(p, a, b), side(p, b, c)), side(p, c, a)); }
// a parallelogram o + u a + v b (u, v in 0..1), any orientation
float sdPar(vec2 p, vec2 o, vec2 a, vec2 b) {
    float s = sign(a.x * b.y - a.y * b.x);
    vec2 A = o, B = o + a, C = o + a + b, D = o + b;
    if (s < 0.0) { vec2 t = B; B = D; D = t; }
    return max(max(side(p, A, B), side(p, B, C)), max(side(p, C, D), side(p, D, A)));
}
float sdBox(vec2 p, vec2 lo, vec2 hi) { vec2 d = max(lo - p, p - hi); return max(d.x, d.y); }
float smin(float a, float b, float k) { float h = max(k - abs(a - b), 0.0) / k; return min(a, b) - h * h * k * 0.25; }
float smax(float a, float b, float k) { return -smin(-a, -b, k); }
void paint(inout vec3 col, vec3 c, float a) { col = mix(col, c, clamp(a, 0.0, 1.0)); }

const float DEP[7] = float[7](0.03, 0.08, 0.16, 0.26, 0.38, 0.6, 1.0);
float PU0;
// The view point in depth i's own units: pushed in about a point low in the scene, shifted by its parallax.
vec2 lay(vec2 pw, int i) {
    float s = 1.0 + uZoom * DEP[i];
    PU = PU0 / s;
    vec2 pv = vec2(uView.x, -4.0);
    return pv + (pw - pv) / s + vec2(uOff[i], 0.0);
}
// The rim of light along a ridge's sunny faces at the golden hour.
vec3 rim(vec3 c, float h, float y, float lit) {
    float band = fill(h - y) - fill(h - y - 0.16);
    return mix(c, mix(c, uSunC, 0.6), uGold * lit * band);
}

// A range of peaks: tents of random heights and slopes (summits rounded by r), the highest of them, plus a jagged
// detail. L: the range's own seed.
float tents(float x, float L, float cw, vec2 hr, vec2 sr, float r) {
    float c = floor(x / cw), h = -1e4;
    for (int i = -2; i <= 2; i++) {
        float k = c + float(i), kk = mod(k, L < 1.5 ? 24.0 : 20.0); // peaks a period: 24 (far range), 20
        float px = (k + 0.2 + 0.6 * H(kk * 1.7 + L * 31.0)) * cw;
        float ph = mix(hr.x, hr.y, H(kk * 2.3 + L * 17.0));
        float s = mix(sr.x, sr.y, H(kk * 3.1 + L * 7.0));
        float sl = s * (0.7 + 0.6 * H(kk * 4.1 + L)), srt = s * (0.7 + 0.6 * H(kk * 5.3 + L * 2.0));
        float t = smin(ph + (x - px) * sl, ph - (x - px) * srt, r + 0.02);
        h = smax(h, t, 0.25 + r * 0.5);
    }
    return h;
}
float jag(float x, float L, float a, float P) {
    return a * ((lnoiseP(x * 1.7 + L * 13.0, 1.7 * P) - 0.5) + 0.45 * (lnoiseP(x * 4.1 + L * 7.0, 4.1 * P) - 0.5));
}
// Lit (1) where the ridge, looked up along the light (down to the right), rises: the faces toward the sun.
float facet(float x, float y, float h, float L, float cw, vec2 hr, vec2 sr, float r, float lean) {
    float xq = x + (h - y) * lean;
    float d = tents(xq + 0.05, L, cw, hr, sr, r) - tents(xq - 0.05, L, cw, hr, sr, r);
    return fill(d * 4.0);
}

// The stretches' lift: each stretch's own, blended smoothly into the next one's.
float lift(float x) {
    float u = (x - uStOrg) / SPAN - 0.5;
    float i = floor(u), f = fract(u);
    float a = uLift[int(mod(i, 16.0))], b = uLift[int(mod(i + 1.0, 16.0))];
    return mix(a, b, smoothstep(0.0, 1.0, f));
}
float meadowH(float x) {
    return uP3.x + 0.9 * sin(x * (TAU * 10.0 / P_MEADOW) + 1.3 + uSeed) + 0.5 * sin(x * (TAU * 25.0 / P_MEADOW) + 0.5 + uSeed * 2.0)
         + 0.18 * sin(x * (TAU * 67.0 / P_MEADOW) + uSeed) + lift(x);
}
float fgH(float x) {
    return uP3.y + 0.9 * sin(x * (TAU * 12.0 / P_FG) + 2.0 + uSeed * 3.0) + 0.45 * sin(x * (TAU * 34.0 / P_FG) + 1.0)
         + 0.12 * sin(x * (TAU * 119.0 / P_FG) + uSeed);
}

// ---- The village and the trees.
// A house seen from three quarters: its gable front lit, its long side in shade, the roof slab over it.
void house(vec2 q, float s, float kind, float hs, inout vec3 col) {
    q /= s;
    float fw = 0.75 + 0.35 * fract(hs * 7.1), wh = 0.62 + 0.38 * fract(hs * 3.7), rh = 0.42 * fw + 0.08;
    float sw = 0.9 + 0.8 * fract(hs * 5.3), k = 0.26;
    bool chalet = kind < 0.5;
    // the long side (shade), its windows
    vec2 o = vec2(fw, 0.0);
    float dSide = sdPar(q, o, vec2(0.0, wh), vec2(sw, sw * k));
    vec3 sideC = uWallSh;
    if (chalet) sideC = mix(uWallSh, uTimber, step(wh * 0.55, q.y - (q.x - fw) * k));
    paint(col, sideC, cov(dSide * s));
    {
        float lu = (q.x - fw) / sw, lv = q.y - (q.x - fw) * k;
        float n = floor(sw / 0.32);
        float cu = fract(lu * n), row = floor(lv / (wh * 0.5));
        float inW = step(0.3, cu) * step(cu, 0.7) * step(0.1, fract(lv / (wh * 0.5))) * step(fract(lv / (wh * 0.5)), 0.5);
        float m = inW * step(0.0, lu) * step(lu, 1.0) * step(0.0, lv) * step(lv, wh) * step(row, 1.0);
        paint(col, uWindow, m * cov(dSide * s) * 0.9);
    }
    // the roof slab over the long side
    vec2 E = vec2(fw + 0.07, wh - 0.05), A = vec2(fw * 0.5, wh + rh + 0.03);
    vec2 b = vec2(sw + 0.08, (sw + 0.08) * k);
    float dRoof = sdPar(q, E, A - E, b);
    paint(col, uRoof, cov(dRoof * s));
    // its eave's shade line
    paint(col, uRoofSh, cov(max(dRoof, -(side(q, E, E + b) + 0.06)) * s));
    // the gable front (lit), the upper floor in timber for a chalet
    float dFront = min(sdBox(q, vec2(0.0), vec2(fw, wh)), sdTri(q, vec2(0.0, wh - 0.001), vec2(fw, wh - 0.001), vec2(fw * 0.5, wh + rh)));
    vec3 frontC = chalet ? mix(uWall, uTimber, step(wh * 0.55, q.y)) : uWall;
    paint(col, frontC, cov(dFront * s));
    // windows on the front: two by two (one in the gable)
    {
        vec2 w = vec2(fw * 0.5, 0.0);
        float d = 1e4;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++) {
                vec2 c = vec2(fw * (0.28 + 0.44 * float(i)), wh * (0.28 + 0.46 * float(j)));
                d = min(d, sdBox(q, c - vec2(0.065, 0.085), c + vec2(0.065, 0.085)));
            }
        d = min(d, sdBox(q, vec2(fw * 0.5 - 0.05, wh + rh * 0.3), vec2(fw * 0.5 + 0.05, wh + rh * 0.3 + 0.12)));
        paint(col, uWindow, cov(d * s));
    }
    // the roof's front edge along both slopes
    float t = 0.07;
    vec2 L0 = vec2(-0.07, wh - 0.05), R0 = vec2(fw + 0.07, wh - 0.05), Ap = vec2(fw * 0.5, wh + rh + 0.03);
    float dl = max(max(side(q, L0, Ap), -side(q, L0 + vec2(0.0, -t), Ap + vec2(0.0, -t))), max(q.y - Ap.y - 0.01, L0.y - t - q.y));
    float dr = max(max(side(q, Ap, R0), -side(q, Ap + vec2(0.0, -t), R0 + vec2(0.0, -t))), max(q.y - Ap.y - 0.01, R0.y - t - q.y));
    paint(col, uRoof, cov(min(dl, dr) * s));
}

// The church: a tower with a clock and a spire beside a nave.
void church(vec2 q, float s, float hs, inout vec3 col) {
    house(q + vec2(1.05 * s, 0.0), s * 1.05, 1.0, hs * 1.3 + 0.37, col);
    q /= s;
    float tw = 0.42, th = 2.0 + 0.4 * fract(hs * 9.1);
    // the tower's side in shade, its front lit
    paint(col, uWallSh, cov(sdPar(q, vec2(tw, 0.0), vec2(0.0, th), vec2(0.2, 0.05)) * s));
    paint(col, uWall, cov(sdBox(q, vec2(0.0), vec2(tw, th)) * s));
    // the belfry's opening, the clock
    paint(col, uWindow, cov(sdBox(q, vec2(tw * 0.32, th - 0.42), vec2(tw * 0.68, th - 0.16)) * s));
    float dc = length(q - vec2(tw * 0.5, th - 0.75)) - 0.11;
    paint(col, uRoof, cov((abs(dc + 0.02) - 0.022) * s));
    // the spire (a roof-colored onion point) and its cross
    vec2 a = vec2(-0.05, th), b = vec2(tw + 0.05, th), c = vec2(tw * 0.5, th + 0.85);
    paint(col, uRoof, cov(sdTri(q, a, b, c) * s));
    paint(col, uRoofSh, cov(sdTri(q, vec2(tw * 0.5, th), b, c) * s));
    float cross = min(sdBox(q, vec2(tw * 0.5 - 0.015, th + 0.85), vec2(tw * 0.5 + 0.015, th + 1.1)),
                      sdBox(q, vec2(tw * 0.5 - 0.07, th + 0.98), vec2(tw * 0.5 + 0.07, th + 1.01)));
    paint(col, uTimber, cov(cross * s));
}

// A fir: three stacked tiers, the sunny half lighter; its top sways with the wind.
void fir(vec2 q, float s, vec3 dk, vec3 lit, float ph, inout vec3 col) {
    q /= s;
    q.x -= uWind * sin(uTime * 1.1 + ph) * q.y * q.y * 0.04;
    paint(col, uTimber, cov(sdBox(q, vec2(-0.05, -0.05), vec2(0.05, 0.3)) * s));
    float d = 1e4;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float y0 = 0.22 + fi * 0.5, w = 0.48 - fi * 0.11, h = 0.95 - fi * 0.1;
        d = min(d, sdTri(q, vec2(-w, y0), vec2(w, y0), vec2(0.0, y0 + h)));
    }
    float c = cov(d * s);
    paint(col, mix(lit, dk, smoothstep(-PU, PU, q.x * s)), c);
}

// A cypress: a tall pointed flame, the sunny side lighter.
void cypress(vec2 q, float s, vec3 dk, vec3 lit, float ph, inout vec3 col) {
    q /= s;
    q.x -= uWind * sin(uTime * 0.9 + ph) * q.y * q.y * 0.03;
    float Hh = 2.6, t = clamp(q.y / Hh, 0.0, 1.0);
    float w = 0.36 * pow(sin(3.14159 * mix(0.08, 1.0, t)), 0.7) * (1.0 - 0.25 * t);
    float d = max(abs(q.x) - w, max(-q.y + 0.05, q.y - Hh)) * 0.8;
    paint(col, mix(lit, dk, smoothstep(-0.12 - PU, -0.12 + PU, q.x * s + 0.0)), cov(d * s));
}

// A round tree or a bush: three discs.
void bush(vec2 q, float s, vec3 dk, vec3 lit, inout vec3 col) {
    q /= s;
    float d = min(min(length(q - vec2(-0.3, 0.3)) - 0.32, length(q - vec2(0.3, 0.28)) - 0.3), length(q - vec2(0.0, 0.55)) - 0.38);
    d = max(d, -q.y);
    float l = length(q - vec2(-0.25, 0.65)) - 0.42;
    paint(col, dk, cov(d * s));
    paint(col, lit, cov(max(d, l) * s));
}

// A stone viaduct across a valley: arches on piers standing on the slopes, its deck level with the higher rim, and
// now and then a little train crossing it.
void viaduct(vec2 q, vec4 e, float yb, inout vec3 col) {
    float s = e.z, span = 6.0 * s;
    if (abs(q.x) > span) return;
    float hd = max(meadowH(e.x - span), meadowH(e.x + span)) - e.y - yb + 0.25 * s;
    float g = meadowH(e.x + q.x) - e.y - yb;
    if (q.y < g - 0.05 || q.y > hd + 0.6 * s) return;
    float a = 1.15 * s, r = 0.4 * a;
    float fu = (fract(q.x / a) - 0.5) * a;
    float archTop = hd - 0.42 * s - (r - sqrt(max(r * r - fu * fu, 0.0)));
    float open = (abs(fu) < r && q.y < archTop) ? 1.0 : 0.0;
    float body = cov(max(q.y - hd, abs(q.x) - span)) * (1.0 - open);
    paint(col, mix(uWall, uWallSh, 0.35), body);
    paint(col, uWallSh, body * step(hd - 0.1 * s, q.y));
    // the train: three carriages running across, every so often
    float tx = (fract(uTime * 0.035 + fract(e.x * 0.31)) * 2.0 - 1.0) * (span + 4.0 * s);
    float tq = q.x - tx;
    float car = step(abs(fract(tq / (0.9 * s)) - 0.5), 0.46);
    float tb = max(max(abs(tq + 1.35 * s) - 1.35 * s, abs(q.x) - span), max(hd - q.y, q.y - hd - 0.32 * s));
    paint(col, uRoof, cov(tb) * car);
    float win = step(abs(fract(tq / (0.3 * s)) - 0.5), 0.22) * step(hd + 0.14 * s, q.y) * step(q.y, hd + 0.24 * s);
    paint(col, uWindow, cov(tb) * car * win);
}

void element(vec2 p, vec4 e, float off, bool fg, inout vec3 col) {
    float s = e.z, kind = e.w;
    float dx = wrapd(p.x, e.x, fg ? P_FG : P_MEADOW);
    if (abs(dx) > (kind > 6.5 ? 6.5 : 4.0) * s) return;
    float yb = (fg ? fgH(e.x) : meadowH(e.x)) - e.y;
    vec2 q = vec2(dx, p.y - yb);
    if (kind > 6.5) { viaduct(q, e, yb, col); return; }
    if (q.y < -0.4 || q.y > 4.0 * s) return;
    float hs = fract(e.x * 0.137 + e.y * 1.31);
    if (kind < 1.5) house(q, s, kind, hs, col);
    else if (kind < 2.5) church(q, s, hs, col);
    else if (kind < 3.5) fir(q, s, uTree, uTreeLit, e.x, col);
    else if (kind < 4.5) cypress(q, s, uTree, uTreeLit, e.x, col);
    else if (kind < 5.5) bush(q, s, uTree, uTreeLit, col);
    else fir(q, s, uTree2, uTree2Lit, e.x, col);
}

// Its cast shadow on the grass, to the right.
float shadowOf(vec2 p, vec4 e, float off, bool fg) {
    if (e.w > 6.5) return 0.0;
    float w = (e.w < 2.5 ? 1.6 : 0.8) * uShadow.y;
    float dx = wrapd(p.x, e.x, fg ? P_FG : P_MEADOW);
    if (abs(dx) > 3.0 * w * e.z) return 0.0;
    float yb = (fg ? fgH(e.x) : meadowH(e.x)) - e.y;
    vec2 q = (vec2(dx, p.y) - vec2(uShadow.x * w * 0.55 * e.z, yb + 0.02)) / (vec2(w, 0.18) * e.z);
    return cov((length(q) - 1.0) * 0.18 * e.z);
}

// ---- Clouds: a flat bottom and round tops, in three bands drifting at their own speed.
float cloudD(vec2 p, int band) {
    float fb = float(band);
    float y0 = 10.6 - fb * 2.3 + 0.0;
    float cw = 10.0 + 3.0 * fb;
    float xx = p.x + uCloudOff[band];
    float c = floor(xx / cw), d = 1e4;
    float fl = mix(1.0, 0.45, uP2.z);
    for (int i = -1; i <= 1; i++) {
        float k = c + float(i), kk = mod(k, 12.0); // 12 cells a period
        if (H(kk * 3.7 + fb * 11.0) > uP2.w) continue;
        float cx = (k + 0.25 + 0.5 * H(kk * 5.1 + fb)) * cw;
        float w = (1.6 + 2.2 * H(kk * 7.3 + fb * 3.0)) * (1.0 + 0.5 * uP2.z);
        float yb = y0 + 1.2 * (H(kk * 9.7 + fb) - 0.5);
        vec2 q = vec2(xx - cx, (p.y - yb) / fl);
        float r0 = 0.45 * w * 0.5;
        float base = length(max(abs(q - vec2(0.0, r0)) - vec2(w - r0, 0.0), 0.0)) - r0;
        float b1 = length(q - vec2(-w * 0.3, r0 * 1.3)) - w * 0.36;
        float b2 = length(q - vec2(w * 0.18, r0 * 1.6)) - w * 0.42;
        float b3 = length(max(abs(q - vec2(-w * 0.3, r0 * 2.3)) - vec2(w * 0.5 - r0, 0.0), 0.0)) - r0 * 0.85;
        float dd = min(base, uP2.z > 0.5 ? b3 : min(b1, b2));
        dd = max(dd, -q.y);
        d = min(d, dd * fl);
    }
    return d;
}

void main() {
    PU0 = 1.0 / uView.z;
    PU = PU0;
    vec2 pw = (gl_FragCoord.xy - 0.5 * uRes) * PU + uView.xy;
    vec2 p = pw;

    // ---- Sky: a flat gradient, the sun with its halos, the rays of a drop.
    float sy = clamp((p.y + 2.0) / 15.0, 0.0, 1.0);
    vec3 col = mix(uSkyBot, uSkyTop, smoothstep(0.0, 1.0, sy));
    vec2 sp = vec2(uSun.x, uSun.y);
    {
        vec2 d = p - sp;
        float a = atan(d.y, d.x);
        float rays = step(0.5, fract(a / 6.28318 * 18.0 + uTime * 0.004));
        paint(col, mix(col, uSunC, 0.45), uGrade.w * 0.42 * rays * smoothstep(uSun.z, uSun.z * 3.0, length(d)));
        float r = length(d);
        paint(col, mix(col, uSunC, 0.35), cov(r - uSun.z * 2.1) * 0.55);
        paint(col, mix(col, uSunC, 0.6), cov(r - uSun.z * 1.5) * 0.6);
        paint(col, uSunC, cov(r - uSun.z));
    }
    for (int b = 2; b >= 0; b--) {
        float d = cloudD(p, b);
        paint(col, mix(uCloudC, uSkyTop, 0.12 * float(b)), cov(d) * 0.94);
    }

    // ---- The far range: snow and rock, lit and shaded faces.
    {
        vec2 p = lay(pw, 1);
        float L = 1.0, x = p.x;
        vec2 hr = vec2(uP0.x + uP0.z * 0.45, uP0.x + uP0.z), sr = vec2(0.75, 1.35);
        float cw = 5.0;
        // massifs and gaps along the range: the high ones carry their glaciers lower
        float env = 0.5 + 0.5 * sin(x * (TAU * 2.0 / 120.0) + uSeed);
        float h = uP0.x + (tents(x, L, cw, hr, sr, uP0.w) - uP0.x) * (0.68 + 0.38 * env) + jag(x, L, 0.45, 120.0);
        float lit = facet(x, p.y, h, L, cw, hr, sr, uP0.w, 0.45);
        float sl = uP0.y - 1.6 * env + 1.1 * (lnoiseP(x * 0.45, 54.0) - 0.5) + 0.5 * (lnoiseP(x * 1.5 + 3.0, 180.0) - 0.5);
        float sn = (p.y - sl) + 1.5 * (vnoiseP(vec2(x * 1.4, p.y * 0.5), 168.0) - 0.5) + 0.8 * (vnoiseP(vec2(x * 3.1, p.y * 1.8), 372.0) - 0.5);
        sn += (lit - 0.5) * 0.5;
        float snow = fill(sn);
        vec3 rock = mix(uRockSh, uRock, lit), ice = mix(uSnowSh, uSnow, lit);
        vec3 c = mix(rock, ice, snow);
        c = mix(c, uHaze, uP1.w * 0.35 * smoothstep(0.0, 6.0, h - p.y));
        c = rim(c, h, p.y, lit);
        paint(col, c, fill(h - p.y));
    }

    // ---- The hero peak (a snow horn or a limestone tooth), then three receding ranges and the lake.
    float midH = uP3.w;
    {
        vec2 p = lay(pw, 2);
        float x = p.x;
        vec3 m = uMid1;
        float L = 2.0, cw = 6.5;
        vec2 hr = vec2(midH * 0.55, midH), sr = vec2(0.45, 0.9);
        float h = tents(x, L, cw, hr, sr, 0.6 + uP0.w) + jag(x, L, 0.25, 130.0);
        float lit = facet(x, p.y, h, L, cw, hr, sr, 0.6 + uP0.w, 0.5);
        vec3 c = mix(m, mix(m, uHaze, 0.24), lit);
        c = mix(c, uHaze, uP1.w * smoothstep(0.0, 5.0, h - p.y));
        if (uP1.x > 0.5) {
            // a hero every 65 units, every other one a little lower
            float dx = wrapd(x, uP2.x, 65.0);
            float top = midH + 6.5 - 1.3 * mod(floor((x - uP2.x + 32.5) / 65.0), 2.0);
            float hh = top - (dx < 0.0 ? -dx * 0.85 : dx * 1.9) + jag(x, 9.0, 0.35, 130.0);
            float hl = fill((top - p.y) * 0.22 - dx);
            vec3 hc;
            if (uP1.x < 1.5) {
                float sn = (p.y - (top - 4.5)) + 2.0 * (vnoiseP(vec2(x * 1.6, p.y * 0.45), 208.0) - 0.5) + (hl - 0.5) * 1.2;
                hc = mix(mix(uRockSh, uRock, hl), mix(uSnowSh, uSnow, hl), fill(sn));
            } else {
                float u = dx - (p.y - top) * 0.42;
                u = u * 1.3 + 0.6 * vnoise(vec2(u * 0.7, p.y * 0.35));
                float band = fill(0.09 - abs(fract(u) - 0.5)) * step(0.35, H(floor(u) + 4.0));
                hc = mix(uHeroSh, uHeroLit, hl);
                hc = mix(hc, mix(uHeroLit, uHeroSh, 0.6), band * hl);
                float gr = (top - 5.2 - dx * 0.3) - p.y + 1.4 * (vnoiseP(vec2(x * 1.2, p.y * 0.6), 156.0) - 0.5);
                hc = mix(hc, mix(uHeroGrass, mix(uHeroGrass, uMid2, 0.4), 1.0 - hl), fill(gr));
            }
            float inH = fill(hh - p.y);
            // the hero stands in front of the first range
            c = mix(c, hc, inH);
            h = max(h, hh);
            lit = mix(lit, hl, inH);
        }
        c = rim(c, h, p.y, lit);
        paint(col, c, fill(h - p.y));
    }
    {
        vec2 p = lay(pw, 3);
        float x = p.x;
        float L = 3.0, cw = 5.5;
        vec2 hr = vec2(midH * 0.3, midH * 0.7), sr = vec2(0.35, 0.75);
        float h = tents(x, L, cw, hr, sr, 0.9 + uP0.w) + jag(x, L, 0.2, 110.0) - 1.5;
        float lit = facet(x, p.y, h, L, cw, hr, sr, 0.9 + uP0.w, 0.5);
        vec3 c = mix(uMid2, mix(uMid2, uHaze, 0.2), lit);
        c = mix(c, uHaze, uP1.w * smoothstep(0.0, 5.0, h - p.y));
        c = rim(c, h, p.y, lit);
        paint(col, c, fill(h - p.y));
    }
    {
        vec2 p = lay(pw, 4);
        float x = p.x;
        float L = 4.0, cw = 7.0;
        vec2 hr = vec2(midH * 0.1, midH * 0.45), sr = vec2(0.3, 0.6);
        float h = tents(x, L, cw, hr, sr, 1.2 + uP0.w) + jag(x, L, 0.15, 140.0) - 3.0;
        float lit = facet(x, p.y, h, L, cw, hr, sr, 1.2 + uP0.w, 0.5);
        vec3 c = mix(uMid3, mix(uMid3, uHaze, 0.16), lit);
        c = mix(c, uHaze, uP1.w * 0.8 * smoothstep(0.0, 5.0, h - p.y));
        c = rim(c, h, p.y, lit);
        paint(col, c, fill(h - p.y));
        // the lake: a flat water plane with long glints, the sun's path on it
        {
            float ly = uP1.z;
            // the lake's stretches (two a period, or everywhere in the lake scenes); between them the shore runs back
            // in perspective and the valley's floor takes over, in fields
            float xs = x + (ly - p.y) * 1.6;
            float lk = max(uP2.y, smoothstep(0.1, 0.25, sin(xs * (TAU * 2.0 / 140.0) + uSeed * 1.7)));
            vec3 floorC = mix(mix(uMeadow, uMid3, 0.55), uHaze, 0.15);
            float fieldB = step(0.5, fract((p.y + 0.3 * sin(x * (TAU * 9.0 / 140.0))) * 1.4));
            floorC = mix(floorC, mix(floorC, uMeadowLit, 0.14), fieldB);
            paint(col, floorC, fill(ly - p.y) * (1.0 - lk));
            float rows = 3.0;
            float r = floor(p.y * rows);
            float nr = 60.0 + floor(H(r + 3.0) * 60.0); // glints a period on this row
            float u = x * nr / 140.0 + uTime * 0.06 * (H(r) - 0.5);
            float seg = mod(floor(u), nr);
            float sunNear = exp(-pow((pw.x - sp.x) / (1.0 + (ly - p.y) * 0.6), 2.0));
            float on = step(H(seg * 1.3 + r * 7.1), 0.32 + 0.6 * sunNear);
            float g = abs(fract(p.y * rows) - 0.5) * 2.0;
            float glint = on * step(0.15, fract(u)) * step(fract(u), 0.85) * fill((0.25 - g) * 3.0);
            vec3 w = mix(uLake, uLakeLit, glint * 0.9);
            w = mix(w, mix(uLake, uSkyBot, 0.35), smoothstep(ly - 0.25, ly, p.y));
            paint(col, w, fill(ly - p.y) * lk);
        }
    }

    // ---- The meadow: grass, its flowers and darker patches, the path, the village and its trees.
    {
        vec2 p = lay(pw, 5);
        float off = 0.0, x = p.x;
        float h = meadowH(x);
        float inM = fill(h - p.y);
        vec3 g = uMeadow;
        float crest = fill((h - p.y) - 0.0) * (1.0 - fill((h - p.y) - 0.3 - 0.15 * sin(x * (TAU * 55.0 / P_MEADOW))));
        g = mix(g, uMeadowLit, crest);
        float patchN = vnoiseP(vec2(x * 0.3125, p.y * 0.8), 120.0) + 0.3 * vnoiseP(vec2(x * 1.09375, p.y * 2.3), 420.0);
        g = mix(g, uMeadowDk, fill((patchN - 1.02) * 2.0) * 0.7);
        // flowers: a sparse dotting, larger nearer
        {
            float cs = 0.32;
            vec2 cell = floor(vec2(x, p.y) / cs);
            vec2 cid = vec2(mod(cell.x, 1200.0), cell.y);
            vec2 j = vec2(H2(cid), H2(cid + 17.0));
            vec2 c = (cell + 0.2 + 0.6 * j) * cs;
            float rr = 0.022 + 0.009 * clamp(h - p.y, 0.0, 4.0);
            float on = step(H2(cid + 5.0), 0.24);
            g = mix(g, uDot, on * cov(length(vec2(x, p.y) - c) - rr) * 0.9);
        }
        // the roads and rivers: a winding path down from a village, a road zigzagging down from a hilltop, a river
        // winding out of a valley's floor, each widening toward the foot of the poster
        for (int i = 0; i < uRoadN; i++) {
            vec4 rd = uRoad[i];
            float dx = wrapd(x, rd.x, P_MEADOW);
            if (abs(dx) > 16.0) continue;
            if (rd.y < 1.5) {
                float top = meadowH(rd.x + 1.6) - 1.0;
                float t = top - p.y;
                float cx = 1.6 + 1.5 * sin(t * 0.55 + 0.4) * min(1.0, t * 0.4) + t * 0.35;
                float w = 0.06 + 0.11 * max(t, 0.0);
                g = mix(g, uPath, cov(max(abs(dx - cx) - w, -t) * 0.9));
            } else if (rd.y < 2.5) {
                float top = meadowH(rd.x) - 0.45;
                float t = top - p.y;
                // switchbacks, longer and wider nearer
                float tri = abs(fract(t / (0.5 + 0.3 * t) + 0.25) - 0.5) * 4.0 - 1.0;
                float cx = (0.35 + 0.22 * t) * tri + t * 0.2;
                float w = 0.05 + 0.07 * max(t, 0.0);
                g = mix(g, uPath, cov(max(abs(dx - cx) - w, -t) * 0.6));
            } else {
                float top = meadowH(rd.x) - 0.12;
                float t = top - p.y;
                float cx = 1.3 * sin(t * 0.7 + 1.0) * min(1.0, t * 0.5) - t * 0.3;
                float w = 0.08 + 0.2 * max(t, 0.0);
                float inR = cov(max(abs(dx - cx) - w, -t));
                float flow = step(fract(t * 1.4 - uTime * 0.25 + 0.3 * sin(dx * 3.0)), 0.18) * step(abs(dx - cx), w * 0.55);
                g = mix(g, mix(uLake, uLakeLit, flow * 0.7), inR);
            }
        }
        paint(col, g, inM);
        float sh = 0.0;
        for (int i = 0; i < uElFg; i++) sh = max(sh, shadowOf(p, uEl[i], off, false));
        col = mix(col, mix(col, uMeadowDk * 0.8, 0.55), sh * inM);
        for (int i = 0; i < uElFg; i++) element(p, uEl[i], off, false, col);
    }

    // ---- The foreground hill and its trees.
    {
        vec2 p = lay(pw, 6);
        float off = 0.0, x = p.x;
        float h = fgH(x);
        float inF = fill(h - p.y);
        vec3 g = uFg;
        float crest = 1.0 - fill((h - p.y) - 0.22 - 0.1 * sin(x * (TAU * 119.0 / P_FG)));
        g = mix(g, mix(uFg, uMeadowLit, 0.5), crest);
        {
            float cs = 0.45;
            vec2 cell = floor(vec2(x, p.y) / cs);
            vec2 cid = vec2(mod(cell.x, 1280.0), cell.y) + 101.0;
            vec2 j = vec2(H2(cid), H2(cid + 17.0));
            vec2 c = (cell + 0.2 + 0.6 * j) * cs;
            float on = step(H2(cid + 5.0), 0.3);
            g = mix(g, mix(uDot, uFg, 0.3), on * cov(length(vec2(x, p.y) - c) - 0.05) * 0.85);
        }
        paint(col, g, inF);
        float sh = 0.0;
        for (int i = uElFg; i < uElCount; i++) sh = max(sh, shadowOf(p, uEl[i], off, true));
        col = mix(col, mix(col, uFg * 0.7, 0.5), sh * inF);
        for (int i = uElFg; i < uElCount; i++) element(p, uEl[i], off, true, col);
    }

    PU = PU0;
    // ---- The drop's light sweeping across the layers
    {
        float b = exp(-pow((pw.x + pw.y * 0.45 - uSweep.x) / 2.2, 2.0));
        col = mix(col, min(col * 1.18 + uSunC * 0.12, vec3(1.0)), b * uSweep.y);
    }

    // ---- The grade: the calm's haze, a drop's warmth, slow motion's cool stillness; a faint print grain.
    col = mix(col, uHaze, uGrade.y);
    col *= mix(vec3(1.0), vec3(1.06, 1.0, 0.9), uGrade.x);
    col = clamp(mix(vec3(dot(col, vec3(0.3, 0.59, 0.11))), col, 1.0 + 0.2 * uGrade.w), 0.0, 1.0);
    float l = dot(col, vec3(0.3, 0.59, 0.11));
    col = mix(col, l * vec3(0.84, 0.93, 1.12), uGrade.z * 0.55);
    col += (H2(floor(gl_FragCoord.xy) * 0.913 + 0.5) - 0.5) * 0.022;
    frag = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)";

} // namespace swiss_shaders

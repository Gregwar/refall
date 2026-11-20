#pragma once
// LIBRARY's shaders (see library.cpp).
//
// SOLID_VS / SOLID_FS draw everything solid, instanced, as toon: boxes, cylinders and the hemisphere, with their
// materials: painted wood, the bookcases' backs, the books (spines, pages, covers), planks, painted plaster and metal,
// lights, round toy shapes (bushes, pots, shades, globes, the rug), the round windows (the sky through a cross of
// bars), the backdrop (the sky, stars at night); the furniture that comes later scaled in from its floor by its
// feature's growth (uFeat). Cel lit (the sun in three bands with its shadow map, a cool colored shade,
// up to 16 lamps in soft-edged pools), inked along every box's edges and round the silhouettes of round shapes.
// SHADOW_FS writes the sun's depth. SPRITE_* are the glows (lamps, dust in the daylight); VEIL_FS the well's deep
// ground and the HUD's rounded cards; LINE_*
// screen-space lines (the ghost, a cleared row's light); DOWN / BLUR / FINAL the bloom and the frame; SHAPE / TEXT the
// HUD.
#include <string>

namespace library_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform float uTime;
// The drop's wave through the shelves: a pulse rolling along each shelf row once a beat, alternate rows the other
// way, a little later on each floor up. s: along the wall (m), y: height (m), level: the floor. 0..1.
uniform float uWaveK, uBeatPh;
float shelfWave(float s, float y, float level) {
    if (uWaveK <= 0.0) return 0.0;
    float row = floor(y / 0.95);
    float dir = mod(row, 2.0) < 0.5 ? 1.0 : -1.0;
    float d = fract(s * dir * 0.075 - uBeatPh - row * 0.035 - level * 0.12) - 0.5;
    return exp(-d * d / 0.011) * uWaveK;
}
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.9375;
}
)";

// ---------------------------------------------------------------- solids (instanced meshes)
// Instance: c (center, material), x y z (half axes, params p0 p1 p2), col (color, seed), e (-, -, floor, -).
inline const char* SOLID_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
layout(location = 7) in vec4 iE;
uniform mat4 uVP;
uniform vec3 uOrigin;
uniform float uFeat[4];   // the features' growth (0 not there .. 1 grown)
out vec3 vP, vL, vLN, vN;
flat out vec3 vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat, vE;
flat out float vWave;
void main() {
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    vec3 C = iC.xyz;
    vWave = 0.0;
    if (iE.x > -0.5 && iE.x < 3.5) {
        // a part of a feature: grown in from its floor (a little overshoot)
        float g = uFeat[int(iE.x + 0.5)];
        float u = g - 1.0;
        float k = g <= 0.0 ? 0.0 : 1.0 + 2.4 * u * u * u + 1.4 * u * u;
        X *= k; Y *= k; Z *= k;
        C.y = iE.y + (C.y - iE.y) * k;
    }
    if (uWaveK > 0.0 && int(iC.w + 0.5) == 3 && iE.z >= 0.0) {
        // a shelved book (its depth axis Z points out of its shelf): slid out and tipped forward about its foot
        vec3 out_ = normalize(Z);
        vec3 up = vec3(0.0, 1.0, 0.0);
        vec3 along = normalize(cross(up, out_));
        float w = shelfWave(dot(C, along), C.y, iE.z);
        vWave = w;
        if (w > 0.001) {
            float hu = abs(X.y) + abs(Y.y) + abs(Z.y);
            vec3 pivot = C - up * hu + out_ * length(Z);
            float a = 0.3 * w, ca = cos(a), sa = sin(a);
            // rotate in the (up, out) plane: the head comes forward
            vec3 v = C - pivot;
            float vy = dot(v, up), vo = dot(v, out_);
            vec3 vr = v + up * (vy * ca - vo * sa - vy) + out_ * (vo * ca + vy * sa - vo);
            C = pivot + vr + out_ * (0.11 * w);
            float xy, xo;
            xy = dot(X, up); xo = dot(X, out_); X += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
            xy = dot(Y, up); xo = dot(Y, out_); Y += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
            xy = dot(Z, up); xo = dot(Z, out_); Z += up * (xy * ca - xo * sa - xy) + out_ * (xo * ca + xy * sa - xo);
        }
    }
    vec3 P = C + X * aPos.x + Y * aPos.y + Z * aPos.z;
    vHalf = vec3(length(X), length(Y), length(Z));
    vAX = X / max(vHalf.x, 1e-6);
    vAY = Y / max(vHalf.y, 1e-6);
    vAZ = Z / max(vHalf.z, 1e-6);
    vN = normalize(vAX * aNrm.x / max(vHalf.x, 1e-6) + vAY * aNrm.y / max(vHalf.y, 1e-6) + vAZ * aNrm.z / max(vHalf.z, 1e-6));
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    vE = iE;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";

inline const char* LIGHTING = R"(
uniform vec3 uAmbSky, uAmbGround, uKeyDir, uKeyCol;
uniform int uNL;
uniform vec4 uLP[16];
uniform vec4 uLC[16];
uniform float uShadowOn, uShadowTexel, uShadowBias;
uniform sampler2DShadow uShadow;
uniform mat4 uShadowVP;
uniform vec3 uShadowO;
uniform float uFogD, uFogStart;
uniform vec3 uFogCol;
float shadowAt(vec3 P, vec3 N) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 s = uShadowVP * vec4(P - uShadowO + N * uShadowBias, 1.0);
    vec3 q = s.xyz / s.w * 0.5 + 0.5;
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
    float t = uShadowTexel;
    float r = 0.0;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) r += texture(uShadow, vec3(q.xy + vec2(i, j) * t * 1.2, q.z));
    return r / 9.0;
}
// Toon light: the sun in crisp bands (full, a half-lit band, the shade), the shade a cool colored light (the sky's)
// rather than a grey; a crisp highlight; the downlights in two soft-edged pools.
float toonBand(float l) { return smoothstep(0.02, 0.07, l) * 0.62 + smoothstep(0.36, 0.42, l) * 0.38; }
vec3 shade(vec3 P, vec3 N, vec3 alb, float spec, float rough, float ao) {
    vec3 V = normalize(uEye - P);
    vec3 amb = mix(uAmbGround, uAmbSky, 0.5 + 0.5 * N.y);
    float aoK = mix(0.72, 1.0, ao);
    vec3 c = alb * amb * aoK;
    float nk = dot(N, uKeyDir);
    if (nk > 0.0) {
        float sh = shadowAt(P, N);
        float band = toonBand(nk * sh);
        c += alb * uKeyCol * band * aoK;
        vec3 H = normalize(uKeyDir + V);
        float hs = pow(max(dot(N, H), 0.0), rough);
        c += uKeyCol * smoothstep(0.55, 0.62, hs) * spec * band;
    }
    for (int i = 0; i < 16; i++) {
        if (i >= uNL) break;
        vec3 L = uLP[i].xyz - P;
        float d2 = dot(L, L);
        float r = uLP[i].w;
        if (d2 > r * r) continue;
        float d = sqrt(d2);
        L /= d;
        float att = 1.0 / (1.0 + d2 / (r * r * 0.08)) * (1.0 - smoothstep(0.5 * r, r, d));
        att *= 0.2 + 0.8 * smoothstep(0.15, 0.8, L.y);
        float nl = max(dot(N, L), 0.0) * 0.85 + 0.15;
        float a = att * nl;
        float pool = smoothstep(0.06, 0.1, a) * 0.45 + smoothstep(0.3, 0.36, a) * 0.55;
        c += alb * uLC[i].rgb * uLC[i].a * mix(a, pool, 0.75) * aoK;
    }
    return c;
}
vec3 applyFog(vec3 c, vec3 P) {
    float dist = length(P - uEye);
    float f = 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD);
    return mix(c, uFogCol, clamp(f, 0.0, 1.0));
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec3 vP, vL, vLN, vN;
flat in vec3 vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat, vE;
flat in float vWave;
out vec4 frag;
uniform int uMesh;          // 0 box, 1 cylinder, 2 hemisphere
uniform float uBoard;       // 1: the well (no fog)
uniform float uStackA;      // the frame's alpha (1 on the stack, for the bridge)
uniform float uPremul;      // 1: a blended pass (the glass, a cleared row): premultiplied color and alpha
uniform vec3 uSkyA, uSkyB, uOutside, uLed, uPaper, uFloorB, uLineCol, uRimCol, uCloud, uCloudShade;
uniform float uLampK, uNight, uWorld, uShelfLight, uLevelH, uBeat, uLineW;
uniform float uLevelOn[8];
uniform float uDropT, uDropK, uDropSpan, uGroupN;

float levelOn(float k) { int i = int(clamp(k, 0.0, 7.0) + 0.5); return uLevelOn[i]; }

// A cell of the board as a bit of a packed shelf seen from the front: uv its front face (-1..1, y up), pn a pixel
// in uv, col its piece's color, seed its own look (a cell keeps its seed through falls and clears, so its books never
// change). Two layouts: three books standing upright (now and then four; widths a little uneven, now and then a
// shorter one leaving the shelf's dark back showing above it), or a pile of three lying on their sides. Each book
// takes its own shade of the piece's color and its spine's marks: a standing one a pale cap at its head with a dark
// title (or two thin pale bands), sometimes a small label lower down or a band at its foot; a lying one a long pale
// label (the pile's top book always), or bands near its ends, or nothing. Thin dark gaps between the books; flat toon
// shading, a lighter edge on each spine's lit side (left when standing, the top when lying).
vec3 shelfCell(vec2 uv, vec2 pn, vec3 col, float seed) {
    col = mix(vec3(dot(col, vec3(0.3, 0.55, 0.15))), col, 0.86);         // a touch softer: toon pastels
    vec3 back = col * vec3(0.16, 0.16, 0.22) + vec3(0.01, 0.01, 0.03);   // the shelf's back, in the books' shadow
    vec3 cream = vec3(1.0, 0.97, 0.9);
    float detail = 1.0 - smoothstep(0.09, 0.2, max(pn.x, pn.y));         // a small preview drops the fine marks
    bool lying = hash12(vec2(seed, 1.7)) < 0.4;
    // in the books' frame: a across the books (x standing, y lying: the pile from its foot up), b along them
    vec2 ab = lying ? uv.yx : uv;
    vec2 pab = lying ? pn.yx : pn;
    float gw = max(0.06, pab.x * 1.1);    // a gap between two books
    // three books, now and then four (standing), or a big one beside a slim one
    float hn = hash12(vec2(seed, 2.3));
    int n = lying ? 3 : hn < 0.18 ? 4 : hn < 0.3 ? 2 : 3;
    float w[4];
    float tot = 0.0;
    for (int i = 0; i < 4; i++) { w[i] = i < n ? 0.8 + 0.4 * hash12(vec2(seed, 3.0 + float(i))) : 0.0; tot += w[i]; }
    if (n == 2) { w[0] = hn < 0.24 ? 2.0 : 1.0; w[1] = 3.0 - w[0]; tot = 3.0; }
    // which book: its span across [a0, a1]
    int k = n - 1;
    float a0 = -1.0, a1 = 1.0, x0 = -1.0;
    for (int i = 0; i < 4; i++) {
        float x1 = i == n - 1 ? 1.0 : x0 + 2.0 * w[i] / tot;
        if (ab.x < x1 || i == n - 1) { k = i; a0 = x0; a1 = x1; break; }
        x0 = x1;
    }
    float r0 = hash12(vec2(seed * 7.13 + float(k), 11.0));
    float r1 = hash12(vec2(seed * 7.13 + float(k), 12.0));
    float r2 = hash12(vec2(seed * 7.13 + float(k), 13.0));
    float r3 = hash12(vec2(seed * 7.13 + float(k), 14.0));
    float sty = hash12(vec2(seed * 3.7 + float(k), 15.0));
    // its span along [b0, b1]: a standing one stands on the shelf (now and then a shorter one), a lying one may stop
    // a little short of one end (not the pile's foot book)
    float b0 = -1.0, b1 = 1.0;
    if (!lying) b1 = r3 < 0.16 ? 0.9 - 0.1 * r1 : 1.0;
    else if (k > 0 && r3 < 0.3) { if (r1 < 0.5) b0 += 0.08 + 0.08 * r2; else b1 -= 0.08 + 0.08 * r2; }
    // the gaps: only between books (the cell's own edges have the ink outline)
    float ea = min(a0 < -0.999 ? 9.0 : ab.x - a0, a1 > 0.999 ? 9.0 : a1 - ab.x);
    float eb = min(b0 < -0.999 ? 9.0 : ab.y - b0, b1 > 0.999 ? 9.0 : b1 - ab.y);
    float m = smoothstep(gw * 0.5 - pab.x * 0.6, gw * 0.5 + pab.x * 0.6, ea) * smoothstep(gw * 0.5 - pab.y * 0.6, gw * 0.5 + pab.y * 0.6, eb);
    // its own shade of the color: lighter or deeper, now and then a paler or a richer one
    vec3 bc = col * (0.88 + 0.2 * r0);
    bc = mix(bc, bc * bc * 1.2, step(0.8, r2) * 0.3);
    bc = mix(bc, vec3(1.0), step(r2, 0.18) * 0.18);
    bool big = n == 2 && a1 - a0 > 1.0;
    if (n == 2 && !big) bc = mix(bc, cream, 0.62);       // the slim one beside a big one: a pale binding
    vec3 pale = mix(bc, cream, 0.7);
    vec3 ink = bc * vec3(0.3, 0.3, 0.38);
    // in the book: s across it (0..1, from its lit side), dh / df how far from its head / foot (uv)
    float s = (ab.x - a0) / (a1 - a0);
    if (lying) s = 1.0 - s;
    bool headHi = !lying || r1 < 0.5;
    float dh = headHi ? b1 - ab.y : ab.y - b0, df = headHi ? ab.y - b0 : b1 - ab.y;
    float ps = pab.x / (a1 - a0), pb = pab.y;
    #define BAND(d, d0, d1) (smoothstep(d0 - pb, d0 + pb, d) * (1.0 - smoothstep(d1 - pb, d1 + pb, d)))
    #define ACROSS(s0, s1) (smoothstep(s0 - ps, s0 + ps, s) * (1.0 - smoothstep(s1 - ps, s1 + ps, s)))
    vec3 c = bc;
    if (big) {
        // a big book: a pale label high on its spine, a dark title on it
        c = mix(c, mix(bc, cream, 0.8), BAND(dh, 0.16, 0.42) * ACROSS(0.14, 0.86));
        c = mix(c, ink, BAND(dh, 0.25, 0.32) * ACROSS(0.24, 0.76) * 0.55 * detail);
    } else if (!lying) {
        if (sty < 0.78) {
            // a pale cap at its head, a dark title across it
            c = mix(c, pale, BAND(dh, 0.05, 0.29) * ACROSS(0.12, 0.88));
            c = mix(c, ink, (BAND(dh, 0.12, 0.17) + BAND(dh, 0.21, 0.25) * step(0.5, r2)) * ACROSS(0.26, 0.74) * 0.55 * detail);
        } else {
            // two thin pale bands at its head
            c = mix(c, pale, min(BAND(dh, 0.07, 0.13) + BAND(dh, 0.2, 0.26), 1.0) * mix(0.6, 0.9, detail));
        }
        // now and then a small label lower down, or a thin band at its foot
        if (r1 > 0.72) c = mix(c, mix(bc, cream, 0.55), BAND(dh, 1.0, 1.14) * ACROSS(0.24, 0.76) * detail);
        else if (r1 < 0.25) c = mix(c, pale, BAND(df, 0.1, 0.16) * ACROSS(0.0, 1.0) * mix(0.5, 0.85, detail));
    } else {
        float len = b1 - b0, mid = (b0 + b1) * 0.5;
        if (k == n - 1 || sty < 0.25) {
            // a long pale label along its spine (a margin of the binding at its ends), a dark title on it
            float lab = smoothstep(0.08 - pb, 0.08 + pb, len * 0.5 - abs(ab.y - mid));
            c = mix(c, pale, lab * ACROSS(0.13, 0.87));
            float tt = BAND(dh, 0.3, 0.3 + 0.5 * (0.6 + 0.4 * r2));
            c = mix(c, ink, tt * ACROSS(0.42, 0.6) * 0.5 * detail);
        } else if (sty < 0.6) {
            // thin pale bands near both ends
            float bands = BAND(dh, 0.08, 0.14) + BAND(dh, 0.2, 0.26) + BAND(df, 0.08, 0.14) + BAND(df, 0.2, 0.26);
            c = mix(c, pale, min(bands, 1.0) * mix(0.6, 0.9, detail));
        }
    }
    #undef BAND
    #undef ACROSS
    // flat toon shading: a lighter edge on its lit side, a darker one opposite
    c = mix(c, mix(c, vec3(1.0), 0.12), 1.0 - smoothstep(0.12 - ps, 0.12 + ps, s));
    c = mix(c, c * 0.88, smoothstep(0.88 - ps, 0.88 + ps, s));
    return mix(back, c, m);
}
)"
R"(
// A toon cumulus field at p (sky units).
float cloudAt(vec2 p) {
    float n = fbm(vec2(p.x * 0.035, p.y * 0.1) + 4.0) + 0.18 * fbm(p * 0.25 + 9.0);
    float band = smoothstep(6.0, 14.0, p.y) * (1.0 - smoothstep(30.0, 44.0, p.y));
    return n * band;
}
// The sky at p: a clear gradient, crisp puffy clouds (white bodies, lavender bellies), stars at night.
vec3 skyAt(vec2 p) {
    vec3 sky = mix(uSkyB, uSkyA, smoothstep(0.0, 36.0, p.y));
    float aa = fwidth(p.y) * 0.02 + 0.006;
    float c0 = cloudAt(p), c1 = cloudAt(p - vec2(0.0, 1.4));
    float body = smoothstep(0.64 - aa, 0.64 + aa, c0);
    float lit = smoothstep(0.64 - aa, 0.64 + aa, c1);
    vec3 cloud = mix(uCloudShade, uCloud, lit);
    sky = mix(sky, cloud, body * (1.0 - 0.75 * uNight));
    // stars (night): sparse points, a few brighter
    float h = hash12(floor(p * 0.9));
    float st = step(0.93, h) * (1.0 - smoothstep(0.05, 0.16, length(fract(p * 0.9) - 0.5)));
    sky += vec3(1.0, 0.95, 0.8) * st * uNight * smoothstep(4.0, 12.0, p.y) * (0.6 + 1.4 * step(0.985, h));
    return sky;
}

// The view out of the glazing at world height y, x along it: a bright sky over round toon trees (day), a town of
// blue houses with lit windows under the stars (night).
vec3 outside(float x, float y) {
    x *= 3.0;
    y = y * 3.0 - 1.0;
    vec3 c = skyAt(vec2(x, y));
    float aa = fwidth(y) * 1.5 + 0.02;
    // round crowns: two rows of discs of varying size
    vec2 ld = normalize(vec2(-0.5, 0.8));
    for (int row = 0; row < 2; row++) {
        float w = row == 0 ? 3.4 : 2.6;
        float off = row == 0 ? 0.0 : 1.3;
        float cx = floor((x + off) / w);
        for (int k = -1; k <= 1; k++) {
            float id = cx + float(k);
            float hr = hash12(vec2(id, 3.0 + float(row)));
            float r = (row == 0 ? 2.6 : 2.0) * (0.75 + 0.5 * hr);
            vec2 ctr = vec2((id + 0.5) * w - off + (hr - 0.5) * 1.2, (row == 0 ? 4.2 : 2.6) + 2.5 * hash12(vec2(id, 9.0 + float(row))));
            vec2 d = vec2(x, y) - ctr;
            float dist = length(d) - r;
            float m = 1.0 - smoothstep(-aa, aa, dist);
            float li = dot(d / r, ld);
            vec3 green = uOutside * (row == 0 ? 0.85 : 1.0);
            vec3 tc = mix(mix(green * 0.5, uSkyA * 0.35, 0.3), green * 0.95, smoothstep(-0.1, -0.02, li));
            tc = mix(tc, green * 1.2 + vec3(0.03, 0.04, 0.0), smoothstep(0.45, 0.52, li));
            tc = mix(tc, green * 0.3, smoothstep(-0.14, -0.05, dist) * m);
            c = mix(c, tc, m * (1.0 - uNight));
        }
    }
    // the town at night: house blocks in deep blue, warm windows
    float blk = floor(x / 3.1);
    float hB = 4.0 + 10.0 * pow(hash12(vec2(blk, 7.1)), 1.4);
    float mB = 1.0 - smoothstep(hB - aa, hB + aa, y);
    vec3 city = uOutside * (0.8 + 0.25 * hash12(vec2(blk, 2.0)));
    vec2 w = vec2(x / 0.55, y / 0.75);
    float lit = step(0.55, hash12(floor(w) + blk * 3.0)) * step(0.3, fract(w.x)) * step(0.3, fract(w.y));
    city += vec3(1.0, 0.75, 0.35) * lit * 0.9 * (1.0 - smoothstep(0.3, 1.0, fwidth(w.x)));
    c = mix(c, city, mB * uNight);
    // the lawn (the street at night) in front
    vec3 lawn = mix(uOutside * 1.05, uOutside * 0.6, uNight);
    c = mix(c, lawn, 1.0 - smoothstep(0.6 - aa, 0.6 + aa, y));
    return c;
}

void main() {
    int mat = int(vMat.x + 0.5);
    vec3 N = normalize(vN);
    if (!gl_FrontFacing && uMesh != 2) N = -N;
    if (uMesh == 2 && dot(N, uEye - vP) < 0.0) N = -N;
    vec2 uv = vec2(0.0), sz = vec2(1.0);
    vec3 T = vAX, B = vAY;
    int face = 0; // box: 0 x sides, 1 top / bottom, 2 front / back (z)
    if (uMesh == 0) {
        if (abs(vLN.x) > 0.5) { uv = vL.zy; sz = vHalf.zy; T = vAZ; B = vAY; face = 0; }
        else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; face = 1; }
        else { uv = vL.xy; sz = vHalf.xy; T = vAX; B = vAY; face = 2; }
    }
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.0, rough = 20.0, ao = 1.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    vec2 pq = vec2(fwidth(q.x), fwidth(q.y)) + 1e-6;   // a pixel on this face
    float px = max(pq.x, pq.y);
    float outA = 1.0;
    float lineK = 1.0;  // how much of the ink outline this surface gets
    vec3 lineC = vec3(-1.0);

    if (mat == 0) {
        // plaster: flat, clean color
        spec = 0.0;
    } else if (mat == 1) {
        // painted wood, chunky: a few broad soft streaks of grain along the longest side, a lighter edge band
        vec2 g = sz.x > sz.y ? q : q.yx;
        float detail = 1.0 - smoothstep(0.004, 0.02, px);
        float grain = fbm(vec2(g.x * 0.6, g.y * 9.0) + seed * 13.0);
        alb *= mix(1.0, 0.93 + 0.1 * smoothstep(0.45, 0.6, grain), detail);
        float edge = min(sz.x - abs(q.x), sz.y - abs(q.y));
        alb = mix(alb, alb * 1.18 + 0.03, (1.0 - smoothstep(0.006, 0.012 + px, edge)) * detail * 0.7);
        spec = 0.18; rough = 24.0;
    } else if (mat == 2) {
        // the shelving's back panel: a strong flat color, washed by the strip light under each shelf
        float yb = q.y + sz.y;
        float cy = (yb - vMat.z) / vMat.y;
        float fr = fract(cy);
        float inside = step(0.0, cy) * step(cy, 8.0);
        float on = levelOn(vE.z);
        float wash = smoothstep(0.55, 0.62, fr) * inside;
        ao = mix(1.0, 0.8 + 0.2 * step(0.3, fr), inside);
        emit += uLed * wash * 0.16 * uShelfLight * on * alb / max(dot(alb, vec3(0.333)), 0.05);
        lineK = 0.0;
    } else if (mat == 3) {
        // a book: the spine toward +z, pages at the ends and at -z, covers on the x sides. p0: its spine's design,
        // p1: its glow (shelved, the drop), p2: a cleared book's fade into light (0 none .. 1 gone)
        vec3 L = vL;
        vec3 h = vHalf;
        int sty = int(vMat.y + 0.5);
        vec3 ink = vec3(0.02, 0.025, 0.07);
        vec3 gold = vec3(1.0, 0.75, 0.2);
        float rim = 0.08 * min(h.x * 2.0, h.z * 2.0);
        float detail = 1.0 - smoothstep(0.25, 0.7, px / max(min(h.x, h.y), 1e-4) * 2.0);
        lineK = 0.7;
        if (face == 2 && vLN.z > 0.0) {
            float u = L.x, v = L.y;   // u across the thickness, v along the length
            float pu = fwidth(u) + 1e-4, pv = fwidth(v) + 1e-4;
            float r = hash12(vec2(seed * 3.1, 9.0));
            #define RECT(v0, v1, uw) (smoothstep(v0 - pv, v0 + pv, v) * (1.0 - smoothstep(v1 - pv, v1 + pv, v)) * (1.0 - smoothstep(uw - pu, uw + pu, abs(u))))
            // toy-bright designs: cream or gold bands, a dark title, a dot
            vec3 light = mix(alb, vec3(1.0, 0.94, 0.82), 0.6);
            vec3 band = r > 0.8 ? mix(alb, gold, 0.7) : light;
            if (sty == 0) {
                alb = mix(alb, band, RECT(0.55, 0.7, 2.0) * detail);
                alb = mix(alb, band, RECT(-0.7, -0.55, 2.0) * detail);
            } else if (sty == 1) {
                alb = mix(alb, light, RECT(0.05, 0.5, 0.55) * detail);
                alb = mix(alb, ink, RECT(0.18, 0.18 + 0.22 * (0.5 + 0.5 * r), 0.25) * detail * 0.8);
            } else if (sty == 2) {
                float lo = smoothstep(-0.25 - pv, -0.25 + pv, v);
                alb = mix(alb * 0.6, alb, lo);
                alb = mix(alb, gold, RECT(-0.36, -0.28, 2.0) * detail);
            } else {
                float dt = 1.0 - smoothstep(0.42 - pu, 0.42 + pu, length(vec2(u, (v - 0.35) * h.y / max(h.x, 1e-3))));
                alb = mix(alb, light, dt * detail);
                alb = mix(alb, band, RECT(-0.72, -0.62, 2.0) * detail);
            }
            spec = 0.15; rough = 18.0;
            emit += alb * vMat.z * 1.2 + uLed * vMat.z * 0.25;
        } else if (face == 1 || (face == 2 && vLN.z < 0.0)) {
            // the page block (head, tail, fore-edge), the boards' edges around it
            float ax = h.x - abs(L.x * h.x);
            float covers = 1.0 - smoothstep(rim * 0.8, rim, ax);
            float spineSide = face == 1 ? step(h.z - rim * 1.2, L.z * h.z) : 0.0;
            float pages = (1.0 - covers) * (1.0 - spineSide);
            alb = mix(alb, uPaper, pages);
            emit += uPaper * vMat.z * 0.4 * pages;
        } else {
            alb *= 0.92;
            emit += alb * vMat.z * 0.6;
        }
        if (uWorld > 0.5) {
            float up = dot(vAX, vec3(0, 1, 0)) * L.x + dot(vAY, vec3(0, 1, 0)) * L.y;
            emit += alb * uLed * uShelfLight * levelOn(vE.z) * (0.04 + 0.12 * step(0.2, up));
            emit += (alb * 0.3 + uLed * 0.12) * vWave;
            float d = (vE.w + 0.5) / uGroupN - uDropT;
            emit += (alb * 1.6 + uLed * 0.3) * uDropK * exp(-d * d / (uDropSpan * uDropSpan));
        }
        if (vMat.w > 0.0) {
            float f = vMat.w;
            emit += (vCol.rgb * 1.4 + 0.25) * (2.2 * f);
            outA = (1.0 - f) * (1.0 - f * 0.5);
        }
    } else if (mat == 12) {
        // a cell of the board: a bit of a packed shelf, its books' spines toward us (shelfCell); the books' tops
        // above, a deep shade on its other sides; inked edges
        lineK = 1.0;
        lineC = vCol.rgb * vec3(0.16, 0.16, 0.22) + vec3(0.01, 0.01, 0.03);
        if (face == 2 && vLN.z > 0.0) {
            alb = shelfCell(q / sz, pq / sz, vCol.rgb, seed);
            spec = 0.0;
        } else {
            // its top and sides: the shelf in the books' shadow (seen through the gaps, they read as the dark lines
            // between the cells)
            alb = lineC * (face == 1 && vLN.y > 0.0 ? 0.9 : 0.7);
            lineK = 0.0;
        }
        if (vMat.w > 0.0) {
            float f = vMat.w;
            emit += (vCol.rgb * 1.3 + 0.25) * (2.0 * f);
            outA = (1.0 - f) * (1.0 - f * 0.5);
            lineK *= 1.0 - f;
        }
    } else if (mat == 4) {
        // floor: big toy tiles in two clear colors (p0 = 1: long planks, else a checker), crisp grout
        vec2 g = vP.xz;
        float aa = px * 1.5 + 0.004;
        if (vMat.y > 0.5 && vMat.y < 1.5) {
            float w = 0.32;
            float row = floor(g.x / w);
            float off = hash12(vec2(row, 3.0)) * 2.4;
            float col = floor((g.y + off) / 2.4);
            float id = hash12(vec2(row, col));
            vec2 pl = vec2(fract(g.x / w), fract((g.y + off) / 2.4));
)"
R"(            float seam = smoothstep(0.0, 0.04 + px / w, min(pl.x, 1.0 - pl.x)) * smoothstep(0.0, 0.006 + px / 2.4, min(pl.y, 1.0 - pl.y));
            alb = mix(alb, uFloorB, step(0.62, id) * 0.6);
            alb *= 0.65 + 0.35 * seam;
        } else {
            vec2 t = g / 1.2;
            vec2 f = fract(t);
            float chk = mod(floor(t.x) + floor(t.y), 2.0);
            alb = mix(alb, uFloorB, chk);
            vec2 e = min(f, 1.0 - f) * 1.2;
            float grout = 1.0 - smoothstep(0.015, 0.015 + aa, min(e.x, e.y));
            alb = mix(alb, alb * 0.72, grout);
        }
        spec = 0.25; rough = 40.0;
        lineK = 0.0;
    } else if (mat == 5) {
        // painted concrete: flat
        spec = 0.0;
    } else if (mat == 6) {
        // painted metal
        spec = 0.4; rough = 30.0;
    } else if (mat == 7) {
        // a light: p0 how bright, p1 = 1: follows the lamps' level; its floor's lights must be on
        float k = vMat.y * (vMat.z > 0.5 ? uLampK : 1.0) * mix(0.25, 1.0, levelOn(vE.z));
        if (vHalf.y < 0.005 && vE.z >= 0.0) k *= 1.0 + 2.2 * shelfWave(dot(vP, vAX), vP.y + 0.05, vE.z);
        emit = alb * k;
        alb = vec3(0.0);
        lineK = 0.0;
    } else if (mat == 8) {
        // glazing (its +z face): the view out between chunky painted mullions. p0: 1 the curtain wall, 0 the slot
        if (face == 2 && vLN.z > 0.0) {
            float x = dot(vP, vAX);
            float y = vP.y;
            float mw = vMat.y > 0.5 ? 1.5 : 0.9;
            float fx = abs(fract(x / mw) - 0.5) * mw;
            float fy = abs(fract(y / (uLevelH * 0.5)) - 0.5) * uLevelH * 0.5;
            float mull = 1.0 - smoothstep(0.05, 0.05 + px * 1.5, mw * 0.5 - fx);
            float tr = 1.0 - smoothstep(0.045, 0.045 + px * 1.5, uLevelH * 0.25 - fy);
            float frameM = max(mull, tr);
            vec3 o = outside(x + seed * 40.0, y);
            emit = o * (1.0 - frameM);
            alb = alb * frameM;
            spec = 0.0;
            lineK = 0.0;
        } else alb *= 0.6;
    } else if (mat == 9) {
        // glass (blended): a faint sky tint, a bright edge
        vec3 V = normalize(uEye - vP);
        float fr = pow(1.0 - abs(dot(N, V)), 4.0);
        vec2 e = sz - abs(q);
        float edge = 1.0 - smoothstep(0.0, 0.01 + px, min(e.x, e.y));
        vec3 c = mix(uSkyA * 0.25, mix(uSkyB, uSkyA, 0.5) * 0.5, fr) + vec3(0.8) * edge * 0.3;
        outA = 0.08 + 0.14 * fr + 0.3 * edge;
        frag = vec4(applyFog(c, vP) * outA, outA);
        return;
    } else if (mat == 10) {
        // the skylight: a grid of chunky beams over the sky (its underside)
        lineK = 0.0;
        if (face == 1 && vLN.y < 0.0) {
            vec2 g = vP.xz;
            vec2 f = abs(fract(g / 2.4 + 0.5) - 0.5) * 2.4;
            float beam = 1.0 - smoothstep(0.14, 0.14 + px * 1.5, min(f.x, f.y));
            vec3 sky = skyAt(vec2(g.x * 2.0, 20.0 + g.y * 1.2)) * 1.1;
            emit = sky * (1.0 - beam);
            alb *= beam;
        }
    } else if (mat == 13) {
        // the backdrop the diorama sits against: the variant's sky, calm (soft clouds by day, stars by night)
        lineK = 0.0;
        float hy = smoothstep(-14.0, 30.0, vP.y);
        emit = mix(uSkyB, uSkyA, hy * hy) * 0.95;
        vec2 sp = vP.xy * 1.6;
        float h = hash12(floor(sp));
        float st = step(0.95, h) * (1.0 - smoothstep(0.04, 0.14, length(fract(sp) - 0.5)));
        emit += vec3(1.0, 0.95, 0.8) * st * uNight * (0.5 + 1.2 * step(0.99, h));
        alb = vec3(0.0);
    } else if (mat == 14) {
        // a round window (a disc facing the room, its axis Y): the sky through a cross of glazing bars, a frame
        if (uMesh == 1 && vLN.y > 0.5) {
            vec2 d = vL.xz;
            float r = length(d);
            float pw = fwidth(r) * 1.2 + 0.004;
            float frameM = smoothstep(0.82 - pw, 0.82 + pw, r);
            float bar = 1.0 - smoothstep(0.05 - pw, 0.05 + pw, min(abs(d.x), abs(d.y)));
            frameM = max(frameM, bar);
            vec3 sky = skyAt(vec2(d.x * 9.0 + seed * 30.0, 15.0 + d.y * 9.0));
            emit = sky * (1.0 - frameM);
            alb *= frameM;
        }
    } else if (mat == 11) {
        // a round, friendly shape (a plant's foliage, a pot, a lamp's globe): flat color, toon lit; p0 > 0: a
        // globe glowing from within
        spec = 0.2; rough = 16.0;
        if (vMat.y > 0.0) {
            emit = alb * vMat.y * mix(0.3, 1.0, levelOn(vE.z)) * uLampK;
            alb *= 0.35;
            lineK = 0.0;
        }
    }

    vec3 Nb = normalize(N + T * bend.x + B * bend.y);
    vec3 c = shade(vP, Nb, alb, spec, rough, ao) + emit;
    vec3 V = normalize(uEye - vP);
    // a warm rim along the silhouettes of round shapes
    if (uMesh != 0) {
        float fr = 1.0 - abs(dot(N, V));
        c += uRimCol * alb * smoothstep(0.6, 0.68, fr) * lineK;
    }
    // ink: a soft dark outline along every box's edges (fading on faces too small to carry it), a dark rim round the
    // silhouettes of round shapes
    if (lineK > 0.0) {
        float ol = 0.0;
        if (uMesh == 0) {
            vec2 pw = vec2(fwidth(q.x), fwidth(q.y)) + 1e-6;
            vec2 e = (sz - abs(q)) / pw;
            float ed = min(e.x, e.y);
            ol = 1.0 - smoothstep(uLineW * 0.5, uLineW * 0.5 + 1.0, ed);
            ol *= smoothstep(1.5, 5.0, min(sz.x / pw.x, sz.y / pw.y));
        } else {
            ol = 1.0 - smoothstep(0.14, 0.24, abs(dot(N, V)));
        }
        vec3 lc = lineC.x >= 0.0 ? lineC : uLineCol * (0.3 + 0.7 * alb);
        c = mix(c, lc, ol * lineK * (uWorld > 0.5 ? 0.6 : 0.85));
    }
    if (uBoard < 0.5) c = applyFog(c, vP);
    if (uPremul > 0.5) frag = vec4(c * outA, outA);
    else frag = vec4(c, uStackA);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vP;
flat in vec4 vMat;
out vec4 frag;
void main() {
    // the skylight's glazing and the lights let the sun through
    int mat = int(vMat.x + 0.5);
    if (mat == 10 || mat == 7) discard;
    frag = vec4(1.0);
}
)";

// ---------------------------------------------------------------- glows (billboards): lamps, motes, sparks
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // position, radius
layout(location = 2) in vec4 iC;   // color, intensity
uniform mat4 uVP;
uniform vec3 uOrigin, uCamR, uCamU;
uniform float uMinR;
out vec2 vC;
out vec4 vCol;
out vec3 vP;
void main() {
    vC = aCorner;
    vCol = iC;
    vec3 c = iP.xyz;
    float d = length(c - uEye);
    float r = max(iP.w, d * uMinR);
    // a sprite smaller than its floor fades rather than shrinking below a pixel
    vCol.a *= min(1.0, (iP.w / r) * (iP.w / r) * 4.0);
    vec3 P = c + (uCamR * aCorner.x + uCamU * aCorner.y) * r;
    vP = c;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
in vec3 vP;
uniform float uFogD, uFogStart;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 40.0);
    float halo = exp(-r2 * 6.0) * 0.35 + exp(-r2 * 2.0) * 0.1;
    float a = (core * 1.4 + halo) * (1.0 - r2);
    vec3 c = vCol.rgb * vCol.a * a;
    float dist = length(vP - uEye);
    c *= mix(1.0, 0.25, 1.0 - exp(-max(dist - uFogStart, 0.0) * uFogD));
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- lines (screen-space): the ghost
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // x: 0 start .. 1 end, y: -1 .. 1 across
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uRes;
out float vAcross;
out vec4 vCol;
out float vW;
void main() {
    vec4 c0 = uVP * vec4(iA.xyz, 1.0), c1 = uVP * vec4(iB.xyz, 1.0);
    vec2 s0 = c0.xy / c0.w * uRes * 0.5, s1 = c1.xy / c1.w * uRes * 0.5;
    vec2 dir = s1 - s0;
    dir = length(dir) > 1e-4 ? normalize(dir) : vec2(0.0, 1.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec4 c = mix(c0, c1, aCorner.x);
    float half_ = max(iA.w, 1.0) * 0.5 + 0.75;
    c.xy += nrm * aCorner.y * half_ / (uRes * 0.5) * c.w;
    vAcross = aCorner.y * half_;
    vW = iA.w;
    vCol = vec4(iC.rgb, iB.w);
    gl_Position = c;
}
)";
inline const char* LINE_FS = R"(#version 330 core
in float vAcross;
in vec4 vCol;
in float vW;
out vec4 frag;
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    frag = vec4(vCol.rgb * a, a);
}
)";

// ---------------------------------------------------------------- the shade behind the well and the HUD's columns
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil, uVeilTop, uCard, uCardEdge;
uniform float uWellA, uColA, uPx;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h, float r) { vec2 q = abs(p - c) - h + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float aa = uPx;
    // the well's ground: one deep, saturated color (a touch lighter at its top), clean edged under the frame
    float dw = box(w, vec2(0.0, 0.0), vec2(5.05, 10.55), 0.2);
    float a = uWellA * (1.0 - smoothstep(-aa, aa, dw));
    vec3 col = mix(uVeil, uVeilTop, smoothstep(-10.0, 10.0, w.y)) * a;
    // the HUD's columns: rounded cards in a clear light color with a colored edge
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5, 0.7);
        // see-through (the room shows through), its edge opaque
        float cov = 1.0 - smoothstep(-aa, aa, dc);
        float inner = 1.0 - smoothstep(-aa, aa, dc + 0.12);
        float m = cov * mix(1.0, uColA, inner) * step(0.001, uColA);
        vec3 cc = mix(uCardEdge, uCard, inner);
        col = col * (1.0 - m) + cc * m;
        a = a + m * (1.0 - a);
    }
    frag = vec4(col, a);
}
)";

// ---------------------------------------------------------------- the glow and the frame
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThr;
void main() {
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb
           + texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    float l = max(max(c.r, c.g), c.b);
    frag = vec4(c * max(l - uThr, 0.0) / max(l, 1e-4), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    vec4 c = texture(uSrc, vUv) * w[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)) + texture(uSrc, vUv - uDir * float(i))) * w[i];
    frag = c;
}
)";
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF, uVig;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint, uLift;
uniform vec4 uCardL, uCardR;           // the HUD's cards (pixels, GL origin)
uniform vec3 uCardCol, uCardEdgeC;
uniform float uCardA, uCardRad, uCardEdgeW;
float rbox(vec2 p, vec4 r, float rad) {
    vec2 c = (r.xy + r.zw) * 0.5, h = (r.zw - r.xy) * 0.5;
    vec2 q = abs(p - c) - h + rad;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - rad;
}
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 b = texture(uBloom, vUv).rgb;
    vec3 c = s.rgb * uExposure + b * uBloomK;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    c = mix(c, mix(vec3(l), c, 0.6) * uSlowTint, uSlow);
    // linear up to 0.8, then a soft shoulder: the toon colors land as painted, highlights don't burn
    vec3 hi = 0.8 + 0.2 * (1.0 - exp(-(c - 0.8) / 0.2));
    c = mix(c, hi, step(0.8, c));
    c += uLift * (1.0 - c);
    vec2 v = vUv - 0.5;
    c *= 1.0 - uVig * dot(v, v) * 2.0;
    if (uCardA > 0.0) {
        for (int i = 0; i < 2; i++) {
            float d = rbox(gl_FragCoord.xy, i == 0 ? uCardL : uCardR, uCardRad);
            float m = 1.0 - smoothstep(-0.75, 0.75, d);
            float inner = 1.0 - smoothstep(-0.75, 0.75, d + uCardEdgeW);
            // a soft shadow round it
            float sh = (1.0 - smoothstep(0.0, uCardRad * 0.6, d)) * (1.0 - m) * 0.18;
            c *= 1.0 - sh * uCardA;
            c = mix(c, mix(uCardEdgeC, uCardCol, inner), m * mix(1.0, uCardA, inner) * min(1.0, uCardA * 3.0));
        }
    }
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
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

// Inserts the common functions (and the lighting) after the version line.
inline std::string withCommon(const char* src, bool lighting = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (lighting ? LIGHTING : "") + s.substr(nl + 1);
}

} // namespace library_shaders

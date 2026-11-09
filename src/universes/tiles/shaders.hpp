#pragma once
// TILES's shaders (see tiles.cpp).
//
// GB_* lays the tesserae once into a G-buffer (albedo and material, the normal of each tessera leaning in the mortar
// with its edges rounded, its id; a first dilated pass gives the grout around each tessera that tessera's id, so the
// grout of a figure not yet laid shows the bare bed too). COMP_FS lights the wall every frame from it: the moving
// raking light, the gold and silver smalti glinting as their tilt catches it, the glass with its sharp highlights, the
// matte stone; tesserae not yet set show the setting bed with the painter's red ochre sketch; it also paints the well's
// mortar bed. CELL_* draws the glazed tiles (the pieces), FRAG_* the fragments of a cleared row flying to their place,
// SPRITE_* the glints and the dust; DOWN/BLUR make the bloom, FINAL_FS encodes the frame.
#include <string>

namespace tiles_shaders {

inline const char* COMMON = R"(
uniform vec4 uView;   // center, half extents (board units)
float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + vec2(17.3, 5.1))); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return vnoise(p) * 0.55 + vnoise(p * 2.1 + 3.7) * 0.28 + vnoise(p * 4.3 + 9.1) * 0.17; }
vec2 worldAt(vec2 uv) { return uView.xy + (uv * 2.0 - 1.0) * uView.zw; }
float box(vec2 w, vec4 r, float soft) {
    return smoothstep(r.x - soft, r.x, w.x) * smoothstep(r.z + soft, r.z, w.x) * smoothstep(r.y - soft, r.y, w.y) * smoothstep(r.w + soft, r.w, w.y);
}
// The edges of a cell pattern (a crackle): small near a crack.
float crackle(vec2 p) {
    vec2 ci = floor(p), cf = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 o = vec2(float(x), float(y)) + hash2(ci + vec2(float(x), float(y))) - cf;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return sqrt(d2) - sqrt(d1);
}
// The raking light: a lamp at uLight.xyz (board units, z toward the eye), its color and reach.
uniform vec4 uLight;    // x, y, z, reach
uniform vec3 uLightCol; // linear
uniform vec3 uAmb;      // the room's light
vec3 lightDir(vec2 w, out float atten) {
    vec3 d = vec3(uLight.xy - w, uLight.z);
    float l = length(d);
    atten = 1.0 / (1.0 + dot(d.xy, d.xy) / (uLight.w * uLight.w));
    return d / l;
}
)";

// ---------------------------------------------------------------- the G-buffer: the tesserae, laid once
inline const char* GB_VS = R"(#version 330 core
layout(location = 1) in vec4 aC01;  // corners: start-, end-
layout(location = 2) in vec4 aC23;  // end+, start+
layout(location = 3) in vec4 aCol;  // rgb (linear), material
layout(location = 4) in vec4 aMisc; // tilt x, y, random, id
uniform mat4 uVP;
uniform float uDilate;
out vec2 vUv;
out vec2 vW;
flat out vec4 vCol;
flat out vec4 vMisc;
flat out vec2 vDim;
void main() {
    vec2 c0 = aC01.xy, c1 = aC01.zw, c2 = aC23.xy, c3 = aC23.zw;
    int i = gl_VertexID;
    vec2 p = i == 0 ? c0 : (i == 1 ? c1 : (i == 2 ? c3 : c2));
    vUv = vec2((i == 1 || i == 3) ? 1.0 : 0.0, i >= 2 ? 1.0 : 0.0);
    vec2 m = (c0 + c1 + c2 + c3) * 0.25;
    if (uDilate > 0.0) p += normalize(p - m) * uDilate;
    vDim = vec2(0.5 * (length(c1 - c0) + length(c2 - c3)), 0.5 * (length(c3 - c0) + length(c2 - c1)));
    vCol = aCol;
    vMisc = aMisc;
    vW = p;
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
inline const char* GB_FS = R"(#version 330 core
in vec2 vUv;
in vec2 vW;
flat in vec4 vCol;
flat in vec4 vMisc;
flat in vec2 vDim;
layout(location = 0) out vec4 o0;
layout(location = 1) out vec4 o1;
layout(location = 2) out vec4 o2;
uniform int uGrout;
uniform float uPx;    // board units per pixel
uniform float uBevel; // the rounded edge's width (board units)
void main() {
    int id = int(vMisc.w + 0.5);
    vec4 idc = vec4(float(id & 255) / 255.0, float((id >> 8) & 255) / 255.0, 0.0, 1.0);
    if (uGrout == 1) {
        o0 = vec4(0.0);
        o1 = vec4(0.5, 0.5, 0.0, vMisc.z);
        o2 = idc;
        return;
    }
    float e = min(min(vUv.x, 1.0 - vUv.x) * vDim.x, min(vUv.y, 1.0 - vUv.y) * vDim.y);
    float hgt = smoothstep(0.0, uBevel, e);
    hgt = sqrt(hgt);
    vec2 gw = vec2(dFdx(hgt), dFdy(hgt)) / uPx;
    int mat = int(vCol.a + 0.5);
    vec2 n = vMisc.xy - gw * uBevel * 0.55;
    // the face of a glass smalto is a fracture (uneven); gold leaf lies flat under its glass; stone is honed
    vec2 q = vW * 9.0 + vMisc.z * 31.0;
    vec2 rough = vec2(vnoise(q) - 0.5, vnoise(q + 7.3) - 0.5);
    n += rough * (mat == 1 ? 0.16 : (mat == 0 ? 0.06 : 0.05));
    o0 = vec4(sqrt(clamp(vCol.rgb, 0.0, 1.0)), (float(mat) + 1.0) / 5.0);
    o1 = vec4(clamp(n * 0.5 + 0.5, 0.0, 1.0), hgt, vMisc.z);
    o2 = vec4(idc.rg, 1.0, 1.0);
}
)";

// ---------------------------------------------------------------- the wall, lit
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uGb0, uGb1, uGb2, uSketch;
uniform sampler2D uState;   // per tessera: set time, glint time, unset time
uniform vec4 uSketchBox;    // the sketch raster's lo, hi
uniform float uTime;
uniform vec3 uGrout, uBed, uSinopia, uWell;
uniform float uBare;        // a break: no well (its place laid with the ground)
uniform vec4 uHudL, uHudR, uQuietA, uQuietB, uQuietC;
uniform float uDrop;        // the drop: the gold ablaze (0..1)
uniform float uIgnite;      // its front's radius from the well
uniform vec4 uWave;         // a ripple lifting the tesserae: radius, amplitude, width, 0
uniform vec4 uRingGlint;    // a Tetris's ring: center x, y, angle, strength
uniform vec2 uRingR;        // its inner and outer radius
uniform float uSparkle;     // how much the smalti sparkle (the section)
uniform float uGlintK;
// ---- The drop's show: the wall turns its tesserae over in waves, like a stadium's cards, each one showing on its
// back a tessera of a new design laid over the whole ground (the medallions keep theirs, their rings chasing).
uniform sampler2D uGeo;     // per tessera: center x, y, the direction across its row (angle), half its width
uniform float uShow;        // the show is on
uniform float uBeat;        // the song's beat
uniform vec4 uWaveA;        // the last wave: its beat, its sweep, the design it turns from, the one it turns to
uniform vec4 uWaveB;        // the one before
uniform vec2 uWaveSpin;     // 1: a full turn (the gold back shows in the middle of it) (A, B)
uniform vec3 uPal[4];       // the designs' colors: the dark, the metal, two accents (linear)
uniform vec2 uPalMat;       // the metal's material, the others'
uniform vec4 uMed;          // the medallions' centers
uniform vec3 uMedR;         // their radius, their ring's inner radius, they exist
uniform float uShowK;       // its strength (eased in and out)
const float FLIP = 1.5;     // a turn lasts this many beats
// When a sweep reaches a tessera at c (beats after the wave).
float sweepDelay(float kind, vec2 c, float rnd) {
    float r = length(c * vec2(0.8, 1.0));
    float d;
    if (kind < 0.5) d = r / 19.0;                                                     // out from the well
    else if (kind < 1.5) d = (c.x + c.y * 1.4 + 40.0) / 80.0;                         // a diagonal, up and right
    else if (kind < 2.5) d = fract(atan(c.y, c.x) / 6.2831853 + 0.5 + r * 0.025);     // a spiral around the well
    else if (kind < 3.5) d = 1.0 - abs(c.x) / 23.0;                                   // in from both sides
    else if (kind < 4.5) d = (uMedR.z > 0.5 ? min(length(c - uMed.xy), length(c - uMed.zw)) : r) / 19.0; // out of the medallions
    else if (kind < 5.5) d = (40.0 - c.x - c.y * 1.4) / 80.0;                         // a diagonal, down and left
    else if (kind < 6.5) d = (c.y + 13.0) / 26.0;                                     // up from the floor
    else d = rnd;                                                                     // here and there
    return clamp(d, 0.0, 1.0) * 1.2 + rnd * 0.08;
}
// The design a tessera at c shows on its back: an index into uPal (0 the dark, 1 the metal, 2 and 3 the accents).
int design(float pat, vec2 c, float rnd) {
    float st = uBeat * 0.25;               // a slow, continuous drift (steps on the eighth notes flickered)
    vec2 q = c * vec2(0.8, 1.0);
    float r = length(q), a = atan(c.y, c.x) / 6.2831853;
    if (pat > 98.5) return 1;
    if (pat < 1.5) {
        // a sunburst out of the well, its rings turning against each other
        float band = floor(r / 2.4);
        float dir = mod(band, 2.0) < 0.5 ? 1.0 : -1.0;
        float ray = floor(a * 18.0 + dir * st);
        int k = int(mod(ray + band, 3.0));
        return k == 0 ? 0 : (k == 1 ? 1 : (mod(band, 2.0) < 0.5 ? 2 : 3));
    }
    if (pat < 2.5) {
        // frames out of the well, marching outward
        vec2 e = abs(c) - vec2(4.0, 9.0);
        float d = length(max(e, 0.0)) + min(max(e.x, e.y), 0.0);
        float band = floor(d / 1.1 - st);
        int k = int(mod(band, 4.0));
        return k == 0 ? 1 : (k == 1 ? 0 : (k == 2 ? 2 : 0));
    }
    if (pat < 3.5) {
        // a spiral of arms around the well, turning
        float s = a * 4.0 + log(r + 1.0) * 1.8 - st * 0.5;
        float m = mod(floor(s * 2.0), 4.0);
        return m < 0.5 ? 1 : (m < 1.5 ? 2 : (m < 2.5 ? 0 : 3));
    }
    if (pat < 4.5) {
        // a lattice of lozenges, a diagonal running through it
        float u = (c.x + c.y) / 2.2, v = (c.x - c.y) / 2.2;
        vec2 f = abs(fract(vec2(u, v)) - 0.5);
        bool edge = max(f.x, f.y) > 0.36;
        float run = mod(floor(u) - floor(st * 2.0), 6.0);
        return edge ? 1 : (run < 0.5 ? 2 : (mod(floor(u) + floor(v), 2.0) < 0.5 ? 0 : 3));
    }
    // checkered waves rising from the floor
    float row = floor((c.y + 13.0) / 1.3);
    float wv = sin(c.x * 0.35 + row * 0.5 - st * 1.5);
    float m = mod(row, 3.0);
    return wv > 0.55 ? 1 : (wv > -0.2 ? (m < 0.5 ? 2 : (m < 1.5 ? 3 : 0)) : 0);
}
float tessState(int id, out float glintT, out float unsetT) {
    vec4 s = texelFetch(uState, ivec2(id % 256, id / 256), 0);
    glintT = s.y;
    unsetT = s.z;
    return s.x;
}
void main() {
    vec2 w = worldAt(vUv);
    vec4 g0 = texture(uGb0, vUv), g1 = texture(uGb1, vUv), g2 = texture(uGb2, vUv);
    int id = int(g2.r * 255.0 + 0.5) + 256 * int(g2.g * 255.0 + 0.5);
    if (id == 0) {
        // a gap no tessera's grout reached: the nearest one's
        vec2 px = 1.0 / vec2(textureSize(uGb2, 0));
        for (int k = 0; k < 8 && id == 0; k++) {
            float an = float(k) * 0.785398;
            vec4 gg = texture(uGb2, vUv + vec2(cos(an), sin(an)) * px * 2.5);
            id = int(gg.r * 255.0 + 0.5) + 256 * int(gg.g * 255.0 + 0.5);
        }
    }
    float att;
    vec3 L = lightDir(w, att);
    // quiet behind the HUD and the well (darker, calmer)
    float quiet = max(box(w, uHudL, 1.2), box(w, uHudR, 1.2));
    quiet = max(quiet, max(max(box(w, uQuietA, 1.0), box(w, uQuietB, 1.0)), box(w, uQuietC, 1.0)) * 0.85);
    float nearWell = box(w, vec4(-6.2, -11.2, 6.2, 11.4), 1.4);
    quiet = max(quiet, nearWell * 0.55);

    // ---- the well: a bed of dark mortar, recessed
    if (abs(w.x) < 5.06 && w.y > -10.06 && w.y < 10.06 + 3.0 && id == 0 && uBare < 0.5) {
        float grain = vnoise(w * 23.0) * 0.5 + vnoise(w * 61.0) * 0.5;
        float trowel = vnoise(vec2(w.x * 0.9 + sin(w.y * 0.7) * 0.6, w.y * 0.25));
        vec3 c = uWell * (0.85 + 0.18 * grain + 0.12 * trowel);
        float ex = 5.06 - abs(w.x), ey = min(w.y + 10.06, 13.06 - w.y);
        float edge = min(ex, ey);
        float shade = mix(0.55, 1.0, smoothstep(0.0, 0.45, edge));
        shade *= mix(0.75, 1.0, smoothstep(0.0, 0.9, 5.06 + w.x)) * mix(0.8, 1.0, smoothstep(0.0, 0.9, 10.06 - w.y));
        c *= shade * (uAmb * 2.2 + uLightCol * att * 0.18);
        frag = vec4(c, 0.0);
        return;
    }

    // ---- the drop's show: the tessera turning over about its row's line (squeezed across it), the bed dark in its
    // shadow; past halfway its back shows, a tessera of the next design
    float showPat = 0.0, turnS = 0.0, turnC = 1.0;
    vec2 turnDir = vec2(0.0);
    if (uShow > 0.5 && id > 0) {
        vec4 ge = texelFetch(uGeo, ivec2(id % 256, id / 256), 0);
        vec2 ctr = ge.xy;
        float rnd0 = hash(ctr * 7.31);
        bool inMed = uMedR.z > 0.5 && min(length(ctr - uMed.xy), length(ctr - uMed.zw)) < uMedR.x + 0.05;
        if (!inMed) {
            float dA = sweepDelay(uWaveA.y, ctr, rnd0);
            bool useA = uBeat >= uWaveA.x + dA;
            vec4 wv = useA ? uWaveA : uWaveB;
            float spin = useA ? uWaveSpin.x : uWaveSpin.y;
            float dl = useA ? dA : sweepDelay(uWaveB.y, ctr, rnd0);
            float p = clamp((uBeat - wv.x - dl) / FLIP, 0.0, 1.0);
            // a smooth ease in and out (no overshoot: a slap on every tessera was hard on the eyes)
            float e = p * p * (3.0 - 2.0 * p);
            float th = 3.14159265 * (spin > 0.5 ? 2.0 : 1.0) * e;
            turnC = cos(th);
            turnS = sin(th);
            showPat = turnC >= 0.0 ? wv.z : (spin > 0.5 ? 99.0 : wv.w);
            if (spin > 0.5 && turnC >= 0.0) showPat = wv.w;
            turnDir = vec2(cos(ge.z), sin(ge.z));
            vec2 dw = w - ctr;
            float across = dot(dw, turnDir);
            float half_ = ge.w * abs(turnC);
            if (abs(across) > ge.w) {
                // the grout around it
            } else if (abs(across) > half_) {
                // the bed under a tessera standing on its edge: in its shadow
                float sh = smoothstep(half_, half_ + ge.w * 0.6, abs(across));
                vec3 c0 = uGrout * 0.18 * (uAmb * 3.0 + uLightCol * att * 0.3) * mix(0.75, 1.0, sh);
                frag = vec4(c0 * (1.0 - 0.5 * quiet), 0.0);
                return;
            } else {
                vec2 src = ctr + dw - turnDir * across + turnDir * (across / max(abs(turnC), 0.05));
                vec2 suv = (src - uView.xy) / uView.zw * 0.5 + 0.5;
                g0 = texture(uGb0, suv);
                g1 = texture(uGb1, suv);
            }
        }
    }
    float glintT = -100.0, unsetT = 1e9;
    float setT = id > 0 ? tessState(id, glintT, unsetT) : -100.0;
    float age = uTime - setT;
    bool isSet = age >= 0.0 && uTime < unsetT;
    if (showPat > 0.5) { isSet = true; age = 10.0; }
    vec3 c;
    if (!isSet) {
        // ---- the bare setting bed: fresh lime, troweled, washed with the colors to come (as the mosaicists
        // painted it, so no gap shows), the painter's sketch in red ochre and the guide lines of the rows
        vec2 su = (w - uSketchBox.xy) / (uSketchBox.zw - uSketchBox.xy);
        vec2 sk = texture(uSketch, su).rg;
        float sk2 = texture(uSketch, su + vec2(0.0011, -0.0008)).r;
        float wob = vnoise(w * 6.0);
        float lines = smoothstep(0.1, 0.42, sk.r) * (0.7 + 0.3 * wob) + 0.3 * smoothstep(0.2, 0.6, sk2);
        float guides = sk.g * (0.5 + 0.5 * vnoise(w * 3.0 + 4.0));
        float tex = fbm(w * 2.5) * 0.55 + vnoise(w * 38.0) * 0.3 + vnoise(vec2(w.x * 1.5 + w.y * 0.8, w.y * 6.0)) * 0.15;
        vec3 bed = uBed * (0.8 + 0.3 * tex);
        vec3 wash = textureLod(uGb0, vUv, 4.2).rgb;
        wash = wash * wash * 1.8;
        float wk = 0.45 * smoothstep(0.25, 0.75, fbm(w * 1.3 + 2.0) + 0.3);
        bed = mix(bed, wash * 0.85 + bed * 0.2, wk);
        bed = mix(bed, uSinopia, clamp(lines, 0.0, 1.0) * 0.85);
        bed = mix(bed, uSinopia * 1.1, guides * 0.4);
        float lit = max(L.z, 0.0);
        c = bed * (uAmb * 1.5 + uLightCol * att * lit * 0.5);
        c *= 1.0 - 0.35 * quiet;
        frag = vec4(c, 0.0);
        return;
    }
    float mat = g0.a * 5.0 - 1.0;
    if (g0.a < 0.05) {
        // ---- grout
        float lit = max(L.z, 0.0);
        float grain = vnoise(w * 50.0);
        c = uGrout * (0.8 + 0.3 * grain) * (uAmb * 1.4 + uLightCol * att * lit * 0.45);
        c *= 1.0 - 0.35 * quiet;
        frag = vec4(c, 0.0);
        return;
    }
    vec3 alb = g0.rgb * g0.rgb;
    vec2 nxy = g1.xy * 2.0 - 1.0;
    float hgt = g1.z, rnd = g1.w;
    if (showPat > 0.5) {
        // at its tessera's center, so the design is laid tessera by tessera
        vec4 ge = texelFetch(uGeo, ivec2(id % 256, id / 256), 0);
        int k = design(showPat, ge.xy, rnd);
        alb = uPal[k] * (0.82 + 0.36 * rnd) * (showPat > 98.5 ? 1.35 : 1.0);
        mat = k == 1 ? uPalMat.x : uPalMat.y;
        // behind the lettering, the design faded toward its dark and darker
        float qh = max(box(w, uHudL, 1.2), box(w, uHudR, 1.2));
        alb = mix(alb, uPal[0], 0.55 * qh) * (1.0 - 0.45 * quiet);
    }
    // turning: its face leans away from the eye and darkens as it stands on its edge
    if (uShow > 0.5) nxy += turnDir * turnS / max(abs(turnC), 0.3) * sign(turnC) * 0.9;
    // the drop: the gold smalti ablaze, each one shimmering on its own as its tilt wavers
    float ign = uDrop * smoothstep(uIgnite, uIgnite - 4.0, length(w * vec2(0.8, 1.0))) * (1.0 - 0.9 * quiet);
    if (mat > 0.5) nxy += ign * (mat > 1.5 ? 0.2 : 0.14) * vec2(sin(uTime * (1.3 + rnd * 2.6) + rnd * 40.0), cos(uTime * (1.6 + rnd * 1.9) + rnd * 17.0));
    // a ripple lifting the tesserae as it passes
    float rho = length(w);
    float wr = (rho - uWave.x) / uWave.z;
    float lift = uWave.y * exp(-wr * wr);
    nxy += normalize(w + vec2(0.001)) * lift * 0.22;
    // just set: tapped in (it still stands a little proud, its edges catching the light), fresh
    float tap = age < 0.8 ? exp(-age * 6.0) : 0.0;
    nxy *= 1.0 + tap * 0.8;
    vec3 n = normalize(vec3(nxy, 1.0));
    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);
    float nl = max(dot(n, L), 0.0), nh = max(dot(n, H), 0.0);
    float glint = exp(-max(uTime - glintT, 0.0) * 3.5) * uGlintK;
    float sparkle = uSparkle * (1.0 - 0.75 * quiet);
    vec3 lc = uLightCol * att;
    // the room's light reflected (the vault): brighter from above
    vec3 r = reflect(-V, n);
    float env = 0.55 + 0.45 * r.y;
    if (mat > 2.5) {
        // silver smalti
        vec3 tint = vec3(0.86, 0.88, 0.92);
        c = alb * (uAmb * 0.9 + lc * nl * 0.25);
        c += tint * lc * (pow(nh, 140.0) * 5.0 * sparkle + pow(nh, 14.0) * 0.3);
        c += tint * uAmb * env * 0.6;
    } else if (mat > 1.5) {
        // gold: the leaf under a thin glass, dark until it catches the light
        vec3 gold = alb * 1.3 + vec3(0.04, 0.025, 0.0);
        c = alb * (uAmb * 0.5 + lc * nl * 0.22);
        c += gold * lc * (pow(nh, 110.0) * 6.0 * sparkle + pow(nh, 16.0) * 0.42 + pow(nh, 4.0) * 0.05);
        c += gold * uAmb * env * 0.75;
        c += gold * ign * (0.03 + pow(nh, 9.0) * 0.3 + pow(nh, 70.0) * 4.5);
    } else if (mat > 0.5) {
        // glass smalti: deep color, a sharp highlight on the fractured face
        c = alb * (uAmb * 1.1 + lc * (nl * 0.85 + 0.12));
        c += lc * (pow(nh, 180.0) * 2.4 * sparkle + pow(nh, 26.0) * 0.06);
        c += (lc * 0.7 + vec3(0.06)) * ign * (pow(nh, 90.0) * 2.2 + pow(nh, 12.0) * 0.08);
        c += alb * ign * 0.25;
    } else {
        // stone: matte, honed
        c = alb * (uAmb * 1.25 + lc * nl * 1.0);
        c += lc * pow(nh, 12.0) * 0.035;
        c += alb * ign * (0.18 + pow(nh, 18.0) * 0.5);
    }
    // the rounded edge into the grout
    c *= 0.55 + 0.45 * hgt;
    c *= 0.65 + 0.35 * abs(turnC);
    // the show: the medallions' rings chase their light around, the two counter-rotating, and on every beat a pulse
    // runs out from the heart of each figure
    if (uShow > 0.5 && uMedR.z > 0.5) {
        vec4 ge = texelFetch(uGeo, ivec2(id % 256, id / 256), 0);
        for (int sdx = 0; sdx < 2; sdx++) {
            vec2 C = sdx == 0 ? uMed.xy : uMed.zw;
            vec2 d = ge.xy - C;
            float rr = length(d);
            if (rr > uMedR.x) continue;
            float an = atan(d.y, d.x);
            float sg = sdx == 0 ? 1.0 : -1.0;
            if (rr > uMedR.y - 0.1) {
                float ch = pow(max(0.0, 0.5 + 0.5 * cos(an * 5.0 - sg * uBeat * 3.14159265 * 0.25)), 10.0);
                c += (mat > 1.5 ? alb * 1.6 + vec3(0.02) : alb * 0.7 + vec3(0.015)) * ch * uShowK * (1.0 - 0.6 * quiet);
            } else {
                float fb = fract(uBeat * 0.5);
                float pr = rr / uMedR.y - fb * 1.25;
                float pulse = exp(-pr * pr * 25.0) * (1.0 - fb);
                c += (mat > 1.5 ? alb * 1.2 : alb * 0.6 + vec3(0.01)) * pulse * uShowK * (1.0 - 0.6 * quiet);
            }
        }
    }
    // a glint on the beat grid; the tap of a tessera just set
    c += (mat > 1.5 ? alb * 2.5 + 0.1 : vec3(0.35)) * glint * pow(nh, 6.0) * 2.0 * (1.0 - 0.6 * quiet);
    c += (mat > 1.5 ? alb * 1.8 : alb * 0.6 + 0.12) * tap * 0.9;
    // a Tetris's ring: a glint running around it
    if (uRingGlint.w > 0.001) {
        vec2 d = w - uRingGlint.xy;
        float rr = length(d);
        if (rr > uRingR.x - 0.2 && rr < uRingR.y + 0.2) {
            float a = atan(d.x, d.y);
            float da = abs(mod(a - uRingGlint.z + 3.14159265, 6.2831853) - 3.14159265);
            c += (mat > 1.5 ? alb * 3.0 : alb * 0.8) * uRingGlint.w * exp(-da * da * 6.0);
        }
    }
    c *= 1.0 - 0.55 * quiet;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the tiles (the pieces)
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, rotation
layout(location = 2) in vec4 aB; // color (linear), alpha
layout(location = 3) in vec4 aC; // type, kind (0 tile, 1 ghost, 2 cracking, 3 shadow), crack, flash
layout(location = 4) in vec4 aD; // the piece's pattern origin x, y, seed, dark
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vW;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
flat out float vScale;
void main() {
    float kind = aC.y;
    vec2 off = kind > 2.5 ? vec2(0.07, -0.09) * aA.z * 2.0 : vec2(0.0);
    float grow = kind > 2.5 ? 1.12 : 1.0;
    float c = cos(aA.w), s = sin(aA.w);
    vec2 l = aPos * aA.z * grow;
    vec2 wp = aA.xy + off + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vLocal = aPos * 0.5 * grow;
    vW = wp;
    vB = aB; vC = aC; vD = aD;
    vScale = aA.z * 2.0;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vW;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
flat in float vScale;
out vec4 frag;
uniform vec3 uKey;      // the fixed key light (direction)
uniform vec3 uSinopia;
uniform float uPx;      // board units per pixel
// The painted motif of a piece type at p (pattern units: one per tile, continuous across the piece and its
// neighbours): its signed distance (< 0 inside). Several motifs meet across the tiles' edges and corners, as
// azulejo patterns do.
float sdMotif(int type, vec2 p) {
    vec2 f = fract(p) - 0.5;          // from the tile's middle
    vec2 cn = fract(p + 0.5) - 0.5;   // from the nearest tile corner
    vec2 ed = vec2(min(abs(f.x), abs(f.y)), 0.5 - max(abs(f.x), abs(f.y))); // to the nearest edge's middle
    float r = length(f), a = atan(f.y, f.x);
    if (type == 0) {
        // a ring in the middle, quarter discs on the corners (circles across four tiles)
        return min(abs(r - 0.2) - 0.055, length(cn) - 0.17);
    } else if (type == 1) {
        // a four-petalled flower, diamonds on the corners
        vec2 q = abs(f);
        float pet = min(length((q - vec2(0.17, 0.0)) * vec2(1.0, 1.9)), length((q - vec2(0.0, 0.17)) * vec2(1.9, 1.0))) - 0.12;
        float dia = (abs(cn.x) + abs(cn.y)) - 0.15;
        return min(min(pet, r - 0.06), dia);
    } else if (type == 2) {
        // a lozenge band, a dot in the middle, half dots on the edges
        float d = abs(abs(f.x) + abs(f.y) - 0.32) - 0.05;
        float e = length(ed) - 0.08;
        return min(min(d, r - 0.07), e);
    } else if (type == 3) {
        // an eight-petalled rosette
        float k = abs(cos(a * 4.0));
        float ros = r - (0.13 + 0.17 * k * k * k);
        return max(ros, -(r - 0.055));
    } else if (type == 4) {
        // a four-pointed star in a ring
        vec2 q = abs(f);
        float st = min((q.x + q.y * 3.2) * 0.42, (q.y + q.x * 3.2) * 0.42) - 0.12;
        return min(st, abs(r - 0.34) - 0.025);
    } else if (type == 5) {
        // a disc, half discs on the edges (whole across two tiles), small dots on the corners
        return min(min(r - 0.14, length(ed) - 0.12), length(cn) - 0.06);
    }
    // two crossed leaves, dots on the corners
    vec2 q = vec2(f.x + f.y, f.x - f.y) * 0.7071;
    float l1 = length(vec2(q.x * 0.5, q.y * 1.7)) - 0.12;
    float l2 = length(vec2(q.y * 0.5, q.x * 1.7)) - 0.12;
    return min(min(l1, l2), length(cn) - 0.08);
}
void main() {
    int kind = int(vC.y + 0.5);
    vec2 q = vLocal; // -0.5 .. 0.5 (a tile)
    float e = 0.455, rad = 0.075;
    vec2 dq = abs(q) - vec2(e - rad);
    float sd = length(max(dq, 0.0)) + min(max(dq.x, dq.y), 0.0) - rad;
    float aa = uPx / max(vScale, 0.05) * 1.2;
    if (kind == 3) {
        // its shadow on the bed
        float a = (1.0 - smoothstep(-0.06, 0.08, sd)) * 0.55 * vB.a;
        frag = vec4(0.0, 0.0, 0.0, a);
        return;
    }
    if (kind == 1) {
        // the ghost: the painter's sketch of where the tile will go
        float line = 1.0 - smoothstep(0.0, aa * 1.5 + 0.01, abs(sd + 0.02));
        float a = (line * 0.75 + (1.0 - smoothstep(-aa, aa, sd)) * 0.06) * vB.a;
        frag = vec4(uSinopia * a * 1.2, 0.0); // added over the bed
        return;
    }
    float cov = 1.0 - smoothstep(-aa, aa, sd);
    if (cov <= 0.0) discard;
    int type = int(vC.x + 0.5);
    vec3 col = vB.rgb;
    float seed = vD.z;
    // the glaze: a pillow, rounded over its edge
    float bevel = 1.0 - smoothstep(0.0, 0.1, -sd);
    vec2 gq = dq.x > dq.y ? vec2(sign(q.x), 0.0) : vec2(0.0, sign(q.y));
    if (dq.x > 0.0 && dq.y > 0.0) gq = normalize(max(dq, 0.0)) * sign(q);
    vec2 pq = vec2(vD.x + q.x + 0.5, vD.y + q.y + 0.5);
    // the motif, painted in a second glaze between thin dark lines (cuerda seca), standing a little proud
    float md = sdMotif(type, pq);
    float aam = aa * 0.8 + 0.004;
    float fillM = 1.0 - smoothstep(-aam, aam, md);
    float lineM = 1.0 - smoothstep(0.008, 0.008 + aam * 1.5, abs(md + 0.004));
    float raised = 1.0 - smoothstep(-0.05, 0.0, md);
    vec2 mg = vec2(dFdx(raised), dFdy(raised)) / max(uPx / max(vScale, 0.05), 1e-4);
    vec2 wob = (vec2(vnoise(pq * 6.0 + seed), vnoise(pq * 6.0 + seed + 5.0)) - 0.5) * 0.025;
    vec3 n = normalize(vec3(gq * bevel * bevel * 1.25 - mg * 0.012 + wob, 1.0));
    // the glaze pooled a little deeper in the middle and along the motif, thinner over the edge
    float th = fbm(pq * 2.3 + seed * 3.1);
    vec3 c = col * (0.86 + 0.2 * th);
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    vec3 paint = lum > 0.4 ? col * col * vec3(0.32, 0.38, 0.55) + vec3(0.015, 0.03, 0.09) : mix(col, vec3(1.0, 0.95, 0.84), 0.72);
    c = mix(c, paint * (0.92 + 0.12 * th), fillM);
    c = mix(c, col * 0.18, lineM * 0.7);
    c = mix(c, c * 0.75 + vec3(0.05), bevel * 0.4);
    // craquelure, faint
    float cr = crackle(pq * 4.2 + seed * 7.0);
    c *= 1.0 - 0.1 * (1.0 - smoothstep(0.0, 0.025, cr));
    // a cleared tile cracking before it breaks
    if (kind == 2) {
        float k = vC.z;
        float cr2 = crackle(q * 3.2 + seed * 3.3);
        float reach = smoothstep(0.0, 0.7, k) * 0.75;
        float line = (1.0 - smoothstep(0.0, 0.035, cr2)) * step(length(q), reach + 0.05);
        c = mix(c, c * 0.15, line * 0.9);
        c += col * k * 0.25;
    }
    // light: the fixed key from the upper left, the moving lamp, the room
    float att;
    vec3 L = lightDir(vW, att);
    // a near eye (the glaze reflects the room in perspective)
    vec3 V = normalize(vec3(0.0, 0.4, 16.0) - vec3(vW, 0.0));
    vec3 K = normalize(uKey);
    float dk = max(dot(n, K), 0.0), dl = max(dot(n, L), 0.0);
    vec3 lit = c * (uAmb * 1.5 + vec3(0.58) * dk + uLightCol * att * dl * 0.35);
    vec3 Hk = normalize(K + V), Hl = normalize(L + V);
    float nhk = max(dot(n, Hk), 0.0);
    float sk = pow(nhk, 90.0) * 0.9 + pow(nhk, 14.0) * 0.04;
    float sl = pow(max(dot(n, Hl), 0.0), 110.0) * 0.6;
    // the room reflected in the glaze: a soft, even sheen, brighter over the rounded edge
    vec3 rf = reflect(-V, n);
    float fres = 0.04 + 0.5 * pow(1.0 - max(dot(n, V), 0.0), 4.0);
    lit += vec3(0.9, 0.88, 0.82) * fres * 0.25 + vec3(sk) + min(uLightCol * att * sl, vec3(0.6));
    lit += vec3(0.06, 0.06, 0.07) * (0.5 + 0.5 * rf.y) * fres * 2.0;
    lit *= 1.0 - vD.w;
    lit += col * vC.w * 0.35;
    float a = cov * vB.a;
    frag = vec4(lit * a, a);
}
)";

// ---------------------------------------------------------------- fragments flying to the mosaic
inline const char* FRAG_VS = R"(#version 330 core
layout(location = 1) in vec4 aA; // x, y, rotation, scale
layout(location = 2) in vec4 aO01; // corner offsets (local)
layout(location = 3) in vec4 aO23;
layout(location = 4) in vec4 aB; // color (linear), alpha
layout(location = 5) in vec4 aC; // material, tumble, flash, 0
uniform mat4 uVP;
out vec2 vUv;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    int i = gl_VertexID;
    vec2 o = i == 0 ? aO01.xy : (i == 1 ? aO01.zw : (i == 2 ? aO23.zw : aO23.xy));
    vUv = vec2((i == 1 || i == 3) ? 1.0 : 0.0, i >= 2 ? 1.0 : 0.0);
    float c = cos(aA.z), s = sin(aA.z);
    o *= aA.w;
    vec2 wp = aA.xy + vec2(c * o.x - s * o.y, s * o.x + c * o.y);
    vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* FRAG_FS = R"(#version 330 core
in vec2 vUv;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    float e = min(min(vUv.x, 1.0 - vUv.x), min(vUv.y, 1.0 - vUv.y));
    float edge = smoothstep(0.0, 0.2, e);
    // a chip tumbling: its face catches the light as it turns
    float face = 0.55 + 0.45 * cos(vC.y);
    float flash = pow(max(cos(vC.y * 0.5 + 0.6), 0.0), 18.0);
    vec3 c = vB.rgb * (0.45 + 0.75 * face) * (0.7 + 0.3 * edge);
    c += (vC.x > 1.5 ? vB.rgb * 2.0 : vec3(0.6)) * flash * 0.9;
    c += vB.rgb * vC.z;
    frag = vec4(c * vB.a, vB.a);
}
)";

// ---------------------------------------------------------------- small lights
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, rx, ry
layout(location = 2) in vec4 aB; // rgb, intensity
layout(location = 3) in vec4 aC; // kind, rotation, a, b
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float c = cos(aC.y), s = sin(aC.y);
    vec2 l = aPos * aA.zw;
    vec2 wp = aA.xy + vec2(c * l.x - s * l.y, s * l.x + c * l.y);
    vQ = aPos;
    vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    int kind = int(vC.x + 0.5);
    float r2 = dot(vQ, vQ);
    if (kind == 0) {
        float k = exp(-r2 * 3.5) * (1.0 - smoothstep(0.7, 1.0, sqrt(r2)));
        frag = vec4(vB.rgb * vB.a * k, 0.0);
    } else if (kind == 1) {
        // a glint: a small star of light
        vec2 q = abs(vQ);
        float arms = exp(-q.x * 30.0) * exp(-q.y * 3.2) + exp(-q.y * 30.0) * exp(-q.x * 3.2);
        float k = (exp(-r2 * 30.0) + 0.55 * arms) * (1.0 - smoothstep(0.8, 1.0, max(q.x, q.y)));
        frag = vec4(vB.rgb * vB.a * k, 0.0);
    } else {
        // mortar dust from under a tile (opaque-ish, pale)
        float k = exp(-r2 * 2.5) * (1.0 - smoothstep(0.6, 1.0, sqrt(r2)));
        float a = vB.a * k;
        frag = vec4(vB.rgb * a, a * 0.6);
    }
}
)";

// ---------------------------------------------------------------- bloom, blur
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(float(x), float(y)) * uTexel).rgb;
    c /= 9.0;
    frag = vec4(max(c - vec3(uThreshold), vec3(0.0)), 1.0);
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

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uDim, uFade, uBridge, uExposure;
uniform vec3 uBridgeCol, uBridgeInk; // linear
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb * uExposure + texture(uBloom, vUv).rgb * uBloomK;
    // a soft knee: the blazing gold never burns to white
    c = c / (1.0 + 0.32 * max(max(c.r, c.g), c.b));
    // a slight vignette
    vec2 d = vUv - 0.5;
    c *= 1.0 - 0.32 * dot(d, d) * 2.0;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
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
// Lettering: pale, a soft dark halo around it.
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
    float halo = smoothstep(0.2, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a shader from its source with the common functions inserted after the version line.
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace tiles_shaders

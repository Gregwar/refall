#pragma once
// LOST PANDA's shaders: the world (one continuous world along the way home, in a side view cut away like a
// cross-section where it goes underground), the train, the pandas (kawaii stickers), the bamboo cells, the lights
// and the snacks, the lettering.

namespace lostpanda_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float hash1(float n) { return hash(vec2(n, n * 0.371 + 17.13)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
float sdEll(vec2 p, vec2 r) { float k = length(p / r); return (k - 1.0) * min(r.x, r.y) * (k < 1.0 ? 1.0 : min(1.0, k)); }
)";

// ------------------------------------------------------------------------------------------------ the train (shared)
// Two cars on a track along the way (the subway's tunnel, a ramp up a cutting to the surface, the terminus), seen in a
// cut-away: the back wall with its windows (the world behind through them), benches, poles, straps, the floor and the
// roof cut; the low near-side wall and its doors are drawn by trainFront (over the cub when it rides).
inline const char* TRAIN_GLSL = R"(
const float GY0 = -7.4, PLAT_Y = -17.4;
const float CAR = 17.0, CGAP = 0.6;
uniform float uTrainX, uDoor, uTrainLit, uTrainBody, uSway, uTrainT;
uniform float uDir;       // which way the train runs: +1 its front car leads (head lamps there), -1 backing away
uniform float uHead;      // how much its head lamps light the walls around them
// the track: level through the station and the tunnel, then a steady grade under 5 degrees (eased at both ends) up
// the cutting to the surface
float trackY(float x) {
    float t = clamp((x - 146.0) / 112.0, 0.0, 1.0);
    return mix(PLAT_Y, GY0, 0.95 * t + 0.05 * t * t * (3.0 - 2.0 * t));
}
vec2 carLocal(vec2 w, float i) {
    float x0 = uTrainX + i * (CAR + CGAP);
    float y0 = trackY(x0) - 0.3, y1 = trackY(x0 + CAR) - 0.3;
    return rot(-atan(y1 - y0, CAR)) * (w - vec2(x0, y0));
}
// a cab at an end of the train, one smooth shape flowing out of the car's body: the same top, bottom, outline and
// shading as the car, a nose rounded generously at the top and a little at the bottom, the front leaning back; one
// windscreen inset from that outline, the lamps inside its lower front corner (white ahead, red behind), a faint
// amber strip at its top, the driver a soft rounded shape behind the glass in the leading cab. n: the distance
// outward past the car's end (the cab starts 3 inside it)
float sdRoundBox4(vec2 p, vec2 b, vec4 r) {
    r.xy = p.x > 0.0 ? r.xy : r.zw;
    float rr = p.y > 0.0 ? r.x : r.y;
    vec2 q = abs(p) - b + rr;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rr;
}
const float NOSE = 1.5, CABIN = 3.0;
float cabBody(float n, float y) {
    vec2 q = vec2(n + (y - 3.75) * 0.09, y);
    return sdRoundBox4(q - vec2((NOSE - 14.0) * 0.5, 3.75), vec2((NOSE + 14.0) * 0.5, 3.55), vec4(2.3, 0.7, 0.0, 0.0));
}
void cab(vec2 l, float n, bool lead, inout vec3 col, inout float a) {
    if (l.y < -0.2 || l.y > 7.7 || n < -CABIN - 0.4 || n > NOSE + 0.6) return;
    float d = cabBody(n, l.y);
    float inCab = step(-CABIN, n);
    // the outline round the outer shape only (not where the cab meets the car)
    float bo = fillA(d - gPx * 1.5) * inCab, ba = fillA(d) * inCab;
    vec3 bc = vec3(0.13, 0.15, 0.18) * uTrainBody * (0.8 + 0.3 * smoothstep(0.0, 8.0, l.y));
    // the stripes the cars carry, following the nose
    bc = mix(bc, vec3(0.5, 0.07, 0.07) * uTrainBody, fillA(abs(l.y - 1.95) - 0.12));
    bc = mix(bc, vec3(0.45, 0.06, 0.06) * uTrainBody, fillA(abs(l.y - 6.75) - 0.08) * step(n, NOSE - 0.9));
    // the windscreen: the body's outline inset, from behind the cab's middle to the nose
    float wd = max(d + 0.32, max(abs(l.y - 4.55) - 1.45, -1.9 - n));
    wd = max(wd, -(n + 1.9) + 0.0);
    float wr = sdRoundBox4(vec2(n, l.y) - vec2(-0.4, 4.55), vec2(1.5, 1.45), vec4(1.2, 0.35, 0.35, 0.35));
    wd = max(d + 0.32, wr);
    float wo = fillA(wd - gPx * 1.5), wa = fillA(wd);
    vec3 glass = mix(vec3(0.04, 0.06, 0.08), vec3(0.08, 0.1, 0.13), smoothstep(3.1, 6.0, l.y)) * (0.8 + 0.4 * uTrainLit);
    glass += vec3(0.3, 0.26, 0.18) * 0.12 * uTrainLit;                       // the cab's own faint light
    if (lead) {
        // the driver: a soft head and shoulders, faintly lit, low in the glass
        vec2 dq = vec2(n + 0.75, l.y - 3.1);
        float drv = min(length(dq - vec2(0.0, 1.45)) - 0.48, sdRoundBox4(dq - vec2(0.0, 0.3), vec2(0.8, 0.75), vec4(0.6, 0.0, 0.6, 0.0)));
        glass = mix(glass, glass * 0.6 + vec3(0.07, 0.055, 0.04) * uTrainLit, 1.0 - smoothstep(-0.05, 0.05, drv));
    }
    // a soft reflection across the glass, an amber strip at its top
    glass += vec3(0.1) * smoothstep(0.3, 0.0, abs(n * 0.6 - (l.y - 4.5) * 0.5 + 0.1)) * 0.35;
    // the lamps in the windscreen's lower front corner
    vec2 hq = vec2(n - (NOSE - 0.75), l.y - 3.45);
    float lamp = length(hq * vec2(1.2, 1.0)) - 0.2;
    vec3 lc = lead ? vec3(1.0, 0.95, 0.82) * 2.4 : vec3(1.0, 0.12, 0.07) * 1.7;
    glass = mix(glass, lc, fillA(lamp));
    bc = mix(bc, vec3(0.02), wo);
    bc = mix(bc, glass, wa);
    col = mix(col, vec3(0.02), bo);
    col = mix(col, bc, ba);
    // the cab's wall on the cut-away side: like the cars' end walls
    float wall = abs(n + CABIN + 0.15) - 0.15;
    wall = max(wall, max(0.2 - l.y, l.y - 7.3));
    col = mix(col, vec3(0.02), fillA(wall - gPx * 1.5) * step(n, -CABIN + 0.01));
    col = mix(col, vec3(0.13, 0.15, 0.18) * uTrainBody * (0.8 + 0.3 * smoothstep(0.0, 8.0, l.y)), fillA(wall));
    a = max(a, max(bo, fillA(wall - gPx * 1.5)));
    // the lamp's glow
    float gl = exp(-dot(hq, hq) / (lead ? 0.7 : 0.25)) * (lead ? 0.55 : 0.35);
    col += (lead ? vec3(1.0, 0.9, 0.7) : vec3(1.0, 0.12, 0.06)) * gl;
    a = max(a, sat(gl * 1.5));
}
// the head lamps' light on the walls ahead (and a little round them)
vec3 headGlow(vec2 w) {
    float x0 = uDir > 0.0 ? uTrainX + 2.0 * CAR + CGAP + 0.8 : uTrainX - 0.8;
    vec2 hp = vec2(x0, trackY(x0) - 0.3 + 3.45);
    vec2 d = w - hp;
    float dx = d.x * sign(uDir);
    float beam = dx > 0.0 ? exp(-dx / 11.0) * exp(-sq(d.y / (0.7 + dx * 0.32))) : 0.0;
    float halo = exp(-dot(d, d) / 4.0);
    return vec3(1.0, 0.92, 0.75) * (beam * 0.9 + halo * 0.25);
}
vec4 trainInside(vec2 w, vec3 behind) {
    vec3 col = behind;
    float a = 0.0;
    for (int ci = 0; ci < 2; ci++) {
        vec2 l = carLocal(w, float(ci));
        if (l.x < -2.8 || l.x > CAR + 2.8 || l.y < -0.8 || l.y > 7.8) continue;
        vec3 body = vec3(0.13, 0.15, 0.18) * uTrainBody;
        vec3 inside = vec3(0.16, 0.15, 0.13);
        // the gangway to the next car
        if (ci == 0) {
            float gw = min(sdBox(l - vec2(CAR + CGAP * 0.5, 3.6), vec2(CGAP * 0.5 + 0.05, 3.0)), sdBox(l - vec2(CAR + CGAP * 0.5, 0.45), vec2(CGAP * 0.5 + 0.25, 0.1)));
            col = mix(col, vec3(0.03), fillA(gw));
            a = max(a, fillA(gw));
        }
        if ((ci == 0 && l.x > CAR + 0.1) || l.x < -0.3 || (ci == 1 && l.x > CAR + 0.1 && false)) {
            if (ci == 0 && l.x < -0.3) cab(l, -l.x, uDir < 0.0, col, a);
            continue;
        }
        if (ci == 1 && l.x > CAR + 0.1) { cab(l, l.x - CAR, uDir > 0.0, col, a); continue; }
        // wheels and bogies
        float wh = 1e3;
        for (int k = 0; k < 4; k++) {
            float wx = k == 0 ? 2.3 : (k == 1 ? 3.7 : (k == 2 ? CAR - 3.7 : CAR - 2.3));
            wh = min(wh, length(l - vec2(wx, 0.0)) - 0.38);
        }
        float bog = min(sdBox(l - vec2(3.0, 0.25), vec2(1.3, 0.14)), sdBox(l - vec2(CAR - 3.0, 0.25), vec2(1.3, 0.14)));
        float wa = fillA(min(wh, bog));
        col = mix(col, vec3(0.025), wa);
        a = max(a, wa);
        // the back wall: windows (the world behind), doors, benches, adverts, the light strip, poles, straps
        float wallA = fillA(sdBox(l - vec2(CAR * 0.5, 3.7), vec2(CAR * 0.5 - 0.3, 2.9)));
        if (wallA > 0.0) {
            vec3 wc = inside * (0.85 + 0.35 * smoothstep(0.8, 7.0, l.y));
            wc += vec3(1.0, 0.85, 0.6) * 0.06 * exp(-sq((l.y - 6.3) / 1.6));
            float dr = min(sdBox(l - vec2(4.6, 3.6), vec2(0.9, 2.8)), sdBox(l - vec2(12.4, 3.6), vec2(0.9, 2.8)));
            wc = mix(wc, inside * 0.72, fillA(dr));
            float win = min(sdBox(l - vec2(2.0, 4.2), vec2(0.95, 1.0)), min(sdBox(l - vec2(8.5, 4.2), vec2(2.4, 1.0)), sdBox(l - vec2(15.0, 4.2), vec2(0.95, 1.0)))) - 0.3;
            win = min(win, min(sdBox(l - vec2(4.6, 4.4), vec2(0.45, 0.8)), sdBox(l - vec2(12.4, 4.4), vec2(0.45, 0.8))) - 0.1);
            wc = mix(wc, vec3(0.05), fillA(win - 0.14));
            vec3 glass = behind * 0.85 + vec3(0.03, 0.035, 0.04) + vec3(0.05) * smoothstep(-1.0, 1.0, l.x * 0.3 + l.y - 4.2);
            wc = mix(wc, glass, fillA(win));
            float bench = min(sdBox(l - vec2(8.5, 1.45), vec2(2.7, 0.62)), min(sdBox(l - vec2(1.95, 1.45), vec2(1.35, 0.62)), sdBox(l - vec2(15.05, 1.45), vec2(1.35, 0.62)))) - 0.12;
            wc = mix(wc, mix(vec3(0.05, 0.08, 0.2), vec3(0.09, 0.14, 0.3), smoothstep(0.8, 2.1, l.y)), fillA(bench));
            float ad = sdBox(vec2(mod(l.x - 1.0, 3.6) - 1.8, l.y - 5.75), vec2(1.3, 0.25));
            vec3 adC = (0.4 + 0.35 * cos(6.2831 * (hash1(floor((l.x - 1.0) / 3.6) + float(ci) * 7.0) + vec3(0.0, 0.33, 0.67)))) * 0.35;
            wc = mix(wc, adC, fillA(ad) * step(1.0, l.x) * step(l.x, CAR - 1.0));
            wc = mix(wc, vec3(1.0, 0.9, 0.7) * 1.5, fillA(sdBox(l - vec2(CAR * 0.5, 6.4), vec2(CAR * 0.5 - 1.0, 0.08))));
            float pole = 1e3;
            for (int k = 0; k < 4; k++) {
                float px = k == 0 ? 3.5 : (k == 1 ? 5.7 : (k == 2 ? 11.3 : 13.5));
                pole = min(pole, abs(l.x - px) - 0.06);
            }
            wc = mix(wc, vec3(0.75, 0.62, 0.18) * 0.6, fillA(max(pole, l.y - 6.15)));
            float inRange = step(0.6, l.x) * step(l.x, CAR - 0.6);
            wc = mix(wc, vec3(0.3, 0.3, 0.28), fillA(abs(l.y - 6.15) - 0.04) * inRange);
            float sx = mod(l.x, 1.5) - 0.75, sid = floor(l.x / 1.5);
            vec2 s0 = rot(-0.07 * sin(uTrainT * 0.9 + sid * 0.7) * uSway) * vec2(sx, l.y - 6.15);
            float strap = min(sdBox(s0 - vec2(0.0, -0.3), vec2(0.035, 0.3)), abs(length(s0 - vec2(0.0, -0.74)) - 0.13) - 0.035);
            wc = mix(wc, vec3(0.45, 0.43, 0.38), fillA(strap) * inRange);
            wc *= uTrainLit;
            col = mix(col, wc, wallA);
            a = max(a, wallA);
        }
        // the floor, the roof, the ends: the cut, outlined (not under the cabs, which are one shape of their own)
        bool cabSide = (ci == 1 && l.x > CAR - 3.15) || (ci == 0 && l.x < 3.15);
        float sh = cabSide ? 1e3 : min(sdBox(l - vec2(CAR * 0.5, 0.5), vec2(CAR * 0.5, 0.3)), sdBox(l - vec2(CAR * 0.5, 6.95), vec2(CAR * 0.5, 0.35)));
        sh = min(sh, min(sdBox(l - vec2(0.15, 3.75), vec2(0.15, 3.55)), sdBox(l - vec2(CAR - 0.15, 3.75), vec2(0.15, 3.55))));
        float so = fillA(sh - gPx * 1.5), sa = fillA(sh);
        col = mix(col, vec3(0.02), so);
        col = mix(col, body * (0.8 + 0.3 * smoothstep(0.0, 8.0, l.y)), sa);
        col = mix(col, vec3(0.45, 0.06, 0.06) * uTrainBody, fillA(abs(l.y - 6.75) - 0.08) * sa * step(abs(l.x - CAR * 0.5), CAR * 0.5 - 0.3));
        a = max(a, so);
        // the cabs at the train's two ends
        if (ci == 1) cab(l, l.x - CAR, uDir > 0.0, col, a);
        else cab(l, -l.x, uDir < 0.0, col, a);
    }
    return vec4(col, a);
}
vec4 trainFront(vec2 w) {
    vec4 o = vec4(0.0);
    for (int ci = 0; ci < 2; ci++) {
        vec2 l = carLocal(w, float(ci));
        if (l.x < -0.3 || l.x > CAR + 0.3 || l.y < 0.4 || l.y > 2.8) continue;
        float p0 = ci == 0 ? 3.3 : 0.15, p1 = ci == 0 ? CAR - 0.15 : CAR - 3.3;   // (the cabs at the ends are solid)
        float panel = sdBox(l - vec2((p0 + p1) * 0.5, 1.55), vec2((p1 - p0) * 0.5, 0.75));
        float open = uDoor * 0.95;
        panel = max(panel, -sdBox(l - vec2(4.6, 1.55), vec2(open, 1.0)));
        panel = max(panel, -sdBox(l - vec2(12.4, 1.55), vec2(open, 1.0)));
        float a = fillA(panel - gPx * 1.5);
        if (a <= 0.0) continue;
        vec3 c = vec3(0.16, 0.18, 0.21) * (0.85 + 0.3 * smoothstep(0.8, 2.3, l.y)) * uTrainBody;
        c = mix(c, vec3(0.5, 0.07, 0.07) * uTrainBody, fillA(abs(l.y - 1.95) - 0.12));
        for (int k = 0; k < 2; k++) {
            float dx = abs(l.x - (k == 0 ? 4.6 : 12.4));
            float seam = min(abs(dx - 0.95), abs(dx - open)) - 0.025;
            c = mix(c, vec3(0.03), fillA(seam) * step(dx, 0.98));
        }
        c = mix(vec3(0.02), c, fillA(panel));
        o = vec4(mix(o.rgb, c, a), max(o.a, a));
    }
    return o;
}
)";

// ------------------------------------------------------------------------------------------------ the world
// One continuous world in a side view, its near plane cut away like a cross-section where it goes underground:
// world x along the way (0 the rainy street .. ~470 the glade), world y up (the street at GY0, the subway's platform
// at PLAT_Y). The camera (uD, uCamY: the world point at the screen's origin) follows the cub. Layers behind the near
// plane scroll at their own parallax k (x and y); what stands on the near plane (lamps, the stairs, the train, the
// ground and the earth's cut) moves with the world. Where each kind of scenery stands is a density along the way,
// evaluated at the object's own place (no popping): the city thins out into the industrial district, then the fields,
// the bamboo, the glade.
inline const char* WORLD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform float uPxP;          // board units per pixel of this target
uniform float uD, uCamY;     // the camera: the world point at the screen's origin
uniform float uTime, uBeat, uPeak, uDrop, uKick, uRain, uStars, uClouds, uOver, uWorldA, uSlow, uMid, uLamp, uDawn;
uniform vec3 uSkyTop, uSkyHor, uFog, uSunCol, uBridgeCol;
uniform vec4 uSun;           // screen x, y, radius, strength
uniform float uSubLight;     // the station's light (it comes on as the cub nears)

const float PI = 3.14159265, TAU = 6.2831853;
float gPx = 0.02;
float gGlow = 0.0;           // the light of lamps at the pixel (it lights the rain)
vec3 gGlowC = vec3(1.0);

float fillA(float d) { return 1.0 - smoothstep(-gPx, gPx, d); }
float fillS(float d, float s) { return 1.0 - smoothstep(-s, s, d); }
float lod(float cell) { return smoothstep(2.0, 5.0, cell / gPx); }
vec3 lay(vec3 c, vec3 col, float a) { return mix(c, col, sat(a)); }
vec3 haze(vec3 c, float h) { return mix(c, uFog, h); }
vec2 layerP(vec2 p, float k) { return vec2(p.x, p.y + uCamY * k); }
//@TRAIN

// ---- where things are along the way (world x)
float zCity(float x) { return 1.0 - smoothstep(205.0, 250.0, x); }
float zShop(float x) { return smoothstep(24.0, 50.0, x) * (1.0 - smoothstep(190.0, 215.0, x)); }
float zInd(float x) { return smoothstep(215.0, 250.0, x) * (1.0 - smoothstep(330.0, 355.0, x)); }
float zNature(float x) { return smoothstep(300.0, 350.0, x); }
float zField(float x) { return smoothstep(328.0, 352.0, x) * (1.0 - smoothstep(392.0, 418.0, x)); }
float zBamboo(float x) { return smoothstep(360.0, 400.0, x) * (1.0 - 0.8 * smoothstep(436.0, 454.0, x)); }
float zGlade(float x) { return smoothstep(436.0, 454.0, x); }
// the place far layers show at an object's screen x (sx): far things fade in and out with the way, gently
float xFar(float sx) { return uD + sx * 1.3; }

// ---------------------------------------------------------------- the sky (its colors follow the time of day)
vec3 sky(vec2 p) {
    float py = p.y + uCamY * 0.08;
    float h = sat((py - GY0) / 19.4);
    vec3 c = mix(uSkyHor, uSkyTop, pow(h, 0.65));
    if (uStars > 0.0) {
        vec2 q = vec2(p.x + uD * 0.01, py) * 1.7;
        vec2 i = floor(q), f = fract(q) - 0.5;
        vec2 o = hash2(i) - 0.5;
        float on = step(0.86, hash(i + 3.1));
        float d = length(f - o * 0.6);
        float tw = 0.6 + 0.4 * sin(uTime * (0.7 + hash(i + 9.0)) + hash(i) * 20.0);
        c += vec3(0.9, 0.92, 1.0) * on * exp(-d * d * 500.0) * uStars * smoothstep(0.25, 0.8, h) * tw * 0.8;
    }
    if (uSun.w > 0.0) {
        float r = length(p - uSun.xy);
        c += uSunCol * (exp(-r * r / sq(uSun.z * 3.0)) * 0.45 + exp(-r / (uSun.z * 6.0)) * 0.35) * uSun.w;
        c = mix(c, uSunCol * 1.5 + 0.2, fillS(r - uSun.z, gPx * 1.5) * uSun.w * 0.9);
    }
    if (uClouds > 0.0) {
        for (int i = 0; i < 2; i++) {
            float fi = float(i);
            float x = p.x + uD * (0.03 + 0.03 * fi) + uTime * (0.1 + 0.06 * fi) + fi * 37.0;
            float cw = 15.0 - fi * 4.0;
            float id = floor(x / cw);
            if (hash1(id + fi * 9.0) > 0.7) continue;
            vec2 ctr = vec2((id + 0.5 + (hash1(id + 2.0) - 0.5) * 0.4) * cw, 3.0 + 4.0 * fi + 2.5 * hash1(id + 4.0));
            float sc = (0.8 + 0.5 * hash1(id + 6.0)) * (1.0 - 0.3 * fi);
            vec2 q = (vec2(x, py) - ctr) / sc;
            float d = 1e3;
            for (int k = 0; k < 5; k++) {
                float fk = float(k);
                float mid = 2.0 - abs(fk - 2.0);
                vec2 o = vec2((fk - 2.0) * 1.05, 0.25 * sin(fk * 2.3 + id) + mid * 0.42);
                d = min(d, length(q - o) - (0.75 + 0.35 * mid + 0.2 * hash1(id + fk)));
            }
            d = max(d, -(q.y + 0.55));
            float a = fillS(d * sc, 0.06 + gPx) * uClouds * (0.92 - 0.25 * fi);
            vec3 lit = mix(mix(uSkyHor, uSkyTop, 0.35) * 0.95, mix(uSunCol, vec3(1.0), 0.55) * 1.05, smoothstep(-0.7, 1.5, q.y));
            c = mix(c, lit, a);
        }
    }
    return c;
}

// ---------------------------------------------------------------- pieces of scenery
float windowsLit(vec2 q, vec2 cell, vec2 half, float seed, float prob, out float glass) {
    vec2 g = q / cell;
    vec2 gi = floor(g), gf = (fract(g) - 0.5) * cell;
    float w = fillA(sdBox(gf, half));
    float lit = step(hash(gi + seed), prob) * (0.55 + 0.45 * hash(gi + seed + 4.7));
    float l = lod(min(cell.x, cell.y));
    glass = mix(half.x * half.y * 4.0 / (cell.x * cell.y), w, l);
    return mix(prob * 0.75 * glass, w * lit, l);
}

// a far skyline of towers in layer k (p in layer coords): only where the city is
vec3 skyline(vec2 p, vec3 c, float k, float base, float hmin, float hmax, float cw, vec3 bc, float prob, float seed, float hz) {
    float u = p.x + uD * k + seed * 13.0;
    float id = floor(u / cw);
    float dens = zCity(xFar((id + 0.5) * cw - seed * 13.0 - uD * k));
    if (dens <= 0.005) return c;
    float lx = (u / cw - id) * cw;
    float h = hmin + (hmax - hmin) * pow(hash1(id + seed), 1.4);
    float w = cw * (0.62 + 0.3 * hash1(id * 1.3 + seed));
    float x0 = 0.2 * cw * hash1(id + 5.0);
    float top = base + h;
    float d = max(abs(lx - x0 - w * 0.5) - w * 0.5, p.y - top);
    if (hash1(id + 11.0) > 0.6) d = min(d, max(abs(lx - x0 - w * 0.5) - w * 0.25, p.y - top - 0.9));
    float a = fillA(d);
    if (a <= 0.0) return c;
    vec3 b = bc * (0.85 + 0.3 * hash1(id + 2.0));
    b *= 0.9 + 0.25 * sat((p.y - base) / max(h, 1.0));
    float gl;
    float lit = windowsLit(vec2(lx - x0, p.y - base), vec2(0.36, 0.5), vec2(0.1, 0.13), id * 7.3 + seed, prob, gl);
    b += vec3(1.0, 0.68, 0.36) * lit * 0.55 * uLamp;
    return mix(c, haze(b, hz), a * dens);
}

// a street lamp every `sp` (world, layer 1): `on` its light; exists where `zone` says
vec3 lampAt(vec2 w, vec3 c, float lx, vec3 lc, float hgt, float on) {
    vec2 head = vec2(0.7, GY0 + hgt);
    float d = sdBox(vec2(lx, w.y - (GY0 + hgt * 0.5)), vec2(0.07, hgt * 0.5));
    d = min(d, sdSeg(vec2(lx, w.y), vec2(0.0, GY0 + hgt - 0.1), vec2(0.35, GY0 + hgt + 0.32)) - 0.05);
    d = min(d, sdSeg(vec2(lx, w.y), vec2(0.35, GY0 + hgt + 0.32), head + vec2(0.0, 0.2)) - 0.05);
    d = min(d, sdBox(vec2(lx, w.y - (GY0 + 0.25)), vec2(0.13, 0.25)));
    float dh = sdBox(vec2(lx - head.x, w.y - head.y - 0.12), vec2(0.28 - (w.y - head.y) * 0.25, 0.1));
    c = lay(c, vec3(0.03, 0.035, 0.05) + lc * 0.04 * on, fillA(min(d, dh)));
    vec2 lp = vec2(lx - head.x, w.y - head.y);
    float r = length(lp * vec2(1.0, 1.3));
    float glass = fillA(sdEll(lp + vec2(0.0, 0.02), vec2(0.22, 0.09)));
    float halo = exp(-r * r / 0.5) * 0.9 + exp(-r / 1.8) * 0.35;
    float coneX = abs(lp.x) / max(0.2, -lp.y * 0.55 + 0.25);
    float cone = (1.0 - smoothstep(0.6, 1.0, coneX)) * step(lp.y, 0.0) * smoothstep(GY0 - 0.5, GY0 + 1.5, w.y) * 0.12 * exp(lp.y * 0.08);
    float g = (halo + cone) * on;
    c += lc * g;
    c = mix(c, lc * 2.2 + 0.3, glass * on);
    gGlow += g; gGlowC = lc;
    return c;
}
float lampReflectAt(float lx, float my) {
    return exp(-lx * lx / 0.08) * exp(-my / 3.2) * 0.55 + exp(-lx * lx / 3.0) * exp(-my * 1.2) * 0.25;
}

vec3 rain(vec2 p, vec3 c, float amt) {
    if (amt <= 0.0) return c;
    float acc = 0.0;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float sc = 1.0 + fi * 0.55;
        vec2 q = p;
        q.x += q.y * 0.12 + uD * (0.6 + 0.3 * fi);
        q.y += uCamY * (0.6 + 0.3 * fi);
        float cw = 0.33 * sc;
        float ci = floor(q.x / cw);
        float fx = q.x / cw - ci - 0.5;
        float h = hash1(ci * 1.37 + fi * 11.0);
        float len = 0.9 * sc, per = 5.5 * sc + h * 3.0;
        float y = q.y + uTime * (16.0 + 5.0 * fi) * (1.0 - 0.6 * uSlow) + h * 97.0;
        float fy = mod(y, per) / len;
        float s = smoothstep(0.0, 0.25, fy) * (1.0 - smoothstep(0.7, 1.0, fy));
        float wid = max(0.012 * sc, gPx * 0.7);
        float xo = (h - 0.5) * 0.6;
        float line = 1.0 - smoothstep(wid, wid + gPx * 1.2, abs(fx - xo) * cw);
        acc += line * s * (0.45 + 0.25 * fi) * step(hash1(ci + fi * 5.0), 0.75 * amt + 0.1);
    }
    vec3 rc = mix(uFog * 1.6 + 0.04, gGlowC * 1.4, sat(gGlow * 1.2));
    return c + rc * acc * amt * (0.25 + 1.4 * sat(gGlow));
}

float puddleRings(vec2 w) {
    vec2 q = vec2(w.x, (w.y - GY0) * 3.2);
    float acc = 0.0;
    for (int i = 0; i < 2; i++) {
        vec2 g = q / 1.1 + float(i) * 0.5;
        vec2 gi = floor(g), gf = fract(g) - 0.5;
        float h = hash(gi + float(i) * 7.0);
        float t = fract(uTime * 0.7 + h);
        vec2 o = (hash2(gi) - 0.5) * 0.5;
        float r = length(gf - o) * 1.1;
        acc += (1.0 - smoothstep(0.0, 0.03 + gPx, abs(r - t * 0.45))) * (1.0 - t) * 0.6;
    }
    return acc;
}

vec3 bokeh(vec2 p, vec3 c, float amt, vec3 c1, vec3 c2) {
    if (amt <= 0.0) return c;
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        vec2 q = vec2(p.x + uD * (1.4 + 0.3 * fi) + fi * 3.0, p.y + uCamY * 1.4);
        float cw = 6.5;
        float id = floor(q.x / cw);
        float h = hash1(id + fi * 31.0);
        if (h > 0.55) continue;
        vec2 ctr = vec2((id + 0.2 + 0.6 * hash1(id + 3.0)) * cw, mix(-10.0, 9.0, hash1(id + 7.0)) + sin(uTime * 0.2 + h * 9.0) * 0.4);
        float r = 0.7 + 1.1 * hash1(id + 9.0);
        float d = length(q - ctr) - r;
        float disc = 1.0 - smoothstep(-0.15, 0.05, d);
        float rim = exp(-sq(d / 0.08)) * 0.5;
        vec3 bc = mix(c1, c2, step(0.5, hash1(id + 13.0)));
        c += bc * (disc * 0.1 + rim * 0.06) * amt;
    }
    return c;
}

// ---- the facades across the street (layer k, p in layer coords): shops and neon where the shopping street is
vec3 facades(vec2 p, vec3 c, float k, float base, float hz) {
    float u = p.x + uD * k + 21.0;
    float cw = 6.2;
    float id = floor(u / cw);
    float xe = ((id + 0.5) * cw - 21.0) / k;
    float city = zCity(xe);
    if (hash1(id + 0.3) > city) return c;
    float shop = zShop(xe), neon = shop;
    float lx = (u / cw - id) * cw;
    float h1 = hash1(id * 3.1);
    float top = base + (6.0 + 7.0 * h1) * (0.45 + 0.55 * city);
    float bw = cw - 0.35;
    vec3 bc = mix(vec3(0.03, 0.04, 0.07), vec3(0.06, 0.04, 0.045), hash1(id + 1.0));
    bc = mix(bc, vec3(0.035, 0.06, 0.065), step(0.7, hash1(id + 12.0)));
    bc *= 0.9 + 0.2 * hash1(id + 4.0);
    bc = mix(bc, bc * 1.6 + uFog * 0.25, uDawn * 0.6);
    float inB = fillA(max(abs(lx - bw * 0.5) - bw * 0.5, p.y - top));
    float roof = fillA(max(abs(lx - bw * 0.5) - bw * 0.5 - 0.12, abs(p.y - top + 0.1) - 0.12));
    if (inB + roof <= 0.0) return c;
    vec3 b = bc * (0.85 + 0.35 * sat((p.y - base) / 10.0));
    b = mix(b, bc * 1.6, roof);
    float gl;
    float wl = windowsLit(vec2(lx - 0.45, p.y - base - 3.2), vec2(1.32, 1.75), vec2(0.34, 0.5), id * 3.7, 0.3 + 0.1 * shop, gl);
    float inWin = step(base + 3.0, p.y) * step(p.y, top - 0.7) * step(0.3, lx) * step(lx, bw - 0.3);
    gl *= inWin; wl *= inWin;
    vec3 warm = mix(vec3(1.0, 0.62, 0.3), vec3(1.0, 0.82, 0.55), hash1(id + 8.0));
    b = mix(b, mix(uFog * 0.5, b * 1.2, 0.5), gl * 0.5);
    b += warm * wl * 0.8 * uLamp;
    float sx = lx - bw * 0.5;
    float shopW = fillA(sdBox(vec2(sx, p.y - base - 1.15), vec2(bw * 0.38, 1.05)));
    vec3 shopC = mix(vec3(1.0, 0.75, 0.45), vec3(0.75, 0.9, 1.0), step(0.6, hash1(id + 21.0)));
    float lit = (0.25 + 0.75 * shop) * step(0.3, hash1(id + 17.0));
    float slat = 0.8 + 0.2 * smoothstep(0.3, 0.5, abs(fract((p.y - base) * 4.0) - 0.5)) * lod(0.25);
    vec3 shutter = vec3(0.05, 0.055, 0.07) * slat + uFog * 0.15;
    float shelves = 0.7 + 0.3 * step(0.5, fract((p.y - base) * 1.6));
    vec3 window = shopC * shelves * (0.4 + 0.6 * uLamp) * 0.9;
    b = mix(b, mix(shutter, window, lit * step(0.5, shop + 0.3)), shopW);
    float aw = fillA(sdBox(vec2(sx, p.y - base - 2.45), vec2(bw * 0.44, 0.22)));
    vec3 awC = mix(vec3(0.35, 0.08, 0.08), vec3(0.08, 0.2, 0.22), step(0.5, hash1(id + 31.0)));
    awC = mix(awC, awC * 1.8 + 0.03, step(0.5, fract(sx * 1.5)));
    b = mix(b, awC * (0.5 + 0.5 * uLamp), aw);
    if (neon > 0.05 && hash1(id + 41.0) < 0.7 * neon) {
        vec3 nc = hash1(id + 43.0) < 0.5 ? vec3(1.0, 0.25, 0.55) : (hash1(id + 44.0) < 0.5 ? vec3(0.25, 0.85, 1.0) : vec3(1.0, 0.65, 0.2));
        vec2 nq = vec2(lx - bw + 0.65, p.y - base - 5.0 - 2.5 * hash1(id + 47.0));
        float sign = sdBox(nq, vec2(0.38, 1.4));
        float tube = 1.0 - smoothstep(0.03, 0.03 + gPx * 1.5, abs(sign + 0.1));
        float letters = (1.0 - smoothstep(0.04, 0.04 + gPx * 1.5, abs(nq.x))) * step(0.35, fract(nq.y * 1.1)) * step(abs(nq.y), 1.2);
        b = mix(b, vec3(0.02), fillA(sign) * 0.8);
        float glow = exp(-max(sign, 0.0) * 1.6) * 0.35;
        b += nc * (tube + letters + glow) * (0.85 + 0.15 * sin(uTime * 0.8 + id)) * (0.6 + 0.4 * uLamp) * uLamp;
    }
    return mix(c, haze(b, hz), sat(inB + roof));
}

// ---- passers-by under umbrellas (layer k), on the shopping street
vec3 passers(vec2 p, vec3 c, float k, vec3 rim) {
    float amt = 0.75 * zShop(p.x + uD) * uRain;
    if (amt <= 0.01) return c;
    for (int g = 0; g < 2; g++) {
        float dir = g == 0 ? 1.0 : -1.0;
        float speed = 1.05 + 0.2 * float(g);
        float u = p.x + uD * k - dir * uTime * speed * (1.0 - 0.6 * uSlow) + float(g) * 4.3;
        float cw = 8.5;
        float id = floor(u / cw);
        float h = hash1(id + float(g) * 17.0);
        if (h > amt) continue;
        float sx = (u / cw - id) * cw - cw * 0.5 - (hash1(id + 3.0) - 0.5) * 3.0;
        vec2 q = vec2(sx * dir, p.y - GY0 - 0.1);
        float s = 0.9 + 0.25 * hash1(id + 5.0);
        q /= s;
        float ph = (uTime * speed * (1.0 - 0.6 * uSlow) * 2.0 + h * 10.0) * 1.1;
        float sw = sin(ph) * 0.4;
        q.y -= 0.08 * abs(cos(ph));                                                   // a light bob with each step
        // a walker in a coat: legs under a flared coat, a round head, one arm up holding the umbrella just overhead
        float d = min(sdSeg(q, vec2(0.0, 2.8), vec2(sw * 0.75, 0.25)), sdSeg(q, vec2(0.0, 2.8), vec2(-sw * 0.75, 0.25))) - 0.3;
        d = min(d, sdBox(q - vec2(0.0, 4.25), vec2(0.5 - (q.y - 4.25) * 0.15, 1.5)) - 0.4);   // rounded shoulders, flared hem
        d = min(d, length(q - vec2(0.1, 6.65)) - 0.64);
        d = min(d, sdSeg(q, vec2(0.3, 5.3), vec2(0.85, 6.05)) - 0.19);                     // the arm, the hand in front
        vec2 uq = q - vec2(0.85, 7.6);
        uq = mat2(0.985, 0.17, -0.17, 0.985) * uq;                                    // tilted a little forward
        float dome = max(length(uq * vec2(1.0, 1.75)) - 2.05, -uq.y);
        float scal = length(vec2(mod(uq.x + 2.05, 1.025) - 0.5125, uq.y + 0.3)) - 0.42;   // the scalloped hem
        dome = max(dome, -scal);
        float um = min(dome, sdBox(uq - vec2(0.0, -0.8), vec2(0.045, 0.8)));
        um = min(um, length(uq - vec2(0.0, 1.2)) - 0.1);                               // the ferrule on top
        vec3 uc = hash1(id + 9.0) < 0.35 ? vec3(0.45, 0.06, 0.08) : (hash1(id + 10.0) < 0.5 ? vec3(0.04, 0.05, 0.07) : vec3(0.1, 0.16, 0.22));
        c = lay(c, vec3(0.022, 0.024, 0.032) + rim * 0.06 * smoothstep(-0.25, 0.0, d), fillA(d * s));
        c = lay(c, uc * (0.7 + 0.5 * smoothstep(-0.6, 0.6, uq.y)) + rim * 0.08 * exp(-abs(dome) * 6.0), fillA(um * s));
    }
    return c;
}

// ---- the industrial district far away (layer k): chimneys and their smoke, gas holders, cranes
vec3 industry(vec2 p, vec3 c, float k) {
    float u = p.x + uD * k;
    float cw = 9.0;
    float id = floor(u / cw);
    float dens = zInd(xFar((id + 0.5) * cw - uD * k));
    if (dens <= 0.005) return c;
    float lx = (u / cw - id) * cw - cw * 0.5;
    vec3 sil = mix(mix(vec3(0.12, 0.09, 0.14), uFog, 0.45), uFog, 1.0 - dens);
    float base = GY0 + 2.5;
    float ch = 6.0 + 6.0 * hash1(id + 2.0);
    float d = sdBox(vec2(lx - 1.5, p.y - base - ch * 0.5), vec2(0.28 - (p.y - base) * 0.01, ch * 0.5));
    if (hash1(id + 5.0) > 0.5) {
        vec2 g = vec2(lx + 2.0, p.y - base - 1.6);
        float lat = min(abs(fract(g.x * 1.2 + g.y * 0.6) - 0.5), abs(fract(g.x * 1.2 - g.y * 0.6) - 0.5)) / 1.2;
        d = min(d, max(sdBox(g, vec2(1.8, 1.6)), -(lat - 0.05)));
        d = min(d, max(sdBox(g, vec2(1.9, 1.62)), -sdBox(g, vec2(1.75, 1.5))));
    }
    if (hash1(id + 8.0) > 0.55) {
        vec2 g = vec2(lx + 3.4, p.y - base);
        d = min(d, sdBox(g - vec2(0.0, 3.5), vec2(0.15, 3.5)));
        d = min(d, sdSeg(g, vec2(-1.5, 7.0), vec2(4.0, 7.0)) - 0.09);
        d = min(d, sdSeg(g, vec2(0.0, 7.6), vec2(3.6, 7.0)) - 0.03);
    }
    d = min(d, p.y - base - 1.0 - 0.6 * step(0.5, fract(u * 0.15)));
    c = lay(c, sil, fillA(d) * dens);
    vec2 sp = vec2(lx - 1.5, p.y - base - ch);
    float sm = fbm(vec2(sp.x * 0.5 - uTime * 0.12 + id, sp.y * 0.35 - uTime * 0.08));
    float plume = smoothstep(0.0, 0.5, sp.y) * exp(-sq((sp.x - sp.y * 0.6) / (0.4 + sp.y * 0.35))) * exp(-sp.y * 0.12);
    c = mix(c, mix(uFog, vec3(1.0, 0.85, 0.8), 0.25), sat(plume * sm * 1.6) * 0.6);
    vec2 rl = vec2(lx - 1.5, p.y - base - ch - 0.15);
    c += vec3(1.0, 0.15, 0.1) * exp(-dot(rl, rl) / 0.03) * (0.5 + 0.5 * sin(uTime * 1.3 + id * 2.0)) * 0.8;
    return c;
}
// warehouses with saw-tooth roofs (layer k), freight wagons
vec3 warehouses(vec2 p, vec3 c, float k) {
    float u = p.x + uD * k + 7.0;
    float cw = 11.0;
    float id = floor(u / cw);
    float dens = zInd(((id + 0.5) * cw - 7.0) / k);
    if (hash1(id + 0.4) < dens) {
        float lx = (u / cw - id) * cw;
        float base = GY0 + 1.3;
        float hgt = 2.6 + 1.4 * hash1(id);
        float saw = fract(lx / 1.6);
        float roofY = base + hgt + 0.9 * saw;
        float d = max(p.y - roofY, abs(lx - cw * 0.45) - cw * 0.42);
        vec3 wh = mix(vec3(0.15, 0.1, 0.12), vec3(0.24, 0.15, 0.14), hash1(id + 3.0));
        wh = haze(wh, 0.25);
        wh = mix(wh, mix(uSkyHor, vec3(1.0, 0.7, 0.55), 0.3), smoothstep(0.75, 0.95, saw) * step(roofY - 0.9 * saw - 0.01, p.y) * 0.6);
        float gl;
        float wl = windowsLit(vec2(lx, p.y - base - 0.6), vec2(1.1, 1.2), vec2(0.28, 0.22), id * 9.0, 0.35, gl);
        wl *= step(p.y, base + hgt - 0.2) * step(base + 0.2, p.y);
        wh += vec3(1.0, 0.6, 0.25) * wl * 0.7 * uLamp;
        c = lay(c, wh, fillA(d));
    }
    return c;
}

// ---- the fields: far ridges, hills, pylons
vec3 ridges(vec2 p, vec3 c) {
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        float k = 0.05 + 0.07 * fi;
        vec2 q = layerP(p, k);
        float u = q.x + uD * k + fi * 50.0;
        float dens = zNature(xFar(p.x));
        float hgt = GY0 + 1.5 - fi * 1.0 + dens * (3.0 + 3.0 * fbm(vec2(u * 0.05, fi)) + 1.0 * vnoise(vec2(u * 0.2, fi)));
        vec3 mc = mix(uFog * 0.95, mix(vec3(0.35, 0.45, 0.55), vec3(0.3, 0.42, 0.38), fi), 0.35 + 0.25 * fi);
        c = lay(c, mc, fillA(q.y - hgt) * smoothstep(0.0, 0.2, dens));
    }
    return c;
}
vec3 hills(vec2 p, vec3 c) {
    vec2 q = layerP(p, 0.25);
    float u = q.x + uD * 0.25;
    float dens = zNature(xFar(p.x));
    if (dens <= 0.01) return c;
    float hgt = GY0 + 0.6 + dens * (1.6 + 1.2 * sin(u * 0.11) + 0.6 * sin(u * 0.27 + 1.0));
    float pid = floor(u / 4.0 + floor((hgt - q.y) / 0.6) * 0.5);
    vec3 fc = mix(vec3(0.42, 0.55, 0.25), vec3(0.62, 0.6, 0.3), hash1(pid));
    fc = mix(fc, vec3(0.3, 0.48, 0.28), step(0.7, hash1(pid + 2.0)));
    fc *= 0.9 + 0.1 * smoothstep(hgt - 1.0, hgt, q.y);
    c = lay(c, haze(fc * (0.5 + 0.5 * uDawn), 0.3), fillA(q.y - hgt));
    float tid = floor(u / 3.0);
    float tx = mod(u, 3.0) - 1.5 + (hash1(tid) - 0.5);
    float tc = tid * 3.0 + 1.5;
    float th = GY0 + 0.6 + dens * (1.6 + 1.2 * sin(tc * 0.11) + 0.6 * sin(tc * 0.27 + 1.0));
    float tree = fillA(length(vec2(tx, (q.y - th - 0.45) * 1.2)) - 0.42) * step(hash1(tid + 7.0), 0.4 * dens);
    return lay(c, haze(vec3(0.16, 0.3, 0.16) * (0.5 + 0.5 * uDawn), 0.3), tree);
}
vec3 pylons(vec2 p, vec3 c) {
    vec2 q0 = layerP(p, 0.45);
    float u = q0.x + uD * 0.45;
    float sp = 18.0;
    float n = floor(u / sp + 0.5);
    float dens = max(zField(n * sp / 0.45), zInd(n * sp / 0.45) * 0.5);
    float dens2 = max(zField((n + 1.0) * sp / 0.45), zInd((n + 1.0) * sp / 0.45) * 0.5);
    float lx = u - n * sp;
    float base = GY0 + 1.0;
    vec2 q = vec2(lx, q0.y - base);
    vec3 pc = haze(vec3(0.3, 0.33, 0.36) * (0.5 + 0.5 * uDawn), 0.2);
    if (dens > 0.5) {
        float tw = 0.9 - q.y * 0.07;
        float d = max(abs(abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)) - 0.05, -q.y);
        d = max(d, q.y - 8.5);
        d = min(d, sdBox(q - vec2(0.0, 7.4), vec2(1.6, 0.06)));
        d = min(d, sdBox(q - vec2(0.0, 6.3), vec2(1.2, 0.06)));
        float lat = min(abs(fract(q.y * 0.9 + q.x * 0.6) - 0.5), abs(fract(q.y * 0.9 - q.x * 0.6) - 0.5)) / 0.9;
        d = min(d, max(lat - 0.03, abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)));
        d = max(max(d, -q.y), q.y - 8.6);
        c = lay(c, pc, fillA(d) * 0.9);
    }
    // the wires between this pylon and the next
    float fx = (lx + sp * 0.5) / sp;
    float nn = lx < 0.0 ? n - 1.0 : n;
    float da = max(zField(nn * sp / 0.45), zInd(nn * sp / 0.45) * 0.5), db = max(zField((nn + 1.0) * sp / 0.45), zInd((nn + 1.0) * sp / 0.45) * 0.5);
    if (da > 0.5 && db > 0.5) {
        float fx2 = fract(u / sp);
        for (int w = 0; w < 2; w++) {
            float wy = base + 7.35 - float(w) * 1.1 - 1.4 * 4.0 * fx2 * (1.0 - fx2);
            c = lay(c, pc * 0.8, (1.0 - smoothstep(0.012, 0.012 + gPx, abs(q0.y - wy))) * 0.7);
        }
    }
    return c;
}

// ---- bamboo (layer k, density by place)
vec3 bambooClump(vec2 p, vec3 c, float k, float base, float scale, float dens0, vec3 col, float hz, float seed) {
    float u = p.x + uD * k + seed * 17.0;
    float cw = 1.25 * scale;
    float id0 = floor(u / cw);
    for (int o = -1; o <= 1; o++) {
        float id = id0 + float(o);
        float fade = 1.0;
        float dens = dens0 * zBamboo(((id + 0.5) * cw - seed * 17.0) / k);
        if (k < 0.3) { fade = zBamboo(xFar((id + 0.5) * cw - seed * 17.0 - uD * k)); dens = dens0 * step(0.005, fade); }
        float h = hash1(id + seed);
        if (h > dens) continue;
        float x0 = (id + 0.5 + (hash1(id + seed + 1.0) - 0.5) * 0.6) * cw;
        float w = (0.13 + 0.1 * hash1(id + seed + 2.0)) * scale;
        float lean = (hash1(id + seed + 3.0) - 0.5) * 0.06;
        float yy = p.y - base;
        float sway = (0.15 * sin(uTime * 0.5 + id * 1.3) + 0.06 * sin(uTime * 1.1 + id)) * scale * sq(max(yy, 0.0) / 20.0);
        float x = u - x0 - lean * yy - sway;
        float d = max(abs(x) - w, base - p.y);
        float a = fillA(d);
        if (a > 0.0) {
            float seg = 1.6 * scale;
            float ny = fract((yy + hash1(id) * seg) / seg);
            float node = exp(-sq((ny - 0.5) * seg / (0.06 * scale + gPx)));
            vec3 cc = col * (0.75 + 0.45 * smoothstep(-w, w, -x));
            cc = mix(cc, col * 0.55, node * 0.7);
            c = mix(c, haze(cc, hz), a * fade);
        }
        for (int l = 0; l < 3; l++) {
            float fl = float(l);
            float ly = (6.0 + 5.0 * hash1(id + fl * 7.0 + seed)) * scale;
            float side = hash1(id + fl * 3.0) < 0.5 ? -1.0 : 1.0;
            vec2 at = vec2(x0 + lean * ly + sway * 0.8, base + ly);
            vec2 q = vec2(u, p.y) - at;
            q.x *= side;
            for (int b = 0; b < 3; b++) {
                float fb = float(b);
                vec2 r = rot(-0.35 - 0.35 * fb + 0.05 * sin(uTime * 0.9 + id + fb)) * q;
                float L = (1.3 + 0.4 * fb) * scale;
                float t = sat(r.x / L);
                float lw = 0.17 * scale * sin(t * PI) * (1.0 - t * 0.3);
                float ld = max(abs(r.y + 0.1 * scale * t * t) - lw, max(-r.x, r.x - L));
                c = mix(c, haze(col * (0.85 + 0.35 * fb / 2.0), hz), fillA(ld) * fade);
            }
        }
    }
    return c;
}

float grassTufts(vec2 w, float y0, float h, float dens, float seed) {
    float cw = 0.22;
    float id = floor(w.x / cw);
    float lx = (w.x / cw - id) * cw - cw * 0.5;
    float hh = h * (0.4 + 0.6 * hash1(id + seed)) * step(hash1(id * 1.7 + seed), dens);
    float t = sat((w.y - y0) / max(hh, 1e-3));
    float lean = (hash1(id + seed + 3.0) - 0.5) * 0.25 + 0.06 * sin(uTime * 1.2 + w.x * 0.3) * t;
    float d = abs(lx - lean * t * hh) - 0.05 * (1.0 - t);
    d = max(max(d, w.y - y0 - hh), y0 - w.y);
    return fillA(d);
}

vec3 catenary(vec2 w, vec3 c);
// =========================================================================== the air: everything above the ground
vec3 airScene(vec2 p, vec2 w) {
    vec3 c = sky(p);
    c = ridges(p, c);
    c = skyline(layerP(p, 0.12), c, 0.12, GY0 + 2.0, 4.0, 15.0, 2.9, mix(vec3(0.03, 0.04, 0.07), uFog * 0.7, uDawn * 0.6), 0.1, 3.0, 0.35);
    c = industry(layerP(p, 0.1), c, 0.1);
    c = hills(p, c);
    c = bambooClump(layerP(p, 0.12), c, 0.12, GY0 + 2.0, 0.55, 0.8, mix(vec3(0.3, 0.42, 0.2), uFog, 0.5), 0.55, 1.0);
    c = facades(layerP(p, 0.42), c, 0.42, GY0 + 1.0, 0.12);
    c = warehouses(layerP(p, 0.4), c, 0.4);
    c = pylons(p, c);
    c = bambooClump(layerP(p, 0.28), c, 0.28, GY0 + 1.5, 0.75, 0.75 * (1.0 - 0.5 * zGlade(w.x)), vec3(0.16, 0.27, 0.08), 0.3, 2.0);
    // shafts of light in the forest
    float shz = zBamboo(w.x) + 0.5 * zGlade(w.x);
    if (shz > 0.01) {
        vec2 sq0 = rot(-0.45) * (p - vec2(-30.0, 14.0));
        float sh = sq(sat(vnoise(vec2((sq0.x + uD * 0.15) * 0.35, uTime * 0.05)) * 1.4 - 0.45));
        c += uSunCol * sh * 0.5 * shz * smoothstep(-12.0, 4.0, p.y);
    }
    c = passers(layerP(p, 0.8), c, 0.8, vec3(1.0, 0.6, 0.5));
    c = bambooClump(layerP(p, 0.5), c, 0.5, GY0 + 1.0, 1.0, 0.55, vec3(0.08, 0.16, 0.04), 0.12, 5.0);
    // the meadow behind the path: grass and flowers (fields, forest, glade)
    {
        float nat = smoothstep(330.0, 350.0, w.x);
        if (nat > 0.01 && w.y < GY0 + 2.0) {
            float top = GY0 + 0.5 + 0.25 * sin(w.x * 0.4);
            vec3 mc = mix(vec3(0.3, 0.5, 0.18), vec3(0.45, 0.62, 0.22), vnoise(vec2(w.x * 0.3, w.y)));
            mc = mix(mc, vec3(0.3, 0.3, 0.12), zBamboo(w.x) * 0.6);
            mc = mix(mc, vec3(0.5, 0.42, 0.2), zGlade(w.x) * 0.5);
            mc *= (0.55 + 0.45 * uDawn);
            c = lay(c, mc, fillA(w.y - top) * nat);
            vec2 fq = vec2(w.x / 0.55, (w.y - GY0) / 0.5);
            vec2 fi = floor(fq), ff = fract(fq) - 0.5;
            vec2 fo = (hash2(fi) - 0.5) * 0.6;
            float on = step(0.72, hash(fi + 2.0)) * step(w.y, top - 0.1) * nat * (1.0 - 0.7 * zBamboo(w.x));
            vec3 flc = hash(fi + 5.0) < 0.5 ? vec3(1.0, 0.95, 0.85) : (hash(fi + 6.0) < 0.5 ? vec3(1.0, 0.8, 0.25) : vec3(1.0, 0.55, 0.7));
            c = lay(c, flc * (0.6 + 0.4 * uDawn), fillS(length(ff - fo) * 0.5 - 0.06, gPx) * on);
            c = lay(c, vec3(0.35, 0.55, 0.2) * (0.55 + 0.45 * uDawn), grassTufts(w, top - 0.15, 0.7 + 0.6 * zGlade(w.x), 0.55 * nat, 1.0));
        }
    }
    c = bambooClump(layerP(p, 0.8), c, 0.8, GY0 + 0.5, 1.35, 0.4 * (1.0 - 0.8 * zGlade(w.x)), vec3(0.035, 0.07, 0.02), 0.03, 9.0);
    // ---- on the near plane: lamps, the subway's entrance, the crossing's light, the station's canopy, bollards
    if (w.x < 160.0) {
        float sp = 15.0, n = floor((w.x - 4.0) / sp + 0.5);
        float lxw = n * sp + 4.0;
        float lx = w.x - lxw;
        if (abs(lxw - 79.0) > 6.0) c = lampAt(w, c, lx, vec3(1.0, 0.72, 0.38), 6.6, uLamp);
    } else if (w.x > 296.0 && w.x < 345.0) {
        float sp = 13.0, n = floor((w.x - 2.0) / sp + 0.5);
        c = lampAt(w, c, w.x - (n * sp + 2.0), vec3(1.0, 0.55, 0.2), 5.6, uLamp);
    }
    // the subway's entrance: railings round the stairwell, a lit sign on a post
    {
        vec2 q = w - vec2(79.0, GY0);
        float rail = min(sdBox(q - vec2(0.7, 1.0), vec2(0.06, 1.0)), sdBox(q - vec2(9.2, 1.0), vec2(0.06, 1.0)));
        rail = min(rail, sdBox(q - vec2(4.95, 1.95), vec2(4.25, 0.05)));
        rail = min(rail, max(abs(mod(q.x - 0.7, 1.7) - 0.85) - 0.03, max(abs(q.y - 1.0) - 0.95, abs(q.x - 4.95) - 4.25)));
        // the canopy: glass on a light frame, on two posts
        float cp = min(sdBox(q - vec2(0.3, 2.8), vec2(0.08, 2.8)), sdBox(q - vec2(9.4, 2.6), vec2(0.08, 2.6)));
        vec2 cq = q - vec2(4.85, 5.4 + (q.x - 4.85) * -0.04);
        float roofF = sdBox(cq, vec2(4.9, 0.1));
        c = mix(c, mix(c, vec3(0.5, 0.7, 0.8) * (0.2 + 0.3 * uLamp), 0.35), fillA(sdBox(cq - vec2(0.0, 0.25), vec2(4.8, 0.2))));
        rail = min(rail, min(cp, roofF));
        rail = min(rail, sdBox(q - vec2(-1.6, 2.4), vec2(0.07, 2.4)));
        c = lay(c, vec3(0.06, 0.07, 0.06) + vec3(0.1, 0.12, 0.1) * uLamp * 0.3, fillA(rail));
        vec2 sq0 = q - vec2(-1.6, 5.2);
        float sg = sdBox(sq0, vec2(0.65, 0.65)) - 0.1;
        c = lay(c, vec3(0.9, 0.95, 1.0) * (0.3 + 0.9 * uLamp), fillA(sg));
        // an M: two legs, a V between them
        float mx = abs(sq0.x);
        float m = min(abs(mx - 0.32) - 0.065, abs(mx - (0.3 - 0.75 * (sq0.y + 0.4)) * 0.5 - 0.0) - 0.06);
        m = max(m, abs(sq0.y) - 0.38);
        c = lay(c, vec3(0.05, 0.2, 0.45), fillA(m));
        c += vec3(0.7, 0.85, 1.0) * exp(-max(sg, 0.0) * 1.5) * 0.25 * uLamp;
    }
    // the crossing's pedestrian light
    {
        float lx = w.x - 58.0;
        float d = min(sdBox(vec2(lx, w.y - GY0 - 2.6), vec2(0.07, 2.6)), sdBox(vec2(lx, w.y - GY0 - 5.6), vec2(0.32, 0.65)) - 0.06);
        c = lay(c, vec3(0.025, 0.028, 0.035), fillA(d));
        float red = step(fract(uTime / 14.0), 0.55), grn = 1.0 - red;
        vec2 r1 = vec2(lx, w.y - GY0 - 5.9), r2 = vec2(lx, w.y - GY0 - 5.3);
        c += vec3(1.0, 0.18, 0.12) * red * (fillA(length(r1) - 0.17) * 1.5 + exp(-dot(r1, r1) / 0.4) * 0.6);
        c += vec3(0.2, 1.0, 0.6) * grn * (fillA(length(r2) - 0.17) * 1.5 + exp(-dot(r2, r2) / 0.4) * 0.6);
    }
    // the terminus: a canopy on posts over the platform, its lamps
    if (w.x > 250.0 && w.x < 312.0) {
        vec2 q = w - vec2(281.0, GY0);
        float roof = sdBox(q - vec2(0.0, 8.6), vec2(26.0, 0.18)) - 0.05;
        float px = mod(q.x + 26.0, 6.5) - 3.25;
        float post = max(abs(px) - 0.09, abs(q.y - 4.3) - 4.3) ;
        post = max(post, abs(q.x) - 26.0);
        c = lay(c, vec3(0.1, 0.09, 0.1) + uFog * 0.15, fillA(min(roof, post)));
        vec2 lq = vec2(px, q.y - 8.2);
        c += vec3(1.0, 0.85, 0.6) * (fillA(sdBox(lq, vec2(0.5, 0.06))) * 1.2 + exp(-dot(lq, lq) / 2.0) * 0.12) * step(abs(q.x), 25.0);
        // the station's name board
        vec2 nq = q - vec2(16.0, 5.8);
        c = lay(c, vec3(0.04, 0.08, 0.16), fillA(sdBox(nq, vec2(2.4, 0.5)) - 0.05));
        c = lay(c, vec3(0.85, 0.88, 0.95), fillA(sdBox(vec2(fract(nq.x * 0.9) - 0.5, nq.y), vec2(0.22, 0.24))) * step(abs(nq.x), 2.0));
    }
    c = catenary(w, c);
    // the terminus: the buffer stop at the end of the track, the way out to the street (a gate, a lit sign)
    {
        vec2 q = w - vec2(297.0, GY0);
        float bs = min(sdBox(q - vec2(0.0, 1.0), vec2(0.35, 0.35)), sdBox(q - vec2(0.0, 0.4), vec2(0.12, 0.45)));
        c = lay(c, vec3(0.5, 0.08, 0.06) * (0.4 + 0.6 * uDawn), fillA(bs));
        c = lay(c, vec3(0.9) * (0.4 + 0.6 * uDawn), fillA(sdBox(q - vec2(0.0, 1.0), vec2(0.35, 0.08))));
        c += vec3(1.0, 0.15, 0.1) * exp(-dot(q - vec2(0.0, 1.55), q - vec2(0.0, 1.55)) / 0.03) * 0.8;
        vec2 g = w - vec2(308.5, GY0);
        float gate = min(min(sdBox(g - vec2(-1.6, 2.6), vec2(0.1, 2.6)), sdBox(g - vec2(1.6, 2.6), vec2(0.1, 2.6))), sdBox(g - vec2(0.0, 5.1), vec2(1.75, 0.12)));
        c = lay(c, vec3(0.1, 0.1, 0.11) + uFog * 0.15, fillA(gate));
        vec2 sq1 = g - vec2(0.0, 5.75);
        float sgn = sdBox(sq1, vec2(1.1, 0.42)) - 0.05;
        c = lay(c, vec3(0.1, 0.55, 0.3) * (0.6 + 0.6 * uLamp), fillA(sgn));
        c = lay(c, vec3(0.95), fillA(sdBox(vec2(fract(sq1.x * 1.4 + 0.5) - 0.5, sq1.y), vec2(0.18, 0.2))) * step(abs(sq1.x), 0.8) * fillA(sgn));
        c += vec3(0.3, 1.0, 0.6) * exp(-max(sgn, 0.0) * 2.0) * 0.12 * uLamp;
    }
    // a fence and a few posts where the town gives way to the fields
    if (w.x > 330.0 && w.x < 362.0) {
        float fx = mod(w.x, 2.2) - 1.1;
        float f = min(sdBox(vec2(fx, w.y - GY0 - 0.8), vec2(0.06, 0.8)), sdBox(vec2(fx, w.y - GY0 - 1.2), vec2(1.1, 0.04)));
        c = lay(c, vec3(0.2, 0.17, 0.14) * (0.5 + 0.5 * uDawn), fillA(f) * (1.0 - smoothstep(352.0, 362.0, w.x)) * smoothstep(330.0, 336.0, w.x));
    }
    // the little bridge over the glade's stream: its rails
    if (w.x > 441.0 && w.x < 452.0) {
        vec2 q = w - vec2(446.5, GY0);
        float r = min(sdBox(q - vec2(0.0, 1.1), vec2(5.0, 0.07)), max(abs(mod(q.x + 5.0, 2.5) - 1.25) - 0.07, abs(q.y - 0.55) - 0.55));
        r = max(r, abs(q.x) - 5.05);
        c = lay(c, vec3(0.32, 0.2, 0.12) * (0.5 + 0.6 * uDawn), fillA(r));
    }
    return c;
}

// =========================================================================== underground
// The stairs: nine steps down from the pavement's edge (a step a quarter of the cub's height), the station's
// platform, the tunnels' track bed; then a gentle grade (under 5 degrees) out of a portal up a walled cutting.
const float STAIR_X = 80.0, TREAD = 1.3, NSTEPS = 9.0;
const float RISE = (GY0 - PLAT_Y) / NSTEPS;
const float STAIR_END = STAIR_X + TREAD * NSTEPS;
float stairLine(float x) { return GY0 - (x - STAIR_X) / TREAD * RISE; }   // through the steps' noses
float stepY(float x) { return GY0 - clamp(floor((x - STAIR_X) / TREAD) + 1.0, 0.0, NSTEPS) * RISE; }
// cavities cut in the earth: 1 the stairs, 2 the station's hall, 3 the tunnel on the left, 4 the tunnel on the right,
// 5 the cutting (open to the sky); floor and ceiling at w.x; the floor band (the solid in front, below the floor)
// reaches `fb` below the floor
int cavity(vec2 w, out float fl, out float ce) {
    fl = -1e3; ce = -1e3;
    if (w.x >= STAIR_X && w.x < STAIR_END + 0.4 && w.y < GY0) {
        fl = w.x < STAIR_X ? GY0 : stepY(w.x); ce = stairLine(w.x) + 5.0;
        if (w.y > stairLine(w.x) - RISE - 1.3 && w.y < ce) return 1;
    }
    if (w.x > 88.0 && w.x < 146.0) { fl = PLAT_Y; ce = PLAT_Y + 7.2; if (w.y > fl - 1.6 && w.y < ce) return 2; }
    if (w.x > 10.0 && w.x <= 88.0) { fl = PLAT_Y - 0.4; ce = PLAT_Y + 7.4; if (w.y > fl - 1.6 && w.y < ce) return 3; }
    if (w.x >= 146.0 && w.x < 155.0) { fl = trackY(w.x) - 0.4; ce = trackY(w.x) + 7.4; if (w.y > fl - 1.6 && w.y < ce) return 4; }
    if (w.x >= 155.0 && w.x < 262.0) { fl = trackY(w.x) - 0.4; ce = GY0; if (w.y > fl - 1.6 && w.y < GY0 + 0.01) return 5; }
    return 0;
}
// the concrete lining round the cavities (0.5 thick), the street's slab
float lining(vec2 w) {
    float a = 0.0;
    if (w.x > STAIR_X - 0.9 && w.x < STAIR_END + 0.9) {
        float sl = stairLine(w.x);
        a = max(a, step(sl - RISE - 1.8, w.y) * step(w.y, sl + 5.5));
    }
    if (w.x > 87.5 && w.x < 146.5) a = max(a, step(PLAT_Y - 2.1, w.y) * step(w.y, PLAT_Y + 7.7));
    if (w.x > 10.0 && w.x <= 88.0) a = max(a, step(PLAT_Y - 2.0, w.y) * step(w.y, PLAT_Y + 7.9));
    if (w.x >= 146.0 && w.x < 155.6) a = max(a, step(trackY(w.x) - 2.0, w.y) * step(w.y, trackY(w.x) + 7.9));
    a = max(a, step(GY0 - 2.3, w.y));   // the street's slab under the pavement
    return a;
}
vec3 concrete(vec2 w) {
    vec3 c = vec3(0.075, 0.074, 0.072) * (0.85 + 0.25 * vnoise(w * 3.0) + 0.1 * vnoise(w * 11.0));
    c *= 1.0 - 0.25 * (1.0 - smoothstep(0.0, 0.04 + gPx, abs(fract(w.x / 3.0) - 0.5) * 3.0 - 1.45)) * lod(3.0);
    return c * (0.8 + 0.4 * uDawn);
}
vec3 earth(vec2 w) {
    if (lining(w) > 0.5) return concrete(w);
    float depth = GY0 - w.y;
    float n = fbm(vec2(w.x * 0.08, w.y * 0.6 + fbm(vec2(w.x * 0.05, w.y * 0.1)) * 2.0));
    vec3 c = mix(vec3(0.05, 0.032, 0.024), vec3(0.085, 0.055, 0.035), n);
    // strata
    c *= 0.88 + 0.16 * smoothstep(0.35, 0.65, vnoise(vec2(w.x * 0.03, w.y * 0.9)));
    c = mix(c, vec3(0.03, 0.022, 0.02), smoothstep(4.0, 14.0, depth) * 0.5);
    vec2 pq = w / vec2(0.9, 0.6);
    vec2 pi = floor(pq), pf = fract(pq) - 0.5;
    float pb = fillS(length(pf - (hash2(pi) - 0.5) * 0.5) - 0.12 * hash(pi + 3.0), 0.03) * step(0.7, hash(pi + 1.0));
    c = mix(c, vec3(0.12, 0.1, 0.09), pb * 0.6 * lod(0.6));
    // a pipe and a cable under the street
    c = mix(c, vec3(0.1, 0.11, 0.1), fillA(abs(w.y - (GY0 - 3.1)) - 0.18) * step(w.x, 150.0));
    c = mix(c, vec3(0.06, 0.05, 0.05), fillA(abs(w.y - (GY0 - 3.7)) - 0.08) * step(w.x, 150.0));
    return c * (0.8 + 0.4 * uDawn);
}
vec3 tiles(vec2 w, vec3 tile) {
    vec2 tq = vec2(w.x / 0.9, w.y / 0.45);
    vec2 tf = abs(fract(tq + vec2(0.5 * step(0.5, fract(tq.y * 0.5)), 0.0)) - 0.5);
    float grout = 1.0 - smoothstep(0.43, 0.48, max(tf.x * 0.5, tf.y));
    return tile * mix(0.93, mix(0.82, 1.0, grout), lod(0.45)) * (0.9 + 0.1 * vnoise(tq * vec2(1.0, 0.5)));
}
// overhead line posts along the track out of the tunnel (a post every 12, its arm, the wire)
vec3 catenary(vec2 w, vec3 c) {
    if (w.x < 155.0 || w.x > 300.0) return c;
    float ty = trackY(w.x);
    float px = mod(w.x - 157.0, 12.0) - 6.0;
    float pole = max(abs(px) - 0.09, max(ty - 0.4 - w.y, w.y - (ty + 8.2)));
    float arm = max(abs(w.y - (ty + 7.75)) - 0.05, max(px - 0.0, -px - 1.6));
    vec3 pc = vec3(0.12, 0.12, 0.13) * (0.5 + 0.8 * uDawn);
    c = mix(c, pc, fillA(min(pole, arm)));
    float wire = abs(w.y - (ty + 7.35 + 0.18 * sq((px) / 6.0))) - 0.02;
    c = mix(c, pc * 0.8, (1.0 - smoothstep(0.0, gPx * 1.5, wire)) * 0.8);
    return c;
}
vec3 interior(int cv, vec2 w, float fl, float ce) {
    float L = uSubLight;
    vec3 lc = vec3(0.85, 1.0, 0.92);
    if (cv == 1) {
        // the stairwell: tiles, a darker dado along the steps, a handrail on posts, lamps under the ceiling
        float sl = stairLine(w.x);
        vec3 c = tiles(w, vec3(0.075, 0.085, 0.08));
        float yy = w.y - sl;
        c = mix(c, vec3(0.03, 0.06, 0.05), smoothstep(1.1, 1.0, yy));
        c = mix(c, vec3(0.02, 0.07, 0.06), fillA(abs(yy - 3.2) - 0.25));
        c *= 0.6 + 0.5 * smoothstep(-1.0, 4.0, yy);
        float rail = abs(yy - 2.0) - 0.07;
        float post = max(abs(mod(w.x - STAIR_X - 0.65, 2.6) - 1.3) - 0.06, max(yy - 2.0, 0.2 - (w.y - fl)));
        c = mix(c, vec3(0.32, 0.34, 0.33), fillA(min(rail, post)));
        c += vec3(0.5) * exp(-sq((yy - 2.07) / 0.03)) * 0.15;
        float lx = mod(w.x - STAIR_X - 2.0, 4.5) - 2.25;
        vec2 lq = vec2(lx, w.y - (ce - 0.35));
        c = mix(c, vec3(1.0, 0.92, 0.75) * 1.4, fillA(length(lq * vec2(1.0, 1.6)) - 0.16) * L);
        c += vec3(1.0, 0.85, 0.6) * exp(-dot(lq, lq) / 3.0) * 0.1 * L;
        c = mix(c, vec3(0.02), smoothstep(ce - 0.4, ce, w.y));
        return c * (0.35 + 0.65 * L);
    }
    if (cv == 2) {
        // the hall: tiles, a band of color, the station's name, posters, strip lights, the track bed below the wall
        vec3 c = tiles(w, vec3(0.13, 0.16, 0.15));
        float yy = w.y - PLAT_Y;
        c = mix(c, vec3(0.02, 0.07, 0.06), fillA(abs(yy - 3.9) - 0.45));
        float nx = mod(w.x - 92.0, 26.0) - 13.0;
        float nameB = fillA(sdBox(vec2(nx, yy - 5.4), vec2(3.0, 0.55)) - 0.05);
        c = mix(c, vec3(0.03, 0.05, 0.12), nameB);
        c = mix(c, vec3(0.85, 0.88, 0.95), fillA(sdBox(vec2(fract(nx * 0.9) - 0.5, yy - 5.4), vec2(0.24, 0.26))) * step(abs(nx), 2.4) * nameB * 0.9);
        float px = mod(w.x - 92.0 + 6.5, 13.0) - 6.5;
        vec2 pq = vec2(px, yy - 2.1);
        float pid = floor((w.x - 92.0 + 6.5) / 13.0);
        vec3 pc1 = 0.5 + 0.5 * cos(TAU * (hash1(pid) + vec3(0.0, 0.33, 0.67)));
        vec3 pic = mix(pc1 * 0.5, (0.5 + 0.5 * cos(TAU * (hash1(pid + 3.0) + vec3(0.1, 0.4, 0.7)))) * 0.7, smoothstep(-1.0, 1.0, pq.y + sin(pq.x * 2.0 + pid) * 0.5));
        pic += 0.25 * fillA(length(pq - vec2(0.3 * sin(pid), 0.4)) - 0.55);
        c = mix(c, vec3(0.05), fillA(sdBox(pq, vec2(1.15, 1.35))));
        c = mix(c, pic * 0.35, fillA(sdBox(pq, vec2(1.0, 1.2))));
        float lx = mod(w.x, 8.0) - 4.0;
        float tube = fillA(sdBox(vec2(lx, w.y - (ce - 0.45)), vec2(2.2, 0.11)));
        c *= 0.55 + 0.6 * exp(-sq(lx / 3.5)) * smoothstep(PLAT_Y, ce, w.y);
        c = mix(c, vec3(0.02, 0.025, 0.025), smoothstep(ce - 0.8, ce - 0.75, w.y));
        c = mix(c, lc * 1.8, tube);
        c += lc * exp(-sq(sdBox(vec2(lx, w.y - (ce - 0.45)), vec2(2.2, 0.0)) / 0.5)) * 0.12;
        c = mix(c, vec3(0.015, 0.016, 0.018), smoothstep(0.9, 0.7, yy));
        return c * (0.3 + 0.7 * L);
    }
    if (cv == 5) {
        // the cutting: the tunnel's portal; along the far side a retaining wall, a grassy bank above it; the posts
        float depth = GY0 - fl;
        float wallTop = fl + min(3.2, depth * 0.55);
        float bankT = sat((w.y - wallTop) / max(GY0 - wallTop, 0.01));
        vec3 bank = mix(vec3(0.09, 0.07, 0.045), vec3(0.16, 0.2, 0.08), smoothstep(0.2, 0.9, bankT));
        bank *= 0.85 + 0.25 * vnoise(w * vec2(1.5, 3.0));
        float tuft = step(0.6, vnoise(vec2(w.x * 4.0, 0.0))) * smoothstep(GY0 - 0.35, GY0 - 0.1, w.y);
        bank = mix(bank, vec3(0.22, 0.32, 0.1), tuft);
        vec3 c = bank;
        vec3 rw = concrete(w) * 1.5;
        rw *= 1.0 - 0.3 * (1.0 - smoothstep(0.0, 0.03 + gPx, abs(fract(w.x / 2.4) - 0.5) * 2.4 - 1.15));
        rw = mix(rw, rw * 1.3, smoothstep(wallTop - 0.25, wallTop - 0.05, w.y));
        c = mix(c, rw, 1.0 - smoothstep(wallTop - gPx, wallTop + gPx, w.y));
        c *= 0.55 + 0.8 * uDawn;
        // the portal: a concrete face with the tunnel's arch, where the cutting begins
        float pf = max(abs(w.x - 155.6) - 0.6, w.y - GY0);
        vec3 pcc = concrete(w) * (1.2 + 0.6 * uDawn);
        float arch = abs(length(vec2(w.x - 155.0, (w.y - fl - 3.2) * 0.6)) - 3.3) - 0.12;
        pcc = mix(pcc, pcc * 0.6, fillA(arch));
        c = mix(c, pcc, fillA(pf));
        return catenary(w, c);
    }
    // tunnels: dark concrete, a lamp every few meters, cables
    vec3 c = mix(vec3(0.03, 0.03, 0.032), vec3(0.018, 0.018, 0.02), sat((w.y - fl) / 7.0));
    float lx = mod(w.x, 9.0) - 4.5;
    vec2 lq = vec2(lx, w.y - fl - 5.4);
    c += vec3(1.0, 0.75, 0.45) * (exp(-dot(lq, lq) / 0.08) * 1.4 + exp(-length(lq) / 1.5) * 0.06);
    c = mix(c, vec3(0.06), fillA(abs(w.y - fl - 4.0 - 0.06 * sin(w.x * 0.5)) - 0.03) * 0.5);
    c = mix(c, vec3(0.05), fillA(abs(w.y - fl - 3.6 - 0.06 * sin(w.x * 0.45 + 1.0)) - 0.025) * 0.5);
    return c;
}
// the floor bands (the solids seen in front, below the floors), in front of the train
vec3 floorBand(int cv, vec2 w, float fl, vec3 base, out float a) {
    a = 0.0;
    float my = fl - w.y;
    if (cv == 1) {
        // the flight of steps in section: concrete under the stepped profile, each tread's nose catching the light
        if (w.y >= fl) return base;
        a = 1.0;
        float sl = stairLine(w.x);
        vec3 c = concrete(w) * 1.6;
        c *= 1.0 - 0.35 * smoothstep(0.0, 1.6, my);
        c = mix(c, vec3(0.32, 0.31, 0.29) * (0.5 + 0.5 * uSubLight), 1.0 - smoothstep(0.1, 0.16, my));   // the tread
        float nose = fract((w.x - STAIR_X) / TREAD);
        c = mix(c, vec3(0.62, 0.5, 0.14), (1.0 - smoothstep(0.08, 0.12, nose * TREAD)) * (1.0 - smoothstep(0.1, 0.16, my)));
        // the section's outline: along the steps, the stringer's underside
        float e = min(my, abs(w.y - (sl - RISE - 1.3)));
        float riser = abs(nose * TREAD) ;
        e = min(e, max(riser, my - RISE));
        c = mix(c, vec3(0.015), (1.0 - smoothstep(gPx * 0.8, gPx * 2.0, e)) * 0.8);
        if (w.x < STAIR_X) { a = 0.0; }
        return c * (0.35 + 0.65 * uSubLight);
    }
    if (my < 0.0 || my > 1.6) return base;
    a = 1.0;
    if (cv == 2) {
        vec3 c = mix(vec3(0.07, 0.075, 0.075), vec3(0.03, 0.032, 0.033), sat(my / 1.6));
        c += vec3(0.85, 1.0, 0.92) * exp(-sq(mod(w.x, 8.0) - 4.0) / 2.0) * exp(-my * 0.6) * 0.1;
        c = mix(c, vec3(0.45, 0.33, 0.04), fillA(abs(my - 0.2) - 0.1) * 0.9);
        return c * (0.3 + 0.7 * uSubLight);
    }
    // track beds: ballast, sleepers
    vec3 c = mix(vec3(0.04, 0.035, 0.03), vec3(0.015, 0.014, 0.013), sat(my / 1.6));
    c *= 0.85 + 0.3 * vnoise(w * 4.0);
    c = mix(c, c * 0.6, (1.0 - smoothstep(0.0, gPx * 2.0, abs(fract(w.x / 1.2) - 0.5) * 1.2 - 0.12)) * step(my, 0.5));
    if (cv == 5) c *= 0.6 + 0.9 * uDawn;
    return c;
}
// the surface band: pavement, the terminus's platform, the towpath, the path through the fields, moss, the glade
vec3 surfaceBand(vec2 w, vec2 p) {
    float my = GY0 - w.y;
    float x = w.x;
    // the wet pavement (the street, the outskirts)
    vec3 pv = mix(vec3(0.05, 0.055, 0.07), vec3(0.025, 0.028, 0.038), sat(my / 1.8));
    pv = mix(pv, pv * 2.0 + uFog * 0.1, uDawn * 0.6);
    float curb = fillA(abs(my - 0.06) - 0.06);
    float pud = smoothstep(0.55, 0.62, fbm(vec2(x * 0.25, my * 1.4)));
    float lxw = x - (floor((x - 4.0) / 15.0 + 0.5) * 15.0 + 4.0) - 0.7;
    float refl = lampReflectAt(lxw + 0.04 * sin(w.y * 9.0 + uTime * 2.3), my) * (0.35 + 0.65 * pud) * uLamp * step(x, 160.0);
    pv += vec3(1.0, 0.72, 0.4) * refl;
    pv += mix(uSkyHor, uSkyTop, 0.3) * pud * 0.4;
    pv += vec3(0.6, 0.7, 0.8) * puddleRings(w) * smoothstep(0.62, 0.7, fbm(vec2(x * 0.25, my * 1.4))) * 0.12 * uRain;
    pv = mix(pv, vec3(0.09, 0.09, 0.11) * (1.0 + uDawn), curb);
    vec3 c = pv;
    // the terminus's platform: concrete, a yellow line along its edge
    float term = smoothstep(252.0, 256.0, x) * (1.0 - smoothstep(306.0, 310.0, x));
    vec3 pl = mix(vec3(0.2, 0.19, 0.19), vec3(0.1, 0.095, 0.1), sat(my / 1.6)) * (0.5 + 0.6 * uDawn);
    pl = mix(pl, vec3(0.6, 0.45, 0.06) * (0.5 + 0.5 * uDawn), fillA(abs(my - 0.2) - 0.1));
    c = mix(c, pl, term);
    // the towpath: cobbles, the canal under its edge
    float tow = smoothstep(306.0, 312.0, x) * (1.0 - smoothstep(334.0, 340.0, x));
    vec3 path = mix(vec3(0.16, 0.12, 0.12), vec3(0.09, 0.07, 0.08), sat(my / 1.2)) * (0.6 + 0.6 * uDawn);
    path *= 1.0 - lod(0.6) * (1.0 - smoothstep(0.32, 0.5, length(fract(vec2(x / 0.6, my / 0.35)) - 0.5))) * 0.15;
    c = mix(c, path, tow);
    // the dirt path through the fields, moss in the forest, grass in the glade
    vec3 dirt = mix(vec3(0.6, 0.48, 0.32), vec3(0.42, 0.33, 0.22), sat(my / 1.6)) * (0.92 + 0.12 * vnoise(vec2(x * 1.5, my * 4.0)));
    vec3 moss = mix(vec3(0.3, 0.22, 0.1), vec3(0.14, 0.2, 0.05), sat(my / 1.6)) * (0.85 + 0.25 * vnoise(vec2(x * 1.2, my * 3.0)));
    vec3 grass = mix(vec3(0.45, 0.42, 0.16), vec3(0.28, 0.32, 0.1), sat(my / 1.6)) * (0.85 + 0.25 * vnoise(vec2(x * 1.2, my * 3.0)));
    vec3 nat = mix(dirt, moss, smoothstep(390.0, 405.0, x));
    nat = mix(nat, grass, smoothstep(436.0, 448.0, x));
    vec2 lq = vec2(x / 0.45, my / 0.25);
    vec2 li = floor(lq), lf = fract(lq) - 0.5;
    float leaf = fillS(sdEll(rot(hash(li) * 6.0) * lf, vec2(0.38, 0.12)), 0.08) * step(0.8, hash(li + 1.0)) * lod(0.25) * smoothstep(392.0, 405.0, x);
    nat = mix(nat, mix(vec3(0.75, 0.6, 0.25), vec3(0.55, 0.45, 0.2), hash(li + 2.0)), leaf * 0.6);
    c = mix(c, nat * (0.55 + 0.45 * uDawn), smoothstep(336.0, 344.0, x));
    // the little bridge over the stream: planks
    float br = smoothstep(441.0, 441.5, x) * (1.0 - smoothstep(451.5, 452.0, x));
    vec3 pk = vec3(0.4, 0.26, 0.15) * (0.85 + 0.15 * step(0.5, fract(x / 0.5))) * (0.5 + 0.6 * uDawn);
    c = mix(c, pk, br * step(my, 0.5));
    return c;
}

void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    gPx = uPxP;
    vec2 w = p + vec2(uD, uCamY);
    float fl, ce;
    int cv = cavity(w, fl, ce);
    vec3 c;
    bool air = false;
    vec3 bandC = vec3(0.0);
    float bandA = 0.0;
    if (cv > 0 && w.y >= fl) {
        c = interior(cv, w, fl, ce);
    } else if (cv > 0) {
        bandC = floorBand(cv, w, fl, vec3(0.0), bandA);
        c = bandC;
    } else if (w.y >= GY0) {
        c = airScene(p, w);
        air = true;
    } else if (w.y >= GY0 - 1.8) {
        bandC = surfaceBand(w, p);
        bandA = 1.0;
        c = bandC;
        air = true; // (rain falls on it)
    } else {
        c = earth(w);
        // the canal's water under the towpath, the stream under the bridge
        float can = step(310.0, w.x) * step(w.x, 336.0) * step(GY0 - 4.2, w.y);
        float strm = step(442.0, w.x) * step(w.x, 451.0) * step(GY0 - 3.4, w.y);
        if (can + strm > 0.0) {
            float wy = GY0 - 1.8 - w.y;
            vec3 water = mix(uSkyHor, uSkyTop, sat(wy / 3.0)) * 0.7;
            water *= 0.85 + 0.3 * vnoise(vec2(w.x * 0.8, wy * 6.0 - uTime * 0.6));
            water = mix(water, vec3(0.03, 0.04, 0.05), sat(wy / 2.5) * 0.5);
            c = water;
        }
    }
    // the head lamps light the walls and the floor ahead of the train (softly)
    if (uHead > 0.0 && (cv > 0 || !air)) {
        vec3 g = headGlow(w) * uHead;
        c += c * g * 2.2 + g * 0.012;
        if (bandA > 0.0) bandC += bandC * g * 2.2 + g * 0.012;
    }
    // the train, then the floor bands in front of it
    vec4 tr = trainInside(w, c);
    c = mix(c, tr.rgb, tr.a);
    if (bandA > 0.0) c = mix(c, bandC, bandA);
    // the cut's edges: a thin dark line where the earth meets the cavities and the surface
    {
        float e = abs(w.y - (GY0 - 1.8));
        if (cv > 0) e = min(e, min(abs(w.y - ce), abs(w.y - (fl - 1.6))));
        if (!(cv == 0 && w.y > GY0 - 1.8 && w.y < GY0)) c = mix(c, vec3(0.015, 0.01, 0.01), (1.0 - smoothstep(gPx * 0.8, gPx * 2.0, e)) * 0.7);
    }
    if (air) c = rain(p, c, uRain);
    if (air && w.x < 215.0) c = bokeh(p, c, uRain * 0.9 + uDrop * 0.3, vec3(1.0, 0.7, 0.4), vec3(0.5, 0.7, 1.0));
    c += uSunCol * uDrop * 0.05 * smoothstep(4.0, -10.0, p.y);
    c = mix(c, c * vec3(0.3, 0.32, 0.4), uOver * 0.75);
    c = mix(uBridgeCol, c, uWorldA);
    frag = vec4(max(c, 0.0), 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ the train's near side
// The train's low near-side wall and its doors, drawn over the cub (who stands inside, behind it); full resolution.
inline const char* FRONT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform float uD, uCamY, uPxP, uWorldA, uOver;
uniform vec3 uBridgeCol;
float gPx;
float fillA(float d) { return 1.0 - smoothstep(-gPx, gPx, d); }
//@TRAIN
void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    gPx = uPxP;
    vec2 w = p + vec2(uD, uCamY);
    vec4 f = trainFront(w);
    f.rgb = mix(f.rgb, f.rgb * vec3(0.3, 0.32, 0.4), uOver * 0.75);
    f.rgb = mix(uBridgeCol, f.rgb, uWorldA);
    frag = vec4(f.rgb * f.a, f.a);
}
)";

// ------------------------------------------------------------------------------------------------ composite
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec2 uTexel;
void main() {
    vec3 c = texture(uWorld, vUv).rgb;
    vec3 n = texture(uWorld, vUv + vec2(uTexel.x, 0.0)).rgb + texture(uWorld, vUv - vec2(uTexel.x, 0.0)).rgb +
             texture(uWorld, vUv + vec2(0.0, uTexel.y)).rgb + texture(uWorld, vUv - vec2(0.0, uTexel.y)).rgb;
    c = max(c + (c - n * 0.25) * 0.2, 0.0);
    frag = vec4(c, 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ the pandas
// A kawaii panda, a sticker mascot seen from the front: few shapes, flat fills (one soft step of shade), one uniform
// outline: a round head (half its height), a bean of a body, little limbs, round ears, oval patches leaning outward
// with a dark eye and a single dot of light, a tiny nose, a minimal mouth (a w when happy), light blush. It waddles
// (rocking from foot to foot, a little bounce). Local units: its height is 1, feet at the origin, facing +x. One
// quad per panda; the reflection in a wet ground is the same panda mirrored under its feet.
inline const char* PANDA_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uVP;
uniform vec4 uRect; // x0, y0, x1, y1 (board units)
out vec2 vP;
void main() {
    vec2 p = mix(uRect.xy, uRect.zw, aPos * 0.5 + 0.5);
    vP = p;
    gl_Position = uVP * vec4(p, 0.0, 1.0);
}
)";
inline const char* PANDA_FS = R"(#version 330 core
in vec2 vP;
out vec4 frag;
uniform vec2 uPos;           // feet (board)
uniform float uH, uFace, uPx, uTime;
uniform float uAdult;        // 0 the cub's proportions .. 1 a grown panda's
uniform float uWalk, uPhase, uMood, uBreath, uTurn, uTilt, uBlink, uSit, uWipe, uTear, uHop, uLean, uArms, uJoyEyes;
uniform vec2 uLook;
uniform vec2 uEars;          // the ears' extra lift (secondary motion)
uniform vec3 uAmb, uKey, uRimC;
uniform vec2 uKeyDir;
uniform float uReflect, uShadow, uAlpha, uHaze;
uniform vec3 uHazeC;
uniform float uHug;
uniform int uPart;
uniform float uMunch, uChew, uStrength;   // munching a bamboo snack; strength gained on the way
uniform float uRoll;                      // the floor's tilt under it (a climbing car)

float gAA, gLW;
vec3 gCol;
float gA;
const vec3 INK = vec3(0.045, 0.035, 0.045);
const vec3 BLACK = vec3(0.06, 0.055, 0.065);
const vec3 WHITE = vec3(0.95, 0.93, 0.89);

float fillP(float d) { return 1.0 - smoothstep(-gAA, gAA, d); }
// a sticker's part: a uniform outline, then a flat fill
void paint(float d, vec3 c) {
    float o = fillP(d - gLW);
    gCol = mix(gCol, INK, o);
    gA = max(gA, o);
    gCol = mix(gCol, c, fillP(d));
}
// a flat color under the scene's light, with one soft step of shade on the side away from the light
vec3 tone(vec3 base, vec2 q, vec2 ctr, float r) {
    vec2 n = (q - ctr) / r;
    float l = dot(n, uKeyDir * vec2(uFace, 1.0));
    vec3 c = base * (uAmb + uKey * 0.85);
    return mix(c * 0.84, c, smoothstep(-0.5, -0.3, l));
}
float capsule(vec2 q, vec2 a, vec2 b, float r) { return sdSeg(q, a, b) - r; }

vec4 panda(vec2 q) {
    gCol = vec3(0.0); gA = 0.0;
    float A = uAdult, walk = uWalk, ph = uPhase, sit = uSit, mood = uMood;
    float sad = (1.0 - mood) * (1.0 - 0.6 * uStrength);   // fed on the way, it stands straighter
    q.y -= uHop;
    // the waddle: rocking from foot to foot about the feet, a little bounce
    float rock = sin(ph) * 0.08 * walk + uLean * 0.06 + uRoll;
    vec2 qr = rot(-rock) * q;
    qr.y -= walk * 0.028 * (1.0 + 0.8 * uStrength) * abs(cos(ph)) + 0.005 * sin(uBreath);
    // a round head (half the height), a bean of a body, little limbs
    vec2 Br = vec2(mix(0.23, 0.255, A) * (1.0 + 0.07 * uStrength), mix(0.2, 0.23, A) * (1.0 + 0.012 * sin(uBreath))) * mix(vec2(1.0), vec2(1.06, 0.92), sit);
    vec2 B = vec2(0.0, mix(0.07 + Br.y, 0.035 + Br.y, sit));
    float headR = mix(0.3, 0.27, A);
    vec2 Hc = B + vec2(0.0, Br.y + headR * 0.62 - sad * 0.02);
    float tilt = uTilt - sad * 0.04;
    vec2 qh = rot(tilt) * (qr - Hc);
    float turn = uTurn;
    // feet, arms
    float lift = 0.035 * walk;
    vec2 fN = mix(vec2(0.12, 0.055 + max(0.0, -sin(ph)) * lift), vec2(0.15, 0.055), sit);
    vec2 fF = mix(vec2(-0.12, 0.055 + max(0.0, sin(ph)) * lift), vec2(-0.15, 0.055), sit);
    vec2 fr = vec2(0.085, 0.062);
    float armR = mix(0.062, 0.07, A);
    vec2 shN = B + vec2(Br.x * 0.85, Br.y * 0.15), shF = B + vec2(-Br.x * 0.85, Br.y * 0.15);
    float swing = sin(ph) * 0.3 * walk;
    vec2 hN = shN + rot(swing) * vec2(0.06, -0.11), hF = shF + rot(swing) * vec2(-0.06, -0.11);
    vec2 eyeN = Hc + rot(-tilt) * vec2(0.11 + 0.07 * turn, 0.0);
    hN = mix(hN, eyeN + vec2(0.035 + 0.01 * sin(uBreath * 7.0), -0.05), uWipe);
    hN = mix(hN, shN + vec2(0.1, 0.14), uArms); hF = mix(hF, shF + vec2(-0.1, 0.14), uArms);
    hN = mix(hN, shN + vec2(0.14, 0.02), uHug);
    // munching: both paws at the mouth, holding the snack
    vec2 mouthP = Hc + rot(-tilt) * vec2(0.06 * turn, -0.14);
    hN = mix(hN, mouthP + vec2(0.06, -0.035), uMunch); hF = mix(hF, mouthP + vec2(-0.06, -0.035), uMunch);
    float ear = mix(0.1, 0.085, A);
    float earDrop = sad * 0.06;
    vec2 eF = Hc + rot(-tilt) * vec2(-headR * 0.74 - 0.025 * turn - earDrop * 0.6, headR * 0.7 - earDrop + uEars.y * 0.05);
    vec2 eN = Hc + rot(-tilt) * vec2(headR * 0.74 - 0.025 * turn + earDrop * 0.6, headR * 0.7 - earDrop + uEars.x * 0.05);
    bool armOver = uWipe > 0.3 || uMunch > 0.3;
    bool farOver = uMunch > 0.3;

    // back to front: far arm, ears, feet, body, near arm, head, face
    if (!farOver) paint(capsule(qr, shF, hF, armR), tone(BLACK, qr, hF, 0.1));
    paint(length(qr - eF) - ear, tone(BLACK, qr, eF, ear));
    paint(length(qr - eN) - ear, tone(BLACK, qr, eN, ear));
    paint(sdEll(qr - fF, fr), tone(BLACK, qr, fF, 0.1));
    paint(sdEll(qr - fN, fr), tone(BLACK, qr, fN, 0.1));
    paint(sdEll(qr - B, Br), tone(WHITE, qr, B, Br.x));
    if (!armOver) paint(capsule(qr, shN, hN, armR), tone(BLACK, qr, hN, 0.1));
    float puff = uMunch * (0.6 + 0.4 * sin(uChew));
    float headD = sdEll(qh, vec2(headR * 1.07 * (1.0 + 0.035 * puff), headR * 0.95));
    paint(headD, tone(WHITE, qh, vec2(0.0), headR));

    // the face, set low on the head: oval patches leaning outward (more when sad), pure black; in each a glossy
    // black eye, large, low and to the inner side, its shine (a big dot of light above, a small one below, the same
    // side on both eyes) what makes it read; a soft blink to a curved line; ^ ^ in joy
    vec2 fc = vec2(0.06 * turn, -0.06);
    float sp = mix(0.13, 0.11, abs(turn)) * mix(1.0, 0.92, A);
    float lean = mix(0.55, 0.3, mood);
    vec2 pNc = fc + vec2(sp, 0.025), pFc = fc + vec2(-sp, 0.025);
    vec2 pr = vec2(0.064, 0.082) * mix(1.0, 0.88, A);
    float pN = sdEll(rot(-lean) * (qh - pNc - vec2(0.008, -0.008)), pr);
    float pF = sdEll(rot(lean) * (qh - pFc - vec2(-0.008, -0.008)), pr);
    vec3 patchC = BLACK * (uAmb + uKey * 0.85);
    gCol = mix(gCol, patchC, fillP(min(pN, pF)));
    float open = 1.0 - max(uBlink, 0.0);
    float esz = mix(0.92, 1.0, mood) * mix(1.0, 0.9, A);
    for (int i = 0; i < 2; i++) {
        float inner = i == 0 ? -1.0 : 1.0;
        vec2 ec = (i == 0 ? pNc : pFc) + vec2(0.007 * inner, -0.012) + uLook * vec2(0.003, 0.002);
        vec2 eq = (qh - ec) / esz;
        float k = 1.0 - uJoyEyes;
        // open: a glossy black bean, a faint dark-grey edge apart from the patch
        float o = smoothstep(0.2, 0.45, open);
        float eye = sdEll(eq, vec2(0.04, 0.047 * max(open, 0.15)));
        float ea = fillP(eye * esz) * k * o;
        gCol = mix(gCol, vec3(0.11, 0.105, 0.12) * (uAmb + uKey * 0.85), fillP(eye * esz - gLW * 0.5) * k * o);
        gCol = mix(gCol, vec3(0.012, 0.01, 0.014), ea);
        float big = length(eq - vec2(-0.013, 0.017)) - mix(0.016, 0.0135, mood);
        float small = length(eq - vec2(0.014, -0.017)) - 0.0065;
        gCol = mix(gCol, vec3(1.0), fillP(min(big, small) * esz) * ea * smoothstep(0.45, 0.7, open));
        // closed (blinking): a soft curved line
        float line = abs(eq.y + 0.004 - 9.0 * eq.x * eq.x) - 0.0045;
        line = max(line, abs(eq.x) - 0.036);
        gCol = mix(gCol, vec3(0.5, 0.48, 0.5) * (uAmb + uKey * 0.85), fillP(line * esz) * k * (1.0 - o));
        // joy: ^ ^
        float arc = abs(length((eq + vec2(0.0, 0.02)) * vec2(1.0, 1.15)) - 0.03) - 0.0075;
        arc = max(arc, -eq.y);
        gCol = mix(gCol, WHITE * (uAmb + uKey * 0.85), fillP(arc * esz) * uJoyEyes);
    }
    // a single tear
    if (uTear > 0.01) {
        vec2 tq = qh - pNc - vec2(0.03, -0.09 - 0.05 * (1.0 - uTear));
        float drop = min(length(tq) - 0.015, sdEll(tq - vec2(0.0, 0.014), vec2(0.008, 0.018)));
        gCol = mix(gCol, vec3(0.6, 0.82, 1.0) * (uAmb + uKey), fillP(drop) * smoothstep(0.0, 0.4, uTear));
    }
    // blush
    float ck = min(sdEll(qh - fc - vec2(sp + 0.055, -0.085), vec2(0.04, 0.022)), sdEll(qh - fc - vec2(-sp - 0.055, -0.085), vec2(0.04, 0.022)));
    gCol = mix(gCol, vec3(1.0, 0.6, 0.62) * (uAmb + uKey * 0.85), fillP(ck - 0.01 * puff) * (0.35 + 0.3 * mood + 0.2 * puff) * fillP(headD + 0.01));
    // nose
    vec2 nq = qh - fc - vec2(0.0, -0.06);
    gCol = mix(gCol, BLACK, fillP(sdEll(nq, vec2(0.022, 0.015))));
    // mouth: a small w when happy, a small frown when sad (open in joy)
    vec2 mq = qh - fc - vec2(0.0, -0.09);
    float happyM = max(smoothstep(0.35, 0.8, mood), uMunch);
    mq.y += uMunch * 0.004 * sin(uChew);
    float wArc = abs(length(vec2(abs(mq.x) - 0.013, mq.y - 0.003)) - 0.013) - 0.0035;
    wArc = max(wArc, mq.y - 0.003);
    float frown = abs(length(vec2(mq.x, mq.y + 0.026)) - 0.02) - 0.0035;
    frown = max(frown, -(mq.y + 0.01));
    float mouth = mix(frown, wArc, happyM);
    float openM = smoothstep(0.85, 1.0, mood) * uJoyEyes;
    if (openM > 0.02) mouth = min(mouth, sdEll(mq - vec2(0.0, -0.01), vec2(0.014, 0.014 * openM)));
    gCol = mix(gCol, INK, fillP(mouth));
    // the snack in its paws, shrinking as it is eaten
    if (uMunch > 0.05) {
        float len = 0.075 * (0.4 + 0.6 * uMunch);
        vec2 sc = mouthP + vec2(0.0, -0.03);
        float st = capsule(qr, sc - vec2(len, 0.0), sc + vec2(len, 0.0), 0.02);
        vec3 stc = mix(vec3(0.35, 0.55, 0.16), vec3(0.7, 0.85, 0.36), smoothstep(-0.02, 0.015, qr.y - sc.y)) * (uAmb + uKey * 0.85);
        stc = mix(stc, vec3(0.22, 0.38, 0.1) * (uAmb + uKey), 1.0 - smoothstep(0.004, 0.008, abs(qr.x - sc.x - 0.02)));
        gCol = mix(gCol, INK, fillP(st - gLW) * smoothstep(0.05, 0.3, uMunch));
        gCol = mix(gCol, stc, fillP(st) * smoothstep(0.05, 0.3, uMunch));
    }
    if (farOver) paint(capsule(qr, shF, hF, armR), tone(BLACK, qr, hF, 0.1));
    if (armOver) paint(capsule(qr, shN, hN, armR), tone(BLACK, qr, hN, 0.1));
    return vec4(gCol, gA);
}

void main() {
    gAA = uPx / uH * 0.85;
    gLW = uPx / uH * 2.2;
    vec2 q = (vP - uPos) / uH;
    q.x *= uFace;
    vec4 c = vec4(0.0);
    if (q.y >= -0.02) {
        c = panda(q);
        c.rgb = mix(c.rgb, uHazeC, uHaze);
    }
    // the reflection in a wet ground
    if (q.y < 0.0 && uReflect > 0.0) {
        vec2 r = vec2(q.x + 0.01 * sin(q.y * 60.0 + uBreath * 3.0), -q.y * 1.05);
        vec4 m = panda(r);
        c = vec4(m.rgb * 0.4, m.a * uReflect * exp(q.y * 3.5));
    }
    vec3 rgb = c.rgb * c.a;
    float a = c.a;
    float sh = exp(-sq(q.x / 0.3) - sq(q.y / 0.03)) * uShadow * (1.0 - 0.6 * sat(uHop * 10.0)) * (1.0 - a);
    a += sh * 0.5;
    frag = vec4(rgb, a) * uAlpha;
}
)";

// ------------------------------------------------------------------------------------------------ lights, butterflies, the gauge
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (radius, board units), mode
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // param, param2, -, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vLocal = aPos;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(aA.xy + aPos * aA.z, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uPxPerUnit;
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 1) {
        // the gauge: a fine ring, the charge an arc of light from the top, clockwise
        float ring = 1.0 - smoothstep(px * 0.6, px * 1.6, abs(r - 0.8));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = (1.0 - smoothstep(px * 1.2, px * 2.6, abs(r - 0.8))) * on;
        float halo = exp(-sq((r - 0.8) / 0.1)) * on * 0.3 * (0.5 + 0.5 * vC.y);
        float v = ring * 0.3 + arc + halo;
        frag = vec4(c * v * vB.a, v * vB.a * 0.5);
        return;
    }
    if (mode == 3) {
        // a butterfly: two pairs of wings flapping (param: the flap 0 shut .. 1 open), a body
        vec2 q = vLocal;
        float open = 0.25 + 0.75 * vC.x;
        vec2 w = vec2(abs(q.x) / max(open, 0.05), q.y);
        float up = length((w - vec2(0.42, 0.22)) * vec2(1.0, 1.15)) - 0.38;
        float lo = length((w - vec2(0.3, -0.25)) * vec2(1.0, 1.0)) - 0.25;
        float wing = min(up, lo);
        float body = length(q * vec2(4.0, 1.0)) - 0.32;
        float aa = px * 1.5 / max(open, 0.3);
        float wa = 1.0 - smoothstep(-aa, aa, wing);
        float ba = 1.0 - smoothstep(-px, px, body);
        vec3 col = c * (0.75 + 0.25 * smoothstep(0.6, 0.0, length(w - vec2(0.4, 0.2))));
        col = mix(col, c * 0.5, (1.0 - smoothstep(-0.06, -0.02, wing)) * 0.0);
        float edge = smoothstep(-0.08, -0.0, wing) * wa;
        col = mix(col, c * 0.45, edge * 0.6);
        col = mix(col, vec3(0.12, 0.1, 0.1), ba);
        float a = max(wa, ba) * vB.a;
        frag = vec4(col * a, a);
        return;
    }
    if (mode == 6) {
        // a heart, a sticker with a thin outline, swaying a little (param: its angle)
        vec2 q = rot(vC.x) * vLocal * 1.25;
        q.y += 0.55;
        q.x = abs(q.x);
        float d;
        if (q.y + q.x > 1.0) d = length(q - vec2(0.25, 0.75)) - 0.3536;
        else d = sqrt(min(dot(q - vec2(0.0, 1.0), q - vec2(0.0, 1.0)), dot(q - 0.5 * max(q.x + q.y, 0.0), q - 0.5 * max(q.x + q.y, 0.0)))) * sign(q.x - q.y);
        d /= 1.25;
        float edge = px * 1.6;
        float a = 1.0 - smoothstep(-px, px, d - edge);
        vec3 col = mix(vB.rgb, vec3(1.0, 0.85, 0.88), exp(-dot(q - vec2(0.3, 0.75), q - vec2(0.3, 0.75)) * 12.0) * 0.6);
        col = mix(vec3(0.25, 0.04, 0.08), col, 1.0 - smoothstep(-px, px, d));
        frag = vec4(col * a, a) * vB.a;
        return;
    }
    if (mode == 5) {
        // a snack: a short piece of bamboo with a node and a leaf, tumbling slowly (param: its angle)
        vec2 q = rot(vC.x) * vLocal;
        float d = sdSeg(q, vec2(-0.7, 0.0), vec2(0.7, 0.0)) - 0.2;
        vec2 lq = rot(0.7) * (q - vec2(0.35, 0.28));
        float leaf = sdEll(lq, vec2(0.32, 0.09));
        float sh = min(d, leaf);
        float a = 1.0 - smoothstep(-px * 1.2, px * 1.2, sh - px * 1.8);
        vec3 col = mix(vec3(0.35, 0.55, 0.16), vec3(0.72, 0.85, 0.38), smoothstep(-0.2, 0.15, q.y));
        col = mix(col, vec3(0.25, 0.4, 0.1), (1.0 - smoothstep(0.03, 0.06, abs(q.x - 0.12))) * step(d, 0.0));
        col = mix(col, vec3(0.3, 0.6, 0.2), step(leaf, d));
        col = mix(vec3(0.05, 0.04, 0.03), col, 1.0 - smoothstep(-px * 1.2, px * 1.2, sh));
        col *= vB.rgb;
        frag = vec4(col * a, a) * vB.a;
        return;
    }
    if (mode == 4) {
        // a firefly / a light of hope: a bright core, a warm halo, gently twinkling
        float core = exp(-r * r * 60.0);
        float halo = exp(-r * r * 6.0) * 0.45;
        float v = (core + halo) * smoothstep(1.0, 0.75, r);
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    // a mote of dust
    float v = exp(-r * r * 9.0) * smoothstep(1.0, 0.7, r);
    frag = vec4(c * v * vB.a, v * vB.a);
}
)";

// ------------------------------------------------------------------------------------------------ the veil
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil, uRail;
uniform float uWellA, uColA, uRailA, uPx;
uniform vec4 uColL, uColR;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = sdBox(w, vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = sdBox(w - (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.2, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.5 + exp(-abs(dw) * 3.0) * 0.08;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ the cells
// One clean, solid cell per block, a short segment of a bamboo stalk in its piece's color: a soft cylindrical shading
// across it (a sheen left of its middle), a node ring near its base, a faint fibre, a thin darker line of its own
// color around it. The ghost is the outline alone; a cleared cell shrinks softly away.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, -
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 block, 1 ghost), erase 0..1, glow, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    vec2 wp = aA.xy + aPos * aA.z;
    vLocal = aPos * 0.5;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uBridge;
uniform vec3 uBridgeInk, uLightC;
float lod2(float aa) { return smoothstep(0.08, 0.03, aa); }
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float shrink = 1.0 - 0.6 * smoothstep(0.0, 1.0, erase);
    vec2 q = vLocal / shrink;
    float d = sdBox(q, vec2(0.44 - 0.12)) - 0.12;
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(d + 0.03));
        float a = (ring * 0.75 + inside * 0.06) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // a stalk: round across (lit a little from above), a sheen, a node ring near its base, a faint fibre
    float cyl = sat(1.0 - sq(q.x / 0.46));
    vec3 c = t * mix(0.74, 1.06, sqrt(cyl)) * mix(0.93, 1.04, smoothstep(-0.45, 0.45, q.y));
    c = mix(c, mix(t, uLightC, 0.5) * 1.12, exp(-sq((q.x + 0.17) / 0.055)) * 0.4);
    float ny = q.y + 0.29;
    c = mix(c, t * 0.66, (1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(ny))) * 0.75);
    c = mix(c, mix(t, vec3(1.0), 0.3), (1.0 - smoothstep(0.012, 0.012 + aa * 1.5, abs(ny - 0.045))) * 0.45);
    c *= 1.0 - 0.03 * smoothstep(0.35, 0.5, abs(fract(q.x * 7.0 + 0.5) - 0.5) * 2.0) * lod2(aa);
    // its own thin darker edge
    c = mix(c, t * 0.55, 1.0 - smoothstep(0.0, 0.035 + aa, -d));
    c += t * vC.z * 0.6;
    c = mix(c, mix(t, vec3(1.0), 0.5), smoothstep(0.0, 0.6, erase) * 0.5);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * (1.0 - smoothstep(0.55, 1.0, erase));
    frag = vec4(c * a, a);
}
)";

// ------------------------------------------------------------------------------------------------ lettering
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
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.15, 0.45, d) * uHaloA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill);
    frag = vec4(c, a) * uAlpha;
}
)";

// ------------------------------------------------------------------------------------------------ post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.3 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.8);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace lostpanda_shaders

#pragma once
#include <string>
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
const float GY0 = -7.4, PLAT_Y = -20.4;
const float CAR = 17.0, CGAP = 0.6;
uniform float uTrainX, uDoor, uTrainLit, uTrainBody, uSway, uTrainT;
uniform float uDir;       // which way the train runs: +1 its front car leads (head lamps there), -1 backing away
uniform float uHead;      // how much its head lamps light the walls around them
uniform float uBlur;      // the camera's motion blur (world units over a shutter's time)
uniform float uRunBlur;   // the train's own motion blur against the world (likewise)
// the track: level through the station and the tunnel, then a steady grade under 7 degrees (eased at both ends) up
// the cutting to the surface
float trackY(float x) {
    float t = clamp((x - 1182.0) / 112.0, 0.0, 1.0);
    return mix(PLAT_Y, GY0, 0.95 * t + 0.05 * t * t * (3.0 - 2.0 * t));
}
// the tunnels' lamps (one every 9, near their ceiling) lighting the train as they sweep along it, smeared by its speed
float tunnelPool(vec2 w) {
    if (w.x < 10.0 || w.x > 1191.0 || (w.x > 88.0 && w.x < 146.0)) return 0.0;
    float lx = mod(w.x, 9.0) - 4.5;
    float sg = sqrt(1.4 * 1.4 + sq(uRunBlur * 0.5));
    return exp(-sq(lx / sg)) * 1.4 / sg * smoothstep(trackY(w.x) + 1.0, trackY(w.x) + 6.5, w.y);
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
    vec3 bc = vec3(0.3, 0.32, 0.36) * uTrainBody * (0.8 + 0.3 * smoothstep(0.0, 8.0, l.y));
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
    col = mix(col, vec3(0.3, 0.32, 0.36) * uTrainBody * (0.8 + 0.3 * smoothstep(0.0, 8.0, l.y)), fillA(wall));
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
        vec3 body = vec3(0.3, 0.32, 0.36) * uTrainBody;
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
        // sparks from the bogies now and then at speed, flying back
        if (uRunBlur > 0.4) {
            for (int k = 0; k < 2; k++) {
                float ph = floor(uTrainT * 9.0 + float(ci) * 3.0 + float(k) * 7.0);
                if (hash1(ph + float(k) * 13.0) > 0.3) continue;
                float t = fract(uTrainT * 9.0);
                vec2 sq0 = l - vec2(k == 0 ? 3.0 : CAR - 3.0, 0.0);
                for (int j = 0; j < 5; j++) {
                    float fj = float(j);
                    vec2 sp = vec2(-(0.6 + 2.2 * hash1(ph + fj)) * t, (0.3 + 1.0 * hash1(ph + fj + 9.0)) * t - 1.4 * t * t);
                    float g = exp(-dot(sq0 - sp, sq0 - sp) / 0.005) * (1.0 - t);
                    col += vec3(1.0, 0.62, 0.22) * g * 1.6;
                    a = max(a, g);
                }
            }
        }
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
            // passengers: silhouettes on the benches or standing (none at the cub's door)
            for (int k = 0; k < 4; k++) {
                float fk = float(k);
                float px = ci == 0 ? (k == 0 ? 5.4 : (k == 1 ? 7.4 : (k == 2 ? 9.6 : 14.6))) : (k == 0 ? 1.8 : (k == 1 ? 3.3 : (k == 2 ? 6.6 : 8.8)));
                float hp = hash1(fk * 3.7 + float(ci) * 11.0);
                if (hp > 0.75) continue;
                vec2 mq = vec2(l.x - px, l.y - (hp < 0.4 ? 1.5 : 0.35));
                float pd = hp < 0.4 ? min(sdBox(mq - vec2(0.0, 0.85), vec2(0.3, 0.6)) - 0.12, length(mq - vec2(0.0, 1.9)) - 0.3)
                                    : min(min(sdBox(mq - vec2(0.0, 2.3), vec2(0.28, 0.8)) - 0.12, length(mq - vec2(0.0, 3.5)) - 0.3),
                                          min(sdSeg(mq, vec2(-0.12, 1.5), vec2(-0.14, 0.0)), sdSeg(mq, vec2(0.12, 1.5), vec2(0.14, 0.0))) - 0.11);
                wc = mix(wc, vec3(0.05, 0.04, 0.04), fillA(pd));
            }
            wc *= uTrainLit;
            col = mix(col, wc, wallA);
            a = max(a, wallA);
        }
        // the floor, the roof, the ends: the cut, outlined (not under the cabs, which are one shape of their own)
        bool cabSide = (ci == 1 && l.x > CAR - 3.15) || (ci == 0 && l.x < 3.15);
)"
R"(        float sh = cabSide ? 1e3 : min(sdBox(l - vec2(CAR * 0.5, 0.5), vec2(CAR * 0.5, 0.3)), sdBox(l - vec2(CAR * 0.5, 6.95), vec2(CAR * 0.5, 0.35)));
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
    col += col * vec3(1.0, 0.8, 0.55) * tunnelPool(w) * 1.3 * a;
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
        vec3 c = vec3(0.34, 0.36, 0.4) * (0.85 + 0.3 * smoothstep(0.8, 2.3, l.y)) * uTrainBody;
        c = mix(c, vec3(0.5, 0.07, 0.07) * uTrainBody, fillA(abs(l.y - 1.95) - 0.12));
        for (int k = 0; k < 2; k++) {
            float dx = abs(l.x - (k == 0 ? 4.6 : 12.4));
            float seam = min(abs(dx - 0.95), abs(dx - open)) - 0.025;
            c = mix(c, vec3(0.03), fillA(seam) * step(dx, 0.98));
        }
        c = mix(vec3(0.02), c, fillA(panel));
        c += c * vec3(1.0, 0.8, 0.55) * tunnelPool(w) * 1.3;
        o = vec4(mix(o.rgb, c, a), max(o.a, a));
    }
    return o;
}
)";

// ------------------------------------------------------------------------------------------------ the world
// One continuous world in a side view, its near plane cut away like a cross-section where it goes underground:
// world x along the way (0 the rainy street .. ~570 the glade), world y up (the street at GY0, the subway's platform
// at PLAT_Y). The camera (uD, uCamY: the world point at the screen's origin) follows the cub. Layers behind the near
// plane scroll at their own parallax k (x and y); what stands on the near plane (lamps, the stairs, the train, the
// ground and the earth's cut) moves with the world. Where each kind of scenery stands is a density along the way,
// evaluated at the object's own place (no popping): the city thins out into the industrial district, then the fields,
// the bamboo, the glade.
inline const std::string WORLD_FS_SRC = std::string(R"(#version 330 core
// the parts of the way the world draws (a variant leaves out the city or the countryside where neither shows)
#ifndef ZONES
#define CITY true
#define COUNTRY true
#endif
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
float zCity(float x) { return 1.0 - smoothstep(1241.0, 1286.0, x); }
float zShop(float x) { return smoothstep(24.0, 50.0, x) * (1.0 - smoothstep(1226.0, 1251.0, x)); }
float zInd(float x) { return smoothstep(1251.0, 1286.0, x) * (1.0 - smoothstep(1366.0, 1391.0, x)); }
float zNature(float x) { return smoothstep(1336.0, 1386.0, x); }
float zField(float x) { return smoothstep(1364.0, 1388.0, x) * (1.0 - smoothstep(1428.0, 1454.0, x)); }
float zBamboo(float x) { return smoothstep(1396.0, 1436.0, x) * (1.0 - 0.8 * smoothstep(1472.0, 1490.0, x)); }
float zGlade(float x) { return smoothstep(1472.0, 1490.0, x); }
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
    // over the city at night: a soft violet haze, and the crescent moon (fixed on the screen)
    float moon = zCity(uD + 20.0) * (1.0 - uDawn) * uLamp;
    if (moon > 0.0) {
        c += vec3(0.22, 0.1, 0.36) * smoothstep(0.4, 0.85, fbm(vec2(p.x * 0.05 + uD * 0.004, py * 0.12))) * 0.3 * moon * (1.0 - h * 0.5);
        vec2 mq = p - (uViewC + uViewH * vec2(0.55, 0.64));
        float disc = length(mq) - 0.95;
        float cres = max(disc, -(length(mq - vec2(0.4, 0.22)) - 0.85));
        c += vec3(0.45, 0.85, 1.0) * exp(-max(disc, 0.0) * 1.1) * 0.13 * moon;
        c = mix(c, vec3(0.82, 0.97, 1.0), fillS(cres, gPx * 1.5) * moon);
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
            // flat, in two tones (the sunlit tops, the shaded bellies), a soft inked edge
            float a = fillS(d * sc, gPx) * uClouds * (0.95 - 0.2 * fi);
            vec3 shd = mix(mix(uSkyHor, uSkyTop, 0.35) * 0.95, mix(uSunCol, vec3(1.0), 0.55), 0.25);
            vec3 lit = mix(uSunCol, vec3(1.0), 0.6) * 1.05;
            float tone = smoothstep(-0.03, 0.03, q.y + 0.05 - 0.25 * sin(q.x * 1.3 + id));
            vec3 cc = mix(shd, lit, tone);
            cc = mix(cc, mix(shd, uSkyTop, 0.5) * 0.8, fillS(abs(d * sc) - 0.025, gPx) * 0.7);
            c = mix(c, cc, a);
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

// ---- the city: a cartoon night city in layers (thin dark outlines, flat shading, warm windows, glowing neon): far
// misty skyscrapers, a denser middle row of towers, an elevated line on a viaduct, the street's brick buildings with
// their shops and signs, power lines, and black silhouettes at the very front. Each layer is a row of cells along
// its own parallax (u = p.x + uD * k), every cell its own hashes: the city never repeats.
const vec3 INK = vec3(0.018, 0.014, 0.04);
const vec3 WARM = vec3(0.95, 0.56, 0.2);
// one of the neon colors: pink, cyan, violet, orange
vec3 neonCol(float h) {
    return h < 0.3 ? vec3(1.0, 0.3, 0.72) : (h < 0.58 ? vec3(0.3, 0.9, 1.0) : (h < 0.8 ? vec3(0.72, 0.42, 1.0) : vec3(1.0, 0.56, 0.22)));
}
// a neon tube (d: its distance): a bright core and a soft bloom
vec3 neonTube(vec3 c, float d, vec3 nc, float on) {
    c = mix(c, nc * 1.5 + 0.3, fillS(d - 0.03, gPx) * on);
    return c + nc * exp(-max(d, 0.0) * 6.0) * 0.32 * on;
}
// an abstract letter in a box of half size b: a few strokes picked by a hash (a distance)
float glyph(vec2 g, vec2 b, float h) {
    float d = 1e3;
    if (fract(h * 3.0) < 0.7) d = min(d, sdSeg(g, -b, vec2(-b.x, b.y)));
    if (fract(h * 7.0) < 0.55) d = min(d, sdSeg(g, vec2(b.x, -b.y), b));
    if (fract(h * 13.0) < 0.6) d = min(d, sdSeg(g, vec2(-b.x, b.y), b));
    if (fract(h * 17.0) < 0.45) d = min(d, sdSeg(g, vec2(-b.x, 0.0), vec2(b.x, 0.0)));
    if (fract(h * 23.0) < 0.55) d = min(d, sdSeg(g, -b, vec2(b.x, -b.y)));
    if (fract(h * 29.0) < 0.2) d = min(d, sdSeg(g, vec2(-b.x, b.y), vec2(b.x, -b.y)));
    if (d > 1e2) d = sdSeg(g, vec2(0.0, -b.y), vec2(0.0, b.y));
    return d;
}
// a row of n letters across a sign (q centered, half size hs); `vert` stacks them
float lettering(vec2 q, vec2 hs, float n, float seed, bool vert) {
    if (vert) { q = q.yx; hs = hs.yx; }
    float gw = 2.0 * hs.x / n;
    float gi = clamp(floor((q.x + hs.x) / gw), 0.0, n - 1.0);
    vec2 g = vec2(q.x + hs.x - (gi + 0.5) * gw, q.y);
    if (vert) g = g.yx;
    vec2 b = vert ? vec2(hs.y * 0.62, gw * 0.3) : vec2(gw * 0.3, hs.y * 0.62);
    return glyph(g, b, hash1(gi * 7.13 + seed));
}

// far skyscrapers (layer k): misty silhouettes, setbacks, spires and antennas with red lights, sparse windows, a few
)") +
R"(// cyan neon strips; two rows, the back one mistier
vec3 cityFar(vec2 p, vec3 c, float k) {
    vec3 mist = mix(uFog, vec3(0.1, 0.05, 0.2), 0.4) * (1.0 + 0.5 * uDawn);
    for (int r = 0; r < 2; r++) {
        float fr = float(r);
        float cw = 3.6 - 0.8 * fr;
        float u = p.x + uD * k + fr * 11.7;
        float id = floor(u / cw);
        float dens = zCity(xFar((id + 0.5) * cw - fr * 11.7 - uD * k));
        if (dens <= 0.005) continue;
        float sd = id + fr * 101.0;
        float lx = (u / cw - id) * cw;
        float h = (r == 0 ? 8.0 : 4.5) + (r == 0 ? 9.0 : 6.0) * pow(hash1(sd), 1.3);
        float w = cw * (0.55 + 0.4 * hash1(sd + 1.0));
        float cx = (cw - w) * hash1(sd + 2.0) + w * 0.5;
        float base = GY0 + 2.0;
        vec2 q = vec2(lx - cx, p.y - base);
        float d = max(abs(q.x) - w * 0.5, q.y - h);
        float st = hash1(sd + 3.0);
        if (st > 0.45) d = min(d, max(abs(q.x) - w * 0.32, q.y - h - 1.4));                  // a setback
        if (st > 0.75) d = min(d, max(abs(q.x) - 0.05 - 0.25 * sat((h + 4.0 - q.y) / 2.6), q.y - h - 4.0)); // a spire
        float ant = hash1(sd + 4.0) > 0.6 ? max(abs(q.x - w * 0.2) - 0.04, q.y - h - (st > 0.45 ? 3.0 : 1.8)) : 1e3;
        float a = fillA(min(d, ant)) * dens;
        if (a > 0.0) {
            vec3 b = mix(vec3(0.02, 0.022, 0.06), mist, r == 0 ? 0.45 : 0.2);
            b *= 0.9 + 0.2 * hash1(sd + 5.0);
            float gl;
            float lit = windowsLit(vec2(q.x + w * 0.5, q.y), vec2(0.4, 0.6), vec2(0.1, 0.14), sd * 3.1, 0.13, gl) * step(d, 0.0);
            vec3 wc = hash1(sd + 6.0) > 0.8 ? vec3(0.35, 0.85, 1.0) : WARM;
            b += wc * lit * (r == 0 ? 0.35 : 0.55) * uLamp;
            if (hash1(sd + 7.0) > 0.82) b = neonTube(b, max(abs(q.x - w * 0.3) - 0.03, abs(q.y - h * 0.5) - h * 0.35), vec3(0.3, 0.9, 1.0), 0.6 * uLamp);
            c = mix(c, b, a);
        }
        // the red lights atop the spires and antennas, blinking
        if (st > 0.75 || hash1(sd + 4.0) > 0.6) {
            float ty = st > 0.75 ? h + 4.0 : h + (st > 0.45 ? 3.0 : 1.8);
            vec2 rl = vec2(q.x - (st > 0.75 ? 0.0 : w * 0.2), q.y - ty);
            c += vec3(1.0, 0.15, 0.12) * exp(-dot(rl, rl) / 0.012) * (0.5 + 0.5 * step(0.5, fract(uTime * 0.6 + hash1(sd)))) * dens * uLamp;
        }
        // a purple mist between the rows, thicker low down
        c = mix(c, mist, 0.18 * dens * (1.0 - smoothstep(GY0 + 2.0, GY0 + 12.0, p.y)));
    }
    return c;
}

// the middle row of towers (layer k): denser and darker, more windows, water tanks and boxes on the roofs
vec3 cityMid(vec2 p, vec3 c, float k) {
    float cw = 2.7;
    float u = p.x + uD * k + 5.3;
    float id = floor(u / cw);
    float dens = zCity(xFar((id + 0.5) * cw - 5.3 - uD * k));
    if (dens <= 0.005) return c;
    float lx = (u / cw - id) * cw;
    float h = 4.0 + 7.0 * pow(hash1(id + 31.0), 1.2);
    float w = cw * (0.7 + 0.3 * hash1(id + 32.0));
    vec2 q = vec2(lx - cw * 0.5, p.y - (GY0 + 1.2));
    float d = max(abs(q.x) - w * 0.5, q.y - h);
    float rt = hash1(id + 33.0);
    vec2 tq = q - vec2(w * (rt - 0.5) * 0.5, h + 0.55);
    float tank = rt > 0.55 ? min(sdBox(tq, vec2(0.32, 0.32)) - 0.06, max(abs(abs(tq.x) - 0.22) - 0.04, abs(tq.y + 0.5) - 0.25)) : 1e3;
    float box = rt < 0.35 ? sdBox(q - vec2(-w * 0.2, h + 0.18), vec2(0.35, 0.2)) : 1e3;
    float dd = min(d, min(tank, box));
    float a = fillA(dd) * dens;
    if (a <= 0.0) return c;
    vec3 b = mix(vec3(0.015, 0.016, 0.045), uFog, 0.15) * (0.85 + 0.3 * hash1(id + 34.0)) * (1.0 + 0.6 * uDawn);
    float gl;
    float lit = windowsLit(vec2(q.x + w * 0.5 - 0.1, q.y - 0.3), vec2(0.42, 0.62), vec2(0.11, 0.15), id * 5.7, 0.26, gl) * step(d, -0.08);
    vec3 wc = hash1(id + 35.0) > 0.8 ? vec3(0.35, 0.85, 1.0) : WARM;
    b += wc * lit * 0.75 * uLamp;
    b = mix(b, INK, fillA(abs(dd) - 0.03) * 0.6);
    return mix(c, b, a);
}

// the elevated line (layer k): stretches of a viaduct on pillars, a lit train running along its deck now and then,
// cars with their lights on its lower deck
vec3 viaduct(vec2 p, vec3 c, float k) {
    float u = p.x + uD * k;
    float sl = 96.0;
    float sid = floor(u / sl);
    float dens = zCity(xFar((sid + 0.5) * sl - uD * k)) * step(0.4, hash1(sid + 61.0));
    if (dens <= 0.005) return c;
    float sx = u - (sid + 0.5) * sl;
    float ends = abs(sx) - sl * 0.42;
    float deckY = GY0 + 5.4, lowY = GY0 + 2.9;
    float px = mod(u, 8.0) - 4.0;
    float d = max(abs(p.y - deckY) - 0.34, ends);
    d = min(d, max(abs(p.y - lowY) - 0.16, ends));
    d = min(d, max(sdBox(vec2(px, p.y - (GY0 + 3.0)), vec2(0.3, 2.4)), ends - 1.0));
    vec3 vc = mix(vec3(0.02, 0.024, 0.06), uFog, 0.2) * (1.0 + 0.6 * uDawn);
    // the train on the upper deck: four cars, its windows lit, passing every so often
    float tx = mod(u - uTime * 10.0 + hash1(sid) * 300.0, 300.0);
    vec2 tq = vec2(mod(tx, 5.2) - 2.6, p.y - deckY - 1.0);
    float car = step(tx, 20.8);
    float body = car > 0.0 ? sdBox(tq, vec2(2.45, 0.62)) - 0.08 : 1e3;
    float wins = sdBox(vec2(mod(tq.x + 2.6, 0.75) - 0.375, tq.y + 0.05), vec2(0.22, 0.2));
    float a = fillA(min(d, body)) * dens;
    if (a > 0.0) {
        vec3 b = vc;
        b = mix(b, mix(vec3(0.07, 0.075, 0.11), uFog, 0.2), fillA(body));
        b = mix(b, WARM * (0.6 + 0.6 * uLamp), fillA(wins) * fillA(body + 0.12) * car);
        b = mix(b, INK, fillA(abs(min(d, body)) - 0.03) * 0.7);
        // lamps under the deck
        b += vec3(0.6, 0.5, 1.0) * exp(-sq(mod(u, 4.0) - 2.0) * 6.0 - sq(p.y - deckY + 0.4) * 30.0) * 0.4 * uLamp * step(ends, 0.0);
        c = mix(c, b, a);
    }
    // the cars on the lower deck: head and tail lights going both ways
    float cx = mod(u + uTime * 6.0, 13.0) - 6.5, cx2 = mod(u - uTime * 5.0 + 4.0, 17.0) - 8.5;
    float onDeck = step(ends, 0.0) * dens * uLamp;
    vec2 h1 = vec2(cx, p.y - lowY - 0.35), h2 = vec2(cx2, p.y - lowY - 0.35);
    c += vec3(1.0, 0.95, 0.8) * exp(-dot(h1, h1) / 0.02) * onDeck + vec3(1.0, 0.15, 0.1) * exp(-dot(h2, h2) / 0.02) * onDeck;
    return c;
}

// the street's buildings (layer k): brick corner buildings of three to five storeys, warm windows (some with blinds),
// a cyan-lit one now and then, cornices, fire escapes, water tanks and boxes on the roofs; shopfronts with awnings,
// neon shop signs and tall vertical neon signs where the shopping street is, arcade cabinets glowing in some shops;
// power lines sagging between poles in front of them
vec3 cityStreet(vec2 p, vec3 c, float k) {
    float cw = 6.6;
    float u = p.x + uD * k + 3.1;
    float id = floor(u / cw);
    float xe = ((id + 0.5) * cw - 3.1) / k;              // where it stands along the way
    float city = zCity(xe);
    if (city > 0.005 && hash1(id + 0.7) > 0.1) {
        float shop = zShop(xe);
        float lx = (u / cw - id) * cw;
        float bw = cw * (0.86 + 0.13 * hash1(id + 1.0));
        float x0 = (cw - bw) * 0.5;
        float base = GY0 + 0.6;
        float nfl = 2.0 + floor(hash1(id + 2.0) * 3.0);
        const float GF = 2.7, FH = 1.7;
        float top = GF + nfl * FH + 0.3;
        vec2 q = vec2(lx - x0, p.y - base);                // building space: 0..bw across, 0 at its foot
        float d = sdBox(q - vec2(bw * 0.5, top * 0.5), vec2(bw * 0.5, top * 0.5));
        d = min(d, sdBox(q - vec2(bw * 0.5, top + 0.12), vec2(bw * 0.5 + 0.12, 0.14)));      // the cornice
        // on the roof: a water tank on legs, boxes, a chimney
        float rh = hash1(id + 3.0);
        vec2 tq = q - vec2(bw * (0.25 + 0.5 * hash1(id + 4.0)), top + 1.05);
        float roof = 1e3;
        if (rh > 0.5) roof = min(sdBox(tq, vec2(0.45, 0.45)) - 0.08, max(abs(abs(tq.x) - 0.32) - 0.05, abs(tq.y + 0.7) - 0.3));
        if (rh < 0.6) roof = min(roof, sdBox(q - vec2(bw * 0.18, top + 0.42), vec2(0.42, 0.24)));
        if (fract(rh * 7.0) < 0.4) roof = min(roof, sdBox(q - vec2(bw * 0.85, top + 0.6), vec2(0.16, 0.45)));
        float sil = min(d, roof);
        float a = fillA(sil) * city;
        if (a > 0.0) {
            float pal = hash1(id + 5.0);
            vec3 bc = pal < 0.3 ? vec3(0.075, 0.032, 0.05) : (pal < 0.55 ? vec3(0.035, 0.035, 0.085) : (pal < 0.8 ? vec3(0.025, 0.05, 0.065) : vec3(0.055, 0.03, 0.08)));
            bc *= (1.0 + 0.7 * uDawn) * (0.9 + 0.2 * hash1(id + 6.0));
            // brick courses, a soft light from the street below, the cornice lighter
            vec3 b = bc * (1.0 - 0.12 * (1.0 - smoothstep(0.02, 0.05, abs(fract(q.y / 0.24) - 0.5) * 0.24)) * lod(0.24));
            b *= 0.85 + 0.3 * vnoise(q * vec2(2.0, 5.0)) * 0.5;
            b += vec3(0.25, 0.14, 0.22) * exp(-q.y * 0.6) * 0.25 * uLamp;
            b = mix(b, bc * 1.7, fillA(sdBox(q - vec2(bw * 0.5, top + 0.12), vec2(bw * 0.5 + 0.12, 0.14))));
            b = mix(b, bc * 1.35, fillA(abs(q.y - GF) - 0.09) * step(q.x, bw) * step(0.0, q.x));   // the ledge over the shops
            b = mix(b, mix(bc * 0.6, uFog * 0.6, 0.3), fillA(roof));
            // the windows: three or four a floor, warm or cyan when lit (some with blinds), dark glass when not
            if (q.y > GF && q.y < top - 0.2) {
                float nw = bw > 6.0 ? 4.0 : 3.0;
                float wcw = bw / nw;
                float wi = floor(q.x / wcw), fi = floor((q.y - GF) / FH);
                vec2 wq = vec2(q.x - (wi + 0.5) * wcw, q.y - GF - (fi + 0.5) * FH + 0.05);
                float wd = sdBox(wq, vec2(0.3, 0.46));
                float hw = hash1(id * 13.0 + wi * 3.0 + fi * 7.0);
                float litW = step(hw, 0.58) * uLamp;
                vec3 wc = hash1(id + 7.0) > 0.78 ? vec3(0.3, 0.9, 1.0) : mix(WARM, vec3(1.0, 0.86, 0.55), fract(hw * 9.0));
                vec3 glass = mix(vec3(0.03, 0.04, 0.09) + uFog * 0.15, wc * (1.0 + 0.2 * (wq.y + 0.46)), litW);
                glass *= 1.0 - 0.45 * step(0.6, fract(hw * 31.0)) * litW * step(0.5, fract(wq.y / 0.13)) * lod(0.13);   // blinds
                b = mix(b, INK, fillA(wd - 0.06));
                b = mix(b, glass, fillA(wd));
                b = mix(b, bc * 1.6, fillA(sdBox(wq - vec2(0.0, -0.52), vec2(0.36, 0.05))));      // the sill
                b += wc * exp(-max(wd, 0.0) * 4.0) * 0.08 * litW;
            }
            // the ground floor: a shop (a lit front, a door, an awning, its neon sign) or a front door and a shutter
            bool isShop = hash1(id + 8.0) < 0.3 + 0.65 * shop;
            if (q.y < GF) {
                float fw = bw * 0.6;
                vec2 sq0 = q - vec2(0.35 + fw * 0.5, 1.0);
                float front = sdBox(sq0, vec2(fw * 0.5, 0.8));
                vec2 dq = q - vec2(bw * 0.8, 0.9);
                float door = sdBox(dq, vec2(0.38, 0.9));
                if (isShop) {
                    vec3 inC = mix(WARM, vec3(1.0, 0.9, 0.7), 0.3) * (0.5 + 0.5 * uLamp);
                    inC *= 0.75 + 0.25 * step(0.5, fract(sq0.y * 1.6 + 0.3));                  // shelves inside
                    if (hash1(id + 9.0) > 0.7) {
                        // two arcade cabinets, their screens glowing
                        float cab = abs(mod(sq0.x + fw * 0.5, 1.1) - 0.55);
                        vec2 cq = vec2(cab, sq0.y + 0.1);
                        inC = mix(inC, vec3(0.05, 0.04, 0.1), fillA(sdBox(cq, vec2(0.28, 0.65))));
                        inC = mix(inC, neonCol(hash1(id + floor((sq0.x + fw * 0.5) / 1.1))) * 1.3, fillA(sdBox(cq - vec2(0.0, 0.25), vec2(0.18, 0.15))));
                    }
                    b = mix(b, INK, fillA(front - 0.08));
                    b = mix(b, inC, fillA(front));
                    b = mix(b, INK, fillA(door - 0.07));
                    b = mix(b, inC * 0.9, fillA(door));
                    b += inC * exp(-max(min(front, door), 0.0) * 2.5) * 0.1;
                    // an awning (striped), or a neon shop sign across the front
                    if (hash1(id + 10.0) < 0.45) {
                        vec2 aq = q - vec2(0.35 + fw * 0.5, 1.98);
)" +
R"(                        float aw = sdBox(vec2(aq.x, aq.y), vec2(fw * 0.5 + 0.2 - aq.y * 0.3, 0.17));
                        vec3 ac = neonCol(hash1(id + 11.0)) * 0.45;
                        ac = mix(ac, vec3(0.85), 0.5 * step(0.5, fract(aq.x * 1.4)));
                        b = mix(b, INK, fillA(aw - 0.05));
                        b = mix(b, ac * (0.4 + 0.6 * uLamp), fillA(aw));
                    } else {
                        vec2 nq = q - vec2(bw * 0.5, 2.25);
                        vec2 ns = vec2(bw * 0.38, 0.27);
                        float sb = sdBox(nq, ns) - 0.05;
                        vec3 nc = neonCol(hash1(id + 12.0));
                        float on = uLamp * (0.85 + 0.15 * step(0.03, fract(uTime * 0.23 + hash1(id))));
                        b = mix(b, vec3(0.02, 0.015, 0.04), fillA(sb));
                        b = neonTube(b, abs(sb) - 0.0, nc, on * 0.8);
                        b = neonTube(b, lettering(nq, ns - vec2(0.15, 0.06), 4.0 + floor(hash1(id + 13.0) * 4.0), id, false), neonCol(hash1(id + 14.0)), on);
                    }
                } else {
                    b = mix(b, INK, fillA(door - 0.07));
                    b = mix(b, vec3(0.06, 0.05, 0.08) + WARM * 0.25 * uLamp * step(0.5, hash1(id + 15.0)), fillA(door));
                    float sh = sdBox(sq0, vec2(fw * 0.5, 0.8));
                    vec3 shut = bc * 0.7 * (0.85 + 0.15 * step(0.5, fract(sq0.y * 4.0)));
                    b = mix(b, INK, fillA(sh - 0.06));
                    b = mix(b, shut, fillA(sh));
                }
            }
            // a fire escape up one side: a landing at each floor, a ladder between them
            if (hash1(id + 16.0) < 0.3 && q.x < 1.7 && q.y > GF - 0.2) {
                float fy = mod(q.y - GF, FH);
                float landing = abs(fy - 0.05) - 0.04;
                float rail = max(abs(fy - 0.5) - 0.025, 0.0);
                float ladder = abs(q.x - 0.4 - fy * 0.55) - 0.04;
                float fe = min(min(landing, rail + 0.0), ladder);
                fe = max(fe, max(q.x - 1.6, 0.15 - q.x));
                b = mix(b, INK, fillA(fe) * step(q.y, top - 0.3));
            }
            // a tall vertical neon sign at one corner
            if (hash1(id + 17.0) < 0.15 + 0.55 * shop) {
                float side = hash1(id + 18.0) < 0.5 ? bw - 0.2 : 0.2;
                vec2 vq = q - vec2(side, GF + 1.2 + 1.3 * hash1(id + 19.0));
                vec2 vs = vec2(0.32, 1.0 + 0.6 * hash1(id + 20.0));
                float vb = sdBox(vq, vs) - 0.05;
                vec3 nc = neonCol(hash1(id + 21.0));
                b = mix(b, vec3(0.02, 0.015, 0.04), fillA(vb));
                b = neonTube(b, abs(vb), nc, uLamp * 0.8);
                b = neonTube(b, lettering(vq, vs - vec2(0.1, 0.12), 3.0 + floor(hash1(id + 22.0) * 3.0), id + 5.0, true), nc, uLamp);
            }
            b = mix(b, INK, fillA(abs(sil) - 0.045));     // the outline
            c = mix(c, b, a);
        }
    }
    // under the buildings, down to the near pavement: the far pavement (its kerb catching the light) and the wet road
    // (the buildings stand on it, never floating over the city behind)
    float gz = zCity((p.x + uD * k) / k);
    float gyq = p.y - (GY0 + 0.6);
    if (gz > 0.005 && gyq < 0.02) {
        vec3 pave = vec3(0.05, 0.04, 0.08) * (1.0 + 0.7 * uDawn);
        vec3 road = vec3(0.026, 0.022, 0.05) * (1.0 + 0.7 * uDawn);
        vec3 g = mix(road, pave, step(-0.5, gyq));
        g = mix(g, pave * 2.2, fillA(abs(gyq + 0.5) - 0.04));                                          // the kerb
        g += WARM * 0.05 * uLamp * smoothstep(-2.5, -0.5, gyq) * (0.6 + 0.4 * vnoise(vec2(p.x * 0.7, gyq * 6.0)));   // the shops' light on it
        g = mix(g, INK, fillA(abs(gyq) - 0.03));
        c = mix(c, g, gz);
    }
    // the power lines: a pole every 14, two wires sagging between them
    float pu = p.x + uD * k + 0.7;
    float pid = floor(pu / 14.0);
    float pcity = zCity(((pid + 0.5) * 14.0 - 0.7) / k);
    if (pcity > 0.005) {
        float pt = pu / 14.0 - pid;
        float top = GY0 + 11.2;
        float pole = max(abs((pt - 0.5) * 14.0) - 0.08, p.y - top - 0.2);
        float arm = max(abs((pt - 0.5) * 14.0) - 0.6, abs(p.y - top + 0.1) - 0.06);
        float s = fract(pt + 0.5);
        float w1 = abs(p.y - (top - 0.1 - 1.3 * 4.0 * s * (1.0 - s))) - 0.025;
        float w2 = abs(p.y - (top - 0.6 - 1.0 * 4.0 * s * (1.0 - s))) - 0.025;
        c = mix(c, INK, fillA(min(min(pole, arm), min(w1, w2))) * pcity * step(GY0 + 0.6, p.y));
    }
    return c;
}

// black silhouettes at the very front (layer k > 1): bins, sign posts, bollards, a traffic light, standing at the
// pavement's front edge
vec3 cityFront(vec2 p, vec2 w, vec3 c, float k) {
    float cw = 15.0;
    float u = p.x + uD * k;
    float id = floor(u / cw);
    float city = zCity(((id + 0.5) * cw) / k);
    float ind = COUNTRY ? zInd(((id + 0.5) * cw) / k) * (1.0 - city) : 0.0;
    if (max(city, ind) <= 0.005 || hash1(id + 71.0) > 0.6) return c;
    float lx = u - (id + 0.5) * cw + (hash1(id + 72.0) - 0.5) * 8.0;
    vec2 q = vec2(lx, w.y - (GY0 - 1.8));
    float t = hash1(id + 73.0);
    float d;
    if (COUNTRY && ind > city) {
        // the works: a stretch of chain-link fence (its mesh, barbed wire on top), drums, a stack of pallets and
        // crates, a warning sign on its post
        city = ind;
        if (t < 0.35) {
            float post = max(abs(mod(q.x + 1.2, 2.4) - 1.2) - 0.07, q.y - 3.0);
            vec2 m = vec2(q.x + q.y, q.x - q.y) / 0.32;
            float mesh = min(abs(fract(m.x) - 0.5), abs(fract(m.y) - 0.5)) * 0.32 * 0.7071 - 0.012;
            mesh = mix(0.03, mesh, lod(0.32));
            float fence = min(post, max(mesh, q.y - 2.7));
            fence = min(fence, abs(q.y - 2.7) - 0.04);
            fence = min(fence, max(abs(q.y - 3.05 - 0.08 * sin(q.x * 9.0)) - 0.025, abs(q.x) - 3.6));
            d = max(max(fence, abs(q.x) - 3.6), -q.y);
        } else if (t < 0.6) {
            float bx = mod(q.x + 0.55, 1.1) - 0.55;
            float n = floor((q.x + 0.55) / 1.1);
            d = max(sdBox(vec2(bx, q.y - 0.75), vec2(0.42, 0.75)) - 0.04, abs(n) - 1.0 - step(0.5, hash1(id + 74.0)) * 0.0);
            d = max(d, -max(abs(bx) - 0.48, abs(abs(q.y - 0.75) - 0.35) - 0.03) );
            d = max(d, abs(q.x) - 1.65);
        } else if (t < 0.85) {
            d = sdBox(q - vec2(0.0, 0.12), vec2(1.2, 0.12));
            d = min(d, sdBox(q - vec2(-0.3, 0.95), vec2(0.8, 0.7)));
            d = min(d, sdBox(q - vec2(0.75, 0.55), vec2(0.45, 0.4)));
            d = min(d, sdBox(q - vec2(-0.1, 2.0), vec2(0.5, 0.35)));
            d = max(d, -max(abs(q.y - 0.12) - 0.05, abs(abs(q.x) - 0.6) - 0.15));
        } else {
            d = sdBox(q - vec2(0.0, 1.6), vec2(0.07, 1.6));
            vec2 tq = q - vec2(0.0, 3.4);
            d = min(d, max(dot(vec2(abs(tq.x), tq.y), vec2(0.866, 0.5)) - 0.35, -tq.y - 0.45) - 0.04);
        }
    } else if (t < 0.3) {          // two bins
        d = min(sdBox(q - vec2(0.0, 0.75), vec2(0.38 - q.y * 0.03, 0.75)), sdBox(q - vec2(0.0, 1.55), vec2(0.45, 0.07)));
        d = min(d, sdBox(q - vec2(1.0, 0.65), vec2(0.34, 0.65)));
    } else if (t < 0.6) {   // a sign post, its arrows
        d = sdBox(q - vec2(0.0, 2.6), vec2(0.08, 2.6));
        d = min(d, sdBox(q - vec2(0.6, 4.6), vec2(0.65, 0.22)) - 0.02);
        d = min(d, sdBox(q - vec2(-0.5, 4.0), vec2(0.6, 0.2)) - 0.02);
    } else if (t < 0.8) {   // bollards
        float bx = mod(q.x + 0.75, 1.5) - 0.75;
        d = max(sdBox(vec2(bx, q.y - 0.5), vec2(0.13, 0.5)) - 0.04, abs(q.x) - 2.3);
    } else {                // a traffic light
        d = min(sdBox(q - vec2(0.0, 2.8), vec2(0.09, 2.8)), sdBox(q - vec2(0.0, 5.3), vec2(0.28, 0.7)) - 0.06);
    }
    vec3 sc = vec3(0.012, 0.01, 0.02) + uFog * 0.08 * uDawn;
    return mix(c, sc, fillA(d) * city);
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

// ---- out of the city, in the same drawn style: thin inked outlines (a dark shade of each thing's own color, hazed
// with its layer), flat shading in two tones, small details, layers of parallax (the far ones slowest, the silhouettes
// at the very front fastest); the light of the hour on all of it (dawn, morning, gold, sunset)
vec3 inkOf(vec3 col) { return col * 0.3 + INK * 0.6; }
// a shape (distance d) filled with b, its outline (half width t) in ink, at alpha a
vec3 drawn(vec3 c, float d, vec3 b, vec3 ink, float t, float a) {
    float al = fillA(d - t) * a;
    if (al <= 0.0) return c;
    return mix(c, mix(b, ink, fillA(abs(d) - t)), al);
}
// a daylight color under the hour's light (dim before the dawn, warmed by the sun)
vec3 dayLit(vec3 col) { return col * mix(vec3(1.0), uSunCol, 0.3) * (0.45 + 0.55 * uDawn); }
float ridgeN(float x) { return 1.0 - abs(2.0 * vnoise(vec2(x, 0.37)) - 1.0); }
// the place a middle layer shows at a cell's screen x
float xMid(float sx) { return uD + sx * 2.0; }

// misty mountains far away, three rows (the furthest palest and slowest): ridged peaks, the slopes facing the sun lit
// and the others shaded in flat tones, a soft inked crest, mist banks at their feet; they sink into the horizon where
// the city still stands
vec3 mountains(vec2 p, vec3 c) {
    float dens = max(zNature(xFar(p.x)), 0.55 * zInd(xFar(p.x)));
    if (dens <= 0.01) return c;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float k = 0.02 + 0.03 * fi;
        vec2 q = layerP(p, k);
        float u = q.x + uD * k + fi * 61.0;
        float base = GY0 + 1.6 - 0.5 * fi;
)" +
R"(        float amp = (11.0 - 3.5 * fi) * dens;
        float f = 0.045 + 0.03 * fi;
        float r0 = ridgeN(u * f + fi * 7.0);
        float d = q.y - (base + amp * (0.68 * r0 + 0.32 * ridgeN(u * f * 2.3 + 5.0 + fi)));
        if (d > gPx * 2.0) continue;
        float s = ridgeN((u + 0.3) * f + fi * 7.0) - r0;
        vec3 mc = mix(mix(uSkyTop, uFog, 0.5), uFog, 0.5 - 0.2 * fi) * (0.94 - 0.08 * fi);
        vec3 b = mix(mc * 0.93, mix(mc, uSunCol, 0.18) * 1.07, smoothstep(-0.002, 0.002, -s));
        float mist = 1.0 - smoothstep(base - 1.0, base + amp * 0.5, q.y);
        mist = sat(mist + (vnoise(vec2(u * 0.05 + uTime * 0.01, q.y * 0.3 + fi * 3.0)) - 0.5) * 0.9);
        b = mix(b, uFog, mist * (0.7 - 0.15 * fi));
        b = mix(b, mix(mc, INK, 0.3), fillA(abs(d) - 0.02) * (0.25 + 0.15 * fi));
        c = mix(c, b, fillA(d));
    }
    return c;
}

// the rolling hills of the countryside, two rows: a patchwork of fields down their slopes (meadows, young crops,
// wheat, furrows), hedgerows between them, round trees along the crests (two tones, inked), a farmhouse or a barn now
// and then on the near row
float hillH(float u, float fi, float dens) {
    return GY0 + 1.0 - 0.6 * fi + dens * ((2.2 - 0.9 * fi) + (1.2 - 0.3 * fi) * sin(u * (0.07 + 0.04 * fi) + fi * 2.0)
        + 0.7 * sin(u * (0.19 + 0.1 * fi) + 1.0) + 0.35 * vnoise(vec2(u * 0.45, fi)));
}
vec3 hills(vec2 p, vec3 c) {
    float dens = zNature(xFar(p.x)) * (1.0 - 0.85 * zBamboo(xFar(p.x)));
    if (dens <= 0.01) return c;
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        float k = 0.13 + 0.11 * fi;
        vec2 q = layerP(p, k);
        float u = q.x + uD * k + fi * 37.0;
        float hgt = hillH(u, fi, dens);
        float hz = 0.42 - 0.2 * fi;
        if (q.y > hgt + 2.4) continue;
        float d = q.y - hgt;
        if (d < gPx * 2.0) {
            float bh = 0.7 + 0.15 * fi;
            float dy = hgt - q.y;
            float bi = floor(dy / bh);
            float pw = 3.0 + 2.5 * hash1(bi + fi * 13.0);
            float px = u + hash1(bi + 2.0) * 9.0;
            float pid = floor(px / pw);
            float ph = hash1(pid * 1.7 + bi * 5.3 + fi * 11.0);
            vec3 fc = ph < 0.35 ? vec3(0.42, 0.6, 0.26) : (ph < 0.6 ? vec3(0.6, 0.7, 0.32) : (ph < 0.82 ? vec3(0.86, 0.74, 0.4) : vec3(0.3, 0.5, 0.24)));
            fc *= 1.0 - 0.1 * step(0.5, fract(px * 2.2)) * step(0.6, ph) * lod(0.45);       // furrows
            float hedge = min(min(fract(dy / bh), 1.0 - fract(dy / bh)) * bh, min(fract(px / pw), 1.0 - fract(px / pw)) * pw);
            fc = mix(fc, vec3(0.2, 0.36, 0.16), fillA(hedge - 0.045) * step(0.0, dy));
            fc = mix(fc, inkOf(fc), fillA(abs(d) - 0.025));
            c = mix(c, haze(dayLit(fc), hz), fillA(d));
        }
        if (q.y < hgt - 0.8) continue;   // (below the trees and the farms on the crest)
        // trees on the crest
        float tw = 2.3 - 0.4 * fi;
        float tid = floor(u / tw);
        if (hash1(tid + 5.0 + fi * 3.0) < 0.45 * dens) {
            float tc = (tid + 0.3 + 0.4 * hash1(tid + fi * 7.0)) * tw;
            float tr = (0.36 + 0.26 * hash1(tid + 3.0)) * (0.8 + 0.3 * fi);
            vec2 tq = vec2(u - tc, q.y - hillH(tc, fi, dens) - tr * 0.95);
            float td = min(length(tq) - tr, min(length(tq - vec2(tr * 0.55, -tr * 0.35)), length(tq + vec2(tr * 0.55, tr * 0.35))) - tr * 0.68);
            td = min(td, sdBox(tq - vec2(0.0, -tr * 1.0), vec2(0.05 + 0.02 * fi, tr * 0.5)));
            vec3 tcol = mix(vec3(0.2, 0.38, 0.16), vec3(0.36, 0.56, 0.22), smoothstep(-0.03, 0.03, dot(tq, vec2(0.7, 0.7)) - tr * 0.1));
            c = drawn(c, td, haze(dayLit(tcol), hz), haze(dayLit(inkOf(tcol)), hz), 0.022, 1.0);
        }
        // a farmhouse (white walls, a red roof) or a barn (red, its white trim) on the near row
        if (i == 1) {
            float fw = 17.0;
            float fid = floor(u / fw);
            if (hash1(fid + 77.0) < 0.5 * dens) {
                float fx = (fid + 0.3 + 0.4 * hash1(fid + 78.0)) * fw;
                vec2 fq = vec2(u - fx, q.y - hillH(fx, fi, dens) + 0.15);
                bool barn = hash1(fid + 79.0) < 0.4;
                vec2 hs = barn ? vec2(0.7, 0.5) : vec2(0.6, 0.38);
                float wall = sdBox(fq - vec2(0.0, hs.y), hs);
                float roof = max(fq.y - hs.y * 2.0 - (hs.x + 0.12 - abs(fq.x)) * (barn ? 0.7 : 0.55), -(fq.y - hs.y * 2.0 + 0.02));
                roof = max(roof, abs(fq.x) - hs.x - 0.15);
                vec3 wc = barn ? vec3(0.7, 0.24, 0.18) : vec3(0.92, 0.88, 0.8);
                vec3 rc = barn ? vec3(0.42, 0.34, 0.32) : vec3(0.75, 0.28, 0.2);
                vec3 b = wc * (0.85 + 0.2 * step(0.0, fq.x));
                b = mix(b, barn ? vec3(0.95) : vec3(0.18, 0.2, 0.26), fillA(sdBox(fq - vec2(barn ? 0.0 : -0.25, barn ? 0.35 : 0.42), barn ? vec2(0.22, 0.35) : vec2(0.1, 0.1))));
                c = drawn(c, wall, haze(dayLit(b), hz), haze(dayLit(inkOf(wc)), hz), 0.02, 1.0);
                c = drawn(c, roof, haze(dayLit(rc * (0.85 + 0.25 * step(0.0, fq.x))), hz), haze(dayLit(inkOf(rc)), hz), 0.02, 1.0);
            }
        }
    }
    return c;
}

// ---- the industrial district far away (layer k): chimneys banded red and white and their smoke, gas holders in
// their frames, cranes, the long roofs of the works with a few lit windows; misty, in two tones, inked
vec3 industry(vec2 p, vec3 c, float k) {
    float u = p.x + uD * k;
    float cw = 9.0;
    float id = floor(u / cw);
    float dens = zInd(xFar((id + 0.5) * cw - uD * k));
    if (max(dens, zInd(xFar((id - 1.5) * cw - uD * k))) <= 0.005) return c;   // (no plant here or two upwind: no smoke)
    float lx = (u / cw - id) * cw - cw * 0.5;
    float base = GY0 + 2.5;
    // the smoke: each chimney's plume leans downwind and widens as it rises, gone within two cells; summed over this
    // cell's chimney and the two upwind of it, so that no plume is cut off at a cell's edge (each plume's own noise)
    float smoke = 0.0;
    for (int j = 0; j < 3; j++) {
        float fj = float(j), idj = id - fj;
        float dj = zInd(xFar((idj + 0.5) * cw - uD * k));
        vec2 sp = vec2(lx + fj * cw - 1.5, p.y - base - (6.0 + 6.0 * hash1(idj + 2.0)));
        if (dj <= 0.005 || sp.y <= 0.0 || sp.y > 16.0) continue;
        float pl = smoothstep(0.0, 0.5, sp.y) * exp(-sq((sp.x - sp.y * 0.5) / (0.4 + sp.y * 0.22))) * exp(-sp.y * 0.14);
        pl *= (1.0 - smoothstep(9.0, 16.0, sp.y)) * dj;
        if (pl < 0.004) continue;
        smoke = max(smoke, pl * fbm(vec2(sp.x * 0.5 - uTime * 0.12 + idj * 7.3, sp.y * 0.35 - uTime * 0.08)));
    }
    vec3 smokeC = mix(uFog, vec3(1.0, 0.85, 0.8), 0.25);
    float sa = sat(smoke * 1.6) * 0.6;
    if (dens <= 0.005) return mix(c, smokeC, sa);
    vec3 sil = mix(mix(vec3(0.16, 0.11, 0.17), uFog, 0.42), uFog, 1.0 - dens);
    float ch = 6.0 + 6.0 * hash1(id + 2.0);
    float chim = sdBox(vec2(lx - 1.5, p.y - base - ch * 0.5), vec2(0.28 - (p.y - base) * 0.01, ch * 0.5));
    chim = min(chim, sdBox(vec2(lx - 1.5, p.y - base - ch + 0.1), vec2(0.36 - ch * 0.01, 0.1)));       // its lip
    float d = chim;
    float gh = 1e3;
    if (hash1(id + 5.0) > 0.5) {
        vec2 g = vec2(lx + 2.0, p.y - base - 1.6);
        float lat = min(abs(fract(g.x * 1.2 + g.y * 0.6) - 0.5), abs(fract(g.x * 1.2 - g.y * 0.6) - 0.5)) / 1.2;
        gh = sdBox(g, vec2(1.6, 1.4)) - 0.15;                                                       // the holder
        d = min(d, max(sdBox(g, vec2(1.85, 1.62)), -(lat - 0.04)));
        d = min(d, max(sdBox(g, vec2(1.9, 1.62)), -sdBox(g, vec2(1.75, 1.5))));
    }
    if (hash1(id + 8.0) > 0.55) {
        vec2 g = vec2(lx + 3.4, p.y - base);
        d = min(d, sdBox(g - vec2(0.0, 3.5), vec2(0.15, 3.5)));
        d = min(d, sdSeg(g, vec2(-1.5, 7.0), vec2(4.0, 7.0)) - 0.09);
        d = min(d, sdSeg(g, vec2(0.0, 7.6), vec2(3.6, 7.0)) - 0.03);
        d = min(d, sdSeg(g, vec2(3.4, 7.0), vec2(3.4, 5.6)) - 0.02);                                  // its hook
    }
    float works = p.y - base - 1.0 - 0.6 * step(0.5, fract(u * 0.15)) - 0.35 * fract(u * 1.1) * step(0.6, hash1(floor(u * 0.15) + 3.0));
    d = min(min(d, gh), works);
    float a = fillA(d - 0.02) * dens;
    if (a > 0.0) {
        vec3 b = sil;
        b = mix(b, sil * 1.18 + uSunCol * 0.04, fillA(gh) * step(0.0, lx + 2.0));                 // the holder's lit side
        b = mix(b, sil * 1.15 + uSunCol * 0.03, fillA(chim) * step(1.5, lx));
        float band = fillA(abs(p.y - base - ch + 0.75) - 0.22) + fillA(abs(p.y - base - ch + 1.6) - 0.22);
        b = mix(b, mix(vec3(0.75, 0.22, 0.18), uFog, 0.45), fillA(chim) * fillA(abs(p.y - base - ch + 0.75) - 0.22));
        b = mix(b, mix(vec3(0.92, 0.88, 0.85), uFog, 0.45), fillA(chim) * fillA(abs(p.y - base - ch + 1.18) - 0.2) * (1.0 - band));
        float gl;
        float wl = windowsLit(vec2(u, p.y - base - 0.2), vec2(0.7, 0.5), vec2(0.16, 0.1), 7.0, 0.3, gl) * fillA(works + 0.25);
        b += WARM * wl * 0.6 * uLamp;
        b = mix(b, mix(sil, INK, 0.45), fillA(abs(d) - 0.02) * 0.8);
        c = mix(c, b, a);
    }
    c = mix(c, smokeC, sa);
    vec2 rl = vec2(lx - 1.5, p.y - base - ch - 0.25);
    c += vec3(1.0, 0.15, 0.1) * exp(-dot(rl, rl) / 0.03) * (0.5 + 0.5 * sin(uTime * 1.3 + id * 2.0)) * 0.8 * dens;
    return c;
}

// the works nearer (layer k): halls with saw-tooth, gabled or flat roofs and their vents, rows of windows (a few lit
// at dawn), short stacks banded red and white, pairs of silos or a water tower on its legs, pipes running between
vec3 factories(vec2 p, vec3 c, float k) {
    float cw = 8.0;
    float u = p.x + uD * k + 2.3;
    float id = floor(u / cw);
    float dens = zInd(xMid((id + 0.5) * cw - 2.3 - uD * k));
    if (dens <= 0.005 || hash1(id + 0.3) > 0.88) return c;
    float lx = (u / cw - id) * cw;
    float base = GY0 + 1.0;
    float pal = hash1(id + 1.0);
    vec3 wall = pal < 0.35 ? vec3(0.62, 0.4, 0.36) : (pal < 0.7 ? vec3(0.42, 0.47, 0.56) : vec3(0.66, 0.58, 0.48));
    float hz = 0.36 + 0.4 * (1.0 - dens);
    vec3 ink = haze(dayLit(inkOf(wall)), hz);
    // on the right of the cell: two silos, or a water tower, or nothing
    float yard = hash1(id + 9.0);
    if (yard < 0.4) {
        for (int s = 0; s < 2; s++) {
            vec2 sq0 = vec2(lx - 6.45 - 0.82 * float(s), p.y - base);
            float sh = 3.2 + 0.6 * float(s);
            float sd = min(sdBox(sq0 - vec2(0.0, sh * 0.5), vec2(0.38, sh * 0.5)), length(vec2(sq0.x, (sq0.y - sh) * 1.8)) - 0.38);
            vec3 sc = vec3(0.78, 0.76, 0.72) * (0.82 + 0.25 * step(0.05, sq0.x));
            sc *= 1.0 - 0.1 * step(0.5, fract(sq0.y / 0.5)) * lod(0.5);
            c = drawn(c, sd, haze(dayLit(sc), hz), ink, 0.022, dens);
        }
    } else if (yard < 0.65) {
        vec2 tq = vec2(lx - 6.9, p.y - base);
        float legs = max(abs(abs(tq.x + tq.y * 0.06 - 0.25) - 0.3) - 0.04, abs(tq.y - 1.6) - 1.6);
        legs = min(legs, max(abs(tq.y - 1.6) - 0.03, abs(tq.x) - 0.65));
        float tank = min(sdBox(tq - vec2(0.0, 3.85), vec2(0.75, 0.65)), max(length(vec2(tq.x, (tq.y - 4.5) * 2.0)) - 0.8, -(tq.y - 4.5)));
        c = drawn(c, legs, ink, ink, 0.0, dens);
        c = drawn(c, tank, haze(dayLit(vec3(0.5, 0.56, 0.6) * (0.82 + 0.25 * step(0.0, tq.x))), hz), ink, 0.022, dens);
    }
    // the hall
    float hw = 2.0 + 0.8 * hash1(id + 2.0);
    vec2 q = vec2(lx - 0.3 - hw, p.y - base);
    float hh = 2.3 + 1.6 * hash1(id + 4.0);
    float rt = hash1(id + 5.0);
    float roof = rt < 0.4 ? hh + 0.7 * fract((q.x + hw) / 1.3) : (rt < 0.72 ? hh + 1.0 * (1.0 - abs(q.x) / hw) : hh);
    float hall = max(abs(q.x) - hw, q.y - roof);
    hall = max(hall, -q.y - 2.0);
    // the stack
    bool stack = hash1(id + 8.0) < 0.55;
    float sx = hw * (hash1(id + 6.0) < 0.5 ? -0.65 : 0.65);
    float sh = hh + 2.4 + 2.0 * hash1(id + 7.0);
    float st = stack ? sdBox(vec2(q.x - sx, q.y - sh * 0.5), vec2(0.24 - q.y * 0.008, sh * 0.5)) : 1e3;
)" +
R"(    // vents on a flat roof
    float vent = rt >= 0.72 ? min(sdBox(q - vec2(-hw * 0.4, hh + 0.25), vec2(0.3, 0.25)), sdBox(q - vec2(hw * 0.2, hh + 0.18), vec2(0.45, 0.18))) : 1e3;
    float sil = min(min(hall, st), vent);
    if (fillA(sil - 0.03) > 0.0) {
        vec3 b = wall * (0.92 + 0.08 * step(0.5, fract(q.x / 0.3))) ;
        b = mix(b, wall * 1.2, step(hh, q.y));                                                     // the roof catches the light
        if (rt < 0.4) b = mix(b, mix(uSkyHor, vec3(1.0), 0.3), step(hh, q.y) * smoothstep(0.85, 0.95, fract((q.x + hw) / 1.3)));
        float gl;
        float wl = windowsLit(vec2(q.x + hw, q.y - 0.5), vec2(0.8, 0.9), vec2(0.24, 0.26), id * 3.3, 0.25, gl);
        float inWall = step(0.35, q.y) * step(q.y, hh - 0.4) * step(abs(q.x), hw - 0.25);
        b = mix(b, wall * 0.45, gl * inWall * lod(0.8));
        b = mix(b, WARM * 1.1, wl * inWall * uLamp);
        // a door
        b = mix(b, wall * 0.55, fillA(sdBox(q - vec2(hw * 0.35, 0.8), vec2(0.45, 0.8))));
        b = mix(b, wall * 0.8, fillA(st));
        b = mix(b, vec3(0.92, 0.9, 0.86), fillA(st) * fillA(abs(q.y - sh + 0.55) - 0.18));
        b = mix(b, vec3(0.78, 0.25, 0.2), fillA(st) * fillA(abs(q.y - sh + 0.95) - 0.18));
        b = mix(b, wall * 0.75, fillA(vent));
        c = drawn(c, sil, haze(dayLit(b), hz), ink, 0.024, dens);
    }
    // a pipe on its trestles along the yard (across the cells)
    if (hash1(id + 11.0) < 0.6) {
        float py = base + 1.5 + 0.4 * hash1(id + 12.0);
        float pd = min(abs(p.y - py) - 0.12, max(abs(mod(lx, 2.0) - 1.0) - 0.05, max(p.y - py, base - p.y)));
        c = drawn(c, pd, haze(dayLit(vec3(0.55, 0.5, 0.45)), hz), ink, 0.02, dens);
    }
    return c;
}

// warehouses (layer k): corrugated walls, saw-tooth roofs glazed on their steep faces, a roller door, a painted sign,
// a high row of windows (a few lit), a downpipe; or stacks of shipping containers in faded colors, their ribs
vec3 warehouses(vec2 p, vec3 c, float k) {
    float u = p.x + uD * k + 7.0;
    float cw = 11.0;
    float id = floor(u / cw);
    float dens = zInd(((id + 0.5) * cw - 7.0) / k);
    if (hash1(id + 0.4) >= dens) return c;
    float lx = (u / cw - id) * cw;
    float base = GY0 + 1.3;
    float hz = 0.2;
    if (hash1(id + 1.4) < 0.3) {
        // containers: two or three columns, one to three high
        float col = floor((lx - 0.6) / 3.45);
        if (col < 0.0 || col > 2.0) return c;
        float n = 1.0 + floor(hash1(id + col * 3.0 + 2.0) * 3.0);
        vec2 q = vec2(lx - 0.6 - (col + 0.5) * 3.45, p.y - base + 0.3);
        float row = floor(q.y / 1.3);
        if (row < 0.0 || row >= n) return c;
        vec2 bq = vec2(q.x, q.y - (row + 0.5) * 1.3);
        float d = sdBox(bq, vec2(1.66, 0.62));
        float h = hash1(id + col * 7.0 + row * 13.0);
        vec3 cc = h < 0.25 ? vec3(0.72, 0.32, 0.24) : (h < 0.5 ? vec3(0.28, 0.46, 0.62) : (h < 0.72 ? vec3(0.32, 0.56, 0.42) : (h < 0.88 ? vec3(0.88, 0.66, 0.26) : vec3(0.84, 0.48, 0.24))));
        vec3 b = cc * (0.86 + 0.14 * step(0.5, fract(bq.x / 0.2)) * lod(0.2));
        b = mix(b, cc * 0.6, fillA(abs(abs(bq.x) - 1.5) - 0.06));                           // the corner posts
        b = mix(b, cc * 1.18, fillA(abs(bq.y - 0.52) - 0.05));                              // the top rail
        c = drawn(c, d, haze(dayLit(b), hz), haze(dayLit(inkOf(cc)), hz), 0.03, 1.0);
        return c;
    }
    float hgt = 2.6 + 1.4 * hash1(id);
    float saw = fract(lx / 1.6);
    float roofY = base + hgt + 0.9 * saw;
    float x0 = 0.6, x1 = 10.4;
    float d = max(p.y - roofY, max(x0 - lx, lx - x1));
    float pal = hash1(id + 3.0);
    vec3 wc = pal < 0.3 ? vec3(0.66, 0.38, 0.3) : (pal < 0.55 ? vec3(0.34, 0.52, 0.54) : (pal < 0.8 ? vec3(0.78, 0.7, 0.56) : vec3(0.42, 0.46, 0.6)));
    if (fillA(d - 0.03) <= 0.0) return c;
    vec2 q = vec2(lx, p.y - base);
    vec3 b = wc * (0.88 + 0.12 * step(0.5, fract(lx / 0.22)) * lod(0.22));                  // the corrugations
    b = mix(b, mix(uSkyHor, vec3(1.0), 0.35), step(hgt, q.y) * smoothstep(0.82, 0.9, saw));   // the glazed faces
    b = mix(b, wc * 1.15, step(hgt, q.y) * (1.0 - smoothstep(0.82, 0.9, saw)));
    b = mix(b, inkOf(wc), fillA(abs(q.y - hgt) - 0.03));
    // the roller door, its slats; the painted sign over it
    float dx = 2.2 + 5.0 * hash1(id + 4.0);
    vec2 dq = vec2(lx - dx, q.y - 1.0);
    float door = sdBox(dq, vec2(1.0, 1.0));
    b = mix(b, inkOf(wc), fillA(door - 0.05));
    b = mix(b, vec3(0.62, 0.62, 0.6) * (0.85 + 0.15 * step(0.5, fract(dq.y / 0.16))), fillA(door));
    b = mix(b, vec3(0.95, 0.75, 0.15), fillA(sdBox(dq - vec2(0.0, -0.95), vec2(1.0, 0.05))) * step(0.5, fract(dq.x * 2.0 + dq.y)));
    vec2 sq0 = vec2(lx - dx, q.y - 2.35);
    vec2 ss = vec2(1.15, 0.24);
    float sgn = sdBox(sq0, ss);
    b = mix(b, INK, fillA(sgn - 0.03));
    b = mix(b, vec3(0.92, 0.88, 0.78), fillA(sgn));
    b = mix(b, vec3(0.2, 0.25, 0.42), fillA(lettering(sq0, ss - vec2(0.12, 0.06), 5.0, id * 3.0, false) - 0.025) * fillA(sgn));
    // a row of windows high up (a few lit), a downpipe at one end
    float gl;
    float wl = windowsLit(vec2(lx - 0.1, q.y - 1.5), vec2(1.1, 1.2), vec2(0.3, 0.22), id * 9.0, 0.35, gl) ;
    float wIn = step(q.y, hgt - 0.3) * step(1.3, q.y) * step(1.4, abs(lx - dx));
    b = mix(b, wc * 0.42, gl * wIn * lod(1.1));
    b = mix(b, WARM * 1.1, wl * wIn * uLamp);
    b = mix(b, inkOf(wc), fillA(abs(lx - (x1 - 0.3)) - 0.05) * step(q.y, hgt));
    c = drawn(c, d, haze(dayLit(b), hz), haze(dayLit(inkOf(wc)), hz), 0.03, 1.0);
    return c;
}

// ---- the pylons striding over the fields (from the works), the wires sagging between them
vec3 pylons(vec2 p, vec3 c) {
    vec2 q0 = layerP(p, 0.45);
    float u = q0.x + uD * 0.45;
    float sp = 18.0;
    float n = floor(u / sp + 0.5);
    float dens = max(zField(n * sp / 0.45), zInd(n * sp / 0.45) * 0.5);
    float lx = u - n * sp;
    float base = GY0 + 1.0;
    vec2 q = vec2(lx, q0.y - base);
    vec3 pc = haze(dayLit(vec3(0.16, 0.17, 0.2)), 0.25);
    if (dens > 0.5) {
        float tw = 0.9 - q.y * 0.07;
        float d = max(abs(abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)) - 0.05, -q.y);
        d = max(d, q.y - 8.5);
        d = min(d, sdBox(q - vec2(0.0, 7.4), vec2(1.6, 0.07)));
        d = min(d, sdBox(q - vec2(0.0, 6.3), vec2(1.2, 0.07)));
        d = min(d, max(abs(abs(q.x) - 1.45) - 0.03, abs(q.y - 7.05) - 0.35));                         // the insulators
        d = min(d, max(abs(abs(q.x) - 1.05) - 0.03, abs(q.y - 5.95) - 0.35));
        float lat = min(abs(fract(q.y * 0.9 + q.x * 0.6) - 0.5), abs(fract(q.y * 0.9 - q.x * 0.6) - 0.5)) / 0.9;
        d = min(d, max(lat - 0.03, abs(q.x) - tw * 0.5 * (1.0 - q.y / 12.0)));
        d = max(max(d, -q.y), q.y - 8.6);
        c = lay(c, pc, fillA(d) * 0.95);
    }
    // the wires between this pylon and the next
    float nn = lx < 0.0 ? n - 1.0 : n;
    float da = max(zField(nn * sp / 0.45), zInd(nn * sp / 0.45) * 0.5), db = max(zField((nn + 1.0) * sp / 0.45), zInd((nn + 1.0) * sp / 0.45) * 0.5);
    if (da > 0.5 && db > 0.5) {
        float fx2 = fract(u / sp);
        for (int w = 0; w < 2; w++) {
            float wy = base + 6.7 - float(w) * 1.1 - 1.4 * 4.0 * fx2 * (1.0 - fx2);
            c = lay(c, pc, (1.0 - smoothstep(0.015, 0.015 + gPx, abs(q0.y - wy))) * 0.85);
        }
    }
    return c;
}

// ---- bamboo (layer k, density by place): culms in two tones with a pale stripe, their nodes inked, leaf sprays of
// narrow drooping leaves (some sunlit yellow-green, some in shade), each leaf outlined; `inkA` how much the layer is
// inked (none for the far silhouettes dissolving in the mist)
vec3 bambooClump(vec2 p, vec3 c, float k, float base, float scale, float dens0, vec3 col, vec3 leafLit, float hz, float seed, float inkA) {
    float u = p.x + uD * k + seed * 17.0;
    float cw = 1.25 * scale;
    float id0 = floor(u / cw);
    float t = 0.022 * inkA;
    for (int o = -1; o <= 1; o++) {
        float id = id0 + float(o);
        float fade = 1.0;
        float dens = dens0 * zBamboo(((id + 0.5) * cw - seed * 17.0) / k);
        if (k < 0.6) {
            float sx = (id + 0.5) * cw - seed * 17.0 - uD * k;
            float xe = k < 0.3 ? xFar(sx) : uD + sx * 1.7;
            float z = zBamboo(xe) * (1.0 - 0.85 * zGlade(xe));
            fade = smoothstep(0.0, 0.3, z);                    // (thinning out where the forest ends, not ghostly)
            dens = dens0 * sqrt(z);
        }
        float h = hash1(id + seed);
        if (dens <= 0.0 || h > dens) continue;   // (a hash can be 0)
        float x0 = (id + 0.5 + (hash1(id + seed + 1.0) - 0.5) * 0.6) * cw;
        float w = (0.13 + 0.1 * hash1(id + seed + 2.0)) * scale;
        float lean = (hash1(id + seed + 3.0) - 0.5) * 0.06;
        float yy = p.y - base;
        float sway = (0.15 * sin(uTime * 0.5 + id * 1.3) + 0.06 * sin(uTime * 1.1 + id)) * scale * sq(max(yy, 0.0) / 20.0);
        float x = u - x0 - lean * yy - sway;
        float d = max(abs(x) - w, base - p.y);
        if (k < 0.25) d = max(d, yy - (12.0 + 2.0 * hash1(id + seed + 4.0)) * scale);   // (far ones: a line of tops)
        if (d < t + gPx) {
            float seg = 1.6 * scale;
            float nyy = (fract((yy + hash1(id) * seg) / seg) - 0.5) * seg;
            vec3 cc = mix(col * 0.8, col * 1.18, smoothstep(-gPx, gPx, x + w * 0.1));
            cc = mix(cc, col * 1.45 + 0.03, fillA(abs(x - w * 0.5) - w * 0.12) * inkA);            // a pale stripe
            cc = mix(cc, col * 1.35, fillA(abs(nyy + 0.09 * scale) - 0.04 * scale) * inkA);         // the ring under a node
            cc = mix(cc, inkOf(col), fillA(abs(nyy) - max(0.025 * scale, gPx * 0.6)) * mix(0.6, 1.0, inkA));
            cc = mix(cc, inkOf(col), fillA(abs(d) - t) * inkA);
            c = mix(c, haze(dayLit(cc), hz), fillA(d - t) * fade);
        }
        if (yy < 3.6 * scale || yy > 13.4 * scale) continue;   // (above or below every spray)
        for (int l = 0; l < 3; l++) {
            float fl = float(l);
            float ly = (6.0 + 5.0 * hash1(id + fl * 7.0 + seed)) * scale;
            vec2 at = vec2(x0 + lean * ly + sway * 0.8, base + ly);
            vec2 q = vec2(u, p.y) - at;
            if (dot(q, q) > sq(2.4 * scale)) continue;
            float side = hash1(id + fl * 3.0) < 0.5 ? -1.0 : 1.0;
            q.x *= side;
            vec3 lc = hash1(id + fl * 5.0 + seed + 0.5) < 0.45 ? leafLit : col * vec3(0.7, 0.95, 0.7);
            q = rot(-0.2 + 0.05 * sin(uTime * 0.9 + id + fl)) * q;
            const mat2 STEP = mat2(0.9553365, -0.2955202, 0.2955202, 0.9553365);   // rot(-0.3)
            for (int b = 0; b < (inkA > 0.0 ? 4 : 3); b++) {
                float fb = float(b);
                vec2 r = q;
                q = STEP * q;
                float L = (1.15 + 0.25 * fb) * scale;
                float tt = sat(r.x / L);
                float lw = 0.15 * scale * sin(sqrt(tt) * PI) * (1.0 - tt * 0.3);
                float ld = max(abs(r.y + 0.12 * scale * tt * tt) - lw, max(-r.x, r.x - L));
                vec3 lcol = lc * (0.88 + 0.12 * fb / 3.0);
                lcol = mix(lcol, inkOf(lc), fillA(abs(ld) - t) * inkA);
                c = mix(c, haze(dayLit(lcol), hz), fillA(ld - t) * fade);
            }
        }
    }
    return c;
}

// the bamboo nearest of all (layer 1.7, in front of the way and the pandas: drawn in its own pass over them, its
// coverage in a): a few big dark culms and their leaves framing the view, fastest of all; quieter behind the well
vec3 nearBamboo(vec2 p, vec3 c, inout float a) {
    float k = 1.7;
    float u = p.x + uD * k;
    float cw = 9.0;
    float id0 = floor(u / cw);
    float calm = mix(0.3, 1.0, smoothstep(5.0, 9.0, abs(p.x - uViewC.x)));
    vec3 col = vec3(0.12, 0.19, 0.08);
    for (int o = -1; o <= 1; o++) {
        float id = id0 + float(o);
        float xe = ((id + 0.5) * cw) / k;
        float dens = zBamboo(xe) * (1.0 - zGlade(xe));
)" +
R"(        if (dens <= 0.0 || hash1(id + 0.5) > 0.6 * dens) continue;
        float x0 = (id + 0.2 + 0.6 * hash1(id + 1.0)) * cw;
        float wd = 0.5 + 0.3 * hash1(id + 2.0);
        float lean = (hash1(id + 3.0) - 0.5) * 0.1;
        float yy = p.y + 12.0;
        float x = u - x0 - lean * yy - 0.15 * sin(uTime * 0.4 + id) * sq(yy / 25.0);
        float d = abs(x) - wd;
        if (d < 0.1) {
            float seg = 3.4;
            float nyy = (fract((yy + hash1(id) * seg) / seg) - 0.5) * seg;
            vec3 cc = mix(col * 0.75, col * 1.25, smoothstep(-gPx, gPx, x + wd * 0.2));
            cc = mix(cc, col * 1.6, fillA(abs(x - wd * 0.5) - wd * 0.1));
            cc = mix(cc, col * 1.4, fillA(abs(nyy + 0.16) - 0.07));
            cc = mix(cc, inkOf(col), fillA(abs(nyy) - 0.05));
            cc = mix(cc, inkOf(col), fillA(abs(d) - 0.05));
            float ca = fillA(d - 0.05) * calm;
            c = mix(c, dayLit(cc), ca);
            a = mix(a, 1.0, ca);
        }
        for (int l = 0; l < 2; l++) {
            float fl = float(l);
            float ly = 13.0 + 6.0 * hash1(id + fl * 7.0);
            vec2 at = vec2(x0 + lean * ly, ly - 12.0);
            vec2 q = vec2(u, p.y) - at;
            if (dot(q, q) > 16.0) continue;
            q.x *= hash1(id + fl * 3.0) < 0.5 ? -1.0 : 1.0;
            vec3 lc = hash1(id + fl * 5.0 + 0.5) < 0.4 ? vec3(0.32, 0.42, 0.12) : vec3(0.1, 0.17, 0.07);
            q = rot(-0.25 + 0.04 * sin(uTime * 0.8 + id + fl)) * q;
            const mat2 STEP = mat2(0.9492, -0.3146, 0.3146, 0.9492);                // rot(-0.32)
            for (int b = 0; b < 4; b++) {
                float fb = float(b);
                vec2 r = q;
                q = STEP * q;
                float L = 2.4 + 0.5 * fb;
                float tt = sat(r.x / L);
                float lw = 0.32 * sin(sqrt(tt) * PI) * (1.0 - tt * 0.3);
                float ld = max(abs(r.y + 0.25 * tt * tt) - lw, max(-r.x, r.x - L));
                vec3 lcol = mix(lc * (0.9 + 0.1 * fb), inkOf(lc), fillA(abs(ld) - 0.035));
                float la = fillA(ld - 0.035) * calm;
                c = mix(c, dayLit(lcol), la);
                a = mix(a, 1.0, la);
            }
        }
    }
    return c;
}

// ---- along the way's edge (the near plane): stone lanterns grown with moss (lit at sunset), mossy rocks, ferns; in
// the forest and the glade, rocks in the fields
vec3 wayside(vec2 w, vec3 c) {
    if (w.x < 1376.0 || w.y > GY0 + 3.6) return c;
    float cw = 6.5;
    float id = floor(w.x / cw);
    float xe = (id + 0.5) * cw;
    if (abs(xe - 1482.5) < 8.0) return c;                                  // (the bridge)
    float fz = zBamboo(xe) + zGlade(xe);
    float fld = zField(xe) * (1.0 - zBamboo(xe));
    float h = hash1(id + 211.0);
    vec2 q = vec2(w.x - xe - (hash1(id + 212.0) - 0.5) * 2.5, w.y - GY0 + 0.15);
    float glow = 0.25 + 0.75 * zGlade(w.x) + 0.3 * uDrop;
    if (h < 0.26 * fz) {
        // a stone lantern (toro): a plinth, a post, the light box with its window, a wide roof, its eaves curling up,
        // the jewel on top; moss on the roof and the plinth
        q /= 0.9 + 0.2 * hash1(id + 213.0);
        float d = sdBox(q - vec2(0.0, 0.15), vec2(0.5, 0.15)) - 0.03;
        d = min(d, sdBox(q - vec2(0.0, 0.85), vec2(0.15 + 0.03 * sat(1.0 - q.y + 0.4), 0.6)));
        d = min(d, sdBox(q - vec2(0.0, 1.5), vec2(0.4, 0.07)) - 0.02);
        float box = sdBox(q - vec2(0.0, 1.88), vec2(0.3, 0.3));
        vec2 rq = q - vec2(0.0, 2.2);
        float yt = 0.42 - 0.5 * abs(rq.x), yb = 0.2 * sq(rq.x / 0.85);
        float roof = max(max(rq.y - yt, yb - rq.y), abs(rq.x) - 0.85);
        float top = length(q - vec2(0.0, 2.7)) - 0.12;
        float all = min(min(d, box), min(roof, top));
        if (all > 0.1) return c;
        vec3 st = vec3(0.6, 0.6, 0.54) * (0.82 + 0.25 * smoothstep(-gPx, gPx, q.x - 0.05)) * (0.9 + 0.15 * vnoise(q * 6.0));
        vec3 moss = vec3(0.36, 0.52, 0.16) * (0.85 + 0.3 * vnoise(q * 9.0));
        float win = sdBox(q - vec2(0.0, 1.88), vec2(0.13, 0.17));
        vec3 b = st;
        b = mix(b, moss, step(roof, 0.0) * step(yt - 0.13 - 0.1 * vnoise(vec2(rq.x * 8.0, 0.0)), rq.y));
        b = mix(b, moss, step(d, 0.0) * step(q.y, 0.34) * step(0.55, vnoise(q * vec2(5.0, 3.0) + id)));
        b = mix(b, mix(vec3(0.1, 0.08, 0.06), vec3(1.0, 0.75, 0.35) * 1.3, glow), fillA(win));
        c = drawn(c, all, dayLit(b), dayLit(inkOf(st)), 0.025, 1.0);
        vec2 gq = q - vec2(0.0, 1.88);
        c += vec3(1.0, 0.65, 0.3) * exp(-dot(gq, gq) / 0.6) * 0.35 * glow * zGlade(w.x);
        return c;
    }
    if (h < 0.26 * fz + 0.32 * (fz + fld)) {
        // mossy rocks: one or two, moss on their tops
        float r = 0.4 + 0.3 * hash1(id + 214.0);
        float d = sdEll(q - vec2(0.0, r * 0.5), vec2(r * 1.35, r));
        if (hash1(id + 215.0) < 0.5) d = min(d, sdEll(q - vec2(r * 1.3, r * 0.25), vec2(r * 0.8, r * 0.55)));
        if (d > 0.1) return c;
        vec3 st = vec3(0.5, 0.5, 0.47) * (0.8 + 0.25 * smoothstep(-0.05, 0.05, dot(q, vec2(0.7, 0.7)) - r * 0.2));
        vec3 b = mix(st, vec3(0.34, 0.5, 0.16) * (0.85 + 0.3 * vnoise(q * 8.0)), step(r * 0.55 + 0.25 * vnoise(vec2(q.x * 4.0, id)) - 0.1, q.y) * fz);
        return drawn(c, d, dayLit(b), dayLit(inkOf(st)), 0.025, 1.0);
    }
    if (h < 0.26 * fz + 0.32 * (fz + fld) + 0.3 * fz) {
        // a fern: five fronds curling out and down, their leaflets
        if (abs(q.x) > 2.6 || q.y > 2.4) return c;
        float dmin = 1e3;
        for (int f = 0; f < 5; f++) {
            float ff = float(f);
            float a = (ff - 2.0) * 0.5 + (hash1(id + ff) - 0.5) * 0.2;
            vec2 dir = vec2(sin(a), cos(a)), nrm = vec2(cos(a), -sin(a));
            float L = 1.6 + 0.5 * hash1(id + ff + 9.0) - 0.25 * abs(ff - 2.0);
            vec2 r = vec2(dot(q, dir), dot(q, nrm));
            float tt = sat(r.x / L);
            float dy = r.y - sign(a) * 0.4 * L * tt * tt;
            float wdt = 0.26 * (1.0 - tt) * (0.45 + 0.55 * abs(sin(r.x * 13.0))) + 0.02;
            dmin = min(dmin, max(abs(dy) - wdt, max(-r.x, r.x - L)));
        }
        vec3 fc = mix(vec3(0.18, 0.36, 0.12), vec3(0.4, 0.6, 0.2), step(0.0, q.x));
        return drawn(c, dmin, dayLit(fc), dayLit(inkOf(fc)), 0.018, 1.0);
    }
    return c;
}

// the front of the scene in the countryside (layer k > 1): tall grass and wildflowers dark against the light, a
// fence post now and then (the fields, the glade)
vec3 grassFront(vec2 p, vec2 w, vec3 c, float k) {
    float cw = 7.0;
    float u = p.x + uD * k;
    float id = floor(u / cw);
    float xe = ((id + 0.5) * cw) / k;
    float dens = zField(xe) * (1.0 - zBamboo(xe)) + zGlade(xe) * 0.8;
    if (dens <= 0.01 || hash1(id + 81.0) > 0.7 * dens) return c;
    float lx = u - (id + 0.5) * cw + (hash1(id + 82.0) - 0.5) * 3.0;
    vec2 q = vec2(lx, w.y - (GY0 - 2.1));
    if (abs(q.x) > 2.2 || q.y > 3.4) return c;
    float d = 1e3;
    for (int b = 0; b < 7; b++) {
        float fb = float(b);
        float hb = 1.2 + 1.6 * hash1(id + fb * 3.0);
        float bx = (fb - 3.0) * 0.22 + (hash1(id + fb) - 0.5) * 0.2;
        float lean = (fb - 3.0) * 0.12 + 0.08 * sin(uTime * 1.1 + fb + id);
        float tt = sat(q.y / hb);
        float x = q.x - bx - lean * tt * tt * hb;
        d = min(d, max(abs(x) - 0.08 * (1.0 - tt) - 0.01, max(q.y - hb, -q.y)));
    }
    float fl = 1e3;
    if (hash1(id + 83.0) < 0.6) {
        vec2 fq = q - vec2(0.5, 2.4 + 0.5 * hash1(id + 84.0));
        d = min(d, max(abs(q.x - 0.5) - 0.025, max(-q.y, q.y - (2.4 + 0.5 * hash1(id + 84.0)))));
        float ang = atan(fq.y, fq.x);
        fl = length(fq) - 0.26 - 0.08 * cos(ang * 8.0);
    }
    vec3 sc = mix(vec3(0.03, 0.06, 0.03), mix(uFog, uSunCol, 0.4) * 0.12, 0.4) * (0.6 + 0.4 * uDawn);
    c = mix(c, sc, fillA(min(d, fl)));
    c += uSunCol * 0.15 * fillA(min(d, fl)) * (1.0 - fillA(min(d, fl) + 0.04)) * zGlade(w.x);   // a rim of light at sunset
    c = mix(c, mix(vec3(0.95, 0.85, 0.5), sc, 0.55), fillA(length(q - vec2(0.5, 2.4 + 0.5 * hash1(id + 84.0))) - 0.09) * step(fl, 0.5));
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
    if (COUNTRY) c = mountains(p, c);
    if (CITY) c = cityFar(layerP(p, 0.06), c, 0.06);
    if (COUNTRY) c = industry(layerP(p, 0.1), c, 0.1);
    if (COUNTRY) c = hills(p, c);
    // the forest's far bamboo, silhouettes dissolving into the mist; banks of mist drifting between the rows
    const vec3 B_COL = vec3(0.5, 0.6, 0.26), B_LIT = vec3(0.8, 0.88, 0.34);
    if (COUNTRY) c = bambooClump(layerP(p, 0.1), c, 0.1, GY0 + 2.6, 0.5, 0.9, mix(B_COL, uFog, 0.55), mix(B_LIT, uFog, 0.55), 0.6, 1.0, 0.0);
    float fz = zBamboo(xFar(p.x));
    if (COUNTRY && fz > 0.01) {
        vec2 mq = vec2((p.x + uD * 0.14) * 0.06 + uTime * 0.015, p.y * 0.22);
        float m = 0.67 * vnoise(mq) + 0.33 * vnoise(mq * 2.3 + 1.7);
        c = mix(c, uFog * 1.04, fz * smoothstep(0.4, 0.75, m) * (1.0 - smoothstep(GY0 + 1.0, GY0 + 10.0, p.y)) * 0.6);
    }
    if (CITY) c = cityMid(layerP(p, 0.16), c, 0.16);
    if (COUNTRY) c = bambooClump(layerP(p, 0.18), c, 0.18, GY0 + 2.0, 0.65, 0.85, mix(B_COL, uFog, 0.3), mix(B_LIT, uFog, 0.3), 0.4, 3.0, 0.35);
    if (COUNTRY) c = factories(layerP(p, 0.22), c, 0.22);
    if (CITY) c = viaduct(layerP(p, 0.28), c, 0.28);
    if (COUNTRY) c = warehouses(layerP(p, 0.4), c, 0.4);
    if (CITY) c = cityStreet(layerP(p, 0.5), c, 0.5);
    if (COUNTRY) c = pylons(p, c);
    if (COUNTRY) c = bambooClump(layerP(p, 0.3), c, 0.3, GY0 + 1.5, 0.8, 0.75 * (1.0 - 0.5 * zGlade(w.x)), B_COL * 0.9, B_LIT, 0.18, 2.0, 0.8);
    // shafts of light in the forest
    float shz = zBamboo(w.x) + 0.5 * zGlade(w.x);
    if (COUNTRY && shz > 0.01) {
        vec2 sq0 = rot(-0.45) * (p - vec2(-30.0, 14.0));
        float sh = sq(sat(vnoise(vec2((sq0.x + uD * 0.15) * 0.35, uTime * 0.05)) * 1.4 - 0.45));
        c += uSunCol * sh * 0.5 * shz * smoothstep(-12.0, 4.0, p.y);
    }
    if (CITY) c = passers(layerP(p, 0.8), c, 0.8, vec3(1.0, 0.6, 0.5));
    if (COUNTRY) c = bambooClump(layerP(p, 0.5), c, 0.5, GY0 + 1.0, 1.0, 0.5, vec3(0.42, 0.55, 0.2), vec3(0.74, 0.84, 0.3), 0.06, 5.0, 1.0);
    // the meadow behind the path: grass and flowers (fields, forest, glade), the stream coming to the little bridge
    if (COUNTRY) {
        float nat = smoothstep(1366.0, 1386.0, w.x);
        if (nat > 0.01 && w.y < GY0 + 2.0) {
            float top = GY0 + 0.5 + 0.25 * sin(w.x * 0.4);
            vec3 mc = mix(vec3(0.36, 0.56, 0.2), vec3(0.46, 0.64, 0.24), step(0.55, vnoise(vec2(w.x * 0.3, w.y * 2.0))));
            mc = mix(mc, vec3(0.3, 0.44, 0.15), zBamboo(w.x) * 0.6);
            mc = mix(mc, vec3(0.52, 0.5, 0.22), zGlade(w.x) * 0.4);
            c = drawn(c, w.y - top, dayLit(mc), dayLit(inkOf(mc)), 0.02, nat);
            if (abs(w.x - 1482.5) < 6.0) {
                float sy = sat((w.y - GY0) / 1.0);
                float sd = max(abs(w.x - 1482.5 - 0.8 * sin(sy * 3.0)) - mix(3.8, 1.0, sy), w.y - top);
                vec3 wc = mix(uSkyHor, vec3(0.55, 0.85, 0.85), 0.6);
                wc *= 0.85 + 0.25 * step(0.62, vnoise(vec2(w.x * 2.0, w.y * 14.0 + uTime * 1.5)));
                c = drawn(c, sd, dayLit(wc), dayLit(vec3(0.16, 0.26, 0.1)), 0.035, nat);
            }
)" +
R"(            vec2 fq = vec2(w.x / 0.55, (w.y - GY0) / 0.5);
            vec2 fi = floor(fq), ff = fract(fq) - 0.5;
            vec2 fo = (hash2(fi) - 0.5) * 0.6;
            float on = step(0.72, hash(fi + 2.0)) * step(w.y, top - 0.1) * nat * (1.0 - 0.7 * zBamboo(w.x)) * step(5.0, abs(w.x - 1482.5));
            vec3 flc = hash(fi + 5.0) < 0.5 ? vec3(1.0, 0.95, 0.85) : (hash(fi + 6.0) < 0.5 ? vec3(1.0, 0.8, 0.25) : vec3(1.0, 0.55, 0.7));
            float fd = length(ff - fo) * 0.5 - 0.06;
            c = lay(c, dayLit(inkOf(flc)), fillS(fd - 0.015, gPx) * on);
            c = lay(c, dayLit(flc), fillS(fd, gPx) * on);
            c = lay(c, dayLit(vec3(0.3, 0.5, 0.18)), grassTufts(w, top - 0.15, 0.7 + 0.6 * zGlade(w.x), 0.55 * nat, 1.0));
        }
    }
    if (COUNTRY) c = bambooClump(layerP(p, 0.8), c, 0.8, GY0 + 0.5, 1.35, 0.35 * (1.0 - 0.8 * zGlade(w.x)), vec3(0.24, 0.34, 0.12), vec3(0.5, 0.64, 0.2), 0.0, 9.0, 1.0);
    if (COUNTRY) c = wayside(w, c);
    // ---- on the near plane: lamps, the subway's entrance, the crossing's light, the station's canopy, bollards
    if (CITY && w.x < 1196.0) {
        float sp = 15.0, n = floor((w.x - 4.0) / sp + 0.5);
        float lxw = n * sp + 4.0;
        float lx = w.x - lxw;
        if (abs(lxw - 86.5) > 9.0) c = lampAt(w, c, lx, vec3(1.0, 0.72, 0.38), 6.6, uLamp);
    } else if (COUNTRY && w.x > 1332.0 && w.x < 1381.0) {
        float sp = 13.0, n = floor((w.x - 2.0) / sp + 0.5);
        c = lampAt(w, c, w.x - (n * sp + 2.0), vec3(1.0, 0.55, 0.2), 5.6, uLamp);
    }
    // the subway's entrance: railings round the stairwell, a lit sign on a post
    if (CITY) {
        vec2 q = w - vec2(79.0, GY0);
        float rail = min(sdBox(q - vec2(0.7, 1.0), vec2(0.06, 1.0)), sdBox(q - vec2(14.2, 1.0), vec2(0.06, 1.0)));
        rail = min(rail, sdBox(q - vec2(7.45, 1.95), vec2(6.75, 0.05)));
        rail = min(rail, max(abs(mod(q.x - 0.7, 1.7) - 0.85) - 0.03, max(abs(q.y - 1.0) - 0.95, abs(q.x - 7.45) - 6.75)));
        // the canopy: glass on a light frame, on two posts
        float cp = min(sdBox(q - vec2(0.3, 2.8), vec2(0.08, 2.8)), sdBox(q - vec2(14.4, 2.4), vec2(0.08, 2.4)));
        vec2 cq = q - vec2(7.35, 5.2 + (q.x - 7.35) * -0.03);
        float roofF = sdBox(cq, vec2(7.4, 0.1));
        c = mix(c, mix(c, vec3(0.5, 0.7, 0.8) * (0.2 + 0.3 * uLamp), 0.35), fillA(sdBox(cq - vec2(0.0, 0.25), vec2(7.3, 0.2))));
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
    if (CITY) {
        float lx = w.x - 58.0;
        float d = min(sdBox(vec2(lx, w.y - GY0 - 2.6), vec2(0.07, 2.6)), sdBox(vec2(lx, w.y - GY0 - 5.6), vec2(0.32, 0.65)) - 0.06);
        c = lay(c, vec3(0.025, 0.028, 0.035), fillA(d));
        float red = step(fract(uTime / 14.0), 0.55), grn = 1.0 - red;
        vec2 r1 = vec2(lx, w.y - GY0 - 5.9), r2 = vec2(lx, w.y - GY0 - 5.3);
        c += vec3(1.0, 0.18, 0.12) * red * (fillA(length(r1) - 0.17) * 1.5 + exp(-dot(r1, r1) / 0.4) * 0.6);
        c += vec3(0.2, 1.0, 0.6) * grn * (fillA(length(r2) - 0.17) * 1.5 + exp(-dot(r2, r2) / 0.4) * 0.6);
    }
    // the terminus: a canopy on posts over the platform (its fascia catching the dawn, knee braces), lamps hanging
    // in each bay, the station's name board on its rods
    if (COUNTRY && w.x > 1286.0 && w.x < 1348.0) {
        vec2 q = w - vec2(1317.0, GY0);
        float px = mod(q.x + 26.0, 6.5) - 3.25;
        float roof = sdBox(q - vec2(0.0, 8.65), vec2(26.0, 0.24));
        float post = max(abs(px) - 0.11, abs(q.y - 4.3) - 4.3);
        post = min(post, max(abs(abs(px) - (q.y - 7.3)) * 0.7071 - 0.06, abs(q.y - 7.85) - 0.55));
        post = max(post, abs(q.x) - 26.0);
        vec3 pc = dayLit(vec3(0.26, 0.32, 0.38));
        vec3 rc = dayLit(vec3(0.46, 0.54, 0.6));
        c = drawn(c, post, pc * (0.85 + 0.3 * step(0.0, px)), INK, 0.03, 1.0);
        c = drawn(c, roof, mix(rc * 0.7, mix(rc * 1.2, uSunCol, 0.2), step(8.68, q.y)), INK, 0.035, 1.0);
        float bx = mod(q.x + 26.0 + 3.25, 6.5) - 3.25;
        vec2 lq = vec2(bx, q.y - 7.75);
        float shade = max(sdBox(lq, vec2(0.42 - lq.y * 0.35, 0.18)), -lq.y - 0.18);
        shade = min(shade, max(abs(bx) - 0.025, abs(q.y - 8.15) - 0.25));
        float inBay = step(abs(q.x), 25.0);
        c = drawn(c, shade, dayLit(vec3(0.2, 0.3, 0.28)), INK, 0.025, inBay);
        vec2 gq = lq + vec2(0.0, 0.22);
        c += vec3(1.0, 0.85, 0.6) * (fillA(sdEll(gq, vec2(0.3, 0.07))) * 1.2 + exp(-dot(gq, gq) / 2.0) * 0.12) * inBay * (0.4 + 0.6 * uLamp);
        // the station's name board
        vec2 nq = q - vec2(16.0, 5.8);
        c = drawn(c, max(abs(abs(nq.x) - 1.8) - 0.04, abs(nq.y - 1.4) - 1.0), INK, INK, 0.0, 1.0);
        c = drawn(c, sdBox(nq, vec2(2.4, 0.5)) - 0.05, vec3(0.06, 0.12, 0.26) * (0.6 + 0.4 * uDawn), INK, 0.035, 1.0);
        c = lay(c, vec3(0.88, 0.9, 0.95), fillA(lettering(nq, vec2(2.0, 0.32), 6.0, 41.0, false) - 0.04));
    }
    c = catenary(w, c);
    // the terminus: the buffer stop at the end of the track (red and white, its red lamp), the way out to the street
    // (a gate, a lit sign)
    if (COUNTRY) {
        vec2 q = w - vec2(1333.0, GY0);
        float bs = min(sdBox(q - vec2(0.0, 1.0), vec2(0.38, 0.35)), sdBox(q - vec2(0.0, 0.4), vec2(0.13, 0.45)));
        bs = min(bs, sdSeg(q, vec2(-0.2, 0.75), vec2(-0.9, 0.0)) - 0.07);
        vec3 bc = mix(vec3(0.72, 0.14, 0.1), vec3(0.92, 0.9, 0.86), fillA(abs(q.y - 1.0) - 0.09) * step(abs(q.x), 0.38));
        c = drawn(c, bs, dayLit(bc), INK, 0.03, 1.0);
        c += vec3(1.0, 0.15, 0.1) * exp(-dot(q - vec2(0.0, 1.55), q - vec2(0.0, 1.55)) / 0.03) * 0.8;
        vec2 g = w - vec2(1344.5, GY0);
        float gate = min(min(sdBox(g - vec2(-1.6, 2.6), vec2(0.1, 2.6)), sdBox(g - vec2(1.6, 2.6), vec2(0.1, 2.6))), sdBox(g - vec2(0.0, 5.1), vec2(1.75, 0.12)));
        gate = min(gate, max(abs(mod(g.x + 1.6, 0.4) - 0.2) - 0.03, max(abs(g.y - 0.75) - 0.75, abs(g.x) - 1.5)));      // its railings
        gate = min(gate, max(abs(g.y - 1.45) - 0.04, abs(g.x) - 1.5));
        c = drawn(c, gate, dayLit(vec3(0.24, 0.3, 0.34)), INK, 0.025, 1.0);
        vec2 sq1 = g - vec2(0.0, 5.75);
        float sgn = sdBox(sq1, vec2(1.1, 0.42)) - 0.05;
        c = drawn(c, sgn, vec3(0.1, 0.55, 0.3) * (0.6 + 0.6 * uLamp), INK, 0.03, 1.0);
        c = lay(c, vec3(0.95), fillA(sdBox(vec2(fract(sq1.x * 1.4 + 0.5) - 0.5, sq1.y), vec2(0.18, 0.2))) * step(abs(sq1.x), 0.8) * fillA(sgn));
        c += vec3(0.3, 1.0, 0.6) * exp(-max(sgn, 0.0) * 2.0) * 0.12 * uLamp;
    }
    // a wooden fence where the town gives way to the fields
    if (COUNTRY && w.x > 1366.0 && w.x < 1398.0) {
        float fx = mod(w.x, 2.2) - 1.1;
        float f = min(sdBox(vec2(fx, w.y - GY0 - 0.8), vec2(0.07, 0.8)), min(sdBox(vec2(fx, w.y - GY0 - 1.25), vec2(1.1, 0.05)), sdBox(vec2(fx, w.y - GY0 - 0.6), vec2(1.1, 0.05))));
        f = min(f, max(length(vec2(fx, w.y - GY0 - 1.6)) - 0.1, -(w.y - GY0 - 1.6)) );
        vec3 fc = vec3(0.5, 0.36, 0.24);
        c = drawn(c, f, dayLit(fc * (0.85 + 0.2 * step(0.0, fx))), dayLit(inkOf(fc)), 0.022, (1.0 - smoothstep(1388.0, 1398.0, w.x)) * smoothstep(1366.0, 1372.0, w.x));
    }
    // the little bridge over the stream: its posts and its rail, in two tones of weathered wood, inked
    if (COUNTRY && w.x > 1476.5 && w.x < 1488.5) {
        vec2 q = w - vec2(1482.5, GY0);
        float r = min(sdBox(q - vec2(0.0, 1.1), vec2(5.1, 0.08)), max(abs(mod(q.x + 5.0, 2.5) - 1.25) - 0.09, abs(q.y - 0.6) - 0.65));
        r = max(r, abs(q.x) - 5.2);
        vec3 wc = vec3(0.56, 0.4, 0.26);
        c = drawn(c, r, dayLit(wc * (0.85 + 0.25 * step(0.0, mod(q.x + 5.0, 2.5) - 1.25)) * (1.0 + 0.15 * step(1.1, q.y))), dayLit(inkOf(wc)), 0.024, 1.0);
    }
    return c;
}

// =========================================================================== underground
// The stairs: eleven steps down from the pavement's edge (a step a quarter of the cub's height), the station's
// platform, the tunnels' track bed (the one ahead runs on level under the city for over a thousand units: the ride's
// long run); then a grade (under 7 degrees) out of a portal up a walled cutting.
const float STAIR_X = 80.0, TREAD = 1.3, NSTEPS = 11.0;
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
    if (w.x >= 146.0 && w.x < 1191.0) { fl = trackY(w.x) - 0.4; ce = trackY(w.x) + 7.4; if (w.y > fl - 1.6 && w.y < ce) return 4; }
    if (w.x >= 1191.0 && w.x < 1298.0) { fl = trackY(w.x) - 0.4; ce = GY0; if (w.y > fl - 1.6 && w.y < GY0 + 0.01) return 5; }
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
    if (w.x >= 146.0 && w.x < 1191.6) a = max(a, step(trackY(w.x) - 2.0, w.y) * step(w.y, trackY(w.x) + 7.9));
    a = max(a, step(GY0 - 2.3, w.y));   // the street's slab under the pavement
    return a;
}
vec3 concrete(vec2 w) {
    vec3 c = vec3(0.075, 0.074, 0.072) * (0.85 + 0.25 * vnoise(w * 3.0) + 0.1 * vnoise(w * 11.0));
    c *= 1.0 - 0.25 * (1.0 - smoothstep(0.0, 0.04 + gPx, abs(fract(w.x / 3.0) - 0.5) * 3.0 - 1.45)) * lod(3.0);
    return c * (0.8 + 0.4 * uDawn);
}
// a pipe across the earth (its axis at y, radius r): a lit top, a dark underside, an outline; a flange every `fl`
vec3 pipeAt(vec2 w, vec3 c, float y, float r, vec3 col, float fl) {
    float d = abs(w.y - y) - r;
    if (d > gPx * 2.0 + 0.12) return c;
    float t = (w.y - y) / r;
    vec3 pc = col * (0.55 + 0.6 * smoothstep(-1.0, 0.7, t)) + col * 0.8 * exp(-sq((t - 0.45) / 0.18));
    float fx = abs(mod(w.x, fl) - fl * 0.5) - 0.1;
    float fd = max(fx, abs(w.y - y) - r - 0.1);
    c = mix(c, INK, fillA(min(d, fd) - 0.04));
    c = mix(c, pc, fillA(d));
    return mix(c, col * 1.2, fillA(fd) * 0.8);
}
// the services in the earth under the street (as far as the tunnel runs): a big water main with its flanges, a bundle
)" +
R"(// of conduits, cables sagging between their hangers, a sewer pipe below (glowing green through its cracks here and
// there), pipes dropping from the street now and then
vec3 services(vec2 w, vec3 c) {
    if (w.x > 1186.0 || w.y > GY0 - 2.3 || w.y < PLAT_Y + 7.6) return c;
    float y0 = GY0 - 2.3;
    // the drops: a pipe down from the street every so often, an elbow into the main
    float vx = mod(w.x - 7.0, 23.0) - 11.5;
    float vd = max(abs(vx) - 0.14, max(w.y - y0, (y0 - 0.75) - w.y));
    c = mix(c, INK, fillA(vd - 0.04));
    c = mix(c, vec3(0.09, 0.09, 0.085) * (0.7 + 0.5 * smoothstep(-0.14, 0.1, vx)), fillA(vd));
    c = pipeAt(w, c, y0 - 0.9, 0.3, vec3(0.08, 0.085, 0.09), 6.0);
    c = pipeAt(w, c, y0 - 1.48, 0.1, vec3(0.1, 0.06, 0.04), 9.0);
    c = pipeAt(w, c, y0 - 1.72, 0.09, vec3(0.06, 0.08, 0.07), 9.0);
    c = pipeAt(w, c, y0 - 1.94, 0.09, vec3(0.08, 0.08, 0.08), 9.0);
    // cables sagging between hangers (a hanger every 5)
    float s = fract(w.x / 5.0);
    for (int i = 0; i < 2; i++) {
        float fi = float(i);
        float cy = y0 - 0.32 - 0.12 * fi - (0.35 + 0.1 * fi) * 4.0 * s * (1.0 - s);
        c = mix(c, vec3(0.012, 0.01, 0.012), fillA(abs(w.y - cy) - 0.035));
    }
    c = mix(c, INK, fillA(max(abs(mod(w.x, 5.0) - 2.5) - 0.04, abs(w.y - (y0 - 0.25)) - 0.25)) * step(2.4, abs(mod(w.x, 5.0) - 2.5)));
    // the sewer: a wide pipe below, its glow where it is cracked
    float sy = y0 - 2.5;
    c = pipeAt(w, c, sy, 0.38, vec3(0.07, 0.065, 0.06), 12.0);
    float sid = floor(w.x / 40.0);
    if (hash1(sid + 91.0) < 0.35 && abs(w.y - sy) < 0.36) {
        float cr = abs(vnoise(vec2(w.x * 1.4, (w.y - sy) * 6.0 + sid)) - 0.5);
        float span = 1.0 - smoothstep(6.0, 14.0, abs(w.x - (sid + 0.5) * 40.0));
        vec3 g = vec3(0.25, 1.0, 0.5);
        c = mix(c, g * 1.3, (1.0 - smoothstep(0.02, 0.05, cr)) * span);
        c += g * exp(-cr * 18.0) * 0.18 * span;
    }
    return c;
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
    c = services(w, c);
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
    if (w.x < 1191.0 || w.x > 1336.0) return c;
    float ty = trackY(w.x);
    float px = mod(w.x - 1193.0, 12.0) - 6.0;
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
        // the hall: pale tiles (grimier low down), graffiti, a band of color, the station's name, posters, vending
        // machines, benches, a ticket machine, commuters waiting, strip lights hanging from the ceiling, the track bed
        float yy = w.y - PLAT_Y;
        vec3 c = tiles(w, vec3(0.3, 0.285, 0.24)) * (0.75 + 0.25 * smoothstep(0.8, 3.0, yy));
        c = mix(c, vec3(0.025, 0.07, 0.07), fillA(abs(yy - 3.9) - 0.4));
        c = mix(c, INK, fillA(abs(abs(yy - 3.9) - 0.4) - 0.03) * 0.6);
        // graffiti: blobs of letters sprayed on the lower wall, outlined, here and there
        float gid = floor(w.x / 9.0);
        if (hash1(gid + 81.0) < 0.5 && yy > 0.9 && yy < 3.2) {
            vec2 gq = vec2(w.x - (gid + 0.5) * 9.0, yy - 2.0);
            float blob = fbm(vec2(w.x * 1.1, yy * 1.6) + gid * 3.7) - 0.5 + 0.22 * (1.0 - length(gq / vec2(2.6, 0.95)));
            vec3 gc = neonCol(hash1(gid + 82.0)) * 0.42;
            c = mix(c, INK, fillA(-blob + 0.035) * 0.8);
            c = mix(c, gc, fillA(-blob + 0.07));
        }
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
        float kind = hash1(pid + 51.0);
        if (kind < 0.45) {
            c = mix(c, INK, fillA(sdBox(pq, vec2(1.15, 1.35))));
            c = mix(c, pic * 0.45, fillA(sdBox(pq, vec2(1.0, 1.2))));
        } else if (kind < 0.75) {
            // two vending machines, lit, their rows of cans
            for (int i = 0; i < 2; i++) {
                vec2 vq = pq - vec2(-0.75 + 1.5 * float(i), -0.3);
                float vd = sdBox(vq, vec2(0.66, 1.75)) - 0.05;
                vec3 vc = mix(vec3(0.35, 0.12, 0.18), vec3(0.12, 0.25, 0.22), float(i));
                vec3 win = vec3(0.8, 0.85, 0.75) * 0.8;
                vec2 cq = vec2(fract((vq.x + 0.42) / 0.21) - 0.5, fract((vq.y - 0.1) / 0.36) - 0.5);
                win = mix(win, neonCol(hash1(pid + floor((vq.y - 0.1) / 0.36) + float(i) * 5.0)) * 0.5, fillA(sdBox(cq * vec2(0.21, 0.36), vec2(0.06, 0.1))));
                c = mix(c, INK, fillA(vd - 0.05));
                c = mix(c, vc, fillA(vd));
                c = mix(c, win, fillA(sdBox(vq - vec2(-0.08, 0.55), vec2(0.42, 0.95))));
                c += vec3(0.6, 0.7, 0.6) * exp(-max(vd, 0.0) * 2.0) * 0.06;
            }
        } else if (kind < 0.9) {
            // a bench, a poster over it
            c = mix(c, INK, fillA(sdBox(pq, vec2(1.0, 1.15)) - 0.05));
            c = mix(c, pic * 0.45, fillA(sdBox(pq, vec2(0.9, 1.05))));
            float bench = min(sdBox(pq - vec2(0.0, -1.35), vec2(1.6, 0.09)), max(abs(abs(pq.x) - 1.3) - 0.07, abs(pq.y + 1.75) - 0.4));
            c = mix(c, INK, fillA(bench - 0.04));
            c = mix(c, vec3(0.08, 0.13, 0.12), fillA(bench));
        } else {
            // a ticket machine
            vec2 tq = pq - vec2(0.0, -0.75);
            float td = sdBox(tq, vec2(0.45, 1.3)) - 0.05;
            c = mix(c, INK, fillA(td - 0.05));
            c = mix(c, vec3(0.16, 0.17, 0.19), fillA(td));
            c = mix(c, vec3(0.3, 0.75, 0.9), fillA(sdBox(tq - vec2(0.0, 0.6), vec2(0.28, 0.22))));
        }
        float lx = mod(w.x, 8.0) - 4.0;
        float tube = fillA(sdBox(vec2(lx, w.y - (ce - 0.45)), vec2(2.2, 0.11)));
        c *= 0.55 + 0.6 * exp(-sq(lx / 3.5)) * smoothstep(PLAT_Y, ce, w.y);
        c = mix(c, vec3(0.02, 0.025, 0.025), smoothstep(ce - 0.8, ce - 0.75, w.y));
        c = mix(c, INK, fillA(max(abs(abs(lx) - 1.8) - 0.03, abs(w.y - (ce - 0.25)) - 0.2)));     // its rods
        c = mix(c, lc * 1.8, tube);
        c += lc * exp(-sq(sdBox(vec2(lx, w.y - (ce - 0.45)), vec2(2.2, 0.0)) / 0.5)) * 0.12;
        c = mix(c, vec3(0.015, 0.016, 0.018), smoothstep(0.9, 0.7, yy));
        // commuters waiting on the platform: silhouettes, swaying a little (none where the cub boards)
        float cid = floor(w.x / 7.0);
        if (hash1(cid + 61.0) < 0.5 && abs((cid + 0.5) * 7.0 - 128.0) > 8.0) {
            float hc = hash1(cid + 62.0);
            vec2 mq = vec2(w.x - (cid + 0.5) * 7.0 - (hc - 0.5) * 3.0, yy);
            mq.x += 0.05 * sin(uTime * 0.7 + hc * 9.0) * mq.y;
            float sz = 0.9 + 0.2 * hash1(cid + 63.0);
            mq /= sz;
            float body = sdBox(mq - vec2(0.0, 1.95), vec2(0.36 - (mq.y - 1.95) * 0.05, 0.75)) - 0.12;
            body = min(body, length(mq - vec2(0.0, 3.15)) - 0.32);
            body = min(body, min(sdSeg(mq, vec2(-0.14, 1.2), vec2(-0.16, 0.02)), sdSeg(mq, vec2(0.14, 1.2), vec2(0.17, 0.02))) - 0.12);
            if (hc > 0.6) body = min(body, sdBox(mq - vec2(0.45, 1.5), vec2(0.18, 0.25)));       // a bag
            c = mix(c, vec3(0.025, 0.022, 0.03), fillA(body * sz));
        }
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
        float pf = max(abs(w.x - 1191.6) - 0.6, w.y - GY0);
        vec3 pcc = concrete(w) * (1.2 + 0.6 * uDawn);
        float arch = abs(length(vec2(w.x - 1191.0, (w.y - fl - 3.2) * 0.6)) - 3.3) - 0.12;
        pcc = mix(pcc, pcc * 0.6, fillA(arch));
        c = mix(c, pcc, fillA(pf));
        return catenary(w, c);
    }
    // tunnels: dark concrete in rings (a joint every 3), a lamp every 9 and its pool of light on the wall, two cables
    // on brackets, now and then a refuge niche or a signal: the rhythm that streams past the train as it runs. At
)" +
R"(    // speed the camera's motion blurs it (uBlur: the way the camera covers in a shutter's time): the joints, the
    // brackets and the niches' edges smear (their contrast spread over the blur, never strobing), the lamps and the
    // signals draw streaks behind them
    float yy = w.y - fl;
    float bl = uBlur;
    vec3 c = mix(vec3(0.05, 0.048, 0.046), vec3(0.022, 0.021, 0.022), sat(yy / 7.5));
    c *= 0.9 + 0.2 * hash1(floor(w.x / 3.0)) * 1.2 / (1.2 + bl);
    float jx = (fract(w.x / 3.0 + 0.5) - 0.5) * 3.0;
    c *= 1.0 - 0.45 * (1.0 - smoothstep(0.03, 0.03 + gPx * 1.5 + bl * 0.5, abs(jx))) * lod(3.0) * 0.1 / (0.1 + bl);
    // the vault: ribs every 6 (outlined), a pillar every 18, a ventilation grille now and then, the contact wire under
    // the crown on its hangers (each smeared by the blur in proportion to its width)
    float es = gPx + bl * 0.5;
    float rib = abs(mod(w.x, 6.0) - 3.0) - 0.28;
    c = mix(c, c * 1.6 + 0.006, fillS(rib, es) * 0.56 / (0.56 + bl));
    c = mix(c, INK, fillS(abs(rib) - 0.025, es) * 0.6 * 0.05 / (0.05 + bl));
    float pcx = mod(w.x - 4.0, 18.0) - 9.0;
    float pil = sdBox(vec2(pcx, yy - 3.9), vec2(0.5, 3.9));
    vec3 pilC = vec3(0.075, 0.072, 0.068) * (0.65 + 0.5 * smoothstep(-0.5, 0.4, pcx));
    c = mix(c, INK, fillS(pil - 0.05, es) * 0.8 * 1.1 / (1.1 + bl));
    c = mix(c, pilC, fillS(pil, es) * 1.0 / (1.0 + bl));
    float gx = mod(w.x - 13.0, 45.0) - 22.5;
    vec2 gq = vec2(gx, yy - 5.6);
    float gr = sdBox(gq, vec2(0.75, 0.42));
    c = mix(c, INK, fillS(gr - 0.05, es) * 1.5 / (1.5 + bl));
    c = mix(c, vec3(0.03) * (0.6 + 0.6 * step(0.5, fract(gq.y / 0.14))), fillS(gr, es) * 1.5 / (1.5 + bl));
    float cwire = abs(yy - 7.05) - 0.03;
    float hang = max(abs(mod(w.x, 4.0) - 2.0) - 0.025, abs(yy - 7.3) - 0.25);
    c = mix(c, vec3(0.06, 0.05, 0.04), fillA(cwire));
    c = mix(c, INK, fillS(hang, es) * 0.05 / (0.05 + bl));
    float nx = mod(w.x - 22.0, 36.0) - 18.0;
    float niche = sdBox(vec2(nx, yy - 2.1), vec2(0.75, 2.1));
    c = mix(c, vec3(0.006, 0.006, 0.007), fillS(niche, gPx + bl * 0.5) * (0.4 + 0.6 * 1.5 / (1.5 + bl)));
    c = mix(c, vec3(0.1, 0.1, 0.095), fillS(abs(niche) - 0.07, gPx + bl * 0.5) * step(0.0, yy) * 0.14 / (0.14 + bl));
    float sx = mod(w.x - 40.0, 54.0) - 27.0;
    vec2 sq0 = vec2(sx, yy - 3.0);
    c = mix(c, vec3(0.012), fillA(sdBox(sq0, vec2(0.2, 0.45)) - 0.04));
    vec2 sl = sq0 - vec2(0.0, 0.18);
    float ss = sdSeg(sl, vec2(0.0), vec2(bl, 0.0));
    c += vec3(0.25, 1.0, 0.55) * (fillS(ss - 0.1, gPx) * 1.2 * 0.2 / (0.2 + bl) + exp(-ss * ss / 0.12) * 0.25);
    float lx = mod(w.x, 9.0) - 4.5;
    vec2 lq = vec2(lx, yy - 5.4);
    c += vec3(1.0, 0.75, 0.45) * (exp(-sq(lx / 2.0)) * exp(-sq((yy - 4.6) / 2.6)) * 0.035);
    c = mix(c, vec3(0.05), fillA(sdBox(lq - vec2(bl * 0.5, 0.12), vec2(0.32 + bl * 0.5, 0.07))) * 0.64 / (0.64 + bl));
    float ls = sdSeg(lq, vec2(0.0), vec2(bl, 0.0));      // the lamp's streak: where it shone over the shutter
    c += vec3(1.0, 0.75, 0.45) * (exp(-ls * ls / 0.08) * 1.4 * (0.35 + 0.65 * 0.5 / (0.5 + bl)) + exp(-length(lq) / 1.5) * 0.06);
    float br = max(abs(mod(w.x, 1.5) - 0.75) - 0.035, abs(yy - 3.8) - 0.32);
    c = mix(c, vec3(0.055), fillS(br, gPx + bl * 0.5) * 0.6 * 0.07 / (0.07 + bl));
    c = mix(c, vec3(0.06), fillA(abs(yy - 4.0 - 0.06 * sin(w.x * 0.5)) - 0.03) * 0.5);
    c = mix(c, vec3(0.05), fillA(abs(yy - 3.6 - 0.06 * sin(w.x * 0.45 + 1.0)) - 0.025) * 0.5);
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
    float refl = lampReflectAt(lxw + 0.04 * sin(w.y * 9.0 + uTime * 2.3), my) * (0.35 + 0.65 * pud) * uLamp * step(x, 1196.0);
    pv += vec3(1.0, 0.72, 0.4) * refl;
    pv += mix(uSkyHor, uSkyTop, 0.3) * pud * 0.4;
    pv += vec3(0.6, 0.7, 0.8) * puddleRings(w) * smoothstep(0.62, 0.7, fbm(vec2(x * 0.25, my * 1.4))) * 0.12 * uRain;
    pv = mix(pv, vec3(0.09, 0.09, 0.11) * (1.0 + uDawn), curb);
    vec3 c = pv;
    if (!COUNTRY) return c;
    // the terminus's platform: concrete slabs, a yellow line along its edge, the edge inked
    float term = smoothstep(1288.0, 1292.0, x) * (1.0 - smoothstep(1342.0, 1346.0, x));
    vec3 pl = mix(vec3(0.22, 0.21, 0.21), vec3(0.11, 0.105, 0.11), sat(my / 1.6)) * (0.5 + 0.6 * uDawn);
    pl = mix(pl, pl * 0.7, fillA(abs(fract(x / 2.4) - 0.5) * 2.4 - 1.17) * step(0.4, my));
    pl = mix(pl, vec3(0.62, 0.47, 0.08) * (0.5 + 0.5 * uDawn), fillA(abs(my - 0.2) - 0.1));
    pl = mix(pl, INK, fillA(abs(my - 1.5) - 0.025));
    c = mix(c, pl, term);
    // the towpath: cobbles (each inked, lit on top), the canal's coping stones along its front edge
    float tow = smoothstep(1342.0, 1348.0, x) * (1.0 - smoothstep(1370.0, 1376.0, x));
    float crow = floor(my / 0.3);
    vec2 cq = vec2(x / 0.42 + hash1(crow + 5.0), my / 0.3);
    vec2 cf = (fract(cq) - 0.5) * vec2(0.42, 0.3);
    float ch = hash(floor(cq));
    float cob = sdEll(cf + (hash2(floor(cq)) - 0.5) * 0.05, vec2(0.17 + 0.03 * ch, 0.1 + 0.02 * ch));
    vec3 stc = mix(vec3(0.4, 0.35, 0.34), vec3(0.32, 0.29, 0.31), ch);
    vec3 path = mix(stc * 0.7, stc * (0.95 + 0.15 * step(0.02, cf.y)), mix(0.8, fillA(cob), lod(0.3)));
    path *= mix(1.0, 0.65, sat(my / 1.8));
    float cope = step(1.25, my) * step(1346.0, x) * step(x, 1372.0);
    vec3 cpc = vec3(0.52, 0.48, 0.44) * (1.0 - 0.3 * step(1.45, my));
    cpc = mix(cpc, inkOf(cpc), fillA(min(0.65 - abs(fract(x / 1.3) - 0.5) * 1.3, abs(my - 1.27)) - 0.025));
    path = mix(path, cpc, cope);
    c = mix(c, path * (0.6 + 0.6 * uDawn), tow);
    // the dirt path through the fields (pebbles), moss in the forest, grass in the glade
    vec3 dirt = mix(vec3(0.62, 0.5, 0.33), vec3(0.44, 0.34, 0.22), sat(my / 1.6)) * (0.92 + 0.12 * vnoise(vec2(x * 1.5, my * 4.0)));
    vec3 moss = mix(vec3(0.5, 0.42, 0.24), vec3(0.24, 0.32, 0.1), sat(my / 1.6)) * (0.85 + 0.25 * vnoise(vec2(x * 1.2, my * 3.0)));
    vec3 grass = mix(vec3(0.5, 0.46, 0.18), vec3(0.3, 0.36, 0.12), sat(my / 1.6)) * (0.85 + 0.25 * vnoise(vec2(x * 1.2, my * 3.0)));
    vec3 nat = mix(dirt, moss, smoothstep(1426.0, 1441.0, x));
    nat = mix(nat, grass, smoothstep(1472.0, 1484.0, x));
    // moss and grass creeping in from the edges of the path
    float edge = smoothstep(0.35, 0.0, my + 0.15 * (vnoise(vec2(x * 1.3, 1.0)) - 0.5)) + smoothstep(1.3, 1.75, my + 0.2 * (vnoise(vec2(x * 1.1, 4.0)) - 0.5));
    nat = mix(nat, vec3(0.32, 0.5, 0.16) * (0.8 + 0.3 * vnoise(vec2(x * 3.0, my * 6.0))), sat(edge) * 0.85);
    vec2 pq = vec2(x / 0.5, my / 0.3);
    vec2 pi = floor(pq), pf = fract(pq) - 0.5;
    float peb = sdEll(pf - (hash2(pi) - 0.5) * 0.4, vec2(0.16, 0.13)) * 0.3;
    float pon = step(0.86, hash(pi + 7.0)) * lod(0.3);
    nat = mix(nat, inkOf(dirt), fillA(peb - 0.012) * pon);
    nat = mix(nat, mix(dirt, vec3(0.85, 0.8, 0.7), 0.4), fillA(peb) * pon);
    vec2 lq = vec2(x / 0.45, my / 0.25);
    vec2 li = floor(lq), lf = fract(lq) - 0.5;
    float leaf = fillS(sdEll(rot(hash(li) * 6.0) * lf, vec2(0.38, 0.12)), 0.08) * step(0.8, hash(li + 1.0)) * lod(0.25) * smoothstep(1428.0, 1441.0, x);
    nat = mix(nat, mix(vec3(0.75, 0.6, 0.25), vec3(0.55, 0.45, 0.2), hash(li + 2.0)), leaf * 0.6);
    c = mix(c, dayLit(nat), smoothstep(1372.0, 1380.0, x));
    // the stream crossing the way toward us under the little bridge, mossy rocks at its sides; the bridge's deck of
    // planks (inked, two tones), its front beam
    if (x > 1476.0 && x < 1489.0) {
        float sx = abs(x - 1482.5) - 4.0 - 0.3 * sin(my * 3.0);
        vec3 wc = mix(mix(uSkyHor, vec3(0.5, 0.82, 0.85), 0.6), vec3(0.3, 0.6, 0.66), sat(my / 1.8));
        float st = step(0.62, vnoise(vec2(x * 1.2, my * 4.0 - uTime * 2.2)));
        wc = mix(wc, vec3(0.92, 0.97, 1.0), st * 0.45);
        c = mix(c, dayLit(wc), fillA(sx));
        c = mix(c, dayLit(vec3(0.16, 0.26, 0.1)), fillA(abs(sx) - 0.04));
        float rk = sdEll(vec2(abs(x - 1482.5) - 4.1, my - 1.3), vec2(0.6, 0.35));
        c = drawn(c, rk, dayLit(mix(vec3(0.5, 0.5, 0.46), vec3(0.34, 0.5, 0.16), step(my, 1.2))), dayLit(inkOf(vec3(0.5, 0.5, 0.46))), 0.022, 1.0);
        float deck = max(abs(x - 1482.5) - 5.0, max(-my - 0.05, my - 0.85));
        float pk = abs(fract((x - 1477.5) / 0.45) - 0.5) * 0.45 - 0.2;
        vec3 wd = vec3(0.6, 0.43, 0.27) * (0.85 + 0.25 * hash1(floor((x - 1477.5) / 0.45))) * (1.0 - 0.25 * smoothstep(0.0, 0.85, my));
        wd = mix(wd, inkOf(wd), fillA(-pk - 0.0) * step(my, 0.85));
        wd = mix(wd, wd * 0.6, step(0.62, my));                                                   // the front beam
        c = drawn(c, deck, dayLit(wd), dayLit(inkOf(vec3(0.6, 0.43, 0.27))), 0.025, 1.0);
    }
    return c;
}

void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    gPx = uPxP;
    vec2 w = p + vec2(uD, uCamY);
#ifdef NEAR_PASS
    // the near bamboo alone, over the pandas (premultiplied)
    float na = 0.0;
    vec3 nc = nearBamboo(p, vec3(0.0), na);
    if (na < 0.002) discard;
    nc /= na;
    nc = mix(nc, nc * vec3(0.3, 0.32, 0.4), uOver * 0.75);
    nc = mix(uBridgeCol, nc, uWorldA);
    frag = vec4(max(nc, 0.0) * na, na);
    return;
#endif
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
)" +
R"(        bandC = surfaceBand(w, p);
        bandA = 1.0;
        c = bandC;
        air = true; // (rain falls on it)
    } else {
        c = earth(w);
        // the canal under the towpath: its stone wall, the water (the sky's colors, ripples drifting, the sodium
        // lamps' reflections wavering); the stream falling from under the little bridge over mossy rocks
        if (COUNTRY && w.x > 1345.0 && w.x < 1373.0) {
            float wy = GY0 - 1.8 - w.y;
            vec3 wall = vec3(0.3, 0.27, 0.27) * (0.9 + 0.15 * hash(floor(vec2(w.x / 0.9 + 0.5 * step(0.5, fract(wy / 0.9)), wy / 0.45))));
            vec2 bq = vec2(fract(w.x / 0.9 + 0.5 * step(0.5, fract(wy / 0.9))) - 0.5, fract(wy / 0.45) - 0.5) * vec2(0.9, 0.45);
            wall = mix(wall, INK, fillA(min(0.45 - abs(bq.x), 0.225 - abs(bq.y)) - 0.02) * lod(0.45) * 0.8);
            float lvl = 0.55 + 0.04 * sin(uTime * 1.3 + w.x * 0.7);
            vec3 water = mix(mix(uSkyHor, uSkyTop, 0.35) * 0.8, vec3(0.04, 0.06, 0.08), sat((wy - lvl) / 2.2) * 0.6);
            float rip = step(0.6, vnoise(vec2(w.x * 0.9 + uTime * 0.25, wy * 7.0)));
            water = mix(water, mix(uSkyHor, vec3(1.0), 0.3), rip * 0.35 * (1.0 - sat((wy - lvl) / 1.5)));
            float lx = w.x - (floor((w.x - 2.0) / 13.0 + 0.5) * 13.0 + 2.0) - 0.7 + 0.12 * sin(wy * 9.0 + uTime * 2.0);
            water += vec3(1.0, 0.6, 0.25) * exp(-lx * lx / 0.06) * exp(-(wy - lvl) * 0.7) * 0.7 * uLamp * step(0.5, fract(wy * 5.0 + 0.3 * sin(w.x * 3.0)));
            vec3 cc = mix(wall, water, step(lvl, wy));
            cc = mix(cc, mix(uSkyHor, vec3(1.0), 0.5), fillA(abs(wy - lvl) - 0.025));
            float side = min(w.x - 1346.0, 1372.0 - w.x);
            cc = mix(cc, wall, step(side, 0.0));
            cc = mix(cc, INK, fillA(abs(side) - 0.03));
            c = mix(c, cc * (0.6 + 0.5 * uDawn), smoothstep(-0.6, 0.0, side + 0.6));
        }
        if (COUNTRY && w.x > 1476.0 && w.x < 1489.0) {
            float wy = GY0 - 1.8 - w.y;
            float sx = w.x - 1482.5;
            float fall = abs(sx + 0.15 * sin(wy * 2.0)) - (3.6 - 0.2 * wy);
            vec3 wc = mix(vec3(0.3, 0.6, 0.66), vec3(0.12, 0.3, 0.36), sat(wy / 2.5));
            float streak = step(0.62, vnoise(vec2(sx * 5.0, wy * 0.5 - uTime * 2.5))) * step(0.3, vnoise(vec2(sx * 2.0, 3.0)));
            wc = mix(wc, vec3(0.9, 0.97, 1.0), streak * 0.7);
            wc = mix(wc, vec3(0.97), fillA(abs(wy - 2.1 - 0.12 * sin(sx * 4.0 + uTime * 3.0)) - 0.18));      // the foam below
            c = mix(c, dayLit(wc), fillA(fall));
            float rk = sdEll(vec2(abs(sx) - 3.9, wy - 1.0), vec2(0.9, 0.6));
            rk = min(rk, sdEll(vec2(abs(sx) - 4.2, wy - 2.4), vec2(1.1, 0.7)));
            vec3 rc = mix(vec3(0.46, 0.46, 0.42) * (0.85 + 0.2 * step(0.0, sx)), vec3(0.34, 0.5, 0.16), step(wy + 0.12 * sin(sx * 5.0), 0.75) + step(wy + 0.12 * sin(sx * 4.0), 2.0) * step(1.6, wy));
            c = drawn(c, rk, dayLit(rc), dayLit(inkOf(vec3(0.42, 0.42, 0.38))), 0.025, 1.0);
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
    if (air) c = cityFront(p, w, c, 1.35);
    if (COUNTRY && air) c = grassFront(p, w, c, 1.35);
    if (air) c = rain(p, c, uRain);
    if (CITY && air && w.x < 1251.0) c = bokeh(p, c, uRain * 0.9 + uDrop * 0.3, vec3(1.0, 0.7, 0.4), vec3(0.5, 0.7, 1.0));
    c += uSunCol * uDrop * 0.05 * smoothstep(4.0, -10.0, p.y);
    c = mix(c, c * vec3(0.3, 0.32, 0.4), uOver * 0.75);
    c = mix(uBridgeCol, c, uWorldA);
    frag = vec4(max(c, 0.0), 1.0);
}
)";
inline const char* WORLD_FS = WORLD_FS_SRC.c_str();

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
// A kawaii chibi panda after the owner's reference image, measured against it (its silhouette, its black parts): a
// sticker's flat fills (no shading) in a uniform thick dark outline; cream fur and pure black parts. A round head a
// little wider than tall (round above, fuller below), round ears high on it and apart from it, big round black patches
// each holding a big eye (a white crescent around its outer side, a big dot of light up toward the nose, a small one
// down on the other side; ^ ^ in joy), a small rounded black nose, flat pink oval blush, an open D of a mouth with a w on
// top and a pink tongue when happy (a w content, a frown sad, a little o surprised); a slim body narrower than the head,
// a black band over the shoulders like a shawl, a white belly, arms hanging close to it, short black legs apart, a small
// round black tail; little pink hearts by the head when it waves for joy, yellow sparkles when surprised. It turns from
// facing us (the patches side by side, the arms at its sides) to a profile (one ear at the back of the crown over the
// head, the other peeking over the top, one patch near the front of the face, the snout's little bump with the nose on
// its tip, the mouth open at the front edge, the near arm hanging from the band, the legs one before the other, the
// tail): uTurn sets how far, its sign to which side. Local units: its height is 1 (ears to feet), feet at the origin,
// facing +x. One quad per panda; the reflection in a wet ground is the same panda mirrored under its feet.
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
uniform float uSurprise;                  // surprised: eyes wide, a little o of a mouth, sparkles

float gAA, gLW, gS;
vec3 gCol;
float gA;
const vec3 BLACK = vec3(0.03, 0.022, 0.02);    // the black parts and the outline, one soft black
const vec3 FUR = vec3(0.9, 0.87, 0.775);       // cream
const vec3 PINK = vec3(0.89, 0.38, 0.4);       // the blush
const vec3 TONGUE = vec3(0.72, 0.19, 0.22);

float fillP(float d) { return 1.0 - smoothstep(-gAA, gAA, d); }
vec3 lit(vec3 c) { return c * (uAmb + uKey * 0.85); }
// a part: its outline, then its flat fill
void paint(float d, vec3 c) {
    float o = fillP(d - gLW);
    gCol = mix(gCol, lit(BLACK), o);
    gA = max(gA, o);
    gCol = mix(gCol, lit(c), fillP(d));
}
// an ellipse's distance, true near its edge (a uniform outline around a long one)
float sdE(vec2 p, vec2 r) { float k = length(p / r); return (k - 1.0) * k / max(length(p / (r * r)), 1e-5); }
// a superellipse (n 2 an ellipse, higher boxier), its distance near the edge
float sdSE(vec2 p, vec2 r, float n) {
    vec2 u = max(abs(p) / r, 1e-4);
    float f = pow(pow(u.x, n) + pow(u.y, n), 1.0 / n);
    vec2 g = pow(u / f, vec2(n - 1.0)) / r;
    return (f - 1.0) / max(length(g), 1e-4);
}
float capsule(vec2 q, vec2 a, vec2 b, float r) { return sdSeg(q, a, b) - r; }
float smin(float a, float b, float k) { float h = sat(0.5 + 0.5 * (b - a) / k); return mix(b, a, h) - k * h * (1.0 - h); }
// a heart, its point at the origin, its lobes up (some 1.1 tall)
float sdHeart(vec2 p) {
    p.x = abs(p.x);
    if (p.y + p.x > 1.0) return length(p - vec2(0.25, 0.75)) - 0.35355;
    return sqrt(min(dot(p - vec2(0.0, 1.0), p - vec2(0.0, 1.0)), dot(p - 0.5 * max(p.x + p.y, 0.0), p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
}
// a limb, thicker at its end b (a paw)
float limb(vec2 q, vec2 a, vec2 b, float ra, float rb) {
    vec2 pa = q - a, ba = b - a;
    float h = sat(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h) - mix(ra, rb, h);
}

vec4 panda(vec2 q) {
    gCol = vec3(0.0); gA = 0.0;
    float A = uAdult, walk = uWalk, ph = uPhase, sit = uSit, mood = uMood;
    float sad = (1.0 - mood) * (1.0 - 0.6 * uStrength);   // fed on the way, it stands straighter
    // the turn: t 0 facing us .. 1 in profile, toward gS (mirrored: drawn turning to +x)
    float tw = clamp(uTurn * 1.45, -1.0, 1.0);
    gS = tw < 0.0 ? -1.0 : 1.0;
    float t = abs(tw), ts = t * t * (3.0 - 2.0 * t);
    q.x *= gS;
    q.y -= uHop;
    // the walk: facing us a waddle (rocking from foot to foot), in profile a bob; a little bounce
    float rock = sin(ph) * 0.08 * walk * (1.0 - 0.75 * ts) - uLean * 0.06 + uRoll * gS;   // (leaning: forward)
    vec2 qr = rot(-rock) * q;
    qr.y -= walk * 0.03 * (1.0 + 0.8 * uStrength) * abs(cos(ph)) + 0.005 * sin(uBreath);
    // Proportions measured on the owner's reference (of the height, ears to feet, facing us -> in profile): the head
    // 0.63 wide, 0.55 tall (0.55 x 0.49 in profile), round above and fuller below; the body under it slim, 0.4 wide
    // (0.37), boxy facing us and an egg in profile; the legs 0.1 tall, apart; the ears 0.24 across.
    float sitD = 0.07 * sit;
    vec2 Tc = vec2(-0.012 * ts, 0.27 - sitD);
    float Ta = mix(0.198, 0.185, ts) * (1.0 + 0.04 * uStrength) * (1.0 + 0.06 * A) * mix(1.0, 1.06, sit);
    float Tt = mix(0.27, 0.26, ts) * (1.0 + 0.012 * sin(uBreath)), Tb = mix(0.17, 0.146, ts) * mix(1.0, 0.85, sit);
    float k = 1.0 - 0.08 * A;   // the head's scale (a grown panda's a little smaller for its body)
    vec2 Hc = vec2(-0.0125 * ts, Tc.y + mix(0.385, 0.41, ts) + 0.02 * A - sad * 0.02);
    vec3 Hr = k * vec3(mix(0.313, 0.27, ts), mix(0.312, 0.256, ts), mix(0.237, 0.23, ts));   // half width, above, below
    Hc.y += Hr.z - mix(0.237, 0.23, ts);   // (a smaller head on the same neck)
    float tilt = uTilt - sad * 0.04;
    mat2 toHead = rot(-tilt), fromHead = rot(tilt);
    vec2 qh = toHead * (qr - Hc);

    // Near and far: facing us, its left side (-x) is the one that stays in view as it turns to +x (patch, ear, cheek,
    // arm, leg); its right side turns away, behind the snout and the body.
    // the face's places (head space), facing us -> in profile: the near patch comes to the front of the head, the far
    // one slides on toward the snout and thins away, the nose comes to the snout's tip
    vec2 pNc = k * mix(vec2(-0.163, -0.001), vec2(0.147, -0.004), ts);
    vec2 pFc = k * vec2(mix(0.163, 0.34, ts), -0.001);
    float wF = sat(1.0 - ts * 1.3);
    vec2 eNc = k * mix(vec2(-0.153, 0.005), vec2(0.1585, -0.006), ts), eFc = k * vec2(mix(0.153, 0.36, ts), 0.005);
    vec2 nC = k * mix(vec2(0.0, -0.033), vec2(0.285, -0.039), ts);
    vec2 mC = k * mix(vec2(0.0, -0.083), vec2(0.26, -0.088), ts);
    vec2 mouthP = Hc + fromHead * mC;

    // legs: facing us apart, lifted in turn; in profile one in front of the other, striding
    float legR = mix(0.052, 0.052, ts) * (1.0 + 0.06 * A), hipR = mix(0.05, 0.048, ts) * (1.0 + 0.06 * A);
    float stride = sin(ph) * walk;
    vec2 fN = vec2(mix(-0.089, 0.026 + 0.065 * stride, ts), legR + 0.016 + 0.02 * ts * (1.0 - walk) + max(0.0, mix(-sin(ph), cos(ph), ts)) * 0.045 * walk);
    vec2 fF = vec2(mix(0.089, -0.058 - 0.065 * stride, ts), legR + 0.016 + max(0.0, mix(sin(ph), -cos(ph), ts)) * 0.045 * walk);
    fN = mix(fN, vec2(mix(-0.15, 0.17, ts), legR + 0.016), sit);           // sitting: the legs out in front
    fF = mix(fF, vec2(mix(0.15, 0.07, ts), legR + 0.026), sit);
    vec2 hipN = vec2(mix(-0.11, 0.055, ts), mix(0.13, 0.15, ts) - sitD), hipF = vec2(mix(0.11, -0.085, ts), mix(0.13, 0.15, ts) - sitD);
    // arms: facing us they hang at its sides, in profile the near one from the band, swinging as it walks
    float armR = mix(0.034, 0.05, ts) * (1.0 + 0.08 * A), pawR = armR * mix(1.06, 1.0, ts);
    vec2 shN = vec2(mix(-0.172, -0.062, ts), mix(0.39, 0.4, ts) - sitD), shF = vec2(mix(0.172, 0.04, ts), mix(0.39, 0.4, ts) - sitD);
    float L = mix(0.153, 0.125, ts), sw = 0.1 * sin(ph) * walk;
    vec2 hN = shN + rot(mix(-0.446 + sw, 0.41 - 0.55 * stride, ts)) * vec2(0.0, -L);
    vec2 hF = shF + rot(mix(0.446 + sw, 0.15 + 0.55 * stride, ts)) * vec2(0.0, -L);
    vec2 eyeN = Hc + fromHead * eNc;
    hN = mix(hN, eyeN + vec2(mix(-0.035, 0.05, ts) + 0.01 * sin(uBreath * 7.0), -0.06), uWipe);
    // arms up, waving (facing us), its surprised run (in profile)
    float up3 = smoothstep(0.85, 1.0, ts);   // (in three-quarter view still beside the head, the paws out of its face)
    hN = mix(hN, shN + mix(vec2(-0.2, 0.19), vec2(0.25, 0.08), up3), uArms);
    hF = mix(hF, shF + mix(vec2(0.2, 0.19), vec2(-0.12, 0.16), up3), uArms);
    // the hug: reaching out ahead
    hN = mix(hN, shN + mix(vec2(0.12, 0.08), vec2(0.34, 0.03), ts), uHug);
    hF = mix(hF, shF + vec2(0.26, 0.08), uHug * ts);
    // munching: both paws at the mouth, holding the snack (in profile held up slanting in front of the mouth)
    float sLen = 0.075 * (0.4 + 0.6 * uMunch);
    vec2 sDir = rot(1.0 * ts) * vec2(1.0, 0.0), sc = mouthP + mix(vec2(0.0, -0.035), vec2(0.045, -0.04), ts);
    hN = mix(hN, mix(mouthP + vec2(-0.06, -0.035), sc - sDir * sLen * 0.55 + vec2(0.0, -0.01), ts), uMunch);
    hF = mix(hF, mix(mouthP + vec2(0.06, -0.035), sc - sDir * sLen * 0.1 + vec2(0.03, 0.0), ts), uMunch);
    // ears, high on the head, apart from it (dropping when sad): facing us at its top corners; in profile the near
    // one at the back of the crown (over the head), the far one peeking over the top
    float earDrop = sad * 0.05;
    vec2 eN = Hc + fromHead * (k * mix(vec2(-0.24, 0.233), vec2(-0.113, 0.2), ts) + vec2(-earDrop * 0.5 * (1.0 - ts), -earDrop + uEars.x * 0.05));
    vec2 eF = Hc + fromHead * (k * mix(vec2(0.24, 0.233), vec2(0.077, 0.243), ts) + vec2(earDrop * 0.5 * (1.0 - ts), -earDrop + uEars.y * 0.05));
    float erN = k * mix(0.094, 0.08, ts), erF = k * mix(0.094, 0.06, ts);
    // (in profile the near ear's base cut straight where it sits on the head)
    float earN = max(length(qr - eN) - erN, mix(-1.0, dot(toHead * (qr - eN), vec2(0.682, -0.731)) - k * 0.038, ts));
    float earF = length(qr - eF) - erF;
    float earOver = smoothstep(0.2, 0.7, ts);   // (over a long part of the turn: no sudden swap over the head)
    bool armOver = uWipe > 0.3 || uMunch > 0.3 || uArms > 0.4;
    bool farOver = uMunch > 0.3;
    bool farBehind = ts > 0.35;

    // the body: facing us black, a white oval belly; in profile white under a black band, the near arm over it
    vec2 tq = qr - Tc;
    bool up = tq.y > 0.0;
    float tA = Ta * mix(0.86 + 0.14 * sat((tq.y + 0.15) / 0.25), tq.x < 0.0 ? 1.0 - 0.08 * sat((tq.y - 0.12) / 0.06) : 1.0, ts);
    float bodyD = sdSE(tq, vec2(tA, up ? Tt : Tb), mix(4.0, 2.0, ts));
    vec2 bq = qr - vec2(0.0, 0.245 - sitD);
    float bellyF = bq.y > 0.0 ? sdE(bq, vec2(0.176 * Ta / 0.198, 0.12)) : sdSE(bq, vec2(0.176 * Ta / 0.198, 0.138 * mix(1.0, 0.85, sit)), 2.3);
    float bellyP = max(bodyD, qr.y - (Tc.y + 0.1 + 1.3 * tq.x * tq.x));
    float belly = fillP(mix(bellyF, bellyP, ts));

    // back to front: the far arm (behind the body in profile), ears, tail, legs, body, the near arm, head, face
    if (!farOver && farBehind) paint(limb(qr, shF, hF, armR, pawR), BLACK);
    paint(earF, BLACK);
    paint(earN, BLACK);
    float tail = smoothstep(0.25, 0.7, t);
    if (tail > 0.0) {
        vec2 tc = Tc + vec2(-0.209, -0.026);
        paint(length(qr - tc) - 0.044 * tail, BLACK);
    }
    paint(limb(qr, hipF, fF, hipR, legR), BLACK);
    paint(limb(qr, hipN, fN, hipR, legR), BLACK);
    paint(bodyD, BLACK);
    gCol = mix(gCol, lit(FUR), belly);
    if (!farOver && !farBehind) paint(limb(qr, shF, hF, armR, pawR), BLACK);
    if (!armOver) paint(limb(qr, shN, hN, armR, pawR), BLACK);

    // the head: round above, fuller below, a little snout in profile
    float puff = uMunch * (0.6 + 0.4 * sin(uChew));
    float headD = qh.y > 0.0 ? sdE(qh, Hr.xy * vec2(1.0 + 0.035 * puff, 1.0))
                             : sdSE(qh, Hr.xz * vec2(1.0 + 0.035 * puff, 1.0), mix(2.2, qh.x > 0.0 ? 2.6 : 2.2, ts));   // (a jaw in profile)
    if (qh.y > 0.0 && qh.x > 0.0) headD = sdSE(qh, Hr.xy * vec2(1.0 + 0.035 * puff, 1.0), mix(2.0, 2.15, ts));
    headD = smin(headD, length(qh - k * vec2(0.235 * ts, -0.035)) - k * 0.03, 0.02 * k);   // the snout
    headD = smin(headD, length(qh - k * vec2(0.225 * ts, -0.1)) - k * 0.04 * ts, 0.02 * k);   // the chin
    paint(headD, FUR);
    float headIn = fillP(headD + gLW * 0.2);
    if (earOver > 0.0) {   // the near ear over the head in profile: the head's edge slides back behind it as it turns
        float wipe = dot(qr - eN, normalize(Hc - eN)) - mix(-erN, erN, earOver);
        float o = fillP(max(earN - gLW, wipe));
        gCol = mix(gCol, lit(BLACK), o);
    }

    // the patches, black ovals leaning in (drooping outward when sad); in each a big eye: a white crescent around its
    // outer side, a big dot of light up toward the nose, a small one down on the other side; a soft blink to a
    // curved line; ^ ^ in joy
    float lean = mix(0.5, 0.25, mood);
    float open = 1.0 - max(uBlink, 0.0);
    float o = smoothstep(0.2, 0.45, open);
    float esz = k * mix(0.94, 1.0, mood) * (1.0 + 0.04 * uSurprise);
    float joy = smoothstep(0.2, 0.4, uJoyEyes);   // (a little joy is enough for the happy ^ ^)
    vec2 look = vec2(uLook.x * gS * 0.007, uLook.y * 0.006) * k;
)"
R"(    for (int i = 0; i < 2; i++) {
        float vis = i == 0 ? 1.0 : wF;
        if (vis < 0.02) continue;
        float inner = i == 0 ? 1.0 : -1.0;
        vec2 pc = i == 0 ? pNc : pFc;
        float ang = i == 0 ? mix(lean, 0.42, ts) : -lean;
        vec2 pr = k * mix(vec2(0.093, 0.11), vec2(0.069, 0.089), i == 0 ? ts : 0.0) * vec2(vis, 1.0);
        float pd = sdE(rot(ang) * (qh - pc), pr);
        gCol = mix(gCol, lit(BLACK), fillP(pd) * headIn);
        float R = mix(0.068, 0.0485, i == 0 ? ts : 0.0) * esz;
        vec2 eq = (qh - (i == 0 ? eNc : eFc) - look) / R;
        eq.x *= inner / vis;
        eq.y /= max(open, 0.15);
        float kk = (1.0 - joy) * headIn * o;
        float cres = max(length(eq) - 1.0, 0.942 - length(eq - vec2(0.204, 0.051)));
        gCol = mix(gCol, lit(vec3(0.95, 0.94, 0.9)), fillP(cres * R) * kk);
        float ti = i == 0 ? ts : 0.0;
        float big = length(eq - mix(vec2(0.46, 0.35), vec2(0.4, 0.51), ti)) - mix(0.26, 0.36, ti) * mix(1.0, 0.85, sad);
        float small = length(eq - mix(vec2(-0.24, -0.32), vec2(-0.16, -0.38), ti)) - mix(0.134, 0.2, ti);
        gCol = mix(gCol, lit(vec3(1.0)), fillP(min(big, small) * R) * kk * smoothstep(0.45, 0.7, open));
        // closed (blinking): a soft curved line
        vec2 lq = (qh - (i == 0 ? eNc : eFc)) / R;
        float line = abs(lq.y + 0.09 - 0.4 * lq.x * lq.x) - 0.1;
        line = max(line, abs(lq.x) - 0.75);
        gCol = mix(gCol, lit(vec3(0.6, 0.57, 0.56)), fillP(line * R) * (1.0 - joy) * headIn * (1.0 - o));
        // joy: ^ ^
        float arc = abs(length((lq + vec2(0.0, 0.4)) * vec2(1.0, 1.15)) - 0.62) - 0.15;
        arc = max(arc, -lq.y);
        gCol = mix(gCol, lit(FUR), fillP(arc * R) * joy * headIn);
    }
    // a single tear
    if (uTear > 0.01) {
        vec2 dq0 = qh - eNc - k * vec2(mix(-0.04, 0.03, ts), -0.1 - 0.05 * (1.0 - uTear));
        float drop = min(length(dq0) - 0.015 * k, sdEll(dq0 - vec2(0.0, 0.014 * k), k * vec2(0.008, 0.018)));
        gCol = mix(gCol, vec3(0.6, 0.82, 1.0) * (uAmb + uKey), fillP(drop) * smoothstep(0.0, 0.4, uTear) * headIn);
    }
    // blush, flat pink ovals under the patches
    vec2 bN = k * mix(vec2(-0.19, -0.125), vec2(0.1125, -0.116), ts), bF = k * vec2(mix(0.19, 0.36, ts), -0.125);
    vec2 br = k * mix(vec2(0.044, 0.025), vec2(0.041, 0.023), ts);
    float ck = min(sdE(qh - bN, br), sdE(qh - bF, br * vec2(max(wF, 0.05), 1.0)) + (1.0 - wF) * 0.05);
    gCol = mix(gCol, lit(PINK), fillP(ck - 0.01 * puff) * (0.75 + 0.25 * max(mood, puff)) * headIn);
    // nose: a small rounded black triangle
    vec2 nq = qh - nC;
    float nose = sdE(nq, k * vec2(mix(0.027, 0.03, ts) * (1.0 + 9.0 * clamp(nq.y / k, -0.03, 0.03)), mix(0.017, 0.02, ts)));
    gCol = mix(gCol, lit(BLACK), fillP(nose));   // (in profile on the snout's tip, over the outline)
    gA = max(gA, fillP(nose));
    // mouth: open (a little D, a pink tongue) when happy, a w when content, a small frown when sad, a little o when
    // surprised; in profile under the snout, its back half
    float km = k * 1.25;
    vec2 mq = (qh - mC) / km;
    float sw2 = gLW * 0.55 / km;
    float happyM = max(smoothstep(0.35, 0.8, mood), uMunch);
    mq.y += uMunch * 0.004 * sin(uChew);
    float wArc = abs(length(vec2(abs(mq.x) - 0.014, mq.y - 0.022)) - 0.014) - sw2;
    wArc = max(wArc, mq.y - 0.022);
    float arcP = max(abs(length(mq - vec2(-0.012, 0.016)) - 0.016) - sw2, mq.y - 0.016);   // (in profile one curve)
    wArc = mix(wArc, arcP, ts);
    float frown = abs(length(vec2(mq.x, mq.y + 0.0)) - 0.02) - sw2;
    frown = max(frown, -(mq.y + 0.005));
    float mouth = mix(frown, wArc, happyM);
    // (open: in joy, or happy facing us)
    float openM = max(smoothstep(0.85, 1.0, mood) * (1.0 - uMunch), smoothstep(0.85, 1.0, mood) * uJoyEyes);
    // (facing us a D, its top a little w under the nose; in profile round, open toward the front)
    float mTop = 0.018 * (1.0 - ts);
    vec2 mh = mix(vec2(0.0368, 0.0464), vec2(0.03, 0.052), ts);
    float dM = max(sdE(mq - vec2(0.0, mTop * (1.0 - openM)), mh * vec2(1.0, openM) + 0.001), mq.y - mTop);
    dM = max(dM, (0.0208 - min(length(mq - vec2(0.0192, 0.026)), length(mq - vec2(-0.0192, 0.026)))) * (1.0 - ts) - ts);
    mouth = mix(mouth, dM, step(0.02, openM));
    float oM = sdE(mq - vec2(-0.004 * ts, -0.0328), vec2(0.0148));
    mouth = mix(mouth, oM, smoothstep(0.3, 0.6, uSurprise));
    float mIn = fillP(mouth * km) * mix(headIn, step(0.02, openM) * (1.0 - smoothstep(0.3, 0.6, uSurprise)) + headIn, ts);   // (open in profile, at the front edge)
    gCol = mix(gCol, lit(BLACK), sat(mIn));
    gA = max(gA, sat(mIn));
    vec2 tgq = mq - mix(vec2(0.0, -0.022), vec2(-0.004, -0.0264), ts);
    float tongue = max(sdE(tgq, mix(vec2(0.0192, 0.0136), vec2(0.0136, 0.0112), ts) * openM), dM + gLW * 0.6 / km);
    gCol = mix(gCol, lit(TONGUE), fillP(tongue * km) * headIn * step(0.02, openM) * (1.0 - smoothstep(0.3, 0.6, uSurprise)));
    if (farOver) paint(limb(qr, shF, hF, armR, pawR), BLACK);
    if (armOver) paint(limb(qr, shN, hN, armR, pawR), BLACK);
    // the snack in its paws, shrinking as it is eaten
    if (uMunch > 0.05) {
        float st = capsule(qr, sc - sDir * sLen, sc + sDir * sLen, 0.02);
        vec2 sl = vec2(dot(qr - sc, sDir), dot(qr - sc, vec2(-sDir.y, sDir.x)));
        vec3 stc = mix(vec3(0.35, 0.55, 0.16), vec3(0.7, 0.85, 0.36), smoothstep(-0.02, 0.015, sl.y)) * (uAmb + uKey * 0.85);
        stc = mix(stc, vec3(0.22, 0.38, 0.1) * (uAmb + uKey), 1.0 - smoothstep(0.004, 0.008, abs(sl.x - 0.02)));
        gCol = mix(gCol, lit(BLACK), fillP(st - gLW) * smoothstep(0.05, 0.3, uMunch));
        gCol = mix(gCol, stc, fillP(st) * smoothstep(0.05, 0.3, uMunch));
    }
    // around a joyful wave little pink hearts float up by the head; sparkles for a surprise
    float hearts = uArms * smoothstep(0.6, 0.9, mood) * (1.0 - uHug);
    if (hearts > 0.02 || uSurprise > 0.02) {
        for (int i = 0; i < 3; i++) {
            float fi = float(i), u = fract(uTime * 0.25 + fi * 0.37);
            vec2 at = Hc + vec2(vec3(-0.4, 0.38, 0.28)[i] + 0.02 * sin(uTime * 2.0 + fi * 2.0), vec3(0.02, 0.1, 0.24)[i] + 0.1 * u);
            float s = vec3(0.06, 0.068, 0.05)[i];
            if (hearts > 0.02) {
                float hd = sdHeart((qr - at) / s + vec2(0.0, 0.55));
                float a = hearts * smoothstep(0.0, 0.15, u) * (1.0 - smoothstep(0.7, 1.0, u));
                gCol = mix(gCol, lit(BLACK), fillP(hd * s - gLW) * a);
                gA = max(gA, fillP(hd * s - gLW) * a);
                gCol = mix(gCol, lit(vec3(1.0, 0.42, 0.5)), fillP(hd * s) * a);
            }
            if (uSurprise > 0.02 && i < 2) {
                vec2 sq0 = qr - Hc - vec2(0.42 + 0.04 * fi, 0.08 + 0.14 * fi);
                float an = atan(sq0.y, sq0.x) + uTime * 0.6, r = length(sq0);
                float st = r - (0.04 - 0.01 * fi) * (0.55 + 0.45 * cos(an * 5.0));   // a little five-pointed star
                float a = uSurprise * (0.75 + 0.25 * sin(uTime * 6.0 + fi * 2.0));
                gCol = mix(gCol, lit(BLACK), fillP(st - gLW * 0.8) * a);
                gA = max(gA, fillP(st - gLW * 0.8) * a);
                gCol = mix(gCol, lit(vec3(1.0, 0.82, 0.25)) * 1.3, fillP(st) * a);
            }
        }
    }
    return vec4(gCol, gA);
}

void main() {
    gAA = uPx / uH * 0.85;
    gLW = max(uPx / uH * 1.5, 0.016);
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

// ----------------------------------------------------------------------------------------------------------- lights, butterflies
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
    if (mode == 7) {
        // a sparkle: a four-pointed star of light, a bright core, a soft halo of its colour (param: its turn)
        float a = vC.x;
        vec2 q = mat2(cos(a), -sin(a), sin(a), cos(a)) * vLocal;
        float star = exp(-abs(q.x) * 14.0) * max(0.0, 1.0 - abs(q.y)) + exp(-abs(q.y) * 14.0) * max(0.0, 1.0 - abs(q.x));
        float core = exp(-r * r * 40.0), halo = exp(-r * r * 6.0) * 0.45;
        float k = clamp(star * 0.9 + core + halo, 0.0, 1.5);
        vec3 col = mix(c, vec3(1.0), clamp(core + star * 0.4, 0.0, 1.0) * 0.7);
        frag = vec4(col * k * vB.a, clamp(k, 0.0, 1.0) * vB.a);
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
// One cell per block, a segment of a bamboo stalk in its piece's color seen a little from above: a cylinder with its
// open top (a pale rim round a dark hollow), round across (dark at its sides, a sheen left of its middle), a node near
// its base, a faint fibre, a thin darker line of its own color around it. The ghost is the outline alone; a cleared cell shrinks softly away.
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
    // a segment of bamboo stalk seen a little from above: a cylinder narrower than the cell, its open top an ellipse
    // (a pale rim round a dark hollow), its bottom rounded the same way
    const float RX = 0.42, RY = 0.085, TOP = 0.37, BOT = -0.43;
    float xr = clamp(q.x / RX, -1.0, 1.0), curve = RY * sqrt(max(0.0, 1.0 - xr * xr));
    float d = max(abs(q.x) - RX, max(q.y - (TOP + curve), (BOT - curve) - q.y));
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(d + 0.03));
        float a = (ring * 0.75 + inside * 0.06) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // the body: round across, dark at its sides, a sheen left of its middle, a little lighter up top
    float cyl = sat(1.0 - xr * xr);
    vec3 c = t * mix(0.52, 1.08, sqrt(cyl)) * mix(0.9, 1.04, smoothstep(BOT, TOP, q.y));
    c = mix(c, mix(t, uLightC, 0.5) * 1.15, exp(-sq((q.x + 0.18) / 0.05)) * 0.45);
    // a node near its base: a dark line round it (following the front of the round), a pale one just above
    float ny = q.y - (-0.27 - curve);
    c = mix(c, t * 0.55, (1.0 - smoothstep(0.016, 0.016 + aa * 1.5, abs(ny))) * 0.7);
    c = mix(c, mix(t, vec3(1.0), 0.35), (1.0 - smoothstep(0.01, 0.01 + aa * 1.5, abs(ny - 0.035))) * 0.5);
    c *= 1.0 - 0.03 * smoothstep(0.35, 0.5, abs(fract(q.x * 7.0 + 0.5) - 0.5) * 2.0) * lod2(aa);
    // the open top: the ellipse, a pale rim and inside it the dark hollow (lit on its far wall)
    float e = length(vec2(q.x / RX, (q.y - TOP) / RY));
    float top = 1.0 - smoothstep(1.0 - aa / RY, 1.0, e);
    float hollow = 1.0 - smoothstep(0.72 - aa / RY, 0.72, e);
    vec3 rim = mix(t, vec3(1.0), 0.35) * mix(0.9, 1.1, sat(0.5 - q.x));
    vec3 inner = t * mix(0.25, 0.55, smoothstep(TOP - RY, TOP + RY, q.y));
    c = mix(c, rim, top);
    c = mix(c, inner, hollow);
    // its own thin darker edge
    c = mix(c, t * 0.5, (1.0 - smoothstep(0.0, 0.03 + aa, -d)) * (1.0 - top * 0.5));
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
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount): the field as baked then (64 px, 8 px
// of reach); the fill's coverage and the halo's shaped as linear blending shaped them (light over dark, dark over
// light).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = pow(smoothstep(0.47 - w, 0.47 + w, d), 1.0 / 2.2);
    float halo = 1.0 - pow(1.0 - smoothstep(0.15, 0.45, d) * uP0.a, 1.0 / 2.2);
    float a = max(fill, halo);
    return vec4(L.ink.rgb * fill + uP0.rgb * (a - fill), a) * L.ink.a;
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

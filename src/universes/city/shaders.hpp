#pragma once
// CITY's shaders (see city.cpp). A 3D night city seen through a shift lens (the board plane z = 0 keeps the layout's
// pixels), rendered into an HDR target with a depth texture:
//  - sky: a dark gradient, the city's light pollution on the horizon, clouds lit from below, a few stars;
//  - ground: the streets far below (sodium lamps, the traffic), in the haze;
//  - boxes: the buildings (instanced boxes), their faces lit by the night (a dim moon, the sky, the streets' glow
//    from below), glass reflecting the sky, small windows lit and put out slowly (or switched into the drop's
//    patterns), lit crowns, all in the haze;
//  - tower: the facade facing the camera whose 10 x 22 windows are the board (two floors above the well): recessed openings (their reveals seen
//    in perspective), glass reflecting the sky, and behind each one a real room (interior mapping: back wall, side
//    walls, floor and ceiling traced per pixel, furnished per window, lit by the window's light), the frame lit by
//    the rooms next to it;
//  - windows: the previews; dots: beacons, fireworks, rain, snow (3D points, depth tested);
//  - bloom (bright pass, blur), then final: the searchlights' volumetric beams (against the depth), the bloom, a
//    filmic curve, sRGB, before the HUD.

namespace city_shaders {

inline const char* COMMON_GLSL = R"(
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float lumv(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
uniform vec3 uSkyTop, uSkyHor, uGlowCol;
uniform float uGlowK;
// the night sky in a direction (no clouds, no stars: for reflections and the haze)
vec3 skyColor(vec3 d) {
    float h = d.y;
    vec3 c = mix(uSkyHor, uSkyTop, smoothstep(-0.02, 0.5, h));
    c += uGlowCol * uGlowK * exp(-max(h, 0.0) / 0.07);
    if (h < 0.0) c = mix(c, uSkyHor * 0.5 + uGlowCol * uGlowK * 0.6, smoothstep(0.0, -0.1, h));
    return c;
}
)";

// Fullscreen triangle with the world ray.
inline const char* FULL_VS = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 1.0, 1.0);
}
)";

inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform vec3 uEye;
uniform float uStars, uClouds, uTime;
uniform vec3 uCloudCol;
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(17.1, 9.7); a *= 0.5; }
    return s / 0.9375;
}
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 d = normalize(w.xyz / w.w - uEye);
    vec3 c = skyColor(d);
    if (d.y > 0.01) {
        vec2 sp = d.xz / d.y;
        if (uStars > 0.0) {
            vec2 g = sp * 60.0;
            vec2 i = floor(g);
            float r = hash12(i);
            if (r > 0.992) {
                vec2 f = fract(g) - 0.5;
                float s = exp(-dot(f, f) * 60.0) * (0.7 + 0.3 * sin(uTime * (0.5 + r * 2.0) + r * 40.0));
                c += vec3(0.7, 0.75, 0.9) * s * uStars * smoothstep(0.08, 0.3, d.y) * (1.0 - uClouds * 0.8);
            }
        }
        if (uClouds > 0.0) {
            // a low deck of clouds lit from below by the city
            float n = fbm(sp * 1.6 + vec2(uTime * 0.004, 0.0));
            float cov = smoothstep(0.45, 0.85, n) * uClouds;
            vec3 cc = uCloudCol * (0.4 + 0.6 * exp(-d.y / 0.15));
            c = mix(c, cc, cov * smoothstep(0.02, 0.12, d.y));
        }
    }
    frag = vec4(c, 1.0);
}
)";

inline const char* GROUND_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
uniform mat4 uVP;
uniform vec4 uExtent; // x0, z0, x1, z1
uniform float uY;
out vec3 vW;
void main() {
    vec2 t = aCorner * 0.5 + 0.5;
    vW = vec3(mix(uExtent.x, uExtent.z, t.x), uY, mix(uExtent.y, uExtent.w, t.y));
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* GROUND_FS = R"(#version 330 core
in vec3 vW;
out vec4 frag;
uniform vec3 uEye, uFogCol, uStreetCol, uAmb;
uniform float uFogD, uTime, uTraffic, uBlack;
uniform float uWake; // the scene's setup and teardown: the share of the city's lights on (0 .. 1)
void main() {
    vec3 P = vW;
    float dist = length(P - uEye);
    const float PITCH = 22.0, ST = 4.0;
    vec2 g = (P.xz + vec2(11.0, 0.0)) / PITCH;
    vec2 cell = floor(g);
    vec2 f = fract(g) * PITCH;
    vec2 fw = fwidth(P.xz);
    float far = smoothstep(0.6, 2.0, max(fw.x, fw.y));
    float sx = smoothstep(ST + fw.x, ST - fw.x, f.x); // a street along z
    float sz = smoothstep(ST + fw.y, ST - fw.y, f.y); // a street along x
    float street = max(sx, sz);
    vec3 c = uAmb * 0.15;
    // the street lamps' pools along the streets
    float lampZ = 0.5 + 0.5 * cos(P.z * 0.9);
    float lampX = 0.5 + 0.5 * cos(P.x * 0.9);
    vec3 e = uStreetCol * (sx * (0.25 + 0.75 * lampZ * lampZ) + sz * (0.25 + 0.75 * lampX * lampX)) * 0.35;
    // the traffic on the streets along x: head lights one way, tail lights the other
    float lane = f.y < ST * 0.5 ? 1.0 : -1.0;
    float spd = 7.0 + 5.0 * hash12(cell.yy + 3.0);
    float u = P.x / 6.0 - lane * uTime * spd / 6.0 + hash12(cell.yy) * 9.0;
    float car = step(hash12(vec2(floor(u), cell.y * 7.0 + lane)), 0.35 * uTraffic);
    float shape = smoothstep(0.5, 0.2, abs(fract(u) - 0.5)) * smoothstep(0.8, 0.3, abs(f.y - ST * (lane > 0.0 ? 0.25 : 0.75)));
    vec3 carCol = lane > 0.0 ? vec3(1.0, 0.9, 0.75) : vec3(1.0, 0.08, 0.03);
    e += carCol * car * shape * sz * 1.2;
    // the cross streets' traffic, along z
    float lz = f.x < ST * 0.5 ? 1.0 : -1.0;
    float uz = P.z / 6.0 - lz * uTime * 8.0 / 6.0 + hash12(cell.xx) * 9.0;
    float carz = step(hash12(vec2(floor(uz), cell.x * 5.0 + lz)), 0.25 * uTraffic);
    float shz = smoothstep(0.5, 0.2, abs(fract(uz) - 0.5)) * smoothstep(0.8, 0.3, abs(f.x - ST * (lz > 0.0 ? 0.25 : 0.75)));
    e += (lz > 0.0 ? vec3(1.0, 0.9, 0.75) : vec3(1.0, 0.08, 0.03)) * carz * shz * sx;
    // far away: the average glow of the streets
    e = mix(e, uStreetCol * 0.12 * (ST * 2.0 / PITCH), far);
    e *= 1.0 - uBlack * 0.8;
    // waking or going to sleep: block by block (an average far away)
    e *= mix(step(hash12(cell * 1.31 + 7.7), uWake * 1.02 - 0.01), uWake, far);
    float fog = 1.0 - exp(-dist * uFogD);
    c = mix(c, uFogCol, fog) + e * (1.0 - 0.75 * fog);
    frag = vec4(c, 1.0);
}
)";

// Instanced boxes. a0 = center x, center z, half width, half depth; a1 = y0, y1, seed, style (0 glass, 1 stone,
// 2 solid: masts, crane); a2 = lit factor, crown, patterned, quiet; a3 = body albedo, reflectivity
inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNor;
layout(location = 2) in vec4 aA0;
layout(location = 3) in vec4 aA1;
layout(location = 4) in vec4 aA2;
layout(location = 5) in vec4 aA3;
uniform mat4 uVP;
out vec3 vW;
out vec3 vN;
flat out vec4 vA0, vA1, vA2, vA3;
void main() {
    vA0 = aA0; vA1 = aA1; vA2 = aA2; vA3 = aA3;
    vec3 p = vec3(aA0.x + aPos.x * aA0.z, mix(aA1.x, aA1.y, aPos.y * 0.5 + 0.5), aA0.y + aPos.z * aA0.w);
    vW = p;
    vN = aNor;
    gl_Position = uVP * vec4(p, 1.0);
}
)";
inline const char* BOX_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
flat in vec4 vA0, vA1, vA2, vA3;
out vec4 frag;
uniform vec3 uEye, uMoonDir, uMoonCol, uAmb, uUpCol, uFogCol, uCityLight, uDropA, uDropB, uCrownCol, uSnowCol;
uniform float uFogD, uTime, uLitFrac, uDropK, uPatEpoch, uPatPhase, uBlack, uFlash, uSnow, uGroundY;
uniform float uWake; // the scene's setup and teardown: the share of the city's lights on (0 .. 1)
uniform float uEq[8];
uniform vec3 uLamp[5]; // the city's room lights (unit luminance): tungsten, fluorescent, halogen, a TV, sodium
uniform vec4 uQuiet[2]; // the HUD's columns, in pixels (gl_FragCoord): the windows there are dimmer
void main() {
    vec3 n = normalize(vN);
    vec3 V = normalize(vW - uEye);
    float dist = length(vW - uEye);
    float seed = vA1.z;
    int st = int(vA1.w + 0.5);
    vec3 alb = vA3.rgb;
    float H = vA1.y - vA1.x;
    float quiet = 1.0;
    for (int i = 0; i < 2; i++) {
        vec2 q = gl_FragCoord.xy;
        vec4 r = uQuiet[i];
        float k = smoothstep(r.x - 40.0, r.x + 10.0, q.x) * smoothstep(r.z + 40.0, r.z - 10.0, q.x) *
                  smoothstep(r.y - 40.0, r.y + 10.0, q.y) * smoothstep(r.w + 40.0, r.w - 10.0, q.y);
        quiet = min(quiet, 1.0 - 0.6 * k * vA2.w);
    }
    // the night's light on a face: the sky above, a dim moon, the streets' glow from below
    float up = exp(-max(vW.y - uGroundY, 0.0) / 30.0);
    vec3 lightIn = uAmb * (0.6 + 0.4 * n.y) + uMoonCol * max(dot(n, uMoonDir), 0.0) + uUpCol * up * (0.5 - 0.5 * n.y);
    vec3 c;
    vec3 emit = vec3(0.0);
    if (n.y > 0.5) {
        c = alb * 0.6 * lightIn;
        vec2 e = vec2(vA0.z - abs(vW.x - vA0.x), vA0.w - abs(vW.z - vA0.y));
        float edge = smoothstep(0.5, 0.0, min(e.x, e.y));
        emit += uCrownCol * vA2.y * edge * 2.0;
        if (uSnow > 0.0) c = mix(c, uSnowCol * (0.5 + lightIn), uSnow * 0.9);
    } else if (n.y < -0.5 || st == 2) {
        c = alb * lightIn;
    } else {
        float u = abs(n.x) > 0.5 ? vW.z : vW.x;
        float v = vW.y - vA1.x;
        float bay = mix(0.9, 1.5, fract(seed * 7.7));
        vec2 q = vec2(u / bay, v);
        vec2 id = floor(q);
        vec2 f = fract(q);
        vec2 fw = fwidth(q);
        int ws = int(fract(seed * 3.31) * 3.0);
        vec2 lo = ws == 0 ? vec2(0.16, 0.2) : (ws == 1 ? vec2(0.03, 0.24) : vec2(0.3, 0.12));
        if (st == 1) lo = vec2(0.28, 0.26);
        vec2 hi = vec2(1.0) - lo;
        vec2 m = smoothstep(lo - fw, lo + fw, f) * (1.0 - smoothstep(hi - fw, hi + fw, f));
        float win = m.x * m.y;
        float tiny = smoothstep(0.2, 0.55, max(fw.x, fw.y));
        float area = (hi.x - lo.x) * (hi.y - lo.y);
        win = mix(win, area, tiny);
        win *= step(v, H - 0.8);
        vec2 key = id + vec2(seed * 37.0, seed * 11.0) + n.xz * 13.0;
        float h = hash12(key);
        float per = 22.0 + 55.0 * h;
        float ep = floor((uTime + h * per) / per);
        float frac = uLitFrac * vA2.x;
        float lit = mix(step(hash12(key * 1.37 + ep * 3.7), frac), frac, tiny);
        // waking or going to sleep: the windows switched on, or put out, one by one
        lit *= mix(step(hash12(key * 0.71 + 5.3), uWake * 1.02 - 0.01), uWake, tiny);
        // each window a real lamp: mostly tungsten, some fluorescent offices, halogen, the odd TV, a sodium stairwell
        float pk = hash12(key * 2.1 + ep);
        vec3 lampC = pk < 0.52 ? uLamp[0] : (pk < 0.74 ? uLamp[1] : (pk < 0.88 ? uLamp[2] : (pk < 0.95 ? uLamp[3] : uLamp[4])));
        vec3 wc = lampC * lumv(uCityLight) * (0.5 + 0.8 * hash12(key + ep * 3.1));
        if (uDropK > 0.001) {
            float rows = max(1.0, floor(H - 0.8));
            int pat = int(mod(floor(seed * 7.0) + uPatEpoch, 5.0));
            float on;
            if (pat == 0) on = mod(id.y + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
            else if (pat == 1) on = mod(id.x + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
            else if (pat == 2) on = mod(id.x + id.y + uPatPhase, 2.0) < 1.0 ? 1.0 : 0.0;
            else if (pat == 3) on = id.y < uEq[int(mod(floor(seed * 13.0), 8.0))] * rows ? 1.0 : 0.0;
            else on = mod(id.x + id.y - uPatPhase * 2.0, 4.0) < 1.5 ? 1.0 : 0.0;
            on = mix(on, 0.5, tiny);
            vec3 dc = mod(floor(seed * 5.0), 2.0) < 1.0 ? uDropA : uDropB;
            bool patterned = vA2.z > 0.5;
            if (patterned) lit = mix(lit, on, uDropK);
            wc = mix(wc, dc * (patterned ? 0.45 + 0.6 * uFlash : 0.35 + 0.3 * uFlash), uDropK * (patterned ? 1.0 : 0.75));
        }
        // the room behind: brighter under its ceiling
        float room = mix(0.75 + 0.5 * smoothstep(0.3, 0.8, f.y), 1.0, tiny);
        emit += wc * lit * win * room * (1.0 - uBlack) * quiet;
        // the face: glass (dark, reflecting the sky) or stone, the unlit windows darker
        vec3 body = alb * lightIn;
        vec3 R = reflect(V, n);
        float fres = 0.04 + 0.96 * pow(1.0 - max(dot(-V, n), 0.0), 5.0);
        float refl = vA3.a * (st == 0 ? 1.0 : 0.25);
        vec3 glass = alb * 0.25 * lightIn;
        c = mix(body, glass, win) + skyColor(R) * fres * refl * (1.0 - lit * win) * mix(1.0, 1.6, win);
        // a floor's slab line on glass towers
        if (st == 0) c *= 1.0 - 0.3 * smoothstep(0.06, 0.0, abs(f.y - 0.04)) * (1.0 - tiny);
        if (uSnow > 0.0 && st == 1) c = mix(c, uSnowCol * lightIn * 2.0, uSnow * 0.6 * smoothstep(0.05, 0.0, abs(f.y - lo.y + 0.03)) * m.x * (1.0 - tiny));
    }
    float fog = 1.0 - exp(-dist * uFogD);
    c = mix(c, uFogCol, fog) + emit * (1.0 - 0.7 * fog);
    frag = vec4(c, 1.0);
}
)";

// The tower's facade: a quad on the board plane (z = 0). a0 = rectangle (x0, y0, x1, y1).
inline const char* TOWER_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
uniform mat4 uVP;
uniform vec4 uRect;
out vec3 vW;
void main() {
    vec2 t = aCorner * 0.5 + 0.5;
    vW = vec3(mix(uRect.xy, uRect.zw, t), 0.0);
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* TOWER_FS = R"(#version 330 core
in vec3 vW;
out vec4 frag;
uniform sampler2D uCellA; // 10 x 22, the board's rows (row 0 at the top: two floors above the well, where pieces spawn): rgb the window's light (color x level)
uniform sampler2D uCellB; // r ghost, g falling
uniform vec3 uEye, uFrame, uGlassTint, uGhostCol, uPierCol, uCrownCol, uAmb, uUpCol, uMoonCol, uMoonDir, uSnowCol, uReflCity;
uniform vec3 uFlood;
uniform float uFrameLight, uMull, uSpand, uRecess, uReflK, uPier, uCrown, uSnow, uRain, uTime, uRoomAmb, uGroundY;
uniform vec4 uContact[4];  // x0, x1, y, intensity
uniform vec3 uContactCol[4];
uniform float uBridge;
uniform vec3 uBridgeColor, uBridgeInk;

// what the glass reflects: the sky, and the city behind the camera (dark blocks, a few lights) below the horizon
vec3 reflection(vec3 R) {
    vec3 c = skyColor(R);
    // the city behind the camera: towers against the glow, their windows
    float bl = smoothstep(0.1, 0.0, R.y - 0.12 * vnoise(vec2(R.x * 5.0, 1.0)));
    c = mix(c, uAmb * 0.5 + uGlowCol * uGlowK * 0.15, bl * 0.8);
    vec2 g = vec2(R.x / max(0.05, R.z), R.y / max(0.05, R.z)) * vec2(16.0, 26.0);
    vec2 i = floor(g);
    vec2 f = fract(g) - 0.5;
    float r = hash12(i);
    if (r > 0.72) c += uReflCity * bl * exp(-dot(f, f) * 40.0) * (r - 0.72) * 2.5;
    return c;
}

// The furnished room behind a window, traced per pixel (interior mapping). Q: the point on the glass, r: the view
// ray, (lo, hi): the room's box. kind: 0 office, 1 flat, 2 hotel; t: the window's own seed.
vec3 roomAt(vec3 Q, vec3 r, vec3 lo, vec3 hi, vec3 L, int kind, float s, float amb) {
    if (abs(r.x) < 1e-5) r.x = 1e-5;
    if (abs(r.y) < 1e-5) r.y = 1e-5;
    vec3 tt = vec3((r.x > 0.0 ? hi.x - Q.x : lo.x - Q.x) / r.x,
                   (r.y > 0.0 ? hi.y - Q.y : lo.y - Q.y) / r.y,
                   (lo.z - Q.z) / r.z);
    float t = min(tt.x, min(tt.y, tt.z));
    vec3 H = Q + r * t;
    vec3 sz = hi - lo;
    vec3 u = (H - lo) / sz; // 0..1 in the room
    int surf = t == tt.z ? 0 : (t == tt.x ? 1 : (r.y < 0.0 ? 2 : 3)); // back, side, floor, ceiling
    // the lamp: a ceiling light (office, hotel) or a floor lamp in a corner (flat)
    vec3 lamp = kind == 1 ? vec3(mix(lo.x, hi.x, 0.2 + 0.6 * fract(s * 7.31)), lo.y + sz.y * 0.55, mix(lo.z, hi.z, 0.3))
                          : vec3(mix(lo.x, hi.x, 0.5), hi.y - 0.02, mix(lo.z, hi.z, 0.5));
    vec3 dl = lamp - H;
    float d2 = dot(dl, dl);
    float alb = surf == 0 ? 0.75 : (surf == 1 ? 0.6 : (surf == 2 ? 0.3 : 0.85));
    // the surfaces' own colors, gently: off-white walls, the floor carpet or wood
    vec3 tint = surf == 2 ? (kind == 1 ? vec3(1.15, 0.9, 0.72) : (kind == 0 ? vec3(0.95, 0.97, 1.02) : vec3(1.1, 0.88, 0.82)))
                          : vec3(1.04, 1.0, 0.94);
    // a real lamp: a pool of light falling off with distance, the walls far from it in shadow
    float k = 0.05 + 1.9 / (1.0 + d2 * 10.0);
    // the light under a shade or a ceiling light throws a soft cone on the back wall
    if (surf == 0) {
        float dx = (H.x - lamp.x) / sz.x, dy = (H.y - lamp.y) / sz.y;
        k += (kind == 1 ? 0.9 : 0.45) * exp(-dx * dx * 14.0) * smoothstep(-0.05, 0.25, kind == 1 ? dy : -dy) * exp(-abs(dy) * 2.5);
    }
    // near the lamp its light reads paler (a hot filament), its color deepens with the falloff
    vec3 Lk = mix(L * 1.08, vec3(lumv(L)) * 1.15, 0.12 * smoothstep(0.9, 1.9, k));
    float dark = 1.0;
    vec3 add = vec3(0.0);
    if (kind == 0) {
        // an office: strip lights on the ceiling, desks and screens against the back wall
        if (surf == 3) add += mix(L, vec3(lumv(L)), 0.45) * 2.2 * smoothstep(0.08, 0.03, abs(fract(u.z * 2.0) - 0.5)) * step(0.1, u.x) * step(u.x, 0.9);
        if (surf == 0 && u.y < 0.32 && u.y > 0.24) dark = 0.35;
        if (surf == 0 && u.y > 0.32 && u.y < 0.46 && fract(u.x * 2.5 + s) < 0.5)
            add += vec3(lumv(L)) * vec3(0.6, 0.85, 1.3) * 0.9;
        if (surf == 2) dark = 0.6 + 0.4 * step(0.5, fract(u.x * 3.0 + u.z * 0.0));
    } else if (kind == 1) {
        // a flat: a sofa against the back wall, a picture, the lamp's shade glowing
        float sx = 0.5 + (fract(s * 3.7) - 0.5) * 0.3;
        if (surf == 0 && u.y < 0.3 && abs(u.x - sx) < 0.3) dark = 0.3;
        if (surf == 0 && abs(u.y - 0.62) < 0.1 && abs(u.x - (1.0 - sx)) < 0.12) dark = 0.45;
        if (surf == 1) dark = 0.75 + 0.25 * step(0.5, fract(u.y * 4.0 + s)); // shelves
        if (surf == 2) dark = 0.75;
        vec3 sh = H - (lamp + vec3(0.0, 0.12, 0.0));
        add += mix(L, vec3(lumv(L)), 0.3) * 1.6 * exp(-dot(sh, sh) * 90.0);
    } else {
        // a hotel room: a bed's head against the back wall, a warm ceiling light
        if (surf == 0 && u.y < 0.36 && abs(u.x - 0.5) < 0.35) dark = 0.45;
        if (surf == 0 && u.y < 0.5 && u.y > 0.36 && abs(u.x - 0.5) < 0.35) dark = 0.7;
        if (surf == 3) add += L * 1.5 * exp(-dot(u.xz - 0.5, u.xz - 0.5) * 40.0);
    }
    vec3 lit = Lk * tint * (alb * k * dark) + add;
    // a little light from outside, the corner shading
    float ao = 0.7 + 0.3 * smoothstep(0.0, 0.15, min(min(u.x, 1.0 - u.x), min(u.y, 1.0 - u.y)));
    return lit * ao + vec3(amb) * alb * dark;
}

void main() {
    vec3 P = vW;
    vec3 r = normalize(P - uEye);
    vec3 c;
    float px = fwidth(P.x);
    if (abs(P.x) > 5.5) discard;
    float up = exp(-max(P.y - uGroundY, 0.0) / 30.0);
    vec3 lightIn = (uAmb + uMoonCol * max(uMoonDir.z, 0.0) + uUpCol * up * 0.5) * uFrameLight
                 + uFlood * (0.35 + 0.65 * smoothstep(12.0, -12.0, P.y));
    if (P.y > 13.1) {
        bool box = P.x > -3.4 && P.x < 0.4 && P.y < 13.75;
        bool mast = abs(P.x - 2.6) < 0.06 + 0.03 * step(P.y, 13.5) && P.y < 16.5;
        if (!box && !mast) discard;
        c = uFrame * lightIn * (box ? 0.7 : 0.6);
        if (box) c += uCrownCol * uCrown * 0.6 * smoothstep(0.06, 0.0, 13.75 - P.y);
    } else if (P.y > 12.0) {
        if (P.y < 12.2) c = uFrame * lightIn;
        else if (P.y < 12.95) {
            float l = fract(P.y * 12.0);
            c = uFrame * lightIn * mix(0.35, 1.1, smoothstep(0.2, 0.5, l) * (1.0 - smoothstep(0.75, 0.95, l)));
            c += uCrownCol * 0.03 * uCrown;
        } else {
            c = uFrame * lightIn * 1.2;
            c += uCrownCol * uCrown * smoothstep(0.05, 0.0, abs(P.y - 13.02));
        }
        if (uSnow > 0.0) c = mix(c, uSnowCol * lightIn * 3.0, uSnow * smoothstep(0.06, 0.0, 13.1 - P.y));
    } else if (P.y < -10.0) {
        // a plant floor under the board: louvres, then the facade going on down
        float l = fract(P.y * 12.0);
        c = uFrame * lightIn * mix(0.35, 1.0, smoothstep(0.2, 0.5, l) * (1.0 - smoothstep(0.75, 0.95, l)));
        c *= smoothstep(-13.0, -10.2, P.y) * 0.6 + 0.4;
    } else if (abs(P.x) > 5.0) {
        // the piers: stone, a vertical light strip in each
        float ax = abs(P.x);
        c = uFrame * lightIn * 1.25;
        float strip = smoothstep(0.04, 0.012, abs(ax - 5.25));
        c += uPierCol * uPier * strip;
        vec2 tc = vec2(P.x < 0.0 ? 0.5 / 10.0 : 9.5 / 10.0, (12.0 - P.y) / 22.0);
        c += texture(uCellA, tc).rgb * 0.06 * smoothstep(5.5, 5.0, ax);
    } else {
        int col = clamp(int(floor(P.x + 5.0)), 0, 9);
        int row = clamp(int(floor(12.0 - P.y)), 0, 21);
        vec2 ctr = vec2(float(col) - 4.5, 11.5 - float(row));
        vec2 lp = P.xy - ctr;
        vec3 A = texelFetch(uCellA, ivec2(col, row), 0).rgb;
        vec4 B = texelFetch(uCellB, ivec2(col, row), 0);
        // the window: an opening in the frame, the glass set back in it
        float gx = 0.5 - uMull * 0.5;
        float gt = 0.5 - uSpand * 0.3, gb = -(0.5 - uSpand * 0.7);
        vec2 oc = vec2(0.0, (gt + gb) * 0.5), oh = vec2(gx, (gt - gb) * 0.5);
        vec2 d = abs(lp - oc) - oh;
        float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
        float inO = 1.0 - smoothstep(-px * 0.7, px * 0.7, sd);
        // each floor its use (offices, flats, a hotel), each window its own furnishings and blinds
        float fs = hash12(vec2(floor(float(row) / 3.0) * 1.7 + 0.3, 5.1));
        int kind = fs < 0.4 ? 0 : (fs < 0.8 ? 1 : 2);
        float s = hash12(vec2(float(col) * 3.1 + 0.7, float(row) * 1.9 + 0.2));
        // the frame: mullions and spandrels, a sill under each window lit by its room
        vec3 fr = uFrame * lightIn;
        float sill = smoothstep(gb - 0.09, gb - 0.06, lp.y) * smoothstep(gb + 0.005, gb - 0.02, lp.y) * step(abs(lp.x), gx + 0.04);
        fr *= 1.0 + 0.5 * sill;
        vec3 spill = texture(uCellA, vec2((P.x + 5.0) / 10.0, (12.0 - P.y) / 22.0)).rgb;
        fr += spill * 0.04 + A * 0.2 * sill + A * 0.06 * smoothstep(0.08, 0.0, sd);
        fr += reflection(reflect(r, vec3(0.0, 0.0, 1.0))) * 0.06 * uReflK;
        if (uSnow > 0.0) fr = mix(fr, uSnowCol * (lightIn * 3.0 + A * 0.5), uSnow * sill);
        vec3 g = vec3(0.0);
        if (inO > 0.0) {
            // the reveal: the depth of the opening, seen in perspective
            float rd = 0.05 + 0.28 * uRecess;
            vec3 Q = P + r * (rd / -r.z);
            vec2 ql = Q.xy - ctr - oc;
            if (abs(ql.x) > oh.x || abs(ql.y) > oh.y) {
                vec3 rv = uFrame * lightIn * 0.6 + A * 0.22 * (0.4 + 0.6 * smoothstep(-oh.y, oh.y, ql.y));
                if (ql.y > oh.y) rv *= 0.5; // the head of the opening, in its own shadow
                g = rv;
            } else {
                vec2 uv = ql / (2.0 * oh) + 0.5;
                vec3 lo = vec3(ctr.x - 0.49, ctr.y - 0.47, -rd - 1.3);
                vec3 hi = vec3(ctr.x + 0.49, ctr.y + 0.48, -rd);
                bool on = lumv(A) > 0.0005;
                vec3 Lr = (A + uGhostCol * B.r * 0.08) * 0.85;
                vec3 room = roomAt(Q, r, lo, hi, Lr, kind, s, uRoomAmb);
                // what hangs at the glass: blinds, drapes, a roller blind, a sheer, or nothing
                float tsel = kind == 0 ? (s < 0.6 ? 0.0 : 5.0) : floor(s * 5.0);
                float lampU = kind == 1 ? 0.2 + 0.6 * fract(s * 7.31) : 0.5;
                float glow = kind == 1 ? 0.55 + 0.6 * exp(-(uv.x - lampU) * (uv.x - lampU) * 8.0) * (1.0 - 0.4 * uv.y)
                                       : 0.5 + 0.55 * uv.y;
                vec3 diff = (A + uGhostCol * B.r * 0.08) * 0.62 * glow + vec3(uRoomAmb) * 0.6;
                if (tsel == 1.0) { // venetian blinds over the top part
                    float cov = 0.35 + 0.5 * fract(s * 13.0);
                    float sl = smoothstep(0.1, 0.25, fract(uv.y * 9.0)) * (1.0 - smoothstep(0.65, 0.8, fract(uv.y * 9.0)));
                    if (uv.y > 1.0 - cov) room = mix(room * 0.25, diff * 1.1, sl);
                } else if (tsel == 2.0) { // drapes at both sides
                    float w = 0.18 + 0.12 * fract(s * 17.0);
                    float inD = step(uv.x, w) + step(1.0 - w, uv.x);
                    room = mix(room, diff * (0.65 + 0.35 * sin(uv.x * 70.0 + s * 9.0)), inD);
                } else if (tsel == 3.0) { // a roller blind half down
                    float hy = 0.35 + 0.4 * fract(s * 23.0);
                    room = mix(room, diff * 1.05, step(hy, uv.y));
                    room *= 1.0 - 0.6 * smoothstep(0.03, 0.0, abs(uv.y - hy));
                } else if (tsel == 4.0) { // a sheer curtain
                    room = mix(room, diff * 0.9, 0.55);
                } else if (tsel == 5.0) { // office vertical blinds, half turned
                    float sl = smoothstep(0.1, 0.3, fract(uv.x * 7.0)) * (1.0 - smoothstep(0.6, 0.8, fract(uv.x * 7.0)));
                    room = mix(room, mix(room * 0.4, diff, sl), 0.8);
                }
                room *= uGlassTint;
                float cosT = -r.z;
                float F = (0.08 + 0.3 * uReflK) * (0.75 + 0.5 * s) + pow(1.0 - cosT, 5.0);
                vec3 refl = reflection(vec3(r.x, r.y, -r.z));
                if (uRain > 0.0) {
                    float sN = vnoise(vec2(Q.x * 26.0, Q.y * 1.3 + uTime * 1.6));
                    refl += (refl + uReflCity * 0.1) * uRain * smoothstep(0.75, 0.95, sN);
                }
                g = room + refl * F * (on ? 0.3 : 1.0);
            }
        }
        // the ghost: the window's frame lit in the piece's light
        if (B.r > 0.001) {
            float line = 1.0 - smoothstep(0.0, max(px * 1.4, 0.022), abs(sd - 0.03));
            fr = mix(fr, uGhostCol * 1.1, line * B.r);
        }
        c = mix(fr, g, inO);
    }
    // the light under a piece that just landed, along the ledge (a soft glow, no flash)
    for (int i = 0; i < 4; i++) {
        if (uContact[i].w <= 0.001) continue;
        float hx = smoothstep(uContact[i].x - 0.25, uContact[i].x + 0.1, P.x) * (1.0 - smoothstep(uContact[i].y - 0.1, uContact[i].y + 0.25, P.x));
        float dy = (P.y - uContact[i].z) / 0.12;
        c += uContactCol[i] * uContact[i].w * hx * exp(-dy * dy);
    }
    if (uBridge > 0.0) {
        float lit = 0.0;
        if (abs(P.x) < 5.0 && P.y > -10.0 && P.y < 12.0) {
            int col = clamp(int(floor(P.x + 5.0)), 0, 9), row = clamp(int(floor(12.0 - P.y)), 0, 21);
            lit = clamp(lumv(texelFetch(uCellA, ivec2(col, row), 0).rgb) * 4.0, 0.0, 1.0);
        }
        c = mix(c, mix(uBridgeColor, uBridgeInk, lit), uBridge);
    }
    frag = vec4(c, 1.0);
}
)";

// The previews: small lit windows on the board plane. a0 = rectangle; a1 = light (rgb), alpha
inline const char* RECT_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA0;
layout(location = 2) in vec4 aA1;
layout(location = 3) in vec4 aA2;
layout(location = 4) in vec4 aA3;
uniform mat4 uVP;
out vec2 vUv;
flat out vec4 vA0, vA1, vA2;
void main() {
    vA0 = aA0; vA1 = aA1; vA2 = aA2;
    vec2 t = aCorner * 0.5 + 0.5;
    vUv = t;
    gl_Position = uVP * vec4(mix(aA0.xy, aA0.zw, t), 0.0, 1.0);
}
)";
inline const char* WIN_FS = R"(#version 330 core
in vec2 vUv;
flat in vec4 vA0, vA1, vA2;
out vec4 frag;
uniform vec3 uFrame, uBridgeInk;
uniform float uBridge;
void main() {
    vec2 uv = vUv;
    float e = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
    float fw = max(fwidth(uv.x), fwidth(uv.y));
    float inG = smoothstep(0.1 - fw, 0.1 + fw, e);
    vec3 L = vA1.rgb;
    vec3 c = L * (0.55 + 0.45 * smoothstep(0.1, 0.9, uv.y));
    c += L * 0.6 * exp(-dot(uv - vec2(0.5, 0.85), uv - vec2(0.5, 0.85)) * 18.0);
    c = mix(uFrame, c, inG);
    c = mix(c, uBridgeInk, uBridge);
    float a = vA1.a;
    frag = vec4(c * a, a);
}
)";

// Additive lights in 3D (depth tested). a0 = center, radius; a1 = color, alpha; a2 = direction (streak), length;
// a3.x = kind (0 soft dot, 1 a trail fading behind, 2 a crisp square, 3 a core and its glow)
inline const char* DOT_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aA0;
layout(location = 2) in vec4 aA1;
layout(location = 3) in vec4 aA2;
layout(location = 4) in vec4 aA3;
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 vA0, vA1, vA2, vA3;
void main() {
    vA0 = aA0; vA1 = aA1; vA2 = aA2; vA3 = aA3;
    vec2 d = length(aA2.xy) > 1e-4 ? normalize(aA2.xy) : vec2(1.0, 0.0);
    vec2 n = vec2(-d.y, d.x);
    float r = aA0.w * 2.2;
    float hl = aA2.w * 0.5 + r, hw = r;
    vQ = vec2(aCorner.x * hl, aCorner.y * hw);
    vec3 w = aA0.xyz + vec3(d * vQ.x + n * vQ.y, 0.0);
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* DOT_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 vA0, vA1, vA2, vA3;
out vec4 frag;
void main() {
    float r = vA0.w, L = vA2.w * 0.5;
    int kind = int(vA3.x + 0.5);
    float dx = max(abs(vQ.x) - L, 0.0);
    float d = length(vec2(dx, vQ.y)) / r;
    float a = exp(-d * d * 2.0);
    if (kind == 3) a = a * 0.3 + (1.0 - smoothstep(0.35, 0.5, d));
    if (kind == 1 && L > 0.0) a *= mix(0.05, 1.0, clamp((vQ.x + L) / (2.0 * L), 0.0, 1.0));
    a *= vA1.a;
    frag = vec4(vA1.rgb * a, 0.0);
}
)";

// Bloom: a bright pass (soft knee) into a reduced target, then separable blurs.
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uTexel;
void main() {
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec2 o = vec2(i & 1, i >> 1) - 0.5;
        c += texture(uTex, vUv + o * uTexel).rgb;
    }
    c *= 0.25;
    float L = lumv(c);
    float k = smoothstep(0.08, 0.6, L);
    frag = vec4(c * k, 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec2 uDir;
void main() {
    const float W[5] = float[](0.227, 0.1945, 0.1216, 0.054, 0.0162);
    vec3 c = texture(uTex, vUv).rgb * W[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uTex, vUv + uDir * float(i)).rgb * W[i];
        c += texture(uTex, vUv - uDir * float(i)).rgb * W[i];
    }
    frag = vec4(c, 1.0);
}
)";

inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uDepth, uBloom, uBloom2;
uniform mat4 uInvVP;
uniform vec3 uEye;
uniform float uMul, uExposure, uBloomK;
uniform vec4 uWell;           // the well on screen (px: x0, y0, x1, y1)
uniform float uDim, uDimR;    // how much darker the city around it, over what distance (px)
uniform vec4 uBeamO[4];   // origin, intensity
uniform vec4 uBeamD[4];   // direction, width
uniform vec3 uBeamCol[4];
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    float z = texture(uDepth, vUv).r;
    vec4 wp = uInvVP * vec4(vUv * 2.0 - 1.0, min(z, 0.99999) * 2.0 - 1.0, 1.0);
    vec3 P = wp.xyz / wp.w;
    vec3 rd = normalize(P - uEye);
    float tS = z >= 0.99999 ? 1e5 : length(P - uEye);
    // the searchlights: volumetric beams in the haze, hidden behind what stands in front of them
    for (int i = 0; i < 4; i++) {
        if (uBeamO[i].w <= 0.0001) continue;
        vec3 O = uBeamO[i].xyz, D = uBeamD[i].xyz;
        vec3 w = uEye - O;
        float b = dot(rd, D), d = dot(rd, w), e = dot(D, w);
        float den = max(1.0 - b * b, 1e-4);
        float t = (b * e - d) / den;
        float s = (e - b * d) / den;
        if (t <= 0.0 || s <= 0.0) continue;
        float vis = smoothstep(tS + 2.0, tS - 2.0, t);
        float dist = length(uEye + rd * t - (O + D * s));
        float wd = uBeamD[i].w * (1.0 + s * 0.035);
        float k = exp(-dist * dist / (wd * wd)) * exp(-s / 420.0) * smoothstep(0.0, 25.0, s);
        // thicker where the ray runs along the beam
        k *= 1.0 / sqrt(den);
        c += uBeamCol[i] * k * uBeamO[i].w * vis;
    }
    c += (texture(uBloom, vUv).rgb * 0.6 + texture(uBloom2, vUv).rgb * 0.4) * uBloomK;
    // the city a little darker away from the well, so the game stands out
    vec2 out2 = max(max(uWell.xy - gl_FragCoord.xy, gl_FragCoord.xy - uWell.zw), 0.0);
    c *= 1.0 - uDim * smoothstep(0.0, uDimR, length(out2));
    c *= uExposure;
    // a filmic curve: deep blacks, the lit windows rolling off without burning to white
    vec3 x = max(c, 0.0);
    c = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
    c *= uMul;
    c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
    c += (hash12(gl_FragCoord.xy) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

inline const char* PANEL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uRes;
void main() { gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0); }
)";
inline const char* PANEL_FS = R"(#version 330 core
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace city_shaders

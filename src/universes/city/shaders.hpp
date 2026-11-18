#pragma once
// CITY's shaders (see city.cpp). A 3D night city seen through a shift lens (the board plane z = 0 keeps the layout's
// pixels), rendered into an HDR target with a depth texture:
//  - sky: a dark gradient, the city's light pollution on the horizon, clouds lit from below, a few stars;
//  - ground: the streets far below (sodium lamps, the traffic), in the haze;
//  - boxes: the buildings (instanced boxes) of five kinds (glass, brick, art deco, concrete slabs with loggias,
//    precast offices) and what stands on their roofs, their faces lit by the night (a dim moon, the sky, the city's
//    and the streets' glow), their windows by kind (frames, sills, balconies, loggias traced in perspective, curtains
//    and blinds), lit and put out slowly (or switched into the drop's patterns), lit crowns, signs, all in the haze;
//  - tower: the facade facing the camera whose 10 x 22 windows are the board (two floors above the well): recessed
//    openings (their reveals seen in perspective), a casement, glass reflecting the sky, curtains, blinds, roller
//    shutters, window boxes, balconies traced in front of the facade, and behind each window a real room (interior
//    mapping: back wall, side walls, floor and ceiling traced per pixel, furnished per window, lit by the window's
//    light), the frame lit by the rooms next to it;
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

// Instanced boxes. a0 = center x, center z, half width, half depth; a1 = y0, y1, seed, style; a2 = lit factor, crown,
// patterned (a sign: its word), quiet; a3 = the material's albedo, reflectivity. The styles: 0 a glass office tower
// (curtain wall: mullions, a spandrel band per floor), 1 a brick apartment block (punched windows with sills and
// lintels, stacked balconies), 2 solid (masts, the crane, rooftop units), 3 an art-deco stone tower (piers, dark
// spandrel panels, setbacks, its top floodlit), 4 a concrete slab with loggias, 5 a precast office block (a regular
// grid of deep windows), 6 a rooftop plant room (louvres), 7 a water tank (wooden staves, hoops), 8 a rooftop sign.
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
uniform float uWallK; // the night's light on the walls
uniform vec3 uWallGlow; // the city's glow on them (the night's own, not a drop's)
uniform float uEq[8];
uniform vec3 uLamp[5]; // the city's room lights (unit luminance): tungsten, fluorescent, halogen, a TV, sodium
uniform vec4 uQuiet[2]; // the HUD's columns, in pixels (gl_FragCoord): the windows there are dimmer
uniform int uGlyph[64]; // the signs' words: 8 letters each (3 x 5 bits, 0 ends the word)

// a rectangle's mask in a cell, anti-aliased (w: half the blur)
float rectm(vec2 f, vec2 lo, vec2 hi, vec2 w) {
    vec2 m = smoothstep(lo - w, lo + w, f) * (1.0 - smoothstep(hi - w, hi + w, f));
    return m.x * m.y;
}

void main() {
    vec3 n = normalize(vN);
    vec3 V = normalize(vW - uEye);
    float dist = length(vW - uEye);
    float seed = vA1.z;
    int st = int(vA1.w + 0.5);
    vec3 alb = vA3.rgb;
    float H = vA1.y - uGroundY; // the top, above the street
    float quiet = 1.0;
    for (int i = 0; i < 2; i++) {
        vec2 q = gl_FragCoord.xy;
        vec4 r = uQuiet[i];
        float k = smoothstep(r.x - 40.0, r.x + 10.0, q.x) * smoothstep(r.z + 40.0, r.z - 10.0, q.x) *
                  smoothstep(r.y - 40.0, r.y + 10.0, q.y) * smoothstep(r.w + 40.0, r.w - 10.0, q.y);
        quiet = min(quiet, 1.0 - 0.6 * k * vA2.w);
    }
    float awake = (1.0 - uBlack) * quiet;
    // the night's light on a face: the sky above, a dim moon, the streets' glow from below (warm low down, the
    // moon's cold higher up)
    float up = exp(-max(vW.y - uGroundY, 0.0) / 30.0);
    vec3 lightIn = (uAmb * (0.6 + 0.4 * n.y) + uMoonCol * max(dot(n, uMoonDir), 0.0) + uUpCol * up * (0.5 - 0.5 * n.y)
                    + uWallGlow) * uWallK;
    vec3 R = reflect(V, n);
    float fres = 0.04 + 0.96 * pow(1.0 - max(dot(-V, n), 0.0), 5.0);
    float s3 = fract(seed * 3.31), s5 = fract(seed * 5.17), s7 = fract(seed * 7.7), s11 = fract(seed * 11.3), s13 = fract(seed * 13.7);
    vec3 c;
    vec3 emit = vec3(0.0);
    // the distance to the face's vertical edges (the building's corners) and to its top
    float edgeD = abs(n.x) > 0.5 ? vA0.w - abs(vW.z - vA0.y) : vA0.z - abs(vW.x - vA0.x);
    float pxW = fwidth(vW.y);
    if (n.y > 0.5) {
        // a roof: gravel, its parapet's coping lighter, the crown's light along its edges, snow
        c = alb * 0.7 * lightIn * (0.8 + 0.4 * vnoise(vW.xz * 1.3));
        vec2 e = vec2(vA0.z - abs(vW.x - vA0.x), vA0.w - abs(vW.z - vA0.y));
        float ed = min(e.x, e.y);
        float edge = smoothstep(0.5, 0.0, ed);
        if (st != 2 && st < 6) c *= 1.0 + 0.6 * smoothstep(0.25, 0.1, ed);
        emit += uCrownCol * vA2.y * edge * 2.0;
        if (uSnow > 0.0) c = mix(c, uSnowCol * (0.5 + lightIn), uSnow * 0.9);
    } else if (n.y < -0.5 || st == 2) {
        c = alb * lightIn;
        if (st == 2) c *= 1.0 + 0.5 * smoothstep(pxW * 2.0, 0.0, edgeD); // a lit edge
    } else if (st == 6) {
        // a plant room: louvres, the night on their slats
        float l = fract(vW.y * 5.0);
        float det = 1.0 - smoothstep(0.04, 0.12, pxW);
        c = alb * lightIn * mix(1.0, mix(0.5, 1.2, smoothstep(0.2, 0.6, l) * smoothstep(1.0, 0.8, l)), det);
        c *= 1.0 + 0.4 * smoothstep(pxW * 2.0, 0.0, edgeD);
    } else if (st == 7) {
        // a water tank: wooden staves, iron hoops
        float u = abs(n.x) > 0.5 ? vW.z : vW.x;
        float det = 1.0 - smoothstep(0.03, 0.1, pxW);
        float stave = mix(1.0, 0.75 + 0.35 * step(0.18, fract(u * 7.0)), det);
        float hy = (vW.y - vA1.x) / (vA1.y - vA1.x);
        float hoop = det * (smoothstep(0.04, 0.0, abs(hy - 0.2)) + smoothstep(0.04, 0.0, abs(hy - 0.5)) + smoothstep(0.04, 0.0, abs(hy - 0.8)));
        c = alb * lightIn * stave * (1.0 - 0.5 * hoop);
        // round: darker towards its sides
        c *= 0.55 + 0.45 * smoothstep(0.0, 0.5, edgeD / max(vA0.z, 0.1));
    } else if (st == 8) {
        // a rooftop sign: its letters lit from behind on a dark panel (no neon: a soft white), on the face to the city
        c = alb * lightIn;
        if (n.z > 0.5) {
            float px_ = (vW.x - (vA0.x - vA0.z)) / (2.0 * vA0.z);
            float py_ = (vW.y - vA1.x) / (vA1.y - vA1.x);
            int w = int(vA2.z + 0.5);
            int len = 0;
            for (int i = 0; i < 8; i++) if (uGlyph[w * 8 + i] != 0) len = i + 1;
            float L = float(max(len, 1));
            float cx = px_ * L;
            int li = int(floor(cx));
            vec2 g = vec2((fract(cx) - 0.14) / 0.72, (py_ - 0.14) / 0.72); // the glyph's box in its cell
            float on = 0.0;
            if (li >= 0 && li < len && g.x >= 0.0 && g.x < 1.0 && g.y >= 0.0 && g.y < 1.0) {
                int gx = int(g.x * 3.0), gy = 4 - int(g.y * 5.0);
                on = float((uGlyph[w * 8 + li] >> (gy * 3 + gx)) & 1);
            }
            vec3 lc = mix(uLamp[2], vec3(1.0), 0.3) * lumv(uCityLight) * 1.3;
            float wake = smoothstep(0.3, 0.6, uWake);
            emit += lc * (on * 0.9 + 0.04) * awake * wake;
        }
    } else {
        // ---- A facade: its bays and floors (a floor is one unit, counted from the street so the setbacks align)
        float u = abs(n.x) > 0.5 ? vW.z : vW.x;
        float v = vW.y - uGroundY;
        float bay = st == 0 ? mix(1.3, 1.9, s7) : (st == 1 ? mix(1.1, 1.4, s7) : (st == 3 ? mix(0.9, 1.15, s7)
                  : (st == 4 ? mix(1.7, 2.3, s7) : mix(1.0, 1.3, s7))));
        vec2 q = vec2(u / bay, v);
        vec2 id = floor(q);
        vec2 f = fract(q);
        vec2 fw = fwidth(q);
        float fwm = max(fw.x, fw.y);
        float tiny = smoothstep(0.2, 0.55, fwm);      // a window is a pixel or less: the averages
        float det = 1.0 - smoothstep(0.07, 0.2, fwm);  // the details of a window, up close
        float detB = 1.0 - smoothstep(0.02, 0.05, fwm); // the bricks, very close only
        vec2 aw = fw * 0.75;
        vec2 key = id + vec2(seed * 37.0, seed * 11.0) + n.xz * 13.0;
        bool office = st == 0 || st == 5 || (st == 3 && s13 < 0.6);
        // the opening in its bay
        vec2 lo, hi;
        bool balc = false;
        if (st == 0) {
            int ws = int(s3 * 3.0);
            lo = vec2(0.0, ws == 0 ? 0.3 : (ws == 1 ? 0.22 : 0.38));
            hi = vec2(1.0, 0.98);
        } else if (st == 1) {
            lo = vec2(0.27, 0.27); hi = vec2(0.73, 0.8);
            // stacked balconies: on every bay or every other one (French windows behind)
            balc = s5 < 0.6 && (s11 < 0.45 || mod(id.x, 2.0) < 1.0) && id.y >= 1.0;
            if (balc) { lo = vec2(0.22, 0.06); hi = vec2(0.78, 0.8); }
        } else if (st == 3) {
            lo = vec2(0.26, 0.12); hi = vec2(0.74, 0.88);
        } else if (st == 4) {
            lo = vec2(0.07, 0.0); hi = vec2(0.93, 0.82);
        } else {
            lo = vec2(0.16, 0.26); hi = vec2(0.84, 0.84);
        }
        float win = rectm(f, lo, hi, aw);
        float area = (hi.x - lo.x) * (hi.y - lo.y);
        win = mix(win, area, tiny);
        win *= step(v, H - 0.8);
        vec2 wl = clamp((f - lo) / (hi - lo), 0.0, 1.0);

        // ---- which windows are lit: offices whole runs of bays, homes window by window; put out and lit again
        // slowly (each its own period)
        vec2 lkey = office ? vec2(floor(id.x / (2.0 + floor(s11 * 4.0))), id.y) * vec2(1.0, 1.0) + vec2(seed * 37.0, seed * 11.0) + n.xz * 13.0 : key;
        float h = hash12(lkey);
        float per = 22.0 + 55.0 * h;
        float ep = floor((uTime + h * per) / per);
        float frac = uLitFrac * vA2.x;
        float lit = mix(step(hash12(lkey * 1.37 + ep * 3.7), frac), frac, tiny);
        // waking or going to sleep: the windows switched on, or put out, one by one
        lit *= mix(step(hash12(key * 0.71 + 5.3), uWake * 1.02 - 0.01), uWake, tiny);
        // each window a real lamp: offices fluorescent and halogen; homes tungsten mostly, halogen, the odd TV, a
        // sodium stairwell
        float pk = hash12(lkey * 2.1 + ep);
        vec3 lampC = office ? (pk < 0.55 ? uLamp[1] : (pk < 0.8 ? uLamp[2] : (pk < 0.95 ? uLamp[0] : uLamp[4])))
                            : (pk < 0.58 ? uLamp[0] : (pk < 0.72 ? uLamp[2] : (pk < 0.82 ? uLamp[1] : (pk < 0.93 ? uLamp[3] : uLamp[4]))));
        vec3 wc = lampC * lumv(uCityLight) * (0.5 + 0.8 * hash12(lkey + ep * 3.1));
        // what hangs in a lit window: blinds in offices; curtains (drawn to the sides, drawn closed with a chink),
        // blinds, a sheer in homes, warm or cool
        float shape = 1.0;
        vec3 ftint = vec3(1.0);
        if (office) {
            shape = 0.75 + 0.45 * smoothstep(0.3, 1.0, wl.y); // the ceiling lights
            if (hash12(key * 3.3) < 0.5) {
                float cov = 0.2 + 0.6 * hash12(key * 4.1);
                shape *= mix(1.0, 0.5 + 0.5 * step(0.45, fract(wl.y * 9.0)), step(1.0 - cov, wl.y));
            }
        } else {
            float ct = hash12(key * 4.7);
            float cx = min(wl.x, 1.0 - wl.x);
            if (ct < 0.3) shape = mix(0.3, 1.0, smoothstep(0.1, 0.28, cx));
            else if (ct < 0.5) shape = 0.55 * (0.85 + 0.15 * sin(wl.x * 45.0)) + 0.7 * smoothstep(0.05, 0.0, abs(wl.x - 0.5));
            else if (ct < 0.62) shape = 0.55 + 0.45 * step(0.4, fract(wl.y * 7.0));
            else if (ct < 0.72) shape = 0.45 + 0.75 * smoothstep(0.6, 0.0, wl.y);
            else shape = 0.85 + 0.25 * smoothstep(0.3, 1.0, wl.y);
            float tt = hash12(key * 6.1);
            ftint = tt < 0.3 ? vec3(1.2, 0.8, 0.6) : (tt < 0.5 ? vec3(1.05, 0.95, 0.82) : (tt < 0.6 ? vec3(0.88, 0.96, 1.12) : vec3(1.0)));
        }
        // the window's frame: its bars (a cross in homes, a transom in offices), up close
        float bars = 0.0;
        if (st == 1 || st == 3 || st == 5) {
            float bx = abs(wl.x - 0.5) * (hi.x - lo.x), by = abs(wl.y - (st == 1 ? 0.72 : 0.78)) * (hi.y - lo.y);
            bars = max(smoothstep(0.022 + aw.x, 0.022 - aw.x, bx), smoothstep(0.02 + aw.y, 0.02 - aw.y, by));
        } else if (st == 0) {
            bars = smoothstep(0.012 + aw.y, 0.012 - aw.y, abs(f.y - 0.8)) * 0.6;
        }
        bars *= det;
        shape = mix(0.85, shape * (1.0 - 0.75 * bars), det);
        ftint = mix(vec3(1.0), ftint, 0.25 + 0.75 * det);
        wc *= ftint;
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
            shape = mix(shape, 0.9, uDropK * (patterned ? 1.0 : 0.0));
        }

        // ---- the materials
        float big = vnoise(vec2(u * 0.12 + seed * 50.0, v * 0.07)); // weathering, large
        vec3 wallA = alb * (0.85 + 0.3 * big);
        vec3 stone = mix(alb, vec3(0.42, 0.4, 0.36), 0.6); // sills, lintels, copings
        if (st == 1 && detB > 0.0) {
)"
R"(            // brick courses (half a brick offset), each brick its own tone, the mortar paler
            float cy = v * 12.0;
            float cxx = u * 5.0 + 0.5 * floor(cy);
            float mort = max(smoothstep(0.16, 0.04, fract(cy)), smoothstep(0.08, 0.02, fract(cxx)));
            float bn = hash12(vec2(floor(cxx), floor(cy)) + seed * 9.0);
            wallA *= mix(1.0, (0.82 + 0.36 * bn) * (1.0 + 0.5 * mort), detB);
        }
        vec3 body = wallA * lightIn;
        // the building's corners and its top caught by the light, so it reads as a volume
        float corner = smoothstep(pxW * 2.5 + 0.06, 0.0, edgeD);
        float cap = smoothstep(H - 0.55, H - 0.4, v) * (st == 0 ? 0.6 : 1.0);
        body = mix(body, stone * lightIn * 1.4, max(cap, corner * 0.5) * (st == 0 ? 0.5 : 1.0));
        // the glass of a dark window: dark, the sky on it (a little more on a glass tower, each pane its own tilt)
        float tilt = (hash12(key * 9.7) - 0.5) * 0.08 * det;
        vec3 sky = skyColor(normalize(R + vec3(tilt, tilt * 0.6, 0.0)));
        float refl = vA3.a * (st == 0 ? 1.0 : 0.3);
        vec3 gtint = st == 0 ? (s13 < 0.3 ? vec3(0.8, 1.0, 1.0) : (s13 < 0.55 ? vec3(1.1, 0.92, 0.75) : (s13 < 0.8 ? vec3(0.82, 0.92, 1.15) : vec3(1.0)))) : vec3(1.0);
        vec3 glass = alb * 0.15 * lightIn + sky * (fres + (st == 0 ? 0.1 : 0.02)) * refl * 1.6 * gtint;
        float sill = 0.0, rail = 0.0, slab = 0.0;
        float lo2 = 1.0; // what the window's light shows (a parapet, a railing in front of it)
        if (st == 0) {
            // a curtain wall: the spandrel band an opaque panel (some sky on it), the mullions catching the light
            float sp = 1.0 - smoothstep(lo.y - aw.y, lo.y + aw.y, f.y);
            vec3 panelA = mix(alb, s11 < 0.5 ? vec3(0.22, 0.23, 0.25) : vec3(0.1, 0.1, 0.11), 0.7);
            body = mix(body, panelA * lightIn + sky * fres * refl * 0.5 * gtint, sp * (1.0 - cap));
            float mull = smoothstep(0.015 + aw.x, 0.015 - aw.x, min(f.x, 1.0 - f.x)) * det;
            float slabL = smoothstep(0.02 + aw.y, 0.02 - aw.y, abs(f.y - lo.y)) * det;
            glass = mix(glass, stone * lightIn * 0.9 + sky * 0.1, max(mull, slabL));
            lo2 *= 1.0 - max(mull, slabL);
        } else if (st == 3) {
            // art deco: the piers run up unbroken, the panels between the windows dark metal with a line of ornament
            float inCol = rectm(vec2(f.x, 0.5), vec2(lo.x, 0.0), vec2(hi.x, 1.0), aw);
            float panel = inCol * (1.0 - win);
            float orn = smoothstep(0.02 + aw.y, 0.0, abs(fract(f.y + 0.5) - 0.5)) * det;
            body = mix(body * (1.0 + 0.25 * (1.0 - inCol)), alb * 0.25 * lightIn * (1.0 + 2.0 * orn), panel);
            // the pier's groove
            body *= 1.0 - 0.35 * det * smoothstep(0.02 + aw.x, 0.0, abs(f.x - 0.0)) * (1.0 - inCol);
            // its top floodlit from its setbacks (some): a warm wash rising up the last floors
            float wash = smoothstep(H - 7.0, H - 0.5, v) * smoothstep(H - 7.0, H - 3.0, v) * step(s11, 0.65) * (1.0 - win);
            emit += wallA * uLamp[0] * lumv(uCityLight) * 0.25 * wash * awake * uWake;
        } else if (st == 4) {
            // loggias: deep openings in their own shadow (darker under their ceiling), a glass door and a window at
            // the back, in front a concrete parapet or a railing (each building its own), the slab's edge
            // the loggia traced in perspective: its side walls, its ceiling, its floor, its back wall
            const float D = 0.9;
            vec3 tu = abs(n.x) > 0.5 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
            float vn = max(-dot(V, n), 0.05);
            vec2 dd = vec2(dot(V, tu) / bay, V.y) / vn; // the cell's coordinates per unit of depth
            float sx = dd.x > 0.0 ? (hi.x - f.x) / dd.x : (lo.x - f.x) / min(dd.x, -1e-4);
            float sy = dd.y > 0.0 ? (hi.y - f.y) / dd.y : (0.0 - f.y) / min(dd.y, -1e-4);
            float sd = min(D, min(max(sx, 0.0), max(sy, 0.0)));
            vec2 fb = f + dd * sd;
            vec2 wb = (fb - lo) / (hi - lo);
            float near = sd / D; // 0 at the facade, 1 at the back
            vec3 lw;
            float gm = 0.0;
            if (sd >= D - 1e-4) {
                // the back: a glass door, a window, the wall between
                vec2 aww = aw / (hi - lo);
                gm = max(rectm(wb, vec2(0.07, 0.0), vec2(0.43, 0.88), aww), rectm(wb, vec2(0.52, 0.36), vec2(0.93, 0.88), aww));
                lw = mix(alb * 0.12 * lightIn, glass * 0.6, gm);
            } else if (sx < sy) lw = alb * 0.5 * lightIn * (1.0 - 0.5 * near);   // a side wall
            else if (dd.y > 0.0) lw = alb * 0.06 * lightIn;                      // the ceiling
            else lw = alb * 0.35 * lightIn;                                       // the floor
            lw = mix(lw, alb * 0.12 * lightIn + glass * 0.2, 1.0 - det);
            glass = lw;
            // its walls lit by the room, more near the glass
            emit += wc * lit * awake * win * (1.0 - gm) * 0.22 * near * near * det;
            bool railing = s11 > 0.55;
            float para = rectm(f, vec2(lo.x - 0.01, -1.0), vec2(hi.x + 0.01, 0.36), aw);
            float cop = rectm(f, vec2(lo.x, 0.32), vec2(hi.x, 0.36), aw) * det;
            if (railing) {
                float bars2 = smoothstep(0.3, 0.1, abs(fract(f.x * bay * 9.0) - 0.5) * 2.0);
                rail = para * mix(0.5, max(bars2, cop), det);
                para = 0.0;
            }
            body = mix(body, stone * lightIn * 1.5, cop * (railing ? 0.0 : 1.0));
            float slabE = smoothstep(hi.y + 0.01, hi.y + 0.03, f.y) * smoothstep(1.0, 0.97, f.y);
            body = mix(body, stone * lightIn * 1.2, slabE * det);
            win *= 1.0 - para * (1.0 - tiny);
            lo2 *= (1.0 - para) * mix(0.35, gm, det); // (its walls: above)
        } else {
            // punched windows: a stone sill and a lintel (brick, precast), the reveal's shadow
            sill = balc ? 0.0 : rectm(f, vec2(lo.x - 0.04, lo.y - 0.05), vec2(hi.x + 0.04, lo.y), aw);
            float lint = st == 1 ? rectm(f, vec2(lo.x - 0.03, hi.y), vec2(hi.x + 0.03, hi.y + 0.06), aw) : 0.0;
            body = mix(body, stone * lightIn * 1.25, max(sill, lint * 0.7) * det);
            float rev = rectm(wl, vec2(0.0, 0.9), vec2(1.0, 1.1), aw / (hi - lo)) + rectm(wl, vec2(-0.1, 0.0), vec2(0.07, 1.0), aw / (hi - lo));
            glass *= 1.0 - 0.6 * min(rev, 1.0) * det * (st == 5 ? 1.0 : 0.6);
            if (balc) {
                // the balcony: its slab at the floor, the railing in front of the French window, the slab above's
                // shadow
                slab = rectm(f, vec2(0.04, -0.1), vec2(0.96, 0.05), aw);
                rail = rectm(f, vec2(0.06, 0.05), vec2(0.94, 0.38), aw);
                float bars2 = smoothstep(0.3, 0.1, abs(fract(f.x * bay * 11.0) - 0.5) * 2.0);
                float hand = smoothstep(0.33, 0.35, f.y);
                rail *= mix(0.55, max(bars2, hand), det);
                float shade = rectm(f, vec2(0.04, 0.9), vec2(0.96, 1.1), aw);
                body *= 1.0 - 0.65 * shade * det;
            }
        }
        // a window's light: brightest under its ceiling
        float room = mix(0.75 + 0.5 * smoothstep(0.3, 0.8, wl.y), 1.0, tiny);
        vec3 wl3 = wc * lit * room * shape * awake;
        emit += wl3 * win * lo2;
        // the room's light around its window: on the sill, on the balcony
        emit += wc * lit * awake * det * (sill * 0.12 + slab * 0.1);
        // the dark window, or the wall
        c = mix(body, glass, win);
        c = mix(c, stone * lightIn * 1.1, bars * win * 0.8);
        // the balcony in front: its slab's edge, the railing (a silhouette against a lit window)
        if (slab > 0.0 || rail > 0.0) {
            vec3 conc = stone * lightIn * 2.2;
            c = mix(c, conc, slab);
            emit *= 1.0 - slab;
            emit += wc * lit * awake * 0.08 * slab;
            vec3 metal = alb * 0.4 * lightIn;
            c = mix(c, metal, rail);
            emit *= 1.0 - rail * 0.85;
            // some balconies lit: a lamp under the slab above
            float bl = step(hash12(key * 5.9), 0.14) * step(0.5, lit);
            vec2 lp = (f - vec2(0.15, 0.93)) / vec2(0.06, 0.04);
            emit += uLamp[0] * lumv(uCityLight) * 1.4 * exp(-dot(lp, lp)) * bl * awake * det;
        }
        // snow on the sills, the balconies' slabs and rails
        if (uSnow > 0.0) {
            float ledge = max(sill * smoothstep(lo.y - 0.03, lo.y, f.y), slab * smoothstep(0.02, 0.05, f.y));
            ledge = max(ledge, rail * smoothstep(0.34, 0.37, f.y));
            c = mix(c, uSnowCol * (lightIn * 3.0 + 0.01), uSnow * 0.7 * ledge * det);
        }
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
uniform float uTowerSeed; // the night: each its own tower (the floors' uses, the windows' furnishings)
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

// The floor's use, by bands of three floors (the two above the well together): 0 offices, 1 flats, 2 flats with
// balconies (French windows), 3 a hotel. Each night its own tower.
int bandOf(int row) { return row < 2 ? 0 : 1 + (row - 2) / 3; }
int floorKind(int row) {
    const int PAT[8] = int[](3, 2, 1, 0, 2, 1, 0, 2);
    return PAT[(bandOf(row) + int(uTowerSeed + 0.5) * 3) % 8];
}
float winSeed(int col, int row) { return hash12(vec2(float(col) * 3.1 + 0.7, float(row) * 1.9 + 0.2 + uTowerSeed * 5.3)); }
float winSeed2(int col, int row) { return hash12(vec2(float(col) * 7.3 + 2.1, float(row) * 4.7 + 9.4 + uTowerSeed * 3.1)); }

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
        if (surf == 1) dark = 0.7 + 0.3 * step(0.5, fract(u.y * 3.0 + s)); // filing shelves
        if (surf == 2) dark = 0.6 + 0.4 * step(0.5, fract(u.x * 3.0));
    } else if (kind == 1) {
        // a flat: a sofa against the back wall, a picture, shelves, the lamp's shade glowing
        float sx = 0.5 + (fract(s * 3.7) - 0.5) * 0.3;
        if (surf == 0 && u.y < 0.3 && abs(u.x - sx) < 0.3) dark = 0.3;
        if (surf == 0 && u.y < 0.36 && u.y > 0.28 && abs(u.x - sx) < 0.3) dark = 0.5; // the sofa's back
        if (surf == 0 && abs(u.y - 0.62) < 0.1 && abs(u.x - (1.0 - sx)) < 0.12)
            dark = abs(u.y - 0.62) > 0.08 || abs(u.x - (1.0 - sx)) > 0.1 ? 0.3 : 0.6; // a framed picture
        if (surf == 1) dark = 0.75 + 0.25 * step(0.5, fract(u.y * 4.0 + s)); // shelves
        if (surf == 2) dark = 0.75;
        // the floor lamp: a shade glowing, its stand
        vec3 sh = H - (lamp + vec3(0.0, 0.12, 0.0));
        add += mix(L, vec3(lumv(L)), 0.3) * 1.6 * exp(-dot(sh, sh) * 90.0);
    } else {
        // a hotel room: a bed's head against the back wall, two bedside lamps' pools, a warm ceiling light
        if (surf == 0 && u.y < 0.36 && abs(u.x - 0.5) < 0.35) dark = 0.45;
        if (surf == 0 && u.y < 0.5 && u.y > 0.36 && abs(u.x - 0.5) < 0.35) dark = 0.7;
        if (surf == 0) add += L * 0.35 * (exp(-pow((u.x - 0.1) * 9.0, 2.0) - pow((u.y - 0.5) * 6.0, 2.0)) +
                                         exp(-pow((u.x - 0.9) * 9.0, 2.0) - pow((u.y - 0.5) * 6.0, 2.0)));
        if (surf == 3) add += L * 1.5 * exp(-dot(u.xz - 0.5, u.xz - 0.5) * 40.0);
    }
    vec3 lit = Lk * tint * (alb * k * dark) + add;
    // a little light from outside, the corner shading
    float ao = 0.7 + 0.3 * smoothstep(0.0, 0.15, min(min(u.x, 1.0 - u.x), min(u.y, 1.0 - u.y)));
    return lit * ao + vec3(amb) * alb * dark;
}

// A window box of plants, as a silhouette (q: units from its middle at its foot, w: its half width): the box, the
// leaves' ragged line above it.
float plants(vec2 q, float w, float s, float px) {
    float box = step(abs(q.x), w) * step(0.0, q.y) * step(q.y, 0.05);
    float top = 0.07 + 0.035 * abs(sin(q.x * 47.0 + s * 20.0)) + 0.02 * sin(q.x * 113.0 + s * 7.0)
              + 0.03 * smoothstep(0.6, 1.0, sin(q.x * 13.0 + s * 31.0));
    float leaves = smoothstep(top + px * 0.5, top - px * 0.5, q.y) * step(0.0, q.y) * step(abs(q.x), w * 0.96);
    return max(box, leaves);
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
    // the night's light that reaches into a window (the sky, the moon, the streets): what shows a dark window's
    // curtains, blinds, frames
    vec3 outL = (uAmb * 2.0 + uMoonCol * 0.8 + uUpCol * up * 0.4) * uFrameLight * 0.6 + uFlood * 0.03;
    float ghostLine = 0.0, ghostK = 0.0;
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
        // each floor its use, each window its own furnishings (they belong to the window, whatever piece lands)
        int kind = floorKind(row);
        bool balc = kind == 2;
        float s = winSeed(col, row), s2 = winSeed2(col, row);
        float bs = hash12(vec2(float(bandOf(row)) * 2.3 + uTowerSeed * 1.7, 7.7)); // the band's own choices
        // the window: an opening in the facade, the glass set back in it (French windows down to the floor on the
        // balconies' floors)
        float gx = 0.5 - uMull * 0.5;
        float gt = 0.5 - uSpand * 0.3, gb = balc ? -0.47 : -(0.5 - uSpand * 0.7);
        vec2 oc = vec2(0.0, (gt + gb) * 0.5), oh = vec2(gx, (gt - gb) * 0.5);
        vec2 d = abs(lp - oc) - oh;
        float sd = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
        float inO = 1.0 - smoothstep(-px * 0.7, px * 0.7, sd);
        // the facade: mullions and spandrels; a stone sill under each window, lit by its room, its shadow below
        vec3 fr = uFrame * lightIn;
        float sillW = step(abs(lp.x), gx + 0.04);
        float sill = balc ? 0.0 : smoothstep(gb - 0.07, gb - 0.055, lp.y) * smoothstep(gb + 0.005, gb - 0.01, lp.y) * sillW;
        float under = balc ? 0.0 : smoothstep(gb - 0.07, gb - 0.075, lp.y) * smoothstep(gb - 0.13, gb - 0.08, lp.y) * sillW;
        fr = mix(fr, (lightIn * 0.6 + outL) * 0.16, sill) * (1.0 - 0.45 * under);
        vec3 spill = texture(uCellA, vec2((P.x + 5.0) / 10.0, (12.0 - P.y) / 22.0)).rgb;
        fr += spill * 0.04 + A * 0.2 * sill + A * 0.06 * smoothstep(0.08, 0.0, sd);
        fr += reflection(reflect(r, vec3(0.0, 0.0, 1.0))) * 0.06 * uReflK;
        if (uSnow > 0.0) fr = mix(fr, uSnowCol * (lightIn * 3.0 + A * 0.5), uSnow * sill);
        vec3 g = vec3(0.0);
        vec3 Lt = A + uGhostCol * B.r * 0.08; // the room's light
        if (inO > 0.0) {
            // the reveal: the depth of the opening, seen in perspective
            float rd = 0.05 + 0.28 * uRecess;
            vec3 Q = P + r * (rd / -r.z);
            vec2 ql = Q.xy - ctr - oc;
            if (abs(ql.x) > oh.x || abs(ql.y) > oh.y) {
                vec3 rv = uFrame * lightIn * 0.6 + A * 0.4 * (0.5 + 0.5 * smoothstep(-oh.y, oh.y, ql.y));
                if (ql.y > oh.y) rv *= 0.5; // the head of the opening, in its own shadow
)"
R"(                g = rv;
            } else {
                vec2 uv = ql / (2.0 * oh) + 0.5;
                vec2 qu = ql + oh; // units from the opening's lower left corner
                vec3 lo = vec3(ctr.x - 0.49, ctr.y - 0.47, -rd - 1.3);
                vec3 hi = vec3(ctr.x + 0.49, ctr.y + 0.48, -rd);
                bool on = lumv(A) > 0.0005;
                int rk = kind == 0 ? 0 : (kind == 3 ? 2 : 1);
                vec3 room = roomAt(Q, r, lo, hi, Lt * 0.85, rk, s, uRoomAmb);
                // what hangs at the glass, lit from behind by the room (its light through the fabric) and from the
                // front by the night
                float lampU = rk == 1 ? 0.2 + 0.6 * fract(s * 7.31) : 0.5;
                float glow = rk == 1 ? 0.55 + 0.6 * exp(-(uv.x - lampU) * (uv.x - lampU) * 8.0) * (1.0 - 0.4 * uv.y)
                                     : 0.5 + 0.55 * uv.y;
                vec3 back = Lt * 0.62 * glow + vec3(uRoomAmb) * 0.6;
                // the fabric: its lightness and color (seen on a dark window; the room's light through it keeps
                // the lamp's own color)
                float fa = 0.5 + 0.5 * fract(s * 29.0);
                float fh = fract(s * 43.0);
                vec3 fabT = fh < 0.3 ? vec3(1.0, 0.93, 0.8) : (fh < 0.5 ? vec3(1.0) : (fh < 0.65 ? vec3(0.72, 0.82, 1.0)
                          : (fh < 0.78 ? vec3(0.95, 0.5, 0.42) : (fh < 0.9 ? vec3(0.8, 0.86, 0.6) : vec3(1.0, 0.78, 0.5)))));
                if (kind == 0) fabT = vec3(0.95, 0.97, 1.0); // office blinds: white
                if (kind == 3) fabT = bs < 0.5 ? vec3(1.0, 0.92, 0.78) : vec3(0.85, 0.85, 0.9); // a hotel's own
                // (the night's light on it only shows on a dark window: on a lit one the lamp's color stays pure)
                vec3 front = outL * fa * fabT * 1.3 * smoothstep(0.06, 0.0, lumv(Lt));
                // a lit window reads first as a pane of its lamp's color: the room's furniture only a soft shade in it
                if (on) room = mix(room, max(room, back), 0.55);
                // 0 nothing, 1 drapes drawn to the sides, 2 curtains drawn (a chink of light between them), 3 a
                // sheer, 4 drapes and a sheer, 5 venetian blinds lowered to cov, 6 vertical blinds
                int tr = 0;
                float cov = 0.3 + 0.45 * fract(s * 13.0);
                if (kind == 0) { tr = bs < 0.55 ? 5 : 6; cov = 0.2 + 0.7 * s; if (s2 > 0.88) tr = 0; }
                else if (kind == 3) tr = s < 0.3 ? 2 : 4;
                else if (kind == 2) tr = s < 0.4 ? 4 : (s < 0.65 ? 1 : (s < 0.85 ? 3 : 2));
                else tr = s < 0.2 ? 0 : (s < 0.38 ? 2 : (s < 0.56 ? 4 : (s < 0.7 ? 1 : (s < 0.84 ? 3 : 5))));
                float folds = 0.72 + 0.28 * sin(qu.x * 78.0 + s * 9.0);
                if (tr == 1 || tr == 4) {
                    if (tr == 4) room = mix(room, back * 0.9 + front * 0.45, 0.5);
                    float w = 0.17 + 0.1 * fract(s * 17.0);
                    float dd = min(uv.x, 1.0 - uv.x);
                    float inD = smoothstep(w + 0.015, w - 0.015, dd);
                    room = mix(room, (back * 0.7 + front) * folds, inD);
                } else if (tr == 2) {
                    float gap = abs(uv.x - 0.5 - (fract(s * 31.0) - 0.5) * 0.25);
                    float chink = smoothstep(0.03, 0.008, gap);
                    room = mix((back * 0.75 + front) * folds, room, chink);
                } else if (tr == 3) {
                    room = mix(room, back * 0.9 + front * 0.7, 0.6);
                } else if (tr == 5) {
                    float sy = fract(qu.y / 0.07);
                    float sl = smoothstep(0.0, 0.2, sy) * smoothstep(1.0, 0.75, sy); // a slat, a gap
                    float down = smoothstep(1.0 - cov - 0.01, 1.0 - cov + 0.01, uv.y);
                    room = mix(room, mix(room * 0.85, back * 1.05 + front * 1.2, sl), down);
                    room *= 1.0 - 0.3 * smoothstep(0.025, 0.0, abs(uv.y - (1.0 - cov))); // the bottom rail
                } else if (tr == 6) {
                    float sx = fract(qu.x / 0.085);
                    float sl = smoothstep(0.0, 0.2, sx) * smoothstep(1.0, 0.75, sx);
                    room = mix(room, mix(room * 0.85, back + front * 1.1, sl), 0.85);
                }
                // a plant on the sill (homes), a silhouette against the room
                if (kind == 1 && s2 > 0.8) {
                    float w = 0.12 + 0.12 * fract(s2 * 19.0);
                    float x0 = s2 > 0.9 ? w + 0.05 : 2.0 * oh.x - w - 0.05;
                    float m = plants(qu - vec2(x0, 0.0), w, s, px);
                    room = mix(room, outL * 0.1 * vec3(0.7, 1.0, 0.6) + Lt * 0.07 * vec3(0.8, 1.0, 0.7), m);
                }
                room *= uGlassTint;
                // the glass: each pane at its own slight tilt, so the reflections differ from window to window
                float cosT = -r.z;
                float F = (0.08 + 0.3 * uReflK) * (0.75 + 0.5 * s) + pow(1.0 - cosT, 5.0);
                vec3 rr = normalize(vec3(r.x + (s - 0.5) * 0.06, r.y + (s2 - 0.5) * 0.05, -r.z));
                vec3 refl = reflection(rr);
                if (uRain > 0.0) {
                    float sN = vnoise(vec2(Q.x * 26.0, Q.y * 1.3 + uTime * 1.6));
                    refl += (refl + uReflCity * 0.1) * uRain * smoothstep(0.75, 0.95, sN);
                }
                g = room + refl * F * (on ? 0.3 : 1.0);
                // the casement: its frame and bars, painted (pale) in homes, dark metal in offices
                float e = min(min(qu.x, 2.0 * oh.x - qu.x), min(qu.y, 2.0 * oh.y - qu.y));
                float fm = smoothstep(0.034 + px * 0.5, 0.034 - px * 0.5, e);
                float hb = 0.014 + px * 0.5;
                if (kind != 0) fm = max(fm, smoothstep(hb, hb - px, abs(qu.x - oh.x)));       // the two leaves
                if (kind == 1 && s2 > 0.35) fm = max(fm, smoothstep(hb, hb - px, abs(uv.y - 0.76) * 2.0 * oh.y)); // a fanlight
                if (kind == 0) fm = max(fm, smoothstep(hb, hb - px, abs(uv.y - 0.82) * 2.0 * oh.y)); // a transom
                vec3 wfA = kind == 0 ? uFrame * 3.0 + 0.06 : vec3(0.75, 0.74, 0.7);
                vec3 wf = wfA * outL * 1.3 + Lt * 0.3;
                g = mix(g, wf, fm);
            }
            // a roller shutter (flats): outside, just in front of the glass, its box at the head of the opening;
            // rolled a little way down (never shut: a lit window stays a window of light), lit by the room on a lit one
            bool sh = (kind == 1 && s2 < 0.4) || (kind == 2 && s2 < 0.2);
            if (sh) {
                float rs = rd * 0.3;
                vec3 Qs = P + r * (rs / -r.z);
                vec2 qs = Qs.xy - ctr - oc;
                float shCov = 0.14 + 0.2 * fract(s2 * 17.0);
                float ys = qs.y / (2.0 * oh.y) + 0.5; // 0 at the sill, 1 at the head
                if (abs(qs.x) < oh.x && ys < 1.0) {
                    float bot = 1.0 - shCov;
                    float m = smoothstep(bot - px, bot + px * 0.2, ys);
                    float sy = fract((oh.y - qs.y) / 0.064); // the slats, from the head down
                    float prof = mix(1.15, 0.55, smoothstep(0.1, 0.95, sy)); // each slat lit from above
                    vec3 shA = vec3(0.3 + 0.25 * fract(s2 * 7.0)) * vec3(1.0, 0.98, 0.94);
                    vec3 scol = shA * outL * 2.2 * prof;
                    scol += Lt * 0.55 * prof; // the room's light on it
                    scol *= 1.0 - 0.5 * smoothstep(bot + 0.04, bot, ys) * (1.0 - step(ys, bot)); // the bottom bar
                    // the box at the head
                    float box = smoothstep(0.93, 0.935, ys);
                    scol = mix(scol, shA * outL * 1.6 + Lt * 0.4, box);
                    g = mix(g, scol, max(m, box));
                }
            }
        }
        c = mix(fr, g, inO);
        // the ghost: the window's frame lit in the piece's light (drawn last, over the balconies)
        ghostLine = 1.0 - smoothstep(0.0, max(px * 1.4, 0.022), abs(sd - 0.03));
        ghostK = B.r;

        // ---- The balconies (flats with balconies): a slab at the floor of each window, a railing on it, a plant
        // sometimes; traced from the eye (they stand out of the facade)
        const float ZS = 0.36, ZR = 0.33, ZP = 0.18, TH = 0.07, RH = 0.36, HW = 0.47;
        // the slab's top (seen from above) or underside (from below): the floor lines around this point
        float zh = -1.0;
        vec3 slabC = vec3(0.0);
        {
            float yb = r.y < 0.0 ? floor(P.y) + 1.0 : floor(P.y);
            float t = (P.y - yb) / r.y;
            float z = -r.z * t;
            int sr = 11 - int(yb);
            if (t > 0.0 && z < ZS && sr >= 0 && sr <= 21 && floorKind(sr) == 2) {
                float hx = P.x - r.x * t;
                int sc = int(floor(hx + 5.0));
                if (sc >= 0 && sc <= 9 && abs(hx - (float(sc) - 4.5)) < HW) {
                    zh = z;
                    vec3 Ar = texelFetch(uCellA, ivec2(sc, sr), 0).rgb;
                    vec3 Ab = sr < 21 ? texelFetch(uCellA, ivec2(sc, sr + 1), 0).rgb : vec3(0.0);
                    float near = exp(-z * 5.0);
                    if (r.y < 0.0) slabC = outL * 0.3 + Ar * 0.5 * near;                  // its top, the room's light on it
                    else slabC = uFlood * 0.1 + outL * 0.1 + Ab * 0.12 * near;             // under it: the floodlights, the room below
                    if (uSnow > 0.0 && r.y < 0.0) slabC = mix(slabC, uSnowCol * lightIn * 2.0, uSnow * 0.8);
                }
            }
        }
        if (zh >= 0.0 && zh < ZP) c = slabC;
        // a window box of plants on some balconies, against the railing
        {
            vec3 Pp = P - r * (ZP / -r.z);
            float yb = floor(Pp.y);
            int pr = 11 - int(yb);
            int pc = int(floor(Pp.x + 5.0));
            if (pr >= 0 && pr <= 21 && pc >= 0 && pc <= 9 && floorKind(pr) == 2) {
                float ps = winSeed2(pc, pr);
                if (ps > 0.6) {
                    float w = 0.12 + 0.14 * fract(ps * 23.0);
                    float x0 = (float(pc) - 4.5) + (ps > 0.8 ? -1.0 : 1.0) * (0.44 - w);
                    float m = plants(Pp.xy - vec2(x0, yb), w, ps, px);
                    vec3 Ar = texelFetch(uCellA, ivec2(pc, pr), 0).rgb;
                    c = mix(c, outL * 0.1 * vec3(0.7, 1.0, 0.6) + Ar * 0.07 * vec3(0.8, 1.0, 0.7), m);
                }
            }
        }
        if (zh >= ZP && zh < ZR) c = slabC;
        // the railing: metal balusters and a handrail, or a glass balustrade (each band its own)
        {
            vec3 Pr = P - r * (ZR / -r.z);
            float yb = floor(Pr.y);
            int rr = 11 - int(yb);
            int rc = int(floor(Pr.x + 5.0));
            float yl = Pr.y - yb;
            if (rr >= 0 && rr <= 21 && rc >= 0 && rc <= 9 && floorKind(rr) == 2) {
                float xl = Pr.x - (float(rc) - 4.5);
                if (abs(xl) < HW && yl < RH) {
                    vec3 Ar = texelFetch(uCellA, ivec2(rc, rr), 0).rgb;
                    float rbs = hash12(vec2(float(bandOf(rr)) * 2.3 + uTowerSeed * 1.7, 7.7));
                    vec3 metal = outL * 0.35 + Ar * 0.3; // (lit by the room behind it: no black bars over a lit window)
                    float rail = smoothstep(RH - 0.035 - px * 0.5, RH - 0.035 + px * 0.5, yl);
                    float post = smoothstep(0.03 + px * 0.5, 0.03 - px * 0.5, HW - abs(xl));
                    if (rbs < 0.5) {
                        // balusters
                        float bx = abs(fract(xl / 0.085) - 0.5) * 0.085;
                        float bar = smoothstep(0.009 + px * 0.5, 0.009 - px * 0.5, bx);
                        float low = smoothstep(0.035 + px * 0.5, 0.035 - px * 0.5, abs(yl - 0.03));
                        c = mix(c, metal, max(max(bar, low), post));
                    } else {
)"
R"(                        // frosted glass: the room's light through it, the night on its face
                        vec3 panel = c * 0.8 + outL * 0.12 + Ar * 0.15;
                        c = mix(c, panel, 1.0 - post);
                        c = mix(c, metal, post);
                    }
                    vec3 hand = metal * 1.6 + outL * 0.25;
                    if (uSnow > 0.0) hand = mix(hand, uSnowCol * lightIn * 2.0, uSnow * 0.7);
                    c = mix(c, hand, rail);
                }
            }
        }
        if (zh >= ZR) c = slabC;
        // the slab's front edge, catching the floodlights
        {
            vec3 Ps = P - r * (ZS / -r.z);
            float yb = ceil(Ps.y);
            int fr2 = 11 - int(yb);
            int fc = int(floor(Ps.x + 5.0));
            if (yb - Ps.y < TH && fr2 >= 0 && fr2 <= 21 && fc >= 0 && fc <= 9 && floorKind(fr2) == 2 &&
                abs(Ps.x - (float(fc) - 4.5)) < HW) {
                vec3 Ar = texelFetch(uCellA, ivec2(fc, fr2), 0).rgb;
                c = outL * 0.4 + uFlood * 0.06 + Ar * 0.03;
                if (uSnow > 0.0) c = mix(c, uSnowCol * lightIn * 2.0, uSnow * smoothstep(0.02, 0.0, yb - Ps.y));
            }
        }
        if (ghostK > 0.001) c = mix(c, uGhostCol * 1.1, ghostLine * ghostK);
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

#pragma once
// CORAL REEF's shaders. The world (meters, at a reduced resolution, with depth): the water (its column and the
// rippling surface seen from below), the seabed (white sand with ripples, rock outcrops that crust over as the reef
// matures), the corals, the fish and the manta, the bubbles and the coral seeds; every world shader writes its
// distance from the eye to alpha. Then, at full resolution: the composite (the shafts of sunlight integrated along
// each view ray up to that distance), the veil behind the well, the pieces, the lettering, the post.

namespace coralreef_shaders {

// Shared noise (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
float hash3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x), b = mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x);
    float c = mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x), d = mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x);
    return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}
)";

// The world's water and light (uniforms shared by the world's shaders), the seabed's height (as reef::groundH),
// the caustics.
inline const char* REEF_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uShadeC, uRimC, uWaterC, uDeepC, uHazeC, uCausticC;
uniform float uFogD, uTime, uCaustK, uCaustPhase, uBridge, uNight, uGlowK;
uniform vec3 uBridgeCol;
uniform vec4 uMounds[16];
uniform float uMoundSeed[16];
uniform int uMoundN;
const float SURF_Y = 7.5;
float moundH(vec2 p, vec4 m, float seed) {
    vec2 d = p - m.xy;
    float dd = length(d) / m.z;
    if (dd > 1.6) return 0.0;
    float a = atan(d.y, d.x);
    float edge = 1.0 + 0.18 * sin(3.0 * a + seed * 6.0) + 0.1 * sin(5.0 * a + seed * 11.0);
    float q = dd / edge;
    float prof = 1.0 - smoothstep(0.15, 1.0, q);
    float lumps = 1.0 + 0.16 * sin(d.x * 2.3 / m.z + seed * 3.0) * sin(d.y * 1.9 / m.z + seed * 5.0)
                + 0.09 * sin(d.x * 5.3 / m.z + seed * 7.0) * sin(d.y * 4.7 / m.z + seed * 2.0) * smoothstep(1.0, 0.4, q);
    return m.w * prof * lumps;
}
float moundSum(vec2 p) {
    float s = 0.0;
    for (int i = 0; i < 16; i++) { if (i >= uMoundN) break; s += moundH(p, uMounds[i], uMoundSeed[i]); }
    return s;
}
float groundBase(vec2 p) {
    float h = max(-0.045 * max(0.0, -p.y - 4.0), -3.5);
    h += 0.22 * sin(0.23 * p.x + 0.11 * p.y + 0.5) * sin(0.19 * p.y - 0.08 * p.x + 1.3) +
         0.1 * sin(0.53 * p.x + 0.9) * sin(0.41 * p.y + 0.2);
    return h;
}
// Caustics: a web of bright lines (the edges of drifting cells, warped), on what faces the sun.
float causticCells(vec2 p, float t) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 8.0, d2 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = hash2(i + g);
            o = 0.5 + 0.38 * sin(t * (0.6 + 0.5 * o.yx) + 6.2831 * o);
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return sqrt(d2) - sqrt(d1);
}
float caustic(vec3 wp) {
    // the light enters at the surface, straight up the sun's way
    vec2 q = wp.xz + uSunDir.xz / max(uSunDir.y, 0.25) * (SURF_Y - wp.y);
    q *= 0.55;
    q += 0.35 * vec2(vnoise(q * 0.6 + uCaustPhase * 0.13), vnoise(q * 0.6 + 7.3 - uCaustPhase * 0.11));
    float e = causticCells(q, uCaustPhase) * 0.65 + causticCells(q * 1.73 + 3.1, uCaustPhase * 1.2) * 0.35;
    float c = smoothstep(0.16, 0.0, e);
    return c * c * uCaustK * exp(-(SURF_Y - wp.y) * 0.05);
}
// The water seen along a direction (no geometry): the deep below, the bright water toward the surface, the sun's glow.
vec3 waterColor(vec3 v) {
    vec3 c = mix(uDeepC, uWaterC, smoothstep(-0.6, 0.12, v.y));
    c = mix(c, uHazeC, smoothstep(0.12, 0.6, v.y) * 0.55);
    c += uSunC * pow(max(dot(v, uSunDir), 0.0), 6.0) * 0.12;
    return c;
}
// Under water: the deeper, the bluer; the farther, the more of the water's own color.
vec3 waterFog(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float L = length(d);
    vec3 v = d / max(L, 1e-3);
    col *= mix(vec3(1.0), uDeepC / max(uWaterC, vec3(0.02)), sat(-wp.y * 0.12) * 0.45);
    col = mix(waterColor(v), col, exp(-L / uFogD));
    return mix(col, uBridgeCol, uBridge);
}
// Toon light: the shade (cool, tinted by the water), the light, the full sun; crisp bands.
float toonBand(float ndl) { return smoothstep(-0.08, 0.02, ndl) * 0.55 + smoothstep(0.38, 0.46, ndl) * 0.45; }
vec3 toonLight(vec3 n, float ao) {
    float ndl = dot(n, uSunDir);
    return mix(uShadeC * mix(0.6, 1.0, ao), uSunC, toonBand(ndl)) * mix(0.75, 1.0, ao);
}
vec3 rimLight(vec3 n, vec3 wp) {
    vec3 v = normalize(uEye - wp);
    return uRimC * pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.55;
}
)";

// ---------------------------------------------------------------- the water and its surface
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform vec3 uSurfC;
void main() {
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    vec3 c = waterColor(d);
    float dist = 400.0;
    if (d.y > 0.0) {
        // the surface from below: a rippling silver sheet (it mirrors the water), bright webs where the sun comes in
        float t = (SURF_Y - uEye.y) / d.y;
        vec3 p = uEye + d * t;
        vec2 q = p.xz * 0.32;
        float w1 = vnoise(q * 1.3 + vec2(uTime * 0.12, uTime * 0.05));
        float w2 = vnoise(q * 3.1 - vec2(uTime * 0.09, -uTime * 0.13));
        float web = smoothstep(0.14, 0.0, causticCells(q * 0.9 + vec2(w1, w2) * 0.6, uTime * 0.45));
        vec3 s = mix(uSurfC * 0.85, uSurfC * 1.15, w1 * 0.6 + w2 * 0.4);
        s += uSunC * web * 0.22;
        s += uSunC * pow(max(dot(d, uSunDir), 0.0), 24.0) * 0.8;
        float k = exp(-t / (uFogD * 1.3));
        c = mix(c, s, k * smoothstep(0.0, 0.08, d.y));
        dist = t;
    }
    frag = vec4(mix(c, uBridgeCol, uBridge), dist);
}
)";

// ---------------------------------------------------------------- the seabed
inline const char* GROUND_VS = R"(#version 330 core
layout(location = 0) in vec2 aXZ;
uniform mat4 uVP;
out vec3 vWorld;
out vec3 vN;
out float vRock;
void main() {
    float m = moundSum(aXZ);
    float h = groundBase(aXZ) + m;
    float e = 0.12;
    vec2 ex = vec2(e, 0.0), ez = vec2(0.0, e);
    float hx = groundBase(aXZ + ex) + moundSum(aXZ + ex) - (groundBase(aXZ - ex) + moundSum(aXZ - ex));
    float hz = groundBase(aXZ + ez) + moundSum(aXZ + ez) - (groundBase(aXZ - ez) + moundSum(aXZ - ez));
    vN = normalize(vec3(-hx, 2.0 * e, -hz));
    vWorld = vec3(aXZ.x, h, aXZ.y);
    vRock = m;
    gl_Position = uVP * vec4(vWorld - uEye, 1.0);
}
)";
inline const char* GROUND_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in float vRock;
out vec4 frag;
uniform vec3 uSandC, uSand2C, uRockC, uRock2C, uCrustC, uCrust2C;
uniform float uCover;
void main() {
    vec3 wp = vWorld;
    vec3 n = normalize(vN);
    vec2 p = wp.xz;
    float fb = fbm(p * 0.9);
    float rock = smoothstep(0.32, 0.55, smoothstep(0.08, 0.42, vRock) + (fb - 0.5) * 0.5);
    // sand: ripples (the light catching their crests), a few warm and cool patches, a fine grain
    vec2 rd = normalize(vec2(0.85, 0.53));
    float ph = dot(p, rd) * 5.5 + fbm(p * 0.35) * 5.0;
    float rip = sin(ph);
    vec3 ns = normalize(n + vec3(rd.x, 0.0, rd.y) * cos(ph) * 0.22);
    vec3 sand = mix(uSandC, uSand2C, smoothstep(0.3, 0.8, fbm(p * 0.12 + 3.0)) * 0.7);
    sand *= 0.97 + 0.05 * smoothstep(0.3, 1.0, rip);
    sand *= 0.98 + 0.04 * hash(floor(p * 40.0));
    // rock: weathered reef limestone, pitted and lumpy, crusting over (pink coralline, green turf) as the reef matures
    float r1 = fbm(p * 1.1 + 9.0), r2 = vnoise(p * 4.3);
    float crev = smoothstep(0.7, 0.8, vnoise(p * 5.0 + 3.0));
    vec3 rockC = mix(uRockC, uRock2C, smoothstep(0.35, 0.7, r1));
    rockC *= 0.8 + 0.3 * r2;
    rockC *= 1.0 - 0.3 * crev;
    float cov = fbm(p * 0.55 + 21.0);
    float crust = smoothstep(1.0 - uCover * 0.55, 1.1 - uCover * 0.55, cov + 0.2 * max(n.y, 0.0) + 0.25 * (vnoise(p * 3.1) - 0.5));
    float turf = smoothstep(0.58, 0.68, fbm(p * 1.3 + 4.0)) * smoothstep(0.15, 0.6, uCover);
    vec3 crustC = uCrustC * mix(0.7, 1.1, smoothstep(0.3, 0.8, fbm(p * 2.7 + 5.0))) * (0.85 + 0.25 * r2);
    rockC = mix(rockC, crustC, crust * 0.8);
    rockC = mix(rockC, uCrust2C * mix(0.75, 1.05, r2), turf * (1.0 - crust * 0.6));
    // the rock's own lumps (a bumpy normal)
    vec2 gq = vec2(fbm(p * 1.7 + vec2(0.13, 0.0)) - fbm(p * 1.7 - vec2(0.13, 0.0)), fbm(p * 1.7 + vec2(0.0, 0.13)) - fbm(p * 1.7 - vec2(0.0, 0.13)));
    vec3 nr = normalize(n - vec3(gq.x, 0.0, gq.y) * 2.2);
    vec3 alb = mix(sand, rockC, rock);
    vec3 nn = normalize(mix(ns, nr, rock));
    // the sand darkens a little at the rocks' feet
    float ao = 1.0 - 0.35 * smoothstep(0.02, 0.25, vRock) * (1.0 - rock);
    vec3 col = alb * toonLight(nn, ao);
    float lit = toonBand(dot(nn, uSunDir));
    col += uCausticC * caustic(wp) * (0.3 + 0.7 * lit) * mix(0.75, 0.55, rock);
    col = waterFog(col, wp);
    frag = vec4(col, length(wp - uEye));
}
)";

// ---------------------------------------------------------------- corals
inline const char* CORAL_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;  // along, param, part, phase
layout(location = 3) in vec4 iA;     // base xyz, size
layout(location = 4) in vec4 iB;     // yaw, grow, seed, kind
layout(location = 5) in vec4 iC;     // color A, sway
layout(location = 6) in vec4 iD;     // color B, glow
uniform mat4 uVP;
uniform vec2 uSurge;   // the water's surge (direction * amount)
out vec3 vWorld;
out vec3 vN;
out vec3 vLocal;
out vec4 vAttr;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float g = iB.y;
    float c = cos(iB.x), s = sin(iB.x);
    vec3 p = aPos;
    // growing: from a bud at the foot, the tips last
    float grow = g * (0.25 + 0.75 * smoothstep(0.0, 1.0, g));
    vec3 lp = p * grow;
    vec3 wp = iA.xyz + vec3(c * lp.x + s * lp.z, lp.y, -s * lp.x + c * lp.z) * iA.w;
    vec3 n = vec3(c * aNrm.x + s * aNrm.z, aNrm.y, -s * aNrm.x + c * aNrm.z);
    // the surge sways what is soft (fans, tentacles, whips), more toward the tips
    float sway = iC.w * aAttr.x * aAttr.x * iA.w * grow;
    float ph = aAttr.w * 6.2831 + iB.z * 6.2831;
    float wave = sin(uTime * 0.9 + wp.x * 0.25 + wp.z * 0.2) * 0.75 + 0.25 * sin(uTime * 2.1 + ph);
    wp.xz += uSurge * wave * sway;
    wp.y -= abs(wave) * sway * 0.08;
    vWorld = wp;
    vN = n;
    vLocal = aPos;
    vAttr = aAttr;
    vB = iB; vC = iC; vD = iD;
    gl_Position = uVP * vec4(wp - uEye, 1.0);
}
)";
inline const char* CORAL_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in vec3 vLocal;
in vec4 vAttr;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec3 uRockC, uRock2C, uCrustC, uCrust2C;
uniform float uCover;
void main() {
    int kind = int(vB.w + 0.5);
    vec3 n = normalize(vN);
    if (!gl_FrontFacing) n = -n;
    vec3 A = vC.rgb, B = vD.rgb;
    float along = vAttr.x, part = vAttr.z;
    vec3 alb = A;
    float ao = 1.0, glow = 0.0;
    float seed = vB.z * 37.0;
    if (kind == 0) {
        // brain coral: meandering grooves (the iso-lines of a warped noise), rounded ridges between them
        vec3 q = vLocal * 10.0 + seed;
        vec3 w = q + 0.55 * vec3(vnoise3(q * 0.5), vnoise3(q * 0.5 + 7.1), vnoise3(q * 0.5 + 13.7));
        float v1 = vnoise3(w), v2 = vnoise3(w * 1.9 + 5.3);
        float d = min(abs(v1 - 0.5), abs(v2 - 0.5) * 1.25);
        float aaN = max(fwidth(d), 1e-3);
        float groove = 1.0 - smoothstep(0.02, 0.02 + 2.0 * aaN + 0.03, d);
        float ridge = smoothstep(0.04, 0.2, d);
        alb = mix(mix(A, B, 0.3), mix(A, B, 0.75) * 1.08, ridge);
        alb = mix(alb, A * 0.3, groove);
        ao = mix(0.5, 1.0, 1.0 - groove) * mix(0.55, 1.0, smoothstep(0.0, 0.35, along));
        glow = ridge * 0.35;
    } else if (kind == 1) {
        // staghorn: the color deepening down the branches, pale tips, fine corallites
        alb = mix(A, B, smoothstep(0.45, 0.95, along));
        alb = mix(alb, mix(B, vec3(1.0), 0.45), smoothstep(0.93, 1.0, along));
        float dots = smoothstep(0.62, 0.72, vnoise3(vLocal * 70.0 + seed));
        alb *= 1.0 - 0.18 * dots;
        ao = mix(0.5, 1.0, smoothstep(0.0, 0.5, along));
        glow = smoothstep(0.7, 1.0, along);
    } else if (kind == 2) {
        // table coral: radial ridges and growth rings on top, the rim paler, its underside in shade
        float rad = sin(vAttr.y * 6.2831 * 46.0 + vnoise3(vLocal * 6.0) * 3.0);
        float ring = sin(along * 34.0 + vnoise3(vLocal * 4.0 + seed) * 2.0);
        alb = mix(A, B, smoothstep(0.55, 1.0, along));
        alb *= 0.9 + 0.1 * rad;
        alb *= 0.94 + 0.06 * ring;
        if (part > 0.5) { alb = A * 0.62; ao = 0.6; }
        glow = smoothstep(0.7, 1.0, along) * (1.0 - part);
    } else if (kind == 3) {
        // sea fan: a lace of fine branches (cut out of its sheet), thicker main branches from the stem
        if (part < 0.5) {
            float r = along, a = vAttr.y;
            vec2 q = vec2(a * r * 24.0, r * 18.0) + seed;
            vec2 ci = floor(q), cf = fract(q);
            float d1 = 8.0, d2 = 8.0;
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++) {
                    vec2 gg = vec2(x, y);
                    vec2 rr = gg + 0.15 + 0.7 * hash2(ci + gg) - cf;
                    float d = dot(rr, rr);
                    if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
                }
            float edge = sqrt(d2) - sqrt(d1);
            float wv = 0.16 + 0.1 * (1.0 - r);
            float main = abs(fract(a * 2.2 + 0.5) - 0.5) / 2.2 * r * 8.5;
            float on = max(smoothstep(wv, wv * 0.4, edge), smoothstep(0.09, 0.04, main) * step(r, 0.92));
            on = max(on, smoothstep(0.955, 0.975, r));
            if (on < 0.5) discard;
            alb = mix(A, B, smoothstep(0.3, 1.0, r)) * (0.85 + 0.25 * smoothstep(wv, 0.0, edge));
            ao = mix(0.75, 1.0, r);
            glow = 0.6;
        }
    } else if (kind == 4) {
        // tube sponges: pitted walls, a lip catching the light, dark inside
        float pits = smoothstep(0.6, 0.7, vnoise3(vLocal * 46.0 + seed));
        alb = mix(A, B, smoothstep(0.75, 1.0, along) * 0.6) * (1.0 - 0.3 * pits);
        if (part > 0.5) { alb = A * 0.16; ao = 0.3; }
        ao *= mix(0.6, 1.0, smoothstep(0.0, 0.4, along));
        glow = smoothstep(0.85, 1.0, along) * 0.5 * (1.0 - part);
    } else if (kind == 5) {
        // anemone: a fleshy column, a crown of tentacles with bright tips
        if (part < 0.5) { alb = A * (0.75 + 0.1 * sin(atan(vLocal.z, vLocal.x) * 18.0)); ao = 0.75; }
        else {
            alb = mix(B, mix(A, vec3(1.0), 0.35), smoothstep(0.75, 1.0, along));
            ao = mix(0.65, 1.0, along);
            glow = smoothstep(0.6, 1.0, along);
        }
    } else if (kind == 7) {
        // a boulder of reef rock: mottled, pitted, crusting over (pink coralline, green turf) on its top
        vec3 lp = vLocal * 3.0 + seed;
        float r1 = vnoise3(lp * 1.3), r2 = vnoise3(lp * 5.0);
        alb = mix(uRockC, uRock2C, smoothstep(0.3, 0.75, r1)) * (0.82 + 0.3 * r2);
        float pit = smoothstep(0.7, 0.8, vnoise3(lp * 7.0 + 4.0));
        alb *= 1.0 - 0.35 * pit;
        float top = smoothstep(0.1, 0.8, n.y);
        float crust = smoothstep(1.0 - uCover * 0.7, 1.12 - uCover * 0.7, vnoise3(lp * 0.9 + 11.0) * 0.8 + 0.35 * top);
        float turf = smoothstep(0.6, 0.7, vnoise3(lp * 1.7 + 2.0) * 0.7 + 0.35 * top) * smoothstep(0.1, 0.5, uCover);
        alb = mix(alb, uCrustC * (0.75 + 0.35 * r2), crust * 0.85);
        alb = mix(alb, uCrust2C * (0.7 + 0.4 * r2), turf * (1.0 - 0.6 * crust));
        ao = mix(0.55, 1.0, smoothstep(0.0, 0.45, along));
    } else {
        // sea whips: rods bristling with polyps, paler toward their tips
        alb = mix(A, B, smoothstep(0.2, 1.0, along));
        alb *= 1.0 - 0.15 * smoothstep(0.6, 0.7, vnoise3(vLocal * 90.0 + seed));
        ao = mix(0.6, 1.0, along);
        glow = along;
    }
    if (kind == 3) n = dot(n, uSunDir) < 0.0 ? normalize(n - 2.0 * dot(n, uSunDir) * uSunDir * 0.7) : n; // (light comes through a fan)
    vec3 col = alb * toonLight(n, ao);
    float lit = toonBand(dot(n, uSunDir));
    col += uCausticC * caustic(vWorld) * alb * 1.6 * smoothstep(-0.2, 0.6, n.y) * (0.4 + 0.6 * lit);
    col += rimLight(n, vWorld) * mix(vec3(1.0), alb, 0.5);
    // fluorescence (a night variant), and a touch of glow in every one
    col += B * glow * (uGlowK * vD.w);
    col = waterFog(col, vWorld);
    frag = vec4(col, length(vWorld - uEye));
}
)";

// ---------------------------------------------------------------- fish and the manta
inline const char* FISH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;
layout(location = 3) in vec4 iA;    // position, size
layout(location = 4) in vec4 iB;    // direction, swim phase
layout(location = 5) in vec4 iC;    // species, swim amount, bank, alpha
uniform mat4 uVP;
out vec3 vWorld;
out vec3 vN;
out vec3 vLocal;
out vec4 vAttr;
flat out vec4 vC;
void main() {
    vec3 fwd = normalize(iB.xyz);
    vec3 side = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)) + vec3(1e-4, 0.0, 0.0));
    vec3 up = cross(side, fwd);
    float b = iC.z;
    vec3 up2 = up * cos(b) + side * sin(b), side2 = side * cos(b) - up * sin(b);
    vec3 p = aPos, n = aNrm;
    int sp = int(iC.x + 0.5);
    if (sp == 7) {
        // the manta: x the span, z the body (head +z); its wings beat slowly, a wave running out to the tips
        float ax = abs(p.x);
        float k = pow(ax, 1.5);
        float ph = iB.w - ax * 1.4;
        p.y += 0.24 * k * sin(ph) * (aAttr.z > 0.5 ? 0.0 : 1.0);
        float dy = 0.36 * pow(ax, 0.5) * sin(ph) * sign(p.x) - 0.24 * k * cos(ph) * 1.4 * sign(p.x);
        n = normalize(n + vec3(-dy, 0.0, 0.0) * (aAttr.z > 0.5 ? 0.0 : 1.0));
        if (aAttr.z > 0.5) p.x += 0.05 * sin(iB.w * 0.5 + p.z * 3.0) * (-p.z - 0.42);
        // local: x -> -side, y -> up, z -> forward (a right-handed frame: the faces keep their sides)
        vec3 wp = iA.xyz + (-side2 * p.x + up2 * p.y + fwd * p.z) * iA.w;
        vWorld = wp;
        vN = -side2 * n.x + up2 * n.y + fwd * n.z;
    } else {
        // a fish: x along the body (nose +x); it swims with a wave down its body, stronger at the tail
        float tailK = smoothstep(0.35, -0.75, p.x);
        float wave = sin(p.x * 7.0 - iB.w) * iC.y * tailK;
        p.z += wave * 0.16;
        n.x -= cos(p.x * 7.0 - iB.w) * iC.y * tailK * 0.5 * sign(n.z + 1e-4);
        vec3 wp = iA.xyz + (fwd * p.x + up2 * p.y + side2 * p.z) * iA.w;
        vWorld = wp;
        vN = fwd * n.x + up2 * n.y + side2 * n.z;
    }
    vLocal = aPos;
    vAttr = aAttr;
    vC = iC;
    gl_Position = uVP * vec4(vWorld - uEye, 1.0);
}
)";
inline const char* FISH_FS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in vec3 vLocal;
in vec4 vAttr;
flat in vec4 vC;
out vec4 frag;
uniform float uFishGlow;
vec3 C(float r, float g, float b) { return pow(vec3(r, g, b), vec3(2.2)); }
float band(float x, float a, float b, float w) { return smoothstep(a - w, a + w, x) * (1.0 - smoothstep(b - w, b + w, x)); }
void main() {
    int sp = int(vC.x + 0.5);
    vec3 n = normalize(vN);
    if (!gl_FrontFacing) n = -n;
    float part = vAttr.z;
    float x = vLocal.x, y = vLocal.y, ny = vAttr.y;
    vec3 alb;
    float belly = smoothstep(0.0, -0.8, ny) * (part < 0.5 ? 1.0 : 0.0);
    if (sp == 7) {
        // the manta: ink above with pale shoulder marks, white below
        bool top = gl_FrontFacing;
        alb = top ? C(0.12, 0.15, 0.22) : C(0.93, 0.95, 0.97);
        if (top) {
            float sh = exp(-sq((abs(x) - 0.3) / 0.13) - sq((vLocal.z - 0.12) / 0.12));
            alb = mix(alb, C(0.75, 0.8, 0.85), sh * 0.7);
        }
        if (part > 0.5) alb = C(0.1, 0.12, 0.16);
    } else if (sp == 0) { // chromis: blue-green, iridescent
        alb = mix(C(0.1, 0.75, 0.75), C(0.2, 0.45, 1.0), smoothstep(-0.5, 0.8, ny));
        alb = mix(alb, C(0.65, 0.95, 0.9), belly * 0.6);
        if (part > 0.5) alb = C(0.15, 0.6, 0.85);
    } else if (sp == 1) { // anthias: orange pink, lilac fins
        alb = mix(C(1.0, 0.45, 0.35), C(1.0, 0.62, 0.3), smoothstep(-0.6, 0.6, ny));
        alb = mix(alb, C(1.0, 0.85, 0.7), belly * 0.5);
        if (part > 0.5) alb = C(0.85, 0.45, 0.85);
    } else if (sp == 2) { // yellow tang
        alb = C(1.0, 0.86, 0.1);
        alb = mix(alb, C(1.0, 0.95, 0.6), belly * 0.3);
        if (part > 0.5) alb = C(1.0, 0.8, 0.05);
    } else if (sp == 3) { // blue tang: royal blue, a dark scribble, a yellow tail
        alb = C(0.1, 0.35, 1.0);
        float scrib = band(ny, 0.05, 0.75, 0.06) * band(x, -0.38, 0.28, 0.05) * smoothstep(0.1, 0.0, abs(ny - 0.45 - 0.35 * x) - 0.22);
        alb = mix(alb, C(0.04, 0.05, 0.15), scrib);
        alb = mix(alb, C(1.0, 0.85, 0.1), smoothstep(-0.36, -0.44, x) * (part < 0.5 ? 1.0 : 0.0));
        if (part > 0.5) alb = part < 1.5 ? C(1.0, 0.85, 0.1) : C(0.05, 0.08, 0.25);
    } else if (sp == 4) { // moorish idol: white and yellow under two black bands, a long white streamer
        alb = mix(C(1.0, 0.97, 0.88), C(1.0, 0.86, 0.25), smoothstep(-0.1, -0.35, x));
        float b1 = band(x, 0.05, 0.25, 0.015), b2 = band(x, -0.33, -0.2, 0.015);
        alb = mix(alb, C(0.06, 0.06, 0.08), max(b1, b2));
        if (part > 0.5) alb = part < 1.5 ? C(0.06, 0.06, 0.08) : C(1.0, 0.98, 0.92);
    } else if (sp == 5) { // parrotfish: teal with pink scale edges, a pink face
        float sc = smoothstep(0.35, 0.45, abs(fract(x * 14.0 + abs(ny) * 3.0) - 0.5));
        alb = mix(C(0.1, 0.8, 0.65), C(1.0, 0.5, 0.65), sc * 0.45);
        alb = mix(alb, C(1.0, 0.55, 0.7), smoothstep(0.3, 0.42, x) * 0.7);
        if (part > 0.5) alb = C(0.2, 0.55, 0.95);
    } else { // clownfish: orange, three white bands edged in black
        alb = C(1.0, 0.42, 0.05);
        float b1 = band(x, 0.12, 0.22, 0.01), b2 = band(x, -0.12, 0.0, 0.01), b3 = band(x, -0.46, -0.4, 0.01);
        float wb = max(max(b1, b2), b3);
        float edge = max(max(band(x, 0.1, 0.24, 0.01), band(x, -0.14, 0.02, 0.01)), band(x, -0.48, -0.38, 0.01)) - wb;
        alb = mix(alb, C(0.05, 0.03, 0.03), sat(edge) * (part < 0.5 ? 1.0 : 0.0));
        alb = mix(alb, C(1.0, 1.0, 1.0), wb * (part < 0.5 ? 1.0 : 0.0));
        if (part > 0.5) alb = mix(C(1.0, 0.45, 0.05), C(0.05, 0.03, 0.03), smoothstep(0.75, 0.95, abs(ny)));
    }
    // the tail's fork
    if (part > 0.5 && part < 1.5 && vAttr.w > 0.0) {
        float u = -vAttr.x;
        if (u > 1.0 - vAttr.w * 0.5 * (1.0 - abs(ny))) discard;
    }
    // an eye on each side
    if (sp != 7 && part < 0.5) {
        float ex = x - 0.36, ey = y - 0.045;
        float e = length(vec2(ex, ey * 1.1));
        alb = mix(alb, C(1.0, 1.0, 0.95), smoothstep(0.042, 0.032, e));
        alb = mix(alb, C(0.02, 0.02, 0.04), smoothstep(0.03, 0.022, e));
    }
    float ao = part > 0.5 ? 0.85 : 1.0;
    vec3 col = alb * toonLight(n, ao);
    col += uCausticC * caustic(vWorld) * alb * 0.8 * smoothstep(-0.3, 0.7, n.y);
    col += rimLight(n, vWorld) * 0.8;
    col += alb * uFishGlow * 0.25;
    col = waterFog(col, vWorld);
    frag = vec4(col, length(vWorld - uEye));
}
)";

// ---------------------------------------------------------------- bubbles, coral seeds, specks (billboards)
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA;   // position, size
layout(location = 2) in vec4 aB;   // color, alpha
layout(location = 3) in vec4 aC;   // mode, seed
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
out vec3 vWorld;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float dist = length(aA.xyz - uEye);
    float size = max(aA.w, dist * uMinPx);
    vec3 wp = aA.xyz + (uCamR * aPos.x + uCamU * aPos.y) * size;
    vLocal = aPos;
    vWorld = aA.xyz;
    vB = vec4(aB.rgb, aB.a * min(1.0, aA.w / max(size, 1e-5)));
    vC = aC;
    gl_Position = uVP * vec4(wp - uEye, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vWorld;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
void main() {
    float r = length(vLocal);
    if (r > 1.0) discard;
    int mode = int(vC.x + 0.5);
    float L = length(vWorld - uEye);
    float fogK = exp(-L / uFogD);
    vec3 c;
    float a;
    if (mode == 0) {
        // a bubble: a clear ball, its rim bright, a highlight up toward the sun
        float rim = smoothstep(0.62, 0.93, r) * smoothstep(1.0, 0.93, r);
        float hi = smoothstep(0.32, 0.12, length(vLocal - vec2(-0.32, 0.38)));
        float body = 0.12 * smoothstep(1.0, 0.0, r);
        a = (rim * 0.85 + hi + body) * vB.a;
        c = mix(vB.rgb, vec3(1.0), 0.35 + 0.65 * hi);
    } else if (mode == 1) {
        // a coral seed: a soft glowing bead
        float core = exp(-r * r * 12.0), halo = exp(-r * r * 3.5) * 0.45;
        a = min(1.0, core * 1.2 + halo) * vB.a;
        c = mix(vB.rgb * 1.8, vec3(1.6), core * 0.6);
    } else {
        // a speck drifting in the water
        a = exp(-r * r * 6.0) * vB.a;
        c = vB.rgb;
    }
    c = mix(waterColor(normalize(vWorld - uEye)), c, fogK);
    a *= 0.35 + 0.65 * fogK;
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- composite: the world + the shafts of sunlight
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform vec3 uRayC;
uniform vec2 uSunVP;
uniform float uRays, uSat;
uniform vec4 uWellUv;   // the well's rectangle (uv), where the world is blurred
uniform float uBlur;
void main() {
    vec4 w = texture(uWorld, vUv);
    vec3 c = w.rgb;
    {
        vec2 dw = max(max(uWellUv.xy - vUv, vUv - uWellUv.zw), 0.0);
        float k = (1.0 - smoothstep(0.0, 0.02, length(dw))) * uBlur;
        if (k > 0.0) {
            vec2 px = 1.0 / vec2(textureSize(uWorld, 0));
            vec3 acc = c;
            for (int i = 0; i < 12; i++) {
                float an = float(i) * 2.39996;
                float r = 2.5 + float(i) * 0.9;
                acc += texture(uWorld, vUv + vec2(cos(an), sin(an)) * r * px).rgb;
            }
            c = mix(c, acc / 13.0, k);
        }
    }
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    // the shafts: fanning down from the surface, from a point high above the view (toward the sun), drifting
    // slowly; they show over the water's depth in front of what they cross (none on what is near)
    vec2 uv = vUv;
    vec2 vp = uSunVP;
    vec2 dd = vec2((uv.x - vp.x) * uTan.x / uTan.y, uv.y - vp.y);
    float ang = atan(dd.x, -dd.y);
    float s1 = vnoise(vec2(ang * 17.0, uTime * 0.12));
    float s2 = vnoise(vec2(ang * 43.0 + 5.0, uTime * 0.2));
    float sh = smoothstep(0.45, 0.95, s1) * (0.55 + 0.45 * s2) + 0.25 * smoothstep(0.6, 1.0, s2);
    float depthK = 1.0 - exp(-min(w.a, 60.0) / 14.0);
    float fall = mix(0.35, 1.0, smoothstep(0.1, 0.95, uv.y));
    c += uRayC * sh * depthK * fall * uRays * 0.16;
    c = mix(c, uBridgeCol, uBridge * 0.0);
    // the floor darkens a little toward the bottom of the view (the HUD's corner reads on it)
    c *= mix(0.8, 1.0, smoothstep(0.0, 0.3, uv.y));
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSat), 0.0);
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the veil: the well and the HUD's columns
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil, uRail;
uniform float uWellA, uColA, uRailA, uPx;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h) { vec2 q = abs(p - c) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0)) - 0.12;
    float a = uWellA * (1.0 - smoothstep(-0.1, 0.8, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    // the well's own glass: a little lighter toward its top, where the light comes from
    vec3 veil = uVeil * (1.0 + 0.12 * smoothstep(-10.0, 10.0, w.y) * step(dw, 0.0));
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.1;
    rail *= smoothstep(10.4, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = veil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: one coral block per cell
// Each cell one clean rounded block of pale, sun-bleached coral: a porous calcified surface (polyp cups, a fine
// grain) shaded like a relief from the top left, a crisp deeper rim of its own hue. A first pass lays a soft
// contact shadow under the stack (a soft glow at night), the second the blocks. The ghost is a thin outline.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, -
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, -, -
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z * 1.36;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uTime, uBridge, uShimmer, uPass, uHaloA, uGlow;
uniform vec2 uHaloOff;
uniform vec3 uBridgeInk, uLightC, uHaloC;
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
// the calcified surface's height: polyp cups (the nearest of jittered centres) with raised lips, a fine grain
float coral(vec2 q) {
    vec2 i = floor(q), f = fract(q);
    float d1 = 8.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 r = g + 0.2 + 0.6 * hash2(i + g) - f;
            d1 = min(d1, dot(r, r));
        }
    float d = sqrt(d1);
    float cup = smoothstep(0.27, 0.08, d);
    float lip = smoothstep(0.16, 0.28, d) * smoothstep(0.48, 0.3, d);
    return 0.4 * lip - 0.8 * cup + 0.2 * vnoise(q * 2.7);
}
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    float shrink = 1.0 - 0.7 * smoothstep(0.0, 1.0, erase);
    vec3 t = vB.rgb;
    if (uPass < 0.5) {
        // the contact shadow (or the night's glow) under a block
        if (mode != 0) discard;
        vec2 q = (vLocal - uHaloOff) / shrink;
        float d = sdBox(q, vec2(0.45 - 0.13)) - 0.13;
        float a = (1.0 - smoothstep(-0.06, 0.2, d)) * uHaloA * vB.a * (1.0 - smoothstep(0.3, 1.0, erase)) * (1.0 - uBridge);
        vec3 c = mix(uHaloC, t * 0.8, uGlow);
        frag = vec4(c * a, a);
        return;
    }
    vec2 q = vLocal / shrink;
    float d = sdBox(q, vec2(0.45 - 0.13)) - 0.13;
    float inside = 1.0 - smoothstep(-aa, aa, d * shrink);
    if (inside <= 0.0) discard;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.03, 0.03 + aa * 1.5, abs(d + 0.04));
        float a = (ring * 0.85 + inside * 0.06) * vB.a;
        vec3 c = mix(t, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // the relief: polyp cups over the block (each cell its own), fading out when tiny on screen
    float lod = smoothstep(0.11, 0.035, aa);
    vec2 cq = q * 4.4 + vA.xy * 1.37;
    float e = 0.06;
    float h0 = coral(cq), hx = coral(cq + vec2(e, 0.0)), hy = coral(cq + vec2(0.0, e));
    vec3 n = vec3(-(hx - h0) / e * 0.22 * lod, -(hy - h0) / e * 0.22 * lod, 0.0);
    // the block's own rounded body, lit from the top left
    float edgeK = smoothstep(0.0, 0.16, -d);
    n = normalize(n + vec3(-q * (1.0 - edgeK) * 1.4, 1.0));
    vec3 L = normalize(vec3(-0.45, 0.6, 0.66));
    float diff = dot(n, L);
    vec3 c = t * (0.7 + 0.42 * diff);
    c *= mix(1.0, 0.8 + 0.2 * smoothstep(-0.7, 0.1, h0), lod);
    c = mix(c, mix(t, vec3(1.0), 0.45), smoothstep(0.92, 1.0, diff) * 0.35);
    // a faint ripple of light drifting over the stack
    float w = sin(vWorld.x * 1.3 + vWorld.y * 0.7 + uTime * 0.8) + sin(vWorld.x * 0.6 - vWorld.y * 1.1 - uTime * 0.6);
    c *= 1.0 + uShimmer * 0.04 * smoothstep(1.2, 2.0, w);
    c += t * uGlow * 0.12;
    // its crisp rim, a deeper tone of its own hue
    vec3 deep = t * t * 0.55;
    c = mix(c, deep, 1.0 - smoothstep(0.035, 0.035 + aa * 1.5, -d));
    c = mix(c, mix(t, vec3(1.0), 0.6), smoothstep(0.0, 0.5, erase) * 0.6);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * (1.0 - smoothstep(0.45, 1.0, erase));
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- board sprites: the gauge, tiny bubbles
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size, mode
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // param, param2
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
        // the gauge: a faint ring, the charge a bright arc from the top, clockwise
        float ring = exp(-sq((r - 0.8) / max(px * 1.2, 0.012)));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = exp(-sq((r - 0.8) / max(px * 2.5, 0.03))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.3 * (0.6 + 0.4 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, v * vB.a * 0.5);
        return;
    }
    // a tiny bubble
    float rim = smoothstep(0.55, 0.85, r) * smoothstep(1.0, 0.85, r);
    float hi = smoothstep(0.3, 0.1, length(vLocal - vec2(-0.3, 0.35)));
    float a = (rim * 0.8 + hi) * vB.a;
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- lettering
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

// ---------------------------------------------------------------- post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.25 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.8);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace coralreef_shaders

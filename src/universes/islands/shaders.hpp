#pragma once
// ISLANDS' shaders (see islands.cpp).
//
// The world is drawn relative to the eye (the camera at the origin, so every float stays small) and bent by the
// sea's curvature (each vertex dropped by curv * d^2 / 2, d its horizontal distance; the curvature grows with the
// altitude, so the horizon curves only from high up). Passes: the sun's shadow map; the seabed (the terrain under
// the water, at half resolution: its color and distance) for the water to see through; the reflection (the world
// above the water, mirrored, at half resolution); the main pass (sky, terrain, objects, then the sea as one
// fullscreen pass: the curved surface hit per pixel, its waves, the seabed seen through it with the absorption of
// its thickness, the sky and the islands reflected with fresnel, the sun's glint, foam on the shores and the reefs,
// the boats' wakes, the clouds' shadows), the clouds (soft lit puffs), additive glows; then the well and its sea
// glass pieces through the layout's camera, a filmic tone map, the HUD.
#include <string>

namespace islands_shaders {

// ---------------------------------------------------------------- common: noise, the sky, the haze, the shadows
inline const char* COMMON = R"(
uniform vec3 uSunDir, uSunCol, uSkyTop, uSkyHor, uSkyLow, uFogCol, uAmb, uBounce;
uniform vec3 uEye;          // the camera's position (world)
uniform float uCurv, uTime, uNight, uHaze, uHazeH, uDip;
uniform vec4 uClouds[24];   // the clouds' shadows: x z (where the shadow falls, world), radius, opacity
uniform int uNumClouds;
float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float n2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * n2(p); p = p * 2.03 + vec2(17.1, 3.7); a *= 0.5; }
    return s / 0.9375;
}
// The sky along a direction (no sun disc): its gradient by the hour (the horizon dips as the camera climbs).
vec3 skyCol(vec3 rd) {
    float e = rd.y + uDip;
    vec3 c = mix(uSkyHor, uSkyTop, pow(smoothstep(0.0, 0.45, e), 0.6));
    c = mix(c, uSkyLow, smoothstep(0.0, -0.08, e));
    float sd = max(dot(rd, uSunDir), 0.0);
    float low = 1.0 - smoothstep(0.02, 0.45, uSunDir.y);
    c += uSunCol * (pow(sd, 5.0) * (0.06 + 0.3 * low) + pow(sd, 48.0) * (0.25 + 0.4 * low)) * smoothstep(-0.2, 0.05, e);
    return c;
}
// Aerial perspective: an exponential haze thinning with height, warmer toward the sun.
float hazeK(float dist, vec3 rd) {
    float b = 1.0 / uHazeH;
    float dy = rd.y;
    float base = uHaze * exp(-max(uEye.y, 0.0) * b);
    float f = abs(dy) > 1e-4 ? base * (1.0 - exp(-dist * dy * b)) / (dy * b) : base * dist;
    return 1.0 - exp(-max(f, 0.0));
}
vec3 hazeCol(vec3 rd) {
    float sd = max(dot(rd, uSunDir), 0.0);
    return mix(uFogCol, uFogCol + uSunCol * 0.18, pow(sd, 6.0));
}
vec3 fogged(vec3 c, float dist, vec3 rd) { return mix(c, hazeCol(rd), hazeK(dist, rd)); }
float cloudShadow(vec2 p) {
    float s = 1.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uNumClouds) break;
        vec2 d = p - uClouds[i].xy;
        float r = uClouds[i].z;
        float k = 1.0 - smoothstep(r * 0.35, r, length(d) * (0.9 + 0.2 * n2(p * 0.004 + float(i))));
        s *= 1.0 - k * uClouds[i].w;
    }
    return s;
}
)";

inline const char* SHADOW_GLSL = R"(
uniform mat4 uLightVP;
uniform sampler2DShadow uShadow;
uniform float uSoft, uShadowOn;
float shadowAt(vec3 rel, float ndl) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 l = uLightVP * vec4(rel, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    float edge = smoothstep(0.0, 0.06, min(min(s.x, s.y), min(1.0 - s.x, 1.0 - s.y)));
    float a = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831;
    float sum = 0.0;
    float bias = 0.0004 + 0.0012 * (1.0 - ndl);
    for (int i = 0; i < 8; i++) {
        float fi = float(i) + 0.5;
        float rr = sqrt(fi / 8.0) * uSoft;
        float an = a + fi * 2.39996;
        sum += texture(uShadow, vec3(s.xy + vec2(cos(an), sin(an)) * rr, s.z - bias));
    }
    return mix(1.0, sum / 8.0, edge);
}
)";

// ---------------------------------------------------------------- the sky (fullscreen, behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uStars;
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize(w.xyz / w.w);
    vec3 c = skyCol(rd);
    float e = rd.y + uDip;
    // the sun's disc (soft edged), under the horizon it sinks into the haze
    float sd = dot(rd, uSunDir);
    float disc = smoothstep(0.99955, 0.99975, sd) * smoothstep(-0.01, 0.01, e);
    c += uSunCol * disc * 6.0;
    // high thin cirrus, a few streaks
    vec2 cp = rd.xz / max(rd.y + 0.12, 0.05) * 0.6;
    float ci = smoothstep(0.55, 0.9, fbm(cp * vec2(0.6, 2.2) + uTime * 0.002)) * smoothstep(0.02, 0.25, e);
    c = mix(c, c + vec3(0.18, 0.16, 0.14) * (0.5 + 0.5 * uSunCol), ci * 0.35 * (1.0 - uNight * 0.7));
    // stars at nightfall
    if (uStars > 0.01 && e > 0.0) {
        vec3 sp = rd * 260.0;
        vec3 si = floor(sp);
        float hs = fract(sin(dot(si, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        vec3 fp = fract(sp) - 0.5;
        float st = smoothstep(0.12, 0.0, length(fp)) * step(0.985, hs);
        c += vec3(0.9, 0.92, 1.0) * st * uStars * smoothstep(0.03, 0.3, e) * (0.6 + 0.4 * sin(uTime * 2.0 + hs * 40.0));
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- the terrain (one mesh per island)
inline const char* TERRAIN_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;
layout(location = 3) in float aAo;
uniform mat4 uVP;
uniform vec3 uOff;      // the island's center, relative to the eye
uniform float uCurv;
uniform float uMirror;  // 1: the reflection (mirrored about the sea)
out vec3 vRel, vLocal, vNrm;
out vec4 vAttr;
out float vAo;
void main() {
    vec3 rel = aPos + uOff;
    rel.y -= 0.5 * uCurv * dot(rel.xz, rel.xz);
    vLocal = aPos;
    vRel = rel;
    vNrm = aNrm;
    vAttr = aAttr;
    vAo = aAo;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

inline const char* TERRAIN_FS = R"(#version 330 core
in vec3 vRel, vLocal, vNrm;
in vec4 vAttr;
in float vAo;
layout(location = 0) out vec4 frag;
uniform int uMode;        // 0 main, 1 the seabed (under the water), 2 the reflection (above it)
uniform vec3 uSand, uSandWet, uGrass, uJungle, uRock, uCliff, uSeabed, uReefA, uReefB, uDry;
uniform float uSeed, uIsleR, uFallA, uFallW, uDryK;
uniform vec3 uFoam;
void main() {
    if (uMode == 3) { frag = vec4(0.0); return; }
    float h = vLocal.y;
    if (uMode == 1 && h > 0.15) discard;
    if (uMode == 2 && h < -0.1) discard;
    vec3 N = normalize(vNrm);
    vec2 lp = vLocal.xz + uSeed * 37.0;
    float dist = length(vRel);
    float fine = 1.0 - smoothstep(150.0, 1200.0, dist);
    vec3 col;
    float rough = 1.0;
    if (h < 0.0) {
        // the seabed: white sand with ripples, the reef's coral patches
        float rip = 0.5 + 0.5 * sin(dot(lp, vec2(0.9, 0.4)) + 2.0 * n2(lp * 0.15));
        col = uSeabed * (0.92 + 0.08 * rip * fine + 0.1 * (n2(lp * 0.05) - 0.5));
        float rf = vAttr.w;
        if (rf > 0.01) {
            float pn = n2(lp * 0.35), pm = n2(lp * 0.09 + 3.0);
            vec3 coral = mix(uReefA, uReefB, smoothstep(0.35, 0.65, pm));
            coral *= 0.7 + 0.5 * pn;
            col = mix(col, coral, smoothstep(0.1, 0.6, rf) * (0.55 + 0.45 * smoothstep(0.3, 0.6, pn)));
        }
        // seagrass and darker sand farther out
        col = mix(col, col * vec3(0.62, 0.72, 0.6), smoothstep(-6.0, -14.0, h) * 0.6 * smoothstep(0.4, 0.7, n2(lp * 0.02 + 7.0)));
    } else {
        float sandK = vAttr.x, vegK = vAttr.y, rockK = vAttr.z;
        vec3 sand = mix(uSandWet, uSand, smoothstep(0.15, 0.9, h)) * (0.94 + 0.08 * n2(lp * 0.7) * fine);
        // the canopy: lumpy crowns, darker gaps (a dense jungle seen from above), or a dry maquis
        float cn = n2(lp * 0.16), cn2 = n2(lp * 0.045 + 5.0);
        vec3 veg = mix(uGrass, uJungle, smoothstep(0.3, 0.75, cn2 + 0.25 * smoothstep(4.0, 40.0, h)));
        veg = mix(veg, uDry, uDryK * smoothstep(0.35, 0.7, n2(lp * 0.03 + 2.0)));
        veg *= 0.72 + 0.4 * cn * mix(0.5, 1.0, fine);
        // rock: strata along the height (warped), rain streaks down the faces, moss on the ledges
        float ar = atan(vLocal.z, vLocal.x) * uIsleR;
        float strata = fract(h * 0.11 + n2(lp * 0.012) * 1.6);
        float band = smoothstep(0.0, 0.06, strata) * smoothstep(0.62, 0.5, strata);
        float streak = n2(vec2(ar * 0.35, h * 0.015)) * 0.6 + n2(vec2(ar * 1.1, h * 0.04)) * 0.4;
        vec3 rock = mix(uRock, uCliff, smoothstep(0.4, 0.8, 1.0 - N.y));
        rock *= (0.7 + 0.22 * band + 0.25 * n2(lp * 0.25)) * (0.78 + 0.32 * streak);
        rock = mix(rock, uJungle * 0.9, smoothstep(0.45, 0.8, N.y) * smoothstep(0.35, 0.7, n2(lp * 0.08)) * 0.8);
        col = mix(sand, veg, smoothstep(0.25, 0.75, vegK) * (1.0 - smoothstep(0.6, 0.9, sandK) * 0.8));
        col = mix(col, rock, smoothstep(0.2, 0.7, rockK));
        rough = mix(1.0, 0.85, sandK);
        // a waterfall down the cliff: white water streaming, a little foam
        if (uFallW > 0.0) {
            float a = atan(vLocal.z, vLocal.x);
            float da = a - uFallA;
            da = mod(da + 3.14159265, 6.2831853) - 3.14159265;
            float x = da / uFallW;
            if (abs(x) < 1.3 && rockK > 0.25) {
                float streak = n2(vec2(x * 7.0, h * 0.18 + uTime * 2.2)) * 0.6 + n2(vec2(x * 15.0, h * 0.5 + uTime * 4.0)) * 0.4;
                float k = (1.0 - smoothstep(0.55, 1.25, abs(x))) * smoothstep(0.3, 0.6, rockK);
                col = mix(col, mix(uFoam * 0.75, uFoam * 1.1, streak), k * (0.75 + 0.25 * streak));
            }
        }
    }
    // the light: sun and soft shadow, the clouds' shadows, the sky ambient, the sea's bounce
    vec3 world = vRel + uEye;
    float ndl = max(dot(N, uSunDir), 0.0);
    float sh = uMode == 0 ? shadowAt(vRel, ndl) : 1.0;
    float cs = cloudShadow(world.xz);
    vec3 amb = uAmb * (0.6 + 0.4 * N.y) * vAo + uBounce * (0.5 - 0.5 * N.y);
    vec3 lit = col * (uSunCol * ndl * sh * cs * rough + amb);
    if (uMode == 1) {
        // caustics on the seabed, rippling with the waves
        vec2 cp = world.xz * 0.45;
        float c1 = n2(cp + vec2(uTime * 0.35, uTime * 0.2)), c2 = n2(cp * 1.3 - vec2(uTime * 0.25, -uTime * 0.3));
        float caus = pow(1.0 - abs(c1 - c2), 6.0);
        lit += col * uSunCol * caus * 0.8 * ndl * cs * smoothstep(-12.0, -0.5, h) * fine;
        frag = vec4(lit, dist);
        return;
    }
    vec3 rd = vRel / max(dist, 1e-3);
    frag = vec4(fogged(lit, dist, rd), uMode == 2 ? 1.0 : 0.0);
}
)";

// ---------------------------------------------------------------- the objects (instanced meshes)
// Instance: a = position, seed; b = scale, material; c = color, param; d = rotation (quaternion).
inline const char* OBJ_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aAux;
layout(location = 3) in vec4 iA;
layout(location = 4) in vec4 iB;
layout(location = 5) in vec4 iC;
layout(location = 6) in vec4 iD;
uniform mat4 uVP;
uniform vec3 uEye;
uniform float uCurv, uTime, uWind;
out vec3 vRel, vObj, vNrm;
out vec2 vAux;
flat out vec3 vCol, vSize;
flat out float vMat, vParam, vSeed;
flat out vec4 vQuat;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float hh(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
float nn(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hh(i), hh(i + vec3(1, 0, 0)), f.x), mix(hh(i + vec3(0, 1, 0)), hh(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hh(i + vec3(0, 0, 1)), hh(i + vec3(1, 0, 1)), f.x), mix(hh(i + vec3(0, 1, 1)), hh(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
    vec3 pos = aPos;
    vec3 nrm = aNrm;
    int mat = int(iB.w + 0.5);
    if (mat == 7 || mat == 12) {
        // foliage and rocks: a lumpy outline
        vec3 d = normalize(aPos);
        float k = nn(d * 2.4 + iA.w * 17.0) * 0.55 + nn(d * 5.5 + iA.w * 5.0) * 0.3 + nn(d * 11.0 + iA.w * 3.0) * 0.15;
        float amp = mat == 12 ? 0.35 : 0.5;
        pos = d * 0.5 * (1.0 - amp * 0.5 + amp * k);
        nrm = normalize(d + (vec3(nn(d * 6.0 + 1.0), nn(d * 6.0 + 2.0), nn(d * 6.0 + 3.0)) - 0.5) * 0.6);
    }
    if (mat == 8 && aAux.x > 0.5) {
        // the palm's fronds sway in the breeze, more toward their tips
        float t = aAux.y;
        float ph = uTime * (1.1 + 0.3 * fract(iA.w * 7.0)) + iA.w * 40.0 + pos.x * 0.3 + pos.z * 0.2;
        pos += vec3(sin(ph), sin(ph * 1.3 + 1.0) * 0.6, cos(ph * 0.9)) * (0.12 + 0.18 * uWind) * t * t;
    }
    if (mat == 16 && aAux.x > 0.5) {
        // a bird's wing beats (or glides: param 0)
        float fl = sin(uTime * 9.0 + iA.w * 30.0) * iC.a;
        pos.y += fl * aAux.y * 0.28;
        nrm = normalize(vec3(0.0, 1.0, -fl * sign(pos.z) * 0.6));
    }
    vec3 obj = pos * iB.xyz;
    vec3 w = iA.xyz + qrot(iD, obj);
    vec3 rel = w - uEye;
    rel.y -= 0.5 * uCurv * dot(rel.xz, rel.xz);
    vRel = rel;
    vObj = obj;
    vNrm = qrot(iD, normalize(nrm / max(iB.xyz, vec3(1e-4))));
    vAux = aAux;
    vCol = iC.rgb;
    vSize = iB.xyz;
    vMat = iB.w;
    vParam = iC.a;
    vSeed = iA.w;
    vQuat = iD;
    gl_Position = uVP * vec4(rel, 1.0);
}
)";

inline const char* OBJ_FS = R"(#version 330 core
in vec3 vRel, vObj, vNrm;
in vec2 vAux;
flat in vec3 vCol, vSize;
flat in float vMat, vParam, vSeed;
flat in vec4 vQuat;
layout(location = 0) out vec4 frag;
uniform int uMode;          // 0 main, 2 the reflection, 3 the shadow map (depth only)
uniform vec3 uLeaf, uLeaf2, uTrunk, uFoliage, uFoliage2, uWater, uWaterGlow, uRoomNight;
uniform float uLights;      // the lights at dusk (windows, the pool, the lighthouse)
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float hb(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float nb(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hb(i), hb(i + vec2(1, 0)), f.x), mix(hb(i + vec2(0, 1)), hb(i + vec2(1, 1)), f.x), f.y);
}
float h3(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.x + p.y) * p.z); }
float n3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h3(i), h3(i + vec3(1, 0, 0)), f.x), mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x), mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void main() {
    vec3 world = vRel + uEye;
    if (uMode == 2 && world.y < -0.05) discard;
    if (uMode == 3) { frag = vec4(0.0); return; }
    int mat = int(vMat + 0.5);
    vec3 N = normalize(vNrm);
    if (!gl_FrontFacing && (mat == 8 || mat == 9 || mat == 16)) N = -N;
    float dist = length(vRel);
    vec3 V = -vRel / max(dist, 1e-3);
    vec3 alb = vCol;
    float spec = 0.0, shin = 16.0, emit = 0.0, trans = 0.0;
    vec3 emitCol = vec3(0.0);
    vec3 face = abs(vObj) / max(vSize * 0.5, vec3(1e-4));
    if (mat == 0) {             // white render: a soft trowel cloud
        alb *= 0.93 + 0.07 * nb(vObj.xy * 1.3 + vObj.z * 0.7 + vSeed * 9.0);
    } else if (mat == 1) {      // glass: the sky in it, the room's light at dusk
        alb = vec3(0.05, 0.08, 0.1);
        spec = 1.0;
        shin = 120.0;
        float fr = pow(1.0 - max(dot(N, V), 0.0), 4.0);
        vec3 R = reflect(-V, N);
        alb += skyCol(R) * (0.25 + 0.6 * fr);
        float on = uLights * step(0.5, vParam);
        emitCol = uRoomNight * (0.7 + 0.3 * nb(vObj.xy * 0.7 + vSeed)) * on * (vParam > 1.5 ? 4.0 : 1.0);
        emit = 1.0;
    } else if (mat == 2) {      // deck: boards along its length, their seams
        vec2 ext = vSize.x > vSize.z ? vec2(vObj.x, vObj.z) : vec2(vObj.z, vObj.x);
        float b = ext.y / 0.16;
        float bi = floor(b), bf = fract(b);
        alb *= 0.86 + 0.16 * hb(vec2(bi, vSeed)) + 0.06 * nb(vec2(ext.x * 2.0, bi));
        alb *= 1.0 - 0.35 * smoothstep(0.1, 0.0, min(bf, 1.0 - bf)) * step(0.5, N.y);
    } else if (mat == 3) {      // weathered wood
        alb *= 0.85 + 0.2 * nb(vec2(vObj.y * 9.0, vObj.x * 0.6 + vSeed));
    } else if (mat == 4) {      // thatch: straw streaks down the slope
        float st = nb(vec2(atan(vObj.z, vObj.x) * 30.0, vObj.y * 1.5 + vSeed * 5.0));
        alb *= 0.75 + 0.4 * st;
        alb *= 0.85 + 0.15 * smoothstep(0.0, 0.3, fract(vObj.y * 2.2));
    } else if (mat == 5) {      // the pool: turquoise water, caustic light, a glow at night
        vec2 p = world.xz * 1.6;
        float c1 = nb(p + uTime * 0.6), c2 = nb(p * 1.3 - uTime * 0.5);
        float caus = pow(1.0 - abs(c1 - c2), 5.0);
        alb = uWater * (0.8 + 0.6 * caus);
        spec = 1.0;
        shin = 200.0;
        float fr = pow(1.0 - max(dot(N, V), 0.0), 5.0);
        alb = mix(alb, skyCol(reflect(-V, N)), fr * 0.6);
        emitCol = uWaterGlow * uLights * (0.8 + 0.4 * caus);
        emit = 1.0;
    } else if (mat == 7) {      // foliage: a clump of leaves (param: 0 jungle, 1 olive, 2 cypress, 3 hedge)
        // leaf clusters: lit tufts and dark gaps between them, their own little normals
        vec3 lp = vObj * (vParam > 1.5 && vParam < 2.5 ? 1.4 : 0.75) + vSeed * 17.0;
        float c1 = n3(lp), c2 = n3(lp * 2.7 + 5.0);
        float tuft = smoothstep(0.3, 0.75, c1 * 0.65 + c2 * 0.35);
        vec3 base = vParam < 0.5 ? mix(uFoliage, uFoliage2, fract(vSeed * 3.7)) : vParam > 2.5 ? uFoliage * 1.1 : vCol;
        alb = base * (0.45 + 0.8 * tuft);
        N = normalize(N + (vec3(n3(lp + 1.3), n3(lp + 7.1), n3(lp + 3.9)) - 0.5) * 1.1);
        alb = mix(alb, base * vec3(1.25, 1.3, 0.8), smoothstep(0.6, 1.0, tuft) * 0.35);
        trans = 0.35;
    } else if (mat == 8) {      // the palm: trunk rings, fronds green to yellow toward their tips
        if (vAux.x < 0.5) {
            float ring = smoothstep(0.3, 0.0, abs(fract(vAux.y * 38.0) - 0.5) - 0.2);
            alb = uTrunk * (0.8 + 0.25 * ring + 0.1 * nb(vObj.xy * 4.0));
        } else if (vAux.x < 1.5) {
            alb = mix(uLeaf, uLeaf2, smoothstep(0.4, 1.0, vAux.y) * 0.7 + 0.2 * fract(vSeed * 5.3));
            alb *= 0.85 + 0.25 * nb(vec2(vAux.y * 30.0, vSeed * 9.0));
            trans = 0.6;
        } else alb = vec3(0.32, 0.24, 0.1);
    } else if (mat == 9) {      // sail: cloth, the light through it
        alb *= 0.92 + 0.08 * smoothstep(0.02, 0.0, abs(fract(vAux.y * 7.0) - 0.5) - 0.47);
        trans = 0.7;
    } else if (mat == 10) {     // a hull: its paint, a stripe along the sheer, the deck
        if (vAux.x > 0.5) alb = vec3(0.62, 0.45, 0.28) * (0.85 + 0.15 * nb(vObj.xz * 3.0));
        else {
            float y = vObj.y / max(vSize.y, 1e-3);
            vec3 stripe = vParam > 2.5 ? vec3(0.6, 0.08, 0.08) : vParam > 1.5 ? vec3(0.04, 0.32, 0.4) : vec3(0.04, 0.08, 0.2);
            alb = mix(alb, stripe, smoothstep(-0.18, -0.15, y) * smoothstep(-0.05, -0.08, y) * step(0.5, vParam));
            spec = 0.6;
            shin = 60.0;
        }
    } else if (mat == 11) {     // canvas: an umbrella's stripes
        float a = atan(vObj.z, vObj.x);
        alb = mix(vCol, vec3(0.97, 0.95, 0.9), step(0.5, fract(a / 6.2831853 * 8.0)));
    } else if (mat == 12) {     // rock
        alb = vec3(0.5, 0.46, 0.4) * (0.75 + 0.4 * nb(vObj.xz * 0.8 + vObj.y + vSeed * 7.0));
    } else if (mat == 13) {     // a lamp
        emitCol = vCol * 4.0;
        emit = 1.0;
    } else if (mat == 14) {     // metal (masts, rails)
        spec = 0.8;
        shin = 40.0;
    } else if (mat == 15) {     // the lighthouse: white with red bands
        float y = vObj.y / vSize.y + 0.5;
        alb = mix(vec3(0.95), vec3(0.75, 0.1, 0.08), step(0.5, fract(y * 3.0 + 0.25)) * step(0.15, y));
    } else if (mat == 16) {     // a seabird: white, a grey back, dark tips
        alb = vParam < 0.5 && vCol.r < 0.3 ? vCol : mix(vec3(0.95), vec3(0.55, 0.58, 0.62), step(0.5, vAux.x) * smoothstep(0.2, 0.6, vAux.y));
        alb = mix(alb, vec3(0.08), step(0.5, vAux.x) * smoothstep(0.85, 0.95, vAux.y));
    } else if (mat == 17) {     // roof tiles: rows
        alb *= 0.82 + 0.2 * smoothstep(0.0, 0.25, fract(vObj.y * 4.0 / max(vSize.y, 0.1))) + 0.08 * nb(vObj.xz * 3.0);
    }
    float ndl = dot(N, uSunDir);
    float sh = uMode == 0 ? shadowAt(vRel + N * 0.05, max(ndl, 0.0)) : 1.0;
    float cs = cloudShadow(world.xz);
    float diff = max(ndl, 0.0) + trans * max(-ndl, 0.0) * 0.6;
    // occlusion near the ground (the instance's base) and inside the clumps
    float ao = mat == 7 ? 0.55 + 0.45 * smoothstep(-0.5, 0.4, vObj.y / max(vSize.y, 0.1)) : 1.0;
    ao *= 0.7 + 0.3 * smoothstep(0.0, 1.5, world.y);
    vec3 amb = uAmb * (0.65 + 0.35 * N.y) * ao + uBounce * (0.5 - 0.5 * N.y);
    vec3 col = alb * (uSunCol * diff * sh * cs + amb);
    if (spec > 0.0) {
        vec3 H = normalize(uSunDir + V);
        col += uSunCol * pow(max(dot(N, H), 0.0), shin) * spec * sh * cs * 0.6;
    }
    // backlit leaves glow
    if (trans > 0.0) col += alb * uSunCol * pow(max(dot(-V, uSunDir), 0.0), 4.0) * trans * 0.5 * sh;
    col += emitCol * emit;
    frag = vec4(fogged(col, dist, -V), uMode == 2 ? 1.0 : 0.0);
}
)";

// ---------------------------------------------------------------- the sea (fullscreen: the curved surface per pixel)
inline const char* WATER_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uVP;
uniform sampler2D uSeabed, uRefl;
uniform vec3 uShallow, uDeep, uSigma, uFoam;
uniform float uWave, uScatterK, uSparkle, uReflOn, uCalm;
uniform vec2 uInvRes;
uniform vec4 uBoats[24];     // x z, heading, speed
uniform vec4 uBoatsB[24];    // length, beam, kind, -
uniform int uNumBoats;
uniform vec4 uFalls[4];      // x z, radius, -
uniform int uNumFalls;
// the waves: long swells and a choppy ripple, all directional sines (each bent a little by a slower one), each
// faded out once a pixel covers it (no glitter aliasing); their slopes (x, z)
vec2 waves(vec2 p, float t, float foot) {
    vec2 g = vec2(0.0);
    const int N = 11;
    vec2 dirs[11] = vec2[11](vec2(0.94, 0.34), vec2(0.62, -0.78), vec2(-0.2, 0.98), vec2(0.82, 0.57), vec2(-0.71, 0.7),
                             vec2(0.99, -0.12), vec2(0.36, 0.93), vec2(-0.88, 0.47), vec2(0.55, -0.83), vec2(0.17, -0.98), vec2(-0.45, -0.89));
    float lens[11] = float[11](46.0, 27.0, 15.0, 9.1, 6.3, 4.1, 2.9, 2.1, 1.5, 1.05, 0.72);
    float amps[11] = float[11](0.06, 0.07, 0.08, 0.09, 0.1, 0.1, 0.1, 0.1, 0.09, 0.08, 0.07);
    for (int i = 0; i < N; i++) {
        float k = 6.2831 / lens[i];
        float fade = 1.0 - smoothstep(lens[i] * 0.12, lens[i] * 0.45, foot);
        if (fade <= 0.0) continue;
        float ph = dot(dirs[i], p) * k + t * sqrt(9.81 * k) * 0.8 + float(i) * 1.7 + 2.0 * sin(dot(dirs[(i + 3) % N], p) * k * 0.21 + float(i));
        g += dirs[i] * (amps[i] * cos(ph) * fade);
    }
    return g * uWave;
}
void main() {
    vec4 w = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rd = normalize(w.xyz / w.w);
    float alt = uEye.y;
    float dy = rd.y;
    if (dy >= -1e-5) discard;
    // the curved sea: alt + t dy + curv/2 (t h)^2 = 0, the near root
    float h2 = 1.0 - dy * dy;
    float a = 0.5 * uCurv * h2;
    float disc = dy * dy - 4.0 * a * alt;
    if (disc < 0.0) discard;
    float t = 2.0 * alt / (-dy + sqrt(disc));
    vec3 P = rd * t;
    vec4 clip = uVP * vec4(P, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    vec2 wp = P.xz + uEye.xz;
    // a pixel's footprint on the sea (stretched at grazing angles): the waves smaller than it fade out
    float foot = t * 0.9 * uInvRes.y / max(-dy, 0.04);
    float near = 1.0 - smoothstep(60.0 + alt * 2.0, 600.0 + alt * 6.0, t);
    float mid = 1.0 - smoothstep(800.0 + alt * 4.0, 6000.0 + alt * 8.0, t);
    vec2 g = waves(wp, uTime, foot) * (1.0 - 0.75 * uCalm);
    // the unresolved waves roughen the surface instead: the glint spreads and dims
    float rough = clamp(foot / 3.0, 0.0, 1.0);
    // the boats' wakes: a turbulent white trail, the Kelvin V spreading behind, the bow's spray
    float foamW = 0.0;
    vec2 gw = vec2(0.0);
    for (int i = 0; i < 24; i++) {
        if (i >= uNumBoats) break;
        vec2 bp = uBoats[i].xy;
        vec2 d = wp - bp;
        float L = uBoatsB[i].x;
        float sp = uBoats[i].w;
        float wl = 10.0 + sp * 10.0;
        if (dot(d, d) > (wl + L) * (wl + L)) continue;
        vec2 fw = vec2(cos(uBoats[i].z), sin(uBoats[i].z));
        vec2 sd = vec2(-fw.y, fw.x);
        float along = -dot(d, fw) - L * 0.35;  // behind the stern: > 0
        float across = dot(d, sd);
        float B = uBoatsB[i].y;
        float sk = clamp(sp / 12.0, 0.0, 1.0);
        if (along > -L) {
            float al = max(along, 0.0);
            float fade = exp(-al / (wl * 0.35));
            float wdt = B * 0.5 + al * 0.06;
            float trail = exp(-across * across / (wdt * wdt)) * fade * smoothstep(-L * 0.6, 0.0, along);
            float arm = abs(across) - al * 0.36 - B * 0.4;
            float v = exp(-arm * arm / (0.6 + al * 0.05)) * fade * smoothstep(-2.0, 2.0, along);
            float bow = exp(-pow(abs(across) - B * 0.55, 2.0) / 0.25) * smoothstep(-L, -L * 0.7, along) * smoothstep(0.0, -L * 0.3, along);
            float n = n2(wp * 1.4 + uTime * 0.8);
            foamW = max(foamW, (trail * (0.55 + 0.45 * n) * (0.4 + 0.6 * sk) + v * 0.5 * sk * smoothstep(0.3, 0.7, n) + bow * 0.6 * (0.3 + 0.7 * sk)));
            gw += sd * sign(across) * v * 0.15 * sk;
        }
    }
    g += gw;
    vec3 Nw = normalize(vec3(-g.x, 1.0, -g.y));
    // the seabed seen through the water, bent a little by the waves
    vec2 uv = gl_FragCoord.xy * uInvRes;
    vec2 uvR = uv + Nw.xz * 0.025 * near;
    vec4 sb = texture(uSeabed, uvR);
    if (sb.a > 0.0 && sb.a < t) sb = texture(uSeabed, uv);
    float thick = sb.a > t ? sb.a - t : 1e4;
    float depthV = thick * (-dy);
    vec3 T = exp(-uSigma * thick);
    float light = dot(uSunCol, vec3(0.3, 0.5, 0.2)) * 0.5 + dot(uAmb, vec3(0.3, 0.5, 0.2)) * 0.8;
    vec3 scatter = mix(uShallow, uDeep, 1.0 - exp(-thick / uScatterK)) * light;
    vec3 refr = (sb.a > t ? sb.rgb : vec3(0.0)) * T + scatter * (1.0 - T);
    // the sky reflected, and the islands (the mirrored pass)
    vec3 R = reflect(rd, Nw);
    R.y = abs(R.y);
    vec3 refl = skyCol(R);
    if (uReflOn > 0.5) {
        vec4 rf = texture(uRefl, uv + Nw.xz * vec2(0.05, 0.08) * (0.4 + 0.6 * near));
        refl = mix(refl, rf.rgb, rf.a);
    }
    float F = 0.02 + 0.98 * pow(1.0 - max(dot(Nw, -rd), 0.0), 5.0);
    F = mix(F, 1.0, smoothstep(0.0, -0.002, dy + uDip * 0.6) * 0.0);
    float cs = cloudShadow(wp);
    vec3 col = mix(refr, refl, F);
    // the sun's glint: a tight core and a broad sheen, sparkles at a peak
    float sg = max(dot(R, uSunDir), 0.0);
    float shin = mix(1400.0, 60.0, rough);
    float hiK = 1.0 - 0.6 * smoothstep(40.0, 600.0, alt);
    col += uSunCol * (pow(sg, shin) * mix(7.0, 0.5, rough) * hiK + pow(sg, mix(120.0, 18.0, rough)) * 0.3) * cs;
    // at a peak, the glints sparkle with the beat (small, near, only where the sun glints)
    float spk = smoothstep(0.82, 0.97, n2(wp * 0.7 + floor(uTime * 6.0) * 3.1)) * pow(sg, 24.0) * near * (1.0 - rough);
    col += uSunCol * spk * 2.5 * uSparkle * cs;
    // foam: the shore's wash and the surf breaking on the reefs, swirling, pushed toward the beach
    float fn = n2(wp * 0.55 + vec2(uTime * 0.15, 0.0)) * 0.6 + n2(wp * 1.9 - uTime * 0.3) * 0.4;
    float shore = 1.0 - smoothstep(0.02, 0.12 + 0.12 * fn, depthV);
    float bands = smoothstep(0.7, 0.97, sin(depthV * 14.0 - uTime * 1.6 + fn * 4.0) * 0.5 + 0.5) * (1.0 - smoothstep(0.1, 0.45, depthV)) * smoothstep(0.01, 0.06, depthV);
    float surf = smoothstep(0.62, 0.9, fn) * (1.0 - smoothstep(0.35, 0.9, depthV)) * smoothstep(0.2, 0.4, depthV) * 0.35;
    float foam = max(max(shore * (0.6 + 0.4 * fn), bands * 0.5), surf) * mid;
    // the waterfalls' white water at their feet
    for (int i = 0; i < 4; i++) {
        if (i >= uNumFalls) break;
        float d = length(wp - uFalls[i].xy) / uFalls[i].z;
        foam = max(foam, (1.0 - smoothstep(0.3, 1.0, d)) * (0.6 + 0.4 * n2(wp * 0.8 + uTime)));
    }
    foam = max(foam, foamW);
    vec3 foamLit = uFoam * (uSunCol * (0.55 * max(uSunDir.y, 0.1) + 0.3) * cs + uAmb * 0.9);
    col = mix(col, foamLit, clamp(foam, 0.0, 1.0) * 0.92);
    frag = vec4(fogged(col, t, rd), 0.0);
}
)";

// ---------------------------------------------------------------- clouds: soft lit puffs (instanced sprites)
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz (relative to the eye), radius
layout(location = 2) in vec4 iP;     // seed, alpha, flat bottom (y, relative), shade
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec3 vRel;
flat out vec4 vP;
flat out float vR, vCy;
void main() {
    vQ = aCorner;
    vP = iP;
    vR = iPos.w;
    vCy = iPos.y;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    vRel = w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* CLOUD_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vRel;
flat in vec4 vP;
flat in float vR, vCy;
out vec4 frag;
uniform vec3 uRight, uUp, uFwd;
uniform vec3 uCloudLit, uCloudShade;
void main() {
    float r = length(vQ);
    float edge = 0.62 + 0.3 * n2(vQ * 2.6 + vP.x * 13.0) + 0.08 * n2(vQ * 7.0 + vP.x * 3.0);
    float a = 1.0 - smoothstep(edge - 0.28, edge, r);
    if (a < 0.003) discard;
    // a sphere's normal from the sprite, toward the camera
    float z = sqrt(max(1.0 - min(r, 1.0), 0.0));
    vec3 N = normalize(uRight * vQ.x + uUp * vQ.y - uFwd * z * 1.2);
    float ndl = dot(N, uSunDir) * 0.5 + 0.5;
    // the flat, shaded base
    float wy = vRel.y + uEye.y;
    float base = smoothstep(vP.z - vR * 0.15, vP.z + vR * 0.55, wy);
    vec3 col = mix(uCloudShade, uCloudLit, pow(ndl, 1.3) * (0.45 + 0.55 * base));
    col *= mix(0.8, 1.0, vP.w);
    // the silver lining against the sun
    vec3 rd = normalize(vRel);
    float back = pow(max(dot(rd, uSunDir), 0.0), 6.0);
    col += uSunCol * back * (1.0 - smoothstep(0.3, 1.0, a)) * 0.9;
    col += uSunCol * 0.05 * ndl;
    float dist = length(vRel);
    col = mix(col, hazeCol(rd), hazeK(dist, rd) * 0.9);
    a *= vP.y;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- glows: additive sprites (lights at dusk, the beam)
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iPos;   // xyz (relative), size
layout(location = 2) in vec4 iCol;   // rgb, softness
uniform mat4 uVP;
uniform vec3 uRight, uUp;
out vec2 vQ;
out vec4 vCol;
void main() {
    vQ = aCorner;
    vCol = iCol;
    vec3 w = iPos.xyz + (uRight * aCorner.x + uUp * aCorner.y) * iPos.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vQ;
in vec4 vCol;
out vec4 frag;
void main() {
    float r = length(vQ);
    float core = smoothstep(0.25, 0.0, r);
    float halo = exp(-r * r * 6.0) * (1.0 - smoothstep(0.75, 1.0, r));
    float a = core * (1.0 - vCol.a) + halo * (0.3 + vCol.a);
    frag = vec4(vCol.rgb * a, 0.0);
}
)";

// ---------------------------------------------------------------- the well: flat quads (backing, shadows, the ghost)
inline const char* FLAT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
layout(location = 2) in vec4 aQ;     // local coords (-1..1), softness x, y
uniform mat4 uVP;
out vec4 vCol;
out vec4 vQ;
void main() {
    vCol = aCol;
    vQ = aQ;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* FLAT_FS = R"(#version 330 core
in vec4 vCol;
in vec4 vQ;
out vec4 frag;
void main() {
    vec2 q = abs(vQ.xy);
    float a = 1.0;
    if (vQ.z > 0.0) a *= smoothstep(1.0, 1.0 - vQ.z, q.x);
    if (vQ.w > 0.0) a *= smoothstep(1.0, 1.0 - vQ.w, q.y);
    frag = vec4(vCol.rgb, vCol.a * a);
}
)";

// ---------------------------------------------------------------- the pieces: sea glass, one tumbled tile per cell
// Instance: a = center xyz, size (board units); b = color rgb, seed; c = dull, glow, alpha, -.
inline const char* GLASS_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
out vec2 vP;
flat out vec3 vCol;
flat out float vSeed;
flat out vec4 vE;
void main() {
    vP = aCorner * 0.5;
    vCol = iB.rgb;
    vSeed = iB.a;
    vE = iC;
    gl_Position = uVP * vec4(iA.xy + aCorner * 0.5 * iA.w, iA.z, 1.0);
}
)";
inline const char* GLASS_FS = R"(#version 330 core
in vec2 vP;
flat in vec3 vCol;
flat in float vSeed;
flat in vec4 vE;
out vec4 frag;
uniform vec3 uKeyDir, uKeyCol, uAmbTop, uAmbBot;
float hb(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float nb(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hb(i), hb(i + vec2(1, 0)), f.x), mix(hb(i + vec2(0, 1)), hb(i + vec2(1, 1)), f.x), f.y);
}
void main() {
    vec2 p = vP;
    float s1 = fract(vSeed * 7.13), s2 = fract(vSeed * 3.71);
    // a piece of sea glass tumbled by the waves: a rounded square, soft worn corners, each its own (one clean
    // silhouette)
    const float HS = 0.455;
    vec4 rads = 0.15 + 0.1 * vec4(fract(vSeed * 11.3), fract(vSeed * 17.9), fract(vSeed * 23.1), fract(vSeed * 29.7));
    float RAD = p.x > 0.0 ? (p.y > 0.0 ? rads.x : rads.y) : (p.y > 0.0 ? rads.z : rads.w);
    vec2 q = abs(p) - (HS - RAD);
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - RAD;
    float aa = max(fwidth(d), 1e-4) * 0.8;
    float cov = 1.0 - smoothstep(-aa, aa, d);
    if (cov <= 0.001) discard;
    vec2 gq = (q.x > 0.0 || q.y > 0.0) ? normalize(max(q, 0.0) + 1e-6) * sign(p) : (q.x > q.y ? vec2(sign(p.x), 0.0) : vec2(0.0, sign(p.y)));
    // its thickness: a low dome, worn round at the shoulders
    float inner = clamp(-d / 0.24, 0.0, 1.0);
    float th = sqrt(inner * (2.0 - inner));
    vec2 wob = (vec2(nb(p * 4.0 + s1 * 9.0), nb(p * 4.0 + s2 * 7.0)) - 0.5) * 0.1;
    vec3 n = normalize(vec3(gq * (1.0 - th) * 1.3 + wob, 0.5 + th));
    vec3 L = normalize(uKeyDir);
    float diff = max(dot(n, L), 0.0);
    vec3 amb = mix(uAmbBot, uAmbTop, n.y * 0.5 + 0.5);
    // the glass on the dark backing: a luminous core (the light scattered in the frosted glass), a deeper,
    // saturated band toward the thin worn edge (where the dark shows through), the light leaving it glowing
    // on the far side
    vec3 deep = pow(vCol, vec3(1.6));
    vec3 core = vCol * (amb * 0.7 + uKeyCol * (0.6 + 0.32 * diff));
    float band = pow(1.0 - th, 1.3);
    vec3 col = mix(core, deep * (0.5 + 0.4 * diff) * uKeyCol, band * 0.85);
    vec2 ex = p - vec2(0.11, -0.12);
    float through = exp(-dot(ex, ex) / 0.03) * th;
    col += mix(vCol, vec3(1.0), 0.3) * through * 0.45 * uKeyCol;
    // frosted by the sea: a milky veil in soft clouds, a fine pitted grain
    float grain = nb(p * 42.0 + s1 * 50.0) * 0.55 + nb(p * 90.0 + s2 * 30.0) * 0.45;
    float cloud = nb(p * 2.4 + vec2(s1, s2) * 20.0);
    float frost = (0.08 + 0.1 * cloud) * th + (grain - 0.5) * 0.1;
    col = mix(col, mix(vCol, vec3(1.0), 0.5) * uKeyCol * (0.8 + 0.3 * diff), clamp(frost, 0.0, 1.0));
    float pits = smoothstep(0.8, 0.93, nb(p * 18.0 + vec2(s2, s1) * 40.0));
    col *= 1.0 - 0.1 * pits * th;
    // the worn lip catching the light on the lit side (a frosted line along the edge)
    float lip = smoothstep(-0.07, -0.02, d) * (1.0 - smoothstep(-0.02, -0.004, d));
    col += mix(vCol, vec3(1.0), 0.6) * lip * max(dot(gq, normalize(L.xy)), 0.0) * 0.4 * uKeyCol;
    // an old bubble or two caught in the glass
    for (int i = 0; i < 2; i++) {
        float hs = fract(vSeed * (13.7 + float(i) * 5.3));
        if (hs > 0.55) continue;
        vec2 bc = vec2(fract(hs * 31.7), fract(hs * 57.3)) * 0.44 - 0.22;
        float br = 0.014 + 0.016 * fract(hs * 91.1);
        float bd = length(p - bc) / br;
        float ring = smoothstep(1.2, 0.95, bd) * smoothstep(0.5, 0.85, bd);
        col = mix(col, mix(vCol, vec3(1.0), 0.75) * uKeyCol, ring * 0.55 * smoothstep(0.0, -0.4, dot(p - bc, normalize(L.xy)) / br));
        col *= 1.0 - 0.18 * smoothstep(0.9, 0.3, bd);
    }
    // a broad satin sheen on the lit shoulder (never a sharp glint)
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    col += uKeyCol * pow(max(dot(n, H), 0.0), 9.0) * 0.08;
    // the outline: crisp, in a deeper tone of its own color
    col = mix(col, deep * 0.4, smoothstep(-0.012, 0.0, d) * 0.8);
    // states: dulled (game over), glowing (a cleared row), held (dim)
    float g = dot(col, vec3(0.3, 0.55, 0.15));
    col = mix(col, vec3(g * 0.55 + 0.08), vE.x);
    col = mix(col, mix(vCol, vec3(1.0), 0.6) * 1.6, vE.y);
    frag = vec4(col, cov * vE.z);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uExposure, uSlow, uSat;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
vec3 aces(vec3 x) {
    x = max(x, 0.0);
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main() {
    vec4 s = texture(uScene, vUv);
    float pk = clamp(s.a, 0.0, 1.0);
    // the world through a filmic curve, the pieces as they are (their colors authored)
    vec3 c = mix(aces(s.rgb * uExposure), clamp(s.rgb, 0.0, 1.0), pk);
    // the world's saturation (a vivid tropical grade), the pieces kept as they are
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(c, mix(vec3(g), c, uSat), 1.0 - pk);
    c = mix(c, mix(vec3(g), c * uSlowTint, 0.65), uSlow * (1.0 - pk));
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.16 * smoothstep(0.45, 0.95, length(q * vec2(1.0, 0.85))) * (1.0 - pk);
    c = mix(c, uBridgeCol, uBridge * (1.0 - pk));
    c = mix(c, uBridgeInk, uBridge * pk);
    c *= (1.0 - 0.5 * uDim) * uFade;
    frag = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
)";

// ---------------------------------------------------------------- HUD
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
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.12, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// A shader with the given blocks inserted after its version line.
inline std::string with(const char* fs, const char* a, const char* b = "", const char* c = "") {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + a + b + c + s.substr(nl + 1);
}

} // namespace islands_shaders

#pragma once
// REWILD's shaders (see rewild.cpp): the 3D world (the sky, the land and its cut face, the plants, the birds, the
// particles, the rain), the well's pane, the stack's glass cubes of seeds, the bonus gauge, the HUD letters and the
// final pass.

namespace rewild_shaders {

// Shared helpers, inserted after each shader's #version line.
inline const char* COMMON_GLSL = R"(
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 hash33(vec3 p3) {
    p3 = fract(p3 * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yxx) * p3.zyx);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x), mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}
vec3 enc(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 17.1; a *= 0.5; }
    return s;
}
)";

// The world: its light, its sky, the land, the haze (rewild.cpp's groundH is the same function). World units are board
// units; the board plane is z = 0, the diorama is cut at z = uFrontZ behind it.
inline const char* WORLD_GLSL = R"(
uniform vec3 uEye;
uniform float uFrontZ;
uniform float uTime, uSlow, uNight, uFlash, uCloudT, uStars, uClouds, uLife, uRain, uWet;
uniform vec3 uSunDir, uSunCol, uAmb;   // toward the sun (or moon); its light; the sky's light
uniform vec4 uSunDisc;                 // angular radius, -, moon (0 / 1), how much of it shows (the rain hides it)
uniform vec3 uSkyTop, uSkyLow, uDust, uMist, uDusk, uStormSky, uStormCloud, uCloud;
uniform vec3 uFlashDir;

// the land: a shallow plain behind the well, gently rolling, rising far out into low hills
float groundH(vec2 p) {
    float dz = -p.y;
    float d = 0.35 * sin(p.x * 0.23 + 1.3 + p.y * 0.13) + 0.22 * sin(p.x * 0.61 + 0.4 - p.y * 0.29) + 0.12 * sin(p.x * 1.3 + 2.0 + p.y * 0.7);
    float hill = smoothstep(30.0, 170.0, dz) * (5.0 + 4.0 * sin(p.x * 0.035 + 0.8) + 3.0 * sin(p.x * 0.071 - p.y * 0.02 + 2.1) +
                                              2.0 * sin(p.x * 0.13 + p.y * 0.05));
    return -9.4 + 1.2 * smoothstep(5.0, 60.0, dz) + d * (1.0 + 0.8 * smoothstep(10.0, 80.0, dz)) + hill;
}

// ---- the sky: bleached and dusty over a parched land, clearer and more saturated as it comes alive, grey in the rain
vec3 skyBase(vec3 d) {
    float h = clamp(d.y / 0.3, 0.0, 1.0);
    vec3 sky = mix(uSkyLow, uSkyTop, smoothstep(0.0, 1.0, pow(h, 0.7)));
    float dry = 1.0 - smoothstep(0.0, 0.7, uLife);
    float l = dot(sky, vec3(0.3, 0.5, 0.2));
    sky = mix(sky, mix(vec3(l), uDust, 0.5) * 1.05, dry * 0.4);
    return mix(sky, mix(uStormSky * 1.15, uStormSky * 0.7, h), uRain * 0.8);
}
vec3 withSun(vec3 sky, vec3 d) {
    float ang = acos(clamp(dot(d, uSunDir), -1.0, 1.0));
    float R = uSunDisc.x;
    float disc = smoothstep(R * 1.06, R * 0.94, ang);
    float halo = exp(-max(ang - R, 0.0) / (R * (2.2 + 0.6 * (1.0 - uSunDisc.z)))) * (0.26 + 0.1 * (1.0 - uSunDisc.z));
    sky = mix(sky, uSunCol, halo * uSunDisc.w);
    return mix(sky, uSunCol, disc * uSunDisc.w);
}
vec3 skyFull(vec3 d, bool stars) {
    vec3 sky = skyBase(d);
    float st = uStars * (1.0 - uRain);
    if (stars && st > 0.01) {
        vec2 g = vec2(atan(d.x, -d.z), d.y) * 150.0;
        vec2 c = floor(g);
        if (hash12(c + 7.0) > 0.985) {
            vec2 q = fract(g) - 0.5 - (vec2(hash12(c + 1.3), hash12(c + 2.9)) - 0.5) * 0.6;
            sky += vec3(0.75, 0.8, 0.95) * smoothstep(0.09, 0.0, length(q)) * st * smoothstep(0.0, 0.05, d.y) * 0.5 *
                   (0.4 + 0.6 * hash12(c + 5.1));
        }
    }
    sky = withSun(sky, d);
    if (d.y > 0.002) {
        vec2 q = d.xz / d.y * 40.0;
        vec2 cp = vec2(q.x * 0.016 - uCloudT * 2.0, q.y * 0.045);
        // a few high wisps over the dry land, more clouds over the living one, an overcast in the rain
        float cov = uClouds * (0.5 + 0.7 * uLife) + 0.7 * uRain;
        float n = fbm(cp + vec2(0.0, 3.0)) + 0.25 * fbm(cp * 2.3 + 9.0);
        float band = smoothstep(0.03, 0.16, d.y);
        float cl = smoothstep(1.05 - cov * 0.55, 1.25 - cov * 0.4, n) * band;
        vec3 cc = mix(uCloud, uStormCloud * (0.8 + 0.4 * smoothstep(0.8, 1.4, n)), uRain);
        float fa = acos(clamp(dot(d, uFlashDir), -1.0, 1.0));
        cc += vec3(0.85, 0.9, 1.0) * uFlash * exp(-fa * 8.0) * 1.2;
        sky = mix(sky, cc, cl * mix(0.5, 0.88, uRain));
        sky = mix(sky, uStormCloud * 0.95, uRain * 0.5 * (1.0 - band));
        sky += vec3(0.6, 0.65, 0.8) * uFlash * 0.08 * exp(-fa * 3.0) * band;
    }
    return sky;
}
vec3 hazeCol(vec3 d);
// the sky as seen: melting into the land's haze at the horizon (no seam, rain or shine)
vec3 skySeen(vec3 d, bool stars) {
    return mix(skyFull(d, stars), hazeCol(d), (1.0 - smoothstep(0.0, 0.07, d.y)) * 0.85);
}
// slow motion: a blue dusk
vec3 grade(vec3 c) { return mix(c, uDusk * (0.45 + 1.5 * dot(c, vec3(0.3, 0.5, 0.2))), uSlow * 0.72); }
// the haze far out: a dusty haze over the dry land, a humid mist settling in the hollows of the living one
vec3 hazeCol(vec3 d) {
    vec3 h = mix(skyBase(normalize(vec3(d.x, 0.0, d.z))), mix(uDust, uMist, smoothstep(0.1, 0.8, uLife)), 0.45);
    return mix(h, uStormCloud * 1.05, uRain * 0.5);
}
vec3 aerial(vec3 c, vec3 P) {
    vec3 d = P - uEye;
    float L = length(d);
    float dens = 0.0042 + 0.003 * (1.0 - uLife) + 0.004 * uRain;
    float k = 1.0 - exp(-max(L - 28.0, 0.0) * dens);
    // the mist: low over the land, far out, in a living land and in the rain
    float hf = exp(-max(P.y + 9.0, 0.0) / 3.5);
    float mist = (0.55 * smoothstep(0.35, 1.0, uLife) + 0.5 * uRain) * hf * (1.0 - exp(-max(L - 20.0, 0.0) * 0.02));
    return mix(c, hazeCol(d), clamp(k * 0.94 + mist * 0.7, 0.0, 0.96));
}

// ---- the life map: grass (r), damp dark soil (g), flowers (b) around what grows
uniform sampler2D uLifeMap;
uniform vec3 uLifeArea;   // x0, z0, size
vec3 lifeAt(vec2 xz) {
    vec2 uv = vec2((xz.x - uLifeArea.x) / uLifeArea.z, (uLifeArea.y - xz.y) / uLifeArea.z);
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) return vec3(0.0);
    vec3 m = texture(uLifeMap, uv).rgb;
    vec2 e = min(uv, 1.0 - uv);
    return m * smoothstep(0.0, 0.04, min(e.x, e.y));
}
// the green spreading over the whole land on its own as it comes alive (in patches, the far hills first covered by
// forest)
float coverAt(vec2 xz) {
    float n = fbm(xz * 0.045 + 3.0) * 0.75 + vnoise(xz * 0.3) * 0.25;
    return smoothstep(1.02 - 0.75 * uLife, 1.22 - 0.75 * uLife, n + 0.15 * smoothstep(20.0, 120.0, -xz.y));
}
)";

// The sun's shadow map (the land and the plants receive it).
inline const char* SHADOW_GLSL = R"(
uniform sampler2DShadow uShadow;
uniform mat4 uLightVP;
uniform float uShadowK;
float shadowAt(vec3 P) {
    vec4 l = uLightVP * vec4(P, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    vec2 tx = 1.0 / vec2(textureSize(uShadow, 0));
    float acc = 0.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) acc += texture(uShadow, vec3(s.xy + vec2(x, y) * tx * 1.5, s.z - 0.0015));
    return mix(1.0, acc / 9.0, uShadowK);
}
)";

// ---------------------------------------------------------------- the sky (a fullscreen pass behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
void main() {
    vec2 n = vUv * 2.0 - 1.0;
    vec4 a = uInvVP * vec4(n, -1.0, 1.0), b = uInvVP * vec4(n, 1.0, 1.0);
    vec3 d = normalize(b.xyz / b.w - a.xyz / a.w);
    vec3 c = d.y < 0.0 ? hazeCol(d) : skySeen(d, true);
    frag = vec4(enc(grade(c)), 1.0);
}
)";

// ---------------------------------------------------------------- the land and its cut face
inline const char* GROUND_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
uniform mat4 uVP;
out vec3 vP;
out vec3 vN;
void main() {
    vP = aPos;
    vN = aNrm;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* GROUND_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
out vec4 frag;
uniform vec3 uSoilDry, uSoilWet, uSoilCut, uCrack, uGrassDry, uGrassLush, uLeafFar, uFlowerA, uFlowerB;
uniform float uPx, uBloom;
// cracked clay: the distance to the nearest edge of a cell of dried mud (F2 - F1 of a jittered grid)
vec2 cells(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 9.0, d2 = 9.0;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(x, y);
            vec2 o = vec2(hash12(i + g), hash12(i + g + 17.3)) * 0.8 + 0.1;
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
        }
    return vec2(sqrt(d2) - sqrt(d1), d1);
}
void main() {
    vec3 N = normalize(vN);
    vec3 P = vP;
    float dist = length(P - uEye);
    vec3 life = lifeAt(P.xz);
    if (N.z > 0.9 && P.z > uFrontZ - 0.05) {
        // the cut face: layers of packed earth, darker deeper, damper under what grows, roots reaching down
        float top = groundH(vec2(P.x, uFrontZ));
        float dd = top - P.y;
        vec3 lt = lifeAt(vec2(P.x, uFrontZ - 1.5));
        float green = max(lt.r, coverAt(vec2(P.x, uFrontZ - 2.0)) * 0.7);
        float band = vnoise(vec2(P.x * 0.6, P.y * 5.0 + 3.0 * vnoise(vec2(P.x * 0.3, 1.0))));
        vec3 alb = mix(uSoilCut, uSoilWet * 1.2, clamp(lt.g * 0.8 + uWet * 0.5, 0.0, 1.0));
        alb *= (0.85 - 0.3 * smoothstep(0.0, 3.0, dd)) * (0.88 + 0.22 * band);
        alb *= 0.96 + 0.08 * hash12(floor(P.xy * 30.0));
        // roots: fine dark threads hanging down from the living soil
        float rx = P.x * 3.0 + 0.6 * sin(P.y * 4.0 + P.x * 0.7) + 0.3 * sin(P.y * 11.0);
        float rid = floor(rx);
        float rlen = (0.4 + 2.2 * hash12(vec2(rid, 4.0))) * green;
        float root = smoothstep(0.12, 0.0, abs(fract(rx) - 0.5) - 0.02) * step(hash12(vec2(rid, 9.0)), 0.45 + 0.4 * green) *
                     smoothstep(rlen, rlen * 0.6, dd);
        alb = mix(alb, uSoilWet * 0.6, root * 0.7);
        vec3 col = alb * (uAmb * 0.95 + uSunCol * max(dot(N, uSunDir), 0.0) * 0.7);
        // the lip of the cut: a lit edge, green once the grass reaches it
        float lip = smoothstep(0.09, 0.0, dd);
        col = mix(col, mix(uSoilDry, uGrassLush, green) * (uAmb + uSunCol * 0.6), lip * 0.6);
        frag = vec4(enc(grade(col)), 1.0);
        return;
    }
    float near = exp(-dist / 40.0);
    // ---- the dry clay: cracked into plates, finer far out
    vec2 cq = P.xz * vec2(1.25, 1.6);
    vec2 cr = cells(cq);
    float lush = max(life.r, coverAt(P.xz));
    float damp = clamp(life.g * 0.9 + lush * 0.5 + uWet * 0.75, 0.0, 1.0);
    float crackW = (0.05 + 0.05 * (1.0 - near)) * (1.0 - damp);
    float crack = smoothstep(crackW + uPx * 2.0, crackW * 0.3, cr.x) * near * (1.0 - damp * 0.85);
    vec3 alb = mix(uSoilDry, uSoilWet, damp * 0.85);
    // each plate a little lighter on its curled-up edges, the cracks dark
    alb *= 0.92 + 0.12 * smoothstep(0.05, 0.3, cr.x) * (1.0 - damp) * near + 0.06 * (vnoise(P.xz * 0.6) - 0.5);
    alb = mix(alb, uCrack * 0.55, crack * 0.85);
    // ---- grass: dry straw tufts in patches, a living green over what grows; flowers in it
    float blade = vnoise(P.xz * vec2(9.0, 3.0)) * near + (1.0 - near) * 0.5;
    vec3 grass = mix(uGrassDry, uGrassLush, smoothstep(0.1, 0.7, lush + 0.3 * uWet)) * (0.8 + 0.35 * blade);
    float gcov = smoothstep(0.15, 0.6, lush + 0.15 * (blade - 0.5));
    alb = mix(alb, grass, gcov);
    // the far land: a forest canopy on the hills and the plain once alive, its crowns in soft bumps
    float far = smoothstep(25.0, 70.0, dist);
    float forest = smoothstep(0.55, 0.9, lush) * far;
    float crowns = vnoise(P.xz * 0.35) * 0.6 + vnoise(P.xz * 0.9) * 0.4;
    alb = mix(alb, uLeafFar * (0.7 + 0.5 * crowns), forest * 0.9);
    // flowers: specks of color in the grass where flowers grew
    {
        vec2 fq = P.xz * 5.0;
        vec2 fi = floor(fq);
        float h = hash12(fi);
        float fl = step(1.0 - life.b * 0.5 * (0.5 + 0.5 * uBloom), h) * smoothstep(0.38, 0.2, length(fract(fq) - 0.5)) * near;
        alb = mix(alb, mix(uFlowerA, uFlowerB, step(0.5, hash12(fi + 3.1))), fl * 0.85);
    }
    // ---- puddles in the hollows after the rain, the sky in them, rings of rain falling in
    float pud = 0.0;
    if (uWet > 0.01) {
        float hol = vnoise(P.xz * 0.11 + 5.0) * 0.7 + vnoise(P.xz * 0.37) * 0.3;
        pud = smoothstep(0.62, 0.66, hol + 0.32 * uWet - 0.28) * near * (1.0 - gcov * 0.7);
    }
    float sh = shadowAt(P + N * 0.05);
    float ndl = max(dot(N, uSunDir), 0.0);
    vec3 col = alb * (uAmb * (0.55 + 0.45 * N.y) + uSunCol * ndl * sh);
    // wet soil glistens a little
    vec3 V = normalize(uEye - P);
    col += uSunCol * pow(max(dot(reflect(-uSunDir, N), V), 0.0), 40.0) * 0.12 * uWet * (1.0 - gcov) * sh;
    if (pud > 0.0) {
        vec3 Np = vec3(0.0, 1.0, 0.0);
        // rain rings
        vec2 rq = P.xz * 2.2;
        vec2 ri = floor(rq);
        float rt = fract(uTime * 1.3 + hash12(ri) * 7.0);
        float ring = smoothstep(0.05, 0.0, abs(length(fract(rq) - 0.5 - (vec2(hash12(ri + 1.0), hash12(ri + 2.0)) - 0.5) * 0.4) - rt * 0.45)) * (1.0 - rt) * uRain;
        Np = normalize(Np + vec3(ring * 0.4, 0.0, ring * 0.3));
        vec3 R = reflect(-V, Np);
        R.y = abs(R.y);
        vec3 refl = skyFull(R, false) * 0.85;
        float F = 0.2 + 0.8 * pow(1.0 - max(dot(Np, V), 0.0), 5.0);
        col = mix(col * 0.5, refl, F * 0.85 + 0.1);
    }
    col = aerial(col, P);
    frag = vec4(enc(grade(col)), 1.0);
}
)";

// ---------------------------------------------------------------- the plants (low-poly meshes, instanced) and the
// shadow casters
inline const char* PLANT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aCtr;   // the leaf clump's / flower's / vine's anchor (growth, blooming)
layout(location = 3) in vec2 aMS;    // material, sway weight
layout(location = 4) in vec4 i0;     // x, y, z, scale
layout(location = 5) in vec4 i1;     // rotation, growth, seed, vines (dead trees: their revival)
layout(location = 6) in vec4 i2;     // type + shade, wither, kind, -
uniform mat4 uVP;
uniform float uSwayT, uWind, uGust, uBloom;
out vec3 vP;
out vec3 vN;
flat out float vMat;
flat out vec4 vI1, vI2;
void main() {
    float g = i1.y;
    int kind = int(i2.z + 0.5);
    int m = int(aMS.x + 0.5);
    float grow = smoothstep(0.0, 1.0, g);
    float s = i0.w * (kind >= 4 ? 0.1 + 0.9 * grow : 0.25 + 0.75 * grow);
    vec3 p = aPos;
    if (m == 1 || m == 2 || m == 8) {
        float ls = kind == 7 ? i1.w : (kind >= 4 ? 0.45 + 0.55 * smoothstep(0.1, 0.7, g) : 1.0);
        p = aCtr + (aPos - aCtr) * ls;
    } else if (m == 4) {
        float open = smoothstep(0.45, 1.0, g) * (0.4 + 0.6 * uBloom);
        p = aCtr + (aPos - aCtr) * (0.2 + 0.8 * open);
    } else if (m == 5) {
        p = aCtr + (aPos - aCtr) * smoothstep(0.35, 0.8, g);
    } else if (m == 6) {
        float v = i1.w;
        p = aCtr + (aPos - aCtr) * vec3(1.0, v, 1.0);
    }
    float c = cos(i1.x), sn = sin(i1.x);
    p = vec3(c * p.x - sn * p.z, p.y, sn * p.x + c * p.z);
    vec3 n = vec3(c * aNrm.x - sn * aNrm.z, aNrm.y, sn * aNrm.x + c * aNrm.z);
    // the wind: a slow sway, the gusts (on the kicks at a peak) bending the crowns and the grass
    float ph = i1.z * 1.7 + i0.x * 0.11 + i0.z * 0.07;
    float amp = (0.22 * uWind * (0.65 + 0.35 * sin(uSwayT * 1.3 + ph)) + 0.32 * uGust) * aMS.y;
    p.x += amp * (0.75 + 0.25 * sin(uSwayT * 2.3 + ph * 2.0));
    p.z += amp * 0.35 * sin(uSwayT * 1.7 + ph);
    vec3 w = i0.xyz + p * s;
    vP = w;
    vN = n;
    vMat = aMS.x;
    vI1 = i1;
    vI2 = i2;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* DEPTH_FS = R"(#version 330 core
void main() {}
)";
inline const char* PLANT_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
flat in float vMat;
flat in vec4 vI1, vI2;
out vec4 frag;
uniform vec3 uBark, uLeafA, uLeafB, uGrassDry, uGrassLush, uDead, uFlowerC, uLeafFar;
uniform vec3 uPieces[7];
void main() {
    int m = int(vMat + 0.5);
    vec3 N = normalize(vN);
    vec3 V = normalize(uEye - vP);
    bool back = dot(N, V) < 0.0;
    if (back) N = -N;
    float seed = vI1.z;
    float h1 = hash12(vec2(seed, 1.7)), h2 = hash12(vec2(seed, 5.3));
    int type = int(clamp(floor(vI2.x), 0.0, 6.0));
    vec3 leaf = mix(uLeafA, uLeafB, h1) * (0.85 + 0.3 * h2);
    vec3 alb;
    bool foliage = false;
    if (m == 0) alb = uBark * (0.85 + 0.3 * h2);
    else if (m == 1) { alb = leaf; foliage = true; }
    else if (m == 2) { alb = mix(leaf, uLeafB, 0.5) * 1.1; foliage = true; }
    else if (m == 3) { alb = mix(uGrassLush, mix(uGrassLush, uGrassDry, 0.4), h1) * (0.85 + 0.3 * h2); foliage = true; }
    else if (m == 4) alb = uPieces[type] * (0.9 + 0.2 * h2);
    else if (m == 5) alb = uFlowerC;
    else if (m == 6) { alb = uLeafA * 0.75; foliage = true; }
    else if (m == 7) alb = mix(uDead, uBark, vI1.w);
    else { alb = mix(leaf, uGrassLush, 0.3); foliage = true; }
    // a facet's own small shade (low-poly crowns)
    alb *= 0.92 + 0.16 * hash13(floor(vP * 3.0));
    // withering (a dry season between songs): straw-colored
    alb = mix(alb, uGrassDry * 0.85, clamp(vI2.y * 2.0, 0.0, 1.0) * (foliage ? 1.0 : 0.3));
    // wet after the rain: darker, a sheen
    alb *= 1.0 - 0.18 * uWet;
    float sh = shadowAt(vP + N * 0.08);
    float ndl = max(dot(N, uSunDir), 0.0);
    vec3 col = alb * (uAmb * (0.55 + 0.45 * max(N.y, 0.0)) + uSunCol * ndl * sh);
    // leaves lit from behind glow through
    if (foliage) col += alb * uSunCol * max(dot(-V, uSunDir), 0.0) * 0.35 * sh;
    col += uSunCol * pow(max(dot(reflect(-uSunDir, N), V), 0.0), 30.0) * (0.04 + 0.2 * uWet) * sh;
    col = aerial(col, vP);
    frag = vec4(enc(grade(col)), 1.0);
}
)";

// ---------------------------------------------------------------- birds, butterflies, fireflies: a camera-facing
// quad each, a small silhouette (a bird's wings beating or gliding; a butterfly's wings opening; a point of light)
inline const char* BIRD_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // position, size
layout(location = 2) in vec4 a1;   // kind, flap, direction (screen x sign), alpha
layout(location = 3) in vec4 a2;   // linear rgb, glow
layout(location = 4) in vec4 a3;   // -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec2 vQ;
out vec3 vP;
flat out vec4 v1, v2;
flat out float vS;
void main() {
    vQ = aQ;
    vP = a0.xyz + (uCamR * aQ.x + uCamU * aQ.y) * a0.w;
    v1 = a1; v2 = a2;
    vS = a0.w;
    gl_Position = uVP * vec4(vP, 1.0);
}
)";
inline const char* BIRD_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vP;
flat in vec4 v1, v2;
flat in float vS;
out vec4 frag;
uniform float uPx;
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}
void main() {
    int kind = int(v1.x + 0.5);
    float flap = v1.y;
    vec2 q = vQ;
    q.x *= v1.z;
    float px = uPx * length(vP - uEye) / 34.6 / vS;
    vec3 col = v2.rgb;
    float a = 0.0;
    float glow = 0.0;
    if (kind <= 1) {
        // a bird: a small body and two wings, their tips beating up and down
        float up = sin(flap);
        vec2 tipL = vec2(-0.85, 0.15 + 0.55 * up), tipR = vec2(0.85, 0.15 + 0.55 * up);
        vec2 elL = vec2(-0.38, 0.12 + 0.12 * up), elR = vec2(0.38, 0.12 + 0.12 * up);
        float w = min(min(sdSeg(q, vec2(-0.05, 0.0), elL), sdSeg(q, elL, tipL)) - 0.07 * (1.0 - length(q) * 0.7),
                      min(sdSeg(q, vec2(0.05, 0.0), elR), sdSeg(q, elR, tipR)) - 0.07 * (1.0 - length(q) * 0.7));
        float body = length((q - vec2(0.12, -0.02)) / vec2(0.3, 0.1)) - 1.0;
        body *= 0.1;
        float d = min(w, body);
        a = smoothstep(px * 1.2, -px * 1.2, d);
    } else if (kind == 2) {
        // a butterfly: two pairs of wings opening and closing
        float open = 0.25 + 0.75 * abs(cos(flap));
        vec2 b = vec2(abs(q.x) / open, q.y);
        float up = length((b - vec2(0.42, 0.22)) / vec2(0.42, 0.36)) - 1.0;
        float lo = length((b - vec2(0.3, -0.25)) / vec2(0.3, 0.26)) - 1.0;
        float d = min(up, lo) * 0.3;
        a = smoothstep(px * 1.2, -px * 1.2, d);
        col *= 0.8 + 0.4 * smoothstep(0.6, 0.1, length(b - vec2(0.4, 0.2)));
        float bd = abs(q.x) - 0.04;
        a = max(a, smoothstep(px, -px, max(bd, abs(q.y) - 0.35)) * 0.9);
    } else {
        // a firefly: a point of light, breathing
        float r = length(q);
        float br = 0.6 + 0.4 * sin(flap * 2.0);
        a = smoothstep(0.25, 0.0, r) * br;
        glow = exp(-r * 3.0) * br;
        col = mix(col, vec3(1.0), 0.4);
    }
    a *= v1.w;
    if (a <= 0.003 && glow <= 0.003) discard;
    vec3 c = kind == 3 ? col : aerial(col, vP);
    float ga = glow * v1.w * 0.6;
    frag = vec4(enc(grade(c)) * (a + ga), max(a, ga * 0.5));
}
)";

// ---------------------------------------------------------------- particles: seeds in flight, glints of glass, dust,
// splashes, pollen. Kinds: 0 a soft disc (stretched along its way), 1 a seed (a small lit grain, turning), 2 a glint
// of glass, 5 a soft cloud.
inline const char* PART_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // position, size
layout(location = 2) in vec4 a1;   // velocity, kind
layout(location = 3) in vec4 a2;   // linear rgb, alpha
layout(location = 4) in vec4 a3;   // stretch, softness, seed, angle
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec2 vQ;
out vec3 vP;
flat out vec4 v1, v2, v3;
flat out float vS;
void main() {
    vec3 c = a0.xyz;
    float s = a0.w;
    vec3 fwd = normalize(c - uEye);
    vec3 ax = uCamR, ay = uCamU;
    float lx = s, ly = s;
    vec3 v = a1.xyz - fwd * dot(a1.xyz, fwd);
    float sp = length(v);
    if (a3.x > 0.0 && sp > 0.05) {
        ax = v / sp;
        ay = normalize(cross(fwd, ax));
        lx = s * (1.0 + min(sp * a3.x, 3.0));
    }
    vQ = aQ;
    vP = c + ax * aQ.x * lx + ay * aQ.y * ly;
    v1 = a1; v2 = a2; v3 = a3;
    vS = s;
    gl_Position = uVP * vec4(vP, 1.0);
}
)";
inline const char* PART_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vP;
flat in vec4 v1, v2, v3;
flat in float vS;
out vec4 frag;
uniform float uPx;
uniform float uAerial;
void main() {
    int kind = int(v1.w + 0.5);
    float r = length(vQ);
    float a;
    vec3 col = v2.rgb;
    if (kind == 1) {
        // a seed: an oval grain turning in the air, lit from the sun's side
        float an = v3.w;
        vec2 q = vec2(cos(an) * vQ.x + sin(an) * vQ.y, -sin(an) * vQ.x + cos(an) * vQ.y);
        float e = length(q / vec2(0.95, 0.55));
        float pxl = uPx * length(vP - uEye) / 34.6 / max(vS, 1e-3);
        a = smoothstep(1.0 + pxl, 1.0 - pxl, e);
        vec3 n = normalize(vec3(q / vec2(0.95, 0.55) * 0.9, sqrt(max(1.0 - e * e, 0.0)) + 0.2));
        float lit = 0.55 + 0.45 * dot(n, normalize(vec3(-0.4, 0.6, 0.7)));
        col = col * lit + vec3(1.0) * pow(max(dot(n, normalize(vec3(-0.3, 0.5, 0.8))), 0.0), 18.0) * 0.5;
        // a fine dark outline: it reads over a pale sky
        col *= mix(0.55, 1.0, smoothstep(0.75, 0.55, e));
    } else if (kind == 2) {
        // a glint of broken glass: a small bright splinter
        float an = v3.w;
        vec2 q = vec2(cos(an) * vQ.x + sin(an) * vQ.y, -sin(an) * vQ.x + cos(an) * vQ.y);
        float d = abs(q.x) * 0.35 + abs(q.y);
        a = smoothstep(0.45, 0.25, d);
    } else if (kind == 5) {
        a = exp(-r * r * 3.0) * smoothstep(1.0, 0.8, r);
    } else {
        a = smoothstep(1.0, 1.0 - max(v3.y, 0.05), r);
    }
    if (a <= 0.003) discard;
    if (uAerial > 0.5) col = aerial(col, vP);
    frag = vec4(enc(grade(col)), a * v2.a);
}
)";

// ---------------------------------------------------------------- the rain: long thin streaks slanting across the
// land (never on the well; lighter over the HUD's columns)
inline const char* RAIN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;   // the board plane: center x, y, half extents x, y
uniform float uPx, uRainA, uTimeR;
uniform vec3 uRainCol;
float layer(vec2 w, float scale, float speed, float seed) {
    float an = 0.18;
    vec2 r = vec2(cos(an) * w.x - sin(an) * w.y, sin(an) * w.x + cos(an) * w.y) * scale;
    float colI = floor(r.x * 3.0);
    float off = hash12(vec2(colI, seed));
    float y = r.y * 0.1 + uTimeR * speed * (1.0 + 0.4 * off) + off * 7.0;
    float v = fract(y);
    float line = smoothstep(uPx * scale * 1.3, 0.0, abs(fract(r.x * 3.0) - 0.5) / 3.0) * smoothstep(0.0, 0.03, v) * smoothstep(0.16, 0.05, v);
    return line * step(0.45, hash12(vec2(colI, floor(y) + seed)));
}
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 wd = abs(w) - vec2(5.08, 10.08);
    float wellD = length(max(wd + 0.25, 0.0)) + min(max(wd.x + 0.25, wd.y + 0.25), 0.0) - 0.25;
    float inWell = smoothstep(uPx, -uPx, wellD);
    float a = layer(w, 1.0, 2.6, 3.0) * 0.6 + layer(w, 1.9, 2.0, 11.0) * 0.35 + layer(w, 3.3, 1.5, 23.0) * 0.2;
    float hudCol = abs(w.x) > 5.8 && abs(w.x) < 11.5 && w.y > -7.0 ? 0.4 : 1.0;
    a *= uRainA * 0.45 * hudCol * (1.0 - inWell);
    frag = vec4(enc(uRainCol), a);
}
)";

// ---------------------------------------------------------------- the well's pane and the bridge
// The well: a calm, even, dark pane over the world behind it, with a fine edge; and, while a change of universe runs,
// the whole world fading to the bridge's color (premultiplied).
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform float uPx, uPanelA, uBridge;
uniform vec3 uPanel, uEdge, uBridgeCol;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 wd = abs(w) - vec2(5.08, 10.08);
    float wellD = length(max(wd + 0.25, 0.0)) + min(max(wd.x + 0.25, wd.y + 0.25), 0.0) - 0.25;
    float inWell = smoothstep(uPx, -uPx, wellD);
    float wellE = smoothstep(uPx * 1.5, 0.0, abs(wellD + uPx));
    float a1 = clamp(inWell * uPanelA + wellE * 0.4 * (1.0 - inWell * uPanelA), 0.0, 1.0);
    vec3 c1 = mix(uPanel, uEdge, wellE * 0.4 / max(a1, 1e-3));
    vec3 pre = enc(c1) * a1 * (1.0 - uBridge) + enc(uBridgeCol) * uBridge;
    frag = vec4(pre, 1.0 - (1.0 - a1) * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the stack's cubes (at the board plane)
// Every cell is its own small glass cube, three quarters full of seeds of its piece's color, seen in a slight
// three-quarter view from above the well's middle (an oblique ray from each pixel of its front face): the seeds are
// traced through the glass (a packed bed of grains, each piece type its own seed), the air above them shows the
// world through the glass.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // x, y, size, type
layout(location = 2) in vec4 a1;   // linear rgb (the seeds' color), alpha
layout(location = 3) in vec4 a2;   // ghost, crack, -, -
layout(location = 4) in vec4 a3;   // seed, -, -, -
uniform mat4 uVP;
out vec2 vP;
flat out vec4 v0, v1, v2, v3;
flat out vec3 vD;
void main() {
    vP = aQ * 0.75;
    v0 = a0; v1 = a1; v2 = a2; v3 = a3;
    vec2 w = a0.xy + vP * a0.z;
    vD = normalize(vec3(clamp(a0.x * 0.045, -0.3, 0.3), -0.3, -1.0));
    gl_Position = uVP * vec4(w, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vP;
flat in vec4 v0, v1, v2, v3;
flat in vec3 vD;
out vec4 frag;
uniform sampler2D uBg;
uniform vec2 uRes, uSunP;
uniform float uPx, uTime, uBridge, uNight;
uniform vec3 uBridgeInk, uSkyTop, uSkyLow, uSunCol, uSoil;
const float H = 0.44;    // the cube's half size
const float RO = 0.06;   // its rounded edges
const float HI = 0.375;  // the inside's half size (the glass is thin)
float sdBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}
float sdOut(vec3 p) { return sdBox(p, vec3(H), RO); }
vec3 nOut(vec3 p) {
    const vec2 k = vec2(1.0, -1.0) * 0.0015;
    return normalize(k.xyy * sdOut(p + k.xyy) + k.yyx * sdOut(p + k.yyx) + k.yxy * sdOut(p + k.yxy) + k.xxx * sdOut(p + k.xxx));
}
vec3 bgAt(vec2 offBoard) {
    vec2 uv = (gl_FragCoord.xy + offBoard / uPx) / uRes;
    return pow(texture(uBg, clamp(uv, vec2(0.001), vec2(0.999))).rgb, vec3(2.2));
}
vec3 sunDir;
vec3 env(vec3 r) {
    vec3 sky = mix(uSkyLow, uSkyTop, smoothstep(0.0, 0.7, r.y));
    vec3 c = r.y > 0.0 ? sky : uSoil * 0.6;
    return c + uSunCol * pow(max(dot(r, sunDir), 0.0), 900.0) * 6.0;
}
vec3 sort3(vec3 a) {
    float lo = min(a.x, min(a.y, a.z)), hi = max(a.x, max(a.y, a.z));
    return vec3(lo, a.x + a.y + a.z - lo - hi, hi);
}
// ---- the seeds: a packed bed of grains, one per cell of a grid, each jittered and turned, its shape the piece's
// (0 long grains, 1 round peas, 2 flat almonds, 3 big kidney beans, 4 striped sunflower seeds, 5 fine millet, 6 lentils)
int gType;
float gCell, gTop, gSeed;
vec3 gR;
float topAt(vec2 xz) { return gTop + 0.025 * sin(xz.x * 11.0 + gSeed) * sin(xz.y * 9.0 + gSeed * 1.7); }
// the grain in cell `id`: its center, its radii, its turn (around y) and tilt
void grain(vec3 id, out vec3 c, out vec3 r, out float ang, out float tilt) {
    vec3 h = hash33(id + gSeed * 7.13);
    float slack;
    if (gType == 0) r = vec3(0.072, 0.03, 0.032);
    else if (gType == 1) r = vec3(0.056);
    else if (gType == 2) r = vec3(0.073, 0.026, 0.05);
    else if (gType == 3) r = vec3(0.086, 0.056, 0.06);
    else if (gType == 4) r = vec3(0.073, 0.034, 0.045);
    else if (gType == 5) r = vec3(0.037);
    else r = vec3(0.064, 0.026, 0.064);
    r *= 0.92 + 0.16 * h.z;
    slack = gCell * 0.5 - max(r.x, max(r.y, r.z));
    c = (id + 0.5) * gCell + (h - 0.5) * 2.0 * max(slack, 0.0) * vec3(1.0, 0.6, 1.0);
    ang = h.x * 6.2832;
    tilt = (h.y - 0.5) * 0.6;
}
vec3 toLocal(vec3 p, float ang, float tilt) {
    float c = cos(ang), s = sin(ang);
    p = vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
    float ct = cos(tilt), st = sin(tilt);
    return vec3(ct * p.x - st * p.y, st * p.x + ct * p.y, p.z);
}
vec3 fromLocal(vec3 p, float ang, float tilt) {
    float ct = cos(tilt), st = sin(tilt);
    p = vec3(ct * p.x + st * p.y, -st * p.x + ct * p.y, p.z);
    float c = cos(ang), s = sin(ang);
    return vec3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z);
}
float sdGrain(vec3 q, vec3 r) {
    float k0 = length(q / r), k1 = length(q / (r * r));
    float d = k0 * (k0 - 1.0) / max(k1, 1e-5);
    if (gType == 3) d = max(d, -(length(q - vec3(0.0, 0.0, r.z * 1.05)) - r.z * 0.45));   // the bean's kidney dent
    return d;
}
// the distance to the bed (inside the cell holding p), and that grain
float sdBed(vec3 p, out vec3 hitC, out vec3 hitR, out float hitA, out float hitT) {
    vec3 id = floor(p / gCell);
    vec3 c, r;
    float ang, tilt;
    grain(id, c, r, ang, tilt);
    hitC = c; hitR = r; hitA = ang; hitT = tilt;
    // only under the bed's top, and not through the glass
    if (c.y > topAt(c.xz) || any(greaterThan(abs(c), vec3(HI)))) return 1e3;
    // pressed flat against the glass where they touch it
    return max(sdGrain(toLocal(p - c, ang, tilt), r), sdBox(p, vec3(HI - 0.004), 0.0));
}
// one sample of the cube at sP (the front face's frame): linear color, coverage
vec4 shade(vec2 sP) {
    float size = v0.z;
    float px = uPx / size;
    vec3 tint = v1.rgb;
    vec3 D = vD;
    sunDir = normalize(vec3(uSunP - v0.xy, 9.0));
    vec3 o = vec3(sP, H) - D * 0.8;
    float t = 0.0, md = 1e9;
    bool hit = false;
    for (int i = 0; i < 40; i++) {
        float s = sdOut(o + D * t);
        md = min(md, s);
        if (s < 0.0004) { hit = true; break; }
        t += s;
        if (t > 2.6) break;
    }
    float cover = hit ? 1.0 : smoothstep(px, 0.0, md);
    if (cover <= 0.0) return vec4(0.0);
    vec3 p1 = o + D * t;
    vec3 n1 = nOut(p1);
    if (v2.x > 0.5) {
        // the ghost: an empty glass cube, its edges bright, a faint tint of its seeds' color inside
        vec3 a = sort3(abs(p1));
        float edge = smoothstep(px * 2.5 + RO * 0.5, RO * 0.3, H - a.y);
        vec3 ec = mix(tint, vec3(1.0), 0.6);
        return vec4(mix(ec, uBridgeInk, uBridge), (edge * 0.8 + 0.12) * cover * v1.a);
    }
    float cosI = clamp(dot(-D, n1), 0.0, 1.0);
    float F = 0.04 + 0.96 * pow(1.0 - cosI, 5.0);
    vec3 refl = reflect(D, n1);
    // ---- through the thin glass into the cube (its faces parallel: the ray keeps its way), to the inner box
    vec3 d = refract(D, n1, 1.0 / 1.5);
    vec3 p = p1 + d * 0.06;
    vec3 inv = 1.0 / D;
    vec3 t0 = (-vec3(HI) - p) * inv, t1 = (vec3(HI) - p) * inv;
    vec3 tmin = min(t0, t1), tmax = max(t0, t1);
    float tN = max(max(tmin.x, tmin.y), max(tmin.z, 0.0)), tF = min(min(tmax.x, tmax.y), tmax.z);
    vec3 col;
    gType = int(v0.w + 0.5);
    gSeed = v3.x;
    gCell = gType == 5 ? 0.082 : gType == 3 ? 0.18 : gType == 1 ? 0.118 : 0.15;
    gTop = 0.25;
    bool seedHit = false;
    vec3 hp = vec3(0.0), hc, hr;
    float ha, ht;
    if (tF > tN) {
        float s = tN;
        for (int i = 0; i < 56; i++) {
            vec3 q = p + D * s;
            vec3 c0, r0;
            float a0, b0;
            float dd = sdBed(q, c0, r0, a0, b0);
            if (dd < 0.0008) { seedHit = true; hp = q; hc = c0; hr = r0; ha = a0; ht = b0; break; }
            // never step past the cell (the next grain may lie in it)
            vec3 cellLo = floor(q / gCell) * gCell;
            vec3 tc = (cellLo + step(0.0, D) * gCell - q) * inv;
            float tcell = min(min(tc.x, tc.y), tc.z) + 0.0015;
            s += min(dd, tcell);
            if (s > tF) break;
        }
    }
    if (seedHit) {
        // a lit grain: its own shade, darker deep in the bed (the light reaches it less)
        vec3 lq = toLocal(hp - hc, ha, ht);
        vec3 n = fromLocal(normalize(lq / (hr * hr)), ha, ht);
        float h = hash13(floor(hp / gCell) + gSeed);
        vec3 alb = tint * (0.78 + 0.4 * h);
        if (gType == 4) alb *= 0.62 + 0.38 * smoothstep(0.25, 0.6, abs(fract(lq.z / hr.z * 1.6 + 0.5) - 0.5) * 2.0);   // stripes
        if (gType == 3) alb = mix(alb, vec3(1.0), smoothstep(0.012, 0.0, length(lq.xz - vec2(0.0, hr.z * 0.6)) - 0.006) * 0.5);
        float depth = topAt(hp.xz) - hp.y;
        float ao = mix(0.32, 1.0, exp(-depth * 5.5)) * (0.75 + 0.25 * smoothstep(-0.6, 0.6, n.y));
        float ndl = max(dot(n, sunDir), 0.0);
        col = alb * (0.38 + 0.8 * ndl) * ao;
        vec3 hv = normalize(sunDir - D);
        col += uSunCol * pow(max(dot(n, hv), 0.0), 40.0) * 0.35 * ao;
        // a soft rim: grains read one by one
        col += tint * pow(1.0 - max(dot(n, -D), 0.0), 3.0) * 0.18 * ao;
        if (uNight > 0.5) col += tint * 0.12;
        col *= vec3(0.96, 0.98, 0.97);   // through the glass
    } else {
        // the air above the seeds: the world through the glass, a little darkened and bent
        vec2 off = (d.xy - D.xy) * 0.6;
        col = bgAt(off * size) * 0.82 + tint * 0.04;
    }
    // ---- the surface: the sky reflected (Fresnel), the sun's highlight
    vec3 rc = env(refl) + mix(uSkyLow, vec3(1.0), 0.6) * 0.4;
    col = mix(col, rc, F * 0.8);
    col += uSunCol * pow(max(dot(refl, sunDir), 0.0), 220.0) * 1.1;
    // ---- crisp bright lines: the glass's edges
    {
        vec3 a = sort3(abs(p1));
        float edge = smoothstep(max(RO * 0.8, RO * 0.35 + px * 1.6), RO * 0.35, H - a.y);
        col = mix(col, mix(uSkyLow, vec3(1.0), 0.7), clamp(edge * 0.6, 0.0, 1.0));
    }
    // ---- a crack (game over: the stack cracks before it breaks)
    float crk = v2.y;
    if (crk > 0.001 && n1.z > 0.5) {
        vec2 q = p1.xy - vec2(0.1, -0.12);
        float r = length(q);
        float ang = atan(q.y, q.x);
        float k = floor(ang / 6.2832 * 5.0 + 0.5);
        float aj = (k + 0.4 * (hash12(vec2(k, v3.x)) - 0.5)) / 5.0 * 6.2832;
        vec2 dir = vec2(cos(aj), sin(aj));
        float along = dot(q, dir);
        float off = abs(q.x * dir.y - q.y * dir.x);
        float len = crk * (0.15 + 0.25 * hash12(vec2(k, v3.x + 3.0)));
        float line = smoothstep(px * 1.4, 0.0, off) * step(0.0, along) * smoothstep(len, len - 0.03, along);
        col = mix(col, vec3(1.0), max(line, smoothstep(0.035 * crk + px, 0.0, r)) * 0.9);
    }
    col = mix(col, uBridgeInk, uBridge);
    return vec4(col, cover * v1.a);
}
void main() {
    float pxl = uPx / v0.z;
    const vec2 G[4] = vec2[4](vec2(-0.125, -0.375), vec2(0.375, -0.125), vec2(0.125, 0.375), vec2(-0.375, 0.125));
    vec3 acc = vec3(0.0);
    float a = 0.0;
    for (int i = 0; i < 4; i++) {
        vec4 r = shade(vP + G[i] * pxl);
        acc += r.rgb * r.a;
        a += r.a;
    }
    if (a <= 0.0) discard;
    frag = vec4(enc(acc / a), a * 0.25);
}
)";

// ---------------------------------------------------------------- the bonus gauge: a round glass filling with seeds
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // x, y, half x, half y
layout(location = 2) in vec4 a1;   // -, -, fill, ready
layout(location = 3) in vec4 a2;   // linear rgb, alpha
layout(location = 4) in vec4 a3;   // -
uniform mat4 uVP;
out vec2 vQ;
flat out vec4 v0, v1, v2;
void main() {
    vQ = aQ;
    v0 = a0; v1 = a1; v2 = a2;
    gl_Position = uVP * vec4(a0.xy + aQ * a0.zw, 0.0, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vQ;
flat in vec4 v0, v1, v2;
out vec4 frag;
uniform float uTime, uPx;
void main() {
    vec3 col = v2.rgb;
    float R = 0.92;
    float pxl = uPx / v0.z;
    float r = length(vQ);
    float fill = v1.z, ready = v1.w;
    float rim = smoothstep(pxl * 1.5, 0.0, abs(r - R) - 0.025);
    float lvl = -R + 2.0 * R * fill + 0.03 * sin(vQ.x * 14.0);
    float inside = smoothstep(R - 0.05 + pxl, R - 0.05 - pxl, r);
    float bed = smoothstep(pxl, -pxl, vQ.y - lvl) * inside;
    // the seeds in it: small round grains
    vec2 g = vQ * 9.0;
    vec2 gi = floor(g);
    float grain = smoothstep(0.48, 0.3, length(fract(g) - 0.5 + (vec2(hash12(gi), hash12(gi + 3.0)) - 0.5) * 0.15));
    vec3 sc = col * (0.55 + 0.6 * grain * (0.8 + 0.4 * hash12(gi + 7.0)));
    sc = mix(sc, mix(col, vec3(1.0), 0.5), ready * 0.3 * (0.5 + 0.5 * sin(uTime * 2.0)));
    vec3 c = mix(vec3(1.0), sc, bed);
    float aa = mix(inside * 0.1, 0.95, bed);
    c = mix(c, vec3(1.0), rim * 0.85);
    aa = max(aa, rim * 0.8);
    frag = vec4(enc(c), aa * v2.a);
}
)";

// The HUD's letters: signed distance fields, with a soft halo of the other value under them (they read over the sky,
// the bare clay and the jungle alike).
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
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uBias, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float a = smoothstep(0.5 - uBias - w, 0.5 - uBias + w, d);
    float h = smoothstep(0.5 - uBias - 0.3, 0.5 - uBias - 0.02, d) * uHaloA;
    float at = max(a, h);
    vec3 c = mix(uHalo, uInk, a / max(at, 1e-3));
    frag = vec4(enc(c), at * uAlpha);
}
)";

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
void main() { frag = vec4(enc(vCol.rgb), vCol.a); }
)";

// ---------------------------------------------------------------- the final pass
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.12 * dot(q, q) * 2.0;
    c *= 1.0 - uDim * 0.5;
    c *= uFade;
    frag = vec4(c, 1.0);
}
)";

} // namespace rewild_shaders

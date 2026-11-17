#pragma once
// BIOME's land, its shaders (see land.cpp; the shared ones in shaders.hpp): the 3D world (the land's haze, the land
// and its cut face, the plants, the mounds and sprouts of the sown seeds, the birds), the seeds inside the glass cubes.

namespace land_shaders {

// The world: the land, its haze, its life (land.hpp's groundH is the same function); the sky is SKY_GLSL's. World
// units are board units; the board plane is z = 0, the diorama is cut at z = uFrontZ behind it.
inline const char* WORLD_GLSL = R"(
uniform float uLife, uWet;   // how alive the land is, how wet its soil
uniform vec3 uMist;

// the land: a shallow plain behind the well, gently rolling, rising far out into low hills
float groundH(vec2 p) {
    float dz = -p.y;
    float d = 0.35 * sin(p.x * 0.23 + 1.3 + p.y * 0.13) + 0.22 * sin(p.x * 0.61 + 0.4 - p.y * 0.29) + 0.12 * sin(p.x * 1.3 + 2.0 + p.y * 0.7);
    float hill = smoothstep(30.0, 170.0, dz) * (5.0 + 4.0 * sin(p.x * 0.035 + 0.8) + 3.0 * sin(p.x * 0.071 - p.y * 0.02 + 2.1) +
                                              2.0 * sin(p.x * 0.13 + p.y * 0.05));
    return -9.4 + 1.2 * smoothstep(5.0, 60.0, dz) + d * (1.0 + 0.8 * smoothstep(10.0, 80.0, dz)) + hill;
}

vec3 hazeCol(vec3 d);
// the sky as seen: melting into the land's haze at the horizon (no seam, rain or shine)
vec3 skySeen(vec3 d, bool stars) {
    return mix(skyFull(d, stars), hazeCol(d), (1.0 - smoothstep(0.0, 0.07, d.y)) * 0.85);
}
// the haze far out: a dusty haze over the dry land, a humid mist settling in the hollows of the living one
vec3 hazeCol(vec3 d) {
    vec3 h = mix(skyBase(normalize(vec3(d.x, 0.0, d.z))), mix(uDust, uMist, smoothstep(0.1, 0.8, uLife)), 0.45);
    // a living land's humid air holds the light down (the forest's depth reads through it)
    return mix(h, uStormCloud * 1.05, uStorm * 0.5) * (1.0 - 0.18 * smoothstep(0.3, 1.0, uLife));
}
vec3 aerial(vec3 c, vec3 P) {
    vec3 d = P - uEye;
    float L = length(d);
    float dens = 0.0042 + 0.003 * (1.0 - uLife) + 0.002 * uStorm;
    float k = 1.0 - exp(-max(L - 28.0, 0.0) * dens);
    // the mist: low over the land, far out, in a living land and in the rain; in banks drifting slowly between the
    // trees (each row of the forest a paler layer than the one before it), lit a little brighter than the haze
    float bank = 0.6 + 0.8 * vnoise(vec2(P.x * 0.045 + uTime * 0.02, P.z * 0.08));
    float hf = exp(-max(P.y + 9.0, 0.0) / (2.5 + 2.5 * bank));
    float mist = (0.6 * smoothstep(0.35, 1.0, uLife) + 0.5 * uStorm) * hf * (1.0 - exp(-max(uFrontZ - P.z - 5.0, 0.0) * 0.03));
    vec3 h = mix(hazeCol(d), uMist * 0.9 + uSunCol * 0.03, clamp(mist, 0.0, 1.0) * 0.5);
    return mix(c, h, clamp(k * 0.94 + mist * 0.65, 0.0, 0.96));
}
// the near land's own grade before the air veils it: deeper shadows, richer greens (the shadows sink, the colors
// are pushed away from their grey)
vec3 rich(vec3 c) {
    float l = dot(c, vec3(0.3, 0.5, 0.2));
    c = mix(vec3(l), c, 1.28);
    return max(c, 0.0) * mix(0.62, 1.0, smoothstep(0.0, 0.16, l));
}
// a particle in the world: through the air
vec3 partFog(vec3 c, vec3 P) { return aerial(c, P); }

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
    // the rain's clouds heavy and dark high up
    c *= 1.0 - 0.35 * uStorm * smoothstep(0.05, 0.4, d.y);
    frag = vec4(enc(grade(c)), 1.0);
}
)";

// ---------------------------------------------------------------- the light shafts: the light falling through the
// gaps of the canopy into the misty air, over the world drawn so far (added). Each pixel's ray is marched through the
// air from the diorama's cut up to what it meets (the world's depth); under the crowns a point of it is lit if the
// light reaches it through a break in the clouds (a few broad beams) and a gap of the canopy (followed up along the light to the crowns' height: a pattern of gaps
// long in depth, so that a shaft stays one across the forest and reads, drifting very slowly), above them always; the mist is denser low. The light comes down slanting from the sun's
// side, a little toward the eye, so that its shafts are seen across the picture.
inline const char* SHAFT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform sampler2D uDepth;
uniform vec3 uShaftDir, uShaftCol;   // toward the light, the light in the air
uniform float uGaps;                 // how open the canopy (1: an open sky)
const float CANOPY_Y = -3.0, CLOUD_Y = 40.0;
float lit(vec3 P) {
    // the breaks in the clouds: a few broad beams
    vec2 b = (P + uShaftDir * ((CLOUD_Y - P.y) / uShaftDir.y)).xz;
    float sky = mix(0.25, 1.0, smoothstep(0.5, 0.7, vnoise(b * vec2(0.035, 0.006) + vec2(1.7, uTime * 0.002))));
    if (P.y > CANOPY_Y) return sky;
    vec2 c = (P + uShaftDir * ((CANOPY_Y - P.y) / uShaftDir.y)).xz;
    float n = vnoise(c * vec2(0.2, 0.012) + vec2(3.1, uTime * 0.004)) * 0.8 + vnoise(c * vec2(0.6, 0.05) + 7.7) * 0.2;
    return sky * smoothstep(0.62 - 0.3 * uGaps, 0.74 - 0.2 * uGaps, n);
}
void main() {
    vec2 n = vUv * 2.0 - 1.0;
    float z = texture(uDepth, vUv).r;
    vec4 b = uInvVP * vec4(n, z * 2.0 - 1.0, 1.0);
    vec3 d = b.xyz / b.w - uEye;
    float far = min(length(d), 130.0);
    d = normalize(d);
    float t0 = d.z < -1e-3 ? (uEye.z - uFrontZ) / -d.z : 0.0;
    if (far <= t0) discard;
    // interleaved gradient noise: each pixel's steps a little apart (no banding)
    float j = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    const int N = 20;
    float T = 1.0, acc = 0.0, prev = t0;
    for (int i = 0; i < N; i++) {
        float t = t0 + (far - t0) * pow((float(i) + j) / float(N), 1.4);
        float dt = t - prev;
        prev = t;
        vec3 P = uEye + d * t;
        float dens = 0.04 * exp(-max(P.y + 9.0, 0.0) / 14.0);
        acc += T * lit(P) * dens * dt;
        T *= exp(-dens * dt);
    }
    frag = vec4(uShaftCol * acc, 0.0);
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
        alb *= 1.0 - 0.35 * uWet;
        vec3 col = alb * (uAmb * 0.95 + uSunCol * max(dot(N, uSunDir), 0.0) * 0.7);
        // the lip of the cut: a lit edge, green once the grass reaches it
        float lip = smoothstep(0.09, 0.0, dd);
        col = mix(col, mix(uSoilDry, uGrassLush, green) * (uAmb + uSunCol * 0.6), lip * 0.6);
        col = rich(col);
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
        pud = smoothstep(0.6, 0.64, hol + 0.4 * uWet - 0.28) * near * (1.0 - gcov * 0.6);
    }
    float sh = shadowAt(P + N * 0.05);
    float ndl = max(dot(N, uSunDir), 0.0);
    // the shade under what grows (the damp soil is where the plants stand)
    float ao = 1.0 - 0.45 * clamp(life.g * 1.2 + forest * 0.5, 0.0, 1.0);
    vec3 col = alb * (uAmb * (0.55 + 0.45 * N.y) * ao + uSunCol * ndl * sh * mix(1.0, ao, 0.4));
    // wet soil glistens: the sun, and the sky at grazing angles
    vec3 V = normalize(uEye - P);
    col += uSunCol * pow(max(dot(reflect(-uSunDir, N), V), 0.0), 40.0) * 0.12 * uWet * (1.0 - gcov) * sh;
    col = mix(col, skyFull(reflect(-V, N), false) * 0.6, (0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0)) * uWet * 0.6 * (1.0 - gcov * 0.8));
    col = rich(col);
    if (pud > 0.0) {
        vec3 Np = vec3(0.0, 1.0, 0.0);
        // rain rings
        vec2 rq = P.xz * 2.2;
        vec2 ri = floor(rq);
        float rt = fract(uTime * 1.3 + hash12(ri) * 7.0);
        float ring = smoothstep(0.05, 0.0, abs(length(fract(rq) - 0.5 - (vec2(hash12(ri + 1.0), hash12(ri + 2.0)) - 0.5) * 0.4) - rt * 0.45)) * (1.0 - rt) * uStorm;
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
// shadow casters; a sown seed's soil mound (kind 8) and its sprout (kind 9)
inline const char* PLANT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aCtr;   // the leaf clump's / flower's / vine's anchor (growth, blooming)
layout(location = 3) in vec2 aMS;    // material, sway weight
layout(location = 4) in vec4 i0;     // x, y, z, scale
layout(location = 5) in vec4 i1;     // rotation, growth (a mound: its height), seed, vines (dead trees: their revival)
layout(location = 6) in vec4 i2;     // type + shade, wither, kind, glow (a young plant's, a sprout's)
uniform mat4 uVP;
uniform float uSwayT, uWind, uGust, uBloom;
out vec3 vP;
out vec3 vN;
out vec3 vL;      // the mesh's own point (a wet leaf's glints stay on it)
out float vAO;    // the light the plant leaves to its own inside (a crown's underside, the trunk under it)
flat out float vMat;
flat out vec4 vI1, vI2;
void main() {
    float g = i1.y;
    int kind = int(i2.z + 0.5);
    int m = int(aMS.x + 0.5);
    float grow = smoothstep(0.0, 1.0, g);
    // a plant grows out of its sprout from a speck (never popping up)
    float s = i0.w * (kind >= 4 ? 0.1 + 0.9 * grow : 0.25 + 0.75 * grow) * smoothstep(0.0, 0.06, g);
    vec3 p = aPos;
    if (kind == 8) {
        // the soil mound over a buried seed: heaped up, then settling
        s = i0.w;
        p = vec3(aPos.x * (0.55 + 0.45 * g), aPos.y * g, aPos.z * (0.55 + 0.45 * g));
    } else if (kind == 9) {
        // a sprout: its stem pushes out of the soil bent in a hook, the folded seed leaves trailing; the hook
        // straightens, the two seed leaves unfold
        s = i0.w;
        float e = smoothstep(0.0, 0.5, g), st = smoothstep(0.3, 0.8, g), op = smoothstep(0.55, 1.0, g);
        const float HS = 0.55, U0 = 0.5;
        float L = HS * e;
        float th = (1.0 - st) * 2.7;
        float u = m == 10 ? clamp(aPos.y / HS, 0.0, 1.0) : 1.0;
        vec3 off = m == 10 ? vec3(aPos.x, 0.0, aPos.z) : aPos - aCtr;
        vec2 cc = vec2(0.0, u * L);
        float phi = 0.0;
        if (u > U0 && th > 1e-3) {
            float r = (1.0 - U0) * L / th;
            phi = th * (u - U0) / (1.0 - U0);
            cc = vec2(r * (1.0 - cos(phi)), U0 * L + r * sin(phi));
        }
        if (m == 11) {
            // folded up together along the stem until they open, small at first
            float f = (off.x >= 0.0 ? 1.0 : -1.0) * (1.0 - op) * 1.3;
            off = vec3(off.x * cos(f) - off.y * sin(f), off.x * sin(f) + off.y * cos(f), off.z);
            off *= 0.3 + 0.7 * smoothstep(0.2, 0.9, g);
        }
        off = vec3(off.x * cos(phi) + off.y * sin(phi), -off.x * sin(phi) + off.y * cos(phi), off.z);
        p = vec3(cc, 0.0) + off;
    } else if (m == 1 || m == 2 || m == 8) {
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
    vL = aPos;
    if (m == 1 || m == 2 || m == 8 || m == 12) vAO = 0.4 + 0.6 * smoothstep(-0.8, 0.7, normalize(aPos - aCtr + vec3(0.0, 1e-4, 0.0)).y);
    else if (m == 0) vAO = 0.55 + 0.25 * aMS.y;
    else vAO = 1.0;
    vMat = aMS.x;
    vI1 = i1;
    vI2 = i2;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* PLANT_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
in vec3 vL;
in float vAO;
flat in float vMat;
flat in vec4 vI1, vI2;
out vec4 frag;
uniform vec3 uBark, uLeafA, uLeafB, uGrassDry, uGrassLush, uDead, uFlowerC, uLeafFar, uSoil, uGlow;
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
    if (m == 0) alb = uBark * (0.6 + 0.2 * h2);
    else if (m == 1) { alb = leaf; foliage = true; }
    else if (m == 2) { alb = mix(leaf, uLeafB, 0.5) * 1.1; foliage = true; }
    else if (m == 3) { alb = mix(uGrassLush, mix(uGrassLush, uGrassDry, 0.4), h1) * (0.85 + 0.3 * h2); foliage = true; }
    else if (m == 4) alb = uPieces[type] * (0.9 + 0.2 * h2);
    else if (m == 5) alb = uFlowerC;
    else if (m == 6) { alb = uLeafA * 0.75; foliage = true; }
    else if (m == 7) alb = mix(uDead, uBark, vI1.w);
    else if (m == 9) alb = uSoil * (0.9 + 0.2 * hash13(floor(vP * 14.0)));                   // freshly turned soil
    else if (m == 10) { alb = mix(uGrassLush, vec3(0.85, 0.9, 0.55), 0.45); foliage = true; } // a pale young stem
    else if (m == 11) { alb = mix(uGrassLush, uLeafB, 0.35) * 1.25; foliage = true; }        // fresh seed leaves
    else if (m == 12) { alb = mix(leaf, uGrassLush, 0.55) * 1.1; foliage = true; }           // broad leaves
    else { alb = mix(leaf, uGrassLush, 0.3); foliage = true; }
    // a facet's own small shade (low-poly crowns)
    alb *= 0.92 + 0.16 * hash13(floor(vP * 3.0));
    // withering (a dry season between songs): straw-colored
    alb = mix(alb, uGrassDry * 0.85, clamp(vI2.y * 2.0, 0.0, 1.0) * (foliage ? 1.0 : 0.3));
    // wet after the rain: darker
    alb *= 1.0 - 0.22 * uWet;
    float sh = shadowAt(vP + N * 0.08);
    float ndl = max(dot(N, uSunDir), 0.0);
    float ao = vAO;
    vec3 col = alb * (uAmb * (0.5 + 0.5 * max(N.y, 0.0)) * ao + uSunCol * ndl * sh * mix(1.0, ao, 0.5));
    // leaves glow through: lit from behind, and a little everywhere (the light passing through the crown)
    if (foliage) col += alb * uSunCol * (pow(max(dot(-V, uSunDir), 0.0), 2.0) * 0.5 + 0.08) * sh * ao;
    col += uSunCol * pow(max(dot(reflect(-uSunDir, N), V), 0.0), 30.0) * (0.04 + 0.2 * uWet) * sh;
    // wet: the sky in the leaves and the bark at grazing angles, and the drops on the leaves' tops glinting
    if (uWet > 0.01 && m != 4 && m != 5 && m != 9) {
        float F = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
        col = mix(col, skyFull(reflect(-V, N), false) * 0.75, F * uWet * 0.55);
        if (foliage) {
            float drop = step(0.95, hash13(floor(vL * 26.0) + seed)) * smoothstep(0.1, 0.7, N.y);
            col += (uAmb * 1.6 + uSunCol * 0.5) * drop * uWet * 0.45 * ao;
        }
    }
    col = rich(col);
    // a young plant glowing as it comes up (a sprout most of all)
    col += uGlow * vI2.w * (m == 10 || m == 11 ? 1.0 : foliage ? 0.45 : 0.15);
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




// ---------------------------------------------------------------- the stack's cubes: glass three quarters full of
// seeds of the piece's color (a packed bed of grains, each piece type its own seed), traced through the glass; the air
// above them shows the world through the glass (CELL_HEAD + this + CELL_MAIN, shaders.hpp)
inline const char* CELL_FILL = R"(
const float HI = 0.375;  // the inside's half size (the glass is thin)
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
vec3 fill(vec3 p1, vec3 n1, vec3 D, vec3 tint, float size, float px) {
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
        // the inner glow: the light caught in the bed, brightest near its top
        col += tint * (0.08 + 0.22 * exp(-max(depth, 0.0) * 9.0)) * ao;
        col *= vec3(0.96, 0.98, 0.97);   // through the glass
    } else {
        // the air above the seeds: the world through the glass, darkened and bent, the seeds' glow rising in it
        vec2 off = (d.xy - D.xy) * 1.4;
        col = bgAt(off * size) * 0.75 + tint * (0.05 + 0.1 * smoothstep(HI, gTop, p1.y));
    }
    return col;
}
)";

} // namespace land_shaders

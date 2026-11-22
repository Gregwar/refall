#pragma once
// BIOME's sea, its shaders (see sea.cpp; the shared ones in shaders.hpp): the 3D world (the sky, the sandy bed and its
// dunes, the sea's surface and its cut face, the boats, the life, the thrown cubes and the light through them), the
// water inside the glass cubes.

namespace sea_shaders {

// The world (after SKY_GLSL): the sandy bed, the sea's swell and its water, the haze (sea.hpp's sandH and sea.cpp's waveH
// are the same functions).
inline const char* WORLD_GLSL = R"(
uniform float uSea;
uniform float uWaveT, uAmp, uSharp, uDir, uCaps;
uniform vec3 uHaze, uWater, uDeep, uFoam, uStormSea;
uniform float uAirDust, uWind;   // the dust blowing over the sand (how thick), the wind's way along x (-1, 1)

// the dunes: ridges across the view (a long windward slope, a sharp crest, a short steep lee), low ones rolling out
// from the beach, big ranges far off
float duneRidge(float u) {
    float s = fract(u);
    return s < 0.7 ? pow(s / 0.7, 1.6) : pow((1.0 - s) / 0.3, 1.6);
}
float dunes(vec2 p, float lake) {
    float u1 = -p.y / 17.0 + 0.9 * sin(p.x * 0.045 + 0.7) + 0.35 * sin(p.x * 0.13 - p.y * 0.03 + 2.1);
    float a1 = 1.7 * (0.7 + 0.3 * sin(p.x * 0.06 + p.y * 0.02 + 1.0)) * smoothstep(10.0, 40.0, -p.y) * (1.0 - 0.8 * lake);
    float u2 = -p.y / 55.0 + 1.2 * sin(p.x * 0.018 + 0.3) + 0.4 * sin(p.x * 0.05 + p.y * 0.01 + 4.0);
    float a2 = 4.6 * (0.7 + 0.3 * sin(p.x * 0.021 + 2.4)) * smoothstep(60.0, 170.0, -p.y);
    return a1 * duneRidge(u1) + a2 * duneRidge(u2);
}
// the sandy bed: a beach under the well sloping gently down far out, a lake's hollow just behind the well (the
// lowest sand: the water gathers there first), a gentle swell of sand near, dunes further out
float sandH(vec2 p) {
    float lake = exp(-(p.x * p.x / 576.0 + (p.y + 15.0) * (p.y + 15.0) / 100.0));
    float base = -9.2 - 1.8 * smoothstep(3.0, 48.0, -p.y) - 5.2 * lake + 5.0 * clamp(log(max(-p.y, 30.0) / 30.0) / log(800.0 / 30.0), 0.0, 1.0);
    float k = (1.0 - 0.4 * smoothstep(10.0, 60.0, -p.y)) * (1.0 - 0.8 * lake);
    float d = 0.55 * sin(p.x * 0.21 + 1.3 + p.y * 0.11) + 0.3 * sin(p.x * 0.53 + 0.4 - p.y * 0.27) +
              0.1 * sin(p.x * 1.37 + 2.0 + p.y * 0.61) + 0.35 * sin(p.x * 0.07 - p.y * 0.19 + 0.5);
    return base + d * k + dunes(p, lake);
}
float crestW(float s) {
    float c = s * 0.5 + 0.5;
    return 2.0 * pow(c, 1.0 + 2.5 * uSharp) - 1.0;
}
// behind the well, near the front, the swell is calmer
float calmAt(vec2 p) { return 1.0 - 0.75 * (1.0 - smoothstep(4.5, 6.5, abs(p.x))) * (1.0 - smoothstep(4.0, 10.0, -p.y)); }
// the swell: two long crested waves along the wind, two shorter ones across, a fine chop
float waveH(vec2 p) {
    float A = uAmp * calmAt(p);
    vec2 d1 = normalize(vec2(uDir, 0.42)), d2 = normalize(vec2(uDir * 0.7, -0.7)), d3 = normalize(vec2(uDir * 0.25, 1.0)),
         d4 = normalize(vec2(-uDir, 0.6));
    float t = uWaveT;
    float s1 = crestW(sin(dot(p, d1) * 0.42 - t));
    float s2 = crestW(sin(dot(p, d2) * 0.93 - t * 1.37 + 1.3));
    float s3 = sin(dot(p, d3) * 2.1 - t * 0.8 + 0.9);
    float s4 = sin(dot(p, d4) * 3.7 - t * 1.9 + 2.0);
    return A * (0.55 * s1 + 0.32 * s2 + 0.13 * s3 + 0.06 * s4);
}
float surfY(vec2 p) { return uSea + waveH(p); }
float darkAt(float depth) { return smoothstep(0.5, 10.0, depth); }

// the haze far out: the sky's own color at the horizon, warmed by the dust the wind blows over the sand and lit
// through toward the sun (over the path in the air only: under the water, the water veils it already)
float gAir = -1.0;
float hazeK(float L) { return 1.0 - exp(-max(L - 14.0, 0.0) * (0.003 + 0.0018 * uAirDust)); }
vec3 hazeCol(vec3 d) {
    vec3 h = mix(skyBase(normalize(vec3(d.x, 0.0, d.z))), uHaze, (0.2 + 0.35 * uAirDust) * (1.0 - uStorm));
    float toSun = max(dot(normalize(d), uSunDir), 0.0);
    h += uSunCol * (pow(toSun, 8.0) * 0.32 + pow(toSun, 40.0) * 0.3) * uAirDust * (1.0 - uStorm);
    return mix(h, uStormCloud * 0.9, uStorm * 0.55);
}
// the dust drifting in plumes along the ground with the wind (0 .. 1 at a point of the ground)
float drift(vec2 p) {
    vec2 q = vec2(p.x * 0.035 - uTime * 0.11 * uWind, p.y * 0.07 + 0.3 * sin(p.x * 0.02));
    return smoothstep(0.35, 0.8, fbm(q + vec2(0.0, uTime * 0.012)));
}
vec3 aerial(vec3 c, vec3 P) {
    vec3 d = P - uEye;
    float L = gAir >= 0.0 ? gAir : length(d);
    gAir = -1.0;
    float k = hazeK(L) * (1.0 + 0.25 * uAirDust * (drift(P.xz) - 0.4));
    return mix(c, hazeCol(d), clamp(k * 0.94, 0.0, 0.97));
}

// ---- the water: its color at a depth (light from above scattered in it), the depths darker and darker
vec3 waterCol(float depth) {
    float dk = darkAt(depth);
    vec3 c = mix(uWater, uDeep * 0.2, dk * 0.92);
    return mix(c, uStormSea * 1.35, uStorm * 0.6 * (1.0 - dk));
}
float gT = 1.0;   // the last underwater() call: how much of the thing itself shows
// what is under the water, seen through it: absorbed with its depth, veiled by the water along the way (from where
// the view enters it: the diorama's cut face or the surface), long soft shafts of light slanting down from the surface
vec3 underwater(vec3 c, vec3 P) {
    gT = 1.0;
    float sY = surfY(P.xz);
    if (P.y > sY) return c;
    vec3 d = P - uEye;
    float L = length(d);
    d /= L;
    float tIn = L, entryDepth = 0.0;
    float tf = d.z < -1e-4 ? (uFrontZ - uEye.z) / d.z : -1.0;
    float yf = uEye.y + d.y * tf;
    if (tf > 0.0 && yf < uSea) { tIn = tf; entryDepth = uSea - yf; }
    else if (d.y < -1e-4) tIn = (uSea - uEye.y) / d.y;
    tIn = clamp(tIn, 0.0, L);
    gAir = tIn;
    float w = L - tIn;
    float depth = sY - P.y;
    vec3 wn = uWater / max(max(uWater.r, uWater.g), max(uWater.b, 1e-3));
    c *= exp(-depth * ((1.0 - wn) * 0.22 + 0.03));
    float T = exp(-w * 0.16);
    gT = T;
    vec3 wc = waterCol(mix(entryDepth, depth, 0.6));
    float sh = 0.0;
    float sx = clamp(uSunDir.x / max(uSunDir.y, 0.2), -1.5, 1.5) * 0.4 + 0.2 * uDir;
    for (int i = 0; i < 3; i++) {
        vec3 Q = uEye + d * (tIn + min(w, 9.0) * (0.15 + 0.32 * float(i)));
        float dq = max(uSea - Q.y, 0.0);
        float r = vnoise(vec2((Q.x + dq * sx) * 0.8 + uWaveT * 0.04, Q.z * 0.3));
        sh += smoothstep(0.55, 0.9, r) * exp(-dq / 3.0);
    }
    wc += uHaze * 0.1 * sh * (1.0 - uStorm) * (1.0 - 0.8 * uSlow);
    return mix(wc, c, T);
}
// a particle in the world: under the water, then through the air
vec3 partFog(vec3 c, vec3 P) { return aerial(underwater(c, P), P); }
// light focused by the swell on the bed: a fine moving network
float caustic(vec2 p) {
    vec2 q = p * 1.1;
    float a = vnoise(q + vec2(uTime * 0.31, 0.0));
    float b = vnoise(q * 1.37 + vec2(-uTime * 0.23, uTime * 0.19) + 7.0);
    float c1 = pow(1.0 - abs(a - b), 9.0);
    float e = vnoise(q * 2.3 + vec2(uTime * 0.2, -uTime * 0.27) + 3.0);
    float f = vnoise(q * 2.9 - vec2(uTime * 0.29, 0.1) + 11.0);
    return c1 * 0.7 + pow(1.0 - abs(e - f), 12.0) * 0.5;
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
    vec3 c;
    float tf = d.z < -1e-4 ? (uFrontZ - uEye.z) / d.z : -1.0;
    if (tf > 0.0 && uEye.y + d.y * tf < uSea) c = waterCol(uSea - (uEye.y + d.y * tf));   // into the water's cut face
    else if (d.y < 0.0) c = hazeCol(d);   // the sea beyond the meshes, as the haze over the world
    else {
        // the sky (its clouds' layer down to the horizon), the sun's broad glow in the dusty air, the dust's warm band
        // low over the horizon
        c = skyFull(normalize(vec3(d.x, max(d.y, 0.0025), d.z)), true);
        float toSun = max(dot(d, uSunDir), 0.0);
        c += uSunCol * (pow(toSun, 5.0) * 0.18 + pow(toSun, 48.0) * 0.45) * (0.4 + 0.6 * uAirDust) * uSunDisc.w * (1.0 - 0.7 * uSunDisc.z);
        c = mix(c, hazeCol(d), uAirDust * 0.75 * exp(-d.y * 9.0) * (1.0 - uStorm));
    }
    frag = vec4(enc(grade(c)), 1.0);
}
)";

// ---------------------------------------------------------------- the sandy bed and its cut face
inline const char* SAND_VS = R"(#version 330 core
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
inline const char* SAND_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
out vec4 frag;
uniform vec3 uSand;
uniform vec4 uBodies[24];   // the thrown cubes lying near (center, how much of it is there): their contact shadows
uniform int uBodyN;
uniform sampler2D uPass;    // the light passed through the thrown cubes' water, from the sun (the shadow map's view)
uniform sampler2D uPud;     // the water on the sand: its film's depth, the sand's wetness
uniform vec4 uPudRect;      // its grid's corner (x, z), 1 / its extent (x, z)
// the dunes' own shadows: a march toward the sun over the heightfield (soft: how close it passed over the sand)
float duneShadow(vec3 P) {
    P.y = max(P.y, sandH(P.xz) + 0.03);   // (from the true surface: the mesh's facets cut under the crests)
    float t = 0.25, res = 1.0;
    for (int i = 0; i < 26; i++) {
        vec3 Q = P + uSunDir * t;
        if (Q.y > 1.0) break;
        float h = Q.y - sandH(Q.xz);
        res = min(res, 7.0 * h / t);
        if (res < 0.0) break;
        t += max(0.3, t * 0.2);
    }
    return smoothstep(0.0, 1.0, res);
}
// wind ripples: fine ridges across the wind, winding, a gentle slope up to a steeper fall; their slope (x, z), faded
// out where they get finer than the pixels
const vec2 RIP = vec2(0.6, 1.0) * 1.15;
vec2 ripples(vec2 p, float fade) {
    float ph = 6.2832 * (dot(p, RIP) + 1.1 * vnoise(p * 0.22) + 0.4 * sin(p.x * 0.37));
    float g = cos(ph + 0.7 * sin(ph)) * (1.0 + 0.7 * cos(ph));
    float k = 0.034 * fade * (0.55 + 0.45 * vnoise(p * 0.11 + 3.0));
    return RIP * 6.2832 * g * k;
}
void main() {
    vec3 P = vP;
    float dist = length(P - uEye);
    vec3 Ng = normalize(vN);
    if (Ng.z > 0.9 && P.z > uFrontZ - 0.05) {
        // the cut face: layers of packed sand, darker deeper, a lit lip along the top
        float top = sandH(vec2(P.x, uFrontZ));
        float dd = top - P.y;
        float band = vnoise(vec2(P.x * 0.5, P.y * 4.0 + 3.0 * vnoise(vec2(P.x * 0.25, 1.0))));
        vec3 alb = uSand * (0.8 - 0.25 * smoothstep(0.0, 3.0, dd)) * (0.88 + 0.22 * band);
        alb *= 0.97 + 0.06 * hash12(floor(P.xy * 40.0));
        vec3 col = alb * (uAmb * 1.2 + uSand * uSunCol * 0.5 + uSunCol * max(dot(Ng, uSunDir), 0.0) * 0.8);
        col = mix(col, uSand * (uAmb + uSunCol * 0.6), smoothstep(0.07, 0.0, dd) * 0.45);
        frag = vec4(enc(grade(col)), 1.0);
        return;
    }
    // the surface's own slope (sharp dune crests whatever the mesh), and the ripples on it
    float e = max(0.04, dist * 0.0025);
    vec3 N = normalize(vec3(sandH(P.xz - vec2(e, 0.0)) - sandH(P.xz + vec2(e, 0.0)), 2.0 * e,
                            sandH(P.xz - vec2(0.0, e)) - sandH(P.xz + vec2(0.0, e))));
    float sY = surfY(P.xz);
    float under = sY - P.y;
    float dry = smoothstep(-0.05, -0.4, under);
    float ru = dot(P.xz, RIP);
    vec2 rg = ripples(P.xz, (1.0 - smoothstep(0.07, 0.22, fwidth(ru))) * (0.35 + 0.65 * dry) * smoothstep(0.55, 0.85, N.y));
    N = normalize(N - vec3(rg.x, 0.0, rg.y));
    // the sand: grains catching the light near, streaks of drifting sand moving over it
    vec3 alb = uSand;
    float near = exp(-dist / 45.0);
    alb *= 0.94 + 0.12 * (vnoise(P.xz * 1.7) - 0.5) * near + 0.1 * (vnoise(P.xz * 0.09) - 0.5);
    float streak = smoothstep(0.55, 0.9, vnoise(vec2(P.x * 0.12 - uTime * 1.4 * uWind, P.z * 1.5 + 2.0 * vnoise(P.xz * 0.05))));
    alb *= 1.0 + 0.1 * streak * uAirDust * dry;
    // wet sand: darker under the water and in a band above it, and where the cubes' water fell and ran
    vec2 pu = (P.xz - uPudRect.xy) * uPudRect.zw;
    vec2 pud = pu == clamp(pu, 0.0, 1.0) ? texture(uPud, pu).rg : vec2(0.0);
    float film = smoothstep(0.04, 0.09, pud.x) * dry;
    alb *= 1.0 - 0.32 * max(smoothstep(-0.3, 0.0, under), max(pud.y * 0.9, film) * dry);
    float ndl = max(dot(N, uSunDir), 0.0);
    float sh = shadowAt(P + Ng * 0.04);
    if (ndl > 0.0 && under < 0.0) sh *= duneShadow(P);
    // the thrown cubes lying on it: a soft dark under each
    float ao = 1.0;
    for (int i = 0; i < uBodyN; i++) {
        vec3 q = P - uBodies[i].xyz;
        ao *= 1.0 - 0.55 * uBodies[i].w * exp(-dot(q.xz, q.xz) * 3.2) * smoothstep(1.3, 0.3, abs(q.y));
    }
    // the light: the sun (through the cubes' water: tinted), the sky, the warm light bounced off the sunlit sand
    vec4 ls = uLightVP * vec4(P, 1.0);
    vec3 pass = texture(uPass, ls.xy / ls.w * 0.5 + 0.5).rgb * 2.0;   // (stored at half)
    vec3 sun = uSunCol * ndl * (sh + (1.0 - sh) * pass);
    vec3 amb = uAmb * (0.55 + 0.45 * N.y) + uSand * uSunCol * 0.16 * (1.15 - N.y * 0.5);
    vec3 col = alb * (amb * ao + sun * mix(1.0, ao, 0.5));
    // a glint of a grain now and then, toward the sun
    float gl = step(0.995, hash12(floor(P.xz * 60.0))) * pow(max(dot(reflect(normalize(P - uEye), N), uSunDir), 0.0), 6.0);
    col += uSunCol * gl * near * dry * sh * 1.5;
    if (film > 0.0) {
        // a puddle: a film of clear water over the dark wet sand, rippling, the sky and the sun's glints in it, a bright
        // rim of foam where it thins out
        vec2 rq = P.xz * 3.0;
        vec3 Nw = normalize(vec3(vnoise(rq + vec2(uTime * 0.9, 0.0)) - 0.5, 4.0, vnoise(rq * 1.3 - vec2(0.0, uTime * 0.8) + 7.0) - 0.5));
        vec3 V = normalize(uEye - P);
        float F = 0.08 + 0.92 * pow(1.0 - max(dot(Nw, V), 0.0), 5.0);
        vec3 R = reflect(-V, Nw);
        R.y = abs(R.y);
        vec3 wc = col * mix(vec3(1.0), uWater, 0.35) * 0.9 + mix(uWater, vec3(1.0), 0.5) * uAmb * 0.25;
        wc = mix(wc, skyFull(R, false), F);
        wc += uSunCol * pow(max(dot(R, uSunDir), 0.0), 300.0) * 6.0 * sh;
        float rim = smoothstep(0.04, 0.05, pud.x) * smoothstep(0.065, 0.05, pud.x);
        wc = mix(wc, uFoam * (uAmb + uSunCol * 0.7), rim * 0.55);
        col = mix(col, wc, film);
    }
    if (under > 0.0) {
        float ca = caustic(P.xz) * smoothstep(0.0, 0.4, under) * exp(-under * 0.2);
        col += mix(uWater, vec3(1.0), 0.6) * uSunCol * ca * 0.55 * (0.4 + 0.6 * sh) * (1.0 - 0.75 * uStorm);
        col = underwater(col, P);
    } else {
        // the water's edge: a thin line of foam where the swell laps the sand
        float lap = 0.05 + 0.04 * sin(uTime * 1.3 + P.x * 0.7 + P.z * 0.4);
        col = mix(col, uFoam * (uAmb + uSunCol * 0.5), smoothstep(lap + 0.06, lap, -under) * 0.35);
    }
    col = aerial(col, P);
    frag = vec4(enc(grade(col)), 1.0);
}
)";

// ---------------------------------------------------------------- the sea's surface
inline const char* SURF_VS = R"(#version 330 core
layout(location = 0) in vec2 aXZ;
uniform mat4 uVP;
out vec3 vP;
void main() {
    vP = vec3(aXZ.x, surfY(aXZ), aXZ.y);
    gl_Position = uVP * vec4(vP, 1.0);
}
)";
inline const char* SURF_FS = R"(#version 330 core
in vec3 vP;
out vec4 frag;
uniform float uGlint;
void main() {
    vec2 p = vP.xz;
    float dist = length(uEye - vP);
    const float e = 0.06;
    float hx = waveH(p + vec2(e, 0.0)) - waveH(p - vec2(e, 0.0));
    float hz = waveH(p + vec2(0.0, e)) - waveH(p - vec2(0.0, e));
    vec3 N = normalize(vec3(-hx, 2.0 * e, -hz));
    // a fine glittering chop (fading out far away)
    float ch = (0.05 + 0.1 * uStorm + 0.05 * uGlint) * exp(-dist / 70.0);
    vec2 cq = p * vec2(2.2, 3.4);
    N = normalize(N + vec3(vnoise(cq + vec2(uTime * 0.6, 0.0)) - 0.5, 0.0, vnoise(cq * 1.3 - vec2(0.0, uTime * 0.5) + 5.0) - 0.5) * ch * 2.0);
    vec3 V = normalize(uEye - vP);
    float cosI = max(dot(N, V), 0.0);
    float F = 0.02 + 0.98 * pow(1.0 - cosI, 5.0);
    vec3 R = reflect(-V, N);
    R.y = abs(R.y);
    vec3 refl = skyFull(R, false);
    float sd = max(dot(R, uSunDir), 0.0);
    float spec = (pow(sd, 900.0) * 9.0 + pow(sd, 70.0) * 0.35) * uSunDisc.w * (0.4 + 0.6 * uGlint);
    vec3 c = refl;
    float a = F;
    // whitecaps on the crests, foam streaks in a storm, foam where the swell meets the sand
    float A = max(uAmp * calmAt(p), 1e-3);
    float hgt = waveH(p) / A;
    float fn = vnoise(p * vec2(1.4, 2.6) - vec2(uWaveT * uDir * 1.5, 0.0));
    float cap = smoothstep(0.3, 0.85, hgt) * smoothstep(0.35, 0.6, fn) * uCaps;
    float streak = smoothstep(0.66, 0.85, vnoise(p * vec2(0.6, 2.2) - vec2(uWaveT * uDir * 0.8, 0.0))) * uStorm * 0.35;
    float depth = uSea - sandH(p);
    float shore = smoothstep(0.16, 0.0, depth + 0.06 * sin(uTime * 1.3 + p.x * 0.7 + p.y * 0.4)) * (0.35 + 0.3 * vnoise(p * 1.7));
    float foam = clamp(cap + streak + shore, 0.0, 0.9) * calmAt(p);
    vec3 fc = uFoam * (uAmb * 0.8 + uSunCol * 0.6);
    c = mix(c, fc, foam);
    a = mix(a, 1.0, foam);
    // the storm: grey, churned water
    c = mix(c, uStormSea * 1.3, uStorm * 0.35);
    a = mix(a, 1.0, uStorm * 0.3);
    c += uSunCol * spec;
    a = clamp(a + spec * 0.6, 0.0, 1.0);
    c = aerial(c, vP);
    a = mix(a, 1.0, hazeK(dist));
    frag = vec4(enc(grade(c)), a);
}
)";

// ---------------------------------------------------------------- the water's cut face (a fine lit line at its top)
inline const char* FRONT_VS = R"(#version 330 core
layout(location = 0) in vec2 aXT;   // x, 0 bottom / 1 top
uniform mat4 uVP;
out vec3 vP;
out float vTop;
void main() {
    vec2 p = vec2(aXT.x, uFrontZ);
    float bot = sandH(p), top = surfY(p);
    vTop = top;
    vP = vec3(aXT.x, aXT.y > 0.5 ? max(top, bot) : bot, uFrontZ);
    gl_Position = uVP * vec4(vP, 1.0);
}
)";
inline const char* FRONT_FS = R"(#version 330 core
in vec3 vP;
in float vTop;
out vec4 frag;
uniform float uPx;
void main() {
    float d = vTop - vP.y;
    float line = smoothstep(uPx * 2.2, uPx * 0.4, d);
    float sheen = 0.07 * exp(-d / 0.6);
    vec3 c = mix(uHaze, uFoam, 0.5) * (uAmb * 1.2 + uSunCol * 0.4);
    float a = clamp(line * 0.55 + sheen, 0.0, 1.0);
    frag = vec4(enc(grade(c)), a);
}
)";

// ---------------------------------------------------------------- the boats (low-poly meshes, instanced) and the
// shadow casters (the boats and the thrown cubes)
inline const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in float aMat;
layout(location = 3) in vec4 m0;
layout(location = 4) in vec4 m1;
layout(location = 5) in vec4 m2;
layout(location = 6) in vec4 m3;
layout(location = 7) in vec4 aX;    // alpha, seed, -, -
uniform mat4 uVP;
out vec3 vP;
out vec3 vN;
out vec3 vL;
flat out float vMat;
flat out vec4 vX;
void main() {
    mat4 M = mat4(m0, m1, m2, m3);
    vec4 w = M * vec4(aPos, 1.0);
    vP = w.xyz;
    vN = mat3(M) * aNrm;
    vL = aPos;
    vMat = aMat;
    vX = aX;
    gl_Position = uVP * w;
}
)";
inline const char* BOAT_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
in vec3 vL;
flat in float vMat;
flat in vec4 vX;
out vec4 frag;
uniform vec3 uMat[10];
uniform vec3 uLit;   // a lit window's color
void main() {
    // fading in and out: a fine dither
    if (hash12(gl_FragCoord.xy + vX.y) > vX.x) discard;
    int m = int(vMat + 0.5);
    vec3 N = normalize(vN);
    if (dot(N, uEye - vP) < 0.0) N = -N;
    vec3 alb = uMat[m];
    float sh = shadowAt(vP + N * 0.06);
    vec3 col = alb * (uAmb * (0.6 + 0.4 * N.y) + uSunCol * max(dot(N, uSunDir), 0.0) * sh);
    if (m == 4) {
        // a band of windows: panes and frames along the length; at night most of them lit
        float cell = floor(vL.x * 2.6 + floor(vL.y * 2.0) * 0.5);
        float frame = smoothstep(0.32, 0.26, abs(fract(vL.x * 2.6) - 0.5));
        float on = step(0.3, hash12(vec2(cell, vX.y + floor(vL.z * 3.0))));
        col = mix(col * 0.6, col + uLit * 1.4 * on * frame, uNight);
        col += uSunCol * 0.15 * frame * (1.0 - uNight);
    }
    col = underwater(col, vP);
    col = aerial(col, vP);
    frag = vec4(enc(grade(col)), 1.0);
}
)";

// The light through a thrown cube, seen from the sun: its water's color, focused to a bright heart (a lens of water)
// with a fine moving caustic; through clear glass once it is empty.
inline const char* PASS_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
in vec3 vL;
flat in float vMat;
flat in vec4 vX;
out vec4 frag;
uniform float uTime;
void main() {
    vec3 tint = vX.rgb / max(max(vX.r, vX.g), max(vX.b, 1e-3));
    vec3 a = abs(vL) / 0.44;
    float r = max(max(a.x, a.y), a.z) > 0.999 ? length(vL.xz + vL.y * 0.4) / 0.44 : 0.5;
    float focus = exp(-r * r * 7.0);
    float caus = vnoise(vL.xz * 9.0 + vec2(uTime * 0.7, vX.w * 3.0)) * vnoise(vL.zx * 11.0 - vec2(uTime * 0.5, 1.0));
    vec3 c = tint * (0.1 + 0.9 * focus + 1.1 * smoothstep(0.25, 0.5, caus) * (0.3 + focus));
    frag = vec4(mix(vec3(0.45), c, vX.w) * 0.5, 1.0);
}
)";

// ---------------------------------------------------------------- the water inside the glass cubes (the stack's and
// the thrown ones): a rounded box of water, a clear band of glass around it (GLASS_GLSL's cube, shaders.hpp)
inline const char* WATER_GLSL = R"(
const float WT = 0.11;   // the glass's thickness: a clear band of glass around the water on every face
const float RI = 0.02;   // the water's rounded corners
float sdIn(vec3 p) { return sdBox(p, vec3(H - WT), RI) - bul * bulge(p, H - WT); }
vec3 nIn(vec3 p) {
    const vec2 k = vec2(1.0, -1.0) * 0.0015;
    return normalize(k.xyy * sdIn(p + k.xyy) + k.yyx * sdIn(p + k.yyx) + k.yxy * sdIn(p + k.yxy) + k.xxx * sdIn(p + k.xxx));
}
float exitIn(vec3 p, vec3 d) {
    float t = 0.002;
    for (int i = 0; i < 40; i++) {
        float s = -sdIn(p + d * t);
        if (s < 0.0005) break;
        t += max(s, 0.001);
    }
    return t;
}
// what is behind, seen through the clear glass and the water: the water's own hue the deeper through it (Tg, Tw: what
// the glass and the water let through)
vec3 waterThrough(vec3 behind, vec3 tint, float glassLen, float waterLen, out vec3 Tg, out vec3 Tw) {
    vec3 tN = clamp(tint / max(max(tint.r, tint.g), max(tint.b, 1e-3)), vec3(0.02), vec3(1.0));
    Tg = exp(-glassLen * vec3(0.14, 0.05, 0.08));
    Tw = pow(tN, vec3(waterLen * 1.5));
    return behind * Tg * Tw;
}
// from outside the water (in the glass), how far along d to the water (or a miss: beyond tMax)
float minIn;   // how close a ray staying in the glass came to the water (the water's crisp outline)
float enterIn(vec3 p, vec3 d, float tMax) {
    float t = 0.002;
    minIn = 1e9;
    for (int i = 0; i < 40; i++) {
        float s = sdIn(p + d * t);
        minIn = min(minIn, s);
        if (s < 0.0005) return t;
        t += s;
        if (t > tMax) break;
    }
    return 1e9;
}
)";

// ---------------------------------------------------------------- the stack's cubes: solid glass full to the brim
// with water, under pressure, traced through the glass and the water (CELL_HEAD + this + CELL_MAIN, shaders.hpp)
inline const char* CELL_FILL = R"(
vec3 fill(vec3 p1, vec3 n1, vec3 D, vec3 tint, float size, float px) {
    bool lightP = uLightPanel > 0.5;
    float seed = v3.x;
    // ---- in through the glass
    vec3 d = refract(D, n1, 1.0 / 1.5);
    float glassLen = 0.0, waterLen = 0.0;
    bool tir = false;
    vec3 p = p1;
    float tOut = exitOut(p, d);
    float tW = enterIn(p, d, tOut);
    vec3 nBottom = vec3(0.0);
    vec3 pW = vec3(0.0);
    if (tW < tOut) {
        glassLen += tW;
        p += d * tW;
        vec3 n2 = nIn(p);
        vec3 d2 = refract(d, n2, 1.5 / 1.33);
        if (dot(d2, d2) < 0.5) d2 = reflect(d, n2);
        float tE = exitIn(p, d2);
        vec3 p3 = p + d2 * tE;
        waterLen = tE;
        vec3 n3 = nIn(p3);
        nBottom = n3; pW = p3;
        d = refract(d2, -n3, 1.33 / 1.5);
        if (dot(d, d) < 0.5) d = reflect(d2, -n3);
        p = p3;
        tOut = exitOut(p, d);
    }
    glassLen += tOut;
    p += d * tOut;
    vec3 n4 = nOut(p);
    vec3 d4 = refract(d, -n4, 1.5);
    if (dot(d4, d4) < 0.5) { tir = true; d4 = reflect(d, -n4); }
    // ---- what it meets behind: the scene, shifted by the whole path
    const float ZB = 2.2;
    vec3 dd = d4;
    if (tir) dd.z = -abs(dd.z);
    dd.z = min(dd.z, -0.25);
    dd = normalize(dd);
    vec3 q = p + dd * ((-ZB - p.z) / dd.z);
    vec3 q0 = p1 + D * ((-ZB - p1.z) / D.z);
    vec2 off = q.xy - q0.xy;
    off *= min(1.0, 1.1 / max(length(off), 1e-4));
    vec3 behind = bgAt(off * size);
    // caught inside at an edge: it mirrors the cube's own lit water and the sky (bright edges, not dark)
    if (tir) behind = behind * 0.3 + env(d4) * 0.3 + tint * (lightP ? 0.12 : 0.55);
    // ---- the colors along the way: clear glass, the water's own color the deeper through it
    vec3 Tg, Tw;
    vec3 col = waterThrough(behind, tint, glassLen, waterLen, Tg, Tw);
    // the water's own light, scattered in it (brighter toward the top where it comes in, the heart of the cube
    // glowing); on a dark scene it is luminous, well above the night behind it
    float inW = smoothstep(0.0, 0.04, waterLen);
    float lit = (0.8 + 0.45 * smoothstep(-0.4, 0.4, p1.y)) * (1.0 - 0.25 * smoothstep(0.1, 0.42, max(abs(p1.x), abs(p1.y))));
    if (lightP) col = col * 0.8 + tint * (0.3 + 0.6 * waterLen) * lit * inW;
    else col = col * 0.7 + tint * (0.3 + 0.5 * waterLen) * lit * (1.0 + 0.2 * uNight) * inW;
    // light focused through the water onto the cube's bottom: a moving caustic sparkling
    if (nBottom.y < -0.7 && waterLen > 0.0) {
        vec2 cq = pW.xz * 3.2;
        float caus = vnoise(cq + vec2(uTime * 0.3, seed)) * vnoise(cq * 1.3 - vec2(uTime * 0.25, seed * 0.7) + 5.0);
        col += mix(tint, vec3(1.0), 0.65) * (smoothstep(0.15, 0.45, caus) * 0.4 + smoothstep(0.4, 0.6, caus) * 0.6) * Tw * Tg;
    }
    // the water's outline seen through the clear glass
    gInner = waterLen > 0.0 ? 0.0 : smoothstep(px * 1.6, 0.0, minIn);
    return col;
}
)";

// ---------------------------------------------------------------- the thrown cubes: the same glass cube, in the world
// One camera-facing quad around each; every pixel traces its real perspective ray through the cube (turned on all axes),
// writes its depth, and is lit and veiled by the world (the sun, the sky, the water around it).
inline const char* CUBE_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // center, size
layout(location = 2) in vec4 a1;   // orientation (quaternion x, y, z, w)
layout(location = 3) in vec4 a2;   // linear rgb, alpha
layout(location = 4) in vec4 a3;   // the crack's point (the cube's frame), crack
layout(location = 5) in vec4 a4;   // fill, seed, -, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec3 vW;
flat out vec4 v0, v2, v3, v4;
flat out mat3 vR;
void main() {
    vec4 q = a1;
    vR = mat3(1.0 - 2.0 * (q.y * q.y + q.z * q.z), 2.0 * (q.x * q.y + q.w * q.z), 2.0 * (q.x * q.z - q.w * q.y),
              2.0 * (q.x * q.y - q.w * q.z), 1.0 - 2.0 * (q.x * q.x + q.z * q.z), 2.0 * (q.y * q.z + q.w * q.x),
              2.0 * (q.x * q.z + q.w * q.y), 2.0 * (q.y * q.z - q.w * q.x), 1.0 - 2.0 * (q.x * q.x + q.y * q.y));
    v0 = a0; v2 = a2; v3 = a3; v4 = a4;
    float r = a0.w * 0.8;
    vW = a0.xyz + normalize(uEye - a0.xyz) * r + (uCamR * aQ.x + uCamU * aQ.y) * r * 1.3;
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* CUBE_FS = R"(#version 330 core
in vec3 vW;
flat in vec4 v0, v2, v3, v4;
flat in mat3 vR;
out vec4 frag;
uniform sampler2D uBg;
uniform mat4 uVP;
uniform float uLightPanel;
vec3 sunL;
vec3 envW(vec3 r) { return withSun(skyBase(vec3(r.x, abs(r.y), r.z)), r) * (r.y < 0.0 ? 0.6 : 1.0); }
vec3 bgAtW(vec3 P, vec2 uv0) {
    vec4 c = uVP * vec4(P, 1.0);
    vec2 uv = c.xy / max(c.w, 1e-3) * 0.5 + 0.5;
    vec2 o = uv - uv0;
    o *= min(1.0, 0.05 / max(length(o), 1e-5));
    return pow(texture(uBg, clamp(uv0 + o, vec2(0.001), vec2(0.999))).rgb, vec3(2.2));
}
// one ray (world) through the cube: linear color, coverage, the hit's distance along it
vec4 shade(vec3 ro, vec3 rdW, vec2 uv0, float px, out float tHit) {
    float size = v0.w;
    vec3 C = v0.xyz;
    vec3 tint = v2.rgb;
    float crk = v3.w, fill = v4.x, seed = v4.y;
    bool lightP = uLightPanel > 0.5;
    mat3 Rt = transpose(vR);
    vec3 o = Rt * (ro - C) / size;
    vec3 D = Rt * rdW;
    sunL = Rt * uSunDir;
    float pressure = crk > 0.001 ? clamp((fill - 0.5) / 0.5, 0.0, 1.0) : 1.0;
    bul = 0.012 * pressure;
    tHit = 1e9;
    // to the bounding sphere, then along the ray to the glass
    float b = dot(o, D), c = dot(o, o) - 0.81;
    float h = b * b - c;
    if (h < 0.0) return vec4(0.0);
    float t = max(-b - sqrt(h), 0.0), md = 1e9;
    float tEnd = -b + sqrt(h);
    bool hit = false;
    for (int i = 0; i < 48; i++) {
        float s = sdOut(o + D * t);
        md = min(md, s);
        if (s < 0.0004) { hit = true; break; }
        t += s;
        if (t > tEnd) break;
    }
    float cover = hit ? 1.0 : smoothstep(px, 0.0, md);
    if (cover <= 0.0) return vec4(0.0);
    vec3 p1 = o + D * t;
    tHit = t * size;
    vec3 n1 = nOut(p1);
    float cosI = clamp(dot(-D, n1), 0.0, 1.0);
    float F = 0.04 + 0.96 * pow(1.0 - cosI, 5.0);
    vec3 refl = reflect(D, n1);
    vec3 d = refract(D, n1, 1.0 / 1.5);
    float glassLen = 0.0, waterLen = 0.0;
    bool tir = false;
    vec3 p = p1;
    float tOut = exitOut(p, d);
    float tW = enterIn(p, d, tOut);
    bool inBox = tW < tOut;
    if (inBox) {
        glassLen += tW;
        p += d * tW;
        vec3 n2 = nIn(p);
        vec3 d2 = refract(d, n2, 1.5 / 1.33);
        if (dot(d2, d2) < 0.5) d2 = reflect(d, n2);
        float tE = exitIn(p, d2);
        vec3 p3 = p + d2 * tE;
        // the water's level inside (once the pressure is gone), across the cube as it lies now
        float wl = min(1.0, fill / 0.5);
        if (wl >= 0.999) waterLen = tE;
        else {
            vec3 up = Rt * vec3(0.0, 1.0, 0.0);
            float hi = H - WT;
            float ey = (abs(up.x) + abs(up.y) + abs(up.z)) * hi;
            float lvl = -ey + 2.0 * ey * wl;
            float s2 = dot(p, up) - lvl, s3 = dot(p3, up) - lvl;
            float k = (s2 < 0.0 && s3 < 0.0) ? 1.0 : (s2 >= 0.0 && s3 >= 0.0) ? 0.0
                    : clamp((s2 < 0.0 ? -s2 : -s3) / max(abs(s2 - s3), 1e-4), 0.0, 1.0);
            waterLen = tE * (wl > 0.001 ? k : 0.0);
        }
        vec3 n3 = nIn(p3);
        d = refract(d2, -n3, 1.33 / 1.5);
        if (dot(d, d) < 0.5) d = reflect(d2, -n3);
        p = p3;
        tOut = exitOut(p, d);
    }
    glassLen += tOut;
    p += d * tOut;
    vec3 n4 = nOut(p);
    vec3 d4 = refract(d, -n4, 1.5);
    if (dot(d4, d4) < 0.5) { tir = true; d4 = reflect(d, -n4); }
    // ---- behind: the world drawn so far, where the ray leaves the cube (a little further along it)
    vec3 behind = bgAtW(C + vR * (p * size + d4 * 1.6), uv0);
    if (tir) behind = behind * 0.3 + envW(vR * d4) * 0.3 + tint * (lightP ? 0.12 : 0.5);
    vec3 Tg, Tw;
    vec3 col = waterThrough(behind, tint, glassLen, waterLen, Tg, Tw);
    // the water's own light: the sun and the sky scattered in it, brighter toward the top
    float inW = smoothstep(0.0, 0.04, waterLen);
    float top = 0.75 + 0.5 * smoothstep(-0.4, 0.4, (vR * p1).y);
    vec3 light = uAmb * 0.8 + uSunCol * 0.45;
    if (lightP) col = col * 0.85 + tint * light * (0.22 + 0.5 * waterLen) * top * inW;
    else col = col * 0.7 + tint * (0.2 + 0.3 * waterLen) * (0.8 + 0.4 * uNight) * top * inW;
    vec3 rc = envW(vR * refl);
    if (!lightP) rc += mix(uSkyLow, vec3(1.0), 0.6) * 0.4;
    col = mix(col, rc, F);
    float spec = pow(max(dot(refl, sunL), 0.0), 220.0);
    col += uSunCol * spec * 1.4 * uSunDisc.w;
    {
        vec3 a = sort3(abs(p1));
        float edge = smoothstep(max(RO * 0.8, RO * 0.35 + px * 1.6), RO * 0.35, H - a.y);
        float inner = inBox ? 0.0 : smoothstep(px * 1.6, 0.0, minIn) * fill;
        vec3 lc = (lightP ? vec3(1.0) : mix(uSkyLow, vec3(1.0), 0.7)) * clamp(uAmb * 1.4 + uSunCol * 0.5, 0.25, 1.0);
        col = mix(col, lc, clamp(edge * 0.55 + inner * 0.6, 0.0, 1.0));
    }
    // ---- the crack, near the corner that hit first: it shows on each of that corner's faces
    if (crk > 0.001) {
        vec3 ap = abs(p1);
        vec2 f2, c2;
        float sameSide;
        if (ap.x >= ap.y && ap.x >= ap.z) { f2 = p1.yz; c2 = v3.yz; sameSide = step(0.0, p1.x * v3.x); }
        else if (ap.y >= ap.z) { f2 = p1.xz; c2 = v3.xz; sameSide = step(0.0, p1.y * v3.y); }
        else { f2 = p1.xy; c2 = v3.xy; sameSide = step(0.0, p1.z * v3.z); }
        float cr = crackAt(f2, c2, crk, seed, px) * sameSide;
        col = mix(col, vec3(1.0) * clamp(uAmb * 1.5 + uSunCol, 0.4, 1.0), cr * 0.9);
    }
    return vec4(col, cover);
}
void main() {
    vec3 dx = dFdx(vW), dy = dFdy(vW);
    vec3 C = v0.xyz;
    float px = length(dx) / v0.w * length(C - uEye) / max(length(vW - uEye), 1e-3);
    vec4 c0 = uVP * vec4(vW, 1.0);
    vec2 uv0 = c0.xy / c0.w * 0.5 + 0.5;
    const vec2 G[4] = vec2[4](vec2(-0.125, -0.375), vec2(0.375, -0.125), vec2(0.125, 0.375), vec2(-0.375, 0.125));
    vec3 acc = vec3(0.0);
    float a = 0.0, tMin = 1e9;
    for (int i = 0; i < 4; i++) {
        vec3 w = vW + dx * G[i].x + dy * G[i].y;
        float th;
        vec4 r = shade(uEye, normalize(w - uEye), uv0, px, th);
        acc += r.rgb * r.a;
        a += r.a;
        tMin = min(tMin, th);
    }
    if (a <= 0.0 || tMin > 1e8) discard;
    vec3 P = uEye + normalize(vW - uEye) * tMin;
    vec3 col = acc / a;
    col = underwater(col, P);
    col = aerial(col, P);
    vec4 cp = uVP * vec4(P, 1.0);
    gl_FragDepth = clamp(cp.z / cp.w * 0.5 + 0.5, 0.0, 1.0);
    frag = vec4(enc(grade(col)), a * 0.25 * v2.a);
}
)";

// ---------------------------------------------------------------- life in the sea
// The creatures as low-poly meshes in the water (instanced: a model matrix; alpha, seed, animation; color, glow): a
// fish's body bending in a wave running down to its tail, the submarine's screw turning, a jellyfish's bell pulsing
// and its tentacles trailing. Materials: 0 a fish's body, 1 its fins; 2 the submarine's hull, 3 its dark fins, 4 its
// sail, 5 its screw, 6 its headlight; 7 the anglerfish, 8 its fins, 9 its lure's stalk, 10 the lure; 11 a jellyfish's
// bell, 12 its tentacles; 13 the submarine's beam of light.
inline const char* CREATURE_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in float aMat;
layout(location = 3) in vec4 m0;
layout(location = 4) in vec4 m1;
layout(location = 5) in vec4 m2;
layout(location = 6) in vec4 m3;
layout(location = 7) in vec4 aX;    // alpha, seed, animation, -
layout(location = 8) in vec4 aC;    // linear rgb, glow
uniform mat4 uVP;
out vec3 vP;
out vec3 vN;
out vec3 vL;
flat out float vMat;
flat out vec4 vX, vC;
void main() {
    int m = int(aMat + 0.5);
    vec3 p = aPos, n = aNrm;
    float t = aX.z;
    if (m <= 1 || m == 7 || m == 8) {
        // a fish swimming: a wave running down its body, stronger toward the tail (the normals turned with it)
        bool angler = m >= 7;
        float A = angler ? 0.07 : 0.15, k = angler ? 2.0 : 3.2;
        float w = smoothstep(angler ? 0.3 : 0.4, -1.0, p.x) + 0.12;
        float ph = t - p.x * k;
        p.z += A * sin(ph) * w;
        vec2 r = normalize(vec2(1.0, -A * k * cos(ph) * w));
        n.xz = vec2(n.x * r.x - n.z * r.y, n.x * r.y + n.z * r.x);
    } else if (m == 5) {
        // the screw turning about the hull's axis
        float c = cos(t * 6.0), s = sin(t * 6.0);
        mat2 R = mat2(c, s, -s, c);
        p.yz = R * p.yz;
        n.yz = R * n.yz;
    } else if (m == 11 || m == 12) {
        // a jellyfish: the bell contracting (narrower, taller) in slow beats, the tentacles trailing and swaying
        float c = pow(0.5 + 0.5 * sin(t * 3.0), 3.0);
        p.xz *= 1.0 - 0.2 * c * smoothstep(-0.6, 0.2, p.y);
        if (p.y > -0.1) p.y *= 1.0 + 0.14 * c;
        if (m == 12) {
            float d = max(-p.y, 0.0);
            float ph = d * 2.6 - t * 2.0 + aPos.x * 7.0 + aPos.z * 5.0;
            p.x += 0.06 * sin(ph) * d;
            p.z += 0.05 * cos(ph * 0.8 + 1.0) * d;
        }
    }
    mat4 M = mat4(m0, m1, m2, m3);
    vec4 w = M * vec4(p, 1.0);
    vP = w.xyz;
    vN = mat3(M) * n;
    vL = aPos;
    vMat = aMat;
    vX = aX;
    vC = aC;
    gl_Position = uVP * w;
}
)";
inline const char* CREATURE_FS = R"(#version 330 core
in vec3 vP;
in vec3 vN;
in vec3 vL;
flat in float vMat;
flat in vec4 vX, vC;
out vec4 frag;
uniform int uMode;      // 0 solid, 1 translucent (a jellyfish), 2 a beam of light (added)
uniform vec3 uSubLit;   // the submarine's lights
void main() {
    // under the water, behind the cut face only
    float sY = surfY(vP.xz);
    if (vP.y > sY || vP.z > uFrontZ) discard;
    int m = int(vMat + 0.5);
    vec3 V = normalize(uEye - vP);
    vec3 N = normalize(vN);
    if (dot(N, V) < 0.0) N = -N;
    float depth = sY - vP.y;
    if (uMode == 2) {
        // the beam: brightest at the lamp, fading ahead and toward its soft edges
        float ax = clamp(vL.x, 0.0, 1.0);
        float I = exp(-ax * 2.4) * smoothstep(0.0, 0.08, ax) * pow(max(dot(N, V), 0.0), 1.6) * vC.a * 0.3;
        underwater(vec3(0.0), vP);
        frag = vec4(enc(grade(uSubLit)) * I * (0.3 + 0.7 * gT), 0.0);
        return;
    }
    // the light under the water: the sun bent toward the vertical by the surface, playing in caustics over what faces
    // up, the sky's light from above, a little of the water's own from below
    vec3 L = normalize(vec3(uSunDir.x * 0.5, max(uSunDir.y, 0.2) + 0.7, uSunDir.z * 0.5));
    float ca = caustic(vP.xz + vec2(vP.y * 0.3, 0.0)) * smoothstep(-0.3, 0.8, N.y) * exp(-depth * 0.18);
    float sunK = 1.0 - 0.7 * uStorm;
    vec3 key = uSunCol * max(dot(N, L), 0.0) * (0.9 + 1.4 * ca) * sunK * 1.7;
    float hemi = N.y * 0.5 + 0.5;
    vec3 amb = uAmb * (0.12 + 0.88 * hemi * hemi) + uWater * uAmb * 0.2 * max(-N.y, 0.0);
    float fr = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3 alb = vC.rgb;
    if (uMode == 1) {
        // a jellyfish: clear, its rim and edges catching the light, glowing softly of its own
        float a = (m == 12 ? 0.42 : 0.14 + 0.62 * fr) * vX.x;
        vec3 col = alb * (amb + key * 0.8) * 1.2 + alb * vC.a * 0.3 + mix(alb, vec3(1.0), 0.6) * fr * 0.25;
        col = underwater(col, vP);
        frag = vec4(enc(grade(col)) * a, a);
        return;
    }
    // fading in and out: a fine dither
    if (hash12(gl_FragCoord.xy + vX.y) > vX.x) discard;
    vec3 emis = vec3(0.0);
    float spec = 0.15;
    if (m <= 1) {
        // a fish: a dark back over a silver belly, its eye; the fins lighter
        alb = mix(alb * 0.8, mix(alb, vec3(0.9), 0.45) * 1.7, smoothstep(0.12, -0.1, vL.y));
        if (m == 1) alb = mix(alb, vec3(1.0), 0.12) * 0.95;
        float e = length(vec2(vL.x - 0.58, vL.y - 0.04));
        if (m == 0) alb = mix(alb, vec3(0.015), smoothstep(0.062, 0.048, e) * step(0.02, abs(vL.z)));
        spec = m == 0 ? 1.2 : 0.1;
    } else if (m <= 6) {
        // the submarine: the hull's plates, its dark keel, lit portholes along its sides and in its sail
        alb *= 0.45 + 0.8 * smoothstep(-0.15, 0.15, vL.y);
        if (m == 2) {
            alb *= 1.0 - 0.1 * smoothstep(0.02, 0.0, abs(fract(vL.x * 8.0) - 0.5) - 0.47);
            alb = mix(alb, alb * 0.5, smoothstep(-0.07, -0.1, vL.y));
            float lo = 0.0, rim = 0.0;
            for (int i = 0; i < 6; i++) {
                float d = length(vec2(vL.x + 0.5 - float(i) * 0.19, vL.y - 0.03));
                lo = max(lo, smoothstep(0.021, 0.016, d));
                rim = max(rim, smoothstep(0.029, 0.024, d));
            }
            lo *= step(0.06, abs(vL.z));
            rim *= step(0.06, abs(vL.z));
            alb = mix(alb, alb * 0.35, rim);
            emis = uSubLit * 1.6 * lo;
        } else if (m == 3) alb *= 0.45;
        else if (m == 4) {
            float lo = smoothstep(0.02, 0.014, length(vec2(vL.x - 0.28, vL.y - 0.33))) * step(0.02, abs(vL.z));
            emis = uSubLit * 1.1 * lo;
        } else if (m == 5) { alb *= 0.55; spec = 1.4; }
        else { alb = vec3(0.05); emis = mix(uSubLit, vec3(1.0), 0.6) * 3.0; }
        spec = max(spec, 1.0);
    } else if (m <= 10) {
        // the anglerfish: a near-black shape, only its lure lit
        if (m == 8) alb *= 1.3;
        else if (m == 9) alb *= 1.6;
        else if (m == 10) emis = vec3(1.0, 0.92, 0.72) * 2.5;
    }
    vec3 R = reflect(-V, N);
    vec3 col = alb * (amb + key) + uSunCol * sunK * spec * pow(max(dot(R, L), 0.0), 12.0) * (0.6 + 1.5 * ca);
    // the water around it: a soft rim of the water's own light
    col += waterCol(depth) * fr * 0.3;
    col = underwater(col, vP);
    col += emis * (0.3 + 0.7 * gT);
    frag = vec4(enc(grade(col)), 1.0);
}
)";

// The lights of the deep, one quad each facing the view: a siphonophore (a chain of lit beads), a point of light, a
// halo (the glow around a creature's light: a jellyfish, the anglerfish's lure, the submarine's lamp), veiled by the
// water between it and the eye.
inline const char* LIFE_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // x, y, half extents
layout(location = 2) in vec4 a1;   // direction, kind, animation, glow
layout(location = 3) in vec4 a2;   // linear rgb, alpha
layout(location = 4) in vec4 a3;   // glow rgb, seed
layout(location = 5) in vec4 a4;   // z, -
uniform mat4 uVP;
out vec2 vQ;
out vec3 vP;
flat out vec4 v0, v1, v2, v3;
void main() {
    vQ = aQ;
    vP = vec3(a0.xy + aQ * a0.zw, a4.x);
    v0 = a0; v1 = a1; v2 = a2; v3 = a3;
    gl_Position = uVP * vec4(vP, 1.0);
}
)";
inline const char* LIFE_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vP;
flat in vec4 v0, v1, v2, v3;
out vec4 frag;
uniform float uPx;
uniform float uLifeGlow;
void main() {
    int kind = int(v1.y + 0.5);
    float t = v1.z, glowK = v1.w;
    vec2 ext = v0.zw;
    vec2 q = vQ * ext;
    q.x *= v1.x;
    float px = uPx * length(vP - uEye) / length(uEye.z);
    vec3 col = v2.rgb;
    float a = 0.0;
    vec3 glow = v3.rgb;
    float g = 0.0;
    if (kind == 3) {
        float L = ext.x - 0.3;
        float x = clamp(q.x, -L, L);
        float yc = 0.22 * sin(x * 1.3 + t) + 0.1 * sin(x * 2.9 - t * 0.7);
        float thread = abs(q.y - yc);
        float on = step(abs(q.x), L);
        float pitch = 0.16;
        float k = floor(q.x / pitch + 0.5);
        float bx = k * pitch;
        float by = 0.22 * sin(bx * 1.3 + t) + 0.1 * sin(bx * 2.9 - t * 0.7);
        float taper = 1.0 - 0.6 * abs(bx) / L;
        float bead = length(q - vec2(bx, by)) - 0.045 * taper * (0.8 + 0.4 * hash12(vec2(k, v3.w)));
        float beadA = smoothstep(px, -px, bead) * step(abs(bx), L);
        float th = smoothstep(px, 0.0, thread - 0.004) * on;
        a = max(beadA * 0.85, th * 0.35);
        col = mix(col, vec3(1.0), beadA * 0.3);
        g = (exp(-max(bead, 0.0) * 30.0) * 0.4 * step(abs(bx), L) + exp(-thread * 9.0) * 0.08 * on) * glowK;
    } else if (kind == 6) {
        // a halo: a bright core, a wide soft glow
        float r = length(vQ);
        g = (exp(-r * 12.0) * 0.6 + exp(-r * 3.5) * 0.2) * glowK;
    } else {
        // a point of light living in the dark, twinkling at its own slow pace
        float r = length(q);
        a = smoothstep(ext.x * 0.18 + px, ext.x * 0.18 - px, r) * 0.9;
        col = mix(glow, vec3(1.0), 0.4);
        g = exp(-r / (ext.x * 0.3)) * 0.35 * glowK;
    }
    // under the water only
    float cut = smoothstep(0.03, -0.03, vP.y - surfY(vP.xz));
    col = underwater(col, vP);
    float T = gT;
    col = grade(col);
    g *= smoothstep(1.0, 0.6, max(abs(vQ.x), abs(vQ.y))) * uLifeGlow;
    float k = cut * v2.a;
    vec3 outC = enc(col) * a + enc(grade(glow)) * g * (0.3 + 0.7 * T);
    frag = vec4(outC * k, a * k);
}
)";

} // namespace sea_shaders

#pragma once
// GIANT TREE's shaders: the world (sky, ground, bark, leaves, glows, clouds) at a reduced resolution with depth,
// a soft bloom, the stars at full resolution, then the veil behind the well, the seed-glass pieces, the lettering.

namespace gianttree_shaders {

// Shared noise and the world's light (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash1(float n) { return fract(sin(n * 127.1 + 0.37) * 43758.5453); }
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
)";

// The world's light: uniforms shared by the world's shaders (sky color for the haze, the sun, the ambient).
inline const char* LIGHT_GLSL = R"(
uniform vec3 uEye, uSunDir, uSunC, uSunGlow, uSkyC, uZenithC, uHorizonC, uHazeC, uGroundC, uMistC;
uniform float uFogK, uNight, uBridge, uTime;
uniform vec3 uBridgeCol;
// The haze between the eye and a point (meters): thicker low down, tinted toward the sun.
vec3 atmos(vec3 col, vec3 wp) {
    vec3 d = wp - uEye;
    float dist = length(d);
    vec3 v = d / max(dist, 1e-3);
    float hAvg = max(0.0, (wp.y + uEye.y) * 0.5);
    float fog = 1.0 - exp(-dist * uFogK * exp(-hAvg / 420.0));
    vec3 hc = mix(uHazeC, uSunGlow, pow(max(dot(v, uSunDir), 0.0), 6.0) * 0.55);
    col = mix(col, hc, clamp(fog, 0.0, 1.0));
    return mix(col, uBridgeCol, uBridge);
}
vec3 ambient(vec3 n) {
    return mix(uGroundC * 0.6 + uHazeC * 0.15, mix(uHorizonC, uSkyC, 0.5), n.y * 0.5 + 0.5);
}
)";

// ---------------------------------------------------------------- the sky
// A gradient from the horizon to the zenith (the zenith deepening into a starry indigo as the camera looks up the
// grown tree), the sun or the moon with its glow, a nebula for COSMOS, far mountain ridges in the haze at the
// horizon. Alpha: where the stars may shine.
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform float uSunSize, uStars, uHigh, uNebula, uGlow;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec3 d = rayDir(vUv);
    float e = d.y;
    float az = atan(d.x, -d.z);
    vec3 col = mix(uHorizonC, uSkyC, smoothstep(0.0, 0.28, e));
    // looking high up the tree: the sky deepens toward space
    float high = smoothstep(0.15, 0.85, e);
    vec3 zen = mix(uZenithC, uZenithC * 0.35, uHigh * (1.0 - uNight));
    col = mix(col, zen, high * (0.55 + 0.45 * uHigh));
    // the haze along the horizon
    col = mix(col, uHazeC, exp(-max(e, 0.0) * 14.0) * 0.55);
    col = mix(col, uHazeC, smoothstep(0.012, -0.004, e));
    // the sun / the moon
    float sd = dot(d, uSunDir);
    float ang = acos(clamp(sd, -1.0, 1.0));
    float disc = smoothstep(uSunSize * 1.05, uSunSize * 0.92, ang);
    float glow = pow(max(sd, 0.0), 6.0) * 0.35 + pow(max(sd, 0.0), 60.0) * 0.6 + exp(-ang / max(uSunSize * 2.5, 1e-3)) * 0.5;
    vec3 sunCol = uSunC;
    if (uNight > 0.5) {
        // the moon's soft maria
        vec2 q = vec2(dot(d, normalize(cross(uSunDir, vec3(0, 1, 0)))), dot(d, normalize(cross(cross(uSunDir, vec3(0, 1, 0)), uSunDir)))) / uSunSize;
        sunCol *= 1.0 - 0.18 * smoothstep(0.45, 0.7, fbm(q * 1.8 + 3.0));
    }
    col += uSunGlow * glow * (1.0 + 0.6 * uGlow);
    col = mix(col, sunCol * (uNight > 0.5 ? 1.1 : 2.0), disc);
    // COSMOS: a nebula across the sky
    if (uNebula > 0.0) {
        vec2 np = vec2(az * 1.3, e * 2.2);
        float n = fbm(np * 1.6 + 4.0);
        float band = exp(-sq((e - 0.35 - 0.2 * sin(az * 1.3)) / 0.28));
        float neb = smoothstep(0.4, 0.85, n) * band * uNebula;
        col += mix(uSunGlow, uHorizonC * 1.8, fbm(np * 3.0)) * neb * 0.45;
    }
    // far ridges in the haze
    float r1 = 0.016 + 0.03 * fbm(vec2(az * 3.0, 1.0)) + 0.02 * fbm(vec2(az * 11.0, 7.0));
    float r2 = 0.006 + 0.02 * fbm(vec2(az * 5.0 + 9.0, 3.0)) + 0.008 * fbm(vec2(az * 23.0, 2.0));
    float m1 = smoothstep(r1 + 0.002, r1 - 0.002, e), m2 = smoothstep(r2 + 0.002, r2 - 0.002, e);
    vec3 ridge1 = mix(col, mix(uHazeC, uSkyC, 0.35) * 0.92, 0.75);
    vec3 ridge2 = mix(col, mix(uHazeC, uGroundC, 0.3) * 0.85, 0.85);
    col = mix(col, ridge1, m1);
    col = mix(col, ridge2, m2);
    float stars = (uStars + (1.0 - uNight) * uHigh * 0.8) * smoothstep(0.02, 0.4, e) * (1.0 - max(m1, m2)) * (1.0 - disc);
    stars *= 1.0 - clamp(glow * 0.8, 0.0, 1.0);
    col = mix(col, uBridgeCol, uBridge);
    frag = vec4(col, stars * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the ground
// A radial grid around the tree's foot (fine near it, kilometers wide), heights baked: a gentle hill under the tree,
// rolling country, mist in the hollows. The crown's soft shadow along the sun. Little flowers in the near grass.
inline const char* GROUND_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aN;
uniform mat4 uVP;
out vec3 vW;
out vec3 vN;
void main() {
    vW = aPos;
    vN = aN;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* GROUND_FS = R"(#version 330 core
in vec3 vW;
in vec3 vN;
out vec4 frag;
uniform vec4 uCrownW;   // the crown's center and radius (meters)
uniform float uScale;
uniform vec3 uBlossomC, uLeafC, uAmbientC;
void main() {
    vec3 n = normalize(vN);
    float dist = length(vW - uEye);
    // grass at several scales, the finer ones fading with distance
    float g = 0.0;
    float w = 0.0;
    for (int i = 0; i < 4; i++) {
        float sc = pow(9.0, float(i)) * 0.4;
        float k = smoothstep(sc * 300.0, sc * 40.0, dist) * (i == 3 ? 1.0 : 1.0);
        if (i == 3) k = 1.0;
        g += (vnoise(vW.xz / sc + float(i) * 7.0) - 0.5) * k;
        w += k;
    }
    g = g / max(w, 1e-3);
    vec3 col = uGroundC * (1.0 + g * 0.55);
    col = mix(col, uLeafC * 0.55, smoothstep(0.1, 0.35, g) * 0.25);
    // little flowers in the near grass
    {
        float sc = 0.18;
        vec2 cell = floor(vW.xz / sc);
        vec2 o = (cell + 0.25 + 0.5 * hash2(cell)) * sc;
        float h = hash(cell + 3.7);
        float r = length(vW.xz - o) / sc;
        float fl = step(h, 0.025) * smoothstep(0.2, 0.1, r) * smoothstep(14.0, 3.0, dist);
        col = mix(col, mix(uBlossomC, vec3(1.0), 0.3 * hash(cell + 1.1)), fl * 0.8);
    }
    float diff = clamp(dot(n, uSunDir) * 0.5 + 0.5, 0.0, 1.0);
    // the crown's shadow, along the sun
    vec3 toC = uCrownW.xyz - vW;
    float along = dot(toC, uSunDir);
    float perp = length(toC - uSunDir * along);
    float shade = along > 0.0 ? smoothstep(uCrownW.w * 1.0, uCrownW.w * 0.45, perp) : 0.0;
    diff *= 1.0 - 0.6 * shade;
    vec3 lit = col * (uSunC * diff * mix(0.9, 0.35, uNight) + ambient(n) * mix(0.55, 0.9, uNight));
    // the night meadow: a faint moonlit sheen
    lit += col * uAmbientC * uNight * 0.9;
    // mist in the hollows
    float mist = exp(-max(vW.y + 42.0, 0.0) / 14.0) * 0.45 * smoothstep(60.0, 600.0, dist);
    lit = mix(lit, uMistC, mist);
    frag = vec4(atmos(lit, vW), 0.0);
}
)";

// ---------------------------------------------------------------- the bark
// Each vertex: a ring's center and radius, its radial normal, its place along the branch; the branch's base, birth
// and duration. Grown: a branch reaches from its base to the growth front (the rings beyond it gather at the front,
// a pointed growing tip), its radius thickens as it matures, the trunk's foot flares into buttresses. The wind
// bends the tree (more toward its top) and stirs the finer branches.
inline const char* SWAY_GLSL = R"(
uniform float uG, uWind, uWindPhase, uTime, uGust;
vec3 sway(vec3 p, float level, float phase, float along) {
    float h = clamp(p.y / 110.0, 0.0, 1.3);
    vec3 dir = normalize(vec3(1.0, 0.0, 0.35));
    float big = (uWind * 1.2 + uGust * 2.5) * h * h * (0.75 * sin(uWindPhase * 0.55 + p.x * 0.012) + 0.25 * sin(uWindPhase * 1.31 + phase));
    vec3 o = dir * big + dir * uGust * h * h * 2.0;
    float l = level * along;
    o += vec3(sin(uWindPhase * 1.7 + phase * 3.0 + p.y * 0.07), 0.5 * sin(uWindPhase * 2.3 + phase), cos(uWindPhase * 1.9 + phase * 2.0))
         * (uWind + uGust) * 0.22 * l;
    return p + o;
}
)";
inline const char* BARK_VS = R"(#version 330 core
layout(location = 0) in vec4 a0; // ring center, radius
layout(location = 1) in vec4 a1; // radial normal, u along the branch
layout(location = 2) in vec4 a2; // branch base, level
layout(location = 3) in vec4 a3; // birth, duration, distance from the root, sway phase
layout(location = 4) in vec4 a4; // bark u (around), bark v (along), root flag, level
uniform mat4 uVP;
uniform float uScale;
out vec3 vW;
out vec3 vT;
out vec3 vN;
out vec2 vBark;
out float vPath;
out float vMat;
void main() {
    float u = a1.w, tb = a3.x, dur = a3.y, level = a2.w;
    float f = clamp((uG - tb) / dur, 0.0, 1.0);
    vec3 c = a0.xyz;
    float r = a0.w;
    if (u > f + 1e-4) {
        c = mix(a2.xyz, c, f / max(u, 1e-4));
        r = 0.0;
    } else if (f < 0.999) {
        r *= sqrt(clamp((f - u) / 0.14, 0.0, 1.0));
    }
    bool trunkLike = level < 0.5 || a4.z > 0.5;
    float mat = clamp((uG - tb) / (trunkLike ? 0.95 : 0.5), 0.0, 1.0);
    r *= mix(trunkLike ? 0.11 : 0.28, 1.0, pow(mat, 0.8));
    if (level < 0.5) r *= 1.0 + 0.85 * exp(-u * 16.0) * mat;
    r *= 1.0 - smoothstep(a4.w, a4.w + 0.06, uG);
    vec3 p = c + a1.xyz * r;
    p = sway(p, level, a3.w, u);
    vT = p;
    vW = p * uScale;
    vN = a1.xyz;
    vBark = a4.xy;
    vPath = a3.z;
    vMat = mat;
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* BARK_FS = R"(#version 330 core
in vec3 vW;
in vec3 vT;
in vec3 vN;
in vec2 vBark;
in float vPath;
in float vMat;
out vec4 frag;
uniform vec3 uBarkC, uBarkDark, uGlowC, uYoungC;
uniform vec4 uCrown;          // the foliage's center and radius (tree units)
uniform float uLeafGlow, uFlow, uFlowK, uFlowW, uBloom;
void main() {
    vec3 n = normalize(vN);
    vec3 v = normalize(uEye - vW);
    // furrowed bark: long fissures winding up the wood
    vec2 bp = vec2(vBark.x * 1.6, vBark.y * 0.09);
    float ridge = fbm(bp + vec2(0.0, fbm(bp * 0.5) * 1.2));
    float fiss = smoothstep(0.38, 0.56, ridge);
    vec3 alb = mix(uBarkDark, uBarkC, 0.35 + 0.65 * fiss);
    alb *= 0.9 + 0.2 * vnoise(vBark * vec2(3.0, 0.6));
    // young wood: smooth and green
    alb = mix(uYoungC * (0.85 + 0.15 * fiss), alb, smoothstep(0.02, 0.4, vMat));
    // inside the crown, in its shade
    float dc = length(vT - uCrown.xyz) / max(uCrown.w, 1.0);
    float occ = mix(0.4, 1.0, smoothstep(0.25, 1.05, dc));
    float diff = max(dot(n, uSunDir), 0.0);
    vec3 col = alb * (uSunC * diff * mix(0.85, 0.35, uNight) * occ + ambient(n) * mix(0.7, 0.9, uNight) * mix(0.6, 1.0, occ));
    // a rim of light along the silhouette
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    col += uSunGlow * rim * 0.3 * (0.4 + 0.6 * max(dot(-v, uSunDir) * 0.5 + 0.5, 0.0));
    // glowing veins in the fissures (the night trees), the drop's light flowing up from the root
    float vein = (1.0 - fiss) * smoothstep(0.3, 0.1, abs(ridge - 0.42));
    col += uGlowC * vein * uLeafGlow * 0.25 * (0.6 + 0.4 * sin(vBark.y * 0.08 - uTime * 0.7));
    float front = exp(-sq((vPath - uFlow) / uFlowW));
    float trail = smoothstep(uFlow + 2.0, uFlow - uFlowW * 6.0, vPath) * 0.35;
    float fl = (front * 1.8 + trail) * uFlowK;
    col += uGlowC * (fl * (0.35 + 1.2 * vein) + uBloom * 0.25 * vein);
    frag = vec4(atmos(col, vW), 0.0);
}
)";

// ---------------------------------------------------------------- the leaves
// One billboard per cluster: a handful of leaves (and blossoms) drawn in it, cut out (alpha test). Lit as a clump:
// its normal points away from the clump and out of the crown, so the crown reads as soft lit masses; the leaves
// glow through when the eye looks toward the sun. It pops open when born (a slight overshoot), falls when the
// branches growing out of it take over, and every leaf falls at game over.
inline const char* LEAF_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // position, size
layout(location = 2) in vec4 i1; // clump normal, birth
layout(location = 3) in vec4 i2; // death, distance from the root, seed, kind
layout(location = 4) in vec4 i3; // sway weight
uniform mat4 uVP;
uniform float uScale, uShed, uBurst, uMinPx;
uniform vec3 uCamR, uCamU, uEye;
out vec2 vLocal;
out vec3 vW;
out vec3 vT;
out vec3 vN;
flat out vec4 vInfo; // seed, kind, distance from the root, opened
flat out float vPx;  // the cluster's radius on screen (pixels)
void main() {
    float birth = i1.w, death = i2.x;
    float open = clamp((uG - birth) / 0.035, 0.0, 1.0);
    float pop = open * (1.0 + 0.3 * sin(open * 3.14159));
    float fall = clamp((uG - death) / 0.05, 0.0, 1.0);
    float size = i0.w * pop * (1.0 - fall) * (1.0 + 0.12 * uBurst);
    vec3 p = sway(i0.xyz, 3.0 * i3.x, i2.z * 6.28, 1.0);
    // a flutter of its own
    p += vec3(sin(uWindPhase * 2.6 + i2.z * 40.0), cos(uWindPhase * 2.2 + i2.z * 31.0), 0.0) * (uWind + uGust) * 0.18 * i3.x;
    // game over: every leaf falls, spinning down and drifting
    float sh = clamp(uShed * 1.6 - i2.z * 0.6, 0.0, 1.0);
    p += vec3(sh * 25.0 * (i2.z - 0.3), -sh * sh * (p.y + 6.0), sh * 10.0 * sin(i2.z * 30.0));
    size *= 1.0 - sh * 0.5;
    vec3 c = p * uScale;
    float s = size * 0.5 * uScale;
    // a cluster never thinner than a pixel or so
    vPx = s / max(uMinPx * length(c - uEye) / 0.8, 1e-6);
    s = max(s, uMinPx * length(c - uEye)) * step(0.001, size);
    float ang = i2.z * 6.28;
    vec2 q = vec2(cos(ang) * aPos.x - sin(ang) * aPos.y, sin(ang) * aPos.x + cos(ang) * aPos.y);
    vec3 w = c + (uCamR * aPos.x + uCamU * aPos.y) * s;
    vLocal = q;
    vW = w;
    vT = p;
    vN = i1.xyz;
    vInfo = vec4(i2.z, i2.w, i2.y, open * (1.0 - fall));
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* LEAF_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vW;
in vec3 vT;
in vec3 vN;
flat in vec4 vInfo;
flat in float vPx;
out vec4 frag;
uniform vec3 uLeafC, uLeaf2C, uBlossomC, uGlowC, uCamR, uCamU;
uniform vec4 uCrown;
uniform float uBlossom, uLeafGlow, uFlow, uFlowK, uFlowW, uBloom, uShimmer, uShed;
void main() {
    float seed = vInfo.x;
    // the leaves of the cluster: pointed ovals, a few blossoms among them (some clusters more in bloom than
    // others); a small cluster on screen draws bigger leaves, so it stays a mass rather than specks
    float ls = clamp(16.0 / max(vPx, 1.0), 1.0, 1.9);
    float patch = vnoise(vT.xz * 0.05 + vT.y * 0.03) * 0.7 + 0.3 * hash1(seed * 3.3 + 0.7);
    float pFlower = clamp(uBlossom * 2.2 * smoothstep(0.25, 0.75, patch), 0.0, 0.95);
    // soft edges (a pixel wide): the cluster is blended over what is behind it, its depth written where solid
    float pxq = length(fwidth(vLocal)) * 0.75;
    int hit = -1;
    float hitFlower = 0.0, cover = 0.0;
    vec2 hq = vec2(0.0);
    for (int i = 0; i < 9; i++) {
        float fi = float(i);
        vec2 h = hash2(vec2(seed * 91.0 + fi * 7.13, fi * 3.7 + seed));
        vec2 c = (h - 0.5) * 1.2;
        if (dot(c, c) > 0.3) c *= 0.65;
        float a = hash1(seed * 13.0 + fi) * 6.283;
        vec2 q = (vLocal - c) / ls;
        q = vec2(cos(a) * q.x + sin(a) * q.y, -sin(a) * q.x + cos(a) * q.y);
        bool flower = hash1(seed * 7.0 + fi * 1.31) < pFlower;
        float sd;
        if (flower) {
            float r = length(q);
            float th = atan(q.y, q.x);
            sd = r - 0.25 * (0.62 + 0.38 * abs(cos(2.5 * th)));
        } else {
            float x = q.x / 0.4;
            float wdt = 0.17 * pow(max(1.0 - x * x, 0.0), 0.7);
            sd = max(abs(q.y) - wdt, (abs(x) - 1.0) * 0.4);
        }
        float cv = clamp(0.5 - sd * ls / pxq, 0.0, 1.0);
        if (cv > 0.0 && (cv >= 0.5 || cv > cover)) { hit = i; hitFlower = flower ? 1.0 : 0.0; hq = q; }
        cover = max(cover, cv);
    }
    if (vPx < 9.0) {
        // far: one soft mass with a leafy edge
        float r = length(vLocal);
        float th = atan(vLocal.y, vLocal.x);
        float edge = 0.72 + 0.12 * sin(th * 5.0 + seed * 20.0) + 0.08 * sin(th * 9.0 + seed * 7.0);
        float cv = clamp(0.5 - (r - edge * mix(1.0, 0.85, smoothstep(4.0, 9.0, vPx))) / pxq, 0.0, 1.0);
        if (cv > cover) { hit = 0; hitFlower = step(0.5, pFlower); hq = vLocal * 0.3; cover = cv; }
    }
    if (hit < 0 || cover < 0.04) discard;
    float fh = float(hit);
    float lh = hash1(seed * 29.0 + fh * 5.1);
    // this leaf's own tilt
    vec2 tl = hash2(vec2(seed * 17.0, fh)) - 0.5;
    vec3 n = normalize(normalize(vN) + (uCamR * tl.x + uCamU * tl.y) * 0.9);
    vec3 alb = mix(uLeafC, uLeaf2C, lh * 0.7 + 0.3 * vnoise(vT.xz * 0.04)) * (0.85 + 0.3 * hash1(seed * 5.7));
    if (hitFlower > 0.5) {
        alb = mix(uBlossomC, vec3(1.0), 0.25 * lh);
        alb *= mix(0.75, 1.05, smoothstep(0.0, 0.15, length(hq)));
    } else {
        // a vein down the middle
        alb *= 1.0 - 0.15 * exp(-abs(hq.y) * 60.0);
    }
    // inside the crown, in its shade; the underside darker
    float dc = length(vT - uCrown.xyz) / max(uCrown.w, 1.0);
    float occ = mix(0.35, 1.0, smoothstep(0.2, 1.0, dc)) * mix(0.75, 1.0, smoothstep(-0.6, 0.4, n.y));
    float diff = clamp(dot(n, uSunDir) * 0.6 + 0.4, 0.0, 1.0);
    vec3 v = normalize(vW - uEye);
    float through = pow(max(dot(v, uSunDir), 0.0), 3.0) * 0.8;
    vec3 col = alb * (uSunC * diff * occ * mix(0.95, 0.35, uNight) + ambient(n) * mix(0.6, 0.85, uNight) * mix(0.55, 1.0, occ));
    col += alb * uSunGlow * through * (0.5 + 0.5 * occ);
    // night leaves: their own glow, twinkling slowly; a peak's shimmer
    float tw = 0.65 + 0.35 * sin(uTime * (0.6 + lh) + seed * 50.0 + fh);
    col += mix(alb, uGlowC, 0.5) * uLeafGlow * (0.35 + 0.65 * tw) * (0.55 + 0.45 * occ) * 0.8;
    col += uGlowC * uShimmer * pow(max(sin(uTime * 2.3 + seed * 70.0 + fh * 2.0), 0.0), 12.0) * 0.6;
    // the drop's light reaching the leaves, then the whole crown blooming
    float front = exp(-sq((vInfo.z - uFlow) / (uFlowW * 1.5)));
    float lit = smoothstep(uFlow + 4.0, uFlow - 10.0, vInfo.z);
    float nk = 1.0 - 0.55 * uNight; // the night trees glow already: less on top
    col += mix(uGlowC, alb * 2.0, 0.6) * front * 0.8 * uFlowK * nk;
    col *= 1.0 + uBloom * lit * (0.25 + 0.35 * hitFlower) * (0.7 + 0.3 * tw) * nk;
    col += uGlowC * uBloom * lit * 0.06 * (0.5 + tw);
    // falling at game over: browning
    col = mix(col, col * vec3(0.8, 0.6, 0.45), clamp(uShed * 1.5, 0.0, 1.0) * (1.0 - uNight));
    frag = vec4(atmos(col, vW) * cover, cover);
}
)";

// ---------------------------------------------------------------- glows: lanterns, petals, spores
// World billboards (meters), premultiplied: mode 0 an orb of light (additive), 1 a petal (spinning, cut soft),
// 2 a spark of light. Never thinner than a pixel or so.
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, size (meters, radius)
layout(location = 2) in vec4 i1; // rgb, alpha
layout(location = 3) in vec4 i2; // mode, seed, spin, -
uniform mat4 uVP;
uniform vec3 uEye, uCamR, uCamU;
uniform float uMinPx;
out vec2 vLocal;
flat out vec4 vB;
flat out vec4 vC;
out float vDim;
void main() {
    float d = length(i0.xyz - uEye);
    vLocal = aPos;
    vB = i1;
    vC = i2;
    if (i2.x > 4.5) {
        // a bird: a fixed size, fading out below a few pixels
        float px = i0.w / max(uMinPx * d / 0.8, 1e-6);
        vDim = smoothstep(0.8, 2.5, px);
        vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * max(i0.w, uMinPx * d * 1.5);
        gl_Position = uVP * vec4(w, 1.0);
        return;
    }
    if (i2.x > 2.5) {
        // a grass tuft or a wildflower: standing on its foot, facing the camera across
        float px = i0.w / max(uMinPx * d / 0.8, 1e-6);
        vDim = smoothstep(1.5, 5.0, px);
        vec3 side = normalize(vec3(uCamR.x, 0.0, uCamR.z));
        vec3 w = i0.xyz + side * aPos.x * i0.w * 1.2 + vec3(0.0, 1.0, 0.0) * (aPos.y + 1.0) * i0.w;
        gl_Position = uVP * vec4(w, 1.0);
        return;
    }
    float minS = uMinPx * d * (i2.x > 0.5 && i2.x < 1.5 ? 1.6 : 2.2);
    float s = max(i0.w, minS);
    vDim = min(1.0, sq(i0.w / max(minS, 1e-6)) + 0.35);
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y) * s;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vB;
flat in vec4 vC;
in float vDim;
out vec4 frag;
void main() {
    int mode = int(vC.x + 0.5);
    float r = length(vLocal);
    if (mode == 5) {
        // a bird: two wings beating
        float fl = sin(vC.z);
        vec2 q = vLocal;
        float x = abs(q.x);
        float wingY = fl * 0.45 * x - 0.15 * x * x;
        float wing = smoothstep(0.16, 0.05, abs(q.y - wingY)) * step(x, 0.95) * (1.0 - smoothstep(0.7, 0.95, x) * 0.6);
        float body = smoothstep(0.2, 0.1, length(q * vec2(1.0, 1.6)));
        float a = max(wing, body) * vB.a * vDim;
        frag = vec4(vB.rgb * a, a);
        return;
    }
    if (mode >= 3) {
        // blades curving in the wind (vC.z), darker at their foot; a wildflower on the tallest
        float y = vLocal.y * 0.5 + 0.5;
        float cov = 0.0, tip = 0.0;
        for (int i = 0; i < 6; i++) {
            float fi = float(i);
            float h = 0.55 + 0.45 * hash1(vC.y * 31.0 + fi);
            float x0 = (hash1(vC.y * 17.0 + fi * 3.1) - 0.5) * 1.1;
            float t = y / h;
            float bend = (hash1(vC.y * 7.0 + fi) - 0.5) * 0.6 + vC.z;
            float x = x0 + bend * t * t;
            float w = 0.11 * (1.0 - t);
            float inside = step(t, 1.0) * smoothstep(w, w * 0.6, abs(vLocal.x - x));
            if (inside > cov) { cov = inside; tip = t; }
        }
        vec3 c = vB.rgb;
        if (mode == 4) {
            vec2 fc = vec2(vC.z + (hash1(vC.y * 5.0) - 0.5) * 0.3, 0.75);
            float fl = smoothstep(0.24, 0.17, length(vLocal - fc));
            float stem = smoothstep(0.05, 0.02, abs(vLocal.x - fc.x * (y + 0.0) / 1.0 * (y))) * step(vLocal.y, 0.7);
            vec3 gc = vec3(0.25, 0.4, 0.2) * 0.6;
            vec3 col = mix(gc, c, fl);
            float a = max(fl, stem * 0.9) * vB.a * vDim;
            frag = vec4(col * a, a);
            return;
        }
        c *= mix(0.45, 1.15, tip);
        float a = cov * vB.a * vDim;
        frag = vec4(c * a, a);
        return;
    }
    if (mode == 1) {
        float a = vC.z;
        vec2 q = vec2(cos(a) * vLocal.x + sin(a) * vLocal.y, -sin(a) * vLocal.x + cos(a) * vLocal.y);
        float e = length(q / vec2(0.9, 0.5 + 0.35 * abs(sin(vC.z * 0.7))));
        float in_ = smoothstep(1.0, 0.75, e);
        vec3 c = vB.rgb * (0.8 + 0.3 * q.x);
        float al = in_ * vB.a * vDim;
        frag = vec4(c * al, al);
        return;
    }
    float v = mode == 2 ? exp(-r * r * 22.0) + 0.25 * exp(-r * r * 5.0) : exp(-r * r * 14.0) * 1.2 + 0.4 * exp(-r * r * 3.0);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(vB.rgb * v * vB.a * vDim, 0.0);
}
)";

// ---------------------------------------------------------------- clouds
// Soft puffs (world billboards, meters), sorted far to near: a ragged edge, lit from the sun as a ball (bright
// tops, shaded bellies), a silver lining when the sun is behind them.
inline const char* CLOUD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // world position, radius
layout(location = 2) in vec4 i1; // seed, alpha, -, -
uniform mat4 uVP;
uniform vec3 uCamR, uCamU;
out vec2 vLocal;
out vec3 vW;
flat out vec4 vI;
void main() {
    vLocal = aPos;
    vI = i1;
    vec3 w = i0.xyz + (uCamR * aPos.x + uCamU * aPos.y * 0.62) * i0.w;
    vW = w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* CLOUD_FS = R"(#version 330 core
in vec2 vLocal;
in vec3 vW;
flat in vec4 vI;
out vec4 frag;
uniform vec3 uCloudLit, uCloudShade, uCamR, uCamU;
uniform float uDrift;
void main() {
    float r = length(vLocal);
    if (r > 1.0) discard;
    float n = fbm(vLocal * 2.2 + vI.x * 17.0 + vec2(uDrift, 0.0));
    float dens = smoothstep(1.0, 0.35, r + (n - 0.5) * 0.7) * smoothstep(-1.0, -0.2, vLocal.y + (n - 0.5) * 0.4);
    if (dens < 0.12) discard;
    vec3 fwd = normalize(uEye - vW);
    float z = sqrt(max(1.0 - r * r, 0.0));
    vec3 nrm = normalize(uCamR * vLocal.x + uCamU * vLocal.y + fwd * z);
    float lit = clamp(dot(nrm, uSunDir) * 0.55 + 0.45, 0.0, 1.0);
    lit *= 0.75 + 0.25 * n;
    vec3 col = mix(uCloudShade, uCloudLit, lit);
    float back = pow(max(dot(-fwd, uSunDir), 0.0), 4.0);
    col += uSunGlow * back * smoothstep(0.4, 1.0, r) * 0.6;
    col = atmos(col, vW);
    float a = dens * 0.9 * vI.y;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the sea of clouds
// A layer of clouds the tree grows through (a horizontal plane, ray traced per pixel, its depth written so the
// trunk passes through it): billowing tops lit by the sun when seen from above, shaded bellies from below, holes
// showing the land far below; it fades away while the camera passes through it.
inline const char* SEA_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan;
uniform mat4 uVP;
uniform float uCloudY, uDrift, uFade, uCover;
uniform vec3 uCloudLit, uCloudShade;
float dens(vec2 xz) {
    vec2 q = xz + vec2(uDrift, uDrift * 0.3);
    return fbm(q / 1600.0) * 0.75 + vnoise(q / 420.0 + 5.0) * 0.25;
}
void main() {
    vec3 d = normalize(uLook + uCamR * (vUv.x * 2.0 - 1.0) * uTan.x + uCamU * (vUv.y * 2.0 - 1.0) * uTan.y);
    float t = (uCloudY - uEye.y) / d.y;
    if (t <= 0.0 || abs(d.y) < 1e-4) discard;
    vec3 p = uEye + d * t;
    float n = dens(p.xz);
    float cover = smoothstep(0.45 - uCover * 0.3, 0.95 - uCover * 0.3, n);
    if (cover < 0.01) discard;
    bool above = uEye.y > uCloudY;
    float e = 18.0;
    vec2 gr = vec2(dens(p.xz + vec2(e, 0.0)) - n, dens(p.xz + vec2(0.0, e)) - n) / e;
    vec3 nrm = normalize(vec3(-gr.x * 1200.0, 1.0, -gr.y * 1200.0));
    float lit;
    vec3 col;
    if (above) {
        lit = clamp(dot(nrm, uSunDir) * 0.6 + 0.45, 0.0, 1.0) * (0.7 + 0.35 * smoothstep(0.45, 0.9, n));
        col = mix(uCloudShade, uCloudLit, lit);
        float back = pow(max(dot(d, uSunDir), 0.0), 5.0);
        col += uSunGlow * back * (1.0 - cover) * 0.8;
    } else {
        lit = 0.35 + 0.4 * (1.0 - cover) + 0.2 * clamp(dot(-nrm, uSunDir), 0.0, 1.0);
        col = mix(uCloudShade, uCloudLit, lit * 0.8);
    }
    col = atmos(col, p);
    float a = cover * uFade * smoothstep(0.0, 0.015, abs(d.y)) * (above ? 1.0 : exp(-t / 2600.0));
    vec4 clip = uVP * vec4(p, 1.0);
    gl_FragDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- composite: the world upscaled, the bloom, stars
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld, uBloomT;
uniform vec3 uLook, uCamR, uCamU;
uniform vec2 uTan, uRes;
uniform float uBloomK, uTwinkle, uTime;
vec3 rayDir(vec2 uv) { return normalize(uLook + uCamR * (uv.x * 2.0 - 1.0) * uTan.x + uCamU * (uv.y * 2.0 - 1.0) * uTan.y); }
void main() {
    vec4 w = texture(uWorld, vUv);
    vec3 c = w.rgb + texture(uBloomT, vUv).rgb * uBloomK;
    float vis = w.a;
    if (vis > 0.002) {
        vec3 d = rayDir(vUv);
        // stars fixed on the sky (a stereographic grid from below)
        vec2 sp = d.xz / (1.0 + max(d.y, 0.0)) * 260.0 * max(uRes.y / 1080.0, 0.5);
        vec3 st = vec3(0.0);
        for (int L = 0; L < 2; L++) {
            float cell = L == 0 ? 1.0 : 3.1;
            vec2 g = floor(sp / cell);
            float rr = hash(g + float(L) * 31.0);
            if (rr < (L == 0 ? 0.28 : 0.35)) {
                vec2 o = (g + 0.2 + 0.6 * hash2(g + 3.1)) * cell;
                float px = length(dFdx(sp)) + 1e-4;
                vec2 dd = (sp - o) / px;
                float b = pow(hash(g + 7.7), L == 0 ? 2.5 : 1.5);
                float rad = (L == 0 ? 0.6 : 0.95) + 0.5 * b;
                float tw = 1.0 + uTwinkle * sin(uTime * (1.2 + 3.0 * hash(g + 1.3)) + rr * 80.0);
                vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.78), hash(g + 9.1));
                st += tint * exp(-dot(dd, dd) / (rad * rad)) * b * max(tw, 0.0) * (L == 0 ? 0.6 : 1.3);
            }
        }
        c += st * vis;
    }
    frag = vec4(c, 1.0);
}
)";
// The bloom: the bright parts of the world (quarter resolution), then a separable blur.
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(x, y) * uTexel).rgb;
    c /= 9.0;
    float l = max(c.r, max(c.g, c.b));
    frag = vec4(c * max(l - uThreshold, 0.0) / max(l, 1e-3), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.227, 0.194, 0.121, 0.054, 0.016);
    vec3 c = texture(uSrc, vUv).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uSrc, vUv + uDir * float(i) * 1.5).rgb * w[i];
        c += texture(uSrc, vUv - uDir * float(i) * 1.5).rgb * w[i];
    }
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- motes over the board, the gauge
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
    if (mode == 1) {
        // the gauge: a faint ring, the charge a bright arc from the top, clockwise, a leaf at its tip
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ring = exp(-sq((r - 0.8) / max(px * 1.2, 0.012)));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = exp(-sq((r - 0.8) / max(px * 2.5, 0.03))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.6 + 0.4 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    float v = exp(-r * r * 14.0) + 0.3 * exp(-r * r * 3.5);
    if (mode == 2) {
        float rays = exp(-abs(vLocal.x) * 40.0) * exp(-abs(vLocal.y) * 3.0) + exp(-abs(vLocal.y) * 40.0) * exp(-abs(vLocal.x) * 3.0);
        v = exp(-r * r * 30.0) + 0.6 * rays + 0.15 * exp(-r * r * 4.0);
    }
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(c * v * vB.a, 0.0);
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
    float dw = box(w, vec2(0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.12;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: seed glass
// Each piece one shape (its cells merge): translucent glass with a warm seed of light at its heart, fine veins
// like a leaf's running through it, a bright inner rim. The ghost is the rim alone; a cleared cell dissolves into
// light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, shine, drain
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
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
uniform float uTime;
uniform vec3 uLightC;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.16, rc = 0.06;
    vec2 q = abs(p);
    bool hx = p.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = p.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = p.x < 0.0 ? (p.y < 0.0 ? 4 : 6) : (p.y < 0.0 ? 5 : 7);
    float c = 0.5 - g;
    if (!hx && !vy) { vec2 k = q - (c - r); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - r; }
    if (hx && !vy) return q.y - c;
    if (!hx && vy) return q.x - c;
    if (has(m, db)) return -1.0;
    return rc - length(max(vec2(c + rc) - q, 0.0));
}

void main() {
    int mode = int(vC.x + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    int mk = int(vA.w + 0.5);
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float sd = shapeSdf(vLocal, mk, 0.05);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    float rimW = max(aa * 1.6, 0.035);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.6);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.08) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // glass: deeper toward the middle of the shape, a warm seed of light glowing at the heart of each cell
    vec3 col = tint * mix(0.95, 0.42, smoothstep(0.02, 0.38, e));
    float seed = smoothstep(0.12, 0.6, e) * (0.75 + 0.25 * vnoise(vWorld * 1.3 + uTime * 0.15));
    col += mix(tint, uLightC, 0.35) * seed * 0.4;
    // fine veins, like a leaf's, running through the glass
    vec2 wv = vWorld * 2.2;
    float vn = abs(vnoise(wv + vec2(0.0, vnoise(wv * 0.7) * 2.0)) - 0.5);
    col += hi * smoothstep(0.035, 0.0, vn) * 0.12 * smoothstep(0.04, 0.2, e);
    // a slow sheen drifting across the stack
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.x * 0.5 - vWorld.y * 0.85) * 0.7 - uTime * 0.3)), 10.0);
    col += uLightC * sheen * 0.14 * smoothstep(0.0, 0.25, e);
    col += tint * exp(-abs(e - 0.1) / 0.025) * 0.2;
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        float n = vnoise(vWorld * 2.3) * 0.65 + vnoise(vWorld * 5.1) * 0.35;
        keep = smoothstep(erase * 1.4 - 0.45, erase * 1.4 - 0.05, n) * (1.0 - smoothstep(0.55, 1.0, erase));
        col = mix(col, hi * 1.35, smoothstep(0.0, 0.4, erase));
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * 0.97 * vB.a * keep;
    frag = vec4(col * a, a);
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
uniform float uAlpha, uHaloA, uGlowA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uHaloA;
    float glow = smoothstep(0.15, 0.47, d) * uGlowA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill) + uInk * glow * (1.0 - fill) * 0.35;
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
    // a soft vignette, a soft roll-off of the brightest lights
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.22 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace gianttree_shaders

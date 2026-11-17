#pragma once
// BIOME's shared shaders (see biome.hpp): the helpers, the sky and its light, the sun's shadow map, the glass cube
// (the stack's cells, with each world's fill inside), the particles, the rain, the well's pane, the
// HUD's letters and the final pass. Each world adds its own (sea_shaders.hpp, land_shaders.hpp): its WORLD_GLSL (its
// ground, its haze, `partFog`), its sky's main, its meshes, its cells' fill.

namespace biome_shaders {

// Helpers, inserted after each shader's #version line.
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

// The sky and its light (Ctx::skyUniforms). World units are board units; the board plane is z = 0, the diorama is cut
// at z = uFrontZ behind it.
inline const char* SKY_GLSL = R"(
uniform vec3 uEye;
uniform float uFrontZ, uTime, uSlow, uNight, uFlash, uCloudT, uStars, uStorm;
uniform vec3 uSunDir, uSunCol, uAmb;   // toward the sun (or moon); its light; the sky's light
uniform vec4 uSunDisc;                 // angular radius, -, moon (0 / 1), how much of it shows (the storm hides it)
uniform vec3 uSkyTop, uSkyLow, uDust, uDusk, uStormSky, uStormCloud, uCloud, uFlashDir;
uniform float uSkyH, uDustK, uCloudCov;   // the gradient's height, the dust's veil, the clouds' cover
uniform vec2 uCloudBand;                  // where the clouds start above the horizon

vec3 skyBase(vec3 d) {
    float h = clamp(d.y / uSkyH, 0.0, 1.0);
    vec3 sky = mix(uSkyLow, uSkyTop, smoothstep(0.0, 1.0, pow(h, 0.7)));
    float l = dot(sky, vec3(0.3, 0.5, 0.2));
    sky = mix(sky, mix(vec3(l), uDust, 0.5) * 1.05, uDustK);
    return mix(sky, mix(uStormSky * 1.12, uStormSky * 0.65, h), uStorm * 0.83);
}
// the sun or the moon and its halo, veiled by the storm
vec3 withSun(vec3 sky, vec3 d) {
    float ang = acos(clamp(dot(d, uSunDir), -1.0, 1.0));
    float R = uSunDisc.x;
    float disc = smoothstep(R * 1.06, R * 0.94, ang);
    float halo = exp(-max(ang - R, 0.0) / (R * (2.0 + 0.6 * (1.0 - uSunDisc.z)))) * (0.25 + 0.1 * (1.0 - uSunDisc.z));
    sky = mix(sky, uSunCol, halo * uSunDisc.w);
    return mix(sky, uSunCol, disc * uSunDisc.w);
}
vec3 skyFull(vec3 d, bool stars) {
    vec3 sky = skyBase(d);
    float st = uStars * (1.0 - uStorm);
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
    // long soft clouds on a layer high above, drifting with the wind; heavier and darker in a storm
    if (d.y > 0.002) {
        vec2 q = d.xz / d.y * 40.0;
        vec2 cp = vec2(q.x * 0.017 - uCloudT * 2.0, q.y * 0.048);
        float n = fbm(cp + vec2(0.0, 3.0)) + 0.25 * fbm(cp * 2.3 + 9.0);
        float band = smoothstep(uCloudBand.x, uCloudBand.y, d.y);
        float cl = smoothstep(1.05 - uCloudCov * 0.55, 1.25 - uCloudCov * 0.4, n) * band;
        vec3 cc = mix(uCloud, uStormCloud * (0.8 + 0.4 * smoothstep(0.8, 1.4, n)), uStorm);
        // lightning: a glow inside the clouds, far out
        float fa = acos(clamp(dot(d, uFlashDir), -1.0, 1.0));
        cc += vec3(0.85, 0.9, 1.0) * uFlash * exp(-fa * 8.5) * 1.25;
        sky = mix(sky, cc, cl * mix(0.52, 0.86, uStorm));
        // the storm's overcast down to the horizon
        sky = mix(sky, uStormCloud * 0.92, uStorm * 0.52 * (1.0 - band));
        sky += vec3(0.6, 0.65, 0.8) * uFlash * 0.09 * exp(-fa * 3.0) * band;
    }
    return sky;
}
// slow motion: dusk
vec3 grade(vec3 c) { return mix(c, uDusk * (0.45 + 1.45 * dot(c, vec3(0.3, 0.5, 0.2))), uSlow * 0.71); }
)";

// The sun's shadow map.
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
        for (int x = -1; x <= 1; x++) acc += texture(uShadow, vec3(s.xy + vec2(x, y) * tx * 1.5, s.z - 0.0013));
    return mix(1.0, acc / 9.0, uShadowK);
}
)";

// A depth-only fragment (the shadow map).
inline const char* DEPTH_FS = R"(#version 330 core
void main() {}
)";

// ---------------------------------------------------------------- the glass cube: a rounded box of clear glass (half
// size H for a cube of size 1), bulging a little under pressure; its edges, a crack
inline const char* GLASS_GLSL = R"(
const float H = 0.44;    // the cube's half size (a gap of 0.12 between neighbours)
const float RO = 0.06;   // its rounded edges
float bul;               // the bulge under pressure
float sdBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}
float bulge(vec3 p, float h) {
    vec3 a = clamp(abs(p) / h, 0.0, 1.0);
    vec3 c = 1.0 - a * a;
    return c.x * c.y + c.y * c.z + c.x * c.z;
}
float sdOut(vec3 p) { return sdBox(p, vec3(H), RO) - bul * bulge(p, H); }
vec3 nOut(vec3 p) {
    const vec2 k = vec2(1.0, -1.0) * 0.0015;
    return normalize(k.xyy * sdOut(p + k.xyy) + k.yyx * sdOut(p + k.yyx) + k.yxy * sdOut(p + k.yxy) + k.xxx * sdOut(p + k.xxx));
}
// from inside the cube, how far along d to its surface
float exitOut(vec3 p, vec3 d) {
    float t = 0.002;
    for (int i = 0; i < 40; i++) {
        float s = -sdOut(p + d * t);
        if (s < 0.0005) break;
        t += max(s, 0.001);
    }
    return t;
}
// one crack: a few jagged lines from its point, growing with `crk` (0..1), a small star of crushed glass at the point
float crackAt(vec2 p, vec2 o, float crk, float seed, float px) {
    vec2 q = p - o;
    float r = length(q);
    float ang = atan(q.y, q.x);
    const float n = 5.0;
    float k = floor(ang / 6.2832 * n + 0.5);
    float aj = (k + 0.4 * (hash12(vec2(k, seed)) - 0.5)) / n * 6.2832;
    float wob = 0.03 * (vnoise(vec2(r * 10.0, k * 3.7 + seed)) - 0.5) * 2.0;
    vec2 dir = vec2(cos(aj), sin(aj));
    float along = dot(q, dir);
    float off = abs(q.x * dir.y - q.y * dir.x - wob * smoothstep(0.0, 0.08, r));
    float len = crk * (0.12 + 0.22 * hash12(vec2(k, seed + 3.0)));
    float line = smoothstep(px * 1.4, 0.0, off) * step(0.0, along) * smoothstep(len, len - 0.03, along);
    float star = smoothstep(0.035 * crk + px, 0.0, r) * 0.8;
    return clamp(max(line, star), 0.0, 1.0);
}
vec3 sort3(vec3 a) {
    float lo = min(a.x, min(a.y, a.z)), hi = max(a.x, max(a.y, a.z));
    return vec3(lo, a.x + a.y + a.z - lo - hi, hi);
}
)";

// ---------------------------------------------------------------- the stack's cubes (at the board plane)
// Every cell is its own small glass cube, seen in a slight three-quarter view from above the well's middle (an oblique
// ray from each pixel of its front face), drawn over the world: the stack stays a clean grid at the layout's pixels.
// What fills it is the world's (CELL_HEAD + the world's fill + CELL_MAIN): `vec3 fill(p1, n1, D, tint, size, px)`, the
// color seen through the front face at p1, `gInner` its crisp inner outline.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aQ;
layout(location = 1) in vec4 a0;   // x, y, size, piece type
layout(location = 2) in vec4 a1;   // linear rgb (its fill's color), alpha
layout(location = 3) in vec4 a2;   // ghost, crack, -, -
layout(location = 4) in vec4 a3;   // seed, -, the crack's point (the front face's frame)
uniform mat4 uVP;
uniform float uTiltY;
out vec2 vP;
flat out vec4 v0, v1, v2, v3;
flat out vec3 vD;
void main() {
    vP = aQ * 0.75;                       // the quad covers the front face and the top and side behind it
    v0 = a0; v1 = a1; v2 = a2; v3 = a3;
    vec2 w = a0.xy + vP * a0.z;
    // the view: from above the well's middle, a little from above, more from the side the farther out
    vD = normalize(vec3(clamp(a0.x * 0.045, -0.3, 0.3), -uTiltY, -1.0));
    gl_Position = uVP * vec4(w, 0.0, 1.0);
}
)";
inline const char* CELL_HEAD = R"(#version 330 core
in vec2 vP;
flat in vec4 v0, v1, v2, v3;
flat in vec3 vD;
out vec4 frag;
uniform sampler2D uBg;          // the scene behind (encoded)
uniform vec2 uRes, uSunP;
uniform float uPx, uTime, uBridge, uLightPanel, uNight, uBulge;
uniform vec4 uSurf;             // the reflection's lift, Fresnel's weight, the highlight, the edges
uniform vec3 uBridgeInk, uSkyTop, uSkyLow, uSunCol, uEnvLow;
vec3 bgAt(vec2 offBoard) {
    vec2 uv = (gl_FragCoord.xy + offBoard / uPx) / uRes;
    return pow(texture(uBg, clamp(uv, vec2(0.001), vec2(0.999))).rgb, vec3(2.2));
}
vec3 sunDir;
// the sky (and the world below the horizon) seen in a direction
vec3 env(vec3 r) {
    vec3 sky = mix(uSkyLow, uSkyTop, smoothstep(0.0, 0.7, r.y));
    vec3 low = mix(uSkyLow * 0.7, uEnvLow, smoothstep(0.0, -0.4, r.y));
    return (r.y > 0.0 ? sky : low) + uSunCol * pow(max(dot(r, sunDir), 0.0), 900.0) * 6.0;
}
float gInner = 0.0;
)";
inline const char* CELL_MAIN = R"(
// one sample of the cube at sP (the front face's frame): linear color, coverage
vec4 shade(vec2 sP) {
    float size = v0.z;
    float px = uPx / size;
    bool lightP = uLightPanel > 0.5;
    vec3 tint = v1.rgb;
    vec3 D = vD;
    sunDir = normalize(vec3(uSunP - v0.xy, 9.0));
    bul = uBulge;
    // ---- the ray: the pixel is where it crosses the front face's plane
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
        // the ghost: an empty glass cube, its edges bright, a faint tint of its fill's color inside
        vec3 a = sort3(abs(p1));
        float edge = smoothstep(px * 2.5 + RO * 0.5, RO * 0.3, H - a.y);
        vec3 ec = mix(tint, vec3(1.0), 0.6);
        return vec4(mix(ec, uBridgeInk, uBridge), (edge * 0.8 + 0.12) * cover * v1.a);
    }
    float cosI = clamp(dot(-D, n1), 0.0, 1.0);
    float F = 0.04 + 0.96 * pow(1.0 - cosI, 5.0);
    vec3 refl = reflect(D, n1);
    gInner = 0.0;
    vec3 col = fill(p1, n1, D, tint, size, px);
    // ---- the surface: the sky reflected (Fresnel), the sun's sharp highlight
    vec3 rc = env(refl) + mix(uSkyLow, vec3(1.0), 0.6) * uSurf.x;
    col = mix(col, rc, F * uSurf.y);
    col += uSunCol * pow(max(dot(refl, sunDir), 0.0), 220.0) * uSurf.z;
    // ---- crisp bright lines: the glass's outer edges, and the fill's outline seen through the clear glass
    {
        vec3 a = sort3(abs(p1));
        float edge = smoothstep(max(RO * 0.8, RO * 0.35 + px * 1.6), RO * 0.35, H - a.y);
        vec3 lc = lightP ? vec3(1.0) : mix(uSkyLow, vec3(1.0), 0.7);
        col = mix(col, lc, clamp(edge * uSurf.w + gInner * (lightP ? 0.6 : 0.7), 0.0, 1.0));
    }
    // ---- a crack (game over: the stack cracks before it leaves)
    float crk = v2.y;
    if (crk > 0.001 && n1.z > 0.5) {
        float c = crackAt(p1.xy, v3.zw, crk, v3.x, px);
        float cd = crackAt(p1.xy + vec2(px * 1.4, -px * 1.4), v3.zw, crk, v3.x, px);
        col = mix(col, col * 0.4, cd * 0.55 * (1.0 - c));
        col = mix(col, vec3(1.0), c * 0.95);
    }
    col = mix(col, uBridgeInk, uBridge);
    return vec4(col, cover * v1.a);
}
// four samples on a rotated grid: smooth silhouettes, face boundaries and the edge lines
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

// ---------------------------------------------------------------- particles: camera-facing quads in the world
// (stretched along their motion), veiled like the world around them by the world's `partFog`
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
// kinds: 0 a soft drop (softness in a3.y), 1 a seed (an oval grain turning), 2 a glint of broken glass, 3 a bubble,
// 4 a drop of water in the light, 5 a soft puff (dust, a plume)
inline const char* PART_FS = R"(#version 330 core
in vec2 vQ;
in vec3 vP;
flat in vec4 v1, v2, v3;
flat in float vS;
out vec4 frag;
uniform float uPx, uAerial;
void main() {
    int kind = int(v1.w + 0.5);
    float r = length(vQ);
    float a;
    vec3 col = v2.rgb;
    float pxl = uPx * length(vP - uEye) / abs(uEye.z) / max(vS, 1e-3);
    if (kind == 1) {
        // a seed: an oval grain turning in the air, lit from the sun's side, a fine dark outline (it reads over a pale sky)
        float an = v3.w;
        vec2 q = vec2(cos(an) * vQ.x + sin(an) * vQ.y, -sin(an) * vQ.x + cos(an) * vQ.y);
        float e = length(q / vec2(0.95, 0.55));
        a = smoothstep(1.0 + pxl, 1.0 - pxl, e);
        vec3 n = normalize(vec3(q / vec2(0.95, 0.55) * 0.9, sqrt(max(1.0 - e * e, 0.0)) + 0.2));
        float lit = 0.55 + 0.45 * dot(n, normalize(vec3(-0.4, 0.6, 0.7)));
        col = col * lit + vec3(1.0) * pow(max(dot(n, normalize(vec3(-0.3, 0.5, 0.8))), 0.0), 18.0) * 0.5;
        col *= mix(0.55, 1.0, smoothstep(0.75, 0.55, e));
    } else if (kind == 2) {
        // a glint of broken glass: a small bright splinter
        float an = v3.w;
        vec2 q = vec2(cos(an) * vQ.x + sin(an) * vQ.y, -sin(an) * vQ.x + cos(an) * vQ.y);
        a = smoothstep(0.45, 0.25, abs(q.x) * 0.35 + abs(q.y));
    } else if (kind == 3) {
        a = smoothstep(pxl * 1.5, 0.0, abs(r - 0.82)) * 0.9 + smoothstep(1.0, 0.0, r) * 0.15;
        a = max(a, smoothstep(0.3, 0.0, length(vQ - vec2(-0.35, 0.35))));
    } else if (kind == 4) {
        // a drop of water in the light: a clear bead, its rim darker, a bright glint and a soft white heart
        float rim = smoothstep(0.45, 1.0, r);
        a = smoothstep(1.0, 1.0 - max(pxl * 1.5, 0.2), r) * (0.6 + 0.4 * rim);
        float hl = smoothstep(0.4, 0.0, length(vQ - vec2(0.28, 0.32)));
        col = mix(col, col * 0.55, rim * 0.6) + vec3(1.0) * hl * 1.2;
    } else if (kind == 5) {
        a = exp(-r * r * 3.0) * smoothstep(1.0, 0.8, r);
    } else {
        a = smoothstep(1.0, 1.0 - max(v3.y, 0.05), r);
    }
    if (a <= 0.003) discard;
    if (uAerial > 0.5) col = partFog(col, vP);
    frag = vec4(enc(grade(col)), a * v2.a);
}
)";

// ---------------------------------------------------------------- the rain: long thin streaks slanting across the
// world in a storm (never on the well; lighter over the HUD's columns)
inline const char* RAIN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;   // the board plane: center x, y, half extents x, y
uniform float uPx, uRainA, uTimeR, uSlant, uFloorY;
uniform vec3 uRainCol;
float layer(vec2 w, float scale, float speed, float seed) {
    float an = 0.18 * uSlant;
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
    a *= uRainA * 0.45 * hudCol * (1.0 - inWell) * smoothstep(-0.2, 0.3, w.y - uFloorY);
    frag = vec4(enc(uRainCol), a);
}
)";

// ---------------------------------------------------------------- the well's pane and the bridge
// The well: a calm, even pane over the world behind it (more opaque over the sea's dark depths), with a fine edge;
// and, while a change of universe runs, the whole world fading to the bridge's color (premultiplied).
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform float uPx, uPanelA, uEdgeA, uDeepY, uBridge;
uniform vec3 uPanel, uEdge, uBridgeCol;
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    vec2 wd = abs(w) - vec2(5.08, 10.08);
    float wellD = length(max(wd + 0.25, 0.0)) + min(max(wd.x + 0.25, wd.y + 0.25), 0.0) - 0.25;
    float inWell = smoothstep(uPx, -uPx, wellD);
    float wellE = smoothstep(uPx * 1.5, 0.0, abs(wellD + uPx));
    float k = mix(uPanelA, min(0.88, uPanelA * 1.45), smoothstep(2.0, 10.0, uDeepY - w.y));
    float a1 = clamp(inWell * k + wellE * uEdgeA * (1.0 - inWell * k), 0.0, 1.0);
    vec3 c1 = mix(uPanel, uEdge, wellE * uEdgeA / max(a1, 1e-3));
    vec3 pre = enc(c1) * a1 * (1.0 - uBridge) + enc(uBridgeCol) * uBridge;
    frag = vec4(pre, 1.0 - (1.0 - a1) * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the lens: what the camera does with the world
// before the stack is laid over it (Ctx::drawLens, called by a world at the end of its drawing). The world (encoded) is taken down to a quarter (`LENS_DOWN_FS`)
// and blurred (`BLUR_FS`: what the frosted glass shows); its brightest light is taken further down and blurred wide
// (the bloom); then `LENS_FS` lays it all back over the world: the bloom, the sun's glare in the lens, the well's
// smoky glass with its fine grid and bright rim, the panels of the held and next pieces in the same glass.
inline const char* LENS_DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;     // the source's texel
uniform float uBright;   // 0: the light as it is; 1: only the brightest (the bloom's source)
void main() {
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        vec2 o = vec2(i & 1, i >> 1) * 2.0 - 1.0;
        c += pow(max(texture(uSrc, vUv + o * uTexel).rgb, vec3(0.0)), vec3(2.2));
    }
    c *= 0.25;
    if (uBright > 0.5) {
        float l = dot(c, vec3(0.3, 0.55, 0.15));
        c *= smoothstep(1.1, 2.4, l) * min(1.0, 2.5 / max(l, 1e-3));   // (a glint's sparkle capped: no blinding blot)
    }
    frag = vec4(c, 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;   // a texel along the blur
void main() {
    const float W[5] = float[5](0.227, 0.194, 0.122, 0.054, 0.016);
    vec3 c = texture(uSrc, vUv).rgb * W[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)).rgb + texture(uSrc, vUv - uDir * float(i)).rgb) * W[i];
    frag = vec4(c, 1.0);
}
)";
inline const char* LENS_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uFrost, uBloom;   // the world (encoded), blurred (linear), its brightest light (linear)
uniform vec4 uView;                         // the board plane: center x, y, half extents x, y
uniform float uPx, uAspect, uWorld, uPane;  // a pixel (board units), the picture's aspect, the world shown, the glass
uniform float uGrid, uPanels;               // how bright the well's grid; the previews' panels (0: none)
uniform vec2 uSunP;                         // the sun on the picture (-1 .. 1, out of it too)
uniform vec4 uGlare;                        // the glare's strength, the bloom's, the glass's tint, the deep's veil
uniform vec3 uGlareCol, uFrostCol, uPanel;
uniform float uDeepY;
float sdRound(vec2 p, vec2 b, float r) {
    vec2 d = abs(p) - b + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}
// a pane of smoky glass (d: its signed distance, board units): darker than whatever is behind it (the world half
// seen through it, blurred, darker still over the deep water), a bright rim, a lighter frosted band inside it
vec3 frosted(vec3 c, vec3 fr, float d, float y, float tintK, float rimK) {
    float inside = smoothstep(uPx, -uPx, d);
    float deep = smoothstep(2.0, 10.0, uDeepY - y) * uGlare.w;
    vec3 tint = mix(uFrostCol, uPanel, deep);
    // what shows through: the world's light held down, so that a white sky or the sun's glint stays dim
    vec3 seen = fr / (1.0 + dot(fr, vec3(0.3, 0.55, 0.15)) * 1.6);
    vec3 g = mix(seen, tint, mix(tintK, min(tintK + 0.08, 0.95), deep));
    g = mix(g, mix(tint, vec3(1.0), 0.5), 0.1 * smoothstep(-0.3, 0.0, d));
    c = mix(c, g, inside);
    float rim = smoothstep(uPx * 1.6, 0.0, abs(d + uPx * 0.8));
    float halo = exp(-max(d, 0.0) / 0.08) * smoothstep(-uPx, uPx, d) * 0.16;
    return c + (rim * 0.6 + halo) * rimK * mix(vec3(1.0), uGlareCol, 0.3);
}
void main() {
    vec3 c = pow(max(texture(uScene, vUv).rgb, vec3(0.0)), vec3(2.2));
    vec3 fr = texture(uFrost, vUv).rgb;
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float glass = 0.0;
    // ---- the glass panes (over the world only: the stack and the previews are laid over them)
    if (uPane > 0.0) {
        float lift = 0.75 + 0.5 * smoothstep(-10.0, 10.0, w.y);
        // the well, a fine grid of its cells etched in it
        float dw = sdRound(w, vec2(5.08, 10.08), 0.25);
        vec3 c0 = c;
        c = frosted(c, fr, dw, w.y, uGlare.z, lift);
        vec2 g = abs(fract(w + 0.5) - 0.5);
        float grid = smoothstep(uPx * 1.2, 0.0, min(g.x, g.y)) * smoothstep(uPx, -uPx, dw + 0.06);
        c = mix(c, mix(uFrostCol, vec3(1.0), 0.6), grid * uGrid);
        // the held piece's panel, the next pieces' panel
        float dh = sdRound(w - vec2(-8.6, 7.2), vec2(1.6, 1.35), 0.22);
        float dn = sdRound(w - vec2(8.6, 4.1), vec2(1.6, 4.45), 0.22);
        float dp = uPanels > 0.5 ? min(dh, dn) : 1e3;
        c = frosted(c, fr, dp, w.y, uGlare.z, 0.8);
        c = mix(c0, c, uPane);
        glass = smoothstep(uPx, -uPx, min(dw, dp)) * uPane;
    }
    // ---- the light in the lens: the bloom, the sun's glare (a broad warm veil, a tighter glow, faint ghosts); held
    // down over the glass (the cubes keep their dark ground)
    float veil = 1.0 - 0.85 * glass;
    c += texture(uBloom, vUv).rgb * uGlare.y * veil;
    vec2 q = (vUv * 2.0 - 1.0) - uSunP;
    q.x *= uAspect;
    float r = length(q);
    vec3 gl = uGlareCol * (exp(-r * 2.4) * 0.3 + exp(-r * 7.0) * 0.5 + exp(-r * 25.0) * 0.9);
    vec2 axis = -uSunP;
    for (int i = 1; i <= 3; i++) {
        vec2 gp = uSunP + axis * (0.55 + 0.45 * float(i));
        vec2 gq = (vUv * 2.0 - 1.0) - gp;
        gq.x *= uAspect;
        float gr = length(gq);
        float sz = 0.05 + 0.04 * float(i);
        gl += uGlareCol * vec3(0.6, 0.85, 1.0) * smoothstep(sz, sz * 0.6, gr) * 0.025 / float(i);
    }
    c += gl * uGlare.x * uWorld * veil;
    frag = vec4(enc(c), 1.0);
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
    c *= 1.0 - 0.11 * dot(q, q) * 2.0;
    c *= 1.0 - uDim * 0.5;
    c *= uFade;
    frag = vec4(c, 1.0);
}
)";

} // namespace biome_shaders

#pragma once
// PLANKS' shaders (see planks.cpp).
//
// Every solid is a plank (or the well's back board, or the plinth): one instanced box, shaded per fragment as wood.
// PLANK_VS / PLANK_FS: a rounded edge from the box's own coordinates (the normal leans out within a small radius of
// every edge, so the planks' arrises catch the light), procedural wood (growth rings around a pith off the plank,
// warped, with fine fibers along the grain: cathedral arches on the wide faces, straight lines on the edges, ring
// arcs and pores on the darker end grain), a stain or paint over it, a soft key light with a wide soft shadow, a sky
// and a bounce from the white table, the screen-space occlusion of the gaps between planks, and a satin sheen.
// PREPASS writes the view-space position and normal of the table and every solid for SSAO (ambient occlusion in the
// gaps and under the constructions); BLUR smooths it, depth aware. STUDIO draws the seamless white sweep (with the
// shadows and the occlusion on it). FINAL tone maps and encodes the frame.
#include <string>

namespace planks_shaders {

// ---------------------------------------------------------------- common: the studio (also seen in reflections)
inline const char* COMMON = R"(
uniform float uSeed;
uniform float uTable, uWall;
uniform vec3 uTableNear, uTableFar, uWallTop, uPoolCol;
uniform vec3 uLightDir, uLightCol;
uniform vec3 uEye;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
vec3 studioAt(vec3 ro, vec3 rd, out vec4 hit) {
    float s;
    vec3 p;
    hit = vec4(0.0, 0.0, 0.0, 2.0);
    float tT = rd.y < -1e-4 ? (uTable - ro.y) / rd.y : 1e9;
    p = ro + rd * tT;
    if (tT < 1e8 && p.z > uWall) {
        s = -p.z;
        hit = vec4(p, 0.0);
    } else {
        float tW = rd.z < -1e-4 ? (uWall - ro.z) / rd.z : 1e9;
        p = ro + rd * tW;
        s = -uWall + max(p.y - uTable, 0.0) * 1.4;
        hit = vec4(p, 1.0);
    }
    float k = clamp(s / (-uWall * 1.15), 0.0, 1.0);
    vec3 c = mix(uTableNear, uTableFar, smoothstep(0.0, 1.0, pow(k, 0.7)));
    float up = clamp((s + uWall) / 140.0, 0.0, 1.0);
    c = mix(c, uWallTop, smoothstep(0.0, 1.0, up));
    vec2 q = vec2(p.x * 0.8, (hit.w < 0.5 ? p.z : uWall - (p.y - uTable)) + 14.0);
    c += uPoolCol * exp(-dot(q, q) / 3000.0);
    return c;
}
vec3 studio(vec3 rd) {
    vec4 h;
    if (rd.z > -0.05 || rd.y > 0.25) {
        vec3 c = mix(uTableNear * 0.8, uWallTop * 1.05, smoothstep(-0.4, 0.6, rd.y));
        c += uLightCol * 0.45 * smoothstep(0.3, 0.95, rd.y);
        return c;
    }
    return studioAt(uEye, rd, h);
}
)";

// ---------------------------------------------------------------- the key light's shadow: two maps, the well's
// close up one where it covers, the village's elsewhere; a rotated spiral of taps (soft, no pattern)
inline const char* SHADOW_GLSL = R"(
uniform mat4 uLightVP, uLightVPn;
uniform sampler2DShadow uShadow, uShadowN;
uniform float uSoftFar, uSoftNear;
float shadowAt(vec3 p, int taps) {
    vec4 ln = uLightVPn * vec4(p, 1.0);
    vec3 sn = ln.xyz / ln.w * 0.5 + 0.5;
    bool nearMap = sn.x > 0.01 && sn.y > 0.01 && sn.x < 0.99 && sn.y < 0.99 && sn.z < 1.0 && sn.z > 0.0;
    vec3 s;
    float r, bias;
    if (nearMap) { s = sn; r = uSoftNear; bias = 0.0008; }
    else {
        vec4 l = uLightVP * vec4(p, 1.0);
        s = l.xyz / l.w * 0.5 + 0.5;
        if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
        r = uSoftFar;
        bias = 0.0006;
    }
    float a = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831;
    float sum = 0.0;
    for (int i = 0; i < 16; i++) {
        if (i >= taps) break;
        float fi = float(i) + 0.5;
        float rr = sqrt(fi / float(taps)) * r;
        float an = a + fi * 2.39996;
        vec2 o = vec2(cos(an), sin(an)) * rr;
        sum += nearMap ? texture(uShadowN, vec3(s.xy + o, s.z - bias)) : texture(uShadow, vec3(s.xy + o, s.z - bias));
    }
    return sum / float(taps);
}
)";

// ---------------------------------------------------------------- the studio (background)
inline const char* STUDIO_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform sampler2D uAO;
uniform float uShadowK, uGrain;
void main() {
    vec4 a = uInvVP * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
    vec4 b = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ro = a.xyz / a.w, rd = normalize(b.xyz / b.w - ro);
    vec4 hit;
    vec3 c = studioAt(ro, rd, hit);
    if (hit.w < 0.5) {
        float sh = shadowAt(hit.xyz, 16);
        float ao = texelFetch(uAO, ivec2(gl_FragCoord.xy), 0).r;
        c *= mix(1.0, 0.6, (1.0 - sh) * uShadowK) * mix(1.0, ao, 0.9);
    }
    c *= 1.0 + (hash(gl_FragCoord.xy * 0.37) - 0.5) * uGrain;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- planks
// Instance: a = center, seed; b = size (length x, width y, thickness z), material (0 wood, 1 board);
// c = tint (linear), tint amount (0 natural .. 1 stained / painted through); d = rotation (quaternion);
// e = stack (bridge mask), glow, fog, grain contrast.
inline const char* PLANK_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iA;
layout(location = 3) in vec4 iB;
layout(location = 4) in vec4 iC;
layout(location = 5) in vec4 iD;
layout(location = 6) in vec4 iE;
uniform mat4 uVP;
uniform mat4 uView;
out vec3 vWorld, vObj, vViewPos;
flat out vec3 vFace, vSize, vTint;
flat out vec4 vQuat, vE;
flat out float vSeed, vMat, vTintK;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
void main() {
    vec3 obj = aPos * iB.xyz;
    vec3 w = iA.xyz + qrot(iD, obj);
    vWorld = w;
    vObj = obj;
    vViewPos = (uView * vec4(w, 1.0)).xyz;
    vFace = aNrm;
    vSize = iB.xyz;
    vMat = iB.w;
    vTint = iC.rgb;
    vTintK = iC.a;
    vQuat = iD;
    vE = iE;
    vSeed = iA.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
void main() {}
)";

// the rounded box normal (object space) and the wood pattern, shared by the main and the prepass
inline const char* PLANK_COMMON = R"(
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
// The normal of a box with rounded edges of radius r, at a point of the box's surface.
vec3 roundedNormal(vec3 p, vec3 face, vec3 size, float r) {
    vec3 h = size * 0.5;
    vec3 q = clamp(p, -h + r, h - r);
    vec3 d = p - q;
    float l = length(d);
    return l > 1e-5 ? d / l : face;
}
float h31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}
float n3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
)";

inline const char* PLANK_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos;
flat in vec3 vFace, vSize, vTint;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vTintK;
out vec4 frag;
uniform sampler2D uAO;
uniform vec3 uAmbTop, uAmbBot, uFillDir, uFillCol;
uniform vec3 uEarly, uLate;      // the natural wood: earlywood, latewood (linear)
uniform float uShadowK, uSheen, uClipZ, uClipY;
void main() {
    // (a plank pushed back into the board vanishes behind it)
    if (vE.x < -0.5 && vE.x > -1.5 && vWorld.z < uClipZ) discard;
    if (vE.x < -1.5 && vWorld.y < uClipY) discard;
    float L = vSize.x;
    float unit = L / 15.0;                 // the plank's thickness unit (1 : 3 : 15)
    float rad = min(min(vSize.y, vSize.z) * 0.4, L * 0.032);
    if (vMat > 0.5) rad = 0.035;
    vec3 n = roundedNormal(vObj, vFace, vSize, rad);
    vec3 N = normalize(qrot(vQuat, n));
    vec3 V = normalize(uEye - vWorld);

    // ---- wood: rings around a pith off the plank (its own per plank), in thickness units
    vec3 p = vObj / unit;
    float s1 = fract(vSeed * 7.13), s2 = fract(vSeed * 13.7), s3 = fract(vSeed * 3.91);
    vec2 pith = vec2((s1 - 0.5) * 7.0, -(4.0 + s2 * 12.0));
    pith += vec2(sin(p.x * 0.09 + s3 * 6.0), cos(p.x * 0.06 + s1 * 5.0)) * 0.7;   // the grain drifting along the length
    float warp = n3(vec3(p.x * 0.1, p.y * 0.22, p.z * 0.22) + s3 * 40.0) * 2.0 - 1.0;
    float rr = length(p.yz - pith) + warp * 0.22;
    float freq = 1.5 + s2 * 0.9;            // rings per thickness unit
    float ringP = rr * freq;
    float fw = fwidth(ringP);
    float ring = fract(ringP);
    // latewood: a soft dark band at the end of each ring, faded where the rings get finer than the pixels
    float late = smoothstep(0.62, 0.9, ring) * (1.0 - smoothstep(0.92, 1.0, ring));
    late *= 1.0 - smoothstep(0.2, 0.6, fw);
    // fine fibers along the grain
    float fib = n3(vec3(p.x * 0.18, p.y * 6.0, p.z * 6.0) + s1 * 30.0);
    float fibA = 1.0 - smoothstep(0.4, 1.2, fwidth(p.y * 6.0) + fwidth(p.z * 6.0));
    float tone = 0.92 + 0.16 * fract(vSeed * 31.7);   // each plank a little lighter or darker
    float grainK = vE.w;
    vec3 wood = mix(uEarly, uLate, late * 0.6 * grainK) * tone;
    wood *= 1.0 + (fib - 0.5) * 0.16 * fibA * grainK;
    // a knot, rarely, on a wide face
    bool endGrain = abs(vFace.x) > 0.5;
    if (endGrain) {
        // end grain: darker, the rings as arcs, fine pores
        float pore = n3(vec3(p.y * 9.0, p.z * 9.0, s2 * 10.0));
        wood *= 0.72 + 0.08 * pore;
        wood = mix(wood, wood * vec3(0.95, 0.88, 0.8), 0.5);
    }
    // stain / paint: a translucent color that keeps the grain
    float lum = dot(wood, vec3(0.3, 0.55, 0.15)) / max(dot(uEarly, vec3(0.3, 0.55, 0.15)), 1e-3);
    vec3 alb = mix(wood, vTint * mix(1.0, lum, 0.6 * min(grainK, 1.0)), vTintK);
    if (vMat > 0.5) {
        // the board: a smooth painted panel with a faint grain under the paint
        alb = vTint * (0.96 + 0.08 * (late * 0.6 + (fib - 0.5) * 0.3));
    }

    // ---- light
    vec3 Ld = normalize(uLightDir);
    float sh = mix(1.0, shadowAt(vWorld + N * 0.012, 12), uShadowK);
    float ao = texelFetch(uAO, ivec2(gl_FragCoord.xy), 0).r;
    float ndl = dot(N, Ld);
    float wrap = max((ndl + 0.15) / 1.15, 0.0);
    vec3 amb = mix(uAmbBot, uAmbTop, N.y * 0.5 + 0.5);
    float fill = max(dot(N, normalize(uFillDir)), 0.0);
    vec3 col = alb * (uLightCol * wrap * sh * mix(1.0, ao, 0.35) + amb * ao + uFillCol * fill * ao);
    // satin: a broad soft sheen, a little stronger on the rounded arrises
    vec3 H = normalize(Ld + V);
    float nh = max(dot(N, H), 0.0);
    float rough = endGrain ? 0.75 : 0.5;
    float a2 = rough * rough * rough * rough;
    float dd = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * dd * dd);
    float F = 0.04 + 0.96 * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    float spec = D * F * 0.25 * max(ndl, 0.0) * sh;
    col += uLightCol * spec * uSheen * (vMat > 0.5 ? 0.6 : 1.0);
    float Fv = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    col += studio(reflect(-V, N)) * Fv * 0.18 * uSheen * ao;
    col += alb * vE.y;
    if (vE.z > 0.0) col = mix(col, studio(-V), vE.z);
    frag = vec4(col, max(vE.x, 0.0));
}
)";

// ---------------------------------------------------------------- the prepass (view-space position and normal)
inline const char* PRE_FS = R"(#version 330 core
in vec3 vWorld, vObj, vViewPos;
flat in vec3 vFace, vSize, vTint;
flat in vec4 vQuat, vE;
flat in float vSeed, vMat, vTintK;
layout(location = 0) out vec4 oPos;
layout(location = 1) out vec4 oNrm;
uniform mat4 uView;
uniform float uClipZ, uClipY;
void main() {
    if (vE.x < -0.5 && vE.x > -1.5 && vWorld.z < uClipZ) discard;
    if (vE.x < -1.5 && vWorld.y < uClipY) discard;
    vec3 N = normalize(qrot(vQuat, vFace));
    oPos = vec4(vViewPos, 1.0);
    oNrm = vec4(normalize(mat3(uView) * N), 1.0);
}
)";

// the table in the prepass (one ray per pixel)
inline const char* PRE_TABLE_FS = R"(#version 330 core
in vec2 vUv;
layout(location = 0) out vec4 oPos;
layout(location = 1) out vec4 oNrm;
uniform mat4 uInvVP, uView, uVP;
uniform float uTable;
void main() {
    vec4 a = uInvVP * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
    vec4 b = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ro = a.xyz / a.w, rd = normalize(b.xyz / b.w - ro);
    if (rd.y > -1e-4) discard;
    float t = (uTable - ro.y) / rd.y;
    vec3 p = ro + rd * t;
    vec4 c = uVP * vec4(p, 1.0);
    float z = c.z / c.w;
    if (z > 1.0) discard;
    gl_FragDepth = z * 0.5 + 0.5;
    oPos = vec4((uView * vec4(p, 1.0)).xyz, 1.0);
    oNrm = vec4(normalize(mat3(uView) * vec3(0.0, 1.0, 0.0)), 1.0);
}
)";

// ---------------------------------------------------------------- ambient occlusion
inline const char* SSAO_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPos, uNrm;
uniform mat4 uProj;
uniform float uRadius, uStrength;
uniform vec2 uTexel;
float hash2(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 45758.5453); }
void main() {
    vec4 P = texture(uPos, vUv);
    if (P.w < 0.5) { frag = vec4(1.0); return; }
    vec3 n = normalize(texture(uNrm, vUv).xyz);
    vec3 p = P.xyz;
    // the radius grows a little with the distance (far constructions keep their contact shades)
    float rad = uRadius * (1.0 + max(-p.z - 34.0, 0.0) * 0.012);
    float a0 = hash2(gl_FragCoord.xy) * 6.2831;
    vec3 t = normalize(abs(n.y) < 0.9 ? cross(n, vec3(0, 1, 0)) : cross(n, vec3(1, 0, 0)));
    vec3 bt = cross(n, t);
    float occ = 0.0;
    const int S = 16;
    for (int i = 0; i < S; i++) {
        float fi = float(i) + 0.5;
        float an = a0 + fi * 2.39996;
        float r = sqrt(fi / float(S));
        float h = sqrt(max(1.0 - r * r, 0.0));
        float sc = mix(0.15, 1.0, fract(fi * 0.618 + hash2(gl_FragCoord.xy + fi)));
        vec3 dir = (t * cos(an) * r + bt * sin(an) * r + n * h) * rad * sc;
        vec3 s = p + dir + n * 0.01;
        vec4 c = uProj * vec4(s, 1.0);
        vec2 uv = c.xy / c.w * 0.5 + 0.5;
        if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) continue;
        vec4 q = texture(uPos, uv);
        if (q.w < 0.5) continue;
        float dz = q.z - s.z;
        float range = smoothstep(0.0, 1.0, rad / max(abs(p.z - q.z), 1e-3));
        occ += (dz > 0.012 ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occ / float(S) * uStrength;
    frag = vec4(clamp(ao, 0.0, 1.0));
}
)";

inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uAO, uPos;
uniform vec2 uDir;
void main() {
    float z0 = texture(uPos, vUv).z;
    float sum = 0.0, wsum = 0.0;
    for (int i = -4; i <= 4; i++) {
        vec2 uv = vUv + uDir * float(i);
        float z = texture(uPos, uv).z;
        float w = exp(-float(i * i) / 10.0) * exp(-abs(z - z0) * 6.0);
        sum += texture(uAO, uv).r * w;
        wsum += w;
    }
    frag = vec4(sum / max(wsum, 1e-4));
}
)";

// ---------------------------------------------------------------- flat (the ghost's outline)
inline const char* FLAT_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aCol;
uniform mat4 uVP;
out vec4 vCol;
void main() {
    vCol = aCol;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";
inline const char* FLAT_FS = R"(#version 330 core
in vec4 vCol;
out vec4 frag;
void main() { frag = vec4(vCol.rgb, vCol.a); }
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uExposure, uSlow;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
vec3 tonemap(vec3 c) {
    c *= uExposure;
    vec3 x = max(c, 0.0);
    return x / (1.0 + max(x - 0.78, 0.0) * 0.85);
}
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = tonemap(s.rgb);
    c = mix(c, c * uSlowTint, uSlow);
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.16 * smoothstep(0.4, 0.95, length(q * vec2(1.0, 0.8)));
    c = mix(c, uBridgeCol, uBridge * (1.0 - s.a));
    c = mix(c, uBridgeInk, uBridge * s.a);
    c *= (1.0 - 0.5 * uDim) * uFade;
    frag = vec4(pow(max(c, 0.0), vec3(1.0 / 2.2)), 1.0);
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

// A shader with the given blocks inserted after its version line.
inline std::string with(const char* fs, const char* a, const char* b = "") {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + a + b + s.substr(nl + 1);
}

} // namespace planks_shaders

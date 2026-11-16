#pragma once
// RUINS' shaders: the world (ancient ruins in a jungle, as meshes rasterized at the full resolution, 4x
// multisampled: the stone carved, jointed, weathered and mossy, its missing blocks seen as holes in the walls, its
// parts re-forming out of golden dust; the sun's shadow map dappled by the canopy, bounce light; the sky; the
// foliage as cards with leafy masks, alpha to coverage; mist and light shafts integrated along each ray to the depth
// and a little ambient occlusion, at a reduced resolution, upsampled with the depth), the backing behind the well,
// the carved stone pieces, dust, chips, birds, lettering.

namespace ruins_shaders {

inline const char* COMMON_GLSL = R"(
float sq(float x) { return x * x; }
float sat(float x) { return clamp(x, 0.0, 1.0); }
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
vec2 hash2(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973)); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.xx + p3.yz) * p3.zy); }
float hash1(float n) { return hash(vec2(n, n * 0.371 + 17.13)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
float fbm2(vec2 p) { return (vnoise(p) * 0.66 + vnoise(p * 2.07 + 3.1) * 0.34); }
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float sdBox2(vec2 p, vec2 b) { vec2 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
)";

// ------------------------------------------------------------------------------------------------ the world
// Shared by the world's programs (after COMMON_GLSL): the camera, the light, the restoration, the canopy's light,
// the sky.
inline const char* SCENE_GLSL = R"(
uniform vec3 uEye, uFwd, uRight, uUp;
uniform float uTanH, uAspect, uTime, uPx;
uniform int uKind;
uniform vec3 uSunDir, uSunCol, uSkyZ, uSkyH, uAmb, uFog;
uniform vec3 uStone, uStone2, uMoss, uMoss2, uLeaf, uLeafLit, uBark, uGround;
uniform float uFogK, uMist, uShaft, uGap, uSway, uGold, uWorldA;
uniform vec3 uBridgeCol;
// the restoration: the jungle's cover, the temple's repair, the share of blocks still missing, the wear, the prime,
// MAYA's collapsed corner (1 open .. 0 closed)
uniform float uJungle, uTemple, uBroken, uWear, uPrime, uCorner;
uniform vec4 uFocus;   // the repair being watched (its point, its glow)
uniform float uFocusR;
const vec3 TC = vec3(0.0, 0.0, -52.0);   // the temple
const float CAN_Y = 31.0;                // the canopy
const vec3 BRZ[12] = vec3[12](vec3(-4.8, 21.0, -48.0), vec3(4.8, 21.0, -48.0), vec3(-5.6, 0.0, -24.5), vec3(5.6, 0.0, -24.5),
                              vec3(-5.0, 4.0, -33.4), vec3(5.0, 4.0, -33.4), vec3(-7.0, 4.0, -40.0), vec3(7.0, 4.0, -40.0),
                              vec3(-6.0, 2.7, -42.0), vec3(6.0, 2.7, -42.0), vec3(-12.5, 0.0, -38.5), vec3(12.5, 0.0, -38.5));

// ---- the canopy: leaf masses over everything but the clearing around the temple; its light at any point
float clearing(vec2 q) {
    vec2 c = (q - vec2(0.0, -50.0)) / (uKind == 2 ? vec2(46.0, 40.0) : vec2(33.0, 28.0));
    return smoothstep(0.82, 1.15, length(c) * (1.0 + 0.9 * uJungle));
}
float canopy(vec2 q, float detail) {
    vec2 s = q + uSway * vec2(sin(uTime * 0.37 + q.y * 0.05), cos(uTime * 0.29 + q.x * 0.05)) * 0.7;
    float n = fbm(s * 0.045) * 0.68 + mix(0.16, vnoise(s * 0.42), detail) * 0.32;
    float th = 0.43 - uGap + 0.1 * uJungle;
    return smoothstep(th - 0.05, th + 0.05, n) * clearing(q);
}
float sunThrough(vec3 p) {
    if (p.y > CAN_Y) return 1.0;
    // (a low sun comes in under the canopy's edge: its light is cut by the leaves nearer by)
    float tc = min((CAN_Y - p.y) / max(uSunDir.y, 0.08), 40.0);
    vec2 q = p.xz + uSunDir.xz * tc;
    return 1.0 - 0.94 * canopy(q, 0.6) * (1.0 - 0.45 * uGold);
}

// ---- the sky, a few clouds, far jungle hills at the horizon
vec3 sky(vec3 rd) {
    float y = rd.y;
    vec3 c = mix(uSkyH, uSkyZ, pow(sat(y), 0.6));
    float mu = sat(dot(rd, uSunDir));
    c += uSunCol * (0.05 * pow(mu, 4.0) + 0.3 * pow(mu, 40.0)) + uSunCol * 6.0 * smoothstep(0.9993, 0.9997, mu);
    if (y > 0.0) {
        vec2 cq = rd.xz / (y + 0.12) * 1.3 + vec2(uTime * 0.003, 0.0);
        float cl = smoothstep(0.52, 0.85, fbm(cq));
        c = mix(c, mix(uSkyH * 1.05, uSunCol * 0.45 + uSkyH * 0.5, mu * mu), cl * 0.45);
    }
    float az = atan(rd.x, -rd.z);
    float h = 0.03 + 0.05 * fbm(vec2(az * 3.0, 1.0));
    float far = 1.0 - smoothstep(h - 0.004, h + 0.004, y);
    return mix(c, mix(uFog, uLeaf * 2.0, 0.22), far * 0.85);
}
vec3 rayDir(vec2 uv) {
    vec2 ndc = uv * 2.0 - 1.0;
    return normalize(uFwd + uRight * (ndc.x * uTanH * uAspect) + uUp * (ndc.y * uTanH));
}
// the sun's shadow map (a few taps for a soft edge)
uniform sampler2DShadow uShadow;
uniform mat4 uLightVP;
uniform float uSoft;
float shadowAt(vec3 wp, vec3 n) {
    vec4 l = uLightVP * vec4(wp + n * 0.05, 1.0);
    vec3 s = l.xyz * 0.5 + 0.5;
    if (s.x <= 0.0 || s.x >= 1.0 || s.y <= 0.0 || s.y >= 1.0 || s.z >= 1.0) return 1.0;
    const vec2 P[12] = vec2[12](vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696, 0.457), vec2(-0.203, 0.621),
                                vec2(0.962, -0.195), vec2(0.473, -0.480), vec2(0.519, 0.767), vec2(0.185, -0.893),
                                vec2(0.507, 0.064), vec2(0.896, 0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598));
    float r = 0.0;
    for (int i = 0; i < 12; i++) r += texture(uShadow, vec3(s.xy + P[i] * uSoft, s.z - 0.0004));
    return r / 12.0;
}
// a part re-forming out of golden dust (its dissolve: 1 gone .. 0 there)
float dissolveN(vec3 p) { return vnoise(p.xy * 2.3 + p.z * 0.7) * 0.6 + vnoise(p.zy * 5.1 - p.x * 1.3) * 0.4; }
)";

// ---- the meshes: the instance's model (rest -> now), its rest placement (pattern space), its params
inline const char* MESH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aAttr;  // material, ao, pattern, seed
layout(location = 3) in vec4 iM0;
layout(location = 4) in vec4 iM1;
layout(location = 5) in vec4 iM2;
layout(location = 6) in vec4 iC;     // rest center, seed
layout(location = 7) in vec4 iS;     // rest scale, dissolve
layout(location = 8) in vec4 iP;     // glow, material override, -, -
uniform mat4 uVP;
out vec3 vW, vP, vNP, vObj;
flat out vec4 vAttr, vS, vInfo;
flat out vec3 vD0, vD1, vD2;
void main() {
    vec4 p4 = vec4(aPos, 1.0);
    vW = vec3(dot(iM0, p4), dot(iM1, p4), dot(iM2, p4));
    vP = iC.xyz + iS.xyz * aPos;
    vNP = aNrm / iS.xyz;
    vObj = aPos;
    vAttr = vec4(iP.y >= 0.0 ? iP.y : aAttr.x, aAttr.y, aAttr.z, aAttr.w + iC.w);
    vS = iS;
    vInfo = iP;
    mat3 L = transpose(mat3(iM0.xyz, iM1.xyz, iM2.xyz));
    vD0 = L[0] / iS.x;
    vD1 = L[1] / iS.y;
    vD2 = L[2] / iS.z;
    gl_Position = uVP * vec4(vW, 1.0);
}
)";
inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vW, vP, vNP, vObj;
flat in vec4 vAttr, vS, vInfo;
out vec4 frag;
void main() {
    if (vS.w > 0.0 && dissolveN(vP) < vS.w * 1.1 - 0.05) discard;
    frag = vec4(1.0);
}
)";

inline const char* MESH_FS = R"(#version 330 core
in vec3 vW, vP, vNP, vObj;
flat in vec4 vAttr, vS, vInfo;
flat in vec3 vD0, vD1, vD2;
out vec4 frag;

// ---- carvings: a height field (1 the surface, 0 the deepest cut) for the bands and panels of each style
float glyph(vec2 g, float id) {
    // a Maya cartouche: a rounded frame, an inner sign made of a few blobs and bars
    float d = sdBox2(g, vec2(0.36, 0.36)) - 0.06;
    float frame = smoothstep(0.035, 0.0, abs(d + 0.04) - 0.03);
    float h1 = hash1(id), h2 = hash1(id + 7.1), h3 = hash1(id + 3.3);
    float inner = 0.0;
    inner = max(inner, smoothstep(0.04, 0.0, length(g - vec2(-0.12, 0.1) * (h1 - 0.3)) - 0.09 - 0.05 * h2));
    inner = max(inner, smoothstep(0.03, 0.0, sdBox2(g - vec2(0.0, -0.14 + 0.1 * h3), vec2(0.22 * h2 + 0.06, 0.035))));
    inner = max(inner, smoothstep(0.03, 0.0, length(g - vec2(0.14, 0.12) * (h3 * 2.0 - 1.0)) - 0.06));
    float cut = smoothstep(0.0, -0.03, d);
    return 1.0 - 0.8 * cut * (1.0 - max(frame, inner));
}
float carve(vec2 uv, vec3 p, float id) {
    vec3 q = p - TC;
    if (uKind == 0) {
        if (id == 3.0 && p.y > 1.4 && p.y < 5.0) {
            vec2 g = vec2(uv.x / 0.46, (p.y - 1.4) / 0.46);
            return glyph(fract(g) - 0.5, floor(g.x) * 13.0 + floor(g.y) * 7.0 + floor(p.z));
        }
        float yl = mod(q.y, 3.0);
        if (id == 1.0 && yl > 2.12 && yl < 2.8 && q.y < 21.0) {
            vec2 g = vec2(uv.x / 0.68, (yl - 2.12) / 0.68);
            return glyph(fract(g) - 0.5, floor(g.x) + floor(q.y / 3.0) * 31.0);
        }
        return 1.0;
    }
    if (uKind == 1) {
        // lotus petals in rows, devata panels on the pillars
        float yl = mod(q.y + 0.3, 2.0);
        if (id == 1.0 && yl < 0.55) {
            vec2 g = vec2(fract(uv.x / 0.5) - 0.5, yl / 0.55);
            float petal = abs(g.x) * 2.0 + sq(g.y - 0.1) * 1.4;
            return 1.0 - 0.75 * smoothstep(0.05, 0.0, abs(petal - 0.75) - 0.06) - 0.4 * step(0.9, g.y);
        }
        if (id == 3.0 && p.y > 1.6 && p.y < 3.6) {
            vec2 g = vec2(uv.x, p.y - 2.6);
            float niche = sdBox2(g * vec2(1.0, 0.5), vec2(0.18, 0.4)) - 0.05;
            float fig = length(g * vec2(1.0, 0.45) - vec2(0.0, 0.05)) - 0.12;
            return 1.0 - 0.7 * smoothstep(0.02, -0.02, niche) * smoothstep(-0.02, 0.03, fig);
        }
        return 1.0;
    }
    // Greek: the triglyph frieze
    if (id == 1.0 && q.y > 13.85 && q.y < 14.85 && q.z > 6.0) {
        float u = mod(uv.x + 1.0, 2.0);
        if (u < 0.9) {
            float g = fract(u / 0.3);
            return 1.0 - 0.7 * smoothstep(0.12, 0.05, abs(g - 0.5) - 0.2) * step(q.y, 14.75);
        }
        return 0.94;
    }
    return 1.0;
}

// ---- masonry: coursed blocks (the block's center in uv, its hash, the distance to its nearest joint)
vec4 masonryCell(vec2 uv, float ch, float bl) {
    float row = floor(uv.y / ch);
    float off = hash1(row) * 0.7 + row * 0.5;
    float u = uv.x / bl + off;
    float c = floor(u);
    vec2 f = vec2(fract(u) * bl, fract(uv.y / ch) * ch);
    float e = min(min(f.x, bl - f.x), min(f.y, ch - f.y));
    return vec4((c + 0.5 - off) * bl, (row + 0.5) * ch, hash(vec2(c, row)), e);
}

// MAYA's collapsed corner: 1 where its facing stones are gone (the rough core of rubble seen), its fringe between
float cornerK(vec3 p) {
    if (uKind != 0 || uCorner <= 0.0) return 0.0;
    vec3 q = (p - TC - vec3(16.0, 7.0, 18.5)) * vec3(1.0, 1.3, 1.0);
    float d = length(q) - 8.0 * uCorner - 1.6 * vnoise(p.xy * 0.45 + p.z * 0.3);
    return sat(0.5 - d / 2.4);
}
// the share of the stones missing here (the ruin's, the corner's fringe)
float missRate(vec3 p, float ck) { return max(uBroken, smoothstep(0.0, 0.5, ck) * 0.55); }

vec3 mossC0(vec3 p) { return mix(uMoss2, uMoss, smoothstep(0.3, 0.8, vnoise(p.xz * 1.7 + p.y))); }
vec3 lightAt(vec3 wp, vec3 nn, float sh, float through, float ao) {
    vec3 L = uSunDir;
    vec3 lin = uSunCol * max(dot(nn, L), 0.0) * sh * through;
    lin += uAmb * ao * (0.55 + 0.45 * nn.y);
    vec3 bounce = mix(uGround, uStone, 0.6) * (uSunCol * 0.16 + uAmb * 0.25);
    lin += bounce * ao * sat(0.6 - 0.6 * nn.y);
    // the warm light thrown back by the sunlit stones around
    lin += uSunCol * uStone * 0.12 * ao * sat(dot(nn, normalize(vec3(-L.x, 0.25, -L.z))));
    // the braziers' fire on the stones around
    if (uPrime > 0.0) {
        for (int i = 0; i < 4; i++) {
            vec3 Lb = BRZ[uKind * 4 + i] + vec3(0.0, 1.8, 0.0) - wp;
            float att = 1.0 / (1.0 + dot(Lb, Lb) * 0.12);
            lin += vec3(2.4, 1.1, 0.35) * uPrime * att * (0.25 + max(dot(nn, normalize(Lb)), 0.0)) * ao;
        }
    }
    return lin;
}

void main() {
    // re-forming out of golden dust
    float edge = 0.0;
    if (vS.w > 0.0) {
        float n = dissolveN(vP), th = vS.w * 1.1 - 0.05;
        if (n < th) discard;
        edge = (1.0 - smoothstep(0.0, 0.07, n - th)) * vInfo.x;
    }
    mat3 Dl = mat3(vD0, vD1, vD2);
    vec3 p = vP, wp = vW;
    vec3 n = normalize(vNP);
    if (!gl_FrontFacing && dot(Dl * n, uEye - wp) < 0.0) n = -n;
    float id = floor(vAttr.x + 0.5), flag = floor(vAttr.z + 0.5), seed = vAttr.w;
    vec3 rd = normalize(wp - uEye);
    float t = length(wp - uEye);
    vec3 an = abs(n);
    vec2 uv;
    vec3 tu, tv;
    if (an.y > 0.75) { uv = p.xz; tu = vec3(1.0, 0.0, 0.0); tv = vec3(0.0, 0.0, 1.0); }
    else if (an.x > an.z) { uv = vec2(p.z * sign(n.x), p.y); tu = vec3(0.0, 0.0, sign(n.x)); tv = vec3(0.0, 1.0, 0.0); }
    else { uv = vec2(-p.x * sign(n.z), p.y); tu = vec3(-sign(n.z), 0.0, 0.0); tv = vec3(0.0, 1.0, 0.0); }
    float fw = max(length(fwidth(uv)), 1e-4);
    float lod = sat(t / 90.0);
    float ao = vAttr.y;
    vec3 nW0 = normalize(Dl * n);
    vec3 alb;
    float moss = 0.0;
    vec3 nn = n;
    float hole = 0.0;
    vec3 holeCol = vec3(0.0);
    vec3 L = uSunDir;
    float through = sunThrough(wp);
    float sh = dot(nW0, L) > -0.2 && through > 0.02 ? shadowAt(wp, nW0) : 0.0;
    float h = 1.0, mY = 0.5, joint = 0.0;
    if (id == 1.0 || id == 3.0) {
        // stone: blocks in courses (on the big faces), single stones, drums; each its own tone, weathered,
        // carved, mossy
        float ch = uKind == 2 ? 0.9 : 0.62, bl = uKind == 2 ? 1.8 : 1.25;
        vec4 m;
        float ck = id == 1.0 && flag < 0.5 ? cornerK(p) : 0.0;
        if (flag < 0.5) m = masonryCell(uv, ch, bl);
        else {
            vec3 hs = vS.xyz, q = vObj * hs;
            float e;
            if (flag < 1.5) {
                vec3 d = hs - abs(q);
                e = an.x > an.y && an.x > an.z ? min(d.y, d.z) : (an.y > an.z ? min(d.x, d.z) : min(d.x, d.y));
            } else e = an.y > 0.7 ? (1.0 - length(vObj.xz)) * hs.x : (0.5 - abs(vObj.y)) * hs.y;
            m = vec4(0.0, 0.0, hash1(seed * 13.7 + 0.3), e + 0.04);
        }
        mY = m.z;
        float jw = 0.012 + fw * 1.2;
        joint = (1.0 - smoothstep(jw, jw + 0.03 + lod * 0.05, m.w)) * (1.0 - smoothstep(0.04, 0.3, fw / ch));
        float wn = fbm(p.xz * 0.35 + p.y * vec2(0.27, -0.21));
        alb = mix(uStone2, uStone, 0.45 + 0.55 * smoothstep(0.2, 0.8, wn));
        alb *= 1.0 + (0.24 * m.z - 0.14) * (0.35 + 0.65 * uWear);
        float grain = vnoise(uv * 9.0) * 0.5 + vnoise(uv * 23.0) * 0.5;
        alb *= 0.9 + 0.2 * grain * (1.0 - smoothstep(0.02, 0.08, fw));
        // dark streaks of rain down the faces
        float streak = smoothstep(0.55, 0.85, fbm(vec2(uv.x * 1.7, p.y * 0.08))) * (1.0 - an.y);
        alb *= 1.0 - 0.3 * streak * uWear;
        h = carve(uv, p, id);
        if (h < 0.999 && fw < 0.2) {
            const float E = 0.015;
            float hx = carve(uv + vec2(E, 0.0), p + tu * E, id) - h, hy = carve(uv + vec2(0.0, E), p + tv * E, id) - h;
            nn = normalize(n - (tu * hx + tv * hy) / E * 0.012);
        }
        h = mix(1.0, h, 1.0 - 0.55 * uWear);
        alb *= mix(0.5, 1.0, h);
        alb *= 1.0 - 0.45 * joint;
        // worn edges of the blocks: rounded toward the joints
        float wear = smoothstep(0.12, 0.02, m.w) * (1.0 - joint) * (1.0 - lod) * uWear;
        nn = normalize(nn + (tv * (fract(uv.y / 0.62) > 0.5 ? 1.0 : -1.0)) * wear * 0.25 * (1.0 - an.y));
        // moss: on the tops, in the crevices and the joints, low on the walls; lichen
        vec2 ruv = mat2(0.8, -0.6, 0.6, 0.8) * uv; // (rotated: the noise grid off the stone's axes)
        float mn = fbm(p.xz * 0.3 + p.y * 0.23 + 3.0);
        float mossAmt = uKind == 2 ? 0.55 : 1.0;
        moss = smoothstep(0.42, 0.7, mn) * (0.25 + 0.9 * sat(nW0.y)) + joint * 0.6 * mn + (1.0 - h) * 0.5 * mn +
               smoothstep(1.5, 0.0, wp.y) * smoothstep(0.3, 0.6, mn);
        moss = sat(moss * mossAmt * (0.12 + 0.88 * uWear) + uJungle * 0.7 * smoothstep(0.2, 0.5, mn));
        moss *= smoothstep(0.25, 0.5, (vnoise(ruv * 5.0) * 0.7 + vnoise(ruv * 13.0 + 3.0) * 0.3) * 0.6 + moss * 0.6);
        float lich = smoothstep(0.72, 0.8, vnoise(ruv * 2.6 + 5.0)) * (1.0 - moss);
        alb = mix(alb, uStone * 1.25 + vec3(0.04, 0.035, 0.0), lich * 0.35 * uWear);
        // ivy and vines hanging down the vertical faces
        if (an.y < 0.5) {
            float iv = fbm(vec2(uv.x * 0.32, p.y * 0.1) + vec2(id * 3.0, 0.0)) + 0.25 * sin(uv.x * 0.21);
            float ivy = smoothstep(0.66 - 0.5 * uJungle, 0.72 - 0.5 * uJungle, iv) * smoothstep(0.25, 0.55, (vnoise(ruv * 7.0) * 0.65 + vnoise(ruv * 17.0 + 7.0) * 0.35) + 0.2 + uJungle) * (uKind == 2 ? 0.4 : 1.0) * max(uWear, uJungle);
            if (ivy > 0.01) {
                vec2 lh = vec2(vnoise(ruv * 6.0), vnoise(ruv * 6.0 + 17.0));
                nn = normalize(mix(nn, normalize(n + (tu * (lh.x - 0.5) + tv * (lh.y - 0.3)) * 1.2), ivy));
                alb = mix(alb, uLeaf * (0.8 + 0.6 * lh.x), ivy);
                moss *= 1.0 - ivy;
            }
        }
        // the prime: the colors temples had
        if (uPrime > 0.0) {
            vec3 paint = alb;
            bool cut = h < 0.9;
            if (uKind == 0) paint = cut ? (fract(uv.x * 0.6) < 0.5 ? vec3(0.08, 0.25, 0.42) : vec3(0.85, 0.62, 0.18)) : vec3(0.52, 0.09, 0.05) * (0.9 + 0.2 * m.z);
            else if (uKind == 1) paint = p.y > 17.0 ? vec3(0.95, 0.66, 0.24) : (cut ? vec3(0.5, 0.16, 0.08) : uStone * 1.08);
            else paint = cut ? vec3(0.06, 0.12, 0.42) : (h < 0.97 && p.y > 13.5 ? vec3(0.5, 0.08, 0.05) : uStone * 1.1);
            if (id == 3.0 && uKind != 0) paint = mix(paint, uStone * 1.1, 0.7);
            alb = mix(alb, paint, uPrime * 0.85);
        }
        // a missing stone (its hole in the wall, seen inside: its walls lit through the opening)
        if (id == 1.0 && flag < 0.5) {
            float rate = ck > 0.5 ? 0.0 : missRate(p, ck);
            if (rate > 0.0) {
                float hh = hash(vec2(m.x * 1.7 + dot(n, vec3(3.1, 7.3, 11.9)), m.y * 2.3));
                float k = sat((rate - hh) / 0.015);
                vec2 half = vec2(bl, ch) * 0.5 * (0.2 + 0.8 * k) - 0.015;
                vec2 lo = uv - m.xy;
                if (k > 0.0 && abs(lo.x) < half.x && abs(lo.y) < half.y) {
                    hole = 1.0;
                    float D = 0.22 + 0.25 * k + 0.35 * step(uBroken + 0.01, rate);
                    vec3 vT = vec3(dot(rd, tu), dot(rd, tv), dot(rd, n));
                    float tb = D / max(-vT.z, 1e-3);
                    float tx = abs(vT.x) > 1e-4 ? (sign(vT.x) * half.x - lo.x) / vT.x : 1e9;
                    float ty = abs(vT.y) > 1e-4 ? (sign(vT.y) * half.y - lo.y) / vT.y : 1e9;
                    float th = min(tb, min(tx, ty));
                    vec3 wn = th == tb ? n : (th == tx ? -sign(vT.x) * tu : -sign(vT.y) * tv);
                    vec3 hp = vec3(lo + vT.xy * th, -vT.z * th); // (in the hole: across, depth)
                    vec3 Lt = vec3(dot(L, tu), dot(L, tv), dot(L, n));
                    float lit = 0.0;
                    if (Lt.z > 0.02) {
                        vec2 ex = hp.xy + Lt.xy * (hp.z / Lt.z);
                        lit = smoothstep(0.03, -0.03, max(abs(ex.x) - half.x, abs(ex.y) - half.y));
                    }
                    float deep = sat(hp.z / D);
                    vec3 ha = mix(mix(uStone2, uStone, 0.55), uMoss2, 0.3 * uWear) * (0.8 + 0.3 * vnoise(hp.xy * 3.0 + hp.z * 2.0));
                    vec3 wnW = normalize(Dl * wn);
                    // (the sun through the opening, the sky, the light bounced off the hole's lit floor)
                    holeCol = ha * (uSunCol * max(dot(wnW, L), 0.0) * lit * sh * through + uAmb * (1.0 - 0.45 * deep) * (0.65 + 0.35 * wnW.y) +
                                    uSunCol * sh * through * 0.07 * (1.0 - 0.5 * deep));
                    // the dust of a block re-forming: golden motes swirling in it
                    float f = k * (1.0 - k) * 4.0 * sat(uFocus.w * 2.0);
                    if (f > 0.01) {
                        float sp = vnoise(uv * 14.0 + uTime * 3.0) * vnoise(uv * 5.0 - uTime);
                        holeCol += vec3(1.0, 0.75, 0.42) * f * (0.25 + 2.5 * sp * sp) * (0.6 + uGold);
                    }
                }
            }
        }
        // the corner's rubble core: rough, lumpy, no courses
        if (ck > 0.5) {
            // (irregular rubble stones in mortar: the cells of a jittered grid, each stone bulging, its own tone)
            vec2 rq = uv * vec2(2.2, 3.0) + 17.0;
            vec2 ri = floor(rq), rf = fract(rq);
            float d1 = 9.0, d2 = 9.0;
            vec2 best = vec2(0.0), bo = vec2(0.0);
            for (int y = -1; y <= 1; y++)
                for (int x = -1; x <= 1; x++) {
                    vec2 g = vec2(float(x), float(y));
                    vec2 o = g + hash2(ri + g) * 0.8 + 0.1 - rf;
                    float d = dot(o, o);
                    if (d < d1) { d2 = d1; d1 = d; best = ri + g; bo = o; }
                    else if (d < d2) d2 = d;
                }
            float edgeD = sqrt(d2) - sqrt(d1);
            float mortar = (1.0 - smoothstep(0.015, 0.07 + fw * 2.4, edgeD + 0.05 * (vnoise(uv * 11.0) - 0.5)));
            float sh2 = hash(best * 1.3 + 0.7);
            float k = smoothstep(0.5, 0.6, ck);
            vec3 rub = mix(uStone2, uStone, 0.25 + 0.55 * sh2) * (0.7 + 0.45 * vnoise(uv * 7.0 + sh2 * 9.0)) * 0.92;
            rub = mix(rub, uStone2 * 0.6, mortar * 0.8);
            alb = mix(alb, mix(rub, mossC0(p), (0.2 + 0.4 * mortar) * uWear * smoothstep(0.4, 0.7, vnoise(uv * 1.3))), k);
            vec2 bulge = -bo * 0.35 * (1.0 - mortar) + (vec2(vnoise(uv * 9.0), vnoise(uv * 9.0 + 5.0)) - 0.5) * 0.4;
            nn = normalize(mix(nn, normalize(n + tu * bulge.x + tv * bulge.y), k));
            ao *= mix(1.0, 1.0 - 0.35 * mortar, k);
            joint *= 1.0 - k;
        }
    } else if (id == 2.0) {
        // bark (and the fig's pale roots), furrowed, mossy at the foot
        float f = vnoise(vec2(atan(n.z, n.x) * 6.0, p.y * 0.6)) * 0.6 + vnoise(uv * 4.0) * 0.4;
        alb = uBark * (0.65 + 0.5 * f);
        float fur = vnoise(vec2(atan(n.z, n.x) * 14.0 + p.y * 0.2, p.y * 0.25));
        alb *= 0.8 + 0.35 * fur;
        nn = normalize(n + tu * (fur - 0.5) * 0.35);
        moss = smoothstep(4.0, 0.0, p.y) * smoothstep(0.35, 0.7, fbm(uv * 0.8)) * (uKind == 2 ? 0.3 : 0.9);
        moss += smoothstep(0.62, 0.8, fbm(uv * 0.5 + 9.0)) * 0.5 * sat(n.y + 0.3);
        moss = sat(moss);
        ao *= 0.55 + 0.45 * smoothstep(0.0, 3.0, p.y);
    } else if (id == 4.0) {
        // bronze, dark, a little verdigris
        float v = vnoise(p.xz * 6.0 + p.y * 3.0);
        alb = mix(vec3(0.16, 0.1, 0.05), vec3(0.12, 0.24, 0.2), smoothstep(0.5, 0.8, v));
    } else {
        // the ground: the avenue's flagstones (joints full of moss and grass), the forest floor elsewhere
        float paved = (1.0 - smoothstep(5.4, 5.9, abs(p.x))) * step(-36.0, p.z);
        vec4 m = masonryCell(p.xz + vec2(0.0, 0.3), 1.1, 1.6);
        float jw = 0.03 + fw;
        float jt = (1.0 - smoothstep(jw, jw + 0.09 + lod * 0.2, m.w)) * (1.0 - smoothstep(0.05, 0.4, fw));
        float wn = fbm(p.xz * 0.25);
        vec3 flag_ = mix(uStone2, uStone, 0.3 + 0.5 * wn) * (0.8 + 0.25 * m.z);
        float fm = smoothstep(0.35, 0.75, wn + jt * 0.6);
        vec3 floorC = mix(uGround, uMoss2, smoothstep(0.35, 0.7, fbm(p.xz * 0.4 + 7.0)));
        float litter = step(0.78, hash(floor(p.xz * 3.0)));
        floorC = mix(floorC, uGround * vec3(1.4, 1.05, 0.7), litter * 0.4 * (1.0 - smoothstep(0.05, 0.3, fw)));
        floorC *= 0.85 + 0.3 * vnoise(p.xz * 2.1);
        alb = mix(floorC, flag_ * (1.0 - 0.35 * jt), paved);
        moss = paved * sat(fm * 0.9 + jt * 0.6) * (uKind == 2 ? 0.5 : 1.0) * (0.1 + 0.9 * uWear) + uJungle * 0.6;
        moss = max(moss, (1.0 - paved) * 0.5 * smoothstep(0.4, 0.7, fbm(p.xz * 0.6)));
    }
    vec3 mossC = mix(uMoss2, uMoss, smoothstep(0.3, 0.8, vnoise(p.xz * 1.7 + p.y)));
    alb = mix(alb, mossC, sat(moss));

    vec3 nnW = normalize(Dl * nn);
    vec3 col = alb * lightAt(wp, nnW, sh, through, ao);
    if (hole > 0.0) col = holeCol;
    // the repair being watched glows softly, motes of light running over the stone
    if (uFocus.w > 0.01 && (id == 1.0 || id == 3.0)) {
        float fd = length(wp - uFocus.xyz) / uFocusR;
        col += vec3(1.0, 0.72, 0.38) * uFocus.w * 0.55 * exp(-fd * fd) * (0.45 + 0.8 * vnoise(uv * 5.0 + uTime * 2.5));
    }
    // a soft sheen on the moss in the sun
    col += mossC * uSunCol * moss * sh * through * 0.12 * pow(sat(dot(reflect(rd, nnW), L)), 4.0);
    // the golden edge of a stone re-forming
    col += vec3(1.6, 1.05, 0.5) * edge * (1.0 + uGold);
    frag = vec4(max(col, 0.0), 1.0);
}
)";

// ---- the sky (full resolution, behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
void main() { frag = vec4(sky(rayDir(vUv)), 1.0); }
)";

// ---- the foliage: cards (jungle walls, the jungle's mass, undergrowth, ferns, lianas, the canopy's edge), their
// leafy masks computed where they are, at the full resolution (alpha to coverage)
inline const char* FOL_VS = R"(#version 330 core
layout(location = 0) in vec4 aPos; // xyz, layer
uniform mat4 uVP;
out vec3 vW;
flat out float vLayer;
void main() {
    vW = aPos.xyz;
    vLayer = aPos.w;
    gl_Position = uVP * vec4(aPos.xyz, 1.0);
}
)";
inline const char* FOL_FS = R"(#version 330 core
in vec3 vW;
flat in float vLayer;
out vec4 frag;
// a crown line along u: soft rounded crowns (jungle) or umbrella pines and cypresses (Mediterranean); returns the
// coverage of (u, v)
float crowns(vec2 uv, float base, float amp, float seed, float fw, int pine) {
    float u = uv.x;
    float top = base + amp * 0.55 * fbm2(vec2(u * 0.06 + seed, seed));
    float cu = u * 0.11 + seed;
    float ci = floor(cu), cf = fract(cu) - 0.5;
    float ch = hash1(ci + seed * 7.0);
    if (pine == 0) {
        top += amp * (0.25 + 0.5 * ch) * sqrt(max(0.0, 0.25 - cf * cf)) * 2.0;
        top += amp * 0.12 * (vnoise(uv * vec2(0.9, 0.9) + seed) - 0.5) + 0.35 * (vnoise(uv * 3.1 + seed) - 0.5);
        top += 0.12 * (vnoise(uv * 9.0 + seed) - 0.5);
        return 1.0 - smoothstep(-fw, fw, uv.y - top);
    }
    // the maquis low, a pine's umbrella or a cypress over it
    float low = base * 0.45 + amp * 0.25 * fbm2(vec2(u * 0.2, seed)) + 0.3 * (vnoise(uv * 2.7) - 0.5) + 0.1 * (vnoise(uv * 9.0) - 0.5);
    float cov = 1.0 - smoothstep(-fw, fw, uv.y - low);
    float H = base + amp * (0.6 + 0.5 * ch);
    if (ch > 0.45) {
        float w = 0.42 + 0.1 * hash1(ci + 3.0);
        H -= amp * 0.35;
        float e = 1.0 - sq(cf / w);
        float topC = H + 1.6 * sqrt(max(e, 0.0)) + 0.9 * (fbm2(uv * 0.5 + seed) - 0.5) + 0.15 * (vnoise(uv * 8.0) - 0.5);
        float botC = H - 2.2 * sqrt(max(e, 0.0)) * (0.5 + 0.6 * vnoise(uv * 0.9)) + 0.6 * (vnoise(uv * 2.0) - 0.5) + 0.3;
        float crown = (1.0 - smoothstep(-fw, fw, uv.y - topC)) * smoothstep(-fw, fw, uv.y - botC) * step(0.0, e);
        float trunk = (1.0 - smoothstep(0.035, 0.035 + fw / 9.0, abs(cf - 0.06 * sin(uv.y * 0.3 + ci)))) * step(uv.y, H);
        cov = max(cov, max(crown, trunk));
    } else if (ch < 0.12) {
        float w = 0.07 * (1.0 - sat(uv.y / (H + 3.0)));
        float cyp = (1.0 - smoothstep(w * 9.0 - fw, w * 9.0 + fw, abs(cf) * 9.0 + 0.2 * (vnoise(uv * 3.0) - 0.5))) * step(uv.y, H + 3.0);
        cov = max(cov, cyp);
    }
    return cov;
}
// ferns: arching fronds along u, serrated
float ferns(vec2 uv, float base, float amp, float seed, float fw) {
    float u = uv.x * 0.9 + seed;
    float c = 0.0;
    for (int k = 0; k < 3; k++) {
        float uu = u + float(k) * 0.37;
        float i = floor(uu), f = fract(uu) - 0.5;
        float h = base + amp * hash1(i + seed * 3.0 + float(k) * 17.0);
        float top = h * (1.0 - sq(f * 1.9)) + 0.09 * sin(uv.x * 34.0 + i) * (0.4 + 0.6 * sat(uv.y / max(h, 0.1)));
        c = max(c, 1.0 - smoothstep(-fw, fw, uv.y - top));
    }
    return c;
}
float lianas(vec2 uv, float seed, float fw) {
    float u = uv.x * 0.7 + seed;
    float i = floor(u), f = fract(u) - 0.5;
    float h = hash1(i * 1.7 + seed);
    if (h < 0.6 - 0.35 * uJungle) return 0.0;
    float bottom = 4.0 + 16.0 * hash1(i + 9.0);
    float x = f + 0.12 * sin(uv.y * 0.18 + i * 3.0 + uSway * sin(uTime * 0.5 + i) * 0.6);
    float leafy = smoothstep(0.55, 0.75, vnoise(vec2(i * 3.0, uv.y * 1.6)));
    float w = (0.035 + 0.11 * leafy) * 0.7;
    float a = 1.0 - smoothstep(w - fw * 0.7, w + fw * 0.7, abs(x) * 1.43);
    return a * smoothstep(bottom - 0.3, bottom + 0.3, uv.y);
}
vec3 foliageCol(float kind, float seed, vec3 p, vec2 uv, vec3 rd, float shade) {
    float n = fbm(uv * (kind == 4.0 ? 0.25 : 0.55) + seed * 3.0);
    float light = sunThrough(p + vec3(0.0, 0.5, 0.0)) * shade;
    if (kind == 4.0) {
        // the canopy's edge from below: dark leaves, a little light through them toward their tips
        float tip = 1.0 - smoothstep(0.0, 3.0, uv.y - (seed == 0.0 ? 15.0 : 20.0));
        vec3 c = uLeaf * (0.35 + 0.4 * n) * uAmb;
        c += uLeafLit * uSunCol * (0.03 + 0.08 * tip) * n;
        return c * (0.8 + 0.4 * vnoise(uv * 3.0));
    }
    float v = uv.y;
    vec3 base = uLeaf * mix(0.55, 1.25, n);
    if (kind == 3.0) base = mix(uBark * 0.5, uLeaf, 0.6);
    // the light from above (the tops lit), and the sun's translucency through the leaves when it is behind them
    float top = kind == 1.0 ? smoothstep(0.0, 14.0, v - 10.0) : sat(v / 2.0);
    float back = pow(sat(dot(rd, uSunDir)), 3.0);
    vec3 c = base * (uAmb * (0.55 + 0.45 * top));
    c += base * uSunCol * light * (0.25 + 0.45 * top);
    c += uLeafLit * uSunCol * light * back * 0.35;
    c *= 0.75 + 0.25 * vnoise(uv * 4.0 + seed);
    // leaf clumps: rounded masses, their upper edges catching the light; small leaves at the full resolution
    float clump = fbm(uv * 0.32 + seed * 7.0);
    float rim = smoothstep(0.5, 0.75, clump) * (1.0 - smoothstep(0.75, 0.95, clump));
    c = c * (0.55 + 0.8 * clump) + uLeafLit * (uSunCol * light * 0.22 + uAmb * 0.1) * rim;
    float leaves = vnoise(uv * 7.0 + seed) * 0.6 + vnoise(uv * 17.0 - seed) * 0.4;
    c *= 0.82 + 0.36 * leaves;
    return c;
}
void main() {
    int L = int(vLayer + 0.5);
    vec3 p = vW;
    vec3 rd = normalize(p - uEye);
    int pine = uKind == 2 ? 1 : 0;
    float a = 0.0, kind = 1.0, seed = 0.0;
    vec2 uv = p.xy;
    float J = uJungle;
    if (L < 4) {
        uv = L < 2 ? vec2(p.z, p.y) : vec2(p.x, p.y);
        float fw = fwidth(uv.y) * 1.2 + 0.02;
        float base = (L == 3 ? 36.0 : 24.0) + (pine == 1 ? -10.0 : 0.0);
        a = crowns(uv, base, pine == 1 ? 8.0 : 10.0, float(L) * 13.7, fw, pine);
        kind = 1.0; seed = float(L);
    } else if (L == 4) {
        if (J <= 0.01) discard;
        float fw = fwidth(uv.y) * 1.2 + 0.02;
        a = crowns(uv, -12.0 + 33.0 * smoothstep(0.0, 1.0, J), 7.0, 41.0, fw, 0);
        kind = 1.0; seed = 7.0;
    } else if (L == 5) {
        if (J <= 0.01) discard;
        float fw = fwidth(uv.y) * 1.2 + 0.01;
        a = crowns(vec2(p.x * 1.6, p.y), -6.0 + 12.0 * J, 5.0, 13.0, fw, 0) * step(10.0 * (1.0 - J), abs(p.x) + 6.0 * J);
        kind = 2.0; seed = 8.0;
    } else if (L <= 10) {
        int i = L - 6;
        float jg = i == 0 ? 0.0 : J;   // (the nearest row stays low: the camera stands by it)
        if (abs(p.x) < 10.2 * (1.0 - jg) || abs(p.x) > 41.0) discard;
        float fw = fwidth(uv.y) * 1.2 + 0.005;
        a = pine == 1 ? crowns(uv, 1.1 + 4.0 * jg, 1.2 + 2.0 * jg, float(i) * 5.3, fw, 0)
                      : ferns(uv, 0.9 + 3.6 * jg, 1.5 + 2.5 * jg, float(i) * 3.1, fw);
        a *= 1.0 - 0.6 * uPrime * step(abs(p.x), 14.0);
        kind = 2.0; seed = float(i);
    } else if (L <= 12) {
        int i = L - 11;
        uv = vec2(p.z, p.y);
        float fw = fwidth(uv.y) * 1.2 + 0.005;
        a = ferns(uv, 0.5 + 3.0 * J, 1.0 + 2.0 * J, 7.0 + float(i) * 2.3, fw) * (1.0 - uPrime);
        kind = 2.0; seed = 5.0 + float(i);
    } else if (L <= 14) {
        if (pine == 1) discard;
        int i = L - 13;
        if (abs(p.x) < 11.0 * (1.0 - J) || abs(p.x) > 40.0 || p.y > CAN_Y) discard;
        float fw = fwidth(uv.y) * 1.2 + 0.005;
        a = lianas(uv, float(i) * 4.7, fw);
        kind = 3.0; seed = float(i);
    } else {
        if (uKind == 2) discard;
        int i = L - 15;
        if (i == 1 && abs(p.x) < 24.0) discard;
        float fw = fwidth(uv.y) * 1.2 + 0.005;
        float u = p.x;
        float bottom = (i == 0 ? 15.5 : 21.0) - 8.0 * J + 3.5 * fbm2(vec2(u * 0.07 + float(i) * 5.0, 0.5));
        // hanging leaf clumps and trailing fronds
        float cu = u * 0.35 + float(i) * 3.0;
        float ci = floor(cu), cf = fract(cu) - 0.5;
        bottom -= 2.2 * hash1(ci) * sqrt(max(0.0, 0.25 - cf * cf)) * 2.0;
        bottom += 0.45 * (vnoise(vec2(u, p.y) * 2.4 + float(i)) - 0.5) + 0.12 * (vnoise(vec2(u, p.y) * 9.0) - 0.5);
        a = smoothstep(-fw, fw, p.y - bottom);
        kind = 4.0; seed = float(i);
    }
    if (a < 0.02) discard;
    float shade = kind == 4.0 ? 1.0 : mix(1.0, shadowAt(p, -rd), 0.75);
    vec3 c = foliageCol(kind, seed, p, uv, rd, shade);
    frag = vec4(max(c, 0.0), sat(a * 1.15));
}
)";

// ---- mist and light shafts along each ray to the depth (and a little ambient occlusion from the depth), at a
// reduced resolution: soft, upsampled with the depth
inline const char* VOL_FS = R"(#version 330 core
in vec2 vUv;
layout(location = 0) out vec4 fragVol; // in-scattered light, transmittance
layout(location = 1) out vec4 fragAux; // ambient occlusion, the distance along the ray
uniform sampler2D uDepth;
uniform vec2 uFull;     // the depth's size
uniform vec2 uNF;       // near, far
float distAt(vec2 uv) {
    float d = texture(uDepth, uv).r;
    if (d >= 0.99999) return 1e6;
    float z = d * 2.0 - 1.0;
    float ze = 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x));
    return ze / dot(rayDir(uv), uFwd);
}
vec3 posAt(vec2 uv) { return uEye + rayDir(uv) * distAt(uv); }
void main() {
    vec2 uv = vUv;
    vec3 rd = rayDir(uv);
    float tEnd = distAt(uv);
    // ---- ambient occlusion (a few samples around, in the depth)
    float ao = 1.0;
    if (tEnd < 200.0) {
        vec3 P = uEye + rd * tEnd;
        vec2 px = 1.0 / uFull;
        vec3 px1 = posAt(uv + vec2(px.x, 0.0)), px0 = posAt(uv - vec2(px.x, 0.0));
        vec3 py1 = posAt(uv + vec2(0.0, px.y)), py0 = posAt(uv - vec2(0.0, px.y));
        vec3 dx = abs(dot(px1 - P, rd)) < abs(dot(P - px0, rd)) ? px1 - P : P - px0;
        vec3 dy = abs(dot(py1 - P, rd)) < abs(dot(P - py0, rd)) ? py1 - P : P - py0;
        vec3 N = normalize(cross(dx, dy));
        if (dot(N, rd) > 0.0) N = -N;
        float rad = 1.1;
        float rpx = rad / (tEnd * 2.0 * uTanH);  // (in uv, vertically)
        float occ = 0.0;
        float jit = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) * 6.2831;
        for (int i = 0; i < 10; i++) {
            float fi = float(i);
            float r = (fi + 0.5) / 10.0;
            float an = jit + fi * 2.39996;
            vec2 o = vec2(cos(an), sin(an)) * r * rpx;
            o.x /= uAspect;
            vec3 S = posAt(uv + o);
            vec3 v = S - P;
            float vv = dot(v, v);
            occ += max(dot(v, N) - 0.02 * tEnd * 0.02, 0.0) / (vv + 0.08) * smoothstep(rad * rad * 4.0, rad * rad, vv);
        }
        ao = sat(1.0 - occ * 0.22);
    }
    // ---- mist and light shafts along the ray (the canopy's light at each sample)
    float tm = min(tEnd, 170.0);
    const int N = 24;
    float dt = tm / float(N);
    float jit = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float mu = dot(rd, uSunDir);
    const float g = 0.45;
    float hg = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * mu, 1.5) * 0.12;
    float phase = 0.05 + hg;
    vec3 scat = vec3(0.0);
    float trans = 1.0;
    for (int i = 0; i < N; i++) {
        vec3 p = uEye + rd * (dt * (float(i) + jit));
        float y = max(p.y, 0.0);
        float mist = uMist * 0.6 * exp(-y / 2.0) * (0.3 + 1.2 * vnoise(p.xz * 0.07 + vec2(uTime * 0.045, uTime * 0.017)));
        float dens = uFogK * (exp(-y / 16.0) + mist);
        float sun = sunThrough(p);
        vec3 Ls = uSunCol * sun * sun * phase * uShaft * 2.2 + uAmb * 0.4;
        float a = dens * dt;
        scat += trans * a * Ls * uFog;
        trans *= exp(-a);
    }
    fragVol = vec4(scat, trans);
    fragAux = vec4(ao, min(tEnd, 1e5), 0.0, 1.0);
}
)";

// ---- the composite (full resolution): the geometry, its occlusion and the mist upsampled with the depth, the
// braziers' flames, the bridge
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uColor, uDepth, uVol, uAux;
uniform vec2 uNF, uLow;
float distAt(vec2 uv) {
    float d = texture(uDepth, uv).r;
    if (d >= 0.99999) return 1e5;
    float z = d * 2.0 - 1.0;
    float ze = 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x));
    return ze / dot(rayDir(uv), uFwd);
}
void main() {
    vec2 uv = vUv;
    vec3 col = texture(uColor, uv).rgb;
    float t = distAt(uv);
    // the low-resolution terms, from the texels whose distance matches this pixel's
    vec2 f = uv * uLow - 0.5;
    vec2 i0 = floor(f), w = f - i0;
    vec4 vol = vec4(0.0);
    float ao = 0.0, ws = 0.0;
    for (int k = 0; k < 4; k++) {
        vec2 o = vec2(float(k & 1), float(k >> 1));
        vec2 tc = (i0 + o + 0.5) / uLow;
        vec4 aux = texture(uAux, tc);
        float bw = (o.x > 0.5 ? w.x : 1.0 - w.x) * (o.y > 0.5 ? w.y : 1.0 - w.y);
        float dw = exp(-abs(log(max(aux.g, 0.05)) - log(max(t, 0.05))) * 12.0);
        float ww = bw * dw + 1e-5 * bw;
        vol += texture(uVol, tc) * ww;
        ao += aux.r * ww;
        ws += ww;
    }
    vol /= ws;
    ao /= ws;
    if (t < 1e4) col *= mix(1.0, ao, 0.8);
    col = col * vol.a + vol.rgb;
    // the braziers' flames
    vec3 rd = rayDir(uv);
    if (uPrime > 0.01) {
        for (int i = 0; i < 4; i++) {
            vec3 fp = BRZ[uKind * 4 + i] + vec3(0.0, 1.55 + 0.35 * uPrime - 1.4 * (1.0 - uPrime), 0.0);
            float tc = dot(fp - uEye, rd);
            if (tc < 0.0 || tc > t + 0.5) continue;
            float dd = length(uEye + rd * tc - fp);
            float fl = 0.85 + 0.15 * sin(uTime * 9.0 + float(i) * 2.0) * sin(uTime * 5.3 + float(i));
            float px = max(tc * uPx, 0.02);
            col += vec3(3.0, 1.4, 0.4) * uPrime * fl * (exp(-sq(dd / max(0.32, px))) + 0.08 * exp(-dd * 0.9));
        }
    }
    col = mix(uBridgeCol, col, uWorldA);
    frag = vec4(max(col, 0.0), 1.0);
}
)";

// ------------------------------------------------------------------------------------------------ sprites
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
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = sat(dot(pa, ba) / dot(ba, ba)); return length(pa - ba * h); }
void main() {
    int mode = int(vA.w + 0.5);
    vec2 q = vLocal;
    float r = length(q);
    vec3 c = vB.rgb;
    float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
    if (mode == 2) {
        // a stone chip: an irregular angular fragment, lit from the top left
        vec2 p = rot(vC.x) * q;
        float k = vC.y;
        float d = max(max(dot(p, normalize(vec2(1.0, 0.3 + k))), dot(p, normalize(vec2(-0.8, 1.0)))),
                      max(dot(p, normalize(vec2(-0.5 - k, -1.0))), dot(p, normalize(vec2(0.9, -0.7))))) - 0.55;
        float a = 1.0 - smoothstep(-px * 1.5, px * 1.5, d);
        float lit = 0.75 + 0.35 * sat(dot(normalize(p + vec2(0.001)), vec2(-0.6, 0.8)) * (1.0 - sat(-d / 0.3)));
        frag = vec4(c * lit * a * vB.a, a * vB.a);
        return;
    }
    if (mode == 3) {
        // a bird: body and two beating wings, a dark silhouette against the light
        float ph = vC.x;
        float wy = sin(ph) * 0.5;
        float d = length(q * vec2(1.0, 2.2)) - 0.16;
        for (int s = -1; s <= 1; s += 2) {
            vec2 tip = vec2(float(s) * 0.92, wy + 0.08);
            vec2 mid = vec2(float(s) * 0.45, 0.12 + wy * 0.35 + 0.06);
            d = min(d, min(sdSeg(q, vec2(0.0, 0.03), mid) - 0.09, sdSeg(q, mid, tip) - 0.035));
        }
        float a = 1.0 - smoothstep(-px * 1.2, px * 1.2, d);
        frag = vec4(c * a * vB.a, a * vB.a);
        return;
    }
    if (mode == 4) {
        // a butterfly seen from the front: two pairs of wings folding and opening
        float open = 0.25 + 0.75 * abs(cos(vC.x));
        vec2 p = vec2(abs(q.x) / max(open, 0.08), q.y);
        float up = length((p - vec2(0.45, 0.22)) * vec2(1.0, 1.25)) - 0.36;
        float lo = length((p - vec2(0.32, -0.3)) * vec2(1.2, 1.0)) - 0.24;
        float d = min(min(up, lo), length(q * vec2(5.0, 1.0)) - 0.3);
        float a = 1.0 - smoothstep(-px * 1.5 / max(open, 0.3), px * 1.5 / max(open, 0.3), d);
        vec3 col = mix(c, c * 0.35, smoothstep(-0.12, 0.0, max(up, lo) + 0.06));
        frag = vec4(col * a * vB.a, a * vB.a);
        return;
    }
    // a puff of dust
    float v = exp(-r * r * 4.0) * smoothstep(1.0, 0.6, r) * (0.7 + 0.3 * vnoise(q * 3.0 + vC.x * 9.0));
    frag = vec4(c * v * vB.a, v * vB.a);
}
)";

// ------------------------------------------------------------------------------------------------ the backing
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
    float a = uWellA * (1.0 - smoothstep(-0.1, 0.6, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.6 + exp(-abs(dw) * 4.0) * 0.06;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ------------------------------------------------------------------------------------------------ pieces: drums
// Each cell one carved stone column drum seen from the front: a clean silhouette (square, its corners worn), the
// cylinder's roundness in its shading (lit from the top left), shallow vertical fluting, a carved band near its
// top, the joint lines top and bottom, the stone's own grain and pits, a touch of moss low in the flutes now and
// then, and a soft contact shadow. A cleared drum cracks and crumbles away, piece by piece.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, seed
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 drum, 1 ghost), erase 0..1, dull, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
const float EXT = 1.2;
void main() {
    vec2 wp = aA.xy + aPos * aA.z * EXT;
    vLocal = aPos * 0.5 * EXT;
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uBridge;
uniform vec3 uBridgeInk, uLightC;
float sdBoxR(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
// cracks: the distance to the borders of a few fragments, and the fragment's id
vec3 voro(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float d1 = 9.0, d2 = 9.0;
    vec2 best = vec2(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 o = hash2(i + g) * 0.8 + 0.1;
            float d = length(g + o - f);
            if (d < d1) { d2 = d1; d1 = d; best = i + g; }
            else if (d < d2) d2 = d;
        }
    return vec3(d2 - d1, best);
}

// ---- the glyph blocks
const vec3 LD = vec3(-0.5145, 0.5613, 0.6489); // normalize(-0.55, 0.6, 0.7)
float sdBox(vec2 p, vec2 b) { vec2 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0); return length(pa - ba * h); }
// a relief glyph per type: the distance to its strokes' center lines
float glyph(vec2 p, int t) {
    float d = 9.0;
    if (t == 0) { // three bars (the Maya numeral's)
        for (int i = -1; i <= 1; i++) d = min(d, sdSeg(p, vec2(-0.19, float(i) * 0.12), vec2(0.19, float(i) * 0.12)));
    } else if (t == 1) { // sun disc: a ring and its dot
        d = min(abs(length(p) - 0.18), max(length(p) - 0.035, 0.0));
    } else if (t == 2) { // the Kan cross
        d = min(sdSeg(p, vec2(0.0, -0.2), vec2(0.0, 0.2)), sdSeg(p, vec2(-0.2, 0.0), vec2(0.2, 0.0)));
        d = min(d, max(abs(p.x) + abs(p.y) - 0.06, 0.0));
    } else if (t == 3) { // spiral
        float r = length(p), a = atan(p.y, p.x) / 6.2832;
        float k = 0.075;
        float f = r / k - a;
        d = abs(fract(f) - 0.5) * k;
        d = max(d, r - 0.215);
        d = min(d, max(r - 0.03, 0.0));
    } else if (t == 4) { // the serpent: two waves of water
        for (int i = -1; i <= 1; i += 2) {
            float yc = float(i) * 0.08;
            float x = clamp(p.x, -0.2, 0.2);
            float dw = abs(p.y - yc - sin(x * 23.5) * 0.045) / sqrt(1.0 + pow(cos(x * 23.5) * 1.06, 2.0));
            vec2 e = vec2(x, yc + sin(x * 23.5) * 0.045);
            d = min(d, abs(p.x) <= 0.2 ? dw : length(p - e));
        }
    } else if (t == 5) { // a stepped pyramid
        const vec2 P[13] = vec2[](vec2(-0.23, -0.17), vec2(-0.23, -0.06), vec2(-0.13, -0.06), vec2(-0.13, 0.05),
            vec2(-0.05, 0.05), vec2(-0.05, 0.17), vec2(0.05, 0.17), vec2(0.05, 0.05), vec2(0.13, 0.05),
            vec2(0.13, -0.06), vec2(0.23, -0.06), vec2(0.23, -0.17), vec2(-0.23, -0.17));
        for (int i = 0; i < 12; i++) d = min(d, sdSeg(p, P[i], P[i + 1]));
    } else { // the eye
        float v = max(length(p - vec2(0.0, -0.16)) - 0.3, length(p - vec2(0.0, 0.16)) - 0.3);
        d = min(abs(v), max(length(p) - 0.045, 0.0));
    }
    return d;
}
float glyphH(vec2 p, int t) {
    float g = glyph(p, t) - 0.026;
    float fr = abs(sdBox(p, vec2(0.35))) - 0.01;
    return smoothstep(0.016, -0.008, min(g, fr)); // 1 in the carving
}
void main() {
    int mode = int(vC.x + 0.5);
    float erase = vC.y;
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    vec2 q = vLocal;
    vec2 sd = vA.xy * vec2(0.173, 0.291) + vA.w;
    // the silhouette: a square block, its corners worn (each its own)
    float wear = 0.06 + 0.05 * hash(sd * 7.0);
    float d = sdBoxR(q, vec2(0.465, 0.465), wear);
    d += 0.012 * (vnoise(q * 16.0 + sd * 31.0) - 0.5);
    vec3 t = vB.rgb;
    if (mode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.02 + aa * 1.5, abs(d + 0.035));
        float carving = glyphH(q, int(vC.w + 0.5)) * 0.3;
        float a = (ring * 0.8 + carving + step(d, 0.0) * 0.07) * vB.a;
        vec3 c = mix(mix(t, vec3(1.0), 0.35), uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // crumbling: cracks open, then the fragments fall away one after the other
    float frag_ = 1.0;
    float crack = 0.0;
    if (erase > 0.0) {
        vec3 v = voro(q * 3.2 + sd * 5.0);
        float delay = hash(v.yz + sd) * 0.5;
        float k = sat((erase - delay) / 0.5);
        crack = 1.0 - smoothstep(0.0, 0.05 + 0.25 * erase, v.x);
        frag_ = smoothstep(k * 0.42, k * 0.42 + 0.03, v.x) * step(k, 0.98);
    }
    float inside = 1.0 - smoothstep(-aa, aa, d);
    float ds = sdBoxR(q - vec2(0.04, -0.055), vec2(0.465), wear);
    float shadow = (1.0 - smoothstep(-0.02, 0.09, ds)) * (1.0 - inside) * 0.55 * (1.0 - smoothstep(0.0, 0.3, erase));
    // a pale block of the variant's stone, its worn edge, a frame and one glyph per type cut into its face, a deeper
    // pigment in the cuts
    int ty = int(vC.w + 0.5);
    vec2 kq = abs(q) - vec2(0.3);
    vec2 out_ = sign(q) * ((kq.x > 0.0 && kq.y > 0.0) ? normalize(kq) : (kq.x > kq.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0)));
    float e = 0.006;
    float h0 = glyphH(q, ty);
    float hx = glyphH(q + vec2(e, 0.0), ty), hy = glyphH(q + vec2(0.0, e), ty);
    vec3 n = normalize(vec3((hx - h0) / e * 0.025, (hy - h0) / e * 0.025, 1.0));
    n = normalize(n + vec3(out_ * smoothstep(-0.07, 0.0, d) * 0.9, 0.0));
    n.xy += (vec2(fbm(q * 9.0 + sd * 11.0 + 0.03), fbm(q * 9.0 + sd * 11.0 + 7.0)) - 0.5) * 0.25;
    n = normalize(n);
    float lit = max(dot(n, LD), 0.0);
    float h = fbm(q * 5.0 + sd * 17.0);
    vec3 c = t * mix(0.88, 1.08, smoothstep(0.3, 0.75, h));
    c = mix(c, t * t * 1.25, h0 * 0.75);
    c *= (0.48 + 0.66 * lit) * (1.0 - 0.25 * h0);
    float pit = step(0.9, hash(floor((q + sd) * 30.0))) * smoothstep(0.4, 0.15, length(fract((q + sd) * 30.0) - 0.5));
    c *= 1.0 - 0.25 * pit;
    c = mix(c, c * 0.5, 1.0 - smoothstep(0.0, 0.014 + aa, -d));
    c *= 1.0 - 0.6 * crack;
    c = mix(c, c * 0.45, smoothstep(0.2, 1.0, erase));
    float dull = vC.z;
    c = mix(c, vec3(dot(c, vec3(0.3, 0.5, 0.2))) * 0.5, dull);
    c = mix(c, uBridgeInk, uBridge);
    float a = inside * vB.a * frag_;
    float sa = shadow * vB.a * (1.0 - uBridge);
    frag = vec4(c * a, a + sa * (1.0 - a));
}
)";

// ------------------------------------------------------------------------------------------------ lettering
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
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount): the field as baked then (64 px, 8 px
// of reach); the fill's coverage and the halo's shaped as linear blending shaped them (light over dark, dark over
// light).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = pow(smoothstep(0.47 - w, 0.47 + w, d), 1.0 / 2.2);
    float halo = 1.0 - pow(1.0 - smoothstep(0.15, 0.45, d) * uP0.a, 1.0 / 2.2);
    float a = max(fill, halo);
    return vec4(L.ink.rgb * fill + uP0.rgb * (a - fill), a) * L.ink.a;
}
)";

// ------------------------------------------------------------------------------------------------ post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.32 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.7, 0.0) * 0.85);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace ruins_shaders

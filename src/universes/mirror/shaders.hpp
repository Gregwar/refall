#pragma once
// MIRROR's shaders (see mirror.cpp).
//
// ROOM_FS traces the room per pixel (sphere tracing a signed distance field: the infinity cove, a step and a pillar
// far away): a soft key light (soft shadows, the glass stack's colored shadow and caustics projected through the
// board, the light the mirrors throw back into the room), two colored accent lights with long falloffs, a hemisphere
// ambient with ambient occlusion, a glossy sheen on the floor and a haze lit by the accents. The same shader draws the
// mirrors (sampling their reflection passes), the smoked glass panel behind the well, and, in a reflection pass,
// the room seen in a mirror (the ray reflected in the mirror's plane). CUBE_VS / CUBE_FS draw the glass cubes
// (a rounded-edge cube mesh, instanced): refraction (a soft offset of the room behind), absorption in the tint over
// the path through the cube, Fresnel reflections of the room's lights, the back edges seen through the glass.
// FINAL tone maps, adds a faint bloom and grain, and encodes the frame.
#include <string>

namespace mirror_shaders {

// ---------------------------------------------------------------- common: lights, the room's look seen from glass
inline const char* COMMON = R"(
uniform vec3 uKeyDir, uKeyCol;
uniform vec3 uAccPos[2], uAccCol[2];
uniform vec3 uAmbTop, uAmbBot, uAlbedo, uHaze;
uniform vec3 uPanelMin, uPanelMax, uPanelCol;
uniform float uPanelTrans;
uniform vec3 uEye;
uniform vec2 uRes;
const float FY = -10.02;
// the room's light as glass sees it (a reflection): the softbox, the accents' glows, the floor and walls
vec3 env(vec3 p, vec3 d) {
    float up = d.y;
    vec3 wall = uAlbedo * (uAmbTop * 0.9 + uKeyCol * 0.25);
    vec3 floorC = uAlbedo * (uAmbBot + uKeyCol * 0.35);
    vec3 c = mix(floorC, wall, smoothstep(-0.25, 0.1, up));
    c = mix(c, uHaze * 0.6, smoothstep(0.2, 0.9, up));
    float k = max(dot(d, uKeyDir), 0.0);
    c += uKeyCol * (smoothstep(0.86, 0.97, k) * 2.6 + pow(k, 6.0) * 0.25);
    for (int i = 0; i < 2; i++) {
        vec3 l = normalize(uAccPos[i] - p);
        float a = max(dot(d, l), 0.0);
        c += uAccCol[i] * (pow(a, 24.0) * 0.5 + pow(a, 4.0) * 0.08);
    }
    return c;
}
float sq(float x) { return x * x; }
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
)";

// ---------------------------------------------------------------- the room
inline const char* ROOM_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform float uTanH, uAspect;
uniform int uReflect;            // 1: a reflection pass (the ray reflected in the plane uPN . x = uPD)
uniform vec3 uPN;
uniform float uPD;
uniform sampler2D uBoard;        // the board's glass, 10 x 20 (rgb: the tint's transmission, a: a cube there)
uniform float uBoardOn;
uniform float uHazeK;
uniform vec3 uPoolC;
uniform float uPoolR;
// the mirrors (up to 3)
uniform int uMirCount;
uniform vec3 uMirC[3], uMirR[3], uMirU[3], uMirN[3];
uniform vec2 uMirHalf;
uniform float uMirA[3], uMirSheen[3], uMirGlint[3];
uniform sampler2D uMirTex0, uMirTex1, uMirTex2;
uniform vec3 uMirTint;
uniform float uThrow;            // how bright the light thrown back by the mirrors is
uniform vec3 uBridgeCol;
uniform float uBridge, uGrain;

float sdBox(vec3 p, vec3 b, float r) { vec3 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r; }
float sdRound2(vec2 p, vec2 b, float r) { vec2 q = abs(p) - b + r; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; }
// the room: a cove (the floor curving into the walls, the walls rounding into each other), open toward the camera
float room(vec3 p) {
    float w = -sdRound2(p.xz - vec2(0.0, 22.0), vec2(42.0, 58.0), 18.0); // distance to the walls (inside > 0)
    vec2 q = vec2(w, p.y - FY);
    const float R = 9.0;
    float d = (q.x < R && q.y < R) ? R - length(vec2(R) - q) : min(q.x, q.y);
    return d;
}
float vols(vec3 p) {
    float s = sdBox(p - vec3(25.0, FY + 1.1, -25.0), vec3(7.5, 1.1, 3.2), 0.12);       // a low step
    float s2 = sdBox(p - vec3(27.5, FY + 3.3, -28.5), vec3(5.0, 1.1, 2.0), 0.12);     // its second tread
    float c = sdBox(p - vec3(-27.0, FY + 13.0, -31.0), vec3(1.5, 13.0, 1.5), 0.1);      // a pillar
    return min(min(s, s2), c);
}
float scene(vec3 p) { return min(room(p), vols(p)); }
vec3 nrm(vec3 p, float e) {
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * scene(p + k.xyy * e) + k.yyx * scene(p + k.yyx * e) + k.yxy * scene(p + k.yxy * e) + k.xxx * scene(p + k.xxx * e));
}
float softShadow(vec3 ro, vec3 rd) {
    float res = 1.0, t = 0.15;
    for (int i = 0; i < 22; i++) {
        float h = vols(ro + rd * t);
        res = min(res, 7.0 * h / t);
        t += clamp(h, 0.3, 4.0);
        if (res < 0.01 || t > 60.0) break;
    }
    return clamp(res, 0.0, 1.0);
}
float ao(vec3 p, vec3 n) {
    float o = 0.0, w = 1.0;
    for (int i = 1; i <= 4; i++) {
        float h = 0.9 * float(i);
        o += w * (h - scene(p + n * h));
        w *= 0.6;
    }
    return clamp(1.0 - 0.09 * o, 0.0, 1.0);
}
// the light reaching p from the key through the board's glass and the smoked panel: colored shadows, caustics
vec3 throughGlass(vec3 p) {
    vec3 T = vec3(1.0);
    if (uKeyDir.z <= 0.01) return T;
    // the panel
    float sp = (uPanelMin.z - p.z) / uKeyDir.z;
    if (sp > 0.0) {
        vec3 q = p + uKeyDir * sp;
        float blur = 0.08 + sp * 0.03;
        float inX = smoothstep(uPanelMin.x - blur, uPanelMin.x + blur, q.x) * (1.0 - smoothstep(uPanelMax.x - blur, uPanelMax.x + blur, q.x));
        float inY = smoothstep(uPanelMin.y - blur, uPanelMin.y + blur, q.y) * (1.0 - smoothstep(uPanelMax.y - blur, uPanelMax.y + blur, q.y));
        T *= mix(1.0, 0.8, inX * inY);
    }
    if (uBoardOn < 0.5) return T;
    float s = (-0.45 - p.z) / uKeyDir.z;
    if (s <= 0.0) return T;
    vec3 q = p + uKeyDir * s;
    vec2 cell = vec2(q.x + 5.0, q.y + 10.0); // 0..10, 0..20
    if (cell.x < -1.0 || cell.x > 11.0 || cell.y < -1.0 || cell.y > 21.0) return T;
    float blur = 0.04 + s * 0.03;
    vec3 acc = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < 5; i++) {
        vec2 o = (i == 0) ? vec2(0.0) : vec2(i == 1 ? 1.0 : i == 2 ? -1.0 : 0.0, i == 3 ? 1.0 : i == 4 ? -1.0 : 0.0);
        vec2 c = cell + o * blur;
        vec4 b = texture(uBoard, c / vec2(10.0, 20.0));
        vec2 f = fract(c);
        // a glass cube: its tint, brighter at the middle (the light focused), a caustic line inside each edge
        float e = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
        float line = exp(-sq((e - 0.1) / 0.035)) * 1.4 + 0.12 * smoothstep(0.5, 0.15, length(f - 0.5));
        float edgeDark = smoothstep(0.0, 0.07, e);
        vec3 g = b.rgb * (0.55 + 0.45 * edgeDark) * (1.0 + line);
        acc += mix(vec3(1.0), g, b.a);
        wsum += 1.0;
    }
    return T * acc / wsum;
}
// the key light thrown back by mirror i onto p
vec3 thrown(vec3 p, vec3 n, int i) {
    vec3 mn = uMirN[i];
    if (dot(uKeyDir, mn) <= 0.0) return vec3(0.0);
    vec3 Lr = reflect(-uKeyDir, mn); // the light leaving the mirror
    float side = dot(p - uMirC[i], mn);
    if (side <= 0.05) return vec3(0.0);
    float den = dot(Lr, mn);
    if (den <= 0.01) return vec3(0.0);
    float s = side / den; // back along -Lr to the mirror
    vec3 q = p - Lr * s - uMirC[i];
    vec2 uv = vec2(dot(q, uMirR[i]), dot(q, uMirU[i]));
    float d = sdRound2(uv, uMirHalf, 0.9);
    float soft = 0.25 + s * 0.05;
    float m = 1.0 - smoothstep(-soft, soft, d);
    return uKeyCol * m * max(dot(n, -Lr), 0.0) * uMirA[i];
}
vec3 shade(vec3 p, vec3 rd, float t) {
    vec3 n = nrm(p, 0.002 * t + 0.01);
    bool isVol = vols(p) < room(p);
    vec3 alb = uAlbedo * (isVol ? 1.04 : 1.0);
    float occ = ao(p, n);
    // the key: a soft area light (a broad pool around the well, long gradients up the walls)
    float pool = exp(-sq(length(p.xz - uPoolC.xz) / uPoolR));
    float hfall = 1.0 - 0.45 * smoothstep(-4.0, 26.0, p.y);
    vec3 key = uKeyCol * max(dot(n, uKeyDir), 0.0) * softShadow(p + n * 0.05, uKeyDir) * throughGlass(p) * (0.45 + 0.75 * pool) * hfall;
    vec3 c = alb * key;
    c += alb * mix(uAmbBot, uAmbTop, n.y * 0.5 + 0.5) * occ * (0.8 + 0.3 * pool);
    for (int i = 0; i < 2; i++) {
        vec3 l = uAccPos[i] - p;
        float d2 = dot(l, l);
        l *= inversesqrt(d2);
        float fall = 1.0 / (1.0 + d2 / 420.0);
        c += alb * uAccCol[i] * (max(dot(n, l), 0.0) * 0.85 + 0.15) * fall * (0.55 + 0.45 * occ);
        // a soft sheen on the floor toward the light
        if (n.y > 0.8) c += uAccCol[i] * pow(max(dot(reflect(rd, n), l), 0.0), 14.0) * fall * 0.35;
    }
    if (n.y > 0.8) c += uKeyCol * pow(max(dot(reflect(rd, n), uKeyDir), 0.0), 10.0) * 0.06 * pool;
    for (int i = 0; i < 3; i++)
        if (i < uMirCount) c += alb * thrown(p, n, i) * uThrow;
    return c;
}
vec3 haze(vec3 c, vec3 ro, vec3 rd, float t) {
    float f = 1.0 - exp(-t * uHazeK);
    vec3 h = uHaze * (1.0 - 0.35 * smoothstep(0.0, 0.6, rd.y));
    c = mix(c, h, f);
    for (int i = 0; i < 2; i++) {
        vec3 lp = uAccPos[i] - ro;
        float tc = dot(lp, rd);
        float hh = length(lp - rd * tc);
        const float R = 9.0;
        float a = sqrt(R * R + hh * hh);
        float g = R * R / a * (atan((t - tc) / a) + atan(tc / a));
        c += uAccCol[i] * g * uHazeK * 0.22;
    }
    return c;
}
vec3 mirrorLook(sampler2D tex, int i, vec3 ro, vec3 rd, float tm, vec2 uv, float aa, out float cover) {
    vec3 p = ro + rd * tm - uMirC[i];
    vec2 m = vec2(dot(p, uMirR[i]), dot(p, uMirU[i]));
    float d = sdRound2(m, uMirHalf, 0.9);
    cover = 1.0 - smoothstep(-aa, aa, d);
    vec3 col;
    if (dot(rd, uMirN[i]) < 0.0) {
        col = texture(tex, gl_FragCoord.xy / uRes).rgb * uMirTint;
        // the polished edge: a thin bright line round it, the sheen of a clear sweeping across it
        float rim = 1.0 - smoothstep(0.0, 0.07 + aa, abs(d + 0.05));
        col = mix(col, env(uMirC[i], reflect(rd, uMirN[i])) * 1.2 + uKeyCol * 0.25, rim * 0.85);
        float sw = uMirSheen[i];
        if (sw > 0.0) {
            float x = (m.x + m.y * 0.35) / uMirHalf.x;
            float pos = mix(-2.2, 2.2, sw);
            col += uKeyCol * exp(-sq((x - pos) / 0.35)) * 0.5 * sin(sw * 3.14159);
        }
        col += uKeyCol * rim * uMirGlint[i];
    } else {
        col = uAlbedo * uAmbTop * 0.35; // its back: a dark, matt backing
    }
    return col;
}
void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec3 ro = uEye;
    vec3 rd = normalize(vec3(ndc.x * uTanH * uAspect, ndc.y * uTanH, -1.0));
    float t0 = 0.0;
    if (uReflect == 1) {
        float den = dot(rd, uPN);
        if (abs(den) < 1e-4) { frag = vec4(uHaze, 0.0); return; }
        t0 = (uPD - dot(ro, uPN)) / den;
        if (t0 < 0.0) { frag = vec4(uHaze, 0.0); return; }
        vec3 hit = ro + rd * t0;
        ro = hit;
        rd = reflect(rd, uPN);
        t0 = 0.0;
    }
    float t = t0 + 0.01;
    bool hit = false;
    for (int i = 0; i < 90; i++) {
        float d = scene(ro + rd * t);
        if (d < 0.0015 * t) { hit = true; break; }
        t += d;
        if (t > 300.0) break;
    }
    vec3 c;
    if (hit) c = shade(ro + rd * t, rd, t);
    else { t = 300.0; c = uHaze; }
    c = haze(c, ro, rd, t);
    // the mirrors (main pass only)
    vec3 cRoom = c;
    float mirCover = 0.0;
    if (uReflect == 0) {
        float aa = 1.5 * uTanH * 2.0 / uRes.y;
        for (int i = 0; i < 3; i++) {
            if (i >= uMirCount || uMirA[i] <= 0.001) continue;
            float den = dot(rd, uMirN[i]);
            if (abs(den) < 1e-4) continue;
            float tm = dot(uMirC[i] - ro, uMirN[i]) / den;
            if (tm <= 0.0 || tm >= t) continue;
            float cover;
            vec3 mc = i == 0 ? mirrorLook(uMirTex0, 0, ro, rd, tm, vUv, aa * tm, cover)
                    : i == 1 ? mirrorLook(uMirTex1, 1, ro, rd, tm, vUv, aa * tm, cover)
                             : mirrorLook(uMirTex2, 2, ro, rd, tm, vUv, aa * tm, cover);
            mc = haze(mc, ro, rd, tm);
            c = mix(c, mc, cover * uMirA[i]);
            mirCover = max(mirCover, cover * uMirA[i]);
            if (cover > 0.5) t = mix(t, tm, uMirA[i]);
        }
    }
    // the smoked glass panel behind the well
    if (uPanelTrans < 1.0 && abs(rd.z) > 1e-4) {
        float tp = (uPanelMin.z - ro.z) / rd.z;
        if (tp > 0.0 && tp < t) {
            vec3 q = ro + rd * tp;
            float px = 1.5 * uTanH * 2.0 / uRes.y * tp;
            vec2 lo = q.xy - uPanelMin.xy, hi = uPanelMax.xy - q.xy;
            float e = min(min(lo.x, hi.x), min(lo.y, hi.y));
            float inside = smoothstep(-px, px, e);
            vec3 lit = throughGlass(q + vec3(0.0, 0.0, -0.001));
            vec3 pc = uPanelCol * (0.75 + 0.25 * lit);
            // (a mirror behind the panel is mostly hidden: its reflections never mingle with the stack)
            vec3 pcol = mix(c, cRoom, mirCover * 0.8) * uPanelTrans + pc;
            // its polished rim
            float rim = (1.0 - smoothstep(0.0, 0.05 + px, abs(e - 0.02))) * step(-px, e);
            pcol += (uKeyCol * 0.18 + uAmbTop * 0.25) * rim * (q.y > uPanelMax.y - 0.2 ? 1.6 : 0.8);
            c = mix(c, pcol, inside);
        }
    }
    c = mix(c, uBridgeCol, uBridge);
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- glass cubes
inline const char* CUBE_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;     // the unit rounded cube ([-1, 1])
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iPos;     // center, half size
layout(location = 3) in vec4 iRot;     // quaternion
layout(location = 4) in vec4 iTint;    // transmission tint, kind (0 stack, 1 ghost, 2 falling, 3 preview, 4 shard)
layout(location = 5) in vec4 iFx;      // alpha, flash, frost, seed
uniform mat4 uVP;
uniform int uClip;
uniform vec3 uPN;
uniform float uPD;
out vec3 vWorld, vNrm, vLocal;
flat out vec4 vRot, vTint, vFx;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
void main() {
    vec3 w = iPos.xyz + qrot(iRot, aPos * iPos.w);
    vWorld = w;
    vNrm = qrot(iRot, aNrm);
    vLocal = aPos;
    vRot = iRot;
    vTint = iTint;
    vFx = iFx;
    gl_ClipDistance[0] = uClip == 1 ? dot(w, uPN) - uPD : 1.0;
    gl_Position = uVP * vec4(w, 1.0);
}
)";
inline const char* CUBE_FS = R"(#version 330 core
in vec3 vWorld, vNrm, vLocal;
flat in vec4 vRot, vTint, vFx;
out vec4 frag;
uniform sampler2D uScene;
uniform int uReflect;
uniform vec3 uPN;
uniform float uPD;
uniform vec3 uFallback;     // what the glass shows behind it in a reflection pass
uniform float uDensity, uBody, uRefract, uEdgeK, uSpecK;
uniform vec3 uInk;
uniform float uBridge;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
vec3 qinv(vec4 q, vec3 v) { return qrot(vec4(-q.xyz, q.w), v); }
void main() {
    vec3 N = normalize(vNrm);
    vec3 V = normalize(uEye - vWorld);
    if (dot(N, V) < 0.0) N = -N;
    vec3 I = -V;
    float kind = vTint.w;
    float alpha = vFx.x, flash = vFx.y, frost = vFx.z;
    vec3 tint = vTint.rgb;
    float ndv = max(dot(N, V), 0.0);
    float F = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);
    // the path through the cube (in its own frame): refraction at the entry, out through the far face
    vec3 Rd = refract(I, N, 1.0 / 1.5);
    vec3 lr = qinv(vRot, Rd), lp = clamp(vLocal, -0.999, 0.999);
    vec3 lrs = mix(lr, vec3(1e-4), vec3(lessThan(abs(lr), vec3(1e-4))));
    vec3 tt = (sign(lrs) - lp) / lrs;
    float te = min(tt.x, min(tt.y, tt.z));
    vec3 pe = lp + lr * te;
    vec3 a = 1.0 - abs(pe);
    // the two smallest distances to the far face's edges
    float e1 = min(a.x, min(a.y, a.z));
    float e2 = a.x + a.y + a.z - e1 - max(a.x, max(a.y, a.z));
    // the far face's edges seen through the glass; the light entering from the key focuses on the far side
    float backEdge = exp(-sq(e2 / 0.06));
    vec3 lk = qinv(vRot, uKeyDir);
    float away = clamp(0.5 - 0.5 * dot(normalize(pe.xy + 1e-4), normalize(lk.xy + 1e-4)), 0.0, 1.0);
    float thick = te * 0.5; // 0 .. ~1.2 (the cube's width is 2)
    // behind the glass: the room, softly offset by the refraction
    vec3 bg;
    if (uReflect == 1) bg = uFallback;
    else {
        vec2 uv = gl_FragCoord.xy / uRes + (Rd.xy - I.xy) * uRefract;
        bg = texture(uScene, uv).rgb;
    }
    vec3 absorb = pow(max(tint, vec3(0.002)), vec3(uDensity * (0.55 + thick)));
    float keyL = dot(uKeyCol, vec3(0.3, 0.5, 0.2));
    float ambL = dot(uAmbTop, vec3(0.3, 0.5, 0.2));
    // the light held in the glass: brighter toward the far side (the key focused through it), dimmer near the light
    float grad = 0.55 + 0.45 * smoothstep(-1.0, 1.0, dot(-lp.xy, normalize(lk.xy + 1e-4)));
    vec3 body = tint * (0.05 + keyL * 0.16 + ambL * 0.35) * uBody * grad;
    vec3 refl = env(vWorld, reflect(I, N));
    vec3 H = normalize(uKeyDir + V);
    float nh = max(dot(N, H), 0.0);
    float spec = (pow(nh, 220.0) * 4.0 + pow(nh, 24.0) * 0.06) * uSpecK;
    vec3 col = bg * absorb * (1.0 - F) + body + refl * F + uKeyCol * spec;
    // the far edges: a caustic rim, bright and tinted away from the light, a faint line near it
    vec3 rimC = mix(tint, vec3(1.0), 0.25) * (keyL * 0.9 + ambL * 0.8 + 0.08);
    col += rimC * backEdge * uEdgeK * (0.25 + 1.4 * away);
    // the bevels: a crisp line where the face turns, brightest on the edges facing the light
    float edgeness = smoothstep(0.84, 0.97, abs(vLocal.x)) + smoothstep(0.84, 0.97, abs(vLocal.y)) + smoothstep(0.84, 0.97, abs(vLocal.z));
    float onEdge = smoothstep(1.4, 2.0, edgeness);
    float facing = max(dot(N, uKeyDir), 0.0);
    col += mix(tint, vec3(1.0), 0.7) * onEdge * uEdgeK * (0.08 + keyL * 0.9 * facing * facing);
    col += (tint * 0.6 + 0.4) * flash;
    // game over: frosted
    col = mix(col, (tint * 0.35 + 0.65) * (uAmbTop + uKeyCol * 0.3) * 0.9 + refl * F, frost * 0.85);
    float outA = 1.0;
    if (kind > 0.5 && kind < 1.5) {
        // the ghost: a breath of glass (its edges only)
        float e = onEdge;
        col = bg * mix(vec3(1.0), absorb, 0.18) + mix(tint, vec3(1.0), 0.4) * e * 0.35;
        outA = 0.4;
    }
    col = mix(bg, col, alpha);
    // a reflection pass: seen from behind, through the smoked panel
    if (uReflect == 1) {
        vec3 dir = uEye - vWorld;
        float s = (uPanelMin.z - vWorld.z) / (abs(dir.z) > 1e-5 ? dir.z : 1e-5);
        float sm = (uPD - dot(vWorld, uPN)) / dot(dir, uPN);
        vec3 q = vWorld + dir * s;
        if (s > 0.0 && s < sm && q.x > uPanelMin.x && q.x < uPanelMax.x && q.y > uPanelMin.y && q.y < uPanelMax.y)
            col = col * 0.85 + uPanelCol * 0.3;
    }
    col = mix(col, uInk * (0.6 + 0.4 * onEdge), uBridge);
    frag = vec4(col, outA * alpha);
}
)";

// ---------------------------------------------------------------- copy, final
inline const char* COPY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
void main() { frag = texture(uTex, vUv); }
)";
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uDim, uFade, uBridge, uBloom, uSlow;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
uniform vec2 uRes;
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 tonemap(vec3 c) {
    vec3 x = max(c, 0.0);
    return x / (1.0 + max(x - 0.7, 0.0) * 1.1);
}
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 b = textureLod(uScene, vUv, 3.0).rgb * 0.5 + textureLod(uScene, vUv, 4.5).rgb * 0.35 + textureLod(uScene, vUv, 6.0).rgb * 0.25;
    vec3 c = s.rgb + max(b - 0.55, 0.0) * uBloom;
    c = tonemap(c);
    c = mix(c, c * uSlowTint, uSlow);
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.2 * smoothstep(0.3, 0.95, length(q * vec2(1.0, 0.85)));
    c = mix(c, uBridgeCol, uBridge * (1.0 - s.a));
    c = mix(c, uBridgeInk, uBridge * s.a);
    c *= (1.0 - 0.5 * uDim) * uFade;
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash12(gl_FragCoord.xy) - 0.5) / 255.0 * 1.5;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- HUD
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
uniform vec4 uInk;
uniform float uBias, uSoft;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75 + uSoft;
    float fill = smoothstep(0.5 - uBias - w, 0.5 - uBias + w, d);
    frag = vec4(uInk.rgb * uInk.a * fill, uInk.a * fill);
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

} // namespace mirror_shaders

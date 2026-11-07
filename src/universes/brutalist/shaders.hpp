#pragma once
// BRUTALIST's shaders (see brutalist.cpp).
//
// SKY_FS paints the sky (the hour's gradient, the sun's disc and glow, a veil of high cloud, stars and the moon by
// night); DEPTH_VS / DEPTH_FS draw the sun's shadow map (every solid, seen from the sun); BOX_VS / BOX_FS everything
// solid (instanced boxes: board-marked, ribbed and precast concrete with its tie holes and weathering streaks, the
// cast blocks of the stack, the slot's dark lining, the glazing in its deep reveals, the paving, the weeds, painted
// steel, the far city), lit by the sun through the shadow map and by the sky, hazed by distance; SPRITE_FS the dust
// glittering in the sun (each mote looks the sun up in the shadow map: it shines only in the light); LINE_FS the
// cables and the ghost (screen-space hairlines); DOWN_FS / BLUR_FS the glow and FINAL_FS the frame.
#include <string>

namespace brutalist_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform vec3 uZen, uHor, uGlow, uSunDir, uSunTint;
uniform float uFogD, uTime, uSat, uVeil;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    const mat2 R = mat2(0.8, 0.6, -0.6, 0.8);
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = R * p * 2.03 + 0.37; a *= 0.5; }
    return s / 0.9375;
}
// The sky's gradient in a direction: the horizon's haze, the zenith, the glow around the sun.
vec3 skyGrad(vec3 d) {
    float e = max(d.y, 0.0);
    vec3 c = mix(uHor, uZen, pow(smoothstep(0.0, 0.7, e), 0.6));
    float s = max(dot(d, uSunDir), 0.0);
    c += uGlow * (pow(s, 6.0) * 0.5 + pow(s, 40.0) * 0.8) * (0.6 + 0.4 * uVeil);
    // the glow of the sun spreads along the horizon when it is low
    c += uGlow * 0.35 * exp(-e * 10.0) * pow(max(dot(normalize(d.xz + 1e-5), normalize(uSunDir.xz + 1e-5)), 0.0), 3.0) * exp(-uSunDir.y * 5.0);
    return c;
}
vec3 applyFog(vec3 c, vec3 P) {
    vec3 v = P - uEye;
    float dist = length(v);
    // nothing near the well is hazed: the air thickens across the plaza
    float f = 1.0 - exp(-max(dist - 45.0, 0.0) * uFogD);
    vec3 d = v / dist;
    vec3 h = skyGrad(normalize(vec3(d.x, max(d.y, 0.0) * 0.4 + 0.02, d.z)));
    return mix(c, h, clamp(f, 0.0, 1.0));
}
vec3 saturateC(vec3 c, float k) {
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return max(mix(vec3(l), c, k), 0.0);
}
)";

// The sun through its shadow map (a few hardware-filtered taps: the edges stay hard, their stairs smoothed).
inline const char* SHADOW = R"(
uniform sampler2DShadow uShadow;
uniform mat4 uSunVP;
uniform float uSoft, uTexel;
float sunShadow(vec3 P, vec3 N) {
    vec4 s = uSunVP * vec4(P + N * 0.035 + uSunDir * 0.03, 1.0);
    vec3 p = s.xyz / s.w * 0.5 + 0.5;
    if (p.x <= 0.0 || p.y <= 0.0 || p.x >= 1.0 || p.y >= 1.0 || p.z >= 1.0) return 1.0;
    float r = uSoft * uTexel;
    float a = hash12(gl_FragCoord.xy) * 6.2832;
    vec2 o1 = vec2(cos(a), sin(a)) * r, o2 = vec2(-o1.y, o1.x);
    float k = texture(uShadow, vec3(p.xy + o1, p.z));
    k += texture(uShadow, vec3(p.xy - o1, p.z));
    k += texture(uShadow, vec3(p.xy + o2, p.z));
    k += texture(uShadow, vec3(p.xy - o2, p.z));
    return k * 0.25;
}
)";

// ---------------------------------------------------------------- the sky (full screen, behind everything)
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP;
uniform float uStars, uSeed, uMoon;
uniform vec3 uDisc;
vec3 rayDir(vec2 uv) {
    vec4 a = uInvVP * vec4(uv * 2.0 - 1.0, -1.0, 1.0), b = uInvVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(b.xyz / b.w - a.xyz / a.w);
}
void main() {
    vec3 d = rayDir(vUv);
    float e = d.y;
    vec3 c = skyGrad(d);
    float az = atan(d.x, -d.z);
    // the sun's (or the moon's) disc
    float m = dot(d, uSunDir);
    c += uDisc * smoothstep(0.99955, 0.99975, m) * (1.0 - 0.6 * uVeil);
    // a veil of high cloud: streaks drifting slowly, lit toward the sun; thicker when the song is calm
    if (e > 0.0) {
        vec2 p = vec2(az * 2.6, 1.0 / (e + 0.12)) + vec2(uTime * 0.003, 0.0);
        float n = fbm(p * vec2(1.6, 0.9) + uSeed);
        float n2 = fbm(p * vec2(4.0, 2.2) - uSeed);
        float band = smoothstep(0.42 - 0.18 * uVeil, 0.85, n * 0.75 + n2 * 0.35) * smoothstep(0.0, 0.08, e);
        float lit = pow(max(m, 0.0), 4.0);
        vec3 cc = mix(mix(uHor, vec3(1.0), 0.35) * 0.9, uGlow * 1.4 + uHor * 0.5, lit * 0.7);
        c = mix(c, cc, band * (0.3 + 0.5 * uVeil));
    }
    // stars by night
    if (uStars > 0.0 && e > 0.03) {
        vec2 g = vec2(az * 240.0, e * 240.0);
        vec2 cell = floor(g);
        float h = hash12(cell + uSeed);
        vec2 f = fract(g) - 0.5;
        float s = smoothstep(0.986, 1.0, h) * smoothstep(0.1, 0.0, length(f)) * smoothstep(0.03, 0.25, e);
        c += vec3(0.85, 0.9, 1.0) * s * uStars * (0.7 + 0.3 * sin(uTime * (1.0 + 2.0 * hash12(cell)) + h * 30.0));
    }
    c = saturateC(c, uSat);
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- solids (instanced boxes)
inline const char* BOX_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
uniform mat4 uVP;
out vec3 vP, vL;
flat out vec3 vLN, vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat;
void main() {
    vec3 P = iC.xyz + iX.xyz * aPos.x + iY.xyz * aPos.y + iZ.xyz * aPos.z;
    vHalf = vec3(length(iX.xyz), length(iY.xyz), length(iZ.xyz));
    vAX = iX.xyz / max(vHalf.x, 1e-6);
    vAY = iY.xyz / max(vHalf.y, 1e-6);
    vAZ = iZ.xyz / max(vHalf.z, 1e-6);
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    gl_Position = uVP * vec4(P, 1.0);
}
)";
inline const char* DEPTH_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
uniform mat4 uVP;
void main() {
    // the far city and the lights cast no shadow
    float m = iC.w;
    vec3 P = iC.xyz + iX.xyz * aPos.x + iY.xyz * aPos.y + iZ.xyz * aPos.z;
    gl_Position = (abs(m - 7.0) < 0.5 || abs(m - 8.0) < 0.5) ? vec4(0.0, 0.0, -2.0, 1.0) : uVP * vec4(P, 1.0);
}
)";
inline const char* DEPTH_FS = R"(#version 330 core
void main() {}
)";
inline const char* BOX_FS = R"(#version 330 core
in vec3 vP, vL;
flat in vec3 vLN, vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat;
out vec4 frag;
uniform float uStackA, uSnow, uWinOn, uGlint, uFlash;
uniform vec3 uSunCol, uAmbSky, uAmbGround, uWinCol, uFill, uFlood;

// Board-marked concrete on a face (w.x along the boards, w.y up the wall): the pour's clouds and lifts, the boards'
// tones, grain and seams, the formwork panels' joints and tie holes; or vertical ribs, or smooth precast panels.
vec3 concrete(vec3 alb, vec2 w, float pattern, float px, inout vec2 bend, float seed) {
    float detail = 1.0 - smoothstep(0.02, 0.08, px);
    float fine = 1.0 - smoothstep(0.008, 0.03, px);
    alb *= 0.86 + 0.26 * fbm(w * vec2(0.09, 0.14) + seed * 7.0);
    alb *= 0.95 + 0.1 * hash12(vec2(floor(w.y / 2.88 + 0.5), floor(seed * 9.0)));
    // (far away, the boards are finer than a pixel: the pour's tone is enough)
    if (detail <= 0.0) return alb;
    if (pattern < 1.5) {
        // boards, 0.24 wide, in panels 2.4 long (pattern 1: the boards stand upright)
        vec2 b = pattern < 0.5 ? w : vec2(w.y, w.x);
        float bh = 0.24;
        float row = floor(b.y / bh);
        float fy = fract(b.y / bh);
        float panel = floor(b.x / 2.4);
        alb *= 1.0 + 0.11 * (hash12(vec2(row, panel + floor(seed * 13.0))) - 0.5) * detail;
        float grain = vnoise(vec2(b.x * 1.4 + row * 17.0, fy * 5.0 + row * 3.0)) + 0.5 * vnoise(vec2(b.x * 5.0 + row * 5.0, fy * 12.0));
        alb *= 1.0 - 0.08 * (grain - 0.75) * fine;
        float seam = 1.0 - smoothstep(0.0, 0.01 + px * 0.8, min(fy, 1.0 - fy) * bh);
        alb *= 1.0 - 0.2 * seam * detail;
        // each board's edge a little proud, catching the raking sun
        float lip = (smoothstep(0.0, 0.1, fy) - smoothstep(0.1, 0.3, fy)) * fine;
        if (pattern < 0.5) bend.y += 0.35 * lip; else bend.x += 0.35 * lip;
        float pj = fract(b.x / 2.4);
        float joint = 1.0 - smoothstep(0.0, 0.014 + px, min(pj, 1.0 - pj) * 2.4);
        alb *= 1.0 - 0.3 * joint * detail;
        vec2 th = vec2(fract(w.x / 0.8) - 0.5, fract((w.y - 0.36) / 1.44) - 0.5) * vec2(0.8, 1.44);
        float r = length(th);
        float hole = 1.0 - smoothstep(0.032, 0.032 + px * 1.2, r);
        float ring = (1.0 - smoothstep(0.05, 0.065 + px, r)) - hole;
        alb *= 1.0 - 0.6 * hole * detail;
        alb *= 1.0 + 0.1 * ring * fine;
    } else if (pattern < 2.5) {
        // ribbed: vertical ribs 0.3 apart, their hammered crests rough and pale
        float f = fract(w.x / 0.3);
        float tri = abs(f - 0.5) * 2.0;
        bend.x += (f < 0.5 ? 0.6 : -0.6) * detail;
        float crest = smoothstep(0.75, 1.0, tri);
        alb *= 1.0 + (vnoise(w * vec2(14.0, 9.0)) - 0.4) * 0.35 * crest * fine;
        alb *= 1.0 - 0.2 * (1.0 - smoothstep(0.0, 0.3, tri)) * detail;
    } else {
        // precast panels: fine joints on a 3 x 1.5 grid
        vec2 g = fract(w / vec2(3.0, 1.5));
        float jt = 1.0 - smoothstep(0.0, 0.012 + px, min(min(g.x, 1.0 - g.x) * 3.0, min(g.y, 1.0 - g.y) * 1.5));
        alb *= 1.0 - 0.32 * jt * detail;
        alb *= 0.96 + 0.08 * hash12(floor(w / vec2(3.0, 1.5)) + seed);
    }
    // pores, blowholes
    float pore = step(0.982, hash12(floor(w * 40.0)));
    alb *= 1.0 - 0.3 * pore * fine;
    return alb;
}

// Weathering on a wall: dark streaks run down from its top edge (rain off the coping), damp and soiling at its foot.
vec3 weather(vec3 alb, vec2 w, vec2 q, vec2 sz, float k, float seed, float above) {
    float down = sz.y - q.y;                       // how far below the face's top edge
    float colh = hash12(vec2(floor(w.x * 1.7 + seed * 3.0), seed));
    float len = mix(0.8, 7.0, colh * colh);
    float st = vnoise(vec2(w.x * 2.4 + seed * 5.0, w.y * 0.05)) * 0.65 + 0.35 * vnoise(vec2(w.x * 9.0, w.y * 0.25)) * step(fwidth(w.x), 0.08);
    float run = smoothstep(0.45, 0.8, st) * (1.0 - smoothstep(0.0, len, down));
    float broad = smoothstep(0.35, 0.9, vnoise(vec2(w.x * 0.45 + seed, w.y * 0.03))) * 0.5;
    alb *= 1.0 - (0.34 * run + 0.18 * broad) * k;
    // the foot: damp, a little green
    float foot = 1.0 - smoothstep(0.0, 1.4 + 0.8 * vnoise(w * 1.3), above);
    alb = mix(alb, alb * vec3(0.72, 0.78, 0.7), foot * k * 0.6);
    return alb;
}

void main() {
    int mat = int(vMat.x + 0.5);
    vec2 uv, sz;
    vec3 T, B, N;
    int face; // 0 x sides, 1 top / bottom, 2 ends
    if (abs(vLN.x) > 0.5) { uv = vL.zy; sz = vHalf.zy; T = vAZ; B = vAY; N = vAX * sign(vLN.x); face = 0; }
    else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; N = vAY * sign(vLN.y); face = 1; }
    else { uv = vL.xy; sz = vHalf.xy; T = vAX; B = vAY; N = vAZ * sign(vLN.z); face = 2; }
    vec2 q = uv * sz;
    vec2 w = vec2(dot(vP, T), dot(vP, B));
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float spec = 0.03, rough = 16.0, ao = 1.0, wrap = 0.0, fill = 0.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    float px = max(fwidth(w.x), fwidth(w.y));
    bool up = face == 1 && N.y > 0.5;
    bool vert = face != 1;
    vec3 V = normalize(uEye - vP);
    if (mat == 0 || mat == 2) {
        // concrete: vMat.y its sky visibility (0: open), vMat.z the pattern, vMat.w how weathered
        ao = vMat.y > 0.0 ? vMat.y : 1.0;
        float pattern = up ? 3.5 : vMat.z;
        if (up) {
            // a trowelled top: dirt, a few cracks
            alb *= 0.88 + 0.2 * fbm(w * 0.4 + seed);
            alb *= 1.0 - 0.15 * smoothstep(0.6, 0.85, vnoise(w * 0.7 + 3.0));
        } else {
            alb = concrete(alb, w, pattern, px, bend, seed);
            // (how high above the ground: the plaza, the steps, the podium)
            float ground = vP.z > -14.0 ? -11.4 : vP.z < -19.4 ? -8.7 : -11.4 + (-14.0 - vP.z) / 5.4 * 2.7;
            float above = vP.y - ground;
            alb = weather(alb, w, q, sz, vMat.w, seed, above);
            // the foot of a wall in the ground's contact shadow
            ao *= 0.7 + 0.3 * smoothstep(0.0, 1.5, above);
        }
        if (mat == 2) {
            // the slot's lining: dark concrete, the stack reads on it
            ao *= 0.75;
        }
        // snow settles on the tops
        if (up) alb = mix(alb, vec3(0.86, 0.88, 0.92), uSnow * (0.75 + 0.25 * vnoise(w * 2.0)));
    } else if (mat == 1) {
        // a cast block of the stack: vMat.y which sides join the same piece (1 left, 2 right, 4 up, 8 down),
        // vMat.z the piece's type (its finish); a chamfer around the piece's outline only
        float m = vMat.y;
        bool jl = mod(m, 2.0) >= 1.0, jr = mod(floor(m / 2.0), 2.0) >= 1.0;
        bool ju = mod(floor(m / 4.0), 2.0) >= 1.0, jd = mod(floor(m / 8.0), 2.0) >= 1.0;
        float detail = 1.0 - smoothstep(0.01, 0.04, px);
        if (face == 2) {
            float dl = jl ? 1e3 : q.x + sz.x, dr = jr ? 1e3 : sz.x - q.x;
            float du = ju ? 1e3 : sz.y - q.y, dd = jd ? 1e3 : q.y + sz.y;
            float b = 0.09;
            bend.x += 0.75 * ((1.0 - smoothstep(0.0, b, dr)) - (1.0 - smoothstep(0.0, b, dl)));
            bend.y += 0.75 * ((1.0 - smoothstep(0.0, b, du)) - (1.0 - smoothstep(0.0, b, dd)));
            // the arris a little worn, pale
            float arr = 1.0 - smoothstep(0.0, 0.02 + px, min(min(dl, dr), min(du, dd)));
            alb *= 1.0 + 0.12 * arr;
            // the finish, by type: smooth with blowholes, exposed aggregate, fine boards
            int fin = int(mod(vMat.z, 3.0) + 0.5);
            vec2 g = vP.xy;
            alb *= 0.93 + 0.12 * fbm(g * 0.9 + vMat.z * 3.1);
            if (fin == 0) {
                float pore = step(0.975, hash12(floor(g * 34.0)));
                alb *= 1.0 - 0.35 * pore * detail;
            } else if (fin == 1) {
                vec2 c = g * 14.0;
                vec2 id = floor(c);
                vec2 f = fract(c) - 0.5 - (vec2(hash12(id), hash12(id + 7.0)) - 0.5) * 0.5;
                float h = hash12(id + 3.3);
                float pebble = (1.0 - smoothstep(0.18, 0.26, length(f))) * step(0.45, h);
                alb = mix(alb, alb * (h > 0.75 ? 1.25 : 0.72), pebble * 0.6 * detail);
            } else {
                float fy = fract(g.y / 0.16);
                float seam = 1.0 - smoothstep(0.0, 0.012 + px, min(fy, 1.0 - fy) * 0.16);
                alb *= 1.0 - 0.14 * seam * detail;
                alb *= 1.0 + 0.06 * (hash12(vec2(floor(g.y / 0.16), 2.0)) - 0.5);
            }
        } else {
            alb *= 0.8;
        }
        alb = saturateC(alb, uSat);
        spec = 0.06;
        rough = 20.0;
        // the plaza's light bounced back onto the stack: it always reads, in a shadow too
        fill = 1.0;
    } else if (mat == 3) {
        // glazing in its deep reveal: the sky in it, the sun's glint, a lit room by dusk (vMat.y how many are lit,
        // vMat.z its sky visibility)
        ao = vMat.z > 0.0 ? vMat.z : 1.0;
        vec3 R = reflect(-V, N);
        float fres = 0.06 + 0.94 * pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);
        vec3 c = vec3(0.012, 0.014, 0.018) + skyGrad(normalize(vec3(R.x, abs(R.y), R.z))) * (0.1 + 0.6 * fres) * ao;
        // mullions
        float mx = fract(w.x / 1.2);
        float mull = 1.0 - smoothstep(0.0, 0.03 + px, min(mx, 1.0 - mx) * 1.2);
        float lit = step(hash12(vec2(seed * 91.0, floor(w.x / 1.2))), vMat.y * uWinOn);
        c += uWinCol * lit * (0.7 + 0.5 * hash12(vec2(seed, floor(w.x / 1.2) + 4.0))) * (1.0 - mull);
        float sh = sunShadow(vP, N);
        c += uSunCol * pow(max(dot(R, uSunDir), 0.0), 600.0) * 18.0 * sh * uGlint;
        c += uSunCol * uFlash * (0.5 + 1.2 * fres) * (0.4 + 0.6 * sh) * (1.0 - mull);
        c = mix(c, vec3(0.05) * (uAmbSky + uSunCol * max(dot(N, uSunDir), 0.0) * sh), mull);
        c = applyFog(c, vP);
        frag = vec4(saturateC(c, uSat), uStackA);
        return;
    } else if (mat == 4) {
        // paving: slabs 1.8 across, their joints, the odd darker one, weeds in the joints
        vec2 g = vP.xz / 2.4;
        vec2 id = floor(g);
        vec2 f = fract(g);
        float jd = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y)) * 2.4;
        float joint = 1.0 - smoothstep(0.0, 0.02 + px, jd);
        alb *= 0.9 + 0.16 * hash12(id + seed);
        alb *= 0.9 + 0.18 * fbm(vP.xz * 0.5);
        alb *= 1.0 - 0.25 * joint;
        alb = mix(alb, alb * vec3(0.7, 0.85, 0.6), joint * smoothstep(0.6, 0.9, vnoise(vP.xz * 0.6)) * 0.6);
        alb = mix(alb, vec3(0.86, 0.88, 0.92), uSnow * (0.8 + 0.2 * vnoise(vP.xz)) * (1.0 - joint * 0.5));
        if (!up) alb = concrete(alb, w, 3.0, px, bend, seed);
    } else if (mat == 5) {
        // a weed's blade, a leaf: the light comes through it a little
        alb *= 0.85 + 0.3 * hash12(vec2(seed, 1.0));
        wrap = 0.5;
        spec = 0.08;
    } else if (mat == 6) {
        // painted steel
        alb *= 0.92 + 0.08 * vnoise(vP.xz * 0.7 + vP.y * 0.3);
        spec = 0.25;
        rough = 40.0;
    } else if (mat == 7) {
        // the far city: slabs and towers in the haze, their rows of windows
        if (vert) {
            float fl = fract(w.y / 3.0);
            float win = step(0.4, fl) * step(fl, 0.75) * step(0.3, fract(w.x / 2.0));
            alb *= 1.0 - 0.35 * win * (1.0 - smoothstep(0.1, 0.5, px));
            float lit = step(hash12(floor(vec2(w.x / 2.0, w.y / 3.0)) + seed * 7.0), 0.22) * win;
            emit += uWinCol * lit * uWinOn * 0.3 * (1.0 - smoothstep(0.1, 0.4, px));
        }
    } else {
        emit = vCol.rgb * vMat.y;
        alb = vec3(0.0);
    }
    vec3 Nb = normalize(N + T * bend.x + B * bend.y);
    float nl = dot(Nb, uSunDir);
    float lam = wrap > 0.0 ? clamp((nl + wrap) / (1.0 + wrap), 0.0, 1.0) : max(nl, 0.0);
    // (the shadow follows the face's own normal: the bent normals only shade)
    float sh = dot(N, uSunDir) > -0.05 || wrap > 0.0 ? sunShadow(vP, N) : 0.0;
    vec3 amb = mix(uAmbGround, uAmbSky, 0.5 + 0.5 * Nb.y) * ao;
    vec3 c = alb * (uSunCol * lam * sh + amb);
    vec3 H = normalize(uSunDir + V);
    c += uSunCol * pow(max(dot(Nb, H), 0.0), rough) * spec * sh * step(0.0, nl);
    c += alb * uFill * fill * (0.35 + 0.65 * max(Nb.z, 0.0));
    // the monument's floodlights by dusk and by night, washing up its face from the plaza
    if (uFlood.r > 0.0) {
        float fm = smoothstep(-7.0, -1.5, vP.z) * (1.0 - smoothstep(9.0, 17.0, abs(vP.x))) * (0.45 + 0.55 * (1.0 - smoothstep(-11.0, 13.0, vP.y)));
        c += alb * uFlood * fm * max(dot(Nb, normalize(vec3(0.0, 0.3, 1.0))), 0.0);
    }
    c += emit;
    c = applyFog(c, vP);
    frag = vec4(c, uStackA);
}
)";

// ---------------------------------------------------------------- dust in the light (billboards)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;
layout(location = 2) in vec4 iC;
uniform mat4 uVP, uSunVP;
uniform sampler2DShadow uShadow;
uniform vec2 uRes;
out vec2 vC;
out vec4 vCol;
void main() {
    vC = aCorner;
    vec3 c = iP.xyz;
    // in the sun or in the shade
    vec4 s = uSunVP * vec4(c, 1.0);
    vec3 p = s.xyz / s.w * 0.5 + 0.5;
    float lit = 1.0;
    if (p.x > 0.0 && p.y > 0.0 && p.x < 1.0 && p.y < 1.0) lit = texture(uShadow, vec3(p.xy, p.z - 0.002));
    vCol = vec4(iC.rgb, iC.a * mix(0.08, 1.0, lit));
    vec4 cp = uVP * vec4(c, 1.0);
    // a size in pixels (iP.w), never under a pixel and a half
    vec2 r = max(iP.w, 1.5) / uRes * 2.0;
    cp.xy += aCorner * r * cp.w;
    gl_Position = cp;
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float a = exp(-r2 * 4.0) * (1.0 - r2);
    frag = vec4(vCol.rgb * vCol.a * a, 0.0);
}
)";

// ---------------------------------------------------------------- cables, the ghost (screen-space hairlines)
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // x: 0 start .. 1 end, y: -1 .. 1 across
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uRes;
out float vAcross;
out vec4 vCol;
out float vW;
out vec3 vP;
void main() {
    vec4 c0 = uVP * vec4(iA.xyz, 1.0), c1 = uVP * vec4(iB.xyz, 1.0);
    vec2 s0 = c0.xy / c0.w * uRes * 0.5, s1 = c1.xy / c1.w * uRes * 0.5;
    vec2 dir = s1 - s0;
    dir = length(dir) > 1e-4 ? normalize(dir) : vec2(0.0, 1.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec4 c = mix(c0, c1, aCorner.x);
    float hw = max(iA.w, 1.0) * 0.5 + 0.75;
    c.xy += nrm * aCorner.y * hw / (uRes * 0.5) * c.w;
    vAcross = aCorner.y * hw;
    vW = iA.w;
    vCol = vec4(iC.rgb, iB.w);
    vP = mix(iA.xyz, iB.xyz, aCorner.x);
    gl_Position = c;
}
)";
inline const char* LINE_FS = R"(#version 330 core
in float vAcross;
in vec4 vCol;
in float vW;
in vec3 vP;
out vec4 frag;
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    vec3 c = applyFog(vCol.rgb, vP);
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- the glow and the frame
inline const char* DOWN_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThr;
void main() {
    vec4 s0 = texture(uSrc, vUv + uTexel * vec2(-1, -1)), s1 = texture(uSrc, vUv + uTexel * vec2(1, -1));
    vec4 s2 = texture(uSrc, vUv + uTexel * vec2(-1, 1)), s3 = texture(uSrc, vUv + uTexel * vec2(1, 1));
    // the stack (the scene's alpha) stays out of the glow; its alpha is blurred into the halo around it
    vec3 c = s0.rgb * (1.0 - s0.a) + s1.rgb * (1.0 - s1.a) + s2.rgb * (1.0 - s2.a) + s3.rgb * (1.0 - s3.a);
    c *= 0.25;
    frag = vec4(max(c - vec3(uThr), 0.0), 0.25 * (s0.a + s1.a + s2.a + s3.a));
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.2270, 0.1945, 0.1216, 0.0541, 0.0162);
    vec4 c = texture(uSrc, vUv) * w[0];
    for (int i = 1; i < 5; i++) c += (texture(uSrc, vUv + uDir * float(i)) + texture(uSrc, vUv - uDir * float(i))) * w[i];
    frag = c;
}
)";
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF, uHalo, uContrast;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint, uHaloCol;
void main() {
    vec4 s = texture(uScene, vUv);
    vec4 b = texture(uBloom, vUv);
    vec3 c = s.rgb * uExposure + b.rgb * uBloomK;
    // a soft halo around the stack and the previews, outside them only (it keeps them off a busy ground)
    float hk = smoothstep(0.0, 0.55, b.a) * (1.0 - clamp(s.a, 0.0, 1.0)) * uHalo;
    c = mix(c, uHaloCol, hk);
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    // slow motion: the evening comes, warm and long
    c = mix(c, mix(vec3(l), c, 0.8) * uSlowTint, uSlow);
    // a filmic shoulder, a touch of contrast
    c = c / (1.0 + 0.18 * max(max(c.r, c.g), c.b));
    c = pow(max(c, vec3(0.0)), vec3(uContrast));
    vec2 v = vUv - 0.5;
    c *= 1.0 - 0.28 * dot(v, v) * 2.0;
    c = mix(c, mix(uBridgeCol, uBridgeInk, clamp(s.a, 0.0, 1.0)), uBridge);
    c *= (1.0 - uDim * 0.5) * uFade;
    c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    c += (fract(sin(dot(floor(gl_FragCoord.xy), vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- the HUD (pixels)
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
    float halo = smoothstep(0.1, 0.47, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a shader with the common functions (and the sun's shadow) inserted after the version line.
inline std::string withCommon(const char* src, bool shadow = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shadow ? SHADOW : "") + s.substr(nl + 1);
}

} // namespace brutalist_shaders

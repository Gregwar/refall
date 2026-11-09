#pragma once
// MUSIC BOX's shaders (see musicbox.cpp).
//
// SOLID_VS / SOLID_FS draw everything solid, instanced: boxes, cylinders and hemispheres, with their materials:
// lacquered veneers with their grain and inlaid stringing (the lid's inside with its rosette), velvet, brushed and
// polished brass, the steel comb, the pins and the well's pieces (metal or enamel plates with a domed brass boss),
// the table, the wall, porcelain (the dancer). Lit by the lamp (a warm spot with its shadow map), a dim cool fill,
// a few point lights (the dancer's ring), and a small procedural room reflected in the metals. SPRITE_* are the
// glows; VEIL_FS the shade behind the well and the HUD's columns; LINE_* the ghost; DOWN / BLUR / FINAL the bloom
// and the frame; SHAPE / TEXT the HUD.
#include <string>

namespace musicbox_shaders {

inline const char* COMMON = R"(
uniform vec3 uEye;
uniform float uTime;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
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
)";

// ---------------------------------------------------------------- solids (instanced meshes)
// Instance: c (center, material), x y z (half axes, params p0 p1 p2), col (color, seed), e (four more params).
inline const char* SOLID_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iC;
layout(location = 3) in vec4 iX;
layout(location = 4) in vec4 iY;
layout(location = 5) in vec4 iZ;
layout(location = 6) in vec4 iCol;
layout(location = 7) in vec4 iE;
uniform mat4 uVP;
uniform vec3 uOrigin;
out vec3 vP, vL, vLN, vN;
flat out vec3 vHalf, vAX, vAY, vAZ;
flat out vec4 vCol, vMat, vE;
void main() {
    vec3 X = iX.xyz, Y = iY.xyz, Z = iZ.xyz;
    vec3 P = iC.xyz + X * aPos.x + Y * aPos.y + Z * aPos.z;
    vHalf = vec3(length(X), length(Y), length(Z));
    vAX = X / max(vHalf.x, 1e-6);
    vAY = Y / max(vHalf.y, 1e-6);
    vAZ = Z / max(vHalf.z, 1e-6);
    vN = normalize(vAX * aNrm.x / max(vHalf.x, 1e-6) + vAY * aNrm.y / max(vHalf.y, 1e-6) + vAZ * aNrm.z / max(vHalf.z, 1e-6));
    vP = P;
    vL = aPos;
    vLN = aNrm;
    vCol = iCol;
    vMat = vec4(iC.w, iX.w, iY.w, iZ.w);
    vE = iE;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";

inline const char* LIGHTING = R"(
uniform vec3 uLampP, uLampCol, uAmb, uAmbLow, uFillDir, uFillCol, uWallCol, uFloorCol;
uniform float uLampK, uSpot;
uniform vec3 uSpotC;
uniform int uNL;
uniform vec4 uLP[8];
uniform vec4 uLC[8];
uniform float uShadowOn, uShadowTexel, uShadowBias;
uniform sampler2DShadow uShadow;
uniform mat4 uShadowVP;
float shadowAt(vec3 P, vec3 N) {
    if (uShadowOn < 0.5) return 1.0;
    vec4 s = uShadowVP * vec4(P + N * uShadowBias, 1.0);
    vec3 q = s.xyz / s.w * 0.5 + 0.5;
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0 || q.z > 1.0) return 1.0;
    float t = uShadowTexel;
    float r = 0.0;
    for (int i = -2; i <= 2; i++)
        for (int j = -2; j <= 2; j++) r += texture(uShadow, vec3(q.xy + vec2(i, j) * t * 1.6, q.z));
    return r / 25.0;
}
// The lamp's pool: brightest on the box, falling off over the table.
float spotAt(vec3 P) {
    float d = length((P - uSpotC).xz);
    return mix(0.12, 1.0, exp(-d * d / (uSpot * uSpot)));
}
// The room as the metals see it: the table and the velvet below, the lid's lit inside behind, the room in front
// (behind the viewer), dark above; the lamp's shade (a bright soft disc), the window's faint evening.
uniform vec3 uEnvBack, uEnvFront;
vec3 envAt(vec3 R, float rough, vec3 P) {
    float up = R.y;
    // behind: the lid's lit inside (a band), the room's dark above it; in front: the dark room
    float back = smoothstep(0.25, -0.35, R.z);
    vec3 c = mix(uEnvFront, uEnvBack, back * (1.0 - smoothstep(0.45, 0.8, up)));
    c = mix(uFloorCol, c, smoothstep(-0.3, 0.1, up));
    c = mix(c, uWallCol * 0.4, smoothstep(0.7, 1.0, up));
    // the lamp's shade: a soft bright disc, its bulb a sharp core
    vec3 L = normalize(uLampP - P);
    float k = max(dot(R, L), 0.0);
    float w = mix(0.012, 0.12, rough);
    float disc = smoothstep(1.0 - 0.03 - w, 1.0 - 0.03 + w * 0.3, k);
    float core = pow(k, mix(2500.0, 60.0, rough));
    c += uLampCol * uLampK * (disc * 0.8 + core * mix(3.0, 0.8, rough) + pow(k, 6.0) * 0.06);
    c += uFillCol * pow(max(dot(R, normalize(uFillDir)), 0.0), mix(60.0, 8.0, rough)) * 1.5;
    return c;
}
// metal 0..1, rough 0 (polished) .. 1 (matte)
vec3 shade(vec3 P, vec3 N, vec3 alb, float metal, float rough, float ao, float coat) {
    vec3 V = normalize(uEye - P);
    float nv = max(dot(N, V), 0.0);
    vec3 Lv = uLampP - P;
    float ld = length(Lv);
    vec3 L = Lv / ld;
    float nl = max(dot(N, L), 0.0);
    float sh = nl > 0.0 ? shadowAt(P, N) : 0.0;
    float att = uLampK * spotAt(P);
    vec3 diffC = alb * (1.0 - metal);
    vec3 c = diffC * (mix(uAmbLow, uAmb, 0.5 + 0.5 * N.y) * ao + uLampCol * att * nl * sh + uFillCol * max(dot(N, normalize(uFillDir)), 0.0) * ao);
    vec3 F0 = mix(vec3(0.04), alb, metal);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - nv, 5.0) * (1.0 - rough * 0.7);
    vec3 R = reflect(-V, N);
    vec3 H = normalize(L + V);
    float pw = mix(1400.0, 24.0, rough);
    float spec = pow(max(dot(N, H), 0.0), pw) * min((pw + 8.0) / 120.0, 4.0);
    c += F * uLampCol * att * spec * sh * nl * 0.6;
    // a metal's micro-roughness spreads some of the lamp over its whole lit side (brass reads golden, not black)
    c += alb * metal * uLampCol * att * nl * sh * mix(0.05, 0.22, rough);
    c += F * envAt(R, rough, P) * mix(1.0, 0.55, rough) * mix(0.35, 1.0, ao) * mix(0.25, 1.0, metal);
    // a clear lacquer over the wood (coat): a faint sharp reflection
    if (coat > 0.0) {
        float Fc = 0.04 + 0.96 * pow(1.0 - nv, 5.0);
        c += coat * Fc * envAt(R, 0.08, P) * 0.5;
        c += coat * Fc * uLampCol * att * pow(max(dot(N, H), 0.0), 600.0) * 4.0 * sh;
    }
    for (int i = 0; i < 8; i++) {
        if (i >= uNL) break;
        vec3 Lp = uLP[i].xyz - P;
        float d2 = dot(Lp, Lp);
        float r = uLP[i].w;
        if (d2 > r * r) continue;
        float d = sqrt(d2);
        Lp /= d;
        float a2 = (1.0 - smoothstep(0.3 * r, r, d)) / (1.0 + d2 / (r * r * 0.08));
        c += (diffC * (max(dot(N, Lp), 0.0) * 0.85 + 0.15) + F * pow(max(dot(N, normalize(Lp + V)), 0.0), pw * 0.5) * 2.0) * uLC[i].rgb * uLC[i].a * a2;
    }
    return c;
}
)";

inline const char* SOLID_FS = R"(#version 330 core
in vec3 vP, vL, vLN, vN;
flat in vec3 vHalf, vAX, vAY, vAZ;
flat in vec4 vCol, vMat, vE;
out vec4 frag;
uniform int uMesh;          // 0 box, 1 cylinder, 2 hemisphere
uniform float uStackA;      // the frame's alpha (1 on the stack, for the bridge)
uniform vec3 uBrass, uInlayA, uInlayB, uHoleC;
uniform float uHoleR, uDim;
uniform vec4 uBoxIn; // the box's inner half extents (x, z), its rim's height, its floor's

void main() {
    int mat = int(vMat.x + 0.5);
    // a gear's disc (brass, p1 = spokes): open between its spokes
    vec3 N = normalize(vN);
    // (the instance's axes may be left-handed: the vertex normal is right whatever the winding; only a hemisphere
    // seen from its open side is turned toward the eye)
    if (uMesh == 2 && dot(N, uEye - vP) < 0.0) N = -N;
    vec2 uv = vec2(0.0), sz = vec2(1.0);
    vec3 T = vAX, B = vAY;
    int face = 0; // box: 0 x sides, 1 top / bottom, 2 front / back (z)
    if (uMesh == 0) {
        if (abs(vLN.x) > 0.5) { uv = vL.zy; sz = vHalf.zy; T = vAZ; B = vAY; face = 0; }
        else if (abs(vLN.y) > 0.5) { uv = vL.xz; sz = vHalf.xz; T = vAX; B = vAZ; face = 1; }
        else { uv = vL.xy; sz = vHalf.xy; T = vAX; B = vAY; face = 2; }
    }
    vec2 q = uv * sz;
    vec3 alb = vCol.rgb;
    vec3 emit = vec3(0.0);
    float metal = 0.0, rough = 0.6, ao = 1.0, coat = 0.0;
    float seed = vCol.a;
    vec2 bend = vec2(0.0);
    vec3 Nl = vec3(0.0); // a normal offset in world space
    float px = max(fwidth(q.x), fwidth(q.y));

    if (mat == 0) {
        // a veneer: the grain along the longest side, lacquered; p0: inlay (1 stringing, 2 the lid's inside, 3 a
        // banding), p1: a darker figure
        vec2 g = sz.x >= sz.y ? q : q.yx;
        float detail = 1.0 - smoothstep(0.02, 0.12, px);
        float fig = fbm(vec2(g.x * 0.08, g.y * 1.3) + seed * 7.0);
        float streak = fbm(vec2(g.x * 0.03 + fig * 0.6, g.y * 5.5) + seed * 3.0);
        float rings = 0.5 + 0.5 * sin(g.y * 9.0 + fig * 7.0 + seed * 3.0);
        alb *= mix(1.0, (0.62 + 0.55 * streak) * (0.86 + 0.18 * rings), detail);
        alb = mix(alb, alb * 0.55, vMat.z * smoothstep(0.55, 0.75, streak) * detail);
        int inlay = int(vMat.y + 0.5);
        if (face == 2 && inlay > 0) {
            vec2 d = sz - abs(q);
            float e = min(d.x, d.y);
            // stringing: a pale line, a dark line, inset from the edge
            float w = 0.09;
            float s1 = 1.0 - smoothstep(w, w + px, abs(e - 1.4));
            float s2 = 1.0 - smoothstep(w * 0.7, w * 0.7 + px, abs(e - 1.75));
            alb = mix(alb, uInlayA, s1 * detail);
            alb = mix(alb, uInlayB, s2 * detail);
            if (inlay == 2) {
                // the lid's inside: a crossbanded border, a rosette in an oval medallion
                float band = step(1.75, e) * step(e, 3.2);
                vec2 bq = abs(q);
                float cross = fbm(vec2((bq.x > bq.y ? q.y : q.x) * 1.4, e * 0.25) + 9.0);
                alb = mix(alb, mix(uInlayB, alb, 0.5) * (0.8 + 0.3 * cross), band * 0.55 * detail);
                float s3 = 1.0 - smoothstep(w, w + px, abs(e - 3.2));
                alb = mix(alb, uInlayA, s3 * detail);
                vec2 m = (q - vec2(0.0, -sz.y * 0.3)) / vec2(sz.x * 0.25, sz.y * 0.5);
                float r = length(m);
                float a = atan(m.y, m.x);
                float ring1 = 1.0 - smoothstep(0.012, 0.012 + px / sz.y * 2.0, abs(r - 1.0));
                float ring2 = 1.0 - smoothstep(0.008, 0.008 + px / sz.y * 2.0, abs(r - 0.94));
                float inside = 1.0 - smoothstep(0.93, 0.94, r);
                // the medallion's ground: a quartered burr
                float burr = fbm(m * 9.0 + 3.0);
                vec3 ground = mix(alb * 0.7, alb * 1.1, burr);
                alb = mix(alb, ground, inside * detail);
                // the rosette: petals alternating two veneers, a dark heart
                float rr = r / 0.62;
                float pet = abs(sin(a * 8.0));
                float petal = step(rr, 0.35 + 0.65 * pow(pet, 0.6)) * step(rr, 1.0);
                float alt = step(0.0, sin(a * 8.0));
                vec3 pc = mix(uInlayA, mix(uInlayA, uInlayB, 0.55), alt);
                pc *= 0.85 + 0.25 * (1.0 - rr) ;
                alb = mix(alb, pc, petal * detail * 0.95);
                float heart = 1.0 - smoothstep(0.16, 0.16 + px / sz.y * 4.0, rr);
                alb = mix(alb, uInlayB * 0.6, heart * detail);
                alb = mix(alb, uInlayA, max(ring1, ring2 * 0.8) * detail);
            } else if (inlay == 3) {
                // the front: a brass escutcheon
                vec2 c = q - vec2(0.0, 0.0);
                float esc = step(length(c * vec2(1.0, 0.75)), 1.1);
                float key = step(length(c - vec2(0.0, 0.18)), 0.22) + step(abs(c.x), 0.08) * step(c.y, 0.18) * step(-0.5, c.y);
                if (esc > 0.5) {
                    alb = uBrass;
                    metal = 1.0;
                    rough = 0.25;
                    Nl = vAZ * 0.0 + (vAX * c.x + vAY * c.y) * 0.25;
                    if (key > 0.5) { alb = vec3(0.01); metal = 0.0; }
                }
            }
        }
        if (metal < 0.5) { rough = 0.55; coat = 1.0; }
    } else if (mat == 1) {
        // velvet: soft, the pile's sheen at grazing angles; p0 > 0: a round opening (the dancer's trap)
        if (vMat.y > 0.0 && length((vP - uHoleC).xz) < vMat.y) discard;
        vec3 V = normalize(uEye - vP);
        float nv = max(dot(N, V), 0.0);
        float n = vnoise(vP.xz * 9.0 + vP.y * 5.0) * 0.5 + vnoise(vP.xz * 30.0) * 0.5;
        alb *= 0.8 + 0.3 * n;
        emit += alb * uAmb * 1.5 * pow(1.0 - nv, 3.0);
        rough = 1.0;
        ao = 0.9;
    } else if (mat == 2) {
        // brass: p0 0 brushed, 1 polished, 2 turned (the cylinder: fine rings round its axis); p1 > 0: a gear's
        // disc with p1 spokes (open between them); p2 > 0: a screw's thread (p2 turns, moving with e.x)
        float spokes = vMat.z;
        if (uMesh == 1 && spokes > 0.5 && abs(vLN.y) > 0.5) {
            float r = length(vL.xz);
            float a = atan(vL.z, vL.x);
            float f = fract(a / 6.28318 * spokes);
            float arc = min(f, 1.0 - f) * 6.28318 * r / spokes;
            if (r > 0.3 && r < 0.82 && arc > 0.075) discard;
        }
        if (uMesh == 1 && spokes > 0.5 && abs(vLN.y) < 0.5) {
            // the rim of a gear: nothing to cut
        }
        int kind = int(vMat.y + 0.5);
        metal = 1.0;
        rough = kind == 1 ? 0.12 : 0.32;
        if (kind == 0) {
            // brushed along the instance's x
            float s = vnoise(vec2(dot(vP, vAX) * 0.6, dot(vP, vAZ + vAY) * 40.0) + seed);
            Nl += (vAY + vAZ) * (s - 0.5) * 0.08;
            alb *= 0.92 + 0.12 * s;
        } else if (kind == 2) {
            // turned: rings round the axis (the instance's y)
            float t = dot(vP, vAY);
            float s = vnoise(vec2(t * 35.0, 0.5) + seed);
            alb *= 0.93 + 0.1 * s;
            rough = 0.3;
        }
        if (vMat.w > 0.0 && uMesh == 1 && abs(vLN.y) < 0.5) {
            // an endless screw: a helical groove
            float a = atan(vL.z, vL.x) / 6.28318;
            float h = fract(vL.y * vMat.w + a + vE.x);
            float groove = smoothstep(0.35, 0.5, abs(h - 0.5) * 2.0);
            alb *= 1.0 - 0.6 * groove;
            Nl += vAY * (h - 0.5) * 0.6;
        }
        emit += alb * vE.y;
    } else if (mat == 3) {
        // steel (the comb, the screws, the fly's vanes): p0 0 bright nickel, 1 blued; brushed along x
        float s = vnoise(vec2(dot(vP, vAX) * 0.5, dot(vP, vAZ) * 30.0) + seed);
        alb *= 0.36 + 0.08 * s;
        Nl += vAZ * (s - 0.5) * 0.05;
        metal = 1.0;
        rough = 0.3;
        emit += vec3(1.0, 0.9, 0.7) * vE.y;
    } else if (mat == 4 || mat == 5) {
        // 4: a pin's head on the cylinder (a hemisphere); 5: a piece's plate in the well (a box), its domed boss.
        // p0: glow, p1: 1 enamel (else metal), p2: the boss' metal (0 its own, 1 brass)
        float enamel = vMat.z;
        metal = 1.0 - enamel;
        rough = enamel > 0.5 ? 0.08 : 0.2;
        coat = enamel;
        if (mat == 5 && uMesh == 0) {
            if (face == 2 && vLN.z > 0.0) {
                vec2 r = vL.xy;
                float ed = min(1.0 - abs(r.x), 1.0 - abs(r.y));
                // a bevel along the edge
                float bev = 1.0 - smoothstep(0.0, 0.16, ed);
                bend = vec2(abs(r.x) > abs(r.y) ? sign(r.x) : 0.0, abs(r.y) >= abs(r.x) ? sign(r.y) : 0.0) * bev * 0.9;
                // the domed boss: brass on enamel, polished on metal; a fine groove around it
                float br = length(r) / 0.56;
                if (br < 1.0) {
                    vec2 d = r / 0.56;
                    float h = sqrt(max(1.0 - br * br, 0.02));
                    bend = d * 0.9 / h * 0.55;
                    if (vMat.w > 0.5) { alb = uBrass * 1.05; metal = 1.0; coat = 0.0; }
                    rough = 0.08;
                }
                float groove = 1.0 - smoothstep(0.0, 0.05 + px * 0.5, abs(br - 1.08));
                alb *= 1.0 - 0.5 * groove;
                ao = 1.0 - 0.35 * groove;
                // engine-turned lines on the metal plates (a fine guilloché), none on the enamel
                if (enamel < 0.5 && br > 1.12) {
                    float gl = 0.5 + 0.5 * sin(length(r) * 70.0);
                    alb *= 0.9 + 0.12 * gl * (1.0 - smoothstep(0.02, 0.08, px));
                }
            } else {
                alb *= 0.6;
                rough = 0.35;
            }
        }
        emit += alb * vMat.y * 1.4 + uBrass * vMat.y * 0.3;
    } else if (mat == 6) {
        // the table: a dark polished top, its grain along x
        vec2 g = vP.xz;
        float detail = 1.0 - smoothstep(0.05, 0.4, px);
        float fig = fbm(vec2(g.x * 0.02, g.y * 0.25) + 4.0);
        float streak = fbm(vec2(g.x * 0.012 + fig, g.y * 0.9));
        alb *= mix(1.0, 0.75 + 0.4 * streak, detail);
        rough = 0.35;
        coat = 0.7;
    } else if (mat == 7) {
        // the wall: dim plaster, warmed by the lamp's spill
        alb *= 0.9 + 0.1 * fbm(vP.xy * 0.05);
        rough = 1.0;
    } else if (mat == 8) {
        // a light: lit by its own (p0 how bright)
        emit = alb * vMat.y;
        alb = vec3(0.0);
    } else if (mat == 9) {
        // porcelain (the dancer), gilt where p0 = 1
        if (vMat.y > 0.5) { alb = uBrass * 1.1; metal = 1.0; rough = 0.15; }
        else { rough = 0.1; coat = 1.0; alb *= 0.95; }
        emit += alb * vE.y;
    } else if (mat == 10) {
        // iron, black lacquer (the bedplate)
        float s = vnoise(vP.xz * 3.0 + seed);
        alb *= 0.85 + 0.25 * s;
        rough = 0.3;
        coat = 0.6;
    }

    // occlusion: the box's inside darkens toward its floor and its walls; the table round the box's foot
    {
        vec2 din = uBoxIn.xy - abs(vP.xz);
        if (din.x > -0.05 && din.y > -0.05 && vP.y < uBoxIn.z + 0.2 && vP.y > 0.5) {
            float w = min(din.x, din.y);
            ao *= mix(0.45, 1.0, smoothstep(0.0, 4.0, w)) * mix(0.55, 1.0, smoothstep(uBoxIn.w, uBoxIn.w + 6.0, vP.y));
        }
        if (mat == 6) {
            vec2 dout = abs(vP.xz) - uBoxIn.xy - vec2(1.8);
            float e = length(max(dout, 0.0));
            ao *= mix(0.35, 1.0, smoothstep(0.0, 9.0, e));
        }
    }
    vec3 Nb = N;
    if (uMesh == 0) Nb = normalize(N + T * bend.x + B * bend.y);
    Nb = normalize(Nb + Nl);
    vec3 c = shade(vP, Nb, alb, metal, rough, ao, coat) + emit;
    c *= 1.0 - uDim;
    frag = vec4(c, uStackA);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
in vec3 vP;
out vec4 frag;
void main() { frag = vec4(1.0); }
)";

// ---------------------------------------------------------------- glows (billboards)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iP;   // position, radius
layout(location = 2) in vec4 iC;   // color, intensity
uniform mat4 uVP;
uniform vec3 uOrigin, uCamR, uCamU;
uniform float uMinR;
out vec2 vC;
out vec4 vCol;
void main() {
    vC = aCorner;
    vCol = iC;
    vec3 c = iP.xyz;
    float d = length(c - uEye);
    float r = max(iP.w, d * uMinR);
    vCol.a *= min(1.0, (iP.w / r) * (iP.w / r) * 4.0);
    vec3 P = c + (uCamR * aCorner.x + uCamU * aCorner.y) * r;
    gl_Position = uVP * vec4(P - uOrigin, 1.0);
}
)";
inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vC;
in vec4 vCol;
out vec4 frag;
void main() {
    float r2 = dot(vC, vC);
    if (r2 > 1.0) discard;
    float core = exp(-r2 * 40.0);
    float halo = exp(-r2 * 6.0) * 0.35 + exp(-r2 * 2.0) * 0.1;
    // a faint four-pointed star on the brightest glints
    float star = exp(-abs(vC.x) * 40.0 - vC.y * vC.y * 6.0) + exp(-abs(vC.y) * 40.0 - vC.x * vC.x * 6.0);
    float a = (core * 1.4 + halo + star * 0.25) * (1.0 - r2);
    frag = vec4(vCol.rgb * vCol.a * a, 0.0);
}
)";

// ---------------------------------------------------------------- lines (screen-space): the ghost
inline const char* LINE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 iA;
layout(location = 2) in vec4 iB;
layout(location = 3) in vec4 iC;
uniform mat4 uVP;
uniform vec2 uRes;
out float vAcross;
out vec4 vCol;
out float vW;
void main() {
    vec4 c0 = uVP * vec4(iA.xyz, 1.0), c1 = uVP * vec4(iB.xyz, 1.0);
    vec2 s0 = c0.xy / c0.w * uRes * 0.5, s1 = c1.xy / c1.w * uRes * 0.5;
    vec2 dir = s1 - s0;
    dir = length(dir) > 1e-4 ? normalize(dir) : vec2(0.0, 1.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec4 c = mix(c0, c1, aCorner.x);
    float half_ = max(iA.w, 1.0) * 0.5 + 0.75;
    c.xy += nrm * aCorner.y * half_ / (uRes * 0.5) * c.w;
    vAcross = aCorner.y * half_;
    vW = iA.w;
    vCol = vec4(iC.rgb, iB.w);
    gl_Position = c;
}
)";
inline const char* LINE_FS = R"(#version 330 core
in float vAcross;
in vec4 vCol;
in float vW;
out vec4 frag;
void main() {
    float w = max(vW, 1.0);
    float a = clamp(w * 0.5 + 0.5 - abs(vAcross), 0.0, 1.0) * min(vW, 1.0) * vCol.a;
    frag = vec4(vCol.rgb * a, a);
}
)";

// ---------------------------------------------------------------- the shade behind the well and the HUD's columns
inline const char* VEIL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uVeil;
uniform float uWellA, uColA;
uniform vec4 uColL, uColR;
float box(vec2 p, vec2 c, vec2 h) { vec2 q = abs(p - c) - h; return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    float dw = box(w, vec2(0.0, 0.0), vec2(5.0, 10.0));
    float a = uWellA * (1.0 - smoothstep(-0.2, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.4, 2.6, dc)));
    }
    frag = vec4(uVeil * a, a);
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
    vec3 c = texture(uSrc, vUv + uTexel * vec2(-1, -1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, -1)).rgb
           + texture(uSrc, vUv + uTexel * vec2(-1, 1)).rgb + texture(uSrc, vUv + uTexel * vec2(1, 1)).rgb;
    c *= 0.25;
    float l = max(max(c.r, c.g), c.b);
    frag = vec4(c * max(l - uThr, 0.0) / max(l, 1e-4), 1.0);
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
uniform float uBloomK, uSlow, uDim, uFade, uBridge, uExposure, uSatF, uVig;
uniform vec3 uBridgeCol, uBridgeInk, uSlowTint;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 b = texture(uBloom, vUv).rgb;
    vec3 c = s.rgb * uExposure + b * uBloomK;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = max(mix(vec3(l), c, uSatF), 0.0);
    c = mix(c, mix(vec3(l), c, 0.7) * uSlowTint, uSlow);
    c = c * (1.0 + c / 9.0) / (1.0 + c);
    c = pow(c, vec3(0.94)) * 1.1;
    vec2 v = vUv - 0.5;
    c *= 1.0 - uVig * dot(v, v) * 2.0;
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
    float halo = smoothstep(0.12, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

inline std::string withCommon(const char* src, bool lighting = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (lighting ? LIGHTING : "") + s.substr(nl + 1);
}

} // namespace musicbox_shaders

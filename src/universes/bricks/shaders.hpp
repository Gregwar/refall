#pragma once
// BRICKS' shaders (see bricks.cpp).
//
// STUDIO_FS draws the photo studio per pixel from the camera's ray: a seamless sweep (the table curving up into the
// backdrop), a soft pool of light around the well, the soft shadows of everything standing on it (from the shadow
// map) and a contact shade under the finished models. BRICK_VS / BRICK_FS draw every solid: the toy bricks (the
// stack's cells merged per piece, the models' bricks, slopes, the studs), the well's board and floor plate, the
// turntables, all in glossy ABS plastic (soft specular, a reflection of the studio's softbox, a little warmth in the
// shadow side), with molded bevels on their edges computed per fragment. FLAT draws the ghost's outline; FINAL tone
// maps and encodes the frame.
#include <string>

namespace bricks_shaders {

// ---------------------------------------------------------------- common: the studio (also seen in reflections)
inline const char* COMMON = R"(
uniform float uSeed;
uniform float uTable, uWall;          // the table's plane (y), the backdrop's plane (z)
uniform vec3 uTableNear, uTableFar, uWallTop, uPoolCol;
uniform vec3 uLightDir, uLightCol;
uniform vec3 uEye;
uniform vec2 uLamp;                   // the studio's light opening (0 a small spot on the well .. 1 the whole studio), the spot's own light
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + uSeed) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
// The sweep: one coordinate running along the table, then up the backdrop, so the table curves into the wall without
// a line. Returns the color and the hit point (w = 0 table, 1 wall, 2 none).
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
    float up = clamp((s + uWall) / 120.0, 0.0, 1.0);
    c = mix(c, uWallTop, smoothstep(0.0, 1.0, up));
    // the pool of the key light, wide and soft, around the well
    vec2 q = vec2(p.x + 4.0, (hit.w < 0.5 ? p.z : uWall - (p.y - uTable)) + 10.0);
    c += uPoolCol * exp(-dot(q, q) / 2600.0);
    // the setup / teardown: the light opens out of a small spot on the well (and closes back onto it)
    c *= mix(uLamp.y * exp(-dot(q, q) / (40.0 + 2600.0 * uLamp.x)), 1.0, pow(uLamp.x, 1.5));
    return c;
}
vec3 studio(vec3 rd) {
    vec4 h;
    if (rd.z > -0.05 || rd.y > 0.25) {
        // behind the camera and above: the studio's soft walls, a broad softbox overhead
        vec3 c = mix(uTableNear * 0.75, uWallTop * 1.1, smoothstep(-0.4, 0.6, rd.y));
        c += uLightCol * 0.5 * smoothstep(0.3, 0.95, rd.y) ;
        return c;
    }
    return studioAt(uEye, rd, h);
}
)";

// ---------------------------------------------------------------- the studio (background)
inline const char* STUDIO_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform mat4 uInvVP, uLightVP;
uniform sampler2DShadow uShadow;
uniform vec4 uBlobs[24];      // contact shades: x, z, radius, strength
uniform int uBlobCount;
uniform float uShadowK, uGrain;
float shadowAt(vec3 p) {
    vec4 l = uLightVP * vec4(p, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    float r = 1.6 / 4096.0, sum = 0.0;
    for (int i = -2; i <= 2; i++)
        for (int j = -2; j <= 2; j++) sum += texture(uShadow, vec3(s.xy + vec2(i, j) * r, s.z - 0.0015));
    return sum / 25.0;
}
void main() {
    vec4 a = uInvVP * vec4(vUv * 2.0 - 1.0, -1.0, 1.0);
    vec4 b = uInvVP * vec4(vUv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ro = a.xyz / a.w, rd = normalize(b.xyz / b.w - ro);
    vec4 hit;
    vec3 c = studioAt(ro, rd, hit);
    if (hit.w < 0.5) {
        float sh = shadowAt(hit.xyz);
        float ao = 1.0;
        for (int i = 0; i < 24; i++) {
            if (i >= uBlobCount) break;
            vec2 d = (hit.xz - uBlobs[i].xy) / uBlobs[i].z;
            ao *= 1.0 - uBlobs[i].w * exp(-dot(d, d) * 2.2);
        }
        c *= mix(1.0, 0.62, (1.0 - sh) * uShadowK) * ao;
    }
    // a fine grain, barely there (no pattern)
    c *= 1.0 + (hash(gl_FragCoord.xy * 0.37) - 0.5) * uGrain;
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- solids
// Instance: a = center (studs: base center), stack neighbor mask; b = size, slope; c = color (linear), fog;
// d = rotation (quaternion); e = stack (bridge mask), material (0 plastic, 1 board, 2 satin), bevel width, glint.
inline const char* BRICK_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 iA;
layout(location = 3) in vec4 iB;
layout(location = 4) in vec4 iC;
layout(location = 5) in vec4 iD;
layout(location = 6) in vec4 iE;
uniform mat4 uVP;
uniform int uStud;
out vec3 vWorld, vObj, vNrmObj;
flat out vec3 vFace, vSize, vCol;
flat out vec4 vQuat, vE;
flat out float vMask, vFog;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
void main() {
    vec3 size = iB.xyz;
    vec3 p = aPos;
    vec3 n = aNrm;
    int mask = int(iA.w + 0.5);
    if (uStud == 0) {
        // the faces between two cells of one piece are removed (one brick per piece)
        if ((n.x < -0.5 && (mask & 1) != 0) || (n.x > 0.5 && (mask & 2) != 0) ||
            (n.y > 0.5 && (mask & 4) != 0) || (n.y < -0.5 && (mask & 8) != 0)) {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }
        // a slope: the top edge toward +z goes down
        if (iB.w > 0.0) {
            if (p.y > 0.0 && p.z > 0.0) p.y -= iB.w;
            if (n.y > 0.5) n = vec3(0.0, size.z, iB.w * size.y);
        }
    }
    vec3 obj = p * size;
    vec3 nObj = normalize(n / size);
    vec3 w = iA.xyz + qrot(iD, obj);
    vWorld = w;
    vObj = obj;
    vNrmObj = nObj;
    vFace = aNrm;
    vSize = size;
    vCol = iC.rgb;
    vFog = iC.a;
    vQuat = iD;
    vE = iE;
    vMask = iA.w;
    gl_Position = uVP * vec4(w, 1.0);
}
)";

inline const char* SHADOW_FS = R"(#version 330 core
void main() {}
)";

inline const char* BRICK_FS = R"(#version 330 core
in vec3 vWorld, vObj, vNrmObj;
flat in vec3 vFace, vSize, vCol;
flat in vec4 vQuat, vE;
flat in float vMask, vFog;
out vec4 frag;
uniform mat4 uLightVP;
uniform sampler2DShadow uShadow;
uniform sampler2D uOcc;           // the stack's cells (board AO)
uniform vec3 uAmbTop, uAmbBot, uFillDir, uFillCol;
uniform float uSpec, uWarm, uShadowK;
vec3 qrot(vec4 q, vec3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float shadowAt(vec3 p) {
    vec4 l = uLightVP * vec4(p, 1.0);
    vec3 s = l.xyz / l.w * 0.5 + 0.5;
    if (s.x < 0.0 || s.y < 0.0 || s.x > 1.0 || s.y > 1.0 || s.z > 1.0) return 1.0;
    float r = 1.2 / 4096.0, sum = 0.0;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) sum += texture(uShadow, vec3(s.xy + vec2(i, j) * r, s.z - 0.0006));
    return sum / 9.0;
}
// a neighbor of the same piece along an axis (x: 1 / 2, y: 4 / 8); along z there is none
bool same(int mask, int axis, float sgn) {
    if (axis == 0) return (mask & (sgn < 0.0 ? 1 : 2)) != 0;
    if (axis == 1) return (mask & (sgn > 0.0 ? 4 : 8)) != 0;
    return false;
}
void main() {
    int mask = int(vMask + 0.5);
    vec3 n = vNrmObj;
    float bw = vE.z;
    float groove = 1.0;
    if (bw > 0.0) {
        // molded bevels: the normal leans out toward each free edge of the face
        int fa = abs(vFace.x) > 0.5 ? 0 : (abs(vFace.y) > 0.5 ? 1 : 2);
        vec3 tilt = vec3(0.0);
        float dmin = 1e3;
        for (int ax = 0; ax < 3; ax++) {
            if (ax == fa) continue;
            float h = vSize[ax] * 0.5;
            float c = vObj[ax];
            for (int k = 0; k < 2; k++) {
                float sg = k == 0 ? -1.0 : 1.0;
                if (same(mask, ax, sg)) continue;
                float d = h - c * sg;
                float wgt = pow(1.0 - smoothstep(0.0, bw, d), 1.5);
                vec3 dir = vec3(0.0);
                dir[ax] = sg;
                tilt += dir * wgt * 1.4;
                dmin = min(dmin, d);
            }
        }
        // inner corners of a piece (front and back faces): a small round bevel
        if (fa == 2) {
            for (int k = 0; k < 4; k++) {
                float sx = (k & 1) == 0 ? -1.0 : 1.0, sy = k < 2 ? 1.0 : -1.0;
                int diag = sy > 0.0 ? (sx < 0.0 ? 16 : 32) : (sx < 0.0 ? 64 : 128);
                if (same(mask, 0, sx) && same(mask, 1, sy) && (mask & diag) == 0) {
                    vec2 cd = vec2(vSize.x * 0.5 - vObj.x * sx, vSize.y * 0.5 - vObj.y * sy);
                    float d = length(cd);
                    float wgt = pow(1.0 - smoothstep(0.0, bw * 1.2, d), 1.5);
                    tilt += vec3(sx * cd.x, sy * cd.y, 0.0) / max(d, 1e-3) * wgt * 1.4;
                    dmin = min(dmin, d);
                }
            }
        }
        n = normalize(n + tilt);
        groove = mix(0.5, 1.0, smoothstep(bw * 0.06, bw * 0.4, dmin));
    }
    vec3 N = normalize(qrot(vQuat, n));
    vec3 V = normalize(uEye - vWorld);
    vec3 L = normalize(uLightDir);
    float sh = mix(1.0, shadowAt(vWorld + N * 0.02), uShadowK);
    float ao = groove;
    int mat = int(vE.y + 0.5);
    if (mat == 1) {
        // the board behind the well: shaded by the cells in front of it
        vec2 uv = vec2((vWorld.x + 5.0) / 10.0, (10.0 - vWorld.y) / 22.0 + 2.0 / 22.0);
        float occ = texture(uOcc, uv).r;
        ao *= 1.0 - 0.35 * occ;
    }
    // under a brick, the side faces darken toward their foot (contact)
    if (abs(vFace.y) < 0.5 && mat == 0 && vE.x < 0.5)
        ao *= mix(0.82, 1.0, smoothstep(0.0, 0.35, vObj.y + vSize.y * 0.5));
    vec3 alb = vCol;
    float ndl = dot(N, L);
    float wrap = max((ndl + 0.25) / 1.25, 0.0);
    vec3 amb = mix(uAmbBot, uAmbTop, N.y * 0.5 + 0.5);
    float fill = max(dot(N, normalize(uFillDir)), 0.0);
    vec3 col = alb * (uLightCol * wrap * sh + amb * ao + uFillCol * fill * ao);
    // a little warmth in the shadow side (the plastic lets light through)
    col += alb * alb * uLightCol * uWarm * (1.0 - smoothstep(-0.2, 0.5, ndl)) * smoothstep(-0.8, -0.2, ndl);
    // glossy ABS: a soft highlight of the key, the studio's softbox in the reflection
    float rough = mat == 0 ? 0.32 : (mat == 1 ? 0.6 : 0.45);
    vec3 H = normalize(L + V);
    float nh = max(dot(N, H), 0.0);
    float a2 = rough * rough * rough * rough;
    float dd = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159 * dd * dd);
    float F = 0.04 + 0.96 * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    float spec = D * F * 0.25 * max(ndl, 0.0) * sh;
    float Fv = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 R = reflect(-V, N);
    vec3 env = studio(R);
    float box = smoothstep(0.82, 0.97, dot(R, L)) * (0.6 + 0.4 * sh);
    float gloss = mat == 0 ? 1.0 : (mat == 1 ? 0.25 : 0.6);
    col = col * (1.0 - Fv * 0.6 * gloss) + (env * 0.55 * Fv + uLightCol * box * 0.35 * (0.3 + Fv)) * gloss * ao;
    col += uLightCol * spec * uSpec * gloss;
    col += vec3(vE.w) * alb;   // a soft glow (completed model, celebrations)
    // far away, the studio's haze
    if (vFog > 0.0) col = mix(col, studio(-V), vFog);
    frag = vec4(col, vE.x);
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
    // a soft shoulder (highlights roll off, mids untouched)
    vec3 x = max(c - 0.0, 0.0);
    return x / (1.0 + max(x - 0.75, 0.0) * 0.9);
}
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = tonemap(s.rgb);
    // slow motion: an evening tint over the world
    c = mix(c, c * uSlowTint, uSlow);
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.22 * smoothstep(0.35, 0.95, length(q * vec2(1.0, 0.8)));
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

// A shader with the common functions inserted after its version line.
inline std::string withCommon(const char* fs) {
    std::string s(fs);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + s.substr(nl + 1);
}

} // namespace bricks_shaders

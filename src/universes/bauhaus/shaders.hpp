#pragma once
// BAUHAUS shaders: flat, crisp shapes (rotated rectangles, disc sectors and rings, triangles) for the game, SDF type,
// the poster's paper cut-outs (thick cards ray-cast in their box and lit by one light, with analytic soft shadows on
// what lies under them), and the print: the slow motion's photogram, the paper's grain, the poster knocked back
// behind the well and the HUD.

namespace bauhaus_shaders {

// Instanced shapes in pixels (top-left origin), each clipped to a rectangle.
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // -1..1
layout(location = 1) in vec4 aGeom;     // center x, y, half sizes x, y (pixels)
layout(location = 2) in vec4 aCol;      // sRGB, straight alpha
layout(location = 3) in vec4 aKind;     // kind (0 box, 1 disc, 2 triangle), rotation, p1, p2
layout(location = 4) in vec4 aExtra;    // p3
layout(location = 5) in vec4 aClip;     // x0, y0, x1, y1 (pixels)
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vGeom, vCol, vKind, vExtra, vClip;
void main() {
    float b = length(aGeom.zw) + 1.5;
    vec2 p = aGeom.xy + aCorner * b;
    vPix = p;
    vGeom = aGeom; vCol = aCol; vKind = aKind; vExtra = aExtra; vClip = aClip;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vGeom, vCol, vKind, vExtra, vClip;
out vec4 frag;
float span(float p, float a, float b) { return clamp(min(p - a, b - p) + 0.5, 0.0, 1.0); }
float sdBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float sdTri(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                 vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}
void main() {
    vec2 d = vPix - vGeom.xy;
    float cr = cos(vKind.y), sr = sin(vKind.y);
    vec2 q = vec2(cr * d.x + sr * d.y, -sr * d.x + cr * d.y); // into the shape's frame (y down)
    vec2 h = vGeom.zw;
    float dist;
    if (vKind.x < 0.5) {
        dist = sdBox(q, h, min(vKind.z, min(h.x, h.y)));
    } else if (vKind.x < 1.5) {
        // a disc of radius h.x, hollowed to a ring by p1, cut to a sector centered on angle p2 (from the top,
        // clockwise) of half span p3 (pi or more: whole)
        float r = length(q);
        dist = max(r - h.x, vKind.z - r);
        if (vExtra.x < 3.14) {
            float a = atan(q.x, -q.y) - vKind.w;
            a = mod(a + 3.14159265, 6.2831853) - 3.14159265;
            dist = max(dist, (abs(a) - vExtra.x) * max(r, 1.0));
        }
    } else {
        // p1 0: isosceles, its apex up; 1: right-angled at the bottom left
        if (vKind.z < 0.5) dist = sdTri(q, vec2(0.0, -h.y), vec2(h.x, h.y), vec2(-h.x, h.y));
        else dist = sdTri(q, vec2(-h.x, -h.y), vec2(-h.x, h.y), vec2(h.x, h.y));
    }
    float cov = clamp(0.5 - dist, 0.0, 1.0);
    cov *= span(vPix.x, vClip.x, vClip.z) * span(vPix.y, vClip.y, vClip.w);
    float a = vCol.a * cov;
    frag = vec4(vCol.rgb * a, a);
}
)";

// SDF type, one flat ink.
inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
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
uniform vec3 uInk;
uniform float uAlpha;
uniform float uSoft;        // > 0: the letters' soft shadow (its spread in SDF units)
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-4) * 0.7;
    float a = smoothstep(0.5 - w - uSoft, 0.5 + w + uSoft * 0.3, d) * uAlpha;
    frag = vec4(uInk * a, a);
}
)";

// The print. The poster's papers (and their shadows) are found by their distance to the sheet: in slow motion the
// sheet becomes a photogram (a dark ground, the cut-outs left pale where they hid the light, the soft shadows of
// their raised edges a grey halo, the darkest papers palest). Then the veil behind the well and the HUD (a union of
// rectangles), the paper's grain and fibers, a faint mottle.
inline const char* COMP_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPoster;
uniform vec2 uRes;
uniform vec3 uPaper;        // the variant's paper (what is not ink)
uniform vec3 uGround;       // the ground as shown (the veil's color)
uniform float uSlow;
uniform float uGrain;
uniform int uCount;
uniform vec4 uRects[24];    // pixels, top-left origin
uniform vec2 uSoft[24];     // feather (pixels), strength 0..1
uniform vec2 uViewC;
uniform float uUnitPx;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void main() {
    vec3 c = texture(uPoster, vUv).rgb;
    vec2 p = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
    float ink = clamp(length(c - uPaper) * 2.2, 0.0, 1.0);
    if (uSlow > 0.001) {
        float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
        float g = ink * (0.45 + 0.5 * (1.0 - lum));
        vec3 photo = mix(vec3(0.075, 0.075, 0.08), vec3(0.9, 0.89, 0.86), g);
        c = mix(c, photo, uSlow);
    }
    float k = 0.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uCount) break;
        vec4 r = uRects[i];
        vec2 q = max(max(r.xy - p, p - r.zw), vec2(0.0));
        float m = 1.0 - smoothstep(0.0, max(uSoft[i].x, 1.0), length(q));
        k = max(k, m * uSoft[i].y);
    }
    // the veil is paper too: in a drop the spotlight passes over it, more softly
    if (uLit.w > 0.001) {
        vec3 P = vec3(uViewC.x + (p.x - uRes.x * 0.5) / uUnitPx, uViewC.y - (p.y - uRes.y * 0.5) / uUnitPx, 0.0);
        c = mix(c, uGround * mix(1.0, min(lightAt(P, vec3(0.0, 0.0, 1.0)), 1.0), 0.55), k);
    } else c = mix(c, uGround, k);
    // the sheet: a fine grain, long soft fibers, the ink a little uneven
    float grain = hash(floor(p)) - 0.5;
    float fiber = vnoise(p * vec2(0.012, 0.05)) - 0.5;
    float mottle = vnoise(p * 0.035) + 0.5 * vnoise(p * 0.11) - 0.75;
    c += uGrain * (grain * 0.022 + fiber * 0.018);
    c *= 1.0 + uGrain * mottle * 0.05 * ink * (1.0 - k);
    frag = vec4(c, 1.0);
}
)";

// ---- The paper cut-outs. A card is one of the flat shapes (in board units, its frame y down like the shapes') cut
// out of paper of some thickness, placed in space by a center and a rotation. Shared: its outline's distance and the
// light (a far sun over the sheet, or the drop's spotlight).
inline const char* CARD_COMMON = R"(#version 330 core
uniform vec3 uC;            // the card's center (board units)
uniform mat3 uR;            // its rotation (local -> world)
uniform vec3 uHalf;         // half sizes x, y, half thickness
uniform vec4 uKind;         // kind, p1, p2, p3 (as the shapes', in board units)
uniform vec3 uLight;        // the light's position
uniform vec3 uSpotDir;      // the spotlight's axis
uniform vec4 uLit;          // ambient, diffuse, normalization (a flat card in the sun: 1), spot amount
float sdBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float sdTri(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                 vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}
// the outline's signed distance at a local point (x right, y up)
float outline(vec2 pl) {
    vec2 q = vec2(pl.x, -pl.y);
    vec2 h = uHalf.xy;
    if (uKind.x < 0.5) return sdBox(q, h, min(uKind.y, min(h.x, h.y)));
    if (uKind.x < 1.5) {
        float r = length(q);
        float dist = uKind.y > 0.0 ? max(r - h.x, uKind.y - r) : r - h.x;
        if (uKind.w < 3.14) {
            float a = atan(q.x, -q.y) - uKind.z;
            a = mod(a + 3.14159265, 6.2831853) - 3.14159265;
            dist = max(dist, (abs(a) - uKind.w) * max(r, 0.02));
        }
        return dist;
    }
    if (uKind.y < 0.5) return sdTri(q, vec2(0.0, -h.y), vec2(h.x, h.y), vec2(-h.x, h.y));
    return sdTri(q, vec2(-h.x, -h.y), vec2(-h.x, h.y), vec2(h.x, h.y));
}
// the light a surface gets (1: a flat card in the sun)
float lightAt(vec3 P, vec3 N) {
    vec3 l = normalize(uLight - P);
    float cone = mix(1.0, smoothstep(0.55, 0.92, dot(-l, uSpotDir)), uLit.w);
    float v = (uLit.x + uLit.y * max(dot(N, l), 0.0) * cone) / uLit.z;
    return v < 1.0 ? v : 1.0 + (v - 1.0) * 0.3; // the highlights held back
}
)";

// A card: its box rasterized, the ray from the eye cast into it (its front or back face, or its cut edge, marched on
// the outline's distance), the depth written where it is hit. The front is the color with a faint paper texture, the
// back plain paper, the cut edge the paper's core; on the faces a thin bevel along the outline catches the light on
// the light's side and falls into shade on the other (the cut's thickness seen from above).
inline const char* CARD_VS = R"(#version 330 core
layout(location = 0) in vec3 aPos; // -1..1
uniform mat4 uVP;
uniform vec3 uC;
uniform mat3 uR;
uniform vec3 uHalf;
uniform float uPad;
out vec3 vWorld;
void main() {
    vec3 l = aPos * vec3(uHalf.xy + uPad, uHalf.z + 0.002);
    vWorld = uC + uR * l;
    gl_Position = uVP * vec4(vWorld, 1.0);
}
)";
inline const char* CARD_FS = R"(
in vec3 vWorld;
out vec4 frag;
uniform mat4 uVP;
uniform vec3 uEye;
uniform float uPad;
uniform vec4 uCol;          // sRGB, alpha
uniform vec3 uBack;         // its back
uniform vec3 uEdge;         // its cut edge
uniform float uSeed;
uniform float uTex;         // the paper texture's strength
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
vec2 grad(vec2 p) {
    vec2 e = vec2(0.003, 0.0);
    return normalize(vec2(outline(p + e.xy) - outline(p - e.xy), outline(p + e.yx) - outline(p - e.yx)) + 1e-7);
}
void main() {
    mat3 Rt = transpose(uR);
    vec3 ro = Rt * (uEye - uC);
    vec3 rd = normalize(Rt * (vWorld - uEye));
    if (abs(rd.z) < 1e-4) rd.z = 1e-4;
    vec3 b = vec3(uHalf.xy + uPad, uHalf.z);
    // the faces' planes: the one facing the eye, the other
    float zs = ro.z > 0.0 ? 1.0 : -1.0;
    float tF = (zs * b.z - ro.z) / rd.z, tB = (-zs * b.z - ro.z) / rd.z;
    vec2 pF = (ro + rd * tF).xy, pB = (ro + rd * tB).xy;
    float dF = outline(pF), dB = outline(pB);
    float pw = clamp(length(fwidth(pF)) * 0.7071, 1e-4, 0.5); // a pixel, in board units on the face
    // the slab's box
    vec3 inv = 1.0 / vec3(abs(rd.x) < 1e-6 ? 1e-6 : rd.x, abs(rd.y) < 1e-6 ? 1e-6 : rd.y, rd.z);
    vec3 t0 = (-b - ro) * inv, t1 = (b - ro) * inv;
    vec3 tmn = min(t0, t1), tmx = max(t0, t1);
    float tn = max(max(tmn.x, tmn.y), tmn.z), tf = min(min(tmx.x, tmx.y), tmx.z);
    if (tf < max(tn, 0.0)) discard;
    float cov = clamp(0.5 - min(dF, dB) / pw, 0.0, 1.0);
    if (cov < 0.004) discard;
    // march into the slab
    float s = tn, d = 1.0;
    vec3 p = ro + rd * s;
    for (int i = 0; i < 28; i++) {
        p = ro + rd * s;
        d = outline(p.xy);
        if (d < 2e-4 || s > tf) break;
        s += max(d / max(length(rd.xy), 1e-3), 2e-4);
    }
    bool wall = false, frontFace = zs > 0.0;
    vec3 n = vec3(0.0, 0.0, zs);
    if (d < 2e-4 && s <= tf + 1e-4) {
        if (abs(abs(p.z) - b.z) > 1e-3) {
            wall = true; // the cut edge
            n = vec3(grad(p.xy), 0.0);
        } else if (sign(p.z) != zs) { frontFace = !frontFace; n = -n; }
    } else {
        // the fringe beyond the outline: the nearer face
        p = dF <= dB ? vec3(pF, zs * b.z) : vec3(pB, -zs * b.z);
        if (dF > dB) { frontFace = !frontFace; n = -n; }
    }
    vec3 c = wall ? uEdge : frontFace ? uCol.rgb : uBack;
    vec3 P = uC + uR * p;
    if (!wall) {
        // the paper: long soft fibers and a fine tooth, fixed to the card
        vec2 tp = p.xy * 45.0 + uSeed * 37.0;
        float fib = vnoise(tp * vec2(0.03, 0.12)) - 0.5, tooth = vnoise(tp * 0.6) - 0.5;
        c *= 1.0 + uTex * (fib * 0.07 + tooth * 0.035);
        // the bevel along the outline: the cut's thickness, lit on the light's side
        float bev = 1.0 - smoothstep(0.0, 2.4 * pw, -outline(p.xy));
        if (bev > 0.001) {
            vec2 g = grad(p.xy);
            vec3 lw = Rt * (uLight - P);
            float side = dot(g, normalize(lw.xy + 1e-5));
            c = mix(c, mix(c, uEdge, 0.55) * (1.0 + 0.3 * max(side, 0.0)), bev * max(side, 0.0));
            c *= 1.0 - 0.3 * bev * max(-side, 0.0);
        }
    }
    c *= lightAt(P, normalize(uR * n));
    vec4 cp = uVP * vec4(P, 1.0);
    gl_FragDepth = clamp(cp.z / cp.w * 0.5 + 0.5, 0.0, 1.0);
    float a = uCol.a * cov;
    frag = vec4(c * a, a);
}
)";

// A card's shadow: a quad on the plane just under it (uZq) covering where its shadow falls; per pixel the ray toward
// the light is cut by the card's plane and the outline's distance there blurred by the penumbra (wider the farther
// the card is above), plus a faint contact shade straight under it. It multiplies what is already drawn (the sheet,
// the cards under it).
inline const char* SHADOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner; // -1..1
uniform mat4 uVP;
uniform vec4 uRect;   // x0, y0, x1, y1 on the plane
uniform float uZq;
out vec3 vWorld;
void main() {
    vWorld = vec3(mix(uRect.xy, uRect.zw, aCorner * 0.5 + 0.5), uZq);
    gl_Position = uVP * vec4(vWorld, 1.0);
}
)";
inline const char* SHADOW_FS = R"(
in vec3 vWorld;
out vec4 frag;
uniform float uLr;          // the light's radius (the penumbra)
uniform vec3 uShade;        // the paper in full shadow (a multiplier)
uniform vec2 uStrength;     // the cast shadow's, the contact shade's
void main() {
    mat3 Rt = transpose(uR);
    vec3 P = vWorld;
    vec3 Pl = Rt * (P - uC), Ll = Rt * (uLight - uC);
    float sh = 0.0;
    float den = Ll.z - Pl.z;
    if (abs(den) > 1e-5) {
        float t = -Pl.z / den;
        if (t > 0.0 && t < 1.0) {
            vec3 H = Pl + (Ll - Pl) * t;
            float L = length(Ll - Pl);
            float gap = t * L;
            float w = uLr * gap / max(L - gap, 0.5) + 0.02 + uHalf.z * 0.5;
            sh = 1.0 - smoothstep(-w, w, outline(H.xy));
            sh *= 1.0 - 0.6 * smoothstep(3.0, 14.0, gap); // a high card's shadow pales
        }
    }
    float ao = 0.0;
    vec3 up = Rt * vec3(0.0, 0.0, 1.0);
    if (abs(up.z) > 0.05) {
        float t = -Pl.z / up.z;
        if (t > 0.0) {
            vec3 H = Pl + up * t;
            float w = 0.05 + t * 0.7;
            ao = (1.0 - smoothstep(-w, w, outline(H.xy))) * exp(-t * 2.0);
        }
    }
    float dark = 1.0 - (1.0 - uStrength.x * sh) * (1.0 - uStrength.y * ao);
    frag = vec4(mix(vec3(1.0), uShade, dark), 1.0);
}
)";

// The sheet under the drop's spotlight: a multiplier of the light it gets (a pool under the spot, dimmer around).
inline const char* SHEET_FS = R"(
in vec2 vUv;
out vec4 frag;
uniform vec2 uRes;
uniform vec2 uViewC;
uniform float uUnitPx;
void main() {
    vec2 p = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
    vec3 P = vec3(uViewC.x + (p.x - uRes.x * 0.5) / uUnitPx, uViewC.y - (p.y - uRes.y * 0.5) / uUnitPx, 0.0);
    frag = vec4(vec3(lightAt(P, vec3(0.0, 0.0, 1.0))), 1.0);
}
)";

} // namespace bauhaus_shaders

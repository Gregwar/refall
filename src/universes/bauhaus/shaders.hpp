#pragma once
// BAUHAUS shaders: flat, crisp shapes (rotated rectangles, disc sectors and rings, triangles), SDF type, and the
// print: the drop's great disc cut out of the poster, the slow motion's photogram, the paper's grain and the ink's
// mottle, the poster knocked back behind the well and the HUD.

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
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-4) * 0.7;
    float a = smoothstep(0.5 - w, 0.5 + w, d) * uAlpha;
    frag = vec4(uInk * a, a);
}
)";

// The print. The poster's inks are found by their distance to the paper: inside the drop's great disc they turn to
// cut-outs (paper on the disc's color); in slow motion the sheet becomes a photogram (a dark ground, the forms
// left pale where they hid the light, the darkest inks palest). Then the veil behind the well and the HUD (a union
// of rectangles), the paper's grain and fibers, a faint mottle in the inks.
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPoster;
uniform vec2 uRes;
uniform vec3 uPaper;        // the variant's paper (what is not ink)
uniform vec3 uGround;       // the ground as shown (the veil's color)
uniform vec4 uDisc;         // the drop's disc: center, radius (pixels), strength
uniform vec3 uDiscCol;
uniform float uSlow;
uniform float uGrain;
uniform int uCount;
uniform vec4 uRects[24];    // pixels, top-left origin
uniform vec2 uSoft[24];     // feather (pixels), strength 0..1
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
    if (uDisc.w > 0.001) {
        float m = clamp(uDisc.z - length(p - uDisc.xy) + 0.5, 0.0, 1.0) * uDisc.w;
        c = mix(c, mix(uDiscCol, uPaper, ink), m);
        ink = mix(ink, 1.0 - ink, m);
    }
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
    c = mix(c, uGround, k);
    // the sheet: a fine grain, long soft fibers, the ink a little uneven
    float grain = hash(floor(p)) - 0.5;
    float fiber = vnoise(p * vec2(0.012, 0.05)) - 0.5;
    float mottle = vnoise(p * 0.035) + 0.5 * vnoise(p * 0.11) - 0.75;
    c += uGrain * (grain * 0.022 + fiber * 0.018);
    c *= 1.0 + uGrain * mottle * 0.05 * ink * (1.0 - k);
    frag = vec4(c, 1.0);
}
)";

} // namespace bauhaus_shaders

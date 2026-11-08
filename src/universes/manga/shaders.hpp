#pragma once
// MANGA's shaders: inked shapes in screentones and washes of color, and the lettering.

namespace manga_shaders {

// ---------------------------------------------------------------- ink
// Triangles in pixels (top-left origin), each printed in a screentone: solid, dots (an AM halftone, white dots on
// black past half density), parallel lines, crosshatch, a disc (a single dot: the crumbs, the lamps' glow) or sand
// (scattered dots). The pattern is set from an origin, so a tone moves with its piece; the density is
// interpolated (gradient tones). Every edge of a dot or a line is antialiased over a pixel, and cells are never
// smaller than a few pixels, so the tones stay calm in motion. A shape may be clipped to a panel (a convex quad).
// A wash (kind 6) is a flat of color, its color interpolated per vertex (gradients): gently textured like
// watercolor on paper (a slow mottle, a fine grain), its density the colorist's progress, flooding in through a
// noise with a darker wet edge; on a printed page (uPrint) it is set in CMYK halftone dots
// instead.
inline const char* INK_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;   // pixels
layout(location = 1) in vec4 aTone;  // kind, density, cell (pixels), angle
layout(location = 2) in vec4 aOrig;  // origin x y (pixels), clip panel (-1: none), -
layout(location = 3) in vec4 aCol;   // straight alpha
uniform vec2 uRes;
out vec2 vPix;
out float vDens;
out vec4 vCol;
flat out vec4 vTone;
flat out vec4 vOrig;
void main() {
    vPix = aPos;
    vDens = aTone.y;
    vCol = aCol;
    vTone = aTone;
    vOrig = aOrig;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* INK_FS = R"(#version 330 core
in vec2 vPix;
in float vDens;
in vec4 vCol;
flat in vec4 vTone;
flat in vec4 vOrig;
out vec4 frag;
uniform vec3 uClip[76];   // 19 panels x 4 edges (the page's, then the drop's): inside where dot(e.xy, p) + e.z >= 0
uniform float uScale;     // pixels per 1080p pixel
uniform float uPrint;     // 1: washes printed in CMYK halftone
uniform vec3 uPaper;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float dotTone(vec2 q, float d, float cell) {
    if (d <= 0.001) return 0.0;
    if (d >= 0.999) return 1.0;
    if (d <= 0.5) {
        vec2 f = (fract(q / cell) - 0.5) * cell;
        float r = sqrt(d / 3.14159) * cell;
        return clamp(r - length(f) + 0.5, 0.0, 1.0);
    }
    vec2 f = (fract(q / cell + 0.5) - 0.5) * cell;
    float r = sqrt((1.0 - d) / 3.14159) * cell;
    return 1.0 - clamp(r - length(f) + 0.5, 0.0, 1.0);
}
float lineTone(vec2 q, float d, float cell) {
    if (d <= 0.001) return 0.0;
    float y = (fract(q.y / cell) - 0.5) * cell;
    float t = 0.5 * d * cell;
    // a line thinner than a pixel keeps its weight as a lighter line
    if (t < 0.5) return clamp(1.0 - abs(y), 0.0, 1.0) * 2.0 * t;
    return clamp(t - abs(y) + 0.5, 0.0, 1.0);
}
mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }

void main() {
    int clipId = int(vOrig.z + 0.5);
    if (vOrig.z > -0.5) {
        for (int i = 0; i < 4; i++) {
            vec3 e = uClip[clipId * 4 + i];
            if (dot(e.xy, vPix) + e.z < 0.0) discard;
        }
    }
    int kind = int(vTone.x + 0.5);
    float cell = max(vTone.z, 1.0);
    vec2 q = rot(vTone.w) * (vPix - vOrig.xy);
    float d = clamp(vDens, 0.0, 1.0);
    float cov = 1.0;
    if (kind == 1) cov = dotTone(q, d, cell);
    else if (kind == 2) cov = lineTone(q, d, cell);
    else if (kind == 3) cov = max(lineTone(q, d, cell), lineTone(rot(1.5708) * q, d, cell));
    else if (kind == 4) cov = clamp(cell - length(vPix - vOrig.xy) + 0.5, 0.0, 1.0) * d;
    else if (kind == 5) {
        vec2 id = floor(q / cell);
        vec2 off = vec2(hash(id), hash(id + 7.31)) - 0.5;
        vec2 f = (fract(q / cell) - 0.5 - off * 0.6) * cell;
        float r = sqrt(d / 3.14159) * cell * (0.6 + 0.8 * hash(id + 3.7));
        cov = hash(id + 1.9) < 0.75 ? clamp(r - length(f) + 0.5, 0.0, 1.0) : 0.0;
    }
    vec3 rgb = vCol.rgb;
    if (kind == 6) {
        vec2 w = (vPix - vOrig.xy) / uScale;
        float n = vnoise(w / 90.0) * 0.6 + vnoise(w / 27.0) * 0.4;
        cov = clamp((d * 1.3 - n) / 0.3, 0.0, 1.0);
        // the wet edge while it floods, a slow mottle and a fine grain once it has
        rgb *= 1.0 - 0.22 * cov * (1.0 - cov) * 4.0 * step(d, 0.999);
        rgb *= 1.0 + 0.09 * (vnoise(w / 48.0 + 5.3) - 0.5) + 0.05 * (hash(floor(w / 1.5)) - 0.5);
        if (uPrint > 0.5) {
            // under color removal, then four screens at their angles
            vec3 cmy = clamp(1.0 - rgb / max(uPaper, vec3(0.05)), 0.0, 1.0);
            float k = min(cmy.r, min(cmy.g, cmy.b)) * 0.8;
            cmy = (cmy - k) / max(1.0 - k, 0.05);
            float cell = 4.4 * uScale;
            vec2 q2 = vPix - vOrig.xy;
            float cc = dotTone(rot(0.2618) * q2, cmy.r, cell);
            float cm = dotTone(rot(1.3090) * q2, cmy.g, cell);
            float cy = dotTone(q2, cmy.b, cell);
            float ck = dotTone(rot(0.7854) * q2, k, cell);
            vec3 dotsRgb = uPaper * mix(vec3(1.0), vec3(0.05, 0.62, 0.86), cc) * mix(vec3(1.0), vec3(0.9, 0.16, 0.5), cm)
                * mix(vec3(1.0), vec3(1.0, 0.9, 0.12), cy) * mix(vec3(1.0), vec3(0.16, 0.14, 0.15), ck);
            // half of it flat (the ink's spread on soft paper), so the screens texture the color without breaking it up
            rgb = mix(rgb, dotsRgb, 0.42);
        }
    }
    float a = vCol.a * cov;
    frag = vec4(rgb * a, a);
}
)";

// ---------------------------------------------------------------- lettering
// SDF type with an outline and a halo around it (sound effects: a white letter, a black line, a white margin).
inline const char* TEXT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;   // pixels
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
uniform vec4 uFill, uLine, uHalo;  // straight alpha
uniform float uLineW, uHaloW;      // in SDF units, outward from the letter's edge
uniform float uAlpha;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-4) * 0.7;
    float cf = smoothstep(0.5 - w, 0.5 + w, d);
    float e1 = 0.5 - uLineW, e2 = e1 - uHaloW;
    float cl = uLineW > 0.0 ? smoothstep(e1 - w, e1 + w, d) : 0.0;
    float ch = uHaloW > 0.0 ? smoothstep(e2 - w, e2 + w, d) : 0.0;
    vec4 c = vec4(uHalo.rgb, 1.0) * uHalo.a * ch;
    vec4 l = vec4(uLine.rgb, 1.0) * uLine.a * cl;
    c = c * (1.0 - l.a) + l;
    vec4 f = vec4(uFill.rgb, 1.0) * uFill.a * cf;
    c = c * (1.0 - f.a) + f;
    frag = c * uAlpha;
}
)";

} // namespace manga_shaders

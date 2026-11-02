#pragma once
// SWISS POSTER shaders: flat, crisp shapes (antialiased rectangles, discs and ring arcs), SDF type, and the
// composite that knocks the poster back behind the well and the HUD.

namespace swiss_shaders {

// Instanced shapes in pixels (top-left origin): a rectangle, a disc or a ring arc, clipped to a rectangle.
inline const char* SHAPE_VS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;   // -1..1
layout(location = 1) in vec4 aRect;     // x0, y0, x1, y1 (pixels)
layout(location = 2) in vec4 aCol;      // sRGB, straight alpha
layout(location = 3) in vec4 aKind;     // kind (0 rect, 1 ring arc, 2 disc), p1, p2, p3
layout(location = 4) in vec4 aClip;     // x0, y0, x1, y1 (pixels)
uniform vec2 uRes;
out vec2 vPix;
flat out vec4 vRect, vCol, vKind, vClip;
void main() {
    vec2 lo = aRect.xy - 1.0, hi = aRect.zw + 1.0;
    vec2 p = mix(lo, hi, aCorner * 0.5 + 0.5);
    vPix = p;
    vRect = aRect; vCol = aCol; vKind = aKind; vClip = aClip;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
inline const char* SHAPE_FS = R"(#version 330 core
in vec2 vPix;
flat in vec4 vRect, vCol, vKind, vClip;
out vec4 frag;
float span(float p, float a, float b) { return clamp(min(p - a, b - p) + 0.5, 0.0, 1.0); }
void main() {
    float cov;
    if (vKind.x < 0.5) {
        cov = span(vPix.x, vRect.x, vRect.z) * span(vPix.y, vRect.y, vRect.w);
    } else {
        vec2 c = 0.5 * (vRect.xy + vRect.zw);
        vec2 d = vPix - c;
        float r = length(d);
        if (vKind.x < 1.5) {
            cov = span(r, vKind.y, vKind.z);
            float a = atan(d.x, -d.y);                 // 0 at the top, clockwise
            if (a < 0.0) a += 6.2831853;
            float edge = vKind.w * 6.2831853;
            cov *= clamp((edge - a) * r + 0.5, 0.0, 1.0);
        } else {
            cov = clamp(vKind.y - r + 0.5, 0.0, 1.0);
        }
    }
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

// The poster under the game: knocked back toward the ground behind the well and the HUD (a union of rectangles,
// each with its feather and strength), then dimmed and faded.
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uPoster;
uniform vec2 uRes;
uniform vec3 uGround;
uniform int uCount;
uniform vec4 uRects[24];    // pixels, top-left origin
uniform vec2 uSoft[24];     // feather (pixels), strength 0..1
void main() {
    vec3 c = texture(uPoster, vUv).rgb;
    vec2 p = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
    float k = 0.0;
    for (int i = 0; i < 24; i++) {
        if (i >= uCount) break;
        vec4 r = uRects[i];
        vec2 q = max(max(r.xy - p, p - r.zw), vec2(0.0));
        float d = length(q);
        float m = 1.0 - smoothstep(0.0, max(uSoft[i].x, 1.0), d);
        k = max(k, m * uSoft[i].y);
    }
    frag = vec4(mix(c, uGround, k), 1.0);
}
)";

} // namespace swiss_shaders

#pragma once
// SUSHI's shaders (see sushi.cpp).
//
// BACK_FS paints the shop in one pass (the wall, the conveyor belt, the counter, the lacquered bento box the well
// is); SPRITE_FS draws the small things (petals, lanterns and their glow, the noren, the dishes on the belt,
// hearts, sparkles); CAT_FS the cats (one quad each, all signed distances); MAKI_FS the maki, each a little roll on its own (the
// stack, the falling piece, the ghost, the previews, the maki in the air). FINAL_FS encodes the frame.
#include <string>

namespace sushi_shaders {

inline const char* COMMON = R"(
float hash12(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float sdBox(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
// An ellipse (approximate distance, good near the outline).
float sdEll(vec2 p, vec2 r) {
    float k0 = length(p / r), k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / max(k1, 1e-5);
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
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}
vec2 rot2(vec2 p, float a) { float c = cos(a), s = sin(a); return vec2(c * p.x - s * p.y, s * p.x + c * p.y); }
float fillA(float d, float aa) { return 1.0 - smoothstep(-aa, aa, d); }
)";

// ---------------------------------------------------------------- the shop (full screen)
inline const char* BACK_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;
uniform vec3 uWallTop, uWallBot, uWood, uWoodDark, uBelt, uLacq, uRim, uWarmCol;
uniform float uBeltOff, uWarm, uBeltTop, uBeltBot, uBeltX0;
uniform vec4 uBox;   // the bento box: half extents of its inside (x, y), its center y, the rim's width
void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw;
    // the wall: a soft vertical gradient, a little light behind the box
    float t = smoothstep(uView.y - uView.w, uView.y + uView.w, w.y);
    vec3 c = mix(uWallBot, uWallTop, t);
    c *= 1.0 + 0.05 * exp(-dot(w * vec2(0.055, 0.05), w * vec2(0.055, 0.05)));
    c = mix(c, c * uWarmCol, uWarm);
    // a soft shade on the wall just above the counter
    c *= 1.0 - 0.07 * smoothstep(uBeltTop + 2.2, uBeltTop, w.y);
    // the conveyor belt: a band of plates moving along, the rail on top
    float bh = 0.5 * (uBeltTop - uBeltBot), bm = 0.5 * (uBeltTop + uBeltBot);
    float beltD = w.x > uBeltX0 + bh ? abs(w.y - bm) - bh : length(w - vec2(uBeltX0 + bh, bm)) - bh;
    if (beltD < 0.0) {
        vec3 b = uBelt;
        float u = (w.x - uBeltOff) / 1.15;
        float dEdge = (0.5 - abs(fract(u) - 0.5)) * 1.15;
        b *= 1.0 - 0.09 * (1.0 - smoothstep(0.015, 0.045, dEdge));
        float vy = (w.y - uBeltBot) / (uBeltTop - uBeltBot);
        b *= 0.9 + 0.12 * vy;
        b = mix(b, vec3(1.0), 0.35 * (1.0 - smoothstep(0.0, 0.07, uBeltTop - w.y)));
        b *= 1.0 - 0.25 * (1.0 - smoothstep(0.0, 0.06, w.y - uBeltBot));
        b = mix(b, b * 0.7, 1.0 - smoothstep(0.0, 0.04, -beltD));
        c = mix(c, b, fillA(beltD, 0.01));
    }
    // the counter's front: soft wood
    if (w.y < uBeltBot) {
        float g = vnoise(vec2(w.x * 0.12, w.y * 5.0)) * 0.6 + vnoise(vec2(w.x * 0.5, w.y * 11.0)) * 0.4;
        vec3 wd = mix(uWood, uWoodDark, 0.25 * g);
        wd *= 1.0 - 0.10 * smoothstep(uBeltBot, uView.y - uView.w, w.y) * 0.0;
        wd = mix(wd, uWood * 1.12, 0.6 * (1.0 - smoothstep(0.0, 0.08, uBeltBot - w.y)));
        wd *= 1.0 - 0.18 * (1.0 - smoothstep(0.08, 0.2, uBeltBot - w.y)) * smoothstep(0.0, 0.08, uBeltBot - w.y);
        c = wd;
    }
    // the bento box: its shadow on the wall, its rim, its dark lacquered inside
    vec2 bp = w - vec2(0.0, uBox.z);
    float dIn = sdBox(bp, uBox.xy - 0.12) - 0.12;
    float dOut = sdBox(bp, uBox.xy + uBox.w - 0.4) - 0.4;
    float sh = sdBox(bp - vec2(0.18, -0.22), uBox.xy + uBox.w - 0.4) - 0.4;
    c *= 1.0 - 0.22 * (1.0 - smoothstep(-0.2, 0.6, sh));
    float aa = 2.0 * uView.w / 1080.0;
    if (dOut < aa) {
        vec3 rim = uRim * (0.92 + 0.12 * smoothstep(-uBox.w, 0.0, -dOut + 0.0));
        rim = mix(rim, rim * 1.25 + 0.05, 0.5 * (1.0 - smoothstep(0.0, 0.07, abs(dOut + uBox.w * 0.45))) * 0.4);
        rim *= 1.0 - 0.35 * (1.0 - smoothstep(0.0, 0.05, -dOut));
        vec3 inside = uLacq * (1.0 + 0.12 * smoothstep(-10.0, 10.0, bp.y));
        inside *= 1.0 - 0.28 * (1.0 - smoothstep(0.0, 0.45, -dIn));
        vec3 bc = mix(rim, inside, fillA(dIn, aa));
        c = mix(c, bc, fillA(dOut, aa));
    }
    frag = vec4(c, 0.0);
}
)";

// ---------------------------------------------------------------- sprites (instanced quads)
inline const char* SPRITE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half width, half height
layout(location = 2) in vec4 aB; // kind, rotation, alpha, param
layout(location = 3) in vec4 aC; // color, param
layout(location = 4) in vec4 aD; // color 2, param
uniform mat4 uVP;
out vec2 vL;
out vec2 vW;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    vec2 l = aPos * aA.zw;
    vec2 wp = aA.xy + rot2(l, aB.y);
    vL = l; vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

inline const char* SPRITE_FS = R"(#version 330 core
in vec2 vL;
in vec2 vW;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec3 uLine;
uniform vec4 uWell; // the box's outer half extents and center (x, y, cx, cy): petals pass behind it
uniform vec4 uHudL, uHudR, uTitle, uCorner; // the HUD's areas (x0, y0, x1, y1): petals fade out there
float inRect(vec2 w, vec4 r) {
    float d = sdBox(w - 0.5 * (r.xy + r.zw), 0.5 * (r.zw - r.xy));
    return 1.0 - smoothstep(-0.3, 0.3, d);
}
void main() {
    int kind = int(vB.x + 0.5);
    float alpha = vB.z;
    vec2 h = vA.zw;
    vec2 q = vL;
    float aa = max(length(fwidth(q)) * 0.75, 1e-4);
    vec3 col = vC.rgb;
    float a = 0.0;
    if (kind == 0) {
        // a sakura petal: a rounded drop with a notch at its tip, paler at its base
        if (abs(vW.x - uWell.z) < uWell.x && abs(vW.y - uWell.w) < uWell.y) discard;
        vec2 p = q / h.y;
        float d = sdEll(p - vec2(0.0, 0.05), vec2(0.62, 0.92));
        d = max(d, -(length(p - vec2(0.0, 1.0)) - 0.24));
        aa = max(length(fwidth(p)) * 0.75, 1e-4);
        a = fillA(d, aa) * (1.0 - max(max(inRect(vW, uHudL), inRect(vW, uHudR)), max(inRect(vW, uTitle), inRect(vW, uCorner))));
        col = mix(vec3(1.0, 0.97, 0.97), col, smoothstep(-0.9, 0.5, p.y));
        col = mix(col, col * 0.86, 1.0 - smoothstep(0.0, 0.12, -d));
    } else if (kind == 1) {
        // a paper lantern: a round belly with ribs, dark caps, lit from inside
        float lit = vB.w;
        vec2 p = q;
        float body = sdEll(p, vec2(h.x * 0.96, h.y * 0.78));
        float capT = sdBox(p - vec2(0.0, h.y * 0.82), vec2(h.x * 0.42, h.y * 0.12)) - 0.03;
        float capB = sdBox(p - vec2(0.0, -h.y * 0.82), vec2(h.x * 0.42, h.y * 0.12)) - 0.03;
        float caps = min(capT, capB);
        float sil = min(body, caps);
        a = fillA(sil, aa);
        float rr = length(p / vec2(h.x, h.y * 0.8));
        vec3 paper = vC.rgb;
        vec3 glow = mix(paper, vec3(1.0, 0.8, 0.42), 0.6) * 1.4;
        vec3 pc = mix(paper * 0.9, glow, lit * (1.0 - 0.45 * rr * rr));
        float rib = abs(fract(p.y / (h.y * 0.26) + 0.5) - 0.5) * h.y * 0.26;
        pc *= 1.0 - 0.14 * (1.0 - smoothstep(0.0, 0.035, rib)) * (1.0 - 0.5 * lit);
        // a band of the second color around the middle
        pc = mix(pc, mix(vD.rgb, vD.rgb * 1.3 + 0.1, lit), (1.0 - smoothstep(0.0, aa * 1.5, abs(p.y) - h.y * 0.12)) * vD.w);
        pc = mix(pc, uLine, 1.0 - smoothstep(0.0, 0.06, -body));
        vec3 cc = uLine * 1.2;
        col = mix(pc, cc, fillA(caps, aa));
        col = mix(col, uLine, (1.0 - smoothstep(0.0, 0.05, -caps)) * fillA(caps, aa));
    } else if (kind == 2) {
        // a soft glow (added: premultiplied with no alpha)
        float r = length(q / h);
        float g = exp(-3.2 * r * r) * (1.0 - smoothstep(0.8, 1.0, r));
        frag = vec4(col * g * alpha, 0.0);
        return;
    } else if (kind == 3) {
        // a cord or a rod (a rounded bar)
        float d = sdBox(q, h - h.y) - h.y;
        a = fillA(d, aa);
    } else if (kind == 4) {
        // a noren panel: dyed cloth hanging from its rod, a darker band at the top, softly folded
        float d = sdBox(q, h - 0.12) - 0.12;
        d = max(d, q.y - h.y);
        a = fillA(d, aa);
        vec3 cl = col * (0.94 + 0.08 * sin(q.x * 2.4 + 0.6));
        cl = mix(cl, cl * 0.78, 1.0 - smoothstep(0.0, aa, (h.y - 0.32) - q.y));
        cl *= 1.0 - 0.12 * smoothstep(0.0, -h.y, q.y);
        col = mix(cl, uLine, 1.0 - smoothstep(0.0, 0.05, -d));
    } else if (kind == 5) {
        // a heart
        vec2 p = q / h.y * 0.62;
        p.y += 0.52;
        p.x = abs(p.x);
        float d;
        if (p.y + p.x > 1.0) d = length(p - vec2(0.25, 0.75)) - 0.35355;
        else d = sqrt(min(dot(p - vec2(0.0, 1.0), p - vec2(0.0, 1.0)), dot(p - 0.5 * max(p.x + p.y, 0.0), p - 0.5 * max(p.x + p.y, 0.0)))) * sign(p.x - p.y);
        float ab = max(length(fwidth(p)) * 0.75, 1e-4);
        a = fillA(d, ab);
        col = mix(col, uLine, 1.0 - smoothstep(0.0, 0.07, -d));
        col = mix(col, vec3(1.0), 0.8 * fillA(length(p - vec2(0.22, 0.78)) - 0.07, ab) * step(0.0, vL.x));
    } else if (kind == 6) {
        // a sparkle: a four-pointed star
        vec2 p = abs(q / h);
        float d = pow(p.x, 0.5) + pow(p.y, 0.5) - 0.9;
        float ab = max(length(fwidth(q / h)) * 1.2, 1e-3);
        a = fillA(d, ab);
        col = mix(col, vec3(1.0), 0.6 * fillA(length(q / h) - 0.18, ab));
    } else if (kind == 7) {
        // a dish on the belt with a piece of nigiri on it
        if (abs(vW.x - uWell.z) < uWell.x && abs(vW.y - uWell.w) < uWell.y) discard;
        vec2 p = q;
        float dish = sdEll(p - vec2(0.0, -h.y * 0.45), vec2(h.x, h.y * 0.42));
        float foot = sdBox(p - vec2(0.0, -h.y * 0.82), vec2(h.x * 0.45, h.y * 0.12)) - 0.03;
        float rice = sdEll(p - vec2(0.0, -h.y * 0.08), vec2(h.x * 0.58, h.y * 0.34));
        float top = sdEll(p - vec2(0.0, h.y * 0.18), vec2(h.x * 0.66, h.y * 0.3));
        float sil = min(min(dish, foot), min(rice, top));
        a = fillA(sil, aa);
        vec3 dc = col;
        dc = mix(dc, dc * 0.75, 1.0 - smoothstep(0.0, 0.05, -dish));
        vec3 cc = mix(dc * 0.8, dc, fillA(dish, aa));
        cc = mix(cc, mix(vec3(0.97, 0.95, 0.9), uLine, 1.0 - smoothstep(0.0, 0.045, -rice)), fillA(rice, aa));
        vec3 tc = vD.rgb * (0.95 + 0.1 * smoothstep(-h.y * 0.1, h.y * 0.45, p.y));
        float stripe = abs(fract((p.x + p.y * 0.6) * 3.2) - 0.5);
        tc = mix(tc, mix(tc, vec3(1.0), 0.55), vD.w * (1.0 - smoothstep(0.06, 0.12, stripe)));
        tc = mix(tc, uLine, 1.0 - smoothstep(0.0, 0.045, -top));
        col = mix(cc, tc, fillA(top, aa));
        col = mix(col, uLine, (1.0 - smoothstep(0.0, 0.045, -min(dish, foot))) * (1.0 - fillA(min(rice, top), aa)));
    } else if (kind == 9) {
        // a cord sagging between two points (vD: x0, x1, y, sag)
        float u = clamp((vW.x - vD.x) / (vD.y - vD.x), 0.0, 1.0);
        float yc = vD.z - vD.w * 4.0 * u * (1.0 - u);
        float slope = -vD.w * 4.0 * (1.0 - 2.0 * u) / (vD.y - vD.x);
        float d = abs(vW.y - yc) / sqrt(1.0 + slope * slope) - vB.w;
        float ab = max(fwidth(vW.y) * 0.75, 1e-4);
        a = fillA(d, ab);
    } else if (kind == 8) {
        // a disc (a bell, an emblem)
        float d = length(q) - h.x;
        a = fillA(d, aa);
        col = mix(col, uLine, 1.0 - smoothstep(0.0, 0.05, -d));
    }
    a *= alpha;
    if (a <= 0.002) discard;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- the cats
inline const char* CAT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 i0; // x, y (the counter), scale, facing
layout(location = 2) in vec4 i1; // coat color, coat kind
layout(location = 3) in vec4 i2; // patch color, -
layout(location = 4) in vec4 i3; // patch 2, eyes open
layout(location = 5) in vec4 i4; // collar, eyes' closed style (0 asleep, 1 happy)
layout(location = 6) in vec4 i5; // mouth open, chewing, blush, alpha
layout(location = 7) in vec4 i6; // left arm raised, right arm raised, tail, sway
layout(location = 8) in vec4 i7; // head tilt, squash, lift, look
uniform mat4 uVP;
out vec2 vP;
out vec2 vW;
flat out vec4 v0, v1, v2, v3, v4, v5, v6, v7;
void main() {
    vec2 l = vec2(mix(-2.0, 2.0, aPos.x * 0.5 + 0.5), mix(-0.4, 3.7, aPos.y * 0.5 + 0.5));
    vec2 wp = i0.xy + vec2(l.x * i0.w, l.y + i7.z) * i0.z;
    vP = l; vW = wp;
    v0 = i0; v1 = i1; v2 = i2; v3 = i3; v4 = i4; v5 = i5; v6 = i6; v7 = i7;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

inline const char* CAT_FS = R"(#version 330 core
in vec2 vP;
in vec2 vW;
flat in vec4 v0, v1, v2, v3, v4, v5, v6, v7;
out vec4 frag;
uniform vec3 uLine, uBlush, uEye, uPink, uMouth;
uniform float uClipY;
const float LW = 0.085;
vec3 C;
float A;
float AA;
// A shape with its own soft outline (inside its edge).
void layer(float d, vec3 col) {
    float f = fillA(d, AA);
    float inner = fillA(d + LW, AA);
    C = mix(C, mix(uLine, col, inner), f);
    A = max(A, f);
}
void paint(float d, vec3 col, float k) { C = mix(C, col, fillA(d, AA) * k); }
void line(float d, float w, vec3 col) { C = mix(C, col, fillA(abs(d) - w, AA)); }
void main() {
    if (vW.y < uClipY) discard;
    int coat = int(v1.w + 0.5);
    vec3 base = v1.rgb, patch = v2.rgb, patch2 = v3.rgb, collar = v4.rgb;
    vec2 p = vP;
    AA = max(length(fwidth(p)) * 0.75, 1e-4);
    C = vec3(0.0); A = 0.0;
    float sway = v6.w, tilt = v7.x;
    vec2 pb = rot2(p, -sway);
    pb.y /= v7.y;
    vec2 hc = vec2(0.0, 1.98);
    vec2 ph = rot2(pb - hc, -tilt);
    bool socks = coat != 5;
    vec3 pawCol = coat == 5 ? patch : (coat == 2 ? vec3(0.98, 0.96, 0.95) : (coat == 0 ? base : vec3(0.99, 0.97, 0.95)));

    // ---- the tail: a curl rising beside the body (a chain of capsules)
    {
        float tw = v6.z;
        vec2 p0 = vec2(0.66, 0.32), p1 = vec2(1.55, 0.22 + 0.1 * tw), p2 = vec2(1.32 + 0.32 * tw, 1.55 - 0.08 * abs(tw));
        float d = 1e5;
        vec2 prev = p0;
        for (int i = 1; i <= 7; i++) {
            float t = float(i) / 7.0;
            vec2 b = mix(mix(p0, p1, t), mix(p1, p2, t), t);
            d = min(d, sdSeg(pb, prev, b) - mix(0.15, 0.115, t));
            prev = b;
        }
        vec3 tc = base;
        if (coat == 1) tc = patch;
        if (coat == 5) tc = patch;
        if (coat == 3 || coat == 4) {
            float ring = abs(fract(length(pb - p0) * 2.6) - 0.5);
            tc = mix(base, patch2, 1.0 - smoothstep(0.12, 0.16, ring));
        }
        layer(d, tc);
    }
    // ---- the body
    {
        vec2 q = pb - vec2(0.0, 0.8);
        float d = sdEll(q, vec2(0.92, 0.95));
        vec3 bc = base;
        if (coat == 1) {
            bc = mix(bc, patch2, fillA(length(q - vec2(0.48, 0.25)) - 0.42, AA));
            bc = mix(bc, patch, fillA(length(q - vec2(-0.55, -0.35)) - 0.38, AA));
        }
        if (coat == 3 || coat == 4) bc = mix(bc, patch, fillA(sdEll(q - vec2(0.0, -0.2), vec2(0.46, 0.62)), AA));
        if (coat == 2) bc = mix(bc, vec3(0.98, 0.96, 0.95), fillA(sdEll(q - vec2(0.0, 0.15), vec2(0.22, 0.3)), AA) * 0.0);
        bc *= 0.9 + 0.1 * smoothstep(-0.9, 0.6, q.y);
        layer(d, bc);
    }
    // ---- the collar and its bell
    {
        vec2 q = pb - vec2(0.0, 1.1);
        float d = sdBox(q, vec2(0.52, 0.04)) - 0.07;
        layer(d, collar);
        float b = length(pb - vec2(0.0, 0.94)) - 0.13;
        layer(b, vec3(1.0, 0.82, 0.32));
        line(sdSeg(pb, vec2(-0.075, 0.9), vec2(0.075, 0.9)), 0.013, uLine);
        paint(length(pb - vec2(-0.045, 0.98)) - 0.032, vec3(1.0, 0.97, 0.85), 1.0);
    }
    // ---- the front legs: resting on the counter, or raised (a cheer, a wave)
    for (int s = 0; s < 2; s++) {
        float sg = s == 0 ? -1.0 : 1.0;
        float up = s == 0 ? v6.x : v6.y;
        vec2 sh = vec2(0.4 * sg, 0.72);
        vec2 rest = vec2(0.34 * sg, 0.2);
        vec2 raised = vec2(1.08 * sg, 2.4) + vec2(0.0, 0.0);
        vec2 paw = mix(rest, raised, up);
        float d = sdSeg(pb, sh, paw) - 0.19;
        d = min(d, sdEll(pb - paw, vec2(0.22, 0.19)));
        if (up < 0.02) d = max(d, -(pb.y - 0.0) - 0.0);
        layer(d, mix(base, pawCol, smoothstep(0.75, 0.3, length(pb - paw))));
        // toes
        vec2 tq = pb - paw;
        if (up < 0.5) {
            line(sdSeg(tq, vec2(-0.06, -0.17), vec2(-0.06, -0.07)), 0.014, uLine);
            line(sdSeg(tq, vec2(0.06, -0.17), vec2(0.06, -0.07)), 0.014, uLine);
        } else {
            paint(length(tq - vec2(0.0, -0.02)) - 0.07, uPink, 0.9);
        }
    }
    // ---- the head with its ears
    {
        vec2 q = ph;
        float head = sdEll(q, vec2(0.95, 0.78));
        vec2 l0 = vec2(-0.82, 0.3), l1 = vec2(-0.66, 1.02), l2 = vec2(-0.16, 0.66);
        float earL = sdTri(q, l0, l1, l2) - 0.07;
        float earR = sdTri(q, vec2(-l0.x, l0.y), vec2(-l1.x, l1.y), vec2(-l2.x, l2.y)) - 0.07;
        float d = smin(head, min(earL, earR), 0.08);
        vec3 hcol = base;
        if (coat == 1) {
            hcol = mix(hcol, patch, fillA(length(q - vec2(0.5, 0.42)) - 0.45, AA));
            hcol = mix(hcol, patch2, fillA(length(q - vec2(-0.62, 0.62)) - 0.34, AA));
        }
        if (coat == 3 || coat == 4) {
            hcol = mix(hcol, patch, fillA(sdEll(q - vec2(0.0, -0.3), vec2(0.42, 0.3)), AA));
            for (int i = -1; i <= 1; i++) {
                float x = float(i) * 0.17;
                hcol = mix(hcol, patch2, fillA(sdSeg(q, vec2(x, 0.5 - 0.04 * abs(float(i))), vec2(x * 1.1, 0.7)) - 0.045, AA));
            }
        }
        if (coat == 5) {
            float m = 1.0 - smoothstep(-0.04, 0.1, sdEll(q - vec2(0.0, -0.2), vec2(0.42, 0.3)));
            hcol = mix(hcol, patch, 0.85 * m);
            hcol = mix(hcol, patch, fillA(min(earL, earR) + 0.02, AA));
        }
        if (coat == 0) hcol = mix(hcol, patch, fillA(length(q - vec2(0.62, 0.62)) - 0.32, AA));
        hcol *= 0.94 + 0.06 * smoothstep(-0.8, 0.6, q.y);
        layer(d, hcol);
        // inner ears
        vec2 c0 = (l0 + l1 + l2) / 3.0;
        float inL = sdTri(q, mix(l0, c0, 0.45), mix(l1, c0, 0.4), mix(l2, c0, 0.45)) - 0.03;
        float inR = sdTri(q, vec2(-mix(l0, c0, 0.45).x, mix(l0, c0, 0.45).y), vec2(-mix(l1, c0, 0.4).x, mix(l1, c0, 0.4).y),
                          vec2(-mix(l2, c0, 0.45).x, mix(l2, c0, 0.45).y)) - 0.03;
        paint(min(inL, inR), coat == 2 ? mix(uPink, base, 0.45) : uPink, 0.9);

        // blush
        float blush = v5.z;
        paint(sdEll(vec2(abs(q.x), q.y) - vec2(0.58, -0.2), vec2(0.15, 0.085)), uBlush, 0.65 * blush);
        // eyes
        float open = v3.w;
        float look = v7.w;
        for (int s = 0; s < 2; s++) {
            float sg = s == 0 ? -1.0 : 1.0;
            vec2 e = q - vec2(0.34 * sg + 0.05 * look, 0.02);
            if (open > 0.18) {
                float o = open;
                if (coat == 2) {
                    paint(sdEll(e, vec2(0.135, 0.165 * o)), vec3(0.98, 0.78, 0.3), 1.0);
                    paint(sdEll(e, vec2(0.07, 0.13 * o)), uEye, 1.0);
                } else {
                    paint(sdEll(e, vec2(0.12, 0.155 * o)), uEye, 1.0);
                }
                paint(length(e - vec2(-0.035, 0.055 * o)) - 0.048, vec3(1.0), 1.0);
                paint(length(e - vec2(0.045, -0.06 * o)) - 0.022, vec3(1.0), 0.9);
            } else {
                vec3 lc = coat == 2 ? vec3(0.98, 0.9, 0.8) : uEye;
                if (v4.w > 0.5) {
                    // happy: ^ ^
                    float d2 = abs(length(e - vec2(0.0, -0.1)) - 0.13);
                    d2 = e.y < -0.03 ? 1e3 : d2;
                    line(d2, 0.026, lc);
                } else {
                    // asleep: a soft curve
                    float d2 = abs(length(e - vec2(0.0, 0.1)) - 0.13);
                    d2 = e.y > 0.02 ? 1e3 : d2;
                    line(d2, 0.024, lc);
                }
            }
        }
        // nose
        paint(sdEll(q - vec2(0.0, -0.13), vec2(0.055, 0.038)), uPink * 0.9, 1.0);
        // mouth: a little w, or open (catching, cheering), chewing
        float mo = v5.x;
        vec3 mc = coat == 2 ? vec3(0.95, 0.88, 0.85) : uLine;
        if (mo > 0.06) {
            vec2 m = q - vec2(0.0, -0.31);
            float d2 = sdEll(m, vec2(0.11, 0.13 * mo));
            paint(d2, uMouth, 1.0);
            paint(sdEll(m - vec2(0.0, -0.08 * mo), vec2(0.07, 0.05 * mo)), uPink, 1.0);
            line(d2, 0.018, mc);
        } else {
            float ch = v5.y;
            float r = 0.065 * (1.0 + 0.25 * ch);
            for (int s = 0; s < 2; s++) {
                float sg = s == 0 ? -1.0 : 1.0;
                vec2 cc = vec2(0.065 * sg, -0.2 + 0.02 * ch);
                vec2 m = q - cc;
                float d2 = abs(length(m) - r);
                d2 = m.y > 0.0 ? 1e3 : d2;
                line(d2, 0.02, mc);
            }
        }
    }
    float a = A * v5.w;
    if (a <= 0.002) discard;
    frag = vec4(C * a, a);
}
)";

// ---------------------------------------------------------------- the maki
inline const char* MAKI_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // type, alpha, plate alpha, spin
layout(location = 3) in vec4 aC; // plate color, maki scale
layout(location = 4) in vec4 aD; // kind (0 plate and maki, 1 ghost), maki alpha, shadow, -
uniform mat4 uVP;
out vec2 vLocal;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
void main() {
    float ext = aA.z * 1.1;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vA = aA; vB = aB; vC = aC; vD = aD;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";

// The plate's outline: its own cells merge (mask bits: left right down up, down-left down-right up-left up-right),
// a thin gap parts it from the others.
inline const char* SHAPE_GLSL = R"(
bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m) {
    const float g = 0.045, r = 0.2, rc = 0.08;
    vec2 q = abs(p);
    bool hx = p.x < 0.0 ? has(m, 0) : has(m, 1);
    bool vy = p.y < 0.0 ? has(m, 2) : has(m, 3);
    int db = p.x < 0.0 ? (p.y < 0.0 ? 4 : 6) : (p.y < 0.0 ? 5 : 7);
    float c = 0.5 - g;
    if (!hx && !vy) { vec2 k = q - (c - r); return length(max(k, 0.0)) + min(max(k.x, k.y), 0.0) - r; }
    if (hx && !vy) return q.y - c;
    if (!hx && vy) return q.x - c;
    if (has(m, db)) return -1.0;
    return rc - length(max(vec2(c + rc) - q, 0.0));
}
bool ownCell(vec2 p, int m) {
    if (p.x < -0.5 && has(m, 0)) return false;
    if (p.x > 0.5 && has(m, 1)) return false;
    if (p.y < -0.5 && has(m, 2)) return false;
    if (p.y > 0.5 && has(m, 3)) return false;
    return true;
}
)";

inline const char* MAKI_FS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
out vec4 frag;
uniform vec3 uNori, uRice, uLine;
vec3 P;
float A;
float AA;
void over(vec3 col, float a) { P = col * a + P * (1.0 - a); A = a + A * (1.0 - a); }
// paint over what is there without adding coverage (inside the maki)
void tint(vec3 col, float a, float inside) {
    float k = a * inside;
    P = mix(P, col * A, k);
}
void main() {
    int kind = int(vD.x + 0.5);
    int m = int(vA.w + 0.5);
    vec2 p = vLocal;
    AA = max(length(fwidth(p)) * 0.75, 1e-4);
    P = vec3(0.0); A = 0.0;
    vec3 pc = vC.rgb;
    // the maki stands on its own, a short roll seen from a little above: the cut face on top (an ellipse), the
    // nori wrapped around its side below
    float sc = max(vC.w, 0.05);
    vec2 s = p / sc;
    float qa = AA / sc;
    const float R = 0.43, K = 0.7, YC = 0.1, H = 0.16;
    vec2 e = vec2(s.x, (s.y - YC) / K);
    float topD = (length(e) - R) * K;
    float botD = (length(vec2(s.x, (s.y - YC + H) / K)) - R) * K;
    float sideD = max(abs(s.x) - R, abs(s.y - YC + H * 0.5) - H * 0.5);
    float bodyD = min(min(topD, botD), sideD);
    if (kind == 1) {
        // the ghost: the roll's outline, faint
        float l = fillA(abs(bodyD + 0.02) - 0.02, qa);
        float a = l * 0.55 * vB.y;
        if (a <= 0.002) discard;
        frag = vec4(pc * a, a);
        return;
    }
    int type = int(vB.x + 0.5);
    float makiA = vD.y;
    // its soft shadow on the lacquer
    float shd = (length(vec2(s.x - 0.03, (s.y - YC + H + 0.05) / (K * 0.8))) - R * 1.02) * K;
    over(vec3(0.0), vD.z * 0.4 * (1.0 - smoothstep(-0.06, 0.05, shd)) * makiA);
    // the side: nori (rice and sesame for the inside-out roll), round shading across it
    float u = clamp(s.x / R, -1.0, 1.0);
    float roundS = sqrt(1.0 - u * u);
    vec3 side;
    if (type == 2) {
        side = uRice * (0.7 + 0.3 * roundS);
        // sesame seeds scattered on the rice
        vec2 sg = s * vec2(14.0, 18.0);
        vec2 sid = floor(sg), sf = fract(sg) - 0.5;
        vec2 so = vec2(fract(sin(dot(sid, vec2(12.9, 78.2))) * 43758.5), fract(sin(dot(sid, vec2(39.3, 11.1))) * 24634.6)) - 0.5;
        float seed = sdEll(rot2(sf - so * 0.5, so.x * 3.0), vec2(0.09, 0.16));
        side = mix(side, so.y > 0.2 ? vec3(0.88, 0.76, 0.5) : vec3(0.15, 0.12, 0.1), fillA(seed, 0.05) * 0.9);
    } else {
        side = uNori * (0.75 + 0.5 * roundS);
        side = mix(side, mix(uNori, vec3(1.0), 0.35), 0.5 * (1.0 - smoothstep(0.0, 0.12, abs(u + 0.45) - 0.05)));
        // the nori's fine ribs
        side *= 0.92 + 0.08 * smoothstep(0.3, 0.7, vnoise(vec2(s.x * 30.0, s.y * 4.0)));
    }
    over(side, fillA(bodyD, qa) * makiA);
    // the cut face
    vec2 q = rot2(e, vB.w);
    float r = length(q);
    float disc = fillA(topD, qa);
    vec3 c;
    if (type == 2) {
        // uramaki: rice outside, sesame on it, a thin nori ring, avocado and salmon inside
        c = uRice * (0.95 + 0.06 * smoothstep(-0.4, 0.4, q.y));
        for (int i = 0; i < 9; i++) {
            float an = float(i) * 2.39996 + 0.5;
            float rr = 0.27 + 0.07 * fract(float(i) * 0.618);
            vec2 sp = rr * vec2(cos(an), sin(an));
            vec2 sq = rot2(q - sp, -an);
            float sd2 = sdEll(sq, vec2(0.018, 0.036));
            c = mix(c, (i % 3 == 0) ? vec3(0.9, 0.78, 0.5) : vec3(0.16, 0.13, 0.12), fillA(sd2, qa));
        }
        c = mix(c, uNori, fillA(abs(r - 0.2) - 0.022, qa));
        vec3 fill = q.x + q.y * 0.3 < 0.0 ? vec3(0.62, 0.8, 0.32) : vec3(1.0, 0.52, 0.3);
        fill = mix(fill, vec3(1.0, 0.85, 0.78), (q.x + q.y * 0.3 >= 0.0 ? 1.0 : 0.0) * (1.0 - smoothstep(0.0, 0.02, abs(fract((q.x - q.y) * 9.0) - 0.5) - 0.3)));
        c = mix(c, fill, fillA(r - 0.175, qa));
        c = mix(c, uLine, 1.0 - smoothstep(0.0, 0.04, R - r));
    } else {
        // hosomaki: the nori outside, white rice, the filling in the middle
        c = uNori * (1.0 + 0.6 * (1.0 - smoothstep(0.0, 0.5, length(q / R - vec2(-0.5, 0.55)))));
        vec3 rice = uRice * (0.94 + 0.08 * smoothstep(-0.35, 0.35, q.y - q.x * 0.4));
        float inR = R - 0.05;
        c = mix(c, rice, fillA(r - inR, qa));
        // a hint of grains along the rice's edge
        float gr = vnoise(vec2(atan(q.y, q.x) * 9.0, r * 18.0));
        c = mix(c, uRice * 0.86, fillA(r - inR, qa) * (1.0 - fillA(r - (inR - 0.04), qa)) * smoothstep(0.55, 0.75, gr));
        // grains all over the rice
        c = mix(c, uRice * 0.88, fillA(r - inR, qa) * 0.5 * smoothstep(0.6, 0.8, vnoise(rot2(q, 0.5) * vec2(34.0, 20.0))));
        // the filling, a bit larger than the plain disc gives it
        q /= 1.22; r = length(q); qa /= 1.22;
        vec3 f = c;
        float fa = 0.0;
        if (type == 0) {
            // kani: a crab stick, white with its red skin
            float d = r - 0.165;
            fa = fillA(d, qa);
            f = mix(vec3(1.0, 0.95, 0.93), vec3(0.95, 0.27, 0.22), smoothstep(-0.03, 0.03, q.y + 0.25 * q.x - 0.0));
            f = mix(f, vec3(1.0, 0.6, 0.5), (1.0 - smoothstep(0.0, 0.03, abs(q.y + 0.25 * q.x))) * 0.6);
        } else if (type == 1) {
            // tamago: layers of sweet omelet
            float d = sdBox(q, vec2(0.12)) - 0.045;
            fa = fillA(d, qa);
            f = vec3(1.0, 0.82, 0.25);
            f = mix(f, vec3(1.0, 0.92, 0.58), fillA(abs(abs(q.y) - 0.06) - 0.012, qa));
            f = mix(f, vec3(0.86, 0.6, 0.12), 1.0 - smoothstep(0.0, 0.03, -d));
        } else if (type == 3) {
            // kappa: cucumber, its dark skin and pale seeds
            float d = r - 0.16;
            fa = fillA(d, qa);
            f = mix(vec3(0.62, 0.84, 0.42), vec3(0.85, 0.95, 0.62), fillA(r - 0.075, qa));
            f = mix(f, vec3(0.2, 0.48, 0.2), 1.0 - smoothstep(0.0, 0.035, -d));
            for (int i = 0; i < 3; i++) {
                vec2 sp = 0.04 * vec2(cos(float(i) * 2.094 + 0.3), sin(float(i) * 2.094 + 0.3));
                f = mix(f, vec3(0.55, 0.75, 0.35), fillA(sdEll(q - sp, vec2(0.012, 0.02)), qa));
            }
        } else if (type == 4) {
            // tekka: deep red tuna
            float d = sdBox(rot2(q, 0.12), vec2(0.13)) - 0.04;
            fa = fillA(d, qa);
            f = vec3(0.82, 0.13, 0.24);
            f = mix(f, vec3(0.95, 0.4, 0.48), 0.6 * (1.0 - smoothstep(0.0, 0.035, abs(q.x - q.y * 0.9 + 0.05) - 0.012)));
            f = mix(f, vec3(0.55, 0.06, 0.15), 1.0 - smoothstep(0.0, 0.03, -d));
        } else if (type == 5) {
            // avocado: a soft green crescent
            float d = max(r - 0.175, -(length(q - vec2(0.1, 0.09)) - 0.12));
            fa = fillA(d, qa);
            f = mix(vec3(0.86, 0.9, 0.45), vec3(0.55, 0.76, 0.27), smoothstep(-0.12, 0.0, d) );
            f = mix(f, vec3(0.25, 0.42, 0.14), 1.0 - smoothstep(0.0, 0.03, -d));
        } else {
            // sake: salmon with its white lines of fat
            float d = sdBox(rot2(q, -0.1), vec2(0.13)) - 0.045;
            fa = fillA(d, qa);
            f = vec3(1.0, 0.52, 0.3);
            float st = abs(fract((q.x + q.y) * 7.5 + 0.25) - 0.5);
            f = mix(f, vec3(1.0, 0.86, 0.76), 1.0 - smoothstep(0.08, 0.13, st));
            f = mix(f, vec3(0.86, 0.33, 0.16), 1.0 - smoothstep(0.0, 0.03, -d));
        }
        c = mix(c, f, fa);
        q *= 1.22; r = length(q); qa *= 1.22;
    }
    // a little shine
    c = mix(c, vec3(1.0), 0.35 * fillA(sdEll(rot2(q - vec2(-0.2, 0.22), 0.7), vec2(0.07, 0.035)), qa) * (type == 2 ? 0.4 : 1.0));
    // its outline
    c = mix(c, uLine, 1.0 - smoothstep(0.0, 0.035 / sc, R - r + 0.0));
    over(c, disc * makiA);
    // the roll's soft outline
    over(uLine, fillA(abs(bodyD) - 0.014, qa) * makiA);
    float a = A * vB.y;
    if (a <= 0.002) discard;
    frag = vec4(P * vB.y, a);
}
)";

// ---------------------------------------------------------------- the frame
inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uSlow, uDim, uFade, uBridge;
uniform vec3 uBridgeCol, uBridgeInk, uDusk;
void main() {
    vec4 s = texture(uScene, vUv);
    vec3 c = s.rgb;
    // slow motion: dusk at the counter (the lanterns stay warm)
    float g = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 dusk = mix(vec3(g), c, 0.7) * uDusk;
    dusk += max(c - vec3(0.85), 0.0) * 0.8;
    c = mix(c, dusk, uSlow * (1.0 - 0.6 * clamp(s.a, 0.0, 1.0)));
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
layout(location = 0) in vec2 aPos;   // pixels, top-left origin
layout(location = 1) in vec2 aUv;
uniform vec2 uRes;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
// Rounded letters with a soft halo of the wall around them.
inline const char* TEXT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
uniform float uAlpha, uHaloA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.25, 0.5, d) * uHaloA;
    float a = max(fill, halo) * uAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

// Builds a fragment shader with the common functions inserted after the version line.
inline std::string withCommon(const char* src, bool shape = false) {
    std::string s(src);
    const size_t nl = s.find('\n');
    return s.substr(0, nl + 1) + COMMON + (shape ? SHAPE_GLSL : "") + s.substr(nl + 1);
}

} // namespace sushi_shaders

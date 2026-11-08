#pragma once
// MYCELIUM's shaders: the world at a reduced resolution with depth (the night sky, the far tree line, the forest
// floor and the soil's cut in one full-screen pass; the roots and the glowing threads on the cut; plants, trees and
// mushrooms as instanced shapes behind it; spores and halos in the air), the threads' light (a blurred quarter
// resolution pass that lights the soil, the stones and the roots), a soft bloom, the stars at full resolution, then
// the veil behind the well, the felt pieces and the lettering.
// The world is in meters (x right, y up, z into the screen, the cut along z = 0), seen through an off-axis
// perspective: the cut maps to the window, what stands behind converges toward the eye (worldClip).

namespace mycelium_shaders {

// Shared (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
uniform vec4 uCam;   // the window's center (m), half height (m), aspect
uniform vec3 uEyeP;  // the eye's x, y (m), its distance in front of the cut (m)
float sq(float x) { return x * x; }
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float hash1(float n) { return fract(sin(n * 127.1 + 0.37) * 43758.5453); }
vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = mat2(0.8, -0.6, 0.6, 0.8) * p * 2.03 + 1.7; a *= 0.5; }
    return s / 0.9375;
}
// The surface along the cut (the same waves as myc::surf).
float surf(float x) {
    float s = 0.0, s0 = 0.0, l = 0.01;
    for (int i = 0; i < 9; i++) {
        s += 0.012 * l * sin(x / l + float(i) * 1.7);
        s0 += 0.012 * l * sin(float(i) * 1.7);
        l *= 3.0;
    }
    return s - s0;
}
// World (meters) -> clip: the off-axis perspective (the cut's plane z = 0 maps to the window).
vec4 worldClip(vec3 p) {
    float s = uEyeP.z / (uEyeP.z + p.z);
    vec2 w = uEyeP.xy + (p.xy - uEyeP.xy) * s;
    vec2 ndc = (w - uCam.xy) / vec2(uCam.z * uCam.w, uCam.z);
    return vec4(ndc, p.z / (p.z + uEyeP.z) * 2.0 - 1.0, 1.0);
}
// A noise whose octaves follow the scale on screen (the same look at every zoom): its finest octave ~ px.
float lfbm(vec2 p, float px) {
    float l = log2(max(px, 1e-7));
    float fl = floor(l), fr = l - fl;
    float s = 0.0, w = 0.0;
    for (int i = 0; i < 6; i++) {
        float o = fl + float(i);
        float sc = exp2(o);
        float a = (i == 0 ? 1.0 - fr : 1.0) * (i == 5 ? fr : 1.0) * pow(1.35, float(i));
        s += a * vnoise(p / sc + vec2(o * 17.13, o * 5.71));
        w += a;
    }
    return s / max(w, 1e-4);
}
)";

// Mushroom lights for the floor, the plants and the trees (the caps lighting the canopy).
inline const char* LIGHTS_GLSL = R"(
uniform vec4 uLP[8];   // x, y, z, radius (m)
uniform vec3 uLC[8];   // color x intensity
uniform int uNL;
uniform vec3 uAmbient, uMistC, uMoonC;
uniform float uFogD, uMistK, uBridge;
uniform vec3 uBridgeCol;
vec3 capLight(vec3 wp, vec3 n, float wrap) {
    vec3 L = vec3(0.0);
    for (int i = 0; i < 8; i++) {
        if (i >= uNL) break;
        vec3 d = uLP[i].xyz - wp;
        float r = uLP[i].w;
        float dd = dot(d, d);
        float ndl = dot(n, d * inversesqrt(max(dd, 1e-12)));
        L += uLC[i] * (r * r / (dd + r * r)) * clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
    }
    return L;
}
vec3 fogged(vec3 c, vec3 wp, float z) {
    float f = 1.0 - exp(-max(z, 0.0) / uFogD);
    vec3 m = uMistC + capLight(wp, vec3(0.0, 1.0, 0.0), 1.0) * uMistK;
    return mix(mix(c, m, f), uBridgeCol, uBridge);
}
)";

// ---------------------------------------------------------------- the sky, the far trees, the floor, the soil
inline const char* BG_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uLight;
uniform float uPx, uTime, uDof, uSoilLight, uNetR, uGlowAll;
uniform vec3 uSkyTop, uSkyLow, uFloorC, uFloorLit, uSoilA, uSoilB, uSoilC, uSoilD, uRock, uSpeck, uThread;
uniform vec2 uMoonPos;

float layerDensity(float d, float b1, float b2, float b3, float b4) {
    if (d < b1) return 0.0;
    if (d < b2) return 0.07;
    if (d < b3) return 0.14;
    if (d < b4) return 0.26;
    return 0.5;
}

vec3 lightAtUv(vec2 uv) { vec3 l = texture(uLight, uv).rgb * uSoilLight; return l / (1.0 + 0.7 * l); }

// The soil's cut: horizons (litter, humus, topsoil, subsoil, weathered rock, bedrock), grain at every scale,
// stones at every scale lit by the threads' light, a fibrous haze where the threads are.
vec3 soil(vec2 p, vec2 uv, float px) {
    float s0 = surf(p.x);
    float d = s0 - p.y;
    float b1 = 0.022 * (0.6 + 0.8 * vnoise(vec2(p.x / 0.03, 3.1)));
    float b2 = 0.26 * (0.7 + 0.6 * vnoise(vec2(p.x / 0.4, 7.3)));
    float b3 = 1.15 * (0.7 + 0.6 * vnoise(vec2(p.x / 2.5, 1.7)));
    float b4 = 4.8 * (0.7 + 0.6 * vnoise(vec2(p.x / 11.0, 5.5)));
    float b5 = 21.0 * (0.7 + 0.6 * vnoise(vec2(p.x / 45.0, 9.1)));
    // wavy boundaries
    float wob = lfbm(p * vec2(1.0, 2.0), px * 6.0) - 0.5;
    float dd = d * (1.0 + 0.25 * wob);
    vec3 c = uSoilA;
    c = mix(c, uSoilB, smoothstep(b1 * 0.8 - px, b1 * 1.2 + px, dd));
    c = mix(c, uSoilC, smoothstep(b2 * 0.8 - px, b2 * 1.2 + px, dd));
    c = mix(c, uSoilD, smoothstep(b3 * 0.8 - px, b3 * 1.2 + px, dd));
    c = mix(c, mix(uSoilD, uRock, 0.6), smoothstep(b4 * 0.8 - px, b4 * 1.2 + px, dd));
    c = mix(c, uRock * 0.5, smoothstep(b5 * 0.85 - px, b5 * 1.15 + px, dd));
    // grain and mottling
    float g = lfbm(p, px * 1.5);
    float m = lfbm(p * vec2(0.7, 1.4) + 3.7, px * 14.0);
    c *= 0.62 + 0.55 * g + 0.35 * (m - 0.5);
    // the litter's leaf fragments (seen close only)
    float leafVis = smoothstep(0.0012, 0.00015, px) * (1.0 - smoothstep(b1 * 0.8, b1 * 1.3, d));
    if (leafVis > 0.0) {
        float lf = vnoise(vec2(p.x / 0.004, p.y / 0.0009));
        c = mix(c, uSoilC * vec3(1.15, 0.95, 0.8), smoothstep(0.62, 0.78, lf) * leafVis * 0.35);
    }
    // bedrock: fractures
    float rockK = smoothstep(b5 * 0.9, b5 * 1.1, d);
    if (rockK > 0.0) {
        float cr = abs(lfbm(p * 1.3 + 9.1, px * 3.0) - 0.5);
        c *= 1.0 - rockK * 0.7 * smoothstep(0.03, 0.0, cr);
    }
    vec3 lt = lightAtUv(uv);
    vec3 amb = mix(uAmbient, vec3(dot(uAmbient, vec3(0.333))), 0.6) * 1.6;
    vec3 col = c * (amb + lt);
    // stones at absolute scales, the biggest first
    float j0 = ceil(log(8.0 * px / 0.0004) / log(2.2));
    for (int i = 0; i < 7; i++) {
        float j = j0 + 6.0 - float(i);
        float sc = 0.0004 * pow(2.2, j);
        float fade = smoothstep(6.0, 14.0, sc / px);
        vec2 gc = floor(p / sc);
        bool hit = false;
        for (int oy = -1; oy <= 1 && !hit; oy++)
            for (int ox = -1; ox <= 1 && !hit; ox++) {
                vec2 cell = gc + vec2(ox, oy);
                float h = hash(cell + j * 13.1);
                vec2 cc = (cell + 0.5 + (hash2(cell + j * 7.7) - 0.5) * 0.4) * sc;
                float dc = s0 - cc.y;
                if (h > layerDensity(dc, b1, b2, b3, b5)) continue;
                float rad = sc * (0.17 + 0.17 * hash(cell + j * 3.3));
                if (dc < rad * 1.3) continue;
                vec2 dp = p - cc;
                float ang = hash(cell + j * 1.9) * 6.283;
                vec2 rp = mat2(cos(ang), sin(ang), -sin(ang), cos(ang)) * dp;
                rp.y /= 0.7 + 0.3 * hash(cell + j * 5.1);
                float a = atan(rp.y, rp.x);
                float rr = length(rp) / rad * (1.0 + 0.1 * sin(a * 3.0 + h * 20.0) + 0.06 * sin(a * 5.0 + h * 9.0));
                if (rr < 1.0) {
                    hit = true;
                    float e = sqrt(max(1.0 - rr * rr, 0.0));
                    vec3 n = normalize(vec3(dp / rad, e * 0.9));
                    // the light's direction from its gradient (toward the glowing threads)
                    vec2 tx = vec2(dFdx(uv.x), dFdy(uv.y)) * 6.0;
                    float lx = dot(lightAtUv(uv + vec2(tx.x, 0.0)) - lightAtUv(uv - vec2(tx.x, 0.0)), vec3(0.33));
                    float ly = dot(lightAtUv(uv + vec2(0.0, tx.y)) - lightAtUv(uv - vec2(0.0, tx.y)), vec3(0.33));
                    vec3 ld = normalize(vec3(lx, ly, 0.0) * 30.0 + vec3(0.0, 0.0, 0.35));
                    float dif = max(dot(n, ld), 0.0);
                    float hue = hash(cell + j * 2.3);
                    vec3 alb = uRock * (0.45 + 0.45 * hue) * mix(vec3(1.0), vec3(1.08, 0.98, 0.9), hash(cell + 4.4));
                    alb *= 0.85 + 0.3 * vnoise(p / (sc * 0.08));
                    vec3 sc3 = alb * (amb * (0.6 + 0.6 * e) + lt * (0.2 + 1.0 * dif)) + lt * pow(dif, 12.0) * 0.12;
                    float edge = smoothstep(1.0, 0.86, rr);
                    // a dark seam around the stone
                    col = mix(col * (1.0 - 0.5 * fade * smoothstep(1.18, 1.0, rr)), sc3, edge * fade);
                } else if (rr < 1.18) {
                    col *= 1.0 - 0.35 * fade * smoothstep(1.18, 1.0, rr);
                }
            }
        if (hit) break;
    }
    // a fibrous haze where the threads glow
    float fib = lfbm(p * vec2(1.0, 3.0) + 1.3, px * 1.2);
    col += lt * smoothstep(0.55, 0.85, fib) * 0.18;
    return col;
}

// The far tree line: rounded crowns at a depth, its height in meters.
float treeLine(float x, float seed) {
    float h = 19.0 + 9.0 * vnoise(vec2(x / 45.0, seed));
    float cell = floor(x / 9.0);
    float f = fract(x / 9.0) - 0.5;
    float hc = 0.7 + 0.5 * hash(vec2(cell, seed));
    float crown = sqrt(max(0.0, 1.0 - sq(f * 2.1))) * 6.0 * hc;
    return h * 0.75 + crown + 3.0 * vnoise(vec2(x / 3.0, seed + 2.0));
}

void main() {
    vec2 ndc = vUv * 2.0 - 1.0;
    vec2 w = uCam.xy + ndc * vec2(uCam.z * uCam.w, uCam.z);
    float D = uEyeP.z;
    vec3 col;
    float depth = 0.0, sky = 0.0;
    if (w.y < surf(w.x)) {
        col = soil(w, vUv, uPx);
        depth = 0.0;
    } else {
        // the ray from the eye through the window
        vec2 dir = w - uEyeP.xy;
        float t = -1.0;
        if (dir.y < 0.0) {
            t = (0.0 - uEyeP.y) / dir.y;
            for (int i = 0; i < 6; i++) t = mix(t, (surf(uEyeP.x + t * dir.x) - uEyeP.y) / dir.y, 0.7);
        }
        // the sky
        float el = (w.y - uEyeP.y) / D;   // the elevation (a tangent)
        vec3 skyC = mix(mix(uMistC, uSkyLow, smoothstep(0.0, 0.06, el)), uSkyTop, smoothstep(0.02, 0.34, el));
        // the moon (fixed on screen: it is at infinity) and its halo
        vec2 md = (ndc - uMoonPos) * vec2(uCam.w, 1.0);
        float mr = length(md);
        vec3 halo = uMoonC * (exp(-mr * 9.0) * 0.18 + exp(-mr * 2.2) * 0.06);
        skyC += halo;
        float disc = smoothstep(0.034, 0.03, mr);
        vec3 moonDisc = uMoonC * (1.1 - 0.25 * vnoise(md * 60.0) - 0.15 * smoothstep(0.0, 0.03, length(md - vec2(0.008, 0.006))));
        skyC = mix(skyC, moonDisc, disc);
        // the mist glowing low over the horizon
        col = skyC;
        sky = 1.0 - disc;
        depth = 1.0;
        // two far tree lines (fogged silhouettes)
        for (int L = 0; L < 2; L++) {
            float zf = L == 0 ? 900.0 : 380.0;
            float k = (D + zf) / D;
            float xf = uEyeP.x + dir.x * k, yf = uEyeP.y + dir.y * k;
            if (t > 0.0 && D * (t - 1.0) < zf) continue;
            float hl = treeLine(xf, float(L) * 7.3);
            float pxf = uPx * k;
            float coc = uDof * zf / (D + zf) * k;
            float aa = max(pxf, coc) * 1.2;
            float inside = smoothstep(aa, -aa, yf - hl);
            if (inside > 0.0) {
                vec3 wp = vec3(xf, yf, zf);
                vec3 tc = uFloorC * 0.25 + capLight(wp, vec3(0.0, 0.0, -1.0), 1.0) * 0.12;
                tc = fogged(tc, wp, zf) + halo * (1.0 - exp(-zf / uFogD));
                col = mix(col, tc, inside);
                sky *= 1.0 - inside;
                depth = mix(depth, zf / (zf + D), step(0.5, inside));
            }
        }
        if (t > 1.0) {
            // the forest floor: moss and litter, glowing specks over the network, the caps' light
            vec3 P = vec3(uEyeP.x + t * dir.x, uEyeP.y + t * dir.y, D * (t - 1.0));
            float pxz = uPx * t;
            vec2 fp = vec2(P.x, P.z * 0.6);
            float coc = uDof * P.z / (D + P.z) * t;
            float m = lfbm(fp, max(pxz * 2.0, coc));
            float m2 = lfbm(fp * 1.7 + 4.1, max(pxz * 8.0, coc));
            vec3 alb = uFloorC * (0.45 + 0.8 * m) * mix(vec3(1.0), vec3(1.25, 1.0, 0.7), smoothstep(0.55, 0.75, m2));
            vec3 L = uAmbient * 0.9 + capLight(P + vec3(0.0, 0.02 * uCam.z, 0.0), vec3(0.0, 1.0, 0.0), 0.3) * 1.2;
            L += uMoonC * 0.04;
            vec3 fc = alb * L * uFloorLit;
            // the threads' light seeping up near the cut
            fc += min(lightAtUv(vUv), vec3(0.3)) * 0.06 * exp(-P.z / (0.03 * uCam.z));
            // specks of foxfire where the network spreads below
            float near = 1.0 - smoothstep(0.6, 1.0, abs(P.x) / max(uNetR, 1e-6));
            if (near > 0.0) {
                float sc = exp2(ceil(log2(pxz * 18.0)));
                vec2 cell = floor(fp / sc);
                vec2 o = (cell + 0.2 + 0.6 * hash2(cell + 3.3)) * sc;
                float h = hash(cell + 9.1);
                float sp = h < 0.16 ? exp(-dot(fp - o, fp - o) / sq(max(pxz * 1.2, coc * 0.6))) : 0.0;
                fc += uSpeck * sp * near * (0.6 + 0.6 * sin(uTime * (0.6 + h * 2.0) + h * 40.0)) * uGlowAll;
            }
            col = fogged(fc, P, P.z) + halo * (1.0 - exp(-P.z / uFogD));
            sky = 0.0;
            depth = P.z / (P.z + D);
        }
    }
    gl_FragDepth = clamp(depth, 0.0, 0.999999);
    frag = vec4(mix(col, uBridgeCol, uBridge), sky * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the threads (and their light)
inline const char* THREAD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // a.xy, b.xy
layout(location = 2) in vec4 aB; // width, g0, g1, kind
layout(location = 3) in vec4 aC; // q0, q1, hash, class
uniform float uG, uPxW, uMode;
out vec2 vP;
flat out vec2 vA, vB;
flat out float vW, vHR, vAlpha, vQ0, vQ1, vHash, vFresh, vTip, vKind;
void main() {
    float fr = clamp((uG - aB.y) / max(aB.z - aB.y, 1e-4), 0.0, 1.0);
    float lenPx = length(aA.zw - aA.xy) / uPxW;
    float vis = aB.w > 1.5 && aB.w < 2.5 ? smoothstep(1.5, 4.0, lenPx) : smoothstep(0.8, 3.0, lenPx);
    if (uMode > 0.5) vis = smoothstep(0.4, 2.0, lenPx);
    if (fr <= 0.0 || vis <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    vec2 a = aA.xy, b = mix(aA.xy, aA.zw, fr);
    float age = max(uG - aB.z, 0.0);
    float w = aB.x * 0.5 * clamp(exp(0.4 * age), 1.0, 1.0 + 1.6 * aC.w * aC.w);
    float alpha = vis;
    float minW = uMode > 0.5 ? 0.5 : 0.55;
    if (w / uPxW < minW) { alpha *= w / uPxW / minW; w = minW * uPxW; }
    float hr = uMode > 0.5 ? max(w * 18.0, 4.0 * uPxW) : max(w * 6.0, 3.2 * uPxW);
    vec2 d = b - a;
    float L = length(d);
    vec2 al = L > 1e-9 ? d / L : vec2(1.0, 0.0);
    vec2 pe = vec2(-al.y, al.x);
    vec2 base = aPos.x < 0.0 ? a : b;
    vec2 p = base + al * aPos.x * hr + pe * aPos.y * hr;
    vP = p; vA = a; vB = b; vW = w; vHR = hr; vAlpha = alpha;
    vQ0 = aC.x; vQ1 = mix(aC.x, aC.y, fr); vHash = aC.z; vKind = aB.w;
    vFresh = exp(-age / 0.35);
    vTip = fr < 1.0 ? 1.0 : exp(-age / 0.12);
    vec2 ndc = (p - uCam.xy) / vec2(uCam.z * uCam.w, uCam.z);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";
inline const char* THREAD_FS = R"(#version 330 core
in vec2 vP;
flat in vec2 vA, vB;
flat in float vW, vHR, vAlpha, vQ0, vQ1, vHash, vFresh, vTip, vKind;
out vec4 frag;
uniform float uPxW, uMode, uBright, uSurfPx;
uniform vec4 uPulse[6];   // q, amplitude, fraction of the threads, seed
uniform vec3 uWave;       // q, amplitude, width
uniform vec3 uThreadC, uTipC, uPulseC;
void main() {
    vec2 pa = vP - vA, ba = vB - vA;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-14), 0.0, 1.0);
    float d = length(pa - ba * h);
    float q = mix(vQ0, vQ1, h);
    // the pulses travelling out along the threads
    float pulse = 0.0;
    for (int i = 0; i < 6; i++) {
        vec4 P = uPulse[i];
        if (P.y <= 0.0) continue;
        float on = step(fract(vHash * 7.31 + P.w), P.z);
        pulse += P.y * on * exp(-sq((q - P.x) / 0.16));
    }
    float wave = uWave.y * (exp(-sq((q - uWave.x) / uWave.z)) + 0.35 * step(q, uWave.x) * exp(-(uWave.x - q) / (uWave.z * 4.0)));
    float tip = vTip * exp(-length(vP - vB) / max(vW * 5.0, uPxW * 3.0));
    float fine = vKind > 1.5 && vKind < 2.5 ? 0.5 : (0.45 + 0.75 * fract(vHash * 13.7)) * (vKind < 0.5 ? 1.35 : 1.0);
    vec3 base = mix(uThreadC, uTipC, clamp(vFresh * 0.8 + (vKind > 2.5 ? 0.5 : 0.0), 0.0, 1.0));
    vec3 c = base * (uBright * fine + pulse * 0.7 + wave) + uPulseC * (pulse * 0.6 + wave * 0.6 + tip * 2.2);
    float a;
    if (uMode > 0.5) {
        a = exp(-d * d / (vHR * vHR * 0.2)) * smoothstep(1.0, 0.7, d / vHR);
        c *= 0.55;
    } else {
        float core = 1.0 - smoothstep(vW - uPxW * 0.6, vW + uPxW * 0.6, d);
        float halo = exp(-d * d / (vHR * vHR * 0.12));
        float soft = exp(-d * d / (vHR * vHR * 0.6)) * smoothstep(1.0, 0.55, d / vHR);
        a = core * 0.9 + halo * 0.3 + soft * 0.12;
    }
    // never above the ground
    a *= smoothstep(uSurfPx, -uSurfPx, vP.y - surf(vP.x));
    frag = vec4(c * a * vAlpha, 0.0);
}
)";

// The nodes: knots where threads branch or fuse, the spore.
inline const char* NODE_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, radius, g
layout(location = 2) in vec4 aB; // q, hash, kind, -
uniform float uG, uPxW;
out vec2 vL;
flat out float vR, vAlpha, vQ, vHash, vKind, vFresh;
void main() {
    float on = smoothstep(aA.w, aA.w + 0.08, uG);
    float rpx = aA.z / uPxW;
    float vis = aB.z > 1.5 ? 1.0 - smoothstep(1.0, 0.25, rpx) * 0.0 : smoothstep(0.35, 1.2, rpx);
    if (on <= 0.0 || vis <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float r = max(aA.z, uPxW * 0.8);
    float ext = r * (aB.z > 1.5 ? 6.0 : 4.0);
    vL = aPos * ext;
    vR = r; vAlpha = on * vis * (aB.z > 1.5 ? min(1.0, rpx / 0.8) : 1.0); vQ = aB.x; vHash = aB.y; vKind = aB.z;
    vFresh = exp(-max(uG - aA.w, 0.0) / 0.3);
    vec2 p = aA.xy + aPos * ext;
    gl_Position = vec4((p - uCam.xy) / vec2(uCam.z * uCam.w, uCam.z), 0.0, 1.0);
}
)";
inline const char* NODE_FS = R"(#version 330 core
in vec2 vL;
flat in float vR, vAlpha, vQ, vHash, vKind, vFresh;
out vec4 frag;
uniform float uPxW, uBright, uTime;
uniform vec4 uPulse[6];
uniform vec3 uWave;
uniform vec3 uThreadC, uTipC, uPulseC;
void main() {
    float d = length(vL);
    float pulse = 0.0;
    for (int i = 0; i < 6; i++) {
        vec4 P = uPulse[i];
        if (P.y <= 0.0) continue;
        pulse += P.y * step(fract(vHash * 7.31 + P.w), P.z) * exp(-sq((vQ - P.x) / 0.16));
    }
    float wave = uWave.y * exp(-sq((vQ - uWave.x) / uWave.z));
    float core = 1.0 - smoothstep(vR - uPxW * 0.7, vR + uPxW * 0.7, d);
    float halo = exp(-d * d / (vR * vR * (vKind > 1.5 ? 8.0 : 3.0))) * smoothstep(1.0, 0.7, d / (vR * (vKind > 1.5 ? 6.0 : 4.0)));
    vec3 c;
    if (vKind > 1.5) {
        // the spore: a bright shell around a glowing heart
        float shell = exp(-sq((d - vR * 0.85) / (vR * 0.12 + uPxW * 0.6)));
        float breathe = 0.85 + 0.15 * sin(uTime * 1.3);
        c = uThreadC * halo * 0.3 + uTipC * core * 0.4 * breathe + uPulseC * (shell * 0.5 + core * exp(-d / (vR * 0.3)) * 0.6);
        c *= 1.0 + wave * 0.5;
    } else {
        float k = vKind > 0.5 ? 1.5 : 0.9;
        c = mix(uThreadC, uTipC, 0.4 + 0.5 * vFresh) * (core * 1.1 + halo * 0.4) * k * (uBright + pulse + wave) +
            uPulseC * core * (pulse * 0.5 + wave * 0.5 + vFresh * 0.8);
    }
    frag = vec4(c * vAlpha, 0.0);
}
)";

// ---------------------------------------------------------------- roots in the cut
inline const char* ROOT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // a.xy, b.xy
layout(location = 2) in vec4 aB; // radius a, radius b, shade, class
uniform float uPxW;
out vec2 vP;
flat out vec2 vA, vB;
flat out float vRa, vRb, vShade, vAlpha;
void main() {
    float rpx = aB.x / uPxW;
    float vis = smoothstep(0.25, 0.9, rpx);
    if (vis <= 0.0) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
    float ra = max(aB.x, uPxW * 0.7), rb = max(aB.y, uPxW * 0.7);
    float ext = max(ra, rb) + uPxW * 1.5;
    vec2 d = aA.zw - aA.xy;
    float L = length(d);
    vec2 al = L > 1e-9 ? d / L : vec2(1.0, 0.0);
    vec2 pe = vec2(-al.y, al.x);
    vec2 base = aPos.x < 0.0 ? aA.xy : aA.zw;
    vec2 p = base + al * aPos.x * ext + pe * aPos.y * ext;
    vP = p; vA = aA.xy; vB = aA.zw; vRa = ra; vRb = rb; vShade = aB.z; vAlpha = vis;
    gl_Position = vec4((p - uCam.xy) / vec2(uCam.z * uCam.w, uCam.z), 0.0, 1.0);
}
)";
inline const char* ROOT_FS = R"(#version 330 core
in vec2 vP;
flat in vec2 vA, vB;
flat in float vRa, vRb, vShade, vAlpha;
out vec4 frag;
uniform sampler2D uLight;
uniform vec2 uRes;
uniform float uPxW, uSoilLight;
uniform vec3 uRootC, uAmbient;
void main() {
    vec2 pa = vP - vA, ba = vB - vA;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-14), 0.0, 1.0);
    float d = length(pa - ba * h);
    float r = mix(vRa, vRb, h);
    float cov = 1.0 - smoothstep(r - uPxW * 0.7, r + uPxW * 0.7, d);
    if (cov <= 0.002) discard;
    vec3 lt = texture(uLight, gl_FragCoord.xy / uRes).rgb * uSoilLight;
    lt = lt / (1.0 + 0.7 * lt);
    float across = clamp(d / r, 0.0, 1.0);
    float round_ = sqrt(max(1.0 - across * across, 0.0));
    // fine bark rings along the root
    float rings = 0.85 + 0.15 * vnoise(vec2(h * length(ba) / (r * 0.7), across * 3.0));
    vec3 alb = uRootC * vShade * rings;
    vec3 c = alb * (uAmbient * 1.6 * (0.5 + 0.5 * round_) + lt * (0.3 + 1.1 * round_)) + lt * 0.15 * pow(round_, 6.0);
    c *= 0.55 + 0.45 * smoothstep(1.0, 0.6, across);
    float a = cov * vAlpha * smoothstep(uPxW, -uPxW, vP.y - surf(vP.x));
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- plants, trees, rocks, mushrooms
// Each one an instanced quad standing on the floor at its depth, drawn as a 2D shape (in units of its height),
// softened by the depth of field, lit by the caps (3D distances) and the moon, fogged by its depth.
inline const char* BOARD_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, z, height
layout(location = 2) in vec4 aB; // half width, type, seed, alpha
layout(location = 3) in vec4 aC; // params
layout(location = 4) in vec4 aD; // color, glow
layout(location = 5) in vec4 aE; // color 2, param
uniform float uPxW, uDof;
out vec2 vL;
out vec3 vW;
flat out vec4 vA, vB, vC, vD, vE;
flat out float vPxL, vCoc, vSlope;
void main() {
    float h = aA.w;
    float bot = aB.y < 0.5 ? 0.06 : 0.04;
    vec2 loc = vec2(aPos.x * aB.x, mix(-bot * h, h * 1.06, aPos.y * 0.5 + 0.5));
    vec3 wp = vec3(aA.x + loc.x, aA.y + loc.y, aA.z);
    vL = loc / h;
    vW = wp;
    vA = aA; vB = aB; vC = aC; vD = aD; vE = aE;
    float s = uEyeP.z / (uEyeP.z + aA.z);
    vPxL = uPxW / (h * s);
    vCoc = uDof * aA.z / (uEyeP.z + aA.z) / (h * s);
    vSlope = (aA.y + h * 0.8 - uEyeP.y) / (uEyeP.z + aA.z);
    gl_Position = worldClip(wp);
}
)";
inline const char* BOARD_FS = R"(#version 330 core
in vec2 vL;
in vec3 vW;
flat in vec4 vA, vB, vC, vD, vE;
flat in float vPxL, vCoc, vSlope;
out vec4 frag;
uniform float uTime, uWind, uWindPhase;
uniform vec3 uStemC, uFoliageC, uBarkC, uRockC, uThreadC;

float aaW;
float cover(float sd) { return smoothstep(aaW, -aaW, sd); }
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
float sway(float v, float seed) {
    return (sin(uWindPhase * 1.3 + seed * 17.0 + vA.x * 0.7) * 0.6 + sin(uWindPhase * 0.7 + seed * 5.0) * 0.4) * uWind * v * v;
}

// ---- the mushroom (parasol tower, bell, dome)
vec4 mushroom(vec2 q) {
    float open = vC.x, kind = vC.y, wave = vC.z, lean = vC.w;
    float glow = vD.w;
    vec3 capC = vD.rgb, gillC = vE.rgb;
    float sw = sway(1.0, vB.z) * 0.25;
    float hs = kind < 0.5 ? 0.8 : kind < 1.5 ? 0.72 : 0.64;
    float x0 = (lean + sw) * q.y * q.y / (hs * hs) * hs;
    float w0 = kind < 0.5 ? 0.034 : kind < 1.5 ? 0.024 : 0.075;
    vec2 cap0 = vec2((lean + sw) * hs, hs);
    float rc = kind < 0.5 ? 0.1 + 0.36 * open : kind < 1.5 ? 0.09 + 0.08 * open : 0.12 + 0.24 * open;
    float hc = kind < 0.5 ? 0.09 + 0.26 * (1.0 - open) : kind < 1.5 ? 0.3 - 0.06 * open : 0.2 + 0.12 * (1.0 - open);
    // the stem
    float v = clamp(q.y / hs, 0.0, 1.0);
    float ws = w0 * (1.0 + 0.7 * exp(-v * 9.0) * (kind > 1.5 ? 1.6 : 1.0)) * (1.0 - 0.18 * v);
    float sdStem = max(abs(q.x - x0) - ws, max(-q.y - 0.02, q.y - hs));
    // the cap
    vec2 cq = q - cap0;
    float xr = cq.x / rc;
    float ax = abs(xr);
    float yTop, yBot;
    if (kind < 0.5) {
        yTop = hc * pow(max(1.0 - ax * ax, 0.0), 0.6) + 0.035 * exp(-sq(xr / 0.18));
        yBot = -0.025 * (1.0 - ax * ax) + 0.012;
    } else if (kind < 1.5) {
        yTop = hc * pow(max(1.0 - ax * ax, 0.0), 0.55) * (1.0 - 0.25 * ax * ax) + 0.02 * exp(-sq(xr / 0.3));
        yBot = hc * 0.12 * (1.0 - ax * ax) - 0.035 * ax * ax;
    } else {
        yTop = hc * sqrt(max(1.0 - ax * ax, 0.0));
        yBot = -0.02 * (1.0 - ax * ax);
    }
    float sdCap = max(max(ax - 1.0, 0.0) * rc + (ax > 1.0 ? 0.0 : -1.0) * 0.0, 0.0);
    sdCap = max(ax * rc - rc, max(yBot - cq.y, cq.y - yTop));
    // the gills, seen from below when the cap is above the eye
    float under = clamp(vSlope * 3.5, 0.0, 1.0) * step(kind, 1.5) * (kind < 0.5 ? 1.0 : 0.6);
    float gh = rc * 0.24 * under;
    float ey = (cq.y - yBot) / max(gh, 1e-4);
    float sdGill = gh > 1e-4 ? (length(vec2(cq.x / (rc * 0.96), (cq.y - (kind < 0.5 ? 0.012 : -0.02)) / max(gh, 1e-4))) - 1.0) * min(rc, gh) : 1.0;
    sdGill = max(sdGill, cq.y - (kind < 0.5 ? 0.012 : -0.02));
    // the ring under a parasol's cap
    float sdRing = kind < 0.5 ? max(abs(q.x - x0) - ws * (1.6 + 1.4 * (hs - 0.1 - q.y) / 0.05), abs(q.y - (hs - 0.115)) - 0.022) : 1.0;
    sdRing = kind < 0.5 && q.y < hs - 0.09 && q.y > hs - 0.14 ? sdRing : 1.0;

    vec3 col = vec3(0.0);
    float a = 0.0;
    // stem: pale, faintly glowing, fibrils, the drop's light climbing it
    float cs = cover(sdStem);
    if (cs > 0.0) {
        float across = clamp((q.x - x0) / ws, -1.0, 1.0);
        float fib = 0.85 + 0.15 * vnoise(vec2(across * 4.0, q.y * 30.0 + vB.z * 10.0));
        vec3 n = normalize(vec3(across, 0.0, -sqrt(max(1.0 - across * across, 0.0))));
        vec3 lit = uAmbient + capLight(vW, -n, 0.5) * 0.7 + uMoonC * 0.08 * max(across, 0.0);
        vec3 sc = uStemC * lit * fib + capC * glow * (0.12 + 0.25 * smoothstep(0.5, 1.0, v)) * (1.0 - 0.4 * abs(across));
        float climb = wave > 0.0 ? exp(-max(wave - v, 0.0) * 6.0) * step(v, wave + 0.02) * (wave < 1.2 ? 1.0 : 0.0) : 0.0;
        sc += mix(capC, vec3(1.0), 0.4) * climb * 1.6 * (1.0 - 0.5 * abs(across));
        col = sc; a = cs;
    }
    float cr = cover(sdRing);
    if (cr > 0.0) {
        vec3 rcC = uStemC * (uAmbient + capLight(vW, vec3(0.0, 0.0, -1.0), 0.5) * 0.6) * 0.9 + gillC * glow * 0.25;
        col = mix(col, rcC, cr); a = max(a, cr);
    }
    float cg = cover(sdGill) * under;
    if (cg > 0.0) {
        float ang = atan((cq.y - yBot) / max(gh, 1e-4), cq.x / rc);
        float rr = length(vec2(cq.x / rc, (cq.y - yBot) / max(gh, 1e-4)));
        float lines = 0.5 + 0.5 * cos(cq.x / rc / max(rr, 0.05) * 70.0);
        lines = mix(lines, 0.6, clamp(vCoc / 0.01 + vPxL * 200.0, 0.0, 1.0));
        vec3 gc = gillC * glow * (0.35 + 0.9 * lines) * (0.4 + 0.9 * smoothstep(0.1, 1.0, rr));
        col = mix(col, gc, cg); a = max(a, cg);
    }
    float cc = cover(sdCap);
    if (cc > 0.0) {
        float yy = clamp((cq.y - yBot) / max(yTop - yBot, 1e-4), 0.0, 1.0);
        vec3 n = normalize(vec3(xr * 0.9, yy * 0.8 + 0.1, -0.6));
        float edge = clamp(-sdCap / max(rc * 0.12, 1e-4), 0.0, 1.0);
        // translucent: brightest toward the rim and the lip, deeper over the top
        float rim = smoothstep(0.45, 1.0, ax) * 0.7 + (1.0 - edge) * 0.6 + (1.0 - yy) * 0.35;
        float stri = 0.9 + 0.1 * sin(xr * 38.0 + vB.z * 20.0);
        stri = mix(stri, 1.0, clamp(vCoc / 0.01, 0.0, 1.0));
        vec3 em = capC * glow * (0.22 + 0.7 * rim) * stri;
        if (kind > 1.5) {
            // glowing spots on the dome
            vec2 sp = vec2(xr * 5.0, yy * 3.0 + vB.z * 7.0);
            vec2 cell = floor(sp);
            vec2 o = cell + 0.3 + 0.4 * hash2(cell + vB.z);
            float spot = smoothstep(0.32, 0.18, length(sp - o)) * step(hash(cell + 2.0 + vB.z), 0.55) * (1.0 - smoothstep(0.75, 1.0, ax));
            em = mix(em, gillC * glow * 1.6, spot);
        }
        vec3 lit = capC * 0.15 * (uAmbient + capLight(vW, n, 0.3) * 0.5);
        vec3 moon = uMoonC * 0.35 * smoothstep(0.85, 1.0, yy) * smoothstep(-0.2, 0.6, xr);
        col = mix(col, em + lit + moon, cc); a = max(a, cc);
    }
    return vec4(col, a);
}

// ---- a tree: a dark trunk with a root flare, a leafy crown lit from below by the caps
vec4 tree(vec2 q) {
    float rt = vC.x, cw = vC.z;
    float seed = vB.z;
    float lean = (seed - 0.5) * 0.05;
    float xc = lean * q.y + sway(max(q.y - 0.4, 0.0), seed) * 0.03;
    float tw = rt * (1.0 - 0.45 * q.y) * (1.0 + 1.8 * exp(-q.y * 30.0));
    float sdT = max(abs(q.x - xc) - tw, max(-q.y - 0.05, q.y - 0.9));
    // limbs
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float y0 = 0.42 + 0.12 * fi + 0.05 * hash1(seed * 9.0 + fi);
        float side = mod(fi + floor(seed * 2.0), 2.0) < 1.0 ? -1.0 : 1.0;
        vec2 a0 = vec2(xc, y0), b0 = vec2(xc + side * cw * (0.5 + 0.3 * hash1(seed + fi)), y0 + 0.14);
        sdT = min(sdT, sdSeg(q, a0, b0) - tw * 0.4);
    }
    // the crown: lumpy blobs with a leafy edge
    float sdC = 1e9;
    vec2 nearC = vec2(0.0, 0.7);
    for (int i = 0; i < 7; i++) {
        float fi = float(i);
        vec2 c = vec2(xc + (hash1(seed * 3.0 + fi) - 0.5) * cw * 1.4, vC.y + 0.1 + hash1(seed * 5.0 + fi * 1.3) * (0.95 - vC.y - 0.12));
        float r = cw * (0.28 + 0.2 * hash1(seed * 7.0 + fi));
        float dd = length(q - c) - r;
        if (dd < sdC) { sdC = dd; nearC = c; }
    }
    float det = 1.0 / (1.0 + vCoc / 0.012);
    float leafy = (fbm(q * 28.0 + seed * 10.0) - 0.5) * 0.06 * det + (vnoise(q * 90.0 + seed) - 0.5) * 0.02 * det;
    sdC += leafy;
    vec3 col = vec3(0.0);
    float a = 0.0;
    float ct = cover(sdT);
    if (ct > 0.0) {
        float across = clamp((q.x - xc) / max(tw, 1e-4), -1.0, 1.0);
        vec3 n = normalize(vec3(across, 0.0, -sqrt(max(1.0 - across * across, 0.0))));
        float bark = 0.8 + 0.3 * vnoise(vec2(across * 3.0, q.y * 40.0 / max(rt * 20.0, 0.2))) * det;
        vec3 c = uBarkC * bark * (uAmbient * 0.8 + capLight(vW, n, 0.4) * 0.9 + uMoonC * 0.1 * smoothstep(0.2, 1.0, across));
        // bracket fungi glowing on the bark
        float fy = fract(q.y * 6.0 + seed * 3.0);
        float fz = step(hash1(floor(q.y * 6.0 + seed * 3.0) + seed * 11.0), 0.35) * step(q.y, 0.5);
        c += uThreadC * 0.7 * fz * smoothstep(0.25, 0.0, abs(fy - 0.5) - 0.05) * smoothstep(0.55, 1.0, abs(across)) * det;
        col = c; a = ct;
    }
    float cc = cover(sdC);
    if (cc > 0.0) {
        vec2 dn = (q - nearC);
        vec3 n = normalize(vec3(dn / max(cw * 0.4, 1e-3), -0.5));
        float leaf = 0.75 + 0.5 * vnoise(q * 60.0 + seed * 3.0) * det;
        vec3 c = uFoliageC * leaf * (uAmbient * 0.7 + capLight(vW, n, 0.6) * 1.1 + uMoonC * 0.12 * smoothstep(0.0, 0.8, n.y + n.x * 0.5));
        col = mix(col, c, cc); a = max(a, cc);
    }
    return vec4(col, a);
}

// ---- grass: a tuft of curving blades
vec4 grass(vec2 q) {
    float seed = vB.z;
    float sd = 1e9;
    float hb = 1.0;
    for (int i = 0; i < 9; i++) {
        float fi = float(i);
        float h1 = hash1(seed * 13.0 + fi);
        float len = 0.55 + 0.45 * hash1(seed * 7.0 + fi * 3.1);
        float bx = (h1 - 0.5) * 0.35;
        float bend = (hash1(seed * 5.0 + fi * 2.3) - 0.5) * 0.7 + sway(1.0, seed + fi * 0.1) * 0.4;
        float t = clamp(q.y / len, 0.0, 1.0);
        float x = bx + bend * t * t * len + bx * t * 0.6;
        float w = 0.022 * (1.0 - t);
        float d = max(abs(q.x - x) - w, max(-q.y, q.y - len));
        if (d < sd) { sd = d; hb = t; }
    }
    float c = cover(sd);
    vec3 col = uFoliageC * (0.8 + 0.6 * hb) * (uAmbient + capLight(vW, vec3(0.0, 0.3, -1.0), 0.6) * 1.1 + uMoonC * 0.06 * hb);
    return vec4(col, c);
}

// ---- a fern: three arching fronds with their leaflets
vec4 fern(vec2 q) {
    float seed = vB.z;
    float sd = 1e9;
    float lit = 0.0;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        float side = fi - 1.0;
        float bend = side * (0.45 + 0.2 * hash1(seed + fi)) + sway(1.0, seed + fi) * 0.3;
        float len = 0.85 + 0.15 * hash1(seed * 3.0 + fi);
        float t = clamp(q.y / len, 0.0, 1.0);
        float x = bend * t * t * 0.6;
        float dx = q.x - x;
        float pin = 0.17 * sin(t * 3.1416) * (1.0 - 0.3 * t);
        float comb = sin(t * 46.0 + seed * 9.0);
        float d = max(abs(dx) - max(pin * smoothstep(-0.3, 0.2, comb), 0.008), max(-q.y, q.y - len));
        if (d < sd) { sd = d; lit = t; }
    }
    float c = cover(sd);
    vec3 col = uFoliageC * 1.1 * (0.7 + 0.5 * lit) * (uAmbient + capLight(vW, vec3(0.0, 0.4, -1.0), 0.6) * 1.1);
    return vec4(col, c);
}

// ---- a rock, mossy on top
vec4 rock(vec2 q) {
    float seed = vB.z;
    vec2 c = vec2(0.0, 0.42);
    vec2 d = q - c;
    float a = atan(d.y, d.x);
    float r = 0.45 * (1.0 + 0.12 * sin(a * 2.0 + seed * 9.0) + 0.06 * sin(a * 5.0 + seed * 3.0));
    float sd = max(length(d * vec2(0.8, 1.0)) - r, -q.y);
    float cv = cover(sd);
    vec3 n = normalize(vec3(d / 0.5, -0.6));
    float det = 1.0 / (1.0 + vCoc / 0.01);
    vec3 alb = mix(uRockC * 0.5 * (0.8 + 0.4 * vnoise(q * 12.0 + seed) * det), uFoliageC * 1.2, smoothstep(0.3, 0.7, n.y + 0.2 * vnoise(q * 20.0)));
    vec3 col = alb * (uAmbient + capLight(vW, n, 0.3) + uMoonC * 0.1 * max(n.y + n.x, 0.0));
    return vec4(col, cv);
}

// ---- a bush: low leafy blobs
vec4 bush(vec2 q) {
    float seed = vB.z;
    float sd = 1e9;
    vec2 nc = vec2(0.0);
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        vec2 c = vec2((hash1(seed * 3.0 + fi) - 0.5) * 1.1, 0.3 + 0.4 * hash1(seed * 5.0 + fi));
        float r = 0.22 + 0.14 * hash1(seed * 7.0 + fi);
        float d = length(q - c) - r;
        if (d < sd) { sd = d; nc = c; }
    }
    float det = 1.0 / (1.0 + vCoc / 0.012);
    sd += (fbm(q * 22.0 + seed * 10.0) - 0.5) * 0.08 * det;
    sd = max(sd, -q.y);
    vec3 n = normalize(vec3((q - nc) / 0.3, -0.5));
    vec3 col = uFoliageC * (0.8 + 0.4 * vnoise(q * 50.0) * det) * (uAmbient * 0.8 + capLight(vW, n, 0.6) * 1.1 + uMoonC * 0.08 * max(n.y, 0.0));
    return vec4(col, cover(sd));
}

// ---- pins: a little cluster of tiny glowing mushrooms
vec4 pins(vec2 q) {
    float seed = vB.z;
    vec3 col = vec3(0.0);
    float a = 0.0;
    for (int i = 0; i < 5; i++) {
        float fi = float(i);
        if (fi > 2.0 + vC.x * 3.0) break;
        float x = (hash1(seed * 3.0 + fi) - 0.5) * 0.9;
        float h = 0.35 + 0.6 * hash1(seed * 5.0 + fi);
        float r = 0.07 + 0.07 * hash1(seed * 9.0 + fi);
        float lean = (hash1(seed * 2.0 + fi) - 0.5) * 0.2;
        float xs = x + lean * q.y / h;
        float sdS = max(abs(q.x - xs) - 0.018, max(-q.y, q.y - h));
        vec2 cq = q - vec2(x + lean, h);
        float ax = abs(cq.x) / r;
        float sdC = max(abs(cq.x) - r, max(-cq.y - 0.01, cq.y - r * 1.1 * pow(max(1.0 - ax, 0.0), 0.7)));
        float cs = cover(sdS), cc = cover(sdC);
        vec3 c = vD.rgb * vD.w;
        col = mix(col, uStemC * 0.4 + c * 0.25, cs); a = max(a, cs);
        col = mix(col, c * (0.6 + 0.8 * ax), cc); a = max(a, cc);
    }
    return vec4(col, a);
}

// ---- moss: a cushion with spore stalks
vec4 moss(vec2 q) {
    float seed = vB.z;
    float det = 1.0 / (1.0 + vCoc / 0.01);
    float top = 0.32 * sqrt(max(1.0 - sq(q.x / 0.9), 0.0)) + (vnoise(vec2(q.x * 30.0, seed)) - 0.5) * 0.08 * det;
    float sd = max(q.y - top, -q.y);
    vec3 col = uFoliageC * 1.3 * (0.7 + 0.5 * vnoise(q * 40.0 + seed) * det);
    float lit = 0.0;
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        float x = (hash1(seed * 3.0 + fi) - 0.5) * 1.2;
        float h = 0.45 + 0.45 * hash1(seed * 7.0 + fi);
        float bend = (hash1(seed * 11.0 + fi) - 0.5) * 0.3 + sway(1.0, seed + fi) * 0.2;
        float t = clamp(q.y / h, 0.0, 1.0);
        float ds = max(abs(q.x - x - bend * t * t) - 0.012, max(-q.y, q.y - h));
        float dc = length(q - vec2(x + bend, h)) - 0.035;
        float dmin = min(ds, dc);
        if (dmin < sd) { sd = dmin; lit = dc < ds ? 1.0 : 0.3; }
    }
    vec3 c = col * (uAmbient + capLight(vW, vec3(0.0, 0.5, -1.0), 0.6)) + uThreadC * 0.25 * lit * vD.w;
    return vec4(c, cover(sd));
}

void main() {
    int type = int(vB.y + 0.5);
    aaW = max(vPxL * 0.9, vCoc);
    vec4 r;
    if (type == 0) r = mushroom(vL);
    else if (type == 1) r = tree(vL);
    else if (type == 2) r = grass(vL);
    else if (type == 3) r = fern(vL);
    else if (type == 4) r = rock(vL);
    else if (type == 5) r = bush(vL);
    else if (type == 6) r = pins(vL);
    else r = moss(vL);
    float a = r.a * vB.w;
    if (a <= 0.003) discard;
    vec3 c = r.rgb;
    float fz = vA.z;
    if (type == 0 || type == 6) {
        // glowing things fog less (their light carries)
        float f = 1.0 - exp(-fz / uFogD);
        c = mix(c, uMistC, f * 0.5);
        c = mix(c, uBridgeCol, uBridge);
    } else {
        c = fogged(c, vW, fz);
    }
    frag = vec4(c * a, a);
}
)";

// ---------------------------------------------------------------- spores and halos in the air
inline const char* GLOW_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, z, size (m)
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // kind, seed, -, -
uniform float uPxW;
out vec2 vL;
flat out vec4 vB, vC;
flat out float vPx;
void main() {
    float s = uEyeP.z / (uEyeP.z + aA.z);
    float sizePx = aA.w * s / uPxW;
    float size = aA.w;
    float alpha = aB.a;
    if (sizePx < 1.5) { alpha *= sizePx * sizePx / 2.25; size = 1.5 * uPxW / s; }
    vL = aPos;
    vB = vec4(aB.rgb, alpha); vC = aC;
    vPx = 1.0 / max(size * s / uPxW, 1e-3);
    gl_Position = worldClip(vec3(aA.xy + aPos * size, aA.z));
}
)";
inline const char* GLOW_FS = R"(#version 330 core
in vec2 vL;
flat in vec4 vB, vC;
flat in float vPx;
out vec4 frag;
void main() {
    float r = length(vL);
    int kind = int(vC.x + 0.5);
    float v;
    if (kind == 1) v = exp(-r * r * 3.0) * 0.7 + exp(-r * r * 9.0) * 0.3;          // a halo in the mist
    else if (kind == 2) v = exp(-r * r * 6.0) + 0.5 * exp(-r * r * 40.0);          // the network's heart
    else v = exp(-r * r * 16.0) + 0.25 * exp(-r * r * 4.0);                         // a spore
    v *= smoothstep(1.0, 0.75, r);
    frag = vec4(vB.rgb * v * vB.a, 0.0);
}
)";

// ---------------------------------------------------------------- the world composited with its bloom, the stars
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uWorld, uBloomT;
uniform vec2 uRes;
uniform float uBloomK, uTwinkle, uTime, uStars;
void main() {
    vec4 w = texture(uWorld, vUv);
    vec3 c = w.rgb + texture(uBloomT, vUv).rgb * uBloomK;
    if (w.a > 0.002 && uStars > 0.0) {
        vec2 sp = gl_FragCoord.xy / max(uRes.y / 1080.0, 0.5);
        vec3 st = vec3(0.0);
        for (int L = 0; L < 2; L++) {
            float cell = L == 0 ? 7.0 : 23.0;
            vec2 g = floor(sp / cell);
            float rr = hash(g + float(L) * 31.0);
            if (rr < 0.3) {
                vec2 o = (g + 0.25 + 0.5 * hash2(g + 3.1)) * cell;
                float b = pow(hash(g + 7.7), L == 0 ? 3.0 : 1.5);
                float rad = (L == 0 ? 0.7 : 1.1) + 0.6 * b;
                float tw = 1.0 + uTwinkle * sin(uTime * (0.8 + 2.0 * hash(g + 1.3)) + rr * 80.0);
                vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.8), hash(g + 9.1));
                st += tint * exp(-dot(sp - o, sp - o) / (rad * rad)) * b * tw * (L == 0 ? 0.5 : 1.1);
            }
        }
        // fewer stars low over the horizon (the mist)
        c += st * w.a * uStars * smoothstep(0.35, 0.75, vUv.y);
    }
    frag = vec4(c, 1.0);
}
)";
inline const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uTexel;
uniform float uThreshold;
void main() {
    vec3 c = vec3(0.0);
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++) c += texture(uSrc, vUv + vec2(x, y) * uTexel).rgb;
    c /= 9.0;
    float l = max(c.r, max(c.g, c.b));
    frag = vec4(c * max(l - uThreshold, 0.0) / max(l, 1e-3), 1.0);
}
)";
inline const char* BLUR_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uDir;
void main() {
    const float w[5] = float[5](0.227, 0.194, 0.121, 0.054, 0.016);
    vec3 c = texture(uSrc, vUv).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uSrc, vUv + uDir * float(i) * 1.5).rgb * w[i];
        c += texture(uSrc, vUv - uDir * float(i) * 1.5).rgb * w[i];
    }
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- motes over the board, the gauge
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
void main() {
    int mode = int(vA.w + 0.5);
    float r = length(vLocal);
    vec3 c = vB.rgb;
    if (mode == 1) {
        // the gauge: a fine ring of threads, the charge a glowing arc from the top, clockwise
        float px = 1.0 / max(vA.z * uPxPerUnit, 1e-3);
        float ring = exp(-sq((r - 0.8) / max(px * 1.2, 0.012)));
        float ang = atan(vLocal.x, vLocal.y);
        if (ang < 0.0) ang += 6.2831853;
        float on = step(ang, vC.x * 6.2831853);
        float arc = exp(-sq((r - 0.8) / max(px * 2.5, 0.03))) * on;
        float halo = exp(-sq((r - 0.8) / 0.12)) * on * 0.35 * (0.6 + 0.4 * vC.y);
        float v = ring * 0.35 + arc + halo;
        frag = vec4(c * v * vB.a, 0.0);
        return;
    }
    float v = exp(-r * r * 14.0) + 0.3 * exp(-r * r * 3.5);
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(c * v * vB.a, 0.0);
}
)";

// ---------------------------------------------------------------- the veil: the well and the HUD's columns
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
    float a = uWellA * (1.0 - smoothstep(-0.15, 0.9, dw));
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.12;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: mycelium felt
// Each piece one shape (its cells merge): a dense felt of glowing threads in the piece's color, woven in three
// directions over a deep translucent body, a bright inner rim. The ghost is the rim alone; a cleared cell frays into
// light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, -, drain
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
flat out vec4 vA;
flat out vec4 vB;
flat out vec4 vC;
void main() {
    float ext = aA.z + 0.06;
    vec2 wp = aA.xy + aPos * ext;
    vLocal = aPos * ext / (2.0 * aA.z);
    vWorld = wp / (2.0 * aA.z);
    vA = aA; vB = aB; vC = aC;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* CELL_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
flat in vec4 vA;
flat in vec4 vB;
flat in vec4 vC;
out vec4 frag;
uniform float uTime;
uniform vec3 uLightC;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.2, rc = 0.07;
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
// fine threads along one direction, wandering
float fibers(vec2 p, float ang, float freq, float seed) {
    vec2 d = vec2(cos(ang), sin(ang));
    vec2 n = vec2(-d.y, d.x);
    float u = dot(p, n) * freq + vnoise(vec2(dot(p, d) * 2.2, seed)) * 2.2 + vnoise(p * 3.0 + seed) * 0.8;
    return smoothstep(0.75, 1.0, abs(sin(u * 3.14159)));
}

void main() {
    int mode = int(vC.x + 0.5);
    float aa = max(length(fwidth(vLocal)) * 0.7071, 1e-4);
    int mk = int(vA.w + 0.5);
    {
        bool hx = vLocal.x < 0.0 ? has(mk, 0) : has(mk, 1);
        bool vy = vLocal.y < 0.0 ? has(mk, 2) : has(mk, 3);
        if ((hx && abs(vLocal.x) > 0.5) || (vy && abs(vLocal.y) > 0.5)) discard;
    }
    float sd = shapeSdf(vLocal, mk, 0.05);
    float inside = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, sd);
    float e = -sd;
    vec3 tint = vB.rgb;
    float rimW = max(aa * 1.6, 0.04);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.55);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.07) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // a deep body, lighter toward the edges, the felt of threads glowing over it
    vec3 col = tint * mix(0.85, 0.5, smoothstep(0.02, 0.36, e));
    vec2 p = vWorld;
    float f = fibers(p, 0.35, 5.0, 1.0) * 0.5 + fibers(p, 2.1, 6.0, 7.0) * 0.4 + fibers(p, 1.2, 9.0, 3.0) * 0.3;
    float fade = smoothstep(0.03, 0.12, e);
    col += mix(tint, uLightC, 0.4) * f * 0.32 * fade;
    // knots of light where threads meet, breathing slowly
    vec2 cell = floor(p * 2.0);
    vec2 o = (cell + 0.25 + 0.5 * hash2(cell + 1.7)) / 2.0;
    float knot = exp(-dot(p - o, p - o) / 0.0035) * step(hash(cell + 4.2), 0.45);
    col += hi * knot * (0.25 + 0.1 * sin(uTime * 1.1 + hash(cell) * 30.0)) * fade;
    col += tint * exp(-abs(e - 0.1) / 0.03) * 0.18;
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        float n = vnoise(vWorld * 4.6) * 0.65 + vnoise(vWorld * 10.2) * 0.35;
        keep = smoothstep(erase * 1.4 - 0.45, erase * 1.4 - 0.05, n) * (1.0 - smoothstep(0.55, 1.0, erase));
        col = mix(col, hi * 1.35, smoothstep(0.0, 0.4, erase));
    }
    col = mix(col, uBridgeInk, uBridge);
    float a = inside * 0.97 * vB.a * keep;
    frag = vec4(col * a, a);
}
)";

// ---------------------------------------------------------------- lettering
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
uniform float uAlpha, uHaloA, uGlowA;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uHaloA;
    float glow = smoothstep(0.15, 0.47, d) * uGlowA;
    float a = max(fill, halo);
    vec3 c = uInk * fill + uHalo * (a - fill) + uInk * glow * (1.0 - fill) * 0.35;
    frag = vec4(c, a) * uAlpha;
}
)";

// ---------------------------------------------------------------- post
inline const char* POST_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uFade, uDim;
void main() {
    vec3 c = texture(uScene, vUv).rgb;
    c *= (1.0 - uDim * 0.5) * uFade;
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.25 * dot(q, q) * 2.0;
    c = c / (1.0 + max(c - 0.75, 0.0) * 0.7);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace mycelium_shaders

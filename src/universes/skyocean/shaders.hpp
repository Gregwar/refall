#pragma once
// CELESTIAL OCEAN's shaders: the night sky and the sea of clouds (half resolution), the stars (full resolution),
// the creatures of light, the motes, the veil behind the well, the moonstone pieces, the lettering.

namespace skyocean_shaders {

// Shared noise (prepended after the #version line).
inline const char* COMMON_GLSL = R"(
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
)";

// ---------------------------------------------------------------- the sky and the sea of clouds
// Half resolution. Above the horizon: the night sky (a deep gradient, a hazy glow along the horizon, the Milky Way,
// the aurora's curtains in AURORA), the moon with its halo and a faint corona, thin wisps drifting across it. Below:
// the sea of clouds in perspective, as rows of cumulus tops (rounded domes, smaller ones riding on them) from the
// horizon to under the viewer, each row hiding the farther ones; their tops catch a silver lining from the moon
// (brighter on the side facing it and in a broad column of moonlight under it), their valleys sink into blue shade
// and the far rows fade into the haze. The rows drift sideways (the near ones faster). The creatures light the
// tops beneath them. Alpha: how much of the sky shows (where the stars may shine).
inline const char* SKY_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec4 uView;     // center x y, half extent x y (board units)
uniform float uHorizon; // board y of the horizon
uniform vec3 uMoon;     // x, y, radius
uniform vec3 uSkyTop, uSkyMid, uHorizonC, uHaze, uMoonC, uMoonGlow, uCloudLit, uCloudMid, uCloudShadow;
uniform vec3 uAur1, uAur2;
uniform float uAurora, uDrift, uTime, uGlow, uPx, uSeed, uMoonLight;
uniform vec4 uLights[10]; // x, y, radius, intensity
uniform vec3 uLightCol[10];
uniform int uLightN;
uniform float uBridge;
uniform vec3 uBridgeCol;

// The cumulus tops along a row: rounded domes of random width and height; returns the height and where across
// its dome the point is (-1..1).
vec2 domes(float s, float seed) {
    float i = floor(s);
    float best = 0.0, across = 0.0;
    for (int k = -1; k <= 1; k++) {
        float c = i + float(k);
        float cx = c + 0.5 + (hash1(c * 2.11 + seed + 3.0) - 0.5) * 0.5;
        float wd = 0.62 + 0.55 * hash1(c * 3.7 + seed + 7.0);
        float ht = 0.4 + 0.6 * hash1(c * 1.37 + seed);
        float q = (s - cx) / wd;
        float e = max(0.0, 1.0 - q * q);
        float y = ht * e * (1.0 + 0.6 * e) / 1.6;   // round on top, no vertical sides
        if (y > best) { best = y; across = q; }
    }
    return vec2(best, across);
}

void main() {
    vec2 w = uView.xy + (vUv * 2.0 - 1.0) * uView.zw; // board units
    float top = uView.y + uView.w;
    float R = uMoon.z;
    vec2 mq = w - uMoon.xy;
    float dm = length(mq);

    // ---- sky
    float t = clamp((w.y - uHorizon) / (top - uHorizon), 0.0, 1.0);
    vec3 sky = mix(uHorizonC, uSkyMid, smoothstep(0.0, 0.42, t));
    sky = mix(sky, uSkyTop, smoothstep(0.38, 1.0, t));
    sky += uHaze * exp(-max(w.y - uHorizon, 0.0) * 0.8) * (0.22 + 0.18 * uGlow);
    float starA = smoothstep(uHorizon + 0.2, uHorizon + 3.0, w.y);
    // the Milky Way, a faint diagonal band
    {
        vec2 n = normalize(vec2(0.5, 1.0));
        float bd = dot(w - vec2(3.0, 7.0), n);
        float mw = exp(-bd * bd / 10.0) * (0.3 + 0.7 * fbm(w * 0.2 + 5.0 + uSeed)) * smoothstep(uHorizon, uHorizon + 5.0, w.y);
        sky += mix(uHaze, uMoonGlow, 0.4) * mw * 0.07;
    }
    // aurora: two thin curtains folding along the sky, bright along their lower hem, fading upward in vertical rays
    if (uAurora > 0.001) {
        for (int k = 0; k < 2; k++) {
            float fk = float(k);
            float fold = fbm(vec2(w.x * 0.04 + uTime * 0.01 + fk * 3.7, 1.7 + uSeed + fk));
            float yb = uHorizon + 4.0 + fk * 2.5 + 2.4 * sin(w.x * (0.07 + 0.03 * fk) + fold * 5.0 + uTime * 0.02);
            float above = w.y - yb;
            float hem = exp(-sq(above / 0.35)) * 0.5;
            float curtain = smoothstep(-0.25, 0.15, above) * exp(-max(above, 0.0) / (2.0 + fk));
            float rays = vnoise(vec2(w.x * 2.2 + fold * 14.0, uTime * 0.25 + fk));
            rays = 0.25 + 0.75 * rays * rays;
            float seg = smoothstep(0.3, 0.65, vnoise(vec2(w.x * 0.09 - uTime * 0.03, fk * 5.0)));   // broken into lengths
            vec3 ac = mix(uAur1, uAur2, smoothstep(0.0, 4.0, above));
            float lightA = (curtain * rays + hem) * seg * (k == 0 ? 1.0 : 0.6);
            sky += ac * lightA * uAurora * (0.4 + 0.2 * uGlow);
            starA *= 1.0 - 0.4 * clamp(lightA, 0.0, 1.0) * uAurora;
        }
    }
    // the moon: its halo, a faint corona, the disc
    sky += uMoonGlow * (exp(-max(dm - R, 0.0) / (R * 0.45)) * 0.32 + exp(-dm / (R * 3.5)) * 0.2 + exp(-dm / (R * 11.0)) * 0.08)
           * uMoonLight * (1.0 + 0.25 * uGlow);
    sky += uMoonGlow * exp(-sq((dm - R * 2.3) / (R * 0.28))) * 0.025 * uMoonLight;
    {
        float disc = clamp((R - dm) * uPx + 0.5, 0.0, 1.0);
        vec2 mu = mq / R;
        float limb = sqrt(max(0.0, 1.0 - dot(mu, mu)));
        float maria = fbm(mu * 1.9 + 3.0 + uSeed * 0.1);
        vec3 moon = uMoonC * (0.78 + 0.22 * limb) * (1.0 - 0.2 * smoothstep(0.42, 0.68, maria)) * 1.5;
        sky = mix(sky, moon, disc);
        starA *= (1.0 - disc) * (1.0 - 0.92 * exp(-dm / (R * 2.5)));
    }
    // thin wisps drifting across the moon's height
    {
        float wy = w.y - (uMoon.y - 0.4);
        float band = exp(-wy * wy / 7.0);
        float n = fbm(vec2(w.x * 0.045 + uDrift * 0.3, w.y * 0.42 + 2.0));
        float wisp = smoothstep(0.52, 0.8, n) * band;
        vec3 wc = mix(uHaze * 0.35 + uSkyMid * 0.3, uMoonGlow * 1.1, exp(-dm / (R * 4.0)));
        sky = mix(sky, wc, wisp * 0.42);
        starA *= 1.0 - wisp * 0.75;
    }

    // ---- the sea of clouds: rows of cumulus tops, near to far
    vec3 acc = vec3(0.0);
    float accA = 0.0;
    const int NL = 18;
    const float Z_NEAR = 1.0, Z_FAR = 34.0;
    const float PERSP = 13.0, AMP = 4.2;
    float side0 = sign(uMoon.x - w.x);
    vec3 lightSum = vec3(0.0);
    if (w.y < uHorizon + 0.5) {
        for (int i = 0; i < 10; i++) {
            if (i >= uLightN) break;
            vec4 L = uLights[i];
            vec2 q = (w - L.xy) * vec2(1.0, 1.7);
            lightSum += uLightCol[i] * (L.w * exp(-dot(q, q) / (L.z * L.z)));
        }
    }
    for (int k = NL - 1; k >= 0; k--) {
        if (w.y > uHorizon + 0.3) break;
        float f = float(k) / float(NL - 1);          // 0 far .. 1 near
        float z = Z_FAR * pow(Z_NEAR / Z_FAR, f);
        float base = uHorizon - PERSP / z;
        float amp = AMP / z * (1.0 + 0.04 * sin(uTime * 0.21 + float(k) * 1.7));
        if (w.y > base + amp * 1.4) continue;
        float s = w.x * z * 0.085 + uDrift + float(k) * 7.31 + uSeed;
        vec2 d1 = domes(s, float(k) * 3.1);
        float fine = smoothstep(0.15, 0.45, f);                    // the far rows keep only their big domes
        vec2 d2 = fine > 0.0 ? domes(s * 2.6 + 1.3, float(k) * 3.1 + 17.0) : vec2(0.0);
        vec2 d0 = domes(s * 0.33 + 4.1, float(k) * 3.1 + 41.0);   // great heaps of cumulus
        float topY = base + amp * (0.55 * d1.x + 0.26 * d2.x * fine + 0.55 * d0.x * d0.x);
        float soft = max(1.6 / uPx, amp * 0.06);
        float cov = smoothstep(-soft, soft, topY - w.y);
        if (cov <= 0.0) continue;
        float below = max(topY - w.y, 0.0) / max(amp, 1e-3); // 0 at the top edge .. 1 a dome's height down
        // the side of each puff facing the moon catches more light, only near its top (no seams down the row)
        float faceMoon = clamp(d1.y * side0, -1.0, 1.0) * exp(-below * 10.0);
        float rim = exp(-below * 6.0) + 0.3 * exp(-below * 1.8);
        float shade = smoothstep(0.0, 1.5, below);
        vec3 c = mix(uCloudMid, mix(uCloudShadow, uCloudMid, 0.2), shade);
        float colW = 2.5 + 7.0 * f;                       // the column of moonlight, broader near the viewer
        float mc = exp(-sq((w.x - uMoon.x) / colW)) * uMoonLight;
        // patches of brighter and dimmer moonlight across the sea
        float patch = 0.65 + 0.7 * vnoise(vec2(s * 0.12 + 3.0, float(k) * 0.37));
        float lit = rim * (0.35 + 0.65 * (0.5 + 0.5 * faceMoon)) * (0.4 + 0.85 * mc) * patch;
        c += uCloudLit * lit * 0.6;
        c += uMoonGlow * mc * 0.07 * (1.0 - shade);
        // lumps inside the billows
        float lump = vnoise(vec2(s * 3.2, w.y * z * 0.5 + float(k)));
        c *= 0.88 + 0.24 * lump * (1.0 - 0.5 * shade);
        // lit from above by the creatures
        c += lightSum * (0.25 + 0.9 * rim) * (1.0 - 0.6 * shade);
        // distance haze
        float fog = pow(max(1.0 - f, 0.0), 1.5) * 0.85;
        c = mix(c, uHaze * 0.55 + uHorizonC * 0.45 + uMoonGlow * mc * 0.1, fog);
        acc += (1.0 - accA) * cov * c;
        accA += (1.0 - accA) * cov;
        if (accA > 0.995) break;
    }
    vec3 col = acc + (1.0 - accA) * sky;
    // a slight fall-off toward the corners
    vec2 e = (vUv - 0.5) * 2.0;
    col *= 1.0 - 0.16 * dot(e * vec2(0.7, 1.0), e * vec2(0.7, 1.0)) * 0.5;
    col = mix(col, uBridgeCol, uBridge);
    frag = vec4(col, starA * (1.0 - accA) * (1.0 - uBridge));
}
)";

// ---------------------------------------------------------------- the stars (full resolution) over the sky
// Two fields of stars hashed per screen cell (fine ones, and fewer bright ones with a faint cross of light), each
// twinkling at its own pace; shown where the sky shows (the sky's alpha).
inline const char* COMP_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uSky;
uniform vec2 uRes;
uniform float uTwinkle, uStarA, uTime, uSeed;
void main() {
    vec4 s = texture(uSky, vUv);
    vec3 c = s.rgb;
    float vis = s.a * uStarA;
    if (vis > 0.002) {
        vec2 px = gl_FragCoord.xy;
        float sc = max(uRes.y / 1080.0, 0.5);
        vec3 st = vec3(0.0);
        for (int L = 0; L < 2; L++) {
            float cell = (L == 0 ? 8.0 : 26.0) * sc;
            vec2 g = floor(px / cell) + uSeed * (L == 0 ? 1.0 : 2.0);
            float r = hash(g + float(L) * 31.0);
            if (r < (L == 0 ? 0.32 : 0.42)) {
                vec2 o = (floor(px / cell) + 0.2 + 0.6 * hash2(g + 3.1 + float(L))) * cell;
                vec2 dd = px - o;
                float b = pow(hash(g + 7.7), L == 0 ? 2.5 : 1.6);
                float rad = ((L == 0 ? 0.55 : 0.85) + 0.55 * b) * sc;
                float tw = 1.0 + uTwinkle * sin(uTime * (1.3 + 3.2 * hash(g + 1.3)) + r * 80.0);
                float core = exp(-dot(dd, dd) / (rad * rad));
                vec3 tint = mix(vec3(0.72, 0.84, 1.0), vec3(1.0, 0.9, 0.76), hash(g + 9.1));
                float crossL = 0.0;
                if (L == 1) {
                    float ln = (2.0 + 6.0 * b) * sc;
                    crossL = exp(-abs(dd.y) / (0.5 * sc)) * exp(-abs(dd.x) / ln) + exp(-abs(dd.x) / (0.5 * sc)) * exp(-abs(dd.y) / ln);
                    crossL *= 0.3 * b;
                }
                st += tint * (core + crossL) * b * max(tw, 0.0) * (L == 0 ? 0.55 : 1.25);
            }
        }
        c += st * vis;
    }
    frag = vec4(c, 1.0);
}
)";

// ---------------------------------------------------------------- creatures of light
// One instance per creature (or per fish of a school): a body drawn from a signed distance in its own frame (the
// body one unit long, heading +x), its motion from its phase (a whale's tail beats and its long fins sway, a manta
// flaps its wings, a jellyfish's bell pulses and its tentacles trail, a fish wags its tail). Translucent: a faint
// body, a bright membrane along its outline, a soft halo, and its own lights (the whale's rows of spots, its
// throat's grooves, the manta's fin rays, the jellyfish's four rings). Under `surface` it is still in the clouds:
// it breaks through a misty edge.
inline const char* BODY_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, size (body length, board units), heading
layout(location = 2) in vec4 aB; // rgb, alpha
layout(location = 3) in vec4 aC; // kind, phase, seed, surface (board y)
layout(location = 4) in vec4 aD; // half extent x y (body units), flip, glow
uniform mat4 uVP;
out vec2 vLocal;
out vec2 vWorld;
out vec2 vQuad;
flat out vec4 vB;
flat out vec4 vC;
flat out vec4 vD;
flat out float vSize;
void main() {
    vQuad = aPos;
    int kind = int(aC.x + 0.5);
    float ofs = (kind == 1 || kind == 2) ? -0.18 : 0.0;
    vec2 l = vec2(ofs, 0.0) + aPos * aD.xy;
    float c = cos(aA.w), s = sin(aA.w);
    vec2 q = vec2(l.x, l.y * aD.z) * aA.z;
    vec2 wp = aA.xy + vec2(c * q.x - s * q.y, s * q.x + c * q.y);
    vLocal = l;
    vWorld = wp;
    vB = aB; vC = aC; vD = aD;
    vSize = aA.z;
    gl_Position = uVP * vec4(wp, 0.0, 1.0);
}
)";
inline const char* BODY_FS = R"(#version 330 core
in vec2 vLocal;
in vec2 vWorld;
in vec2 vQuad;
flat in vec4 vB;
flat in vec4 vC;
flat in vec4 vD;
flat in float vSize;
out vec4 frag;
uniform float uTime, uPxPerUnit;
uniform vec3 uTint2;

float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }
float seg(vec2 p, vec2 a, vec2 b, float ra, float rb) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - mix(ra, rb, h);
}
float ellipse(vec2 p, vec2 c, vec2 r, float ang) {
    vec2 q = p - c;
    float cs = cos(ang), sn = sin(ang);
    q = vec2(cs * q.x + sn * q.y, -sn * q.x + cs * q.y);
    float k = length(q / r);
    return (k - 1.0) * min(r.x, r.y);
}

// ---- a whale, side view (a humpback: a blunt head, grooves under the throat, long fins, wide flukes)
float whale(vec2 p, float ph, out float lights) {
    float u = 0.5 - p.x;                     // 0 head .. 1 tail
    float uc = clamp(u, 0.0, 1.0);
    float beat = sin(ph);
    float bend = 0.07 * beat * uc * uc * uc + 0.015 * sin(ph - uc * 2.2) * uc;
    float y = p.y - bend;
    float r = 0.112 * pow(smoothstep(-0.045, 0.17, u), 0.42) * (1.0 - 0.86 * smoothstep(0.3, 0.9, u)) + 0.004;
    float rr = y > 0.0 ? r * 0.8 : r * 1.14;
    float d = max(abs(y) - rr, max(-u - 0.04, u - 0.94));
    float dBody = d;
    // flukes: two lobes spreading from the tail stock, tilted with the beat
    vec2 T = vec2(0.5 - 0.92, 0.07 * beat * 0.78 + 0.015 * sin(ph - 2.0) * 0.92);
    float tilt = 0.55 * cos(ph);
    float spread = 0.55 + 0.4 * abs(cos(ph * 0.5 + 0.8));
    float l1 = ellipse(p, T + vec2(-0.055, 0.04 * spread), vec2(0.07, 0.012), -0.8 * spread + tilt * 0.4);
    float l2 = ellipse(p, T + vec2(-0.055, -0.04 * spread), vec2(0.07, 0.012), 0.8 * spread + tilt * 0.4);
    d = smin(d, min(l1, l2), 0.012);
    // the long pectoral fin, swaying slowly
    float fa = -2.35 + 0.22 * sin(ph * 0.5 + 1.0);
    vec2 root = vec2(0.5 - 0.27, -r * 0.0 + 0.0);
    root.y = -0.075 + 0.07 * beat * pow(0.27, 3.0);
    vec2 tip = root + 0.27 * vec2(cos(fa), sin(fa));
    d = smin(d, seg(p, root, tip, 0.02, 0.004), 0.015);
    // a small dorsal fin
    d = smin(d, seg(p, vec2(0.5 - 0.6, bend + 0.05), vec2(0.5 - 0.66, bend + 0.072), 0.012, 0.002), 0.01);
    // its lights: two rows of spots pulsing along the flank, the grooves under the throat, the eye
    lights = 0.0;
    if (d < 0.0) {
        float yn = y / rr;
        for (int row = 0; row < 2; row++) {
            float ry = row == 0 ? 0.25 : -0.4;
            float sp = row == 0 ? 0.034 : 0.026;
            float cu = (uc - 0.08) / sp;
            float id = floor(cu);
            vec2 q = vec2((fract(cu) - 0.5) * sp, (yn - ry) * rr);
            float dotR = (row == 0 ? 0.0055 : 0.0042) * (1.0 - 0.6 * smoothstep(0.5, 0.85, uc));
            float pulse = 0.45 + 0.55 * sq(0.5 + 0.5 * sin(id * 0.7 - uTime * 2.2 + float(row)));
            if (uc > 0.08 && uc < 0.86) lights += exp(-dot(q, q) / (dotR * dotR)) * pulse;
        }
        if (y < 0.0 && uc > 0.03 && uc < 0.42 && dBody < 0.0) {
            float g = abs(sin(yn * 3.14159 * 5.5));
            lights += smoothstep(0.9, 1.0, g) * 0.3 * smoothstep(0.03, 0.1, uc) * (1.0 - smoothstep(0.3, 0.42, uc));
        }
        vec2 eq = vec2(uc - 0.11, yn + 0.22) * vec2(1.0, rr);
        lights += exp(-dot(eq, eq) / (0.006 * 0.006)) * 1.5;
    }
    return d;
}

// ---- a manta ray seen from above, flapping
float manta(vec2 p, float ph, out float lights) {
    float fl = sin(ph);
    float s = 0.55 * (0.8 + 0.2 * cos(ph));
    float a = abs(p.y) / s;
    float x = p.x + 0.07 * fl * a * a + 0.16 * min(a * a * a, 1.0);   // the tips flap and sweep back
    float am = min(a, 1.0);
    float lead = 0.22 * (1.0 - pow(am, 1.3)) * (1.0 - 0.3 * am) - 0.1 * am;
    float trail = -0.2 + 0.25 * pow(am, 1.5) - 0.13 * am * am * am * am;
    float dW = max(max(x - lead, trail - x), (a - 1.0) * s * 0.5);
    dW *= 0.8;
    float dB = ellipse(p, vec2(-0.0, 0.0), vec2(0.22, 0.085), 0.0);
    float d = smin(dW, dB, 0.03);
    // cephalic fins and the tail
    d = min(d, seg(p, vec2(0.12, 0.05), vec2(0.21, 0.04 + 0.01 * fl), 0.014, 0.008));
    d = min(d, seg(p, vec2(0.12, -0.05), vec2(0.21, -0.04 - 0.01 * fl), 0.014, 0.008));
    vec2 tq = p - vec2(-0.16, 0.0);
    float tw = 0.04 * sin(ph * 0.7 + p.x * 8.0);
    d = min(d, seg(p, vec2(-0.16, 0.0), vec2(-0.62, tw), 0.008, 0.002));
    lights = 0.0;
    if (d < 0.0) {
        // fin rays fanning out from the body, spots along the leading edge, gill slits
        // its back glows along the body's ridge, fading out toward the wingtips
        float rays = exp(-sq(p.y / 0.06)) * 0.35 + (1.0 - smoothstep(0.0, 0.9, a)) * 0.12;
        float ls = abs(x - (lead - 0.035));
        float cid = floor(a * 12.0);
        float spots = exp(-sq(ls / 0.012)) * exp(-sq((fract(a * 12.0) - 0.5) / 0.18)) * step(0.15, a) * step(a, 0.92);
        spots *= 0.5 + 0.5 * sin(cid * 1.3 - uTime * 2.5);
        float gills = smoothstep(0.85, 1.0, abs(sin((p.x + 0.02) * 120.0))) * step(abs(abs(p.y) - 0.05), 0.012) * step(abs(p.x - 0.0), 0.05);
        // the pale chevrons on its shoulders
        vec2 cq = vec2(p.x - 0.06 + abs(p.y) * 0.5, abs(p.y) - 0.14);
        float chev = exp(-sq(cq.x / 0.03) - sq(cq.y / 0.07)) * 0.45;
        lights = rays + spots * 1.2 + gills * 0.5 + chev;
        float eye = exp(-dot(p - vec2(0.15, 0.065), p - vec2(0.15, 0.065)) / 0.00005) + exp(-dot(p - vec2(0.15, -0.065), p - vec2(0.15, -0.065)) / 0.00005);
        lights += eye;
    }
    return d;
}

// ---- a jellyfish, bell up (+x), pulsing, its tentacles trailing
float jelly(vec2 p, float ph, out float lights) {
    float pulse = 0.5 + 0.5 * sin(ph);
    float bw = 0.19 * (1.0 - 0.15 * pulse), bh = 0.15 * (1.0 + 0.12 * pulse);
    vec2 q = vec2(p.x - 0.06, p.y);
    float dome = ellipse(q, vec2(0.0), vec2(bh, bw), 0.0);
    float margin = -q.x - 0.012 + 0.008 * sin(q.y * 70.0);
    float dBell = max(dome, margin);
    float d = dBell;
    float tent = 1e3;
    for (int i = 0; i < 7; i++) {
        float fi = float(i) / 6.0;
        float y0 = bw * mix(-0.85, 0.85, fi);
        float L = 0.48 + 0.18 * hash1(float(i) * 3.1);
        float t = clamp(-q.x / L, 0.0, 1.0);
        float wave = 0.045 * sin(-q.x * 13.0 - ph * 1.0 + float(i) * 1.7) * t;
        float dt = abs(q.y - y0 * (1.0 - 0.35 * t) - wave) - 0.0045 * (1.0 - 0.7 * t);
        dt = max(dt, max(q.x - 0.0, -q.x - L));
        tent = min(tent, dt);
    }
    // two frilly oral arms
    for (int j = 0; j < 2; j++) {
        float L = 0.32;
        float t = clamp(-q.x / L, 0.0, 1.0);
        float yj = (j == 0 ? -0.025 : 0.025) + 0.03 * sin(-q.x * 9.0 - ph * 0.8 + float(j) * 2.0) * t;
        float wd = 0.018 * (1.0 - t) + 0.006 + 0.006 * sin(-q.x * 60.0 + float(j));
        float da = max(abs(q.y - yj) - wd, max(q.x - 0.02, -q.x - L));
        tent = min(tent, da);
    }
    d = min(d, tent);
    lights = 0.0;
    if (dBell < 0.0) {
        // the four rings seen through the bell, and its glowing margin
        for (int k = 0; k < 4; k++) {
            float ang = 0.785 + 1.5708 * float(k);
            vec2 c = vec2(0.035, 0.0) + vec2(cos(ang) * 0.035, sin(ang) * 0.075 * (bw / 0.19));
            float rr = length((q - c) * vec2(1.3, 1.0));
            lights += exp(-sq((rr - 0.026) / 0.006)) * 0.8;
        }
        lights += exp(-sq((q.x - 0.0) / 0.012)) * 0.6;
    }
    return d;
}

// ---- a small fish, side view
float fish(vec2 p, float ph, out float lights) {
    float wag = sin(ph);
    float y = p.y - 0.08 * wag * sq(max(-p.x + 0.05, 0.0)) * 3.0;
    float d = ellipse(vec2(p.x, y), vec2(0.05, 0.0), vec2(0.29, 0.115), 0.0);
    float tx = -0.24 - p.x;
    float tailH = 0.02 + max(tx, 0.0) * 0.75;
    float dt = max(abs(y + 0.0) - tailH, max(-tx, tx - 0.2));
    dt = max(dt, -(0.06 - abs(y) + (tx - 0.2) * 0.9) * 0.5);
    d = smin(d, dt, 0.02);
    lights = 0.0;
    if (d < 0.0) {
        lights = exp(-sq((y + 0.01) / 0.012)) * smoothstep(-0.25, 0.0, p.x) * 0.6;
        lights += exp(-dot(p - vec2(0.27, 0.02), p - vec2(0.27, 0.02)) / 0.0005);
    }
    return d;
}

void main() {
    int kind = int(vC.x + 0.5);
    float ph = vC.y;
    vec2 p = vLocal;
    float lights = 0.0;
    float d;
    if (kind == 0) d = fish(p, ph, lights);
    else if (kind == 1) d = jelly(p, ph, lights);
    else if (kind == 2) d = manta(p, ph, lights);
    else d = whale(p, ph, lights);
    float px = 1.0 / max(vSize * uPxPerUnit, 1e-3);   // body units per pixel
    float aa = px;
    float inside = clamp(0.5 - d / aa, 0.0, 1.0);
    float glow = vD.w;
    // the membrane: a fine bright line along the outline (at least a pixel and a half)
    float mw = max(1.3 * px, 0.0025);
    float membrane = exp(-abs(d) / mw);
    // a halo of light around it: a few board units, whatever its size, fading out before the quad's edge
    float hw = min(0.06, 0.5 / vSize);
    float edge = 1.0 - smoothstep(0.7, 1.0, max(abs(vQuad.x), abs(vQuad.y)));
    float halo = (exp(-max(d, 0.0) / hw) * 0.26 + exp(-max(d, 0.0) / (hw * 3.0)) * 0.07) * edge;
    float depthIn = clamp(-d / 0.05, 0.0, 1.0);
    // small creatures glow through (a school of fish reads as bodies of light, not outlines)
    float small = 1.0 - smoothstep(0.6, 3.0, vSize);
    float fill = inside * (0.12 + 0.2 * (1.0 - depthIn) + 0.45 * small);
    // its color shifts along the body (the second hue toward the tail), its light drifts in slow waves
    float along = clamp(0.5 - vLocal.x, 0.0, 1.0);
    vec3 col = mix(vB.rgb, uTint2, smoothstep(0.25, 1.0, along) * 0.55);
    float wave = 0.85 + 0.15 * sin(along * 9.0 - uTime * 1.3 + vC.z);
    vec3 core = mix(col, vec3(1.0), 0.55);
    vec3 c = col * (fill * wave + membrane * 0.75 + halo * glow) + core * (lights * inside * 1.1 + membrane * membrane * membrane * 0.3);
    // still in the clouds below its surface: a misty edge it rises through
    float n = vnoise(vWorld * 1.3 + vC.z) * 0.7 + vnoise(vWorld * 3.7) * 0.3;
    float above = vWorld.y - vC.w + (n - 0.5) * 1.2;
    float show = smoothstep(-0.4, 0.6, above);
    float a = vB.a * show;
    frag = vec4(c * a, inside * 0.35 * a);
}
)";

// ---------------------------------------------------------------- motes, sparkles, the gauge
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
        // the gauge: a faint ring, the charge a bright arc from the top, clockwise
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
    if (mode == 2) {
        // a sparkle: a core and four thin rays
        float rays = exp(-abs(vLocal.x) * 40.0) * exp(-abs(vLocal.y) * 3.0) + exp(-abs(vLocal.y) * 40.0) * exp(-abs(vLocal.x) * 3.0);
        v = exp(-r * r * 30.0) + 0.6 * rays + 0.15 * exp(-r * r * 4.0);
    }
    v *= smoothstep(1.0, 0.8, r);
    frag = vec4(c * v * vB.a, 0.0);
}
)";

// ---------------------------------------------------------------- the veil: the well and the HUD's columns
// A deep, calm veil of night behind the well (soft edges, the creatures passing behind it seen dimly) and a softer
// one behind each HUD column; a hairline of moonlight around the well.
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
    // a touch lighter toward the bottom of the well (moonlight from the clouds under it)
    float lift = smoothstep(10.0, -10.0, w.y) * 0.06;
    for (int i = 0; i < 2; i++) {
        vec4 r = i == 0 ? uColL : uColR;
        float dc = box(w, (r.xy + r.zw) * 0.5, (r.zw - r.xy) * 0.5);
        a = max(a, uColA * (1.0 - smoothstep(-1.5, 2.6, dc)));
    }
    float rail = clamp(1.0 - abs(dw) * uPx * 0.9, 0.0, 1.0) * 0.55 + exp(-abs(dw) * 3.0) * 0.12;
    rail *= smoothstep(10.3, 8.0, w.y) * 0.5 + 0.5;
    vec3 c = uVeil * a * (1.0 + lift * 4.0) + uRail * rail * uRailA;
    frag = vec4(c, a);
}
)";

// ---------------------------------------------------------------- pieces: moonstone
// Each piece one shape (its cells merge): translucent glowing stone, deeper in the middle and lit toward its edges,
// a bright inner rim, a fine line of light just inside it, a slow moonlit sheen and a faint pearly shimmer. The
// ghost is the rim alone; a cleared cell dissolves into light.
inline const char* CELL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aA; // x, y, half, mask
layout(location = 2) in vec4 aB; // tint rgb, alpha
layout(location = 3) in vec4 aC; // mode (0 piece, 1 ghost), erase 0..1, shine, drain
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
    vWorld = wp;
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
uniform vec3 uMoonC;
uniform float uBridge;
uniform vec3 uBridgeInk;

bool has(int m, int b) { return ((m >> b) & 1) != 0; }
// Signed distance (cell units) to the outline of the piece the cell belongs to (mask bits: left right down up,
// down-left down-right up-left up-right).
float shapeSdf(vec2 p, int m, float g) {
    const float r = 0.14, rc = 0.06;
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
    float e = -sd;                                  // inward from the outline
    vec3 tint = vB.rgb;
    float rimW = max(aa * 1.6, 0.035);
    float rim = smoothstep(rimW + aa, rimW - aa, e) * inside;
    vec3 hi = mix(tint, vec3(1.0), 0.6);
    if (mode == 1) {
        float a = (rim * 0.85 + inside * 0.07) * vB.a;
        vec3 c = mix(hi, uBridgeInk, uBridge);
        frag = vec4(c * a, a);
        return;
    }
    // translucent stone: lit toward its edges, deeper in the middle
    vec3 col = tint * mix(1.0, 0.5, smoothstep(0.02, 0.36, e));
    col += tint * exp(-abs(e - 0.11) / 0.025) * 0.22;
    // a slow sheen of moonlight drifting across the stack, a faint pearly shimmer
    float sheen = pow(max(0.0, 0.5 + 0.5 * sin((vWorld.x * 0.55 - vWorld.y * 0.8) * 0.8 - uTime * 0.35)), 10.0);
    col += uMoonC * sheen * 0.16 * smoothstep(0.0, 0.25, e);
    col *= 1.0 + 0.07 * sin(vWorld.x * 1.3 + vWorld.y * 0.9 + uTime * 0.25);
    col = mix(col, hi * 1.15, rim);
    float drain = vC.w;
    col = mix(col, vec3(dot(col, vec3(0.3, 0.5, 0.2))) * 0.35, drain);
    float keep = 1.0;
    float erase = vC.y;
    if (erase > 0.0) {
        // dissolving into light: it brightens softly, then thins out in soft patches
        float n = vnoise(vWorld * 2.3) * 0.65 + vnoise(vWorld * 5.1) * 0.35;
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
// Moonlit lettering: the ink with a soft glow of its own around it, and a faint shadow of the night under it.
// The HUD's lettering (text::Lettering, the host letters it over the post): the letters as they were drawn in the
// linear scene, over their halo of the veil (uP0: its color, sRGB, and amount) and in a glow of their ink (uP1.x),
// the field as baked then (64 px, 8 px of reach): the letter and its halo mixed in linear light, the glow added to
// the light under it (uP1.y: about the world's, linear).
inline const char* LETTER_GLSL = R"(
vec4 letter(Letter L) {
    float d = 0.5 + L.dist / L.em * 4.0157;
    float w = max(fwidth(d), 1e-3) * 0.75;
    float fill = smoothstep(0.47 - w, 0.47 + w, d);
    float halo = smoothstep(0.2, 0.45, d) * uP0.a;
    float a = max(fill, halo);
    vec3 ink = pow(L.ink.rgb, vec3(2.2));
    vec3 c = ink * fill + pow(uP0.rgb, vec3(2.2)) * (a - fill);
    vec3 under = c + uP1.y * (1.0 - a);
    vec3 glow = ink * (smoothstep(0.15, 0.47, d) * uP1.x * (1.0 - fill) * 0.35);
    vec3 col = pow(c / max(a, 1e-4), vec3(1.0 / 2.2)) * a + pow(under + glow, vec3(1.0 / 2.2)) - pow(under, vec3(1.0 / 2.2));
    return vec4(col, a) * L.ink.a;
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
    // a soft roll-off of the brightest lights
    c = c / (1.0 + max(c - 0.8, 0.0) * 0.6);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    // dither the deep gradients
    c += (hash(gl_FragCoord.xy + 0.5) - 0.5) / 255.0;
    frag = vec4(c, 1.0);
}
)";

} // namespace skyocean_shaders

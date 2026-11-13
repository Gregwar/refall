#pragma once
// SHADOW's shaders: the wall (the lit circle, the cast shadows: the playfield), the model (ray marched: the little
// well, its gel blocks, the table, the projector, the spent blocks), the beam's haze and dust, the final pass, text.
namespace shadow_shaders {

// Shared by the wall and the model: the board's cells (a 10 x 22 texture: type + 1, the merge mask of the piece's
// neighbours, the lock flash), each game row's height on screen (rows falling after a clear) and, per display slot,
// the game row drawn there. Board space: the well's cells are x in [-5, 5], y in [-10, 10]; the model is built at
// the same scale (one unit per cell), so the shadow is the model, cell for cell.
inline const char* COMMON_GLSL = R"(
uniform sampler2D uCells;
uniform float uRowY[22];
uniform float uSlot[24];
uniform vec4 uPiece[4];   // the falling piece: x, y (board), merge mask, on
uniform float uPieceType;

float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float hash13(vec3 p3) { p3 = fract(p3 * 0.1031); p3 += dot(p3, p3.zyx + 31.32); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x), mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y);
    float b = mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x), mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y);
    return mix(a, b, f.z);
}
float fbm(vec2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 7.1; a *= 0.5; } return s; }

// A cell's shape from its merge mask: two bars, one reaching toward its piece's neighbours left and right, one up
// and down (to their centers), so that a piece is one seamless block and its inner corners stay square.
const float HALF = 0.465, REACH = 1.0;
float sdBox2(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float sdRBox2(vec2 p, vec2 b, float r) { return sdBox2(p, b - r) - r; }
float cellShape(vec2 p, vec2 c, float mask, float r) {
    int m = int(mask + 0.5);
    float px = (m & 1) != 0 ? REACH : HALF, nx = (m & 2) != 0 ? REACH : HALF;
    float py = (m & 4) != 0 ? REACH : HALF, ny = (m & 8) != 0 ? REACH : HALF;
    float a = sdRBox2(p - c - vec2(px - nx, 0.0) * 0.5, vec2((px + nx) * 0.5, HALF), r);
    float b = sdRBox2(p - c - vec2(0.0, py - ny) * 0.5, vec2(HALF, (py + ny) * 0.5), r);
    float d = min(a, b);
    // a 2 x 2 square of the same piece (the O): its middle filled too
    if (m >= 16) {
        for (int k = 0; k < 4; k++) {
            if ((m & (16 << k)) == 0) continue;
            vec2 s = vec2((k & 1) == 0 ? 1.0 : -1.0, (k & 2) == 0 ? 1.0 : -1.0);
            d = min(d, sdRBox2(p - c - s * 0.5, vec2(0.5), r));
        }
    }
    return d;
}
vec4 cellAt(int x, int row) { return texelFetch(uCells, ivec2(x, row), 0); }

// The nearest board block (the stack and the falling piece) to p, in the board plane: distance, type, flash.
// Only the cell under p and its neighbours toward p are looked at (others are farther than ~0.4).
float boardSDF2(vec2 p, float r, out float type, out float flash) {
    type = -1.0; flash = 0.0;
    float d = 1e3;
    float ob = sdBox2(p - vec2(0.0, 1.0), vec2(5.1, 11.6));
    if (ob < 0.6) {
        int cx = clamp(int(floor(p.x + 5.0)), 0, 9);
        float fx = p.x + 4.5 - float(cx);
        int nx = cx + (fx > 0.0 ? 1 : -1);
        int s = clamp(int(floor(12.0 - p.y)), 0, 23);
        int r0 = int(uSlot[s]);
        if (r0 >= 0) {
            int r1 = p.y > uRowY[r0] ? r0 - 1 : r0 + 1;
            for (int k = 0; k < 4; k++) {
                int x = (k & 1) == 0 ? cx : nx;
                int row = (k & 2) == 0 ? r0 : r1;
                if (x < 0 || x > 9 || row < 2 || row > 21) continue;
                vec4 c = cellAt(x, row);
                float t = floor(c.r * 255.0 + 0.5) - 1.0;
                if (t < 0.0) continue;
                float e = cellShape(p, vec2(float(x) - 4.5, uRowY[row]), c.g * 255.0, r);
                if (e < d) { d = e; type = t; flash = c.b; }
            }
            d = min(d, max(ob, 0.0) + 0.42);
        } else d = max(ob, 0.0) + 0.42;
    } else d = ob;
    for (int i = 0; i < 4; i++) {
        if (uPiece[i].w < 0.5) continue;
        float e = cellShape(p, uPiece[i].xy, uPiece[i].z, r);
        if (e < d) { d = e; type = uPieceType; flash = 0.0; }
    }
    return d;
}
)";

// ---------------------------------------------------------------------------------------------------------------
// The wall: plaster lit by the projector's circle, the model's shadow cast on it (the playfield).
inline const char* WALL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform vec3 uLight1, uLight2, uWall, uAmbient;
uniform vec4 uCircle1, uCircle2;   // center, radii
uniform float uDrop, uWallKind, uBare, uTime;
uniform vec3 uGel[7];              // what each piece's glass lets through (linear)
uniform vec4 uGhost[4];            // x, y, mask, on
uniform float uGhostA;
uniform vec4 uPrev[16];            // the previews' cells: x, y, size, type
uniform int uPrevN;
uniform vec4 uClr[40];             // cleared cells shrinking: x, y, scale, type
uniform int uClrN;
uniform vec2 uEchoOff, uEchoScale; // the second projector's shadow: offset, stretch (from the table's line)
uniform vec2 uLens;                // where the beam comes from (board space, on screen)

float lightCircle(vec2 p, vec4 c, float spread) {
    vec2 q = (p - c.xy) / c.zw;
    float r = length(q);
    float core = smoothstep(1.0, 0.955 - spread, r);
    return core * (1.0 - 0.22 * r * r) + 0.035 * exp(-4.0 * max(r - 1.0, 0.0)) * step(0.9, r);
}

// The transmission at p of what stands between the projector and the wall: the model's frame and table (opaque
// wood), its gel blocks (colored light, a darker rim where the glass bends it away, a brighter heart), the ghost
// (a faint outline), the previews, the cleared blocks lifted away. pen: the penumbra's half width.
vec3 transmit(vec2 p, float pen, bool previews) {
    vec3 T = vec3(1.0);
    // the frame: two posts and the base, and the table's edge under it
    if (uBare < 0.5) {
        float fr = min(sdBox2(vec2(abs(p.x) - 5.36, p.y - 0.0), vec2(0.31, 10.3)), sdBox2(p - vec2(0.0, -10.42), vec2(5.67, 0.38)));
        float cov = smoothstep(pen, -pen, fr);
        T *= 1.0 - cov;
    }
    float table = smoothstep(-10.8 + pen * 2.0, -10.8 - pen * 2.0, p.y);
    T *= 1.0 - table;
    // the blocks
    float type, flash;
    float d = boardSDF2(p, 0.07, type, flash);
    if (type >= 0.0 && d < 0.35) {
        // (a soft fringe outside the crisp edge: the lamp's filament is not a point)
        T *= 1.0 - 0.16 * smoothstep(0.35, 0.0, d) * step(pen, d);
    }
    if (type >= 0.0 && d < pen) {
        float cov = smoothstep(pen, -pen, d);
        vec3 g = uGel[int(type)];
        // glass: a dark rim where the light bends away, a bright seam just inside it (the light it focuses),
        // a heart with the glass's slight waviness
        float rim = smoothstep(-0.15, -0.07, d);
        float wav = vnoise(p * 2.7 + type * 3.1) * 0.6 + vnoise(p * 6.3 - type) * 0.4;
        vec3 t = g * mix(1.0, 0.3, rim) * (0.88 + 0.24 * wav) * (1.0 + 0.5 * flash);
        T *= mix(vec3(1.0), t, cov);
    }
    // the ghost: the outline of the landing place
    if (uGhostA > 0.001 && previews) {
        float gd = 1e3;
        for (int i = 0; i < 4; i++) {
            if (uGhost[i].w < 0.5) continue;
            gd = min(gd, cellShape(p, uGhost[i].xy, uGhost[i].z, 0.07));
        }
        float line = smoothstep(0.05, 0.015, abs(gd + 0.03));
        T *= mix(vec3(1.0), uGel[int(uPieceType)] * 0.75, line * uGhostA);
    }
    // the cleared blocks, shrinking away
    for (int i = 0; i < uClrN; i++) {
        float s = uClr[i].z;
        float e = sdRBox2(p - uClr[i].xy, vec2(HALF * s), 0.07 * s);
        if (e < pen) {
            float cov = smoothstep(pen, -pen, e) * smoothstep(0.0, 0.25, s);
            T *= mix(vec3(1.0), uGel[int(uClr[i].w)] * mix(1.0, 0.4, smoothstep(-0.12 * s, -0.04 * s, e)), cov);
        }
    }
    if (previews) {
        for (int i = 0; i < uPrevN; i++) {
            float s = uPrev[i].z;
            float e = sdBox2(p - uPrev[i].xy, vec2(0.5 * s));
            if (e < 0.3) {
                // (the preview's cells touch: one seamless shape, its rim drawn by the union's distance)
                float best = e;
                for (int j = 0; j < uPrevN; j++) {
                    if (uPrev[j].w != uPrev[i].w || abs(uPrev[j].z - s) > 1e-3) continue;
                    best = min(best, sdBox2(p - uPrev[j].xy, vec2(0.5 * s)));
                }
                float cov = smoothstep(pen * 0.7, -pen * 0.7, best);
                float rim = smoothstep(-0.13 * s, -0.05 * s, best);
                T *= mix(vec3(1.0), uGel[int(uPrev[i].w)] * mix(1.0, 0.36, rim), cov);
                break;
            }
        }
    }
    return T;
}

void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    // the wall: plaster (soft, a fine grain), or a woven screen
    float n = fbm(p * 0.45) * 0.6 + fbm(p * 2.3 + 3.0) * 0.4;
    float grain = vnoise(p * 14.0) * 0.5 + vnoise(p * 31.0) * 0.5;
    vec3 alb = uWall * (0.9 + 0.12 * (n - 0.5) * (1.0 - 0.6 * uWallKind) + 0.05 * (grain - 0.5));
    // the projector: a focused circle, slightly brighter toward the lamp's side, a faint colored fringe at its edge
    vec2 toLens = normalize(uLens - uCircle1.xy);
    float side = 1.0 + 0.1 * dot(normalize(p - uCircle1.xy + 1e-4), toLens) * length((p - uCircle1.xy) / uCircle1.zw);
    vec3 c1 = vec3(lightCircle(p, uCircle1 * vec4(1, 1, 1.006, 1.006), 0.0), lightCircle(p, uCircle1, 0.0),
                   lightCircle(p, uCircle1 * vec4(1, 1, 0.992, 0.992), 0.0));
    // the penumbra widens a little with the distance from the model (higher up the wall)
    float pen = 0.045 + 0.03 * clamp((p.y + 10.0) / 20.0, 0.0, 1.0);
    vec3 T1 = transmit(p, pen, true);
    vec3 light = uLight1 * c1 * side * T1;
    if (uDrop > 0.002) {
        // the second projector: its own circle, the model's shadow cast again, offset and stretched
        vec2 base = vec2(0.0, -10.8);
        vec2 q = base + (p - base - uEchoOff) / uEchoScale;
        vec3 T2 = mix(vec3(1.0), transmit(q, pen * 3.5, false), 0.8);
        float c2 = lightCircle(p, uCircle2, 0.05);
        // (inside the main shadows its light is held back: the playfield keeps its colors)
        float keep = 1.0 - 0.85 * smoothstep(0.0, 0.5, 1.0 - dot(T1, vec3(0.333)));
        light += uLight2 * c2 * T2 * uDrop * keep;
    }
    vec3 col = alb * (light + uAmbient * (0.6 + 0.4 * n));
    frag = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------------------------------------------------------
// The model, ray marched in its own corner: board space (one unit per cell), the table top at y = -10.8.
inline const char* MODEL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec3 uEye, uFwd, uRight, uUp;
uniform vec2 uTan;                 // tan(half fov) x aspect, tan(half fov)
uniform vec3 uLensPos, uLensDir;   // the projector
uniform vec3 uLight1, uLight2, uL2Pos;
uniform float uDrop, uBare, uTime;
uniform vec3 uGelC[7];             // the glass blocks' colors (linear)
uniform vec3 uWood, uTable, uBody, uRoom;
uniform float uTableKind;          // 0 wood, 1 drafting board, 2 velvet
uniform vec4 uFly[24];             // blocks in flight: position, type
uniform vec4 uFlyR[24];            // their rotation: axis (xyz), angle
uniform float uFlyS[24];            // their size
uniform int uFlyN;
uniform vec4 uPile[7];             // the spent blocks' stacks: x, z, count, type
uniform vec4 uBasket;              // the box of the oldest ones: x, z, count
uniform float uLensGlow;

const float TABLE_Y = -10.8, PILE_S = 0.44, TABLE_R = 9.4;
const vec2 TABLE_C = vec2(-0.6, 2.2);
const vec2 TRAY_C = vec2(3.95, 5.2), TRAY_H = vec2(3.25, 1.6);
const float BOX_W = 1.0, BOX_H = 1.5;
int gMat; float gType; float gFlash;

float sdBox(vec3 p, vec3 b) { vec3 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0); }
float sdRBox(vec3 p, vec3 b, float r) { return sdBox(p, b - r) - r; }
float sdCyl(vec3 p, float h, float r) { vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h); return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)); }
vec3 rotAxis(vec3 p, vec3 a, float ang) { float c = cos(ang), s = sin(ang); return p * c + cross(a, p) * s + a * dot(a, p) * (1.0 - c); }

// The projector's frame: forward along uLensDir.
vec3 projLocal(vec3 p) {
    vec3 f = uLensDir, s = normalize(cross(f, vec3(0, 1, 0))), u = cross(s, f);
    vec3 q = p - uLensPos;
    return vec3(dot(q, s), dot(q, u), dot(q, f));
}

float mapProjector(vec3 p, out int mat) {
    mat = 3;
    vec3 q = projLocal(p);
    // the barrel: a cylinder behind the lens with cooling fins, a deeper hood around the lens
    vec3 qb = vec3(q.x, q.z + 2.6, q.y);
    float body = sdCyl(qb, 2.0, 1.45) - 0.08;
    float ring = (abs(fract((q.z + 2.6) * 1.6) - 0.5) - 0.33) / 1.6;
    float carve = max(max(ring, 1.33 - length(q.xy)), max(-q.z - 4.2, q.z + 1.4));
    body = max(body, -carve);
    float hood = max(sdCyl(vec3(q.x, q.z + 0.15, q.y), 0.45, 1.25) - 0.04, -(length(q.xy) - 1.02));
    float lens = sdCyl(vec3(q.x, q.z + 0.35, q.y), 0.08, 1.02);
    float back = length(vec3(q.xy, (q.z + 4.6) * 1.6)) - 1.2;
    float d = min(body, back);
    if (hood < d) { d = hood; mat = 4; }
    if (lens < d) { d = lens; mat = 5; }
    // the cradle under the barrel and its stand down to the table
    vec3 bc = uLensPos - uLensDir * 2.6;
    float cradle = sdRBox(p - bc + vec3(0.0, 1.55, 0.0), vec3(0.55, 0.3, 0.55), 0.12);
    float top = bc.y - 1.6, bottom = TABLE_Y - 30.0;
    float post = sdCyl(vec3(p.x - bc.x, p.y - 0.5 * (top + bottom), p.z - bc.z), 0.5 * (top - bottom), 0.24);
    float collar = sdCyl(vec3(p.x - bc.x, p.y - (top - 3.0), p.z - bc.z), 0.25, 0.38) - 0.05;
    float stand = min(min(cradle, post), collar);
    if (stand < d) { d = stand; mat = 3; }
    return d;
}

// A stack of n cubes of side S on an N x N footprint, filled layer by layer (row by row): three boxes, grooved.
float stackSDF(vec3 q, float n, float N, float S) {
    float per = N * N;
    float L = floor(n / per), r = n - L * per, fr = floor(r / N), rr = r - fr * N;
    float H = 0.5 * N * S;
    float e = 1e3;
    if (L > 0.0) e = sdBox(q - vec3(0.0, L * S * 0.5, 0.0), vec3(H, L * S * 0.5, H));
    if (fr > 0.0) e = min(e, sdBox(q - vec3(0.0, (L + 0.5) * S, -H + fr * S * 0.5), vec3(H, 0.5 * S, fr * S * 0.5)));
    if (rr > 0.0) e = min(e, sdBox(q - vec3(-H + rr * S * 0.5, (L + 0.5) * S, -H + (fr + 0.5) * S), vec3(rr * S * 0.5, 0.5 * S, 0.5 * S)));
    vec3 fg = abs(fract((q + vec3(H, 0.0, H)) / S) - 0.5);
    float plane = (0.5 - max(fg.x, max(fg.y, fg.z))) * S;
    return max(e, 0.03 - plane);
}

// The spent blocks: low stacks sorted by color in a tray at the table's front right, out of the beam, and the
// overflow in a wooden box behind the model (on the side away from the lamp).
float mapPiles(vec3 p) {
    float d = 1e3;
    float tray = sdRBox(p - vec3(TRAY_C.x, TABLE_Y + 0.06, TRAY_C.y), vec3(TRAY_H.x, 0.06, TRAY_H.y), 0.04);
    if (tray < d) { d = tray; gMat = 1; }
    if (sdBox(p - vec3(TRAY_C.x, TABLE_Y + 0.6, TRAY_C.y), vec3(TRAY_H.x, 0.6, TRAY_H.y)) < 0.3) {
        for (int i = 0; i < 7; i++) {
            float n = uPile[i].z;
            if (n < 0.5) continue;
            float e = stackSDF(p - vec3(uPile[i].x, TABLE_Y + 0.12, uPile[i].y), n, 3.0, PILE_S);
            if (e < d) { d = e; gMat = 6; gType = uPile[i].w; }
        }
    }
    // the box: thin walls and a floor, filling with the oldest blocks of every color
    vec3 q = p - vec3(uBasket.x, TABLE_Y, uBasket.y);
    float outer = sdRBox(q - vec3(0.0, BOX_H * 0.5, 0.0), vec3(BOX_W, BOX_H * 0.5, BOX_W), 0.05);
    if (outer < d + 0.1) {
        float box = max(outer, -sdBox(q - vec3(0.0, BOX_H * 0.5 + 0.12, 0.0), vec3(BOX_W - 0.12, BOX_H * 0.5, BOX_W - 0.12)));
        if (box < d) { d = box; gMat = 1; }
        if (uBasket.z > 0.5) {
            vec3 qi = q - vec3(0.0, 0.12, 0.0);
            float e = stackSDF(qi, uBasket.z, 4.0, PILE_S);
            if (e < d) {
                d = e; gMat = 6;
                vec3 cell = floor((qi + vec3(2.0 * PILE_S, 0.0, 2.0 * PILE_S)) / PILE_S);
                gType = floor(hash13(cell + 7.0) * 6.999);
            }
        }
    }
    return d;
}
)" R"(
// Everything but the glass blocks.
float mapSolid(vec3 p) {
    gMat = 0; gType = -1.0; gFlash = 0.0;
    // the table: a round top on a pedestal
    vec3 tq = p - vec3(TABLE_C.x, TABLE_Y - 0.7, TABLE_C.y);
    float d = sdCyl(tq, 0.7, TABLE_R - 0.2) - 0.2;
    d = min(d, sdCyl(tq + vec3(0.0, 9.0, 0.0), 8.5, 1.1));
    float dd;
    // the model's frame (wood): two posts, the base
    if (uBare < 0.5) {
        dd = min(sdRBox(vec3(abs(p.x) - 5.36, p.y, p.z), vec3(0.31, 10.3, 0.62), 0.1),
                 sdRBox(p - vec3(0.0, -10.42, 0.0), vec3(5.67, 0.38, 0.9), 0.12));
        if (dd < d) { d = dd; gMat = 1; }
    }
    if (p.y < TABLE_Y + 3.0) {
        int m = gMat; float t = gType;
        dd = mapPiles(p);
        if (dd < d) d = dd; else { gMat = m; gType = t; }
    }
    int pm;
    dd = mapProjector(p, pm);
    if (dd < d) { d = dd; gMat = pm; }
    return d;
}

// The glass blocks: the board's (one seamless block per piece) and those lifted out, flying to the table.
float gGType, gGFlash;
float glassMap(vec3 p) {
    gGType = -1.0; gGFlash = 0.0;
    float d = 1e3;
    float wb = sdBox(p - vec3(0.0, 0.5, 0.0), vec3(5.2, 11.2, 0.6));
    if (wb < 0.5) {
        float type, flash;
        float e2 = boardSDF2(p.xy, 0.09, type, flash);
        if (type >= 0.0) {
            // (inset a little more than the shadow: the gaps between two pieces read on the small model too)
            vec2 w = vec2(e2 + 0.135, abs(p.z) - 0.465 + 0.09);
            d = min(max(w.x, w.y), 0.0) + length(max(w, 0.0)) - 0.09;
            gGType = type; gGFlash = flash;
        } else d = max(e2, abs(p.z) - 0.5);
    } else d = wb;
    for (int i = 0; i < uFlyN; i++) {
        vec3 q = rotAxis(p - uFly[i].xyz, uFlyR[i].xyz, uFlyR[i].w);
        float e = sdRBox(q, vec3(0.42 * uFlyS[i]), 0.08 * uFlyS[i]);
        if (e < d) { d = e; gGType = uFly[i].w; gGFlash = 0.0; }
    }
    return d;
}

float map(vec3 p) {
    float d = mapSolid(p);
    float g = glassMap(p);
    if (g < d) { gMat = 2; gType = gGType; gFlash = gGFlash; return g; }
    return d;
}

vec3 normalSolid(vec3 p) {
    const vec2 e = vec2(0.0025, -0.0025);
    return normalize(e.xyy * mapSolid(p + e.xyy) + e.yyx * mapSolid(p + e.yyx) + e.yxy * mapSolid(p + e.yxy) + e.xxx * mapSolid(p + e.xxx));
}
vec3 normalGlass(vec3 p) {
    const vec2 e = vec2(0.0025, -0.0025);
    return normalize(e.xyy * glassMap(p + e.xyy) + e.yyx * glassMap(p + e.yyx) + e.yxy * glassMap(p + e.yxy) + e.xxx * glassMap(p + e.xxx));
}
vec3 gelTint(float type) {
    vec3 g = uGelC[int(clamp(type, 0.0, 6.0))];
    return g / max(max(g.r, g.g), max(g.b, 1e-3));
}
// The lamp's light reaching ro: soft shadows of the solids; through the glass it comes out colored (the light
// that paints the colored shadows on the wall, and spills on the model's floor and the table).
vec3 lampLight(vec3 ro, vec3 rd, float tmax) {
    float res = 1.0, t = 0.04;
    vec3 tr = vec3(1.0);
    bool through = false;
    for (int i = 0; i < 80 && t < tmax; i++) {
        vec3 p = ro + rd * t;
        float h = mapSolid(p);
        res = min(res, 14.0 * h / t);
        if (res < 0.002) break;
        float g = glassMap(p);
        float s = clamp(min(h, max(abs(g), 0.04)), 0.03, 0.8);
        if (g < 0.0) { tr *= pow(mix(vec3(1.0), gelTint(gGType), 0.92), vec3(s * 1.1)); through = true; }
        t += s;
    }
    return clamp(res, 0.0, 1.0) * tr * (through ? 1.7 : 1.0);
}
float ambientOcc(vec3 p, vec3 n) {
    float o = 0.0, s = 1.0;
    for (int i = 1; i <= 5; i++) {
        float h = 0.06 + 0.16 * float(i);
        o += (h - mapSolid(p + n * h)) * s;
        s *= 0.7;
    }
    return clamp(1.0 - 1.6 * o, 0.0, 1.0);
}

vec3 lampAt(vec3 p, vec3 n, out vec3 L) {
    vec3 Lv = uLensPos - p;
    float dist = length(Lv);
    L = Lv / dist;
    float cone = smoothstep(0.76, 0.9, dot(-L, uLensDir));
    if (cone <= 0.0) return vec3(0.0);
    return uLight1 * cone * lampLight(p + n * 0.03, L, dist - 2.0) * 2.1 / (1.0 + dist * dist * 0.0009);
}

// An opaque surface: the table, the wood, the lamp, the spent blocks.
vec3 shadeSolid(vec3 p, vec3 n, vec3 rd, int mat, float type) {
    vec3 v = -rd;
    vec3 L = vec3(0.0, 1.0, 0.0);
    vec3 lit = mat == 5 ? vec3(0.0) : lampAt(p, n, L);
    float ao = ambientOcc(p, n);
    vec3 bounce = uLight1 * 0.07 * (0.35 + 0.65 * max(-n.z, 0.0)) + uRoom;
    vec3 L2 = normalize(uL2Pos - p);
    vec3 lit2 = uLight2 * uDrop * 0.9;
    float diff = max(dot(n, L), 0.0), diff2 = max(dot(n, L2), 0.0);
    vec3 h = normalize(L + v), h2 = normalize(L2 + v);
    vec3 col;
    if (mat == 6) {
        // the spent blocks: small glass cubes packed together, glowing in the light
        vec3 g = uGelC[int(clamp(type, 0.0, 6.0))];
        float edge = 1.0 - max(abs(n.x), max(abs(n.y), abs(n.z)));
        col = g * lit * (0.25 * diff + 0.45) + g * lit2 * (0.3 + 0.7 * diff2) * 0.6 + g * bounce * 1.4 * ao;
        col += lit * (pow(max(dot(n, h), 0.0), 90.0) * 1.4 + edge * edge * 2.2 * g);
        col *= mix(0.55, 1.0, ao);
    } else if (mat == 1 || mat == 0) {
        vec3 base = mat == 1 ? uWood : uTable;
        float along = mat == 1 ? (abs(p.x) > 5.0 ? p.y : p.x) : p.x;
        float across = mat == 1 ? (abs(p.x) > 5.0 ? p.x + p.z * 0.5 : p.y + p.z * 0.5) : p.z;
        float grain = sin((across * 9.0 + vnoise(vec2(along * 0.35, across * 3.0)) * 5.0) * 3.0);
        float woodK = mat == 1 ? 1.0 : (uTableKind < 0.5 ? 1.0 : 0.25);
        base *= 1.0 + woodK * (0.13 * grain + 0.18 * (vnoise(vec2(along * 0.8, across * 6.0)) - 0.5));
        if (mat == 0 && uTableKind > 1.5) base *= 0.9 + 0.2 * vnoise(p.xz * 18.0);
        col = base * (lit * diff + bounce * ao + lit2 * diff2 * 0.5) * mix(0.35, 1.0, ao);
        float gloss = mat == 1 ? 0.35 : (uTableKind > 1.5 ? 0.0 : 0.18);
        col += gloss * (lit * pow(max(dot(n, h), 0.0), 40.0) + lit2 * pow(max(dot(n, h2), 0.0), 40.0) * 0.5);
        if (mat == 0) col *= mix(0.6, 1.0, smoothstep(0.0, 0.4, ao));
    } else if (mat == 5) {
        vec3 q = projLocal(p);
        float r = length(q.xy) / 1.02;
        col = uLight1 * (3.0 + 6.0 * uLensGlow) * (1.0 - 0.5 * r * r) + uLight1 * 0.5;
    } else {
        vec3 base = mat == 4 ? vec3(0.62, 0.44, 0.2) : uBody;
        float spec = mat == 4 ? 1.0 : 0.5;
        col = base * (lit * diff * 0.5 + bounce * ao * 1.5 + lit2 * diff2 * 0.5) * mix(0.4, 1.0, ao);
        vec3 rl = reflect(rd, n);
        float sky = smoothstep(-0.2, 0.8, rl.y) * 0.06 + smoothstep(0.6, 1.0, dot(rl, normalize(vec3(0.2, 0.3, -1.0)))) * 0.5;
        col += spec * (uLight1 * sky * (mat == 4 ? base : vec3(0.6)) + lit2 * pow(max(dot(n, h2), 0.0), 50.0) * 0.6);
        col += uLight1 * 0.2 * spec * pow(max(dot(rl, normalize(vec3(-0.3, 0.6, 0.7))), 0.0), 30.0);
    }
    return col;
}

// A glass surface: what it reflects (the dark room, the lamp in a sharp highlight), its edges catching the light,
// and the lamp's light scattered inside it, glowing in its color.
vec3 shadeGlass(vec3 p, vec3 n, vec3 rd, float type, float flash, float F) {
    vec3 v = -rd;
    vec3 L = vec3(0.0, 1.0, 0.0);
    vec3 lit = lampAt(p, n, L);
    vec3 g = uGelC[int(clamp(type, 0.0, 6.0))];
    vec3 L2 = normalize(uL2Pos - p);
    vec3 lit2 = uLight2 * uDrop * 0.9;
    float edge = 1.0 - max(abs(n.x), max(abs(n.y), abs(n.z)));
    vec3 h = normalize(L + v), h2 = normalize(L2 + v);
    vec3 col = g * lit * (0.08 + 0.2 * max(dot(-n, L), 0.0)) + g * lit2 * 0.1 + g * uRoom * 1.5;
    col += lit * (pow(max(dot(n, h), 0.0), 120.0) * 2.6 + edge * edge * 4.0 * mix(g, vec3(1.0), 0.35));
    col += lit2 * pow(max(dot(n, h2), 0.0), 80.0) * 1.2;
    vec3 rl = reflect(rd, n);
    col += F * (uRoom * 3.0 + uLight1 * 0.12 * smoothstep(0.2, 1.0, dot(rl, normalize(vec3(-0.2, 0.25, -1.0)))));
    col += g * flash * lit * 0.4;
    return col;
}

void main() {
    vec3 rd = normalize(uFwd + uRight * (vUv.x * 2.0 - 1.0) * uTan.x + uUp * (vUv.y * 2.0 - 1.0) * uTan.y);
    vec3 ro = uEye;
    vec3 acc = vec3(0.0), tr = vec3(1.0);
    bool opaque = false;
    float travelled = 0.0;
    for (int layer = 0; layer < 4; layer++) {
        bool glass = layer < 3;
        float t = 0.0;
        bool hit = false;
        for (int i = 0; i < 160; i++) {
            vec3 q = ro + rd * t;
            float h = glass ? map(q) : mapSolid(q);
            if (h < 0.0015 * (t + travelled)) { hit = true; break; }
            t += h;
            if (t > 160.0) break;
        }
        if (!hit) break;
        vec3 p = ro + rd * t;
        travelled += t;
        if (glass) map(p); else mapSolid(p);
        int mat = gMat; float type = gType, flash = gFlash;
        if (mat == 2) {
            // through the glass: a Fresnel reflection, the tint deepening with the thickness crossed, the view
            // behind shifted by the refraction
            vec3 n = normalGlass(p);
            float F = 0.04 + 0.96 * pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
            acc += tr * shadeGlass(p, n, rd, type, flash, F);
            tr *= 1.0 - F;
            vec3 ri = refract(rd, n, 1.0 / 1.45);
            float s = 0.02;
            for (int k = 0; k < 48; k++) {
                float gd = glassMap(p + ri * s);
                if (gd > 0.0) break;
                float ds = max(-gd, 0.04);
                tr *= pow(mix(vec3(1.0), gelTint(gGType), 0.85), vec3(ds * 0.55));
                // (the lamp's light crossing the glass, scattered toward the eye: it glows in its color)
                acc += tr * gelTint(gGType) * uLight1 * 0.05 * ds;
                s += ds;
            }
            ro = p + ri * s + rd * 0.03;
            travelled += s;
            continue;
        }
        vec3 n = normalSolid(p);
        vec3 col = shadeSolid(p, n, rd, mat, type);
        col *= exp(-max(travelled - 60.0, 0.0) * 0.02);
        acc += tr * col;
        opaque = true;
        break;
    }
    float alpha = opaque ? 1.0 : 1.0 - dot(tr, vec3(0.3333));
    frag = vec4(acc, alpha);
}
)";

// ---------------------------------------------------------------------------------------------------------------
// The beam's haze and its dust (additive; never over the well or the HUD's columns), the lens's glow.
inline const char* HAZE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uViewC, uViewH;
uniform vec2 uLens;
uniform vec4 uCircle1;
uniform vec3 uLight1;
uniform float uTime, uSwirl, uSwirlAmp, uDust, uHaze, uLensGlow;
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
void main() {
    vec2 p = uViewC + (vUv * 2.0 - 1.0) * uViewH;
    vec2 ax = uCircle1.xy - uLens;
    float len = length(ax);
    ax /= len;
    vec2 q = p - uLens;
    float along = dot(q, ax);
    float across = abs(q.x * ax.y - q.y * ax.x);
    float w = 0.15 + along / len * uCircle1.w * 0.92;
    float beam = smoothstep(w, w * 0.6, across) * smoothstep(-0.2, 1.2, along);
    // (the haze lives near the lamp: thinning out long before the well, and never over it)
    float keep = smoothstep(-6.4, -10.5, p.x);
    float dens = exp(-along / 8.0) * 0.9 + 0.1;
    float swirlN = vnoise(p * 0.32 + vec2(uTime * 0.05, -uTime * 0.03) + uSwirl * 0.05) * 0.7 + vnoise(p * 0.9 - uTime * 0.04) * 0.3;
    vec3 col = uLight1 * beam * keep * dens * uHaze * (0.5 + 0.9 * swirlN) * 0.06;
    // dust motes drifting in the beam, swirled by the beat
    vec2 sp = p + uSwirlAmp * vec2(sin(p.y * 0.55 + uSwirl), cos(p.x * 0.5 + uSwirl * 0.8)) * 0.8;
    float dust = 0.0;
    for (int l = 0; l < 2; l++) {
        float cs = l == 0 ? 0.9 : 0.55;
        vec2 g = sp / cs + float(l) * 17.3 + vec2(uTime * 0.05, uTime * 0.03 * (l == 0 ? 1.0 : -1.0));
        vec2 id = floor(g);
        float h1 = hash12(id), h2 = hash12(id + 5.7), h3 = hash12(id + 13.1);
        if (h3 > 0.35 + 0.5 * (1.0 - uDust)) {
            vec2 c = id + 0.5 + 0.32 * vec2(sin(uTime * (0.3 + h1) + h2 * 6.28), cos(uTime * (0.25 + h2 * 0.8) + h1 * 6.28));
            float d = length(g - c) * cs;
            float tw = 0.5 + 0.5 * sin(uTime * (1.0 + 2.0 * h1) + h3 * 20.0);
            dust += smoothstep(0.045, 0.0, d) * (0.4 + 0.6 * tw) * (l == 0 ? 1.0 : 0.6);
        }
    }
    col += uLight1 * dust * beam * keep * (0.4 + dens) * 0.55;
    // the lens's glow
    float r = length(p - uLens);
    col += uLight1 * (exp(-r * r / 0.5) * 0.8 + 0.06 / (1.0 + r * r * 0.6)) * (0.6 + uLensGlow) * keep;
    frag = vec4(col, 0.0);
}
)";

inline const char* FINAL_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uScene;
uniform float uMul, uTime, uExposure;
void main() {
    vec3 c = texture(uScene, vUv).rgb * uExposure;
    c = 1.0 - exp(-c * 1.6);
    vec2 q = vUv - 0.5;
    c *= 1.0 - 0.35 * dot(q, q);
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    float g = fract(sin(dot(gl_FragCoord.xy + fract(uTime) * 61.0, vec2(12.9898, 78.233))) * 43758.5453);
    c += (g - 0.5) / 255.0;
    frag = vec4(c * uMul, 1.0);
}
)";

inline const char* COMPOSE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
void main() { frag = texture(uTex, vUv); }
)";

inline const char* RECT_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec4 uRect; // x0, y0, x1, y1 (NDC)
out vec2 vUv;
void main() {
    vUv = aPos * 0.5 + 0.5;
    gl_Position = vec4(mix(uRect.xy, uRect.zw, vUv), 0.0, 1.0);
}
)";

inline const char* PANEL_VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uRes;
void main() { gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0); }
)";
inline const char* PANEL_FS = R"(#version 330 core
out vec4 frag;
uniform vec4 uColor;
void main() { frag = uColor; }
)";

} // namespace shadow_shaders

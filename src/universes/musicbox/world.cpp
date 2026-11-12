// MUSIC BOX: building the box and its mechanism, and the frame (see musicbox.cpp).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "layout.hpp"
#include "universes/musicbox/musicbox.hpp"
#include "universes/musicbox/world.hpp"

using namespace mb_world;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

namespace {
const vec3 UP(0.f, 1.f, 0.f), AX(1.f, 0.f, 0.f), AZ(0.f, 0.f, 1.f);
vec3 rot(const vec3& v, const vec3& k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}
// Two unit vectors orthogonal to a (and to each other).
void basis(const vec3& a, vec3& u, vec3& w) {
    const vec3 ref = std::fabs(a.y) < 0.9f ? UP : AX;
    u = normalize(cross(ref, a));
    w = cross(a, u);
}
double wrapd(double x, double n) {
    x = std::fmod(x + n * 0.5, n);
    if (x < 0) x += n;
    return x - n * 0.5;
}
constexpr float SLIDE_T = 0.3f;
} // namespace

void MusicBoxUniverse::put(Mesh m, const vec3& c, const vec3& ax, const vec3& ay, const vec3& az, const vec3& col, int mat, float p0, float p1,
                           float p2, float seed, const vec4& e) {
    Inst in;
    in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
    in.x[0] = ax.x; in.x[1] = ax.y; in.x[2] = ax.z; in.x[3] = p0;
    in.y[0] = ay.x; in.y[1] = ay.y; in.y[2] = ay.z; in.y[3] = p1;
    in.z[0] = az.x; in.z[1] = az.y; in.z[2] = az.z; in.z[3] = p2;
    in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = seed;
    in.e[0] = e.x; in.e[1] = e.y; in.e[2] = e.z; in.e[3] = e.w;
    (out_ ? out_ : stat_)[m].push_back(in);
}

void MusicBoxUniverse::cylAx(const vec3& c, const vec3& axis, float r, float h, const vec3& col, int mat, float p0, float p1, float p2,
                             float seed, const vec4& e) {
    vec3 u, w;
    basis(axis, u, w);
    put(MESH_CYL, c, u * r, axis * h, w * r, col, mat, p0, p1, p2, seed, e);
}

void MusicBoxUniverse::gear(const vec3& c, const vec3& axis, float r, float halfW, int teeth, float angle, int spokes, const vec3& col) {
    vec3 u, w;
    basis(axis, u, w);
    const vec3 ur = rot(u, axis, angle), wr = rot(w, axis, angle);
    const float rd = r * 0.93f;
    put(MESH_CYL, c, ur * rd, axis * halfW, wr * rd, col, M_BRASS, 1.f, (float)spokes, 0.f, 0.3f);
    // a hub
    put(MESH_CYL, c, ur * (r * 0.22f), axis * (halfW * 1.8f), wr * (r * 0.22f), col * 0.9f, M_BRASS, 1.f);
    const float m = 2.f * r / teeth; // the module
    for (int k = 0; k < teeth; k++) {
        const float a = angle + TAU * k / teeth;
        const vec3 rd2 = rot(u, axis, a);
        const vec3 tg = cross(axis, rd2);
        put(MESH_BOX, c + rd2 * (rd + m * 0.55f), rd2 * (m * 0.62f), axis * (halfW * 0.98f), tg * (PI * r / teeth * 0.42f), col, M_BRASS,
            1.f, 0.f, 0.f, 0.f);
    }
}

// =====================================================================================================
// The box: built once per variant (the static parts)
// =====================================================================================================

void MusicBoxUniverse::buildStatic() {
    for (auto& v : stat_) v.clear();
    out_ = nullptr;
    const Palette& P = pal_;
    // the table, the wall far behind
    box(vec3(0.f, -1.f, -20.f), vec3(320.f, 1.f, 110.f), P.table, M_TABLE);
    box(vec3(0.f, 60.f, -130.f), vec3(320.f, 62.f, 1.f), P.wall, M_WALL);
    // the box: a plinth, four walls (the front inlaid with stringing and a brass escutcheon), a bead on top
    const vec3 wd = P.wood;
    box(vec3(0.f, 0.7f, 0.f), vec3(BX + 0.6f, 0.7f, BZ + 0.6f), wd * 0.7f, M_WOOD, 0.f, P.figure, 0.f, 1.3f);
    const float wy0 = 1.4f, wh = (BH - wy0) * 0.5f, wc = wy0 + wh;
    box(vec3(0.f, wc, BZ - WT * 0.5f), vec3(BX, wh, WT * 0.5f), wd, M_WOOD, 3.f, P.figure, 0.f, 2.1f);
    box(vec3(0.f, wc, -BZ + WT * 0.5f), vec3(BX, wh, WT * 0.5f), wd, M_WOOD, 1.f, P.figure, 0.f, 3.7f);
    for (int s = -1; s <= 1; s += 2)
        box(vec3(s * (BX - WT * 0.5f), wc, 0.f), vec3(WT * 0.5f, wh, BZ - WT), wd, M_WOOD, 1.f, P.figure, 0.f, 4.4f + s);
    // a fine moulding round the top edge, a brass lock plate on the front's edge
    box(vec3(0.f, BH + 0.1f, BZ - WT * 0.5f), vec3(BX + 0.1f, 0.1f, WT * 0.5f + 0.1f), wd * 0.8f, M_WOOD, 0.f, 0.f, 0.f, 5.f);
    box(vec3(0.f, BH + 0.1f, -BZ + WT * 0.5f), vec3(BX + 0.1f, 0.1f, WT * 0.5f + 0.1f), wd * 0.8f, M_WOOD, 0.f, 0.f, 0.f, 5.5f);
    for (int s = -1; s <= 1; s += 2) box(vec3(s * (BX - WT * 0.5f), BH + 0.1f, 0.f), vec3(WT * 0.5f + 0.1f, 0.1f, BZ), wd * 0.8f, M_WOOD, 0.f, 0.f, 0.f, 6.f);
    box(vec3(0.f, BH + 0.05f, BZ - WT * 0.5f), vec3(1.6f, 0.07f, 0.45f), P.brass, M_BRASS, 1.f);
    // the velvet floor (an opening for the dancer's trap), the dark below it
    box(vec3(0.f, 2.8f, 0.f), vec3(BX - WT, 0.2f, BZ - WT), P.velvet, M_VELVET, STAGE_R);
    box(vec3(0.f, 2.0f, 0.f), vec3(BX - WT, 0.6f, BZ - WT), P.velvet * 0.08f, M_VELVET, STAGE_R + 0.2f);
    cyl(vec3(STAGE.x, 1.5f, STAGE.z), STAGE_R + 0.3f, 0.06f, P.velvet * 0.05f, M_VELVET);
    // a brass ring round the trap
    for (int k = 0; k < 40; k++) {
        const float a0 = TAU * k / 40.f;
        const vec3 rd(std::cos(a0), 0.f, std::sin(a0));
        put(MESH_BOX, STAGE + vec3(0.f, 0.06f, 0.f) + rd * (STAGE_R + 0.18f), rd * 0.16f, UP * 0.06f, cross(UP, rd) * (TAU * (STAGE_R + 0.18f) / 80.f + 0.02f),
            P.brass, M_BRASS, 1.f);
    }
    // the bedplate: black iron
    box(vec3(0.f, (FLOOR + BED) * 0.5f, CZ + 2.6f), vec3(27.5f, (BED - FLOOR) * 0.5f, 8.6f), P.bed, M_IRON, 0.f, 0.f, 0.f, 0.7f);
    // the cylinder's cocks (brass), the governor's and the barrel's
    for (int s = -1; s <= 1; s += 2) {
        const float x = s * 18.4f;
        box(vec3(x, (BED + CY) * 0.5f, CZ), vec3(0.7f, (CY - BED) * 0.5f, 1.5f), P.brass, M_BRASS, 0.f);
        cylAx(vec3(x, CY, CZ), AX, 1.5f, 0.7f, P.brass, M_BRASS, 0.f);
        // a steel screw on each
        cylAx(vec3(x, BED + 0.9f, CZ + 1.55f), AZ, 0.35f, 0.08f, P.steel, M_STEEL);
    }
    // the barrel's cocks
    for (int s = 0; s < 2; s++) {
        const float x = s == 0 ? BAR_C.x - 4.9f : BAR_C.x + 4.9f;
        box(vec3(x, (BED + BAR_C.y) * 0.5f, BAR_C.z), vec3(0.6f, (BAR_C.y - BED) * 0.5f, 1.3f), P.brass, M_BRASS, 0.f);
        cylAx(vec3(x, BAR_C.y, BAR_C.z), AX, 1.3f, 0.6f, P.brass, M_BRASS, 0.f);
    }
    // the governor: a cock for the train's arbor, the screw's bottom and top cocks
    {
        const vec3 p1(GR_R + PIN_GR, 0.f, 0.f);
        (void)p1;
        const float py = CY + 1.6f, pz = CZ - 3.17f;
        box(vec3(21.2f, (BED + py) * 0.5f, pz), vec3(0.6f, (py - BED) * 0.5f, 1.1f), P.brass, M_BRASS, 0.f);
        cylAx(vec3(21.2f, py, pz), AX, 1.1f, 0.6f, P.brass, M_BRASS, 0.f);
        const float wx = 19.6f, wz = pz + WH2_R + 0.5f;
        box(vec3(wx, (BED + CY - 1.6f) * 0.5f, wz), vec3(0.8f, (CY - 1.6f - BED) * 0.5f, 0.8f), P.brass, M_BRASS, 0.f);
        // the arched top cock: two posts and a bridge over the fly
        const float top = CY + 3.9f;
        for (int s = -1; s <= 1; s += 2) box(vec3(wx + s * 2.6f, (BED + top) * 0.5f, wz), vec3(0.35f, (top - BED) * 0.5f, 0.5f), P.brass, M_BRASS, 0.f);
        box(vec3(wx, top + 0.25f, wz), vec3(2.95f, 0.25f, 0.5f), P.brass, M_BRASS, 1.f);
        cylAx(vec3(wx, top + 0.55f, wz), UP, 0.45f, 0.12f, P.steel, M_STEEL);
    }
    // the comb: its base, the plate behind the teeth (one piece with them), screws
    {
        const float y0 = BED, y1 = CY - 0.17f;
        box(vec3(0.f, (y0 + y1) * 0.5f, (CZ + 9.6f + COMB_BACK + 0.4f) * 0.5f), vec3(16.6f, (y1 - y0) * 0.5f, (COMB_BACK + 0.4f - CZ - 9.6f) * 0.5f), P.brass,
            M_BRASS, 0.f);
        for (int i = 0; i < TRACKS; i++) {
            const float root = TIP_Z + toothLen(i);
            const float z0 = root - 0.02f, z1 = COMB_BACK;
            box(vec3(trackX(i), CY, (z0 + z1) * 0.5f), vec3(PITCH * 0.5f, 0.16f, (z1 - z0) * 0.5f), P.steel, M_STEEL, 0.f, 0.f, 0.f, 0.2f * i);
        }
        for (int k = 0; k < 6; k++) {
            const float x = -13.5f + k * 5.4f;
            const vec3 c(x, CY + 0.16f, COMB_BACK - 0.85f);
            put(MESH_DOME, c, vec3(0.42f, 0, 0), vec3(0, 0.16f, 0), vec3(0, 0, 0.42f), P.steel, M_STEEL);
            put(MESH_BOX, c + UP * 0.12f, vec3(0.36f, 0, 0), vec3(0, 0.05f, 0), vec3(0, 0, 0.05f), P.steel * 0.2f, M_IRON);
        }
    }
    // the lid, open: inlaid inside (stringing, crossbanding, a rosette), its edge a darker moulding
    {
        const vec3 Y = rot(AZ, AX, -LID_OPEN), Z = rot(vec3(0.f, -1.f, 0.f), AX, -LID_OPEN);
        const vec3 c = HINGE + Y * (LID_L * 0.5f) - Z * (LID_T * 0.5f);
        obox(c, AX, Y, Z, vec3(BX + 0.3f, LID_L * 0.5f + 0.3f, LID_T * 0.5f), P.woodLid, M_WOOD, 2.f, P.figure, 0.f, 7.3f);
        // hinges
        for (int s = -1; s <= 1; s += 2) cylAx(HINGE + vec3(s * 20.f, 0.1f, -0.1f), AX, 0.35f, 1.8f, P.brass, M_BRASS, 1.f);
    }
    staticDirty_ = false;
    for (int m = 0; m < MESH_COUNT; m++) {
        glBindBuffer(GL_ARRAY_BUFFER, statVbo_[m]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(stat_[m].size() * sizeof(Inst)), stat_[m].empty() ? nullptr : stat_[m].data(), GL_STATIC_DRAW);
        statCount_[m] = stat_[m].size();
    }
}

// The dancer's foot: under the floor, rising through the trap to stand over the cylinder.
vec3 MusicBoxUniverse::dancerBase() const {
    const float k = smoothstepf(0.2f, 1.f, dancer_);
    const float e = k * k * (3.f - 2.f * k);
    return STAGE + UP * lerpf(-11.f, 10.2f, e);
}

// =====================================================================================================
// The moving parts: the cylinders and their pins, the wheels, the governor, the comb's teeth, the trap and the
// dancer, the ratchet
// =====================================================================================================

void MusicBoxUniverse::buildMoving(float lidOpen) {
    (void)lidOpen;
    const Palette& P = pal_;
    out_ = dyn_;
    const float cylA = -(float)(phi_ * (TAU / SLOTS)); // (about +x: the front moves up)
    // ---- the cylinders and their pins
    float maxOrder = 1.f;
    for (const Cylinder& c : cyls_)
        for (const Pin& p : c.pins) maxOrder = std::max(maxOrder, p.order);
    for (int ci = 0; ci < (int)cyls_.size(); ci++) {
        const Cylinder& c = cyls_[ci];
        if (c.rest < 0 && c.inT <= 0.f) continue;
        const vec3 cc = cylCenter(ci);
        const float a = c.rest < 0 ? cylA : -(float)(c.phiAt * (TAU / SLOTS));
        vec3 fr;
        const vec3 ca = cylAxis(ci, &fr);
        // the cylinder, turned brass, its end flanges, the arbor
        put(MESH_CYL, cc, rot(UP, ca, a) * RC, ca * CYL_HALF, rot(fr, ca, a) * RC, P.brass, M_BRASS, 2.f, 0.f, 0.f, 0.1f * ci);
        for (int s = -1; s <= 1; s += 2) cylAx(cc + ca * (s * (CYL_HALF + 0.12f)), ca, RC + 0.14f, 0.12f, P.brass * 0.95f, M_BRASS, 1.f);
        cylAx(cc, ca, 0.33f, CYL_HALF + (c.rest < 0 ? 3.6f : 1.2f), P.steel, M_STEEL);
        if (c.rest >= 0) {
            // set aside on the table: two brass cradles under it once it's down
            if (c.swapT >= 1.f)
                for (int s = -1; s <= 1; s += 2) box(vec3(cc.x, 0.55f, cc.z + s * 12.f), vec3(2.2f, 0.55f, 0.6f), P.brass, M_BRASS, 1.f);
        } else {
            // the great wheel (left), the governor's first wheel (right)
            gear(cc + AX * GW_X, AX, GW_R, 0.22f, 44, a, 5, P.brass);
            gear(vec3(16.6f + cc.x, cc.y, cc.z), AX, GR_R, 0.2f, 32, a, 4, P.brass);
        }
        for (const Pin& p : c.pins) {
            if (!p.landed) continue;
            vec3 rd;
            const vec3 hp = pinPos(ci, p, 0.f, &rd);
            const vec3 col = P.piece[p.type];
            // the shaft, the head
            const vec3 sc = hp - rd * (PIN_L * 0.5f + 0.02f);
            vec3 u, w;
            basis(rd, u, w);
            put(MESH_CYL, sc, u * PIN_R, rd * (PIN_L * 0.5f + 0.04f), w * PIN_R, lerp(col, P.brass, 0.4f), M_PIN, 0.f, P.enamel[p.type] * 0.f, 0.f, 0.f);
            float g = p.glow;
            const float f = p.order / maxOrder;
            if (quadT_ < 6.f) g += 0.6f * std::exp(-std::fabs(f - quadT_ / 3.f) * 14.f);
            if (dropAmt_ > 0.01f) g += 0.35f * dropAmt_ * std::exp(-std::fabs(f - std::fmod(worldTime_ * 0.09f, 1.25f) + 0.1f) * 16.f);
            put(MESH_DOME, hp - rd * 0.05f, u * HEAD_R, rd * (HEAD_R * 0.62f), w * HEAD_R, col, M_PIN, g, P.enamel[p.type], 0.f, 0.f);
        }
    }
    // ---- the spring barrel and its ratchet (left), the governor's train, its screw and fly (right)
    {
        const float barA = -cylA * (GW_R / BAR_GR) * 0.25f;
        cylAx(BAR_C, AX, BAR_R, 4.3f, P.brass, M_BRASS, 2.f, 0.f, 0.f, 0.4f);
        for (int s = -1; s <= 1; s += 2) cylAx(BAR_C + AX * (s * 4.35f), AX, BAR_R + 0.12f, 0.1f, P.brass * 0.95f, M_BRASS, 1.f);
        gear(vec3(GW_X, BAR_C.y, BAR_C.z), AX, BAR_GR, 1.0f, 52, -cylA * (GW_R / BAR_GR), 6, P.brass);
        cylAx(BAR_C, AX, 0.3f, 6.4f, P.steel, M_STEEL);
        // the ratchet (steel) and its click
        const float wind = windAngle_;
        gear(vec3(BAR_C.x - 5.7f, BAR_C.y, BAR_C.z), AX, 2.3f, 0.18f, 24, barA + wind, 0, P.steel);
        // the click: a steel pawl on the ratchet, a spring behind it
        put(MESH_BOX, vec3(BAR_C.x - 5.7f, BAR_C.y + 2.55f, BAR_C.z + 0.6f), AX * 0.16f, vec3(0.f, 0.25f, -0.1f), vec3(0.f, 0.05f, 0.9f) * 0.9f, P.steel,
            M_STEEL);
        // the train: a pinion and a wheel on one arbor, the endless screw, the fly
        const float py = CY + 1.6f, pz = CZ - 3.17f;
        const float pinA = cylA * (GR_R / PIN_GR) * -1.f;
        gear(vec3(16.6f, py, pz), AX, PIN_GR, 1.1f, 10, pinA, 0, P.steel);
        gear(vec3(19.6f, py, pz), AX, WH2_R, 0.18f, 36, pinA, 4, P.brass);
        cylAx(vec3(18.9f, py, pz), AX, 0.22f, 2.6f, P.steel, M_STEEL);
        const float wx = 19.6f, wz = pz + WH2_R + 0.5f;
        const float wy0 = CY - 1.6f, wy1 = CY + 3.9f;
        cylAx(vec3(wx, (wy0 + wy1) * 0.5f, wz), UP, 0.14f, (wy1 - wy0) * 0.5f, P.steel, M_STEEL);
        cylAx(vec3(wx, py, wz), UP, 0.5f, 1.3f, P.steel, M_BRASS, 1.f, 0.f, 3.5f, 0.f, vec4(govAngle_ / TAU, 0.f, 0.f, 0.f));
        // the fly: two vanes
        const vec3 fc(wx, CY + 3.1f, wz);
        for (int s = -1; s <= 1; s += 2) {
            const vec3 rd = rot(AX, UP, govAngle_) * (float)s;
            put(MESH_BOX, fc + rd * 1.3f, rd * 1.05f, UP * 0.55f, cross(UP, rd) * 0.03f, P.brass, M_BRASS, 1.f);
        }
        cylAx(fc, UP, 0.28f, 0.2f, P.brass, M_BRASS, 1.f);
    }
    // ---- the comb's teeth: each lifted by the pin under it, ringing after it slips off
    for (int i = 0; i < TRACKS; i++) {
        const float L = toothLen(i);
        const vec3 root(trackX(i), CY, TIP_Z + L);
        const float lift = toothLift_[i];
        const vec3 d = normalize(vec3(0.f, lift, -L));
        const vec3 c = root + d * (L * 0.5f);
        const vec3 Y = normalize(cross(d * -1.f, AX)) * -1.f;
        const float glow = toothRing_[i] < 1.f ? std::exp(-toothRing_[i] * 9.f) * (0.05f + 0.25f * kickK_) : 0.f;
        // (tapered: wider at the root)
        const vec3 Yt = cross(d, AX) * -0.15f;
        put(MESH_BOX, root + d * (L * 0.3f), AX * 0.6f, Yt, d * (L * 0.3f), P.steel, M_STEEL, 0.f, 0.f, 0.f, 0.3f * i, vec4(0.f, glow, 0.f, 0.f));
        put(MESH_BOX, root + d * (L * 0.8f), AX * 0.42f, Yt, d * (L * 0.2f), P.steel, M_STEEL, 0.f, 0.f, 0.f, 0.3f * i, vec4(0.f, glow, 0.f, 0.f));
        (void)c;
        (void)Y;
    }
    // ---- the dancer's trap and the dancer
    {
        const float open = smoothstepf(0.f, 0.3f, dancer_) * 1.95f;
        const vec3 hinge = STAGE + vec3(0.f, 0.15f, -STAGE_R);
        const vec3 axis = rot(UP, AX, -open), toC = rot(AZ, AX, -open);
        const vec3 c = hinge + toC * STAGE_R;
        vec3 u, w;
        basis(axis, u, w);
        put(MESH_CYL, c, u * STAGE_R, axis * 0.15f, w * STAGE_R, P.velvet, M_VELVET);
        // under the trap's velvet: brass
        put(MESH_CYL, c - axis * 0.17f, u * (STAGE_R - 0.1f), axis * 0.03f, w * (STAGE_R - 0.1f), P.brass * 0.8f, M_BRASS, 0.f);
        if (dancer_ > 0.2f) {
            const vec3 base = dancerBase();
            const float ta = dancerTurn_;
            const float DS = DANCER_S;
            const vec3 pc = P.porcelain;
            const vec3 tint = lerp(pc, P.piece[4], 0.12f);
            auto at = [&](float x, float y, float z) { return base + rot(vec3(x, y, z) * DS, UP, ta); };
            auto limb = [&](const vec3& a, const vec3& b, float r0, const vec3& col, float gilt = 0.f) {
                const float r = r0 * DS;
                const vec3 d = b - a;
                const float l = length(d);
                if (l < 1e-4f) return;
                cylAx((a + b) * 0.5f, d / l, r, l * 0.5f, col, M_PORCELAIN, gilt);
                put(MESH_DOME, b, vec3(r, 0, 0), vec3(0, r, 0), vec3(0, 0, r), col, M_PORCELAIN, gilt);
                put(MESH_DOME, b, vec3(r, 0, 0), vec3(0, -r, 0), vec3(0, 0, r), col, M_PORCELAIN, gilt);
            };
            // the spindle and a gilt base
            const float sp = std::max(0.1f, base.y - (FLOOR - 1.f));
            cylAx(base - UP * (sp * 0.5f), UP, 0.16f, sp * 0.5f, P.brass, M_BRASS, 1.f);
            cylAx(base - UP * 0.08f, UP, 0.62f * DS, 0.1f, P.brass, M_BRASS, 1.f);
            // legs: one en pointe, one in passe (its toe at the knee)
            const vec3 hip = at(0.f, 3.25f, 0.f);
            limb(at(0.f, 0.1f, 0.f), at(0.f, 1.65f, 0.f), 0.13f, pc);
            limb(at(0.f, 1.65f, 0.f), hip, 0.17f, pc);
            const vec3 knee = at(1.05f, 2.6f, 0.25f);
            limb(at(0.18f, 3.2f, 0.f), knee, 0.17f, pc);
            limb(knee, at(0.12f, 1.75f, 0.05f), 0.13f, pc);
            // the tutu: a flat round skirt, gilt-edged
            const vec3 tc = at(0.f, 3.35f, 0.f);
            const vec3 rx = rot(AX, UP, ta) * DS, rz = rot(AZ, UP, ta) * DS, uy = UP * DS;
            put(MESH_DOME, tc, rx * 1.75f, uy * 0.42f, rz * 1.75f, tint, M_PORCELAIN);
            put(MESH_DOME, tc, rx * 1.75f, uy * -0.2f, rz * 1.75f, tint * 0.9f, M_PORCELAIN);
            put(MESH_CYL, tc, rx * 1.78f, uy * 0.035f, rz * 1.78f, P.brass, M_PORCELAIN, 1.f);
            // the bodice and the chest, the neck, the head and its bun
            put(MESH_DOME, at(0.f, 3.5f, 0.f), rx * 0.36f, uy * 1.25f, rz * 0.27f, tint, M_PORCELAIN);
            put(MESH_DOME, at(0.f, 4.6f, 0.f), rx * 0.44f, uy * -0.5f, rz * 0.3f, tint, M_PORCELAIN);
            put(MESH_DOME, at(0.f, 4.6f, 0.f), rx * 0.44f, uy * 0.3f, rz * 0.3f, pc, M_PORCELAIN);
            limb(at(0.f, 4.8f, 0.f), at(0.f, 5.15f, 0.f), 0.11f, pc);
            const vec3 head = at(0.f, 5.5f, 0.02f);
            put(MESH_DOME, head, rx * 0.32f, uy * 0.4f, rz * 0.32f, pc, M_PORCELAIN);
            put(MESH_DOME, head, rx * 0.32f, uy * -0.36f, rz * 0.32f, pc, M_PORCELAIN);
            const vec3 hair = lerp(P.wood, vec3(0.05f, 0.03f, 0.02f), 0.6f);
            put(MESH_DOME, head + UP * (0.05f * DS) + rot(vec3(0.f, 0.f, -0.04f * DS), UP, ta), rx * 0.34f, uy * 0.38f, rz * 0.34f, hair, M_PORCELAIN);
            put(MESH_DOME, at(0.f, 5.85f, -0.25f), rx * 0.17f, uy * 0.17f, rz * 0.17f, hair, M_PORCELAIN);
            // arms en couronne: shoulder, elbow, hand meeting over the head
            for (int s = -1; s <= 1; s += 2) {
                const vec3 sh = at(s * 0.4f, 4.75f, 0.f), el = at(s * 0.95f, 5.7f, 0.12f), ha = at(s * 0.22f, 6.35f, 0.1f);
                limb(sh, el, 0.085f, pc);
                limb(el, ha, 0.075f, pc);
            }
        }
    }
    out_ = nullptr;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MusicBoxUniverse::render(const Frame& f, GLuint fbo) {
    bare_ = f.bare;
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.8f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    if (staticDirty_) buildStatic();
    if (cyls_.empty()) newCylinder(); // (an empty cylinder from the start)

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    // (ZEN_FX=drop, a test hook, forces the drop whatever the section)
    static const bool fxDrop = std::getenv("ZEN_FX") && std::strncmp(std::getenv("ZEN_FX"), "drop", 4) == 0;
    const bool isDrop = dropMode_ && overT_ < 0.f && (fxDrop || (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f));
    dropAmt_ = approach(dropAmt_, isDrop ? 1.f : 0.f, isDrop ? 1.0f : 0.22f, dt);
    // the dancer: up with the drop, staying a few bars after it, then down
    if (isDrop) dancerHold_ = 7.f;
    else dancerHold_ = std::max(0.f, dancerHold_ - dt);
    const bool up = isDrop || dancerHold_ > 0.f;
    dancer_ = approach(dancer_, up ? 1.f : 0.f, up ? 0.55f : 0.18f, dt);
    quadT_ += dt;
    levelT_ += dt;
    if (levelT_ < 1.6f) windAngle_ += dt * 1.2f * (std::fmod(levelT_, 0.8f) < 0.4f ? 1.f : 0.f);
    kickK_ = std::max(kickK_ * std::exp(-dt * 5.f), kickIn_);
    kickIn_ = 0.f;
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The cylinder's turn: a slot past the comb per beat (two in calm parts), locked to the song's beat grid.
    {
        if (music.bpm > 20.f) lastBpm_ = music.bpm;
        double beatNow;
        if (music.hasSong) beatNow = music.beat;
        else {
            clockBeat_ += dt * lastBpm_ / 60.f;
            beatNow = clockBeat_;
        }
        double dBeat = beatNow - lastBeat_;
        if (lastBeat_ < -0.5 || dBeat < 0.0 || dBeat > 1.0) dBeat = dt * lastBpm_ / 60.f;
        if (f.paused) dBeat = 0.0;
        lastBeat_ = beatNow;
        rate_ = approach(rate_, lv_ < 0.5f ? 0.5f : 1.f, 0.4f, dt);
        speedK_ = approach(speedK_, overT_ >= 0.f ? 0.f : 1.f, overT_ >= 0.f ? 0.22f : 0.5f, dt);
        const float slowK = lerpf(1.f, 0.35f, slow_);
        const double dphi = dBeat * rate_ * speedK_ * slowK;
        phi_ += dphi;
        govAngle_ += (float)(dphi * (TAU / SLOTS) * (GR_R / PIN_GR) * 20.0);
        // keep the slots on the beat (none while slowed or winding down)
        if (music.hasSong && slow_ < 0.02f && speedK_ > 0.98f && (rate_ == 0.5f || rate_ == 1.f) && dt > 0.f) {
            const double want = beatNow * rate_ - std::floor(beatNow * rate_);
            const double have = phi_ - std::floor(phi_);
            phi_ += wrapd(want - have, 1.0) * std::min(1.f, dt * 1.5f);
        }
        // a new turn: the next pinned tune (the cylinder shifts along its axis)
        const int rev = (int)std::floor(phi_ / SLOTS);
        if (rev != lastRev_ && !cyls_.empty()) {
            lastRev_ = rev;
            const Cylinder& c = cyls_.back();
            for (int k = 1; k <= TUNES; k++) {
                const int t = (playTune_ + k) % TUNES;
                bool any = false;
                for (const Pin& p : c.pins)
                    if (p.tune == t && p.landed) { any = true; break; }
                if (any) { playTune_ = t; break; }
            }
        }
        shiftX_ = approach(shiftX_, -tuneDx(playTune_), 0.9f, dt);
    }
    // the cylinders: one set aside, a fresh one coming in
    for (Cylinder& c : cyls_) {
        if (c.rest >= 0 && c.swapT < 1.f) c.swapT = std::min(1.f, c.swapT + dt / 3.5f);
        if (c.rest < 0 && c.inT < 1.f) c.inT = std::min(1.f, c.inT + dt / 2.2f);
        for (Pin& p : c.pins) p.glow = approach(p.glow, 0.f, 0.6f, dt);
    }

    // ---- The test hooks: rows already pinned, rows cleared at a rate.
    {
        if (!hookPlaced_) {
            hookPlaced_ = true;
            const int n = startLinesHook();
            for (int l = 0; l < n; l++) {
                const int slot = reserveSlot(0.f);
                Cylinder& c = cyls_.back();
                c.rows++;
                rowsPinned_++;
                for (int x = 0; x < TRACKS; x++) {
                    Pin p;
                    p.slot = slot;
                    p.tune = fillTune_;
                    p.track = x;
                    p.type = rng_.irange(0, 6);
                    p.landed = true;
                    p.order = (float)c.rows + x * 0.05f;
                    c.pins.push_back(p);
                }
            }
            // the ones set aside are already at rest
            for (Cylinder& c : cyls_) {
                if (c.rest >= 0) c.swapT = 1.f;
                c.inT = 1.f;
            }
        }
        const int rate = rateLinesHook(time_);
        while (hookLines_ < rate) {
            hookLines_++;
            std::vector<GameEvent::CellInfo> row;
            for (int x = 0; x < Game::W; x++) row.push_back({x, Game::H - 1, rng_.irange(0, 6)});
            pinRow(row, Game::H - 1, 1, false, 0.f);
        }
    }

    // ---- The teeth: lifted by the playing tune's pins, ringing after they slip off.
    {
        const float bps = std::max(0.5f, lastBpm_ / 60.f);
        const float secPerSlot = 1.f / std::max(0.05f, bps * rate_ * speedK_ * lerpf(1.f, 0.35f, slow_));
        std::vector<float> push(TRACKS, 0.f), ring(TRACKS, 99.f);
        if (!cyls_.empty() && cyls_.back().rest < 0 && cyls_.back().inT >= 1.f) {
            for (const Pin& p : cyls_.back().pins) {
                if (!p.landed || p.tune != playTune_) continue;
                const double d = wrapd(phi_ - p.slot, SLOTS);
                if (d > -0.35 && d <= 0.0) push[p.track] = std::max(push[p.track], (float)((d + 0.35) / 0.35));
                else if (d > 0.0 && d < 4.0) ring[p.track] = std::min(ring[p.track], (float)d * secPerSlot);
            }
        }
        for (int i = 0; i < TRACKS; i++) {
            const float w = 22.f + 3.5f * i;
            const float r = ring[i] < 6.f ? std::exp(-ring[i] * 5.f) * std::cos(ring[i] * w) : 0.f;
            toothLift_[i] = 0.6f * (push[i] > 0.f ? push[i] * push[i] : r);
            toothRing_[i] = ring[i];
        }
    }

    // ---- Flights: out of the well, then to their pin.
    for (Flight& fl : flights_) {
        fl.t += dt;
        if (!fl.world && fl.t - fl.delay >= SLIDE_T) launch(fl);
    }
    // the dancer turns on the beat: half a turn every two beats, a little accent on each
    {
        const double B = (music.hasSong ? music.beat : clockBeat_) * 0.5;
        const float target = (float)(PI * (B + 0.12 * std::sin(TAU * B)));
        dancerTurn_ = dancer_ > 0.2f ? target : dancerTurn_;
    }

    // ---- Instances.
    for (auto& v : dyn_) v.clear();
    flightInst_.clear();
    cells_.clear();
    cellLines_.clear();
    sprites_.clear();
    buildMoving(1.f);
    for (auto it = flights_.begin(); it != flights_.end();) {
        Flight& fl = *it;
        const float tt = fl.t - fl.delay;
        Pin* pin = (fl.cyl < (int)cyls_.size() && fl.pin >= 0 && fl.pin < (int)cyls_[fl.cyl].pins.size()) ? &cyls_[fl.cyl].pins[fl.pin] : nullptr;
        if (!pin) { it = flights_.erase(it); continue; }
        if (!fl.world) {
            // lifting forward out of the well
            const float s = saturate(tt / SLIDE_T);
            const float e = s * s * (3.f - 2.f * s);
            if (tt >= 0.f || fl.hold) cellInst(cells_, fl.b0 + vec3(0.f, 0.f, 0.9f * e), 1.f, fl.type, 0.4f * e, fl.b0.x);
            ++it;
            continue;
        }
        vec3 rd;
        const vec3 target = pinPos(fl.cyl, *pin, 0.f, &rd);
        const float s = saturate(fl.t / fl.dur);
        const float L = length(target - fl.p0);
        const vec3 pA = target + rd * 1.2f;
        const vec3 p1 = fl.p0 + UP * (0.12f * L) + (fwdW_ * (0.2f * L));
        const vec3 p2 = pA + rd * (0.25f * L) + UP * (0.15f * L);
        const float sb = std::min(1.f, s / 0.85f);
        const float e = 1.f - std::pow(1.f - sb, 2.2f);
        const float u = 1.f - e;
        vec3 p = fl.p0 * (u * u * u) + p1 * (3.f * u * u * e) + p2 * (3.f * u * e * e) + pA * (e * e * e);
        if (s > 0.85f) p = lerp(pA, target, smoothstepf(0.85f, 1.f, s));
        const float size = lerpf(fl.size0, HEAD_R * 0.95f, smoothstepf(0.0f, 0.75f, s));
        const vec3 face = normalize(lerp(fl.face0, rd, smoothstepf(0.1f, 0.85f, s)));
        vec3 X = normalize(cross(UP, face));
        if (length(cross(UP, face)) < 1e-3f) X = AX;
        const vec3 Y = cross(face, X);
        Inst in{};
        const vec3 col = pal_.piece[fl.type];
        const float en = pal_.enamel[fl.type];
        const vec3 c = p - face * (size * 0.45f);
        in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)M_CELL;
        in.x[0] = X.x * size; in.x[1] = X.y * size; in.x[2] = X.z * size; in.x[3] = 0.25f;
        in.y[0] = Y.x * size; in.y[1] = Y.y * size; in.y[2] = Y.z * size; in.y[3] = en;
        in.z[0] = face.x * size * 0.45f; in.z[1] = face.y * size * 0.45f; in.z[2] = face.z * size * 0.45f; in.z[3] = en;
        in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = fl.b0.x;
        flightInst_.push_back(in);
        if (s >= 1.f) {
            pin->landed = true;
            pin->glow = 0.9f;
            it = flights_.erase(it);
            continue;
        }
        ++it;
    }
    if (f.bare) {
        cells_.clear();
        cellLines_.clear();
        flightInst_.clear();
    } else buildBoard(game);

    // ---- The light.
    const Palette& P = pal_;
    const float overK = overT_ >= 0.f ? 1.f - 0.25f * smoothstepf(0.f, 4.f, overT_) : 1.f;
    const float lampLevel = P.lampK * (0.84f + 0.06f * wm + 0.08f * wp + 0.08f * dropAmt_) * (1.f - 0.45f * slow_) * overK;
    const vec3 lampCol = lerp(P.lamp, P.lamp * vec3(1.f, 0.8f, 0.55f), slow_);
    const vec3 fillCol = P.fill * (0.08f + 0.35f * P.night + 0.25f * slow_);
    const vec3 fillDir = normalize(vec3(0.7f, 0.45f, -0.35f));
    // the dancer's ring of light and its lamp
    std::vector<vec4> LP, LC;
    if (dancer_ > 0.2f) {
        const float k = smoothstepf(0.2f, 1.f, dancer_);
        const vec3 base = dancerBase();
        const float beatP = music.hasSong ? std::pow(1.f - (float)(music.beat * 0.5 - std::floor(music.beat * 0.5)), 3.f) : 0.f;
        const vec3 rc = lerp(P.lamp, vec3(1.f), 0.65f);
        const int N = 28;
        for (int i = 0; i < N; i++) {
            const float a = TAU * i / N - worldTime_ * 0.35f;
            const vec3 p = base + vec3(std::cos(a) * 3.3f, 0.1f, std::sin(a) * 3.3f);
            sprites_.push_back({{p.x, p.y, p.z, 0.42f}, {rc.x, rc.y, rc.z, k * (0.55f + 0.35f * beatP) * (0.75f + 0.25f * std::sin(a * 3.f + worldTime_))}});
        }
        sprites_.push_back({{base.x, base.y + 4.5f, base.z, 6.f}, {rc.x, rc.y, rc.z, 0.03f * k}});
        LP.push_back(vec4(base + vec3(0.f, 7.f, 5.f), 22.f));
        LC.push_back(vec4(rc, 0.7f * k));
        LP.push_back(vec4(base + vec3(0.f, 0.4f, 0.f), 10.f));
        LC.push_back(vec4(rc, 0.6f * k * (0.8f + 0.2f * beatP)));
    }
    // glints on the pins' heads (mid and peak), sparks on the teeth as they ring (peak)
    {
        const float gk = 0.15f + 0.45f * wm + 0.6f * wp + 0.4f * dropAmt_;
        if (!cyls_.empty() && gk > 0.05f) {
            const int ci = (int)cyls_.size() - 1;
            for (const Pin& p : cyls_[ci].pins) {
                if (!p.landed) continue;
                vec3 rd;
                const vec3 hp = pinPos(ci, p, 0.f, &rd);
                const vec3 V = normalize(eyeW_ - hp), Lm = normalize(LAMP - hp);
                const float g = std::pow(std::max(0.f, dot(rd, normalize(V + Lm))), 400.f);
                if (g > 0.15f) {
                    const vec3 gc = lerp(P.lamp, vec3(1.f), 0.6f);
                    sprites_.push_back({{hp.x + rd.x * HEAD_R, hp.y + rd.y * HEAD_R, hp.z + rd.z * HEAD_R, 0.55f}, {gc.x, gc.y, gc.z, g * gk * 1.2f}});
                }
            }
        }
        for (int i = 0; i < TRACKS; i++) {
            if (toothRing_[i] > 0.6f) continue;
            const float k = std::exp(-toothRing_[i] * 8.f) * (0.1f + 0.5f * wp + 0.3f * kickK_ * wp);
            if (k < 0.02f) continue;
            const vec3 tip(trackX(i), CY + toothLift_[i] + 0.15f, TIP_Z + 0.1f);
            const vec3 gc = lerp(P.lamp, vec3(1.f), 0.7f);
            sprites_.push_back({{tip.x, tip.y, tip.z, 0.7f}, {gc.x, gc.y, gc.z, k}});
        }
    }

    // ---- Shadow map from the lamp.
    const vec3 shC(0.f, 6.f, -2.f);
    const float shR = 44.f;
    const vec3 lampDir = normalize(LAMP - shC);
    mat4 shVP;
    {
        const mat4 view = mat4::lookAt(shC + lampDir * 120.f, shC, UP);
        shVP = mat4::ortho(-shR, shR, -shR, shR, 40.f, 220.f) * view;
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, 2048, 2048);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(progShadow_);
        setMat(progShadow_, "uVP", shVP);
        set3f(progShadow_, "uOrigin", vec3(0.f));
        for (int m = 0; m < MESH_COUNT; m++) {
            if (statCount_[m]) {
                glBindVertexArray(statVao_[m]);
                // (not the table and the wall: the first two boxes)
                glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)statCount_[m]);
            }
            if (!dyn_[m].empty()) {
                glBindVertexArray(dynVao_[m]);
                glBindBuffer(GL_ARRAY_BUFFER, dynVbo_[m]);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(dyn_[m].size() * sizeof(Inst)), dyn_[m].data(), GL_STREAM_DRAW);
                glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)dyn_[m].size());
            }
        }
    }

    // ---- The scene (multisampled).
    updateCamera(wdt);
    auto lighting = [&](GLuint p, bool board) {
        set3f(p, "uWallCol", P.wall * 1.4f + lampCol * 0.02f);
        set3f(p, "uFloorCol", lerp(P.table, P.velvet, 0.4f) * 1.2f);
        set3f(p, "uFillDir", fillDir);
        set3f(p, "uFillCol", fillCol);
        set3f(p, "uAmb", P.amb * (3.6f + 0.5f * wm) * (1.f - 0.3f * slow_) + P.fill * 0.06f * slow_);
        set3f(p, "uAmbLow", P.ambLow * 3.f);
        if (board) {
            set3f(p, "uEnvFront", lerp(vec3(0.3f), lampCol, 0.3f) * 0.55f);
            set3f(p, "uEnvBack", P.woodLid * lampCol * 1.2f + vec3(0.02f));
        } else {
            set3f(p, "uEnvFront", P.wall * 1.2f + lampCol * 0.01f);
            set3f(p, "uEnvBack", P.woodLid * lampCol * lampLevel * 4.5f + P.wall);
        }
        set3f(p, "uLampCol", lampCol);
        set1f(p, "uLampK", board ? 2.3f * (0.92f + 0.08f * wp) * (1.f - 0.3f * slow_) : lampLevel * 4.2f);
        if (board) {
            set3f(p, "uLampP", vec3(-14.f, 26.f, 30.f));
            set3f(p, "uSpotC", vec3(0.f));
            set1f(p, "uSpot", 1e4f);
            set1f(p, "uShadowOn", 0.f);
            set1i(p, "uNL", 0);
        } else {
            set3f(p, "uLampP", LAMP);
            set3f(p, "uSpotC", vec3(-2.f, 0.f, 0.f));
            set1f(p, "uSpot", 40.f);
            static const bool noShadow = std::getenv("ZEN_MB_NOSHADOW") != nullptr;
            set1f(p, "uShadowOn", noShadow ? 0.f : 1.f);
            const int n = (int)LP.size();
            set1i(p, "uNL", n);
            if (n) {
                glUniform4fv(ugl::U(p, "uLP"), n, &LP[0].x);
                glUniform4fv(ugl::U(p, "uLC"), n, &LC[0].x);
            }
        }
    };
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    const vec3 bg = P.wall * 0.5f;
    glClearColor(bg.x, bg.y, bg.z, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(progSolid_);
    set3f(progSolid_, "uEye", eyeW_);
    set1f(progSolid_, "uTime", worldTime_);
    setMat(progSolid_, "uVP", vpW_);
    set3f(progSolid_, "uOrigin", eyeW_);
    lighting(progSolid_, false);
    set1f(progSolid_, "uStackA", 0.f);
    set3f(progSolid_, "uBrass", P.brass);
    set3f(progSolid_, "uInlayA", P.inlayA);
    set3f(progSolid_, "uInlayB", P.inlayB);
    set3f(progSolid_, "uHoleC", STAGE);
    set4f(progSolid_, "uBoxIn", BX - WT, BZ - WT, BH, FLOOR);
    set1f(progSolid_, "uDim", 0.f);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    set1i(progSolid_, "uShadow", 1);
    setMat(progSolid_, "uShadowVP", shVP);
    set1f(progSolid_, "uShadowTexel", 1.f / 2048.f);
    set1f(progSolid_, "uShadowBias", 2.f * shR / 2048.f * 1.5f);
    glActiveTexture(GL_TEXTURE0);
    for (int m = 0; m < MESH_COUNT; m++) {
        if (statCount_[m]) {
            glBindVertexArray(statVao_[m]);
            glUniform1i(ugl::U(progSolid_, "uMesh"), m);
            glDrawElementsInstanced(GL_TRIANGLES, meshN_[m], GL_UNSIGNED_INT, nullptr, (GLsizei)statCount_[m]);
        }
        if (!dyn_[m].empty()) drawInst(dynVao_[m], dynVbo_[m], m, &dyn_[m], dyn_[m].size());
    }
    // the glows
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDepthMask(GL_FALSE);
    if (!sprites_.empty()) {
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progSprite_);
        set3f(progSprite_, "uEye", eyeW_);
        set1f(progSprite_, "uTime", worldTime_);
        setMat(progSprite_, "uVP", vpW_);
        set3f(progSprite_, "uOrigin", eyeW_);
        set3f(progSprite_, "uCamR", rightW_);
        set3f(progSprite_, "uCamU", upW_);
        set1f(progSprite_, "uMinR", 1.4f * 2.f * tanW_ / height_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    // the veil behind the well and the HUD's columns (none in a break)
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!f.bare) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uVeil", P.veil);
        set1f(progVeil_, "uWellA", 0.42f);
        set1f(progVeil_, "uColA", 0.5f);
        set4f(progVeil_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(progVeil_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
        ugl::drawFullscreen();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    if (!flightInst_.empty()) {
        glUseProgram(progSolid_);
        drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &flightInst_, flightInst_.size());
    }
    // ---- The well: fine brass rails, the plates, the ghost (none in a break)
    glClear(GL_DEPTH_BUFFER_BIT);
    if (!f.bare) {
        std::vector<Inst> frame;
        auto fb = [&](const vec3& c, const vec3& h, const vec3& col, int mat, float p0 = 0.f) {
            Inst in{};
            in.c[0] = c.x; in.c[1] = c.y; in.c[2] = c.z; in.c[3] = (float)mat;
            in.x[0] = h.x; in.x[3] = p0;
            in.y[1] = h.y;
            in.z[2] = h.z;
            in.col[0] = col.x; in.col[1] = col.y; in.col[2] = col.z; in.col[3] = 0.4f;
            frame.push_back(in);
        };
        for (int s = -1; s <= 1; s += 2) fb(vec3(s * 5.12f, -0.06f, -0.2f), vec3(0.06f, 10.12f, 0.2f), P.brass, M_BRASS, 1.f);
        fb(vec3(0.f, -10.12f, -0.2f), vec3(5.18f, 0.06f, 0.2f), P.brass, M_BRASS, 1.f);
        glUseProgram(progSolid_);
        set3f(progSolid_, "uEye", eyeB_);
        setMat(progSolid_, "uVP", vpB_);
        set3f(progSolid_, "uOrigin", vec3(0.f));
        lighting(progSolid_, true);
        set4f(progSolid_, "uBoxIn", -1.f, -1.f, 0.f, 0.f);
        set1f(progSolid_, "uStackA", 0.f);
        drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &frame, frame.size());
        set1f(progSolid_, "uStackA", 1.f);
        if (!cells_.empty()) drawInst(dynVao_[MESH_BOX], dynVbo_[MESH_BOX], MESH_BOX, &cells_, cells_.size());
    }
    if (!cellLines_.empty()) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progLine_);
        setMat(progLine_, "uVP", vpB_);
        set2f(progLine_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(cellLines_.size() * sizeof(Line)), cellLines_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cellLines_.size());
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ---- Resolve, the bloom, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glDisable(GL_DEPTH_TEST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 1.0f);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float stp = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", stp / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, stp / bloomA_.h);
        ugl::drawFullscreen();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE0);
    const vec3 slowTint(0.85f, 0.8f, 1.1f);
    set1f(progFinal_, "uBloomK", 0.5f + 0.06f * wp + 0.1f * dropAmt_);
    set1f(progFinal_, "uExposure", P.exposure);
    set1f(progFinal_, "uSatF", 1.02f + 0.04f * wp + 0.05f * dropAmt_);
    set1f(progFinal_, "uSlow", slow_ * 0.45f);
    set3f(progFinal_, "uSlowTint", slowTint);
    set1f(progFinal_, "uVig", 0.45f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

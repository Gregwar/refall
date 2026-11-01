#pragma once
// BRICKS' models: small toy-brick constructions authored as voxel paintings (one voxel = one stud, one brick high),
// then cut into bricks the way they would really be built: runs of one color cut into 1 x 1 .. 1 x 4 bricks in a
// running bond (the joints of a course offset from the course under it), paired into 2 x N bricks where they can,
// rows of roof tiles as slopes, sorted bottom-up (a course at a time, around the model).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

namespace bricks_models {

// Colors: the seven piece colors by role (each variant keeps their hues in this order), then neutrals.
enum Role { RED, YEL, BLU, GRN, ORA, PUR, CYA, WHT, DRK, TAN, GRY, ROLE_COUNT };
// The piece type of each colored role (I O T S Z J L = 0..6), -1 for the neutrals.
constexpr int ROLE_PIECE[ROLE_COUNT] = {4, 1, 5, 3, 6, 2, 0, -1, -1, -1, -1};
// The role of each piece type.
constexpr int PIECE_ROLE[7] = {CYA, YEL, PUR, GRN, RED, BLU, ORA};

// Slope directions (the side the slope goes down to).
enum Dir { PZ, NZ, PX, NX };

struct BrickDef {
    int x0, z0, fx, fz;   // footprint (studs)
    int layer;
    int role;
    bool slope;
    float yaw;            // the brick's turn about y: its long axis is its local x, its slope goes down to local +z
    int len, wid;         // long and short sides (studs)
    int group;            // 1: turns with the windmill's sails
};

struct ModelDef {
    const char* name;
    int W, D, layers;
    int baseRole;
    std::vector<BrickDef> bricks;
    float hubX = 0, hubY = 0, hubZ = 0; // the sails' hub (studs, layers), group 1 turns about z through it
};

struct Grid {
    static constexpr int MAXL = 18, MAXW = 10;
    int W = 10, D = 10;
    int off = 0;          // painting offset (models drawn on a 10 x 10 grid, kept on 8 x 8)
    signed char role[MAXL][MAXW][MAXW];
    signed char kind[MAXL][MAXW][MAXW]; // 0 empty, 1 brick, 2 slope
    signed char dir[MAXL][MAXW][MAXW];
    signed char group[MAXL][MAXW][MAXW];
    Grid(int w, int d, int o = 0) : W(w), D(d), off(o) {
        for (int l = 0; l < MAXL; l++)
            for (int z = 0; z < MAXW; z++)
                for (int x = 0; x < MAXW; x++) { role[l][z][x] = -1; kind[l][z][x] = 0; dir[l][z][x] = 0; group[l][z][x] = 0; }
    }
    bool in(int x, int l, int z) const { return x >= 0 && z >= 0 && l >= 0 && x < W && z < D && l < MAXL; }
    void set(int x, int l, int z, int r, int g = 0) {
        x += off; z += off;
        if (!in(x, l, z)) return;
        role[l][z][x] = (signed char)r; kind[l][z][x] = 1; group[l][z][x] = (signed char)g;
    }
    void clear(int x, int l, int z) {
        x += off; z += off;
        if (in(x, l, z)) { kind[l][z][x] = 0; role[l][z][x] = -1; }
    }
    void slope(int x, int l, int z, int r, int d) {
        x += off; z += off;
        if (!in(x, l, z)) return;
        role[l][z][x] = (signed char)r; kind[l][z][x] = 2; dir[l][z][x] = (signed char)d; group[l][z][x] = 0;
    }
    void fill(int x0, int z0, int x1, int z1, int l, int r) {
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) set(x, l, z, r);
    }
    void ring(int x0, int z0, int x1, int z1, int l, int r) {
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (x == x0 || x == x1 || z == z0 || z == z1) set(x, l, z, r);
    }
    // a ring with its four corners left out (a rounder tower)
    void ringRound(int x0, int z0, int x1, int z1, int l, int r) {
        ring(x0, z0, x1, z1, l, r);
        clear(x0, l, z0); clear(x1, l, z0); clear(x0, l, z1); clear(x1, l, z1);
    }
    void fillRound(int x0, int z0, int x1, int z1, int l, int r) {
        fill(x0, z0, x1, z1, l, r);
        clear(x0, l, z0); clear(x1, l, z0); clear(x0, l, z1); clear(x1, l, z1);
    }
    // a ring of slopes going down outward (a roof course); the corners belong to the front and back rows
    void slopeRing(int x0, int z0, int x1, int z1, int l, int r) {
        for (int x = x0; x <= x1; x++) { slope(x, l, z0, r, NZ); slope(x, l, z1, r, PZ); }
        for (int z = z0 + 1; z < z1; z++) { slope(x0, l, z, r, NX); slope(x1, l, z, r, PX); }
    }
};

// Cuts a run of n studs into brick lengths (1..4), the bond offset on alternate courses.
inline std::vector<int> chop(int n, bool off) {
    std::vector<int> v;
    int rest = n;
    if (off && n > 3) { v.push_back(2); rest -= 2; }
    while (rest > 0) {
        const int k = rest >= 6 ? 4 : (rest == 5 ? 3 : (rest == 4 ? 4 : rest));
        v.push_back(k);
        rest -= k;
    }
    return v;
}

inline ModelDef brickify(const char* name, const Grid& g, int baseRole) {
    ModelDef m;
    m.name = name;
    m.W = g.W;
    m.D = g.D;
    m.baseRole = baseRole;
    m.layers = 0;
    for (int l = 0; l < Grid::MAXL; l++) {
        bool used[Grid::MAXW][Grid::MAXW] = {};
        struct Run { int a0, b, n, role, kind, dir, group; bool alongZ; };
        std::vector<BrickDef> course;
        for (int pass = 0; pass < 2; pass++) {
            // pass 0: bricks along the course's axis; slopes along their row's axis; pass 1 (the other axis) for
            // what's left (a column of single studs)
            const bool evenAxisZ = (l % 2) == 1;
            for (int b = 0; b < Grid::MAXW; b++) {
                for (int a = 0; a < Grid::MAXW;) {
                    const bool alongZ = pass == 0 ? evenAxisZ : !evenAxisZ;
                    const int x = alongZ ? b : a, z = alongZ ? a : b;
                    if (!g.in(x, l, z) || g.kind[l][z][x] == 0 || used[z][x]) { a++; continue; }
                    const int k = g.kind[l][z][x];
                    // a slope's row runs across its direction
                    if (k == 2) {
                        const int d = g.dir[l][z][x];
                        const bool rowZ = d == PX || d == NX;
                        if (rowZ != alongZ) { a++; continue; }
                    }
                    const int r = g.role[l][z][x], dd = g.dir[l][z][x], gr = g.group[l][z][x];
                    int n = 0;
                    for (;;) {
                        const int xx = alongZ ? b : a + n, zz = alongZ ? a + n : b;
                        if (!g.in(xx, l, zz) || used[zz][xx] || g.kind[l][zz][xx] != k || g.role[l][zz][xx] != r ||
                            g.group[l][zz][xx] != gr || (k == 2 && g.dir[l][zz][xx] != dd))
                            break;
                        n++;
                    }
                    // pass 0 leaves single studs to pass 1 when a longer run exists across
                    if (pass == 0 && n == 1 && k == 1) {
                        const int xa = alongZ ? b + 1 : a, za = alongZ ? a : b + 1;
                        const int xb = alongZ ? b - 1 : a, zb = alongZ ? a : b - 1;
                        auto sameAt = [&](int xx, int zz) {
                            return g.in(xx, l, zz) && !used[zz][xx] && g.kind[l][zz][xx] == 1 && g.role[l][zz][xx] == r &&
                                   g.group[l][zz][xx] == gr;
                        };
                        if (sameAt(xa, za) || sameAt(xb, zb)) { a++; continue; }
                    }
                    if (gr == 1) {
                        // the sails: one stud each (they turn as a whole, but stay single bricks)
                        n = 1;
                    }
                    const std::vector<int> parts = chop(n, ((a + b + l) & 1) != 0);
                    int at = a;
                    for (int len : parts) {
                        BrickDef bd;
                        bd.layer = l;
                        bd.role = r;
                        bd.slope = k == 2;
                        bd.len = len;
                        bd.wid = 1;
                        bd.group = gr;
                        if (alongZ) { bd.x0 = b; bd.z0 = at; bd.fx = 1; bd.fz = len; }
                        else { bd.x0 = at; bd.z0 = b; bd.fx = len; bd.fz = 1; }
                        // the turn: long axis along x (0) or z (+-90 degrees), the slope going down to its side
                        if (!alongZ) bd.yaw = (k == 2 && dd == NZ) ? 3.14159265f : 0.f;
                        else bd.yaw = (k == 2 && dd == NX) ? -1.5707963f : 1.5707963f;
                        course.push_back(bd);
                        for (int i = 0; i < len; i++) {
                            const int xx = alongZ ? b : at + i, zz = alongZ ? at + i : b;
                            used[zz][xx] = true;
                        }
                        at += len;
                    }
                    a += n;
                }
            }
        }
        // pairs of equal bricks side by side become 2 x N bricks
        for (size_t i = 0; i < course.size(); i++) {
            BrickDef& p = course[i];
            if (p.slope || p.len < 2 || p.wid != 1 || p.group) continue;
            for (size_t j = i + 1; j < course.size(); j++) {
                BrickDef& q = course[j];
                if (q.slope || q.wid != 1 || q.len != p.len || q.role != p.role || q.group) continue;
                const bool alongZ = p.fz > p.fx;
                if (alongZ != (q.fz > q.fx)) continue;
                if (alongZ) {
                    if (q.z0 != p.z0 || std::abs(q.x0 - p.x0) != 1) continue;
                    p.x0 = std::min(p.x0, q.x0); p.fx = 2;
                } else {
                    if (q.x0 != p.x0 || std::abs(q.z0 - p.z0) != 1) continue;
                    p.z0 = std::min(p.z0, q.z0); p.fz = 2;
                }
                p.wid = 2;
                course.erase(course.begin() + j);
                break;
            }
        }
        if (course.empty()) continue;
        m.layers = l + 1;
        // around the model, a course at a time
        const float cx = g.W * 0.5f, cz = g.D * 0.5f;
        std::vector<std::pair<float, int>> ord;
        for (size_t i = 0; i < course.size(); i++) {
            const BrickDef& b = course[i];
            float ang = std::atan2((b.z0 + b.fz * 0.5f) - cz, (b.x0 + b.fx * 0.5f) - cx);
            if (b.slope) ang += 10.f; // a course's slopes after its bricks
            ord.push_back({ang, (int)i});
        }
        std::sort(ord.begin(), ord.end());
        for (auto& o : ord) m.bricks.push_back(course[o.second]);
    }
    return m;
}

// ---------------------------------------------------------------- the models

inline ModelDef cottage() {
    Grid g(8, 8, -1);
    // walls
    for (int l = 0; l < 4; l++) g.ring(2, 2, 7, 6, l, WHT);
    g.set(4, 0, 6, RED); g.set(4, 1, 6, RED); g.set(5, 0, 6, RED); g.set(5, 1, 6, RED);    // the door
    for (int l = 1; l <= 2; l++) {
        g.set(2, l, 6, CYA); g.set(7, l, 6, CYA);                                            // windows
        g.set(3, l, 2, CYA); g.set(6, l, 2, CYA);
        g.set(2, l, 4, CYA); g.set(7, l, 4, CYA);
    }
    g.ring(2, 2, 7, 6, 3, WHT);
    // the gabled roof, the ridge along x
    for (int x = 1; x <= 8; x++) {
        g.slope(x, 4, 1, RED, NZ); g.slope(x, 4, 7, RED, PZ);
        g.slope(x, 5, 2, RED, NZ); g.slope(x, 5, 6, RED, PZ);
        g.slope(x, 6, 3, RED, NZ); g.slope(x, 6, 5, RED, PZ);
        g.set(x, 6, 4, RED);
    }
    for (int z = 2; z <= 6; z++) { g.set(2, 4, z, WHT); g.set(7, 4, z, WHT); }
    for (int z = 3; z <= 5; z++) { g.set(2, 5, z, WHT); g.set(7, 5, z, WHT); }
    // the chimney
    g.set(6, 5, 3, DRK); g.set(6, 6, 3, DRK); g.set(6, 7, 3, DRK);
    // a hedge and flowers in front
    g.set(7, 0, 8, GRN); g.set(8, 0, 8, GRN); g.set(8, 1, 8, GRN); g.set(7, 0, 7, GRN);
    g.set(1, 0, 8, YEL); g.set(2, 0, 8, ORA); g.set(1, 0, 7, PUR);
    return brickify("COTTAGE", g, GRN);
}

inline ModelDef lighthouse() {
    Grid g(8, 8, -1);
    g.fillRound(1, 1, 8, 8, 0, GRY);                       // the rock
    g.clear(1, 0, 2); g.clear(8, 0, 7); g.clear(2, 0, 8);
    for (int l = 1; l <= 9; l++) g.ringRound(3, 3, 6, 6, l, ((l - 1) / 2) % 2 == 0 ? RED : WHT);
    g.ring(3, 3, 6, 6, 1, RED);
    g.set(4, 1, 6, BLU); g.set(4, 2, 6, BLU);              // the door
    g.set(5, 5, 3, CYA); g.set(4, 7, 6, CYA);              // windows
    g.ringRound(2, 2, 7, 7, 10, DRK);                       // the gallery
    g.ring(3, 3, 6, 6, 10, DRK);
    g.ringRound(3, 3, 6, 6, 11, YEL);                       // the lamp
    g.ringRound(3, 3, 6, 6, 12, YEL);
    g.slopeRing(3, 3, 6, 6, 13, RED);                       // the cap
    g.fill(4, 4, 5, 5, 13, RED);
    g.fill(4, 4, 5, 5, 14, DRK);
    g.set(4, 15, 4, DRK);
    return brickify("LIGHTHOUSE", g, BLU);
}

inline ModelDef appleTree() {
    Grid g(8, 8);
    for (int l = 0; l < 4; l++) g.fill(3, 3, 4, 4, l, TAN);
    g.fillRound(1, 1, 6, 6, 4, GRN);
    for (int l = 5; l <= 6; l++) {
        g.fill(0, 1, 7, 6, l, GRN);
        g.fill(1, 0, 6, 7, l, GRN);
    }
    g.fillRound(1, 1, 6, 6, 7, GRN);
    g.fillRound(2, 2, 5, 5, 8, GRN);
    g.fill(3, 3, 4, 4, 9, GRN);
    // apples on the canopy
    g.set(0, 5, 2, RED); g.set(7, 6, 5, RED); g.set(3, 6, 0, RED); g.set(5, 5, 7, RED); g.set(2, 7, 6, RED);
    g.set(6, 7, 2, RED); g.set(4, 8, 5, RED); g.set(1, 4, 4, RED);
    // grass and a flower at its foot
    g.set(6, 0, 1, YEL); g.set(1, 0, 6, ORA);
    return brickify("APPLE TREE", g, GRN);
}

inline ModelDef windmill() {
    Grid g(8, 8, -1);
    for (int l = 0; l < 3; l++) g.ringRound(2, 2, 7, 7, l, TAN);
    g.set(4, 0, 7, RED); g.set(5, 0, 7, RED); g.set(4, 1, 7, RED); g.set(5, 1, 7, RED);
    g.ring(2, 2, 7, 7, 3, TAN);
    for (int l = 4; l < 9; l++) g.ring(3, 3, 6, 6, l, WHT);
    g.set(3, 5, 4, CYA); g.set(6, 6, 5, CYA);
    g.slopeRing(3, 3, 6, 6, 9, RED);
    g.fill(4, 4, 5, 5, 9, RED);
    g.fill(4, 4, 5, 5, 10, RED);
    // the hub and the sails (they turn)
    g.set(4, 7, 7, DRK); g.set(5, 7, 7, DRK);
    for (int k = 1; k <= 3; k++) {
        g.set(4, 7 + k, 8, k % 2 ? ORA : WHT, 1);
        g.set(4, 7 - k, 8, k % 2 ? ORA : WHT, 1);
        g.set(4 + k, 7, 8, k % 2 ? ORA : WHT, 1);
        g.set(4 - k, 7, 8, k % 2 ? ORA : WHT, 1);
    }
    g.set(4, 7, 8, DRK, 1);
    ModelDef m = brickify("WINDMILL", g, GRN);
    m.hubX = 3.5f; m.hubY = 7.5f; m.hubZ = 7.5f;
    return m;
}

inline ModelDef castleTower() {
    Grid g(8, 8);
    for (int l = 0; l < 9; l++) g.ring(1, 1, 6, 6, l, GRY);
    g.set(3, 0, 6, DRK); g.set(4, 0, 6, DRK); g.set(3, 1, 6, DRK); g.set(4, 1, 6, DRK); // the gate
    g.set(3, 4, 1, DRK); g.set(6, 4, 4, DRK); g.set(1, 6, 3, DRK); g.set(4, 6, 6, DRK);  // arrow slits
    g.ring(1, 1, 6, 6, 5, BLU);                                                          // a painted band
    g.fill(2, 2, 5, 5, 8, GRY);
    for (int x = 0; x < 8; x++)
        for (int z = 0; z < 8; z++)
            if ((x == 1 || x == 6 || z == 1 || z == 6) && x >= 1 && x <= 6 && z >= 1 && z <= 6 && (x + z) % 2 == 0)
                g.set(x, 9, z, GRY);
    for (int l = 9; l <= 12; l++) g.set(3, l, 3, DRK);   // the flag
    g.set(4, 11, 3, RED); g.set(5, 11, 3, RED); g.set(4, 12, 3, RED);
    return brickify("TOWER", g, GRN);
}

inline ModelDef rocket() {
    Grid g(8, 8);
    // fins
    for (int l = 0; l < 2; l++) {
        g.set(3, l, 0, RED); g.set(4, l, 0, RED); g.set(3, l, 7, RED); g.set(4, l, 7, RED);
        g.set(0, l, 3, RED); g.set(0, l, 4, RED); g.set(7, l, 3, RED); g.set(7, l, 4, RED);
        g.set(3, l, 1, RED); g.set(4, l, 1, RED); g.set(3, l, 6, RED); g.set(4, l, 6, RED);
        g.set(1, l, 3, RED); g.set(1, l, 4, RED); g.set(6, l, 3, RED); g.set(6, l, 4, RED);
    }
    for (int x = 3; x <= 4; x++) { g.slope(x, 2, 1, RED, NZ); g.slope(x, 2, 6, RED, PZ); }
    for (int z = 3; z <= 4; z++) { g.slope(1, 2, z, RED, NX); g.slope(6, 2, z, RED, PX); }
    for (int l = 0; l < 11; l++) g.ringRound(2, 2, 5, 5, l, l == 4 || l == 5 ? BLU : WHT);
    for (int l = 0; l < 2; l++) g.ring(2, 2, 5, 5, l, WHT);
    g.set(3, 7, 5, CYA); g.set(4, 7, 5, CYA); g.set(3, 7, 2, CYA); g.set(4, 7, 2, CYA);   // windows
    g.set(3, 8, 5, CYA); g.set(4, 8, 5, CYA);
    g.slopeRing(2, 2, 5, 5, 11, RED);
    g.clear(2, 11, 2); g.clear(5, 11, 2); g.clear(2, 11, 5); g.clear(5, 11, 5);
    g.fill(3, 3, 4, 4, 11, RED);
    g.fill(3, 3, 4, 4, 12, RED);
    g.set(3, 13, 3, YEL);
    return brickify("ROCKET", g, DRK);
}

inline ModelDef pagoda() {
    Grid g(8, 8, -1);
    int l = 0;
    for (int tier = 0; tier < 3; tier++) {
        const int a = 3 + (tier == 2 ? 1 : 0), b = 6 - (tier == 2 ? 1 : 0);
        for (int k = 0; k < 2; k++, l++) {
            g.ring(a, a, b, b, l, RED);
            if (tier < 2 && k == 0) { g.set(4, l, b, YEL); g.set(5, l, b, YEL); }
        }
        g.slopeRing(a - 1, a - 1, b + 1, b + 1, l, GRN);
        g.ring(a, a, b, b, l, GRN);
        l++;
    }
    g.fill(4, 4, 5, 5, l, YEL);
    g.set(4, l + 1, 4, YEL);
    g.set(4, l + 2, 4, ORA);
    return brickify("PAGODA", g, TAN);
}

inline ModelDef duck() {
    Grid g(8, 8);
    g.fillRound(1, 1, 6, 6, 0, YEL);
    g.fillRound(2, 1, 5, 6, 1, YEL);
    g.fill(3, 1, 4, 2, 2, YEL);                 // the tail
    g.slope(3, 3, 1, YEL, NZ); g.slope(4, 3, 1, YEL, NZ);
    for (int ll = 2; ll <= 4; ll++) g.fill(3, 4, 4, 5, ll, YEL); // the head
    g.set(3, 4, 5, DRK); g.set(4, 4, 5, DRK);   // eyes
    g.set(3, 5, 4, YEL); g.set(4, 5, 4, YEL);
    g.set(3, 3, 6, ORA); g.set(4, 3, 6, ORA);   // the beak
    g.set(1, 1, 3, WHT); g.set(6, 1, 3, WHT);   // wings
    g.set(1, 1, 4, WHT); g.set(6, 1, 4, WHT);
    return brickify("DUCK", g, BLU);
}

inline ModelDef sailboat() {
    Grid g(8, 8, -1);
    g.fill(2, 4, 7, 5, 0, TAN);
    g.fill(1, 3, 8, 6, 1, BLU);
    g.fill(1, 3, 8, 6, 2, WHT);
    g.fill(2, 4, 7, 5, 2, TAN);
    for (int ll = 3; ll <= 11; ll++) g.set(4, ll, 4, DRK);    // the mast
    for (int ll = 3; ll <= 10; ll++)                            // the mainsail
        for (int x = 5; x <= 5 + (10 - ll) / 2 && x <= 8; x++) g.set(x, ll, 4, WHT);
    for (int ll = 3; ll <= 7; ll++)                             // the jib
        for (int x = 3; x >= 3 - (7 - ll) / 2 && x >= 1; x--) g.set(x, ll, 4, ll % 2 ? YEL : WHT);
    g.set(5, 11, 4, RED);                                       // the pennant
    return brickify("SAILBOAT", g, CYA);
}

inline const std::vector<ModelDef>& all() {
    static const std::vector<ModelDef> v = {cottage(), lighthouse(), appleTree(), windmill(), castleTower(),
                                            rocket(),  pagoda(),     duck(),      sailboat()};
    return v;
}

} // namespace bricks_models

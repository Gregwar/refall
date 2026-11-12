// BIOME's sea: glass cubes of water over a rising sea, a 3D diorama.
//
// One world in 3D, seen by a fixed lens raised above the sea and shifted back down (so the board plane keeps the
// layout's pixels while the world behind the well is seen from above): a sandy bed of low dunes, a beach under the
// well sloping down to the deep far out, cut away just behind the board plane like a diorama, so its section shows
// the sand's layers and, once the sea is up, the water's depth (an aquarium's side); the sea a real body of water over
// it, its surface a swell under the sky (glassy at calm, a gentle swell at mid, whitecaps in a chorus, a storm on a
// drop), reflecting the sky by Fresnel, the sun's or moon's path glittering on it; under it the light dims and turns
// to the sea's hue with depth, long soft shafts slant down from the surface and caustics play on the sand; far out a
// haze melts the sea into the sky. Sun or moon light with soft shadows (a shadow map). Boats are low-poly ships
// sailing across, bigger as the sea deepens; life fills the water in layers (fish, jellyfish, deep glowing life).
//
// The stack stays a clean, readable grid at the layout's pixels in front of it all: every cell its own small solid
// glass cube full to the brim with the sea's water, under pressure, traced per pixel through the glass and the water
// (render.cpp, CELL_FS), drawn over the world behind a calm veil (the well's pane).
//
// The song starts with the sea low, far out. A cleared row's cubes are thrown out of the well into the world behind
// it: rigid bodies tumbling on all axes (real physics: gravity, contacts on the dunes, friction, bounces, collisions
// with the cubes already lying there), landing on the sand or in the sea. Each cracks at the very point where it first
// hits, and its water jets out of the crack under pressure, along the outward normal there (turned by the ground), as
// 3D spray running over the sand to the sea or churning out as a plume under it; the sea rises as that water lands,
// linearly (about sixty lines to fill it). Empty, the cube sinks into the sand (each gone three to five seconds after
// it first hit). Everything thrown or poured stays behind the well: nothing is ever drawn over the stack.
//
// A drop raises a storm, eased in over about a second: big waves with whitecaps and spray, rain, dark clouds,
// lightning in them on its first downbeat and every four bars. Slow motion brings dusk and a glassy sea, game over
// cracks the stack row by row and throws it out, a change of universe fades the world to the bridge.
//
// Layout contract: the board plane (z = 0) falls exactly on the layout's reference camera (the shift lens).
#include "universes/biome/sea.hpp"

#include <algorithm>
#include <cstdio>

#include "layout.hpp"
#include "universes/biome/gl.hpp"

namespace biome {

using namespace sea;

namespace {

float hashf(float a, float b) {
    const uint64_t h = splitmix64((uint64_t)(int64_t)std::lround(a * 1000.f) * 73856093ull ^ (uint64_t)(int64_t)std::lround(b * 1000.f) * 19349663ull);
    return (h >> 40) * (1.0f / 16777216.0f);
}

// The variants (sRGB): the sky (top, low), the sun or moon, the clouds, the storm's sky and clouds; the sea's water
// (far, middle, near), its depths, foam, haze and storm water; the well's panel; the boats (hull, superstructure,
// sails, accent, windows, three container colors); the HUD ink, the accent, the dusk of slow motion; the seven waters
// (the sea's life); the sun as the stack's cubes see it (x, y, radius), moon 0/1, stars, the panel's opacity,
// cloudiness, a light panel, night (lit windows); the 3D sun (azimuth, elevation), its strength, the sky's light,
// the shadows' depth.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, sun, cloud, stormSky, stormCloud;
    unsigned sea0, sea1, sea2, deep, foam, haze, stormSea, panel;
    unsigned hull, super_, sail, baccent, window, cont0, cont1, cont2;
    unsigned ink, accent, dusk;
    unsigned piece[7];
    float sunX, sunY, sunR, moon, stars, panelA, clouds, lightPanel, night;
    float sunAz, sunEl, lightK, ambK, shadowK;
};
const VariantDef VARIANTS[] = {
    // A calm noon: a clear blue sky, a bright sea, white sails; the sun high behind the eye.
    {"CALM NOON", 0x6aa6dc, 0xe2eef2, 0xfffbea, 0xffffff, 0x5a6472, 0x3e4652,
     0xa2ccd8, 0x4f9fc0, 0x23709c, 0x0f3d62, 0xf6fbfb, 0xd6e8ee, 0x3a5462, 0x0a1422,
     0x22344a, 0xf3f1ea, 0xfbfaf4, 0xd0583c, 0x2c4458, 0xd0583c, 0x3d7fa6, 0xe0b040,
     0x1b3348, 0x1f88b8, 0x2a2f5a,
     {0x14c0cc, 0x1e56b0, 0x7a4cc8, 0x1fa676, 0x0a7a84, 0x6aa8f4, 0x8ad6a8},
     -14.f, 8.5f, 1.0f, 0.f, 0.f, 0.3f, 0.3f, 1.f, 0.f,
     -140.f, 52.f, 1.05f, 0.62f, 0.7f},
    // A golden hour: a low sun in an apricot haze over the sea, its path glittering to the shore.
    {"GOLDEN HOUR", 0x34446e, 0xf4b47c, 0xffe4b4, 0xf2a888, 0x3a3248, 0x2a2534,
     0xe0aa88, 0x8a7c94, 0x3a4870, 0x1a2242, 0xffe8d4, 0xf2c6a0, 0x2c3048, 0x1a2140,
     0x1d1f33, 0xf6e6d2, 0xfff1e0, 0xe0603a, 0xffd08a, 0xe0603a, 0x3d6f96, 0xe8b24a,
     0xfbeee0, 0xffb36a, 0x241a3c,
     {0x4ae0ea, 0x3c8af2, 0x8e7cf6, 0x48d69a, 0x20b2b0, 0x90caff, 0xb8f2d0},
     15.5f, -1.0f, 1.6f, 0.f, 0.f, 0.62f, 0.35f, 0.f, 0.35f,
     21.f, -0.22f, 1.15f, 0.5f, 0.6f},
    // A grey North Sea: an overcast sky, grey-green water, red fishing hulls; soft light from above.
    {"GREY NORTH SEA", 0x98a4ac, 0xd6dbd8, 0xe4e6e2, 0xc6cccd, 0x4a5254, 0x343b3e,
     0x9aaaa6, 0x5f7571, 0x34494a, 0x18282c, 0xeef2ee, 0xc8d0ce, 0x26363a, 0x0c1416,
     0x2a3236, 0xe8e6de, 0xd8d4c8, 0xc8452e, 0x34404a, 0xb8503a, 0x4e6e80, 0xd09a3c,
     0x232c30, 0x3c8a86, 0x2a3040,
     {0x16a2b6, 0x2a5a90, 0x5a54a6, 0x2e9a64, 0x0e6a6c, 0x5c98d0, 0x7cc496},
     -12.f, 7.f, 0.95f, 0.f, 0.f, 0.3f, 0.75f, 1.f, 0.f,
     150.f, 42.f, 0.5f, 0.9f, 0.3f},
    // A night harbor: a deep blue night, a moon low over the sea, boats with lit windows; luminous water in the glass.
    {"NIGHT HARBOR", 0x070d22, 0x1c2a4c, 0xe8ecf4, 0x2a3658, 0x0a0f1c, 0x1a2236,
     0x26385c, 0x17284c, 0x0d1a38, 0x060c1c, 0xa4b8d4, 0x34466c, 0x0c1426, 0x0c1428,
     0x0a1020, 0x2e3c5a, 0x5c6c8c, 0x6a3a40, 0xffc870, 0x4a3040, 0x2a4058, 0x5a5030,
     0xdce6f4, 0x8ad8ff, 0x101030,
     {0x6ee6f2, 0x5c9cff, 0xac9cff, 0x6eeab0, 0x3ccaca, 0xa4ccff, 0xc4f6da},
     13.f, 8.f, 0.8f, 1.f, 1.f, 0.55f, 0.25f, 0.f, 1.f,
     18.f, -0.55f, 0.5f, 0.42f, 0.45f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The cubes' water: the sea's own color (its near water's hue, a little more saturated, light), each piece type a
// small shift around it (hue, brightness, saturation) so the pieces stay apart in the stack and what pours out of
// them is the sea's water.
vec3 seaWater(unsigned seaHex, int type, float night) {
    const vec3 c = hex(seaHex);
    const float mx = std::max({c.x, c.y, c.z}), mn = std::min({c.x, c.y, c.z}), dl = mx - mn;
    float h = 0.f;
    if (dl > 1e-5f) {
        if (mx == c.x) h = std::fmod((c.y - c.z) / dl + 6.f, 6.f);
        else if (mx == c.y) h = (c.z - c.x) / dl + 2.f;
        else h = (c.x - c.y) / dl + 4.f;
    }
    h *= 60.f;
    const float sat = mx > 1e-5f ? dl / mx : 0.f;
    //                              hue shift (degrees), brightness, saturation
    static const float SH[7][3] = {{0.f, 1.f, 1.f},     {-18.f, 0.84f, 1.12f}, {18.f, 1.1f, 0.88f}, {-9.f, 1.16f, 0.72f},
                                   {9.f, 0.78f, 1.2f},  {-20.f, 1.14f, 0.85f}, {20.f, 0.86f, 1.16f}};
    // at night the water is luminous: moonlit, a little toward cyan, brighter and more saturated than the dark sea
    const float hh = std::fmod(h - 22.f * night + SH[type][0] + 360.f, 360.f) / 60.f;
    const float S = std::clamp(std::clamp(sat, 0.45f + 0.15f * night, 0.75f) * SH[type][2], 0.f, 1.f);
    const float V = std::clamp((0.82f + 0.14f * night) * SH[type][1], 0.f, 1.f);
    const float C = V * S, X = C * (1.f - std::fabs(std::fmod(hh, 2.f) - 1.f)), m = V - C;
    vec3 r;
    if (hh < 1.f) r = vec3(C, X, 0.f);
    else if (hh < 2.f) r = vec3(X, C, 0.f);
    else if (hh < 3.f) r = vec3(0.f, C, X);
    else if (hh < 4.f) r = vec3(0.f, X, C);
    else if (hh < 5.f) r = vec3(X, 0.f, C);
    else r = vec3(C, 0.f, X);
    return ugl::toLinear(vec3(r.x + m, r.y + m, r.z + m));
}

// ---- quaternions (x, y, z, w)
vec3 qRotInv(const float q[4], const vec3& v) {
    const float c[4] = {-q[0], -q[1], -q[2], q[3]};
    return qRot(c, v);
}
void qIntegrate(float q[4], const vec3& w, float h) {
    const float k = 0.5f * h;
    const float x = q[0], y = q[1], z = q[2], s = q[3];
    q[0] = x + k * (w.x * s + w.y * z - w.z * y);
    q[1] = y + k * (w.y * s + w.z * x - w.x * z);
    q[2] = z + k * (w.z * s + w.x * y - w.y * x);
    q[3] = s + k * (-w.x * x - w.y * y - w.z * z);
    const float l = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; i++) q[i] /= l;
}

// a cube's half size (collisions) and its inertia (a solid cube of mass 1: isotropic)
constexpr float CH = 0.44f;
constexpr float INERTIA = (2.f * CH) * (2.f * CH) / 6.f;

} // namespace

Palette SeaWorld::palette(int v) const {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    Palette p{};
    p.skyTop = lin(d.skyTop); p.skyLow = lin(d.skyLow); p.sun = lin(d.sun); p.cloud = lin(d.cloud); p.stormSky = lin(d.stormSky);
    p.stormCloud = lin(d.stormCloud); p.sea0 = lin(d.sea0); p.sea1 = lin(d.sea1); p.sea2 = lin(d.sea2); p.deep = lin(d.deep);
    p.foam = lin(d.foam); p.haze = lin(d.haze); p.stormSea = lin(d.stormSea); p.panel = lin(d.panel);
    p.hull = lin(d.hull); p.super_ = lin(d.super_); p.sail = lin(d.sail); p.baccent = lin(d.baccent); p.window = lin(d.window);
    p.cont0 = lin(d.cont0); p.cont1 = lin(d.cont1); p.cont2 = lin(d.cont2);
    p.ink = lin(d.ink); p.accent = lin(d.accent); p.dusk = lin(d.dusk);
    for (int i = 0; i < 7; i++) p.piece[i] = lin(d.piece[i]);
    for (int i = 0; i < 7; i++) p.cell[i] = seaWater(d.sea2, i, d.night);
    p.sunX = d.sunX; p.sunY = d.sunY; p.sunR = d.sunR; p.moon = d.moon; p.stars = d.stars; p.panelA = d.panelA;
    p.clouds = d.clouds; p.lightPanel = d.lightPanel; p.night = d.night;
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.lightK = d.lightK; p.ambK = d.ambK; p.shadowK = d.shadowK;
    // the well's edge, the HUD (the day variants veil the well in a dark glass, but their HUD panels stay light; the ink
    // dark over a light sky or panel, pale over the sea)
    p.edge = lerp(p.panel, p.haze, 0.5f);
    p.hudPanel = d.lightPanel > 0.5f ? lin(0xf0f5f6) : p.panel;
    const bool inkDark = ugl::luma(p.ink) < 0.2f;
    p.inkDark = inkDark ? p.ink : lin(0x1b2a38);
    p.inkLight = inkDark ? lin(0xf2f5f4) : p.ink;
    // the cubes reflect the sea under the horizon; the gauge fills with the first water
    p.envLow = p.sea2 * 0.5f;
    p.gauge = p.piece[0];
    return p;
}
int sea::variantCount() { return VARIANT_COUNT; }
const char* sea::variantName(int i) { return VARIANTS[i].name; }

// =====================================================================================================
// Song, level
// =====================================================================================================

// The sea from the song: bright major songs at noon or at the golden hour; minor songs on the grey North Sea or in a
// night harbor; the key's hue picks among them.
int SeaWorld::pickVariant(const SongLook& t) const {
    float h = std::fmod(t.hue / TAU, 1.f);
    if (h < 0) h += 1.f;
    int v;
    if (t.hasSong && !t.minor) v = h < 0.5f ? 0 : 1;
    else if (t.hasSong && t.minor) v = h < 0.5f ? 2 : 3;
    else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
    if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    return v;
}

void SeaWorld::setSong(const SongLook& look, float seconds, bool fresh) {
    dir_ = (look.seed >> 7) & 1 ? 1.f : -1.f;
    // a new song: the sea goes back down (at once before the first frame, else it drains over a few seconds)
    waterLines_ = 0.f;
    if (seconds <= 0.f || fresh) {
        rise_ = 0.f;
        boats_.clear();
        critters_.clear();
        bodies_.clear();
        parts_.clear();
    }
}

void SeaWorld::dropHit() {
    // a gust tearing spray off the crests far out
    for (int i = 0; i < 4; i++) {
        float x, z;
        sprayAt(x, z);
        sprayBurst(x, z, 18, 1.3f);
    }
    // the life under it startles: the fish dart away
    for (Critter& c : critters_)
        if (c.kind == L_FISH) {
            c.dart = 1.f;
            c.dyv = ctx.rng.range(-1.6f, 1.6f);
        }
}

// a kick at a peak: spray torn off the crests
void SeaWorld::kick() {
    float x, z;
    if (ctx.storm > 0.5f) {
        for (int i = 0; i < 2; i++) {
            sprayAt(x, z);
            sprayBurst(x, z, 12, 1.2f);
        }
    } else {
        sprayAt(x, z);
        sprayBurst(x, z, 5, 0.6f);
    }
}

// a spot of the sea for spray, at the sides of the well
void SeaWorld::sprayAt(float& x, float& z) {
    z = -ctx.rng.range(2.f, 22.f);
    x = (ctx.rng.chance(0.5f) ? -1.f : 1.f) * ctx.rng.range(7.f, ctx.hwAt(z) - 1.f);
}

// =====================================================================================================
// The sea
// =====================================================================================================


// The swell (WORLD_GLSL's waveH).
float SeaWorld::waveH(float x, float z) const {
    const float A = amp_ * calmAt(x, z);
    auto nd = [](float a, float b) { const float l = std::sqrt(a * a + b * b); return v2(a / l, b / l); };
    const vec2 d1 = nd(dir_, 0.42f), d2 = nd(dir_ * 0.7f, -0.7f), d3 = nd(dir_ * 0.25f, 1.f), d4 = nd(-dir_, 0.6f);
    const float t = waveT_;
    const float s1 = crest(std::sin((x * d1.x + z * d1.y) * 0.42f - t), sharp_);
    const float s2 = crest(std::sin((x * d2.x + z * d2.y) * 0.93f - t * 1.37f + 1.3f), sharp_);
    const float s3 = std::sin((x * d3.x + z * d3.y) * 2.1f - t * 0.8f + 0.9f);
    const float s4 = std::sin((x * d4.x + z * d4.y) * 3.7f - t * 1.9f + 2.f);
    return A * (0.55f * s1 + 0.32f * s2 + 0.13f * s3 + 0.06f * s4);
}

// A boat comes in from one side (or is already out there when the song starts): the deeper the sea, the bigger; the
// small ones near, the big ships far out, each where the water is deep enough for it.
bool SeaWorld::spawnBoat(bool onScreen) {
    if (boats_.size() >= 9) return false;
    const float S = seaLevel();
    if (S < BOAT_LEVEL) return false;
    const int maxT = std::clamp((int)(1.f + (S - BOAT_LEVEL) / 2.6f), 1, 5);
    const int minT = std::max(0, maxT - 2);
    const int type = ctx.rng.chance(0.45f) ? maxT : ctx.rng.irange(minT, maxT);
    static const float Z0[6] = {8.f, 10.f, 16.f, 45.f, 75.f, 85.f}, Z1[6] = {30.f, 40.f, 60.f, 110.f, 160.f, 170.f};
    static const float DRAFT[6] = {0.45f, 0.6f, 0.8f, 1.1f, 1.3f, 1.3f};
    static const float SPEED[6] = {0.6f, 0.9f, 1.0f, 1.6f, 2.2f, 2.6f};
    float z = -ctx.rng.range(Z0[type], Z1[type]);
    // out to where it floats (over the highest dunes at that distance)
    auto deep = [&](float zz) {
        const float base = -9.2f - 3.6f * smoothstepf(3.f, 48.f, -zz) + FAR_RISE * farT(zz), k = 1.f - 0.4f * smoothstepf(10.f, 60.f, -zz);
        return S - (base + 1.3f * k) > DRAFT[type];
    };
    for (int i = 0; i < 6 && !deep(z); i++) z = -ctx.rng.range(Z0[type], Z1[type]);
    if (!deep(z)) return false;
    Boat b;
    b.type = type;
    b.z = z;
    b.scale = ctx.rng.range(0.9f, 1.1f);
    b.dir = ctx.rng.chance(0.5f) ? 1.f : -1.f;
    b.speed = SPEED[type] * ctx.rng.range(0.8f, 1.2f);
    b.phase = ctx.rng.range(0.f, TAU);
    b.seed = ctx.rng.range(0.f, 50.f);
    const float len = BOAT_LEN[type] * b.scale, hw = ctx.hwAt(z);
    b.x = onScreen ? ctx.rng.range(-hw * 0.9f, hw * 0.9f) : -b.dir * (hw + len * 0.6f + 1.f);
    for (const Boat& o : boats_)
        if (std::fabs(o.z - b.z) < 3.f + 0.25f * (len + BOAT_LEN[o.type]) && std::fabs(o.x - b.x) < len + BOAT_LEN[o.type] * o.scale + 2.f)
            return false;
    b.alpha = onScreen ? 1.f : 0.f;
    boats_.push_back(b);
    return true;
}

// =====================================================================================================
// Life in the sea
// =====================================================================================================

float SeaWorld::waterDepth() const { return seaLevel() - sandH(0.f, FRONT_Z - 2.f); }
float SeaWorld::critterY(const Critter& c) const {
    const float bed = sandH(c.x, c.z) + 0.45f, surf = seaLevel() - 0.45f;
    return lerpf(bed, surf, c.u + c.du) + c.dartY;
}

// Something new comes into the water (or is already there): what the sea's depth asks for and is missing.
void SeaWorld::spawnLife(bool onScreen) {
    const float D = waterDepth();
    int have[5] = {0, 0, 0, 0, 0};
    for (const Critter& c : critters_)
        if (c.kind == L_FISH) have[0] += c.du == 0.f; // one leader per school
        else if (c.kind < 5) have[c.kind]++;
    const int want[5] = {D > 2.f ? std::min(4, 1 + (int)((D - 2.f) / 3.5f)) : 0, D > 6.f ? std::min(5, 1 + (int)((D - 6.f) / 2.2f)) : 0,
                         D > 11.f ? 1 : 0, D > 10.f ? (D > 13.f ? 2 : 1) : 0, D > 9.f ? std::min(16, (int)((D - 9.f) * 2.5f)) : 0};
    int kind = -1;
    const int start = ctx.rng.irange(0, 4);
    for (int i = 0; i < 5 && kind < 0; i++) {
        const int k = (start + i) % 5;
        if (have[k] < want[k]) kind = k;
    }
    if (kind < 0) return;
    Critter c{};
    c.kind = kind;
    c.dir = ctx.rng.chance(0.5f) ? 1.f : -1.f;
    c.phase = ctx.rng.range(0.f, TAU);
    c.seed = ctx.rng.range(0.f, 50.f);
    c.alpha = onScreen ? 1.f : 0.f;
    // behind the cut face, near enough to be seen through the water
    c.z = FRONT_Z - ctx.rng.range(0.6f, kind == L_MOTE ? 9.f : 6.5f);
    const float hw = ctx.hwAt(c.z);
    // a depth under the surface, as a place in the water column (so that it rises with the sea)
    auto under = [&](float lo, float hi) { return saturate(1.f - std::clamp(ctx.rng.range(lo, hi), 0.f, D) / std::max(D, 0.5f)); };
    const float margin = 2.5f;
    const float x0 = onScreen ? ctx.rng.range(-hw + 1.f, hw - 1.f) : -c.dir * (hw + margin);
    c.x = x0;
    if (kind == L_FISH) {
        // a small school near the surface or in the middle water, now and then one in the accent color
        const int n = ctx.rng.irange(5, 9);
        c.speed = ctx.rng.range(0.7f, 1.3f);
        c.size = ctx.rng.range(0.24f, 0.32f);
        c.u = under(0.9f, std::min(5.f, D * 0.6f));
        c.school = nextSchool_++;
        const bool accent = ctx.rng.chance(0.25f);
        c.col = accent ? lerp(ctx.pal.baccent, ctx.pal.foam, 0.2f) : lerp(ctx.pal.sea2, ctx.pal.foam, 0.62f);
        for (int i = 0; i < n; i++) {
            Critter f = c;
            f.x = x0 - c.dir * (i == 0 ? 0.f : ctx.rng.range(0.3f, 2.2f));
            f.z = c.z + (i == 0 ? 0.f : ctx.rng.range(-0.6f, 0.6f));
            f.du = i == 0 ? 0.f : ctx.rng.range(-0.6f, 0.6f) / std::max(D, 1.f);
            if (i > 0 && f.du == 0.f) f.du = 1e-4f;
            f.phase = ctx.rng.range(0.f, TAU);
            f.size = c.size * ctx.rng.range(0.85f, 1.1f);
            critters_.push_back(f);
        }
        return;
    }
    if (kind == L_JELLY) {
        // jellyfish: slowly pulsing bells, translucent, a soft glow of their own
        c.speed = ctx.rng.range(0.05f, 0.14f);
        c.size = ctx.rng.range(0.32f, 0.5f);
        c.u = under(2.5f, std::min(D * 0.75f, 8.f));
        const vec3 hues[3] = {ctx.pal.piece[2], ctx.pal.piece[0], lerp(ctx.pal.baccent, ctx.pal.piece[2], 0.4f)};
        c.col = lerp(hues[ctx.rng.irange(0, 2)], vec3(1.f), 0.45f);
        c.glow = c.col;
    } else if (kind == L_ANGLER) {
        // an anglerfish in the dark: a near-black shape, only its lure lit
        c.speed = ctx.rng.range(0.1f, 0.18f);
        c.size = 0.6f;
        c.u = ctx.rng.range(0.06f, 0.2f);
        c.col = ctx.pal.deep * 0.35f + vec3(0.012f, 0.016f, 0.024f);
        c.glow = vec3(1.f, 0.88f, 0.55f);
    } else if (kind == L_SIPHON) {
        // a siphonophore: a long chain of small lit beads drifting slowly
        c.speed = ctx.rng.range(0.04f, 0.09f);
        c.size = ctx.rng.range(1.6f, 2.4f);
        c.u = ctx.rng.range(0.12f, 0.3f);
        c.col = lerp(ctx.pal.piece[0], vec3(1.f), 0.35f);
        c.glow = c.col;
    } else {
        // a point of light in the dark, drifting
        c.speed = ctx.rng.range(0.02f, 0.06f);
        c.size = ctx.rng.range(0.12f, 0.2f);
        c.u = ctx.rng.range(0.03f, 0.4f) * std::min(1.f, 10.f / std::max(D, 1.f));
        c.glow = ctx.rng.chance(0.5f) ? vec3(0.35f, 0.9f, 1.f) : vec3(0.6f, 1.f, 0.75f);
        c.col = c.glow;
    }
    if (onScreen && kind != L_FISH) c.x = ctx.rng.range(-hw + 2.f, hw - 2.f);
    critters_.push_back(c);
}

// A submarine crossing the deep water slowly, its portholes and its headlight lit.
void SeaWorld::spawnSub() {
    for (const Critter& o : critters_)
        if (o.kind == L_SUB) return;
    Critter c{};
    c.kind = L_SUB;
    c.dir = ctx.rng.chance(0.5f) ? 1.f : -1.f;
    c.phase = ctx.rng.range(0.f, TAU);
    c.seed = ctx.rng.range(0.f, 50.f);
    c.alpha = 1.f;
    c.z = FRONT_Z - ctx.rng.range(2.f, 4.f);
    c.size = ctx.rng.range(3.6f, 4.4f);
    c.speed = ctx.rng.range(0.7f, 1.f);
    c.u = ctx.rng.range(0.3f, 0.55f);
    c.col = lerp(ctx.pal.deep * 0.6f, vec3(0.2f, 0.22f, 0.25f), 0.5f);
    c.glow = lerp(ctx.pal.window, vec3(1.f, 0.9f, 0.7f), 0.3f);
    c.x = -c.dir * (ctx.hwAt(c.z) + c.size * 1.2f + 1.f);
    critters_.push_back(c);
}

void SeaWorld::simulateLife(float dt, float wdt, float world) {
    lifeT_ -= wdt;
    if (lifeT_ <= 0.f && world > 0.5f && !ctx.over) {
        spawnLife(false);
        lifeT_ = ctx.rng.range(1.f, 3.f);
    }
    subT_ -= wdt;
    if (subT_ <= 0.f) {
        subT_ = ctx.rng.range(25.f, 50.f);
        if (world > 0.5f && !ctx.over && waterDepth() > SUB_DEPTH) spawnSub();
    }
    glowUp_ = approach(glowUp_, ctx.storm, 1.f, dt);
    const float surf = seaLevel();
    for (Critter& c : critters_) {
        c.dart = approach(c.dart, 0.f, 0.6f, dt);
        c.x += c.dir * c.speed * wdt * (1.f + 2.5f * c.dart);
        c.dartY += c.dyv * wdt;
        c.dyv *= std::exp(-2.5f * wdt);
        c.dartY = approach(c.dartY, 0.f, 0.15f, wdt);
        // out of the water (the sea draining for a new song): it fades away
        const float y = critterY(c);
        const bool wet = y < surf - c.size * 0.6f && y > sandH(c.x, c.z) + 0.1f;
        c.alpha = approach(c.alpha, wet ? 1.f : 0.f, 0.8f, dt);
    }
    critters_.erase(std::remove_if(critters_.begin(), critters_.end(),
                                   [&](const Critter& c) {
                                       const float y = critterY(c);
                                       const bool wet = y < surf - c.size * 0.6f;
                                       return c.dir * c.x > ctx.hwAt(c.z) + c.size * 2.f + 3.f || (!wet && c.alpha <= 0.f);
                                   }),
                     critters_.end());
}

// =====================================================================================================
// The thrown cubes
// =====================================================================================================

void SeaWorld::throwCube(const vec3& p, int type, const vec3& v, const vec3& w, float delay, float water) {
    if (bodies_.size() >= 260) return;
    Body b{};
    b.p = p;
    b.v = v;
    b.w = w;
    b.q[0] = b.q[1] = b.q[2] = 0.f;
    b.q[3] = 1.f;
    b.type = std::clamp(type, 0, 6);
    b.seed = ctx.rng.range(0.f, 50.f);
    b.delay = delay;
    b.fill = 1.f;
    b.water = water;
    bodies_.push_back(b);
}

// The crack opens at the corner that hit first (the cube's frame: it turns with it); the water jets out of it a moment
// later, along the outward normal there.
void SeaWorld::crackAt(Body& b, const vec3& corner) {
    if (b.hit) return;
    b.hit = true;
    b.hitT = b.age;
    const vec3 s(corner.x < 0.f ? -1.f : 1.f, corner.y < 0.f ? -1.f : 1.f, corner.z < 0.f ? -1.f : 1.f);
    b.crackL = s * 0.4f;
    b.crackN = normalize(s);
}

// The rigid bodies: gravity; in the water, buoyancy and drag; contacts with the dunes at the cube's eight corners
// (impulses with restitution and friction, so they tumble, bounce and come to rest on a face); collisions between the
// cubes (as spheres); the glass of the diorama's front once they are behind it. Real time, small substeps.
void SeaWorld::simulateBodies(float dt) {
    const int N = 4;
    const float h = std::min(dt, 0.05f) / N;
    static const vec3 K[8] = {vec3(-1, -1, -1), vec3(1, -1, -1), vec3(-1, 1, -1), vec3(1, 1, -1),
                              vec3(-1, -1, 1),  vec3(1, -1, 1),  vec3(-1, 1, 1),  vec3(1, 1, 1)};
    for (Body& b : bodies_) b.age += dt;
    for (int step = 0; step < N; step++) {
        for (Body& b : bodies_) {
            if (b.age < b.delay) continue;
            if (b.fade > 0.f) {
                // empty: it sinks into the sand
                b.p.y -= h * 0.45f;
                continue;
            }
            if (b.asleep) continue;
            b.v.y -= GRAV * h;
            if (b.p.y < surfY(b.p.x, b.p.z)) {
                if (!b.wet) {
                    b.wet = true;
                    splash(vec3(b.p.x, surfY(b.p.x, b.p.z), b.p.z), std::min(1.5f, -b.v.y * 0.12f + 0.4f), WATER);
                    splash(vec3(b.p.x, surfY(b.p.x, b.p.z), b.p.z), std::min(1.2f, -b.v.y * 0.1f + 0.3f), WATER);
                    // into the sea first: it cracks at its lowest corner
                    int lo = 0;
                    float ly = 1e9f;
                    for (int i = 0; i < 8; i++) {
                        const float y = qRot(b.q, K[i] * CH).y;
                        if (y < ly) { ly = y; lo = i; }
                    }
                    crackAt(b, K[lo]);
                }
                b.v.y += GRAV * 0.62f * h;
                b.v *= std::exp(-2.2f * h);
                b.w *= std::exp(-1.8f * h);
            }
            b.p += b.v * h;
            qIntegrate(b.q, b.w, h);
            // the diorama's front: once behind it, it stays behind it (its glass)
            const float wall = FRONT_Z - 0.6f;
            if (b.p.z < wall) b.passed = true;
            if (b.passed && b.p.z > wall) {
                b.p.z = wall;
                if (b.v.z > 0.f) b.v.z *= -0.3f;
            }
            // the sand
            float maxPen = 0.f;
            for (int it = 0; it < 2; it++) {
                for (int i = 0; i < 8; i++) {
                    const vec3 r = qRot(b.q, K[i] * CH);
                    const vec3 c = b.p + r;
                    const float pen = sandH(c.x, c.z) - c.y;
                    if (pen <= 0.f) continue;
                    if (it == 0) maxPen = std::max(maxPen, pen);
                    const vec3 n = sandN(c.x, c.z);
                    const vec3 vc = b.v + cross(b.w, r);
                    const float vn = dot(vc, n);
                    if (vn >= 0.f) continue;
                    if (vn < -1.f) crackAt(b, K[i]);
                    const vec3 rn = cross(r, n);
                    const float e = vn < -2.5f ? 0.28f : 0.f;
                    const float j = -(1.f + e) * vn / (1.f + dot(rn, rn) / INERTIA);
                    b.v += n * j;
                    b.w += cross(r, n * j) * (1.f / INERTIA);
                    // friction
                    const vec3 vc2 = b.v + cross(b.w, r);
                    const vec3 vt = vc2 - n * dot(vc2, n);
                    const float lt = length(vt);
                    if (lt > 1e-4f) {
                        const vec3 t = vt / lt;
                        const vec3 rt = cross(r, t);
                        const float jt = std::min(lt / (1.f + dot(rt, rt) / INERTIA), 0.6f * j);
                        b.v += t * -jt;
                        b.w += cross(r, t * -jt) * (1.f / INERTIA);
                    }
                }
            }
            b.p.y += maxPen * 0.85f;
            // at rest: it sleeps (no creeping)
            if (length(b.v) < 0.12f && length(b.w) < 0.25f && maxPen > 0.f) {
                b.still += h;
                if (b.still > 0.35f) {
                    b.asleep = true;
                    b.v = vec3(0.f);
                    b.w = vec3(0.f);
                }
            } else b.still = 0.f;
        }
        // the cubes against each other (as spheres: a lying cube pushes a falling one aside and both turn)
        for (size_t i = 0; i < bodies_.size(); i++) {
            Body& a = bodies_[i];
            if (a.age < a.delay || a.fade > 0.f) continue;
            for (size_t k = i + 1; k < bodies_.size(); k++) {
                Body& c = bodies_[k];
                if (c.age < c.delay || c.fade > 0.f) continue;
                vec3 d = c.p - a.p;
                const float l2 = dot(d, d);
                if (l2 > 0.94f * 0.94f || l2 < 1e-8f) continue;
                if (a.asleep && c.asleep) continue;
                const float l = std::sqrt(l2);
                const vec3 n = d / l;
                const float pen = 0.94f - l;
                const float wa = a.asleep ? 0.f : (c.asleep ? 1.f : 0.5f), wc = 1.f - wa;
                a.p += n * (-pen * wa);
                c.p += n * (pen * wc);
                const float vn = dot(c.v - a.v, n);
                if (vn < 0.f) {
                    const float j = -1.2f * vn * 0.5f;
                    if (std::fabs(vn) > 0.4f) { a.asleep = false; c.asleep = false; a.still = c.still = 0.f; }
                    if (!a.asleep) a.v += n * (-j * (c.asleep ? 2.f : 1.f));
                    if (!c.asleep) c.v += n * (j * (a.asleep ? 2.f : 1.f));
                    // a glancing knock turns them
                    a.w += cross(n, c.v - a.v) * 0.3f;
                    c.w += cross(n, a.v - c.v) * 0.3f;
                    if (vn < -1.5f) {
                        crackAt(a, qRotInv(a.q, n));
                        crackAt(c, qRotInv(c.q, n * -1.f));
                    }
                }
            }
        }
    }
    for (Body& b : bodies_) {
        if (b.age < b.delay) continue;
        // never hit anything in time (a rare lucky one): it cracks anyway
        if (!b.hit && b.age - b.delay > 3.f) crackAt(b, vec3(0.f, -1.f, 0.f));
        leak(b, dt);
    }
    bodies_.erase(std::remove_if(bodies_.begin(), bodies_.end(), [](const Body& b) { return b.fade >= 1.f; }), bodies_.end());
}

// The crack opens, then the water jets out of it: a blast, then a fine strong jet tapering off over a couple of
// seconds as the pressure drops (the cube stays full while under pressure, then its level drops); empty, it sinks.
void SeaWorld::leak(Body& b, float dt) {
    if (!b.hit) return;
    const float since = b.age - b.hitT;
    b.crack = smoothstepf(0.f, 0.18f, since);
    if (since < 0.06f) return;
    if (b.fill <= 0.f) {
        b.fade = std::min(1.f, b.fade + dt / 1.4f);
        return;
    }
    const float before = b.fill;
    b.fill = std::max(0.f, b.fill - dt * (0.1f + 0.5f * b.fill));
    if (b.fill < 0.02f) b.fill = 0.f;
    const float out = b.water * (before - b.fill);
    const vec3 m = b.p + qRot(b.q, b.crackL);
    const vec3 n = qRot(b.q, b.crackN);
    const float pr = std::pow(before, 1.3f);
    const float blast = std::exp(-(since - 0.06f) * 9.f);
    // the jet: out along the crack's outward normal, turned by the ground (from a bottom corner it sprays sideways
    // along the sand)
    vec3 d = n;
    if (m.y - sandH(m.x, m.z) < 0.3f && d.y < 0.2f) d.y = 0.2f + 0.15f * std::max(0.f, -n.y);
    d = normalize(d);
    // its push on the cube (it is kicked back, rocks)
    if (b.asleep && blast > 0.3f) { b.asleep = false; b.still = 0.f; }
    if (!b.asleep) {
        b.v += d * (-(0.8f * pr + 4.f * blast) * dt);
        b.w += cross(qRot(b.q, b.crackL), d * (-(0.8f * pr + 4.f * blast) * dt)) * (0.6f / INERTIA);
    }
    const vec3 glass = ctx.pal.cell[b.type];
    const vec3 jetCol = lerp(glass * (1.f / std::max({glass.x, glass.y, glass.z, 1e-3f})), WATER, 0.25f);
    if (m.y < surfY(m.x, m.z)) {
        // under the sea: the jet churns out as a pale plume, bubbles rising from it
        waterLines_ += out;
        b.acc += dt * (18.f + 40.f * pr + 60.f * blast);
        while (b.acc >= 1.f && parts_.size() < 6000) {
            b.acc -= 1.f;
            Particle q{};
            const bool plume = ctx.rng.chance(0.55f);
            const float sp = plume ? (1.5f + 5.f * pr + 3.f * blast) * ctx.rng.range(0.7f, 1.1f) : ctx.rng.range(0.5f, 1.5f);
            q.p = m + vec3(ctx.rng.range(-0.04f, 0.04f), ctx.rng.range(-0.04f, 0.04f), ctx.rng.range(-0.04f, 0.04f));
            q.v = d * sp + vec3(ctx.rng.range(-0.4f, 0.4f), ctx.rng.range(0.2f, 1.f), ctx.rng.range(-0.4f, 0.4f));
            if (plume) {
                q.life = ctx.rng.range(0.9f, 1.6f);
                q.size = ctx.rng.range(0.14f, 0.26f) * (0.7f + 0.6f * pr);
                q.grav = -0.6f; q.drag = 2.4f;
                q.kind = P_PLUME;
                q.col = lerp(lerp(WATER, jetCol, 0.3f), vec3(1.f), 0.45f);
            } else {
                q.life = ctx.rng.range(2.f, 3.5f);
                q.size = ctx.rng.range(0.03f, 0.075f);
                q.grav = -3.5f; q.drag = 1.6f;
                q.kind = P_BUBBLE;
                q.col = vec3(0.85f, 0.93f, 1.f);
            }
            q.real = true; q.seed = ctx.rng.uniform();
            parts_.push_back(q);
        }
    } else {
        b.pend += out;
        b.acc += dt * (60.f + 200.f * pr + 260.f * blast);
        const int k = (int)b.acc;
        b.acc -= (float)k;
        // two directions across the jet (for its spread)
        const vec3 u = normalize(cross(d, std::fabs(d.y) < 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0)));
        const vec3 w = cross(d, u);
        for (int i = 0; i < k && parts_.size() < 6000; i++) {
            Particle q{};
            // most of it the jet's core; some spray around it, wider and finer
            const bool spray = ctx.rng.chance(0.16f + 0.2f * pr + 0.3f * blast);
            const float s = spray ? 0.45f * (1.f + blast) : 0.022f;
            const vec3 dd = normalize(d + u * ctx.rng.range(-s, s) + w * ctx.rng.range(-s, s));
            const float sp = (2.2f + 8.5f * pr + 4.f * blast) * (spray ? ctx.rng.range(0.55f, 1.05f) : ctx.rng.range(0.98f, 1.02f));
            q.p = m + dd * 0.02f;
            q.v = dd * sp + b.v;
            q.age = -(float)i / (float)k * dt;
            q.life = 3.f;
            q.size = spray ? ctx.rng.range(0.035f, 0.06f) : ctx.rng.range(0.065f, 0.09f) * (0.8f + 0.5f * (1.f - pr));
            q.grav = 15.f; q.drag = spray ? 0.6f : 0.15f;
            q.kind = P_JET; q.real = true; q.seed = ctx.rng.uniform();
            q.col = spray ? lerp(WATER, vec3(1.f), 0.5f) : lerp(lerp(WATER, ctx.pal.sea1, 0.12f + 0.1f * ctx.rng.uniform()), jetCol, 0.5f);
            q.water = b.pend / (float)k;
            parts_.push_back(q);
        }
        if (k > 0) b.pend = 0.f;
    }
    if (b.fill <= 0.f) { waterLines_ += b.pend; b.pend = 0.f; }
}

// A whole stack filled at once while shown (a board filled for a review): its sea has risen
// already, deep, with its boats and its life.
void SeaWorld::grown() {
    if (waterLines_ >= 48.f) return;
    waterLines_ = 48.f;
    rise_ = riseTarget();
    boats_.clear();
    for (int i = 0; i < 6; i++) spawnBoat(true);
    critters_.clear();
    for (int i = 0; i < 30; i++) spawnLife(true);
}

// =====================================================================================================
// Game events
// =====================================================================================================

void SeaWorld::splash(const vec3& p, float power, const vec3& col) {
    if (parts_.size() > 5500) return;
    const int n = 2 + (int)(power * 4.f);
    for (int i = 0; i < n; i++) {
        Particle q{};
        q.p = p + vec3(ctx.rng.range(-0.1f, 0.1f), 0.f, ctx.rng.range(-0.1f, 0.1f));
        q.v = vec3(ctx.rng.range(-1.2f, 1.2f), ctx.rng.range(1.5f, 4.f) * power, ctx.rng.range(-1.2f, 1.2f));
        q.life = ctx.rng.range(0.35f, 0.6f);
        q.size = ctx.rng.range(0.03f, 0.055f);
        q.grav = 14.f; q.drag = 0.5f;
        q.kind = P_SPLASH; q.real = true; q.seed = ctx.rng.uniform();
        q.col = lerp(col, ctx.pal.foam, 0.6f);
        parts_.push_back(q);
    }
}

// Spray torn off a crest by the wind.
void SeaWorld::sprayBurst(float x, float z, int n, float power) {
    for (int i = 0; i < n && parts_.size() < 3000; i++) {
        Particle q{};
        const float px = x + ctx.rng.range(-0.6f, 0.6f), pz = z + ctx.rng.range(-0.6f, 0.6f);
        q.p = vec3(px, surfY(px, pz) + ctx.rng.range(-0.05f, 0.1f), pz);
        q.v = vec3(dir_ * ctx.rng.range(2.f, 6.f) * power, ctx.rng.range(1.f, 3.5f) * power, ctx.rng.range(-0.6f, 0.6f));
        q.life = ctx.rng.range(0.8f, 1.4f);
        q.size = ctx.rng.range(0.035f, 0.07f);
        q.grav = 5.f; q.drag = 0.7f;
        q.kind = P_SPRAY; q.real = false; q.seed = ctx.rng.uniform();
        q.col = ctx.pal.foam;
        parts_.push_back(q);
    }
}

void SeaWorld::onEvent(const GameEvent& ev, const Game&) {
    switch (ev.type) {
    case GameEvent::Clear: {
        // The row's cubes are thrown out of the well into the world behind it (the row itself is gone from the board
        // at once), tumbling, each toward its own place on the sand at its side.
        const int n = std::min(ev.count, 4);
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const float dx = c.x - (Game::W - 1) * 0.5f;
            const float side = dx < 0.f ? -1.f : 1.f;
            const float x = side * (6.4f + std::fabs(dx) * 0.5f + ctx.rng.range(0.f, 8.f));
            const float z = -ctx.rng.range(2.6f, 9.f);
            const float ty = sandH(x, z) + 0.7f;
            const float T = 0.72f + 0.03f * std::fabs(x - p.x) + ctx.rng.range(0.f, 0.15f);
            const vec3 v((x - p.x) / T, (ty - p.y) / T + 0.5f * GRAV * T, (z - p.z) / T);
            // tumbling on all axes
            const vec3 axis = normalize(vec3(ctx.rng.range(-1.f, 1.f), ctx.rng.range(-1.f, 1.f), ctx.rng.range(-1.f, 1.f)));
            // each cube's water: a tenth of a line of sea (more for a quad)
            throwCube(vec3(p.x, p.y, 0.f), c.type, v, axis * ctx.rng.range(3.f, 9.f), 0.f, (n >= 4 ? 1.5f : 1.f) / Game::W);
        }
        if (ev.count >= 4) {
            // a quad: the sea answers with spray up from both sides
            ctx.gust = std::max(ctx.gust, 0.8f);
            for (int i = 0; i < 4; i++) {
                const float side = i % 2 ? 1.f : -1.f;
                const float z = -ctx.rng.range(3.f, 14.f);
                const float x = side * ctx.rng.range(7.f, ctx.hwAt(z) - 2.f);
                for (int k = 0; k < 26; k++) {
                    Particle q{};
                    const float px = x + ctx.rng.range(-0.5f, 0.5f), pz = z + ctx.rng.range(-0.5f, 0.5f);
                    q.p = vec3(px, surfY(px, pz), pz);
                    q.v = vec3(ctx.rng.range(-1.f, 1.f), ctx.rng.range(4.f, 9.f), ctx.rng.range(-1.f, 1.f));
                    q.age = -ctx.rng.range(0.f, 0.3f) - i * 0.12f;
                    q.life = ctx.rng.range(1.f, 1.5f);
                    q.size = ctx.rng.range(0.04f, 0.08f);
                    q.grav = 9.f; q.drag = 0.4f;
                    q.kind = P_SPLASH; q.real = true; q.seed = ctx.rng.uniform();
                    q.col = ctx.pal.foam;
                    parts_.push_back(q);
                }
            }
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Simulation
// =====================================================================================================

void SeaWorld::simulateParticles(float dt, float wdt) {
    struct Hit { vec3 p; vec3 col; float power; };
    std::vector<Hit> hits;
    for (Particle& p : parts_) {
        const float pdt = p.real ? dt : wdt;
        p.age += pdt;
        if (p.age < 0.f) continue;
        if (p.kind == P_SPRAY) p.v.x += dir_ * 2.f * (ctx.storm + ctx.gust) * pdt;
        // bubbles wobble up and pop at the surface
        if (p.kind == P_BUBBLE) {
            p.v.x += std::sin(ctx.time * 5.f + p.seed * 40.f) * 1.5f * pdt;
            if (p.p.y > surfY(p.p.x, p.p.z) - p.size) p.age = p.life + 1.f;
        }
        p.v.y -= p.grav * pdt;
        p.v *= std::exp(-p.drag * pdt);
        p.p += p.v * pdt;
        // the diorama's front glass
        if (p.p.z > FRONT_Z - 0.05f) {
            p.p.z = FRONT_Z - 0.05f;
            if (p.v.z > 0.f) p.v.z *= -0.3f;
        }
        if (p.kind == P_JET && p.v.y < 0.f) {
            // water from a cube: into the sea it splashes; on the sand it runs on downhill and soaks in
            const float sea = surfY(p.p.x, p.p.z), sand = sandH(p.p.x, p.p.z);
            if (p.p.y < sea) {
                if (ctx.rng.chance(0.3f)) hits.push_back({vec3(p.p.x, sea, p.p.z), p.col, 0.8f});
                waterLines_ += p.water;
                p.water = 0.f;
                p.age = p.life + 1.f;
            } else if (p.p.y < sand + 0.02f) {
                const vec3 n = sandN(p.p.x, p.p.z);
                vec3 dh(n.x + p.v.x * 0.05f, 0.f, n.z + p.v.z * 0.05f);
                const float l = std::sqrt(dh.x * dh.x + dh.z * dh.z);
                dh = l > 1e-4f ? dh / l : vec3(p.p.x < 0.f ? -1.f : 1.f, 0.f, 0.f);
                p.kind = P_RUN;
                p.age = 0.f;
                p.life = ctx.rng.range(0.35f, 0.7f);
                p.p.y = sand + 0.02f;
                p.v = dh * ctx.rng.range(1.2f, 2.8f) + vec3(p.v.x * 0.25f, 0.f, p.v.z * 0.25f);
                p.grav = 0.f; p.drag = 2.f;
                if (ctx.rng.chance(0.25f)) hits.push_back({vec3(p.p.x, sand, p.p.z), p.col, 0.5f});
            }
            continue;
        }
        if (p.kind == P_RUN) {
            p.p.y = sandH(p.p.x, p.p.z) + 0.02f;
            if (p.p.y < surfY(p.p.x, p.p.z)) {
                waterLines_ += p.water;
                p.water = 0.f;
                p.age = p.life + 1.f;
            }
            continue;
        }
        if ((p.kind == P_SPLASH || p.kind == P_SPRAY) && p.v.y < 0.f && p.p.y < surfY(p.p.x, p.p.z) - 0.05f) p.age = p.life + 1.f;
        if (p.kind == P_SAND && p.v.y < 0.f && p.p.y < sandH(p.p.x, p.p.z)) p.age = p.life + 1.f;
    }
    for (const Particle& p : parts_)
        if (p.age > p.life) waterLines_ += p.water;
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.age > p.life; }), parts_.end());
    for (const Hit& h : hits) splash(h.p, h.power, h.col);
}

void SeaWorld::simulate(const Game& game, const MusicState& music, float dt, float wdt) {
    (void)game;
    (void)music;
    const float wm = ctx.wm(), wp = ctx.wp();
    const float world = ctx.world();
    const float chorus = wp * (1.f - ctx.storm);
    const float calmSlow = 1.f - 0.75f * ctx.slow;
    amp_ = lerpf(0.05f + 0.07f * wm + 0.12f * wp, 1.15f, ctx.storm) * calmSlow * (1.f - 0.6f * smoothstepf(0.f, 0.6f, ctx.bridge.amount));
    sharp_ = 0.15f + 0.2f * wp + 0.65f * ctx.storm;
    caps_ = std::min(1.f, 0.4f * chorus + ctx.storm + 0.3f * ctx.gust * wp) * calmSlow;
    glint_ = (0.45f + 0.3f * wm + 0.25f * wp) * (1.f - ctx.storm) * (1.f - 0.7f * ctx.slow);
    rock_ = (0.025f + 0.02f * wm + 0.035f * wp + 0.16f * ctx.storm) * calmSlow;
    waveT_ += wdt * (0.3f + 0.08f * wm + 0.14f * wp + 0.7f * ctx.storm + 0.25f * ctx.gust);

    // the sea rises with the water poured in (or drains back for a new song)
    rise_ = approach(rise_, riseTarget(), 2.f, dt);

    // Spray off the crests through the storm.
    if (ctx.storm > 0.05f && world > 0.5f && !ctx.over) {
        sprayAcc_ += 26.f * ctx.storm * wdt;
        int guard = 0;
        while (sprayAcc_ >= 1.f && guard++ < 20) {
            sprayAcc_ -= 1.f;
            float x, z;
            sprayAt(x, z);
            sprayBurst(x, z, 1, 0.7f + 0.6f * ctx.storm);
        }
    } else sprayAcc_ = 0.f;

    // Boats: a few out at sea, more in a chorus, sailing slowly; they keep coming as long as the world is there.
    {
        const float S = seaLevel();
        const bool afloat = S >= BOAT_LEVEL;
        const int want = !afloat ? 0 : (int)(3.f + 1.f * wp + 2.f * rise_);
        boatT_ -= wdt;
        if ((int)boats_.size() < want && boatT_ <= 0.f && world > 0.5f) {
            if (spawnBoat(false)) boatT_ = ctx.rng.range(3.f, 8.f);
            else boatT_ = 1.f;
        }
        for (Boat& b : boats_) {
            b.x += b.dir * b.speed * wdt * (1.f + 0.4f * ctx.storm);
            // the sea draining (a new song): they fade before they would sit on the sand
            const bool deep = S - sandH(b.x, b.z) > 0.4f;
            b.alpha = approach(b.alpha, deep ? 1.f : 0.f, 1.5f, dt);
        }
        boats_.erase(std::remove_if(boats_.begin(), boats_.end(),
                                    [&](const Boat& b) {
                                        return b.dir * b.x > ctx.hwAt(b.z) + BOAT_LEN[b.type] * b.scale + 1.f || (b.alpha <= 0.01f && b.dir * b.x > -ctx.hwAt(b.z) && !afloat);
                                    }),
                     boats_.end());
    }

    // The thrown cubes (at the real speed).
    simulateBodies(dt);

    simulateLife(dt, wdt, world);

    simulateParticles(dt, wdt);
}

// Game over: a cracked cube of the stack is thrown out behind the well, as a cleared one.
void SeaWorld::breakOut(const vec3& p, int type) {
    const float side = p.x < 0.f ? -1.f : 1.f;
    const vec3 axis = normalize(vec3(ctx.rng.range(-1.f, 1.f), ctx.rng.range(-1.f, 1.f), ctx.rng.range(-1.f, 1.f)));
    throwCube(p, type, vec3(side * ctx.rng.range(1.5f, 6.f), ctx.rng.range(2.f, 7.f), -ctx.rng.range(4.f, 9.f)),
              axis * ctx.rng.range(2.f, 7.f), 0.f, 0.5f / Game::W);
}

} // namespace biome

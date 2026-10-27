// BIOME's land: glass cubes of seeds over a parched land that grows into a jungle.
//
// A wide plain of dry, cracked clay under a bleached, dusty sky, a few dead trees standing on it, low hills on the
// horizon; the land cut open just behind the well like a diorama, its section showing the soil's layers (and, later,
// roots). The stack's cubes are full of seeds of their piece's color (each piece its own seed: long grains, peas, flat
// almonds, big beans, striped sunflower seeds, fine millet, lentils), traced through the glass (CELL_FILL).
//
// A cleared row's cubes vanish at once and their seeds burst out, each on its own high arc
// into the land behind the well, a faint trail behind it. Each one lands with a little bounce and a puff of dust, comes
// to rest and buries itself under a small mound of soil; the ones sown near then sprout in view, one after the other
// (a dozen at most at once): the stem pushes out of the mound bent in a hook, straightens, its two seed leaves unfold,
// and the plant grows out of it, slowly: grass in seconds, flowers in their seed's color, ferns and bushes, and trees
// over minutes (round canopies, tall emergent trees, palms); vines hang from the grown trees. The seeds sown far out
// just come up. The more lines are cleared, the more the land comes alive: the soil darkens and greens around what
// grows, the far hills fill with forest, the sky clears from dust to a humid haze, birds arrive and fly between the
// trees, butterflies come to the flowers, mist settles in the distance; the dead trees leaf out again. About sixty
// lines turn the parched plain into a jungle.
//
// A drop brings the rain (the shell's storm): the soil soaks dark and puddles form, everything grows several times
// faster, flowers open, a flock lifts from the trees. It eases back after the drop, the growth stays. A chorus is a
// bright, breezy bloom; a break a still, hazy land. Slow motion is a blue dusk with fireflies.
#include "universes/biome/land.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "layout.hpp"

using namespace biome;
using namespace biome::land;

namespace {

// The variants (sRGB): the sky (top, low), the sun or moon, the clouds, the dry season's dust haze, the humid mist,
// the rain's sky and clouds; the soil (dry, wet, its cut face, its cracks), the grass (dry, lush), the leaves (two,
// far), the bark, the dead wood; two flower colors, the birds, the well's pane, the HUD ink, the accent, the dusk of
// slow motion; the seven seeds; the sun (azimuth: 0 into the scene, 90 to the right, 180 behind the eye; elevation in
// degrees, negative: a fraction of the sky shown above the horizon; its disc's size), moon, stars, night, the sun's
// strength, the sky's light, the shadows' depth, the pane's opacity, cloudiness.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyLow, sun, cloud, dust, mist, stormSky, stormCloud;
    unsigned soilDry, soilWet, soilCut, crack, grassDry, grassLush, leafA, leafB, leafFar, bark, dead;
    unsigned flowerA, flowerB, bird, panel, ink, accent, dusk;
    unsigned piece[7];
    float sunAz, sunEl, sunR, moon, stars, night, lightK, ambK, shadowK, panelA, clouds;
};
const VariantDef VARIANTS[] = {
    // A savanna at noon: a bleached blue sky, pale ochre clay, a hard sun from behind the eye; bright seeds.
    {"SAVANNA NOON", 0x7ea4c2, 0xece2cc, 0xfff6e0, 0xfbf8f2, 0xe2d2b4, 0xd6e2d8, 0x5a6064, 0x3e4448,
     0xcaa984, 0x4e3828, 0x8a6446, 0x6e4e36, 0xc8b27a, 0x5c8c2c, 0x3c7026, 0x74a036, 0x4c6c3c, 0x5a4636, 0xb8a890,
     0xf2563a, 0xffd23a, 0x2a2622, 0x1a130c, 0x2a1e14, 0xe8762a, 0x2a2048,
     {0x2f9ef5, 0xff7a1c, 0xe8264a, 0xffd21f, 0x22c27e, 0x9a4ae0, 0xff6fb8},
     -140.f, 55.f, 0.9f, 0.f, 0.f, 0.f, 1.15f, 0.55f, 0.7f, 0.64f, 0.15f},
    // Red earth at dusk: laterite soil, a low sun in an apricot haze, a violet sky; pale luminous seeds.
    {"RED EARTH DUSK", 0x2e3466, 0xf2a070, 0xffd6a0, 0xe89a84, 0xe8a47c, 0xc8a8a8, 0x3a2c3c, 0x2a2030,
     0xb0582e, 0x3e180e, 0x7a3418, 0x5a2410, 0xc08a50, 0x4a7a2a, 0x2c5826, 0x5c8a2e, 0x3c4a3c, 0x3e2a22, 0x9a7a6a,
     0xffd04a, 0xff6a8a, 0x1e1620, 0x160c10, 0xfbeee0, 0xffb36a, 0x241a3c,
     {0x8ee8ff, 0xffe27a, 0xff9a6a, 0xb8ff7a, 0xffa8d8, 0xd0b8ff, 0x7affd8},
     20.f, -0.25f, 1.5f, 0.f, 0.f, 0.15f, 1.1f, 0.5f, 0.6f, 0.62f, 0.35f},
    // A misty morning: grey-brown clay, a soft sun in a pale haze, the distance lost in mist; jewel seeds.
    {"MISTY MORNING", 0x9fb2c2, 0xe6e8e4, 0xfff4e4, 0xf4f4f0, 0xd8d8d2, 0xe2e6e2, 0x5a6268, 0x424a50,
     0xa49582, 0x3e352e, 0x6e6052, 0x5c5044, 0xb4ab84, 0x588a48, 0x32623a, 0x689450, 0x6e8478, 0x4a4038, 0x9c958a,
     0xf07aa0, 0xf4f0e0, 0x30343a, 0x10161a, 0x24302c, 0x2f8a78, 0x2a3048,
     {0x2ec0d8, 0x2266d0, 0xf0a818, 0xe0402f, 0x10a07a, 0x7a34b4, 0xe04c96},
     30.f, -0.5f, 1.1f, 0.f, 0.f, 0.f, 0.8f, 0.75f, 0.45f, 0.62f, 0.3f},
    // Moonlit clay: a deep blue night, a moon over the plain, the land grey-violet; pastel seeds, fireflies.
    {"MOONLIT CLAY", 0x060a1c, 0x1e2848, 0xdfe6f4, 0x283458, 0x2a3048, 0x3a4664, 0x0a0e1a, 0x161c2c,
     0x6a6070, 0x1c1822, 0x3a3242, 0x2a2432, 0x5a5a5a, 0x2e4a3a, 0x1e3a30, 0x2e5440, 0x1c2a34, 0x2a2228, 0x5a5664,
     0xc8a8ff, 0xa8f0ff, 0x0c0e18, 0x0a0c1c, 0xdce6f4, 0x8ad8ff, 0x101030,
     {0x88f0e8, 0x8ed4ff, 0xffc890, 0xfff090, 0x9effc0, 0xc4a8ff, 0xff98b8},
     18.f, -0.55f, 0.8f, 1.f, 1.f, 1.f, 0.55f, 0.5f, 0.5f, 0.56f, 0.2f},
    // An ochre haze: a dusty amber afternoon, the sun from the side; cool, deep seeds.
    {"OCHRE HAZE", 0xb49a78, 0xecd6ae, 0xfff0d0, 0xf2e2c4, 0xe6cfa2, 0xd6dcc4, 0x5e5a50, 0x46423a,
     0xc89c62, 0x4a3622, 0x8a6038, 0x6a4a2a, 0xccb070, 0x668a2a, 0x3c6a22, 0x82a032, 0x6a7442, 0x54402c, 0xc0a888,
     0xe04a2a, 0xf8f0d8, 0x2e261c, 0x16100a, 0x2a1c10, 0xc8542a, 0x2a1e40,
     {0x10a8a8, 0x2a62e8, 0xf2ece0, 0xd8302a, 0x2fb04a, 0x7a3cc8, 0xf04c8c},
     70.f, 28.f, 1.0f, 0.f, 0.f, 0.f, 1.0f, 0.6f, 0.6f, 0.64f, 0.1f},
    // A grey monsoon: an overcast sky, soft light without shadows, cool green when it grows; vivid seeds.
    {"GREY MONSOON", 0x76848a, 0xc8ccc4, 0xf0f0e8, 0xd0d4d0, 0xc6c6bc, 0xc4ccc6, 0x4a5256, 0x343c40,
     0x8e7c68, 0x342a22, 0x5e5040, 0x4e4234, 0xa0986e, 0x4c7e3e, 0x2a5830, 0x588842, 0x56685c, 0x40362e, 0x8a8478,
     0xff7a3a, 0xf0e040, 0x262a2c, 0x0e1214, 0x1e2628, 0x3aa07a, 0x2a3040,
     {0x22c4dc, 0x9a78ff, 0xff8a1c, 0xffd43c, 0x3ad48a, 0xc4e436, 0xff4a6a},
     150.f, 45.f, 1.0f, 0.f, 0.f, 0.f, 0.55f, 0.9f, 0.3f, 0.62f, 0.7f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// How fast each kind grows (seconds to full size at the base pace) and how wide it greens the soil.
constexpr float GROW_S[LandWorld::K_COUNT] = {7.f, 11.f, 18.f, 26.f, 95.f, 150.f, 80.f, 1.f, 1.f, 1.f};
constexpr float COVER_R[LandWorld::K_COUNT] = {1.6f, 1.3f, 2.f, 2.6f, 4.5f, 6.f, 3.5f, 0.f, 0.f, 0.f};

} // namespace

Palette LandWorld::palette(int v) const {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    Palette p{};
    p.skyTop = lin(d.skyTop); p.skyLow = lin(d.skyLow); p.sun = lin(d.sun); p.cloud = lin(d.cloud); p.dust = lin(d.dust);
    p.mist = lin(d.mist); p.stormSky = lin(d.stormSky); p.stormCloud = lin(d.stormCloud);
    p.soilDry = lin(d.soilDry); p.soilWet = lin(d.soilWet); p.soilCut = lin(d.soilCut); p.crack = lin(d.crack);
    p.grassDry = lin(d.grassDry); p.grassLush = lin(d.grassLush); p.leafA = lin(d.leafA); p.leafB = lin(d.leafB);
    p.leafFar = lin(d.leafFar); p.bark = lin(d.bark); p.dead = lin(d.dead);
    p.flowerA = lin(d.flowerA); p.flowerB = lin(d.flowerB); p.bird = lin(d.bird); p.panel = lin(d.panel); p.ink = lin(d.ink);
    p.accent = lin(d.accent); p.dusk = lin(d.dusk);
    for (int i = 0; i < 7; i++) p.cell[i] = p.piece[i] = lin(d.piece[i]);
    p.sunAz = d.sunAz; p.sunEl = d.sunEl; p.sunR = d.sunR; p.moon = d.moon; p.stars = d.stars; p.night = d.night;
    p.lightK = d.lightK; p.ambK = d.ambK; p.shadowK = d.shadowK; p.panelA = d.panelA; p.clouds = d.clouds;
    p.haze = p.dust;
    // the well's edge, the HUD (its panels the pane's; the ink dark over the bleached sky, pale over the dark land)
    p.edge = lerp(p.skyLow, vec3(1.f), 0.3f);
    p.hudPanel = p.panel;
    const bool inkDark = ugl::luma(p.ink) < 0.2f;
    p.inkDark = inkDark ? p.ink : lin(0x231a12);
    p.inkLight = inkDark ? lin(0xf6f0e4) : p.ink;
    // the cubes reflect the soil under the horizon; the gauge fills with the fourth seed; the cubes see the 3D sun
    p.envLow = p.soilDry * 0.6f;
    p.gauge = p.piece[3];
    p.sunX = p.sunY = 0.f;
    return p;
}
int land::variantCount() { return VARIANT_COUNT; }
const char* land::variantName(int i) { return VARIANTS[i].name; }

// =====================================================================================================
// Song
// =====================================================================================================

// The land from the song: major songs in the sun (a savanna noon, an ochre haze, a red earth at dusk), minor songs
// in the grey and the dark (a misty morning, a monsoon sky, a moonlit plain); the key's hue picks among them.
int LandWorld::pickVariant(const SongLook& t) const {
    float h = std::fmod(t.hue / TAU, 1.f);
    if (h < 0) h += 1.f;
    int v;
    const int k = std::min(2, (int)(h * 3.f));
    if (t.hasSong && !t.minor) v = k == 0 ? 0 : k == 1 ? 4 : 1;
    else if (t.hasSong) v = k == 0 ? 2 : k == 1 ? 5 : 3;
    else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
    if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    return v;
}

// A parched land, a few dead trees.
void LandWorld::resetLand() {
    lines_ = life_ = 0.f;
    plants_.clear();
    birds_.clear();
    parts_.clear();
    sows_.clear();
    puffs_.clear();
    for (int k = 0; k < K_COUNT; k++) plantCount_[k] = 0;
    std::fill(lifeG_.begin(), lifeG_.end(), 0.f);
    std::fill(lifeM_.begin(), lifeM_.end(), 0.f);
    std::fill(lifeF_.begin(), lifeF_.end(), 0.f);
    lifeDirty_ = true;
    wet_ = surge_ = 0.f;
    plantStumps();
    started_ = true;
}

void LandWorld::setSong(const SongLook& look, float seconds, bool fresh) {
    (void)seconds;
    if (fresh || !started_) {
        resetLand();
        return;
    }
    // a new song in the same land: a dry season between them; the grass and the flowers wither, the young plants and
    // most of the far forest with them, the grown trees stay
    (void)look;
    lines_ *= 0.35f;
    for (Plant& p : plants_) {
        if (p.kind == K_DEAD) continue;
        const bool keep = (p.kind == K_TREE || p.kind == K_EMERGENT || p.kind == K_PALM) && p.g > 0.6f && (!p.wild || ctx.rng.chance(0.3f));
        if (!keep) p.wither = std::max(p.wither, 0.01f);
    }
    for (size_t i = 0; i < lifeG_.size(); i++) {
        lifeG_[i] *= 0.3f;
        lifeM_[i] *= 0.5f;
        lifeF_[i] *= 0.2f;
    }
    lifeDirty_ = true;
}

void LandWorld::refresh(float strength) { ctx.gust = std::max(ctx.gust, 0.5f * strength); }

// The drop's hit: a growth surge, every flower opening, a flock lifting from the trees.
void LandWorld::dropHit() {
    surge_ = 1.f;
    bloom_ = 1.f;
    if (life_ > 0.1f && ctx.world() > 0.5f)
        for (int i = 0; i < 6 + (int)(10.f * life_); i++) spawnBird(life_ > 0.4f && i % 4 == 0 ? 1 : 0, true, true);
}

// A kick at a peak: a gust through the leaves.
void LandWorld::kick() { ctx.gust = std::max(ctx.gust, 0.35f + 0.3f * ctx.storm); }

// A whole stack at once: the land has grown already.
void LandWorld::grown() {
    if (lines_ < 40.f) seedInstant(46.f);
}

// Game over: a cracked cube breaks open like a cleared one (its seeds sown far).
void LandWorld::breakOut(const vec3& p, int type) { breakCube(p, type, 0.8f, 1.2f, 0, 0.f); }

// =====================================================================================================
// The plants
// =====================================================================================================

// The dead trees standing on the parched plain (each leafs out again once the land is alive enough).
void LandWorld::plantStumps() {
    Rng r(ctx.seed ^ 0xDEADull);
    const float Z[6] = {-9.f, -15.f, -24.f, -34.f, -48.f, -70.f};
    for (int i = 0; i < 6; i++) {
        const float z = Z[i] + r.range(-2.f, 2.f);
        const float hw = ctx.hwAt(z);
        float x = (i % 2 ? 1.f : -1.f) * r.range(0.25f, 0.85f) * hw;
        // never right behind the HUD's columns, near
        if (z > -18.f && std::fabs(x) < 14.f) x = (x < 0.f ? -1.f : 1.f) * r.range(14.f, std::max(15.f, hw * 0.85f));
        plant(vec3(x, groundH(x, z), z), K_DEAD, 1.f, false, 0);
    }
}

// What a seed grows into: grass most often, flowers in the seed's own color, ferns, bushes, and trees (more of them
// as the land comes alive: a jungle); only low plants right behind the HUD's columns.
int LandWorld::pickKind(float x, float z, int type) {
    (void)type;
    // the HUD's columns as seen from the eye (x scaled back to the board plane): only low plants there, near
    const float xs = std::fabs(x) * ctx.camDist / (ctx.camDist - z);
    const bool hud = z > -45.f && xs > 4.9f && xs < 12.6f;
    const float L = life_;
    float w[K_COUNT] = {0.40f, 0.20f, 0.10f, 0.12f, 0.10f + 0.08f * L, z < -14.f ? 0.02f + 0.04f * L : 0.f, 0.04f + 0.03f * L, 0.f};
    if (hud) w[K_BUSH] = w[K_TREE] = w[K_EMERGENT] = w[K_PALM] = 0.f;
    // not too many trees near: they need room
    if (plantCount_[K_TREE] + plantCount_[K_PALM] + plantCount_[K_EMERGENT] > 420) w[K_TREE] = w[K_PALM] = w[K_EMERGENT] = 0.f;
    int k = ctx.rng.weighted(w);
    if (k >= K_BUSH && k != K_DEAD) {
        const float room = k == K_BUSH ? 1.4f : k == K_EMERGENT ? 4.f : 2.6f;
        for (const Plant& p : plants_) {
            if (p.kind < K_BUSH || p.wild) continue;
            const float dx = p.p.x - x, dz = p.p.z - z;
            if (dx * dx + dz * dz < room * room) { k = ctx.rng.chance(0.5f) ? K_GRASS : K_FERN; break; }
        }
    }
    return k;
}

void LandWorld::plant(const vec3& p, int kind, float g0, bool wild, int type) {
    if (plants_.size() >= 3800) return;
    Plant q{};
    q.p = p;
    q.kind = kind;
    q.g = g0;
    q.rate = 1.f / (GROW_S[kind] * ctx.rng.range(0.8f, 1.25f));
    q.rot = ctx.rng.range(0.f, TAU);
    q.seed = ctx.rng.range(0.f, 50.f);
    q.tint = (float)type + ctx.rng.range(0.f, 0.99f);   // the seed's type (flowers in its color) and a shade
    switch (kind) {
    case K_GRASS: q.scale = ctx.rng.range(0.8f, 1.3f); break;
    case K_FLOWER: q.scale = ctx.rng.range(0.8f, 1.2f); break;
    case K_FERN: q.scale = ctx.rng.range(0.8f, 1.3f); break;
    case K_BUSH: q.scale = ctx.rng.range(0.8f, 1.4f); break;
    case K_TREE: q.scale = ctx.rng.range(0.75f, 1.35f) * (wild ? 1.2f : 1.f); break;
    case K_EMERGENT: q.scale = ctx.rng.range(0.9f, 1.3f) * (wild ? 1.25f : 1.f); break;
    case K_PALM: q.scale = ctx.rng.range(0.8f, 1.2f) * (wild ? 1.15f : 1.f); break;
    default: q.scale = ctx.rng.range(0.9f, 1.2f); break;
    }
    q.vines = 0.f;
    q.wild = wild;
    q.cov = wild ? 1.f : 0.f;   // the wild forest greens through the shader's cover, not the map
    plants_.push_back(q);
    plantCount_[kind]++;
}

// The life map: grass cover (R), damp dark soil (G), flowers (B), in soft round stamps.
void LandWorld::stamp(float x, float z, float radius, float g, float m, float f) {
    if (radius <= 0.f) return;
    const float cx = (x - LIFE_X0) / LIFE_CELL, cz = (LIFE_Z0 - z) / LIFE_CELL;
    const float r = radius / LIFE_CELL;
    const int x0 = std::max(0, (int)(cx - r * 1.8f)), x1 = std::min(LIFE_RES - 1, (int)(cx + r * 1.8f) + 1);
    const int z0 = std::max(0, (int)(cz - r * 1.8f)), z1 = std::min(LIFE_RES - 1, (int)(cz + r * 1.8f) + 1);
    if (x0 > x1 || z0 > z1) return;
    for (int j = z0; j <= z1; j++)
        for (int i = x0; i <= x1; i++) {
            const float dx = i + 0.5f - cx, dz = j + 0.5f - cz;
            const float w = std::exp(-(dx * dx + dz * dz) / (r * r));
            if (w < 0.02f) continue;
            const int k = j * LIFE_RES + i;
            lifeG_[k] = std::min(1.f, lifeG_[k] + g * w);
            lifeM_[k] = std::min(1.f, lifeM_[k] + m * w);
            lifeF_[k] = std::min(1.f, lifeF_[k] + f * w);
        }
    lifeDirty_ = true;
}

void LandWorld::growPlants(float wdt) {
    // the pace: slower in calm parts, faster at the peaks, several times faster in the rain (and the drop's surge)
    const float pace = (0.75f + 0.2f * ctx.lv) * (1.f + 2.2f * ctx.storm + 1.6f * surge_ + 0.4f * wet_);
    const float revive = smoothstepf(0.35f, 0.75f, life_);
    for (Plant& p : plants_) {
        if (p.wither > 0.f) {
            p.wither += wdt;
            p.g -= wdt * 0.3f;
            continue;
        }
        if (p.kind == K_DEAD) {
            p.vines = approach(p.vines, revive, 0.3f, wdt);
            continue;
        }
        p.g = std::min(1.f, p.g + wdt * p.rate * pace);
        // vines on the grown trees, in a lush land
        if ((p.kind == K_TREE || p.kind == K_EMERGENT) && p.g > 0.7f) p.vines = approach(p.vines, smoothstepf(0.45f, 0.9f, life_), 0.1f, wdt);
        if (!p.wild && p.g - p.cov > 0.05f) {
            const float d = p.g - p.cov;
            p.cov = p.g;
            const float R = COVER_R[p.kind] * p.scale;
            switch (p.kind) {
            case K_GRASS: stamp(p.p.x, p.p.z, R, 0.5f * d, 0.25f * d, 0.f); break;
            case K_FLOWER: stamp(p.p.x, p.p.z, R, 0.35f * d, 0.2f * d, 0.6f * d); break;
            case K_FERN: stamp(p.p.x, p.p.z, R, 0.45f * d, 0.4f * d, 0.f); break;
            case K_BUSH: stamp(p.p.x, p.p.z, R, 0.4f * d, 0.5f * d, 0.1f * d); break;
            default: stamp(p.p.x, p.p.z, R, 0.55f * d, 0.8f * d, 0.05f * d); break;
            }
        }
    }
    const size_t before = plants_.size();
    plants_.erase(std::remove_if(plants_.begin(), plants_.end(), [](const Plant& p) { return p.wither > 0.f && p.g <= 0.f; }), plants_.end());
    if (plants_.size() != before) {
        for (int k = 0; k < K_COUNT; k++) plantCount_[k] = 0;
        for (const Plant& p : plants_) plantCount_[p.kind]++;
    }
}

// The forest spreading on its own far out (the seeds carried further by the wind and the birds): clumps of trees on
// the plain and the hills, more and more of them as the land comes alive.
void LandWorld::spreadWild(float wdt, bool instant) {
    int have = 0;
    for (const Plant& p : plants_) have += p.wild && p.wither <= 0.f;
    const int want = (int)(1300.f * std::pow(smoothstepf(0.06f, 1.f, life_), 1.15f));
    if (have >= want) return;
    int n = want - have;
    if (!instant) {
        wildAcc_ += wdt * 30.f;
        n = std::min(n, (int)wildAcc_);
        wildAcc_ -= (float)n;
    }
    for (int i = 0, tries = 0; i < n && tries < n * 12; tries++) {
        const float z = -(30.f + 260.f * std::pow(ctx.rng.uniform(), 1.35f));
        const float hw = ctx.hwAt(z) * 0.98f;
        const float x = ctx.rng.range(-hw, hw);
        // in clumps: where the forest's noise is high (lower and lower as the land comes alive)
        const float n1 = vnoise2(x * 0.035f + 11.f, z * 0.035f) * 0.65f + vnoise2(x * 0.11f + 5.f, z * 0.11f) * 0.35f;
        if (n1 < 0.66f - 0.42f * life_) continue;
        const float w[3] = {0.66f, 0.16f, 0.18f};
        static const int KINDS[3] = {K_TREE, K_EMERGENT, K_PALM};
        int k = KINDS[ctx.rng.weighted(w)];
        // the tall ones stay out of the HUD's columns, near
        const float xs = std::fabs(x) * ctx.camDist / (ctx.camDist - z);
        if (z > -70.f && xs > 4.9f && xs < 12.6f) {
            if (z > -50.f) continue;
            k = K_PALM;
        }
        plant(vec3(x, groundH(x, z), z), k, instant ? ctx.rng.range(0.55f, 1.f) : ctx.rng.range(0.03f, 0.12f), true, ctx.rng.irange(0, 6));
        i++;
    }
}

// A land already grown (a review's stack, a game joined in progress): as if `lines` lines had been
// cleared over the last minutes, the oldest plants grown, the latest still young.
void LandWorld::seedInstant(float lines) {
    lines_ = std::max(lines_, lines);
    life_ = lifeTarget();
    const int seeds = (int)(lines * 22.f);
    for (int i = 0; i < seeds; i++) {
        const float z = -(3.f + 40.f * std::pow(ctx.rng.uniform(), 1.5f));
        const float hw = ctx.hwAt(z) * 0.94f;
        const float x = ctx.rng.range(-hw, hw);
        const int type = ctx.rng.irange(0, 6);
        const int k = pickKind(x, z, type);
        const float age = ctx.rng.uniform();
        plant(vec3(x, groundH(x, z), z), k, 0.f, false, type);
        Plant& p = plants_.back();
        p.g = GROW_S[k] < 30.f ? (age > 0.08f ? 1.f : 0.5f) : 0.2f + 0.8f * age;
    }
    // their cover at once
    for (Plant& p : plants_)
        if (!p.wild && p.kind != K_DEAD) {
            const float d = p.g - p.cov;
            p.cov = p.g;
            const float R = COVER_R[p.kind] * p.scale;
            stamp(p.p.x, p.p.z, R, 0.5f * d, 0.5f * d, p.kind == K_FLOWER ? 0.6f * d : 0.f);
            if ((p.kind == K_TREE || p.kind == K_EMERGENT) && p.g > 0.7f) p.vines = smoothstepf(0.45f, 0.9f, life_);
        }
    for (Plant& p : plants_)
        if (p.kind == K_DEAD) p.vines = smoothstepf(0.35f, 0.75f, life_);
    spreadWild(0.f, true);
    birds_.clear();
}


// =====================================================================================================
// Birds, butterflies, fireflies
// =====================================================================================================

void LandWorld::spawnBird(int kind, bool onScreen, bool flock) {
    if (birds_.size() >= 90) return;
    Bird b{};
    b.kind = kind;
    b.phase = ctx.rng.range(0.f, TAU);
    b.alpha = onScreen ? 1.f : 0.f;
    b.flap = ctx.rng.range(0.f, TAU);
    b.life = ctx.rng.range(25.f, 60.f);
    if (kind == 2 || kind == 3) {
        // a butterfly near the flowers, a firefly among the plants: near, low, fluttering around a point
        const Plant* near = nullptr;
        for (int t = 0; t < 12 && !near; t++) {
            if (plants_.empty()) break;
            const Plant& p = plants_[ctx.rng.irange(0, (int)plants_.size() - 1)];
            if (p.wild || p.p.z < -26.f || p.kind == K_DEAD) continue;
            if (kind == 2 && p.kind != K_FLOWER && p.kind != K_BUSH && p.kind != K_GRASS) continue;
            near = &p;
        }
        float x, z;
        if (near) { x = near->p.x; z = near->p.z; }
        else { z = -ctx.rng.range(3.f, 20.f); x = ctx.rng.range(-ctx.hwAt(z), ctx.hwAt(z)) * 0.8f; }
        b.c = vec3(x, groundH(x, z), z);
        b.r = ctx.rng.range(0.4f, 1.6f);
        b.w = ctx.rng.range(0.4f, 0.9f) * (ctx.rng.chance(0.5f) ? 1.f : -1.f);
        b.h = kind == 2 ? ctx.rng.range(0.5f, 1.8f) : ctx.rng.range(0.3f, 3.f);
        b.size = kind == 2 ? ctx.rng.range(0.13f, 0.2f) : ctx.rng.range(0.05f, 0.08f);
        b.life = ctx.rng.range(10.f, 25.f);
        static const int pick[4] = {0, 2, 4, 6};
        b.col = kind == 3 ? vec3(1.f, 0.9f, 0.45f) : ctx.rng.chance(0.35f) ? ctx.pal.flowerB : ctx.pal.piece[pick[ctx.rng.irange(0, 3)]];
        b.ang = ctx.rng.range(0.f, TAU);
        birds_.push_back(b);
        return;
    }
    // a bird: circling over a spot of the land, between and above the trees (higher as they grow), or crossing
    const float z = -ctx.rng.range(5.f, 55.f);
    const float hw = ctx.hwAt(z);
    const float trees = 4.f + 12.f * life_;
    b.size = ctx.rng.range(0.22f, 0.32f) * (1.f + 0.6f * smoothstepf(10.f, 50.f, -z));
    b.col = kind == 1 ? (ctx.rng.chance(0.5f) ? ctx.pal.accent : lerp(ctx.pal.flowerA, ctx.pal.piece[ctx.rng.irange(0, 6)], 0.4f)) : ctx.pal.bird;
    if (flock || ctx.rng.chance(0.35f)) {
        // crossing the view in a line
        const float dir = ctx.rng.chance(0.5f) ? 1.f : -1.f;
        const float x = onScreen ? ctx.rng.range(-hw, hw) : -dir * (hw + 3.f);
        b.p = vec3(x, groundH(x, z) + ctx.rng.range(2.f, trees + 4.f), z);
        b.v = vec3(dir * ctx.rng.range(3.5f, 6.f), ctx.rng.range(-0.2f, 0.4f), ctx.rng.range(-0.8f, 0.8f));
        b.r = 0.f;
        b.life = 2.f * (hw + 6.f) / std::fabs(b.v.x);
    } else {
        const float x = ctx.rng.range(-hw, hw) * 0.85f;
        b.c = vec3(x, groundH(x, z) + ctx.rng.range(1.5f, trees), z);
        b.r = ctx.rng.range(3.f, 9.f);
        b.w = ctx.rng.range(0.25f, 0.5f) * (ctx.rng.chance(0.5f) ? 1.f : -1.f) * 4.f / b.r;
        b.ang = ctx.rng.range(0.f, TAU);
        b.p = b.c;
    }
    birds_.push_back(b);
}

void LandWorld::simulateBirds(float dt, float wdt, float world) {
    const float wm = smoothstepf(0.2f, 1.f, ctx.lv), wp = smoothstepf(1.1f, 2.f, ctx.lv);
    int have[4] = {0, 0, 0, 0};
    for (const Bird& b : birds_) have[b.kind == 1 ? 0 : b.kind]++;
    const float L = life_;
    const int wantBirds = L < 0.12f ? (L > 0.05f ? 1 : 0) : (int)(2.f + 12.f * L + 5.f * wp * L + 6.f * ctx.storm * L);
    const int wantFlies = L < 0.25f ? 0 : (int)((3.f + 9.f * wp + 3.f * wm) * smoothstepf(0.25f, 0.8f, L) * (1.f - ctx.storm) * (1.f - ctx.pal.night));
    const int wantGlow = (int)((ctx.pal.night * (4.f + 26.f * L) + ctx.slow * (6.f + 24.f * L)) * (1.f - 0.7f * ctx.storm));
    birdT_ -= wdt;
    if (birdT_ <= 0.f && world > 0.5f && !ctx.over) {
        birdT_ = ctx.rng.range(0.3f, 1.2f);
        if (have[0] < wantBirds) spawnBird(L > 0.4f && ctx.rng.chance(0.3f) ? 1 : 0, false, false);
        if (have[2] < wantFlies) spawnBird(2, false, false);
        for (int i = 0; i < 3 && have[3] + i < wantGlow; i++) spawnBird(3, false, false);
    }
    for (Bird& b : birds_) {
        b.life -= wdt;
        const float fadeOut = b.life < 1.5f ? 0.f : 1.f;
        const bool shy = (b.kind == 2 && ctx.storm > 0.5f) || (b.kind == 3 && ctx.pal.night < 0.5f && ctx.slow < 0.3f);
        b.alpha = approach(b.alpha, shy ? 0.f : fadeOut, b.kind >= 2 ? 1.5f : 2.f, dt);
        if (b.kind >= 2) {
            // fluttering around its point
            b.ang += b.w * wdt * (b.kind == 2 ? 1.6f : 0.5f);
            const float t = ctx.worldTime + b.phase;
            const vec3 q = b.c + vec3(std::cos(b.ang) * b.r + 0.3f * std::sin(t * 2.3f), b.h + 0.35f * std::sin(t * 1.7f) + 0.15f * std::sin(t * 5.1f),
                                      std::sin(b.ang) * b.r * 0.6f);
            b.v = (q - b.p) * (1.f / std::max(wdt, 1e-3f));
            b.p = q;
            b.flap += wdt * (b.kind == 2 ? 16.f : 1.f);
            continue;
        }
        if (b.r > 0.f) {
            b.ang += b.w * wdt;
            const vec3 q = b.c + vec3(std::cos(b.ang) * b.r, 0.6f * std::sin(b.ang * 2.f + b.phase), std::sin(b.ang) * b.r * 0.45f);
            b.v = (q - b.p) * (1.f / std::max(wdt, 1e-3f));
            b.p = q;
        } else {
            b.p += b.v * wdt;
        }
        // flapping, then gliding
        const float glide = 0.5f + 0.5f * std::sin(ctx.worldTime * 0.7f + b.phase * 3.f);
        b.flap += wdt * (glide > 0.7f ? 2.f : 11.f);
    }
    birds_.erase(std::remove_if(birds_.begin(), birds_.end(),
                                [&](const Bird& b) { return (b.life <= 0.f && b.alpha < 0.02f) || std::fabs(b.p.x) > ctx.hwAt(b.p.z) + 12.f; }),
                 birds_.end());
}

// =====================================================================================================
// Clearing: the cubes break open, their seeds fly out and are sown
// =====================================================================================================

// A cube breaks open: it is gone at once (nothing over the cleared row), and its seeds burst out of where it was, each
// on its own high arc to a spot of the land behind the well. The `heroes` are sown near, in the open land on either
// side (where they will be seen sprouting); the others anywhere further out.
void LandWorld::breakCube(const vec3& p, int type, float power, float seeds, int heroes, float pause) {
    type = std::clamp(type, 0, 6);
    const int n = std::max(heroes, (int)seeds + (ctx.rng.uniform() < seeds - std::floor(seeds) ? 1 : 0));
    for (int i = 0; i < n && parts_.size() < 7000; i++) {
        Particle q{};
        q.hero = i < heroes;
        float x, z;
        if (q.hero) {
            // near, out on the open land beside the well (the side the cube was on, mostly), beyond the HUD's columns
            // or between them and the well
            z = -ctx.rng.range(3.f, 13.f);
            const float k = (ctx.camDist - z) / ctx.camDist;   // board plane -> depth z, as seen
            const float side = (p.x < 0.f) == ctx.rng.chance(0.75f) ? -1.f : 1.f;
            const float xs = ctx.rng.chance(0.7f) ? ctx.rng.range(13.2f, 20.f) : ctx.rng.range(5.8f, 11.5f);
            x = std::clamp(side * xs * k, -ctx.hwAt(z) * 0.92f, ctx.hwAt(z) * 0.92f);
            q.pause = pause + 0.2f * (float)i + ctx.rng.range(0.f, 0.25f);
        } else {
            z = -(9.f + 40.f * std::pow(ctx.rng.uniform(), 1.4f));
            const float hw = ctx.hwAt(z) * 0.94f;
            x = std::clamp(p.x * 1.5f + ctx.rng.range(-hw, hw), -hw, hw);
            q.pause = ctx.rng.range(0.3f, 3.f);
        }
        const float ty = groundH(x, z);
        q.p = p + vec3(ctx.rng.range(-0.3f, 0.3f), ctx.rng.range(-0.3f, 0.3f), -0.1f);
        // a high arc: up from the cube to its apex, then down onto its spot
        q.grav = SEED_GRAV;
        const float apex = std::max(q.p.y, ty) + ctx.rng.range(2.2f, 4.2f) * (0.8f + 0.25f * power);
        const float up = std::sqrt(2.f * (apex - q.p.y) / q.grav), down = std::sqrt(2.f * (apex - ty) / q.grav);
        const float T = up + down;
        q.v = vec3((x - q.p.x) / T, up * q.grav, (z - q.p.z) / T);
        q.life = T + 3.f;
        q.size = ctx.rng.range(0.17f, 0.22f) * (q.hero ? 1.15f : 1.f);
        q.drag = 0.f;
        q.kind = P_SEED; q.type = type; q.real = true; q.seed = ctx.rng.uniform(); q.spin = ctx.rng.range(-9.f, 9.f);
        q.col = ctx.pal.piece[type];
        parts_.push_back(q);
    }
}

void LandWorld::dustPuff(const vec3& p, int n, float power) {
    for (int i = 0; i < n && parts_.size() < 7000; i++) {
        Particle q{};
        q.p = p + vec3(ctx.rng.range(-0.12f, 0.12f), 0.04f, ctx.rng.range(-0.12f, 0.12f));
        q.v = vec3(ctx.rng.range(-0.9f, 0.9f), ctx.rng.range(0.2f, 0.8f), ctx.rng.range(-0.9f, 0.9f)) * power;
        q.life = ctx.rng.range(0.5f, 0.9f);
        q.size = ctx.rng.range(0.1f, 0.2f) * power;
        q.grav = -0.2f; q.drag = 2.5f;
        q.kind = P_DUST; q.real = true; q.seed = ctx.rng.uniform();
        q.col = lerp(ctx.pal.soilDry, ctx.pal.dust, 0.5f);
        parts_.push_back(q);
    }
}

int LandWorld::heroes() const {
    int n = 0;
    for (const Sow& s : sows_) n += s.hero;
    for (const Particle& p : parts_) n += p.kind == P_SEED && p.hero;
    return n;
}

// A seed at rest on the soil: it will bury itself, and sprout.
void LandWorld::settle(const Particle& s) {
    Sow w{};
    w.p = vec3(s.p.x, groundH(s.p.x, s.p.z), s.p.z);
    w.col = s.col;
    w.type = s.type;
    w.kind = pickKind(w.p.x, w.p.z, s.type);
    w.t = 0.f;
    w.pause = s.pause;
    w.rot = ctx.rng.range(0.f, TAU);
    w.seed = s.seed;
    w.size = s.size;
    w.spin = s.spin * s.age + s.seed * 6.f;   // it lies as it fell
    w.scale = ctx.rng.range(1.2f, 1.45f);
    w.hero = s.hero;
    w.planted = false;
    sows_.push_back(w);
}

// The sown seeds: each one settles (0.35 s), sinks into the soil as a small mound heaps up over it (to 0.95 s);
// near, after its pause, a sprout pushes out of the mound and opens (1.8 s), the plant grows out of it, the sprout's
// seed leaves wither away and the mound settles flat. Far out, the plant just comes up after its pause.
void LandWorld::simulateSows(float dt) {
    for (Sow& s : sows_) {
        s.t += dt;
        if (!s.hero) {
            if (!s.planted && s.t >= 0.95f + s.pause) {
                s.planted = true;
                if (plants_.size() < 3500) plant(s.p, s.kind, 0.f, false, s.type);
                else stamp(s.p.x, s.p.z, 1.5f, 0.2f, 0.2f, 0.05f);
            }
            continue;
        }
        if (!s.planted && sproutAt(s) >= 1.5f) {
            // the plant takes over from the opened sprout
            s.planted = true;
            plant(s.p, s.kind, 0.f, false, s.type);
            Plant& q = plants_.back();
            q.rot = s.rot;
        }
    }
    sows_.erase(std::remove_if(sows_.begin(), sows_.end(),
                               [this](const Sow& s) { return s.hero ? sproutAt(s) > 7.f : s.planted && s.t > 1.f + s.pause; }),
                sows_.end());
}

void LandWorld::onEvent(const GameEvent& ev, const Game&) {
    switch (ev.type) {
    case GameEvent::Clear: {
        // The row's cubes break open at once (the row itself is gone from the board), their seeds flying out; a few
        // of them sown near, to be seen sprouting one after the other (never more than a dozen at once).
        const bool tetris = ev.count >= 4;
        int n = 0;
        for (auto& c : ev.cells) n += !(c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W);
        const int want = std::max(0, std::min(2 + 2 * ev.count, MAX_SPROUTS - heroes()));
        int i = 0, given = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            // the heroes spread evenly over the row's cubes
            const int h = n > 0 ? ((i + 1) * want) / n - (i * want) / n : 0;
            breakCube(layout::cellPos((float)c.x, (float)c.y), c.type, tetris ? 1.25f : 1.f, tetris ? 3.2f : 2.4f, h,
                      0.2f * (float)given);
            given += h;
            i++;
        }
        lines_ += (float)ev.count * (tetris ? 1.5f : 1.f);
        if (tetris) {
            // a Tetris: every flower opens, a flock rises from the land
            bloom_ = 1.f;
            ctx.gust = std::max(ctx.gust, 0.8f);
            if (life_ > 0.1f)
                for (int k = 0; k < 8; k++) spawnBird(life_ > 0.4f && k % 3 == 0 ? 1 : 0, true, true);
        }
        break;
    }
    case GameEvent::Lock:
    case GameEvent::HardDrop: {
        if (ev.cells.empty()) break;
        // a small puff of dry dust pushed out from under the piece, at the contact (none once the soil is wet)
        const bool hard = ev.type == GameEvent::HardDrop;
        if (ev.type == GameEvent::Lock && !puffs_.empty() && puffs_.back().age < 0.05f) break; // a hard drop's lock
        float lowY = 1e9f;
        for (auto& c : ev.cells) lowY = std::min(lowY, layout::cellPos((float)c.x, (float)c.y).y);
        const float a = 1.f - 0.8f * wet_;
        if (a < 0.1f) break;
        for (auto& c : ev.cells) {
            const vec3 cp = layout::cellPos((float)c.x, (float)c.y);
            if (cp.y > lowY + 0.1f) continue;
            const int k = hard ? 3 : 1;
            for (int j = 0; j < k; j++) {
                Puff q;
                const float side = ctx.rng.chance(0.5f) ? 1.f : -1.f;
                q.p = v2(cp.x + ctx.rng.range(-0.45f, 0.45f), cp.y - 0.48f);
                q.v = v2(side * ctx.rng.range(0.6f, 1.8f) * (hard ? 1.3f : 0.8f), ctx.rng.range(0.1f, 0.5f));
                q.age = 0.f;
                q.life = ctx.rng.range(0.45f, 0.7f);
                q.size = ctx.rng.range(0.16f, 0.26f) * (hard ? 1.f : 0.75f) * a;
                puffs_.push_back(q);
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

void LandWorld::simulateParticles(float dt, float wdt) {
    std::vector<Particle> rest;
    std::vector<Particle> trails;
    for (Particle& p : parts_) {
        const float pdt = p.real ? dt : wdt;
        p.age += pdt;
        if (p.age < 0.f) continue;
        if (p.kind == P_MOTE || p.kind == P_PETAL) {
            // drifting on the breeze
            p.v.x += (wind_ * 0.6f + ctx.gust * 0.8f) * pdt * (p.kind == P_PETAL ? 1.5f : 0.5f);
            p.v.y += std::sin(ctx.time * 0.9f + p.seed * 30.f) * 0.15f * pdt;
        }
        p.v.y -= p.grav * pdt;
        p.v *= std::exp(-p.drag * pdt);
        p.p += p.v * pdt;
        if (p.kind == P_SEED) {
            // a faint trail of its color behind it, in flight
            if (p.bounces == 0) {
                p.trail += pdt;
                while (p.trail > 0.022f && trails.size() < 2000) {
                    p.trail -= 0.022f;
                    Particle t{};
                    t.p = p.p - p.v * p.trail;
                    t.life = p.hero ? 0.42f : 0.3f;
                    t.size = p.size * 0.45f;
                    t.kind = P_TRAIL; t.real = true; t.seed = p.seed;
                    t.col = lerp(p.col, vec3(1.f), 0.35f);
                    trails.push_back(t);
                }
            }
            const float gy = groundH(p.p.x, p.p.z);
            if (p.v.y < 0.f && p.p.z < FRONT_Z - 0.5f && p.p.y - p.size * 0.4f <= gy) {
                if (p.bounces == 0) {
                    // the impact: a puff of dust (a splash on wet soil), a small hop
                    const vec3 at(p.p.x, gy, p.p.z);
                    if (wet_ < 0.5f) dustPuff(at, p.hero ? 3 : 2, p.hero ? 0.55f : 0.4f);
                    else
                        for (int i = 0; i < 3 && parts_.size() + trails.size() < 7000; i++) {
                            Particle q{};
                            q.p = at + vec3(0.f, 0.03f, 0.f);
                            q.v = vec3(ctx.rng.range(-0.8f, 0.8f), ctx.rng.range(1.2f, 2.2f), ctx.rng.range(-0.8f, 0.8f));
                            q.life = 0.35f; q.size = 0.04f; q.grav = 12.f; q.drag = 0.4f;
                            q.kind = P_SPLASH; q.real = true; q.seed = ctx.rng.uniform();
                            q.col = lerp(ctx.pal.mist, vec3(1.f), 0.4f);
                            trails.push_back(q);
                        }
                    p.p.y = gy + p.size * 0.4f;
                    p.v = vec3(p.v.x * 0.18f, -p.v.y * 0.2f, p.v.z * 0.18f);
                    p.spin *= 0.4f;
                    p.bounces = 1;
                } else {
                    // at rest
                    p.p.y = gy;
                    rest.push_back(p);
                    p.age = p.life + 1.f;
                }
            }
            continue;
        }
        if ((p.kind == P_SPLASH || p.kind == P_DUST || p.kind == P_PETAL) && p.p.z < FRONT_Z && p.p.y < groundH(p.p.x, p.p.z) - 0.05f) {
            if (p.kind == P_PETAL) { p.v = vec3(0.f); p.grav = 0.f; p.p.y = groundH(p.p.x, p.p.z) + 0.02f; }
            else p.age = p.life + 1.f;
        }
    }
    parts_.erase(std::remove_if(parts_.begin(), parts_.end(), [](const Particle& p) { return p.age > p.life; }), parts_.end());
    parts_.insert(parts_.end(), trails.begin(), trails.end());
    for (const Particle& s : rest) settle(s);
    for (Puff& q : puffs_) {
        q.age += dt;
        q.p = v2(q.p.x + q.v.x * dt, q.p.y + q.v.y * dt);
        const float k = std::exp(-3.f * dt);
        q.v = v2(q.v.x * k, q.v.y * k);
    }
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(), [](const Puff& q) { return q.age > q.life; }), puffs_.end());
}

void LandWorld::simulate(const Game& game, const MusicState& music, float dt, float wdt) {
    (void)game;
    (void)music;
    if (!started_) resetLand();
    const float wm = ctx.wm(), wp = ctx.wp(), world = ctx.world();

    // the land comes alive with the cleared lines (slowly, so a clear is seen growing)
    life_ = approach(life_, lifeTarget(), 0.35f, dt);
    // the rain soaks the soil (it dries slowly after), the drop's surge eases
    wet_ = ctx.storm > 0.2f ? std::min(1.f, wet_ + dt * 0.45f * ctx.storm) : std::max(0.f, wet_ - dt / 45.f);
    surge_ = approach(surge_, 0.f, 0.08f, dt);
    // the breeze: still at calm, a breeze at mid, wind at a peak, gusts of rain in a drop
    const float windT = (0.08f + 0.25f * wm + 0.3f * wp + 0.35f * ctx.storm) * (1.f - 0.6f * ctx.slow);
    wind_ = approach(wind_, windT, 0.8f, dt);
    swayT_ += wdt * (0.6f + 0.6f * wind_);
    bloom_ = approach(bloom_, saturate(0.15f + 0.3f * wm + 0.45f * wp + 0.3f * ctx.storm), 0.25f, dt);

    // the rain's splashes on the land near
    if (ctx.storm > 0.05f && world > 0.5f) {
        const int n = (int)(ctx.rng.uniform() + 90.f * ctx.storm * dt);
        for (int i = 0; i < n && parts_.size() < 6000; i++) {
            const float z = -ctx.rng.range(2.f, 22.f);
            const float x = ctx.rng.range(-ctx.hwAt(z), ctx.hwAt(z));
            Particle q{};
            q.p = vec3(x, groundH(x, z) + 0.02f, z);
            q.v = vec3(ctx.rng.range(-0.4f, 0.4f), ctx.rng.range(1.f, 2.f), ctx.rng.range(-0.4f, 0.4f));
            q.life = 0.28f; q.size = 0.035f; q.grav = 12.f; q.drag = 0.3f;
            q.kind = P_SPLASH; q.real = false; q.seed = ctx.rng.uniform();
            q.col = lerp(ctx.pal.mist, vec3(1.f), 0.5f);
            parts_.push_back(q);
        }
    }
    // dust motes in the dry air, pollen in a living land: still at calm, more at mid, a drift of them at a peak
    if (world > 0.5f && ctx.storm < 0.5f) {
        const float rate = (2.f + 6.f * wm + 14.f * wp) * (1.f - ctx.storm);
        const int n = (int)(ctx.rng.uniform() + rate * wdt);
        for (int i = 0; i < n && parts_.size() < 6000; i++) {
            const float z = -ctx.rng.range(2.f, 24.f);
            const float x = ctx.rng.range(-ctx.hwAt(z), ctx.hwAt(z));
            Particle q{};
            q.p = vec3(x, groundH(x, z) + ctx.rng.range(0.3f, 6.f), z);
            q.v = vec3(ctx.rng.range(-0.15f, 0.15f), ctx.rng.range(-0.05f, 0.1f), ctx.rng.range(-0.1f, 0.1f));
            q.life = ctx.rng.range(4.f, 7.f); q.size = ctx.rng.range(0.025f, 0.045f); q.grav = 0.f; q.drag = 0.2f;
            q.kind = P_MOTE; q.real = false; q.seed = ctx.rng.uniform();
            q.col = lerp(lerp(ctx.pal.dust, vec3(1.f), 0.4f), lerp(ctx.pal.flowerB, vec3(1.f), 0.3f), smoothstepf(0.3f, 0.8f, life_));
            parts_.push_back(q);
        }
    }

    growPlants(wdt);
    spreadWild(wdt, false);
    simulateBirds(dt, wdt, world);
    simulateParticles(dt, wdt);
    simulateSows(dt);
}

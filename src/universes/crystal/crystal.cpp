// CRYSTAL CAVE: gemstone pieces in a geode cave, each phase of the song its own mineral.
//
// One world: a dark cave inside a geode, its walls lined with crystal clusters that glow softly from within, a
// still pool on its floor, water dripping from the crystals hanging from the vault. The pieces are cut gems: each
// piece one stone (its cells merged, a thin seam between stones), faceted over its whole outline like an emerald cut
// (steps that follow the outline, mitred at the corners, each side stepped on its own and broken by slanted cuts, a
// table of irregular facets where the stone is wide). Each facet mirrors another part of the stone: its brightness and
// shade are seeded per stone (the falling piece already has the cut it will lock with), with a slight dispersion and
// rare complementary sparks, a deep glowing core, a paler girdle and crisp highlights.
//
// The song chooses the mineral. Each level of the song has its own (calm, mid, peak) and the drop a special one;
// a new section of the same level moves the mineral on to another of its three tones, so two choruses stay apart.
// The falling piece (and the previews) is cut from the mineral of the moment, easing into the next one when the
// phase changes; a stone keeps the color it locked in, so the stack grows in strata like the bands of an agate and
// records the song's structure. Within a mineral the seven piece types differ by a slight shade and their own cut
// of inner facets. The cave's crystals echo the minerals in the stack (each crystal takes a share of them, slowly).
//
// Staging: a calm part is a dim, still cave (a drop of water every four beats); mid lights the crystals more and
// the water drips every two beats; a peak lights the whole cave, two drops every two beats, the kicks make one
// cluster glow. The highs make crystals glint (a small star on a crystal's point, a soft flash on a gem's crown).
// The drop turns the mineral to the drop's stone and lights the cave in it, eased in over a beat or two and
// settling after. A cleared row shatters into shards that tumble, flashing, and fall into the pool, where they
// sink (the row is empty in under half a second); a quad rings every crystal of the cave, a ring of light running
// outward from the well. Slow motion stills the cave in a cold, silver light. Game over puts the light out of the
// stack's stones, row by row from the top.
//
// Cost: one full-screen pass for the rock, ~150 instanced crystals, one pass for the pool (the cave mirrored),
// instanced gems, a quarter-resolution bloom.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/crystal/crystal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/crystal/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Marcellus (the labels and numbers, carved capitals) and Cormorant Garamond Italic (the song, the section, the
// celebrations), SIL Open Font License, see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_REG[] = {
#include "universes/crystal/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/crystal/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = crystal_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A cave: its rock (in the dark, where light falls), the agate shell of the geode, the cave's depth, the pool's
// dark, the lettering, and its four minerals (calm, mid, peak, drop), each in three tones (one per section).
struct VariantDef {
    const char* name;
    unsigned rock, rockLit, band, fog, deep, ink;
    unsigned min[4][3];
    const char* minName[4];
};
const VariantDef VARIANTS[] = {
    {"GEODE", 0x0d0a0f, 0x3b2f3a, 0x6e5e68, 0x0a080d, 0x040508, 0xece6ef,
     {{0x9a8270, 0x7f7a80, 0xb08a6a}, {0x9b5de5, 0x6e58e6, 0xc565d2}, {0x2fbf71, 0x1fae9a, 0x6cc954}, {0xf2b632, 0xf58f2c, 0xeed64e}},
     {"smoky quartz", "amethyst", "emerald", "citrine"}},
    {"AMETHYST", 0x0e0815, 0x3a2550, 0x6a4c80, 0x0c0715, 0x05040b, 0xeee8f6,
     {{0x80a8de, 0x9a9fd6, 0x62b8de}, {0xa45ce2, 0x7656e6, 0xc95fd0}, {0x3cc49a, 0x62c066, 0x27b7c0}, {0xf59ac0, 0xf7ad9c, 0xe788d8}},
     {"celestite", "amethyst", "tourmaline", "rose quartz"}},
    {"ICE", 0x070b12, 0x22374c, 0x52708a, 0x060a12, 0x03060b, 0xe6eef6,
     {{0xa6bacd, 0x93a6d0, 0xb6c8c4}, {0x45c6d2, 0x5ad6b4, 0x3caee2}, {0x6c98ff, 0x8a84ff, 0x52b0ff}, {0xffc23a, 0xffa042, 0xf8dc66}},
     {"rock crystal", "aquamarine", "sapphire", "citrine"}},
    {"EMBER", 0x100907, 0x3c2419, 0x7a5642, 0x0f0806, 0x070504, 0xf2e8e0,
     {{0x9e8474, 0x8a7f7c, 0xae8a68}, {0xea922e, 0xf0742c, 0xdcae36}, {0xe4384e, 0xd22f6c, 0xec5236}, {0xbfeaf6, 0xd8d4ff, 0xb8f4d8}},
     {"smoky topaz", "amber", "ruby", "opal"}},
    {"FLUORITE", 0x061010, 0x1c3b3b, 0x3f6c66, 0x050d0e, 0x030807, 0xe4f2ee,
     {{0x88d2ac, 0x74c4c4, 0xa2d690}, {0x8e6ce2, 0xa660cc, 0x7276ee}, {0x5ca8ff, 0x3cc4f0, 0x7290ff}, {0xf2e04c, 0xdcef5e, 0xfac63c}},
     {"green fluorite", "purple fluorite", "blue fluorite", "yellow fluorite"}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The seven piece types inside one mineral: a slight turn of hue and a value of their own.
const float TYPE_HUE[7] = {0.f, -8.f, 8.f, -4.f, 4.f, -12.f, 12.f}; // degrees
const float TYPE_VAL[7] = {1.1f, 0.9f, 1.02f, 1.22f, 0.86f, 0.96f, 1.16f};

// A hue turn (YIQ, the luma kept) and a value, on a linear color (done in display space).
vec3 shade(const vec3& linCol, float deg, float val) {
    const vec3 c = ugl::toSrgb(linCol);
    const float y = 0.299f * c.x + 0.587f * c.y + 0.114f * c.z;
    const float i = 0.596f * c.x - 0.274f * c.y - 0.322f * c.z;
    const float q = 0.211f * c.x - 0.523f * c.y + 0.312f * c.z;
    const float a = deg * PI / 180.f, ca = std::cos(a), sa = std::sin(a);
    const float i2 = i * ca - q * sa, q2 = i * sa + q * ca;
    vec3 o{y + 0.956f * i2 + 0.621f * q2, y - 0.272f * i2 - 0.647f * q2, y - 1.106f * i2 + 1.703f * q2};
    o = o * val;
    return ugl::toLinear(vec3(saturate(o.x), saturate(o.y), saturate(o.z)));
}
// A stone's own cut (the reflections inside it), from its id: the falling piece takes the id it will lock with.
float seedOf(int id) { return std::fmod((float)id * 0.618034f, 1.f) * 97.f + 3.f; }

vec3 typeTint(const vec3& linCol, int type) {
    type = std::clamp(type, 0, 6);
    return shade(linCol, TYPE_HUE[type], TYPE_VAL[type]);
}

// Water falls this fast (board units / s^2), shards this fast.
constexpr float G_WATER = 26.f, G_SHARD = 34.f;
// The drop's mineral stays this many beats, then the peak's own comes back.
constexpr double DROP_BEATS = 32.0;

void instAttribs(GLuint vao, GLuint vbo, GLuint quadVbo, int vec4s, size_t stride) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < vec4s; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool CrystalUniverse::init(int width, int height) {
    using shaders::withCommon;
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::BG_FS, false).c_str(), "crystal rock");
    progPool_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::POOL_FS, false).c_str(), "crystal pool");
    progCrys_ = ugl::program(shaders::CRYS_VS, withCommon(shaders::CRYS_FS, false).c_str(), "crystal crystals");
    progGlow_ = ugl::program(shaders::GLOW_VS, shaders::GLOW_FS, "crystal glows");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS, true).c_str(), "crystal gems");
    progShard_ = ugl::program(shaders::SHARD_VS, shaders::SHARD_FS, "crystal shards");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "crystal down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "crystal blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "crystal final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "crystal shape");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    instAttribs(cellVao_, cellVbo_, quadVbo_, 4, sizeof(CellInst));
    glGenVertexArrays(1, &crysVao_);
    glGenBuffers(1, &crysVbo_);
    instAttribs(crysVao_, crysVbo_, quadVbo_, 3, sizeof(CrysInst));
    glGenVertexArrays(1, &glowVao_);
    glGenBuffers(1, &glowVbo_);
    instAttribs(glowVao_, glowVbo_, quadVbo_, 3, sizeof(GlowInst));
    glGenVertexArrays(1, &shardVao_);
    glGenBuffers(1, &shardVbo_);
    instAttribs(shardVao_, shardVbo_, quadVbo_, 3, sizeof(ShardInst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void CrystalUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    // the pool's far shore: a little under the lowest HUD lines, the water down to the screen's bottom
    waterY_ = viewC_.y - hh + std::min(2.4f, hh * 0.2f);
    bgT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    placeCrystals();
}

CrystalUniverse::~CrystalUniverse() {
    for (ugl::Target* t : {&bgT_, &sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progBg_, progPool_, progCrys_, progGlow_, progCell_, progShard_, progDown_, progBlur_, progFinal_,
                     progShape_})
        glDeleteProgram(p);
    for (GLuint v : {cellVao_, crysVao_, glowVao_, shardVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, crysVbo_, glowVbo_, shardVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The cave's crystals
// =====================================================================================================

// Clusters lining the geode's walls in the outer margins (clear of the HUD columns), a few hanging from the vault
// (the water drips from their points), a few on the pool's far shore, and a druse of small ones along both walls.
void CrystalUniverse::placeCrystals() {
    crys_.clear();
    clusters_.clear();
    dripSrc_.clear();
    Rng r(seed_ ^ 0xC2E5ull);
    const float L = viewC_.x - viewH_.x, R = viewC_.x + viewH_.x;
    const float T = viewC_.y + viewH_.y;
    const float room = viewH_.x - 11.6f;
    auto add = [&](vec2 base, float angle, float len, float wk, float depth, float alpha, int cluster) {
        Crys c;
        c.base = base;
        c.angle = angle;
        c.len = len;
        c.width = len * wk;
        c.tip = r.range(0.12f, 0.24f);
        c.depth = depth;
        c.alpha = alpha;
        c.u = r.uniform();
        c.phase = r.range(0.f, TAU);
        c.cluster = cluster;
        crys_.push_back(c);
    };
    auto cluster = [&](vec2 c, float rad) {
        clusters_.push_back({c, rad, vec3(0.f)});
        return (int)clusters_.size() - 1;
    };
    if (room > 1.6f) {
        const float reach = std::min(room * 0.75f + 0.6f, 6.f);
        // the walls: two clusters on each side, pointing into the cave
        for (int side = 0; side < 2; side++) {
            const float sx = side == 0 ? L - 0.4f : R + 0.4f;
            const float a0 = side == 0 ? 0.f : PI;
            const float ys[2] = {side == 0 ? 2.5f : 4.f, side == 0 ? -3.5f : -2.5f};
            for (int k = 0; k < 2; k++) {
                const float cy = viewC_.y + ys[k] + r.range(-1.f, 1.f);
                const int ci = cluster(vec2{sx + (side == 0 ? 1.f : -1.f) * reach * 0.45f, cy}, reach * 0.75f);
                const int n = 8 + r.irange(0, 4);
                for (int i = 0; i < n; i++) {
                    const float t = r.uniform();
                    const float spread = (t - 0.5f) * 1.7f + r.range(-0.35f, 0.35f);
                    const float ang = a0 + (side == 0 ? spread : -spread);
                    // a few long points among many shorter, stubby ones
                    const float len = reach * (r.chance(0.3f) ? r.range(0.7f, 1.f) : r.range(0.25f, 0.6f)) * (1.f - 0.4f * std::fabs(t - 0.5f));
                    const vec2 base{sx, cy + (t - 0.5f) * reach * 0.6f + r.range(-0.4f, 0.4f)};
                    add(base, ang, len, r.range(0.15f, 0.24f) * (len < reach * 0.5f ? 1.15f : 0.9f), r.range(0.f, 0.45f), 0.95f, ci);
                }
            }
        }
        // hanging from the vault on the right (the left corner keeps the song's title clear): water gathers on
        // their points and drips
        for (int side = 1; side < 2; side++) {
            const float sgn = side == 0 ? -1.f : 1.f;
            const float cx = viewC_.x + sgn * (11.6f + room * r.range(0.4f, 0.62f));
            const int ci = cluster(vec2{cx, T - 1.2f}, 2.6f);
            const int n = 4 + r.irange(0, 2);
            for (int i = 0; i < n; i++) {
                const float ang = -PI * 0.5f + r.range(-0.45f, 0.45f);
                const float len = r.range(1.f, 3.f) * (i == 0 ? 1.15f : 1.f);
                const vec2 base{cx + r.range(-1.3f, 1.3f), T + 0.35f};
                add(base, ang, len, r.range(0.12f, 0.18f), r.range(0.f, 0.3f), 0.9f, ci);
                if (i < 3) dripSrc_.push_back(vec2{base.x + std::cos(ang) * len * 0.97f, base.y + std::sin(ang) * len * 0.97f});
            }
        }
        // on the far shore of the pool
        for (int side = 0; side < 2; side++) {
            const float sgn = side == 0 ? -1.f : 1.f;
            const float cx = viewC_.x + sgn * (12.2f + room * r.range(0.35f, 0.7f));
            const int ci = cluster(vec2{cx, waterY_ + 1.f}, 2.4f);
            for (int i = 0; i < 4; i++) {
                const float ang = PI * 0.5f + r.range(-0.55f, 0.55f);
                add(vec2{cx + r.range(-1.2f, 1.2f), waterY_ + 0.05f}, ang, r.range(0.8f, 2.4f), r.range(0.12f, 0.18f),
                    r.range(0.1f, 0.4f), 0.9f, ci);
            }
        }
    }
    // a druse along both walls: many small crystals
    for (int side = 0; side < 2; side++) {
        const float sx = side == 0 ? L - 0.1f : R + 0.1f;
        const int ci = clusters_.empty() ? cluster(vec2{sx, viewC_.y}, 4.f) : 0;
        for (int i = 0; i < 36; i++) {
            const float y = r.range(waterY_ + 0.2f, T + 0.2f);
            const float ang = (side == 0 ? 0.f : PI) + r.range(-1.f, 1.f);
            add(vec2{sx, y}, ang, r.range(0.3f, 1.1f), r.range(0.14f, 0.22f), r.range(0.15f, 0.55f), 0.75f, ci);
        }
    }
    // without margins (narrow screens), the water drips beside the well
    if (dripSrc_.empty()) {
        dripSrc_.push_back(vec2{viewC_.x - 5.6f, T});
        dripSrc_.push_back(vec2{viewC_.x + 5.6f, T});
    }
    // far ones first
    std::stable_sort(crys_.begin(), crys_.end(), [](const Crys& a, const Crys& b) { return a.depth > b.depth; });
    // shuffle the drip sources' order (alternate sides)
    for (Crys& c : crys_) c.col = c.target = currentColor();
}

// =====================================================================================================
// Song, level, minerals
// =====================================================================================================

vec3 CrystalUniverse::mineralColor(int m, int tone) const {
    const VariantDef& V = VARIANTS[variant_];
    return lin(V.min[std::clamp(m, 0, 3)][((tone % 3) + 3) % 3]);
}

vec3 CrystalUniverse::currentColor() const { return curSet_ ? ugl::toLinear(cur_) : mineralColor(mineral_, tone_[mineral_]); }

void CrystalUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 1;      // AMETHYST: dark minor songs
        else if (t.hasSong && t.bassWeight > 0.64f) v = 3;            // EMBER: heavy bass
        else if (t.hasSong && t.airWeight > 0.62f) v = 2;             // ICE: airy songs
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xC1ull);
    for (int i = 0; i < 4; i++) {
        tone_[i] = prevTone_[i] = 0;
        used_[i] = false;
    }
    used_[std::clamp(level_, 0, 2)] = true;
    if (old != seed_) placeCrystals();
    if (seconds <= 0.1f) {
        curSet_ = false;
        for (Crys& c : crys_) c.col = c.target = currentColor();
    }
}

void CrystalUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropOn_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        if (level != level_) enterMineral(level);
        used_[level] = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    // rising into a drop: the drop's stone comes now (the hit follows on the downbeat)
    if (rising && level == 2 && impact >= 0.9f && !dropOn_) {
        dropOn_ = true;
        dropBeat_ = beat_ + 0.45 * bps_;
        dropStart_ = time_ + 0.45f;
        enterMineral(3);
    }
    if (level != level_) enterMineral(level);
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: its stone (if it is not up yet) and the cave lit in it, eased in.
void CrystalUniverse::drop(float strength) {
    if (!dropOn_ || std::fabs(beat_ - dropBeat_) > 2.0) {
        dropOn_ = true;
        dropBeat_ = beat_;
        dropStart_ = time_;
        enterMineral(3);
    }
    dropT_ = std::max(dropT_, 0.6f + 0.4f * strength);
}

// A mineral coming back later in the song (a second verse, a second drop) takes its next tone, so that the two
// strata stay apart.
void CrystalUniverse::enterMineral(int m) {
    m = std::clamp(m, 0, 3);
    if (used_[m]) tone_[m]++;
    used_[m] = true;
}

// A new section of the same level: the mineral moves on to its next tone.
void CrystalUniverse::refresh(float strength) {
    (void)strength;
    const int m = mineral_;
    for (int i = 0; i < 4; i++) prevTone_[i] = tone_[i];
    tone_[m]++;
    refreshAt_ = time_;
}

// A new game level is not a new section of the song: take back the refresh that came with it (same frame).
void CrystalUniverse::levelUp() {
    if (refreshAt_ == time_)
        for (int i = 0; i < 4; i++) tone_[i] = prevTone_[i];
    for (Crys& c : crys_) c.glint = std::max(c.glint, 0.35f);
}

vec2 CrystalUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints CrystalUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = lin(V.rock) * 1.4f + lin(V.rockLit) * 0.15f;
    h.light = false;
    h.ink = currentColor();
    h.accent = currentColor();
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the stones in step with the board. Cells that appear behind our back (a new board, a stack inherited from
// another universe) are cut in strata of the cave's minerals from the bottom up, grouped into stones of at most
// four cells of one type.
void CrystalUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; glint_[y][x] = 0.f; }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            const int band = (Game::H - 1 - y) / 3;
            const int m = band % 3, tone = band / 3;
            const vec3 col = mineralColor(m, tone);
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            while (sp > 0 && count < 4) {
                sp--;
                const int cx = stack[sp][0], cy = stack[sp][1];
                st_[cy][cx] = Stone{col, m, tone, (float)cx, (float)cy, seedOf(id)};
                count++;
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& dd : d) {
                    const int nx = cx + dd[0], ny = cy + dd[1];
                    if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || ids_[ny][nx]) continue;
                    if (g.cell(nx, ny).type != g.cell(x, y).type || sp >= 8 || count + sp >= 4) continue;
                    ids_[ny][nx] = id;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void CrystalUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the stone keeps the mineral of the moment, as the falling piece showed it
        const int id = nextId_++;
        const vec3 col = currentColor();
        float sx = 0.f, lowY = -99.f;
        int n = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            st_[c.y][c.x] = Stone{col, mineral_, tone_[mineral_], (float)c.x, (float)c.y, seedOf(id)};
            glint_[c.y][c.x] = 0.45f;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            sx += p.x;
            n++;
            lowY = std::max(lowY, (float)c.y);
        }
        // one small star where it touched down
        if (n > 0) {
            const vec3 p = layout::cellPos(0.f, lowY);
            sparkles_.push_back({vec2{sx / n, p.y - 0.42f}, 0.f, 0.45f, 0.55f, lerp(col, vec3(1.f), 0.5f)});
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a few chips knocked off at the contact
        if (!game.hasPiece()) break;
        int cells[4][2];
        game.pieceCells(game.piece(), cells);
        int low = -1;
        for (auto& c : cells) low = std::max(low, c[1]);
        const vec3 col = typeTint(currentColor(), game.piece().type);
        for (auto& c : cells) {
            if (c[1] != low) continue;
            const vec3 p = layout::cellPos((float)c[0], (float)low);
            for (int k = 0; k < 2; k++) {
                Shard s;
                s.p = vec2{p.x + rng_.range(-0.4f, 0.4f), p.y - 0.45f};
                s.v = vec2{rng_.range(-3.f, 3.f), rng_.range(0.8f, 2.4f)};
                s.rot = rng_.range(0.f, TAU);
                s.spin = rng_.range(-14.f, 14.f);
                s.size = rng_.range(0.05f, 0.09f);
                s.t = 0.f;
                s.life = 0.9f;
                s.landY = waterY_ - rng_.range(0.2f, 1.4f);
                s.col = col;
                s.seed = rng_.uniform();
                s.landed = false;
                shards_.push_back(s);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the row shatters: shards tumble down into the pool
        const int per = ev.count >= 4 ? 6 : 5;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const vec3 col = ids_[c.y][c.x] ? typeTint(st_[c.y][c.x].col, c.type) : typeTint(currentColor(), c.type);
            shatter(p.x, p.y, col, per, ev.count >= 4 ? 1.5f : 0.f);
        }
        // compact the stones like the game did
        {
            int nb[Game::H][Game::W] = {};
            float ng[Game::H][Game::W] = {};
            Stone ns[Game::H][Game::W];
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(ng[dst], glint_[y], sizeof(glint_[y]));
                for (int x = 0; x < Game::W; x++) ns[dst][x] = st_[y][x];
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(glint_, ng, sizeof(glint_));
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) st_[y][x] = ns[y][x];
        }
        // a quad: a ring of light runs through the cave's crystals
        if (ev.count >= 4) ring_ = 0.f;
        break;
    }
    case GameEvent::TopOut: {
        // the game empties its board: the stones stay here, their light goes out from the top, then they crumble
        overT_ = 0.f;
        dead_.clear();
        auto inEv = [&](int x, int y) {
            for (auto& c : ev.cells)
                if (c.x == x && c.y == y) return true;
            return false;
        };
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const int id = ids_[c.y][c.x];
            const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
            int m = 0;
            for (int i = 0; i < 8; i++) {
                const int nx = c.x + dx[i], ny = c.y + dy[i];
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H || !inEv(nx, ny)) continue;
                if (id ? ids_[ny][nx] == id : false) m |= 1 << i;
            }
            const Stone& st = st_[c.y][c.x];
            const vec3 col = id ? st.col : currentColor();
            dead_.push_back({c.x, c.y, c.type, m, col, id ? st.px : (float)c.x, id ? st.py : (float)c.y, id ? st.seed : seedOf(nextId_), false});
        }
        break;
    }
    default: break;
    }
}

// A cell breaks into n shards that tumble down into the pool (`lift` throws them a little higher).
void CrystalUniverse::shatter(float x, float y, const vec3& col, int n, float lift) {
    for (int k = 0; k < n; k++) {
        Shard s;
        const vec2 o{rng_.range(-0.32f, 0.32f), rng_.range(-0.32f, 0.32f)};
        s.p = vec2{x + o.x, y + o.y};
        s.v = vec2{o.x * 4.f + rng_.range(-1.6f, 1.6f), rng_.range(0.8f, 4.f) + o.y * 2.f + lift};
        s.rot = rng_.range(0.f, TAU);
        s.spin = rng_.range(-12.f, 12.f);
        s.size = rng_.range(0.11f, 0.22f);
        s.t = 0.f;
        s.life = 2.2f;
        s.landY = waterY_ - rng_.range(0.15f, 1.7f);
        s.col = col;
        s.seed = rng_.uniform();
        s.landed = false;
        shards_.push_back(s);
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void CrystalUniverse::buildBoard(const Game& g) {
    const vec3 curLin = currentColor();
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto gem = [&](float x, float y, float half, int mask, int type, const vec3& col, float alpha, float glint, float dull,
                   float px, float py, int kind, float seed) {
        cells_.push_back({{x, y, half, (float)mask}, {col.x, col.y, col.z, alpha}, {(float)type, glint, (float)kind, dull}, {px, py, seed, 0.f}});
    };
    // The ghost: a fine line of the mineral's light.
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto inGhost = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 gcCol = typeTint(curLin, p.type) * 1.2f;
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                gem(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, gcCol, 0.5f, 0.f, 0.f, 0.f, 0.f, 1, 0.f);
            }
        }
    }
    // The stones.
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return g.cell(nx, ny).type == c.type;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            const Stone& s = st_[y][x];
            const float dull = 0.f;
            gem(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, typeTint(s.col, c.type), 1.f, glint_[y][x], dull, s.px, -s.py, 0, s.seed);
        }
    // Game over: the stones' light goes out row by row from the top, then each row crumbles into the pool.
    for (Dead& d : dead_) {
        if (d.gone) continue;
        const float rowT = overT_ - 0.1f * (d.y - Game::HIDDEN);
        const float dull = smoothstepf(0.05f, 0.6f, rowT);
        const vec3 col = typeTint(d.col, d.type);
        if (rowT > 1.3f) {
            const vec3 p = layout::cellPos((float)d.x, (float)d.y);
            shatter(p.x, p.y, lerp(col, vec3(luminance(col)) * 0.6f, 0.8f), 3, 1.f);
            d.gone = true;
            continue;
        }
        const vec3 p = layout::cellPos((float)d.x, (float)d.y);
        gem(p.x, p.y, 0.5f, d.mask, d.type, col, 1.f, 0.f, dull, d.px, -d.py, 0, d.seed);
    }
    // The falling piece: cut from the mineral of the moment (exactly as it will lock).
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        const vec3 col = typeTint(curLin, p.type);
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            gem(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, col, 1.f, 0.f, 0.f, (float)c[0], -(float)c[1], 0, seedOf(nextId_));
        }
    }
    // Previews.
    auto preview = [&](int type, vec2 center, float sc, float alpha, float seed) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const vec3 col = typeTint(curLin, type);
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            gem(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, col, alpha, 0.f, 0.f, (float)c[0], -(float)c[1], 0, seed);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f, seedOf(nextId_ + 7));
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, seedOf(nextId_ + 1 + i));
    }
}

void CrystalUniverse::drawGlows() {
    if (glows_.empty()) return;
    glUseProgram(progGlow_);
    setMat(progGlow_, "uVP", vp_);
    glBindVertexArray(glowVao_);
    glBindBuffer(GL_ARRAY_BUFFER, glowVbo_);
    glBufferData(GL_ARRAY_BUFFER, glows_.size() * sizeof(GlowInst), glows_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)glows_.size());
    glows_.clear();
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void CrystalUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The HUD's panels, on a dark halo of the cave (the lettering: hudLine).
void CrystalUniverse::drawHud(const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const vec3 halo = hex(V.rock) * 0.6f;
    vec3 accent = ugl::toSrgb(currentColor());
    accent = lerp(accent, vec3(1.f), 0.22f);

    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
}

// The lettering, by the host (text.hpp): Marcellus for the names, labels and numbers, Cormorant Garamond Italic for
// the song, the section and the celebrations; pale crystal light on a soft halo of the cave's dark, the accent the
// mineral of the moment.
void CrystalUniverse::hudLine(const HudText& h, text::Line& out) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink = hex(V.ink), halo = hex(V.rock) * 0.6f;
    const vec3 accent = lerp(ugl::toSrgb(currentColor()), vec3(1.f), 0.22f);
    text::Font* reg = text::font(FONT_REG);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(reg, text::upper(h.text), base * 0.82f, base * 0.24f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.85f, base * 0.06f); break;
    case HUD_SECTION: {
        // the section, then the mineral of the moment, quieter
        const float a = saturate(0.25f + h.alpha * 1.7f);
        put(it, h.text, base * 1.05f, base * 0.06f, accent, a, -base * 0.1f);
        put(it, std::string("  ·  ") + V.minName[std::clamp(mineral_, 0, 3)], base * 1.05f, base * 0.06f, ink, a * 0.7f, -base * 0.1f);
        break;
    }
    case HUD_TITLE: put(it, h.text, base * 1.14f, 0.f, ink, h.alpha * 1.3f, -base * 0.12f); break;
    case HUD_INFO: put(reg, h.text, base * 0.74f, base * 0.1f, ink, saturate(alpha * 1.2f) * 0.66f); break;
    case HUD_LABEL: put(reg, text::upper(h.text), base * 0.8f, base * 0.3f, ink, saturate(alpha * 1.2f) * 0.68f); break;
    case HUD_VALUE: put(reg, h.text, base * 1.1f, base * 0.04f, ink, alpha, -base * 0.08f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.05f, base * 0.04f, accent, alpha); break;
    case HUD_KEY: put(reg, h.text, base * 0.9f, 0.f, ink, alpha); break;
    case HUD_TOAST: put(it, h.text, base * 1.2f, base * 0.12f, ink, alpha, -base * 0.15f); break;
    case HUD_TOAST_SUB: put(it, h.text, base * 0.95f, base * 0.04f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(it, h.text, base * 1.1f, base * 0.06f, ink, alpha);
        else put(reg, h.text, base * 0.82f, base * 0.06f, h.accent ? accent : ink, alpha);
        break;
    default: put(reg, h.text, base * 0.8f, base * 0.02f, h.accent ? accent : ink, alpha); break;
    }
    out.look.halo = halo;
    out.look.haloA = 0.45f;
    out.look.haloW = 0.07f;
    out.look.haloPow = 1.5f;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void CrystalUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    bare_ = f.bare;
    if (!bare_) syncIds(game); // the world alone: the stones kept as they were, for the whole frame
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) {
            overT_ = -1.f;
            dead_.clear();
        }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    hasSong_ = music.hasSong;
    if (music.hasSong) {
        beat_ = music.beat;
        bps_ = std::max(0.5f, music.bpm / 60.f);
    } else {
        beat_ += wdt * 2.0;
        bps_ = 2.f;
    }

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);

    // ---- The mineral of the moment (the drop's for its first bars), and its color easing there.
    if (dropOn_) {
        const bool over = hasSong_ ? (beat_ - dropBeat_ > DROP_BEATS) : (time_ - dropStart_ > DROP_BEATS * 0.5f);
        if (over) dropOn_ = false;
    }
    mineral_ = dropOn_ ? 3 : level_;
    {
        const vec3 target = ugl::toSrgb(mineralColor(mineral_, tone_[mineral_]));
        if (!curSet_) {
            cur_ = target;
            curSet_ = true;
        } else {
            const float k = 1.f - std::exp(-1.7f * dt);
            cur_ = lerp(cur_, target, k);
        }
    }
    const vec3 curLin = currentColor();

    // ---- The drop's illumination: two eased stages, so it swells in over a beat or two and settles slowly.
    {
        float target = 0.f;
        if (dropOn_ && dropT_ > 0.f) {
            const float since = hasSong_ ? (float)((beat_ - dropBeat_) / bps_) : time_ - dropStart_;
            target = dropT_ * (since < 4.f ? 1.f : lerpf(1.f, 0.5f, smoothstepf(4.f, 10.f, since)));
        } else dropT_ = 0.f;
        dropA_ = approach(dropA_, target, 1.7f, dt);
        dropEnv_ = approach(dropEnv_, dropA_, 1.7f, dt);
    }
    const float glow = (0.5f + 0.28f * wm + 0.32f * wp) * (1.f - 0.45f * slow_) + 0.35f * dropEnv_;

    // ---- The stack's minerals: what the cave's crystals echo.
    float bins[4][3] = {};
    float total = 0.f;
    vec3 sum{0.f};
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] && (bare_ || game.cell(x, y).type >= 0)) {
                const Stone& s = st_[y][x];
                bins[std::clamp(s.mineral, 0, 3)][((s.tone % 3) + 3) % 3] += 1.f;
                sum += s.col;
                total += 1.f;
            }
    {
        const float wcur = std::max(4.f, total * 0.25f);
        bins[mineral_][((tone_[mineral_] % 3) + 3) % 3] += wcur;
        sum += curLin * wcur;
        total += wcur;
        stackAvg_ = lerp(stackAvg_, sum / total, 1.f - std::exp(-0.6f * dt));
    }
    for (Crys& c : crys_) {
        float acc = 0.f, pick = c.u * total;
        int bm = mineral_, bt = tone_[mineral_];
        for (int m = 0; m < 4 && acc <= pick; m++)
            for (int t = 0; t < 3; t++) {
                acc += bins[m][t];
                if (acc > pick) { bm = m; bt = t; break; }
            }
        c.target = mineralColor(bm, bt);
        // the drop lights them in its stone
        c.target = lerp(c.target, curLin, 0.55f * dropEnv_);
        const float k = 1.f - std::exp(-0.45f * dt);
        c.col = lerp(c.col, c.target, k);
        c.glint = approach(c.glint, 0.f, 3.f, dt);
    }

    // ---- The highs: crystals glint (small stars on their points), a gem's crown flashes now and then.
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f && !crys_.empty()) {
        float highs = 0.f;
        for (int b = 11; b < NUM_BANDS; b++) highs += music.bandsFast[b];
        highs /= (float)(NUM_BANDS - 11);
        const float rate = (music.hasSong ? 7.f * std::pow(saturate(highs * 2.2f - 0.15f), 1.5f) : 0.6f) * (0.45f + 0.3f * wm + 0.4f * wp) *
                           (1.f - 0.7f * slow_);
        if (rng_.uniform() < rate * dt) {
            Crys& c = crys_[rng_.irange(0, (int)crys_.size() - 1)];
            if (c.depth < 0.5f) {
                c.glint = std::max(c.glint, 0.55f);
                const vec2 dir{std::cos(c.angle), std::sin(c.angle)};
                const vec2 tp{c.base.x + dir.x * c.len * 0.94f, c.base.y + dir.y * c.len * 0.94f};
                sparkles_.push_back({tp, 0.f, 0.5f, std::min(0.75f, 0.3f + c.len * 0.08f), lerp(c.col, vec3(1.f), 0.55f)});
            }
        }
        if (rng_.uniform() < rate * 0.25f * dt) {
            const int x = rng_.irange(0, Game::W - 1), y = rng_.irange(Game::HIDDEN, Game::H - 1);
            // the whole stone flashes (never one cell of it)
            if (const int id = ids_[y][x])
                for (int yy = 0; yy < Game::H; yy++)
                    for (int xx = 0; xx < Game::W; xx++)
                        if (ids_[yy][xx] == id) glint_[yy][xx] = std::max(glint_[yy][xx], 0.3f);
        }
        // the kicks of a peak: one cluster glows
        if (wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f && !clusters_.empty()) {
            const int ci = rng_.irange(0, (int)clusters_.size() - 1);
            for (Crys& c : crys_)
                if (c.cluster == ci) c.glint = std::max(c.glint, 0.3f * kick);
        }
    }
    kickPrev_ = kick;
    // a quad: a ring of light running outward through the crystals
    if (ring_ >= 0.f) {
        ring_ += dt;
        const float rad = ring_ * 16.f;
        for (Crys& c : crys_) {
            const vec2 dir{std::cos(c.angle), std::sin(c.angle)};
            const float d = std::sqrt(std::pow(c.base.x + dir.x * c.len * 0.5f, 2.f) + std::pow(c.base.y + dir.y * c.len * 0.5f, 2.f));
            const float k = std::exp(-std::pow((d - rad) / 1.6f, 2.f));
            c.glint = std::max(c.glint, 0.75f * k);
        }
        if (ring_ > 2.f) ring_ = -1.f;
    }
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 3.5f, dt);

    // ---- The water: a drop every four beats in a calm part, every two from mid on (two at a peak), each landing
    // on the beat; their ripples.
    {
        const double period = lv_ < 0.5f ? 4.0 : 2.0;
        if (nextDripBeat_ < 0.0 || nextDripBeat_ < beat_ - 8.0 || nextDripBeat_ > beat_ + 16.0)
            nextDripBeat_ = std::ceil((beat_ + 3.0 * bps_) / period) * period;
        const int ns = (int)dripSrc_.size();
        for (int guard = 0; guard < 4 && ns > 0; guard++) {
            const vec2 src = dripSrc_[dripTurn_ % ns];
            const float landY = waterY_ - 0.25f - 0.9f * (float)((dripTurn_ * 37) % 10) / 10.f;
            const float fall = std::sqrt(2.f * std::max(0.5f, src.y - landY) / G_WATER);
            if (beat_ < nextDripBeat_ - fall * bps_) break;
            if (overT_ < 0.f && !f.paused) {
                drips_.push_back({src.x, landY, (float)nextDripBeat_, dripTurn_ % ns});
                if (wp > 0.5f && ns > 1) {
                    const int o = (dripTurn_ + ns / 2 + 1) % ns;
                    const vec2 s2 = dripSrc_[o];
                    const float l2 = waterY_ - 0.4f - 0.8f * (float)((dripTurn_ * 53) % 10) / 10.f;
                    // the second lands with the first (its fall timed from its own height)
                    drips_.push_back({s2.x, l2, (float)nextDripBeat_, o});
                }
            }
            dripTurn_++;
            nextDripBeat_ = std::floor(nextDripBeat_ / period + 0.5) * period + period;
        }
    }
    for (Ripple& rp : ripples_) rp.t += wdt;
    ripples_.erase(std::remove_if(ripples_.begin(), ripples_.end(), [](const Ripple& r) { return r.t > 2.6f; }), ripples_.end());

    // ---- Shards (real time, even in slow motion).
    for (Shard& s : shards_) {
        s.t += dt;
        if (!s.landed) {
            s.v.y -= G_SHARD * dt;
            s.p.x += s.v.x * dt;
            s.p.y += s.v.y * dt;
            s.rot += s.spin * dt;
            if (s.p.y < s.landY) {
                s.landed = true;
                s.life = s.t + 0.3f;
                if (rng_.chance(0.12f)) ripples_.push_back({s.p.x, waterY_ - s.landY, 0.f, 0.12f});
                s.v = vec2{s.v.x * 0.1f, -0.9f};
            }
        } else {
            s.p.y += s.v.y * dt;
            s.v.y = approach(s.v.y, -0.25f, 3.f, dt);
            s.rot += s.spin * 0.2f * dt;
        }
    }
    shards_.erase(std::remove_if(shards_.begin(), shards_.end(), [](const Shard& s) { return s.t > s.life; }), shards_.end());
    if (ripples_.size() > 16) ripples_.erase(ripples_.begin(), ripples_.begin() + (ripples_.size() - 16));

    for (Sparkle& sp : sparkles_) sp.t += dt;
    sparkles_.erase(std::remove_if(sparkles_.begin(), sparkles_.end(), [](const Sparkle& s) { return s.t > s.life; }), sparkles_.end());

    // ---- The cave's light: the clusters' glow.
    for (Cluster& cl : clusters_) cl.col = vec3(0.f);
    {
        std::vector<float> cnt(clusters_.size(), 0.f);
        for (const Crys& c : crys_) {
            if (c.cluster < 0 || c.cluster >= (int)clusters_.size()) continue;
            clusters_[c.cluster].col += c.col * (1.f + 2.f * c.glint);
            cnt[c.cluster] += 1.f;
        }
        for (size_t i = 0; i < clusters_.size(); i++)
            if (cnt[i] > 0.f) clusters_[i].col = clusters_[i].col / cnt[i];
    }

    cells_.clear();
    if (!bare_) buildBoard(game);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float worldK = 1.f - bridgeK;
    const vec3 lightDir = normalize(vec3(-0.45f, 0.7f, 0.6f));
    const vec4 quietA{viewC_.x - viewH_.x - 1.f, viewC_.y + viewH_.y - 2.8f, viewC_.x - 10.f, viewC_.y + viewH_.y + 1.f};
    const vec4 quietB{viewC_.x - viewH_.x - 1.f, viewC_.y - viewH_.y - 1.f, viewC_.x - 10.f, viewC_.y - viewH_.y + 3.4f};
    auto common = [&](GLuint p) {
        set4f(p, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set1f(p, "uSeed", seedU);
    };
    auto hudRects = [&](GLuint p) {
        set4f(p, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
        set4f(p, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    };

    // ---- The rock.
    bgT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    common(progBg_);
    hudRects(progBg_);
    set3f(progBg_, "uRock", lin(V.rock));
    set3f(progBg_, "uRockLit", lin(V.rockLit));
    set3f(progBg_, "uBand", lin(V.band) * 0.5f);
    {
        // the section's light, tinted by the stack's minerals
        const float l = luminance(stackAvg_);
        const vec3 tint = l > 1e-4f ? stackAvg_ / (l * 3.f + 0.05f) : vec3(1.f);
        const vec3 amb = lerp(vec3(1.f), tint, 0.45f) * (0.85f + 0.5f * wm + 0.75f * wp) * (1.f - 0.35f * slow_) * worldK;
        set3f(progBg_, "uAmb", amb);
    }
    {
        const float l = luminance(curLin);
        set3f(progBg_, "uDropCol", curLin / (l * 2.f + 0.05f));
    }
    set1f(progBg_, "uDrop", dropEnv_ * 1.6f * worldK);
    {
        float L[8 * 4] = {};
        float C[8 * 3] = {};
        const int n = std::min(8, (int)clusters_.size());
        for (int i = 0; i < n; i++) {
            const Cluster& cl = clusters_[i];
            L[i * 4 + 0] = cl.c.x;
            L[i * 4 + 1] = cl.c.y;
            L[i * 4 + 2] = cl.r * 1.4f;
            L[i * 4 + 3] = glow * 1.5f * worldK;
            C[i * 3 + 0] = cl.col.x;
            C[i * 3 + 1] = cl.col.y;
            C[i * 3 + 2] = cl.col.z;
        }
        glUniform4fv(ugl::U(progBg_, "uL"), 8, L);
        glUniform3fv(ugl::U(progBg_, "uLC"), 8, C);
        set1i(progBg_, "uNL", n);
    }
    set1f(progBg_, "uWater", waterY_);
    set3f(progBg_, "uWellCol", curLin * (0.05f + 0.03f * wp) * worldK * (bare_ ? 0.f : 1.f));
    set1f(progBg_, "uBare", bare_ ? 1.f : 0.f);
    ugl::drawFullscreen();

    // ---- The crystals.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    crysInst_.clear();
    for (const Crys& c : crys_) {
        const float sh = 0.04f * std::sin(worldTime_ * 0.3f + c.phase); // a very slow breath of inner light
        crysInst_.push_back({{c.base.x, c.base.y, c.angle, c.len},
                             {c.width, c.tip, c.depth, c.alpha * worldK},
                             {c.col.x, c.col.y, c.col.z, c.glint + sh + 0.04f}});
    }
    if (!crysInst_.empty()) {
        glUseProgram(progCrys_);
        common(progCrys_);
        hudRects(progCrys_);
        set4f(progCrys_, "uQuietA", quietA.x, quietA.y, quietA.z, quietA.w);
        set4f(progCrys_, "uQuietB", quietB.x, quietB.y, quietB.z, quietB.w);
        setMat(progCrys_, "uVP", vp_);
        set3f(progCrys_, "uLight", lightDir);
        set3f(progCrys_, "uFog", lin(V.fog));
        set1f(progCrys_, "uGlow", glow);
        glBindVertexArray(crysVao_);
        glBindBuffer(GL_ARRAY_BUFFER, crysVbo_);
        glBufferData(GL_ARRAY_BUFFER, crysInst_.size() * sizeof(CrysInst), crysInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)crysInst_.size());
    }

    // ---- Light: glows at the crystals' points, the falling water (it shows in the pool too).
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
    for (const Crys& c : crys_) {
        if (c.depth > 0.45f || c.len < 1.2f) continue;
        const vec2 dir{std::cos(c.angle), std::sin(c.angle)};
        const vec2 p{c.base.x + dir.x * c.len * 0.72f, c.base.y + dir.y * c.len * 0.72f};
        const float rr = c.len * 0.55f;
        const float in = (0.05f * glow + 0.25f * c.glint) * worldK;
        glows_.push_back({{p.x, p.y, rr, rr}, {c.col.x, c.col.y, c.col.z, in}, {0.f, 0.f, 0.f, 0.f}});
    }
    {
        const vec3 water = lerp(curLin, vec3(1.f), 0.6f) * 0.9f;
        for (auto it = drips_.begin(); it != drips_.end();) {
            const vec2 src = dripSrc_.empty() ? vec2{it->x, viewC_.y + viewH_.y} : dripSrc_[std::min(it->src, (int)dripSrc_.size() - 1)];
            const float fall = std::sqrt(2.f * std::max(0.5f, src.y - it->landY) / G_WATER);
            const float tau = (float)((it->hitBeat - beat_) / bps_); // seconds before it lands
            if (tau <= 0.f) {
                ripples_.push_back({it->x, waterY_ - it->landY, 0.f, 1.f});
                it = drips_.erase(it);
                continue;
            }
            const float te = std::max(0.f, fall - tau);
            const float y = src.y - 0.5f * G_WATER * te * te;
            const float speed = G_WATER * te;
            const float ry = 0.08f + std::min(0.5f, speed * 0.02f);
            glows_.push_back({{it->x, y + ry * 0.75f, 0.06f, ry}, {water.x, water.y, water.z, 0.9f * worldK}, {2.f, 0.f, 0.f, 0.f}});
            ++it;
        }
        // the next drop gathering on its point
        if (!dripSrc_.empty()) {
            const vec2 src = dripSrc_[dripTurn_ % dripSrc_.size()];
            const float landY = waterY_ - 0.25f - 0.9f * (float)((dripTurn_ * 37) % 10) / 10.f;
            const float fall = std::sqrt(2.f * std::max(0.5f, src.y - landY) / G_WATER);
            const float toGo = (float)((nextDripBeat_ - fall * bps_ - beat_) / bps_);
            const float k = 1.f - smoothstepf(0.f, 1.6f, toGo);
            if (k > 0.01f)
                glows_.push_back({{src.x, src.y - 0.05f * k, 0.07f * k, 0.09f * k}, {water.x, water.y, water.z, 0.8f * k * worldK}, {0.f, 0.f, 0.f, 0.f}});
        }
    }
    drawGlows();

    // ---- The pool (the cave mirrored, the ripples).
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progPool_);
    common(progPool_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bgT_.tex);
    set1i(progPool_, "uBg", 0);
    set1f(progPool_, "uWater", waterY_);
    {
        float R[16 * 4] = {};
        const int n = std::min(16, (int)ripples_.size());
        for (int i = 0; i < n; i++) {
            R[i * 4 + 0] = ripples_[i].x;
            R[i * 4 + 1] = ripples_[i].depth;
            R[i * 4 + 2] = ripples_[i].t;
            R[i * 4 + 3] = ripples_[i].amp * worldK;
        }
        glUniform4fv(ugl::U(progPool_, "uRip"), 16, R);
        set1i(progPool_, "uNR", n);
    }
    set3f(progPool_, "uDeep", lin(V.deep));
    set3f(progPool_, "uGlintCol", lerp(stackAvg_, vec3(0.6f), 0.5f) * (0.5f + 0.5f * glow));
    set1f(progPool_, "uTime", worldTime_);
    ugl::drawFullscreen();

    // ---- Shards (behind the stack).
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    shardInst_.clear();
    for (const Shard& s : shards_) {
        if (bare_) break; // the stones' shards are the game's
        const float under = s.landed ? saturate((s.landY - s.p.y) / 0.35f + 0.3f) : 0.f;
        float a = 1.f;
        if (s.landed) a = (1.f - smoothstepf(s.life - 0.3f, s.life, s.t)) * (1.f - 0.6f * under);
        shardInst_.push_back({{s.p.x, s.p.y, s.size, s.rot}, {s.col.x, s.col.y, s.col.z, a * worldK},
                              {s.rot * 1.7f + s.seed * 9.f, under, s.seed, 0.f}});
    }
    if (!shardInst_.empty()) {
        glUseProgram(progShard_);
        setMat(progShard_, "uVP", vp_);
        set3f(progShard_, "uDeep", lin(V.deep));
        glBindVertexArray(shardVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shardVbo_);
        glBufferData(GL_ARRAY_BUFFER, shardInst_.size() * sizeof(ShardInst), shardInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shardInst_.size());
    }

    // ---- The gems.
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        common(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uLight", lightDir);
        set1f(progCell_, "uSpark", 0.3f * wp + 0.3f * dropEnv_);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }

    // ---- Sparkles.
    glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
    for (const Sparkle& sp : sparkles_) {
        const float k = std::sin(PI * saturate(sp.t / sp.life));
        const float rr = sp.size * (0.6f + 0.4f * k);
        glows_.push_back({{sp.p.x, sp.p.y, rr, rr}, {sp.col.x, sp.col.y, sp.col.z, 0.9f * k * worldK}, {1.f, 0.3f, 0.f, 0.f}});
    }
    drawGlows();

    // ---- Bloom.
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    for (int i = 0; i < 2; i++) {
        const float st = 1.f + i;
        bloomB_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
        set2f(progBlur_, "uDir", st / bloomA_.w, 0.f);
        ugl::drawFullscreen();
        bloomA_.bind();
        glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
        set2f(progBlur_, "uDir", 0.f, st / bloomA_.h);
        ugl::drawFullscreen();
    }

    // ---- The frame.
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
    set1f(progFinal_, "uBloomK", (0.16f + 0.08f * wp + 0.2f * dropEnv_) * worldK);
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_crystal() {
    static const UniverseEntry e{
        "CRYSTAL CAVE", "gemstone pieces in a geode cave; each phase of the song its own mineral, frozen into the stack as strata",
        [] { return std::unique_ptr<Universe>(new CrystalUniverse()); },
        // Fits most songs; airy, dynamic ones a little more (the glints, the drop's light).
        [](const SongLook& l) { return 0.8f + 0.15f * l.airWeight + 0.15f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

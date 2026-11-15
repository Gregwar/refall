// NEON: luminous watercolor shapes over a painted night skyline.
//
// One world: a near-black night with faint stars and dust, and low along the bottom of the screen a city skyline
// painted in dark, translucent watercolor washes, the buildings overlapping and bleeding into each other, their tops
// fading into the night. Everything takes its color from one spectrum swept across the screen from left to right (the
// variant's five stops, NOCTURNE's teal, blue-violet, magenta, orange and gold), the city in deep, dark tones of it.
//
// The pieces are chunky, rounded shapes, one per piece (its cells merged, round outer corners and filleted inner
// ones, a thin dark gap from the others), each filled with a glowing watercolor wash: cloudy wet-in-wet blotches,
// blooms with ragged darker fringes, granulation, the pigment pooled into a darker dried rim, luminous where it is
// thin. A piece's color is the spectrum at its place in the well, so the stack reads as one sweep across it; each
// piece has its own wash (fixed for good when it locks: nothing swims) and a slight nuance of its own, so neighbours
// of a similar color still part. The song's sections move the spectrum (it slides sideways and its hue turns a little),
// and a piece keeps the colors it locked in: the stack keeps strata of the song's sections. The falling piece is exactly a locked one at
// its place, the ghost a thin line of its outline, the previews washes in the colors of the well's middle.
//
// A cleared row's pigment lifts off (the row is empty in a quarter of a second) and drifts up, behind the stack, as
// soft washes of color that float out to the skyline and paint a building in, from its foot up: the city grows with
// the lines, more colorful over the song, each building in the hue of the section it was painted in; once every slot
// is painted, new lines re-tint the oldest. A quad paints a whole block of four at once. A new song starts back
// from a dark city.
//
// The song: calm parts are a dark, quiet city with a few lit windows; mid lights windows on the beat grid (each
// window eased in and out, never flashing) and the stars twinkle slowly; a peak brings a richer glow and colored mist
// rising behind the skyline, the kicks lighting a few windows. The drop, eased in over a beat or two, washes the
// whole skyline in the luminous spectrum, then settles back over the following bars. Slow motion is deep night,
// desaturated; game over lets the stack's pigment run down in drips and fade.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/neon/neon.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/neon/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Marcellus (capitals, numbers) and Cormorant Garamond Italic (the song, the celebrations), SIL Open Font License,
// see OFL-Marcellus.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/neon/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/neon/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = neon_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// OKLab (from linear sRGB) and back.
vec3 toLab(const vec3& c) {
    const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s, 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
            0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}
vec3 fromLab(const vec3& L) {
    float l = L.x + 0.3963377774f * L.y + 0.2158037573f * L.z;
    float m = L.x - 0.1055613458f * L.y - 0.0638541728f * L.z;
    float s = L.x - 0.0894841775f * L.y - 1.2914855480f * L.z;
    l = l * l * l; m = m * m * m; s = s * s * s;
    return {std::max(0.f, 4.0767416621f * l - 3.3077085540f * m + 0.2309624716f * s),
            std::max(0.f, -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s),
            std::max(0.f, -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s)};
}

// A night (sRGB): its spectrum's five stops from left to right, the sky (top, low), how far the sections turn its hue.
struct VariantDef {
    const char* name;
    unsigned stops[5];
    unsigned skyTop, skyLow;
    float drift;
};
const VariantDef VARIANTS[] = {
    // the reference: teal, blue-violet, magenta, orange, gold
    {"NOCTURNE", {0x1cc8c2, 0x5b5ff2, 0xe03cc2, 0xff7838, 0xffc84a}, 0x030307, 0x0b0911, 1.0f},
    // greens to blues to violet
    {"AURORA", {0x44e08a, 0x26d0c6, 0x3a9cf2, 0x5a66f2, 0x9c5ae8}, 0x020508, 0x060d14, 1.0f},
    // crimson to gold
    {"EMBER", {0xd8304c, 0xf2503a, 0xff7a2e, 0xffa83a, 0xffd65a}, 0x050304, 0x110908, 0.9f},
    // navy to cyan to mint
    {"TIDE", {0x4058dc, 0x2a8af0, 0x1cc4e6, 0x34dcc0, 0x94f0b8}, 0x020409, 0x060c17, 1.0f},
    // rose violet to pink to peach
    {"ROSEWATER", {0x8e5ae8, 0xc65ad8, 0xf25aa8, 0xff8288, 0xffb68a}, 0x050308, 0x100912, 1.0f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The sections' phases (times the variant's drift): the spectrum slides sideways by half of it (its end colors
// held), its hue turns by 0.45 radian per unit. The song starts on the variant's own colors, each new section moves on.
const float SECTION_TURN[8] = {0.f, 0.36f, -0.3f, 0.2f, -0.4f, 0.3f, -0.16f, 0.4f};
constexpr float DROP_TURN = 0.44f;

constexpr float LIFT_T = 0.24f;      // a cleared row's pigment is gone in this long
constexpr float RUN_T = 2.4f;        // game over: the stack's pigment runs down and fades

float easeOut(float u) { u = saturate(u); return 1.f - (1.f - u) * (1.f - u); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// A VAO over the unit quad with `n` per-instance vec4 attributes (locations 1..n) of `stride` bytes.
void instVao(GLuint quad, GLuint& vao, GLuint& vbo, int n, size_t stride) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < n; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool NeonUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "neon sky");
    progBuild_ = ugl::program(shaders::BUILD_VS, withCommon(shaders::BUILD_FS).c_str(), "neon skyline");
    progVeil_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::VEIL_FS).c_str(), "neon well");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS, true).c_str(), "neon pieces");
    progWisp_ = ugl::program(shaders::WISP_VS, withCommon(shaders::WISP_FS).c_str(), "neon wisps");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "neon down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "neon blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "neon final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "neon shape");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, buildVao_, buildVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, pieceVao_, pieceVbo_, 4, sizeof(Inst));
    instVao(quadVbo_, wispVao_, wispVbo_, 3, sizeof(WispInst));
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
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void NeonUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    const bool rebuild = std::fabs(hw - viewH_.x) > 0.05f || std::fabs(hh - viewH_.y) > 0.05f;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (rebuild && !city_.empty()) {
        // a new frame size: the same city, its slots laid out again (what was painted stays painted)
        std::vector<Building> old = city_;
        buildCity(true);
        int k = 0;
        for (const Building& b : old)
            if (b.painted && b.layer == 1) k++;
        for (int i = 0; i < k; i++) {
            const int bi = paintBuilding(phase_, 0.f, 0);
            if (bi >= 0) city_[bi].paint = 1.f;
        }
    }
}

NeonUniverse::~NeonUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progSky_, progBuild_, progVeil_, progPiece_, progWisp_, progDown_, progBlur_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (GLuint v : {buildVao_, pieceVao_, wispVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, buildVbo_, pieceVbo_, wispVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The spectrum
// =====================================================================================================

vec3 NeonUniverse::spectrumAt(float u, float phase) const {
    u = saturate(u - phase * 0.5f) * 4.f;
    phase *= 0.45f;
    const int i = std::min(3, (int)std::floor(u));
    const float f = u - i;
    const vec3 a = lab_[i], b = lab_[i + 1];
    vec3 l = lerp(a, b, f);
    const float C = lerpf(std::sqrt(a.y * a.y + a.z * a.z), std::sqrt(b.y * b.y + b.z * b.z), f);
    const float h = std::atan2(l.z, l.y) + phase;
    l.y = std::cos(h) * C;
    l.z = std::sin(h) * C;
    return fromLab(l);
}

float NeonUniverse::phaseOf(int section) const {
    if (section < 0) return DROP_TURN * drift_;
    return SECTION_TURN[section % 8] * drift_;
}

void NeonUniverse::setSpectrum(GLuint prog) {
    float v[15];
    for (int i = 0; i < 5; i++) { v[i * 3] = lab_[i].x; v[i * 3 + 1] = lab_[i].y; v[i * 3 + 2] = lab_[i].z; }
    glUniform3fv(ugl::U(prog, "uLab"), 5, v);
}

// =====================================================================================================
// The city
// =====================================================================================================

// The skyline's slots, from the song's seed: a back row of taller, fainter buildings (dark ghosts at first), a
// front row of lower ones (empty at first). Near the well and under the HUD's columns they stay low.
void NeonUniverse::buildCity(bool instant) {
    if (!instant) {
        // the old city fades away
        for (Building& b : city_)
            if (b.fadeOut < 0.f) b.fadeOut = 0.f;
    } else city_.clear();
    std::vector<Building> fresh;
    Rng r(seed_ ^ 0xC17Full);
    const float bottom = viewC_.y - viewH_.y - 0.4f;
    const float W = viewH_.x + 1.2f;
    auto topLimit = [&](float x, int layer) {
        const float ax = std::fabs(x);
        if (ax < 5.6f) return layer == 0 ? 2.f : -99.f;
        if (x < -15.f) return layer == 0 ? 5.f : 1.5f;
        if (ax < 12.2f) {
            if (x < 0.f) return layer == 0 ? -5.6f : -7.3f;
            return layer == 0 ? -4.6f : -4.4f;
        }
        return layer == 0 ? 6.5f : 3.5f;
    };
    for (int layer = 0; layer < 2; layer++) {
        float x = -W;
        while (x < W) {
            const float hw = layer == 0 ? r.range(0.8f, 1.7f) : r.range(0.6f, 1.45f);
            const float cx = x + hw;
            const float lim = std::min(topLimit(cx - hw * 0.8f, layer), topLimit(cx + hw * 0.8f, layer));
            x = cx + hw + r.range(-0.55f, 0.3f);
            if (lim < bottom + 1.5f) continue;
            Building b;
            b.x = cx;
            b.base = bottom;
            b.hw = hw;
            const float room = lim - bottom;
            b.h = std::max(1.6f, room * r.range(layer == 0 ? 0.55f : 0.45f, 1.f));
            const float k = r.uniform();
            b.kind = k < 0.34f ? 0 : k < 0.52f ? 1 : k < 0.62f ? 2 : k < 0.72f ? 3 : k < 0.84f ? 4 : 5;
            if (b.kind == 2 && b.h < 4.f) b.kind = 0;
            b.layer = layer;
            b.seed = r.uniform();
            b.windows = r.chance(layer == 0 ? 0.5f : 0.75f) && std::fabs(cx) > 5.8f && b.h > 2.2f;
            if (layer == 0) {
                // a dark ghost of a city from the start
                b.painted = false;
                b.paint = instant ? 1.f : 0.f;
                b.paintDelay = instant ? 0.f : r.range(1.5f, 3.5f);
                b.paintDur = 2.5f;
                b.alpha = r.range(0.45f, 0.65f);
                b.tint = b.tintTarget = 0.f;
            } else {
                b.painted = false;
                b.paint = 0.f;
                b.alpha = r.range(0.55f, 0.75f);
                b.tint = b.tintTarget = 1.f;
            }
            fresh.push_back(b);
        }
    }
    // the order they are painted in: the front row first, the two sides in turn, near and far mixed; then the back
    // row (but not behind the well)
    std::vector<int> left, right, back;
    const int off = (int)city_.size();
    for (int i = 0; i < (int)fresh.size(); i++) {
        const Building& b = fresh[i];
        if (b.layer == 1) (b.x < 0.f ? left : right).push_back(off + i);
        else if (std::fabs(b.x) > 6.2f) back.push_back(off + i);
    }
    auto shuffle = [&](std::vector<int>& v) {
        for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[r.irange(0, i)]);
    };
    shuffle(left);
    shuffle(right);
    shuffle(back);
    paintOrder_.clear();
    const bool startLeft = r.chance(0.5f);
    for (size_t i = 0; i < std::max(left.size(), right.size()); i++) {
        if (startLeft) {
            if (i < left.size()) paintOrder_.push_back(left[i]);
            if (i < right.size()) paintOrder_.push_back(right[i]);
        } else {
            if (i < right.size()) paintOrder_.push_back(right[i]);
            if (i < left.size()) paintOrder_.push_back(left[i]);
        }
    }
    for (int i : back) paintOrder_.push_back(i);
    for (Building& b : fresh) city_.push_back(b);
}

// Paints the next slot in (or re-tints the oldest when every slot is in color), after `delay` seconds, in the
// phase's spectrum. `prefer` (-1 left, 1 right, 0 either) picks the side when it can. Returns the building.
int NeonUniverse::paintBuilding(float phase, float delay, int prefer) {
    int bi = -1;
    for (size_t k = 0; k < paintOrder_.size(); k++) {
        const int i = paintOrder_[k];
        if (prefer == 0 || (city_[i].x < 0.f) == (prefer < 0) || k + 1 == paintOrder_.size() || k >= 2) {
            bi = i;
            paintOrder_.erase(paintOrder_.begin() + (long)k);
            break;
        }
    }
    if (bi < 0) {
        // every slot is in color: the oldest painted one takes the new color
        int best = -1;
        for (int i = 0; i < (int)city_.size(); i++) {
            const Building& b = city_[i];
            if (!b.painted || b.fadeOut >= 0.f || std::fabs(b.x) < 6.2f) continue;
            if (best < 0 || b.order < city_[best].order) best = i;
        }
        if (best < 0) return -1;
        Building& b = city_[best];
        b.phaseA = b.mixAB >= 1.f ? b.phaseB : lerpf(b.phaseA, b.phaseB, b.mixAB);
        b.phaseB = phase;
        b.mixAB = 0.f;
        b.paintDelay = delay;
        b.alpha = std::min(0.9f, b.alpha + 0.04f);
        b.order = ++paintStamp_;
        return best;
    }
    Building& b = city_[bi];
    b.painted = true;
    b.order = ++paintStamp_;
    b.phaseA = b.phaseB = phase;
    b.mixAB = 1.f;
    b.paintDelay = delay;
    if (b.layer == 1) {
        b.paint = 0.f;
        b.paintDur = 1.7f;
    } else {
        b.tintTarget = 1.f;   // a ghost of the back row takes color
        b.alpha = std::min(0.85f, b.alpha + 0.12f);
    }
    return bi;
}

// A quad: a block of neighbouring slots painted at once.
int NeonUniverse::paintBlock(float phase, float delay, int side) {
    const int first = paintBuilding(phase, delay, side);
    if (first < 0) return -1;
    const float x0 = city_[first].x;
    for (int n = 0; n < 3; n++) {
        int best = -1;
        float bd = 1e9f;
        for (size_t k = 0; k < paintOrder_.size(); k++) {
            const Building& b = city_[paintOrder_[k]];
            const float d = std::fabs(b.x - x0) + (b.layer == 1 ? 0.f : 4.f);
            if ((b.x < 0.f) == (x0 < 0.f) && d < bd) { bd = d; best = (int)k; }
        }
        if (best < 0) {
            paintBuilding(phase, delay + 0.1f * n, side);
            continue;
        }
        // move it to the front of the order and paint it
        const int bi = paintOrder_[best];
        paintOrder_.erase(paintOrder_.begin() + best);
        paintOrder_.insert(paintOrder_.begin(), bi);
        paintBuilding(phase, delay + 0.12f * (n + 1), 0);
    }
    return first;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void NeonUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 3;                                  // tide
        else if (t.hasSong && t.bassWeight > 0.6f && t.dynamics > 0.5f) v = 2;                    // ember
        else if (t.hasSong && t.airWeight > 0.6f && t.brightness > 0.55f && !t.minor) v = 4;      // rosewater
        else if (t.hasSong && t.minor && t.brightness >= 0.45f) v = 1;                            // aurora
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    for (int i = 0; i < 5; i++) labTarget_[i] = toLab(lin(V.stops[i]));
    skyTarget_[0] = lin(V.skyTop);
    skyTarget_[1] = lin(V.skyLow);
    drift_ = V.drift;
    const bool instant = !labSet_ || seconds <= 0.1f;
    if (instant) {
        for (int i = 0; i < 5; i++) lab_[i] = labTarget_[i];
        sky_[0] = skyTarget_[0];
        sky_[1] = skyTarget_[1];
        labSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x4E304Eull);
    // a new song: back to the spectrum's own colors, and a dark city again
    section_ = 0;
    phaseTarget_ = 0.f;
    if (instant) phase_ = 0.f;
    buildCity(instant);
}

void NeonUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    (void)impact;
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (level != level_) {
        // a new section: the spectrum turns
        section_++;
        phaseTarget_ = phaseOf(section_);
    }
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
}

// The drop: its own turn of the spectrum, and the skyline washed in it (eased in: see render).
void NeonUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    phaseTarget_ = phaseOf(-1);
}

// A new section inside the same level: the spectrum moves on.
void NeonUniverse::refresh(float strength) {
    (void)strength;
    prevSection_ = section_;
    section_++;
    if (!dropMode_) phaseTarget_ = phaseOf(section_);
    refreshAt_ = time_;
}

// A new game level is not a new section of the song: take back the refresh that came with it (same frame).
void NeonUniverse::levelUp() {
    if (refreshAt_ == time_) {
        section_ = prevSection_;
        if (!dropMode_) phaseTarget_ = phaseOf(section_);
    }
}

vec2 NeonUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints NeonUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(sky_[0], sky_[1], 0.6f);
    h.light = false;
    h.ink = spectrumAt(0.5f, phase_);
    h.accent = spectrumAt(0.75f, phase_);
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// A piece's wash: its noise origin (the cell's place within the piece, so the wash moves with it and stays put once
// it locks), its seed, a slight nuance of its own (brightness, a shift along the spectrum), the phase it shows.
NeonUniverse::CellLook NeonUniverse::lookOf(int id, float relX, float relY, float phase) const {
    CellLook l;
    const uint64_t h = splitmix64((uint64_t)id * 0x9E3779B97F4A7C15ull ^ seed_);
    l.nx = hashf(h) * 180.f + relX;
    l.ny = hashf(h ^ 0x51) * 180.f - relY;
    l.seed = hashf(h ^ 0xA7);
    l.phase = phase;
    l.bright = 0.9f + 0.18f * hashf(h ^ 0x33);
    l.hue = (hashf(h ^ 0x77) - 0.5f) * 0.07f;
    return l;
}

// Keeps the ids in step with the board: cells that appear behind our back (a new board, a stack inherited from
// another universe) are grouped into pieces of up to four cells of one type, in the phase of the moment.
void NeonUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) ids_[y][x] = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            while (sp > 0 && count < 4) {
                sp--;
                const int cx = stack[sp][0], cy = stack[sp][1];
                look_[cy][cx] = lookOf(id, (float)(cx - x), (float)(cy - y), phase_);
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

void NeonUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the piece keeps the wash and the phase it fell with
        const int id = nextId_++;
        int minX = 99, minY = 99;
        for (auto& c : ev.cells) { minX = std::min(minX, c.x); minY = std::min(minY, c.y); }
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            look_[c.y][c.x] = lookOf(id, (float)(c.x - minX), (float)(c.y - minY), phase_);
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a small breath of pigment pushed out from under the piece, at the contact
        if (!game.hasPiece()) break;
        Piece gp = game.piece();
        gp.y = game.ghostY();
        int cells[4][2];
        game.pieceCells(gp, cells);
        int low = -1, minX = 99, maxX = -1;
        for (auto& c : cells) low = std::max(low, c[1]);
        for (auto& c : cells)
            if (c[1] == low) { minX = std::min(minX, c[0]); maxX = std::max(maxX, c[0]); }
        if (low < Game::HIDDEN) break;
        for (int s = -1; s <= 1; s += 2) {
            const vec3 p = layout::cellPos(s < 0 ? (float)minX - 0.35f : (float)maxX + 0.35f, (float)low + 0.42f);
            Wisp w;
            w.p0 = w.p = vec2{p.x, p.y};
            w.target = vec2{p.x + s * 0.9f, p.y + 0.25f};
            w.life = 0.7f;
            w.r0 = 0.22f;
            w.r1 = 0.55f;
            w.u = (p.x + 5.f) / 10.f;
            w.phase = phase_;
            w.seed = rng_.uniform();
            w.rot = rng_.range(0.f, TAU);
            w.stretch = 1.5f;
            w.peak = 0.3f;
            w.soft = true;
            wisps_.push_back(w);
        }
        break;
    }
    case GameEvent::Clear: {
        // each row's pigment lifts off, drifts out to the skyline and paints a building in (a quad: a block)
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool quadClear = ev.count >= 4;
        std::vector<int> targets;
        if (quadClear) {
            const int side = rng_.chance(0.5f) ? -1 : 1;
            const int first = paintBlock(phase_, 1.0f, side);
            if (first >= 0) {
                // the block's buildings: the first and the ones painted with it
                std::vector<std::pair<int, int>> recent;
                for (int i = 0; i < (int)city_.size(); i++)
                    if (city_[i].order > paintStamp_ - 4) recent.push_back({city_[i].order, i});
                for (auto& pr : recent) targets.push_back(pr.second);
            }
            quadGlow_ = 1.f;
        } else {
            for (size_t k = 0; k < rows.size(); k++) {
                const int bi = paintBuilding(phase_, 1.05f + 0.15f * k, (k % 2) ? 1 : -1);
                if (bi >= 0) targets.push_back(bi);
            }
        }
        for (size_t k = 0; k < rows.size(); k++) {
            const int y = rows[k];
            for (auto& c : ev.cells) {
                if (c.y != y || c.x < 0 || c.x >= Game::W) continue;
                const vec3 p = layout::cellPos((float)c.x, (float)c.y);
                const int id = ids_[c.y][c.x];
                int m = 0;
                if (id) {
                    if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                    if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                }
                const CellLook l = id ? look_[c.y][c.x] : lookOf(nextId_ + 1000 + c.x, (float)c.x, 0.f, phase_);
                lifts_.push_back({p.x, p.y, m, l, 0.f});
                Wisp w;
                w.p0 = w.p = vec2{p.x, p.y};
                int bi = targets.empty() ? -1 : targets[(quadClear ? (size_t)c.x * targets.size() / Game::W : k % targets.size())];
                if (bi >= 0) {
                    const Building& b = city_[bi];
                    w.target = vec2{b.x + rng_.range(-0.9f, 0.9f) * b.hw, b.base + std::min(b.h * rng_.range(0.2f, 0.6f), 6.f)};
                } else {
                    const float s = c.x < Game::W / 2 ? -1.f : 1.f;
                    w.target = vec2{s * rng_.range(8.f, 16.f), viewC_.y - viewH_.y + rng_.range(2.f, 6.f)};
                }
                w.delay = 0.012f * (float)(c.x % 5);
                w.life = rng_.range(1.25f, 1.55f);
                w.r0 = 0.5f;
                w.r1 = rng_.range(1.6f, 2.3f);
                w.u = (p.x + 5.f) / 10.f + l.hue;
                w.phase = l.phase;
                w.seed = rng_.uniform();
                w.rot = rng_.range(0.f, TAU);
                w.stretch = rng_.range(1.0f, 1.5f);
                w.peak = quadClear ? 0.22f : 0.3f;
                wisps_.push_back(w);
            }
        }
        // the ids and looks of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            int nb[Game::H][Game::W] = {};
            CellLook nl[Game::H][Game::W];
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { nb[dst][x] = ids_[y][x]; nl[dst][x] = look_[y][x]; }
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) look_[y][x] = nl[y][x];
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack's pigment runs down and fades, from the top
        overT_ = 0.f;
        runs_.clear();
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                for (int i = 0; i < 8; i++) {
                    const int nx = c.x + dx[i], ny = c.y + dy[i];
                    if (nx >= 0 && nx < Game::W && ny >= 0 && ny < Game::H && ids_[ny][nx] == id) m |= 1 << i;
                }
            }
            const CellLook l = id ? look_[c.y][c.x] : lookOf(5000 + c.y * 10 + c.x, 0.f, 0.f, phase_);
            runs_.push_back({p.x, p.y, m, l, 0.3f + 0.05f * (c.y - top) + 0.08f * hashf((uint64_t)(c.x * 31 + c.y))});
        }
        std::memset(ids_, 0, sizeof(ids_));
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void NeonUniverse::addCell(float x, float y, float half, int mask, const CellLook& l, float xoff, float alpha, int kind,
                           float prog) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)mask}, {l.nx, l.ny, l.seed, l.phase}, {xoff, l.bright, alpha, (float)kind},
                      {prog, l.hue, 0.f, 0.f}});
}

void NeonUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    // the falling piece's look: the id it will lock with, its place within the piece
    auto pieceLook = [&](int cells[4][2], int id, int cx, int cy, float phase) {
        int minX = 99, minY = 99;
        for (int i = 0; i < 4; i++) { minX = std::min(minX, cells[i][0]); minY = std::min(minY, cells[i][1]); }
        return lookOf(id, (float)(cx - minX), (float)(cy - minY), phase);
    };
    // the ghost: the piece's outline where it will land
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
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                addCell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), pieceLook(gc, nextId_, c[0], c[1], phase_), 0.f, 1.f, 1, 0.f);
            }
        }
    }
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const float off = g.rowOffset(y);
            auto same = [&](int nx, int ny) {
                if (nx < 0 || nx >= Game::W || ny < Game::HIDDEN || ny >= Game::H) return false;
                if (std::fabs(g.rowOffset(ny) - off) > 0.02f) return false;
                if (ids_[y][x] && ids_[ny][nx]) return ids_[ny][nx] == ids_[y][x];
                return false;
            };
            const vec3 p = layout::cellPos((float)x, y - off);
            addCell(p.x, p.y, 0.5f, maskOf(x, y, same), look_[y][x], 0.f, 1.f, 0, 0.f);
        }
    // the falling piece: exactly a locked one at its place
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
        for (auto& c : cells) {
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy); // (whole above the well too: it spawns there)
            addCell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), pieceLook(cells, nextId_, c[0], c[1], phase_), 0.f, 1.f, 0, 0.f);
        }
    }
    // the previews: in the colors of the well's middle, where they come in
    auto preview = [&](int type, vec2 center, float sc, float alpha, int id) {
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
        for (auto& c : cells) {
            const float ox = c[0] - (minx + maxx) * 0.5f, oy = c[1] - (miny + maxy) * 0.5f;
            const float px = center.x + ox * sc, py = center.y - oy * sc;
            addCell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), pieceLook(cells, id, c[0], c[1], phase_), ox - px, alpha, 0, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.45f : 1.f,
            90000 + g.holdType());
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, nextId_ + 1 + i);
    }
}

void NeonUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void NeonUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void NeonUniverse::drawHud(const std::vector<HudRect>& panels) {
    // the panels on a soft halo of the night
    const vec3 halo = ugl::toSrgb(sky_[1]) * 0.6f;
    vec3 accent = ugl::toSrgb(spectrumAt(0.62f, phase_));
    accent = lerp(accent, vec3(1.f), 0.25f);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
}


// The lettering, by the host (text.hpp): Marcellus in spaced capitals for the names, the numbers and the
// celebrations, Cormorant Garamond Italic for the song and the notices; pale grey ink like a caption, the accent
// drifting with the spectrum, on a soft halo of the night.
void NeonUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    const vec3 ink = vec3(0.80f, 0.79f, 0.82f), halo = ugl::toSrgb(sky_[1]) * 0.6f;
    const vec3 accent = lerp(ugl::toSrgb(spectrumAt(0.62f, phase_)), vec3(1.f), 0.25f);
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float haloA = 0.45f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(caps, text::upper(h.text), base * 0.78f, base * 0.34f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.8f, base * 0.06f); break;
    case HUD_SECTION: put(caps, text::upper(h.text), base * 0.86f, base * 0.36f, accent, 0.3f + h.alpha * 1.7f, -base * 0.04f); break;
    case HUD_TITLE: put(it, h.text, base * 1.16f, base * 0.02f, ink, h.alpha * 1.3f, -base * 0.12f); break;
    case HUD_INFO: put(caps, text::upper(h.text), base * 0.7f, base * 0.26f, ink, saturate(alpha * 1.2f) * 0.78f); break;
    case HUD_LABEL: put(caps, text::upper(h.text), base * 0.74f, base * 0.42f, ink, saturate(alpha * 1.2f) * 0.6f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.1f, base * 0.12f, ink, alpha * 0.95f, -base * 0.08f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.1f, base * 0.04f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.9f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(caps, text::upper(h.text), base * 1.1f, base * 0.4f, ink, alpha, -base * 0.12f); haloA = 0.6f; break;
    case HUD_TOAST_SUB: put(it, h.text, base * 1.0f, base * 0.04f, accent, alpha * 0.9f); haloA = 0.6f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, text::upper(h.text), base * 0.95f, base * 0.3f, ink, alpha);
        else put(it, h.text, base * 0.95f, base * 0.03f, h.accent ? accent : ink, alpha);
        haloA = 0.7f;
        break;
    default: put(caps, h.text, base * 0.8f, base * 0.08f, h.accent ? accent : ink, alpha); break;
    }
    // the letters a touch full (their edge a little out), the halo a smooth falloff just around them
    out.look.weight = 0.0125f;
    out.look.halo = halo;
    out.look.haloA = haloA;
    out.look.haloW = 0.075f;
    out.look.haloPow = 1.5f;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void NeonUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    if (!bare_) syncIds(game); // the world alone: the pieces' washes kept for the whole frame
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        for (int i = 0; i < 5; i++) lab_[i] = lerp(lab_[i], labTarget_[i], k);
        for (int i = 0; i < 2; i++) sky_[i] = lerp(sky_[i], skyTarget_[i], k);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) { overT_ = -1.f; runs_.clear(); }
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    if (music.hasSong) beat_ = (float)music.beat;
    else beat_ += wdt * 2.f;
    const float spb = 60.f / std::max(60.f, music.bpm);
    // the drop: eased in over about two beats after its downbeat, held through the first bars, then settling
    dropT_ += dt;
    float festT = 0.f;
    if (dropMode_) festT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.85f * smoothstepf(16.f * spb, 40.f * spb, dropT_));
    fest_ = approach(fest_, festT, festT > fest_ ? 1.6f : 0.3f, dt);
    swell_ = approach(swell_, 0.f, 0.25f, dt);
    quadGlow_ = approach(quadGlow_, 0.f, 0.35f, dt);
    kickEnv_ = std::max(kickEnv_ * std::exp(-dt * 5.f), kickIn_ * wp);
    kickIn_ = 0.f;
    // the falling piece's colors follow the section, eased
    phase_ += (phaseTarget_ - phase_) * (1.f - std::exp(-dt * 1.4f));

    // ---- The city.
    for (Building& b : city_) {
        if (b.fadeOut >= 0.f) { b.fadeOut += dt / 3.f; continue; }
        if (b.paintDelay > 0.f) { b.paintDelay -= dt; continue; }
        if (b.layer == 1 && b.painted && b.paint < 1.f) b.paint = std::min(1.f, b.paint + dt / b.paintDur);
        if (b.layer == 0 && b.paint < 1.f) b.paint = std::min(1.f, b.paint + dt / b.paintDur);
        b.tint = approach(b.tint, b.tintTarget, 0.6f, dt);
        if (b.mixAB < 1.f) b.mixAB = std::min(1.f, b.mixAB + dt / 1.8f);
    }
    // the old city gone (after a new song): its slots leave, the order of what is left to paint follows
    if (std::any_of(city_.begin(), city_.end(), [](const Building& b) { return b.fadeOut >= 1.f; })) {
        std::vector<int> remap(city_.size(), -1);
        std::vector<Building> kept;
        for (size_t i = 0; i < city_.size(); i++)
            if (city_[i].fadeOut < 1.f) { remap[i] = (int)kept.size(); kept.push_back(city_[i]); }
        city_.swap(kept);
        std::vector<int> order;
        for (int i : paintOrder_)
            if (i >= 0 && i < (int)remap.size() && remap[i] >= 0) order.push_back(remap[i]);
        paintOrder_.swap(order);
    }
    // ---- The pigment in the air (real time: what the play drives).
    for (Wisp& w : wisps_) {
        w.t += dt;
        const float s = saturate((w.t - w.delay) / w.life);
        const float e = 1.f - std::pow(1.f - s, 2.2f);
        // a curve up and out, then down onto the building
        const vec2 c1{w.p0.x + (w.target.x - w.p0.x) * 0.25f, w.p0.y + (w.soft ? 0.2f : 1.2f)};
        const vec2 c2{w.target.x, w.target.y + (w.soft ? 0.f : 1.6f)};
        const float a = 1.f - e;
        const float k0 = a * a * a, k1 = 3.f * a * a * e, k2 = 3.f * a * e * e, k3 = e * e * e;
        w.p = vec2{w.p0.x * k0 + c1.x * k1 + c2.x * k2 + w.target.x * k3, w.p0.y * k0 + c1.y * k1 + c2.y * k2 + w.target.y * k3};
    }
    wisps_.erase(std::remove_if(wisps_.begin(), wisps_.end(), [](const Wisp& w) { return w.t > w.delay + w.life; }), wisps_.end());
    for (Lift& l : lifts_) l.t += dt;
    lifts_.erase(std::remove_if(lifts_.begin(), lifts_.end(), [](const Lift& l) { return l.t > LIFT_T; }), lifts_.end());

    // ---- Instances.
    builds_.clear();
    cells_.clear();
    wispInst_.clear();
    for (int pass = 0; pass < 2; pass++)
        for (const Building& b : city_) {
            if (b.layer != pass) continue;
            const float fa = b.fadeOut >= 0.f ? 1.f - smoothstepf(0.f, 1.f, b.fadeOut) : 1.f;
            const float paint = b.layer == 1 ? (b.painted ? b.paint : 0.f) : b.paint;
            if (paint <= 0.f || fa <= 0.f) continue;
            builds_.push_back({{b.x, b.base, b.hw, b.h}, {(float)b.kind, b.seed, paint, b.alpha * fa},
                               {b.phaseA, b.phaseB, b.mixAB, b.tint}, {(float)b.layer, b.windows ? 1.f : 0.f, 0.f, 0.f}});
        }
    if (!bare_) { // the world alone: no stack, nothing lifting off or running down
        for (const Lift& l : lifts_) addCell(l.x, l.y, 0.5f, l.mask, l.l, 0.f, 1.f, 2, saturate(l.t / LIFT_T));
        if (overT_ >= 0.f)
            for (const Run& r : runs_) {
                const float t = saturate((overT_ - r.delay) / RUN_T);
                if (t < 1.f) addCell(r.x, r.y, 0.5f, r.mask, r.l, 0.f, 1.f, 3, t);
            }
        buildBoard(game);
    }
    for (const Wisp& w : wisps_) {
        const float tt = w.t - w.delay;
        if (tt <= 0.f) continue;
        const float s = saturate(tt / w.life);
        float a = smoothstepf(0.f, 0.12f, s) * (1.f - smoothstepf(0.55f, 1.f, s)) * w.peak;
        // softer while it is still over the well
        if (!w.soft) a *= lerpf(0.55f, 1.f, smoothstepf(5.f, 6.5f, std::fabs(w.p.x)));
        const float r = lerpf(w.r0, w.r1, easeOut(s));
        wispInst_.push_back({{w.p.x, w.p.y, r, w.rot + s * 0.8f}, {w.u, w.phase, a, w.stretch}, {w.seed, 0.f, 0.f, 0.f}});
    }

    // ---- Draw.
    const float aaW = 2.f * viewH_.y / (float)height_;
    const float skyline = viewC_.y - viewH_.y + 6.f;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progSky_);
    setSpectrum(progSky_);
    set4f(progSky_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set3f(progSky_, "uSkyTop", sky_[0]);
    set3f(progSky_, "uSkyLow", sky_[1]);
    set1f(progSky_, "uTime", worldTime_);
    set1f(progSky_, "uTwinkle", wm * (1.f - slow_));
    set1f(progSky_, "uMist", (0.5f * wp + 0.4f * fest_) * (1.f - 0.7f * slow_));
    set1f(progSky_, "uDrop", fest_ * (1.f - 0.6f * slow_));
    set1f(progSky_, "uPhi", phase_);
    set1f(progSky_, "uSeed", seedF_);
    set1f(progSky_, "uSkyline", skyline);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    // the skyline
    if (!builds_.empty()) {
        glUseProgram(progBuild_);
        setSpectrum(progBuild_);
        set4f(progBuild_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progBuild_, "uGhost", lerp(sky_[1], lin(0x2a1f36), 0.6f) * 0.9f);
        set1f(progBuild_, "uBeat", beat_);
        set1f(progBuild_, "uWinStatic", (0.035f + 0.02f * wm) * (1.f - 0.5f * slow_));
        set1f(progBuild_, "uWinVar", (0.07f * wm + 0.07f * wp + 0.05f * fest_) * (1.f - 0.6f * slow_));
        set1f(progBuild_, "uWinGlow", 0.2f + 0.08f * wp + 0.06f * fest_);
        set1f(progBuild_, "uDrop", saturate(fest_ * 1.1f + 0.35f * quadGlow_) * (1.f - 0.6f * slow_));
        set1f(progBuild_, "uRich", 0.5f * wp + 0.3f * swell_);
        set1f(progBuild_, "uPhi", phase_);
        set1f(progBuild_, "uKick", kickEnv_);
        set1f(progBuild_, "uAA", aaW * 1.5f);
        drawInst(progBuild_, buildVao_, buildVbo_, builds_.data(), builds_.size() * sizeof(Inst), builds_.size());
    }
    // the well
    if (!bare_) {
        glUseProgram(progVeil_);
        set4f(progVeil_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progVeil_, "uSkyLow", sky_[1]);
        set3f(progVeil_, "uLine", vec3(0.5f, 0.5f, 0.55f));
        set1f(progVeil_, "uAA", aaW);
        set1f(progVeil_, "uLineA", 0.16f);
        ugl::drawFullscreen();
    }
    // the pigment in the air, behind the stack
    if (!wispInst_.empty()) {
        glUseProgram(progWisp_);
        setSpectrum(progWisp_);
        drawInst(progWisp_, wispVao_, wispVbo_, wispInst_.data(), wispInst_.size() * sizeof(WispInst), wispInst_.size());
    }
    // the pieces: the stack writes the scene's alpha
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    setSpectrum(progPiece_);
    set1f(progPiece_, "uGlow", (1.f + 0.08f * wp + 0.06f * fest_) * (1.f - 0.25f * slow_));
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());

    // ---- The glow.
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uBlockCut", 0.75f);   // the blocks feed the glow only faintly: crisp, the city keeps its glow
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
    set1f(progFinal_, "uBloomK", (0.42f + 0.15f * wp + 0.2f * fest_) * (1.f - 0.4f * slow_));
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
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

const UniverseEntry& universe_neon() {
    static const UniverseEntry e{
        "NEON", "glowing watercolor shapes in a spectrum over a painted night skyline; each cleared row paints a building in",
        [] { return std::unique_ptr<Universe>(new NeonUniverse()); },
        // A night world: fits most songs, a little more the airy, dynamic ones.
        [](const SongLook& l) { return 0.8f + 0.15f * l.airWeight + 0.15f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

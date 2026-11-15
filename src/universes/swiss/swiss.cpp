// SWISS POSTER: a vintage travel poster in flat design, its landscape drifting in parallax behind the well.
//
// One world: the landscape of a mid-century travel poster, all flat shapes with crisp edges (no photographic
// shading, a faint print grain), in seven depths. A flat sky gradient with a sun and its pale halos and flat clouds
// in three bands; a far range of peaks, each face lit or in shade, snow above a jagged snow line with rock showing
// through in flat patches; a hero peak on one side (a snow horn, or a pale limestone tooth with cliff bands and
// grass on its lower slopes); two nearer ranges in receding tones (aerial perspective: the farther, the paler, with a
// mist at their feet in the misty scenes); a lake in some scenes, its long glints and the sun's path on the water; a
// meadow dotted with flowers, darker patches of grass and a winding path up to a village of chalets and houses
// (gable fronts lit, long sides in shade, red roof slabs, windows, a church with a clock and a spire) among cypresses
// and firs, each casting a flat shadow to the right; a foreground hill with big firs at the poster's corners. No
// lettering on the poster.
//
// Parallax: every depth is shifted by its own share of one pan (sky 3 %, far range 8 %, then 16, 26, 38, 60 and
// 100 % for the foreground): a slow, calm swing across the scene (80 s back and forth), a gentle sway on the bars
// (one way, then the other, over two bars; a little more at mid and peak, more in a drop) and a lean away from the
// falling piece (the scene turns slightly as it moves across the well). The clouds drift on their own, each band at
// its speed; the trees' tips sway in the wind.
//
// The well stands on a calm paper panel (the poster's own paper, nearly opaque) with two side rules and a baseline;
// the hold and next previews on small paper plates, the bonus gauge (the host's ring) on a paper disc. The pieces are
// flat, crisp color blocks taken from the scene's palette: one clean shape per piece (its cells merge, a narrow
// gutter around it), the falling piece exactly a locked one, the ghost a fine outline.
//
// Staging: calm is a hazier, softer light; mid and peak clear it, a peak warms it a little. The drop look holds only
// while the song plan's DROP section lasts (music.inDrop): the light warms, the sun swells and throws flat rays
// across the sky, the clouds speed up, the sway widens, the pieces' colors deepen, and flocks of birds cross the sky
// every two bars (a big flock rises from both sides on the drop's downbeat). A cleared row closes into a flat bar and
// slides out of the well sideways in 0.3 s, and a small flock of birds takes off from the meadow on one side and
// flies away outward (more birds for more lines); a hard drop leaves a thin rule at the contact. Slow motion stills
// the scene (the pan, the clouds and the birds slow down) and cools its light. Game over slides the stack out row by
// row from the top.
//
// Cost: one full-screen pass for the landscape (a few dozen elements tested per pixel), the board, plates and birds as
// instanced flat shapes.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/swiss/swiss.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/swiss/shaders.hpp"

namespace {

// Inter (Bold: the values; Regular: the information, the labels), SIL Open Font License, see OFL-Inter.txt: the
// HUD's own lettering when the host does not unify it (ZEN_HUD=universe).
const unsigned char FONT_BOLD[] = {
#include "universes/swiss/Inter-Bold.ttf.inc"
};
const unsigned char FONT_REG[] = {
#include "universes/swiss/Inter-Regular.ttf.inc"
};

namespace shaders = swiss_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A poster: its palette, its pieces, its scene.
struct VariantDef {
    const char* name;
    unsigned skyTop, skyBot, haze, cloud, sun;
    unsigned snow, snowSh, rock, rockSh;
    unsigned mid1, mid2, mid3;
    unsigned heroLit, heroSh, heroGrass;
    unsigned lake, lakeLit;
    unsigned meadow, meadowLit, meadowDk, fg, dot;
    unsigned path, wall, wallSh, roof, roofSh, timber, window;
    unsigned tree, treeLit, tree2, tree2Lit;
    unsigned paper, ink, accent;
    unsigned pieces[7];       // I O T S Z J L
    float farBase, snowLine, farHeight, round; // the far range
    int hero;                 // 0 none, 1 snow horn, 2 limestone
    float lakeY, mist;        // lake level (-99: none), mist at the ranges' feet
    int village;              // 0 a hut or two, 1 a village of houses and chalets, 2 a village of chalets
    int hasPath;
    float cloudFlat, cloudCover;
    float meadowY, fgY, midH;
    float sunX, sunY, sunR;   // the sun: x as a share of the half width on the hero's side, height, radius
    int trees;                // 0 cypresses and firs, 1 firs, 2 larches and firs
};

const VariantDef VARIANTS[] = {
    // Blue and white peaks over a green meadow, a village of red roofs, a church tower, cypresses (the reference).
    {"ALPINE VILLAGE",
     0x4f9fdc, 0xd8edf6, 0xbad9ee, 0xf7fbfe, 0xfff7dc,
     0xfafcfd, 0xa9cbe8, 0x3f84c8, 0x28609f,
     0x4a86c0, 0x336fa8, 0x285f8c,
     0xf3ead2, 0xa9b9c6, 0x5aa83c,
     0x3d8cc8, 0xd5ecf6,
     0x5ab43d, 0x86cc4a, 0x3d8c33, 0x47a23a, 0xf6f6e2,
     0xf3e9c8, 0xf6f2e6, 0xbcb6d2, 0xd8262c, 0x9b1c24, 0x5b2e24, 0x3b2c33,
     0x1f5848, 0x2f7a58, 0x2a6b3f, 0x3d8a4a,
     0xf5f2e9, 0x16233a, 0xd8262c,
     {0x2f6fb5, 0xe2a62a, 0xd8262c, 0x4ea83a, 0x1d3d6b, 0x2f8f86, 0x7b4630},
     4.0f, 5.5f, 6.5f, 0.0f, 1, -99.f, 0.15f, 1, 1, 0.0f, 0.42f, -4.4f, -9.0f, 5.0f, 0.62f, 9.8f, 0.85f, 0},
    // Teal and green: misty ridges, a lake, a pale limestone peak, firs, a yellow path, long cloud bands.
    {"GRAND SOM",
     0x3f9a92, 0xf2e7b4, 0xcae3c6, 0xdcefd6, 0xfff3c4,
     0xa8d4c3, 0x8cc2b4, 0x83bcab, 0x6eac9e,
     0x5aa596, 0x428e80, 0x2f7668,
     0xefdda2, 0xb9c197, 0x92c247,
     0x9fd0c2, 0xeef5e0,
     0x9cc43f, 0xc8d659, 0x6ea438, 0x5f9c3a, 0xeedb7e,
     0xe9c04a, 0xf0e8d0, 0xb7b89a, 0xc8552e, 0x8e3a22, 0x5a3a28, 0x3a3a30,
     0x2b6a4a, 0x3f8c58, 0x245c40, 0x357a50,
     0xf4f1df, 0x1d3a36, 0xe0662c,
     {0x2a8f86, 0xe8b53a, 0xe0662c, 0x6aa636, 0x2b5f4a, 0x3a78a8, 0x8a5a3a},
     2.4f, 99.f, 5.0f, 0.8f, 2, -1.6f, 0.55f, 0, 1, 1.0f, 0.62f, -4.2f, -8.9f, 4.6f, -0.62f, 9.0f, 1.1f, 1},
    // An indigo dusk over a lake: rose snow, violet ranges, the sun's path on the water, lit windows.
    {"LAKE AT DUSK",
     0x2c3a72, 0xf4b277, 0xd99a8c, 0xf2a58c, 0xffe2a8,
     0xf6cdbf, 0x9c88b8, 0x705f95, 0x584c82,
     0x5e4f88, 0x483f71, 0x37305b,
     0xf3ead2, 0xa9b9c6, 0x5aa83c,
     0x6d5b8e, 0xf7c48a,
     0x3c5c52, 0x557862, 0x2c4640, 0x263c3a, 0x86a088,
     0xc9a98a, 0xddc8c8, 0x8f7e9e, 0xa83e45, 0x6e2a3a, 0x3e2b36, 0xffcf74,
     0x1e3030, 0x2e4644, 0x1e3030, 0x2e4644,
     0xf6eee4, 0x2a2342, 0xe86a52,
     {0x3d4f96, 0xf0b44a, 0xe86a52, 0x2f8a86, 0x7a4a8a, 0xd987a0, 0x4a4070},
     2.6f, 7.6f, 6.0f, 0.3f, 0, -2.2f, 0.35f, 1, 0, 1.0f, 0.5f, -5.0f, -9.2f, 4.0f, 0.72f, 5.6f, 1.6f, 0},
    // Ochre meadows, orange larches, brown chalets under the first snow.
    {"AUTUMN VALLEY",
     0x7fb9d6, 0xf5e3bf, 0xe4d4bd, 0xfbf3e4, 0xfff1cc,
     0xfbfaf6, 0xbac4dd, 0x8f86a8, 0x6f6b92,
     0x8c819e, 0x72708c, 0x5d6a62,
     0xf3ead2, 0xa9b9c6, 0x5aa83c,
     0x3d8cc8, 0xd5ecf6,
     0xd7a53f, 0xe8c45c, 0xb07a2e, 0xa8692c, 0xf6e2a6,
     0xf3e6c8, 0xf2e7d2, 0xc0aa9a, 0x8a3a26, 0x5c2618, 0x6a3420, 0x3a2620,
     0x2f4f3a, 0x456a4a, 0xd8682a, 0xec8f3a,
     0xf6f0e3, 0x2e2420, 0xc8482a,
     {0x4f6f9a, 0xe6b43a, 0xc4482a, 0x7f8a3a, 0x6a3f2a, 0xe8892a, 0x8a4a6a},
     3.6f, 8.0f, 6.2f, 0.1f, 1, -99.f, 0.2f, 2, 1, 0.3f, 0.36f, -4.5f, -9.0f, 4.6f, 0.5f, 8.6f, 1.0f, 2},
    // Snow everywhere at dawn: white meadows, chalets under snowy roofs, dark firs, a pink horizon.
    {"WINTER MORNING",
     0x86b5df, 0xf1e5e2, 0xd8e2ee, 0xffffff, 0xfff4e2,
     0xffffff, 0xb0c8e4, 0x5f86b4, 0x48709f,
     0xa2bad7, 0x86a2c4, 0x6f8db1,
     0xf3ead2, 0xa9b9c6, 0x5aa83c,
     0x3d8cc8, 0xd5ecf6,
     0xf2f6fb, 0xffffff, 0xc6d6e9, 0xe4edf7, 0xd2dfee,
     0xc3d1e4, 0xf1e4cf, 0xa9a3bd, 0xfafcff, 0xb5c7dd, 0x6b3a2a, 0xffc85a,
     0x24463f, 0x3c6a5c, 0x24463f, 0x3c6a5c,
     0xf3f6f9, 0x1c2b44, 0xd23a3a,
     {0x4f9ad0, 0xe2a63a, 0xd23a3a, 0x2f8a6a, 0x24406e, 0x7a3f6a, 0x5f6f86},
     3.0f, 3.0f, 6.5f, 0.0f, 1, -99.f, 0.25f, 2, 1, 0.2f, 0.25f, -4.5f, -9.0f, 4.6f, -0.55f, 6.8f, 1.1f, 1},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The gutter around a piece (board units, each side), the clear's slide (seconds).
constexpr float GAP = 0.055f;
constexpr float WIPE_S = 0.3f;
// Each depth's share of the pan: sky, far range, hero and first range, second, third and lake, meadow, foreground.
constexpr float DEPTH[7] = {0.03f, 0.08f, 0.16f, 0.26f, 0.38f, 0.6f, 1.0f};

float ease(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}

vec3 saturateColor(const vec3& c, float k) {
    const float l = ugl::luma(c);
    const vec3 o = vec3(l) + (c - vec3(l)) * (1.f + k);
    return {saturate(o.x), saturate(o.y), saturate(o.z)};
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SwissUniverse::init(int width, int height) {
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "swiss shapes");
    progLand_ = ugl::program(ugl::FULLSCREEN_VS, shaders::LAND_FS, "swiss landscape");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glBindVertexArray(0);
    resize(width, height);
    return progShape_ && progLand_ && glGetError() == GL_NO_ERROR;
}

void SwissUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    unitPx_ = height_ / (2.f * hh);
    composeScene();
}

SwissUniverse::~SwissUniverse() {
    for (GLuint p : {progShape_, progLand_}) glDeleteProgram(p);
    glDeleteVertexArrays(1, &shapeVao_);
    for (GLuint b : {quadVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SwissUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        static const int BRIGHT[3] = {0, 1, 3}, DARK[2] = {2, 4};
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = DARK[(int)(h * 2) % 2];
        else v = BRIGHT[(int)(h * 3) % 3];
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x5A155ull);
    composeScene();
}

void SwissUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)impact;
    level = std::clamp(level, 0, 2);
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, 1.f, 5.f);
}

// The drop's downbeat: a big flock rises from both sides (only if this is a real DROP section: see render).
void SwissUniverse::drop(float strength) {
    (void)strength;
    dropHit_ = true;
}

vec2 SwissUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SwissUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    const vec3 bg = lerp(hex(V.skyTop), hex(V.skyBot), 0.55f);
    h.background = ugl::toLinear(bg);
    h.ink = ugl::toLinear(hex(V.ink));
    h.accent = ugl::toLinear(hex(V.accent));
    h.light = ugl::luma(bg) > 0.4f;
    h.styles = TS_TILES | TS_CURTAIN | TS_REBUILD;
    return h;
}

// =====================================================================================================
// The scene
// =====================================================================================================

// The village, the trees and the foreground's firs, in each layer's own units (sorted far to near: drawn in order).
// The village stands on one side of the poster, the hero peak on the other.
void SwissUniverse::composeScene() {
    const VariantDef& V = VARIANTS[variant_];
    Rng r(seed_ ^ 0xA1F5ull ^ ((uint64_t)variant_ << 32));
    const float W = viewH_.x;
    const float side = (seed_ >> 7) & 1 ? 1.f : -1.f; // the village's side
    const float vx = side * std::max(14.5f, (W + 12.f) * 0.5f);
    std::vector<El> m, fg;
    auto treeKind = [&](bool nearVillage) -> float {
        if (V.trees == 0) return nearVillage && r.chance(0.6f) ? 4.f : 3.f;
        if (V.trees == 2) return r.chance(0.55f) ? 6.f : 3.f;
        return 3.f;
    };
    // the village: a back row, the church, a front row
    if (V.village > 0) {
        const int nBack = r.irange(4, 5), nFront = r.irange(3, 4);
        float x = vx - 4.2f;
        for (int i = 0; i < nBack; i++) {
            x += r.range(1.2f, 1.8f);
            const float kind = V.village == 2 || r.chance(0.45f) ? 0.f : 1.f;
            m.push_back({x, r.range(0.25f, 0.55f), r.range(0.68f, 0.82f), kind});
        }
        m.push_back({vx + side * r.range(-1.2f, -0.2f), 0.6f, 0.82f, 2.f});
        x = vx - 3.6f;
        for (int i = 0; i < nFront; i++) {
            x += r.range(1.5f, 2.3f);
            const float kind = V.village == 2 || r.chance(0.5f) ? 0.f : 1.f;
            m.push_back({x, r.range(1.15f, 1.75f), r.range(0.95f, 1.1f), kind});
        }
        // cypresses and firs at the village's edges
        for (int i = 0; i < 4; i++) {
            const float ex = vx + (i & 1 ? 1.f : -1.f) * r.range(4.4f, 6.2f) + r.range(-0.5f, 0.5f);
            m.push_back({ex, r.range(0.4f, 1.6f), r.range(0.8f, 1.0f), treeKind(true)});
        }
    } else {
        // a hut or two on the alpine pasture
        m.push_back({vx + r.range(-1.f, 1.f), 0.5f, 0.8f, 0.f});
        if (r.chance(0.6f)) m.push_back({vx + r.range(2.f, 3.f), 0.9f, 0.7f, 0.f});
    }
    // trees scattered over the meadow, in clumps, away from the village
    for (int c = 0; c < 6; c++) {
        float cx = r.range(-W - 4.f, W + 4.f);
        if (V.village > 0 && std::fabs(cx - vx) < 6.f) cx = vx - side * r.range(7.f, 12.f);
        const int n = r.irange(2, 4);
        for (int i = 0; i < n; i++)
            m.push_back({cx + r.range(-1.6f, 1.6f), r.range(0.05f, 1.2f), r.range(0.5f, 0.8f), treeKind(false)});
    }
    std::sort(m.begin(), m.end(), [](const El& a, const El& b) { return a.depth < b.depth; });
    // the foreground: big trees at the poster's corners and a few bushes
    for (float s : {-1.f, 1.f}) {
        const int n = r.irange(3, 4);
        for (int i = 0; i < n; i++) {
            const float ex = s * (W - r.range(0.f, 6.5f));
            const float k = r.chance(0.2f) ? 5.f : treeKind(false);
            fg.push_back({ex, r.range(0.3f, 1.4f), k == 5.f ? r.range(0.8f, 1.1f) : r.range(1.1f, 1.6f), k});
        }
    }
    std::sort(fg.begin(), fg.end(), [](const El& a, const El& b) { return a.depth < b.depth; });
    els_.clear();
    for (const El& e : m) if (els_.size() < 34) els_.push_back(e);
    elFg_ = (int)els_.size();
    for (const El& e : fg) if (els_.size() < 48) els_.push_back(e);
}

void SwissUniverse::drawLandscape() {
    const VariantDef& V = VARIANTS[variant_];
    const float W = viewH_.x;
    const float side = (seed_ >> 7) & 1 ? 1.f : -1.f;
    const GLuint p = progLand_;
    glUseProgram(p);
    set2f(p, "uRes", (float)width_, (float)height_);
    glUniform3f(ugl::U(p, "uView"), viewC_.x, viewC_.y, unitPx_);
    set1f(p, "uTime", sceneT_);
    glUniform1fv(ugl::U(p, "uOff"), 7, layerOff_);
    glUniform3f(ugl::U(p, "uCloudOff"), cloudOff_[0], cloudOff_[1], cloudOff_[2]);
    set1f(p, "uSeed", (float)(seed_ % 997) * 0.37f);
    const float drop = smoothstepf(0.f, 1.f, dropAmt_);
    auto c = [&](const char* n, unsigned v) { set3f(p, n, hex(v)); };
    c("uSkyTop", V.skyTop);
    set3f(p, "uSkyBot", lerp(hex(V.skyBot), hex(V.sun), 0.35f * drop));
    c("uHaze", V.haze); c("uCloudC", V.cloud); c("uSunC", V.sun);
    c("uSnow", V.snow); c("uSnowSh", V.snowSh); c("uRock", V.rock); c("uRockSh", V.rockSh);
    c("uMid1", V.mid1); c("uMid2", V.mid2); c("uMid3", V.mid3);
    c("uHeroLit", V.heroLit); c("uHeroSh", V.heroSh); c("uHeroGrass", V.heroGrass);
    c("uLake", V.lake); c("uLakeLit", V.lakeLit);
    c("uMeadow", V.meadow); c("uMeadowLit", V.meadowLit); c("uMeadowDk", V.meadowDk); c("uFg", V.fg); c("uDot", V.dot);
    c("uPath", V.path); c("uWall", V.wall); c("uWallSh", V.wallSh); c("uRoof", V.roof); c("uRoofSh", V.roofSh);
    c("uTimber", V.timber); c("uWindow", V.window);
    c("uTree", V.tree); c("uTreeLit", V.treeLit); c("uTree2", V.tree2); c("uTree2Lit", V.tree2Lit);
    set4f(p, "uP0", V.farBase, V.snowLine, V.farHeight, V.round);
    const float heroX = -side * std::max(13.f, (W + 9.f) * 0.5f);
    set4f(p, "uP1", (float)V.hero, heroX, V.lakeY, V.mist);
    const float vx = side * std::max(14.5f, (W + 12.f) * 0.5f);
    set4f(p, "uP2", vx, (float)V.hasPath, V.cloudFlat, V.cloudCover);
    set4f(p, "uP3", V.meadowY, V.fgY, 0.f, V.midH);
    const float sunX = V.sunR > 0.f ? -side * V.sunX * W : 0.f;
    glUniform3f(ugl::U(p, "uSun"), sunX, V.sunY, V.sunR * (1.f + 0.25f * drop));
    const float calm = 1.f - saturate(lv_);
    set4f(p, "uGrade", 0.3f * drop + 0.1f * smoothstepf(1.f, 2.f, lv_), 0.1f * calm, smoothstepf(0.f, 1.f, slow_), drop);
    set1f(p, "uWind", 0.5f + 0.4f * saturate(lv_ * 0.5f) + 0.8f * drop);
    std::vector<float> el;
    for (const El& e : els_) el.insert(el.end(), {e.x, e.depth, e.size, e.kind});
    set1i(p, "uElCount", (int)els_.size());
    set1i(p, "uElFg", elFg_);
    if (!els_.empty()) glUniform4fv(ugl::U(p, "uEl"), (GLsizei)els_.size(), el.data());
    ugl::drawFullscreen();
}

// =====================================================================================================
// Birds
// =====================================================================================================

// A loose V of n birds from (x, y), flying toward dir (-1 left, 1 right), climbing at first.
void SwissUniverse::flock(float x, float y, int n, float dir, float climb) {
    const float speed = rng_.range(1.7f, 2.3f);
    for (int i = 0; i < n && birds_.size() < 96; i++) {
        const int k = (i + 1) / 2, s = (i & 1) ? 1 : -1;
        Bird b;
        b.p = vec2{x - dir * 0.55f * k + rng_.range(-0.2f, 0.2f), y + s * 0.35f * k + rng_.range(-0.2f, 0.2f)};
        b.v = vec2{dir * speed * rng_.range(0.92f, 1.08f), climb * rng_.range(0.8f, 1.2f)};
        b.phase = rng_.range(0.f, TAU);
        b.rate = rng_.range(9.f, 12.f);
        b.size = rng_.range(0.22f, 0.32f);
        b.glide = rng_.range(0.f, TAU);
        b.wob = rng_.range(0.f, TAU);
        birds_.push_back(b);
    }
}

void SwissUniverse::updateBirds(float dt) {
    for (Bird& b : birds_) {
        b.v.y = approach(b.v.y, 0.25f + 0.2f * std::sin(b.wob), 0.35f, dt);
        b.wob += dt * 0.6f;
        b.p = vec2{b.p.x + b.v.x * dt, b.p.y + b.v.y * dt};
        b.phase += b.rate * dt;
        b.glide += dt * 0.9f;
    }
    const float W = viewH_.x + 3.f, Ht = viewH_.y + 3.f;
    birds_.erase(std::remove_if(birds_.begin(), birds_.end(),
                                [&](const Bird& b) { return std::fabs(b.p.x - viewC_.x) > W || b.p.y - viewC_.y > Ht; }),
                 birds_.end());
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void SwissUniverse::syncIds(const Game& g) {
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

void SwissUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::HardDrop: {
        // a thin rule at the contact, closing toward the middle
        float x0 = 99, x1 = -99, yl = 99;
        for (auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            x0 = std::min(x0, p.x - 0.5f);
            x1 = std::max(x1, p.x + 0.5f);
            yl = std::min(yl, p.y - 0.5f);
        }
        if (x1 > x0) { lockLine_ = 1.f; lockX0_ = x0; lockX1_ = x1; lockY_ = yl; }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // each cleared row closes into a flat bar and slides out sideways
        int k = 0;
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            if (!rows[y]) continue;
            Wipe w;
            w.y = layout::cellPos(0.f, (float)y).y;
            for (int x = 0; x < Game::W; x++) { w.type[x] = -1; w.mask[x] = 0; }
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) w.type[c.x] = std::clamp(c.type, 0, 6);
            w.t = 0.f;
            w.delay = 0.f;
            w.dir = ((y + k) & 1) ? 1.f : -1.f;
            k++;
            wipes_.push_back(w);
        }
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
        }
        // a flock takes off from the meadow on one side and flies away outward
        {
            const float s = rng_.chance(0.5f) ? 1.f : -1.f;
            const float x = viewC_.x + s * rng_.range(12.5f, std::max(13.f, viewH_.x - 2.f));
            const VariantDef& V = VARIANTS[variant_];
            flock(x, V.meadowY - 0.3f, 2 + 2 * std::min(ev.count, 4), s, rng_.range(1.0f, 1.4f));
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack slides out row by row from the top
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            Wipe w;
            bool any = false;
            w.y = layout::cellPos(0.f, (float)y).y;
            for (int x = 0; x < Game::W; x++) w.type[x] = -1;
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) {
                    w.type[c.x] = std::clamp(c.type, 0, 6);
                    any = true;
                }
            if (!any) continue;
            // the pieces keep their shapes until their row leaves
            for (int x = 0; x < Game::W; x++) {
                const int id = ids_[y][x];
                auto same = [&](int nx, int ny) {
                    return id && nx >= 0 && nx < Game::W && ny >= Game::HIDDEN && ny < Game::H && ids_[ny][nx] == id;
                };
                const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
                int m = 0;
                for (int i = 0; i < 8; i++)
                    if (same(x + dx[i], y + dy[i])) m |= 1 << i;
                w.mask[x] = m;
            }
            w.t = 0.f;
            w.delay = 0.5f + 0.06f * (y - Game::HIDDEN);
            w.dir = (y & 1) ? 1.f : -1.f;
            wipes_.push_back(w);
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Shapes
// =====================================================================================================

vec2 SwissUniverse::px(float x, float y) const {
    return {(x - viewC_.x) * unitPx_ + width_ * 0.5f, height_ * 0.5f - (y - viewC_.y) * unitPx_};
}

void SwissUniverse::rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    const float clip[4] = {-1e5f, -1e5f, 1e5f, 1e5f};
    rectPxClip(x0, y0, x1, y1, c, a, clip);
}

void SwissUniverse::rectPxClip(float x0, float y0, float x1, float y1, const vec3& c, float a, const float clip[4]) {
    if (a <= 0.002f || x1 <= x0 || y1 <= y0) return;
    Shape s{{x0, y0, x1, y1}, {c.x, c.y, c.z, a}, {0, 0, 0, 0}, {clip[0], clip[1], clip[2], clip[3]}};
    shapes_.push_back(s);
}

void SwissUniverse::rectB(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    const vec2 p0 = px(std::min(x0, x1), std::max(y0, y1)), p1 = px(std::max(x0, x1), std::min(y0, y1));
    rectPx(p0.x, p0.y, p1.x, p1.y, c, a);
}

void SwissUniverse::plateB(float x0, float y0, float x1, float y1, float radius, const vec3& c, float a) {
    if (a <= 0.002f) return;
    const vec2 p0 = px(std::min(x0, x1), std::max(y0, y1)), p1 = px(std::max(x0, x1), std::min(y0, y1));
    Shape s{{p0.x, p0.y, p1.x, p1.y}, {c.x, c.y, c.z, a}, {1, radius * unitPx_, 0, 0}, {-1e5f, -1e5f, 1e5f, 1e5f}};
    shapes_.push_back(s);
}

void SwissUniverse::discB(float cx, float cy, float r, const vec3& c, float a) {
    if (r <= 0.01f || a <= 0.002f) return;
    const vec2 p = px(cx, cy);
    const float rp = r * unitPx_;
    Shape s{{p.x - rp, p.y - rp, p.x + rp, p.y + rp}, {c.x, c.y, c.z, a}, {2, rp, 0, 0}, {-1e5f, -1e5f, 1e5f, 1e5f}};
    shapes_.push_back(s);
}

void SwissUniverse::birdB(float cx, float cy, float halfW, float beat, const vec3& c, float a) {
    const vec2 p = px(cx, cy);
    const float h = halfW * unitPx_;
    Shape s{{p.x - h, p.y - h, p.x + h, p.y + h}, {c.x, c.y, c.z, a}, {3, beat, 0, 0}, {-1e5f, -1e5f, 1e5f, 1e5f}};
    shapes_.push_back(s);
}

void SwissUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(Shape), shapes_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shapes_.size());
    shapes_.clear();
}

// =====================================================================================================
// The board
// =====================================================================================================

// The seven types: the scene's own colors (roofs, sky, meadow, path...), deeper in a drop.
vec3 SwissUniverse::pieceColor(int type) const {
    const vec3 c = hex(VARIANTS[variant_].pieces[std::clamp(type, 0, 6)]);
    return saturateColor(c, 0.25f * smoothstepf(0.f, 1.f, dropAmt_));
}

// One cell of a piece: a flat square, inset by the gutter on the sides it does not share with its piece, its corner
// cut where two shared sides meet around a missing diagonal (so the piece's outline stays clean). Edges snapped to
// pixels so the cells of a piece meet without a seam. mask bits: 1 left, 2 right, 4 below, 8 above, 16 below-left,
// 32 below-right, 64 above-left, 128 above-right.
void SwissUniverse::drawCell(float x, float y, float half, int m, const vec3& c, float a, bool snap) {
    const float g = GAP * half * 2.f;
    const float x0 = x - half + ((m & 1) ? 0.f : g), x1 = x + half - ((m & 2) ? 0.f : g);
    const float yb = y - half + ((m & 4) ? 0.f : g), yt = y + half - ((m & 8) ? 0.f : g);
    vec2 p0 = px(x0, yt), p1 = px(x1, yb);
    vec2 gi = px(x0 + g, yt - g), go = px(x1 - g, yb + g);
    if (snap) {
        p0 = vec2{std::round(p0.x), std::round(p0.y)};
        p1 = vec2{std::round(p1.x), std::round(p1.y)};
        gi = vec2{std::round(gi.x), std::round(gi.y)};
        go = vec2{std::round(go.x), std::round(go.y)};
    }
    const bool cutBL = (m & 1) && (m & 4) && !(m & 16), cutBR = (m & 2) && (m & 4) && !(m & 32);
    const bool cutTL = (m & 1) && (m & 8) && !(m & 64), cutTR = (m & 2) && (m & 8) && !(m & 128);
    if (!cutBL && !cutBR && !cutTL && !cutTR) { rectPx(p0.x, p0.y, p1.x, p1.y, c, a); return; }
    // three slabs: top (y p0..gi), middle, bottom (go..p1)
    rectPx(cutTL ? gi.x : p0.x, p0.y, cutTR ? go.x : p1.x, gi.y, c, a);
    rectPx(p0.x, gi.y, p1.x, go.y, c, a);
    rectPx(cutBL ? gi.x : p0.x, go.y, cutBR ? go.x : p1.x, p1.y, c, a);
}

void SwissUniverse::drawBoard(const Game& g) {
    const VariantDef& V = VARIANTS[variant_];
    const float br = saturate(bridge_.amount);
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const vec3 ink = hex(V.ink), paper = hex(V.paper);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, smoothstepf(0.f, 1.f, br)); };
    auto mask8 = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const float plate = 0.965f * (1.f - smoothstepf(0.f, 1.f, br));

    // the paper: the well's panel, the previews' plates, a disc under the gauge
    rectB(-5.45f, -10.45f, 5.45f, 10.f, paper, plate);
    plateB(layout::HOLD_CENTER.x - 1.9f, layout::HOLD_CENTER.y - 1.45f, layout::HOLD_CENTER.x + 1.9f,
           layout::HOLD_LABEL.y + 0.75f, 0.3f, paper, plate);
    plateB(layout::nextCenter(0).x - 1.9f, layout::nextCenter(2).y - 1.25f, layout::nextCenter(0).x + 1.9f,
           layout::NEXT_LABEL.y + 0.75f, 0.3f, paper, plate);
    discB(layout::GAUGE_CENTER.x, layout::GAUGE_CENTER.y, layout::GAUGE_R + 0.45f, paper, plate);

    // the well's frame: two rules at its sides, the baseline under it
    {
        const float th = std::max(1.f, height_ / 1080.f * 1.5f);
        const vec3 c = tone(ink);
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const float al = 0.55f * (1.f - 0.6f * br);
        rectPx(std::round(a.x) - 2.f * th, a.y, std::round(a.x) - th, b.y + th, c, al);
        rectPx(std::round(b.x) + th, a.y, std::round(b.x) + 2.f * th, b.y + th, c, al);
        rectPx(std::round(a.x) - 2.f * th, std::round(b.y) + th, std::round(b.x) + 2.f * th, std::round(b.y) + 2.f * th, c, al);
    }

    // the ghost: the piece's outline, a fine rule along the edge of the piece's flat shape (inset by the gutter like
    // a locked piece), one closed line: a side runs to the inset corner where the piece turns outward, straight on
    // into its neighbor's side where the piece continues, and across its neighbor's rule where the piece turns
    // inward (a concave corner), so every corner closes.
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto in = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const float th = std::max(1.f, std::round(height_ / 1080.f * 2.f));
            const vec3 c = tone(lerp(paper, ink, 0.45f));
            auto X = [&](float bx) { return std::round(px(bx, 0.f).x); };
            auto Y = [&](float by) { return std::round(px(0.f, by).y); };
            const float gg = GAP;
            for (auto& cc : gc) {
                if (cc[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)cc[0], (float)cc[1]);
                auto has = [&](int dx, int dy) { return in(cc[0] + dx, cc[1] + dy); };
                const float xl = w.x - 0.5f, xr = w.x + 0.5f, yb = w.y - 0.5f, yt = w.y + 0.5f;
                const float L = X(xl + gg), R = X(xr - gg), T = Y(yt - gg), B = Y(yb + gg);
                auto topEnd = [&](int ox) { return !has(0, -1) ? T : has(ox, -1) ? Y(yt + gg) - th : Y(yt); };
                auto botEnd = [&](int ox) { return !has(0, 1) ? B : has(ox, 1) ? Y(yb - gg) + th : Y(yb); };
                auto leftEnd = [&](int oy) { return !has(-1, 0) ? L : has(-1, oy) ? X(xl - gg) - th : X(xl); };
                auto rightEnd = [&](int oy) { return !has(1, 0) ? R : has(1, oy) ? X(xr + gg) + th : X(xr); };
                if (!has(-1, 0)) rectPx(L, topEnd(-1), L + th, botEnd(-1), c, 1.f);
                if (!has(1, 0)) rectPx(R - th, topEnd(1), R, botEnd(1), c, 1.f);
                if (!has(0, -1)) rectPx(leftEnd(-1), T, rightEnd(-1), T + th, c, 1.f);
                if (!has(0, 1)) rectPx(leftEnd(1), B - th, rightEnd(1), B, c, 1.f);
            }
        }
    }

    // locked pieces
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
            drawCell(p.x, p.y, 0.5f, mask8(x, y, same), tone(pieceColor(c.type)), 1.f);
        }

    // cleared rows: a flat bar sliding out sideways, clipped at the well's edges
    {
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const float clip[4] = {std::round(a.x), -1e5f, std::round(b.x), 1e5f};
        for (const Wipe& w : wipes_) {
            if (w.t < w.delay) {
                for (int x = 0; x < Game::W; x++) {
                    if (w.type[x] < 0) continue;
                    const vec3 p = layout::cellPos((float)x, 0.f);
                    drawCell(p.x, w.y, 0.5f, w.mask[x], tone(pieceColor(w.type[x])), 1.f);
                }
                continue;
            }
            const float k = saturate((w.t - w.delay) / WIPE_S);
            const float dx = w.dir * (k * k) * 10.5f;
            for (int x = 0; x < Game::W; x++) {
                if (w.type[x] < 0) continue;
                const vec3 p = layout::cellPos((float)x, 0.f);
                vec2 p0 = px(p.x - 0.5f + dx, w.y + 0.5f - GAP), p1 = px(p.x + 0.5f + dx, w.y - 0.5f + GAP);
                rectPxClip(std::round(p0.x), std::round(p0.y), std::round(p1.x), std::round(p1.y),
                           tone(pieceColor(w.type[x])), 1.f, clip);
            }
        }
    }

    // the falling piece: exactly a locked piece
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto in = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) { // (whole above the well too: it spawns there)
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            drawCell(w.x, w.y, 0.5f, mask8(c[0], c[1], in), tone(pieceColor(p.type)), 1.f);
        }
    }

    // the hard drop's rule at the contact
    if (lockLine_ > 0.f) {
        const float k = 1.f - lockLine_;
        const float cx = 0.5f * (lockX0_ + lockX1_), hw = (0.5f * (lockX1_ - lockX0_) + 0.4f) * (1.f - ease(k));
        const float th = std::max(1.f, height_ / 1080.f * 2.f);
        const vec2 a = px(cx - hw, lockY_), b = px(cx + hw, lockY_);
        rectPx(a.x, a.y + th, b.x, a.y + 2.f * th, tone(ink), 0.9f);
    }

    // previews: small flat pieces on their plates
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        auto in = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            const float x = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, y = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            drawCell(x, y, 0.5f * sc, mask8(c[0], c[1], in), tone(pieceColor(type)), alpha);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    flushShapes();
}

// =====================================================================================================
// The HUD (lettered by the host)
// =====================================================================================================

// The unified HUD's inks: the poster's paper over the dark parts of the scene, its ink over the light ones, the
// roofs' red (or the scene's accent).
HudInk SwissUniverse::hudInk(const HudText& h) {
    (void)h;
    const VariantDef& V = VARIANTS[variant_];
    HudInk k;
    k.pale = hex(V.paper);
    k.deep = hex(V.ink);
    k.accent = hex(V.accent);
    return k;
}

// The HUD's own lettering (ZEN_HUD=universe): Inter, labels small and spaced in capitals, values bold and set tight,
// in the poster's ink and accent.
void SwissUniverse::hudLine(const HudText& h, text::Line& out) {
    const VariantDef& V = VARIANTS[variant_];
    text::Font* bold = text::font(FONT_BOLD);
    text::Font* reg = text::font(FONT_REG);
    constexpr float CAP = 0.601f; // Inter's cap height (em)
    const float s = height_ / 1080.f, base = h.scale * 12.f;
    const vec3 ink = hex(V.ink), accent = hex(V.accent);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    const float alpha = saturate(h.alpha) * vis;
    auto put = [&](text::Font* f, const std::string& t, float cap, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, cap / CAP, c, track * CAP, saturate(a));
        out.dy = dy;
        out.look.halo = hex(V.paper);
        out.look.haloA = 0.6f;
        out.look.haloW = 0.18f;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(reg, text::upper(h.text), base * 0.5f, 0.08f, ink, saturate(0.35f + h.alpha * 1.4f) * vis, base * 0.1f); break;
    case HUD_SECTION: put(bold, text::upper(h.text), base * 0.55f, 0.06f, accent, saturate(0.4f + h.alpha * 1.5f) * vis, base * 0.05f); break;
    case HUD_TITLE: put(bold, h.text, base * 0.62f, -0.01f, ink, saturate(h.alpha * 1.6f) * vis, base * 0.02f); break;
    case HUD_INFO: put(reg, h.text, base * 0.48f, 0.06f, ink, saturate(alpha * 1.3f) * 0.8f, base * 0.1f); break;
    case HUD_LABEL: put(reg, text::upper(h.text), base * 0.42f, 0.14f, ink, saturate(alpha * 1.3f) * 0.75f, base * 0.12f); break;
    case HUD_VALUE: put(bold, h.text, base * 0.62f, -0.01f, ink, alpha, base * 0.02f); break;
    case HUD_NOTICE: put(bold, h.text, base * 0.55f, 0.02f, accent, alpha); break;
    case HUD_KEY: put(bold, h.text, base * 0.55f, 0.f, ink, alpha); break;
    case HUD_TOAST: put(bold, text::lower(h.text), base * 0.72f, -0.02f, ink, alpha, -base * 0.05f); break;
    case HUD_TOAST_SUB: put(reg, h.text, base * 0.5f, 0.04f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(bold, h.text, base * 0.62f, -0.01f, ink, alpha);
        else put(reg, h.text, base * 0.5f, 0.03f, h.accent ? accent : ink, alpha);
        break;
    default: put(reg, h.text, base * 0.5f, 0.02f, h.accent ? accent : ink, alpha); break;
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SwissUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    bare_ = f.bare;
    if (!bare_) syncIds(game); // the world alone: the pieces' ids kept for the whole frame
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) {
            overT_ = -1.f;
            wipes_.erase(std::remove_if(wipes_.begin(), wipes_.end(), [](const Wipe& w) { return w.delay > 0.f; }), wipes_.end());
        } else overT_ += dt;
    }
    time_ += dt;
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    // the scene's own clock: still when paused, slowed in slow motion
    const float sdt = f.paused ? 0.f : dt * (1.f - 0.8f * smoothstepf(0.f, 1.f, slow_));
    sceneT_ += sdt;

    // ---- The drop's look, only while the song plan's DROP section lasts.
    const bool inDrop = music.hasSong && music.inDrop;
    dropAmt_ = approach(dropAmt_, inDrop ? 1.f : 0.f, inDrop ? 1.6f : 0.8f, dt);
    const float W = viewH_.x;
    if (dropHit_) {
        dropHit_ = false;
        if (inDrop || dropAmt_ > 0.05f) {
            for (float s : {-1.f, 1.f}) flock(viewC_.x + s * rng_.range(13.f, std::max(13.5f, W - 2.f)), V.meadowY, 9, s, 1.3f);
        }
    }
    beat_ = music.hasSong ? music.beat : time_ * 2.0;
    if (inDrop && !f.paused) {
        // a flock crossing the sky every two bars, from either side
        const float two = (float)std::floor(beat_ / 8.0);
        if (two != flockBar_) {
            flockBar_ = two;
            const float s = rng_.chance(0.5f) ? 1.f : -1.f;
            flock(viewC_.x - s * (W + 1.5f), rng_.range(5.5f, 10.5f), rng_.irange(4, 7), s, rng_.range(-0.1f, 0.3f));
        }
    }

    // ---- Parallax: a slow swing, a sway on the bars, a lean away from the falling piece.
    {
        panT_ += sdt;
        float target = 0.f;
        if (game.hasPiece() && !game.over() && !bare_) {
            int cells[4][2];
            game.pieceCells(game.piece(), cells);
            float cx = 0.f;
            for (auto& c : cells) cx += c[0];
            cx = cx / 4.f + game.pieceVisualX() - game.piece().x;
            target = -0.12f * layout::cellPos(cx, 0.f).x;
        }
        pieceLean_ = approach(pieceLean_, target, 1.2f, sdt);
        const float swayA = 0.12f + 0.12f * saturate(lv_ * 0.5f) + 0.3f * smoothstepf(0.f, 1.f, dropAmt_);
        const float sway = swayA * (float)std::sin(beat_ / 8.0 * TAU);
        pan_ = 2.6f * std::sin(panT_ * TAU / 80.f) + (f.paused ? 0.f : 1.f) * sway + pieceLean_;
        for (int i = 0; i < 7; i++) layerOff_[i] = pan_ * DEPTH[i];
        const float cs = 1.f + 2.5f * smoothstepf(0.f, 1.f, dropAmt_);
        const float speeds[3] = {0.11f, 0.17f, 0.25f};
        for (int i = 0; i < 3; i++) cloudOff_[i] = std::fmod(cloudOff_[i] + sdt * speeds[i] * cs, 1000.f);
    }
    updateBirds(sdt);
    lockLine_ = std::max(0.f, lockLine_ - dt / 0.28f);
    for (Wipe& w : wipes_) w.t += dt;
    wipes_.erase(std::remove_if(wipes_.begin(), wipes_.end(), [](const Wipe& w) { return w.t > w.delay + WIPE_S; }), wipes_.end());

    // ---- The landscape.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    drawLandscape();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The birds, over the landscape (behind the paper).
    if (!birds_.empty()) {
        const vec3 bc = lerp(hex(V.ink), hex(V.skyTop), 0.18f);
        for (const Bird& b : birds_) {
            const float gl = smoothstepf(0.2f, 0.8f, std::sin(b.glide)); // gliding stretches between wing beats
            const float beat = lerpf(std::sin(b.phase), 0.35f, gl);
            birdB(b.p.x, b.p.y + 0.04f * std::sin(b.phase), b.size, beat, bc, 0.92f);
        }
        flushShapes();
    }

    // ---- The bridge: the world gives way to the bridge's color (the stack turns to its ink in drawBoard).
    if (bridge_.amount > 0.001f) {
        rectPx(0, 0, (float)width_, (float)height_, ugl::toSrgb(bridge_.color), smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        flushShapes();
    }

    // ---- The game.
    if (!bare_) drawBoard(game);

    // ---- Dim (pause, game over screen) and the startup fade.
    {
        const float d = 0.45f * dimT_;
        if (d > 0.002f) rectPx(0, 0, (float)width_, (float)height_, hex(V.paper), d);
        if (f.fade < 0.999f) rectPx(0, 0, (float)width_, (float)height_, vec3(0.f), 1.f - saturate(f.fade));
        flushShapes();
    }
    if (!f.panels.empty() && !bare_) {
        for (const HudRect& r : f.panels) {
            const vec3 c = r.custom ? r.color : r.accent ? hex(V.accent) : hex(V.paper);
            rectPx(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_swiss() {
    static const UniverseEntry e{
        "SWISS POSTER", "a vintage travel poster in flat design: peaks, ranges, a lake, a village and firs in parallax behind a paper panel; birds rise on clears, the drop warms the light and sends flocks across the sky",
        [] { return std::unique_ptr<Universe>(new SwissUniverse()); },
        // Bright and calm: it suits most songs, airy, melodic ones best.
        [](const SongLook& l) { return 0.8f + 0.2f * l.brightness + 0.1f * (1.f - l.dynamics); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

// BRUTALIST: raw concrete under a hard sun, in 3D. The blocks are cast concrete.
//
// One world: a plaza of board-marked concrete, seen frontally like an architect's photograph (a shift lens: the
// verticals stay vertical). The well is a tall slot cut into a monument, between two piers and under a heavy lintel,
// its back lined with dark concrete. Across the plaza, up a flight of steps, a civic building stands on pilotis, its
// upper floors cantilevered behind their fins over deep-set windows, a stair tower and an outside stair at its end; on
// the right a pile of cast modules grows beside its service core, lowered one by one by a tower crane; beyond, the
// far city in the haze under the sky of the hour. Weeds grow from the cracks, a birch sapling from the paving.
//
// The pieces are cast concrete blocks, each piece one cast (the joints show between pieces only, their arrises
// chamfered), each of the seven types its own concrete or a sober pigment and finish (smooth with blowholes, exposed
// aggregate, fine board marks). The sun casts hard shadows through a shadow map: the stack's shadow on the slot's
// lining, the lintel's over its top, the fins' across the facades; it moves on with every phase of the song, its
// shadows sweeping the place. Dust drifts in the air and only shines where the sun crosses it.
//
// The song: calm is a veiled sun (a thin high cloud, soft shadows, little dust); mid brings the sun out; a peak is the
// hard light (crisp shadows, the dust glittering on the two-beat grid, the crane working faster). The drop is the sun
// breaking through (a real drop only, not a chorus): the veil clears, the light swells over half a second and settles,
// the shadows turn razor sharp, the glazing glints, a cloud of dust shaken out of the slot glitters in the light (by
// dusk and by night, the monument's floodlights come up and the windows light). A cleared row's blocks pop out
// of the slot and tumble down onto the plaza, and a module is lowered onto the growing pile for each of them: over the song the
// building grows, its shadows longer and more complex. A quad sends a glint over all the glazing and shakes the
// dust out of the slot. Slow motion is the evening: the sun sinks low and warm, the shadows stretch, windows light up,
// time stretches. Game over topples the stack out of the slot onto the plaza, block by block from the top, in dust.
//
// Layout contract: the reference camera lowered to EYE_Y and looking straight ahead, its image shifted back (a lens
// shift): the board's plane keeps its pixels, the place recedes in perspective.
#include "universes/brutalist/brutalist.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/brutalist/shaders.hpp"
#include "universes/brutalist/world.hpp"
#include "universes/registry.hpp"

using namespace brutalist_world;

namespace {

// Space Mono (Bold and Regular), SIL Open Font License, see OFL-SpaceMono.txt.
const unsigned char FONT_BOLD[] = {
#include "universes/brutalist/SpaceMono-Bold.ttf.inc"
};
const unsigned char FONT_REGULAR[] = {
#include "universes/brutalist/SpaceMono-Regular.ttf.inc"
};

namespace shaders = brutalist_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
float hashf(uint64_t x) { return (splitmix64(x) >> 40) * (1.0f / 16777216.0f); }

// An hour on the plaza (sRGB): the sky (zenith, horizon, the sun's glow), the sun's light, the sky's and the ground's
// ambient light, the concrete, the slot's lining, the glazing, the lit windows, the weeds, the crane's paint, the far
// city, the seven blocks (I O T S Z J L); the sun's azimuth (0: behind us, + from the right) and height, snow, haze,
// the high veil, stars, how many windows are lit, the HUD's ink (1 dark on a bright day).
struct VariantDef {
    const char* name;
    unsigned zen, hor, glow, sun, sky, ground, conc, lining, glass, win, plant, steel, far;
    unsigned piece[7];
    float sunAz, sunEl, sunK, snow, haze, veil, stars, warmLights, hudDark;
};
const VariantDef VARIANTS[] = {
    // a high summer sun, a deep blue sky, pale grey concrete
    {"MIDDAY", 0x2f62a4, 0xc4d4e2, 0xfff0d6, 0xfff2e0, 0x7e9cc4, 0x9c9488, 0xb6b2aa, 0x353638, 0x1c2026, 0xffd9a0, 0x5e7a3c, 0xc8a434,
     0x9eaabb,
     {0xe8e4da, 0xd6a443, 0xb4553a, 0x83a487, 0xdb9c8c, 0x6a88ac, 0xc4b08a},
     0.85f, 0.88f, 1.f, 0.f, 0.0045f, 0.f, 0.f, 0.f, 1.f},
    // the morning: a low sun from the left, long cool shadows, a pale warm sky
    {"MORNING", 0x5582b6, 0xeedcc6, 0xffc88c, 0xffdcb0, 0x7690bc, 0x8a7c6c, 0xbab4aa, 0x34353a, 0x1c2028, 0xffd29a, 0x667e3e, 0xd8d8d4,
     0xb0b4c0,
     {0xece6da, 0xd8a24a, 0xb85a3e, 0x7ea488, 0xe0a08e, 0x6a8ab4, 0xc6b28c},
     -0.8f, 0.42f, 1.f, 0.f, 0.0036f, 0.f, 0.f, 0.f, 1.f},
    // a winter's day: snow on every ledge, a pale low sun, cold concrete, a red crane
    {"WINTER", 0x6a8cb2, 0xdce4ec, 0xfff0e4, 0xfff0e6, 0x9cb0cc, 0xb6bac4, 0xa8a9aa, 0x303236, 0x1a1e26, 0xffd7a4, 0x7a6e4c, 0xb8402e,
     0xb4bccb,
     {0xf2f0ea, 0xd2a046, 0xb04a36, 0x6f9c80, 0xd8909c, 0x5a80b4, 0xbca78a},
     0.95f, 0.42f, 0.92f, 1.f, 0.0055f, 0.f, 0.f, 0.f, 1.f},
    // an autumn afternoon: a warm raking sun, ochre concrete, rust in the weeds, a dark crane
    {"AUTUMN", 0x46709e, 0xe6d6b6, 0xffd096, 0xffe0b2, 0x7a8ebc, 0x8a7458, 0xbcab92, 0x37332e, 0x1e1e22, 0xffcf8c, 0xa45a28, 0x3e4246,
     0xb2aca4,
     {0xeee4d2, 0xcf9a3c, 0xa84a32, 0x7f9a74, 0xd49480, 0x5f7c9e, 0xb39a74},
     1.f, 0.55f, 1.f, 0.f, 0.0038f, 0.f, 0.f, 0.f, 1.f},
    // dusk: the sun on the horizon, orange light on the concrete, the windows lighting up
    {"DUSK", 0x26325c, 0xf09666, 0xff8846, 0xff9a58, 0x55588a, 0x5c4a44, 0xaca298, 0x2e2e34, 0x181a22, 0xffc070, 0x5a6a3a, 0xd6d6d2,
     0x6a6680,
     {0xeee6dc, 0xe2ae54, 0xc2563c, 0x8cb094, 0xeaa496, 0x7896c4, 0xd2bc96},
     -1.05f, 0.12f, 1.f, 0.f, 0.0062f, 0.f, 0.15f, 0.75f, 0.f},
    // moonlight: a cold moon, stars, the windows and the floodlights
    {"MOONLIGHT", 0x060a18, 0x1c2842, 0x34446a, 0x8ea8d8, 0x2a3656, 0x14161e, 0x9c9ea6, 0x1e2026, 0x10131a, 0xffbe6c, 0x3a4a34, 0xb0b4bc,
     0x2a3044,
     {0xe6e8ee, 0xe2b45c, 0xd06a50, 0x8ec4a2, 0xeaa6b0, 0x82a6e0, 0xd4c4a2},
     0.75f, 0.7f, 0.42f, 0.f, 0.0055f, 0.f, 1.f, 1.f, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float CELL_H = 0.47f;    // a block's half size in the well (the joints between pieces)
constexpr float CELL_D = 0.47f;    // its half depth

mat4 inverse(const mat4& m) {
    const float* a = m.m;
    float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    const float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    mat4 r{};
    if (std::fabs(det) < 1e-12f) return mat4::identity();
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] / det;
    return r;
}

int glyphIndex(unsigned char ch) {
    if (ch < 32 || ch >= 128) ch = '?';
    return ch - 32;
}

void lerpPal(vec3& a, const vec3& b, float k) { a = lerp(a, b, k); }

// The sun's way over the song: an hour of the day's arc for each phase (it swings back at the arc's ends).
float sunSweep(int hour) {
    const int n = 10;
    const int k = hour % (2 * n);
    const float u = (k < n ? k : 2 * n - k) / (float)n; // 0 .. 1 .. 0
    return -0.55f + 1.1f * u;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

void BrutalistUniverse::setupBoxVao(GLuint& vao, GLuint& vbo) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(2 + i);
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Box), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(2 + i, 1);
    }
    glBindVertexArray(0);
}

bool BrutalistUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "brutalist sky");
    progBox_ = ugl::program(shaders::BOX_VS, withCommon(shaders::BOX_FS, true).c_str(), "brutalist solids");
    progDepth_ = ugl::program(shaders::DEPTH_VS, shaders::DEPTH_FS, "brutalist shadow");
    progSprite_ = ugl::program(shaders::SPRITE_VS, withCommon(shaders::SPRITE_FS).c_str(), "brutalist dust");
    progLine_ = ugl::program(shaders::LINE_VS, withCommon(shaders::LINE_FS).c_str(), "brutalist cables");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "brutalist down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "brutalist blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "brutalist final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "brutalist shape");

    // a unit cube (-1..1): 36 vertices, position + normal
    {
        std::vector<float> v;
        const int faces[6][3] = {{0, 1, 2}, {0, -1, 2}, {1, 1, 2}, {1, -1, 2}, {2, 1, 0}, {2, -1, 0}};
        for (int f = 0; f < 6; f++) {
            const int ax = faces[f][0];
            const float sg = (float)faces[f][1];
            const int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
            const float corners[6][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, -1}, {1, 1}, {-1, 1}};
            for (int k = 0; k < 6; k++) {
                float p[3], n[3] = {0, 0, 0};
                p[ax] = sg;
                // keep the winding counter-clockwise seen from outside
                p[a1] = corners[k][0];
                p[a2] = corners[k][1] * sg;
                n[ax] = sg;
                v.insert(v.end(), {p[0], p[1], p[2], n[0], n[1], n[2]});
            }
        }
        glGenBuffers(1, &cubeVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    }
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    setupBoxVao(staticVao_, staticVbo_);
    setupBoxVao(boxVao_, boxVbo_);
    setupBoxVao(cellVao_, cellVbo_);
    // dust: the quad + (position size, color intensity) per instance
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 2; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    // cables: a strip (along 0..1, across -1..1) + three vec4 per instance
    {
        const float strip[8] = {0, -1, 1, -1, 0, 1, 1, 1};
        GLuint stripVbo = 0;
        glGenVertexArrays(1, &lineVao_);
        glGenBuffers(1, &lineVbo_);
        glGenBuffers(1, &stripVbo);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, stripVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(strip), strip, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Line), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
        // the strip's buffer stays bound to the VAO (freed with the context)
    }
    glGenVertexArrays(1, &quadVao_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    // the sun's shadow map: a depth texture compared in the shaders (hardware filtered)
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_N, SHADOW_N, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    resize(width, height);
    // the dust in the air, in front of the monument and around it
    motes_.clear();
    for (int i = 0; i < 260; i++) {
        Mote m;
        m.p = vec3(rng_.range(-22.f, 22.f), rng_.range(Y_G, 12.f), rng_.range(-6.f, 14.f));
        m.v = vec3(rng_.range(-0.1f, 0.1f), rng_.range(-0.02f, 0.08f), rng_.range(-0.05f, 0.05f));
        m.size = rng_.range(0.8f, 1.6f);
        m.seed = rng_.uniform();
        m.max = 1e9f;
        motes_.push_back(m);
    }
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

// The layout's reference camera lowered to EYE_Y and looking straight ahead, its image shifted back: the board plane
// lands exactly on the reference camera's pixels.
void BrutalistUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float focal = 1.f / std::tan(half);
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = focal * (EYE_Y - ref.target.y) / ref.dist;
    vp_ = shift * mat4::perspective(2.f * half, aspect, 0.5f, 900.f) * view;
    invVp_ = inverse(vp_);

    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    if (msFbo_) {
        glDeleteFramebuffers(1, &msFbo_);
        glDeleteRenderbuffers(1, &msColor_);
        glDeleteRenderbuffers(1, &msDepth_);
    }
    glGenFramebuffers(1, &msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA16F, width_, height_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, width_, height_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

BrutalistUniverse::~BrutalistUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progSky_, progBox_, progDepth_, progSprite_, progLine_, progDown_, progBlur_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (GLuint v : {staticVao_, boxVao_, cellVao_, spriteVao_, lineVao_, quadVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {cubeVbo_, quadVbo_, staticVbo_, boxVbo_, cellVbo_, spriteVbo_, lineVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    if (msFbo_) glDeleteFramebuffers(1, &msFbo_);
    if (msColor_) glDeleteRenderbuffers(1, &msColor_);
    if (msDepth_) glDeleteRenderbuffers(1, &msDepth_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 BrutalistUniverse::pieceColor(int type) const { return pal_.piece[std::clamp(type, 0, 6)]; }

void BrutalistUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = 5;                                  // moonlight
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 4;                   // dusk
        else if (t.hasSong && t.minor && t.airWeight > 0.55f) v = 2;                              // winter
        else if (t.hasSong && !t.minor && t.brightness > 0.62f) v = 0;                            // midday
        else if (t.hasSong && t.minor) v = 3;                                                     // autumn
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_ || !colSet_;
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    Palette& P = palT_;
    P.zen = lin(V.zen); P.hor = lin(V.hor); P.glow = lin(V.glow); P.sun = lin(V.sun) * 3.f * V.sunK;
    P.sky = lin(V.sky) * 0.46f; P.ground = lin(V.ground) * 0.34f;
    P.conc = lin(V.conc) * 0.72f; P.lining = lin(V.lining) * 1.5f; P.glass = lin(V.glass); P.win = lin(V.win) * 1.1f;
    P.plant = lin(V.plant); P.steel = lin(V.steel); P.far = lin(V.far);
    for (int i = 0; i < 7; i++) P.piece[i] = lin(V.piece[i]);
    P.sunAz = V.sunAz; P.sunEl = V.sunEl; P.snow = V.snow; P.haze = V.haze; P.veil = V.veil; P.stars = V.stars;
    P.warmLights = V.warmLights; P.hudDark = V.hudDark;
    const bool instant = !colSet_ || seconds <= 0.1f;
    if (instant) {
        pal_ = palT_;
        colSet_ = true;
        sunAz_ = sunAzTo_ = P.sunAz + sunSweep(hour_) * 0.6f;
        sunEl_ = sunElTo_ = P.sunEl;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0xB2ull);
    if (changed || instant || static_.empty()) {
        buildWorld();
        planGrowth();
        placed_ = 0;
        queued_ = 0;
        crane_ = Crane();
        // a song starts with the pile's first levels already standing
        const int start = std::min((int)plan_.size(), 6);
        for (int i = 0; i < start; i++) { plan_[i].t = 1.f; plan_[i].landed = 99.f; }
        placed_ = start;
    }
}

void BrutalistUniverse::setLevel(int level, float energy, float seconds, float impact) {
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
    if (level != level_) {
        // a new phase: the sun moves on along its arc
        hour_++;
        sunAzTo_ = palT_.sunAz + sunSweep(hour_) * 0.6f;
        sunElTo_ = palT_.sunEl + (level == 2 ? 0.06f : 0.f);
    }
    const bool rising = level > level_;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

// The drop: the sun breaks through (on a real drop: render checks the section).
void BrutalistUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swellPeak_ = 0.7f + 0.3f * strength;
}

void BrutalistUniverse::refresh(float strength) {
    refreshK_ = std::max(refreshK_, strength);
    // a new section: the sun steps on a little
    sunAzTo_ += 0.04f * (sunSweep(hour_ + 1) > sunSweep(hour_) ? 1.f : -1.f);
}

vec2 BrutalistUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints BrutalistUniverse::hints() const {
    TransitionHints h;
    const bool day = pal_.hudDark > 0.5f;
    h.background = lerp(pal_.conc * 0.7f, pal_.hor, 0.35f);
    h.light = day;
    h.ink = day ? pal_.lining * 0.8f : lerp(pal_.conc, vec3(1.f), 0.3f);
    h.accent = pal_.piece[1];
    h.styles = TS_REBUILD | TS_LUMA | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// A puff of dust: motes pushed out from a point, settling.
void BrutalistUniverse::puff(const vec3& at, int count, float spread, float up, float k) {
    for (int i = 0; i < count; i++) {
        Mote m;
        const float a = rng_.range(-1.f, 1.f);
        m.p = at + vec3(a * spread * 0.5f, rng_.range(-0.1f, 0.2f), rng_.range(0.05f, 0.5f));
        m.v = vec3(a * rng_.range(0.6f, 1.6f) + rng_.range(-0.2f, 0.2f), rng_.range(0.05f, 0.5f) * up, rng_.range(0.2f, 0.9f));
        m.max = rng_.range(0.7f, 1.5f);
        m.size = rng_.range(1.2f, 2.6f);
        m.k = k;
        m.kind = 1;
        m.seed = rng_.uniform();
        motes_.push_back(m);
    }
}

void BrutalistUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        // the piece is one cast: its cells share an id (the joints only show between pieces)
        const int id = nextPid_++;
        int bottom = -1;
        float xs = 0.f, n = 0.f;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) {
                pid_[c.y][c.x] = id;
                seeds_[c.y][c.x] = rng_.uniform();
                bottom = std::max(bottom, c.y);
            }
        // dust pushed out from under it, at the contact
        for (auto& c : ev.cells)
            if (c.y == bottom) { xs += (float)c.x; n += 1.f; }
        if (n > 0.f && bottom >= Game::HIDDEN) {
            const vec3 p = layout::cellPos(xs / n, (float)bottom);
            puff(vec3(p.x, p.y - CELL_H, 0.05f), 10, n + 0.4f, 0.6f, 0.7f);
        }
        break;
    }
    case GameEvent::HardDrop: {
        float xs = 0.f, n = 0.f;
        int bottom = -1;
        for (auto& c : ev.cells) bottom = std::max(bottom, c.y);
        for (auto& c : ev.cells)
            if (c.y == bottom) { xs += (float)c.x; n += 1.f; }
        if (n > 0.f && bottom >= Game::HIDDEN) {
            const vec3 p = layout::cellPos(xs / n, (float)bottom);
            puff(vec3(p.x, p.y - CELL_H, 0.05f), 16, n + 1.f, 1.f, 1.f);
        }
        break;
    }
    case GameEvent::Clear: {
        // each block of the row pops out of the slot and tumbles down onto the plaza; a module will be lowered for each row
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        if (ev.count >= 4) quadT_ = 0.f;
        clearT_ = 0.f;
        queued_ += (int)rows.size();
        for (auto& c : ev.cells) {
            if (c.x < 0 || c.x >= Game::W || c.y < Game::HIDDEN) continue;
            Flight fl;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            fl.p0 = fl.cur = vec3(p.x, p.y, -CELL_D);
            fl.type = std::clamp(c.type, 0, 6);
            fl.seed = rng_.uniform();
            fl.delay = 0.015f * (float)std::abs(c.x * 2 - 9) + rng_.range(0.f, 0.03f);
            const float side = (c.x - 4.5f) / 4.5f;
            fl.vel = vec3(side * rng_.range(1.6f, 3.f) + rng_.range(-0.4f, 0.4f), rng_.range(3.5f, 6.f) + (ev.count >= 4 ? 1.5f : 0.f),
                          rng_.range(2.6f, 4.4f));
            fl.spin = normalize(vec3(rng_.range(0.5f, 1.f), rng_.range(-0.6f, 0.6f), side + rng_.range(-0.4f, 0.4f))) * rng_.range(4.f, 9.f);
            fl.kind = 0;
            flights_.push_back(fl);
        }
        for (int y : rows) {
            const vec3 p = layout::cellPos(4.5f, (float)y);
            puff(vec3(0.f, p.y - CELL_H * 0.5f, 0.1f), 8 + 4 * (int)rows.size(), 10.f, 1.2f, 0.9f);
        }
        // the ids and seeds of the rows above step down
        {
            bool cleared[Game::H] = {};
            for (int y : rows) cleared[y] = true;
            int np[Game::H][Game::W] = {};
            float ns[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                for (int x = 0; x < Game::W; x++) { np[dst][x] = pid_[y][x]; ns[dst][x] = seeds_[y][x]; }
                dst--;
            }
            std::memcpy(pid_, np, sizeof(pid_));
            std::memcpy(seeds_, ns, sizeof(seeds_));
        }
        break;
    }
    case GameEvent::TopOut: {
        // the stack topples out of the slot onto the plaza, block by block from the top
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            Flight fl;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            fl.p0 = fl.cur = vec3(p.x, p.y, -CELL_D);
            fl.vel = vec3(rng_.range(-0.6f, 0.6f), rng_.range(0.f, 0.8f), rng_.range(1.6f, 3.2f));
            fl.spin = normalize(vec3(rng_.range(0.6f, 1.f), rng_.range(-0.3f, 0.3f), rng_.range(-0.3f, 0.3f))) * rng_.range(2.f, 5.f);
            fl.type = std::clamp(c.type, 0, 6);
            fl.kind = 1;
            fl.delay = 0.07f * (c.y - top) + 0.2f * hashf((uint64_t)(c.x * 17 + c.y * 3));
            fl.seed = rng_.uniform();
            flights_.push_back(fl);
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The well: the cast blocks, the ghost, the previews
// =====================================================================================================

void BrutalistUniverse::buildBoard(const Game& g) {
    // a block: its box reaches its neighbors of the same piece (one cast), else stops short of them (the joint)
    auto cell = [&](const vec3& c, float half, int type, float seed, int join) {
        const float gap = half * (0.5f / CELL_H - 1.f);
        const float l = half + ((join & 1) ? gap : 0.f), rr = half + ((join & 2) ? gap : 0.f);
        const float u = half + ((join & 4) ? gap : 0.f), d = half + ((join & 8) ? gap : 0.f);
        const float dz = CELL_D * half / CELL_H;
        box(cells_, vec3(c.x + (rr - l) * 0.5f, c.y + (u - d) * 0.5f, c.z - dz), vec3((l + rr) * 0.5f, (u + d) * 0.5f, dz), pieceColor(type), M_BLOCK,
            (float)join, 0.f, (float)type, seed);
    };
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const int id = pid_[y][x];
            auto same = [&](int xx, int yy) {
                if (xx < 0 || xx >= Game::W || yy < Game::HIDDEN || yy >= Game::H) return false;
                return id != 0 && pid_[yy][xx] == id && g.cell(xx, yy).type == c.type && g.rowOffset(yy) == g.rowOffset(y);
            };
            const int join = (same(x - 1, y) ? 1 : 0) | (same(x + 1, y) ? 2 : 0) | (same(x, y - 1) ? 4 : 0) | (same(x, y + 1) ? 8 : 0);
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            cell(p, CELL_H, c.type, seeds_[y][x], join);
        }
    // the falling piece, one cast
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto has = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int i = 0; i < 4; i++) {
            const int x = cells[i][0], y = cells[i][1];
            if (y < Game::HIDDEN - 1) continue;
            const int join = (has(x - 1, y) ? 1 : 0) | (has(x + 1, y) ? 2 : 0) | (has(x, y - 1) ? 4 : 0) | (has(x, y + 1) ? 8 : 0);
            cell(layout::cellPos(x + dx, y + dy), CELL_H, p.type, 0.37f * i + 0.11f * p.type, join);
        }
        // the ghost: a fine chalk outline of where it lands
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            auto gh = [&](int x, int y) {
                for (auto& c : gc)
                    if (c[0] == x && c[1] == y) return true;
                return false;
            };
            const vec3 col = lerp(pieceColor(p.type), vec3(1.f), 0.4f);
            const float wpx = std::max(1.f, 1.4f * height_ / 1080.f);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                const float h = 0.47f, z = 0.01f;
                if (!gh(c[0], c[1] - 1)) line(cellLines_, vec3(w.x - h, w.y + h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.55f);
                if (!gh(c[0], c[1] + 1)) line(cellLines_, vec3(w.x - h, w.y - h, z), vec3(w.x + h, w.y - h, z), wpx, col, 0.55f);
                if (!gh(c[0] - 1, c[1])) line(cellLines_, vec3(w.x - h, w.y - h, z), vec3(w.x - h, w.y + h, z), wpx, col, 0.55f);
                if (!gh(c[0] + 1, c[1])) line(cellLines_, vec3(w.x + h, w.y - h, z), vec3(w.x + h, w.y + h, z), wpx, col, 0.55f);
            }
        }
    }
    // the previews: small casts at the layout's anchors
    auto preview = [&](int type, vec2 center, float sc) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        g.pieceCells(p, cells);
        auto has = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (int i = 0; i < 4; i++) {
            const int x = cells[i][0], y = cells[i][1];
            const float ox = x - (minx + maxx) * 0.5f, oy = y - (miny + maxy) * 0.5f;
            const int join = (has(x - 1, y) ? 1 : 0) | (has(x + 1, y) ? 2 : 0) | (has(x, y - 1) ? 4 : 0) | (has(x, y + 1) ? 8 : 0);
            cell(vec3(center.x + ox * sc, center.y - oy * sc, 0.f), CELL_H * sc, type, 0.21f * i, join);
        }
    };
    if (!g.over()) {
        preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0));
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i));
        }
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void BrutalistUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The HUD's inks (sRGB): charcoal on a pale halo by day, pale on a dark halo by night.
void BrutalistUniverse::hudInks(vec3& ink, vec3& halo, vec3& accent) const {
    const float day = pal_.hudDark;
    ink = lerp(vec3(0.92f, 0.91f, 0.88f), vec3(0.11f, 0.11f, 0.12f), day);
    halo = lerp(ugl::toSrgb(lerp(pal_.zen, pal_.lining, 0.5f)) * 0.5f, lerp(ugl::toSrgb(pal_.hor), vec3(1.f), 0.4f), day);
    accent = lerp(lerp(ugl::toSrgb(pal_.piece[1]), vec3(1.f), 0.1f), ugl::toSrgb(pal_.piece[2]) * 0.8f, day);
}

// The panels and the bonus gauge (the lettering: hudLine).
void BrutalistUniverse::drawHud(const Game& g, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    vec3 ink, halo, accent;
    hudInks(ink, halo, accent);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
    // the bonus gauge: a square frame (cast, like the slot), its charge running round it in the accent
    if (vis > 0.01f && !bare_) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = (project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x) * 0.86f;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const float th = std::max(1.5f, 2.f * s), th2 = std::max(3.f, 6.f * s);
        const float dimk = vis * (1.f - 0.5f * dimT_);
        auto rect = [&](float x0, float y0, float x1, float y1, const vec3& col, float al) {
            const float qd[6][6] = {{x0, y0, col.x, col.y, col.z, al}, {x1, y0, col.x, col.y, col.z, al}, {x1, y1, col.x, col.y, col.z, al},
                                    {x0, y0, col.x, col.y, col.z, al}, {x1, y1, col.x, col.y, col.z, al}, {x0, y1, col.x, col.y, col.z, al}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        };
        // the frame
        const float fa = (g.bonusReady() ? 0.85f : 0.55f) * dimk;
        rect(c.x - r, c.y - r - th * 0.5f, c.x + r, c.y - r + th * 0.5f, ink, fa);
        rect(c.x - r, c.y + r - th * 0.5f, c.x + r, c.y + r + th * 0.5f, ink, fa);
        rect(c.x - r - th * 0.5f, c.y - r, c.x - r + th * 0.5f, c.y + r, ink, fa);
        rect(c.x + r - th * 0.5f, c.y - r, c.x + r + th * 0.5f, c.y + r, ink, fa);
        // ticks: one per line of the charge, along the top edge
        for (int i = 1; i < Game::BONUS_LINES; i++) {
            const float x = c.x - r + 2.f * r * i / Game::BONUS_LINES;
            rect(x - th * 0.5f, c.y - r, x + th * 0.5f, c.y - r + th2 * 1.3f, ink, 0.4f * dimk);
        }
        flushShapes();
        // the charge: clockwise from the top left corner
        float left = fill * 8.f * r;
        auto run = [&](float x0, float y0, float x1, float y1) {
            const float len = std::fabs(x1 - x0) + std::fabs(y1 - y0);
            if (left <= 0.f) return;
            const float k = std::min(1.f, left / len);
            left -= len;
            const float xe = x0 + (x1 - x0) * k, ye = y0 + (y1 - y0) * k;
            rect(std::min(x0, xe) - th2 * 0.5f, std::min(y0, ye) - th2 * 0.5f, std::max(x0, xe) + th2 * 0.5f, std::max(y0, ye) + th2 * 0.5f, accent,
                 0.95f * dimk);
        };
        run(c.x - r, c.y - r, c.x + r, c.y - r);
        run(c.x + r, c.y - r, c.x + r, c.y + r);
        run(c.x + r, c.y + r, c.x - r, c.y + r);
        run(c.x - r, c.y + r, c.x - r, c.y - r);
        flushShapes();
    }
}

// The lettering, by the host (text.hpp): Space Mono, a mono grotesque like stencilled lettering, Bold nearly
// everywhere (Regular for the small print), a touch heavier than the font; charcoal on a pale halo by day, pale on a
// dark halo by night, the halo's strength by role.
void BrutalistUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    vec3 ink, halo, accent;
    hudInks(ink, halo, accent);
    text::Font* bold = text::font(FONT_BOLD);
    text::Font* reg = text::font(FONT_REGULAR);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float haloA = 0.75f;
    // a run: its size and letter spacing in pixels, moved down by dy, on a halo of strength ha
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float ha, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
        haloA = ha;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(bold, text::upper(h.text), base * 0.72f, base * 0.14f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.85f, 0.7f, base * 0.06f); break;
    case HUD_SECTION: put(bold, text::upper(h.text), base * 0.78f, base * 0.16f, accent, 0.3f + h.alpha * 1.7f, 0.55f, -base * 0.04f); break;
    case HUD_TITLE: put(bold, h.text, base * 1.0f, 0.f, ink, h.alpha * 1.3f, 0.55f, -base * 0.1f); break;
    case HUD_INFO: put(bold, text::upper(h.text), base * 0.7f, base * 0.1f, ink, saturate(alpha * 1.2f) * 0.85f, 0.7f); break;
    case HUD_LABEL: put(bold, text::upper(h.text), base * 0.74f, base * 0.22f, ink, saturate(alpha * 1.2f) * 0.88f, 0.75f); break;
    case HUD_VALUE: put(bold, h.text, base * 1.0f, 0.f, ink, alpha, 0.75f, -base * 0.04f); break;
    case HUD_NOTICE: put(bold, text::upper(h.text), base * 0.86f, base * 0.1f, accent, alpha, 0.6f); break;
    case HUD_KEY: put(bold, h.text, base * 0.8f, 0.f, ink, alpha, 0.6f); break;
    case HUD_TOAST: put(bold, text::upper(h.text), base * 1.0f, base * 0.2f, ink, alpha, 0.7f, -base * 0.1f); break;
    case HUD_TOAST_SUB: put(reg, h.text, base * 0.9f, base * 0.04f, accent, alpha * 0.95f, 0.7f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(bold, text::upper(h.text), base * 0.9f, base * 0.14f, ink, alpha, 0.75f);
        else put(reg, h.text, base * 0.9f, 0.f, h.accent ? accent : ink, alpha, 0.75f);
        break;
    default: put(reg, h.text, base * 0.74f, 0.f, h.accent ? accent : ink, alpha, 0.6f); break;
    }
    out.look.weight = 0.0075f;
    out.look.halo = halo;
    out.look.haloA = haloA;
    out.look.haloW = 0.092f;
    out.look.haloSmooth = true;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Drawing
// =====================================================================================================

void BrutalistUniverse::drawBoxes(GLuint vao, GLuint vbo, const std::vector<Box>* data, size_t count, bool upload) {
    if (!count) return;
    glBindVertexArray(vao);
    if (upload && data) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(Box)), data->data(), GL_STREAM_DRAW);
    }
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)count);
}

void BrutalistUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    // ---- A new variant eases in.
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        Palette& P = pal_;
        const Palette& T = palT_;
        lerpPal(P.zen, T.zen, k); lerpPal(P.hor, T.hor, k); lerpPal(P.glow, T.glow, k); lerpPal(P.sun, T.sun, k);
        lerpPal(P.sky, T.sky, k); lerpPal(P.ground, T.ground, k); lerpPal(P.conc, T.conc, k); lerpPal(P.lining, T.lining, k);
        lerpPal(P.glass, T.glass, k); lerpPal(P.win, T.win, k); lerpPal(P.plant, T.plant, k); lerpPal(P.steel, T.steel, k);
        lerpPal(P.far, T.far, k);
        for (int i = 0; i < 7; i++) lerpPal(P.piece[i], T.piece[i], k);
        P.snow = lerpf(P.snow, T.snow, k); P.haze = lerpf(P.haze, T.haze, k); P.veil = lerpf(P.veil, T.veil, k);
        P.stars = lerpf(P.stars, T.stars, k); P.warmLights = lerpf(P.warmLights, T.warmLights, k); P.hudDark = lerpf(P.hudDark, T.hudDark, k);
        P.sunAz = T.sunAz; P.sunEl = T.sunEl;
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
        if (!game.over()) {
            overT_ = -1.f;
            flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& fl) { return fl.kind == 1; }), flights_.end());
        } else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
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
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    bool sectionKnown = false, sectionDrop = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown = true;
            sectionDrop = h.text == "DROP";
        }
    const bool isDrop = dropMode_ && (sectionKnown ? sectionDrop : !music.hasSong || music.speed > 1.26f);
    dropT_ += dt;
    quadT_ += dt;
    clearT_ += dt;
    // the drop's level eases in over half a second (never a jump), out over a few seconds
    dropAmt_ = std::clamp(dropAmt_ + (isDrop ? dt / 0.5f : -dt / 3.f), 0.f, 1.f);
    const float drop = smoothstepf(0.f, 1.f, dropAmt_);
    // the swell of the sun breaking through: up over half a second, settling over seconds
    if (!isDrop && dropT_ < 0.1f) swellPeak_ = 0.f;
    const float swellIn = smoothstepf(0.f, 0.55f, dropT_);
    swellPeak_ = approach(swellPeak_, 0.f, 0.3f, dt);
    swell_ = isDrop || swell_ > 0.f ? swellPeak_ * swellIn : 0.f;
    refreshK_ = approach(refreshK_, 0.f, 0.5f, dt);
    if (isDrop && dropT_ < dt * 1.5f + 0.25f && dropT_ >= 0.25f - dt * 0.5f) {
        // the dust shaken out of the slot, a quarter of a second in
        for (int i = 0; i < 6; i++) puff(vec3(rng_.range(-4.5f, 4.5f), rng_.range(-9.f, 9.f), 0.1f), 10, 2.f, 1.4f, 1.1f);
    }
    const float half = beat_ * 0.5f;
    const int halfI = (int)std::floor(half);
    const float ph = half - std::floor(half);
    const bool newHalf = halfI != lastHalf_;
    lastHalf_ = halfI;
    float kick = kickIn_;
    kickIn_ = 0.f;
    const float slowE = smoothstepf(0.f, 1.f, slow_);

    // ---- The sun along its arc (slow motion: the evening, low and warm).
    sunAz_ = approach(sunAz_, sunAzTo_, 0.28f, wdt);
    sunEl_ = approach(sunEl_, sunElTo_, 0.28f, wdt);
    const float el = lerpf(sunEl_, std::min(sunEl_, 0.13f), slowE);
    float az = lerpf(sunAz_, sunAz_ * 0.6f + (sunAz_ < 0.f ? -0.55f : 0.55f), slowE);
    {
        // never so far round that the stack's faces lose the sun
        const float lim = std::acos(std::min(1.f, 0.3f / std::cos(el)));
        az = std::clamp(az, -lim, lim);
    }
    const vec3 sunDir = normalize(vec3(std::sin(az) * std::cos(el), std::sin(el), std::cos(az) * std::cos(el)));

    // ---- The growing building: the crane takes the next module from its foot, lifts it, slews, lowers it in place.
    for (Module& m : plan_) {
        if (m.t >= 1.f) m.landed += wdt;
    }
    {
        Crane& c = crane_;
        const float pace = (0.6f + 0.25f * wm + 0.35f * wp + 0.3f * drop) * (1.f + 0.35f * std::min(queued_, 4));
        const vec3 mast(CRANE_X, 0.f, CRANE_Z);
        const float hookTop = JIB_Y - 2.5f;
        auto aim = [&](const vec3& p, float& jib, float& u) {
            const float dx = mast.x - p.x, dz = mast.z - p.z;
            u = std::sqrt(dx * dx + dz * dz);
            jib = std::atan2(dz, dx);
        };
        c.wait += wdt * pace;
        static const float DUR[6] = {1.f, 3.2f, 3.6f, 2.8f, 1.4f, 3.f};
        if (c.carrying < 0) {
            // idle: the hook parked high near the mast, the jib resting over the pile
            c.trolley = approach(c.trolley, 14.f, 0.4f, wdt);
            c.hook = approach(c.hook, hookTop, 0.5f, wdt);
            c.jib = approach(c.jib, 0.06f, 0.3f, wdt);
            if (queued_ > 0 && placed_ < (int)plan_.size() && c.wait > 1.f) {
                c.carrying = placed_++;
                queued_--;
                plan_[c.carrying].t = 0.f;
                c.tT = 0.f;
                c.tFrom = c.trolley;
                c.jibFrom = c.jib;
                c.hookFrom = c.hook;
                c.wait = 0.f;
            }
        } else {
            Module& m = plan_[c.carrying];
            // stages: 0 hook down to the module at the crane's foot, 1 hoist, 2 slew and travel, 3 lower, 4 release
            const int stage = std::min(4, (int)c.tT);
            float e = smoothstepf(0.f, 1.f, c.tT - stage);
            c.tT += wdt * pace / DUR[stage];
            float jibT, uT;
            aim(m.pos, jibT, uT);
            const float pickU = 9.f, pickJib = 0.9f;
            const float pickY = Y_P + MOD_H * 0.5f + 1.6f, topY = hookTop;
            const float slotY = m.pos.y + MOD_H * 0.5f + 1.6f;
            if (stage == 0) {
                // eased from where the idle crane was, so it meets the pick exactly as stage 1 starts
                c.jib = lerpf(c.jibFrom, pickJib, e);
                c.trolley = lerpf(c.tFrom, pickU, e);
                c.hook = lerpf(c.hookFrom, pickY, e);
            } else if (stage == 1) {
                c.jib = pickJib;
                c.trolley = pickU;
                c.hook = lerpf(pickY, topY, e);
            } else if (stage == 2) {
                c.jib = lerpf(pickJib, jibT, e);
                c.trolley = lerpf(pickU, uT, e);
                c.hook = topY;
            } else if (stage == 3) {
                c.jib = jibT;
                c.trolley = uT;
                const float ee = 1.f - (1.f - e) * (1.f - e);
                c.hook = lerpf(topY, slotY, ee);
                if (c.tT >= 4.f) {
                    m.t = 1.f;
                    m.landed = 0.f;
                }
            } else {
                c.hook = lerpf(slotY, topY, e);
                if (c.tT >= 5.f) {
                    c.carrying = -1;
                    c.wait = 0.f;
                    c.tFrom = c.trolley;
                }
            }
            // (not once it has landed: that would hang it back on the hook, rising with it, then pop it into its slot)
            if (c.carrying >= 0 && c.tT < 4.f) m.t = std::min(0.99f, c.tT / 4.f);
        }
    }

    // ---- The cleared blocks leaving, the stack falling at game over.
    for (Flight& fl : flights_) {
        if (fl.kind == 0) {
            fl.t += dt;
            const float tt = fl.t - fl.delay;
            if (tt <= 0.f) continue;
            // popped out of the slot, tumbling down onto the sill or the plaza, bouncing, then shrinking away
            const float rest = (fl.cur.z < 1.3f && std::fabs(fl.cur.x) < X_PIER ? Y_SILL : Y_G) + CELL_H * 0.7f;
            if (fl.cur.y > rest || fl.vel.y > 0.f) {
                fl.vel.y -= 24.f * dt;
                fl.cur = fl.cur + fl.vel * dt;
                if (fl.cur.y <= rest) {
                    fl.cur.y = rest;
                    if (fl.vel.y < -4.f) puff(fl.cur + vec3(0.f, -CELL_H * 0.5f, 0.f), 3, 1.f, 0.7f, 0.8f);
                    fl.vel = vec3(fl.vel.x * 0.7f, -fl.vel.y * 0.3f, fl.vel.z * 0.7f);
                    fl.spinK *= 0.6f;
                    if (fl.vel.y < 1.2f) fl.vel.y = 0.f;
                }
            } else {
                // sliding to a stop, then gone
                fl.vel = fl.vel * std::exp(-4.f * dt);
                fl.cur = fl.cur + vec3(fl.vel.x, 0.f, fl.vel.z) * dt;
                fl.spinK *= std::exp(-6.f * dt);
                fl.rest = fl.rest < 0.f ? 0.f : fl.rest + dt;
            }
            fl.ang += dt * fl.spinK;
            if (fl.rest > 0.6f) fl.scale = std::max(0.f, 1.f - (fl.rest - 0.6f) / 0.45f);
            if (tt > 4.f) fl.scale = std::max(0.f, fl.scale - dt * 3.f);
        } else {
            fl.t += dt;
            const float tt = fl.t - fl.delay;
            if (tt <= 0.f) continue;
            // out of the slot and down onto the plaza; it lands, tips over and stays
            // on the sill if it has not cleared it, else on the plaza
            const float rest = (fl.cur.z < 1.3f && std::fabs(fl.cur.x) < X_PIER ? Y_SILL : Y_G) + CELL_H * 0.7f;
            if (fl.cur.y > rest || fl.vel.y > 0.f) {
                fl.vel.y -= 22.f * dt;
                fl.cur = fl.cur + fl.vel * dt;
                fl.ang += dt;
                if (fl.cur.y <= rest) {
                    fl.cur.y = rest;
                    if (fl.vel.y < -4.f) puff(fl.cur, 5, 1.2f, 0.8f, 0.9f);
                    fl.vel = vec3(fl.vel.x * 0.3f, -fl.vel.y * 0.15f, fl.vel.z * 0.3f);
                    if (std::fabs(fl.vel.y) < 0.6f) fl.vel = vec3(0.f);
                }
            }
        }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& fl) { return fl.kind == 0 && fl.scale <= 0.f; }),
                   flights_.end());
    if (!f.bare) gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The dust: the air's motes drift (and glitter on the grid at a peak); the puffs spread and settle.
    sprites_.clear();
    {
        const float airK = (0.04f + 0.12f * wm + 0.3f * wp + 0.5f * drop) * (1.f + 0.5f * slowE);
        const vec3 sc = pal_.sun * (1.f / 3.2f);
        if (newHalf && wp > 0.3f) {
            // a handful of motes catch the light on the two-beat grid
            for (int i = 0; i < 26; i++) {
                Mote& m = motes_[rng_.irange(0, 259)];
                m.k = 1.f + 3.f * (0.4f * wp + 0.6f * drop);
            }
        }
        if (kick > 0.3f && wp > 0.5f)
            for (int i = 0; i < 8; i++) motes_[rng_.irange(0, 259)].k = 2.5f;
        const float ps = std::max(1.f, height_ / 1080.f);
        for (size_t i = 0; i < motes_.size(); i++) {
            Mote& m = motes_[i];
            if (m.kind == 0) {
                const float t = worldTime_ * 0.2f + m.seed * 50.f;
                m.p = m.p + (m.v + vec3(0.12f * std::sin(t), 0.06f * std::sin(t * 1.3f + 1.f), 0.05f * std::cos(t * 0.7f))) * wdt;
                if (m.p.y > 13.f) m.p.y = Y_G;
                if (m.p.x > 22.f) m.p.x = -22.f;
                if (m.p.x < -22.f) m.p.x = 22.f;
                m.k = approach(m.k, 1.f, 3.f, dt);
                const float tw = 0.6f + 0.4f * std::sin(worldTime_ * (1.f + 2.f * m.seed) + m.seed * 40.f);
                // fewer right in front of the stack
                const float front = (std::fabs(m.p.x) < 5.5f && m.p.z > -0.5f) ? 0.35f : 1.f;
                const float k = airK * tw * m.k * front;
                if (k > 0.005f) sprites_.push_back({{m.p.x, m.p.y, m.p.z, m.size * 1.6f * ps * (0.7f + 0.3f * m.k)}, {sc.x, sc.y, sc.z, k * 0.9f}});
            } else {
                m.life += dt;
                m.v = m.v * std::exp(-dt * 2.2f) + vec3(0.f, 0.1f * dt, 0.f);
                m.p = m.p + m.v * dt;
                const float u = m.life / m.max;
                const float k = m.k * smoothstepf(0.f, 0.08f, u) * (1.f - smoothstepf(0.3f, 1.f, u)) * 0.5f;
                const vec3 dc = lerp(pal_.conc, sc, 0.5f);
                if (k > 0.005f) sprites_.push_back({{m.p.x, m.p.y, m.p.z, m.size * 2.2f * ps * (1.f + u)}, {dc.x, dc.y, dc.z, k}});
            }
        }
        motes_.erase(std::remove_if(motes_.begin() + 260, motes_.end(), [](const Mote& m) { return m.life >= m.max; }), motes_.end());
    }

    // ---- Geometry.
    boxes_.clear();
    cells_.clear();
    lines_.clear();
    cellLines_.clear();
    buildCrane(boxes_);
    for (size_t i = 0; i < plan_.size(); i++) {
        const Module& m = plan_[i];
        if (m.t < 0.f) continue;
        vec3 at = m.pos;
        if ((int)i == crane_.carrying && crane_.tT < 1.f) {
            // waiting at the crane's foot while the hook comes down: it rises out of the podium through a hatch
            // (it used to appear on the hook in the air)
            const float pickU = 9.f, pickJib = 0.9f;
            const float k = smoothstepf(0.f, 0.6f, crane_.tT);
            at = vec3(CRANE_X - pickU * std::cos(pickJib), Y_P - MOD_H * 1.02f * (1.f - k), CRANE_Z - pickU * std::sin(pickJib));
        } else if (m.t < 1.f && (int)i == crane_.carrying) {
            // hanging from the hook
            const float c = std::cos(crane_.jib), sn = std::sin(crane_.jib);
            const vec3 tr(CRANE_X - crane_.trolley * c, JIB_Y - 0.2f, CRANE_Z - crane_.trolley * sn);
            at = vec3(tr.x, crane_.hook - 1.6f - MOD_H * 0.5f, tr.z);
            // the slings and the hoist rope
            const vec3 rc(0.12f, 0.12f, 0.12f);
            line(lines_, tr, vec3(tr.x, crane_.hook + 0.3f, tr.z), 1.f, rc, 0.8f);
            for (int sx = -1; sx <= 1; sx += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    line(lines_, vec3(tr.x, crane_.hook - 0.5f, tr.z), at + vec3(sx * MOD_W * 0.45f, MOD_H * 0.5f, sz * MOD_D * 0.45f), 1.f, rc, 0.6f);
        }
        buildModule(boxes_, m, at, 1.f);
    }
    if (crane_.carrying < 0) {
        const float c = std::cos(crane_.jib), sn = std::sin(crane_.jib);
        const vec3 tr(CRANE_X - crane_.trolley * c, JIB_Y - 0.2f, CRANE_Z - crane_.trolley * sn);
        line(lines_, tr, vec3(tr.x, crane_.hook + 0.3f, tr.z), 1.f, vec3(0.12f), 0.8f);
    } else {
        const float c = std::cos(crane_.jib), sn = std::sin(crane_.jib);
        const vec3 tr(CRANE_X - crane_.trolley * c, JIB_Y - 0.2f, CRANE_Z - crane_.trolley * sn);
        if (plan_[crane_.carrying].t >= 1.f) line(lines_, tr, vec3(tr.x, crane_.hook + 0.3f, tr.z), 1.f, vec3(0.12f), 0.8f);
    }
    // the jib's ties
    {
        const float c = std::cos(crane_.jib), sn = std::sin(crane_.jib);
        const vec3 top(CRANE_X, CRANE_TOP, CRANE_Z);
        for (float u : {-12.f, 20.f, 36.f}) line(lines_, top, vec3(CRANE_X - u * c, JIB_Y + 1.3f, CRANE_Z - u * sn), 1.f, pal_.steel * 0.6f, 0.7f);
    }
    // the cleared blocks and the fallen ones (none in a break)
    for (const Flight& fl : flights_) {
        if ((fl.t < fl.delay && fl.kind == 1) || bare_) continue;
        const vec3 col = pieceColor(fl.type);
        {
            const float a = fl.ang * length(fl.spin), sc = fl.kind == 0 ? fl.scale : 1.f;
            const vec3 ax = normalize(fl.spin + vec3(1e-4f, 0, 0));
            // a rotation about ax by a (Rodrigues) for the three half axes
            auto rot = [&](const vec3& v) {
                const float c = std::cos(a), s = std::sin(a);
                return v * c + cross(ax, v) * s + ax * (dot(ax, v) * (1.f - c));
            };
            obox(boxes_, fl.cur, rot(vec3(CELL_H * sc, 0, 0)), rot(vec3(0, CELL_H * sc, 0)), rot(vec3(0, 0, CELL_D * sc)), col, M_BLOCK, 0.f, 0.f, (float)fl.type,
                 fl.seed);
        }
    }
    // (at game over, the stack is the falling blocks)
    if (overT_ < 0.05f && !bare_) buildBoard(game);

    // ---- The sun's shadow map: an orthographic view along the sun over the place's extents.
    {
        const vec3 center(0.f, 4.f, -40.f);
        const vec3 up = std::fabs(sunDir.y) > 0.95f ? vec3(0, 0, 1) : vec3(0, 1, 0);
        const mat4 view = mat4::lookAt(center + sunDir * 200.f, center, up);
        float mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};
        for (int i = 0; i < 8; i++) {
            const vec4 p(i & 1 ? 72.f : -72.f, i & 2 ? 34.f : Y_G - 1.f, i & 4 ? 16.f : -90.f, 1.f);
            const vec4 q = view * p;
            const float v[3] = {q.x, q.y, q.z};
            for (int k = 0; k < 3; k++) { mn[k] = std::min(mn[k], v[k]); mx[k] = std::max(mx[k], v[k]); }
        }
        sunVp_ = mat4::ortho(mn[0], mx[0], mn[1], mx[1], -mx[2] - 5.f, -mn[2] + 5.f) * view;
    }
    if (staticDirty_) {
        // nearest first, so that the depth test rejects most of what is hidden before it is shaded; the monument
        // last (a break draws the place without it)
        std::vector<size_t> idx(static_.size());
        for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
        auto dist2 = [&](const Box& b) {
            const float dx = b.c[0] - eye_.x, dy = b.c[1] - eye_.y, dz = b.c[2] - eye_.z;
            return dx * dx + dy * dy + dz * dz;
        };
        auto isMon = [&](size_t i) { return i >= monument0_ && i < monument1_; };
        std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
            if (isMon(a) != isMon(b)) return isMon(b);
            return dist2(static_[a]) < dist2(static_[b]);
        });
        std::vector<Box> sorted;
        sorted.reserve(idx.size());
        for (size_t i : idx) sorted.push_back(static_[i]);
        worldCount_ = static_.size() - (monument1_ - monument0_);
        glBindBuffer(GL_ARRAY_BUFFER, staticVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sorted.size() * sizeof(Box)), sorted.data(), GL_STATIC_DRAW);
        staticCount_ = sorted.size();
        staticDirty_ = false;
    }
    // (uploaded once: the shadow pass and the main pass share them)
    for (int i = 0; i < 2; i++) {
        const std::vector<Box>& v = i ? cells_ : boxes_;
        glBindBuffer(GL_ARRAY_BUFFER, i ? cellVbo_ : boxVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(Box)), v.data(), GL_STREAM_DRAW);
    }
    {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glViewport(0, 0, SHADOW_N, SHADOW_N);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 3.f);
        glUseProgram(progDepth_);
        setMat(progDepth_, "uVP", sunVp_);
        drawBoxes(staticVao_, staticVbo_, nullptr, bare_ ? worldCount_ : staticCount_, false);
        drawBoxes(boxVao_, boxVbo_, nullptr, boxes_.size(), false);
        drawBoxes(cellVao_, cellVbo_, nullptr, cells_.size(), false);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // ---- Uniforms shared by the passes.
    const Palette& P = pal_;
    // the light by level: a veiled sun when calm, out at mid, hard at a peak; the drop's swell on top
    const float veil = std::max(P.veil, saturate(1.f - 0.45f * wm - 0.3f * wp - 0.25f * drop)) * (1.f - 0.6f * slowE);
    const float sunK = (0.33f + 0.3f * wm + 0.22f * wp + 0.15f * drop + 0.3f * swell_) * (1.f + 0.3f * slowE);
    const vec3 warm = vec3(1.f, 0.62f, 0.34f);
    const vec3 sunCol = lerp(lerp(P.sun, P.sun * vec3(1.06f, 0.98f, 0.88f), drop), P.sun * warm * 1.1f, slowE) * sunK;
    const float ambK = (1.f + 0.6f * veil) * (1.f - 0.25f * slowE);
    const vec3 ambSky = lerp(P.sky, P.sky * vec3(0.8f, 0.75f, 1.f), slowE) * ambK, ambGround = P.ground * ambK * lerpf(1.f, 0.7f, slowE);
    const float soft = lerpf(1.f, 7.f, veil) * (1.f - 0.3f * drop);
    const float sat = 0.9f + 0.06f * wm + 0.04f * wp + 0.1f * drop;
    const vec3 zen = lerp(P.zen, P.zen * vec3(0.55f, 0.5f, 0.75f), slowE * 0.7f);
    const vec3 hor = lerp(P.hor, lerp(P.hor, vec3(1.f, 0.55f, 0.35f), 0.5f) * 0.9f, slowE * 0.8f);
    const vec3 glow = lerp(P.glow, vec3(1.f, 0.5f, 0.22f) * 1.2f, slowE) * (0.9f + 0.3f * swell_);
    const float winOn = saturate(P.warmLights * (0.45f + 0.25f * wm + 0.3f * drop) + 0.75f * slowE);
    const float glint = 1.f + 1.2f * drop * smoothstepf(0.f, 0.6f, dropT_);
    // a quad: the sun flashes on all the glazing at once, eased in, fading over a second or two
    const float flash = quadT_ < 4.f ? smoothstepf(0.f, 0.3f, quadT_) * std::exp(-std::max(0.f, quadT_ - 0.3f) * 1.5f) : 0.f;
    auto common = [&](GLuint p) {
        set3f(p, "uEye", eye_);
        set3f(p, "uZen", zen);
        set3f(p, "uHor", hor);
        set3f(p, "uGlow", glow * 0.5f);
        set3f(p, "uSunDir", sunDir);
        set3f(p, "uSunTint", sunCol);
        set1f(p, "uFogD", P.haze * (1.f + 0.25f * veil));
        set1f(p, "uTime", worldTime_);
        set1f(p, "uSat", sat);
        set1f(p, "uVeil", veil);
    };
    auto shadowing = [&](GLuint p) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        set1i(p, "uShadow", 1);
        setMat(p, "uSunVP", sunVp_);
        set1f(p, "uSoft", soft);
        set1f(p, "uTexel", 1.f / SHADOW_N);
        glActiveTexture(GL_TEXTURE0);
    };

    // ---- The scene (multisampled).
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(progBox_);
    common(progBox_);
    shadowing(progBox_);
    setMat(progBox_, "uVP", vp_);
    set3f(progBox_, "uSunCol", sunCol);
    set3f(progBox_, "uAmbSky", ambSky);
    set3f(progBox_, "uAmbGround", ambGround);
    set3f(progBox_, "uWinCol", P.win);
    // (more of it when the sun is low or veiled, or by night: the stack keeps its contrast with the lining)
    set3f(progBox_, "uFill", (ambSky + ambGround) * 0.55f + vec3(0.1f) * (1.f - P.hudDark) + P.sun * 0.05f * (1.f - std::max(0.f, sunDir.z)));
    set1f(progBox_, "uSnow", P.snow);
    set3f(progBox_, "uFlood", lin(0xffc488) * (P.warmLights * (1.f - P.hudDark) * (0.25f + 0.15f * wm + 0.1f * wp + 0.7f * drop + 0.4f * swell_)));
    set1f(progBox_, "uWinOn", winOn);
    set1f(progBox_, "uGlint", glint);
    set1f(progBox_, "uFlash", flash);
    set1f(progBox_, "uStackA", 0.f);
    drawBoxes(staticVao_, staticVbo_, nullptr, bare_ ? worldCount_ : staticCount_, false);
    drawBoxes(boxVao_, boxVbo_, nullptr, boxes_.size(), false);
    // the stack (the scene's alpha is the stack's, for the halo and the bridge)
    set1f(progBox_, "uStackA", 1.f);
    drawBoxes(cellVao_, cellVbo_, nullptr, cells_.size(), false);
    // the sky, only where nothing stands (at the far plane, behind the depth test)
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(1.0, 1.0);
    glUseProgram(progSky_);
    common(progSky_);
    setMat(progSky_, "uInvVP", invVp_);
    set1f(progSky_, "uStars", P.stars * (1.f - 0.5f * slowE));
    set1f(progSky_, "uSeed", seedF_);
    set3f(progSky_, "uDisc", lerp(P.sun, P.sun * warm, slowE) * (P.stars > 0.5f ? 1.5f : 6.f));
    ugl::drawFullscreen();
    glDepthRange(0.0, 1.0);
    glDepthFunc(GL_LESS);
    // the dust (additive), the cables, the ghost
    glEnable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBlendFunc(GL_ONE, GL_ONE);
    if (!sprites_.empty()) {
        glUseProgram(progSprite_);
        common(progSprite_);
        setMat(progSprite_, "uVP", vp_);
        setMat(progSprite_, "uSunVP", sunVp_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowTex_);
        set1i(progSprite_, "uShadow", 1);
        glActiveTexture(GL_TEXTURE0);
        set2f(progSprite_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(spriteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sprites_.size() * sizeof(Sprite)), sprites_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    auto drawLineList = [&](const std::vector<Line>& L) {
        if (L.empty()) return;
        glUseProgram(progLine_);
        common(progLine_);
        setMat(progLine_, "uVP", vp_);
        set2f(progLine_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(L.size() * sizeof(Line)), L.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)L.size());
    };
    drawLineList(lines_);
    glDisable(GL_DEPTH_TEST);
    drawLineList(cellLines_);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // ---- Resolve, the glow, the frame.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneT_.fbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThr", 1.1f);
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
    const bool day = P.hudDark > 0.5f;
    set1f(progFinal_, "uBloomK", 0.5f + 0.2f * swell_);
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uHalo", 0.35f);
    set3f(progFinal_, "uHaloCol", day ? P.lining * 0.6f : P.zen * 0.4f);
    set1f(progFinal_, "uSatF", 0.92f + 0.05f * wm + 0.03f * wp + 0.1f * drop);
    set1f(progFinal_, "uContrast", 1.1f);
    set1f(progFinal_, "uSlow", slowE * 0.35f);
    set3f(progFinal_, "uSlowTint", vec3(1.08f, 0.92f, 0.8f));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.panels);
    glDisable(GL_BLEND);
    (void)ph;
    (void)refreshK_;
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_brutalist() {
    static const UniverseEntry e{
        "BRUTALIST", "raw concrete under a hard sun in 3D: the pieces are cast blocks, cleared rows grow a building of modules",
        [] { return std::unique_ptr<Universe>(new BrutalistUniverse()); },
        // A monumental, rhythmic place: suits steady, driving songs best; fits most.
        [](const SongLook& l) { return 0.75f + 0.2f * l.bassWeight + 0.15f * (1.f - l.airWeight); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

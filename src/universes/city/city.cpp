// CITY: a tower's facade in a 3D night city, its windows the board.
//
// One world, in 3D: we look across at a tall modern tower from about thirty floors up, through a shift lens (the
// camera looks straight ahead and its image is shifted so that the board plane keeps the layout's pixels): the
// tower's facade faces the camera and fills the well, its 10 x 22 windows are the board (two floors above the well, where the pieces spawn). Each window is a recessed
// opening (its reveals seen in perspective), glass reflecting the night, and behind it a real room traced per pixel
// (back wall, side walls, floor, ceiling): each window is its own room, furnished its own way, each floor its own
// use (offices with strip lights and screens, flats with a lamp and a sofa, hotel rooms), with its own blinds,
// drapes, roller blind or sheer. A block is a lit window: its room lit by the piece's light (the seven pieces told
// apart by their light's color only); an empty cell is a dark room behind dark glass. The falling piece is four
// rooms lighting up as it moves, brighter, settling softly when it lands; the ghost four window frames lit in its
// color. Lights only change smoothly (short eased blends); only a cleared floor goes dark quickly.
//
// Around it the city at night, in perspective: blocks of towers on a street grid far below (glass or stone, their
// faces lit by a dim moon, the sky and the streets' glow from below, the glass reflecting the sky), their small
// windows lit and put out slowly; the streets with their lamps and traffic; light pollution on the horizon, a deck
// of clouds lit from below, haze; red aircraft beacons, a crane; bloom on every light.
//
// Staging: a break is the city asleep (few windows, thick haze, the beacons blinking on their own); mid wakes it
// (more windows, the traffic, the beacons on the beat grid every two beats); a chorus lights the crowns and the
// tower's piers, two searchlights sweeping slowly in the haze; a drop switches the city at once: the near towers'
// windows turn to the drop's two colors in patterns and level meters (changing every two bars), the others in the
// drop's colors, four colored searchlights, the beacons with the kicks. Nothing pulses the whole scene.
//
// Events: a lock leaves the piece lit (it settles from the falling brightness) with a soft glow along the ledge
// under it; a cleared floor's rooms go dark (nothing drawn over the row), then the floors above step down one by one
// in smooth crossfades; a quad sends fireworks up over the city and a chase of light up the piers; slow motion is
// a long exposure (a colder night, cold city lights, the bloom wider); game over is a blackout floor by floor from
// the top, the city going dark with it.
#include "universes/city/city.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/city/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Barlow, SIL Open Font License, see OFL-Barlow.txt.
const unsigned char FONT_BARLOW[] = {
#include "universes/city/Barlow-Medium.ttf.inc"
};

namespace shaders = city_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec2 v2(float x, float y) { return vec2{x, y}; }
vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

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

// The variants: the sky (top, horizon, the city's light pollution, fog, the clouds lit from below), the night's
// light (ambient, moon, the streets' glow from below), the streets' sodium, the city's window light, the tower
// (frame, glass tint), the buildings' albedo (near, far), the accent, the HUD's text and halo, what the glass
// reflects of the city, the seven pieces' lamps (I fluorescent, O tungsten, T a red lantern, S a TV, Z halogen, J sodium,
// L an aquarium; one family tilted to each night, O, J and Z parted by value and saturation), fog density, the facade
// (mullions, spandrels, depth of the windows), reflectivity, stars, clouds, rain, snow, glow, exposure.
struct VariantDef {
    const char* name;
    unsigned top, hor, glow, fog, cloud, amb, moon, upl, street, city, frame, tint, nearB, farB, accent, text, shade, reflCity;
    unsigned piece[7];
    float fogD, mull, spand, recess, reflK, stars, clouds, rain, snow, glowK, exposure;
};
const VariantDef VARIANTS[] = {
    // a glass tower after the blue hour: the last deep blue in the sky, cold glass
    {"BLUE HOUR", 0x02050e, 0x0d1a33, 0x382a26, 0x0b1426, 0x1a1e30, 0x060a14, 0x0c1426, 0x2a1e18, 0xff9a40, 0xffd2a0,
     0x161a22, 0xc8d8ff, 0x10141c, 0x0c1220, 0x8ab8f0, 0xe4ecf6, 0x02040a, 0xffd8a8,
     {0xaff1d2, 0xfba041, 0xd43c3d, 0x6da8ed, 0xfbe3b4, 0xbd600a, 0x4ec6bc},
     0.0045f, 0.1f, 0.22f, 0.2f, 0.7f, 0.25f, 0.f, 0.f, 0.f, 0.5f, 1.f},
    // downtown at midnight: bronze frames, a sodium haze, warm rooms
    {"MIDNIGHT", 0x010204, 0x0c0a0e, 0x40220e, 0x0e0a08, 0x2a1a10, 0x040406, 0x06070c, 0x3a2410, 0xffa040, 0xffc070,
     0x1c1610, 0xffe8c8, 0x0e0c0c, 0x0c0a0a, 0xffb45a, 0xf0e4d4, 0x020100, 0xffc080,
     {0xb0f3c0, 0xff991a, 0xe1342f, 0x6ba8e0, 0xffdf9c, 0xc75d00, 0x40c8ae},
     0.005f, 0.12f, 0.26f, 0.35f, 0.5f, 0.35f, 0.5f, 0.f, 0.f, 0.6f, 1.f},
    // a rainy night: a low sodium haze, wet glass, the streets shining
    {"RAIN", 0x020407, 0x0e1116, 0x34220e, 0x101318, 0x221c16, 0x05070a, 0x07090d, 0x2c1c0e, 0xff9a40, 0xffc890,
     0x101216, 0xe0e6ee, 0x0c0e12, 0x0e1014, 0x6fd0c4, 0xeef0f0, 0x020304, 0xffc890,
     {0xb3f2cb, 0xff9f3f, 0xd93c3b, 0x70a8e5, 0xffe3ad, 0xc1600a, 0x4fc7b5},
     0.008f, 0.08f, 0.2f, 0.1f, 0.9f, 0.f, 0.8f, 1.f, 0.f, 0.7f, 1.f},
    // fog before dawn: the far towers lost, the lights blooming in the fog
    {"FOG DAWN", 0x0a0e16, 0x1c222c, 0x30241e, 0x161a20, 0x1a1c22, 0x0a0c12, 0x0c1018, 0x201812, 0xffb060, 0xffd090,
     0x1a1e24, 0xd8e0ea, 0x12161c, 0x141820, 0xffa070, 0xe2e6ea, 0x04060a, 0xffd0a0,
     {0xb8f0d2, 0xf6a35a, 0xce4646, 0x76a7e3, 0xfbe4b9, 0xb8632d, 0x62c5b9},
     0.02f, 0.1f, 0.22f, 0.2f, 0.6f, 0.f, 0.3f, 0.f, 0.f, 0.4f, 1.1f},
    // a snowy evening: brick, snow falling and lying on the ledges and roofs
    {"SNOWFALL", 0x03050c, 0x161a2c, 0x382a30, 0x141828, 0x22242e, 0x080a12, 0x10141e, 0x2a2020, 0xffa050, 0xffc47c,
     0x221a1a, 0xffffff, 0x101218, 0x12141c, 0xbfdcff, 0xeef2ff, 0x020308, 0xffc890,
     {0xadf0d9, 0xf1a04e, 0xcb4043, 0x6ea7f1, 0xf3e2bc, 0xb5611d, 0x54c5c1},
     0.007f, 0.14f, 0.26f, 0.45f, 0.4f, 0.f, 0.6f, 0.f, 1.f, 0.5f, 1.05f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// A drop's two colors, from the night's own lamps: 0 tungsten (O), 1 sodium (J), 2 halogen (Z), 3 fluorescent (I),
// 4 the night's accent lamp (one of the cooler or redder pieces), a new pair each drop.
const int DROP_LAMPS[5] = {1, 5, 4, 0, -1};
const int ACCENT_PIECE[6] = {3, 2, 6, 3, 2, 3}; // per variant: S blue, T red, L teal
const int PAIRS[][2] = {{0, 4}, {1, 2}, {2, 4}, {0, 2}, {1, 4}, {2, 0}, {0, 3}, {1, 0}}; // the first always warm
constexpr int PAIR_COUNT = (int)(sizeof(PAIRS) / sizeof(PAIRS[0]));
constexpr float GROUND_Y = -30.f, EYE_Y = 1.f, PITCH = 22.f;
const vec3 BEACON_RED{1.f, 0.06f, 0.02f};
const vec3 MOON_DIR = vec3(-0.45f, 0.55f, 0.7f) * (1.f / std::sqrt(0.45f * 0.45f + 0.55f * 0.55f + 0.7f * 0.7f));

float hashf(float x) {
    const float s = std::sin(x * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool CityUniverse::init(int width, int height) {
    progSky_ = compile(shaders::FULL_VS, shaders::SKY_FS, "city sky");
    progGround_ = compile(shaders::GROUND_VS, shaders::GROUND_FS, "city ground");
    progBox_ = compile(shaders::BOX_VS, shaders::BOX_FS, "city boxes");
    progTower_ = compile(shaders::TOWER_VS, shaders::TOWER_FS, "city tower");
    progWin_ = compile(shaders::RECT_VS, shaders::WIN_FS, "city windows");
    progDot_ = compile(shaders::DOT_VS, shaders::DOT_FS, "city dots");
    progBright_ = compile(shaders::FULL_VS, shaders::BRIGHT_FS, "city bright");
    progBlur_ = compile(shaders::FULL_VS, shaders::BLUR_FS, "city blur");
    progFinal_ = compile(shaders::FULL_VS, shaders::FINAL_FS, "city final");
    progPanel_ = compile(shaders::PANEL_VS, shaders::PANEL_FS, "city panel");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    // a plain quad (the ground, the tower)
    glGenVertexArrays(1, &quadVao_);
    glBindVertexArray(quadVao_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    // instanced quads (previews, dots)
    glGenVertexArrays(1, &instVao_);
    glGenBuffers(1, &instVbo_);
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    // instanced boxes (the buildings): a unit cube with its face normals
    {
        std::vector<float> v;
        const int F[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (auto& f : F) {
            const vec3 n((float)f[0], (float)f[1], (float)f[2]);
            const vec3 a = std::fabs(n.y) > 0.5f ? vec3(1.f, 0.f, 0.f) : vec3(0.f, 1.f, 0.f);
            const vec3 b = cross(n, a);
            const vec3 c[4] = {n - a - b, n + a - b, n + a + b, n - a + b};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int i : idx) {
                v.insert(v.end(), {c[i].x, c[i].y, c[i].z, n.x, n.y, n.z});
            }
        }
        glGenVertexArrays(1, &cubeVao_);
        glGenBuffers(1, &cubeVbo_);
        glGenBuffers(1, &cubeInstVbo_);
        glBindVertexArray(cubeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, cubeInstVbo_);
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(2 + i, 1);
        }
    }
    glGenVertexArrays(1, &emptyVao_);
    glGenVertexArrays(1, &panelVao_);
    glGenBuffers(1, &panelVbo_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    GLuint* texs[2] = {&cellA_, &cellB_};
    for (int i = 0; i < 2; i++) {
        glGenTextures(1, texs[i]);
        glBindTexture(GL_TEXTURE_2D, *texs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, COLS, ROWS, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            pendingOff_[r][c] = 0;
            pendingDelay_[r][c] = 0.f;
        }
    for (int& o : off_) o = 0;
    Rng fr(99);
    for (int i = 0; i < 360; i++)
        flakes_.push_back({vec3(fr.range(-60.f, 60.f), fr.range(-30.f, 45.f), fr.range(-110.f, -18.f)), fr.range(0.6f, 1.4f), fr.range(0.f, TAU)});
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

// The layout's reference camera raised to EYE_Y and looking straight ahead, its image shifted back down: the board
// plane lands exactly on the reference camera's pixels.
void CityUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const layout::Camera ref = layout::camera(width_, height_);
    const float aspect = (float)width_ / height_;
    const float half = ref.fovDeg * 0.5f * PI / 180.f;
    const float f = 1.f / std::tan(half);
    eye_ = vec3(ref.target.x, EYE_Y, ref.target.z + ref.dist);
    const mat4 view = mat4::lookAt(eye_, eye_ + vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f));
    mat4 shift = mat4::identity();
    shift.at(1, 3) = f * (EYE_Y - ref.target.y) / ref.dist;
    vp_ = shift * mat4::perspective(2.f * half, aspect, 1.f, 3000.f) * view;
    invVP_ = inverse(vp_);

    if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
    if (sceneTex_) glDeleteTextures(1, &sceneTex_);
    if (depthTex_) glDeleteTextures(1, &depthTex_);
    glGenFramebuffers(1, &sceneFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glGenTextures(1, &sceneTex_);
    glBindTexture(GL_TEXTURE_2D, sceneTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex_, 0);
    glGenTextures(1, &depthTex_);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const int bw = std::max(1, width_ / 4), bh = std::max(1, height_ / 4);
    for (ugl::Target* t : {&bloomA_, &bloomB_}) {
        t->destroy();
        t->create(bw, bh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    }
    for (ugl::Target* t : {&bloomC_, &bloomD_}) {
        t->destroy();
        t->create(std::max(1, bw / 4), std::max(1, bh / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    }
}

CityUniverse::~CityUniverse() {
    for (ugl::Target* t : {&bloomA_, &bloomB_, &bloomC_, &bloomD_}) t->destroy();
    if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
    for (GLuint t : {sceneTex_, depthTex_, cellA_, cellB_}) glDeleteTextures(1, &t);
    for (GLuint p : {progSky_, progGround_, progBox_, progTower_, progWin_, progDot_, progBright_, progBlur_, progFinal_,
                     progPanel_})
        glDeleteProgram(p);
    for (GLuint v : {instVao_, cubeVao_, quadVao_, panelVao_, emptyVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, instVbo_, cubeVbo_, cubeInstVbo_, panelVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void CityUniverse::setSong(const SongLook& t, float seconds) {
    if (t.variant >= 0) variant_ = t.variant % VARIANT_COUNT;
    else {
        // night songs downtown or in the rain, bright ones after the blue hour or in the dawn fog, slow quiet ones
        // in the snow
        int v;
        if (t.bpm < 100.f && t.density < 0.5f) v = 4;
        else if (t.minor && t.brightness < 0.55f) v = (t.bpm > 118.f || t.airWeight > 0.55f) ? 2 : 1;
        else v = (t.brightness > 0.6f && t.dynamics < 0.5f) ? 3 : 0;
        if ((t.seed % 5) == 0) v = (v + 1 + (int)((t.seed >> 8) % 4)) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 4)) % VARIANT_COUNT;
        variant_ = v;
    }
    rng_ = Rng(t.seed ^ 0xC17Full);
    songSeed_ = t.seed;
    dropIndex_ = 0;
    pickDropColors();
    if (seconds <= 0.f) palSet_ = false;
}

void CityUniverse::setLevel(int level, float, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    if (level < 2) inDrop_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        dropWait_ = 0.f;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    if (rising && level >= 2) dropWait_ = 1.f; // the peak is announced just before its downbeat: wait for drop()
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 0.8f + 0.8f * (1.f - impact) : 2.5f, 6.f);
}

void CityUniverse::drop(float strength) {
    if (level_ >= 2) inDrop_ = true;
    dropWait_ = 0.f;
    dropT_ = 0.f;
    dropAmp_ = 0.6f + 0.4f * saturate(strength);
    if (level_ >= 2) {
        pickDropColors();
        drop_ = std::max(drop_, 0.85f); // the city switches at once, on the downbeat
    }
}

void CityUniverse::refresh(float strength) {
    // a new section: the city's windows are put out and lit again elsewhere (the same number of them)
    if (strength > 0.2f) cityShift_ += 97.f;
}

void CityUniverse::forceFx(int slow, int quadClear) {
    (void)slow;
    if (quadClear >= 0) {
        quadT_ = 0.f;
        spawnFireworks();
    }
}

void CityUniverse::pickDropColors() {
    Rng r(songSeed_ ^ (0x9E3779B97F4A7C15ull * (uint64_t)(dropIndex_ + 1)) ^ (uint64_t)variant_);
    int h = r.irange(0, PAIR_COUNT - 1);
    if (h == lastPair_) h = (h + 1 + r.irange(0, PAIR_COUNT - 3)) % PAIR_COUNT;
    lastPair_ = h;
    const VariantDef& d = VARIANTS[variant_];
    auto lamp = [&](int i) {
        const int p = DROP_LAMPS[i] < 0 ? ACCENT_PIECE[variant_ % 6] : DROP_LAMPS[i];
        vec3 c = lin(d.piece[p]);
        const float m = std::max(c.x, std::max(c.y, c.z));
        return c * (1.15f / std::max(m, 1e-3f)); // brighter than the rooms, the lamp's own hue
    };
    dropA_ = lamp(PAIRS[h][0]); // the warm one: the glow, the crowns
    dropB_ = lamp(PAIRS[h][1]);
    dropIndex_++;
}

vec2 CityUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints CityUniverse::hints() const {
    const VariantDef& d = VARIANTS[variant_];
    TransitionHints h;
    h.light = false;
    h.background = lin(d.hor) * 0.6f;
    h.ink = lin(d.text) * 0.8f;
    h.accent = lin(d.accent);
    h.styles = TS_TILES | TS_LUMA | TS_REBUILD;
    return h;
}

// =====================================================================================================
// The city
// =====================================================================================================

void CityUniverse::buildCity(int variant) {
    bldgs_.clear();
    Rng r(0xC1C1ull + (uint64_t)variant * 7919ull);
    const VariantDef& d = VARIANTS[variant];
    const float dist = eye_.z;
    auto add = [&](float x0, float x1, float z0, float z1, float h, int style, float crown, bool beacon, bool patterned) {
        Bldg b;
        b.cx = (x0 + x1) * 0.5f; b.cz = (z0 + z1) * 0.5f;
        b.hw = (x1 - x0) * 0.5f; b.hd = (z1 - z0) * 0.5f;
        b.y0 = GROUND_Y; b.y1 = GROUND_Y + h;
        b.style = style;
        b.seed = r.uniform();
        b.lit = r.range(0.6f, 1.25f);
        b.crown = crown;
        b.beacon = beacon;
        b.patterned = patterned;
        const vec3 base = lin(std::fabs(b.cz) < 60.f ? d.nearB : d.farB);
        b.alb = style == 1 ? base * r.range(1.6f, 2.6f) : base * r.range(0.8f, 1.3f);
        b.refl = style == 0 ? r.range(0.6f, 1.f) : 0.3f;
        bldgs_.push_back(b);
    };
    // the blocks between the streets, from near to far; the tower's own block left to it
    for (int m = -1; m >= -16; m--) {
        const float z1 = PITCH * m + PITCH, z0 = PITCH * m + 4.f; // (z0 < z1, both negative)
        const float reach = (dist - z0) * 0.75f + 20.f;
        const int kmax = (int)std::ceil(reach / PITCH);
        for (int k = -kmax; k <= kmax; k++) {
            const float x0 = PITCH * k - 7.f, x1 = PITCH * k + 11.f;
            if (m == -1 && k >= -1 && k <= 1) continue; // the tower's block, and the near corners (set below)
            if (m == -2 && k == 0) {
                // right behind the tower: low, so it never peeks over its crown
                add(x0 + 1.f, x1 - 1.f, z0 + 1.f, z1 - 1.f, r.range(10.f, 24.f), 1, 0.f, false, false);
                continue;
            }
            const float far = saturate((-z0 - 60.f) / 250.f);
            const int split = r.irange(0, 2);
            auto height = [&]() {
                float h = r.range(10.f, 30.f) + far * r.range(0.f, 25.f);
                if (far > 0.3f && r.chance(0.07f)) h = r.range(65.f, 105.f); // a landmark tower
                return h;
            };
            auto one = [&](float a0, float a1, float b0, float b1) {
                const float h = height();
                const int st = r.chance(0.55f) ? 0 : 1;
                const float in = r.range(0.4f, 1.6f);
                add(a0 + in, a1 - in, b0 + in, b1 - in, h, st, h > 40.f && r.chance(0.5f) ? 1.f : 0.f, h > 55.f,
                    -z0 > 55.f && -z0 < 200.f && r.chance(0.6f));
            };
            if (split == 0) one(x0, x1, z0, z1);
            else if (split == 1) {
                const float xm = r.range(x0 + 6.f, x1 - 6.f);
                one(x0, xm, z0, z1);
                one(xm, x1, z0, z1);
            } else {
                const float xm = (x0 + x1) * 0.5f, zm = (z0 + z1) * 0.5f;
                one(x0, xm, z0, zm); one(xm, x1, z0, zm); one(x0, xm, zm, z1); one(xm, x1, zm, z1);
            }
        }
    }
    // the near corners, at the edges of the screen: tall, their inner faces seen at an angle
    add(-38.f, -16.f, -17.f, -3.f, 36.f, 0, 1.f, true, false);
    add(16.f, 34.f, -17.f, -5.f, 31.f, 1, 1.f, false, false);
    bldgs_[bldgs_.size() - 1].lit = bldgs_[bldgs_.size() - 2].lit = 0.6f;
    builtFor_ = variant;
}

void CityUniverse::spawnFireworks() {
    Rng r(songSeed_ ^ (uint64_t)(time_ * 1000.f));
    const VariantDef& d = VARIANTS[variant_];
    for (int b = 0; b < 6; b++) {
        const float side = (b % 2) ? 1.f : -1.f;
        const vec3 c(side * r.range(55.f, 85.f), r.range(22.f, 50.f), r.range(-170.f, -130.f));
        const vec3 col = lin(d.piece[r.irange(0, 6)]);
        const int n = 54;
        for (int i = 0; i < n; i++) {
            // a sphere of sparks
            const float a = TAU * (i + r.uniform() * 0.5f) / n, e = std::asin(r.range(-1.f, 1.f));
            const float s = r.range(11.f, 16.f);
            Particle p;
            p.p = c;
            p.v = vec3(std::cos(a) * std::cos(e), std::sin(e), std::sin(a) * std::cos(e)) * s;
            p.c = col;
            p.t = 0.f;
            p.life = r.range(1.4f, 2.0f);
            p.delay = b * 0.3f + r.range(0.f, 0.03f);
            sparks_.push_back(p);
        }
    }
}

// =====================================================================================================
// The facade's windows
// =====================================================================================================

void CityUniverse::updateWindows(const Game& g, float dt) {
    // the stack's floors stepping down after a clear
    bool stepping = false;
    for (int y = 0; y < Game::H; y++) stepping |= off_[y] > 0;
    if (stepping) {
        if (offHold_ > 0.f) offHold_ -= dt;
        else {
            offStep_ += dt;
            while (offStep_ >= 0.08f) {
                offStep_ -= 0.08f;
                for (int& o : off_) o = std::max(0, o - 1);
            }
        }
    }
    struct Want { bool on = false, falling = false; int type = -1; };
    Want want[ROWS][COLS];
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < COLS; x++) {
            const int t = g.cell(x, y).type;
            if (t < 0) continue;
            const int vr = y - off_[y];
            if (vr < 0 || vr >= ROWS) continue;
            want[vr][x].on = true;
            want[vr][x].type = t;
        }
    bool ghost[ROWS][COLS] = {};
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        for (auto& c : cells) {
            const int vr = c[1];
            if (vr < 0 || vr >= ROWS || c[0] < 0 || c[0] >= COLS) continue;
            want[vr][c[0]] = {true, true, p.type};
        }
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                const int vr = c[1];
                if (vr < 0 || vr >= ROWS || c[0] < 0 || c[0] >= COLS || want[vr][c[0]].on) continue;
                ghost[vr][c[0]] = true;
            }
        }
    }
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            Win& w = win_[r][c];
            const Want& q = want[r][c];
            const vec3 target = q.on ? pal_.piece[std::clamp(q.type, 0, 6)] * (q.falling ? 1.35f : 1.f) : vec3(0.f);
            const vec3 dd = target - w.to;
            if (std::fabs(dd.x) + std::fabs(dd.y) + std::fabs(dd.z) > 1e-3f) {
                // a new state: an eased blend from what the window shows now (never a jump)
                float dur;
                w.delay = 0.f;
                if (!q.on && pendingOff_[r][c] == 1) dur = 0.14f;                     // a cleared floor going dark
                else if (!q.on && pendingOff_[r][c] == 2) { dur = 0.55f; w.delay = pendingDelay_[r][c]; } // blackout
                else if (q.falling || w.falling) dur = (q.on && w.falling && !q.falling) ? 0.3f : 0.06f; // moving, landing
                else dur = 0.14f;                                                        // the stack stepping down
                w.from = w.cur;
                w.to = target;
                w.t = 0.f;
                w.dur = dur;
            }
            w.falling = q.on && q.falling;
            pendingOff_[r][c] = 0;
            pendingDelay_[r][c] = 0.f;
            if (w.delay > 0.f) w.delay -= dt;
            else w.t = std::min(w.dur, w.t + dt);
            w.cur = lerp(w.from, w.to, smoothstepf(0.f, 1.f, w.t / std::max(1e-3f, w.dur)));
            w.fall = w.falling ? 1.f : std::max(0.f, w.fall - dt / 0.3f);
            w.ghost = ghost[r][c] ? std::min(1.f, w.ghost + dt / 0.06f) : std::max(0.f, w.ghost - dt / 0.06f);
        }
}

void CityUniverse::onEvent(const GameEvent& ev, const Game& g) {
    (void)g;
    switch (ev.type) {
    case GameEvent::HardDrop: hardPending_ = true; break;
    case GameEvent::Lock: {
        // a soft glow along the ledge under the piece (no surge, no flicker: the windows simply stay lit)
        float lowY = 1e9f, x0 = 1e9f, x1 = -1e9f;
        int type = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= COLS) continue;
            type = c.type;
            const float y = (float)(Game::H - 1 - c.y) - 9.5f;
            if (y < lowY - 0.1f) { lowY = y; x0 = x1 = c.x - 4.5f; }
            else if (std::fabs(y - lowY) < 0.1f) { x0 = std::min(x0, c.x - 4.5f); x1 = std::max(x1, c.x - 4.5f); }
        }
        if (lowY < 1e8f && lowY > -10.f) {
            Contact ct;
            ct.x0 = x0 - 0.5f;
            ct.x1 = x1 + 0.5f;
            ct.y = lowY - 0.5f;
            ct.col = pal_.piece[std::clamp(type, 0, 6)];
            ct.power = hardPending_ ? 0.5f : 0.25f;
            ct.life = hardPending_ ? 0.6f : 0.45f;
            contacts_.push_back(ct);
            if (contacts_.size() > 4) contacts_.erase(contacts_.begin());
        }
        hardPending_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool cleared[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) {
                cleared[c.y] = true;
                const int vr = c.y - off_[c.y];
                if (vr >= 0 && vr < ROWS && c.x >= 0 && c.x < COLS) pendingOff_[vr][c.x] = 1;
            }
        // the floors above keep their lights until the cleared floor is dark, then step down
        int noff[Game::H] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            noff[dst] = off_[y] + (dst - y);
            dst--;
        }
        std::memcpy(off_, noff, sizeof(noff));
        contacts_.clear(); // (nothing lingers where the floors went dark)
        offHold_ = 0.16f;
        offStep_ = 0.f;
        if (ev.count >= 4) {
            quadT_ = 0.f;
            spawnFireworks();
        }
        break;
    }
    case GameEvent::TopOut: {
        // the blackout, floor by floor from the top
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            const int vr = c.y - off_[c.y];
            if (vr < 0 || vr >= ROWS || c.x < 0 || c.x >= COLS) continue;
            pendingOff_[vr][c.x] = 2;
            pendingDelay_[vr][c.x] = (c.y - top) * 0.08f + rng_.range(0.f, 0.04f);
        }
        for (int& o : off_) o = 0;
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

// The HUD's panels (its lettering: hudLine, the host letters it).
void CityUniverse::drawHud(const std::vector<HudRect>& panels, float mul) {
    if (panels.empty()) return;
    const float bridge = saturate(bridge_.amount);
    const vec3 acc = lerp(pal_.accent, bridge_.ink, bridge) * mul;
    const vec3 shadow = ugl::toSrgb(lerp(pal_.shade, bridge_.color, bridge));
    glUseProgram(progPanel_);
    set2f(progPanel_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    for (const HudRect& r : panels) {
        const float q[12] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
        glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
        const vec3 c = r.custom ? r.color : r.accent ? ugl::toSrgb(acc) : shadow;
        const float a = saturate(r.accent ? r.alpha * 0.35f : r.alpha * 0.9f);
        glUniform4f(ugl::U(progPanel_, "uColor"), c.x * a, c.y * a, c.z * a, a);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// The HUD in CITY's typography, lettered by the host (text.hpp): Barlow (a signage face), the labels in small widely
// tracked capitals, the numbers plain, each line on a soft shadow of the night (of the fog's pale at dawn) so it
// reads over the lit buildings; the section, the notices and the universe's name in the accent, like a building's
// sign.
void CityUniverse::hudLine(const HudText& h, text::Line& out) {
    text::Font* barlow = text::font(FONT_BARLOW);
    const float bridge = saturate(bridge_.amount);
    const vec3 ink = ugl::toSrgb(lerp(pal_.text, bridge_.ink, bridge) * hudMul_);
    const vec3 acc = ugl::toSrgb(lerp(pal_.accent, bridge_.ink, bridge) * hudMul_);
    const float s = height_ / 1080.f;
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a bias on the old distance field (8 px of spread at 64 px), in em
    constexpr float BIAS = 255.f / 16.f / 64.f;
    float bias = 0.02f;
    auto put = [&](const std::string& t, float size, float track, float dy, const vec3& c, float a, float b) {
        out.add(t, barlow, size, c, track / size, saturate(a));
        out.dy = dy;
        bias = b;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in the accent, the variant smaller and quieter after it
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 0.9f, tr = base * 0.4f, vsz = base * 0.68f, vtr = base * 0.3f;
        const float a = saturate(0.35f + h.alpha * 1.6f);
        put(name, sz, tr, 0.f, acc, a, 0.03f);
        if (!var.empty()) {
            out.add("", barlow, base, acc, (base * 0.9f - tr) / base); // the gap
            // its cap line (sz - vsz) * 0.75 lower (Barlow's capitals: 0.583 of its size)
            out.add(var, barlow, vsz, ink, vtr / vsz, a * 0.75f).dy = (0.75f - 0.583f) * (sz - vsz);
        }
        break;
    }
    case HUD_TITLE: put(h.text, base * 1.0f, base * 0.04f, 0.f, ink, h.alpha * 1.3f, 0.03f); break;
    case HUD_INFO: put(h.text, base * 0.74f, base * 0.14f, 0.f, ink, alpha * 0.65f, 0.01f); break;
    case HUD_SECTION: put(h.text, base * 0.9f, base * 0.45f, 0.f, acc, 0.2f + h.alpha * 1.9f, 0.04f); break;
    case HUD_LABEL: put(h.text, base * 0.64f, base * 0.42f, 0.f, ink, alpha * 0.85f, 0.04f); break;
    case HUD_VALUE: put(h.text, base * 1.05f, base * 0.06f, 0.f, ink, alpha * 0.96f, 0.03f); break;
    case HUD_NOTICE: put(h.text, base * 0.95f, base * 0.16f, 0.f, acc, alpha, 0.03f); break;
    case HUD_TOAST: put(h.text, base * 1.2f, base * 0.36f, -base * 0.1f, acc, alpha, 0.05f); break;
    case HUD_TOAST_SUB: put(h.text, base * 0.82f, base * 0.14f, 0.f, ink, alpha, 0.02f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(h.text, base * 1.05f, base * 0.2f, -base * 0.1f, ink, alpha, 0.03f);
        else put(h.text, base * 0.8f, base * 0.06f, 0.f, h.accent ? acc : ink, alpha, 0.02f);
        break;
    default: put(h.text, base * 0.8f, base * 0.05f, 0.f, h.accent ? acc : ink, alpha, 0.02f); break;
    }
    // a wide soft shadow of the night under the letters (bias .32, softness .36 on the old field), then the fill
    out.look.weight = bias * BIAS;
    out.look.shadow = ugl::toSrgb(lerp(pal_.shade, bridge_.color, bridge));
    out.look.shadowA = 0.85f;
    out.look.shadowW = 0.32f * BIAS;
    out.look.shadowSoft = 0.36f * BIAS;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void CityUniverse::drawInst(const std::vector<Inst>& v) {
    if (v.empty()) return;
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void CityUniverse::fullscreen() {
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void CityUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& d = VARIANTS[variant_];
    if (builtFor_ != variant_) buildCity(variant_);
    {
        const float k = palSet_ ? 1.f - std::exp(-dt * 1.2f) : 1.f;
        palSet_ = true;
        auto ease = [&](vec3& a, unsigned v) { a = lerp(a, lin(v), k); };
        auto easef = [&](float& a, float v) { a = lerpf(a, v, k); };
        ease(pal_.top, d.top); ease(pal_.hor, d.hor); ease(pal_.glow, d.glow); ease(pal_.fog, d.fog); ease(pal_.cloud, d.cloud);
        ease(pal_.amb, d.amb); ease(pal_.moon, d.moon); ease(pal_.upl, d.upl); ease(pal_.street, d.street); ease(pal_.city, d.city);
        ease(pal_.frame, d.frame); ease(pal_.tint, d.tint); ease(pal_.near, d.nearB); ease(pal_.far, d.farB);
        ease(pal_.accent, d.accent); ease(pal_.text, d.text); ease(pal_.shade, d.shade); ease(pal_.reflCity, d.reflCity);
        for (int i = 0; i < 7; i++) ease(pal_.piece[i], d.piece[i]);
        easef(pal_.fogD, d.fogD); easef(pal_.mull, d.mull); easef(pal_.spand, d.spand); easef(pal_.recess, d.recess);
        easef(pal_.reflK, d.reflK); easef(pal_.stars, d.stars); easef(pal_.clouds, d.clouds); easef(pal_.rain, d.rain);
        easef(pal_.snow, d.snow); easef(pal_.glowK, d.glowK); easef(pal_.exposure, d.exposure);
    }
    // the section, from the HUD (a drop, or any other peak: a chorus)
    sectionKnown_ = false;
    for (const HudText& h : f.hud)
        if (h.role == HUD_SECTION) {
            sectionKnown_ = true;
            sectionDrop_ = h.text == "DROP";
        }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    const float bridge = saturate(bridge_.amount);

    if (!f.paused) {
        time_ += dt;
        world_ += wdt;
        if (lvT_ < 1.f) {
            lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
            lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
        }
        dropT_ += dt;
        quadT_ += dt;
        overT_ += dt;
        dropWait_ = std::max(0.f, dropWait_ - dt);
        const bool dropish = sectionKnown_ ? sectionDrop_
                                           : (music.hasSong ? music.speed > 1.33f || (inDrop_ && music.speed > 1.26f) : inDrop_);
        const bool hot = level_ >= 2 && dropWait_ <= 0.f && dropish;
        drop_ = hot ? std::min(1.f, drop_ + dt / 0.25f) : std::max(0.f, drop_ - dt / 2.0f);
        chorus_ = approach(chorus_, level_ >= 2 && !hot ? 1.f : 0.f, 1.2f, dt);
        // the level meters: the music's bands, smoothed (no flicker)
        for (int i = 0; i < 8; i++) {
            const float v = 0.5f * (music.bandsFast[i * 2] + music.bandsFast[i * 2 + 1]);
            eq_[i] = approach(eq_[i], 0.25f + 0.7f * saturate(v * 1.1f), 5.f, dt);
        }
        // the patterns: a new one every two bars, a step every two beats
        if (music.hasSong) {
            patEpoch_ = (float)std::floor(music.beat / 8.0);
            patPhase_ = (float)std::fmod(std::floor(music.beat / 2.0), 4.0);
        } else {
            patEpoch_ = std::floor(world_ / 8.f);
            patPhase_ = std::fmod(std::floor(world_ / 2.f), 4.f);
        }
        for (Contact& c : contacts_) c.t += dt;
        contacts_.erase(std::remove_if(contacts_.begin(), contacts_.end(), [](const Contact& c) { return c.t >= c.life; }), contacts_.end());
        for (Particle& p : sparks_) {
            if (p.delay > 0.f) { p.delay -= dt; continue; }
            p.t += dt;
            p.v.y -= 7.f * dt;
            p.v = p.v * std::exp(-1.1f * dt);
            p.p = p.p + p.v * dt;
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Particle& p) { return p.delay <= 0.f && p.t >= p.life; }), sparks_.end());
        if (!f.bare) updateWindows(game, dt); // (a break's board is blank: the windows are kept as they are)
    }

    // ---- The staging.
    const float lvm = smoothstepf(0.f, 1.f, lv_), lvp = smoothstepf(1.f, 2.f, lv_);
    const float worldOn = (1.f - smoothstepf(0.f, 0.8f, bridge)) * (1.f - 0.35f * dimT_);
    const float swell = inDrop_ && dropT_ < 12.f ? smoothstepf(0.f, 0.15f, dropT_) * std::exp(-std::max(0.f, dropT_ - 0.15f) / 2.5f) * dropAmp_ : 0.f;
    const float flash = dropT_ < 3.f ? smoothstepf(0.f, 0.06f, dropT_) * std::exp(-dropT_ / 0.6f) * dropAmp_ : 0.f;
    const float tet = quadT_ < 6.f ? smoothstepf(0.f, 0.2f, quadT_) * (1.f - smoothstepf(1.5f, 3.f, quadT_)) : 0.f;
    const float black = overT_ < 9.f ? smoothstepf(0.2f, 1.8f, overT_) * (1.f - smoothstepf(4.5f, 8.f, overT_)) : 0.f;
    const float peakK = std::max(chorus_ * lvp, drop_);
    const float kickK = music.kick * lvp; // (small lights only: the beacons, the crowns)
    const float crown = std::max(0.f, 0.55f * chorus_ * lvp + 1.1f * drop_ + 0.6f * tet) * worldOn * (1.f - black);
    const vec3 crownCol = lerp(pal_.accent, dropA_, smoothstepf(0.f, 0.5f, drop_)) * (1.f + 0.5f * kickK * drop_ + 1.2f * flash);
    const float litFrac = (lerpf(0.07f, 0.2f, lvm) + 0.12f * chorus_ * lvp) * lerpf(1.f, 0.45f, slow_);
    const float cityTime = world_ + cityShift_;
    const float bri = smoothstepf(0.f, 1.f, bridge);
    // slow motion, a long exposure: a colder, deeper night
    const vec3 cold = lin(0x0a1a48);
    const vec3 skyTop = lerp(pal_.top, cold * 0.15f, 0.6f * slow_), skyHor = lerp(pal_.hor, cold * 0.6f, 0.6f * slow_);
    const vec3 glowCol = lerp(lerp(pal_.glow, dropA_ * 0.3f, 0.5f * smoothstepf(0.f, 0.5f, drop_)), cold * 0.3f, 0.6f * slow_);
    const float glowK = pal_.glowK * (lerpf(0.7f, 1.f, lvm) + 0.25f * peakK + 0.5f * swell) * (1.f - 0.6f * black);
    const vec3 cityLight = lerp(pal_.city, vec3(0.45f, 0.7f, 1.f), 0.7f * slow_) * (worldOn * lerpf(1.f, 1.3f, peakK));
    const vec3 fogCol = lerp(lerp(pal_.fog, skyHor, 0.3f), glowCol * 0.5f, 0.2f) * (1.f - bri) + bridge_.color * bri;
    const float fogD = pal_.fogD * (1.f + 0.5f * (1.f - lvm));
    auto setSky = [&](GLuint p) {
        set3f(p, "uSkyTop", lerp(skyTop, bridge_.color, bri));
        set3f(p, "uSkyHor", lerp(skyHor, bridge_.color, bri));
        set3f(p, "uGlowCol", glowCol * (1.f - bri));
        set1f(p, "uGlowK", glowK);
    };
    // the HUD's columns on screen (gl_FragCoord), where the city's windows are dimmer
    float quiet[8];
    {
        const float* cols[2] = {layout::LEFT_COLUMN, layout::RIGHT_COLUMN};
        for (int i = 0; i < 2; i++) {
            const vec2 a = project(vec3(cols[i][0], cols[i][1], 0.f)), b = project(vec3(cols[i][2], cols[i][3], 0.f));
            quiet[i * 4 + 0] = std::min(a.x, b.x);
            quiet[i * 4 + 1] = height_ - std::max(a.y, b.y);
            quiet[i * 4 + 2] = std::max(a.x, b.x);
            quiet[i * 4 + 3] = height_ - std::min(a.y, b.y);
        }
    }

    // ---- The scene, into the HDR target.
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progSky_);
    setSky(progSky_);
    setMat(progSky_, "uInvVP", invVP_);
    set3f(progSky_, "uEye", eye_);
    set1f(progSky_, "uStars", pal_.stars * (1.f - 0.5f * peakK) * (1.f - bri));
    set1f(progSky_, "uClouds", pal_.clouds * (1.f - bri));
    set3f(progSky_, "uCloudCol", lerp(pal_.cloud, glowCol * 0.6f, 0.3f * peakK) * (1.f - 0.6f * black));
    set1f(progSky_, "uTime", world_);
    fullscreen();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // the streets far below
    glUseProgram(progGround_);
    setMat(progGround_, "uVP", vp_);
    set4f(progGround_, "uExtent", -1500.f, 20.f, 1500.f, -2500.f);
    set1f(progGround_, "uY", GROUND_Y);
    set3f(progGround_, "uEye", eye_);
    set3f(progGround_, "uFogCol", fogCol);
    set3f(progGround_, "uStreetCol", lerp(pal_.street, cold, 0.6f * slow_) * worldOn);
    set3f(progGround_, "uAmb", pal_.amb);
    set1f(progGround_, "uFogD", fogD);
    set1f(progGround_, "uTime", world_);
    set1f(progGround_, "uTraffic", lerpf(0.35f, 1.f, lvm) * worldOn);
    set1f(progGround_, "uBlack", black);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // the buildings, and the crane
    inst_.clear();
    for (const Bldg& b : bldgs_) {
        inst_.push_back(Inst{{b.cx, b.cz, b.hw, b.hd}, {b.y0, b.y1, b.seed, (float)b.style},
                             {b.lit, b.crown * crown, b.patterned ? 1.f : 0.f, 1.f}, {b.alb.x, b.alb.y, b.alb.z, b.refl}});
    }
    const Bldg* craneHost = nullptr;
    for (const Bldg& b : bldgs_)
        if (b.cz < -45.f && b.cz > -100.f && b.cx > 25.f && b.cx < 70.f && b.y1 - b.y0 < 40.f && (!craneHost || b.y1 > craneHost->y1)) craneHost = &b;
    vec3 craneTip(0.f);
    if (craneHost) {
        const vec3 c = pal_.near * 1.4f;
        const float mx = craneHost->cx + craneHost->hw * 0.3f, mz = craneHost->cz, base = craneHost->y1;
        auto solid = [&](float x0, float x1, float y0, float y1, float z0, float z1) {
            inst_.push_back(Inst{{(x0 + x1) * 0.5f, (z0 + z1) * 0.5f, (x1 - x0) * 0.5f, (z1 - z0) * 0.5f}, {y0, y1, 0.f, 2.f},
                                 {0.f, 0.f, 0.f, 0.f}, {c.x, c.y, c.z, 0.f}});
        };
        solid(mx - 0.3f, mx + 0.3f, base, base + 9.f, mz - 0.3f, mz + 0.3f);
        const float ang = world_ * TAU / 160.f + 2.f;
        const float ca = std::cos(ang), sa = std::sin(ang);
        // the jib: a chain of small boxes along its direction (it turns very slowly)
        for (int i = -3; i <= 14; i++) {
            const float u = i * 0.9f;
            const float x = mx + ca * u, z = mz + sa * u;
            solid(x - 0.3f, x + 0.3f, base + 8.6f, base + 9.f, z - 0.3f, z + 0.3f);
        }
        solid(mx - ca * 2.4f - 0.6f, mx - ca * 2.4f + 0.6f, base + 7.8f, base + 8.6f, mz - sa * 2.4f - 0.6f, mz - sa * 2.4f + 0.6f);
        craneTip = vec3(mx + ca * 12.6f, base + 9.2f, mz + sa * 12.6f);
    }
    glUseProgram(progBox_);
    setSky(progBox_);
    setMat(progBox_, "uVP", vp_);
    set3f(progBox_, "uEye", eye_);
    set3f(progBox_, "uMoonDir", MOON_DIR);
    set3f(progBox_, "uMoonCol", pal_.moon * (1.f - 0.3f * slow_));
    set3f(progBox_, "uAmb", pal_.amb);
    set3f(progBox_, "uUpCol", pal_.upl * (1.f - 0.6f * black));
    set3f(progBox_, "uFogCol", fogCol);
    set1f(progBox_, "uFogD", fogD);
    set3f(progBox_, "uCityLight", cityLight);
    {
        // the city's lamps: the night's own tungsten, fluorescent, halogen, a dim TV and sodium (unit luminance),
        // all turning cold in slow motion
        const vec3 src[5] = {pal_.city, pal_.piece[0], pal_.piece[4], lerp(pal_.piece[3], pal_.piece[0], 0.35f), pal_.piece[5]};
        float lc[15];
        for (int i = 0; i < 5; i++) {
            vec3 c = src[i];
            const float y = std::max(0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z, 1e-4f);
            c = c * ((i == 3 ? 0.55f : 1.f) / y);
            if (i == 1) c = lerp(c, vec3(1.f), 0.45f); // fluorescent tubes read nearly white from afar
            c = lerp(c, vec3(0.45f, 0.7f, 1.f) * 1.4f, 0.7f * slow_);
            lc[i * 3] = c.x; lc[i * 3 + 1] = c.y; lc[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progBox_, "uLamp"), 5, lc);
    }
    set3f(progBox_, "uDropA", dropA_);
    set3f(progBox_, "uDropB", dropB_);
    set3f(progBox_, "uCrownCol", crownCol);
    set3f(progBox_, "uSnowCol", lin(0xdfe6f4) * 0.4f);
    set1f(progBox_, "uTime", cityTime);
    set1f(progBox_, "uLitFrac", litFrac);
    set1f(progBox_, "uDropK", smoothstepf(0.f, 0.4f, drop_) * (1.f - black));
    set1f(progBox_, "uPatEpoch", patEpoch_);
    set1f(progBox_, "uPatPhase", patPhase_);
    set1f(progBox_, "uBlack", black);
    set1f(progBox_, "uFlash", flash);
    set1f(progBox_, "uSnow", pal_.snow);
    set1f(progBox_, "uGroundY", GROUND_Y);
    glUniform1fv(ugl::U(progBox_, "uEq"), 8, eq_);
    glUniform4fv(ugl::U(progBox_, "uQuiet"), 2, quiet);
    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstVbo_);
    glBufferData(GL_ARRAY_BUFFER, inst_.size() * sizeof(Inst), inst_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, (GLsizei)inst_.size());

    // ---- The tower: the windows' light into the two textures, then the facade. Not in a break's frame: the tower
    // is the well, it goes with the game and the city behind it shows.
    if (!f.bare) {
        {
            std::vector<float> a(ROWS * COLS * 4), b(ROWS * COLS * 4);
            const float bright = (1.f + 0.1f * peakK) * worldOn * (1.f - 0.2f * slow_);
            for (int r = 0; r < ROWS; r++)
                for (int c = 0; c < COLS; c++) {
                    const Win& w = win_[r][c];
                    const int i = (r * COLS + c) * 4;
                    const vec3 col = w.cur * bright;
                    a[i] = col.x; a[i + 1] = col.y; a[i + 2] = col.z; a[i + 3] = 1.f;
                    b[i] = w.ghost * worldOn;
                    b[i + 1] = w.fall;
                    b[i + 2] = 0.f;
                    b[i + 3] = 0.f;
                }
            glBindTexture(GL_TEXTURE_2D, cellA_);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, COLS, ROWS, GL_RGBA, GL_FLOAT, a.data());
            glBindTexture(GL_TEXTURE_2D, cellB_);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, COLS, ROWS, GL_RGBA, GL_FLOAT, b.data());
        }
        glUseProgram(progTower_);
        setSky(progTower_);
        setMat(progTower_, "uVP", vp_);
        set4f(progTower_, "uRect", -5.5f, -40.f, 5.5f, 16.6f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cellA_);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cellB_);
        glActiveTexture(GL_TEXTURE0);
        set1i(progTower_, "uCellA", 0);
        set1i(progTower_, "uCellB", 1);
        set3f(progTower_, "uEye", eye_);
        set3f(progTower_, "uFrame", pal_.frame * (1.f - 0.5f * black));
        set3f(progTower_, "uGlassTint", lerp(pal_.tint, vec3(1.f), 0.6f)); // only a hint of the glass color: the lamps keep their own
        {
            vec3 gc = pal_.piece[0];
            if (game.hasPiece()) gc = pal_.piece[std::clamp(game.piece().type, 0, 6)];
            set3f(progTower_, "uGhostCol", gc * 0.8f);
        }
        set3f(progTower_, "uPierCol", crownCol);
        set3f(progTower_, "uCrownCol", crownCol);
        set3f(progTower_, "uAmb", pal_.amb);
        set3f(progTower_, "uUpCol", pal_.upl * (1.f - 0.6f * black));
        set3f(progTower_, "uMoonCol", pal_.moon);
        set3f(progTower_, "uMoonDir", MOON_DIR);
        set3f(progTower_, "uSnowCol", lin(0xdfe6f4) * 0.4f);
        set3f(progTower_, "uReflCity", pal_.reflCity * (0.25f * (1.f - black) * worldOn) + crownCol * (0.02f * crown));
        set1f(progTower_, "uMull", pal_.mull);
        set1f(progTower_, "uSpand", pal_.spand);
        set1f(progTower_, "uRecess", pal_.recess);
        set1f(progTower_, "uReflK", pal_.reflK);
        set1f(progTower_, "uPier", (0.5f * chorus_ * lvp + 0.9f * drop_ + 0.4f * kickK * drop_) * worldOn * (1.f - black));
        set1f(progTower_, "uCrown", (0.25f + crown) * worldOn * (1.f - black));
        set1f(progTower_, "uSnow", pal_.snow);
        set1f(progTower_, "uRain", pal_.rain);
        set1f(progTower_, "uTime", world_);
        set1f(progTower_, "uChase", tet * worldOn);
        set1f(progTower_, "uRoomAmb", 0.0035f);
        set1f(progTower_, "uFrameLight", 4.f);
        // the facade's floodlights, from the ledge of each floor and from below
        set3f(progTower_, "uFlood", pal_.upl * 3.f * (1.f - 0.7f * black) * worldOn);
        set1f(progTower_, "uGroundY", GROUND_Y);
        {
            float cc[21];
            for (int i = 0; i < 7; i++) { cc[i * 3] = pal_.piece[i].x; cc[i * 3 + 1] = pal_.piece[i].y; cc[i * 3 + 2] = pal_.piece[i].z; }
            glUniform3fv(ugl::U(progTower_, "uChaseCol"), 7, cc);
            float ct[16] = {}, ccol[12] = {};
            for (size_t i = 0; i < contacts_.size() && i < 4; i++) {
                const Contact& c = contacts_[i];
                const float u = c.t / c.life;
                ct[i * 4] = c.x0; ct[i * 4 + 1] = c.x1; ct[i * 4 + 2] = c.y;
                ct[i * 4 + 3] = c.power * smoothstepf(0.f, 0.25f, u) * (1.f - smoothstepf(0.3f, 1.f, u)) * worldOn;
                ccol[i * 3] = c.col.x; ccol[i * 3 + 1] = c.col.y; ccol[i * 3 + 2] = c.col.z;
            }
            glUniform4fv(ugl::U(progTower_, "uContact"), 4, ct);
            glUniform3fv(ugl::U(progTower_, "uContactCol"), 4, ccol);
        }
        set1f(progTower_, "uBridge", bri);
        set3f(progTower_, "uBridgeColor", bridge_.color);
        set3f(progTower_, "uBridgeInk", bridge_.ink);
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // ---- The lights: beacons, the crane's tip, fireworks, rain, snow (depth tested, additive).
    dots_.clear();
    auto dot = [&](const vec3& p, float r, const vec3& c, float a, const vec3& dir, float len, float kind) {
        dots_.push_back(Inst{{p.x, p.y, p.z, r}, {c.x, c.y, c.z, a}, {dir.x, dir.y, dir.z, len}, {kind, 0.f, 0.f, 0.f}});
    };
    {
        // the beacons: blinking on their own in a break, every two beats on the beat grid from mid, hitting with
        // the kicks at a peak
        std::vector<vec3> bs;
        for (const Bldg& b : bldgs_)
            if (b.beacon) bs.push_back(vec3(b.cx, b.y1 + 0.4f, b.cz));
        if (!f.bare) { // the tower's own
            bs.push_back(vec3(-5.38f, 11.18f, 0.05f));
            bs.push_back(vec3(5.38f, 11.18f, 0.05f));
            bs.push_back(vec3(2.6f, 14.4f, 0.05f));
        }
        const bool grid = music.hasSong && lv_ > 0.5f;
        for (size_t i = 0; i < bs.size(); i++) {
            float on;
            if (grid) {
                const float ph = (float)std::fmod(music.beat / 2.0 + (i % 2) * 0.5, 1.0);
                on = std::exp(-ph * 5.f);
            } else {
                const float ph = std::fmod(world_ / 2.4f + hashf((float)i), 1.f);
                on = ph < 0.18f ? smoothstepf(0.f, 0.05f, ph) : std::exp(-(ph - 0.18f) * 12.f);
            }
            on = std::max(on, 0.06f) * (1.f + 0.8f * kickK * drop_);
            const float dz = eye_.z - bs[i].z;
            const float r = 0.09f * dz / eye_.z + 0.05f * slow_ * dz / eye_.z;
            dot(bs[i], r, BEACON_RED * 3.f, on * worldOn * (1.f - 0.8f * black), vec3(1.f, 0.f, 0.f), 0.f, 3.f);
        }
        if (craneHost) {
            const float blink = 0.55f + 0.45f * (std::fmod(world_ * 0.5f, 1.f) < 0.5f ? 1.f : 0.f);
            dot(craneTip, 0.2f, BEACON_RED * 3.f, blink * worldOn, vec3(1.f, 0.f, 0.f), 0.f, 3.f);
        }
    }
    for (const Particle& p : sparks_) {
        if (p.delay > 0.f) continue;
        const float u = p.t / p.life;
        const float a = std::pow(1.f - u, 1.5f) * smoothstepf(0.f, 0.05f, p.t) * worldOn;
        const float sp = length(p.v);
        dot(p.p, 0.28f, p.c * 4.f, a, p.v, sp * 0.12f, 1.f);
    }
    if (pal_.rain > 0.01f) {
        const vec3 rc = lerp(pal_.city, pal_.accent, 0.3f) * 0.12f;
        for (const Flake& fl : flakes_) {
            float y = std::fmod(fl.p.y + 30.f - world_ * 40.f * fl.s, 75.f);
            if (y < 0.f) y += 75.f;
            y -= 30.f;
            dot(vec3(fl.p.x + (y + 30.f) * 0.1f, y, fl.p.z), 0.05f, rc, worldOn * pal_.rain * (0.5f + 0.5f * fl.s), vec3(0.1f, -1.f, 0.f), 2.4f, 0.f);
        }
    }
    if (pal_.snow > 0.01f) {
        const vec3 sc = lin(0xe8eeff) * 0.25f;
        for (const Flake& fl : flakes_) {
            float y = std::fmod(fl.p.y + 30.f - world_ * 2.4f * fl.s, 75.f);
            if (y < 0.f) y += 75.f;
            y -= 30.f;
            const float x = fl.p.x + 1.2f * std::sin(world_ * 0.4f * fl.s + fl.ph);
            dot(vec3(x, y, fl.p.z), 0.1f + 0.08f * fl.s, sc, worldOn * pal_.snow * (0.4f + 0.4f * fl.s), vec3(1.f, 0.f, 0.f), 0.f, 0.f);
        }
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glUseProgram(progDot_);
    setMat(progDot_, "uVP", vp_);
    drawInst(dots_);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    // ---- The previews (small lit windows), on the board plane (none in a break's frame).
    wins_.clear();
    if (!f.bare) {
        auto preview = [&](int type, vec2 center, float sc, float level) {
            if (type < 0) return;
            Piece p{type, 0, 0, 0};
            int cells[4][2];
            game.pieceCells(p, cells);
            float minx = 9, maxx = -9, miny = 9, maxy = -9;
            for (auto& c : cells) {
                minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
                miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
            }
            const vec3 L = pal_.piece[type] * (level * worldOn);
            for (int i = 0; i < 4; i++) {
                const vec2 pos = v2(center.x + (cells[i][0] - (minx + maxx) * 0.5f) * sc, center.y - (cells[i][1] - (miny + maxy) * 0.5f) * sc);
                const float h = sc * 0.5f;
                wins_.push_back(Inst{{pos.x - h, pos.y - h, pos.x + h, pos.y + h}, {L.x, L.y, L.z, 1.f}, {0, 0, 0, 0}, {0, 0, 0, 0}});
            }
        };
        preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.3f : 0.85f);
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            if (game.next(i) >= 0) preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), i == 0 ? 1.05f : 0.75f);
        }
        if (!wins_.empty()) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(progWin_);
            setMat(progWin_, "uVP", vp_);
            set3f(progWin_, "uFrame", pal_.frame * 0.3f);
            set1f(progWin_, "uBridge", bri);
            set3f(progWin_, "uBridgeInk", bridge_.ink);
            drawInst(wins_);
        }
    }
    glDisable(GL_BLEND);

    // ---- Bloom: the lit windows and lights glow in the night air (wider in slow motion).
    glUseProgram(progBright_);
    bloomA_.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex_);
    set1i(progBright_, "uTex", 0);
    set2f(progBright_, "uTexel", 1.f / width_, 1.f / height_);
    fullscreen();
    auto blur = [&](ugl::Target& src, ugl::Target& dst, float dx, float dy) {
        dst.bind();
        glUseProgram(progBlur_);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        set1i(progBlur_, "uTex", 0);
        set2f(progBlur_, "uDir", dx / src.w, dy / src.h);
        fullscreen();
    };
    blur(bloomA_, bloomB_, 1.f, 0.f);
    blur(bloomB_, bloomA_, 0.f, 1.f);
    blur(bloomA_, bloomC_, 1.5f, 0.f);
    blur(bloomC_, bloomD_, 0.f, 1.5f);
    blur(bloomD_, bloomC_, 1.5f, 0.f);
    blur(bloomC_, bloomD_, 0.f, 1.5f);

    // ---- The final pass: the searchlights' beams, the bloom, the filmic curve; then the HUD.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, bloomD_.tex);
    glActiveTexture(GL_TEXTURE0);
    set1i(progFinal_, "uScene", 0);
    set1i(progFinal_, "uDepth", 1);
    set1i(progFinal_, "uBloom", 2);
    set1i(progFinal_, "uBloom2", 3);
    setMat(progFinal_, "uInvVP", invVP_);
    set3f(progFinal_, "uEye", eye_);
    {
        // the searchlights: two in a chorus, four in a drop, swaying slowly in the haze over the city
        float bo[16] = {}, bd[16] = {}, bc[12] = {};
        const float xs[4] = {-70.f, 85.f, -150.f, 160.f}, zs[4] = {-200.f, -230.f, -320.f, -300.f};
        const float periods[4] = {13.f, 17.f, 11.f, 15.f};
        for (int i = 0; i < 4; i++) {
            const float on = (i < 2 ? std::max(chorus_ * lvp, drop_) : drop_) * worldOn * (1.f - black);
            if (on < 0.002f) continue;
            const float sgn = xs[i] < 0.f ? 1.f : -1.f;
            const float a = sgn * (0.2f + 0.25f * std::sin(world_ * TAU / periods[i] + i * 1.7f));
            const float b = 0.25f * std::sin(world_ * TAU / (periods[i] * 1.3f) + i);
            const vec3 D = normalize(vec3(std::sin(a), std::cos(a), b));
            bo[i * 4] = xs[i]; bo[i * 4 + 1] = GROUND_Y; bo[i * 4 + 2] = zs[i];
            bo[i * 4 + 3] = on * (0.035f + 0.025f * swell);
            bd[i * 4] = D.x; bd[i * 4 + 1] = D.y; bd[i * 4 + 2] = D.z; bd[i * 4 + 3] = 1.6f;
            vec3 c = drop_ > 0.01f ? lerp(pal_.accent, (i % 2) ? dropB_ : dropA_, smoothstepf(0.f, 0.6f, drop_)) : pal_.accent;
            c = lerp(c * (1.f / std::max(0.05f, ugl::luma(c))), vec3(1.f), 0.4f);
            bc[i * 3] = c.x; bc[i * 3 + 1] = c.y; bc[i * 3 + 2] = c.z;
        }
        glUniform4fv(ugl::U(progFinal_, "uBeamO"), 4, bo);
        glUniform4fv(ugl::U(progFinal_, "uBeamD"), 4, bd);
        glUniform3fv(ugl::U(progFinal_, "uBeamCol"), 4, bc);
    }
    set1f(progFinal_, "uBloomK", lerpf(0.9f, 1.6f, slow_));
    set1f(progFinal_, "uExposure", pal_.exposure);
    const float fade = (1.f - 0.5f * dimT_) * saturate(f.fade);
    set1f(progFinal_, "uMul", fade);
    fullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    hudMul_ = fade;
    if (!f.bare) drawHud(f.panels, fade);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_city() {
    static const UniverseEntry e{
        "CITY", "a tower's facade in a night city, its lit rooms the blocks",
        [] { return std::unique_ptr<Universe>(new CityUniverse()); },
        // Busy, driving songs suit it best (a city at night), any tempo.
        [](const SongLook& l) { return 0.75f + 0.25f * l.density + 0.15f * (1.f - l.brightness); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

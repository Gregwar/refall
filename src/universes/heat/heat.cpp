// HEAT: the stack seen through a thermal camera.
//
// One world: a cold, dark room seen through an infrared camera, the well a pane a shade colder than the wall, a few
// warm shapes far away. Every piece lands white hot and cools down through the camera's colormap (white, yellow,
// orange, red, magenta, deep blue), so the stack shows its own history as temperature. The heat is simulated: it
// conducts between neighbouring cells (a fresh piece warms the old ones under it) and into the well's walls and
// floor, and leaves through the exposed faces into the air, where it rises, sways and fades: the stack glows, and
// the air shimmers above it. The pieces are made of seven materials of different emissivity: each type settles to
// its own band of the colormap when cold (the camera reads the material, not only the heat), and each piece radiates
// through its outline (a cooler rim), so the pieces stay apart. The falling piece is the hottest thing on screen, the
// ghost a cold outline.
//
// The song is the room's weather. Calm parts and breaks are cold and quiet (the camera's range wide, the far shapes
// cold, the air still). Choruses are warmer (the range narrows, the far shapes warm up, the air rises faster, a
// bubble of hot air leaves the stack with the kicks). The drop flares: the whole stack flashes hot and cools over
// seconds, the camera switches to another colormap, the far shapes swell with the bars. A cleared row vents its heat:
// its blocks fade out as they cool, the cells above it are warmed by the passing heat and a plume of hot air rises
// out of the stack. A quad is an updraft. Slow motion is the camera's isotherm mode (grey but the hottest band),
// game over the stack cooling into the room from the top down.
//
// The frame is one thermal image: the room, the air and the pieces are written as temperatures into a float
// target, then mapped through the colormap; the camera's overlay (a scale bar with the range in degrees, the
// lettering) is drawn on top.
//
// Cost: the heat of the 10 x 22 cells and the air on a ~150 x 85 grid are simulated on the CPU (a few microseconds
// and a semi-Lagrangian step a frame); two full-screen passes.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/heat/heat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/heat/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Noto Sans Mono (a subset), SIL Open Font License, see OFL-NotoSansMono.txt.
const unsigned char FONT_TTF[] = {
#include "universes/heat/NotoSansMono-Regular.ttf.inc"
};

namespace shaders = heat_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// ---- The colormaps (sRGB stops, 0 cold .. 1 hot).
struct Stop { float t; unsigned c; };
struct Colormap { const char* name; Stop s[10]; int n; };
const Colormap MAPS[] = {
    // 0: ironbow, the classic: black, indigo, purple, magenta, red, orange, yellow, white.
    {"IRONBOW", {{0.f, 0x000005}, {0.08f, 0x0e0e46}, {0.18f, 0x2e1084}, {0.28f, 0x5c118f}, {0.38f, 0x9a1a8a},
                 {0.48f, 0xcc2f55}, {0.6f, 0xe85a22}, {0.72f, 0xf7941a}, {0.85f, 0xfdcd45}, {1.f, 0xfffbea}}, 10},
    // 1: arctic: deep blues and cyans, the heat in gold.
    {"ARCTIC", {{0.f, 0x01040c}, {0.12f, 0x061c40}, {0.26f, 0x0b3f7c}, {0.4f, 0x1673b0}, {0.52f, 0x3fb2d4},
                {0.62f, 0xa6e2e4}, {0.72f, 0xf3d27a}, {0.85f, 0xfb9b2c}, {1.f, 0xfff4dc}}, 9},
    // 2: white hot: plain grey.
    {"WHITE HOT", {{0.f, 0x030303}, {0.35f, 0x404244}, {0.7f, 0xa6a8a8}, {1.f, 0xfdfdfb}}, 4},
    // 3: lava: blue-black, teal, wine, red, orange, yellow.
    {"LAVA", {{0.f, 0x020509}, {0.13f, 0x052832}, {0.27f, 0x0f5458}, {0.4f, 0x5a3456}, {0.52f, 0xa8233a},
              {0.65f, 0xe2441e}, {0.78f, 0xf8941c}, {0.9f, 0xfde06a}, {1.f, 0xfffbe6}}, 9},
    // 4: rainbow (high contrast): indigo, blue, cyan, green, yellow, orange, red, pink white.
    {"RAINBOW", {{0.f, 0x010004}, {0.12f, 0x0e0a40}, {0.24f, 0x1a34b0}, {0.37f, 0x00b2d6}, {0.5f, 0x1ccf5c},
                 {0.62f, 0xdce024}, {0.75f, 0xff8a12}, {0.87f, 0xf01c30}, {1.f, 0xffdcf2}}, 9},
    // 5: glow (hot metal): black, deep red, red, orange, yellow, white.
    {"GLOW", {{0.f, 0x030102}, {0.18f, 0x2c0306}, {0.36f, 0x7c0c0a}, {0.52f, 0xc8260a}, {0.66f, 0xf0620e},
              {0.8f, 0xfcad2a}, {0.92f, 0xfee88a}, {1.f, 0xfffdf4}}, 8},
};

// The variants: the camera's palette, the drop's palette, the room's temperature and its grain.
struct VariantDef { const char* name; int map, dropMap; float wall, grain; };
const VariantDef VARIANTS[] = {
    {"IRONBOW", 0, 4, 0.03f, 0.008f},
    {"ARCTIC", 1, 3, 0.035f, 0.008f},
    {"WHITE HOT", 2, 0, 0.04f, 0.010f},
    {"LAVA", 3, 5, 0.03f, 0.008f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The seven materials (I O T S Z J L): the temperature each settles to when the stack's heat has left it (the
// camera reads emissivity), spread over the colormap's cold half.
const float BASE[7] = {0.54f, 0.21f, 0.41f, 0.27f, 0.47f, 0.14f, 0.34f};

// The displayed temperature of a cell (the shader's formula without the rim).
float cellTemp(int type, float h, float gain) {
    const float b = BASE[std::clamp(type, 0, 6)];
    return b + (1.02f - b) * std::min(1.f, 0.88f * h * gain);
}
// The camera's reading of a temperature, in degrees.
float degrees(float d) { return 14.f + 520.f * std::pow(std::max(0.f, d), 1.6f); }

float hashf(int x, int y, uint64_t s) {
    const uint64_t h = splitmix64((uint64_t)(uint32_t)x * 73856093ull ^ (uint64_t)(uint32_t)y * 19349663ull ^ s);
    return (h >> 40) * (1.0f / 16777216.0f);
}
float vnoise(float x, float y, uint64_t s) {
    const int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    const float a = hashf(xi, yi, s), b = hashf(xi + 1, yi, s), c = hashf(xi, yi + 1, s), d = hashf(xi + 1, yi + 1, s);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}

// UTF-8 to the font's characters: the degree sign is glyph 127, anything else outside ASCII a '?'.
std::string ascii(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 128) { o += (char)c; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB0) { o += '\x7f'; i++; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) { o += ' '; i++; continue; }
        if ((c & 0xE0) == 0xC0) i += 1;
        else if ((c & 0xF0) == 0xE0) i += 2;
        else if ((c & 0xF8) == 0xF0) i += 3;
        o += '?';
    }
    return o;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool HeatUniverse::init(int width, int height) {
    progBg_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BG_FS, "heat room");
    progCell_ = ugl::program(shaders::CELL_VS, shaders::CELL_FS, "heat cell");
    progMap_ = ugl::program(ugl::FULLSCREEN_VS, shaders::MAP_FS, "heat map");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "heat blur");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "heat shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "heat text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    auto mkTex = [&](GLuint& t) {
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    mkTex(heatTex_);
    mkTex(airTex_);
    mkTex(lutTex_);

    buildFont(font_, FONT_TTF);
    buildLuts();
    resize(width, height);
    placeBlobs();
    return glGetError() == GL_NO_ERROR;
}

void HeatUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] heat font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 8;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB0 : ch; // the degree sign in DEL's slot
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f.ok = true;
}

void HeatUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    tempT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glowA_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    glowB_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    buildAir();
}

HeatUniverse::~HeatUniverse() {
    tempT_.destroy();
    glowA_.destroy();
    glowB_.destroy();
    for (GLuint p : {progBg_, progCell_, progMap_, progBlur_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (GLuint t : {heatTex_, airTex_, lutTex_})
        if (t) glDeleteTextures(1, &t);
    if (font_.tex) glDeleteTextures(1, &font_.tex);
    delete (stbtt_fontinfo*)font_.info;
}

void HeatUniverse::buildLuts() {
    const VariantDef& v = VARIANTS[variant_];
    auto fill = [&](const Colormap& m, float (*out)[3]) {
        for (int i = 0; i < 256; i++) {
            const float t = i / 255.f;
            int k = 0;
            while (k < m.n - 2 && t > m.s[k + 1].t) k++;
            const float u = saturate((t - m.s[k].t) / std::max(1e-4f, m.s[k + 1].t - m.s[k].t));
            const vec3 a = ugl::toLinear(hex(m.s[k].c)), b = ugl::toLinear(hex(m.s[k + 1].c));
            const vec3 c = ugl::toSrgb(lerp(a, b, u)); // blend in linear light, store display colors
            out[i][0] = c.x; out[i][1] = c.y; out[i][2] = c.z;
        }
    };
    fill(MAPS[v.map], lutBase_);
    fill(MAPS[v.dropMap], lutDrop_);
    std::vector<unsigned char> px(256 * 2 * 3);
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++) {
            px[i * 3 + c] = (unsigned char)std::lround(saturate(lutBase_[i][c]) * 255.f);
            px[(256 + i) * 3 + c] = (unsigned char)std::lround(saturate(lutDrop_[i][c]) * 255.f);
        }
    glBindTexture(GL_TEXTURE_2D, lutTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 256, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

vec3 HeatUniverse::lutAt(float t) const {
    const int i = std::clamp((int)std::lround(saturate(t) * 255.f), 0, 255);
    const float k = smoothstepf(0.f, 1.f, colAmt_);
    return lerp(vec3(lutBase_[i][0], lutBase_[i][1], lutBase_[i][2]), vec3(lutDrop_[i][0], lutDrop_[i][1], lutDrop_[i][2]), k);
}

// =====================================================================================================
// The air
// =====================================================================================================

void HeatUniverse::buildAir() {
    ax0_ = viewC_.x - viewH_.x - 1.f;
    ay0_ = viewC_.y - viewH_.y - 1.f;
    aw_ = (int)std::ceil((2.f * viewH_.x + 2.f) / as_) + 1;
    ah_ = (int)std::ceil((2.f * viewH_.y + 2.f) / as_) + 1;
    air_.assign((size_t)aw_ * ah_, 0.f);
    airTmp_.assign(air_.size(), 0.f);
}

void HeatUniverse::splatAir(float x, float y, float r, float amt) {
    if (aw_ <= 0 || amt == 0.f) return;
    const float gx = (x - ax0_) / as_, gy = (y - ay0_) / as_;
    if (r <= as_) {
        const int i = (int)std::floor(gx), j = (int)std::floor(gy);
        const float fx = gx - i, fy = gy - j;
        auto add = [&](int a, int b, float w) {
            if (a >= 0 && b >= 0 && a < aw_ && b < ah_) air_[(size_t)b * aw_ + a] += amt * w;
        };
        add(i, j, (1 - fx) * (1 - fy));
        add(i + 1, j, fx * (1 - fy));
        add(i, j + 1, (1 - fx) * fy);
        add(i + 1, j + 1, fx * fy);
        return;
    }
    const float rg = r / as_;
    const int R = (int)std::ceil(rg * 2.f);
    const int ci = (int)std::lround(gx), cj = (int)std::lround(gy);
    float sum = 0.f;
    for (int pass = 0; pass < 2; pass++) {
        const float k = pass == 0 ? 0.f : amt / std::max(sum, 1e-6f);
        for (int j = std::max(0, cj - R); j <= std::min(ah_ - 1, cj + R); j++)
            for (int i = std::max(0, ci - R); i <= std::min(aw_ - 1, ci + R); i++) {
                const float dx = i - gx, dy = j - gy;
                const float w = std::exp(-(dx * dx + dy * dy) / (rg * rg));
                if (pass == 0) sum += w;
                else air_[(size_t)j * aw_ + i] += k * w;
            }
    }
}

void HeatUniverse::updateAir(const Game& g, float dt) {
    if (dt <= 0.f || aw_ <= 0) return;
    airT_ += dt;
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float live = (1.f - over_) * (bridge_.amount < 0.5f ? 1.f : 0.4f);
    // ---- sources: the stack's exposed faces, the falling piece, the vents, the far shapes
    const float kTop = 5.f * (0.9f + 0.5f * wm + 0.5f * wp) * live, kSide = 0.35f * kTop;
    auto filled = [&](int x, int y) { return x >= 0 && x < Game::W && y >= 0 && y < Game::H && g.cell(x, y).type >= 0; };
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0) continue;
            const float h = heat_[y][x];
            if (h < 0.01f) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            if (!filled(x, y - 1)) splatAir(p.x, p.y + 0.55f, 0.4f, h * kTop * dt);
            if (x > 0 && !filled(x - 1, y)) splatAir(p.x - 0.55f, p.y, 0.f, h * kSide * dt);
            if (x < Game::W - 1 && !filled(x + 1, y)) splatAir(p.x + 0.55f, p.y, 0.f, h * kSide * dt);
        }
    if (g.hasPiece() && !g.over()) {
        int cells[4][2];
        g.pieceCells(g.piece(), cells);
        const float dx = g.pieceVisualX() - g.piece().x, dy = g.pieceVisualY() - g.piece().y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            bool top = true;
            for (auto& o : cells)
                if (o[0] == c[0] && o[1] == c[1] - 1) top = false;
            if (!top) continue;
            const vec3 p = layout::cellPos(c[0] + dx, c[1] + dy);
            splatAir(p.x, p.y + 0.55f, 0.4f, 0.55f * kTop * dt);
        }
    }
    for (Vent& v : vents_) {
        if (v.t >= v.life) continue;
        const float k = dt / v.life;
        const int n = std::max(1, (int)std::ceil((v.x1 - v.x0) / 0.5f));
        for (int i = 0; i <= n; i++) {
            const float x = lerpf(v.x0, v.x1, i / (float)n);
            splatAir(x, v.y, 0.45f, v.amt * k / (n + 1) * 3.f);
        }
        v.t += dt;
    }
    vents_.erase(std::remove_if(vents_.begin(), vents_.end(), [](const Vent& v) { return v.t >= v.life; }), vents_.end());
    for (const Blob& b : blobs_) splatAir(b.x, b.y + b.ry * 0.5f, 0.6f, b.temp * 1.6f * dt);

    // ---- advection: the air rises and sways (semi-Lagrangian), spreads a little and fades
    const float up = 1.9f + 1.1f * wm + 1.4f * wp * (0.6f + 0.6f * dropAmt_) + 1.5f * quadFlash_;
    const float swayA = 0.5f + 0.4f * wp;
    const uint64_t s = seed_ ^ 0xA1Bull;
    for (int j = 0; j < ah_; j++) {
        const float y = ay0_ + j * as_;
        for (int i = 0; i < aw_; i++) {
            const float x = ax0_ + i * as_;
            const float u = swayA * 2.f * (vnoise(x * 0.11f, y * 0.11f - airT_ * 0.12f, s) - 0.5f);
            const float sx = (x - u * dt - ax0_) / as_, sy = (y - up * dt - ay0_) / as_;
            const int i0 = (int)std::floor(sx), j0 = (int)std::floor(sy);
            const float fx = sx - i0, fy = sy - j0;
            auto at = [&](int a, int b) {
                if (a < 0 || b < 0 || a >= aw_ || b >= ah_) return 0.f;
                return air_[(size_t)b * aw_ + a];
            };
            airTmp_[(size_t)j * aw_ + i] = lerpf(lerpf(at(i0, j0), at(i0 + 1, j0), fx), lerpf(at(i0, j0 + 1), at(i0 + 1, j0 + 1), fx), fy);
        }
    }
    const float diff = std::min(0.22f, 0.35f * dt / (as_ * as_));
    const float decay = std::exp(-dt * (0.3f + 0.25f * (1.f - wm)));
    for (int j = 0; j < ah_; j++)
        for (int i = 0; i < aw_; i++) {
            const size_t k = (size_t)j * aw_ + i;
            const float c = airTmp_[k];
            const float l = i > 0 ? airTmp_[k - 1] : 0.f, r = i < aw_ - 1 ? airTmp_[k + 1] : 0.f;
            const float d = j > 0 ? airTmp_[k - aw_] : 0.f, t = j < ah_ - 1 ? airTmp_[k + aw_] : 0.f;
            air_[k] = std::max(0.f, (c + diff * (l + r + d + t - 4.f * c)) * decay);
        }
}

// Far away: two or three soft warm shapes in the margins, clear of the HUD columns.
void HeatUniverse::placeBlobs() {
    blobs_.clear();
    Rng r(seed_ ^ 0xB10Bull);
    const float side = r.chance(0.5f) ? -1.f : 1.f;
    const float xm = std::max(13.5f, viewH_.x - 3.5f);
    blobs_.push_back({side * r.range(13.5f, xm), r.range(-9.f, -4.f), r.range(1.6f, 2.4f), r.range(2.4f, 3.6f), 0.f, 0.f});
    blobs_.push_back({-side * r.range(13.5f, xm), r.range(-2.f, 6.f), r.range(1.2f, 1.8f), r.range(1.2f, 1.8f), 0.f, 0.f});
    if (r.chance(0.6f)) blobs_.push_back({side * r.range(14.f, xm), r.range(6.f, 10.f), r.range(0.9f, 1.3f), r.range(0.9f, 1.3f), 0.f, 0.f});
}

// =====================================================================================================
// The stack's heat
// =====================================================================================================

// Keeps the heat map and the piece ids in step with the board; when the board was changed behind our back (a new
// universe mid-game, a test board), the unknown cells get a plausible history: warmer toward the top.
void HeatUniverse::syncCells(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    if (wrong > 4) {
        // a new board altogether: forget the old one's history
        std::memset(ids_, 0, sizeof(ids_));
        std::memset(heat_, 0, sizeof(heat_));
    }
    int top = Game::H;
    for (int y = 0; y < Game::H && top == Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type >= 0) { top = y; break; }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; heat_[y][x] = 0.f; }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            const float h = 0.95f * std::exp(-(y - top) / 3.f) * rng_.range(0.6f, 1.f);
            heat_[y][x] = h;
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
                    heat_[ny][nx] = h;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

// Conduction between neighbouring cells and into the walls and the floor, losses through the exposed faces.
void HeatUniverse::conduct(const Game& g, float dt) {
    if (dt <= 0.f) return;
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    const float warm = saturate(0.5f * wm + 0.5f * wp);
    const float lossMul = lerpf(1.45f, 0.8f, warm);
    const int steps = std::max(1, (int)std::ceil(dt / (1.f / 60.f)));
    const float h = dt / steps;
    const float kc = 0.3f, kw = 0.3f, kLoss = 0.09f, kFace = 0.06f;
    float nh[Game::H][Game::W];
    for (int s = 0; s < steps; s++) {
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                if (g.cell(x, y).type < 0) { nh[y][x] = 0.f; continue; }
                const float c = heat_[y][x];
                float flux = 0.f;
                int faces = 0;
                const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& dd : d) {
                    const int nx = x + dd[0], ny = y + dd[1];
                    if (nx < 0) flux += kw * (wallL_[y] - c);
                    else if (nx >= Game::W) flux += kw * (wallR_[y] - c);
                    else if (ny >= Game::H) flux += kw * (floor_[x] - c);
                    else if (ny < 0) faces++;
                    else if (g.cell(nx, ny).type >= 0) flux += kc * (heat_[ny][nx] - c);
                    else faces++;
                }
                nh[y][x] = std::max(0.f, c + h * (flux - c * (kLoss + kFace * faces) * lossMul));
            }
        // the walls and the floor: warmed by the cells against them, spreading along, cooling slowly
        float nl[Game::H], nr[Game::H], nf[Game::W];
        for (int y = 0; y < Game::H; y++) {
            const float cl = g.cell(0, y).type >= 0 ? heat_[y][0] : wallL_[y];
            const float cr = g.cell(Game::W - 1, y).type >= 0 ? heat_[y][Game::W - 1] : wallR_[y];
            const float ul = y > 0 ? wallL_[y - 1] : wallL_[y], dl = y < Game::H - 1 ? wallL_[y + 1] : wallL_[y];
            const float ur = y > 0 ? wallR_[y - 1] : wallR_[y], dr = y < Game::H - 1 ? wallR_[y + 1] : wallR_[y];
            nl[y] = std::max(0.f, wallL_[y] + h * (kw * (cl - wallL_[y]) + 0.4f * (ul + dl - 2.f * wallL_[y]) - 0.22f * wallL_[y]));
            nr[y] = std::max(0.f, wallR_[y] + h * (kw * (cr - wallR_[y]) + 0.4f * (ur + dr - 2.f * wallR_[y]) - 0.22f * wallR_[y]));
        }
        for (int x = 0; x < Game::W; x++) {
            const float cf = g.cell(x, Game::H - 1).type >= 0 ? heat_[Game::H - 1][x] : floor_[x];
            const float l = x > 0 ? floor_[x - 1] : wallL_[Game::H - 1], r = x < Game::W - 1 ? floor_[x + 1] : wallR_[Game::H - 1];
            nf[x] = std::max(0.f, floor_[x] + h * (kw * (cf - floor_[x]) + 0.4f * (l + r - 2.f * floor_[x]) - 0.22f * floor_[x]));
        }
        std::memcpy(heat_, nh, sizeof(nh));
        std::memcpy(wallL_, nl, sizeof(nl));
        std::memcpy(wallR_, nr, sizeof(nr));
        std::memcpy(floor_, nf, sizeof(nf));
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void HeatUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = h < 0.5f ? 1 : 2;
        else if (t.hasSong && t.bassWeight > 0.6f) v = 3;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool changed = v != variant_;
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x4EA7ull);
    if (changed || lutTex_) buildLuts(); // (the colormaps are rebuilt for every song: cheap)
    if (old != seed_) placeBlobs();
    (void)seconds;
}

void HeatUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    const bool rising = level > level_;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
    (void)impact;
}

// The drop: the stack flares, the camera switches its colormap, a sheet of hot air leaves the stack.
void HeatUniverse::drop(float strength) {
    dropMode_ = true;
    flare_ = std::max(flare_, 0.6f + 0.4f * strength);
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x]) heat_[y][x] = std::min(1.3f, heat_[y][x] + (0.5f + 0.35f * strength) * (0.6f + 0.4f * rng_.uniform()));
    // the far shapes swell
    for (auto& b : blobs_) b.temp = std::max(b.temp, 0.16f);
    // a sheet of hot air over the stack's top
    int top = Game::H;
    for (int y = Game::HIDDEN; y < Game::H && top == Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x]) { top = y; break; }
    const float wy = layout::cellPos(0.f, (float)std::min(top, Game::H - 1)).y + 0.6f;
    vents_.push_back({-4.8f, 4.8f, wy, 1.4f * (0.6f + 0.4f * strength), 0.f, 0.6f});
}

void HeatUniverse::refresh(float strength) {
    if (blobs_.empty()) return;
    Blob& b = blobs_[rng_.irange(0, (int)blobs_.size() - 1)];
    b.temp = std::max(b.temp, 0.12f + 0.12f * strength);
}

void HeatUniverse::levelUp() {
    for (auto& b : blobs_) b.temp = std::max(b.temp, 0.2f);
}

vec2 HeatUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints HeatUniverse::hints() const {
    TransitionHints h;
    auto L = [&](int i) { return ugl::toLinear(vec3(lutBase_[i][0], lutBase_[i][1], lutBase_[i][2])); };
    h.background = L(14);
    h.ink = L(200);
    h.accent = L(235);
    h.light = false;
    h.styles = TS_LUMA | TS_INK | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void HeatUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::HardDrop:
        hardPending_ = true;
        break;
    case GameEvent::Lock: {
        // the piece lands white hot; a hard drop's impact heats the cells it hits, a breath of hot air at the contact
        const int id = nextId_++;
        const bool hard = hardPending_;
        hardPending_ = false;
        float cx = 0.f, minY = 99.f;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            heat_[c.y][c.x] = hard ? 1.12f : 1.f;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            cx += p.x;
            minY = std::min(minY, p.y);
        }
        if (hard) {
            for (auto& c : ev.cells) {
                const int bx = c.x, by = c.y + 1;
                if (bx < 0 || bx >= Game::W || by < 0) continue;
                if (by >= Game::H) { floor_[bx] = std::min(1.2f, floor_[bx] + 0.4f); continue; }
                bool own = false;
                for (auto& o : ev.cells)
                    if (o.x == bx && o.y == by) own = true;
                if (own) continue;
                if (game.cell(bx, by).type >= 0 && ids_[by][bx] != id) heat_[by][bx] = std::min(1.2f, heat_[by][bx] + 0.35f);
            }
            if (!ev.cells.empty()) {
                cx /= ev.cells.size();
                splatAir(cx - 1.2f, minY - 0.2f, 0.5f, 0.35f);
                splatAir(cx + 1.2f, minY - 0.2f, 0.5f, 0.35f);
            }
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the cleared cells fade out as they cool (their heat leaves with the plume)
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && rows[c.y + 1] && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && rows[c.y - 1] && ids_[c.y - 1][c.x] == id) m |= 8;
            }
            fading_.push_back({p.x, p.y, c.x, c.y, std::clamp(c.type, 0, 6), m, heat_[c.y][c.x], 0.f});
        }
        // the vent: each column's heat passes up into the first cell above the cleared rows (the stack warms where
        // the row was), and a plume rises from the top of the stack over each cleared row
        const bool quadClear = ev.count >= 4;
        int lowest = -1, highest = Game::H;
        for (int y = 0; y < Game::H; y++)
            if (rows[y]) { lowest = std::max(lowest, y); highest = std::min(highest, y); }
        for (int x = 0; x < Game::W; x++) {
            float vented = 0.f;
            for (int y = 0; y < Game::H; y++)
                if (rows[y]) vented += heat_[y][x];
            for (int y = highest - 1; y >= 0; y--) {
                if (rows[y] || !ids_[y][x]) continue;
                heat_[y][x] = std::min(1.2f, heat_[y][x] + 0.18f + 0.45f * vented / ev.count);
                break;
            }
        }
        {
            const float wy = layout::cellPos(0.f, (float)lowest).y;
            vents_.push_back({-4.5f, 4.5f, wy, (quadClear ? 2.5f : 0.8f) * ev.count, 0.f, quadClear ? 1.1f : 0.45f});
        }
        // compact the heat and the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            float nhv[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nhv[dst], heat_[y], sizeof(heat_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(heat_, nhv, sizeof(heat_));
        }
        if (quadClear) {
            quadFlash_ = 1.f;
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++)
                    if (ids_[y][x]) heat_[y][x] = std::min(1.2f, heat_[y][x] + 0.25f);
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack stays a while to cool into the room (the game has emptied its board)
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            fading_.push_back({p.x, p.y, c.x, c.y, std::clamp(c.type, 0, 6), 0, heat_[c.y][c.x], -100.f});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void HeatUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto push = [&](float x, float y, float half, int mask, float gx, float gy, float h, float alpha, float base, int kind, float shrink) {
        if (alpha <= 0.003f) return;
        cells_.push_back({{x, y, half, (float)mask}, {gx, gy, h, alpha}, {base, (float)kind, shrink, 0.f}});
    };
    // Locked cells: their heat from the heat map.
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
            push(p.x, p.y, 0.5f, maskOf(x, y, same), (float)x, (float)y, 0.f, 1.f, BASE[std::clamp((int)c.type, 0, 6)], 0, 0.f);
        }
    // Cleared cells cooling out (real time), the topped-out stack cooling into the room from the top.
    for (const Fading& f : fading_) {
        if (f.t <= -50.f) {
            const float rowFrac = (Game::H - 1 - f.gy) / (float)(Game::H - Game::HIDDEN);
            const float k = smoothstepf(1.f - rowFrac * 0.75f - 0.25f, 1.f - rowFrac * 0.75f, over_ * 1.15f);
            const float base = lerpf(BASE[f.type], 0.06f, k);
            push(f.x, f.y, 0.5f, 0, -1.f, 0.f, f.h * (1.f - k), 1.f - smoothstepf(0.6f, 1.f, k), base, 0, 0.f);
            continue;
        }
        const float k = smoothstepf(0.f, 0.32f, f.t);
        push(f.x, f.y, 0.5f, f.mask, -1.f, 0.f, f.h * (1.f - k) * 0.8f, 1.f - smoothstepf(0.08f, 0.34f, f.t),
             lerpf(BASE[f.type], 0.06f, k), 0, 0.12f * k);
    }
    // The ghost (a cold outline) and the falling piece (the hottest thing on screen).
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
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
                push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), -1.f, 0.f, 0.f, 0.9f, 0.3f, 1, 0.f);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), -1.f, 0.f, 1.25f, 1.f, BASE[p.type], 0, 0.f);
        }
    }
    // Previews: each material at a little warmth, so its band shows.
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
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            push(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), -1.f, 0.f, 0.12f, alpha, BASE[type], 0, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// =====================================================================================================
// Text and the overlay
// =====================================================================================================

float HeatUniverse::textWidth(const std::string& s, float size, float track) const {
    const Font& f = font_;
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
    }
    return w - track;
}

void HeatUniverse::textQuads(const std::string& s, float x, float y, float size, float track, std::vector<float>& out) const {
    const Font& f = font_;
    if (!f.ok) return;
    const float k = size / f.px;
    const float baseline = y + f.capH * k;
    float pen = x;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) out.insert(out.end(), q, q + 4);
        }
        pen += g.adv * k + track;
    }
}

void HeatUniverse::drawText(const std::vector<float>& q, const vec3& ink, float alpha) {
    if (q.empty() || !font_.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", vec3(0.01f, 0.01f, 0.015f));
    set1f(progText_, "uHaloA", 0.45f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void HeatUniverse::rect(float x0, float y0, float x1, float y1, const vec3& c, float a) {
    if (a <= 0.003f) return;
    const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                           {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
    for (auto& p : q) shapes_.insert(shapes_.end(), p, p + 6);
}

void HeatUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The camera's overlay: the scale bar (the colormap and its range in degrees) on the right edge, the bonus gauge
// as a thin ring.
void HeatUniverse::drawOverlay(const Game& g, bool bare) {
    const float s = height_ / 1080.f;
    const float vis = (1.f - smoothstepf(0.3f, 0.8f, bridge_.amount)) * (1.f - 0.6f * dimT_);
    const vec3 ink(0.93f, 0.94f, 0.95f), shadow(0.f);
    if (vis <= 0.01f) return;
    std::vector<float> q;
    char buf[32];
    // ---- the scale bar
    {
        const float bw = std::max(4.f, 7.f * s), x1 = width_ - 34.f * s, x0 = x1 - bw;
        const float y0 = height_ * 0.3f, y1 = height_ * 0.7f;
        rect(x0 - 1.5f * s, y0 - 1.5f * s, x1 + 1.5f * s, y1 + 1.5f * s, shadow, 0.45f * vis);
        const int N = 64;
        for (int i = 0; i < N; i++) {
            const float a = y1 - (y1 - y0) * i / N, b = y1 - (y1 - y0) * (i + 1) / N;
            rect(x0, b, x1, a + 0.5f, lutAt((i + 0.5f) / N), vis);
        }
        flushShapes();
        q.clear();
        const float ts = 15.f * s;
        std::snprintf(buf, sizeof(buf), "%.0f\x7f", degrees(hi_ - 0.08f * flare_));
        std::string hiS = buf;
        std::snprintf(buf, sizeof(buf), "%.0f\x7f", degrees(lo_));
        std::string loS = buf;
        textQuads(hiS, x1 - textWidth(hiS, ts, 0.f), y0 - ts * 1.9f, ts, 0.f, q);
        textQuads(loS, x1 - textWidth(loS, ts, 0.f), y1 + ts * 0.9f, ts, 0.f, q);
        drawText(q, ink, 0.75f * vis);
    }
    // ---- the bonus gauge: a thin ring, its charge an arc in the hot color (none at a break)
    if (!bare) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 96;
        const vec3 hot = lutAt(0.86f);
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(2.f, 3.f * s);
        for (int i = 0; i < N; i++) {
            const float a0 = TAU * i / N, a1 = TAU * (i + 1) / N;
            const bool on = (i + 0.5f) / N <= fill;
            const float w = on ? th2 : th;
            const vec3 col = on ? hot : ink;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.6f : 0.28f)) * vis;
            // a quad along the arc (clockwise from the top)
            const float s0 = std::sin(a0), c0 = std::cos(a0), s1 = std::sin(a1), c1 = std::cos(a1);
            const float ri = r - w * 0.5f, ro = r + w * 0.5f;
            const float pts[4][2] = {{c.x + s0 * ri, c.y - c0 * ri}, {c.x + s0 * ro, c.y - c0 * ro},
                                     {c.x + s1 * ro, c.y - c1 * ro}, {c.x + s1 * ri, c.y - c1 * ri}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int k : idx) {
                const float v[6] = {pts[k][0], pts[k][1], col.x, col.y, col.z, al};
                shapes_.insert(shapes_.end(), v, v + 6);
            }
        }
        flushShapes();
    }
}

// The HUD in the camera's lettering: one monospace, light on a soft dark outline, labels spaced out.
void HeatUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? lutAt(0.8f) : vec3(0.02f);
            rect(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }
    const vec3 ink(0.93f, 0.94f, 0.95f), accent = lutAt(0.84f);
    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(text, size, track);
            textQuads(text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string t = h.text;
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            place(t, base * 0.8f, base * 0.18f, base * 0.1f);
            drawText(q, ink, saturate(0.3f + h.alpha * 1.5f));
            continue;
        }
        case HUD_SECTION:
            place(h.text, base * 0.8f, base * 0.3f);
            drawText(q, accent, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(h.text, base * 0.95f, 0.f);
            drawText(q, ink, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(h.text, base * 0.75f, base * 0.05f);
            drawText(q, ink, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_LABEL:
            place(h.text, base * 0.68f, base * 0.3f);
            drawText(q, ink, saturate(alpha * 1.2f) * 0.68f);
            continue;
        case HUD_VALUE:
            place(h.text, base * 0.95f, 0.f);
            drawText(q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(h.text, base * 0.95f, base * 0.05f);
            drawText(q, accent, alpha);
            continue;
        case HUD_KEY:
            place(h.text, base * 0.85f, 0.f);
            drawText(q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(h.text, base * 1.05f, base * 0.4f, -base * 0.1f);
            drawText(q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(h.text, base * 0.85f, base * 0.05f);
            drawText(q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(h.text, base * 1.0f, base * 0.1f);
                drawText(q, ink, alpha);
            } else {
                place(h.text, base * 0.8f, base * 0.05f);
                drawText(q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(h.text, base * 0.78f, 0.f);
            drawText(q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void HeatUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    // a break: the room alone (its blank board leaves the stack's heat, the well's walls and floor as they are)
    const bool bare = f.bare;
    if (!bare) syncCells(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t <= -50.f; }), fading_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? saturate(overT_ / 3.5f) : 0.f, overT_ >= 0.f ? 10.f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    dropAmt_ = approach(dropAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 3.f : 0.4f, dt);
    // the drop's colormap comes in on the downbeat, quickly (no muddy in-between), and leaves slowly
    colAmt_ = approach(colAmt_, dropMode_ ? wp : 0.f, dropMode_ ? 7.f : 0.5f, dt);
    flare_ = approach(flare_, 0.f, 0.35f, dt);
    quadFlash_ = approach(quadFlash_, 0.f, 0.6f, dt);
    const float warm = saturate(0.45f * wm + 0.55f * wp) + 0.2f * dropAmt_ * wp;
    hi_ = approach(hi_, lerpf(1.12f, 0.93f, saturate(warm)) - 0.03f * dropAmt_ * wp, 1.f, dt);
    lo_ = approach(lo_, 0.012f * dropAmt_ * wp, 1.f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f) {
        // the kicks of a peak: a bubble of hot air leaves the stack (alternating sides)
        if (wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f) {
            int col = bubbleSide_ > 0 ? rng_.irange(5, 9) : rng_.irange(0, 4);
            bubbleSide_ = -bubbleSide_;
            int top = Game::H;
            for (int y = Game::HIDDEN; y < Game::H; y++)
                if (game.cell(col, y).type >= 0) { top = y; break; }
            const vec3 p = layout::cellPos((float)col, (float)top);
            splatAir(p.x, p.y + 0.4f, 0.55f, kick * (0.5f + 0.7f * dropAmt_));
        }
        // the bars of a peak: one far shape warms, the others cool (slowly)
        if (music.hasSong) {
            const double beat = std::floor(music.beat + 1e-4);
            if (lastBeat_ < 0.0 || beat < lastBeat_ - 8.0) lastBeat_ = beat;
            if (beat > lastBeat_) {
                lastBeat_ = beat;
                beatCount_++;
                if (std::fmod(beat, 4.0) < 0.5 && wp > 0.5f && !blobs_.empty()) {
                    const int i = (int)(std::fmod(beat / 4.0, (double)blobs_.size()));
                    blobs_[std::clamp(i, 0, (int)blobs_.size() - 1)].temp += 0.04f + 0.05f * dropAmt_;
                }
            }
        }
    }
    kickPrev_ = kick;
    {
        const float target = 0.035f + 0.05f * wm + 0.06f * wp + 0.04f * dropAmt_ * wp;
        for (auto& b : blobs_) b.temp = approach(b.temp, target * (overT_ >= 0.f ? 0.3f : 1.f), 0.5f, wdt);
    }
    for (auto& fd : fading_)
        if (fd.t > -50.f) fd.t += dt; // a clear runs in real time, even in slow motion
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& x) { return x.t > 0.4f; }), fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- The heat.
    if (!bare) conduct(game, wdt);
    updateAir(game, wdt);
    const float gain = (0.92f + 0.12f * saturate(warm) + 0.25f * flare_) * (1.f - over_);

    // ---- Upload the heat map (12 x 23: the walls left and right, the floor below; empty cells take their
    // neighbours' heat so the pieces' edges sample cleanly) and the air.
    {
        heatUp_.assign(12 * 23, 0.f);
        for (int y = 0; y < Game::H; y++) {
            heatUp_[y * 12 + 0] = wallL_[y];
            heatUp_[y * 12 + 11] = wallR_[y];
            for (int x = 0; x < Game::W; x++) {
                float v = heat_[y][x];
                if (game.cell(x, y).type < 0) {
                    float sum = 0.f;
                    int n = 0;
                    const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto& dd : d) {
                        const int nx = x + dd[0], ny = y + dd[1];
                        if (nx < 0 || ny < 0 || nx >= Game::W || ny >= Game::H || game.cell(nx, ny).type < 0) continue;
                        sum += heat_[ny][nx];
                        n++;
                    }
                    v = n ? sum / n : 0.f;
                }
                heatUp_[y * 12 + 1 + x] = v;
            }
        }
        for (int x = 0; x < Game::W; x++) heatUp_[22 * 12 + 1 + x] = floor_[x];
        heatUp_[22 * 12 + 0] = wallL_[Game::H - 1];
        heatUp_[22 * 12 + 11] = wallR_[Game::H - 1];
        glBindTexture(GL_TEXTURE_2D, heatTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 12, 23, 0, GL_RED, GL_FLOAT, heatUp_.data());
        glBindTexture(GL_TEXTURE_2D, airTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, aw_, ah_, 0, GL_RED, GL_FLOAT, air_.data());
    }

    // ---- The thermal image: the room and the air.
    const VariantDef& V = VARIANTS[variant_];
    tempT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progBg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, airTex_);
    set1i(progBg_, "uAir", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, heatTex_);
    set1i(progBg_, "uHeat", 1);
    glActiveTexture(GL_TEXTURE0);
    set4f(progBg_, "uAirGrid", ax0_, ay0_, 1.f / as_, 0.f);
    set4f(progBg_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progBg_, "uRes", (float)width_, (float)height_);
    set1f(progBg_, "uTime", worldTime_);
    set1f(progBg_, "uSeed", (float)(seed_ % 1000) * 0.137f);
    set1f(progBg_, "uWall", V.wall * (1.f - 0.25f * (1.f - wm)) + 0.01f * wp);
    set1f(progBg_, "uAirGain", (0.13f + 0.09f * saturate(warm) + 0.06f * flare_ + 0.06f * quadFlash_) * (1.f - 0.5f * over_));
    set1f(progBg_, "uShimmer", 0.5f + 0.7f * saturate(warm));
    set1f(progBg_, "uGrain", V.grain);
    set1f(progBg_, "uWallHeat", 0.45f);
    set1f(progBg_, "uWell", bare ? 0.f : 1.f);
    set1f(progBg_, "uVig", 0.32f);
    {
        float bl[16] = {}, bt[4] = {};
        for (int i = 0; i < 4 && i < (int)blobs_.size(); i++) {
            bl[i * 4 + 0] = blobs_[i].x; bl[i * 4 + 1] = blobs_[i].y;
            bl[i * 4 + 2] = blobs_[i].rx; bl[i * 4 + 3] = blobs_[i].ry;
            bt[i] = blobs_[i].temp;
        }
        for (int i = (int)blobs_.size(); i < 4; i++) { bl[i * 4 + 0] = 999.f; bl[i * 4 + 2] = bl[i * 4 + 3] = 1.f; }
        glUniform4fv(ugl::U(progBg_, "uBlob"), 4, bl);
        set4f(progBg_, "uBlobT", bt[0], bt[1], bt[2], bt[3]);
    }
    ugl::drawFullscreen();

    // ---- The pieces, ghost, previews.
    cells_.clear();
    if (!bare) buildBoard(game);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, heatTex_);
        set1i(progCell_, "uHeat", 0);
        set1f(progCell_, "uRim", 1.f);
        set1f(progCell_, "uGain", gain + 0.0001f);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
        // the same pieces at a quarter resolution, blurred: the halo the camera sees around hot things
        glowA_.bind();
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    } else {
        glowA_.bind();
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_BLEND);
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    glActiveTexture(GL_TEXTURE0);
    for (int pass = 0; pass < 4; pass++) {
        const bool h = pass % 2 == 0;
        const ugl::Target& dst = h ? glowB_ : glowA_;
        const ugl::Target& src = h ? glowA_ : glowB_;
        const float step = pass < 2 ? 1.5f : 3.f;
        dst.bind();
        glBindTexture(GL_TEXTURE_2D, src.tex);
        set2f(progBlur_, "uDir", h ? step / src.w : 0.f, h ? 0.f : step / src.h);
        ugl::drawFullscreen();
    }

    // ---- The camera: temperatures to colors.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progMap_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tempT_.tex);
    set1i(progMap_, "uTemp", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lutTex_);
    set1i(progMap_, "uLut", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glowA_.tex);
    set1i(progMap_, "uGlow", 2);
    set1f(progMap_, "uGlowAmt", (0.55f + 0.25f * saturate(warm) + 0.3f * flare_) * (1.f - over_));
    glActiveTexture(GL_TEXTURE0);
    set1f(progMap_, "uLo", lo_);
    set1f(progMap_, "uHi", hi_ - 0.08f * flare_);
    set1f(progMap_, "uMix", smoothstepf(0.f, 1.f, colAmt_));
    set1f(progMap_, "uIso", slow_);
    set1f(progMap_, "uIsoThr", 0.62f);
    set1f(progMap_, "uDim", dimT_);
    set1f(progMap_, "uFade", saturate(f.fade));
    set1f(progMap_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progMap_, "uBridgeCol", ugl::toSrgb(bridge_.color));
    set3f(progMap_, "uBridgeInk", ugl::toSrgb(bridge_.ink));
    ugl::drawFullscreen();

    // ---- The overlay and the HUD.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawOverlay(game, bare);
    drawHud(f.hud, bare ? std::vector<HudRect>{} : f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_heat() {
    static const UniverseEntry e{
        "HEAT", "a thermal camera: pieces land white hot and cool down, the heat conducts and rises, the drop flares",
        [] { return std::unique_ptr<Universe>(new HeatUniverse()); },
        // Energetic, bass-heavy songs suit the heat best; it fits most songs.
        [](const SongLook& l) { return 0.75f + 0.25f * l.bassWeight + 0.2f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

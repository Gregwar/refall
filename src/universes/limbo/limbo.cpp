// LIMBO: a monochrome world of silhouettes in fog.
//
// One world: layered planes of black shapes (dead trees, a forest edge, distant towers and a windmill, hanging
// ropes, a gear, grasses on low hills) fading into a luminous grey fog, a soft light behind it all, low behind the
// well. The pieces are deep black silhouettes standing in front of that light: a piece's cells merge into one shape,
// a thin rim of the fog's glow runs along its edges (brighter where they face the light), and the faint color each
// piece type scatters into its edges is the only color on the stack. The falling piece is the same silhouette.
//
// The world fills up with the play: each cleared row adds something to the landscape, for good (a bird lands on a
// branch, a lantern is lit, a tree grows, a distant tower rises out of the fog, grass comes up, a fence post...), so
// that by the end of a song the world is richer than it began.
//
// The song is the fog. Calm parts are thick fog (the far planes gone, only the nearest shapes dark, a dim diffuse
// light); a chorus thins it (the forest edge and the towers come through, the shapes sway on the beat grid, every two
// beats, the gear turns a notch every two beats); a peak opens it (the far planes clear, the light brighter, the fog
// banks drifting faster, the lanterns brighten on the kicks). The drop tears the fog open, eased in over a beat or two:
// the banks part from the middle and roll away, a vast mountain range appears, the light sinks into a big low sun
// with occluded light shafts through the trees, rim-lit silhouettes, a gust, a large flock taking off, every lantern
// lit, the gears spinning; it holds for the peak (16 s for a drop outside one) and closes slowly. A cleared
// row dissolves into the fog (its edges erode into mist, a few motes drift off); a Tetris sends a flock up from the
// forest. Slow motion is a still, moonlit fog; game over dissolves the stack from the top down as the fog closes in.
// Changes of universe: the fog closes in to the bridge's color.
//
// Cost: one full-resolution float target; the sky and three fog banks are full-screen passes, the silhouettes a few
// thousand instanced quads rebuilt every frame (they sway).
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/limbo/limbo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/limbo/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Cantarell Light (labels, numbers) and EB Garamond Italic (the song, the section, the celebrations), SIL Open Font
// License, see OFL-Cantarell.txt and OFL-EBGaramond.txt.
const unsigned char FONT_SANS[] = {
#include "universes/limbo/Cantarell-Light.ttf.inc"
};
const unsigned char FONT_SERIF[] = {
#include "universes/limbo/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = limbo_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
vec2 V2(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 dirOf(float a) { return {std::cos(a), std::sin(a)}; }
vec2 mad(vec2 a, vec2 d, float s) { return {a.x + d.x * s, a.y + d.y * s}; }

// The moods: the fog near the ground and high up, the light behind it, the silhouettes' black, the pieces' black,
// a lantern's light, the HUD's ink, halo and accent, the slow motion's moonlight, how strong the glow and the
// pieces' rims are, and the seven tints each piece type scatters into its edges.
struct VariantDef {
    const char* name;
    unsigned fogLo, fogHi, glow, sil, ink, lamp, text, halo, accent, night;
    float glowK, rimK;
    bool dark;
    unsigned tint[7]; // I O T S Z J L
};
const VariantDef VARIANTS[] = {
    // neutral grey fog, a white light
    {"ASH", 0xb4b6b8, 0x85898c, 0xf3f3f1, 0x0c0d0e, 0x060607, 0xf1ece4, 0x16181a, 0xd2d4d5, 0x3d4246, 0xbcc6d6,
     0.42f, 1.f, false, {0x8fd0e2, 0xe6d08e, 0xc0a2e6, 0x9ad6a0, 0xec9c9c, 0x98aef0, 0xecae84}},
    // a hint of warm grey, a rosy light
    {"DUSK", 0xb9b3b0, 0x8a8586, 0xf4e8e2, 0x100d0e, 0x080607, 0xf6d8c8, 0x1b1516, 0xd8cfcc, 0x5e4743, 0xc4c2d8,
     0.44f, 1.f, false, {0x92cfe0, 0xe8cc8a, 0xc8a0e2, 0xa0d4a0, 0xf09a9a, 0x9cacee, 0xf0ac80}},
    // cool blue-grey moorland
    {"MOOR", 0xa5b1b8, 0x76838c, 0xe8f1f5, 0x090d10, 0x05070a, 0xe2eef4, 0x111619, 0xc9d3d8, 0x2f4c5a, 0xb0c4dc,
     0.42f, 1.f, false, {0x86d4e4, 0xe2cf92, 0xbaa4ea, 0x92d8aa, 0xea9ca4, 0x92b0f2, 0xe8b08c}},
    // dark: a single faint light far behind
    {"DEEP", 0x353b3f, 0x1b1f22, 0xa6b0b6, 0x040506, 0x020203, 0xd6dde0, 0xd4d9dc, 0x1a1e21, 0xeef1f2, 0x8ea2bc,
     0.75f, 1.4f, true, {0x86d0e4, 0xe2cc88, 0xbea0ea, 0x90d49c, 0xec9494, 0x94acf2, 0xeca880}},
    // grey fog, the light alone in color: a low red sun
    {"EMBER", 0xb2b2b3, 0x848588, 0xf2a48c, 0x0c0c0d, 0x060606, 0xffa47e, 0x161617, 0xd0d0d1, 0xa2412c, 0xbcc4d4,
     0.40f, 1.f, false, {0x8ccfe0, 0xe4cf8e, 0xc0a2e6, 0x98d4a0, 0xf2a08c, 0x98acf0, 0xeeb088}},
    // high key: pale fog, graphic black
    {"PALE", 0xd8d9da, 0xbcbfc1, 0xffffff, 0x101113, 0x08090a, 0xf6f3ee, 0x1a1c1e, 0xe6e7e8, 0x4a4f54, 0xc8d0de,
     0.30f, 0.9f, false, {0x80c8de, 0xdcc480, 0xb898e2, 0x8ccc94, 0xe68e8e, 0x8ca4ec, 0xe4a478}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The ground (board y): the near hills' tops, the mid ground, the forest edge, the far structures' feet.
constexpr float Y_NEAR = -10.95f, Y_MID = -9.7f, Y_EDGE = -7.4f, Y_FAR = -5.2f;
// How many things the play can add to the world (one per cleared row).
constexpr int GROW_COUNT = 48;
// How long a thing takes to grow (seconds).
constexpr float GROW_SECONDS = 2.6f;

// UTF-8 to the font's characters: the middle dot is glyph 127, accented Latin letters lose their accent, anything
// else outside ASCII becomes a '?'.
std::string ascii(const std::string& s) {
    static const char* LATIN = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 128) { o += (char)c; continue; }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB7) { o += '\x7f'; i++; continue; }
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = (unsigned char)s[i + 1];
            if (d >= 0x80 && d <= 0xBF) { o += LATIN[d - 0x80]; i++; continue; }
        }
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

bool LimboUniverse::init(int width, int height) {
    using shaders::withCommon;
    progSky_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SKY_FS).c_str(), "limbo sky");
    progSil_ = ugl::program(shaders::SIL_VS, withCommon(shaders::SIL_FS).c_str(), "limbo silhouettes");
    progBank_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::BANK_FS).c_str(), "limbo fog bank");
    progPiece_ = ugl::program(shaders::CELL_VS, withCommon(shaders::PIECE_FS, true).c_str(), "limbo pieces");
    progMote_ = ugl::program(shaders::MOTE_VS, shaders::MOTE_FS, "limbo motes");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "limbo final");
    progRays_ = ugl::program(ugl::FULLSCREEN_VS, shaders::RAYS_FS, "limbo rays");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "limbo shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "limbo text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, int vec4s, size_t stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < vec4s; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(primVao_, primVbo_, 3, sizeof(Prim));
    instanced(cellVao_, cellVbo_, 3, sizeof(CellInst));
    instanced(moteVao_, moteVbo_, 2, sizeof(MoteInst));
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

    buildFont(fontSans_, FONT_SANS);
    buildFont(fontSerif_, FONT_SERIF);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void LimboUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] limbo font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 10;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        const int cp = ch == 127 ? 0xB7 : ch; // the middle dot in DEL's slot
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

void LimboUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    rayT_.create(std::max(1, width_ / 4), std::max(1, height_ / 4), GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    placeWorld();
}

LimboUniverse::~LimboUniverse() {
    sceneT_.destroy();
    rayT_.destroy();
    for (GLuint p : {progRays_, progSky_, progSil_, progBank_, progPiece_, progMote_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {primVao_, cellVao_, moteVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, primVbo_, cellVbo_, moteVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontSans_, &fontSerif_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The world
// =====================================================================================================

// Where everything stands, from the song's seed: the base world, then the things the play will add, in order.
void LimboUniverse::placeWorld() {
    things_.clear();
    Rng r(seed_ ^ 0x11B0ull);
    const float W = viewH_.x + 1.5f;                    // a little past the screen's edges
    const float m0 = 12.f, m1 = std::max(m0 + 2.f, viewH_.x - 0.8f); // the outer margins (clear of the HUD columns)
    // the near, darkest things stand on the right (the left keeps the song's title and the section clear)
    const float side = 1.f;
    auto add = [&](int kind, int layer, float x, float y, float size, int grow = -1, int host = -1) {
        Thing t;
        t.kind = kind; t.layer = layer; t.x = x; t.y = y; t.size = size;
        t.seed = r.next(); t.phase = r.range(0.f, TAU); t.grow = grow; t.host = host;
        t.shown = grow < 0 ? 1.f : 0.f;
        things_.push_back(t);
        return (int)things_.size() - 1;
    };
    // a place in the outer margins (left: below the title, above the section; mid and far things only)
    auto margin = [&](float s) { return s * r.range(m0, m1); };
    // grass: on the right, or under the left HUD column (clear of the section's lines)
    auto grassX = [&](float s) { return s > 0 ? r.range(6.3f, m1 + 1.f) : -r.range(6.3f, 10.3f); };

    // the ground: low hills near, a mid ground, the forest edge's rise
    for (float x = -W - 2.f; x < W + 2.f; x += r.range(3.5f, 5.5f)) add(K_HILL, L_NEAR, x, Y_NEAR + r.range(-0.15f, 0.2f), r.range(4.f, 6.5f));
    for (float x = -W - 2.f; x < W + 2.f; x += r.range(4.f, 7.f)) add(K_HILL, L_MID, x, Y_MID + r.range(-0.3f, 0.5f), r.range(4.f, 8.f));
    for (float x = -W - 2.f; x < W + 2.f; x += r.range(5.f, 9.f)) add(K_HILL, L_EDGE, x, Y_EDGE + r.range(-0.6f, 0.6f), r.range(6.f, 10.f));
    // the forest edge: pines and bare trees all along
    for (float x = -W; x < W; x += r.range(0.6f, 1.5f)) {
        const float y = Y_EDGE + 0.25f + 0.5f * std::sin(x * 0.21f + 1.3f) + r.range(-0.2f, 0.2f);
        if (r.chance(0.72f)) add(K_PINE, L_EDGE, x, y, r.range(0.7f, 1.35f));
        else add(K_DEADTREE, L_EDGE, x, y, r.range(0.35f, 0.55f));
    }
    // far away: a windmill, a tower
    add(K_MILL, L_FAR, -side * r.range(13.f, 17.f), Y_FAR, r.range(0.9f, 1.1f));
    add(K_TOWER, L_FAR, side * r.range(8.f, 11.f), Y_FAR - 0.3f, r.range(0.9f, 1.2f));
    // the mid ground: a bare tree on the left
    add(K_DEADTREE, L_MID, -side * r.range(14.5f, 17.5f), Y_MID + 0.3f, r.range(0.75f, 0.9f));
    // near: the big bare tree, a lantern hanging from above between it and the HUD column
    const int bigTree = add(K_DEADTREE, L_NEAR, side * r.range(15.f, 16.5f), Y_NEAR + 0.1f, r.range(1.15f, 1.3f));
    add(K_LANTERN, L_NEAR, side * r.range(12.f, 12.8f), viewH_.y + 0.5f, r.range(4.f, 5.5f));
    // grass
    for (int i = 0; i < 4; i++) add(K_GRASS, L_NEAR, grassX(i % 2 ? 1.f : -1.f), Y_NEAR + 0.05f, r.range(0.7f, 1.1f));
    // a bird already waiting
    add(K_BIRD, L_NEAR, 0.f, 0.f, 1.f, -1, bigTree);

    // What the play adds, one thing per cleared row, in this order (a varied rhythm of small and large things,
    // near and far). What hangs from above hangs on the right, in a few slots (the left keeps the title clear).
    const int order[16] = {K_BIRD, K_GRASS, K_DEADTREE, K_TOWER, K_LANTERN, K_BIRD, K_PINE, K_POST,
                           K_WATERTOWER, K_BIRD, K_ROPE, K_DEADTREE, K_GRASS, K_PYLON, K_BIRD, K_MILL};
    const float hang[6] = {13.6f, 11.9f, 19.6f, 12.8f, 18.4f, 14.6f};
    int nHang = 0;
    int g = 0;
    int midTrees[8], nMid = 0;
    for (int k = 0; k < GROW_COUNT; k++) {
        const int kind = order[k % 16];
        const float s = r.chance(0.5f) ? 1.f : -1.f;
        switch (kind) {
        case K_BIRD: {
            // a bird lands on a tree: the big one, or a tree the play grew
            int host = bigTree;
            if (nMid > 0 && r.chance(0.5f)) host = midTrees[r.irange(0, nMid - 1)];
            add(K_BIRD, L_NEAR, 0.f, 0.f, r.range(0.85f, 1.1f), g++, host);
            break;
        }
        case K_GRASS: add(K_GRASS, L_NEAR, grassX(s), Y_NEAR + 0.05f, r.range(0.6f, 1.1f), g++); break;
        case K_LANTERN:
        case K_ROPE:
            if (nHang < 6 && hang[nHang] < m1 + 0.5f) {
                const float x = std::min(hang[nHang++], m1 + 0.5f) + r.range(-0.2f, 0.2f);
                if (kind == K_LANTERN) add(K_LANTERN, L_NEAR, x, viewH_.y + 0.5f, r.range(2.5f, 6.f), g++);
                else add(K_ROPE, L_NEAR, x, viewH_.y + 0.5f, r.range(2.5f, 5.f), g++);
            } else {
                add(K_LANTERN, L_MID, r.range(12.5f, m1), Y_MID + 0.2f, -r.range(1.8f, 2.6f), g++); // on a post
            }
            break;
        case K_DEADTREE: {
            const int i = add(K_DEADTREE, L_MID, margin(s), Y_MID + 0.3f, r.range(0.55f, 0.85f), g++);
            if (nMid < 8) midTrees[nMid++] = i;
            break;
        }
        case K_TOWER: add(K_TOWER, L_FAR, s * r.range(5.f, W - 1.f), Y_FAR - 0.3f, r.range(0.8f, 1.25f), g++); break;
        case K_PYLON: add(K_PYLON, L_FAR, s * r.range(5.f, W - 1.f), Y_FAR, r.range(0.75f, 1.f), g++); break;
        case K_MILL: add(K_MILL, L_FAR, s * r.range(4.f, W - 2.f), Y_FAR - 0.2f, r.range(0.7f, 0.9f), g++); break;
        case K_PINE: add(K_PINE, L_MID, margin(s), Y_MID + 0.3f, r.range(1.4f, 2.f), g++); break;
        case K_POST: add(K_POST, L_MID, margin(s), Y_MID + 0.25f, r.range(0.9f, 1.15f), g++); break;
        case K_WATERTOWER: add(K_WATERTOWER, L_FAR, s * r.range(6.f, W - 2.f), Y_FAR - 0.2f, r.range(0.85f, 1.1f), g++); break;
        default: break;
        }
    }
    grownInit_ = false;
}

// A gnarled branch and its children, recursively; `sway` bends the thinner branches more, `grow` scales it all
// (a tree coming up). The tips are where birds may perch.
void LimboUniverse::branch(std::vector<Prim>& out, Rng& r, vec2 p, float ang, float len, float rad, int depth, float fog,
                           float sway, float grow, std::vector<vec2>* perches) {
    const float bend = sway * (0.05f + 0.07f * (6 - depth));
    const float a1 = ang + r.range(-0.2f, 0.2f) + bend;
    const vec2 mid = mad(p, dirOf(a1), len * 0.5f * grow);
    const float a2 = a1 + r.range(-0.25f, 0.25f) + bend * 0.5f;
    const vec2 end = mad(mid, dirOf(a2), len * 0.5f * grow);
    const float rg = rad * grow;
    out.push_back({{p.x, p.y, mid.x, mid.y}, {rg, rg * 0.86f, 0.f, fog}, {1.f, 0.f, 0.f, 0.f}});
    out.push_back({{mid.x, mid.y, end.x, end.y}, {rg * 0.86f, rg * 0.72f, 0.f, fog}, {1.f, 0.f, 0.f, 0.f}});
    if (depth == 0) {
        if (perches) perches->push_back(end);
        return;
    }
    // a side twig off the middle, now and then
    if (depth >= 2 && r.chance(0.45f)) {
        const float ta = a1 + (r.chance(0.5f) ? 1.f : -1.f) * r.range(0.6f, 1.1f) + bend;
        const vec2 te = mad(mid, dirOf(ta), len * r.range(0.25f, 0.4f) * grow);
        out.push_back({{mid.x, mid.y, te.x, te.y}, {rg * 0.45f, rg * 0.15f, 0.f, fog}, {1.f, 0.f, 0.f, 0.f}});
        if (perches && depth <= 3) perches->push_back(te);
    }
    const int n = r.chance(0.3f) ? 3 : 2;
    for (int i = 0; i < n; i++) {
        const float spread = n == 2 ? (i == 0 ? -1.f : 1.f) * r.range(0.3f, 0.65f) : (i - 1) * r.range(0.45f, 0.7f);
        // branches reach upward and outward (a little gravity pulls the long ones down)
        float ca = a2 + spread;
        ca += (PI * 0.5f - ca) * 0.08f;
        branch(out, r, end, ca, len * r.range(0.64f, 0.8f), rad * r.range(0.6f, 0.7f), depth - 1, fog, sway, grow, perches);
    }
}

void LimboUniverse::tree(std::vector<Prim>& out, const Thing& t, float fog, float sway, std::vector<vec2>* perches) {
    Rng r(t.seed);
    const float g = smoothstepf(0.f, 1.f, t.shown);
    const int depth = t.layer == L_EDGE ? 4 : t.layer == L_MID ? 5 : 6;
    const float lean = r.range(-0.14f, 0.14f) + (t.x > 0 ? 0.04f : -0.04f);
    // roots flaring into the ground
    if (t.layer != L_EDGE) {
        for (int i = 0; i < 3; i++) {
            const float a = PI * (i == 0 ? 0.08f : i == 1 ? 0.92f : r.range(0.3f, 0.7f)) - PI * 0.0f;
            const vec2 e = mad(V2(t.x, t.y + 0.25f * t.size * g), dirOf(-a), 0.75f * t.size * g);
            out.push_back({{t.x, t.y + 0.35f * t.size * g, e.x, e.y}, {0.16f * t.size * g, 0.02f, 0.f, fog}, {1.f, 0.f, 0.f, 0.f}});
        }
    }
    branch(out, r, V2(t.x, t.y - 0.3f), PI * 0.5f + lean, 2.6f * t.size, 0.2f * t.size, depth, fog, sway, g, perches);
}

// Every thing's primitives for this frame. `sway` (radians-ish, -1..1 scaled) bends trees and grass and swings the
// ropes; `beatStep` turns the gears.
void LimboUniverse::buildWorld(float sway, float beatStep) {
    for (auto& l : layer_) l.clear();
    glows_.clear();
    flockPrims_.clear();
    const VariantDef& V = VARIANTS[variant_];
    (void)V;
    for (auto& p : perches_) p.clear();
    auto prim = [&](int layer, float x0, float y0, float x1, float y1, float r0, float r1, int kind, float alpha = 1.f,
                    float angle = 0.f, float tone = 0.f) {
        layer_[layer].push_back({{x0, y0, x1, y1}, {r0, r1, (float)kind, layerFog_[layer]}, {alpha, angle, 0.f, tone}});
    };
    // a hanging line from (x, top) down `len`, swinging like a pendulum; returns its end
    auto rope = [&](const Thing& t, float len, float a) {
        const int N = 10;
        vec2 p = V2(t.x, t.y);
        for (int i = 0; i < N; i++) {
            const float u = (i + 1.f) / N;
            const float ang = -PI * 0.5f + a * (0.4f + 0.6f * u);
            const vec2 q = mad(p, dirOf(ang), len / N);
            prim(t.layer, p.x, p.y, q.x, q.y, 0.045f, 0.045f, 0, 1.f);
            p = q;
        }
        return p;
    };
    auto lantern = [&](int layer, vec2 top, float alpha, float s, float glowK) {
        // a cap, a frame with its glass lit, a soft halo around it
        const float w = 0.2f * s, h = 0.3f * s;
        prim(layer, top.x, top.y - 0.02f, top.x, top.y - 0.12f * s, 0.03f * s, 0.03f * s, 0, alpha);
        layer_[layer].push_back({{top.x, top.y - 0.1f * s, top.x, top.y - 0.22f * s}, {w * 1.1f, 0.f, 4.f, layerFog_[layer]}, {alpha, 0.f, 0.f, 0.f}});
        prim(layer, top.x, top.y - 0.22f * s - h, 0, 0, w, h, 2, alpha);
        prim(layer, top.x, top.y - 0.22f * s - h, 0, 0, w * 0.62f, h * 0.72f, 2, alpha, 0.f, 0.85f);
        prim(layer, top.x, top.y - 0.22f * s - 2.f * h - 0.04f * s, 0, 0, w * 0.8f, 0.04f * s, 2, alpha);
        glows_.push_back({{top.x, top.y - 0.22f * s - h, 0, 0}, {1.5f * s, 0.f, 5.f, 0.f}, {alpha * glowK, 0.f, 0.f, 0.f}});
    };
    const float lampK = 0.3f + 0.25f * lampKick_ + 0.45f * part_;

    for (size_t i = 0; i < things_.size(); i++) {
        const Thing& t = things_[i];
        if (t.shown <= 0.002f) continue;
        const float g = smoothstepf(0.f, 1.f, t.shown);
        // each its own share of the motion; the drop's gust bends everything outward, away from the light
        const float sw = sway * (0.75f + 0.5f * std::sin(t.phase)) + gust_ * 0.55f * (t.x > 0.f ? -1.f : 1.f);
        std::vector<vec2>* perch = i < 64 ? &perches_[i] : nullptr;
        Rng r(t.seed);
        switch (t.kind) {
        case K_HILL: prim(t.layer, t.x, t.y - t.size, 0, 0, t.size, 0, 1, g); break;
        case K_DEADTREE: tree(layer_[t.layer], t, layerFog_[t.layer], sw * (t.layer == L_EDGE ? 0.4f : 1.f), perch); break;
        case K_PINE: {
            const float s = t.size * g, h = 2.6f * s;
            prim(t.layer, t.x, t.y - 0.3f, t.x, t.y + h * 0.95f, 0.07f * s, 0.03f * s, 0, g);
            const int tiers = 4;
            const float bend = sw * 0.08f * (t.layer == L_EDGE ? 0.4f : 1.f);
            for (int k = 0; k < tiers; k++) {
                const float u = k / (float)tiers;
                const float baseY = t.y + h * (0.12f + 0.62f * u);
                const float apexY = baseY + h * (0.42f - 0.08f * u);
                const float hw = s * (0.62f - 0.42f * u) * r.range(0.85f, 1.1f);
                const float ax = t.x + bend * (apexY - t.y);
                layer_[t.layer].push_back({{ax, apexY, t.x + bend * (baseY - t.y) * 0.5f, baseY}, {hw, 0.f, 4.f, layerFog_[t.layer]}, {g, 0.f, 0.f, 0.f}});
            }
            break;
        }
        case K_GRASS: {
            const int n = 10 + (int)(t.size * 14.f);
            for (int k = 0; k < n; k++) {
                const float x = t.x + r.range(-1.1f, 1.1f) * t.size;
                const float h = r.range(0.35f, 1.25f) * t.size * g * (1.f - 0.5f * std::fabs(x - t.x) / (1.1f * t.size));
                const float lean = r.range(-0.35f, 0.35f) + sw * 0.25f * (0.6f + 0.4f * std::sin(t.phase + k));
                prim(t.layer, x, t.y - 0.1f, x + lean * h, t.y + h, 0.04f * t.size, 0.004f, 0, 1.f);
            }
            break;
        }
        case K_POST: {
            const float h = 1.5f * t.size;
            const float lean = r.range(-0.08f, 0.08f);
            prim(t.layer, t.x, t.y - 0.3f, t.x + lean, t.y + h * g, 0.075f, 0.06f, 0, g);
            // a slack wire to the next post (drawn from each post toward +x)
            if (g > 0.98f) {
                const float len = r.range(1.4f, 2.1f);
                vec2 a = V2(t.x + lean * 0.8f, t.y + h * 0.8f);
                for (int k = 0; k < 6; k++) {
                    const float u0 = k / 6.f, u1 = (k + 1) / 6.f;
                    const float s0 = -0.25f * 4.f * u0 * (1 - u0), s1 = -0.25f * 4.f * u1 * (1 - u1);
                    prim(t.layer, a.x + len * u0, a.y + s0, a.x + len * u1, a.y + s1, 0.018f, 0.018f, 0, 0.9f);
                }
            }
            break;
        }
        case K_ROPE: {
            const float a = sw * 0.09f;
            const vec2 e = rope(t, t.size * (0.3f + 0.7f * g), a);
            if (t.grow >= 0 || (t.seed & 1)) {
                // a gear hanging at its end, turning a notch every two beats
                const float R = 0.42f + 0.25f * (float)((t.seed >> 8) % 100) / 100.f;
                prim(t.layer, e.x, e.y - R * 0.95f, 0, 0, R * g, 9.f, 3, g, gearAngle_ * (t.seed & 2 ? 1.f : -1.f) / R * 0.5f + t.phase);
            } else {
                prim(t.layer, e.x, e.y - 0.18f, 0, 0, 0.13f, 0.2f, 2, g);
            }
            break;
        }
        case K_GEARS: {
            // a long chain holding two meshed gears
            const vec2 e = rope(t, t.size, sw * 0.05f);
            const float R1 = 0.8f, R2 = 0.5f;
            const vec2 c1 = V2(e.x, e.y - R1);
            const float link = (R1 + R2) * 1.02f;
            const vec2 c2 = mad(c1, dirOf(-0.65f), link);
            prim(t.layer, c1.x, c1.y, 0, 0, R1, 12.f, 3, g, gearAngle_);
            prim(t.layer, c2.x, c2.y, 0, 0, R2, 8.f, 3, g, -gearAngle_ * R1 / R2 + 0.2f);
            break;
        }
        case K_LANTERN: {
            if (t.size < 0.f) {
                // on a post with an arm
                const float h = -t.size;
                prim(t.layer, t.x, t.y - 0.3f, t.x, t.y + h, 0.07f, 0.05f, 0, g);
                const float dx = t.x > 0 ? -0.55f : 0.55f;
                prim(t.layer, t.x, t.y + h - 0.05f, t.x + dx, t.y + h + 0.05f, 0.03f, 0.03f, 0, g);
                const float swing = sw * 0.03f;
                lantern(t.layer, V2(t.x + dx + swing, t.y + h + 0.02f), g, 0.9f, lampK);
            } else {
                const vec2 e = rope(t, t.size, sw * 0.07f);
                lantern(t.layer, e, g, 1.1f, lampK);
            }
            break;
        }
        case K_BIRD: break; // after their hosts (below)
        case K_TOWER: {
            const float s = t.size, h = 7.f * s;
            const float y0 = t.y - h * (1.f - g); // rising out of the fog
            const int style = (int)(t.seed % 3);
            if (style == 0) { // a tall house of stone, a pointed roof
                prim(t.layer, t.x, y0 + h * 0.4f, 0, 0, 0.55f * s, h * 0.4f, 2, 1.f);
                layer_[t.layer].push_back({{t.x, y0 + h, t.x, y0 + h * 0.8f}, {0.7f * s, 0.f, 4.f, layerFog_[t.layer]}, {1.f, 0.f, 0.f, 0.f}});
                prim(t.layer, t.x, y0 + h, t.x, y0 + h + 0.8f * s, 0.03f, 0.02f, 0, 1.f);
            } else if (style == 1) { // a spire
                layer_[t.layer].push_back({{t.x, y0 + h * 1.1f, t.x, y0}, {0.8f * s, 0.f, 4.f, layerFog_[t.layer]}, {1.f, 0.f, 0.f, 0.f}});
            } else { // two chimneys
                prim(t.layer, t.x, y0 + h * 0.45f, 0, 0, 0.32f * s, h * 0.45f, 2, 1.f);
                prim(t.layer, t.x + 1.1f * s, y0 + h * 0.33f, 0, 0, 0.26f * s, h * 0.33f, 2, 1.f);
                prim(t.layer, t.x + 0.55f * s, y0 + h * 0.12f, 0, 0, 1.0f * s, h * 0.12f, 2, 1.f);
            }
            break;
        }
        case K_PYLON: {
            const float s = t.size, h = 6.5f * s;
            const float y0 = t.y - h * (1.f - g);
            const float b = 1.1f * s;
            prim(t.layer, t.x - b, y0, t.x - 0.12f * s, y0 + h, 0.06f, 0.04f, 0, 1.f);
            prim(t.layer, t.x + b, y0, t.x + 0.12f * s, y0 + h, 0.06f, 0.04f, 0, 1.f);
            for (int k = 0; k < 4; k++) {
                const float u0 = k / 4.f, u1 = (k + 1) / 4.f;
                const float w0 = lerpf(b, 0.12f * s, u0), w1 = lerpf(b, 0.12f * s, u1);
                prim(t.layer, t.x - w0, y0 + h * u0, t.x + w1, y0 + h * u1, 0.025f, 0.025f, 0, 1.f);
                prim(t.layer, t.x + w0, y0 + h * u0, t.x - w1, y0 + h * u1, 0.025f, 0.025f, 0, 1.f);
            }
            for (int k = 0; k < 2; k++) {
                const float yy = y0 + h * (0.72f + 0.16f * k), aw = s * (1.4f - 0.4f * k);
                prim(t.layer, t.x - aw, yy, t.x + aw, yy, 0.04f, 0.04f, 0, 1.f);
                prim(t.layer, t.x - aw, yy, t.x - aw, yy - 0.35f * s, 0.015f, 0.015f, 0, 1.f);
                prim(t.layer, t.x + aw, yy, t.x + aw, yy - 0.35f * s, 0.015f, 0.015f, 0, 1.f);
            }
            break;
        }
        case K_WATERTOWER: {
            const float s = t.size, h = 4.2f * s;
            const float y0 = t.y - (h + 1.6f * s) * (1.f - g);
            const float b = 0.9f * s;
            for (int k = -1; k <= 1; k += 2) {
                prim(t.layer, t.x + k * b, y0, t.x + k * b * 0.75f, y0 + h, 0.05f, 0.05f, 0, 1.f);
                prim(t.layer, t.x + k * b * 0.4f, y0, t.x + k * b * 0.3f, y0 + h, 0.04f, 0.04f, 0, 1.f);
            }
            for (int k = 0; k < 3; k++) {
                const float u0 = k / 3.f, u1 = (k + 1) / 3.f;
                prim(t.layer, t.x - lerpf(b, b * 0.75f, u0), y0 + h * u0, t.x + lerpf(b, b * 0.75f, u1), y0 + h * u1, 0.022f, 0.022f, 0, 1.f);
                prim(t.layer, t.x + lerpf(b, b * 0.75f, u0), y0 + h * u0, t.x - lerpf(b, b * 0.75f, u1), y0 + h * u1, 0.022f, 0.022f, 0, 1.f);
            }
            prim(t.layer, t.x, y0 + h + 0.7f * s, 0, 0, 1.0f * s, 0.7f * s, 2, 1.f);
            layer_[t.layer].push_back({{t.x, y0 + h + 2.1f * s, t.x, y0 + h + 1.4f * s}, {1.1f * s, 0.f, 4.f, layerFog_[t.layer]}, {1.f, 0.f, 0.f, 0.f}});
            break;
        }
        case K_MILL: {
            // a windmill's tower and its four bare sails, turning slowly (faster as the song rises)
            const float s = t.size, h = 5.5f * s;
            const float y0 = t.y - (h + 3.5f * s) * (1.f - g);
            layer_[t.layer].push_back({{t.x, y0 + h + 0.6f * s, t.x, y0}, {1.05f * s, 0.f, 4.f, layerFog_[t.layer]}, {1.f, 0.f, 0.f, 0.f}});
            prim(t.layer, t.x, y0 + h * 0.35f, 0, 0, 0.75f * s, h * 0.35f, 2, 1.f);
            const vec2 hub = V2(t.x, y0 + h * 0.88f);
            for (int k = 0; k < 4; k++) {
                const float a = millAngle_ + t.phase + k * PI * 0.5f;
                const vec2 e = mad(hub, dirOf(a), 3.2f * s);
                prim(t.layer, hub.x, hub.y, e.x, e.y, 0.06f, 0.03f, 0, 1.f);
                // the sail's lattice: a thin frame alongside the arm
                const vec2 n = dirOf(a + PI * 0.5f);
                const vec2 f0 = mad(mad(hub, dirOf(a), 0.9f * s), n, 0.42f * s), f1 = mad(e, n, 0.42f * s);
                prim(t.layer, f0.x, f0.y, f1.x, f1.y, 0.025f, 0.025f, 0, 1.f);
                for (int j = 0; j < 4; j++) {
                    const float u = 0.9f / 3.2f + (1.f - 0.9f / 3.2f) * (j / 3.f);
                    const vec2 q0 = mad(hub, dirOf(a), 3.2f * s * u), q1 = mad(q0, n, 0.42f * s);
                    prim(t.layer, q0.x, q0.y, q1.x, q1.y, 0.018f, 0.018f, 0, 1.f);
                }
            }
            prim(t.layer, hub.x, hub.y, 0, 0, 0.16f * s, 0, 1, 1.f);
            break;
        }
        default: break;
        }
    }
    // the birds, on their hosts' branches (a perch picked from the bird's seed)
    for (const Thing& t : things_) {
        if (t.kind != K_BIRD || t.shown <= 0.002f || t.host < 0 || t.host >= 64) continue;
        const std::vector<vec2>& ps = perches_[t.host];
        if (ps.empty()) continue;
        Rng r(t.seed);
        // prefer the higher, outer tips
        int best = 0;
        float bestScore = -1e9f;
        for (int k = 0; k < 6; k++) {
            const int idx = r.irange(0, (int)ps.size() - 1);
            const float sc = ps[idx].y + 0.3f * std::fabs(ps[idx].x) + r.range(0.f, 2.f);
            if (sc > bestScore) { bestScore = sc; best = idx; }
        }
        const vec2 p0 = ps[best];
        const int lay = things_[t.host].layer;
        const float s = 0.75f * t.size * (lay == L_NEAR ? 1.f : 0.7f);
        // landing: it glides in from above during the first part of its growth
        const float land = smoothstepf(0.f, 1.f, t.shown);
        const float dir = (t.seed & 4) ? 1.f : -1.f;
        const vec2 p = V2(p0.x + dir * 3.f * (1.f - land), p0.y + 0.16f * s + 2.2f * (1.f - land) * (1.f - land));
        const float alpha = smoothstepf(0.f, 0.25f, t.shown);
        if (land < 0.97f) {
            const float flap = std::sin(worldTime_ * 14.f + t.phase) * 0.6f * (1.f - land);
            for (int k = -1; k <= 1; k += 2) {
                const vec2 w = mad(p, dirOf(PI * 0.5f - k * (1.1f - flap)), 0.36f * s);
                prim(lay, p.x, p.y, w.x, w.y, 0.05f * s, 0.015f * s, 0, alpha);
            }
        }
        // body, head, tail
        prim(lay, p.x, p.y, p.x - dir * 0.12f * s, p.y + 0.05f * s, 0.11f * s, 0.09f * s, 0, alpha);
        prim(lay, p.x - dir * 0.15f * s, p.y + 0.16f * s, 0, 0, 0.07f * s, 0, 1, alpha);
        prim(lay, p.x + dir * 0.06f * s, p.y - 0.02f * s, p.x + dir * 0.32f * s, p.y - 0.12f * s, 0.05f * s, 0.015f * s, 0, alpha);
        prim(lay, p.x - dir * 0.2f * s, p.y + 0.17f * s, p.x - dir * 0.28f * s, p.y + 0.15f * s, 0.022f * s, 0.006f * s, 0, alpha);
    }
    // the flock of a Tetris, flying off over the forest
    for (const Bird& b : flock_) {
        const float a = smoothstepf(0.f, 0.4f, b.t) * (1.f - smoothstepf(7.5f, 10.f, b.t));
        const float flap = std::sin(b.flap) * 0.55f;
        for (int k = -1; k <= 1; k += 2) {
            const vec2 w = mad(b.p, dirOf(PI * 0.5f - k * (1.2f - flap)), 0.3f * b.size);
            flockPrims_.push_back({{b.p.x, b.p.y, w.x, w.y}, {0.04f * b.size, 0.012f * b.size, 0.f, layerFog_[L_MID]}, {a, 0.f, 0.f, 0.f}});
        }
        flockPrims_.push_back({{b.p.x, b.p.y, 0, 0}, {0.06f * b.size, 0.f, 1.f, layerFog_[L_MID]}, {a, 0.f, 0.f, 0.f}});
    }
    (void)beatStep;
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void LimboUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.4f) v = 3;          // DEEP
        else if (t.hasSong && t.brightness > 0.68f) v = 5;               // PALE
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const uint64_t old = seed_;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x1B0ull);
    (void)seconds;
    if (old != seed_) placeWorld();
}

void LimboUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropMode_ = false; // (the parting closes in render, once a drop's hold is over)
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
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.5f, 7.f);
    (void)impact;
}

// The drop: the fog parts on a vast landscape and the light swells into a low sun (both eased in, see render).
void LimboUniverse::drop(float strength) {
    dropMode_ = true;
    dropHold_ = 16.f; // a drop called outside a peak (or a short peak) still holds its look this long
    swellGoal_ = std::max(swellGoal_, 0.7f + 0.3f * strength);
    gustGoal_ = 1.f;
    // a large flock takes off from the forest edge on both sides and crosses the sky, slowly
    for (int k = 0; k < 40; k++) {
        const float s = k % 2 ? 1.f : -1.f;
        Bird b;
        b.p = V2(s * rng_.range(3.f, 17.f), Y_EDGE + rng_.range(1.f, 3.5f));
        b.v = V2(s * rng_.range(0.7f, 1.6f), rng_.range(0.6f, 1.3f));
        b.flap = rng_.range(0.f, TAU);
        b.t = -rng_.range(0.2f, 2.2f);
        b.size = rng_.range(0.75f, 1.15f);
        flock_.push_back(b);
    }
}

// A new section: one piece's rim catches the light.
void LimboUniverse::refresh(float strength) {
    int n = 0, pick = -1;
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] && rng_.irange(0, n++) == 0) pick = ids_[y][x];
    if (pick < 0) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x] == pick) glint_[y][x] = std::max(glint_[y][x], 0.3f + 0.3f * strength);
}

void LimboUniverse::levelUp() { celebrate_ = std::max(celebrate_, 0.5f); }

vec2 LimboUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints LimboUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = lerp(lin(V.fogLo), lin(V.fogLo) + lin(V.glow) * V.glowK, 0.35f);
    h.light = !V.dark;
    h.ink = lin(V.ink);
    h.accent = V.dark ? lin(V.glow) : lin(V.accent);
    h.styles = TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void LimboUniverse::syncIds(const Game& g) {
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

void LimboUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    const VariantDef& V = VARIANTS[variant_];
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            glint_[c.y][c.x] = 0.f;
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a breath of mist pushed out from under the piece, at its lowest cells
        int low = -1;
        for (auto& c : ev.cells) low = std::max(low, c.y);
        if (low < Game::HIDDEN) break;
        float x0 = 99.f, x1 = -99.f;
        for (auto& c : ev.cells)
            if (c.y == low) { x0 = std::min(x0, (float)c.x); x1 = std::max(x1, (float)c.x); }
        const vec3 a = layout::cellPos(x0, (float)low), b = layout::cellPos(x1, (float)low);
        const float y = a.y - 0.5f;
        for (int k = 0; k < 7; k++) {
            const float sgn = k % 2 ? 1.f : -1.f;
            const float x = sgn > 0 ? b.x + 0.4f : a.x - 0.4f;
            motes_.push_back({V2(x, y + rng_.range(0.f, 0.25f)), V2(sgn * rng_.range(0.8f, 1.8f), rng_.range(0.1f, 0.5f)),
                              rng_.range(0.22f, 0.4f), 0.3f, rng_.range(0.45f, 0.6f), 0.f, 0});
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        int lowest = -1, highest = Game::H;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            lowest = std::max(lowest, c.y);
            highest = std::min(highest, c.y);
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && rows[c.y + 1] && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && rows[c.y - 1] && ids_[c.y - 1][c.x] == id) m |= 8;
                if ((m & 1) && (m & 4) && c.x > 0 && c.y < Game::H - 1 && ids_[c.y + 1][c.x - 1] == id) m |= 16;
                if ((m & 2) && (m & 4) && c.x < Game::W - 1 && c.y < Game::H - 1 && ids_[c.y + 1][c.x + 1] == id) m |= 32;
                if ((m & 1) && (m & 8) && c.x > 0 && c.y > 0 && ids_[c.y - 1][c.x - 1] == id) m |= 64;
                if ((m & 2) && (m & 8) && c.x < Game::W - 1 && c.y > 0 && ids_[c.y - 1][c.x + 1] == id) m |= 128;
            }
            fading_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.f, rng_.range(0.f, 50.f)});
            // the row's motes drifting off into the fog
            for (int k = 0; k < 2; k++)
                motes_.push_back({V2(p.x + rng_.range(-0.45f, 0.45f), p.y + rng_.range(-0.4f, 0.4f)),
                                  V2(rng_.range(-0.5f, 0.5f), rng_.range(0.5f, 1.4f)), rng_.range(0.04f, 0.09f), 0.8f,
                                  rng_.range(0.5f, 0.8f), 0.f, 1});
            if ((c.x + c.y) % 2 == 0)
                motes_.push_back({V2(p.x, p.y), V2(rng_.range(-0.3f, 0.3f), rng_.range(0.3f, 0.7f)), rng_.range(0.5f, 0.75f),
                                  0.16f, 0.45f, 0.f, 0});
        }
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            float ng[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(ng[dst], glint_[y], sizeof(glint_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(glint_, ng, sizeof(glint_));
        }
        // a Tetris: a flock rises from the forest edge and flies off, the light warms for a moment
        if (ev.count >= 4) {
            celebrate_ = 1.f;
            const float s = rng_.chance(0.5f) ? 1.f : -1.f;
            const float x0 = s * rng_.range(7.f, 11.f);
            for (int k = 0; k < 15; k++) {
                Bird b;
                b.p = V2(x0 + rng_.range(-2.5f, 2.5f), Y_EDGE + 2.f + rng_.range(-0.5f, 1.2f));
                b.v = V2(s * rng_.range(1.6f, 2.6f), rng_.range(0.9f, 1.6f));
                b.flap = rng_.range(0.f, TAU);
                b.t = -rng_.range(0.f, 0.9f);
                b.size = rng_.range(0.7f, 1.05f);
                flock_.push_back(b);
            }
        }
        (void)V;
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack dissolves into the fog from the top down
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
            }
            fading_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.7f + 0.09f * (c.y - Game::HIDDEN), rng_.range(0.f, 50.f)});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// The pieces
// =====================================================================================================

void LimboUniverse::buildBoard(const Game& g) {
    const VariantDef& V = VARIANTS[variant_];
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto cell = [&](std::vector<CellInst>& out, float x, float y, float half, int mask, int type, float alpha, float kind,
                    float dissolve, float glint, float seed) {
        if (alpha <= 0.003f) return;
        const vec3 c = lin(V.tint[std::clamp(type, 0, 6)]);
        out.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, alpha}, {kind, dissolve, glint, seed}});
    };
    // The ghost: a faint shadow where the piece will land.
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
                cell(ghost_, w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, 1.f, 1.f, 0.f, 0.f, 0.f);
            }
        }
    }
    // Cells dissolving into the fog (a clear in real time; game over from the top down), behind the stack: the
    // rows sliding down over them cover them.
    for (const Fading& l : fading_) {
        const float k = l.t < l.delay ? 0.f : smoothstepf(0.f, l.delay > 0.f ? 0.9f : 0.38f, l.t - l.delay);
        cell(cells_, l.x, l.y + 0.25f * k * k, 0.5f, l.mask, l.type, 1.f, 0.f, std::max(k, 0.001f), 0.f, l.seed);
    }
    // Locked pieces.
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
            cell(cells_, p.x, p.y, 0.5f, maskOf(x, y, same), c.type, 1.f, 0.f, 0.f, glint_[y][x], 0.f);
        }
    // The falling piece: the same silhouette.
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
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(cells_, w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, 1.f, 0.f, 0.f, 0.f, 0.f);
        }
    }
    // Previews.
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
            cell(cells_, px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, alpha, 0.f, 0.f, 0.f, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float LimboUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void LimboUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                              std::vector<float>& out) const {
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

void LimboUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", haloA_);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void LimboUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// Quiet lettering in the fog, each line with a soft halo of the fog's light; the bonus gauge a fine ring.
void LimboUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink = hex(V.text), halo = hex(V.halo), accent = hex(V.accent);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float q[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                   {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : q) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }

    // the bonus gauge: a fine ring, its charge a firmer stroke
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 96;
        const float th = std::max(1.f, 1.3f * s), th2 = std::max(2.f, 3.4f * s);
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < N; i++) {
                const float a0 = TAU * i / N, a1 = TAU * (i + 1) / N;
                const bool on = (i + 0.5f) / N <= fill;
                // a soft halo of the fog under the ring, then the ring
                const float w = pass == 0 ? (on ? th2 : th) + 5.f * s : (on ? th2 : th);
                const vec3 col = pass == 0 ? halo : on ? accent : ink;
                const float al = (pass == 0 ? 0.35f : (on ? 0.95f : (g.bonusReady() ? 0.7f : 0.38f))) * vis * (1.f - 0.5f * dimT_);
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

    std::vector<float> q;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        // the bottom corner's lines sit over the dark hills: a fuller halo there
        haloA_ = (h.role == HUD_UNIVERSE || h.role == HUD_SECTION) ? 0.95f : 0.5f;
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontSans_, upper(h.text), base * 0.86f, base * 0.3f, base * 0.08f);
            drawText(fontSans_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        case HUD_SECTION:
            place(fontSerif_, h.text, base * 1.05f, base * 0.06f, -base * 0.1f);
            drawText(fontSerif_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontSerif_, h.text, base * 1.12f, 0.f, -base * 0.12f);
            drawText(fontSerif_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontSans_, h.text, base * 0.8f, base * 0.12f);
            drawText(fontSans_, q, ink, halo, saturate(alpha * 1.3f));
            continue;
        case HUD_LABEL:
            place(fontSans_, upper(h.text), base * 0.8f, base * 0.34f);
            drawText(fontSans_, q, ink, halo, saturate(alpha * 1.3f));
            continue;
        case HUD_VALUE:
            place(fontSans_, h.text, base * 1.12f, base * 0.04f, -base * 0.08f);
            drawText(fontSans_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontSerif_, h.text, base * 1.05f, base * 0.04f);
            drawText(fontSerif_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontSans_, h.text, base * 0.9f, 0.f);
            drawText(fontSans_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontSerif_, h.text, base * 1.2f, base * 0.1f, -base * 0.15f);
            drawText(fontSerif_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontSans_, h.text, base * 0.85f, base * 0.12f);
            drawText(fontSans_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontSerif_, h.text, base * 1.1f, base * 0.04f);
                drawText(fontSerif_, q, ink, halo, alpha);
            } else {
                place(fontSans_, h.text, base * 0.84f, base * 0.06f);
                drawText(fontSans_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontSans_, h.text, base * 0.82f, base * 0.02f);
            drawText(fontSans_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void LimboUniverse::drawPrims(const std::vector<Prim>& v) {
    if (v.empty()) return;
    glBindVertexArray(primVao_);
    glBindBuffer(GL_ARRAY_BUFFER, primVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Prim), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void LimboUniverse::drawBank(float density, float y, float thick, float drift, float scale, float layer, float bright) {
    if (density <= 0.003f) return;
    glUseProgram(progBank_);
    set1f(progBank_, "uDensity", density);
    set1f(progBank_, "uY", y);
    set1f(progBank_, "uThick", thick);
    set2f(progBank_, "uDrift", drift, 0.f);
    set1f(progBank_, "uScale", scale);
    set1f(progBank_, "uLayer", layer);
    set1f(progBank_, "uBright", bright);
    set1f(progBank_, "uPart", part_);
    ugl::drawFullscreen();
}

void LimboUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            fading_.erase(std::remove_if(fading_.begin(), fading_.end(), [](const Fading& l) { return l.delay > 0.f; }), fading_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 1.2f : 0.8f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging: the fog.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.15f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the drop's parting, eased twice (it starts gently, with no jump, and settles); the swell follows it in and
    // settles over several seconds
    // (held for the whole peak after a drop, or dropHold_ seconds for a drop outside one; then it closes slowly)
    dropHold_ = std::max(0.f, dropHold_ - dt);
    partGoal_ = (overT_ < 0.f && ((dropMode_ && level_ == 2) || dropHold_ > 0.f)) ? 1.f : 0.f;
    const float partRate = partGoal_ > partMid_ ? 2.6f : 0.3f;
    partMid_ = approach(partMid_, partGoal_, partRate, dt);
    part_ = approach(part_, partMid_, partRate, dt);
    gust_ = approach(gust_, gustGoal_, 1.6f, dt);
    gustGoal_ = approach(gustGoal_, 0.f, 0.3f, dt);
    gearSpin_ += dt * 1.4f * part_;
    swell_ = approach(swell_, swellGoal_, 1.8f, dt);
    swellGoal_ = approach(swellGoal_, 0.f, 0.22f, dt);
    celebrate_ = approach(celebrate_, 0.f, 0.6f, dt);
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 2.5f, dt);
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (wp > 0.3f && kick > lampKick_) lampKick_ = kick * wp;
    lampKick_ = approach(lampKick_, 0.f, 3.f, dt);
    kickPrev_ = kick;

    // how thick the fog is: calm thick, a chorus thinner, a peak open, the drop parting it; slow motion and a game
    // over close it in
    float fog = lerpf(lerpf(lerpf(1.f, 0.72f, wm), 0.5f, wp), 0.06f, part_);
    fog = lerpf(fog, 1.1f, std::max(0.6f * slow_, over_ * 0.8f));
    fog = std::clamp(fog, 0.f, 1.2f);
    layerFog_[L_FAR] = std::min(0.98f, 0.4f + 0.58f * fog);
    layerFog_[L_EDGE] = std::min(0.96f, 0.24f + 0.64f * fog);
    layerFog_[L_MID] = std::min(0.9f, 0.1f + 0.56f * fog);
    layerFog_[L_NEAR] = 0.02f + 0.2f * fog;
    const float glow = (V.glowK * (lerpf(0.75f, 1.f, wm) + 0.18f * wp + 0.22f * part_ + 0.08f * swell_ + 0.12f * celebrate_)) *
                       (1.f - 0.45f * slow_);
    drift_ += wdt * (0.18f + 0.25f * wm + 0.45f * wp);
    millAngle_ += wdt * (0.06f + 0.12f * wm + 0.2f * wp + 0.5f * part_);

    // ---- Motion on the beat grid: the shapes sway to one side, then the other, every two beats (a breeze when calm).
    float sway = 0.35f * std::sin(worldTime_ * 0.35f);
    float notch = worldTime_ * 0.1f;
    if (music.hasSong) {
        sway = sway * (1.f - wm) + (0.28f * wm + 0.22f * wp) * (float)std::sin(PI * music.beat * 0.5) + 0.12f * std::sin(worldTime_ * 0.35f);
        const double b2 = music.beat * 0.5;
        const float fr = (float)(b2 - std::floor(b2));
        notch = (float)std::floor(b2) + smoothstepf(0.f, 0.35f, fr);
    }
    sway *= 1.f - over_;
    gearAngle_ = notch * (TAU / 24.f) + gearSpin_;

    // ---- The world filling up with the play.
    grownTarget_ = std::min(game.lines(), GROW_COUNT);
    for (Thing& t : things_) {
        if (t.grow < 0) continue;
        const float target = t.grow < grownTarget_ ? 1.f : 0.f;
        if (!grownInit_) t.shown = target;
        else if (t.shown < target) t.shown = std::min(target, t.shown + wdt / GROW_SECONDS);
        else if (t.shown > target) t.shown = std::max(target, t.shown - dt / 1.5f);
    }
    grownInit_ = true;

    // ---- Particles and the flock (real time, even in slow motion).
    for (Mote& m : motes_) {
        m.t += dt;
        m.p = mad(m.p, m.v, dt);
        m.v.x *= std::exp(-1.5f * dt);
        m.v.y *= std::exp(-0.8f * dt);
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.t > m.life; }), motes_.end());
    for (Bird& b : flock_) {
        b.t += wdt;
        if (b.t < 0.f) continue;
        b.p = mad(b.p, b.v, wdt);
        b.v.y = approach(b.v.y, 0.35f, 0.4f, wdt);
        b.flap += wdt * 11.f;
    }
    flock_.erase(std::remove_if(flock_.begin(), flock_.end(), [](const Bird& b) { return b.t > 10.f; }), flock_.end());
    for (Fading& l : fading_) l.t += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(),
                                 [](const Fading& l) { return l.t > l.delay + (l.delay > 0.f ? 0.95f : 0.45f); }),
                  fading_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    buildWorld(sway, notch);
    cells_.clear();
    ghost_.clear();
    buildBoard(game);

    // ---- The scene.
    const vec3 fogLo = lin(V.fogLo), fogHi = lin(V.fogHi), glowCol = lin(V.glow);
    // the drop: the light sinks into a low sun near the horizon, behind the well, and its halo widens
    const float sunY = 1.5f - 2.3f * part_;
    const float sunR = lerpf(10.5f, 8.f, wp) + 0.5f * part_;
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    auto common = [&](GLuint p) {
        set4f(p, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(p, "uFogLo", fogLo);
        set3f(p, "uFogHi", fogHi);
        set3f(p, "uGlowCol", glowCol);
        set4f(p, "uSun", 0.f, sunY, sunR, glow);
        set1f(p, "uSeed", seedU);
        set1f(p, "uTime", worldTime_);
    };
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    sceneT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glUseProgram(progSky_);
    common(progSky_);
    set1f(progSky_, "uReveal", part_ * (1.f - over_));
    set1f(progSky_, "uFar", saturate(1.f - fog));
    set1f(progSky_, "uShaft", (0.1f * wp + 0.15f * part_ + 0.1f * swell_) * (1.f - slow_));
    set1f(progSky_, "uDiskR", 1.4f + 1.1f * part_);
    set1f(progSky_, "uDisk", (0.08f * wm + 0.16f * wp + 0.34f * part_ + 0.06f * swell_) * (1.f - 0.7f * slow_) * V.glowK * 1.6f);
    set3f(progSky_, "uSil", lin(V.sil));
    ugl::drawFullscreen();

    // the world, far to near, a fog bank drifting between each depth (the frame's alpha is kept for the stack)
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUseProgram(progBank_);
    common(progBank_);
    glUseProgram(progSil_);
    common(progSil_);
    setMat(progSil_, "uVP", vp_);
    set3f(progSil_, "uSil", lin(V.sil));
    set3f(progSil_, "uLamp", lin(V.lamp));
    set1f(progSil_, "uVeil", 0.55f);
    set1f(progSil_, "uRimLit", (0.12f * wp + 0.75f * part_) * (1.f - slow_));
    set4f(progSil_, "uQuiet", -5.4f, -10.1f, 5.4f, 11.f);
    set4f(progSil_, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progSil_, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    set1f(progSil_, "uQuietK", 0.75f);
    set4f(progSil_, "uQuietTL", -viewH_.x - 2.f, 8.2f, -10.2f, viewH_.y + 2.f);
    const float bankK = 0.25f + 0.75f * saturate(fog);
    const float veil[L_COUNT] = {0.6f, 0.5f, 0.32f, 0.1f};
    for (int l = 0; l < L_COUNT; l++) {
        glUseProgram(progSil_);
        set1f(progSil_, "uVeil", veil[l]);
        drawPrims(layer_[l]);
        if (l == L_NEAR) {
            // the lanterns' halos (additive)
            set1f(progSil_, "uQuietK", 0.f);
            drawPrims(glows_);
            set1f(progSil_, "uQuietK", 0.75f);
        }
        if (l == L_FAR) drawBank(0.75f * bankK, Y_FAR + 0.5f, 3.2f, drift_ * 0.35f, 0.16f, 1.f, 1.02f);
        if (l == L_EDGE) drawBank(0.7f * bankK, Y_EDGE - 0.8f, 2.4f, drift_ * 0.6f, 0.22f, 2.f, 1.03f);
        if (l == L_MID) {
            // the flock flies over everything (never faded behind the HUD)
            set1f(progSil_, "uQuietK", 0.f);
            drawPrims(flockPrims_);
            set1f(progSil_, "uQuietK", 0.75f);
        }
        if (l == L_MID) drawBank(0.6f * bankK, Y_MID - 0.8f, 1.6f, drift_, 0.3f, 3.f, 1.04f);
    }
    // the well's edges: two faint posts in the fog
    {
        std::vector<Prim> frame;
        const float a = 0.22f * (1.f - over_ * 0.5f);
        for (int k = -1; k <= 1; k += 2)
            frame.push_back({{k * 5.1f, -10.05f, k * 5.1f, 10.f}, {0.025f, 0.012f, 0.f, 0.f}, {a, 0.f, 0.f, 0.f}});
        frame.push_back({{-5.1f, -10.08f, 5.1f, -10.08f}, {0.022f, 0.022f, 0.f, 0.f}, {a * 1.2f, 0.f, 0.f, 0.f}});
        glUseProgram(progSil_);
        set1f(progSil_, "uQuietK", 0.f);
        set1f(progSil_, "uVeil", 0.f);
        set1f(progSil_, "uRimLit", 0.f);
        drawPrims(frame);
    }

    // ---- The motes (behind the stack), the ghost, the pieces (they write the frame's alpha: the stack, for the bridge).
    if (!motes_.empty()) {
        moteInst_.clear();
        const vec3 dark = lerp(lin(V.sil), fogLo, 0.35f), mist = fogLo * 1.1f + glowCol * (0.3f * glow);
        for (const Mote& m : motes_) {
            const float u = m.t / m.life;
            const float a = m.a * (1.f - smoothstepf(0.35f, 1.f, u)) * smoothstepf(0.f, 0.08f, m.t);
            const float r = m.r * (m.tone == 0 ? 1.f + 0.8f * u : 1.f - 0.4f * u);
            const vec3 c = m.tone == 0 ? mist : dark;
            moteInst_.push_back({{m.p.x, m.p.y, r, a}, {c.x, c.y, c.z, m.tone == 0 ? 0.95f : 0.5f}});
        }
        glUseProgram(progMote_);
        setMat(progMote_, "uVP", vp_);
        glBindVertexArray(moteVao_);
        glBindBuffer(GL_ARRAY_BUFFER, moteVbo_);
        glBufferData(GL_ARRAY_BUFFER, moteInst_.size() * sizeof(MoteInst), moteInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)moteInst_.size());
    }
    glUseProgram(progPiece_);
    common(progPiece_);
    setMat(progPiece_, "uVP", vp_);
    set3f(progPiece_, "uInk", lin(V.ink));
    set3f(progPiece_, "uMist", fogLo * 1.08f + glowCol * (0.25f * glow));
    set1f(progPiece_, "uRim", V.rimK * (0.85f + 0.15f * wp) * (1.f - 0.3f * slow_));
    set1f(progPiece_, "uGhostA", V.dark ? 1.6f : 1.f);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    if (!ghost_.empty()) {
        glBufferData(GL_ARRAY_BUFFER, ghost_.size() * sizeof(CellInst), ghost_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)ghost_.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (!cells_.empty()) {
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // ---- The light shafts: the bright fog smeared toward the sun, occluded by every silhouette (and the stack)
    const float rays = (0.1f * wp + 1.f * part_ + 0.25f * swell_) * (1.f - slow_) * (1.f - over_);
    if (rays > 0.01f) {
        rayT_.bind();
        glDisable(GL_BLEND);
        glUseProgram(progRays_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
        set1i(progRays_, "uScene", 0);
        set2f(progRays_, "uSunUv", (0.f - viewC_.x) / viewH_.x * 0.5f + 0.5f, (sunY - viewC_.y) / viewH_.y * 0.5f + 0.5f);
        set1f(progRays_, "uThr", ugl::luma(fogLo) * 1.08f);
        set1f(progRays_, "uTime", worldTime_);
        set1f(progRays_, "uAspect", (float)width_ / height_);
        ugl::drawFullscreen();
    }

    // ---- The frame.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, rayT_.tex);
    set1i(progFinal_, "uRays", 1);
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uRayK", rays);
    set3f(progFinal_, "uRayCol", glowCol);
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uVig", V.dark ? 0.35f : 0.3f);
    set1f(progFinal_, "uGrain", 2.2f / 255.f);
    set1f(progFinal_, "uTime", time_);
    set3f(progFinal_, "uNight", lin(V.night) * (V.dark ? 1.1f : 0.82f));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_limbo() {
    static const UniverseEntry e{
        "LIMBO", "black silhouettes in a luminous fog that thickens and parts with the song; each cleared row adds to the landscape",
        [] { return std::unique_ptr<Universe>(new LimboUniverse()); },
        // Quiet, atmospheric songs suit the fog best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.brightness) + 0.1f * (1.f - l.density); },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

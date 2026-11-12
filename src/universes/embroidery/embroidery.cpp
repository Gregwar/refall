// EMBROIDERY: cross-stitch on aida cloth, a sampler stitched in the margins over the song.
//
// One world: a sheet of aida cloth under a soft daylight, the well a backstitched outline on it. The pieces are
// embroidered: every cell is four by four cross stitches of stranded floss (two plies twisted, catching the light on
// their crests), and each piece is outlined in a backstitch of a dark shade of its thread, as charted patterns are,
// so two pieces of one color never merge. The falling piece is held over the cloth (a soft shadow under it); the
// ghost is a running stitch around where it will land. In the margins, a sampler is stitched over the song: small
// charted motifs (a heart, a tree, a flower, a bird, a star, a strawberry) stacked in two panels, each framed by a
// budding vine. A needle does it, one strand at a time: what the player clears is thread for it (a cleared row is
// a run of stitches), and the music keeps it going: a stitch every two bars in a break, two a bar at mid, one a beat
// at a peak, two a beat in a drop. So the cloth fills up with the song and with the game.
//
// A cleared row is unpicked: its crosses come out one by one, top strand first, in a third of a second. A drop
// turns the cloth over to its drop side (white aida becomes black aida, natural linen indigo...) and the threads
// to brighter silks; the kicks of a peak run a glint along one piece's floss. Slow motion is the evening, a warm
// lamp over the frame; game over unpicks the stack row by row from the top. A new song unpicks the old sampler and
// starts a new one.
//
// Cost: one full-screen pass for the cloth and the sampler (its state is a small texture, a texel a stitch), the
// pieces as instanced cells drawing their crosses procedurally.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/embroidery/embroidery.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/embroidery/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Lora (Medium: the numbers and labels; Italic: the song, the section, the celebrations), SIL Open Font License,
// see OFL-Lora.txt.
const unsigned char FONT_REG[] = {
#include "universes/embroidery/Lora-Medium.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/embroidery/Lora-Italic.ttf.inc"
};

namespace shaders = embroidery_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// The cloths: the cloth and its drop side, the seven threads of the pieces and their drop silks, the leaves, the
// brown, the outline ink (the ghost, the well), the lettering's accent.
struct VariantDef {
    const char* name;
    unsigned cloth, dropCloth;
    unsigned thread[7], dropThread[7]; // I O T S Z J L
    unsigned leaf, dropLeaf, brown, dropBrown, ink, dropInk, accent;
};
const VariantDef VARIANTS[] = {
    // white aida and the classic floss; the drop turns it to black aida and bright silks
    {"WHITE AIDA", 0xf1efe9, 0x1d1d22,
     {0x3a9ad9, 0xf0bf2c, 0x9b5cc6, 0x4ea55a, 0xd93a45, 0x2f5bb8, 0xef8a2c},
     {0x4fc3ff, 0xffd84a, 0xc77dff, 0x5fe07a, 0xff4f6a, 0x6b95ff, 0xff9e3d},
     0x4f8a3c, 0x6fd06a, 0x7a5236, 0xc08850, 0x3a3836, 0xd8d2c8, 0xd93a45},
    // natural linen, earthy threads; the drop: indigo cloth and pale silks
    {"LINEN", 0xd8d2c6, 0x23324f,
     {0x4d8fb3, 0xe3b23c, 0xa0638f, 0x6b9a4e, 0xc4483e, 0x3f5a8a, 0xd9783a},
     {0x8fd3f0, 0xf6d77a, 0xe6a3d8, 0xa8dc8a, 0xff8f7a, 0xb5c7ff, 0xffb36b},
     0x5e8a46, 0x9fd68a, 0x6e4b30, 0xc99a6a, 0x3e3830, 0xc4cad8, 0xc4483e},
    // black aida, bright floss; the drop: a deep crimson cloth, pale gold silks
    {"BLACK AIDA", 0x1c1c20, 0x4a1020,
     {0x56c8f0, 0xf7d354, 0xc58cf0, 0x72d67c, 0xf25c6e, 0x6f8cf7, 0xf79b48},
     {0x9fe6ff, 0xffe58a, 0xe2b8ff, 0xa6f0a8, 0xffb0a0, 0xa9b9ff, 0xffc27a},
     0x5cbf6a, 0x8fe39a, 0xa07a52, 0xd4a878, 0x9a958e, 0xd8b0b8, 0xf7d354},
    // indigo cotton, white and pastel threads (sashiko colors); the drop: white linen and saturated floss
    {"INDIGO", 0x22304d, 0xece9e2,
     {0xa6d8ee, 0xf3e19a, 0xcdb6ee, 0xade0a2, 0xf2a5a5, 0xf4f1ea, 0xf7c28c},
     {0x2a8fc9, 0xe0a820, 0x8a4fc0, 0x3f9a4c, 0xd0353f, 0x233a7a, 0xe57a1f},
     0x8fcf86, 0x3f8a3a, 0xc8a07a, 0x6e4a2e, 0xa9b4cc, 0x5a6070, 0xf3e19a},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The sampler's charted motifs, drawn from the top row down. Letters are threads: r red, y yellow, p purple, s
// green, u sky blue, w deep blue, o orange (the pieces' threads), g the leaves, b the brown, k the ink.
struct Motif { int w, h; const char* rows[14]; };
const Motif MOTIFS[] = {
    {11, 10, {".rrr...rrr.", "rrrrr.rrrrr", "rryrrrrrrrr", "ryyrrrrrrrr", "rrrrrrrrrrr", ".rrrrrrrrr.", "..rrrrrrr..",
              "...rrrrr...", "....rrr....", ".....r....."}},
    {11, 13, {".....g.....", "....ggg....", "...ggrgg...", "....ggg....", "...ggggg...", "..ggggrgg..", "....ggg....",
              "..ggggggg..", ".grgggggrg.", "ggggggggggg", ".....b.....", ".....b.....", "....bbb...."}},
    {11, 13, {"....ppp....", "...ppppp...", "..pppyppp..", "..ppyyypp..", "..pppyppp..", "...ppppp...", "....ppp....",
              ".....g.....", ".gg..g..gg.", "..ggg.ggg..", "....ggg....", ".....g.....", ".....g....."}},
    {13, 10, {"...uuu.......", "..uuuuu......", ".ouukuu......", "oouuuuu....uu", "..uuuuuuuuuu.", "..uuwwwwuuu..",
              "...uwwwwwuu..", "....uuuuuu...", ".....o..o....", ".....o..o...."}},
    {9, 9, {"....y....", "....y....", "...yyy...", "yyyyyyyyy", ".yyyyyyy.", "..yyyyy..", "..yy.yy..", ".yy...yy.",
            ".y.....y."}},
    {9, 11, {"...g.g...", "..ggggg..", ".rrgggrr.", "rryrrryrr", "rrrrrrrrr", "ryrrryrrr", "rrrryrrrr", ".rryrrrr.",
             "..rrrry..", "...rrr...", "....r...."}},
    {11, 11, {"....www....", "...wuuuw...", "..wuuuuuw..", ".wuuoooouw.", "wuuoooooouw", "wuuooyyoouw", "wuuoooooouw",
              ".wuuoooouw.", "..wuuuuuw..", "...wuuuw...", "....www...."}},
};
constexpr int MOTIF_COUNT = (int)(sizeof(MOTIFS) / sizeof(MOTIFS[0]));
int threadOf(char c) {
    switch (c) {
    case 'u': return 0;
    case 'y': return 1;
    case 'p': return 2;
    case 's': return 3;
    case 'r': return 4;
    case 'w': return 5;
    case 'o': return 6;
    case 'g': return 7;
    case 'b': return 8;
    case 'k': return 9;
    default: return -1;
    }
}

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

bool EmbroideryUniverse::init(int width, int height) {
    using shaders::withCommon;
    progCloth_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::CLOTH_FS).c_str(), "embroidery cloth");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS).c_str(), "embroidery cells");
    progSeg_ = ugl::program(shaders::SEG_VS, shaders::SEG_FS, "embroidery needle");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "embroidery final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "embroidery shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "embroidery text");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto instanced = [&](GLuint& vao, GLuint& vbo, size_t stride) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        for (int i = 0; i < 3; i++) {
            glEnableVertexAttribArray(1 + i);
            glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
            glVertexAttribDivisor(1 + i, 1);
        }
    };
    instanced(cellVao_, cellVbo_, sizeof(CellInst));
    instanced(segVao_, segVbo_, sizeof(SegInst));
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

    glGenTextures(1, &designTex_);
    glBindTexture(GL_TEXTURE_2D, designTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    buildFont(fontReg_, FONT_REG);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void EmbroideryUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] embroidery font init failed\n");
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

void EmbroideryUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    // the stitch grid: a quarter of a cell a stitch, its origin on a cell corner (the board's cells fall on it)
    gO_ = vec2{std::floor(viewC_.x - viewH_.x) - 1.f, std::floor(viewC_.y - viewH_.y) - 1.f};
    gw_ = (int)std::ceil((2.f * viewH_.x + 2.f) * 4.f) + 4;
    gh_ = (int)std::ceil((2.f * viewH_.y + 2.f) * 4.f) + 4;
    const size_t keep = done_;
    const bool half = half_;
    planDesign();
    // a new size: the same progress on the new layout
    for (size_t i = 0; i < std::min(keep, plan_.size()); i++) {
        const Stitch& s = plan_[i];
        design_[((size_t)s.j * gw_ + s.i) * 4 + 0] = (unsigned char)(s.pal * 16);
        design_[((size_t)s.j * gw_ + s.i) * 4 + 3] = 255;
    }
    done_ = std::min(keep, plan_.size());
    half_ = half && done_ < plan_.size();
    if (half_) design_[((size_t)plan_[done_].j * gw_ + plan_[done_].i) * 4 + 3] = 128;
}

EmbroideryUniverse::~EmbroideryUniverse() {
    sceneT_.destroy();
    for (GLuint p : {progCloth_, progCell_, progSeg_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, segVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, segVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    if (designTex_) glDeleteTextures(1, &designTex_);
    for (Font* f : {&fontReg_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The sampler
// =====================================================================================================

// Plans the sampler of this song: in each outer margin (clear of the HUD columns) a panel framed by a budding vine,
// motifs stacked in it; the order the needle stitches them (motifs alternating left and right, bottom up, then the
// frames).
void EmbroideryUniverse::planDesign() {
    plan_.clear();
    design_.assign((size_t)gw_ * gh_ * 4, 0);
    done_ = 0;
    half_ = false;
    designDirty_ = true;
    auto gx = [&](float x) { return (int)std::floor((x - gO_.x) * 4.f); };
    auto gy = [&](float y) { return (int)std::floor((y - gO_.y) * 4.f); };
    const float xa = 11.9f, xb = viewH_.x - 0.75f;
    const int W = (int)std::floor((xb - xa) * 4.f);
    if (W < 14) return;
    const int y0 = gy(-9.0f), y1 = gy(9.6f);
    Rng r(seed_ ^ 0x5A3Bull);
    struct Panel { int x0, x1; std::vector<Stitch> motifs, frame; std::vector<std::vector<Stitch>> blocks; };
    Panel panels[2];
    for (int side = 0; side < 2; side++) {
        Panel& P = panels[side];
        P.x0 = side == 0 ? gx(-xb) : gx(xa);
        P.x1 = P.x0 + W - 1;
        // the frame: a vine of leaves on every other stitch, a bud every six
        int k = 0;
        auto frameAt = [&](int i, int j) {
            if (k % 2 == 0) P.frame.push_back({i, j, 7});
            if (k % 6 == 3) {
                const int ii = std::clamp(i, P.x0 + 1, P.x1 - 1), jj = std::clamp(j, y0 + 1, y1 - 1);
                P.frame.push_back({ii, jj, r.chance(0.5f) ? 4 : 2});
            }
            k++;
        };
        for (int i = P.x0; i <= P.x1; i++) frameAt(i, y0);
        for (int j = y0 + 1; j <= y1; j++) frameAt(P.x1, j);
        for (int i = P.x1 - 1; i >= P.x0; i--) frameAt(i, y1);
        for (int j = y1 - 1; j > y0; j--) frameAt(P.x0, j);
        // the motifs, bottom up, in this song's order
        int order[MOTIF_COUNT];
        for (int i = 0; i < MOTIF_COUNT; i++) order[i] = i;
        for (int i = MOTIF_COUNT - 1; i > 0; i--) std::swap(order[i], order[r.irange(0, i)]);
        int y = y0 + 4;
        for (int oi = 0; oi < MOTIF_COUNT; oi++) {
            const Motif& m = MOTIFS[order[oi]];
            if (m.w > W - 6) continue;
            if (y + m.h > y1 - 3) break;
            const int x = P.x0 + (W - m.w) / 2;
            std::vector<Stitch> b;
            // stitched color by color, row by row from the bottom (the rows alternate their direction)
            for (int pass = 0; pass < 10; pass++)
                for (int row = 0; row < m.h; row++) {
                    const int ry = m.h - 1 - row;
                    for (int c0 = 0; c0 < m.w; c0++) {
                        const int c = row % 2 == 0 ? c0 : m.w - 1 - c0;
                        const int t = threadOf(m.rows[ry][c]);
                        if (t != pass) continue;
                        b.push_back({x + c, y + row, t});
                    }
                }
            P.blocks.push_back(b);
            y += m.h + 4;
        }
    }
    const size_t nb = std::max(panels[0].blocks.size(), panels[1].blocks.size());
    for (size_t i = 0; i < nb; i++)
        for (int side = 0; side < 2; side++)
            if (i < panels[side].blocks.size()) plan_.insert(plan_.end(), panels[side].blocks[i].begin(), panels[side].blocks[i].end());
    for (int side = 0; side < 2; side++) plan_.insert(plan_.end(), panels[side].frame.begin(), panels[side].frame.end());
    // keep what falls on the grid
    plan_.erase(std::remove_if(plan_.begin(), plan_.end(), [&](const Stitch& s) { return s.i < 0 || s.j < 0 || s.i >= gw_ || s.j >= gh_; }), plan_.end());
}

// One pass of the needle: the first strand of the next cross, or its second (the cross is done).
void EmbroideryUniverse::stitchStep() {
    if (done_ >= plan_.size() || designOut_) return;
    const Stitch& s = plan_[done_];
    unsigned char* t = &design_[((size_t)s.j * gw_ + s.i) * 4];
    t[0] = (unsigned char)(s.pal * 16);
    const vec2 c = stitchPos(s.i, s.j);
    needleFrom_ = needle_;
    if (!half_) {
        t[3] = 128;
        half_ = true;
        needleTo_ = vec2{c.x + 0.1f, c.y + 0.1f};
    } else {
        t[3] = 255;
        half_ = false;
        done_++;
        needleTo_ = vec2{c.x + 0.1f, c.y - 0.1f};
    }
    if (!needleSet_) { needle_ = needleFrom_ = needleTo_; needleSet_ = true; }
    needleT_ = 0.f;
    stepT_ = 0.f;
    designDirty_ = true;
}

// The needle over the cloth (its tip at the last stitch, lifted while it travels) and its thread back to the cloth.
void EmbroideryUniverse::buildNeedle() {
    segs_.clear();
    if (plan_.empty() || needleA_ <= 0.01f || !needleSet_) return;
    const float e = smoothstepf(0.f, 1.f, needleT_);
    const vec2 p{lerpf(needleFrom_.x, needleTo_.x, e), lerpf(needleFrom_.y, needleTo_.y, e)};
    needle_ = p;
    const float lift = std::sin(PI * saturate(needleT_)) * 0.25f + 0.06f;
    const float ang = 0.95f;
    const vec2 d{std::cos(ang), std::sin(ang)};
    const vec2 tip{p.x, p.y}, eye{p.x + d.x * 1.7f, p.y + d.y * 1.7f};
    const size_t cur = std::min(done_, plan_.size() - 1);
    const vec3 tc = thread(plan_[cur].pal);
    const float a = needleA_ * designA_;
    // the thread: from the eye, sagging back to the cloth beside the stitch
    const vec2 end{p.x - 0.6f, p.y - 0.35f};
    const vec2 ctrl{(eye.x + end.x) * 0.5f + 0.5f, std::min(eye.y, end.y) - 0.9f};
    vec2 prev = eye;
    std::vector<SegInst> thread_;
    for (int i = 1; i <= 10; i++) {
        const float t = i / 10.f, u = 1.f - t;
        const vec2 q{u * u * eye.x + 2 * u * t * ctrl.x + t * t * end.x, u * u * eye.y + 2 * u * t * ctrl.y + t * t * end.y};
        thread_.push_back({{prev.x, prev.y, q.x, q.y}, {tc.x, tc.y, tc.z, a}, {0.032f, 0.f, 0.f, 0.f}});
        prev = q;
    }
    // shadows first, then the thread, then the steel
    const float so = 0.12f + lift * 0.5f;
    for (const SegInst& s : thread_)
        segs_.push_back({{s.a[0] + 0.06f, s.a[1] - 0.06f, s.a[2] + 0.06f, s.a[3] - 0.06f}, {0, 0, 0, 0.18f * a}, {0.08f, 2.f, 0, 0}});
    segs_.push_back({{tip.x + so, tip.y - so, eye.x + so, eye.y - so}, {0, 0, 0, 0.25f * a}, {0.1f + lift * 0.1f, 2.f, 0, 0}});
    segs_.insert(segs_.end(), thread_.begin(), thread_.end());
    const vec3 steel(0.55f, 0.57f, 0.6f);
    segs_.push_back({{tip.x, tip.y, eye.x, eye.y}, {steel.x, steel.y, steel.z, a}, {0.045f, 1.f, 0, 0}});
    // the eye: a dark slot near the end
    const vec2 e0{p.x + d.x * 1.45f, p.y + d.y * 1.45f}, e1{p.x + d.x * 1.62f, p.y + d.y * 1.62f};
    segs_.push_back({{e0.x, e0.y, e1.x, e1.y}, {0.08f, 0.08f, 0.09f, a}, {0.016f, 0.f, 0, 0}});
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 EmbroideryUniverse::cloth() const {
    const VariantDef& V = VARIANTS[variant_];
    return lerp(lin(V.cloth), lin(V.dropCloth), smoothstepf(0.f, 1.f, dev_));
}

vec3 EmbroideryUniverse::thread(int i) const {
    const VariantDef& V = VARIANTS[variant_];
    const float k = smoothstepf(0.f, 1.f, dev_);
    if (i >= 0 && i < 7) return lerp(lin(V.thread[i]), lin(V.dropThread[i]), k);
    if (i == 7) return lerp(lin(V.leaf), lin(V.dropLeaf), k);
    if (i == 8) return lerp(lin(V.brown), lin(V.dropBrown), k);
    return lerp(lin(V.ink), lin(V.dropInk), k);
}

void EmbroideryUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = h < 0.5f ? 2 : 3;
        else if (t.hasSong && t.brightness > 0.6f) v = 0;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xE3Bull);
    // a new song: the old sampler is unpicked and a new one planned (at once for the first one)
    if (seconds <= 0.1f) { planDesign(); designA_ = 1.f; designOut_ = false; credit_ = 0.f; }
    else designOut_ = true;
}

void EmbroideryUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: the cloth is turned to its drop side and the threads to bright silks; every piece catches the light.
void EmbroideryUniverse::drop(float strength) {
    dropMode_ = true;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (ids_[y][x]) glint_[y][x] = std::max(glint_[y][x], 0.35f + 0.2f * strength);
}

// A new section: a few stitches at once.
void EmbroideryUniverse::refresh(float strength) { credit_ += 4.f + 6.f * strength; }

void EmbroideryUniverse::levelUp() { credit_ += 12.f; }

vec2 EmbroideryUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints EmbroideryUniverse::hints() const {
    TransitionHints h;
    h.background = cloth();
    h.light = ugl::luma(h.background) > 0.2f;
    h.ink = thread(9);
    h.accent = lin(VARIANTS[variant_].accent);
    h.styles = TS_REBUILD | TS_TILES | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void EmbroideryUniverse::syncIds(const Game& g) {
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

void EmbroideryUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            glint_[c.y][c.x] = 0.25f; // the thread pulled tight
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // the cleared rows are unpicked
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
            unpick_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.f, (float)(id % 97)});
        }
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
        // the thread goes to the sampler: a run of stitches per row, a long one for a quad
        credit_ += ev.count >= 4 ? 60.f : 12.f * ev.count;
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack stays, then is unpicked row by row from the top
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                if (c.y < Game::H - 1 && ids_[c.y + 1][c.x] == id) m |= 4;
                if (c.y > 0 && ids_[c.y - 1][c.x] == id) m |= 8;
            }
            unpick_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, 0.f, 0.8f + 0.13f * (c.y - Game::HIDDEN), (float)(id % 97)});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void EmbroideryUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int m = 0;
        for (int i = 0; i < 4; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto push = [&](float x, float y, float half, int mask, const vec3& c, float alpha, int kind, float prog, float glint, float seed) {
        if (alpha <= 0.003f) return;
        cells_.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, alpha}, {(float)kind, prog, glint, seed}});
    };
    const vec3 ink = thread(9);
    // The falling piece's shadow, the ghost (under everything).
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            push(w.x + 0.16f, w.y - 0.2f, 0.5f, 0, vec3(0.f), 0.26f, 2, 0.f, 0.f, 0.f);
        }
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
                push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), ink, 0.75f, 1, 0.f, 0.f, 0.f);
            }
        }
    }
    // The stack.
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
            push(p.x, p.y, 0.5f, maskOf(x, y, same), thread(c.type), 1.f, 0, 0.f, glint_[y][x], (float)(ids_[y][x] % 97));
        }
    // Cells being unpicked (a clear at the real speed; game over from the top down).
    for (const Unpick& u : unpick_) {
        const float k = saturate((u.t - u.delay) / (u.delay > 0.f ? 0.55f : 0.33f));
        push(u.x, u.y, 0.5f, u.mask, thread(u.type), 1.f, 3, k, 0.f, u.seed);
    }
    // The falling piece, held over the cloth.
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
            push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), thread(p.type), 1.f, 0, 0.f, 0.f, 7.f);
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
            push(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), thread(type), alpha, 0, 0.f, 0.f, 3.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float EmbroideryUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void EmbroideryUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void EmbroideryUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.3f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void EmbroideryUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in a thread color over the cloth (dark on a pale cloth, pale on a dark one); the bonus gauge a ring
// of running stitches whose charge is stitched solid in the accent.
void EmbroideryUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const vec3 cl = cloth();
    const bool pale = ugl::luma(cl) > 0.2f;
    const vec3 ink = ugl::toSrgb(pale ? lin(0x2b2a33) : lin(0xf1ede4)), halo = ugl::toSrgb(cl);
    const vec3 accent = ugl::toSrgb(lerp(lin(VARIANTS[variant_].accent), thread(4), smoothstepf(0.f, 1.f, dev_)));
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

    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 48;
        const float th = std::max(1.2f, 1.6f * s), th2 = std::max(2.f, 3.6f * s);
        for (int i = 0; i < N; i++) {
            const bool on = (i + 0.5f) / N <= fill;
            // a running stitch: every other segment only, unless charged
            if (!on && i % 2 == 1) continue;
            const float a0 = TAU * (i + 0.08f) / N, a1 = TAU * (i + 0.92f) / N;
            const float w = on ? th2 : th;
            const vec3 col = on ? accent : ink;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.75f : 0.45f)) * vis * (1.f - 0.5f * dimT_);
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
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string t = h.text;
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            place(fontReg_, t, base * 0.78f, base * 0.16f, base * 0.08f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        }
        case HUD_SECTION:
            place(fontIt_, h.text, base * 0.92f, base * 0.06f, -base * 0.05f);
            drawText(fontIt_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.0f, 0.f, -base * 0.06f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontReg_, h.text, base * 0.74f, base * 0.04f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL: {
            std::string t = h.text;
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            place(fontReg_, t, base * 0.7f, base * 0.2f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        }
        case HUD_VALUE:
            place(fontReg_, h.text, base * 1.0f, 0.f, -base * 0.05f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 0.95f, base * 0.03f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.88f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontIt_, h.text, base * 1.08f, base * 0.12f, -base * 0.1f);
            drawText(fontIt_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 0.88f, base * 0.03f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontIt_, h.text, base * 1.0f, base * 0.05f);
                drawText(fontIt_, q, ink, halo, alpha);
            } else {
                place(fontReg_, h.text, base * 0.82f, base * 0.03f);
                drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.8f, 0.f);
            drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void EmbroideryUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            unpick_.erase(std::remove_if(unpick_.begin(), unpick_.end(), [](const Unpick& u) { return u.delay > 0.f; }), unpick_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 2.f : 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the cloth turns over on the drop's downbeat (quickly) and back slowly after it
    dev_ = approach(dev_, dropMode_ ? wp : 0.f, dropMode_ ? 3.5f : 0.3f, dt);
    swell_ = approach(swell_, 0.f, 0.5f, dt);
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 3.f, dt);

    // ---- The kicks of a peak: a glint runs along one piece's floss.
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f && wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f) {
        int n = 0, pick = -1;
        for (int y = Game::HIDDEN; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++)
                if (ids_[y][x] && rng_.irange(0, n++) == 0) pick = ids_[y][x];
        if (pick >= 0)
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++)
                    if (ids_[y][x] == pick) glint_[y][x] = std::max(glint_[y][x], 0.5f * kick + 0.2f * dev_);
    }
    kickPrev_ = kick;

    // ---- The needle: steps on the half-beat grid (a stitch every two bars in a break, two a bar at mid, one a
    // beat at a peak, two a beat in a drop), and the thread the player clears.
    if (!f.paused && overT_ < 0.f && !designOut_) {
        int every = 16; // half-beats between steps
        if (level_ == 1) every = 4;
        if (level_ == 2) every = dropMode_ ? 1 : 2;
        if (music.hasSong) {
            const double hb = std::floor(music.beat * 2.0 + 1e-4);
            if (lastBeat_ < 0.0 || hb < lastBeat_ - 16.0) lastBeat_ = hb;
            if (hb > lastBeat_) {
                lastBeat_ = hb;
                if (std::fmod(hb, (double)every) < 0.5) stitchStep();
            }
        } else {
            barClock_ += wdt;
            if (barClock_ > every * 0.25f) { barClock_ = 0.f; stitchStep(); }
        }
        stepT_ += wdt;
        if (credit_ >= 1.f && stepT_ > 0.11f) {
            credit_ -= 1.f;
            stitchStep();
        }
    }
    if (done_ >= plan_.size()) credit_ = 0.f;
    needleT_ = std::min(1.f, needleT_ + wdt / 0.2f);
    needleA_ = approach(needleA_, done_ < plan_.size() && !designOut_ ? 1.f : 0.f, 2.f, dt);
    // a new song: the old sampler fades as it is unpicked, then a new one is planned
    if (designOut_) {
        designA_ = std::max(0.f, designA_ - dt / 1.5f);
        if (designA_ <= 0.f) {
            planDesign();
            designOut_ = false;
            credit_ = 0.f;
            needleSet_ = false;
        }
    } else designA_ = std::min(1.f, designA_ + dt / 0.5f);
    buildNeedle();

    // ---- Unpicked cells (real time, even in slow motion).
    for (Unpick& u : unpick_) u.t += dt;
    unpick_.erase(std::remove_if(unpick_.begin(), unpick_.end(),
                                 [](const Unpick& u) { return u.t > u.delay + (u.delay > 0.f ? 0.6f : 0.36f); }),
                  unpick_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    if (designDirty_ && !design_.empty()) {
        glBindTexture(GL_TEXTURE_2D, designTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, gw_, gh_, 0, GL_RGBA, GL_UNSIGNED_BYTE, design_.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        designDirty_ = false;
    }

    // ---- The cloth, the sampler, the well.
    const float px = 2.f * viewH_.y / height_;
    const float sheen = (0.55f + 0.25f * wm + 0.35f * wp + 0.3f * swell_) * (1.f - 0.5f * slow_);
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    sceneT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progCloth_);
    set4f(progCloth_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progCloth_, "uRes", (float)width_, (float)height_);
    set1f(progCloth_, "uSeed", seedU);
    set3f(progCloth_, "uCloth", cloth());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, designTex_);
    set1i(progCloth_, "uDesign", 0);
    set4f(progCloth_, "uGrid", gO_.x, gO_.y, (float)gw_, (float)gh_);
    {
        float pal[30];
        for (int i = 0; i < 10; i++) {
            const vec3 c = thread(i);
            pal[i * 3] = c.x; pal[i * 3 + 1] = c.y; pal[i * 3 + 2] = c.z;
        }
        glUniform3fv(ugl::U(progCloth_, "uPal"), 10, pal);
    }
    set1f(progCloth_, "uDesignA", designA_);
    set1f(progCloth_, "uSheen", sheen);
    set1f(progCloth_, "uLight", (0.94f + 0.04f * wm + 0.04f * wp + 0.05f * swell_));
    set1f(progCloth_, "uVig", 0.18f);
    set3f(progCloth_, "uWell", thread(9));
    ugl::drawFullscreen();

    // ---- The pieces, the ghost, the needle.
    cells_.clear();
    buildBoard(game);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1f(progCell_, "uSeed", seedU);
        set1f(progCell_, "uSheen", sheen);
        set1f(progCell_, "uPxCell", px);
        set3f(progCell_, "uInk", thread(9));
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }
    if (!segs_.empty()) {
        glUseProgram(progSeg_);
        setMat(progSeg_, "uVP", vp_);
        set1f(progSeg_, "uPx", px);
        glBindVertexArray(segVao_);
        glBindBuffer(GL_ARRAY_BUFFER, segVbo_);
        glBufferData(GL_ARRAY_BUFFER, segs_.size() * sizeof(SegInst), segs_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)segs_.size());
    }

    // ---- The frame.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
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

const UniverseEntry& universe_embroidery() {
    static const UniverseEntry e{
        "EMBROIDERY", "cross-stitch on aida: the pieces embroidered, a sampler stitched in the margins over the song",
        [] { return std::unique_ptr<Universe>(new EmbroideryUniverse()); },
        // Gentle, acoustic songs suit the needlework best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.dynamics) + 0.1f * l.brightness; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

// ORIGAMI: folded paper pieces on a sheet, each cleared row folded into a paper crane before your eyes.
//
// One world: a large sheet of paper under a soft light from its upper left, the well a shallow tray folded into it
// (two valley creases and a floor, open at the top). The sheet was folded before: a giant crease pattern lies over
// it, its facets catching the light faintly at calm and more at a peak (a new pattern after each drop), and the light
// is warm where it comes from, cool across the sheet. The pieces are folded paper, each of its own colour: a strip
// folded along a ridge through its cells' centres (mitred where it turns), lit facet by facet, its cut edge lit on
// its sides toward the light; every piece lies on the sheet with a soft shadow, so two pieces of one colour never
// merge. The falling piece looks exactly like a locked one, only held a little above the sheet (its shadow further);
// its shadow settles on lock. The ghost is a dashed valley-fold line, as in folding diagrams.
//
// A cleared row slides out of the well through one of its walls (the other one than the last row's) as one strip of
// paper, pleating against the wall, and the pleated packet glides to a folding corner on that side (under the right
// HUD column, or in the left margin above the corner texts), turns over into a square and is folded into a crane
// step by step, every flap turning about its crease in 3D. The crane then flies to a place around the well, where it
// settles for the rest of the song: the margins fill up with the play. A Tetris folds a larger crane in the accent
// paper and a ripple of wing beats runs through the whole flock.
//
// Staging: calm, the cranes are still, the light soft and the creases faint; at mid, every two beats a few cranes
// beat their wings; at a peak, half the flock beats its wings in turn every two beats, the kicks make one hop, the
// creases come up and the light warms. The drop turns the sheet over to its coloured side and sets every crane
// flying in slow loops in the margins (wings on the two-beat grid), until the peak ends and they land back. Slow
// motion is the blue hour, a low light from the right. Game over folds the stack away row by row from the top. A new
// song sends the cranes away and starts a new flock.
//
// Cost: one full-screen pass for the sheet (the shadows from a half-resolution target), the pieces as instanced
// cells drawing their facets procedurally, the cranes as a few triangles each.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/origami/origami.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/origami/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Josefin Sans (Regular: the numbers and labels; Light: the song, the section, the celebrations), SIL Open Font
// License, see OFL-JosefinSans.txt.
const unsigned char FONT_REG[] = {
#include "universes/origami/JosefinSans-Regular.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/origami/JosefinSans-Light.ttf.inc"
};

namespace shaders = origami_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }
// Two palette colours blended in display space (a blend in linear light looks washed at mid values), to linear.
vec3 mixHex(unsigned a, unsigned b, float t) { return ugl::toLinear(lerp(hex(a), hex(b), t)); }

// The papers: the sheet and its other side (the drop), the seven papers of the pieces and their drop papers, the
// back of the paper (seen when a cell folds over), the accent (a Tetris crane, the gauge) on each side; foil: a
// metallic glint on the facets.
struct VariantDef {
    const char* name;
    unsigned sheet, dropSheet;
    unsigned piece[7], dropPiece[7]; // I O T S Z J L
    unsigned back, accent, dropAccent;
    float foil;
};
const VariantDef VARIANTS[] = {
    // white paper, classic kami colours; the drop turns the sheet to its deep blue side
    {"KAMI", 0xebeae7, 0x1e2536,
     {0x2f9bd6, 0xf0b52e, 0x8a55c4, 0x3ea65e, 0xe0443e, 0x2c4fa8, 0xee7a2f},
     {0x5ec4f5, 0xffd35a, 0xb98bf2, 0x66d68a, 0xff6a5e, 0x7d9bf5, 0xffa257},
     0xf7f6f3, 0xd23b36, 0xffd35a, 0.f},
    // a cool snow-grey sheet, soft papers; the drop: black paper and bright ones
    {"SNOW", 0xe2e5e8, 0x16171b,
     {0x5aa7d6, 0xe6bb4c, 0x9c7fd0, 0x62b37a, 0xe0706a, 0x5272bd, 0xe68f50},
     {0x8fd6ff, 0xffe07a, 0xc9adff, 0x9ae8ad, 0xff9d96, 0x9cb4ff, 0xffb57a},
     0xf8f8f8, 0x4c6bb8, 0xffe07a, 0.f},
    // a graphite sheet, bright papers; the drop: a deep crimson side and pale papers
    {"GRAPHITE", 0x2a2b2f, 0x561923,
     {0x4cb8ec, 0xf5cb48, 0xb07de8, 0x5cc97a, 0xf0605a, 0x5d82e6, 0xf5913f},
     {0x9fe2ff, 0xffe58a, 0xdcbcff, 0xa8f0b8, 0xffb8a8, 0xb4c6ff, 0xffc887},
     0xefeeea, 0xf5cb48, 0xffe58a, 0.f},
    // an indigo sheet, pale papers; the drop turns it to its white side, the papers saturated
    {"INDIGO", 0x1f2a48, 0xeceae6,
     {0x8fd0f2, 0xf6dc78, 0xc1a6f2, 0x9ddc9a, 0xf59a96, 0xeef1f6, 0xf8b679},
     {0x2a8fc9, 0xe0a820, 0x7f4dbd, 0x3a9a50, 0xd23c3c, 0x24407e, 0xe0702a},
     0xf4f2ee, 0xf4df8e, 0xd23c3c, 0.f},
    // black paper and foil papers (silver, gold, copper...); the drop: a deep teal side
    {"FOIL", 0x151517, 0x0e3438,
     {0xc3c8d0, 0xdcb45a, 0x9c78d4, 0x52b88c, 0xdc8aa2, 0x5c88dc, 0xcc7a4a},
     {0xe2e6ee, 0xf2cf72, 0xbd9cf2, 0x7ad8ac, 0xf2a8bc, 0x86aaf2, 0xe8965e},
     0xe8e8ea, 0xdcb45a, 0xf2cf72, 1.f},
    // a pale celadon-grey sheet, deep jewel papers; the drop: a forest-green side, the papers lighter
    {"CELADON", 0xdfe5e1, 0x16291f,
     {0x2b7fb8, 0xd99a1e, 0x7a4aa8, 0x2f8a52, 0xc23b3b, 0x2b3f86, 0xd2632a},
     {0x6cc0ee, 0xf6c54e, 0xb58ae6, 0x74d494, 0xf27a70, 0x8ea2f0, 0xf6a060},
     0xf7f8f6, 0xc23b3b, 0xf6c54e, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

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

constexpr float FOLD_REST = 0.42f;

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool OrigamiUniverse::init(int width, int height) {
    using shaders::withCommon;
    progPaper_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::PAPER_FS).c_str(), "origami paper");
    progCell_ = ugl::program(shaders::CELL_VS, shaders::CELL_FS, "origami cells");
    progTri_ = ugl::program(shaders::TRI_VS, shaders::TRI_FS, "origami cranes");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "origami final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "origami shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "origami text");

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
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    auto colored = [&](GLuint& vao, GLuint& vbo) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    };
    colored(triVao_, triVbo_);
    colored(shapeVao_, shapeVbo_);
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(fontReg_, FONT_REG);
    buildFont(fontLight_, FONT_LIGHT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void OrigamiUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] origami font init failed\n");
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

void OrigamiUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    shadowT_.create(std::max(1, width_ / 2), std::max(1, height_ / 2), GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    // the cranes keep their order; their places move to the new layout
    planSlots();
    std::vector<int> used(slots_.size(), 0);
    for (Crane& c : cranes_) {
        if (c.slot >= (int)slots_.size()) c.slot = -1;
        if (c.slot < 0 && !c.leaving) {
            for (size_t i = 0; i < slots_.size(); i++)
                if (!used[i]) { c.slot = (int)i; break; }
        }
        if (c.slot >= 0) { used[c.slot]++; c.home = slots_[c.slot]; }
        else c.leaving = true;
    }
    slotUsed_ = used;
}

OrigamiUniverse::~OrigamiUniverse() {
    sceneT_.destroy();
    shadowT_.destroy();
    for (GLuint p : {progPaper_, progCell_, progTri_, progFinal_, progShape_, progText_}) glDeleteProgram(p);
    for (GLuint v : {cellVao_, triVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, triVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontReg_, &fontLight_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The cranes
// =====================================================================================================

// The places where cranes settle: a staggered, jittered grid over the margins, clear of the well and of the HUD
// columns, in the order they fill (beside the well's foot first, then up and out).
void OrigamiUniverse::planSlots() {
    slots_.clear();
    const float x0 = viewC_.x - viewH_.x + 1.0f, x1 = viewC_.x + viewH_.x - 1.0f;
    const float y0 = viewC_.y - viewH_.y + 0.55f, y1 = viewC_.y + viewH_.y - 1.4f;
    Rng r(0x0A1Bull);
    auto inRect = [&](float x, float y, const float* R, float pad) {
        return x + 1.0f > R[0] - pad && x - 1.0f < R[2] + pad && y + 1.0f > R[1] - pad && y - 0.25f < R[3] + pad;
    };
    const float WELL[4] = {-5.f, -10.f, 5.f, 12.f};
    struct Cand { vec2 p; float key; };
    std::vector<Cand> cands;
    // the corner texts (the song's title, the section and the universe's name)
    const float TITLE[4] = {x0 - 2.f, 8.6f, -12.2f, 20.f}, CORNER[4] = {x0 - 2.f, -20.f, -11.6f, -6.6f};
    // the HUD columns, down to the last line set in them (the best score, the combo notice under the gauge)
    // (under the left one: the lane the rows folded on the left glide along)
    const float LEFT[4] = {layout::LEFT_COLUMN[0], -20.f, layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]};
    const float RIGHT[4] = {layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1] - 2.0f, layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]};
    // the corner where the rows are folded into cranes
    const float FOLDING[4] = {6.0f, -12.f, 11.6f, -5.6f}, FOLDING_L[4] = {-21.f, -6.4f, -12.6f, -0.3f};
    const float sx = 2.55f, sy = 2.2f;
    int row = 0;
    for (float y = y0; y <= y1; y += sy, row++)
        for (float x = x0 + (row % 2 ? sx * 0.5f : 0.f); x <= x1; x += sx) {
            const float jx = x + r.range(-0.55f, 0.55f), jy = y + r.range(-0.45f, 0.45f);
            if (jx < x0 || jx > x1 || jy < y0 || jy > y1) continue;
            if (inRect(jx, jy, WELL, 0.5f)) continue;
            if (inRect(jx, jy, LEFT, 0.35f) || inRect(jx, jy, RIGHT, 0.35f)) continue;
            if (inRect(jx, jy, TITLE, 0.2f) || inRect(jx, jy, CORNER, 0.2f)) continue;
            if (inRect(jx, jy, FOLDING, 0.f) || inRect(jx, jy, FOLDING_L, 0.f)) continue;
            // the order: close to the well's foot first
            const float dx = std::max(0.f, std::fabs(jx) - 5.f), dy = std::max(0.f, jy + 10.f);
            const float key = std::sqrt(dx * dx + dy * dy * 0.55f) + r.range(0.f, 0.6f);
            cands.push_back({vec2{jx, jy}, key});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.key < b.key; });
    for (const Cand& c : cands) slots_.push_back(c.p);
    slotUsed_.assign(slots_.size(), 0);
}

int OrigamiUniverse::takeSlot(float side) {
    // a place on the side the crane is folded on, else anywhere
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < slots_.size(); i++)
            if (!slotUsed_[i] && (pass == 1 || (slots_[i].x < 0.f) == (side < 0.f))) { slotUsed_[i] = 1; return (int)i; }
    // all taken: the oldest crane flies away and leaves its place to the new one
    for (int pass = 0; pass < 2; pass++)
    for (Crane& c : cranes_)
        if (!c.leaving && c.slot >= 0 && c.age > c.arrive && (pass == 1 || (c.home.x < 0.f) == (side < 0.f))) {
            const int s = c.slot;
            c.leaving = true;
            c.slot = -1;
            return s;
        }
    return -1;
}

// The crane's timeline (seconds since its row was cleared; the row and the folding run at the real speed, even in
// slow motion): the row slides out of the well and pleats against its wall, the pleated packet turns over into a
// square, the square is folded into a crane step by step, the crane flies to its place.
namespace {
constexpr float SLIDE = 0.45f;      // the row is out of the well
constexpr float PLEAT = 0.62f;      // the pleats are pressed
constexpr float STEP = 0.3f;        // one fold step
constexpr int STEPS = 7;            // turn over, kite, petal, legs, neck and tail, head, wings
constexpr float FOLD0_R = 0.85f, FOLD0_L = 1.4f; // the packet has glided to its folding spot (right, left)
constexpr float FOLD_SCALE = 1.35f; // the model's size while it is folded
constexpr float THETA = 1.47f;      // the pleats' angle when pressed (84 degrees)

vec3 rotAbout(const vec3& p, const vec3& a, const vec3& k, float phi) {
    const vec3 v = p - a;
    const float c = std::cos(phi), s = std::sin(phi);
    return a + v * c + cross(k, v) * s + k * (dot(k, v) * (1.f - c));
}
float ease(float u) { u = saturate(u); return u * u * (3.f - 2.f * u); }
} // namespace

void OrigamiUniverse::addCrane(float rowY, float side, const int* strip, int color, float scale) {
    const int s = takeSlot(side);
    if (s < 0) return;
    Crane c;
    c.slot = s;
    c.home = slots_[s];
    c.stripY = rowY;
    // the row leaves by one wall; it is folded in a free corner on that side (under the right HUD column, or in the
    // left margin above the corner texts), a spot of its own when several rows are folded at once
    c.side = side;
    c.fold0 = side > 0.f ? FOLD0_R : FOLD0_L;
    c.foldEnd = c.fold0 + STEP * STEPS;
    for (int i = 0; i < 10; i++) c.strip[i] = strip[i];
    const vec2 SPOTS_R[4] = {{7.4f, -7.7f}, {10.4f, -7.7f}, {7.4f, -10.1f}, {10.4f, -10.1f}};
    const vec2 SPOTS_L[4] = {{-14.6f, -4.3f}, {-17.9f, -4.3f}, {-14.6f, -1.9f}, {-17.9f, -1.9f}};
    const vec2* SPOTS = side > 0.f ? SPOTS_R : SPOTS_L;
    int si = 0;
    for (; si < 4; si++) {
        bool used = false;
        for (const Crane& o : cranes_)
            if (!o.leaving && o.age < o.foldEnd && std::fabs(o.spot.x - SPOTS[si].x) + std::fabs(o.spot.y - SPOTS[si].y) < 0.1f) used = true;
        if (!used) break;
    }
    c.spot = SPOTS[si % 4];
    c.from = c.spot;
    c.pos = c.spot;
    c.color = color;
    c.scale = scale * (0.82f + 0.18f * rng_.uniform());
    c.age = 0.f;
    c.arrive = c.foldEnd + ((c.home.x < 0.f) != (side < 0.f) ? 3.6f : 1.4f + 0.25f * std::fabs(c.home.x - c.spot.x) / 6.f + 0.1f * std::fabs(c.home.y - c.spot.y) / 6.f);
    c.face = side;
    c.phase = rng_.range(-0.18f, 0.18f);
    c.flyDelay = rng_.range(0.f, 1.4f);
    // its loop in the flock: an ellipse in the outer margin of its side
    const float hs = c.home.x < 0.f ? -1.f : 1.f;
    const float cx = hs * (11.9f + viewH_.x - 1.f) * 0.5f;
    const float half = std::max(1.f, (viewH_.x - 1.f - 11.9f) * 0.5f);
    c.rx = rng_.range(0.6f, half - 0.4f);
    // the left margin keeps clear of the corner texts (the title above, the section and the name below)
    c.ry = hs < 0.f ? rng_.range(3.5f, 6.4f) : rng_.range(5.f, std::max(5.5f, viewH_.y - 2.5f));
    c.orbit = std::atan2((c.home.y - (hs < 0.f ? 1.0f : 0.4f)) / c.ry, (c.home.x - cx) / c.rx);
    c.speed = hs * rng_.range(0.85f, 1.2f) / (0.5f * (c.rx + c.ry));
    cranes_.push_back(c);
}

void OrigamiUniverse::updateCranes(const MusicState& m, float dt, float wdt, bool paused) {
    const float wp = smoothstepf(1.1f, 2.f, lv_), wm = smoothstepf(0.3f, 1.f, lv_);
    // the two-beat grid: wing beats
    bool tick = false;
    if (!paused) {
        if (m.hasSong) {
            const double hb = std::floor(m.beat * 0.5 + 1e-4);
            if (lastHalf_ < 0.0 || hb < lastHalf_ - 8.0) lastHalf_ = hb;
            if (hb > lastHalf_) { lastHalf_ = hb; tick = true; }
        } else {
            beatClock_ += wdt;
            if (beatClock_ > 1.0f) { beatClock_ = 0.f; tick = true; }
        }
    }
    if (tick && overT_ < 0.f) {
        beatCount_++;
        for (Crane& c : cranes_) {
            if (c.leaving || c.age < c.arrive || c.fly > 0.05f || c.wingT < 1.f) continue;
            bool flap = false;
            if (wp > 0.5f) flap = ((c.slot + beatCount_) % 2) == 0;            // a peak: half the flock in turn
            else if (wm > 0.5f) flap = rng_.chance(0.16f);                       // mid: a few
            else flap = (beatCount_ % 4 == 0) && rng_.chance(0.04f);             // calm: now and then one
            if (flap) { c.wingT = 0.f; c.wingDelay = std::max(0.f, std::fabs(c.home.x) - 5.f) * 0.035f; }
        }
    }
    // the kicks of a peak: a crane hops
    const float kick = std::max(m.kick, kickIn_);
    if (!paused && overT_ < 0.f && wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f && !cranes_.empty()) {
        for (int tries = 0; tries < 6; tries++) {
            Crane& c = cranes_[rng_.irange(0, (int)cranes_.size() - 1)];
            if (c.leaving || c.age < c.arrive || c.fly > 0.05f || c.hop > 0.f) continue;
            c.hopV = 1.6f + 1.4f * kick;
            c.wingT = 0.f;
            c.wingDelay = 0.f;
            break;
        }
    }
    kickPrev_ = kick;

    for (Crane& c : cranes_) {
        const vec2 prev = c.pos;
        // the row and the folding at the real speed, the rest in the world's time
        const float step = c.age < c.foldEnd ? (paused ? 0.f : dt) : wdt;
        c.age += step;
        if (c.wingT < 1.f) {
            if (c.wingDelay > 0.f) c.wingDelay -= wdt;
            else c.wingT = std::min(1.f, c.wingT + wdt / 0.62f);
        }
        if (c.hopV != 0.f || c.hop > 0.f) {
            c.hop += c.hopV * wdt;
            c.hopV -= 12.f * wdt;
            if (c.hop <= 0.f) { c.hop = 0.f; c.hopV = 0.f; }
        }
        if (c.leaving) {
            c.leaveT += wdt;
            const float side = c.pos.x < 0.f ? -1.f : 1.f;
            c.vel = vec2{c.vel.x + side * 0.5f * wdt, std::min(3.2f, c.vel.y + 1.6f * wdt)};
            c.pos = vec2{c.pos.x + c.vel.x * wdt, c.pos.y + c.vel.y * wdt};
        } else if (c.age < c.foldEnd) {
            c.pos = c.spot;
            c.face = c.side;
        } else if (c.age < c.arrive) {
            // the finished crane takes off to its place
            const float t = saturate((c.age - c.foldEnd) / (c.arrive - c.foldEnd));
            if ((c.home.x < 0.f) != (c.from.x < 0.f)) {
                // to the other side: up beside the well, over its top (behind the pieces), down the other side
                const float top = 11.3f, sd = c.from.x < 0.f ? -1.f : 1.f;
                const float up = smoothstepf(0.f, 0.32f, t), across = smoothstepf(0.25f, 0.72f, t), down = smoothstepf(0.66f, 1.f, t);
                const float x0 = lerpf(c.from.x, sd * 5.7f, smoothstepf(0.f, 0.2f, t));
                const float x1 = lerpf(-sd * 5.7f, c.home.x, smoothstepf(0.8f, 1.f, t));
                c.pos = vec2{lerpf(x0, x1, across), lerpf(lerpf(c.from.y, top, up), c.home.y, down)};
            } else {
                // out of the corner first, then up (or down) the outer margin to its place
                const float out = smoothstepf(0.f, 0.55f, t), up = smoothstepf(0.3f, 1.f, t);
                c.pos = vec2{lerpf(c.from.x, c.home.x, out), lerpf(c.from.y, c.home.y, up) + 0.6f * std::sin(PI * t)};
            }
        } else {
            // perched, or in the flock
            const float want = flock_ > 0.5f ? 1.f : 0.f;
            if (want > c.fly) {
                if (c.flyDelay > 0.f) c.flyDelay -= wdt;
                else c.fly = std::min(1.f, c.fly + wdt / 2.4f);
            } else if (want < c.fly) {
                c.fly = std::max(0.f, c.fly - wdt / 3.2f);
                c.flyDelay = rng_.range(0.f, 1.4f);
            }
            if (c.fly > 0.f) c.orbit += c.speed * wdt * smoothstepf(0.f, 0.6f, c.fly);
            const float side = c.home.x < 0.f ? -1.f : 1.f;
            const float cx = side * (11.9f + viewH_.x - 1.f) * 0.5f;
            const vec2 track{cx + c.rx * std::cos(c.orbit), (side < 0.f ? 1.0f : 0.4f) + c.ry * std::sin(c.orbit)};
            const float e = smoothstepf(0.f, 1.f, c.fly);
            const float lift = std::sin(PI * e) * 1.4f;
            c.pos = vec2{lerpf(c.home.x, track.x, e), lerpf(c.home.y, track.y, e) + lift + c.hop};
        }
        if (c.age >= c.foldEnd) {
            const float vx = (c.pos.x - prev.x) / std::max(1e-4f, wdt);
            if (wdt > 0.f && std::fabs(vx) > 0.25f) c.face = vx > 0.f ? 1.f : -1.f;
            else if (!c.leaving && c.age >= c.arrive && c.fly <= 0.01f) c.face = c.home.x < 0.f ? 1.f : -1.f; // perched: toward the well
        }
    }
    cranes_.erase(std::remove_if(cranes_.begin(), cranes_.end(),
                                 [&](const Crane& c) { return c.leaving && (c.leaveT > 7.f || c.pos.y > viewC_.y + viewH_.y + 2.f); }),
                  cranes_.end());
}

// The row as a strip of paper (board units): it slides out of the well through its wall and pleats against it, a
// pleat a cell, valley and mountain in turn (each pleat lit by its own slope), its papers turning to the crane's as
// the pleats are pressed. flipX scales it about its centre (the packet turning over).
void OrigamiUniverse::stripFaces(const Crane& c, float t, float flipX, std::vector<PFace>& out) const {
    const float s = 10.f * ease(t / SLIDE);
    const float press = smoothstepf(SLIDE * 0.6f, PLEAT, t);
    float th[10], w[10];
    float total = 0.f;
    for (int k = 0; k < 10; k++) { // k = 0: the leading cell (the one nearest the wall it leaves by)
        const float d = s - (k + 0.5f);
        th[k] = THETA * std::max(smoothstepf(-0.4f, 0.9f, d), press);
        w[k] = std::cos(th[k]);
        total += w[k];
    }
    // the trailing end slides flat until it reaches the wall; the pleated stack then glides to the folding spot
    const float xT = 5.f + s - 10.f;
    const float stackC = xT + total * 0.5f;
    const float glide = smoothstepf(SLIDE, c.fold0, t);
    // the packet's centre: at the wall, then gliding to its spot (on the left, along the well's wall and under the
    // HUD column, clear of its text)
    vec2 cen{c.side * stackC, c.stripY};
    if (t > SLIDE) {
        const vec2 p0{c.side * (5.f + total * 0.5f), c.stripY};
        vec2 pts[5] = {p0, c.spot, c.spot, c.spot, c.spot};
        int n = 2;
        if (c.side < 0.f) {
            const float low = std::min(c.stripY, -10.45f);
            pts[1] = vec2{-5.75f, low};
            pts[2] = vec2{-11.9f, low};
            pts[3] = vec2{-11.9f, c.spot.y};
            pts[4] = c.spot;
            n = 5;
        }
        float len[5] = {0.f}, total2 = 0.f;
        for (int i = 1; i < n; i++) {
            const float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y;
            total2 += std::sqrt(dx * dx + dy * dy);
            len[i] = total2;
        }
        const float at = glide * total2;
        cen = pts[n - 1];
        for (int i = 1; i < n; i++)
            if (at <= len[i]) {
                const float f = (at - len[i - 1]) / std::max(1e-4f, len[i] - len[i - 1]);
                cen = vec2{lerpf(pts[i - 1].x, pts[i].x, f), lerpf(pts[i - 1].y, pts[i].y, f)};
                break;
            }
    }
    const float col = smoothstepf(SLIDE * 0.7f, PLEAT, t);
    const vec3 target = paper(c.color);
    float x = xT, z = 0.f;
    for (int k = 9; k >= 0; k--) {
        const int column = c.side > 0.f ? 9 - k : k;
        const float x1 = x + w[k];
        const float z1 = z + ((k % 2) ? 1.f : -1.f) * std::sin(th[k]);
        auto X = [&](float xx) { return cen.x + c.side * (xx - stackC) * flipX; };
        const vec3 p = ugl::toLinear(lerp(ugl::toSrgb(paper(c.strip[column])), ugl::toSrgb(target), col));
        const float y0 = cen.y - 0.5f, y1 = cen.y + 0.5f;
        const vec3 a(X(x), y0, z), b(X(x1), y0, z1), cc(X(x1), y1, z1), d(X(x), y1, z);
        // (a pleat never shows its back face on: mirrored for the left wall, it keeps its paper)
        out.push_back({{a, b, cc}, p, p, 0.f});
        out.push_back({{a, cc, d}, p, p, 0.f});
        x = x1;
        z = z1;
    }
}

// The crane folded from a square, as faces of paper (local units, facing right, z toward the viewer). F: the fold
// progress 0..STEPS (each unit a step: turn over, kite, petal, legs, neck and tail, head, wings), flap: the wings'
// beat (radians, 0 at rest). Every flap turns about its crease in 3D.
void OrigamiUniverse::craneModel(float F, float flap, const vec3& col, std::vector<PFace>& out) const {
    const float a = 0.7f, wb = 0.24f;
    const int step = std::min(STEPS - 1, (int)std::floor(F));
    const float u = F >= STEPS ? 1.f : ease(F - step);
    const vec3 back = col * 0.93f; // the inner layers: the same paper
    int order = 0;
    auto face = [&](const vec3& p0, const vec3& p1, const vec3& p2, float zo) {
        PFace f{{p0, p1, p2}, col, back, 0.f};
        f.key = (p0.z + p1.z + p2.z) / 3.f + zo + order * 0.004f;
        out.push_back(f);
    };
    // a triangle in two facets, its middle crease raised (a mountain fold) so that the two sides catch the light
    // differently
    auto tri2 = [&](vec3 p0, vec3 p1, vec3 tip, float rz, float zo) {
        vec3 m = (p0 + p1) * 0.5f;
        m.z += rz * length(p1 - p0);
        face(p0, m, tip, zo);
        face(m, p1, tip, zo);
    };
    // turns a set of points about a crease (A -> B) by phi, toward the viewer (toward = 1) or behind (-1)
    auto flapPts = [&](vec3* pts, int n, vec3 A, vec3 B, float phi, float toward) {
        vec3 k = normalize(B - A);
        vec3 cen(0.f);
        for (int i = 0; i < n; i++) cen = cen + pts[i] * (1.f / n);
        if (cross(k, cen - A).z * toward < 0.f) k = k * -1.f;
        for (int i = 0; i < n; i++) pts[i] = rotAbout(pts[i], A, k, phi);
    };
    auto V = [](float x, float y, float z = 0.f) { return vec3(x, y, z); };
    if (step == 0) {
        // the square, turned over (its pre-creased diagonals barely raised)
        if (u < 0.5f) return; // the pleated packet shows first (stripFaces)
        const float sx = std::fabs(std::cos(PI * u));
        const vec3 O = V(0, 0, 0.03f), T = V(0, a), R = V(a * sx, 0), B = V(0, -a), L = V(-a * sx, 0);
        face(O, T, R, 0); face(O, R, B, 0); face(O, B, L, 0); face(O, L, T, 0);
        return;
    }
    if (step == 1) {
        // kite: the side corners fold in to the centre
        const vec3 O = V(0, 0, 0.03f);
        const vec3 H[6] = {V(0, a), V(a / 2, a / 2), V(a / 2, -a / 2), V(0, -a), V(-a / 2, -a / 2), V(-a / 2, a / 2)};
        for (int i = 0; i < 6; i++) face(O, H[i], H[(i + 1) % 6], 0);
        order = 5;
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 f[3] = {V(sd * a, 0), V(sd * a / 2, a / 2), V(sd * a / 2, -a / 2)};
            flapPts(f, 3, V(sd * a / 2, -1), V(sd * a / 2, 1), PI * u, 1.f);
            face(f[0], f[1], f[2], 0.02f);
        }
        return;
    }
    auto mixv = [&](const vec3& p, const vec3& q, float k) { return p + (q - p) * k; };
    if (step == 2) {
        // petal fold: the front layer's lower half swings up (it becomes the near wing), the paper stretches long
        const float m = u;
        const vec3 U0 = mixv(V(-a / 2, 0), V(-wb, 0), m), U1 = mixv(V(-a / 2, a / 2), V(-wb * 0.5f, 0.5f), m),
                   U2 = mixv(V(0, a), V(0, 1.f), m), U3 = V(-U1.x, U1.y), U4 = V(-U0.x, 0);
        const vec3 D1 = mixv(V(-a / 2, -a / 2), V(-wb * 0.5f, -0.475f), m), D2 = mixv(V(0, -a), V(0, -0.95f), m),
                   D3 = V(-D1.x, D1.y);
        order = 0;
        tri2(U0, U4, U2, 0.12f, -0.04f);
        face(U0, U1, U2, -0.04f);
        face(U4, U3, U2, -0.04f);
        order = 1;
        tri2(U0, U4, D2, 0.12f, -0.03f);
        face(U0, D1, D2, -0.03f);
        face(U4, D3, D2, -0.03f);
        order = 6;
        vec3 p[5] = {U0, U4, D3, D2, D1};
        flapPts(p, 5, V(-1, 0), V(1, 0), PI * u, 1.f);
        face(p[0], p[1], p[3], 0.03f);
        face(p[0], p[3], p[4], 0.03f);
        face(p[1], p[2], p[3], 0.03f);
        return;
    }
    // the bird base and on: two wings (up), two legs (down, then neck and tail), the body
    const float wingU = step >= 6 ? u : 0.f;
    {
        // the far wing, behind
        const float ps = -(0.55f * wingU + flap);
        const float ww = wb + 0.12f * wingU;
        vec3 w[3] = {V(-ww - 0.06f * wingU, 0.04f * wingU), V(ww - 0.06f * wingU, 0.04f * wingU), V(-0.4f * wingU, 1.f + 0.05f * wingU)};
        flapPts(w, 3, V(-1, 0), V(1, 0), std::fabs(ps), ps < 0.f ? -1.f : 1.f);
        order = 0;
        tri2(w[0], w[1], w[2], 0.14f, -0.1f);
    }
    auto leg = [&](float sd) {
        // a leg: narrowed (its outer part folded behind), then swung up (the neck on the right, the tail on the left)
        const float lw = 0.15f;
        const float swing = step == 4 ? u : (step > 4 ? 1.f : 0.f);
        const float ang = sd * 2.3f * swing; // the right leg turns anticlockwise, the left one clockwise
        const vec3 piv = V(sd * 0.075f, 0);
        auto R = [&](vec3 p) {
            const float c = std::cos(ang), s = std::sin(ang);
            const vec3 d = p - piv;
            return V(piv.x + d.x * c - d.y * s, piv.y + d.x * s + d.y * c, p.z + 0.25f * std::sin(PI * swing));
        };
        order = 2 + (sd > 0.f ? 1 : 0);
        const vec3 tip = V(sd * 0.02f, -0.95f);
        if (step == 3) {
            tri2(V(sd * lw, 0), V(0, 0), tip, 0.1f, -0.02f);
            vec3 f[3] = {V(sd * wb, 0), V(sd * lw, 0), tip};
            flapPts(f, 3, V(sd * lw, 0), tip, PI * u, -1.f);
            face(f[0], f[1], f[2], -0.02f);
            return;
        }
        if (sd < 0.f || step < 5) {
            tri2(R(V(sd * lw, 0)), R(V(0, 0)), R(tip), 0.1f, -0.02f);
            return;
        }
        // the neck, its tip folded down into the head
        const vec3 p0 = R(V(0, 0)), p1 = R(V(lw, 0)), t = R(tip);
        const vec3 e0 = p0 + (t - p0) * 0.72f, e1 = p1 + (t - p1) * 0.86f;
        tri2(p1, p0, e0, 0.1f, -0.02f);
        face(p1, e0, e1, -0.02f);
        vec3 h[3] = {e0, e1, t};
        flapPts(h, 3, e0, e1, PI * 0.92f * (step == 5 ? u : 1.f), 1.f);
        order = 4;
        face(h[0], h[1], h[2], 0.0f);
    };
    leg(-1.f);
    leg(1.f);
    if (step >= 4) {
        // the body: where the wings and the legs meet
        const float g = step == 4 ? u : 1.f;
        order = 5;
        tri2(V(-0.4f * g, 0.03f), V(0.4f * g, 0.03f), V(0.02f, -0.24f * g), -0.2f, 0.02f);
        tri2(V(-0.4f * g, 0.03f), V(0.4f * g, 0.03f), V(-0.02f, 0.17f * g), 0.2f, 0.02f);
    }
    {
        // the near wing, in front
        const float ps = 0.5f * wingU + flap;
        const float ww = wb + 0.14f * wingU;
        vec3 w[3] = {V(-ww, 0.04f * wingU), V(ww, 0.04f * wingU), V(-0.18f * wingU, 1.08f * (1.f + 0.06f * wingU))};
        flapPts(w, 3, V(-1, 0), V(1, 0), std::fabs(ps), ps < 0.f ? -1.f : 1.f);
        order = 8;
        tri2(w[0], w[1], w[2], 0.14f, 0.1f);
    }
}

// Lit, sorted faces to triangles (and their shadow on the sheet): each face lit by its own normal (the side facing
// the viewer chooses the colour), a darker rim under it for the paper's edge.
void OrigamiUniverse::emitFaces(std::vector<PFace>& faces, vec2 pos, float scale, float facing, float alpha,
                                float shadowOff, float shadowA, bool outline) {
    std::stable_sort(faces.begin(), faces.end(), [](const PFace& x, const PFace& y) { return x.key < y.key; });
    const vec3 L = lightDir();
    const float ll = 1.f / std::max(1e-3f, std::sqrt(L.x * L.x + L.y * L.y));
    const vec2 shDir{-L.x * ll, -L.y * ll};
    for (const PFace& f : faces) {
        vec3 n = cross(f.v[1] - f.v[0], f.v[2] - f.v[0]);
        if (length(n) < 1e-7f) continue;
        n = normalize(n);
        vec3 c = f.front;
        if (n.z < 0.f) { n = n * -1.f; c = f.back; }
        n.x *= facing;
        const float k = std::max(0.3f, 1.f + 1.2f * (dot(n, L) - L.z));
        c = c * k;
        vec2 p[3];
        for (int i = 0; i < 3; i++) p[i] = vec2{pos.x + facing * f.v[i].x * scale, pos.y + f.v[i].y * scale};
        if (outline) {
            const vec2 cen{(p[0].x + p[1].x + p[2].x) / 3.f, (p[0].y + p[1].y + p[2].y) / 3.f};
            const vec3 e = c * 0.6f;
            for (int i = 0; i < 3; i++) {
                const vec2 d{p[i].x - cen.x, p[i].y - cen.y};
                const float l = std::max(1e-4f, std::sqrt(d.x * d.x + d.y * d.y));
                const float g = 0.035f / l;
                const float v[6] = {p[i].x + d.x * g, p[i].y + d.y * g, e.x, e.y, e.z, alpha};
                craneTris_.insert(craneTris_.end(), v, v + 6);
            }
        }
        for (int i = 0; i < 3; i++) {
            const float v[6] = {p[i].x, p[i].y, c.x, c.y, c.z, alpha};
            craneTris_.insert(craneTris_.end(), v, v + 6);
        }
        if (shadowA > 0.f)
            for (int i = 0; i < 3; i++) {
                const float v[6] = {p[i].x + shDir.x * shadowOff, p[i].y + shDir.y * shadowOff, 0.f, 0.f, 0.f, shadowA};
                craneShadow_.insert(craneShadow_.end(), v, v + 6);
            }
    }
}

void OrigamiUniverse::buildCranes() {
    craneTris_.clear();
    craneShadow_.clear();
    const vec3 L = lightDir();
    const float ll = 1.f / std::max(1e-3f, std::sqrt(L.x * L.x + L.y * L.y));
    const vec2 shDir{-L.x * ll, -L.y * ll};
    const float vis = 1.f - smoothstepf(0.2f, 0.7f, bridge_.amount);
    std::vector<PFace> faces;
    for (const Crane& c : cranes_) {
        float a = vis;
        if (c.leaving) a *= 1.f - smoothstepf(5.f, 7.f, c.leaveT);
        if (a <= 0.01f) continue;
        faces.clear();
        // the folding: the row's strip, then the model step by step
        if (!c.leaving && c.age < c.foldEnd) {
            const float F = std::max(0.f, (c.age - c.fold0) / STEP);
            if (F < 0.5f) {
                const float flipX = c.age < c.fold0 ? 1.f : std::fabs(std::cos(PI * F));
                stripFaces(c, c.age, flipX, faces);
                const float lift = 0.08f + 0.25f * std::sin(PI * smoothstepf(SLIDE, c.fold0, c.age));
                emitFaces(faces, vec2{0, 0}, 1.f, 1.f, a, lift, 0.5f * a, false);
            } else {
                craneModel(F, 0.f, paper(c.color), faces);
                emitFaces(faces, c.pos, FOLD_SCALE, c.face, a, 0.18f, 0.5f * a, true);
            }
            continue;
        }
        // the wings: at rest, a stroke down and up on a beat, beating in flight
        float flap = 0.f;
        const bool flying = c.leaving || c.age < c.arrive || c.fly > 0.02f;
        if (flying) {
            const float ph = flapPhase_ + c.phase;
            const float beat = -0.25f + 1.9f * 0.5f * (1.f + std::sin(TAU * ph));
            const float k = c.leaving ? 1.f : c.age < c.arrive ? smoothstepf(c.foldEnd, c.foldEnd + 0.4f, c.age) : smoothstepf(0.f, 0.4f, c.fly);
            flap = beat * k;
        }
        if (c.wingT < 1.f && c.wingDelay <= 0.f) flap += 1.7f * std::sin(PI * c.wingT);
        // grows from its folding size to its own on the way to its place
        const float sc = c.leaving ? c.scale : lerpf(FOLD_SCALE, c.scale, smoothstepf(c.foldEnd, c.arrive, c.age));
        // perched cranes a shade quieter than the pieces (blended toward the sheet in display space)
        const float quiet = 0.2f * (flying ? 0.f : 1.f);
        const vec3 base = ugl::toLinear(lerp(ugl::toSrgb(paper(c.color)), ugl::toSrgb(sheet()), quiet));
        craneModel((float)STEPS, flap, base, faces);
        const float alt = c.leaving ? 1.f : std::max(c.age < c.arrive ? std::sin(PI * saturate((c.age - c.foldEnd) / (c.arrive - c.foldEnd))) : 0.f,
                                                     std::sin(0.5f * PI * smoothstepf(0.f, 1.f, c.fly)));
        const float off = 0.14f + 0.9f * alt + c.hop * 0.5f;
        emitFaces(faces, c.pos, sc, c.face, a, off, a * lerpf(0.5f, 0.2f, alt), true);
        // a soft contact shadow under a perched crane
        const float ca = a * 0.55f * (1.f - alt) * (1.f - saturate(c.hop * 2.f));
        if (ca > 0.01f) {
            const vec2 cc{c.pos.x + shDir.x * 0.05f, c.pos.y - 0.16f * sc};
            const int N = 12;
            for (int i = 0; i < N; i++) {
                const float a0 = TAU * i / N, a1 = TAU * (i + 1) / N;
                const float v[3][6] = {{cc.x, cc.y, 0, 0, 0, ca},
                                       {cc.x + std::cos(a0) * 0.42f * sc, cc.y + std::sin(a0) * 0.12f * sc, 0, 0, 0, 0},
                                       {cc.x + std::cos(a1) * 0.42f * sc, cc.y + std::sin(a1) * 0.12f * sc, 0, 0, 0, 0}};
                for (auto& q : v) craneShadow_.insert(craneShadow_.end(), q, q + 6);
            }
        }
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

vec3 OrigamiUniverse::sheet() const {
    const VariantDef& V = VARIANTS[variant_];
    return mixHex(V.sheet, V.dropSheet, smoothstepf(0.f, 1.f, dev_));
}

vec3 OrigamiUniverse::paper(int i) const {
    const VariantDef& V = VARIANTS[variant_];
    const float k = smoothstepf(0.f, 1.f, dev_);
    if (i >= 0 && i < 7) return mixHex(V.piece[i], V.dropPiece[i], k);
    return mixHex(V.accent, V.dropAccent, k);
}

vec3 OrigamiUniverse::lightDir() const {
    const vec3 day(-0.5f, 0.66f, 0.6f), dusk(0.8f, 0.12f, 0.5f);
    return normalize(lerp(day, dusk, smoothstepf(0.f, 1.f, slow_)));
}

void OrigamiUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = h < 0.33f ? 2 : h < 0.66f ? 3 : 4;
        else if (t.hasSong && t.brightness > 0.6f) v = h < 0.33f ? 0 : h < 0.66f ? 1 : 5;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x0516A41ull);
    // a new song: the flock flies away (at once for the first song)
    if (seconds <= 0.1f) {
        cranes_.clear();
        slotUsed_.assign(slots_.size(), 0);
    } else {
        for (Crane& c : cranes_) {
            if (c.leaving) continue;
            c.leaving = true;
            c.leaveT = -rng_.range(0.f, 1.5f);
            c.slot = -1;
        }
        slotUsed_.assign(slots_.size(), 0);
    }
}

void OrigamiUniverse::setLevel(int level, float energy, float seconds, float impact) {
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

// The drop: the sheet turns over to its coloured side and the whole flock takes off.
void OrigamiUniverse::drop(float strength) {
    patternOut_ = true; // the sheet's creases change: the old pattern fades, a new one comes up
    // the flock's loops: each side's cranes spread evenly around their margin
    if (!dropMode_) {
        for (int side = -1; side <= 1; side += 2) {
            int n = 0;
            for (const Crane& c : cranes_)
                if (!c.leaving && (c.home.x < 0.f) == (side < 0)) n++;
            int i = 0;
            const float off = rng_.range(0.f, TAU);
            for (Crane& c : cranes_) {
                if (c.leaving || (c.home.x < 0.f) != (side < 0)) continue;
                if (c.fly < 0.05f) c.orbit = off + TAU * (i + 0.3f * rng_.uniform()) / std::max(1, n);
                i++;
            }
        }
    }
    dropMode_ = true;
    swell_ = std::max(swell_, 0.5f + 0.5f * strength);
}

// A new section: a ripple of wing beats from the well outward.
void OrigamiUniverse::refresh(float strength) {
    (void)strength;
    for (Crane& c : cranes_) {
        if (c.leaving || c.age < c.arrive || c.fly > 0.05f) continue;
        c.wingT = 0.f;
        c.wingDelay = std::max(0.f, std::fabs(c.home.x) - 5.f) * 0.05f + std::fabs(c.home.y + 10.f) * 0.02f;
    }
}

void OrigamiUniverse::levelUp() { refresh(1.f); }

vec2 OrigamiUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints OrigamiUniverse::hints() const {
    TransitionHints h;
    h.background = sheet();
    h.light = ugl::luma(ugl::toSrgb(h.background)) > 0.45f;
    h.ink = h.light ? lin(0x2a2b30) : lin(0xeceae6);
    h.accent = paper(7);
    h.styles = TS_REBUILD | TS_TILES | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void OrigamiUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; fold_[y][x] = 0.f; }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            fold_[y][x] = 0.f;
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
                    fold_[ny][nx] = 0.f;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void OrigamiUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            fold_[c.y][c.x] = 0.f; // time since it landed: its shadow settles
        }
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        // each row slides out of the well as a strip of paper and is folded into a crane of its most common paper
        int strips[Game::H][10] = {}, counts[Game::H][7] = {};
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            strips[c.y][c.x] = std::clamp(c.type, 0, 6);
            counts[c.y][std::clamp(c.type, 0, 6)]++;
        }
        int k = 0;
        for (int y = Game::H - 1; y >= Game::HIDDEN; y--) {
            if (!rows[y]) continue;
            int best = 0;
            for (int t = 1; t < 7; t++)
                if (counts[y][t] > counts[y][best]) best = t;
            const vec3 p = layout::cellPos(0.f, (float)y);
            const bool special = ev.count >= 4 && k == 1;
            // each row leaves by the other wall than the last one (a double clear sends one row each way), unless that
            // side's folding corner is full
            float side = -lastSide_;
            int busy = 0;
            for (const Crane& o : cranes_)
                if (!o.leaving && o.age < o.foldEnd && (o.side < 0.f) == (side < 0.f)) busy++;
            if (busy >= 4) side = -side;
            lastSide_ = side;
            addCrane(p.y, side, strips[y], special ? 7 : best, special ? 1.25f : 1.f);
            k++;
        }
        if (ev.count >= 4) {
            // a Tetris: a ripple of wing beats through the flock
            for (Crane& c : cranes_) {
                if (c.leaving || c.age < c.arrive || c.fly > 0.05f) continue;
                c.wingT = 0.f;
                c.wingDelay = 0.35f + std::max(0.f, std::fabs(c.home.x) - 5.f) * 0.06f + std::fabs(c.home.y + 10.f) * 0.025f;
            }
        }
        {
            int nb[Game::H][Game::W] = {};
            float nf[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nf[dst], fold_[y], sizeof(fold_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(fold_, nf, sizeof(fold_));
        }
        break;
    }
    case GameEvent::TopOut:
        overT_ = 0.f;
        // the stack stays a moment, then folds away row by row from the top
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const int id = ids_[c.y][c.x];
            int m = 0, r = 0;
            if (id) {
                if (c.x > 0 && ids_[c.y][c.x - 1] == id) m |= 1;
                if (c.x < Game::W - 1 && ids_[c.y][c.x + 1] == id) m |= 2;
                r = m;
                if (c.y < Game::H - 1 && ids_[c.y + 1][c.x] == id) r |= 4;
                if (c.y > 0 && ids_[c.y - 1][c.x] == id) r |= 8;
            }
            folding_.push_back({p.x, p.y, std::clamp(c.type, 0, 6), m, r, 0.f, 0.8f + 0.12f * (c.y - Game::HIDDEN)});
        }
        break;
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void OrigamiUniverse::buildBoard(const Game& g) {
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, 1, -1};
        int m = 0;
        for (int i = 0; i < 4; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const vec3 L = lightDir();
    const float ll = std::max(1e-3f, std::sqrt(L.x * L.x + L.y * L.y));
    const vec2 shDir{-L.x / ll, -L.y / ll};
    const float shLen = lerpf(1.f, 2.4f, slow_);
    auto push = [&](float x, float y, float half, int mask, const vec3& c, float alpha, int kind, float prog, float fold,
                    float lift, int ridge = 0) {
        if (alpha <= 0.003f) return;
        // the strip's width across its fold (seen from above, a folded strip is narrower)
        const float sl = 0.75f * 0.8f * fold;
        const float w = kind == 0 ? std::min(1.f, 1.f / std::sqrt(1.f + sl * sl)) : 1.f;
        cells_.push_back({{x, y, half, (float)mask}, {c.x, c.y, c.z, alpha}, {(float)kind, prog, fold, lift}, {(float)ridge, w, 0.f, 0.f}});
    };
    auto shadow = [&](float x, float y, float half, float lift, float alpha) {
        const float o = (0.05f + lift) * shLen;
        shadows_.push_back({{x + shDir.x * o, y + shDir.y * o, half, 0.f}, {0, 0, 0, alpha}, {2.f, 0.f, 0.f, lift}, {0, 0, 0, 0}});
    };
    const vec3 ink = hints().ink;
    // The ghost (under everything).
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
                push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), ink, 0.55f, 1, 0.f, 0.f, 0.f);
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
            // after the lock: the shadow settles
            const float t = fold_[y][x];
            const float fold = FOLD_REST; // a piece looks the same falling and locked; only its shadow settles
            const float lift = 0.1f + 0.25f * std::exp(-9.f * t);
            push(p.x, p.y, 0.5f, maskOf(x, y, same), paper(c.type), 1.f, 0, 0.f, std::max(0.f, fold), lift);
            shadow(p.x, p.y, 0.5f, lift, 0.9f);
        }
    // Cells folding away (a clear at the real speed; game over from the top down).
    for (const Folding& u : folding_) {
        const float k = saturate((u.t - u.delay) / (u.delay > 0.f ? 0.5f : 0.32f));
        push(u.x, u.y, 0.5f, u.mask, paper(u.type), 1.f, 3, k, FOLD_REST, 0.1f, u.ridge);
    }
    // The falling piece, held a little above the sheet.
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
        const float lp = g.lockProgress();
        const float fold = FOLD_REST, lift = 0.38f - 0.12f * lp;
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            push(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), paper(p.type), 1.f, 0, 0.f, fold, lift);
            shadow(w.x, w.y, 0.5f, lift, 0.75f);
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
            push(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), paper(type), alpha, 0, 0.f, FOLD_REST, 0.1f);
            shadow(px, py, 0.5f * sc, 0.08f, 0.7f * alpha);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void OrigamiUniverse::drawCells(const std::vector<CellInst>& v) {
    if (v.empty()) return;
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(CellInst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void OrigamiUniverse::drawTris(const std::vector<float>& v) {
    if (v.empty()) return;
    glBindVertexArray(triVao_);
    glBindBuffer(GL_ARRAY_BUFFER, triVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float OrigamiUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void OrigamiUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void OrigamiUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.35f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void OrigamiUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in graphite or chalk-white over the sheet (from the sheet's own value, so it reads on both sides of
// the paper); the bonus gauge a ring of folded paper segments, each in two facets, charged in the accent paper.
void OrigamiUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    const vec3 sh = ugl::toSrgb(sheet());
    const float paleK = smoothstepf(0.4f, 0.47f, ugl::luma(sh)); // decisive: graphite on pale paper, white on dark
    const vec3 ink = lerp(hex(0xeeece8), hex(0x26272c), paleK), halo = sh;
    const vec3 accent = ugl::toSrgb(paper(7));
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
        const int N = 16;
        const float th = std::max(2.f, 5.f * s);
        for (int i = 0; i < N; i++) {
            const bool on = (i + 0.5f) / N <= fill;
            const float a0 = TAU * (i + 0.06f) / N, a1 = TAU * (i + 0.94f) / N, am = (a0 + a1) * 0.5f;
            const float ri = r - th * 0.5f, ro = r + th * 0.5f;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.6f : 0.32f)) * vis * (1.f - 0.5f * dimT_);
            const vec3 col = on ? accent : ink;
            // two facets: the outer half lit, the inner half in shade (a folded strip)
            const vec3 cA = col, cB = on ? col * 0.78f : col;
            const float alB = on ? al : al * 0.7f;
            auto pt = [&](float ang, float rr) { return vec2{c.x + std::sin(ang) * rr, c.y - std::cos(ang) * rr}; };
            const vec2 o0 = pt(a0, ro), o1 = pt(a1, ro), i0 = pt(a0, ri), i1 = pt(a1, ri), mo = pt(am, ro + th * 0.25f),
                       mi = pt(am, ri + th * 0.25f);
            const float tris[4][3][2] = {{{o0.x, o0.y}, {mo.x, mo.y}, {mi.x, mi.y}}, {{o0.x, o0.y}, {mi.x, mi.y}, {i0.x, i0.y}},
                                         {{mo.x, mo.y}, {o1.x, o1.y}, {i1.x, i1.y}}, {{mo.x, mo.y}, {i1.x, i1.y}, {mi.x, mi.y}}};
            for (int t = 0; t < 4; t++) {
                const vec3 cc = (t % 2 == 0) ? cA : cB;
                const float aa = (t % 2 == 0) ? al : alB;
                for (int k = 0; k < 3; k++) {
                    const float v[6] = {tris[t][k][0], tris[t][k][1], cc.x, cc.y, cc.z, aa};
                    shapes_.insert(shapes_.end(), v, v + 6);
                }
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
        auto upper = [&](std::string t) {
            for (char& ch : t) ch = (char)std::toupper((unsigned char)ch);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontReg_, upper(h.text), base * 0.74f, base * 0.2f, base * 0.1f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f));
            continue;
        case HUD_SECTION:
            place(fontLight_, upper(h.text), base * 0.9f, base * 0.28f, -base * 0.02f);
            drawText(fontLight_, q, ink, halo, saturate(0.25f + h.alpha * 1.7f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.02f, base * 0.02f, -base * 0.04f);
            drawText(fontLight_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontReg_, h.text, base * 0.72f, base * 0.06f, base * 0.04f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.72f);
            continue;
        case HUD_LABEL:
            place(fontReg_, upper(h.text), base * 0.68f, base * 0.26f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontReg_, h.text, base * 1.02f, base * 0.02f, -base * 0.02f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontReg_, h.text, base * 0.92f, base * 0.06f);
            drawText(fontReg_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.86f, 0.f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontLight_, upper(h.text), base * 1.04f, base * 0.24f, -base * 0.06f);
            drawText(fontLight_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontReg_, h.text, base * 0.84f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontLight_, upper(h.text), base * 0.98f, base * 0.2f);
                drawText(fontLight_, q, ink, halo, alpha);
            } else {
                place(fontReg_, h.text, base * 0.8f, base * 0.04f);
                drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.78f, base * 0.02f);
            drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void OrigamiUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) {
            overT_ = -1.f;
            folding_.erase(std::remove_if(folding_.begin(), folding_.end(), [](const Folding& u) { return u.delay > 0.f; }), folding_.end());
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, 1.f, dt);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += dt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    // the sheet turns over on the drop's downbeat and back slowly after the peak
    dev_ = approach(dev_, dropMode_ ? wp : 0.f, dropMode_ ? 1.2f : 0.3f, dt);
    flock_ = dropMode_ && wp > 0.3f ? 1.f : 0.f;
    swell_ = approach(swell_, 0.f, 0.4f, dt);
    if (patternOut_) {
        patternA_ = std::max(0.f, patternA_ - dt / 1.2f);
        if (patternA_ <= 0.f) { pattern_++; patternOut_ = false; }
    } else patternA_ = std::min(1.f, patternA_ + dt / 3.f);
    for (auto& row : fold_)
        for (float& t : row) t = std::min(t + dt, 10.f);
    // the wings' two-beat cycle (flight)
    if (music.hasSong) flapPhase_ = (float)std::fmod(music.beat * 0.5, 1.0);
    else flapPhase_ = std::fmod(flapPhase_ + wdt, 1.f);
    updateCranes(music, dt, wdt, f.paused);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    // ---- Cells folding away (real time, even in slow motion).
    for (Folding& u : folding_) u.t += dt;
    folding_.erase(std::remove_if(folding_.begin(), folding_.end(),
                                  [](const Folding& u) { return u.t > u.delay + (u.delay > 0.f ? 0.52f : 0.34f); }),
                   folding_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    cells_.clear();
    shadows_.clear();
    buildBoard(game);
    buildCranes();
    const vec3 L = lightDir();
    const float px = 2.f * viewH_.y / height_;
    const float seedU = (float)(seed_ % 1000) * 0.137f;
    const float dark = 1.f - smoothstepf(0.25f, 0.5f, ugl::luma(ugl::toSrgb(sheet())));

    // ---- The shadows of everything on the sheet (max blended: overlapping shadows never darken twice).
    shadowT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progCell_);
    setMat(progCell_, "uVP", vp_);
    set1f(progCell_, "uPx", px * 2.f);
    drawCells(shadows_);
    glUseProgram(progTri_);
    setMat(progTri_, "uVP", vp_);
    set1f(progTri_, "uShadowPass", 1.f);
    drawTris(craneShadow_);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);

    // ---- The sheet and the well.
    sceneT_.bind();
    glUseProgram(progPaper_);
    set4f(progPaper_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progPaper_, "uRes", (float)width_, (float)height_);
    set1f(progPaper_, "uSeed", seedU);
    set3f(progPaper_, "uSheet", sheet());
    set1f(progPaper_, "uLight", 0.95f + 0.03f * wm + 0.04f * wp + 0.04f * swell_);
    set1f(progPaper_, "uDark", dark);
    set2f(progPaper_, "uLightXY", L.x, L.y);
    // the sheet's old creases: faint at calm, catching the light at a peak; a new pattern after each drop
    set1f(progPaper_, "uCrease", (0.022f + 0.02f * wm + 0.035f * wp + 0.03f * swell_) * patternA_);
    set1f(progPaper_, "uPattern", (float)pattern_);
    set1f(progPaper_, "uWarm", 0.35f + 0.25f * wm + 0.4f * wp);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowT_.tex);
    set1i(progPaper_, "uShadow", 0);
    set2f(progPaper_, "uShadowTexel", 1.f / shadowT_.w, 1.f / shadowT_.h);
    ugl::drawFullscreen();

    // ---- The pieces (their alpha is the stack's mask for the bridge), the ghost and the cranes (no mask).
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // the cranes and the rows being folded first: a row sliding out passes under the stack settling down
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glUseProgram(progTri_);
    setMat(progTri_, "uVP", vp_);
    set1f(progTri_, "uShadowPass", 0.f);
    drawTris(craneTris_);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progCell_);
    setMat(progCell_, "uVP", vp_);
    set1f(progCell_, "uPx", px);
    set3f(progCell_, "uLightDir", L);
    set3f(progCell_, "uInk", hints().ink);
    set3f(progCell_, "uBack", lin(VARIANTS[variant_].back));
    set1f(progCell_, "uFoil", VARIANTS[variant_].foil);
    drawCells(cells_);

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
    set1f(progFinal_, "uOver", over_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    set3f(progFinal_, "uSheet", sheet());
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_origami() {
    static const UniverseEntry e{
        "ORIGAMI", "folded paper pieces on a sheet; cleared rows fold into paper cranes that gather around the well",
        [] { return std::unique_ptr<Universe>(new OrigamiUniverse()); },
        // Calm, airy songs suit the paper best; it fits most songs.
        [](const SongLook& l) { return 0.85f + 0.2f * (1.f - l.dynamics) + 0.1f * l.airWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

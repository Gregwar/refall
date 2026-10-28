// RECYCLE: the pieces are assembled from heaps of cubes beside the well, and a cleared row goes back to them.
//
// One world: a table seen from straight above, lit from the upper left. The well is a shallow tray in it; on both
// sides, in the margins, lie heaps of loose cubes (the same cubes the pieces are made of), neutral in color, their
// height told by soft shadows and by the layers of a pyramid. Nothing is created or destroyed: the world holds a
// fixed number of cubes. A new piece takes its four cubes from the top of a heap: they fly in (a short eased arc,
// their shadow growing and shrinking with the height) and converge onto the piece, which is playable at once; they
// take the piece's color on the way. A cleared row lifts out cube by cube and flies back to the heaps (left half to
// the left, right half to the right), landing on top with a small bounce, still tinted by their piece for a moment.
// The heaps shrink as the stack grows. The previews are printed silhouettes on the table (the plan, not the
// material); holding a piece sends its cubes back.
//
// The song: a break is still; mid parts move a cube from one heap to another every bar, the chorus every two beats
// (the heaps get tidied); a drop throws every cube in the air on its downbeat and lands them in a new formation
// (rows of colors, concentric rings, towers; a new one every four bars), the cubes take the drop's colors and hop in a wave
// every two beats. Slow motion is a low evening light (long shadows), game over sends the whole stack back.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane (like SUMI); a
// cube above the table is drawn a little larger and further from the center (a long-lens parallax), cubes on the
// table (the stack) stay exactly on the layout.
#include "universes/recycle/recycle.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/recycle/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Outfit (Light for names and numbers, Medium for labels), SIL Open Font License, see OFL-Outfit.txt.
const unsigned char FONT_LIGHT[] = {
#include "universes/recycle/Outfit-Light.ttf.inc"
};
const unsigned char FONT_MEDIUM[] = {
#include "universes/recycle/Outfit-Medium.ttf.inc"
};

namespace shaders = recycle_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// A table (sRGB): the table, the tray, the shadows' tint, HUD ink, accent, the heaps' cubes; the seven piece colors
// (I O T S Z J L) and the drop's seven; shadow strength, concrete mottling, grain, the light's falloff, the tray's
// depth, dark, gloss, wood grain on the heaps' cubes and on the pieces, brightening with height, edge shading.
struct VariantDef {
    const char* name;
    unsigned table, tray, shadowTint, ink, accent, pile;
    unsigned piece[7], drop[7];
    float shadowAmt, mottle, grain, lamp, trayDepth, dark, gloss, wood, woodPiece, lift, edge;
};
const VariantDef VARIANTS[] = {
    // White porcelain: matte white cubes on a cool white table, pastel pieces; color only where the play is.
    {"PORCELAIN", 0xedeef0, 0xe2e4e7, 0x8d939c, 0x2a2e35, 0x3f7fe0, 0xe6e7e9,
     {0x4fb0cb, 0xf0bd45, 0x9f83d6, 0x6dbd84, 0xec7a6c, 0x5a82dc, 0xf19a50},
     {0xff4f6d, 0x14b8a6, 0xffb000, 0x7c5cff, 0x2f9bff, 0xff7a3d, 0x2ec27e},
     0.62f, 0.f, 0.022f, 0.10f, 0.55f, 0.f, 0.f, 0.f, 0.f, 0.10f, 1.f},
    // Onyx: glossy graphite cubes on a black table, jewel pieces.
    {"ONYX", 0x111214, 0x0b0c0d, 0x202020, 0xd9dde3, 0xffd166, 0x2b2d32,
     {0x2ec4d6, 0xffd166, 0xb58cff, 0x52d996, 0xff6b81, 0x5b8cff, 0xff9f4a},
     {0xff3d7f, 0x00e0c6, 0xffe14d, 0x8f6bff, 0x3db4ff, 0xff8a3d, 0x9dff6b},
     0.75f, 0.f, 0.03f, 0.35f, 0.7f, 1.f, 1.f, 0.f, 0.f, 0.16f, 1.2f},
    // Concrete: maple blocks on a concrete slab, painted pieces like a wooden toy's.
    {"CONCRETE", 0xa9a8a4, 0x9d9c98, 0x5a5b60, 0x232427, 0xd9822b, 0xcfae80,
     {0x3d8fb3, 0xe5b53e, 0x8b5aa1, 0x5c9c5a, 0xcc553f, 0x2f4f8f, 0xdc7f2d},
     {0xf25c54, 0x1fb5a4, 0xf7b538, 0x6c63ff, 0x3a86ff, 0xff6fa3, 0x8ac926},
     0.6f, 0.07f, 0.04f, 0.12f, 0.6f, 0.f, 0.f, 1.f, 0.45f, 0.05f, 1.f},
    // Night: slate-blue cubes on a navy table under a soft lamp, pastel pieces.
    {"NIGHT", 0x151b29, 0x10151f, 0x0a0d14, 0xc9d2e4, 0x9fb4f5, 0x3a4560,
     {0x8fd6ea, 0xf6da8c, 0xc9a9f2, 0xa6e2a0, 0xf4a3ab, 0xa2b6f7, 0xf7bf8e},
     {0xff9ecb, 0x7ff0e0, 0xfff07a, 0xb59cff, 0x7fc8ff, 0xffb37a, 0xb6ff8a},
     0.7f, 0.f, 0.03f, 0.3f, 0.65f, 1.f, 0.35f, 0.f, 0.f, 0.12f, 1.1f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

using Palette = RecycleUniverse::Palette;
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
constexpr int PAL_FLOATS = (int)(sizeof(Palette) / sizeof(float));

Palette paletteOf(int v) {
    const VariantDef& d = VARIANTS[((v % VARIANT_COUNT) + VARIANT_COUNT) % VARIANT_COUNT];
    auto L = [](unsigned c) { return ugl::toLinear(hex(c)); };
    Palette p;
    p.table = L(d.table); p.tray = L(d.tray); p.shadowTint = L(d.shadowTint); p.ink = L(d.ink); p.accent = L(d.accent);
    p.pile = L(d.pile);
    for (int i = 0; i < 7; i++) { p.piece[i] = L(d.piece[i]); p.drop[i] = L(d.drop[i]); }
    p.shadowAmt = d.shadowAmt; p.mottle = d.mottle; p.grain = d.grain; p.lamp = d.lamp; p.trayDepth = d.trayDepth;
    p.dark = d.dark; p.gloss = d.gloss; p.wood = d.wood; p.woodPiece = d.woodPiece; p.lift = d.lift; p.edge = d.edge;
    return p;
}

void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < PAL_FLOATS; i++) pa[i] += (pb[i] - pa[i]) * k;
}

std::string titleCase(const std::string& s) {
    std::string o = s;
    bool start = true;
    for (char& c : o) {
        if (std::isalpha((unsigned char)c)) {
            c = start ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
            start = false;
        } else start = c == ' ' || c == '-';
    }
    return o;
}

constexpr float CUBE = 0.9f;      // a cube's side (cells are one unit: a fine gap between cubes)
constexpr float HOVER = 0.35f;    // the falling piece's height above the tray
constexpr float PERSP = 85.f;     // the camera's height (board units): the parallax of cubes above the table
const vec2 LIGHT = {-0.555f, 0.832f}; // toward the light (upper left)

float ease(float u) {
    u = saturate(u);
    return u < 0.5f ? 4.f * u * u * u : 1.f - std::pow(-2.f * u + 2.f, 3.f) * 0.5f;
}
float easeOut(float u) {
    u = saturate(u);
    return 1.f - (1.f - u) * (1.f - u) * (1.f - u);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool RecycleUniverse::init(int width, int height) {
    progTable_ = ugl::program(ugl::FULLSCREEN_VS, shaders::TABLE_FS, "recycle table");
    progShadow_ = ugl::program(shaders::INST_VS, shaders::SHADOW_FS, "recycle shadow");
    progCube_ = ugl::program(shaders::INST_VS, shaders::CUBE_FS, "recycle cube");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "recycle shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "recycle text");
    progPost_ = ugl::program(ugl::FULLSCREEN_VS, shaders::POST_FS, "recycle post");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
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

    buildFont(fontLight_, FONT_LIGHT);
    buildFont(fontMedium_, FONT_MEDIUM);
    pal_ = basePal_ = paletteOf(0);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void RecycleUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] recycle font init failed\n");
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
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
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

void RecycleUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    const bool changed = std::fabs(hw - viewH_.x) > 0.01f;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    shadowT_.create(std::max(1, width_ / 2), std::max(1, height_ / 2), GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    if (changed && !piles_.empty()) switchForm(form_, 0.f); // the margins changed: lay the heaps out again
}

RecycleUniverse::~RecycleUniverse() {
    sceneT_.destroy();
    shadowT_.destroy();
    for (GLuint p : {progTable_, progShadow_, progCube_, progShape_, progText_, progPost_}) glDeleteProgram(p);
    for (GLuint v : {instVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, instVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontLight_, &fontMedium_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The heaps
// =====================================================================================================

// The formations, in each margin (u: from the well's side outward, v: up), clear of the HUD's corners.
void RecycleUniverse::buildForm(Form f, std::vector<Pile>& out) {
    out.clear();
    const float X0 = 12.4f, W = std::max(5.f, std::min(viewH_.x - 0.9f, 21.8f) - X0);
    const float V0 = -7.2f, V1 = 8.3f;
    Rng r(0xC0BEull + (uint64_t)f * 977u);
    for (int side = -1; side <= 1; side += 2) {
        auto pos = [&](float u, float v, float z) { return vec3(side * (X0 + u), v, z); };
        if (f == F_HEAPS) {
            // stepped pyramids of cubes, each course sitting on the gaps of the one below, filled course by course
            const float hu[4] = {2.9f, 6.6f, 2.3f, 6.8f}, hv[4] = {4.4f, -0.2f, -3.6f, -5.2f};
            const int hb[4] = {5, 4, 4, 3};
            for (int h = 0; h < 4; h++) {
                Pile p;
                p.side = side;
                p.colorIdx = (h * 2 + (side > 0 ? 3 : 0)) % 7;
                const float cu = hu[h] * W / 8.9f, cv = hv[h];
                for (int L = 0; L < hb[h]; L++) {
                    const int nb = hb[h] - L;
                    for (int j = 0; j < nb; j++)
                        for (int i = 0; i < nb; i++) {
                            const float du = i - (nb - 1) * 0.5f, dv = j - (nb - 1) * 0.5f;
                            Slot s;
                            s.home = pos(cu + du + r.range(-0.03f, 0.03f), cv + dv + r.range(-0.03f, 0.03f), L * CUBE);
                            s.rot = r.range(-0.045f, 0.045f);
                            s.key = L * 100.f + std::sqrt(du * du + dv * dv) + r.range(0.f, 0.3f);
                            p.slots.push_back(s);
                        }
                }
                std::sort(p.slots.begin(), p.slots.end(), [](const Slot& a, const Slot& b) { return a.key < b.key; });
                p.weight = -(float)(h + 1); // the heaps fill one after the other (whole pyramids, one partial)
                out.push_back(std::move(p));
            }
        } else if (f == F_TOWERS) {
            // towers on a grid, their heights a slope rising toward the well
            const int cols = W > 7.5f ? 3 : 2, rows = 4;
            for (int cj = 0; cj < rows; cj++)
                for (int ci = 0; ci < cols; ci++) {
                    Pile p;
                    p.side = side;
                    p.colorIdx = (ci * 2 + cj * 3 + (side > 0 ? 1 : 0)) % 7;
                    const float u = (ci + 0.5f) * W / cols, v = lerpf(V1 - 1.2f, V0 + 1.0f, cj / (float)(rows - 1));
                    for (int L = 0; L < 7; L++) {
                        Slot s;
                        s.home = pos(u, v, L * CUBE);
                        s.rot = 0.f;
                        s.key = (float)L;
                        p.slots.push_back(s);
                    }
                    p.weight = 0.45f + (cols - ci) * 0.45f + 0.35f * std::sin(cj * 1.7f + side);
                    out.push_back(std::move(p));
                }
        } else if (f == F_WAVE) {
            // rows of cubes with gaps between them, each row a color, a second course laid on the first
            const float pitch = 1.45f;
            const int rows = (int)((V1 - V0 - 1.f) / pitch) + 1;
            const int n = std::max(3, (int)(W / 1.0f));
            const float vtop = (V0 + V1) * 0.5f + (rows - 1) * pitch * 0.5f;
            for (int j = 0; j < rows; j++) {
                Pile p;
                p.side = side;
                p.colorIdx = (j + (side > 0 ? 3 : 0)) % 7;
                const float v = vtop - j * pitch;
                for (int L = 0; L < 2; L++)
                    for (int i = 0; i < n; i++) {
                        Slot sl;
                        sl.home = pos(0.5f + i * (W - 1.f) / (n - 1), v, L * CUBE);
                        sl.rot = 0.f;
                        sl.key = L * 100.f + i;
                        p.slots.push_back(sl);
                    }
                p.weight = 1.f;
                out.push_back(std::move(p));
            }
        } else {
            // concentric rectangles drawn with cubes, a gap between the rings, each ring its own color; they fill
            // from the middle outward, then a second course on the inner ones
            Pile p;
            p.side = side;
            p.colorIdx = -2;
            const float cu = W * 0.5f, cv = (V0 + V1) * 0.5f, pitch = 1.45f;
            const int K = std::max(1, (int)((W * 0.5f - 0.5f) / pitch));
            const float E = std::max(1.f, (V1 - V0) * 0.5f - 0.5f - K * pitch);
            auto ring = [&](int k, int L, float base) {
                const float hx = k * pitch, hy = k * pitch + E;
                std::vector<vec2> pts;
                if (k == 0) {
                    const int m = (int)std::lround(2.f * hy) + 1;
                    for (int j = 0; j < m; j++) pts.push_back({0.f, -hy + 2.f * hy * j / (m - 1)});
                } else {
                    const int nx = (int)std::lround(2.f * hx) + 1, ny = (int)std::lround(2.f * hy) + 1;
                    for (int i = 0; i < nx; i++) {
                        const float x = -hx + 2.f * hx * i / (nx - 1);
                        pts.push_back({x, hy});
                        pts.push_back({x, -hy});
                    }
                    for (int j = 1; j < ny - 1; j++) {
                        const float y = -hy + 2.f * hy * j / (ny - 1);
                        pts.push_back({hx, y});
                        pts.push_back({-hx, y});
                    }
                }
                for (const vec2& q : pts) {
                    Slot sl;
                    sl.home = pos(cu + q.x, cv + q.y, L * CUBE);
                    sl.rot = 0.f;
                    sl.tag = k;
                    // around the ring from the middle of its side facing the well, both ways at once (a partial
                    // ring opens away from the well)
                    const float ax = q.x, ay = std::fabs(q.y); // (u: outward from the well)
                    float arc;
                    if (ax <= -hx + 0.01f) arc = ay;
                    else if (ay >= hy - 0.01f) arc = hy + (ax + hx);
                    else arc = hy + 2.f * hx + (hy - ay);
                    sl.key = base + arc * 0.1f;
                    p.slots.push_back(sl);
                }
            };
            for (int k = 0; k <= K; k++) ring(k, 0, k * 100.f);
            for (int k = 0; k < K; k++) ring(k, 1, (K + 1 + k) * 100.f);
            std::sort(p.slots.begin(), p.slots.end(), [](const Slot& a, const Slot& b2) { return a.key < b2.key; });
            p.weight = 1.f;
            out.push_back(std::move(p));
        }
    }
    for (Pile& p : out) p.occ.assign(p.slots.size(), Occ{});
}

vec3 RecycleUniverse::slotPos(const Pile& p, int i) const {
    const Occ& o = p.occ[i];
    const vec3 home = p.slots[i].home;
    vec3 q = home;
    if (o.t < o.delay + o.dur) {
        const float u = saturate((o.t - o.delay) / o.dur);
        const float e = ease(u);
        q = lerp(o.from, home, e);
        q.z += o.arc * 4.f * u * (1.f - u);
    }
    const float h = o.hopT - o.hopDelay;
    if (h > 0.f && h < 0.36f) q.z += o.hopA * std::sin(PI * h / 0.36f);
    return q;
}

void RecycleUniverse::switchForm(Form f, float spread) {
    std::vector<Pile> np;
    buildForm(f, np);
    for (int side = -1; side <= 1; side += 2) {
        // the cubes on this side, top to bottom of the screen
        struct C { vec3 p; float rot; int tint; float tintAge; };
        std::vector<C> cs;
        for (Pile& p : piles_)
            if (p.side == side)
                for (size_t i = 0; i < p.slots.size(); i++)
                    if (p.occ[i].state == FULL) cs.push_back({slotPos(p, (int)i), p.occ[i].rotShown, p.occ[i].tint, p.occ[i].tintAge});
        const int n = (int)cs.size();
        // each pile's share, by weight (largest remainders), within its capacity
        std::vector<int> idx;
        for (int k = 0; k < (int)np.size(); k++)
            if (np[k].side == side) idx.push_back(k);
        std::vector<int> cnt(idx.size(), 0);
        int left = n;
        if (!idx.empty() && np[idx[0]].weight < 0.f) {
            // in order, each one full
            std::vector<size_t> ord(idx.size());
            for (size_t j = 0; j < ord.size(); j++) ord[j] = j;
            std::sort(ord.begin(), ord.end(), [&](size_t a, size_t b) { return np[idx[a]].weight > np[idx[b]].weight; });
            for (size_t j : ord) {
                cnt[j] = std::min(left, (int)np[idx[j]].slots.size());
                left -= cnt[j];
            }
        }
        for (int round = 0; round < 4 && left > 0; round++) {
            float ws = 0.f;
            for (size_t j = 0; j < idx.size(); j++)
                if (cnt[j] < (int)np[idx[j]].slots.size()) ws += np[idx[j]].weight;
            if (ws <= 0.f) break;
            const int pool = left;
            for (size_t j = 0; j < idx.size() && left > 0; j++) {
                const int cap = (int)np[idx[j]].slots.size() - cnt[j];
                if (cap <= 0) continue;
                const int add = std::min({cap, left, (int)std::floor(pool * np[idx[j]].weight / ws)});
                cnt[j] += add;
                left -= add;
            }
        }
        for (size_t j = 0; left > 0 && j < idx.size() * 40; j++) {
            const size_t k = j % idx.size();
            if (cnt[k] < (int)np[idx[k]].slots.size()) { cnt[k]++; left--; }
        }
        // the target slots, matched to the cubes by height on screen (few crossings)
        struct T { int pile, slot; float v; };
        std::vector<T> ts;
        for (size_t j = 0; j < idx.size(); j++)
            for (int s = 0; s < cnt[j]; s++) ts.push_back({idx[j], s, np[idx[j]].slots[s].home.y});
        std::sort(cs.begin(), cs.end(), [](const C& a, const C& b) { return a.p.y > b.p.y; });
        std::sort(ts.begin(), ts.end(), [](const T& a, const T& b) { return a.v > b.v; });
        for (size_t k = 0; k < ts.size() && k < cs.size(); k++) {
            Occ& o = np[ts[k].pile].occ[ts[k].slot];
            const vec3 home = np[ts[k].pile].slots[ts[k].slot].home;
            o.state = FULL;
            o.from = cs[k].p;
            o.rotFrom = cs[k].rot;
            o.rotShown = cs[k].rot;
            o.tint = cs[k].tint;
            o.tintAge = cs[k].tintAge;
            o.t = 0.f;
            o.delay = spread > 0.f ? rng_.range(0.f, spread) : 0.f;
            const float d = std::sqrt((home.x - o.from.x) * (home.x - o.from.x) + (home.y - o.from.y) * (home.y - o.from.y));
            o.dur = spread > 0.f ? 0.5f + 0.04f * d + rng_.range(0.f, 0.15f) : 0.f;
            o.arc = spread > 0.f ? 0.8f + 0.18f * d + std::fabs(home.z - o.from.z) * 0.3f : 0.f;
            if (spread <= 0.f) o.t = 1.f, o.dur = 0.001f;
            o.shown = o.from;
        }
    }
    piles_ = std::move(np);
    form_ = f;
    // cubes on their way to a heap: a slot in the new formation
    for (Flight& fl : flights_) {
        if (fl.target != T_PILE) continue;
        int pi, si;
        const int side = fl.last.x < 0.f ? -1 : 1;
        if (reserveSlot(side, fl.last, pi, si) || reserveSlot(-side, fl.last, pi, si)) {
            fl.a = pi; fl.b = si;
            fl.from = fl.t > fl.delay ? fl.last : fl.from;
            const float rest = std::max(0.25f, fl.dur - std::max(0.f, fl.t - fl.delay));
            fl.t = 0.f; fl.delay = 0.f; fl.dur = rest;
        } else fl.a = -1;
    }
}

bool RecycleUniverse::takeCube(int side, vec3& pos, float& rot, int& tint) {
    int best = -1;
    float bf = -1.f;
    for (int k = 0; k < (int)piles_.size(); k++) {
        const Pile& p = piles_[k];
        if (p.side != side) continue;
        int n = 0;
        for (const Occ& o : p.occ) n += o.state == FULL;
        if (n == 0) continue;
        // the fullest (by share); heaps that fill in order: the last one holding cubes
        const float fr = p.weight < 0.f ? -p.weight : n / std::max(0.1f, p.weight) + rng_.range(0.f, 0.02f);
        if (fr > bf) { bf = fr; best = k; }
    }
    if (best < 0) return false;
    Pile& p = piles_[best];
    for (int i = (int)p.occ.size() - 1; i >= 0; i--)
        if (p.occ[i].state == FULL) {
            pos = slotPos(p, i);
            rot = p.occ[i].rotShown;
            tint = p.occ[i].tint;
            p.occ[i] = Occ{};
            return true;
        }
    return false;
}

bool RecycleUniverse::reserveSlot(int side, const vec3& near, int& pile, int& slot) {
    int best = -1;
    float bf = 1e9f;
    for (int k = 0; k < (int)piles_.size(); k++) {
        const Pile& p = piles_[k];
        if (p.side != side) continue;
        int n = 0;
        bool room = false;
        for (const Occ& o : p.occ) { n += o.state != EMPTY; room |= o.state == EMPTY; }
        if (!room) continue;
        const vec3 c = p.slots[0].home;
        const float d = std::sqrt((c.x - near.x) * (c.x - near.x) + (c.y - near.y) * (c.y - near.y));
        // the emptiest, near; heaps that fill in order: the first one with room
        const float fr = p.weight < 0.f ? -p.weight : n / std::max(0.1f, p.weight) + d * 0.004f;
        if (fr < bf) { bf = fr; best = k; }
    }
    if (best < 0) return false;
    Pile& p = piles_[best];
    for (int i = 0; i < (int)p.occ.size(); i++)
        if (p.occ[i].state == EMPTY) {
            p.occ[i] = Occ{};
            p.occ[i].state = RESERVED;
            pile = best;
            slot = i;
            return true;
        }
    return false;
}

int RecycleUniverse::pileCubes() const {
    int n = 0;
    for (const Pile& p : piles_)
        for (const Occ& o : p.occ) n += o.state == FULL;
    return n;
}

int RecycleUniverse::boardCubes(const Game& g) const {
    int n = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) n += g.cell(x, y).type >= 0;
    return n;
}

// A hop rippling through the heaps, outward from the well.
void RecycleUniverse::ripple(float amp, float spread, float delay) {
    for (Pile& p : piles_)
        for (size_t i = 0; i < p.occ.size(); i++) {
            Occ& o = p.occ[i];
            if (o.state != FULL) continue;
            const vec3& h = p.slots[i].home;
            o.hopT = 0.f;
            o.hopDelay = delay + spread * saturate((std::fabs(h.x) - 12.f) / 9.f) + 0.03f * h.z;
            o.hopA = amp * (0.85f + 0.3f * rng_.uniform()) * (1.f - 0.1f * std::min(h.z, 4.f));
        }
}

// The heaps get tidied: a cube from the top of the fullest heap hops onto the emptiest one, on the same side.
void RecycleUniverse::hopBetweenPiles() {
    const int side = rng_.chance(0.5f) ? -1 : 1;
    vec3 p;
    float rot;
    int tint;
    int count = 0;
    for (const Pile& q : piles_) count += q.side == side;
    if (count < 2) return;
    if (!takeCube(side, p, rot, tint)) return;
    int pi, si;
    if (!reserveSlot(side, p, pi, si)) return;
    Flight fl;
    fl.from = p;
    fl.rotFrom = rot;
    fl.target = T_PILE;
    fl.a = pi; fl.b = si;
    fl.dur = 0.55f;
    fl.arc = 1.4f;
    fl.type = fl.fromType = tint;
    fl.fromTint = 0.f;
    fl.realTime = false;
    fl.last = p;
    flights_.push_back(fl);
}

// =====================================================================================================
// Cubes in the air
// =====================================================================================================

vec3 RecycleUniverse::targetPos(const Flight& fl, const Game& g) const {
    if (fl.target == T_PIECE) {
        if (g.hasPiece() && pieceIn_) {
            int c[4][2];
            g.pieceCells(g.piece(), c);
            const float dx = g.pieceVisualX() - g.piece().x, dy = g.pieceVisualY() - g.piece().y;
            const vec3 w = layout::cellPos(c[fl.a][0] + dx, c[fl.a][1] + dy);
            return {w.x, w.y, HOVER};
        }
        return fl.last;
    }
    if (fl.target == T_CELL) {
        const vec3 w = layout::cellPos((float)fl.a, fl.b - g.rowOffset(fl.b));
        return {w.x, w.y, 0.f};
    }
    if (fl.a >= 0 && fl.a < (int)piles_.size() && fl.b < (int)piles_[fl.a].slots.size()) return piles_[fl.a].slots[fl.b].home;
    return fl.last;
}

void RecycleUniverse::sendHome(const vec3& from, int type, float delay, float dur, float arc, bool realTime) {
    int side = from.x < -0.01f ? -1 : (from.x > 0.01f ? 1 : (rng_.chance(0.5f) ? -1 : 1));
    int pi, si;
    if (!reserveSlot(side, from, pi, si) && !reserveSlot(-side, from, pi, si)) return; // no room: never (capacity > cubes)
    Flight fl;
    fl.from = from;
    fl.target = T_PILE;
    fl.a = pi; fl.b = si;
    fl.delay = delay;
    fl.dur = dur;
    fl.arc = arc;
    fl.type = fl.fromType = type;
    fl.spin = (rng_.chance(0.5f) ? 1.f : -1.f) * PI * 0.5f;
    fl.realTime = realTime;
    fl.last = from;
    flights_.push_back(fl);
}

// A new piece: four cubes from the top of a heap fly to it.
void RecycleUniverse::assemblePiece(const Game& g, bool instant) {
    pieceIn_ = true;
    pieceType_ = g.piece().type;
    for (int k = 0; k < 4; k++) pieceArrived_[k] = instant;
    if (instant) {
        for (int k = 0; k < 4; k++) {
            vec3 p; float rot; int tint;
            if (!takeCube(1, p, rot, tint)) takeCube(-1, p, rot, tint);
        }
        return;
    }
    // from the fuller side (both sides take turns when they are even)
    int nl = 0, nr = 0;
    for (const Pile& p : piles_)
        for (const Occ& o : p.occ) (p.side < 0 ? nl : nr) += o.state == FULL;
    static int alt = 0;
    alt ^= 1;
    const int side = std::abs(nl - nr) > 6 ? (nl > nr ? -1 : 1) : (alt ? -1 : 1);
    int c[4][2];
    g.pieceCells(g.piece(), c);
    // the cell nearest the heaps gets the first cube
    int order[4] = {0, 1, 2, 3};
    std::sort(order, order + 4, [&](int a, int b) { return c[a][0] * side > c[b][0] * side; });
    for (int j = 0; j < 4; j++) {
        const int k = order[j];
        Flight fl;
        float rot = 0.f;
        int tint = -1;
        if (!takeCube(side, fl.from, rot, tint) && !takeCube(-side, fl.from, rot, tint))
            fl.from = vec3(side * 16.f, 0.f, 6.f); // (never: the world holds more cubes than the well)
        fl.rotFrom = rot;
        fl.spin = (rng_.chance(0.5f) ? 1.f : -1.f) * PI * 0.5f;
        fl.target = T_PIECE;
        fl.a = k;
        fl.delay = 0.03f * j;
        fl.dur = 0.36f + 0.02f * j;
        fl.arc = 1.3f;
        fl.type = pieceType_;
        fl.fromType = tint;
        fl.fromTint = 1.f;
        fl.realTime = true;
        fl.last = fl.from;
        flights_.push_back(fl);
    }
}

// The stack as the game has it vs as we know it (a board filled behind our back, a universe entering mid-game):
// cubes fly in from the heaps, or back to them.
void RecycleUniverse::reconcile(const Game& g) {
    if (fresh_) {
        fresh_ = false;
        flights_.clear();
        std::memset(incoming_, 0, sizeof(incoming_));
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) mirror_[y][x] = g.cell(x, y).type >= 0;
        buildForm(form_, piles_);
        const int n = std::max(0, total_ - boardCubes(g) - (g.hasPiece() ? 4 : 0));
        // n cubes in the slots, half on each side, then shared out between the piles by weight
        for (Pile& p : piles_) std::fill(p.occ.begin(), p.occ.end(), Occ{});
        {
            int left = n;
            for (int side = -1; side <= 1; side += 2) {
                int want = side < 0 ? n / 2 : n - n / 2;
                for (Pile& p : piles_) {
                    if (p.side != side) continue;
                    for (size_t i = 0; i < p.occ.size() && want > 0 && left > 0; i++) {
                        p.occ[i].state = FULL;
                        p.occ[i].shown = p.slots[i].home;
                        p.occ[i].rotShown = p.slots[i].rot;
                        want--;
                        left--;
                    }
                }
            }
            switchForm(form_, 0.f);
        }
        pieceIn_ = g.hasPiece();
        pieceType_ = g.hasPiece() ? g.piece().type : -1;
        for (bool& a : pieceArrived_) a = true;
        return;
    }
    int k = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool has = g.cell(x, y).type >= 0;
            if (has == mirror_[y][x]) continue;
            mirror_[y][x] = has;
            const vec3 w = layout::cellPos((float)x, (float)y);
            if (has) {
                Flight fl;
                float rot = 0.f;
                int tint = -1;
                const int side = x < 5 ? -1 : 1;
                if (!takeCube(side, fl.from, rot, tint) && !takeCube(-side, fl.from, rot, tint)) continue;
                fl.rotFrom = rot;
                fl.target = T_CELL;
                fl.a = x; fl.b = y;
                fl.delay = 0.006f * k++;
                fl.dur = 0.45f;
                fl.arc = 1.5f;
                fl.type = g.cell(x, y).type;
                fl.fromType = tint;
                fl.fromTint = 1.f;
                fl.last = fl.from;
                flights_.push_back(fl);
                incoming_[y][x] = true;
            } else {
                if (incoming_[y][x]) {
                    for (Flight& fl : flights_)
                        if (fl.target == T_CELL && fl.a == x && fl.b == y) {
                            int pi, si;
                            const int side = fl.last.x < 0 ? -1 : 1;
                            if (reserveSlot(side, fl.last, pi, si) || reserveSlot(-side, fl.last, pi, si)) {
                                fl.target = T_PILE; fl.a = pi; fl.b = si;
                                fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                            }
                        }
                    incoming_[y][x] = false;
                } else sendHome(vec3(w.x, w.y, 0.f), g.cell(x, y).type, 0.006f * k++, 0.55f, 1.6f, true);
            }
        }
    if (g.hasPiece() && !g.over() && !pieceIn_) assemblePiece(g, false);
    if ((!g.hasPiece() || g.over()) && pieceIn_) {
        // the piece went away without locking (game over): its cubes go back
        for (Flight& fl : flights_)
            if (fl.target == T_PIECE) {
                int pi, si;
                const int side = fl.last.x < 0 ? -1 : 1;
                if (reserveSlot(side, fl.last, pi, si) || reserveSlot(-side, fl.last, pi, si)) {
                    fl.target = T_PILE; fl.a = pi; fl.b = si; fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                } else fl.a = -1;
            }
        if (lastPieceOk_)
            for (int j = 0; j < 4; j++)
                if (pieceArrived_[j]) sendHome(lastPiece_[j], lastPieceType_, 0.05f * j, 0.6f, 1.6f, true);
        pieceIn_ = false;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void RecycleUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = h < 0.5f ? 1 : 3;
        else if (t.hasSong && !t.minor && t.brightness > 0.55f) v = h < 0.6f ? 0 : 2;
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x2EC7ull);
    if (seconds <= 0.f) palSet_ = false;
}

void RecycleUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2 && dropMode_) {
        dropMode_ = false;
        if (form_ != F_HEAPS && !piles_.empty()) switchForm(F_HEAPS, seconds < 0.1f ? 0.f : 1.0f);
    }
    (void)impact;
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
    lvDur_ = std::clamp(seconds, rising ? 1.5f : 3.f, 6.f);
}

// The drop: every cube in the heaps leaps on the downbeat and lands in a new formation, in the drop's colors.
void RecycleUniverse::drop(float strength) {
    dropMode_ = true;
    const Form forms[3] = {F_WAVE, F_RINGS, F_TOWERS};
    if (!piles_.empty()) {
        switchForm(forms[dropForm_ % 3], 0.12f);
        for (Pile& p : piles_)
            for (Occ& o : p.occ)
                if (o.state == FULL) o.arc += 1.6f * (0.6f + 0.4f * strength);
    }
    dropForm_++;
    lastTick_ = -1.0;
    ticks_ = 0;
}

void RecycleUniverse::refresh(float strength) { ripple(0.25f + 0.3f * strength, 0.5f); }

void RecycleUniverse::levelUp() { ripple(0.45f, 0.6f); }

vec2 RecycleUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints RecycleUniverse::hints() const {
    const Palette p = paletteOf(variant_);
    TransitionHints h;
    h.background = p.table;
    h.light = p.dark < 0.5f;
    h.ink = p.dark > 0.5f ? p.piece[0] : p.ink;
    h.accent = p.accent;
    h.styles = TS_REBUILD | TS_TILES | TS_LUMA;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void RecycleUniverse::onEvent(const GameEvent& ev, const Game& game) {
    if (fresh_) return; // the first frame takes the board as it is
    switch (ev.type) {
    case GameEvent::Spawn:
        if (pieceIn_) {
            // held: the old piece's cubes go back to the heaps
            for (Flight& fl : flights_)
                if (fl.target == T_PIECE) {
                    int pi, si;
                    const int side = fl.last.x < 0 ? -1 : 1;
                    if (reserveSlot(side, fl.last, pi, si) || reserveSlot(-side, fl.last, pi, si)) {
                        fl.target = T_PILE; fl.a = pi; fl.b = si; fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                        fl.dur = 0.5f;
                    } else fl.a = -1;
                }
            if (lastPieceOk_)
                for (int j = 0; j < 4; j++)
                    if (pieceArrived_[j]) sendHome(lastPiece_[j], lastPieceType_, 0.02f * j, 0.5f, 1.3f, true);
            pieceIn_ = false;
        }
        if (game.hasPiece()) assemblePiece(game, false);
        break;
    case GameEvent::Lock: {
        for (int k = 0; k < (int)ev.cells.size() && k < 4; k++) {
            const auto& c = ev.cells[k];
            const bool in = c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W;
            if (in) mirror_[c.y][c.x] = true;
            for (Flight& fl : flights_)
                if (fl.target == T_PIECE && fl.a == k) {
                    if (in) { fl.target = T_CELL; fl.a = c.x; fl.b = c.y; incoming_[c.y][c.x] = true; }
                    else {
                        int pi, si;
                        if (reserveSlot(fl.last.x < 0 ? -1 : 1, fl.last, pi, si)) {
                            fl.target = T_PILE; fl.a = pi; fl.b = si; fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                        } else fl.a = -1;
                    }
                }
        }
        pieceIn_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        const bool tetris = ev.count >= 4;
        // each cube lifts out of its row and flies back to the heaps on its side
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H) continue;
            const vec3 w = layout::cellPos((float)c.x, (float)c.y);
            const float delay = 0.018f * std::fabs(c.x - 4.5f) + (tetris ? 0.03f * (Game::H - 1 - c.y) : 0.f);
            if (incoming_[c.y][c.x]) {
                for (Flight& fl : flights_)
                    if (fl.target == T_CELL && fl.a == c.x && fl.b == c.y) {
                        int pi, si;
                        const int side = c.x < 5 ? -1 : 1;
                        if (reserveSlot(side, fl.last, pi, si) || reserveSlot(-side, fl.last, pi, si)) {
                            fl.target = T_PILE; fl.a = pi; fl.b = si; fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                            fl.dur = 0.6f;
                        } else fl.a = -1;
                    }
                incoming_[c.y][c.x] = false;
            } else {
                sendHome(vec3(w.x - (c.x < 5 ? 0.001f : -0.001f), w.y, 0.f), c.type, delay,
                         0.62f + rng_.range(0.f, 0.1f), tetris ? 3.f : 2.1f, true);
            }
        }
        // compact like the game did (our view of the stack, the cubes still flying to it)
        int newY[Game::H];
        {
            bool nm[Game::H][Game::W] = {}, ni[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                newY[y] = -1;
                if (rows[y]) continue;
                std::memcpy(nm[dst], mirror_[y], sizeof(mirror_[y]));
                std::memcpy(ni[dst], incoming_[y], sizeof(incoming_[y]));
                newY[y] = dst;
                dst--;
            }
            std::memcpy(mirror_, nm, sizeof(mirror_));
            std::memcpy(incoming_, ni, sizeof(incoming_));
        }
        for (Flight& fl : flights_)
            if (fl.target == T_CELL && fl.b >= 0 && fl.b < Game::H && newY[fl.b] >= 0) fl.b = newY[fl.b];
        if (tetris) ripple(0.7f, 0.5f, 0.75f);
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        // the whole stack is recycled, from the top down
        int k = 0;
        std::vector<GameEvent::CellInfo> cs = ev.cells;
        std::sort(cs.begin(), cs.end(), [](const GameEvent::CellInfo& a, const GameEvent::CellInfo& b) { return a.y < b.y; });
        for (auto& c : cs) {
            if (c.y < 0 || c.y >= Game::H) continue;
            if (incoming_[c.y][c.x]) continue;
            const vec3 w = layout::cellPos((float)c.x, (float)c.y);
            sendHome(vec3(w.x, w.y, 0.f), c.type, 0.25f + 0.012f * k++, 0.8f, 1.8f, true);
        }
        for (Flight& fl : flights_)
            if (fl.target == T_CELL) {
                int pi, si;
                if (reserveSlot(fl.last.x < 0 ? -1 : 1, fl.last, pi, si)) {
                    fl.target = T_PILE; fl.a = pi; fl.b = si; fl.from = fl.last; fl.t = 0.f; fl.delay = 0.f;
                } else fl.a = -1;
            }
        std::memset(mirror_, 0, sizeof(mirror_));
        std::memset(incoming_, 0, sizeof(incoming_));
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Drawing
// =====================================================================================================

vec3 RecycleUniverse::pileColor(const Pile& p, const Occ& o, int slot) const {
    vec3 c = pal_.pile;
    if (o.tint >= 0) {
        const float k = 0.08f + 0.82f * std::exp(-o.tintAge / 1.1f);
        c = lerp(c, pieceCol(o.tint), k);
    }
    if (colAmt_ > 0.001f) {
        int ci = p.colorIdx;
        if (ci == -2) {
            ci = p.slots[slot].tag * 2 + (p.side > 0 ? 3 : 0);
        }
        else if (ci < 0) ci = (int)(slot * 7 / std::max<size_t>(1, p.slots.size()) + (p.side > 0 ? 3 : 0)) % 7;
        c = lerp(c, pal_.drop[ci % 7], 0.92f * colAmt_);
    }
    return c;
}

void RecycleUniverse::addCube(const vec3& p, float rot, const vec3& col, float alpha, float squash, float seed, bool piece) {
    if (alpha <= 0.003f) return;
    Drawn d;
    d.p = p;
    d.rot = rot;
    d.half = CUBE * 0.5f;
    d.col = col;
    d.alpha = alpha;
    d.squash = squash;
    d.gloss = pal_.gloss;
    d.wood = piece ? pal_.woodPiece : pal_.wood;
    d.edge = pal_.edge;
    d.seed = seed;
    drawn_.push_back(d);
}

void RecycleUniverse::flushCubes() {
    std::stable_sort(drawn_.begin(), drawn_.end(), [](const Drawn& a, const Drawn& b) { return a.p.z < b.p.z; });
    const float len = 0.62f * (1.f + 1.7f * slow_);
    const vec2 sd = {-LIGHT.x * len, -LIGHT.y * len};
    for (const Drawn& d : drawn_) {
        // the long-lens parallax: higher cubes a little larger and further from the middle
        const float z = std::max(0.f, d.p.z);
        const float k = PERSP / (PERSP - std::min(z, PERSP * 0.5f));
        const float x = viewC_.x + (d.p.x - viewC_.x) * k, y = viewC_.y + (d.p.y - viewC_.y) * k;
        const float half = d.half * k;
        // its shadow on the table
        {
            const float h = z + CUBE * 0.5f;
            Inst s{{d.p.x + sd.x * h, d.p.y + sd.y * h, d.half + 0.03f + 0.02f * z, d.rot},
                   {0.f, d.alpha * 0.75f / (1.f + 0.18f * z), 0.f, 0.f}, {-1.f, z, 0.f, 0.f}, {0.f, 0.f, 0.07f + 0.09f * z, 0.f}};
            shadows_.push_back(s);
        }
        // its soft shadow on the cubes under it
        if (z > 0.6f) {
            const float h = std::min(z, 1.8f) * 0.55f;
            Inst s{{x + sd.x * h, y + sd.y * h, half * (1.f + 0.04f * h), d.rot}, {0.f, 0.f, 0.f, d.alpha * 0.5f * pal_.shadowAmt / (1.f + 0.2f * z)},
                   {1.f, z, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}};
            cubes_.push_back(s);
        }
        Inst c{{x, y, half, d.rot}, {d.col.x, d.col.y, d.col.z, d.alpha}, {0.f, z, d.seed, 0.f}, {d.gloss, d.wood, d.squash, d.edge}};
        cubes_.push_back(c);
    }
    drawn_.clear();
}

void RecycleUniverse::buildScene(const Game& g) {
    const float bridgeA = 1.f - smoothstepf(0.f, 0.8f, bridge_.amount);
    auto maskOf = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    auto flat = [&](float x, float y, float half, int mask, const vec3& col, float alpha, int kind, float fill = 0.f) {
        if (alpha <= 0.003f) return;
        flat_.push_back({{x, y, half, 0.f}, {col.x, col.y, col.z, alpha}, {(float)kind, 0.f, 0.f, (float)mask}, {0.f, 0.f, fill, 0.f}});
    };
    // ---- The heaps.
    for (Pile& p : piles_)
        for (size_t i = 0; i < p.occ.size(); i++) {
            Occ& o = p.occ[i];
            if (o.state != FULL) continue;
            const vec3 q = slotPos(p, (int)i);
            float rot = p.slots[i].rot;
            if (o.t < o.delay + o.dur) rot = lerpf(o.rotFrom, rot, ease(saturate((o.t - o.delay) / o.dur)));
            o.shown = q;
            o.rotShown = rot;
            addCube(q, rot, pileColor(p, o, (int)i), bridgeA, 0.f, (float)((i * 37 + p.side * 11) % 97), false);
        }
    // ---- The stack: cubes resting in the tray (a hard drop squashes them a little as they land).
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0 || incoming_[y][x]) continue;
            const vec3 w = layout::cellPos((float)x, y - g.rowOffset(y));
            const float age = c.flash > 0.f ? (1.f - c.flash) * Game::LOCK_FX_SECONDS : 9.f;
            const float z = HOVER * (1.f - smoothstepf(0.f, 0.07f, age));
            const float sq = c.dropped && age < 0.25f ? 0.06f * (1.f - age / 0.25f) * smoothstepf(0.f, 0.05f, age) : 0.f;
            addCube(vec3(w.x, w.y, z), 0.f, pieceCol(c.type), 1.f, sq, (float)((x * 7 + y * 13) % 97), true);
        }
    // ---- The ghost (an outline on the tray) and the piece, hovering.
    lastPieceOk_ = false;
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
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
                flat(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), lerp(pieceCol(p.type), pal_.ink, pal_.dark > 0.5f ? 0.f : 0.25f),
                     pal_.dark > 0.5f ? 0.75f : 0.85f, 3);
            }
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (int k = 0; k < 4; k++) {
            const vec3 w = layout::cellPos(cells[k][0] + dx, cells[k][1] + dy);
            lastPiece_[k] = vec3(w.x, w.y, HOVER);
            if (!pieceIn_ || !pieceArrived_[k]) continue;
            if (cells[k][1] < Game::HIDDEN - 1) continue;
            addCube(vec3(w.x, w.y, HOVER), 0.f, pieceCol(p.type), 1.f, 0.f, (float)(k * 29 % 97), true);
        }
        lastPieceOk_ = true;
        lastPieceType_ = p.type;
    }
    // ---- Cubes in the air.
    for (Flight& fl : flights_) {
        if (fl.a < 0) continue;
        const vec3 to = targetPos(fl, g);
        const float u = saturate((fl.t - fl.delay) / fl.dur);
        const float e = ease(u);
        vec3 q = lerp(fl.from, to, e);
        q.z += fl.arc * 4.f * u * (1.f - u);
        fl.last = q;
        const float targetRot = fl.target == T_PILE && fl.a < (int)piles_.size() && fl.b < (int)piles_[fl.a].slots.size()
                                    ? piles_[fl.a].slots[fl.b].rot : 0.f;
        const float rot = lerpf(fl.rotFrom, targetRot + fl.spin, e);
        // the color: a heap's cube takes its piece's color on the way; a returning one keeps it
        vec3 from = pal_.pile;
        if (fl.fromType >= 0) from = lerp(pal_.pile, pieceCol(fl.fromType), fl.target == T_PILE ? 1.f : 0.08f);
        const vec3 toC = fl.type >= 0 ? pieceCol(fl.type) : pal_.pile;
        const vec3 col = fl.target == T_PILE ? toC : lerp(from, toC, smoothstepf(0.f, 0.6f, u));
        addCube(q, rot, col, fl.target == T_PILE ? bridgeA : 1.f, 0.f, (float)(((size_t)&fl - (size_t)flights_.data()) / sizeof(Flight) % 97), true);
    }
    // ---- Previews: the pieces' silhouettes, printed on the table.
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
            flat(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), lerp(pal_.table, pieceCol(type), alpha), 1.f, 2);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 0.95f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), i == 0 ? 0.95f : 0.8f);
    }
    // ---- The bonus gauge: twelve pips, one per line.
    {
        const vec3 gc = layout::GAUGE_CENTER;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int n = Game::BONUS_LINES;
        const bool ready = g.bonusReady() || g.bonusActive();
        for (int i = 0; i < n; i++) {
            const float a = PI * 0.5f - TAU * i / n;
            const float x = gc.x + std::cos(a) * layout::GAUGE_R * 0.82f, y = gc.y + std::sin(a) * layout::GAUGE_R * 0.82f;
            const float f = saturate(fill * n - i);
            const vec3 col = ready ? pal_.accent : pal_.ink;
            flat(x, y, 0.13f, 0, col, (0.35f + 0.6f * f) * bridgeA, 4, f);
        }
    }
}

void RecycleUniverse::drawInst(GLuint prog, const std::vector<Inst>& v) {
    if (v.empty()) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Inst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

// =====================================================================================================
// Text
// =====================================================================================================

float RecycleUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
    if (!f.ok || s.empty()) return s.size() * size * 0.5f;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
    const float k = size / f.px;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv * k + track;
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            w += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
    return w - track;
}

void RecycleUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
                                std::vector<float>& out) const {
    if (!f.ok) return;
    const stbtt_fontinfo* info = (const stbtt_fontinfo*)f.info;
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
        if (i + 1 < s.size()) {
            unsigned char n = (unsigned char)s[i + 1];
            if (n < 32 || n >= 128) n = '?';
            pen += stbtt_GetCodepointKernAdvance(info, ch, n) * f.scaleTT * k;
        }
    }
}

void RecycleUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    const float b = smoothstepf(0.3f, 0.95f, bridge_.amount);
    set3f(progText_, "uInk", lerp(ink, bridge_.ink, b));
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// The HUD in RECYCLE's lettering: Outfit, light and generous for names and numbers, small spaced capitals for
// labels, like the labels on a designer's storage boxes.
void RecycleUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const float s = height_ / 1080.f;
    if (!panels.empty()) {
        std::vector<float> v;
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? ugl::toLinear(r.color) : r.accent ? pal_.accent : pal_.table;
            const float a = std::min(1.f, r.alpha);
            const float q[6][6] = {{r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a},
                                   {r.x, r.y, c.x, c.y, c.z, a}, {r.x + r.w, r.y + r.h, c.x, c.y, c.z, a}, {r.x, r.y + r.h, c.x, c.y, c.z, a}};
            for (auto& p : q) v.insert(v.end(), p, p + 6);
        }
        glUseProgram(progShape_);
        set2f(progShape_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
    }
    const vec3 ink = pal_.ink, accent = pal_.accent;
    std::vector<float> q;
    for (const HudText& h : hud) {
        if (h.alpha <= 0.01f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha);
        q.clear();
        auto place = [&](const Font& f, const std::string& text, float size, float track, float dy = 0.f) {
            const float w = textWidth(f, text, size, track);
            textQuads(f, text, ugl::hudLeft(h, w, width_), h.y + dy, size, track, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE: {
            std::string name = h.text, var;
            const size_t dot = name.find("\xC2\xB7");
            if (dot != std::string::npos) { var = name.substr(dot + 2); name = name.substr(0, dot); }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!var.empty() && var.front() == ' ') var.erase(var.begin());
            name = titleCase(name);
            const float sz = base * 1.2f, vsz = base * 0.6f, vtr = vsz * 0.34f;
            const float wName = textWidth(fontLight_, name, sz, sz * 0.02f);
            const float wVar = var.empty() ? 0.f : textWidth(fontMedium_, var, vsz, vtr);
            const float gap = base * 0.7f;
            const float x = ugl::hudLeft(h, wName + (var.empty() ? 0.f : gap + wVar), width_);
            textQuads(fontLight_, name, x, h.y - base * 0.1f, sz, sz * 0.02f, q);
            const float a = saturate(0.35f + h.alpha * 1.6f);
            drawText(fontLight_, q, ink, a);
            if (!var.empty()) {
                q.clear();
                textQuads(fontMedium_, var, x + wName + gap, h.y + base * 0.22f, vsz, vtr, q);
                drawText(fontMedium_, q, ink, a * 0.75f);
            }
            continue;
        }
        case HUD_SECTION:
            place(fontMedium_, h.text, base * 0.9f, base * 0.42f);
            drawText(fontMedium_, q, ink, saturate(0.2f + h.alpha * 1.9f));
            continue;
        case HUD_TITLE:
            place(fontLight_, h.text, base * 1.15f, 0.f, -base * 0.1f);
            drawText(fontLight_, q, ink, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontMedium_, h.text, base * 0.68f, base * 0.16f);
            drawText(fontMedium_, q, ink, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_LABEL:
            place(fontMedium_, h.text, base * 0.66f, base * 0.4f);
            drawText(fontMedium_, q, ink, saturate(alpha * 1.2f) * 0.62f);
            continue;
        case HUD_VALUE:
            place(fontLight_, h.text, base * 1.12f, base * 0.04f);
            drawText(fontLight_, q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(fontLight_, h.text, base * 1.1f, 0.f);
            drawText(fontLight_, q, accent, alpha);
            continue;
        case HUD_KEY:
            place(fontMedium_, h.text, base * 0.8f, base * 0.1f);
            drawText(fontMedium_, q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(fontMedium_, h.text, base * 1.25f, base * 0.5f, -base * 0.15f);
            drawText(fontMedium_, q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontLight_, h.text, base * 0.95f, 0.f);
            drawText(fontLight_, q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontLight_, h.text, base * 1.25f, 0.f, -base * 0.1f);
                drawText(fontLight_, q, ink, alpha);
            } else {
                place(fontMedium_, h.text, base * 0.82f, base * 0.1f);
                drawText(fontMedium_, q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(fontMedium_, h.text, base * 0.78f, base * 0.06f);
            drawText(fontMedium_, q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void RecycleUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    {
        const Palette target = paletteOf(variant_);
        if (!palSet_) { basePal_ = target; palSet_ = true; }
        else lerpPalette(basePal_, target, 1.f - std::exp(-dt * 1.0f));
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 0.9f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float rdt = f.paused ? 0.f : dt;                       // real time (the play: clears, new pieces)
    const float wdt = rdt * lerpf(1.f, 0.4f, slow_);              // the world's time (slower in slow motion)
    worldTime_ += wdt;

    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    dropAmt_ = approach(dropAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 3.f : 0.5f, dt);
    colAmt_ = approach(colAmt_, dropMode_ ? 1.f : 0.f, dropMode_ ? 6.f : 0.6f, dt);
    pal_ = basePal_;
    for (int i = 0; i < 7; i++) pal_.piece[i] = lerp(basePal_.piece[i], basePal_.drop[i], smoothstepf(0.f, 1.f, colAmt_));
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);

    reconcile(game);

    // ---- The beat: every two beats (half time).
    if (!f.paused && music.hasSong && !piles_.empty()) {
        const double tick = std::floor(music.beat / 2.0 + 1e-4);
        if (lastTick_ < 0.0 || tick < lastTick_ - 4.0) lastTick_ = tick;
        if (tick > lastTick_) {
            lastTick_ = tick;
            ticks_++;
            const bool bar = std::fmod(tick, 2.0) < 0.5;
            if (dropMode_ && lv_ > 1.5f) {
                if (ticks_ % 8 == 0) {
                    // a new formation every four bars
                    const Form forms[3] = {F_WAVE, F_RINGS, F_TOWERS};
                    switchForm(forms[dropForm_ % 3], 0.5f);
                    dropForm_++;
                } else {
                    // a hop rippling outward from the well, stronger with the kick, on every other beat
                    const float k = saturate(std::max(music.kick, kickIn_));
                    ripple((bar ? 0.7f : 0.48f) * (0.8f + 0.45f * k), 0.35f);
                }
            } else if (lv_ > 1.5f) {
                hopBetweenPiles();
                if (energy_ > 0.6f) hopBetweenPiles();
            } else if (lv_ > 0.5f) {
                if (bar) hopBetweenPiles();
            }
        }
    }

    // ---- Time.
    for (Pile& p : piles_)
        for (Occ& o : p.occ) {
            if (o.state != FULL) continue;
            o.t += wdt;
            o.hopT += wdt;
            o.tintAge += rdt;
        }
    for (Flight& fl : flights_) {
        fl.t += fl.realTime ? rdt : wdt;
        // a cube coming back takes the lowest free place of its heap (cubes were taken meanwhile): no cube left
        // hanging over a gap
        if (fl.target == T_PILE && fl.a >= 0 && fl.a < (int)piles_.size()) {
            std::vector<Occ>& oc = piles_[fl.a].occ;
            for (int i = 0; i < fl.b && i < (int)oc.size(); i++)
                if (oc[i].state == EMPTY) {
                    oc[i].state = RESERVED;
                    oc[fl.b] = Occ{};
                    fl.b = i;
                    break;
                }
        }
    }
    // landed
    for (Flight& fl : flights_) {
        if (fl.t < fl.delay + fl.dur || fl.a < 0) continue;
        if (fl.target == T_PIECE) {
            if (fl.a >= 0 && fl.a < 4) pieceArrived_[fl.a] = true;
        } else if (fl.target == T_CELL) {
            if (fl.b >= 0 && fl.b < Game::H) incoming_[fl.b][fl.a] = false;
        } else if (fl.a < (int)piles_.size() && fl.b < (int)piles_[fl.a].occ.size()) {
            Occ& o = piles_[fl.a].occ[fl.b];
            o = Occ{};
            o.state = FULL;
            o.tint = fl.type;
            o.tintAge = 0.f;
            o.hopT = 0.f;
            o.hopDelay = 0.f;
            o.hopA = 0.14f; // settling with a tiny bounce
            o.rotShown = piles_[fl.a].slots[fl.b].rot;
        }
        fl.a = -1;
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& x) { return x.a < 0; }), flights_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Geometry.
    shadows_.clear();
    flat_.clear();
    cubes_.clear();
    buildScene(game);
    flushCubes();

    const float bridge = smoothstepf(0.f, 1.f, saturate(bridge_.amount));

    // ---- Shadows (coverage).
    shadowT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    if (!shadows_.empty()) {
        glEnable(GL_BLEND);
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(progShadow_);
        set1f(progShadow_, "uPad", 0.f);
        set1f(progShadow_, "uBlurPad", 1.f);
        drawInst(progShadow_, shadows_);
        glBlendEquation(GL_FUNC_ADD);
        glDisable(GL_BLEND);
    }

    // ---- The table.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progTable_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowT_.tex);
    set1i(progTable_, "uShadow", 0);
    ugl::set4f(progTable_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(progTable_, "uRes", (float)width_, (float)height_);
    const float evening = slow_;
    set3f(progTable_, "uTable", pal_.table * (1.f - 0.18f * evening));
    set3f(progTable_, "uTray", pal_.tray * (1.f - 0.18f * evening));
    set3f(progTable_, "uShadowTint", pal_.shadowTint);
    set1f(progTable_, "uShadowAmt", pal_.shadowAmt * (1.f + 0.2f * evening));
    set1f(progTable_, "uMottle", pal_.mottle);
    set1f(progTable_, "uGrain", pal_.grain);
    set1f(progTable_, "uLamp", pal_.lamp * (1.f + 1.5f * evening));
    set1f(progTable_, "uTrayDepth", pal_.trayDepth);
    set1f(progTable_, "uDark", pal_.dark);
    set2f(progTable_, "uLight", LIGHT.x, LIGHT.y);
    set1f(progTable_, "uBridge", bridge);
    set3f(progTable_, "uBridgeCol", bridge_.color);
    ugl::drawFullscreen();

    // ---- Things on the table: the previews, the ghost, the gauge; then the cubes.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progCube_);
    set1f(progCube_, "uPad", 0.3f);
    set1f(progCube_, "uBlurPad", 0.f);
    set2f(progCube_, "uLight", LIGHT.x, LIGHT.y);
    set1f(progCube_, "uLift", pal_.lift);
    set1f(progCube_, "uLineW", std::max(1.f, 1.6f * height_ / 1080.f));
    set3f(progCube_, "uSpec", lerp(vec3(1.f), vec3(1.f, 0.75f, 0.5f), evening));
    set1f(progCube_, "uBridge", smoothstepf(0.2f, 0.9f, bridge));
    set3f(progCube_, "uBridgeInk", bridge_.ink);
    drawInst(progCube_, flat_);
    drawInst(progCube_, cubes_);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);

    // ---- Post.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progPost_, "uScene", 0);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", dimT_);
    ugl::drawFullscreen();
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_recycle() {
    static const UniverseEntry e{
        "RECYCLE", "a table seen from above: pieces assembled from heaps of cubes, cleared rows recycled into them",
        [] { return std::unique_ptr<Universe>(new RecycleUniverse()); },
        // Rhythmic songs suit the hopping heaps best; it fits most songs.
        [](const SongLook& l) { return 0.8f + 0.25f * l.dynamics + 0.1f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

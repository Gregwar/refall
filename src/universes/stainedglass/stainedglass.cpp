// STAINED GLASS: a chapel's window, the well and its pieces leaded glass, the cleared rows running as molten glass
// through the lead into the side windows.
//
// One world: the inside of a dim stone chapel, facing a great window. The well is the window's central light, set
// between two slender stone jambs over a sill: its empty glass is a dark, clear glazing of diamond quarries, and the
// pieces are the window's colored glass, each piece one sheet (its texture runs on across its cells: an uneven
// thickness, streaks from the sheet's draw, small seeds catching the light) cut into cells and leaded, a fine came
// between its own cells and a broad one around it, soldered where the cames meet. The painter's matting darkens the
// glass toward the piece's lead, and each piece type has its own grisaille motif painted in each cell (a quatrefoil,
// a lozenge, a roundel, a leaf, a cross, scales, rays). The falling piece is exactly the glass it will lock as; the
// ghost is its empty came, a faint line.
//
// On both sides, past the HUD's stone piers, a two-light window under a pointed arch: two lancets parted by a
// mullion, each a column of panels between iron saddle bars (a roundel or a quatrefoil medallion on a ground of
// quarries, a border), a rose of eight petals in the head. Its panes start clear (pale painted grisaille), and the
// cleared rows glaze them with molten glass: the row goes incandescent in the well (its glass glowing in its own
// colors, the lead melting with it) for as long as the game holds the stack, then runs out of the well's side as a
// stream of liquid light (white-hot at its head, the row's glasses running along it cell by cell) down a groove cut
// in the jamb, along a string course under the HUD, up beside the window and along its sill, up the mullion and
// along the saddle bar to the next empty pane (bottom up, side to side), which it fills from the bottom with a
// glowing meniscus; the pane then cools from white-hot to its color, a crackle of light running through it and a
// few sparks as it sets (the stream leaves a trace cooling in the lead behind it). A double or triple sends one
// stream per row, quicker, branching at the window; a Tetris sends its four rows together up to a whole rose, where
// the stream branches around the ring and in along the spokes into every petal, and the rose blazes once set. A
// long song ends with both windows complete (then the oldest panes are glazed again).
//
// The light: the sun stands behind the window, its glare moving across the glass with the song (low at one side in a
// calm part, higher at mid, high at a peak; each new section moves it). Its light comes through every pane in color:
// on the stone around the windows (their mouldings, the jambs, the sills), in shafts through the dusty air (a
// quarter-resolution radial streak of the glass's light from the sun, kept faint over the well and the HUD), where
// motes of dust drift and show only in the beams, and in pools of color thrown on the flagstones of the floor. Calm is
// a dim, still morning; mid brightens it and the panes glint on the two-beat grid; a peak is high noon, glints on the
// grid and the kicks, the stack's glass catching them too. The drop is the sun striking full through the window: its
// glare, the shafts and the pools blaze, eased in over half a second and settling over the next bars (a chorus's rise
// only brightens it a little), and molten light runs out from the well through every lead channel, the windows'
// mullions, saddle bars and roses included. Slow motion is night: the sun gives
// way to a dim moonlight and the candles on the window sills are lit. Game over: the light goes out of the stack's
// glass from the top, then it falls as shards.
//
// Variants: GOTHIC (jewel tones, deep blue and ruby), GRISAILLE (pale glass and silver-stain amber), ABSTRACT (a
// modern window: vivid flashed glass, no painting), VESPERS (night: the moon behind, the candles lit inside) and
// LAPIS (blue and violet with a touch of gold).
//
// Cost: one full-screen pass for the chapel (analytic tracery), the same at a quarter resolution for the light through
// the glass, a 28-sample quarter-resolution shaft pass, quarter-resolution blurs and bloom, instanced cells, sprites
// and molten-glass capsules (a few hundred, a MAX blend; also drawn at a quarter resolution after the shafts so they
// light the stone around them without throwing beams).
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/stainedglass/stainedglass.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/stainedglass/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Marcellus (the labels and numbers, capitals cut in stone) and EB Garamond Italic (the song, the section, the
// celebrations), SIL Open Font License, see OFL-Marcellus.txt and OFL-EBGaramond.txt.
const unsigned char FONT_REG[] = {
#include "universes/stainedglass/Marcellus-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/stainedglass/EBGaramond12-Italic.ttf.inc"
};

namespace shaders = stainedglass_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float s) { return {a.x * s, a.y * s}; }
float length(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
vec2 lerp2(vec2 a, vec2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A window: its seven glasses (one per piece type), the ground glass of its side windows, the clear glass of its empty panes and of the well, its stone
// (in the dark, lit), the lead, the light behind it, the lettering and its accent, the hours its light goes through
// (calm, mid, peak, the drop, slow motion).
struct VariantDef {
    const char* name;
    unsigned glass[7];
    unsigned ground;  // the side windows' ground glass, the rows' colors in its medallions (0: the rows' own)
    unsigned clear, well, stone, stoneLit, lead, sun, ink, accent;
    float motif;      // grisaille painting
    float sunK;       // the light's strength behind the window
    bool night;       // the candles always lit
    const char* hours[5];
};
const VariantDef VARIANTS[] = {
    {"GOTHIC", {0x2f63e6, 0xe02a40, 0x22a85c, 0xf4b62a, 0x9246d0, 0x23b0d2, 0xf27a36},
     0x1d3cb0, 0x94a6b2, 0x22304a, 0x121316, 0x4c4c55, 0x0b0b0d, 0xffe6c0, 0xebe4d8, 0xf2c45a, 1.f, 1.f, false,
     {"lauds", "terce", "sext", "noonday", "compline"}},
    {"GRISAILLE", {0xe4ead8, 0xeba530, 0xf6d572, 0x9fcc9f, 0x9cc4ea, 0xec9aa4, 0xbaa2e0},
     0xb4b8a6, 0xa4aeaa, 0x2a2620, 0x16120e, 0x5e5242, 0x0c0b09, 0xfff0d4, 0xf1e8d8, 0xeba530, 1.f, 0.95f, false,
     {"prime", "terce", "sext", "noonday", "compline"}},
    {"ABSTRACT", {0xf2472c, 0x2f45ea, 0xf6de2c, 0x18b46c, 0xdc3c9e, 0xfa8c1c, 0x22c6e4},
     0, 0x96a2ac, 0x18202a, 0x111213, 0x46484b, 0x050506, 0xfff4e6, 0xe9e9e6, 0xf6de2c, 0.f, 1.05f, false,
     {"morning", "late morning", "noon", "full sun", "night"}},
    {"VESPERS", {0x4272f2, 0xe63a52, 0x30ba70, 0xf4ae3e, 0xa262e6, 0x30bcb4, 0xf084ac},
     0x2a3fae, 0x6a7488, 0x162036, 0x0d0c0f, 0x3e352e, 0x080709, 0xa8bcff, 0xeee2d0, 0xffb45a, 1.f, 0.55f, true,
     {"vespers", "compline", "vigil", "full moon", "vigil"}},
    {"LAPIS", {0x2c4cd6, 0x42a4ee, 0x52d2c2, 0xdcebf6, 0x7e56de, 0xc456c6, 0xf2be3c},
     0x1c2470, 0x9aaabe, 0x1a2640, 0x101318, 0x3e4655, 0x09090c, 0xeef2ff, 0xe6eaf2, 0x96c6ff, 1.f, 1.f, false,
     {"lauds", "terce", "sext", "noonday", "compline"}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

vec3 glassColor(int variant, int type) { return lin(VARIANTS[variant].glass[std::clamp(type, 0, 6)]); }
// A sheet's own texture, from its id: the falling piece takes the id it will lock with.
float seedOf(int id) { return std::fmod((float)id * 0.618034f, 1.f) * 97.f + 3.f; }

// Shards fall this fast (board units / s^2).
constexpr float G_SHARD = 30.f;
// A cleared row melts this long (seconds) before it runs out of the well (the game holds the stack as long).
constexpr float MELT_T = 0.3f;
// The drop's strike lasts this many beats, then the peak's own light comes back.
constexpr double DROP_BEATS = 32.0;

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

// The 8-neighbour mask of a cell (bits: left right down up, down-left down-right up-left up-right).
template <class F> int maskOf(int x, int y, F same) {
    const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    int m = 0;
    for (int i = 0; i < 8; i++)
        if (same(x + dx[i], y + dy[i])) m |= 1 << i;
    return m;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool StainedGlassUniverse::init(int width, int height) {
    using shaders::withCommon;
    progWall_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::WALL_FS).c_str(), "stained glass chapel");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS).c_str(), "stained glass cells");
    progShard_ = ugl::program(shaders::SHARD_VS, shaders::SHARD_FS, "stained glass shards");
    progSprite_ = ugl::program(shaders::SPRITE_VS, shaders::SPRITE_FS, "stained glass sprites");
    progShaft_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::SHAFT_FS).c_str(), "stained glass shafts");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "stained glass down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "stained glass blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::FINAL_FS).c_str(), "stained glass final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "stained glass shape");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "stained glass text");
    progFlow_ = ugl::program(shaders::FLOW_VS, withCommon(shaders::FLOW_FS).c_str(), "stained glass molten glass");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    instAttribs(cellVao_, cellVbo_, quadVbo_, 4, sizeof(CellInst));
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    instAttribs(spriteVao_, spriteVbo_, quadVbo_, 3, sizeof(SpriteInst));
    glGenVertexArrays(1, &shardVao_);
    glGenBuffers(1, &shardVbo_);
    instAttribs(shardVao_, shardVbo_, quadVbo_, 3, sizeof(ShardInst));
    glGenVertexArrays(1, &flowVao_);
    glGenBuffers(1, &flowVbo_);
    instAttribs(flowVao_, flowVbo_, quadVbo_, 4, sizeof(FlowInst));
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

    buildFont(fontReg_, FONT_REG);
    buildFont(fontIt_, FONT_IT);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void StainedGlassUniverse::buildFont(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] stained glass font init failed\n");
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

void StainedGlassUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    // the floor: a little under the well's sill, down to the screen's bottom
    floorY_ = std::min(-10.5f, viewC_.y - hh + 1.4f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    for (ugl::Target* t : {&emitT_, &glowA_, &glowB_, &shaftT_, &bloomA_, &bloomB_}) t->create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    placeWindows();
    placeCandles();
}

StainedGlassUniverse::~StainedGlassUniverse() {
    for (ugl::Target* t : {&sceneT_, &emitT_, &glowA_, &glowB_, &shaftT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progWall_, progCell_, progShard_, progSprite_, progShaft_, progDown_, progBlur_, progFinal_, progShape_, progText_, progFlow_})
        glDeleteProgram(p);
    for (GLuint v : {cellVao_, spriteVao_, shardVao_, shapeVao_, textVao_, flowVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, spriteVbo_, shardVbo_, shapeVbo_, textVbo_, flowVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&fontReg_, &fontIt_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// The side windows
// =====================================================================================================

// A two-light window in each margin (one lancet on narrow screens), under a pointed arch that stays clear of the
// title, its base above the corner's lettering; the rose as big as the head allows.
void StainedGlassUniverse::placeWindows() {
    const std::vector<Pane> old = panes_;
    panes_.clear();
    lancetOrder_.clear();
    channels_.clear();
    const float room = viewH_.x - 11.7f;
    winOn_ = room > 2.4f;
    if (!winOn_) {
        slotsPerWin_ = 0;
        roseOn_ = false;
        flows_.clear();
        return;
    }
    winCx_ = 11.7f + room * 0.5f;
    winA0_ = std::min(room * 0.36f, 3.6f);
    lancets_ = winA0_ > 2.2f ? 2 : 1;
    winM_ = lancets_ == 2 ? 0.13f : 0.f;
    winR0_ = winA0_ * 1.9f;
    const float h0 = std::sqrt(winR0_ * winR0_ - (winR0_ - winA0_) * (winR0_ - winA0_));
    const float top = viewC_.y + viewH_.y - 2.4f;
    winBase_ = viewC_.y - viewH_.y + 3.6f;
    winSpring_ = std::max(top - h0, winBase_ + 4.f);
    winA1_ = lancets_ == 2 ? (winA0_ - winM_) * 0.5f : winA0_;
    winR1_ = winA1_ * 1.8f;
    const float body = winSpring_ - winBase_;
    panelsK_ = std::clamp((int)std::lround(body / 1.7f), 3, 7);
    panelH_ = body / panelsK_;
    const float h1 = std::sqrt(winR1_ * winR1_ - (winR1_ - winA1_) * (winR1_ - winA1_));
    // the rose: the largest circle on the axis clear of the outer arch and of both lancets' heads
    roseOn_ = lancets_ == 2;
    if (roseOn_) {
        std::vector<vec2> lancetPts;
        for (int li = 0; li < 2; li++) {
            const float xl = (li == 0 ? -1.f : 1.f) * (winM_ + winA1_);
            for (int i = 0; i <= 24; i++) {
                const float y = h1 * i / 24.f;
                const float dx = std::sqrt(std::max(0.f, winR1_ * winR1_ - y * y)) - (winR1_ - winA1_);
                lancetPts.push_back(vec2{xl + dx, y});
                lancetPts.push_back(vec2{xl - dx, y});
            }
        }
        float best = 0.f, bestY = h1;
        for (int i = 0; i <= 60; i++) {
            const float yr = h1 * 0.4f + (h0 - h1 * 0.4f) * i / 60.f;
            float r = winR0_ - std::sqrt((winR0_ - winA0_) * (winR0_ - winA0_) + yr * yr);
            for (const vec2& p : lancetPts) r = std::min(r, std::sqrt(p.x * p.x + (p.y - yr) * (p.y - yr)));
            if (r > best) { best = r; bestY = yr; }
        }
        roseY_ = bestY;
        roseR_ = best - 0.16f;
        roseCore_ = roseR_ * 0.3f;
        roseOn_ = roseR_ > 0.8f;
    }
    slotsPerWin_ = lancets_ * (panelsK_ + 1) + (roseOn_ ? 9 : 0);
    panes_.resize(2 * slotsPerWin_);
    for (int side = 0; side < 2; side++) {
        const float sx = (side == 0 ? -1.f : 1.f) * winCx_;
        for (int li = 0; li < lancets_; li++) {
            const float xl = lancets_ == 1 ? 0.f : (li == 0 ? -1.f : 1.f) * (winM_ + winA1_);
            for (int k = 0; k <= panelsK_; k++) {
                Pane& p = panes_[side * slotsPerWin_ + li * (panelsK_ + 1) + k];
                p.pos = vec2{sx + xl, k < panelsK_ ? winBase_ + (k + 0.5f) * panelH_ : winSpring_ + h1 * 0.4f};
                p.yLo = k < panelsK_ ? winBase_ + k * panelH_ : winSpring_;
                p.yHi = k < panelsK_ ? p.yLo + panelH_ : winSpring_ + h1;
                p.entry = vec2{sx + xl, p.yLo};
                p.half = k < panelsK_ ? winA1_ * 0.85f : winA1_ * 0.6f;
            }
        }
        if (roseOn_) {
            const int r0 = side * slotsPerWin_ + lancets_ * (panelsK_ + 1);
            const vec2 rc{sx, winSpring_ + roseY_};
            panes_[r0].pos = rc;
            panes_[r0].rose = true;
            panes_[r0].yLo = rc.y - roseCore_;
            panes_[r0].yHi = rc.y + roseCore_;
            panes_[r0].entry = vec2{rc.x, rc.y - roseCore_};
            panes_[r0].half = roseCore_ * 0.7f;
            const float rm = (roseCore_ + roseR_) * 0.5f;
            for (int j = 0; j < 8; j++) {
                Pane& p = panes_[r0 + 1 + j];
                const float th = j * PI / 4.f;
                p.pos = vec2{rc.x + std::cos(th) * rm, rc.y + std::sin(th) * rm};
                p.rose = true;
                float lo = 1e9f, hi = -1e9f;
                for (int i = 0; i <= 6; i++)
                    for (float rr : {roseCore_ + 0.1f, roseR_ - 0.1f}) {
                        const float a = th + (i / 6.f - 0.5f) * PI / 4.f * 0.8f;
                        lo = std::min(lo, rc.y + std::sin(a) * rr);
                        hi = std::max(hi, rc.y + std::sin(a) * rr);
                    }
                p.yLo = lo;
                p.yHi = hi;
                p.entry = vec2{rc.x + std::cos(th) * (roseR_ - 0.11f), rc.y + std::sin(th) * (roseR_ - 0.11f)};
                p.half = (roseR_ - roseCore_) * 0.3f;
            }
        }
    }
    // the lancets fill bottom up, from side to side
    for (int k = 0; k <= panelsK_; k++)
        for (int i = 0; i < 2 * lancets_; i++) {
            const int side = i % 2, li = lancets_ == 1 ? 0 : ((i / 2 + side) % 2);
            lancetOrder_.push_back(side * slotsPerWin_ + li * (panelsK_ + 1) + k);
        }
    // a resize keeps what was glazed when the window keeps its panes
    if (old.size() == panes_.size())
        for (size_t i = 0; i < panes_.size(); i++) {
            const Pane geo = panes_[i];
            panes_[i] = old[i];
            panes_[i].pos = geo.pos;
            panes_[i].rose = geo.rose;
            panes_[i].yLo = geo.yLo;
            panes_[i].yHi = geo.yHi;
            panes_[i].entry = geo.entry;
            panes_[i].half = geo.half;
        }
    else flows_.clear();
    placeChannels();
}

// The lead channels: a groove cut down each of the well's jambs, along a string course under the HUD, up beside
// the window's moulding and along its sill (where the molten glass runs); the window's own leads (its mullion, the
// saddle bars, the rose's ring and spokes) only show when molten light runs through them at the drop. Each knows
// how far it is from the well (the drop's light runs out from it).
void StainedGlassUniverse::placeChannels() {
    channels_.clear();
    if (!winOn_) return;
    riserX_ = std::max(5.9f, (11.2f + (winCx_ - winA0_ - 0.75f)) * 0.5f);
    gutterY_ = -9.75f;
    const float sillY = winBase_ - 0.2f;
    for (int side = 0; side < 2; side++) {
        const float s = side == 0 ? -1.f : 1.f;
        float d = 0.f;
        auto add = [&](vec2 a, vec2 b, bool cut, float d0) {
            const float l = length(b - a);
            channels_.push_back({a, b, d0, d0 + l, cut});
            return d0 + l;
        };
        add(vec2{s * 5.38f, 0.f}, vec2{s * 5.38f, 10.3f}, true, 0.f);
        d = add(vec2{s * 5.38f, 0.f}, vec2{s * 5.38f, gutterY_}, true, 0.f);
        d = add(vec2{s * 5.38f, gutterY_}, vec2{s * riserX_, gutterY_}, true, d);
        d = add(vec2{s * riserX_, gutterY_}, vec2{s * riserX_, sillY}, true, d);
        const float xr = winCx_ - (lancets_ == 2 ? 0.f : winA0_ - 0.07f);
        d = add(vec2{s * riserX_, sillY}, vec2{s * xr, sillY}, true, d);
        d = add(vec2{s * xr, sillY}, vec2{s * xr, winBase_}, false, d);
        // the window's leads
        const float top = roseOn_ ? winSpring_ + roseY_ - roseR_ : winSpring_;
        const float dm = add(vec2{s * xr, winBase_}, vec2{s * xr, top}, false, d);
        for (int k = 1; k <= panelsK_; k++) {
            const float y = winBase_ + k * panelH_;
            const float dk = d + (y - winBase_);
            if (lancets_ == 2) {
                add(vec2{s * winCx_, y}, vec2{s * winCx_ - (winM_ + 2.f * winA1_ - 0.05f), y}, false, dk);
                add(vec2{s * winCx_, y}, vec2{s * winCx_ + (winM_ + 2.f * winA1_ - 0.05f), y}, false, dk);
            } else add(vec2{s * xr, y}, vec2{s * xr + (s > 0 ? 1.f : -1.f) * (2.f * winA0_ - 0.14f), y}, false, dk);
        }
        if (roseOn_) {
            const vec2 rc{s * winCx_, winSpring_ + roseY_};
            const int N = 24;
            for (int i = 0; i < N; i++) {
                // the ring, both ways from its bottom
                const float a0 = -PI * 0.5f + PI * i / N, a1 = -PI * 0.5f + PI * (i + 1) / N;
                for (float sg : {-1.f, 1.f}) {
                    const vec2 p0 = rc + vec2{std::cos(-PI * 0.5f + sg * (a0 + PI * 0.5f)), std::sin(-PI * 0.5f + sg * (a0 + PI * 0.5f))} * roseR_;
                    const vec2 p1 = rc + vec2{std::cos(-PI * 0.5f + sg * (a1 + PI * 0.5f)), std::sin(-PI * 0.5f + sg * (a1 + PI * 0.5f))} * roseR_;
                    add(p0, p1, false, dm + roseR_ * (a0 + PI * 0.5f));
                }
            }
            for (int j = 0; j < 8; j++) {
                const float a = PI / 8.f + j * PI / 4.f;
                float da = std::fabs(std::remainder(a + PI * 0.5f, TAU));
                add(rc + vec2{std::cos(a), std::sin(a)} * roseR_, rc + vec2{std::cos(a), std::sin(a)} * roseCore_, false, dm + roseR_ * da);
            }
        }
    }
}

// The path of a cleared row's molten glass to a window (side sgn): across the row to the well's side, down the
// jamb's groove, along the string course, up beside the window and along its sill to the foot of its mullion.
std::vector<vec2> StainedGlassUniverse::trunk(int sgn, float rowY) const {
    const float s = (float)sgn, sillY = winBase_ - 0.2f;
    const float xr = winCx_ - (lancets_ == 2 ? 0.f : winA0_ - 0.07f);
    return {vec2{-s * 4.6f, rowY}, vec2{s * 5.0f, rowY}, vec2{s * 5.38f, rowY}, vec2{s * 5.38f, gutterY_},
            vec2{s * riserX_, gutterY_}, vec2{s * riserX_, sillY}, vec2{s * xr, sillY}, vec2{s * xr, winBase_}};
}

// From the mullion's foot up to the pane: up the mullion, along the saddle bar under it; for the rose, around its
// ring and in along a spoke.
void StainedGlassUniverse::paneTail(int pane, std::vector<vec2>& pts) const {
    const Pane& p = panes_[pane];
    const float s = pane / std::max(1, slotsPerWin_) == 0 ? -1.f : 1.f;
    const float xr = s * (winCx_ - (lancets_ == 2 ? 0.f : winA0_ - 0.07f));
    if (!p.rose) {
        pts.push_back(vec2{xr, p.yLo});
        pts.push_back(p.entry);
        return;
    }
    const vec2 rc{s * winCx_, winSpring_ + roseY_};
    pts.push_back(vec2{xr, rc.y - roseR_});
    const int j = pane - ((pane / slotsPerWin_) * slotsPerWin_ + lancets_ * (panelsK_ + 1)) - 1; // -1: the core
    const float target = j < 0 ? -PI * 0.5f + PI / 8.f * (s > 0 ? -1.f : 1.f) : j * PI / 4.f;
    const float delta = std::remainder(target + PI * 0.5f, TAU);
    const int n = std::max(1, (int)std::ceil(std::fabs(delta) / (PI / 18.f)));
    for (int i = 1; i <= n; i++) {
        const float a = -PI * 0.5f + delta * i / n;
        pts.push_back(rc + vec2{std::cos(a), std::sin(a)} * roseR_);
    }
    if (j < 0) pts.push_back(rc + vec2{std::cos(target), std::sin(target)} * (roseCore_ + 0.02f));
    pts.push_back(j < 0 ? p.entry : rc + vec2{std::cos(target), std::sin(target)} * ((roseCore_ + roseR_) * 0.5f));
}

// Candles on both windows' sills (lit at night, and in slow motion).
void StainedGlassUniverse::placeCandles() {
    candles_.clear();
    if (!winOn_) return;
    Rng r(0x5CA9D1Eull);
    for (int side = 0; side < 2; side++) {
        const float sx = (side == 0 ? -1.f : 1.f) * winCx_;
        const int n = winA0_ > 2.2f ? 5 : 3;
        for (int i = 0; i < n; i++) {
            const float u = (i + 0.5f) / n * 2.f - 1.f;
            candles_.push_back({vec2{sx + u * winA0_ * 0.8f + r.range(-0.12f, 0.12f), winBase_}, r.range(0.45f, 0.95f), r.range(0.f, TAU)});
        }
    }
}

// The next empty pane is promised a cleared row's glass (the oldest one once both windows are complete): its old
// glass drains away while the molten glass runs to it.
int StainedGlassUniverse::fillPane(const vec3& a, const vec3& b) {
    if (panes_.empty()) return -1;
    int pick = -1;
    for (int s : lancetOrder_)
        if (panes_[s].order < 0) { pick = s; break; }
    if (pick < 0)
        for (int i = 0; i < (int)panes_.size() && pick < 0; i++) {
            const int s = (i % 2) * slotsPerWin_ + i / 2; // the roses' petals from side to side
            if (s < (int)panes_.size() && panes_[s].rose && panes_[s].order < 0) pick = s;
        }
    if (pick < 0) {
        int oldest = 1 << 30;
        for (int i = 0; i < (int)panes_.size(); i++)
            if (panes_[i].order < oldest && !panes_[i].pending) { oldest = panes_[i].order; pick = i; }
    }
    if (pick < 0) return -1;
    Pane& p = panes_[pick];
    p.na = a;
    p.nb = b;
    p.pending = true;
    p.target = 1.f;
    p.order = fillCount_++;
    return pick;
}

// A Tetris glazes a whole rose: the emptier one.
int StainedGlassUniverse::roseSide() {
    int side = 0, best = -1;
    for (int s = 0; s < 2; s++) {
        int empty = 0;
        for (int j = 0; j < 9; j++)
            if (panes_[s * slotsPerWin_ + lancets_ * (panelsK_ + 1) + j].order < 0) empty++;
        if (empty > best) { best = empty; side = s; }
    }
    if (best == 0) side = (fillCount_ / 3) % 2;
    return side;
}

// A row's molten glass sets out (after `delay`, while the row melts) along its path to the pane.
void StainedGlassUniverse::addFlow(int sgn, float rowY, const vec3* cols, int pane, float delay, float speed, float vol) {
    Flow fl;
    fl.pts = trunk(sgn, rowY);
    if (pane >= 0) paneTail(pane, fl.pts);
    fl.cum.resize(fl.pts.size());
    fl.cum[0] = 0.f;
    for (size_t i = 1; i < fl.pts.size(); i++) fl.cum[i] = fl.cum[i - 1] + length(fl.pts[i] - fl.pts[i - 1]);
    fl.len = fl.cum.back();
    fl.sWell = fl.cum[1];
    fl.t = -delay;
    fl.speed = speed;
    fl.vol = vol;
    fl.pane = pane;
    fl.sgn = sgn;
    vec3 sum{0.f};
    for (int i = 0; i < 10; i++) {
        fl.cols[i] = cols[i];
        sum += cols[i];
    }
    fl.tint = sum / 10.f;
    flows_.push_back(fl);
}

// =====================================================================================================
// Song, level, the sun
// =====================================================================================================

void StainedGlassUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;        // VESPERS: dark minor songs
        else if (t.hasSong && t.airWeight > 0.62f && !t.minor) v = 1;  // GRISAILLE: airy, bright songs
        else if (t.hasSong && (t.bassWeight > 0.64f || t.dynamics > 0.72f)) v = 2; // ABSTRACT: heavy, punchy
        else if (t.hasSong && t.minor) v = 4;                         // LAPIS
        else v = (int)(h * 3.f) == 0 ? 4 : 0;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x57A1ull);
    sunTurn_ = (int)(seed_ % 2);
    // a new song: the windows' panes go back to clear (at once on the first frame)
    for (Pane& p : panes_) {
        p.target = 0.f;
        p.order = -1;
        if (seconds <= 0.1f) p.fill = 0.f;
    }
    fillCount_ = 0;
    aimSun(seconds);
}

void StainedGlassUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropOn_ = false;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        level_ = level;
        lv_ = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        aimSun(0.f);
        return;
    }
    const bool rising = level > level_;
    (void)impact;
    level_ = level;
    lvFrom_ = lv_;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, rising ? 1.2f : 3.f, 6.f);
    aimSun(std::max(4.f, seconds * 1.5f));
}

// Where the sun glares through the glass: low at one side in a calm part, higher at mid, high at a peak, overhead
// when it strikes (the drop); each section takes it to the other side.
void StainedGlassUniverse::aimSun(float seconds) {
    const float side = (sunTurn_ % 2) ? 1.f : -1.f;
    const float H = viewH_.y;
    vec2 to;
    if (dropOn_ && realDrop_) to = vec2{side * 2.f, H * 0.8f};
    else if (level_ <= 0) to = vec2{side * 17.f, H * 0.15f};
    else if (level_ == 1) to = vec2{side * 11.f, H * 0.5f};
    else to = vec2{side * 5.f, H * 0.85f};
    sunFrom_ = sun_;
    sunTo_ = to;
    if (seconds <= 0.1f) {
        sun_ = sunFrom_ = to;
        sunT_ = 1.f;
    } else {
        sunT_ = 0.f;
        sunDur_ = seconds;
    }
}

// The drop: the sun strikes full through the window (a chorus's rise only brightens it a little: see render).
void StainedGlassUniverse::drop(float strength) {
    dropOn_ = true;
    dropBeat_ = beat_;
    dropStart_ = time_;
    dropT_ = std::max(dropT_, 0.6f + 0.4f * strength);
    leadT_ = 0.f;
}

// A new section of the same level: the sun moves on to the other side.
void StainedGlassUniverse::refresh(float strength) {
    (void)strength;
    sunTurn_++;
    refreshAt_ = time_;
    sunMovedByRefresh_ = true;
    aimSun(5.f);
}

// A new game level is not a new section of the song: take back the move that came with it (same frame); a few
// panes glint.
void StainedGlassUniverse::levelUp() {
    if (refreshAt_ == time_ && sunMovedByRefresh_) {
        sunTurn_--;
        aimSun(5.f);
    }
    for (Pane& p : panes_)
        if (p.order >= 0 && rng_.chance(0.3f)) p.glint = std::max(p.glint, 0.5f);
}

vec2 StainedGlassUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints StainedGlassUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = lin(V.stone) * 1.6f + lin(V.stoneLit) * 0.1f;
    h.light = false;
    h.ink = lin(V.glass[0]);
    h.accent = lin(V.accent);
    h.styles = TS_LUMA | TS_INK | TS_CURTAIN;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the sheets in step with the board. Cells that appear behind our back (a new board, a stack inherited from
// another universe) are cut into sheets of at most four cells of one type.
void StainedGlassUniverse::syncIds(const Game& g) {
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
                sh_[cy][cx] = Sheet{(float)x, (float)y, seedOf(id)};
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

void StainedGlassUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the sheet is set in its came; a little dust from under it at the contact
        const int id = nextId_++;
        float sx = 0.f, lowY = -99.f;
        int n = 0;
        float minx = 99.f, maxx = -99.f;
        int minCx = 99, minCy = 99;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            minCx = std::min(minCx, c.x);
            minCy = std::min(minCy, c.y);
        }
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            sh_[c.y][c.x] = Sheet{(float)minCx, (float)minCy, seedOf(id)};
            glint_[c.y][c.x] = 0.35f;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            sx += p.x;
            minx = std::min(minx, p.x);
            maxx = std::max(maxx, p.x);
            n++;
            lowY = std::max(lowY, (float)c.y);
        }
        if (n > 0) {
            const vec3 p = layout::cellPos(0.f, lowY);
            puffs_.push_back({vec2{sx / n, p.y - 0.5f}, 0.f, 0.9f, 0.35f + 0.25f * (maxx - minx)});
        }
        break;
    }
    case GameEvent::HardDrop: {
        if (!game.hasPiece()) break;
        int cells[4][2];
        game.pieceCells(game.piece(), cells);
        int low = -1;
        float minx = 99.f, maxx = -99.f;
        for (auto& c : cells) low = std::max(low, c[1]);
        for (auto& c : cells) {
            const vec3 p = layout::cellPos((float)c[0], (float)low);
            minx = std::min(minx, p.x);
            maxx = std::max(maxx, p.x);
        }
        const vec3 p = layout::cellPos(0.f, (float)low);
        puffs_.push_back({vec2{(minx + maxx) * 0.5f, p.y - 0.5f}, 0.f, 1.1f, 0.7f + 0.3f * (maxx - minx)});
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        auto inClear = [&](int x, int y) {
            if (x < 0 || x >= Game::W || y < 0 || y >= Game::H || !rows[y]) return false;
            return true;
        };
        // the rows melt, then run out of the well as molten glass to the windows: each row to its pane (bottom up,
        // from side to side), a Tetris's four rows together to a whole rose, branching around its ring
        std::vector<vec3> rowCols;
        std::vector<int> rowYs;
        std::vector<std::array<vec3, 10>> rowCells;
        for (int y = 0; y < Game::H; y++) {
            if (!rows[y]) continue;
            int count[7] = {};
            std::array<vec3, 10> cells;
            for (int x = 0; x < 10; x++) cells[x] = glassColor(variant_, 0);
            for (auto& c : ev.cells)
                if (c.y == y && c.type >= 0 && c.type < 7) {
                    count[c.type]++;
                    if (c.x >= 0 && c.x < 10) cells[c.x] = glassColor(variant_, c.type);
                }
            int a = 0, b = -1;
            for (int k = 1; k < 7; k++)
                if (count[k] > count[a]) a = k;
            for (int k = 0; k < 7; k++)
                if (k != a && count[k] > 0 && (b < 0 || count[k] > count[b])) b = k;
            if (b < 0) b = a;
            const vec3 ca = glassColor(variant_, a), cb = glassColor(variant_, b);
            rowCols.push_back(ca);
            rowYs.push_back(y);
            rowCells.push_back(cells);
            // the variant's ground, the row's main glass in the medallion (its second if the main one has the
            // ground's hue), or the row's two glasses
            const unsigned gr = VARIANTS[variant_].ground;
            const float rowY = layout::cellPos(0.f, (float)y).y;
            const int n = (int)rowYs.size() - 1;
            const float speed = 9.5f + 2.2f * ev.count, vol = 3.6f + 0.5f * ev.count;
            if (ev.count < 4 || !roseOn_) {
                int pane;
                if (ev.count >= 4) pane = fillPane(ca, ca);
                else if (!gr) pane = fillPane(ca, cb);
                else {
                    const vec3 g = lin(gr);
                    const bool close = dot(normalize(ca), normalize(g)) > 0.97f;
                    pane = fillPane(g, close && b != a ? cb : ca);
                }
                if (pane >= 0) addFlow(pane / slotsPerWin_ == 0 ? -1 : 1, rowY, cells.data(), pane, MELT_T + 0.09f * n, speed, vol);
            }
            // a few drops spit from the melting row
            spit(vec2{0.f, rowY}, vec2{4.5f, 0.3f}, 3 + ev.count, lerp(ca, vec3(1.f, 0.6f, 0.25f), 0.6f), 0.5f);
        }
        if (ev.count >= 4 && roseOn_ && !rowYs.empty()) {
            const int side = roseSide();
            const int r0 = side * slotsPerWin_ + lancets_ * (panelsK_ + 1);
            const unsigned gr = VARIANTS[variant_].ground;
            for (int j = 0; j < 9; j++) {
                Pane& p = panes_[r0 + j];
                const int ri = j % (int)rowYs.size();
                p.na = rowCols[(j + 3) % rowCols.size()];
                p.nb = gr ? lin(gr) : rowCols[(j + 4) % rowCols.size()];
                if (j == 0) p.na = p.nb = rowCols[0];
                p.pending = true;
                p.target = 1.f;
                p.order = fillCount_++;
                addFlow(side == 0 ? -1 : 1, layout::cellPos(0.f, (float)rowYs[ri]).y, rowCells[ri].data(), r0 + j,
                        MELT_T + 0.025f * j, 18.f, 4.6f);
            }
            roseLit_ = side;
        }
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const int id = ids_[c.y][c.x];
            const int m = maskOf(c.x, c.y, [&](int nx, int ny) { return inClear(nx, ny) && id && ids_[ny][nx] == id; });
            const Sheet& s = sh_[c.y][c.x];
            cracks_.push_back({c.x, c.y, c.type, m, id ? s.px : (float)c.x, id ? s.py : (float)c.y, id ? s.seed : seedOf(nextId_), 0.f});
        }
        // a Tetris: the windows glint (the rose blazes once its glass has set)
        if (ev.count >= 4)
            for (Pane& p : panes_)
                if (p.order >= 0 && !p.pending) p.glint = std::max(p.glint, 0.45f);
        // compact the sheets like the game did
        {
            int nb[Game::H][Game::W] = {};
            float ng[Game::H][Game::W] = {};
            Sheet ns[Game::H][Game::W];
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(ng[dst], glint_[y], sizeof(glint_[y]));
                for (int x = 0; x < Game::W; x++) ns[dst][x] = sh_[y][x];
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(glint_, ng, sizeof(glint_));
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) sh_[y][x] = ns[y][x];
        }
        break;
    }
    case GameEvent::TopOut: {
        // the game empties its board: the glass stays here, its light goes out from the top, then it falls
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
            const int m = maskOf(c.x, c.y, [&](int nx, int ny) {
                return nx >= 0 && nx < Game::W && ny >= Game::HIDDEN && ny < Game::H && inEv(nx, ny) && id && ids_[ny][nx] == id;
            });
            const Sheet& s = sh_[c.y][c.x];
            dead_.push_back({c.x, c.y, c.type, m, id ? s.px : (float)c.x, id ? s.py : (float)c.y, id ? s.seed : seedOf(nextId_), false});
        }
        break;
    }
    default: break;
    }
}

// A cell of glass breaks into n shards that fall to the floor (`lift` throws them a little higher).
void StainedGlassUniverse::shatter(float x, float y, const vec3& col, int n, float lift) {
    for (int k = 0; k < n; k++) {
        Shard s;
        const vec2 o{rng_.range(-0.34f, 0.34f), rng_.range(-0.34f, 0.34f)};
        s.p = vec2{x + o.x, y + o.y};
        s.v = vec2{o.x * 3.f + rng_.range(-1.2f, 1.2f), rng_.range(0.2f, 2.6f) + o.y * 2.f + lift};
        s.rot = rng_.range(0.f, TAU);
        s.spin = rng_.range(-10.f, 10.f);
        s.size = rng_.range(0.12f, 0.24f);
        s.t = 0.f;
        s.life = 3.f;
        s.floorY = floorY_ - rng_.range(0.15f, std::max(0.2f, floorY_ - (viewC_.y - viewH_.y) - 0.15f));
        s.col = col;
        s.seed = rng_.uniform();
        s.landed = false;
        shards_.push_back(s);
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void StainedGlassUniverse::buildBoard(const Game& g) {
    auto cell = [&](float x, float y, float half, int mask, int type, const vec3& col, float alpha, float glint, int kind,
                    float dark, float px, float py, float seed, float crack) {
        cells_.push_back({{x, y, half, (float)mask}, {col.x, col.y, col.z, alpha}, {(float)type, glint, (float)kind, dark}, {px, py, seed, crack}});
    };
    // The ghost: the empty came where the piece will be set.
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
            const vec3 col = glassColor(variant_, p.type);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inGhost), p.type, col, 0.45f, 0.f, 1, 0.f, 0.f, 0.f, 0.f, 0.f);
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
            const Sheet& s = sh_[y][x];
            cell(p.x, p.y, 0.5f, maskOf(x, y, same), c.type, glassColor(variant_, c.type), 1.f, glint_[y][x], 0, 0.f, s.px, -s.py, s.seed, 0.f);
        }
    // A cleared row, cracking.
    for (const Crack& k : cracks_) {
        const vec3 p = layout::cellPos((float)k.x, (float)k.y);
        cell(p.x, p.y, 0.5f, k.mask, k.type, glassColor(variant_, k.type), 1.f, 0.f, 2, 0.f, k.px, -k.py, k.seed, saturate(k.t / MELT_T));
    }
    // Game over: the light goes out of the glass row by row from the top, then each row falls as shards.
    for (Dead& d : dead_) {
        if (d.gone) continue;
        const float rowT = overT_ - 0.1f * (d.y - Game::HIDDEN);
        const float dark = smoothstepf(0.05f, 0.6f, rowT);
        const vec3 col = glassColor(variant_, d.type);
        const vec3 p = layout::cellPos((float)d.x, (float)d.y);
        if (rowT > 1.3f) {
            shatter(p.x, p.y, lerp(col, vec3(luminance(col)) * 0.4f, 0.8f), 3, 0.5f);
            d.gone = true;
            continue;
        }
        cell(p.x, p.y, 0.5f, d.mask, d.type, col, 1.f, 0.f, 0, dark, d.px, -d.py, d.seed, 0.f);
    }
    // The falling piece: exactly the glass it will lock as (it and the previews light nothing in the chapel).
    previewStart_ = (int)cells_.size();
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        auto inPiece = [&](int x, int y) {
            for (auto& c : cells)
                if (c[0] == x && c[1] == y) return true;
            return false;
        };
        int minCx = 99, minCy = 99;
        for (auto& c : cells) {
            minCx = std::min(minCx, c[0]);
            minCy = std::min(minCy, c[1]);
        }
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        const vec3 col = glassColor(variant_, p.type);
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, maskOf(c[0], c[1], inPiece), p.type, col, 1.f, 0.f, 0, 0.f, (float)minCx, -(float)minCy, seedOf(nextId_), 0.f);
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
        const vec3 col = glassColor(variant_, type);
        for (auto& c : cells) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, maskOf(c[0], c[1], inPiece), type, col, alpha, 0.f, 0, 0.f, (float)c[0], -(float)c[1], seed, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f, seedOf(nextId_ + 7));
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, seedOf(nextId_ + 1 + i));
    }
}

// Drops of molten glass spat out around p (within spread), thrown up a little (`up`), falling.
void StainedGlassUniverse::spit(vec2 p, vec2 spread, int n, const vec3& col, float up) {
    for (int k = 0; k < n && sparks_.size() < 160; k++) {
        Spark s;
        s.p = vec2{p.x + rng_.range(-spread.x, spread.x), p.y + rng_.range(-spread.y, spread.y)};
        s.v = vec2{rng_.range(-1.6f, 1.6f), rng_.range(0.6f, 2.8f) + up};
        s.t = 0.f;
        s.life = rng_.range(0.45f, 0.9f);
        s.size = rng_.range(0.05f, 0.09f);
        s.col = col;
        sparks_.push_back(s);
    }
}

// The streams as capsules: each one from its tail to its head (thick across the well where the row was, as thin
// as a came in the channels, a rounder head), hottest at its head, the row's glass running along it cell by cell;
// behind it a trace of light cooling in the lead. Then the channels (dark grooves; at a real drop, molten light).
void StainedGlassUniverse::buildFlows() {
    flowInst_.clear();
    chanInst_.clear();
    auto push = [&](std::vector<FlowInst>& out, vec2 a, vec2 b, float r0, float r1, float s0, float s1, const vec3& c,
                    float h0, float h1, int kind, float alpha, float halo) {
        out.push_back({{a.x, a.y, b.x, b.y}, {r0, r1, s0, s1}, {c.x, c.y, c.z, h0}, {h1, (float)kind, alpha, halo}});
    };
    for (const Flow& fl : flows_) {
        if (fl.t < 0.f) continue;
        const float headU = fl.sWell + fl.speed * (fl.t - 0.22f * (1.f - std::exp(-fl.t / 0.22f)));
        const float lenNow = lerpf(fl.sWell, fl.vol, smoothstepf(0.f, 0.5f, fl.t));
        const float tailU = headU - lenNow;
        const float head = std::min(headU, fl.len), tail = std::min(std::max(tailU, 0.f), fl.len);
        size_t seg = 0;
        auto at = [&](float s) {
            s = std::clamp(s, 0.f, fl.len);
            while (seg + 2 < fl.pts.size() && fl.cum[seg + 1] < s) seg++;
            while (seg > 0 && fl.cum[seg] > s) seg--;
            const float l = std::max(1e-5f, fl.cum[seg + 1] - fl.cum[seg]);
            return lerp2(fl.pts[seg], fl.pts[seg + 1], saturate((s - fl.cum[seg]) / l));
        };
        const float rIn = 0.36f, rCh = 0.13f;
        auto radius = [&](float s) {
            float r = lerpf(rIn, rCh, smoothstepf(fl.sWell - 1.2f, fl.sWell + 0.3f, s));
            r *= 0.3f + 0.7f * smoothstepf(tail, tail + 0.9f, s); // the tail thins out
            if (headU < fl.len) r *= 1.f + 0.3f * std::exp(-(head - s) * (head - s) / 0.12f); // a round head
            return r;
        };
        auto colorAt = [&](float s) {
            const float u = saturate((s - tail) / std::max(0.01f, head - tail)) * 9.999f;
            const int i = (int)u;
            const float f = u - i;
            const int i0 = fl.sgn > 0 ? i : 9 - i, i1 = fl.sgn > 0 ? std::min(9, i + 1) : std::max(0, 8 - i);
            return lerp(fl.cols[i0], fl.cols[i1], smoothstepf(0.6f, 1.f, f));
        };
        // the trace cooling behind it (not in the well)
        {
            const float from = std::max(fl.sWell + 0.3f, tail - fl.speed * 1.4f);
            if (tail > from + 0.05f) {
                const int n = std::min(80, (int)std::ceil((tail - from) / 0.3f));
                auto heat = [&](float s) { return 0.62f * std::exp(-((tailU - s) / fl.speed) / 0.3f); };
                const vec3 ember = lerp(fl.tint, vec3(1.f, 0.4f, 0.15f), 0.6f);
                for (int i = 0; i < n; i++) {
                    const float s0 = from + (tail - from) * i / n, s1 = from + (tail - from) * (i + 1) / n;
                    push(flowInst_, at(s0), at(s1), rCh * 0.65f, rCh * 0.65f, s0, s1, ember, heat(s0), heat(s1), 0, 1.f, 1.4f);
                }
            }
        }
        if (head <= tail + 0.01f) continue;
        const int n = std::min(160, (int)std::ceil((head - tail) / 0.2f));
        for (int i = 0; i < n; i++) {
            const float s0 = tail + (head - tail) * i / n, s1 = tail + (head - tail) * (i + 1) / n;
            // hottest at its head; in the well (the row's glass still showing its colors) only orange
            auto heat = [&](float s) {
                const float h = 0.78f + 0.2f * (s - tail) / (head - tail);
                return lerpf(0.66f, h, smoothstepf(fl.sWell - 1.5f, fl.sWell + 1.5f, s));
            };
            const float h0 = heat(s0), h1 = heat(s1);
            const vec3 c = colorAt((s0 + s1) * 0.5f);
            push(flowInst_, at(s0), at(s1), radius(s0), radius(s1), s0, s1, c, h0, h1, 0, 1.f, 1.6f);
        }
    }
    // the channels
    const float front = leadT_ * 15.f;
    const vec3 warm{1.f, 0.62f, 0.3f};
    for (const Channel& c : channels_) {
        auto heat = [&](float d) { return leadAmp_ * smoothstepf(front, front - 3.f, d); };
        const float h0 = heat(c.d0), h1 = heat(c.d1);
        if (!c.cut && h0 < 0.01f && h1 < 0.01f) continue;
        push(chanInst_, c.a, c.b, c.cut ? 0.065f : 0.055f, c.cut ? 0.065f : 0.055f, c.d0, c.d1, warm, h0, h1, 1, c.cut ? 0.85f : 0.f, 2.2f);
    }
}

void StainedGlassUniverse::drawFlows(const std::vector<FlowInst>& v, bool molten, int emit) {
    if (v.empty()) return;
    glUseProgram(progFlow_);
    wallUniforms(progFlow_);
    setMat(progFlow_, "uVP", vp_);
    set1f(progFlow_, "uTime", time_);
    set1i(progFlow_, "uEmit", emit);
    glEnable(GL_BLEND);
    if (molten) {
        glBlendEquation(GL_MAX);
        glBlendFunc(GL_ONE, GL_ONE);
    } else if (emit) glBlendFunc(GL_ONE, GL_ONE);
    else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(flowVao_);
    glBindBuffer(GL_ARRAY_BUFFER, flowVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(FlowInst), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
    glBlendEquation(GL_FUNC_ADD);
}

void StainedGlassUniverse::drawSprites() {
    if (sprites_.empty()) return;
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set4f(progSprite_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
    sprites_.clear();
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

float StainedGlassUniverse::textWidth(const Font& f, const std::string& s, float size, float track) const {
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

void StainedGlassUniverse::textQuads(const Font& f, const std::string& s, float x, float y, float size, float track,
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

void StainedGlassUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, const vec3& halo, float alpha) {
    if (q.empty() || !f.ok) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set3f(progText_, "uHalo", halo);
    set1f(progText_, "uHaloA", 0.5f);
    set1f(progText_, "uAlpha", alpha * (1.f - smoothstepf(0.4f, 0.9f, bridge_.amount)));
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

void StainedGlassUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering pale as lit stone over a dark halo; the section followed by the hour of its light; the bonus gauge
// a small rose of twelve petals, glazed in the accent as it charges.
void StainedGlassUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink = hex(V.ink), halo = hex(V.stone) * 0.5f;
    const vec3 accent = hex(V.accent);
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

    // the bonus gauge: a ring of petals (pointed ovals)
    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const int N = 12;
        for (int i = 0; i < N; i++) {
            const float am = TAU * (i + 0.5f) / N;
            const bool on = (i + 0.5f) / N <= fill;
            const vec3 col = on ? accent : ink;
            const float al = (on ? 0.95f : (g.bonusReady() ? 0.55f : 0.24f)) * vis * (1.f - 0.5f * dimT_);
            const float wA = TAU / N * (on ? 0.3f : 0.22f);
            const float ri = r * 0.62f, ro = r * 1.05f, rm = r * 0.86f;
            const float pts[4][2] = {{c.x + std::sin(am) * ri, c.y - std::cos(am) * ri},
                                     {c.x + std::sin(am - wA) * rm, c.y - std::cos(am - wA) * rm},
                                     {c.x + std::sin(am) * ro, c.y - std::cos(am) * ro},
                                     {c.x + std::sin(am + wA) * rm, c.y - std::cos(am + wA) * rm}};
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
        auto upper = [](std::string t) {
            for (char& c : t) c = (char)std::toupper((unsigned char)c);
            return t;
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(fontReg_, upper(h.text), base * 0.82f, base * 0.24f, base * 0.06f);
            drawText(fontReg_, q, ink, halo, saturate(0.3f + h.alpha * 1.5f) * 0.85f);
            continue;
        case HUD_SECTION: {
            // the section, then the hour of its light
            const int hi = slowOn_ ? 4 : (dropOn_ && realDrop_ ? 3 : std::clamp(level_, 0, 2));
            const std::string name = V.hours[hi];
            const float size = base * 1.05f, track = base * 0.04f;
            const std::string full = h.text + "  \x7f  " + name;
            const float wFull = textWidth(fontIt_, full, size, track);
            const float x0 = ugl::hudLeft(h, wFull, width_);
            textQuads(fontIt_, h.text, x0, h.y - base * 0.1f, size, track, q);
            drawText(fontIt_, q, accent, halo, saturate(0.25f + h.alpha * 1.7f));
            q.clear();
            const float wSec = textWidth(fontIt_, h.text + "  ", size, track);
            textQuads(fontIt_, std::string("\x7f  ") + name, x0 + wSec + track, h.y - base * 0.1f, size, track, q);
            drawText(fontIt_, q, ink, halo, saturate(0.25f + h.alpha * 1.7f) * 0.7f);
            continue;
        }
        case HUD_TITLE:
            place(fontIt_, h.text, base * 1.12f, 0.f, -base * 0.12f);
            drawText(fontIt_, q, ink, halo, saturate(h.alpha * 1.3f));
            continue;
        case HUD_INFO:
            place(fontReg_, h.text, base * 0.74f, base * 0.1f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.66f);
            continue;
        case HUD_LABEL:
            place(fontReg_, upper(h.text), base * 0.8f, base * 0.3f);
            drawText(fontReg_, q, ink, halo, saturate(alpha * 1.2f) * 0.7f);
            continue;
        case HUD_VALUE:
            place(fontReg_, h.text, base * 1.1f, base * 0.04f, -base * 0.08f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_NOTICE:
            place(fontIt_, h.text, base * 1.05f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha);
            continue;
        case HUD_KEY:
            place(fontReg_, h.text, base * 0.9f, 0.f);
            drawText(fontReg_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST:
            place(fontIt_, h.text, base * 1.2f, base * 0.1f, -base * 0.15f);
            drawText(fontIt_, q, ink, halo, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(fontIt_, h.text, base * 0.95f, base * 0.04f);
            drawText(fontIt_, q, accent, halo, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(fontIt_, h.text, base * 1.1f, base * 0.06f);
                drawText(fontIt_, q, ink, halo, alpha);
            } else {
                place(fontReg_, h.text, base * 0.82f, base * 0.06f);
                drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            }
            continue;
        default:
            place(fontReg_, h.text, base * 0.8f, base * 0.02f);
            drawText(fontReg_, q, h.accent ? accent : ink, halo, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void StainedGlassUniverse::wallUniforms(GLuint p) {
    const VariantDef& V = VARIANTS[variant_];
    set4f(p, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(p, "uSeed", (float)(seed_ % 1000) * 0.137f);
    set4f(p, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(p, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    set3f(p, "uLeadCol", lin(V.lead));
    set1f(p, "uMotif", V.motif);
}

void StainedGlassUniverse::drawCells(bool emit) {
    if (cells_.empty()) return;
    glUseProgram(progCell_);
    wallUniforms(progCell_);
    setMat(progCell_, "uVP", vp_);
    set1i(progCell_, "uEmit", emit ? 1 : 0);
    glBindVertexArray(cellVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    const size_t n = emit ? std::min(cells_.size(), (size_t)previewStart_) : cells_.size();
    if (!n) return;
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)n);
}

void StainedGlassUniverse::render(const Frame& f, GLuint fbo) {
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
            dead_.clear();
        } else overT_ += dt;
    }
    over_ = approach(over_, overT_ >= 0.f ? 1.f : 0.f, overT_ >= 0.f ? 0.5f : 1.5f, dt);
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
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    {
        bool sectionKnown = false, sectionDrop = false;
        for (const HudText& h : f.hud)
            if (h.role == HUD_SECTION) {
                sectionKnown = true;
                sectionDrop = h.text == "DROP";
            }
        const bool was = dropOn_ && realDrop_;
        realDrop_ = sectionKnown ? sectionDrop : (!music.hasSong || music.speed > 1.26f);
        if (dropOn_) {
            const bool over = hasSong_ ? (beat_ - dropBeat_ > DROP_BEATS) : (time_ - dropStart_ > DROP_BEATS * 0.5f);
            if (over) dropOn_ = false;
        }
        if (was != (dropOn_ && realDrop_)) aimSun(dropOn_ && realDrop_ ? 1.6f : 5.f);
    }
    // ---- The sun striking: two eased stages, so it swells in over half a second and settles slowly.
    {
        float target = 0.f;
        if (dropOn_ && dropT_ > 0.f) {
            const float since = hasSong_ ? (float)((beat_ - dropBeat_) / bps_) : time_ - dropStart_;
            target = dropT_ * (realDrop_ ? 1.f : 0.3f) * (since < 4.f ? 1.f : lerpf(1.f, 0.45f, smoothstepf(4.f, 12.f, since)));
        } else dropT_ = 0.f;
        dropA_ = approach(dropA_, target, 6.f, dt);
        dropEnv_ = approach(dropEnv_, dropA_, 6.f, dt);
        flareA_ = approach(flareA_, flare_, 5.f, dt);
        flare_ = approach(flare_, 0.f, 0.6f, dt);
    }
    if (sunT_ < 1.f) {
        sunT_ = std::min(1.f, sunT_ + dt / sunDur_);
        const float k = smoothstepf(0.f, 1.f, sunT_);
        sun_ = vec2{lerpf(sunFrom_.x, sunTo_.x, k), lerpf(sunFrom_.y, sunTo_.y, k)};
    }
    sunMovedByRefresh_ = refreshAt_ == time_;
    {
        const float target = (0.5f + 0.28f * wm + 0.25f * wp) * (1.f - 0.3f * slow_) * (1.f - 0.6f * over_);
        sunI_ = approach(sunI_, target, 1.2f, dt);
    }
    // slow motion is night: the moon's cold light, the candles lit (VESPERS, already at night: the full moon
    // comes out silver and the candles burn low)
    const float night = V.night ? 1.f : slow_;
    const float candleK = V.night ? 1.f - 0.6f * slow_ : smoothstepf(0.f, 0.7f, slow_);
    vec3 baseSun = lin(V.sun) * V.sunK;
    if (V.night) baseSun = lerp(baseSun, vec3(0.78f, 0.84f, 1.f) * 1.25f, slow_);
    else baseSun = lerp(baseSun, vec3(0.5f, 0.62f, 1.f) * 0.95f, slow_);
    baseSun = baseSun * sunI_ * (V.night ? 1.f + 0.45f * slow_ : 1.f);
    const vec3 sunCol = baseSun * (1.f + 0.9f * dropEnv_ + 0.35f * flareA_);
    const float glare0 = (0.5f + 0.45f * wm + 0.5f * wp) * (1.f - 0.5f * night * (V.night ? 0.5f : 1.f));
    const float glare = glare0 + 1.f * dropEnv_ + 0.6f * flareA_;
    // the pieces catch only a little of the strike (the stack and the previews must stay crisp)
    const vec3 cellSun = baseSun * (1.f + 0.2f * dropEnv_ + 0.1f * flareA_);
    const float cellGlare = glare0 + 0.3f * dropEnv_;
    const float sunR = 7.f + 3.f * lv_ + 5.f * dropEnv_;

    // ---- Molten glass running in the lead (real time, even in slow motion): when its head reaches the pane it
    // pours in, the level rising as the stream drains into it.
    for (Flow& fl : flows_) {
        fl.t += dt;
        if (fl.t < 0.f || fl.pane < 0 || fl.pane >= (int)panes_.size()) continue;
        const float headU = fl.sWell + fl.speed * (fl.t - 0.22f * (1.f - std::exp(-fl.t / 0.22f)));
        const float lenNow = lerpf(fl.sWell, fl.vol, smoothstepf(0.f, 0.5f, fl.t));
        Pane& p = panes_[fl.pane];
        if (!fl.arrived && headU >= fl.len) {
            fl.arrived = true;
            fl.lenArrive = lenNow;
            p.a = p.na;
            p.b = p.nb;
            p.fill = 0.f;
            p.heat = 1.f;
            p.pouring = true;
            p.pending = false;
        }
        if (fl.arrived && p.pouring) {
            p.fill = saturate((headU - fl.len) / std::max(0.5f, fl.lenArrive));
            p.heat = 1.f;
            if (p.fill >= 1.f) p.pouring = false;
        }
    }
    flows_.erase(std::remove_if(flows_.begin(), flows_.end(), [&](const Flow& fl) {
        if (fl.t < 0.f) return false;
        const float headU = fl.sWell + fl.speed * (fl.t - 0.22f * (1.f - std::exp(-fl.t / 0.22f)));
        return headU - fl.vol > fl.len + fl.speed * 1.6f;
    }), flows_.end());
    // ---- The panes: draining (glass on its way), filling, cooling (it sets with a crackle and a few sparks), glinting.
    for (int i = 0; i < (int)panes_.size(); i++) {
        Pane& p = panes_[i];
        if (p.pending && !p.pouring) p.fill = std::max(0.f, p.fill - dt / 1.1f);
        else if (!p.pouring) p.fill = p.target > p.fill ? std::min(p.target, p.fill + dt / 1.3f) : std::max(p.target, p.fill - dt / 2.5f);
        if (!p.pouring && p.heat > 0.f) {
            const float was = p.heat;
            p.heat = std::max(0.f, p.heat - dt / 2.4f);
            const vec3 hot = lerp(p.a, vec3(1.f, 0.62f, 0.28f), 0.65f);
            if (was > 0.3f && p.heat <= 0.3f) spit(vec2{p.pos.x, (p.yLo + p.yHi) * 0.5f}, vec2{p.half, (p.yHi - p.yLo) * 0.4f}, p.rose ? 3 : 6, hot, 0.8f);
            if (was > 0.1f && p.heat <= 0.1f) {
                p.glint = std::max(p.glint, p.rose ? 0.8f : 0.6f);
                sparkles_.push_back({p.pos, 0.f, 0.7f, p.rose ? 0.6f : 0.5f, lerp(p.a, vec3(1.f), 0.6f)});
                // the rose has set: it blazes
                if (p.rose && roseLit_ >= 0 && i == roseLit_ * slotsPerWin_ + lancets_ * (panelsK_ + 1)) flare_ = 1.f;
            }
        }
        p.glint = approach(p.glint, 0.f, 2.2f, dt);
    }
    // the beat grid (every two beats from mid on, with the kicks at a peak): a few glazed panes glint, a piece of
    // the stack catches it at a peak
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (!f.paused && overT_ < 0.f) {
        const double b2 = std::floor(beat_ / 2.0);
        if (lastGlintBeat_ < 0.0 || b2 < lastGlintBeat_ - 1.0) lastGlintBeat_ = b2;
        if (b2 > lastGlintBeat_) {
            lastGlintBeat_ = b2;
            if (lv_ > 0.6f && slow_ < 0.5f) {
                const int n = wp > 0.5f ? 3 : 1;
                for (int k = 0; k < n && !panes_.empty(); k++) {
                    Pane& p = panes_[rng_.irange(0, (int)panes_.size() - 1)];
                    if (p.order >= 0 && !p.pending && p.heat <= 0.f) {
                        p.glint = std::max(p.glint, 0.35f + 0.2f * wp);
                        sparkles_.push_back({vec2{p.pos.x + rng_.range(-0.5f, 0.5f), p.pos.y + rng_.range(-0.5f, 0.5f)}, 0.f, 0.6f, 0.55f, lerp(p.b, vec3(1.f), 0.6f)});
                    }
                }
                if (wp > 0.5f) {
                    const int x = rng_.irange(0, Game::W - 1), y = rng_.irange(Game::HIDDEN, Game::H - 1);
                    if (const int id = ids_[y][x])
                        for (int yy = 0; yy < Game::H; yy++)
                            for (int xx = 0; xx < Game::W; xx++)
                                if (ids_[yy][xx] == id) glint_[yy][xx] = std::max(glint_[yy][xx], 0.45f);
                }
            }
        }
        if (wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f && !panes_.empty()) {
            Pane& p = panes_[rng_.irange(0, (int)panes_.size() - 1)];
            if (p.order >= 0) p.glint = std::max(p.glint, 0.3f * kick);
        }
    }
    kickPrev_ = kick;
    for (auto& row : glint_)
        for (float& gl : row) gl = approach(gl, 0.f, 3.f, dt);

    // ---- A cleared row melting (real time, even in slow motion); then its molten glass runs (flows_).
    for (auto it = cracks_.begin(); it != cracks_.end();) {
        it->t += dt;
        if (it->t >= MELT_T) it = cracks_.erase(it);
        else ++it;
    }
    // sparks: drops of molten glass spat out, falling
    for (Spark& sp : sparks_) {
        sp.t += dt;
        sp.v.y -= 9.f * dt;
        sp.v = sp.v * (1.f - 0.8f * dt);
        sp.p = sp.p + sp.v * dt;
    }
    sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.t > s.life; }), sparks_.end());
    // a real drop: molten light runs out from the well through the whole lead network, then settles
    leadT_ += dt;
    leadAmp_ = (dropOn_ && realDrop_) ? std::max(leadAmp_ * (1.f - dt), dropEnv_ * 0.58f) : approach(leadAmp_, 0.f, 0.8f, dt);
    for (Shard& s : shards_) {
        s.t += dt;
        if (!s.landed) {
            s.v.y -= G_SHARD * dt;
            s.p.x += s.v.x * dt;
            s.p.y += s.v.y * dt;
            s.rot += s.spin * dt;
            if (s.p.y < s.floorY) {
                s.landed = true;
                s.p.y = s.floorY;
                s.life = s.t + 0.6f;
                if (rng_.chance(0.25f)) sparkles_.push_back({s.p, 0.f, 0.35f, 0.3f, lerp(s.col, vec3(1.f), 0.6f)});
            }
        }
    }
    shards_.erase(std::remove_if(shards_.begin(), shards_.end(), [](const Shard& s) { return s.t > s.life; }), shards_.end());
    for (Sparkle& sp : sparkles_) sp.t += dt;
    sparkles_.erase(std::remove_if(sparkles_.begin(), sparkles_.end(), [](const Sparkle& s) { return s.t > s.life; }), sparkles_.end());
    for (Puff& p : puffs_) p.t += dt;
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(), [](const Puff& p) { return p.t > p.life; }), puffs_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    // ---- Dust in the air: slow motes drifting (only the beams show them).
    if (motes_.empty()) {
        Rng r(seed_ ^ 0xD057ull);
        for (int i = 0; i < 110; i++) {
            Mote m;
            m.p = vec2{r.range(viewC_.x - viewH_.x, viewC_.x + viewH_.x), r.range(viewC_.y - viewH_.y, viewC_.y + viewH_.y)};
            m.v = vec2{r.range(-0.08f, 0.08f), r.range(-0.06f, 0.03f)};
            m.size = r.range(0.035f, 0.08f);
            m.phase = r.range(0.f, TAU);
            motes_.push_back(m);
        }
    }
    for (Mote& m : motes_) {
        m.phase += wdt * 0.3f;
        m.p.x += (m.v.x + 0.05f * std::sin(m.phase)) * wdt;
        m.p.y += (m.v.y + 0.04f * std::cos(m.phase * 1.3f)) * wdt;
        if (m.p.x < viewC_.x - viewH_.x) m.p.x += 2.f * viewH_.x;
        if (m.p.x > viewC_.x + viewH_.x) m.p.x -= 2.f * viewH_.x;
        if (m.p.y < viewC_.y - viewH_.y) m.p.y += 2.f * viewH_.y;
        if (m.p.y > viewC_.y + viewH_.y) m.p.y -= 2.f * viewH_.y;
    }

    cells_.clear();
    buildBoard(game);
    buildFlows();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float worldK = 1.f - bridgeK;

    // the uniforms of the light and of the chapel
    float paneA[50 * 4] = {}, paneB[50 * 4] = {}, paneC[50 * 4] = {};
    for (int i = 0; i < (int)panes_.size() && i < 50; i++) {
        const Pane& p = panes_[i];
        paneA[i * 4 + 0] = p.a.x; paneA[i * 4 + 1] = p.a.y; paneA[i * 4 + 2] = p.a.z;
        paneA[i * 4 + 3] = p.fill;
        paneB[i * 4 + 0] = p.b.x; paneB[i * 4 + 1] = p.b.y; paneB[i * 4 + 2] = p.b.z;
        const bool lit = p.rose && roseLit_ >= 0 && i / std::max(1, slotsPerWin_) == roseLit_;
        paneB[i * 4 + 3] = p.glint + (lit ? 1.1f * flareA_ * p.fill : 0.f);
        paneC[i * 4 + 0] = p.heat * worldK;
        paneC[i * 4 + 1] = p.yLo;
        paneC[i * 4 + 2] = p.yHi;
        paneC[i * 4 + 3] = p.pouring ? 1.f : 0.f;
    }
    float candles[12 * 4] = {};
    const int nc = candleK > 0.01f ? std::min(12, (int)candles_.size()) : 0;
    for (int i = 0; i < nc; i++) {
        const Candle& c = candles_[i];
        const float fl = 0.5f + 0.5f * std::sin(worldTime_ * 9.f + c.phase) * std::sin(worldTime_ * 5.3f + c.phase * 2.f);
        candles[i * 4 + 0] = c.p.x;
        candles[i * 4 + 1] = c.p.y + c.h + 0.2f;
        candles[i * 4 + 2] = candleK * 1.3f * worldK;
        candles[i * 4 + 3] = fl;
    }
    const vec2 sunUv{(sun_.x * 1.5f - (viewC_.x - viewH_.x)) / (2.f * viewH_.x), (sun_.y + 24.f - (viewC_.y - viewH_.y)) / (2.f * viewH_.y)};
    auto setWall = [&](GLuint p, int emit) {
        wallUniforms(p);
        set1i(p, "uEmit", emit);
        set4f(p, "uSun", sun_.x, sun_.y, sunR, glare);
        set3f(p, "uSunCol", sunCol * worldK);
        set3f(p, "uStone", lin(V.stone));
        set3f(p, "uStoneLit", lin(V.stoneLit));
        set3f(p, "uClear", lin(V.clear) * vec3(0.13f, 0.14f, 0.16f));
        set3f(p, "uWellGlass", lin(V.well));
        set3f(p, "uAmb", vec3(0.16f + 0.06f * wm + 0.08f * wp) * (1.f - 0.4f * slow_) * worldK);
        set1f(p, "uGlowK", (0.5f + 0.25f * wp + 1.f * dropEnv_ + 0.4f * flareA_) * worldK);
        set1f(p, "uPool", (1.1f + 0.6f * wp + 2.2f * dropEnv_) * worldK * (1.f - 0.5f * night));
        set2f(p, "uPoolShift", -sun_.x * 0.12f, 0.5f + sun_.y * 0.12f);
        set4f(p, "uWinA", winCx_, winBase_, winSpring_, winA0_);
        set4f(p, "uWinB", winR0_, winA1_, winR1_, winM_);
        set4f(p, "uRose", roseY_, roseR_, roseCore_, roseOn_ ? 1.f : 0.f);
        set2f(p, "uPanel", panelH_, (float)panelsK_);
        set1i(p, "uNL", lancets_);
        set1i(p, "uSlots", slotsPerWin_);
        set1f(p, "uWinOn", winOn_ ? 1.f : 0.f);
        glUniform4fv(ugl::U(p, "uPaneA"), 50, paneA);
        glUniform4fv(ugl::U(p, "uPaneB"), 50, paneB);
        glUniform4fv(ugl::U(p, "uPaneC"), 50, paneC);
        set1f(p, "uTime", time_);
        set1f(p, "uFloorY", floorY_);
        glUniform4fv(ugl::U(p, "uCandle"), 12, candles);
        set1i(p, "uNC", nc);
        set3f(p, "uCandleCol", vec3(1.f, 0.62f, 0.3f));
        set1f(p, "uWellLight", 0.11f + 0.04f * wp);
    };
    auto cellUniforms = [&]() {
        glUseProgram(progCell_);
        set4f(progCell_, "uSun", sun_.x, sun_.y, sunR, cellGlare);
        set3f(progCell_, "uSunCol", cellSun * worldK + vec3(0.02f));
        set1f(progCell_, "uMinLight", 0.72f * (1.f - 0.6f * over_));
        set3f(progCell_, "uRoom", vec3(0.07f, 0.065f, 0.06f) * (0.4f + 0.6f * sunI_) + vec3(1.f, 0.62f, 0.3f) * 0.15f * candleK);
    };

    // ---- The light through the glass (quarter resolution): the windows, the stack.
    emitT_.bind();
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glUseProgram(progWall_);
    setWall(progWall_, 1);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    cellUniforms();
    drawCells(true);
    glDisable(GL_BLEND);

    // ---- The shafts: the glass's light streaked from the sun, softened.
    shaftT_.bind();
    glUseProgram(progShaft_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, emitT_.tex);
    set1i(progShaft_, "uSrc", 0);
    wallUniforms(progShaft_);
    set2f(progShaft_, "uSunUv", sunUv.x, sunUv.y);
    set2f(progShaft_, "uSunW", sun_.x * 1.5f, sun_.y + 24.f);
    set1f(progShaft_, "uTime", worldTime_);
    set1f(progShaft_, "uSpan", 0.3f + 0.08f * dropEnv_);
    ugl::drawFullscreen();
    glUseProgram(progBlur_);
    bloomB_.bind();
    glBindTexture(GL_TEXTURE_2D, shaftT_.tex);
    set2f(progBlur_, "uDir", 1.f / shaftT_.w, 0.f);
    ugl::drawFullscreen();
    shaftT_.bind();
    glBindTexture(GL_TEXTURE_2D, bloomB_.tex);
    set2f(progBlur_, "uDir", 0.f, 1.f / shaftT_.h);
    ugl::drawFullscreen();

    // ---- Molten glass (after the shafts: it lights the stone around it, it throws no beams).
    emitT_.bind();
    drawFlows(chanInst_, false, 1);
    drawFlows(flowInst_, true, 1);
    glDisable(GL_BLEND);

    // ---- Its blur: the light falling on the stone and the floor.
    glUseProgram(progBlur_);
    set1i(progBlur_, "uSrc", 0);
    glActiveTexture(GL_TEXTURE0);
    {
        const GLuint srcs[2] = {emitT_.tex, glowA_.tex};
        for (int i = 0; i < 2; i++) {
            const float st = 1.5f + 1.5f * i;
            glowB_.bind();
            glBindTexture(GL_TEXTURE_2D, srcs[i]);
            set2f(progBlur_, "uDir", st / glowA_.w, 0.f);
            ugl::drawFullscreen();
            glowA_.bind();
            glBindTexture(GL_TEXTURE_2D, glowB_.tex);
            set2f(progBlur_, "uDir", 0.f, st / glowA_.h);
            ugl::drawFullscreen();
        }
    }
    // ---- The chapel.
    sceneT_.bind();
    glUseProgram(progWall_);
    setWall(progWall_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glowA_.tex);
    set1i(progWall_, "uGlow", 0);
    ugl::drawFullscreen();

    // ---- The lead channels in the stone, the molten glass running in them (behind the stack: the rows falling
    // into place cover what is left of the row's glass in the well).
    if (worldK > 0.01f) {
        drawFlows(chanInst_, false, 0);
        drawFlows(flowInst_, true, 0);
    }

    // ---- Shards (behind the stack).
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    shardInst_.clear();
    for (const Shard& s : shards_) {
        const float a = s.landed ? 1.f - smoothstepf(s.life - 0.6f, s.life, s.t) : 1.f;
        shardInst_.push_back({{s.p.x, s.p.y, s.size * (s.landed ? 0.8f : 1.f), s.rot}, {s.col.x, s.col.y, s.col.z, a * worldK},
                              {s.rot * 1.7f + s.seed * 9.f, s.seed, 0.f, 0.f}});
    }
    if (!shardInst_.empty()) {
        glUseProgram(progShard_);
        setMat(progShard_, "uVP", vp_);
        set3f(progShard_, "uLeadCol", lin(V.lead));
        glBindVertexArray(shardVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shardVbo_);
        glBufferData(GL_ARRAY_BUFFER, shardInst_.size() * sizeof(ShardInst), shardInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shardInst_.size());
    }

    // ---- The glass pieces.
    cellUniforms();
    drawCells(false);

    // ---- Small lights: dust in the beams, the candles, glints, puffs of dust.
    {
        const vec4 hl{layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]};
        const vec4 hr{layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]};
        auto inBox = [](vec2 p, const vec4& b) { return p.x > b.x && p.x < b.z && p.y > b.y && p.y < b.w; };
        for (const Mote& m : motes_) {
            if (std::fabs(m.p.x) < 5.8f || inBox(m.p, hl) || inBox(m.p, hr)) continue;
            const float tw = 0.6f + 0.4f * std::sin(m.phase * 3.f);
            sprites_.push_back({{m.p.x, m.p.y, m.size, m.size}, {1.f, 1.f, 1.f, tw * worldK}, {2.f, 0.f, 0.f, 0.f}});
        }
        const vec3 wax = lerp(vec3(0.5f, 0.44f, 0.36f) * 0.25f, vec3(0.95f, 0.78f, 0.55f) * 0.55f, candleK);
        for (int i = 0; i < (int)candles_.size(); i++) {
            const Candle& c = candles_[i];
            if (candleK > 0.01f) {
                sprites_.push_back({{c.p.x, c.p.y + c.h * 0.5f, 0.09f, c.h * 0.5f}, {wax.x, wax.y, wax.z, candleK * worldK}, {4.f, 0.f, 0.f, 0.f}});
                const float fl = i < nc ? candles[i * 4 + 3] : 0.5f;
                const float sway = 0.06f * std::sin(worldTime_ * 2.3f + c.phase);
                sprites_.push_back({{c.p.x, c.p.y + c.h + 0.24f, 0.12f, 0.3f * (0.9f + 0.15f * fl)}, {1.f, 0.62f, 0.25f, 1.3f * candleK * worldK}, {3.f, sway, 0.f, 0.f}});
                sprites_.push_back({{c.p.x, c.p.y + c.h + 0.25f, 1.3f, 1.3f}, {1.f, 0.55f, 0.22f, 0.22f * candleK * worldK * (0.8f + 0.2f * fl)}, {0.f, 0.f, 0.f, 0.f}});
            }
        }
        const vec3 dustCol = lerp(lin(V.stoneLit) * 1.5f, sunCol * 0.5f, 0.5f);
        for (const Puff& p : puffs_) {
            const float k = p.t / p.life;
            const float rr = p.size * (0.6f + 0.9f * std::sqrt(k));
            sprites_.push_back({{p.p.x, p.p.y, rr, rr * 0.35f}, {dustCol.x, dustCol.y, dustCol.z, 0.35f * (1.f - k) * (1.f - k) * worldK}, {5.f, 0.f, 0.f, 0.f}});
        }
        // a Tetris: the glazed rose blazes, a halo of its glass around it
        if (roseLit_ >= 0 && flareA_ > 0.01f && roseOn_) {
            const int r0 = roseLit_ * slotsPerWin_ + lancets_ * (panelsK_ + 1);
            vec3 avg{0.f};
            for (int j = 0; j < 9; j++) avg += panes_[r0 + j].a * panes_[r0 + j].fill;
            avg = avg / 9.f;
            const vec2 c = panes_[r0].pos;
            sprites_.push_back({{c.x, c.y, roseR_ * 2.6f, roseR_ * 2.6f}, {avg.x, avg.y, avg.z, 0.9f * flareA_ * worldK}, {0.f, 0.f, 0.f, 0.f}});
        }
        // the molten glass pouring into its pane: a glow where it comes in; sparks of it, streaking as they fall
        for (const Flow& fl : flows_)
            if (fl.arrived && fl.pane >= 0 && fl.pane < (int)panes_.size() && panes_[fl.pane].pouring) {
                const Pane& p = panes_[fl.pane];
                const vec3 c = lerp(p.a, vec3(1.f, 0.7f, 0.35f), 0.7f);
                sprites_.push_back({{p.entry.x, p.entry.y, 0.7f, 0.7f}, {c.x, c.y, c.z, 0.9f * worldK}, {0.f, 0.f, 0.f, 0.f}});
            }
        for (const Spark& sp : sparks_) {
            const float k = 1.f - saturate(sp.t / sp.life);
            const float v = std::sqrt(sp.v.x * sp.v.x + sp.v.y * sp.v.y);
            const vec3 c = lerp(sp.col, vec3(1.f, 0.85f, 0.6f), 0.5f * k);
            sprites_.push_back({{sp.p.x, sp.p.y, sp.size * (1.f + 0.12f * v), sp.size * 0.7f}, {c.x, c.y, c.z, 2.2f * k * worldK}, {0.f, std::atan2(sp.v.y, sp.v.x), 0.f, 0.f}});
        }
        for (const Sparkle& sp : sparkles_) {
            const float k = std::sin(PI * saturate(sp.t / sp.life));
            const float rr = sp.size * (0.6f + 0.4f * k);
            sprites_.push_back({{sp.p.x, sp.p.y, rr, rr}, {sp.col.x, sp.col.y, sp.col.z, 0.8f * k * worldK}, {1.f, 0.3f, 0.f, 0.f}});
        }
        glUseProgram(progSprite_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, shaftT_.tex);
        set1i(progSprite_, "uShaft", 0);
        set1f(progSprite_, "uDustK", 2.5f * (0.4f + 0.6f * wm) * (1.f + dropEnv_));
        drawSprites();
    }

    // ---- Bloom.
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThreshold", 0.45f);
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

    // ---- The frame: the shafts in the air.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    wallUniforms(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomA_.tex);
    set1i(progFinal_, "uBloom", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, shaftT_.tex);
    set1i(progFinal_, "uShaft", 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, emitT_.tex);
    set1i(progFinal_, "uEmitT", 3);
    {
        const float L = viewC_.x - viewH_.x, R = viewC_.x + viewH_.x, T = viewC_.y + viewH_.y, B = viewC_.y - viewH_.y;
        set4f(progFinal_, "uQuietA", L - 1.f, T - 2.6f, -10.f, T + 1.f);
        set4f(progFinal_, "uQuietB", L - 1.f, B - 1.f, -11.f, B + 3.2f);
        set4f(progFinal_, "uQuietC", 11.f, B - 1.f, R + 1.f, B + 2.2f);
    }
    glActiveTexture(GL_TEXTURE0);
    set1f(progFinal_, "uBloomK", (0.2f + 0.08f * wp + 0.12f * dropEnv_ + 0.12f * flareA_) * worldK);
    set1f(progFinal_, "uShaftK", (0.35f + 0.3f * wm + 0.35f * wp + 1.2f * dropEnv_ + 0.6f * flareA_) * (1.f - 0.3f * night) * worldK);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
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

const UniverseEntry& universe_stainedglass() {
    static const UniverseEntry e{
        "STAINED GLASS", "a chapel's window: leaded glass pieces lit by the sun, cleared rows run as molten glass into the side windows",
        [] { return std::unique_ptr<Universe>(new StainedGlassUniverse()); },
        // Fits most songs; dynamic ones a little more (the light striking on the drop).
        [](const SongLook& l) { return 0.85f + 0.2f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

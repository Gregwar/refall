// BAUHAUS: a Bauhaus poster (Bayer, Schmidt, Kandinsky, Moholy-Nagy, Albers) recomposed by the song.
//
// One world: a printed sheet of cream paper (a fine grain, long soft fibers, the ink a little uneven) carrying a
// strict, balanced composition of flat geometric forms in the primaries and black: a great circle, a triangle, a
// square, a half disc on the sheet's edge, a quarter disc in a corner, a long black bar on the diagonal that ties both
// sides of the sheet together, a short bar, thin rules, small dots threaded on the bar, and lowercase type (the song's
// title turned up the light side or set across it, its tempo as big numerals with the section, tempo and key beside
// them). The composition is asymmetric but balanced: the circle weighs one side (the heavy side), the triangle and the
// square answer it on the other, diagonally; the cleared rows' sampler sits under the circle. On light sheets some inks
// overprint (a triangle over the bar darkens like ink over ink). The poster lives in the free zones beyond the HUD
// columns: the band of the well and the columns, and a crisp box behind each HUD line in the corners, veil it toward
// the paper.
//
// The pieces are flat blocks in the variant's primaries, one clean shape per piece (its cells merge, a narrow gutter
// of paper around it, so two pieces never fuse), each carrying one small geometric mark in paper or ink (a disc, a
// diamond, a triangle, a half disc, a ring, a quarter disc: the seven types read by color and by mark).
//
// The song sets the composition. On the bar grid the forms recompose: they slide, turn and scale into a new balanced
// arrangement with slow eased motion (small changes every bar, the structure every four or eight bars; a form that must
// cross the sheet shrinks away and grows again in its new place instead of sliding across). Calm is sparse (the
// circle, the bar, the title, much paper); mid adds the triangle, the square, the half disc, the short bar, a rule,
// the numerals and dots stepping along the bar every two beats; a peak is dense (a ring around the circle, a second
// rule, bigger numerals) and its big forms turn slowly, the dots and the ring swell with the kicks. The drop (a real DROP section, not a chorus) grows a great
// disc in the signature color out of the heavy side in half a second, every form inside it cut out of it in paper,
// held while the drop lasts and shrinking back after. Every cleared row closes into discs sliding out of the well
// sideways (0.35 s, clipped at its edge) and joins the poster as one small form (a square, disc, triangle, half or
// quarter disc in the row's main color) in a sampler grid that grows over the song; a Tetris turns every big form a
// quarter turn and adds a medallion of four quarter discs in the four rows' colors. Slow motion turns the sheet into a
// photogram (a dark ground, the forms pale where they held back the light). Game over slides the stack out row by row
// from the top.
//
// Cost: flat instanced shapes and SDF type into one full-resolution poster target, one composite pass.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/bauhaus/bauhaus.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/bauhaus/shaders.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Outfit (Medium: the poster's words, the values; Light: the information, the labels), a geometric sans in the
// spirit of the Bauhaus alphabets, SIL Open Font License, see OFL-Outfit.txt.
const unsigned char FONT_MED[] = {
#include "universes/bauhaus/Outfit-Medium.ttf.inc"
};
const unsigned char FONT_LIGHT[] = {
#include "universes/bauhaus/Outfit-Light.ttf.inc"
};

namespace shaders = bauhaus_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }

// How a variant composes: the Dessau poster (the forms plain), Kandinsky (the circles in concentric rings, more
// rules), Albers (the squares nested, homage to the square), Moholy-Nagy (translucent overprinted planes, many rules).
enum Style { ST_DESSAU, ST_KANDINSKY, ST_ALBERS, ST_MOHOLY };

// The sheets: the paper, the ink, four poster colors (the first is the signature: the drop's disc) and the seven
// piece colors (I O T S Z J L).
struct VariantDef {
    const char* name;
    unsigned paper, ink;
    unsigned col[4];
    unsigned piece[7];
    Style style;
    bool overprint; // inks multiply where they cross (light sheets)
};
const VariantDef VARIANTS[] = {
    // the classic: red, yellow, blue and black on cream
    {"DESSAU", 0xeee6d3, 0x1b1a19, {0xd8352a, 0xf2b71b, 0x1d4e9e, 0x9a948a},
     {0x1d4e9e, 0xe9a915, 0xd8352a, 0x1b1a19, 0x8f897f, 0x6b9fcb, 0xe46f24}, ST_DESSAU, true},
    // circles in rings, violet and green beside the primaries, a warmer paper
    {"KANDINSKY", 0xf2e7cf, 0x1d1b22, {0xcf3a2c, 0x274a9a, 0xf0c21d, 0x6a3f8c},
     {0x274a9a, 0xeab01a, 0xcf3a2c, 0x1d1b22, 0x6a3f8c, 0x2f8a5c, 0xe8762c}, ST_KANDINSKY, true},
    // nested squares in warm earths
    {"ALBERS", 0xede5d5, 0x26231f, {0xd9622b, 0xe2a52e, 0xa8322a, 0x77745a},
     {0xdc9c22, 0xd9622b, 0xa8322a, 0x26231f, 0x77745a, 0x4d6787, 0x8a8174}, ST_ALBERS, true},
    // the same primaries printed on black, cream for the ink
    {"NACHT", 0x161515, 0xece4d2, {0xe2412f, 0xf5c31f, 0x3d6fd0, 0x6e6a63},
     {0x4a7ad8, 0xf5c31f, 0xe2412f, 0xece4d2, 0x8a857c, 0x86b6e2, 0xf07c2c}, ST_DESSAU, false},
    // translucent planes, black and red on a grey paper
    {"MOHOLY", 0xe3e1db, 0x151515, {0xcc2e24, 0xeec22c, 0x55779f, 0x8f8d88},
     {0x4f6f96, 0xe5b21e, 0xcc2e24, 0x151515, 0x8a8883, 0xdf6a2a, 0x8fb2d2}, ST_MOHOLY, true},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// The gutter around a piece (board units, each side), the clear's slide (seconds), the trace's flight to the poster.
constexpr float GAP = 0.06f;
constexpr float WIPE_S = 0.35f;
constexpr float TRACE_S = 1.6f;
constexpr int MAX_TRACES = 32, SAMPLER_ROWS = 4; // the sampler: 8 columns of 4 rows
// How much of the poster is veiled behind the well and the HUD's boxes.
constexpr float VEIL = 0.93f;

float ease(float t) {
    t = saturate(t);
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
}
float easeOut(float t) {
    t = saturate(t);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
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

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// The composition's structural choices, each drawn by its own number from the era's seed: a choice never depends
// on which others were drawn, so the drop and a change of level keep the arrangement they rise from.
struct Pick {
    uint64_t seed;
    float u(int i) const { return (splitmix64(seed ^ ((uint64_t)i * 0x9E3779B97F4A7C15ull)) >> 40) * (1.f / 16777216.f); }
    bool chance(int i, float p) const { return u(i) < p; }
    float range(int i, float a, float b) const { return a + (b - a) * u(i); }
    int irange(int i, int a, int b) const { return a + std::min(b - a, (int)(u(i) * (float)(b - a + 1))); }
};

// The cell of a piece that carries its mark: the one nearest the piece's middle (the first of equals).
int markIndex(const int cells[4][2]) {
    float mx = 0, my = 0;
    for (int i = 0; i < 4; i++) { mx += cells[i][0] * 0.25f; my += cells[i][1] * 0.25f; }
    int best = 0;
    float bd = 1e9f;
    for (int i = 0; i < 4; i++) {
        const float d = (cells[i][0] - mx) * (cells[i][0] - mx) + (cells[i][1] - my) * (cells[i][1] - my);
        if (d < bd - 1e-3f) { bd = d; best = i; }
    }
    return best;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool BauhausUniverse::init(int width, int height) {
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "bauhaus shapes");
    progText_ = ugl::program(shaders::TEXT_VS, shaders::TEXT_FS, "bauhaus text");
    progComp_ = ugl::program(ugl::FULLSCREEN_VS, shaders::COMP_FS, "bauhaus composite");

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
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Shape), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    buildFont(med_, FONT_MED, 128.f, 2048);
    buildFont(light_, FONT_LIGHT, 64.f, 1024);
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void BauhausUniverse::buildFont(Font& f, const unsigned char* ttf, float pxSize, int AW) {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] bauhaus font init failed\n");
        delete info;
        return;
    }
    f.info = info;
    f.px = pxSize;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AH = AW, PAD = (int)(f.px / 8.f);
    std::vector<unsigned char> atlas((size_t)AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    auto cpOf = [](int ch) { return ch == 127 ? 0xB7 : ch; }; // the middle dot in DEL's slot
    for (int ch = 32; ch < 128; ch++) {
        const int cp = cpOf(ch);
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, cp, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) { g.w = g.h = 0; continue; }
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(size_t)(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    for (int a = 0; a < 96; a++)
        for (int b = 0; b < 96; b++)
            f.kern[a][b] = stbtt_GetCodepointKernAdvance(info, cpOf(a + 32), cpOf(b + 32)) * f.scaleTT;
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

void BauhausUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    unitPx_ = height_ / (2.f * hh);
    posterT_.create(width_, height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    if (bar_ > -1000) compose(bar_, 0.f, true);
}

BauhausUniverse::~BauhausUniverse() {
    posterT_.destroy();
    for (GLuint p : {progShape_, progText_, progComp_}) glDeleteProgram(p);
    for (GLuint v : {shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (Font* f : {&med_, &light_}) {
        if (f->tex) glDeleteTextures(1, &f->tex);
        delete (stbtt_fontinfo*)f->info;
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void BauhausUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;          // a dark song: the night print
        else if (t.hasSong && t.bassWeight > 0.62f) v = 4;                // heavy: Moholy's black and red planes
        else if (t.hasSong && t.airWeight > 0.6f) v = 1;                  // airy: Kandinsky's circles
        else if (t.hasSong && t.bpm < 96.f && !t.minor) v = 2;            // slow and warm: Albers
        else v = (int)(h * 3) % 3;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0xBA0BA05ull);
    bpm_ = t.bpm > 30.f ? t.bpm : 120.f;
    (void)seconds;
    traces_.clear(); // a new song starts a new sheet
    heavy_ = rng_.chance(0.5f) ? -1 : 1;
    bar_ = -1000; // recompose on the next frame
}

void BauhausUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)energy;
    (void)impact;
    level = std::clamp(level, 0, 2);
    if (level < 2) dropMode_ = false;
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

// The drop: on its downbeat the poster recomposes around the heavy side and, on a real drop, the great disc grows.
void BauhausUniverse::drop(float strength) {
    (void)strength;
    dropMode_ = true;
    dropPending_ = true;
}

vec2 BauhausUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints BauhausUniverse::hints() const {
    const VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hex(V.paper));
    h.ink = ugl::toLinear(hex(V.ink));
    h.accent = ugl::toLinear(hex(V.col[0]));
    h.light = ugl::luma(hex(V.paper)) > 0.4f;
    h.styles = TS_TILES | TS_CURTAIN | TS_REBUILD;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

// Keeps the pieces' ids in step with the board (a new board, or one changed behind our back, gets fresh ids).
void BauhausUniverse::syncIds(const Game& g) {
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if ((g.cell(x, y).type >= 0) != (ids_[y][x] != 0)) wrong++;
    if (!wrong) return;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) { ids_[y][x] = 0; mark_[y][x] = false; }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0 || ids_[y][x]) continue;
            const int id = nextId_++;
            int stack[8][2], sp = 0, count = 0;
            stack[sp][0] = x; stack[sp][1] = y; sp++;
            ids_[y][x] = id;
            mark_[y][x] = false;
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
                    mark_[ny][nx] = false;
                    stack[sp][0] = nx; stack[sp][1] = ny; sp++;
                }
            }
        }
}

void BauhausUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        int cells[4][2] = {};
        for (int i = 0; i < 4 && i < (int)ev.cells.size(); i++) { cells[i][0] = ev.cells[i].x; cells[i][1] = ev.cells[i].y; }
        const int mi = ev.cells.size() == 4 ? markIndex(cells) : -1;
        for (int i = 0; i < (int)ev.cells.size(); i++) {
            const auto& c = ev.cells[i];
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            mark_[c.y][c.x] = i == mi;
        }
        break;
    }
    case GameEvent::HardDrop: {
        // a short black bar at the contact, closing toward its middle
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
        // each cleared row closes into discs sliding out sideways, then joins the poster as one small form
        int k = 0;
        int rowCol[4] = {0, 0, 0, 0};
        for (int y = Game::HIDDEN; y < Game::H; y++) {
            if (!rows[y]) continue;
            Wipe w;
            w.y = layout::cellPos(0.f, (float)y).y;
            int count[7] = {};
            for (int x = 0; x < Game::W; x++) { w.type[x] = -1; w.mask[x] = 0; w.mark[x] = false; }
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) {
                    w.type[c.x] = std::clamp(c.type, 0, 6);
                    w.mark[c.x] = mark_[y][c.x];
                    count[w.type[c.x]]++;
                }
            w.t = 0.f;
            w.delay = 0.f;
            w.dir = ((y + k) & 1) ? 1.f : -1.f;
            int main = 0;
            for (int i = 1; i < 7; i++)
                if (count[i] > count[main]) main = i;
            if (k < 4) rowCol[k] = main;
            // the row's form: its kind from the main type, a quarter turn from its place
            static const int KIND[7] = {4, 0, 2, 3, 3, 1, 4};
            Trace tr{};
            tr.kind = KIND[main];
            tr.col = main;
            tr.medallion = false;
            tr.rot = (float)((y + (int)traces_.size()) & 3) * 0.5f * PI;
            tr.from.x = w.dir * 5.6f;
            tr.from.y = w.y;
            tr.from.s = 0.45f;
            tr.from.a = 1.f;
            tr.cur = tr.from;
            tr.t = 0.f;
            tr.dur = TRACE_S;
            traces_.push_back(tr);
            k++;
            wipes_.push_back(w);
        }
        if (ev.count >= 4) {
            // a medallion of four quarter discs in the rows' colors; every big form turns a quarter
            Trace m{};
            m.medallion = true;
            m.kind = 1;
            for (int i = 0; i < 4; i++) m.quarters[i] = rowCol[i];
            if (m.quarters[0] == m.quarters[1] && m.quarters[1] == m.quarters[2]) { m.quarters[1] = 3; m.quarters[3] = (m.quarters[0] + 2) % 7; }
            m.from.x = 0.f;
            m.from.y = 0.f;
            m.from.s = 0.f;
            m.from.a = 1.f;
            m.cur = m.from;
            m.t = 0.f;
            m.dur = 2.f;
            int medals = 0;
            for (const Trace& t : traces_) medals += t.medallion;
            if (medals >= 2)
                for (size_t i = 0; i < traces_.size(); i++)
                    if (traces_[i].medallion) { traces_.erase(traces_.begin() + i); break; }
            traces_.push_back(m);
            tetrisTurnTo_ += 0.5f * PI;
        }
        // the oldest forms of a full sampler leave it
        for (;;) {
            int n = 0;
            for (const Trace& t : traces_) n += !t.medallion;
            if (n <= MAX_TRACES) break;
            for (size_t i = 0; i < traces_.size(); i++)
                if (!traces_[i].medallion) { traces_.erase(traces_.begin() + i); break; }
        }
        placeTraces(TRACE_S, false);
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            bool nm[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nm[dst], mark_[y], sizeof(mark_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(mark_, nm, sizeof(mark_));
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
            for (int x = 0; x < Game::W; x++) { w.type[x] = -1; w.mark[x] = false; }
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < Game::W) {
                    w.type[c.x] = std::clamp(c.type, 0, 6);
                    w.mark[c.x] = mark_[y][c.x];
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
            w.delay = 0.5f + 0.07f * (y - Game::HIDDEN);
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

vec2 BauhausUniverse::px(float x, float y) const {
    return {(x - viewC_.x) * unitPx_ + width_ * 0.5f, height_ * 0.5f - (y - viewC_.y) * unitPx_};
}

void BauhausUniverse::shape(Kind kind, float cx, float cy, float hx, float hy, float rot, const vec3& c, float a,
                            float p1, float p2, float p3, const float* clip) {
    if (a <= 0.002f || hx <= 0.05f || hy <= 0.05f) return;
    Shape s{{cx, cy, hx, hy}, {c.x, c.y, c.z, a}, {(float)kind, rot, p1, p2}, {p3, 0, 0, 0},
            {clip ? clip[0] : -1e5f, clip ? clip[1] : -1e5f, clip ? clip[2] : 1e5f, clip ? clip[3] : 1e5f}};
    shapes_.push_back(s);
}

void BauhausUniverse::rectPx(float x0, float y0, float x1, float y1, const vec3& c, float a, const float* clip) {
    if (x1 <= x0 || y1 <= y0) return;
    shape(K_BOX, 0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (x1 - x0), 0.5f * (y1 - y0), 0.f, c, a, 0.f, 0.f, 0.f, clip);
}

// Overprinted shapes multiply with what is under them (ink over ink darkens); the others cover it.
void BauhausUniverse::setOverprint(bool on) {
    if (on == overprint_) return;
    flushShapes();
    overprint_ = on;
}

void BauhausUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    if (overprint_) glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(Shape), shapes_.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shapes_.size());
    if (overprint_) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    shapes_.clear();
}

// =====================================================================================================
// Type
// =====================================================================================================

float BauhausUniverse::capWidth(const Font& f, const std::string& s, float track) const {
    if (!f.ok || s.empty()) return s.size() * 0.7f;
    float w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        w += f.g[ch - 32].adv / f.capH + track;
        if (i + 1 < s.size()) {
            unsigned char nx = (unsigned char)s[i + 1];
            if (nx < 32 || nx >= 128) nx = '?';
            w += f.kern[ch - 32][nx - 32] / f.capH;
        }
    }
    return w - track;
}

void BauhausUniverse::textQuads(const Font& f, const std::string& s, float x, float base, float capPx, float track,
                                float angle, std::vector<float>& out) const {
    if (!f.ok) return;
    const float k = capPx / f.capH;
    const float ca = std::cos(angle), sa = std::sin(angle);
    float pen = 0.f;
    // u along the line, v down the glyph (pixels, y down)
    auto map = [&](float u, float v, float& X, float& Y) {
        X = x + u * ca + v * sa;
        Y = base - u * sa + v * ca;
    };
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 32 || ch >= 128) ch = '?';
        const Glyph& g = f.g[ch - 32];
        if (g.w > 0) {
            const float u0 = pen + g.ox * k, v0 = g.oy * k, u1 = u0 + g.w * k, v1 = v0 + g.h * k;
            float P[4][2];
            map(u0, v0, P[0][0], P[0][1]);
            map(u1, v0, P[1][0], P[1][1]);
            map(u1, v1, P[2][0], P[2][1]);
            map(u0, v1, P[3][0], P[3][1]);
            const float T[4][2] = {{g.x0, g.y0}, {g.x1, g.y0}, {g.x1, g.y1}, {g.x0, g.y1}};
            const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int j : idx) {
                const float v[4] = {P[j][0], P[j][1], T[j][0], T[j][1]};
                out.insert(out.end(), v, v + 4);
            }
        }
        pen += g.adv * k + track * capPx;
        if (i + 1 < s.size()) {
            unsigned char nx = (unsigned char)s[i + 1];
            if (nx < 32 || nx >= 128) nx = '?';
            pen += f.kern[ch - 32][nx - 32] * k;
        }
    }
}

void BauhausUniverse::drawText(const Font& f, const std::vector<float>& q, const vec3& ink, float alpha) {
    if (q.empty() || !f.ok || alpha <= 0.002f) return;
    glUseProgram(progText_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    set1i(progText_, "uFont", 0);
    set2f(progText_, "uRes", (float)width_, (float)height_);
    set3f(progText_, "uInk", ink);
    set1f(progText_, "uAlpha", alpha);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, q.size() * sizeof(float), q.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(q.size() / 4));
}

// =====================================================================================================
// The composition
// =====================================================================================================

// A balanced arrangement for this bar: the structure (which side the circle weighs, its height, the bar's angle,
// the forms' families) changes every four bars (eight when calm), the details (turns, nudges, the dots) on every bar.
//
// The zones: the free columns left and right of the HUD (x beyond +-11.8), the full height of the sheet. The heavy
// zone holds the circle in one half and the sampler of cleared rows (with the tempo's numerals) in the other; the
// light zone answers diagonally with the triangle and the square; the half disc bleeds off the light zone's outer
// edge, the quarter disc sits in a corner of the heavy one. The long bar crosses the whole sheet on the diagonal
// between the circle and the sampler, the title set along it in the light zone.
void BauhausUniverse::compose(long bar, float dur, bool instant) {
    const VariantDef& V = VARIANTS[variant_];
    const float W = viewH_.x, H = viewH_.y;
    const float ZX = 11.8f;
    const float zw = std::max(5.f, W - ZX);
    const int lvl = level_;
    const bool drop = dropMode_ && realDrop_;
    const bool peak = lvl >= 2 || dropMode_;
    const int period = lvl >= 1 ? 4 : 8;
    const long era = (long)std::floor((double)bar / period);
    const Pick rb{seed_ ^ ((uint64_t)(era + 100000) * 0x9E3779B97F4A7C15ull)};
    Rng rs(seed_ ^ ((uint64_t)(bar + 100000) * 0xC2B2AE3D27D4EB4Full));
    // the circle changes side now and then with the structure (never on the drop: it grows the disc there)
    if (!dropMode_ && (bar_ <= -1000 || (rb.chance(1, 0.3f) && (era & 1))))
        heavy_ = rb.chance(2, 0.5f) ? -1 : 1;
    const float hs = (float)heavy_;
    const float hx = hs * (ZX + zw * 0.5f), lx = -hx;
    const float up = rb.chance(3, 0.5f) ? 1.f : -1.f; // the circle in the upper or lower half
    const float k = lvl == 0 ? 0.82f : lvl == 1 ? 0.92f : 1.f;
    auto col = [&](int i) { return i < 0 ? hex(V.ink) : hex(V.col[i & 3]); };
    const float Q = 0.5f * PI;
    // the left zone keeps its corners for the HUD's song and section lines: its forms stay between them
    auto edgeY = [&](float side, float sign) { return side < 0.f ? (sign > 0.f ? hudTop_ : hudBottom_) : H; };

    Form T[F_COUNT];
    for (int i = 0; i < F_COUNT; i++) T[i] = el_[i].to, T[i].a = 0.f;

    // ---- The great circle and its ring.
    const int circleCol = drop ? -1 : (rb.chance(4, 0.55f) ? 0 : rb.chance(5, 0.5f) ? 2 : -1);
    {
        // the heavy zone's height, less the sampler's block at its foot (a full sampler, the numerals, the bar's
        // lane), holds the circle and its ring
        const bool ring = V.style == ST_KANDINSKY ? lvl >= 1 || drop : peak;
        const float ringK = ring ? (V.style == ST_KANDINSKY ? 1.34f : 1.22f) : 1.f;
        const float far = up * (edgeY(hs, up) - 0.4f), near = -up * (edgeY(hs, -up) - 1.2f - SAMPLER_ROWS * 0.9f - 4.6f);
        const float span = std::fabs(far - near);
        Form& f = T[F_CIRCLE];
        f.s = std::min({zw * 0.42f, 4.6f, span * 0.5f / ringK}) * k;
        f.x = hx + hs * rs.range(-0.5f, 0.5f) - hs * (drop ? 0.8f : 0.f);
        const float slack = std::max(0.f, span * 0.5f - f.s * ringK);
        f.y = 0.5f * (far + near) + up * slack * (rb.range(6, -0.6f, 0.6f) + 0.25f * (float)rs.irange(-1, 1));
        f.rot = 0.f;
        f.a = 1.f;
        f.col = col(circleCol);
        Form& r = T[F_RING];
        if (ring) {
            r = f;
            r.s = f.s * ringK;
            r.s2 = V.style == ST_KANDINSKY ? 0.32f : 0.12f; // the ring's width
            r.col = V.style == ST_KANDINSKY ? col(circleCol == 2 ? 2 : 1) : col(-1);
            if (V.style == ST_KANDINSKY && circleCol == 2) r.col = col(0);
            r.a = 1.f;
        }
    }
    const float cy = T[F_CIRCLE].y, cr = T[F_CIRCLE].s;

    // ---- The long bar: across the whole sheet on the diagonal, in the lane between the circle and the sampler.
    static const float ANG[6] = {0.2f, 0.28f, 0.36f, -0.2f, -0.28f, -0.36f};
    const float ang = -up * std::fabs(ANG[rb.irange(7, 0, 5)]) + 0.04f * (float)rs.irange(-1, 1);
    // above the numerals, rising away from them (they start left of the circle's center)
    const float barY = -up * (edgeY(hs, -up) - 1.2f - SAMPLER_ROWS * 0.9f - 3.8f);
    auto barAt = [&](float x) { return barY + std::tan(ang) * (x - hx); };
    {
        Form& f = T[F_BAR0];
        f.x = hx;
        f.y = barY;
        f.s = W * 1.6f;
        f.s2 = lvl == 0 ? 0.2f : peak ? 0.34f : 0.27f;
        f.rot = ang;
        f.a = 1.f;
        f.col = col(-1);
    }

    // ---- The light zone, in three bands: the triangle on the sampler's side, the square on the circle's side, the
    // half disc on the sheet's edge beyond the triangle, a short bar beyond the square.
    {
        Form& t = T[F_TRIANGLE];
        if (lvl >= 1 || dropMode_ || rb.chance(8, 0.4f)) {
            static const float TROT[5] = {0.f, PI, 0.f, PI / 6.f, -PI / 12.f};
            t.s = std::min(zw * 0.32f, 3.4f) * k * (drop ? 1.1f : 1.f);
            t.s2 = t.s * 0.92f;
            t.x = lx - hs * 0.5f + 0.4f * (float)rs.irange(-1, 1);
            t.y = -up * (rb.range(9, 3.8f, 4.6f) + 0.3f * (float)rs.irange(-1, 1));
            t.rot = TROT[rb.irange(10, 0, 4)] + (float)rs.irange(-1, 1) * PI / 12.f;
            t.a = 1.f;
            t.col = col(circleCol == 0 ? (rb.chance(11, 0.5f) ? 1 : 2) : (rb.chance(12, 0.6f) ? 1 : 0));
        }
        Form& q = T[F_SQUARE];
        if (lvl >= 1 || dropMode_ || V.style == ST_ALBERS) {
            static const float SROT[4] = {0.f, PI / 4.f, PI / 12.f, 0.f};
            const bool albers = V.style == ST_ALBERS;
            q.rot = albers ? 0.f : SROT[rb.irange(13, 0, 3)] + (float)rs.irange(0, 1) * PI / 12.f;
            q.s = std::min(zw * 0.24f, 2.5f) * k * (albers ? 1.3f : 1.f) / (std::fabs(std::sin(2.f * q.rot)) > 0.5f ? 1.25f : 1.f);
            q.x = lx - hs * 0.3f + 0.4f * (float)rs.irange(-1, 1);
            q.y = up * (rb.range(14, 4.2f, 5.2f) + 0.3f * (float)rs.irange(-1, 1));
            q.a = 1.f;
            int c = rb.chance(15, 0.5f) ? 2 : -1;
            if (c == circleCol) c = c == 2 ? -1 : 2;
            q.col = col(c);
        }
        Form& h = T[F_HALF];
        if (lvl >= 1 || dropMode_) {
            h.s = std::min(zw * 0.27f, 2.9f) * k;
            h.x = lx + hs * 0.6f * (float)rs.irange(-1, 1);
            h.y = -up * (edgeY(-hs, -up) + 0.05f);
            h.rot = 0.f;
            h.a = 1.f;
            h.col = col(rb.chance(16, 0.5f) ? 0 : 3);
            if (V.style == ST_ALBERS) h.col = col(2);
        }
        Form& b = T[F_BAR1];
        if (lvl >= 1 || dropMode_) {
            b.s = zw * 0.3f;
            b.s2 = 0.4f;
            b.x = lx + hs * (rb.chance(17, 0.5f) ? 0.8f : -0.8f);
            b.y = up * (edgeY(-hs, up) - 1.6f);
            b.rot = 0.f;
            b.a = 1.f;
            b.col = col(rb.chance(18, 0.5f) ? 0 : 2);
            if (q.a > 0.f && length(b.col - q.col) < 0.05f) b.col = col(length(q.col - col(0)) < 0.05f ? 1 : 0);
        }
    }

    // ---- The quarter disc in the heavy zone's corner beyond the circle.
    {
        Form& q = T[F_QUARTER];
        q.s = std::min(zw * 0.34f, 3.6f) * (lvl == 0 ? 0.8f : 1.f);
        q.x = hs * W;
        q.y = up * edgeY(hs, up);
        q.a = lvl >= 1 || rb.chance(19, 0.5f) ? 1.f : 0.f;
        int c = rb.chance(20, 0.5f) ? 1 : -1;
        if (c == circleCol) c = 1;
        q.col = col(V.style == ST_ALBERS ? 3 : c);
        const float gap = std::hypot(q.x - T[F_CIRCLE].x, q.y - T[F_CIRCLE].y) - T[F_CIRCLE].s * (T[F_RING].a > 0.f ? 1.4f : 1.f) - 0.6f;
        q.s = std::min(q.s, gap);
        if (q.s < 1.6f) q.a = 0.f;
    }

    // ---- Thin rules.
    {
        const bool lines = V.style == ST_MOHOLY || V.style == ST_KANDINSKY;
        Form& l0 = T[F_LINE0];
        if (lvl >= 1 || lines || dropMode_) {
            l0.x = lx + hs * rs.range(-1.f, 1.f);
            l0.y = 0.f;
            l0.s = W * 1.4f;
            l0.s2 = 0.045f;
            l0.rot = Q + ang * 0.6f * (rb.chance(21, 0.5f) ? 1.f : -1.f);
            l0.a = 1.f;
            l0.col = col(-1);
        }
        Form& l1 = T[F_LINE1];
        if (peak || (lines && lvl >= 1)) {
            l1.x = 0.f;
            l1.y = cy + up * (cr + 0.8f);
            l1.s = W * 1.6f;
            l1.s2 = 0.045f;
            l1.rot = -ang * 0.5f;
            l1.a = 1.f;
            l1.col = col(V.style == ST_MOHOLY ? 0 : -1);
        }
    }

    // ---- Dots threaded on the bar in the heavy zone (they step along it every two beats).
    for (int i = 0; i < 3; i++) {
        Form& d = T[F_DOT0 + i];
        if (i >= (lvl == 0 ? 1 : 3)) continue;
        const float x = hx - 1.f - i * zw * 0.14f + 0.4f * (float)rs.irange(-1, 1);
        d.x = x;
        d.y = barAt(x);
        d.s = i == 0 ? 0.5f : i == 1 ? 0.36f : 0.26f;
        d.a = 1.f;
        d.col = col(i == 0 ? 0 : i == 1 ? 2 : -1);
    }

    // ---- The type: the title up the light zone's inner edge (or across its middle band), the tempo's numerals and
    // a small block of information over the sampler.
    std::string title = title_.empty() ? lower(V.name) : title_;
    {
        Form& t = T[F_TITLE];
        const float w = capWidth(med_, title, 0.02f);
        if (era & 1) {
            // across the middle band, between the triangle and the square
            t.s = std::clamp((zw - 0.8f) / std::max(0.5f, w), 0.5f, 0.9f);
            t.x = lx - std::min(w * t.s, zw - 0.8f) * 0.5f;
            t.y = t.s * 0.5f + 0.25f * (float)rs.irange(-1, 1);
            t.rot = 0.f;
        } else {
            // turned up the inner edge, reading upward from the foot of the band
            t.s = std::clamp(15.f / std::max(0.5f, w), 0.6f, lvl == 0 ? 1.f : 1.25f);
            const float edge = lx + hs * (zw * 0.5f - 0.3f);
            t.x = hs > 0.f ? edge : edge + t.s;
            t.y = -w * t.s * 0.5f + t.s;
            t.rot = 0.5f * PI;
        }
        t.a = 1.f;
        t.col = col(-1);
    }
    // the sampler: in the heavy zone's other half, from the sheet's edge toward the middle
    {
        int n = 0;
        for (const Trace& tr : traces_) n += !tr.medallion;
        samplerCols_ = MAX_TRACES / SAMPLER_ROWS;
        samplerCell_ = std::min(0.9f, zw * 0.85f / samplerCols_);
        samplerX_ = hx - samplerCols_ * samplerCell_ * 0.5f + hs * 0.6f;
        samplerY_ = -up * (edgeY(hs, -up) - 1.2f);
        samplerDir_ = up;
        const int rows = std::max(1, (n + samplerCols_ - 1) / samplerCols_);
        const float top = samplerY_ + samplerDir_ * (rows * samplerCell_ + 0.9f); // the sampler's inner edge
        Form& num = T[F_NUMBER];
        if (lvl >= 1 || dropMode_) {
            num.s = lvl >= 2 ? 1.8f : 1.5f;
            num.x = samplerX_;
            num.y = top + (up > 0.f ? num.s : 0.f);
            num.rot = 0.f;
            num.a = 1.f;
            num.col = col(drop ? -1 : (circleCol == 0 ? 2 : 0));
        }
        Form& inf = T[F_INFO];
        inf.s = 0.32f;
        char nb[16];
        std::snprintf(nb, sizeof(nb), "%d", (int)std::lround(bpm_));
        inf.x = num.a > 0.f ? num.x + capWidth(med_, nb, -0.02f) * num.s + 0.6f : samplerX_;
        inf.y = num.a > 0.f ? num.y : top + (up > 0.f ? 1.2f : 0.f);
        inf.rot = 0.f;
        inf.a = 1.f;
        inf.col = col(-1);
        // the medallions in the light zone's middle band, beside the title
        medR_ = std::min(1.5f, zw * 0.15f);
        medalX_[0] = lx - hs * zw * 0.22f;
        medalY_[0] = (era & 1) ? up * 2.f : 0.f;
        medalX_[1] = lx + hs * zw * 0.1f;
        medalY_[1] = (era & 1) ? -up * 2.f : up * 0.4f;
    }
    char num[16];
    std::snprintf(num, sizeof(num), "%d", (int)std::lround(bpm_));
    std::string info = lower(section_.empty() ? std::string(V.name) : section_);
    if (!info1_.empty()) info += "\n" + lower(info1_);
    if (!info2_.empty()) info += "\n" + lower(info2_);

    for (int i = 0; i < F_COUNT; i++) {
        const std::string text = i == F_TITLE ? title : i == F_NUMBER ? std::string(num) : i == F_INFO ? info : std::string();
        setTarget(i, T[i], dur, instant, text);
    }
    placeTraces(std::max(dur, 0.6f), instant);
    // the drop's disc: on the left it stays clear of the HUD's corners
    if (drop) { discTX_ = hx; discTY_ = hs < 0.f ? 0.5f : -up * 1.5f; }
}

void BauhausUniverse::setTarget(int slot, Form to, float dur, bool instant, const std::string& text) {
    Element& e = el_[slot];
    const bool isText = slot == F_TITLE || slot == F_NUMBER || slot == F_INFO;
    if (instant) {
        e.cur = e.from = e.to = to;
        e.t = 1.f;
        e.twoPhase = false;
        e.text = e.nextText = text;
        return;
    }
    const bool same = std::fabs(e.to.x - to.x) + std::fabs(e.to.y - to.y) + std::fabs(e.to.s - to.s) +
                      std::fabs(e.to.rot - to.rot) + std::fabs(e.to.a - to.a) + std::fabs(e.to.s2 - to.s2) < 1e-3f &&
                      length(e.to.col - to.col) < 1e-3f && e.nextText == text;
    if (same) return;
    e.from = e.cur;
    // a form appearing grows (a word fades in) where it goes, one leaving shrinks (fades) where it is
    if (e.from.a <= 0.01f) { e.from = to; e.from.a = 0.f; if (!isText) { e.from.a = to.a; e.from.s = 0.f; } }
    if (to.a <= 0.01f) { to = e.cur; if (!isText) to.s = 0.f; else to.a = 0.f; if (!isText) to.a = e.cur.a; }
    e.to = to;
    e.t = 0.f;
    e.dur = std::max(0.05f, dur);
    e.swapped = false;
    const float far = std::hypot(e.from.x - to.x, e.from.y - to.y);
    const bool bar = slot == F_BAR0 || slot == F_BAR1 || slot == F_LINE0 || slot == F_LINE1;
    const bool turn = (bar || isText) && std::fabs(e.from.rot - to.rot) > (isText ? 0.05f : 0.15f);
    e.twoPhase = e.cur.a > 0.01f && to.a > 0.01f && (far > 6.f || turn || (isText && text != e.text));
    if (!e.twoPhase && text != e.text) e.text = text;
    e.nextText = text;
}

void BauhausUniverse::updateElements(float dt) {
    for (int i = 0; i < F_COUNT; i++) {
        Element& e = el_[i];
        const bool isText = i == F_TITLE || i == F_NUMBER || i == F_INFO;
        if (e.t >= 1.f) {
            e.cur = e.to;
            if (e.to.s <= 0.f && !isText) e.cur.a = e.to.a = 0.f;
            continue;
        }
        e.t = std::min(1.f, e.t + dt / e.dur);
        auto lerpForm = [](const Form& a, const Form& b, float s) {
            Form o;
            o.x = lerpf(a.x, b.x, s); o.y = lerpf(a.y, b.y, s); o.s = lerpf(a.s, b.s, s); o.s2 = lerpf(a.s2, b.s2, s);
            o.rot = lerpf(a.rot, b.rot, s); o.a = lerpf(a.a, b.a, s); o.col = lerp(a.col, b.col, s);
            return o;
        };
        if (e.twoPhase) {
            if (e.t < 0.5f) {
                const float s = ease(e.t * 2.f);
                e.cur = e.from;
                if (isText) e.cur.a = e.from.a * (1.f - s);
                else e.cur.s = e.from.s * (1.f - s);
            } else {
                if (!e.swapped) { e.swapped = true; e.text = e.nextText; }
                const float s = ease(e.t * 2.f - 1.f);
                e.cur = e.to;
                if (isText) e.cur.a = e.to.a * s;
                else e.cur.s = e.to.s * s;
            }
            continue;
        }
        e.cur = lerpForm(e.from, e.to, ease(e.t));
    }
}

// The cleared rows' places: the sampler's grid (from the sheet's edge toward the middle, the newest last), the
// medallions beside the circle's answer.
void BauhausUniverse::placeTraces(float dur, bool instant) {
    int n = 0, m = 0;
    const float hs = (float)heavy_;
    for (Trace& t : traces_) {
        Form to;
        to.a = 1.f;
        if (t.medallion) {
            const int i = std::min(m++, 1);
            to.x = medalX_[i];
            to.y = medalY_[i];
            to.s = medR_;
        } else {
            const int c = n % samplerCols_, r = n / samplerCols_;
            n++;
            // the grid reads from the circle's side outward
            const int cc = hs > 0.f ? c : samplerCols_ - 1 - c;
            to.x = samplerX_ + (cc + 0.5f) * samplerCell_;
            to.y = samplerY_ + samplerDir_ * (r + 0.5f) * samplerCell_;
            to.s = samplerCell_ * 0.4f;
        }
        if (instant) { t.cur = t.from = t.to = to; t.t = 1.f; continue; }
        if (std::fabs(t.to.x - to.x) + std::fabs(t.to.y - to.y) + std::fabs(t.to.s - to.s) < 1e-3f && t.t > 0.f) continue;
        t.from = t.cur;
        t.to = to;
        t.far = t.t >= 1.f && std::hypot(t.from.x - to.x, t.from.y - to.y) > 10.f;
        t.t = 0.f;
        t.dur = dur;
    }
}

// One form of the poster, in its style (spin: the peak's turn).
void BauhausUniverse::drawForm(int slot, const Form& f, float spin) {
    const VariantDef& V = VARIANTS[variant_];
    if (f.a <= 0.002f || f.s <= 0.02f) return;
    const vec2 c = px(f.x, f.y);
    const float s = f.s * unitPx_;
    const vec3 paper = hex(V.paper), ink = hex(V.ink);
    const bool op = V.overprint;
    const float planeA = V.style == ST_MOHOLY ? 0.86f : 1.f;
    switch (slot) {
    case F_CIRCLE:
        setOverprint(false);
        shape(K_DISC, c.x, c.y, s, s, 0.f, f.col, f.a, 0.f, 0.f, 4.f);
        if (V.style == ST_KANDINSKY) {
            // concentric circles, a little off center
            const vec3 c2 = length(f.col - hex(V.col[2])) < 0.1f ? hex(V.col[0]) : hex(V.col[2]);
            const vec3 c3 = length(f.col - ink) < 0.1f ? paper : ink;
            shape(K_DISC, c.x + s * 0.08f, c.y - s * 0.06f, s * 0.62f, s * 0.62f, 0.f, c2, f.a, 0.f, 0.f, 4.f);
            shape(K_DISC, c.x + s * 0.12f, c.y - s * 0.1f, s * 0.28f, s * 0.28f, 0.f, c3, f.a, 0.f, 0.f, 4.f);
        }
        break;
    case F_RING:
        setOverprint(false);
        shape(K_DISC, c.x, c.y, s, s, 0.f, f.col, f.a, s - f.s2 * unitPx_, 0.f, 4.f);
        break;
    case F_SQUARE:
        setOverprint(false);
        if (V.style == ST_ALBERS) {
            // homage to the square: three more squares nested, each lower in the one before
            const vec3 cols[4] = {hex(V.col[3]), hex(V.col[1]), hex(V.col[0]), hex(V.col[2])};
            float h = s, oy = 0.f;
            for (int i = 0; i < 4; i++) {
                shape(K_BOX, c.x, c.y + oy, h, h, f.rot + spin * 0.3f, cols[i], f.a);
                const float nh = h * 0.72f;
                oy += (h - nh) * 0.45f;
                h = nh;
            }
        } else {
            setOverprint(op && V.style == ST_MOHOLY);
            shape(K_BOX, c.x, c.y, s, s, f.rot - spin * 0.7f, f.col, f.a * planeA);
        }
        break;
    case F_TRIANGLE:
        setOverprint(op);
        shape(K_TRI, c.x, c.y, s, f.s2 * unitPx_, f.rot + spin, f.col, f.a * planeA, 0.f);
        break;
    case F_HALF: {
        (void)spin;
        setOverprint(op);
        // the flat side on the sheet's edge (or the left zone's line above the HUD's corner), facing in
        const float face = f.y > 0.f ? PI : 0.f;
        shape(K_DISC, c.x, c.y, s, s, 0.f, f.col, f.a * planeA, 0.f, face, 0.5f * PI);
        break;
    }
    case F_QUARTER: {
        setOverprint(false);
        // a quarter of a disc centered on the sheet's corner, facing in
        const float face = (f.x > 0.f ? -1.f : 1.f) * (f.y > 0.f ? 0.75f * PI : 0.25f * PI);
        shape(K_DISC, c.x, c.y, s, s, 0.f, f.col, f.a, 0.f, face, 0.25f * PI);
        break;
    }
    case F_BAR0: case F_BAR1: case F_LINE0: case F_LINE1: {
        setOverprint(slot == F_BAR1 && op);
        const float th = std::max(1.2f, f.s2 * unitPx_);
        shape(K_BOX, c.x, c.y, s, th, -f.rot, f.col, f.a);
        break;
    }
    default: // dots
        setOverprint(false);
        shape(K_DISC, c.x, c.y, s, s, 0.f, f.col, f.a, 0.f, 0.f, 4.f);
        break;
    }
}

// The poster itself (into posterT_): the forms back to front, the cleared rows' forms, the words.
void BauhausUniverse::drawPoster() {
    const VariantDef& V = VARIANTS[variant_];
    const float spin = spin_ + tetrisTurn_;
    for (int i : {F_QUARTER, F_HALF, F_SQUARE, F_RING, F_CIRCLE, F_LINE0, F_LINE1, F_BAR0, F_TRIANGLE, F_BAR1}) {
        Form f = el_[i].cur;
        if (i == F_RING) f.s2 *= 1.f + 0.6f * kickShown_; // the ring thickens with the kicks
        drawForm(i, f, spin);
    }
    // the dots swell with the kicks
    for (int i = F_DOT0; i <= F_DOT2; i++) {
        Form f = el_[i].cur;
        f.s *= 1.f + 0.35f * kickShown_;
        drawForm(i, f, spin);
    }
    // the cleared rows: small forms in the piece colors, the medallions of a Tetris
    setOverprint(false);
    for (const Trace& t : traces_) {
        const Form& f = t.cur;
        if (f.s <= 0.02f) continue;
        const vec2 c = px(f.x, f.y);
        const float s = f.s * unitPx_;
        if (t.medallion) {
            for (int q = 0; q < 4; q++)
                shape(K_DISC, c.x, c.y, s, s, 0.f, hex(V.piece[std::clamp(t.quarters[q], 0, 6)]), f.a, 0.f,
                      0.25f * PI + q * 0.5f * PI + spin, 0.25f * PI + 0.004f);
            shape(K_DISC, c.x, c.y, s * 0.18f, s * 0.18f, 0.f, hex(V.paper), f.a, 0.f, 0.f, 4.f);
            continue;
        }
        const vec3 cc = hex(V.piece[std::clamp(t.col, 0, 6)]);
        switch (t.kind) {
        case 0: shape(K_BOX, c.x, c.y, s * 0.88f, s * 0.88f, t.rot, cc, f.a); break;
        case 1: shape(K_DISC, c.x, c.y, s, s, 0.f, cc, f.a, 0.f, 0.f, 4.f); break;
        case 2: shape(K_TRI, c.x, c.y, s, s, t.rot, cc, f.a, 1.f); break;
        case 3: shape(K_DISC, c.x, c.y + s * 0.35f, s, s, 0.f, cc, f.a, 0.f, t.rot, 0.5f * PI); break;
        default: {
            // a quarter disc filling the cell from one corner
            const float a = t.rot + 0.25f * PI;
            const float ox = -std::sin(a) * s * 0.9f, oy = std::cos(a) * s * 0.9f;
            shape(K_DISC, c.x + ox, c.y + oy, s * 1.75f, s * 1.75f, 0.f, cc, f.a, 0.f, a, 0.25f * PI);
            break;
        }
        }
    }
    flushShapes();
    setOverprint(false);
    // the words
    std::vector<float> q;
    for (int i : {F_TITLE, F_NUMBER, F_INFO}) {
        const Element& e = el_[i];
        if (e.cur.a <= 0.002f || e.text.empty()) continue;
        const Font& f = i == F_INFO ? light_ : med_;
        const float track = i == F_INFO ? 0.06f : i == F_NUMBER ? -0.02f : 0.02f;
        const float capPx = e.cur.s * unitPx_;
        q.clear();
        size_t a = 0;
        int line = 0;
        while (a <= e.text.size()) {
            size_t b = e.text.find('\n', a);
            if (b == std::string::npos) b = e.text.size();
            const std::string s = e.text.substr(a, b - a);
            const float base = e.cur.y - e.cur.s - line * e.cur.s * 1.7f;
            const vec2 p = px(e.cur.x, base);
            textQuads(f, s, p.x, p.y, capPx, track, e.cur.rot, q);
            a = b + 1;
            line++;
        }
        drawText(f, q, e.cur.col, e.cur.a);
    }
}

// =====================================================================================================
// The board
// =====================================================================================================

vec3 BauhausUniverse::pieceColor(int type) const {
    const VariantDef& V = VARIANTS[variant_];
    vec3 c = hex(V.piece[std::clamp(type, 0, 6)]);
    // on the photogram's dark ground the darkest piece turns pale
    if (slow_ > 0.001f && ugl::luma(c) < 0.2f) c = lerp(c, vec3(0.78f, 0.77f, 0.74f), smoothstepf(0.f, 1.f, slow_));
    return c;
}

// One cell of a piece: a flat square, inset by the gutter on the sides it does not share with its piece, its corner
// cut where two shared sides meet around a missing diagonal (so the piece's outline stays clean). Edges snapped to
// pixels so the cells of a piece meet without a seam. mask bits: 1 left, 2 right, 4 below, 8 above, 16 below-left,
// 32 below-right, 64 above-left, 128 above-right. round > 0 turns a lone cell toward a disc.
void BauhausUniverse::drawCell(float x, float y, float half, int m, const vec3& c, float a, float round, const float* clip) {
    const float g = GAP * half * 2.f;
    const float x0 = x - half + ((m & 1) ? 0.f : g), x1 = x + half - ((m & 2) ? 0.f : g);
    const float yb = y - half + ((m & 4) ? 0.f : g), yt = y + half - ((m & 8) ? 0.f : g);
    vec2 p0 = px(x0, yt), p1 = px(x1, yb);
    p0 = vec2{std::round(p0.x), std::round(p0.y)};
    p1 = vec2{std::round(p1.x), std::round(p1.y)};
    if (round > 0.f) {
        const float h = 0.5f * (p1.x - p0.x);
        shape(K_BOX, 0.5f * (p0.x + p1.x), 0.5f * (p0.y + p1.y), h, 0.5f * (p1.y - p0.y), 0.f, c, a, round * h, 0.f, 0.f, clip);
        return;
    }
    vec2 gi = px(x0 + g, yt - g), go = px(x1 - g, yb + g);
    gi = vec2{std::round(gi.x), std::round(gi.y)};
    go = vec2{std::round(go.x), std::round(go.y)};
    const bool cutBL = (m & 1) && (m & 4) && !(m & 16), cutBR = (m & 2) && (m & 4) && !(m & 32);
    const bool cutTL = (m & 1) && (m & 8) && !(m & 64), cutTR = (m & 2) && (m & 8) && !(m & 128);
    if (!cutBL && !cutBR && !cutTL && !cutTR) { rectPx(p0.x, p0.y, p1.x, p1.y, c, a, clip); return; }
    // three slabs: top (y p0..gi), middle, bottom (go..p1)
    rectPx(cutTL ? gi.x : p0.x, p0.y, cutTR ? go.x : p1.x, gi.y, c, a, clip);
    rectPx(p0.x, gi.y, p1.x, go.y, c, a, clip);
    rectPx(cutBL ? gi.x : p0.x, go.y, cutBR ? go.x : p1.x, p1.y, c, a, clip);
}

// A piece's mark, on one of its cells: a small geometric form in paper or ink, whichever stands out on the piece.
void BauhausUniverse::drawMark(float x, float y, float half, int type, float a) {
    const vec3 pc = pieceColor(type);
    const VariantDef& V = VARIANTS[variant_];
    const vec3 paper = lerp(hex(V.paper), vec3(1.f), 0.25f), ink = hex(V.ink);
    const vec3 light = ugl::luma(paper) > ugl::luma(ink) ? paper : ink, dark = ugl::luma(paper) > ugl::luma(ink) ? ink : paper;
    const vec3 c = ugl::luma(pc) > 0.42f ? dark : light;
    const vec2 p = px(x, y);
    const float r = half * 2.f * unitPx_ * 0.24f;
    switch (std::clamp(type, 0, 6)) {
    case I: shape(K_DISC, p.x, p.y, r, r, 0.f, c, a, 0.f, 0.f, 4.f); break;
    case O: shape(K_BOX, p.x, p.y, r * 0.8f, r * 0.8f, 0.25f * PI, c, a); break;
    case T: shape(K_TRI, p.x, p.y + r * 0.1f, r * 1.05f, r * 0.95f, 0.f, c, a, 0.f); break;
    case S: shape(K_DISC, p.x, p.y + r * 0.4f, r * 1.1f, r * 1.1f, 0.f, c, a, 0.f, 0.f, 0.5f * PI); break;
    case Z: shape(K_DISC, p.x, p.y - r * 0.4f, r * 1.1f, r * 1.1f, 0.f, c, a, 0.f, PI, 0.5f * PI); break;
    case J: shape(K_DISC, p.x, p.y, r * 1.05f, r * 1.05f, 0.f, c, a, r * 0.55f, 0.f, 4.f); break;
    default: shape(K_DISC, p.x - r * 0.75f, p.y + r * 0.75f, r * 1.7f, r * 1.7f, 0.f, c, a, 0.f, 0.25f * PI, 0.25f * PI); break;
    }
}

void BauhausUniverse::drawBoard(const Game& g) {
    const float br = saturate(bridge_.amount);
    const vec3 bInk = ugl::toSrgb(bridge_.ink);
    const float bk = smoothstepf(0.f, 1.f, br);
    auto tone = [&](const vec3& c) { return lerp(c, bInk, bk); };
    auto mask8 = [&](int x, int y, auto same) {
        const int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1}, dy[8] = {0, 0, 1, -1, 1, 1, -1, -1};
        int m = 0;
        for (int i = 0; i < 8; i++)
            if (same(x + dx[i], y + dy[i])) m |= 1 << i;
        return m;
    };
    const float markA = 1.f - bk;

    // the well: a fine frame of ink, open at the top, a heavier baseline
    {
        const float th = std::max(1.f, std::round(height_ / 1080.f * 2.f));
        const vec3 c = tone(ink_);
        const vec2 a = px(-5.f, 10.f), b = px(5.f, -10.f);
        const float al = 0.7f * (1.f - 0.6f * br);
        const float L = std::round(a.x), R = std::round(b.x), B = std::round(b.y);
        rectPx(L - 2.f * th, a.y, L - th, B + th, c, al);
        rectPx(R + th, a.y, R + 2.f * th, B + th, c, al);
        rectPx(L - 2.f * th, B + th, R + 2.f * th, B + 4.f * th, c, al);
    }

    // the ghost: the piece's cells as fine outlined squares
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
            const vec3 c = tone(lerp(ground_, ink_, 0.5f));
            for (auto& cc : gc) {
                if (cc[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)cc[0], (float)cc[1]);
                const vec2 p0 = px(w.x - 0.5f + GAP, w.y + 0.5f - GAP), p1 = px(w.x + 0.5f - GAP, w.y - 0.5f + GAP);
                const float L = std::round(p0.x), T = std::round(p0.y), R = std::round(p1.x), B = std::round(p1.y);
                if (!in(cc[0] - 1, cc[1])) rectPx(L, T, L + th, B, c, 1.f);
                if (!in(cc[0] + 1, cc[1])) rectPx(R - th, T, R, B, c, 1.f);
                if (!in(cc[0], cc[1] - 1)) rectPx(L, T, R, T + th, c, 1.f);
                if (!in(cc[0], cc[1] + 1)) rectPx(L, B - th, R, B, c, 1.f);
            }
        }
    }

    // locked pieces, then their marks
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
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0 || !mark_[y][x]) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            drawMark(p.x, p.y, 0.5f, c.type, markA);
        }

    // cleared rows: the cells round into discs and slide out sideways, clipped at the well's edges
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
            const float round = smoothstepf(0.f, 0.45f, k);
            for (int x = 0; x < Game::W; x++) {
                if (w.type[x] < 0) continue;
                const vec3 p = layout::cellPos((float)x, 0.f);
                drawCell(p.x + dx, w.y, 0.5f - 0.08f * round, 0, tone(pieceColor(w.type[x])), 1.f, round, clip);
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
        const int mi = markIndex(cells);
        for (auto& c : cells) {
            if (c[1] < Game::HIDDEN) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            drawCell(w.x, w.y, 0.5f, mask8(c[0], c[1], in), tone(pieceColor(p.type)), 1.f);
        }
        if (cells[mi][1] >= Game::HIDDEN) {
            const vec3 w = layout::cellPos(cells[mi][0] + dx, cells[mi][1] + dy);
            drawMark(w.x, w.y, 0.5f, p.type, markA);
        }
    }

    // the hard drop's bar at the contact
    if (lockLine_ > 0.f) {
        const float k = 1.f - lockLine_;
        const float cx = 0.5f * (lockX0_ + lockX1_), hw = (0.5f * (lockX1_ - lockX0_) + 0.3f) * (1.f - ease(k));
        const float th = std::max(1.f, height_ / 1080.f * 3.f);
        const vec2 a = px(cx - hw, lockY_), b = px(cx + hw, lockY_);
        rectPx(a.x, a.y + th * 0.5f, b.x, a.y + 1.5f * th, tone(ink_), 0.9f);
    }

    // previews: small flat pieces with their marks in the HUD columns
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
        const int mi = markIndex(cells);
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 4; j++) {
                const auto& c = cells[j];
                const float x = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, y = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
                if (i == 0) drawCell(x, y, 0.5f * sc, mask8(c[0], c[1], in), tone(pieceColor(type)), alpha);
                else if (j == mi) drawMark(x, y, 0.5f * sc, type, alpha * markA);
            }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.35f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
    flushShapes();
}

// =====================================================================================================
// The HUD
// =====================================================================================================

// Outfit on the paper, lowercase in the Bauhaus manner: labels light and spaced, each after a small form (a square,
// a disc, a triangle in the primaries), values medium, the section in the signature color after a disc; the bonus
// gauge a fine ring charged in the signature color.
void BauhausUniverse::drawHud(const Game& g, const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink = ink_, ground = ground_, accent = hex(V.col[0]);
    const float vis = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);

    if (!panels.empty()) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : ground;
            rectPx(r.x, r.y, r.x + r.w, r.y + r.h, c, std::min(1.f, r.alpha));
        }
        flushShapes();
    }

    if (vis > 0.01f) {
        const vec2 c = project(layout::GAUGE_CENTER);
        const float r = project(vec3(layout::GAUGE_CENTER.x + layout::GAUGE_R, layout::GAUGE_CENTER.y, 0.f)).x - c.x;
        const float fill = g.bonusActive() ? saturate(g.bonusLeft() / Game::BONUS_SECONDS) : saturate(gaugeShown_);
        const float th = std::max(1.f, 1.5f * s), th2 = std::max(2.f, 6.f * s);
        shape(K_DISC, c.x, c.y, r + th * 0.5f, r + th * 0.5f, 0.f, ink, (g.bonusReady() ? 0.75f : 0.35f) * vis, r - th * 0.5f, 0.f, 4.f);
        if (fill > 0.002f)
            shape(K_DISC, c.x, c.y, r + th2 * 0.5f, r + th2 * 0.5f, 0.f, accent, 0.95f * vis, r - th2 * 0.5f,
                  fill * PI, fill * PI);
        // a small yellow triangle in the ring once it is charged
        if (g.bonusReady()) shape(K_TRI, c.x, c.y + r * 0.05f, r * 0.32f, r * 0.28f, 0.f, hex(V.col[V.style == ST_KANDINSKY ? 2 : 1]), vis);
        flushShapes();
    }

    std::vector<float> q;
    int labelIdx = 0;
    for (const HudText& h0 : hud) {
        if (h0.alpha <= 0.01f || h0.text.empty()) continue;
        HudText h = h0;
        h.text = ascii(h.text);
        const float base = h.scale * 12.f;
        const float alpha = saturate(h.alpha) * vis;
        q.clear();
        float left = 0.f;
        // cap: cap height in pixels, dy: shift of the top of the caps from the anchor
        auto place = [&](const Font& f, const std::string& text, float cap, float track, float dy = 0.f) {
            const float w = capWidth(f, text, track) * cap;
            left = ugl::hudLeft(h, w, width_);
            textQuads(f, text, left, h.y + dy + cap, cap, track, 0.f, q);
        };
        switch (h.role) {
        case HUD_UNIVERSE:
            place(light_, lower(h.text), base * 0.55f, 0.06f, base * 0.1f);
            drawText(light_, q, ink, saturate(0.35f + h.alpha * 1.4f) * vis);
            continue;
        case HUD_SECTION: {
            const float cap = base * 0.6f;
            place(med_, lower(h.text), cap, 0.04f, base * 0.05f);
            const float a = saturate(0.4f + h.alpha * 1.5f) * vis;
            shape(K_DISC, left - cap * 0.75f, h.y + base * 0.05f + cap * 0.62f, cap * 0.32f, cap * 0.32f, 0.f, accent, a, 0.f, 0.f, 4.f);
            flushShapes();
            drawText(med_, q, accent, a);
            continue;
        }
        case HUD_TITLE:
            place(med_, lower(h.text), base * 0.62f, 0.f, base * 0.02f);
            drawText(med_, q, ink, saturate(h.alpha * 1.6f) * vis);
            continue;
        case HUD_INFO:
            place(light_, lower(h.text), base * 0.5f, 0.05f, base * 0.1f);
            drawText(light_, q, ink, saturate(alpha * 1.3f) * 0.85f);
            continue;
        case HUD_LABEL: {
            const float cap = base * 0.46f;
            place(light_, lower(h.text), cap, 0.16f, base * 0.12f);
            const float a = saturate(alpha * 1.3f) * 0.85f;
            // a small form before the label: a square, a disc, a triangle in turn
            const int k = labelIdx++ % 3;
            const vec3 fc = hex(V.col[k == 0 ? 0 : k == 1 ? 2 : 1]);
            const float bx = left - cap * 0.95f, by = h.y + base * 0.12f + cap * 0.55f, r = cap * 0.36f;
            if (k == 0) shape(K_BOX, bx, by, r, r, 0.f, fc, a);
            else if (k == 1) shape(K_DISC, bx, by, r * 1.1f, r * 1.1f, 0.f, fc, a, 0.f, 0.f, 4.f);
            else shape(K_TRI, bx, by, r * 1.15f, r, 0.f, fc, a, 0.f);
            flushShapes();
            drawText(light_, q, ink, a);
            continue;
        }
        case HUD_VALUE:
            place(med_, h.text, base * 0.64f, 0.f, base * 0.02f);
            drawText(med_, q, ink, alpha);
            continue;
        case HUD_NOTICE:
            place(med_, lower(h.text), base * 0.55f, 0.02f);
            drawText(med_, q, accent, alpha);
            continue;
        case HUD_KEY:
            place(med_, h.text, base * 0.55f, 0.f);
            drawText(med_, q, ink, alpha);
            continue;
        case HUD_TOAST:
            place(med_, lower(h.text), base * 0.74f, 0.f, -base * 0.05f);
            drawText(med_, q, ink, alpha);
            continue;
        case HUD_TOAST_SUB:
            place(light_, h.text, base * 0.5f, 0.04f);
            drawText(light_, q, accent, alpha * 0.9f);
            continue;
        case HUD_DIALOG:
            if (h.scale >= 3.5f * s) {
                place(med_, lower(h.text), base * 0.62f, 0.f);
                drawText(med_, q, ink, alpha);
            } else {
                place(light_, h.text, base * 0.5f, 0.03f);
                drawText(light_, q, h.accent ? accent : ink, alpha);
            }
            continue;
        default:
            place(light_, h.text, base * 0.5f, 0.02f);
            drawText(light_, q, h.accent ? accent : ink, alpha);
            continue;
        }
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void BauhausUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const VariantDef& V = VARIANTS[variant_];
    syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.5f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
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
    const float wp = smoothstepf(1.2f, 2.f, lv_);

    // ---- What the song says: its title, its information, its section.
    bool sectionKnown = false;
    for (const HudText& h : f.hud) {
        if (h.role == HUD_TITLE) {
            std::string t = ascii(h.text), clean;
            // brackets go, "artist - title" keeps the title
            int depth = 0;
            for (char c : t) {
                if (c == '(' || c == '[') depth++;
                else if (c == ')' || c == ']') depth = std::max(0, depth - 1);
                else if (!depth) clean += (c == '_' ? ' ' : c);
            }
            const size_t dash = clean.find(" - ");
            if (dash != std::string::npos) clean = clean.substr(dash + 3);
            while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
            while (!clean.empty() && clean.back() == ' ') clean.pop_back();
            title_ = lower(clean);
        } else if (h.role == HUD_INFO) {
            const std::string t = ascii(h.text);
            const size_t sp = t.find("   ");
            info1_ = sp == std::string::npos ? t : t.substr(0, sp);
            info2_ = sp == std::string::npos ? std::string() : t.substr(sp + 3);
            while (!info2_.empty() && info2_.front() == ' ') info2_.erase(info2_.begin());
        } else if (h.role == HUD_SECTION) {
            section_ = ascii(h.text);
            sectionKnown = true;
        }
    }
    // a real drop, not a chorus (both rise into a peak): from the HUD's section, else from the music's pace
    realDrop_ = dropMode_ && (sectionKnown ? section_ == "DROP" : !music.hasSong || music.speed > 1.26f);
    // the HUD's lines in the left zone's corners (the song above, the section and the universe below), in board units
    {
        float top = 9.6f, bottom = 8.6f;
        const float x0 = px(-11.5f, 0.f).x;
        for (const HudText& h : f.hud) {
            if (h.text.empty() || h.role == HUD_TOAST || h.role == HUD_TOAST_SUB || h.role == HUD_DIALOG) continue;
            if (ugl::hudLeft(h, 1.f, width_) > x0) continue;
            const float base = h.scale * 12.f;
            const float yTop = viewC_.y + (height_ * 0.5f - h.y) / unitPx_, yBot = yTop - base * 0.9f / unitPx_;
            if (yBot > 4.f) top = std::min(top, yBot - 0.5f);
            else if (yTop < -4.f) bottom = std::min(bottom, -yTop - 0.5f);
        }
        hudTop_ = top;
        hudBottom_ = bottom;
    }

    // ---- The paper and the ink: slow motion turns the sheet into a photogram (a dark ground, pale forms).
    {
        const vec3 p = hex(V.paper), i = hex(V.ink);
        const float sn = smoothstepf(0.f, 1.f, slow_);
        const bool dark = ugl::luma(p) < 0.4f;
        const vec3 photoG(0.075f, 0.075f, 0.08f), photoI(0.9f, 0.89f, 0.86f);
        ground_ = lerp(p, photoG, sn);
        ink_ = dark ? lerp(i, photoI, sn) : lerp(i, photoI, sn);
    }

    // ---- The bar grid: a new arrangement on every bar, the dots stepping every two beats.
    {
        const double beat = music.hasSong ? music.beat : time_ * 2.0;
        const long bar = (long)std::floor(beat / 4.0);
        const long step = (long)std::floor(beat / 2.0);
        const float barS = 4.f * 60.f / (music.hasSong ? std::max(40.f, music.bpm) : 120.f);
        if (bar_ <= -1000) {
            compose(bar, 0.f, true);
            bar_ = bar;
        } else if (dropPending_) {
            dropPending_ = false;
            bar_ = bar;
            compose(bar, 0.8f, false);
        } else if (bar != bar_ && overT_ < 0.f) {
            bar_ = bar;
            compose(bar, std::clamp(barS * 0.7f, 1.2f, 2.4f), false);
        }
        if (step != step_) {
            step_ = step;
            if (lv_ > 0.5f && !f.paused) {
                // a dot steps along the bar
                const int i = F_DOT0 + (int)(((step % 3) + 3) % 3);
                Element& e = el_[i];
                const Element& b = el_[F_BAR0];
                if (e.to.a > 0.f && e.t >= 1.f) {
                    Form t = e.to;
                    const float d = (((step / 3) & 1) ? -1.f : 1.f) * 0.9f;
                    t.x += d * std::cos(b.to.rot);
                    t.y += d * std::sin(b.to.rot);
                    setTarget(i, t, 0.45f, false);
                }
            }
        }
    }
    updateElements(dt);

    // ---- Traces fly to their places.
    for (Trace& t : traces_) {
        if (t.t >= 1.f) { t.cur = t.to; continue; }
        t.t = std::min(1.f, t.t + dt / std::max(0.1f, t.dur));
        t.cur.a = 1.f;
        if (t.far) {
            // across the sheet: out where it was, in where it goes
            const bool second = t.t >= 0.5f;
            const Form& at = second ? t.to : t.from;
            const float s = second ? ease(t.t * 2.f - 1.f) : 1.f - ease(t.t * 2.f);
            t.cur.x = at.x;
            t.cur.y = at.y;
            t.cur.s = at.s * s;
            continue;
        }
        const float s = ease(t.t);
        t.cur.x = lerpf(t.from.x, t.to.x, s);
        t.cur.y = lerpf(t.from.y, t.to.y, s);
        t.cur.s = lerpf(t.from.s, t.to.s, s);
    }

    // ---- Peak: the big forms turn slowly (faster in a drop); a Tetris turns them a quarter.
    {
        const float rate = wp * (realDrop_ ? 0.32f : 0.16f) * (f.paused ? 0.f : 1.f) * lerpf(1.f, 0.35f, slow_);
        spin_ += rate * dt;
        tetrisTurn_ = approach(tetrisTurn_, tetrisTurnTo_, 3.f, dt);
        const float kick = std::max(music.kick, kickIn_);
        kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
        kickShown_ = std::max(kickShown_ * std::exp(-dt * 6.f), kick * wp);
    }
    // ---- The drop's disc: in over half a second, held, back over a second and a half.
    {
        const float target = realDrop_ && overT_ < 0.f ? 1.f : 0.f;
        // it grows where the drop puts it and drifts slowly after the composition
        if (disc_ <= 0.f) { discX_ = discTX_; discY_ = discTY_; }
        if (target > disc_) disc_ = std::min(1.f, disc_ + dt / 0.5f);
        else disc_ = std::max(0.f, disc_ - dt / 1.5f);
        discX_ = approach(discX_, discTX_, 0.6f, dt);
        discY_ = approach(discY_, discTY_, 0.6f, dt);
    }
    lockLine_ = std::max(0.f, lockLine_ - dt / 0.28f);
    for (Wipe& w : wipes_) w.t += dt;
    wipes_.erase(std::remove_if(wipes_.begin(), wipes_.end(), [](const Wipe& w) { return w.t > w.delay + WIPE_S; }), wipes_.end());
    gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The poster.
    posterT_.bind();
    const vec3 paper = hex(V.paper);
    glClearColor(paper.x, paper.y, paper.z, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawPoster();

    // ---- On screen: the print, the poster faint behind the well and the HUD's boxes.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_BLEND);
    glUseProgram(progComp_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, posterT_.tex);
    set1i(progComp_, "uPoster", 0);
    set2f(progComp_, "uRes", (float)width_, (float)height_);
    set3f(progComp_, "uPaper", paper);
    set3f(progComp_, "uGround", ground_);
    {
        const vec2 c = px(discX_, discY_);
        const float R = discTX_ < 0.f ? 8.9f : std::min(viewH_.x - 11.8f, 10.f) * 0.5f + 5.2f;
        const float r = R * unitPx_ * easeOut(disc_);
        set4f(progComp_, "uDisc", c.x, c.y, r, disc_ > 0.f ? 1.f : 0.f);
        set3f(progComp_, "uDiscCol", hex(V.col[0]));
    }
    set1f(progComp_, "uSlow", smoothstepf(0.f, 1.f, slow_));
    set1f(progComp_, "uGrain", 1.f - saturate(bridge_.amount));
    {
        std::vector<float> rects, soft;
        auto add = [&](float x0, float y0, float x1, float y1, float feather, float k) {
            if (rects.size() / 4 >= 24) return;
            rects.insert(rects.end(), {x0, y0, x1, y1});
            soft.insert(soft.end(), {feather, k});
        };
        // The veil covers only what is played or read: the band of the well and the HUD columns, and a crisp box
        // behind each HUD line outside it (the song and the section in the corners). Boxes that touch are merged.
        const float pad = 12.f * height_ / 1080.f;
        std::vector<vec4> boxes;
        const vec2 a = px(-11.5f, 20.f), b = px(11.5f, -20.f);
        add(a.x, a.y, b.x, b.y, 1.f, VEIL);
        for (const HudText& h : f.hud) {
            if (h.alpha <= 0.02f || h.text.empty()) continue;
            const float base = h.scale * 12.f;
            const float w = capWidth(light_, ascii(h.text), 0.06f) * base * 0.62f;
            const float x0 = ugl::hudLeft(h, w, width_);
            const vec4 r{x0 - pad - base * 0.5f, h.y - pad, x0 + w + pad, h.y + base * 0.8f + pad};
            if (r.x > a.x && r.z < b.x) continue; // in the band, veiled already
            boxes.push_back(r);
        }
        for (bool merged = true; merged;) {
            merged = false;
            for (size_t i = 0; i < boxes.size() && !merged; i++)
                for (size_t j = i + 1; j < boxes.size() && !merged; j++) {
                    vec4 &p = boxes[i], &q = boxes[j];
                    if (p.x <= q.z + 1.f && q.x <= p.z + 1.f && p.y <= q.w + 1.f && q.y <= p.w + 1.f) {
                        p = {std::min(p.x, q.x), std::min(p.y, q.y), std::max(p.z, q.z), std::max(p.w, q.w)};
                        boxes.erase(boxes.begin() + j);
                        merged = true;
                    }
                }
        }
        for (const vec4& r : boxes) add(r.x, r.y, r.z, r.w, 1.f, VEIL);
        const int n = (int)rects.size() / 4;
        set1i(progComp_, "uCount", n);
        if (n) {
            glUniform4fv(ugl::U(progComp_, "uRects"), n, rects.data());
            glUniform2fv(ugl::U(progComp_, "uSoft"), n, soft.data());
        }
    }
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- The bridge: the world gives way to the bridge's color (the stack turns to its ink in drawBoard).
    if (bridge_.amount > 0.001f) {
        rectPx(0, 0, (float)width_, (float)height_, ugl::toSrgb(bridge_.color), smoothstepf(0.f, 1.f, saturate(bridge_.amount)));
        flushShapes();
    }

    // ---- The game.
    drawBoard(game);

    // ---- Dim (pause, game over screen) and the startup fade.
    {
        const float d = 0.45f * dimT_;
        if (d > 0.002f) rectPx(0, 0, (float)width_, (float)height_, ground_, d);
        if (f.fade < 0.999f) rectPx(0, 0, (float)width_, (float)height_, vec3(0.f), 1.f - saturate(f.fade));
        flushShapes();
    }
    drawHud(game, f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_bauhaus() {
    static const UniverseEntry e{
        "BAUHAUS", "a Bauhaus poster: circles, triangles and squares in flat primaries on printed paper recompose on the bar grid, cleared rows join the poster",
        [] { return std::unique_ptr<Universe>(new BauhausUniverse()); },
        // Graphic and clean, it suits most songs, rhythmic and bright ones best.
        [](const SongLook& l) { return 0.8f + 0.2f * l.density + 0.1f * l.brightness; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

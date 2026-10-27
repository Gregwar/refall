// MAGNETS: magnets lying on a sheet of paper strewn with iron filings, the school experiment.
//
// Every piece is a ferrite magnet painted in two halves (north and south across its axis). Every cell of the stack,
// the falling piece and the previews is a small dipole (two magnetic charges at its faces: inside a piece they
// cancel, so each piece is one bar magnet with its poles at its ends). Tens of thousands of filings lie on the
// paper, simulated on the GPU: each one turns toward the real field where it lies (a damped torque, no jitter),
// creeps slowly toward the strong field around the poles (never under a magnet) and keeps where it went: the paper
// remembers where the stack has been. Far from the magnets the field is too weak against friction and the
// filings stay scattered at random.
//
// The music taps the paper. A calm part shows a few fine filings, lines only near the poles; the verse taps the
// sheet on each bar; a chorus taps it on the kicks, more filings, darker, the lines reaching further. The drop
// turns the polarity of every magnet over: every filing spins half a turn at once, the magnets change their
// paint, and for the whole peak the kicks tap the paper hard while every four bars half of the magnets flip again
// and the lines re-route. A cleared row is lifted off the paper and the filings under it jump and resettle; a
// landing piece pushes a little dust of filings out from under it.
#include "universes/magnets/magnets.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/magnets/shaders.hpp"
#include "universes/registry.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Space Mono (Colophon Foundry), SIL Open Font License, see OFL-SpaceMono.txt.
const unsigned char FONT_TTF[] = {
#include "universes/magnets/SpaceMono-Regular.ttf.inc"
};

namespace sh = magnets_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hexc(uint32_t h) { return vec3(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f); } // sRGB
vec2 v2(float x, float y) { return vec2{x, y}; }
vec2 add(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 sub(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 mul(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

// ---- Variants: the paper, the filings, the lettering, and two sets of paint for the magnets (the drop's set).
struct Variant {
    const char* name;
    uint32_t paper, filing, ink, accent;
    float filingA;     // filings' opacity at full density
    uint32_t calm[7], drop[7];
    float south;       // the south half: the paint darkened
    bool light;
    float shadowA;     // magnets' shadow on the paper
};
const Variant VARIANTS[] = {
    // white paper, black filings, the classroom's ferrite colours
    {"GRAPHITE", 0xF4F4F2, 0x17181A, 0x1C1D20, 0xD7263D, 0.9f,
     {0xE0442F, 0xF2B33A, 0x2F5DA8, 0x2E9466, 0xC23B7A, 0x22A0B4, 0x7A4DB0},
     {0x1E7FD8, 0xE53935, 0xF6C632, 0x8E3FB0, 0x3FA24E, 0xF4842A, 0x16877B}, 0.72f, true, 0.42f},
    // black paper, pale steel filings, bright paint
    {"SILVER", 0x0E0F11, 0xC9CED6, 0xE4E6EA, 0xFF5A4E, 1.f,
     {0xFF6B5B, 0xFFD166, 0x5B8CFF, 0x4CC38A, 0xF06FB0, 0x3FC1D3, 0xA98BFF},
     {0x5B8CFF, 0xFF6B5B, 0xFFD166, 0xA98BFF, 0x4CC38A, 0xFF9F43, 0x3FC1D3}, 0.6f, false, 0.f},
    // a blueprint: white filings on cyanotype blue, pale paint
    {"BLUEPRINT", 0x173A6C, 0xE8EFF8, 0xF0F4FA, 0xFFD166, 0.95f,
     {0xF4F1EA, 0xFFD166, 0xFF8A65, 0x9DE0B8, 0xFF9EC4, 0x8FD3FF, 0xC7B5FF},
     {0xFF6B5B, 0xF4F1EA, 0x8FD3FF, 0xFFD166, 0xC7B5FF, 0x9DE0B8, 0xFF8A65}, 0.72f, false, 0.f},
    // copper filings on black, cool paint
    {"COPPER", 0x111011, 0xD08A50, 0xE9DCCB, 0xE07A3F, 1.f,
     {0x9CC5D6, 0xEDE6DA, 0x4F86C6, 0x6DB38F, 0xC8627A, 0xE3B85C, 0x8F7FD0},
     {0xEDE6DA, 0x4F86C6, 0xE3B85C, 0xC8627A, 0x9CC5D6, 0x6DB38F, 0xF2994A}, 0.62f, false, 0.f},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

// ---- Staging: calm, mid, chorus, drop.
const float DENS[4] = {0.32f, 0.48f, 0.66f, 0.8f};      // share of the filings shown
const float INKA[4] = {0.5f, 0.66f, 0.78f, 0.88f};    // their ink
const float LEN[4] = {0.12f, 0.135f, 0.15f, 0.16f};   // their length (board units)
const float CHAIN[4] = {0.5f, 0.8f, 1.1f, 1.4f};      // longer where the field is strong
const float THR[4] = {0.26f, 0.19f, 0.14f, 0.1f};    // the field that overcomes friction (lower: lines reach further)
const float MIG[4] = {0.015f, 0.03f, 0.05f, 0.08f};   // creeping toward the poles (board units / s)
const float TORQUE[4] = {40.f, 55.f, 70.f, 90.f};
const float WIDTH[4] = {1.05f, 1.1f, 1.2f, 1.3f};     // pixels at 1080p
const float FAR[4] = {0.1f, 0.085f, 0.07f, 0.065f};    // scattered filings (out of the field's hold): faint

// Each piece type's polarity and its axis in its spawn orientation (board space, y up).
const float TYPE_SIGN[7] = {1, -1, 1, -1, 1, -1, 1};
vec2 baseAxis(int type) { return (type == O || type == T) ? v2(0, 1) : v2(1, 0); }
vec2 axisFor(int type, int rot) {
    vec2 a = baseAxis(type);
    for (int r = 0; r < ((rot % 4) + 4) % 4; r++) a = v2(a.y, -a.x); // clockwise
    return a;
}
vec2 cellXY(float x, float y) { const vec3 p = layout::cellPos(x, y); return v2(p.x, p.y); }

// UTF-8 to glyph slots (ASCII, the middle dot, the multiplication sign; anything else as '?').
std::vector<int> slots(const std::string& s) {
    std::vector<int> out;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out.push_back(c >= 32 && c < 127 ? c - 32 : '?' - 32); continue; }
        int cp = 0, extra = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else { cp = c & 0x07; extra = 3; }
        for (int k = 0; k < extra && i + 1 < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[++i] & 0x3F);
        out.push_back(cp == 0xB7 ? 95 : cp == 0xD7 ? 96 : '?' - 32);
    }
    return out;
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool MagnetsUniverse::init(int width, int height) {
    progSim_ = ugl::program(ugl::FULLSCREEN_VS, (std::string(sh::SIM_FS).insert(std::strlen("#version 330 core\n"), sh::HASH_GLSL)).c_str(), "magnets filings sim");
    progFiling_ = ugl::program(sh::FILING_VS, sh::FILING_FS, "magnets filings");
    progCell_ = ugl::program(sh::CELL_VS, sh::CELL_FS, "magnets cells");
    progShape_ = ugl::program(sh::SHAPE_VS, sh::SHAPE_FS, "magnets shapes");
    progPaper_ = ugl::program(ugl::FULLSCREEN_VS, sh::PAPER_FS, "magnets paper");
    progText_ = ugl::program(sh::TEXT_VS, sh::TEXT_FS, "magnets text");
    progPost_ = ugl::program(ugl::FULLSCREEN_VS, sh::POST_FS, "magnets post");

    glGenVertexArrays(1, &emptyVao_);
    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    auto quadVao = [&](GLuint& vao) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    };
    quadVao(filingVao_);
    quadVao(cellVao_);
    glGenBuffers(1, &cellVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(CellInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    quadVao(shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    for (int i = 0; i < 3; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(ShapeInst), (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
    glGenVertexArrays(1, &textVao_);
    glGenBuffers(1, &textVbo_);
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 4));
    glBindVertexArray(0);

    // The filings' state (two sets of two float textures, ping-pong) and the magnetic charges of the frame.
    for (int s = 0; s < 2; s++) {
        glGenTextures(2, stateTex_[s]);
        for (int k = 0; k < 2; k++) {
            glBindTexture(GL_TEXTURE_2D, stateTex_[s][k]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, FW, FH, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        glGenFramebuffers(1, &stateFbo_[s]);
        glBindFramebuffer(GL_FRAMEBUFFER, stateFbo_[s]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, stateTex_[s][0], 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, stateTex_[s][1], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::fprintf(stderr, "[gl] magnets: filings framebuffer incomplete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenTextures(1, &chargeTex_);
    glBindTexture(GL_TEXTURE_2D, chargeTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, MAXQ, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    buildFont();
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

MagnetsUniverse::~MagnetsUniverse() {
    sceneT_.destroy();
    for (GLuint p : {progSim_, progFiling_, progCell_, progShape_, progPaper_, progText_, progPost_})
        if (p) glDeleteProgram(p);
    for (GLuint v : {emptyVao_, filingVao_, cellVao_, shapeVao_, textVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, cellVbo_, shapeVbo_, textVbo_}) glDeleteBuffers(1, &b);
    for (int s = 0; s < 2; s++) glDeleteTextures(2, stateTex_[s]);
    glDeleteFramebuffers(2, stateFbo_);
    glDeleteTextures(1, &chargeTex_);
    if (font_.tex) glDeleteTextures(1, &font_.tex);
    delete (stbtt_fontinfo*)font_.info;
}

void MagnetsUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    // A flat view of the paper at the reference camera's scale on the board plane (layout contract).
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    px_ = 2.f * hh / height_;
    createTargets();
    if (!seeded_) seedFilings();
}

void MagnetsUniverse::createTargets() {
    sceneT_.destroy();
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

// Filings strewn at random over the whole sheet, every one at its own angle.
void MagnetsUniverse::seedFilings() {
    std::vector<float> a(FW * FH * 4), b(FW * FH * 4, 0.f);
    Rng r(0x5EED5EEDull);
    const float x0 = viewC_.x - viewH_.x - 0.3f, x1 = viewC_.x + viewH_.x + 0.3f;
    const float y0 = viewC_.y - viewH_.y - 0.3f, y1 = viewC_.y + viewH_.y + 0.3f;
    for (int i = 0; i < FW * FH; i++) {
        a[i * 4 + 0] = r.range(x0, x1);
        a[i * 4 + 1] = r.range(y0, y1);
        a[i * 4 + 2] = r.range(-PI, PI);
        a[i * 4 + 3] = 0.f;
    }
    for (int s = 0; s < 2; s++) {
        glBindTexture(GL_TEXTURE_2D, stateTex_[s][0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FW, FH, GL_RGBA, GL_FLOAT, a.data());
        glBindTexture(GL_TEXTURE_2D, stateTex_[s][1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FW, FH, GL_RGBA, GL_FLOAT, b.data());
    }
    seeded_ = true;
}

void MagnetsUniverse::buildFont() {
    stbtt_fontinfo* info = new stbtt_fontinfo;
    if (!stbtt_InitFont(info, FONT_TTF, stbtt_GetFontOffsetForIndex(FONT_TTF, 0))) {
        std::fprintf(stderr, "[gl] magnets font init failed\n");
        delete info;
        return;
    }
    Font& f = font_;
    f.info = info;
    f.px = 64.f;
    f.scaleTT = stbtt_ScaleForPixelHeight(info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * f.scaleTT;
    const int AW = 1024, AH = 1024, PAD = 8;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int slot = 0; slot < 97; slot++) {
        const int ch = slot < 95 ? 32 + slot : (slot == 95 ? 0xB7 : 0xD7);
        Glyph& g = f.g[slot];
        int adv, lsb;
        stbtt_GetCodepointHMetrics(info, ch, &adv, &lsb);
        g.adv = adv * f.scaleTT;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(info, f.scaleTT, ch, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) continue;
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

float MagnetsUniverse::textWidth(const std::string& s, float size) const {
    const std::vector<int> sl = slots(s);
    if (!font_.ok) return sl.size() * size * 0.6f;
    float w = 0;
    for (int c : sl) w += font_.g[c].adv * size / font_.px;
    return w;
}

// A line of lettering (pixels, top of the capitals at y); `track`: extra spacing, in em.
void MagnetsUniverse::text(const std::string& s, float x, float y, float size, vec3 col, float alpha, float track) {
    if (!font_.ok || alpha <= 0.003f) return;
    const float k = size / font_.px;
    const float baseline = y + font_.capH * k;
    float pen = x;
    for (int slot : slots(s)) {
        const Glyph& g = font_.g[slot];
        if (g.w > 0) {
            const float x0 = pen + g.ox * k, y0 = baseline + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            const float v[6][4] = {{x0, y0, g.x0, g.y0}, {x1, y0, g.x1, g.y0}, {x1, y1, g.x1, g.y1},
                                   {x0, y0, g.x0, g.y0}, {x1, y1, g.x1, g.y1}, {x0, y1, g.x0, g.y1}};
            for (auto& q : v) {
                textVerts_.insert(textVerts_.end(), q, q + 4);
                textVerts_.insert(textVerts_.end(), {col.x, col.y, col.z, alpha});
            }
        }
        pen += g.adv * k + track * size;
    }
}

// =====================================================================================================
// Song, staging, events
// =====================================================================================================

int MagnetsUniverse::pickVariant(const SongLook& t) const {
    if (!t.hasSong) return 0;
    const int r = (int)((t.seed >> 9) % 100);
    if (t.brightness >= 0.55f && !t.minor) return r < 75 ? 0 : 2;  // bright: white paper, now and then a blueprint
    if (t.minor && t.brightness < 0.45f) return r < 60 ? 1 : 3;    // dark: black paper, silver or copper
    if (t.bassWeight > 0.6f) return 3;                              // heavy: copper
    return r % VARIANT_COUNT;
}

void MagnetsUniverse::setSong(const SongLook& look, float seconds) {
    (void)seconds;
    int v = look.variant >= 0 ? look.variant % VARIANT_COUNT : pickVariant(look);
    if (look.variant < 0 && v == look.avoidVariant) v = (v + 1 + (int)(look.seed % (VARIANT_COUNT - 1))) % VARIANT_COUNT;
    variant_ = v;
    rng_ = Rng(look.seed ^ 0x3A6E7Eull);
}

void MagnetsUniverse::setLevel(int level, float, float seconds, float) {
    level_ = std::clamp(level, 0, 2);
    if (level_ < 2) dropOn_ = false;
    stage_ = level_ == 0 ? 0 : level_ == 1 ? 1 : dropOn_ ? 3 : 2;
    if (seconds <= 0.1f) {
        for (int i = 0; i < 4; i++) stg_[i] = i == stage_ ? 1.f : 0.f;
        pal_ = stage_ == 3 ? 1.f : 0.f;
    }
    if (seconds > 0.1f && level_ >= 1) tap(0.12f); // the sheet tapped once as the music changes
}

// The drop's first downbeat: every magnet's polarity turns over at once. Every filing spins half a turn (the same
// way), the magnets change their paint, and the sheet is tapped hard.
void MagnetsUniverse::drop(float strength) {
    if (level_ < 2) return;
    const float s = 0.6f + 0.4f * saturate(strength);
    dropOn_ = true;
    stage_ = 3;
    pol_ = -pol_;
    pal_ = 1.f; // the new paint and the turned halves at once, on the beat
    for (auto& row : mag_)
        for (Mag& m : row) m.shown = m.sign * pol_;
    curShown_ = pol_;
    spinPending_ = 15.f * s;
    tap(0.35f * s);
    flash_ = s;
    barsInDrop_ = 0;
}

void MagnetsUniverse::refresh(float strength) { tap(0.08f + 0.12f * saturate(strength)); }

vec2 MagnetsUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints MagnetsUniverse::hints() const {
    const Variant& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hexc(V.paper));
    h.ink = ugl::toLinear(hexc(V.ink));
    h.accent = ugl::toLinear(hexc(V.accent));
    h.light = V.light;
    h.styles = TS_INK | TS_LUMA | TS_RIPPLE;
    return h;
}

vec3 MagnetsUniverse::pieceColor(int type) const {
    const Variant& V = VARIANTS[variant_];
    const int t = std::clamp(type, 0, 6);
    return lerp(hexc(V.calm[t]), hexc(V.drop[t]), pal_);
}

void MagnetsUniverse::impulse(float x0, float y0, float x1, float y1, float speed, float spin, float reach, float up) {
    if (imps_.size() >= 8) return;
    imps_.push_back({{x0, y0, x1, y1}, {speed, spin, reach, up}});
}

void MagnetsUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Spawn:
    case GameEvent::Hold:
        curId_ = serial_++;
        break;
    case GameEvent::HardDrop:
    case GameEvent::Lock: {
        if (ev.type == GameEvent::Lock) {
            const int type = ev.cells.empty() ? 0 : ev.cells[0].type;
            const int rot = type == curType_ ? curRot_ : 0;
            const uint64_t id = curId_ ? curId_ : serial_++;
            for (const auto& c : ev.cells) {
                if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
                Mag& m = mag_[c.y][c.x];
                m.id = id;
                m.type = c.type;
                m.axis = axisFor(type, rot);
                m.sign = TYPE_SIGN[type];
                m.shown = m.sign * pol_;
            }
            curId_ = 0;
        }
        // a little dust of filings pushed out from under the piece, where it touches
        if (!ev.cells.empty()) {
            float minX = 99, maxX = -99, bot = 99;
            for (const auto& c : ev.cells) {
                const vec2 p = cellXY((float)c.x, (float)c.y);
                minX = std::min(minX, p.x - 0.5f);
                maxX = std::max(maxX, p.x + 0.5f);
                bot = std::min(bot, p.y - 0.5f);
            }
            if (ev.type == GameEvent::HardDrop) impulse(minX, bot - 0.12f, maxX, bot + 0.05f, 1.3f, 10.f, 0.55f, -0.4f);
            else impulse(minX, bot - 0.08f, maxX, bot + 0.04f, 0.45f, 4.f, 0.35f, -0.4f);
        }
        break;
    }
    case GameEvent::Clear: {
        bool cleared[Game::H] = {};
        for (const auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
        // the cleared magnets are lifted off the paper
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const Mag& m = mag_[c.y][c.x];
            int mask = 0;
            auto same = [&](int x, int y) {
                return x >= 0 && x < Game::W && y >= 0 && y < Game::H && cleared[y] && m.id != 0 && mag_[y][x].id == m.id;
            };
            if (same(c.x - 1, c.y)) mask |= 1;
            if (same(c.x + 1, c.y)) mask |= 2;
            if (same(c.x, c.y + 1)) mask |= 4;
            if (same(c.x, c.y - 1)) mask |= 8;
            vec2 cen = cellXY((float)c.x, (float)c.y);
            int n = 0;
            vec2 sum = v2(0, 0);
            for (int y = 0; y < Game::H && m.id; y++)
                for (int x = 0; x < Game::W; x++)
                    if (cleared[y] && mag_[y][x].id == m.id) { sum = add(sum, cellXY((float)x, (float)y)); n++; }
            if (n) cen = mul(sum, 1.f / n);
            dying_.push_back({cellXY((float)c.x, (float)c.y), c.type, mask, cen, mul(m.axis, m.id ? m.shown : 0.f), 0.f, 0.f});
        }
        // the filings under the rows jump and resettle (a Tetris throws them further)
        const float k = ev.count >= 4 ? 1.5f : 1.f;
        for (int y = 0; y < Game::H; y++) {
            if (!cleared[y]) continue;
            const float wy = cellXY(0, (float)y).y;
            impulse(-5.f, wy - 0.5f, 5.f, wy + 0.5f, 3.2f * k, 30.f * k, 1.f * k, 0.f);
            if (carry_.size() < 4 * 4) carry_.insert(carry_.end(), {wy + 0.5f, 1.f, 0.f, 0.f});
        }
        if (ev.count >= 4) spinPending_ = std::max(spinPending_, 22.f); // a Tetris: the whole sheet shivers round once
        // collapse the bookkeeping like the board
        Mag next[Game::H][Game::W];
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            for (int x = 0; x < Game::W; x++) next[dst][x] = mag_[y][x];
            dst--;
        }
        std::memcpy(mag_, next, sizeof(mag_));
        break;
    }
    case GameEvent::TopOut: {
        // Game over: every magnet is lifted away, from the top down, and the filings in the well fly apart.
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Mag& m = mag_[y][x];
                if (m.type < 0) continue;
                dying_.push_back({cellXY((float)x, (float)y), m.type, 0, cellXY((float)x, (float)y), mul(m.axis, m.shown), 0.f,
                                  0.04f * y + rng_.range(0.f, 0.1f)});
                mag_[y][x] = Mag{};
            }
        impulse(-5.f, -10.f, 5.f, 10.f, 2.2f, 20.f, 1.f, 0.f);
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// Cells of the board the stack doesn't know (a stack inherited from another universe) become magnets of their own,
// vertical, by type; emptied cells are forgotten.
void MagnetsUniverse::syncStack(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            Mag& m = mag_[y][x];
            if (c.type < 0) { m = Mag{}; continue; }
            if (m.type != c.type || m.id == 0) {
                m.id = 0x8000000000000000ull | serial_++;
                m.type = c.type;
                m.axis = v2(0, 1);
                m.sign = TYPE_SIGN[c.type];
                m.shown = m.sign * pol_;
                // grouped with the same type to its left or above, four cells at most (one magnet per piece)
                auto count = [&](uint64_t id) {
                    int n = 0;
                    for (int yy = 0; yy < Game::H; yy++)
                        for (int xx = 0; xx < Game::W; xx++) n += mag_[yy][xx].id == id;
                    return n;
                };
                if (x > 0 && mag_[y][x - 1].type == c.type && (mag_[y][x - 1].id >> 63) && count(mag_[y][x - 1].id) < 4)
                    m.id = mag_[y][x - 1].id;
                else if (y > 0 && mag_[y - 1][x].type == c.type && (mag_[y - 1][x].id >> 63) && count(mag_[y - 1][x].id) < 4)
                    m.id = mag_[y - 1][x].id;
            }
        }
}

// =====================================================================================================
// The field
// =====================================================================================================

// A cell's dipole: a north charge on the face its axis points to, a south one on the opposite face.
void MagnetsUniverse::addCharges(vec2 c, vec2 axis, float q, std::vector<float>& out) {
    const vec2 n = add(c, mul(axis, 0.5f)), s = sub(c, mul(axis, 0.5f));
    out.insert(out.end(), {n.x, n.y, q, 0.f, s.x, s.y, -q, 0.f});
}

namespace {
// Charges on the same spot add up (inside a piece the faces cancel: one bar magnet).
int mergeCharges(std::vector<float>& v, size_t from) {
    struct Q { long long key; float x, y, q; };
    std::vector<Q> qs;
    for (size_t i = from; i + 3 < v.size(); i += 4)
        qs.push_back({(long long)std::lround(v[i] * 64.f) * 1000003LL + std::lround(v[i + 1] * 64.f), v[i], v[i + 1], v[i + 2]});
    std::sort(qs.begin(), qs.end(), [](const Q& a, const Q& b) { return a.key < b.key; });
    v.resize(from);
    for (size_t i = 0; i < qs.size();) {
        size_t j = i;
        float q = 0;
        while (j < qs.size() && qs[j].key == qs[i].key) q += qs[j++].q;
        if (std::fabs(q) > 1e-3f) v.insert(v.end(), {qs[i].x, qs[i].y, q, 0.f});
        i = j;
    }
    return (int)((v.size() - from) / 4);
}
} // namespace

void MagnetsUniverse::buildCharges(const Game& game) {
    charges_.clear();
    // the stack
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Mag& m = mag_[y][x];
            if (m.type < 0) continue;
            addCharges(cellXY((float)x, y - game.rowOffset(y)), m.axis, m.sign * pol_, charges_);
        }
    // the previews: smaller magnets in the margins
    auto preview = [&](int type, vec2 center, float sc) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        const vec2 ax = axisFor(type, 0);
        for (auto& c : cells) {
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            const vec2 n = add(pos, mul(ax, 0.5f * sc)), s = sub(pos, mul(ax, 0.5f * sc));
            const float q = 0.45f * TYPE_SIGN[type] * pol_;
            charges_.insert(charges_.end(), {n.x, n.y, q, 0.f, s.x, s.y, -q, 0.f});
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0));
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        preview(game.next(i), v2(c.x, c.y), layout::previewScale(i));
    }
    nStatic_ = mergeCharges(charges_, 0);
    // the falling piece, where it is drawn
    nPiece_ = 0;
    if (game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        int cells[4][2];
        game.pieceCells(p, cells);
        const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
        const vec2 ax = axisFor(p.type, p.rot);
        const size_t from = charges_.size();
        for (auto& c : cells) addCharges(cellXY(c[0] + dx, c[1] + dy), ax, TYPE_SIGN[p.type] * pol_, charges_);
        nPiece_ = mergeCharges(charges_, from);
    }
    const int total = std::min(MAXQ, nStatic_ + nPiece_);
    if (nStatic_ > MAXQ) { nStatic_ = MAXQ; nPiece_ = 0; }
    else nPiece_ = total - nStatic_;
    if (total > 0) {
        glBindTexture(GL_TEXTURE_2D, chargeTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, total, 1, GL_RGBA, GL_FLOAT, charges_.data());
    }
}

vec2 MagnetsUniverse::fieldAt(vec2 p) const {
    vec2 B = v2(0, 0);
    for (size_t i = 0; i + 3 < charges_.size(); i += 4) {
        const vec2 d = v2(p.x - charges_[i], p.y - charges_[i + 1]);
        B = add(B, mul(d, charges_[i + 2] / (d.x * d.x + d.y * d.y + 0.012f)));
    }
    return B;
}

// =====================================================================================================
// Drawing helpers
// =====================================================================================================

void MagnetsUniverse::shape(vec2 a, vec2 b, float w0, float w1, vec3 col, float alpha, int kind, float extra) {
    if (alpha <= 0.003f) return;
    shapes_.push_back({{a.x, a.y, b.x, b.y}, {w0, w1, (float)kind, extra}, {col.x, col.y, col.z, alpha}});
}

// Cells of one piece (game cell coordinates, laid out from `origin` at `scale`): merged into one magnet.
void MagnetsUniverse::addPieceCells(const int (*cells)[2], int n, int type, vec2 origin, float scale, bool gameCoords,
                                    vec2 axis, float shown, float alpha, int mode, float lift, std::vector<CellInst>& out) {
    const Variant& V = VARIANTS[variant_];
    const vec3 cn = pieceColor(type), cs = cn * V.south;
    auto has = [&](int x, int y) {
        for (int i = 0; i < n; i++)
            if (cells[i][0] == x && cells[i][1] == y) return true;
        return false;
    };
    vec2 pos[4];
    vec2 cen = v2(0, 0);
    for (int i = 0; i < n; i++) {
        pos[i] = gameCoords ? add(cellXY((float)cells[i][0], (float)cells[i][1]), origin)
                            : v2(origin.x + cells[i][0] * scale, origin.y - cells[i][1] * scale);
        cen = add(cen, pos[i]);
    }
    cen = mul(cen, 1.f / std::max(1, n));
    for (int i = 0; i < n; i++) {
        const int x = cells[i][0], y = cells[i][1];
        if (gameCoords && y < Game::HIDDEN - 1) continue;
        int mask = 0;
        if (has(x - 1, y)) mask |= 1;
        if (has(x + 1, y)) mask |= 2;
        if (has(x, y + 1)) mask |= 4;
        if (has(x, y - 1)) mask |= 8;
        out.push_back({{pos[i].x, pos[i].y, 0.5f * scale, (float)mask}, {cn.x, cn.y, cn.z, alpha}, {cs.x, cs.y, cs.z, lift},
                       {cen.x, cen.y, axis.x * shown, axis.y * shown}});
    }
    (void)mode;
}

// =====================================================================================================
// The filings
// =====================================================================================================

void MagnetsUniverse::simulate(float dt) {
    float thr = 0, mig = 0, torque = 0;
    for (int i = 0; i < 4; i++) {
        thr += stg_[i] * THR[i];
        mig += stg_[i] * MIG[i];
        torque += stg_[i] * TORQUE[i];
    }
    const int nxt = 1 - cur_;
    glBindFramebuffer(GL_FRAMEBUFFER, stateFbo_[nxt]);
    const GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, bufs);
    glViewport(0, 0, FW, FH);
    glDisable(GL_BLEND);
    glUseProgram(progSim_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stateTex_[cur_][0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, stateTex_[cur_][1]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, chargeTex_);
    set1i(progSim_, "uA", 0);
    set1i(progSim_, "uB", 1);
    set1i(progSim_, "uQ", 2);
    set1i(progSim_, "uNS", nStatic_);
    set1i(progSim_, "uNP", nPiece_);
    set1f(progSim_, "uDt", dt);
    set1f(progSim_, "uThr", thr);
    set1f(progSim_, "uMig", mig);
    set1f(progSim_, "uK", torque);
    set1f(progSim_, "uSeed", rng_.range(0.f, 1000.f));
    set1f(progSim_, "uTap", tapNow_);
    set1f(progSim_, "uSpin", spinNow_);
    set4f(progSim_, "uBounds", viewC_.x - viewH_.x - 0.3f, viewC_.y - viewH_.y - 0.3f, viewC_.x + viewH_.x + 0.3f,
          viewC_.y + viewH_.y + 0.3f);
    glUniform1iv(ugl::U(progSim_, "uOcc"), 22, occ_);
    set1i(progSim_, "uNImp", (int)imps_.size());
    if (!imps_.empty()) {
        std::vector<float> r, k;
        for (const Impulse& im : imps_) {
            r.insert(r.end(), im.r, im.r + 4);
            k.insert(k.end(), im.k, im.k + 4);
        }
        glUniform4fv(ugl::U(progSim_, "uImp"), (GLsizei)imps_.size(), r.data());
        glUniform4fv(ugl::U(progSim_, "uImpK"), (GLsizei)imps_.size(), k.data());
    }
    set1i(progSim_, "uNCarry", (int)carry_.size() / 4);
    if (!carry_.empty()) glUniform4fv(ugl::U(progSim_, "uCarry"), (GLsizei)carry_.size() / 4, carry_.data());
    carry_.clear();
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    const GLenum one = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &one);
    cur_ = nxt;
    imps_.clear();
    tapNow_ = 0.f;
    spinNow_ = 0.f;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void MagnetsUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const Variant& V = VARIANTS[variant_];
    const float dt = std::min(f.dt, 1.f / 20.f);
    pauseFade_ = approach(pauseFade_, f.paused ? 1.f : (f.dim ? 0.6f : 0.f), f.paused ? 4.f : 1.5f, f.dt);
    syncStack(game);
    if (game.hasPiece()) {
        curType_ = game.piece().type;
        curRot_ = game.piece().rot;
    }

    if (!f.paused) {
        time_ += dt;
        // the stage: a peak is a drop when the music drives it (else a chorus)
        if (level_ >= 2 && !dropOn_ && music.hasSong && music.speed >= 1.42f) {
            dropOn_ = true;
            pol_ = -pol_;
            pal_ = 1.f;
            for (auto& row : mag_)
                for (Mag& m : row) m.shown = m.sign * pol_;
            curShown_ = pol_;
            spinPending_ = 12.f;
        }
        stage_ = level_ == 0 ? 0 : level_ == 1 ? 1 : dropOn_ ? 3 : 2;
        for (int i = 0; i < 4; i++) stg_[i] = approach(stg_[i], i == stage_ ? 1.f : 0.f, stage_ == 3 ? 5.f : 1.3f, dt);
        pal_ = approach(pal_, dropOn_ ? 1.f : 0.f, dropOn_ ? 30.f : 1.2f, dt);
        slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 2.5f, dt);
        flash_ = approach(flash_, 0.f, 0.9f, dt);
        // the music taps the paper: the verse on each bar, a chorus and a drop on the kicks (every two beats)
        if (music.hasSong) {
            const long bar = (long)std::floor(music.beat / 4.0);
            if (bar != lastBar_) {
                if (lastBar_ >= 0) {
                    if (stage_ == 1) tap(0.1f);
                    if (stage_ == 3 && ++barsInDrop_ % 4 == 0) {
                        // every four bars of a drop, every other magnet of the stack turns over: the lines re-route
                        for (auto& row : mag_)
                            for (Mag& m : row)
                                if (m.type >= 0 && (m.id & 1)) m.sign = -m.sign;
                    }
                }
                lastBar_ = bar;
            }
        }
        const float kick = music.kick;
        if (kickArmed_ && kick > 0.45f && kick > kickPrev_) {
            kickArmed_ = false;
            if (stage_ == 3) tap(0.3f * (0.6f + 0.4f * kick));
            else if (stage_ == 2) tap(0.17f * (0.6f + 0.4f * kick));
        }
        if (kick < 0.25f) kickArmed_ = true;
        kickPrev_ = kick;
        if (tapPending_ > 0.f) { tap(tapPending_); tapPending_ = 0.f; }
        if (spinPending_ != 0.f) { spinNow_ = spinPending_; spinPending_ = 0.f; }
        // the halves as painted follow the polarity quickly
        for (auto& row : mag_)
            for (Mag& m : row) m.shown = approach(m.shown, m.sign * pol_, 14.f, dt);
        curShown_ = approach(curShown_, pol_, 14.f, dt);
        // cleared magnets: real time, even in slow motion
        for (Dying& d : dying_) d.t += f.dt;
        dying_.erase(std::remove_if(dying_.begin(), dying_.end(), [](const Dying& d) { return d.t > d.delay + 0.4f; }), dying_.end());
        if (overT_ >= 0.f) {
            overT_ += f.dt;
            if (overT_ > 3.f && !game.over()) overT_ = -1.f;
        }
    }

    // the field of the frame, and the filings following it
    for (int y = 0; y < Game::H; y++) {
        occ_[y] = 0;
        for (int x = 0; x < Game::W; x++)
            if (game.cell(x, y).type >= 0) occ_[y] |= 1 << x;
    }
    buildCharges(game);
    if (!f.paused) simulate(dt);

    // the compass in the gauge turns toward the field where it sits (a damped needle)
    {
        const vec2 gc = v2(layout::GAUGE_CENTER.x, layout::GAUGE_CENTER.y);
        const vec2 B = fieldAt(gc);
        if (len(B) > 1e-4f && !f.paused) {
            const float tgt = std::atan2(B.y, B.x);
            float d = tgt - needleA_;
            d -= TAU * std::floor((d + PI) / TAU);
            needleW_ += (60.f * std::sin(d) - 9.f * needleW_) * dt;
            needleA_ += needleW_ * dt;
        }
    }

    // ---- colours of the moment (the slow motion inverts the sheet: filings pale on dark paper, or the reverse)
    const vec3 paper = lerp(hexc(V.paper), hexc(V.filing) * 0.9f, slow_);
    const vec3 filing = lerp(hexc(V.filing), hexc(V.paper), slow_);
    const vec3 ink = lerp(hexc(V.ink), hexc(V.paper), slow_);
    const vec3 accent = hexc(V.accent);
    const float world = 1.f - saturate(bridge_.amount * 1.4f);

    float dens = 0, inkA = 0, flen = 0, chain = 0, thr = 0, wid = 0, far = 0;
    for (int i = 0; i < 4; i++) {
        dens += stg_[i] * DENS[i];
        inkA += stg_[i] * INKA[i];
        flen += stg_[i] * LEN[i];
        chain += stg_[i] * CHAIN[i];
        thr += stg_[i] * THR[i];
        wid += stg_[i] * WIDTH[i];
        far += stg_[i] * FAR[i];
    }
    inkA = std::min(1.f, inkA * (1.f + 0.15f * flash_));

    // ---- the paper and the filings
    sceneT_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(progPaper_);
    set3f(progPaper_, "uPaper", paper);
    set1f(progPaper_, "uLight", V.light ? 1.f : 0.f);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUseProgram(progFiling_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stateTex_[cur_][0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, stateTex_[cur_][1]);
    set1i(progFiling_, "uA", 0);
    set1i(progFiling_, "uB", 1);
    set1i(progFiling_, "uW", FW);
    setMat(progFiling_, "uVP", vp_);
    set1f(progFiling_, "uDensity", dens * world);
    set1f(progFiling_, "uLen", flen);
    set1f(progFiling_, "uChain", chain);
    set1f(progFiling_, "uThr", thr);
    set1f(progFiling_, "uFarA", far);
    set1f(progFiling_, "uPx", px_);
    set1f(progFiling_, "uWidth", std::max(0.8f, wid * height_ / 1080.f));
    set4f(progFiling_, "uColL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progFiling_, "uColR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    set3f(progFiling_, "uInk", filing);
    set1f(progFiling_, "uInkA", inkA * V.filingA);
    glBindVertexArray(filingVao_);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, FW * FH);

    // ---- the magnets: the stack, the cleared ones lifting off, the ghost, the falling piece, the previews
    cells_.clear();
    std::vector<CellInst> ghost;
    {
        // the stack, one magnet per piece (its center from its cells still on the board)
        struct Cen { uint64_t id; float x, y; int n; };
        std::vector<Cen> cens;
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Mag& m = mag_[y][x];
                if (m.type < 0) continue;
                const vec2 p = cellXY((float)x, y - game.rowOffset(y));
                bool found = false;
                for (Cen& c : cens)
                    if (c.id == m.id) { c.x += p.x; c.y += p.y; c.n++; found = true; break; }
                if (!found) cens.push_back({m.id, p.x, p.y, 1});
            }
        for (int y = Game::HIDDEN - 1; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const Mag& m = mag_[y][x];
                if (m.type < 0) continue;
                const float off = game.rowOffset(y);
                auto same = [&](int nx, int ny) {
                    return nx >= 0 && nx < Game::W && ny >= 0 && ny < Game::H && mag_[ny][nx].id == m.id &&
                           std::fabs(game.rowOffset(ny) - off) < 0.02f;
                };
                int mask = 0;
                if (same(x - 1, y)) mask |= 1;
                if (same(x + 1, y)) mask |= 2;
                if (same(x, y + 1)) mask |= 4;
                if (same(x, y - 1)) mask |= 8;
                const vec2 p = cellXY((float)x, y - off);
                vec2 cen = p;
                for (const Cen& c : cens)
                    if (c.id == m.id) { cen = v2(c.x / c.n, c.y / c.n); break; }
                const vec3 cn = pieceColor(m.type), cs = cn * V.south;
                cells_.push_back({{p.x, p.y, 0.5f, (float)mask}, {cn.x, cn.y, cn.z, 1.f}, {cs.x, cs.y, cs.z, 0.f},
                                  {cen.x, cen.y, m.axis.x * m.shown, m.axis.y * m.shown}});
            }
        for (const Dying& d : dying_) {
            const float t = saturate((d.t - d.delay) / 0.24f);
            if (d.t < d.delay) {
                const vec3 cn = pieceColor(d.type), cs = cn * V.south;
                cells_.push_back({{d.p.x, d.p.y, 0.5f, (float)d.mask}, {cn.x, cn.y, cn.z, 1.f}, {cs.x, cs.y, cs.z, 0.f},
                                  {d.cen.x, d.cen.y, d.axis.x, d.axis.y}});
                continue;
            }
            const float a = 1.f - smoothstepf(0.f, 1.f, t);
            if (a <= 0.f) continue;
            const vec3 cn = pieceColor(d.type), cs = cn * V.south;
            cells_.push_back({{d.p.x, d.p.y, 0.5f, (float)d.mask}, {cn.x, cn.y, cn.z, a}, {cs.x, cs.y, cs.z, smoothstepf(0.f, 1.f, t)},
                              {d.cen.x, d.cen.y, d.axis.x, d.axis.y}});
        }
        // the falling piece and its ghost (an outline where it will land)
        if (game.hasPiece() && !game.over()) {
            const Piece& p = game.piece();
            int cells[4][2];
            game.pieceCells(p, cells);
            const vec2 ax = axisFor(p.type, p.rot);
            const float shown = TYPE_SIGN[p.type] * curShown_;
            Piece gp = p;
            gp.y = game.ghostY();
            if (gp.y > p.y) {
                int gc[4][2];
                game.pieceCells(gp, gc);
                addPieceCells(gc, 4, p.type, v2(0, 0), 1.f, true, ax, shown, 0.7f, 2, 0.f, ghost);
            }
            const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
            addPieceCells(cells, 4, p.type, v2(dx, -dy), 1.f, true, ax, shown, 1.f, 0, 0.f, cells_);
        }
        auto preview = [&](int type, vec2 center, float sc, float alpha) {
            if (type < 0) return;
            Piece p{type, 0, 0, 0};
            int cells[4][2];
            game.pieceCells(p, cells);
            float minx = 9, maxx = -9, miny = 9, maxy = -9;
            for (auto& c : cells) {
                minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
                miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
            }
            const vec2 origin = v2(center.x - (minx + maxx) * 0.5f * sc, center.y + (miny + maxy) * 0.5f * sc);
            addPieceCells(cells, 4, type, origin, sc, false, axisFor(type, 0), TYPE_SIGN[type] * curShown_, alpha, 0, 0.f, cells_);
        };
        preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.4f : 1.f);
        for (int i = 0; i < 3; i++) {
            const vec3 c = layout::nextCenter(i);
            preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
        }
    }
    auto drawCells = [&](const std::vector<CellInst>& v, int mode) {
        if (v.empty()) return;
        glUseProgram(progCell_);
        setMat(progCell_, "uVP", vp_);
        set1i(progCell_, "uMode", mode);
        set1f(progCell_, "uPx", px_);
        set1f(progCell_, "uShadowA", V.shadowA * (1.f - slow_));
        set1f(progCell_, "uOutlineW", 1.6f * std::max(1.f, height_ / 1080.f));
        set3f(progCell_, "uShadow", paper * 0.55f);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(CellInst), v.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
    };
    // shadows (darkest wins: one soft shadow per magnet, no seams), then the magnets
    if (V.shadowA > 0.f) {
        glBlendEquation(GL_MIN);
        drawCells(cells_, 1);
        glBlendEquation(GL_FUNC_ADD);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawCells(ghost, 2);
    drawCells(cells_, 0);

    // ---- the well (a hairline frame) and the compass in the gauge
    shapes_.clear();
    {
        const float w = 1.3f * px_ * std::max(1.f, height_ / 1080.f);
        const float x0 = -5.08f, x1 = 5.08f, y0 = -10.08f, y1 = 10.f;
        shape(v2(x0, y1), v2(x0, y0), w, w, ink, 0.55f);
        shape(v2(x0, y0), v2(x1, y0), w, w, ink, 0.55f);
        shape(v2(x1, y0), v2(x1, y1), w, w, ink, 0.55f);
        const vec2 gc = v2(layout::GAUGE_CENTER.x, layout::GAUGE_CENTER.y);
        const float R = layout::GAUGE_R;
        const float fill = game.bonusGauge();
        shape(gc, v2(R, 0), w, w, ink, 0.4f, 1, 1.f);
        shape(gc, v2(R, 0), 0.085f, 0.085f, accent, game.bonusActive() || game.bonusReady() ? 1.f : 0.85f, 1, fill);
        const float needleA = game.bonusReady() ? 0.f : 1.f;
        const vec2 d = v2(std::cos(needleA_), std::sin(needleA_));
        const float L = 0.66f * R;
        shape(gc, add(gc, mul(d, L)), 0.12f, 0.f, accent, needleA);
        shape(gc, sub(gc, mul(d, L)), 0.12f, 0.f, ink, 0.8f * needleA);
        shape(gc, gc, 0.07f, 0.07f, paper, needleA);
    }
    if (!shapes_.empty()) {
        glUseProgram(progShape_);
        setMat(progShape_, "uVP", vp_);
        set1f(progShape_, "uPx", px_);
        glBindVertexArray(shapeVao_);
        glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
        glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(ShapeInst), shapes_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)shapes_.size());
    }

    // ---- the lettering: a plain monospace, labels spaced out
    textVerts_.clear();
    for (const HudText& h : f.hud) {
        if (h.alpha <= 0.02f || h.text.empty()) continue;
        const float base = h.scale * 12.f;
        float size = base, track = 0.f, dy = 0.f, a = std::min(1.f, h.alpha * 1.25f);
        vec3 col = h.accent ? accent : ink;
        switch (h.role) {
        case HUD_TITLE: size = base * 0.95f; break;
        case HUD_INFO: size = base * 0.95f; a *= 0.75f; col = ink; break;
        case HUD_SECTION: size = base * 0.95f; track = 0.25f; col = stage_ == 3 ? accent : ink; break;
        case HUD_UNIVERSE: size = base * 0.9f; track = 0.12f; a *= 0.7f; break;
        case HUD_LABEL: size = base * 0.82f; track = 0.3f; a *= 0.8f; col = ink; break;
        case HUD_VALUE: size = base * 0.92f; break;
        case HUD_TOAST: size = base * 0.9f; track = 0.2f; break;
        case HUD_KEY: col = accent; break;
        default: break;
        }
        const float w = textWidth(h.text, size) + track * size * std::max(0, (int)slots(h.text).size() - 1);
        text(h.text, ugl::hudLeft(h, w, width_), h.y + dy, size, col, a, track);
    }
    if (!textVerts_.empty() && font_.ok) {
        glUseProgram(progText_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, font_.tex);
        set1i(progText_, "uFont", 0);
        set2f(progText_, "uRes", (float)width_, (float)height_);
        glBindVertexArray(textVao_);
        glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
        glBufferData(GL_ARRAY_BUFFER, textVerts_.size() * sizeof(float), textVerts_.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(textVerts_.size() / 8));
    }
    glDisable(GL_BLEND);

    // ---- the print
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progPost_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progPost_, "uScene", 0);
    set1f(progPost_, "uFade", saturate(f.fade));
    set1f(progPost_, "uDim", pauseFade_);
    set1f(progPost_, "uBridge", smoothstepf(0.f, 1.f, bridge_.amount));
    set3f(progPost_, "uBridgeCol", ugl::toSrgb(bridge_.color));
    set3f(progPost_, "uBridgeInk", ugl::toSrgb(bridge_.ink));
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_magnets() {
    static const UniverseEntry e{
        "MAGNETS", "magnets on a sheet of paper, iron filings tracing their field",
        [] { return std::unique_ptr<Universe>(new MagnetsUniverse()); },
        // anything fits; punchy, dynamic songs make the most of the taps and the flips
        [](const SongLook& l) { return 0.8f + 0.4f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const Variant& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

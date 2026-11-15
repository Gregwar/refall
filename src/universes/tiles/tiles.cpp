// TILES: a great wall mosaic, the pieces glazed ceramic tiles; each cleared row breaks into tesserae that fly out and
// are set, one by one along the andamento, into the figures of the mosaic.
//
// One world: a wall of thousands of small tesserae set in grout (mosaic.cpp lays them like a mosaicist: in rows
// following the outlines of the figures, the ground's rows rippling out from them, every tessera its own size, its
// own tilt in the mortar, its own shade). The well is a recessed bed of dark mortar framed by three rows of tesserae;
// on each side a medallion (a ring of gold between two rows of color, a figure on its ground: a sun, a rosette, an
// eight-pointed star), a vase below it from which a vine springs around it, leaves and grapes. The ground and the
// frames are laid from the start; the figures are only the painter's sketch in red ochre on the bare setting bed
// (the sinopia), and the cleared rows lay them: each row's tiles crack and break into tesserae that fly out of the
// well in a stream and are tapped in one after another along the next rows of the figures (their outline first, then
// row by row inward; then the medallion's ground, the vase, the stems, the leaves, the grapes), each chip taking its
// smalto's color as it flies, each one standing a little proud and catching the light as it is set (a gold one
// flashes). The rows alternate between the two sides; a double or a triple lays more, faster. A quad sends a swarm
// that lays a great part of a figure in one sweep, then a glint runs around its medallion's gold ring. Once both sides
// are complete the rows re-set tesserae of the ground, each one glinting (a quad: the gold of a whole side).
//
// The pieces are glazed tiles set in the bed: a pillowed glaze rounded over the edges (pale where it breaks over
// them), a painted motif per type in tin white (or a deep tone on a pale tile) standing a little proud and running on
// across the piece and its neighbours, a faint craquelure, a highlight from a fixed key light and from the moving
// lamp, a shadow on the bed. The ghost is the painter's sketch of where the tile goes.
//
// The light: a lamp moving slowly over the wall, its raking light making each tilted tessera glint in turn (gold and
// silver strongly, glass sharply, stone barely). Calm: a dim, still light; mid: it travels across the wall and a few
// smalti glint on the two-beat grid; a peak: brighter and quicker, more glints, the kicks too. A real drop sets every
// gold smalto ablaze, the fire spreading out from the well, each one shimmering on its own as its tilt wavers, and on
// every bar a gentle ripple lifts the tesserae as it passes outward (a band of light running through the gold); it
// eases in and settles over the next bars. Above all the wall comes alive (updateShow): every bar a slow wave sweeps
// over it turning the tesserae over like a stadium's cards, about their own row's line, their backs a new design laid
// tessera by tessera over the whole ground (a sunburst, frames, a spiral, a lattice, waves) drifting slowly; the
// medallions' rings chase a soft light, the lamp sways from side to side every bar (kept soft: no flash, no slap);
// the drop over, a last wave lays the mosaic back. Slow motion is night: an oil lamp low under the wall, the gold glowing
// warm in the dark. Game over: the tiles come off the wall row by row from the top and fall. Around a song: a lamp
// carried in from the left over the dark wall, its raking light catching the tesserae (the setup); the light sinking
// to the oil lamp of the night, which goes out (the teardown).
//
// Variants (from the song): BYZANTINE (a gold ground, lapis and crimson), ROMAN (a floor of black basalt, marble,
// terracotta and ochre stone, matte), NOCTURNE (a vault of deep blue glass strewn with gold stars), TRENCADIS (broken
// glazed tiles in bright colors, pale grout) and LAGOON (Venetian sea-green glass and silver).
//
// Cost: the mosaic is laid once per variant and screen size (~150 ms on the CPU) and drawn once into a G-buffer; each
// frame is one full-screen lighting pass over it, instanced tiles, fragments and sprites, a quarter-resolution bloom.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/tiles/tiles.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/tiles/shaders.hpp"

namespace {

// Cinzel (the labels and numbers: Roman capitals) and Cormorant Garamond Italic (the song, the section, the
// celebrations), SIL Open Font License, see OFL-Cinzel.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/tiles/Cinzel.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/tiles/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = tiles_shaders;
using tiles::VARIANTS;
using tiles::VARIANT_COUNT;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float s) { return {a.x * s, a.y * s}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

vec3 lin(unsigned v) { return tiles::linc(v); }
vec3 hex(unsigned v) { return tiles::hexc(v); }

// A cleared tile cracks this long (seconds) before it breaks into fragments.
constexpr float CRACK_T = 0.16f;
// The drop's fire lasts this many beats.
constexpr double DROP_BEATS = 32.0;
constexpr int STATE_W = 256;
// The drop's show lasts at most this many beats (it ends with the drop).
constexpr double SHOW_BEATS = 64.0;

// The designs the drop lays over the wall, per variant: the dark, the metal, two accents (sRGB), the metal's material
// and the others'.
struct ShowPalette { unsigned c[4]; int metal, other; };
const ShowPalette SHOW_PAL[] = {
    {{0x101634, 0xd8a644, 0xa8243a, 0x2a4cb0}, tiles::MAT_GOLD, tiles::MAT_GLASS},    // BYZANTINE: lapis, gold, crimson
    {{0x161514, 0xe6dccb, 0xb4542e, 0xd09a48}, tiles::MAT_STONE, tiles::MAT_STONE},   // ROMAN: basalt, marble, terracotta, ochre
    {{0x080e2c, 0xd8aa48, 0x1c3478, 0xe8e4dc}, tiles::MAT_GOLD, tiles::MAT_GLASS},    // NOCTURNE: night, gold, blue, silver
    {{0x123656, 0xf6c42a, 0xe84a2a, 0xf2efe6}, tiles::MAT_GLASS, tiles::MAT_GLASS},   // TRENCADIS: blue, yellow, red, white
    {{0x062424, 0xc0ccd2, 0x2ab0b0, 0x2a8ad0}, tiles::MAT_SILVER, tiles::MAT_GLASS},  // LAGOON: deep sea, silver, turquoise, blue
};

float seedOf(int id) { return std::fmod((float)id * 0.618034f, 1.f) * 97.f + 3.f; }

// Instanced attributes: `vec4s` per instance from location `first`, the unit quad at location 0 when quadVbo != 0.
void instAttribs(GLuint vao, GLuint vbo, GLuint quadVbo, int first, int vec4s, size_t stride) {
    glBindVertexArray(vao);
    if (quadVbo) {
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < vec4s; i++) {
        glEnableVertexAttribArray(first + i);
        glVertexAttribPointer(first + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(first + i, 1);
    }
}

vec2 bezier(vec2 a, vec2 b, vec2 c, vec2 d, float t) {
    const float u = 1.f - t;
    return a * (u * u * u) + b * (3.f * u * u * t) + c * (3.f * u * t * t) + d * (t * t * t);
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool TilesUniverse::init(int width, int height) {
    using shaders::withCommon;
    progGb_ = ugl::program(shaders::GB_VS, withCommon(shaders::GB_FS).c_str(), "tiles gbuffer");
    progComp_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::COMP_FS).c_str(), "tiles wall");
    progCell_ = ugl::program(shaders::CELL_VS, withCommon(shaders::CELL_FS).c_str(), "tiles cells");
    progFrag_ = ugl::program(shaders::FRAG_VS, shaders::FRAG_FS, "tiles fragments");
    progSprite_ = ugl::program(shaders::SPRITE_VS, shaders::SPRITE_FS, "tiles sprites");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "tiles down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "tiles blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "tiles final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "tiles shape");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &tessVao_);
    glGenBuffers(1, &tessVbo_);
    instAttribs(tessVao_, tessVbo_, 0, 1, 4, sizeof(float) * 16);
    glGenVertexArrays(1, &bareVao_);
    glGenBuffers(1, &bareVbo_);
    instAttribs(bareVao_, bareVbo_, 0, 1, 4, sizeof(float) * 16);
    glGenVertexArrays(1, &cellVao_);
    glGenBuffers(1, &cellVbo_);
    instAttribs(cellVao_, cellVbo_, quadVbo_, 1, 4, sizeof(CellInst));
    glGenVertexArrays(1, &fragVao_);
    glGenBuffers(1, &fragVbo_);
    instAttribs(fragVao_, fragVbo_, 0, 1, 5, sizeof(FragInst));
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    instAttribs(spriteVao_, spriteVbo_, quadVbo_, 1, 3, sizeof(SpriteInst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    glBindVertexArray(0);

    glGenTextures(1, &stateTex_);
    glGenTextures(1, &sketchTex_);
    glGenTextures(1, &geoTex_);
    for (GLuint t : {stateTex_, sketchTex_, geoTex_}) {
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t == sketchTex_ ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, t == sketchTex_ ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void TilesUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    for (ugl::Target* t : {&bloomA_, &bloomB_}) t->create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    createGbuffer();
    gbDirty_ = true;
}

void TilesUniverse::createGbuffer() {
    if (gbFbo_) {
        glDeleteFramebuffers(1, &gbFbo_);
        glDeleteTextures(3, gbTex_);
    }
    glGenFramebuffers(1, &gbFbo_);
    glGenTextures(3, gbTex_);
    glBindFramebuffer(GL_FRAMEBUFFER, gbFbo_);
    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, gbTex_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        // the id buffer must not be filtered
        const GLint filt = i == 2 ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, i == 0 ? GL_LINEAR_MIPMAP_LINEAR : filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gbTex_[i], 0);
    }
    const GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "[gl] tiles gbuffer incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

TilesUniverse::~TilesUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progGb_, progComp_, progCell_, progFrag_, progSprite_, progDown_, progBlur_, progFinal_, progShape_})
        glDeleteProgram(p);
    for (GLuint v : {tessVao_, bareVao_, cellVao_, fragVao_, spriteVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, tessVbo_, bareVbo_, cellVbo_, fragVbo_, spriteVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    if (gbFbo_) {
        glDeleteFramebuffers(1, &gbFbo_);
        glDeleteTextures(3, gbTex_);
    }
    glDeleteTextures(1, &stateTex_);
    glDeleteTextures(1, &geoTex_);
    glDeleteTextures(1, &sketchTex_);
}

// =====================================================================================================
// The mosaic
// =====================================================================================================

// Lays the mosaic for the variant and the screen (the ground set, the figures only sketched).
void TilesUniverse::rebuild() {
    design_ = tiles::build(variant_, viewC_, viewH_, seed_ ^ 0x7115ull);
    designVariant_ = variant_;
    designC_ = viewC_;
    designH_ = viewH_;
    frags_.clear();
    // the break's ground over the frame's box
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const tiles::Tessera& t : design_.tess)
            if (t.label == tiles::FRAME_LABEL)
                for (const vec2& c : t.c) {
                    x0 = std::min(x0, c.x); x1 = std::max(x1, c.x);
                    y0 = std::min(y0, c.y); y1 = std::max(y1, c.y);
                }
        fill_.clear();
        if (x1 > x0) {
            const tiles::Design bare = tiles::build(variant_, viewC_, viewH_, seed_ ^ 0x7115ull, false);
            for (const tiles::Tessera& t : bare.tess)
                if (t.center.x > x0 && t.center.x < x1 && t.center.y > y0 && t.center.y < y1 && t.group == 0) fill_.push_back(t);
        }
    }
    const int n = (int)design_.tess.size(), nAll = n + (int)fill_.size();
    stateRows_ = (nAll + 1 + STATE_W - 1) / STATE_W;
    state_.assign((size_t)stateRows_ * STATE_W * 4, 0.f);
    for (int i = 0; i <= nAll; i++) {
        float* s = &state_[(size_t)i * 4];
        const bool figure = i > 0 && i <= n && design_.tess[i - 1].group > 0;
        s[0] = figure ? 1e9f : -1000.f;
        s[1] = -1000.f;
        s[2] = 1e9f;
        s[3] = 0.f;
    }
    progress_[0] = progress_[1] = 0;
    stateDirty_ = true;
    uploadDesign();
    gbDirty_ = true;
}

void TilesUniverse::uploadDesign() {
    const int n = (int)design_.tess.size(), nAll = n + (int)fill_.size();
    auto put = [](std::vector<float>& v, const tiles::Tessera& t, int id) {
        const float o[16] = {t.c[0].x, t.c[0].y, t.c[1].x, t.c[1].y, t.c[2].x, t.c[2].y, t.c[3].x, t.c[3].y,
                             t.color.x, t.color.y, t.color.z, (float)t.mat, t.tilt.x, t.tilt.y, t.rnd, (float)id};
        v.insert(v.end(), o, o + 16);
    };
    std::vector<float> v, vb;
    v.reserve((size_t)n * 16);
    vb.reserve((size_t)nAll * 16);
    for (int i = 0; i < n; i++) {
        put(v, design_.tess[i], i + 1);
        if (design_.tess[i].label != tiles::FRAME_LABEL) put(vb, design_.tess[i], i + 1);
    }
    for (int i = 0; i < (int)fill_.size(); i++) put(vb, fill_[i], n + 1 + i);
    glBindBuffer(GL_ARRAY_BUFFER, tessVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    tessCount_ = n;
    glBindBuffer(GL_ARRAY_BUFFER, bareVbo_);
    glBufferData(GL_ARRAY_BUFFER, vb.size() * sizeof(float), vb.data(), GL_STATIC_DRAW);
    bareCount_ = (int)(vb.size() / 16);
    glBindTexture(GL_TEXTURE_2D, sketchTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, design_.sw, design_.sh, 0, GL_RG, GL_UNSIGNED_BYTE, design_.sketch.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, stateTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, STATE_W, stateRows_, 0, GL_RGBA, GL_FLOAT, state_.data());
    stateDirty_ = false;
    // each tessera's place for the drop's show: its center, the direction across its row, half its width
    std::vector<float> geo((size_t)stateRows_ * STATE_W * 4, 0.f);
    for (int i = 0; i < nAll; i++) {
        const tiles::Tessera& t = i < n ? design_.tess[i] : fill_[i - n];
        const float ax0 = t.c[3].x - t.c[0].x, ay0 = t.c[3].y - t.c[0].y, ax1 = t.c[2].x - t.c[1].x, ay1 = t.c[2].y - t.c[1].y;
        float* o = &geo[(size_t)(i + 1) * 4];
        o[0] = t.center.x;
        o[1] = t.center.y;
        o[2] = std::atan2(ay0 + ay1, ax0 + ax1);
        o[3] = 0.25f * (std::hypot(ax0, ay0) + std::hypot(ax1, ay1));
    }
    glBindTexture(GL_TEXTURE_2D, geoTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, STATE_W, stateRows_, 0, GL_RGBA, GL_FLOAT, geo.data());
}

void TilesUniverse::renderGbuffer(bool bare) {
    glBindFramebuffer(GL_FRAMEBUFFER, gbFbo_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    if (tessCount_ > 0) {
        glUseProgram(progGb_);
        setMat(progGb_, "uVP", vp_);
        set4f(progGb_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        const float px = 2.f * viewH_.y / height_;
        set1f(progGb_, "uPx", px);
        const tiles::VariantDef& V = VARIANTS[variant_];
        set1f(progGb_, "uBevel", std::max(px * 1.6f, V.pitch * 0.16f));
        glBindVertexArray(bare ? bareVao_ : tessVao_);
        const int count = bare ? bareCount_ : tessCount_;
        // the grout around each tessera takes its id (a figure not laid shows its bed there too), then the tesserae
        set1i(progGb_, "uGrout", 1);
        set1f(progGb_, "uDilate", V.grout_w * 0.9f);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
        set1i(progGb_, "uGrout", 0);
        set1f(progGb_, "uDilate", 0.f);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // the albedo's mip chain: the colors to come, washed on the bare bed
    glBindTexture(GL_TEXTURE_2D, gbTex_[0]);
    glGenerateMipmap(GL_TEXTURE_2D);
    gbDirty_ = false;
    gbBare_ = bare;
}

// A new song in the same mosaic: the figures are pried out (over a second and a half) back to the sketch.
void TilesUniverse::resetFigures(float seconds) {
    frags_.clear();
    const int n = (int)design_.tess.size();
    for (int i = 0; i < n; i++) {
        if (design_.tess[i].group == 0) continue;
        float* s = &state_[(size_t)(i + 1) * 4];
        if (seconds <= 0.1f) { s[0] = 1e9f; s[2] = 1e9f; continue; }
        if (s[0] <= time_) s[2] = time_ + rng_.range(0.f, 1.5f);
        else s[0] = 1e9f;
    }
    progress_[0] = progress_[1] = 0;
    stateDirty_ = true;
}

// The next n figure tesserae of a side (the other side's once it is complete; none once both are).
std::vector<int> TilesUniverse::takeTargets(int side, int n) {
    std::vector<int> out;
    for (int pass = 0; pass < 2 && (int)out.size() < n; pass++) {
        const int s = pass == 0 ? side : 1 - side;
        const std::vector<int>& o = design_.order[s];
        while ((int)out.size() < n && progress_[s] < (int)o.size()) out.push_back(o[progress_[s]++]);
    }
    return out;
}

// A row's fragments: each target tessera gets a chip from the tile of the closest color (the load spread over the
// row), out of the well in a stream along an arc, landing one after another `tap` apart from t0.
void TilesUniverse::sendRow(const std::vector<vec3>& cellCols, const std::vector<float>& cellY, const std::vector<int>& targets,
                            float t0, float tap, bool renew) {
    const int M = (int)targets.size();
    if (!M || cellCols.empty()) return;
    const int nc = (int)cellCols.size();
    std::vector<int> load(nc, 0);
    const float cap = (float)M / nc;
    for (int j = 0; j < M; j++) {
        const tiles::Tessera& t = design_.tess[targets[j]];
        int best = 0;
        float bestS = 1e9f;
        for (int c = 0; c < nc; c++) {
            const vec3 d = cellCols[c] - t.color;
            const float s = std::sqrt(dot(d, d)) + 0.25f * std::max(0.f, load[c] - cap) + rng_.range(0.f, 0.08f);
            if (s < bestS) { bestS = s; best = c; }
        }
        load[best]++;
        Frag f;
        f.tess = targets[j];
        f.renew = renew;
        f.from = vec2{layout::cellPos((float)(best % 10), 0.f).x + rng_.range(-0.36f, 0.36f), cellY[best] + rng_.range(-0.34f, 0.34f)};
        f.appear = t0;
        const float sx = t.center.x > 0.f ? 1.f : -1.f;
        f.ctrl1 = f.from + vec2{sx * rng_.range(2.5f, 4.5f), rng_.range(2.0f, 4.0f)};
        f.ctrl2 = t.center + vec2{-sx * rng_.range(0.f, 1.2f), rng_.range(2.2f, 3.6f)};
        f.land = t0 + 0.6f + j * tap + rng_.range(0.f, tap * 0.5f);
        f.launch = std::min(f.land - 0.45f, t0 + 0.34f * j / std::max(1, M - 1) + rng_.range(0.f, 0.04f));
        f.rot0 = rng_.range(-PI, PI);
        f.spin = rng_.range(5.f, 11.f) * (rng_.chance(0.5f) ? 1.f : -1.f);
        f.col0 = cellCols[best];
        frags_.push_back(f);
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void TilesUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.45f) v = 2;                    // NOCTURNE: dark minor songs
        else if (t.hasSong && !t.minor && t.brightness > 0.55f && t.density > 0.5f) v = 3; // TRENCADIS: bright, busy
        else if (t.hasSong && (t.bassWeight > 0.62f || t.bpm < 92.f)) v = 1;        // ROMAN: heavy or slow
        else if (t.hasSong && t.airWeight > 0.6f) v = 4;                           // LAGOON: airy
        else v = (int)(h * 4.f) == 1 ? 4 : 0;                                      // BYZANTINE
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    const bool same = v == variant_ && designVariant_ == v;
    variant_ = v;
    seed_ = t.seed ? t.seed : 1;
    rng_ = Rng(seed_ ^ 0x711Eull);
    lightPhase_ = (float)(seed_ % 628) * 0.01f;
    if (same && !design_.tess.empty()) resetFigures(seconds);
    // otherwise the mosaic is laid anew before the next frame (render)
}

void TilesUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    if (level < 2) dropOn_ = false;
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
}

// The drop (or a chorus's rise: see render, only a real DROP sets the gold ablaze).
void TilesUniverse::drop(float strength) {
    dropOn_ = true;
    dropBeat_ = beat_;
    dropStart_ = time_;
    dropT_ = std::max(dropT_, 0.6f + 0.4f * strength);
    ignite_ = 0.f;
    lastWaveBeat_ = std::floor(beat_ / 4.0);
    waveT_ = 0.f;
    waveA_ = 1.f;
}

// The drop's show, on the beat grid: every bar a wave turns the tesserae over to a new design (kept gentle: one
// wave a bar, no full spins); once the drop is over, a last wave lays the mosaic back. Successive drops start from
// other designs and sweeps.
void TilesUniverse::updateShow(bool want) {
    static const int PATS[5] = {1, 2, 3, 4, 5};
    static const int TURN_SWEEPS[7] = {1, 2, 4, 6, 3, 5, 7};
    if (!show_) {
        if (!want || beat_ - dropBeat_ > 2.0) return;
        show_ = true;
        showEnding_ = false;
        showStart_ = std::round(beat_);
        showStep_ = 0;
        showPat_ = PATS[(showCount_ * 2) % 5];
        swB_ = Sweep{};
        swA_ = Sweep{beat_, 0, 0, showPat_, false};
        showNext_ = showStart_ + 4.0;
        showCount_++;
        return;
    }
    if (beat_ < showNext_) return;
    if (showEnding_) {
        show_ = false;
        return;
    }
    swB_ = swA_;
    if (!want || beat_ - showStart_ > SHOW_BEATS) {
        swA_ = Sweep{showNext_, 0, showPat_, 0, false};
        showEnding_ = true;
    } else {
        showStep_++;
        const int k = showStep_ + showCount_ * 3;
        const int next = PATS[(showStep_ + (showCount_ - 1) * 2) % 5];
        swA_ = Sweep{showNext_, TURN_SWEEPS[k % 7], showPat_, next, false};
        showPat_ = next;
    }
    showNext_ += 4.0;
}

// A new section of the same level: the lamp moves on.
void TilesUniverse::refresh(float strength) {
    (void)strength;
    lightPhase_ += 1.3f;
}

void TilesUniverse::levelUp() {
    // a few gold smalti glint
    for (int k = 0; k < 6 && !design_.field.empty(); k++) {
        const int i = design_.field[rng_.irange(0, (int)design_.field.size() - 1)];
        if (std::fabs(design_.tess[i].center.x) > 11.f) glintTess(i, time_);
    }
}

vec2 TilesUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints TilesUniverse::hints() const {
    const tiles::VariantDef& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = lin(V.field[0]) * 0.45f;
    h.light = false;
    h.ink = lin(V.piece[0]);
    h.accent = lin(V.accent);
    h.styles = TS_TILES | TS_REBUILD | TS_LUMA;
    return h;
}

vec3 TilesUniverse::pieceColor(int type) const { return lin(VARIANTS[variant_].piece[std::clamp(type, 0, 6)]); }

// =====================================================================================================
// Game events
// =====================================================================================================

void TilesUniverse::syncIds(const Game& g) {
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const bool has = g.cell(x, y).type >= 0;
            if (has && !ids_[y][x]) ids_[y][x] = nextId_++;
            if (!has) ids_[y][x] = 0;
        }
}

void TilesUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::Lock: {
        // the tile pressed into the bed: a little mortar squeezed out from under it
        const int id = nextId_++;
        float sx = 0.f, minx = 99.f, maxx = -99.f, lowY = -99.f;
        int n = 0;
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            ids_[c.y][c.x] = id;
            flash_[c.y][c.x] = 1.f;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            sx += p.x;
            minx = std::min(minx, p.x);
            maxx = std::max(maxx, p.x);
            lowY = std::max(lowY, (float)c.y);
            n++;
        }
        if (n > 0) puffs_.push_back({vec2{sx / n, layout::cellPos(0.f, lowY).y - 0.5f}, 0.f, 0.8f, 0.3f + 0.22f * (maxx - minx)});
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
        puffs_.push_back({vec2{(minx + maxx) * 0.5f, layout::cellPos(0.f, (float)low).y - 0.5f}, 0.f, 1.0f, 0.6f + 0.28f * (maxx - minx)});
        break;
    }
    case GameEvent::Clear: {
        bool rows[Game::H] = {};
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H) rows[c.y] = true;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            cracks_.push_back({c.x, c.y, c.type, (float)c.x, (float)-c.y, seedOf(ids_[c.y][c.x] ? ids_[c.y][c.x] : nextId_), 0.f});
        }
        const bool quadClear = ev.count >= 4 || forceQuad_ >= 0;
        forceQuad_ = -1;
        const float t0 = time_ + CRACK_T;
        // the rows' tiles, cell by cell
        std::vector<std::vector<vec3>> rowCols;
        std::vector<float> rowY;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (!rows[y]) continue;
            std::vector<vec3> cols(10, pieceColor(0));
            for (auto& c : ev.cells)
                if (c.y == y && c.x >= 0 && c.x < 10 && c.type >= 0) cols[c.x] = pieceColor(c.type);
            rowCols.push_back(cols);
            rowY.push_back(layout::cellPos(0.f, (float)y).y);
        }
        if (!design_.lay.sides || rowCols.empty()) {
            // no room for the medallions: the chips only shatter (a few glints in the ground)
        } else if (quadClear) {
            // a quad: a great part of a figure laid in one sweep, then a glint running around its medallion's
            // gold ring; once the figures are complete, the whole gold of that side catches the light in turn
            const int side = nextSide_;
            std::vector<vec3> all;
            std::vector<float> allY;
            for (size_t r = 0; r < rowCols.size(); r++) {
                all.insert(all.end(), rowCols[r].begin(), rowCols[r].end());
                allY.insert(allY.end(), 10, rowY[r]);
            }
            const float sweep = 1.6f;
            std::vector<int> tg = takeTargets(side, 380);
            bool renew = false;
            if (tg.empty()) {
                renew = true;
                for (int k = 0; k < 400 && !design_.field.empty(); k++) {
                    const int i = design_.field[rng_.irange(0, (int)design_.field.size() - 1)];
                    const tiles::Tessera& t = design_.tess[i];
                    if (t.mat >= tiles::MAT_GOLD && std::fabs(t.center.x) > 11.6f && (t.center.x > 0.f) == (side == 1)) tg.push_back(i);
                    if (tg.size() >= 160) break;
                }
            }
            if (!tg.empty()) sendRow(all, allY, tg, t0, sweep / tg.size(), renew);
            ringGlintSide_ = tg.empty() ? side : (design_.tess[tg[0]].center.x > 0.f ? 1 : 0);
            ringGlintT_ = t0 + 0.6f + sweep;
            nextSide_ = 1 - side;
        } else {
            const int per = 52 + 8 * ev.count;
            for (size_t r = 0; r < rowCols.size(); r++) {
                const int side = nextSide_;
                nextSide_ = 1 - nextSide_;
                std::vector<int> tg = takeTargets(side, per);
                bool renew = false;
                if (tg.empty()) {
                    // the figures are complete: the row re-sets tesserae of the ground, each one glinting
                    renew = true;
                    for (int k = 0; k < per / 2 && !design_.field.empty(); k++) {
                        const int i = design_.field[rng_.irange(0, (int)design_.field.size() - 1)];
                        const tiles::Tessera& t = design_.tess[i];
                        if (std::fabs(t.center.x) > 11.6f && (t.center.x > 0.f) == (side == 1)) tg.push_back(i);
                    }
                }
                const float tap = (ev.count == 1 ? 0.016f : 0.011f);
                sendRow(rowCols[r], std::vector<float>(10, rowY[r]), tg, t0 + 0.1f * r, tap, renew);
            }
        }
        // compact the ids like the game did
        {
            int nb[Game::H][Game::W] = {};
            float nf[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (rows[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                std::memcpy(nf[dst], flash_[y], sizeof(flash_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
            std::memcpy(flash_, nf, sizeof(flash_));
        }
        break;
    }
    case GameEvent::TopOut: {
        overT_ = 0.f;
        dead_.clear();
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            dead_.push_back({c.x, c.y, c.type, (float)c.x, (float)-c.y, seedOf(ids_[c.y][c.x] ? ids_[c.y][c.x] : nextId_ + c.x)});
        }
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void TilesUniverse::buildBoard(const Game& g) {
    std::vector<CellInst> shadows;
    auto cell = [&](float x, float y, float half, float rot, int type, const vec3& col, float alpha, int kind, float crack,
                    float flash, float px, float py, float seed, float dark) {
        const CellInst c{{x, y, half, rot}, {col.x, col.y, col.z, alpha}, {(float)type, (float)kind, crack, flash}, {px, py, seed, dark}};
        if (kind == 0 || kind == 2) {
            CellInst s = c;
            s.c[1] = 3.f;
            shadows.push_back(s);
        }
        cells_.push_back(c);
    };
    // the ghost: the painter's sketch of where the tile goes
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        Piece gp = p;
        gp.y = g.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            g.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cell(w.x, w.y, 0.5f, 0.f, p.type, pieceColor(p.type), 0.6f, 1, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
            }
        }
    }
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            cell(p.x, p.y, 0.5f, 0.f, c.type, pieceColor(c.type), 1.f, 0, 0.f, flash_[y][x] * 0.6f, (float)x, (float)-y, seedOf(ids_[y][x]), 0.f);
        }
    // cleared tiles cracking
    for (const Crack& k : cracks_) {
        const vec3 p = layout::cellPos((float)k.x, (float)k.y);
        cell(p.x, p.y, 0.5f, 0.f, k.type, pieceColor(k.type), 1.f, 2, saturate(k.t / CRACK_T), 0.f, k.px, k.py, k.seed, 0.f);
    }
    // game over: the tiles come off the wall, row by row from the top, and fall
    for (const Dead& d : dead_) {
        const float t = overT_ - 0.09f * (d.y - Game::HIDDEN) - 0.04f * std::fabs(d.x - 4.5f);
        const vec3 p = layout::cellPos((float)d.x, (float)d.y);
        if (t <= 0.f) {
            cell(p.x, p.y, 0.5f, 0.f, d.type, pieceColor(d.type), 1.f, 0, 0.f, 0.f, d.px, d.py, d.seed, 0.f);
            continue;
        }
        const float fall = 12.f * t * t;
        const float a = 1.f - smoothstepf(0.7f, 1.3f, t);
        if (a <= 0.f) continue;
        const float spin = (std::fmod(d.seed, 2.f) - 1.f) * 1.4f * t;
        cell(p.x + (d.x - 4.5f) * 0.12f * t, p.y - fall, 0.5f * (1.f - 0.1f * t), spin, d.type, pieceColor(d.type), a, 0, 0.f, 0.f,
             d.px, d.py, d.seed, 0.25f * t);
    }
    // the falling piece
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cs[4][2];
        g.pieceCells(p, cs);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        for (auto& c : cs) {
            if (c[1] < Game::HIDDEN - 1) continue;
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cell(w.x, w.y, 0.5f, 0.f, p.type, pieceColor(p.type), 1.f, 0, 0.f, 0.f, (float)c[0], (float)-c[1], seedOf(nextId_), 0.f);
        }
    }
    // previews
    auto preview = [&](int type, vec2 center, float sc, float alpha, float seed) {
        if (type < 0) return;
        Piece p{type, 0, 0, 0};
        int cs[4][2];
        g.pieceCells(p, cs);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cs) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cs) {
            const float px = center.x + (c[0] - (minx + maxx) * 0.5f) * sc, py = center.y - (c[1] - (miny + maxy) * 0.5f) * sc;
            cell(px, py, 0.5f * sc, 0.f, type, pieceColor(type), alpha, 0, 0.f, 0.f, (float)c[0], (float)-c[1], seed, 0.f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f, seedOf(nextId_ + 7));
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f, seedOf(nextId_ + 1 + i));
    }
    // shadows first
    cells_.insert(cells_.begin(), shadows.begin(), shadows.end());
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void TilesUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

// The lettering in pale Roman capitals over a dark halo.
void TilesUniverse::drawHud(const std::vector<HudText>& hud, const std::vector<HudRect>& panels) {
    const tiles::VariantDef& V = VARIANTS[variant_];
    const vec3 halo = hex(V.well) * 0.6f, accent = hex(V.accent);
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
}


// The lettering, by the host (text.hpp): Cinzel in capitals for the names, the numbers and the celebrations,
// Cormorant Garamond Italic for the song, the section and the notices; the variant's pale ink on a soft halo of the
// well's dark.
void TilesUniverse::hudLine(const HudText& h, text::Line& out) {
    const tiles::VariantDef& V = VARIANTS[variant_];
    const float s = height_ / 1080.f;
    const vec3 ink = hex(V.ink), halo = hex(V.well) * 0.6f, accent = hex(V.accent);
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(caps, text::upper(h.text), base * 0.86f, base * 0.18f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.9f, base * 0.04f); break;
    case HUD_SECTION: put(it, h.text, base * 1.15f, base * 0.03f, accent, 0.25f + h.alpha * 1.7f, -base * 0.12f); break;
    case HUD_TITLE: put(it, h.text, base * 1.2f, 0.f, ink, h.alpha * 1.3f, -base * 0.14f); break;
    case HUD_INFO: put(caps, text::upper(h.text), base * 0.74f, base * 0.1f, ink, saturate(alpha * 1.3f) * 0.8f); break;
    case HUD_LABEL: put(caps, text::upper(h.text), base * 0.92f, base * 0.22f, ink, alpha * 1.4f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.05f, base * 0.06f, ink, alpha, -base * 0.06f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.1f, base * 0.04f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.85f, 0.f, ink, alpha); break;
    case HUD_TOAST: put(caps, text::upper(h.text), base * 1.05f, base * 0.16f, ink, alpha, -base * 0.1f); break;
    case HUD_TOAST_SUB: put(it, h.text, base * 1.0f, base * 0.04f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(it, h.text, base * 1.15f, base * 0.05f, ink, alpha);
        else put(caps, h.text, base * 0.76f, base * 0.08f, h.accent ? accent : ink, alpha);
        break;
    default: put(caps, h.text, base * 0.74f, base * 0.04f, h.accent ? accent : ink, alpha); break;
    }
    out.look.halo = halo;
    out.look.haloA = 0.8f;
    out.look.haloW = 0.075f;
    out.look.haloPow = 1.5f;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void TilesUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const tiles::VariantDef& V = VARIANTS[variant_];
    // a new variant: laid at once; a new screen size: once it has stayed for a moment (a window being dragged)
    const bool sized = std::fabs(designH_.x - viewH_.x) > 0.01f || std::fabs(designH_.y - viewH_.y) > 0.01f;
    resizeWait_ = sized ? resizeWait_ + f.dt : 0.f;
    if (designVariant_ != variant_ || design_.tess.empty() || (sized && resizeWait_ > 0.3f)) rebuild();
    if (gbDirty_ || gbBare_ != f.bare) renderGbuffer(f.bare);
    bare_ = f.bare;
    if (!f.bare) syncIds(game); // (a break's blank board keeps the stack's ids)
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !f.bare) { // (a break's blank board is never over)
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
        realDrop_ = sectionKnown ? sectionDrop : (!music.hasSong || music.speed > 1.26f);
        if (dropOn_) {
            const bool over = hasSong_ ? (beat_ - dropBeat_ > DROP_BEATS) : (time_ - dropStart_ > DROP_BEATS * 0.5f);
            if (over) dropOn_ = false;
        }
    }
    if (!f.paused) updateShow(dropOn_ && realDrop_ && level_ >= 2 && overT_ < 0.f);
    showK_ = approach(showK_, show_ && !showEnding_ ? 1.f : 0.f, show_ && !showEnding_ ? 4.f : 1.5f, dt);
    // the gold ablaze: swells in over half a second, settles over the next bars (a chorus only warms it a little)
    {
        float target = 0.f;
        if (dropOn_ && dropT_ > 0.f) {
            const float since = hasSong_ ? (float)((beat_ - dropBeat_) / bps_) : time_ - dropStart_;
            target = dropT_ * (realDrop_ ? 1.f : 0.18f) * (since < 4.f ? 1.f : lerpf(1.f, 0.5f, smoothstepf(4.f, 14.f, since)));
        } else dropT_ = 0.f;
        dropA_ = approach(dropA_, target, 5.f, dt);
        dropEnv_ = approach(dropEnv_, dropA_, 5.f, dt);
        ignite_ += dt * 15.f;
    }
    // a ripple lifting the tesserae every bar while a real drop lasts
    if (dropOn_ && realDrop_ && !f.paused) {
        const double b2 = std::floor(beat_ / 4.0);
        if (b2 > lastWaveBeat_) {
            lastWaveBeat_ = b2;
            waveT_ = 0.f;
            waveA_ = 0.35f + 0.15f * saturate(music.kick);
        }
    }
    waveT_ += dt;
    const float waveAmp = waveA_ * std::exp(-waveT_ * 0.55f) * (dropOn_ && realDrop_ ? 1.f : 0.f) * smoothstepf(0.f, 0.1f, waveT_);

    // ---- The lamp: still and dim in a calm part, travelling over the wall from mid on, quicker at a peak; at night
    // (slow motion, and after a song) an oil lamp low under the wall. Before a song it is carried in from the left
    // over the dark wall, its raking light catching the tesserae as it comes; after one it sinks to the oil lamp,
    // which goes out.
    const float night = std::max(slow_, smoothstepf(0.f, 0.5f, f.teardown));
    const float lampIn = smoothstepf(0.f, 0.9f, f.setup), lampOut = 1.f - smoothstepf(0.5f, 1.f, f.teardown);
    lightPhase_ += wdt * (0.02f + 0.05f * wm + 0.06f * wp + 0.05f * dropEnv_);
    {
        const float ph = lightPhase_;
        const float sideL = std::fmod(std::floor(ph / 3.f), 2.f) < 1.f ? 1.f : -1.f;
        const vec3 still{sideL * (16.f + 1.5f * std::sin(ph * 0.5f)), 9.f + 1.f * std::cos(ph * 0.4f), 12.f};
        const vec3 travel{17.f * std::sin(ph), 2.f + 6.5f * std::sin(ph * 0.73f + 1.1f), 11.f + 2.f * wp};
        vec3 tgt = lerp(still, travel, wm);
        const vec3 lamp{sideL * (15.f + 2.f * std::sin(worldTime_ * 0.3f)), viewC_.y - viewH_.y - 1.5f, 7.5f};
        tgt = lerp(tgt, lamp, night);
        // the show: the lamp sways from one side to the other every bar, its raking light gliding over the wall
        const float sw = std::fmod((float)std::floor(beat_ / 4.0), 2.f) < 1.f ? 1.f : -1.f;
        const vec3 swing{sw * 16.f, viewC_.y + 3.f * std::sin((float)beat_ * 0.2f), 10.f};
        tgt = lerp(tgt, swing, showK_ * (1.f - night));
        lightPos_ = lerp(lightPos_, tgt, 1.f - std::exp(-dt * lerpf(1.5f, 2.2f, showK_)));
        const float target = (0.7f + 0.3f * wm + 0.35f * wp) * (1.f - 0.25f * slow_) * (1.f - 0.6f * over_);
        lightI_ = approach(lightI_, target, 1.2f, dt);
    }
    const float flick = night * (0.9f + 0.1f * std::sin(worldTime_ * 9.f) * std::sin(worldTime_ * 5.3f + 1.f));
    vec3 lightCol = lin(V.light) * (lightI_ + 0.2f * dropEnv_);
    lightCol = lerp(lightCol, vec3(1.f, 0.62f, 0.3f) * 1.3f * (0.85f + 0.15f * flick), night) * smoothstepf(0.05f, 0.5f, f.setup) * lampOut;
    const float reach = 13.f + 4.f * wp + 2.f * dropEnv_ + 2.f * night;
    const vec3 amb = lerp(vec3(0.13f, 0.125f, 0.12f) * (0.8f + 0.25f * wm + 0.3f * wp), vec3(0.035f, 0.03f, 0.03f), night) * (1.f - 0.5f * over_) *
                     lerpf(0.05f, 1.f, lampIn) * lerpf(0.4f, 1.f, lampOut);
    const vec3 lampPos = lerp(vec3(viewC_.x - viewH_.x - 8.f, viewC_.y - 2.f, 8.f), lightPos_, lampIn);
    const float sparkle = 0.5f + 0.25f * wm + 0.4f * wp + 0.4f * dropEnv_;

    // ---- Fragments landing: each one sets its tessera (real time, even in slow motion).
    for (Frag& fr : frags_) {
        if (fr.land > time_) continue;
        const tiles::Tessera& t = design_.tess[fr.tess];
        if (!fr.renew) setTess(fr.tess, fr.land);
        if (t.mat >= tiles::MAT_GOLD || fr.renew) {
            glintTess(fr.tess, fr.land);
            if (rng_.chance(fr.renew ? 0.5f : 0.25f)) sparkles_.push_back({t.center, 0.f, 0.45f, 0.32f, lerp(t.color * 2.f, vec3(1.f, 0.95f, 0.8f), 0.5f)});
        }
        fr.tess = -1;
    }
    frags_.erase(std::remove_if(frags_.begin(), frags_.end(), [](const Frag& fr) { return fr.tess < 0; }), frags_.end());

    // ---- Glints on the beat grid (every two beats from mid on, the kicks too at a peak): a few smalti near the lamp.
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    auto glintNearLamp = [&](int n) {
        for (int k = 0, tries = 0; k < n && tries < 60 && !design_.tess.empty(); tries++) {
            const int i = rng_.irange(0, (int)design_.tess.size() - 1);
            const tiles::Tessera& t = design_.tess[i];
            if (t.mat == tiles::MAT_STONE || !isSet(i)) continue;
            if (std::fabs(t.center.x) < 6.5f || (std::fabs(t.center.x) < 11.4f && t.center.y > -7.f)) continue;
            const float dx = t.center.x - lampPos.x, dy = t.center.y - lampPos.y;
            if (dx * dx + dy * dy > reach * reach * 0.6f) continue;
            glintTess(i, time_);
            sparkles_.push_back({t.center, 0.f, 0.55f, 0.28f + 0.12f * wp, lerp(t.color * 2.f, vec3(1.f, 0.95f, 0.85f), 0.6f)});
            k++;
        }
    };
    if (!f.paused && overT_ < 0.f) {
        const double b2 = std::floor(beat_ / 2.0);
        if (lastGlintBeat_ < 0.0 || b2 < lastGlintBeat_ - 1.0) lastGlintBeat_ = b2;
        if (b2 > lastGlintBeat_) {
            lastGlintBeat_ = b2;
            if (lv_ > 0.6f && slow_ < 0.5f) glintNearLamp(wp > 0.5f ? 4 : 1);
        }
        if (wp > 0.5f && kick > 0.45f && kickPrev_ <= 0.3f) glintNearLamp(2);
        // the lamp carried in before a song: the smalti catch it as it passes
        if (f.setup > 0.1f && f.setup < 0.95f && rng_.chance(dt * 7.f)) glintNearLamp(1);
    }
    kickPrev_ = kick;

    // ---- Cracks, flashes, sparkles, dust.
    for (auto it = cracks_.begin(); it != cracks_.end();) {
        it->t += dt;
        if (it->t >= CRACK_T) it = cracks_.erase(it);
        else ++it;
    }
    for (auto& row : flash_)
        for (float& fl : row) fl = approach(fl, 0.f, 5.f, dt);
    for (Sparkle& sp : sparkles_) sp.t += dt;
    sparkles_.erase(std::remove_if(sparkles_.begin(), sparkles_.end(), [](const Sparkle& s) { return s.t > s.life; }), sparkles_.end());
    for (Puff& p : puffs_) p.t += dt;
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(), [](const Puff& p) { return p.t > p.life; }), puffs_.end());

    if (stateDirty_) {
        glBindTexture(GL_TEXTURE_2D, stateTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, STATE_W, stateRows_, GL_RGBA, GL_FLOAT, state_.data());
        stateDirty_ = false;
    }

    cells_.clear();
    if (!f.bare) buildBoard(game);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float worldK = 1.f - bridgeK;
    auto lightSet = [&](GLuint p) {
        set4f(p, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set4f(p, "uLight", lampPos.x, lampPos.y, lampPos.z, reach);
        set3f(p, "uLightCol", lightCol * worldK);
        set3f(p, "uAmb", amb);
    };

    // ---- The wall.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glUseProgram(progComp_);
    lightSet(progComp_);
    for (int i = 0; i < 3; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, gbTex_[i]);
    }
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, stateTex_);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, sketchTex_);
    glActiveTexture(GL_TEXTURE0);
    set1i(progComp_, "uGb0", 0);
    set1i(progComp_, "uGb1", 1);
    set1i(progComp_, "uGb2", 2);
    set1i(progComp_, "uState", 3);
    set1i(progComp_, "uSketch", 4);
    set4f(progComp_, "uSketchBox", design_.lo.x, design_.lo.y, design_.hi.x, design_.hi.y);
    set1f(progComp_, "uTime", time_);
    set3f(progComp_, "uGrout", lin(V.grout));
    set3f(progComp_, "uBed", lin(V.bed));
    set3f(progComp_, "uSinopia", lin(V.sinopia));
    set3f(progComp_, "uWell", lin(V.well));
    set1f(progComp_, "uBare", f.bare ? 1.f : 0.f);
    set4f(progComp_, "uHudL", layout::LEFT_COLUMN[0], layout::LEFT_COLUMN[1], layout::LEFT_COLUMN[2], layout::LEFT_COLUMN[3]);
    set4f(progComp_, "uHudR", layout::RIGHT_COLUMN[0], layout::RIGHT_COLUMN[1], layout::RIGHT_COLUMN[2], layout::RIGHT_COLUMN[3]);
    {
        const float L = viewC_.x - viewH_.x, R = viewC_.x + viewH_.x, T = viewC_.y + viewH_.y, B = viewC_.y - viewH_.y;
        set4f(progComp_, "uQuietA", L - 1.f, T - 2.6f, -10.f, T + 1.f);
        set4f(progComp_, "uQuietB", L - 1.f, B - 1.f, -11.f, B + 3.2f);
        set4f(progComp_, "uQuietC", 11.f, B - 1.f, R + 1.f, B + 2.2f);
    }
    set1f(progComp_, "uDrop", dropEnv_ * (realDrop_ ? 1.f : 0.3f) * (1.f - 0.6f * showK_));
    set1f(progComp_, "uIgnite", ignite_);
    set4f(progComp_, "uWave", waveT_ * 10.f, waveAmp, 1.6f, 0.f);
    {
        float strength = 0.f, ang = 0.f;
        if (ringGlintT_ > 0.f && time_ > ringGlintT_ && design_.lay.sides) {
            const float u = time_ - ringGlintT_;
            strength = std::sin(PI * saturate(u / 1.6f)) * 1.2f;
            const float sg = ringGlintSide_ == 0 ? 1.f : -1.f;
            ang = sg * (u * 4.4f);
            if (u > 1.6f) ringGlintT_ = -1.f;
        }
        const vec2 C = design_.lay.med[ringGlintSide_];
        set4f(progComp_, "uRingGlint", C.x, C.y, ang, strength);
        set2f(progComp_, "uRingR", design_.lay.R - design_.lay.ringW, design_.lay.R);
    }
    set1f(progComp_, "uSparkle", sparkle);
    {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, geoTex_);
        glActiveTexture(GL_TEXTURE0);
        set1i(progComp_, "uGeo", 5);
        set1f(progComp_, "uShow", show_ ? 1.f : 0.f);
        set1f(progComp_, "uShowK", showK_);
        set1f(progComp_, "uBeat", (float)(beat_ - showStart_));
        auto sweep = [&](const char* n, const Sweep& w) {
            set4f(progComp_, n, (float)(w.beat - showStart_), (float)w.kind, (float)w.from, (float)w.to);
        };
        sweep("uWaveA", swA_);
        sweep("uWaveB", swB_);
        set2f(progComp_, "uWaveSpin", swA_.spin ? 1.f : 0.f, swB_.spin ? 1.f : 0.f);
        const ShowPalette& P = SHOW_PAL[variant_ % 5];
        const GLint loc = glGetUniformLocation(progComp_, "uPal");
        float pal[12];
        for (int i = 0; i < 4; i++) {
            const vec3 c = lin(P.c[i]);
            pal[i * 3] = c.x; pal[i * 3 + 1] = c.y; pal[i * 3 + 2] = c.z;
        }
        glUniform3fv(loc, 4, pal);
        set2f(progComp_, "uPalMat", (float)P.metal, (float)P.other);
        const tiles::Layout& L = design_.lay;
        set4f(progComp_, "uMed", L.med[0].x, L.med[0].y, L.med[1].x, L.med[1].y);
        set3f(progComp_, "uMedR", vec3(L.R, L.R - L.ringW, L.sides ? 1.f : 0.f));
    }
    set1f(progComp_, "uGlintK", 1.f);
    ugl::drawFullscreen();

    // ---- The tiles: shadows on the bed, then the glazed tiles (alpha = the stack, for the bridge).
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    if (!cells_.empty()) {
        glUseProgram(progCell_);
        lightSet(progCell_);
        setMat(progCell_, "uVP", vp_);
        set3f(progCell_, "uKey", vec3(-0.45f, 0.6f, 0.75f));
        set3f(progCell_, "uSinopia", lin(V.sinopia) * 1.6f);
        set1f(progCell_, "uPx", 2.f * viewH_.y / height_);
        glBindVertexArray(cellVao_);
        glBindBuffer(GL_ARRAY_BUFFER, cellVbo_);
        glBufferData(GL_ARRAY_BUFFER, cells_.size() * sizeof(CellInst), cells_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)cells_.size());
    }

    // ---- The fragments in flight.
    fragInst_.clear();
    for (const Frag& fr : frags_) {
        if (time_ < fr.appear || f.bare) continue; // its tile is still cracking (a break: none in flight)
        const tiles::Tessera& t = design_.tess[fr.tess];
        const float u = saturate((time_ - fr.launch) / std::max(0.05f, fr.land - fr.launch));
        const float sEase = 1.f - std::pow(1.f - u, 2.3f);
        const vec2 p = bezier(fr.from, fr.ctrl1, fr.ctrl2, t.center, sEase);
        const float rot = fr.rot0 * std::pow(1.f - sEase, 1.4f);
        const float scale = lerpf(1.75f, 1.f, smoothstepf(0.f, 0.9f, u)) * (1.f + 0.2f * std::sin(PI * sEase));
        const float tumble = fr.spin * (time_ - fr.launch) * (1.f - sEase) * (u > 0.f ? 1.f : 0.f);
        const vec3 c = lerp(fr.col0, fr.renew ? fr.col0 : t.color, smoothstepf(0.25f, 0.85f, u)) * (1.f + 0.4f * slow_);
        FragInst fi;
        fi.a[0] = p.x; fi.a[1] = p.y; fi.a[2] = rot; fi.a[3] = scale;
        for (int k = 0; k < 2; k++) {
            fi.o01[k * 2 + 0] = t.c[k].x - t.center.x;
            fi.o01[k * 2 + 1] = t.c[k].y - t.center.y;
            fi.o23[k * 2 + 0] = t.c[k + 2].x - t.center.x;
            fi.o23[k * 2 + 1] = t.c[k + 2].y - t.center.y;
        }
        const float a = worldK;
        fi.b[0] = c.x; fi.b[1] = c.y; fi.b[2] = c.z; fi.b[3] = a;
        fi.c[0] = (float)t.mat; fi.c[1] = tumble; fi.c[2] = 0.15f * smoothstepf(0.85f, 1.f, u); fi.c[3] = 0.f;
        fragInst_.push_back(fi);
    }
    if (!fragInst_.empty()) {
        glUseProgram(progFrag_);
        setMat(progFrag_, "uVP", vp_);
        glBindVertexArray(fragVao_);
        glBindBuffer(GL_ARRAY_BUFFER, fragVbo_);
        glBufferData(GL_ARRAY_BUFFER, fragInst_.size() * sizeof(FragInst), fragInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)fragInst_.size());
    }

    // ---- Glints and dust.
    {
        const vec3 dust = lin(V.bed) * 0.5f + amb;
        for (const Puff& p : puffs_) {
            if (f.bare) break;
            const float k = p.t / p.life;
            const float rr = p.size * (0.6f + 0.9f * std::sqrt(k));
            sprites_.push_back({{p.p.x, p.p.y, rr, rr * 0.33f}, {dust.x, dust.y, dust.z, 0.5f * (1.f - k) * (1.f - k) * worldK}, {2.f, 0.f, 0.f, 0.f}});
        }
        for (const Sparkle& sp : sparkles_) {
            const float k = std::sin(PI * saturate(sp.t / sp.life));
            const float rr = sp.size * (0.6f + 0.4f * k);
            sprites_.push_back({{sp.p.x, sp.p.y, rr, rr}, {sp.col.x, sp.col.y, sp.col.z, 0.9f * k * worldK}, {1.f, 0.3f, 0.f, 0.f}});
        }
        if (!sprites_.empty()) {
            glUseProgram(progSprite_);
            setMat(progSprite_, "uVP", vp_);
            glBindVertexArray(spriteVao_);
            glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
            glBufferData(GL_ARRAY_BUFFER, sprites_.size() * sizeof(SpriteInst), sprites_.data(), GL_STREAM_DRAW);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)sprites_.size());
            sprites_.clear();
        }
    }
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // ---- Bloom.
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
    set1f(progDown_, "uThreshold", 1.0f);
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

    // ---- The frame.
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
    set1f(progFinal_, "uBloomK", (0.22f + 0.08f * wp + 0.15f * dropEnv_) * worldK);
    set1f(progFinal_, "uExposure", 1.f);
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.hud, f.panels);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_tiles() {
    static const UniverseEntry e{
        "TILES", "a great wall mosaic of tesserae; the pieces are glazed tiles, each cleared row breaks into tesserae set one by one into the mosaic's figures",
        [] { return std::unique_ptr<Universe>(new TilesUniverse()); },
        // Fits most songs; dynamic ones a little more (the gold ablaze on the drop).
        [](const SongLook& l) { return 0.85f + 0.2f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (int i = 0; i < VARIANT_COUNT; i++) v.push_back(VARIANTS[i].name);
            return v;
        }()};
    return e;
}

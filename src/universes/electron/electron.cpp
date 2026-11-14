// ELECTRON: bare silicon dies on a dark substrate, and charges finding them.
//
// A dark, softly textured slab seen from straight above. The well is a hairline frame on it and the pieces' blocks are
// bare dies: the piece's color shimmering, a fine grid of circuitry, a border of gold bond pads. Two terminals sit at
// the edges of the picture, one on the left, one on the right, half out of it: a dark plate with a ring of contact
// pads and a glowing core.
//
// The charges are a real (small) ant-colony model, only drawn abstract: hot points with a short tail. They leave the
// terminals and wander; the ones that touch a cube take a little of its charge (the contact throws sparks, the cube's
// edges light up for a moment) and carry it home, laying a field on the way; the others follow the field, so
// connections form between the terminals and the stack, and they show as glowing conductive traces with a hot
// filament along their middle.
//
// A cleared row's cubes shatter into shards and burst into fragments with sparks; the fragments thrown out of the
// well stay on the substrate until charges fetch them. A DROP section (and it alone) pours charges out of both
// terminals: many more, much faster, the traces brighter, more sparks, the substrate in a deep shade of its current's
// color, and the dies power on: a wave of current runs up the stack every two beats, lighting their circuitry and pads
// in the charges' color as it passes. A chorus is a busy circuit, a break a few slow charges on fading traces. A lock
// scatters the charges around the impact (a hard one sparks there); slow motion slows the circuit; a game over
// shatters the whole stack.
//
// Layout contract: a flat orthographic view at the reference camera's scale on the board plane.
#include "universes/electron/electron.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "layout.hpp"
#include "universes/electron/shaders.hpp"
#include "universes/registry.hpp"

namespace {

// Jura Medium (the song, the section, the celebrations) and Jura Light (labels and numbers), SIL Open Font License,
// see OFL-Jura.txt beside this file.
const unsigned char FONT_SERIF[] = {
#include "universes/electron/Jura-Medium.ttf.inc"
};
const unsigned char FONT_SANS[] = {
#include "universes/electron/Jura-Light.ttf.inc"
};

namespace sh = electron_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

GLuint compile(const char* vs, const char* fs, const char* name) {
    std::string f(fs);
    f.insert(f.find('\n') + 1, sh::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec3 hex(uint32_t h) { return vec3(((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f); }
vec2 v2(float x, float y) { return vec2{x, y}; }
vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
vec2 operator*(vec2 a, float k) { return {a.x * k, a.y * k}; }
float len(vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
float len2(vec2 a) { return a.x * a.x + a.y * a.y; }
vec2 dirOf(float a) { return {std::cos(a), std::sin(a)}; }
float wrapAngle(float a) {
    while (a > PI) a -= TAU;
    while (a < -PI) a += TAU;
    return a;
}

// The substrates, all dark. Colors are sRGB (the whole frame is drawn in display colors). Each has its own seven
// cubes.
struct Variant {
    const char* name;
    uint32_t ground, mottle, panel, trace, core, term, charge, ink, dim, accent, frame, slow;
    float frameA, grain, traceAmt, shadow;
    uint32_t piece[7];
};
const Variant VARIANTS[] = {
    // Graphite: a cold dark grey, blue-white current.
    {"GRAPHITE", 0x16181c, 0x1e2126, 0x121417, 0x2c8cff, 0xdff3ff, 0x0d0f12, 0x7fd4ff, 0xe4e9f0, 0x7b8594, 0x6cc8ff,
     0x5a6575, 0x101a2c, 0.5f, 0.022f, 1.f, 0.55f,
     {0x3fa7c9, 0xe0a83c, 0x9370d8, 0x52b874, 0xd85a5e, 0x5277de, 0xe0843f}},
    // Oxide: dark rusted iron, amber current.
    {"OXIDE", 0x1c1512, 0x261c17, 0x15100e, 0xff8a2a, 0xfff0d6, 0x0f0b09, 0xffbd6e, 0xf3e7dc, 0x96837a, 0xffb25a,
     0x76604f, 0x1e1428, 0.5f, 0.022f, 1.f, 0.55f,
     {0x5fb3c0, 0xe8c25a, 0xa982d0, 0x7dbd6a, 0xe0705a, 0x6f8fd8, 0xf09a50}},
    // Verdigris: a deep green-black like an old board, mint current.
    {"VERDIGRIS", 0x0f1916, 0x15221e, 0x0c1412, 0x22e0a0, 0xe4fff4, 0x080e0c, 0x8affd6, 0xe0f2ea, 0x77908a, 0x5af0c0,
     0x4f6f66, 0x0f1626, 0.5f, 0.022f, 1.f, 0.55f,
     {0x48a8d0, 0xe2b94a, 0xa07ad8, 0xb0d060, 0xe0606a, 0x5f80e0, 0xe89048}},
    // Plasma: dark indigo, violet-white current.
    {"PLASMA", 0x15121f, 0x1d182c, 0x110f19, 0xb060ff, 0xf6e8ff, 0x0c0a12, 0xd8a8ff, 0xece6f6, 0x8a82a0, 0xd090ff,
     0x675d80, 0x0e1530, 0.5f, 0.022f, 1.f, 0.55f,
     {0x4ab4d4, 0xe8b850, 0xc07ad0, 0x5cc08a, 0xe85c78, 0x6a7ce8, 0xf08c5a}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float ANT_L = 0.3f;    // an ant's length unit (board units; the body is ~0.8 of it)
constexpr float CUBE = 0.43f;    // a sugar cube's half size (a cell is 0.5)
constexpr float TAU_FOOD = 9.f; // pheromone evaporation (seconds)
constexpr float TAU_HOME = 8.f;
constexpr int MAX_GRAINS = 1400;

} // namespace

// =====================================================================================================
// GL
// =====================================================================================================

bool ElectronUniverse::init(int width, int height) {
    progGround_ = compile(ugl::FULLSCREEN_VS, sh::GROUND_FS, "electron ground");
    progSprite_ = compile(sh::SPRITE_VS, sh::SPRITE_FS, "electron sprites");
    progShape_ = compile(sh::SHAPE_VS, sh::SHAPE_FS, "electron shapes");
    progVeil_ = compile(ugl::FULLSCREEN_VS, sh::VEIL_FS, "electron veil");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &spriteVao_);
    glGenBuffers(1, &spriteVbo_);
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite), (void*)(sizeof(float) * 4 * i));
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
    glBindVertexArray(0);
    glGenTextures(1, &pherTex_);
    for (auto& row : occ_)
        for (auto& c : row) c = -1;
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

ElectronUniverse::~ElectronUniverse() {
    for (GLuint p : {progGround_, progSprite_, progVeil_, progShape_}) glDeleteProgram(p);
    for (GLuint v : {spriteVao_, shapeVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, spriteVbo_, shapeVbo_}) glDeleteBuffers(1, &b);
    if (pherTex_) glDeleteTextures(1, &pherTex_);
}

void ElectronUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = v2(hw, hh);
    viewC_ = v2(cam.target.x, cam.target.y);
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    px_ = 2.f * hh / height_;
    // the nests: on the left and right edges, half out of the picture, below the HUD columns
    nestR_ = 1.55f;
    nest_[0] = v2(viewC_.x - hw - 0.1f, -4.6f);
    nest_[1] = v2(viewC_.x + hw + 0.1f, -0.9f);
    // the pheromone fields cover the view
    const int gw = (int)std::ceil(2.f * hw / CELL) + 2, gh = (int)std::ceil(2.f * hh / CELL) + 2;
    if (gw != gw_ || gh != gh_) {
        gw_ = gw;
        gh_ = gh;
        food_.assign(gw_ * gh_, 0.f);
        home_.assign(gw_ * gh_, 0.f);
    }
    const int tw = (int)std::ceil(2.f * hw / TCELL) + 2, th = (int)std::ceil(2.f * hh / TCELL) + 2;
    if (tw != tw_ || th != th_) {
        tw_ = tw;
        th_ = th;
        trace_.assign(tw_ * th_, 0.f);
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, tw_, th_, 0, GL_RED, GL_FLOAT, trace_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    g0_ = v2(viewC_.x - hw - CELL, viewC_.y - hh - CELL);
    t0_ = v2(viewC_.x - hw - TCELL, viewC_.y - hh - TCELL);
}

// =====================================================================================================
// Song, staging, events
// =====================================================================================================

void ElectronUniverse::setSong(const SongLook& t, float seconds) {
    int v = 0;
    if (t.variant >= 0) {
        v = t.variant % VARIANT_COUNT;
    } else if (t.hasSong) {
        const int r = (int)((t.seed >> 9) % 100);
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.minor && t.brightness < 0.45f) v = r < 55 ? 1 : 3;      // dark songs: oxide, or plasma
        else if (!t.minor && t.brightness > 0.58f) v = r < 60 ? 0 : 2; // bright ones: graphite, or verdigris
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % (VARIANT_COUNT - 1))) % VARIANT_COUNT;
    }
    variant_ = v;
    rng_ = Rng(t.seed ^ 0xA175A175ull);
    if (seconds <= 0.f) warm_ = false;
}

void ElectronUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level_ = std::clamp(level, 0, 2);
    energy_ = energy;
    lvTarget_ = std::min((float)level_, 1.f); // (the peak: a DROP section's alone, see simulate)
    if (seconds <= 0.1f) lv_ = lvTarget_;
}

// The drop's first downbeat: the charges pour out of both terminals (if it lands in a DROP section; see simulate).
void ElectronUniverse::drop(float strength) { hit_ = std::max(hit_, saturate(strength)); }

void ElectronUniverse::refresh(float strength) {
    const int n = 4 + (int)(10 * saturate(strength));
    pour(0, n);
    pour(1, n);
}

vec2 ElectronUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints ElectronUniverse::hints() const {
    const Variant& V = VARIANTS[variant_];
    TransitionHints h;
    h.background = ugl::toLinear(hex(V.ground));
    h.ink = ugl::toLinear(hex(V.ink));
    h.accent = ugl::toLinear(hex(V.accent));
    h.light = false;
    h.styles = TS_INK | TS_LUMA | TS_REBUILD;
    return h;
}

vec3 ElectronUniverse::pieceColor(int type) const {
    const Variant& V = VARIANTS[variant_];
    type = std::clamp(type, 0, 6);
    return hex(V.piece[type]); // (the same in a DROP: its wave of current runs through them)
}

// Loose grains of a cube: thrown out mostly sideways, along the row (the ones that land outside the well stay there
// for the ants; inside the well they melt away at once).
void ElectronUniverse::burst(float x, float y, int type, int n, float speedK, bool quadClear) {
    const vec3 c = pieceColor(type);
    for (int k = 0; k < n && (int)grains_.size() < MAX_GRAINS; k++) {
        Grain g;
        g.p = v2(x + rng_.range(-0.35f, 0.35f), y + rng_.range(-0.35f, 0.35f));
        float side = x + rng_.range(-3.f, 3.f) > 0.f ? 1.f : -1.f;
        const float a = (side > 0 ? 0.f : PI) + rng_.range(-0.75f, 0.75f) + (quadClear ? rng_.range(-0.6f, 0.6f) : 0.f);
        const float sp = rng_.range(0.6f, 3.4f) * speedK * (rng_.chance(0.25f) ? 2.2f : 1.f);
        g.v = dirOf(a) * sp;
        g.col = c * rng_.range(0.9f, 1.08f);
        g.size = rng_.range(0.035f, 0.07f);
        g.rot = rng_.range(0.f, TAU);
        g.spin = rng_.range(-9.f, 9.f);
        g.life = rng_.range(0.45f, 0.8f);
        g.delay = rng_.range(0.f, 0.18f);
        grains_.push_back(g);
    }
}

// Sparks thrown from a point, mostly along dir (fast, short-lived, slowed by drag), with a flash where they start.
void ElectronUniverse::sparks(vec2 at, vec2 dir, const vec3& col, int n, float speed, float spread, float flash) {
    const float base = std::atan2(dir.y, dir.x);
    for (int k = 0; k < n && (int)sparks_.size() < MAX_SPARKS; k++) {
        Spark s;
        s.p = at;
        const float a = base + rng_.range(-spread, spread);
        s.v = dirOf(a) * (speed * rng_.range(0.45f, 1.25f));
        s.col = col;
        s.life = rng_.range(0.12f, 0.3f);
        s.w = rng_.range(0.009f, 0.016f);
        sparks_.push_back(s);
    }
    if (flash > 0.f && (int)flashes_.size() < 64) flashes_.push_back({at, col, 0.f, 0.12f + 0.05f * flash, 0.16f + 0.14f * flash});
}

void ElectronUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::HardDrop:
        hard_ = true;
        break;
    case GameEvent::Lock: {
        // fresh cubes; the ants around the impact scatter, a hard drop pushes a few grains out from under it
        float minY = 1e9f, sx = 0.f;
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            nib_[c.y][c.x] = 0.f;
            seed_[c.y][c.x] = rng_.range(1.f, 97.f);
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            minY = std::min(minY, p.y);
        }
        int nb = 0;
        for (const auto& c : ev.cells) {
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            if (p.y > minY + 0.1f) continue;
            sx += p.x;
            nb++;
        }
        if (nb) {
            const vec2 at = v2(sx / nb, minY - 0.5f);
            lockAt_.push_back(at);
            if (hard_) {
                // a hard landing: a short spray of sparks out from under the piece, to both sides
                const vec3 col = lerp(hex(VARIANTS[variant_].charge), vec3(1.f), 0.3f);
                const float half = nb * 0.5f;
                sparks(v2(at.x - half, at.y + 0.02f), v2(-1.f, -0.25f), col, 4, 4.5f, 0.45f, 0.f);
                sparks(v2(at.x + half, at.y + 0.02f), v2(1.f, -0.25f), col, 4, 4.5f, 0.45f, 0.f);
            }
        }
        hard_ = false;
        break;
    }
    case GameEvent::Clear: {
        bool cleared[Game::H] = {};
        float sy = 0.f;
        int n = 0;
        for (const auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            cleared[c.y] = true;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Dying d{p.x, p.y, std::clamp(c.type, 0, 6), 0.f, std::fabs(c.x - 4.5f) * 0.012f, seed_[c.y][c.x],
                    nib_[c.y][c.x], false};
            if (d.seed <= 0.f) d.seed = rng_.range(1.f, 97.f);
            dying_.push_back(d);
            burst(p.x, p.y, d.type, ev.count >= 4 ? 9 : 7, ev.count >= 4 ? 1.35f : 1.f, ev.count >= 4);
            {
                const float side = p.x + rng_.range(-2.f, 2.f) > 0.f ? 1.f : -1.f;
                sparks(v2(p.x, p.y), v2(side, rng_.range(-0.5f, 0.5f)), lerp(pieceColor(d.type), vec3(1.f), 0.5f),
                       ev.count >= 4 ? 2 : 1, 7.f, 0.6f, 0.f);
            }
            sy += p.y;
            n++;
        }
        (void)sy;
        if (ev.count >= 4) {
            pour(0, 25);
            pour(1, 25);
        }
        // the nibbles follow the cubes down
        float nn[Game::H][Game::W] = {}, ss[Game::H][Game::W] = {};
        int dst = Game::H - 1;
        for (int y = Game::H - 1; y >= 0; y--) {
            if (cleared[y]) continue;
            for (int x = 0; x < Game::W; x++) {
                nn[dst][x] = nib_[y][x];
                ss[dst][x] = seed_[y][x];
            }
            dst--;
        }
        std::memcpy(nib_, nn, sizeof(nib_));
        std::memcpy(seed_, ss, sizeof(seed_));
        break;
    }
    case GameEvent::TopOut: {
        // the whole stack crumbles, from the top down
        int top = Game::H;
        for (const auto& c : ev.cells) top = std::min(top, c.y);
        for (const auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Dying d{p.x, p.y, std::clamp(c.type, 0, 6), 0.f, (c.y - top) * 0.07f + rng_.range(0.f, 0.06f),
                    seed_[c.y][c.x] > 0.f ? seed_[c.y][c.x] : rng_.range(1.f, 97.f), nib_[c.y][c.x], true};
            dying_.push_back(d);
        }
        for (auto& row : nib_)
            for (float& v : row) v = 0.f;
        overT_ = 0.f;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// The colony
// =====================================================================================================

bool ElectronUniverse::sugarAt(vec2 p, float half, int* gx, int* gy) const {
    const int cx = (int)std::floor(p.x + 5.f), cy = (int)std::floor(12.f - p.y); // the cell under p
    for (int y = cy - 1; y <= cy + 1; y++) {
        if (y < Game::HIDDEN || y >= Game::H) continue;
        for (int x = cx - 1; x <= cx + 1; x++) {
            if (x < 0 || x >= Game::W || occ_[y][x] < 0) continue;
            const float ccx = x - 4.5f, ccy = 11.5f - y;
            if (std::fabs(p.x - ccx) < half && std::fabs(p.y - ccy) < half) {
                if (gx) *gx = x;
                if (gy) *gy = y;
                return true;
            }
        }
    }
    return false;
}

bool ElectronUniverse::blocked(vec2 p) const {
    const float x0 = viewC_.x - viewH_.x + 0.15f, x1 = viewC_.x + viewH_.x - 0.15f;
    const float y0 = viewC_.y - viewH_.y + 0.15f, y1 = viewC_.y + viewH_.y - 0.15f;
    if (p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) {
        const float r = nestR_ * 1.1f;
        if (len2(p - nest_[0]) > r * r && len2(p - nest_[1]) > r * r) return true;
    }
    // the HUD columns stay quiet
    if (p.x > layout::LEFT_COLUMN[0] - 0.2f && p.x < layout::LEFT_COLUMN[2] + 0.2f && p.y > layout::LEFT_COLUMN[1] - 1.2f &&
        p.y < layout::LEFT_COLUMN[3] + 0.4f)
        return true;
    if (p.x > layout::RIGHT_COLUMN[0] - 0.2f && p.x < layout::RIGHT_COLUMN[2] + 0.2f && p.y > layout::RIGHT_COLUMN[1] - 1.6f &&
        p.y < layout::RIGHT_COLUMN[3] + 0.4f)
        return true;
    // inside the well (and the lanes beside it, between the HUD columns), only just above the stack
    if (std::fabs(p.x) < 6.f && p.y > openTop_) return true;
    // nor over the HUD columns' tops (the ants keep to the lower part of the picture between the nests' sides)
    if (p.x > layout::LEFT_COLUMN[0] - 0.2f && p.x <= -6.f && p.y > layout::LEFT_COLUMN[1] - 1.2f) return true;
    if (p.x < layout::RIGHT_COLUMN[2] + 0.2f && p.x >= 6.f && p.y > layout::RIGHT_COLUMN[1] - 1.6f) return true;
    return sugarAt(p, 0.56f);
}

float ElectronUniverse::pher(const std::vector<float>& f, vec2 p) const {
    const float fx = (p.x - g0_.x) / CELL - 0.5f, fy = (p.y - g0_.y) / CELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= gw_ || iy + 1 >= gh_) return 0.f;
    const float tx = fx - ix, ty = fy - iy;
    const float* r0 = &f[iy * gw_ + ix];
    const float* r1 = r0 + gw_;
    return lerpf(lerpf(r0[0], r0[1], tx), lerpf(r1[0], r1[1], tx), ty);
}

void ElectronUniverse::deposit(std::vector<float>& f, vec2 p, float amount) {
    const float fx = (p.x - g0_.x) / CELL - 0.5f, fy = (p.y - g0_.y) / CELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= gw_ || iy + 1 >= gh_) return;
    const float tx = fx - ix, ty = fy - iy;
    float* r0 = &f[iy * gw_ + ix];
    float* r1 = r0 + gw_;
    r0[0] = std::min(3.f, r0[0] + amount * (1 - tx) * (1 - ty));
    r0[1] = std::min(3.f, r0[1] + amount * tx * (1 - ty));
    r1[0] = std::min(3.f, r1[0] + amount * (1 - tx) * ty);
    r1[1] = std::min(3.f, r1[1] + amount * tx * ty);
}

void ElectronUniverse::wear(vec2 p, float amount) {
    const float fx = (p.x - t0_.x) / TCELL - 0.5f, fy = (p.y - t0_.y) / TCELL - 0.5f;
    const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
    if (ix < 0 || iy < 0 || ix + 1 >= tw_ || iy + 1 >= th_) return;
    const float tx = fx - ix, ty = fy - iy;
    float* r0 = &trace_[iy * tw_ + ix];
    float* r1 = r0 + tw_;
    r0[0] = std::min(1.2f, r0[0] + amount * (1 - tx) * (1 - ty));
    r0[1] = std::min(1.2f, r0[1] + amount * tx * (1 - ty));
    r1[0] = std::min(1.2f, r1[0] + amount * (1 - tx) * ty);
    r1[1] = std::min(1.2f, r1[1] + amount * tx * ty);
}

// A new ant leaves the nest in a random direction. A recruit (called out by a forager that brought sugar home)
// follows trails faithfully; a wanderer explores, half-heeding what it smells.
void ElectronUniverse::spawn(int nest, bool recruit) {
    Ant a;
    const vec2 c = nest_[nest];
    const float toward = nest == 0 ? 0.f : PI;
    const float ang = toward + rng_.range(-1.f, 1.f) * 1.35f;
    a.p = c + dirOf(ang) * rng_.range(0.f, nestR_ * 0.35f);
    a.a = ang + rng_.range(-0.5f, 0.5f);
    a.speed = rng_.range(0.85f, 1.15f);
    a.gait = rng_.range(0.f, TAU);
    a.nest = nest;
    a.lustre = rng_.range(0.08f, 0.2f);
    a.fid = recruit ? rng_.range(0.85f, 1.f) : rng_.range(0.3f, 0.75f);
    a.life = recruit ? rng_.range(40.f, 80.f) : rng_.range(35.f, 110.f);
    ants_.push_back(a);
}

void ElectronUniverse::syncCells(const Game& game) {
    float top = -10.f;
    int appeared = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = game.cell(x, y);
            const int8_t was = occ_[y][x];
            occ_[y][x] = (int8_t)(y >= Game::HIDDEN ? c.type : -1);
            if (was < 0 && occ_[y][x] >= 0) appeared++;
            if (c.type < 0) {
                nib_[y][x] = 0.f;
                seed_[y][x] = 0.f;
            } else {
                if (seed_[y][x] <= 0.f) seed_[y][x] = rng_.range(1.f, 97.f);
                if (y >= Game::HIDDEN) top = std::max(top, 11.5f - y + 0.5f);
            }
        }
    // a whole stack at once (a scene review's board): the colony has had time to find it
    if (appeared > 24) warm_ = false;
    openTop_ = top + 1.25f;
}

// The colony, as a classic ant-colony model: no ant knows where the sugar is. Each one only feels the two
// pheromones under its three antennae-sensors (ahead-left, ahead, ahead-right) and the sugar right in front of it.
// Searching ants lay "home" pheromone and climb the "food" one; ants carrying a crumb lay "food" pheromone (weaker
// the further they have walked from the sugar) and climb the "home" one, helped by their sense of the nest's
// direction. Both fields diffuse and evaporate, so the trails that are walked most (the shorter ones) win.
void ElectronUniverse::stepAnts(float dt, float speedK, float explore, float layK) {
    const float sense = 0.5f, sideA = 0.55f;
    const float nestIn = nestR_ * 0.6f;
    for (size_t i = 0; i < ants_.size(); i++) {
        Ant& a = ants_[i];
        a.alpha = std::min(1.f, a.alpha + dt * 2.f);
        a.panic = std::max(0.f, a.panic - dt);
        a.meetCd -= dt;
        a.stray -= dt;
        // stuck inside a cube that just landed: out to the nearest free side, or gone
        if (sugarAt(a.p, 0.5f)) {
            bool out = false;
            const vec2 dirs[4] = {v2(0, 1), v2(-1, 0), v2(1, 0), v2(0, -1)};
            const int cx = (int)std::floor(a.p.x + 5.f), cy = (int)std::floor(12.f - a.p.y);
            const vec2 cc = v2(cx - 4.5f, 11.5f - cy);
            for (const vec2& d : dirs) {
                const vec2 q = cc + d * 0.62f;
                if (!blocked(q)) { a.p = q; a.a = std::atan2(d.y, d.x); out = true; break; }
            }
            if (!out) { a.alpha = -1.f; continue; }
        }
        // stopped, antennae touching another's
        if (a.pause > 0.f) {
            a.pause -= dt;
            continue;
        }
        a.since += dt;
        const vec2 nest = nest_[a.nest];
        const bool homing = a.state != 0 || a.retire;
        // a correlated random walk
        a.w += rng_.range(-1.f, 1.f) * (homing ? 9.f : 13.f) * explore * dt;
        a.w *= std::exp(-3.f * dt);
        a.w = clampf(a.w, -2.6f, 2.6f);
        float turn = a.w;
        // the pheromone under the three sensors
        const std::vector<float>& F = homing ? home_ : food_;
        const float l = pher(F, a.p + dirOf(a.a + sideA) * sense), c = pher(F, a.p + dirOf(a.a) * sense),
                    r = pher(F, a.p + dirOf(a.a - sideA) * sense);
        const float mx = std::max(c, std::max(l, r));
        a.trail = mx > 0.03f && a.stray <= 0.f;
        if (a.trail) {
            const float k = homing ? 1.f : a.fid;
            turn *= 1.f - 0.6f * k;
            if (c >= l && c >= r) turn += (l - r) / (l + r + c + 1e-3f) * 2.5f * k;
            else turn += (l > r ? 1.f : -1.f) * 3.8f * k;
            // now and then one wanders off the trail: a branch, that dies out unless it leads somewhere
            if (!homing && rng_.chance(dt * 0.04f * explore)) a.stray = rng_.range(1.f, 3.f);
        }
        if (homing) {
            // the sense of the nest's direction (around the HUD columns: under them)
            const float side = a.p.x < 0.f ? -1.f : 1.f;
            const vec2 corner = side < 0.f ? v2(layout::LEFT_COLUMN[0] - 0.6f, layout::LEFT_COLUMN[1] - 1.9f)
                                           : v2(layout::RIGHT_COLUMN[2] + 0.6f, layout::RIGHT_COLUMN[1] - 2.3f);
            const bool under = std::fabs(a.p.x) < std::fabs(corner.x) - 0.2f && a.p.y > corner.y - 0.5f && a.p.y < 10.8f;
            const vec2 g = under ? corner : nest;
            const float dn = len(nest - a.p);
            const float kN = (a.trail ? 0.5f : 1.5f) + 7.f / (dn + 0.5f);
            turn += clampf(wrapAngle(std::atan2(g.y - a.p.y, g.x - a.p.x) - a.a), -1.f, 1.f) * kN;
        } else {
            // the antennae touch sugar just ahead: a cube, or a loose grain
            int gx = 0, gy = 0;
            const vec2 ahead = a.p + dirOf(a.a) * 0.32f;
            if (sugarAt(ahead, CUBE + 0.14f, &gx, &gy)) {
                const vec2 q = v2(gx - 4.5f, 11.5f - gy);
                turn += clampf(wrapAngle(std::atan2(q.y - a.p.y, q.x - a.p.x) - a.a), -1.f, 1.f) * 5.f;
            } else {
                for (const Grain& g : grains_)
                    if (g.food && len2(g.p - ahead) < 0.16f) {
                        turn += clampf(wrapAngle(std::atan2(g.p.y - a.p.y, g.p.x - a.p.x) - a.a), -1.f, 1.f) * 5.f;
                        break;
                    }
            }
        }
        if (a.panic > 0.f) turn = a.w * 0.2f;
        // left standing where it may not be (a clear lowered the stack's top under it), or boxed in with no free
        // step for a while: it walks through, home
        const bool trapped = a.stuck > 0.5f || blocked(a.p);
        if (trapped) turn = clampf(wrapAngle(std::atan2(nest.y - a.p.y, nest.x - a.p.x) - a.a), -1.f, 1.f) * 6.f;
        a.a = wrapAngle(a.a + turn * dt);
        const float sp = a.speed * speedK * (a.panic > 0.f ? 1.6f + a.panic * 2.f : 1.f);
        vec2 np = a.p + dirOf(a.a) * (sp * dt);
        if (trapped) {
            if (!blocked(np)) a.stuck = 0.f;
        } else if (blocked(np)) {
            // feel along the obstacle: the closest free heading
            bool ok = false;
            const float sgn = rng_.chance(0.5f) ? 1.f : -1.f;
            for (float t : {0.45f, 0.9f, 1.4f, 2.0f, 2.6f}) {
                for (float s : {sgn, -sgn}) {
                    const float na = a.a + s * t;
                    const vec2 q = a.p + dirOf(na) * (sp * dt);
                    if (!blocked(q)) { a.a = wrapAngle(na); np = q; ok = true; break; }
                }
                if (ok) break;
            }
            if (!ok) { a.a = wrapAngle(a.a + PI); np = a.p; a.stuck += dt; }
            else a.stuck = 0.f;
            a.w *= 0.3f;
        }
        const float moved = len(np - a.p);
        a.p = np;
        a.dist += moved;
        a.gait += moved / (ANT_L * 0.42f) * PI;
        // the pheromones, and the paths worn on the ground (by the foragers)
        if (a.state == 1) {
            deposit(food_, a.p, dt * layK * 8.f * std::exp(-a.dist / 20.f));
            wear(a.p, dt * 7.f);
        } else if (a.state == 0 && !a.retire) {
            deposit(home_, a.p, dt * layK * 2.4f * std::exp(-a.dist / 25.f));
            if (a.trail) wear(a.p, dt * 1.8f);
        }
        // sugar under the jaws: a crumb, and back home
        if (a.state == 0 && !a.retire) {
            const vec2 head = a.p + dirOf(a.a) * (ANT_L * 0.42f);
            int gx = 0, gy = 0;
            bool found = false;
            if (sugarAt(head, CUBE + 0.07f, &gx, &gy)) {
                const vec3 cc = pieceColor(occ_[gy][gx]);
                a.crumb = lerp(cc, vec3(1.f), 0.35f);
                nib_[gy][gx] = std::min(1.f, nib_[gy][gx] + 0.55f);
                found = true;
                // the connection: sparks off the contact, back toward the charge (a few contacts a second at most)
                if (sparkTok_ >= 1.f) {
                    sparkTok_ -= 1.f;
                    const vec3 hot = lerp(hex(VARIANTS[variant_].charge), cc, 0.35f);
                    sparks(head, dirOf(a.a + PI), hot, 3 + (int)rng_.range(0.f, 2.99f) + (int)(2.f * dropMix_),
                           2.6f + 1.4f * dropMix_, 1.0f, 0.6f + 0.4f * dropMix_);
                }
            } else {
                for (Grain& g : grains_)
                    if (g.food && g.life > 0.f && len2(g.p - head) < 0.03f) {
                        a.crumb = g.col;
                        g.life = -1.f;
                        g.food = false;
                        found = true;
                        break;
                    }
            }
            if (found) {
                a.state = 1;
                a.since = 0.f;
                a.dist = 0.f;
                a.stray = 0.f;
                a.a = wrapAngle(a.a + PI + rng_.range(-0.5f, 0.5f));
                a.w = 0.f;
            } else if (a.since > a.life) {
                a.state = 2; // tired of searching: home
            }
        }
        // home
        if (homing && len2(a.p - nest) < nestIn * nestIn) {
            if (a.retire || a.state == 2) { a.alpha = -1.f; continue; }
            // a crumb brought home calls others out (recruitment), and the forager goes back out on its trail
            recruits_[a.nest] += 1.2f + 2.f * dropMix_;
            a.state = 0;
            a.since = 0.f;
            a.dist = 0.f;
            a.fid = std::max(a.fid, 0.9f);
            a.life = rng_.range(40.f, 80.f);
            a.a = wrapAngle(a.a + PI + rng_.range(-0.6f, 0.6f));
        }
    }
    ants_.erase(std::remove_if(ants_.begin(), ants_.end(), [](const Ant& a) { return a.alpha < 0.f; }), ants_.end());

    // ants meeting head-on on a trail stop and touch antennae; a searcher touched by a forager carrying sugar
    // takes to the trail
    const float HC = 0.5f;
    const int hw = (int)std::ceil(2.f * viewH_.x / HC) + 8, hh = (int)std::ceil(2.f * viewH_.y / HC) + 8;
    const vec2 h0 = v2(viewC_.x - viewH_.x - 2.f * HC, viewC_.y - viewH_.y - 2.f * HC);
    bucket_.assign(hw * hh, -1);
    chain_.assign(ants_.size(), -1);
    auto cellOf = [&](vec2 p, int& x, int& y) {
        x = std::clamp((int)((p.x - h0.x) / HC), 0, hw - 1);
        y = std::clamp((int)((p.y - h0.y) / HC), 0, hh - 1);
    };
    for (size_t i = 0; i < ants_.size(); i++) {
        int x, y;
        cellOf(ants_[i].p, x, y);
        chain_[i] = bucket_[y * hw + x];
        bucket_[y * hw + x] = (int)i;
    }
    for (size_t i = 0; i < ants_.size(); i++) {
        Ant& a = ants_[i];
        if (a.meetCd > 0.f || a.pause > 0.f || a.panic > 0.f || a.alpha < 0.5f) continue;
        if (a.state != 1 && !a.trail) continue;
        int x, y;
        cellOf(a.p, x, y);
        for (int yy = std::max(0, y - 1); yy <= std::min(hh - 1, y + 1) && a.pause <= 0.f; yy++)
            for (int xx = std::max(0, x - 1); xx <= std::min(hw - 1, x + 1) && a.pause <= 0.f; xx++)
                for (int j = bucket_[yy * hw + xx]; j >= 0; j = chain_[j]) {
                    if ((size_t)j == i) continue;
                    Ant& b = ants_[j];
                    if (b.meetCd > 0.f || b.pause > 0.f || b.panic > 0.f || b.state == 2) continue;
                    if (len2(b.p - a.p) > 0.3f * 0.3f) continue;
                    if (std::cos(b.a - a.a) > -0.55f) continue; // head-on only
                    const float t = rng_.range(0.3f, 0.65f);
                    a.pause = b.pause = t;
                    a.meetCd = rng_.range(2.f, 4.f);
                    b.meetCd = rng_.range(2.f, 4.f);
                    // face each other
                    a.a = std::atan2(b.p.y - a.p.y, b.p.x - a.p.x);
                    b.a = wrapAngle(a.a + PI);
                    Ant* searcher = a.state == 1 && b.state == 0 ? &b : b.state == 1 && a.state == 0 ? &a : nullptr;
                    if (searcher) {
                        searcher->fid = 1.f;
                        searcher->stray = 0.f;
                        searcher->life = std::max(searcher->life, searcher->since + 30.f);
                    }
                    break;
                }
    }
}

// The fields spread a little and fade.
void ElectronUniverse::relax(std::vector<float>& f, float tau, float dt) {
    const float ke = std::exp(-dt / tau);
    const float kd = 0.5f * (1.f - std::exp(-dt * 2.f));
    tmp_.resize(f.size());
    for (int y = 0; y < gh_; y++)
        for (int x = 0; x < gw_; x++) {
            const int i = y * gw_ + x;
            if (x == 0 || y == 0 || x == gw_ - 1 || y == gh_ - 1) { tmp_[i] = f[i] * ke; continue; }
            const float avg = 0.25f * (f[i - 1] + f[i + 1] + f[i - gw_] + f[i + gw_]);
            tmp_[i] = (f[i] + kd * (avg - f[i])) * ke;
        }
    f.swap(tmp_);
}

void ElectronUniverse::simulate(const Game& game, float wdt, float dt, const MusicState& music) {
    (void)game;
    (void)dt;
    // the staging: a DROP section alone floods the circuit (a chorus stays a busy circuit, the level below)
    inDrop_ = music.inDrop;
    if (inDrop_) lvTarget_ = 2.f;
    else lvTarget_ = std::min((float)level_, 1.f);
    if (hit_ > 0.f && inDrop_) {
        swell_ = std::max(swell_, 0.6f + 0.4f * hit_);
        const int nh = 40 + (int)(40 * hit_);
        pour(0, nh);
        pour(1, nh);
    }
    hit_ = 0.f;
    lv_ = approach(lv_, lvTarget_, 0.9f, dt);
    dropMix_ = approach(dropMix_, inDrop_ ? 1.f : 0.f, inDrop_ ? 5.f : 2.f, dt); // (gone by the section's end)
    swell_ = approach(swell_, 0.f, 0.35f, dt);
    const float pm = smoothstepf(0.f, 1.f, lv_), pk = smoothstepf(1.f, 2.f, lv_);
    const float kick = music.hasSong ? music.kick : 0.f;
    const float target = 75.f + 35.f * pm + 45.f * pk + 110.f * dropMix_;
    const float speedK = (1.25f + 0.3f * pm + 0.35f * pk + 1.05f * dropMix_) * (1.f + 0.4f * kick * dropMix_);
    const float explore = 1.f + 0.3f * pk + 0.6f * dropMix_;
    const float layK = 1.f + 0.4f * pk + 1.f * dropMix_ + 0.5f * swell_;

    // the nests: wanderers keep going out, a few at a time; recruits follow the foragers' news; a stream when the
    // colony pours out
    const float n = (float)ants_.size();
    const float recruitRate = 2.f + 3.f * pk + 8.f * dropMix_;
    for (int k = 0; k < 2; k++) {
        if (pourLeft_[k] > 0) {
            spawnAcc_[k] += wdt * 24.f;
            while (spawnAcc_[k] >= 1.f && pourLeft_[k] > 0) {
                spawn(k, rng_.chance(0.6f));
                pourLeft_[k]--;
                spawnAcc_[k] -= 1.f;
            }
        } else {
            spawnAcc_[k] = 0.f;
        }
        if (n < target) {
            wanderAcc_[k] += wdt * 2.5f;
            if (wanderAcc_[k] >= 1.f) { spawn(k, false); wanderAcc_[k] -= 1.f; }
        }
        if (recruits_[k] >= 1.f && n < target * 1.6f) {
            recruitAcc_[k] += wdt * recruitRate;
            while (recruitAcc_[k] >= 1.f && recruits_[k] >= 1.f) {
                spawn(k, true);
                recruits_[k] -= 1.f;
                recruitAcc_[k] -= 1.f;
            }
        } else {
            recruitAcc_[k] = 0.f;
        }
        recruits_[k] = std::min(recruits_[k], 30.f) * std::exp(-wdt / 25.f);
    }
    // too many: some go back in
    {
        int excess = (int)ants_.size() - (int)(target * 1.7f) - 6;
        for (size_t i = 0; i < ants_.size() && excess > 0; i++) {
            Ant& a = ants_[(i * 7919 + (size_t)(clock_ * 13.f)) % ants_.size()];
            if (a.retire || a.state == 1) continue;
            if (rng_.chance(wdt * 0.6f)) a.retire = true;
            excess--;
        }
    }
    // scattered by impacts
    for (const vec2& at : lockAt_)
        for (Ant& a : ants_) {
            const float d2 = len2(a.p - at);
            if (d2 < 2.6f) {
                a.a = std::atan2(a.p.y - at.y, a.p.x - at.x) + rng_.range(-0.4f, 0.4f);
                a.panic = 0.35f + 0.3f * (1.f - d2 / 2.6f);
                a.pause = 0.f;
            }
        }
    lockAt_.clear();
    // the budget of sparking contacts (more at a peak, many more in a drop)
    sparkTok_ = std::min(4.f, sparkTok_ + wdt * (5.f + 4.f * pk + 12.f * dropMix_));
    // a cube's charge drains away after a touch
    {
        const float kc = std::exp(-wdt * 2.2f);
        for (auto& row : nib_)
            for (float& v : row) v *= kc;
    }

    stepAnts(wdt, speedK, explore, layK);

    relax(food_, TAU_FOOD, wdt);
    relax(home_, TAU_HOME, wdt);
    // the traces fade fast once no one walks them (a route left behind is gone in ~6 s)
    const float kt = std::exp(-wdt / 2.4f);
    for (float& v : trace_) v *= kt;
}

void ElectronUniverse::warmUp(const Game& game, bool bare) {
    // the colony has been there a while: its trails already lead to the stack
    MusicState quiet;
    if (!bare) syncCells(game);
    for (int i = 0; i < 900; i++) simulate(game, 0.05f, 0.05f, quiet);
    sparks_.clear();
    flashes_.clear();
    warm_ = true;
}

// =====================================================================================================
// The HUD
// =====================================================================================================

// The panels under the HUD text (the text itself: hudLine, lettered by the host).
void ElectronUniverse::drawPanels(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    const Variant& V = VARIANTS[variant_];
    std::vector<float> v;
    for (const HudRect& r : panels) {
        const vec3 c = r.custom ? r.color : r.accent ? hex(V.accent) : hex(V.ground) * 0.8f;
        const float a = r.accent || r.custom ? r.alpha : std::min(1.f, r.alpha * 1.1f);
        const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
        const float q[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
        for (auto& p : q) v.insert(v.end(), {p[0], p[1], c.x, c.y, c.z, a});
    }
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 6));
}

// The HUD as an instrument panel: Jura Medium for the song, the section and the celebrations, small spaced capitals
// of Jura Light for the labels, light numbers; each line on a halo of the substrate's own color. The inks turn to
// the bridge's, and the lines dim with the world under the veil (startup, pause, game over).
void ElectronUniverse::hudLine(const HudText& h, text::Line& out) {
    const Variant& V = VARIANTS[variant_];
    text::Font* medium = text::font(FONT_SERIF);
    text::Font* light = text::font(FONT_SANS);
    const float s = height_ / 1080.f, base = h.scale * 12.f, alpha = saturate(h.alpha);
    const float b = smoothstepf(0.f, 1.f, bridge_.amount);
    auto ink = [&](const vec3& c) { return lerp(c, ugl::toSrgb(bridge_.ink), b); };
    const vec3 inkC = hex(V.ink), dim = hex(V.dim), accent = hex(V.accent);
    float bias = 0.08f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, ink(c), track / size, saturate(a));
        out.dy = dy;
        if (f == medium) bias = 0.04f;
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 1.3f, vsz = base * 0.62f, a = 0.35f + h.alpha * 1.6f;
        put(medium, text::upper(name), sz, sz * 0.22f, inkC, a, -base * 0.06f);
        if (!var.empty()) {
            text::Span& v = out.add(var, light, vsz, ink(accent), 0.42f, saturate(a));
            v.gap = (base * 0.8f - sz * 0.22f) / vsz; // the gap (less the name's tracking after it)
            v.dy = base * 0.32f + 0.535f * vsz - 0.554f * sz; // its own cap line, lower
        }
        bias = 0.05f;
        break;
    }
    case HUD_SECTION: put(medium, text::titleCase(h.text), base * 1.4f, 0.f, accent, 0.2f + h.alpha * 1.9f); break;
    case HUD_TITLE: put(medium, h.text, base * 1.3f, 0.f, inkC, h.alpha * 1.3f, -base * 0.14f); break;
    case HUD_INFO: put(light, text::upper(h.text), base * 0.66f, base * 0.32f, dim, 0.3f + alpha * 2.f); bias = 0.1f; break;
    case HUD_LABEL: put(light, h.text, base * 0.7f, base * 0.42f, dim, alpha * 1.3f); break;
    case HUD_VALUE: put(light, h.text, base * 1.12f, base * 0.06f, inkC, alpha); bias = 0.07f; break;
    case HUD_NOTICE: put(medium, h.text, base * 1.2f, 0.f, accent, alpha); break;
    case HUD_KEY: put(light, h.text, base * 0.85f, base * 0.1f, inkC, alpha); break;
    case HUD_TOAST: put(medium, text::titleCase(h.text), base * 1.55f, 0.f, inkC, alpha, -base * 0.15f); break;
    case HUD_TOAST_SUB: put(light, h.text, base * 0.8f, base * 0.3f, accent, alpha * 0.9f); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(medium, text::titleCase(h.text), base * 1.35f, 0.f, inkC, alpha, -base * 0.1f);
        else put(light, h.text, base * 0.82f, base * 0.16f, h.accent ? accent : inkC, alpha);
        break;
    default: put(light, h.text, base * 0.8f, base * 0.12f, h.accent ? accent : inkC, alpha); break;
    }
    // (the old field: 10 px of distance at 64 px, one unit of it 0.31 em; its halo from 0.12, squared; its wide
    // edge left the small letters a little heavier)
    text::Look& k = out.look;
    k.weight = bias * 0.31f + 0.2f / std::max(1.f, out.spans[0].size);
    k.halo = lerp(hex(V.ground), ugl::toSrgb(bridge_.color), b);
    k.haloA = 0.55f;
    k.haloW = 0.118f;
    k.haloPow = 2.f;
    out.alpha = 1.f - veil_;
}

// =====================================================================================================
// Frame
// =====================================================================================================

void ElectronUniverse::cube(std::vector<Sprite>& out, float x, float y, float scale, const vec3& col, float alpha, float nib,
                        float seed, float glint, bool ghost, float crumbled) {
    const float h = 0.5f * scale;
    out.push_back({{x, y, h, h}, {0.f, 0.f, seed, alpha}, {col.x, col.y, col.z, nib}, {CUBE * scale, glint, ghost ? 1.f : 0.f, crumbled}});
}

void ElectronUniverse::shadow(std::vector<Sprite>& out, float x, float y, float half, float blur, float alpha) {
    const float h = half + blur * 1.5f;
    out.push_back({{x, y, h, h}, {0.f, 1.f, 0.f, alpha}, {0.f, 0.f, 0.f, 0.f}, {half, blur, 0.f, 0.f}});
}

void ElectronUniverse::drawSprites(const std::vector<Sprite>& v) {
    if (v.empty()) return;
    glBindVertexArray(spriteVao_);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVbo_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Sprite), v.data(), GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)v.size());
}

void ElectronUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = f.dt;
    const Variant& V = VARIANTS[variant_];
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 2.f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.55f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    // (a break's blank board is not the stack: the colony keeps the cubes it knows; but it has been there a while from
    // the first frame on, a song's title card included: warmed up then, it never fills up all at once as play starts)
    if (!f.bare) syncCells(game);
    if (!warm_) warmUp(game, f.bare);
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.3f, slow_);
    if (!f.paused) {
        clock_ += wdt;
        simulate(game, wdt, dt, music);
        // grains: fly, settle; inside the well they melt away, outside they wait for the ants
        for (Grain& g : grains_) {
            g.t += dt; // real time: a clear is never slowed
            if (g.t < g.delay) continue;
            const float k = g.food ? wdt : dt;
            g.p = g.p + g.v * k;
            g.v = g.v * std::exp(-5.5f * k);
            g.rot += g.spin * k;
            g.spin *= std::exp(-4.f * k);
            if (!g.food && g.t > g.delay + 0.3f && len2(g.v) < 0.04f) {
                const bool inWell = std::fabs(g.p.x) < 5.15f && g.p.y > -10.15f && g.p.y < 10.f;
                if (!inWell && !blocked(g.p) && g.life > 0.f && g.size > 0.04f) {
                    g.food = true;
                    g.life = 30.f;
                    g.t = g.delay;
                }
            }
        }
        grains_.erase(std::remove_if(grains_.begin(), grains_.end(),
                                     [](const Grain& g) { return g.life < 0.f || g.t > g.delay + g.life; }),
                      grains_.end());
        // sparks and flashes: real time
        for (Spark& sp : sparks_) {
            sp.t += dt;
            sp.p = sp.p + sp.v * dt;
            sp.v = sp.v * std::exp(-7.f * dt);
        }
        sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& sp) { return sp.t > sp.life; }),
                      sparks_.end());
        for (Flash& fl : flashes_) fl.t += dt;
        flashes_.erase(std::remove_if(flashes_.begin(), flashes_.end(), [](const Flash& fl) { return fl.t > fl.life; }),
                       flashes_.end());
        for (Dying& d : dying_) d.t += dt;
        dying_.erase(std::remove_if(dying_.begin(), dying_.end(),
                                    [](const Dying& d) { return d.t > d.delay + (d.over ? 1.4f : 0.5f); }),
                     dying_.end());
        gaugeShown_ = approach(gaugeShown_, game.bonusGauge(), 3.f, dt);
        // the trails, as the ground shows them
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw_, th_, GL_RED, GL_FLOAT, trace_.data());
    }

    const float b = smoothstepf(0.f, 1.f, bridge_.amount);
    const float world = 1.f - b;
    const vec3 bridgeCol = ugl::toSrgb(bridge_.color), bridgeInk = ugl::toSrgb(bridge_.ink);
    const float pm = smoothstepf(0.f, 1.f, lv_), pk = smoothstepf(1.f, 2.f, lv_);

    // ---- build the sprites
    under_.clear();
    stack_.clear();
    antsV_.clear();
    over_.clear();
    top_.clear();
    const float shA = V.shadow;
    const float glint = 0.25f * pk + 0.5f * dropMix_;
    // a break: the substrate, its terminals, traces and charges alone (no cubes, fragments, sparks nor gauge)
    const bool gameOn = !f.bare;
    // the stack
    for (int y = Game::HIDDEN; y < Game::H && gameOn; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = game.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - game.rowOffset(y));
            shadow(under_, p.x + 0.07f, p.y - 0.1f, CUBE, 0.08f, shA);
            shadow(under_, p.x + 0.015f, p.y - 0.06f, CUBE, 0.02f, shA * 0.7f); // the contact
            cube(stack_, p.x, p.y, 1.f, pieceColor(c.type), 1.f, nib_[y][x], seed_[y][x], glint, false, 0.f);
        }
    // cleared cubes crumbling
    for (const Dying& d : dying_) {
        if (!gameOn) break;
        const float t = d.t - d.delay;
        const float k = t < 0.f ? 0.f : saturate(t / (d.over ? 1.1f : 0.32f));
        if (k >= 1.f) continue;
        cube(stack_, d.x, d.y, 1.f, pieceColor(d.type), 1.f, d.nib, d.seed, glint, false, std::max(0.02f, k));
    }
    // the ghost and the falling piece (in the air: its shadow further off and softer)
    if (gameOn && game.hasPiece() && !game.over()) {
        const Piece& p = game.piece();
        int cells[4][2];
        game.pieceCells(p, cells);
        const vec3 col = pieceColor(p.type);
        Piece gp = p;
        gp.y = game.ghostY();
        if (gp.y > p.y) {
            int gc[4][2];
            game.pieceCells(gp, gc);
            for (auto& c : gc) {
                if (c[1] < Game::HIDDEN) continue;
                const vec3 w = layout::cellPos((float)c[0], (float)c[1]);
                cube(under_, w.x, w.y, 1.f, col, 0.9f, 0.f, 3.f, 0.f, true, 0.f);
            }
        }
        const float dx = game.pieceVisualX() - p.x, dy = game.pieceVisualY() - p.y;
        for (auto& c : cells) { // (whole above the well too: it spawns there)
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            shadow(over_, w.x + 0.16f, w.y - 0.22f, CUBE, 0.16f, shA * 0.8f);
        }
        for (auto& c : cells) {
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy);
            cube(top_, w.x, w.y, 1.f, col, 1.f, 0.f, 11.f + c[0] * 3.f + c[1] * 7.f, glint, false, 0.f);
        }
    }
    // previews
    auto preview = [&](int type, vec2 center, float sc, float alpha) {
        if (type < 0 || !gameOn) return;
        Piece p{type, 0, 0, 0};
        int cells[4][2];
        game.pieceCells(p, cells);
        float minx = 9, maxx = -9, miny = 9, maxy = -9;
        for (auto& c : cells) {
            minx = std::min(minx, (float)c[0]); maxx = std::max(maxx, (float)c[0]);
            miny = std::min(miny, (float)c[1]); maxy = std::max(maxy, (float)c[1]);
        }
        for (auto& c : cells) {
            const vec2 pos = v2(center.x + (c[0] - (minx + maxx) * 0.5f) * sc, center.y - (c[1] - (miny + maxy) * 0.5f) * sc);
            shadow(under_, pos.x + 0.06f * sc, pos.y - 0.09f * sc, CUBE * sc, 0.07f * sc, shA * alpha);
            shadow(under_, pos.x + 0.015f * sc, pos.y - 0.06f * sc, CUBE * sc, 0.02f * sc, shA * 0.7f * alpha);
            cube(top_, pos.x, pos.y, sc, pieceColor(type), alpha, 0.f, 5.f + type * 13.f + c[0] * 3.f + c[1], glint, false, 0.f);
        }
    };
    preview(game.holdType(), v2(layout::HOLD_CENTER.x, layout::HOLD_CENTER.y), layout::previewScale(0), game.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (game.next(i) >= 0) preview(game.next(i), v2(c.x, c.y), layout::previewScale(i), 1.f);
    }
    // grains: lying on the ground (under the ants), flying (over everything)
    for (const Grain& g : grains_) {
        if (g.t < g.delay || !gameOn) continue;
        const float k = (g.t - g.delay) / g.life;
        float a = g.food ? (1.f - smoothstepf(0.93f, 1.f, k)) * world : 1.f - smoothstepf(0.55f, 1.f, k);
        const float s = g.food ? g.size : g.size * (1.f - 0.5f * smoothstepf(0.5f, 1.f, k));
        if (a <= 0.f) continue;
        Sprite sp{{g.p.x, g.p.y, s * 2.6f, s * 2.6f}, {g.rot, 3.f, 0.f, a}, {g.col.x, g.col.y, g.col.z, g.food ? 0.5f : 0.9f}, {s, 0.f, 0.f, 0.f}};
        const bool inWell = std::fabs(g.p.x) < 5.05f && g.p.y > -10.05f;
        (g.food || inWell ? under_ : over_).push_back(sp);
    }
    // the charges: searching ones in the circuit's color, carrying ones in the color of the cube they touched
    const float tailK = 1.f + 0.6f * dropMix_ + 0.2f * pk;
    const vec3 chargeC = hex(V.charge);
    for (size_t i = 0; i < ants_.size(); i++) {
        const Ant& a = ants_[i];
        // inside the well (over the blocks) a charge is faint, fading in across the border
        const float inside = std::min(std::min(5.f - std::fabs(a.p.x), a.p.y + 10.f), 10.f - a.p.y);
        float al = saturate(a.alpha) * world * (1.f - (gameOn ? 0.66f : 0.f) * smoothstepf(-0.3f, 0.3f, inside));
        const bool carry = a.state == 1;
        if (!carry) al *= a.state == 2 || a.retire ? 0.3f : 0.5f;
        if (al <= 0.f) continue;
        const float r = carry ? 0.055f : 0.032f;
        const float tail = (carry ? 0.26f : 0.24f) * tailK * (a.pause > 0.f ? 0.3f : 1.f);
        const float h = tail + r * 3.f;
        const vec3 c = carry ? a.crumb : chargeC;
        antsV_.push_back({{a.p.x, a.p.y, h, h}, {a.a, 2.f, 0.f, al},
                          {c.x, c.y, c.z, a.pause > 0.f ? std::sin(clock_ * 31.f + i * 1.7f) : 0.f},
                          {r, tail, carry ? 1.f : 0.f, 0.f}});
    }
    // sparks and flashes (over everything, small and brief)
    for (const Spark& sp : sparks_) {
        if (!gameOn) break;
        const float k = sp.t / sp.life;
        const float spd = len(sp.v);
        const vec2 d = spd > 1e-4f ? sp.v * (1.f / spd) : v2(1.f, 0.f);
        const float Lh = clampf(spd * 0.03f, 0.03f, 0.2f);
        const vec2 c = sp.p - d * Lh;
        const float al = std::pow(1.f - k, 1.5f) * world;
        over_.push_back({{c.x, c.y, Lh, sp.w * 3.f}, {std::atan2(d.y, d.x), 5.f, 0.f, al}, {sp.col.x, sp.col.y, sp.col.z, 1.f - k},
                         {Lh, sp.w, 0.f, 0.f}});
    }
    for (const Flash& fl : flashes_) {
        if (!gameOn) break;
        const float k = fl.t / fl.life;
        const float al = (1.f - k) * (1.f - k) * world;
        over_.push_back({{fl.p.x, fl.p.y, fl.r * 1.3f, fl.r * 1.3f}, {0.f, 6.f, 0.f, al}, {fl.col.x, fl.col.y, fl.col.z, 0.f},
                         {fl.r * (0.7f + 0.3f * k), 0.f, 0.f, 0.f}});
    }
    // the bonus gauge: twelve grains of sugar in a ring
    if (gameOn) {
        const vec3 gc = layout::GAUGE_CENTER;
        const float R = layout::GAUGE_R * 0.78f;
        const int N = Game::BONUS_LINES;
        const float lvl = saturate(gaugeShown_) * N;
        const bool ready = game.bonusReady(), active = game.bonusActive();
        const vec3 lit = ready || active ? hex(V.accent) : hex(V.ink);
        for (int i = 0; i < N; i++) {
            const float ang = PI * 0.5f - TAU * i / N;
            const vec2 p = v2(gc.x + std::cos(ang) * R, gc.y + std::sin(ang) * R);
            const float fill = saturate(lvl - i);
            const float s = 0.075f;
            top_.push_back({{p.x, p.y, 0.14f, 0.14f}, {0.f, 4.f, 0.f, 0.35f}, {hex(V.dim).x, hex(V.dim).y, hex(V.dim).z, 1.f},
                            {s * 0.75f, 0.012f, 0.f, 0.f}});
            if (fill > 0.f)
                top_.push_back({{p.x, p.y, s * 2.6f, s * 2.6f}, {ang + PI * 0.25f, 3.f, 0.f, fill}, {lit.x, lit.y, lit.z, ready || active ? 0.8f : 0.f}, {s, 0.f, 0.f, 0.f}});
        }
    }

    // ---- draw
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    {
        vec3 ground = hex(V.ground), mottle = hex(V.mottle), panel = hex(V.panel);
        // a DROP: the substrate takes a deep shade of its current's color (still dark: the stack reads on it)
        if (dropMix_ > 0.001f) {
            const vec3 ch = hex(V.charge);
            ground = lerp(ground, ch * 0.2f, 0.65f * dropMix_);
            mottle = lerp(mottle, ch * 0.27f, 0.65f * dropMix_);
            panel = lerp(panel, ch * 0.12f, 0.4f * dropMix_);
        }
        if (slow_ > 0.001f) {
            ground = lerp(ground, hex(V.slow), 0.35f * slow_);
            mottle = lerp(mottle, hex(V.slow), 0.35f * slow_);
            panel = lerp(panel, hex(V.slow), 0.2f * slow_);
        }
        glUseProgram(progGround_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pherTex_);
        set1i(progGround_, "uPher", 0);
        set4f(progGround_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set4f(progGround_, "uPherRect", t0_.x, t0_.y, tw_ * TCELL, th_ * TCELL);
        set2f(progGround_, "uPherTexel", 1.f / tw_, 1.f / th_);
        set1f(progGround_, "uPx", px_);
        set3f(progGround_, "uGround", ground);
        set3f(progGround_, "uMottle", mottle);
        set3f(progGround_, "uPanel", panel);
        set3f(progGround_, "uTrace", lerp(hex(V.trace), hex(V.charge), 0.25f * dropMix_));
        set3f(progGround_, "uCore", hex(V.core));
        set3f(progGround_, "uTerm", hex(V.term));
        set3f(progGround_, "uCharge", hex(V.charge));
        set3f(progGround_, "uFrame", hex(V.frame));
        set1f(progGround_, "uFrameA", V.frameA);
        set1f(progGround_, "uWell", gameOn ? 1.f : 0.f); // a break: no panel, no frame
        set1f(progGround_, "uGrain", V.grain);
        // a steady glow per section, brighter at a peak, a swell on the drop that settles
        set1f(progGround_, "uTraceAmt", V.traceAmt * (0.62f + 0.12f * pm + 0.16f * pk + 0.3f * dropMix_ + 0.3f * swell_));
        set1f(progGround_, "uTermGlow", 0.25f * pk + 0.6f * dropMix_ + 0.6f * swell_);
        set1f(progGround_, "uTime", clock_);
        set1f(progGround_, "uWorld", world);
        set4f(progGround_, "uNest0", nest_[0].x, nest_[0].y, nestR_, 0.f);
        set4f(progGround_, "uNest1", nest_[1].x, nest_[1].y, nestR_, 0.f);
        set3f(progGround_, "uBridgeCol", bridgeCol);
        set1f(progGround_, "uBridge", b);
        ugl::drawFullscreen();
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progSprite_);
    setMat(progSprite_, "uVP", vp_);
    set1f(progSprite_, "uPx", px_);
    const vec3 ld = normalize(vec3(-0.45f, 0.6f, 0.8f));
    set3f(progSprite_, "uLightDir", ld);
    set1f(progSprite_, "uBridge", b);
    set3f(progSprite_, "uBridgeInk", bridgeInk);
    set3f(progSprite_, "uRim", lerp(hex(V.charge), hex(V.core), 0.4f));
    // a DROP powers the dies: their circuitry glows, a wave of current running up the stack every two beats
    set1f(progSprite_, "uPower", dropMix_);
    set1f(progSprite_, "uWave", music.hasSong ? -11.f + 24.f * (float)std::fmod(music.beat * 0.5, 1.0) : -99.f);
    drawSprites(under_);
    drawSprites(stack_);
    drawSprites(antsV_);
    drawSprites(over_);
    drawSprites(top_);
    if (gameOn) drawPanels(f.panels);

    // ---- the veil: startup fade, pause / game-over dim
    veil_ = std::max(1.f - saturate(f.fade), dimT_ * 0.5f);
    if (veil_ > 0.001f) {
        glUseProgram(progVeil_);
        const vec3 vc(0.f, 0.f, 0.f);
        set4f(progVeil_, "uVeil", vc.x, vc.y, vc.z, veil_);
        ugl::drawFullscreen();
    }
    glDisable(GL_BLEND);
    // hand the frame over now (the headless EGL readback of large frames came back black without it)
    glFlush();
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_electron() {
    static const UniverseEntry e{
        "ELECTRON", "bare silicon dies on a dark substrate, and charges wiring themselves to them",
        [] { return std::unique_ptr<Universe>(new ElectronUniverse()); },
        // busy, lively songs suit the circuit a little better
        [](const SongLook& l) { return 0.8f + 0.3f * l.density; },
        [] {
            std::vector<std::string> v;
            for (const Variant& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

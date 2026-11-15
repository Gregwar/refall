// SHADOW: the game is a shadow.
//
// One world: a dark room. At the bottom left, on a table, stands a little model of the game: a wooden well with
// little glass blocks in it, the falling piece moving and turning in it, lit by a vintage projector standing beside
// it. The big board on screen is that model's shadow, cast on the plaster wall behind, cell for cell (the model is
// built at the board's own scale, so its shadow lands exactly on the grid): the frame's posts and base in solid
// shadow, each glass block a pool of colored light with a darker rim where the glass bends the light away and a
// brighter heart. The ghost is a faint outline, the previews are small shadows beside it. The beam links them:
// haze and dust drifting near the lamp (never over the well or the HUD).
//
// Staging: calm is a dimmer lamp and a few motes; mid brightens it; a peak warms it, the dust swirls on the kicks
// and the lens glints. A drop turns on a second projector of another color for the whole drop: its light floods
// the wall too and casts a second, stretched shadow of the model beside the first (colored shadows), held until the
// drop ends, then eased back. The lamp breathes with the energy and flickers very slightly, never pulses.
// A cleared row's shadow shrinks away while, on the model, its blocks are lifted out and set aside on the table,
// sorted on stacks by color: the table fills up with spent blocks over the song. Slow motion turns the lamp to a cool
// moonlight; game over dims it.
//
// Around a song, the projector is switched on and off: in the setup its lamp warms up from a dull red filament and
// its iris opens on the wall; in the teardown the iris closes back to a spot and the lamp cools down to dark.
//
// Layout contract: a flat orthographic view of the wall at the reference camera's scale on the board plane; the
// model is ray marched in its own corner with a camera of its own.
#include "universes/shadow/shadow.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/shadow/shaders.hpp"

namespace {

// Marcellus, SIL Open Font License, see OFL-Marcellus.txt.
const unsigned char FONT_MARCELLUS[] = {
#include "universes/shadow/Marcellus-Regular.ttf.inc"
};

namespace shaders = shadow_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;

GLuint compile(const char* vs, const char* fs, const char* name, bool common) {
    std::string f(fs);
    if (common) f.insert(f.find('\n') + 1, shaders::COMMON_GLSL);
    return ugl::program(vs, f.c_str(), name);
}

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

struct VariantDef {
    const char* name;
    unsigned light, light2, wall, ambient;
    unsigned gel[7];        // the glass blocks (I O T S Z J L)
    unsigned wood, table, body, room;
    float tableKind, wallKind;
    unsigned inkDark, inkLight, accent;
};
const VariantDef VARIANTS[] = {
    // a tungsten lamp on a plaster wall, walnut and birch, jewel-colored glass; the drop: a cold blue lamp
    {"CINEMA", 0xffd29c, 0x6aa8ff, 0xcfc4b2, 0x0c0907,
     {0x3cc0cc, 0xeedc3a, 0xa45ad2, 0x66c447, 0xe8413a, 0x3a6ee0, 0xf3801e},
     0xc89a64, 0x7a5034, 0x1f1c1a, 0x070504, 0.f, 0.f, 0x2b2119, 0xeadcc6, 0xffcf8a},
    // a cold daylight lamp on pale drafting paper, a blue-grey board; the drop: an amber lamp
    {"BLUEPRINT", 0xd2e6ff, 0xffaa55, 0xc9d3da, 0x07090c,
     {0x2ac8e2, 0xf6dc40, 0xa47ae8, 0x40cc98, 0xf2605a, 0x3460d6, 0xf89a3c},
     0xd9c9a8, 0x5f7686, 0x9cadb9, 0x040608, 1.f, 0.4f, 0x1b2733, 0xdde8f2, 0xa8d4ff},
    // a white stage lamp on a cinema screen, black velvet, theatre gels; the drop: a magenta lamp
    {"COLOR GEL", 0xfff3e4, 0xff4fb4, 0xd8d6d2, 0x080708,
     {0x00ccd8, 0xffd600, 0xc42ccc, 0x4cd63a, 0xff2a3c, 0x2a4cff, 0xff7a00},
     0xb89870, 0x2c2930, 0x6a1c1a, 0x050405, 2.f, 1.f, 0x1d1a1f, 0xeee8e2, 0xffb0dc},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

const vec3 MOON(0.42f, 0.58f, 1.0f);
constexpr float TABLE_Y = -10.8f, PILE_S = 0.44f;
constexpr int MAX_FLIGHTS = 24, MAX_SHRINKS = 40;
// the spent blocks stay low: two layers per color in the tray, then the oldest go into the box (three layers)
constexpr int TRAY_MAX = 18, BOX_MAX = 48;
const vec2 BOX_C{7.2f, 1.9f};

float easeInOut(float t) { t = saturate(t); return t * t * (3.f - 2.f * t); }

// A piece's cell joined to its neighbours (the shader's cellShape mask): +x, -x, up, down, and the 2 x 2 corners.
float joinMask(const int cells[4][2], int i) {
    int m = 0;
    for (int j = 0; j < 4; j++) {
        if (j == i) continue;
        const int dx = cells[j][0] - cells[i][0], dy = cells[j][1] - cells[i][1];
        if (dx == 1 && dy == 0) m |= 1;
        if (dx == -1 && dy == 0) m |= 2;
        if (dx == 0 && dy == -1) m |= 4;
        if (dx == 0 && dy == 1) m |= 8;
    }
    for (int k = 0; k < 4; k++) {
        const int sx = (k & 1) ? -1 : 1, sy = (k & 2) ? 1 : -1;
        const int bx = sx > 0 ? 1 : 2, by = sy < 0 ? 4 : 8;
        bool diag = false;
        for (int j = 0; j < 4; j++) diag |= cells[j][0] == cells[i][0] + sx && cells[j][1] == cells[i][1] + sy;
        if ((m & bx) && (m & by) && diag) m |= 16 << k;
    }
    return (float)m;
}
} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool ShadowUniverse::init(int width, int height) {
    progWall_ = compile(ugl::FULLSCREEN_VS, shaders::WALL_FS, "shadow wall", true);
    progModel_ = compile(shaders::RECT_VS, shaders::MODEL_FS, "shadow model", true);
    progHaze_ = compile(ugl::FULLSCREEN_VS, shaders::HAZE_FS, "shadow haze", false);
    progFinal_ = compile(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "shadow final", false);
    progCompose_ = compile(shaders::RECT_VS, shaders::COMPOSE_FS, "shadow compose", false);
    progPanel_ = compile(shaders::PANEL_VS, shaders::PANEL_FS, "shadow panel", false);

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glGenVertexArrays(1, &rectVao_);
    glBindVertexArray(rectVao_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenVertexArrays(1, &panelVao_);
    glGenBuffers(1, &panelVbo_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    glGenTextures(1, &cellsTex_);
    glBindTexture(GL_TEXTURE_2D, cellsTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Game::W, Game::H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // test hook: ZEN_SHADOW_SPENT=N starts with N spent blocks set aside (a late still)
    if (const char* s = std::getenv("ZEN_SHADOW_SPENT"))
        for (int i = 0, n = std::atoi(s); i < n; i++) {
            const int type = (i * 3 + i / 7) % 7, dest = pileDest(type);
            pile_[dest] = std::min(dest >= 7 ? BOX_MAX : TRAY_MAX, pile_[dest] + 1);
        }
    resize(width, height);
    return glGetError() == GL_NO_ERROR;
}

void ShadowUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    // the model's corner: from the screen's left edge to the HUD column, from the bottom up
    modelRect_[0] = viewC_.x - hw - 0.2f;
    sceneT_.destroy();
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    const float pxPerUnit = height_ / (2.f * hh);
    static const float ss = std::getenv("ZEN_SHADOW_SS") ? (float)std::atof(std::getenv("ZEN_SHADOW_SS")) : 1.5f;
    const int mw = std::max(8, (int)std::lround((modelRect_[2] - modelRect_[0]) * pxPerUnit * ss));
    const int mh = std::max(8, (int)std::lround((modelRect_[3] - modelRect_[1]) * pxPerUnit * ss));
    modelT_.destroy();
    modelT_.create(mw, mh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    setupModelCamera();
}

void ShadowUniverse::setupModelCamera() {
    // the camera looks at the model from the front right, a little from above; the projector stands at its left
    const vec3 target(-4.4f, -3.4f, 2.f);
    eye_ = vec3(12.f, 11.5f, 66.f);
    fwd_ = normalize(target - eye_);
    right_ = normalize(cross(fwd_, vec3(0, 1, 0)));
    up_ = cross(right_, fwd_);
    const float aspect = (modelRect_[2] - modelRect_[0]) / (modelRect_[3] - modelRect_[1]);
    tan_.y = std::tan(15.5f * PI / 180.f);
    tan_.x = tan_.y * aspect;
    lensPos_ = vec3(-11.f, -5.2f, 9.5f);
    lensDir_ = normalize(vec3(0.f, -0.8f, -0.6f) - lensPos_);
    // where the lens is on screen (board space)
    const vec3 q = lensPos_ - eye_;
    const float z = dot(q, fwd_);
    const float nx = dot(q, right_) / (z * tan_.x), ny = dot(q, up_) / (z * tan_.y);
    lensScreen_ = vec2{lerpf(modelRect_[0], modelRect_[2], nx * 0.5f + 0.5f), lerpf(modelRect_[1], modelRect_[3], ny * 0.5f + 0.5f)};
    if (std::getenv("ZEN_SHADOW_DEBUG")) std::fprintf(stderr, "[shadow] lens on screen %.2f %.2f\n", lensScreen_.x, lensScreen_.y);
}

ShadowUniverse::~ShadowUniverse() {
    sceneT_.destroy();
    modelT_.destroy();
    for (GLuint p : {progWall_, progModel_, progHaze_, progFinal_, progCompose_, progPanel_}) glDeleteProgram(p);
    for (GLuint v : {rectVao_, panelVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, panelVbo_}) glDeleteBuffers(1, &b);
    if (cellsTex_) glDeleteTextures(1, &cellsTex_);
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void ShadowUniverse::setSong(const SongLook& t, float seconds) {
    (void)seconds;
    if (t.variant >= 0) variant_ = t.variant % VARIANT_COUNT;
    else {
        // dark or minor songs get the tungsten cinema, airy bright ones the blueprint, dense punchy ones the gels
        int v = 0;
        if (t.dynamics > 0.6f || t.density > 0.62f) v = 2;
        else if (t.brightness > 0.55f && !t.minor) v = 1;
        if ((t.seed % 4) == 0) v = (v + 1 + (int)((t.seed >> 8) % 2)) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 2)) % VARIANT_COUNT;
        variant_ = v;
    }
    seed_ = t.seed;
    rng_ = Rng(t.seed ^ 0x5AD0ull);
}

void ShadowUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    energy_ = energy;
    // the second projector: on for the whole peak (the drop), off when it ends
    dropMode_ = level >= 2;
    if (seconds < 0.1f) {
        drop_ = dropMode_ ? 1.f : 0.f;
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
    lvDur_ = std::clamp(seconds, rising ? 0.8f + 0.8f * (1.f - impact) : 2.5f, 6.f);
}

void ShadowUniverse::drop(float strength) {
    if (level_ < 2) return;
    dropMode_ = true;
    dropSwell_ = std::max(dropSwell_, 0.5f + 0.5f * saturate(strength));
    swirlAmp_ = std::max(swirlAmp_, 1.2f);
}

vec2 ShadowUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints ShadowUniverse::hints() const {
    const VariantDef& d = VARIANTS[variant_];
    TransitionHints h;
    h.background = lin(d.wall) * lin(d.light) * 0.5f;
    h.ink = lin(d.inkDark);
    h.accent = lin(d.accent);
    h.light = true;
    h.styles = TS_LUMA | TS_CURTAIN | TS_INK;
    return h;
}

// =====================================================================================================
// The board
// =====================================================================================================

void ShadowUniverse::syncIds(const Game& g) {
    // an id is kept only while its cells are one piece's: filled, one type, at most four (anything else, such as a
    // board filled for a review or a stack inherited from another universe, is grouped again below)
    int wrong = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            if (g.cell(x, y).type < 0) { if (ids_[y][x]) { ids_[y][x] = 0; wrong++; } continue; }
            if (!ids_[y][x]) { wrong++; continue; }
            int n = 0;
            bool mixed = false;
            for (int yy = 0; yy < Game::H; yy++)
                for (int xx = 0; xx < Game::W; xx++)
                    if (ids_[yy][xx] == ids_[y][x]) { n++; mixed |= g.cell(xx, yy).type != g.cell(x, y).type; }
            if (n > 4 || mixed) {
                const int bad = ids_[y][x];
                for (int yy = 0; yy < Game::H; yy++)
                    for (int xx = 0; xx < Game::W; xx++)
                        if (ids_[yy][xx] == bad) ids_[yy][xx] = 0;
                wrong++;
            }
        }
    if (!wrong) return;
    // cells of unknown origin (a filled board, a stack inherited from another universe): grouped into pieces of at
    // most four cells of one color
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

void ShadowUniverse::uploadBoard(const Game& g) {
    unsigned char data[Game::H][Game::W][4];
    for (int y = 0; y < Game::H; y++) {
        rowY_[y] = layout::cellPos(0.f, y - g.rowOffset(y)).y;
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            const int id = ids_[y][x];
            int m = 0;
            if (c.type >= 0 && id) {
                if (x + 1 < Game::W && ids_[y][x + 1] == id) m |= 1;
                if (x > 0 && ids_[y][x - 1] == id) m |= 2;
                if (y > 0 && ids_[y - 1][x] == id && std::fabs(g.rowOffset(y - 1) - g.rowOffset(y)) < 0.02f) m |= 4;
                if (y + 1 < Game::H && ids_[y + 1][x] == id && std::fabs(g.rowOffset(y + 1) - g.rowOffset(y)) < 0.02f) m |= 8;
                // the diagonals of a 2 x 2 square of one piece (+x+y, -x+y, +x-y, -x-y)
                for (int k = 0; k < 4; k++) {
                    const int sx = (k & 1) ? -1 : 1, sy = (k & 2) ? 1 : -1; // (board +y is game row y - 1)
                    const int bx = sx > 0 ? 1 : 2, by = sy < 0 ? 4 : 8;
                    const int xx = x + sx, yy = y + sy;
                    if ((m & bx) && (m & by) && xx >= 0 && xx < Game::W && yy >= 0 && yy < Game::H && ids_[yy][xx] == id) m |= 16 << k;
                }
            }
            data[y][x][0] = (unsigned char)(c.type + 1);
            data[y][x][1] = (unsigned char)m;
            data[y][x][2] = (unsigned char)std::lround(255.f * saturate(c.flash * c.flash));
            data[y][x][3] = 255;
        }
    }
    static const bool check = std::getenv("ZEN_SHADOW_CHECK") != nullptr;
    if (check) {
        // (test hook: every join between two cells must be one piece's: same id, same type)
        int bad = 0;
        for (int y = 0; y < Game::H; y++)
            for (int x = 0; x < Game::W; x++) {
                const int m = data[y][x][1];
                const int t = g.cell(x, y).type;
                if ((m & 1) && g.cell(x + 1, y).type != t) bad++;
                if ((m & 4) && g.cell(x, y - 1).type != t) bad++;
                if (t >= 0 && !ids_[y][x]) bad++;
            }
        if (bad) std::fprintf(stderr, "[shadow] check: %d bad joins\n", bad);
    }
    for (int s = 0; s < 24; s++) {
        const float yc = 11.5f - s;
        int best = -1;
        float bd = 1.5f;
        for (int r = Game::HIDDEN; r < Game::H; r++) {
            const float dd = std::fabs(rowY_[r] - yc);
            if (dd < bd) { bd = dd; best = r; }
        }
        slot_[s] = (float)best;
    }
    glBindTexture(GL_TEXTURE_2D, cellsTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Game::W, Game::H, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // the falling piece and its ghost
    for (float& v : piece_) v = 0.f;
    for (float& v : ghost_) v = 0.f;
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        pieceType_ = (float)p.type;
        for (int i = 0; i < 4; i++) {
            const float yy = cells[i][1] + dy;
            const vec3 c = layout::cellPos(cells[i][0] + dx, yy);
            piece_[i * 4 + 0] = c.x; piece_[i * 4 + 1] = c.y; piece_[i * 4 + 2] = joinMask(cells, i);
            piece_[i * 4 + 3] = 1.f; // (above the well too: it spawns there)
        }
        const int gy = g.ghostY();
        if (gy > p.y + 0) {
            for (int i = 0; i < 4; i++) {
                const vec3 c = layout::cellPos((float)cells[i][0], (float)(cells[i][1] - p.y + gy));
                ghost_[i * 4 + 0] = c.x; ghost_[i * 4 + 1] = c.y; ghost_[i * 4 + 2] = joinMask(cells, i);
                ghost_[i * 4 + 3] = cells[i][1] - p.y + gy >= Game::HIDDEN ? 1.f : 0.f;
            }
        }
    }
    // the previews: small shadows at the HOLD / NEXT anchors
    prev_.clear();
    auto preview = [&](int type, const vec3& center, float scale) {
        if (type < 0 || type >= PIECE_COUNT) return;
        Piece p;
        p.type = type; p.rot = 0; p.x = 0; p.y = 0;
        int cells[4][2];
        g.pieceCells(p, cells);
        float x0 = 99, x1 = -99, y0 = 99, y1 = -99;
        for (auto& c : cells) { x0 = std::min(x0, (float)c[0]); x1 = std::max(x1, (float)c[0]); y0 = std::min(y0, (float)c[1]); y1 = std::max(y1, (float)c[1]); }
        const float cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
        for (int i = 0; i < 4; i++) {
            prev_.push_back(center.x + (cells[i][0] - cx) * scale);
            prev_.push_back(center.y - (cells[i][1] - cy) * scale);
            prev_.push_back(scale);
            prev_.push_back((float)type + 8.f * joinMask(cells, i)); // (the type and the join mask)
        }
    };
    preview(g.holdType(), layout::HOLD_CENTER, layout::previewScale(0));
    for (int i = 0; i < 3; i++) preview(g.next(i), layout::nextCenter(i), layout::previewScale(i));
}

vec2 ShadowUniverse::pileCenter(int type) const {
    // a low tray at the table's front right, out of the beam: four stacks in front, three behind
    if (type < 4) return vec2{1.6f + type * 1.55f, 6.0f};
    return vec2{2.4f + (type - 4) * 1.55f, 4.45f};
}

int ShadowUniverse::pileDest(int type) const {
    return pile_[type] + reserved_[type] < TRAY_MAX ? type : 7;
}

vec3 ShadowUniverse::pileSlot(int dest, int n) const {
    const bool box = dest >= 7;
    const int N = box ? 4 : 3, per = N * N;
    if (box && n >= BOX_MAX) n = BOX_MAX - per + n % per; // (a full box: on its top layer)
    n = std::min(n, box ? BOX_MAX - 1 : TRAY_MAX - 1);
    const vec2 c = box ? BOX_C : pileCenter(dest);
    const int layer = n / per, i = n % per;
    const float H = 0.5f * N * PILE_S;
    return vec3(c.x - H + (i % N + 0.5f) * PILE_S, TABLE_Y + 0.12f + (layer + 0.5f) * PILE_S, c.y - H + (i / N + 0.5f) * PILE_S);
}

void ShadowUniverse::onEvent(const GameEvent& ev, const Game& game) {
    (void)game;
    switch (ev.type) {
    case GameEvent::Lock: {
        const int id = nextId_++;
        for (auto& c : ev.cells)
            if (c.y >= 0 && c.y < Game::H && c.x >= 0 && c.x < Game::W) ids_[c.y][c.x] = id;
        break;
    }
    case GameEvent::Clear: {
        // lifted out of the model, row by row from the bottom, from the middle out, and set on their stacks
        int rowsSeen[Game::H] = {};
        int k = 0;
        for (int y = Game::H - 1; y >= 0; y--) {
            bool any = false;
            for (auto& c : ev.cells) any |= c.y == y;
            if (any) rowsSeen[y] = ++k;
        }
        // the piece ids follow the rows down exactly as the game compacts its board (else stale ids would merge
        // cells of different pieces, and the model's and the shadow's shapes would no longer be the pieces')
        {
            bool full[Game::H] = {};
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) full[c.y] = true;
            // a piece cut by the clear: each part keeps its own id (the part above falls by more rows than the
            // part below, and stays a separate shape even once they touch again)
            int below[Game::H] = {};
            for (int y = Game::H - 2; y >= 0; y--) below[y] = below[y + 1] + (full[y + 1] ? 1 : 0);
            std::vector<std::pair<long long, int>> remap;
            for (int y = 0; y < Game::H; y++)
                for (int x = 0; x < Game::W; x++) {
                    if (full[y] || !ids_[y][x] || below[y] == 0) continue;
                    const long long key = (long long)ids_[y][x] * 64 + below[y];
                    int nid = 0;
                    for (auto& r : remap) if (r.first == key) nid = r.second;
                    if (!nid) { nid = nextId_++; remap.push_back({key, nid}); }
                    ids_[y][x] = nid;
                }
            int nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (full[y]) continue;
                std::memcpy(nb[dst], ids_[y], sizeof(ids_[y]));
                dst--;
            }
            std::memcpy(ids_, nb, sizeof(ids_));
        }
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H) continue;
            const int type = std::clamp(c.type, 0, 6);
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const float order = (rowsSeen[c.y] - 1) * 0.22f + std::fabs(c.x - 4.5f) * 0.035f;
            if ((int)shrinks_.size() < MAX_SHRINKS) shrinks_.push_back({p.x, p.y, -std::fabs(c.x - 4.5f) * 0.012f, type});
            const int dest = pileDest(type);
            const int n = pile_[dest] + reserved_[dest];
            if ((int)flights_.size() < MAX_FLIGHTS) {
                Flight f;
                f.from = vec3(p.x, p.y, 0.f);
                f.to = pileSlot(dest, n);
                f.dest = dest;
                f.axis = normalize(vec3(rng_.uniform() - 0.5f, rng_.uniform() - 0.5f, rng_.uniform() - 0.5f) + vec3(0.f, 0.f, 0.3f));
                f.t = -order;
                f.dur = 1.05f + 0.15f * rng_.uniform();
                f.spin = (rng_.uniform() < 0.5f ? -1.f : 1.f) * (1.f + rng_.uniform()) * PI * 0.5f;
                f.type = type;
                flights_.push_back(f);
                reserved_[dest]++;
            } else pile_[dest] = std::min(dest >= 7 ? BOX_MAX : TRAY_MAX, pile_[dest] + 1);
        }
        if (ev.count >= 4) { quadFlash_ = 1.f; swirlAmp_ = std::max(swirlAmp_, 1.5f); }
        break;
    }
    case GameEvent::TopOut: overT_ = 0.f; break;
    default: break;
    }
}

// =====================================================================================================
// Text
// =====================================================================================================

float ShadowUniverse::wallLightAt(float px, float py) const {
    // (the CPU twin of the wall's circle: is this pixel on the lit wall or in the dark around it?)
    const float bx = viewC_.x + (px / width_ * 2.f - 1.f) * viewH_.x;
    const float by = viewC_.y + (1.f - py / height_ * 2.f) * viewH_.y;
    const float qx = bx / 13.6f, qy = (by - 0.3f) / 12.9f;
    const float r = std::sqrt(qx * qx + qy * qy);
    return smoothstepf(1.02f, 0.9f, r) * (by > TABLE_Y ? 1.f : 0.f);
}

// The HUD's panels (its lettering: hudLine, the host letters it).
void ShadowUniverse::drawHud(const std::vector<HudRect>& panels) {
    if (panels.empty()) return;
    const vec3 acc = hex(VARIANTS[variant_].accent);
    glUseProgram(progPanel_);
    set2f(progPanel_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(panelVao_);
    glBindBuffer(GL_ARRAY_BUFFER, panelVbo_);
    for (const HudRect& r : panels) {
        const float q[12] = {r.x, r.y, r.x + r.w, r.y, r.x + r.w, r.y + r.h, r.x, r.y, r.x + r.w, r.y + r.h, r.x, r.y + r.h};
        glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STREAM_DRAW);
        const vec3 c = r.custom ? r.color : r.accent ? acc : vec3(0.03f, 0.025f, 0.02f);
        const float a = saturate(r.accent ? r.alpha * 0.35f : r.alpha * 0.9f);
        glUniform4f(ugl::U(progPanel_, "uColor"), c.x * a, c.y * a, c.z * a, a);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

// The HUD in SHADOW's typography, lettered by the host (text.hpp): Marcellus, set on the wall like the titles of a
// magic-lantern slide. On the lit wall the letters are dark (shadows themselves), in the dark around the circle they
// are pale, each on a soft halo of the other.
void ShadowUniverse::hudLine(const HudText& h, text::Line& out) {
    const VariantDef& d = VARIANTS[variant_];
    const vec3 dark = hex(d.inkDark), pale = hex(d.inkLight), acc = hex(d.accent);
    text::Font* marcellus = text::font(FONT_MARCELLUS);
    const float s = height_ / 1080.f, mul = hudMul_;
    const float base = h.scale * 12.f;
    const float alpha = saturate(h.alpha) * mul;
    // a bias on the old distance field (8 px of spread at 64 px), in em
    constexpr float BIAS = 255.f / 16.f / 64.f;
    out.look.shadowW = 0.25f * BIAS;
    out.look.shadowSoft = 0.3f * BIAS;
    out.look.weight = 0.02f * BIAS;
    auto put = [&](const std::string& t, float size, float track, float dy, bool accent, float a) {
        out.add(t, marcellus, size, vec3(1.f), track / size, saturate(a));
        out.dy = dy;
        // how lit the wall is under the line's middle (its width roughly from its letters)
        size_t n = 0;
        for (unsigned char c : t) n += (c & 0xC0) != 0x80;
        const float w = n * (size * 0.6f + track);
        const float lit = wallLightAt(ugl::hudLeft(h, w, width_) + w * 0.5f, h.y + size * 0.5f);
        out.spans.back().ink = (accent ? lerp(acc, dark * 1.2f, lit) : lerp(pale, dark, lit)) * mul;
        out.look.shadow = lerp(vec3(0.02f, 0.018f, 0.015f), pale, lit);
        out.look.shadowA = lerpf(0.7f, 0.25f, lit);
    };
    switch (h.role) {
    case HUD_UNIVERSE: {
        // the name in the accent, the variant smaller and pale after it, on a dark halo
        std::string name, var;
        text::splitUniverse(h.text, name, var);
        const float sz = base * 0.9f, tr = base * 0.34f, vsz = base * 0.68f, vtr = base * 0.26f;
        const float a = saturate(0.35f + h.alpha * 1.6f) * mul;
        out.add(name, marcellus, sz, acc * mul, tr / sz, a);
        if (!var.empty()) {
            out.add("", marcellus, base, acc, (base * 0.9f - tr) / base); // the gap
            // its cap line (sz - vsz) * 0.75 lower (Marcellus' capitals: 0.558 of its size)
            out.add(var, marcellus, vsz, pale * mul, vtr / vsz, a * 0.75f).dy = (0.75f - 0.558f) * (sz - vsz);
        }
        out.look.shadow = vec3(0.02f);
        out.look.shadowA = 0.7f;
        out.look.weight = 0.03f * BIAS;
        break;
    }
    case HUD_TITLE: put(h.text, base * 1.0f, base * 0.04f, 0.f, false, saturate(h.alpha * 1.3f) * mul); break;
    case HUD_INFO: put(h.text, base * 0.74f, base * 0.12f, 0.f, false, alpha * 0.65f); break;
    case HUD_SECTION: put(h.text, base * 0.88f, base * 0.42f, 0.f, true, saturate(0.2f + h.alpha * 1.9f) * mul); break;
    case HUD_LABEL: put(h.text, base * 0.66f, base * 0.42f, 0.f, false, saturate(alpha * 1.3f)); break;
    case HUD_VALUE: put(h.text, base * 1.05f, base * 0.05f, 0.f, false, alpha); break;
    case HUD_NOTICE: put(h.text, base * 0.95f, base * 0.14f, 0.f, true, alpha); break;
    case HUD_TOAST: put(h.text, base * 1.2f, base * 0.34f, -base * 0.1f, false, alpha); break;
    case HUD_TOAST_SUB: put(h.text, base * 0.82f, base * 0.14f, 0.f, false, alpha); break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(h.text, base * 1.05f, base * 0.2f, -base * 0.1f, false, alpha);
        else put(h.text, base * 0.8f, base * 0.06f, 0.f, h.accent, alpha);
        break;
    default: put(h.text, base * 0.8f, base * 0.05f, 0.f, h.accent, alpha); break;
    }
}

// =====================================================================================================
// Frame
// =====================================================================================================

void ShadowUniverse::setCommon(GLuint prog) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cellsTex_);
    set1i(prog, "uCells", 0);
    glUniform1fv(ugl::U(prog, "uRowY"), 22, rowY_);
    glUniform1fv(ugl::U(prog, "uSlot"), 24, slot_);
    glUniform4fv(ugl::U(prog, "uPiece"), 4, piece_);
    set1f(prog, "uPieceType", pieceType_);
}

void ShadowUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    const VariantDef& V = VARIANTS[variant_];
    const bool bare = f.bare;

    // ---- Time, staging.
    if (!bare) syncIds(game);
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.2f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f) overT_ += dt;
    if (overT_ > 4.f) overT_ = -1.f;
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.35f, slow_);
    time_ += f.paused ? 0.f : dt;
    worldTime_ += wdt;
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, easeInOut(lvT_));
    }
    energyS_ = approach(energyS_, saturate(0.5f * energy_ + 0.5f * music.energy), 0.8f, dt);
    const float peak = saturate(lv_ - 1.f);
    // the drop's second projector: up over ~2 s, held, eased back over ~3 s
    drop_ += ((dropMode_ ? 1.f : 0.f) - drop_) * std::min(1.f, dt * (dropMode_ ? 1.3f : 0.8f));
    dropSwell_ = approach(dropSwell_, 0.f, 0.35f, dt);
    // the kicks: the dust swirls, the lens glints (peaks only)
    if (!f.paused) {
        const float k = std::max(kickIn_, music.kick * 0.8f);
        if (k > 0.05f) {
            swirlAmp_ = std::max(swirlAmp_, k * lerpf(0.25f, 1.1f, peak));
            lensGlow_ = std::max(lensGlow_, k * 0.35f * peak);
        }
    }
    kickIn_ = 0.f;
    lensGlow_ = std::max(lensGlow_, glowIn_);
    glowIn_ = 0.f;
    swirlAmp_ = approach(swirlAmp_, 0.f, 0.9f, dt);
    lensGlow_ = approach(lensGlow_, 0.f, 1.6f, dt);
    quadFlash_ = approach(quadFlash_, 0.f, 0.6f, dt);
    swirl_ += wdt * (0.15f + 2.2f * swirlAmp_);
    // a slight warm flicker of the lamp (no pulse)
    flickT_ -= dt;
    if (flickT_ <= 0.f) { flickT_ = 0.06f + 0.12f * rng_.uniform(); flickTo_ = 1.f + (rng_.uniform() - 0.5f) * (0.02f + 0.015f * lv_); }
    flicker_ = approach(flicker_, flickTo_, 6.f, dt);

    float intensity = lerpf(0.8f, 1.02f, saturate(lv_ * 0.5f)) * (0.95f + 0.1f * energyS_) * flicker_ * (1.f + 0.12f * dropSwell_);
    if (overT_ >= 0.f) intensity *= 1.f - 0.55f * smoothstepf(0.f, 0.6f, overT_) * (1.f - smoothstepf(2.2f, 4.f, overT_));
    // the scene's setup and teardown: the projector switched on (its lamp warming up from a dull filament, then its
    // iris opening on the wall), and switched off (the iris closing, the lamp cooling down)
    const float lamp = std::min(smoothstepf(0.f, 0.5f, f.setup), 1.f - smoothstepf(0.4f, 0.95f, f.teardown));
    const float iris = 0.12f + 0.88f * std::min(smoothstepf(0.25f, 0.8f, f.setup), 1.f - smoothstepf(0.05f, 0.7f, f.teardown));
    intensity *= lamp * lamp;
    vec3 light1 = lin(V.light) * intensity * lerp(vec3(1.f, 0.35f, 0.08f), vec3(1.f), lamp);
    light1 = lerp(light1, MOON * 0.8f * intensity, 0.75f * slow_);
    // a peak warms the tungsten a touch
    light1 = light1 * lerp(vec3(1.f), vec3(1.05f, 1.f, 0.92f), peak * (variant_ == 1 ? 0.3f : 1.f));
    const vec3 light2 = lin(V.light2) * 0.5f * intensity;

    // ---- The clears: lifted blocks, shrinking shadows (at the real speed, even in slow motion).
    for (auto& s : shrinks_) s.t += dt;
    shrinks_.erase(std::remove_if(shrinks_.begin(), shrinks_.end(), [](const Shrink& s) { return s.t > 0.5f; }), shrinks_.end());
    std::vector<float> fly, flyR, flyS;
    for (auto& fl : flights_) {
        fl.t += dt;
        if (fl.t >= fl.dur) { pile_[fl.dest] = std::min(fl.dest >= 7 ? BOX_MAX : TRAY_MAX, pile_[fl.dest] + 1); reserved_[fl.dest]--; fl.type = -1; }
    }
    flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& fl) { return fl.type < 0; }), flights_.end());
    for (auto& fl : flights_) {
        // lifted straight out of the well toward the lamp's side, then carried in an arc and set down
        const float t = std::max(0.f, fl.t);
        const float lift = 0.28f;
        vec3 p;
        float ang = 0.f, sc = 1.f;
        if (t < lift) {
            const float u = easeInOut(t / lift);
            p = fl.from + vec3(0.f, 0.35f, 1.8f) * u;
        } else {
            const float u = easeInOut((t - lift) / (fl.dur - lift));
            const vec3 a = fl.from + vec3(0.f, 0.35f, 1.8f);
            p = lerp(a, fl.to, u);
            p.y += std::sin(u * PI) * (3.f + 0.15f * std::fabs(fl.to.x - a.x));
            ang = fl.spin * std::sin(u * PI * 0.5f) * (1.f - u) * 2.f;
            sc = lerpf(1.f, 0.5f * PILE_S / 0.42f, u);
        }
        fly.insert(fly.end(), {p.x, p.y, p.z, (float)fl.type});
        flyR.insert(flyR.end(), {fl.axis.x, fl.axis.y, fl.axis.z, ang});
        flyS.push_back(sc);
    }
    std::vector<float> clr;
    for (auto& s : shrinks_) {
        const float u = saturate(s.t / 0.42f);
        const float sc = 1.f - u * u * (3.f - 2.f * u);
        clr.insert(clr.end(), {s.x, s.y, s.t < 0.f ? 1.f : sc, (float)s.type});
    }

    // ---- The board.
    uploadBoard(game);
    if (bare) { for (float& v : piece_) v = 0.f; for (float& v : ghost_) v = 0.f; prev_.clear(); }

    vec3 gelShadow[7], gelModel[7];
    for (int i = 0; i < 7; i++) {
        gelModel[i] = lin(V.gel[i]);
        // (the light's own tint is partly taken out of the glass's: the seven hues stay apart under a warm or a
        // cold lamp)
        const vec3 lt = lin(V.light);
        const float mx = std::max(lt.x, std::max(lt.y, lt.z));
        const vec3 comp(std::pow(mx / std::max(0.05f, lt.x), 0.7f), std::pow(mx / std::max(0.05f, lt.y), 0.7f), std::pow(mx / std::max(0.05f, lt.z), 0.7f));
        gelShadow[i] = gelModel[i] * comp * 0.6f;
        gelShadow[i] = vec3(std::min(gelShadow[i].x, 1.1f), std::min(gelShadow[i].y, 1.1f), std::min(gelShadow[i].z, 1.1f));
    }
    const float circle1[4] = {0.f, 0.3f, 13.6f * iris, 12.9f * iris};
    const float circle2[4] = {2.6f, -0.2f, 14.6f * iris, 13.6f * iris};
    const float sway = std::sin((float)(music.hasSong ? music.beat : worldTime_ * 2.0) * TAU / 32.f);
    const vec2 echoOff{(2.1f + 0.3f * sway) * drop_, 0.15f * drop_};
    const vec2 echoScale{1.f + 0.05f * drop_, 1.f + 0.1f * drop_};

    // ---- The wall.
    sceneT_.bind();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(progWall_);
    setCommon(progWall_);
    set2f(progWall_, "uViewC", viewC_.x, viewC_.y);
    set2f(progWall_, "uViewH", viewH_.x, viewH_.y);
    set3f(progWall_, "uLight1", light1);
    set3f(progWall_, "uLight2", light2);
    set3f(progWall_, "uWall", lin(V.wall));
    set3f(progWall_, "uAmbient", lin(V.ambient) + light1 * 0.075f);
    set4f(progWall_, "uCircle1", circle1[0], circle1[1], circle1[2], circle1[3]);
    set4f(progWall_, "uCircle2", circle2[0], circle2[1], circle2[2], circle2[3]);
    set1f(progWall_, "uDrop", drop_);
    set1f(progWall_, "uWallKind", V.wallKind);
    set1f(progWall_, "uBare", bare ? 1.f : 0.f);
    set1f(progWall_, "uTime", worldTime_);
    glUniform3fv(ugl::U(progWall_, "uGel"), 7, &gelShadow[0].x);
    glUniform4fv(ugl::U(progWall_, "uGhost"), 4, ghost_);
    set1f(progWall_, "uGhostA", bare ? 0.f : 0.55f);
    const int prevN = std::min(16, (int)prev_.size() / 4);
    if (prevN) glUniform4fv(ugl::U(progWall_, "uPrev"), prevN, prev_.data());
    set1i(progWall_, "uPrevN", prevN);
    const int clrN = std::min(40, (int)clr.size() / 4);
    if (clrN) glUniform4fv(ugl::U(progWall_, "uClr"), clrN, clr.data());
    set1i(progWall_, "uClrN", clrN);
    set2f(progWall_, "uEchoOff", echoOff.x, echoOff.y);
    set2f(progWall_, "uEchoScale", echoScale.x, echoScale.y);
    set2f(progWall_, "uLens", lensScreen_.x, lensScreen_.y);
    ugl::drawFullscreen();

    // ---- The model, in its corner.
    modelT_.bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(progModel_);
    setCommon(progModel_);
    set4f(progModel_, "uRect", -1.f, -1.f, 1.f, 1.f);
    set3f(progModel_, "uEye", eye_);
    set3f(progModel_, "uFwd", fwd_);
    set3f(progModel_, "uRight", right_);
    set3f(progModel_, "uUp", up_);
    set2f(progModel_, "uTan", tan_.x, tan_.y);
    set3f(progModel_, "uLensPos", lensPos_);
    set3f(progModel_, "uLensDir", lensDir_);
    set3f(progModel_, "uLight1", light1);
    set3f(progModel_, "uLight2", light2 * 1.6f);
    set3f(progModel_, "uL2Pos", vec3(22.f, 8.f, 18.f));
    set1f(progModel_, "uDrop", drop_);
    set1f(progModel_, "uBare", bare ? 1.f : 0.f);
    set1f(progModel_, "uTime", worldTime_);
    glUniform3fv(ugl::U(progModel_, "uGelC"), 7, &gelModel[0].x);
    set3f(progModel_, "uWood", lin(V.wood));
    set3f(progModel_, "uTable", lin(V.table));
    set3f(progModel_, "uBody", lin(V.body));
    set3f(progModel_, "uRoom", lin(V.room) + light1 * 0.006f);
    set1f(progModel_, "uTableKind", V.tableKind);
    const int flyN = (int)flyS.size();
    if (flyN) {
        glUniform4fv(ugl::U(progModel_, "uFly"), flyN, fly.data());
        glUniform4fv(ugl::U(progModel_, "uFlyR"), flyN, flyR.data());
        glUniform1fv(ugl::U(progModel_, "uFlyS"), flyN, flyS.data());
    }
    set1i(progModel_, "uFlyN", flyN);
    float piles[28];
    for (int i = 0; i < 7; i++) {
        const vec2 c = pileCenter(i);
        piles[i * 4 + 0] = c.x; piles[i * 4 + 1] = c.y; piles[i * 4 + 2] = (float)pile_[i]; piles[i * 4 + 3] = (float)i;
    }
    glUniform4fv(ugl::U(progModel_, "uPile"), 7, piles);
    set4f(progModel_, "uBasket", BOX_C.x, BOX_C.y, (float)pile_[7], 0.f);
    set1f(progModel_, "uLensGlow", lensGlow_ + 0.6f * quadFlash_);
    glBindVertexArray(rectVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // composite it over the wall
    sceneT_.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progCompose_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, modelT_.tex);
    set1i(progCompose_, "uTex", 0);
    auto ndcX = [&](float bx) { return (bx - viewC_.x) / viewH_.x; };
    auto ndcY = [&](float by) { return (by - viewC_.y) / viewH_.y; };
    set4f(progCompose_, "uRect", ndcX(modelRect_[0]), ndcY(modelRect_[1]), ndcX(modelRect_[2]), ndcY(modelRect_[3]));
    glBindVertexArray(rectVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // the beam's haze and dust
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progHaze_);
    set2f(progHaze_, "uViewC", viewC_.x, viewC_.y);
    set2f(progHaze_, "uViewH", viewH_.x, viewH_.y);
    set2f(progHaze_, "uLens", lensScreen_.x, lensScreen_.y);
    set4f(progHaze_, "uCircle1", circle1[0], circle1[1], circle1[2], circle1[3]);
    set3f(progHaze_, "uLight1", light1);
    set1f(progHaze_, "uTime", worldTime_);
    set1f(progHaze_, "uSwirl", swirl_);
    set1f(progHaze_, "uSwirlAmp", 0.3f + swirlAmp_);
    set1f(progHaze_, "uDust", saturate(0.3f + 0.35f * lv_ + 0.3f * quadFlash_));
    set1f(progHaze_, "uHaze", 0.65f + 0.25f * lv_);
    set1f(progHaze_, "uLensGlow", lensGlow_ + 0.6f * quadFlash_);
    ugl::drawFullscreen();
    glDisable(GL_BLEND);

    // ---- The final pass, then the HUD.
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width_, height_);
    glUseProgram(progFinal_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progFinal_, "uScene", 0);
    const float fade = (1.f - 0.5f * dimT_) * saturate(f.fade);
    set1f(progFinal_, "uMul", fade);
    set1f(progFinal_, "uTime", time_);
    set1f(progFinal_, "uExposure", 1.f);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    hudMul_ = fade;
    if (!bare) drawHud(f.panels);
    glDisable(GL_BLEND);
    (void)lastHalf_;
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_shadow() {
    static const UniverseEntry e{
        "SHADOW", "a little model of the game on a table, lit by a projector: the playfield is its shadow on the wall",
        [] { return std::unique_ptr<Universe>(new ShadowUniverse()); },
        // A quiet, cinematic world: suits most songs, a little more the moodier ones.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.brightness) + 0.1f * l.dynamics; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

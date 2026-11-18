// SONAR: a dark sea where shapes surface from the black when an echo returns from them.
//
// One world: a teal murk over sunken temple ruins (weathered stone in the water's own light, shafts from the
// surface, motes, streams of bubbles, small schools of fish crossing the glide), where each echo paints what it
// finds as a ragged patch of dense glowing dots with a bright rim. The well is a pane of dark glass; the pieces are
// cubes of colored glass with bubbles rising inside; a wireframe submarine sits above the corner lettering. It is a true 3D scene
// (scene.cpp: a seabed of long dunes, rocks, a wreck and a sunken city, one static mesh drawn in perspective with a
// depth buffer) and the camera glides through it, behind the well, along a closed loop: down a colonnade, under two
// arches, past a tower, a dome, a stepped pyramid, a street of towers and a round temple, over the wreck. The pings
// run on the beat grid (every four beats in calm parts, every two from a chorus on), each one around a single
// point in view, never a sweep and never a wave across the screen: the surfaces around that point that face it
// glow up softly in the variant's echo color, their edges as fine lines of light, and fade back over a couple of
// seconds, the particles in the water glinting as the echo passes.
//
// The pieces are solid blocks lit from within, one cube per cell, crisp (no glow around them), each of the seven
// types its own clean color. **The chart grows with the lines**: a cleared row's blocks sink into the dark (gone in
// under half a second, at the real speed even in slow motion) and their light sinks on, as one point, to a spot of
// the seabed in view (later now and then a building), which stays charted for the rest of the song: its edges
// faintly lit for good, the seabed's depth contours drawn there. A quad charts four spots at once and lets the
// whole city glow up for a moment.
//
// The song: calm is the dark sea, a slow glide and a slow ping now and then on the seabed; mid pings every two
// beats, the wreck and the city among them; a peak pings both sides at once, mostly the city, which glows faintly
// out of the black for the whole peak, its windows lit by the kicks, and the glide quickens a little. The drop,
// eased in over two beats, raises the whole sunken world out of the dark in the drop's own echo color (its edges
// and the seabed's contours) and the glide surges to about twice the peak's pace, eased up and down by a critically
// damped spring over a couple of seconds, then settles back over the following bars. Slow motion is silent running
// (the echoes linger, the glide slows, one cold light); game over sinks the stack into the dark from the top, the
// chart fades and the glide drifts to a halt. Before a song the camera sinks down into its glide from above, the
// seabed's contours glowing up softly as it nears them, then settling; after it the camera rises away into the dark
// and the chart fades.
//
// Layout contract: the well, the blocks and the HUD are drawn through a flat orthographic view at the reference
// camera's scale on the board plane, and never move; only the world behind them has its own moving camera.
#include "universes/sonar/sonar.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "layout.hpp"
#include "universes/registry.hpp"
#include "universes/sonar/shaders.hpp"

namespace {

// Tenor Sans (capitals, numbers) and Cormorant Garamond Italic (the song, the notices), SIL Open Font License,
// see OFL-TenorSans.txt and OFL-CormorantGaramond.txt.
const unsigned char FONT_CAPS[] = {
#include "universes/sonar/TenorSans-Regular.ttf.inc"
};
const unsigned char FONT_IT[] = {
#include "universes/sonar/CormorantGaramond-Italic.ttf.inc"
};

namespace shaders = sonar_shaders;
using ugl::set1f;
using ugl::set1i;
using ugl::set2f;
using ugl::set3f;
using ugl::set4f;
using ugl::setMat;

vec3 hex(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
vec3 lin(unsigned v) { return ugl::toLinear(hex(v)); }

// A sea (sRGB): the water (top, low), the stone in its light, the echo (seabed, city), the drop's echo, the seven
// pieces (I O T S Z J L). The water is always a teal murk, leaning toward the sea's own hue.
struct VariantDef {
    const char* name;
    unsigned waterTop, waterLow, stone, echo, echoCity, dropEcho;
    unsigned pieces[7];
};
const VariantDef VARIANTS[] = {
    {"TRENCH", 0x0e2c3c, 0x040b12, 0x34525e, 0x3ed6c6, 0x4aa2e6, 0xa08cff,
     {0x4fe0f0, 0xf2d36b, 0xa995ff, 0x5fe39a, 0xff7070, 0x4f8dff, 0xff9d4f}},
    {"AMBER", 0x12353a, 0x051113, 0x3a5856, 0xffb25a, 0xe88a52, 0xff7466,
     {0x9fe3e0, 0xffe7a6, 0xe08fbe, 0xc9d97a, 0xff6a4a, 0x8f9cf0, 0xffb347}},
    {"ICE", 0x15323f, 0x060f16, 0x405c66, 0xbfe2ff, 0x90b6ff, 0x9ef0d6,
     {0x8ef0ff, 0xfff0a0, 0xd6b0ff, 0xa8f0c0, 0xffa0b2, 0x92b0ff, 0xffc492}},
    {"VIOLET", 0x14223c, 0x060812, 0x3c4660, 0xa88cff, 0xd07ae8, 0x5ae0d0,
     {0x3fd0ff, 0xffc94a, 0xff5fd8, 0x4fe0a8, 0xff4f6f, 0x7a6bff, 0xff8f4a}},
    {"JADE", 0x0f3532, 0x04110e, 0x355a52, 0x6ef0a2, 0x40cfc0, 0xf0e070,
     {0x7ff0e0, 0xf5f0b0, 0xc6a0ff, 0x9ff060, 0xff7a8a, 0x60a8ff, 0xffc060}},
    {"CORAL", 0x123236, 0x061012, 0x3e5856, 0xff8c8c, 0xff9ac6, 0xffd282,
     {0x6fe0e6, 0xffe0a0, 0xc8a8ff, 0xa4e6a8, 0xff5070, 0x8aa4ff, 0xff9a78}},
};
constexpr int VARIANT_COUNT = (int)(sizeof(VARIANTS) / sizeof(VARIANTS[0]));

constexpr float FOV_Y = 52.f;      // the world camera's vertical field of view (degrees)
constexpr float CAM_AHEAD = 9.f;   // it looks at the loop this far ahead, a little down
constexpr int SNOW_N = 1600;       // particles in the water
constexpr int BUBBLE_N = 420;      // of which drawn again as bubbles rising in streams
constexpr int SCHOOLS = 4;         // schools of fish about

constexpr float SINK_T = 0.4f;     // a cleared row's blocks are gone in this long
constexpr float OVER_T = 1.1f;     // game over: each block sinking
constexpr float MOTE_T = 1.5f;     // a cleared row's light sinking to the spot it charts

// A VAO over the unit quad with `n` per-instance vec4 attributes (locations 1..n) of `stride` bytes.
void instVao(GLuint quad, GLuint& vao, GLuint& vbo, int n, size_t stride) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < n; i++) {
        glEnableVertexAttribArray(1 + i);
        glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(sizeof(float) * 4 * i));
        glVertexAttribDivisor(1 + i, 1);
    }
}

} // namespace

// =====================================================================================================
// Setup
// =====================================================================================================

bool SonarUniverse::init(int width, int height) {
    using shaders::withCommon;
    progWater_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::WATER_FS, true).c_str(), "sonar water");
    progMesh_ = ugl::program(shaders::MESH_VS, withCommon(shaders::MESH_FS, true).c_str(), "sonar world");
    progSnow_ = ugl::program(withCommon(shaders::SNOW_VS, true).c_str(), withCommon(shaders::SNOW_FS, true).c_str(), "sonar snow");
    progWell_ = ugl::program(ugl::FULLSCREEN_VS, withCommon(shaders::WELL_FS).c_str(), "sonar well");
    progPiece_ = ugl::program(shaders::PIECE_VS, withCommon(shaders::PIECE_FS).c_str(), "sonar blocks");
    progMote_ = ugl::program(shaders::MOTE_VS, shaders::MOTE_FS, "sonar motes");
    progDown_ = ugl::program(ugl::FULLSCREEN_VS, shaders::DOWN_FS, "sonar down");
    progBlur_ = ugl::program(ugl::FULLSCREEN_VS, shaders::BLUR_FS, "sonar blur");
    progFinal_ = ugl::program(ugl::FULLSCREEN_VS, shaders::FINAL_FS, "sonar final");
    progShape_ = ugl::program(shaders::SHAPE_VS, shaders::SHAPE_FS, "sonar shape");
    progFish_ = ugl::program(withCommon(shaders::FISH_VS, true).c_str(), withCommon(shaders::FISH_FS, true).c_str(), "sonar fish");

    const float quad[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    instVao(quadVbo_, pieceVao_, pieceVbo_, 3, sizeof(Inst));
    instVao(quadVbo_, moteVao_, moteVbo_, 2, sizeof(MoteInst));
    instVao(quadVbo_, fishVao_, fishVbo_, 2, sizeof(Inst));
    glGenVertexArrays(1, &shapeVao_);
    glGenBuffers(1, &shapeVbo_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 2));
    // the sunken world: one static mesh
    scene_.build();
    glGenVertexArrays(1, &meshVao_);
    glGenBuffers(1, &meshVbo_);
    glGenBuffers(1, &meshIbo_);
    glBindVertexArray(meshVao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(scene_.verts.size() * sizeof(sonar_scene::Vtx)), scene_.verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(scene_.idx.size() * sizeof(uint32_t)), scene_.idx.data(), GL_STATIC_DRAW);
    {
        const int sizes[4] = {3, 3, 4, 4};
        size_t off = 0;
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, (GLsizei)sizeof(sonar_scene::Vtx), (void*)off);
            off += sizeof(float) * sizes[i];
        }
    }
    meshCount_ = (GLsizei)scene_.idx.size();
    scene_.verts.clear();
    scene_.verts.shrink_to_fit();
    scene_.idx.clear();
    scene_.idx.shrink_to_fit();
    // particles in the water
    {
        std::vector<float> pts(SNOW_N * 4);
        Rng r(0x5A0Full);
        for (float& v : pts) v = r.uniform();
        glGenVertexArrays(1, &snowVao_);
        glGenBuffers(1, &snowVbo_);
        glBindVertexArray(snowVao_);
        glBindBuffer(GL_ARRAY_BUFFER, snowVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(float)), pts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        snowCount_ = SNOW_N;
    }
    glBindVertexArray(0);

    resize(width, height);
    SongLook none;
    none.variant = 0;
    setSong(none, 0.f);
    return glGetError() == GL_NO_ERROR;
}

void SonarUniverse::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    const float aspect = (float)width_ / height_;
    const layout::Camera cam = layout::camera(width_, height_);
    const float hh = cam.dist * std::tan(cam.fovDeg * 0.5f * PI / 180.f), hw = hh * aspect;
    viewH_ = vec2{hw, hh};
    viewC_ = vec2{cam.target.x, cam.target.y};
    vp_ = mat4::ortho(viewC_.x - hw, viewC_.x + hw, viewC_.y - hh, viewC_.y + hh, -1.f, 1.f);
    sceneT_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, true);
    const int qw = std::max(1, width_ / 4), qh = std::max(1, height_ / 4);
    bloomA_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
    bloomB_.create(qw, qh, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
}

SonarUniverse::~SonarUniverse() {
    for (ugl::Target* t : {&sceneT_, &bloomA_, &bloomB_}) t->destroy();
    for (GLuint p : {progWater_, progMesh_, progSnow_, progWell_, progPiece_, progMote_, progDown_, progBlur_, progFinal_, progShape_, progFish_})
        glDeleteProgram(p);
    for (GLuint v : {pieceVao_, moteVao_, shapeVao_, meshVao_, snowVao_, fishVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {quadVbo_, pieceVbo_, moteVbo_, shapeVbo_, meshVbo_, meshIbo_, snowVbo_, fishVbo_}) glDeleteBuffers(1, &b);
}

// =====================================================================================================
// The camera, the echoes, the chart
// =====================================================================================================

// The glide along the loop: the speed follows its target through a critically damped spring (it eases up and down
// over a couple of seconds, never a jerk), and the path parameter integrates it (never a jump).
void SonarUniverse::updateCamera(float wdt, float targetSpeed, float dt) {
    const float w = 1.6f;
    if (dt > 0.f) {
        const float a = w * w * (targetSpeed - speed_) - 2.f * w * speedV_;
        speedV_ += a * dt;
        speed_ += speedV_ * dt;
    }
    pathS_ += speed_ * wdt;
    if (pathS_ > scene_.loopLength()) pathS_ -= scene_.loopLength();
    camPos_ = scene_.path(pathS_);
    vec3 ahead = scene_.path(pathS_ + CAM_AHEAD);
    ahead.y -= 2.6f;
    // (above the glide: looking down a little more)
    camPos_.y += rise_;
    ahead.y += 0.55f * rise_;
    const vec3 up(0.f, 1.f, 0.f);
    camF_ = normalize(ahead - camPos_);
    camR_ = normalize(cross(camF_, up));
    camU_ = cross(camR_, camF_);
    const float aspect = (float)width_ / height_;
    wTan_ = std::tan(FOV_Y * 0.5f * PI / 180.f);
    wvp_ = mat4::perspective(FOV_Y * PI / 180.f, aspect, 0.3f, 260.f) * mat4::lookAt(camPos_, camPos_ + camF_, up);
}

bool SonarUniverse::toNdc(const vec3& p, vec2& ndc, float& depth) const {
    const vec4 c = wvp_ * vec4(p, 1.f);
    if (c.w < 0.3f) return false;
    ndc = vec2{c.x / c.w, c.y / c.w};
    depth = c.w;
    return true;
}

bool SonarUniverse::toBoard(const vec3& p, vec2& board) const {
    vec2 n;
    float d;
    if (!toNdc(p, n, d)) return false;
    board = vec2{viewC_.x + n.x * viewH_.x, viewC_.y + n.y * viewH_.y};
    return true;
}

bool SonarUniverse::seabedAt(float nx, float ny, vec3& hit) const {
    const float aspect = (float)width_ / height_;
    const vec3 d = normalize(camF_ + camR_ * (nx * wTan_ * aspect) + camU_ * (ny * wTan_));
    float prev = 0.5f;
    for (float t = 0.5f; t < 45.f; t += 0.3f) {
        const vec3 p = camPos_ + d * t;
        if (p.y < scene_.height(p.x, p.z)) {
            float a = prev, b = t;
            for (int i = 0; i < 6; i++) {
                const float m = 0.5f * (a + b);
                const vec3 q = camPos_ + d * m;
                if (q.y < scene_.height(q.x, q.z)) b = m;
                else a = m;
            }
            hit = camPos_ + d * b;
            return true;
        }
        prev = t;
    }
    return false;
}

void SonarUniverse::ping(const vec3& p, float strength, float reach) {
    Ping& q = pings_[pingNext_];
    pingNext_ = (pingNext_ + 1) % 8;
    q.p = p;
    q.age = 0.f;
    q.strength = strength;
    q.reach = reach;
}

// A ping on one side of the screen (-1 left, 1 right), outside the well: a building in view (the city with some
// chance), a rock or the wreck, or a point of the seabed.
void SonarUniverse::pingSomewhere(int side, float strength, float reach, int maxLayer, float cityChance) {
    using namespace sonar_scene;
    const float wellN = 5.4f / viewH_.x;
    const bool city = maxLayer >= 2 && rng_.chance(cityChance);
    const bool object = city || rng_.chance(maxLayer >= 1 ? 0.45f : 0.25f);
    const int si = side < 0 ? 0 : 1;
    if (object) {
        std::vector<int> pool;
        for (int i = 0; i < (int)scene_.anchors.size(); i++) {
            const Anchor& a = scene_.anchors[i];
            if (city ? a.layer != CITY : (a.layer == CITY || (a.layer == WRECK && maxLayer < 1))) continue;
            vec2 n;
            float d;
            if (!toNdc(a.p, n, d) || d < 7.f || d > 40.f) continue;
            if (n.x * side < wellN + 0.02f || n.x * side > 0.97f || n.y < -0.95f || n.y > 0.9f) continue;
            if (i == lastAnchor_[si]) continue;
            pool.push_back(i);
        }
        if (!pool.empty()) {
            const int i = pool[rng_.irange(0, (int)pool.size() - 1)];
            lastAnchor_[si] = i;
            const vec3 j(rng_.range(-0.5f, 0.5f), rng_.range(-0.4f, 0.4f), rng_.range(-0.5f, 0.5f));
            ping(scene_.anchors[i].p + j, strength, reach * (scene_.anchors[i].layer == CITY ? 1.25f : 1.f));
            return;
        }
    }
    for (int k = 0; k < 8; k++) {
        vec3 hit;
        if (!seabedAt(side * rng_.range(wellN + 0.08f, 0.9f), rng_.range(-0.92f, 0.05f), hit)) continue;
        const float d = length(hit - camPos_);
        if (d < 8.f || d > 34.f) continue;
        ping(hit + vec3(0.f, 0.5f, 0.f), strength, reach);
        return;
    }
}

// Charts a spot in view on one side (a point of the seabed, now and then a building) after `delay` seconds;
// returns its index in charts_ (-1 if none could be found or the chart is full).
int SonarUniverse::chartSpot(int side, float delay) {
    using namespace sonar_scene;
    if (charts_.size() >= 64) return -1;
    const float wellN = 5.4f / viewH_.x;
    auto spaced = [&](const vec3& p) {
        for (const Chart& c : charts_)
            if (!c.fading && length(c.p - p) < 3.5f) return false;
        return true;
    };
    Chart c;
    bool ok = false;
    if (chartCount_ >= 4 && rng_.chance(0.3f)) {
        std::vector<int> pool;
        for (int i = 0; i < (int)scene_.anchors.size(); i++) {
            const Anchor& a = scene_.anchors[i];
            if (a.layer != CITY && a.layer != WRECK) continue;
            vec2 n;
            float d;
            if (!toNdc(a.p, n, d) || d < 4.f || d > 32.f) continue;
            if (n.x * side < wellN + 0.04f || n.x * side > 0.95f || std::fabs(n.y) > 0.9f || !spaced(a.p)) continue;
            pool.push_back(i);
        }
        if (!pool.empty()) {
            c.p = scene_.anchors[pool[rng_.irange(0, (int)pool.size() - 1)]].p;
            c.r = 6.f;
            ok = true;
        }
    }
    for (int k = 0; k < 16 && !ok; k++) {
        const int sd = k < 10 ? side : -side;
        vec3 hit;
        if (!seabedAt(sd * rng_.range(wellN + 0.1f, 0.82f), rng_.range(-0.88f, -0.2f), hit)) continue;
        if (length(hit - camPos_) > 30.f || !spaced(hit)) continue;
        c.p = hit + vec3(0.f, 0.3f, 0.f);
        c.r = 5.f;
        ok = true;
    }
    if (!ok) return -1;
    c.delay = delay;
    charts_.push_back(c);
    chartCount_++;
    return (int)charts_.size() - 1;
}

// Schools of fish: each is born ahead along the loop, off to one side, a few units above the seabed, and swims
// across the glide's path at its own slow pace, turning gently; its fish keep loose places around it, each with
// its own slow sway and tail beat. A school the camera has passed (or that has lived its time) leaves, another is
// born ahead.
void SonarUniverse::updateFish(float wdt) {
    for (School& sc : schools_) {
        sc.age += wdt;
        sc.turn += wdt * 0.35f;
        const float yaw = 0.18f * std::sin(sc.turn) * wdt;
        const float cy = std::cos(yaw), sy = std::sin(yaw);
        sc.v = vec3(sc.v.x * cy - sc.v.z * sy, sc.v.y, sc.v.x * sy + sc.v.z * cy);
        sc.c = sc.c + sc.v * wdt;
        const float floorY = scene_.height(sc.c.x, sc.c.z) + 1.2f;
        if (sc.c.y < floorY) sc.c.y += (floorY - sc.c.y) * std::min(1.f, wdt * 1.5f);
    }
    schools_.erase(std::remove_if(schools_.begin(), schools_.end(),
                                  [&](const School& sc) {
                                      const vec3 r = sc.c - camPos_;
                                      return sc.age > sc.life || (sc.age > 3.f && dot(r, camF_) < -4.f) || length(r) > 70.f;
                                  }),
                   schools_.end());
    schoolClock_ -= wdt;
    if ((int)schools_.size() < SCHOOLS && schoolClock_ <= 0.f) {
        schoolClock_ = rng_.range(2.f, 6.f);
        School sc;
        const float ahead = rng_.range(14.f, 30.f);
        const vec3 a = scene_.path(pathS_ + ahead), b = scene_.path(pathS_ + ahead + 1.f);
        vec3 t = b - a;
        t.y = 0.f;
        t = normalize(t);
        const vec3 side(t.z, 0.f, -t.x);
        const float sd = rng_.chance(0.5f) ? 1.f : -1.f;
        sc.c = a + side * (sd * rng_.range(3.f, 11.f));
        sc.c.y = scene_.height(sc.c.x, sc.c.z) + rng_.range(2.f, 5.5f);
        // across the path, toward the other side, a little along it
        const float sp = rng_.range(0.6f, 1.2f);
        sc.v = normalize(side * (-sd) + t * rng_.range(-0.5f, 0.5f)) * sp;
        sc.life = rng_.range(30.f, 50.f);
        sc.turn = rng_.range(0.f, 6.f);
        const int n = rng_.irange(6, 12);
        const float big = rng_.range(0.11f, 0.19f);
        for (int i = 0; i < n; i++) {
            Fish fh;
            fh.off = vec3(rng_.range(-1.6f, 1.6f), rng_.range(-0.6f, 0.6f), rng_.range(-1.4f, 1.4f));
            fh.wob = vec3(rng_.range(0.f, 6.f), rng_.range(0.f, 6.f), rng_.range(0.25f, 0.45f));
            fh.ph = rng_.range(0.f, 6.f);
            fh.rate = rng_.range(0.8f, 1.3f);
            fh.size = big * rng_.range(0.75f, 1.15f);
            fh.tail = rng_.range(0.f, 6.f);
            sc.fish.push_back(fh);
        }
        schools_.push_back(sc);
    }
    fishInst_.clear();
    for (School& sc : schools_) {
        const float in = smoothstepf(0.f, 3.f, sc.age) * (1.f - smoothstepf(sc.life - 4.f, sc.life, sc.age));
        const float sp = length(sc.v);
        for (Fish& fh : sc.fish) {
            fh.ph += wdt * fh.rate * 0.6f;
            fh.tail += wdt * (6.f + 5.f * sp) * fh.rate;
            const float A = fh.wob.z;
            const vec3 sway(A * std::sin(fh.ph + fh.wob.x), 0.4f * A * std::sin(fh.ph * 1.3f + fh.wob.y), A * std::cos(fh.ph * 0.8f + fh.wob.x));
            const vec3 swayV(A * std::cos(fh.ph + fh.wob.x), 0.52f * A * std::cos(fh.ph * 1.3f + fh.wob.y), -0.8f * A * std::sin(fh.ph * 0.8f + fh.wob.x));
            const vec3 p = sc.c + fh.off + sway;
            const vec3 h = sc.v + swayV * (fh.rate * 0.6f);
            fishInst_.push_back({{p.x, p.y, p.z, fh.size * in}, {h.x, h.y, h.z, fh.tail}, {0, 0, 0, 0}});
        }
    }
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void SonarUniverse::setSong(const SongLook& t, float seconds) {
    int v;
    if (t.variant >= 0) v = t.variant % VARIANT_COUNT;
    else {
        float h = std::fmod(t.hue / TAU, 1.f);
        if (h < 0) h += 1.f;
        if (t.hasSong && t.minor && t.brightness < 0.42f) v = 3;                               // violet
        else if (t.hasSong && t.bassWeight > 0.62f && t.dynamics > 0.5f) v = 1;                // amber
        else if (t.hasSong && t.airWeight > 0.6f && t.brightness > 0.55f) v = 2;               // ice
        else if (t.hasSong && !t.minor && t.brightness > 0.56f) v = 5;                         // coral
        else if (t.hasSong && t.minor) v = 0;                                                   // trench
        else v = (int)(h * VARIANT_COUNT) % VARIANT_COUNT;
        if (v == t.avoidVariant) v = (v + 1 + (int)(t.seed % 3)) % VARIANT_COUNT;
    }
    variant_ = v;
    const VariantDef& V = VARIANTS[v];
    waterTopT_ = lin(V.waterTop);
    waterLowT_ = lin(V.waterLow);
    stoneT_ = lin(V.stone);
    echoT_ = lin(V.echo);
    echoCityT_ = lin(V.echoCity);
    dropEchoT_ = lin(V.dropEcho);
    for (int i = 0; i < 7; i++) pieceColT_[i] = lin(V.pieces[i]);
    const bool instant = !colSet_ || seconds <= 0.1f;
    if (instant) {
        waterTop_ = waterTopT_;
        waterLow_ = waterLowT_;
        stone_ = stoneT_;
        echo_ = echoT_;
        echoCity_ = echoCityT_;
        dropEcho_ = dropEchoT_;
        for (int i = 0; i < 7; i++) pieceCol_[i] = pieceColT_[i];
        colSet_ = true;
    }
    seed_ = t.seed ? t.seed : 1;
    seedF_ = (float)((seed_ >> 20) % 1000) * 0.137f;
    rng_ = Rng(seed_ ^ 0x50A450ull);
    // a new song: nothing charted yet; the first one starts the glide somewhere along the loop (later ones glide on)
    if (instant) {
        charts_.clear();
        Rng r(seed_ ^ 0xBEDull);
        pathS_ = r.range(0.f, scene_.loopLength());
        speed_ = 0.85f;
        speedV_ = 0.f;
        schools_.clear();
    } else
        for (Chart& c : charts_) c.fading = true;
    chartCount_ = 0;
}

void SonarUniverse::setLevel(int level, float energy, float seconds, float impact) {
    level = std::clamp(level, 0, 2);
    (void)energy;
    (void)impact;
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
}

// The drop: the whole sunken city rises out of the dark in the drop's echo color (eased in: see render).
void SonarUniverse::drop(float strength) {
    dropMode_ = true;
    dropT_ = 0.f;
    swell_ = std::max(swell_, 0.6f + 0.4f * strength);
}

void SonarUniverse::refresh(float strength) {
    // a new section inside the same level: one echo from the city, the other side
    if (strength > 0.2f) pingSomewhere(-pingSide_, 0.7f, 5.5f, 2, 1.f);
}

vec2 SonarUniverse::project(const vec3& p) const { return layout::project(vp_, p, width_, height_); }

TransitionHints SonarUniverse::hints() const {
    TransitionHints h;
    h.background = lerp(waterTop_, waterLow_, 0.6f);
    h.light = false;
    h.ink = lerp(echo_, vec3(1.f), 0.2f);
    h.accent = echo_;
    h.styles = TS_LUMA | TS_INK | TS_RIPPLE;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void SonarUniverse::onEvent(const GameEvent& ev, const Game& game) {
    switch (ev.type) {
    case GameEvent::HardDrop: {
        // a faint glint of light at the contact, under the piece
        if (!game.hasPiece()) break;
        Piece gp = game.piece();
        gp.y = game.ghostY();
        int cells[4][2];
        game.pieceCells(gp, cells);
        int low = -1, minX = 99, maxX = -1;
        for (auto& c : cells) low = std::max(low, c[1]);
        for (auto& c : cells)
            if (c[1] == low) { minX = std::min(minX, c[0]); maxX = std::max(maxX, c[0]); }
        if (low < Game::HIDDEN) break;
        const vec3 a = layout::cellPos((float)minX, (float)low + 0.5f), b = layout::cellPos((float)maxX, (float)low + 0.5f);
        Mote m;
        m.p0 = m.p1 = m.p = vec2{(a.x + b.x) * 0.5f, a.y};
        m.life = 0.45f;
        m.size = 0.16f;
        m.stretch = 1.6f + (maxX - minX) * 1.4f;
        m.peak = 0.5f;
        m.col = pieceCol_[std::clamp(gp.type, 0, 6)];
        m.glint = true;
        motes_.push_back(m);
        break;
    }
    case GameEvent::Lock: {
        // the locked blocks keep the bubbles they had while falling
        for (auto& c : ev.cells) {
            if (c.y < 0 || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            for (int i = 0; i < fallN_; i++)
                if (fall_[i].x == c.x && fall_[i].y == c.y) seeds_[c.y][c.x] = fall_[i].seed;
        }
        fallN_ = 0;
        break;
    }
    case GameEvent::Clear: {
        // the blocks' seeds move down with their rows, like the game's
        {
            bool cleared[Game::H] = {};
            for (auto& c : ev.cells)
                if (c.y >= 0 && c.y < Game::H) cleared[c.y] = true;
            float nb[Game::H][Game::W] = {};
            int dst = Game::H - 1;
            for (int y = Game::H - 1; y >= 0; y--) {
                if (cleared[y]) continue;
                std::memcpy(nb[dst], seeds_[y], sizeof(seeds_[y]));
                dst--;
            }
            std::memcpy(seeds_, nb, sizeof(seeds_));
        }
        // the row's blocks sink into the dark; its light sinks on to a spot of the seabed, which stays charted
        std::vector<int> rows;
        for (auto& c : ev.cells)
            if (c.y >= Game::HIDDEN && c.y < Game::H && std::find(rows.begin(), rows.end(), c.y) == rows.end()) rows.push_back(c.y);
        std::sort(rows.begin(), rows.end());
        const bool quadClear = ev.count >= 4;
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            sinks_.push_back({p.x, p.y, pieceCol_[std::clamp(c.type, 0, 6)], 0.f, SINK_T, 0.012f * (float)(c.x % 3), 2});
        }
        for (size_t k = 0; k < rows.size(); k++) {
            const int side0 = ((int)k + chartCount_) % 2 ? 1 : -1;
            const int ci = chartSpot(side0, MOTE_T + 0.12f * k);
            const vec3 p = layout::cellPos(0.f, (float)rows[k]);
            Mote m;
            vec2 b{side0 * rng_.range(12.f, 17.f), -9.f};
            if (ci >= 0) {
                m.world = true;
                m.target = charts_[ci].p;
                toBoard(m.target, b);
            }
            const float side = b.x < 0.f ? -1.f : 1.f;
            m.p0 = m.p = vec2{side * 4.2f, p.y};
            m.p1 = b;
            m.delay = 0.08f + 0.1f * k;
            m.life = MOTE_T;
            m.size = 0.2f;
            m.peak = 0.9f;
            m.col = lerp(echo_, vec3(1.f), 0.25f);
            motes_.push_back(m);
        }
        if (quadClear) quadGlow_ = 1.f;
        break;
    }
    case GameEvent::TopOut: {
        // the stack sinks into the dark from the top; the chart fades
        overT_ = 0.f;
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            const float h = (float)((splitmix64((uint64_t)(c.x * 31 + c.y)) >> 40) & 255) / 255.f;
            sinks_.push_back({p.x, p.y, pieceCol_[std::clamp(c.type, 0, 6)], 0.f, OVER_T, 0.1f + 0.045f * (c.y - top) + 0.08f * h, 3});
        }
        for (Chart& c : charts_) c.fading = true;
        chartCount_ = 0;
        break;
    }
    default: break;
    }
}

// =====================================================================================================
// Geometry
// =====================================================================================================

void SonarUniverse::addCell(float x, float y, float half, int kind, const vec3& col, float alpha, float flash, float prog, float seed) {
    if (alpha <= 0.003f) return;
    cells_.push_back({{x, y, half, (float)kind}, {col.x, col.y, col.z, alpha}, {flash, prog, 1.f, seed}});
}

void SonarUniverse::buildBoard(const Game& g) {
    // the ghost: the blocks' edges where the piece will land
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
                addCell(w.x, w.y, 0.5f, 1, pieceCol_[p.type], 1.f, 0.f, 0.f);
            }
        }
    }
    // the stack
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) {
            const Cell& c = g.cell(x, y);
            if (c.type < 0) continue;
            const vec3 p = layout::cellPos((float)x, y - g.rowOffset(y));
            const float fl = std::pow(saturate(c.flash), 8.f);
            if (seeds_[y][x] == 0.f) seeds_[y][x] = (float)(x * 7 + y * 13 + c.type * 3) + 0.5f;
            addCell(p.x, p.y, 0.5f, 0, pieceCol_[c.type], 1.f, fl, 0.f, seeds_[y][x]);
        }
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            if (g.cell(x, y).type < 0) seeds_[y][x] = 0.f;
    // the falling piece: exactly a locked block at its place
    if (g.hasPiece() && !g.over()) {
        const Piece& p = g.piece();
        int cells[4][2];
        g.pieceCells(p, cells);
        const float dx = g.pieceVisualX() - p.x, dy = g.pieceVisualY() - p.y;
        // (a new piece: new seeds, kept by its blocks once it locks)
        if (fallN_ == 0 || fallType_ != p.type) pieceSeed_ += 4.f;
        fallType_ = p.type;
        int k = 0;
        for (auto& c : cells) {
            const vec3 w = layout::cellPos(c[0] + dx, c[1] + dy); // (whole above the well too: it spawns there)
            const float sd = pieceSeed_ + (float)k;
            fall_[k] = {c[0], c[1], sd};
            addCell(w.x, w.y, 0.5f, 0, pieceCol_[p.type], 1.f, 0.f, 0.f, sd);
            k++;
        }
        fallN_ = 4;
    }
    // the previews
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
        for (auto& c : cells) {
            const float ox = c[0] - (minx + maxx) * 0.5f, oy = c[1] - (miny + maxy) * 0.5f;
            addCell(center.x + ox * sc, center.y - oy * sc, 0.5f * sc, 0, pieceCol_[type], alpha, 0.f, 0.f, (float)(type * 5 + c[0] * 2 + c[1]) + 200.5f);
        }
    };
    preview(g.holdType(), vec2{layout::HOLD_CENTER.x, layout::HOLD_CENTER.y}, layout::previewScale(0), g.holdUsed() ? 0.4f : 1.f);
    for (int i = 0; i < 3; i++) {
        const vec3 c = layout::nextCenter(i);
        if (g.next(i) >= 0) preview(g.next(i), vec2{c.x, c.y}, layout::previewScale(i), 1.f);
    }
}

void SonarUniverse::drawInst(GLuint prog, GLuint vao, GLuint vbo, const void* data, size_t bytes, size_t count) {
    if (!count) return;
    glUseProgram(prog);
    setMat(prog, "uVP", vp_);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STREAM_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)count);
}

// =====================================================================================================
// Text and the HUD
// =====================================================================================================

void SonarUniverse::flushShapes() {
    if (shapes_.empty()) return;
    glUseProgram(progShape_);
    set2f(progShape_, "uRes", (float)width_, (float)height_);
    glBindVertexArray(shapeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, shapeVbo_);
    glBufferData(GL_ARRAY_BUFFER, shapes_.size() * sizeof(float), shapes_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(shapes_.size() / 6));
    shapes_.clear();
}

void SonarUniverse::drawHud(const std::vector<HudRect>& panels) {
    // the panels on a soft halo of the dark water; the accent is the echo
    const vec3 halo = ugl::toSrgb(waterLow_) * 0.6f;
    const vec3 accent = lerp(ugl::toSrgb(lerp(echo_, dropEcho_, fest_ * 0.8f)), vec3(1.f), 0.2f);
    if (!panels.empty() && !bare_) {
        for (const HudRect& r : panels) {
            const vec3 c = r.custom ? r.color : r.accent ? accent : halo;
            const float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h, a = std::min(1.f, r.alpha);
            const float qd[6][6] = {{x0, y0, c.x, c.y, c.z, a}, {x1, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a},
                                    {x0, y0, c.x, c.y, c.z, a}, {x1, y1, c.x, c.y, c.z, a}, {x0, y1, c.x, c.y, c.z, a}};
            for (auto& v : qd) shapes_.insert(shapes_.end(), v, v + 6);
        }
        flushShapes();
    }
}


// A fine line in pixels (top-left origin), feathered by a pixel on both sides.
void SonarUniverse::line(float x0, float y0, float x1, float y1, float w, const vec3& c, float a) {
    float dx = x1 - x0, dy = y1 - y0;
    const float l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-3f || a <= 0.002f) return;
    dx /= l;
    dy /= l;
    const float nx = -dy, ny = dx;
    const float h = 0.5f * w, f = h + 1.f;
    auto v = [&](float x, float y, float al) { const float q[6] = {x, y, c.x, c.y, c.z, al}; shapes_.insert(shapes_.end(), q, q + 6); };
    auto band = [&](float o0, float a0, float o1, float a1) {
        v(x0 + nx * o0, y0 + ny * o0, a0); v(x1 + nx * o0, y1 + ny * o0, a0); v(x1 + nx * o1, y1 + ny * o1, a1);
        v(x0 + nx * o0, y0 + ny * o0, a0); v(x1 + nx * o1, y1 + ny * o1, a1); v(x0 + nx * o1, y0 + ny * o1, a1);
    };
    band(-f, 0.f, -h, a);
    band(-h, a, h, a);
    band(h, a, f, 0.f);
}

// The wireframe submarine above the corner lettering: a side view of a long hull (blunt bow to the right, the
// stern tapering to its cruciform fins and screw), its sail and masts, its longitudinal lines and ribs in a fine
// pale ink. A sonar sweep runs along it on the beat grid, in the echo's color.
void SonarUniverse::drawSub(const Frame& f) {
    const float H = (float)height_;
    float bottom = 0.885f * H, left = 0.031f * H;
    if (f.hudBoxes && f.hudBoxes->size() == f.hud.size()) {
        float top = 1e9f;
        for (size_t i = 0; i < f.hud.size(); i++) {
            const HudText& t = f.hud[i];
            if ((t.role == HUD_SECTION || t.role == HUD_UNIVERSE) && t.x >= 0.f && t.y > 0.55f * H) {
                top = std::min(top, (*f.hudBoxes)[i].y);
                left = (*f.hudBoxes)[i].x;
            }
        }
        if (top < 1e8f) bottom = top - 0.022f * H;
    }
    const float L = 0.245f * H, s = H / 1080.f;
    const float R = 0.068f;      // hull radius (in hull lengths)
    const float cy = bottom - (R + 0.006f) * L;
    auto rad = [&](float u) {
        if (u > 0.86f) { const float k = (u - 0.86f) / 0.14f; return R * std::sqrt(std::max(0.f, 1.f - k * k)); }
        if (u < 0.3f) { const float k = std::max(0.f, (u - 0.04f) / 0.26f); return R * (0.16f + 0.84f * std::sqrt(k * (2.f - k))); }
        return R;
    };
    const vec3 ink(0.78f, 0.82f, 0.85f);
    const vec3 ec = lerp(ugl::toSrgb(lerp(echo_, dropEcho_, fest_ * 0.8f)), vec3(1.f), 0.15f);
    const float scan = (float)std::fmod(beat_ / 4.0, 1.0) * 1.5f - 0.25f; // the sweep's position along the hull
    const float A = (1.f - smoothstepf(0.3f, 0.8f, bridge_.amount)) * (bare_ ? 0.6f : 1.f) * (1.f - 0.5f * dimT_);
    auto colAt = [&](float u, float& a) {
        const float k = std::exp(-std::pow((u - scan) / 0.06f, 2.f));
        a *= 1.f + 1.2f * k;
        return lerp(ink, ec, k);
    };
    auto X = [&](float u) { return left + u * L; };
    auto Y = [&](float v) { return cy - v * L; };
    const int N = 40;
    // the hull's lines: the outline, then the latitudes (an ellipse's sections seen from the side)
    const float lat[] = {1.f, 0.866f, 0.5f, 0.f, -0.5f, -0.866f, -1.f};
    for (float k : lat) {
        const bool out = std::fabs(k) > 0.99f;
        const float w = (out ? 1.25f : 0.8f) * s, a0 = out ? 0.62f : 0.26f;
        for (int i = 0; i < N; i++) {
            const float u0 = 0.04f + 0.96f * i / N, u1 = 0.04f + 0.96f * (i + 1) / N;
            float a = a0 * A;
            const vec3 c = colAt(0.5f * (u0 + u1), a);
            line(X(u0), Y(k * rad(u0)), X(u1), Y(k * rad(u1)), w, c, a);
        }
    }
    // the ribs
    for (int i = 0; i <= 22; i++) {
        const float u = 0.07f + 0.86f * i / 22.f;
        float a = 0.22f * A;
        const vec3 c = colAt(u, a);
        const float r = rad(u);
        line(X(u), Y(r), X(u), Y(-r), 0.8f * s, c, a);
    }
    // the sail, its masts and the planes on it
    {
        const float u0 = 0.53f, u1 = 0.66f, top = R + 0.062f, base = R * 0.95f;
        float a = 0.6f * A;
        const vec3 c = colAt(0.6f, a);
        line(X(u0), Y(base), X(u0 + 0.018f), Y(top), 1.2f * s, c, a);
        line(X(u0 + 0.018f), Y(top), X(u1 - 0.008f), Y(top), 1.2f * s, c, a);
        line(X(u1 - 0.008f), Y(top), X(u1), Y(base), 1.2f * s, c, a);
        for (int j = 1; j < 4; j++) {
            const float v = base + (top - base) * j / 4.f;
            line(X(u0 + 0.018f * j / 4.f), Y(v), X(u1 - 0.008f * j / 4.f), Y(v), 0.8f * s, c, a * 0.45f);
        }
        line(X(0.57f), Y(top), X(0.57f), Y(top + 0.035f), 1.f * s, c, a);
        line(X(0.6f), Y(top), X(0.6f), Y(top + 0.05f), 1.f * s, c, a);
        line(X(0.595f), Y(top + 0.05f), X(0.607f), Y(top + 0.05f), 1.f * s, c, a);
        line(X(0.555f), Y(top - 0.018f), X(0.535f), Y(top - 0.018f), 1.f * s, c, a * 0.8f);
        line(X(0.655f), Y(top - 0.018f), X(0.675f), Y(top - 0.018f), 1.f * s, c, a * 0.8f);
    }
    // the stern: cruciform fins, the screw
    {
        float a = 0.6f * A;
        const vec3 c = colAt(0.08f, a);
        const float r0 = rad(0.08f);
        line(X(0.13f), Y(r0 * 0.8f), X(0.07f), Y(R * 1.25f), 1.1f * s, c, a);
        line(X(0.07f), Y(R * 1.25f), X(0.045f), Y(R * 1.25f), 1.1f * s, c, a);
        line(X(0.045f), Y(R * 1.25f), X(0.05f), Y(rad(0.05f)), 1.1f * s, c, a);
        line(X(0.13f), Y(-r0 * 0.8f), X(0.07f), Y(-R * 1.25f), 1.1f * s, c, a);
        line(X(0.07f), Y(-R * 1.25f), X(0.045f), Y(-R * 1.25f), 1.1f * s, c, a);
        line(X(0.045f), Y(-R * 1.25f), X(0.05f), Y(-rad(0.05f)), 1.1f * s, c, a);
        line(X(0.12f), Y(0.f), X(0.045f), Y(0.f), 1.f * s, c, a * 0.7f);
        line(X(0.04f), Y(rad(0.04f)), X(0.022f), Y(0.f), 1.f * s, c, a);
        line(X(0.04f), Y(-rad(0.04f)), X(0.022f), Y(0.f), 1.f * s, c, a);
        line(X(0.03f), Y(R * 0.55f), X(0.03f), Y(-R * 0.55f), 1.1f * s, c, a);
        line(X(0.022f), Y(0.f), X(0.0f), Y(0.f), 1.f * s, c, a * 0.7f);
    }
    // the bow's planes, the hull's hatches
    {
        float a = 0.45f * A;
        const vec3 c = colAt(0.85f, a);
        line(X(0.82f), Y(R * 0.35f), X(0.86f), Y(R * 0.35f), 1.f * s, c, a);
        line(X(0.36f), Y(R * 0.2f), X(0.4f), Y(R * 0.2f), 0.9f * s, c, a * 0.7f);
        line(X(0.36f), Y(-R * 0.2f), X(0.4f), Y(-R * 0.2f), 0.9f * s, c, a * 0.7f);
        line(X(0.36f), Y(R * 0.2f), X(0.36f), Y(-R * 0.2f), 0.9f * s, c, a * 0.7f);
        line(X(0.4f), Y(R * 0.2f), X(0.4f), Y(-R * 0.2f), 0.9f * s, c, a * 0.7f);
    }
    flushShapes();
}


// The lettering, by the host (text.hpp): Tenor Sans in widely spaced capitals for the names, the numbers and the
// celebrations, Cormorant Garamond Italic for the song and the notices; pale ink on a soft halo of the dark water,
// the accent the echo (warming to the drop's echo).
void SonarUniverse::hudLine(const HudText& h, text::Line& out) {
    const float s = height_ / 1080.f;
    const vec3 ink = vec3(0.78f, 0.81f, 0.84f), halo = ugl::toSrgb(waterLow_) * 0.6f;
    const vec3 accent = lerp(ugl::toSrgb(lerp(echo_, dropEcho_, fest_ * 0.8f)), vec3(1.f), 0.2f);
    text::Font* caps = text::font(FONT_CAPS);
    text::Font* it = text::font(FONT_IT);
    const float base = h.scale * 12.f, alpha = saturate(h.alpha);
    float haloA = 0.45f;
    // a run: its size and letter spacing in pixels, moved down by dy
    auto put = [&](text::Font* f, const std::string& t, float size, float track, const vec3& c, float a, float dy = 0.f) {
        out.add(t, f, size, c, track / size, saturate(a));
        out.dy = dy;
    };
    switch (h.role) {
    case HUD_UNIVERSE: put(caps, text::upper(h.text), base * 0.74f, base * 0.36f, ink, saturate(0.3f + h.alpha * 1.5f) * 0.75f, base * 0.06f); break;
    case HUD_SECTION: put(caps, text::upper(h.text), base * 0.82f, base * 0.4f, accent, 0.3f + h.alpha * 1.7f, -base * 0.04f); break;
    case HUD_TITLE: put(it, h.text, base * 1.16f, base * 0.02f, ink, h.alpha * 1.3f, -base * 0.12f); break;
    case HUD_INFO: put(caps, text::upper(h.text), base * 0.66f, base * 0.28f, ink, saturate(alpha * 1.2f) * 0.72f); break;
    case HUD_LABEL: put(caps, text::upper(h.text), base * 0.7f, base * 0.44f, ink, saturate(alpha * 1.2f) * 0.55f); break;
    case HUD_VALUE: put(caps, h.text, base * 1.05f, base * 0.14f, ink, alpha * 0.95f, -base * 0.06f); break;
    case HUD_NOTICE: put(it, h.text, base * 1.1f, base * 0.04f, accent, alpha); break;
    case HUD_KEY: put(caps, h.text, base * 0.86f, base * 0.1f, ink, alpha); break;
    case HUD_TOAST: put(caps, text::upper(h.text), base * 1.05f, base * 0.45f, ink, alpha, -base * 0.12f); haloA = 0.6f; break;
    case HUD_TOAST_SUB: put(it, h.text, base * 1.0f, base * 0.04f, accent, alpha * 0.9f); haloA = 0.6f; break;
    case HUD_DIALOG:
        if (h.scale >= 3.5f * s) put(caps, text::upper(h.text), base * 0.92f, base * 0.32f, ink, alpha);
        else put(it, h.text, base * 0.95f, base * 0.03f, h.accent ? accent : ink, alpha);
        haloA = 0.7f;
        break;
    default: put(caps, h.text, base * 0.78f, base * 0.1f, h.accent ? accent : ink, alpha); break;
    }
    // the letters a touch full (their edge a little out), the halo a smooth falloff just around them
    out.look.weight = 0.0125f;
    out.look.halo = halo;
    out.look.haloA = haloA;
    out.look.haloW = 0.075f;
    out.look.haloPow = 1.5f;
    out.alpha = 1.f - smoothstepf(0.3f, 0.8f, bridge_.amount);
}

// =====================================================================================================
// Frame
// =====================================================================================================

void SonarUniverse::setWorldUniforms(GLuint prog) {
    float pv[32], ps[16];
    for (int i = 0; i < 8; i++) {
        const Ping& p = pings_[i];
        pv[i * 4] = p.p.x; pv[i * 4 + 1] = p.p.y; pv[i * 4 + 2] = p.p.z; pv[i * 4 + 3] = p.age;
        ps[i * 2] = p.strength; ps[i * 2 + 1] = p.reach;
    }
    glUniform4fv(ugl::U(prog, "uPing"), 8, pv);
    glUniform2fv(ugl::U(prog, "uPingS"), 8, ps);
    set1f(prog, "uDecay", decay_);
    set4f(prog, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set2f(prog, "uRes", (float)width_, (float)height_);
    set3f(prog, "uWaterTop", waterTop_);
    set3f(prog, "uWaterLow", waterLow_);
    const float aspect = (float)width_ / height_;
    set3f(prog, "uCamF", camF_);
    set3f(prog, "uCamR", camR_ * (wTan_ * aspect));
    set3f(prog, "uCamU", camU_ * wTan_);
    set1f(prog, "uTime", worldTime_);
    set1f(prog, "uSeed", seedF_);
}

void SonarUniverse::render(const Frame& f, GLuint fbo) {
    const Game& game = f.game;
    const MusicState& music = f.music;
    const float dt = std::min(f.dt, 0.1f);
    bare_ = f.bare;
    // a new variant eases in
    {
        const float k = 1.f - std::exp(-dt * 1.2f);
        waterTop_ = lerp(waterTop_, waterTopT_, k);
        waterLow_ = lerp(waterLow_, waterLowT_, k);
        stone_ = lerp(stone_, stoneT_, k);
        echo_ = lerp(echo_, echoT_, k);
        echoCity_ = lerp(echoCity_, echoCityT_, k);
        dropEcho_ = lerp(dropEcho_, dropEchoT_, k);
        for (int i = 0; i < 7; i++) pieceCol_[i] = lerp(pieceCol_[i], pieceColT_[i], k);
    }
    slow_ = approach(slow_, slowOn_ ? 1.f : 0.f, 1.0f, dt);
    dimT_ = approach(dimT_, f.paused ? 1.f : (f.dim ? 0.5f : 0.f), 3.f, dt);
    if (overT_ >= 0.f && !bare_) {
        if (!game.over()) overT_ = -1.f;
        else overT_ += dt;
    }
    const float wdt = f.paused ? 0.f : dt * lerpf(1.f, 0.4f, slow_);
    time_ += dt;
    worldTime_ += wdt;
    // ---- Staging.
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        lv_ = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wm = smoothstepf(0.2f, 1.f, lv_), wp = smoothstepf(1.1f, 2.f, lv_);
    if (music.hasSong) beat_ = music.beat;
    else if (!f.paused) beat_ += wdt * 2.0;
    const float spb = 60.f / std::max(60.f, music.bpm);
    // the drop: eased in over two beats after its downbeat, held through the first bars, then settling
    dropT_ += dt;
    float festT = 0.f;
    if (dropMode_) festT = smoothstepf(0.f, 2.f * spb, dropT_) * (1.f - 0.8f * smoothstepf(16.f * spb, 40.f * spb, dropT_));
    fest_ = approach(fest_, festT, festT > fest_ ? 1.6f : 0.3f, dt);
    swell_ = approach(swell_, 0.f, 0.22f, dt);
    quadGlow_ = approach(quadGlow_, 0.f, 0.45f, dt);
    kickEnv_ = std::max(kickEnv_ * std::exp(-dt * 4.f), kickIn_ * wp);
    kickIn_ = 0.f;

    // ---- The glide: calm and slow, a little livelier from a chorus on, a clear surge of travel on the drop that
    // eases in over a couple of seconds and settles over the following bars; game over drifts to a halt.
    {
        float target = lerpf(0.8f, 1.1f, wm) + 0.4f * wp + 1.4f * fest_;
        if (overT_ >= 0.f) target = 0.25f;
        // the setup sinks the camera down into its glide, the teardown lets it rise away into the dark
        rise_ = 16.f * (1.f - smoothstepf(0.f, 0.9f, f.setup)) + 16.f * smoothstepf(0.1f, 1.f, f.teardown);
        updateCamera(wdt, target, f.paused ? 0.f : dt);
        updateFish(wdt);
    }

    // ---- The pings, on the beat grid: every four beats in calm parts, every two from mid on, both sides at a peak.
    if (!f.paused && overT_ < 0.f && bridge_.amount < 0.5f) {
        const long long slot = (long long)std::floor(beat_ / 2.0);
        if (slot != pingSlot_) {
            const bool first = pingSlot_ < 0 || slot < pingSlot_ || slot > pingSlot_ + 8;
            pingSlot_ = slot;
            const bool calm = lv_ < 0.6f || slowOn_;
            if (!first && (!calm || slot % 2 == 0)) {
                pingSide_ = -pingSide_;
                const float str = lerpf(0.6f, 0.85f, wm) + 0.15f * wp;
                const float reach = (lerpf(2.8f, 3.4f, wm) + 1.f * wp + 1.f * fest_) * 1.4f;
                const int maxLayer = wm > 0.5f ? 2 : 0;
                const float cityChance = 0.15f * wm + 0.6f * wp;
                pingSomewhere(pingSide_, str, reach, maxLayer, cityChance);
                if (wp > 0.5f || fest_ > 0.3f) pingSomewhere(-pingSide_, str * 0.85f, reach * 0.9f, 2, cityChance);
            }
        }
    }
    decay_ = lerpf(1.5f, 4.f, slow_);
    for (Ping& p : pings_)
        if (p.age >= 0.f) {
            p.age += wdt;
            if (p.age > decay_ * 6.f + 4.f) p.age = -1.f;
        }
    // ---- The chart.
    for (Chart& c : charts_) {
        if (c.fading) { c.amount = approach(c.amount, 0.f, 0.8f, dt); continue; }
        if (c.delay > 0.f) { c.delay -= dt; continue; }
        c.amount = std::min(1.f, c.amount + dt / 1.2f);
    }
    charts_.erase(std::remove_if(charts_.begin(), charts_.end(), [](const Chart& c) { return c.fading && c.amount < 0.01f; }),
                  charts_.end());
    // ---- Light sinking (real time: what the play drives), toward its spot as the camera glides on.
    for (Mote& m : motes_) {
        m.t += dt;
        if (m.world) toBoard(m.target, m.p1);
        const float s = saturate((m.t - m.delay) / m.life);
        const float e = s * s * (3.f - 2.f * s);
        // out from the well's side, then down onto the seabed
        const vec2 c1{m.p0.x + (m.p1.x - m.p0.x) * 0.55f, m.p0.y - 0.6f};
        const float a = 1.f - e;
        m.p = vec2{a * a * m.p0.x + 2.f * a * e * c1.x + e * e * m.p1.x, a * a * m.p0.y + 2.f * a * e * c1.y + e * e * m.p1.y};
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(), [](const Mote& m) { return m.t > m.delay + m.life; }), motes_.end());
    for (Sink& s : sinks_) s.t += dt;
    sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(), [](const Sink& s) { return s.t > s.delay + s.dur; }), sinks_.end());

    // ---- Instances.
    cells_.clear();
    moteInst_.clear();
    for (const Sink& s : sinks_) {
        if (bare_) break; // the world alone: no blocks, no light sinking out of them
        const float t = saturate((s.t - s.delay) / s.dur);
        addCell(s.x, s.y, 0.5f, s.kind, s.col, 1.f, 0.f, t, s.x * 3.f + s.y);
    }
    if (!bare_) buildBoard(game);
    for (const Mote& m : motes_) {
        if (bare_) break;
        const float tt = m.t - m.delay;
        if (tt <= 0.f) continue;
        const float s = saturate(tt / m.life);
        float a;
        if (m.glint) a = (1.f - std::exp(-tt / 0.03f)) * std::exp(-tt / 0.16f) * m.peak;
        else a = smoothstepf(0.f, 0.15f, s) * (1.f - smoothstepf(0.7f, 1.f, s)) * m.peak;
        moteInst_.push_back({{m.p.x, m.p.y, m.size, a}, {m.col.x, m.col.y, m.col.z, m.stretch}});
    }

    // ---- The world.
    const float aaW = 2.f * viewH_.y / (float)height_;
    const float bridgeK = smoothstepf(0.f, 1.f, saturate(bridge_.amount));
    const float quietSlow = 1.f - 0.5f * slow_;
    const vec3 ech = lerp(echo_, dropEcho_, fest_ * 0.85f), echC = lerp(echoCity_, dropEcho_, fest_ * 0.9f);
    sceneT_.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(progWater_);
    setWorldUniforms(progWater_);
    set1f(progWater_, "uSlow", slow_);
    set1f(progWater_, "uFade", 1.f - bridgeK);
    ugl::drawFullscreen();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glUseProgram(progMesh_);
    setWorldUniforms(progMesh_);
    setMat(progMesh_, "uWVP", wvp_);
    set3f(progMesh_, "uCam", camPos_);
    {
        float cv[64 * 4], ca[64];
        const int n = (int)std::min<size_t>(64, charts_.size());
        for (int i = 0; i < n; i++) {
            const Chart& c = charts_[i];
            cv[i * 4] = c.p.x; cv[i * 4 + 1] = c.p.y; cv[i * 4 + 2] = c.p.z; cv[i * 4 + 3] = c.r;
            ca[i] = c.amount;
        }
        if (n > 0) {
            glUniform4fv(ugl::U(progMesh_, "uChart"), n, cv);
            glUniform1fv(ugl::U(progMesh_, "uChartA"), n, ca);
        }
        set1i(progMesh_, "uCharts", n);
    }
    set3f(progMesh_, "uEcho", ech);
    set3f(progMesh_, "uEchoCity", echC);
    set3f(progMesh_, "uStone", stone_);
    set1f(progMesh_, "uAmb", (1.f - 0.25f * fest_) * (1.f - 0.35f * slow_));
    set1f(progMesh_, "uPaths", fest_);
    // (the setup: the seabed's contours glowing up softly as the camera sinks to them, then settling; the teardown:
    // the chart fading into the dark)
    const float sink = smoothstepf(0.15f, 0.55f, f.setup) * (1.f - smoothstepf(0.6f, 1.f, f.setup));
    const float away = 1.f - smoothstepf(0.25f, 0.95f, f.teardown);
    set1f(progMesh_, "uBed", (0.004f + 0.03f * wp + 0.3f * fest_ + 0.15f * swell_ + 0.2f * quadGlow_ + 0.12f * sink) * quietSlow);
    set1f(progMesh_, "uCity", (0.2f * wp + 0.7f * fest_ + 0.25f * swell_ + 0.55f * quadGlow_) * quietSlow);
    set1f(progMesh_, "uWin", (0.6f * kickEnv_ + 0.3f * fest_) * quietSlow);
    set1f(progMesh_, "uSlow", slow_);
    set1f(progMesh_, "uFade", (1.f - bridgeK) * away);
    glBindVertexArray(meshVao_);
    glDrawElements(GL_TRIANGLES, meshCount_, GL_UNSIGNED_INT, nullptr);
    // the particles in the water
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(progSnow_);
    setWorldUniforms(progSnow_);
    setMat(progSnow_, "uWVP", wvp_);
    set3f(progSnow_, "uCam", camPos_);
    set1f(progSnow_, "uBase", (0.05f + 0.03f * wm + 0.05f * fest_) * quietSlow);
    set1f(progSnow_, "uFocal", height_ * 0.5f / wTan_);
    set3f(progSnow_, "uEcho", lerp(ech, vec3(1.f), 0.3f));
    set3f(progSnow_, "uMote", lerp(waterTop_, vec3(1.f), 0.35f) * 4.f);
    set1f(progSnow_, "uFade", 1.f - bridgeK);
    set1f(progSnow_, "uBubbles", 0.f);
    glBindVertexArray(snowVao_);
    glDrawArrays(GL_POINTS, 0, snowCount_);
    // bubbles rising in thin streams
    set1f(progSnow_, "uBubbles", 1.f);
    set1f(progSnow_, "uBase", 0.32f * quietSlow);
    glDrawArrays(GL_POINTS, 0, BUBBLE_N);
    glDisable(GL_PROGRAM_POINT_SIZE);
    // the fish
    if (!fishInst_.empty()) {
        glUseProgram(progFish_);
        setWorldUniforms(progFish_);
        setMat(progFish_, "uWVP", wvp_);
        set3f(progFish_, "uFR", camR_);
        set3f(progFish_, "uFU", camU_);
        set3f(progFish_, "uCam", camPos_);
        set3f(progFish_, "uFishCol", lerp(waterTop_, vec3(0.8f, 0.9f, 0.95f), 0.18f) * 0.8f * quietSlow);
        set3f(progFish_, "uEcho", ech);
        set1f(progFish_, "uFade", 1.f - bridgeK);
        glBindVertexArray(fishVao_);
        glBindBuffer(GL_ARRAY_BUFFER, fishVbo_);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(fishInst_.size() * sizeof(Inst)), fishInst_.data(), GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (GLsizei)fishInst_.size());
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    // ---- The glow: of the world only (the blocks stay crisp).
    glDisable(GL_BLEND);
    bloomA_.bind();
    glUseProgram(progDown_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneT_.tex);
    set1i(progDown_, "uSrc", 0);
    set2f(progDown_, "uTexel", 1.5f / sceneT_.w, 1.5f / sceneT_.h);
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

    // ---- The well and the blocks, through the layout's camera.
    sceneT_.bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); // the scene's alpha is the stack's (for the bridge)
    if (!bare_) {
        glUseProgram(progWell_);
        set4f(progWell_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
        set3f(progWell_, "uLine", lerp(echo_, vec3(0.6f), 0.5f));
        set3f(progWell_, "uPane", waterLow_ * 0.35f);
        set1f(progWell_, "uAA", aaW);
        set1f(progWell_, "uLineA", 0.2f);
        set1f(progWell_, "uPaneA", 0.7f);
        ugl::drawFullscreen();
    }
    // the light sinking, behind the stack
    if (!moteInst_.empty()) drawInst(progMote_, moteVao_, moteVbo_, moteInst_.data(), moteInst_.size() * sizeof(MoteInst), moteInst_.size());
    // the blocks: the stack writes the scene's alpha
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(progPiece_);
    set1f(progPiece_, "uTime", (float)time_);
    set1f(progPiece_, "uGlow", (1.f + 0.04f * wp + 0.04f * fest_) * (1.f - 0.2f * slow_));
    drawInst(progPiece_, pieceVao_, pieceVbo_, cells_.data(), cells_.size() * sizeof(Inst), cells_.size());
    glDisable(GL_BLEND);

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
    set1f(progFinal_, "uBloomK", (0.5f + 0.1f * wp + 0.15f * fest_) * (1.f - 0.3f * slow_));
    set1f(progFinal_, "uSlow", slow_ * 0.85f);
    set3f(progFinal_, "uSlowTint", lin(0x9ab8ff));
    set1f(progFinal_, "uDim", dimT_);
    set1f(progFinal_, "uFade", saturate(f.fade));
    set1f(progFinal_, "uBridge", bridgeK);
    set4f(progFinal_, "uView", viewC_.x, viewC_.y, viewH_.x, viewH_.y);
    set1f(progFinal_, "uPaneA", bare_ ? 0.f : 0.7f);
    set3f(progFinal_, "uBridgeCol", bridge_.color);
    set3f(progFinal_, "uBridgeInk", bridge_.ink);
    ugl::drawFullscreen();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawHud(f.panels);
    drawSub(f);
    glDisable(GL_BLEND);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_sonar() {
    static const UniverseEntry e{
        "SONAR", "a dark sea where shapes glow up from the black as echoes return; each cleared row charts the seabed",
        [] { return std::unique_ptr<Universe>(new SonarUniverse()); },
        // A dark, calm world: fits most songs, a little more the darker, bass-heavy ones.
        [](const SongLook& l) { return 0.8f + 0.2f * (1.f - l.brightness) + 0.1f * l.bassWeight; },
        [] {
            std::vector<std::string> v;
            for (const VariantDef& x : VARIANTS) v.push_back(x.name);
            return v;
        }()};
    return e;
}

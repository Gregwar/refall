// refall-voyage-viewer's overlay (see voyageviewer.hpp): the strip of the campaign's songs, the current song's timeline,
// the tour, and their keys and mouse.
#include "voyageviewer.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "library.hpp"
#include "mathutil.hpp"
#include "menu.hpp"
#include "menuworlds.hpp"
#include "platform.hpp"
#include "youtube.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO_WARNINGS
#include "stb_image.h"

namespace fs = std::filesystem;

namespace {
using F = Painter::Font;
constexpr float TOUR_DWELL = 6.f; // seconds of each section the tour plays
constexpr float TOUR_LEAD = 1.5f; // from this far into it (the scene has turned)
constexpr float SEEK_STEP = 10.f;
float g_wheel = 0; // the mouse wheel, since the last frame

std::string clock(double t) {
    const int s = std::max(0, (int)t);
    char b[16];
    std::snprintf(b, sizeof(b), "%d:%02d", s / 60, s % 60);
    return b;
}
// a section's colour: calm parts cool and grey, the lifts warm
Col kindCol(int k) {
    static const uint32_t C[SEG_COUNT] = {0x6c7a96, 0x3f8f8a, 0xc79a3c, 0xc0567e, 0xd8613c, 0x6a5fb0, 0x5c6474};
    return hex(C[std::clamp(k, 0, SEG_COUNT - 1)]);
}
std::string specOf(const CampaignSong* s) { return s && !s->universe.empty() ? s->universe : std::string("VECTOR"); }
} // namespace

VoyageViewer::VoyageViewer(const Campaign* c, int difficulty) : camp_(c), diff_(difficulty) {
    std::vector<std::string> specs;
    for (int st = 0; st < c->size(); st++)
        for (int k = 0; k < (int)c->stages()[st].songs.size(); k++) {
            songs_.push_back({st, k, &c->stages()[st].songs[k]});
            specs.push_back(specOf(songs_.back().s));
        }
    // the universes in miniature: cached snapshots, the missing ones made on the worker thread
    worlds_ = std::make_unique<WorldPool>();
    worlds_->setSnapshotsOnly(true);
    worlds_->setGenerate(true, 0.4f);
    worlds_->setIdle(true);
    worlds_->prefetch(specs);
    cached_.assign(songs_.size(), false);
    thumbThread_ = std::thread([this] { thumbLoop(); });
}

VoyageViewer::~VoyageViewer() {
    { std::lock_guard<std::mutex> lk(tm_); thumbStop_ = true; }
    tcv_.notify_all();
    if (thumbThread_.joinable()) thumbThread_.join();
}

int VoyageViewer::find(int stage, int song) const {
    for (int i = 0; i < count(); i++)
        if (songs_[i].stage == stage && songs_[i].song == song) return i;
    return -1;
}

int VoyageViewer::indexOf(const std::string& path) const {
    const std::string name = fs::path(path).stem().string();
    for (int i = 0; i < count(); i++)
        if (name == songs_[i].s->id) return i;
    return -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// thumbnails: https://i.ytimg.com/vi/<id>/mqdefault.jpg (320 x 180), kept in <cache>/refall/thumbs/

void VoyageViewer::thumbLoop() {
    const fs::path dir = platform::appCacheDir() / "thumbs";
    const std::string curl = platform::findExecutable("curl");
    for (;;) {
        std::string id;
        {
            std::unique_lock<std::mutex> lk(tm_);
            tcv_.wait(lk, [&] { return thumbStop_ || !thumbJobs_.empty(); });
            if (thumbStop_) return;
            id = thumbJobs_.front();
            thumbJobs_.pop_front();
        }
        const fs::path file = dir / (id + ".jpg");
        std::error_code ec;
        if (!fs::exists(file, ec) && !curl.empty()) {
            fs::create_directories(dir, ec);
            const fs::path part = dir / (id + ".part");
            const std::string cmd = platform::quoteArg(curl) + " -fsL --max-time 20 -o " + platform::quoteArg(part.string()) + " " +
                                    platform::quoteArg("https://i.ytimg.com/vi/" + id + "/mqdefault.jpg");
            if (platform::run(cmd) == 0) fs::rename(part, file, ec);
            else fs::remove(part, ec);
        }
        Thumb t;
        int n = 0;
        if (unsigned char* px = stbi_load(file.string().c_str(), &t.w, &t.h, &n, 4)) {
            t.px.assign(px, px + (size_t)t.w * t.h * 4);
            stbi_image_free(px);
        } else {
            std::fprintf(stderr, "[viewer] no thumbnail for %s\n", id.c_str());
        }
        std::lock_guard<std::mutex> lk(tm_);
        thumbDone_.push_back({id, std::move(t)});
    }
}

GLuint VoyageViewer::thumb(const std::string& id) {
    auto it = thumbTex_.find(id);
    if (it != thumbTex_.end()) return it->second;
    if (thumbAsked_.insert(id).second) {
        { std::lock_guard<std::mutex> lk(tm_); thumbJobs_.push_back(id); }
        tcv_.notify_one();
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// input and the tour

bool VoyageViewer::key(GLFWwindow* win, int k, bool repeat, float dt) {
    const bool down = win && glfwGetKey(win, k) == GLFW_PRESS;
    const bool was = keyWas_[k];
    keyWas_[k] = down;
    float& held = keyHeld_[k];
    if (!down) { held = 0; return false; }
    if (!was) return true;
    // held: repeats after 0.4 s, every 0.15 s
    const float before = held;
    held += dt;
    return repeat && held > 0.4f && std::floor((held - 0.4f) / 0.15f) != std::floor((before - 0.4f) / 0.15f);
}

std::vector<VoyageViewer::Action> VoyageViewer::update(GLFWwindow* win, const Input& in, const Now& now, float dt) {
    std::vector<Action> out;
    time_ += dt;
    alpha_ = approach(alpha_, shown_ ? 1.f : 0.f, 8.f, dt);
    cachedAge_ += dt;
    if (cachedAge_ > 3.f) {
        cachedAge_ = 0;
        for (int i = 0; i < count(); i++) cached_[i] = isYoutubeCached(entry(i));
    }
    if (count() == 0) return out;

    // keys, as commands
    if (win) {
        static bool wheelSet = false;
        if (!wheelSet) {
            wheelSet = true;
            glfwSetScrollCallback(win, [](GLFWwindow*, double x, double y) { g_wheel += (float)(y - x); });
        }
        if (key(win, GLFW_KEY_TAB) || in.pressed(A_HUD)) command("hide");
        if (key(win, GLFW_KEY_SPACE)) command("tour");
        if (key(win, GLFW_KEY_UP, true, dt)) command("prev");
        if (key(win, GLFW_KEY_DOWN, true, dt)) command("next");
        if (key(win, GLFW_KEY_PAGE_UP)) command("stage-");
        if (key(win, GLFW_KEY_PAGE_DOWN)) command("stage+");
        if (key(win, GLFW_KEY_LEFT, true, dt)) command("back");
        if (key(win, GLFW_KEY_RIGHT, true, dt)) command("fwd");
        for (char c : in.typed()) { // (the brackets wherever the keyboard layout puts them)
            if (c == '[') command("sec-");
            if (c == ']') command("sec+");
        }
    }
    const int n = count(), f = focus(now);
    auto jump = [&](int i) { out.push_back({Action::JUMP, ((i % n) + n) % n, 0.0}); };
    auto seek = [&](double t) { out.push_back({Action::SEEK, 0, std::max(0.0, t)}); };
    const std::vector<Segment>* segs = now.an && !now.an->segments.empty() ? &now.an->segments : nullptr;
    for (const std::string& c : pending_) {
        const bool nav = c != "hide" && c != "tour";
        if (nav) tour_ = false; // the owner takes over: the tour waits for Space
        if (c == "hide") shown_ = !shown_;
        else if (c == "tour") { tour_ = !tour_; tourSeg_ = -1; }
        else if (c == "next") jump(f + 1);
        else if (c == "prev") jump(f - 1);
        else if (c == "stage+" || c == "stage-") {
            const int st = songs_[f].stage + (c == "stage+" ? 1 : songs_[f].song > 0 ? 0 : -1);
            const int to = find(((st % camp_->size()) + camp_->size()) % camp_->size(), 0);
            if (to >= 0) jump(to);
        } else if (now.current >= 0 && (c == "fwd" || c == "back")) {
            seek(now.time + (c == "fwd" ? SEEK_STEP : -SEEK_STEP));
        } else if (now.current >= 0 && segs && (c == "sec+" || c == "sec-")) {
            const int k = now.an->segmentAt(now.time);
            if (c == "sec+" && k + 1 < (int)segs->size()) seek((*segs)[k + 1].start);
            else if (c == "sec-") seek(now.time - (*segs)[k].start > 2.0 || k == 0 ? (*segs)[k].start : (*segs)[k - 1].start);
        }
    }
    pending_.clear();

    // the mouse: a card plays its song, the timeline seeks (on release, the time under the pointer while dragging)
    if (win) {
        double cx, cy;
        int ww, wh;
        glfwGetCursorPos(win, &cx, &cy);
        glfwGetWindowSize(win, &ww, &wh);
        mx_ = (float)(cx * fbw_ / std::max(1, ww));
        my_ = (float)(cy * fbh_ / std::max(1, wh));
        const bool down = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool live = shown_ && alpha_ > 0.5f;
        auto timeAt = [&](float x) { return std::clamp((double)(x - bar_.x) / std::max(1.f, bar_.w), 0.0, 1.0) * now.duration; };
        dragTime_ = live && now.current >= 0 && now.duration > 0 && (dragging_ || bar_.in(mx_, my_)) ? timeAt(mx_) : -1.0;
        if (live && down && !mouseWas_) {
            if (dragTime_ >= 0) dragging_ = true;
            else
                for (int i = 0; i < (int)cards_.size(); i++)
                    if (cards_[i].in(mx_, my_)) { tour_ = false; jump(i); break; }
        }
        if (dragging_ && !down) {
            dragging_ = false;
            tour_ = false;
            if (dragTime_ >= 0) seek(dragTime_);
        }
        mouseWas_ = down;
        if (g_wheel != 0.f && live) {
            scrollManual_ -= g_wheel * 120.f * fbh_ / 900.f;
            scrollIdle_ = 0;
        }
        g_wheel = 0;
    }
    scrollIdle_ += dt;
    if (scrollIdle_ > 5.f) scrollManual_ = approach(scrollManual_, 0.f, 3.f, dt); // back to the song playing
    tour(now, out);
    return out;
}

void VoyageViewer::tour(const Now& now, std::vector<Action>& out) {
    if (!tour_ || now.current < 0 || now.wanted >= 0 || now.paused || !now.an || now.an->segments.empty()) return;
    const std::vector<Segment>& sg = now.an->segments;
    auto window = [&](int k, double from) {
        tourFrom_ = from;
        tourUntil_ = std::max(from + 2.5, std::min(from + TOUR_DWELL, sg[k].end - 0.3));
    };
    if (tourSeg_ < 0 || now.time < tourFrom_ - 1.0) { // starting, or seeked behind: from the playhead's section
        tourSeg_ = now.an->segmentAt(now.time);
        window(tourSeg_, now.time);
        return;
    }
    if (now.time < tourUntil_) return;
    if (tourSeg_ + 1 < (int)sg.size()) {
        tourSeg_++;
        const Segment& s = sg[tourSeg_];
        const double from = s.start + std::min((double)TOUR_LEAD, (s.end - s.start) * 0.2);
        window(tourSeg_, from);
        out.push_back({Action::SEEK, 0, from});
    } else {
        tourSeg_ = -1;
        out.push_back({Action::JUMP, (now.current + 1) % count(), 0.0});
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// drawing

void VoyageViewer::draw(int w, int h, const Now& now) {
    fbw_ = w;
    fbh_ = h;
    {
        std::vector<std::pair<std::string, Thumb>> done;
        { std::lock_guard<std::mutex> lk(tm_); done.swap(thumbDone_); }
        for (auto& [id, t] : done) {
            GLuint tex = 0;
            if (!t.px.empty()) {
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.px.data());
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            thumbTex_[id] = tex;
        }
    }
    static const MusicState mu; // (the snapshots-only pool renders nothing)
    dt_ = (float)std::clamp(time_ - lastDraw_, 0.0, 0.1);
    lastDraw_ = time_;
    if (count() > 0) {
        const int f = focus(now);
        for (int i = std::max(0, f - 10); i < std::min(count(), f + 11); i++) worlds_->want(specOf(songs_[i].s), 0);
    }
    worlds_->update(time_, dt_, mu);
    if (alpha_ < 0.01f || count() == 0) { cards_.clear(); bar_ = Rect{}; return; }
    p_.begin(w, h);
    p_.setFade(alpha_);
    drawStrip(w, h, now);
    drawTimeline(w, h, now);
    p_.setFade(1.f);
    p_.end();
}

// a line of text cut to a width, with an ellipsis
static std::string fit(Painter& p, F f, std::string s, float size, float maxW, float track = 0.f) {
    if (p.width(f, s, size, track) <= maxW) return s;
    while (s.size() > 1 && p.width(f, s + "...", size, track) > maxW) s.pop_back();
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s + "...";
}

void VoyageViewer::drawStrip(int w, int h, const Now& now) {
    Painter& p = p_;
    const float s = h / 900.f;
    const float CW = 176 * s, pad = 4 * s, IW = CW - 2 * pad, IH = IW * 9.f / 16.f, CH = pad + IH + 44 * s;
    const float GAP = 8 * s, STAGE_GAP = 24 * s, EDGE = 24 * s, TOP = 36 * s;
    const int n = count(), f = focus(now);
    auto xOf = [&](int i) { return EDGE + i * (CW + GAP) + songs_[i].stage * STAGE_GAP; };
    const float total = xOf(n - 1) + CW + EDGE;
    // the strip follows the song playing (or the one jumped to), centred; the wheel moves it a while
    const float goal = std::clamp(xOf(f) + CW * 0.5f - w * 0.5f + scrollManual_, 0.f, std::max(0.f, total - w));
    scroll_ = scrollSnap_ ? goal : approach(scroll_, goal, 9.f, dt_);
    scrollSnap_ = false;
    // the band
    const float bandH = TOP + CH + 18 * s;
    p.rrect(-80 * s, -80 * s, w + 160 * s, bandH + 80 * s, 0, hex(0x05050a, 0.76f), 36 * s);
    cards_.assign(n, Rect{});
    const Col accent = hex(0xffd27a);
    int lastStage = -1;
    for (int i = 0; i < n; i++) {
        const Song& sg = songs_[i];
        const float x = xOf(i) - scroll_, y = TOP;
        if (sg.stage != lastStage) { // the stage's label over its songs
            lastStage = sg.stage;
            const bool here = songs_[f].stage == sg.stage;
            p.text(F::JOSEFIN, "STAGE " + std::to_string(sg.stage + 1), std::max(x, EDGE * 0.5f) + 2 * s, y - 12 * s, 11.5f * s,
                   hex(0xffffff, here ? 0.85f : 0.42f), -1, 0.35f);
        }
        if (x > w || x + CW < 0) continue;
        cards_[i] = Rect{x, y, CW, CH};
        const bool cur = i == now.current, want = i == now.wanted, hov = cards_[i].in(mx_, my_) && !dragging_;
        unsigned char uc[3];
        menuUniverseColor(camp_, specOf(sg.s), uc);
        const Col ucol = rgb8(uc);
        if (cur) {
            p.setAdditive(true);
            p.rrect(x - 5 * s, y - 5 * s, CW + 10 * s, CH + 10 * s, 12 * s, hex(0xffffff, 0.16f), 14 * s);
            p.setAdditive(false);
        }
        p.rrect(x, y + 6 * s, CW, CH, 9 * s, hex(0x000000, 0.35f), 12 * s);
        p.rrect(x, y, CW, CH, 8 * s, hex(0x0c0c14, cur ? 0.94f : 0.84f));
        // the universe, and the song's thumbnail in its corner
        const float ix = x + pad, iy = y + pad;
        const float dim = cur || want || hov ? 1.f : 0.72f;
        if (GLuint t = worlds_->tex(specOf(sg.s))) p.image(t, ix, iy, IW, IH, Col{dim, dim, dim, 1.f}, true);
        else p.rrect(ix, iy, IW, IH, 5 * s, withA(mixc(ucol, hex(0x0c0c14), 0.7f), 1.f));
        const float tw = IW * 0.4f, th = tw * 9.f / 16.f, tx = ix + IW - tw - 5 * s, ty = iy + IH - th - 5 * s;
        if (GLuint t = thumb(sg.s->id)) {
            p.rrect(tx - 1.5f * s, ty - 1.5f * s, tw + 3 * s, th + 3 * s, 2 * s, hex(0x000000, 0.75f));
            p.image(t, tx, ty, tw, th, Col{dim, dim, dim, 1.f});
        }
        if (cur && now.duration > 0) { // how far into the song
            const float k = (float)std::clamp(now.time / now.duration, 0.0, 1.0);
            p.rrect(ix, iy + IH - 3 * s, IW, 3 * s, 0, hex(0x000000, 0.5f));
            p.rrect(ix, iy + IH - 3 * s, IW * k, 3 * s, 0, hex(0xffffff, 0.9f));
        }
        if (want) { // waiting for its song
            const float pulse = 0.55f + 0.45f * std::sin((float)time_ * 5.f);
            const char* label = cached_[i] ? "LOADING" : "DOWNLOADING";
            const float lw = p.width(F::JOSEFIN, label, 11 * s, 0.3f) + 20 * s;
            p.rrect(ix + IW * 0.5f - lw * 0.5f, iy + IH * 0.5f - 13 * s, lw, 24 * s, 12 * s, hex(0x05050a, 0.8f));
            p.text(F::JOSEFIN, label, ix + IW * 0.5f, iy + IH * 0.5f + 4 * s, 11 * s, withA(accent, 0.6f + 0.4f * pulse), 0, 0.3f);
        }
        // its title, universe and velocity
        const float ty0 = iy + IH + 18 * s;
        p.text(F::JOSEFIN, fit(p, F::JOSEFIN, asciiFold(sg.s->title), 12.5f * s, IW - 4 * s), ix + 2 * s, ty0, 12.5f * s,
               hex(0xffffff, cur ? 1.f : 0.8f));
        char vel[16];
        std::snprintf(vel, sizeof(vel), "%.1f", sg.s->level[diff_]);
        const float vw = p.width(F::JOSEFIN, vel, 10.5f * s, 0.1f);
        p.text(F::JOSEFIN, vel, ix + IW - 2 * s, ty0 + 18 * s, 10.5f * s, hex(0xe0e0f0, 0.6f), 1, 0.1f);
        p.text(F::JOSEFIN, fit(p, F::JOSEFIN, specOf(sg.s), 10.5f * s, IW - vw - 14 * s, 0.15f), ix + 2 * s, ty0 + 18 * s, 10.5f * s,
               withA(mixc(ucol, hex(0xffffff), 0.3f), cur ? 1.f : 0.75f), -1, 0.15f);
        const float pulse = 0.5f + 0.5f * std::sin((float)time_ * 5.f);
        p.rrectLine(x, y, CW, CH, 8 * s, (cur ? 1.8f : 1.2f) * s,
                    cur ? hex(0xffffff, 0.95f) : want ? withA(accent, 0.5f + 0.5f * pulse) : hex(0xffffff, hov ? 0.5f : 0.13f));
    }
}

void VoyageViewer::drawTimeline(int w, int h, const Now& now) {
    Painter& p = p_;
    const float s = h / 900.f;
    const float x0 = 28 * s, x1 = w - 28 * s, by = h - 64 * s, bh = 22 * s;
    p.rrect(-80 * s, h - 168 * s, w + 160 * s, 250 * s, 0, hex(0x05050a, 0.76f), 36 * s);
    bar_ = Rect{x0, by - 22 * s, x1 - x0, bh + 30 * s};
    const Col ink = hex(0xffffff), dim = hex(0xe0e0f0, 0.6f), accent = hex(0xffd27a);
    const int shownSong = now.current >= 0 ? now.current : now.wanted;
    const Song* sg = shownSong >= 0 ? &songs_[shownSong] : nullptr;
    // the song: where it is in the campaign, its title, universe and velocity
    if (sg) {
        char b[96];
        std::snprintf(b, sizeof(b), "STAGE %d   /   SONG %d OF %d   /   %s", sg->stage + 1, sg->song + 1,
                      (int)camp_->stages()[sg->stage].songs.size(), difficultyName(diff_));
        p.text(F::JOSEFIN, b, x0, h - 132 * s, 11.5f * s, dim, -1, 0.3f);
        unsigned char uc[3];
        menuUniverseColor(camp_, specOf(sg->s), uc);
        char vel[32];
        std::snprintf(vel, sizeof(vel), "VELOCITY %.1f", sg->s->level[diff_]);
        const float vw = p.text(F::JOSEFIN, vel, x1, h - 98 * s, 13 * s, dim, 1, 0.3f);
        const float uw = p.text(F::JOSEFIN, specOf(sg->s), x1 - vw - 28 * s, h - 98 * s, 14 * s, mixc(rgb8(uc), hex(0xffffff), 0.25f), 1, 0.3f);
        p.text(F::JOSEFIN_LIGHT, fit(p, F::JOSEFIN_LIGHT, asciiFold(sg->s->title), 28 * s, x1 - x0 - vw - uw - 80 * s), x0, h - 96 * s,
               28 * s, ink, -1, 0.01f);
    }
    // the tour, or the song coming
    {
        std::string right;
        Col c = dim;
        if (now.wanted >= 0) {
            right = std::string(cached_[now.wanted] ? "LOADING   " : "DOWNLOADING   ") + asciiFold(songs_[now.wanted].s->title);
            c = withA(accent, 0.65f + 0.35f * std::sin((float)time_ * 5.f));
        } else if (tour_ && now.an && !now.an->segments.empty()) {
            right = "TOUR   " + std::to_string(std::max(0, tourSeg_) + 1) + " / " + std::to_string(now.an->segments.size());
            c = accent;
        } else {
            right = "TOUR OFF";
        }
        right = fit(p, F::JOSEFIN, right, 11.5f * s, (x1 - x0) * 0.45f, 0.3f);
        p.text(F::JOSEFIN, right, x1, h - 132 * s, 11.5f * s, c, 1, 0.3f);
    }
    p.text(F::JOSEFIN, "SPACE TOUR     LEFT RIGHT SEEK     [ ] SECTION     UP DOWN SONG     PAGE UP DOWN STAGE     TAB HIDE", x0,
           h - 18 * s, 10 * s, hex(0xe0e0f0, 0.4f), -1, 0.25f);
    if (now.current < 0 || !now.an || now.duration <= 0) { // the first song isn't there yet
        p.rrect(x0, by, x1 - x0, bh, 5 * s, hex(0xffffff, 0.06f));
        const bool dl = now.wanted >= 0 && !cached_[now.wanted];
        p.text(F::JOSEFIN, dl ? "DOWNLOADING" : "LOADING", (x0 + x1) * 0.5f, by + bh * 0.5f + 4 * s, 11 * s,
               withA(accent, 0.6f + 0.4f * std::sin((float)time_ * 5.f)), 0, 0.35f);
        return;
    }
    const double dur = now.duration;
    auto X = [&](double t) { return x0 + (x1 - x0) * (float)std::clamp(t / dur, 0.0, 1.0); };
    const Analysis& an = *now.an;
    const int curSeg = an.segments.empty() ? -1 : an.segmentAt(now.time);
    // the sections
    for (int k = 0; k < (int)an.segments.size(); k++) {
        const Segment& sgm = an.segments[k];
        const float a = X(sgm.start) + 1 * s, b = X(sgm.end) - 1 * s;
        if (b - a < 1.f) continue;
        const bool here = k == curSeg;
        const Col c = kindCol(sgm.kind);
        p.rrect(a, by, b - a, bh, 4 * s, withA(here ? mixc(c, hex(0xffffff), 0.12f) : c, here ? 0.95f : 0.5f));
        p.rrect(a, by, b - a, bh * 0.45f, 4 * s, hex(0xffffff, 0.05f));
        const char* name = segmentName(sgm.kind);
        if (p.width(F::JOSEFIN, name, 10 * s, 0.2f) < b - a - 8 * s)
            p.text(F::JOSEFIN, name, (a + b) * 0.5f, by + bh * 0.5f + 3.6f * s, 10 * s, hex(0xffffff, here ? 0.95f : 0.7f), 0, 0.2f);
    }
    // the scene levels (calm, mid, peak) as steps over the sections
    if (now.plan) {
        const float base = by - 5 * s, step = 5 * s;
        float lastY = -1;
        for (const ScenePhase& ph : now.plan->phases) {
            const float y = base - ph.level * step, a = X(ph.start), b = X(ph.end);
            if (lastY >= 0) p.line(a, lastY, a, y, 1.2f * s, hex(0xffffff, 0.35f));
            p.line(a, y, b, y, 1.6f * s, hex(0xffffff, ph.level == 2 ? 0.7f : 0.4f));
            lastY = y;
        }
    }
    // the tour's few seconds in this section
    if (tour_ && tourSeg_ >= 0 && now.wanted < 0) {
        const float a = X(tourFrom_), b = X(tourUntil_), c = X(std::clamp(now.time, (double)tourFrom_, (double)tourUntil_));
        p.line(a, by + bh + 5 * s, b, by + bh + 5 * s, 2 * s, withA(accent, 0.25f));
        p.line(a, by + bh + 5 * s, c, by + bh + 5 * s, 2 * s, withA(accent, 0.9f));
    }
    // the playhead, the time under the pointer
    const float px = X(now.time);
    p.line(px, by - 16 * s, px, by + bh + 3 * s, 2 * s, ink);
    p.circle(px, by - 16 * s, 3.5f * s, ink);
    const std::string tt = clock(now.time) + " / " + clock(dur);
    p.text(F::JOSEFIN, tt, x1, h - 18 * s, 11 * s, hex(0xffffff, 0.75f), 1, 0.2f);
    if (dragTime_ >= 0) {
        const float gx = X(dragTime_);
        p.line(gx, by - 10 * s, gx, by + bh + 3 * s, 1.5f * s, hex(0xffffff, 0.45f));
        const std::string g = clock(dragTime_);
        const float gw = p.width(F::JOSEFIN, g, 11 * s, 0.2f) + 14 * s;
        p.rrect(gx - gw * 0.5f, by - 40 * s, gw, 20 * s, 6 * s, hex(0x05050a, 0.85f));
        p.text(F::JOSEFIN, g, gx, by - 26 * s, 11 * s, ink, 0, 0.2f);
    }
}

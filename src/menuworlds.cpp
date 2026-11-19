#include "menuworlds.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

#include "platform.hpp"
#include "web.hpp"
#include "universes/registry.hpp"

namespace fs = std::filesystem;

namespace {
double nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Colourfulness (Hasler & Suesstrunk: the spread and the mean of the opponent channels) plus contrast (the spread of
// the luminance), and the luminance's 5th / 95th percentiles; from every 4th pixel.
void imageStats(const std::vector<unsigned char>& px, float& colourful, float& lo, float& hi) {
    double srg = 0, syb = 0, srg2 = 0, syb2 = 0, sl = 0, sl2 = 0;
    int hist[256] = {};
    int n = 0;
    for (size_t i = 0; i + 2 < px.size(); i += 12) {
        const double r = px[i], g = px[i + 1], b = px[i + 2];
        const double rg = r - g, yb = 0.5 * (r + g) - b, l = 0.299 * r + 0.587 * g + 0.114 * b;
        srg += rg; syb += yb; srg2 += rg * rg; syb2 += yb * yb; sl += l; sl2 += l * l;
        hist[std::min(255, (int)l)]++;
        n++;
    }
    if (!n) { colourful = 0; lo = 0; hi = 1; return; }
    const double mrg = srg / n, myb = syb / n;
    const double vrg = srg2 / n - mrg * mrg, vyb = syb2 / n - myb * myb, vl = sl2 / n - (sl / n) * (sl / n);
    colourful = (float)((std::sqrt(std::max(0.0, vrg + vyb)) + 0.3 * std::sqrt(mrg * mrg + myb * myb) + 0.5 * std::sqrt(std::max(0.0, vl))) / 255.0);
    int acc = 0, l5 = 0, l95 = 255;
    for (int k = 0; k < 256; k++) {
        acc += hist[k];
        if (acc < n * 0.05) l5 = k;
        if (acc < n * 0.95) l95 = k;
    }
    lo = l5 / 255.f;
    hi = std::max(lo + 0.05f, l95 / 255.f);
}

uint64_t specSeed(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ (unsigned char)c) * 1099511628211ull;
    return h;
}
} // namespace

namespace {
std::function<bool()> g_bindWorker;
std::function<void()> g_unbindWorker;
} // namespace

void WorldPool::setWorkerContext(std::function<bool()> bind, std::function<void()> unbind) {
    g_bindWorker = std::move(bind);
    g_unbindWorker = std::move(unbind);
}

WorldPool::WorldPool() {
    blank_.makeBlank();
    if (g_bindWorker) {
        workerOn_ = true;
        worker_ = std::thread([this] { workerLoop(); });
    }
}

WorldPool::~WorldPool() {
    if (worker_.joinable()) {
        { std::lock_guard<std::mutex> lk(wm_); stop_ = true; jobs_.clear(); }
        wcv_.notify_all();
        worker_.join(); // (the snapshot being made is finished first)
    }
}

// The worker: one snapshot at a time, in its own context; the pixels go to the cache file and back to the main thread.
void WorldPool::workerLoop() {
    if (!g_bindWorker()) {
        std::fprintf(stderr, "[menu] no worker context: snapshots are made on the main thread\n");
        std::lock_guard<std::mutex> lk(wm_);
        workerOn_ = false;
        return;
    }
    Game blank;
    blank.makeBlank();
    const std::vector<HudText> noHud;
    const std::vector<HudRect> noPanels;
    MusicState music;
    music.hasSong = true;
    music.bpm = 112.f;
    for (;;) {
        std::string spec;
        {
            std::unique_lock<std::mutex> lk(wm_);
            wcv_.wait(lk, [&] { return stop_ || !jobs_.empty(); });
            if (stop_) break;
            spec = jobs_.front();
            jobs_.pop_front();
            busy_ = spec;
        }
        int variant = -1;
        const int idx = universes::parse(spec, &variant);
        std::vector<unsigned char> px;
        if (idx >= 0) {
            std::unique_ptr<Universe> u = universes::entry(idx).make();
            u->init(LW, LH);
            SongLook look = makeSongLook(nullptr, specSeed(spec));
            look.hasSong = true;
            look.variant = variant;
            look.bpm = 112.f;
            look.duration = look.remaining = 240.f;
            u->setSceneLines(sceneLinesFor(look));
            u->setSong(look, 0.f);
            u->setLevel(1, 0.6f, 0.f, 0.f);
            ugl::Target tgt;
            tgt.create(LW, LH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
            for (int i = 0; i < 10; i++) {
                music.beat = i * 0.2;
                const Frame f{blank, music, 1.0 + i * 0.1, 0.1f, false, false, 1.f, noHud, noPanels, true};
                tgt.bind();
                u->render(f, tgt.fbo);
            }
            px.resize((size_t)SW * SH * 3);
            glBindFramebuffer(GL_FRAMEBUFFER, tgt.fbo);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, SW, SH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            tgt.destroy();
            u.reset();
            glFinish();
            std::error_code ec;
            fs::create_directories(fs::path(path(spec)).parent_path(), ec);
            std::ofstream out(path(spec), std::ios::binary);
            if (out) out.write((const char*)px.data(), (std::streamsize)px.size());
        }
        std::lock_guard<std::mutex> lk(wm_);
        if (px.empty()) failed_.push_back(spec);
        else done_.push_back({spec, std::move(px)});
        busy_.clear();
    }
    g_unbindWorker();
}

void WorldPool::uploadSnap(const std::string& spec, const std::vector<unsigned char>& px) {
    Snap& s = snaps_[spec];
    if (!s.tgt.tex) s.tgt.create(SW, SH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false);
    glBindTexture(GL_TEXTURE_2D, s.tgt.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SW, SH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    imageStats(px, s.colourful, s.lo, s.hi);
    s.ok = s.saved = s.tried = true;
}

std::string WorldPool::path(const std::string& spec) const {
    std::string n;
    for (char c : spec) n += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
    return (platform::appCacheDir() / "menu-worlds" / (n + "-v2.rgb")).string();
}

void WorldPool::want(const std::string& spec, float fps) {
    if (spec.empty()) return;
    if (!wanted_.count(spec)) wantOrder_.push_back(spec);
    float& f = wanted_[spec];
    f = std::max(f, fps);
}

std::unique_ptr<Universe> WorldPool::make(const std::string& spec, double time) {
    int variant = -1;
    const int idx = universes::parse(spec, &variant);
    if (idx < 0) return nullptr;
    const double t0 = nowMs();
    std::unique_ptr<Universe> u = universes::entry(idx).make();
    if (!u->init(LW, LH)) std::fprintf(stderr, "[menu] universe %s: init reported a GL error\n", spec.c_str());
    SongLook look = makeSongLook(nullptr, specSeed(spec));
    look.hasSong = true;
    look.variant = variant;
    look.bpm = 112.f;
    look.duration = look.remaining = 240.f;
    u->setSceneLines(sceneLinesFor(look));
    u->setSong(look, 0.f);
    u->setLevel(1, 0.6f, 0.f, 0.f);
    makeMs += nowMs() - t0;
    made++;
    (void)time;
    return u;
}

void WorldPool::renderOne(Live& l, const std::string& spec, double time, float dt, const MusicState& music) {
    (void)spec;
    const double t0 = nowMs();
    const Frame f{blank_, music, time, dt, false, false, 1.f, noHud_, noPanels_, true};
    l.tgt.bind();
    l.u->render(f, l.tgt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    l.last = time;
    renderMs += nowMs() - t0;
    rendered++;
}

void WorldPool::snapshot(const std::string& spec, const ugl::Target& from) {
    Snap& s = snaps_[spec];
    if (!s.tgt.fbo) s.tgt.create(SW, SH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, from.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.tgt.fbo);
    glBlitFramebuffer(0, 0, from.w, from.h, 0, 0, SW, SH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s.ok = s.tried = true;
    snapped++;
    if (!s.saved) saveSnap(spec);
}

void WorldPool::saveSnap(const std::string& spec) {
    Snap& s = snaps_[spec];
    std::vector<unsigned char> px((size_t)SW * SH * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, s.tgt.fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, SW, SH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    imageStats(px, s.colourful, s.lo, s.hi);
    std::error_code ec;
    fs::create_directories(fs::path(path(spec)).parent_path(), ec);
    std::ofstream f(path(spec), std::ios::binary);
    if (!f) return;
    f.write(px.data() ? (const char*)px.data() : "", (std::streamsize)px.size());
    s.saved = true;
}

bool WorldPool::loadSnap(const std::string& spec) {
    Snap& s = snaps_[spec];
    s.tried = true;
    std::ifstream f(path(spec), std::ios::binary);
#ifdef REFALL_WEB
    if (!f) f.open(std::string(web::WORLDS_DIR) + "/" + fs::path(path(spec)).filename().string(), std::ios::binary);
#endif
    if (!f) return false;
    std::vector<unsigned char> px((size_t)SW * SH * 3);
    f.read((char*)px.data(), (std::streamsize)px.size());
    if (f.gcount() != (std::streamsize)px.size()) return false;
    s.tgt.create(SW, SH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false);
    glBindTexture(GL_TEXTURE_2D, s.tgt.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SW, SH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    imageStats(px, s.colourful, s.lo, s.hi);
    s.ok = s.saved = true;
    return true;
}

void WorldPool::update(double time, float dt, const MusicState& music) {
    made = rendered = snapped = 0;
    makeMs = renderMs = 0;
    if (snapshotsOnly_) {
        // from the disk cache: cheap, two a frame (the wanted ones first, then the prefetch list)
        int loads = 0;
        for (const std::vector<std::string>* list : {&wantOrder_, &prefetch_})
            for (size_t i = 0; i < list->size() && loads < 2; i++) {
                const std::string& spec = (*list)[i];
                Snap& s = snaps_[spec];
                if (s.ok || s.tried) continue;
                loadSnap(spec);
                loads++;
            }
        // the worker's snapshots (an upload each, about a millisecond), and its queue: the wanted ones first
        if (workerOn_) {
            std::vector<std::pair<std::string, std::vector<unsigned char>>> done;
            {
                std::lock_guard<std::mutex> lk(wm_);
                done.swap(done_);
                for (const std::string& f : failed_) snaps_[f].dead = true;
                failed_.clear();
            }
            for (auto& [spec, px] : done) uploadSnap(spec, px);
            if (generate_ && !idle_) { // a key pressed lately: no new snapshot is started (its init would contend with the frame)
                std::lock_guard<std::mutex> lk(wm_);
                jobs_.clear();
            } else if (generate_) {
                std::lock_guard<std::mutex> lk(wm_);
                auto queued = [&](const std::string& s) { return s == busy_ || std::find(jobs_.begin(), jobs_.end(), s) != jobs_.end(); };
                std::deque<std::string> next;
                for (const std::vector<std::string>* list : {&wantOrder_, &prefetch_})
                    for (const std::string& spec : *list) {
                        const Snap& s = snaps_[spec];
                        if (s.ok || s.dead || !s.tried || spec == busy_) continue;
                        if (std::find(next.begin(), next.end(), spec) == next.end()) next.push_back(spec);
                    }
                (void)queued;
                jobs_.swap(next); // the current priorities (the wanted ones first)
            }
            if (!jobs_.empty()) wcv_.notify_one();
        }
        // without a worker: a missing one made on the main thread, at most one, spaced out, only while idle
        else if (generate_ && idle_ && time - lastHeavy_ >= gap_) {
            for (const std::vector<std::string>* list : {&wantOrder_, &prefetch_}) {
                bool done = false;
                for (size_t i = 0; i < list->size(); i++) {
                    const std::string& spec = (*list)[i];
                    Snap& s = snaps_[spec];
                    if (s.ok || s.dead || !s.tried) continue;
                    makeSnapshot(spec, time, music);
                    lastHeavy_ = time;
                    done = true;
                    break;
                }
                if (done) break;
            }
        }
        wanted_.clear();
        wantOrder_.clear();
        return;
    }
    bool madeOne = false;
    // the live ones: render when due, make the missing ones (the fastest wanted first)
    std::vector<std::pair<float, std::string>> order;
    for (auto& [spec, fps] : wanted_) order.push_back({-fps, spec});
    std::sort(order.begin(), order.end());
    for (auto& [negFps, spec] : order) {
        const float fps = -negFps;
        if (fps <= 0) continue;
        auto it = live_.find(spec);
        if (it == live_.end()) {
            if (madeOne || snaps_[spec].dead) continue;
            if ((int)live_.size() >= MAX_LIVE) { // the least recently wanted goes
                auto old = std::min_element(live_.begin(), live_.end(), [](auto& a, auto& b) { return a.second.lastWanted < b.second.lastWanted; });
                if (old->second.last >= 0) snapshot(old->first, old->second.tgt);
                old->second.tgt.destroy();
                live_.erase(old);
            }
            Live l;
            l.u = make(spec, time);
            madeOne = true;
            if (!l.u) { snaps_[spec].dead = true; continue; }
            l.tgt.create(LW, LH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
            l.born = time;
            // a few quick frames so it shows a settled world at once
            for (int i = 0; i < 4; i++) renderOne(l, spec, time - 0.6 + i * 0.15, 0.15f, music);
            it = live_.emplace(spec, std::move(l)).first;
        }
        Live& l = it->second;
        l.lastWanted = time;
        if (time - l.last >= 1.0 / fps - 1e-4) renderOne(l, spec, time, (float)std::min(0.25, time - l.last), music);
        if (!l.snapDone && time - l.born > 1.0) { snapshot(spec, l.tgt); l.snapDone = true; }
    }
    // live ones not wanted for a while go (their snapshot stays)
    for (auto it = live_.begin(); it != live_.end();) {
        if (time - it->second.lastWanted > 2.0) {
            if (it->second.last >= 0) snapshot(it->first, it->second.tgt);
            it->second.tgt.destroy();
            it = live_.erase(it);
        } else ++it;
    }
    // snapshots: from the disk cache, else rendered once by a short-lived instance
    for (auto& [spec, fps] : wanted_) {
        Snap& s = snaps_[spec];
        if (s.ok || s.dead || live_.count(spec)) continue;
        if (!s.tried && loadSnap(spec)) continue;
        if (madeOne) continue;
        Live l;
        l.u = make(spec, time);
        madeOne = true;
        if (!l.u) { s.dead = true; continue; }
        l.tgt.create(LW, LH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        for (int i = 0; i < 10; i++) renderOne(l, spec, time + i * 0.1, 0.1f, music);
        snapshot(spec, l.tgt);
        l.tgt.destroy();
    }
    wanted_.clear();
}

GLuint WorldPool::tex(const std::string& spec) const {
    auto it = live_.find(spec);
    if (it != live_.end() && it->second.last >= 0) return it->second.tgt.tex;
    auto s = snaps_.find(spec);
    return s != snaps_.end() && s->second.ok ? s->second.tgt.tex : 0;
}

bool WorldPool::makeSnapshot(const std::string& spec, double time, const MusicState& music) {
    Live l;
    l.u = make(spec, time);
    if (!l.u) { snaps_[spec].dead = true; return false; }
    l.tgt.create(LW, LH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
    for (int i = 0; i < 10; i++) renderOne(l, spec, time + i * 0.1, 0.1f, music);
    snapshot(spec, l.tgt);
    l.tgt.destroy();
#ifdef REFALL_WEB
    std::printf("[menu] snapshot of %s made on the main thread (%.0f ms)\n", spec.c_str(), makeMs + renderMs);
#endif
    return true;
}

void WorldPool::retryMissing() {
    for (auto& [spec, s] : snaps_)
        if (!s.ok) s.tried = false;
}

int WorldPool::missing() const {
    int n = 0;
    for (const std::string& s : prefetch_) {
        auto it = snaps_.find(s);
        if (it == snaps_.end() || (!it->second.ok && !it->second.dead)) n++;
    }
    return n;
}

bool WorldPool::stats(const std::string& spec, float& colourful, float& lo, float& hi) const {
    auto it = snaps_.find(spec);
    if (it == snaps_.end() || !it->second.ok) return false;
    colourful = it->second.colourful;
    lo = it->second.lo;
    hi = it->second.hi;
    return true;
}

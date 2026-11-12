#include "menuworlds.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "platform.hpp"
#include "universes/registry.hpp"

namespace fs = std::filesystem;

namespace {
double nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
uint64_t specSeed(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ (unsigned char)c) * 1099511628211ull;
    return h;
}
} // namespace

WorldPool::WorldPool() { blank_.makeBlank(); }
WorldPool::~WorldPool() = default;

std::string WorldPool::path(const std::string& spec) const {
    std::string n;
    for (char c : spec) n += std::isalnum((unsigned char)c) ? (char)std::tolower((unsigned char)c) : '_';
    return (platform::cacheDir() / "zentris" / "menu-worlds" / (n + "-v1.rgb")).string();
}

void WorldPool::want(const std::string& spec, float fps) {
    if (spec.empty()) return;
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
    if (!f) return false;
    std::vector<unsigned char> px((size_t)SW * SH * 3);
    f.read((char*)px.data(), (std::streamsize)px.size());
    if (f.gcount() != (std::streamsize)px.size()) return false;
    s.tgt.create(SW, SH, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false);
    glBindTexture(GL_TEXTURE_2D, s.tgt.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SW, SH, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    s.ok = s.saved = true;
    return true;
}

void WorldPool::update(double time, float dt, const MusicState& music) {
    made = rendered = snapped = 0;
    makeMs = renderMs = 0;
    if (snapshotsOnly_) {
        int loads = 0;
        for (auto& [spec, fps] : wanted_) {
            Snap& s = snaps_[spec];
            if (s.ok || s.tried || loads >= 2) continue;
            loadSnap(spec);
            loads++;
        }
        wanted_.clear();
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

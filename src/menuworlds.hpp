#pragma once
// Live previews of the game's universes for the menus (WINDOWS and KALEIDO styles): real universe instances rendering
// their bare world (no board, no HUD) into small textures, at a low resolution and their own frame rate, plus a
// snapshot per universe (kept on disk) shown while an instance isn't live.
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>

#include "game.hpp"
#include "ugl.hpp"
#include "universe.hpp"

class WorldPool {
public:
    static constexpr int LW = 640, LH = 360;  // a live instance's frame
    static constexpr int SW = 640, SH = 360;  // a snapshot
    static constexpr int MAX_LIVE = 8;

    WorldPool();
    ~WorldPool();
    // Each frame: say what shows (a universe spec "NAME" or "NAME:VARIANT"; fps > 0 wants it live at that rate,
    // 0 a snapshot is enough), then update() renders what is due. Creating an instance (its init) costs the most:
    // at most one per frame, the live ones first.
    void want(const std::string& spec, float fps);
    void update(double time, float dt, const MusicState& music);
    // Snapshots only: update() just loads cached snapshots from the disk (a few per frame), never makes an instance
    // (the menu: an instance's init costs 100-500 ms on the main thread).
    void setSnapshotsOnly(bool on) { snapshotsOnly_ = on; }
    // With snapshots only, a missing snapshot may still be made (an instance, a few frames, then gone: 100-500 ms on
    // the main thread), one at a time, at least `gap` seconds apart, only while `idle` (no key pressed lately): the
    // ones wanted first, then the `prefetch` list in its order.
    void setGenerate(bool on, float gap = 0.4f) { generate_ = on; gap_ = gap; }
    void setIdle(bool idle) { idle_ = idle; }
    void prefetch(const std::vector<std::string>& specs) { prefetch_ = specs; }
    int missing() const; // prefetch entries without a snapshot yet
    // A worker thread with its own OpenGL context makes the missing snapshots (an instance's init, 100 ms to seconds,
    // never on the main thread): `bind` makes that context current on the calling thread, `unbind` releases it. Set
    // once by the app; without it the missing snapshots are made on the main thread while the menu is idle.
    static void setWorkerContext(std::function<bool()> bind, std::function<void()> unbind);
    // The texture to show (live frame, else the snapshot), 0 while neither exists yet.
    GLuint tex(const std::string& spec) const;
    bool isLive(const std::string& spec) const { return live_.count(spec) != 0; }
    // what the pool did this frame (for the logs): instances made, frames rendered, snapshots made
    int made = 0, rendered = 0, snapped = 0;
    double makeMs = 0, renderMs = 0;

private:
    struct Live {
        std::unique_ptr<Universe> u;
        ugl::Target tgt;
        double last = -1, lastWanted = 0, born = 0;
        bool snapDone = false;
    };
    struct Snap {
        ugl::Target tgt;
        bool ok = false, tried = false, saved = false, dead = false;
    };
    std::map<std::string, Live> live_;
    std::map<std::string, Snap> snaps_;
    std::map<std::string, float> wanted_;
    bool snapshotsOnly_ = false, generate_ = false, idle_ = false;
    float gap_ = 0.4f;
    double lastHeavy_ = -1e9;
    std::vector<std::string> wantOrder_, prefetch_;
    bool makeSnapshot(const std::string& spec, double time, const MusicState& music);
    // the worker
    std::thread worker_;
    std::mutex wm_;
    std::condition_variable wcv_;
    std::deque<std::string> jobs_;                                         // specs to make
    std::vector<std::pair<std::string, std::vector<unsigned char>>> done_; // made: their pixels (RGB, SW x SH)
    std::vector<std::string> failed_;
    std::string busy_;
    bool stop_ = false;
    std::atomic<bool> workerOn_{false};
    void workerLoop();
    void uploadSnap(const std::string& spec, const std::vector<unsigned char>& px);
    Game blank_;
    std::vector<HudText> noHud_;
    std::vector<HudRect> noPanels_;
    std::unique_ptr<Universe> make(const std::string& spec, double time);
    void renderOne(Live& l, const std::string& spec, double time, float dt, const MusicState& music);
    void snapshot(const std::string& spec, const ugl::Target& from);
    bool loadSnap(const std::string& spec);
    void saveSnap(const std::string& spec);
    std::string path(const std::string& spec) const;
};

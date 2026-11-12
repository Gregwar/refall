#pragma once
// Live previews of the game's universes for the menus (WINDOWS and KALEIDO styles): real universe instances rendering
// their bare world (no board, no HUD) into small textures, at a low resolution and their own frame rate, plus a
// snapshot per universe (kept on disk) shown while an instance isn't live.
#include <cstdint>
#include <map>
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
    static constexpr int SW = 320, SH = 180;  // a snapshot
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
    bool snapshotsOnly_ = false;
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

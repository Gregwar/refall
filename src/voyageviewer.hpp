#pragma once
// refall-voyage-viewer: a campaign reviewed with the game playing itself. The app plays each song in its universe (the
// computer clearing lines); this is the overlay over it, drawn with the menu's painter: a strip of the campaign's songs
// by stage (each card the universe in miniature, the song's YouTube thumbnail, its title, universe and velocity), the
// current song's timeline (its sections, scene levels, the playhead), and the tour that plays each section of a song a
// few seconds, then moves on to the next song.
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <glad/gl.h>

#include "analysis.hpp"
#include "campaign.hpp"
#include "input.hpp"
#include "menupaint.hpp"
#include "songplan.hpp"

struct GLFWwindow;
class WorldPool;

class VoyageViewer {
public:
    struct Song {
        int stage = 0, song = 0;
        const CampaignSong* s = nullptr;
    };
    // What the app is doing, each frame.
    struct Now {
        int current = -1;   // the song playing (-1: none yet)
        int wanted = -1;    // a song jumped to, still loading
        double time = 0, duration = 0;
        const Analysis* an = nullptr;
        const SongPlan* plan = nullptr;
        bool paused = false;
    };
    // What the viewer asks of the app: play a song, or seek in the current one.
    struct Action {
        enum Kind { JUMP, SEEK } kind = JUMP;
        int index = 0;
        double time = 0;
    };

    VoyageViewer(const Campaign* c, int difficulty);
    ~VoyageViewer();
    int count() const { return (int)songs_.size(); }
    const Song& song(int i) const { return songs_[i]; }
    std::string entry(int i) const { return songs_[i].s->entry(); }
    int find(int stage, int song) const;              // -1 if the campaign has no such song
    int indexOf(const std::string& path) const;       // the song whose YouTube id is in a track's path, -1 none
    // One frame: the keys and the mouse (no window: none), the tour; the actions for the app to perform.
    std::vector<Action> update(GLFWwindow* win, const Input& in, const Now& now, float dt);
    // A command, as a key gives it (also the test hook ZEN_VIEWER_KEYS): next, prev, stage+, stage-, fwd, back, sec+,
    // sec-, tour, hide.
    void command(const std::string& c) { pending_.push_back(c); }
    void songStarted() { tourSeg_ = -1; } // the tour starts again with the new song
    void draw(int w, int h, const Now& now);
    bool visible() const { return shown_; }

private:
    const Campaign* camp_;
    int diff_;
    std::vector<Song> songs_;
    Painter p_;
    std::unique_ptr<WorldPool> worlds_;
    double time_ = 0, lastDraw_ = 0;
    float dt_ = 0;
    bool shown_ = true, tour_ = true;
    float alpha_ = 1.f;
    std::vector<std::string> pending_;
    // the tour: the section it is on (-1: to pick from the playhead), and when it moves on (song time)
    int tourSeg_ = -1;
    double tourFrom_ = 0, tourUntil_ = 0;
    // keys (edges, held time for the seek's repeat), the mouse (framebuffer pixels)
    std::map<int, bool> keyWas_;
    std::map<int, float> keyHeld_;
    float mx_ = -1, my_ = -1, scrollIdle_ = 99.f;
    bool mouseWas_ = false, dragging_ = false;
    double dragTime_ = -1; // the time under the pointer on the timeline (-1: not over it)
    // the strip's scroll (pixels, eased), a manual offset from the wheel
    float scroll_ = 0, scrollManual_ = 0;
    bool scrollSnap_ = true;
    // the layout of the last frame (for the mouse)
    struct Rect { float x = 0, y = 0, w = 0, h = 0; bool in(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
    std::vector<Rect> cards_;
    Rect bar_;
    int fbw_ = 1, fbh_ = 1;
    // which songs are downloaded (checked every few seconds)
    std::vector<bool> cached_;
    float cachedAge_ = 99.f;

    // the thumbnails: downloaded (curl) and decoded on a thread, then made into textures
    struct Thumb { int w = 0, h = 0; std::vector<unsigned char> px; };
    std::map<std::string, GLuint> thumbTex_;
    std::set<std::string> thumbAsked_;
    std::thread thumbThread_;
    std::mutex tm_;
    std::condition_variable tcv_;
    std::deque<std::string> thumbJobs_;
    std::vector<std::pair<std::string, Thumb>> thumbDone_;
    bool thumbStop_ = false;
    void thumbLoop();
    GLuint thumb(const std::string& id);

    int focus(const Now& now) const { return now.wanted >= 0 ? now.wanted : std::max(0, now.current); }
    bool key(GLFWwindow* win, int k, bool repeat = false, float dt = 0.f);
    void tour(const Now& now, std::vector<Action>& out);
    void drawStrip(int w, int h, const Now& now);
    void drawTimeline(int w, int h, const Now& now);
};

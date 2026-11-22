// Refall: a minimalist, music-driven falling-block puzzle game.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "icon.inc"
#include "title.hpp"
#include "headless.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include "audio.hpp"
#include "campaign.hpp"
#include "game.hpp"
#include "input.hpp"
#include "library.hpp"
#include "menu.hpp"
#include "menuworlds.hpp"
#include "platform.hpp"
#include "renderer.hpp"
#include "saveslots.hpp"
#include "songplan.hpp"
#include "voyages.hpp"
#include "voyageviewer.hpp"
#include "web.hpp"
#include "youtube.hpp"

namespace fs = std::filesystem;

#if defined(_WIN32)
// Laptops with two GPUs: ask the NVIDIA and AMD drivers for the discrete one.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// Linux machines with two GPUs (an integrated one driving the screen, a discrete one for offload): render on the
// discrete one through PRIME offload, unless the user already chose (their own variables, or REFALL_GPU=integrated).
// Returns whether it changed anything, so a failed window can be retried without it.
static bool preferDiscreteGpu() {
#if defined(__linux__)
    const char* pref = std::getenv("REFALL_GPU");
    if (pref && std::strcmp(pref, "integrated") == 0) return false;
    if (std::getenv("__NV_PRIME_RENDER_OFFLOAD") || std::getenv("__GLX_VENDOR_LIBRARY_NAME") || std::getenv("DRI_PRIME"))
        return false;
    if (fs::exists("/proc/driver/nvidia/version")) {
        setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
        setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
        setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
    } else {
        setenv("DRI_PRIME", "1", 0); // Mesa: the other GPU when there is one, no change otherwise
    }
    return true;
#else
    return false;
#endif
}
static void undoDiscreteGpu() {
#if defined(__linux__)
    unsetenv("__NV_PRIME_RENDER_OFFLOAD");
    unsetenv("__GLX_VENDOR_LIBRARY_NAME");
    unsetenv("__VK_LAYER_NV_optimus");
    unsetenv("DRI_PRIME");
#endif
}

struct Options {
    std::vector<std::string> paths;
    bool cheatSkip = false;  // --cheat-skip: S completes the song
    bool autoplay = false, mute = false, fullscreen = false, hidden = false, shuffle = true;
    bool stream = false; // --stream: played from afar (tools/stream.py)
    std::string shotPrefix;
    int shotCount = 0;
    bool phaseShots = false;
    float shotTime = 25.f;
    uint64_t seed = 0;
    std::string clipPath; // --clip: record a video around the first drop of the first song, then exit
    float clipBefore = 4.f, clipAfter = 6.f;
    float clipAt = -1.f; // the clip's moment (song seconds), the first drop when < 0
    bool clipBreak = false; // --clip-break: the clip spans the break after the first song
    std::string analysisPath; // --analysis: write the first song's beat grid, structure and scene plan as JSON, then exit
    std::string universe;   // forced universe "NAME" or "NAME:VARIANT" (as shown in the corner), "" = picked per song
    bool universeOnPeak = false; // also change universe when a long song rises into a peak
    std::string toUniverse, transition; // --transition-shots: the universe changed to, the transition forced
    std::string transitionShots; int transitionShotCount = 8;
    std::string reviewPath; // --review-shot: one snapshot of a song scene with a filled board, then exit
    int reviewLevel = -1;   // scene level to show (0 calm, 1 mid, 2 peak), -1 any
    int width = 1600, height = 900;
    // Modes: the title screen (the default when the game is started with no songs and no offline render), free
    // play straight away (--free, or songs given), or a campaign stage straight away (--stage).
    bool menu = false;         // start on the title screen
    bool menuEnabled = false;  // game overs and Q go back to the menu (the title screen or --stage, when playing)
    int difficulty = DIFF_HARD; bool difficultyGiven = false;
    int level = 1; bool levelGiven = false; // --level: free play's starting level
    float speedMul = 1.f;      // free play's gravity multiplier (--difficulty: HARD 1, or a number)
    std::string stage;         // --stage S[:SONG]: a campaign run straight away
    std::string campaignPath, progressPath, playlistsPath;
    std::string menuShot, menuScreen = "title", menuHover; // --menu-shot: a screenshot of a menu, then exit
    float menuTime = 0.f;      // --menu-time: the shot taken this many seconds into the screen (a debrief's animation)
    std::string menuClip;      // --menu-clip: a video of the menus walked through, then exit
    bool menuDemo = false;
    std::string menuDemoKind; // --menu-demo normal-done: every stage cleared on NORMAL, some on EASY, none on HARD
    bool menuMusic = true;     // --no-menu-music: the menu stays silent (no music bed)
    // refall-voyage-viewer: a campaign reviewed (its songs in their universes, the computer playing), from a song
    bool viewer = false;
    int viewStage = 1, viewSong = 1;
    std::string viewShot;     // --shot: the viewer saved after --shot-after seconds of the first song, then exit
    float viewShotAfter = 8.f;
    bool menuPetal = false;    // --menu-petal: shots with the hovered stage's petal open (KALEIDO)     // --menu-demo: made-up campaign progress (never saved), for shots and clips
};

// Test hook: REFALL_TEST_PLAY=1 runs the game as when played (the menus, breaks, game overs) without a window: on the GPU
// with no sound, the songs on a simulated clock; REFALL_TEST_TAIL=S starts each song S seconds before its end,
// REFALL_TEST_KEYS="T:KEY,..." presses keys (up, down, left, right, ok, back, pause, quit, skip) at T seconds of the run,
// REFALL_TEST_SHOTS="T:FILE.png,..." saves the frame at T seconds, REFALL_TEST_QUIT=T stops the run at T seconds.
static bool g_refresh = false; // the window must be drawn again (uncovered)
static std::chrono::steady_clock::time_point g_startTime; // (the menu's first frame is timed from it)
static bool testPlay() {
    static const bool on = std::getenv("REFALL_TEST_PLAY") != nullptr;
    return on;
}
static std::vector<std::pair<double, std::string>> testList(const char* env) {
    std::vector<std::pair<double, std::string>> out;
    const char* v = std::getenv(env);
    std::string s = v ? v : "";
    for (size_t a = 0; a < s.size();) {
        size_t b = s.find(',', a);
        if (b == std::string::npos) b = s.size();
        const std::string item = s.substr(a, b - a);
        const size_t c = item.find(':');
        if (c != std::string::npos) out.push_back({std::atof(item.substr(0, c).c_str()), item.substr(c + 1)});
        a = b + 1;
    }
    return out;
}

// Test hook: REFALL_WARMUP=S plays S seconds (default 12) before a shot or a clip, so that the board fills and a
// universe that accumulates (FLUO's paint) has time to build up.
// Test hook: REFALL_CLIP_VELOCITY / REFALL_CLIP_PACE given as "A:B" ramp from A to B over the song times of REFALL_CLIP_RAMP
// ("T0:T1"); a single number is constant. (tools/demo.py: every shot of a continuous trailer at the same values.)
static float clipRamp(const char* var, double songTime, float none) {
    const char* v = std::getenv(var);
    if (!v) return none;
    float a = 0.f, b = 0.f;
    if (std::sscanf(v, "%f:%f", &a, &b) != 2) return (float)std::atof(v);
    float t0 = 0.f, t1 = 1.f;
    const char* r = std::getenv("REFALL_CLIP_RAMP");
    if (!r || std::sscanf(r, "%f:%f", &t0, &t1) != 2 || t1 <= t0) return b;
    return a + (b - a) * saturate(((float)songTime - t0) / (t1 - t0));
}

static double warmup() {
    static const double w = std::getenv("REFALL_WARMUP") ? std::max(0.0, std::atof(std::getenv("REFALL_WARMUP"))) : 12.0;
    return w;
}

static void usage() {
    std::printf(
        "usage: refall [options] [songs or folders...]\n"
        "  (default: a built-in YouTube playlist, via yt-dlp; local files, folders and other YouTube\n"
        "   playlist/video URLs work too)\n"
        "  --fullscreen         start fullscreen\n"
        "  --size WxH           window size (default 1600x900)\n"
        "  --seed N             fixed run seed (default: random each run)\n"
        "  --universe NAME[:VARIANT]  play every song in this universe (and variant), as shown in the bottom-left\n"
        "                       corner (e.g. vector, vector:p3 amber, vector:1); T / Y moves on to the next one\n"
        "  --list               print the universes and their variants, then exit\n"
        "  --universe-on-peak   also change universe when a song rises into a peak after a minute in one universe\n"
        "  --in-order           play songs in order (default: random order)\n"
        "  --autoplay           let the computer play\n"
        "  --cheat-skip         S completes the song, or on the Voyage's screen clears the focused stage (testing;\n"
        "                       S is then no soft drop); Ctrl+Shift+Left/Right also seek in a Voyage's song\n"
        "  --mute               no sound\n"
        "  --stream             played from afar (tools/stream.py runs it): no window, in real time, each frame raw RGB\n"
        "                       on the fd REFALL_STREAM_VIDEO_FD, the sound (16-bit stereo) on REFALL_STREAM_AUDIO_FD, a\n"
        "                       gamepad's state per line on stdin (\"pad BUTTONS AXES\": 15 digits, 6 numbers)\n"
        "  --shots PREFIX N     render N screenshots of different scenes then exit (testing)\n"
        "  --shot-time SEC      song position used for screenshots (default 25)\n"
        "  --phase-shots PREFIX one screenshot per phase of the first song, then exit (testing)\n"
        "  --review-shot OUT.png  one screenshot of the first song's universe (random, or --universe), in a\n"
        "                       random part of the song (of level --review-level 0|1|2), with a filled board;\n"
        "                       prints a [review] JSON line describing it, then exits (tools/scenereview.py)\n"
        "  --transition-shots PREFIX N  at --shot-time in the first song (with a filled board), changes from the\n"
        "                       universe (--universe) to --to NAME[:VARIANT] (default: the next one) with --transition\n"
        "                       NAME (default: picked), saves N frames across the transition, then exits (testing)\n"
        "  --clip OUT.mp4       record a video (with sound, needs ffmpeg) around the first drop of the first\n"
        "                       song, then exit; --clip-before / --clip-after SEC set the span (default 4 / 6),\n"
        "                       --clip-at SEC records around that song time instead of the drop, --clip-break around\n"
        "                       the break between the first song and the next (from 9 s before its end)\n"
        "  --analysis OUT.json  write the first song's analysis (beats, downbeat, structure, scene levels, first\n"
        "                       drop) as JSON, then exit (tools/demo.py cuts on it)\n"
        "modes (with no songs and no option above, the game opens on its title screen: VOYAGE, FREE PLAY):\n"
        "  --free               free play straight away, without the menu (also when songs are given)\n"
        "  --difficulty D       easy, normal, hard (default) or expert (e/n/h/x), or a gravity multiplier (e.g. 0.7):\n"
        "                       free play's speed (EASY x0.6, NORMAL x0.75, HARD x1, EXPERT x1.4), the voyage's difficulty\n"
        "  --level N            free play's starting level (1..40, default 1; the menu picks the nearest of its steps)\n"
        "  --stage S[:SONG]     a voyage run straight away: stage S (1..15) from its song SONG (default 1)\n"
        "  --campaign FILE      the campaign data (default: data/campaign.tsv)\n"
        "  --progress FILE      save slot 1's progress file (default: campaign-progress.tsv in the user data\n"
        "                       folder); slots 2 and 3 in slot2/ and slot3/ next to it\n"
        "  --playlists FILE     free play's remembered playlists (default: playlists.tsv in the user data folder)\n"
        "  --no-menu-music      no music on the menu's screens\n"
        "  --menu-shot OUT.png  a screenshot of a menu (--menu-screen title|campaign|free|slots|debrief-over|debrief-clear|\n"
        "                       debrief-songs|debrief-over-songs|unlock, --menu-hover S:SONG, --menu-time SECONDS\n"
        "                       into the screen), then exit (testing)\n"
        "  --menu-clip OUT.mp4  a video walking through the menus (needs ffmpeg), then exit; --menu-demo shows\n"
        "                       made-up campaign progress in shots (clips always do; never saved)\n");
}


class App {
public:
    explicit App(Options o) : opt_(std::move(o)) {}
    int run();

private:
    void startTrack(std::shared_ptr<Track> t);
    void newScene(bool instant);
    void computePhases();
    void writeAnalysis();
    int phaseLevel(int phase) const;
    float phaseEnergy(int phase) const;
    int songUniverse(int* variant);
    void changeUniverse(int index, int variant, transitions::Reason why, bool instant);
    double firstDropTime() const;
    void printScene() const;
    std::string universeSpec() const; // "VECTOR:P3 AMBER": --universe gives it back
    void handleEvents();
    void updateGravity(double songTime, float dt);
    void autoplay(float dt);
    void toggleFullscreen();
    std::vector<HudText> buildHud(float dt);
    void updateBreak(float dt, const std::vector<HudText>& hud);
    // The mouse pointer hides once it has stayed still a few seconds, and comes back as soon as it moves.
    double cursorX_ = -1, cursorY_ = -1;
    float cursorIdle_ = 0;
    bool cursorHidden_ = false;
    // A screen holding still (the pause menu) is drawn a while, then kept as it is: the frames are
    // not drawn again, swap() waits for a key instead (the gamepads read 30 times a second).
    float stillT_ = 0;
    bool held_ = false;
    long statFrames_ = 0; // REFALL_FRAME_STATS
    double statUpd_ = 0, statCpu_ = 0, statGpu_ = 0, statCpuMax_ = 0, statGpuMax_ = 0;
    void hideIdleCursor(float dt) {
        double x, y;
        glfwGetCursorPos(win_, &x, &y);
        if (x != cursorX_ || y != cursorY_) {
            cursorX_ = x;
            cursorY_ = y;
            cursorIdle_ = 0;
            if (cursorHidden_) { glfwSetInputMode(win_, GLFW_CURSOR, GLFW_CURSOR_NORMAL); cursorHidden_ = false; }
        } else if ((cursorIdle_ += dt) > 2.5f && !cursorHidden_) {
            glfwSetInputMode(win_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            cursorHidden_ = true;
        }
    }

    // ---- Modes: the menu, free play, a campaign run.
    enum Mode { MODE_MENU, MODE_FREE, MODE_CAMPAIGN } mode_ = MODE_FREE;
    bool menuEnabled_ = false; // started on the title screen: game overs and Q go back to it
    Campaign campaign_;        // the voyage open: the main campaign, or a custom voyage (and its progress)
    Progress progress_;        // (the save slot's in use)
    SaveSlots slots_;
    void useSlot(int slot);
    VoyageStore voyages_;
    std::string voyage_;       // the custom voyage open ("" the main campaign)
    bool openVoyage(const std::string& id);
    PlaylistStore playlists_;
    Menu menu_;
    float speedMul_ = 1.f;     // gravity multiplier: free play's difficulty, the Voyage's EXPERT
    static constexpr float EXPERT_VOYAGE_SPEED = 1.15f;
    int freeDiff_ = DIFF_HARD; // free play: the difficulty (20G is HARD's and EXPERT's)
    // The campaign run: a stage, played from song `from` to its last one; `current` is the song playing.
    struct Run {
        int stage = 0, from = 0, diff = DIFF_NORMAL, current = -1;
        int songStartScore = 0, songStartLines = 0;
        bool survived = false;  // the current song was survived (recorded)
        bool finishing = false; // the stage's last song was survived: the run ends with its outro
        bool stageRun = false;  // a run of the stage (its game over offers RETRY STAGE)
        bool songPick = false;  // RETRY SONG chosen in this run: the game over's default
        // The song's velocity is not a constant: it climbs from `velFrom` to the song's own over its first `velRamp`
        // seconds (a stage's first song from a warm-up, the next ones from where the previous song ended).
        float velFrom = 1, velTo = 1, velRamp = 0;
        // as it started: the stages open on each difficulty (stage * DIFF_COUNT + difficulty), and whether its stage was
        // done on its difficulty (cleared, or its songs all beaten)
        std::vector<char> openBefore;
        bool doneBefore = false;
    } run_;
    bool muffleWas_ = false;
    // where the menu was: saved next to the progress (debounced while the menu shows, and when leaving it)
    std::string placePath_;
    Menu::Place savedPlace_, lastPlace_;
    float placeChangedAt_ = -1;
    void keepPlace(bool now);
    bool loadPlace(Menu::Place& pl) const;
    void applyOption(const Menu::Choice& c);
    bool remapKeys();
    // The menu's music (data/menu_music.mp3): decoded in the background, a looping bed under the menu's screens, ducked
    // under the song previews, faded out when a game starts and back in on the menu (where it left off).
    std::future<std::shared_ptr<std::vector<int16_t>>> musicLoad_;
    bool musicReady_ = false;
    int musicState_ = 0; // 0 silent, 1 playing, 2 ducked
    static constexpr float MENU_MUSIC_GAIN = 0.7f, MENU_MUSIC_DUCKED = 0.04f;
    void startMenuMusic();
    void updateMenuMusic();
    // A key or button meant to move on (a gameplay or a menu action), on a game over's screen.
    bool anyKeyPressed() const {
        for (Action a : {A_LEFT, A_RIGHT, A_SOFT, A_HARD, A_CW, A_CCW, A_HOLD, A_PAUSE, A_UI_OK, A_UI_BACK})
            if (input_.pressed(a)) return true;
        return false;
    }
    std::string testShot_; // REFALL_TEST_SHOTS: a screenshot of this frame
    void beginSession(const Menu::Choice& c);
    void toMenu(Menu::Screen s, const std::string& msg = "", const std::string& sub = "", bool bad = false);
    void campaignSongStarted();
    void campaignSongSurvived();
    void campaignEnd();
    void runSnapshot(); // (a run's start: what it may open)
    // A run's end (or game over): its stage, its songs now all beaten (in this run or over several), counts as cleared
    // (`score` and `lines` its record, kept if better); the stages it opened get their unlocks on the wheel. `fresh`:
    // the stage is done for the first time on this difficulty (its clear is celebrated), `nextOpened`: the next stage
    // just opened on it, `expertOpened`: the stage itself on EXPERT (done on HARD).
    struct RunOutcome { bool fresh = false, nextOpened = false, expertOpened = false; };
    RunOutcome campaignOutcome(int score, int lines);
    // A Voyage game over: once the board has dissolved, the frame is kept and the debrief comes over it.
    bool debriefPending_ = false;
    void gameOverDebrief();
    void toDebrief(const Menu::Debrief& d);
    int runMenuShot();
    // The game's loop, a frame at a time (frame(), finish() once it is over), and what lives across its frames
    struct Loop {
        bool clipMode = false, shotMode = false, reviewMode = false, fixedStep = false;
        double last = 0;
        float fade = 0.f;
        int shotsTaken = 0;
        float shotSettle = 0;
        FILE* clipPipe = nullptr;
        std::vector<unsigned char> clipPx;
    } loop_;
    bool frame();
    int finish();
    const CampaignStage* runStage() const {
        return mode_ == MODE_CAMPAIGN && run_.stage < campaign_.size() ? &campaign_.stages()[run_.stage] : nullptr;
    }

    // The break between songs: a little before a song ends the game freezes and fades away with its HUD, the
    // world stays alone a moment, then fades to black; the next song starts on the black with its title and
    // universe lettered big in the middle, which then shrink into their corners as the new world fades in; the
    // game comes back last, its falling piece back at the top. REFALL_NO_BREAK=1 goes back to the transitions.
    enum BreakStage { BREAK_OFF, BREAK_OUT, BREAK_IN } break_ = BREAK_OFF;
    float breakT_ = 0;         // seconds into the stage
    bool breakSkip_ = false;   // an outro asked by "next song": a shorter one
    bool breaks_ = true;
    bool introAtStart_ = true; // the program starts on the black, the first song coming in with its title card
    // A song without an intro waits silent through its title card and starts with the game (see startTrack).
    bool songHeld_ = false;
    bool songHasIntro() const;
    Game blank_;               // what the universes draw while the game is hidden
    // the outro starts this long before the song's end (REFALL_OUT_LEAD=SECONDS: earlier or later, for demo videos)
    // The outro starts on a piece landing, so the game never fades out mid-fall: from OUT_EARLY s before outLead() to
    // OUT_LATE s after it the first lock starts it, and at the window's end it starts anyway.
    // A lock clearing lines waits for the stack to settle (up to OUT_SETTLE s more at the window's end).
    static constexpr float OUT_EARLY = 2.5f, OUT_LATE = 2.f, OUT_SETTLE = 1.5f;
    bool landed_ = false; // a piece locked since the last check (or since its clear, until the stack settled)
    static float outLead() {
        static const float lead = std::getenv("REFALL_OUT_LEAD") ? (float)std::atof(std::getenv("REFALL_OUT_LEAD")) : 7.5f;
        return lead;
    }
    // A Voyage run's first song starts at this share of its velocity, reached after WARMUP_SECONDS; the next songs
    // climb from the previous one's over NEXT_SONG_RAMP_SECONDS.
    static constexpr float WARMUP_VELOCITY = 0.35f, WARMUP_SECONDS = 30.f, NEXT_SONG_RAMP_SECONDS = 20.f;
    // EXPERT's warm-ups are short: little time to settle in.
    static constexpr float WARMUP_VELOCITY_EXPERT = 0.7f, WARMUP_SECONDS_EXPERT = 10.f, NEXT_SONG_RAMP_SECONDS_EXPERT = 8.f;
    // Free play: every song starts at this share of the level's velocity and climbs to it over its first seconds.
    static constexpr float FREE_WARMUP_VELOCITY = 0.5f, FREE_WARMUP_SECONDS = 20.f;
    static constexpr float FREE_WARMUP_VELOCITY_EXPERT = 0.8f, FREE_WARMUP_SECONDS_EXPERT = 8.f;
    // A song's section pace is at most x1 over its first seconds (EXPERT: fewer).
    static constexpr float CALM_START_SECONDS = 20.f, CALM_START_SECONDS_EXPERT = 8.f;
    static constexpr float OUT_HIDE = 1.6f;  // the game fades away
    static constexpr float OUT_DARK = 2.0f;  // the world fades to black, ending with the song
    static constexpr float IN_CARD = 0.4f, IN_HOLD = 1.4f, IN_SHRINK = 3.2f, IN_WORLD = 5.2f, IN_SHOW = 6.8f, IN_END = 8.4f;
    bool gameFrozen() const { return break_ != BREAK_OFF; }

    Options opt_;
    GLFWwindow* win_ = nullptr;
    // offline renders: an EGL context on the GPU, no window (win_ stays null)
    bool headless_ = false;
    int headW_ = 0, headH_ = 0;
    void fbSize(int& w, int& h) {
        if (headless_) { w = headW_; h = headH_; }
        else glfwGetFramebufferSize(win_, &w, &h);
    }
    // --stream: the frames' and the sound's pipes, and the clock pacing the frames (headless, no vsync)
    int streamVideo_ = -1, streamAudio_ = -1;
    std::vector<unsigned char> streamPx_;
    std::chrono::steady_clock::time_point streamNext_{};
    double clock() const {
        if (win_) return glfwGetTime();
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void streamFrame();
    void startStream();
    void swap() {
        if (streamVideo_ >= 0) streamFrame();
        if (headless_) return;
#ifdef REFALL_WEB
        web::endFrame(); // (the browser shows the canvas once frame() returns, and calls it again at its next frame)
        return;
#endif
        if (held_) { glfwWaitEventsTimeout(1.0 / 30.0); return; }
        glfwSwapBuffers(win_);
        // Wait for the frame to be shown: otherwise the driver queues up to a few frames ahead under vsync (more
        // through PRIME offload), and every key press waits behind them. REFALL_FRAME_QUEUE=1 lets it queue.
        static const bool queue = std::getenv("REFALL_FRAME_QUEUE") != nullptr;
        if (!queue) glFinish();
    }
    AudioEngine audio_;
    Library lib_;
    Game game_;
    Input input_;
    Renderer R_;
    uint64_t runSeed_ = 0;
    double clipStart_ = -1; // song time where the --clip recording starts
    uint64_t sceneCounter_ = 0;
    std::shared_ptr<Track> track_, nextTrack_;
    SongLook look_;              // the song's inputs to universes
    int forcedUniverse_ = -1, forcedVariant_ = -1;
    float universeAge_ = 0;      // seconds in the current universe
    std::vector<int> recentUniverses_; // the last universes played, oldest first (left out of the next picks)
    // Song phases: detected sections, with long sections split into sub-phases so the scene keeps evolving.
    SongPlan plan_;
    int lastPhase_ = 0;
    double dropAt_ = -1; // song time of the next drop hit (the first downbeat of a peak), -1 none
    float skipToast_ = 0;
    float hit_ = 0;
    float kickLevel_ = 0; // how much kick the music has right now (slow: over a few beats)
    int lastSeg_ = 0;          // for soft refreshes between same-level sections
    float seekToast_ = 0;
    std::string failedTitle_;
    float failedToast_ = 0;
    float overTime_ = 0; // seconds since game over
    int lastLevel_ = 1;
    float levelToast_ = 0;
    // Slow-motion bonus (gravity at x0.25 while it lasts) and quad rewards.
    bool slowWas_ = false;
    float slowToast_ = 0, quadToast_ = 0;
    int quadPoints_ = 0;
    bool quadB2B_ = false;
    bool paused_ = false, wantSkip_ = false, pauseControls_ = false; // (the pause menu's controls page)
    std::atomic<bool> quit_{false}; // (also set by --stream's input thread)
    int pauseSel_ = 0; // the pause menu's choice: 0 continue, 1 controls, 2 quit (to the menu)
    int ctrlSel_ = 2;  // the controls page's: 0 remap the gamepad, 1 its default back, 2 back
    std::string ctrlNote_; // what the controls page just did ("GAMEPAD REMAPPED"), for a few seconds
    float ctrlNoteT_ = 0;
    double songTime_ = 0, simSongTime_ = 0;
    int fallSteps_ = 0; // gravity steps taken this frame (the REFALL_DEBUG_FALL log)
    bool offline_ = false; // shots and clips: the song runs on simSongTime_, not on the audio's clock
    // The song reached its end: by the audio's clock when playing, by the simulated song time offline (a slow
    // offline render must not run past the song's real length and start the next one).
    bool songFinished() const {
        return offline_ ? (!track_ || simSongTime_ >= track_->duration()) : audio_.finished();
    }
    float trackAge_ = 0, sceneNameTimer_ = 0, runTime_ = 0;
    // Gravity bookkeeping.
    // Slow-motion bonus: nothing falls, whatever the velocity, while the music keeps its speed (only a short tape bend
    // marks the start and the end).
    static constexpr float BONUS_GAME_SPEED = 0.25f;
    static constexpr float TRANSITION_GAME_SPEED = 0.25f; // gravity during a change of universe
    double rowAcc_ = 0, lastGravityTime_ = -1; // gravity rows accumulated, song time at the last update
    static constexpr float GRAVITY_BPM = 130.f; // the tempo gravity counts its beats in, whatever the song's
    float gameRate_ = 1.f; // gravity speed in real time (eased)
    float gravityV_ = 1.f; // the velocity gravity runs at (the autoplay hurries as it rises)
    bool hyperAble_ = false; // hyperdrop (20G) can happen in this game (HARD, EXPERT)
    float hyperFrom_ = Game::TWENTY_G; // the velocity it can happen from (Game::hyperdropFrom)
    float speedPos_ = -1.f, speedHot_ = 0.f; // the speed gauge as shown (eased): its fill, hyperdrop on
    bool preroll_ = false; // a clip played from the song's start: the game alone until its warm-up (nothing drawn)
    float bonusFall_ = 0.f; // how much the bonus's stillness has taken over the fall (eased)
    float pace_ = -1, fallbackTimer_ = 0; // section pace (rows per beat before level scaling), eased
    // Autoplay.
    bool planned_ = false;
    int planRot_ = 0, planX_ = 0;
    float botTimer_ = 0;
    // Windowed geometry for fullscreen toggling.
    int winX_ = 100, winY_ = 100, winW_ = 1600, winH_ = 900;
    MusicState music_;
    // --review-shot.
    int reviewPhase_ = -1;
    bool reviewFilled_ = false;
    void pickReviewTime();
    void printReview() const;
    // ---- refall-voyage-viewer: the campaign's songs in a row (VoyageViewer's indices), each at its velocity in its
    // universe, the computer playing; the overlay jumps from song to song and seeks.
    bool viewer_ = false;
    std::unique_ptr<VoyageViewer> vv_;
    int viewCur_ = -1, viewWant_ = -1; // the song playing, the one jumped to (still loading)
    float viewShotT_ = 0;
    VoyageViewer::Now viewerNow() const;
    bool viewerStart();
    void viewerFrame(float dt);
    void viewerJump(int i);
    void viewerSeek(double t);
    void seekSong(double t);
    void viewerSongStarted();
};

void App::startTrack(std::shared_ptr<Track> t) {
    bool first = !track_;
    track_ = std::move(t);
    if (mode_ == MODE_CAMPAIGN) campaignSongStarted();
    if (viewer_) viewerSongStarted();
    // free play without the menu: a song that ran out over a game over brings a new game (the next one never opens on
    // the last one's full well)
    if (mode_ == MODE_FREE && !menuEnabled_ && !viewer_ && game_.over()) {
        game_.restart();
        overTime_ = 0;
        std::printf("[app] game over: a new game with the new song\n");
    }
    // Coming in from the black with its title card: a song that has an intro plays from the title on; one that
    // starts straight in waits until the game is back and starts with it, on its very first beat.
    songHeld_ = break_ == BREAK_IN && !songHasIntro();
    audio_.play(track_, !songHeld_);
    audio_.setPaused(paused_ || songHeld_);
    if (songHeld_) std::printf("[app] no intro: the song waits for the game\n");
    trackAge_ = 0;
    lastPhase_ = 0;
    lastSeg_ = 0;
    pace_ = -1;
    lastGravityTime_ = -1;
    simSongTime_ = 0;
    if (testPlay() && std::getenv("REFALL_TEST_TAIL"))
        simSongTime_ = std::max(0.0, track_->duration() - std::atof(std::getenv("REFALL_TEST_TAIL")));
    std::printf("[app] now playing: %s (%s)\n", track_->title.c_str(), track_->path.c_str());
    sceneCounter_++;
    computePhases();
    lastPhase_ = first ? plan_.phaseAt(0.0) : 0;
    look_ = makeSongLook(&track_->analysis.fp, runSeed_ ^ splitmix64(sceneCounter_));
    look_.difficulty = mode_ == MODE_CAMPAIGN ? run_.diff : freeDiff_;
    songTime_ = 0;
    if (track_->duration() > 0) look_.duration = look_.remaining = (float)track_->duration();
    {
        const int sec = (int)look_.duration;
        std::printf("[app] song %d:%02d: a progression scene completes in %.0f lines (%.0f lines/min on %s)\n", sec / 60,
                    sec % 60, sceneLinesFor(look_), linesPerMinute(look_.difficulty), difficultyName(look_.difficulty));
    }
    // Each song gets its universe (picked from its footprint, never the same twice in a row), swept in with a
    // transition on the beat; a forced universe stays and performs its own change of song, and so does the
    // universe shown while the first song loaded (the game doesn't jump to another world as the music starts).
    int variant = -1;
    const bool offline = !opt_.shotPrefix.empty() || !opt_.clipPath.empty();
    const bool keep = first && !offline && opt_.reviewPath.empty() && !viewer_;
    const int u = keep ? R_.universe() : songUniverse(&variant);
    if (keep) variant = forcedVariant_ >= 0 ? forcedVariant_ : R_.variant();
    if ((forcedUniverse_ >= 0 || keep) && u == R_.universe() && !offline) {
        look_.variant = variant;
        R_.setSong(look_, 1.5f);
        R_.setLevel(phaseLevel(lastPhase_), phaseEnergy(lastPhase_), 1.5f, 0.f);
    } else {
        changeUniverse(u, variant, transitions::SONG_CHANGE, offline || break_ != BREAK_OFF);
    }
    sceneNameTimer_ = 8.f;
    printScene();
    lib_.prefetch(runSeed_ + sceneCounter_ * 7919);
}

// A calm intro: the song's first 6 seconds are much quieter (8 dB and more) than its usual level (the median of its
// 2 s windows), or much sparser in attacks (its onsets, under a third of its usual: a slow start of pads and long notes,
// AVAION's COLORS): before the beat comes in. An opening with its beat already going is within a few dB and as busy.
bool App::songHasIntro() const {
    const std::vector<float>& pcm = track_->pcm;
    const size_t win = (size_t)track_->sampleRate * 2 * 2; // 2 s, interleaved stereo
    if (pcm.size() < win * 8) return false;
    std::vector<double> rms;
    for (size_t at = 0; at + win <= pcm.size(); at += win) {
        double e = 0;
        for (size_t k = at; k < at + win; k++) e += (double)pcm[k] * pcm[k];
        rms.push_back(e / win);
    }
    const double opening = (rms[0] + rms[1] + rms[2]) / 3.0;
    std::vector<double> sorted = rms;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const double usual = sorted[sorted.size() / 2];
    const double db = 10.0 * std::log10((opening + 1e-12) / (usual + 1e-12));
    // the attacks: the opening's mean onset against the median of the song's 8 s windows from 30 s on
    const Analysis& an = track_->analysis;
    double sparse = 1.0;
    if (!an.frames.empty() && an.hop > 0) {
        const int per = std::max(1, (int)std::lround(8.0 / an.hop)), n6 = (int)std::lround(6.0 / an.hop);
        auto mean = [&](int f0, int f1) {
            double s = 0;
            int c = 0;
            for (int f = std::max(0, f0); f < std::min(f1, (int)an.frames.size()); f++, c++) s += an.frames[f].onset;
            return c ? s / c : 0.0;
        };
        std::vector<double> wins;
        for (int f = (int)std::lround(30.0 / an.hop); f + per < (int)an.frames.size() - (int)std::lround(30.0 / an.hop); f += per)
            wins.push_back(mean(f, f + per));
        if (!wins.empty()) {
            std::nth_element(wins.begin(), wins.begin() + wins.size() / 2, wins.end());
            const double body = wins[wins.size() / 2];
            if (body > 1e-6) sparse = mean(0, n6) / body;
        }
    }
    const bool intro = db <= -8.0 || sparse <= 0.3;
    std::printf("[app] opening %.1f dB from the song's usual level, %.0f%% of its attacks: %s\n", db, 100.0 * sparse,
                intro ? "a calm intro" : "no intro");
    return intro;
}

// The universe for the current song: the forced one, else one picked from the song's footprint.
int App::songUniverse(int* variant) {
    *variant = forcedVariant_;
    if (forcedUniverse_ >= 0) return forcedUniverse_;
    return universes::pick(look_, look_.seed, R_.universe(), recentUniverses_);
}

void App::changeUniverse(int index, int variant, transitions::Reason why, bool instant) {
    SongLook l = look_;
    l.variant = variant;
    if (track_) l.remaining = (float)std::max(0.0, track_->duration() - songTime_); // mid-song: the time left
    const int lv = phaseLevel(lastPhase_);
    const float en = phaseEnergy(lastPhase_);
    universeAge_ = 0;
    recentUniverses_.erase(std::remove(recentUniverses_.begin(), recentUniverses_.end(), index), recentUniverses_.end());
    recentUniverses_.push_back(index);
    if ((int)recentUniverses_.size() > universes::RECENT_UNIVERSES)
        recentUniverses_.erase(recentUniverses_.begin());
    if (instant) {
        R_.setUniverse(index, l, lv, en);
        return;
    }
    Renderer::Change c;
    c.why = why;
    R_.changeUniverse(index, l, lv, en, c);
}

// T / Y: the next universe, with a transition. With --universe the universe stays locked: T moves on to its next
// variant (or, with a forced variant, just a new seed) so its configurations can be tested one after the other.
void App::newScene(bool instant) {
    sceneCounter_++;
    look_.seed = splitmix64(look_.seed ^ sceneCounter_);
    if (forcedUniverse_ >= 0) {
        int variant = forcedVariant_;
        if (variant < 0) {
            const int n = std::max<int>(1, (int)universes::entry(forcedUniverse_).variants.size());
            variant = (R_.variant() + 1) % n;
        }
        changeUniverse(forcedUniverse_, variant, transitions::NEW_SCENE, instant);
    } else {
        changeUniverse(universes::next(R_.universe()), -1, transitions::NEW_SCENE, instant);
    }
    sceneNameTimer_ = 6.f;
    printScene();
}

std::string App::universeSpec() const {
    const UniverseEntry& e = universes::entry(R_.universe());
    const int v = std::clamp(R_.variant(), 0, std::max(0, (int)e.variants.size() - 1));
    return std::string(e.name) + (e.variants.empty() ? "" : ":" + e.variants[v]);
}

// Prints the universe and the command line that shows it again (same universe and variant, same song).
void App::printScene() const {
    const UniverseEntry& e = universes::entry(R_.universe());
    std::printf("[app] universe: %s (%s)\n[app] same universe: refall --universe %s", R_.label().c_str(), e.blurb,
                platform::quoteArg(universeSpec()).c_str());
    if (track_) std::printf(" %s", platform::quoteArg(track_->path).c_str());
    std::printf("\n");
}

// Start of the first drop (else of the first peak scene level, else a third into the song).
double App::firstDropTime() const {
    for (const Segment& sg : track_->analysis.segments)
        if (sg.kind == SEG_DROP) return sg.start;
    for (const ScenePhase& ph : plan_.phases)
        if (ph.level == 2) return ph.start;
    return track_->duration() * 0.3;
}

// --review-shot: a phase of the wanted level (any if the song has none), shot once its scene has settled.
void App::pickReviewTime() {
    Rng r(runSeed_ ^ 0x5EE5ull);
    std::vector<int> cand;
    for (int k = 0; k < (int)plan_.phases.size(); k++)
        if (opt_.reviewLevel < 0 || plan_.phases[k].level == opt_.reviewLevel) cand.push_back(k);
    if (cand.empty())
        for (int k = 0; k < (int)plan_.phases.size(); k++) cand.push_back(k);
    reviewPhase_ = cand.empty() ? 0 : cand[r.next() % cand.size()];
    if (plan_.phases.empty()) {
        opt_.shotTime = (float)std::min(40.0, track_->duration() * 0.5);
        return;
    }
    const ScenePhase& ph = plan_.phases[reviewPhase_];
    const double end = reviewPhase_ + 1 < (int)plan_.phases.size() ? plan_.phases[reviewPhase_ + 1].start
                                                                   : track_->duration();
    opt_.shotTime = (float)std::max(ph.start + 1.0, std::min(ph.start + 10.0 + r.uniform() * 20.0, end - 0.5));
}

static std::string jsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}

void App::printReview() const {
    const double t = songTime_;
    const ScenePhase* ph = plan_.phases.empty() ? nullptr : &plan_.phases[plan_.phaseAt(t)];
    const Analysis& an = track_->analysis;
    const char* section = an.segments.empty() ? "?" : segmentName(an.segments[an.segmentAt(t)].kind);
    std::printf("[review] {\"file\": %s, \"code\": %s, \"scene\": %s, \"song\": %s, \"title\": %s, \"time\": %.1f, "
                "\"duration\": %.1f, \"level\": %d, \"phase\": %d, \"phases\": %d, \"energy\": %.2f, \"section\": %s, "
                "\"bpm\": %.0f, \"levels\": %s, \"lines\": %d, \"scene_lines\": %.1f}\n",
                jsonStr(opt_.reviewPath).c_str(), jsonStr(universeSpec()).c_str(), jsonStr(R_.label()).c_str(),
                jsonStr(track_->path).c_str(), jsonStr(track_->title).c_str(), t, track_->duration(), ph ? ph->level : 1,
                ph ? plan_.phaseAt(t) : 0, (int)plan_.phases.size(), ph ? ph->energy : 0.f, jsonStr(section).c_str(),
                an.fp.bpm, jsonStr([&] {
                    std::string l;
                    for (const ScenePhase& p : plan_.phases) {
                        char b[32];
                        std::snprintf(b, sizeof(b), "%sL%d@%.0f", l.empty() ? "" : " ", p.level, p.start);
                        l += b;
                    }
                    return l;
                }()).c_str(), game_.lines(), sceneLinesFor(look_));
}

void App::computePhases() {
    plan_ = planSong(track_->analysis);
    const Analysis& an = track_->analysis;
    std::printf("[app] structure:");
    for (const Segment& sg : an.segments) std::printf(" %s@%.0f", segmentName(sg.kind), sg.start);
    std::printf("\n[app] scene levels:");
    for (const ScenePhase& p : plan_.phases) std::printf(" L%d@%.0f", p.level, p.start);
    std::printf("\n[app] %zu scene changes, %zu pulse zones\n", plan_.phases.size() - 1, plan_.pulseZones.size());
    if (!opt_.analysisPath.empty()) writeAnalysis();
}

// --analysis: the song as the game hears it, for tools that cut on its beats (tools/demo.py); then exit.
void App::writeAnalysis() {
    const Analysis& an = track_->analysis;
    FILE* f = std::fopen(opt_.analysisPath.c_str(), "w");
    if (!f) { std::fprintf(stderr, "[analysis] could not write %s\n", opt_.analysisPath.c_str()); quit_ = true; return; }
    std::fprintf(f, "{\"path\": \"%s\", \"duration\": %.4f, \"bpm\": %.3f, \"downbeat\": %d, \"first_drop\": %.4f,\n",
                 track_->path.c_str(), track_->duration(), an.fp.bpm, an.downbeat, firstDropTime());
    std::fprintf(f, " \"beats\": [");
    for (size_t k = 0; k < an.beats.size(); k++) std::fprintf(f, "%s%.4f", k ? ", " : "", an.beats[k]);
    std::fprintf(f, "],\n \"segments\": [");
    for (size_t k = 0; k < an.segments.size(); k++) {
        const Segment& sg = an.segments[k];
        std::fprintf(f, "%s{\"kind\": \"%s\", \"start\": %.4f, \"end\": %.4f, \"energy\": %.3f}", k ? ", " : "",
                     segmentName(sg.kind), sg.start, sg.end, sg.energy);
    }
    std::fprintf(f, "],\n \"phases\": [");
    for (size_t k = 0; k < plan_.phases.size(); k++) {
        const ScenePhase& p = plan_.phases[k];
        std::fprintf(f, "%s{\"level\": %d, \"start\": %.4f, \"end\": %.4f}", k ? ", " : "", p.level, p.start, p.end);
    }
    std::fprintf(f, "],\n \"loudness\": {\"hop\": %.4f, \"values\": [", 0.1);
    for (int k = 0; k * 0.1 < track_->duration(); k++) std::fprintf(f, "%s%.3f", k ? ", " : "", an.intensityAt(k * 0.1));
    std::fprintf(f, "]}}\n");
    std::fclose(f);
    std::printf("[analysis] wrote %s\n", opt_.analysisPath.c_str());
    quit_ = true;
}

int App::phaseLevel(int phase) const {
    if (!track_ || plan_.phases.empty()) return 0;
    return plan_.phases[std::clamp(phase, 0, (int)plan_.phases.size() - 1)].level;
}

float App::phaseEnergy(int phase) const {
    if (!track_ || plan_.phases.empty()) return 0.3f;
    return plan_.phases[std::clamp(phase, 0, (int)plan_.phases.size() - 1)].energy;
}

void App::toggleFullscreen() {
#ifdef REFALL_WEB
    web::toggleFullscreen(win_);
    return;
#endif
    GLFWmonitor* mon = glfwGetWindowMonitor(win_);
    if (mon) {
        glfwSetWindowMonitor(win_, nullptr, winX_, winY_, winW_, winH_, 0);
    } else {
        glfwGetWindowPos(win_, &winX_, &winY_);
        glfwGetWindowSize(win_, &winW_, &winH_);
        // the monitor the window is on: the one it overlaps most (the primary one if it overlaps none)
        GLFWmonitor* m = glfwGetPrimaryMonitor();
        int count = 0, best = 0;
        GLFWmonitor** mons = glfwGetMonitors(&count);
        for (int i = 0; i < count; i++) {
            const GLFWvidmode* mode = glfwGetVideoMode(mons[i]);
            if (!mode) continue;
            int mx = 0, my = 0;
            glfwGetMonitorPos(mons[i], &mx, &my);
            const int ox = std::min(winX_ + winW_, mx + mode->width) - std::max(winX_, mx);
            const int oy = std::min(winY_ + winH_, my + mode->height) - std::max(winY_, my);
            if (ox > 0 && oy > 0 && ox * oy > best) { best = ox * oy; m = mons[i]; }
        }
        const GLFWvidmode* vm = glfwGetVideoMode(m);
        glfwSetWindowMonitor(win_, m, 0, 0, vm->width, vm->height, vm->refreshRate);
    }
}

// Game events only drive visuals: the song is the only thing you hear.
void App::handleEvents() {
    for (const GameEvent& ev : game_.events()) {
        if (!preroll_) R_.onEvent(ev, game_);
        // --clip: the line clears with their song time (tools/demo.py keeps the shots that show one)
        // (REFALL_CLIP_ALL_CLEARS=1: all of them, from the song's start: tools/demo.py's seed search for a continuous game)
        static const bool allClears = std::getenv("REFALL_CLIP_ALL_CLEARS") != nullptr;
        if (ev.type == GameEvent::Clear && !opt_.clipPath.empty() && clipStart_ >= 0 && (allClears || songTime_ >= clipStart_ - 2.0))
            std::printf("[clip] clear %d at %.3f\n", ev.count, songTime_);
        if (ev.type == GameEvent::Clear && ev.count >= 4) {
            quadToast_ = 2.4f;
            quadPoints_ = ev.points;
            quadB2B_ = ev.backToBack;
        }
        if (ev.type == GameEvent::BonusStart) slowToast_ = 2.f;
        if (ev.type == GameEvent::Lock) landed_ = true;
        if (ev.type == GameEvent::Spawn) planned_ = false; // (a clip's autoplay lets its piece land: plan the next)
    }
    game_.events().clear();
}

// Gravity: the song section sets the pace in rows per beat (x0.4 in a break .. x1.3 in a drop), beats of a fixed
// GRAVITY_BPM whatever the song's tempo, and the velocity scales it up gradually: x10 at 20, x19.5 at 40 (capped
// at 60 rows/s).
void App::updateGravity(double songTime, float dt) {
    game_.setFallGlide(0.f, 0.f); // (set below when the song's gravity runs)
    // A Voyage song's velocity climbs to its own over its first seconds (see Run).
    if (mode_ == MODE_CAMPAIGN && game_.fixedLevel() > 0.f && run_.velRamp > 0.f) {
        const float k = smoothstepf(0.f, run_.velRamp, (float)songTime);
        game_.setFixedLevel(run_.velFrom + (run_.velTo - run_.velFrom) * k);
    }
    // Gravity slows right down during a change of universe (the stack can be hard to read while two worlds
    // blend) and the slow-motion bonus (20G lets go meanwhile); it eases back over about a second.
    const float rateGoal = std::min(game_.bonusActive() ? BONUS_GAME_SPEED : 1.f, R_.changeProgress() >= 0.f ? TRANSITION_GAME_SPEED : 1.f);
    gameRate_ = approach(gameRate_, rateGoal, 3.f, dt);
    if (gameRate_ > 0.999f) gameRate_ = 1.f;
    bonusFall_ = game_.bonusActive() ? 1.f : approach(bonusFall_, 0.f, 3.f, dt); // (still at once, the fall eased back)
    // Free play (levels rising with the lines): each song warms up, from a share of the level's velocity to all of it.
    const bool freeLevels = mode_ == MODE_FREE && game_.fixedLevel() <= 0.f && !viewer_;
    float v = game_.velocity();
    const bool expert = game_.runDifficulty() == DIFF_EXPERT;
    if (freeLevels)
        v = std::max(1.f, v * lerpf(expert ? FREE_WARMUP_VELOCITY_EXPERT : FREE_WARMUP_VELOCITY, 1.f,
                                    smoothstepf(0.f, expert ? FREE_WARMUP_SECONDS_EXPERT : FREE_WARMUP_SECONDS, (float)songTime)));
    // Test hook: REFALL_CLIP_VELOCITY=V falls at velocity V (tools/demo.py ramps it up), with no 20G unless REFALL_CLIP_20G=1
    const float clipVel = clipRamp("REFALL_CLIP_VELOCITY", songTime, 0.f);
    if (clipVel > 0.f) v = std::clamp(clipVel, 1.f, Game::MAX_VELOCITY);
    gravityV_ = v;
    const float diff = Game::speedDifficulty(v);
    // Hyperdrop (20G, HARD and EXPERT only): from velocity TWENTY_G on (EXPERT: TWENTY_G_EXPERT), in a drop (and at the
    // top velocity, in a chorus too), a piece falls to the stack the moment it appears; the rest of the song keeps a
    // visible fall. The last song of HARD's later stages has some, every song of the finale; free play has it from
    // level 30 on (EXPERT 24, once the song has warmed up).
    const float pace = track_ && !track_->analysis.segments.empty() ? gravityPace(track_->analysis, songTime, expert) : 0.f;
    static const bool clip20G = std::getenv("REFALL_CLIP_20G") && std::getenv("REFALL_CLIP_20G")[0] == '1';
    const float from = clipVel > 0.f ? (clip20G ? Game::TWENTY_G : -1.f)
                     : mode_ == MODE_CAMPAIGN ? Game::hyperdropFrom(run_.diff)
                     : freeLevels ? Game::hyperdropFrom(freeDiff_) : -1.f;
    const bool hard = from > 0.f;
    hyperAble_ = hard;
    if (hard) hyperFrom_ = from;
    const bool twentyG = hard && gameRate_ > 0.999f &&
                         v >= from - 0.05f && (pace >= 1.29f || (pace >= 1.19f && v >= Game::MAX_VELOCITY - 0.05f));
    if (twentyG != game_.twentyG()) {
        game_.setTwentyG(twentyG);
        std::printf("[game] hyperdrop %s at %.1f s\n", twentyG ? "on" : "off", songTime);
    }
    if (twentyG) {
        for (int i = 0; i < Game::H; i++) game_.gravityStep();
        lastGravityTime_ = -1;
        return;
    }
    if (!track_ || songFinished()) {
        fallbackTimer_ += dt * gameRate_;
        const float every = 0.9f * (1.f - 0.45f * diff) / speedMul_ / std::max(1e-3f, 1.f - bonusFall_);
        if (fallbackTimer_ > every) { fallbackTimer_ = 0; game_.gravityStep(); }
        return;
    }
    const Analysis& an = track_->analysis;
    // Section pace, in rows per beat (gravityPace: one constant per section, x0.4 in a break .. x1.3 in a
    // drop; EXPERT has no slower part than x1 but the verses' x0.8). Eased over ~0.25 s; at most x1 in the first 20 s of a song
    // (8 s on EXPERT).
    float want = gravityPace(an, songTime + 0.25, expert);
    if (trackAge_ < (expert ? CALM_START_SECONDS_EXPERT : CALM_START_SECONDS)) want = std::min(want, 1.f);
    pace_ = pace_ < 0 ? want : approach(pace_, want, 4.f, dt);
    // Velocity scaling: x10 at 20, x19.5 at 40 (the top velocity), capped at 60 rows per second.
    const float MAX_ROWS_PER_SEC = 60.f;
    // The fall doesn't follow the song's tempo: a beat is always that of a 130 BPM song (the same velocity falls as
    // fast in every song).
    const float beatsPerSec = GRAVITY_BPM / 60.f;
    // Free play's difficulty scales every level's speed (HARD x1); a campaign song's level is set by the data (EXPERT x1.15).
    const float rowsPerBeat = std::min(pace_ * (1.f + 9.f * diff) * speedMul_, MAX_ROWS_PER_SEC / beatsPerSec);
    // Rows accumulate with the song's time (scaled during the bonus so gravity runs at gameRate_ in real time).
    double dBeat = 0;
    if (lastGravityTime_ < 0 || songTime < lastGravityTime_ || songTime - lastGravityTime_ > 4.0) rowAcc_ = 0; // start, seek, new song
    else dBeat = (songTime - lastGravityTime_) * beatsPerSec * std::min(1.f, gameRate_ / std::max(0.05f, audio_.currentSpeed()));
    lastGravityTime_ = songTime;
    // (the bonus: no fall at all, easing back after it)
    rowAcc_ += (float)(dBeat * rowsPerBeat) * (1.f - bonusFall_);
    const int steps = (int)std::min(4.0, std::floor(rowAcc_));
    for (int i = 0; i < steps; i++) game_.gravityStep();
    fallSteps_ = steps;
    rowAcc_ = std::min(rowAcc_ - steps, 1.0); // a long frame doesn't pile up steps
    // (the drawn piece glides at this speed: rows per second of real time)
    game_.setFallGlide((float)rowAcc_, rowsPerBeat * beatsPerSec * std::min(audio_.currentSpeed(), gameRate_) * (1.f - bonusFall_));
}

void App::autoplay(float dt) {
    // Test hook: REFALL_TEST_TOPOUT=1 drops every piece at once (a quick game over, to check its animation).
    static const bool topOut = std::getenv("REFALL_TEST_TOPOUT") != nullptr;
    if (topOut) {
        botTimer_ -= dt;
        if (game_.hasPiece() && botTimer_ <= 0) { game_.hardDrop(); botTimer_ = 0.15f; }
        return;
    }
    static const bool noBonus = std::getenv("REFALL_TEST_BONUS") && std::getenv("REFALL_TEST_BONUS")[0] == '0';
    if (game_.bonusReady() && !viewer_ && !noBonus) game_.activateBonus(); // (the viewer keeps the song unmuffled)
    if (!game_.hasPiece()) { planned_ = false; return; }
    // Recording a clip: a calm, human pace (a moment to think, unhurried moves, the piece falls visibly).
    // Test hook: REFALL_CLIP_PACE=K plays a clip K times faster (tools/demo.py speeds the trailer up toward its end).
    const bool calm = !opt_.clipPath.empty();
    const float pace = std::max(0.2f, clipRamp("REFALL_CLIP_PACE", songTime_, 1.f));
    // (and hurries as the fall gets faster: no time to think once a piece reaches the stack in a second or two)
    const float rush = smoothstepf(4.f, 22.f, gravityV_);
    const float k = (calm ? 1.f / pace : 1.f) * (1.f - 0.85f * rush);
    if (!planned_) {
        planned_ = game_.planMove(planRot_, planX_);
        botTimer_ = (calm ? 0.45f : 0.25f) * k;
        if (!planned_) return;
    }
    // 20G: the piece is on the stack the moment it appears, so it is turned and slid into place at once (an expert's
    // reflexes), before the lock delay runs out
    if (game_.twentyG()) {
        for (int i = 0; i < 12; i++) {
            const Piece& p = game_.piece();
            bool ok = true;
            if (p.rot != planRot_) ok = game_.rotate(1);
            else if (p.x < planX_) ok = game_.move(1);
            else if (p.x > planX_) ok = game_.move(-1);
            else break;
            if (!ok) break;
        }
        game_.hardDrop();
        planned_ = false;
        return;
    }
    botTimer_ -= dt;
    if (botTimer_ > 0) return;
    botTimer_ = (calm ? 0.13f : 0.07f) * k;
    const Piece& p = game_.piece();
    if (p.rot != planRot_) { if (!game_.rotate(1)) planned_ = false; return; }
    if (p.x < planX_) { if (!game_.move(1)) planned_ = false; return; }
    if (p.x > planX_) { if (!game_.move(-1)) planned_ = false; return; }
    // a clip: in place, the piece falls at the gravity's own speed (what the velocity shows), no drop by the player;
    // while the fall is slow, some pieces are hard dropped (more line clears to see)
    if (calm) {
        const float hard = 1.f - smoothstepf(10.f, 20.f, gravityV_);
        if (hard <= 0.f || std::fmod((float)game_.lines() * 0.37f + game_.piece().x * 0.61f + game_.piece().rot * 0.29f, 1.f) >= hard) return;
    }
    // (REFALL_DEBUG_FALL: the pieces fall all the way at the gravity's speed, to watch the fall)
    static const bool watchFall = std::getenv("REFALL_DEBUG_FALL") != nullptr;
    if (watchFall) return;
    game_.hardDrop();
    planned_ = false;
}

// The break's frame: how much of the game shows, the black, the bare HUD and the title card.
void App::updateBreak(float dt, const std::vector<HudText>& hud) {
    (void)dt;
    Renderer::Break b;
    b.blank = &blank_;
    float t = breakT_;
    // Test hook: REFALL_TEST_CARD=<seconds> holds the break's intro (the title card) at that moment, e.g. in a review
    // shot (0.4 the card comes up, 1.4 .. 3.2 held big, 5.2 landed in the corners).
    static const float testCard = std::getenv("REFALL_TEST_CARD") ? std::atof(std::getenv("REFALL_TEST_CARD")) : -1.f;
    const bool cardTest = testCard >= 0.f && break_ == BREAK_OFF;
    if (cardTest) t = std::min(testCard, IN_SHOW - 0.01f);
    // Test hook: REFALL_BARE=1 shows the world alone (what a break leaves), e.g. in screenshots.
    static const bool bareTest = std::getenv("REFALL_BARE") != nullptr;
    if (bareTest && break_ == BREAK_OFF) b.play = 0.f;
    // Test hook: REFALL_TEST_PHASES=T plays a scene's setup then its teardown around song time T, as a break would
    // (from black: the world alone, then the game for two seconds, then the game away and the world to black), for
    // clips of them (--clip-at T+5.5 --clip-before 5.5 --clip-after 6).
    static const float phasesAt = std::getenv("REFALL_TEST_PHASES") ? (float)std::atof(std::getenv("REFALL_TEST_PHASES")) : -1.f;
    if (phasesAt >= 0.f && break_ == BREAK_OFF) {
        const float u = (float)songTime_ - phasesAt;
        if (u < 5.2f) {
            b.setup = saturate(u / 5.2f);
            b.black = 1.f - smoothstepf(0.f, 1.2f, u);
            b.play = smoothstepf(3.4f, 5.2f, u);
        } else if (u > 7.2f) {
            const float t = u - 7.2f;
            b.leaving = true;
            b.teardown = saturate(t / 4.f);
            b.play = 1.f - smoothstepf(0.f, 1.6f, t);
            b.black = smoothstepf(2.6f, 4.f, t);
        }
        if (b.play < 1.f)
            for (const HudText& h : hud)
                if (h.role == HUD_TITLE || h.role == HUD_INFO || h.role == HUD_UNIVERSE) b.bareHud.push_back(h);
    }
    if (introAtStart_ && !track_) {
        // Before the first song: the black, a quiet word while it loads.
        int w, h;
        fbSize(w, h);
        b.black = 1.f;
        TitleCard::Line& l = b.card[1];
        l.text = lib_.empty() ? "NO SONGS FOUND" : "LOADING THE FIRST SONG";
        l.face = TitleCard::SANS;
        l.cap = h * 0.016f;
        l.track = 0.45f;
        l.center = true;
        l.x = w * 0.5f;
        l.y = h * 0.5f;
        l.alpha = 0.45f * smoothstepf(0.5f, 1.5f, runTime_);
    }
    if (break_ == BREAK_OUT) {
        b.leaving = true;
        const float hide = breakSkip_ ? 1.f : OUT_HIDE;
        // the teardown runs from the song's last game frame to the end of the black (with the song when it plays on)
        float tEnd = hide + 1.6f;
        if (!breakSkip_ && track_) tEnd = std::max(OUT_HIDE + 1.f, t + (float)(track_->duration() - 0.2 - songTime_));
        b.teardown = saturate(t / std::max(0.1f, tEnd));
        b.play = 1.f - smoothstepf(0.f, hide, t);
        const float dark0 = hide + 0.4f;
        float black = smoothstepf(dark0, dark0 + 1.2f, t);
        if (!breakSkip_ && track_) {
            // ends with the song, but never before the game is gone
            const float songBlack = smoothstepf((float)track_->duration() - OUT_DARK - 0.2f, (float)track_->duration() - 0.2f, (float)songTime_);
            black = std::min(songBlack, smoothstepf(OUT_HIDE, OUT_HIDE + 1.f, t));
            if (songFinished() && t >= OUT_HIDE + 1.f) black = 1.f;
        }
        b.black = black;
    } else if (break_ == BREAK_IN || cardTest) {
        if (t >= IN_SHOW && t - dt < IN_SHOW) {
            game_.respawn(); // the falling piece back at the top, to fall again as the game comes back
            handleEvents();
        }
        b.play = smoothstepf(IN_SHOW, IN_END, t);
        b.black = 1.f - smoothstepf(IN_SHRINK, IN_WORLD, t);
        // the setup runs from the world's first light to the game's arrival
        b.setup = saturate((t - IN_SHRINK) / (IN_END - IN_SHRINK));
        const float bareA = smoothstepf(IN_WORLD - 0.6f, IN_WORLD + 0.2f, t);
        for (const HudText& h : hud)
            if (h.role == HUD_TITLE || h.role == HUD_INFO || h.role == HUD_UNIVERSE) {
                b.bareHud.push_back(h);
                b.bareHud.back().alpha *= bareA;
            }
        // The title card: big in the middle, then shrinking into the corners where the universe's HUD letters them.
        int w, h;
        fbSize(w, h);
        const float s = std::max(1.f, h / 900.f);
        const float k = smoothstepf(IN_SHRINK, IN_WORLD, t), e = k * k * (3.f - 2.f * k);
        const float a = smoothstepf(IN_CARD, IN_HOLD, t) * (1.f - smoothstepf(IN_WORLD - 0.5f, IN_WORLD + 0.2f, t));
        auto fold = [](std::string x) {
            for (size_t i; (i = x.find("\xC2\xB7")) != std::string::npos;) x.replace(i, 2, "\x7F");
            return x;
        };
        TitleCard::Line& ti = b.card[0];
        ti.text = track_ ? fold(asciiFold(track_->title)) : std::string();
        ti.face = TitleCard::SERIF_ITALIC;
        ti.cap = h * 0.075f;
        ti.track = 0.f;
        const float bigW = std::min(1.f, w * 0.86f / std::max(1.f, R_.titleCard().width(ti)));
        const float capBig = ti.cap * bigW, capSmall = 2.2f * s * 8.f;
        ti.cap = capBig;
        const float leftBig = w * 0.5f - R_.titleCard().width(ti) * 0.5f;
        ti.cap = lerpf(capBig, capSmall, e);
        ti.x = lerpf(leftBig, 28.f * s, e);
        ti.y = lerpf(h * 0.42f - capBig, 24.f * s + 2.f * s, e);
        ti.alpha = a;
        TitleCard::Line& un = b.card[1];
        un.text = fold(R_.label());
        un.face = TitleCard::SANS;
        un.track = lerpf(0.45f, 0.12f, e);
        const float uBig = h * 0.022f, uSmall = 1.3f * s * 8.f;
        un.cap = uBig;
        const float uLeftBig = w * 0.5f - R_.titleCard().width(un) * 0.5f;
        un.cap = lerpf(uBig, uSmall, e);
        un.x = lerpf(uLeftBig, 28.f * s, e);
        un.y = lerpf(h * 0.42f + h * 0.06f, h - 50.f * s + 2.f * s, e);
        un.alpha = a * lerpf(0.85f, 0.6f, e);
        // the same card in the universe's own lettering (the host's above when it can't be captured)
        for (const HudText& h : hud)
            if (h.role == HUD_TITLE || h.role == HUD_UNIVERSE) b.cardHud.push_back(h);
        b.cardId = sceneCounter_;
        b.cardShrink = e;
        b.cardAlpha = a;
        b.cardIn = smoothstepf(IN_CARD, IN_HOLD, t);
        if (t >= IN_END) {
            if (songHeld_) { // the game starts: so does the song
                songHeld_ = false;
                audio_.setPaused(paused_);
                std::printf("[app] the song starts with the game\n");
            }
            break_ = BREAK_OFF;
            b = Renderer::Break{};
            std::printf("[app] break: play\n");
        }
    }
    // Paused in a break (on the title card, the black): the pause menu over the world, dimmed, the card hidden.
    static const bool testPause = std::getenv("REFALL_TEST_PAUSE") != nullptr; // (see buildHud)
    if ((paused_ || testPause) && break_ != BREAK_OFF) {
        b.black = std::min(b.black, 0.55f);
        b.card[0].alpha = b.card[1].alpha = 0.f;
        b.cardAlpha = b.cardIn = 0.f;
        if (!b.blank) b.blank = &blank_;
        b.play = std::min(b.play, 0.999f); // the world alone, with the menu's lines
        for (const HudText& h : hud)
            if (h.role == HUD_DIALOG) b.bareHud.push_back(h);
    }
    R_.setBreak(std::move(b));
}

std::vector<HudText> App::buildHud(float dt) {
    std::vector<HudText> hud;
    int w, h;
    fbSize(w, h);
    const float s = std::max(1.f, h / 900.f);
    auto upper = [](std::string x) { for (auto& c : x) c = (char)toupper((unsigned char)c); return x; };

    if (track_) {
        float a = trackAge_ < 7.f ? smoothstepf(0, 1, trackAge_) : 0.35f;
        const Footprint& fp = track_->analysis.fp;
        hud.push_back({displayTitle(track_->title), 28 * s, 24 * s, 2.2f * s, a, false, false, HUD_TITLE});
        char info[128];
        std::snprintf(info, sizeof(info), "%.0f BPM   %s", fp.bpm, keyName(fp.key, fp.minor).c_str());
        hud.push_back({info, 28 * s, 50 * s, 1.5f * s, a * 0.7f, true, false, HUD_INFO});
    } else if (!introAtStart_) { // (the intro's black says it in a quieter word)
        hud.push_back({lib_.empty() ? "NO SONGS FOUND" : "LOADING THE FIRST SONG...", w * 0.5f, h * 0.5f - 60 * s,
                       3.f * s, 0.8f, false, true, HUD_DIALOG});
    }
    sceneNameTimer_ = std::max(0.f, sceneNameTimer_ - dt);
    if (track_ && !track_->analysis.segments.empty()) {
        const Segment& sg = track_->analysis.segments[track_->analysis.segmentAt(songTime_)];
        hud.push_back({segmentName(sg.kind), 28 * s, h - 80.f * s, 1.3f * s, 0.42f, true, false, HUD_SECTION});
    }
    // The universe and its variant ("VECTOR · P3 AMBER").
    const float sceneA = 0.26f + 0.3f * smoothstepf(0, 2, sceneNameTimer_);
    hud.push_back({R_.label(), 28 * s, h - 50.f * s, 1.3f * s, sceneA, false, false, HUD_UNIVERSE});

    // Score panel under the hold slot, labels above previews.
    vec2 hold = R_.project(vec3(-8.6f, 9.4f, 0)), next = R_.project(vec3(8.6f, 9.4f, 0));
    vec2 sc = R_.project(vec3(-8.6f, 1.5f, 0));
    hud.push_back({"HOLD", hold.x, hold.y, 1.6f * s, 0.62f, false, true, HUD_LABEL});
    hud.push_back({"NEXT", next.x, next.y, 1.6f * s, 0.62f, false, true, HUD_LABEL});
    hud.push_back({"SCORE", sc.x, sc.y - 2 * s, 1.6f * s, 0.62f, false, true, HUD_LABEL});
    hud.push_back({text::thousands(game_.score()), sc.x, sc.y + 18 * s, 2.8f * s, 0.9f, false, true, HUD_VALUE});
    hud.push_back({"LINES", sc.x, sc.y + 60 * s, 1.6f * s, 0.62f, false, true, HUD_LABEL});
    hud.push_back({std::to_string(game_.lines()), sc.x, sc.y + 80 * s, 2.8f * s, 0.9f, false, true, HUD_VALUE});
    if (const CampaignStage* st = runStage()) {
        // the campaign has no levels: the stage and the song in it
        hud.push_back({"STAGE " + std::to_string(run_.stage + 1), sc.x, sc.y + 122 * s, 1.6f * s, 0.62f, false, true, HUD_LABEL});
        hud.push_back({std::to_string(std::max(0, run_.current) + 1) + "/" + std::to_string(st->songs.size()), sc.x,
                       sc.y + 142 * s, 2.8f * s, 0.9f, false, true, HUD_VALUE});
    } else {
        hud.push_back({"LEVEL", sc.x, sc.y + 122 * s, 1.6f * s, 0.62f, false, true, HUD_LABEL});
        hud.push_back({std::to_string(game_.level()) + (game_.level() >= Game::MAX_LEVEL ? " MAX" : ""), sc.x, sc.y + 142 * s,
                       2.8f * s, 0.9f, false, true, HUD_VALUE});
    }
    if (game_.best() > 0) {
        hud.push_back({"BEST", sc.x, sc.y + 184 * s, 1.6f * s, 0.5f, false, true, HUD_LABEL});
        hud.push_back({text::thousands(game_.best()), sc.x, sc.y + 204 * s, 2.f * s, 0.6f, false, true, HUD_VALUE});
    }
    // The speed gauge under the panel: the velocity on a log scale (level 1 .. MAX_VELOCITY, so the slow ones still
    // fill some of it), the hyperdrop's zone from where it can happen on (HARD 30, EXPERT 24), "SPEED" under it and
    // "HYPERDROP" in the accent while it is on.
    {
        const float lnMax = std::log(Game::MAX_VELOCITY);
        const float pos = std::log(std::clamp(gravityV_, 1.f, Game::MAX_VELOCITY)) / lnMax;
        speedPos_ = speedPos_ < 0.f ? pos : approach(speedPos_, pos, 4.f, dt);
        speedHot_ = approach(speedHot_, game_.twentyG() ? 1.f : 0.f, 6.f, dt);
        const float bw = 80.f * s, by = sc.y + 248.f * s;
        Renderer::Speed sp;
        sp.x = sc.x - bw * 0.5f;
        sp.y = by;
        sp.w = bw;
        sp.pos = speedPos_;
        sp.zone = hyperAble_ ? std::log(hyperFrom_) / lnMax : -1.f;
        sp.hot = speedHot_;
        sp.alpha = 1.f;
        R_.setSpeed(sp);
        const bool hot = speedHot_ > 0.5f;
        hud.push_back({hot ? "HYPERDROP" : "SPEED", sc.x, by + 12 * s, 1.4f * s, hot ? 0.7f : 0.5f, hot, true, HUD_LABEL});
    }
    if (game_.combo() > 0) {
        vec2 c = R_.project(vec3(8.6f, -4.5f, 0));
        hud.push_back({"COMBO " + std::to_string(game_.combo()), c.x, c.y, 2.f * s, 0.8f, true, true, HUD_NOTICE});
    }

    // Persistent, discreet skip hint (top right) and feedback when skipping.
    if (lib_.size() > 1 && mode_ != MODE_CAMPAIGN && !viewer_) {
        const char* key = input_.active() == Input::Gamepad ? "BACK  NEXT SONG" : "N  NEXT SONG";
        hud.push_back({key, -28 * s, 24 * s, 1.4f * s, 0.3f, false, false, HUD_HINT});
    }
    skipToast_ = std::max(0.f, skipToast_ - dt);
    if (wantSkip_ || skipToast_ > 0)
        hud.push_back({wantSkip_ ? "NEXT SONG LOADING: " + displayTitle(lib_.loadingTitle()) : "NOW: " + (track_ ? displayTitle(track_->title) : std::string()), -28 * s, 44 * s, 1.4f * s,
                       wantSkip_ ? 0.8f : std::min(0.8f, skipToast_), true, false, HUD_HINT});

    // Seek feedback: song time and the section the song is in.
    seekToast_ = std::max(0.f, seekToast_ - dt);
    if (seekToast_ > 0 && track_) {
        const Analysis& an = track_->analysis;
        int sec = (int)songTime_;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%d:%02d  %s", sec / 60, sec % 60,
                      an.segments.empty() ? "" : segmentName(an.segments[an.segmentAt(songTime_)].kind));
        hud.push_back({buf, w * 0.5f, 24 * s, 2.f * s, std::min(1.f, seekToast_ * 2.f), true, true, HUD_NOTICE});
    }

    failedToast_ = std::max(0.f, failedToast_ - dt);
    if (failedToast_ > 0)
        hud.push_back({"COULD NOT LOAD " + asciiFold(failedTitle_) + ", TRYING ANOTHER SONG", -28 * s, 64 * s, 1.3f * s,
                       std::min(1.f, failedToast_), true, false, HUD_HINT});

    // Bonus gauge (a ring below the "next" previews): the key to press in its center once it's full.
    {
        const vec3 gc = Renderer::gaugeCenter();
        vec2 g = R_.project(gc), lbl = R_.project(gc - vec3(0, Renderer::GAUGE_R + 0.55f, 0));
        const bool pad = input_.active() == Input::Gamepad;
        if (game_.bonusReady()) {
            float br = 0.55f + 0.3f * (0.5f + 0.5f * std::sin(runTime_ * TAU * 0.4f));
            hud.push_back({pad ? "ZL ZR" : "V", g.x, g.y - 7 * s, (pad ? 1.3f : 1.8f) * s, br, true, true, HUD_KEY});
        }
        hud.push_back({game_.bonusActive() || game_.bonusReady() ? "SLOW" : "BONUS", lbl.x, lbl.y, 1.4f * s,
                       game_.bonusActive() ? 0.7f : game_.bonusReady() ? 0.62f : 0.5f, game_.bonusActive(), true, HUD_LABEL});
    }
    slowToast_ = std::max(0.f, slowToast_ - dt);
    if (slowToast_ > 0) {
        vec2 c = R_.project(vec3(0, 8.5f, 0));
        float a = std::min(1.f, slowToast_) * std::min(1.f, (2.f - slowToast_) * 4.f);
        hud.push_back({"SLOW MOTION", c.x, c.y, 3.f * s, 0.75f * a, true, true, HUD_TOAST});
    }
    quadToast_ = std::max(0.f, quadToast_ - dt);
    if (quadToast_ > 0) {
        vec2 c = R_.project(vec3(0, 6.5f, 0)); // between the slow-motion and level toasts
        float a = std::min(1.f, quadToast_ * 1.5f) * std::min(1.f, (2.4f - quadToast_) * 5.f);
        float grow = 1.f + 0.08f * smoothstepf(2.4f, 1.6f, quadToast_);
        hud.push_back({quadB2B_ ? std::string("BACK TO BACK ") + QUAD_NAME : std::string(QUAD_NAME), c.x, c.y, (quadB2B_ ? 3.4f : 4.2f) * grow * s, 0.9f * a,
                       true, true, HUD_TOAST});
        hud.push_back({"+" + text::thousands(quadPoints_), c.x, c.y + 44 * s, 2.2f * s, 0.75f * a, false, true, HUD_TOAST_SUB});
    }

    levelToast_ = std::max(0.f, levelToast_ - dt);
    if (levelToast_ > 0) {
        vec2 c = R_.project(vec3(0, 2.f, 0));
        float a = std::min(1.f, levelToast_) * std::min(1.f, (2.5f - levelToast_) * 4.f);
        hud.push_back({"LEVEL " + std::to_string(game_.level()), c.x, c.y, 3.2f * s, 0.8f * a, true, true, HUD_TOAST});
    }

        // Input device toast.
    if (input_.deviceChangedTimer() > 0 && opt_.shotPrefix.empty() && opt_.clipPath.empty()) { // not in offline renders
        std::string d = input_.active() == Input::Gamepad ? "GAMEPAD  " + upper(input_.gamepadName()) : "KEYBOARD";
        hud.push_back({d, -28 * s, 70.f * s, 1.5f * s, std::min(1.f, input_.deviceChangedTimer()), true, false, HUD_HINT});
    }

    if (game_.over() && overTime_ > 1.0f && mode_ != MODE_CAMPAIGN) { // (a Voyage's game over has its debrief)
        float a = std::min(1.f, (overTime_ - 1.0f) / 1.2f);
        hud.push_back({"GAME OVER", w * 0.5f, h * 0.5f - 70 * s, 5.f * s, 0.95f * a, true, true, HUD_DIALOG});
        hud.push_back({"SCORE  " + text::thousands(game_.lastScore()), w * 0.5f, h * 0.5f + 0 * s, 2.f * s, 0.85f * a, false, true, HUD_DIALOG});
        hud.push_back({"BEST  " + text::thousands(game_.best()), w * 0.5f, h * 0.5f + 30 * s, 1.6f * s, 0.6f * a, false, true, HUD_DIALOG});
        if (overTime_ > 2.5f) {
            float b = std::min(1.f, (overTime_ - 2.5f) / 0.8f);
            const bool pad = input_.active() == Input::Gamepad;
            std::string again = pad ? "PRESS A BUTTON TO PLAY AGAIN" : "PRESS A KEY TO PLAY AGAIN";
            if (mode_ == MODE_CAMPAIGN) again = pad ? "PRESS A BUTTON: BACK TO THE VOYAGE" : "PRESS A KEY: BACK TO THE VOYAGE";
            else if (menuEnabled_) again = pad ? "PRESS A BUTTON: BACK TO THE MENU" : "PRESS A KEY: BACK TO THE MENU";
            hud.push_back({again, w * 0.5f, h * 0.5f + 70 * s, 1.8f * s, 0.75f * b, false, true, HUD_DIALOG});
        }
    }
    // Test hook: REFALL_TEST_PAUSE=0|1|2 shows the pause menu (that choice lit) without pausing, e.g. in a review shot;
    // 3 its controls page.
    static const int testPause = std::getenv("REFALL_TEST_PAUSE") ? std::atoi(std::getenv("REFALL_TEST_PAUSE")) : -1;
    if (testPause >= 0) {
        pauseSel_ = std::min(testPause, 2);
        pauseControls_ = testPause == 3;
    }
    if ((paused_ || testPause >= 0) && pauseControls_ && input_.remapping()) {
        // the gamepad remapped: the control asked for
        const int st = input_.remapStep();
        hud.push_back({"PRESS", w * 0.5f, h * 0.5f - 110 * s, 1.8f * s, 0.75f, false, true, HUD_DIALOG});
        hud.push_back({Input::remapName(st), w * 0.5f, h * 0.5f - 40 * s, 5.f * s, 0.95f, true, true, HUD_DIALOG});
        hud.push_back({Input::remapUse(st), w * 0.5f, h * 0.5f + 20 * s, 2.f * s, 0.8f, false, true, HUD_DIALOG});
        hud.push_back({std::to_string(st + 1) + " / " + std::to_string(Input::REMAP_COUNT), w * 0.5f, h * 0.5f + 70 * s, 1.8f * s, 0.6f,
                       false, true, HUD_DIALOG});
        hud.push_back({input_.gamepadConnected() ? "SPACE SKIPS   ESC CANCELS" : "CONNECT A GAMEPAD   ESC CANCELS", w * 0.5f,
                       h * 0.5f + 120 * s, 1.6f * s, 0.5f, false, true, HUD_DIALOG});
    } else if ((paused_ || testPause >= 0) && pauseControls_) {
        // the controls, one per line: the action, then its keys (or buttons)
        std::vector<std::pair<const char*, const char*>> lines;
        if (input_.active() == Input::Gamepad)
            lines = {{"MOVE", "DPAD / STICK"}, {"ROTATE", "A / B"}, {"HARD DROP", "UP"}, {"HOLD", "LB / RB"},
                     {"SLOW MOTION", "ZL / ZR"}, {"NEW SCENE", "Y"}, {"NEXT SONG", "BACK"}, {"PAUSE", "START"}};
        else
            lines = {{"MOVE", "ARROWS"}, {"ROTATE", "UP / Z"}, {"HARD DROP", "SPACE"}, {"HOLD", "C"}, {"SLOW MOTION", "V"},
                     {"NEW SCENE", "T"}, {"NEXT SONG", "N"}, {"SEEK", "CTRL+SHIFT+ARROWS"}, {"FULLSCREEN", "F"}, {"PAUSE", "ESC"}};
        if (mode_ == MODE_CAMPAIGN) // no skipping in a campaign, its universes are set
            lines.erase(std::remove_if(lines.begin(), lines.end(), [](const std::pair<const char*, const char*>& l) {
                const std::string a = l.first;
                return a == "NEXT SONG" || a == "SEEK" || a == "NEW SCENE"; }), lines.end());
        const float lineH = 30.f * s, top = h * 0.5f - (lines.size() - 1) * lineH * 0.5f - 60 * s;
        hud.push_back({"CONTROLS", w * 0.5f, top - 80 * s, 4.f * s, 0.95f, true, true, HUD_DIALOG});
        for (size_t i = 0; i < lines.size(); i++)
            hud.push_back({std::string(lines[i].first) + "   " + lines[i].second, w * 0.5f, top + i * lineH, 1.8f * s, 0.8f,
                           false, true, HUD_DIALOG});
        // then its choices: the gamepad remapped button by button, or its default back, and BACK (lit in the accent)
        const char* items[3] = {"REMAP GAMEPAD", "RESTORE MAPPING", "BACK"};
        const float y0 = top + (lines.size() - 1) * lineH + 60 * s;
        for (int i = 0; i < 3; i++)
            hud.push_back({items[i], w * 0.5f, y0 + 40.f * i * s, 2.f * s, ctrlSel_ == i ? 0.95f : 0.4f, ctrlSel_ == i, true, HUD_DIALOG});
        if (ctrlNoteT_ > 0)
            hud.push_back({ctrlNote_, w * 0.5f, y0 + 140 * s, 1.6f * s, 0.8f * std::min(1.f, ctrlNoteT_), false, true, HUD_DIALOG});
    } else if (paused_ || testPause >= 0) {
        hud.push_back({"PAUSED", w * 0.5f, h * 0.5f - 60 * s, 5.f * s, 0.95f, true, true, HUD_DIALOG});
        // the menu: the choice lit in the accent, the others dimmed
        const char* items[3] = {"CONTINUE", "CONTROLS", menuEnabled_ ? "MENU" : "QUIT"};
        for (int i = 0; i < 3; i++)
            hud.push_back({items[i], w * 0.5f, h * 0.5f + (20.f + 44.f * i) * s, 2.4f * s, pauseSel_ == i ? 0.95f : 0.4f,
                           pauseSel_ == i, true, HUD_DIALOG});
    }
    // the viewer's overlay shows the song, its universe and its section: the corners' lines go while it shows
    if (viewer_ && vv_ && vv_->visible())
        hud.erase(std::remove_if(hud.begin(), hud.end(), [](const HudText& t) {
            return t.role == HUD_TITLE || t.role == HUD_INFO || t.role == HUD_UNIVERSE || t.role == HUD_SECTION; }), hud.end());
    return hud;
}

// --stream: the pipes tools/stream.py hands over, and a thread reading the phone's gamepad from stdin.
void App::startStream() {
#ifndef _WIN32
    auto fd = [](const char* name) { const char* v = std::getenv(name); return v ? std::atoi(v) : -1; };
    streamVideo_ = fd("REFALL_STREAM_VIDEO_FD");
    streamAudio_ = fd("REFALL_STREAM_AUDIO_FD");
    if (streamAudio_ >= 0) {
        // the sound never waits: a block the server is too late for is dropped
        fcntl(streamAudio_, F_SETFL, fcntl(streamAudio_, F_GETFL) | O_NONBLOCK);
        const int afd = streamAudio_;
        audio_.setTap([afd](const float* pcm, unsigned int frames) {
            int16_t buf[4096];
            for (unsigned int i = 0; i < frames * 2;) {
                const unsigned int n = std::min(frames * 2 - i, 4096u);
                for (unsigned int k = 0; k < n; k++) buf[k] = (int16_t)std::lround(std::clamp(pcm[i + k], -1.f, 1.f) * 32767.f);
                if (write(afd, buf, n * 2) < 0) return;
                i += n;
            }
        });
    }
    std::thread([this] {
        char line[256];
        while (std::fgets(line, sizeof(line), stdin)) {
            unsigned char b[15] = {};
            float a[6] = {0, 0, 0, 0, -1, -1};
            char bits[32] = {};
            if (std::sscanf(line, "pad %31s %f %f %f %f %f %f", bits, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) == 7) {
                for (int i = 0; i < 15 && bits[i]; i++) b[i] = bits[i] == '1';
                input_.setRemotePad(b, a);
            }
        }
        quit_ = true; // the server is gone
    }).detach();
    std::printf("[stream] %dx%d, video fd %d, audio fd %d\n", opt_.width, opt_.height, streamVideo_, streamAudio_);
#endif
}

// --stream: the frame (bottom row first) after an 8-byte header (width, height), then a wait for the next 60th of a second.
void App::streamFrame() {
#ifndef _WIN32
    int w, h;
    fbSize(w, h);
    streamPx_.resize(8 + (size_t)w * h * 3);
    const uint32_t head[2] = {(uint32_t)w, (uint32_t)h};
    std::memcpy(streamPx_.data(), head, 8);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, streamPx_.data() + 8);
    for (size_t off = 0; off < streamPx_.size();) {
        const ssize_t n = write(streamVideo_, streamPx_.data() + off, streamPx_.size() - off);
        if (n <= 0) { quit_ = true; streamVideo_ = -1; return; }
        off += (size_t)n;
    }
    const auto now = std::chrono::steady_clock::now();
    streamNext_ = std::max(streamNext_ + std::chrono::microseconds(16667), now - std::chrono::milliseconds(50));
    if (streamNext_ > now) std::this_thread::sleep_for(streamNext_ - now);
#endif
}

int App::run() {
    runSeed_ = opt_.seed ? opt_.seed
                         : (std::random_device{}() ^ (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count());
    const bool clipMode = !opt_.clipPath.empty();
    const bool shotMode = !opt_.shotPrefix.empty() || clipMode; // offline: fixed time step, no sound
    const bool reviewMode = !opt_.reviewPath.empty();
    offline_ = shotMode;
    viewer_ = opt_.viewer;
    if (viewer_) { // the computer always plays; a --shot runs on the song's simulated clock, silent
        opt_.autoplay = true;
        if (!opt_.viewShot.empty()) offline_ = opt_.mute = true;
    }

    // Offline renders (clips, shots, reviews) run headless on the GPU when they can (REFALL_HEADLESS=0: a window)
    if (testPlay()) { offline_ = true; opt_.mute = true; }
    const bool wantHeadless = (testPlay() || shotMode || reviewMode || !opt_.transitionShots.empty() || !opt_.analysisPath.empty() || !opt_.menuShot.empty() || !opt_.menuClip.empty() || !opt_.viewShot.empty()) && !opt_.fullscreen &&
                              !(std::getenv("REFALL_HEADLESS") && std::strcmp(std::getenv("REFALL_HEADLESS"), "0") == 0);
    // (and REFALL_DEBUG_FALL with --hidden: real play, in real time, headless)
    const bool debugFallHeadless = opt_.hidden && std::getenv("REFALL_DEBUG_FALL");
    const bool streamHeadless = (opt_.stream || debugFallHeadless) && !opt_.fullscreen;
    if ((wantHeadless || streamHeadless) && headless::init(opt_.width, opt_.height)) {
        headless_ = true;
        headW_ = opt_.width; headH_ = opt_.height;
        if (!gladLoadGL((GLADloadfunc)headless::proc)) { std::fprintf(stderr, "could not load OpenGL functions\n"); return 1; }
    } else {
    if (wantHeadless) std::fprintf(stderr, "[gl] no headless GPU context, using a window\n");
    // on the discrete GPU when there are two; if that fails, again on the default one
    bool discrete = preferDiscreteGpu();
    // (GLFW's own reason when it fails: no display, a missing library...)
    glfwSetErrorCallback([](int code, const char* what) {
#ifdef REFALL_WEB
        if (code == 0) return; // (the web port's notes on the desktop hints it ignores)
#endif
        std::fprintf(stderr, "[glfw] error %d: %s\n", code, what);
    });
    for (;;) {
        if (!glfwInit()) {
            if (discrete) { undoDiscreteGpu(); discrete = false; continue; }
            std::fprintf(stderr, "glfwInit failed\n");
            return 1;
        }
#ifndef REFALL_WEB // (the web build: WebGL 2)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        if (opt_.hidden) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        if (shotMode) glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWmonitor* mon = opt_.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        int ww = opt_.width, wh = opt_.height;
        if (mon) { const GLFWvidmode* vm = glfwGetVideoMode(mon); ww = vm->width; wh = vm->height; }
        // the desktop's name for the window (the taskbar / Alt+Tab match it to an installed refall.desktop)
#ifdef GLFW_WAYLAND_APP_ID
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, "refall");
#endif
#ifdef GLFW_X11_CLASS_NAME
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "refall");
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "refall");
#endif
        win_ = glfwCreateWindow(ww, wh, (opt_.viewer ? std::string(GAME_TITLE) + " voyage viewer" : std::string(GAME_TITLE)).c_str(), mon, nullptr);
        if (win_) break;
        glfwTerminate();
        if (discrete) { undoDiscreteGpu(); discrete = false; continue; }
        std::fprintf(stderr, "could not create an OpenGL 3.3 window\n");
        return 1;
    }
    Input::loadMappings();
#ifndef REFALL_WEB
    { // the window's icon: the REFALL logo (where the platform takes one; Wayland uses the desktop file's)
        std::vector<GLFWimage> icons;
        for (const IconImage& ic : ICON_IMAGES) icons.push_back({ic.size, ic.size, const_cast<unsigned char*>(ic.rgba)});
#ifdef GLFW_PLATFORM_WAYLAND
        if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
#endif
            glfwSetWindowIcon(win_, (int)icons.size(), icons.data());
    }
#endif
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(shotMode || opt_.hidden ? 0 : 1);
#ifndef REFALL_WEB
    glfwSetWindowRefreshCallback(win_, [](GLFWwindow*) { g_refresh = true; }); // (a held frame is drawn again)
#endif
    if (!gladLoadGL(glfwGetProcAddress)) { std::fprintf(stderr, "could not load OpenGL functions\n"); return 1; }
#ifdef REFALL_WEB
    web::setupContext(win_);
#endif
    }
    glGetError();
    std::printf("[gl] %s · %s\n", (const char*)glGetString(GL_VENDOR), (const char*)glGetString(GL_RENDERER));
    // The voyage viewer's universe snapshots are made on a worker thread with its own context (an instance's init can
    // take seconds): a hidden window's context, or a second EGL context when headless.
#ifndef REFALL_WEB
    if (opt_.viewer) {
#else
    if (false) {
#endif
        if (headless_) {
            if (void* wc = headless::createWorkerContext())
                WorldPool::setWorkerContext([wc] { return headless::makeWorkerCurrent(wc); }, [] { headless::makeWorkerCurrent(nullptr); });
        } else {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            if (GLFWwindow* ww = glfwCreateWindow(16, 16, "", nullptr, nullptr))
                WorldPool::setWorkerContext([ww] { glfwMakeContextCurrent(ww); return true; }, [] { glfwMakeContextCurrent(nullptr); });
            glfwMakeContextCurrent(win_);
        }
    }

    campaign_.load(opt_.campaignPath);
    slots_.init(opt_.progressPath, campaign_);
    menu_.setSlots(&slots_);
    progress_.load(slots_.progressPath(slots_.active()), &campaign_);
#ifndef REFALL_WEB // (the web build: the campaign only, no voyages of the player's own, no free play)
    voyages_.load(VoyageStore::defaultDir(opt_.progressPath));
    {
        std::vector<std::string> dirs;
        for (int i = 0; i < SaveSlots::COUNT; i++) dirs.push_back(slots_.voyagesDir(i));
        voyages_.setProgressDirs(dirs, slots_.active());
    }
    voyages_.setMain(campaign_, slots_.progressPath(slots_.active()));
    menu_.setVoyages(&voyages_, [this](const std::string& id) { return openVoyage(id); });
    playlists_.load(opt_.playlistsPath);
    menu_.setPlaylists(&playlists_);
#endif
    menu_.setFreeDifficulty(opt_.difficulty);
    menu_.setFreeLevel(opt_.level);
    menu_.setCampaignDifficulty(opt_.difficultyGiven ? opt_.difficulty : DIFF_NORMAL);
    if (!opt_.menuShot.empty() || !opt_.menuClip.empty()) return runMenuShot();
    menuEnabled_ = opt_.menuEnabled;
    mode_ = opt_.menu ? MODE_MENU : MODE_FREE;
    speedMul_ = opt_.speedMul;
    freeDiff_ = opt_.difficulty;
    game_.setRunDifficulty(opt_.difficulty);
    game_.setStartLevel(opt_.level);
    lastLevel_ = game_.level();

    int fbw, fbh;
    fbSize(fbw, fbh);
    if (!R_.init(fbw, fbh)) std::fprintf(stderr, "[gl] warning: renderer init reported a GL error\n");
    if (win_) input_.init(win_);
    input_.setCheatSkip(opt_.cheatSkip);
    // the player's gamepad remaps, next to the progress (slot 1's: the same on every slot)
    input_.setMappingFile((fs::path(slots_.globalPlacePath()).parent_path() / "gamepad-mapping.tsv").string());
    menu_.setInput(&input_);

    if (opt_.stream) startStream();
    if (!audio_.init(opt_.stream)) std::fprintf(stderr, "[audio] continuing without sound\n");
    audio_.setMuted(opt_.mute || shotMode);
    lib_.setSampleRate(audio_.sampleRate());
    lib_.setShuffle(opt_.shuffle);
    menu_.init(&campaign_, &progress_);
    if (mode_ != MODE_MENU && opt_.stage.empty() && !viewer_) { // the menu's sessions (and --stage, the viewer) scan their own songs
        lib_.scan(opt_.paths);
        std::printf("[app] %zu songs found\n", lib_.size());
        lib_.prefetch(runSeed_);
    }

    game_.reset(runSeed_);
    blank_.makeBlank();
    breaks_ = !std::getenv("REFALL_NO_BREAK") && (!shotMode || clipMode) && !viewer_; // (the viewer goes straight from song to song)
    introAtStart_ = breaks_ && (!shotMode || std::getenv("REFALL_INTRO_TEST"));
    // A universe while the first song loads (the forced one, else one picked from the run seed); the first song
    // keeps it.
    if (!opt_.universe.empty()) {
        forcedUniverse_ = universes::parse(opt_.universe, &forcedVariant_);
    }
    if (viewer_ && !viewerStart()) return 1;
    look_ = makeSongLook(nullptr, runSeed_);
    if (mode_ != MODE_MENU) { // (on the menu, the first session makes its universe: the menu's first frame comes at once)
        int variant = -1;
        changeUniverse(songUniverse(&variant), variant, transitions::NEW_SCENE, true);
    }

    if (!opt_.stage.empty()) { // --stage S[:SONG]: a campaign run straight away
        Menu::Choice c;
        c.kind = Menu::Choice::PLAY_CAMPAIGN;
        c.difficulty = opt_.difficultyGiven ? opt_.difficulty : DIFF_NORMAL;
        int st = 1, sg = 1;
        std::sscanf(opt_.stage.c_str(), "%d:%d", &st, &sg);
        c.stage = std::clamp(st - 1, 0, std::max(0, campaign_.size() - 1));
        c.song = sg - 1;
        if (campaign_.size() == 0 || c.song < 0 || c.song >= (int)campaign_.stages()[c.stage].songs.size()) {
            std::fprintf(stderr, "[campaign] no stage %s\n", opt_.stage.c_str());
            return 1;
        }
        beginSession(c);
    }
    if (mode_ == MODE_MENU) menu_.open(Menu::TITLE);
    if (menuEnabled_) { // back where the menu was last time (the command line's difficulty wins)
        placePath_ = slots_.placePath(slots_.active());
        Menu::Place pl;
        // the game's first launch (no place kept), or REFALL_MENU_INTRO=1: the title's opening
        const bool known = loadPlace(pl);
        if ((!known && mode_ == MODE_MENU) || std::getenv("REFALL_MENU_INTRO")) menu_.startIntro();
        if (known) {
            if (!pl.voyage.empty() && !menu_.selectVoyage(pl.voyage)) pl.stage = -1; // (a voyage deleted since)
            if (opt_.levelGiven) pl.freeLevel = -1; // (--level wins)
            menu_.restore(pl, !opt_.difficultyGiven);
        }
        savedPlace_ = lastPlace_ = menu_.place();
    }
    if (menuEnabled_ && opt_.menuMusic && menu_.music()) startMenuMusic();

    loop_.clipMode = clipMode;
    loop_.shotMode = shotMode;
    loop_.reviewMode = reviewMode;
    loop_.fixedStep = headless_ && !opt_.stream && !(opt_.hidden && std::getenv("REFALL_DEBUG_FALL"));
    loop_.last = loop_.fixedStep ? 0.0 : clock();
#ifdef REFALL_WEB
    // the browser calls the frames (the page's main thread never waits): run() returns, the game goes on
    web::runFrames([this] {
        if (frame()) return true;
        finish();
        return false;
    });
    return 0;
#endif
    while (frame()) {}
    return finish();
}

// One frame of the game's loop (false: the loop is over). On the desktop run() calls it again and again, its swap()
// waiting for the screen; on the web the browser calls it at each of its frames (web::runFrames).
bool App::frame() {
    const bool clipMode = loop_.clipMode, shotMode = loop_.shotMode, reviewMode = loop_.reviewMode, fixedStep = loop_.fixedStep;
    double& last = loop_.last;
    float& fade = loop_.fade;
    int& shotsTaken = loop_.shotsTaken;
    float& shotSettle = loop_.shotSettle;
    FILE*& clipPipe = loop_.clipPipe;
    std::vector<unsigned char>& clipPx = loop_.clipPx;
    int fbw = 0, fbh = 0;
    if ((win_ && glfwWindowShouldClose(win_)) || quit_) return false;
    {
        double now = fixedStep ? last + 1.0 / 60.0 : clock();
        const auto frameStart = std::chrono::steady_clock::now();
        float dt = shotMode ? 1.f / 60.f : (float)std::min(0.1, now - last);
        last = now;
        runTime_ += dt;
        if (win_) {
            glfwPollEvents();
            hideIdleCursor(dt);
        }
        if (!shotMode && (win_ || opt_.stream)) input_.update(dt); // offline renders ignore the keyboard and gamepads: they must be reproducible
        const bool still = win_ && !shotMode && !testPlay() && !viewer_ && mode_ != MODE_MENU && paused_ &&
                           !input_.changed() && cursorIdle_ > 0 && !g_refresh &&
                           !input_.remapping() && ctrlNoteT_ <= 0; // (a remap's steps come without any action changing)
        g_refresh = false;
        stillT_ = still ? stillT_ + dt : 0;
        held_ = stillT_ > 2.5f; // (the world's dimming settled)
        if (testPlay()) {
            static const auto keys = testList("REFALL_TEST_KEYS"), shots = testList("REFALL_TEST_SHOTS");
            static const double quitAt = std::getenv("REFALL_TEST_QUIT") ? std::atof(std::getenv("REFALL_TEST_QUIT")) : 1e9;
            input_.testFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // background loads keep up with the simulated clock
            static const std::pair<const char*, Action> NAMES[] = {{"up", A_UI_UP}, {"down", A_UI_DOWN}, {"left", A_UI_LEFT},
                {"right", A_UI_RIGHT}, {"ok", A_UI_OK}, {"back", A_UI_BACK}, {"alt", A_UI_ALT}, {"pause", A_PAUSE}, {"quit", A_QUIT},
                {"skip", A_CHEAT_SKIP}};
            for (auto& [t, k] : keys)
                if (runTime_ - dt < t && runTime_ >= t)
                    for (auto& [n, a] : NAMES)
                        if (k == n) { input_.testPress(a); std::printf("[test] %.2f s: %s\n", runTime_, n); }
            for (auto& [t, f] : shots)
                if (runTime_ - dt < t && runTime_ >= t) testShot_ = f;
            if (runTime_ >= quitAt) quit_ = true;
        }

        updateMenuMusic();
#ifdef REFALL_WEB
        web::persist(runTime_);
        web::adaptResolution(mode_ == MODE_MENU ? -1 : R_.universe()); // (each universe its own resolution)
#endif

        // ---- The menus.
        if (mode_ == MODE_MENU) {
            if (input_.pressed(A_FULLSCREEN) && !menu_.editing()) toggleFullscreen();
            // --cheat-skip: S on an open petal clears its stage as a whole run would (the debrief, then the petal's
            // unlock), its songs beaten, with no score
            if (opt_.cheatSkip && input_.pressed(A_CHEAT_SKIP) && menu_.screen() == Menu::CAMPAIGN && !menu_.editing() && !menu_.celebrating()) {
                const Menu::Place pl = menu_.place();
                if (pl.stage >= 0 && pl.stage < campaign_.size() && progress_.unlocked(pl.stage, pl.voyageDiff)) {
                    run_ = Run{};
                    run_.stage = pl.stage;
                    run_.diff = pl.voyageDiff;
                    runSnapshot();
                    game_.reset(runSeed_);
                    for (int k = 0; k < (int)campaign_.stages()[pl.stage].songs.size(); k++) progress_.setBeaten(pl.stage, k, run_.diff, 0, 0);
                    std::printf("[cheat] stage %d cleared\n", pl.stage + 1);
                    campaignEnd();
                    swap();
                    return true;
                }
            }
            const bool remapped = remapKeys(); // (a remap takes the keys)
            if (input_.remapDone()) menu_.note(Menu::GAMEPAD, "GAMEPAD REMAPPED");
            const Menu::Choice c = remapped ? Menu::Choice{} : menu_.update(input_, dt);
            const bool plays = c.kind == Menu::Choice::PLAY_FREE || c.kind == Menu::Choice::PLAY_CAMPAIGN;
            // an option's choice before this frame is drawn (its "done" shows at once, the rows not seen again)
            if (!plays && c.kind != Menu::Choice::NONE && c.kind != Menu::Choice::QUIT) applyOption(c);
            fbSize(fbw, fbh);
            // (a choice leaves the menu at once: its screen may already have moved on, e.g. RETRY back on the Voyage's,
            // so this last frame is black, as the leaving screen faded to, rather than a flash of the menu)
            if (plays) {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, fbw, fbh);
                glClearColor(0.f, 0.f, 0.f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);
            } else if (fbw > 0 && fbh > 0 && !held_) menu_.render(fbw, fbh);
            static bool firstFrame = true;
            if (firstFrame) {
                firstFrame = false;
                std::printf("[menu] first frame %.0f ms after the start\n",
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_startTime).count());
            }
            if (!testShot_.empty()) { menu_.screenshot(testShot_, fbw, fbh); std::printf("[test] shot %s\n", testShot_.c_str()); testShot_.clear(); }
            swap();
            keepPlace(c.kind != Menu::Choice::NONE);
            if (c.kind == Menu::Choice::QUIT) {
#ifndef REFALL_WEB // (a page isn't quit)
                quit_ = true;
#endif
            } else if (plays) beginSession(c);
            return true;
        }

        // ---- Songs.
        if (!nextTrack_) {
            bool wasLoading = lib_.loading();
            std::string tried = lib_.loadingTitle();
            nextTrack_ = lib_.takeReady();
            if (nextTrack_) std::printf("[app] next song ready: %s\n", nextTrack_->title.c_str());
            // A failed load (e.g. an unavailable video) just moves on to another song.
            if (!nextTrack_ && wasLoading && !lib_.loading()) {
                std::printf("[app] could not load %s, trying another song\n", tried.c_str());
                failedTitle_ = tried;
                failedToast_ = 4.f;
                if (mode_ == MODE_CAMPAIGN) { // the run needs its songs in order
                    toMenu(Menu::CAMPAIGN, "NO SONG", "COULD NOT LOAD " + asciiFold(tried), true);
                    swap();
                    return true;
                }
            }
            if (!nextTrack_ && !lib_.loading() && !lib_.empty()) lib_.prefetch(runSeed_ + (uint64_t)(runTime_ * 1000));
        }
        if (!track_ && nextTrack_) {
            // The first song comes in like every next one: its title card on the black, then its world, then the game.
            if (introAtStart_) {
                break_ = BREAK_IN;
                breakT_ = 0;
            }
            startTrack(std::move(nextTrack_));
        }
        if (breaks_ && track_) {
            // The break: the outro starts before the song's end (or at once for "next song"), the next song on its black.
            const double left = track_->duration() - songTime_;
            const bool lost = mode_ == MODE_CAMPAIGN && game_.over(); // a campaign game over goes back to the menu
            // (a lock that clears lines: once the rows above have fallen, so that the next song never opens on them falling)
            const bool rest = game_.settled();
            const bool outroDue = (left <= outLead() + OUT_EARLY && landed_ && rest) || (left <= outLead() - OUT_LATE && rest) ||
                                  left <= outLead() - OUT_LATE - OUT_SETTLE;
            if (rest) landed_ = false;
            if (break_ == BREAK_OFF && !lost && ((wantSkip_ && nextTrack_) || outroDue || songFinished())) {
                if (mode_ == MODE_CAMPAIGN && !wantSkip_) campaignSongSurvived();
                break_ = BREAK_OUT;
                breakT_ = 0;
                breakSkip_ = wantSkip_ || songFinished();
                wantSkip_ = false;
                skipToast_ = 0;
                std::printf("[app] break: outro at %.2f s\n", songTime_);
            }
            const bool outroDone = breakSkip_ ? breakT_ >= OUT_HIDE + 0.4f + 1.2f : songFinished() && breakT_ >= OUT_HIDE + 1.f;
            if (break_ == BREAK_OUT && outroDone && mode_ == MODE_CAMPAIGN && run_.finishing) {
                campaignEnd();
                swap();
                return true;
            }
            if (break_ == BREAK_OUT && outroDone && nextTrack_) {
                break_ = BREAK_IN;
                breakT_ = 0;
                startTrack(std::move(nextTrack_));
                std::printf("[app] break: intro\n");
            }
        } else if (mode_ == MODE_CAMPAIGN && track_ && songFinished() && !game_.over()) { // no breaks (REFALL_NO_BREAK)
            campaignSongSurvived();
            if (run_.finishing) { campaignEnd(); swap(); return true; }
            if (nextTrack_) startTrack(std::move(nextTrack_));
        } else if (track_ && nextTrack_ && (songFinished() || wantSkip_)) {
            wantSkip_ = false;
            skipToast_ = 0;
            startTrack(std::move(nextTrack_));
        }
        if (clipMode && track_) {
            if (clipStart_ < 0) {
                const double drop = opt_.clipBreak ? track_->duration() - 9.0 : opt_.clipAt >= 0.f ? (double)opt_.clipAt : firstDropTime();
                clipStart_ = std::max(0.0, drop - opt_.clipBefore);
                simSongTime_ = std::max(0.0, clipStart_ - warmup()); // warm-up: the board fills, the scene settles
                // Test hook: REFALL_CLIP_FROM_START=1 plays the game from the song's start, unseen until the warm-up, so
                // that every clip of one song (same seed, same hooks) shows the same game at the same moment: a
                // continuous game across the cuts of a trailer (tools/demo.py)
                if (std::getenv("REFALL_CLIP_FROM_START")) simSongTime_ = 0.0;
                // Test hook: REFALL_CLIP_FILL=1 starts the warm-up on a played-in stack (the review shots' filled board),
                // for clips early in a song, where the warm-up is too short to fill it (tools/demo.py).
                if (std::getenv("REFALL_CLIP_FILL") && std::getenv("REFALL_CLIP_FILL")[0] == '1') game_.debugFillBoard(runSeed_);
                std::printf("[clip] moment at %.1f s: recording %.1f s .. %.1f s\n", drop, clipStart_,
                            clipStart_ + opt_.clipBefore + opt_.clipAfter);
            }
            simSongTime_ += dt; // real song time: the recorded audio is cut from the same span
            songTime_ = simSongTime_;
            preroll_ = simSongTime_ < clipStart_ - warmup();
            // Test hook: REFALL_TEST_BONUS=1 also starts the slow-motion bonus REFALL_TEST_BONUS_AT s into a clip (default 3)
            static const char* tbc = std::getenv("REFALL_TEST_BONUS");
            static const double tbAt = std::getenv("REFALL_TEST_BONUS_AT") ? std::atof(std::getenv("REFALL_TEST_BONUS_AT")) : 3.0;
            static bool tbDone = false;
            if (tbc && tbc[0] == '1' && !tbDone && simSongTime_ >= clipStart_ + tbAt) {
                tbDone = true;
                game_.debugChargeBonus();
                game_.activateBonus();
            }
        } else if (shotMode && track_) {
            if (opt_.phaseShots) {
                // Settle for 12 s ending 2 s before the next phase change (or the song end).
                int k = std::min(shotsTaken, (int)plan_.phases.size() - 1);
                double end = k + 1 < (int)plan_.phases.size() ? plan_.phases[k + 1].start : track_->duration();
                if (simSongTime_ < end - 14.0) simSongTime_ = std::max(plan_.phases[k].start, end - 14.0);
            } else {
                if (reviewMode && reviewPhase_ < 0) pickReviewTime();
                if (simSongTime_ < opt_.shotTime - warmup()) simSongTime_ = std::max(0.0, opt_.shotTime - warmup());
                // Test hook: REFALL_TEST_BONUS=1 starts the slow-motion bonus 4 s before the shot (its look settled),
                // REFALL_TEST_BONUS=0 keeps it off (autoplay won't start it)
                static const char* tb = std::getenv("REFALL_TEST_BONUS");
                if (tb && tb[0] == '1' && simSongTime_ >= opt_.shotTime - 4.0 && !game_.bonusActive()) {
                    game_.debugChargeBonus();
                    game_.activateBonus();
                }
            }
            simSongTime_ += dt * audio_.currentSpeed();
            songTime_ = simSongTime_;
        } else if (testPlay() || (viewer_ && offline_)) {
            if (track_ && !paused_ && !songHeld_) simSongTime_ += dt;
            songTime_ = simSongTime_;
        } else {
            songTime_ = audio_.smoothPosition(); // (the raw position moves by whole audio blocks: an uneven fall)
        }

        // ---- Global controls.
        // Until the first song is ready the game waits, paused (no play without music).
        const bool waiting = !track_;
        // Pause: a menu (continue / controls / quit), with the arrows or the gamepad; Esc / Start / B resume (back
        // from the controls page), Q quits at once.
        bool quitNow = false;
        ctrlNoteT_ = std::max(0.f, ctrlNoteT_ - dt);
        if (paused_ && pauseControls_) {
            if (input_.remapDone()) { ctrlNote_ = "GAMEPAD REMAPPED"; ctrlNoteT_ = 4.f; }
            if (!remapKeys()) { // (a remap takes the keys)
                if (input_.pressed(A_UI_UP)) ctrlSel_ = (ctrlSel_ + 2) % 3;
                if (input_.pressed(A_UI_DOWN)) ctrlSel_ = (ctrlSel_ + 1) % 3;
                if (input_.pressed(A_PAUSE) || input_.pressed(A_UI_BACK)) pauseControls_ = false;
                else if (input_.pressed(A_UI_OK)) {
                    if (ctrlSel_ == 0) input_.startRemap();
                    else if (ctrlSel_ == 1) { input_.resetMapping(); ctrlNote_ = "MAPPING RESTORED"; ctrlNoteT_ = 4.f; }
                    else pauseControls_ = false;
                }
            }
        } else if (paused_) {
            if (input_.pressed(A_UI_UP)) pauseSel_ = (pauseSel_ + 2) % 3;
            if (input_.pressed(A_UI_DOWN)) pauseSel_ = (pauseSel_ + 1) % 3;
            if (input_.pressed(A_PAUSE) || input_.pressed(A_UI_BACK)) {
                paused_ = false;
            } else if (input_.pressed(A_UI_OK)) {
                if (pauseSel_ == 0) paused_ = false;
                else if (pauseSel_ == 1) { pauseControls_ = true; ctrlSel_ = 2; ctrlNoteT_ = 0; }
                else quitNow = true;
            }
            if (input_.pressed(A_QUIT)) quitNow = true;
            if (!paused_) audio_.setPaused(songHeld_);
        } else if (input_.pressed(A_PAUSE) && !game_.over() && !waiting) {
            paused_ = true;
            pauseSel_ = 0;
            pauseControls_ = false;
            audio_.setPaused(true);
        }
        if (quitNow) {
            if (menuEnabled_) {
                // (the songs survived may have done the stage: its clear and unlocks play on the wheel)
                if (runStage()) campaignOutcome(run_.survived ? game_.score() : run_.songStartScore, run_.survived ? game_.lines() : run_.songStartLines);
                toMenu(mode_ == MODE_CAMPAIGN ? Menu::CAMPAIGN : Menu::FREE);
                swap();
                return true;
            }
            quit_ = true;
        }
        // still loading the first song: Esc goes back to the menu
        if (waiting && menuEnabled_ && input_.pressed(A_UI_BACK)) { toMenu(mode_ == MODE_CAMPAIGN ? Menu::CAMPAIGN : Menu::FREE); swap(); return true; }
        if (input_.pressed(A_FULLSCREEN)) toggleFullscreen();
        if (viewer_) viewerFrame(dt); // (its own keys: the game's shortcuts below are off)
        // Test hook: REFALL_TEST_SKIP=<seconds> presses "next song" periodically.
        static const double testSkip = std::getenv("REFALL_TEST_SKIP") ? std::atof(std::getenv("REFALL_TEST_SKIP")) : 0.0;
        const bool testSkipNow = testSkip > 0 && std::fmod(runTime_, testSkip) < dt && runTime_ > dt;
        if (testSkipNow) std::printf("[test] skip pressed (next ready: %s)\n", nextTrack_ ? "yes" : "no");
        if (input_.pressed(A_NEXT_SONG) && !testSkipNow)
            std::printf("[input] next song pressed (%zu songs, next %s)\n", lib_.size(),
                        nextTrack_ ? ("ready: " + nextTrack_->title).c_str() : ("loading: " + lib_.loadingTitle()).c_str());
        // (not on a game over: Enter, "next song", is one of the keys that start a new game there)
        if ((input_.pressed(A_NEXT_SONG) || testSkipNow) && lib_.size() > 1 && mode_ != MODE_CAMPAIGN && !viewer_ && !game_.over()) {
            wantSkip_ = true;
            skipToast_ = 2.5f;
        }
        if (input_.pressed(A_NEW_SCENE) && mode_ != MODE_CAMPAIGN && !viewer_) newScene(false); // the Voyage keeps its universes
        // Ctrl+Shift+Left/Right: jump 10 s in the song (repeats while held) to test sections quickly.
        {
            int dir = input_.repeatSteps(A_SEEK_FWD) - input_.repeatSteps(A_SEEK_BACK);
            if (dir != 0 && track_ && !audio_.finished() && (mode_ != MODE_CAMPAIGN || opt_.cheatSkip) && !viewer_) { // (in a campaign: only with --cheat-skip)
                seekSong(std::clamp(audio_.position() + 10.0 * dir, 0.0, track_->duration() - 1.0));
                seekToast_ = 1.5f;
            }
        }
        if (input_.pressed(A_DEBUG_LEVEL) && !game_.over() && !viewer_) game_.skipToNextLevel();
        if (input_.pressed(A_DEBUG_BONUS) && mode_ != MODE_CAMPAIGN && !viewer_) game_.debugChargeBonus();
        // --cheat-skip: S jumps to the song's last seconds, survived: its outro starts as at a real end (a Voyage song
        // counts as cleared, the stage and the run go on).
        if (opt_.cheatSkip && input_.pressed(A_CHEAT_SKIP) && track_ && break_ == BREAK_OFF && !game_.over() && !paused_ && !viewer_) {
            const double end = track_->duration() - (OUT_HIDE + 2.0);
            if (songTime_ < end) {
                viewerSeek(end);
                std::printf("[cheat] song skipped to %.1f s\n", end);
            }
        }

        // ---- Game.
        if (!paused_ && !waiting && gameFrozen()) {
            breakT_ += dt;
            if (break_ == BREAK_IN) {
                trackAge_ = 1.f; // the corner title stays lit through the intro
                sceneNameTimer_ = 8.f;
            } else {
                trackAge_ += dt;
            }
        } else if (!paused_ && !waiting) {
            trackAge_ += dt;
            if (opt_.autoplay || shotMode) {
                // Test hook: REFALL_TEST_OVER=T hard-drops every piece from T seconds into a shot's settle (a game
                // over in screenshots, to check each universe's top out).
                static const float overAt = std::getenv("REFALL_TEST_OVER") ? (float)std::atof(std::getenv("REFALL_TEST_OVER")) : -1.f;
                if (shotMode && overAt >= 0.f && shotSettle >= overAt && !game_.over()) {
                    if (game_.hasPiece()) game_.hardDrop();
                } else if (!reviewFilled_) autoplay(dt); // a review's filled board is left as it is
            } else {
                for (int i = 0; i < input_.repeatSteps(A_LEFT); i++) game_.move(-1);
                for (int i = 0; i < input_.repeatSteps(A_RIGHT); i++) game_.move(1);
                // (the bonus: down pressed again on a piece already resting sets it down; held, it only brings it there)
                const bool rested = game_.bonusActive() && game_.grounded();
                for (int i = 0; i < input_.repeatSteps(A_SOFT); i++) game_.softDrop();
                if (rested && input_.pressed(A_SOFT)) game_.settle();
                if (input_.pressed(A_CW)) game_.rotate(1);
                if (input_.pressed(A_CCW)) game_.rotate(-1);
                if (input_.pressed(A_HOLD)) game_.hold();
                if (input_.pressed(A_HARD)) game_.hardDrop();
                if (input_.pressed(A_BONUS)) game_.activateBonus();
            }
            if (game_.over() && viewer_) { // the review goes on: a new board at once
                game_.restart();
                overTime_ = 0;
            } else if (game_.over() && mode_ == MODE_CAMPAIGN && !shotMode) { // (offline renders play on)
                // A campaign game over: the board dissolves and the game darkens (the song muffled), then the frame is
                // kept and the debrief comes over it (see after the drawing); a key there goes back to the Voyage.
                const bool was = overTime_ > 1.8f;
                overTime_ += dt;
                if (!was && overTime_ > 1.8f) debriefPending_ = true;
            } else if (game_.over()) {
                // Game over: the board dissolves; from the menu it holds until the player presses a key, then back to
                // the menu; without the menu (songs on the command line) a key starts a new game.
                overTime_ += dt;
                if (menuEnabled_ && overTime_ > 2.5f && anyKeyPressed()) { toMenu(Menu::FREE); swap(); return true; }
                const bool any = anyKeyPressed() || input_.pressed(A_NEXT_SONG); // (Enter too)
                if ((any && overTime_ > 2.5f) || ((opt_.autoplay || shotMode) && overTime_ > 4.f)) {
                    game_.restart();
                    overTime_ = 0;
                }
            } else {
                updateGravity(songTime_, dt);
            }
            game_.setHeldShift(opt_.autoplay || shotMode || paused_ || waiting ? 0 : input_.heldDirection());
            game_.update(dt);
            handleEvents();
            if (game_.level() > lastLevel_ && !game_.over() && mode_ != MODE_CAMPAIGN && !viewer_) {
                levelToast_ = 2.5f;
                R_.refresh(0.35f); // soft refresh wave
                R_.levelUp();
                std::printf("[app] level %d\n", game_.level());
            }
            lastLevel_ = game_.level();
            R_.setDim(game_.over() && overTime_ > 1.2f);
            // Slow motion: the song is muffled for the whole bonus (bass kept, highs attenuated); gravity's own
            // slow-down is in updateGravity. A game over sounds the same, quieter, for as long as it lasts.
            const bool over = game_.over() && !viewer_; // (the viewer's game overs are silent)
            const bool muffle = game_.bonusActive() || over;
            if (muffle != muffleWas_) {
                muffleWas_ = muffle;
                audio_.setMuffled(muffle, over ? 1.2f : 0.6f);
            }
            // after a game over the muffled song fades out to silence (from 2.5 s on, over 3 s)
            audio_.setSongGain(!over ? 1.f : overTime_ < 2.5f ? 0.45f : 0.f, !over ? 0.6f : overTime_ < 2.5f ? 1.5f : 3.f);
            if (game_.bonusActive() != slowWas_) {
                slowWas_ = game_.bonusActive();
                // The screen's colors are reversed for the whole bonus (a front sweeps it in and out).
                R_.setBonus(slowWas_);
            }
        }

        // Test hook: REFALL_DEBUG_FALL=<file> logs each frame (wall time, song time, the audio's raw position,
        // gravity steps, the piece's row and its drawn row) to look at the fall's regularity
        static FILE* fallLog = std::getenv("REFALL_DEBUG_FALL") ? std::fopen(std::getenv("REFALL_DEBUG_FALL"), "w") : nullptr;
        if (fallLog) {
            static const auto t0 = std::chrono::steady_clock::now();
            std::fprintf(fallLog, "%.6f %.6f %.6f %d %d %.4f %d %.3f\n",
                         std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), songTime_,
                         audio_.position(), fallSteps_, game_.hasPiece() ? game_.piece().y : -99, game_.pieceVisualY(),
                         paused_ ? 1 : 0, audio_.currentSpeed());
        }
        fallSteps_ = 0;
        // (REFALL_DEBUG_FALL_EVENTS="P10,B15,S20": at those song times a 1.5 s pause, the bonus, a 10 s jump ahead)
        static std::string fallEvents = std::getenv("REFALL_DEBUG_FALL_EVENTS") ? std::getenv("REFALL_DEBUG_FALL_EVENTS") : "";
        static double resumeAt = -1;
        {
            const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            if (resumeAt > 0 && wall >= resumeAt) { resumeAt = -1; paused_ = false; audio_.setPaused(false); std::printf("[debug] resume at %.3f\n", songTime_); }
            if (!fallEvents.empty() && track_ && !paused_) {
                const char c = fallEvents[0];
                const double at = std::atof(fallEvents.c_str() + 1);
                if (songTime_ >= at) {
                    const size_t comma = fallEvents.find(',');
                    fallEvents = comma == std::string::npos ? "" : fallEvents.substr(comma + 1);
                    std::printf("[debug] %c at %.3f\n", c, songTime_);
                    if (c == 'P') { paused_ = true; audio_.setPaused(true); resumeAt = wall + 1.5; }
                    else if (c == 'B') { game_.debugChargeBonus(); game_.activateBonus(); }
                    else if (c == 'S') seekSong(songTime_ + 10.0);
                }
            }
        }
        // (REFALL_DEBUG_FPS=N: frames paced at N per second, a vsync for a hidden window)
        static const double debugFps = std::getenv("REFALL_DEBUG_FPS") ? std::atof(std::getenv("REFALL_DEBUG_FPS")) : 0.0;
        if (debugFps > 0) {
            static auto next = std::chrono::steady_clock::now();
            next += std::chrono::microseconds((long long)(1e6 / debugFps));
            const auto now = std::chrono::steady_clock::now();
            if (next < now - std::chrono::milliseconds(50)) next = now;
            std::this_thread::sleep_until(next);
        }
        // ---- Music-driven state.
        if (track_) {
            const Analysis& an = track_->analysis;
            double t = songTime_ - 0.03;
            // Visuals only follow slow envelopes (~1 s): no per-beat hits, no transient flicker.
            FrameFeatures f = an.at(t);
            const float rate = 1.2f;
            for (int b = 0; b < NUM_BANDS; b++) music_.audio.bands[b] = approach(music_.audio.bands[b], f.bands[b], rate, dt);
            music_.audio.bass = approach(music_.audio.bass, f.bass, rate, dt);
            music_.audio.mid = approach(music_.audio.mid, f.mid, rate, dt);
            music_.audio.high = approach(music_.audio.high, f.high, rate, dt);
            music_.audio.loud = approach(music_.audio.loud, f.loud, rate, dt);
            music_.audio.onset = 0;
            music_.intensity = approach(music_.intensity, an.intensityAt(t), 0.5f, dt);
            // Slow breathing over two bars (8 beats) instead of a pulse on every beat.
            double bp = an.beatPosition(t);
            music_.beatPhase = (float)(bp - std::floor(bp));
            music_.beat = bp;
            music_.bpm = an.fp.bpm;
            music_.hasSong = !songFinished();
            float breath = 0.5f - 0.5f * std::cos(TAU * (float)std::fmod(bp / 8.0, 1.0));
            // Beat pulses only inside rare accent windows, as a soft swell (no hard hits).
            // Half time: whole-scene pulses come every two beats (on every beat they are tiring to watch).
            const double bp2 = bp / 2.0;
            const float phase2 = (float)(bp2 - std::floor(bp2));
            float swell = 0.5f + 0.5f * std::cos(TAU * phase2);
            // Pulses only inside pulse zones (peak sections, drops); stronger for faster songs.
            const float tempoAmp = pulseTempoAmp(an.fp.bpm);
            const float zone = plan_.pulseEnvelope(t);
            // Every pulse sits on the beat grid: the bass and the detected onsets only set how strong the pulses
            // are, smoothed over a few beats. (Onsets fire on syncopations, miss beats and double up: driving the
            // pulses from them directly made them irregular, which is unsettling to watch.)
            const float bassSlow = music_.audio.bass;
            float accent = zone * tempoAmp * swell * swell * (0.4f + 0.6f * bassSlow);
            // Overall energy: faster songs, intense sections and higher levels all push it up.
            const int lvl = plan_.phases[plan_.phaseAt(t)].level;
            music_.energy = approach(music_.energy,
                                     saturate((0.3f + 0.7f * smoothstepf(80.f, 135.f, an.fp.bpm)) *
                                              (0.45f + 0.55f * music_.intensity) * (1.f + 0.25f * game_.difficulty())),
                                     1.f, dt);
            const float E = music_.energy;
            // Outside pulse zones, energetic songs still pulse on the beat in mid/peak parts.
            float base = (lvl >= 1 ? 0.5f : 0.15f) * E * smoothstepf(95.f, 130.f, an.fp.bpm);
            float groove = base * swell * swell * swell * (0.5f + 0.5f * bassSlow);
            // The hit: a quick swell every two beats, decaying (~0.35 s), the bar's downbeat a little stronger.
            const float beatDur = 60.f / std::max(40.f, (float)an.fp.bpm);
            const float sinceBeat = phase2 * 2.f * beatDur;
            const bool downbeat = (((long long)std::floor(bp)) % 4 + 4) % 4 < 2;
            const float beatHit = std::exp(-sinceBeat * 3.f) * (downbeat ? 1.f : 0.8f);
            const float onsetK = smoothstepf(0.45f, 0.85f, f.onset);
            float hitTarget = std::max(zone, lvl >= 1 ? 0.5f * E : 0.f) * smoothstepf(92.f, 120.f, an.fp.bpm) * onsetK;
            hit_ = std::max(approach(hit_, 0.f, 0.25f, dt), hitTarget);
            music_.beatPulse = 0.25f * breath + accent + groove + 0.7f * hit_ * beatHit;
            // The kick envelope: every two beats while the music has kicks (pushes the particle fields, surges).
            // (a peak held over a few beats: the onsets come and go within each beat)
            kickLevel_ = std::max(approach(kickLevel_, 0.f, 0.25f, dt), E * onsetK * (0.4f + 0.6f * f.bass));
            const float kickTarget = kickLevel_ * beatHit;
            music_.kick = kickTarget > music_.kick ? approach(music_.kick, kickTarget, 30.f, dt) : kickTarget;
            for (int b = 0; b < NUM_BANDS; b++)
                music_.bandsFast[b] = f.bands[b] > music_.bandsFast[b] ? approach(music_.bandsFast[b], f.bands[b], 25.f, dt)
                                                                       : approach(music_.bandsFast[b], f.bands[b], 6.f, dt);

            // Structure-driven profile (density, speed, glow, saturation), eased over a few seconds.
            {
                const StructureProfile prof = structureProfile(an, t + 1.2); // look ahead: reach the new section's feel on time
                const float ease = 1.0f; // ~1 s, smooth but on time
                music_.density = approach(music_.density, prof.density, ease, dt);
                music_.speed = approach(music_.speed, prof.speed, ease, dt);
                music_.glow = approach(music_.glow, prof.glow, ease, dt);
                music_.saturation = approach(music_.saturation, prof.saturation, ease, dt);
            }
            if (songFinished()) {
                music_.beatPulse = 0;
                music_.intensity = approach(music_.intensity, 0.2f, 0.5f, dt);
            }
            // The song is known in advance: start the ripple early so it crosses the board on the boundary.
            // Soft refresh: a new section inside the same scene (chorus -> chorus, verse -> verse) is marked
            // by a light wave and a small glow lift; colors and layout stay the same.
            // a DROP section (as the corner shows it), until a little before its end: what leaves with it is gone by then
            music_.inDrop = !an.segments.empty() && an.segments[an.segmentAt(songTime_)].kind == SEG_DROP &&
                            an.segments[an.segmentAt(songTime_ + DROP_OUT_LEAD)].kind == SEG_DROP;
            if (!an.segments.empty()) {
                int seg = an.segmentAt(songTime_ + 0.45);
                if (seg > lastSeg_) {
                    const Segment& sg = an.segments[seg];
                    const ScenePhase& ph = plan_.phases[plan_.phaseAt(sg.start + 0.01)];
                    bool phaseStart = std::fabs(ph.start - sg.start) < 0.05;
                    if (!phaseStart && sg.end - sg.start >= 8.0) {
                        float strength = (sg.kind == SEG_CHORUS || sg.kind == SEG_DROP) ? 0.5f : 0.3f;
                        R_.refresh(strength);
                        std::printf("[app] refresh (%s)\n", segmentName(sg.kind));
                    }
                    // A DROP section inside a scene that was already at its peak (after a loud build or chorus)
                    // lands too: its drop hit, on its first beat.
                    if (!phaseStart && sg.kind == SEG_DROP && ph.level >= 2) dropAt_ = sg.start;
                    lastSeg_ = seg;
                } else if (seg < lastSeg_) {
                    lastSeg_ = seg; // seeked back
                }
            }
            // Scene changes are anticipated so the wave crosses the board on the boundary. Rising changes
            // (into mid, and above all into a peak/drop) are quick and marked by a ring and a glow swell;
            // falling ones (into a break or calm part) are slow and soft.
            {
                int cur = std::clamp(lastPhase_, 0, (int)plan_.phases.size() - 1);
                int nxt = plan_.phaseAt(songTime_ + 1.5);
                if (nxt != cur) {
                    const ScenePhase& to = plan_.phases[nxt];
                    bool rising = to.level > plan_.phases[cur].level;
                    double lead = rising ? 0.45 : 1.2;
                    if (songTime_ + lead >= to.start || nxt < cur) {
                        lastPhase_ = nxt;
                        float impact = rising ? (to.level >= 2 ? 1.f : 0.5f) : 0.f;
                        R_.setLevel(to.level, to.energy, rising ? 1.8f : 4.5f, impact);
                        // (a peak that starts on a loud BUILD has no drop hit: its DROP section brings it, see above)
                        dropAt_ = rising && to.level >= 2 && an.segments[to.seg].kind != SEG_BUILD ? to.start : -1.0;
                        // Optional (--universe-on-peak): a long song may also move to another universe when it
                        // rises into a peak, after a minute in the current one.
                        if (opt_.universeOnPeak && rising && to.level >= 2 && universeAge_ > 60.f && forcedUniverse_ < 0 &&
                            opt_.shotPrefix.empty() && opt_.clipPath.empty()) {
                            sceneCounter_++;
                            look_.seed = splitmix64(look_.seed ^ sceneCounter_);
                            changeUniverse(universes::pick(look_, look_.seed, R_.universe(), recentUniverses_), -1, transitions::PHASE_PEAK, false);
                        }
                        lastSeg_ = to.seg;
                        const Segment& sg = an.segments[to.seg];
                        std::printf("[app] scene level %d (%s, energy %.2f)%s\n", to.level, segmentName(sg.kind),
                                    sg.energy, rising ? " rising" : "");
                    }
                }
                // The drop hit, on the peak's first downbeat (a seek past it or back cancels it).
                if (dropAt_ >= 0 && songTime_ >= dropAt_) {
                    if (songTime_ - dropAt_ < 0.25) {
                        R_.drop(1.f);
                        std::printf("[app] drop\n");
                    }
                    dropAt_ = -1;
                }
            }
        } else {
            music_.intensity = 0.2f;
            music_.beatPhase = (float)std::fmod(runTime_, 1.0);
            music_.beatPulse = 0;
            music_.beat = runTime_ * 2.0;
            music_.hasSong = false;
            music_.inDrop = false;
        }
        universeAge_ += dt;

        fbSize(fbw, fbh);
        static int lastW = 0, lastH = 0;
        if ((fbw != lastW || fbh != lastH) && fbw > 0 && fbh > 0) {
            R_.resize(fbw, fbh);
            lastW = fbw;
            lastH = fbh;
        }
        fade = std::min(1.f, fade + dt * 0.8f);
        if (fbw > 0 && fbh > 0) {
            std::vector<HudText> hud = buildHud(dt);
            updateBreak(dt, hud);
            // REFALL_FRAME_STATS=1 (or slow): each frame's cost (the game's update, the drawing calls, the GPU), printed at the end
            static const bool stats = std::getenv("REFALL_FRAME_STATS") != nullptr;
            static GLuint statQ = 0;
            if (stats && !statQ) glGenQueries(1, &statQ);
            const auto r0 = std::chrono::steady_clock::now();
            if (stats) glBeginQuery(GL_TIME_ELAPSED, statQ);
            // Test hook: REFALL_TEST_SPAWN=1 draws a review's piece where it spawns (its top row in the rows above the well)
            static const bool spawnTest = std::getenv("REFALL_TEST_SPAWN") != nullptr;
            if (spawnTest && reviewFilled_) game_.respawn();
            if (!preroll_ && !held_) R_.render(game_, music_, runTime_, dt, paused_ || waiting, hud, shotMode ? 1.f : fade);
            if (stats) {
                glEndQuery(GL_TIME_ELAPSED);
                const double cpu = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - r0).count();
                GLuint64 ns = 0;
                glGetQueryObjectui64v(statQ, GL_QUERY_RESULT, &ns);
                const double upd = std::chrono::duration<double, std::milli>(r0 - frameStart).count();
                statFrames_++; statUpd_ += upd; statCpu_ += cpu; statGpu_ += ns / 1e6;
                statCpuMax_ = std::max(statCpuMax_, cpu); statGpuMax_ = std::max(statGpuMax_, ns / 1e6);
                static const bool slow = std::string(std::getenv("REFALL_FRAME_STATS")) == "slow"; // =slow: each frame over 10 ms
                if (slow && std::max(cpu, ns / 1e6) > 10.0)
                    std::printf("[stats] slow frame at %.2f s (song %.2f s): drawing calls %.1f ms, GPU %.1f ms (%s%s)\n", runTime_,
                                songTime_, cpu, ns / 1e6, R_.label().c_str(), R_.changing() ? ", changing" : "");
            }
            if (vv_) vv_->draw(fbw, fbh, viewerNow());
        }
        if (viewer_ && !opt_.viewShot.empty()) { // --shot: the viewer, a few seconds into the first song
            if (viewShotT_ >= opt_.viewShotAfter || runTime_ > 300.f) {
                const bool ok = R_.screenshot(opt_.viewShot);
                std::printf("[shot] %s %s\n", ok ? "wrote" : "FAILED", opt_.viewShot.c_str());
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // in real time: the loads keep up
        }

        if (!testShot_.empty()) { R_.screenshot(testShot_); std::printf("[test] shot %s\n", testShot_.c_str()); testShot_.clear(); }
        if (debriefPending_) { // this frame stays behind the debrief
            debriefPending_ = false;
            if (fbw > 0 && fbh > 0) menu_.captureBackdrop(fbw, fbh);
            gameOverDebrief();
            swap();
            return true;
        }
        static long clipFrames = -1; // frames recorded (the song time restarts with the next song)
        if (clipMode && track_ && clipStart_ >= 0 && (clipFrames >= 0 || songTime_ >= clipStart_)) {
            clipFrames++;
            int cw, ch;
            R_.readFrame(clipPx, cw, ch);
            if (!clipPipe) {
                std::string ff = std::getenv("REFALL_FFMPEG") ? std::getenv("REFALL_FFMPEG") : platform::findExecutable("ffmpeg");
                if (ff.empty()) { std::fprintf(stderr, "[clip] ffmpeg not found\n"); return false; }
                char spec[160];
                std::snprintf(spec, sizeof(spec), " -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r 60 -i - -ss %.3f -t %.3f -i ",
                              cw, ch, clipStart_, opt_.clipBefore + opt_.clipAfter);
                std::string cmd = platform::quoteArg(ff) + spec + platform::quoteArg(track_->path) +
                                  " -map 0:v -map 1:a -vf vflip " +
                                  // on the GPU's encoder when there is one, else x264 on two threads (a light load)
                                  std::string(headless_ && headless::nvidia() ? "-c:v h264_nvenc -preset p5 -rc vbr -cq 19 -b:v 0"
                                                                              : "-c:v libx264 -threads 2 -preset medium -crf 18") +
                                  " -pix_fmt yuv420p -c:a aac -b:a 192k -af apad -shortest " + platform::quoteArg(opt_.clipPath);
                clipPipe = platform::openWrite(cmd);
                if (!clipPipe) { std::fprintf(stderr, "[clip] could not start ffmpeg\n"); return false; }
            }
            std::fwrite(clipPx.data(), 1, clipPx.size(), clipPipe);
            if (clipFrames >= (long)((opt_.clipBefore + opt_.clipAfter) * 60.f)) {
                const int rc = platform::closeRead(clipPipe);
                clipPipe = nullptr;
                std::printf("[clip] %s %s (%s)\n", rc == 0 ? "wrote" : "FAILED", opt_.clipPath.c_str(), R_.label().c_str());
                return false;
            }
        }
        if (shotMode && !clipMode && track_) {
            // Test hook: REFALL_FX=slow:N, quad:N or drop plays that special animation REFALL_FX_AT seconds before each shot.
            static const char* fxEnv = std::getenv("REFALL_FX");
            static const float fxAt = std::getenv("REFALL_FX_AT") ? (float)std::atof(std::getenv("REFALL_FX_AT")) : 6.f;
            if (fxEnv && shotSettle < 12.f - fxAt && shotSettle + dt >= 12.f - fxAt) {
                int idx = std::atoi(std::strchr(fxEnv, ':') ? std::strchr(fxEnv, ':') + 1 : "0") + shotsTaken;
                if (std::strncmp(fxEnv, "drop", 4) == 0) {
                    R_.drop(1.f);
                } else if (std::strncmp(fxEnv, "slow", 4) == 0) {
                    R_.forceFx(idx, -1);
                    game_.debugChargeBonus();
                } else {
                    R_.forceFx(-1, idx);
                    GameEvent ev{GameEvent::Clear};
                    ev.count = 4;
                    for (int y = 18; y < 22; y++)
                        for (int x = 0; x < Game::W; x++) ev.cells.push_back({x, y, (x + y) % 7});
                    ev.points = 1200;
                    quadToast_ = 2.4f;
                    quadPoints_ = 1200;
                    R_.onEvent(ev, game_);
                }
            }
            // --transition-shots: a filled board, then a change of universe at the shot time, N frames across it.
            if (!opt_.transitionShots.empty()) {
                static int tShots = -1; // -1: not triggered yet
                if (!reviewFilled_ && shotSettle > 10.5f) {
                    game_.debugFillBoard(runSeed_);
                    reviewFilled_ = true;
                }
                shotSettle += dt;
                if (tShots < 0 && shotSettle > 12.f) {
                    int variant = -1;
                    int to = opt_.toUniverse.empty() ? universes::next(R_.universe(), true) : universes::parse(opt_.toUniverse, &variant);
                    if (to < 0) { std::fprintf(stderr, "[app] unknown universe '%s'\n", opt_.toUniverse.c_str()); return false; }
                    SongLook l = look_;
                    l.variant = variant;
                    if (track_) l.remaining = (float)std::max(0.0, track_->duration() - songTime_);
                    Renderer::Change c;
                    c.transition = opt_.transition.empty() ? -1 : transitions::find(opt_.transition);
                    if (!opt_.transition.empty() && c.transition < 0)
                        std::fprintf(stderr, "[app] unknown transition '%s'\n", opt_.transition.c_str());
                    c.why = transitions::SONG_CHANGE;
                    R_.changeUniverse(to, l, phaseLevel(plan_.phaseAt(songTime_)), phaseEnergy(plan_.phaseAt(songTime_)), c);
                    tShots = 0;
                } else if (tShots >= 0) {
                    const int N = opt_.transitionShotCount;
                    const float p = R_.changeProgress();
                    const float want = N == 1 ? 0.5f : 0.03f + 0.94f * tShots / (N - 1);
                    if (!R_.changing() || p >= want) {
                        char name[512];
                        std::snprintf(name, sizeof(name), "%s%02d.png", opt_.transitionShots.c_str(), tShots);
                        R_.screenshot(name);
                        std::printf("[shot] %s  <- %s at %.2f (%s)\n", name, R_.transitionName(), p, R_.label().c_str());
                        if (++tShots >= N || !R_.changing()) return false;
                    }
                }
                swap();
                return true;
            }
            if (reviewMode && !reviewFilled_ && shotSettle > warmup() - 1.5) {
                game_.debugFillBoard(runSeed_);
                reviewFilled_ = true;
            }
            shotSettle += dt;
            if (shotSettle > warmup()) {
                char name[512];
                std::snprintf(name, sizeof(name), "%s%02d.png", opt_.shotPrefix.c_str(), shotsTaken);
                if (reviewMode) std::snprintf(name, sizeof(name), "%s", opt_.reviewPath.c_str());
                R_.screenshot(name);
                if (reviewMode) printReview();
                std::printf("[shot] %s  <- %s\n", name, R_.label().c_str());
                shotsTaken++;
                shotSettle = 0;
                if (shotsTaken >= opt_.shotCount) return false;
                if (opt_.phaseShots) {
                    if (shotsTaken >= (int)plan_.phases.size()) return false;
                    swap();
                    return true;
                }
                newScene(true);
                if (shotsTaken % 2 == 0 && nextTrack_) startTrack(std::move(nextTrack_));
                simSongTime_ = 0;
            }
        }
        swap();
        // Debug: REFALL_FPS=1 prints the average frame time every 2 s.
        static const bool showFps = std::getenv("REFALL_FPS") != nullptr;
        if (showFps) {
            static double acc = 0, t0 = glfwGetTime();
            static int frames = 0;
            frames++;
            double tn = glfwGetTime();
            if (tn - t0 > 2.0) {
                std::printf("[fps] %.1f fps (%.2f ms/frame)\n", frames / (tn - t0), 1000.0 * (tn - t0) / frames);
                frames = 0;
                t0 = tn;
            }
            (void)acc;
        }
    }
    return true;
}

// The end of the run (the loop over): the frames' stats, the menu's place kept, the window closed.
int App::finish() {
    if (statFrames_ > 0)
        std::printf("[stats] %s: %ld frames, update %.2f ms, drawing calls %.2f ms (max %.1f), GPU %.2f ms (max %.1f)\n",
                    R_.label().c_str(), statFrames_, statUpd_ / statFrames_, statCpu_ / statFrames_, statCpuMax_,
                    statGpu_ / statFrames_, statGpuMax_);
    if (mode_ == MODE_MENU) keepPlace(true);
    std::printf("[app] end of the run (quit %d, %.1f s)\n", (int)quit_, runTime_);
    audio_.shutdown();
    if (headless_) headless::shutdown();
    else {
        glfwDestroyWindow(win_);
        glfwTerminate();
    }
    return 0;
}

// The menu's music file: next to the campaign data in the sources, else next to the executable (installed builds).
static std::string menuMusicPath() {
    std::vector<fs::path> c;
#ifdef REFALL_SOURCE_DIR
    c.push_back(fs::path(REFALL_SOURCE_DIR) / "data" / "menu_music.mp3");
#endif
    c.push_back(platform::executableDir() / "menu_music.mp3");
    c.push_back(platform::executableDir() / "data" / "menu_music.mp3");
    std::error_code ec;
    for (const fs::path& p : c)
        if (fs::exists(p, ec)) return p.string();
#ifdef REFALL_STANDALONE
    { // the standalone build carries it: written out once into the cache, where the decoder reads it
        static const unsigned char MP3[] = {
#include "standalone/menu_music.mp3.inc"
        };
        const fs::path p = platform::appCacheDir() / "menu_music.mp3";
        if (!fs::exists(p, ec) || fs::file_size(p, ec) != sizeof(MP3)) {
            fs::create_directories(p.parent_path(), ec);
            std::ofstream(p, std::ios::binary).write((const char*)MP3, sizeof(MP3));
        }
        if (fs::exists(p, ec)) return p.string();
    }
#endif
    return "";
}

void App::startMenuMusic() {
    const std::string path = menuMusicPath();
    if (path.empty()) { std::printf("[menu music] no menu_music.mp3 found: the menu stays silent\n"); return; }
    const uint32_t sr = audio_.sampleRate();
    musicLoad_ = std::async(std::launch::async, [path, sr]() { return decodeAudio16(path, sr); });
}

// The menu's music follows the screen: playing on the menu (0.7, a 1.5 s fade in), faded out (0.8 s) when a game
// starts; back on the menu after a game it starts over from its beginning, once a stage's celebration is over.
void App::updateMenuMusic() {
    if (musicLoad_.valid() && musicLoad_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto pcm = musicLoad_.get();
        if (pcm) {
            std::printf("[menu music] loaded (%.0f s)\n", pcm->size() / 2.0 / audio_.sampleRate());
            audio_.setBed(std::move(pcm));
            musicReady_ = true;
            musicState_ = 0;
        } else {
            std::printf("[menu music] could not decode it: the menu stays silent\n");
        }
    }
    // switched on in the options while it was never loaded (off at the start)
    static bool started = false; // (once: a missing file isn't looked for again each frame)
    if (musicLoad_.valid() || musicReady_) started = true;
    if (!started && menuEnabled_ && opt_.menuMusic && menu_.music() && mode_ == MODE_MENU) {
        started = true;
        startMenuMusic();
    }
    if (!musicReady_) return;
    // (silent on a run's debrief, while a cleared stage's petal turns and the next one unlocks, and when switched off)
    const int want = mode_ != MODE_MENU || menu_.screen() == Menu::DEBRIEF || menu_.celebrating() || !menu_.music() ? 0 : 1;
    if (want == musicState_) return;
    static const char* NAMES[3] = {"faded out", "playing", "ducked"};
    if (want == 1 && musicState_ == 0 && audio_.bedGain() <= 0.f) audio_.rewindBed(); // back from a game: from the top
    const float gain = want == 1 ? MENU_MUSIC_GAIN : want == 2 ? MENU_MUSIC_DUCKED : 0.f;
    const float glide = want == 1 ? (musicState_ == 2 ? 1.0f : 1.5f) : want == 2 ? 0.6f : 0.8f;
    audio_.setBedGain(gain, glide);
    std::printf("[menu music] %s (gain %.2f over %.1f s, at %.1f s of the loop, %.1f s in)\n", NAMES[want], gain, glide,
                audio_.bedPosition(), runTime_);
    musicState_ = want;
}

// The menu's place on the save slot in use; the options it keeps (the menu's music and theme) are slot 1's file's,
// the same on every slot. False when none was kept (the game's first launch).
bool App::loadPlace(Menu::Place& pl) const {
    bool known = Menu::loadPlace(placePath_, pl);
    Menu::Place g;
    if (slots_.active() > 0 && Menu::loadPlace(slots_.globalPlacePath(), g)) {
        known = true;
        pl.music = g.music;
        pl.theme = g.theme;
    }
    return known;
}

// Another save slot in use: the main voyage open, with that slot's progress, the menu where it was on that slot.
void App::useSlot(int slot) {
    keepPlace(true);
    menu_.selectVoyage(""); // (on the slot left)
    slots_.setActive(slot);
    const std::string path = slots_.progressPath(slot);
    std::vector<std::string> dirs;
    for (int i = 0; i < SaveSlots::COUNT; i++) dirs.push_back(slots_.voyagesDir(i));
    voyages_.setProgressDirs(dirs, slot);
    voyages_.setMain(campaign_, path);
    progress_.load(path, &campaign_);
    menu_.resetPlace();
    if (placePath_.empty()) return; // (no menu place kept: shots)
    placePath_ = slots_.placePath(slot);
    Menu::Place pl;
    if (loadPlace(pl)) {
        if (!pl.voyage.empty() && !menu_.selectVoyage(pl.voyage)) pl.stage = -1; // (a voyage deleted since)
        menu_.restore(pl, true);
    }
    savedPlace_ = lastPlace_ = menu_.place();
}

// The main campaign ("") or a custom voyage becomes the one played, with its progress.
bool App::openVoyage(const std::string& id) {
    if (!id.empty()) voyages_.used(id);
    if (id == voyage_) return true;
    Campaign c;
    if (id.empty() ? !c.load(opt_.campaignPath) : !voyages_.find(id) || !c.load(voyages_.campaignPath(id))) return false;
    campaign_ = c;
    progress_.load(id.empty() ? slots_.progressPath(slots_.active()) : voyages_.progressPath(id), &campaign_);
    voyage_ = id;
    std::printf("[voyages] %s\n", id.empty() ? "the main voyage" : voyages_.find(id)->title.c_str());
    return true;
}

// The menu's choices that stay on it: the save slots' (one put in use; every stage of its main voyage unlocked, the
// slot marked so; one forgotten: its progress and its menu place), the gamepad's.
void App::applyOption(const Menu::Choice& c) {
    const std::string slot = "SLOT " + std::to_string(c.slot + 1);
    if (c.kind == Menu::Choice::SLOT_USE) {
        if (c.slot != slots_.active()) useSlot(c.slot);
        menu_.slotsChanged(); // (the box under it says SELECTED)
    } else if (c.kind == Menu::Choice::SLOT_UNLOCK) {
        // (the voyage open, main or custom, unlocked in memory and saved first, then every voyage's file on that slot)
        if (c.slot == slots_.active()) {
            progress_.unlockAll();
            progress_.save();
        }
        slots_.unlockAll(c.slot);
        menu_.slotsChanged();
        menu_.note(Menu::SLOTS, "ALL STAGES UNLOCKED");
    } else if (c.kind == Menu::Choice::SLOT_ERASE) {
        if (c.slot == slots_.active()) { // (the main voyage open, its progress forgotten, the menu as on a first launch)
            menu_.selectVoyage("");
            progress_.reset();
            slots_.erase(c.slot);
            menu_.resetPlace();
            savedPlace_ = lastPlace_ = menu_.place();
            if (!placePath_.empty()) Menu::savePlace(placePath_, savedPlace_); // (the options it keeps stay)
        } else {
            slots_.erase(c.slot);
            if (c.slot == 0) { // (slot 1's place keeps every slot's options)
                Menu::Place g;
                g.music = menu_.place().music;
                g.theme = menu_.place().theme;
                Menu::savePlace(slots_.globalPlacePath(), g);
            }
        }
        menu_.slotsChanged();
        menu_.note(Menu::SLOTS, slot + " ERASED");
    } else if (c.kind == Menu::Choice::PAD_REMAP) {
        input_.startRemap();
        return;
    } else if (c.kind == Menu::Choice::PAD_DEFAULT) {
        input_.resetMapping();
        menu_.note(Menu::GAMEPAD, "MAPPING RESTORED");
    }
#ifdef REFALL_WEB
    web::persist(runTime_, true);
#endif
}

// While the gamepad is remapped (its presses are the remap's): Esc cancels, Space skips the control asked for. True
// while a remap runs (the frame's keys are its own).
bool App::remapKeys() {
    if (!input_.remapping()) return false;
    if (input_.pressed(A_PAUSE)) input_.cancelRemap();
    else if (input_.pressed(A_UI_ALT)) input_.skipRemap();
#ifdef REFALL_WEB
    if (!input_.remapping()) web::persist(runTime_, true);
#endif
    return true;
}

// Saves where the menu is, 1.5 s after it last changed (or at once when leaving the menu).
void App::keepPlace(bool now) {
    if (placePath_.empty()) return;
    const Menu::Place pl = menu_.place();
    if (pl != lastPlace_) { lastPlace_ = pl; placeChangedAt_ = runTime_; }
    if (pl != savedPlace_ && (now || runTime_ - placeChangedAt_ > 1.5f)) {
        Menu::savePlace(placePath_, pl);
        if (slots_.active() > 0 && (pl.music != savedPlace_.music || pl.theme != savedPlace_.theme)) { // (every slot's)
            Menu::Place g;
            Menu::loadPlace(slots_.globalPlacePath(), g);
            g.music = pl.music;
            g.theme = pl.theme;
            Menu::savePlace(slots_.globalPlacePath(), g);
        }
        savedPlace_ = pl;
    }
}

// A session from the menu (or --stage): free play on a difficulty, or a campaign run from a song of a stage.
void App::beginSession(const Menu::Choice& c) {
    const bool camp = c.kind == Menu::Choice::PLAY_CAMPAIGN;
    mode_ = camp ? MODE_CAMPAIGN : MODE_FREE;
    lib_.clear();
    lib_.setShuffle(camp ? false : opt_.shuffle);
    lib_.setOnce(camp);
    std::vector<std::string> paths;
    float fixed = 0.f;
    forcedUniverse_ = forcedVariant_ = -1;
    if (!opt_.universe.empty()) forcedUniverse_ = universes::parse(opt_.universe, &forcedVariant_);
    if (camp) {
        const CampaignStage& st = campaign_.stages()[c.stage];
        run_ = Run{};
        run_.stage = c.stage;
        run_.from = c.song;
        run_.diff = c.difficulty;
        run_.stageRun = c.stageRun;
        run_.songPick = c.songPick;
        runSnapshot();
        for (int k = c.song; k < (int)st.songs.size(); k++) paths.push_back(st.songs[k].entry());
        fixed = st.songs[c.song].level[c.difficulty];
        // EXPERT's fall is a little faster again on top of its velocities (which stop at the top, 40)
        speedMul_ = c.difficulty == DIFF_EXPERT ? EXPERT_VOYAGE_SPEED : 1.f;
        if (opt_.universe.empty() && !st.songs[c.song].universe.empty())
            forcedUniverse_ = universes::parse(st.songs[c.song].universe, &forcedVariant_);
        std::printf("[campaign] stage %d from song %d on %s\n", c.stage + 1, c.song + 1, difficultyName(c.difficulty));
    } else {
        paths = c.playlist.empty() ? opt_.paths : std::vector<std::string>{c.playlist};
        if (!c.playlist.empty()) std::printf("[app] free play on %s\n", c.playlist.c_str());
        speedMul_ = opt_.difficultyGiven && c.difficulty == opt_.difficulty ? opt_.speedMul : freePlayMultiplier(c.difficulty);
        freeDiff_ = c.difficulty;
        std::printf("[app] free play on %s (gravity x%.2f)\n", difficultyName(c.difficulty), speedMul_);
    }
    lib_.scan(paths);
    std::printf("[app] %zu songs found\n", lib_.size());
    sceneCounter_++;
    lib_.prefetch(runSeed_ + sceneCounter_ * 7919);

    track_.reset();
    nextTrack_.reset();
    game_.setStartLevel(camp ? 1 : c.level);
    game_.setRunDifficulty(c.difficulty);
    game_.reset(runSeed_ ^ splitmix64(sceneCounter_));
    game_.setFixedLevel(fixed);
    lastLevel_ = game_.level();
    break_ = BREAK_OFF;
    breakT_ = 0;
    breakSkip_ = false;
    introAtStart_ = breaks_;
    R_.setBreak(Renderer::Break{});
    R_.setDim(false);
    if (slowWas_) R_.setBonus(false);
    slowWas_ = muffleWas_ = false;
    audio_.setMuffled(false, 0.3f);
    audio_.setSongGain(1.f, 0.3f);
    paused_ = wantSkip_ = songHeld_ = false;
    audio_.setPaused(false);
    overTime_ = 0;
    levelToast_ = slowToast_ = quadToast_ = skipToast_ = seekToast_ = failedToast_ = 0;
    songTime_ = simSongTime_ = 0;
    trackAge_ = 0;
    rowAcc_ = 0;
    lastGravityTime_ = -1;
    gameRate_ = 1.f;
    bonusFall_ = 0.f;
    pace_ = -1;
    dropAt_ = -1;
    look_ = makeSongLook(nullptr, runSeed_ ^ splitmix64(sceneCounter_ * 31));
    int variant = -1;
    changeUniverse(songUniverse(&variant), variant, transitions::NEW_SCENE, true);
}

void App::toMenu(Menu::Screen s, const std::string& msg, const std::string& sub, bool bad) {
    std::printf("[app] menu%s%s (%.1f s)\n", msg.empty() ? "" : ": ", msg.c_str(), runTime_);
    songHeld_ = false;
    audio_.setPaused(false);
    audio_.play(nullptr); // the song fades out (still muffled after a game over)
    paused_ = false;
    lib_.clear();
    track_.reset();
    nextTrack_.reset();
    R_.setBreak(Renderer::Break{});
    R_.setDim(false);
    if (slowWas_) R_.setBonus(false);
    slowWas_ = muffleWas_ = false;
    break_ = BREAK_OFF;
    mode_ = MODE_MENU;
    menu_.open(s, msg, sub, bad);
}

// A campaign song starts: its velocity and universe, from the data.
void App::campaignSongStarted() {
    const CampaignStage* st = runStage();
    if (!st) return;
    int k = -1;
    for (int i = std::max(run_.from, run_.current + 1); i < (int)st->songs.size() && k < 0; i++)
        if (track_->path.find(st->songs[i].id) != std::string::npos) k = i;
    if (k < 0) k = std::min((int)st->songs.size() - 1, std::max(run_.from, run_.current + 1));
    run_.current = k;
    run_.survived = false;
    run_.songStartScore = game_.score();
    run_.songStartLines = game_.lines();
    const CampaignSong& s = st->songs[k];
    run_.velTo = s.level[run_.diff];
    const bool expert = run_.diff == DIFF_EXPERT;
    if (k == run_.from) { // the run's first song: a chill warm-up first
        run_.velFrom = std::max(1.f, run_.velTo * (expert ? WARMUP_VELOCITY_EXPERT : WARMUP_VELOCITY));
        run_.velRamp = expert ? WARMUP_SECONDS_EXPERT : WARMUP_SECONDS;
    } else {
        run_.velFrom = std::min(run_.velTo, st->songs[k - 1].level[run_.diff]);
        run_.velRamp = expert ? NEXT_SONG_RAMP_SECONDS_EXPERT : NEXT_SONG_RAMP_SECONDS;
    }
    game_.setFixedLevel(run_.velFrom);
    lastLevel_ = game_.level();
    if (opt_.universe.empty()) {
        forcedUniverse_ = forcedVariant_ = -1;
        if (!s.universe.empty()) forcedUniverse_ = universes::parse(s.universe, &forcedVariant_);
    }
    std::printf("[campaign] stage %d song %d/%zu: %s, %s, velocity %.1f (%s)\n", run_.stage + 1, k + 1, st->songs.size(),
                s.title.c_str(), s.universe.empty() ? "a picked universe" : s.universe.c_str(), s.level[run_.diff],
                difficultyName(run_.diff));
}

// The current song was survived to its end: recorded as beaten, with the points scored during it.
void App::campaignSongSurvived() {
    const CampaignStage* st = runStage();
    if (!st || run_.current < 0 || run_.survived) return;
    run_.survived = true;
    const int pts = game_.score() - run_.songStartScore, lines = game_.lines() - run_.songStartLines;
    progress_.setBeaten(run_.stage, run_.current, run_.diff, pts, lines);
    progress_.save();
    std::printf("[campaign] song %d/%zu survived (%d points, %d lines)\n", run_.current + 1, st->songs.size(), pts, lines);
    if (run_.current + 1 >= (int)st->songs.size()) run_.finishing = true;
}

void App::runSnapshot() {
    run_.openBefore.assign((size_t)campaign_.size() * DIFF_COUNT, 0);
    for (int s = 0; s < campaign_.size(); s++)
        for (int d = 0; d < DIFF_COUNT; d++) run_.openBefore[s * DIFF_COUNT + d] = progress_.unlocked(s, d);
    run_.doneBefore = progress_.cleared(run_.stage, run_.diff) || progress_.allBeaten(run_.stage, run_.diff);
}

App::RunOutcome App::campaignOutcome(int score, int lines) {
    const int stage = run_.stage, diff = run_.diff;
    RunOutcome r;
    if (progress_.allBeaten(stage, diff)) progress_.setCleared(stage, diff, score, lines);
    progress_.save();
    r.fresh = progress_.cleared(stage, diff) && !run_.doneBefore;
    for (int s = 0; s < campaign_.size() && (size_t)s * DIFF_COUNT < run_.openBefore.size(); s++)
        for (int d = 0; d < DIFF_COUNT; d++) {
            if (run_.openBefore[s * DIFF_COUNT + d] || !progress_.unlocked(s, d)) continue;
            menu_.celebrateUnlock(s, d);
            if (s == stage + 1 && d == diff) r.nextOpened = true;
            if (s == stage && d == DIFF_EXPERT) r.expertOpened = true;
            std::printf("[campaign] stage %d opened on %s\n", s + 1, difficultyName(d));
        }
    if (r.fresh) {
        menu_.setCursor(stage, 0);
        menu_.celebrateClear(stage, stage + 1 < campaign_.size() ? stage + 1 : -1);
    }
    return r;
}

// The stage's last song is over: a clear when the run started from its first song, or when its songs are now all
// beaten (the others in earlier runs).
void App::campaignEnd() {
    const int stage = run_.stage;
    menu_.setCursor(stage, 0);
    const RunOutcome r = campaignOutcome(game_.score(), game_.lines());
    std::string note = r.nextOpened ? "NEXT STAGE UNLOCKED" : "";
    if (stage + 1 >= campaign_.size() && (run_.from == 0 || r.fresh)) note = "THE VOYAGE IS COMPLETE";
    if (r.expertOpened) note += note.empty() ? "EXPERT UNLOCKED" : "   /   EXPERT UNLOCKED";
    char sub[128];
    if (run_.from == 0 || r.fresh) {
        std::printf("[campaign] stage %d cleared on %s (%d points, %d lines)%s\n", stage + 1, difficultyName(run_.diff),
                    game_.score(), game_.lines(), run_.from == 0 ? "" : ", its last songs beaten");
        std::snprintf(sub, sizeof(sub), "STAGE %d CLEARED   /   %s", stage + 1, difficultyName(run_.diff));
        toDebrief({true, "CONGRATULATIONS", sub, note, game_.score(), game_.lines()});
    } else {
        std::snprintf(sub, sizeof(sub), "STAGE %d   /   SONGS %d-%zu SURVIVED   /   %s", stage + 1, run_.from + 1,
                      campaign_.stages()[stage].songs.size(), difficultyName(run_.diff));
        toDebrief({true, "WELL PLAYED", sub, note, game_.score(), game_.lines()});
    }
}

// A Voyage game over's debrief: where the run ended, its score and lines.
void App::gameOverDebrief() {
    const CampaignStage* st = runStage();
    char sub[128];
    std::snprintf(sub, sizeof(sub), "STAGE %d   /   SONG %d OF %d   /   %s", run_.stage + 1, run_.current + 1,
                  st ? (int)st->songs.size() : 0, difficultyName(run_.diff));
    std::printf("[campaign] game over: stage %d song %d, %d points, %d lines\n", run_.stage + 1, run_.current + 1,
                game_.lastScore(), game_.lines());
    // the songs survived before may have done the stage (its others beaten in earlier runs): told under the words, its
    // clear and unlocks played on the wheel once back there (the record: the points and lines up to the song lost)
    const RunOutcome r = campaignOutcome(run_.songStartScore, run_.songStartLines);
    char note[128] = "";
    if (r.fresh)
        std::snprintf(note, sizeof(note), "STAGE %d CLEARED   /   %s", run_.stage + 1,
                      r.nextOpened ? "NEXT STAGE UNLOCKED" : run_.stage + 1 >= campaign_.size() ? "THE VOYAGE IS COMPLETE" : "ALL SONGS BEATEN");
    if (r.expertOpened) std::snprintf(note + std::strlen(note), sizeof(note) - std::strlen(note), "%sEXPERT UNLOCKED", r.fresh ? "   /   " : "");
    Menu::Choice retry; // RETRY SONG: the song the game ended on, on to the stage's end from there
    retry.kind = Menu::Choice::PLAY_CAMPAIGN;
    retry.difficulty = run_.diff;
    retry.stage = run_.stage;
    retry.song = run_.current;
    retry.stageRun = run_.stageRun;
    retry.songPick = true;
    Menu::Choice stage = retry; // RETRY STAGE: the stage from its first song (offered in a run of the stage only)
    stage.song = 0;
    stage.songPick = false;
    if (!run_.stageRun) stage.kind = Menu::Choice::NONE;
    // (ended on the stage's first song: retrying that song alone is retrying the stage, RETRY STAGE alone)
    const bool first = run_.stageRun && run_.current == 0;
    if (first) { retry = stage; stage.kind = Menu::Choice::NONE; }
    toDebrief({false, "GAME OVER", sub, note, game_.lastScore(), game_.lines(), retry, stage, run_.songPick && !first, first});
}

// The end of a run: back to the menu, on the debrief (then the Voyage's screen, on the same stage and difficulty).
void App::toDebrief(const Menu::Debrief& d) {
    toMenu(Menu::CAMPAIGN);
    if (d.clear) menu_.dropBackdrop(); // (on the black the outro ended on)
    menu_.openDebrief(d);
}

// ---- refall-voyage-viewer

VoyageViewer::Now App::viewerNow() const {
    VoyageViewer::Now n;
    n.current = track_ ? viewCur_ : -1;
    n.wanted = viewWant_;
    n.paused = paused_;
    if (track_) {
        n.time = songTime_;
        n.duration = track_->duration();
        n.an = &track_->analysis;
        n.plan = &plan_;
    }
    return n;
}

// The campaign's songs, the first one (--stage, --song) on its way, its universe shown while it loads.
bool App::viewerStart() {
    if (campaign_.size() == 0) { std::fprintf(stderr, "[viewer] the campaign has no songs\n"); return false; }
    vv_ = std::make_unique<VoyageViewer>(&campaign_, opt_.difficulty);
    const int start = vv_->find(opt_.viewStage - 1, opt_.viewSong - 1);
    if (start < 0) {
        std::fprintf(stderr, "[viewer] no stage %d song %d in %s\n", opt_.viewStage, opt_.viewSong, campaign_.source().c_str());
        return false;
    }
    std::printf("[viewer] %d songs (%s), velocities of %s\n", vv_->count(), campaign_.source().c_str(),
                difficultyName(opt_.difficulty));
    lib_.setShuffle(false);
    const std::string& u = vv_->song(start).s->universe;
    forcedUniverse_ = forcedVariant_ = -1;
    if (!u.empty()) forcedUniverse_ = universes::parse(u, &forcedVariant_);
    viewerJump(start);
    return true;
}

// Each frame: the overlay's keys, mouse and tour (paused: nothing moves), and the campaign's end starting it over.
void App::viewerFrame(float dt) {
    if (!vv_) return;
    if (track_) viewShotT_ += dt;
    // Test hook: REFALL_VIEWER_KEYS="T:COMMAND,..." gives the overlay a command (VoyageViewer::command) T seconds after the
    // first song started.
    static const auto keys = testList("REFALL_VIEWER_KEYS");
    for (auto& [t, k] : keys)
        if (viewShotT_ - dt < t && viewShotT_ >= t) { vv_->command(k); std::printf("[test] %.2f s: %s\n", viewShotT_, k.c_str()); }
    if (paused_) return;
    if (track_ && songFinished() && !nextTrack_ && !lib_.loading() && viewWant_ < 0) viewerJump(viewCur_ + 1);
    for (const VoyageViewer::Action& a : vv_->update(win_, input_, viewerNow(), dt)) {
        if (a.kind == VoyageViewer::Action::JUMP) viewerJump(a.index);
        else viewerSeek(a.time);
    }
}

// Plays song i: the one playing gives way as soon as it is ready (downloaded, decoded and analysed in the background,
// then the songs after it, in order).
void App::viewerJump(int i) {
    const int n = vv_->count();
    i = ((i % n) + n) % n;
    if (i == viewCur_ && viewWant_ < 0 && track_) { // the song playing: from its start
        viewerSeek(0);
        vv_->songStarted();
        return;
    }
    viewWant_ = i;
    wantSkip_ = true;
    if (nextTrack_ && vv_->indexOf(nextTrack_->path) == i) return;
    const std::string& id = vv_->song(i).s->id;
    if (!nextTrack_ && lib_.loading())
        for (const std::string& e : lib_.files())
            if (e.rfind("ytdl:" + id + "\t", 0) == 0 && youtubeEntryTitle(e) == lib_.loadingTitle()) return; // on its way
    lib_.clear();
    lib_.setOnce(true);
    std::vector<std::string> paths;
    for (int k = i; k < n; k++) paths.push_back(vv_->entry(k));
    lib_.scan(paths);
    nextTrack_.reset();
    lib_.prefetch(runSeed_);
    std::printf("[viewer] to stage %d song %d: %s\n", vv_->song(i).stage + 1, vv_->song(i).song + 1, vv_->song(i).s->title.c_str());
}

void App::viewerSeek(double t) {
    if (!track_) return;
    seekSong(std::clamp(t, 0.0, std::max(0.0, track_->duration() - 1.0)));
}

// A jump in the song: the scene is set to where it lands at once (into a peak, with its drop hit, which a jump past
// the peak's first beat would otherwise miss: the universes would never take their drop look).
void App::seekSong(double t) {
    audio_.seek(t);
    songTime_ = simSongTime_ = t;
    pace_ = -1; // re-sync the beat-driven gravity
    lastGravityTime_ = -1;
    lastSeg_ = track_->analysis.segmentAt(t);
    if (plan_.phases.empty()) return;
    const int ph = plan_.phaseAt(t);
    dropAt_ = -1;
    if (ph == lastPhase_) return;
    lastPhase_ = ph;
    const ScenePhase& to = plan_.phases[ph];
    R_.setLevel(to.level, to.energy, 1.f, to.level >= 2 ? 1.f : 0.f);
    if (to.level >= 2) {
        R_.drop(1.f);
        std::printf("[app] drop (landed in the peak)\n");
    }
}

// A song starts: its title, velocity and universe, from the campaign.
void App::viewerSongStarted() {
    viewCur_ = vv_->indexOf(track_->path);
    if (viewCur_ < 0) viewCur_ = std::max(0, viewWant_);
    viewWant_ = -1;
    wantSkip_ = false;
    const VoyageViewer::Song& v = vv_->song(viewCur_);
    track_->title = v.s->title; // (a song no cached listing names would show its id)
    game_.setFixedLevel(v.s->level[opt_.difficulty]);
    lastLevel_ = game_.level();
    forcedUniverse_ = forcedVariant_ = -1;
    if (!v.s->universe.empty()) forcedUniverse_ = universes::parse(v.s->universe, &forcedVariant_);
    vv_->songStarted();
    std::printf("[viewer] stage %d song %d: %s, %s, velocity %.1f\n", v.stage + 1, v.song + 1, v.s->title.c_str(),
                v.s->universe.empty() ? "a picked universe" : v.s->universe.c_str(), v.s->level[opt_.difficulty]);
}

// --menu-shot: a menu screen, rendered offline, saved, then exit. --menu-clip: a video walking through the menus (a
// built-in key sequence, or REFALL_TEST_KEYS), then exit.
int App::runMenuShot() {
    if (opt_.menuDemo || !opt_.menuClip.empty()) {
        // made-up progress, in memory only (never saved): by default stages 1 and 2 cleared, stage 3 started, the rest
        // locked; "normal-done": every stage cleared on NORMAL
        progress_ = Progress{};
        progress_.setCampaign(&campaign_); // (its songs resolved: a stage whose songs are all beaten counts)
        playlists_.demo();
        if (opt_.menuDemoKind == "normal-done") {
            // every stage cleared on NORMAL (every song beaten), the first five on EASY (songs of the sixth beaten),
            // nothing on HARD
            for (int s = 0; s < campaign_.size(); s++) {
                const int n = (int)campaign_.stages()[s].songs.size();
                int run = 0, runLines = 0;
                for (int k = 0; k < n; k++) {
                    const int pts = 7400 + s * 860 + ((s * 7 + k * 13) % 9) * 310;
                    const int ln = 26 + s * 2 + ((s * 5 + k * 11) % 7) * 3; // a song is a few minutes: ~25-60 lines
                    progress_.setBeaten(s, k, DIFF_NORMAL, pts, ln);
                    run += pts;
                    runLines += ln;
                    if (s < 6) progress_.setBeaten(s, k, DIFF_EASY, pts * 3 / 4, ln - 4);
                }
                progress_.setCleared(s, DIFF_NORMAL, run, runLines);
                if (s < 5) progress_.setCleared(s, DIFF_EASY, run * 3 / 4, runLines - 4 * n);
            }
        } else {
            // (as many songs as the data's stages have)
            const int pts[5] = {8210, 9420, 10020, 9900, 10690}, ln[5] = {31, 36, 38, 35, 42};
            for (int s = 0; s < 2 && s < campaign_.size(); s++) {
                const int n = (int)campaign_.stages()[s].songs.size();
                int run = 0, runLines = 0, runE = 0, runLinesE = 0;
                for (int k = 0; k < n; k++) {
                    const int i = (k + 2 * s) % 5, p = pts[i] + 1300 * s, l = ln[i] + 4 * s;
                    progress_.setBeaten(s, k, DIFF_NORMAL, p, l);
                    run += p; runLines += l;
                    if (s == 0) { progress_.setBeaten(s, k, DIFF_EASY, p - 2100, l - 6); runE += p - 2100; runLinesE += l - 6; }
                }
                progress_.setCleared(s, DIFF_NORMAL, run, runLines);
                if (s == 0) progress_.setCleared(s, DIFF_EASY, runE, runLinesE);
            }
            if (campaign_.size() > 2)
                for (int k = 0; k < 2 && k < (int)campaign_.stages()[2].songs.size(); k++)
                    progress_.setBeaten(2, k, DIFF_NORMAL, k ? 9870 : 11240, k ? 37 : 40);
            progress_.setBeaten(0, 0, DIFF_HARD, 12650, 44);
        }
    }
    menu_.init(&campaign_, &progress_);
    const std::string& sc = opt_.menuScreen;
    menu_.open(sc == "campaign" || sc == "unlock" || sc.rfind("debrief", 0) == 0 ? Menu::CAMPAIGN : sc == "free" ? Menu::FREE
               : sc == "slots" ? Menu::SLOTS : Menu::TITLE);
    if (sc == "free" && opt_.levelGiven) menu_.setFreeLevel(opt_.level, true); // (the level's row in focus)
    // (debrief-over-song: a run started on a later song, RETRY SONG only; debrief-over-retried: RETRY SONG chosen
    // before; debrief-over-first: ended on the stage's first song, RETRY STAGE only)
    const bool over = sc == "debrief-over" || sc == "debrief-over-song" || sc == "debrief-over-retried" || sc == "debrief-over-first";
    if (over || sc == "debrief-clear") { // a made-up run's end (over the black: no game frame here)
        const int songs = campaign_.size() > 2 ? (int)campaign_.stages()[2].songs.size() : 5;
        char b[128];
        if (over) std::snprintf(b, sizeof(b), "STAGE 3   /   SONG 4 OF %d   /   NORMAL", songs);
        else std::snprintf(b, sizeof(b), "STAGE 3 CLEARED   /   NORMAL");
        Menu::Choice retry;
        retry.kind = Menu::Choice::PLAY_CAMPAIGN;
        Menu::Choice stage = retry;
        if (sc == "debrief-over-song" || sc == "debrief-over-first") stage.kind = Menu::Choice::NONE;
        menu_.openDebrief(over ? Menu::Debrief{false, "GAME OVER", b, "", 27340, 96, retry, stage, sc == "debrief-over-retried",
                                               sc == "debrief-over-first"}
                                               : Menu::Debrief{true, "CONGRATULATIONS", b, "NEXT STAGE UNLOCKED", 48710, 163});
        if (sc == "debrief-clear" && campaign_.size() > 3) { // then back on the Voyage: stage 3 turns cleared, 4 opens
            progress_.setCleared(2, DIFF_NORMAL, 48710, 163);
            menu_.setCampaignDifficulty(DIFF_NORMAL);
            menu_.setCursor(2, 0);
            menu_.celebrateClear(2, 3);
            menu_.celebrateUnlock(3, DIFF_NORMAL);
        }
    }
    // the stage 3 done over several runs (its first two songs beaten before), as the game ends it: its last two songs
    // survived (debrief-songs), or song 3 survived and song 4, beaten before, lost (debrief-over-songs); then the wheel
    if ((sc == "debrief-songs" || sc == "debrief-over-songs") && campaign_.size() > 3) {
        const int ns = (int)campaign_.stages()[2].songs.size();
        menu_.setCampaignDifficulty(DIFF_NORMAL);
        menu_.setCursor(2, 2);
        if (sc == "debrief-over-songs") progress_.setBeaten(2, ns - 1, DIFF_NORMAL, 10450, 39);
        run_ = Run{};
        run_.stage = 2;
        run_.from = 2;
        run_.diff = DIFF_NORMAL;
        runSnapshot();
        const int last = sc == "debrief-songs" ? ns : ns - 1;
        for (int k = 2; k < last; k++) progress_.setBeaten(2, k, DIFF_NORMAL, 10100 + 230 * k, 37 + k);
        if (sc == "debrief-songs") campaignEnd();
        else {
            run_.current = ns - 1;
            mode_ = MODE_CAMPAIGN; // (runStage)
            gameOverDebrief();
        }
        Menu::Debrief d = menu_.debrief(); // (a made-up score: no game was played)
        d.score = sc == "debrief-songs" ? 21460 : 13870;
        d.lines = sc == "debrief-songs" ? 79 : 52;
        menu_.openDebrief(d);
    }
    if (sc == "unlock" && campaign_.size() > 3) { // the Voyage, stage 4 just opened (stage 3 done, celebrated before)
        progress_.setCleared(2, DIFF_NORMAL, 48710, 163);
        menu_.setCampaignDifficulty(DIFF_NORMAL);
        menu_.setCursor(2, 0);
        menu_.celebrateUnlock(3, DIFF_NORMAL);
    }
    if (!opt_.menuHover.empty()) {
        int st = 1, sg = 1;
        std::sscanf(opt_.menuHover.c_str(), "%d:%d", &st, &sg);
        menu_.setCursor(std::max(0, st - 1), std::max(0, sg - 1));
    }
    if (opt_.menuPetal) menu_.setPetalOpen(true);
    menu_.setPlaylists(&playlists_);
    menu_.snap();
    if (std::getenv("REFALL_MENU_INTRO") && sc == "title") menu_.startIntro(); // (the opening, as on a first launch)
    int w, h;
    fbSize(w, h);
    if (!opt_.menuClip.empty()) {
        // the walk: petals focused (one locked), one opened and its songs browsed, closed, another opened, the
        // difficulties; then free play: a difficulty, the playlists, a playlist link typed and read
        static const std::pair<double, const char*> WALK[] = {
            {1.2, "down"}, {1.9, "up"}, {2.7, "ok"}, {3.15, "ok"},
            {3.6, "right"}, {4.4, "right"}, {5.2, "right"}, {6.0, "ok"}, {6.9, "left"}, {7.7, "left"}, {8.6, "alt"},
            {9.8, "right"}, {10.6, "right"}, {11.4, "right"}, {12.2, "left"}, {13.4, "back"}, {14.2, "right"}, {15.0, "alt"},
            {16.0, "right"}, {16.8, "right"}, {17.8, "back"}, {18.6, "up"}, {19.4, "right"}, {20.2, "left"}, {21.0, "down"},
            {21.9, "back"}, {22.35, "back"}, {22.8, "down"}, {23.5, "ok"}, {24.3, "left"}, {25.0, "down"}, {25.7, "right"}, {26.4, "right"},
            {27.1, "right"}, {27.8, "ok"}, {30.8, "enter"}, {32.6, "up"}};
        const double END = std::getenv("REFALL_MENU_CLIP_SECONDS") ? std::atof(std::getenv("REFALL_MENU_CLIP_SECONDS")) : 34.5;
        std::vector<std::pair<double, std::string>> keys = testList("REFALL_TEST_KEYS");
        if (keys.empty() && sc.rfind("debrief", 0) != 0 && sc != "unlock") { // (a debrief's or an unlock's clip: its animation, no key)
            for (auto& [t, k] : WALK) keys.push_back({t, k});
            const std::string link = "youtube.com/playlist?list=PLPWC9IDC1n_Q";
            for (size_t i = 0; i < link.size(); i++) keys.push_back({28.2 + i * 0.05, "type:" + link.substr(i, 1)});
        }
        std::string ff = std::getenv("REFALL_FFMPEG") ? std::getenv("REFALL_FFMPEG") : platform::findExecutable("ffmpeg");
        if (ff.empty()) { std::fprintf(stderr, "[clip] ffmpeg not found\n"); return 1; }
        char spec[160];
        std::snprintf(spec, sizeof(spec), " -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r 60 -i - ", w, h);
        const std::string cmd = platform::quoteArg(ff) + spec + "-vf vflip " +
                                std::string(headless_ && headless::nvidia() ? "-c:v h264_nvenc -preset p5 -rc vbr -cq 23 -b:v 0 -maxrate 6M -bufsize 12M"
                                                                            : "-c:v libx264 -preset medium -crf 20 -maxrate 6M -bufsize 12M") +
                                " -pix_fmt yuv420p -movflags +faststart " + platform::quoteArg(opt_.menuClip);
        FILE* pipe = platform::openWrite(cmd);
        if (!pipe) { std::fprintf(stderr, "[clip] could not start ffmpeg\n"); return 1; }
        static const std::pair<const char*, Action> NAMES[] = {{"up", A_UI_UP}, {"down", A_UI_DOWN}, {"left", A_UI_LEFT},
            {"right", A_UI_RIGHT}, {"ok", A_UI_OK}, {"back", A_UI_BACK}, {"enter", A_UI_ENTER}, {"alt", A_UI_ALT}};
        std::vector<unsigned char> px((size_t)w * h * 3);
        const int frames = (int)(END * 60);
        // the menu's frame cost: CPU (update + drawing calls) and GPU (a timer query around the drawing)
        GLuint gpuQ = 0;
        glGenQueries(1, &gpuQ);
        double cpuSum = 0, cpuMax = 0, gpuSum = 0, gpuMax = 0, wallSum = 0, navMax = 0, lastKey = -10;
        for (int f = 0; f < frames; f++) {
            const double t0 = f / 60.0, t1 = (f + 1) / 60.0;
            input_.testFrame();
            for (auto& [t, k] : keys)
                if (t >= t0 && t < t1) {
                    if (k.rfind("type:", 0) == 0) input_.testType(k.substr(5));
                    for (auto& [n, a] : NAMES)
                        if (k == n) input_.testPress(a);
                }
            const auto c0 = std::chrono::steady_clock::now();
            const Menu::Choice c = menu_.update(input_, 1.f / 60.f);
            (void)c; // a choice (PLAY) is not followed: the walk stays in the menus
            double cpu = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0).count();
            // a playlist being read: the video waits for it (up to 30 s)
            for (int i = 0; i < 1500 && menu_.fetching() && !menu_.fetchDone(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            const auto c1 = std::chrono::steady_clock::now();
            glBeginQuery(GL_TIME_ELAPSED, gpuQ);
            menu_.render(w, h);
            glEndQuery(GL_TIME_ELAPSED);
            cpu += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c1).count();
            if (std::getenv("REFALL_MENU_FINISH")) { // software renderers: the frame's whole cost as wall time
                glFinish();
                wallSum += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c1).count();
            }
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            GLuint64 ns = 0;
            glGetQueryObjectui64v(gpuQ, GL_QUERY_RESULT, &ns);
            const double gpu = ns / 1e6;
            cpuSum += cpu; gpuSum += gpu;
            cpuMax = std::max(cpuMax, cpu); gpuMax = std::max(gpuMax, gpu);
            for (auto& [t, k] : keys)
                if (t < t1) lastKey = std::max(lastKey, t);
            if (t0 - lastKey < 0.6) navMax = std::max(navMax, std::max(cpu, gpu)); // a frame right after a key: navigating
            if (std::getenv("REFALL_MENU_SLOW") && std::max(cpu, gpu) > 30.0) std::printf("[menu] slow frame %d (%.2f s): CPU %.1f ms, GPU %.1f ms\n", f, t0, cpu, gpu);
            std::fwrite(px.data(), 1, px.size(), pipe);
        }
        glDeleteQueries(1, &gpuQ);
        std::printf("[menu] %d frames at %dx%d: CPU %.2f ms on average (max %.1f), GPU %.2f ms on average (max %.1f)\n", frames, w, h,
                    cpuSum / frames, cpuMax, gpuSum / frames, gpuMax);
        if (wallSum > 0) std::printf("[menu] drawing with glFinish: %.2f ms on average\n", wallSum / frames);
        std::printf("[menu] worst frame while navigating (within 0.6 s of a key): %.1f ms\n", navMax);
        const int rc = platform::closeRead(pipe);
        std::printf("[clip] %s %s (%s, %.0f s)\n", rc == 0 ? "wrote" : "FAILED", opt_.menuClip.c_str(), "kaleidoscope", END);
        if (headless_) headless::shutdown();
        return rc == 0 ? 0 : 1;
    }
    // REFALL_TEST_KEYS="T:KEY,...": keys pressed before the shot (it is taken 1.5 s after the last one)
    {
        const auto keys = testList("REFALL_TEST_KEYS");
        double end = 0.2;
        for (auto& [t, k] : keys) end = std::max(end, t + 1.5);
        static const std::pair<const char*, Action> NAMES[] = {{"up", A_UI_UP}, {"down", A_UI_DOWN}, {"left", A_UI_LEFT},
            {"right", A_UI_RIGHT}, {"ok", A_UI_OK}, {"back", A_UI_BACK}, {"enter", A_UI_ENTER}, {"alt", A_UI_ALT}};
        const int frames = std::max({12, (int)(end * 60), (int)std::lround(opt_.menuTime * 60)});
        for (int f = 0; f < frames; f++) {
            const double t0 = f / 60.0, t1 = (f + 1) / 60.0;
            input_.testFrame();
            for (auto& [t, k] : keys)
                if (t >= t0 && t < t1)
                    for (auto& [n, a] : NAMES)
                        if (k == n) input_.testPress(a);
            menu_.update(input_, 1.f / 60.f);
            if (keys.empty() && f < frames - 1) continue; // (a plain shot: only its last frame needs drawing)
            menu_.render(w, h);
        }
    }
    glFinish();
    const bool ok = menu_.screenshot(opt_.menuShot, w, h);
    std::printf("[shot] %s%s\n", opt_.menuShot.c_str(), ok ? "" : " FAILED");
    if (headless_) headless::shutdown();
    else { glfwDestroyWindow(win_); glfwTerminate(); }
    return ok ? 0 : 1;
}

static void viewerUsage() {
    std::printf(
        "usage: refall-voyage-viewer [campaign] [options]\n"
        "  a campaign reviewed: its songs in their universes, the computer playing; the overlay (Tab / H) shows\n"
        "  the songs by stage and the song's sections, and its tour plays each section a few seconds (Space)\n"
        "  campaign             a campaign .tsv, or a custom voyage (its id or title); default: the main campaign\n"
        "  --difficulty D       the velocities played: easy, normal (default), hard or expert\n"
        "  --stage N --song K   start on that song (default 1, 1)\n"
        "  --size WxH           window size (default 1600x900)\n"
        "  --fullscreen         start fullscreen\n"
        "  --mute               no sound\n"
        "  --shot OUT.png       save the viewer --shot-after SEC (default 8) into the first song, then exit (testing)\n"
        "keys: Space tour, Left/Right seek 10 s, [ ] previous/next section, Up/Down song, Page Up/Down stage,\n"
        "  Tab / H the overlay, a click on a card plays its song, on the timeline seeks; Esc pause, F fullscreen\n");
}

// refall-voyage-viewer's command line.
static int viewerMain(int argc, char** argv) {
    Options o;
    o.viewer = o.autoplay = true;
    o.difficulty = DIFF_NORMAL;
    std::string camp;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "-h" || a == "--help") { viewerUsage(); return 0; }
        else if (a == "--difficulty" && i + 1 < argc) {
            if (!parseDifficulty(argv[++i], &o.difficulty, &o.speedMul)) {
                std::fprintf(stderr, "unknown difficulty '%s': easy, normal, hard or expert\n", argv[i]);
                return 1;
            }
        }
        else if (a == "--stage" && i + 1 < argc) o.viewStage = std::atoi(argv[++i]);
        else if (a == "--song" && i + 1 < argc) o.viewSong = std::atoi(argv[++i]);
        else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &o.width, &o.height);
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--mute") o.mute = true;
        else if (a == "--seed" && i + 1 < argc) o.seed = std::stoull(argv[++i]);
        else if (a == "--shot" && i + 1 < argc) o.viewShot = argv[++i];
        else if (a == "--shot-after" && i + 1 < argc) o.viewShotAfter = std::stof(argv[++i]);
        else if (a.rfind("--", 0) == 0) { std::fprintf(stderr, "unknown option %s (--help lists them)\n", a.c_str()); return 1; }
        else camp = a;
    }
    if (!camp.empty()) {
        std::error_code ec;
        if (fs::is_regular_file(camp, ec)) {
            o.campaignPath = camp;
        } else { // a custom voyage, by its id or its title
            VoyageStore vs;
            vs.load(VoyageStore::defaultDir(""));
            auto low = [](std::string x) { for (char& c : x) c = (char)std::tolower((unsigned char)c); return x; };
            for (const CustomVoyage& v : vs.list())
                if (v.id == camp || low(v.title) == low(camp)) o.campaignPath = vs.campaignPath(v.id);
            if (o.campaignPath.empty()) {
                std::fprintf(stderr, "no campaign file or voyage named '%s'\n", camp.c_str());
                return 1;
            }
        }
    }
    App app(o);
    const int rc = app.run();
    std::fflush(stdout);
    std::_Exit(rc); // (background downloads aren't waited for)
}

// The game, its command line read (on the web: once the player's folders are there).
static int start(int argc, char** argv) {
#ifdef REFALL_VOYAGE_VIEWER
    return viewerMain(argc, argv);
#endif
    Options o;
    bool freePlay = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "--autoplay") o.autoplay = true;
        else if (a == "--cheat-skip") o.cheatSkip = true;
        else if (a == "--mute") o.mute = true;
        else if (a == "--shuffle") o.shuffle = true; // the default; kept for older command lines
        else if (a == "--in-order") o.shuffle = false;
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--hidden") o.hidden = true;
        else if (a == "--list") {
            for (int k = 0; k < universes::count(); k++) {
                const UniverseEntry& e = universes::entry(k);
                std::printf("%-10s %s%s\n", e.name, e.blurb, e.hidden ? " (test, never picked)" : "");
                for (size_t v = 0; v < e.variants.size(); v++) std::printf("  %s:%s\n", e.name, e.variants[v].c_str());
            }
            return 0;
        }
        else if (a == "--stream") o.stream = o.hidden = true;
        else if (a == "--seed" && i + 1 < argc) o.seed = std::stoull(argv[++i]);
        else if ((a == "--universe" || a == "--scene") && i + 1 < argc) o.universe = argv[++i];
        else if (a == "--universe-on-peak") o.universeOnPeak = true;
        else if (a == "--to" && i + 1 < argc) o.toUniverse = argv[++i];
        else if (a == "--transition" && i + 1 < argc) o.transition = argv[++i];
        else if (a == "--transition-shots" && i + 2 < argc) {
            o.transitionShots = argv[++i];
            o.transitionShotCount = std::max(1, std::atoi(argv[++i]));
            o.shotPrefix = o.transitionShots;
            o.shotCount = 1;
        }
        else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &o.width, &o.height);
        else if (a == "--shots" && i + 2 < argc) { o.shotPrefix = argv[++i]; o.shotCount = std::atoi(argv[++i]); }
        else if (a == "--phase-shots" && i + 1 < argc) { o.shotPrefix = argv[++i]; o.shotCount = 99; o.phaseShots = true; }
        else if (a == "--shot-time" && i + 1 < argc) o.shotTime = std::stof(argv[++i]);
        else if (a == "--review-shot" && i + 1 < argc) { o.reviewPath = argv[++i]; o.shotPrefix = o.reviewPath; o.shotCount = 1; }
        else if (a == "--review-level" && i + 1 < argc) o.reviewLevel = std::atoi(argv[++i]);
        else if (a == "--clip" && i + 1 < argc) o.clipPath = argv[++i];
        else if (a == "--clip-at" && i + 1 < argc) o.clipAt = std::stof(argv[++i]);
        else if (a == "--clip-break") o.clipBreak = true;
        else if (a == "--clip-before" && i + 1 < argc) o.clipBefore = std::stof(argv[++i]);
        else if (a == "--clip-after" && i + 1 < argc) o.clipAfter = std::stof(argv[++i]);
        else if (a == "--analysis" && i + 1 < argc) { o.analysisPath = argv[++i]; o.mute = true; }
        else if (a == "--free") freePlay = true;
        else if (a == "--difficulty" && i + 1 < argc) {
            if (!parseDifficulty(argv[++i], &o.difficulty, &o.speedMul)) {
                std::fprintf(stderr, "unknown difficulty '%s': easy, normal, hard, expert or a multiplier (e.g. 0.7)\n", argv[i]);
                return 1;
            }
            o.difficultyGiven = true;
        }
        else if (a == "--level" && i + 1 < argc) {
            o.level = std::atoi(argv[++i]);
            if (o.level < 1 || o.level > Game::MAX_LEVEL) {
                std::fprintf(stderr, "--level: a level from 1 to %d\n", (int)Game::MAX_LEVEL);
                return 1;
            }
            o.levelGiven = true;
        }
        else if (a == "--stage" && i + 1 < argc) o.stage = argv[++i];
        else if (a == "--campaign" && i + 1 < argc) o.campaignPath = argv[++i];
        else if (a == "--progress" && i + 1 < argc) o.progressPath = argv[++i];
        else if (a == "--playlists" && i + 1 < argc) o.playlistsPath = argv[++i];
        else if (a == "--menu-petal") o.menuPetal = true;
        else if (a == "--menu-shot" && i + 1 < argc) o.menuShot = argv[++i];
        else if (a == "--menu-clip" && i + 1 < argc) o.menuClip = argv[++i];
        else if (a == "--menu-demo") {
            o.menuDemo = true;
            if (i + 1 < argc && std::string(argv[i + 1]) == "normal-done") o.menuDemoKind = argv[++i];
        }
        else if (a == "--no-menu-music") o.menuMusic = false;
        else if (a == "--menu-screen" && i + 1 < argc) o.menuScreen = argv[++i];
        else if (a == "--menu-hover" && i + 1 < argc) o.menuHover = argv[++i];
        else if (a == "--menu-time" && i + 1 < argc) o.menuTime = std::stof(argv[++i]);
        else o.paths.push_back(a);
    }
    // The title screen when the game is started for play with no songs; songs on the command line, --free and the
    // offline renders play straight away (free play), --stage runs a campaign stage.
    const bool offline = !o.shotPrefix.empty() || !o.clipPath.empty() || !o.analysisPath.empty() || !o.reviewPath.empty() ||
                         !o.transitionShots.empty();
    o.menuEnabled = !offline && !freePlay && o.paths.empty() && o.menuShot.empty() && o.menuClip.empty();
    o.menu = o.menuEnabled && o.stage.empty();
    if (!o.universe.empty() && !universes::check(o.universe)) return 1;
    if (!o.toUniverse.empty() && !universes::check(o.toUniverse)) return 1;
    // No songs given: the default playlist (free play: the electro one, thousands of songs)
    if (o.paths.empty()) o.paths.push_back(freePlay ? ELECTRO_PLAYLIST : DEFAULT_PLAYLIST);
#ifdef REFALL_WEB
    (new App(o))->run(); // (kept: the browser calls its frames once this returns)
    return 0;
#endif
    App app(o);
    int rc = app.run();
    // Exit now: don't wait for background downloads/analysis (an unfinished download resumes next time).
    std::fflush(stdout);
    std::_Exit(rc);
}

int main(int argc, char** argv) {
    g_startTime = std::chrono::steady_clock::now();
    platform::standaloneStart();
    platform::lineBufferStdout();
#ifdef REFALL_WEB
    // the player's folders first, filled from IndexedDB in the background: the game starts once they are
    static std::vector<std::string> args(argv, argv + argc);
    web::mountStorage([] {
        static std::vector<char*> av;
        for (std::string& a : args) av.push_back(a.data());
        av.push_back(nullptr);
        start((int)args.size(), av.data());
    });
    return 0;
#endif
    return start(argc, argv);
}

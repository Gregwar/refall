#pragma once
// The menus, over a crystal kaleidoscope with frosted glass panels (menuwin.cpp): the title screen (VOYAGE, FREE
// PLAY, SAVE SLOTS, OPTIONS, QUIT), the save slots, the voyages to choose from (the game's own, and those made from the player's YouTube playlists), free
// play's difficulty, playlist and starting level, and the campaign's stages (gem petals in a quiet rosette, opening
// onto their songs), with an audio preview of the song under the cursor.
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>

#include "audio.hpp"
#include "campaign.hpp"
#include "input.hpp"
#include "menupaint.hpp"
#include "playlists.hpp"
#include "saveslots.hpp"
#include "title.hpp"
#include "voyages.hpp"

inline std::string gameTitle(bool upperCase) {
    std::string s = GAME_TITLE;
    if (upperCase)
        for (char& c : s) c = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
    return s;
}

// A colour per universe for the campaign's squares (the 8-bit palette, in order of first appearance).
void menuUniverseColor(const Campaign* camp, const std::string& spec, unsigned char out[3]);

class Menu {
public:
    // DEBRIEF: the end of a Voyage run (a game over or a stage cleared), before the Voyage's screen comes back.
    // VOYAGES: the voyages (the game's own first, then the player's) and the making of one from a playlist; the one
    // chosen then shows as CAMPAIGN. SLOTS: the save slots (saveslots.hpp), one to use, unlock or erase.
    enum Screen { TITLE, FREE, CAMPAIGN, DEBRIEF, VOYAGES, OPTIONS, GAMEPAD, SLOTS };
    struct Choice {
        // SLOT_USE, SLOT_UNLOCK, SLOT_ERASE: the save slots' (`slot` the one: put in use, every stage unlocked on it,
        // forgotten); PAD_REMAP, PAD_DEFAULT: the gamepad's (a remap started, its own mapping back); the menu stays
        enum Kind { NONE, QUIT, PLAY_FREE, PLAY_CAMPAIGN, SLOT_USE, SLOT_UNLOCK, SLOT_ERASE, PAD_REMAP, PAD_DEFAULT } kind = NONE;
        int slot = 0;
        int difficulty = DIFF_HARD;
        int stage = 0, song = 0; // PLAY_CAMPAIGN: where the run starts
        bool stageRun = false;   // PLAY_CAMPAIGN: a run of the stage (from its first song, or RETRY SONG within one)
        bool songPick = false;   // PLAY_CAMPAIGN: RETRY SONG chosen in this run: the next game over's default
        std::string playlist;    // PLAY_FREE: the playlist chosen ("" : the command line's songs or the default)
        int level = 1;           // PLAY_FREE: the level the game starts at
    };

    Menu();
    ~Menu();
    void init(const Campaign* c, Progress* p);
    // Shows a screen; `message` (e.g. "GAME OVER") shows a moment over it, with `sub` under it.
    void open(Screen s, const std::string& message = "", const std::string& sub = "", bool bad = false);
    Screen screen() const { return screen_; }
    // The debrief at the end of a run: the REFALL logo (as white contours after a game over; turning from its contours
    // into its colours for a stage cleared), the heading, the score and lines, a small line of details (the stage, the
    // song, the difficulty) and an optional note; a key (after a moment) goes on to the Voyage's screen. With a `retry`
    // (a game over), it offers RETRY (that run again) or MENU (the Voyage's screen) instead.
    struct Debrief {
        bool clear = false;
        std::string heading, detail, note;
        int score = 0, lines = 0;
        Choice retry;      // RETRY SONG: the song the game ended on (NONE: no choice, any key leaves)
        Choice retryStage; // RETRY STAGE: the stage from its first song (NONE: a run started on a later song, not offered)
        bool songFirst = false; // RETRY SONG the default (chosen before in this run), else RETRY STAGE
        bool retryIsStage = false; // `retry` is the stage's (the game ended on its first song): RETRY STAGE alone
    };
    void openDebrief(const Debrief& d);
    const Debrief& debrief() const { return debrief_; }
    // The frame on screen (the bound framebuffer's back buffer) kept as the next debrief's backdrop, frozen and darkened.
    void captureBackdrop(int w, int h);
    void dropBackdrop();
    void setFreeDifficulty(int d) { freeDiff_ = d; }
    // Free play's starting level: the nearest of FREE_LEVELS; `focus` puts the free screen's focus on its row (shots).
    static constexpr int FREE_LEVELS[] = {1, 5, 10, 15, 20, 25, 30, 35, 40};
    static constexpr int FREE_LEVEL_COUNT = sizeof(FREE_LEVELS) / sizeof(FREE_LEVELS[0]);
    void setFreeLevel(int level, bool focus = false);
    int freeLevel() const { return FREE_LEVELS[lvlSel_]; }
    void setCampaignDifficulty(int d) { diff_ = d; }
    void setCursor(int stage, int song) { stage_ = stage; song_ = song; row_ = 1; petal_ = false; }
    // A stage just cleared: once the Voyage's screen is back (after the debrief), its petal turns into its cleared look
    // under the focus, then the focus moves on to the next stage (if the player hasn't moved).
    void celebrateClear(int stage, int next) { celebrate_ = stage; celebrateNext_ = next; celebrateT_ = 0.f; }
    // A stage just opened on difficulty `diff` (by the run that ended): its petal is drawn still locked until its
    // unlock plays, once the Voyage's screen shows that difficulty and no clear is being celebrated (a clear's focus
    // moves on to the next stage, whose unlock then follows); the focus goes to it. The player moving cancels it.
    void celebrateUnlock(int stage, int diff);
    bool unlockToPlay(int stage, int diff) const; // (pending, or playing up to its lock's opening)
    // (until the focus has moved on and the unlock that follows is over, or the player moved)
    bool celebrating() const { return celebrate_ >= 0 || unlock_ >= 0; }
    void snap(); // the animated focus jumps to its place (screenshots)
    // Where the player was in the menu (kept between launches): the title entry, Voyage's difficulty and focused
    // stage, free play's difficulty and playlist.
    struct Place {
        int titleItem = -1, voyageDiff = -1, stage = -1, freeDiff = -1, freeLevel = -1;
        int music = -1; // the menu's music: 1 on, 0 off (-1: not said)
        int theme = -1; // the menu's backdrop: 0 space, 1 the crystal kaleidoscope (-1: not said)
        std::string playlist, voyage; // voyage: the custom voyage open ("" the main one), whose stage `stage` is
        bool operator==(const Place& o) const {
            return titleItem == o.titleItem && voyageDiff == o.voyageDiff && stage == o.stage && freeDiff == o.freeDiff && freeLevel == o.freeLevel &&
                   music == o.music && theme == o.theme && playlist == o.playlist && voyage == o.voyage;
        }
        bool operator!=(const Place& o) const { return !(*this == o); }
    };
    Place place() const;
    // Back to a saved place (the stage clamped to the last unlocked one); the difficulties only when `difficulties`.
    void restore(const Place& p, bool difficulties);
    // Everything as on the game's first launch: the title's first entry, the main voyage's first stage, the default
    // difficulties and playlist (another save slot put in use, or the one in use erased).
    void resetPlace();
    void setInput(const Input* in) { input_ = in; } // (the gamepad page shows its remap)
    bool music() const { return music_; } // the menu's music wanted (OPTIONS > MENU MUSIC)
    // An options page's "done", in its panel for a moment (instead of a message over the menu).
    void note(Screen s, const std::string& text) { screen_ = s; note_ = text; noteT_ = NOTE_TIME; }
    // The opening (the game's first launch, or REFALL_MENU_INTRO=1): on black, the title's disc and its name come up, the
    // petals open round it, then the world and the menu; a key hurries it to its end.
    static constexpr float INTRO_END = 6.f;
    void startIntro() { introT_ = 0.f; }
    static bool loadPlace(const std::string& path, Place& p);
    static bool savePlace(const std::string& path, const Place& p);
    // The save slots (the slots' screen shows what each holds, read again by slotsChanged()); the app's.
    void setSlots(const SaveSlots* s) { slots_ = s; }
    void slotsChanged();
    // Free play's playlists (the free screen picks one); the store is the app's.
    void setPlaylists(PlaylistStore* s) { playlists_ = s; }
    // The custom voyages; `open` makes the app's campaign and progress those of a voyage ("" the main one), false if
    // it can't be read.
    void setVoyages(VoyageStore* s, std::function<bool(const std::string&)> open) { voyages_ = s; openVoyage_ = std::move(open); }
    // A voyage becomes the one the Voyage's screen shows (its focus where it was this session, else on the next stage
    // to play); false if it can't be opened.
    bool selectVoyage(const std::string& id);
    const std::string& voyage() const { return voyageId_; }
    bool editing() const { return editing_; }
    // whether the screen holds still: a game over's debrief, once its words and logo are in (a clear's keeps turning)
    bool still() const;
                                                // (typing a playlist: letters are text, not shortcuts)
    bool fetching() const { return fetch_.valid() || vFetch_.valid(); }
    bool fetchDone() const {
        auto done = [](const auto& f) { return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; };
        return done(fetch_) || done(vFetch_);
    }
    // The campaign has two levels: the stage petals, then the songs of the opened petal.
    void setPetalOpen(bool o) { petal_ = o; }
    // One frame of navigation; a choice ends the menu.
    Choice update(const Input& in, float dt);
    // Draws the screen over the whole bound framebuffer.
    void render(int fbw, int fbh);
    bool screenshot(const std::string& path, int w, int h);

private:
    // ---- drawing: the kaleidoscope (menuwin.cpp), on a painter
    Painter paint_;
    void drawMessage();
    // the debrief (debrief.cpp)
    Debrief debrief_;
    float debT_ = 0, leaveT_ = -1, enterT_ = 1e9f; // seconds into it; since a key ended it; since the menu came back
    GLuint backdrop_ = 0;
    int backW_ = 0, backH_ = 0;
    // the game's logo, the ringed planet (menuwin.cpp): its opening `k` seconds in (99: open), its colours from silver
    // (0, a game over) to full (1)
    void drawOrbitLogo(float cx, float cy, float k, float color);
    int debSel_ = 0;                               // the game over's choice: RETRY SONG, RETRY STAGE (when offered), MENU
    bool debRetry_ = false;                        // a RETRY chosen: the debrief fades out, then the run starts again
    bool updateDebrief(const Input& in, float dt, Choice& out); // true while it shows
    void drawDebrief(int w, int h);
    void drawEnterFade();
    friend struct Worlds; // the kaleidoscope (menuwin.cpp)
    void renderWorlds(int w, int h);
    float u_ = 1, sx_ = 0, sy_ = 0, fw_ = 1920, fh_ = 1080; // the 1920x1080 layout in the frame
    float X(float v) const { return sx_ + v * u_; }
    float Y(float v) const { return sy_ + v * u_; }
    // Portrait (a window taller than wide): the 1920x1080 layout's pieces stacked down a column instead, each placed
    // and scaled on its own (Worlds::pLayout). The column is 1080 units across (pk_ pixels each) and ph_ high; its
    // pieces go between pTop_ and pBottom_ (on the web, above the touch pad, below its buttons).
    bool portrait_ = false;
    float pk_ = 1, ph_ = 1920, pTop_ = 0, pBottom_ = 1920;
    // the layout's point (dx, dy) at the column's point (px, py), `s` column units per layout unit
    void place(float dx, float dy, float px, float py, float s) { u_ = s * pk_; sx_ = (px - dx * s) * pk_; sy_ = (py - dy * s) * pk_; }
    struct SongView {
        bool has = false, locked = false, beaten = false;
        int stage = 0, song = 0, songs = 0, best = 0, lines = -1;
        std::string title, universe, status;
        float velocity = 0;
        unsigned char col[3] = {150, 150, 150};
    };
    SongView songView() const;
    void stageColor(int stage, unsigned char out[3]) const;

    // eased focus (indices as floats) for the animated transitions
    float aTitle_ = 0, aFree_ = DIFF_HARD, aDiff_ = DIFF_NORMAL, aStage_ = 0, aSong_ = 0, aRow_ = 1;
    float aStageTurns_ = 0; // whole turns taken round the Voyage's wheel (keeps the kaleidoscope's spin continuous)
    // the opened petal (its songs fanned out), eased
    bool petal_ = false;
    float aOpen_ = 0;
    // free play's rows: 0 the difficulty, 1 the playlists (the remembered ones, the default, "add one"; a text field
    // to add one), 2 the starting level (an index in FREE_LEVELS)
    PlaylistStore* playlists_ = nullptr;
    int freeRow_ = 0, plSel_ = 1, lvlSel_ = 0; // (plSel_ 1: the electro playlist, free play's default)
    float aFreeRow_ = 0, aPl_ = 0, aLvl_ = 0;
    bool editing_ = false, plMsgBad_ = false;
    std::string editText_, plMsg_;
    struct Fetched { bool ok = false; std::string url, title, error; int count = 0; };
    std::future<Fetched> fetch_;
    std::vector<PlaylistEntry> plChoices() const; // the playlists to pick from, the most recent first
    // the custom voyages: a row of cards (theirs, then "new voyage"); making one: its playlist's link (step 1), being
    // read, then its name (step 2, the playlist's title to start with)
    VoyageStore* voyages_ = nullptr;
    std::function<bool(const std::string&)> openVoyage_;
    std::string voyageId_, voyageTitle_;       // the voyage the Voyage's screen shows ("" the main one)
    std::map<std::string, int> voyageStage_;   // the stage each voyage was on, this session
    int vSel_ = 0, vStep_ = 0;
    int voyageCols() const { return portrait_ ? 1 : 2; } // the cards' grid (one column in portrait)
    float aV_ = 0, aVRow_ = 0;                 // the focused card, its row (eased: the grid scrolls to it)
    std::vector<VoyageProgress> vProg_;        // how far each card's voyage went (read when the screen opens)
    std::string vUrl_, vLink_, vDelete_;       // the playlist read, the link typed, a voyage DEL was pressed on once
    std::vector<std::string> vEntries_;
    struct VFetched { bool ok = false; std::string url, title, error; std::vector<std::string> entries; };
    std::future<VFetched> vFetch_;
    void enterVoyage(std::string id);          // selectVoyage, then its screen

    // ---- state
    const Campaign* camp_ = nullptr;
    Progress* progress_ = nullptr;
    Screen screen_ = TITLE;
    int titleItem_ = 0, freeDiff_ = DIFF_HARD, diff_ = DIFF_NORMAL;
#ifdef REFALL_WEB
    static constexpr int TITLE_ITEMS = 3; // the web build: the campaign only
    static constexpr int TI_VOYAGE = 0, TI_FREE = -1, TI_SLOTS = 1, TI_OPTIONS = 2, TI_QUIT = -1;
#else
    static constexpr int TITLE_ITEMS = 5;
    static constexpr int TI_VOYAGE = 0, TI_FREE = 1, TI_SLOTS = 2, TI_OPTIONS = 3, TI_QUIT = 4;
#endif
    // the options: 0 the gamepad's page, 1 the menu's music on or off, 2 its theme (space or the kaleidoscope; both
    // kept with the menu's place, the same on every save slot)
    static constexpr int OPTION_ITEMS = 3;
    bool music_ = true;
    int theme_ = 0;
    int optSel_ = 0, optYes_ = 0;
    float aOpt_ = 0, aYes_ = 0;
    // the save slots: a card each (left / right), under them what to do with the focused one: 0 use it, 1 unlock
    // every stage, 2 erase it (either asks "are you sure?" first: YES / NO, NO focused)
    static constexpr int SLOT_ACTIONS = 3;
    const SaveSlots* slots_ = nullptr;
    std::vector<SaveSlots::Summary> slotSum_;
    int slotSel_ = 0, slotRow_ = 0;
    float aSlot_ = 0, aSlotRow_ = 0;
    bool slotConfirm_ = false;
    // the gamepad's page: 0 remap it control by control, 1 its default mapping back; while a remap runs (`input_`
    // tells), the control asked for
    static constexpr int PAD_ITEMS = 2;
    int padSel_ = 0;
    float aPad_ = 0;
    const Input* input_ = nullptr;
    // a choice's "done" (SLOT 2 ERASED, GAMEPAD REMAPPED...), told in the options' (or the slots') panel for a moment
    static constexpr float NOTE_TIME = 2.2f;
    std::string note_;
    float noteT_ = 0;
    int row_ = 1;              // campaign: 0 the difficulty row, 1 the stages
    int stage_ = 0, song_ = 0; // campaign cursor
    static constexpr float CELEBRATE_MOVE = 4.8f; // when the focus moves on to the next stage
    int celebrate_ = -1, celebrateNext_ = -1; // the stage just cleared (see celebrateClear), the one to move on to
    float celebrateT_ = 0;
    // the unlocks (see celebrateUnlock): still to play, and the one playing (its time from when the wheel stands on
    // it: it starts at -UNLOCK_LEAD while the wheel turns there; the lock opens at UNLOCK_OPEN, it's over at UNLOCK_END)
    static constexpr float UNLOCK_LEAD = 0.7f, UNLOCK_OPEN = 1.05f, UNLOCK_END = 2.6f;
    struct Unlock { int stage, diff; };
    std::vector<Unlock> unlocks_;
    int unlock_ = -1, unlockDiff_ = 0, unlockRow_ = 1;
    float unlockT_ = 0;
    float time_ = 0, msgT_ = 0, lastInput_ = 0; // lastInput_: when a key was last pressed (the menu's idle time)
    float introT_ = -1.f; // the opening's time (< 0: none)
    std::string msg_, msgSub_;
    bool msgBad_ = false;

    const CampaignSong* hovered() const;
};

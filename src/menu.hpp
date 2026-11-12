#pragma once
// The menus, drawn 8-bit style on a small pixel canvas scaled up with sharp pixels: the title screen (CAMPAIGN,
// FREE PLAY, QUIT), free play's difficulty, and the campaign's stages (a line of squares per stage, one per song,
// coloured by its universe), with an audio preview of the song under the cursor.
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
#include "title.hpp"

class WorldPool;
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
    enum Screen { TITLE, FREE, CAMPAIGN };
    struct Choice {
        enum Kind { NONE, QUIT, PLAY_FREE, PLAY_CAMPAIGN } kind = NONE;
        int difficulty = DIFF_EXPERT;
        int stage = 0, song = 0; // PLAY_CAMPAIGN: where the run starts
        std::string playlist;    // PLAY_FREE: the playlist chosen ("" : the command line's songs or the default)
    };

    Menu();
    ~Menu();
    void init(const Campaign* c, Progress* p);
    // Shows a screen; `message` (e.g. "GAME OVER") shows a moment over it, with `sub` under it.
    void open(Screen s, const std::string& message = "", const std::string& sub = "", bool bad = false);
    Screen screen() const { return screen_; }
    void setFreeDifficulty(int d) { freeDiff_ = d; }
    void setCampaignDifficulty(int d) { diff_ = d; }
    void setCursor(int stage, int song) { stage_ = stage; song_ = song; row_ = 1; petal_ = false; }
    void snap(); // the animated focus jumps to its place (screenshots)
    // Free play's playlists (the free screen picks one); the store is the app's.
    void setPlaylists(PlaylistStore* s) { playlists_ = s; }
    bool editing() const { return editing_; }
                                                // (typing a playlist: letters are text, not shortcuts)
    bool fetching() const { return fetch_.valid(); }
    bool fetchDone() const { return fetch_.valid() && fetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
    // The campaign has two levels: the stage petals, then the songs of the opened petal.
    void setPetalOpen(bool o) { petal_ = o; }
    // One frame of navigation; a choice ends the menu.
    Choice update(const Input& in, float dt);
    // Draws the screen over the whole bound framebuffer.
    void render(int fbw, int fbh);
    bool screenshot(const std::string& path, int w, int h);

private:
    // ---- drawing: the kaleidoscope (menuwin.cpp), on a painter, over live universes (menuworlds.hpp)
    Painter paint_;
    void drawMessage();
    friend struct Worlds; // the kaleidoscope (menuwin.cpp), with live universes
    std::unique_ptr<WorldPool> worlds_;
    void renderWorlds(int w, int h);
    float u_ = 1, sx_ = 0, sy_ = 0, fw_ = 1920, fh_ = 1080; // the 1920x1080 layout in the frame
    float X(float v) const { return sx_ + v * u_; }
    float Y(float v) const { return sy_ + v * u_; }
    struct SongView {
        bool has = false, locked = false, beaten = false;
        int stage = 0, song = 0, songs = 0, best = 0;
        std::string title, universe, status;
        float velocity = 0;
        unsigned char col[3] = {150, 150, 150};
    };
    SongView songView() const;
    void stageColor(int stage, unsigned char out[3]) const;

    // eased focus (indices as floats) for the animated transitions
    float aTitle_ = 0, aFree_ = DIFF_EXPERT, aDiff_ = DIFF_NORMAL, aStage_ = 0, aSong_ = 0, aRow_ = 1;
    // the opened petal (its songs fanned out), eased
    bool petal_ = false;
    float aOpen_ = 0;
    // free play's playlist row: the remembered playlists, the default, "add one"; a text field to add one
    PlaylistStore* playlists_ = nullptr;
    int freeRow_ = 0, plSel_ = 0;
    float aFreeRow_ = 0, aPl_ = 0;
    bool editing_ = false, plMsgBad_ = false;
    std::string editText_, plMsg_;
    struct Fetched { bool ok = false; std::string url, title, error; int count = 0; };
    std::future<Fetched> fetch_;
    std::vector<PlaylistEntry> plChoices() const; // the playlists to pick from, the most recent first

    // ---- state
    const Campaign* camp_ = nullptr;
    Progress* progress_ = nullptr;
    Screen screen_ = TITLE;
    int titleItem_ = 0, freeDiff_ = DIFF_EXPERT, diff_ = DIFF_NORMAL;
    int row_ = 1;              // campaign: 0 the difficulty row, 1 the stages
    int stage_ = 0, song_ = 0; // campaign cursor
    float time_ = 0, msgT_ = 0;
    std::string msg_, msgSub_;
    bool msgBad_ = false;

    const CampaignSong* hovered() const;
};

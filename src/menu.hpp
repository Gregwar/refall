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

class WorldPool;
// The game's name, as the menus' title screens letter it (one place to change it).
inline constexpr const char* GAME_TITLE = "Refall";
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
    // The menus' looks (same screens and navigation): 8-BIT ARCADE (the default), COSMIC, SWISS, WATERCOLOUR, GLASS.
    // The journey variations on COSMIC: GALAXY (3D spiral galaxy), ORRERY (a solar system), ATLAS (an antique star
    // chart), SYNTHWAVE (a neon outrun cosmos), PASTEL (a kawaii candy cosmos).
    // WINDOWS: windows onto the game's real universes, rendered live; KALEIDO: the universes folded in a kaleidoscope.
    enum Style { ARCADE, COSMIC, SWISS, WATERCOLOUR, GLASS, GALAXY, ORRERY, ATLAS, SYNTHWAVE, PASTEL, WINDOWS, KALEIDO, STYLE_COUNT };
    static const char* styleName(int s);
    static int parseStyle(const std::string& s); // a number 1..5 or a name, -1 if unknown
    void setStyle(int s) { style_ = s; }
    struct Choice {
        enum Kind { NONE, QUIT, PLAY_FREE, PLAY_CAMPAIGN } kind = NONE;
        int difficulty = DIFF_EXPERT;
        int stage = 0, song = 0; // PLAY_CAMPAIGN: where the run starts
        std::string playlist;    // PLAY_FREE: the playlist chosen ("" : the command line's songs or the default)
    };

    Menu();
    ~Menu();
    // `audio` plays the previews (null: none, e.g. screenshots).
    void init(const Campaign* c, Progress* p, AudioEngine* audio);
    // Shows a screen; `message` (e.g. "GAME OVER") shows a moment over it, with `sub` under it.
    void open(Screen s, const std::string& message = "", const std::string& sub = "", bool bad = false);
    Screen screen() const { return screen_; }
    void setFreeDifficulty(int d) { freeDiff_ = d; }
    void setCampaignDifficulty(int d) { diff_ = d; }
    void setCursor(int stage, int song) { stage_ = stage; song_ = song; row_ = 1; petal_ = false; }
    void snap(); // the animated focus jumps to its place (screenshots)
    // Free play's playlists (the KALEIDO free screen picks one); the store is the app's.
    void setPlaylists(PlaylistStore* s) { playlists_ = s; }
    bool editing() const { return editing_; }   // typing a playlist: letters are text, not shortcuts
    bool fetching() const { return fetch_.valid(); }
    bool fetchDone() const { return fetch_.valid() && fetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
    // KALEIDO's campaign has two levels: the stage petals, then the songs of the opened petal.
    void setPetalOpen(bool o) { petal_ = o; }
    // One frame of navigation; a choice ends the menu (the preview stops).
    Choice update(const Input& in, float dt);
    // Draws the screen over the whole bound framebuffer.
    void render(int fbw, int fbh);
    bool screenshot(const std::string& path, int w, int h);
    void stopPreview();

private:
    // ---- the pixel canvas
    int cw_ = 400, ch_ = 225, ox_ = 0, oy_ = 0; // canvas size, origin of the 400x225 layout inside it
    std::vector<unsigned char> px_;
    struct Rgb { unsigned char r, g, b; };
    void clear(Rgb c);
    void set(int x, int y, Rgb c);
    void rect(int x, int y, int w, int h, Rgb c);
    void frame(int x, int y, int w, int h, Rgb c);
    int text(const std::string& s, int x, int y, Rgb c, bool big = false, int scale = 1, bool shadow = true);
    int textC(const std::string& s, int cx, int y, Rgb c, bool big = false, int scale = 1, bool shadow = true);
    void drawBackground();
    void drawTitle();
    void drawFree();
    void drawCampaign();
    void drawMessage();
    // the smooth styles (menustyles.cpp)
    Painter paint_;
    void renderSmooth(int w, int h);
    void cosmicTitle(), cosmicFree(), cosmicCampaign();
    void swissTitle(), swissFree(), swissCampaign();
    void paperTitle(), paperFree(), paperCampaign();
    void glassTitle(), glassFree(), glassCampaign();
    void smoothMessage();
    friend struct Cosmos; // the journey styles (menucosmos.cpp)
    void renderCosmos();
    friend struct Worlds; // WINDOWS and KALEIDO (menuwin.cpp), with live universes
    std::unique_ptr<WorldPool> worlds_;
    void renderWorlds(int w, int h);
    float u_ = 1, sx_ = 0, sy_ = 0, fw_ = 1920, fh_ = 1080; // the smooth styles' 1920x1080 layout in the frame
    float X(float v) const { return sx_ + v * u_; }
    float Y(float v) const { return sy_ + v * u_; }
    struct SongView {
        bool has = false, locked = false, beaten = false;
        int stage = 0, song = 0, songs = 0, best = 0;
        std::string title, universe, status, preview;
        float velocity = 0;
        unsigned char col[3] = {150, 150, 150};
    };
    SongView songView() const;
    void stageColor(int stage, unsigned char out[3]) const;

    // ---- GL
    GLuint tex_ = 0, prog_ = 0;
    int style_ = ARCADE;
    // eased focus (indices as floats) for the animated transitions
    float aTitle_ = 0, aFree_ = DIFF_EXPERT, aDiff_ = DIFF_NORMAL, aStage_ = 0, aSong_ = 0, aRow_ = 1;
    float level_ = 0; // the preview's loudness (backgrounds breathe with it)
    // KALEIDO: the opened petal (its songs fanned out), eased
    bool petal_ = false;
    float aOpen_ = 0;
    // free play's playlist row (KALEIDO): the remembered playlists, the default, "add one"; a text field to add one
    PlaylistStore* playlists_ = nullptr;
    int freeRow_ = 0, plSel_ = 0;
    float aFreeRow_ = 0, aPl_ = 0;
    bool editing_ = false, plMsgBad_ = false;
    std::string editText_, plMsg_;
    struct Fetched { bool ok = false; std::string url, title, error; int count = 0; };
    std::future<Fetched> fetch_;
    std::vector<PlaylistEntry> plChoices() const; // the playlists to pick from, the most recent first
    int texW_ = 0, texH_ = 0;

    // ---- state
    const Campaign* camp_ = nullptr;
    Progress* progress_ = nullptr;
    AudioEngine* audio_ = nullptr;
    Screen screen_ = TITLE;
    int titleItem_ = 0, freeDiff_ = DIFF_EXPERT, diff_ = DIFF_NORMAL;
    int row_ = 1;              // campaign: 0 the difficulty row, 1 the stages
    int stage_ = 0, song_ = 0; // campaign cursor
    float time_ = 0, msgT_ = 0, hoverT_ = 0;
    std::string msg_, msgSub_;
    bool msgBad_ = false;

    // ---- previews: a ~14 s snippet from the song's drop (else its chorus, else 35% in), faded and loudness-matched
    struct Snip { std::shared_ptr<Track> track; bool failed = false; };
    std::map<std::string, Snip> snips_;       // by video id
    std::vector<std::string> snipOrder_;      // least recently used first
    std::string loadingId_;                   // the snippet being prepared
    std::future<std::shared_ptr<Track>> loading_;
    std::string playingId_;
    float replayT_ = 0;
    const CampaignSong* hovered() const;
    void updatePreview(float dt);
    std::string previewStatus() const;
};

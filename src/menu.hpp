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

class Menu {
public:
    enum Screen { TITLE, FREE, CAMPAIGN };
    struct Choice {
        enum Kind { NONE, QUIT, PLAY_FREE, PLAY_CAMPAIGN } kind = NONE;
        int difficulty = DIFF_EXPERT;
        int stage = 0, song = 0; // PLAY_CAMPAIGN: where the run starts
    };

    ~Menu();
    // `audio` plays the previews (null: none, e.g. screenshots).
    void init(const Campaign* c, Progress* p, AudioEngine* audio);
    // Shows a screen; `message` (e.g. "GAME OVER") shows a moment over it, with `sub` under it.
    void open(Screen s, const std::string& message = "", const std::string& sub = "", bool bad = false);
    Screen screen() const { return screen_; }
    void setFreeDifficulty(int d) { freeDiff_ = d; }
    void setCampaignDifficulty(int d) { diff_ = d; }
    void setCursor(int stage, int song) { stage_ = stage; song_ = song; row_ = 1; }
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

    // ---- GL
    GLuint tex_ = 0, prog_ = 0;
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

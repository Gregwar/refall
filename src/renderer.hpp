#pragma once
// The host renderer: owns the active universe (universe.hpp) and, during a change of universe, the incoming one;
// renders each into its own texture and composites them with a transition (transitions.hpp). Outside changes,
// only the active universe renders, straight to the window.
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>

#include "layout.hpp"
#include "hud.hpp"
#include "text.hpp"
#include "titlecard.hpp"
#include "transitions.hpp"
#include "ugl.hpp"
#include "universe.hpp"
#include "universes/registry.hpp"

class Renderer {
public:
    ~Renderer();
    bool init(int width, int height);
    void resize(int width, int height);

    // ---- Universes.
    // Switches to universe `index` (registry) at once: no transition.
    void setUniverse(int index, const SongLook& look, int level, float energy);
    // Changes to universe `index` with a transition, started on the next downbeat and lasting a few bars. A change
    // asked while another runs finishes that one first. Universe `index` may be the active one: a fresh instance
    // (e.g. with another variant) replaces it.
    struct Change {
        int transition = -1;   // transitions::Kind, -1: picked from both universes' hints
        transitions::Reason why = transitions::NEW_SCENE;
        float bars = 0;        // length in bars, 0: about 4 s at the song's tempo (1 to 4 bars)
        bool onBeat = true;    // wait for the next downbeat (or beat) to start
    };
    void changeUniverse(int index, const SongLook& look, int level, float energy, const Change& c);
    void changeUniverse(int index, const SongLook& look, int level, float energy) {
        changeUniverse(index, look, level, energy, Change{});
    }
    // A new song in the same universe: the universe performs its own change.
    void setSong(const SongLook& look, float seconds);
    int universe() const;              // the universe shown (the incoming one during a change)
    int variant() const;               // its variant
    std::string label() const;         // "VECTOR · P3 AMBER"
    bool changing() const { return state_ != IDLE; }
    float changeProgress() const { return state_ == RUNNING ? p_ : (state_ == PENDING ? 0.f : -1.f); }
    const char* transitionName() const { return transitions::name(kind_); }

    // ---- Song phase and game, forwarded to the universe(s).
    void setLevel(int level, float energy, float seconds, float impact);
    void refresh(float strength);
    void drop(float strength);
    void onEvent(const GameEvent& ev, const Game& game);
    void kick(float amount);
    void levelUp();
    void setBonus(bool on);
    void setDim(bool d) { dim_ = d; }
    void setPanels(std::vector<HudRect> p) { panels_ = std::move(p); }
    // The unified HUD (hud.hpp): the host letters every universe's HUD in one typographic system, each universe
    // giving only its colors (Universe::hudInk). On by default; ZEN_HUD=universe turns it off at start.
    void setUnifiedHud(bool on) { unified_ = on; }
    bool unifiedHud() const { return unified_; }
    // The speed gauge (the app's HUD): a hairline (pixels, top-left origin: its left end, its width) filled to `pos`
    // (0 .. 1), the hyperdrop's zone from `zone` on (< 0: none), `hot` 0 .. 1 while hyperdrop is on. The host draws
    // it over every universe in the ink of its labels (and their accent for the zone and hyperdrop).
    struct Speed {
        float x = 0, y = 0, w = 0;
        float pos = 0, zone = -1, hot = 0, alpha = 0;
    };
    void setSpeed(const Speed& s) { speed_ = s; }

    // ---- A break between songs (the app's timeline): the game and its HUD fade away, leaving the world alone,
    // the frame fades to black, the host's title card shows the new song, and everything comes back in turn.
    struct Break {
        float play = 1;              // 1: the whole frame; 0: the world alone (a blank board and `bareHud`)
        float black = 0;             // 0 .. 1: the frame fades to black (under the title card)
        const Game* blank = nullptr; // the blank board drawn while the game is hidden (needed when play < 1)
        std::vector<HudText> bareHud; // the HUD while the game is hidden (the corner title)
        TitleCard::Line card[2];     // the title card (the song, the universe), over everything
        // The title card in the universe's own lettering: its corner lines (the title, the universe: the HUD's own
        // entries) captured big once per card (`cardId`), shown big in the middle, then shrinking onto the very
        // spot where the universe's HUD letters them (cardShrink 0 .. 1). Empty: the host's lettering (`card`).
        std::vector<HudText> cardHud;
        int cardId = 0;
        float cardShrink = 0, cardAlpha = 0;
        float cardIn = 0; // the card's lines coming up (0 .. 1, held: a universe the host letters glides them home)
        bool leaving = false; // after a song (the teardown), else before one (the setup)
        float setup = 1, teardown = 0; // the scene's phases (Frame::setup / teardown), on the break's clock
    };
    void setBreak(Break b) { brk_ = std::move(b); }
    const TitleCard& titleCard() const { return card_; }
    void forceFx(int slow, int quadClear);

    void render(const Game& game, const MusicState& music, double time, float dt, bool paused,
                const std::vector<HudText>& hud, float fade);
    // Board space -> window pixels, through the universe shown (blended during a change).
    vec2 project(const vec3& p) const;
    bool screenshot(const std::string& path);
    // The frame just rendered, RGB, rows bottom to top.
    void readFrame(std::vector<unsigned char>& px, int& w, int& h);

    static vec3 gaugeCenter() { return layout::GAUGE_CENTER; }
    static constexpr float GAUGE_R = layout::GAUGE_R;

private:
    struct Slot {
        std::unique_ptr<Universe> u;
        int index = -1;
    };
    Slot make(int index, const SongLook& look, int level, float energy);
    void finishChange();
    template <class F> void each(F f) {
        if (cur_.u) f(*cur_.u);
        if (next_.u) f(*next_.u);
    }

    Slot cur_, next_;
    enum State { IDLE, PENDING, RUNNING } state_ = IDLE;
    int kind_ = 0, lastKind_ = -1;
    float p_ = 0, seconds_ = 3.f, beats_ = 8.f, pendingT_ = 0;
    double startBeat_ = 0;
    bool pendingInit_ = false;
    Change want_;
    uint64_t changes_ = 0;
    ugl::Target texA_, texB_;
    transitions::Compositor comp_;
    GLuint cellsTex_ = 0;
    int level_ = 1;
    float energy_ = 0.5f;
    bool bonus_ = false, dim_ = false;
    std::vector<HudRect> panels_;
    int w_ = 1, h_ = 1;
    Break brk_;
    TitleCard card_;
    GLuint mixProg_ = 0;
    // ---- The slow-motion bonus's common look, over every universe: the world drained to a cold silver, a dark
    // vignette and a pale halo round the well (the well keeps its colors).
    ugl::Target post_;
    GLuint bonusProg_ = 0;
    float bonusK_ = 0.f;
    void bonusLook(const Game& game, double time);
    // ---- The speed gauge, in the label ink of `u` (a universe lettering its own HUD: pale or dark ink on what is
    // behind the gauge).
    Speed speed_;
    GLuint speedProg_ = 0;
    void speedGauge(Universe& u);
    // ---- The bonus gauge, the same in every universe: a fine ring round layout::GAUGE_CENTER in the label ink, its
    // charge running clockwise from 12 o'clock; full, it takes the accent and a soft glow; in the bonus it drains.
    GLuint ringProg_ = 0;
    float ringShown_ = 0.f, ringHot_ = 0.f; // the charge and the full / running look as shown (eased)
    void bonusRing(Universe& u, const Game& game, double time);
    // the label ink and its accent where a label at (x, y) (pixels) is lettered: the unified HUD's, or u's own
    // hudLine's (false: u letters its own HUD, none to read)
    bool labelInks(Universe& u, float x, float y, vec3& ink, vec3& acc);
    // ---- The HUD lettered by the host (Universe::hostText, or every universe in the unified HUD).
    text::Engine text_;
    bool unified_ = false;
    Universe* mixWith_ = nullptr; // the unified HUD during a change: the incoming universe's colors, mixed by mixT_
    float mixT_ = 0.f;
    bool hosted(const Universe& u) const { return unified_ || u.hostText(); }
    // line h in the HUD's style: the unified one in u's colors, else u's own hudLine
    void style(Universe& u, const HudText& h, text::Line& out);
    HudInk inkOf(Universe& u, const HudText& h);
    // what is behind each line, read from the frame (see sampleFrame), each line's side (pale / deep ink) eased
    struct Sampler {
        GLuint tex = 0, fbo = 0, pbo[2] = {0, 0};
        int w = 0, h = 0, cur = 0;
        bool filled[2] = {false, false};
        std::vector<unsigned char> px; // the last frame read, 1/8 scale, RGBA, rows from the bottom
        int pw = 0, ph = 0;
        uint64_t frame = 0;
        struct Side { float deep = 0, target = 0; uint64_t seen = 0; };
        std::unordered_map<uint64_t, Side> lines;
    } smp_;
    float hudDt_ = 0.f;
    void sampleFrame();
    void backdrop(const HudText& h, float x0, float y0, float x1, float y1, hud::Backdrop& out);
    // the HUD handed to u's render(): in the unified HUD, a universe lettering its own gets its lines silent
    // (alpha 0, their text and roles kept for what it reads from them), so it draws no text of its own
    const std::vector<HudText>& quiet(const Universe& u, const std::vector<HudText>& hud, std::vector<HudText>& store) const;
    static Frame withHud(const Frame& f, const std::vector<HudText>& hud);
    // each line of `hud` as `u` styles it, with its box (pixels) where it is lettered
    void hudLines(Universe& u, const std::vector<HudText>& hud, std::vector<text::Line>& lines, std::vector<vec4>& boxes);
    // letters them (alpha x `weight`); `night` 0..1 turns them to white letters on the black (the effects fade)
    void letter(Universe& u, const std::vector<HudText>& hud, float weight, double time, float night = 0.f);
    void letterCard(Universe& u, double time);
    void renderBreak(const Frame& f);
    // ---- The title card captured from the universe's own HUD lettering (see Break::cardHud).
    struct CardLine {
        bool ok = false;
        GLuint tex = 0;          // the lettering: its colors and coverage (RGBA, mipmapped), cropped to its box
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // its box in the capture, pixels (top-left origin), at scale k
        float k = 1, ax = 0, ay = 0;         // the capture's scale and the point it scales about
        float aMax = 1;                      // the lettering's own opacity in the HUD
    };
    CardLine cardLines_[2];
    int cardId_ = -1;
    GLuint cardProg_ = 0;
    void captureCard(const Frame& f);
    bool captureLine(const Frame& f, const HudText& line, bool fromBottom, float bigCap, CardLine& out);
    void drawCard();
};

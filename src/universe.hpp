#pragma once
// A universe: one complete, hand-directed art direction (its world, palette, materials, HUD typography, effects,
// and its own way of staging calm / mid / peak, beats, kicks, line clears, slow motion and game over).
//
// The host (Renderer, renderer.hpp) owns the universes: one renders at a time, two during a change of universe,
// each into the framebuffer the host gives it; the host then composites them with a transition (transitions.hpp).
// Every universe follows the common layout contract (layout.hpp): the well, previews, gauge and HUD anchors sit
// at the same pixels whatever the universe, so a change of universe keeps the stack in place and only changes
// its material and its world.
//
// Adding a universe: see docs/adding-a-universe.md (a folder in src/universes/ and one line in
// src/universes/registry.def, with its weight).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>

#include "analysis.hpp"
#include "game.hpp"
#include "mathutil.hpp"
#include "text.hpp"

// MusicState::inDrop ends this long before a DROP section does.
constexpr double DROP_OUT_LEAD = 1.2;

// Per-frame audio / music state the visuals react to.
struct MusicState {
    FrameFeatures audio;
    float intensity = 0.3f;  // slow section energy
    float beatPhase = 0;     // 0..1 inside current beat
    float beatPulse = 0;     // decaying pulse on each beat
    // Slow multipliers from the song structure (intro sparse, chorus full, builds ramping...).
    float density = 1, speed = 1, glow = 1, saturation = 1;
    float energy = 0.5f;     // overall energy: tempo x section intensity x level (0..1)
    float kick = 0;          // envelope of strong bass hits (fast attack, ~0.3 s decay)
    std::array<float, NUM_BANDS> bandsFast{}; // quickly smoothed band levels (equalizer)
    float bpm = 120.f;       // the song's tempo
    double beat = 0;         // beats since the song's beat grid origin (bars are 4 beats: beat 0, 4, 8... downbeats)
    bool inDrop = false;     // in a DROP section, from its first beat to DROP_OUT_LEAD s before its end
    bool hasSong = false;    // false while no song plays (no beat grid)
};

// What a HUD line is, so a universe can give each kind its own typography (letter spacing, weight, a word of its
// own after the universe's name...). The text, anchor and size stay the host's (the layout contract).
enum HudRole {
    HUD_PLAIN,    // anything else (toasts, hints, menus)
    HUD_TITLE,    // the song's title
    HUD_INFO,     // tempo and key under the title
    HUD_SECTION,  // the song section (VERSE, DROP...)
    HUD_UNIVERSE, // "UNIVERSE · VARIANT"
    HUD_LABEL,    // HOLD, NEXT, SCORE, LINES...
    HUD_VALUE,    // the numbers under the labels
    HUD_NOTICE,   // a short notice floating on its own (combo, seek position)
    HUD_KEY,      // the key to press, in the bonus gauge
    HUD_TOAST,    // a celebration's big word (SLOW MOTION, QUAD, LEVEL 4)
    HUD_TOAST_SUB,// the line under it (+1200)
    HUD_HELP,     // the controls list (one line each)
    HUD_DIALOG,   // the lines of a centered message (paused, game over, loading)
    HUD_HINT,     // corner hints and status (next song, device, errors)
};

struct HudText {
    std::string text;
    float x, y;       // pixels (top-left origin); negative x = right aligned from the right edge
    float scale;
    float alpha;
    bool accent = false;
    bool center = false;
    HudRole role = HUD_PLAIN;
};

// The unified HUD (hud.hpp, the default): the host letters every universe's HUD in one typographic system and
// picks, line by line, the pale or the deep ink from the frame behind the letters; a universe only gives the two
// inks and its accent in its own colors. sRGB.
struct HudInk {
    vec3 pale{0.95f, 0.94f, 0.91f};   // the letters over a dark world
    vec3 deep{0.16f, 0.15f, 0.17f};   // the letters over a light world
    vec3 accent{0.95f, 0.8f, 0.55f};  // the accented lines (a toast, HYPERDROP, the lit choice), either side
};

// A filled rectangle under the HUD text (pixels, top-left origin): the universe's backdrop tone (dark on dark
// universes, light on pale ones), the accent color, or a given color.
struct HudRect {
    float x, y, w, h;
    float alpha;
    bool accent = false;
    bool custom = false; // use `color` (sRGB) instead
    vec3 color{};
};

// What a song brings to a universe: a few song-derived inputs. A universe may pick a variant from them (VECTOR
// picks its phosphor from the key's hue); the rest of its look is its own.
struct SongLook {
    bool hasSong = false;
    uint64_t seed = 0;      // song fingerprint x run seed x scene counter: different every run
    float hue = 0;          // key hue (radians): the key on the circle of fifths, +-30 degrees from the seed
    int key = 0;            // pitch class 0 = C .. 11 = B
    bool minor = false;
    float bpm = 120.f;
    float brightness = 0.5f, bassWeight = 0.5f, airWeight = 0.5f, dynamics = 0.5f, density = 0.5f;
    int variant = -1;       // forced variant index (--universe NAME:VARIANT), -1: the universe picks one
    int avoidVariant = -1;  // a variant not to pick (the one on screen when a universe follows itself), -1: none
    float duration = 0;     // the song's length in seconds (0: unknown)
    float remaining = 0;    // seconds of the song left when this look is handed over (a universe coming in mid-song)
};
SongLook makeSongLook(const Footprint* fp, uint64_t seed); // fp null: no song (loading)

// The pace of the progression universes (GIANT TREE, MEGACITY, LIBRARY...): they complete their scene in
// LINES_PER_MINUTE lines per minute of song, so one song of normal play sees the whole journey whatever its length.
constexpr float LINES_PER_MINUTE = 15.f;
// Their tables are tuned on this many lines (a 3:20 song); a universe scales its lines by SCENE_REF_LINES / sceneLines().
constexpr float SCENE_REF_LINES = 50.f;
constexpr float SCENE_LINES_MIN = 20.f, SCENE_LINES_MAX = 100.f;
// The lines a scene takes for the time left in the song (look.remaining, else its duration): LINES_PER_MINUTE x
// minutes, clamped to [SCENE_LINES_MIN, SCENE_LINES_MAX]; SCENE_REF_LINES when the length is unknown (no song,
// the viewer). Test hooks: ZEN_SCENE_LINES=N forces N, ZEN_LINES_PER_MINUTE=R changes the pace.
float sceneLinesFor(const SongLook& look);

// How a universe looks from afar, used by the host to color transition fronts and to bridge very different
// brightnesses (paper white vs CRT black). Linear RGB.
// Declared once per universe: the transition library is driven only by this data (no code or tuning per pair of
// universes, so any universe pairs with any other).
enum TransitionStyle : unsigned {
    TS_REBUILD = 1u << 0, // the stack re-materializes row by row, then the world follows outward
    TS_RIPPLE = 1u << 1,  // a refracting ring from the board
    TS_INK = 1u << 2,     // an organic dissolve with inked / glowing edges
    TS_CURTAIN = 1u << 3, // the old world opens like curtains from the board's axis
    TS_LUMA = 1u << 4,    // darks first, highlights last
    TS_TILES = 1u << 5,   // board-sized cells flipping like cards, a ring per beat
    TS_ALL = 0xFFFFu,
};
struct TransitionHints {
    vec3 background{0.f};   // dominant backdrop color
    vec3 ink{1.f};          // the board's lines / blocks color (what the stack is drawn with)
    vec3 accent{1.f};       // a highlight color (glows, seals, phosphor)
    bool light = false;     // a light universe (dark marks on a light ground)
    unsigned styles = 0;    // TransitionStyle bits this universe likes to leave / enter by (0: no preference)
};

// The bridge: the neutral state every universe can go to (when leaving) and come from (when entering), defined
// by the host from both universes' hints: the world gone, the well and the stack left in a plain form, drawn in
// `ink` over `color`. Both universes meet there, so any pair stitches without knowing each other.
struct Bridge {
    vec3 color{0.f};   // linear RGB: the backdrop (a blend of both universes' backgrounds)
    vec3 ink{1.f};     // linear RGB: the stack's lines / fill
    float amount = 0;  // 0: fully itself .. 1: fully the bridge
    bool entering = false; // false: leaving (the outgoing universe), true: entering (the incoming one)
};

// Everything a universe needs to draw one frame.
struct Frame {
    const Game& game;
    const MusicState& music;
    double time;                        // seconds since start
    float dt;                           // seconds since the last frame
    bool paused;                        // paused (or waiting for a song): the world holds still, dimmed
    bool dim;                           // dimmed without freezing (behind the game-over screen)
    float fade;                         // 0 black .. 1: startup fade in
    const std::vector<HudText>& hud;    // HUD text, in the universe's own typography
    const std::vector<HudRect>& panels; // rectangles under the HUD text (refall-scene's menu)
    // A break between songs: the game is hidden. Draw the world alone, as if there were no game: no well (its
    // walls, glass, back panel, grid, floor), no hold / next boxes, no bonus gauge, no HUD plates; only the HUD
    // text given in `hud` (the corner title). `game` is then a blank board. The host crossfades this frame with
    // the whole one, so it must keep the same world, camera and light.
    bool bare = false;
    // The host captures the HUD's corner lines, big, for its title card (shown on black): a universe whose lettering
    // adapts to the world behind it (dark ink where the world is light) letters them as over its darkest.
    bool card = false;
    // A universe lettered by the host (Universe::hostText): each HUD line's box as the host letters it (pixels,
    // top-left origin: x0, y0, x1, y1), in `hud`'s order, for a world that makes room for the text.
    const std::vector<vec4>* hudBoxes = nullptr;
};

class Universe {
public:
    virtual ~Universe() = default;
    // Creates the GL resources (the context is current). The destructor frees them.
    virtual bool init(int width, int height) = 0;
    virtual void resize(int width, int height) = 0;
    // The song: before the first frame (seconds 0, instant), then for each new song played in this universe
    // (seconds > 0: the universe performs its own change, e.g. a new phosphor swept in).
    virtual void setSong(const SongLook& look, float seconds) = 0;
    // Song phase: scene level 0 calm, 1 mid, 2 peak, with that phase's energy. seconds <= 0.1: instant (first
    // frame, viewer); else the universe performs its own in-universe transition, in about `seconds`. impact 0..1:
    // how strongly a rising change hits (1: into a drop).
    virtual void setLevel(int level, float energy, float seconds, float impact) = 0;
    // The first downbeat of a drop (the song rises into a peak), on the beat itself (setLevel came ~0.5 s before):
    // the universe's signature hit, 0..1. The peak that follows should stay alive and punchy with the music.
    virtual void drop(float strength) { (void)strength; }
    // A new section inside the same level (chorus -> chorus...), or a level up: a light refresh, 0..1.
    virtual void refresh(float strength) { (void)strength; }
    virtual void onEvent(const GameEvent& ev, const Game& game) = 0;
    // Slow-motion bonus on / off.
    virtual void setBonus(bool on) = 0;
    // The player reached a new level.
    virtual void levelUp() {}
    virtual void kick(float amount) { (void)amount; }
    // Draws the whole frame (world, board, pieces, ghost, previews, gauge, effects, HUD) into `fbo` (the default
    // framebuffer or a host texture, width x height, display-ready sRGB-encoded colors).
    virtual void render(const Frame& f, GLuint fbo) = 0;
    // Board space -> window pixels (top-left origin). Must stay close to layout::project (the layout contract);
    // the default is the layout's reference camera.
    virtual vec2 project(const vec3& p) const;
    virtual TransitionHints hints() const = 0;
    // The HUD's lettering. A universe the host letters (hostText() true; see text.hpp) draws no HUD text itself:
    // after render() the host letters each line of the frame's `hud` as hudLine styles it (its spans from h.text at
    // the size h.scale gives, its inks and effects, read every frame), at the line's anchor (ugl::hudLeft of its
    // width), and so is the title card, gliding from the middle into the very same lines. The default: a plain style.
    virtual bool hostText() const { return false; }
    virtual void hudLine(const HudText& h, text::Line& out);
    // The unified HUD's inks for line h (read every frame, like hudLine). The default: a universe the host letters
    // gives its hudLine's ink (as the pale or the deep one, by its lightness) and accent; one lettering its own HUD
    // gives its hints(): its ink as the deep one on a light universe, its accent. In unified mode a universe
    // lettering its own HUD is handed its lines with alpha 0 (their text and roles kept): it draws none.
    virtual HudInk hudInk(const HudText& h);
    // Optional native exit / entrance: while a change of universe runs, the host calls setBridge every frame
    // before render() (amount rises while leaving, falls while entering; 0 once it's over). A universe that
    // supports it (returns true from bridgeable()) dissolves its own way toward the bridge: its world calms and
    // fades to bridge.color, its stack turns to bridge.ink. Otherwise the host blends its frame toward the bridge.
    virtual bool bridgeable() const { return false; }
    virtual void setBridge(const Bridge& b) { (void)b; }
    // Variants: every universe ships a few curated variants (palettes, materials... VECTOR's phosphors), listed in
    // its registry entry. setSong picks one from the SongLook (look.variant >= 0 forces it). The HUD shows
    // "UNIVERSE · VARIANT", and hints() describes the active variant.
    virtual int variant() const { return 0; }
    // Testing: force the next slow-motion / quad animation (index in its pool, -1 = random).
    virtual void forceFx(int slow, int quadClear) { (void)slow; (void)quadClear; }
    // The host, before setSong: the lines this universe's scene takes from now on (sceneLinesFor). A new target
    // (the next song in the same universe) applies from the lines played so far: what grew stays where it is.
    void setSceneLines(float lines) { pendingScene_ = std::max(1.f, lines); }

protected:
    int width_ = 1, height_ = 1;
    // The lines cleared since this universe came in, for the universes that grow with the play: one coming in
    // mid-game starts small and grows with the lines played in it; a new game starts it again from nothing.
    // (Test hook: ZEN_LINES_HERE=N holds it at N whatever the game does: exact stills of a stage.)
    int linesHere(const Game& g) {
        static const int forced = std::getenv("ZEN_LINES_HERE") ? std::atoi(std::getenv("ZEN_LINES_HERE")) : -1;
        if (forced >= 0) return forced;
        if (lineBase_ < 0 || g.lines() < lineBase_) lineBase_ = g.lines();
        return g.lines() - lineBase_;
    }
    // The lines the scene completes in (LINES_PER_MINUTE x the song's minutes, 20..100; 50 for a 3:20 song).
    float sceneLines() const { return pendingScene_ > 0.f ? pendingScene_ : sceneLines_; }
    // linesHere() on the SCENE_REF_LINES scale the progression tables are written in: SCENE_REF_LINES at scene
    // completion. Each line counts SCENE_REF_LINES / sceneLines() (half as much in a 100-line scene, 2.5 times in a
    // 20-line one). `extra` are real lines added on top (test hooks). When the target changes (a new song), the
    // progress made so far is kept and the new pace applies to the lines that follow (nothing jumps). A new game
    // starts it again from nothing.
    float paceLines(const Game& g, float extra = 0.f) {
        const int here = linesHere(g);
        if (here < paceLast_) { paceLine_ = 0; paceAt_ = 0.f; }
        paceLast_ = here;
        if (pendingScene_ > 0.f) {
            if (paceStarted_) { // the lines so far at the old pace, the next ones at the new (the first call: all at this one)
                paceAt_ += (here - paceLine_) * SCENE_REF_LINES / sceneLines_;
                paceLine_ = here;
            }
            sceneLines_ = pendingScene_;
            pendingScene_ = -1.f;
        }
        paceStarted_ = true;
        const float v = std::max(0.f, paceAt_ + (here - paceLine_ + extra) * SCENE_REF_LINES / sceneLines_);
        return std::round(v * 1024.f) / 1024.f; // (exactly 50 at the target, not 49.99998)
    }
    // Real lines -> the SCENE_REF_LINES scale (a span or a count of lines at the current pace).
    float paceScale() const { return SCENE_REF_LINES / sceneLines(); }

private:
    int lineBase_ = -1;
    float sceneLines_ = SCENE_REF_LINES, pendingScene_ = -1.f, paceAt_ = 0.f;
    int paceLine_ = 0, paceLast_ = 0;
    bool paceStarted_ = false;
};

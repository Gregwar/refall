#pragma once
// NOTEPAD's storyboard: a stick figure living his adventures in the margins, all song long, and dancing through the
// drops (see story.cpp). Pure geometry: lines, colouring and words in board units, each with the song beat the hand
// starts drawing it at; notepad.cpp draws them in its stop motion and rubs out what goes.
#include <cstdint>
#include <string>
#include <vector>

#include "mathutil.hpp"

namespace notepad_story {

// The page's tools (the marks' shader knows them by these numbers).
enum Tool { T_PENCIL = 0, T_PEN = 1, T_MARKER = 2, T_CHALK = 3, T_SMUDGE = 4, T_CPENCIL = 5, T_HIGHLIGHT = 6 };

enum Kind { E_LINE = 0, E_SEGS = 1, E_TEXT = 2 };

// One thing the hand draws: a line (a polyline), a colouring (strokes, in pairs of points, in the order they are
// drawn) or a word. Its id and pose make its key: what moves is drawn anew in each pose (the old one rubbed out).
struct Elem {
    uint32_t id = 0;
    uint32_t pose = 0;
    int kind = E_LINE;
    std::vector<vec2> pts;
    uint32_t col = 0; // sRGB
    int tool = 0;
    float width = 0.05f, alpha = 1.f;
    float t0 = 0.f, dur = 0.f; // song beats: when the hand starts drawing it, and how long it takes (0: at once)
    float residue = 0.3f;      // what the eraser leaves of it when it goes
    std::string text;          // E_TEXT: centred on pts[0], `width` its size
};

// The free page: left of the HUD's left column, right of its right column (board units).
struct Panel { float x0 = 0, x1 = 0, y0 = 0, y1 = 0; };

class Story {
public:
    // A new song: a new run of adventures.
    void reset(uint64_t seed);
    // On each drawing, before build: the song's beat, the stage (0 break, 1 mid, 2 chorus, 3 drop), the panels.
    // `danceAt` starts a drop's dance that many beats in (a test hook).
    void update(double beat, int stage, const Panel P[2], float danceAt = 0.f);
    // The drawing at `beat` in a notebook's tools (0 felt pens, 1 pencil, 2 chalk).
    void build(int variant, double beat, const Panel P[2], std::vector<Elem>& out) const;

    // An episode: one of the guy's adventures, from where the last one left him (positions are his feet, from the
    // panel's bottom middle).
    struct Ep {
        int kind = 0;
        double b0 = 0, b1 = 0;
        int side = 0, dir = 1;
        vec2 p0, p1;
        float rate = 1.f;      // steps (or rungs) a beat
        float h = 0.f;         // a height (the ladder's, the ledge's)
        float ea = 0, eb = 0;  // the ledge he starts on (its ends)
        std::vector<vec2> pts; // the hops' landings
        std::vector<int> seq;  // the tricks, the falling things
        uint32_t id = 0;
    };
    // Something he needs, drawn in before and rubbed out after (panel coordinates).
    struct Prop {
        int kind = 0, side = 0;
        float xa = 0, xb = 0, h = 0;
        double t0 = 0, t1 = 1e9;
        uint32_t id = 0;
    };

private:
    void restart(double beat, int side, float x);
    void extend(double until, int stage, const Panel P[2]);
    Prop* prop(uint32_t id);
    void leave(double when); // the ledge he is on (and its ladder) rubbed out after he left it

    uint64_t seed_ = 1;
    Rng rng_{1};
    uint32_t serial_ = 1;
    float s_ = 1.f; // the guy's size
    std::vector<Ep> eps_;
    std::vector<Prop> props_;
    // where the last episode leaves him
    struct At { int side = 0, dir = 1; float x = 0, y = 0; float la = 0, lb = 0; int recent[4] = {-1, -1, -1, -1}; int sinceCross = 0; };
    At at_;
    std::vector<uint32_t> holding_; // what he stands on (and climbed up by)
    uint32_t ground_ = 0;           // the ground line of his margin
    double chain0_ = -1;
    // the drop's dance
    bool dancing_ = false;
    double dance0_ = 0, danceEnd_ = -1;
    int danceSide_ = 0, danceNo_ = 0;
    uint64_t danceSeed_ = 0;
};

} // namespace notepad_story

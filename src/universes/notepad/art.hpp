#pragma once
// NOTEPAD's drop pictures: one big drawing per notebook, drawn on the free page on both sides of the well while a
// drop lasts (see art.cpp). Pure geometry: the picture as lines, hatching and words in its own units; notepad.cpp
// places it on the page, draws it in stop motion and rubs it out.
#include <cstdint>
#include <string>
#include <vector>

#include "mathutil.hpp"

namespace notepad_art {

// The page's tools (the marks' shader knows them by these numbers).
enum Tool { T_PENCIL = 0, T_PEN = 1, T_MARKER = 2, T_CHALK = 3, T_SMUDGE = 4, T_CPENCIL = 5, T_HIGHLIGHT = 6 };

// The two panels of free page, left and right of the HUD's columns, in the picture's units (one scale for both).
// The left panel's origin is the middle of its bottom edge (its ground); the right one's origin is at the same
// height, `offset` to the right.
struct Panels {
    float hw[2] = {4.75f, 4.75f};             // half widths
    float y0[2] = {0.f, -2.7f}, y1[2] = {21.f, 23.f}; // vertical extents
    vec2 offset{33.f, 0.f};
};

enum Kind { E_LINE = 0, E_SEGS = 1, E_TEXT = 2 };

// One thing the hand draws: a line (a polyline), a colouring (strokes, in pairs of points, in the order they are
// drawn) or a word. Its id (part and rank) and pose make its key: a part that moves is drawn anew in a new pose.
struct Elem {
    uint32_t id = 0;
    uint32_t pose = 0;
    int kind = E_LINE;
    int side = 0;
    int phase = 0; // 0 construction, 1 outline, 2 colour, 3 finish (words, details)
    std::vector<vec2> pts;
    uint32_t col = 0; // sRGB
    int tool = 0;
    float width = 0.05f, alpha = 1.f, dash = 0.f;
    float t0 = 0.f, dur = 0.f; // beats from the picture's start (see schedule)
    std::string text;          // E_TEXT: centred on pts[0], `width` its size (cap to baseline about 0.7 of it)
};

// Beats from the picture's start: it is finished, its moving parts start moving.
constexpr float ALIVE = 30.f;

// The picture of a notebook (0 felt pens, 1 pencil, 2 chalk) for a drop (its number varies it), `beat` beats after
// it started (its moving parts' poses).
void build(int variant, int dropNo, uint64_t seed, float beat, const Panels& P, std::vector<Elem>& out);
// When each thing is drawn: the construction, the outlines, the colouring, the words, each in the order given, at
// the hand's pace, fitted to be finished by ALIVE.
void schedule(std::vector<Elem>& e);

} // namespace notepad_art

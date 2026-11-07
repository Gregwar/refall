#pragma once
// MANGA's page: the panels and the drawings in them (see art.cpp), in board units (y up), so they follow the
// layout's camera. Every drawing is a list of elements, inked one after the other as the song goes on.
#include <cstdint>
#include <vector>

namespace manga {

struct V2 {
    float x = 0, y = 0;
    V2() = default;
    V2(float a, float b) : x(a), y(b) {}
    V2 operator+(const V2& o) const { return {x + o.x, y + o.y}; }
    V2 operator-(const V2& o) const { return {x - o.x, y - o.y}; }
    V2 operator*(float s) const { return {x * s, y * s}; }
};

// Screentones (the fragment shader's kinds).
enum ToneKind { TONE_SOLID = 0, TONE_DOTS = 1, TONE_LINES = 2, TONE_CROSS = 3, TONE_DISC = 4, TONE_SAND = 5 };
// What a fill or a stroke is printed in: the ink, the paper (a knock-out) or the variant's spot color.
enum InkColor { C_INK = 0, C_PAPER = 1, C_SPOT = 2 };

// A screentone: its kind, a density running from d0 at g0 to d1 at g1 (a gradient tone), its cell (board units)
// and angle (radians).
struct Tone {
    int kind = TONE_SOLID;
    float d0 = 1.f, d1 = 1.f;
    V2 g0{0, 0}, g1{0, 1};
    float cell = 0.14f, angle = 0.785398f;
    int color = C_INK;
};

// A filled shape: a polygon fanned from its centroid (convex or star-shaped), or a strip (pairs of points, top
// and bottom, along a silhouette), with its tone.
struct Fill {
    std::vector<V2> pts;
    Tone tone;
    bool strip = false;
};

// An inked line: a polyline with its width (pixels at 1080p) tapered at both ends (taper: the fraction of its
// length that thins out at each end, 0 for a ruled line).
struct Stroke {
    std::vector<V2> pts;
    float width = 1.6f;
    float taper = 0.2f;
    bool closed = false;
    int color = C_INK;
};

// Moving parts the universe animates (a transform or an extra drawn on top).
enum Anim { A_NONE = 0, A_CHIME, A_LAMPS, A_BEAM };

// One step of the inking: a few fills and strokes of one panel.
struct Element {
    int panel = 0;
    std::vector<Fill> fills;
    std::vector<Stroke> strokes;
    int anim = A_NONE;
    V2 pivot{0, 0};
    V2 center{0, 0}; // where its dots fly to
    bool pencil = true; // sketched in pencil before it is inked
};

// A panel: a convex quad (top-left, top-right, bottom-right, bottom-left), its art panels hold a drawing.
struct Panel {
    V2 c[4];
    bool art = false;
    bool shown = true; // false: no room for it on this screen
};

enum PanelId { P_WELL = 0, P_HOLD, P_SCORE, P_NEXT, P_GAUGE, P_A, P_B, P_C, P_D, P_E, P_F, P_COUNT };

struct Page {
    Panel panels[P_COUNT];
    std::vector<Element> elems; // in inking order
    // Anchors for the animated parts.
    V2 lamp[2];
    float lampRadius = 0.3f;
    V2 beamFrom{0, 0};
    bool outer = true;          // the outer columns fit on screen
};

// The page for a screen whose half width is `halfW` board units (the outer columns reach its edge), the score's
// panel reaching down to `scoreBottom` (the HUD's column is taller in board units on small screens).
Page buildPage(float halfW, float scoreBottom, uint64_t seed);

} // namespace manga

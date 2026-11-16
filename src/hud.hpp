#pragma once
// The unified HUD: one typographic system for every universe (the default; REFALL_HUD=universe goes back to each
// universe's own lettering, or Renderer::setUnifiedHud).
//
// One family, the game's own (Josefin Sans, as in the menu), in two weights; a fixed size, weight, case, tracking
// and opacity for each HudRole; the layout is the host's (buildHud's anchors). Only the colors adapt to the world:
// the host samples the frame behind each line (its lightness, its color, how busy it is) and letters it in the
// universe's deep ink on a light world or its pale ink on a dark one (Universe::hudInk), over a soft veil of the
// world's own tone (a feathered halo and a diffuse shadow, never a box), as dense as the world is busy or close
// in lightness to the ink.
#include "text.hpp"
#include "universe.hpp"

namespace hud {

// The unified HUD unless REFALL_HUD=universe in the environment (read once).
bool unifiedFromEnv();

// What is behind a line.
struct Backdrop {
    vec3 color{0.08f, 0.08f, 0.1f}; // its mean color (sRGB)
    float luma = 0.1f;              // its mean lightness
    float busy = 0.3f;              // 0 flat .. 1 very busy (its lightness's spread at the letters' scale)
    float deep = 0.f;               // 0: the pale ink .. 1: the deep ink (eased by the host, with hysteresis)
};

// Line h lettered in the unified style (its anchor and size from h, as Universe::hudLine).
void line(const HudText& h, const HudInk& ink, const Backdrop& back, text::Line& out);

// The two inks mixed (a change of universe: the lettering stays, its colors cross over), t 0..1.
HudInk mix(const HudInk& a, const HudInk& b, float t);

} // namespace hud

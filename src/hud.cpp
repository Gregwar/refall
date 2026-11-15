#include "hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
// Josefin Sans (SIL Open Font License, src/embed/OFL-JosefinSans.txt): the menu's family.
const unsigned char HUD_LIGHT[] = {
#include "universes/host/JosefinSans-Light.ttf.inc"
};
const unsigned char HUD_REGULAR[] = {
#include "universes/host/JosefinSans-Regular.ttf.inc"
};

float luma(const vec3& c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

// The type scale: for each role, its weight, size (x the line's own base, h.scale x 12 px), tracking (em), case
// and opacity (x the line's alpha, lifted a little: a faint line still reads).
struct Style {
    bool regular;   // Regular, else Light
    float size;     // x base
    float track;    // em
    bool caps;
    float opacity;
};
Style styleOf(HudRole r) {
    switch (r) {
    case HUD_TITLE: return {true, 1.05f, 0.02f, false, 1.f};
    case HUD_INFO: return {true, 0.72f, 0.24f, true, 0.9f};
    case HUD_SECTION: return {true, 0.8f, 0.32f, true, 1.f};
    case HUD_UNIVERSE: return {true, 0.8f, 0.32f, true, 1.f};
    case HUD_LABEL: return {true, 0.68f, 0.32f, true, 1.f};
    case HUD_VALUE: return {true, 0.92f, 0.03f, false, 1.f};
    case HUD_NOTICE: return {true, 0.8f, 0.24f, true, 1.f};
    case HUD_KEY: return {true, 0.86f, 0.1f, true, 1.f};
    case HUD_TOAST: return {false, 0.86f, 0.16f, true, 1.f};
    case HUD_TOAST_SUB: return {false, 0.8f, 0.06f, false, 1.f};
    case HUD_HELP: return {true, 0.7f, 0.16f, true, 1.f};
    case HUD_DIALOG: return {false, 0.8f, 0.14f, true, 1.f};
    case HUD_HINT: return {true, 0.72f, 0.22f, true, 1.f};
    case HUD_PLAIN: default: return {true, 0.8f, 0.1f, false, 1.f};
    }
}
} // namespace

namespace hud {

bool unifiedFromEnv() {
    static const bool on = [] {
        const char* v = std::getenv("ZEN_HUD");
        return v && std::strcmp(v, "unified") == 0;
    }();
    return on;
}

namespace {
// the accent on the ink's side: light enough over a dark world, deep enough over a light one
vec3 accentFor(const vec3& acc, bool overDark) {
    const float la = luma(acc);
    if (overDark && la < 0.55f) return lerp(acc, vec3(1.f), saturate((0.55f - la) / std::max(0.05f, 1.f - la)));
    if (!overDark && la > 0.25f) return acc * (0.25f / la);
    return acc;
}
// an ink kept on its side (a universe's pale ink a little too dim, its deep ink a little too light)
vec3 sideOf(const vec3& c, bool pale) {
    const float l = luma(c);
    if (pale && l < 0.7f) return lerp(c, vec3(1.f), saturate((0.7f - l) / std::max(0.05f, 1.f - l)));
    if (!pale && l > 0.12f) return c * (0.12f / l);
    return c;
}
} // namespace

HudInk mix(const HudInk& a, const HudInk& b, float t) {
    HudInk k;
    k.pale = lerp(a.pale, b.pale, t);
    k.deep = lerp(a.deep, b.deep, t);
    k.accent = lerp(a.accent, b.accent, t);
    return k;
}

void line(const HudText& h, const HudInk& ink, const Backdrop& back, text::Line& out) {
    text::Font* light = text::font(HUD_LIGHT);
    text::Font* regular = text::font(HUD_REGULAR);
    const Style st = styleOf(h.role);
    const float b = h.scale * 12.f;
    const float size = b * st.size;
    const float d = saturate(back.deep);
    const vec3 inkC = lerp(sideOf(ink.pale, true), sideOf(ink.deep, false), d);
    const vec3 accC = lerp(accentFor(ink.accent, true), accentFor(ink.accent, false), d);
    const vec3 col = h.accent ? accC : inkC;
    // the corners' and the panel's opacity lifted (1 - (1 - a)^3): their quiet lines still read, a fade still ends
    // at 0; the others keep theirs (a dimmed choice in a menu says it is not the one)
    const float a0 = saturate(h.alpha);
    const bool lift = h.role == HUD_TITLE || h.role == HUD_INFO || h.role == HUD_SECTION || h.role == HUD_UNIVERSE ||
                      h.role == HUD_LABEL || h.role == HUD_VALUE;
    const float a = (lift ? 1.f - (1.f - a0) * (1.f - a0) * (1.f - a0) : a0) * st.opacity;
    if (h.role == HUD_UNIVERSE) {
        // the universe in Regular, its variant after a middle dot in Light, a little dimmer
        std::string name, variant;
        text::splitUniverse(h.text, name, variant);
        out.add(text::upper(name), regular, size, col, st.track, a);
        if (!variant.empty()) {
            out.add(" · ", regular, size, col, 0.3f, a * 0.8f);
            out.add(text::upper(variant), light, size, col, st.track, a * 0.9f);
        }
    } else {
        out.add(st.caps ? text::upper(h.text) : h.text, st.regular ? regular : light, size, col, st.track, a);
    }
    // the letters a little heavier than the font (small sizes over a world), pale ones a hair more (light on dark
    // reads thinner than dark on light)
    out.look.weight = (st.regular ? 0.018f : 0.022f) + (1.f - d) * 0.008f;
    // The veil, in the world's own tone deepened (under pale letters) or lightened (under deep ones): as dense as
    // the world behind is busy, or as close in lightness to the ink; on a calm, contrasted ground almost nothing.
    const float contrast = std::fabs(luma(inkC) - back.luma);
    const float weak = saturate((0.55f - contrast) / 0.4f);
    const float veil = saturate(0.12f + 0.55f * saturate(back.busy) + 0.45f * weak);
    const vec3 under = lerp(back.color * 0.18f, lerp(back.color, vec3(1.f, 0.99f, 0.96f), 0.7f), d);
    out.look.halo = under;
    out.look.haloSmooth = true;
    out.look.haloW = 0.2f;
    out.look.haloA = veil * 0.85f;
    out.look.shadow = under;
    out.look.shadowA = veil * 0.55f;
    out.look.shadowW = 0.12f;
    out.look.shadowSoft = 0.08f;
}

} // namespace hud

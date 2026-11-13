#pragma once
// The text engine: every HUD line is lettered here, by the host, over the universe's frame (always crisp, never
// under its post-processing), and so is the title card gliding into its corners: the same line, the same code, at
// every size. A universe only styles its lines (Universe::hudLine): fonts, case, tracking, inks and effects, and
// for a lettering of its own medium (chalk, ink bleed, pigment...) a small GLSL function.
//
// Fonts are TrueType files built into the game, their glyphs baked on demand (UTF-8: accents and symbols too) as
// signed distance fields at BAKE_PX, shared by everything that letters with the same file. Sizes are pixels; the
// effects' widths are in em (the span's font size), so they scale with the text.
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>

#include "mathutil.hpp"

namespace text {

class Font;
// The font of a built-in TrueType file (its bytes stay alive: the game's embedded arrays). The same bytes give the
// same Font (one atlas), whoever asks. Null when the file can't be read.
Font* font(const unsigned char* ttf, size_t size);
template <size_t N> Font* font(const unsigned char (&ttf)[N]) { return font(ttf, N); }

// A font of the universe's own letter shapes (a pixel font, a stroke font...): `raster` draws a letter's coverage
// (0..255, row by row from the top) for a font size of `px` pixels, its top-left corner at (ox, oy) pixels from the
// pen on the baseline (y down), and gives its advance; false when it has no such letter. The engine turns it into a
// distance field like any other. `cap` and `descent` are the capitals' height and the descent, in em. The same
// `key` gives the same Font.
struct Bitmap {
    int w = 0, h = 0;
    std::vector<unsigned char> cov;
    float ox = 0, oy = 0, adv = 0;
};
using Raster = std::function<bool(uint32_t cp, float px, Bitmap& out)>;
Font* customFont(const std::string& key, float cap, float descent, Raster raster);
// The font's capitals' height, in em (to line up the cap lines of two fonts).
float capHeight(const Font* f);

// A run of text in one font, size and ink.
struct Span {
    std::string text;           // UTF-8
    Font* font = nullptr;
    float size = 16;            // the font size (em), pixels
    float track = 0;            // extra space after each letter, in em
    float dy = 0;               // baseline shift, pixels (down)
    float gap = 0;              // space before the span, in em
    vec3 ink{1, 1, 1};          // sRGB
    float alpha = 1;
};

// The effects of a line, under its letters (widths and offsets in em of each span's size). All optional.
struct Look {
    float weight = 0;           // em: > 0 bolder (the letters grow), < 0 thinner
    float slant = 0;            // synthetic italic: the shear (0.2 ~ 11 degrees)
    // a soft halo of `halo` around the letters (where the world must not show through them): at full `haloA` on
    // their edge, fading out over `haloW` (haloPow 2: a tighter falloff)
    vec3 halo{0, 0, 0};
    float haloA = 0, haloW = 0.08f, haloPow = 1;
    bool haloSmooth = false; // a smoothstep falloff instead (haloPow ignored)
    // a light around the letters, added to what is behind (neon, a lit sign)
    vec3 glow{1, 1, 1};
    float glowA = 0, glowW = 0.12f;
    // a crisp line around the letters (a sticker's rim, a comic's inked letter)
    vec3 outline{0, 0, 0};
    float outlineA = 0, outlineW = 0.04f;
    // a shadow: the letters again under them, moved by shadowOff (em, down and right), grown by shadowW and blurred
    // over shadowSoft
    vec3 shadow{0, 0, 0};
    float shadowA = 0, shadowW = 0, shadowSoft = 0.05f;
    vec2 shadowOff{0, 0};
};

enum Blend {
    BLEND_OVER,     // ink over the frame
    BLEND_MULTIPLY, // ink multiplied into the frame (pencil, ink on paper): the halo / glow effects don't apply
    BLEND_ADD,      // light added to the frame
    BLEND_SCREEN,
};

// A backdrop behind the line: a rounded box (a pill, a caption plate).
struct Plate {
    vec3 color{0, 0, 0};
    float alpha = 0;
    float padX = 0.4f, padY = 0.3f; // em of the first span, around the line's box (cap line to baseline)
    float radius = 0.3f;            // em; a large value: a pill
    vec3 border{0, 0, 0};
    float borderA = 0, borderW = 0.06f; // em
};

// A lettering of its own: GLSL defining
//     vec4 letter(Letter L)
// returning the premultiplied color of a pixel, from
//     struct Letter {
//         float dist;   // signed distance to the letter's edge, pixels (> 0 inside), the weight applied
//         float em;     // pixels per em here
//         vec2 px;      // the pixel (top-left origin)
//         vec2 pos;     // where in the line, em of the first span (0, 0: its left end on the cap line)
//         float glyph;  // the letter's index in the line
//         vec4 ink;     // the span's ink (sRGB) and alpha
//         vec4 std;     // the standard lettering here (Look's effects), premultiplied
//     };
// with the uniforms uTime (seconds), uRes (the screen, pixels) and uP0, uP1 (vec4: Lettering::p0, p1), and the
// helpers hash12(vec2) and noise(vec2) (value noise, 0..1). `move`, optional, is GLSL defining
//     vec2 move(float glyph, vec2 pos)
// each letter's offset (em), for jittered or waving letters (the same uniforms and helpers).
struct Lettering {
    std::string glsl;           // empty: the standard lettering
    std::string move;
    vec4 p0{0, 0, 0, 0}, p1{0, 0, 0, 0};
};

struct Line {
    std::vector<Span> spans;
    Look look;
    Plate plate;
    Blend blend = BLEND_OVER;
    const Lettering* lettering = nullptr; // (the universe keeps it alive)
    float alpha = 1;            // the whole line
    float dx = 0, dy = 0;       // pixels: the line moved from its anchor
    // Convenience: a span appended (with the line's last span's font when none is given).
    Span& add(const std::string& t, Font* f, float size, const vec3& ink, float track = 0, float alpha = 1);
};

// The line's box, pixels: from its anchor (the first span's cap line, at its left end).
struct Box {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // the letters' extent: cap line to baseline (+ descenders in y1)
    float cap = 0;                         // the first span's cap height
    float width() const { return x1 - x0; }
};

// Text transforms the universes use.
std::string upper(const std::string& utf8);
std::string lower(const std::string& utf8);
std::string titleCase(const std::string& utf8); // "VERSE" -> "Verse", "LIMBO" -> "Limbo"
// "NAME · VARIANT" split at its middle dot (variant empty when there is none).
void splitUniverse(const std::string& utf8, std::string& name, std::string& variant);

class Engine {
public:
    ~Engine();
    bool init();
    // The line's box from its anchor (no drawing).
    Box measure(const Line& l) const;
    // Draws the line with its anchor at (x, y), pixels, top-left origin, over the bound framebuffer (w x h).
    void draw(const Line& l, float x, float y, int w, int h, float time);

private:
    struct Prog {
        GLuint prog = 0;
        bool move = false;
    };
    GLuint vao_ = 0, vbo_ = 0, plateProg_ = 0;
    std::unordered_map<std::string, Prog> progs_; // by lettering source ("" the standard)
    std::vector<float> verts_;
    const Prog& prog(const Lettering* l);
};

} // namespace text

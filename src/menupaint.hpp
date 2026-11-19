#pragma once
// A small 2D painter for the menu: anti-aliased rounded rectangles, rings, lines, triangles and soft glows (signed
// distance fields), frosted glass panels, text in the OFL Josefin Sans of src/embed/, and the crystal kaleidoscope.
#include <cstdint>
#include <string>
#include <vector>

#include <glad/gl.h>

#include "ugl.hpp"

struct Col {
    float r = 1, g = 1, b = 1, a = 1;
};
inline Col hex(uint32_t h, float a = 1.f) {
    return {((h >> 16) & 255) / 255.f, ((h >> 8) & 255) / 255.f, (h & 255) / 255.f, a};
}
inline Col rgb8(const unsigned char c[3], float a = 1.f) { return {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, a}; }
inline Col withA(Col c, float a) { c.a = a; return c; }
inline Col mixc(Col a, Col b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

class Painter {
public:
    enum Font { JOSEFIN_LIGHT, JOSEFIN, FONT_COUNT };
    // A flat triangle, a colour per corner.
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c0, Col c1, Col c2);
    // A full-screen kaleidoscope folding two procedural panes of glass in the tint's hues (each from its seed, mixed by
    // `mixAB`) around (cx, cy) pixels into `segs` mirrored segments, turned by `rot`, cut into crystal: small facets and
    // larger shards, each lit from its own side, read through a ramp in the hue of `tint` (each shard leaning to a
    // neighbouring hue), bright ridges along the cuts, a prism's rainbow on some shards, twinkling glints and a light
    // flare in the top right corner; `saturation` < 1 greys them (a locked stage).
    void kaleido(float seedA, float seedB, float mixAB, float cx, float cy, float rot, int segs, Col tint, float time,
                 float level, float saturation = 1.f, float duo = 0.f);
    // An arc of a circle (radians, 0 = right, clockwise on screen), with round ends.
    void arc(float cx, float cy, float r, float a0, float a1, float width, Col c, float soft = 1.f);
    ~Painter();
    void begin(int w, int h);
    void end();
    // Shapes (pixels, top-left origin). `soft` is the edge's blur width (1: anti-aliased, larger: a glow).
    void rrect(float x, float y, float w, float h, float r, Col c, float soft = 1.f);
    void rrectLine(float x, float y, float w, float h, float r, float width, Col c, float soft = 1.f);
    void circle(float cx, float cy, float r, Col c, float soft = 1.f) { rrect(cx - r, cy - r, 2 * r, 2 * r, r, c, soft); }
    void ring(float cx, float cy, float r, float width, Col c, float soft = 1.f) {
        rrectLine(cx - r, cy - r, 2 * r, 2 * r, r, width, c, soft);
    }
    void glow(float cx, float cy, float r, Col c) { circle(cx, cy, r * 0.35f, c, r * 1.3f); }
    void line(float x0, float y0, float x1, float y1, float width, Col c, float soft = 1.f);
    void check(float cx, float cy, float s, float width, Col c); // a check mark
    void lock(float cx, float cy, float s, Col c);               // a padlock
    // A texture (premultiplied RGBA) over a rectangle, its colours multiplied by the tint's, faded by its alpha;
    // `flipY` for a texture read from the framebuffer (its first row at the bottom).
    void image(GLuint tex, float x, float y, float w, float h, Col tint = {}, bool flipY = false);
    // Text on its baseline at y; align -1 left, 0 center, 1 right; tracking in ems. Returns the width.
    float text(Font f, const std::string& s, float x, float y, float size, Col c, int align = -1, float tracking = 0.f);
    float width(Font f, const std::string& s, float size, float tracking = 0.f);
    // The inked extent of a line set left-aligned at x = 0 (its first letter's left edge, its last letter's right edge:
    // no side bearing, no trailing tracking), and the height of the capitals above the baseline.
    void inkExtent(Font f, const std::string& s, float size, float tracking, float& left, float& right);
    float capHeight(Font f, float size);
    // Frosted glass: frostSource() keeps the frame drawn so far, blurred (call it once the background is down);
    // frost() then shows it through a rounded rectangle, a little lifted, mixed toward `tint` by `amount`.
    void frostSource();
    void frost(float x, float y, float w, float h, float r, Col tint, float amount);
    // A frosted glass panel: frost() drawn toward a deep blue by `tint`, a soft shadow under it, a fine light along its
    // top edge and a thin rim, brighter and glowing when `lit` (0..1); `px` is a layout pixel's size.
    void glass(float x, float y, float w, float h, float r, float px, float lit = 0.f, float tint = 0.5f);
    void setAdditive(bool on);
    // An opacity on everything drawn from now on (1: as given), e.g. a scrolled list fading at its edges.
    void setFade(float f) { fade_ = f; }

private:
    struct V { float x, y, lx, ly, hx, hy, r, g, b, a, rad, soft, lw, mode; };
    std::vector<V> verts_;
    ugl::Target kalTgt_; // the kaleidoscope's low-resolution frame
    ugl::Target frostA_, frostB_; // the frost: the frame at a quarter of its size, blurred (A), and a pass's scratch (B)
    GLuint upProg_ = 0, blurProg_ = 0;
    GLuint prog_ = 0, kalProg_ = 0, vao_ = 0, vbo_ = 0, curTex_ = 0;
    int w_ = 1, h_ = 1;
    bool additive_ = false;
    float fade_ = 1.f;
    struct FontData {
        bool tried = false, ok = false;
        GLuint tex = 0;
        float px = 128.f;
        std::vector<unsigned char> packed; // stbtt_packedchar[95]
    } fonts_[FONT_COUNT];
    FontData& font(Font f);
    void quad(const float px[4][2], const float lc[4][2], float hx, float hy, Col c, float rad, float soft, float lw, float mode,
              const Col* bottom = nullptr);
    void flush();
};

#pragma once
// A small 2D painter for the menus' smooth styles: anti-aliased rounded rectangles, rings, lines and soft glows
// (signed distance fields), text in the OFL fonts of src/embed/, and animated full-screen backgrounds.
#include <cstdint>
#include <string>
#include <vector>

#include <glad/gl.h>

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
    enum Font { JOSEFIN_LIGHT, JOSEFIN, INTER_BOLD, INTER, KALAM, YRSA, MANROPE_LIGHT, MANROPE,
                MARCELLUS, CORMORANT_ITALIC, JURA_LIGHT, JURA, COMFORTAA, QUICKSAND, FONT_COUNT };
    enum Background { BG_COSMIC = 1, BG_SWISS, BG_PAPER, BG_GLASS, BG_GALAXY, BG_ORRERY, BG_ATLAS, BG_SYNTH, BG_PASTEL };
    // A shaded sphere (a planet): type 0 rocky, 1 banded gas, 2 ocean world, 3 soft toy (kawaii), 4 a star; `rot`
    // turns it (radians), `seed` varies its surface.
    void sphere(float cx, float cy, float r, Col c, float seed, int type, float rot);
    // A texture (an OpenGL frame, rows bottom to top) in a rounded rectangle, tinted; flipV mirrors it upside down.
    void image(GLuint tex, float x, float y, float w, float h, Col tint, float radius = 0.f, bool flipV = false);
    // A flat triangle, a colour per corner.
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, Col c0, Col c1, Col c2);
    // A full-screen kaleidoscope folding two frames (mixed by `mixAB`) around (cx, cy) pixels into `segs` mirrored
    // segments, turned by `rot`, cut into glass facets with lead lines, tinted toward `tint`.
    void kaleido(GLuint a, GLuint b, float mixAB, float cx, float cy, float rot, int segs, Col tint, float time, float level);
    // An arc of an ellipse (radians, 0 = right, clockwise on screen), as line segments.
    void arc(float cx, float cy, float rx, float ry, float a0, float a1, float width, Col c, float soft = 1.f);
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
    // Text on its baseline at y; align -1 left, 0 center, 1 right; tracking in ems. Returns the width.
    float text(Font f, const std::string& s, float x, float y, float size, Col c, int align = -1, float tracking = 0.f);
    // Text with a vertical gradient (top -> bottom colour) and a slant (shear: x shift per pixel of height).
    float textFx(Font f, const std::string& s, float x, float y, float size, Col top, Col bottom, int align = -1,
                 float tracking = 0.f, float shear = 0.f);
    float width(Font f, const std::string& s, float size, float tracking = 0.f);
    void setAdditive(bool on);
    // A full-screen animated background, drawn at once (call before the shapes).
    void background(int kind, float time, float level, float camX = 0.f, float camY = 0.f);

private:
    struct V { float x, y, lx, ly, hx, hy, r, g, b, a, rad, soft, lw, mode; };
    std::vector<V> verts_;
    GLuint prog_ = 0, bgProg_ = 0, kalProg_ = 0, vao_ = 0, vbo_ = 0, curTex_ = 0;
    int w_ = 1, h_ = 1;
    bool additive_ = false;
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

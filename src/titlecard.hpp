#pragma once
// The title card of a break between songs: the new song's title and its universe, lettered by the host (the
// same in every universe) on the black between two songs, then shrinking into their corners as the new universe
// fades in, where the universe's own HUD takes them over.
#include <string>

#include <glad/gl.h>

#include "mathutil.hpp"

class TitleCard {
public:
    enum Face { SERIF_ITALIC, SANS };
    struct Line {
        std::string text;
        Face face = SANS;
        float x = 0, y = 0;   // pixels, top-left origin: the cap line's left end (center: its middle)
        float cap = 20;       // cap height in pixels
        float track = 0;      // extra letter spacing, in cap heights
        float alpha = 0;
        bool center = false;
    };

    ~TitleCard();
    bool init();
    float width(const Line& l) const; // pixels
    // Draws the lines over the bound framebuffer (premultiplied blending), in `ink` on a soft `halo` (sRGB).
    void draw(const Line* lines, int n, int screenW, int screenH, const vec3& ink, const vec3& halo);

private:
    struct Glyph { float x0, y0, x1, y1, ox, oy, w, h, adv; };
    struct Font {
        bool ok = false;
        GLuint tex = 0;
        float px = 64.f, capH = 40.f;
        Glyph g[96] = {};
    };
    Font fonts_[2];
    GLuint prog_ = 0, vao_ = 0, vbo_ = 0;
    bool build(Font& f, const unsigned char* ttf);
};

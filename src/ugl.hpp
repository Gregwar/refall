#pragma once
// Small shared GL helpers for universes and the host: shader programs, uniforms, render targets, a fullscreen
// triangle, HUD text placement. Nothing here imposes a look.
#include <string>

#include <glad/gl.h>

#include "mathutil.hpp"
#include "universe.hpp"

namespace ugl {

// A fullscreen triangle: vertex shader with `out vec2 vUv` (0..1), drawn by drawFullscreen().
extern const char* FULLSCREEN_VS;
void drawFullscreen();

// Compiles and links a program; errors go to stderr, prefixed by `name`.
GLuint program(const char* vs, const char* fs, const char* name);

inline GLint U(GLuint p, const char* n) { return glGetUniformLocation(p, n); }
inline void set1f(GLuint p, const char* n, float v) { glUniform1f(U(p, n), v); }
inline void set1i(GLuint p, const char* n, int v) { glUniform1i(U(p, n), v); }
inline void set2f(GLuint p, const char* n, float a, float b) { glUniform2f(U(p, n), a, b); }
inline void set3f(GLuint p, const char* n, const vec3& v) { glUniform3f(U(p, n), v.x, v.y, v.z); }
inline void set4f(GLuint p, const char* n, float a, float b, float c, float d) { glUniform4f(U(p, n), a, b, c, d); }
inline void setMat(GLuint p, const char* n, const mat4& m) { glUniformMatrix4fv(U(p, n), 1, GL_FALSE, m.m); }

// An offscreen target: one color texture, optionally a depth (+ stencil) renderbuffer.
struct Target {
    GLuint fbo = 0, tex = 0, depth = 0, depthTex = 0;
    int w = 0, h = 0;
    // internal / format / type as for glTexImage2D (e.g. GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE).
    void create(int width, int height, GLenum internal = GL_RGBA8, GLenum format = GL_RGBA,
                GLenum type = GL_UNSIGNED_BYTE, bool withDepth = false, GLenum filter = GL_LINEAR);
    // a depth texture (32-bit float, nearest) attached in place of the renderbuffer, to be sampled later
    void attachDepthTexture();
    void destroy();
    void bind() const; // binds the framebuffer and sets the viewport
};

// A multisampled target (color + 32-bit float depth renderbuffers), resolved into a Target of the same size.
struct MsTarget {
    GLuint fbo = 0, color = 0, depth = 0;
    int w = 0, h = 0, samples = 0;
    // samples: clamped to what the driver offers
    void create(int width, int height, int samples = 4, GLenum internal = GL_RGBA16F);
    void destroy();
    void bind() const;
    void resolve(const Target& dst, bool withDepth = false) const; // the color (and the depth) averaged into dst
};

// Reduced resolution under full resolution: draws a low-resolution layer (uColor, filtered) with its depth (uDepth,
// a depth texture, taken at the nearest texel) into the current target, so that meshes drawn after are depth tested
// against it. Fragment shader for FULLSCREEN_VS.
extern const char* UPSAMPLE_DEPTH_FS;

// sRGB <-> linear (the 2.2 gamma used everywhere in the game).
inline vec3 toLinear(const vec3& c) { return {std::pow(c.x, 2.2f), std::pow(c.y, 2.2f), std::pow(c.z, 2.2f)}; }
inline vec3 toSrgb(const vec3& c) {
    return {std::pow(std::max(0.f, c.x), 1 / 2.2f), std::pow(std::max(0.f, c.y), 1 / 2.2f), std::pow(std::max(0.f, c.z), 1 / 2.2f)};
}
inline float luma(const vec3& c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

// HUD text placement: the left x of a text of the given width (pixels), from its anchor (center flag, negative x
// = right aligned from the right edge of a screen `screenW` wide).
inline float hudLeft(const HudText& h, float width, int screenW) {
    if (h.center) return h.x - width * 0.5f;
    if (h.x < 0) return screenW + h.x - width;
    return h.x;
}

} // namespace ugl

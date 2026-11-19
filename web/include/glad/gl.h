#pragma once
// The web build's stand-in for glad (third_party/glad): WebGL 2 through Emscripten's OpenGL ES 3.0 headers, its
// functions linked directly (nothing to load), and the few desktop OpenGL 3.3 calls and names the game uses mapped to
// their ES equivalents (or to nothing where WebGL has none).
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <cstring>
#include <unordered_set>
#include <vector>

// Set at start-up (web.cpp) from the extensions the browser gave.
inline bool refallWebClip = false;

typedef void (*GLADapiproc)(void);
typedef GLADapiproc (*GLADloadfunc)(const char* name);
static inline int gladLoadGL(GLADloadfunc) { return 1; }

// no border colour in WebGL: clamped to the edge instead
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#endif
#define GL_TEXTURE_BORDER_COLOR 0x1004
static inline void refallTexParameterfv(GLenum target, GLenum pname, const GLfloat* v) {
    if (pname != GL_TEXTURE_BORDER_COLOR) glTexParameterfv(target, pname, v);
}
#define glTexParameterfv refallTexParameterfv
// points take their size from the vertex shader in ES: nothing to enable
#define GL_PROGRAM_POINT_SIZE 0x8642
// (and clip distances only through their extension)
static inline bool refallCapOk(GLenum cap) { return cap != GL_PROGRAM_POINT_SIZE && (cap != 0x3000 || refallWebClip); }
static inline void refallEnable(GLenum cap) { if (refallCapOk(cap)) glEnable(cap); }
static inline void refallDisable(GLenum cap) { if (refallCapOk(cap)) glDisable(cap); }
#define glEnable refallEnable
#define glDisable refallDisable

static inline void glClearDepth(double d) { glClearDepthf((float)d); }
static inline void glDrawBuffer(GLenum b) { glDrawBuffers(1, &b); }
static inline void glDepthRange(double n, double f) { glDepthRangef((float)n, (float)f); }

// GPU timers (REFALL_GPUTIME, never on the web): the names only
#define GL_TIME_ELAPSED 0x88BF
static inline void glGetQueryObjectui64v(GLuint, GLenum, GLuint64* v) { *v = 0; }

// Through WebGL extensions, enabled at start-up (web/web.cpp) where the browser has them: dual-source blending
// (WEBGL_blend_func_extended) and clip distances (WEBGL_clip_cull_distance); ugl::program adds the shaders'
// #extension lines.
#ifndef GL_SRC1_COLOR
#define GL_SRC1_COLOR 0x88F9
#endif
#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0 0x3000
#endif

// No mapping buffers in WebGL 2: reads go through getBufferSubData instead (renderer.cpp's colour sampling).
extern "C" void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void* data);

// Textures: ES pairs each sized format with fewer types (a 24-bit depth is UNSIGNED_INT, never FLOAT), and a depth
// texture can't be filtered linearly unless it compares (a shadow sampler): those read their nearest texel instead.
inline std::unordered_set<GLuint> refallDepthTextures;
static inline GLuint refallBoundTexture2D() {
    GLint t = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &t);
    return (GLuint)t;
}
static inline void refallTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border,
                                    GLenum format, GLenum type, const void* data) {
    const bool depth = format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL;
    if (internal == GL_DEPTH_COMPONENT24 || internal == GL_DEPTH_COMPONENT16) {
        if (type == GL_FLOAT && !data) type = GL_UNSIGNED_INT;
    }
    glTexImage2D(target, level, internal, w, h, border, format, type, data);
    if (target != GL_TEXTURE_2D) return;
    const GLuint t = refallBoundTexture2D();
    if (depth) {
        refallDepthTextures.insert(t);
        GLint cmp = GL_NONE;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &cmp);
        if (cmp == GL_NONE) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    } else {
        refallDepthTextures.erase(t);
    }
}
#define glTexImage2D refallTexImage2D
static inline void refallTexParameteri(GLenum target, GLenum pname, GLint v) {
    if (target == GL_TEXTURE_2D && (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER) && v != GL_NEAREST &&
        !refallDepthTextures.empty() && refallDepthTextures.count(refallBoundTexture2D())) {
        GLint cmp = GL_NONE;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &cmp);
        if (cmp == GL_NONE) v = GL_NEAREST;
    }
    glTexParameteri(target, pname, v);
    // a depth texture turned into a shadow sampler may filter again
    if (target == GL_TEXTURE_2D && pname == GL_TEXTURE_COMPARE_MODE && v != GL_NONE && !refallDepthTextures.empty() &&
        refallDepthTextures.count(refallBoundTexture2D())) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
}
#define glTexParameteri refallTexParameteri

// Reading pixels: WebGL reads RGBA bytes only (to memory): RGB reads go through an RGBA copy, packed as asked.
inline GLint refallPackAlignment = 4;
static inline void refallPixelStorei(GLenum pname, GLint v) {
    if (pname == GL_PACK_ALIGNMENT) refallPackAlignment = v;
    glPixelStorei(pname, v);
}
#define glPixelStorei refallPixelStorei
static inline void refallReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* data) {
    GLint pbo = 0;
    if (format == GL_RGB && type == GL_UNSIGNED_BYTE) glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    if (format != GL_RGB || type != GL_UNSIGNED_BYTE || pbo || !data) {
        glReadPixels(x, y, w, h, format, type, data);
        return;
    }
    std::vector<unsigned char> rgba((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glPixelStorei(GL_PACK_ALIGNMENT, refallPackAlignment);
    const size_t a = (size_t)refallPackAlignment, stride = ((size_t)w * 3 + a - 1) / a * a;
    unsigned char* out = (unsigned char*)data;
    for (GLsizei r = 0; r < h; r++)
        for (GLsizei c = 0; c < w; c++) std::memcpy(out + r * stride + c * 3, &rgba[((size_t)r * w + c) * 4], 3);
}
#define glReadPixels refallReadPixels

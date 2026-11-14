#include "gles.hpp"

#include <glad/gl.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gles {

bool active() {
    const char* v = (const char*)glGetString(GL_VERSION);
    return v && std::strncmp(v, "OpenGL ES", 9) == 0;
}

// The source with "#version 330 core" turned into ES 3.2 and default precisions (high everywhere: the desktop shaders
// count on 32-bit floats); the result is kept for the program's life (GL copies it, but callers may hold the pointer).
const char* patchShader(const char* src) {
    static std::mutex mu;
    static std::unordered_map<std::string, std::string> cache;
    std::lock_guard<std::mutex> lock(mu);
    auto it = cache.find(src);
    if (it != cache.end()) return it->second.c_str();
    std::string s = src;
    static const char* PREC =
        "precision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp sampler3D;\n"
        "precision highp sampler2DArray;\nprecision highp samplerCube;\nprecision highp isampler2D;\n"
        "precision highp usampler2D;\nprecision highp sampler2DShadow;\n";
    const size_t v = s.find("#version");
    if (v != std::string::npos) {
        const size_t e = s.find('\n', v);
        // (an #extension before anything else but the #version)
        std::string head = "#version 320 es\n";
        if (s.find("gl_ClipDistance") != std::string::npos) head += "#extension GL_EXT_clip_cull_distance : enable\n";
        if (std::regex_search(s, std::regex("index\\s*=\\s*1"))) head += "#extension GL_EXT_blend_func_extended : enable\n"; // (dual-source blending)
        head += PREC;
        s.replace(v, (e == std::string::npos ? s.size() : e + 1) - v, head);
    } else {
        s = std::string("#version 320 es\n") + PREC + s;
    }
    // words reserved in GLSL ES that the desktop drivers let pass as names: renamed
    static const char* RESERVED[] = {"cast", "input", "output", "filter", "sample", "active", "common", "partition",
                                     "half", "fixed", "long", "short", "superp", "sizeof", "namespace", "using",
                                     "packed", "resource", "patch", "union", "enum", "class", "template", "this",
                                     "public", "static", "extern", "external", "interface", "inline", "goto", "shared", "buffer",
                                     "precise", "coherent", "volatile", "restrict", "readonly", "writeonly",
                                     "attribute", "varying", "subroutine", "noinline", "asm", "typedef"};
    for (const char* w : RESERVED) {
        const size_t n = std::strlen(w);
        for (size_t p = s.find(w); p != std::string::npos; p = s.find(w, p + n)) {
            auto idc = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
            if ((p > 0 && idc(s[p - 1])) || (p + n < s.size() && idc(s[p + n]))) continue;
            s.insert(p + n, "_");
            p++;
        }
    }
    return cache.emplace(src, std::move(s)).first->second.c_str();
}

namespace {

PFNGLSHADERSOURCEPROC realShaderSource;
PFNGLTEXIMAGE2DPROC realTexImage2D;
PFNGLRENDERBUFFERSTORAGEPROC realRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC realRenderbufferStorageMS;
PFNGLDRAWBUFFERSPROC realDrawBuffers;
PFNGLREADPIXELSPROC realReadPixels;
void(GLAD_API_PTR* clearDepthf)(GLfloat);
void(GLAD_API_PTR* depthRangef)(GLfloat, GLfloat);
PFNGLGETQUERYOBJECTUI64VPROC queryUi64Ext;

void GLAD_API_PTR shaderSource(GLuint sh, GLsizei n, const GLchar* const* str, const GLint* len) {
    // (the game always passes one null-terminated string)
    std::string all;
    for (GLsizei i = 0; i < n; i++) all += len && len[i] >= 0 ? std::string(str[i], len[i]) : std::string(str[i]);
    const char* p = patchShader(all.c_str());
    realShaderSource(sh, 1, &p, nullptr);
}

// ES takes only some (internal format, format, type) triples; the desktop is lenient: map the ones the game uses
GLint sizedFor(GLint internal, GLenum type) {
    const bool f = type == GL_FLOAT, h = type == GL_HALF_FLOAT;
    switch (internal) {
    case GL_RGBA: return f ? GL_RGBA32F : h ? GL_RGBA16F : GL_RGBA8;
    case GL_RGB: return f ? GL_RGB32F : h ? GL_RGB16F : GL_RGB8;
    case GL_RG: return f ? GL_RG32F : h ? GL_RG16F : GL_RG8;
    case GL_RED: return f ? GL_R32F : h ? GL_R16F : GL_R8;
    case GL_R16: return GL_R16F;     // (no 16-bit normalized formats in core ES)
    case GL_RG16: return GL_RG16F;
    case GL_RGBA16: return GL_RGBA16F;
    case GL_DEPTH_COMPONENT: return GL_DEPTH_COMPONENT24;
    default: return internal;
    }
}

void fixTriple(GLint& internal, GLenum& format, GLenum& type, const void* data) {
    internal = sizedFor(internal, type);
    switch (internal) {
    case GL_RGBA16F: case GL_RGB16F: case GL_RG16F: case GL_R16F:
        if (type != GL_FLOAT && type != GL_HALF_FLOAT) type = data ? GL_FLOAT : GL_HALF_FLOAT;
        break;
    case GL_RGBA32F: case GL_RGB32F: case GL_RG32F: case GL_R32F: case GL_DEPTH_COMPONENT32F:
        type = GL_FLOAT;
        break;
    case GL_RGBA8: case GL_RGB8: case GL_RG8: case GL_R8: case GL_SRGB8_ALPHA8:
        if (!data) type = GL_UNSIGNED_BYTE;
        break;
    case GL_DEPTH_COMPONENT24: case GL_DEPTH_COMPONENT16:
        type = GL_UNSIGNED_INT;
        format = GL_DEPTH_COMPONENT;
        break;
    case GL_DEPTH24_STENCIL8:
        type = GL_UNSIGNED_INT_24_8;
        format = GL_DEPTH_STENCIL;
        break;
    default: break;
    }
}

void GLAD_API_PTR texImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format,
                             GLenum type, const void* data) {
    const GLint in0 = internal;
    const GLenum f0 = format, t0 = type;
    if (data && (in0 == GL_R16 || in0 == GL_RG16 || in0 == GL_RGBA16) && type == GL_UNSIGNED_SHORT) {
        // 16-bit normalized data: as floats
        const int ch = in0 == GL_R16 ? 1 : in0 == GL_RG16 ? 2 : 4;
        const size_t n = (size_t)w * h * ch;
        float* fl = (float*)std::malloc(n * sizeof(float));
        for (size_t i = 0; i < n; i++) fl[i] = ((const unsigned short*)data)[i] / 65535.f;
        realTexImage2D(target, level, sizedFor(in0, GL_FLOAT) == GL_R16F ? GL_R16F : sizedFor(in0, GL_FLOAT), w, h, border, format, GL_FLOAT, fl);
        std::free(fl);
        return;
    }
    fixTriple(internal, format, type, data);
    static const bool dbg = std::getenv("ZEN_GLES_DEBUG") != nullptr;
    if (dbg) while (glGetError()) {}
    realTexImage2D(target, level, internal, w, h, border, format, type, data);
    if (dbg) {
        const GLenum e = glGetError();
        if (e) std::fprintf(stderr, "[gles] glTexImage2D(0x%x, 0x%x, 0x%x) -> (0x%x, 0x%x, 0x%x): error 0x%x\n", in0, f0, t0, internal, format, type, e);
    }
}

void GLAD_API_PTR renderbufferStorage(GLenum target, GLenum internal, GLsizei w, GLsizei h) {
    realRenderbufferStorage(target, (GLenum)sizedFor((GLint)internal, GL_UNSIGNED_BYTE), w, h);
}
void GLAD_API_PTR renderbufferStorageMS(GLenum target, GLsizei samples, GLenum internal, GLsizei w, GLsizei h) {
    realRenderbufferStorageMS(target, samples, (GLenum)sizedFor((GLint)internal, GL_UNSIGNED_BYTE), w, h);
}

// ES reads RGBA only (with GL_UNSIGNED_BYTE): an RGB read goes through RGBA, packed back at the pack alignment
void GLAD_API_PTR readPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* out) {
    if (format != GL_RGB || type != GL_UNSIGNED_BYTE) { realReadPixels(x, y, w, h, format, type, out); return; }
    GLint align = 4, pbo = 0;
    glGetIntegerv(GL_PACK_ALIGNMENT, &align);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
    if (pbo) { realReadPixels(x, y, w, h, format, type, out); return; } // (not used by the game)
    std::vector<unsigned char> rgba((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    realReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glPixelStorei(GL_PACK_ALIGNMENT, align);
    const size_t stride = ((size_t)w * 3 + align - 1) / align * align;
    for (GLsizei r = 0; r < h; r++) {
        unsigned char* d = (unsigned char*)out + r * stride;
        const unsigned char* s4 = rgba.data() + (size_t)r * w * 4;
        for (GLsizei i = 0; i < w; i++) { d[i * 3] = s4[i * 4]; d[i * 3 + 1] = s4[i * 4 + 1]; d[i * 3 + 2] = s4[i * 4 + 2]; }
    }
}

void GLAD_API_PTR drawBuffer(GLenum b) {
    GLint fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if (fbo == 0 && b != GL_NONE) b = GL_BACK;
    realDrawBuffers(1, &b);
}
void GLAD_API_PTR clearDepth(GLdouble d) { if (clearDepthf) clearDepthf((GLfloat)d); }
void GLAD_API_PTR depthRange(GLdouble n, GLdouble f) { if (depthRangef) depthRangef((GLfloat)n, (GLfloat)f); }
void GLAD_API_PTR queryObjectUi64(GLuint id, GLenum pname, GLuint64* out) {
    if (queryUi64Ext) queryUi64Ext(id, pname, out);
    else if (out) *out = 0;
}

} // namespace

void install() {
    realShaderSource = glad_glShaderSource;
    glad_glShaderSource = shaderSource;
    realTexImage2D = glad_glTexImage2D;
    glad_glTexImage2D = texImage2D;
    realRenderbufferStorage = glad_glRenderbufferStorage;
    glad_glRenderbufferStorage = renderbufferStorage;
    realRenderbufferStorageMS = glad_glRenderbufferStorageMultisample;
    if (realRenderbufferStorageMS) glad_glRenderbufferStorageMultisample = renderbufferStorageMS;
    realDrawBuffers = glad_glDrawBuffers;
    realReadPixels = glad_glReadPixels;
    glad_glReadPixels = readPixels;
    glad_glDrawBuffer = drawBuffer;
    glad_glClearDepth = clearDepth;
    glad_glDepthRange = depthRange;
    glad_glGetQueryObjectui64v = queryObjectUi64;
    std::printf("[gles] OpenGL ES: %s, %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
}

// The ES-only functions, found through the platform's loader (set by the caller before install()).
void setLoader(void* (*load)(const char*)) {
    // glad reads "OpenGL ES 3.2" as desktop 3.2 and leaves out the 3.3 functions, which ES 3.0 has
    if (!glad_glVertexAttribDivisor) glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)load("glVertexAttribDivisor");
    if (!glad_glGenSamplers) glad_glGenSamplers = (PFNGLGENSAMPLERSPROC)load("glGenSamplers");
    if (!glad_glDeleteSamplers) glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)load("glDeleteSamplers");
    if (!glad_glBindSampler) glad_glBindSampler = (PFNGLBINDSAMPLERPROC)load("glBindSampler");
    if (!glad_glSamplerParameteri) glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)load("glSamplerParameteri");
    if (!glad_glSamplerParameterf) glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC)load("glSamplerParameterf");
    if (!glad_glSamplerParameterfv) glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC)load("glSamplerParameterfv");
    if (!glad_glIsSampler) glad_glIsSampler = (PFNGLISSAMPLERPROC)load("glIsSampler");
    clearDepthf = (decltype(clearDepthf))load("glClearDepthf");
    depthRangef = (decltype(depthRangef))load("glDepthRangef");
    queryUi64Ext = (PFNGLGETQUERYOBJECTUI64VPROC)load("glGetQueryObjectui64vEXT");
}

} // namespace gles

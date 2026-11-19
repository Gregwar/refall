#include "ugl.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

namespace ugl {

static bool webBlendExt = false, webClipExt = false;
void setWebExtensions(bool blendFuncExtended, bool clipDistance) {
    webBlendExt = blendFuncExtended;
    webClipExt = clipDistance;
}

// GLSL 3.30 core -> GLSL ES 3.00: the same language but for the header (version, precisions, extensions); the
// shaders keep to the part both share.
std::string toGlslEs(const char* src, GLenum type) {
    std::string s = src;
    size_t v = s.find("#version");
    if (v == std::string::npos) return s;
    const size_t eol = s.find('\n', v);
    std::string head = "#version 300 es\n";
    const bool clip = s.find("gl_ClipDistance") != std::string::npos;
    if (clip && webClipExt) head += "#extension GL_ANGLE_clip_cull_distance : require\n";
    if (type == GL_FRAGMENT_SHADER && s.find("index = 1") != std::string::npos)
        head += "#extension GL_EXT_blend_func_extended : require\n";
    head += "precision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp sampler3D;\n"
            "precision highp samplerCube;\nprecision highp sampler2DArray;\nprecision highp sampler2DShadow;\n"
            "precision highp isampler2D;\nprecision highp usampler2D;\n";
    s = s.substr(0, v) + head + (eol == std::string::npos ? std::string() : s.substr(eol + 1));
    // words GLSL ES reserves that desktop drivers let pass as names: renamed (whole words)
    static const char* RESERVED[] = {"half", "patch", "cast", "sample", "filter", "input", "output", "active", "common",
                                     "partition", "resource"};
    auto word = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
    for (const char* r : RESERVED) {
        const size_t n = std::strlen(r);
        for (size_t p = s.find(r); p != std::string::npos; p = s.find(r, p + 1)) {
            if ((p > 0 && word(s[p - 1])) || (p + n < s.size() && word(s[p + n]))) continue;
            s.replace(p, n, std::string("refall_") + r);
            p += 7;
        }
    }
    // no clip distances without the extension: the writes go to a throwaway variable
    if (clip && !webClipExt)
        for (size_t p; (p = s.find("gl_ClipDistance[0]")) != std::string::npos;) s.replace(p, 18, "refallClip0_");
    if (clip && !webClipExt) {
        const size_t m = s.find("void main");
        if (m != std::string::npos) s.insert(m, "float refallClip0_;\n");
    }
    return s;
}

void shaderSource(GLuint shader, GLenum type, const char* src) {
    static const char* dump = std::getenv("REFALL_GLSL_DUMP");
    if (dump && *dump) { // every distinct shader once, named by its hash
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);
        char name[64];
        std::snprintf(name, sizeof(name), "%016zx.%s", std::hash<std::string>{}(src), type == GL_VERTEX_SHADER ? "vert" : "frag");
        std::error_code ec;
        std::filesystem::create_directories(dump, ec);
        const std::filesystem::path f = std::filesystem::path(dump) / name;
        if (!std::filesystem::exists(f, ec))
            if (FILE* o = std::fopen(f.string().c_str(), "wb")) { std::fputs(src, o); std::fclose(o); }
    }
#ifdef REFALL_WEB
    const std::string es = toGlslEs(src, type);
    const char* c = es.c_str();
    glShaderSource(shader, 1, &c, nullptr);
#else
    glShaderSource(shader, 1, &src, nullptr);
#endif
}

const char* FULLSCREEN_VS = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

void drawFullscreen() {
    static GLuint vao = 0;
    if (!vao) glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static GLuint stage(GLenum type, const char* src, const char* name) {
    GLuint s = glCreateShader(type);
    shaderSource(s, type, src);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[gl] %s %s shader error:\n%s\n", name, type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                     log);
    }
    return s;
}

GLuint program(const char* vs, const char* fs, const char* name) {
    GLuint p = glCreateProgram();
    GLuint a = stage(GL_VERTEX_SHADER, vs, name), b = stage(GL_FRAGMENT_SHADER, fs, name);
    glAttachShader(p, a);
    glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[gl] %s link error:\n%s\n", name, log);
    }
    glDeleteShader(a);
    glDeleteShader(b);
    return p;
}

void Target::create(int width, int height, GLenum internal, GLenum format, GLenum type, bool withDepth, GLenum filter) {
    destroy();
    w = std::max(1, width);
    h = std::max(1, height);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (withDepth) {
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[gl] framebuffer incomplete (%dx%d)\n", w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | (withDepth ? GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT : 0));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Target::attachDepthTexture() {
    if (depth) glDeleteRenderbuffers(1, &depth);
    depth = 0;
    if (!depthTex) glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[gl] framebuffer with a depth texture incomplete (%dx%d)\n", w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Target::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
    if (depth) glDeleteRenderbuffers(1, &depth);
    if (depthTex) glDeleteTextures(1, &depthTex);
    fbo = tex = depth = depthTex = 0;
}

void MsTarget::create(int width, int height, int want, GLenum internal) {
    destroy();
    w = std::max(1, width);
    h = std::max(1, height);
    GLint maxS = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxS);
    samples = std::max(1, std::min(want, (int)maxS));
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &color);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internal, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT32F, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::fprintf(stderr, "[gl] multisampled framebuffer incomplete (%dx%d x%d)\n", w, h, samples);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MsTarget::destroy() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (color) glDeleteRenderbuffers(1, &color);
    if (depth) glDeleteRenderbuffers(1, &depth);
    fbo = color = depth = 0;
}

void MsTarget::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

void MsTarget::resolve(const Target& dst, bool withDepth) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, dst.w, dst.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    if (withDepth) glBlitFramebuffer(0, 0, w, h, 0, 0, dst.w, dst.h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

const char* UPSAMPLE_DEPTH_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uColor, uDepth;
void main() {
    frag = texture(uColor, vUv);
    gl_FragDepth = texture(uDepth, vUv).r;
}
)";

void Target::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

} // namespace ugl

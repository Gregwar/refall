#pragma once

// OpenGL ES 3.2 (Android, and ZEN_GLES=1 headless renders that test it on the desktop): the game is written for
// desktop OpenGL 3.3 core; on an ES context, install() wraps the few GL entry points whose rules differ (the shaders'
// #version and precisions, unsized or desktop-only texture formats, glDrawBuffer) so that the rest runs unchanged.
namespace gles {

// whether the GL context is OpenGL ES (from GL_VERSION, once the functions are loaded)
bool active();
// the ES-only functions (glClearDepthf...), through the platform's loader: before install()
void setLoader(void* (*load)(const char*));
// wraps the entry points (after gladLoadGL, when active())
void install();
// the ES version of a desktop GLSL 330 source
const char* patchShader(const char* src);

} // namespace gles

#pragma once
// BIOME's GL helpers, shared by the shell and the worlds: compiling a program with the shared GLSL pieces, a 4x4
// inverse, model matrices, quaternion rotation.
#include <initializer_list>
#include <string>

#include "ugl.hpp"

namespace biome {

// Compiles a program, COMMON_GLSL and then `pre` (in order) inserted after each #version line.
GLuint compile(const char* vs, const char* fs, const char* name, std::initializer_list<const char*> pre);
// The same, the fragment shader assembled from pieces (the cells: head, the world's fill, main).
GLuint compile(const char* vs, const std::string& fs, const char* name, std::initializer_list<const char*> pre);

mat4 inverse(const mat4& m);
// a model matrix: translation, rotation (columns), uniform scale
mat4 model(const vec3& t, const vec3& cx, const vec3& cy, const vec3& cz, float s);
// a quaternion (x, y, z, w) turning a vector
vec3 qRot(const float q[4], const vec3& v);

} // namespace biome

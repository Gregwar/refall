#pragma once
// Headless OpenGL 3.3 (EGL on a GPU device, no window) for offline renders; see headless.cpp.

namespace headless {
// Creates the context and makes it current, with a default framebuffer of width x height. False if unavailable.
bool init(int width, int height);
// For gladLoadGL.
void* proc(const char* name);
void shutdown();
// The context runs on an NVIDIA GPU (its video encoder can take the clips).
bool nvidia();
// A second, independent context (and a tiny pbuffer) for a worker thread: created on the main thread, made current
// (or released) on the worker. Null if unavailable.
void* createWorkerContext();
bool makeWorkerCurrent(void* worker); // null: releases the calling thread's context
} // namespace headless

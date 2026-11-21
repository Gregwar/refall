#pragma once
// The web build (Emscripten, REFALL_WEB): what the browser does differently. The game runs on the page's main
// thread, which never waits: the browser calls the game's loop a frame at a time (requestAnimationFrame); the player's
// files (campaign progress, the menu's place) live in the browser's IndexedDB.
#ifdef REFALL_WEB
#include <functional>

struct GLFWwindow;

namespace web {

// Before anything reads them: the player's folders (<data>/refall) mounted from IndexedDB and filled from it, in the
// background: `then` once they are.
void mountStorage(std::function<void()> then);
// Writes changed files back to IndexedDB (every few seconds at most; `force`: now, e.g. after a run's progress).
void persist(double now, bool force = false);
// The game's loop: `frame` called at each of the browser's frames (requestAnimationFrame), until it returns false.
void runFrames(std::function<bool()> frame);
// The frame drawn (the browser shows it once `frame` returns): its cost sampled for adaptResolution.
void endFrame();
// The WebGL extensions the game uses where the browser has them (float render targets, dual-source blending, clip
// distances), and the canvas following the page's size.
void setupContext(GLFWwindow* win);
void toggleFullscreen(GLFWwindow* win);
// Adaptive resolution, once a frame: the canvas's pixels per CSS pixel lowered by steps while frames come late (60
// frames a second kept at any cost), raised one step at a time: soon when the frame's measured cost (CPU, and GPU where
// the browser times it) leaves room, else after a long calm (a step that brings the late frames back waits twice as
// long before the next try). Learned per scene (a universe, the menu), as their costs differ; starts at the screen's
// own ratio, at most 2. The page hidden or unfocused, and its first 2 s back, aren't measured.
void adaptResolution(int scene);

} // namespace web
#endif

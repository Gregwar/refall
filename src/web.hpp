#pragma once
// The web build (Emscripten, REFALL_WEB): what the browser does differently. The game runs on the page's main
// thread, its loop handing a frame back to the browser at each swap (ASYNCIFY); the player's files (campaign progress,
// the menu's place) live in the browser's IndexedDB.
#ifdef REFALL_WEB
struct GLFWwindow;

namespace web {

// Before anything reads them: the player's folders (<data>/refall) mounted from IndexedDB
// and filled from it (waits for the browser).
void mountStorage();
// Writes changed files back to IndexedDB (every few seconds at most; `force`: now, e.g. after a run's progress).
void persist(double now, bool force = false);
// Hands the frame to the browser and waits for its next one (requestAnimationFrame).
void waitFrame();
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

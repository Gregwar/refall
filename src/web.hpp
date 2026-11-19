#pragma once
// The web build (Emscripten, REFALL_WEB): what the browser does differently. The game runs on the page's main
// thread, its loop handing a frame back to the browser at each swap (ASYNCIFY); the player's files (campaign progress,
// the menu's place, the universes' snapshots) live in the browser's IndexedDB.
#ifdef REFALL_WEB
struct GLFWwindow;

namespace web {

// Before anything reads them: the player's folders (<data>/refall, <cache>/refall/menu-worlds) mounted from IndexedDB
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
// The universes' snapshots made for the site (worlds/*.jpg, by tools/web.sh): fetched in the background into
// WORLDS_DIR, so the menu doesn't make them on the main thread (an instance's init: a hitch, and the music stutters).
inline constexpr const char* WORLDS_DIR = "/refall/worlds";
void fetchWorlds();
bool worldsPending(); // still fetching

} // namespace web
#endif

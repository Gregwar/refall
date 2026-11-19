#include "web.hpp"

#ifdef REFALL_WEB
#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

#include <GLFW/glfw3.h>
#include <GLFW/emscripten_glfw3.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "platform.hpp"
#include "ugl.hpp"


EM_ASYNC_JS(void, refall_mount, (const char* dir), {
    const d = UTF8ToString(dir);
    FS.mkdirTree(d);
    FS.mount(IDBFS, {}, d);
    await new Promise((resolve) => FS.syncfs(true, (err) => {
        if (err) console.warn("[web] storage:", err);
        resolve();
    }));
});

EM_JS(void, refall_sync, (), {
    if (Module.refallSyncing) { Module.refallSyncAgain = true; return; }
    Module.refallSyncing = true;
    FS.syncfs(false, (err) => {
        Module.refallSyncing = false;
        if (err) console.warn("[web] storage:", err);
        if (Module.refallSyncAgain) { Module.refallSyncAgain = false; _refall_sync_again(); }
    });
});

extern "C" EMSCRIPTEN_KEEPALIVE void refall_sync_again() { refall_sync(); }

EM_ASYNC_JS(void, refall_wait_frame, (), {
    await new Promise((resolve) => requestAnimationFrame(resolve));
});

EM_JS(int, refall_extension, (const char* name), {
    const gl = GL.currentContext && GL.currentContext.GLctx;
    return gl && gl.getExtension(UTF8ToString(name)) ? 1 : 0;
});

// The canvas's resolution: GLFW sizes it by window.devicePixelRatio, which the page answers itself from then on
// (the screen's real ratio kept), then tells GLFW it changed.
EM_JS(double, refall_real_dpr, (), {
    if (Module.refallDpr === undefined) {
        Module.refallDpr = window.devicePixelRatio || 1;
        Module.refallScale = Module.refallDpr;
        Object.defineProperty(window, "devicePixelRatio", { get: () => Module.refallScale, configurable: true });
    }
    return Module.refallDpr;
});

EM_JS(void, refall_set_scale, (double s), {
    Module.refallScale = s;
    GLFW3.onScaleChange();
});

namespace {
struct SceneRes {
    int level = -1;     // its step (-1: not met yet)
    double wait = 8.0;  // the calm before a step up is tried
};
std::map<int, SceneRes> g_scenes;
std::vector<double> g_levels; // the ratios, the sharpest first
int g_level = 0, g_scene = -1000000, g_frames = 0, g_slow = 0;
double g_last = 0, g_changed = -1e9, g_calm = 0, g_up = -1e9, g_window = 0;

void applyLevel(int level, double now) {
    g_level = level;
    g_changed = now;
    g_frames = g_slow = 0;
    g_window = now;
    refall_set_scale(g_levels[level]);
    std::printf("[web] resolution x%.2f (step %d of %d)\n", g_levels[level], level + 1, (int)g_levels.size());
}
} // namespace

namespace web {

void adaptResolution(int scene) {
    const double now = emscripten_get_now() / 1000.0;
    if (g_levels.empty()) {
        const double top = std::min(refall_real_dpr(), 2.0);
        for (double s = top; s > 0.5 + 1e-3; s *= 0.85) g_levels.push_back(s);
        if (g_levels.empty() || g_levels.back() > 0.5 + 1e-3) g_levels.push_back(std::min(0.5, top));
        g_last = g_calm = now;
        applyLevel(0, now);
    }
    const double dt = now - g_last;
    g_last = now;
    if (scene != g_scene) { // another scene: its own step (a new one starts from the current)
        if (g_scene != -1000000) g_scenes[g_scene].level = g_level;
        g_scene = scene;
        SceneRes& sr = g_scenes[scene];
        if (sr.level < 0) sr.level = g_level;
        g_calm = now;
        g_up = -1e9;
        if (sr.level != g_level) applyLevel(sr.level, now);
        return;
    }
    // (a change's own hitch and a hidden page aren't counted)
    if (now - g_changed < 0.6 || dt > 2.0) { g_window = now; g_frames = g_slow = 0; return; }
    g_frames++;
    if (dt > 0.0205) g_slow++; // a frame past 1/60 s (a missed refresh)
    if (now - g_window < 0.5 || g_frames < 3) return; // half a second's frames
    const double late = (double)g_slow / g_frames;
    g_frames = g_slow = 0;
    g_window = now;
    SceneRes& sr = g_scenes[scene];
    if (late > 0.15) { // (a song's load, a frame or two, stays under)
        if (now - g_up < 5.0) sr.wait = std::min(sr.wait * 2.0, 240.0); // the step up just tried was too much
        g_calm = now;
        const int down = late > 0.6 ? 2 : 1; // far behind: two steps at once
        if (g_level + 1 < (int)g_levels.size()) applyLevel(std::min((int)g_levels.size() - 1, g_level + down), now);
    } else if (late > 0.03) {
        g_calm = now;
    } else if (g_level > 0 && now - g_calm > sr.wait) {
        g_up = g_calm = now;
        applyLevel(g_level - 1, now);
    }
}


void mountStorage() {
    refall_mount(platform::appDataDir().string().c_str());
    // (the page saves once more when it is closed or hidden)
    EM_ASM({
        const save = () => { if (!Module.refallSyncing) FS.syncfs(false, () => {}); };
        addEventListener("pagehide", save);
        document.addEventListener("visibilitychange", () => { if (document.visibilityState === "hidden") save(); });
    });
}

void persist(double now, bool force) {
    static double last = 0;
    if (!force && now - last < 3.0) return;
    last = now;
    refall_sync();
}

void waitFrame() { refall_wait_frame(); }

void setupContext(GLFWwindow* win) {
    const bool floats = refall_extension("EXT_color_buffer_float");
    refall_extension("EXT_color_buffer_half_float");
    refall_extension("OES_texture_float_linear");
    const bool blend = refall_extension("WEBGL_blend_func_extended");
    const bool clip = refall_extension("WEBGL_clip_cull_distance");
    ugl::setWebExtensions(blend, clip);
    refallWebClip = clip;
    std::printf("[web] float targets %s, dual-source blending %s, clip distances %s\n", floats ? "yes" : "NO",
                blend ? "yes" : "NO", clip ? "yes" : "no");
    emscripten_glfw_make_canvas_resizable(win, "window", nullptr);
    // gamepads the page saw before the game's window existed (the browser announces each one once, at its first
    // button press, e.g. the one that started the game): announced again, now that GLFW listens
    EM_ASM({
        for (const pad of (navigator.getGamepads ? navigator.getGamepads() : []))
            if (pad && pad.connected) {
                const e = new Event("gamepadconnected");
                e.gamepad = pad;
                dispatchEvent(e);
            }
    });
}

void toggleFullscreen(GLFWwindow* win) {
    if (emscripten_glfw_is_window_fullscreen(win)) emscripten_exit_fullscreen();
    else emscripten_glfw_request_fullscreen(win, false, true);
}

} // namespace web
#endif

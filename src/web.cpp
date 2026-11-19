#include "web.hpp"

#ifdef REFALL_WEB
#include <cstdio>
#include <filesystem>

#include <GLFW/glfw3.h>
#include <GLFW/emscripten_glfw3.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "platform.hpp"
#include "ugl.hpp"

namespace fs = std::filesystem;

EM_ASYNC_JS(void, refall_mount, (const char* a, const char* b), {
    const dirs = [UTF8ToString(a), UTF8ToString(b)];
    for (const d of dirs) {
        FS.mkdirTree(d);
        FS.mount(IDBFS, {}, d);
    }
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

// worlds/index.json lists the names; each jpg goes back to the raw RGB the snapshot cache holds (rows as they were)
EM_JS(void, refall_fetch_worlds, (const char* dir, int w, int h), {
    const out = UTF8ToString(dir);
    Module.refallWorldsPending = 1;
    (async () => {
        try {
            FS.mkdirTree(out);
            const names = await (await fetch("worlds/index.json")).json();
            const canvas = new OffscreenCanvas(w, h);
            const ctx = canvas.getContext("2d", { willReadFrequently: true });
            let next = 0;
            const one = async () => {
                while (next < names.length) {
                    const n = names[next++];
                    try {
                        const bmp = await createImageBitmap(await (await fetch("worlds/" + n + ".jpg")).blob());
                        ctx.drawImage(bmp, 0, 0, w, h);
                        bmp.close();
                        const px = ctx.getImageData(0, 0, w, h).data, rgb = new Uint8Array(w * h * 3);
                        for (let i = 0, j = 0; i < px.length; i += 4, j += 3) {
                            rgb[j] = px[i]; rgb[j + 1] = px[i + 1]; rgb[j + 2] = px[i + 2];
                        }
                        FS.writeFile(out + "/" + n + "-v2.rgb", rgb);
                    } catch (e) { console.warn("[web] world " + n + ":", e); }
                }
            };
            await Promise.all([one(), one(), one(), one()]);
        } catch (e) { console.warn("[web] worlds:", e); }
        Module.refallWorldsPending = 0;
    })();
});

EM_JS(int, refall_worlds_pending, (), { return Module.refallWorldsPending ? 1 : 0; });

namespace web {

void fetchWorlds() { refall_fetch_worlds(WORLDS_DIR, 640, 360); }
bool worldsPending() { return refall_worlds_pending(); }

void mountStorage() {
    const fs::path data = platform::appDataDir(), snaps = platform::appCacheDir() / "menu-worlds";
    refall_mount(data.string().c_str(), snaps.string().c_str());
    // (the page saves once more when it is closed or hidden)
    EM_ASM({
        const save = () => { if (!Module.refallSyncing) FS.syncfs(false, () => {}); };
        addEventListener("pagehide", save);
        document.addEventListener("visibilitychange", () => { if (document.visibilityState === "hidden") save(); });
    });
    fetchWorlds();
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

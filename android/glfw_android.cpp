// GLFW on Android: the part of GLFW's API the game uses, over a NativeActivity (android_native_app_glue) and EGL with
// an OpenGL ES 3.2 context. The window is the activity's; a second "window" is a pbuffer sharing the context (the
// menu's worker thread). Touch is read as the keys of a keyboard:
//   tap                  rotate (X) and confirm in menus (keypad Enter)
//   drag left / right    one step (Left / Right) per cell dragged
//   drag down            soft drop (Down) while dragging; a quick flick down: hard drop (Space)
//   flick up             Up (rotate, or up in menus)
//   two-finger tap       hold (C)
//   back button          Escape (pause, back)
// The game's entry point is refall_main(), called from android_main() once the window exists, with HOME set to the
// app's external files folder (adb push songs and caches there: .cache/zentris/...).

#include <GLFW/glfw3.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <thread>

int refall_main(int argc, char** argv);

struct GLFWwindow {
    EGLSurface surf = EGL_NO_SURFACE;
    EGLContext ctx = EGL_NO_CONTEXT;
    bool main = false;
};

namespace {

android_app* g_app = nullptr;
EGLDisplay g_dpy = EGL_NO_DISPLAY;
EGLConfig g_cfg = nullptr;
GLFWwindow g_main;
bool g_haveMain = false, g_focused = false;
std::atomic<bool> g_quit{false};
const auto g_t0 = std::chrono::steady_clock::now();
GLFWcharfun g_charCb = nullptr;
GLFWjoystickfun g_joyCb = nullptr;
int g_hintVisible = GLFW_TRUE;

// keys: held ones, and taps (down for two frames, then up for one)
bool g_held[GLFW_KEY_LAST + 1] = {};
struct Tap { int key, key2, frames; };
std::deque<Tap> g_taps;
bool g_tapDown[GLFW_KEY_LAST + 1] = {};

void tap(int key, int key2 = -1) { g_taps.push_back({key, key2, 0}); }

// the touch gesture in progress
struct Touch {
    bool on = false;
    int id = -1;
    float x0 = 0, y0 = 0, lastX = 0, lastY = 0, stepX = 0;
    double t0 = 0;
    bool moved = false, soft = false, multi = false;
} g_touch;

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_t0).count(); }

float cell() { // a cell's size on screen, about: the well is ~ 22 cells of the height
    const int h = g_app && g_app->window ? ANativeWindow_getHeight(g_app->window) : 1080;
    return h / 24.f;
}

void releaseSoft() {
    if (g_touch.soft) g_held[GLFW_KEY_DOWN] = false;
    g_touch.soft = false;
}

int32_t onInput(android_app*, AInputEvent* ev) {
    const int32_t type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int32_t code = AKeyEvent_getKeyCode(ev), act = AKeyEvent_getAction(ev);
        if (code == AKEYCODE_BACK) {
            if (act == AKEY_EVENT_ACTION_UP) tap(GLFW_KEY_ESCAPE);
            return 1;
        }
        // a hardware keyboard: the arrows, Enter, Space, Escape, letters
        int k = -1;
        switch (code) {
        case AKEYCODE_DPAD_LEFT: k = GLFW_KEY_LEFT; break;
        case AKEYCODE_DPAD_RIGHT: k = GLFW_KEY_RIGHT; break;
        case AKEYCODE_DPAD_UP: k = GLFW_KEY_UP; break;
        case AKEYCODE_DPAD_DOWN: k = GLFW_KEY_DOWN; break;
        case AKEYCODE_ENTER: k = GLFW_KEY_ENTER; break;
        case AKEYCODE_SPACE: k = GLFW_KEY_SPACE; break;
        case AKEYCODE_ESCAPE: k = GLFW_KEY_ESCAPE; break;
        case AKEYCODE_DEL: k = GLFW_KEY_BACKSPACE; break;
        default:
            if (code >= AKEYCODE_A && code <= AKEYCODE_Z) k = GLFW_KEY_A + (code - AKEYCODE_A);
        }
        if (k < 0) return 0;
        g_held[k] = act != AKEY_EVENT_ACTION_UP;
        return 1;
    }
    if (type != AINPUT_EVENT_TYPE_MOTION) return 0;
    const int32_t full = AMotionEvent_getAction(ev), act = full & AMOTION_EVENT_ACTION_MASK;
    const float c = cell();
    if (act == AMOTION_EVENT_ACTION_DOWN) {
        g_touch = Touch{};
        g_touch.on = true;
        g_touch.id = AMotionEvent_getPointerId(ev, 0);
        g_touch.x0 = g_touch.lastX = AMotionEvent_getX(ev, 0);
        g_touch.y0 = g_touch.lastY = AMotionEvent_getY(ev, 0);
        g_touch.t0 = now();
    } else if (act == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        g_touch.multi = true;
    } else if (act == AMOTION_EVENT_ACTION_MOVE && g_touch.on && !g_touch.multi) {
        const float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
        // sideways: a step per cell
        g_touch.stepX += x - g_touch.lastX;
        while (g_touch.stepX > c) { tap(GLFW_KEY_RIGHT); g_touch.stepX -= c; g_touch.moved = true; }
        while (g_touch.stepX < -c) { tap(GLFW_KEY_LEFT); g_touch.stepX += c; g_touch.moved = true; }
        // down, slowly: soft drop while it goes on
        const float dy = y - g_touch.y0, dx = x - g_touch.x0;
        if (dy > 1.5f * c && dy > 1.5f * std::fabs(dx)) {
            g_touch.moved = true;
            if (!g_touch.soft) { g_touch.soft = true; g_held[GLFW_KEY_DOWN] = true; }
        }
        g_touch.lastX = x;
        g_touch.lastY = y;
    } else if (act == AMOTION_EVENT_ACTION_UP || act == AMOTION_EVENT_ACTION_CANCEL) {
        if (g_touch.on && act == AMOTION_EVENT_ACTION_UP) {
            const float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
            const float dx = x - g_touch.x0, dy = y - g_touch.y0, dt = (float)(now() - g_touch.t0);
            const float vy = dy / std::max(dt, 0.01f);
            if (g_touch.multi) tap(GLFW_KEY_C);
            else if (dy > 2.f * c && vy > 25.f * c && dt < 0.35f && std::fabs(dx) < dy) tap(GLFW_KEY_SPACE); // a flick down
            else if (-dy > 2.f * c && -dy > 1.5f * std::fabs(dx) && dt < 0.4f) tap(GLFW_KEY_UP);               // a flick up
            else if (!g_touch.moved && std::fabs(dx) < c && std::fabs(dy) < c && dt < 0.3f) tap(GLFW_KEY_X, GLFW_KEY_KP_ENTER);
        }
        releaseSoft();
        g_touch.on = false;
    }
    return 1;
}

void makeSurface() {
    if (!g_app->window || g_main.surf != EGL_NO_SURFACE) return;
    g_main.surf = eglCreateWindowSurface(g_dpy, g_cfg, g_app->window, nullptr);
    if (g_main.ctx != EGL_NO_CONTEXT) eglMakeCurrent(g_dpy, g_main.surf, g_main.surf, g_main.ctx);
}

void dropSurface() {
    if (g_main.surf == EGL_NO_SURFACE) return;
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(g_dpy, g_main.surf);
    g_main.surf = EGL_NO_SURFACE;
}

void onCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW: if (g_haveMain) makeSurface(); break;
    case APP_CMD_TERM_WINDOW: dropSurface(); break;
    case APP_CMD_GAINED_FOCUS: g_focused = true; break;
    case APP_CMD_LOST_FOCUS:
        if (g_focused) tap(GLFW_KEY_ESCAPE); // (the game pauses)
        g_focused = false;
        break;
    case APP_CMD_DESTROY: g_quit = true; break;
    default: break;
    }
    (void)app;
}

// The activity's pending events. While the window is gone (in the background) it waits for it to come back, once the
// taps queued (the Escape that pauses the game) have been played.
void pump() {
    for (;;) {
        const bool gone = g_haveMain ? g_main.surf == EGL_NO_SURFACE : !g_app->window;
        const bool wait = gone && !g_quit && g_taps.empty();
        int events = 0;
        android_poll_source* src = nullptr;
        const int r = ALooper_pollOnce(wait ? -1 : 0, nullptr, &events, (void**)&src);
        if (src) src->process(g_app, src);
        if (g_app->destroyRequested) g_quit = true;
        if (r < 0 && !wait) break;
    }
}

bool eglSetup() {
    if (g_dpy != EGL_NO_DISPLAY) return true;
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(g_dpy, nullptr, nullptr)) return false;
    const EGLint attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                           EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24,
                           EGL_STENCIL_SIZE, 8, EGL_NONE};
    EGLint n = 0;
    if (!eglChooseConfig(g_dpy, attr, &g_cfg, 1, &n) || n < 1) return false;
    eglBindAPI(EGL_OPENGL_ES_API);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// GLFW

extern "C" {

int glfwInit(void) { return eglSetup() ? GLFW_TRUE : GLFW_FALSE; }
void glfwTerminate(void) {}
void glfwWindowHint(int hint, int value) { if (hint == GLFW_VISIBLE) g_hintVisible = value; }
void glfwWindowHintString(int, const char*) {}
int glfwGetPlatform(void) { return GLFW_PLATFORM_NULL; }

GLFWwindow* glfwCreateWindow(int, int, const char*, GLFWmonitor*, GLFWwindow*) {
    if (!eglSetup()) return nullptr;
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    if (!g_haveMain) {
        g_main.main = true;
        g_main.ctx = eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctxAttr);
        if (g_main.ctx == EGL_NO_CONTEXT) return nullptr;
        g_haveMain = true;
        makeSurface();
        return &g_main;
    }
    // a hidden window: a pbuffer with a context sharing the main one
    GLFWwindow* w = new GLFWwindow;
    w->ctx = eglCreateContext(g_dpy, g_cfg, g_main.ctx, ctxAttr);
    const EGLint pb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    w->surf = eglCreatePbufferSurface(g_dpy, g_cfg, pb);
    if (w->ctx == EGL_NO_CONTEXT || w->surf == EGL_NO_SURFACE) { delete w; return nullptr; }
    return w;
}
void glfwDestroyWindow(GLFWwindow* w) {
    if (!w || w == &g_main) return;
    eglDestroySurface(g_dpy, w->surf);
    eglDestroyContext(g_dpy, w->ctx);
    delete w;
}
void glfwMakeContextCurrent(GLFWwindow* w) {
    eglBindAPI(EGL_OPENGL_ES_API);
    if (!w) eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    else eglMakeCurrent(g_dpy, w->surf, w->surf, w->ctx);
}
void glfwSwapInterval(int i) { eglSwapInterval(g_dpy, i); }
void glfwSwapBuffers(GLFWwindow* w) {
    if (w == &g_main && g_main.surf != EGL_NO_SURFACE) eglSwapBuffers(g_dpy, g_main.surf);
}
GLFWglproc glfwGetProcAddress(const char* name) { return (GLFWglproc)eglGetProcAddress(name); }
int glfwWindowShouldClose(GLFWwindow*) { return g_quit ? GLFW_TRUE : GLFW_FALSE; }
void glfwPollEvents(void) {
    pump();
    // the taps, one after the other: its keys down for two frames, then up for one
    std::memset(g_tapDown, 0, sizeof(g_tapDown));
    if (!g_taps.empty()) {
        Tap& t = g_taps.front();
        if (t.frames < 2) {
            g_tapDown[t.key] = true;
            if (t.key2 >= 0) g_tapDown[t.key2] = true;
        }
        if (++t.frames >= 3) g_taps.pop_front();
    }
}
void glfwGetFramebufferSize(GLFWwindow* w, int* width, int* height) {
    EGLint ww = 1, hh = 1;
    if (w && w->surf != EGL_NO_SURFACE) {
        eglQuerySurface(g_dpy, w->surf, EGL_WIDTH, &ww);
        eglQuerySurface(g_dpy, w->surf, EGL_HEIGHT, &hh);
    }
    if (width) *width = ww;
    if (height) *height = hh;
}
void glfwGetWindowSize(GLFWwindow* w, int* width, int* height) { glfwGetFramebufferSize(w, width, height); }
void glfwGetWindowPos(GLFWwindow*, int* x, int* y) { if (x) *x = 0; if (y) *y = 0; }
GLFWmonitor* glfwGetPrimaryMonitor(void) { return nullptr; }
GLFWmonitor* glfwGetWindowMonitor(GLFWwindow*) { return nullptr; }
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor*) {
    static GLFWvidmode m{1920, 1080, 8, 8, 8, 60};
    glfwGetFramebufferSize(&g_main, &m.width, &m.height);
    return &m;
}
void glfwSetWindowMonitor(GLFWwindow*, GLFWmonitor*, int, int, int, int, int) {}
void glfwSetWindowIcon(GLFWwindow*, int, const GLFWimage*) {}
void glfwSetInputMode(GLFWwindow*, int, int) {}
double glfwGetTime(void) { return now(); }

int glfwGetKey(GLFWwindow*, int key) {
    if (key < 0 || key > GLFW_KEY_LAST) return GLFW_RELEASE;
    return g_held[key] || g_tapDown[key] ? GLFW_PRESS : GLFW_RELEASE;
}
const char* glfwGetKeyName(int, int) { return nullptr; }
int glfwGetMouseButton(GLFWwindow*, int) { return GLFW_RELEASE; }
void glfwGetCursorPos(GLFWwindow*, double* x, double* y) { if (x) *x = 0; if (y) *y = 0; }
GLFWcharfun glfwSetCharCallback(GLFWwindow*, GLFWcharfun cb) { GLFWcharfun o = g_charCb; g_charCb = cb; return o; }
GLFWscrollfun glfwSetScrollCallback(GLFWwindow*, GLFWscrollfun) { return nullptr; }
const char* glfwGetClipboardString(GLFWwindow*) { return nullptr; }

GLFWjoystickfun glfwSetJoystickCallback(GLFWjoystickfun cb) { GLFWjoystickfun o = g_joyCb; g_joyCb = cb; return o; }
int glfwJoystickPresent(int) { return GLFW_FALSE; }
int glfwJoystickIsGamepad(int) { return GLFW_FALSE; }
int glfwUpdateGamepadMappings(const char*) { return GLFW_TRUE; }
const char* glfwGetGamepadName(int) { return nullptr; }
const char* glfwGetJoystickName(int) { return nullptr; }
int glfwGetGamepadState(int, GLFWgamepadstate*) { return GLFW_FALSE; }
const float* glfwGetJoystickAxes(int, int* n) { *n = 0; return nullptr; }
const unsigned char* glfwGetJoystickButtons(int, int* n) { *n = 0; return nullptr; }
const unsigned char* glfwGetJoystickHats(int, int* n) { *n = 0; return nullptr; }

} // extern "C"

// ---------------------------------------------------------------------------------------------------------------------

void android_main(android_app* app) {
    g_app = app;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    // stdout / stderr to the log (adb logcat -s refall)
    static int pfd[2];
    pipe(pfd);
    dup2(pfd[1], 1);
    dup2(pfd[1], 2);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    std::thread([] {
        char buf[1024];
        std::string line;
        ssize_t n;
        while ((n = read(pfd[0], buf, sizeof(buf))) > 0)
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n') { __android_log_write(ANDROID_LOG_INFO, "refall", line.c_str()); line.clear(); }
                else line += buf[i];
            }
    }).detach();
    const char* home = app->activity->externalDataPath ? app->activity->externalDataPath : app->activity->internalDataPath;
    setenv("HOME", home, 1);
    chdir(home);
    std::printf("[android] HOME %s\n", home);
    while (!app->window && !g_quit) pump(); // the window first
    if (g_quit) return;
    char a0[] = "refall";
    char* argv[] = {a0, nullptr};
    refall_main(1, argv);
    ANativeActivity_finish(app->activity);
    while (!app->destroyRequested) pump();
}

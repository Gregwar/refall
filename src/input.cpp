#include "input.hpp"

#include "platform.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(__linux__) && !defined(REFALL_WEB)
#include <chrono>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define REFALL_EVDEV_KEYS 1
#endif

namespace {
Input* g_input = nullptr;
bool g_joystickEvent = false;
void joystickCallback(int, int) { g_joystickEvent = true; }
void charCallback(GLFWwindow*, unsigned int cp) { if (g_input) g_input->onChar(cp); }

// Auto-repeat: delay before repeating, then the repeat interval (close to modern guideline handling).
constexpr float DAS = 0.13f, ARR = 0.033f, SOFT_RATE = 0.033f;

// The remap's controls, in the order they are asked for: what each sets in the gamepad state (a button, or 100 + a
// trigger's axis), its name and what it does.
const int REMAP_TARGET[Input::REMAP_COUNT] = {
    GLFW_GAMEPAD_BUTTON_DPAD_UP, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
    GLFW_GAMEPAD_BUTTON_A, GLFW_GAMEPAD_BUTTON_B, GLFW_GAMEPAD_BUTTON_X, GLFW_GAMEPAD_BUTTON_Y,
    GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, 100 + GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,
    100 + GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, GLFW_GAMEPAD_BUTTON_BACK, GLFW_GAMEPAD_BUTTON_START};
const char* REMAP_NAME[Input::REMAP_COUNT] = {"D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT", "A", "B", "X", "Y",
                                              "LB", "RB", "LT", "RT", "BACK", "START"};
const char* REMAP_USE[Input::REMAP_COUNT] = {"HARD DROP", "SOFT DROP", "MOVE LEFT", "MOVE RIGHT", "ROTATE / OK",
                                             "ROTATE LEFT / BACK", "ROTATE LEFT", "NEW SCENE", "HOLD", "HOLD",
                                             "SLOW MOTION", "SLOW MOTION", "NEXT SONG", "PAUSE"};

// A controller's keys that are no joystick buttons (an Xbox pad over Bluetooth sends its View button as the keyboard's
// "Back" key): GLFW does not list them, and worse, reads them as the controller's button 0 (it indexes its table of
// buttons below its start). They are read here from the device itself, so that they can be remapped (the source 'k'),
// and button 0 is not believed while one of them is down.
#ifdef REFALL_EVDEV_KEYS
struct PadKeys {
    int fd = -1;
    std::string guid; // as GLFW names the controller
    std::vector<int> down;
    std::chrono::steady_clock::time_point last{};
};
std::vector<PadKeys> g_padKeys;

void scanPadKeys() {
    for (PadKeys& d : g_padKeys) close(d.fd);
    g_padKeys.clear();
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator("/dev/input", ec)) {
        if (e.path().filename().string().rfind("event", 0) != 0) continue;
        const int fd = open(e.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        unsigned char keys[(KEY_CNT + 7) / 8] = {};
        struct input_id id {};
        bool low = false, joy = false;
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && ioctl(fd, EVIOCGID, &id) >= 0) {
            auto has = [&](int c) { return (keys[c / 8] >> (c % 8)) & 1; };
            for (int c = 1; c < BTN_MISC; c++) low |= has(c);
            for (int c = BTN_JOYSTICK; c < BTN_DIGI; c++) joy |= has(c); // (a controller only: never a keyboard)
        }
        if (!low || !joy || !id.vendor || !id.product || !id.version) { close(fd); continue; }
        char g[40];
        std::snprintf(g, sizeof(g), "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000", id.bustype & 0xff, id.bustype >> 8,
                      id.vendor & 0xff, id.vendor >> 8, id.product & 0xff, id.product >> 8, id.version & 0xff, id.version >> 8);
        PadKeys d;
        d.fd = fd;
        d.guid = g;
        g_padKeys.push_back(d);
    }
}

// The controller's keys now (read at each call: after GLFW read its buttons, nothing it saw is missed here).
PadKeys* padKeys(const std::string& guid) {
    PadKeys* found = nullptr;
    for (PadKeys& d : g_padKeys) {
        if (d.guid != guid) continue;
        struct input_event ev[16];
        for (ssize_t n; (n = read(d.fd, ev, sizeof(ev))) > 0;)
            for (size_t i = 0; i < n / sizeof(ev[0]); i++) {
                if (ev[i].type != EV_KEY || ev[i].code >= BTN_MISC) continue;
                d.down.erase(std::remove(d.down.begin(), d.down.end(), (int)ev[i].code), d.down.end());
                if (ev[i].value) d.down.push_back(ev[i].code);
                d.last = std::chrono::steady_clock::now();
            }
        if (!found || !d.down.empty()) found = &d;
    }
    return found;
}
// one of them held (0: none)
int padKeyDown(const std::string& guid) {
    const PadKeys* d = padKeys(guid);
    return d && !d->down.empty() ? d->down.front() : 0;
}
bool padKeyOn(const std::string& guid, int code) {
    const PadKeys* d = padKeys(guid);
    return d && std::find(d->down.begin(), d->down.end(), code) != d->down.end();
}
// whether the controller's button 0 is one of these keys' echo (while it is held, and a moment after)
bool padKeyEcho(const std::string& guid) {
    const PadKeys* d = padKeys(guid);
    return d && (!d->down.empty() || std::chrono::steady_clock::now() - d->last < std::chrono::milliseconds(120));
}
#else
void scanPadKeys() {}
int padKeyDown(const std::string&) { return 0; }
bool padKeyOn(const std::string&, int) { return false; }
bool padKeyEcho(const std::string&) { return false; }
#endif
} // namespace

const char* Input::remapName(int step) { return step >= 0 && step < REMAP_COUNT ? REMAP_NAME[step] : ""; }
const char* Input::remapUse(int step) { return step >= 0 && step < REMAP_COUNT ? REMAP_USE[step] : ""; }

std::string Input::guidOf(int jid) const {
    const char* g = glfwGetJoystickGUID(jid);
    if (g && g[0]) return g;
    const char* n = glfwGetJoystickName(jid);
    return n ? n : "joystick";
}

// One line per controller: its GUID, then each control's source ("-" none, b<button>, h<hat>:<bits>,
// a<axis>:<side>:<rest>, k<key>: a key of the controller that is no joystick button).
void Input::setMappingFile(const std::string& path) {
    mapPath_ = path;
    maps_.clear();
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string guid, tok;
        if (!std::getline(ls, guid, '\t') || guid.empty()) continue;
        std::vector<Source> v;
        while (std::getline(ls, tok, '\t') && (int)v.size() < REMAP_COUNT) {
            Source s;
            if (tok.size() > 1 && (tok[0] == 'b' || tok[0] == 'k')) { s.kind = tok[0]; s.index = std::atoi(tok.c_str() + 1); }
            else if (tok.size() > 1 && tok[0] == 'h') { s.kind = 'h'; std::sscanf(tok.c_str() + 1, "%d:%d", &s.index, &s.dir); }
            else if (tok.size() > 1 && tok[0] == 'a') { s.kind = 'a'; std::sscanf(tok.c_str() + 1, "%d:%d:%f", &s.index, &s.dir, &s.rest); }
            v.push_back(s);
        }
        v.resize(REMAP_COUNT);
        maps_[guid] = v;
    }
    if (!maps_.empty()) std::printf("[input] %d remapped gamepad(s) (%s)\n", (int)maps_.size(), path.c_str());
}

void Input::saveMappings() const {
    if (mapPath_.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(mapPath_).parent_path(), ec);
    if (maps_.empty()) { std::filesystem::remove(mapPath_, ec); return; }
    std::ofstream f(mapPath_);
    for (const auto& [guid, v] : maps_) {
        f << guid;
        for (const Source& s : v) {
            char b[48] = "-";
            if (s.kind == 'b' || s.kind == 'k') std::snprintf(b, sizeof(b), "%c%d", s.kind, s.index);
            else if (s.kind == 'h') std::snprintf(b, sizeof(b), "h%d:%d", s.index, s.dir);
            else if (s.kind == 'a') std::snprintf(b, sizeof(b), "a%d:%d:%.2f", s.index, s.dir, s.rest);
            f << '\t' << b;
        }
        f << '\n';
    }
}

void Input::startRemap() {
    remapNew_.assign(REMAP_COUNT, Source{});
    remapStep_ = 0;
    remapJid_ = -1;
    remapCalm_ = 0;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++) { // the axes at rest (a trigger's is -1, or 0)
        remapRest_[j].clear();
        int na = 0;
        const float* ax = glfwJoystickPresent(j) ? glfwGetJoystickAxes(j, &na) : nullptr;
        for (int i = 0; ax && i < na; i++) remapRest_[j].push_back(ax[i]);
    }
    std::printf("[input] remapping the gamepad\n");
}

void Input::skipRemap() {
    if (remapStep_ < 0) return;
    remapNew_[remapStep_] = Source{};
    remapCalm_ = 0;
    if (++remapStep_ < REMAP_COUNT) return;
    if (remapJid_ >= 0) {
        maps_[guidOf(remapJid_)] = remapNew_;
        saveMappings();
        std::printf("[input] gamepad remapped\n");
        remapDone_ = true;
    }
    remapStep_ = -1;
    padHold_ = true;
    padHoldT_ = 0.3f;
}

void Input::cancelRemap() {
    if (remapStep_ >= 0) std::printf("[input] remap cancelled\n");
    remapStep_ = -1;
    padHold_ = true;
    padHoldT_ = 0.3f;
}

void Input::resetMapping() {
    bool any = false;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++)
        if (glfwJoystickPresent(j)) any |= maps_.erase(guidOf(j)) > 0;
    if (any) std::printf("[input] gamepad mapping back to its default\n");
    saveMappings();
}

bool Input::customMapping() const {
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++)
        if (glfwJoystickPresent(j) && maps_.count(guidOf(j))) return true;
    return false;
}

bool Input::sourceOn(int jid, const Source& s) const {
    int n = 0;
    if (s.kind == 'b') {
        const unsigned char* b = glfwGetJoystickButtons(jid, &n);
        return b && s.index < n && b[s.index] == GLFW_PRESS && !(s.index == 0 && padKeyEcho(guidOf(jid)));
    }
    if (s.kind == 'k') return padKeyOn(guidOf(jid), s.index);
    if (s.kind == 'h') { const unsigned char* h = glfwGetJoystickHats(jid, &n); return h && s.index < n && (h[s.index] & s.dir); }
    if (s.kind == 'a') { const float* a = glfwGetJoystickAxes(jid, &n); return a && s.index < n && (a[s.index] - s.rest) * s.dir > 0.5f; }
    return false;
}

// One step of a remap: once every control of the controllers is back at rest (0.15 s), the first one pressed is the
// step's (a button first, then a hat, then an axis pushed past half its range); then the next step, from the same
// controller.
void Input::remapRead(float dt) {
    bool calm = true;
    Source hit;
    int hitJid = -1;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++) {
        if (!glfwJoystickPresent(j) || (remapJid_ >= 0 && j != remapJid_)) continue;
        int nb = 0, nh = 0, na = 0;
        const unsigned char* bt = glfwGetJoystickButtons(j, &nb);
        const unsigned char* ht = glfwGetJoystickHats(j, &nh);
        const float* ax = glfwGetJoystickAxes(j, &na);
        Source s;
        const std::string guid = guidOf(j);
        if (const int k = padKeyDown(guid)) { s.kind = 'k'; s.index = k; }
        const bool echo = padKeyEcho(guid); // (button 0 is then that key's, not the player's)
        if (echo) calm = false;
        for (int i = echo ? 1 : 0; bt && i < nb && !s.kind; i++)
            if (bt[i] == GLFW_PRESS) { s.kind = 'b'; s.index = i; }
        for (int i = 0; ht && i < nh && !s.kind; i++)
            if (ht[i]) { s.kind = 'h'; s.index = i; s.dir = ht[i] & -ht[i]; }
        for (int i = 0; ax && i < na && !s.kind; i++) {
            const float rest = i < (int)remapRest_[j].size() ? remapRest_[j][i] : 0.f, d = ax[i] - rest;
            if (std::fabs(d) > 0.5f) { s.kind = 'a'; s.index = i; s.dir = d > 0 ? 1 : -1; s.rest = rest; }
        }
        if (s.kind) {
            calm = false;
            if (!hit.kind) { hit = s; hitJid = j; }
        }
    }
    // (a control already given to an earlier step is not taken twice: BACK on the button of A skipped the song at
    // every rotation)
    for (int k = 0; hit.kind && k < remapStep_; k++) {
        const Source& o = remapNew_[k];
        if (o.kind != hit.kind || o.index != hit.index || o.dir != hit.dir) continue;
        if (remapCalm_ >= 0.15f) std::printf("[input] %s: %c%d is %s already, ignored\n", REMAP_NAME[remapStep_], hit.kind, hit.index, REMAP_NAME[k]);
        hit = Source{};
    }
    if (hit.kind && remapCalm_ >= 0.15f) {
        remapJid_ = hitJid;
        remapNew_[remapStep_] = hit;
        std::printf("[input] %s: %c%d\n", REMAP_NAME[remapStep_], hit.kind, hit.index);
        remapCalm_ = 0;
        if (++remapStep_ == REMAP_COUNT) {
            maps_[guidOf(remapJid_)] = remapNew_;
            saveMappings();
            std::printf("[input] gamepad remapped\n");
            remapDone_ = true;
            remapStep_ = -1;
            padHold_ = true;
    padHoldT_ = 0.3f;
        }
        return;
    }
    remapCalm_ = calm ? remapCalm_ + dt : 0.f;
}

void Input::loadMappings() {
#ifdef REFALL_WEB
    return; // (the browser maps the gamepads itself: its standard layout)
#endif
    namespace fs = std::filesystem;
    fs::path exe = platform::executableDir();
    std::vector<fs::path> candidates = {exe / "gamecontrollerdb.txt"};
#ifdef REFALL_SOURCE_DIR
    candidates.push_back(fs::path(REFALL_SOURCE_DIR) / "third_party" / "gamecontrollerdb.txt");
#endif
    bool loaded = false;
    for (const auto& p : candidates) {
        std::ifstream f(p);
        if (!f) continue;
        std::stringstream ss;
        ss << f.rdbuf();
        if (glfwUpdateGamepadMappings(ss.str().c_str())) {
            std::printf("[input] gamepad mappings loaded from %s\n", p.string().c_str());
            loaded = true;
            break;
        }
    }
#ifdef REFALL_STANDALONE
    if (!loaded) { // the standalone build carries them
        static const unsigned char DB[] = {
#include "standalone/gamecontrollerdb.txt.inc"
        };
        if (glfwUpdateGamepadMappings(std::string((const char*)DB, sizeof(DB)).c_str()))
            std::printf("[input] gamepad mappings (built in) loaded\n");
    }
#else
    (void)loaded;
#endif
    // User additions, SDL style.
    if (const char* extra = std::getenv("SDL_GAMECONTROLLERCONFIG")) glfwUpdateGamepadMappings(extra);
}

void Input::init(GLFWwindow* w) {
    win_ = w;
    // Letter shortcuts follow the keyboard layout (e.g. Z is Z on AZERTY): find which physical key
    // produces each letter; default to the US position.
    for (int c = 0; c < 26; c++) letter_[c] = GLFW_KEY_A + c;
    for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_GRAVE_ACCENT; k++) {
        const char* name = glfwGetKeyName(k, 0);
        if (name && name[0] && !name[1] && name[0] >= 'a' && name[0] <= 'z') letter_[name[0] - 'a'] = k;
    }
    g_input = this;
    glfwSetJoystickCallback(joystickCallback);
    glfwSetCharCallback(w, charCallback);
    detectGamepad();
    if (jid_ >= 0) {
        active_ = Gamepad;
        changedTimer_ = 3.f;
    }
}

void Input::detectGamepad() {
    int old = jid_;
    scanPadKeys();
    jid_ = -1;
    padName_.clear();
    // Prefer a controller with a known mapping; otherwise use any joystick with a generic layout.
    for (int pass = 0; pass < 2 && jid_ < 0; pass++)
        for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++) {
            if (!glfwJoystickPresent(j)) continue;
            bool isPad = glfwJoystickIsGamepad(j);
            if (pass == 0 && !isPad) continue;
            jid_ = j;
            mapped_ = isPad;
            const char* n = isPad ? glfwGetGamepadName(j) : glfwGetJoystickName(j);
            padName_ = n ? n : "GAMEPAD";
            if (!isPad) std::printf("[input] %s has no known mapping: using a generic layout\n", padName_.c_str());
            break;
        }
    if (jid_ != old) {
        if (jid_ >= 0) {
            std::printf("[input] gamepad connected: %s\n", padName_.c_str());
            active_ = Gamepad;
        } else {
            std::printf("[input] gamepad disconnected, using keyboard\n");
            active_ = Keyboard;
        }
        changedTimer_ = 3.f;
    }
}

void Input::update(float dt) {
    if (g_joystickEvent) {
        g_joystickEvent = false;
        for (bool& l : stickLive_) l = false;
        detectGamepad();
    }
    changedTimer_ = std::max(0.f, changedTimer_ - dt);
    for (int i = 0; i < A_COUNT; i++) prev_[i] = down_[i];

    auto key = [&](int k) { return win_ && glfwGetKey(win_, k) == GLFW_PRESS; };
    auto letter = [&](char c) { return key(letter_[c - 'a']); };
    bool kb[A_COUNT] = {};
    const bool ctrl = key(GLFW_KEY_LEFT_CONTROL) || key(GLFW_KEY_RIGHT_CONTROL);
    const bool shift = key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_RIGHT_SHIFT);
    const bool seekChord = ctrl && shift;
    // Ctrl+Shift+Left/Right seeks in the song; the arrows then don't move the piece.
    kb[A_SEEK_BACK] = seekChord && key(GLFW_KEY_LEFT);
    kb[A_SEEK_FWD] = seekChord && key(GLFW_KEY_RIGHT);
    kb[A_LEFT] = (!seekChord && key(GLFW_KEY_LEFT)) || letter('a');
    kb[A_RIGHT] = (!seekChord && key(GLFW_KEY_RIGHT)) || letter('d');
    kb[A_SOFT] = key(GLFW_KEY_DOWN) || (!cheatSkip_ && letter('s'));
    kb[A_CHEAT_SKIP] = cheatSkip_ && letter('s');
    kb[A_HARD] = key(GLFW_KEY_SPACE) || letter('w');
    kb[A_CW] = key(GLFW_KEY_UP) || letter('x') || letter('k');
    kb[A_CCW] = letter('z') || letter('j');
    // Shift holds only when tapped alone (released without Ctrl or an arrow), so it can be part of the
    // Ctrl+Shift seek chord.
    bool shiftTap = false;
    if (shift) {
        if (ctrl || key(GLFW_KEY_LEFT) || key(GLFW_KEY_RIGHT)) shiftChord_ = true;
    } else if (shiftWasDown_) {
        shiftTap = !shiftChord_;
        shiftChord_ = false;
    }
    shiftWasDown_ = shift;
    kb[A_HOLD] = letter('c') || shiftTap;
    kb[A_BONUS] = letter('v') || letter('e');
    kb[A_PAUSE] = key(GLFW_KEY_ESCAPE) || letter('p');
    kb[A_NEXT_SONG] = letter('n') || key(GLFW_KEY_TAB) || key(GLFW_KEY_PAGE_DOWN) || key(GLFW_KEY_ENTER);
    kb[A_NEW_SCENE] = letter('t');
    kb[A_FULLSCREEN] = key(GLFW_KEY_F11) || letter('f');
    kb[A_QUIT] = letter('q');
    kb[A_HUD] = letter('h');
    kb[A_DEBUG_LEVEL] = letter('l');
    kb[A_DEBUG_BONUS] = letter('b');
    kb[A_UI_UP] = key(GLFW_KEY_UP);
    kb[A_UI_DOWN] = key(GLFW_KEY_DOWN);
    kb[A_UI_LEFT] = key(GLFW_KEY_LEFT);
    kb[A_UI_RIGHT] = key(GLFW_KEY_RIGHT);
    kb[A_UI_OK] = key(GLFW_KEY_ENTER) || key(GLFW_KEY_KP_ENTER);
    kb[A_UI_ALT] = key(GLFW_KEY_SPACE) || key(GLFW_KEY_TAB);
    kb[A_UI_BACK] = key(GLFW_KEY_ESCAPE) || key(GLFW_KEY_BACKSPACE);
    kb[A_UI_ENTER] = key(GLFW_KEY_ENTER) || key(GLFW_KEY_KP_ENTER);
    kb[A_UI_ERASE] = key(GLFW_KEY_BACKSPACE);
    kb[A_UI_DELETE] = key(GLFW_KEY_DELETE);
    kb[A_UI_PASTE] = ctrl && letter('v');
    typed_ = pending_;
    pending_.clear();

    // Every connected controller is read and their presses combined: a Bluetooth controller that reconnected
    // can leave a stale duplicate device behind (listed first, sending nothing), which hid the live one.
    // (While the gamepad is remapped, its presses are the remap's only.)
    bool pad[A_COUNT] = {};
    const bool remap = remapStep_ >= 0;
    if (remap) remapRead(dt);
    auto readPad = [&](const GLFWgamepadstate& st, bool& live) {
        auto b = [&](int i) { return st.buttons[i] == GLFW_PRESS; };
        float ax = st.axes[GLFW_GAMEPAD_AXIS_LEFT_X], ay = st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        float lt = st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER], rt = st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
        // The stick counts once it has been seen at rest: a stale duplicate device reports it pushed to a
        // corner forever, which held hard drop down.
        if (std::fabs(ax) < 0.3f && std::fabs(ay) < 0.3f) live = true;
        if (!live) ax = ay = 0.f;
        // Stick: dominant axis only, to avoid accidental diagonals.
        bool sx = std::fabs(ax) > 0.5f && std::fabs(ax) > std::fabs(ay);
        bool sy = std::fabs(ay) > 0.5f && std::fabs(ay) >= std::fabs(ax);
        pad[A_LEFT] |= b(GLFW_GAMEPAD_BUTTON_DPAD_LEFT) || (sx && ax < 0);
        pad[A_RIGHT] |= b(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT) || (sx && ax > 0);
        pad[A_SOFT] |= b(GLFW_GAMEPAD_BUTTON_DPAD_DOWN) || (sy && ay > 0);
        pad[A_HARD] |= b(GLFW_GAMEPAD_BUTTON_DPAD_UP) || (sy && ay < -0.8f);
        pad[A_CW] |= b(GLFW_GAMEPAD_BUTTON_A);
        pad[A_CCW] |= b(GLFW_GAMEPAD_BUTTON_B) || b(GLFW_GAMEPAD_BUTTON_X);
        pad[A_HOLD] |= b(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) || b(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER);
        pad[A_BONUS] |= lt > 0.5f || rt > 0.5f;
        pad[A_NEW_SCENE] |= b(GLFW_GAMEPAD_BUTTON_Y);
        pad[A_PAUSE] |= b(GLFW_GAMEPAD_BUTTON_START);
        pad[A_NEXT_SONG] |= b(GLFW_GAMEPAD_BUTTON_BACK);
        pad[A_HUD] |= b(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB);
        pad[A_UI_UP] |= b(GLFW_GAMEPAD_BUTTON_DPAD_UP) || (sy && ay < 0);
        pad[A_UI_DOWN] |= b(GLFW_GAMEPAD_BUTTON_DPAD_DOWN) || (sy && ay > 0);
        pad[A_UI_LEFT] |= b(GLFW_GAMEPAD_BUTTON_DPAD_LEFT) || (sx && ax < 0);
        pad[A_UI_RIGHT] |= b(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT) || (sx && ax > 0);
        pad[A_UI_OK] |= b(GLFW_GAMEPAD_BUTTON_A) || b(GLFW_GAMEPAD_BUTTON_START);
        pad[A_UI_BACK] |= b(GLFW_GAMEPAD_BUTTON_B);
        pad[A_UI_ALT] |= b(GLFW_GAMEPAD_BUTTON_X) || b(GLFW_GAMEPAD_BUTTON_Y);
    };
    {
        std::lock_guard<std::mutex> lock(remoteMutex_);
        if (remote_ && !remap) {
            GLFWgamepadstate st;
            std::memcpy(st.buttons, remoteButtons_, sizeof(st.buttons));
            std::memcpy(st.axes, remoteAxes_, sizeof(st.axes));
            readPad(st, remoteLive_);
        }
    }
    for (int jid = GLFW_JOYSTICK_1; win_ && jid_ >= 0 && !remap && jid <= GLFW_JOYSTICK_LAST; jid++) {
        if (!glfwJoystickPresent(jid)) continue;
        const bool mapped = glfwJoystickIsGamepad(jid);
        GLFWgamepadstate st;
        bool ok = mapped && glfwGetGamepadState(jid, &st);
        if (!mapped) {
            // Generic layout for unknown controllers: axes 0/1 = left stick, hat 0 = D-pad, face buttons 0-3,
            // shoulders 4-5, select/start 8-9 (the most common HID ordering).
            int na = 0, nb = 0, nh = 0;
            const float* ax = glfwGetJoystickAxes(jid, &na);
            const unsigned char* bt = glfwGetJoystickButtons(jid, &nb);
            const unsigned char* ht = glfwGetJoystickHats(jid, &nh);
            if (ax && bt) {
                std::memset(&st, 0, sizeof(st));
                for (int i = 0; i < 6 && i < na; i++) st.axes[i] = ax[i];
                // Triggers are usually buttons 6/7 on such controllers.
                st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = 6 < nb && bt[6] ? 1.f : -1.f;
                st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = 7 < nb && bt[7] ? 1.f : -1.f;
                const bool echo = padKeyEcho(guidOf(jid)); // (button 0 is then a key's echo)
                auto B = [&](int i) { return (unsigned char)(i < nb && !(i == 0 && echo) ? bt[i] : 0); };
                st.buttons[GLFW_GAMEPAD_BUTTON_A] = B(1);
                st.buttons[GLFW_GAMEPAD_BUTTON_B] = B(2);
                st.buttons[GLFW_GAMEPAD_BUTTON_X] = B(0);
                st.buttons[GLFW_GAMEPAD_BUTTON_Y] = B(3);
                st.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = B(4);
                st.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = B(5);
                st.buttons[GLFW_GAMEPAD_BUTTON_BACK] = B(8);
                st.buttons[GLFW_GAMEPAD_BUTTON_START] = B(9);
                unsigned char h = nh > 0 && ht ? ht[0] : 0;
                st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = (h & GLFW_HAT_UP) ? 1 : 0;
                st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = (h & GLFW_HAT_DOWN) ? 1 : 0;
                st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = (h & GLFW_HAT_LEFT) ? 1 : 0;
                st.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = (h & GLFW_HAT_RIGHT) ? 1 : 0;
                ok = true;
            }
        }
        // the player's own mapping over it, for the controls they set
        auto custom = maps_.find(guidOf(jid));
        if (custom != maps_.end()) {
            if (!ok) { std::memset(&st, 0, sizeof(st)); st.axes[4] = st.axes[5] = -1.f; ok = true; }
            for (int k = 0; k < REMAP_COUNT; k++) {
                const Source& s = custom->second[k];
                if (!s.kind) continue;
                const bool on = sourceOn(jid, s);
                if (REMAP_TARGET[k] >= 100) st.axes[REMAP_TARGET[k] - 100] = on ? 1.f : -1.f;
                else st.buttons[REMAP_TARGET[k]] = on ? GLFW_PRESS : GLFW_RELEASE;
            }
        }
        if (ok) readPad(st, stickLive_[jid]);
    }

    if (padHold_) { // (the remap's last press doesn't act)
        padHoldT_ -= dt;
        bool any = false;
        for (int i = 0; i < A_COUNT; i++) any |= pad[i];
        if (any || padHoldT_ > 0.f) std::memset(pad, 0, sizeof(pad));
        else padHold_ = false;
    }
    bool padAny = false, keyAny = false;
    for (int i = 0; i < A_COUNT; i++) {
        padAny |= pad[i];
        keyAny |= kb[i];
        down_[i] = kb[i] || pad[i];
    }
    // Switch the active device on a fresh press from the other one.
    if (padAny && !prevPadAny_ && active_ != Gamepad) { active_ = Gamepad; changedTimer_ = 2.f; }
    if (keyAny && !prevKeyAny_ && active_ != Keyboard) { active_ = Keyboard; changedTimer_ = 2.f; }
    prevPadAny_ = padAny;
    prevKeyAny_ = keyAny;

    // Auto-repeat.
    for (int i = 0; i < A_COUNT; i++) {
        repeat_[i] = 0;
        if (!down_[i]) { held_[i] = 0; acc_[i] = 0; continue; }
        if (!prev_[i]) { repeat_[i] = 1; held_[i] = 0; acc_[i] = 0; continue; }
        held_[i] += dt;
        const bool seek = i == A_SEEK_BACK || i == A_SEEK_FWD;
        const bool ui = (i >= A_UI_UP && i <= A_UI_RIGHT) || i == A_UI_ERASE;
        float delay = (i == A_SOFT) ? 0.f : seek ? 0.35f : ui ? 0.32f : DAS;
        float rate = (i == A_SOFT) ? SOFT_RATE : seek ? 0.2f : ui ? 0.09f : ARR;
        if (held_[i] >= delay) {
            acc_[i] += dt;
            while (acc_[i] >= rate) { acc_[i] -= rate; repeat_[i]++; }
        }
    }
    // Opposite directions: latest wins.
    if (down_[A_LEFT] && down_[A_RIGHT]) {
        if (held_[A_LEFT] < held_[A_RIGHT]) repeat_[A_RIGHT] = 0; else repeat_[A_LEFT] = 0;
    }
}

void Input::setRemotePad(const unsigned char* buttons, const float* axes) {
    std::lock_guard<std::mutex> lock(remoteMutex_);
    remote_ = true;
    std::memcpy(remoteButtons_, buttons, sizeof(remoteButtons_));
    std::memcpy(remoteAxes_, axes, sizeof(remoteAxes_));
}

std::string Input::clipboard() const {
    const char* c = win_ ? glfwGetClipboardString(win_) : nullptr;
    return c ? std::string(c) : std::string();
}

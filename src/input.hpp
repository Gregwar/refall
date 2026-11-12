#pragma once
// Unified keyboard / gamepad input. Both always work; the "active" device (the one used last)
// decides which control hints are shown. Gamepads are detected at startup and hot-plugged.
#include <string>

struct GLFWwindow;

enum Action {
    A_LEFT, A_RIGHT, A_SOFT, A_HARD, A_CW, A_CCW, A_HOLD,
    A_BONUS,                 // slow-motion bonus: V / E, gamepad triggers (ZL / ZR, LT / RT)
    A_PAUSE, A_NEXT_SONG, A_NEW_SCENE, A_FULLSCREEN, A_QUIT, A_HUD,
    A_SEEK_BACK, A_SEEK_FWD, // Ctrl+Shift+Left/Right: seek in the song (testing)
    A_DEBUG_LEVEL,           // L: jump to the next level (debugging)
    A_DEBUG_BONUS,           // B: fill the bonus gauge (testing the slow-motion effects)
    // Menus: arrows / D-pad / stick (with auto-repeat), Enter / Space / A / Start, Esc / Backspace / B.
    A_UI_UP, A_UI_DOWN, A_UI_LEFT, A_UI_RIGHT, A_UI_OK, A_UI_BACK,
    // Text fields: Enter alone (Space types a space), Backspace (with auto-repeat), Delete, Ctrl+V.
    A_UI_ENTER, A_UI_ERASE, A_UI_DELETE, A_UI_PASTE,
    A_UI_ALT, // the menu's second action (open a stage's petal): Space / Tab, gamepad X / Y
    A_COUNT
};

class Input {
public:
    enum Device { Keyboard, Gamepad };

    void init(GLFWwindow* w);
    // Loads SDL_GameControllerDB mappings so many more controllers are recognised as gamepads.
    static void loadMappings();
    void update(float dt);

    bool down(Action a) const { return down_[a]; }
    bool pressed(Action a) const { return down_[a] && !prev_[a]; }
    // Auto-repeat for movement (DAS / ARR): returns number of steps to apply this frame.
    int repeatSteps(Action a) const { return repeat_[a]; }

    Device active() const { return active_; }
    bool gamepadConnected() const { return jid_ >= 0; }
    const std::string& gamepadName() const { return padName_; }
    // Testing without a window (ZEN_TEST_PLAY): a new frame forgets the presses, testPress presses an action.
    void testFrame() {
        for (int i = 0; i < A_COUNT; i++) { prev_[i] = down_[i]; down_[i] = false; repeat_[i] = 0; }
        typed_.clear();
    }
    void testPress(Action a) { down_[a] = true; repeat_[a] = 1; }
    void testType(const std::string& s) { typed_ += s; }
    // The text typed this frame (printable ASCII), and the clipboard (Ctrl+V).
    const std::string& typed() const { return typed_; }
    std::string clipboard() const;
    void onChar(unsigned int cp) { if (cp >= 32 && cp < 127) pending_ += (char)cp; }
    // Set briefly when the device changes, for a HUD toast.
    float deviceChangedTimer() const { return changedTimer_; }

private:
    void detectGamepad();
    GLFWwindow* win_ = nullptr;
    bool down_[A_COUNT] = {}, prev_[A_COUNT] = {};
    int repeat_[A_COUNT] = {};
    float held_[A_COUNT] = {}, acc_[A_COUNT] = {};
    int jid_ = -1;
    bool mapped_ = true; // false: raw joystick without a known mapping (generic layout)
    std::string padName_;
    bool stickLive_[16] = {}; // per joystick: its stick was seen at rest (a stale device reports -1 forever)
    Device active_ = Keyboard;
    float changedTimer_ = 0;
    bool prevPadAny_ = false, prevKeyAny_ = false;
    bool shiftWasDown_ = false, shiftChord_ = false; // Shift holds on a clean tap only
    int letter_[26] = {}; // physical key producing each letter on the current keyboard layout
    std::string pending_, typed_; // characters from the window, this frame's
};

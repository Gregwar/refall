#pragma once
// The game's name, as the title screen, the window and the worlds that letter it show it (one place to change it).
inline constexpr const char* GAME_TITLE = "Refall";
// What a four-line clear is called on screen.
inline constexpr const char* QUAD_NAME = "QUAD";
#include <string>
inline std::string gameTitleUpper() {
    std::string s = GAME_TITLE;
    for (char& c : s) c = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
    return s;
}

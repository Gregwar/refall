#pragma once
// A song's title as the HUD letters it (YouTube titles carry their upload's labels).
#include <string>

// Its accents and typographic punctuation kept, the symbols no lettering has (emoji, chess pieces...) dropped, and the
// upload's labels taken out: "(Official Video)", "[Lyric Video]", "(Visualizer)", "(Audio)", "(Original Mix)",
// "- Official Music Video", "| Official Video"...; a title in quotes after the artist unquoted ("nimino - 'Rest Easy'"
// -> "nimino - Rest Easy"). What names the version stays: "(feat. ...)", "(... Remix)", "(edit)", "(... Version)".
std::string displayTitle(const std::string& utf8);

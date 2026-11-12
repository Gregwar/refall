#pragma once
// A pixel font for the menus: `big` is 5x7 (with lower case and descenders), else 3x5 capitals.
#include <functional>
#include <string>

namespace pixelfont {
inline int height(bool big) { return big ? 7 : 5; }
int width(bool big, const std::string& s, int scale = 1); // pixels, one pixel (x scale) between letters
// Calls plot(x, y) for every lit pixel of the text, its top-left corner at (x, y).
void draw(bool big, const std::string& s, int x, int y, int scale, const std::function<void(int, int)>& plot);
} // namespace pixelfont

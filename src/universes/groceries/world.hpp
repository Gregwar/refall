#pragma once
// GROCERIES: the checkout counter seen from above, in board units (one unit = one product, about 10 cm). The board's
// plane z = 0 holds the products' centers; the counter's top lies just behind it, +z points up toward the camera.
namespace groc {

constexpr float Z_TOP = -0.5f;      // the counter's top (the belt's surface is level with it)
constexpr float CELL = 0.46f;       // a product's half size in the well
constexpr float CHUTE_X = 6.15f;    // the steel lane along the well's right rail, down to the scanner
constexpr float SCAN_X = 8.6f, SCAN_Y = -7.6f;  // the scanner's glass
constexpr float BAG_X = 13.6f, BAG_Y = -7.6f;   // the bag being packed
constexpr float BAG_HX = 1.37f, BAG_HY = 1.0f, BAG_H = 2.6f; // a bag's half width, half depth, height
constexpr float PACK = 0.78f;       // a packed product's size relative to a cell
constexpr float PRINT_X = -16.5f, PRINT_Y = 0.2f; // the receipt printer

} // namespace groc

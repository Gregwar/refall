#pragma once
// GROCERIES: the store's dimensions, in board units (one unit = one product, about 10 cm). The well's cells lie on
// the plane z = 0 (the layout contract), standing on our checkout's counter; the store recedes toward -z, the car
// park lies behind the first camera (+z).
namespace groc {

constexpr float EYE_Y = 11.f;      // the first camera's height (the horizon sits at this board y)
constexpr float Y_FLOOR = -19.5f;  // the floor (and the car park, the land around)
constexpr float Y_CT = -10.3f;     // the counters' top
constexpr float Y_CEIL = 26.f;     // the ceiling
constexpr float LANE_DZ = 42.f;    // lane k's counter is centered on z = -k * LANE_DZ
constexpr int LANES = 8;
constexpr float LANE_X0 = -32.f, LANE_X1 = 34.f;   // a counter's extent
constexpr float Z_AISLE0 = -360.f, Z_AISLE1 = -650.f; // the aisles' gondolas
constexpr float Z_BACK = -668.f;   // the back wall
constexpr float X_WALL = 236.f;    // the side walls
constexpr float Z_FRONT = 72.f;    // the glazed front (behind the first camera)
constexpr float MODULE_W = 2.f * X_WALL;  // a hypermarket wing's width
constexpr float CELL = 0.46f;      // a product's half size in the well
constexpr float CAR_Z1 = 640.f;    // the car park's far edge
constexpr float BAG_SX = 1.7f, BAG_SY = 2.3f, BAG_SZ = 1.15f; // a bag's half size

// The mountains of bags: where they rise, when they start (lines), how fast they grow (units a line), how high.
constexpr int MOUNTAINS = 6;
constexpr float MOUNT_X[MOUNTAINS] = {-1750.f, 1850.f, -2700.f, 2800.f, -3700.f, 3900.f};
constexpr float MOUNT_Z[MOUNTAINS] = {-250.f, 150.f, 850.f, -900.f, -1300.f, 1200.f};
inline float mountainL0(int i) { return 74.f + 8.f * i; }
inline float mountainH(int i, float L) {
    const float h = (L - mountainL0(i)) * (9.f + 0.8f * i);
    return h < 0.f ? 0.f : (h > 950.f ? 950.f : h);
}

} // namespace groc

#pragma once
// DOCKS: the port's dimensions, in board units (one unit = one container's height, about 2.6 m). The well's cells lie
// on the plane z = 0 (the layout contract); the port recedes toward -z.
namespace docks_world {

constexpr float EYE_Y = 3.2f;     // the camera's height (the horizon sits at this board y)
constexpr float Y_W = -13.f;      // the water
constexpr float Y_Q = -10.f;      // the near quay (the stack stands on it)
constexpr float Z_QE = -14.f;     // the near quay's edge
constexpr float Y_F = -10.6f;     // the far quay
constexpr float Z_FE = -62.f;     // the far quay's edge
constexpr float SHIP_Z = -54.5f;  // the ships' center line
constexpr float SHIP_HALF_B = 6.f;
constexpr float SHIP_LEN = 52.f;
constexpr float BOW_U = SHIP_LEN - 12.f;  // where the bow starts narrowing
constexpr float DECK_Y = Y_W + 4.2f;
constexpr float CRANE_SEA = -65.f, CRANE_LAND = -81.f; // the ship-to-shore cranes' legs
constexpr float BOOM_Y = Y_F + 17.f;
constexpr float BOOM_BACK = -98.f, BOOM_TIP = -38.f;
constexpr float Z_YARD = -92.f;     // the yard's first block row
constexpr float CRANE_TOP = Y_F + 13.5f, CRANE_APEX = Y_F + 26.f;
constexpr float NEAR_LEG_X = 18.f, NEAR_LEG_Z0 = 12.f, NEAR_LEG_Z1 = -4.5f; // the gantry over the well

} // namespace docks_world

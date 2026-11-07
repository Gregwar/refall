#pragma once
// BRUTALIST: the place's dimensions, in board units (one unit = one cast block, about 0.8 m). The well's cells lie on
// the plane z = 0 (the layout contract), cut into the monument's face; the plaza and its buildings recede toward -z.
namespace brutalist_world {

constexpr float EYE_Y = -1.5f;     // the camera's height (the horizon sits at this board y)
constexpr float Y_G = -11.4f;      // the plaza
constexpr float Y_SILL = -10.f;    // the sill the stack stands on
constexpr float Y_P = -8.7f;       // the podium behind the plaza's steps
constexpr float Z_STEPS = -14.f;   // the foot of the steps up to the podium
constexpr float Z_BACK = -0.96f;   // the slot's back wall (the blocks are 0.94 deep, their faces at z = 0)
constexpr float X_SLOT = 5.04f;    // the slot's side walls
constexpr float X_PIER = 6.7f;     // the piers' outer faces
constexpr float Z_PIER = 0.55f;    // the piers' faces, a little proud of the stack
constexpr float Y_LINTEL = 10.1f;  // the lintel's soffit over the well
constexpr float Y_MON = 12.6f;     // the monument's top

// The civic building on the left (on pilotis, its upper floors cantilevered), the growing one on the right.
constexpr float CIV_X0 = -66.f, CIV_X1 = -15.f, CIV_Z0 = -50.f, CIV_Z1 = -30.f;
constexpr float GROW_X0 = 13.f, GROW_Z0 = -58.f, GROW_Z1 = -42.f;
constexpr int GROW_COLS = 4, GROW_LEVELS = 7;
constexpr float MOD_W = 7.2f, MOD_H = 3.7f, MOD_D = 9.f;   // a module of the growing building
constexpr float GROW_Y0 = Y_P + 5.2f;                        // its first floor, over the pilotis
constexpr float CRANE_X = 50.f, CRANE_Z = -52.f, CRANE_TOP = 28.f, JIB_Y = 25.f; // the tower crane

} // namespace brutalist_world

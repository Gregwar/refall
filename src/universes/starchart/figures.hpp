#pragma once
// STAR CHART's figures: the line-art drawings engraved around the constellations (see figures.cpp).
#include <utility>
#include <vector>

#include "mathutil.hpp"

namespace starchart {

// A figure in its own space (about [-1, 1], y up): where its stars sit (in the order the clear's stars take them),
// the constellation's lines between them, and its name on the chart.
struct FigureDef {
    const char* name;
    std::vector<vec2> anchors;
    std::vector<std::pair<int, int>> edges;
};
int figureCount();
const FigureDef& figure(int i);
// The figure's engraving: polylines in its space and a weight for each (1 the outline, below 1 the hatching and
// the details). `life` 0 still .. 1 alive (the figure moving: a serpent winding, a fish's tail, wings beating...),
// `t` the time it moves by (seconds).
void figureLines(int i, float life, float t, std::vector<std::vector<vec2>>& lines, std::vector<float>& weights);

} // namespace starchart

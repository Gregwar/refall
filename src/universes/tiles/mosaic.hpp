#pragma once
// TILES: the mosaic's design and its tesserae, laid on the CPU like a mosaicist lays them (see mosaic.cpp).
#include <cstdint>
#include <vector>

#include "mathutil.hpp"

namespace tiles {

enum Material { MAT_STONE = 0, MAT_GLASS = 1, MAT_GOLD = 2, MAT_SILVER = 3 };
enum FigureKind { FIG_SUN = 0, FIG_ROSETTE = 1, FIG_STAR = 2 };

// A variant: its seven tiles, and the mosaic's colors and materials (sRGB hex).
struct VariantDef {
    const char* name;
    unsigned piece[7];
    unsigned field[3]; int fieldMat;     // the ground of the whole wall: main, a lighter, a darker tone
    unsigned frame[2]; int frameMat;     // the well's frame: its outer rows, its middle row
    unsigned ring[3]; int ringMat;       // the medallions' ring: outline, accent row, the band
    unsigned ground[2]; int groundMat;   // the medallions' ground: its middle, its rim
    unsigned fig[4]; int figMat;         // the figure: outline, main, light (its heart), second part
    unsigned fig2Mat;                    // the second part's material
    unsigned leaf[3];                    // outline, main, light
    unsigned vase[3]; int vaseMat;       // outline, body, band
    unsigned grape;
    unsigned grout, bed, sinopia;        // grout, the bare setting bed, the painter's red ochre sketch on it
    unsigned well;                       // the well's mortar bed
    unsigned light, ink, accent;         // the light's color, the lettering, its accent
    int figL, figR;                      // the two medallions' figures
    float pitch;                         // tessera row pitch (board units)
    float grout_w;                       // grout width
    bool shards;                         // broken tiles (trencadis): irregular shards
    bool stars;                          // gold stars scattered over the ground
};
extern const VariantDef VARIANTS[];
extern const int VARIANT_COUNT;

inline vec3 hexc(unsigned v) { return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f}; }
inline vec3 linc(unsigned v) {
    const vec3 c = hexc(v);
    return {std::pow(c.x, 2.2f), std::pow(c.y, 2.2f), std::pow(c.z, 2.2f)};
}

// One tessera: a small quadrilateral of stone, glass or gold set in the mortar.
struct Tessera {
    vec2 c[4];          // corners: start-, end-, end+, start+ (along its row, across it)
    vec2 center;
    vec3 color;         // linear
    int mat = MAT_GLASS;
    vec2 tilt;          // how it leans in the mortar (the normal's x, y)
    float rnd = 0.f;
    int label = 0;      // the region it belongs to
    int group = 0;      // 0: laid from the start; > 0: a figure laid by the cleared rows (its order on its side)
    int side = -1;      // 0 left, 1 right, -1 none
    int k = 0;          // its row, counted from the region's outline
    int stroke = 0;     // the run of tesserae it was laid in (an andamento line), and its place in it
    int idx = 0;
};

// Where things are (board units), so the universe can aim at them.
struct Layout {
    bool sides = false;     // the side panels hold medallions (wide enough screens)
    vec2 med[2];            // medallion centers
    float R = 4.f;          // medallion radius (outer edge of its ring)
    float ringW = 1.f;      // its ring's width
    vec2 vase[2];
};

struct Design {
    std::vector<Tessera> tess;
    // The painter's sketch on the bed (an RG8 raster over [lo, hi]): the outlines of the figures not yet laid, the guide
    // lines of their rows.
    std::vector<uint8_t> sketch;
    int sw = 0, sh = 0;
    vec2 lo, hi;
    Layout lay;
    // Per side: the figure tesserae in the order they are laid (indices into tess).
    std::vector<int> order[2];
    // The ground's tesserae (laid from the start), for the glints and gilding.
    std::vector<int> field;
};

// Lays the whole mosaic over the view (center c, half extents h) for a variant.
Design build(int variant, vec2 viewC, vec2 viewH, uint64_t seed);

} // namespace tiles

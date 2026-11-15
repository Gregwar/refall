// BIOME: glass cubes over a 3D diorama world that the cleared lines bring to life.
//
// One universe, two worlds, each a few hand-made variants (a light, a season, a palette). Both are seen the same way:
// a fixed lens raised above the world and shifted back down (the board plane keeps the layout's pixels while the world
// behind the well is seen from above), the world cut open just behind the well like a diorama, its section showing
// its layers; the stack a clean grid of small glass cubes traced per pixel in front of it, behind the calm pane of
// the well. A clear sends the row's cubes into the world behind, and the world answers:
//
//   the sea (sea.cpp): the cubes hold the sea's water. Thrown out tumbling, they crack where they land and their water
//   jets out; the sea rises from dry dunes into a deep sea with boats and life in its layers. A drop is a storm.
//
//   the land (land.cpp): the cubes hold seeds of the piece's color. They break open, the seeds fly into the parched,
//   cracked clay behind the well, bury themselves and sprout, and the plain grows into a jungle with birds and mist.
//   A drop is the rain.
//
// This file is the shell both worlds share: the variants over both worlds and the pick from the song, the scene's
// level, the weather's director (a drop's storm, its lightning, the kicks), game over, the registry. render.cpp draws
// the frame around the world's passes: the camera, the stack's cubes, the well's pane, the rain, the HUD.
#include "universes/biome/biome.hpp"

#include <algorithm>
#include <cstdio>

#include "layout.hpp"
#include "universes/biome/land.hpp"
#include "universes/biome/sea.hpp"
#include "universes/registry.hpp"

using namespace biome;

namespace biome {
static_assert(sizeof(Palette) % sizeof(float) == 0, "a palette is floats only");
void lerpPalette(Palette& a, const Palette& b, float k) {
    float* pa = reinterpret_cast<float*>(&a);
    const float* pb = reinterpret_cast<const float*>(&b);
    for (int i = 0; i < (int)(sizeof(Palette) / sizeof(float)); i++) pa[i] += (pb[i] - pa[i]) * k;
}
} // namespace biome

// =====================================================================================================
// Variants: the sea's, then the land's
// =====================================================================================================

int BiomeUniverse::variantCount() { return sea::variantCount() + land::variantCount(); }
const char* BiomeUniverse::variantName(int i) {
    return i < sea::variantCount() ? sea::variantName(i) : land::variantName(i - sea::variantCount());
}
int BiomeUniverse::worldOf(int v) const { return v >= sea::variantCount() ? 1 : 0; }
int BiomeUniverse::localOf(int v) const { return v >= sea::variantCount() ? v - sea::variantCount() : v; }

// The world from the song: the sea for airy, flowing songs over a deep bass; the land for dense songs that build and
// drop (the run's seed tips the close ones either way). Then the world picks its light from the key and the mode.
int BiomeUniverse::pickVariant(const SongLook& t) const {
    const float jitter = (float)((t.seed >> 21) % 1000) / 1000.f - 0.5f;
    const float seaness = (t.airWeight + t.bassWeight) - (t.density + t.dynamics) + 0.5f * jitter;
    // (air and bass weigh more than density and dynamics on most songs: the threshold keeps the two worlds even)
    const int w = t.hasSong ? (seaness > 0.18f ? 0 : 1) : (int)((t.seed >> 9) & 1);
    SongLook l = t;
    l.avoidVariant = t.avoidVariant >= 0 && worldOf(t.avoidVariant) == w ? localOf(t.avoidVariant) : -1;
    return (w ? sea::variantCount() : 0) + worlds_[w]->pickVariant(l);
}

BiomeUniverse::BiomeUniverse() {
    worlds_[0].reset(new SeaWorld(c_));
    worlds_[1].reset(new LandWorld(c_));
    w_ = worlds_[0].get();
}

// =====================================================================================================
// Song, level
// =====================================================================================================

void BiomeUniverse::setSong(const SongLook& look, float seconds) {
    const int v = look.variant >= 0 ? look.variant % variantCount() : pickVariant(look);
    biome::World* w = worlds_[worldOf(v)].get();
    // another world: it starts over at once, in its own light
    const bool fresh = w != w_ || seconds <= 0.f;
    variant_ = v;
    w_ = w;
    c_.seed = look.seed;
    c_.rng = Rng(look.seed ^ 0xB10Eull);
    if (fresh) palSet_ = false;
    w_->setSong(look, seconds, fresh);
}

void BiomeUniverse::setLevel(int level, float energy, float seconds, float impact) {
    (void)impact;
    level = std::clamp(level, 0, 2);
    c_.energy = energy;
    if (!hasLevel_ || seconds < 0.1f) {
        hasLevel_ = true;
        c_.level = level;
        c_.lv = lvFrom_ = lvTo_ = (float)level;
        lvT_ = 1.f;
        return;
    }
    c_.level = level;
    lvFrom_ = c_.lv;
    lvTo_ = (float)level;
    lvT_ = 0.f;
    lvDur_ = std::clamp(seconds, 1.5f, 6.f);
}

// The drop's first downbeat: its hit fires as soon as the section is known to be a drop (a chorus stays dry).
void BiomeUniverse::drop(float strength) {
    (void)strength;
    dropPending_ = 2.5f;
}

void BiomeUniverse::refresh(float strength) {
    c_.gust = std::max(c_.gust, 0.4f * strength);
    w_->refresh(strength);
}

vec2 BiomeUniverse::project(const vec3& p) const { return layout::project(c_.vp, p, width_, height_); }

TransitionHints BiomeUniverse::hints() const {
    const biome::World* w = worlds_[worldOf(variant_)].get();
    const Palette p = w->palette(localOf(variant_));
    TransitionHints h;
    h.background = w->background(p);
    h.light = ugl::luma(h.background) > 0.28f;
    h.ink = p.cell[0];
    h.accent = p.accent;
    h.styles = TS_RIPPLE | TS_LUMA | TS_INK;
    return h;
}

// =====================================================================================================
// Game events
// =====================================================================================================

void BiomeUniverse::onEvent(const GameEvent& ev, const Game& game) {
    if (ev.type == GameEvent::TopOut) {
        // game over: the stack cracks row by row from the top, and each cube leaves it
        int top = Game::H;
        for (auto& c : ev.cells) top = std::min(top, c.y);
        for (auto& c : ev.cells) {
            if (c.y < Game::HIDDEN || c.y >= Game::H || c.x < 0 || c.x >= Game::W) continue;
            const vec3 p = layout::cellPos((float)c.x, (float)c.y);
            Dying d;
            d.x = p.x; d.y = p.y; d.type = std::clamp((int)c.type, 0, 6);
            d.delay = 0.15f + (c.y - top) * 0.07f + std::fabs(c.x - 4.5f) * 0.015f;
            d.t = 0.f;
            d.gone = false;
            dying_.push_back(d);
        }
        c_.over = true;
    }
    w_->onEvent(ev, game);
}

// A whole stack filled at once while the world is shown (a board filled for a review): the world has grown already.
// The stack found on entering the universe (a game going on when the song or a peak switches to BIOME) grows nothing:
// the world always starts bare and grows only from the lines cleared in it.
void BiomeUniverse::watchBoard(const Game& g) {
    int n = 0;
    for (int y = 0; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++) n += g.cell(x, y).type >= 0;
    if (boardCount_ >= 0 && n - boardCount_ >= 12) w_->grown();
    boardCount_ = n;
}

// =====================================================================================================
// The weather's director
// =====================================================================================================

void BiomeUniverse::simulate(const Game& game, const MusicState& music, float dt, float wdt) {
    Ctx& c = c_;
    c.worldTime += wdt;
    if (lvT_ < 1.f) {
        lvT_ = std::min(1.f, lvT_ + dt / lvDur_);
        c.lv = lerpf(lvFrom_, lvTo_, smoothstepf(0.f, 1.f, lvT_));
    }
    const float wp = c.wp(), world = c.world();

    // Is this peak a drop? The HUD's section says it when there is one; else the song's drive. A drop's storm comes in
    // fast and calms over a few seconds.
    const float dropish = sectionKnown_ ? (sectionDrop_ ? 1.f : 0.f) : smoothstepf(1.27f, 1.42f, music.speed);
    const bool stormOn = c.level == 2 && dropish > 0.5f && !c.over;
    stormLin_ = stormOn ? std::min(1.f, stormLin_ + dt / w_->stormIn()) : std::max(0.f, stormLin_ - dt / 4.5f);
    c.storm = smoothstepf(0.f, 1.f, stormLin_);
    // the hit fires on the drop's first downbeat once the section is known to be a drop, or when the storm sets in (the
    // section's name may come a little after the downbeat); once
    const bool starts = stormOn && !stormWasOn_;
    stormWasOn_ = stormOn;
    sinceHit_ += dt;
    if (dropPending_ > 0.f || starts) {
        dropPending_ -= dt;
        if (stormOn) {
            dropPending_ = 0.f;
            if (sinceHit_ > 4.f) {
                sinceHit_ = 0.f;
                // the hit: lightning in the clouds far out, a gust, the world's answer
                c.flash = 1.f;
                c.flashAz = (c.rng.chance(0.5f) ? -1.f : 1.f) * c.rng.range(12.f, 30.f);
                c.gust = 1.f;
                w_->dropHit();
            }
        }
    }
    c.flash = approach(c.flash, 0.f, 4.f, dt);
    c.gust = approach(c.gust, 0.f, 0.8f, dt);
    c.cloudT += wdt * (0.007f + w_->cloudDrift() * c.storm);

    // the kicks at a peak; a lightning glow every four bars through the storm
    sinceKick_ += dt;
    const float kick = std::max(music.kick, kickIn_);
    kickIn_ = approach(kickIn_, 0.f, 6.f, dt);
    if (kick > 0.5f && prevKick_ <= 0.5f && sinceKick_ > 0.4f && world > 0.5f && !c.over && wp > 0.5f) {
        sinceKick_ = 0.f;
        w_->kick();
    }
    prevKick_ = kick;
    if (music.hasSong) {
        const double bar = std::floor(music.beat / 4.0 + 1e-4);
        if (lastBar_ < 0.0 || bar < lastBar_ - 4.0) lastBar_ = bar;
        if (bar > lastBar_) {
            lastBar_ = bar;
            if (c.storm > 0.6f && ((long long)bar % 4) == 0 && world > 0.5f) {
                c.flash = std::max(c.flash, 0.62f);
                c.flashAz = (c.rng.chance(0.5f) ? -1.f : 1.f) * c.rng.range(12.f, 30.f);
            }
        }
    }

    // game over: each cracked cube leaves the stack in its turn
    for (Dying& d : dying_) {
        d.t += dt;
        if (!d.gone && d.t >= d.delay) {
            d.gone = true;
            w_->breakOut(vec3(d.x, d.y, 0.f), d.type);
        }
    }
    dying_.erase(std::remove_if(dying_.begin(), dying_.end(), [](const Dying& d) { return d.gone; }), dying_.end());

    w_->simulate(game, music, dt, wdt);
}

// =====================================================================================================
// Registry
// =====================================================================================================

const UniverseEntry& universe_biome() {
    static const UniverseEntry e{
        "BIOME", "glass cubes of water or seeds bringing a 3D world to life",
        [] { return std::unique_ptr<Universe>(new BiomeUniverse()); },
        // Songs that build and drop suit it best (the storm, the rain); any of them a little.
        [](const SongLook& l) { return 0.75f + 0.22f * l.dynamics + 0.12f * l.airWeight + 0.05f * l.bassWeight + 0.05f * l.density; },
        [] {
            std::vector<std::string> v;
            for (int i = 0; i < BiomeUniverse::variantCount(); i++) v.push_back(BiomeUniverse::variantName(i));
            return v;
        }()};
    return e;
}

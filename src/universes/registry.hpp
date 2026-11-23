#pragma once
// The universe registry: every universe the game knows, how to make one, and how much it suits a song.
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "universe.hpp"

struct UniverseEntry {
    const char* name;      // upper case, shown in the HUD corner and given to --universe (any case)
    const char* blurb;     // one line: the look
    std::function<std::unique_ptr<Universe>()> make;
    // How well the universe suits a song (relative weight, ~1 neutral; 0 never picked for that song). Null: 1.
    std::function<float(const SongLook&)> affinity;
    std::vector<std::string> variants; // the variants' names (upper case), in index order (at least one)
    bool hidden = false;   // never picked at random (test universes); --universe and the viewer still reach it
    // variants renamed or merged: their former names (upper case) and the index each now loads as (a saved setting,
    // a campaign or a --universe naming one still works)
    std::vector<std::pair<std::string, int>> formerVariants = {};
};

namespace universes {
const std::vector<const UniverseEntry*>& all();
int count();
const UniverseEntry& entry(int i);
// Index of a universe by name (any case), -1 if unknown.
int find(const std::string& name);
// "NAME" or "NAME:VARIANT" (a variant name, any case, or its index): the index (-1 if unknown) and the variant
// (-1 if none or unknown).
int parse(const std::string& spec, int* variant);
// How often a universe is picked (registry.def: 1 for generic atmospheres, lower for specific worlds).
float weight(int i);
// How many of the last universes played are left out of the next pick (no repeats).
inline constexpr int RECENT_UNIVERSES = 10;
// A universe for a song: weighted by affinity times weight, hidden universes left out. `recent` lists the last universes
// played (oldest first): none of them plays again while another one can, then only the oldest come back (and never
// `current` while another one can).
int pick(const SongLook& look, uint64_t seed, int current, const std::vector<int>& recent = {});
// "VECTOR · P3 AMBER": a universe and one of its variants, as the HUD shows them (UTF-8 middle dot).
std::string label(int universe, int variant);
// Checks a --universe spec ("NAME" or "NAME:VARIANT"): true if it names a universe (and, when one is given, a
// variant of it); otherwise prints what is unknown and what exists (the universes, or that universe's variants).
bool check(const std::string& spec);
// The next universe in the registry order after `current` (hidden ones skipped unless `withHidden`).
int next(int current, bool withHidden = false);
} // namespace universes

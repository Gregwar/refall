#pragma once
// The save slots (SAVE SLOTS on the title screen): three players' runs side by side, one of them in use. A slot holds
// the main voyage's progress (stages cleared, songs beaten, best scores, the "every stage unlocked" mark), the menu's
// place and the custom voyages' progress; the options (the menu's music and theme), the gamepad's mapping, the
// playlists and the custom voyages themselves are the game's, shared by every slot.
// Slot 1 is the files of before the slots, where they were: <user data>/refall/campaign-progress.tsv,
// menu-state.tsv and voyages/<id>-progress.tsv (an older save simply is slot 1, and an older game still reads it).
// Slots 2 and 3 are the same files in slot2/ and slot3/ of that folder. The slot in use: save-slot.tsv there.
// (With --progress FILE, FILE is slot 1's progress and the slots sit round it the same way.)
#include <cstdint>
#include <string>

#include "campaign.hpp"

class SaveSlots {
public:
    static constexpr int COUNT = 3;
    // `progressPath`: slot 1's progress file ("" the default); `main` the main voyage (the summaries count its stages).
    void init(const std::string& progressPath, const Campaign& main);
    int active() const { return active_; } // 0 .. COUNT - 1
    void setActive(int slot);              // (remembered)
    std::string progressPath(int slot) const;
    std::string placePath(int slot) const;       // the menu's place
    std::string voyagesDir(int slot) const;      // where the custom voyages' progress files are
    std::string globalPlacePath() const { return placePath(0); } // (the options the menu's place keeps: slot 1's)

    // What a slot holds, on the main voyage (any difficulty).
    struct Summary {
        bool empty = true;      // nothing recorded (no song beaten, no stage cleared, not unlocked)
        bool unlocked = false;  // UNLOCK ALL was used on it
        bool allUnlocked = false; // and every custom voyage open too (one made since isn't: UNLOCK ALL again opens it)
        int cleared = 0, stages = 0, beaten = 0, songs = 0;
        int64_t score = 0;      // the songs' best scores, summed (each song's best on any difficulty)
        int64_t played = 0;     // last played (seconds since the epoch; 0 unknown)
        int percent() const { return songs > 0 ? (beaten * 100 + songs / 2) / songs : 0; }
    };
    Summary summary(int slot) const;
    // Forgets a slot: its progress, its menu place and its voyages' progress (files removed).
    void erase(int slot) const;
    // Every stage open on that slot, the main voyage's and every custom voyage's, each marked so (saved).
    void unlockAll(int slot) const;

private:
    std::string base_; // slot 1's progress file
    Campaign main_;
    int active_ = 0;
    std::string dir(int slot) const;   // the folder of a slot's files
    std::string activePath() const;    // save-slot.tsv
    // The custom voyages (their campaign files, in slot 1's folder): fn(its campaign, its progress file on `slot`).
    template <class F> void forEachVoyage(int slot, F fn) const;
};

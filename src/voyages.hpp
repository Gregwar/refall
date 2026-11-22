#pragma once
// Custom voyages: a campaign made from a YouTube playlist. Up to 60 of its songs (a random choice, in the playlist's
// order, past 60), in stages of 4 (as even as can be), each song in a universe of the main campaign drawn at random
// (each once before any comes back) with a random variant, and at the velocity the main campaign has at the same point
// of its journey: its 15 stages compressed onto fewer when there are fewer songs, a stage's songs onto its own.
// Registered in the user data folder (voyages/ next to the campaign progress): an index, and per voyage its campaign
// file (data/campaign.tsv's format) and its own progress.
#include <cstdint>
#include <string>
#include <vector>

#include "campaign.hpp"

struct CustomVoyage {
    std::string id, title, url;
    int songs = 0, stages = 0;
    int64_t created = 0, lastUsed = 0; // seconds since the epoch
};

// Stages cleared and songs beaten, on any difficulty.
struct VoyageProgress {
    int cleared = 0, beaten = 0, songs = 0;
    int percent() const { return songs > 0 ? (beaten * 100 + songs / 2) / songs : 0; }
};

class VoyageStore {
public:
    static constexpr int MAX_SONGS = 60, STAGE_SONGS = 4;
    // <the progress file's folder>/voyages
    static std::string defaultDir(const std::string& progressPath);
    void load(const std::string& dir);
    bool save() const;
    const std::vector<CustomVoyage>& list() const { return list_; } // the most recently played first
    const CustomVoyage* find(const std::string& id) const;
    std::string campaignPath(const std::string& id) const;
    std::string progressPath(const std::string& id) const; // (the save slot's in use)
    // Where each save slot keeps the voyages' progress, and the slot in use (by default, next to the voyages).
    void setProgressDirs(const std::vector<std::string>& dirs, int active) { progDirs_ = dirs; progActive_ = active; }
    // The main campaign, whose velocities the voyages follow, and its progress file.
    void setMain(const Campaign& main, const std::string& progressPath) { curve_ = main; mainProgress_ = progressPath; }
    const Campaign& main() const { return curve_; }
    // Makes a voyage of `entries` (library entries, "ytdl:ID\tTITLE") and registers it: its id, or "" (*err why not).
    // `shuffle`: the songs in a random order (else the playlist's).
    std::string create(const std::string& title, const std::string& url, const std::vector<std::string>& entries,
                       uint64_t seed, std::string* err, bool shuffle = false);
    void remove(const std::string& id); // with its files (its progress on every slot)
    void used(const std::string& id);   // now its last use (it moves first)
    // How far a voyage went (read from its progress file); "" the main campaign.
    VoyageProgress progress(const std::string& id) const;

private:
    std::vector<CustomVoyage> list_;
    std::string dir_;
    std::vector<std::string> progDirs_;
    int progActive_ = 0;
    Campaign curve_;
    std::string mainProgress_;

    void sort();
};

// A voyage's campaign file, as Campaign::load reads it.
std::string makeVoyageCampaign(const std::string& title, const std::vector<std::string>& entries, const Campaign& curve,
                               uint64_t seed, bool shuffle = false);

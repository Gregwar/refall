#pragma once
// Game modes beyond the music: the difficulty (EASY, NORMAL, HARD, EXPERT), the campaign (stages of songs played in a
// row, each song at a velocity fixed by the campaign data, data/campaign.tsv) and the player's campaign progress
// (stages cleared and best scores, per difficulty, saved in the user data folder).
#include <cstdint>
#include <map>
#include <algorithm>
#include <string>
#include <vector>

#include "difficulty.hpp"

const char* difficultyName(int d); // "EASY", "NORMAL", "HARD", "EXPERT", "MASTER"
// Free play: the gravity multiplier of each difficulty (on every level's speed); HARD is the game as it was, EXPERT
// 1.4 times it, MASTER too (from the top level).
float freePlayMultiplier(int d);
// "easy" / "normal" / "hard" / "expert" / "master" (any case, or e/n/h/x/c) or a number (the multiplier itself, e.g. 0.7): false if
// neither.
// A number sets *mul and *d to the nearest difficulty; a name sets *d and its multiplier.
bool parseDifficulty(const std::string& s, int* d, float* mul);
// A Voyage song's EXPERT velocity when its line doesn't give one: HARD's + 7 (one decimal), at most Game::MAX_VELOCITY.
float expertVelocity(float hard);

struct CampaignSong {
    std::string id;       // YouTube video id
    std::string title;
    std::string universe; // "NAME" or "NAME:VARIANT" (as --universe takes it)
    float level[DIFF_COUNT] = {1, 1, 1, 1, 1}; // the velocity (1 .. Game::MAX_VELOCITY, may be fractional; MASTER: the top)
    std::string entry() const { return "ytdl:" + id + "\t" + title; } // a library entry
};
struct CampaignStage {
    std::vector<CampaignSong> songs; // 5 in the shipped campaign
};

class Campaign {
public:
    // Reads the campaign (`path` if given, else data/campaign.tsv from the sources, else next to the executable, else
    // the copy built into the game). False if no stage could be read (printed).
    bool load(const std::string& path = "");
    const std::vector<CampaignStage>& stages() const { return stages_; }
    int size() const { return (int)stages_.size(); }
    const std::string& source() const { return source_; }
    // Universes the data names that the game doesn't know are printed by load() and replaced by a picked universe.
private:
    bool parse(const std::string& text);
    std::vector<CampaignStage> stages_;
    std::string source_;
};

// Progress, per stage, song and difficulty: a song is "beaten" once survived to its end in a campaign run (started
// from any song of its stage), with its best score (the points scored during that song), the lines cleared during that
// best-scoring song and the most lines ever cleared during it; a stage is "cleared" once
// its songs are all beaten (in one run or several), and "perfect" once they were all survived in one run started from
// its first song (no game over: RETRY SONG starts another run). Stage N + 1 is unlocked on a difficulty once
// stage N is cleared on it or on a harder one, or once its songs are all beaten there, in any runs (stage 1 always is;
// reaching a stage on HARD opens it on NORMAL and EASY too, not done there but playable), or everything once UNLOCK ALL
// was used on the save slot (saveslots.hpp).
// On EXPERT a stage also opens once it is itself cleared (or its songs all beaten) on HARD; on MASTER, on EXPERT.
// The file keys songs by video id and stage clears by their songs, so that regrouping the campaign keeps the songs
// beaten (a clear whose stage no longer exists is dropped, and the stages it had opened stay open). A file of the
// first layout (12 stages of 5, keyed by index) is migrated on load, its old copy kept as <file>.bak.
// Difficulties are written by name; EXPERT is written "EXPERT2" because files of before it say "EXPERT" for HARD (its
// former name), which they still read as HARD.
class Progress {
public:
    static std::string defaultPath(); // <user data>/refall/campaign-progress.tsv
    // `camp` maps the file's song ids to the campaign's stages and songs (without it, ids aren't resolved).
    void load(const std::string& path = "", const Campaign* camp = nullptr);
    bool save() const;
    // Forgets everything (the file is removed); prints what it held.
    void reset();
    void unlockAll() { unlockAll_ = true; }
    int64_t played() const { return played_; } // when a song was last beaten or a stage cleared (0 unknown)
    void setCampaign(const Campaign* camp) { camp_ = camp; } // (an in-memory progress: the made-up one of shots)
    bool allUnlocked() const { return unlockAll_; }

    bool unlocked(int stage, int diff) const {
        if (unlockAll_ || stage <= reach_) return true;
        for (int d = std::max(0, diff); d < DIFF_COUNT; d++)
            if (cleared(stage - 1, d) || allBeaten(stage - 1, d)) return true;
        // EXPERT: a stage done on HARD opens there too (MASTER: done on EXPERT)
        if (diff >= DIFF_EXPERT && (cleared(stage, diff - 1) || allBeaten(stage, diff - 1))) return true;
        return false;
    }
    bool cleared(int stage, int diff) const;
    bool perfect(int stage, int diff) const; // a PERFECT CLEAR: every song survived in one run from the first
    bool allBeaten(int stage, int diff) const; // every song of the stage beaten on that difficulty (not in one run)
    bool clearedAny(int stage) const;
    bool beaten(int stage, int song, int diff) const;
    int best(int stage, int song, int diff) const;   // 0 when never beaten
    int stageBest(int stage, int diff) const;        // best run score of a cleared stage
    // Lines cleared, -1 when unknown (never beaten, or recorded before lines were): during the song's best-scoring
    // run, the most during the song, and the whole run of the stage's best clear.
    int lines(int stage, int song, int diff) const;
    int mostLines(int stage, int song, int diff) const;
    int stageLines(int stage, int diff) const;
    void setBeaten(int stage, int song, int diff, int score, int lines = -1);
    void setCleared(int stage, int diff, int runScore, int runLines = -1);
    void setPerfect(int stage, int diff); // (on a cleared stage only)
    int clearedCount() const;

private:
    struct SongRec { bool beaten = false; int best = 0, lines = -1, mostLines = -1; };
    struct StageRec {
        bool cleared[DIFF_COUNT] = {}, perfect[DIFF_COUNT] = {};
        int best[DIFF_COUNT] = {}, lines[DIFF_COUNT] = {-1, -1, -1, -1, -1};
        std::vector<SongRec> songs[DIFF_COUNT];
    };
    StageRec& at(int stage);
    const StageRec* get(int stage) const { return stage >= 0 && stage < (int)stages_.size() ? &stages_[stage] : nullptr; }
    std::vector<StageRec> stages_;
    bool unlockAll_ = false;
    int64_t played_ = 0;             // seconds since the epoch
    int reach_ = 0;                  // stages up to this one are open (kept from clears a regrouping dropped)
    std::string path_;
    const Campaign* camp_ = nullptr;
    std::vector<std::string> keep_;  // records of songs the campaign doesn't have (kept as they were)
    std::string stageKey(int stage) const; // its songs' ids, joined by commas
};

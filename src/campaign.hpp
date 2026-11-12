#pragma once
// Game modes beyond the music: the difficulty (EASY, NORMAL, EXPERT), the campaign (stages of songs played in a
// row, each song at a velocity fixed by the campaign data, data/campaign.tsv) and the player's campaign progress
// (stages cleared and best scores, per difficulty, saved in the user data folder).
#include <string>
#include <vector>

enum Difficulty { DIFF_EASY = 0, DIFF_NORMAL, DIFF_EXPERT, DIFF_COUNT };
const char* difficultyName(int d); // "EASY", "NORMAL", "EXPERT"
// Free play: the gravity multiplier of each difficulty (on every level's speed); EXPERT is the game as it was.
float freePlayMultiplier(int d);
// "easy" / "normal" / "expert" (any case, or e/n/x) or a number (the multiplier itself, e.g. 0.7): false if neither.
// A number sets *mul and *d to the nearest difficulty; a name sets *d and its multiplier.
bool parseDifficulty(const std::string& s, int* d, float* mul);

struct CampaignSong {
    std::string id;       // YouTube video id
    std::string title;
    std::string universe; // "NAME" or "NAME:VARIANT" (as --universe takes it)
    float level[DIFF_COUNT] = {1, 1, 1}; // the velocity, as a game level (1 .. 20, may be fractional)
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
// its songs were all survived in one run started from its first song. Stage N + 1 is unlocked once stage N is
// cleared on any difficulty (stage 1 always is), or everything with --unlock.
class Progress {
public:
    static std::string defaultPath(); // <user data>/zentris/campaign-progress.tsv
    void load(const std::string& path = "");
    bool save() const;
    // --reset: forgets everything (the file is removed); prints what it held.
    void reset();
    void unlockAll() { unlockAll_ = true; }
    bool allUnlocked() const { return unlockAll_; }

    bool unlocked(int stage) const { return unlockAll_ || stage == 0 || clearedAny(stage - 1); }
    bool cleared(int stage, int diff) const;
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
    int clearedCount() const;

private:
    struct SongRec { bool beaten = false; int best = 0, lines = -1, mostLines = -1; };
    struct StageRec {
        bool cleared[DIFF_COUNT] = {};
        int best[DIFF_COUNT] = {}, lines[DIFF_COUNT] = {-1, -1, -1};
        std::vector<SongRec> songs[DIFF_COUNT];
    };
    StageRec& at(int stage);
    const StageRec* get(int stage) const { return stage >= 0 && stage < (int)stages_.size() ? &stages_[stage] : nullptr; }
    std::vector<StageRec> stages_;
    bool unlockAll_ = false;
    std::string path_;
};

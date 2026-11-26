#include "title.hpp"
#include "campaign.hpp"
#include "game.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "platform.hpp"
#include "universes/registry.hpp"

namespace fs = std::filesystem;

namespace {
// The campaign built into the game (data/campaign.tsv at build time).
const unsigned char BUILTIN[] = {
#include "campaign.tsv.inc"
    0};

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    size_t a = 0;
    for (;;) {
        size_t b = line.find('\t', a);
        out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}
bool isNumber(const std::string& s) {
    char* end = nullptr;
    std::strtof(s.c_str(), &end);
    return !s.empty() && end && !*end;
}
} // namespace

float expertVelocity(float hard) { return std::min(Game::MAX_VELOCITY, std::round((hard + 7.f) * 10.f) / 10.f); }

const char* difficultyName(int d) {
    static const char* N[] = {"EASY", "NORMAL", "HARD", "EXPERT", "MASTER"};
    return N[std::clamp(d, 0, DIFF_COUNT - 1)];
}

float freePlayMultiplier(int d) {
    static const float M[] = {0.6f, 0.75f, 1.f, 1.4f, 1.4f};
    return M[std::clamp(d, 0, DIFF_COUNT - 1)];
}

bool parseDifficulty(const std::string& s0, int* d, float* mul) {
    std::string s;
    for (char c : s0) s += (char)std::tolower((unsigned char)c);
    for (int k = 0; k < DIFF_COUNT; k++) {
        std::string n = difficultyName(k);
        for (char& c : n) c = (char)std::tolower((unsigned char)c);
        if (s == n || (s.size() == 1 && s[0] == "enhxm"[k])) {
            *d = k;
            *mul = freePlayMultiplier(k);
            return true;
        }
    }
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (s.empty() || !end || *end || !(v > 0.f) || v > 4.f) return false;
    *mul = v;
    int best = 0;
    for (int k = 1; k < DIFF_COUNT; k++)
        if (std::fabs(freePlayMultiplier(k) - v) < std::fabs(freePlayMultiplier(best) - v)) best = k;
    *d = best;
    return true;
}

bool Campaign::load(const std::string& path) {
    std::vector<std::pair<std::string, std::string>> tries; // (source name, text)
    if (!path.empty()) tries.push_back({path, readFile(path)});
#ifdef REFALL_SOURCE_DIR
    else tries.push_back({std::string(REFALL_SOURCE_DIR) + "/data/campaign.tsv", readFile(fs::path(REFALL_SOURCE_DIR) / "data" / "campaign.tsv")});
#endif
    if (path.empty()) {
        tries.push_back({(platform::executableDir() / "campaign.tsv").string(), readFile(platform::executableDir() / "campaign.tsv")});
        tries.push_back({"(built in)", std::string((const char*)BUILTIN)});
    }
    for (auto& [src, text] : tries) {
        if (text.empty()) continue;
        if (parse(text)) {
            source_ = src;
            std::printf("[campaign] %d stages from %s\n", size(), src.c_str());
            return true;
        }
        std::fprintf(stderr, "[campaign] no stage in %s\n", src.c_str());
    }
    if (!path.empty()) std::fprintf(stderr, "[campaign] could not read %s\n", path.c_str());
    return false;
}

bool Campaign::parse(const std::string& text) {
    stages_.clear();
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        n++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c = splitTabs(line);
        if (c.size() < 7) {
            std::fprintf(stderr, "[campaign] line %d: expected stage, song, id, universe, easy, normal, hard, expert, title\n", n);
            continue;
        }
        const int stage = std::atoi(c[0].c_str()), song = std::atoi(c[1].c_str());
        if (stage < 1 || stage > 99 || song < 1 || song > 20) {
            std::fprintf(stderr, "[campaign] line %d: bad stage or song number\n", n);
            continue;
        }
        CampaignSong s;
        s.id = c[2];
        s.universe = c[3];
        for (int d = 0; d <= DIFF_HARD; d++) s.level[d] = std::clamp((float)std::atof(c[4 + d].c_str()), 1.f, Game::MAX_VELOCITY);
        // EXPERT's column (after HARD's) may be missing in older or custom files: HARD + 7 then. It is there when the
        // eighth column is a number followed by a title.
        const bool expert = c.size() > 8 && isNumber(c[7]);
        s.level[DIFF_EXPERT] = expert ? std::clamp((float)std::atof(c[7].c_str()), 1.f, Game::MAX_VELOCITY)
                                      : expertVelocity(s.level[DIFF_HARD]);
        s.level[DIFF_MASTER] = Game::MAX_VELOCITY; // (every song at the top)
        const size_t t = expert ? 8 : 7;
        s.title = c.size() > t ? c[t] : s.id;
        int v = -1;
        if (!s.universe.empty() && universes::parse(s.universe, &v) < 0) {
            std::fprintf(stderr, "[campaign] line %d: unknown universe '%s' (one is picked for the song)\n", n, s.universe.c_str());
            s.universe.clear();
        }
        if ((int)stages_.size() < stage) stages_.resize(stage);
        auto& songs = stages_[stage - 1].songs;
        if ((int)songs.size() < song) songs.resize(song);
        songs[song - 1] = s;
    }
    // Drop holes (a stage or a song number skipped).
    for (auto& st : stages_)
        st.songs.erase(std::remove_if(st.songs.begin(), st.songs.end(), [](const CampaignSong& s) { return s.id.empty(); }),
                       st.songs.end());
    stages_.erase(std::remove_if(stages_.begin(), stages_.end(), [](const CampaignStage& s) { return s.songs.empty(); }),
                  stages_.end());
    return !stages_.empty();
}

// ---- Progress.

namespace {
// The first campaign layout (12 stages of 5 songs): progress files written then key songs by (stage, song).
const char* const LEGACY_12x5[12][5] = {
    {"H_9xqc084UA", "pJguiXxgaFo", "YyFyudLOkeA", "gjTkkswfVLE", "agu39idW5VY"},
    {"7PcQtXcOxrY", "uOFTqVi-qp4", "HZ6L_6a7RcQ", "8ayH60ET4U0", "JNVXi3QpaMo"},
    {"YrAc_iVKn48", "1TlckH2mCmI", "bCneCCvp0N0", "iZAqaykiS70", "u40t4XMxmMc"},
    {"oHRrxtcm21Q", "PfPfGknEjXE", "sUwm6SjlpJ0", "F0R7LKT9ZaU", "S9KsOqTftFA"},
    {"komlDqhaAKQ", "pLuQ0MGLBXU", "qbGZDEFHBmc", "TkceGi1EXzk", "IhVvPrGh48E"},
    {"3KFPGWIfLnM", "Og0A10H7YqU", "xhpudFvsMjU", "S9ld5hiKg8w", "UKou-bIHgYA"},
    {"lTgIZtKoYQc", "nXIZCMwFKis", "Zy7XaQdWbEg", "D5drYkLiLI8", "QBcX3cZQdek"},
    {"MlwBZ2MSNtE", "r-sGtrldNKc", "_6-AtgXd27w", "6xD7YhdzmIc", "TsdT5bSGypc"},
    {"KgkQ_HvNjSQ", "z2qstV9p-EU", "heUI3TYAZjw", "N2hKqI8nU7w", "eqc50IErJxo"},
    {"iWiilOH4cQE", "fMvEOabCEFc", "zjANqMu7m2c", "ko1x5v-zaNU", "6QvMcQ2Eejo"},
    {"n1ToxNTIyZc", "VUzg5liIjcs", "qdjmd9J6cr0", "dP7KuXdb-wE", "BSVOAD-Nyfc"},
    {"4V3exJW1-Ig", "jmnBSB7DCTI", "MHWLaNbUx7M", "a0W9bS7qZSk", "9Sh9ZSecyFU"},
};
} // namespace

namespace {
// A difficulty in the progress file: its name, but EXPERT2 for EXPERT (see load()).
const char* fileName(int d) { return d == DIFF_EXPERT ? "EXPERT2" : difficultyName(d); }
} // namespace

std::string Progress::defaultPath() { return (platform::appDataDir() / "campaign-progress.tsv").string(); }

Progress::StageRec& Progress::at(int stage) {
    if ((int)stages_.size() <= stage) stages_.resize(stage + 1);
    return stages_[stage];
}

bool Progress::cleared(int stage, int diff) const { const StageRec* r = get(stage); return r && r->cleared[diff]; }
bool Progress::perfect(int stage, int diff) const { const StageRec* r = get(stage); return r && r->cleared[diff] && r->perfect[diff]; }
bool Progress::clearedAny(int stage) const {
    for (int d = 0; d < DIFF_COUNT; d++)
        if (cleared(stage, d)) return true;
    return false;
}
int Progress::stageBest(int stage, int diff) const { const StageRec* r = get(stage); return r ? r->best[diff] : 0; }
bool Progress::beaten(int stage, int song, int diff) const {
    const StageRec* r = get(stage);
    return r && song < (int)r->songs[diff].size() && r->songs[diff][song].beaten;
}
bool Progress::allBeaten(int stage, int diff) const {
    if (!camp_ || stage < 0 || stage >= camp_->size() || camp_->stages()[stage].songs.empty()) return false;
    for (int k = 0; k < (int)camp_->stages()[stage].songs.size(); k++)
        if (!beaten(stage, k, diff)) return false;
    return true;
}
int Progress::best(int stage, int song, int diff) const {
    const StageRec* r = get(stage);
    return r && song < (int)r->songs[diff].size() ? r->songs[diff][song].best : 0;
}
int Progress::lines(int stage, int song, int diff) const {
    const StageRec* r = get(stage);
    return r && song < (int)r->songs[diff].size() && r->songs[diff][song].beaten ? r->songs[diff][song].lines : -1;
}
int Progress::mostLines(int stage, int song, int diff) const {
    const StageRec* r = get(stage);
    return r && song < (int)r->songs[diff].size() && r->songs[diff][song].beaten ? r->songs[diff][song].mostLines : -1;
}
int Progress::stageLines(int stage, int diff) const { const StageRec* r = get(stage); return r && r->cleared[diff] ? r->lines[diff] : -1; }
void Progress::setBeaten(int stage, int song, int diff, int score, int lines) {
    auto& v = at(stage).songs[diff];
    if ((int)v.size() <= song) v.resize(song + 1);
    SongRec& s = v[song];
    // the lines go with the best score (a tie keeps known lines over unknown ones)
    if (!s.beaten || score > s.best || (score == s.best && s.lines < 0)) { s.best = score; s.lines = lines; }
    s.mostLines = std::max(s.mostLines, lines);
    s.beaten = true;
    played_ = (int64_t)std::time(nullptr);
}
void Progress::setCleared(int stage, int diff, int runScore, int runLines) {
    StageRec& r = at(stage);
    if (!r.cleared[diff] || runScore > r.best[diff] || (runScore == r.best[diff] && r.lines[diff] < 0)) {
        r.best[diff] = runScore;
        r.lines[diff] = runLines;
    }
    r.cleared[diff] = true;
    played_ = (int64_t)std::time(nullptr);
}
void Progress::setPerfect(int stage, int diff) {
    StageRec& r = at(stage);
    if (r.cleared[diff]) r.perfect[diff] = true;
}
int Progress::clearedCount() const {
    int n = 0;
    for (int s = 0; s < (int)stages_.size(); s++) n += clearedAny(s);
    return n;
}

std::string Progress::stageKey(int stage) const {
    std::string k;
    if (!camp_ || stage < 0 || stage >= camp_->size()) return k;
    for (auto& song : camp_->stages()[stage].songs) k += (k.empty() ? "" : ",") + song.id;
    return k;
}

// The file, one record per line (tabs):
//   unlock	all	(UNLOCK ALL used on the save slot)
//   played	<seconds since the epoch>	(the last song beaten or stage cleared)
//   reach	<stage 1..>	(stages up to it open)
//   stage	<stage 1..>	<DIFF>	<best run score>	<its lines>	<its songs' ids, comma separated>	[perfect]
//   songid	<video id>	<DIFF>	<best score>	<its lines>	<most lines>
// DIFF is EASY, NORMAL, HARD, EXPERT2 or MASTER (EXPERT: a bare EXPERT is HARD's former name, read as HARD, so that files
// written before EXPERT existed keep their HARD results). Lines are -1 when unknown. Older files: "song <stage> <song> <diff> <best> [<lines> <most>]" and stage lines
// without the ids (the 12 x 5 layout), migrated on load.
void Progress::load(const std::string& path, const Campaign* camp) {
    path_ = path.empty() ? defaultPath() : path;
    camp_ = camp && camp->size() > 0 ? camp : nullptr;
    stages_.clear();
    keep_.clear();
    unlockAll_ = false;
    played_ = 0;
    reach_ = 0;
    std::map<std::string, std::pair<int, int>> where; // video id -> (stage, song) in the campaign
    if (camp_)
        for (int s = 0; s < camp_->size(); s++)
            for (int k = 0; k < (int)camp_->stages()[s].songs.size(); k++) where[camp_->stages()[s].songs[k].id] = {s, k};
    auto stageOf = [&](const std::string& id) { auto it = where.find(id); return it == where.end() ? -1 : it->second.first; };
    const std::string text = readFile(path_);
    std::istringstream in(text);
    std::string line;
    auto diffOf = [](const std::string& s) {
        for (int d = 0; d < DIFF_COUNT; d++)
            if (s == fileName(d)) return d;
        if (s == "HARDCORE") return (int)DIFF_MASTER; // (MASTER's name while it was being tried)
        return s == "EXPERT" ? (int)DIFF_HARD : -1;   // (HARD's former name, in older progress files)
    };
    auto num = [](const std::vector<std::string>& c, size_t i) { return c.size() > i && !c[i].empty() ? std::atoi(c[i].c_str()) : -1; };
    bool legacy = false;
    int dropped = 0;
    int64_t played = 0;
    auto song = [&](const std::string& id, int d, const std::vector<std::string>& c, size_t i, const std::string& raw) {
        auto it = where.find(id);
        if (it == where.end()) { keep_.push_back(raw); return; }
        const int s = it->second.first, k = it->second.second;
        setBeaten(s, k, d, std::atoi(c[i].c_str()), num(c, i + 1));
        SongRec& r = at(s).songs[d][k];
        r.mostLines = std::max(r.mostLines, num(c, i + 2));
    };
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> c = splitTabs(line);
        if (c[0] == "unlock") unlockAll_ = true;
        else if (c[0] == "played" && c.size() >= 2) played = std::atoll(c[1].c_str());
        else if (c[0] == "reach" && c.size() >= 2) reach_ = std::max(reach_, std::atoi(c[1].c_str()) - 1);
        else if (c[0] == "stage" && c.size() >= 4) {
            const int s = std::atoi(c[1].c_str()) - 1, d = diffOf(c[2]);
            if (s < 0 || s >= 100 || d < 0) continue;
            const bool perf = c.size() >= 7 && c[6] == "perfect"; // (a column older versions ignore)
            if (!camp_ || (c.size() >= 6 && c[5] == stageKey(s))) {
                setCleared(s, d, std::atoi(c[3].c_str()), num(c, 4));
                if (perf) setPerfect(s, d);
                continue;
            }
            // a clear of a stage that is no longer one (regrouped): dropped, but the stage after it stays open
            std::string last;
            if (c.size() >= 6) last = c[5].substr(c[5].rfind(',') == std::string::npos ? 0 : c[5].rfind(',') + 1);
            else if (s < 12) { legacy = true; last = LEGACY_12x5[s][4]; }
            if (stageOf(last) >= 0) reach_ = std::max(reach_, std::min(camp_->size() - 1, stageOf(last) + 1));
            dropped++;
        } else if (c[0] == "songid" && c.size() >= 4) {
            const int d = diffOf(c[2]);
            if (d >= 0) song(c[1], d, c, 3, line);
        } else if (c[0] == "song" && c.size() >= 5) {
            const int s = std::atoi(c[1].c_str()) - 1, k = std::atoi(c[2].c_str()) - 1, d = diffOf(c[3]);
            if (s < 0 || s >= 100 || k < 0 || k >= 20 || d < 0) continue;
            if (!camp_) { // (nothing to map with: by index, as written)
                setBeaten(s, k, d, std::atoi(c[4].c_str()), num(c, 5));
                continue;
            }
            legacy = true;
            if (s < 12 && k < 5) song(LEGACY_12x5[s][k], d, c, 4, "");
        }
    }
    played_ = played; // (the records read set it to now)
    if (!played_ && !stages_.empty()) { // (a file of before the record: its last change)
        std::error_code ec;
        const auto t = fs::last_write_time(path_, ec);
        if (!ec)
            played_ = std::chrono::duration_cast<std::chrono::seconds>(
                          (t - fs::file_time_type::clock::now() + std::chrono::system_clock::now()).time_since_epoch()).count();
    }
    // the stages where songs were beaten are open (they were reached)
    if (legacy || dropped)
        for (int s = 0; s < (int)stages_.size(); s++)
            for (int d = 0; d < DIFF_COUNT; d++)
                for (auto& r : stages_[s].songs[d])
                    if (r.beaten) reach_ = std::max(reach_, s);
    keep_.erase(std::remove(keep_.begin(), keep_.end(), std::string()), keep_.end());
    if (legacy || dropped) {
        int songs = 0;
        for (auto& r : stages_)
            for (int d = 0; d < DIFF_COUNT; d++)
                for (auto& x : r.songs[d]) songs += x.beaten;
        std::printf("[campaign] progress regrouped to %d stages: %d beaten songs kept, %d stage clears dropped, stages 1-%d open%s\n",
                    camp_->size(), songs, dropped, reach_ + 1, legacy ? " (from the 12 x 5 layout)" : "");
        if (legacy) { // the old file kept aside, once
            std::error_code ec;
            if (!fs::exists(path_ + ".bak", ec)) {
                fs::copy_file(path_, path_ + ".bak", ec);
                std::printf("[campaign] the old progress kept as %s.bak\n", path_.c_str());
            }
        }
        save();
    }
}

bool Progress::save() const {
    if (path_.empty()) return false; // (an in-memory progress, never loaded: the made-up one of shots)
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    const std::string tmp = path_ + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) { std::fprintf(stderr, "[campaign] could not save %s\n", path_.c_str()); return false; }
    std::fprintf(f, "# %s campaign progress (SAVE SLOTS > ERASE forgets it)\n", GAME_TITLE);
    if (unlockAll_) std::fprintf(f, "unlock\tall\n");
    if (played_ > 0) std::fprintf(f, "played\t%lld\n", (long long)played_);
    if (reach_ > 0) std::fprintf(f, "reach\t%d\n", reach_ + 1);
    for (int s = 0; s < (int)stages_.size(); s++)
        for (int d = 0; d < DIFF_COUNT; d++) {
            if (stages_[s].cleared[d])
                std::fprintf(f, "stage\t%d\t%s\t%d\t%d\t%s%s\n", s + 1, fileName(d), stages_[s].best[d], stages_[s].lines[d],
                             stageKey(s).c_str(), stages_[s].perfect[d] ? "\tperfect" : "");
            for (int k = 0; k < (int)stages_[s].songs[d].size(); k++) {
                const SongRec& r = stages_[s].songs[d][k];
                if (!r.beaten) continue;
                if (camp_ && s < camp_->size() && k < (int)camp_->stages()[s].songs.size())
                    std::fprintf(f, "songid\t%s\t%s\t%d\t%d\t%d\n", camp_->stages()[s].songs[k].id.c_str(), fileName(d), r.best,
                                 r.lines, r.mostLines);
                else std::fprintf(f, "song\t%d\t%d\t%s\t%d\t%d\t%d\n", s + 1, k + 1, fileName(d), r.best, r.lines, r.mostLines);
            }
        }
    for (auto& l : keep_) std::fprintf(f, "%s\n", l.c_str());
    std::fclose(f);
    fs::rename(tmp, path_, ec);
    if (ec) { std::fprintf(stderr, "[campaign] could not save %s\n", path_.c_str()); return false; }
    return true;
}

void Progress::reset() {
    int songs = 0, cleared = 0;
    for (auto& r : stages_)
        for (int d = 0; d < DIFF_COUNT; d++) {
            cleared += r.cleared[d];
            for (auto& s : r.songs[d]) songs += s.beaten;
        }
    std::printf("[campaign] progress reset: %d stage clears and %d beaten songs forgotten%s (%s)\n", cleared, songs,
                unlockAll_ ? ", stages locked again" : "", path_.c_str());
    stages_.clear();
    keep_.clear();
    unlockAll_ = false;
    played_ = 0;
    reach_ = 0;
    std::error_code ec;
    fs::remove(path_, ec);
}

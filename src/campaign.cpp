#include "title.hpp"
#include "campaign.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
} // namespace

const char* difficultyName(int d) {
    static const char* N[] = {"EASY", "NORMAL", "EXPERT"};
    return N[std::clamp(d, 0, DIFF_COUNT - 1)];
}

float freePlayMultiplier(int d) {
    static const float M[] = {0.6f, 0.8f, 1.f};
    return M[std::clamp(d, 0, DIFF_COUNT - 1)];
}

bool parseDifficulty(const std::string& s0, int* d, float* mul) {
    std::string s;
    for (char c : s0) s += (char)std::tolower((unsigned char)c);
    for (int k = 0; k < DIFF_COUNT; k++) {
        std::string n = difficultyName(k);
        for (char& c : n) c = (char)std::tolower((unsigned char)c);
        if (s == n || (s.size() == 1 && s[0] == "enx"[k])) {
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
#ifdef ZEN_SOURCE_DIR
    else tries.push_back({std::string(ZEN_SOURCE_DIR) + "/data/campaign.tsv", readFile(fs::path(ZEN_SOURCE_DIR) / "data" / "campaign.tsv")});
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
            std::fprintf(stderr, "[campaign] line %d: expected stage, song, id, universe, easy, normal, expert, title\n", n);
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
        for (int d = 0; d < DIFF_COUNT; d++) s.level[d] = std::clamp((float)std::atof(c[4 + d].c_str()), 1.f, 20.f);
        s.title = c.size() > 7 ? c[7] : s.id;
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

std::string Progress::defaultPath() { return (platform::dataDir() / "zentris" / "campaign-progress.tsv").string(); }

Progress::StageRec& Progress::at(int stage) {
    if ((int)stages_.size() <= stage) stages_.resize(stage + 1);
    return stages_[stage];
}

bool Progress::cleared(int stage, int diff) const { const StageRec* r = get(stage); return r && r->cleared[diff]; }
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
}
void Progress::setCleared(int stage, int diff, int runScore, int runLines) {
    StageRec& r = at(stage);
    if (!r.cleared[diff] || runScore > r.best[diff] || (runScore == r.best[diff] && r.lines[diff] < 0)) {
        r.best[diff] = runScore;
        r.lines[diff] = runLines;
    }
    r.cleared[diff] = true;
}
int Progress::clearedCount() const {
    int n = 0;
    for (int s = 0; s < (int)stages_.size(); s++) n += clearedAny(s);
    return n;
}

// The file, one record per line (tabs):
//   unlock	all
//   stage	<stage 1..>	<EASY|NORMAL|EXPERT>	<best run score>	[<its lines>]
//   song	<stage 1..>	<song 1..>	<EASY|NORMAL|EXPERT>	<best score>	[<its lines>	<most lines>]
// (the lines columns came later: a file without them loads, its lines unknown; -1 is unknown)
void Progress::load(const std::string& path) {
    path_ = path.empty() ? defaultPath() : path;
    stages_.clear();
    unlockAll_ = false;
    std::istringstream in(readFile(path_));
    std::string line;
    auto diffOf = [](const std::string& s) {
        for (int d = 0; d < DIFF_COUNT; d++)
            if (s == difficultyName(d)) return d;
        return -1;
    };
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> c = splitTabs(line);
        if (c[0] == "unlock") unlockAll_ = true;
        else if (c[0] == "stage" && c.size() >= 4) {
            const int s = std::atoi(c[1].c_str()) - 1, d = diffOf(c[2]);
            if (s >= 0 && s < 100 && d >= 0) setCleared(s, d, std::atoi(c[3].c_str()), c.size() >= 5 && !c[4].empty() ? std::atoi(c[4].c_str()) : -1);
        } else if (c[0] == "song" && c.size() >= 5) {
            const int s = std::atoi(c[1].c_str()) - 1, k = std::atoi(c[2].c_str()) - 1, d = diffOf(c[3]);
            if (s >= 0 && s < 100 && k >= 0 && k < 20 && d >= 0) {
                setBeaten(s, k, d, std::atoi(c[4].c_str()), c.size() >= 6 && !c[5].empty() ? std::atoi(c[5].c_str()) : -1);
                SongRec& r = at(s).songs[d][k];
                if (c.size() >= 7 && !c[6].empty()) r.mostLines = std::max(r.mostLines, std::atoi(c[6].c_str()));
            }
        }
    }
}

bool Progress::save() const {
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    const std::string tmp = path_ + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) { std::fprintf(stderr, "[campaign] could not save %s\n", path_.c_str()); return false; }
    std::fprintf(f, "# %s campaign progress (--reset forgets it)\n", GAME_TITLE);
    if (unlockAll_) std::fprintf(f, "unlock\tall\n");
    for (int s = 0; s < (int)stages_.size(); s++)
        for (int d = 0; d < DIFF_COUNT; d++) {
            if (stages_[s].cleared[d])
                std::fprintf(f, "stage\t%d\t%s\t%d\t%d\n", s + 1, difficultyName(d), stages_[s].best[d], stages_[s].lines[d]);
            for (int k = 0; k < (int)stages_[s].songs[d].size(); k++) {
                const SongRec& r = stages_[s].songs[d][k];
                if (r.beaten) std::fprintf(f, "song\t%d\t%d\t%s\t%d\t%d\t%d\n", s + 1, k + 1, difficultyName(d), r.best, r.lines, r.mostLines);
            }
        }
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
    unlockAll_ = false;
    std::error_code ec;
    fs::remove(path_, ec);
}

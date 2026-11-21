#include "voyages.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "universes/registry.hpp"
#include "youtube.hpp"

namespace fs = std::filesystem;

namespace {
int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(line);
    while (std::getline(in, cur, '\t')) out.push_back(cur);
    return out;
}

// Tabs and line breaks can't be in a field.
std::string clean(std::string s) {
    for (char& c : s)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

// The main campaign's velocity at a point of its journey: `s` its stage (0 .. stages - 1, fractional between two) and
// `k` how far along that stage (0 its first song .. 1 its last), interpolated between its songs.
float curveLevel(const Campaign& c, float s, float k, int d) {
    auto at = [&](int i) {
        const auto& songs = c.stages()[i].songs;
        const float x = k * (float)(songs.size() - 1);
        const int a = std::min((int)x, (int)songs.size() - 1), b = std::min(a + 1, (int)songs.size() - 1);
        return songs[a].level[d] + (songs[b].level[d] - songs[a].level[d]) * (x - (float)a);
    };
    const int a = std::min((int)s, c.size() - 1), b = std::min(a + 1, c.size() - 1);
    return at(a) + (at(b) - at(a)) * (s - (float)a);
}
} // namespace

std::string makeVoyageCampaign(const std::string& title, const std::vector<std::string>& entries0, const Campaign& curve,
                               uint64_t seed) {
    std::mt19937_64 rng(seed);
    // the songs: all of them up to 60, else 60 drawn at random (kept in the playlist's order)
    std::vector<std::string> entries = entries0;
    if ((int)entries.size() > VoyageStore::MAX_SONGS) {
        std::vector<int> idx(entries.size());
        for (int i = 0; i < (int)idx.size(); i++) idx[i] = i;
        std::shuffle(idx.begin(), idx.end(), rng);
        idx.resize(VoyageStore::MAX_SONGS);
        std::sort(idx.begin(), idx.end());
        std::vector<std::string> pick;
        for (int i : idx) pick.push_back(entries[i]);
        entries = pick;
    }
    const int n = (int)entries.size();
    const int S = std::max(1, (n + VoyageStore::STAGE_SONGS - 1) / VoyageStore::STAGE_SONGS);
    // the universes: the main campaign's (each once, in a shuffled deck, dealt again when it runs out)
    std::vector<int> pool;
    for (const CampaignStage& st : curve.stages())
        for (const CampaignSong& s : st.songs) {
            int v = -1;
            const int u = universes::parse(s.universe, &v);
            if (u >= 0 && std::find(pool.begin(), pool.end(), u) == pool.end()) pool.push_back(u);
        }
    if (pool.empty())
        for (int u = 0; u < universes::count(); u++)
            if (!universes::entry(u).hidden) pool.push_back(u);
    std::vector<int> deck;
    std::vector<int> stageUs;
    auto deal = [&]() {
        for (int tries = 0; tries < 8; tries++) {
            if (deck.empty()) { deck = pool; std::shuffle(deck.begin(), deck.end(), rng); }
            const int u = deck.back();
            deck.pop_back();
            // a reshuffled deck may deal a universe of this stage again: put it back under the deck
            if (std::find(stageUs.begin(), stageUs.end(), u) != stageUs.end() && (int)pool.size() > VoyageStore::STAGE_SONGS) {
                deck.insert(deck.begin(), u);
                continue;
            }
            return u;
        }
        return pool[rng() % pool.size()];
    };
    std::ostringstream o;
    o << "# Refall custom voyage: " << clean(title) << "\n"
      << "# stage, song, video id, universe, velocity on EASY, NORMAL, HARD, EXPERT, title (as data/campaign.tsv)\n";
    int i = 0;
    for (int s = 0; s < S; s++) {
        // stages of 4, the first ones taking one more when the songs don't divide evenly
        const int ns = n / S + (s < n % S ? 1 : 0);
        stageUs.clear();
        for (int k = 0; k < ns; k++, i++) {
            const std::string& e = entries[i];
            const size_t tab = e.find('\t');
            const std::string id = e.substr(5, tab == std::string::npos ? std::string::npos : tab - 5);
            const std::string name = youtubeEntryTitle(e);
            const int u = deal();
            stageUs.push_back(u);
            const auto& vars = universes::entry(u).variants;
            std::string spec = universes::entry(u).name;
            if (vars.size() > 1) spec += ":" + vars[rng() % vars.size()];
            const float cs = S > 1 ? (float)s * (float)(curve.size() - 1) / (float)(S - 1) : 0.f;
            const float ck = ns > 1 ? (float)k / (float)(ns - 1) : 0.f;
            char lv[64];
            float L[DIFF_COUNT] = {1, 1, 1, 1};
            if (curve.size() > 0)
                for (int d = 0; d < DIFF_COUNT; d++) L[d] = curveLevel(curve, cs, ck, d);
            std::snprintf(lv, sizeof(lv), "%.1f\t%.1f\t%.1f\t%.1f", L[0], L[1], L[2], L[3]);
            o << s + 1 << '\t' << k + 1 << '\t' << id << '\t' << spec << '\t' << lv << '\t' << clean(name) << '\n';
        }
    }
    return o.str();
}

std::string VoyageStore::defaultDir(const std::string& progressPath) {
    const std::string p = progressPath.empty() ? Progress::defaultPath() : progressPath;
    return (fs::path(p).parent_path() / "voyages").string();
}

// The index: one "id<TAB>created<TAB>last used<TAB>songs<TAB>stages<TAB>url<TAB>title" per voyage.
void VoyageStore::load(const std::string& dir) {
    dir_ = dir;
    list_.clear();
    std::ifstream f(fs::path(dir_) / "voyages.tsv");
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto c = splitTabs(line);
        if (c.size() < 7) continue;
        CustomVoyage v;
        v.id = c[0];
        v.created = std::atoll(c[1].c_str());
        v.lastUsed = std::atoll(c[2].c_str());
        v.songs = std::atoi(c[3].c_str());
        v.stages = std::atoi(c[4].c_str());
        v.url = c[5];
        v.title = c[6];
        std::error_code ec;
        if (!v.id.empty() && fs::exists(campaignPath(v.id), ec)) list_.push_back(v);
    }
    sort();
}

bool VoyageStore::save() const {
    if (dir_.empty()) return false;
    std::error_code ec;
    fs::create_directories(dir_, ec);
    const fs::path path = fs::path(dir_) / "voyages.tsv", tmp = fs::path(dir_) / "voyages.tsv.tmp";
    {
        std::ofstream f(tmp);
        if (!f) return false;
        f << "# Refall custom voyages: id, created, last used, songs, stages, playlist, title\n";
        for (const CustomVoyage& v : list_)
            f << v.id << '\t' << v.created << '\t' << v.lastUsed << '\t' << v.songs << '\t' << v.stages << '\t' << clean(v.url)
              << '\t' << clean(v.title) << '\n';
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

void VoyageStore::sort() {
    std::stable_sort(list_.begin(), list_.end(), [](const CustomVoyage& a, const CustomVoyage& b) {
        return std::max(a.lastUsed, a.created) > std::max(b.lastUsed, b.created);
    });
}

const CustomVoyage* VoyageStore::find(const std::string& id) const {
    for (const CustomVoyage& v : list_)
        if (v.id == id) return &v;
    return nullptr;
}

std::string VoyageStore::campaignPath(const std::string& id) const { return (fs::path(dir_) / (id + ".tsv")).string(); }
std::string VoyageStore::progressPath(const std::string& id) const {
    const std::string d = progActive_ < (int)progDirs_.size() ? progDirs_[progActive_] : dir_;
    return (fs::path(d) / (id + "-progress.tsv")).string();
}

std::string VoyageStore::create(const std::string& title, const std::string& url, const std::vector<std::string>& entries,
                                uint64_t seed, std::string* err) {
    std::vector<std::string> songs;
    for (const std::string& e : entries)
        if (isYoutubeEntry(e)) songs.push_back(e);
    if (songs.empty()) { if (err) *err = "No song in this playlist"; return ""; }
    if (curve_.size() == 0) { if (err) *err = "No campaign to follow"; return ""; }
    char id[32];
    std::snprintf(id, sizeof(id), "v%llx%04x", (unsigned long long)now(), (unsigned)(seed & 0xffff));
    std::error_code ec;
    fs::create_directories(dir_, ec);
    {
        std::ofstream f(campaignPath(id));
        if (!f) { if (err) *err = "Couldn't write " + campaignPath(id); return ""; }
        f << makeVoyageCampaign(title, songs, curve_, seed);
    }
    Campaign c;
    if (!c.load(campaignPath(id))) { if (err) *err = "Couldn't read the voyage back"; fs::remove(campaignPath(id), ec); return ""; }
    CustomVoyage v;
    v.id = id;
    v.title = title;
    v.url = url;
    int n = 0;
    for (const CampaignStage& st : c.stages()) n += (int)st.songs.size();
    v.songs = n;
    v.stages = c.size();
    v.created = v.lastUsed = now();
    list_.push_back(v);
    sort();
    save();
    std::printf("[voyages] %s: \"%s\", %d songs in %d stages (%s)\n", v.id.c_str(), title.c_str(), v.songs, v.stages, url.c_str());
    return v.id;
}

void VoyageStore::remove(const std::string& id) {
    list_.erase(std::remove_if(list_.begin(), list_.end(), [&](const CustomVoyage& v) { return v.id == id; }), list_.end());
    std::error_code ec;
    fs::remove(campaignPath(id), ec);
    fs::remove(progressPath(id), ec);
    for (const std::string& d : progDirs_) fs::remove(fs::path(d) / (id + "-progress.tsv"), ec);
    save();
}

void VoyageStore::used(const std::string& id) {
    for (CustomVoyage& v : list_)
        if (v.id == id) v.lastUsed = now();
    sort();
    save();
}

VoyageProgress VoyageStore::progress(const std::string& id) const {
    VoyageProgress r;
    Campaign c;
    if (!id.empty() && (!find(id) || !c.load(campaignPath(id)))) return r;
    const Campaign& camp = id.empty() ? curve_ : c;
    Progress p;
    p.load(id.empty() ? mainProgress_ : progressPath(id), &camp);
    r.cleared = p.clearedCount();
    for (int s = 0; s < camp.size(); s++)
        for (int k = 0; k < (int)camp.stages()[s].songs.size(); k++) {
            r.songs++;
            bool b = false;
            for (int d = 0; d < DIFF_COUNT; d++) b = b || p.beaten(s, k, d);
            r.beaten += b;
        }
    return r;
}

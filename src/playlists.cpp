#include "playlists.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "platform.hpp"

namespace fs = std::filesystem;

namespace {
bool idChar(char c) { return std::isalnum((unsigned char)c) || c == '-' || c == '_'; }
std::string idAfter(const std::string& s, size_t pos) {
    size_t e = pos;
    while (e < s.size() && idChar(s[e])) e++;
    return s.substr(pos, e - pos);
}
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
} // namespace

std::string normalizePlaylistInput(const std::string& in0, std::string* err) {
    const std::string in = trim(in0);
    auto fail = [&](const char* why) { if (err) *err = why; return std::string(); };
    if (in.empty()) return fail("Type or paste a YouTube playlist link");
    if (size_t p = in.find("list="); p != std::string::npos) {
        const std::string id = idAfter(in, p + 5);
        if (id.size() < 10) return fail("This playlist ID looks too short");
        return "https://www.youtube.com/playlist?list=" + id;
    }
    const bool bare = std::all_of(in.begin(), in.end(), idChar);
    if (bare && in.size() >= 12) {
        static const char* PRE[] = {"PL", "OL", "UU", "FL", "LL", "RD"};
        for (const char* p : PRE)
            if (in.rfind(p, 0) == 0) return "https://www.youtube.com/playlist?list=" + in;
    }
    for (const char* key : {"watch?v=", "youtu.be/", "shorts/", "&v="}) {
        if (size_t p = in.find(key); p != std::string::npos) {
            const std::string id = idAfter(in, p + std::string(key).size());
            if (id.size() == 11) return "https://www.youtube.com/watch?v=" + id;
            return fail("This video ID doesn't look right");
        }
    }
    if (bare && in.size() == 11) return "https://www.youtube.com/watch?v=" + in;
    return fail("Not a YouTube playlist or video link");
}

std::string playlistIdOf(const std::string& url) {
    const size_t p = url.find("list=");
    return p == std::string::npos ? std::string() : idAfter(url, p + 5);
}

std::string youtubeTitle(const std::string& url) {
    std::string tool = platform::findExecutable("yt-dlp");
    if (tool.empty()) {
        std::error_code ec;
        const fs::path p = platform::homeDir() / ".local" / "bin" / "yt-dlp";
        if (fs::exists(p, ec)) tool = p.string();
    }
    if (tool.empty()) return "";
    const bool list = url.find("list=") != std::string::npos;
    const std::string cmd = platform::quoteArg(tool) + " --flat-playlist --playlist-items 1 --no-warnings --print " +
                            platform::quoteArg(list ? "%(playlist_title)s" : "%(title)s") + " " + platform::quoteArg(url);
    FILE* f = platform::openRead(cmd);
    if (!f) return "";
    char buf[512];
    std::string title;
    if (std::fgets(buf, sizeof(buf), f)) title = trim(buf);
    platform::closeRead(f);
    if (title == "NA") title.clear();
    return title;
}

std::string PlaylistStore::defaultPath() { return (platform::dataDir() / "zentris" / "playlists.tsv").string(); }

// One playlist per line: url <TAB> title <TAB> songs <TAB> last use (seconds since the epoch).
void PlaylistStore::load(const std::string& path) {
    path_ = path.empty() ? defaultPath() : path;
    list_.clear();
    std::ifstream f(path_);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, '\t')) c.push_back(cell);
        if (c.empty() || c[0].empty()) continue;
        PlaylistEntry e;
        e.url = c[0];
        if (c.size() > 1) e.title = c[1];
        if (c.size() > 2) e.count = std::atoi(c[2].c_str());
        if (c.size() > 3) e.lastUsed = std::atoll(c[3].c_str());
        list_.push_back(e);
    }
    std::stable_sort(list_.begin(), list_.end(), [](const PlaylistEntry& a, const PlaylistEntry& b) { return a.lastUsed > b.lastUsed; });
}

bool PlaylistStore::save() const {
    if (path_.empty()) return false;
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    std::ofstream f(path_ + ".tmp");
    if (!f) return false;
    f << "# Refall: the playlists played in free play (url, title, songs, last use)\n";
    for (const PlaylistEntry& e : list_) {
        std::string t = e.title;
        std::replace(t.begin(), t.end(), '\t', ' ');
        f << e.url << '\t' << t << '\t' << e.count << '\t' << e.lastUsed << '\n';
    }
    f.close();
    fs::rename(path_ + ".tmp", path_, ec);
    return !ec;
}

const PlaylistEntry* PlaylistStore::find(const std::string& url) const {
    for (const PlaylistEntry& e : list_)
        if (e.url == url) return &e;
    return nullptr;
}

void PlaylistStore::use(const PlaylistEntry& e0) {
    PlaylistEntry e = e0;
    e.lastUsed = now();
    for (auto it = list_.begin(); it != list_.end(); ++it)
        if (it->url == e.url) {
            if (e.title.empty()) e.title = it->title;
            if (e.count == 0) e.count = it->count;
            list_.erase(it);
            break;
        }
    list_.insert(list_.begin(), e);
}

void PlaylistStore::remove(const std::string& url) {
    list_.erase(std::remove_if(list_.begin(), list_.end(), [&](const PlaylistEntry& e) { return e.url == url; }), list_.end());
}

void PlaylistStore::demo() {
    path_.clear();
    list_ = {{"https://www.youtube.com/playlist?list=PLdemoNightDrive01", "Night drive", 24, now() - 3600},
             {"https://www.youtube.com/playlist?list=PLdemoSundayChill02", "Sunday chill", 41, now() - 86400}};
}

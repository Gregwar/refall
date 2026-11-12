#pragma once
// The playlists the player used in free play, remembered in the user data folder (playlists.tsv, next to the
// campaign progress), most recent first; and the reading of what the player types or pastes as a playlist.
#include <cstdint>
#include <string>
#include <vector>

struct PlaylistEntry {
    std::string url, title;
    int count = 0;         // songs, when known
    int64_t lastUsed = 0;  // seconds since the epoch
};

class PlaylistStore {
public:
    static std::string defaultPath(); // <user data>/zentris/playlists.tsv
    void load(const std::string& path = "");
    bool save() const;                // no-op without a path (demo lists)
    const std::vector<PlaylistEntry>& list() const { return list_; } // most recent first
    // Adds or refreshes one (now its last use), keeping the title and count when the new ones are unknown.
    void use(const PlaylistEntry& e);
    void remove(const std::string& url);
    const PlaylistEntry* find(const std::string& url) const;
    // Made-up entries for clips and shots (in memory only).
    void demo();

private:
    std::vector<PlaylistEntry> list_;
    std::string path_;
};

// What the player typed: a playlist URL (any URL with list=ID), a bare playlist ID (PL..., OL..., UU..., RD...), or a
// single video URL (watch?v=, youtu.be/, shorts/). Returns the canonical URL, or "" with *err saying why not.
std::string normalizePlaylistInput(const std::string& in, std::string* err);
// The playlist ID of a canonical playlist URL ("" for a video).
std::string playlistIdOf(const std::string& url);
// The title of a YouTube playlist (or video), through yt-dlp; "" when it can't tell (offline, no yt-dlp).
std::string youtubeTitle(const std::string& url);

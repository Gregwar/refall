#include "menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>

#include "library.hpp"
#include "stb_image_write.h"
#include "ugl.hpp"
#include "youtube.hpp"

namespace {

// The squares' colours, an 8-bit palette: each universe takes the next one in the order it first appears in the
// campaign, so the songs of a stage (up to 12) never share one unless they share their universe.
const unsigned char PALETTE[12][3] = {{255, 0, 77},   {255, 163, 0},  {255, 236, 39}, {0, 228, 54},
                                      {41, 173, 255}, {255, 119, 168}, {131, 118, 230}, {0, 200, 180},
                                      {255, 204, 170}, {190, 90, 60},  {170, 60, 220},  {160, 220, 60}};
} // namespace

void menuUniverseColor(const Campaign* camp, const std::string& spec, unsigned char out[3]) {
    auto base = [](const std::string& s) {
        std::string n = s.substr(0, s.find(':'));
        for (char& c : n) c = (char)std::toupper((unsigned char)c);
        return n;
    };
    const std::string name = base(spec);
    if (name.empty() || !camp) { out[0] = out[1] = out[2] = 150; return; }
    std::vector<std::string> seen;
    for (const CampaignStage& st : camp->stages())
        for (const CampaignSong& s : st.songs) {
            const std::string n = base(s.universe);
            if (n.empty() || std::find(seen.begin(), seen.end(), n) != seen.end()) continue;
            if (n == name) {
                std::memcpy(out, PALETTE[seen.size() % 12], 3);
                return;
            }
            seen.push_back(n);
        }
    std::memcpy(out, PALETTE[0], 3);
}

namespace {

} // namespace

Menu::Menu() = default;

Menu::~Menu() {
    dropBackdrop();
}

void Menu::init(const Campaign* c, Progress* p) {
    camp_ = c;
    progress_ = p;
}

void Menu::open(Screen s, const std::string& message, const std::string& sub, bool bad) {
    screen_ = s;
    msg_ = message;
    msgSub_ = sub;
    msgBad_ = bad;
    msgT_ = message.empty() ? 0.f : 5.f;
    if (s == FREE) { freeRow_ = 0; editing_ = false; plMsg_.clear(); }
    if (s == OPTIONS && message.empty()) { optSel_ = 0; aOpt_ = 0; optConfirm_ = false; }
    if (s == GAMEPAD && message.empty()) { padSel_ = 0; aPad_ = 0; }
    if (s == VOYAGES) {
        editing_ = false;
        vStep_ = 0;
        plMsg_.clear();
        vDelete_.clear();
        vProg_.clear();
        if (voyages_) {
            vProg_.push_back(voyages_->progress(""));
            for (const CustomVoyage& v : voyages_->list()) vProg_.push_back(voyages_->progress(v.id));
            vSel_ = 0; // the voyage open (the main one first)
            for (int i = 0; i < (int)voyages_->list().size(); i++)
                if (voyages_->list()[i].id == voyageId_) vSel_ = i + 1;
        }
        aV_ = (float)vSel_;
        aVRow_ = (float)(vSel_ / voyageCols());
    }
}

bool Menu::selectVoyage(const std::string& id) {
    if (openVoyage_ && !openVoyage_(id)) return false;
    if (id == voyageId_) return true; // (its focus stays)
    voyageStage_[voyageId_] = stage_;
    voyageId_ = id;
    const CustomVoyage* v = voyages_ && !id.empty() ? voyages_->find(id) : nullptr;
    voyageTitle_ = v ? v->title : "";
    const int n = camp_ ? camp_->size() : 0;
    auto it = voyageStage_.find(id);
    if (it != voyageStage_.end()) stage_ = it->second;
    else { // the next stage to play: the first open one not cleared, else the last open one
        stage_ = -1;
        int last = 0;
        for (int s = 0; s < n; s++) {
            const bool o = !progress_ || progress_->unlocked(s, diff_);
            if (o) last = s;
            if (o && stage_ < 0 && !(progress_ && progress_->cleared(s, diff_))) stage_ = s;
        }
        if (stage_ < 0) stage_ = last;
    }
    stage_ = std::clamp(stage_, 0, std::max(0, n - 1));
    song_ = 0;
    row_ = 1;
    petal_ = false;
    celebrate_ = -1;
    unlock_ = -1;
    unlocks_.clear(); // (they were the other voyage's)
    aStage_ = (float)stage_;
    aOpen_ = 0;
    return true;
}

void Menu::celebrateUnlock(int stage, int diff) {
    for (const Unlock& x : unlocks_)
        if (x.stage == stage && x.diff == diff) return;
    unlocks_.push_back({stage, diff});
}

bool Menu::unlockToPlay(int stage, int diff) const {
    if (unlock_ == stage && unlockDiff_ == diff && unlockT_ < UNLOCK_OPEN) return true;
    for (const Unlock& x : unlocks_)
        if (x.stage == stage && x.diff == diff) return true;
    return false;
}

void Menu::enterVoyage(std::string id) {
    if (!selectVoyage(id)) {
        plMsg_ = "Couldn't open this voyage";
        plMsgBad_ = true;
        return;
    }
    open(CAMPAIGN);
}

std::vector<PlaylistEntry> Menu::plChoices() const {
    // the game's own playlist first (featured), the built-in electro one, then the player's, the most recent first
    std::vector<PlaylistEntry> v;
    PlaylistEntry featured{DEFAULT_PLAYLIST, GAME_TITLE, 0, 0}, electro{ELECTRO_PLAYLIST, ELECTRO_PLAYLIST_TITLE, 0, 0};
    if (playlists_)
        for (const PlaylistEntry& e : playlists_->list()) {
            if (e.url == DEFAULT_PLAYLIST) featured.count = e.count;
            else if (e.url == ELECTRO_PLAYLIST) electro.count = e.count;
            else v.push_back(e);
        }
    v.insert(v.begin(), {featured, electro});
    return v;
}

const CampaignSong* Menu::hovered() const {
    if (screen_ != CAMPAIGN || row_ != 1 || !camp_ || stage_ >= camp_->size()) return nullptr;
    if (!petal_) return nullptr; // a closed petal: no song in focus
    const auto& songs = camp_->stages()[stage_].songs;
    return song_ < (int)songs.size() ? &songs[song_] : nullptr;
}

Menu::Place Menu::place() const {
    Place p;
    p.titleItem = titleItem_;
    p.voyageDiff = diff_;
    p.stage = stage_;
    p.freeDiff = freeDiff_;
    p.freeLevel = freeLevel();
    const auto ch = plChoices();
    if (plSel_ < (int)ch.size()) p.playlist = ch[plSel_].url;
    p.voyage = voyageId_;
    p.music = music_ ? 1 : 0;
    p.theme = theme_;
    return p;
}

void Menu::restore(const Place& p, bool difficulties) {
    // (the title opens on what to play, never on OPTIONS or QUIT: the game was just started)
    if (p.titleItem == TI_VOYAGE || (p.titleItem == TI_FREE && TI_FREE >= 0)) titleItem_ = p.titleItem;
    if (difficulties && p.voyageDiff >= 0 && p.voyageDiff < DIFF_COUNT) diff_ = p.voyageDiff;
    if (difficulties && p.freeDiff >= 0 && p.freeDiff < DIFF_COUNT) freeDiff_ = p.freeDiff;
    if (p.freeLevel >= 1) setFreeLevel(p.freeLevel);
    if (p.music >= 0) music_ = p.music != 0;
    if (p.theme >= 0) theme_ = p.theme == 1 ? 1 : 0;
    if (p.stage >= 0 && camp_ && camp_->size() > 0) {
        int last = 0; // the last unlocked stage: the saved one if it still is, else that one
        for (int s = 0; s < camp_->size(); s++)
            if (!progress_ || progress_->unlocked(s, diff_)) last = s;
        stage_ = std::min(p.stage, last);
        song_ = 0;
        row_ = 1;
        petal_ = false;
    }
    if (!p.playlist.empty()) {
        const auto ch = plChoices();
        for (int i = 0; i < (int)ch.size(); i++)
            if (ch[i].url == p.playlist) plSel_ = i;
    }
    snap(); // the rosette already turned to the stage
}

// One "key<TAB>value" per line.
bool Menu::loadPlace(const std::string& path, Place& p) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        const size_t tab = l.find('\t');
        if (tab == std::string::npos) continue;
        const std::string k = l.substr(0, tab), v = l.substr(tab + 1);
        if (k == "title") p.titleItem = std::atoi(v.c_str());
        else if (k == "voyage-difficulty") p.voyageDiff = std::atoi(v.c_str());
        else if (k == "voyage-stage") p.stage = std::atoi(v.c_str()) - 1;
        else if (k == "free-difficulty") p.freeDiff = std::atoi(v.c_str());
        else if (k == "free-level") p.freeLevel = std::atoi(v.c_str());
        else if (k == "free-playlist") p.playlist = v;
        else if (k == "voyage") p.voyage = v;
        else if (k == "menu-music") p.music = std::atoi(v.c_str());
        else if (k == "menu-theme") p.theme = std::atoi(v.c_str());
    }
    std::fclose(f);
    return true;
}

bool Menu::savePlace(const std::string& path, const Place& p) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "# Refall: where the menu was (--reset forgets it)\ntitle\t%d\nvoyage-difficulty\t%d\nvoyage-stage\t%d\n"
                    "free-difficulty\t%d\nfree-level\t%d\nfree-playlist\t%s\nvoyage\t%s\nmenu-music\t%d\nmenu-theme\t%d\n",
                 p.titleItem, p.voyageDiff, p.stage + 1, p.freeDiff, p.freeLevel, p.playlist.c_str(), p.voyage.c_str(), p.music < 0 ? 1 : p.music, p.theme < 0 ? 0 : p.theme);
    std::fclose(f);
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

void Menu::setFreeLevel(int level, bool focus) {
    for (int i = 0; i < FREE_LEVEL_COUNT; i++)
        if (std::abs(FREE_LEVELS[i] - level) < std::abs(FREE_LEVELS[lvlSel_] - level)) lvlSel_ = i;
    if (focus) freeRow_ = 2;
}

void Menu::resetPlace() {
    titleItem_ = TI_VOYAGE;
    freeDiff_ = DIFF_HARD;
    diff_ = DIFF_NORMAL;
    stage_ = song_ = 0;
    row_ = 1;
    petal_ = false;
    freeRow_ = lvlSel_ = 0;
    plSel_ = 1; // (the electro playlist, free play's default)
    voyageStage_.clear();
    snap();
}

void Menu::snap() {
    aTitle_ = (float)titleItem_;
    aFree_ = (float)freeDiff_;
    aDiff_ = (float)diff_;
    aStage_ = (float)stage_;
    aSong_ = (float)song_;
    aRow_ = (float)row_;
    aOpen_ = petal_ ? 1.f : 0.f;
    aFreeRow_ = (float)freeRow_;
    aPl_ = (float)plSel_;
    aLvl_ = (float)lvlSel_;
}

Menu::Choice Menu::update(const Input& in, float dt) {
    time_ += dt;
    {
        const float k = 1.f - std::exp(-dt * 11.f);
        aTitle_ += (titleItem_ - aTitle_) * k;
        aOpt_ += (optSel_ - aOpt_) * k;
        aPad_ += (padSel_ - aPad_) * k;
        aYes_ += (optYes_ - aYes_) * k;
        noteT_ = std::max(0.f, noteT_ - dt);
        aFree_ += (freeDiff_ - aFree_) * k;
        aDiff_ += (diff_ - aDiff_) * k;
        aStage_ += (stage_ - aStage_) * k;
        aSong_ += (song_ - aSong_) * k;
        aRow_ += (row_ - aRow_) * k;
        aFreeRow_ += (freeRow_ - aFreeRow_) * k;
        aPl_ += (plSel_ - aPl_) * k;
        aLvl_ += (lvlSel_ - aLvl_) * k;
        aV_ += (vSel_ - aV_) * k;
        aVRow_ += ((float)(vSel_ / voyageCols()) - aVRow_) * k;
        aOpen_ += ((petal_ ? 1.f : 0.f) - aOpen_) * (1.f - std::exp(-dt * 5.f)); // a petal opens a little slower
    }
    enterT_ += dt;
    if (Choice r; updateDebrief(in, dt, r)) return r;
    if (celebrate_ >= 0 && screen_ == CAMPAIGN) {
        celebrateT_ += dt;
        if (stage_ != celebrate_ || row_ != 1 || petal_) { // the player moved: it ends (cleared), and the unlocks it led to
            celebrate_ = -1;
            unlocks_.erase(std::remove_if(unlocks_.begin(), unlocks_.end(), [&](const Unlock& x) { return x.diff == diff_; }), unlocks_.end());
        } else if (celebrateT_ >= CELEBRATE_MOVE) {
            if (celebrateNext_ >= 0) stage_ = celebrateNext_;
            celebrate_ = -1;
        }
    }
    if (screen_ == CAMPAIGN && unlock_ >= 0) {
        unlockT_ += dt;
        // (the player moved: it ends, the petal open)
        if (stage_ != unlock_ || row_ != unlockRow_ || petal_ || diff_ != unlockDiff_ || unlockT_ >= UNLOCK_END) unlock_ = -1;
    }
    if (screen_ == CAMPAIGN && celebrate_ < 0 && unlock_ < 0) // the next unlock to play on this difficulty
        for (size_t i = 0; i < unlocks_.size(); i++) {
            if (unlocks_[i].diff != diff_) continue;
            unlock_ = unlocks_[i].stage;
            unlockDiff_ = diff_;
            unlocks_.erase(unlocks_.begin() + i);
            // (the wheel turns to it first, unless it stands there)
            unlockT_ = std::fabs(std::remainder(aStage_ - (float)unlock_, (float)std::max(1, camp_ ? camp_->size() : 1))) > 0.2f ? -UNLOCK_LEAD : 0.f;
            stage_ = unlock_;
            song_ = 0;
            petal_ = false;
            unlockRow_ = row_;
            break;
        }
    // a playlist being read: its result
    if (fetch_.valid() && fetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const Fetched r = fetch_.get();
        if (r.ok && playlists_) {
            playlists_->use({r.url, r.title, r.count, 0});
            playlists_->save();
            const auto ch = plChoices();
            for (int i = 0; i < (int)ch.size(); i++)
                if (ch[i].url == r.url) plSel_ = i;
            editing_ = false;
            char b[160];
            std::snprintf(b, sizeof(b), "%s: %d SONGS", asciiFold(r.url == DEFAULT_PLAYLIST ? std::string(GAME_TITLE) + " playlist" : r.title).c_str(), r.count);
            plMsg_ = b;
            plMsgBad_ = false;
        } else {
            plMsg_ = r.error;
            plMsgBad_ = true;
        }
    }
    // a custom voyage's playlist being read: on to its name
    if (vFetch_.valid() && vFetch_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const VFetched r = vFetch_.get();
        if (r.ok) {
            vUrl_ = r.url;
            vEntries_ = r.entries;
            vStep_ = 2;
            editText_ = asciiFold(r.title);
            if (editText_.size() > 60) editText_.resize(60);
            char b[160];
            const int n = std::min((int)r.entries.size(), VoyageStore::MAX_SONGS), st = (n + VoyageStore::STAGE_SONGS - 1) / VoyageStore::STAGE_SONGS;
            if ((int)r.entries.size() > n) std::snprintf(b, sizeof(b), "%d OF ITS %d SONGS   /   %d STAGES", n, (int)r.entries.size(), st);
            else std::snprintf(b, sizeof(b), "%d SONGS   /   %d STAGE%s", n, st, st > 1 ? "S" : "");
            plMsg_ = b;
            plMsgBad_ = false;
        } else {
            plMsg_ = r.error;
            plMsgBad_ = true;
        }
    }
    Choice ch;
    auto steps = [&](Action a) { return in.repeatSteps(a); };
    const int dy = steps(A_UI_DOWN) - steps(A_UI_UP), dx = steps(A_UI_RIGHT) - steps(A_UI_LEFT);
    const bool ok = in.pressed(A_UI_OK), back = in.pressed(A_UI_BACK);
    if (dx || dy || ok || back || in.pressed(A_UI_ALT) || in.pressed(A_UI_ENTER) || !in.typed().empty()) lastInput_ = time_;
    if (introT_ >= 0.f) { // the opening: no menu yet (a key hurries it)
        introT_ += dt;
        if (ok || back || dx || dy) introT_ = std::max(introT_, INTRO_END - 0.6f);
        if (introT_ >= INTRO_END) introT_ = -1.f;
        else return ch;
    }
    if (msgT_ > 0) {
        msgT_ = std::max(0.f, msgT_ - dt);
        if (msgT_ < 4.2f && (ok || back || dx || dy)) { msgT_ = 0; return ch; } // a key dismisses the message
        if (msgT_ > 0) return ch;
    }
    switch (screen_) {
    case TITLE:
        titleItem_ = (titleItem_ + dy + TITLE_ITEMS * 8) % TITLE_ITEMS;
        if (ok) {
            if (titleItem_ == TI_VOYAGE) open(voyages_ ? VOYAGES : CAMPAIGN);
            else if (titleItem_ == TI_FREE) open(FREE);
            else if (titleItem_ == TI_OPTIONS) open(OPTIONS);
            else ch.kind = Choice::QUIT;
        }
        if (back && TI_QUIT >= 0) titleItem_ = TI_QUIT; // (never quits by itself: it goes to QUIT, to be confirmed)
        break;
    case OPTIONS:
        if (noteT_ > 0.f) { if (ok || back || dx || dy) noteT_ = 0.f; break; } // (a key: back to the rows)
        if (optConfirm_) { // are you sure? YES / NO
            if (dx || dy) optYes_ = 1 - optYes_;
            if (ok && optYes_) ch.kind = optSel_ == 0 ? Choice::UNLOCK_ALL : Choice::RESET_ALL;
            if (ok || back) optConfirm_ = false;
            break;
        }
        optSel_ = (optSel_ + dy + OPTION_ITEMS * 8) % OPTION_ITEMS;
        if (ok && optSel_ == 2) open(GAMEPAD);
        else if (ok && optSel_ == 3) music_ = !music_; // (at once, no question)
        else if (ok && optSel_ == 4) theme_ = 1 - theme_;
        else if (ok) { optConfirm_ = true; optYes_ = 0; aYes_ = 0; }
        if (back) open(TITLE);
        break;
    case GAMEPAD:
        if (input_ && input_->remapping()) break; // (its keys are the app's: Esc cancels, Space skips)
        if (noteT_ > 0.f) { if (ok || back || dx || dy) noteT_ = 0.f; break; }
        padSel_ = (padSel_ + dy + PAD_ITEMS * 8) % PAD_ITEMS;
        if (ok) ch.kind = padSel_ == 0 ? Choice::PAD_REMAP : Choice::PAD_DEFAULT;
        if (back) { open(OPTIONS); optSel_ = 2; aOpt_ = 2; }
        break;
    case FREE: {
        if (!playlists_) { // no playlist store: a difficulty only
            freeDiff_ = std::clamp(freeDiff_ + dy + dx, 0, DIFF_COUNT - 1);
            if (ok) { ch.kind = Choice::PLAY_FREE; ch.difficulty = freeDiff_; ch.level = freeLevel(); }
            if (back) open(TITLE);
            break;
        }
        const auto choices = plChoices();
        const int nCh = (int)choices.size() + 1; // the last: add a playlist
        if (editing_) { // typing or pasting a playlist link
            if (!fetch_.valid()) {
                editText_ += in.typed();
                if (in.pressed(A_UI_PASTE))
                    for (char c : in.clipboard())
                        if (c >= 32 && c < 127) editText_ += c;
                for (int i = 0; i < in.repeatSteps(A_UI_ERASE) && !editText_.empty(); i++) editText_.pop_back();
                if (editText_.size() > 300) editText_.resize(300);
                if (in.pressed(A_UI_ENTER)) {
                    std::string err;
                    const std::string url = normalizePlaylistInput(editText_, &err);
                    if (url.empty()) { plMsg_ = err; plMsgBad_ = true; }
                    else {
                        plMsg_ = "READING THE PLAYLIST...";
                        plMsgBad_ = false;
                        fetch_ = std::async(std::launch::async, [url]() {
                            Fetched f;
                            f.url = url;
                            const std::vector<std::string> entries = resolveYoutube(url);
                            if (entries.empty()) { f.error = "Couldn't read it: offline, private, or empty?"; return f; }
                            f.count = (int)entries.size();
                            f.title = url == DEFAULT_PLAYLIST ? std::string(GAME_TITLE) + " playlist" : youtubeTitle(url);
                            if (f.title.empty()) f.title = playlistIdOf(url).empty() ? "YouTube video" : "Playlist " + playlistIdOf(url);
                            f.ok = true;
                            return f;
                        });
                    }
                }
            }
            if (in.pressed(A_UI_BACK) && !in.down(A_UI_ERASE) && !fetch_.valid()) { editing_ = false; plMsg_.clear(); }
            break;
        }
        if (freeRow_ == 0) {
            freeDiff_ = std::clamp(freeDiff_ + dx, 0, DIFF_COUNT - 1);
            if (dy > 0) freeRow_ = 1;
        } else if (freeRow_ == 2) { // the starting level
            lvlSel_ = std::clamp(lvlSel_ + dx, 0, FREE_LEVEL_COUNT - 1);
            if (dy < 0) freeRow_ = 1;
        } else {
            plSel_ = std::clamp(plSel_ + dx, 0, nCh - 1);
            if (dy < 0) freeRow_ = 0;
            if (dy > 0) freeRow_ = 2;
            if (in.pressed(A_UI_DELETE) && plSel_ < (int)choices.size() && choices[plSel_].url != DEFAULT_PLAYLIST &&
                choices[plSel_].url != ELECTRO_PLAYLIST) {
                playlists_->remove(choices[plSel_].url);
                playlists_->save();
                plMsg_ = "REMOVED " + asciiFold(choices[plSel_].title);
                plMsgBad_ = false;
                plSel_ = std::min(plSel_, (int)plChoices().size());
            }
        }
        plSel_ = std::clamp(plSel_, 0, nCh - 1);
        if (ok) {
            if (freeRow_ == 1 && plSel_ == nCh - 1) { editing_ = true; editText_.clear(); plMsg_.clear(); }
            else {
                const PlaylistEntry e = choices[std::min(plSel_, (int)choices.size() - 1)];
                ch.kind = Choice::PLAY_FREE;
                ch.difficulty = freeDiff_;
                ch.level = freeLevel();
                ch.playlist = e.url;
                playlists_->use(e);
                playlists_->save();
            }
        }
        if (back) { if (freeRow_ > 0) freeRow_ = 0; else open(TITLE); }
        break;
    }
    case CAMPAIGN: {
        const int n = camp_ ? camp_->size() : 0;
        if (n > 0) {
            // two levels: the stage petals (left / right), then the songs of the opened petal
            if (row_ == 0) {
                diff_ = std::clamp(diff_ + dx, 0, DIFF_COUNT - 1);
                if (dy > 0 || ok) row_ = 1;
                else if (back) open(voyages_ ? VOYAGES : TITLE);
            } else if (!petal_) {
                // round the wheel: past the last stage comes the first again, and back (the animated position
                // jumps a whole turn with it, so the wheel takes the short way round)
                if (dx) {
                    const int to = ((stage_ + dx) % n + n) % n;
                    if (to - stage_ != dx) { aStage_ += (float)(to - stage_ - dx); aStageTurns_ -= (float)(to - stage_ - dx); }
                    stage_ = to;
                }
                if (dy < 0) row_ = 0;
                const bool unlocked = progress_ && progress_->unlocked(stage_, diff_);
                if (ok && unlocked) { // the whole stage, from its first song: a real run
                    ch.kind = Choice::PLAY_CAMPAIGN;
                    ch.difficulty = diff_;
                    ch.stage = stage_;
                    ch.song = 0;
                    ch.stageRun = true;
                } else if (in.pressed(A_UI_ALT) && unlocked) {
                    petal_ = true; // its songs, to start from a later one (practice)
                    song_ = std::min(song_, (int)camp_->stages()[stage_].songs.size() - 1);
                } else if (back) open(voyages_ ? VOYAGES : TITLE);
            } else {
                song_ = std::clamp(song_ + dx + dy, 0, (int)camp_->stages()[stage_].songs.size() - 1);
                if (ok && hovered()) {
                    ch.kind = Choice::PLAY_CAMPAIGN;
                    ch.difficulty = diff_;
                    ch.stage = stage_;
                    ch.song = song_;
                    ch.stageRun = song_ == 0; // (a later song: practice, its game over offers RETRY SONG only)
                }
                if (back || in.pressed(A_UI_ALT)) petal_ = false;
            }
            break;
        }
        if (back) open(voyages_ ? VOYAGES : TITLE);
        break;
    }
    case VOYAGES: {
        if (!voyages_) { if (back) open(TITLE); break; }
        auto type = [&](size_t max) {
            editText_ += in.typed();
            if (in.pressed(A_UI_PASTE))
                for (char c : in.clipboard())
                    if (c >= 32 && c < 127) editText_ += c;
            for (int i = 0; i < in.repeatSteps(A_UI_ERASE) && !editText_.empty(); i++) editText_.pop_back();
            if (editText_.size() > max) editText_.resize(max);
        };
        const bool esc = in.pressed(A_UI_BACK) && !in.down(A_UI_ERASE);
        if (vStep_ == 1) { // the playlist's link
            if (!vFetch_.valid()) {
                type(300);
                if (in.pressed(A_UI_ENTER)) {
                    std::string err;
                    const std::string url = normalizePlaylistInput(editText_, &err);
                    if (url.empty()) { plMsg_ = err; plMsgBad_ = true; }
                    else {
                        vLink_ = editText_;
                        plMsg_ = "READING THE PLAYLIST...";
                        plMsgBad_ = false;
                        vFetch_ = std::async(std::launch::async, [url]() {
                            VFetched f;
                            f.url = url;
                            f.entries = resolveYoutube(url);
                            if (f.entries.empty()) { f.error = "Couldn't read it: offline, private, or empty?"; return f; }
                            f.title = url == DEFAULT_PLAYLIST ? std::string(GAME_TITLE) : youtubeTitle(url);
                            if (f.title.empty()) f.title = playlistIdOf(url).empty() ? "My voyage" : "Playlist " + playlistIdOf(url);
                            f.ok = true;
                            return f;
                        });
                    }
                }
                if (esc) { vStep_ = 0; editing_ = false; plMsg_.clear(); }
            }
            break;
        }
        if (vStep_ == 2) { // its name
            type(60);
            std::string name = editText_;
            while (!name.empty() && name.back() == ' ') name.pop_back();
            while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            if (in.pressed(A_UI_ENTER) && !name.empty()) {
                std::string err;
                const std::string id = voyages_->create(name, vUrl_, vEntries_, std::random_device{}() ^ ((uint64_t)std::random_device{}() << 32), &err);
                if (id.empty()) { plMsg_ = err; plMsgBad_ = true; }
                else {
                    vStep_ = 0;
                    editing_ = false;
                    plMsg_.clear();
                    vEntries_.clear();
                    enterVoyage(id);
                }
            } else if (esc) { vStep_ = 1; editText_ = vLink_; plMsg_.clear(); }
            break;
        }
        const auto& vs = voyages_->list();
        const int nCards = (int)vs.size() + 2; // the main voyage first, then the player's, then "add a playlist"
        const int was = vSel_;
        // a grid: left / right one card, up / down one row
        if (dx) vSel_ = std::clamp(vSel_ + dx, 0, nCards - 1);
        if (dy) {
            const int to = vSel_ + dy * voyageCols();
            if (to >= 0 && to < nCards) vSel_ = to;
            else if (dy > 0 && vSel_ / voyageCols() < (nCards - 1) / voyageCols()) vSel_ = nCards - 1; // a shorter last row
        }
        if (vSel_ != was) { vDelete_.clear(); plMsg_.clear(); }
        if (in.pressed(A_UI_DELETE) && vSel_ >= 1 && vSel_ <= (int)vs.size()) {
            const CustomVoyage v = vs[vSel_ - 1];
            if (vDelete_ != v.id) { // once more to delete it (its progress goes with it)
                vDelete_ = v.id;
                plMsg_ = "DEL AGAIN TO DELETE " + asciiFold(v.title);
                plMsgBad_ = true;
            } else {
                if (voyageId_ == v.id) selectVoyage("");
                voyageStage_.erase(v.id);
                voyages_->remove(v.id);
                if (vSel_ < (int)vProg_.size()) vProg_.erase(vProg_.begin() + vSel_);
                vDelete_.clear();
                plMsg_ = "DELETED " + asciiFold(v.title);
                plMsgBad_ = false;
                vSel_ = std::min(vSel_, (int)voyages_->list().size() + 1);
            }
        }
        if (ok) {
            if (vSel_ == nCards - 1) { vStep_ = 1; editing_ = true; editText_.clear(); plMsg_.clear(); }
            else enterVoyage(vSel_ == 0 ? std::string() : std::string(vs[vSel_ - 1].id)); // (a copy: opening it reorders the list)
        }
        if (back) open(TITLE);
        break;
    }
    case DEBRIEF: break; // (updateDebrief)
    }
    return ch;
}

// ---- drawing

namespace {
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
// The stages' own colours (the petals, the kaleidoscope's tint): 15 vivid hues 24 degrees apart on the perceptual
// (OKLCH) wheel (hue is what survives the kaleidoscope's grading), each halfway between its most saturated and a calmer
// lightness, the teal and the aqua set apart in lightness too; ordered so that neighbouring stages are at least 120
// degrees apart. Stage 12 keeps its electric indigo.
const unsigned char STAGE_COL[][3] = {
    {72, 164, 254}, {250, 150, 24}, {174, 90, 253}, {144, 222, 54},  {232, 84, 242}, {39, 226, 147}, {252, 80, 127},
    {0, 178, 160},  {250, 78, 185}, {22, 191, 244}, {204, 203, 20},  {92, 60, 255},  {240, 176, 16}, {120, 232, 245},
    {252, 114, 64}};
const int STAGE_COLS = (int)(sizeof(STAGE_COL) / sizeof(STAGE_COL[0]));

} // namespace

void Menu::stageColor(int stage, unsigned char out[3]) const {
    for (int i = 0; i < 3; i++) out[i] = STAGE_COL[((stage % STAGE_COLS) + STAGE_COLS) % STAGE_COLS][i];
}

Menu::SongView Menu::songView() const {
    SongView v;
    const CampaignSong* h = hovered();
    if (!h) return v;
    v.has = true;
    v.stage = stage_;
    v.song = song_;
    v.songs = (int)camp_->stages()[stage_].songs.size();
    v.locked = progress_ && !progress_->unlocked(stage_, diff_);
    v.beaten = progress_ && progress_->beaten(stage_, song_, diff_);
    v.best = progress_ ? progress_->best(stage_, song_, diff_) : 0;
    v.lines = progress_ ? progress_->lines(stage_, song_, diff_) : -1;
    v.title = asciiFold(h->title);
    v.universe = h->universe.empty() ? "A UNIVERSE PICKED BY THE SONG" : h->universe;
    v.velocity = h->level[diff_];
    menuUniverseColor(camp_, h->universe, v.col);
    return v;
}

void Menu::render(int w, int h) {
    fw_ = (float)w;
    fh_ = (float)h;
    u_ = std::min(w / 1920.f, h / 1080.f);
    sx_ = (w - 1920.f * u_) * 0.5f;
    sy_ = (h - 1080.f * u_) * 0.5f;
    portrait_ = h > w;
    pk_ = w / 1080.f;
    ph_ = h / pk_;
#ifdef REFALL_WEB
    pTop_ = 0.05f * ph_, pBottom_ = 0.78f * ph_; // (the page's buttons at the top, the touch pad at the bottom)
#else
    pTop_ = 0.02f * ph_, pBottom_ = 0.98f * ph_;
#endif
    if (screen_ == DEBRIEF) {
        drawDebrief(w, h);
        paint_.end();
        return;
    }
    renderWorlds(w, h);
    drawMessage();
    drawEnterFade();
    paint_.end();
}

// The message over a screen (GAME OVER, STAGE CLEAR): a frosted glass card.
void Menu::drawMessage() {
    if (msgT_ <= 0 || msg_.empty()) return;
    using F = Painter::Font;
    Painter& p = paint_;
    const float a = sstep(std::min(msgT_, 5.f - msgT_) * 3.f);
    if (portrait_) place(960, 520, 540, 0.5f * (pTop_ + pBottom_), 1.f); // (across the column)
    const float cx = X(960), cy = Y(520);
    const Col ink = hex(0xffffff), sub = hex(0xd8dcf0), edge = msgBad_ ? hex(0xff8a9a) : hex(0xe8c06a);
    const float tw = std::max(p.width(F::JOSEFIN_LIGHT, msg_, 110 * u_, 0.05f), p.width(F::JOSEFIN, msgSub_, 26 * u_, 0.1f));
    const float bw = tw + 140 * u_, bh = 250 * u_;
    p.setFade(a);
    p.glass(cx - bw / 2, cy - bh / 2, bw, bh, 24 * u_, u_, 0.f, 0.8f);
    p.setFade(1.f);
    p.rrectLine(cx - bw / 2, cy - bh / 2, bw, bh, 24 * u_, 2 * u_, withA(edge, edge.a * a));
    p.text(F::JOSEFIN_LIGHT, msg_, cx, cy + 20 * u_, 110 * u_, withA(ink, a), 0, 0.05f);
    p.text(F::JOSEFIN, msgSub_, cx, cy + 85 * u_, 26 * u_, withA(sub, sub.a * a), 0, 0.1f);
}

bool Menu::screenshot(const std::string& path, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 3), flipped(px.size());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++) std::memcpy(&flipped[(size_t)y * w * 3], &px[(size_t)(h - 1 - y) * w * 3], (size_t)w * 3);
    return stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3) != 0;
}

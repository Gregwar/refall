#include "menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "library.hpp"
#include "menuworlds.hpp"
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

// The preview snippet of a song: ~14 s from its first drop (else chorus, else 35% in), faded in and out and
// brought to an even loudness (RMS), its peaks kept under the clip guard.
std::shared_ptr<Track> cutSnippet(const Track& t) {
    const Analysis& an = t.analysis;
    double at = -1;
    for (int pass = 0; pass < 2 && at < 0; pass++)
        for (const Segment& sg : an.segments)
            if (sg.kind == (pass == 0 ? SEG_DROP : SEG_CHORUS) && sg.start > 8.0) { at = sg.start - 1.0; break; }
    if (at < 0) at = t.duration() * 0.35;
    const double len = 14.0;
    at = std::clamp(at, 0.0, std::max(0.0, t.duration() - len));
    const uint64_t a = (uint64_t)(at * t.sampleRate), n = std::min<uint64_t>((uint64_t)(len * t.sampleRate), t.frames() - a);
    auto s = std::make_shared<Track>();
    s->path = t.path;
    s->title = t.title;
    s->sampleRate = t.sampleRate;
    s->pcm.assign(t.pcm.begin() + a * 2, t.pcm.begin() + (a + n) * 2);
    double sum = 0;
    float peak = 1e-6f;
    for (float v : s->pcm) { sum += (double)v * v; peak = std::max(peak, std::fabs(v)); }
    const float rms = (float)std::sqrt(sum / std::max<size_t>(1, s->pcm.size()));
    const float gain = std::min({0.16f / std::max(1e-4f, rms), 0.8f / peak, 3.f});
    const uint64_t fin = t.sampleRate * 1, fout = t.sampleRate * 2;
    for (uint64_t i = 0; i < n; i++) {
        float g = gain;
        if (i < fin) g *= (float)i / fin;
        if (n - i < fout) g *= (float)(n - i) / fout;
        s->pcm[i * 2] *= g;
        s->pcm[i * 2 + 1] *= g;
    }
    return s;
}

} // namespace

Menu::Menu() = default;

Menu::~Menu() = default;

void Menu::init(const Campaign* c, Progress* p, AudioEngine* audio) {
    camp_ = c;
    progress_ = p;
    audio_ = audio;
}

void Menu::open(Screen s, const std::string& message, const std::string& sub, bool bad) {
    screen_ = s;
    msg_ = message;
    msgSub_ = sub;
    msgBad_ = bad;
    msgT_ = message.empty() ? 0.f : 5.f;
    hoverT_ = 0;
    playingId_.clear();
    if (s == FREE) { freeRow_ = 0; editing_ = false; plMsg_.clear(); }
}

std::vector<PlaylistEntry> Menu::plChoices() const {
    std::vector<PlaylistEntry> v;
    const std::string def = std::string(GAME_TITLE) + " playlist";
    bool hasDefault = false;
    if (playlists_)
        for (const PlaylistEntry& e : playlists_->list()) {
            v.push_back(e);
            if (e.url == DEFAULT_PLAYLIST) { v.back().title = def; hasDefault = true; }
        }
    if (!hasDefault) v.push_back({DEFAULT_PLAYLIST, def, 0, 0});
    return v;
}

void Menu::stopPreview() {
    if (audio_ && !playingId_.empty()) audio_->play(nullptr);
    playingId_.clear();
}

const CampaignSong* Menu::hovered() const {
    if (screen_ != CAMPAIGN || row_ != 1 || !camp_ || stage_ >= camp_->size()) return nullptr;
    if (!petal_) return nullptr; // a closed petal: no song in focus
    const auto& songs = camp_->stages()[stage_].songs;
    return song_ < (int)songs.size() ? &songs[song_] : nullptr;
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
}

Menu::Choice Menu::update(const Input& in, float dt) {
    time_ += dt;
    {
        const float k = 1.f - std::exp(-dt * 11.f);
        aTitle_ += (titleItem_ - aTitle_) * k;
        aFree_ += (freeDiff_ - aFree_) * k;
        aDiff_ += (diff_ - aDiff_) * k;
        aStage_ += (stage_ - aStage_) * k;
        aSong_ += (song_ - aSong_) * k;
        aRow_ += (row_ - aRow_) * k;
        aFreeRow_ += (freeRow_ - aFreeRow_) * k;
        aPl_ += (plSel_ - aPl_) * k;
        aOpen_ += ((petal_ ? 1.f : 0.f) - aOpen_) * (1.f - std::exp(-dt * 5.f)); // a petal opens a little slower
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
    Choice ch;
    auto steps = [&](Action a) { return in.repeatSteps(a); };
    const int dy = steps(A_UI_DOWN) - steps(A_UI_UP), dx = steps(A_UI_RIGHT) - steps(A_UI_LEFT);
    const bool ok = in.pressed(A_UI_OK), back = in.pressed(A_UI_BACK);
    if (msgT_ > 0) {
        msgT_ = std::max(0.f, msgT_ - dt);
        if (msgT_ < 4.2f && (ok || back || dx || dy)) { msgT_ = 0; return ch; } // a key dismisses the message
        if (msgT_ > 0) { updatePreview(dt); return ch; }
    }
    switch (screen_) {
    case TITLE:
        titleItem_ = (titleItem_ + dy + 3 * 8) % 3;
        if (ok) {
            if (titleItem_ == 0) open(CAMPAIGN);
            else if (titleItem_ == 1) open(FREE);
            else ch.kind = Choice::QUIT;
        }
        if (back) ch.kind = Choice::QUIT;
        break;
    case FREE: {
        if (!playlists_) { // no playlist store: a difficulty only
            freeDiff_ = std::clamp(freeDiff_ + dy + dx, 0, DIFF_COUNT - 1);
            if (ok) { ch.kind = Choice::PLAY_FREE; ch.difficulty = freeDiff_; }
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
        } else {
            plSel_ = std::clamp(plSel_ + dx, 0, nCh - 1);
            if (dy < 0) freeRow_ = 0;
            if (in.pressed(A_UI_DELETE) && plSel_ < (int)choices.size() && choices[plSel_].url != DEFAULT_PLAYLIST) {
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
                ch.playlist = e.url;
                playlists_->use(e);
                playlists_->save();
            }
        }
        if (back) { if (freeRow_ == 1) freeRow_ = 0; else open(TITLE); }
        break;
    }
    case CAMPAIGN: {
        const int n = camp_ ? camp_->size() : 0;
        if (n > 0) {
            // two levels: the stage petals (left / right), then the songs of the opened petal
            const int oldStage = stage_, oldSong = song_, oldRow = row_;
            if (row_ == 0) {
                diff_ = std::clamp(diff_ + dx, 0, DIFF_COUNT - 1);
                if (dy > 0 || ok) row_ = 1;
                else if (back) open(TITLE);
            } else if (!petal_) {
                stage_ = std::clamp(stage_ + dx, 0, n - 1);
                if (dy < 0) row_ = 0;
                if (ok && progress_ && progress_->unlocked(stage_)) { petal_ = true; song_ = std::min(song_, (int)camp_->stages()[stage_].songs.size() - 1); }
                else if (back) open(TITLE);
            } else {
                song_ = std::clamp(song_ + dx + dy, 0, (int)camp_->stages()[stage_].songs.size() - 1);
                if (ok && hovered()) {
                    ch.kind = Choice::PLAY_CAMPAIGN;
                    ch.difficulty = diff_;
                    ch.stage = stage_;
                    ch.song = song_;
                }
                if (back) petal_ = false;
            }
            if (stage_ != oldStage || song_ != oldSong || row_ != oldRow) hoverT_ = 0;
            break;
        }
        if (back) open(TITLE);
        break;
    }
    }
    if (ch.kind != Choice::NONE) {
        worlds_.reset(); // the live universes free their GPU memory for the game (snapshots stay on disk)
        // the game takes over the sound: the song crossfades from the preview
        playingId_.clear();
        return ch;
    }
    updatePreview(dt);
    return ch;
}

void Menu::updatePreview(float dt) {
    if (!audio_) return;
    level_ *= std::exp(-dt * 1.5f); // (kept up while a preview plays)
    hoverT_ += dt;
    // a finished load goes into the cache
    if (loading_.valid() && loading_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::shared_ptr<Track> t = loading_.get();
        Snip& s = snips_[loadingId_];
        s.track = t;
        s.failed = !t;
        snipOrder_.push_back(loadingId_);
        while (snipOrder_.size() > 16) { snips_.erase(snipOrder_.front()); snipOrder_.erase(snipOrder_.begin()); }
        loadingId_.clear();
    }
    const CampaignSong* h = hovered();
    const bool locked = h && progress_ && !progress_->unlocked(stage_);
    const std::string want = h && !locked && hoverT_ > 0.3f ? h->id : std::string();
    if (want.empty()) {
        if (!playingId_.empty() && (!h || h->id != playingId_)) stopPreview();
        return;
    }
    auto it = snips_.find(want);
    if (it == snips_.end()) {
        if (!loading_.valid()) {
            loadingId_ = want;
            const std::string entry = h->entry();
            const uint32_t sr = audio_->sampleRate();
            loading_ = std::async(std::launch::async, [entry, sr]() -> std::shared_ptr<Track> {
                std::shared_ptr<Track> full = loadTrack(entry, sr);
                return full ? cutSnippet(*full) : nullptr;
            });
        }
        if (!playingId_.empty() && playingId_ != want) stopPreview();
        return;
    }
    if (!it->second.track) return;
    if (playingId_ == want) { // the preview's loudness, for the backgrounds
        const Track& tr = *it->second.track;
        const uint64_t at = (uint64_t)(audio_->position() * tr.sampleRate);
        double sum = 0;
        int n = 0;
        for (uint64_t i = at; i < std::min<uint64_t>(tr.frames(), at + 2048); i++, n++) sum += tr.pcm[i * 2] * tr.pcm[i * 2];
        const float rms = n ? (float)std::sqrt(sum / n) : 0.f;
        level_ += (std::min(1.f, rms * 4.f) - level_) * std::min(1.f, dt * 3.f);
    }
    if (playingId_ != want) {
        audio_->setMuffled(false, 0.3f);
        audio_->setSongGain(1.f, 0.3f);
        audio_->play(it->second.track);
        playingId_ = want;
        replayT_ = 0;
    } else if (audio_->finished()) {
        // loops, after a short breath
        replayT_ += dt;
        if (replayT_ > 0.8f) { audio_->play(it->second.track); replayT_ = 0; }
    }
}

std::string Menu::previewStatus() const {
    const CampaignSong* h = hovered();
    if (!h) return "";
    if (progress_ && !progress_->unlocked(stage_)) return "LOCKED: CLEAR STAGE " + std::to_string(stage_) + " FIRST";
    auto it = snips_.find(h->id);
    if (it != snips_.end()) return it->second.failed ? "PREVIEW UNAVAILABLE" : playingId_ == h->id ? "PREVIEW PLAYING" : "PREVIEW";
    if (!audio_) return isYoutubeCached(h->entry()) ? "PREVIEW" : "NOT DOWNLOADED YET";
    if (hoverT_ <= 0.3f) return "";
    return isYoutubeCached(h->entry()) || loadingId_ != h->id ? "PREVIEW LOADING..." : "DOWNLOADING THE SONG...";
}

// ---- drawing

namespace {
float sstep(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
// The stages' own colours (planets, map stops): a calm 12-step wheel.
const unsigned char STAGE_COL[12][3] = {{110, 200, 255}, {150, 130, 255}, {255, 120, 200}, {255, 170, 90},
                                        {120, 230, 170}, {90, 170, 255},  {220, 120, 255}, {255, 210, 110},
                                        {110, 220, 230}, {255, 130, 120}, {170, 230, 110}, {200, 200, 255}};

} // namespace

void Menu::stageColor(int stage, unsigned char out[3]) const {
    for (int i = 0; i < 3; i++) out[i] = STAGE_COL[((stage % 12) + 12) % 12][i];
}

Menu::SongView Menu::songView() const {
    SongView v;
    const CampaignSong* h = hovered();
    if (!h) return v;
    v.has = true;
    v.stage = stage_;
    v.song = song_;
    v.songs = (int)camp_->stages()[stage_].songs.size();
    v.locked = progress_ && !progress_->unlocked(stage_);
    v.beaten = progress_ && progress_->beaten(stage_, song_, diff_);
    v.best = progress_ ? progress_->best(stage_, song_, diff_) : 0;
    v.title = asciiFold(h->title);
    v.universe = h->universe.empty() ? "A UNIVERSE PICKED BY THE SONG" : h->universe;
    v.velocity = h->level[diff_];
    v.preview = previewStatus();
    menuUniverseColor(camp_, h->universe, v.col);
    return v;
}

void Menu::render(int w, int h) {
    fw_ = (float)w;
    fh_ = (float)h;
    u_ = std::min(w / 1920.f, h / 1080.f);
    sx_ = (w - 1920.f * u_) * 0.5f;
    sy_ = (h - 1080.f * u_) * 0.5f;
    renderWorlds(w, h); // the live universes render first, then the painter draws
    drawMessage();
    paint_.end();
}

// The message over a screen (GAME OVER, STAGE CLEAR): a dark glass card.
void Menu::drawMessage() {
    if (msgT_ <= 0 || msg_.empty()) return;
    using F = Painter::Font;
    Painter& p = paint_;
    const float a = sstep(std::min(msgT_, 5.f - msgT_) * 3.f);
    const float cx = X(960), cy = Y(520);
    const Col ink = hex(0xffffff), sub = hex(0xd8dcf0), card = hex(0x0c0c14, 0.9f), edge = msgBad_ ? hex(0xff8a9a) : hex(0xe8c06a);
    const float tw = std::max(p.width(F::JOSEFIN_LIGHT, msg_, 110 * u_, 0.05f), p.width(F::JOSEFIN, msgSub_, 26 * u_, 0.1f));
    const float bw = tw + 140 * u_, bh = 250 * u_;
    p.rrect(cx - bw / 2, cy - bh / 2 + 14 * u_, bw, bh, 26 * u_, hex(0x000000, 0.35f * a), 40 * u_);
    p.rrect(cx - bw / 2, cy - bh / 2, bw, bh, 24 * u_, withA(card, card.a * a));
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

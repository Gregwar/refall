#include "menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "library.hpp"
#include "pixelfont.hpp"
#include "stb_image_write.h"
#include "ugl.hpp"
#include "youtube.hpp"

namespace {

// The seven tetromino colours (I O T S Z J L), NES-bright.
const unsigned char PIECE[7][3] = {{0, 228, 238}, {255, 214, 0}, {178, 76, 255}, {40, 214, 80},
                                   {255, 56, 72},  {48, 108, 255}, {255, 140, 20}};

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

const char* VS = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
const char* FS = R"(#version 330 core
in vec2 vUv;
out vec4 o;
uniform sampler2D uTex;
uniform vec2 uCanvas; // canvas pixels
uniform float uTime;
// 8-BIT ARCADE: the canvas on a CRT: a slight curvature, scanlines, a phosphor glow and a vignette.
void main() {
    vec2 c = vUv - 0.5;
    c *= 1.0 + 0.035 * dot(c, c) * vec2(1.0, 1.4);
    vec2 uv = c + 0.5;
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) { o = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec3 col = texture(uTex, uv).rgb;
    vec2 px = 1.0 / uCanvas;
    vec3 glow = vec3(0.0);
    for (int i = -2; i <= 2; i++)
        for (int j = -2; j <= 2; j++) glow += texture(uTex, uv + vec2(i, j) * px * 1.3).rgb;
    glow /= 25.0;
    float scan = 0.78 + 0.22 * sin((uv.y * uCanvas.y) * 6.2831853 + 1.5708);
    col = col * scan + glow * 0.55;
    col *= 0.97 + 0.03 * sin(uTime * 1.3); // a calm breathing of the tube
    col *= 1.0 - 0.55 * pow(length(c * vec2(1.0, 1.15)) * 1.25, 3.0);
    o = vec4(col, 1.0);
}
)";

} // namespace

Menu::~Menu() {
    if (tex_) glDeleteTextures(1, &tex_);
    if (prog_) glDeleteProgram(prog_);
}

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
}

void Menu::stopPreview() {
    if (audio_ && !playingId_.empty()) audio_->play(nullptr);
    playingId_.clear();
}

const CampaignSong* Menu::hovered() const {
    if (screen_ != CAMPAIGN || row_ != 1 || !camp_ || stage_ >= camp_->size()) return nullptr;
    const auto& songs = camp_->stages()[stage_].songs;
    return song_ < (int)songs.size() ? &songs[song_] : nullptr;
}

const char* Menu::styleName(int s) {
    static const char* N[] = {"8-BIT ARCADE", "COSMIC", "SWISS", "WATERCOLOUR", "GLASS",
                              "GALAXY",       "ORRERY", "ATLAS", "SYNTHWAVE",   "PASTEL"};
    return N[std::clamp(s, 0, STYLE_COUNT - 1)];
}

int Menu::parseStyle(const std::string& s0) {
    std::string s;
    for (char c : s0) s += (char)std::tolower((unsigned char)c);
    if (!s.empty() && s.find_first_not_of("0123456789") == std::string::npos) {
        const int n = std::atoi(s.c_str());
        return n >= 1 && n <= STYLE_COUNT ? n - 1 : -1;
    }
    static const char* N[][2] = {{"arcade", "8-bit"}, {"cosmic", "space"}, {"swiss", "minimal"}, {"watercolour", "watercolor"},
                                 {"glass", "neon"}, {"galaxy", "galaxy3d"}, {"orrery", "solar"}, {"atlas", "constellation"},
                                 {"synthwave", "synth"}, {"pastel", "kawaii"}};
    for (int i = 0; i < STYLE_COUNT; i++)
        if (s == N[i][0] || s == N[i][1]) return i;
    return -1;
}

void Menu::snap() {
    aTitle_ = (float)titleItem_;
    aFree_ = (float)freeDiff_;
    aDiff_ = (float)diff_;
    aStage_ = (float)stage_;
    aSong_ = (float)song_;
    aRow_ = (float)row_;
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
    case FREE:
        freeDiff_ = std::clamp(freeDiff_ + dy + dx, 0, DIFF_COUNT - 1);
        if (ok) { ch.kind = Choice::PLAY_FREE; ch.difficulty = freeDiff_; }
        if (back) open(TITLE);
        break;
    case CAMPAIGN: {
        const int n = camp_ ? camp_->size() : 0;
        const int oldStage = stage_, oldSong = song_, oldRow = row_;
        if (row_ == 0) {
            diff_ = std::clamp(diff_ + dx, 0, DIFF_COUNT - 1);
            if (dy > 0 && n > 0) row_ = 1;
        } else if (n > 0) {
            stage_ += dy;
            if (stage_ < 0) { stage_ = 0; row_ = 0; }
            stage_ = std::min(stage_, n - 1);
            song_ = std::clamp(song_ + dx, 0, (int)camp_->stages()[stage_].songs.size() - 1);
        }
        if (stage_ != oldStage || song_ != oldSong || row_ != oldRow) hoverT_ = 0;
        if (ok && row_ == 0 && n > 0) row_ = 1;
        else if (ok && hovered() && progress_ && progress_->unlocked(stage_)) {
            ch.kind = Choice::PLAY_CAMPAIGN;
            ch.difficulty = diff_;
            ch.stage = stage_;
            ch.song = song_;
        }
        if (back) open(TITLE);
        break;
    }
    }
    if (ch.kind != Choice::NONE) {
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

void Menu::clear(Rgb c) {
    for (size_t i = 0; i + 2 < px_.size(); i += 3) { px_[i] = c.r; px_[i + 1] = c.g; px_[i + 2] = c.b; }
}
void Menu::set(int x, int y, Rgb c) {
    if (x < 0 || y < 0 || x >= cw_ || y >= ch_) return;
    unsigned char* p = &px_[((size_t)y * cw_ + x) * 3];
    p[0] = c.r; p[1] = c.g; p[2] = c.b;
}
void Menu::rect(int x, int y, int w, int h, Rgb c) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) set(x + i, y + j, c);
}
void Menu::frame(int x, int y, int w, int h, Rgb c) {
    rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c);
    rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c);
}
// The font is ASCII: song titles are folded (asciiFold also collapses spaces, so plain text is left as it is).
static std::string fold(const std::string& s) {
    for (unsigned char ch : s)
        if (ch >= 0x80) return asciiFold(s);
    return s;
}

int Menu::text(const std::string& s0, int x, int y, Rgb c, bool big, int scale, bool shadow) {
    const std::string s = fold(s0);
    if (shadow) pixelfont::draw(big, s, x + scale, y + scale, scale, [&](int X, int Y) { set(X, Y, Rgb{8, 6, 20}); });
    pixelfont::draw(big, s, x, y, scale, [&](int X, int Y) { set(X, Y, c); });
    return pixelfont::width(big, s, scale);
}
int Menu::textC(const std::string& s, int cx, int y, Rgb c, bool big, int scale, bool shadow) {
    return text(s, cx - pixelfont::width(big, fold(s), scale) / 2, y, c, big, scale, shadow);
}

void Menu::drawBackground() {
    clear(Rgb{10, 8, 26});
    // drifting stars
    for (int i = 0; i < 70; i++) {
        uint32_t h = (uint32_t)i * 2654435761u;
        const float speed = 2.f + (h >> 28);
        const int x = (int)std::fmod((h % 997) * 0.7f + time_ * speed * 0.6f, (float)cw_);
        const int y = (int)((h >> 10) % (uint32_t)ch_);
        const bool twinkle = ((int)(time_ * 2.f + i) % 7) == 0;
        const unsigned char v = (unsigned char)(70 + (h >> 24) % 90 + (twinkle ? 60 : 0));
        set(x, y, Rgb{v, v, (unsigned char)std::min(255, v + 30)});
    }
    // a few dim tetrominoes falling slowly (behind the title only: the other screens are busy enough)
    if (screen_ != TITLE) return;
    static const int SH[7][4][2] = {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}, {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
                                    {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
                                    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
                                    {{2, 0}, {0, 1}, {1, 1}, {2, 1}}};
    const int cell = 6;
    for (int i = 0; i < 9; i++) {
        const int type = (i * 3 + 1) % 7;
        const float speed = 7.f + (i * 37 % 9);
        const float span = ch_ + 40.f;
        const float yy = std::fmod(time_ * speed + i * 53.f, span) - 30.f;
        const int x = (i * 97 + 13) % std::max(1, cw_ - 30);
        const int y = (int)yy / 3 * 3; // steps down a few pixels at a time, like gravity
        Rgb c{(unsigned char)(PIECE[type][0] / 4 + 6), (unsigned char)(PIECE[type][1] / 4 + 6), (unsigned char)(PIECE[type][2] / 4 + 14)};
        Rgb hi{(unsigned char)(PIECE[type][0] / 3 + 10), (unsigned char)(PIECE[type][1] / 3 + 10), (unsigned char)(PIECE[type][2] / 3 + 18)};
        for (auto& q : SH[type]) {
            rect(x + q[0] * cell, y + q[1] * cell, cell - 1, cell - 1, c);
            rect(x + q[0] * cell, y + q[1] * cell, cell - 1, 1, hi);
        }
    }
}

void Menu::drawTitle() {
    const int cx = ox_ + 200;
    // TETRIS, one tetromino colour per letter
    const char* word = "TETRIS";
    const int sc = 5;
    int w = pixelfont::width(true, word, sc), x = cx - w / 2;
    for (int i = 0; word[i]; i++) {
        const std::string l(1, word[i]);
        const unsigned char* c = PIECE[(i * 2 + 4) % 7];
        x += text(l, x, oy_ + 26, Rgb{c[0], c[1], c[2]}, true, sc) + sc;
    }
    textC("U N I V E R S E", cx, oy_ + 72, Rgb{235, 232, 255}, true, 2);
    const char* items[3] = {"CAMPAIGN", "FREE PLAY", "QUIT"};
    for (int i = 0; i < 3; i++) {
        const bool sel = i == titleItem_;
        const int y = oy_ + 112 + i * 22;
        Rgb c = sel ? Rgb{255, 214, 0} : Rgb{120, 116, 160};
        const int tw = textC(items[i], cx, y, c, true, 2);
        if (sel && std::fmod(time_, 0.8f) < 0.55f) {
            text(">", cx - tw / 2 - 16, y, c, true, 2);
            text("<", cx + tw / 2 + 6, y, c, true, 2);
        }
    }
    if (progress_ && camp_) {
        char b[64];
        std::snprintf(b, sizeof(b), "%d / %d STAGES CLEARED", progress_->clearedCount(), camp_->size());
        textC(b, cx, oy_ + 186, Rgb{110, 106, 150});
    }
    textC("ARROWS MOVE   ENTER SELECT   ESC QUIT", cx, oy_ + 206, Rgb{90, 86, 130});
}

void Menu::drawFree() {
    const int cx = ox_ + 200;
    textC("FREE PLAY", cx, oy_ + 22, Rgb{0, 228, 238}, true, 3);
    textC("THE PLAYLIST IN A RANDOM ORDER, A UNIVERSE PICKED FOR EACH SONG.", cx, oy_ + 56, Rgb{170, 166, 210});
    textC("LEVELS RISE WITH THE LINES YOU CLEAR.", cx, oy_ + 64, Rgb{170, 166, 210});
    textC("DIFFICULTY", cx, oy_ + 86, Rgb{120, 116, 160});
    for (int d = 0; d < DIFF_COUNT; d++) {
        const bool sel = d == freeDiff_;
        const int y = oy_ + 100 + d * 22;
        Rgb c = sel ? Rgb{255, 214, 0} : Rgb{120, 116, 160};
        const int tw = textC(difficultyName(d), cx, y, c, true, 2);
        if (sel && std::fmod(time_, 0.8f) < 0.55f) {
            text(">", cx - tw / 2 - 16, y, c, true, 2);
            text("<", cx + tw / 2 + 6, y, c, true, 2);
        }
    }
    char b[96];
    std::snprintf(b, sizeof(b), "GRAVITY AT %d%% OF ITS FULL SPEED, ON EVERY LEVEL", (int)std::lround(freePlayMultiplier(freeDiff_) * 100));
    textC(b, cx, oy_ + 172, Rgb{210, 206, 240});
    textC("ENTER START   ESC BACK", cx, oy_ + 206, Rgb{90, 86, 130});
}

void Menu::drawCampaign() {
    const int cx = ox_ + 200;
    textC("CAMPAIGN", cx, oy_ + 5, Rgb{255, 214, 0}, true, 2);
    // the difficulty row
    {
        int total = 0;
        for (int d = 0; d < DIFF_COUNT; d++) total += pixelfont::width(true, difficultyName(d)) + (d ? 18 : 0);
        int x = cx - total / 2;
        for (int d = 0; d < DIFF_COUNT; d++) {
            const int w = pixelfont::width(true, difficultyName(d));
            const bool sel = d == diff_;
            if (sel) rect(x - 4, oy_ + 22, w + 8, 11, row_ == 0 ? Rgb{255, 214, 0} : Rgb{70, 64, 120});
            text(difficultyName(d), x, oy_ + 24, sel ? (row_ == 0 ? Rgb{20, 12, 40} : Rgb{255, 255, 255}) : Rgb{110, 106, 150}, true, 1, !sel);
            if (sel && row_ == 0 && std::fmod(time_, 0.8f) < 0.55f) { text("<", x - 12, oy_ + 24, Rgb{255, 214, 0}, true); text(">", x + w + 7, oy_ + 24, Rgb{255, 214, 0}, true); }
            x += w + 18;
        }
    }
    if (!camp_ || camp_->size() == 0) {
        textC("NO CAMPAIGN DATA (DATA/CAMPAIGN.TSV)", cx, oy_ + 100, Rgb{255, 56, 72});
        return;
    }
    // the stages: a line of squares each
    const int n = camp_->size();
    const int rowH = std::min(12, 140 / std::max(1, n));
    const int y0 = oy_ + 40, sq = std::min(9, rowH - 2), step = sq + 5;
    const int labelX = ox_ + 112, sqX = ox_ + 160;
    for (int s = 0; s < n; s++) {
        const int y = y0 + s * rowH;
        const bool open = !progress_ || progress_->unlocked(s);
        const bool cur = row_ == 1 && s == stage_;
        char lb[24];
        std::snprintf(lb, sizeof(lb), "STAGE %d", s + 1);
        Rgb lc = !open ? Rgb{70, 68, 96} : cur ? Rgb{255, 255, 255} : Rgb{150, 146, 190};
        text(lb, labelX, y + (sq - 5) / 2, lc, false, 1, false);
        if (!open) { // a little padlock
            const int lx = labelX - 10, ly = y + (sq - 7) / 2;
            Rgb g{110, 106, 140};
            frame(lx + 1, ly, 4, 3, g);
            rect(lx, ly + 3, 6, 4, g);
            set(lx + 2, ly + 4, Rgb{40, 36, 60}); set(lx + 3, ly + 4, Rgb{40, 36, 60});
        }
        const auto& songs = camp_->stages()[s].songs;
        for (int k = 0; k < (int)songs.size(); k++) {
            const int x = sqX + k * step;
            unsigned char col[3];
            menuUniverseColor(camp_, songs[k].universe, col);
            const bool beat = progress_ && progress_->beaten(s, k, diff_);
            Rgb c{col[0], col[1], col[2]};
            if (!open) { const unsigned char v = (unsigned char)((col[0] + col[1] + col[2]) / 12 + 30); c = Rgb{v, v, (unsigned char)(v + 8)}; }
            rect(x, y, sq, sq, c);
            if (open) rect(x, y, sq, 1, Rgb{(unsigned char)std::min(255, c.r + 50), (unsigned char)std::min(255, c.g + 50), (unsigned char)std::min(255, c.b + 50)});
            if (beat) { // a check mark
                Rgb d{16, 12, 30};
                const int bx = x + sq / 2 - 3, by = y + sq / 2 - 1;
                set(bx, by + 1, d); set(bx + 1, by + 2, d); set(bx + 2, by + 3, d); set(bx + 3, by + 2, d);
                set(bx + 4, by + 1, d); set(bx + 5, by, d); set(bx + 6, by - 1, d);
            }
            if (cur && k == song_ && std::fmod(time_, 0.6f) < 0.42f) frame(x - 2, y - 2, sq + 4, sq + 4, Rgb{255, 255, 255});
        }
        // the stage's state on this difficulty
        const int sx = sqX + 5 * step + 6;
        if (progress_ && progress_->cleared(s, diff_)) {
            Rgb gold{255, 214, 0};
            text("CLEAR", sx, y + (sq - 5) / 2, gold, false, 1, false);
            text(std::to_string(progress_->stageBest(s, diff_)), sx + 24, y + (sq - 5) / 2, Rgb{200, 190, 120}, false, 1, false);
        } else if (progress_ && progress_->clearedAny(s)) {
            std::string on = "CLEARED ON";
            for (int d = 0; d < DIFF_COUNT; d++)
                if (progress_->cleared(s, d)) on += std::string(" ") + difficultyName(d);
            text(on, sx, y + (sq - 5) / 2, Rgb{100, 96, 130}, false, 1, false);
        }
    }
    // the song under the cursor
    const int py = oy_ + 186;
    rect(ox_ + 4, py - 4, 392, 1, Rgb{40, 36, 72});
    if (const CampaignSong* h = hovered()) {
        char b[160];
        std::snprintf(b, sizeof(b), "STAGE %d  SONG %d/%d", stage_ + 1, song_ + 1, (int)camp_->stages()[stage_].songs.size());
        text(b, ox_ + 8, py, Rgb{150, 146, 190}, false, 1, false);
        std::string title = asciiFold(h->title);
        while (pixelfont::width(true, title) > 384 - 80 && title.size() > 4) title.pop_back();
        unsigned char col[3];
        menuUniverseColor(camp_, h->universe, col);
        text(title, ox_ + 8, py + 8, Rgb{255, 255, 255}, true);
        std::string uni = h->universe.empty() ? "UNIVERSE PICKED BY THE SONG" : h->universe;
        std::snprintf(b, sizeof(b), "%s   VELOCITY %.1f", uni.c_str(), h->level[diff_]);
        text(b, ox_ + 8, py + 18, Rgb{col[0], col[1], col[2]}, false, 1, false);
        const bool beat = progress_ && progress_->beaten(stage_, song_, diff_);
        if (beat) std::snprintf(b, sizeof(b), "BEATEN  BEST %d", progress_->best(stage_, song_, diff_));
        else std::snprintf(b, sizeof(b), "NOT BEATEN ON %s", difficultyName(diff_));
        const int rx = ox_ + 392 - pixelfont::width(false, b);
        text(b, rx, py, beat ? Rgb{255, 214, 0} : Rgb{120, 116, 160}, false, 1, false);
        const std::string st = previewStatus();
        text(st, ox_ + 392 - pixelfont::width(false, st), py + 18, Rgb{120, 200, 255}, false, 1, false);
        if (song_ > 0) text("A RUN FROM HERE DOESN'T CLEAR THE STAGE", ox_ + 8, py + 28, Rgb{100, 96, 130}, false, 1, false);
    } else {
        textC("LEFT / RIGHT: DIFFICULTY   DOWN: THE STAGES", cx, py + 8, Rgb{150, 146, 190});
    }
    const char* hint = "ENTER PLAY   ESC BACK";
    text(hint, ox_ + 392 - pixelfont::width(false, hint), py + 28, Rgb{90, 86, 130}, false, 1, false);
}

void Menu::drawMessage() {
    if (msgT_ <= 0 || msg_.empty()) return;
    const int cx = ox_ + 200, cy = oy_ + 100;
    const int w = std::max(pixelfont::width(true, msg_, 3), pixelfont::width(false, msgSub_)) + 24;
    rect(cx - w / 2 - 2, cy - 26, w + 4, 56, Rgb{0, 0, 0});
    rect(cx - w / 2, cy - 24, w, 52, msgBad_ ? Rgb{70, 10, 30} : Rgb{20, 40, 90});
    frame(cx - w / 2, cy - 24, w, 52, msgBad_ ? Rgb{255, 56, 72} : Rgb{255, 214, 0});
    textC(msg_, cx, cy - 16, msgBad_ ? Rgb{255, 120, 130} : Rgb{255, 214, 0}, true, 3);
    textC(msgSub_, cx, cy + 14, Rgb{230, 226, 255});
}

void Menu::render(int fbw, int fbh) {
    if (style_ != ARCADE) { renderSmooth(fbw, fbh); return; }
    const int scale = std::max(1, std::min(fbw / 400, fbh / 225));
    cw_ = std::max(1, fbw / scale);
    ch_ = std::max(1, fbh / scale);
    ox_ = (cw_ - 400) / 2;
    oy_ = (ch_ - 225) / 2;
    px_.assign((size_t)cw_ * ch_ * 3, 0);
    drawBackground();
    if (screen_ == TITLE) drawTitle();
    else if (screen_ == FREE) drawFree();
    else drawCampaign();
    drawMessage();

    if (!prog_) prog_ = ugl::program(VS, FS, "menu");
    if (!tex_) glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (texW_ != cw_ || texH_ != ch_) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, cw_, ch_, 0, GL_RGB, GL_UNSIGNED_BYTE, px_.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        texW_ = cw_;
        texH_ = ch_;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cw_, ch_, GL_RGB, GL_UNSIGNED_BYTE, px_.data());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    // sharp pixels: the canvas covers scale * its size, centered
    glViewport((fbw - cw_ * scale) / 2, (fbh - ch_ * scale) / 2, cw_ * scale, ch_ * scale);
    glUseProgram(prog_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    ugl::set1i(prog_, "uTex", 0);
    ugl::set2f(prog_, "uCanvas", (float)cw_, (float)ch_);
    ugl::set1f(prog_, "uTime", time_);
    ugl::drawFullscreen();
    glViewport(0, 0, fbw, fbh);
}

bool Menu::screenshot(const std::string& path, int w, int h) {
    std::vector<unsigned char> px((size_t)w * h * 3), flipped(px.size());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++) std::memcpy(&flipped[(size_t)y * w * 3], &px[(size_t)(h - 1 - y) * w * 3], (size_t)w * 3);
    return stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3) != 0;
}

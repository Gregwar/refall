#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "stb_image_write.h"

Renderer::~Renderer() {
    cur_.u.reset();
    next_.u.reset();
    texA_.destroy();
    texB_.destroy();
    if (cellsTex_) glDeleteTextures(1, &cellsTex_);
    if (mixProg_) glDeleteProgram(mixProg_);
    if (cardProg_) glDeleteProgram(cardProg_);
    for (CardLine& l : cardLines_)
        if (l.tex) glDeleteTextures(1, &l.tex);
}

namespace {
// A break's frame: the world alone (B) and the whole frame (A) mixed by how much of the game shows, then toward black.
const char* BREAK_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uA, uB;
uniform float uPlay, uLight;
void main() {
    vec3 c = mix(texture(uB, vUv).rgb, texture(uA, vUv).rgb, uPlay);
    frag = vec4(c * uLight, 1.0);
}
)";
// A captured line of the title card: its letters (their colors, their coverage) over the box uRect (x0, y0, x1, y1
// in uv, bottom-left origin), dark ink turned white by uNight (on the black), premultiplied.
const char* CARD_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform sampler2D uTex;
uniform vec4 uRect;
uniform vec3 uWhite;
uniform float uAlpha, uNight;
void main() {
    vec2 q = (vUv - uRect.xy) / (uRect.zw - uRect.xy);
    if (q.x < 0.0 || q.y < 0.0 || q.x > 1.0 || q.y > 1.0) discard;
    vec4 t = texture(uTex, vec2(q.x, 1.0 - q.y));
    float a = t.a * uAlpha;
    // dark ink turns white on the black (light ink keeps its color)
    float l = dot(t.rgb, vec3(0.3, 0.55, 0.15));
    frag = vec4(mix(t.rgb, uWhite, uNight * (1.0 - smoothstep(0.35, 0.65, l))) * a, a);
}
)";
} // namespace

bool Renderer::init(int width, int height) {
    w_ = std::max(1, width);
    h_ = std::max(1, height);
    comp_.init();
    card_.init();
    if (!text_.init()) std::fprintf(stderr, "[text] the text engine could not start\n");
    mixProg_ = ugl::program(ugl::FULLSCREEN_VS, BREAK_FS, "break");
    cardProg_ = ugl::program(ugl::FULLSCREEN_VS, CARD_FS, "card");
    glGenTextures(1, &cellsTex_);
    glBindTexture(GL_TEXTURE_2D, cellsTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, Game::W, Game::H - Game::HIDDEN, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return glGetError() == GL_NO_ERROR;
}

void Renderer::resize(int width, int height) {
    w_ = std::max(1, width);
    h_ = std::max(1, height);
    each([&](Universe& u) { u.resize(w_, h_); });
    if (texA_.fbo) {
        texA_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        texB_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
}

// =====================================================================================================
// Universes
// =====================================================================================================

Renderer::Slot Renderer::make(int index, const SongLook& look, int level, float energy) {
    Slot s;
    s.index = std::clamp(index, 0, universes::count() - 1);
    s.u = universes::entry(s.index).make();
    if (!s.u->init(w_, h_)) std::fprintf(stderr, "[gl] warning: universe %s init reported a GL error\n", universes::entry(s.index).name);
    s.u->setSceneLines(sceneLinesFor(look));
    s.u->setSong(look, 0.f);
    s.u->setLevel(level, energy, 0.f, 0.f);
    if (bonus_) s.u->setBonus(true);
    return s;
}

void Renderer::setUniverse(int index, const SongLook& look, int level, float energy) {
    level_ = level;
    energy_ = energy;
    next_ = Slot{};
    state_ = IDLE;
    cur_ = Slot{}; // the old one's GL resources go first
    cur_ = make(index, look, level, energy);
    std::printf("[universe] %s\n", label().c_str());
}

void Renderer::finishChange() {
    if (state_ == IDLE || !next_.u) return;
    next_.u->setBridge(Bridge{});
    cur_ = std::move(next_);
    next_ = Slot{};
    state_ = IDLE;
}

void Renderer::changeUniverse(int index, const SongLook& look, int level, float energy, const Change& c) {
    if (!cur_.u) {
        setUniverse(index, look, level, energy);
        return;
    }
    if (state_ == RUNNING) finishChange(); // a change asked during another: that one ends at once
    level_ = level;
    energy_ = energy;
    next_ = Slot{};
    SongLook l = look;
    if (index == cur_.index && l.variant < 0) l.avoidVariant = cur_.u->variant(); // a universe following itself: another variant
    next_ = make(index, l, level, energy);
    want_ = c;
    changes_++;
    const TransitionHints a = cur_.u->hints(), b = next_.u->hints();
    kind_ = c.transition >= 0 ? std::min(c.transition, transitions::count() - 1)
                              : transitions::pick(a, b, c.why, look.seed ^ splitmix64(changes_), lastKind_);
    lastKind_ = kind_;
    state_ = PENDING;
    pendingInit_ = true;
    pendingT_ = 0;
    p_ = 0;
    if (!texA_.fbo) {
        texA_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        texB_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
    std::printf("[universe] %s -> %s (%s)\n", universes::label(cur_.index, cur_.u->variant()).c_str(), label().c_str(),
                transitions::name(kind_));
}

void Renderer::setSong(const SongLook& look, float seconds) {
    finishChange();
    if (!cur_.u) return;
    cur_.u->setSceneLines(sceneLinesFor(look)); // the same universe into the next song: on at its pace
    cur_.u->setSong(look, seconds);
}

int Renderer::universe() const { return next_.u ? next_.index : cur_.index; }
int Renderer::variant() const {
    const Slot& s = next_.u ? next_ : cur_;
    return s.u ? s.u->variant() : 0;
}
std::string Renderer::label() const { return universe() < 0 ? std::string() : universes::label(universe(), variant()); }

// =====================================================================================================
// Forwarding: both universes hear everything during a change (locks and clears look right in both halves)
// =====================================================================================================

void Renderer::setLevel(int level, float energy, float seconds, float impact) {
    level_ = level;
    energy_ = energy;
    each([&](Universe& u) { u.setLevel(level, energy, seconds, impact); });
}
void Renderer::refresh(float strength) { each([&](Universe& u) { u.refresh(strength); }); }
void Renderer::drop(float strength) { each([&](Universe& u) { u.drop(strength); }); }
void Renderer::onEvent(const GameEvent& ev, const Game& game) { each([&](Universe& u) { u.onEvent(ev, game); }); }
void Renderer::kick(float amount) { each([&](Universe& u) { u.kick(amount); }); }
void Renderer::levelUp() { each([&](Universe& u) { u.levelUp(); }); }
void Renderer::setBonus(bool on) {
    bonus_ = on;
    each([&](Universe& u) { u.setBonus(on); });
}
void Renderer::forceFx(int slow, int quadClear) { each([&](Universe& u) { u.forceFx(slow, quadClear); }); }

vec2 Renderer::project(const vec3& p) const {
    if (!cur_.u) return layout::project(p, w_, h_);
    const vec2 a = cur_.u->project(p);
    if (state_ != RUNNING || !next_.u) return a;
    const vec2 b = next_.u->project(p);
    const float k = smoothstepf(0.f, 1.f, p_);
    return {lerpf(a.x, b.x, k), lerpf(a.y, b.y, k)};
}

// =====================================================================================================
// Frame
// =====================================================================================================

void Renderer::render(const Game& game, const MusicState& music, double time, float dt, bool paused,
                      const std::vector<HudText>& hud, float fade) {
    if (!cur_.u) return;
    const Frame f{game, music, time, dt, paused, dim_, fade, hud, panels_};
    const float bpm = music.hasSong ? std::clamp(music.bpm, 40.f, 220.f) : 120.f;

    // ---- A change waits for the next downbeat (or the next beat if the bar is far), then runs for a few bars.
    if (state_ == PENDING) {
        if (pendingInit_) {
            pendingInit_ = false;
            const float barSec = 240.f / bpm;
            const float bars = want_.bars > 0 ? want_.bars : std::clamp(std::round(4.f / barSec), 1.f, 4.f);
            beats_ = bars * 4.f;
            seconds_ = beats_ * 60.f / bpm;
            startBeat_ = music.beat;
            if (music.hasSong && want_.onBeat) {
                const double nextBar = (std::floor(music.beat / 4.0 + 1e-3) + 1.0) * 4.0;
                startBeat_ = (nextBar - music.beat) * 60.0 / bpm <= 1.6 ? nextBar : std::floor(music.beat) + 1.0;
            }
        }
        if (!paused) pendingT_ += dt;
        const bool reached = !music.hasSong || !want_.onBeat || music.beat >= startBeat_ - 1e-3 ||
                             music.beat < startBeat_ - 12.0 || pendingT_ > 3.f;
        if (reached) {
            state_ = RUNNING;
            p_ = 0;
        }
    }
    if (state_ == RUNNING && !paused) p_ += dt / std::max(0.2f, seconds_);

    if (state_ != RUNNING) {
        if (brk_.play < 1.f || brk_.black > 0.f || brk_.card[0].alpha > 0.f || brk_.card[1].alpha > 0.f || brk_.cardAlpha > 0.f) {
            renderBreak(f);
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w_, h_);
        if (cur_.u->hostText()) {
            std::vector<text::Line> lines;
            std::vector<vec4> boxes;
            hudLines(*cur_.u, hud, lines, boxes);
            Frame fl = f;
            fl.hudBoxes = &boxes;
            cur_.u->render(fl, 0);
            letter(*cur_.u, hud, 1.f, time);
            return;
        }
        cur_.u->render(f, 0);
        return;
    }

    // ---- Both universes, each in its own texture, meeting at the bridge in the middle of the change.
    const float p = saturate(p_);
    const TransitionHints ha = cur_.u->hints(), hb = next_.u->hints();
    const float depth = transitions::bridgeDepth(kind_);
    Bridge ba = transitions::bridgeFor(ha, hb, false), bb = transitions::bridgeFor(hb, ha, true);
    ba.amount = depth * smoothstepf(0.f, 0.5f, p);
    bb.amount = depth * (1.f - smoothstepf(0.5f, 1.f, p));
    cur_.u->setBridge(ba);
    next_.u->setBridge(bb);

    std::vector<text::Line> lines;
    std::vector<vec4> boxA, boxB;
    Frame fa = f, fb = f;
    if (cur_.u->hostText()) { hudLines(*cur_.u, hud, lines, boxA); fa.hudBoxes = &boxA; }
    if (next_.u->hostText()) { hudLines(*next_.u, hud, lines, boxB); fb.hudBoxes = &boxB; }
    texA_.bind();
    cur_.u->render(fa, texA_.fbo);
    texB_.bind();
    next_.u->render(fb, texB_.fbo);

    // The stack, for the bridge the host draws (rows from the bottom).
    unsigned char cells[(Game::H - Game::HIDDEN) * Game::W];
    for (int y = Game::HIDDEN; y < Game::H; y++)
        for (int x = 0; x < Game::W; x++)
            cells[(Game::H - 1 - y) * Game::W + x] = game.cell(x, y).type >= 0 ? 255 : 0;
    if (game.hasPiece() && !game.over()) {
        int pc[4][2];
        game.pieceCells(game.piece(), pc);
        for (auto& c : pc)
            if (c[1] >= Game::HIDDEN && c[1] < Game::H) cells[(Game::H - 1 - c[1]) * Game::W + c[0]] = 180;
    }
    glBindTexture(GL_TEXTURE_2D, cellsTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Game::W, Game::H - Game::HIDDEN, GL_RED, GL_UNSIGNED_BYTE, cells);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w_, h_);
    transitions::Inputs in;
    in.texA = texA_.tex;
    in.texB = texB_.tex;
    in.cells = cellsTex_;
    in.width = w_;
    in.height = h_;
    in.progress = p;
    in.beats = beats_;
    in.a = ha;
    in.b = hb;
    in.bridgeA = ba;
    in.bridgeB = bb;
    in.hostBridgeA = cur_.u->bridgeable() ? 0.f : ba.amount;
    in.hostBridgeB = next_.u->bridgeable() ? 0.f : bb.amount;
    in.kick = music.kick;
    in.beatPhase = music.beatPhase;
    in.time = time;
    in.seed = changes_ * 7919u;
    comp_.draw(kind_, in);
    // the lettering changes hands at the bridge
    const float handover = smoothstepf(0.35f, 0.65f, p);
    if (cur_.u->hostText()) letter(*cur_.u, hud, 1.f - handover, time);
    if (next_.u->hostText()) letter(*next_.u, hud, handover, time);

    if (p_ >= 1.f) finishChange();
}

// A break: the world alone (a blank board, the bare HUD) and the whole frame, each in its own texture while both
// show (the world alone renders without time passing, the whole frame moves the universe on), mixed, darkened,
// then the title card on top.
void Renderer::renderBreak(const Frame& f) {
    if (!texA_.fbo) {
        texA_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
        texB_.create(w_, h_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
    const bool host = cur_.u->hostText();
    if (!host && !brk_.cardHud.empty() && brk_.cardId != cardId_ && brk_.blank) {
        cardId_ = brk_.cardId;
        captureCard(f);
    }
    const float play = saturate(brk_.play);
    const bool bare = play < 1.f && brk_.blank, whole = play > 0.f || !brk_.blank;
    if (brk_.black < 0.999f) {
        std::vector<text::Line> lines;
        std::vector<vec4> boxBare, boxWhole;
        if (bare) {
            Frame fb{*brk_.blank, f.music, f.time, whole ? 0.f : f.dt, false, false, f.fade, brk_.bareHud, panels_, true};
            if (host) { hudLines(*cur_.u, brk_.bareHud, lines, boxBare); fb.hudBoxes = &boxBare; }
            texB_.bind();
            cur_.u->render(fb, texB_.fbo);
        }
        if (whole) {
            Frame fw = f;
            if (host) { hudLines(*cur_.u, f.hud, lines, boxWhole); fw.hudBoxes = &boxWhole; }
            texA_.bind();
            cur_.u->render(fw, texA_.fbo);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w_, h_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(mixProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whole ? texA_.tex : texB_.tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bare ? texB_.tex : texA_.tex);
    glActiveTexture(GL_TEXTURE0);
    ugl::set1i(mixProg_, "uA", 0);
    ugl::set1i(mixProg_, "uB", 1);
    ugl::set1f(mixProg_, "uPlay", bare ? play : 1.f);
    ugl::set1f(mixProg_, "uLight", 1.f - saturate(brk_.black));
    ugl::drawFullscreen();
    // White on the black; on a light universe it turns to the universe's ink as its world comes up.
    const TransitionHints hi = cur_.u->hints();
    const float day = hi.light ? 1.f - saturate(brk_.black) : 0.f;
    const vec3 white(0.96f, 0.95f, 0.92f), dark = ugl::toSrgb(hi.ink) * 0.8f;
    if (host) {
        // the HUD over the mixed frame: the world alone's lines and the whole frame's, each line once; the card's
        // lines gliding home instead of the corner's own
        const bool card = !brk_.cardHud.empty() && brk_.cardIn > 0.f;
        std::vector<HudText> lines;
        // (the tempo and key under the title wait for it to land)
        const float landed = card ? smoothstepf(0.85f, 1.f, brk_.cardShrink) : 1.f;
        for (const HudText& h : f.hud) {
            if (card && (h.role == HUD_TITLE || h.role == HUD_UNIVERSE)) continue;
            lines.push_back(h);
            lines.back().alpha *= (whole ? (bare ? play : 1.f) : 0.f) * (h.role == HUD_INFO ? landed : 1.f);
        }
        for (const HudText& h : brk_.bareHud) {
            if (!bare || (card && (h.role == HUD_TITLE || h.role == HUD_UNIVERSE))) continue;
            const float a = h.alpha * (1.f - play) * (h.role == HUD_INFO ? landed : 1.f);
            auto same = std::find_if(lines.begin(), lines.end(), [&](const HudText& o) { return o.role == h.role && o.text == h.text; });
            if (same != lines.end()) same->alpha += a;
            else {
                lines.push_back(h);
                lines.back().alpha = a;
            }
        }
        // the lines go dark with the frame they letter (as they did inside it), a dialog (the pause menu) aside
        const float lit = 1.f - saturate(brk_.black);
        for (HudText& h : lines)
            if (h.role != HUD_DIALOG) h.alpha *= lit;
        letter(*cur_.u, lines, 1.f, f.time, saturate(brk_.black));
        if (card) letterCard(*cur_.u, f.time);
        if (brk_.card[0].alpha > 0.f || brk_.card[1].alpha > 0.f)
            if (!card) card_.draw(brk_.card, 2, w_, h_, white + (dark - white) * day, vec3(0.f) + (white - vec3(0.f)) * day);
        return;
    }
    if (!brk_.cardHud.empty() && cardLines_[0].ok && cardId_ == brk_.cardId) drawCard();
    else card_.draw(brk_.card, 2, w_, h_, white + (dark - white) * day, vec3(0.f) + (white - vec3(0.f)) * day);
}

// ---- The HUD lettered by the host.

namespace {
// The default style: the title card's fonts (Cormorant Garamond Italic, Tenor Sans), pale ink on a soft dark halo.
const unsigned char HOST_SERIF[] = {
#include "universes/host/CormorantGaramond-Italic.ttf.inc"
};
const unsigned char HOST_SANS[] = {
#include "universes/host/TenorSans-Regular.ttf.inc"
};
} // namespace

void Universe::hudLine(const HudText& h, text::Line& out) {
    text::Font* serif = text::font(HOST_SERIF);
    text::Font* sans = text::font(HOST_SANS);
    const float b = h.scale * 12.f;
    const vec3 ink(0.94f, 0.93f, 0.9f), accent(0.95f, 0.8f, 0.55f);
    const bool italic = h.role == HUD_TITLE || h.role == HUD_SECTION || h.role == HUD_NOTICE || h.role == HUD_TOAST;
    const bool caps = h.role == HUD_UNIVERSE || h.role == HUD_LABEL || h.role == HUD_INFO;
    out.add(caps ? text::upper(h.text) : h.text, italic ? serif : sans, italic ? b * 1.25f : b, h.accent ? accent : ink,
            caps ? 0.25f : 0.02f, std::min(1.f, h.alpha));
    out.look.halo = vec3(0.f);
    out.look.haloA = 0.45f;
    out.look.haloW = 0.12f;
}

void Renderer::hudLines(Universe& u, const std::vector<HudText>& hud, std::vector<text::Line>& lines, std::vector<vec4>& boxes) {
    lines.clear();
    boxes.clear();
    for (const HudText& h : hud) {
        lines.emplace_back();
        if (h.alpha > 0.002f && !h.text.empty()) u.hudLine(h, lines.back());
        const text::Box b = text_.measure(lines.back());
        const float x = ugl::hudLeft(h, b.width(), w_); // (the box's x0 is the line's own dx)
        boxes.push_back(vec4{x + b.x0, h.y + b.y0, x + b.x1, h.y + b.y1});
    }
}

void Renderer::letter(Universe& u, const std::vector<HudText>& hud, float weight, double time, float night) {
    if (weight <= 0.002f) return;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const vec3 white(0.96f, 0.95f, 0.92f);
    for (const HudText& h : hud) {
        if (h.alpha * weight <= 0.002f || h.text.empty()) continue;
        text::Line l;
        u.hudLine(h, l);
        l.alpha *= weight;
        if (night > 0.f) {
            for (text::Span& s : l.spans) s.ink = s.ink + (white - s.ink) * night;
            l.look.haloA *= 1.f - night;
            l.look.shadowA *= 1.f - night;
            l.look.outlineA *= 1.f - night;
            l.plate.alpha *= 1.f - night;
            l.plate.borderA *= 1.f - night;
            if (l.blend == text::BLEND_MULTIPLY) l.blend = text::BLEND_OVER;
        }
        const text::Box b = text_.measure(l);
        text_.draw(l, ugl::hudLeft(h, b.width(), w_), h.y, w_, h_, (float)time);
    }
}

// The title card of a universe the host letters: its corner lines (the song, the universe) big in the middle, then
// gliding and shrinking onto the corner, where they are the HUD's own lines: the same line all along.
void Renderer::letterCard(Universe& u, double time) {
    const HudText* t = nullptr;
    const HudText* un = nullptr;
    for (const HudText& h : brk_.cardHud) {
        if (h.role == HUD_TITLE) t = &h;
        if (h.role == HUD_UNIVERSE) un = &h;
    }
    const float e = saturate(brk_.cardShrink), night = saturate(brk_.black), in = saturate(brk_.cardIn);
    // each line's size in the middle: its capitals as tall as asked, never wider than most of the screen
    struct Big { const HudText* h; float k, top; text::Box box; };
    auto bigOf = [&](const HudText* h, float cap) {
        Big g{h, 1.f, 0.f, {}};
        if (!h) return g;
        text::Line l;
        u.hudLine(*h, l);
        const text::Box b = text_.measure(l);
        g.k = std::clamp(std::min(cap / std::max(1.f, b.cap), w_ * 0.86f / std::max(1.f, b.width())), 1.f, 8.f);
        HudText big = *h;
        big.scale *= g.k;
        text::Line lb;
        u.hudLine(big, lb);
        g.box = text_.measure(lb);
        return g;
    };
    Big bt = bigOf(t, h_ * 0.075f), bu = bigOf(un, h_ * 0.022f);
    const float gap = h_ * 0.035f;
    const float tH = t ? bt.box.y1 - bt.box.y0 : 0.f, uH = un ? bu.box.y1 - bu.box.y0 : 0.f;
    const float top = h_ * 0.45f - (tH + gap + uH) * 0.5f;
    bt.top = top;
    bu.top = top + tH + gap;
    std::vector<HudText> lines;
    for (const Big* g : {&bt, &bu}) {
        if (!g->h) continue;
        // big: centered; small: the corner line itself (its anchor, its size)
        const float k = std::exp(std::log(g->k) * (1.f - e));
        const float bigX = w_ * 0.5f - (g->box.x0 + g->box.x1) * 0.5f, bigY = g->top - g->box.y0;
        HudText h = *g->h;
        text::Line l0;
        u.hudLine(h, l0);
        const text::Box small = text_.measure(l0);
        const float smallX = ugl::hudLeft(h, small.width(), w_);
        // the anchor moves as the box does: the box's left edge and cap line go straight from the middle to the corner
        h.scale *= k;
        h.center = false;
        const float w0 = g->box.x0 * (k / g->k) , y0 = g->box.y0 * (k / g->k);
        h.x = lerpf(bigX + g->box.x0, smallX + small.x0, e) - w0;
        h.y = lerpf(bigY + g->box.y0, h.y + small.y0, e) - y0;
        h.alpha *= in;
        lines.push_back(h);
    }
    letter(u, lines, 1.f, time, night);
}

// ---- The title card in the universe's lettering. Each corner line is rendered by the universe on its bare world
// at k times its size (about the screen's corner it hangs from: offsets proportional to the text size scale with
// it), and once without: the difference is the lettering. Its coverage is kept with one ink; shrunk back by k about
// the same corner, it lands exactly on the universe's own corner lettering, which takes over at the end.
void Renderer::captureCard(const Frame& f) {
    for (CardLine& l : cardLines_) l.ok = false;
    const HudText* title = nullptr;
    const HudText* uni = nullptr;
    for (const HudText& h : brk_.cardHud) {
        if (h.role == HUD_TITLE) title = &h;
        if (h.role == HUD_UNIVERSE) uni = &h;
    }
    if (!title || !uni) return;
    if (!captureLine(f, *title, false, h_ * 0.075f, cardLines_[0])) return;
    if (!captureLine(f, *uni, true, h_ * 0.024f, cardLines_[1])) cardLines_[0].ok = false;
}

bool Renderer::captureLine(const Frame& f, const HudText& line, bool fromBottom, float bigCap, CardLine& out) {
    const std::vector<HudText> none;
    auto grab = [&](const std::vector<HudText>& hud, std::vector<unsigned char>& px) {
        Frame fb{*brk_.blank, f.music, f.time, 0.f, false, false, f.fade, hud, panels_, true};
        fb.card = true;
        texB_.bind();
        cur_.u->render(fb, texB_.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, texB_.fbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        px.resize((size_t)w_ * h_ * 4);
        glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    };
    std::vector<unsigned char> world, base, with;
    grab(none, world);
    // the lettering at scale 1 first: its height gives the scale that makes it as big as the card asks
    float k = 1.f;
    for (int pass = 0; pass < 3; pass++) {
        HudText big = line;
        big.alpha = line.alpha;
        const float ay = fromBottom ? (float)h_ : 0.f;
        big.scale = line.scale * k;
        big.x = line.x * k;
        big.y = ay + (line.y - ay) * k;
        grab({big}, with);
        base = world;
        // the difference, and the lettering's ink (the color where it differs most)
        std::vector<float> d((size_t)w_ * h_);
        float dMax = 0;
        for (size_t i = 0; i < d.size(); i++) {
            float m = 0;
            for (int c = 0; c < 3; c++) m = std::max(m, std::fabs((float)with[i * 4 + c] - (float)base[i * 4 + c]) / 255.f);
            d[i] = m;
            dMax = std::max(dMax, m);
        }
        if (dMax < 0.12f) return false;
        double ink[3] = {0, 0, 0}, wsum = 0;
        for (size_t i = 0; i < d.size(); i++)
            if (d[i] >= 0.7f * dMax) {
                for (int c = 0; c < 3; c++) ink[c] += with[i * 4 + c] / 255.0;
                wsum += 1;
            }
        const vec3 I((float)(ink[0] / wsum), (float)(ink[1] / wsum), (float)(ink[2] / wsum));
        // a pixel's difference when it is a move toward the ink, else none (see the coverage below)
        auto toward = [&](const unsigned char* W8, const unsigned char* P8) {
            const vec3 W(W8[0] / 255.f, W8[1] / 255.f, W8[2] / 255.f), P(P8[0] / 255.f, P8[1] / 255.f, P8[2] / 255.f);
            const vec3 dv = P - W, iv = I - W;
            const float li = length(iv), ld = length(dv);
            if (li < 0.14f || dot(dv, iv) < 0.5f * li * ld) return 0.f;
            return std::max({std::fabs(dv.x), std::fabs(dv.y), std::fabs(dv.z)});
        };
        if (pass > 0) {
            // The lettering again lower (or higher) on the world, each of its pixels kept from where it stands out
            // most: where the world behind it is as light as its ink (a beam of light, a bright sky) another place
            // shows it.
            std::vector<float> best(d.size());
            for (size_t i = 0; i < d.size(); i++) best[i] = toward(&base[i * 4], &with[i * 4]);
            std::vector<unsigned char> shot;
            const int dir = fromBottom ? -1 : 1;
            for (int j = 1; j < 3; j++) {
                const int sh = dir * (int)std::lround(h_ * 0.27f * j);
                HudText moved = big;
                moved.y += (float)sh;
                grab({moved}, shot);
                for (int y = 0; y < h_; y++) {
                    const int ys = y + sh; // top-left rows: the moved lettering's row
                    if (ys < 0 || ys >= h_) continue;
                    const size_t r0 = (size_t)(h_ - 1 - y) * w_, rs = (size_t)(h_ - 1 - ys) * w_;
                    for (int x = 0; x < w_; x++) {
                        const size_t i = r0 + x, is = rs + x;
                        const float v = toward(&world[is * 4], &shot[is * 4]);
                        if (v > best[i] + 0.02f) {
                            best[i] = v;
                            for (int c = 0; c < 4; c++) { base[i * 4 + c] = world[is * 4 + c]; with[i * 4 + c] = shot[is * 4 + c]; }
                            d[i] = v;
                        }
                    }
                }
            }
        }
        // coverage: how much each pixel differs, against the lettering's core (its strongest differences), with a
        // firm threshold: a soft glow or halo around the letters (the universe's style over its world, a blob on
        // black) is left out, the letters' own edges kept. Only a move toward the ink counts: a halo or a plate
        // lifting a line of the world into the paper's color (MANGA's panel borders) is not the lettering.
        std::vector<float> strong;
        for (float v : d)
            if (v > 0.15f * dMax) strong.push_back(v);
        std::nth_element(strong.begin(), strong.begin() + strong.size() * 4 / 5, strong.end());
        const float dCore = std::max(0.05f, strong[strong.size() * 4 / 5]);
        std::vector<float> a(d.size(), 0.f), opac;
        std::vector<vec3> col(d.size(), vec3(0.f));
        int bx0 = w_, by0 = h_, bx1 = -1, by1 = -1;
        for (int y = 0; y < h_; y++)
            for (int x = 0; x < w_; x++) {
                const size_t i = (size_t)y * w_ + x;
                if (d[i] < 0.03f) continue;
                const vec3 W(base[i * 4] / 255.f, base[i * 4 + 1] / 255.f, base[i * 4 + 2] / 255.f);
                const vec3 P(with[i * 4] / 255.f, with[i * 4 + 1] / 255.f, with[i * 4 + 2] / 255.f);
                const vec3 dv = P - W, iv = I - W;
                const float li = length(iv), ld = length(dv);
                if (li < 0.14f || dot(dv, iv) < 0.5f * li * ld) continue;
                const float r = std::min(1.f, d[i] / dCore);
                const float v = smoothstepf(0.45f, 0.9f, r);
                if (v <= 0.f) continue;
                a[i] = v;
                // the ink's own color here (several inks: SKY WHALE's name and its variant in the accent)
                const float f = std::max(0.25f, r);
                col[i] = vec3(std::clamp(W.x + dv.x / f, 0.f, 1.f), std::clamp(W.y + dv.y / f, 0.f, 1.f), std::clamp(W.z + dv.z / f, 0.f, 1.f));
                if (r > 0.95f) opac.push_back(std::clamp(dot(dv, iv) / (li * li), 0.f, 1.f));
                if (v > 0.06f) {
                    const int yt = h_ - 1 - y; // top-left origin
                    bx0 = std::min(bx0, x); bx1 = std::max(bx1, x);
                    by0 = std::min(by0, yt); by1 = std::max(by1, yt);
                }
            }
        float aMax = 1.f; // the lettering's own opacity (its core's move toward the ink)
        if (!opac.empty()) {
            std::nth_element(opac.begin(), opac.begin() + opac.size() / 2, opac.end());
            aMax = std::max(0.2f, opac[opac.size() / 2]);
        }
        if (bx1 < 0) return false;
        const float bw = (float)(bx1 - bx0 + 1), bh = (float)(by1 - by0 + 1);
        if (pass == 0) {
            // the scale for the card, from the lettering at its own size (bigger, a universe may leave out what runs
            // off the screen, and the box would shrink): the capital height asked (the box is a little taller than
            // the capitals), never wider than most of the screen
            k = std::min({bigCap * 1.25f / bh, w_ * 0.86f / bw, 6.f});
            continue;
        }
        // keep it: the coverage cropped to its box (a margin for the glow), normalized
        const int m = 4;
        const int x0 = std::max(0, bx0 - m), x1 = std::min(w_ - 1, bx1 + m);
        const int y0 = std::max(0, by0 - m), y1 = std::min(h_ - 1, by1 + m);
        const int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
        std::vector<unsigned char> cov((size_t)cw * ch * 4);
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) {
                const size_t i = (size_t)(h_ - 1 - (y0 + y)) * w_ + (x0 + x);
                unsigned char* o = &cov[((size_t)y * cw + x) * 4];
                o[0] = (unsigned char)std::lround(255.f * col[i].x);
                o[1] = (unsigned char)std::lround(255.f * col[i].y);
                o[2] = (unsigned char)std::lround(255.f * col[i].z);
                o[3] = (unsigned char)std::lround(255.f * a[i]);
            }
        // the edges' color bled into the transparent texels around, so the mipmaps don't darken them
        for (int pass2 = 0; pass2 < 3; pass2++)
            for (int y = 0; y < ch; y++)
                for (int x = 0; x < cw; x++) {
                    unsigned char* o = &cov[((size_t)y * cw + x) * 4];
                    if (o[3] > 8) continue;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            const int xx = x + dx, yy = y + dy;
                            if (xx < 0 || yy < 0 || xx >= cw || yy >= ch) continue;
                            const unsigned char* n = &cov[((size_t)yy * cw + xx) * 4];
                            if (n[3] > 8) { o[0] = n[0]; o[1] = n[1]; o[2] = n[2]; }
                        }
                }
        if (!out.tex) glGenTextures(1, &out.tex);
        glBindTexture(GL_TEXTURE_2D, out.tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, cov.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        out.x0 = (float)x0; out.y0 = (float)y0; out.x1 = (float)x1 + 1; out.y1 = (float)y1 + 1;
        out.k = k;
        out.ax = 0.f;
        out.ay = fromBottom ? (float)h_ : 0.f;
        out.aMax = aMax;
        out.ok = true;
        // Test hook: ZEN_CARD_DEBUG=DIR writes each capture (the world, the world with the lettering, the coverage).
        if (const char* dbg = std::getenv("ZEN_CARD_DEBUG")) {
            const std::string pre = std::string(dbg) + "/card-" + std::to_string(fromBottom ? 1 : 0);
            stbi_flip_vertically_on_write(1);
            stbi_write_png((pre + "-world.png").c_str(), w_, h_, 4, world.data(), w_ * 4);
            stbi_write_png((pre + "-with.png").c_str(), w_, h_, 4, with.data(), w_ * 4);
            stbi_flip_vertically_on_write(0);
            stbi_write_png((pre + "-cov.png").c_str(), cw, ch, 4, cov.data(), cw * 4);
        }
        std::printf("[card] %s captured at x%.2f (%.0f x %.0f px)\n", line.text.c_str(), k, bw, bh);
        return true;
    }
    return false;
}

// The captured lines: big in the middle (the title above the universe), shrinking onto their corners, in their
// own colors (dark ink white on the black, taking its color as the world comes up).
void Renderer::drawCard() {
    const float e = saturate(brk_.cardShrink), day = 1.f - saturate(brk_.black);
    const vec3 white(0.96f, 0.95f, 0.92f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(cardProg_);
    ugl::set1i(cardProg_, "uTex", 0);
    glActiveTexture(GL_TEXTURE0);
    const CardLine& t = cardLines_[0];
    const CardLine& u = cardLines_[1];
    const float gap = h_ * 0.035f;
    const float tH = t.y1 - t.y0, uH = u.y1 - u.y0;
    const float top = h_ * 0.45f - (tH + gap + uH) * 0.5f;
    for (int i = 0; i < 2; i++) {
        const CardLine& l = cardLines_[i];
        // big: centered; small: the capture shrunk by k about its corner
        const float bw = l.x1 - l.x0, bh = l.y1 - l.y0;
        const float bx0 = w_ * 0.5f - bw * 0.5f, by0 = i == 0 ? top : top + tH + gap;
        const float sx0 = l.ax + (l.x0 - l.ax) / l.k, sy0 = l.ay + (l.y0 - l.ay) / l.k;
        const float sx1 = l.ax + (l.x1 - l.ax) / l.k, sy1 = l.ay + (l.y1 - l.ay) / l.k;
        const float x0 = lerpf(bx0, sx0, e), y0 = lerpf(by0, sy0, e), x1 = lerpf(bx0 + bw, sx1, e), y1 = lerpf(by0 + bh, sy1, e);
        ugl::set4f(cardProg_, "uRect", x0 / w_, 1.f - y1 / h_, x1 / w_, 1.f - y0 / h_);
        ugl::set3f(cardProg_, "uWhite", white);
        ugl::set1f(cardProg_, "uNight", 1.f - day);
        ugl::set1f(cardProg_, "uAlpha", saturate(brk_.cardAlpha) * lerpf(i == 0 ? 1.f : 0.85f, l.aMax, e));
        glBindTexture(GL_TEXTURE_2D, l.tex);
        ugl::drawFullscreen();
    }
    glDisable(GL_BLEND);
}

void Renderer::readFrame(std::vector<unsigned char>& px, int& w, int& h) {
    w = w_;
    h = h_;
    px.resize((size_t)w_ * h_ * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w_, h_, GL_RGB, GL_UNSIGNED_BYTE, px.data());
}

bool Renderer::screenshot(const std::string& path) {
    std::vector<unsigned char> px, flipped;
    int w, h;
    readFrame(px, w, h);
    flipped.resize(px.size());
    for (int y = 0; y < h; y++) std::memcpy(&flipped[(size_t)y * w * 3], &px[(size_t)(h - 1 - y) * w * 3], w * 3);
    return stbi_write_png(path.c_str(), w, h, 3, flipped.data(), w * 3) != 0;
}

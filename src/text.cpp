#include "text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <map>

#include "library.hpp"
#include "ugl.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace text {

namespace {

// Glyphs are baked at BAKE_PX, their distance field reaching PAD pixels out of (and into) the outline: the effects
// reach PAD / BAKE_PX em around the letters.
constexpr float BAKE_PX = 96.f;
constexpr int PAD = 20;
constexpr float DIST_SCALE = 128.f / PAD; // field units (0..255) per baked pixel
constexpr int ATLAS = 2048;

uint32_t nextCodepoint(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t cp = n == 3 ? c & 0x07 : n == 2 ? c & 0x0F : c & 0x1F;
    if (n == 0) return 0xFFFD;
    for (; n > 0 && i < s.size(); n--) {
        const unsigned char d = (unsigned char)s[i];
        if ((d & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (d & 0x3F);
        i++;
    }
    return cp;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F));
    }
}

// Latin-1 and Latin Extended-A case pairs (enough for song titles).
uint32_t toUpper(uint32_t c) {
    if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 0x20;
    if (c == 0xFF) return 0x178;
    if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149) {
        const bool oddPairs = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
        if (oddPairs ? (c % 2 == 0) : (c % 2 == 1)) return c - 1;
    }
    return c;
}
uint32_t toLower(uint32_t c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return c + 0x20;
    if (c == 0x178) return 0xFF;
    if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149) {
        const bool oddPairs = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
        if (oddPairs ? (c % 2 == 1) : (c % 2 == 0)) return c + 1;
    }
    return c;
}
bool isLetter(uint32_t c) { return toUpper(c) != toLower(c) || (c >= '0' && c <= '9'); }

const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;    // pixels, top-left origin
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec2 aEm;     // pixels per em, the letter's index
layout(location = 3) in vec2 aLine;   // where in the line (em of the first span)
layout(location = 4) in vec4 aInk;
uniform vec2 uRes;
uniform vec2 uShift;                  // the shadow pass: its offset (em)
uniform float uTime;
uniform vec4 uP0, uP1;
out vec2 vUv;
out float vEm, vGlyph;
out vec2 vLine;
out vec4 vInk;
//MOVE
void main() {
    vec2 p = aPos + uShift * aEm.x;
#ifdef MOVE
    p += move(aEm.y, aLine) * aEm.x;
#endif
    vUv = aUv;
    vEm = aEm.x;
    vGlyph = aEm.y;
    vLine = aLine;
    vInk = aInk;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
}
)";

const char* FS_HEAD = R"(#version 330 core
in vec2 vUv;
in float vEm, vGlyph;
in vec2 vLine;
in vec4 vInk;
out vec4 frag;
uniform sampler2D uAtlas;
uniform float uTime;
uniform vec4 uP0, uP1;
uniform vec2 uRes;
uniform int uPass;                  // 0: the shadow, 1: the letters
uniform float uBake, uScale;        // the baking size, field units per baked pixel
uniform float uWeight;
uniform vec4 uHalo;                 // rgb, alpha
uniform vec3 uHaloShape;            // width (em), power, smoothstep falloff
uniform vec4 uGlow;
uniform float uGlowW;
uniform vec4 uOutline;
uniform float uOutlineW;
uniform vec4 uShadow;
uniform vec2 uShadowShape;          // growth, softness (em)
struct Letter { float dist; float em; vec2 px; vec2 pos; float glyph; vec4 ink; vec4 std; };
float hash12(vec2 p) { vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
)";

const char* FS_MAIN = R"(
void main() {
    float raw = (texture(uAtlas, vUv).r * 255.0 - 128.0) / uScale; // baked pixels, > 0 inside
    float dist = raw * vEm / uBake + uWeight * vEm;                  // screen pixels
    float fw = max(fwidth(dist), 1e-3);
    if (uPass == 0) {
        float grow = uShadowShape.x * vEm, soft = max(uShadowShape.y * vEm, 0.5 * fw);
        float a = smoothstep(-soft, soft, dist + grow) * uShadow.a * vInk.a;
        frag = vec4(uShadow.rgb * a, a);
        return;
    }
    float fill = clamp(dist / fw + 0.5, 0.0, 1.0);
    if (vEm < 40.0) {
        // small letters: the coverage of four points of the pixel (one point misses a stroke thinner than a pixel,
        // a light E's stem, depending on where the letter falls)
        vec2 du = dFdx(vUv) * 0.25, dv = dFdy(vUv) * 0.25;
        float k = vEm / (uBake * uScale), w = 2.0 / fw;
        fill = 0.0;
        for (int i = 0; i < 4; i++) {
            vec2 o = (i == 0 ? du + dv : i == 1 ? du - dv : i == 2 ? -du + dv : -du - dv);
            float d = (texture(uAtlas, vUv + o).r * 255.0 - 128.0) * k + uWeight * vEm;
            fill += 0.25 * clamp(d * w + 0.5, 0.0, 1.0);
        }
    }
    vec4 c = vec4(0.0);
    if (uHalo.a > 0.0) {
        float h = clamp(1.0 + dist / max(uHaloShape.x * vEm, 1e-3), 0.0, 1.0);
        h = (uHaloShape.z > 0.5 ? smoothstep(0.0, 1.0, h) : pow(h, uHaloShape.y)) * uHalo.a;
        c = vec4(uHalo.rgb * h, h);
    }
    if (uOutline.a > 0.0) {
        float o = clamp((dist + uOutlineW * vEm) / fw + 0.5, 0.0, 1.0) * uOutline.a;
        c = vec4(uOutline.rgb * o, o) + c * (1.0 - o);
    }
    c = vec4(vInk.rgb * fill, fill) + c * (1.0 - fill);
    if (uGlow.a > 0.0) {
        float g = clamp(1.0 + dist / max(uGlowW * vEm, 1e-3), 0.0, 1.0);
        c.rgb += uGlow.rgb * (g * g * uGlow.a * (1.0 - fill));
    }
    // a line fades as a whole: its ink stays over its halo
    c *= vInk.a;
    frag = letter(Letter(dist, vEm, vec2(gl_FragCoord.x, uRes.y - gl_FragCoord.y), vLine, vGlyph, vInk, c));
}
)";

const char* STD_LETTER = "vec4 letter(Letter L) { return L.std; }\n";

const char* PLATE_FS = R"(#version 330 core
in vec2 vUv;
out vec4 frag;
uniform vec2 uRes;
uniform vec4 uBox;      // pixels: x0, y0, x1, y1 (top-left origin)
uniform float uRadius, uBorderW;
uniform vec4 uColor, uBorder;
void main() {
    vec2 p = vec2(vUv.x * uRes.x, (1.0 - vUv.y) * uRes.y);
    vec2 c = (uBox.xy + uBox.zw) * 0.5, e = (uBox.zw - uBox.xy) * 0.5;
    float r = min(uRadius, min(e.x, e.y));
    vec2 q = abs(p - c) - e + r;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r; // < 0 inside
    float a = clamp(0.5 - d, 0.0, 1.0);
    float b = uBorder.a * clamp(0.5 - abs(d + uBorderW * 0.5) + uBorderW * 0.5, 0.0, 1.0);
    vec4 col = vec4(uColor.rgb, 1.0) * uColor.a * a;
    col = vec4(uBorder.rgb, 1.0) * b + col * (1.0 - b);
    frag = col;
}
)";

} // namespace

// ---- Fonts

class Font {
public:
    struct Glyph {
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // atlas
        float ox = 0, oy = 0, w = 0, h = 0;   // baked pixels, from the pen on the baseline
        float adv = 0;
        bool ink = false;
    };
    bool ok = false;
    float capH = 0.7f * BAKE_PX, descent = 0.2f * BAKE_PX;
    GLuint tex = 0;

    bool init(const unsigned char* ttf) {
        if (!stbtt_InitFont(&info_, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) return false;
        scale_ = stbtt_ScaleForPixelHeight(&info_, BAKE_PX);
        int x0, y0, x1, y1;
        if (stbtt_GetCodepointBox(&info_, 'H', &x0, &y0, &x1, &y1)) capH = y1 * scale_;
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&info_, &asc, &desc, &gap);
        descent = -desc * scale_;
        ok = true;
        // the usual letters' fields, made ahead on a worker: one takes up to 20 ms, a hitch when made in a frame (the
        // score's next digit, a section's name); they go into the atlas, in the same order, as they are first drawn
        aheadJob_ = std::async(std::launch::async, [this] {
            std::unordered_map<uint32_t, Field> out;
            for (uint32_t cp = 0x21; cp <= 0xb7; cp = cp == 0x7e ? 0xb7 : cp + 1)
                if (stbtt_FindGlyphIndex(&info_, (int)cp)) out[cp] = field(cp);
            return out;
        });
        return true;
    }
    const Glyph& glyph(uint32_t cp) {
        auto it = glyphs_.find(cp);
        if (it != glyphs_.end()) return it->second;
        return glyphs_[cp] = bake(cp);
    }
    float kern(uint32_t a, uint32_t b) { return a && b && !raster_ ? stbtt_GetCodepointKernAdvance(&info_, a, b) * scale_ : 0.f; }
    void initCustom(float cap, float desc, Raster r) {
        capH = cap * BAKE_PX;
        descent = desc * BAKE_PX;
        raster_ = std::move(r);
        ok = true;
    }
    ~Font() {
        if (tex) glDeleteTextures(1, &tex);
    }

private:
    stbtt_fontinfo info_{};
    float scale_ = 1;
    Raster raster_; // a custom font's letters
    std::unordered_map<uint32_t, Glyph> glyphs_;
    int x_ = 1, y_ = 1, rowH_ = 0;
    // a letter's distance field, before it goes into the atlas
    struct Field {
        std::vector<unsigned char> px;
        int w = 0, h = 0, xo = 0, yo = 0;
    };
    std::future<std::unordered_map<uint32_t, Field>> aheadJob_;
    std::unordered_map<uint32_t, Field> ahead_;

    Field field(uint32_t cp) const {
        Field f;
        unsigned char* sdf = stbtt_GetCodepointSDF(&info_, scale_, (int)cp, PAD, 128, DIST_SCALE, &f.w, &f.h, &f.xo, &f.yo);
        if (sdf) f.px.assign(sdf, sdf + (size_t)f.w * f.h);
        stbtt_FreeSDF(sdf, nullptr);
        return f;
    }

    // A custom letter: its coverage, padded, turned into a distance field (exact Euclidean distances to the nearest
    // covered / uncovered pixel, by two passes of the 1D squared distance transform).
    unsigned char* customField(uint32_t cp, int& w, int& h, int& xo, int& yo, float& adv) {
        Bitmap b;
        if (!raster_(cp, BAKE_PX, b) || b.w < 0 || b.h < 0) return nullptr;
        adv = b.adv;
        w = b.w + 2 * PAD;
        h = b.h + 2 * PAD;
        xo = (int)std::lround(b.ox) - PAD;
        yo = (int)std::lround(b.oy) - PAD;
        auto edt = [&](bool inside) {
            const double INF = 1e20;
            std::vector<double> f((size_t)w * h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    const int bx = x - PAD, by = y - PAD;
                    const bool on = bx >= 0 && by >= 0 && bx < b.w && by < b.h && b.cov[(size_t)by * b.w + bx] >= 128;
                    f[(size_t)y * w + x] = (on == inside) ? 0.f : INF;
                }
            auto pass = [](std::vector<double>& g, int n, int stride, int count, int step) {
                std::vector<double> d(n), z(n + 1), src(n);
                std::vector<int> v(n);
                for (int c = 0; c < count; c++) {
                    for (int i = 0; i < n; i++) src[i] = g[(size_t)c * step + (size_t)i * stride];
                    int k = 0;
                    v[0] = 0; z[0] = -1e20f; z[1] = 1e20f;
                    for (int q = 1; q < n; q++) {
                        double sp;
                        while (true) {
                            sp = ((src[q] + (double)q * q) - (src[v[k]] + (double)v[k] * v[k])) / (2.0 * q - 2.0 * v[k]);
                            if (sp <= z[k] && k > 0) k--; else break;
                        }
                        if (sp <= z[k]) { v[0] = q; z[0] = -1e20f; z[1] = 1e20f; k = 0; continue; }
                        k++; v[k] = q; z[k] = sp; z[k + 1] = 1e20f;
                    }
                    k = 0;
                    for (int q = 0; q < n; q++) {
                        while (z[k + 1] < q) k++;
                        d[q] = (double)(q - v[k]) * (q - v[k]) + src[v[k]];
                    }
                    for (int i = 0; i < n; i++) g[(size_t)c * step + (size_t)i * stride] = d[i];
                }
            };
            pass(f, w, 1, h, w);  // rows
            pass(f, h, w, w, 1);  // columns
            return f;
        };
        const std::vector<double> out = edt(true), in = edt(false); // to the nearest covered / uncovered pixel
        unsigned char* field = (unsigned char*)std::malloc((size_t)w * h);
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const float d = in[i] > 0.0 ? (float)std::sqrt(in[i]) - 0.5f : -((float)std::sqrt(out[i]) - 0.5f); // > 0 inside
            field[i] = (unsigned char)std::clamp(128.f + d * DIST_SCALE, 0.f, 255.f);
        }
        return field;
    }

    Glyph bake(uint32_t cp) {
        if (raster_) {
            Glyph g;
            int w = 0, h = 0, xo = 0, yo = 0;
            unsigned char* field = customField(cp, w, h, xo, yo, g.adv);
            if (!field) return cp == '?' || cp == ' ' ? g : glyph(cp >= 'a' && cp <= 'z' ? cp - 32 : '?');
            place(g, field, w, h, xo, yo);
            std::free(field);
            return g;
        }
        // a letter the font lacks: its unaccented form, else a question mark
        if (cp != ' ' && !stbtt_FindGlyphIndex(&info_, (int)cp)) {
            std::string s;
            appendUtf8(s, cp);
            const std::string folded = asciiFold(s);
            const uint32_t alt = !folded.empty() && (unsigned char)folded[0] < 0x80 && folded != s ? (uint32_t)folded[0] : '?';
            return glyph(alt == cp ? '?' : alt);
        }
        Glyph g;
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&info_, (int)cp, &adv, &lsb);
        g.adv = adv * scale_;
        if (aheadJob_.valid() && aheadJob_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) ahead_ = aheadJob_.get();
        auto a = ahead_.find(cp);
        const Field f = a != ahead_.end() ? std::move(a->second) : field(cp); // (made here while the worker is not done)
        if (f.px.empty()) return g;
        place(g, f.px.data(), f.w, f.h, f.xo, f.yo);
        return g;
    }

    // a letter's field into the atlas
    void place(Glyph& g, const unsigned char* field, int w, int h, int xo, int yo) {
        if (!tex) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            std::vector<unsigned char> zero((size_t)ATLAS * ATLAS, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS, ATLAS, 0, GL_RED, GL_UNSIGNED_BYTE, zero.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        if (x_ + w + 1 > ATLAS) { x_ = 1; y_ += rowH_ + 1; rowH_ = 0; }
        if (y_ + h + 1 > ATLAS) return; // full (hundreds of distinct letters): the rest shows as spaces
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x_, y_, w, h, GL_RED, GL_UNSIGNED_BYTE, field);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        g.u0 = x_ / (float)ATLAS; g.v0 = y_ / (float)ATLAS; g.u1 = (x_ + w) / (float)ATLAS; g.v1 = (y_ + h) / (float)ATLAS;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        g.ink = true;
        x_ += w + 1;
        rowH_ = std::max(rowH_, h);
    }
};

Font* font(const unsigned char* ttf, size_t size) {
    (void)size;
    static std::map<const unsigned char*, std::unique_ptr<Font>> fonts;
    auto it = fonts.find(ttf);
    if (it != fonts.end()) return it->second.get();
    auto f = std::make_unique<Font>();
    if (!f->init(ttf)) {
        std::fprintf(stderr, "[text] a font could not be read\n");
        fonts[ttf] = nullptr;
        return nullptr;
    }
    return (fonts[ttf] = std::move(f)).get();
}

float capHeight(const Font* f) { return f ? f->capH / BAKE_PX : 0.7f; }

Font* customFont(const std::string& key, float cap, float descent, Raster raster) {
    static std::map<std::string, std::unique_ptr<Font>> fonts;
    auto it = fonts.find(key);
    if (it != fonts.end()) return it->second.get();
    auto f = std::make_unique<Font>();
    f->initCustom(cap, descent, std::move(raster));
    return (fonts[key] = std::move(f)).get();
}

// ---- Lines

Span& Line::add(const std::string& t, Font* f, float size, const vec3& ink, float track, float a) {
    Span s;
    s.text = t;
    s.font = f ? f : (spans.empty() ? nullptr : spans.back().font);
    s.size = size;
    s.ink = ink;
    s.track = track;
    s.alpha = a;
    spans.push_back(s);
    return spans.back();
}

std::string upper(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size();) appendUtf8(o, toUpper(nextCodepoint(s, i)));
    return o;
}
std::string lower(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size();) appendUtf8(o, toLower(nextCodepoint(s, i)));
    return o;
}
std::string titleCase(const std::string& s) {
    std::string o;
    bool start = true;
    for (size_t i = 0; i < s.size();) {
        const uint32_t c = nextCodepoint(s, i);
        appendUtf8(o, start ? toUpper(c) : toLower(c));
        start = !isLetter(c) && c != '\'' && c != 0x2019;
    }
    return o;
}
std::string thousands(long long n) {
    std::string d = std::to_string(n < 0 ? -n : n), o;
    for (size_t i = 0; i < d.size(); i++) {
        if (i && (d.size() - i) % 3 == 0) o += ',';
        o += d[i];
    }
    return n < 0 ? "-" + o : o;
}
void splitUniverse(const std::string& s, std::string& name, std::string& variant) {
    const size_t dot = s.find("\xC2\xB7");
    auto trim = [](std::string t) {
        while (!t.empty() && t.back() == ' ') t.pop_back();
        size_t a = 0;
        while (a < t.size() && t[a] == ' ') a++;
        return t.substr(a);
    };
    if (dot == std::string::npos) { name = trim(s); variant.clear(); return; }
    name = trim(s.substr(0, dot));
    variant = trim(s.substr(dot + 2));
}

namespace {

// The quads of a line (12 floats a vertex), from its anchor at (x, y); its box.
Box layoutLine(const Line& l, float x, float y, std::vector<float>* out) {
    Box b;
    x += l.dx;
    y += l.dy;
    if (l.spans.empty() || !l.spans[0].font) return b;
    const Span& first = l.spans[0];
    b.cap = first.font->capH * first.size / BAKE_PX;
    const float baseline = y + b.cap;
    const float em0 = first.size;
    b.x0 = x;
    b.y0 = y;
    b.y1 = baseline;
    float pen = x;
    int index = 0;
    for (const Span& s : l.spans) {
        Font* f = s.font ? s.font : first.font;
        if (!f) continue;
        const float k = s.size / BAKE_PX, base = baseline + s.dy;
        b.y1 = std::max(b.y1, base + f->descent * k * 0.5f);
        pen += s.gap * s.size;
        uint32_t prev = 0;
        for (size_t i = 0; i < s.text.size();) {
            const uint32_t cp = nextCodepoint(s.text, i);
            if (cp == '\n' || cp == '\r') continue;
            pen += f->kern(prev, cp) * k;
            prev = cp;
            const Font::Glyph& g = f->glyph(cp);
            if (g.ink && out) {
                const float x0 = pen + g.ox * k, y0 = base + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
                const float sl = l.look.slant;
                const float pts[4][4] = {{x0, y0, g.u0, g.v0}, {x1, y0, g.u1, g.v0}, {x1, y1, g.u1, g.v1}, {x0, y1, g.u0, g.v1}};
                const int idx[6] = {0, 1, 2, 0, 2, 3};
                for (int q : idx) {
                    const float px = pts[q][0] + sl * (base - pts[q][1]), py = pts[q][1];
                    const float v[12] = {px, py, pts[q][2], pts[q][3], s.size, (float)index, (px - x) / em0, (py - y) / em0,
                                         s.ink.x, s.ink.y, s.ink.z, s.alpha * l.alpha};
                    out->insert(out->end(), v, v + 12);
                }
            }
            pen += g.adv * k + s.track * s.size;
            index++;
        }
        if (!s.text.empty()) pen -= s.track * s.size; // no space after a span's last letter
        pen += s.track * s.size * (&s != &l.spans.back() ? 1.f : 0.f);
    }
    b.x1 = pen;
    return b;
}

} // namespace

Engine::~Engine() {
    for (auto& [k, p] : progs_)
        if (p.prog) glDeleteProgram(p.prog);
    if (plateProg_) glDeleteProgram(plateProg_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

bool Engine::init() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 12 * sizeof(float);
    const int sizes[5] = {2, 2, 2, 2, 4};
    size_t off = 0;
    for (int i = 0; i < 5; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, stride, (void*)(off * sizeof(float)));
        off += sizes[i];
    }
    glBindVertexArray(0);
    plateProg_ = ugl::program(ugl::FULLSCREEN_VS, PLATE_FS, "text plate");
    return plateProg_ != 0 && prog(nullptr).prog != 0;
}

const Engine::Prog& Engine::prog(const Lettering* l) {
    const std::string key = l ? l->glsl + "\n//move\n" + l->move : std::string();
    auto it = progs_.find(key);
    if (it != progs_.end()) return it->second;
    Prog p;
    std::string vs = VS;
    if (l && !l->move.empty()) {
        // the lettering's move() (and the helpers) in the vertex shader
        p.move = true;
        const char* helpers = R"(float hash12(vec2 p) { vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), f.x), mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), f.x), f.y);
}
)";
        vs.replace(vs.find("//MOVE"), 6, std::string("#define MOVE\n") + helpers + l->move);
    }
    std::string fs = std::string(FS_HEAD) + (l && !l->glsl.empty() ? l->glsl : std::string(STD_LETTER)) + FS_MAIN;
    p.prog = ugl::program(vs.c_str(), fs.c_str(), "text");
    return progs_[key] = p;
}

Box Engine::measure(const Line& l) const { return layoutLine(l, 0.f, 0.f, nullptr); }
Box measure(const Line& l) { return layoutLine(l, 0.f, 0.f, nullptr); }

void Engine::draw(const Line& l, float x, float y, int w, int h, float time) {
    if (l.alpha <= 0.002f || l.spans.empty() || !l.spans[0].font) return;
    // (the vertices of all spans; the atlas is per font, so a line with several fonts draws one batch per font)
    const Look& k = l.look;
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);
    // the plate
    if (l.plate.alpha > 0.002f || l.plate.borderA > 0.002f) {
        const Box b = layoutLine(l, x, y, nullptr);
        const float em = l.spans[0].size;
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(plateProg_);
        ugl::set2f(plateProg_, "uRes", (float)w, (float)h);
        ugl::set4f(plateProg_, "uBox", b.x0 - l.plate.padX * em, b.y0 - l.plate.padY * em, b.x1 + l.plate.padX * em,
                   b.y0 + b.cap + l.plate.padY * em);
        ugl::set1f(plateProg_, "uRadius", l.plate.radius * em);
        ugl::set1f(plateProg_, "uBorderW", l.plate.borderW * em);
        ugl::set4f(plateProg_, "uColor", l.plate.color.x, l.plate.color.y, l.plate.color.z, l.plate.alpha * l.alpha);
        ugl::set4f(plateProg_, "uBorder", l.plate.border.x, l.plate.border.y, l.plate.border.z, l.plate.borderA * l.alpha);
        ugl::drawFullscreen();
    }
    const Prog& p = prog(l.lettering);
    if (!p.prog) return;
    glUseProgram(p.prog);
    ugl::set2f(p.prog, "uRes", (float)w, (float)h);
    ugl::set1f(p.prog, "uTime", time);
    const vec4 p0 = l.lettering ? l.lettering->p0 : vec4{0, 0, 0, 0}, p1 = l.lettering ? l.lettering->p1 : vec4{0, 0, 0, 0};
    ugl::set4f(p.prog, "uP0", p0.x, p0.y, p0.z, p0.w);
    ugl::set4f(p.prog, "uP1", p1.x, p1.y, p1.z, p1.w);
    ugl::set1i(p.prog, "uAtlas", 0);
    ugl::set1f(p.prog, "uBake", BAKE_PX);
    ugl::set1f(p.prog, "uScale", DIST_SCALE);
    ugl::set1f(p.prog, "uWeight", k.weight);
    const bool fx = l.blend == BLEND_OVER || l.blend == BLEND_SCREEN || l.blend == BLEND_ADD;
    ugl::set4f(p.prog, "uHalo", k.halo.x, k.halo.y, k.halo.z, fx ? k.haloA : 0.f);
    glUniform3f(ugl::U(p.prog, "uHaloShape"), std::min(k.haloW, PAD / BAKE_PX), std::max(0.1f, k.haloPow), k.haloSmooth ? 1.f : 0.f);
    ugl::set4f(p.prog, "uGlow", k.glow.x, k.glow.y, k.glow.z, fx ? k.glowA : 0.f);
    ugl::set1f(p.prog, "uGlowW", std::min(k.glowW, PAD / BAKE_PX));
    ugl::set4f(p.prog, "uOutline", k.outline.x, k.outline.y, k.outline.z, k.outlineA);
    ugl::set1f(p.prog, "uOutlineW", std::min(k.outlineW, PAD / BAKE_PX * 0.9f));
    ugl::set4f(p.prog, "uShadow", k.shadow.x, k.shadow.y, k.shadow.z, k.shadowA);
    ugl::set2f(p.prog, "uShadowShape", std::min(k.shadowW, PAD / BAKE_PX * 0.6f), std::min(k.shadowSoft, PAD / BAKE_PX * 0.4f));
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    // one batch per font (lines rarely mix more than two)
    std::vector<Font*> fonts;
    for (const Span& s : l.spans)
        if (s.font && std::find(fonts.begin(), fonts.end(), s.font) == fonts.end()) fonts.push_back(s.font);
    std::vector<float> all;
    layoutLine(l, x, y, &all); // bakes what's missing first
    for (int pass = (fx && k.shadowA > 0.002f) ? 0 : 1; pass < 2; pass++) {
        ugl::set1i(p.prog, "uPass", pass);
        ugl::set2f(p.prog, "uShift", pass == 0 ? k.shadowOff.x : 0.f, pass == 0 ? k.shadowOff.y : 0.f);
        switch (pass == 0 ? BLEND_OVER : l.blend) {
        case BLEND_OVER: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;
        case BLEND_MULTIPLY: glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA); break;
        case BLEND_ADD: glBlendFunc(GL_ONE, GL_ONE); break;
        case BLEND_SCREEN: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR); break;
        }
        for (Font* f : fonts) {
            // this font's letters: a line's spans laid out again with the others' text kept as spacing only
            Line part = l;
            for (Span& s : part.spans)
                if ((s.font ? s.font : l.spans[0].font) != f) s.alpha = -1.f; // marked: skipped below
            verts_.clear();
            layoutLine(part, x, y, &verts_);
            std::vector<float> keep;
            keep.reserve(verts_.size());
            for (size_t v = 0; v + 12 <= verts_.size(); v += 12)
                if (verts_[v + 11] >= 0.f) keep.insert(keep.end(), verts_.begin() + v, verts_.begin() + v + 12);
            if (keep.empty()) continue;
            glBindTexture(GL_TEXTURE_2D, f->tex);
            glBufferData(GL_ARRAY_BUFFER, keep.size() * sizeof(float), keep.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(keep.size() / 12));
        }
    }
    glBindVertexArray(0);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace text

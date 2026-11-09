#include "titlecard.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ugl.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

namespace {

// Cormorant Garamond Italic and Tenor Sans, SIL Open Font License, see src/embed/OFL-*.txt.
const unsigned char SERIF_TTF[] = {
#include "universes/host/CormorantGaramond-Italic.ttf.inc"
};
const unsigned char SANS_TTF[] = {
#include "universes/host/TenorSans-Regular.ttf.inc"
};

const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in float aAlpha;
uniform vec2 uRes;
out vec2 vUv;
out float vAlpha;
void main() {
    vUv = aUv;
    vAlpha = aAlpha;
    gl_Position = vec4(aPos.x / uRes.x * 2.0 - 1.0, 1.0 - aPos.y / uRes.y * 2.0, 0.0, 1.0);
}
)";
const char* FS = R"(#version 330 core
in vec2 vUv;
in float vAlpha;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uInk, uHalo;
void main() {
    float d = texture(uFont, vUv).r;
    float w = max(fwidth(d), 1e-3) * 0.7;
    float fill = smoothstep(0.5 - w, 0.5 + w, d);
    float halo = smoothstep(0.1, 0.5, d) * 0.45;
    float a = max(fill, halo) * vAlpha;
    vec3 c = mix(uHalo, uInk, fill / max(max(fill, halo), 1e-3));
    frag = vec4(c * a, a);
}
)";

int glyphIndex(unsigned char ch) { return ch < 32 || ch >= 128 ? '?' - 32 : ch - 32; }

} // namespace

TitleCard::~TitleCard() {
    for (Font& f : fonts_)
        if (f.tex) glDeleteTextures(1, &f.tex);
    if (prog_) glDeleteProgram(prog_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
}

bool TitleCard::init() {
    prog_ = ugl::program(VS, FS, "title card");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
    return build(fonts_[SERIF_ITALIC], SERIF_TTF) && build(fonts_[SANS], SANS_TTF);
}

// A signed distance field atlas of printable ASCII, large enough to stay sharp at the card's big size.
bool TitleCard::build(Font& f, const unsigned char* ttf) {
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        std::fprintf(stderr, "[gl] title card font init failed\n");
        return false;
    }
    f.px = 96.f;
    const float scale = stbtt_ScaleForPixelHeight(&info, f.px);
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1)) f.capH = y1 * scale;
    const int AW = 1024, AH = 1024, PAD = 10;
    std::vector<unsigned char> atlas(AW * AH, 0);
    int x = 1, y = 1, rowH = 0;
    for (int ch = 32; ch < 128; ch++) {
        Glyph& g = f.g[ch - 32];
        int adv, lsb;
        const int cp = ch == 127 ? 0xB7 : ch; // 127: the middle dot of "UNIVERSE \xB7 VARIANT"
        stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
        g.adv = adv * scale;
        int w = 0, h = 0, xo = 0, yo = 0;
        unsigned char* sdf = stbtt_GetCodepointSDF(&info, scale, cp, PAD, 128, 128.f / PAD, &w, &h, &xo, &yo);
        if (!sdf) continue;
        if (x + w + 1 > AW) { x = 1; y += rowH + 1; rowH = 0; }
        if (y + h + 1 > AH) { stbtt_FreeSDF(sdf, nullptr); break; }
        for (int r = 0; r < h; r++) std::memcpy(&atlas[(y + r) * AW + x], sdf + r * w, w);
        g.x0 = x / (float)AW; g.y0 = y / (float)AH; g.x1 = (x + w) / (float)AW; g.y1 = (y + h) / (float)AH;
        g.ox = (float)xo; g.oy = (float)yo; g.w = (float)w; g.h = (float)h;
        x += w + 1;
        rowH = std::max(rowH, h);
        stbtt_FreeSDF(sdf, nullptr);
    }
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, AW, AH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f.ok = true;
    return true;
}

float TitleCard::width(const Line& l) const {
    const Font& f = fonts_[l.face];
    if (!f.ok || l.text.empty()) return 0.f;
    const float k = l.cap / f.capH;
    float w = 0;
    for (unsigned char c : l.text) w += f.g[glyphIndex(c)].adv * k + l.track * l.cap;
    return w - l.track * l.cap;
}

void TitleCard::draw(const Line* lines, int n, int screenW, int screenH, const vec3& ink, const vec3& halo) {
    std::vector<float> v;
    for (int i = 0; i < n; i++) {
        const Line& l = lines[i];
        const Font& f = fonts_[l.face];
        if (!f.ok || l.alpha <= 0.002f || l.text.empty()) continue;
        const float k = l.cap / f.capH;
        float pen = l.center ? l.x - width(l) * 0.5f : l.x;
        const float base = l.y + l.cap;
        for (unsigned char c : l.text) {
            const Glyph& g = f.g[glyphIndex(c)];
            if (g.w > 0) {
                const float x0 = pen + g.ox * k, y0 = base + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
                const float q[6][5] = {{x0, y0, g.x0, g.y0, l.alpha}, {x1, y0, g.x1, g.y0, l.alpha}, {x1, y1, g.x1, g.y1, l.alpha},
                                       {x0, y0, g.x0, g.y0, l.alpha}, {x1, y1, g.x1, g.y1, l.alpha}, {x0, y1, g.x0, g.y1, l.alpha}};
                for (auto& p : q) v.insert(v.end(), p, p + 5);
            }
            pen += g.adv * k + l.track * l.cap;
        }
        // Each face has its own atlas: draw this line now.
        if (v.empty()) continue;
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(prog_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, f.tex);
        ugl::set1i(prog_, "uFont", 0);
        ugl::set2f(prog_, "uRes", (float)screenW, (float)screenH);
        ugl::set3f(prog_, "uInk", ink);
        ugl::set3f(prog_, "uHalo", halo);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 5));
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        v.clear();
    }
}

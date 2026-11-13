#include "titles.hpp"

#include <cctype>
#include <cstdint>
#include <vector>

namespace {

std::string lowerAscii(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool hasWord(const std::string& s, const char* w) {
    const std::string l = lowerAscii(s), word = w;
    for (size_t at = l.find(word); at != std::string::npos; at = l.find(word, at + 1)) {
        const bool start = at == 0 || !std::isalnum((unsigned char)l[at - 1]);
        const size_t end = at + word.size();
        if (start && (end >= l.size() || !std::isalnum((unsigned char)l[end]))) return true;
    }
    return false;
}

// An upload's label rather than part of the song's name.
bool isLabel(const std::string& inner) {
    // what names the version stays
    for (const char* keep : {"feat", "ft", "remix", "edit", "version", "rework", "bootleg", "cover", "acoustic", "live"})
        if (hasWord(inner, keep)) return false;
    for (const char* w : {"official", "video", "visualizer", "visualiser", "lyric", "lyrics", "audio", "clip", "hd",
                          "4k", "hq", "mv", "original mix", "extended mix", "radio edit", "out now", "premiere"})
        if (hasWord(inner, w)) return true;
    return false;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '-' || s.back() == '|')) s.pop_back();
    size_t a = 0;
    while (a < s.size() && s[a] == ' ') a++;
    return s.substr(a);
}

} // namespace

std::string displayTitle(const std::string& s) {
    // the letters the HUD can draw
    std::string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        const int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        uint32_t cp = c;
        if (len == 2 && i + 1 < s.size()) cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
        else if (len == 3 && i + 2 < s.size()) cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
        else if (len == 4) cp = 0x10000;
        // Latin letters, accents and punctuation; general punctuation (dashes, quotes, ellipsis); the superscripts
        const bool keep = (cp >= 0x20 && cp < 0x7F) || (cp >= 0xA0 && cp < 0x250) || (cp >= 0x2010 && cp <= 0x2027) ||
                          (cp >= 0x2070 && cp <= 0x209F);
        if (keep) out += s.substr(i, len);
        i += len;
    }
    // the bracketed labels: "(Official Video)", "[LYRIC VIDEO]"...
    std::string t;
    for (size_t i = 0; i < out.size(); i++) {
        const char open = out[i];
        if (open == '(' || open == '[') {
            const size_t close = out.find(open == '(' ? ')' : ']', i + 1);
            if (close != std::string::npos && isLabel(out.substr(i + 1, close - i - 1))) {
                i = close;
                continue;
            }
        }
        t += open;
    }
    // a trailing " - Official Video", " | Official Music Video"
    for (const char* sep : {" | ", " - ", " -- "}) {
        const size_t at = t.rfind(sep);
        if (at != std::string::npos && at > 0 && isLabel(t.substr(at + std::string(sep).size()))) t = t.substr(0, at);
    }
    // spaces collapsed, the ends trimmed
    std::string clean;
    for (char ch : t)
        if (!(ch == ' ' && (clean.empty() || clean.back() == ' '))) clean += ch;
    clean = trim(clean);
    // "Artist - 'Title'": the title unquoted
    const size_t dash = clean.find(" - ");
    if (dash != std::string::npos) {
        std::string title = clean.substr(dash + 3);
        for (const char* q : {"'", "\"", "\xE2\x80\x98", "\xE2\x80\x9C"}) {
            const std::string open = q, close = open == "\xE2\x80\x98" ? "\xE2\x80\x99" : open == "\xE2\x80\x9C" ? "\xE2\x80\x9D" : open;
            if (title.size() > open.size() + close.size() && title.compare(0, open.size(), open) == 0) {
                const size_t end = title.find(close, open.size());
                if (end != std::string::npos) {
                    title = title.substr(open.size(), end - open.size()) + title.substr(end + close.size());
                    clean = clean.substr(0, dash + 3) + trim(title);
                }
                break;
            }
        }
    }
    return clean;
}

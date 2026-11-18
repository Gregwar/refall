#include "platform.hpp"

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace platform {

fs::path executableDir() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n > 0) return fs::path(std::wstring(buf, n)).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) return fs::canonical(buf, ec).parent_path();
#else
    fs::path p = fs::canonical("/proc/self/exe", ec);
    if (!ec) return p.parent_path();
#endif
    return fs::current_path(ec);
}

fs::path homeDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("USERPROFILE")) return p;
#endif
    if (const char* p = std::getenv("HOME")) return p;
    return ".";
}

fs::path cacheDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("LOCALAPPDATA")) return p;
    return homeDir() / "AppData" / "Local";
#elif defined(__APPLE__)
    return homeDir() / "Library" / "Caches";
#else
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    return xdg && *xdg ? fs::path(xdg) : homeDir() / ".cache";
#endif
}

fs::path dataDir() {
#if defined(_WIN32)
    if (const char* p = std::getenv("APPDATA")) return p;
    return homeDir() / "AppData" / "Roaming";
#elif defined(__APPLE__)
    return homeDir() / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    return xdg && *xdg ? fs::path(xdg) : homeDir() / ".local" / "share";
#endif
}

// <root>/refall, taking over <root>/zentris when only that one exists
static fs::path appDir(const fs::path& root) {
    const fs::path dir = root / "refall", old = root / "zentris";
    std::error_code ec;
    if (!fs::exists(dir, ec) && fs::is_directory(old, ec)) fs::rename(old, dir, ec);
    return dir;
}

fs::path appCacheDir() {
    static const fs::path dir = appDir(cacheDir());
    return dir;
}

fs::path appDataDir() {
    static const fs::path dir = appDir(dataDir());
    return dir;
}

std::string findExecutable(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (!path) return "";
#if defined(_WIN32)
    const char sep = ';';
    const std::vector<std::string> exts = {".exe", ".cmd", ".bat", ""};
#else
    const char sep = ':';
    const std::vector<std::string> exts = {""};
#endif
    std::string p(path);
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find(sep, start);
        if (end == std::string::npos) end = p.size();
        std::string dir = p.substr(start, end - start);
        if (!dir.empty()) {
            for (const auto& ext : exts) {
                std::error_code ec;
                fs::path cand = fs::path(dir) / (name + ext);
                if (fs::is_regular_file(cand, ec)) return cand.string();
            }
        }
        start = end + 1;
    }
    return "";
}

std::string quoteArg(const std::string& arg) {
#if defined(_WIN32)
    std::string r = "\"";
    for (char c : arg) r += c == '"' ? std::string("\\\"") : std::string(1, c);
    return r + "\"";
#else
    std::string r = "'";
    for (char c : arg) r += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return r + "'";
#endif
}

#if defined(_WIN32)
// cmd.exe strips one pair of outer quotes: wrap the whole line so quoted arguments survive.
static std::string wrap(const std::string& cmd) { return "\"" + cmd + "\""; }
static const char* NUL_OUT = " >NUL 2>&1";
static const char* NUL_ERR = " 2>NUL";
#else
static std::string wrap(const std::string& cmd) { return cmd; }
static const char* NUL_OUT = " >/dev/null 2>&1";
static const char* NUL_ERR = " 2>/dev/null";
#endif

int run(const std::string& cmd) { return std::system(wrap(cmd + NUL_OUT).c_str()); }

FILE* openRead(const std::string& cmd) {
#if defined(_WIN32)
    return _popen(wrap(cmd + NUL_ERR).c_str(), "r");
#else
    return popen((cmd + NUL_ERR).c_str(), "r");
#endif
}

FILE* openWrite(const std::string& cmd) {
#if defined(_WIN32)
    return _popen(wrap(cmd).c_str(), "wb");
#else
    return popen(cmd.c_str(), "w");
#endif
}

int closeRead(FILE* f) {
#if defined(_WIN32)
    return _pclose(f);
#else
    return pclose(f);
#endif
}

void lineBufferStdout() {
#if defined(_WIN32)
    std::setvbuf(stdout, nullptr, _IONBF, 0); // MSVC has no line buffering (and rejects size 0)
#else
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
#endif
}

#if defined(REFALL_STANDALONE) && defined(_WIN32)
void standaloneStart() {
    // A window program has no console: give it a hidden one, else every tool it runs (yt-dlp, ffmpeg) would open its
    // own console window, flashing over the game.
    if (!GetConsoleWindow() && AllocConsole()) ShowWindow(GetConsoleWindow(), SW_HIDE);
    std::error_code ec;
    fs::create_directories(appCacheDir(), ec);
    const std::wstring log = (appCacheDir() / "refall.log").wstring();
    _wfreopen(log.c_str(), L"w", stdout);
    _wfreopen(log.c_str(), L"a", stderr);
}

namespace {
// Windows 10 and later ship curl and tar (bsdtar, which also reads zip files).
std::string systemTool(const char* name) {
    const char* root = std::getenv("SystemRoot");
    return quoteArg((fs::path(root ? root : "C:\\Windows") / "System32" / name).string());
}

bool fetch(const std::string& url, const fs::path& to) {
    std::printf("[tools] downloading %s\n", url.c_str());
    std::fflush(stdout);
    std::error_code ec;
    const fs::path part = to.string() + ".part";
    fs::remove(part, ec);
    const int rc = run(systemTool("curl.exe") + " -L --fail --retry 2 -s -o " + quoteArg(part.string()) + " " + quoteArg(url));
    if (rc != 0 || !fs::exists(part, ec)) {
        std::printf("[tools] download failed (%d): %s\n", rc, url.c_str());
        fs::remove(part, ec);
        return false;
    }
    fs::rename(part, to, ec);
    return !ec;
}

// One file out of a zip (or the whole zip when `member` is empty) into `dir`.
bool unzip(const fs::path& zip, const fs::path& dir, const std::string& member = "") {
    const int rc = run(systemTool("tar.exe") + " -xf " + quoteArg(zip.string()) + " -C " + quoteArg(dir.string()) +
                       (member.empty() ? "" : " " + quoteArg(member)));
    std::error_code ec;
    fs::remove(zip, ec);
    return rc == 0;
}
} // namespace

void ensureTools() {
    static std::once_flag once;
    std::call_once(once, [] {
        const fs::path dir = appCacheDir() / "tools";
        std::error_code ec;
        fs::create_directories(dir, ec);
        const char* path = std::getenv("PATH");
        const std::string newPath = dir.string() + ";" + (path ? path : "");
        _putenv_s("PATH", newPath.c_str());
        // yt-dlp: its own release (one .exe); YouTube changes often, so it updates itself once a week
        const fs::path ytdlp = dir / "yt-dlp.exe";
        if (findExecutable("yt-dlp").empty()) {
            fetch("https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", ytdlp);
        } else if (fs::exists(ytdlp, ec)) {
            const auto age = fs::file_time_type::clock::now() - fs::last_write_time(ytdlp, ec);
            if (!ec && age > std::chrono::hours(24 * 7)) {
                std::printf("[tools] updating yt-dlp\n");
                run(quoteArg(ytdlp.string()) + " -U");
                fs::last_write_time(ytdlp, fs::file_time_type::clock::now(), ec);
            }
        }
        // ffmpeg (yt-dlp converts the songs with it): the single-file build of the imageio-ffmpeg package, which the
        // Python package uses too (a fixed version: PyPI's files never change)
        if (findExecutable("ffmpeg").empty()) {
            const fs::path whl = dir / "ffmpeg.whl";
            const std::string member = "imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v9.0.2.exe";
            if (fetch("https://files.pythonhosted.org/packages/82/0c/a1d49f3dd2688bd750b4986f8e271ad11f8e729a896070ea25f587b44da8/"
                      "imageio_ffmpeg-0.8.1-py3-none-win_amd64.whl", whl) && unzip(whl, dir, member)) {
                fs::rename(dir / member, dir / "ffmpeg.exe", ec);
                fs::remove_all(dir / "imageio_ffmpeg", ec);
            }
        }
        // deno: the JavaScript runtime yt-dlp needs for YouTube (unless node is there)
        if (findExecutable("deno").empty() && findExecutable("node").empty()) {
            const fs::path zip = dir / "deno.zip";
            if (fetch("https://github.com/denoland/deno/releases/latest/download/deno-x86_64-pc-windows-msvc.zip", zip))
                unzip(zip, dir);
        }
        std::printf("[tools] yt-dlp %s, ffmpeg %s, deno %s\n", findExecutable("yt-dlp").empty() ? "missing" : "ready",
                    findExecutable("ffmpeg").empty() ? "missing" : "ready",
                    findExecutable("deno").empty() && findExecutable("node").empty() ? "missing" : "ready");
        std::fflush(stdout);
    });
}
#else
void standaloneStart() {}
void ensureTools() {}
#endif

} // namespace platform

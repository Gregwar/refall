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
#if !defined(_WIN32)
#include <unistd.h> // isatty, setenv
#endif

namespace fs = std::filesystem;

namespace platform {

fs::path executableDir() {
    std::error_code ec;
#if defined(__EMSCRIPTEN__)
    return "/refall"; // (the web build's preloaded files: the menu's music, the gamepad mappings)
#elif defined(_WIN32)
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

#if defined(__EMSCRIPTEN__)
// The web build runs no other programs.
int run(const std::string&) { return -1; }
FILE* openRead(const std::string&) { return nullptr; }
FILE* openWrite(const std::string&) { return nullptr; }
int closeRead(FILE*) { return -1; }
#else
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
#endif

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
    std::setvbuf(stderr, nullptr, _IONBF, 0); // (the game ends with _Exit, which flushes nothing)
}
#elif defined(REFALL_STANDALONE)
void standaloneStart() {
    // Started from a desktop (the AppImage, the macOS app) there is no terminal to print to: the log goes to
    // <cache>/refall/refall.log instead. Started from a terminal, it stays there.
    std::error_code ec;
    fs::create_directories(appCacheDir(), ec);
    if (!isatty(1) && !isatty(2)) {
        const std::string log = (appCacheDir() / "refall.log").string();
        if (std::freopen(log.c_str(), "w", stdout) && std::freopen(log.c_str(), "a", stderr))
            std::setvbuf(stderr, nullptr, _IONBF, 0); // (the game ends with _Exit, which flushes nothing)
    }
}
#endif

#if defined(REFALL_STANDALONE)
namespace {
#if defined(_WIN32)
// Windows 10 and later ship curl and tar (bsdtar, which also reads zip files).
std::string systemTool(const char* name) {
    const char* root = std::getenv("SystemRoot");
    return quoteArg((fs::path(root ? root : "C:\\Windows") / "System32" / name).string());
}
const char PATH_SEP = ';';
const char* EXE = ".exe";
#else
const char PATH_SEP = ':';
const char* EXE = "";
#endif

// What to download on this system: yt-dlp's own single-file build, the ffmpeg of the imageio-ffmpeg package (the one
// the Python package uses too, a fixed version: PyPI's files never change) and deno.
struct Downloads {
    const char* ytdlp;
    const char* ffmpegWheel;
    const char* ffmpegMember;
    const char* deno;
};
#define PYPI "https://files.pythonhosted.org/packages/"
#define YTDLP "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"
#define DENO "https://github.com/denoland/deno/releases/latest/download/"
const Downloads DOWNLOADS = {
#if defined(_WIN32)
    YTDLP "yt-dlp.exe",
    PYPI "82/0c/a1d49f3dd2688bd750b4986f8e271ad11f8e729a896070ea25f587b44da8/imageio_ffmpeg-0.8.1-py3-none-win_amd64.whl",
    "imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v9.0.2.exe",
    DENO "deno-x86_64-pc-windows-msvc.zip",
#elif defined(__APPLE__) && defined(__aarch64__)
    YTDLP "yt-dlp_macos",
    PYPI "2b/47/9f9b654f853598bae0850fa22bba168103e45d31b5a2a893ed474f28d54a/imageio_ffmpeg-0.8.1-py3-none-macosx_12_0_arm64.whl",
    "imageio_ffmpeg/binaries/ffmpeg-macos-aarch64-v9.0",
    DENO "deno-aarch64-apple-darwin.zip",
#elif defined(__APPLE__)
    YTDLP "yt-dlp_macos",
    PYPI "da/24/2c3a7c69453bfb8374ddbbb5007346bad4a4247bc86882f3521cd615f929/imageio_ffmpeg-0.8.1-py3-none-macosx_10_13_x86_64.whl",
    "imageio_ffmpeg/binaries/ffmpeg-macos-x86_64-v9.0.2",
    DENO "deno-x86_64-apple-darwin.zip",
#elif defined(__aarch64__)
    YTDLP "yt-dlp_linux_aarch64",
    PYPI "53/02/e139e53a71f33f728cb2ce6c1124edc29e4c008427923f01a4d37f9a1867/imageio_ffmpeg-0.8.1-py3-none-manylinux_2_28_aarch64.whl",
    "imageio_ffmpeg/binaries/ffmpeg-linux-aarch64-v9.0.2",
    DENO "deno-aarch64-unknown-linux-gnu.zip",
#else
    YTDLP "yt-dlp_linux",
    PYPI "de/0f/49292fbef69ee10ebcf97bb741602521caafcc6966d5fa1a2b868cc15655/imageio_ffmpeg-0.8.1-py3-none-manylinux_2_28_x86_64.whl",
    "imageio_ffmpeg/binaries/ffmpeg-linux-x86_64-v9.0.2",
    DENO "deno-x86_64-unknown-linux-gnu.zip",
#endif
};

void makeExecutable(const fs::path& p) {
#if !defined(_WIN32)
    std::error_code ec;
    fs::permissions(p, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
#else
    (void)p;
#endif
}

bool fetch(const std::string& url, const fs::path& to) {
    std::printf("[tools] downloading %s\n", url.c_str());
    std::fflush(stdout);
    std::error_code ec;
    const fs::path part = to.string() + ".part";
    fs::remove(part, ec);
#if defined(_WIN32)
    const std::string cmd = systemTool("curl.exe") + " -L --fail --retry 2 -s -o " + quoteArg(part.string()) + " " + quoteArg(url);
#else
    const std::string cmd = !findExecutable("curl").empty()
        ? "curl -L --fail --retry 2 -s -o " + quoteArg(part.string()) + " " + quoteArg(url)
        : "wget -q -O " + quoteArg(part.string()) + " " + quoteArg(url);
#endif
    const int rc = run(cmd);
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
    const std::string z = quoteArg(zip.string()), d = quoteArg(dir.string()), m = member.empty() ? "" : " " + quoteArg(member);
#if defined(_WIN32)
    const int rc = run(systemTool("tar.exe") + " -xf " + z + " -C " + d + m);
#elif defined(__APPLE__)
    const int rc = run("tar -xf " + z + " -C " + d + m); // (bsdtar reads zip files)
#else
    // GNU tar does not read zip files: unzip, else bsdtar, else Python's zipfile
    int rc = -1;
    if (!findExecutable("unzip").empty()) rc = run("unzip -o -q " + z + m + " -d " + d);
    else if (!findExecutable("bsdtar").empty()) rc = run("bsdtar -xf " + z + " -C " + d + m);
    else if (!findExecutable("python3").empty())
        rc = run("python3 -c \"import sys, zipfile; z = zipfile.ZipFile(sys.argv[1]); "
                 "z.extract(sys.argv[3], sys.argv[2]) if len(sys.argv) > 3 else z.extractall(sys.argv[2])\" " + z + " " + d + m);
    else std::printf("[tools] cannot open %s: no unzip, bsdtar or python3\n", zip.string().c_str());
#endif
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
        const std::string newPath = dir.string() + PATH_SEP + (path ? path : "");
#if defined(_WIN32)
        _putenv_s("PATH", newPath.c_str());
#else
        setenv("PATH", newPath.c_str(), 1);
#endif
        // yt-dlp: its own release (one file); YouTube changes often, so it updates itself once a week
        const fs::path ytdlp = dir / (std::string("yt-dlp") + EXE);
        if (findExecutable("yt-dlp").empty()) {
            if (fetch(DOWNLOADS.ytdlp, ytdlp)) makeExecutable(ytdlp);
        } else if (fs::exists(ytdlp, ec)) {
            const auto age = fs::file_time_type::clock::now() - fs::last_write_time(ytdlp, ec);
            if (!ec && age > std::chrono::hours(24 * 7)) {
                std::printf("[tools] updating yt-dlp\n");
                run(quoteArg(ytdlp.string()) + " -U");
                fs::last_write_time(ytdlp, fs::file_time_type::clock::now(), ec);
            }
        }
        // ffmpeg (yt-dlp converts the songs with it)
        if (findExecutable("ffmpeg").empty()) {
            const fs::path whl = dir / "ffmpeg.whl";
            if (fetch(DOWNLOADS.ffmpegWheel, whl) && unzip(whl, dir, DOWNLOADS.ffmpegMember)) {
                fs::rename(dir / DOWNLOADS.ffmpegMember, dir / (std::string("ffmpeg") + EXE), ec);
                makeExecutable(dir / (std::string("ffmpeg") + EXE));
                fs::remove_all(dir / "imageio_ffmpeg", ec);
            }
        }
        // deno: the JavaScript runtime yt-dlp needs for YouTube (unless node is there)
        if (findExecutable("deno").empty() && findExecutable("node").empty()) {
            const fs::path zip = dir / "deno.zip";
            if (fetch(DOWNLOADS.deno, zip) && unzip(zip, dir)) makeExecutable(dir / (std::string("deno") + EXE));
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

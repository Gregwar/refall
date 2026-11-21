#include "saveslots.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void SaveSlots::init(const std::string& progressPath, const Campaign& main) {
    base_ = progressPath.empty() ? Progress::defaultPath() : progressPath;
    main_ = main;
    active_ = 0;
    std::ifstream f(activePath());
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("slot\t", 0) == 0) active_ = std::clamp(std::atoi(line.c_str() + 5) - 1, 0, COUNT - 1);
    std::printf("[slots] slot %d in use (%s)\n", active_ + 1, SaveSlots::progressPath(active_).c_str());
}

std::string SaveSlots::dir(int slot) const {
    const fs::path d = fs::path(base_).parent_path();
    return (slot <= 0 ? d : d / ("slot" + std::to_string(slot + 1))).string();
}
std::string SaveSlots::activePath() const { return (fs::path(dir(0)) / "save-slot.tsv").string(); }
std::string SaveSlots::progressPath(int slot) const {
    return slot <= 0 ? base_ : (fs::path(dir(slot)) / fs::path(base_).filename()).string();
}
std::string SaveSlots::placePath(int slot) const { return (fs::path(dir(slot)) / "menu-state.tsv").string(); }
std::string SaveSlots::voyagesDir(int slot) const { return (fs::path(dir(slot)) / "voyages").string(); }

void SaveSlots::setActive(int slot) {
    active_ = std::clamp(slot, 0, COUNT - 1);
    std::error_code ec;
    fs::create_directories(dir(0), ec);
    const std::string path = activePath(), tmp = path + ".tmp";
    if (FILE* f = std::fopen(tmp.c_str(), "w")) {
        std::fprintf(f, "# Refall: the save slot in use\nslot\t%d\n", active_ + 1);
        std::fclose(f);
        fs::rename(tmp, path, ec);
    }
    std::printf("[slots] slot %d in use\n", active_ + 1);
}

SaveSlots::Summary SaveSlots::summary(int slot) const {
    Summary r;
    const std::string path = progressPath(slot);
    std::error_code ec;
    if (!fs::exists(path, ec)) return r;
    Progress p;
    p.load(path, &main_);
    r.unlocked = p.allUnlocked();
    r.cleared = p.clearedCount();
    r.stages = main_.size();
    for (int s = 0; s < main_.size(); s++)
        for (int k = 0; k < (int)main_.stages()[s].songs.size(); k++) {
            r.songs++;
            int best = 0;
            bool b = false;
            for (int d = 0; d < DIFF_COUNT; d++) {
                b = b || p.beaten(s, k, d);
                best = std::max(best, p.best(s, k, d));
            }
            r.beaten += b;
            r.score += best;
        }
    r.empty = !r.unlocked && r.cleared == 0 && r.beaten == 0;
    r.played = r.empty ? 0 : p.played();
    return r;
}

void SaveSlots::erase(int slot) const {
    std::error_code ec;
    int n = 0;
    for (const std::string& f : {progressPath(slot), placePath(slot)}) n += fs::remove(f, ec);
    // the voyages' progress (in slot 1's folder, the voyages themselves stay)
    for (fs::directory_iterator it(voyagesDir(slot), ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.size() > 13 && name.compare(name.size() - 13, 13, "-progress.tsv") == 0) n += fs::remove(it->path(), ec);
    }
    if (slot > 0) fs::remove(voyagesDir(slot), ec), fs::remove(dir(slot), ec); // (only when empty)
    std::printf("[slots] slot %d erased (%d files)\n", slot + 1, n);
}

void SaveSlots::unlockAll(int slot) const {
    Progress p;
    p.load(progressPath(slot), &main_);
    p.unlockAll();
    p.save();
    std::printf("[slots] slot %d: every stage unlocked\n", slot + 1);
}

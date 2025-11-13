#include "scroll_loader.h"
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace scrolls {

static std::vector<std::string> defaultRoots() {
    std::vector<std::string> roots;
#ifdef _WIN32
    char* envHome = std::getenv("ARDENT_HOME");
    if (envHome) {
        roots.emplace_back(std::string(envHome) + "\\scrolls\\");
    }
    char* pf = std::getenv("ProgramFiles");
    if (pf) roots.emplace_back(std::string(pf) + "\\Ardent\\scrolls\\");
    // Project-local fallback
    roots.emplace_back("scrolls\\");
#else
    char* envHome = std::getenv("ARDENT_HOME");
    if (envHome) {
        roots.emplace_back(std::string(envHome) + "/scrolls/");
    }
    roots.emplace_back("/usr/local/lib/ardent/scrolls/");
    // Project-local fallback
    roots.emplace_back("scrolls/");
#endif
    return roots;
}

std::vector<std::string> candidateRoots() { return defaultRoots(); }

ResolveResult resolve(const std::string& logicalName) {
    // If name already has extension, use it directly; else append .ardent
    bool hasExt = false;
    if (logicalName.size() >= 7) {
        std::string tail = logicalName.substr(logicalName.size()-7);
        if (tail == ".ardent" || tail == ".avm") hasExt = true;
    }
    std::string base = logicalName;
    const std::vector<std::string> subdirs = {"", "core/", "chronicles/", "alchemy/", "heroes/"};
    auto roots = defaultRoots();
    for (const auto& r : roots) {
        for (const auto& sd : subdirs) {
            std::string path = r + sd + base + (hasExt ? "" : ".ardent");
            return {path, true};
        }
    }
    return {"", false};
}

}

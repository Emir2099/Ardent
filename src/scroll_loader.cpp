#include "scroll_loader.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace scrolls {

namespace fs = std::filesystem;

static bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static std::string expandUserTilde(const std::string& in) {
    if (in.size() >= 2 && in[0] == '~' && (in[1] == '/' || in[1] == '\\')) {
#ifdef _WIN32
        const char* up = std::getenv("USERPROFILE");
        if (up && *up) {
            fs::path base(up);
            fs::path rest = in.substr(2);
            return (base / rest).string();
        }
#else
        const char* home = std::getenv("HOME");
        if (home && *home) {
            fs::path base(home);
            fs::path rest = in.substr(2);
            return (base / rest).string();
        }
#endif
    }
    return in;
}

static std::vector<std::string> defaultRoots() {
    std::vector<std::string> roots;
#ifdef _WIN32
    if (const char* envHome = std::getenv("ARDENT_HOME")) {
        roots.emplace_back((fs::path(envHome) / "scrolls").string() + "\\");
    }
    if (const char* user = std::getenv("USERPROFILE")) {
        roots.emplace_back((fs::path(user) / ".ardent" / "scrolls").string() + "\\");
    }
    if (const char* pf = std::getenv("ProgramFiles")) {
        roots.emplace_back((fs::path(pf) / "Ardent" / "scrolls").string() + "\\");
    }
    // Project-local fallbacks
    roots.emplace_back((fs::path("scrolls")).string() + "\\");
    roots.emplace_back((fs::path("test_scrolls")).string() + "\\");
#else
    if (const char* envHome = std::getenv("ARDENT_HOME")) {
        roots.emplace_back((fs::path(envHome) / "scrolls").string() + "/");
    }
    if (const char* home = std::getenv("HOME")) {
        roots.emplace_back((fs::path(home) / ".ardent" / "scrolls").string() + "/");
    }
    roots.emplace_back("/usr/local/lib/ardent/scrolls/");
    // Project-local fallbacks
    roots.emplace_back("scrolls/");
    roots.emplace_back("test_scrolls/");
#endif
    return roots;
}

std::vector<std::string> candidateRoots() { return defaultRoots(); }

// Simple cache for resolved names within a single process.
static std::unordered_map<std::string, ResolveResult> g_cache;

ResolveResult resolve(const std::string& logicalName) {
    if (auto it = g_cache.find(logicalName); it != g_cache.end()) return it->second;

    // Handle versioned imports: name@ver → name
    std::string name = logicalName;
    auto atPos = name.find('@');
    if (atPos != std::string::npos) {
        name = name.substr(0, atPos);
    }

    // Expand '~/...' for convenience
    name = expandUserTilde(name);

    auto hasKnownExt = [](const std::string& s) {
        auto n = s.size();
        return (n >= 7 && s.substr(n - 7) == ".ardent") || (n >= 4 && s.substr(n - 4) == ".avm");
    };

    // Relative or explicit path import (./ or ../ or contains separator)
    bool looksRelative = (!name.empty() && (name.rfind("./", 0) == 0 || name.rfind("../", 0) == 0)) || (name.find('/') != std::string::npos) || (name.find('\\') != std::string::npos);
    if (looksRelative) {
        fs::path base(name);
        if (hasKnownExt(name)) {
            if (fileExists(base)) {
                std::error_code ec; auto canon = fs::weakly_canonical(base, ec);
                return g_cache[logicalName] = { ec ? base.string() : canon.string(), true };
            }
        } else {
            fs::path candAvm = base; candAvm += ".avm";
            fs::path candSrc = base; candSrc += ".ardent";
            if (fileExists(candAvm)) {
                std::error_code ec; auto canon = fs::weakly_canonical(candAvm, ec);
                return g_cache[logicalName] = { ec ? candAvm.string() : canon.string(), true };
            }
            if (fileExists(candSrc)) {
                std::error_code ec; auto canon = fs::weakly_canonical(candSrc, ec);
                return g_cache[logicalName] = { ec ? candSrc.string() : canon.string(), true };
            }
        }
        return g_cache[logicalName] = { "", false };
    }

    // Standard library/rooted search. Prefer .avm then .ardent
    const std::vector<std::string> subdirs = {"", "core/", "chronicles/", "alchemy/", "heroes/", "numbers/", "truths/", "time/", "echoes/"};
    auto roots = defaultRoots();
    for (const auto& r : roots) {
        for (const auto& sd : subdirs) {
            fs::path dir = fs::path(r) / sd;
            if (hasKnownExt(name)) {
                fs::path cand = dir / name;
                if (fileExists(cand)) {
                    std::error_code ec; auto canon = fs::weakly_canonical(cand, ec);
                    return g_cache[logicalName] = { ec ? cand.string() : canon.string(), true };
                }
            } else {
                fs::path candAvm = dir / (name + ".avm");
                fs::path candSrc = dir / (name + ".ardent");
                if (fileExists(candAvm)) {
                    std::error_code ec; auto canon = fs::weakly_canonical(candAvm, ec);
                    return g_cache[logicalName] = { ec ? candAvm.string() : canon.string(), true };
                }
                if (fileExists(candSrc)) {
                    std::error_code ec; auto canon = fs::weakly_canonical(candSrc, ec);
                    return g_cache[logicalName] = { ec ? candSrc.string() : canon.string(), true };
                }
            }
        }
    }

    return g_cache[logicalName] = { "", false };
}

}

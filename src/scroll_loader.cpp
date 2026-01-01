#include "scroll_loader.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif

namespace scrolls {

namespace fs = std::filesystem;

static bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

static bool dirExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec);
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

// Get the Ardent home directory (for installed packages)
static fs::path getArdentDir() {
#ifdef _WIN32
    const char* up = std::getenv("USERPROFILE");
    if (up && *up) return fs::path(up) / ".ardent";
    return fs::path("C:") / ".ardent";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) return fs::path(home) / ".ardent";
    return fs::path("/tmp") / ".ardent";
#endif
}

// Simple SemVer comparison for package resolution
struct SimpleSemVer {
    int major = 0, minor = 0, patch = 0;
    std::string prerelease;
    
    static SimpleSemVer parse(const std::string& s) {
        SimpleSemVer v;
        size_t pos = 0;
        size_t dot1 = s.find('.');
        if (dot1 == std::string::npos) { v.major = std::stoi(s); return v; }
        v.major = std::stoi(s.substr(0, dot1));
        size_t dot2 = s.find('.', dot1 + 1);
        if (dot2 == std::string::npos) { v.minor = std::stoi(s.substr(dot1 + 1)); return v; }
        v.minor = std::stoi(s.substr(dot1 + 1, dot2 - dot1 - 1));
        size_t hyphen = s.find('-', dot2 + 1);
        if (hyphen == std::string::npos) {
            v.patch = std::stoi(s.substr(dot2 + 1));
        } else {
            v.patch = std::stoi(s.substr(dot2 + 1, hyphen - dot2 - 1));
            v.prerelease = s.substr(hyphen + 1);
        }
        return v;
    }
    
    bool operator>(const SimpleSemVer& o) const {
        if (major != o.major) return major > o.major;
        if (minor != o.minor) return minor > o.minor;
        if (patch != o.patch) return patch > o.patch;
        if (prerelease.empty() && !o.prerelease.empty()) return true;
        if (!prerelease.empty() && o.prerelease.empty()) return false;
        return prerelease > o.prerelease;
    }
};

// Try to resolve from installed packages at ~/.ardent/scrolls/
static ResolveResult resolveFromInstalledPackages(const std::string& name, const std::string& versionConstraint) {
    fs::path scrollsDir = getArdentDir() / "scrolls";
    if (!dirExists(scrollsDir)) return {"", false};
    
    std::vector<std::pair<SimpleSemVer, fs::path>> candidates;
    std::string prefix = name + "@";
    
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(scrollsDir, ec)) {
        if (!entry.is_directory()) continue;
        std::string dirname = entry.path().filename().string();
        if (dirname.rfind(prefix, 0) == 0) {
            std::string verStr = dirname.substr(prefix.size());
            try {
                SimpleSemVer ver = SimpleSemVer::parse(verStr);
                candidates.push_back({ver, entry.path()});
            } catch (...) {
                // Invalid version format, skip
            }
        }
    }
    
    if (candidates.empty()) return {"", false};
    
    // Sort by version descending
    std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
        return a.first > b.first;
    });
    
    // For now, pick the highest version (version constraint matching can be added later)
    fs::path pkgDir = candidates[0].second;
    
    // Priority: native binary > .avm bytecode > .ardent source
    // Look for lib/<name>.* or <name>.*
    fs::path libDir = pkgDir / "lib";
    std::vector<fs::path> searchDirs = {pkgDir, libDir};
    
    for (const auto& dir : searchDirs) {
        if (!dirExists(dir)) continue;
        
#ifdef _WIN32
        fs::path nativePath = dir / (name + ".dll");
#else
        fs::path nativePath = dir / (name + ".so");
#endif
        if (fileExists(nativePath)) {
            auto canon = fs::weakly_canonical(nativePath, ec);
            return {ec ? nativePath.string() : canon.string(), true};
        }
        
        fs::path avmPath = dir / (name + ".avm");
        if (fileExists(avmPath)) {
            auto canon = fs::weakly_canonical(avmPath, ec);
            return {ec ? avmPath.string() : canon.string(), true};
        }
        
        fs::path srcPath = dir / (name + ".ardent");
        if (fileExists(srcPath)) {
            auto canon = fs::weakly_canonical(srcPath, ec);
            return {ec ? srcPath.string() : canon.string(), true};
        }
    }
    
    // Try main entry from scroll.toml if exists
    fs::path tomlPath = pkgDir / "scroll.toml";
    if (fileExists(tomlPath)) {
        std::ifstream f(tomlPath);
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("main") != std::string::npos && line.find('=') != std::string::npos) {
                size_t eq = line.find('=');
                std::string val = line.substr(eq + 1);
                // Trim quotes and whitespace
                size_t start = val.find_first_not_of(" \t\"'");
                size_t end = val.find_last_not_of(" \t\"'");
                if (start != std::string::npos && end != std::string::npos) {
                    std::string mainFile = val.substr(start, end - start + 1);
                    fs::path mainPath = pkgDir / mainFile;
                    if (fileExists(mainPath)) {
                        auto canon = fs::weakly_canonical(mainPath, ec);
                        return {ec ? mainPath.string() : canon.string(), true};
                    }
                }
            }
        }
    }
    
    return {"", false};
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

    // Extract version constraint if present (e.g., "name@^1.0")
    std::string versionConstraint;
    auto atPosOriginal = logicalName.find('@');
    if (atPosOriginal != std::string::npos) {
        versionConstraint = logicalName.substr(atPosOriginal + 1);
    }

    // Try installed packages first (Scrollsmith packages at ~/.ardent/scrolls/)
    auto pkgResult = resolveFromInstalledPackages(name, versionConstraint);
    if (pkgResult.found) {
        return g_cache[logicalName] = pkgResult;
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

// scroll_registry.h — Ardent 2.3 Scrollsmith
// Registry fetching and local cache management
#ifndef SCROLL_REGISTRY_H
#define SCROLL_REGISTRY_H

#include "scroll_manifest.h"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace ardent {

namespace fs = std::filesystem;

// ─── Path Utilities ────────────────────────────────────────────────────────

inline std::string getHomeDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return std::string(path);
    }
    const char* home = std::getenv("USERPROFILE");
    if (home) return home;
    return "C:\\Users\\Default";
#else
    const char* home = std::getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    if (pw) return pw->pw_dir;
    return "/tmp";
#endif
}

inline std::string getArdentDir() {
    return getHomeDir() + "/.ardent";
}

inline std::string getScrollsDir() {
    return getArdentDir() + "/scrolls";
}

inline std::string getRegistryDir() {
    return getArdentDir() + "/registry";
}

inline std::string getKeysDir() {
    return getArdentDir() + "/keys";
}

// ─── Registry Entry ────────────────────────────────────────────────────────

struct RegistryEntry {
    std::string name;
    std::vector<SemVer> versions;
    std::string latestVersion;
    std::string description;
    std::string author;
};

// ─── Installed Scroll ──────────────────────────────────────────────────────

struct InstalledScroll {
    std::string name;
    SemVer version;
    fs::path path;
    bool hasAvm = false;
    bool hasNative = false;
    bool hasSource = false;
    ScrollManifest manifest;
};

// ─── Scroll Cache ──────────────────────────────────────────────────────────

class ScrollCache {
public:
    ScrollCache() {
        ensureDirs();
    }
    
    void ensureDirs() {
        fs::create_directories(getScrollsDir());
        fs::create_directories(getRegistryDir());
        fs::create_directories(getKeysDir());
    }
    
    // Get path to installed scroll
    fs::path getScrollPath(const std::string& name, const SemVer& version) const {
        return fs::path(getScrollsDir()) / (name + "@" + version.toString());
    }
    
    // Check if scroll is installed
    bool isInstalled(const std::string& name, const SemVer& version) const {
        return fs::exists(getScrollPath(name, version));
    }
    
    // List all installed scrolls
    std::vector<InstalledScroll> listInstalled() const {
        std::vector<InstalledScroll> result;
        fs::path scrollsDir = getScrollsDir();
        
        if (!fs::exists(scrollsDir)) return result;
        
        for (const auto& entry : fs::directory_iterator(scrollsDir)) {
            if (!entry.is_directory()) continue;
            
            std::string dirname = entry.path().filename().string();
            size_t at = dirname.find('@');
            if (at == std::string::npos) continue;
            
            InstalledScroll scroll;
            scroll.name = dirname.substr(0, at);
            auto v = SemVer::parse(dirname.substr(at + 1));
            if (!v) continue;
            scroll.version = *v;
            scroll.path = entry.path();
            
            // Check for artifacts
            scroll.hasAvm = fs::exists(entry.path() / (scroll.name + ".avm"));
            scroll.hasNative = fs::exists(entry.path() / (scroll.name + ".exe")) ||
                               fs::exists(entry.path() / (scroll.name + ".so")) ||
                               fs::exists(entry.path() / (scroll.name + ".dylib"));
            scroll.hasSource = fs::exists(entry.path() / (scroll.name + ".ardent"));
            
            // Load manifest if present
            auto manifestPath = entry.path() / "scroll.toml";
            if (fs::exists(manifestPath)) {
                auto m = ScrollManifest::parseFile(manifestPath.string());
                if (m) scroll.manifest = *m;
            }
            
            result.push_back(scroll);
        }
        
        return result;
    }
    
    // Find best matching installed version
    std::optional<InstalledScroll> findBestMatch(const std::string& name, const VersionRange& range) const {
        auto installed = listInstalled();
        InstalledScroll* best = nullptr;
        
        for (auto& scroll : installed) {
            if (scroll.name != name) continue;
            if (!range.matches(scroll.version)) continue;
            if (!best || scroll.version > best->version) {
                best = &scroll;
            }
        }
        
        if (best) return *best;
        return std::nullopt;
    }
    
    // Install scroll from downloaded content
    bool install(const std::string& name, const SemVer& version, 
                 const std::map<std::string, std::string>& files) {
        fs::path dest = getScrollPath(name, version);
        fs::create_directories(dest);
        
        for (const auto& [filename, content] : files) {
            std::ofstream f(dest / filename, std::ios::binary);
            if (!f.is_open()) return false;
            f.write(content.data(), content.size());
        }
        
        return true;
    }
    
    // Remove installed scroll
    bool remove(const std::string& name, const SemVer& version) {
        fs::path path = getScrollPath(name, version);
        if (!fs::exists(path)) return false;
        fs::remove_all(path);
        return true;
    }
};

// ─── Registry Source ───────────────────────────────────────────────────────

struct RegistrySource {
    std::string name;
    std::string url;       // HTTP URL or file:// path
    bool isOfficial = false;
};

// ─── Version Resolver ──────────────────────────────────────────────────────

struct ResolvedDependency {
    std::string name;
    SemVer version;
    bool alreadyInstalled = false;
    std::string downloadUrl;
};

struct ResolutionResult {
    bool success = false;
    std::vector<ResolvedDependency> toInstall;
    std::vector<std::string> errors;
};

class DependencyResolver {
public:
    DependencyResolver(ScrollCache& cache) : cache_(cache) {}
    
    ResolutionResult resolve(const std::vector<Dependency>& deps,
                             const std::vector<RegistryEntry>& available) {
        ResolutionResult result;
        result.success = true;
        
        for (const auto& dep : deps) {
            // First check if already installed and satisfies constraint
            auto installed = cache_.findBestMatch(dep.name, dep.range);
            if (installed) {
                ResolvedDependency rd;
                rd.name = dep.name;
                rd.version = installed->version;
                rd.alreadyInstalled = true;
                result.toInstall.push_back(rd);
                continue;
            }
            
            // Find best match from registry
            const RegistryEntry* entry = nullptr;
            for (const auto& e : available) {
                if (e.name == dep.name) {
                    entry = &e;
                    break;
                }
            }
            
            if (!entry) {
                result.errors.push_back("Scroll not found in registry: " + dep.name);
                result.success = false;
                continue;
            }
            
            // Find best matching version
            SemVer bestVersion;
            bool found = false;
            for (const auto& v : entry->versions) {
                if (dep.range.matches(v)) {
                    if (!found || v > bestVersion) {
                        bestVersion = v;
                        found = true;
                    }
                }
            }
            
            if (!found) {
                result.errors.push_back("No compatible version found for: " + dep.name);
                result.success = false;
                continue;
            }
            
            ResolvedDependency rd;
            rd.name = dep.name;
            rd.version = bestVersion;
            rd.alreadyInstalled = false;
            result.toInstall.push_back(rd);
        }
        
        return result;
    }
    
private:
    ScrollCache& cache_;
};

// ─── Simple HTTP Fetch (placeholder for actual HTTP client) ────────────────

// This is a minimal placeholder. In production, use libcurl or similar.
inline bool fetchUrl(const std::string& url, std::string& outContent) {
    // For file:// URLs, read directly
    if (url.substr(0, 7) == "file://") {
        std::string path = url.substr(7);
#ifdef _WIN32
        // Handle Windows paths like file:///C:/...
        if (!path.empty() && path[0] == '/') path = path.substr(1);
#endif
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        outContent = ss.str();
        return true;
    }
    
    // For HTTP URLs, we need an actual HTTP client
    // For now, return false (not implemented)
    // In production: use WinHTTP on Windows, libcurl on Unix
    (void)url;
    (void)outContent;
    return false;
}

// ─── Signature Verification (placeholder) ──────────────────────────────────

inline bool verifySignature(const std::string& content, const std::string& signature,
                            const std::string& publicKey) {
    // Placeholder for Ed25519/RSA signature verification
    // In production: use OpenSSL or libsodium
    (void)content;
    (void)signature;
    (void)publicKey;
    return true; // Trust all for now
}

} // namespace ardent

#endif // SCROLL_REGISTRY_H

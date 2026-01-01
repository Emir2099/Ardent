// scroll_manifest.h — Ardent 2.3 Scrollsmith
// TOML manifest parser for scroll.toml package files
#ifndef SCROLL_MANIFEST_H
#define SCROLL_MANIFEST_H

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace ardent {

// ─── Semantic Version ──────────────────────────────────────────────────────

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;
    
    SemVer() = default;
    SemVer(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}
    
    static std::optional<SemVer> parse(const std::string& str) {
        std::regex re(R"(^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9.-]+))?$)");
        std::smatch m;
        if (!std::regex_match(str, m, re)) return std::nullopt;
        SemVer v;
        v.major = std::stoi(m[1].str());
        v.minor = std::stoi(m[2].str());
        v.patch = std::stoi(m[3].str());
        if (m[4].matched) v.prerelease = m[4].str();
        return v;
    }
    
    std::string toString() const {
        std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        if (!prerelease.empty()) s += "-" + prerelease;
        return s;
    }
    
    bool operator<(const SemVer& o) const {
        if (major != o.major) return major < o.major;
        if (minor != o.minor) return minor < o.minor;
        if (patch != o.patch) return patch < o.patch;
        // Prerelease versions are less than release
        if (prerelease.empty() && !o.prerelease.empty()) return false;
        if (!prerelease.empty() && o.prerelease.empty()) return true;
        return prerelease < o.prerelease;
    }
    bool operator==(const SemVer& o) const {
        return major == o.major && minor == o.minor && patch == o.patch && prerelease == o.prerelease;
    }
    bool operator<=(const SemVer& o) const { return *this < o || *this == o; }
    bool operator>(const SemVer& o) const { return !(*this <= o); }
    bool operator>=(const SemVer& o) const { return !(*this < o); }
    bool operator!=(const SemVer& o) const { return !(*this == o); }
};

// ─── Version Constraint ────────────────────────────────────────────────────

enum class ConstraintOp { Exact, Caret, Tilde, GreaterEq, Greater, LessEq, Less };

struct VersionConstraint {
    ConstraintOp op = ConstraintOp::Caret;
    SemVer version;
    
    static std::optional<VersionConstraint> parse(const std::string& str) {
        std::string s = str;
        // Trim whitespace
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
        
        VersionConstraint c;
        if (s.empty()) return std::nullopt;
        
        if (s[0] == '^') {
            c.op = ConstraintOp::Caret;
            s = s.substr(1);
        } else if (s[0] == '~') {
            c.op = ConstraintOp::Tilde;
            s = s.substr(1);
        } else if (s.substr(0, 2) == ">=") {
            c.op = ConstraintOp::GreaterEq;
            s = s.substr(2);
        } else if (s.substr(0, 2) == "<=") {
            c.op = ConstraintOp::LessEq;
            s = s.substr(2);
        } else if (s[0] == '>') {
            c.op = ConstraintOp::Greater;
            s = s.substr(1);
        } else if (s[0] == '<') {
            c.op = ConstraintOp::Less;
            s = s.substr(1);
        } else {
            c.op = ConstraintOp::Exact;
        }
        
        // Trim again after operator
        s.erase(0, s.find_first_not_of(" \t"));
        
        auto v = SemVer::parse(s);
        if (!v) return std::nullopt;
        c.version = *v;
        return c;
    }
    
    bool matches(const SemVer& v) const {
        switch (op) {
            case ConstraintOp::Exact: return v == version;
            case ConstraintOp::GreaterEq: return v >= version;
            case ConstraintOp::Greater: return v > version;
            case ConstraintOp::LessEq: return v <= version;
            case ConstraintOp::Less: return v < version;
            case ConstraintOp::Caret:
                // ^1.2.3 means >=1.2.3 and <2.0.0 (for major > 0)
                // ^0.2.3 means >=0.2.3 and <0.3.0
                if (v < version) return false;
                if (version.major > 0) return v.major == version.major;
                return v.minor == version.minor;
            case ConstraintOp::Tilde:
                // ~1.2.3 means >=1.2.3 and <1.3.0
                if (v < version) return false;
                return v.major == version.major && v.minor == version.minor;
        }
        return false;
    }
};

// ─── Compound Version Range ────────────────────────────────────────────────

struct VersionRange {
    std::vector<VersionConstraint> constraints;
    
    static std::optional<VersionRange> parse(const std::string& str) {
        VersionRange range;
        std::string s = str;
        // Split on spaces (AND constraints)
        std::istringstream iss(s);
        std::string part;
        while (iss >> part) {
            auto c = VersionConstraint::parse(part);
            if (!c) return std::nullopt;
            range.constraints.push_back(*c);
        }
        if (range.constraints.empty()) return std::nullopt;
        return range;
    }
    
    bool matches(const SemVer& v) const {
        for (const auto& c : constraints) {
            if (!c.matches(v)) return false;
        }
        return true;
    }
};

// ─── Dependency ────────────────────────────────────────────────────────────

struct Dependency {
    std::string name;
    VersionRange range;
    bool optional = false;
};

// ─── Build Target ──────────────────────────────────────────────────────────

enum class BuildTarget { AVM, Native, Source };

// ─── Scroll Manifest ───────────────────────────────────────────────────────

struct ScrollManifest {
    // [scroll] section
    std::string name;
    SemVer version;
    std::string description;
    std::string author;
    std::string license;
    std::vector<std::string> keywords;
    std::string repository;
    
    // [dependencies] section
    std::vector<Dependency> dependencies;
    
    // [build] section
    std::string entry;
    std::vector<BuildTarget> targets;
    
    // [compat] section
    VersionRange ardentVersion;
    
    // Parsing
    static std::optional<ScrollManifest> parseFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return std::nullopt;
        std::stringstream ss;
        ss << f.rdbuf();
        return parse(ss.str());
    }
    
    static std::optional<ScrollManifest> parse(const std::string& toml) {
        ScrollManifest m;
        std::string currentSection;
        
        std::istringstream iss(toml);
        std::string line;
        while (std::getline(iss, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            if (line.empty() || line[0] == '#') continue;
            
            // Section header
            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != std::string::npos) {
                    currentSection = line.substr(1, end - 1);
                }
                continue;
            }
            
            // Key = value
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            
            // Trim key/value
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            
            // Remove quotes from string values
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            
            if (currentSection == "scroll") {
                if (key == "name") m.name = value;
                else if (key == "version") {
                    auto v = SemVer::parse(value);
                    if (v) m.version = *v;
                }
                else if (key == "description") m.description = value;
                else if (key == "author") m.author = value;
                else if (key == "license") m.license = value;
                else if (key == "repository") m.repository = value;
                else if (key == "keywords") {
                    // Parse array: ["a", "b", "c"]
                    if (value.front() == '[' && value.back() == ']') {
                        value = value.substr(1, value.size() - 2);
                        std::istringstream arrStream(value);
                        std::string item;
                        while (std::getline(arrStream, item, ',')) {
                            item.erase(0, item.find_first_not_of(" \t\""));
                            item.erase(item.find_last_not_of(" \t\"") + 1);
                            if (!item.empty()) m.keywords.push_back(item);
                        }
                    }
                }
            }
            else if (currentSection == "dependencies") {
                Dependency dep;
                dep.name = key;
                auto range = VersionRange::parse(value);
                if (range) {
                    dep.range = *range;
                    m.dependencies.push_back(dep);
                }
            }
            else if (currentSection == "build") {
                if (key == "entry") m.entry = value;
                else if (key == "targets") {
                    // Parse array
                    if (value.front() == '[' && value.back() == ']') {
                        value = value.substr(1, value.size() - 2);
                        std::istringstream arrStream(value);
                        std::string item;
                        while (std::getline(arrStream, item, ',')) {
                            item.erase(0, item.find_first_not_of(" \t\""));
                            item.erase(item.find_last_not_of(" \t\"") + 1);
                            if (item == "avm") m.targets.push_back(BuildTarget::AVM);
                            else if (item == "native") m.targets.push_back(BuildTarget::Native);
                            else if (item == "source") m.targets.push_back(BuildTarget::Source);
                        }
                    }
                }
            }
            else if (currentSection == "compat") {
                if (key == "ardent") {
                    auto range = VersionRange::parse(value);
                    if (range) m.ardentVersion = *range;
                }
            }
        }
        
        if (m.name.empty()) return std::nullopt;
        return m;
    }
    
    // Generate TOML string
    std::string toToml() const {
        std::ostringstream oss;
        oss << "[scroll]\n";
        oss << "name = \"" << name << "\"\n";
        oss << "version = \"" << version.toString() << "\"\n";
        if (!description.empty()) oss << "description = \"" << description << "\"\n";
        if (!author.empty()) oss << "author = \"" << author << "\"\n";
        if (!license.empty()) oss << "license = \"" << license << "\"\n";
        if (!repository.empty()) oss << "repository = \"" << repository << "\"\n";
        if (!keywords.empty()) {
            oss << "keywords = [";
            for (size_t i = 0; i < keywords.size(); i++) {
                if (i > 0) oss << ", ";
                oss << "\"" << keywords[i] << "\"";
            }
            oss << "]\n";
        }
        
        if (!dependencies.empty()) {
            oss << "\n[dependencies]\n";
            for (const auto& dep : dependencies) {
                oss << dep.name << " = \"";
                for (size_t i = 0; i < dep.range.constraints.size(); i++) {
                    if (i > 0) oss << " ";
                    const auto& c = dep.range.constraints[i];
                    switch (c.op) {
                        case ConstraintOp::Caret: oss << "^"; break;
                        case ConstraintOp::Tilde: oss << "~"; break;
                        case ConstraintOp::GreaterEq: oss << ">="; break;
                        case ConstraintOp::Greater: oss << ">"; break;
                        case ConstraintOp::LessEq: oss << "<="; break;
                        case ConstraintOp::Less: oss << "<"; break;
                        case ConstraintOp::Exact: break;
                    }
                    oss << c.version.toString();
                }
                oss << "\"\n";
            }
        }
        
        if (!entry.empty() || !targets.empty()) {
            oss << "\n[build]\n";
            if (!entry.empty()) oss << "entry = \"" << entry << "\"\n";
            if (!targets.empty()) {
                oss << "targets = [";
                for (size_t i = 0; i < targets.size(); i++) {
                    if (i > 0) oss << ", ";
                    switch (targets[i]) {
                        case BuildTarget::AVM: oss << "\"avm\""; break;
                        case BuildTarget::Native: oss << "\"native\""; break;
                        case BuildTarget::Source: oss << "\"source\""; break;
                    }
                }
                oss << "]\n";
            }
        }
        
        if (!ardentVersion.constraints.empty()) {
            oss << "\n[compat]\n";
            oss << "ardent = \"";
            for (size_t i = 0; i < ardentVersion.constraints.size(); i++) {
                if (i > 0) oss << " ";
                const auto& c = ardentVersion.constraints[i];
                switch (c.op) {
                    case ConstraintOp::GreaterEq: oss << ">="; break;
                    case ConstraintOp::Greater: oss << ">"; break;
                    case ConstraintOp::LessEq: oss << "<="; break;
                    case ConstraintOp::Less: oss << "<"; break;
                    case ConstraintOp::Caret: oss << "^"; break;
                    case ConstraintOp::Tilde: oss << "~"; break;
                    case ConstraintOp::Exact: break;
                }
                oss << c.version.toString();
            }
            oss << "\"\n";
        }
        
        return oss.str();
    }
};

// ─── Lock File ─────────────────────────────────────────────────────────────

struct LockedDependency {
    std::string name;
    SemVer version;
    std::string integrity; // SHA256 hash
};

struct ScrollLock {
    std::vector<LockedDependency> locked;
    
    static std::optional<ScrollLock> parseFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return std::nullopt;
        
        ScrollLock lock;
        std::string line;
        while (std::getline(f, line)) {
            // Format: name@version sha256:hash
            std::istringstream iss(line);
            std::string nameVer, hash;
            iss >> nameVer >> hash;
            
            size_t at = nameVer.find('@');
            if (at == std::string::npos) continue;
            
            LockedDependency dep;
            dep.name = nameVer.substr(0, at);
            auto v = SemVer::parse(nameVer.substr(at + 1));
            if (!v) continue;
            dep.version = *v;
            
            if (hash.substr(0, 7) == "sha256:") {
                dep.integrity = hash.substr(7);
            }
            lock.locked.push_back(dep);
        }
        return lock;
    }
    
    void saveFile(const std::string& path) const {
        std::ofstream f(path);
        for (const auto& dep : locked) {
            f << dep.name << "@" << dep.version.toString();
            if (!dep.integrity.empty()) {
                f << " sha256:" << dep.integrity;
            }
            f << "\n";
        }
    }
};

} // namespace ardent

#endif // SCROLL_MANIFEST_H

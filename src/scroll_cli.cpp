// scroll_cli.cpp — Ardent 2.3 Scrollsmith CLI
// Package manager command-line interface
#include "scroll_manifest.h"
#include "scroll_registry.h"
#include "version.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace ardent;

// ─── ANSI Colors ───────────────────────────────────────────────────────────

#ifdef _WIN32
#include <windows.h>
static void enableAnsiColors() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
static void enableAnsiColors() {}
#endif

const char* RESET = "\033[0m";
const char* BOLD = "\033[1m";
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* BLUE = "\033[34m";
const char* CYAN = "\033[36m";
const char* RED = "\033[31m";

// ─── Help ──────────────────────────────────────────────────────────────────

void printHelp() {
    std::cout << BOLD << "scroll" << RESET << " -- Ardent Package Manager\n\n";
    std::cout << BOLD << "USAGE:" << RESET << "\n";
    std::cout << "    scroll <command> [options]\n\n";
    std::cout << BOLD << "COMMANDS:" << RESET << "\n";
    std::cout << "    " << GREEN << "init" << RESET << "              Create a new scroll.toml manifest\n";
    std::cout << "    " << GREEN << "install" << RESET << " [name]    Install a scroll (or all dependencies)\n";
    std::cout << "    " << GREEN << "list" << RESET << "              List installed scrolls\n";
    std::cout << "    " << GREEN << "info" << RESET << " <name>       Show scroll information\n";
    std::cout << "    " << GREEN << "remove" << RESET << " <name>     Remove an installed scroll\n";
    std::cout << "    " << GREEN << "search" << RESET << " <query>    Search for scrolls in registry\n";
    std::cout << "    " << GREEN << "publish" << RESET << "           Package and publish current scroll\n";
    std::cout << "    " << GREEN << "cache" << RESET << "             Show cache information\n";
    std::cout << "    " << GREEN << "help" << RESET << "              Show this help message\n";
    std::cout << "    " << GREEN << "version" << RESET << "           Show version information\n\n";
    std::cout << BOLD << "EXAMPLES:" << RESET << "\n";
    std::cout << "    scroll init                # Create scroll.toml\n";
    std::cout << "    scroll install truths      # Install 'truths' scroll\n";
    std::cout << "    scroll install truths@1.0  # Install specific version\n";
    std::cout << "    scroll list                # Show installed scrolls\n";
}

// ─── Version Command ───────────────────────────────────────────────────────

void cmdVersion() {
    std::cout << BOLD << "Scroll" << RESET << " " << ARDENT_VERSION << " -- \"" << ARDENT_CODENAME << "\"\n";
    std::cout << "Ardent Package Manager\n";
}

// ─── Init Command ──────────────────────────────────────────────────────────

int cmdInit(const std::string& name = "") {
    if (fs::exists("scroll.toml")) {
        std::cerr << RED << "Error:" << RESET << " scroll.toml already exists in current directory.\n";
        return 1;
    }
    
    ScrollManifest m;
    m.name = name.empty() ? fs::current_path().filename().string() : name;
    m.version = SemVer(1, 0, 0);
    m.description = "A poetic scroll for Ardent";
    m.author = "Unknown Mage";
    m.license = "MIT";
    m.entry = m.name + ".ardent";
    m.targets.push_back(BuildTarget::AVM);
    
    // Set compat to current Ardent version
    VersionConstraint c;
    c.op = ConstraintOp::GreaterEq;
    c.version = SemVer(2, 3, 0);
    m.ardentVersion.constraints.push_back(c);
    
    std::ofstream f("scroll.toml");
    f << m.toToml();
    
    // Create src directory if it doesn't exist
    fs::create_directories("src");
    
    // Create entry file if it doesn't exist
    if (!fs::exists(m.entry)) {
        std::ofstream ef(m.entry);
        ef << "~~ " << m.name << " -- A Poetic Scroll ~~\n\n";
        ef << "Let it be proclaimed: \"Hail from " << m.name << "!\"\n";
    }
    
    std::cout << GREEN << "[ok]" << RESET << " Created scroll.toml for '" << BOLD << m.name << RESET << "'\n";
    std::cout << "  Entry: " << m.entry << "\n";
    return 0;
}

// ─── List Command ──────────────────────────────────────────────────────────

int cmdList() {
    ScrollCache cache;
    auto scrolls = cache.listInstalled();
    
    if (scrolls.empty()) {
        std::cout << YELLOW << "No scrolls installed." << RESET << "\n";
        std::cout << "Use '" << CYAN << "scroll install <name>" << RESET << "' to install scrolls.\n";
        return 0;
    }
    
    std::cout << BOLD << "Installed Scrolls" << RESET << " (" << scrolls.size() << ")\n\n";
    
    // Sort by name
    std::sort(scrolls.begin(), scrolls.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    
    for (const auto& scroll : scrolls) {
        std::cout << "  " << GREEN << scroll.name << RESET << "@" << scroll.version.toString();
        
        // Show available formats
        std::cout << " [";
        bool first = true;
        if (scroll.hasAvm) { std::cout << "avm"; first = false; }
        if (scroll.hasNative) { std::cout << (first ? "" : ", ") << "native"; first = false; }
        if (scroll.hasSource) { std::cout << (first ? "" : ", ") << "source"; }
        std::cout << "]\n";
        
        if (!scroll.manifest.description.empty()) {
            std::cout << "    " << scroll.manifest.description << "\n";
        }
    }
    
    return 0;
}

// ─── Info Command ──────────────────────────────────────────────────────────

int cmdInfo(const std::string& nameSpec) {
    std::string name = nameSpec;
    std::optional<SemVer> version;
    
    // Parse name@version
    size_t at = nameSpec.find('@');
    if (at != std::string::npos) {
        name = nameSpec.substr(0, at);
        version = SemVer::parse(nameSpec.substr(at + 1));
    }
    
    ScrollCache cache;
    auto scrolls = cache.listInstalled();
    
    for (const auto& scroll : scrolls) {
        if (scroll.name == name) {
            if (version && scroll.version != *version) continue;
            
            std::cout << BOLD << scroll.name << RESET << "@" << scroll.version.toString() << "\n\n";
            
            if (!scroll.manifest.description.empty()) {
                std::cout << "  " << CYAN << "Description:" << RESET << " " << scroll.manifest.description << "\n";
            }
            if (!scroll.manifest.author.empty()) {
                std::cout << "  " << CYAN << "Author:" << RESET << " " << scroll.manifest.author << "\n";
            }
            if (!scroll.manifest.license.empty()) {
                std::cout << "  " << CYAN << "License:" << RESET << " " << scroll.manifest.license << "\n";
            }
            
            std::cout << "  " << CYAN << "Path:" << RESET << " " << scroll.path.string() << "\n";
            
            std::cout << "  " << CYAN << "Formats:" << RESET << " ";
            if (scroll.hasAvm) std::cout << "avm ";
            if (scroll.hasNative) std::cout << "native ";
            if (scroll.hasSource) std::cout << "source ";
            std::cout << "\n";
            
            if (!scroll.manifest.dependencies.empty()) {
                std::cout << "\n  " << CYAN << "Dependencies:" << RESET << "\n";
                for (const auto& dep : scroll.manifest.dependencies) {
                    std::cout << "    - " << dep.name << "\n";
                }
            }
            
            return 0;
        }
    }
    
    std::cerr << RED << "Error:" << RESET << " Scroll '" << name << "' not found.\n";
    std::cerr << "Use '" << CYAN << "scroll list" << RESET << "' to see installed scrolls.\n";
    return 1;
}

// ─── Install Command ───────────────────────────────────────────────────────

int cmdInstall(const std::string& nameSpec) {
    ScrollCache cache;
    
    if (nameSpec.empty()) {
        // Install dependencies from scroll.toml
        if (!fs::exists("scroll.toml")) {
            std::cerr << RED << "Error:" << RESET << " No scroll.toml found and no scroll name specified.\n";
            return 1;
        }
        
        auto manifest = ScrollManifest::parseFile("scroll.toml");
        if (!manifest) {
            std::cerr << RED << "Error:" << RESET << " Failed to parse scroll.toml\n";
            return 1;
        }
        
        if (manifest->dependencies.empty()) {
            std::cout << GREEN << "[ok]" << RESET << " No dependencies to install.\n";
            return 0;
        }
        
        std::cout << "Installing " << manifest->dependencies.size() << " dependencies...\n";
        
        for (const auto& dep : manifest->dependencies) {
            // Check if already installed
            auto existing = cache.findBestMatch(dep.name, dep.range);
            if (existing) {
                std::cout << "  " << GREEN << "[ok]" << RESET << " " << dep.name << "@" << existing->version.toString() << " (already installed)\n";
            } else {
                std::cout << "  " << YELLOW << "[!]" << RESET << " " << dep.name << " not available (registry fetch not implemented)\n";
            }
        }
        
        return 0;
    }
    
    // Install specific scroll
    std::string name = nameSpec;
    std::optional<SemVer> version;
    
    size_t at = nameSpec.find('@');
    if (at != std::string::npos) {
        name = nameSpec.substr(0, at);
        version = SemVer::parse(nameSpec.substr(at + 1));
    }
    
    if (version) {
        if (cache.isInstalled(name, *version)) {
            std::cout << GREEN << "[ok]" << RESET << " " << name << "@" << version->toString() << " is already installed.\n";
            return 0;
        }
    }
    
    // TODO: Fetch from registry
    std::cout << YELLOW << "[!]" << RESET << " Registry fetch not yet implemented.\n";
    std::cout << "  To install scrolls, place them manually in: " << getScrollsDir() << "\n";
    
    return 1;
}

// ─── Remove Command ────────────────────────────────────────────────────────

int cmdRemove(const std::string& nameSpec) {
    std::string name = nameSpec;
    std::optional<SemVer> version;
    
    size_t at = nameSpec.find('@');
    if (at != std::string::npos) {
        name = nameSpec.substr(0, at);
        version = SemVer::parse(nameSpec.substr(at + 1));
    }
    
    ScrollCache cache;
    auto scrolls = cache.listInstalled();
    
    bool removed = false;
    for (const auto& scroll : scrolls) {
        if (scroll.name == name) {
            if (version && scroll.version != *version) continue;
            
            if (cache.remove(scroll.name, scroll.version)) {
                std::cout << GREEN << "[ok]" << RESET << " Removed " << scroll.name << "@" << scroll.version.toString() << "\n";
                removed = true;
            }
        }
    }
    
    if (!removed) {
        std::cerr << RED << "Error:" << RESET << " Scroll '" << nameSpec << "' not found.\n";
        return 1;
    }
    
    return 0;
}

// ─── Cache Command ─────────────────────────────────────────────────────────

int cmdCache() {
    std::cout << BOLD << "Ardent Cache Information" << RESET << "\n\n";
    std::cout << "  " << CYAN << "Home:" << RESET << " " << getArdentDir() << "\n";
    std::cout << "  " << CYAN << "Scrolls:" << RESET << " " << getScrollsDir() << "\n";
    std::cout << "  " << CYAN << "Registry:" << RESET << " " << getRegistryDir() << "\n";
    std::cout << "  " << CYAN << "Keys:" << RESET << " " << getKeysDir() << "\n\n";
    
    ScrollCache cache;
    auto scrolls = cache.listInstalled();
    
    size_t totalSize = 0;
    for (const auto& scroll : scrolls) {
        for (const auto& entry : fs::recursive_directory_iterator(scroll.path)) {
            if (entry.is_regular_file()) {
                totalSize += entry.file_size();
            }
        }
    }
    
    std::cout << "  " << CYAN << "Installed:" << RESET << " " << scrolls.size() << " scrolls\n";
    std::cout << "  " << CYAN << "Size:" << RESET << " " << (totalSize / 1024) << " KB\n";
    
    return 0;
}

// ─── Main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    enableAnsiColors();
    
    if (argc < 2) {
        printHelp();
        return 0;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printHelp();
        return 0;
    }
    
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        cmdVersion();
        return 0;
    }
    
    if (cmd == "init") {
        std::string name = argc > 2 ? argv[2] : "";
        return cmdInit(name);
    }
    
    if (cmd == "list" || cmd == "ls") {
        return cmdList();
    }
    
    if (cmd == "info") {
        if (argc < 3) {
            std::cerr << RED << "Error:" << RESET << " Missing scroll name.\n";
            std::cerr << "Usage: scroll info <name[@version]>\n";
            return 1;
        }
        return cmdInfo(argv[2]);
    }
    
    if (cmd == "install" || cmd == "add") {
        std::string name = argc > 2 ? argv[2] : "";
        return cmdInstall(name);
    }
    
    if (cmd == "remove" || cmd == "rm" || cmd == "uninstall") {
        if (argc < 3) {
            std::cerr << RED << "Error:" << RESET << " Missing scroll name.\n";
            std::cerr << "Usage: scroll remove <name[@version]>\n";
            return 1;
        }
        return cmdRemove(argv[2]);
    }
    
    if (cmd == "cache") {
        return cmdCache();
    }
    
    if (cmd == "publish") {
        std::cout << YELLOW << "⚠" << RESET << " Publishing not yet implemented.\n";
        return 1;
    }
    
    if (cmd == "search") {
        std::cout << YELLOW << "⚠" << RESET << " Search not yet implemented (requires registry).\n";
        return 1;
    }
    
    std::cerr << RED << "Error:" << RESET << " Unknown command '" << cmd << "'\n";
    std::cerr << "Run '" << CYAN << "scroll help" << RESET << "' for available commands.\n";
    return 1;
}

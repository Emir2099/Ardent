#ifndef ARDENT_SCROLL_LOADER_H
#define ARDENT_SCROLL_LOADER_H

#include <string>
#include <vector>

namespace scrolls {

struct ResolveResult {
    std::string path;
    bool found{false};
};

// Resolve a logical scroll name to a physical file path.
// Search order:
// 1) Relative to current working directory
// 2) ARDENT_HOME/scrolls/{core,chronicles,alchemy,heroes}/
// 3) System defaults: /usr/local/lib/ardent/scrolls/... (POSIX) or %ProgramFiles%\Ardent\scrolls (Windows)
ResolveResult resolve(const std::string& logicalName);

// Return candidate directories considered for debug/diagnostics.
std::vector<std::string> candidateRoots();

}

#endif

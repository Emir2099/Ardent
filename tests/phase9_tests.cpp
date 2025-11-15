#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

static std::string runCmd(const std::string &cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    FILE *p = POPEN(cmd.c_str(), "r");
    if (!p) return out;
    while (fgets(buf.data(), static_cast<int>(buf.size()), p)) {
        out += buf.data();
    }
    PCLOSE(p);
    return out;
}

struct TestCase { std::string name; std::string path; std::vector<std::string> expectedLines; };

int main() {
    std::string exe = "./build/ardent"; // assume run from project root with build existing
#ifdef _WIN32
    exe = ".\\build\\ardent.exe";
#endif
    std::vector<TestCase> tests = {
        {"numbers", "test_scrolls/numbers_test.ardent", {"1","2","3"}},
        {"phrases", "test_scrolls/phrases_test.ardent", {"Hello","World","Hello World"}},
        {"arithmetic", "test_scrolls/arithmetic_test.ardent", {"13","7","20","4"}},
        {"variables", "test_scrolls/variables_test.ardent", {"Vars Demo","42","7"}},
        {"let_assign", "test_scrolls/let_assign_test.ardent", {"Assign Demo","10","AB"}},
        {"if_else", "test_scrolls/if_else_test.ardent", {"If Demo","True Path!"}},
        {"while", "test_scrolls/while_test.ardent", {"While Demo","0","1","2"}},
        {"spell_declare", "test_scrolls/spell_declare_test.ardent", {"Greeting Ada","Returned Ada"}},
        {"spell_return", "test_scrolls/spell_return_test.ardent", {"Echoing Rune","Rune"}},
        {"combination", "test_scrolls/combination_test.ardent", {"Combo Demo","Inner X","Return X","7","CDE"}},
    };

    int failures = 0; int passes = 0;
    for (auto &tc : tests) {
        std::string cmdInterp = exe + " --interpret --quiet-assign " + tc.path;
        std::string cmdJIT    = exe + " --llvm --quiet-assign " + tc.path;
        std::string outInterp = runCmd(cmdInterp);
        std::string outJIT    = runCmd(cmdJIT);
        auto normalize = [](const std::string &s){
            std::vector<std::string> lines; std::istringstream iss(s);
            std::string line; while (std::getline(iss, line)) { if(!line.empty() && line.back()=='\r') line.pop_back(); if(!line.empty()) lines.push_back(line); }
            return lines;
        };
        auto linesInterp = normalize(outInterp);
        auto linesJIT = normalize(outJIT);
        bool ok = true;
        // Check expected lines subset and equality between modes
        if (linesInterp != linesJIT) ok = false;
        if (ok && linesInterp.size() != tc.expectedLines.size()) ok = false;
        if (ok) {
            for (size_t i=0;i<tc.expectedLines.size();++i) {
                if (linesInterp[i] != tc.expectedLines[i]) { ok=false; break; }
            }
        }
        if (ok) { ++passes; std::cout << "[PASS] " << tc.name << "\n"; }
        else {
            ++failures;
            std::cout << "[FAIL] " << tc.name << "\n";
            std::cout << "  Interp lines (" << linesInterp.size() << "):\n";
            for (auto &l : linesInterp) std::cout << "    " << l << "\n";
            std::cout << "  JIT lines (" << linesJIT.size() << "):\n";
            for (auto &l : linesJIT) std::cout << "    " << l << "\n";
            std::cout << "  Expected (" << tc.expectedLines.size() << "):\n";
            for (auto &l : tc.expectedLines) std::cout << "    " << l << "\n";
        }
    }
    std::cout << "Summary: " << passes << " passed, " << failures << " failed." << std::endl;
    return failures == 0 ? 0 : 1;
}

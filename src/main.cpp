#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "interpreter.h"
#include "optimizer.h"
#include <memory>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <unordered_set>
#include <set>
#include <cctype>
#include "avm/opcode.h"
#include "avm/bytecode.h"
#include "avm/vm.h"
#include "avm/compiler_avm.h"
#include "avm/disassembler.h"
#include "version.h"
#include "scroll_loader.h"
#ifdef ARDENT_ENABLE_LLVM
#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#ifndef LLVM_SUPPORT_HOST_H
namespace llvm { namespace sys { std::string getDefaultTargetTriple(); } }
#endif
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/Mangling.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include "irgen/compiler_ir.h"
#include "runtime/ardent_runtime.h"
#endif
#ifdef ARDENT_ENABLE_LLD
#include <lld/Common/Driver.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#if defined(ARDENT_ENABLE_LLVM) && (defined(__MINGW32__) || defined(__MINGW64__))
extern "C" void __main();
#endif

// Helper to parse an optional Prologue header and strip it before lexing
static std::string stripPrologue(const std::string& src, std::optional<ScrollPrologue>& outMeta) {
    std::vector<std::string> lines; lines.reserve(src.size()/16+1);
    std::string line; std::istringstream iss(src);
    while (std::getline(iss, line)) { if (!line.empty() && line.back()=='\r') line.pop_back(); lines.push_back(line); }
    size_t n = lines.size(); size_t i = 0;
    while (i<n && lines[i].find_first_not_of(" \t") == std::string::npos) ++i; if (i>=n) return src;
    std::string first = lines[i].substr(lines[i].find_first_not_of(" \t"));
    if (first.rfind("Prologue", 0) != 0) return src;
    ScrollPrologue meta; bool have=false; size_t j = i+1;
    auto trim = [](std::string s){ auto s1=s.find_first_not_of(" \t"); auto e1=s.find_last_not_of(" \t"); if (s1==std::string::npos) return std::string(); return s.substr(s1, e1-s1+1); };
    for (; j<n; ++j) {
        const std::string &L = lines[j]; if (L.find_first_not_of(" \t") == std::string::npos) break;
        auto pos = L.find(':'); if (pos==std::string::npos) continue; std::string k = trim(L.substr(0,pos)); std::string v = trim(L.substr(pos+1));
        if (k=="Title") { meta.title=v; have=true; } else if (k=="Version") { meta.version=v; have=true; } else if (k=="Author") { meta.author=v; have=true; } else { meta.extras[k]=v; have=true; }
    }
    if (j<n && lines[j].find_first_not_of(" \t") == std::string::npos) ++j;
    std::ostringstream out; for (size_t k=j;k<n;++k) out<<lines[k]<<"\n";
    if (have) outMeta = meta; else outMeta.reset();
    return out.str();
}

static void runArdentProgram(const std::string& code, const std::string& sourceName = std::string("<inline>"), bool enableOpt = true) {
    std::optional<ScrollPrologue> metaOpt;
    std::string filtered = stripPrologue(code, metaOpt);
    Lexer lexer(filtered);
    auto tokens = lexer.tokenize();
    Arena astArena; // arena for this one-shot program parse
    Parser parser(tokens, &astArena);
    auto ast = parser.parse();
    if (!ast) {
        std::cerr << "Error: Parser returned NULL AST!" << std::endl;
        return;
    }
    // Run optimizer passes (constant folding, purity analysis, partial evaluation)
    if (enableOpt) {
        opt::Optimizer optimizer;
        ast = optimizer.optimize(ast);
    }
    Interpreter interpreter;
    interpreter.setSourceName(sourceName);
    if (metaOpt) interpreter.setCurrentPrologue(*metaOpt);
    interpreter.execute(ast);
}

static bool initWindowsConsole(bool wantVT) {
#ifdef _WIN32
    // Set UTF-8 code page for input/output
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    if (wantVT) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
                if (!SetConsoleMode(hOut, mode)) {
                    return false;
                }
            }
        }
    }
    return true;
#else
    (void)wantVT;
    return true;
#endif
}

static std::string nowTimestamp() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Windows interactive input with history and basic autocomplete
#ifdef _WIN32
static std::string readInteractiveLine(const std::string& prompt, std::vector<std::string>& history, size_t& histIndex,
                                       const Interpreter& interp, bool colorize) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD origMode = 0; GetConsoleMode(hIn, &origMode);
    SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT);
    auto printPrompt = [&](){ std::cout << prompt; };
    printPrompt();
    std::string buffer;
    auto redraw = [&](){
        std::cout << '\r' << prompt << buffer << "    ";
        // move cursor to end
        // no-op; cursor is at end after print
    };
    INPUT_RECORD rec; DWORD read = 0;
    while (ReadConsoleInput(hIn, &rec, 1, &read)) {
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) continue;
        auto ke = rec.Event.KeyEvent;
        WORD vk = ke.wVirtualKeyCode;
        DWORD mods = ke.dwControlKeyState;
        if (vk == VK_RETURN) {
            if (mods & SHIFT_PRESSED) {
                buffer.push_back('\n');
                std::cout << '\n';
                printPrompt();
            } else {
                std::cout << "\n";
                if (!buffer.empty() && (history.empty() || history.back() != buffer)) {
                    history.push_back(buffer);
                }
                histIndex = history.size();
                break;
            }
        } else if (vk == VK_BACK) {
            if (!buffer.empty()) {
                buffer.pop_back();
                std::cout << '\b' << ' ' << '\b';
            }
        } else if (vk == VK_TAB) {
            // Autocomplete on last token [A-Za-z0-9_.]
            size_t i = buffer.size();
            while (i>0) {
                char c = buffer[i-1];
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c=='_' || c=='.')) break;
                --i;
            }
            std::string prefix = buffer.substr(i);
            if (!prefix.empty()) {
                std::vector<std::string> options;
                for (auto &n : const_cast<Interpreter&>(interp).getVariableNames()) if (n.rfind(prefix,0)==0) options.push_back(n);
                for (auto &n : const_cast<Interpreter&>(interp).getSpellNames()) if (n.rfind(prefix,0)==0) options.push_back(n);
                if (options.size() == 1) {
                    buffer.erase(i);
                    buffer += options[0];
                    redraw();
                } else if (!options.empty()) {
                    std::cout << "\n";
                    for (auto &opt : options) {
                        if (colorize) std::cout << "  \x1b[90m";
                        std::cout << opt;
                        if (colorize) std::cout << "\x1b[0m";
                        std::cout << "\n";
                    }
                    printPrompt();
                    std::cout << buffer;
                }
            }
        } else if (vk == VK_UP) {
            if (!history.empty() && histIndex>0) {
                --histIndex; buffer = history[histIndex]; redraw();
            }
        } else if (vk == VK_DOWN) {
            if (!history.empty() && histIndex+1 < history.size()) { ++histIndex; buffer = history[histIndex]; redraw(); }
            else { histIndex = history.size(); buffer.clear(); redraw(); }
        } else {
            // Printable characters
            wchar_t wc = ke.uChar.UnicodeChar;
            if (wc >= 32) {
                char utf8[4] = {0};
                // naive ASCII-only for now
                buffer.push_back(static_cast<char>(wc & 0xFF));
                std::cout << static_cast<char>(wc & 0xFF);
            }
        }
    }
    SetConsoleMode(hIn, origMode);
    return buffer;
}
#endif

static void startOracleMode(bool colorize, bool emoji, bool poetic) {
    // Try to prepare Windows console for UTF-8 and VT; ignore failure (we'll fall back)
#ifdef _WIN32
    bool ok = initWindowsConsole(colorize || emoji);
    if (!ok) { colorize = false; }
#endif
    Interpreter interpreter; // persistent across lines
    // Scroll logging: open append mode per session
    std::ofstream scroll("ardent_scroll.log", std::ios::app);
    int verse = 0;
    if (colorize) {
        std::cout << "\x1b[92m** The Oracle of Ardent **\x1b[0m" << std::endl;
        std::cout << "\x1b[90;3mSpeak thy words (or say 'farewell' to depart).\x1b[0m" << std::endl;
    } else {
        std::cout << "** The Oracle of Ardent **" << std::endl;
        std::cout << "Speak thy words (or say 'farewell' to depart)." << std::endl;
    }
    std::vector<std::string> history;
    size_t histIndex = 0;
    while (true) {
        std::string prompt;
        if (emoji) prompt = colorize ? "\n\x1b[96m✒️ \x1b[0m " : "\n✒️ ";
        else prompt = colorize ? "\n\x1b[96m> \x1b[0m" : "\n> ";
        std::string line;
#ifdef _WIN32
        line = readInteractiveLine(prompt, history, histIndex, interpreter, colorize);
        if (!std::cin.good() && line.empty()) break;
#else
        std::cout << prompt;
        if (!std::getline(std::cin, line)) break;
#endif
        // Log the verse input
        ++verse;
        if (scroll.good()) {
            scroll << "[" << nowTimestamp() << "] [Verse " << verse << "] "
                   << (emoji ? "✒️  " : "> ") << line << "\n";
            scroll.flush();
        }
        if (line == "farewell" || line == "exit") {
            if (colorize) std::cout << "\x1b[90;3mThe Oracle falls silent...\x1b[0m" << std::endl;
            else std::cout << "The Oracle falls silent..." << std::endl;
            break;
        }
        if (line.empty()) continue;
        try {
            // Begin REPL line: enable ephemeral line arena inside the interpreter
            interpreter.beginLine();
            interpreter.setSourceName("<repl>");
            Lexer lx(line);
            auto toks = lx.tokenize();
            // Parser uses its own ephemeral arena for AST nodes
            Arena lineAstArena; // ephemeral arena per REPL line (AST only)
            Parser p(std::move(toks), &lineAstArena);
            auto ast = p.parse();
            if (ast) {
                // Detect if last statement is an explicit proclamation to avoid double printing
                bool explicitPrint = false;
                if (auto block = std::dynamic_pointer_cast<BlockStatement>(ast)) {
                    if (!block->statements.empty()) {
                        auto &last = block->statements.back();
                        explicitPrint = (bool)std::dynamic_pointer_cast<PrintStatement>(last);
                        if (!explicitPrint) {
                            if (auto be = std::dynamic_pointer_cast<BinaryExpression>(last)) {
                                if (be->op.type == TokenType::IS_OF) {
                                    explicitPrint = true; // treat declarations/assignments as non-printing
                                }
                            }
                        }
                    }
                }
                interpreter.execute(ast);
                // Bind '_' to last result (typed if simple, phrase if complex)
                Interpreter::Value v = interpreter.evaluateReplValue(ast);
                if (std::holds_alternative<int>(v)) interpreter.assignVariable("_", std::get<int>(v));
                else if (std::holds_alternative<bool>(v)) interpreter.assignVariable("_", std::get<bool>(v));
                else if (std::holds_alternative<std::string>(v)) interpreter.assignVariable("_", std::get<std::string>(v));
                else {
                    std::string s = interpreter.stringifyValueForRepl(v);
                    interpreter.assignVariable("_", s);
                }
                // Auto-print expressions with colorization (but not proclamations)
                // Heuristic: declarative openings shouldn't auto-print in REPL
                if (!explicitPrint) {
                    std::string trimmed = line;
                    // Trim leading spaces
                    while (!trimmed.empty() && (trimmed[0]==' '||trimmed[0]=='\t')) trimmed.erase(trimmed.begin());
                    if (trimmed.rfind("Let it be known", 0) == 0) explicitPrint = true;
                }
                if (!explicitPrint) {
                    auto printColored = [&](const Interpreter::Value &val){
                        if (!colorize) {
                            if (std::holds_alternative<int>(val)) std::cout << std::get<int>(val) << std::endl;
                            else if (std::holds_alternative<std::string>(val)) std::cout << std::get<std::string>(val) << std::endl;
                            else if (std::holds_alternative<bool>(val)) std::cout << (std::get<bool>(val) ? "True" : "False") << std::endl;
                            else std::cout << interpreter.stringifyValueForRepl(val) << std::endl;
                            return;
                        }
                        if (std::holds_alternative<int>(val)) {
                            std::cout << "\x1b[96m" << std::get<int>(val) << "\x1b[0m" << std::endl; // pale cyan
                        } else if (std::holds_alternative<std::string>(val)) {
                            std::cout << "\x1b[93m" << std::get<std::string>(val) << "\x1b[0m" << std::endl; // soft gold
                        } else if (std::holds_alternative<bool>(val)) {
                            bool b = std::get<bool>(val);
                            std::cout << (b ? "\x1b[92mTrue\x1b[0m" : "\x1b[91mFalse\x1b[0m") << std::endl; // green/red
                        } else {
                            std::cout << "\x1b[90m" << interpreter.stringifyValueForRepl(val) << "\x1b[0m" << std::endl;
                        }
                    };
                    printColored(v);
                    if (poetic) {
                        if (colorize) std::cout << "\x1b[90;3m"; // grey italic
                        // Simple reflection by type
                        if (std::holds_alternative<int>(v)) {
                            std::cout << "(The numbers march, yet tell no lies.)";
                        } else if (std::holds_alternative<std::string>(v)) {
                            std::cout << "(Words, like silk, bind thought to breath.)";
                        } else if (std::holds_alternative<bool>(v)) {
                            std::cout << (std::get<bool>(v) ? "(Truth stands; the candle does not flicker.)" : "(Falsehood settles like dust upon the floor.)");
                        } else {
                            std::cout << "(Shapes and ledgers whisper of hidden order.)";
                        }
                        if (colorize) std::cout << "\x1b[0m";
                        std::cout << std::endl;
                    }
                }
            }
            // End REPL line: promote persistent values and reset line arena
            interpreter.endLine();
        } catch (const std::exception& e) {
            // Ensure we end the line even if an exception occurred
            interpreter.endLine();
            if (colorize) std::cerr << "\x1b[33m⚠️  \x1b[0m" << " " << e.what() << std::endl;
            else std::cerr << "⚠️  " << e.what() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    // Ensure Windows console uses UTF-8 for consistent glyph rendering
    initWindowsConsole(true);
    // Oracle mode: --oracle or -o, optional --color / --no-color, --emoji / --no-emoji
    bool wantOracle = false;
    bool wantVmRepl = false; // AVM REPL: compile each line, retain globals
    bool wantReplAlias = false; // --repl alias for VM REPL
    bool printVersion = false;
    bool compileOnly = false; // --compile
    std::string compileOutPath; // -o path for compile
    bool interpretMode = false; // --interpret
    bool bannerOnly = false; // --banner
    bool wantHelp = false; // --help
    bool wantBench = false; // --bench
    bool wantLint = false; // --lint
    bool wantPretty = false; // --pretty
    bool wantScrollList = false; // --scrolls list available stdlib scrolls
    bool wantDemo = false; // --demo run a short poetic sample
    bool vmDemo = false;   // run minimal AVM demo with hand-authored bytecode
    bool vmMode = false;   // compile provided scroll (or demo) to bytecode & run
    bool vmDisasm = false; // compile and disassemble the bytecode or disassemble a .avm file
    bool vmSaveAvm = false; // when compiling, save the chunk to a .avm file
    std::string vmSavePath;
    // LLVM modes
    bool wantLLVMJIT = false;     // --llvm <file>
    bool wantEmitLLVM = false;    // --emit-llvm <file>
    bool wantAOT = false;         // --aot <file> -o out.exe
    std::string aotOutPath;       // -o path for AOT
    bool wantEmitObject = false;  // --emit-o <file>
    std::string targetOverride;   // --target <triple>
    bool colorize = true;  // default color highlight on
    bool emoji = true; // default to emoji prompt
    bool poetic = false; // default off
    bool chroniclesOnly = false; // run only the Chronicle Rites demo
    bool quietAssign = false; // --quiet-assign suppress variable assignment logs
    bool noOptimize = false; // --no-optimize disable constant folding / purity analysis
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // Reserve '-o' for output path; oracle mode uses only long form now
        if (arg == "--oracle") wantOracle = true;
        else if (arg == "--vm-repl") wantVmRepl = true;
        else if (arg == "--repl") { wantVmRepl = true; wantReplAlias = true; }
        else if (arg == "--version") printVersion = true;
        else if (arg == "--compile") compileOnly = true;
        else if (arg == "-o" && i + 1 < argc) {
            // Single unified -o: if AOT requested, treat as executable path, else compile output
            if (wantAOT) aotOutPath = argv[++i]; else compileOutPath = argv[++i];
        }
        else if (arg == "--interpret") interpretMode = true;
        else if (arg == "--banner") bannerOnly = true;
        else if (arg == "--help") wantHelp = true;
        else if (arg == "--bench") wantBench = true;
        else if (arg == "--lint") wantLint = true;
        else if (arg == "--pretty") wantPretty = true;
        else if (arg == "--scrolls") wantScrollList = true;
        else if (arg == "--demo") wantDemo = true;
        else if (arg == "--vm-demo") vmDemo = true;
        else if (arg == "--vm") vmMode = true;
        else if (arg == "--disassemble" || arg == "--vm-disasm") vmDisasm = true;
        else if (arg == "--save-avm" && i + 1 < argc) { vmSaveAvm = true; vmSavePath = argv[++i]; }
        else if (arg == "--llvm") wantLLVMJIT = true;
        else if (arg == "--emit-llvm") wantEmitLLVM = true;
        else if (arg == "--emit-o") wantEmitObject = true;
        else if (arg == "--aot") wantAOT = true;
        else if (arg == "--target" && i + 1 < argc) { targetOverride = argv[++i]; }
        else if (arg == "--no-optimize" || arg == "--no-opt") noOptimize = true;
        // (Removed duplicate -o handler for AOT/compile)
        else if (arg == "--color") colorize = true;
        else if (arg == "--no-color") colorize = false;
        else if (arg == "--emoji") emoji = true;
        else if (arg == "--no-emoji") emoji = false;
        else if (arg == "--poetic") poetic = true;
        else if (arg == "--chronicles-demo") chroniclesOnly = true;
        else if (arg == "--quiet-assign") quietAssign = true;
    }
    if (printVersion) {
        initWindowsConsole(true);
        std::cout << "Ardent " << ARDENT_VERSION << " — \"" << ARDENT_CODENAME << "\"\n";
        std::cout << "Forged with poetic precision on " << ARDENT_BUILD_DATE << "\n";
        std::cout << "Commit: " << ARDENT_BUILD_HASH << "\n";
        return 0;
    }

    if (bannerOnly) {
        initWindowsConsole(true);
        // Simplified, unambiguous banner (avoid misreading as ONANET in some fonts)
        std::cout << "========================================\n";
        std::cout << "              A R D E N T               \n";
        std::cout << "========================================\n";
        std::cout << "Version: " << ARDENT_VERSION << "  Codename: \"" << ARDENT_CODENAME << "\"\n";
        std::cout << "Build Date: " << ARDENT_BUILD_DATE << "  Commit: " << ARDENT_BUILD_HASH << "\n";
        return 0;
    }

    if (wantHelp) {
        std::cout << "Usage: ardent [mode] [flags] [file]\n";
        std::cout << "  --interpret <file>   Interpret a source scroll in classic mode.\n";
        std::cout << "  --compile -o out.avm <file>  Compile scroll to bytecode (.avm).\n";
        std::cout << "  --vm <file|.avm>     Run in the Virtual Ember (compile or load).\n";
        std::cout << "  --repl / --oracle    Poetic interactive REPL.\n";
        std::cout << "  --disassemble <file|.avm>  Show bytecode listing.\n";
        std::cout << "  --llvm <file>         Compile to LLVM IR and run via JIT.\n";
        std::cout << "  --emit-llvm <file>    Output LLVM IR (.ll) for the scroll.\n";
        std::cout << "  --emit-o <file>       Output only the object file (AOT stage 1).\n";
        std::cout << "  --aot <file> -o out   Ahead-of-time compile to native (experimental).\n";
        std::cout << "  --target <triple>     Override target triple for AOT/object emission.\n";
        std::cout << "  --bench              Measure the swiftness of your spells.\n";
        std::cout << "  --lint               Inspect scrolls for structural blemishes.\n";
        std::cout << "  --pretty             Beautify and reindent Ardent verses.\n";
        std::cout << "  --scrolls            List available standard library scrolls.\n";
        std::cout << "  --demo               Run a brief poetic showcase.\n";
        std::cout << "  --banner             Print logo + version only.\n";
        std::cout << "  --version            Display Ardent version and codename.\n";
        std::cout << "  --quiet-assign       Suppress 'Variable assigned:' lines (test parity).\n";
        std::cout << "  --no-optimize        Disable constant folding / purity analysis.\n";
        return 0;
    }
    // Apply quiet assignment flag globally so interpreter paths honor it
    extern bool gQuietAssign; gQuietAssign = quietAssign;
    if (wantBench) {
        // --bench <file>: execute scroll, measure time and arena allocations
        std::string path; for (int i=1;i<argc;++i) if (argv[i][0] != '-') { path = argv[i]; break; }
        if (path.empty()) { std::cerr << "Provide a scroll path for benchmarking." << std::endl; return 1; }
        std::ifstream f(path); if (!f.is_open()) { std::cerr << "The scroll cannot be found at this path: '" << path << "'." << std::endl; return 1; }
        std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Strip optional Prologue before lexing
        std::optional<ScrollPrologue> metaOpt;
        source = stripPrologue(source, metaOpt);
        // Parse using a dedicated AST arena to measure AST memory
        Lexer lx(source);
        auto toks = lx.tokenize();
        Arena astArena;
        Parser parser(std::move(toks), &astArena);
        auto ast = parser.parse();
        if (!ast) { std::cerr << "Error: Parser returned NULL AST!" << std::endl; return 1; }
        std::size_t astBytes = astArena.bytesUsed();
        // Execute and time
        Interpreter interpreter;
        interpreter.setSourceName(path);
        auto t0 = std::chrono::high_resolution_clock::now();
        interpreter.execute(ast);
        auto t1 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        double secs = static_cast<double>(dur) / 1'000'000.0;
        std::size_t memBytes = interpreter.bytesUsed() + astBytes;
        // Format output
        auto fmtBytes = [](std::size_t n) {
            std::string s = std::to_string(n);
            for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
            return s;
        };
        std::cout.setf(std::ios::fixed); std::cout.precision(3);
        std::cout << "\xE2\x8F\xB3  Scroll completed in " << secs << "s\n";
        std::cout << "Memory consumed: " << fmtBytes(memBytes) << " bytes" << std::endl;
        return 0;
    }

    if (wantScrollList) {
        std::cout << "Available scroll roots:\n";
        auto roots = scrolls::candidateRoots();
        for (auto &r : roots) {
            std::error_code ec; if (!std::filesystem::exists(r, ec)) continue;
            std::cout << "  Root: " << r << "\n";
            int count=0;
            for (auto &entry : std::filesystem::directory_iterator(r, ec)) {
                if (ec) break; if (!entry.is_regular_file()) continue;
                auto p = entry.path(); auto ext = p.extension().string();
                if (ext == ".ardent" || ext == ".avm") {
                    std::cout << "    - " << p.filename().string();
                    if (ext == ".avm") std::cout << " (bytecode)";
                    std::cout << '\n'; ++count;
                }
            }
            if (count == 0) std::cout << "    (none found)\n";
        }
        return 0;
    }

    if (wantDemo) {
        std::string demo =
            "Let it be proclaimed: \"--- Ardent Demo ---\"\n"
            "Let it be known throughout the land, a phrase named hero is of \"Aragorn\".\n"
            "By decree of the elders, a spell named hail is cast upon a traveler known as name:\n"
            "    Let it be proclaimed: \"Hail, noble \" + name + \"!\"\n"
            "Invoke the spell hail upon hero\n"
            "Let it be known throughout the land, a number named a is of 2 winters.\n"
            "Let it be known throughout the land, a number named b is of 3 winters.\n"
            "Should the fates decree that a is lesser than b then Let it be proclaimed: \"a<b\" Else whisper \"a>=b\"\n"
            "Inscribe upon \"demo.txt\" the words \"A tale begins.\"\n"
            "Let it be known throughout the land, a phrase named lines is of reading from \"demo.txt\".\n"
            "Let it be proclaimed: lines\n"
            "Banish the scroll \"demo.txt\".\n";
        runArdentProgram(demo, "<demo>");
        return 0;
    }

    if (wantLint) {
        // --lint <file>: warn on unused variables/spells, unreachable branches, poetic redundancy
        std::string path; for (int i=1;i<argc;++i) if (argv[i][0] != '-') { path = argv[i]; break; }
        if (path.empty()) { std::cerr << "Provide a scroll path for linting." << std::endl; return 1; }
        std::ifstream f(path); if (!f.is_open()) { std::cerr << "Cannot open scroll: " << path << std::endl; return 1; }
        std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Strip optional Prologue before lexing
        std::optional<ScrollPrologue> metaOpt;
        source = stripPrologue(source, metaOpt);
        Lexer lx(source); auto toks = lx.tokenize();
        Arena astArena; Parser parser(std::move(toks), &astArena); auto ast = parser.parse();
        if (!ast) { std::cerr << "Lint: parse failed." << std::endl; return 1; }

        struct Lint {
            std::set<std::string> spellsDeclared;
            std::set<std::string> spellsInvoked;
            std::set<std::string> globalsDeclared;
            std::set<std::string> globalsUsed;
            std::vector<std::string> warnings;
            // per-spell collections
            struct SpellInfo { std::set<std::string> varsDeclared; std::set<std::string> varsUsed; };
            std::unordered_map<std::string, SpellInfo> perSpell;
        } L;

        // Simple constexpr evaluator for if conditions
        std::function<std::optional<long long>(std::shared_ptr<ASTNode>)> evalInt;
        evalInt = [&](std::shared_ptr<ASTNode> n)->std::optional<long long>{
            if (!n) return std::nullopt;
            if (auto e = std::dynamic_pointer_cast<Expression>(n)) {
                if (e->token.type == TokenType::NUMBER) { try { return std::stoll(e->token.value); } catch(...) { return 0; } }
                if (e->token.type == TokenType::BOOLEAN) return e->token.value == "True" ? 1 : 0;
                return std::nullopt; // identifiers unknown at lint time
            }
            if (auto un = std::dynamic_pointer_cast<UnaryExpression>(n)) {
                auto v = evalInt(un->operand); if (!v) return std::nullopt; if (un->op.type == TokenType::NOT) return (*v) ? 0 : 1; return v;
            }
            if (auto b = std::dynamic_pointer_cast<BinaryExpression>(n)) {
                auto Lv = evalInt(b->left); auto Rv = evalInt(b->right);
                if (!Lv || !Rv) return std::nullopt;
                switch (b->op.type) {
                    case TokenType::AND: return ((*Lv)!=0 && (*Rv)!=0) ? 1:0;
                    case TokenType::OR:  return ((*Lv)!=0 || (*Rv)!=0) ? 1:0;
                    case TokenType::SURPASSETH: return (*Lv > *Rv) ? 1:0;
                    case TokenType::REMAINETH:  return (*Lv < *Rv) ? 1:0;
                    case TokenType::EQUAL:      return (*Lv == *Rv) ? 1:0;
                    case TokenType::NOT_EQUAL:  return (*Lv != *Rv) ? 1:0;
                    case TokenType::GREATER:    return (*Lv > *Rv) ? 1:0;
                    case TokenType::LESSER:     return (*Lv < *Rv) ? 1:0;
                    default: break;
                }
                // arithmetic tokens by value
                if (b->op.type == TokenType::OPERATOR) {
                    if (b->op.value == "+") return *Lv + *Rv;
                    if (b->op.value == "-") return *Lv - *Rv;
                    if (b->op.value == "*") return *Lv * *Rv;
                    if (b->op.value == "/") { if (*Rv == 0) return 0; return *Lv / *Rv; }
                    if (b->op.value == "%") { if (*Rv == 0) return 0; return *Lv % *Rv; }
                }
                return std::nullopt;
            }
            if (auto c = std::dynamic_pointer_cast<CastExpression>(n)) {
                auto v = evalInt(c->operand); if (!v) return std::nullopt; if (c->target == CastTarget::ToPhrase) return 0; return (*v);
            }
            return std::nullopt;
        };

        // AST walk with scope tracking
        std::vector<std::string> spellStack;
        std::function<void(std::shared_ptr<ASTNode>)> walk;
        walk = [&](std::shared_ptr<ASTNode> n){
            if (!n) return;
            if (auto blk = std::dynamic_pointer_cast<BlockStatement>(n)) {
                bool seenReturn = false;
                std::string currentSpell = spellStack.empty() ? std::string() : spellStack.back();
                for (size_t i=0;i<blk->statements.size();++i) {
                    auto &s = blk->statements[i];
                    if (seenReturn) {
                        std::string msg = "🪶  Warning: Unreachable statement after return";
                        if (!currentSpell.empty()) msg += " in spell '" + currentSpell + "'."; else msg += ".";
                        L.warnings.push_back(msg);
                        // still walk to catch nested constructs
                        walk(s);
                        continue;
                    }
                    if (std::dynamic_pointer_cast<ReturnStatement>(s)) {
                        seenReturn = true;
                    }
                    walk(s);
                }
                return;
            }
            if (auto sp = std::dynamic_pointer_cast<SpellStatement>(n)) {
                L.spellsDeclared.insert(sp->spellName);
                spellStack.push_back(sp->spellName);
                walk(sp->body);
                spellStack.pop_back();
                return;
            }
            if (auto inv = std::dynamic_pointer_cast<SpellInvocation>(n)) {
                L.spellsInvoked.insert(inv->spellName);
                for (auto &a : inv->args) walk(a);
                return;
            }
            if (auto pr = std::dynamic_pointer_cast<PrintStatement>(n)) { walk(pr->expression); return; }
            if (auto si = std::dynamic_pointer_cast<NativeInvocation>(n)) { for (auto &a: si->args) walk(a); return; }
            if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(n)) { for (auto &e: arr->elements) walk(e); return; }
            if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(n)) { for (auto &kv: obj->entries) walk(kv.second); return; }
            if (auto idx = std::dynamic_pointer_cast<IndexExpression>(n)) { walk(idx->target); walk(idx->index); return; }
            if (auto un = std::dynamic_pointer_cast<UnaryExpression>(n)) { walk(un->operand); return; }
            if (auto cast = std::dynamic_pointer_cast<CastExpression>(n)) { walk(cast->operand); return; }
            if (auto ifs = std::dynamic_pointer_cast<IfStatement>(n)) {
                // constant condition detection
                auto v = evalInt(ifs->condition);
                if (v) {
                    if ((*v) != 0 && ifs->elseBranch) {
                        L.warnings.push_back("🪶  Warning: Unreachable else-branch (condition always True).");
                    } else if ((*v) == 0) {
                        L.warnings.push_back("🪶  Warning: Unreachable then-branch (condition always False).");
                    }
                }
                walk(ifs->condition); walk(ifs->thenBranch); walk(ifs->elseBranch); return;
            }
            if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(n)) {
                if (bin->op.type == TokenType::IS_OF) {
                    // redundancy: is of is of
                    if (std::dynamic_pointer_cast<BinaryExpression>(bin->right) &&
                        std::dynamic_pointer_cast<BinaryExpression>(bin->right)->op.type == TokenType::IS_OF) {
                        std::string lhsName = "it";
                        if (auto lhs = std::dynamic_pointer_cast<Expression>(bin->left)) lhsName = lhs->token.value;
                        L.warnings.push_back("🪶  Warning: Poetic redundancy: 'is of is of' in declaration of '" + lhsName + "'.");
                    }
                    // declaration tracking
                    std::string var;
                    if (auto lhs = std::dynamic_pointer_cast<Expression>(bin->left)) var = lhs->token.value;
                    if (!var.empty()) {
                        if (spellStack.empty()) L.globalsDeclared.insert(var);
                        else L.perSpell[spellStack.back()].varsDeclared.insert(var);
                    }
                    walk(bin->right);
                    return;
                }
                walk(bin->left); walk(bin->right); return;
            }
            if (auto e = std::dynamic_pointer_cast<Expression>(n)) {
                if (e->token.type == TokenType::IDENTIFIER) {
                    // variable use
                    if (spellStack.empty()) L.globalsUsed.insert(e->token.value);
                    else L.perSpell[spellStack.back()].varsUsed.insert(e->token.value);
                }
                return;
            }
        };

        walk(ast);

        // Unused spells
        for (const auto &name : L.spellsDeclared) {
            if (L.spellsInvoked.find(name) == L.spellsInvoked.end()) {
                L.warnings.push_back("🪶  Warning: The spell '" + name + "' is declared but never invoked.");
            }
        }
        // Unused globals
        for (const auto &name : L.globalsDeclared) {
            if (L.globalsUsed.find(name) == L.globalsUsed.end()) {
                L.warnings.push_back("🪶  Warning: The variable '" + name + "' is declared but never used.");
            }
        }
        // Unused locals per spell
        for (auto &p : L.perSpell) {
            for (const auto &name : p.second.varsDeclared) {
                if (p.second.varsUsed.find(name) == p.second.varsUsed.end()) {
                    L.warnings.push_back("🪶  Warning: In spell '" + p.first + "', the variable '" + name + "' is declared but never used.");
                }
            }
        }

        // Emit warnings (stable order)
        for (const auto &w : L.warnings) std::cout << w << std::endl;
        return 0;
    }

    if (wantPretty) {
        std::string path; for (int i=1;i<argc;++i) if (argv[i][0] != '-') { path = argv[i]; break; }
        if (path.empty()) { std::cerr << "Provide a scroll path for pretty printing." << std::endl; return 1; }
        std::ifstream f(path); if (!f.is_open()) { std::cerr << "Cannot open scroll: " << path << std::endl; return 1; }
        std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Strip optional Prologue before lexing
        std::optional<ScrollPrologue> metaOpt;
        source = stripPrologue(source, metaOpt);
        Lexer lx(source); auto toks = lx.tokenize(); Arena astArena; Parser parser(std::move(toks), &astArena); auto ast = parser.parse(); if (!ast) { std::cerr << "Pretty: parse failed." << std::endl; return 1; }
        int indent = 0; auto pad=[&](){ for(int i=0;i<indent;++i) std::cout << "    "; };
        std::function<void(std::shared_ptr<ASTNode>)> pStmt; std::function<std::string(std::shared_ptr<ASTNode>)> pExpr;
        pExpr = [&](std::shared_ptr<ASTNode> n)->std::string{
            if (!n) return "";
            if (auto e = std::dynamic_pointer_cast<Expression>(n)) {
                if (e->token.type == TokenType::STRING) return std::string("\"") + e->token.value + "\"";
                return e->token.value;
            } else if (auto b = std::dynamic_pointer_cast<BinaryExpression>(n)) {
                return pExpr(b->left) + " " + b->op.value + " " + pExpr(b->right);
            } else if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(n)) {
                std::string s = "["; for (size_t i=0;i<arr->elements.size();++i){ if(i) s += ", "; s += pExpr(arr->elements[i]); } s += "]"; return s;
            } else if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(n)) {
                std::string s = "{"; for (size_t i=0;i<obj->entries.size();++i){ if(i) s += ", "; s += std::string("\"") + obj->entries[i].first + "\": " + pExpr(obj->entries[i].second);} s += "}"; return s;
            } else if (auto si = std::dynamic_pointer_cast<SpellInvocation>(n)) {
                std::string s = "Invoke the spell " + si->spellName + " upon ";
                for (size_t i=0;i<si->args.size();++i){ if(i) s += ", "; s += pExpr(si->args[i]); }
                return s;
            } else if (auto ni = std::dynamic_pointer_cast<NativeInvocation>(n)) {
                std::string s = "Invoke the spirit of " + ni->funcName + " upon ";
                for (size_t i=0;i<ni->args.size();++i){ if(i) s += ", "; s += pExpr(ni->args[i]); }
                return s;
            }
            return "";
        };
        pStmt = [&](std::shared_ptr<ASTNode> n){
            if (!n) return;
            if (auto blk = std::dynamic_pointer_cast<BlockStatement>(n)) {
                for (auto &s : blk->statements) pStmt(s);
            } else if (auto pr = std::dynamic_pointer_cast<PrintStatement>(n)) {
                pad(); std::cout << "Let it be proclaimed: " << pExpr(pr->expression) << "\n";
            } else if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(n)) {
                if (bin->op.type == TokenType::IS_OF) {
                    // Try infer type from RHS literal shape
                    std::string typeWord = "a thing named";
                    if (auto e = std::dynamic_pointer_cast<Expression>(bin->right)) {
                        if (e->token.type == TokenType::NUMBER) typeWord = "a number named";
                        else if (e->token.type == TokenType::STRING) typeWord = "a phrase named";
                        else if (e->token.type == TokenType::BOOLEAN) typeWord = "a truth named";
                    } else if (std::dynamic_pointer_cast<ArrayLiteral>(bin->right)) typeWord = "an order named";
                    else if (std::dynamic_pointer_cast<ObjectLiteral>(bin->right)) typeWord = "a tome named";
                    auto lhs = std::dynamic_pointer_cast<Expression>(bin->left);
                    pad(); std::cout << "Let it be known throughout the land, " << typeWord << " " << (lhs?lhs->token.value:std::string("it")) << " is of " << pExpr(bin->right) << ".\n";
                } else {
                    pad(); std::cout << pExpr(n) << "\n";
                }
            } else if (auto ifs = std::dynamic_pointer_cast<IfStatement>(n)) {
                pad(); std::cout << "Should the fates decree " << pExpr(ifs->condition) << " then\n";
                indent++; pStmt(ifs->thenBranch); indent--;
                if (ifs->elseBranch) { pad(); std::cout << "Else\n"; indent++; pStmt(ifs->elseBranch); indent--; }
            } else if (auto sp = std::dynamic_pointer_cast<SpellStatement>(n)) {
                pad(); std::cout << "By decree of the elders, a spell named " << sp->spellName << " is cast upon ";
                for (size_t i=0;i<sp->params.size();++i){ if(i) std::cout << ", "; std::cout << "a traveler known as " << sp->params[i]; }
                std::cout << ":\n"; indent++; pStmt(sp->body); indent--; 
            } else if (auto si = std::dynamic_pointer_cast<SpellInvocation>(n)) {
                pad(); std::cout << pExpr(n) << "\n";
            } else if (auto ni = std::dynamic_pointer_cast<NativeInvocation>(n)) {
                pad(); std::cout << pExpr(n) << "\n";
            } else {
                pad(); std::cout << pExpr(n) << "\n";
            }
        };
        pStmt(ast);
        return 0;
    }

    if (wantOracle) {
        startOracleMode(colorize, emoji, poetic);
        return 0;
    }

    if (wantVmRepl) {
        // Lightweight AVM REPL: compiles each line to bytecode and runs on a persistent VM
        // No auto-printing; use proclamations to print results.
        initWindowsConsole(colorize || emoji);
        avm::CompilerAVM cavm; // persists symbol table across lines
        avm::VM vm;            // persists global slots across lines
        if (colorize) {
            std::cout << "\x1b[92m** Ardent AVM REPL **\x1b[0m" << std::endl;
            std::cout << "\x1b[90;3mType 'exit' or 'farewell' to leave.\x1b[0m" << std::endl;
        } else {
            std::cout << "** Ardent AVM REPL **" << std::endl;
            std::cout << "Type 'exit' or 'farewell' to leave." << std::endl;
        }
        while (true) {
            if (emoji) {
                if (colorize) std::cout << "\n\x1b[96m✒️ \x1b[0m "; else std::cout << "\n✒️ ";
            } else {
                if (colorize) std::cout << "\n\x1b[96m> \x1b[0m"; else std::cout << "\n> ";
            }
            std::string line;
            if (!std::getline(std::cin, line)) break;
            if (line == "farewell" || line == "exit") break;
            if (line.empty()) continue;
            try {
                Lexer lx(line);
                auto toks = lx.tokenize();
                Arena astArena; // ephemeral AST arena per line
                Parser parser(std::move(toks), &astArena);
                auto ast = parser.parse();
                if (!ast) continue;
                avm::Chunk chunk = cavm.compile(ast);
                auto res = vm.run(chunk);
                if (!res.ok && colorize) std::cerr << "\x1b[90;3m<execution error>\x1b[0m" << std::endl;
                else if (!res.ok) std::cerr << "<execution error>" << std::endl;
            } catch (const std::exception& e) {
                if (colorize) std::cerr << "\x1b[90;3m" << e.what() << "\x1b[0m" << std::endl;
                else std::cerr << e.what() << std::endl;
            }
        }
        return 0;
    }

    if (chroniclesOnly) {
        // Minimal, focused Chronicle Rites program
        const std::string chronicles = R"ARDENT(
        Let it be proclaimed: "--- Chronicle Rites Demo ---"
        Inscribe upon "epic.txt" the words "In the beginning, there was code."
        Let it be proclaimed: "Written epic.txt"

        Let it be known throughout the land, a phrase named lines is of reading from "epic.txt".
        Let it be proclaimed: lines

        Etch upon "epic.txt" the words "\nAnd thus Ardent was born."
        Let it be proclaimed: "Appended new verse."

        Let it be known throughout the land, a truth named exists is of Invoke the spirit of chronicles.exists upon "epic.txt".
        Let it be proclaimed: exists

        Banish the scroll "epic.txt".
        Let it be proclaimed: "Scroll destroyed."
        )ARDENT";
        runArdentProgram(chronicles);
        return 0;
    }

    // Minimal AVM demo: showcase bytecode scaffolding without compiler
    if (vmDemo) {
        // Program: print (2 + 3) and halt
        avm::BytecodeEmitter em;
        uint16_t c2 = em.addConst(2);
        uint16_t c3 = em.addConst(3);
        em.emit(avm::OpCode::OP_PUSH_CONST); em.emit_u16(c2);
        em.emit(avm::OpCode::OP_PUSH_CONST); em.emit_u16(c3);
        em.emit(avm::OpCode::OP_ADD);
        em.emit(avm::OpCode::OP_PRINT);
        em.emit(avm::OpCode::OP_HALT);
        avm::Chunk chunk = em.build();
        avm::VM vm;
        auto res = vm.run(chunk);
        return res.ok ? 0 : 1;
    }

    if (interpretMode) {
        // Scroll mode through interpreter: ardent --interpret <path>
        std::string path;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') { path = argv[i]; break; }
            if ((std::string)argv[i] == std::string("-o") || (std::string)argv[i] == std::string("--save-avm")) ++i;
        }
        if (path.empty()) { std::cerr << "Provide a scroll path: ardent --interpret <file>\n"; return 1; }
        std::ifstream f(path);
        if (!f.is_open()) { std::cerr << "The scroll cannot be found at this path: '" << path << "'.\n"; return 1; }
        std::string code((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        runArdentProgram(code, path, !noOptimize);
        return 0;
    }

    if (compileOnly && !vmDisasm) {
        // Compile source to .avm and exit: ardent --compile -o out.avm <scroll>
        std::string argPath;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') { argPath = argv[i]; break; }
            if ((std::string)argv[i] == std::string("-o") || (std::string)argv[i] == std::string("--save-avm")) ++i;
        }
        if (compileOutPath.empty()) { std::cerr << "Missing output path: use -o <file.avm> with --compile\n"; return 1; }
        if (argPath.empty()) { std::cerr << "Provide a scroll path: ardent --compile -o out.avm <file>\n"; return 1; }
        std::ifstream f(argPath);
        if (!f.is_open()) { std::cerr << "The scroll cannot be found at this path: '" << argPath << "'.\n"; return 1; }
        std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        Lexer lx(source);
        auto toks = lx.tokenize();
        Arena astArena;
        Parser parser(std::move(toks), &astArena);
        auto ast = parser.parse();
        if (!ast) { std::cerr << "Error: Parser returned NULL AST!\n"; return 1; }
        // Apply optimizer passes before bytecode compilation
        if (!noOptimize) {
            opt::Optimizer optimizer;
            ast = optimizer.optimize(ast);
        }
        avm::CompilerAVM cavm;
        avm::Chunk chunk = cavm.compile(ast);
        if (!avm_io::save_chunk(chunk, compileOutPath)) { std::cerr << "Failed to save AVM file to '" << compileOutPath << "'\n"; return 1; }
        std::cout << "Compiled \"" << argPath << "\" into " << chunk.code.size() << " bytes of bytecode.\n";
        return 0;
    }

    if (vmMode || vmDisasm) {
        // Determine if the user passed a path argument (could be .avm or source scroll)
        std::string argPath;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') { argPath = argv[i]; break; }
            // skip parameter for --save-avm
            if ((std::string)argv[i] == std::string("--save-avm")) ++i;
        }

        // If disassembling and given a .avm file, load and dump it.
        if (vmDisasm && !argPath.empty()) {
            if (avm_io::is_avm_file(argPath)) {
                avm::Chunk loaded;
                if (!avm_io::load_chunk(argPath, loaded)) {
                    std::cerr << "Failed to load AVM file: " << argPath << std::endl;
                    return 1;
                }
                std::cout << avm::disassemble(loaded);
                return 0;
            }
        }

        // Else compile source: from file if provided or from built-in demo input.
        std::string source;
        if (!argPath.empty()) {
            std::ifstream f(argPath);
            if (!f.is_open()) {
                std::cerr << "The scroll cannot be found at this path: '" << argPath << "'." << std::endl;
                return 1;
            }
            source.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        } else {
            source = "Let it be known throughout the land, a number named a is of 2 winters.\n"
                     "Let it be known throughout the land, a number named b is of 3 winters.\n"
                     "Should the fates decree that a is greater than b then Let it be proclaimed: \"gt\" Else whisper \"le\"\n"
                     "Let it be known throughout the land, a phrase named p is of \"apple\".\n"
                     "Let it be known throughout the land, a phrase named q is of \"banana\".\n"
                     "Should the fates decree that p is lesser than q then Let it be proclaimed: \"lex-true\" Else whisper \"lex-false\"\n"
                     "Let it be proclaimed: a + b";
        }
        Lexer lx(source);
        auto toks = lx.tokenize();
        Arena astArena;
        Parser parser(std::move(toks), &astArena);
        auto ast = parser.parse();
        if (!ast) {
            std::cerr << "Error: Parser returned NULL AST!" << std::endl;
            return 1;
        }
        avm::CompilerAVM cavm;
        avm::Chunk chunk = cavm.compile(ast);
        if (vmSaveAvm && !vmSavePath.empty()) {
            if (!avm_io::save_chunk(chunk, vmSavePath)) {
                std::cerr << "Failed to save AVM file to '" << vmSavePath << "'" << std::endl;
            }
        }
        if (vmDisasm) {
            std::string listing = avm::disassemble(chunk);
            std::cout << listing;
            return 0;
        } else {
            // If vmMode and user provided a .avm path, prefer loading and running it instead of compiled chunk
            if (!argPath.empty() && avm_io::is_avm_file(argPath)) {
                avm::Chunk loaded;
                if (!avm_io::load_chunk(argPath, loaded)) {
                    std::cerr << "Failed to load AVM file: " << argPath << std::endl;
                    return 1;
                }
                avm::VM vm;
                auto res = vm.run(loaded);
                return res.ok ? 0 : 1;
            }
            avm::VM vm;
            auto res = vm.run(chunk);
            return res.ok ? 0 : 1;
        }
    }

#ifdef ARDENT_ENABLE_LLVM
    // LLVM IR/JIT/AOT modes
    if (wantLLVMJIT || wantEmitLLVM || wantAOT || wantEmitObject) {
        // Determine input path
        std::string path;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') { path = argv[i]; break; }
            if ((std::string)argv[i] == std::string("-o")) ++i; // skip output param
        }
        if (path.empty()) { std::cerr << "Provide a scroll path for LLVM modes." << std::endl; return 1; }
        std::ifstream f(path); if (!f.is_open()) { std::cerr << "Cannot open scroll: " << path << std::endl; return 1; }
        std::string source((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Strip optional Prologue before lexing
        std::optional<ScrollPrologue> metaOpt; source = stripPrologue(source, metaOpt);
        Lexer lx(source); auto toks = lx.tokenize(); Arena astArena; Parser parser(std::move(toks), &astArena); auto ast = parser.parse();
        if (!ast) { std::cerr << "LLVM: parse failed." << std::endl; return 1; }
        auto root = std::dynamic_pointer_cast<BlockStatement>(ast);
        if (!root) { std::cerr << "LLVM: expected BlockStatement root." << std::endl; return 1; }

        // Build module
        using namespace llvm; using namespace llvm::orc;
        InitializeNativeTarget(); InitializeNativeTargetAsmPrinter(); InitializeNativeTargetAsmParser();
        auto ctx = std::make_unique<LLVMContext>();
        auto module = std::make_unique<Module>("ardent_module", *ctx);
        ArdentRuntime rt; IRCompiler comp(*ctx, *module, rt);
        comp.compileProgram(root.get());
        std::fprintf(stderr, "[JIT] compileProgram done\n");

        if (wantEmitLLVM) {
            // Write to <input>.ll next to the source
            std::string outPath = path + ".ll";
            std::error_code ec; raw_fd_ostream os(outPath, ec, sys::fs::OF_Text);
            if (ec) { std::cerr << "Failed to open output .ll: " << outPath << std::endl; return 1; }
            module->print(os, nullptr);
            std::cout << "Emitted LLVM IR to: " << outPath << std::endl;
            return 0;
        }

        if (wantLLVMJIT) {
            std::fprintf(stderr, "[JIT] creating LLJIT...\n");
            auto jitExpected = LLJITBuilder().create();
            if (!jitExpected) { std::cerr << "Failed to create LLJIT: " << toString(jitExpected.takeError()) << std::endl; return 1; }
            auto jit = std::move(*jitExpected);
            // Search current process and define absolute symbols for runtime
            {
                std::fprintf(stderr, "[JIT] adding current-process generator...\n");
                auto gen = cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(jit->getDataLayout().getGlobalPrefix()));
                jit->getMainJITDylib().addGenerator(std::move(gen));
            }
            {
                MangleAndInterner M(jit->getExecutionSession(), jit->getDataLayout());
                SymbolMap symbols;
                symbols[M("ardent_rt_add_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_add_i64), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_sub_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_sub_i64), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_mul_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_mul_i64), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_div_i64")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_div_i64), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_print_av")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_print_av), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_print_av_ptr")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_print_av_ptr), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_print_av_ptr")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_print_av_ptr), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_concat_av_ptr")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_concat_av_ptr), JITSymbolFlags::Exported);
                symbols[M("ardent_rt_version")] = ExecutorSymbolDef(ExecutorAddr::fromPtr(&ardent_rt_version), JITSymbolFlags::Exported);
                symbols[M("__main")]            = ExecutorSymbolDef(ExecutorAddr::fromPtr(&__main), JITSymbolFlags::Exported);
                std::fprintf(stderr, "[JIT] defining absolute symbols...\n");
                if (auto err = jit->getMainJITDylib().define(absoluteSymbols(symbols))) {
                    std::cerr << "Failed to define absolute symbols: " << toString(std::move(err)) << std::endl; return 1;
                }
            }
            ThreadSafeModule tsm(std::move(module), std::move(ctx));
            std::fprintf(stderr, "[JIT] adding IR module...\n");
            if (auto err = jit->addIRModule(std::move(tsm))) { std::cerr << "Failed to add module: " << toString(std::move(err)) << std::endl; return 1; }
            std::fprintf(stderr, "[JIT] looking up ardent_entry...\n");
            auto symExpected = jit->lookup("ardent_entry");
            if (!symExpected) { std::cerr << "Symbol 'ardent_entry' not found: " << toString(symExpected.takeError()) << std::endl; return 1; }
            using EntryFn = int(*)();
            EntryFn entry = symExpected->toPtr<EntryFn>();
            std::fprintf(stderr, "[JIT] calling ardent_entry...\n");
            std::cout << "[LLVM JIT] Invoking ardent_entry..." << std::endl;
            int rc = entry();
            std::cout << "[LLVM JIT] ardent_entry returned: " << rc << std::endl;
            return rc;
        }

        if (wantEmitObject || wantAOT) {
            // ===== Stage 1: Object file emission =====
            std::string triple = targetOverride.empty() ? llvm::sys::getDefaultTargetTriple() : targetOverride;
            module->setTargetTriple(triple);
            std::string error;
            const llvm::Target *target = llvm::TargetRegistry::lookupTarget(triple, error);
            if (!target) { std::cerr << "Target lookup failed: " << error << std::endl; return 1; }
            llvm::TargetOptions opt;
            // Modern createTargetMachine prefers optional reloc/code model parameters
            std::unique_ptr<llvm::TargetMachine> TM(target->createTargetMachine(
                triple, "generic", "", opt, std::nullopt, std::nullopt, llvm::CodeGenOptLevel::Default));
            module->setDataLayout(TM->createDataLayout());
            // Derive object file path
#ifdef _WIN32
            std::string objExt = ".obj";
#else
            std::string objExt = ".o";
#endif
            std::string baseName = path;
            // strip directory
            auto slashPos = baseName.find_last_of("/\\"); if (slashPos != std::string::npos) baseName = baseName.substr(slashPos+1);
            // strip extension
            auto dotPos = baseName.find_last_of('.'); if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
            std::string objPath = baseName + objExt;
            if (wantEmitObject && !compileOutPath.empty()) objPath = compileOutPath; // reuse -o if user passed (legacy)
            if (wantAOT && !aotOutPath.empty()) {
                // Place object next to requested exe name
                objPath = aotOutPath + objExt;
            }
            {
                llvm::legacy::PassManager pm;
                std::error_code ec;
                llvm::raw_fd_ostream dest(objPath, ec, llvm::sys::fs::OF_None);
                if (ec) { std::cerr << "Failed to open object output: " << objPath << " (" << ec.message() << ")" << std::endl; return 1; }
                if (TM->addPassesToEmitFile(pm, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
                    std::cerr << "TargetMachine cannot emit an object file for this target." << std::endl; return 1; }
                pm.run(*module);
                dest.flush();
            }
            std::cout << "Forged object scroll: " << objPath << std::endl;
            if (wantEmitObject && !wantAOT) return 0; // Object-only mode ends here
#ifdef ARDENT_ENABLE_LLD
            // ===== Stage 2: Link with LLD =====
            std::string exePath;
#ifdef _WIN32
            exePath = aotOutPath.empty() ? (baseName + ".exe") : aotOutPath;
            // Prefer MinGW g++ linking if our static runtime archive exists; fallback to lld-link.
            bool linkOk = false;
            {
                // Try common locations for the static runtime archive created by our build
                std::string rtLib;
                if (std::filesystem::exists("build/" "libardent_runtime.a")) rtLib = "build/libardent_runtime.a";
                else if (std::filesystem::exists("libardent_runtime.a")) rtLib = "libardent_runtime.a";

                if (!rtLib.empty()) {
                    std::ostringstream gcc;
                    gcc << "g++ -o " << '"' << exePath << '"'
                        << ' ' << '"' << objPath << '"'
                        << ' ' << '"' << rtLib << '"'
                        << " -static-libgcc -static-libstdc++ -lwinpthread -lkernel32";
                    int rc = std::system(gcc.str().c_str());
                    linkOk = (rc == 0) && std::filesystem::exists(exePath);
                }
            }
            if (!linkOk) {
                // Build a shell command to invoke external lld-link, adding kernel32 only.
                std::ostringstream cmd;
                cmd << "lld-link /OUT:" << '"' << exePath << '"' << ' ' << '"' << objPath << '"' << " kernel32.lib";
                int lldRc = std::system(cmd.str().c_str());
                linkOk = (lldRc == 0) && std::filesystem::exists(exePath);
                if (!linkOk) {
                    std::cerr << "LLD (lld-link) failed to link executable. If you are using MSVC, run from a Developer Command Prompt or set LIB to your Windows SDK and MSVC lib paths; if using MinGW, ensure g++ is in PATH." << std::endl;
                    return 1;
                }
            }
#elif defined(__APPLE__)
            exePath = aotOutPath.empty() ? (baseName) : aotOutPath;
            std::vector<const char*> lldArgs; lldArgs.push_back("ld64.lld");
            lldArgs.push_back("-o"); lldArgs.push_back(exePath.c_str());
            lldArgs.push_back(objPath.c_str());
            const std::array<lld::DriverDef, 1> drivers = {{{"ld64", lld::macho::link}}};
            auto lldRes = lld::lldMain(lldArgs, llvm::outs(), llvm::errs(), drivers);
            bool linkOk = std::filesystem::exists(exePath);
            if (!linkOk) { std::cerr << "LLD (MachO) failed to link executable." << std::endl; return 1; }
#else
            exePath = aotOutPath.empty() ? (baseName) : aotOutPath;
            std::vector<const char*> lldArgs; lldArgs.push_back("ld.lld");
            lldArgs.push_back("-o"); lldArgs.push_back(exePath.c_str());
            lldArgs.push_back(objPath.c_str());
            // Rely on system default libs; user may need to add -lc etc. in future.
            const std::array<lld::DriverDef, 1> drivers = {{{"ld.lld", lld::elf::link}}};
            auto lldRes = lld::lldMain(lldArgs, llvm::outs(), llvm::errs(), drivers);
            bool linkOk = std::filesystem::exists(exePath);
            if (!linkOk) { std::cerr << "LLD (ELF) failed to link executable." << std::endl; return 1; }
#endif
            std::cout << "Forged native scroll: " << exePath << std::endl;
            return 0;
#else
            std::cerr << "LLD not linked into this build; AOT linking unavailable. Object file generated." << std::endl;
            return 0;
#endif
        }
    }
#else
    if (wantLLVMJIT || wantEmitLLVM || wantAOT) {
        std::cerr << "Ardent was built without LLVM support. Reconfigure with -DARDENT_ENABLE_LLVM=ON." << std::endl;
        return 1;
    }
#endif

    // Scroll mode: ardent <path> (only when first non-flag argument is a path)
    if (argc > 1 && argv[1][0] != '-') {
        std::string path = argv[1];
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "The scroll cannot be found at this path: '" << path << "'." << std::endl;
            return 1;
        }
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        runArdentProgram(code, path);
        return 0;
    }
    
    std::string input =  R"ARDENT(
    Let it be proclaimed: "--- Core Demo ---"
    Let it be known throughout the land, a number named ct is of 0 winters.  
    Let it be known throughout the land, a number named count is of -3 winters.
    Let it be known throughout the land, a phrase named greeting is of "Hello, world!".  
    Let it be proclaimed: greeting + " How art thou?"
   
    Let it be known throughout the land, a truth named flag is of True.
    Let it be proclaimed: True
    Let it be proclaimed: flag
    Let it be known throughout the land, a truth named off is of False.
    Let it be proclaimed: off

    Let it be known throughout the land, a truth named brave is of True.
    Let it be known throughout the land, a truth named strong is of False.
    Should the fates decree brave and strong then Let it be proclaimed: "and-ok" Else whisper "and-nay"
    Should the fates decree brave or strong then Let it be proclaimed: "or-ok" Else whisper "or-nay"
    Should the fates decree not brave then Let it be proclaimed: "not-yes" Else whisper "not-no"
    Should the fates decree brave and not strong or False then Let it be proclaimed: "prec-pass" Else whisper "prec-fail"

    Let it be known throughout the land, a number named age is of 18 winters.
    Should the fates decree that age is equal to 18 then Let it be proclaimed: "Aye!" Else whisper "Nay!"
    Let it be known throughout the land, a number named cnt is of 0 winters.
    Should the fates decree that cnt is not 0 then Let it be proclaimed: "Not zero!" Else whisper "Zero!"
    Let it be known throughout the land, a number named x is of 7 winters.
    Should the fates decree that x is greater than 3 then Let it be proclaimed: "x>3" Else whisper "x<=3"
    Should the fates decree that x is lesser than 10 then Let it be proclaimed: "x<10" Else whisper "x>=10"


    Let it be known throughout the land, a number named n is of 25 winters.
    Let it be known throughout the land, a phrase named msg is of "The number is ".
    Let it be proclaimed: msg + cast n as phrase

    Let it be known throughout the land, a truth named nonzero is of cast n as truth.
    Let it be proclaimed: nonzero

    Let it be proclaimed: cast True as number

    Let it be known throughout the land, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].
    Let it be proclaimed: heroes[1]
    Let it be proclaimed: heroes[ct+2]
    Let it be proclaimed: heroes[-1]
    Let it be proclaimed: heroes[-5]
    Let it be known throughout the land, a tome named hero is of {"name": "Aragorn", "title": "King of Gondor"}.
    Let it be proclaimed: hero["title"]
    Let it be proclaimed: hero.title

    Let it be proclaimed: "--- Collection Rites Demo ---"
    Let it be known throughout the land, an order named moreHeroes is of ["Boromir", "Frodo"].
    Let it be proclaimed: moreHeroes
    Let the order moreHeroes expand with "Sam"
    Let it be proclaimed: moreHeroes
    Let the order moreHeroes remove "Boromir"
    Let it be proclaimed: moreHeroes

    Let it be known throughout the land, a tome named realm is of {name: "Gondor", ruler: "Steward"}.
    Let it be proclaimed: realm
    Let the tome realm amend "ruler" to "Aragorn"
    Let it be proclaimed: realm.ruler
    Let the tome realm erase "name"
    Let it be proclaimed: realm

    Let it be proclaimed: "(After attempting to remove absent element)"
    Let the order moreHeroes remove "Boromir"
    Let it be proclaimed: moreHeroes
    Let it be proclaimed: "(After attempting to erase missing key)"
    Let the tome realm erase "lineage"
    Let it be proclaimed: realm

    Let it be proclaimed: "--- Spell Demo ---"
    By decree of the elders a spell named greet is cast upon a traveler known as name:
        Let it be proclaimed: "Hail, noble " + name + "!"
    Invoke the spell greet upon "Aragorn"
    By decree of the elders a spell named bless is cast upon a warrior known as name:
        Let it be proclaimed: "Blessings upon thee, " + name + "."
    Invoke the spell bless upon "Faramir"
    
    By decree of the elders, a spell named bestow is cast upon a warrior known as target, a gift known as item:
        Let it be proclaimed: "Blessings upon " + target + ", bearer of " + item
    Invoke the spell bestow upon "Faramir", "the Horn of Gondor"
    
    Let it be proclaimed: "--- Return Spell Demo ---"
    By decree of the elders, a spell named bless is cast upon a warrior known as name:
        Let it be proclaimed: "Blessing " + name
        And let it return "Blessed " + name
    Let it be proclaimed: Invoke the spell bless upon "Boromir"
    Let it be known throughout the land, a phrase named result is of Invoke the spell bless upon "Gimli".
    Let it be proclaimed: result
    )ARDENT";

    // Append Scope & Shadowing Demo (non-destructive)
    input += R"ARDENT(
    
    Let it be proclaimed: "--- Scoping & Shadowing Demo ---"
    Let it be known throughout the land, a phrase named name is of "Outer".
    By decree of the elders, a spell named echo is cast upon a traveler known as name:
        Let it be proclaimed: "Inner sees " + name
    Invoke the spell echo upon "Inner"
    Let it be proclaimed: name

    Let it be proclaimed: "--- Spell Locals Isolation Demo ---"
    By decree of the elders, a spell named forge is cast upon a traveler known as who:
        Let it be known throughout the land, a phrase named temp is of "Secret".
        Let it be proclaimed: "Crafting for " + who
    Invoke the spell forge upon "Rune"
    Let it be proclaimed: temp

    Let it be proclaimed: "--- Return Non-Effect Demo ---"
    Let it be known throughout the land, a phrase named result is of "Start".
    By decree of the elders, a spell named giver is cast upon a warrior known as result:
        And let it return "Gifted " + result
    Let it be proclaimed: Invoke the spell giver upon "Inner"
    Let it be proclaimed: result

    Let it be proclaimed: "--- Global Persistence After Loop Demo ---"
    Let it be known throughout the land, a number named outer is of 0 winters.
    Whilst the sun doth rise outer remaineth below 3 so shall these words be spoken
    outer
    let outer ascend 1
    Let it be proclaimed: outer
    )ARDENT";

    // Imported Scrolls demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Imported Scrolls Demo ---"
    From the scroll of "heroes.ardent" draw all knowledge.
    Invoke the spell greet upon "Aragorn"

    Let it be proclaimed: "--- Selective Import Demo ---"
    From the scroll of "spells.ardent" take the spells bless, bestow.
    Let it be proclaimed: Invoke the spell bless upon "Boromir"

    Let it be proclaimed: "--- Alias Import Demo ---"
    From the scroll of "alchemy.ardent" draw all knowledge as alch.
    Invoke the spell alch.transmute upon "lead", "gold"

    Let it be proclaimed: "--- Unfurl Include Demo ---"
    Unfurl the scroll "legends/warriors.ardent".
    Let it be proclaimed: who
    )ARDENT";
    
    // Native Bridge demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Native Bridge Demo ---"
    Let it be proclaimed: "Sum is " + Invoke the spirit of math.add upon 2, 3
    Let it be known throughout the land, a number named s is of Invoke the spirit of math.add upon 10, 20.
    Let it be proclaimed: s
    Let it be proclaimed: "Len of 'abc' is " + Invoke the spirit of system.len upon "abc"
    )ARDENT";
    
    // Exception Rites demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Exception Rites Demo ---"
    Try:
    Invoke the spirit of math.divide upon 10, 0
    Catch the curse as omen:
    Let it be proclaimed: "Caught: " + omen

    Try:
    Invoke the spirit of math.add upon 2, 3
    Catch the curse as omen:
    Let it be proclaimed: "Should not happen"
    Finally:
    Let it be proclaimed: "All is well."

    Let it be proclaimed: "--- Nested Try Demo ---"
    Try:
    Try:
    Invoke the spirit of math.divide upon 1, 0
    Catch the curse as omen:
    Let it be proclaimed: "Inner: " + omen
    Catch the curse as outer:
    Let it be proclaimed: "Outer: " + outer
    )ARDENT";
    

    // Chronicle Rites demo (non-destructive append)
    input += R"ARDENT(

    Let it be proclaimed: "--- Chronicle Rites Demo ---"
    Inscribe upon "epic.txt" the words "In the beginning, there was code."
    Let it be proclaimed: "Written epic.txt"

    Let it be known throughout the land, a phrase named lines is of reading from "epic.txt".
    Let it be proclaimed: lines

    Etch upon "epic.txt" the words "\nAnd thus Ardent was born."
    Let it be proclaimed: "Appended new verse."

    Let it be known throughout the land, a truth named exists is of Invoke the spirit of chronicles.exists upon "epic.txt".
    Let it be proclaimed: exists

    Banish the scroll "epic.txt".
    Let it be proclaimed: "Scroll destroyed."
    )ARDENT";
    

    // Demo fallback (no arguments): keep token/AST debug visible
    {
        Lexer lexer(input);
        std::vector<Token> tokens = lexer.tokenize();
        std::cout << "=== Tokens Generated ===" << std::endl;
        for (const auto& token : tokens) {
            std::cout << "Token: " << token.value << ", Type: " << tokenTypeToString(token.type) << std::endl;
        }
    Arena astArena2; // arena for demo program
    Parser parser(tokens, &astArena2);
        auto ast = parser.parse();
        if (!ast) {
            std::cerr << "Error: Parser returned NULL AST!" << std::endl;
            return 1;
        }
        std::cout << "=== AST Debug Output ===" << std::endl;
        std::cout << typeid(*ast).name() << std::endl;
        std::cout << "Parsing complete!" << std::endl;
        Interpreter interpreter;
        interpreter.execute(ast);
    }

    return 0;
}




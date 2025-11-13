#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "arena.h"
#include "interpreter.h"
#include <memory>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include "avm/opcode.h"
#include "avm/bytecode.h"
#include "avm/vm.h"
#include "avm/compiler_avm.h"
#include "avm/disassembler.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

static void runArdentProgram(const std::string& code) {
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Arena astArena; // arena for this one-shot program parse
    Parser parser(tokens, &astArena);
    auto ast = parser.parse();
    if (!ast) {
        std::cerr << "Error: Parser returned NULL AST!" << std::endl;
        return;
    }
    Interpreter interpreter;
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
    while (true) {
        if (emoji) {
            // Preferred prompt: place one space inside the colored span and one outside.
            // Due to emoji width behavior, this yields exactly one visible gap across terminals.
            if (colorize) std::cout << "\n\x1b[96m✒️ \x1b[0m "; else std::cout << "\n✒️ ";
        } else {
            if (colorize) std::cout << "\n\x1b[96m> \x1b[0m"; else std::cout << "\n> ";
        }
        std::string line;
        if (!std::getline(std::cin, line)) break;
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
            if (colorize) std::cerr << "\x1b[90;3m" << e.what() << "\x1b[0m" << std::endl;
            else std::cerr << e.what() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    // Oracle mode: --oracle or -o, optional --color / --no-color, --emoji / --no-emoji
    bool wantOracle = false;
    bool wantVmRepl = false; // AVM REPL: compile each line, retain globals
    bool vmDemo = false;   // run minimal AVM demo with hand-authored bytecode
    bool vmMode = false;   // compile provided scroll (or demo) to bytecode & run
    bool vmDisasm = false; // compile and disassemble the bytecode or disassemble a .avm file
    bool vmSaveAvm = false; // when compiling, save the chunk to a .avm file
    std::string vmSavePath;
    bool colorize = true;  // default color highlight on
    bool emoji = true; // default to emoji prompt
    bool poetic = false; // default off
    bool chroniclesOnly = false; // run only the Chronicle Rites demo
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--oracle" || arg == "-o") wantOracle = true;
        else if (arg == "--vm-repl") wantVmRepl = true;
        else if (arg == "--vm-demo") vmDemo = true;
        else if (arg == "--vm") vmMode = true;
        else if (arg == "--disassemble" || arg == "--vm-disasm") vmDisasm = true;
        else if (arg == "--save-avm" && i + 1 < argc) { vmSaveAvm = true; vmSavePath = argv[++i]; }
        else if (arg == "--color") colorize = true;
        else if (arg == "--no-color") colorize = false;
        else if (arg == "--emoji") emoji = true;
        else if (arg == "--no-emoji") emoji = false;
        else if (arg == "--poetic") poetic = true;
        else if (arg == "--chronicles-demo") chroniclesOnly = true;
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

    // Scroll mode: ardent <path> (only when first non-flag argument is a path)
    if (argc > 1 && argv[1][0] != '-') {
        std::string path = argv[1];
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "The scroll cannot be found at this path: '" << path << "'." << std::endl;
            return 1;
        }
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        runArdentProgram(code);
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




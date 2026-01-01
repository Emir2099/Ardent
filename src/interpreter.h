#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>
#include <variant>
#include <vector>
#include <functional>
#include <filesystem>
#include <optional>
#include <fstream>
#include "scroll_loader.h"
#include "ast.h"
#include "arena.h"
#include "env.h"
#include "phrase.h"

class Interpreter {
public:
    Interpreter();
    // Allow integers, strings/phrases, and booleans as variable values
    // Note: For now, collections still use std::string entries; Phrase is introduced for top-level values first.
    using SimpleValue = std::variant<int, std::string, bool>;
    // Arena-backed immutable collection views
    struct Order { size_t size; const SimpleValue* data; };
    struct TomeEntry { std::string key; SimpleValue value; };
    struct Tome { size_t size; const TomeEntry* data; };
    using Value = std::variant<
        int,
        std::string,
        bool,
        std::vector<SimpleValue>, // legacy
        std::unordered_map<std::string, SimpleValue>, // legacy
        Phrase,
        Order,
        Tome
    >;
    struct SpellDef { std::vector<std::string> params; std::shared_ptr<BlockStatement> body; };
    // Module: cached variables/spells from imported scroll
    struct Module {
        std::unordered_map<std::string, Value> variables;
        std::unordered_map<std::string, SpellDef> spells;
        std::optional<ScrollPrologue> prologue; // parsed metadata if present
    };
    using NativeFunc = std::function<Value(const std::vector<Value>&)>;
    // Public helpers for REPL and tooling
    void setSourceName(const std::string& name) { currentSource_ = name; }
    std::string getSourceName() const { return currentSource_; }
    std::vector<std::string> getSpellNames() const {
        std::vector<std::string> out; out.reserve(spells.size());
        for (auto &p : spells) out.push_back(p.first);
        return out;
    }
    std::vector<std::string> getVariableNames() const {
        std::vector<std::string> out; if (scopes.empty()) return out;
        out.reserve(scopes.front().size());
        for (auto &p : scopes.front()) out.push_back(p.first);
        return out;
    }
private:
    // Nested lexical scopes: scopes[0] = global, scopes.back() = current
    std::vector<std::unordered_map<std::string, Value>> scopes;
    // Dual-arenas: long-lived global + ephemeral per-REPL-line
    Arena globalArena_{};
    Arena lineArena_{};
    bool inLineMode_ {false};
    Arena::Frame currentLineFrame_{};
    std::vector<std::string> lineTouched_{}; // vars declared/assigned this line for promotion
    // Env stack uses global arena for keys/buckets
    std::vector<Arena::Frame> scopeFrames_{};
    EnvStack<Value> env_{};
    // Active arena selector
    Arena& activeArena() { return inLineMode_ ? lineArena_ : globalArena_; }
    // Helpers for scoping
    void enterScope();
    void exitScope();
    void declareVariable(const std::string& name, const Value& value);
    void assignVariableAny(const std::string& name, const Value& value);
    bool lookupVariable(const std::string& name, Value& out) const;
    int findScopeIndex(const std::string& name) const; // -1 if not found
    // Stored spells: name -> (param list, body)
    std::unordered_map<std::string, SpellDef> spells;
    struct ReturnSignal { Value value; };
    bool runtimeError = false; // flag to suppress output on runtime errors (e.g., bounds)
    bool inTryContext = false; // when true, allow curses to bubble to enclosing try/catch
    // Error context & call stack
    std::string currentSource_ = "<repl>";
    std::vector<std::string> callStack_{}; // nested spells / natives
    void pushCall(const std::string& label) { callStack_.push_back(label); }
    void popCall() { if (!callStack_.empty()) callStack_.pop_back(); }
    void printPoeticCurse(const std::string& message) const;
    // Import support
    std::unordered_map<std::string, Module> moduleCache;
    std::unordered_map<std::string, Module> logicalModuleCache; // cache by logical import string
    std::unordered_map<std::string, bool> importing;
    std::optional<ScrollPrologue> currentPrologue_{};
    Module loadModule(const std::string& path);
    Module loadModuleLogical(const std::string& logicalName);
    
    // Stream scribes (2.4 Living Chronicles)
    std::unordered_map<std::string, std::shared_ptr<std::fstream>> scribes;
    
    // Native functions
    std::unordered_map<std::string, NativeFunc> nativeRegistry;
    int evaluateExpr(std::shared_ptr<ASTNode> expr); 
    std::string evaluatePrintExpr(std::shared_ptr<ASTNode> expr);
    Value evaluateValue(std::shared_ptr<ASTNode> expr);
    Value executeSpellBody(const SpellDef &def);
    // Promotion helpers
    Value promoteToGlobal(const Value& v);
public:
    void execute(std::shared_ptr<ASTNode> ast);
    void evaluateExpression(std::shared_ptr<ASTNode> expr);
    void assignVariable(const std::string& name, int value);
    void assignVariable(const std::string& name, const std::string& value);
    void assignVariable(const std::string& name, bool value);
    int getIntVariable(const std::string& name);
    std::string getStringVariable(const std::string& name);
    void executeWhileLoop(std::shared_ptr<WhileLoop> loop);
    void executeForLoop(std::shared_ptr<ForLoop> loop);
    void executeDoWhileLoop(std::shared_ptr<DoWhileLoop> loop);
    // REPL line lifecycle
    void beginLine();
    void endLine();
    // Export helpers for module system
    std::unordered_map<std::string, Value> getGlobals() const { return scopes.front(); }
    std::unordered_map<std::string, SpellDef> getSpells() const { return spells; }
    void registerSpell(const std::string& name, const SpellDef& def) { spells[name] = def; }
    void registerNative(const std::string& name, NativeFunc fn) { nativeRegistry[name] = std::move(fn); }

    // REPL helpers
    Value evaluateReplValue(std::shared_ptr<ASTNode> node);
    std::string stringifyValueForRepl(const Value& v);

    // Diagnostics
    std::size_t bytesUsed() const; // total arena-backed bytes used (global + line)

    // Prologue metadata for the current primary scroll (e.g., --interpret)
    void setCurrentPrologue(const ScrollPrologue& p) { currentPrologue_ = p; }
    const std::optional<ScrollPrologue>& getCurrentPrologue() const { return currentPrologue_; }

};

#endif

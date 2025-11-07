#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>
#include <variant>
#include <vector>
#include <functional>
#include "ast.h"

class Interpreter {
public:
    Interpreter();
    // Allow integers, strings, and booleans as variable values
    using SimpleValue = std::variant<int, std::string, bool>;
    using Value = std::variant<int, std::string, bool, std::vector<SimpleValue>, std::unordered_map<std::string, SimpleValue>>;
    struct SpellDef { std::vector<std::string> params; std::shared_ptr<BlockStatement> body; };
    struct Module { std::unordered_map<std::string, Value> variables; std::unordered_map<std::string, SpellDef> spells; };
    using NativeFunc = std::function<Value(const std::vector<Value>&)>;
private:
    // Nested lexical scopes: scopes[0] = global, scopes.back() = current
    std::vector<std::unordered_map<std::string, Value>> scopes;
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
    // Import support
    std::unordered_map<std::string, Module> moduleCache;
    std::unordered_map<std::string, bool> importing;
    Module loadModule(const std::string& path);
    // Native functions
    std::unordered_map<std::string, NativeFunc> nativeRegistry;
    int evaluateExpr(std::shared_ptr<ASTNode> expr); 
    std::string evaluatePrintExpr(std::shared_ptr<ASTNode> expr);
    Value evaluateValue(std::shared_ptr<ASTNode> expr);
    Value executeSpellBody(const SpellDef &def);
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
    // Export helpers for module system
    std::unordered_map<std::string, Value> getGlobals() const { return scopes.front(); }
    std::unordered_map<std::string, SpellDef> getSpells() const { return spells; }
    void registerSpell(const std::string& name, const SpellDef& def) { spells[name] = def; }
    void registerNative(const std::string& name, NativeFunc fn) { nativeRegistry[name] = std::move(fn); }

};

#endif

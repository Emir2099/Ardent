#include "interpreter.h"
#include <iostream>
#include "lexer.h"
#include "parser.h"
#include <functional>
#include <memory>
#include <vector>
#include <variant>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <exception>
// Shared caches/guards for module system across nested interpreters
#include "token.h"
#include "arena.h"
static std::unordered_map<std::string, bool> g_importing;

// ===== Line arena lifecycle and promotion (after includes for full type visibility) =====
Interpreter::Value Interpreter::promoteToGlobal(const Value& v) {
    // For Phrase: if currently built in line arena, copy data into global arena
    if (std::holds_alternative<Phrase>(v)) {
        const Phrase &p = std::get<Phrase>(v);
        // If phrase is large (arena-backed), we allocate new global copy
        if (p.size() > Phrase::SSO_MAX) {
            Phrase np = Phrase::make(p.data(), p.size(), globalArena_);
            return np;
        }
        return v; // small inline phrase already independent
    }
    if (std::holds_alternative<Order>(v)) {
        const Order &ord = std::get<Order>(v);
        void* mem = globalArena_.alloc(sizeof(SimpleValue) * ord.size, alignof(SimpleValue));
        auto* buf = reinterpret_cast<SimpleValue*>(mem);
        for (size_t i = 0; i < ord.size; ++i) new (&buf[i]) SimpleValue(ord.data[i]);
        return Order{ ord.size, buf };
    }
    if (std::holds_alternative<Tome>(v)) {
        const Tome &tm = std::get<Tome>(v);
        void* mem = globalArena_.alloc(sizeof(TomeEntry) * tm.size, alignof(TomeEntry));
        auto* buf = reinterpret_cast<TomeEntry*>(mem);
        for (size_t i = 0; i < tm.size; ++i) new (&buf[i]) TomeEntry{ tm.data[i].key, tm.data[i].value };
        return Tome{ tm.size, buf };
    }
    return v; // primitives and legacy containers unaffected
}

void Interpreter::beginLine() {
    if (inLineMode_) return; // nested safeguard
    inLineMode_ = true;
    currentLineFrame_ = lineArena_.pushFrame();
    lineTouched_.clear();
}

void Interpreter::endLine() {
    if (!inLineMode_) return;
    // Promote touched variables into global arena when they reference line arena allocations
    for (const auto &name : lineTouched_) {
        Value v; if (!lookupVariable(name, v)) continue;
        Value promoted = promoteToGlobal(v);
        assignVariableAny(name, promoted); // rebind (will not duplicate promotion for primitives)
    }
    lineArena_.popFrame(currentLineFrame_);
    inLineMode_ = false;
}
 
// Helpers for pretty-printing values in narrative style
// Exception type for Ardent curses
class ArdentError : public std::exception {
    std::string msg;
public:
    explicit ArdentError(std::string m) : msg(std::move(m)) {}
    const char* what() const noexcept override { return msg.c_str(); }
};
static std::string escapeString(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out.push_back(c);
    }
    return out;
}

// Phrase interop helpers (defined early for global use)
static inline bool isStringLike(const Interpreter::Value& v) {
    return std::holds_alternative<std::string>(v) || std::holds_alternative<Phrase>(v);
}
static inline std::string asStdString(const Interpreter::Value& v) {
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<Phrase>(v)) { const Phrase& p = std::get<Phrase>(v); return std::string(p.data(), p.size()); }
    return std::string("");
}

static std::string formatSimple(const Interpreter::SimpleValue &sv) {
    if (std::holds_alternative<int>(sv)) return std::to_string(std::get<int>(sv));
    if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv) ? std::string("True") : std::string("False");
    // phrase -> quoted with escaping
    return std::string("\"") + escapeString(std::get<std::string>(sv)) + "\"";
}

static std::string formatValue(const Interpreter::Value &v) {
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
    if (std::holds_alternative<std::string>(v)) return std::string("\"") + escapeString(std::get<std::string>(v)) + "\"";
    if (std::holds_alternative<Phrase>(v)) {
        const Phrase &p = std::get<Phrase>(v);
        return std::string("\"") + escapeString(std::string(p.data(), p.size())) + "\"";
    }
    if (std::holds_alternative<Interpreter::Order>(v)) {
        const auto &ord = std::get<Interpreter::Order>(v);
        std::ostringstream oss; oss << "[ ";
        for (size_t i = 0; i < ord.size; ++i) { if (i) oss << ", "; oss << formatSimple(ord.data[i]); }
        oss << " ]"; return oss.str();
    }
    if (std::holds_alternative<std::vector<Interpreter::SimpleValue>>(v)) {
        const auto &vec = std::get<std::vector<Interpreter::SimpleValue>>(v);
        std::ostringstream oss;
        oss << "[ ";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i) oss << ", ";
            oss << formatSimple(vec[i]);
        }
        oss << " ]";
        return oss.str();
    }
    if (std::holds_alternative<Interpreter::Tome>(v)) {
        const auto &tm = std::get<Interpreter::Tome>(v);
        std::ostringstream oss; oss << "{ "; bool first = true;
        for (size_t i = 0; i < tm.size; ++i) {
            const auto &e = tm.data[i];
            if (!first) oss << ", "; first = false;
            oss << "\"" << escapeString(e.key) << "\"" << ": " << formatSimple(e.value);
        }
        oss << " }"; return oss.str();
    }
    if (std::holds_alternative<std::unordered_map<std::string, Interpreter::SimpleValue>>(v)) {
        const auto &mp = std::get<std::unordered_map<std::string, Interpreter::SimpleValue>>(v);
        std::ostringstream oss;
        oss << "{ ";
        bool first = true;
        for (const auto &p : mp) {
            if (!first) oss << ", ";
            first = false;
            oss << "\"" << escapeString(p.first) << "\"" << ": " << formatSimple(p.second);
        }
        oss << " }";
        return oss.str();
    }
    return "";
}

// ===== Scoping helpers =====
Interpreter::Interpreter() {
    // Initialize global scope
    scopes.emplace_back();
    // Initialize environment stack with global frame
    scopeFrames_.push_back(globalArena_.pushFrame());
    env_.push(globalArena_);
    // Register built-in native functions
    registerNative("math.add", [this](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) {
            throw ArdentError("The spirits demand 2 offerings for 'math.add', yet " + std::to_string(args.size()) + " were placed.");
        }
        auto toNum = [&](const Value &v) -> int {
            if (std::holds_alternative<int>(v)) return std::get<int>(v);
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
            if (std::holds_alternative<std::string>(v)) { try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; } }
            if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); try { return std::stoi(std::string(p.data(), p.size())); } catch (...) { return 0; } }
            return 0;
        };
        return toNum(args[0]) + toNum(args[1]);
    });
    registerNative("system.len", [this](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw ArdentError("The spirits demand 1 offering for 'system.len', yet " + std::to_string(args.size()) + " were placed.");
        }
        const Value &v = args[0];
        if (std::holds_alternative<std::string>(v)) return static_cast<int>(std::get<std::string>(v).size());
        if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return static_cast<int>(p.size()); }
        if (std::holds_alternative<Order>(v)) return static_cast<int>(std::get<Order>(v).size);
        if (std::holds_alternative<std::vector<SimpleValue>>(v)) return static_cast<int>(std::get<std::vector<SimpleValue>>(v).size());
        if (std::holds_alternative<Tome>(v)) return static_cast<int>(std::get<Tome>(v).size);
        if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(v)) return static_cast<int>(std::get<std::unordered_map<std::string, SimpleValue>>(v).size());
        // numbers/bools -> length not meaningful; return 0
        return 0;
    });
    registerNative("math.divide", [this](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) {
            throw ArdentError("The spirits demand 2 offerings for 'math.divide', yet " + std::to_string(args.size()) + " were placed.");
        }
        auto toNum = [&](const Value &v) -> int {
            if (std::holds_alternative<int>(v)) return std::get<int>(v);
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
            if (std::holds_alternative<std::string>(v)) { try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; } }
            if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); try { return std::stoi(std::string(p.data(), p.size())); } catch (...) { return 0; } }
            return 0;
        };
        int a = toNum(args[0]);
        int b = toNum(args[1]);
        if (b == 0) throw ArdentError("A curse was cast: Division by zero in spirit 'math.divide'.");
        return a / b;
    });
    // Chronicle Rites: file I/O with sandboxing
    auto pathAllowed = [](const std::string &path) -> bool {
        if (path.empty()) return false;
        // Disallow absolute and parent traversal
        if (path.size() > 1 && path[1] == ':') return false; // drive letter
        if (path[0] == '/' || path[0] == '\\') return false;
        if (path.find("..") != std::string::npos) return false;
        return true;
    };
    registerNative("chronicles.read", [pathAllowed](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) {
            throw ArdentError("The spirits demand 1 offering for 'chronicles.read', yet " + std::to_string(args.size()) + " were placed.");
        }
        if (!(std::holds_alternative<std::string>(args[0]) || std::holds_alternative<Phrase>(args[0]))) {
            throw ArdentError("A scroll path must be a phrase.");
        }
        std::string path = std::holds_alternative<std::string>(args[0]) ? std::get<std::string>(args[0]) : std::string(std::get<Phrase>(args[0]).data(), std::get<Phrase>(args[0]).size());
        if (!pathAllowed(path)) throw ArdentError("The Chronicle ward forbids that path: '" + path + "'.");
        std::ifstream in(path, std::ios::binary);
        if (!in) throw ArdentError("The scroll cannot be opened: '" + path + "'.");
        std::ostringstream ss; ss << in.rdbuf();
        return ss.str();
    });
    registerNative("chronicles.write", [pathAllowed](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw ArdentError("The spirits demand 2 offerings for 'chronicles.write'.");
        if (!(std::holds_alternative<std::string>(args[0]) || std::holds_alternative<Phrase>(args[0]))) throw ArdentError("A scroll path must be a phrase.");
        std::string path = std::holds_alternative<std::string>(args[0]) ? std::get<std::string>(args[0]) : std::string(std::get<Phrase>(args[0]).data(), std::get<Phrase>(args[0]).size());
        if (!pathAllowed(path)) throw ArdentError("The Chronicle ward forbids that path: '" + path + "'.");
        std::string content;
        const Value &v = args[1];
        if (std::holds_alternative<std::string>(v)) content = std::get<std::string>(v);
        else if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); content.assign(p.data(), p.size()); }
        else if (std::holds_alternative<int>(v)) content = std::to_string(std::get<int>(v));
        else if (std::holds_alternative<bool>(v)) content = std::get<bool>(v) ? "True" : "False";
        else content = formatValue(v);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) throw ArdentError("The scroll cannot be opened: '" + path + "'.");
        out << content;
        return 0;
    });
    registerNative("chronicles.append", [pathAllowed](const std::vector<Value>& args) -> Value {
        if (args.size() != 2) throw ArdentError("The spirits demand 2 offerings for 'chronicles.append'.");
        if (!(std::holds_alternative<std::string>(args[0]) || std::holds_alternative<Phrase>(args[0]))) throw ArdentError("A scroll path must be a phrase.");
        std::string path = std::holds_alternative<std::string>(args[0]) ? std::get<std::string>(args[0]) : std::string(std::get<Phrase>(args[0]).data(), std::get<Phrase>(args[0]).size());
        if (!pathAllowed(path)) throw ArdentError("The Chronicle ward forbids that path: '" + path + "'.");
        std::string content;
        const Value &v = args[1];
        if (std::holds_alternative<std::string>(v)) content = std::get<std::string>(v);
        else if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); content.assign(p.data(), p.size()); }
        else if (std::holds_alternative<int>(v)) content = std::to_string(std::get<int>(v));
        else if (std::holds_alternative<bool>(v)) content = std::get<bool>(v) ? "True" : "False";
        else content = formatValue(v);
        std::ofstream out(path, std::ios::binary | std::ios::app);
        if (!out) throw ArdentError("The scroll cannot be opened: '" + path + "'.");
        out << content;
        return 0;
    });
    registerNative("chronicles.exists", [pathAllowed](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw ArdentError("The spirits demand 1 offering for 'chronicles.exists'.");
        if (!(std::holds_alternative<std::string>(args[0]) || std::holds_alternative<Phrase>(args[0]))) throw ArdentError("A scroll path must be a phrase.");
        std::string path = std::holds_alternative<std::string>(args[0]) ? std::get<std::string>(args[0]) : std::string(std::get<Phrase>(args[0]).data(), std::get<Phrase>(args[0]).size());
        if (!pathAllowed(path)) return false;
        std::error_code ec; return std::filesystem::exists(path, ec);
    });
    registerNative("chronicles.delete", [pathAllowed](const std::vector<Value>& args) -> Value {
        if (args.size() != 1) throw ArdentError("The spirits demand 1 offering for 'chronicles.delete'.");
        if (!(std::holds_alternative<std::string>(args[0]) || std::holds_alternative<Phrase>(args[0]))) throw ArdentError("A scroll path must be a phrase.");
        std::string path = std::holds_alternative<std::string>(args[0]) ? std::get<std::string>(args[0]) : std::string(std::get<Phrase>(args[0]).data(), std::get<Phrase>(args[0]).size());
        if (!pathAllowed(path)) throw ArdentError("The Chronicle ward forbids that path: '" + path + "'.");
        std::error_code ec; bool ok = std::filesystem::remove(path, ec);
        if (!ok && ec) throw ArdentError("The scroll cannot be banished: '" + path + "'.");
        return ok ? 1 : 0;
    });
}

void Interpreter::enterScope() {
    scopes.emplace_back();
    scopeFrames_.push_back(globalArena_.pushFrame());
    env_.push(globalArena_);
}
void Interpreter::exitScope() {
    if (scopes.size() > 1) {
        scopes.pop_back();
        env_.pop();
        auto fr = scopeFrames_.back();
        scopeFrames_.pop_back();
        globalArena_.popFrame(fr);
    }
}

int Interpreter::findScopeIndex(const std::string& name) const {
    for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i) {
        if (scopes[static_cast<size_t>(i)].find(name) != scopes[static_cast<size_t>(i)].end()) return i;
    }
    return -1;
}

bool Interpreter::lookupVariable(const std::string& name, Value& out) const {
    if (auto* v = const_cast<EnvStack<Value>&>(env_).lookup(name.c_str(), static_cast<std::uint32_t>(name.size()))) { out = *v; return true; }
    int idx = findScopeIndex(name);
    if (idx >= 0) { out = scopes[static_cast<size_t>(idx)].at(name); return true; }
    return false;
}

void Interpreter::declareVariable(const std::string& name, const Value& value) {
    scopes.back()[name] = value;
    // Mirror into new env stack (local scope)
    env_.declare(name.c_str(), static_cast<std::uint32_t>(name.size()), value);
    if (inLineMode_) lineTouched_.push_back(name);
    // Debug
    std::cout << "Variable assigned: " << name << " = ";
    if (std::holds_alternative<int>(value)) std::cout << std::get<int>(value);
    else if (std::holds_alternative<std::string>(value)) std::cout << std::get<std::string>(value);
    else if (std::holds_alternative<Phrase>(value)) std::cout.write(std::get<Phrase>(value).data(), static_cast<std::streamsize>(std::get<Phrase>(value).size()));
    else if (std::holds_alternative<bool>(value)) std::cout << (std::get<bool>(value) ? "True" : "False");
    else if (std::holds_alternative<Order>(value)) std::cout << "[order size=" << std::get<Order>(value).size << "]";
    else if (std::holds_alternative<std::vector<SimpleValue>>(value)) std::cout << "[order size=" << std::get<std::vector<SimpleValue>>(value).size() << "]";
    else if (std::holds_alternative<Tome>(value)) std::cout << "{tome size=" << std::get<Tome>(value).size << "}";
    else if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(value)) std::cout << "{tome size=" << std::get<std::unordered_map<std::string, SimpleValue>>(value).size() << "}";
    std::cout << std::endl;
}

void Interpreter::assignVariableAny(const std::string& name, const Value& value) {
    int idx = findScopeIndex(name);
    if (idx >= 0) {
        scopes[static_cast<size_t>(idx)][name] = value;
    } else {
        scopes.back()[name] = value; // implicit local
    }
    // Mirror assignment/update into new env stack (will search for existing scope, else local declare)
    env_.assign(name.c_str(), static_cast<std::uint32_t>(name.size()), value);
    if (inLineMode_) lineTouched_.push_back(name);
    // Debug
    std::cout << "Variable assigned: " << name << " = ";
    if (std::holds_alternative<int>(value)) std::cout << std::get<int>(value);
    else if (std::holds_alternative<std::string>(value)) std::cout << std::get<std::string>(value);
    else if (std::holds_alternative<Phrase>(value)) std::cout.write(std::get<Phrase>(value).data(), static_cast<std::streamsize>(std::get<Phrase>(value).size()));
    else if (std::holds_alternative<bool>(value)) std::cout << (std::get<bool>(value) ? "True" : "False");
    else if (std::holds_alternative<Order>(value)) std::cout << "[order size=" << std::get<Order>(value).size << "]";
    else if (std::holds_alternative<std::vector<SimpleValue>>(value)) std::cout << "[order size=" << std::get<std::vector<SimpleValue>>(value).size() << "]";
    else if (std::holds_alternative<Tome>(value)) std::cout << "{tome size=" << std::get<Tome>(value).size << "}";
    else if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(value)) std::cout << "{tome size=" << std::get<std::unordered_map<std::string, SimpleValue>>(value).size() << "}";
    std::cout << std::endl;
}

void Interpreter::assignVariable(const std::string& name, int value) {
    assignVariableAny(name, Value(value));
}

void Interpreter::assignVariable(const std::string& name, const std::string& value) {
    assignVariableAny(name, Value(value));
}

void Interpreter::assignVariable(const std::string& name, bool value) {
    assignVariableAny(name, Value(value));
}

int Interpreter::getIntVariable(const std::string& name) {
    Value v;
    if (lookupVariable(name, v)) {
        if (std::holds_alternative<int>(v)) return std::get<int>(v);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
        std::cerr << "Error: Variable '" << name << "' is not a number" << std::endl;
    } else {
        std::cerr << "Error: Undefined variable '" << name << "'" << std::endl;
    }
    return 0;
}

std::string Interpreter::getStringVariable(const std::string& name) {
    Value v;
    if (lookupVariable(name, v)) {
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
    } else {
        std::cerr << "Error: Undefined variable '" << name << "'" << std::endl;
    }
    return "";
}

// Evaluate an expression into a typed Value (int, string, bool, order, tome)
Interpreter::Value Interpreter::evaluateValue(std::shared_ptr<ASTNode> expr) {
    // Literal or identifier
    if (auto e = std::dynamic_pointer_cast<Expression>(expr)) {
        switch (e->token.type) {
            case TokenType::NUMBER:
                return std::stoi(e->token.value);
            case TokenType::STRING: {
                Phrase p = Phrase::make(e->token.value.data(), e->token.value.size(), activeArena());
                return p;
            }
            case TokenType::BOOLEAN:
                return e->token.value == "True";
            case TokenType::IDENTIFIER: {
                Value v;
                if (lookupVariable(e->token.value, v)) return v;
                std::cerr << "Error: Undefined variable '" << e->token.value << "'" << std::endl;
                return 0;
            }
            default:
                break;
        }
    }
    if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(expr)) {
        size_t n = arr->elements.size();
    void* mem = activeArena().alloc(sizeof(SimpleValue) * n, alignof(SimpleValue));
        auto* buf = reinterpret_cast<SimpleValue*>(mem);
        size_t i = 0;
        for (auto &el : arr->elements) {
            Value v = evaluateValue(el);
            if (std::holds_alternative<int>(v)) new (&buf[i++]) SimpleValue(std::get<int>(v));
            else if (std::holds_alternative<std::string>(v)) new (&buf[i++]) SimpleValue(std::get<std::string>(v));
            else if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); new (&buf[i++]) SimpleValue(std::string(p.data(), p.size())); }
            else if (std::holds_alternative<bool>(v)) new (&buf[i++]) SimpleValue(std::get<bool>(v));
            else { std::cerr << "TypeError: Only simple values (number, phrase, truth) allowed inside an order" << std::endl; new (&buf[i++]) SimpleValue(0); }
        }
        return Order{ n, buf };
    }
    if (auto invoke = std::dynamic_pointer_cast<SpellInvocation>(expr)) {
        // Resolve spell
        auto it = spells.find(invoke->spellName);
        if (it == spells.end()) {
            std::cerr << "Error: Unknown spell '" << invoke->spellName << "'" << std::endl;
            return 0;
        }
        const auto &def = it->second;
        if (def.params.size() != invoke->args.size()) {
            std::cerr << "Error: Spell '" << invoke->spellName << "' expects " << def.params.size() << " arguments but got " << invoke->args.size() << std::endl;
            return 0;
        }
        // New lexical scope for parameters
        enterScope();
        for (size_t i = 0; i < def.params.size(); ++i) {
            Value val = evaluateValue(invoke->args[i]);
            declareVariable(def.params[i], val);
        }
        Value retVal = std::string("");
        bool hasReturn = false;
        for (auto &stmt : def.body->statements) {
            if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(stmt)) {
                retVal = evaluateValue(ret->expression);
                hasReturn = true;
                break;
            }
            execute(stmt);
        }
        exitScope();
        if (!hasReturn) return std::string("");
        return retVal;
    }
    if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(expr)) {
        size_t n = obj->entries.size();
    void* mem = activeArena().alloc(sizeof(TomeEntry) * n, alignof(TomeEntry));
        auto* buf = reinterpret_cast<TomeEntry*>(mem);
        for (size_t i = 0; i < n; ++i) {
            const auto &kv = obj->entries[i];
            Value v = evaluateValue(kv.second);
            SimpleValue sv;
            if (std::holds_alternative<int>(v)) sv = std::get<int>(v);
            else if (std::holds_alternative<std::string>(v)) sv = std::get<std::string>(v);
            else if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); sv = std::string(p.data(), p.size()); }
            else if (std::holds_alternative<bool>(v)) sv = std::get<bool>(v);
            else { std::cerr << "TypeError: Only simple values (number, phrase, truth) allowed inside a tome" << std::endl; sv = 0; }
            new (&buf[i]) TomeEntry{ kv.first, std::move(sv) };
        }
        return Tome{ n, buf };
    }
    if (auto native = std::dynamic_pointer_cast<NativeInvocation>(expr)) {
        auto it = nativeRegistry.find(native->funcName);
        if (it == nativeRegistry.end()) {
            throw ArdentError(std::string("The spirits know not the rite '") + native->funcName + "'.");
        }
        std::vector<Value> argv;
        argv.reserve(native->args.size());
        for (auto &a : native->args) argv.push_back(evaluateValue(a));
        try {
            return it->second(argv);
        } catch (const ArdentError&) {
            throw; // propagate
        } catch (...) {
            throw ArdentError(std::string("A rift silences the spirits during '") + native->funcName + "'.");
        }
    }
    if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value target = evaluateValue(idx->target);
        Value key = evaluateValue(idx->index);
        // Index into order (arena-backed)
        if (std::holds_alternative<Order>(target)) {
            int i = 0;
            if (std::holds_alternative<int>(key)) i = std::get<int>(key);
            else { std::cerr << "TypeError: Order index must be a number" << std::endl; return 0; }
            const auto &ord = std::get<Order>(target);
            size_t n = ord.size;
            if (i < 0) { int abs = -i; if (static_cast<size_t>(abs) > n) { std::cerr << "Error: None stand that far behind in the order, for only " << n << " dwell within." << std::endl; runtimeError = true; return 0; } i = static_cast<int>(n) + i; }
            if (i < 0 || static_cast<size_t>(i) >= n) { std::string oname = "the order"; if (auto idExpr = std::dynamic_pointer_cast<Expression>(idx->target)) { if (idExpr->token.type == TokenType::IDENTIFIER) { oname = "'" + idExpr->token.value + "'"; } } std::cerr << "Error: The council knows no element at position " << i << ", for the order " << oname << " holds but " << n << "." << std::endl; runtimeError = true; return 0; }
            const SimpleValue &sv = ord.data[static_cast<size_t>(i)];
            if (std::holds_alternative<int>(sv)) return std::get<int>(sv);
            if (std::holds_alternative<std::string>(sv)) return std::get<std::string>(sv);
            if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv);
            return 0;
        }
        // Index into order (legacy)
        if (std::holds_alternative<std::vector<SimpleValue>>(target)) {
            int i = 0;
            if (std::holds_alternative<int>(key)) i = std::get<int>(key);
            else {
                std::cerr << "TypeError: Order index must be a number" << std::endl;
                return 0;
            }
            const auto &vec = std::get<std::vector<SimpleValue>>(target);
            size_t n = vec.size();
            // Support Python-style negative indices
            if (i < 0) {
                int abs = -i;
                if (static_cast<size_t>(abs) > n) {
                    // Too far negative: special narrative error
                    std::cerr << "Error: None stand that far behind in the order, for only " << n << " dwell within." << std::endl;
                    runtimeError = true;
                    return 0;
                }
                i = static_cast<int>(n) + i; // e.g., -1 -> n-1
            }
            if (i < 0 || static_cast<size_t>(i) >= n) {
                // Compose narrative error for positive OOB or zero-size corner
                std::string oname = "the order";
                if (auto idExpr = std::dynamic_pointer_cast<Expression>(idx->target)) {
                    if (idExpr->token.type == TokenType::IDENTIFIER) {
                        oname = "'" + idExpr->token.value + "'";
                    }
                }
                std::cerr << "Error: The council knows no element at position " << i
                          << ", for the order " << oname << " holds but " << n << "." << std::endl;
                runtimeError = true;
                return 0;
            }
            const SimpleValue &sv = vec[static_cast<size_t>(i)];
            if (std::holds_alternative<int>(sv)) return std::get<int>(sv);
            if (std::holds_alternative<std::string>(sv)) return std::get<std::string>(sv);
            if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv);
            return 0;
        }
        // Index into tome (arena-backed)
        if (std::holds_alternative<Tome>(target)) {
            std::string k;
            if (std::holds_alternative<std::string>(key)) k = std::get<std::string>(key);
            else if (std::holds_alternative<Phrase>(key)) { const Phrase &p = std::get<Phrase>(key); k.assign(p.data(), p.size()); }
            else { std::cerr << "TypeError: Tome key must be a phrase" << std::endl; return 0; }
            const auto &tm = std::get<Tome>(target);
            for (size_t i = 0; i < tm.size; ++i) {
                if (tm.data[i].key == k) {
                    const SimpleValue &sv = tm.data[i].value;
                    if (std::holds_alternative<int>(sv)) return std::get<int>(sv);
                    if (std::holds_alternative<std::string>(sv)) return std::get<std::string>(sv);
                    if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv);
                    return 0;
                }
            }
            std::cerr << "KeyError: Tome has no entry for '" << k << "'" << std::endl; return 0;
        }
        // Index into tome (legacy)
        if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(target)) {
            std::string k;
            if (std::holds_alternative<std::string>(key)) k = std::get<std::string>(key);
            else if (std::holds_alternative<Phrase>(key)) { const Phrase &p = std::get<Phrase>(key); k.assign(p.data(), p.size()); }
            else {
                std::cerr << "TypeError: Tome key must be a phrase" << std::endl;
                return 0;
            }
            const auto &mp = std::get<std::unordered_map<std::string, SimpleValue>>(target);
            auto it = mp.find(k);
            if (it == mp.end()) {
                std::cerr << "KeyError: Tome has no entry for '" << k << "'" << std::endl;
                return 0;
            }
            const SimpleValue &sv = it->second;
            if (std::holds_alternative<int>(sv)) return std::get<int>(sv);
            if (std::holds_alternative<std::string>(sv)) return std::get<std::string>(sv);
            if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv);
            return 0;
        }
        std::cerr << "TypeError: Target is not an order or tome" << std::endl;
        return 0;
    }
    // Cast
    if (auto c = std::dynamic_pointer_cast<CastExpression>(expr)) {
        Value v = evaluateValue(c->operand);
        if (c->target == CastTarget::ToPhrase) {
            if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
            return std::string("");
        } else if (c->target == CastTarget::ToTruth) {
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
            if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0;
            if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
            return false;
        } else { // ToNumber
            if (std::holds_alternative<int>(v)) return std::get<int>(v);
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
            if (std::holds_alternative<std::string>(v)) {
                try {
                    return std::stoi(std::get<std::string>(v));
                } catch (...) {
                    std::cerr << "CastError: cannot convert phrase to number, defaulting to 0" << std::endl;
                    return 0;
                }
            }
            return 0;
        }
    }
    // Unary NOT and other cases -> compute numerically and infer type
    if (std::dynamic_pointer_cast<UnaryExpression>(expr) || std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        // Determine if boolean-style by operator types
        if (auto u = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
            if (u->op.type == TokenType::NOT) {
                int v = evaluateExpr(expr);
                return v != 0;
            }
        }
        if (auto b = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
            if (b->op.type == TokenType::AND || b->op.type == TokenType::OR ||
                b->op.type == TokenType::SURPASSETH || b->op.type == TokenType::REMAINETH ||
                b->op.type == TokenType::EQUAL || b->op.type == TokenType::NOT_EQUAL ||
                b->op.type == TokenType::GREATER || b->op.type == TokenType::LESSER) {
                int v = evaluateExpr(expr);
                return v != 0;
            }
            if (b->op.type == TokenType::OPERATOR && b->op.value == "+") {
                // String dominance: if stringy, return phrase
                // Reuse evaluatePrintExpr for correct concatenation spacing
                // Detect stringy by checking print domain
                // Conservative: if either side yields string in evaluateValue, treat as string
                Value lv = evaluateValue(b->left);
                Value rv = evaluateValue(b->right);
                if (isStringLike(lv) || isStringLike(rv)) {
                    std::string s = evaluatePrintExpr(expr);
                    Phrase p = Phrase::make(s.data(), s.size(), activeArena());
                    return p;
                }
                // Otherwise numeric sum
                int sum = evaluateExpr(expr);
                return sum;
            }
        }
        // Fallback numeric
        int n = evaluateExpr(expr);
        return n;
    }
    // Unknown node type
    return 0;
}

int Interpreter::evaluateExpr(std::shared_ptr<ASTNode> expr) {
    if (auto numExpr = std::dynamic_pointer_cast<Expression>(expr)) {
        if (numExpr->token.type == TokenType::IDENTIFIER) {
            return getIntVariable(numExpr->token.value);
        } else if (numExpr->token.type == TokenType::NUMBER) {
            return std::stoi(numExpr->token.value);
        } else if (numExpr->token.type == TokenType::BOOLEAN) {
            return (numExpr->token.value == "True") ? 1 : 0;
        }
    } else if (auto castExpr = std::dynamic_pointer_cast<CastExpression>(expr)) {
        // Evaluate cast in numeric domain
        Value v = evaluateValue(castExpr->operand);
        switch (castExpr->target) {
            case CastTarget::ToNumber:
                if (std::holds_alternative<int>(v)) return std::get<int>(v);
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
                if (std::holds_alternative<std::string>(v)) { try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; } }
                if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); try { return std::stoi(std::string(p.data(), p.size())); } catch (...) { return 0; } }
                break;
            case CastTarget::ToTruth:
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
                if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0 ? 1 : 0;
                if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty() ? 1 : 0;
                if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return p.size() != 0 ? 1 : 0; }
                break;
            case CastTarget::ToPhrase:
                // In numeric context, phrase has no numeric value; return 0
                return 0;
        }
    } else if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value v = evaluateValue(expr);
        if (std::holds_alternative<int>(v)) return std::get<int>(v);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
        if (std::holds_alternative<std::string>(v)) { try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; } }
        if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); try { return std::stoi(std::string(p.data(), p.size())); } catch (...) { return 0; } }
        return 0;
    } else if (auto unary = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
        int val = evaluateExpr(unary->operand);
        if (unary->op.type == TokenType::NOT) {
            return val ? 0 : 1;
        }
        return val;
    } else if (auto binExpr = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        int left = evaluateExpr(binExpr->left);
        int right = evaluateExpr(binExpr->right);
        // Handle comparison operators
        if (binExpr->op.type == TokenType::SURPASSETH) {
            return left > right ? 1 : 0;
        } else if (binExpr->op.type == TokenType::REMAINETH) {
            return left < right ? 1 : 0;
        } else if (binExpr->op.type == TokenType::EQUAL) {
            return (left == right) ? 1 : 0;
        } else if (binExpr->op.type == TokenType::NOT_EQUAL) {
            return (left != right) ? 1 : 0;
        } else if (binExpr->op.type == TokenType::GREATER) {
            return (left > right) ? 1 : 0;
        } else if (binExpr->op.type == TokenType::LESSER) {
            return (left < right) ? 1 : 0;
        } else if (binExpr->op.type == TokenType::AND) {
            return (left != 0) && (right != 0) ? 1 : 0;
        } else if (binExpr->op.type == TokenType::OR) {
            return (left != 0) || (right != 0) ? 1 : 0;
        }
        // Handle arithmetic operators
        else if (binExpr->op.value == "+") {
            return left + right;
        } else if (binExpr->op.value == "-") {
            return left - right;
        } else if (binExpr->op.value == "*") {
            return left * right;
        } else if (binExpr->op.value == "/") {
            if (right == 0) {
                std::cerr << "Runtime error: Division by zero." << std::endl;
                return 0;
            }
            return left / right;
        } else if (binExpr->op.value == "%") {
            if (right == 0) {
                std::cerr << "Runtime error: Modulo division by zero." << std::endl;
                return 0;
            }
            return left % right;
        }
    
    }
    
    return 0; // Default for errors
}

void Interpreter::execute(std::shared_ptr<ASTNode> ast) {
    if (!ast) {
        std::cerr << "Error: NULL AST Node encountered!" << std::endl;
        return;
    }
    
    // Execute a block of statements.
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(ast)) {
        for (auto& stmt : block->statements) {
            if (inTryContext) {
                // Within a try/catch context: let curses bubble to the enclosing handler
                execute(stmt);
            } else {
                try {
                    execute(stmt);
                } catch (const ArdentError& e) {
                    // Top-level: narrate unhandled curse
                    std::cerr << e.what() << std::endl;
                }
            }
        }
    }
    // Evaluate a simple expression.
    else if (auto expr = std::dynamic_pointer_cast<Expression>(ast)) {
        std::cout << "Evaluating expression: " << expr->token.value << std::endl;
    }
    // Handle binary expressions (used for assignments and conditions).
    else if (auto binExpr = std::dynamic_pointer_cast<BinaryExpression>(ast)) {
        // For assignments: operator IS_OF.
        if (binExpr->op.type == TokenType::IS_OF) {
            auto leftExpr = std::dynamic_pointer_cast<Expression>(binExpr->left);
            if (leftExpr) {
                std::string varName = leftExpr->token.value;
                Value rhs = evaluateValue(binExpr->right);
                // Assign into nearest scope (or current if new)
                assignVariableAny(varName, rhs);
            }
        } else {
            // Otherwise, simply execute left and right.
            execute(binExpr->left);
            execute(binExpr->right);
        }
    }
    // Handle if-statements.
    else if (auto ifStmt = std::dynamic_pointer_cast<IfStatement>(ast)) {
        std::cout << "Executing IF condition..." << std::endl;
        int cond = evaluateExpr(ifStmt->condition);
        if (cond != 0) {
            enterScope();
            execute(ifStmt->thenBranch);
            exitScope();
        } else if (ifStmt->elseBranch) {
            enterScope();
            execute(ifStmt->elseBranch);
            exitScope();
        }
    }
     // Handle while loop execution
     else if (auto whileLoop = std::dynamic_pointer_cast<WhileLoop>(ast)) {
        executeWhileLoop(whileLoop);
    }
    
    else if (auto forLoop = std::dynamic_pointer_cast<ForLoop>(ast)) {
        executeForLoop(forLoop);
    }
    else if (auto doWhileStmt = std::dynamic_pointer_cast<DoWhileLoop>(ast)) {
        executeDoWhileLoop(doWhileStmt);
    }
    else if (auto tc = std::dynamic_pointer_cast<TryCatch>(ast)) {
        bool hadCurse = false;
        std::string curseMsg;
        try {
            bool prev = inTryContext;
            inTryContext = true;
            execute(tc->tryBlock);
            inTryContext = prev;
        } catch (const ArdentError& e) {
            hadCurse = true;
            curseMsg = e.what();
        }
        if (hadCurse && tc->catchBlock) {
            enterScope();
            if (!tc->catchVar.empty()) {
                declareVariable(tc->catchVar, Value(curseMsg));
            }
            bool prev = inTryContext;
            inTryContext = true;
            execute(tc->catchBlock);
            inTryContext = prev;
            exitScope();
        } else if (hadCurse && !tc->catchBlock) {
            // rethrow if not handled here
            throw ArdentError(curseMsg);
        }
        if (tc->finallyBlock) {
            bool prev = inTryContext;
            inTryContext = true;
            execute(tc->finallyBlock);
            inTryContext = prev;
        }
    }
    else if (auto rite = std::dynamic_pointer_cast<CollectionRite>(ast)) {
        // Clone-modify-reassign pattern with scoped lookup
        int scopeIdx = findScopeIndex(rite->varName);
        if (scopeIdx < 0) {
            std::cerr << "[CollectionRite] Undefined collection '" << rite->varName << "'" << std::endl;
            return;
        }
        Value current = scopes[static_cast<size_t>(scopeIdx)][rite->varName];
        if (rite->riteType == CollectionRiteType::OrderExpand || rite->riteType == CollectionRiteType::OrderRemove) {
            // Support arena-backed immutable Order first
            if (std::holds_alternative<Order>(current)) {
                const Order &old = std::get<Order>(current);
                if (rite->riteType == CollectionRiteType::OrderExpand) {
                    Value ev = evaluateValue(rite->valueExpr);
                    SimpleValue sv;
                    if (std::holds_alternative<int>(ev)) sv = std::get<int>(ev);
                    else if (std::holds_alternative<std::string>(ev)) sv = std::get<std::string>(ev);
                    else if (std::holds_alternative<Phrase>(ev)) { const Phrase &p = std::get<Phrase>(ev); sv = std::string(p.data(), p.size()); }
                    else if (std::holds_alternative<bool>(ev)) sv = std::get<bool>(ev);
                    else { std::cerr << "TypeError: Only simple values may be placed within an order" << std::endl; return; }
                    size_t n = old.size;
                    void* mem = activeArena().alloc(sizeof(SimpleValue) * (n + 1), alignof(SimpleValue));
                    auto* buf = reinterpret_cast<SimpleValue*>(mem);
                    for (size_t i = 0; i < n; ++i) new (&buf[i]) SimpleValue(old.data[i]);
                    new (&buf[n]) SimpleValue(std::move(sv));
                    assignVariableAny(rite->varName, Order{ n + 1, buf });
                } else { // remove
                    Value v = evaluateValue(rite->keyExpr);
                    auto equalsSimple = [&](const SimpleValue &sv) -> bool {
                        if (std::holds_alternative<int>(v) && std::holds_alternative<int>(sv)) return std::get<int>(v)==std::get<int>(sv);
                        if (std::holds_alternative<std::string>(v) && std::holds_alternative<std::string>(sv)) return std::get<std::string>(v)==std::get<std::string>(sv);
                        if (std::holds_alternative<Phrase>(v) && std::holds_alternative<std::string>(sv)) { const Phrase &p = std::get<Phrase>(v); return std::string(p.data(), p.size()) == std::get<std::string>(sv); }
                        if (std::holds_alternative<bool>(v) && std::holds_alternative<bool>(sv)) return std::get<bool>(v)==std::get<bool>(sv);
                        return false;
                    };
                    size_t n = old.size; size_t newN = n; bool removed = false;
                    for (size_t i = 0; i < n; ++i) { if (!removed && equalsSimple(old.data[i])) { removed = true; --newN; } }
                    void* mem = activeArena().alloc(sizeof(SimpleValue) * newN, alignof(SimpleValue));
                    auto* buf = reinterpret_cast<SimpleValue*>(mem); size_t w=0; removed=false;
                    for (size_t i = 0; i < n; ++i) { if (!removed && equalsSimple(old.data[i])) { removed=true; continue; } new (&buf[w++]) SimpleValue(old.data[i]); }
                    assignVariableAny(rite->varName, Order{ newN, buf });
                }
            } else if (std::holds_alternative<std::vector<SimpleValue>>(current)) {
                auto vec = std::get<std::vector<SimpleValue>>(current); // legacy clone
                if (rite->riteType == CollectionRiteType::OrderExpand) {
                    Value v = evaluateValue(rite->valueExpr);
                    if (std::holds_alternative<int>(v)) vec.emplace_back(std::get<int>(v));
                    else if (std::holds_alternative<std::string>(v)) vec.emplace_back(std::get<std::string>(v));
                    else if (std::holds_alternative<bool>(v)) vec.emplace_back(std::get<bool>(v));
                    else { std::cerr << "TypeError: Only simple values may be placed within an order" << std::endl; }
                } else {
                    Value v = evaluateValue(rite->keyExpr);
                    bool removed = false;
                    auto equalsSimple = [&](const SimpleValue &sv) -> bool {
                        if (std::holds_alternative<int>(v) && std::holds_alternative<int>(sv)) return std::get<int>(v)==std::get<int>(sv);
                        if (std::holds_alternative<std::string>(v) && std::holds_alternative<std::string>(sv)) return std::get<std::string>(v)==std::get<std::string>(sv);
                        if (std::holds_alternative<bool>(v) && std::holds_alternative<bool>(sv)) return std::get<bool>(v)==std::get<bool>(sv);
                        return false;
                    };
                    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const SimpleValue &sv){ if (!removed && equalsSimple(sv)) { removed=true; return true;} return false;}), vec.end());
                }
                assignVariableAny(rite->varName, vec);
            } else {
                std::cerr << "TypeError: '" << rite->varName << "' is not an order" << std::endl; return;
            }
        } else {
            // Tome amend/erase (supports arena-backed Tome and legacy map)
            Value keyV = evaluateValue(rite->keyExpr);
            std::string key;
            if (std::holds_alternative<std::string>(keyV)) key = std::get<std::string>(keyV);
            else if (std::holds_alternative<Phrase>(keyV)) { const Phrase &p = std::get<Phrase>(keyV); key.assign(p.data(), p.size()); }
            else { std::cerr << "TypeError: Tome keys must be phrases" << std::endl; return; }
            if (std::holds_alternative<Tome>(current)) {
                const Tome &oldT = std::get<Tome>(current);
                // Compute new size and copy entries
                size_t n = oldT.size; size_t newN = n;
                bool found = false;
                for (size_t i = 0; i < n; ++i) { if (oldT.data[i].key == key) { found = true; break; } }
                if (rite->riteType == CollectionRiteType::TomeAmend && !found) newN = n + 1;
                if (rite->riteType == CollectionRiteType::TomeErase && found) newN = n - 1;
                void* mem = activeArena().alloc(sizeof(TomeEntry) * newN, alignof(TomeEntry));
                auto* buf = reinterpret_cast<TomeEntry*>(mem);
                size_t w = 0;
                if (rite->riteType == CollectionRiteType::TomeAmend) {
                    // Compute new value
                    Value val = evaluateValue(rite->valueExpr);
                    SimpleValue sv;
                    if (std::holds_alternative<int>(val)) sv = std::get<int>(val);
                    else if (std::holds_alternative<std::string>(val)) sv = std::get<std::string>(val);
                    else if (std::holds_alternative<Phrase>(val)) { const Phrase &p = std::get<Phrase>(val); sv = std::string(p.data(), p.size()); }
                    else if (std::holds_alternative<bool>(val)) sv = std::get<bool>(val);
                    else { std::cerr << "TypeError: Tome values must be simple" << std::endl; return; }
                    bool updated = false;
                    for (size_t i = 0; i < n; ++i) {
                        if (oldT.data[i].key == key) { new (&buf[w++]) TomeEntry{ key, sv }; updated = true; }
                        else { new (&buf[w++]) TomeEntry{ oldT.data[i].key, oldT.data[i].value }; }
                    }
                    if (!updated) { new (&buf[w++]) TomeEntry{ key, sv }; }
                } else { // erase
                    for (size_t i = 0; i < n; ++i) { if (oldT.data[i].key != key) { new (&buf[w++]) TomeEntry{ oldT.data[i].key, oldT.data[i].value }; } }
                }
                assignVariableAny(rite->varName, Tome{ newN, buf });
            } else if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(current)) {
                auto mp = std::get<std::unordered_map<std::string, SimpleValue>>(current); // clone
                if (rite->riteType == CollectionRiteType::TomeAmend) {
                    Value val = evaluateValue(rite->valueExpr);
                    if (std::holds_alternative<int>(val)) mp[key] = std::get<int>(val);
                    else if (std::holds_alternative<std::string>(val)) mp[key] = std::get<std::string>(val);
                    else if (std::holds_alternative<bool>(val)) mp[key] = std::get<bool>(val);
                    else { std::cerr << "TypeError: Tome values must be simple" << std::endl; }
                } else { mp.erase(key); }
                assignVariableAny(rite->varName, mp);
            } else {
                std::cerr << "TypeError: '" << rite->varName << "' is not a tome" << std::endl; return;
            }
        }
    }
    else if (auto impAll = std::dynamic_pointer_cast<ImportAll>(ast)) {
        Module m = loadModule(impAll->path);
        if (!impAll->alias.empty()) {
            for (const auto &p : m.spells) {
                registerSpell(impAll->alias + "." + p.first, p.second);
            }
            // Variables under alias are not namespaced for now.
        } else {
            // Merge variables and spells into current global scope
            for (const auto &v : m.variables) {
                assignVariableAny(v.first, v.second);
            }
            for (const auto &p : m.spells) {
                registerSpell(p.first, p.second);
            }
        }
    }
    else if (auto impSel = std::dynamic_pointer_cast<ImportSelective>(ast)) {
        Module m = loadModule(impSel->path);
        for (const auto &name : impSel->names) {
            auto it = m.spells.find(name);
            if (it == m.spells.end()) {
                std::cerr << "The scroll yields no such spell '" << name << "' to be taken." << std::endl;
                continue;
            }
            registerSpell(name, it->second);
        }
    }
    else if (auto unfurl = std::dynamic_pointer_cast<UnfurlInclude>(ast)) {
        // Inline include: read, parse, and execute in current context
        std::error_code ec;
        if (!std::filesystem::exists(unfurl->path, ec)) {
            std::cerr << "The scroll cannot be found at this path: '" << unfurl->path << "'." << std::endl;
            return;
        }
        std::ifstream in(unfurl->path);
        if (!in) { std::cerr << "The scroll cannot be found at this path: '" << unfurl->path << "'." << std::endl; return; }
        std::stringstream buffer; buffer << in.rdbuf();
    Lexer lx(buffer.str());
    auto toks = lx.tokenize();
    Arena astArena; // ephemeral arena for included scroll parse
    Parser p(std::move(toks), &astArena);
        auto otherAst = p.parse();
        if (!otherAst) return;
        execute(otherAst);
    }
    else if (auto spellDef = std::dynamic_pointer_cast<SpellStatement>(ast)) {
        // Register spell
        spells[spellDef->spellName] = {spellDef->params, spellDef->body};
    }
    else if (auto invoke = std::dynamic_pointer_cast<SpellInvocation>(ast)) {
        auto it = spells.find(invoke->spellName);
        if (it == spells.end()) {
            std::cerr << "Error: Unknown spell '" << invoke->spellName << "'" << std::endl;
            return;
        }
        const auto &def = it->second;
        if (def.params.size() != invoke->args.size()) {
            std::cerr << "Error: Spell '" << invoke->spellName << "' expects " << def.params.size() << " arguments but got " << invoke->args.size() << std::endl;
            return;
        }
        // Enter a new scope for spell parameters
        enterScope();
        for (size_t i = 0; i < def.params.size(); ++i) {
            Value val = evaluateValue(invoke->args[i]);
            declareVariable(def.params[i], val);
        }
        // Execute body and capture possible return value
        Value retVal = 0;
        bool hasReturn = false;
        for (auto &stmt : def.body->statements) {
            if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(stmt)) {
                retVal = evaluateValue(ret->expression);
                hasReturn = true;
                break; // stop executing further statements after return
            }
            execute(stmt);
        }
        // Exit spell scope
        exitScope();
        // If invocation used in a print or assignment context, variable evaluation handles it.
        // For standalone invocation, if returned a phrase, print it automatically (optional design choice).
        if (hasReturn && std::holds_alternative<std::string>(retVal)) {
            std::cout << std::get<std::string>(retVal) << std::endl;
        } else if (hasReturn && std::holds_alternative<int>(retVal)) {
            std::cout << std::get<int>(retVal) << std::endl;
        } else if (hasReturn && std::holds_alternative<bool>(retVal)) {
            std::cout << (std::get<bool>(retVal) ? "True" : "False") << std::endl;
        }
    }
    else if (auto native = std::dynamic_pointer_cast<NativeInvocation>(ast)) {
        // Evaluate for side effects or exceptions; do not print automatically
        (void)evaluateValue(native);
    }
    


      // Handle print statements.
else if (auto printStmt = std::dynamic_pointer_cast<PrintStatement>(ast)) {
    // Only preflight suppress when directly printing an index expression; otherwise print normally
    if (std::dynamic_pointer_cast<IndexExpression>(printStmt->expression)) {
        runtimeError = false;
        (void)evaluateValue(printStmt->expression);
        bool hadError = runtimeError;
        runtimeError = false;
        if (hadError) {
            std::cout << "" << std::endl;
            return;
        }
    }
    std::string output = evaluatePrintExpr(printStmt->expression);
    std::cout << output << std::endl;
}
    else {
        std::cerr << "Error: Unknown AST Node encountered!" << std::endl;
    }
}


void Interpreter::executeWhileLoop(std::shared_ptr<WhileLoop> loop) {
    std::string varName = loop->loopVar->token.value;
    int limitVal = evaluateExpr(loop->limit);
    int stepVal = evaluateExpr(loop->step);
    if (findScopeIndex(varName) < 0) {
        std::cerr << "Error: Undefined loop variable '" << varName << "'" << std::endl;
        return;
    }

    while (true) {
    int currentVal = getIntVariable(varName);
        bool conditionMet;
        switch (loop->comparisonOp) {
            case TokenType::SURPASSETH:
                conditionMet = (currentVal > limitVal);
                break;
            case TokenType::REMAINETH:
                conditionMet = (currentVal < limitVal);
                break;
            default:
                std::cerr << "Error: Invalid comparison operator" << std::endl;
                return;
        }

        if (!conditionMet) break;

        // Execute body in its own scope per iteration
        enterScope();
        for (const auto& stmt : loop->body) { execute(stmt); }
        exitScope();

        // Update variable
        if (loop->stepDirection == TokenType::DESCEND) assignVariableAny(varName, Value(getIntVariable(varName) - stepVal));
        else assignVariableAny(varName, Value(getIntVariable(varName) + stepVal));
    }
}


void Interpreter::executeForLoop(std::shared_ptr<ForLoop> loop) {
    // Initialize loop variable 
    std::string varName;
    if (auto expr = std::dynamic_pointer_cast<Expression>(loop->init)) {
    varName = expr->token.value;
    assignVariableAny(varName, Value(evaluateExpr(loop->init)));
    }

    // Loop while condition holds
    while (evaluateExpr(loop->condition)) {
        // Execute the loop body (BlockStatement) in new scope each iteration
        enterScope();
        execute(loop->body);
        exitScope();
    int stepVal = evaluateExpr(loop->increment);
    if (loop->stepDirection == TokenType::DESCEND) stepVal = -stepVal;
    // Increment loop variable
    assignVariableAny(varName, Value(getIntVariable(varName) + stepVal));
    }
}


void Interpreter::executeDoWhileLoop(std::shared_ptr<DoWhileLoop> loop) {
    std::string varName = loop->loopVar->token.value;
    if (findScopeIndex(varName) < 0) {
        std::cerr << "Error: Undefined loop variable '" << varName << "'" << std::endl;
        return;
    }
    do {
        // Execute body
        enterScope();
        for (const auto &stmt : loop->body->statements) { execute(stmt); }
        exitScope();
        // Apply update
        if (loop->update) {
            int inc = evaluateExpr(loop->update);
            if (loop->stepDirection == TokenType::DESCEND) inc = -inc;
            assignVariableAny(varName, Value(getIntVariable(varName) + inc));
        }
    } while (evaluateExpr(loop->condition)); // Loop while condition is TRUE
}



void Interpreter::evaluateExpression(std::shared_ptr<ASTNode> expr) {
    std::cout << "Inside evaluateExpression()" << std::endl;
    if (auto value = std::dynamic_pointer_cast<Expression>(expr)) {
        if (value->token.type == TokenType::IDENTIFIER) {
            std::string varName = value->token.value;
            Value v;
            if (lookupVariable(varName, v)) {
                if (std::holds_alternative<int>(v)) std::cout << "valueeee: " << std::get<int>(v) << std::endl;
                else if (std::holds_alternative<std::string>(v)) std::cout << "valueeee: " << std::get<std::string>(v) << std::endl;
                else if (std::holds_alternative<bool>(v)) std::cout << "valueeee: " << (std::get<bool>(v) ? "True" : "False") << std::endl;
            } else {
                std::cerr << "Error: Undefined variable '" << varName << "'" << std::endl;
            }
        } else {
            std::cout << value->token.value << std::endl;
        }
    } else {
        std::cerr << "Error: Invalid Expression Node!" << std::endl;
    }
}
std::string Interpreter::evaluatePrintExpr(std::shared_ptr<ASTNode> expr) {
    if (runtimeError) {
        // Suppress output for the current print after an error was signaled
        runtimeError = false;
        return std::string("");
    }
    if (auto strExpr = std::dynamic_pointer_cast<Expression>(expr)) {
        if (strExpr->token.type == TokenType::STRING) {
            return strExpr->token.value;
        } else if (strExpr->token.type == TokenType::BOOLEAN) {
            return strExpr->token.value; // print literal True/False
        } else if (strExpr->token.type == TokenType::IDENTIFIER) {
            // Return string value for identifiers if present, otherwise number/bool-as-string
            Value v;
            if (lookupVariable(strExpr->token.value, v)) {
                if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
                if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return std::string(p.data(), p.size()); }
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
                if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
                if (std::holds_alternative<Order>(v) || std::holds_alternative<Tome>(v) ||
                    std::holds_alternative<std::vector<SimpleValue>>(v) || std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(v)) return formatValue(v);
            }
            std::cerr << "Error: Undefined variable '" << strExpr->token.value << "'" << std::endl;
            return "";
        } else {
            int n = evaluateExpr(expr);
            if (runtimeError) {
                runtimeError = false;
                return std::string("");
            }
            return std::to_string(n);
        }
    } else if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value v = evaluateValue(expr);
        if (runtimeError) {
            runtimeError = false;
            return std::string("");
        }
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return std::string(p.data(), p.size()); }
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
        if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
        // Collections and other values via pretty-printer
        return formatValue(v);
    } else if (auto castExpr = std::dynamic_pointer_cast<CastExpression>(expr)) {
        if (castExpr->target == CastTarget::ToPhrase) {
            Value v = evaluateValue(castExpr->operand);
            if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
            if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return std::string(p.data(), p.size()); }
            if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
            return std::string("");
        } else {
            // For number/truth casts, print numeric truthiness
            int v = evaluateExpr(expr);
            return std::to_string(v);
        }
    } else if (auto unary = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
        // Pretty boolean output for NOT expressions
        int v = evaluateExpr(expr);
        return v != 0 ? std::string("True") : std::string("False");
    } else if (auto binExpr = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        // Logical and comparison expressions -> pretty boolean output
        if (binExpr->op.type == TokenType::AND || binExpr->op.type == TokenType::OR ||
            binExpr->op.type == TokenType::SURPASSETH || binExpr->op.type == TokenType::REMAINETH ||
            binExpr->op.type == TokenType::EQUAL || binExpr->op.type == TokenType::NOT_EQUAL ||
            binExpr->op.type == TokenType::GREATER || binExpr->op.type == TokenType::LESSER) {
            int v = evaluateExpr(expr);
            return v != 0 ? std::string("True") : std::string("False");
        }
        // Handle '+' with coercion rules
        if (binExpr->op.type == TokenType::OPERATOR && binExpr->op.value == "+") {
            std::function<bool(const std::shared_ptr<ASTNode>&)> isStringNode;
            isStringNode = [&](const std::shared_ptr<ASTNode>& n) -> bool {
                if (auto e = std::dynamic_pointer_cast<Expression>(n)) {
                    if (e->token.type == TokenType::STRING) return true;
                    if (e->token.type == TokenType::IDENTIFIER) {
                        Value tmp;
                        if (lookupVariable(e->token.value, tmp) && (std::holds_alternative<std::string>(tmp) || std::holds_alternative<Phrase>(tmp))) return true;
                    }
                }
                if (auto c = std::dynamic_pointer_cast<CastExpression>(n)) {
                    if (c->target == CastTarget::ToPhrase) return true;
                }
                if (auto b = std::dynamic_pointer_cast<BinaryExpression>(n)) {
                    if (b->op.type == TokenType::OPERATOR && b->op.value == "+") {
                        return isStringNode(b->left) || isStringNode(b->right);
                    }
                }
                return false;
            };
            bool stringy = isStringNode(binExpr->left) || isStringNode(binExpr->right);
            if (stringy) {
                std::string left = evaluatePrintExpr(binExpr->left);
                std::string right = evaluatePrintExpr(binExpr->right);
                bool leftEndsSpace = !left.empty() && left.back() == ' ';
                bool rightStartsSpace = !right.empty() && right.front() == ' ';
                bool rightStartsPunct = (!right.empty() && std::string(",.;:)]}").find(right.front()) != std::string::npos);
                // Insert a single space boundary when concatenating phrase-like parts,
                // except when the right-hand side begins with punctuation.
                if (!leftEndsSpace && !rightStartsSpace && !rightStartsPunct) {
                    left.push_back(' ');
                }
                // If both sides already have a boundary space, collapse to a single space
                if (!left.empty() && !right.empty() && left.back() == ' ' && right.front() == ' ') {
                    right.erase(0, 1);
                }
                return left + right;
            } else {
                // numeric domain (+): number + number, number + truth, truth + number
                int sum = evaluateExpr(binExpr->left) + evaluateExpr(binExpr->right);
                return std::to_string(sum);
            }
        }
        // Other arithmetic ops: evaluate then print as number
        int v = evaluateExpr(expr);
        return std::to_string(v);
    } else if (auto invoke = std::dynamic_pointer_cast<SpellInvocation>(expr)) {
        // Evaluate a spell invocation to a value and pretty-print it
        // Resolve and bind args
        auto it = spells.find(invoke->spellName);
        if (it == spells.end()) {
            std::cerr << "Error: Unknown spell '" << invoke->spellName << "'" << std::endl;
            return std::string("");
        }
        const auto &def = it->second;
        if (def.params.size() != invoke->args.size()) {
            std::cerr << "Error: Spell '" << invoke->spellName << "' expects " << def.params.size() << " arguments but got " << invoke->args.size() << std::endl;
            return std::string("");
        }
        enterScope();
        for (size_t i = 0; i < def.params.size(); ++i) {
            Value val = evaluateValue(invoke->args[i]);
            declareVariable(def.params[i], val);
        }
        Value retVal = std::string("");
        bool hasReturn = false;
        for (auto &stmt : def.body->statements) {
            if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(stmt)) {
                retVal = evaluateValue(ret->expression);
                hasReturn = true;
                break;
            }
            execute(stmt);
        }
        exitScope();
        // Pretty print result if present
        if (!hasReturn) return std::string("");
        if (std::holds_alternative<std::string>(retVal)) return std::get<std::string>(retVal);
        if (std::holds_alternative<Phrase>(retVal)) { const Phrase &p = std::get<Phrase>(retVal); return std::string(p.data(), p.size()); }
        if (std::holds_alternative<bool>(retVal)) return std::get<bool>(retVal) ? std::string("True") : std::string("False");
        if (std::holds_alternative<int>(retVal)) return std::to_string(std::get<int>(retVal));
        return formatValue(retVal);
    } else if (auto native = std::dynamic_pointer_cast<NativeInvocation>(expr)) {
        auto it = nativeRegistry.find(native->funcName);
        if (it == nativeRegistry.end()) {
            std::cerr << "The spirits know not the rite '" << native->funcName << "'." << std::endl;
            return std::string("");
        }
        std::vector<Value> argv;
        argv.reserve(native->args.size());
        for (auto &a : native->args) argv.push_back(evaluateValue(a));
        Value ret;
        try { ret = it->second(argv); }
        catch (...) {
            std::cerr << "A rift silences the spirits during '" << native->funcName << "'." << std::endl;
            return std::string("");
        }
        if (std::holds_alternative<std::string>(ret)) return std::get<std::string>(ret);
        if (std::holds_alternative<bool>(ret)) return std::get<bool>(ret) ? std::string("True") : std::string("False");
        if (std::holds_alternative<int>(ret)) return std::to_string(std::get<int>(ret));
        return formatValue(ret);
    }
    // Direct array/object literals: pretty-print
    if (std::dynamic_pointer_cast<ArrayLiteral>(expr) || std::dynamic_pointer_cast<ObjectLiteral>(expr)) {
        Value v = evaluateValue(expr);
        if (runtimeError) {
            runtimeError = false;
            return std::string("");
        }
        return formatValue(v);
    }
    // Generic fallback: if evaluating yields a collection, pretty-print it
    {
        Value v = evaluateValue(expr);
        if (runtimeError) {
            runtimeError = false;
            return std::string("");
        }
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
        if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
        if (std::holds_alternative<std::vector<SimpleValue>>(v) ||
            std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(v)) {
            return formatValue(v);
        }
    }
    return "";
}

Interpreter::Module Interpreter::loadModule(const std::string& path) {
    // Circular import guard
    if (g_importing[path]) {
        std::cerr << "The scroll '" << path << "' folds upon itself — circular invocation forbidden." << std::endl;
        return Module{};
    }
    // Cache hit
    auto found = moduleCache.find(path);
    if (found != moduleCache.end()) return found->second;
    // File exists?
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::cerr << "The scroll cannot be found at this path: '" << path << "'." << std::endl;
        return Module{};
    }
    // Read file contents
    std::ifstream in(path);
    if (!in) { std::cerr << "The scroll cannot be found at this path: '" << path << "'." << std::endl; return Module{}; }
    std::stringstream buffer; buffer << in.rdbuf();
    // Parse using global arena so spell bodies persist
    Lexer lx(buffer.str());
    auto toks = lx.tokenize();
    Parser p(std::move(toks), &globalArena_);
    auto ast = p.parse();
    if (!ast) return Module{};
    g_importing[path] = true;
    // Execute directly; declarations land in global scope, spells registered here
    execute(ast);
    g_importing.erase(path);
    Module m;
    m.variables = scopes.front(); // snapshot of global variables (may include pre-existing ones)
    m.spells = spells;            // all spells currently registered
    moduleCache[path] = m;
    return m;
}

// ===== REPL helpers =====
Interpreter::Value Interpreter::evaluateReplValue(std::shared_ptr<ASTNode> node) {
    if (!node) return 0;
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(node)) {
        if (!block->statements.empty()) {
            auto stmt = block->statements.back();
            if (auto printStmt = std::dynamic_pointer_cast<PrintStatement>(stmt)) {
                return evaluateValue(printStmt->expression);
            }
            if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(stmt)) {
                if (bin->op.type == TokenType::IS_OF) {
                    return evaluateValue(bin->right);
                }
            }
            if (auto nat = std::dynamic_pointer_cast<NativeInvocation>(stmt)) {
                return evaluateValue(nat);
            }
            if (auto spellInv = std::dynamic_pointer_cast<SpellInvocation>(stmt)) {
                return evaluateValue(spellInv);
            }
        }
        return 0;
    }
    if (std::dynamic_pointer_cast<Expression>(node) ||
        std::dynamic_pointer_cast<IndexExpression>(node) ||
        std::dynamic_pointer_cast<CastExpression>(node) ||
        std::dynamic_pointer_cast<BinaryExpression>(node) ||
        std::dynamic_pointer_cast<UnaryExpression>(node) ||
        std::dynamic_pointer_cast<NativeInvocation>(node) ||
        std::dynamic_pointer_cast<SpellInvocation>(node) ||
        std::dynamic_pointer_cast<ArrayLiteral>(node) ||
        std::dynamic_pointer_cast<ObjectLiteral>(node)) {
        return evaluateValue(node);
    }
    return 0;
}

std::string Interpreter::stringifyValueForRepl(const Interpreter::Value& v) {
    if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
    if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<Phrase>(v)) { const Phrase &p = std::get<Phrase>(v); return std::string(p.data(), p.size()); }
    return formatValue(v);
}
 
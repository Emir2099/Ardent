// ─────────────────────────────────────────────────────────────────────────
// Ardent 3.0  ·  Type Checker
// ─────────────────────────────────────────────────────────────────────────
// Static type checking pass that validates type constraints after inference.
// Reports errors for type mismatches and validates spell signatures.
// In AOT mode, enforces stricter rules for zero runtime type errors.
// ─────────────────────────────────────────────────────────────────────────
#ifndef TYPE_CHECK_H
#define TYPE_CHECK_H

#include "ast.h"
#include "types.h"
#include "type_infer.h"
#include <vector>
#include <string>
#include <memory>

namespace ardent {

// ─── Compilation Mode ──────────────────────────────────────────────────
enum class CompilationMode {
    Interpreter,  // Lenient: allows dynamic features, runtime type checks
    AOT           // Strict: zero runtime type errors, no dynamic features
};

// ─── Type Error ────────────────────────────────────────────────────────
struct TypeError {
    int line;
    std::string message;
    std::string hint;        // Optional suggestion for fixing
    bool isWarning {false};  // true = warning, false = error
    
    TypeError(int l, const std::string& msg, const std::string& h = "", bool warn = false)
        : line(l), message(msg), hint(h), isWarning(warn) {}
    
    std::string format() const;
};

// ─── Type Check Result ─────────────────────────────────────────────────
struct TypeCheckResult {
    std::vector<TypeError> errors;
    std::vector<TypeError> warnings;
    bool success() const { return errors.empty(); }
    
    // Pretty-print all diagnostics
    std::string formatAll() const;
};

// ─── Spell Signature for Call Validation ───────────────────────────────
struct SpellSignature {
    std::string name;
    std::vector<Type> paramTypes;
    Type returnType;
    bool isVariadic {false};  // For spells like print
    bool isPure {true};       // Ardent 3.0: purity tracking
    bool hasAllReturns {true}; // All control paths return
    int declarationLine {0};
};

// ─── Type Checker ──────────────────────────────────────────────────────
class TypeChecker {
public:
    explicit TypeChecker(InferenceContext& inferCtx, CompilationMode mode = CompilationMode::Interpreter);
    
    // Run full type check on program
    TypeCheckResult check(const std::vector<std::shared_ptr<ASTNode>>& program);
    
    // Check individual constructs
    void checkStatement(ASTNode* node);
    void checkExpression(ASTNode* node);
    
private:
    InferenceContext& inferCtx_;
    TypeCheckResult result_;
    CompilationMode mode_;
    
    // Registered spell signatures
    std::unordered_map<std::string, SpellSignature> spells_;
    
    // Current function context (for return type checking)
    Type currentReturnType_ {Type::unknown()};
    std::string currentSpellName_;
    
    // Helpers
    void registerSpell(SpellStatement* spell);
    void checkSpellCall(SpellInvocation* call);
    void checkAssignment(const Type& lhs, const Type& rhs, int line);
    void checkCondition(ASTNode* cond);
    
    // Ardent 3.0: AOT-specific checks
    void checkReturnPaths(SpellStatement* spell);
    void checkNoDynamicFeatures(ASTNode* node);
    bool hasDeterministicReturn(ASTNode* body);
    bool isPureSpell(SpellStatement* spell);
    void rejectAmbiguousConversion(const Type& from, const Type& to, int line);
    
    void addError(int line, const std::string& msg, const std::string& hint = "");
    void addWarning(int line, const std::string& msg, const std::string& hint = "");
};

// ─── Convenience Functions ─────────────────────────────────────────────

// Full type-check pipeline: inference + validation
TypeCheckResult typeCheckProgram(const std::vector<std::shared_ptr<ASTNode>>& program,
                                  bool verbose = false,
                                  CompilationMode mode = CompilationMode::Interpreter);

// Check if a value can be safely coerced to target type
bool canCoerceTo(const Type& from, const Type& to);

// Ardent 3.0: Strict coercion check for AOT (no implicit conversions)
bool canCoerceToStrict(const Type& from, const Type& to);

} // namespace ardent

#endif // TYPE_CHECK_H

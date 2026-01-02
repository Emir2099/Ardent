// ─────────────────────────────────────────────────────────────────────────
// Ardent 3.0  ·  Type Inference Pass
// ─────────────────────────────────────────────────────────────────────────
// Flow-sensitive type inference for Ardent programs. This pass annotates
// AST nodes with inferred types without requiring explicit :rune annotations.
// Works in harmony with gradual typing - inferred types can be overridden
// by explicit declarations.
// ─────────────────────────────────────────────────────────────────────────
#ifndef TYPE_INFER_H
#define TYPE_INFER_H

#include "ast.h"
#include "types.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

namespace ardent {

// ─── Type Environment for Inference ────────────────────────────────────
// Tracks inferred types through scopes during the inference pass.
class TypeEnv {
public:
    void pushScope();
    void popScope();
    
    // Declare or update a variable's type in current scope
    void declare(const std::string& name, Type type);
    void update(const std::string& name, Type type);
    
    // Lookup type (searches outward through scopes)
    Type lookup(const std::string& name) const;
    bool exists(const std::string& name) const;
    
private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
};

// ─── Type Inference Context ────────────────────────────────────────────
// Holds state during inference, including diagnostics.
struct InferenceContext {
    TypeEnv env;
    std::vector<std::string> warnings;  // Non-fatal type notes
    std::vector<std::string> errors;    // Fatal type errors
    bool verbose {false};               // Enable for --explain-types
    
    void warn(int line, const std::string& msg);
    void error(int line, const std::string& msg);
};

// ─── Type Inference Pass ───────────────────────────────────────────────
// Main entry point for running type inference on an AST.
class TypeInferrer {
public:
    explicit TypeInferrer(InferenceContext& ctx);
    
    // Infer types for an entire program (list of statements)
    void inferProgram(const std::vector<std::shared_ptr<ASTNode>>& stmts);
    
    // Infer type of a single statement or expression
    void inferStatement(ASTNode* node);
    Type inferExpression(ASTNode* node);
    
    // Specialized inferrers
    Type inferBinaryExpr(BinaryExpression* node);
    Type inferSpellInvocation(SpellInvocation* node);
    Type inferArrayLiteral(ArrayLiteral* node);
    Type inferMapLiteral(ObjectLiteral* node);
    
private:
    InferenceContext& ctx_;
    
    // Spell return type cache (spell name -> return type)
    std::unordered_map<std::string, Type> spellReturnTypes_;
    
    // Register a spell's signature for later lookup
    void registerSpell(SpellStatement* spell);
    
    // Get return type of a registered spell
    Type getSpellReturnType(const std::string& name) const;
};

// ─── Convenience Functions ─────────────────────────────────────────────

// Run type inference on program, returning context with results
InferenceContext inferTypes(const std::vector<std::shared_ptr<ASTNode>>& program,
                            bool verbose = false);

// Get human-readable type explanation for an expression
std::string explainType(ASTNode* node, const InferenceContext& ctx);

} // namespace ardent

#endif // TYPE_INFER_H

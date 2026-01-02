// ─────────────────────────────────────────────────────────────────────────
// Ardent 3.0 Optimizer
// ─────────────────────────────────────────────────────────────────────────
// Optimization passes for Ardent AST before LLVM IR generation.
// Includes: constant folding, purity analysis, partial evaluation,
// and dead code elimination.
// ─────────────────────────────────────────────────────────────────────────
#ifndef ARDENT_OPTIMIZER_H
#define ARDENT_OPTIMIZER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>
#include "ast.h"
#include "token.h"

namespace opt {

// ============================================================================
// Constant Folding Pass
// ============================================================================
// Evaluates compile-time expressions and replaces nodes with literal results.
// Handles: numeric ops, boolean logic, phrase concatenation, comparisons.

using ConstValue = std::variant<int, std::string, bool>;

class ConstantFolder {
public:
    // Transform an AST, returning a (possibly modified) root.
    // The returned node may be the same as input if no folding occurred,
    // or a new Expression node holding a literal value.
    std::shared_ptr<ASTNode> fold(std::shared_ptr<ASTNode> node);

    // Statistics
    size_t foldedCount() const { return foldedCount_; }

private:
    size_t foldedCount_{0};

    // Attempt to extract a constant value from a node (only if it's a literal).
    std::optional<ConstValue> asConstant(const std::shared_ptr<ASTNode>& node) const;

    // Create a literal Expression node from a constant value.
    std::shared_ptr<ASTNode> makeLiteral(const ConstValue& cv) const;

    // Fold binary operation if both operands are constants.
    std::optional<ConstValue> foldBinary(const std::string& op, const ConstValue& lhs, const ConstValue& rhs) const;

    // Fold unary operation.
    std::optional<ConstValue> foldUnary(TokenType op, const ConstValue& operand) const;

    // Fold comparison operations.
    std::optional<bool> foldComparison(TokenType op, const ConstValue& lhs, const ConstValue& rhs) const;
};

// ============================================================================
// Pure Spell Detection
// ============================================================================
// A spell is pure if:
// - No I/O (print statements)
// - No chronicle operations
// - No external mutation (assignments to variables outside parameters/locals)
// - Only calls other pure spells
// - Depends only on parameters and constants

class PurityAnalyzer {
public:
    // Analyze all spells in the AST. Call after parsing to populate purity info.
    void analyze(std::shared_ptr<ASTNode> root);

    // Check if a spell is pure (must call analyze first).
    bool isPure(const std::string& spellName) const;

    // Get all pure spells.
    const std::unordered_set<std::string>& pureSpells() const { return pureSpells_; }

private:
    std::unordered_map<std::string, std::shared_ptr<SpellStatement>> spellDefs_;
    std::unordered_set<std::string> pureSpells_;
    std::unordered_set<std::string> impureSpells_; // known impure
    std::unordered_set<std::string> analyzing_;    // cycle detection

    // Determine purity of a single spell (recursive with memoization).
    bool computePurity(const std::string& name);

    // Check if a block/statement is pure given a set of allowed local variables.
    bool isBlockPure(const std::shared_ptr<BlockStatement>& block,
                     const std::unordered_set<std::string>& locals);

    bool isStatementPure(const std::shared_ptr<ASTNode>& stmt,
                         const std::unordered_set<std::string>& locals);

    bool isExpressionPure(const std::shared_ptr<ASTNode>& expr,
                          const std::unordered_set<std::string>& locals);
};

// ============================================================================
// Partial Evaluator
// ============================================================================
// Folds pure spell invocations with constant arguments at compile time.

class PartialEvaluator {
public:
    // Must provide purity info and spell definitions.
    PartialEvaluator(const PurityAnalyzer& purity,
                     const std::unordered_map<std::string, std::shared_ptr<SpellStatement>>& spells)
        : purity_(purity), spells_(spells) {}

    // Transform AST by partially evaluating pure spell calls with constant args.
    std::shared_ptr<ASTNode> evaluate(std::shared_ptr<ASTNode> node);

    size_t evaluatedCount() const { return evaluatedCount_; }

private:
    const PurityAnalyzer& purity_;
    const std::unordered_map<std::string, std::shared_ptr<SpellStatement>>& spells_;
    size_t evaluatedCount_{0};

    // Attempt to evaluate a pure spell with constant arguments.
    std::optional<ConstValue> tryEvaluate(const std::string& spellName,
                                          const std::vector<ConstValue>& args);

    // Simple interpreter for pure spells (no side effects).
    std::optional<ConstValue> interpretPure(const std::shared_ptr<BlockStatement>& body,
                                            const std::vector<std::string>& params,
                                            const std::vector<ConstValue>& args);

    std::optional<ConstValue> evalExpr(const std::shared_ptr<ASTNode>& expr,
                                       std::unordered_map<std::string, ConstValue>& env);
};

// ============================================================================
// Optimizer Entry Point
// ============================================================================
// Runs all optimization passes in sequence.

class Optimizer {
public:
    // Run all passes on an AST. Returns optimized AST.
    std::shared_ptr<ASTNode> optimize(std::shared_ptr<ASTNode> root);

    // Access analyzers for external use (e.g., IR compilation).
    const PurityAnalyzer& purity() const { return purity_; }

    // Statistics
    size_t constantsFolded() const { return folder_.foldedCount(); }
    size_t spellsEvaluated() const { return partialEvaluator_ ? partialEvaluator_->evaluatedCount() : 0; }

private:
    ConstantFolder folder_;
    PurityAnalyzer purity_;
    std::unique_ptr<PartialEvaluator> partialEvaluator_;
    std::unordered_map<std::string, std::shared_ptr<SpellStatement>> spellDefs_;

    // Collect spell definitions from AST.
    void collectSpells(std::shared_ptr<ASTNode> node);
};

} // namespace opt

#endif // ARDENT_OPTIMIZER_H

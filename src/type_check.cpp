// ─────────────────────────────────────────────────────────────────────────
// Ardent 2.2  ·  Type Checker Implementation
// ─────────────────────────────────────────────────────────────────────────
#include "type_check.h"
#include <sstream>
#include <algorithm>

namespace ardent {

// ─── TypeError Implementation ──────────────────────────────────────────

std::string TypeError::format() const {
    std::ostringstream oss;
    oss << (isWarning ? "Warning" : "Error") << " [Line " << line << "]: " << message;
    if (!hint.empty()) {
        oss << "\n  Hint: " << hint;
    }
    return oss.str();
}

// ─── TypeCheckResult Implementation ────────────────────────────────────

std::string TypeCheckResult::formatAll() const {
    std::ostringstream oss;
    
    for (const auto& warn : warnings) {
        oss << warn.format() << "\n";
    }
    for (const auto& err : errors) {
        oss << err.format() << "\n";
    }
    
    if (errors.empty() && warnings.empty()) {
        oss << "No type errors found.\n";
    } else {
        oss << "\nSummary: " << errors.size() << " error(s), " 
            << warnings.size() << " warning(s)\n";
    }
    
    return oss.str();
}

// ─── TypeChecker Implementation ────────────────────────────────────────

TypeChecker::TypeChecker(InferenceContext& inferCtx) 
    : inferCtx_(inferCtx) {}

void TypeChecker::addError(int line, const std::string& msg, const std::string& hint) {
    result_.errors.emplace_back(line, msg, hint, false);
}

void TypeChecker::addWarning(int line, const std::string& msg, const std::string& hint) {
    result_.warnings.emplace_back(line, msg, hint, true);
}

TypeCheckResult TypeChecker::check(const std::vector<std::shared_ptr<ASTNode>>& program) {
    // First pass: register all spell signatures
    for (const auto& stmt : program) {
        if (auto* spell = dynamic_cast<SpellStatement*>(stmt.get())) {
            registerSpell(spell);
        }
    }
    
    // Second pass: type-check all statements
    for (const auto& stmt : program) {
        checkStatement(stmt.get());
    }
    
    return result_;
}

void TypeChecker::registerSpell(SpellStatement* spell) {
    if (!spell) return;
    
    SpellSignature sig;
    sig.name = spell->spellName;
    sig.returnType = spell->returnType;
    sig.declarationLine = spell->sourceLine;
    
    // Collect parameter types
    for (size_t i = 0; i < spell->params.size(); ++i) {
        if (i < spell->paramTypes.size()) {
            sig.paramTypes.push_back(spell->paramTypes[i]);
        } else {
            sig.paramTypes.push_back(Type::unknown()); // Untyped param
        }
    }
    
    spells_[spell->spellName] = sig;
}

void TypeChecker::checkStatement(ASTNode* node) {
    if (!node) return;
    
    // Variable declaration
    if (auto* vd = dynamic_cast<VariableDeclaration*>(node)) {
        checkExpression(vd->initializer.get());
        
        if (vd->typeInfo.hasRune) {
            checkAssignment(vd->typeInfo.declaredType, 
                           vd->typeInfo.inferredType,
                           vd->sourceLine);
        }
        return;
    }
    
    // Assignment
    if (auto* bin = dynamic_cast<BinaryExpression*>(node)) {
        if (bin->op.type == TokenType::IS_OF) {
            if (auto* lhs = dynamic_cast<Expression*>(bin->left.get())) {
                if (lhs->token.type == TokenType::IDENTIFIER) {
                    checkExpression(bin->right.get());
                    
                    // Check if variable has a declared type
                    Type declType = inferCtx_.env.lookup(lhs->token.value);
                    Type rhsType = bin->right ? bin->right->typeInfo.inferredType : Type::unknown();
                    
                    if (declType.kind != TypeKind::Unknown && 
                        rhsType.kind != TypeKind::Unknown) {
                        checkAssignment(declType, rhsType, node->sourceLine);
                    }
                    return;
                }
            }
        }
        // Other binary expressions
        checkExpression(bin);
        return;
    }
    
    // Block
    if (auto* block = dynamic_cast<BlockStatement*>(node)) {
        for (const auto& stmt : block->statements) {
            checkStatement(stmt.get());
        }
        return;
    }
    
    // If statement
    if (auto* ifStmt = dynamic_cast<IfStatement*>(node)) {
        checkCondition(ifStmt->condition.get());
        checkStatement(ifStmt->thenBranch.get());
        if (ifStmt->elseBranch) {
            checkStatement(ifStmt->elseBranch.get());
        }
        return;
    }
    
    // While loop
    if (auto* whileLoop = dynamic_cast<WhileLoop*>(node)) {
        for (const auto& stmt : whileLoop->body) {
            checkStatement(stmt.get());
        }
        return;
    }
    
    // Spell definition
    if (auto* spell = dynamic_cast<SpellStatement*>(node)) {
        // Set current return type for checking returns
        Type prevReturn = currentReturnType_;
        currentReturnType_ = spell->returnType;
        
        checkStatement(spell->body.get());
        
        currentReturnType_ = prevReturn;
        return;
    }
    
    // Return statement
    if (auto* ret = dynamic_cast<ReturnStatement*>(node)) {
        checkExpression(ret->expression.get());
        
        Type retType = ret->expression ? ret->expression->typeInfo.inferredType : Type::voidTy();
        
        if (currentReturnType_.kind != TypeKind::Unknown) {
            if (!isAssignableFrom(currentReturnType_, retType)) {
                addError(ret->sourceLine,
                    "Return type mismatch: spell expects " + typeToString(currentReturnType_) +
                    " but returning " + typeToString(retType));
            }
        }
        return;
    }
    
    // Print statement - accepts any type
    if (auto* print = dynamic_cast<PrintStatement*>(node)) {
        checkExpression(print->expression.get());
        return;
    }
}

void TypeChecker::checkExpression(ASTNode* node) {
    if (!node) return;
    
    // Spell invocation - validate against signature
    if (auto* inv = dynamic_cast<SpellInvocation*>(node)) {
        checkSpellCall(inv);
        return;
    }
    
    // Binary expression - check operands
    if (auto* bin = dynamic_cast<BinaryExpression*>(node)) {
        checkExpression(bin->left.get());
        checkExpression(bin->right.get());
        
        // Type-specific operator checks
        Type leftType = bin->left ? bin->left->typeInfo.inferredType : Type::unknown();
        Type rightType = bin->right ? bin->right->typeInfo.inferredType : Type::unknown();
        
        // Division by zero warning for literals
        if (bin->op.value == "/" && bin->right) {
            if (auto* rexpr = dynamic_cast<Expression*>(bin->right.get())) {
                if (rexpr->token.type == TokenType::NUMBER && rexpr->token.value == "0") {
                    addWarning(node->sourceLine, "Division by zero", 
                              "This will cause a runtime error");
                }
            }
        }
        
        // Incompatible operand types for arithmetic
        if (bin->op.type == TokenType::OPERATOR) {
            const std::string& op = bin->op.value;
            if (op == "-" || op == "*" || op == "/" || op == "%") {
                if (leftType.kind == TypeKind::Phrase || rightType.kind == TypeKind::Phrase) {
                    addError(node->sourceLine,
                        "Invalid operation: cannot use '" + op + "' with phrase type",
                        "Use '+' for phrase concatenation");
                }
            }
        }
        return;
    }
    
    // Array literal
    if (auto* arr = dynamic_cast<ArrayLiteral*>(node)) {
        for (const auto& elem : arr->elements) {
            checkExpression(elem.get());
        }
        return;
    }
    
    // Map literal
    if (auto* tome = dynamic_cast<ObjectLiteral*>(node)) {
        for (const auto& entry : tome->entries) {
            checkExpression(entry.second.get());
        }
        return;
    }
}

void TypeChecker::checkSpellCall(SpellInvocation* call) {
    if (!call) return;
    
    // Check arguments
    for (const auto& arg : call->args) {
        checkExpression(arg.get());
    }
    
    // Find spell signature
    auto it = spells_.find(call->spellName);
    if (it == spells_.end()) {
        // Unknown spell - could be builtin or error (don't error here, let runtime handle)
        return;
    }
    
    const SpellSignature& sig = it->second;
    
    // Check argument count
    if (!sig.isVariadic && call->args.size() != sig.paramTypes.size()) {
        addError(call->sourceLine,
            "Spell '" + call->spellName + "' expects " + 
            std::to_string(sig.paramTypes.size()) + " argument(s) but got " +
            std::to_string(call->args.size()),
            "Check spell definition at line " + std::to_string(sig.declarationLine));
        return;
    }
    
    // Check argument types
    for (size_t i = 0; i < call->args.size() && i < sig.paramTypes.size(); ++i) {
        Type argType = call->args[i] ? call->args[i]->typeInfo.inferredType : Type::unknown();
        Type paramType = sig.paramTypes[i];
        
        if (paramType.kind != TypeKind::Unknown && 
            argType.kind != TypeKind::Unknown &&
            !isAssignableFrom(paramType, argType)) {
            addError(call->sourceLine,
                "Argument " + std::to_string(i + 1) + " to '" + call->spellName + 
                "' has wrong type: expected " + typeToString(paramType) +
                " but got " + typeToString(argType));
        }
    }
}

void TypeChecker::checkAssignment(const Type& lhs, const Type& rhs, int line) {
    if (lhs.kind == TypeKind::Unknown || rhs.kind == TypeKind::Unknown) {
        return; // Skip check for dynamic types
    }
    
    if (!isAssignableFrom(lhs, rhs)) {
        addError(line,
            "Type mismatch in assignment: cannot assign " + typeToString(rhs) +
            " to " + typeToString(lhs),
            "Consider using explicit type conversion");
    }
}

void TypeChecker::checkCondition(ASTNode* cond) {
    if (!cond) return;
    
    checkExpression(cond);
    
    Type condType = cond->typeInfo.inferredType;
    if (condType.kind != TypeKind::Unknown && condType.kind != TypeKind::Truth) {
        addWarning(cond->sourceLine,
            "Condition has type " + typeToString(condType) + ", expected truth",
            "Non-truth values will be coerced at runtime");
    }
}

// ─── Convenience Functions ─────────────────────────────────────────────

TypeCheckResult typeCheckProgram(const std::vector<std::shared_ptr<ASTNode>>& program,
                                  bool verbose) {
    // Run inference first
    InferenceContext inferCtx = inferTypes(program, verbose);
    
    // Then run type checker
    TypeChecker checker(inferCtx);
    return checker.check(program);
}

bool canCoerceTo(const Type& from, const Type& to) {
    // Everything can coerce to Any
    if (to.kind == TypeKind::Any) return true;
    
    // Unknown can coerce to anything (gradual typing)
    if (from.kind == TypeKind::Unknown) return true;
    
    // Same types always coerce
    if (from.kind == to.kind) return true;
    
    // Numeric widening: whole -> phrase (string conversion)
    if (from.kind == TypeKind::Whole && to.kind == TypeKind::Phrase) return true;
    
    // Truth -> phrase
    if (from.kind == TypeKind::Truth && to.kind == TypeKind::Phrase) return true;
    
    return false;
}

} // namespace ardent

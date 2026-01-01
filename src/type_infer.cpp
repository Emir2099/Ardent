// ─────────────────────────────────────────────────────────────────────────
// Ardent 2.2  ·  Type Inference Pass Implementation
// ─────────────────────────────────────────────────────────────────────────
#include "type_infer.h"
#include <sstream>
#include <algorithm>

namespace ardent {

// ─── TypeEnv Implementation ────────────────────────────────────────────

void TypeEnv::pushScope() {
    scopes_.emplace_back();
}

void TypeEnv::popScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void TypeEnv::declare(const std::string& name, Type type) {
    if (scopes_.empty()) {
        scopes_.emplace_back();
    }
    scopes_.back()[name] = type;
}

void TypeEnv::update(const std::string& name, Type type) {
    // Update in nearest scope where variable exists
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            // Type widening: if new type is compatible, unify
            auto unified = unifyTypes(found->second, type);
            found->second = unified.value_or(type);
            return;
        }
    }
    // Not found - declare in current scope
    declare(name, type);
}

Type TypeEnv::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return Type::unknown();
}

bool TypeEnv::exists(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return true;
        }
    }
    return false;
}

// ─── InferenceContext Implementation ───────────────────────────────────

void InferenceContext::warn(int line, const std::string& msg) {
    std::ostringstream oss;
    oss << "[Line " << line << "] Warning: " << msg;
    warnings.push_back(oss.str());
}

void InferenceContext::error(int line, const std::string& msg) {
    std::ostringstream oss;
    oss << "[Line " << line << "] Error: " << msg;
    errors.push_back(oss.str());
}

// ─── TypeInferrer Implementation ───────────────────────────────────────

TypeInferrer::TypeInferrer(InferenceContext& ctx) : ctx_(ctx) {
    // Initialize with global scope
    ctx_.env.pushScope();
}

void TypeInferrer::inferProgram(const std::vector<std::shared_ptr<ASTNode>>& stmts) {
    // First pass: collect spell signatures
    for (const auto& stmt : stmts) {
        if (auto* spell = dynamic_cast<SpellStatement*>(stmt.get())) {
            registerSpell(spell);
        }
    }
    
    // Second pass: infer types for all statements
    for (const auto& stmt : stmts) {
        inferStatement(stmt.get());
    }
}

void TypeInferrer::registerSpell(SpellStatement* spell) {
    if (!spell) return;
    
    // If spell has explicit return type, use it
    if (spell->returnType.kind != TypeKind::Unknown) {
        spellReturnTypes_[spell->spellName] = spell->returnType;
    } else {
        // Will be inferred when we analyze the body
        spellReturnTypes_[spell->spellName] = Type::unknown();
    }
}

Type TypeInferrer::getSpellReturnType(const std::string& name) const {
    auto it = spellReturnTypes_.find(name);
    if (it != spellReturnTypes_.end()) {
        return it->second;
    }
    return Type::unknown();
}

void TypeInferrer::inferStatement(ASTNode* node) {
    if (!node) return;
    
    // Variable declaration with type annotation
    if (auto* vd = dynamic_cast<VariableDeclaration*>(node)) {
        Type initType = inferExpression(vd->initializer.get());
        
        // If has declared type, use it; otherwise use inferred
        if (vd->typeInfo.hasRune) {
            ctx_.env.declare(vd->varName, vd->typeInfo.declaredType);
            vd->typeInfo.inferredType = initType;
            
            // Check compatibility
            if (!isAssignableFrom(vd->typeInfo.declaredType, initType)) {
                ctx_.error(vd->sourceLine, 
                    "Type mismatch: cannot assign " + typeToString(initType) + 
                    " to variable '" + vd->varName + "' declared as " + 
                    typeToString(vd->typeInfo.declaredType));
            }
        } else {
            ctx_.env.declare(vd->varName, initType);
            vd->typeInfo.inferredType = initType;
        }
        return;
    }
    
    // Legacy assignment (BinaryExpression with IS_OF)
    if (auto* bin = dynamic_cast<BinaryExpression*>(node)) {
        if (bin->op.type == TokenType::IS_OF) {
            if (auto* lhs = dynamic_cast<Expression*>(bin->left.get())) {
                if (lhs->token.type == TokenType::IDENTIFIER) {
                    Type rhsType = inferExpression(bin->right.get());
                    ctx_.env.update(lhs->token.value, rhsType);
                    bin->typeInfo.inferredType = rhsType;
                    return;
                }
            }
        }
        // Other binary expressions - infer for both sides
        inferExpression(bin);
        return;
    }
    
    // Block statement
    if (auto* block = dynamic_cast<BlockStatement*>(node)) {
        ctx_.env.pushScope();
        for (const auto& stmt : block->statements) {
            inferStatement(stmt.get());
        }
        ctx_.env.popScope();
        return;
    }
    
    // If statement
    if (auto* ifStmt = dynamic_cast<IfStatement*>(node)) {
        Type condType = inferExpression(ifStmt->condition.get());
        if (condType.kind != TypeKind::Truth && condType.kind != TypeKind::Unknown) {
            ctx_.warn(ifStmt->sourceLine, 
                "Condition has type " + typeToString(condType) + ", expected truth");
        }
        inferStatement(ifStmt->thenBranch.get());
        if (ifStmt->elseBranch) {
            inferStatement(ifStmt->elseBranch.get());
        }
        return;
    }
    
    // While loop
    if (auto* whileLoop = dynamic_cast<WhileLoop*>(node)) {
        ctx_.env.pushScope();
        // Loop variable should be whole
        if (whileLoop->loopVar) {
            if (auto* varExpr = dynamic_cast<Expression*>(whileLoop->loopVar.get())) {
                ctx_.env.declare(varExpr->token.value, Type::whole());
            }
        }
        for (const auto& stmt : whileLoop->body) {
            inferStatement(stmt.get());
        }
        ctx_.env.popScope();
        return;
    }
    
    // Spell definition
    if (auto* spell = dynamic_cast<SpellStatement*>(node)) {
        ctx_.env.pushScope();
        
        // Declare parameters with their types
        for (size_t i = 0; i < spell->params.size(); ++i) {
            Type paramType = Type::unknown();
            if (i < spell->paramTypes.size()) {
                paramType = spell->paramTypes[i];
            }
            ctx_.env.declare(spell->params[i], paramType);
        }
        
        // Infer body
        inferStatement(spell->body.get());
        
        ctx_.env.popScope();
        return;
    }
    
    // Return statement
    if (auto* ret = dynamic_cast<ReturnStatement*>(node)) {
        Type retType = inferExpression(ret->expression.get());
        ret->typeInfo.inferredType = retType;
        return;
    }
    
    // Print statement
    if (auto* print = dynamic_cast<PrintStatement*>(node)) {
        inferExpression(print->expression.get());
        return;
    }
}

Type TypeInferrer::inferExpression(ASTNode* node) {
    if (!node) return Type::unknown();
    
    // Literal or identifier expression
    if (auto* expr = dynamic_cast<Expression*>(node)) {
        Type result;
        switch (expr->token.type) {
            case TokenType::NUMBER:
                result = Type::whole();
                break;
            case TokenType::STRING:
                result = Type::phrase();
                break;
            case TokenType::BOOLEAN:
                result = Type::truth();
                break;
            case TokenType::IDENTIFIER:
                result = ctx_.env.lookup(expr->token.value);
                break;
            default:
                result = Type::unknown();
        }
        expr->typeInfo.inferredType = result;
        return result;
    }
    
    // Binary expression
    if (auto* bin = dynamic_cast<BinaryExpression*>(node)) {
        return inferBinaryExpr(bin);
    }
    
    // Spell invocation
    if (auto* inv = dynamic_cast<SpellInvocation*>(node)) {
        return inferSpellInvocation(inv);
    }
    
    // Array literal
    if (auto* arr = dynamic_cast<ArrayLiteral*>(node)) {
        return inferArrayLiteral(arr);
    }
    
    // Map literal
    if (auto* tome = dynamic_cast<ObjectLiteral*>(node)) {
        return inferMapLiteral(tome);
    }
    
    return Type::unknown();
}

Type TypeInferrer::inferBinaryExpr(BinaryExpression* node) {
    if (!node) return Type::unknown();
    
    Type leftType = inferExpression(node->left.get());
    Type rightType = inferExpression(node->right.get());
    
    Type result;
    
    // Arithmetic operators: whole + whole = whole
    if (node->op.type == TokenType::OPERATOR) {
        const std::string& op = node->op.value;
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
            if (leftType.kind == TypeKind::Whole && rightType.kind == TypeKind::Whole) {
                result = Type::whole();
            } else if (op == "+" && 
                       (leftType.kind == TypeKind::Phrase || rightType.kind == TypeKind::Phrase)) {
                // String concatenation
                result = Type::phrase();
            } else {
                result = Type::unknown(); // Dynamic dispatch at runtime
            }
        }
    }
    
    // Comparison operators: always return truth
    if (node->op.type == TokenType::EQUAL || 
        node->op.type == TokenType::NOT_EQUAL ||
        node->op.type == TokenType::GREATER ||
        node->op.type == TokenType::LESSER) {
        result = Type::truth();
    }
    
    // Logical operators: truth + truth = truth
    if (node->op.type == TokenType::AND ||
        node->op.type == TokenType::OR) {
        result = Type::truth();
    }
    
    node->typeInfo.inferredType = result;
    return result;
}

Type TypeInferrer::inferSpellInvocation(SpellInvocation* node) {
    if (!node) return Type::unknown();
    
    // Infer argument types
    for (const auto& arg : node->args) {
        inferExpression(arg.get());
    }
    
    // Look up spell return type
    Type result = getSpellReturnType(node->spellName);
    
    // Built-in spells
    if (node->spellName == "len" || node->spellName == "count") {
        result = Type::whole();
    } else if (node->spellName == "str" || node->spellName == "phrase") {
        result = Type::phrase();
    } else if (node->spellName == "empty") {
        result = Type::truth();
    }
    
    node->typeInfo.inferredType = result;
    return result;
}

Type TypeInferrer::inferArrayLiteral(ArrayLiteral* node) {
    if (!node) return Type::order(Type::unknown());
    
    Type elemType = Type::unknown();
    
    // Infer element type from first non-unknown element
    for (const auto& elem : node->elements) {
        Type t = inferExpression(elem.get());
        if (t.kind != TypeKind::Unknown) {
            if (elemType.kind == TypeKind::Unknown) {
                elemType = t;
            } else {
                // Unify element types
                auto unified = unifyTypes(elemType, t);
                elemType = unified.value_or(t);
            }
        }
    }
    
    Type result = Type::order(elemType);
    node->typeInfo.inferredType = result;
    return result;
}

Type TypeInferrer::inferMapLiteral(ObjectLiteral* node) {
    if (!node) return Type::tome(Type::unknown(), Type::unknown());
    
    Type keyType = Type::phrase(); // Keys in ObjectLiteral are always strings
    Type valType = Type::unknown();
    
    for (const auto& entry : node->entries) {
        // entry.first is string key, entry.second is value expression
        Type vt = inferExpression(entry.second.get());
        
        if (vt.kind != TypeKind::Unknown) {
            auto unified = unifyTypes(valType, vt);
            valType = (valType.kind == TypeKind::Unknown) ? vt : unified.value_or(vt);
        }
    }
    
    Type result = Type::tome(keyType, valType);
    node->typeInfo.inferredType = result;
    return result;
}

// ─── Convenience Functions ─────────────────────────────────────────────

InferenceContext inferTypes(const std::vector<std::shared_ptr<ASTNode>>& program,
                            bool verbose) {
    InferenceContext ctx;
    ctx.verbose = verbose;
    
    TypeInferrer inferrer(ctx);
    inferrer.inferProgram(program);
    
    return ctx;
}

std::string explainType(ASTNode* node, const InferenceContext& /*ctx*/) {
    if (!node) return "void (no expression)";
    
    std::ostringstream oss;
    
    // Get type annotation
    const TypeAnnotation& ta = node->typeInfo;
    
    if (ta.hasRune) {
        oss << "Declared: " << typeToString(ta.declaredType);
        if (ta.inferredType.kind != TypeKind::Unknown) {
            oss << ", Inferred: " << typeToString(ta.inferredType);
            if (!isAssignableFrom(ta.declaredType, ta.inferredType)) {
                oss << " [TYPE MISMATCH]";
            }
        }
    } else if (ta.inferredType.kind != TypeKind::Unknown) {
        oss << "Inferred: " << typeToString(ta.inferredType);
    } else {
        oss << "Unknown (dynamic)";
    }
    
    return oss.str();
}

} // namespace ardent

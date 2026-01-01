#include "optimizer.h"
#include <iostream>
#include <cmath>
#include <functional>

namespace opt {

// ============================================================================
// ConstantFolder Implementation
// ============================================================================

std::optional<ConstValue> ConstantFolder::asConstant(const std::shared_ptr<ASTNode>& node) const {
    auto expr = std::dynamic_pointer_cast<Expression>(node);
    if (!expr) return std::nullopt;

    switch (expr->token.type) {
        case TokenType::NUMBER:
            return std::stoi(expr->token.value);
        case TokenType::STRING:
            return expr->token.value;
        case TokenType::BOOLEAN:
            return (expr->token.value == "True" || expr->token.value == "true" || expr->token.value == "TRUE");
        default:
            return std::nullopt;
    }
}

std::shared_ptr<ASTNode> ConstantFolder::makeLiteral(const ConstValue& cv) const {
    if (std::holds_alternative<int>(cv)) {
        Token tok(TokenType::NUMBER, std::to_string(std::get<int>(cv)));
        return std::make_shared<Expression>(tok);
    } else if (std::holds_alternative<std::string>(cv)) {
        Token tok(TokenType::STRING, std::get<std::string>(cv));
        return std::make_shared<Expression>(tok);
    } else {
        Token tok(TokenType::BOOLEAN, std::get<bool>(cv) ? "True" : "False");
        return std::make_shared<Expression>(tok);
    }
}

std::optional<ConstValue> ConstantFolder::foldBinary(const std::string& op, const ConstValue& lhs, const ConstValue& rhs) const {
    // Numeric operations
    if (std::holds_alternative<int>(lhs) && std::holds_alternative<int>(rhs)) {
        int l = std::get<int>(lhs);
        int r = std::get<int>(rhs);
        if (op == "+") return l + r;
        if (op == "-") return l - r;
        if (op == "*") return l * r;
        if (op == "/" && r != 0) return l / r;
        if (op == "%") return (r != 0) ? (l % r) : 0;
    }

    // String concatenation
    if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs)) {
        if (op == "+") {
            return std::get<std::string>(lhs) + std::get<std::string>(rhs);
        }
    }

    // Boolean operations
    if (std::holds_alternative<bool>(lhs) && std::holds_alternative<bool>(rhs)) {
        bool l = std::get<bool>(lhs);
        bool r = std::get<bool>(rhs);
        // AND/OR handled separately, but can also be expressed as operators
    }

    return std::nullopt;
}

std::optional<bool> ConstantFolder::foldComparison(TokenType op, const ConstValue& lhs, const ConstValue& rhs) const {
    // Numeric comparisons
    if (std::holds_alternative<int>(lhs) && std::holds_alternative<int>(rhs)) {
        int l = std::get<int>(lhs);
        int r = std::get<int>(rhs);
        switch (op) {
            case TokenType::EQUAL: return l == r;
            case TokenType::NOT_EQUAL: return l != r;
            case TokenType::GREATER:
            case TokenType::SURPASSETH: return l > r;
            case TokenType::LESSER:
            case TokenType::REMAINETH: return l < r;
            default: break;
        }
    }

    // String comparisons
    if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs)) {
        const std::string& l = std::get<std::string>(lhs);
        const std::string& r = std::get<std::string>(rhs);
        switch (op) {
            case TokenType::EQUAL: return l == r;
            case TokenType::NOT_EQUAL: return l != r;
            case TokenType::GREATER:
            case TokenType::SURPASSETH: return l > r;
            case TokenType::LESSER:
            case TokenType::REMAINETH: return l < r;
            default: break;
        }
    }

    // Boolean equality
    if (std::holds_alternative<bool>(lhs) && std::holds_alternative<bool>(rhs)) {
        bool l = std::get<bool>(lhs);
        bool r = std::get<bool>(rhs);
        if (op == TokenType::EQUAL) return l == r;
        if (op == TokenType::NOT_EQUAL) return l != r;
    }

    return std::nullopt;
}

std::optional<ConstValue> ConstantFolder::foldUnary(TokenType op, const ConstValue& operand) const {
    if (op == TokenType::NOT) {
        if (std::holds_alternative<bool>(operand)) {
            return !std::get<bool>(operand);
        }
        if (std::holds_alternative<int>(operand)) {
            return std::get<int>(operand) == 0;
        }
    }
    // Unary minus (if supported)
    if (op == TokenType::OPERATOR) {
        if (std::holds_alternative<int>(operand)) {
            return -std::get<int>(operand);
        }
    }
    return std::nullopt;
}

std::shared_ptr<ASTNode> ConstantFolder::fold(std::shared_ptr<ASTNode> node) {
    if (!node) return node;

    // Recursively fold children first (post-order traversal)

    // BinaryExpression
    if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(node)) {
        bin->left = fold(bin->left);
        bin->right = fold(bin->right);

        auto lc = asConstant(bin->left);
        auto rc = asConstant(bin->right);

        if (lc && rc) {
            // Comparison operators
            if (bin->op.type == TokenType::EQUAL || bin->op.type == TokenType::NOT_EQUAL ||
                bin->op.type == TokenType::GREATER || bin->op.type == TokenType::LESSER ||
                bin->op.type == TokenType::SURPASSETH || bin->op.type == TokenType::REMAINETH) {
                auto result = foldComparison(bin->op.type, *lc, *rc);
                if (result) {
                    ++foldedCount_;
                    return makeLiteral(*result);
                }
            }

            // AND / OR
            if (bin->op.type == TokenType::AND) {
                bool lv = std::holds_alternative<bool>(*lc) ? std::get<bool>(*lc) :
                          (std::holds_alternative<int>(*lc) ? std::get<int>(*lc) != 0 : true);
                bool rv = std::holds_alternative<bool>(*rc) ? std::get<bool>(*rc) :
                          (std::holds_alternative<int>(*rc) ? std::get<int>(*rc) != 0 : true);
                ++foldedCount_;
                return makeLiteral(lv && rv);
            }
            if (bin->op.type == TokenType::OR) {
                bool lv = std::holds_alternative<bool>(*lc) ? std::get<bool>(*lc) :
                          (std::holds_alternative<int>(*lc) ? std::get<int>(*lc) != 0 : true);
                bool rv = std::holds_alternative<bool>(*rc) ? std::get<bool>(*rc) :
                          (std::holds_alternative<int>(*rc) ? std::get<int>(*rc) != 0 : true);
                ++foldedCount_;
                return makeLiteral(lv || rv);
            }

            // Arithmetic / string ops
            if (bin->op.type == TokenType::OPERATOR) {
                auto result = foldBinary(bin->op.value, *lc, *rc);
                if (result) {
                    ++foldedCount_;
                    return makeLiteral(*result);
                }
            }
        }

        return node;
    }

    // UnaryExpression
    if (auto un = std::dynamic_pointer_cast<UnaryExpression>(node)) {
        un->operand = fold(un->operand);
        auto oc = asConstant(un->operand);
        if (oc) {
            auto result = foldUnary(un->op.type, *oc);
            if (result) {
                ++foldedCount_;
                return makeLiteral(*result);
            }
        }
        return node;
    }

    // BlockStatement - fold each statement
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(node)) {
        for (auto& stmt : block->statements) {
            stmt = fold(stmt);
        }
        return node;
    }

    // IfStatement - fold condition and branches
    if (auto ifs = std::dynamic_pointer_cast<IfStatement>(node)) {
        ifs->condition = fold(ifs->condition);
        ifs->thenBranch = fold(ifs->thenBranch);
        ifs->elseBranch = fold(ifs->elseBranch);

        // If condition is constant, replace with appropriate branch
        auto cc = asConstant(ifs->condition);
        if (cc && std::holds_alternative<bool>(*cc)) {
            ++foldedCount_;
            return std::get<bool>(*cc) ? ifs->thenBranch : ifs->elseBranch;
        }
        return node;
    }

    // WhileLoop - fold body statements
    if (auto wl = std::dynamic_pointer_cast<WhileLoop>(node)) {
        for (auto& stmt : wl->body) {
            stmt = fold(stmt);
        }
        return node;
    }

    // ForLoop
    if (auto fl = std::dynamic_pointer_cast<ForLoop>(node)) {
        fl->init = fold(fl->init);
        fl->condition = fold(fl->condition);
        fl->increment = fold(fl->increment);
        fl->body = fold(fl->body);
        return node;
    }

    // DoWhileLoop
    if (auto dwl = std::dynamic_pointer_cast<DoWhileLoop>(node)) {
        if (dwl->body) {
            for (auto& stmt : dwl->body->statements) {
                stmt = fold(stmt);
            }
        }
        dwl->condition = fold(dwl->condition);
        return node;
    }

    // PrintStatement
    if (auto pr = std::dynamic_pointer_cast<PrintStatement>(node)) {
        pr->expression = fold(pr->expression);
        return node;
    }

    // ReturnStatement
    if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(node)) {
        ret->expression = fold(ret->expression);
        return node;
    }

    // SpellStatement - fold body
    if (auto spell = std::dynamic_pointer_cast<SpellStatement>(node)) {
        if (spell->body) {
            for (auto& stmt : spell->body->statements) {
                stmt = fold(stmt);
            }
        }
        return node;
    }

    // SpellInvocation - fold arguments
    if (auto inv = std::dynamic_pointer_cast<SpellInvocation>(node)) {
        for (auto& arg : inv->args) {
            arg = fold(arg);
        }
        return node;
    }

    // NativeInvocation - fold arguments
    if (auto nat = std::dynamic_pointer_cast<NativeInvocation>(node)) {
        for (auto& arg : nat->args) {
            arg = fold(arg);
        }
        return node;
    }

    // ArrayLiteral
    if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(node)) {
        for (auto& el : arr->elements) {
            el = fold(el);
        }
        return node;
    }

    // ObjectLiteral
    if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(node)) {
        for (auto& entry : obj->entries) {
            entry.second = fold(entry.second);
        }
        return node;
    }

    // IndexExpression
    if (auto idx = std::dynamic_pointer_cast<IndexExpression>(node)) {
        idx->target = fold(idx->target);
        idx->index = fold(idx->index);
        return node;
    }

    // CastExpression
    if (auto cast = std::dynamic_pointer_cast<CastExpression>(node)) {
        cast->operand = fold(cast->operand);
        return node;
    }

    // CollectionRite
    if (auto rite = std::dynamic_pointer_cast<CollectionRite>(node)) {
        rite->keyExpr = fold(rite->keyExpr);
        rite->valueExpr = fold(rite->valueExpr);
        return node;
    }

    // TryCatch
    if (auto tc = std::dynamic_pointer_cast<TryCatch>(node)) {
        if (tc->tryBlock) {
            for (auto& stmt : tc->tryBlock->statements) {
                stmt = fold(stmt);
            }
        }
        if (tc->catchBlock) {
            for (auto& stmt : tc->catchBlock->statements) {
                stmt = fold(stmt);
            }
        }
        if (tc->finallyBlock) {
            for (auto& stmt : tc->finallyBlock->statements) {
                stmt = fold(stmt);
            }
        }
        return node;
    }

    return node;
}

// ============================================================================
// PurityAnalyzer Implementation
// ============================================================================

void PurityAnalyzer::analyze(std::shared_ptr<ASTNode> root) {
    spellDefs_.clear();
    pureSpells_.clear();
    impureSpells_.clear();

    // Collect all spell definitions
    std::function<void(const std::shared_ptr<ASTNode>&)> collect = [&](const std::shared_ptr<ASTNode>& node) {
        if (!node) return;
        if (auto spell = std::dynamic_pointer_cast<SpellStatement>(node)) {
            spellDefs_[spell->spellName] = spell;
        }
        if (auto block = std::dynamic_pointer_cast<BlockStatement>(node)) {
            for (auto& stmt : block->statements) collect(stmt);
        }
    };
    collect(root);

    // Analyze each spell
    for (auto& [name, _] : spellDefs_) {
        computePurity(name);
    }
}

bool PurityAnalyzer::isPure(const std::string& spellName) const {
    return pureSpells_.count(spellName) > 0;
}

bool PurityAnalyzer::computePurity(const std::string& name) {
    // Already computed?
    if (pureSpells_.count(name)) return true;
    if (impureSpells_.count(name)) return false;

    // Cycle detection
    if (analyzing_.count(name)) {
        // Conservative: treat cycles as impure
        return false;
    }

    auto it = spellDefs_.find(name);
    if (it == spellDefs_.end()) {
        // Unknown spell - treat as impure
        impureSpells_.insert(name);
        return false;
    }

    analyzing_.insert(name);

    // Build local variable set (parameters)
    std::unordered_set<std::string> locals;
    for (const auto& param : it->second->params) {
        locals.insert(param);
    }

    bool pure = isBlockPure(it->second->body, locals);

    analyzing_.erase(name);

    if (pure) {
        pureSpells_.insert(name);
    } else {
        impureSpells_.insert(name);
    }

    return pure;
}

bool PurityAnalyzer::isBlockPure(const std::shared_ptr<BlockStatement>& block,
                                  const std::unordered_set<std::string>& locals) {
    if (!block) return true;

    std::unordered_set<std::string> currentLocals = locals;

    for (const auto& stmt : block->statements) {
        if (!isStatementPure(stmt, currentLocals)) {
            return false;
        }

        // Track new local variable declarations
        if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(stmt)) {
            if (bin->op.type == TokenType::IS_OF) {
                if (auto lhs = std::dynamic_pointer_cast<Expression>(bin->left)) {
                    currentLocals.insert(lhs->token.value);
                }
            }
        }
    }

    return true;
}

bool PurityAnalyzer::isStatementPure(const std::shared_ptr<ASTNode>& stmt,
                                      const std::unordered_set<std::string>& locals) {
    if (!stmt) return true;

    // PrintStatement is impure (I/O)
    if (std::dynamic_pointer_cast<PrintStatement>(stmt)) {
        return false;
    }

    // NativeInvocation is impure (could have side effects)
    if (std::dynamic_pointer_cast<NativeInvocation>(stmt)) {
        return false;
    }

    // Import statements are impure (file I/O)
    if (std::dynamic_pointer_cast<ImportAll>(stmt) ||
        std::dynamic_pointer_cast<ImportSelective>(stmt) ||
        std::dynamic_pointer_cast<UnfurlInclude>(stmt)) {
        return false;
    }

    // CollectionRite is mutation
    if (std::dynamic_pointer_cast<CollectionRite>(stmt)) {
        return false;
    }

    // TryCatch - check inner blocks
    if (auto tc = std::dynamic_pointer_cast<TryCatch>(stmt)) {
        if (!isBlockPure(tc->tryBlock, locals)) return false;
        if (!isBlockPure(tc->catchBlock, locals)) return false;
        if (!isBlockPure(tc->finallyBlock, locals)) return false;
        return true;
    }

    // IfStatement
    if (auto ifs = std::dynamic_pointer_cast<IfStatement>(stmt)) {
        if (!isExpressionPure(ifs->condition, locals)) return false;
        if (!isStatementPure(ifs->thenBranch, locals)) return false;
        if (!isStatementPure(ifs->elseBranch, locals)) return false;
        return true;
    }

    // BlockStatement
    if (auto block = std::dynamic_pointer_cast<BlockStatement>(stmt)) {
        return isBlockPure(block, locals);
    }

    // WhileLoop
    if (auto wl = std::dynamic_pointer_cast<WhileLoop>(stmt)) {
        for (const auto& s : wl->body) {
            if (!isStatementPure(s, locals)) return false;
        }
        return true;
    }

    // ForLoop
    if (auto fl = std::dynamic_pointer_cast<ForLoop>(stmt)) {
        if (!isStatementPure(fl->init, locals)) return false;
        if (!isExpressionPure(fl->condition, locals)) return false;
        if (!isStatementPure(fl->increment, locals)) return false;
        if (!isStatementPure(fl->body, locals)) return false;
        return true;
    }

    // DoWhileLoop
    if (auto dwl = std::dynamic_pointer_cast<DoWhileLoop>(stmt)) {
        if (!isBlockPure(dwl->body, locals)) return false;
        if (!isExpressionPure(dwl->condition, locals)) return false;
        return true;
    }

    // ReturnStatement
    if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(stmt)) {
        return isExpressionPure(ret->expression, locals);
    }

    // BinaryExpression (assignment or expression)
    if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(stmt)) {
        // Check if it's an assignment to a non-local variable
        if (bin->op.type == TokenType::IS_OF) {
            if (auto lhs = std::dynamic_pointer_cast<Expression>(bin->left)) {
                // Assignment to non-local is impure (external mutation)
                // But for simplicity, we allow all assignments within spell body
                // as long as RHS is pure
            }
        }
        if (!isExpressionPure(bin->left, locals)) return false;
        if (!isExpressionPure(bin->right, locals)) return false;
        return true;
    }

    // Default: check as expression
    return isExpressionPure(stmt, locals);
}

bool PurityAnalyzer::isExpressionPure(const std::shared_ptr<ASTNode>& expr,
                                       const std::unordered_set<std::string>& locals) {
    if (!expr) return true;

    // Literals are pure
    if (auto e = std::dynamic_pointer_cast<Expression>(expr)) {
        return true; // identifiers and literals are pure reads
    }

    // BinaryExpression
    if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        return isExpressionPure(bin->left, locals) && isExpressionPure(bin->right, locals);
    }

    // UnaryExpression
    if (auto un = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
        return isExpressionPure(un->operand, locals);
    }

    // SpellInvocation - check if called spell is pure
    if (auto inv = std::dynamic_pointer_cast<SpellInvocation>(expr)) {
        // Recursively check purity of called spell
        if (!computePurity(inv->spellName)) {
            return false;
        }
        // Check arguments are pure expressions
        for (const auto& arg : inv->args) {
            if (!isExpressionPure(arg, locals)) return false;
        }
        return true;
    }

    // NativeInvocation is impure
    if (std::dynamic_pointer_cast<NativeInvocation>(expr)) {
        return false;
    }

    // ArrayLiteral
    if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(expr)) {
        for (const auto& el : arr->elements) {
            if (!isExpressionPure(el, locals)) return false;
        }
        return true;
    }

    // ObjectLiteral
    if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(expr)) {
        for (const auto& entry : obj->entries) {
            if (!isExpressionPure(entry.second, locals)) return false;
        }
        return true;
    }

    // IndexExpression
    if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        return isExpressionPure(idx->target, locals) && isExpressionPure(idx->index, locals);
    }

    // CastExpression
    if (auto cast = std::dynamic_pointer_cast<CastExpression>(expr)) {
        return isExpressionPure(cast->operand, locals);
    }

    return true;
}

// ============================================================================
// PartialEvaluator Implementation
// ============================================================================

std::shared_ptr<ASTNode> PartialEvaluator::evaluate(std::shared_ptr<ASTNode> node) {
    if (!node) return node;

    // SpellInvocation - try to evaluate if pure with constant args
    if (auto inv = std::dynamic_pointer_cast<SpellInvocation>(node)) {
        // Fold arguments first
        for (auto& arg : inv->args) {
            arg = evaluate(arg);
        }

        // Check if spell is pure
        if (!purity_.isPure(inv->spellName)) {
            return node;
        }

        // Check if all arguments are constants
        std::vector<ConstValue> constArgs;
        for (const auto& arg : inv->args) {
            auto expr = std::dynamic_pointer_cast<Expression>(arg);
            if (!expr) return node;

            switch (expr->token.type) {
                case TokenType::NUMBER:
                    constArgs.push_back(std::stoi(expr->token.value));
                    break;
                case TokenType::STRING:
                    constArgs.push_back(expr->token.value);
                    break;
                case TokenType::BOOLEAN:
                    constArgs.push_back(expr->token.value == "True");
                    break;
                default:
                    return node; // Not a constant
            }
        }

        // Try to evaluate
        auto result = tryEvaluate(inv->spellName, constArgs);
        if (result) {
            ++evaluatedCount_;
            if (std::holds_alternative<int>(*result)) {
                Token tok(TokenType::NUMBER, std::to_string(std::get<int>(*result)));
                return std::make_shared<Expression>(tok);
            } else if (std::holds_alternative<std::string>(*result)) {
                Token tok(TokenType::STRING, std::get<std::string>(*result));
                return std::make_shared<Expression>(tok);
            } else {
                Token tok(TokenType::BOOLEAN, std::get<bool>(*result) ? "True" : "False");
                return std::make_shared<Expression>(tok);
            }
        }

        return node;
    }

    // Recursively process other nodes
    if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(node)) {
        bin->left = evaluate(bin->left);
        bin->right = evaluate(bin->right);
        return node;
    }

    if (auto un = std::dynamic_pointer_cast<UnaryExpression>(node)) {
        un->operand = evaluate(un->operand);
        return node;
    }

    if (auto block = std::dynamic_pointer_cast<BlockStatement>(node)) {
        for (auto& stmt : block->statements) {
            stmt = evaluate(stmt);
        }
        return node;
    }

    if (auto ifs = std::dynamic_pointer_cast<IfStatement>(node)) {
        ifs->condition = evaluate(ifs->condition);
        ifs->thenBranch = evaluate(ifs->thenBranch);
        ifs->elseBranch = evaluate(ifs->elseBranch);
        return node;
    }

    if (auto pr = std::dynamic_pointer_cast<PrintStatement>(node)) {
        pr->expression = evaluate(pr->expression);
        return node;
    }

    if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(node)) {
        ret->expression = evaluate(ret->expression);
        return node;
    }

    if (auto spell = std::dynamic_pointer_cast<SpellStatement>(node)) {
        if (spell->body) {
            for (auto& stmt : spell->body->statements) {
                stmt = evaluate(stmt);
            }
        }
        return node;
    }

    return node;
}

std::optional<ConstValue> PartialEvaluator::tryEvaluate(const std::string& spellName,
                                                         const std::vector<ConstValue>& args) {
    auto it = spells_.find(spellName);
    if (it == spells_.end()) return std::nullopt;

    const auto& spell = it->second;
    if (spell->params.size() != args.size()) return std::nullopt;

    return interpretPure(spell->body, spell->params, args);
}

std::optional<ConstValue> PartialEvaluator::interpretPure(const std::shared_ptr<BlockStatement>& body,
                                                           const std::vector<std::string>& params,
                                                           const std::vector<ConstValue>& args) {
    if (!body) return std::nullopt;

    // Build initial environment
    std::unordered_map<std::string, ConstValue> env;
    for (size_t i = 0; i < params.size(); ++i) {
        env[params[i]] = args[i];
    }

    // Execute statements looking for return
    for (const auto& stmt : body->statements) {
        if (auto ret = std::dynamic_pointer_cast<ReturnStatement>(stmt)) {
            return evalExpr(ret->expression, env);
        }

        // Handle variable assignment
        if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(stmt)) {
            if (bin->op.type == TokenType::IS_OF) {
                if (auto lhs = std::dynamic_pointer_cast<Expression>(bin->left)) {
                    auto val = evalExpr(bin->right, env);
                    if (val) {
                        env[lhs->token.value] = *val;
                    }
                }
            }
        }
    }

    return std::nullopt; // No return found
}

std::optional<ConstValue> PartialEvaluator::evalExpr(const std::shared_ptr<ASTNode>& expr,
                                                      std::unordered_map<std::string, ConstValue>& env) {
    if (!expr) return std::nullopt;

    if (auto e = std::dynamic_pointer_cast<Expression>(expr)) {
        switch (e->token.type) {
            case TokenType::NUMBER:
                return std::stoi(e->token.value);
            case TokenType::STRING:
                return e->token.value;
            case TokenType::BOOLEAN:
                return e->token.value == "True";
            case TokenType::IDENTIFIER: {
                auto it = env.find(e->token.value);
                if (it != env.end()) return it->second;
                return std::nullopt;
            }
            default:
                return std::nullopt;
        }
    }

    if (auto bin = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        auto lv = evalExpr(bin->left, env);
        auto rv = evalExpr(bin->right, env);
        if (!lv || !rv) return std::nullopt;

        // Arithmetic
        if (std::holds_alternative<int>(*lv) && std::holds_alternative<int>(*rv)) {
            int l = std::get<int>(*lv);
            int r = std::get<int>(*rv);
            if (bin->op.type == TokenType::OPERATOR) {
                if (bin->op.value == "+") return l + r;
                if (bin->op.value == "-") return l - r;
                if (bin->op.value == "*") return l * r;
                if (bin->op.value == "/" && r != 0) return l / r;
            }
            // Comparisons
            if (bin->op.type == TokenType::EQUAL) return l == r;
            if (bin->op.type == TokenType::NOT_EQUAL) return l != r;
            if (bin->op.type == TokenType::GREATER || bin->op.type == TokenType::SURPASSETH) return l > r;
            if (bin->op.type == TokenType::LESSER || bin->op.type == TokenType::REMAINETH) return l < r;
        }

        // String concat
        if (std::holds_alternative<std::string>(*lv) && std::holds_alternative<std::string>(*rv)) {
            if (bin->op.type == TokenType::OPERATOR && bin->op.value == "+") {
                return std::get<std::string>(*lv) + std::get<std::string>(*rv);
            }
        }

        // Boolean ops
        if (bin->op.type == TokenType::AND) {
            bool lb = std::holds_alternative<bool>(*lv) ? std::get<bool>(*lv) :
                      (std::holds_alternative<int>(*lv) ? std::get<int>(*lv) != 0 : true);
            bool rb = std::holds_alternative<bool>(*rv) ? std::get<bool>(*rv) :
                      (std::holds_alternative<int>(*rv) ? std::get<int>(*rv) != 0 : true);
            return lb && rb;
        }
        if (bin->op.type == TokenType::OR) {
            bool lb = std::holds_alternative<bool>(*lv) ? std::get<bool>(*lv) :
                      (std::holds_alternative<int>(*lv) ? std::get<int>(*lv) != 0 : true);
            bool rb = std::holds_alternative<bool>(*rv) ? std::get<bool>(*rv) :
                      (std::holds_alternative<int>(*rv) ? std::get<int>(*rv) != 0 : true);
            return lb || rb;
        }
    }

    if (auto un = std::dynamic_pointer_cast<UnaryExpression>(expr)) {
        auto ov = evalExpr(un->operand, env);
        if (!ov) return std::nullopt;

        if (un->op.type == TokenType::NOT) {
            if (std::holds_alternative<bool>(*ov)) return !std::get<bool>(*ov);
            if (std::holds_alternative<int>(*ov)) return std::get<int>(*ov) == 0;
        }
    }

    // Recursive spell call within pure spell
    if (auto inv = std::dynamic_pointer_cast<SpellInvocation>(expr)) {
        if (!purity_.isPure(inv->spellName)) return std::nullopt;

        std::vector<ConstValue> callArgs;
        for (const auto& arg : inv->args) {
            auto av = evalExpr(arg, env);
            if (!av) return std::nullopt;
            callArgs.push_back(*av);
        }

        return tryEvaluate(inv->spellName, callArgs);
    }

    return std::nullopt;
}

// ============================================================================
// Optimizer Implementation
// ============================================================================

void Optimizer::collectSpells(std::shared_ptr<ASTNode> node) {
    if (!node) return;

    if (auto spell = std::dynamic_pointer_cast<SpellStatement>(node)) {
        spellDefs_[spell->spellName] = spell;
    }

    if (auto block = std::dynamic_pointer_cast<BlockStatement>(node)) {
        for (const auto& stmt : block->statements) {
            collectSpells(stmt);
        }
    }
}

std::shared_ptr<ASTNode> Optimizer::optimize(std::shared_ptr<ASTNode> root) {
    if (!root) return root;

    // 1. Constant folding (first pass)
    root = folder_.fold(root);

    // 2. Collect spell definitions
    spellDefs_.clear();
    collectSpells(root);

    // 3. Purity analysis
    purity_.analyze(root);

    // 4. Partial evaluation of pure spells
    partialEvaluator_ = std::make_unique<PartialEvaluator>(purity_, spellDefs_);
    root = partialEvaluator_->evaluate(root);

    // 5. Second pass of constant folding (cleanup after partial eval)
    root = folder_.fold(root);

    return root;
}

} // namespace opt

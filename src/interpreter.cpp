#include "interpreter.h"
#include <iostream>
#include "lexer.h"
#include "parser.h"
#include <functional>
#include <memory>
#include <vector>
#include <variant>

void Interpreter::assignVariable(const std::string& name, int value) {
    variables[name] = value;
    std::cout << "Variable assigned: " << name << " = " << value << std::endl;
}

void Interpreter::assignVariable(const std::string& name, const std::string& value) {
    variables[name] = value;
    std::cout << "Variable assigned: " << name << " = " << value << std::endl;
}

void Interpreter::assignVariable(const std::string& name, bool value) {
    variables[name] = value;
    std::cout << "Variable assigned: " << name << " = " << (value ? "True" : "False") << std::endl;
}

int Interpreter::getIntVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        if (std::holds_alternative<int>(it->second)) {
            return std::get<int>(it->second);
        }
        if (std::holds_alternative<bool>(it->second)) {
            return std::get<bool>(it->second) ? 1 : 0;
        }
        std::cerr << "Error: Variable '" << name << "' is not a number" << std::endl;
    } else {
        std::cerr << "Error: Undefined variable '" << name << "'" << std::endl;
    }
    return 0;
}

std::string Interpreter::getStringVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        if (std::holds_alternative<std::string>(it->second)) {
            return std::get<std::string>(it->second);
        }
        // If it's an int, convert to string (useful for printing)
        if (std::holds_alternative<int>(it->second)) {
            return std::to_string(std::get<int>(it->second));
        }
        if (std::holds_alternative<bool>(it->second)) {
            return std::get<bool>(it->second) ? std::string("True") : std::string("False");
        }
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
            case TokenType::STRING:
                return e->token.value;
            case TokenType::BOOLEAN:
                return e->token.value == "True";
            case TokenType::IDENTIFIER: {
                auto it = variables.find(e->token.value);
                if (it != variables.end()) return it->second;
                std::cerr << "Error: Undefined variable '" << e->token.value << "'" << std::endl;
                return 0;
            }
            default:
                break;
        }
    }
    if (auto arr = std::dynamic_pointer_cast<ArrayLiteral>(expr)) {
        std::vector<SimpleValue> out;
        out.reserve(arr->elements.size());
        for (auto &el : arr->elements) {
            Value v = evaluateValue(el);
            if (std::holds_alternative<int>(v)) out.emplace_back(std::get<int>(v));
            else if (std::holds_alternative<std::string>(v)) out.emplace_back(std::get<std::string>(v));
            else if (std::holds_alternative<bool>(v)) out.emplace_back(std::get<bool>(v));
            else {
                std::cerr << "TypeError: Only simple values (number, phrase, truth) allowed inside an order" << std::endl;
                out.emplace_back(0);
            }
        }
        return out;
    }
    if (auto obj = std::dynamic_pointer_cast<ObjectLiteral>(expr)) {
        std::unordered_map<std::string, SimpleValue> out;
        for (auto &kv : obj->entries) {
            Value v = evaluateValue(kv.second);
            if (std::holds_alternative<int>(v)) out.emplace(kv.first, std::get<int>(v));
            else if (std::holds_alternative<std::string>(v)) out.emplace(kv.first, std::get<std::string>(v));
            else if (std::holds_alternative<bool>(v)) out.emplace(kv.first, std::get<bool>(v));
            else {
                std::cerr << "TypeError: Only simple values (number, phrase, truth) allowed inside a tome" << std::endl;
                out.emplace(kv.first, 0);
            }
        }
        return out;
    }
    if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value target = evaluateValue(idx->target);
        Value key = evaluateValue(idx->index);
        // Index into order
        if (std::holds_alternative<std::vector<SimpleValue>>(target)) {
            int i = 0;
            if (std::holds_alternative<int>(key)) i = std::get<int>(key);
            else {
                std::cerr << "TypeError: Order index must be a number" << std::endl;
                return 0;
            }
            const auto &vec = std::get<std::vector<SimpleValue>>(target);
            if (i < 0 || static_cast<size_t>(i) >= vec.size()) {
                std::cerr << "IndexError: Order index out of range" << std::endl;
                return 0;
            }
            const SimpleValue &sv = vec[static_cast<size_t>(i)];
            if (std::holds_alternative<int>(sv)) return std::get<int>(sv);
            if (std::holds_alternative<std::string>(sv)) return std::get<std::string>(sv);
            if (std::holds_alternative<bool>(sv)) return std::get<bool>(sv);
            return 0;
        }
        // Index into tome
        if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(target)) {
            std::string k;
            if (std::holds_alternative<std::string>(key)) k = std::get<std::string>(key);
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
                if (std::holds_alternative<std::string>(lv) || std::holds_alternative<std::string>(rv)) {
                    return evaluatePrintExpr(expr);
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
                if (std::holds_alternative<std::string>(v)) {
                    try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; }
                }
                break;
            case CastTarget::ToTruth:
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
                if (std::holds_alternative<int>(v)) return std::get<int>(v) != 0 ? 1 : 0;
                if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty() ? 1 : 0;
                break;
            case CastTarget::ToPhrase:
                // In numeric context, phrase has no numeric value; return 0
                return 0;
        }
    } else if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value v = evaluateValue(expr);
        if (std::holds_alternative<int>(v)) return std::get<int>(v);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
        if (std::holds_alternative<std::string>(v)) {
            try { return std::stoi(std::get<std::string>(v)); } catch (...) { return 0; }
        }
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
            execute(stmt);
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
                // Store any value type, including order and tome
                variables[varName] = rhs;
                // Debug print
                std::cout << "Variable assigned: " << varName << " = ";
                if (std::holds_alternative<int>(rhs)) std::cout << std::get<int>(rhs);
                else if (std::holds_alternative<std::string>(rhs)) std::cout << std::get<std::string>(rhs);
                else if (std::holds_alternative<bool>(rhs)) std::cout << (std::get<bool>(rhs) ? "True" : "False");
                else if (std::holds_alternative<std::vector<SimpleValue>>(rhs)) std::cout << "[order size=" << std::get<std::vector<SimpleValue>>(rhs).size() << "]";
                else if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(rhs)) std::cout << "{tome size=" << std::get<std::unordered_map<std::string, SimpleValue>>(rhs).size() << "}";
                std::cout << std::endl;
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
            execute(ifStmt->thenBranch);
        } else if (ifStmt->elseBranch) {
            execute(ifStmt->elseBranch);
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
    


      // Handle print statements.
else if (auto printStmt = std::dynamic_pointer_cast<PrintStatement>(ast)) {
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

    if (variables.find(varName) == variables.end()) {
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

        // Execute body
        for (const auto& stmt : loop->body) {
            execute(stmt);
        }

        // Update variable
        if (loop->stepDirection == TokenType::DESCEND) {
            variables[varName] = getIntVariable(varName) - stepVal;
        } else {
            variables[varName] = getIntVariable(varName) + stepVal;
        }
    }
}


void Interpreter::executeForLoop(std::shared_ptr<ForLoop> loop) {
    // Initialize loop variable 
    std::string varName;
    if (auto expr = std::dynamic_pointer_cast<Expression>(loop->init)) {
    varName = expr->token.value;
    variables[varName] = evaluateExpr(loop->init);
    }

    // Loop while condition holds
    while (evaluateExpr(loop->condition)) {
        // Execute the loop body (BlockStatement)
        execute(loop->body);
    int stepVal = evaluateExpr(loop->increment);
    if (loop->stepDirection == TokenType::DESCEND) stepVal = -stepVal;
    // Increment loop variable
    variables[varName] = getIntVariable(varName) + stepVal;
    }
}


void Interpreter::executeDoWhileLoop(std::shared_ptr<DoWhileLoop> loop) {
    std::string varName = loop->loopVar->token.value;
    if (variables.find(varName) == variables.end()) {
        std::cerr << "Error: Undefined loop variable '" << varName << "'" << std::endl;
        return;
    }
    do {
        // Execute body
        for (const auto &stmt : loop->body->statements) {
            execute(stmt);
        }
        // Apply update
        if (loop->update) {
            int inc = evaluateExpr(loop->update);
            if (loop->stepDirection == TokenType::DESCEND) inc = -inc;
            variables[varName] = getIntVariable(varName) + inc;
        }
    } while (evaluateExpr(loop->condition)); // Loop while condition is TRUE
}



void Interpreter::evaluateExpression(std::shared_ptr<ASTNode> expr) {
    std::cout << "Inside evaluateExpression()" << std::endl;
    if (auto value = std::dynamic_pointer_cast<Expression>(expr)) {
        if (value->token.type == TokenType::IDENTIFIER) {
            std::string varName = value->token.value;
            if (variables.find(varName) != variables.end()) {
                if (std::holds_alternative<int>(variables[varName])) {
                    std::cout << "valueeee: " << std::get<int>(variables[varName]) << std::endl;  
                } else if (std::holds_alternative<std::string>(variables[varName])) {
                    std::cout << "valueeee: " << std::get<std::string>(variables[varName]) << std::endl;  
                } else if (std::holds_alternative<bool>(variables[varName])) {
                    std::cout << "valueeee: " << (std::get<bool>(variables[varName]) ? "True" : "False") << std::endl;  
                }
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
    if (auto strExpr = std::dynamic_pointer_cast<Expression>(expr)) {
        if (strExpr->token.type == TokenType::STRING) {
            return strExpr->token.value;
        } else if (strExpr->token.type == TokenType::BOOLEAN) {
            return strExpr->token.value; // print literal True/False
        } else if (strExpr->token.type == TokenType::IDENTIFIER) {
            // Return string value for identifiers if present, otherwise number/bool-as-string
            auto it = variables.find(strExpr->token.value);
            if (it != variables.end()) {
                if (std::holds_alternative<std::string>(it->second)) {
                    return std::get<std::string>(it->second);
                }
                if (std::holds_alternative<bool>(it->second)) {
                    return std::get<bool>(it->second) ? std::string("True") : std::string("False");
                }
                if (std::holds_alternative<int>(it->second)) {
                    return std::to_string(std::get<int>(it->second));
                }
            }
            std::cerr << "Error: Undefined variable '" << strExpr->token.value << "'" << std::endl;
            return "";
        } else {
            return std::to_string(evaluateExpr(expr));
        }
    } else if (auto idx = std::dynamic_pointer_cast<IndexExpression>(expr)) {
        Value v = evaluateValue(expr);
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? std::string("True") : std::string("False");
        if (std::holds_alternative<int>(v)) return std::to_string(std::get<int>(v));
        // Collections printing (basic)
        if (std::holds_alternative<std::vector<SimpleValue>>(v)) {
            const auto &vec = std::get<std::vector<SimpleValue>>(v);
            std::string out = "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i) out += ", ";
                const auto &sv = vec[i];
                if (std::holds_alternative<int>(sv)) out += std::to_string(std::get<int>(sv));
                else if (std::holds_alternative<std::string>(sv)) out += std::get<std::string>(sv);
                else out += (std::get<bool>(sv) ? "True" : "False");
            }
            out += "]";
            return out;
        }
        if (std::holds_alternative<std::unordered_map<std::string, SimpleValue>>(v)) {
            const auto &mp = std::get<std::unordered_map<std::string, SimpleValue>>(v);
            std::string out = "{";
            bool first = true;
            for (const auto &p : mp) {
                if (!first) out += ", ";
                first = false;
                out += p.first + ": ";
                const auto &sv = p.second;
                if (std::holds_alternative<int>(sv)) out += std::to_string(std::get<int>(sv));
                else if (std::holds_alternative<std::string>(sv)) out += std::get<std::string>(sv);
                else out += (std::get<bool>(sv) ? "True" : "False");
            }
            out += "}";
            return out;
        }
        return "";
    } else if (auto castExpr = std::dynamic_pointer_cast<CastExpression>(expr)) {
        if (castExpr->target == CastTarget::ToPhrase) {
            Value v = evaluateValue(castExpr->operand);
            if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
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
                        auto it = variables.find(e->token.value);
                        if (it != variables.end() && std::holds_alternative<std::string>(it->second)) return true;
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
                if (!leftEndsSpace && !rightStartsSpace) {
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
    }
    return "";
}
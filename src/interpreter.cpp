#include "interpreter.h"
#include <iostream>
#include "lexer.h"
#include "parser.h"
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

int Interpreter::evaluateExpr(std::shared_ptr<ASTNode> expr) {
    if (auto numExpr = std::dynamic_pointer_cast<Expression>(expr)) {
        if (numExpr->token.type == TokenType::IDENTIFIER) {
            return getIntVariable(numExpr->token.value);
        } else if (numExpr->token.type == TokenType::NUMBER) {
            return std::stoi(numExpr->token.value);
        } else if (numExpr->token.type == TokenType::BOOLEAN) {
            return (numExpr->token.value == "True") ? 1 : 0;
        }
    } else if (auto binExpr = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        int left = evaluateExpr(binExpr->left);
        int right = evaluateExpr(binExpr->right);
        // Handle comparison operators
        if (binExpr->op.type == TokenType::SURPASSETH) {
            return left > right ? 1 : 0;
        } else if (binExpr->op.type == TokenType::REMAINETH) {
            return left < right ? 1 : 0;
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
            auto rightExpr = std::dynamic_pointer_cast<Expression>(binExpr->right);
            if (leftExpr && rightExpr) {
                std::string varName = leftExpr->token.value;
                if (rightExpr->token.type == TokenType::NUMBER) {
                    int value = std::stoi(rightExpr->token.value);
                    assignVariable(varName, value);
                } else if (rightExpr->token.type == TokenType::STRING) {
                    assignVariable(varName, rightExpr->token.value);
                } else if (rightExpr->token.type == TokenType::BOOLEAN) {
                    assignVariable(varName, rightExpr->token.value == "True");
                } else if (rightExpr->token.type == TokenType::IDENTIFIER) {
                    // Assignment from another variable
                    auto it = variables.find(rightExpr->token.value);
                    if (it != variables.end()) {
                        if (std::holds_alternative<int>(it->second)) {
                            assignVariable(varName, std::get<int>(it->second));
                        } else if (std::holds_alternative<std::string>(it->second)) {
                            assignVariable(varName, std::get<std::string>(it->second));
                        } else if (std::holds_alternative<bool>(it->second)) {
                            assignVariable(varName, std::get<bool>(it->second));
                        }
                    } else {
                        std::cerr << "Error: Undefined variable '" << rightExpr->token.value << "'" << std::endl;
                    }
                }
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
        // expect the condition to be a BinaryExpression.
        auto condBinExpr = std::dynamic_pointer_cast<BinaryExpression>(ifStmt->condition);
        if (condBinExpr) {
            auto leftExpr = std::dynamic_pointer_cast<Expression>(condBinExpr->left);
            auto rightExpr = std::dynamic_pointer_cast<Expression>(condBinExpr->right);
            if (leftExpr && rightExpr) {
                int varValue = getIntVariable(leftExpr->token.value);
                int condValue = std::stoi(rightExpr->token.value);
                if (condBinExpr->op.value == "surpasseth" && varValue > condValue) {
                    execute(ifStmt->thenBranch);
                } else if (ifStmt->elseBranch) {
                    execute(ifStmt->elseBranch);
                }
            }
        } else {
            std::cerr << "Error: IF condition is not a valid binary expression." << std::endl;
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
    } else if (auto binExpr = std::dynamic_pointer_cast<BinaryExpression>(expr)) {
        std::string left = evaluatePrintExpr(binExpr->left);
        std::string right = evaluatePrintExpr(binExpr->right);
        // Insert a space between left and right if needed.
        if (!left.empty() && left.back() != ' ') {
            left.push_back(' ');
        }
        return left + right;
    }
    return "";
}
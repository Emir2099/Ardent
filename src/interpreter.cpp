#include "interpreter.h"
#include <iostream>
#include "lexer.h"
#include "parser.h"
#include <memory>
#include <vector>

void Interpreter::assignVariable(std::string name, int value) {
    variables[name] = value;
    std::cout << "Variable assigned: " << name << " = " << value << std::endl;
}

int Interpreter::getVariable(std::string name) {
    if (variables.find(name) != variables.end()) {
        return variables[name];
    } else {
        std::cerr << "Error: Undefined variable '" << name << "'" << std::endl;
        return 0;
    }
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
                int value = std::stoi(rightExpr->token.value);
                assignVariable(varName, value);
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
        // Now, expect the condition to be a BinaryExpression.
        auto condBinExpr = std::dynamic_pointer_cast<BinaryExpression>(ifStmt->condition);
        if (condBinExpr) {
            auto leftExpr = std::dynamic_pointer_cast<Expression>(condBinExpr->left);
            auto rightExpr = std::dynamic_pointer_cast<Expression>(condBinExpr->right);
            if (leftExpr && rightExpr) {
                int varValue = getVariable(leftExpr->token.value);
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
    // Handle print statements.
    else if (auto printStmt = std::dynamic_pointer_cast<PrintStatement>(ast)) {
        if (auto expr = std::dynamic_pointer_cast<Expression>(printStmt->expression)) {
            std::cout << "Output: " << expr->token.value << std::endl;
        }
    }
    else {
        std::cerr << "Error: Unknown AST Node encountered!" << std::endl;
    }
}

void Interpreter::evaluateExpression(std::shared_ptr<ASTNode> expr) {
    if (auto value = std::dynamic_pointer_cast<Expression>(expr)) {
        std::cout << "Evaluating expression: " << value->token.value << std::endl;
    } else {
        std::cerr << "Error: Invalid Expression Node!" << std::endl;
    }
}

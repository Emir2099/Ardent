#ifndef AST_H
#define AST_H

#include <memory>
#include <string>
#include <vector>
#include "token.h"

class ASTNode {
public:
    virtual ~ASTNode() = default;
};

// Represents a number or identifier
class Expression : public ASTNode {
public:
    Token token;
    Expression(Token token) : token(token) {}
};

// Represents binary operations (e.g., `name + "!"`)
class BinaryExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> left;
    Token op;
    std::shared_ptr<ASTNode> right;

    BinaryExpression(std::shared_ptr<ASTNode> left, Token op, std::shared_ptr<ASTNode> right)
        : left(left), op(op), right(right) {}
};

// Represents function calls like `greet(name)`
class FunctionCall : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> arguments;

    FunctionCall(std::string name, std::vector<std::shared_ptr<ASTNode>> arguments)
        : name(name), arguments(arguments) {}
};

// Represents an if-else condition
class IfStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::shared_ptr<ASTNode> thenBranch;
    std::shared_ptr<ASTNode> elseBranch;

    IfStatement(std::shared_ptr<ASTNode> condition, std::shared_ptr<ASTNode> thenBranch, std::shared_ptr<ASTNode> elseBranch)
        : condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}
};

#endif

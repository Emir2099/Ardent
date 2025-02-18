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

// Represents binary operations (e.g., assignments)
class BinaryExpression : public ASTNode {
public:
    std::shared_ptr<ASTNode> left;
    Token op;
    std::shared_ptr<ASTNode> right;

    BinaryExpression(std::shared_ptr<ASTNode> left, Token op, std::shared_ptr<ASTNode> right)
        : left(left), op(op), right(right) {}
};

// Represents an if-else statement
class IfStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> condition;
    std::shared_ptr<ASTNode> thenBranch;
    std::shared_ptr<ASTNode> elseBranch;

    IfStatement(std::shared_ptr<ASTNode> condition,
                std::shared_ptr<ASTNode> thenBranch,
                std::shared_ptr<ASTNode> elseBranch)
        : condition(condition), thenBranch(thenBranch), elseBranch(elseBranch) {}
};

// Represents a block of statements
class BlockStatement : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> statements;

    BlockStatement(std::vector<std::shared_ptr<ASTNode>> statements)
        : statements(statements) {}
};

// Represents a print statement (e.g., when a "let it be proclaimed" is used)
class PrintStatement : public ASTNode {
public:
    std::shared_ptr<ASTNode> expression;
    PrintStatement(std::shared_ptr<ASTNode> expression)
        : expression(expression) {}
};

struct ForLoop : public ASTNode {
    std::shared_ptr<ASTNode> init;   // Variable initialization
    std::shared_ptr<ASTNode> condition;  // Condition (count < limit)
    std::shared_ptr<ASTNode> increment;  // Step (count += 1)
    std::shared_ptr<ASTNode> body;  // Loop body

    ForLoop(std::shared_ptr<ASTNode> init, std::shared_ptr<ASTNode> condition, 
            std::shared_ptr<ASTNode> increment, std::shared_ptr<ASTNode> body)
        : init(init), condition(condition), increment(increment), body(body) {}
};


// Represents a while loop
class WhileLoop : public ASTNode {
public:
    std::shared_ptr<Expression> loopVar;
    std::shared_ptr<Expression> limit;
    std::shared_ptr<Expression> step;
    std::vector<std::shared_ptr<ASTNode>> body;

    WhileLoop(std::shared_ptr<Expression> loopVar, std::shared_ptr<Expression> limit,
              std::shared_ptr<Expression> step, std::vector<std::shared_ptr<ASTNode>> body)
        : loopVar(loopVar), limit(limit), step(step), body(body) {}
};

// Represents a do-while loop.

// NEW: Represents a do-while loop.
class DoWhileLoop : public ASTNode {
    public:
        std::shared_ptr<Expression> loopVar;  // The loop variable (e.g. count)
        std::shared_ptr<BlockStatement> body;
        std::shared_ptr<ASTNode> condition;
        std::shared_ptr<Expression> update;     // The update/increment value
        DoWhileLoop(std::shared_ptr<BlockStatement> body,
                    std::shared_ptr<ASTNode> condition,
                    std::shared_ptr<Expression> update,
                    std::shared_ptr<Expression> loopVar)
            : body(body), condition(condition), update(update), loopVar(loopVar) {}
    };
    
    

#endif
